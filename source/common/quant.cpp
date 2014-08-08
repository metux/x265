/*****************************************************************************
 * Copyright (C) 2014 x265 project
 *
 * Authors: Steve Borho <steve@borho.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111, USA.
 *
 * This program is also available under a commercial proprietary license.
 * For more information, contact us at license @ x265.com.
 *****************************************************************************/

#include "common.h"
#include "primitives.h"
#include "quant.h"
#include "frame.h"
#include "TLibCommon/TComDataCU.h"
#include "TLibCommon/TComYuv.h"
#include "TLibCommon/ContextTables.h"

using namespace x265;

#define SIGN(x,y) ((x^(y >> 31))-(y >> 31))

namespace {

struct coeffGroupRDStats
{
    int    nnzBeforePos0;     /* indicates coeff other than pos 0 are coded */
    double codedLevelAndDist; /* distortion and level cost of coded coefficients */
    double uncodedDist;       /* uncoded distortion cost of coded coefficients */ 
    double sigCost;           /* cost of signaling significant coeff bitmap */
    double sigCost0;          /* cost of signaling sig coeff bit of coeff 0 */
};

inline int fastMin(int x, int y)
{
    return y + ((x - y) & ((x - y) >> (sizeof(int) * CHAR_BIT - 1))); // min(x, y)
}

inline void denoiseDct(coeff_t* dctCoef, uint32_t* resSum, uint16_t* offset, int numCoeff)
{
    for (int i = 0; i < numCoeff; i++)
    {
        int level = dctCoef[i];
        int sign = level >> 31;
        level = (level + sign) ^ sign;
        resSum[i] += level;
        level -= offset[i];
        dctCoef[i] = level < 0 ? 0 : (level ^ sign) - sign;
    }
}

inline int getICRate(uint32_t absLevel, int32_t diffLevel, const int *greaterOneBits, const int *levelAbsBits, uint32_t absGoRice, uint32_t c1c2Idx)
{
    X265_CHECK(c1c2Idx <= 3, "c1c2Idx check failure\n");
    X265_CHECK(absGoRice <= 4, "absGoRice check failure\n");
    if (!absLevel)
    {
        X265_CHECK(diffLevel < 0, "diffLevel check failure\n");
        return 0;
    }
    int rate = 0;

    if (diffLevel < 0)
    {
        X265_CHECK(absLevel <= 2, "absLevel check failure\n");
        rate += greaterOneBits[(absLevel == 2)];

        if (absLevel == 2)
            rate += levelAbsBits[0];
    }
    else
    {
        uint32_t symbol = diffLevel;
        const uint32_t maxVlc = g_goRiceRange[absGoRice];
        bool expGolomb = (symbol > maxVlc);

        if (expGolomb)
        {
            absLevel = symbol - maxVlc;

            // NOTE: mapping to x86 hardware instruction BSR
            unsigned long size;
            CLZ32(size, absLevel);
            int egs = size * 2 + 1;

            rate += egs << 15;

            // NOTE: in here, expGolomb=true means (symbol >= maxVlc + 1)
            X265_CHECK(fastMin(symbol, (maxVlc + 1)) == (int)maxVlc + 1, "min check failure\n");
            symbol = maxVlc + 1;
        }

        uint32_t prefLen = (symbol >> absGoRice) + 1;
        uint32_t numBins = fastMin(prefLen + absGoRice, 8 /* g_goRicePrefixLen[absGoRice] + absGoRice */);

        rate += numBins << 15;

        if (c1c2Idx & 1)
            rate += greaterOneBits[1];

        if (c1c2Idx == 3)
            rate += levelAbsBits[1];
    }
    return rate;
}

/* Calculates the cost for specific absolute transform level */
inline uint32_t getICRateCost(uint32_t absLevel, int32_t diffLevel, const int *greaterOneBits, const int *levelAbsBits, uint32_t absGoRice, uint32_t c1c2Idx)
{
    X265_CHECK(absLevel, "absLevel should not be zero\n");

    if (diffLevel < 0)
    {
        X265_CHECK((absLevel == 1) || (absLevel == 2), "absLevel range check failure\n");

        uint32_t rate = greaterOneBits[(absLevel == 2)];
        if (absLevel == 2)
            rate += levelAbsBits[0];
        return rate;
    }
    else
    {
        uint32_t rate;
        uint32_t symbol = diffLevel;
        if ((symbol >> absGoRice) < COEF_REMAIN_BIN_REDUCTION)
        {
            uint32_t length = symbol >> absGoRice;
            rate = (length + 1 + absGoRice) << 15;
        }
        else
        {
            uint32_t length = 0;
            symbol = (symbol >> absGoRice) - COEF_REMAIN_BIN_REDUCTION;
            if (symbol)
            {
                unsigned long idx;
                CLZ32(idx, symbol + 1);
                length = idx;
            }

            rate = (COEF_REMAIN_BIN_REDUCTION + length + absGoRice + 1 + length) << 15;
        }
        if (c1c2Idx & 1)
            rate += greaterOneBits[1];
        if (c1c2Idx == 3)
            rate += levelAbsBits[1];
        return rate;
    }
}

}

Quant::Quant()
{
    m_resiDctCoeff = NULL;
    m_fencDctCoeff = NULL;
    m_fencShortBuf = NULL;
}

bool Quant::init(bool useRDOQ, double psyScale, const ScalingList& scalingList)
{
    m_useRDOQ = useRDOQ;
    m_psyRdoqScale = (uint64_t)(psyScale * 256.0);
    m_scalingList = &scalingList;
    m_resiDctCoeff = X265_MALLOC(coeff_t, MAX_TR_SIZE * MAX_TR_SIZE * 2);
    m_fencDctCoeff = m_resiDctCoeff + (MAX_TR_SIZE * MAX_TR_SIZE);
    m_fencShortBuf = X265_MALLOC(int16_t, MAX_TR_SIZE * MAX_TR_SIZE);
    
    return m_resiDctCoeff && m_fencShortBuf;
}

Quant::~Quant()
{
    X265_FREE(m_resiDctCoeff);
    X265_FREE(m_fencShortBuf);
}

void Quant::setQPforQuant(TComDataCU* cu)
{
    int qpy = cu->getQP(0);
    int chFmt = cu->getChromaFormat();

    m_qpParam[TEXT_LUMA].setQpParam(qpy + QP_BD_OFFSET);
    setQPforQuant(qpy, TEXT_CHROMA_U, cu->m_slice->m_pps->chromaCbQpOffset, chFmt);
    setQPforQuant(qpy, TEXT_CHROMA_V, cu->m_slice->m_pps->chromaCrQpOffset, chFmt);
}

void Quant::setQPforQuant(int qpy, TextType ttype, int chromaQPOffset, int chFmt)
{
    X265_CHECK(ttype == TEXT_CHROMA_U || ttype == TEXT_CHROMA_V, "invalid ttype\n");

    int qp = Clip3(-QP_BD_OFFSET, 57, qpy + chromaQPOffset);
    if (qp >= 30)
    {
        if (chFmt == X265_CSP_I420)
            qp = g_chromaScale[qp];
        else
            qp = X265_MIN(qp, 51);
    }
    m_qpParam[ttype].setQpParam(qp + QP_BD_OFFSET);
}

/* To minimize the distortion only. No rate is considered */
uint32_t Quant::signBitHidingHDQ(coeff_t* coeff, int32_t* deltaU, uint32_t numSig, const TUEntropyCodingParameters &codeParams)
{
    const uint32_t log2TrSizeCG = codeParams.log2TrSizeCG;
    const uint16_t *scan = codeParams.scan;
    bool lastCG = true;

    for (int cg = (1 << log2TrSizeCG * 2) - 1; cg >= 0; cg--)
    {
        int cgStartPos = cg << LOG2_SCAN_SET_SIZE;
        int n;

        for (n = SCAN_SET_SIZE - 1; n >= 0; --n)
            if (coeff[scan[n + cgStartPos]])
                break;
        if (n < 0)
            continue;

        int lastNZPosInCG = n;

        for (n = 0;; n++)
            if (coeff[scan[n + cgStartPos]])
                break;

        int firstNZPosInCG = n;

        if (lastNZPosInCG - firstNZPosInCG >= SBH_THRESHOLD)
        {
            uint32_t signbit = coeff[scan[cgStartPos + firstNZPosInCG]] > 0 ? 0 : 1;
            uint32_t absSum = 0;

            for (n = firstNZPosInCG; n <= lastNZPosInCG; n++)
                absSum += coeff[scan[n + cgStartPos]];

            if (signbit != (absSum & 0x1)) // compare signbit with sum_parity
            {
                int minCostInc = MAX_INT,  minPos = -1, finalChange = 0, curCost = MAX_INT, curChange = 0;

                for (n = (lastCG ? lastNZPosInCG : SCAN_SET_SIZE - 1); n >= 0; --n)
                {
                    uint32_t blkPos = scan[n + cgStartPos];
                    if (coeff[blkPos])
                    {
                        if (deltaU[blkPos] > 0)
                        {
                            curCost = -deltaU[blkPos];
                            curChange = 1;
                        }
                        else
                        {
                            if (n == firstNZPosInCG && abs(coeff[blkPos]) == 1)
                                curCost = MAX_INT;
                            else
                            {
                                curCost = deltaU[blkPos];
                                curChange = -1;
                            }
                        }
                    }
                    else
                    {
                        if (n < firstNZPosInCG)
                        {
                            uint32_t thisSignBit = m_resiDctCoeff[blkPos] >= 0 ? 0 : 1;
                            if (thisSignBit != signbit)
                                curCost = MAX_INT;
                            else
                            {
                                curCost = -deltaU[blkPos];
                                curChange = 1;
                            }
                        }
                        else
                        {
                            curCost = -deltaU[blkPos];
                            curChange = 1;
                        }
                    }

                    if (curCost < minCostInc)
                    {
                        minCostInc = curCost;
                        finalChange = curChange;
                        minPos = blkPos;
                    }
                }

                /* do not allow change to violate coeff clamp */
                if (coeff[minPos] == 32767 || coeff[minPos] == -32768)
                    finalChange = -1;

                if (!coeff[minPos])
                    numSig++;
                else if (finalChange == -1 && abs(coeff[minPos]) == 1)
                    numSig--;

                if (m_resiDctCoeff[minPos] >= 0)
                    coeff[minPos] += finalChange;
                else
                    coeff[minPos] -= finalChange;
            }
        }

        lastCG = false;
    }

    return numSig;
}

uint32_t Quant::transformNxN(TComDataCU* cu, pixel* fenc, uint32_t fencStride, int16_t* residual, uint32_t stride,
                             coeff_t* coeff, uint32_t log2TrSize, TextType ttype, uint32_t absPartIdx, bool useTransformSkip)
{
    if (cu->getCUTransquantBypass(absPartIdx))
    {
        X265_CHECK(log2TrSize >= 2 && log2TrSize <= 5, "Block size mistake!\n");
        return primitives.cvt16to32_cnt[log2TrSize - 2](coeff, residual, stride);
    }

    bool isLuma  = ttype == TEXT_LUMA;
    bool usePsy  = m_psyRdoqScale && isLuma && !useTransformSkip;
    bool isIntra = cu->getPredictionMode(absPartIdx) == MODE_INTRA;
    int transformShift = MAX_TR_DYNAMIC_RANGE - X265_DEPTH - log2TrSize; // Represents scaling through forward transform
    int trSize = 1 << log2TrSize;

    X265_CHECK((cu->m_slice->m_sps->quadtreeTULog2MaxSize >= log2TrSize), "transform size too large\n");
    if (useTransformSkip)
    {
#if X265_DEPTH <= 10
        primitives.cvt16to32_shl(m_resiDctCoeff, residual, stride, transformShift, trSize);
#else
        if (transformShift >= 0)
            primitives.cvt16to32_shl(m_resiDctCoeff, residual, stride, transformShift, trSize);
        else
        {
            int shift = -transformShift;
            int offset = (1 << (shift - 1));
            primitives.cvt16to32_shr[log2TrSize - 2](m_resiDctCoeff, residual, stride, shift, offset);
        }
#endif
    }
    else
    {
        const uint32_t sizeIdx = log2TrSize - 2;
        int useDST = !sizeIdx && isLuma && isIntra;
        int index = DCT_4x4 + sizeIdx - useDST;

        primitives.dct[index](residual, m_resiDctCoeff, stride);

        /* NOTE: if RDOQ is disabled globally, psy-rdoq is also disabled, so
         * there is no risk of performing this DCT unnecessarily */
        if (usePsy)
        {
            /* perform DCT on source pixels for psy-rdoq */
            primitives.square_copy_ps[sizeIdx](m_fencShortBuf, trSize, fenc, fencStride);
            primitives.dct[index](m_fencShortBuf, m_fencDctCoeff, trSize);
        }

        if (m_nr->bNoiseReduction && !isIntra)
        {
            /* denoise is not applied to intra residual, so DST can be ignored */
            int cat = sizeIdx + 4 * !isLuma;
            int numCoeff = 1 << log2TrSize * 2;
            denoiseDct(m_resiDctCoeff, m_nr->residualSum[cat], m_nr->offsetDenoise[cat], numCoeff);
            m_nr->count[cat]++;
        }
    }

    if (m_useRDOQ)
        return rdoQuant(cu, coeff, log2TrSize, ttype, absPartIdx, usePsy);
    else
    {
        int deltaU[32 * 32];

        int scalingListType = ttype + (isLuma ? 3 : 0);
        int rem = m_qpParam[ttype].rem;
        int per = m_qpParam[ttype].per;
        int32_t *quantCoeff = m_scalingList->m_quantCoef[log2TrSize - 2][scalingListType][rem];

        int qbits = QUANT_SHIFT + per + transformShift;
        int add = (cu->m_slice->m_sliceType == I_SLICE ? 171 : 85) << (qbits - 9);
        int numCoeff = 1 << log2TrSize * 2;

        uint32_t numSig = primitives.quant(m_resiDctCoeff, quantCoeff, deltaU, coeff, qbits, add, numCoeff);

        if (numSig >= 2 && cu->m_slice->m_pps->bSignHideEnabled)
        {
            TUEntropyCodingParameters codeParams;
            cu->getTUEntropyCodingParameters(codeParams, absPartIdx, log2TrSize, isLuma);
            return signBitHidingHDQ(coeff, deltaU, numSig, codeParams);
        }
        else
            return numSig;
    }
}

void Quant::invtransformNxN(bool transQuantBypass, int16_t* residual, uint32_t stride, coeff_t* coeff,
                            uint32_t log2TrSize, TextType ttype, bool bIntra, bool useTransformSkip, uint32_t numSig)
{
    if (transQuantBypass)
    {
        int trSize = 1 << log2TrSize;
        for (int k = 0; k < trSize; k++)
            for (int j = 0; j < trSize; j++)
                residual[k * stride + j] = (int16_t)(coeff[k * trSize + j]);
        return;
    }

    // Values need to pass as input parameter in dequant
    int rem = m_qpParam[ttype].rem;
    int per = m_qpParam[ttype].per;
    int transformShift = MAX_TR_DYNAMIC_RANGE - X265_DEPTH - log2TrSize;
    int shift = QUANT_IQUANT_SHIFT - QUANT_SHIFT - transformShift;
    int numCoeff = 1 << log2TrSize * 2;

    if (m_scalingList->m_bEnabled)
    {
        int scalingListType = (bIntra ? 0 : 3) + ttype;
        int32_t *dequantCoef = m_scalingList->m_dequantCoef[log2TrSize - 2][scalingListType][rem];
        primitives.dequant_scaling(coeff, dequantCoef, m_resiDctCoeff, numCoeff, per, shift);
    }
    else
    {
        int scale = m_scalingList->s_invQuantScales[rem] << per;
        primitives.dequant_normal(coeff, m_resiDctCoeff, numCoeff, scale, shift);
    }

    if (useTransformSkip)
    {
        int trSize = 1 << log2TrSize;
        shift = transformShift;

#if X265_DEPTH <= 10
        primitives.cvt32to16_shr(residual, m_resiDctCoeff, stride, shift, trSize);
#else
        if (shift >= 0)
            primitives.cvt32to16_shr(residual, m_resiDctCoeff, stride, shift, trSize);
        else
            primitives.cvt32to16_shl[log2TrSize - 2](residual, m_resiDctCoeff, stride, -shift);
#endif
    }
    else
    {
        const uint32_t sizeIdx = log2TrSize - 2;
        int useDST = !sizeIdx && ttype == TEXT_LUMA && bIntra;

        X265_CHECK((int)numSig == primitives.count_nonzero(coeff, 1 << log2TrSize * 2), "numSig differ\n");

        // DC only
        if (numSig == 1 && coeff[0] != 0 && !useDST)
        {
            const int shift_1st = 7;
            const int add_1st = 1 << (shift_1st - 1);
            const int shift_2nd = 12 - (X265_DEPTH - 8);
            const int add_2nd = 1 << (shift_2nd - 1);

            int dc_val = (((m_resiDctCoeff[0] * 64 + add_1st) >> shift_1st) * 64 + add_2nd) >> shift_2nd;
            primitives.blockfill_s[sizeIdx](residual, stride, (int16_t)dc_val);
            return;
        }

        // TODO: this may need larger data types for X265_DEPTH > 10
        primitives.idct[IDCT_4x4 + sizeIdx - useDST](m_resiDctCoeff, residual, stride);
    }
}

/* Rate distortion optimized quantization for entropy coding engines using
 * probability models like CABAC */
uint32_t Quant::rdoQuant(TComDataCU* cu, coeff_t* dstCoeff, uint32_t log2TrSize, TextType ttype, uint32_t absPartIdx, bool usePsy)
{
    uint32_t trSize = 1 << log2TrSize;
    int transformShift = MAX_TR_DYNAMIC_RANGE - X265_DEPTH - log2TrSize; /* Represents scaling through forward transform */
    int scalingListType = (cu->isIntra(absPartIdx) ? 0 : 3) + ttype;

    X265_CHECK(scalingListType < 6, "scaling list type out of range\n");

    int rem = m_qpParam[ttype].rem;
    int per = m_qpParam[ttype].per;
    int qbits = QUANT_SHIFT + per + transformShift; /* Right shift of non-RDOQ quantizer level = (coeff*Q + offset)>>q_bits */
    int add = (1 << (qbits - 1));
    int32_t *qCoef = m_scalingList->m_quantCoef[log2TrSize - 2][scalingListType][rem];

    int numCoeff = 1 << log2TrSize * 2;
    int scaledCoeff[32 * 32];
    uint32_t numSig = primitives.nquant(m_resiDctCoeff, qCoef, scaledCoeff, dstCoeff, qbits, add, numCoeff);

    X265_CHECK((int)numSig == primitives.count_nonzero(dstCoeff, numCoeff), "numSig differ\n");
    if (!numSig)
        return 0;

    x265_emms();

    /* unquant constants for psy-rdoq. The dequant coefficients have a (1<<4) scale applied
     * that must be removed during unquant.  This may be larger than the QP upshift, which
     * would turn some shifts around. To avoid this we add an optional pre-up-shift of the
     * quantized level. Note that in real dequant there is clipping at several stages. We
     * skip the clipping when measuring RD cost. */
    int32_t *unquantScale = m_scalingList->m_dequantCoef[log2TrSize - 2][scalingListType][rem];
    int unquantShift = QUANT_IQUANT_SHIFT - QUANT_SHIFT - transformShift;
    int unquantRound, unquantPreshift;
    unquantShift += 4;
    if (unquantShift > per)
    {
        unquantRound = 1 << (unquantShift - per - 1);
        unquantPreshift = 0;
    }
    else
    {
        unquantPreshift = 4;
        unquantShift += unquantPreshift;
        unquantRound = 0;
    }
    int scaleBits = SCALE_BITS - 2 * transformShift;

    double lambda2 = m_lambdas[ttype];
    bool bIsLuma = ttype == TEXT_LUMA;

    double totalUncodedCost = 0;
    double costCoeff[32 * 32];   /* d*d + lambda * bits */
    double costUncoded[32 * 32]; /* d*d + lambda * 0    */
    double costSig[32 * 32];     /* lambda * bits       */

    int rateIncUp[32 * 32];      /* signal overhead of increasing level */
    int rateIncDown[32 * 32];    /* signal overhead of decreasing level */
    int sigRateDelta[32 * 32];   /* signal difference between zero and non-zero */
    int deltaU[32 * 32];

    double   costCoeffGroupSig[MLS_GRP_NUM]; /* lambda * bits of group coding cost */
    uint64_t sigCoeffGroupFlag64 = 0;

    uint32_t ctxSet      = 0;
    int    c1            = 1;
    int    c2            = 0;
    uint32_t goRiceParam = 0;
    uint32_t c1Idx       = 0;
    uint32_t c2Idx       = 0;
    int cgLastScanPos    = -1;
    int lastScanPos      = -1;
    const uint32_t cgSize = (1 << MLS_CG_SIZE); /* 4x4 num coef = 16 */

    /* Total rate distortion cost of this transform block, counting te distortion of uncoded blocks,
     * the distortion and signal cost of coded blocks, and the coding cost of significant
     * coefficient and coefficient group bitmaps */
    double totalRdCost = 0;

    TUEntropyCodingParameters codeParams;
    cu->getTUEntropyCodingParameters(codeParams, absPartIdx, log2TrSize, bIsLuma);
    const uint32_t cgNum = 1 << codeParams.log2TrSizeCG * 2;

    uint32_t scanPos;
    coeffGroupRDStats cgRdStats;

    /* iterate over coding groups in reverse scan order */
    for (int cgScanPos = cgNum - 1; cgScanPos >= 0; cgScanPos--)
    {
        const uint32_t cgBlkPos = codeParams.scanCG[cgScanPos];
        const uint32_t cgPosY   = cgBlkPos >> codeParams.log2TrSizeCG;
        const uint32_t cgPosX   = cgBlkPos - (cgPosY << codeParams.log2TrSizeCG);
        const uint64_t cgBlkPosMask = ((uint64_t)1 << cgBlkPos);
        memset(&cgRdStats, 0, sizeof(coeffGroupRDStats));

        const int patternSigCtx = calcPatternSigCtx(sigCoeffGroupFlag64, cgPosX, cgPosY, codeParams.log2TrSizeCG);

        /* iterate over coefficients in each group in reverse scan order */
        for (int scanPosinCG = cgSize - 1; scanPosinCG >= 0; scanPosinCG--)
        {
            scanPos              = (cgScanPos << MLS_CG_SIZE) + scanPosinCG;
            uint32_t blkPos      = codeParams.scan[scanPos];
            uint32_t maxAbsLevel = abs(dstCoeff[blkPos]);             /* abs(quantized coeff) */
            int signCoef         = m_resiDctCoeff[blkPos];            /* pre-quantization DCT coeff */
            int predictedCoef    = m_fencDctCoeff[blkPos] - signCoef; /* predicted DCT = source DCT - residual DCT*/

            /* RDOQ measures distortion as the squared difference between the unquantized coded level
             * and the original DCT coefficient. The result is shifted scaleBits to account for the
             * FIX15 nature of the CABAC cost tables minus the forward transform scale */

            /* cost of not coding this coefficient (all distortion, no signal bits) */
            costUncoded[scanPos] = (double)((uint64_t)(signCoef * signCoef) << scaleBits);
            if (usePsy && blkPos)
                /* when no coefficient is coded, predicted coef == recon coef */
                costUncoded[scanPos] -= (int)(((m_psyRdoqScale * predictedCoef) << scaleBits) >> 8);

            totalUncodedCost += costUncoded[scanPos];

            if (maxAbsLevel && lastScanPos < 0)
            {
                /* remember the first non-zero coef found in this reverse scan as the last pos */
                lastScanPos   = scanPos;
                ctxSet        = (scanPos < SCAN_SET_SIZE || !bIsLuma) ? 0 : 2;
                cgLastScanPos = cgScanPos;
            }

            if (lastScanPos < 0)
            {
                /* No non-zero coefficient yet found, but this does not mean
                 * there is no uncoded-cost for this coefficient. Pre-
                 * quantization the coefficient may have been non-zero */
                costCoeff[scanPos] = 0;
                totalRdCost += costUncoded[scanPos];

                /* coefficients after lastNZ have no signal cost */
                costSig[scanPos] = 0;
            }
            else
            {
                const uint32_t c1c2Idx = ((c1Idx - 8) >> (sizeof(int) * CHAR_BIT - 1)) + (((-(int)c2Idx) >> (sizeof(int) * CHAR_BIT - 1)) + 1) * 2;
                const uint32_t baseLevel = ((uint32_t)0xD9 >> (c1c2Idx * 2)) & 3;  // {1, 2, 1, 3}

                X265_CHECK(!!((int)c1Idx < C1FLAG_NUMBER) == (int)((c1Idx - 8) >> (sizeof(int) * CHAR_BIT - 1)), "scan validation 1\n");
                X265_CHECK(!!(c2Idx == 0) == ((-(int)c2Idx) >> (sizeof(int) * CHAR_BIT - 1)) + 1, "scan validation 2\n");
                X265_CHECK((int)baseLevel == ((c1Idx < C1FLAG_NUMBER) ? (2 + (c2Idx == 0)) : 1), "scan validation 3\n");

                // coefficient level estimation
                const uint32_t oneCtx = 4 * ctxSet + c1;
                const uint32_t absCtx = ctxSet + c2;
                const int *greaterOneBits = m_estBitsSbac.greaterOneBits[oneCtx];
                const int *levelAbsBits = m_estBitsSbac.levelAbsBits[absCtx];

                uint32_t level = 0;
                uint32_t sigCoefBits = 0;
                costCoeff[scanPos] = MAX_DOUBLE;

                if ((int)scanPos == lastScanPos)
                    sigRateDelta[blkPos] = 0;
                else
                {
                    const uint32_t ctxSig = getSigCtxInc(patternSigCtx, log2TrSize, trSize, blkPos, bIsLuma, codeParams.firstSignificanceMapContext);
                    if (maxAbsLevel < 3)
                    {
                        /* set default costs to uncoded costs */
                        costSig[scanPos] = lambda2 * m_estBitsSbac.significantBits[ctxSig][0];
                        costCoeff[scanPos] = costUncoded[scanPos] + costSig[scanPos];
                    }
                    sigRateDelta[blkPos] = m_estBitsSbac.significantBits[ctxSig][1] - m_estBitsSbac.significantBits[ctxSig][0];
                    sigCoefBits = m_estBitsSbac.significantBits[ctxSig][1];
                }
                if (maxAbsLevel)
                {
                    uint32_t minAbsLevel = X265_MAX(maxAbsLevel - 1, 1);
                    for (uint32_t lvl = maxAbsLevel; lvl >= minAbsLevel; lvl--)
                    {
                        uint32_t levelBits = getICRateCost(lvl, lvl - baseLevel, greaterOneBits, levelAbsBits, goRiceParam, c1c2Idx) + IEP_RATE;

                        int unquantAbsLevel = ((lvl << unquantPreshift) * (unquantScale[blkPos] << per) + unquantRound) >> unquantShift;
                        int d = unquantAbsLevel - abs(signCoef);
                        uint64_t distortion = ((uint64_t)(d * d)) << scaleBits;
                        double curCost = distortion + lambda2 * (sigCoefBits + levelBits);

                        // Psy RDOQ: bias in favor of higher AC coefficients in the reconstructed frame
                        if (usePsy && blkPos)
                        {
                            int reconCoef = abs(unquantAbsLevel + SIGN(predictedCoef, signCoef));
                            curCost -= (int)(((m_psyRdoqScale * reconCoef) << scaleBits) >> 8);
                        }

                        if (curCost < costCoeff[scanPos])
                        {
                            level = lvl;
                            costCoeff[scanPos] = curCost;
                            costSig[scanPos] = lambda2 * sigCoefBits;
                        }
                    }
                }

                deltaU[blkPos] = (scaledCoeff[blkPos] - ((int)level << qbits)) >> (qbits - 8);
                dstCoeff[blkPos] = level;
                totalRdCost += costCoeff[scanPos];

                /* record costs for sign-hiding performed at the end */
                if (level)
                {
                    int rateNow = getICRate(level, level - baseLevel, greaterOneBits, levelAbsBits, goRiceParam, c1c2Idx);
                    rateIncUp[blkPos] = getICRate(level + 1, level + 1 - baseLevel, greaterOneBits, levelAbsBits, goRiceParam, c1c2Idx) - rateNow;
                    rateIncDown[blkPos] = getICRate(level - 1, level - 1 - baseLevel, greaterOneBits, levelAbsBits, goRiceParam, c1c2Idx) - rateNow;
                }
                else
                {
                    rateIncUp[blkPos] = greaterOneBits[0];
                    rateIncDown[blkPos] = 0;
                }

                /* Update CABAC estimation state */
                if (level >= baseLevel && goRiceParam < 4 && level > (3U << goRiceParam))
                    goRiceParam++;

                c1Idx -= (-(int32_t)level) >> 31;

                /* update bin model */
                if (level > 1)
                {
                    c1 = 0;
                    c2 += (uint32_t)(c2 - 2) >> 31;
                    c2Idx++;
                }
                else if ((c1 < 3) && (c1 > 0) && level)
                    c1++;

                /* context set update */
                if (!(scanPos % SCAN_SET_SIZE) && scanPos)
                {
                    c2 = 0;
                    goRiceParam = 0;

                    c1Idx = 0;
                    c2Idx = 0;
                    ctxSet = (scanPos == SCAN_SET_SIZE || !bIsLuma) ? 0 : 2;
                    X265_CHECK(c1 >= 0, "c1 is negative\n");
                    ctxSet -= ((int32_t)(c1 - 1) >> 31);
                    c1 = 1;
                }
            }

            cgRdStats.sigCost += costSig[scanPos];
            if (!scanPosinCG)
                cgRdStats.sigCost0 = costSig[scanPos];

            if (dstCoeff[blkPos])
            {
                sigCoeffGroupFlag64 |= cgBlkPosMask;
                cgRdStats.codedLevelAndDist += costCoeff[scanPos] - costSig[scanPos];
                cgRdStats.uncodedDist += costUncoded[scanPos];
                cgRdStats.nnzBeforePos0 += scanPosinCG;
            }
        } /* end for (scanPosinCG) */

        /* Summarize costs for the coeff group */
        if (cgLastScanPos >= 0)
        {
            costCoeffGroupSig[cgScanPos] = 0;
            if (!cgScanPos)
            {
                /* coeff group 0 is implied to be present, no signal cost */
                sigCoeffGroupFlag64 |= cgBlkPosMask;
            }
            else
            {
                if (!(sigCoeffGroupFlag64 & cgBlkPosMask))
                {
                    /* no coefficients were coded in this coefficient group */
                    uint32_t ctxSig = getSigCoeffGroupCtxInc(sigCoeffGroupFlag64, cgPosX, cgPosY, codeParams.log2TrSizeCG);
                    costCoeffGroupSig[cgScanPos] = lambda2 * m_estBitsSbac.significantCoeffGroupBits[ctxSig][0];
                    totalRdCost += costCoeffGroupSig[cgScanPos];  /* add cost of 0 bit in significant CG bitmap */
                    totalRdCost -= cgRdStats.sigCost;             /* remove cost of significant coefficient bitmap */
                }
                else
                {
                    /* skip the last coefficient group, which will be handled together with last position below */
                    if (cgScanPos < cgLastScanPos)
                    {
                        uint32_t sigCtx = getSigCoeffGroupCtxInc(sigCoeffGroupFlag64, cgPosX, cgPosY, codeParams.log2TrSizeCG);

                        if (!cgRdStats.nnzBeforePos0)
                        {
                            /* if only coeff 0 in this CG is coded, its significant coeff bit is implied */
                            totalRdCost -= cgRdStats.sigCost0;
                            cgRdStats.sigCost -= cgRdStats.sigCost0;
                        }

                        /* calculate cost of not signaling this coefficient group */
                        double costZeroCG = totalRdCost + lambda2 * m_estBitsSbac.significantCoeffGroupBits[sigCtx][0];
                        costZeroCG += cgRdStats.uncodedDist;       /* add distortion for resetting non-zero levels to zero levels */
                        costZeroCG -= cgRdStats.codedLevelAndDist; /* remove distortion and level cost of coded coefficients */
                        costZeroCG -= cgRdStats.sigCost;           /* remove signaling cost of significant coeff bitmap */

                        costCoeffGroupSig[cgScanPos] = lambda2 * m_estBitsSbac.significantCoeffGroupBits[sigCtx][1];
                        totalRdCost += costCoeffGroupSig[cgScanPos];  /* add the cost of 1 bit in significant CG bitmap */

                        /* if we can save RD cost, change this group to all-zero group */
                        if (costZeroCG < totalRdCost)
                        {
                            sigCoeffGroupFlag64 &= ~cgBlkPosMask;
                            totalRdCost = costZeroCG;
                            costCoeffGroupSig[cgScanPos] = lambda2 * m_estBitsSbac.significantCoeffGroupBits[sigCtx][0];

                            /* reset all coeffs to 0. UNCODE THIS COEFF GROUP! */
                            for (int scanPosinCG = cgSize - 1; scanPosinCG >= 0; scanPosinCG--)
                            {
                                scanPos         = cgScanPos * cgSize + scanPosinCG;
                                uint32_t blkPos = codeParams.scan[scanPos];
                                if (dstCoeff[blkPos])
                                {
                                    costCoeff[scanPos] = costUncoded[scanPos];
                                    costSig[scanPos] = 0;
                                }
                                dstCoeff[blkPos] = 0;
                            }
                        }
                    }
                }
            }
        }
    } /* end for (cgScanPos) */

    if (lastScanPos < 0)
    {
        /* this should be un-possible */
        return 0;
    }

    /* estimate cost of uncoded block CBF=0 */
    double bestCost;
    if (!cu->isIntra(absPartIdx) && bIsLuma && !cu->getTransformIdx(absPartIdx))
    {
        bestCost  = totalUncodedCost + lambda2 * m_estBitsSbac.blockRootCbpBits[0][0];
        totalRdCost += lambda2 * m_estBitsSbac.blockRootCbpBits[0][1];
    }
    else
    {
        int ctx = cu->getCtxQtCbf(ttype, cu->getTransformIdx(absPartIdx));
        bestCost = totalUncodedCost + lambda2 * m_estBitsSbac.blockCbpBits[ctx][0];
        totalRdCost += lambda2 * m_estBitsSbac.blockCbpBits[ctx][1];
    }

    /* Find the least cost last non-zero coefficient position  */
    int  bestLastIdx = 0;
    bool foundLast = false;
    for (int cgScanPos = cgLastScanPos; cgScanPos >= 0 && !foundLast; cgScanPos--)
    {
        uint32_t cgBlkPos = codeParams.scanCG[cgScanPos];
        totalRdCost -= costCoeffGroupSig[cgScanPos];

        if (!(sigCoeffGroupFlag64 & ((uint64_t)1 << cgBlkPos))) /* skip empty CGs */
            continue;

        for (int scanPosinCG = cgSize - 1; scanPosinCG >= 0; scanPosinCG--)
        {
            scanPos = cgScanPos * cgSize + scanPosinCG;
            if ((int)scanPos > lastScanPos)
                continue;

            uint32_t blkPos = codeParams.scan[scanPos];
            if (dstCoeff[blkPos])
            {
                /* found the current last non-zero; estimate the trade-off of setting it to zero */
                uint32_t posY = blkPos >> log2TrSize;
                uint32_t posX = blkPos - (posY << log2TrSize);
                uint32_t bitsLast = codeParams.scanType == SCAN_VER ? getRateLast(posY, posX) : getRateLast(posX, posY);
                double rdCostLast = totalRdCost + lambda2 * bitsLast - costSig[scanPos];

                /* TODO: perhaps psy-cost should be used here as well */

                if (rdCostLast < bestCost)
                {
                    bestLastIdx = scanPos + 1;
                    bestCost = rdCostLast;
                }
                if (dstCoeff[blkPos] > 1)
                {
                    foundLast = true;
                    break;
                }
                /* UNCODE THIS COEFF! */
                totalRdCost -= costCoeff[scanPos];
                totalRdCost += costUncoded[scanPos];
            }
            else
                totalRdCost -= costSig[scanPos];
        }
    }

    /* recount non-zero coefficients and re-apply sign of DCT coef */
    numSig = 0;
    for (int pos = 0; pos < bestLastIdx; pos++)
    {
        int blkPos = codeParams.scan[pos];
        int level  = dstCoeff[blkPos];
        numSig += (level != 0);

        uint32_t mask = (int32_t)m_resiDctCoeff[blkPos] >> 31;
        dstCoeff[blkPos] = (level ^ mask) - mask;
    }

    /* clean uncoded coefficients */
    for (int pos = bestLastIdx; pos <= lastScanPos; pos++)
        dstCoeff[codeParams.scan[pos]] = 0;

    /* rate-distortion based sign-hiding */
    if (cu->m_slice->m_pps->bSignHideEnabled && numSig >= 2)
    {
        int64_t invQuant = ScalingList::s_invQuantScales[rem] << per;
        int64_t rdFactor = (int64_t)((invQuant * invQuant) / (lambda2 * 16) + 0.5);

        int lastCG = true;
        for (int subSet = cgLastScanPos; subSet >= 0; subSet--)
        {
            int subPos = subSet << LOG2_SCAN_SET_SIZE;
            int n;

            /* measure distance between first and last non-zero coef in this
             * coding group */
            for (n = SCAN_SET_SIZE - 1; n >= 0; --n)
                if (dstCoeff[codeParams.scan[n + subPos]])
                    break;
            if (n < 0)
                continue;

            int lastNZPosInCG = n;

            for (n = 0;; n++)
                if (dstCoeff[codeParams.scan[n + subPos]])
                    break;

            int firstNZPosInCG = n;

            if (lastNZPosInCG - firstNZPosInCG >= SBH_THRESHOLD)
            {
                uint32_t signbit = (dstCoeff[codeParams.scan[subPos + firstNZPosInCG]] > 0 ? 0 : 1);
                int absSum = 0;

                for (n = firstNZPosInCG; n <= lastNZPosInCG; n++)
                    absSum += dstCoeff[codeParams.scan[n + subPos]];

                if (signbit != (absSum & 1U))
                {
                    /* We must find a coeff to toggle up or down so the sign bit of the first non-zero coeff
                     * is properly implied. Note dstCoeff[] are signed by this point but curChange and
                     * finalChange imply absolute levels (+1 is away from zero, -1 is towards zero) */

                    int64_t minCostInc = MAX_INT64, curCost = MAX_INT64;
                    int minPos = -1, finalChange = 0, curChange = 0;

                    /* in this section, RDOQ uses yet another novel approach at measuring cost. rdFactor is
                     * roughly 1/errScale of the earlier section.  Except it also divides by lambda2 to avoid
                     * having to multiply the signal bits portion. Here again the cost scale factor (1 << 15)
                     * is hard coded in three more places */

                    for (n = (lastCG ? lastNZPosInCG : SCAN_SET_SIZE - 1); n >= 0; --n)
                    {
                        uint32_t blkPos = codeParams.scan[n + subPos];
                        if (dstCoeff[blkPos])
                        {
                            int64_t costUp = rdFactor * (-deltaU[blkPos]) + rateIncUp[blkPos];

                            /* if decrementing would make the coeff 0, we can remove the sigRateDelta */
                            bool isOne = abs(dstCoeff[blkPos]) == 1;
                            int64_t costDown = rdFactor * (deltaU[blkPos]) + rateIncDown[blkPos] -
                                (isOne ? ((1 << 15) + sigRateDelta[blkPos]) : 0);

                            if (lastCG && lastNZPosInCG == n && isOne)
                                costDown -= (4 << 15);

                            if (costUp < costDown)
                            {
                                curCost = costUp;
                                curChange =  1;
                            }
                            else
                            {
                                curChange = -1;
                                if (n == firstNZPosInCG && isOne)
                                    curCost = MAX_INT64;
                                else
                                    curCost = costDown;
                            }
                        }
                        else
                        {
                            /* evaluate changing an uncoded coeff 0 to a coded coeff +/-1 */
                            curCost = rdFactor * (-(abs(deltaU[blkPos]))) + (1 << 15) + rateIncUp[blkPos] + sigRateDelta[blkPos];
                            curChange = 1;

                            if (n < firstNZPosInCG)
                            {
                                uint32_t thissignbit = (m_resiDctCoeff[blkPos] >= 0 ? 0 : 1);
                                if (thissignbit != signbit)
                                    curCost = MAX_INT64;
                            }
                        }

                        if (curCost < minCostInc)
                        {
                            minCostInc = curCost;
                            finalChange = curChange;
                            minPos = blkPos;
                        }
                    }

                    if (dstCoeff[minPos] == 32767 || dstCoeff[minPos] == -32768)
                        finalChange = -1;

                    if (dstCoeff[minPos] == 0)
                        numSig++;
                    else if (finalChange == -1 && abs(dstCoeff[minPos]) == 1)
                        numSig--;

                    if (m_resiDctCoeff[minPos] >= 0)
                        dstCoeff[minPos] += finalChange;
                    else
                        dstCoeff[minPos] -= finalChange;
                }
            }

            lastCG = false;
        }
    }

    return numSig;
}

/** Pattern decision for context derivation process of significant_coeff_flag */
uint32_t Quant::calcPatternSigCtx(uint64_t sigCoeffGroupFlag64, uint32_t cgPosX, uint32_t cgPosY, uint32_t log2TrSizeCG)
{
    if (!log2TrSizeCG)
        return 0;

    const uint32_t trSizeCG = 1 << log2TrSizeCG;
    X265_CHECK(trSizeCG <= 8, "transform CG is too large\n");
    const uint32_t sigPos = (uint32_t)(sigCoeffGroupFlag64 >> (1 + (cgPosY << log2TrSizeCG) + cgPosX));
    const uint32_t sigRight = ((int32_t)(cgPosX - (trSizeCG - 1)) >> 31) & (sigPos & 1);
    const uint32_t sigLower = ((int32_t)(cgPosY - (trSizeCG - 1)) >> 31) & (sigPos >> (trSizeCG - 2)) & 2;

    return sigRight + sigLower;
}

/** Context derivation process of coeff_abs_significant_flag */
uint32_t Quant::getSigCtxInc(uint32_t patternSigCtx, uint32_t log2TrSize, uint32_t trSize, uint32_t blkPos, bool bIsLuma,
                             uint32_t firstSignificanceMapContext)
{
    static const uint8_t ctxIndMap[16] =
    {
        0, 1, 4, 5,
        2, 3, 4, 5,
        6, 6, 8, 8,
        7, 7, 8, 8
    };

    if (!blkPos) // special case for the DC context variable
        return 0;

    if (log2TrSize == 2) // 4x4
        return ctxIndMap[blkPos];

    const uint32_t posY = blkPos >> log2TrSize;
    const uint32_t posX = blkPos & (trSize - 1);
    X265_CHECK((blkPos - (posY << log2TrSize)) == posX, "block pos check failed\n");

    int posXinSubset = blkPos & 3;
    X265_CHECK((posX & 3) == (blkPos & 3), "pos alignment fail\n");
    int posYinSubset = posY & 3;

    // NOTE: [patternSigCtx][posXinSubset][posYinSubset]
    static const uint8_t table_cnt[4][4][4] =
    {
        // patternSigCtx = 0
        {
            { 2, 1, 1, 0 },
            { 1, 1, 0, 0 },
            { 1, 0, 0, 0 },
            { 0, 0, 0, 0 },
        },
        // patternSigCtx = 1
        {
            { 2, 1, 0, 0 },
            { 2, 1, 0, 0 },
            { 2, 1, 0, 0 },
            { 2, 1, 0, 0 },
        },
        // patternSigCtx = 2
        {
            { 2, 2, 2, 2 },
            { 1, 1, 1, 1 },
            { 0, 0, 0, 0 },
            { 0, 0, 0, 0 },
        },
        // patternSigCtx = 3
        {
            { 2, 2, 2, 2 },
            { 2, 2, 2, 2 },
            { 2, 2, 2, 2 },
            { 2, 2, 2, 2 },
        }
    };

    int cnt = table_cnt[patternSigCtx][posXinSubset][posYinSubset];
    int offset = firstSignificanceMapContext;

    offset += cnt;

    return (bIsLuma && (posX | posY) >= 4) ? 3 + offset : offset;
}

/* Calculates the cost of signaling the last significant coefficient in the block */
inline uint32_t Quant::getRateLast(uint32_t posx, uint32_t posy) const
{
    uint32_t ctxX = getGroupIdx(posx);
    uint32_t ctxY = getGroupIdx(posy);
    uint32_t cost = m_estBitsSbac.lastXBits[ctxX] + m_estBitsSbac.lastYBits[ctxY];

    int32_t maskX = (int32_t)(2 - posx) >> 31;
    int32_t maskY = (int32_t)(2 - posy) >> 31;

    cost += maskX & (IEP_RATE * ((ctxX - 2) >> 1));
    cost += maskY & (IEP_RATE * ((ctxY - 2) >> 1));
    return cost;
}

/* Context derivation process of coeff_abs_significant_flag */
uint32_t Quant::getSigCoeffGroupCtxInc(uint64_t cgGroupMask, uint32_t cgPosX, uint32_t cgPosY, uint32_t log2TrSizeCG)
{
    const uint32_t trSizeCG = 1 << log2TrSizeCG;

    const uint32_t sigPos = (uint32_t)(cgGroupMask >> (1 + (cgPosY << log2TrSizeCG) + cgPosX));
    const uint32_t sigRight = ((int32_t)(cgPosX - (trSizeCG - 1)) >> 31) & sigPos;
    const uint32_t sigLower = ((int32_t)(cgPosY - (trSizeCG - 1)) >> 31) & (sigPos >> (trSizeCG - 1));

    return (sigRight | sigLower) & 1;
}
