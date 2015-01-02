/*****************************************************************************
* Copyright (C) 2013 x265 project
*
* Author: Gopu Govindaswamy <gopu@multicorewareinc.com>
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
#include "deblock.h"
#include "framedata.h"
#include "picyuv.h"
#include "slice.h"
#include "mv.h"

using namespace x265;

#define DEBLOCK_SMALLEST_BLOCK  8
#define DEFAULT_INTRA_TC_OFFSET 2

void Deblock::deblockCTU(const CUData* ctu, int32_t dir)
{
    uint8_t blockStrength[MAX_NUM_PARTITIONS];

    memset(blockStrength, 0, sizeof(uint8_t) * m_numPartitions);

    deblockCU(ctu, 0, 0, dir, blockStrength);
}

static inline uint8_t bsCuEdge(const CUData* cu, uint32_t absPartIdx, int32_t dir)
{
    if (dir == Deblock::EDGE_VER)
    {
        if (cu->m_cuPelX + g_zscanToPelX[absPartIdx] > 0)
        {
            uint32_t    tempPartIdx;
            const CUData* tempCU = cu->getPULeft(tempPartIdx, absPartIdx);
            return tempCU ? 2 : 0;
        }
    }
    else
    {
        if (cu->m_cuPelY + g_zscanToPelY[absPartIdx] > 0)
        {
            uint32_t    tempPartIdx;
            const CUData* tempCU = cu->getPUAbove(tempPartIdx, absPartIdx);
            return tempCU ? 2 : 0;
        }
    }

    return 0;
}

/* Deblocking filter process in CU-based (the same function as conventional's)
 * param Edge the direction of the edge in block boundary (horizonta/vertical), which is added newly */
void Deblock::deblockCU(const CUData* cu, uint32_t absPartIdx, uint32_t depth, const int32_t dir, uint8_t blockStrength[])
{
    if (cu->m_predMode[absPartIdx] == MODE_NONE)
        return;

    uint32_t curNumParts = NUM_CU_PARTITIONS >> (depth << 1);

    const SPS& sps = *cu->m_slice->m_sps;

    if (cu->m_cuDepth[absPartIdx] > depth)
    {
        uint32_t qNumParts   = curNumParts >> 2;
        uint32_t xmax = sps.picWidthInLumaSamples  - cu->m_cuPelX;
        uint32_t ymax = sps.picHeightInLumaSamples - cu->m_cuPelY;
        for (uint32_t partIdx = 0; partIdx < 4; partIdx++, absPartIdx += qNumParts)
            if (g_zscanToPelX[absPartIdx] < xmax && g_zscanToPelY[absPartIdx] < ymax)
                deblockCU(cu, absPartIdx, depth + 1, dir, blockStrength);
        return;
    }

    const uint32_t numUnits  = sps.numPartInCUSize >> depth;
    setEdgefilterPU(cu, absPartIdx, dir, blockStrength, numUnits);
    setEdgefilterTU(cu, absPartIdx, depth, dir, blockStrength);
    setEdgefilterMultiple(cu, absPartIdx, dir, 0, bsCuEdge(cu, absPartIdx, dir), blockStrength, numUnits);

    for (uint32_t partIdx = absPartIdx; partIdx < absPartIdx + curNumParts; partIdx++)
    {
        uint32_t bsCheck = !(partIdx & (1 << dir));

        if (bsCheck && blockStrength[partIdx])
            blockStrength[partIdx] = getBoundaryStrength(cu, dir, partIdx, blockStrength);
    }

    const uint32_t partIdxIncr = DEBLOCK_SMALLEST_BLOCK >> LOG2_UNIT_SIZE;
    uint32_t sizeInPU = sps.numPartInCUSize >> depth;
    uint32_t shiftFactor = (dir == EDGE_VER) ? cu->m_hChromaShift : cu->m_vChromaShift;
    uint32_t chromaMask = ((DEBLOCK_SMALLEST_BLOCK << shiftFactor) >> LOG2_UNIT_SIZE) - 1;
    uint32_t e0 = (dir == EDGE_VER ? g_zscanToPelX[absPartIdx] : g_zscanToPelY[absPartIdx]) >> LOG2_UNIT_SIZE;
        
    for (uint32_t e = 0; e < sizeInPU; e += partIdxIncr)
    {
        edgeFilterLuma(cu, absPartIdx, depth, dir, e, blockStrength);
        if (!((e0 + e) & chromaMask))
            edgeFilterChroma(cu, absPartIdx, depth, dir, e, blockStrength);
    }
}

static inline uint32_t calcBsIdx(const CUData* cu, uint32_t absPartIdx, int32_t dir, int32_t edgeIdx, int32_t baseUnitIdx)
{
    uint32_t numPartInCUSize = cu->m_slice->m_sps->numPartInCUSize;

    if (dir)
        return g_rasterToZscan[g_zscanToRaster[absPartIdx] + edgeIdx * numPartInCUSize + baseUnitIdx];
    else
        return g_rasterToZscan[g_zscanToRaster[absPartIdx] + baseUnitIdx * numPartInCUSize + edgeIdx];
}

void Deblock::setEdgefilterMultiple(const CUData* cu, uint32_t scanIdx, int32_t dir, int32_t edgeIdx, uint8_t value, uint8_t blockStrength[], uint32_t numUnits)
{
    X265_CHECK(numUnits > 0, "numUnits edge filter check\n");
    for (uint32_t i = 0; i < numUnits; i++)
    {
        const uint32_t bsidx = calcBsIdx(cu, scanIdx, dir, edgeIdx, i);
        blockStrength[bsidx] = value;
    }
}

void Deblock::setEdgefilterTU(const CUData* cu, uint32_t absPartIdx, uint32_t depth, int32_t dir, uint8_t blockStrength[])
{
    if ((uint32_t)cu->m_tuDepth[absPartIdx] + cu->m_cuDepth[absPartIdx] > depth)
    {
        const uint32_t curNumParts = NUM_CU_PARTITIONS >> (depth << 1);
        const uint32_t qNumParts   = curNumParts >> 2;

        for (uint32_t partIdx = 0; partIdx < 4; partIdx++, absPartIdx += qNumParts)
            setEdgefilterTU(cu, absPartIdx, depth + 1, dir, blockStrength);
        return;
    }

    uint32_t numUnits  = 1 << (cu->m_log2CUSize[absPartIdx] - cu->m_tuDepth[absPartIdx] - LOG2_UNIT_SIZE);
    setEdgefilterMultiple(cu, absPartIdx, dir, 0, 2, blockStrength, numUnits);
}

void Deblock::setEdgefilterPU(const CUData* cu, uint32_t absPartIdx, int32_t dir, uint8_t blockStrength[], uint32_t numUnits)
{
    const uint32_t hNumUnits = numUnits >> 1;
    const uint32_t qNumUnits = numUnits >> 2;

    switch (cu->m_partSize[absPartIdx])
    {
    case SIZE_2NxN:
        if (EDGE_HOR == dir)
            setEdgefilterMultiple(cu, absPartIdx, dir, hNumUnits, 1, blockStrength, numUnits);
        break;
    case SIZE_Nx2N:
        if (EDGE_VER == dir)
            setEdgefilterMultiple(cu, absPartIdx, dir, hNumUnits, 1, blockStrength, numUnits);
        break;
    case SIZE_NxN:
        setEdgefilterMultiple(cu, absPartIdx, dir, hNumUnits, 1, blockStrength, numUnits);
        break;
    case SIZE_2NxnU:
        if (EDGE_HOR == dir)
            setEdgefilterMultiple(cu, absPartIdx, dir, qNumUnits, 1, blockStrength, numUnits);
        break;
    case SIZE_nLx2N:
        if (EDGE_VER == dir)
            setEdgefilterMultiple(cu, absPartIdx, dir, qNumUnits, 1, blockStrength, numUnits);
        break;
    case SIZE_2NxnD:
        if (EDGE_HOR == dir)
            setEdgefilterMultiple(cu, absPartIdx, dir, numUnits - qNumUnits, 1, blockStrength, numUnits);
        break;
    case SIZE_nRx2N:
        if (EDGE_VER == dir)
            setEdgefilterMultiple(cu, absPartIdx, dir, numUnits - qNumUnits, 1, blockStrength, numUnits);
        break;

    case SIZE_2Nx2N:
    default:
        break;
    }
}

uint8_t Deblock::getBoundaryStrength(const CUData* cuQ, int32_t dir, uint32_t partQ, const uint8_t blockStrength[])
{
    // Calculate block index
    uint32_t partP;
    const CUData* cuP = (dir == EDGE_VER ? cuQ->getPULeft(partP, partQ) : cuQ->getPUAbove(partP, partQ));

    // Set BS for Intra MB : BS = 2
    if (cuP->isIntra(partP) || cuQ->isIntra(partQ))
        return 2;

    // Set BS for not Intra MB : BS = 1 or 0
    if (blockStrength[partQ] > 1 &&
        (cuQ->getCbf(partQ, TEXT_LUMA, cuQ->m_tuDepth[partQ]) ||
         cuP->getCbf(partP, TEXT_LUMA, cuP->m_tuDepth[partP])))
        return 1;

    static const MV zeroMv(0, 0);
    const Slice* const sliceQ = cuQ->m_slice;
    const Slice* const sliceP = cuP->m_slice;

    const Frame* refP0 = sliceP->getRefPic(0, cuP->m_refIdx[0][partP]);
    const Frame* refQ0 = sliceQ->getRefPic(0, cuQ->m_refIdx[0][partQ]);
    const MV& mvP0 = refP0 ? cuP->m_mv[0][partP] : zeroMv;
    const MV& mvQ0 = refQ0 ? cuQ->m_mv[0][partQ] : zeroMv;

    if (sliceQ->isInterP() && sliceP->isInterP())
    {
        return ((refP0 != refQ0) ||
                (abs(mvQ0.x - mvP0.x) >= 4) || (abs(mvQ0.y - mvP0.y) >= 4)) ? 1 : 0;
    }

    // (sliceQ->isInterB() || sliceP->isInterB())
    const Frame* refP1 = sliceP->getRefPic(1, cuP->m_refIdx[1][partP]);
    const Frame* refQ1 = sliceQ->getRefPic(1, cuQ->m_refIdx[1][partQ]);
    const MV& mvP1 = refP1 ? cuP->m_mv[1][partP] : zeroMv;
    const MV& mvQ1 = refQ1 ? cuQ->m_mv[1][partQ] : zeroMv;

    if (((refP0 == refQ0) && (refP1 == refQ1)) || ((refP0 == refQ1) && (refP1 == refQ0)))
    {
        if (refP0 != refP1) // Different L0 & L1
        {
            if (refP0 == refQ0)
                return ((abs(mvQ0.x - mvP0.x) >= 4) || (abs(mvQ0.y - mvP0.y) >= 4) ||
                        (abs(mvQ1.x - mvP1.x) >= 4) || (abs(mvQ1.y - mvP1.y) >= 4)) ? 1 : 0;
            else
                return ((abs(mvQ1.x - mvP0.x) >= 4) || (abs(mvQ1.y - mvP0.y) >= 4) ||
                        (abs(mvQ0.x - mvP1.x) >= 4) || (abs(mvQ0.y - mvP1.y) >= 4)) ? 1 : 0;
        }
        else // Same L0 & L1
        {
            return (((abs(mvQ0.x - mvP0.x) >= 4) || (abs(mvQ0.y - mvP0.y) >= 4) ||
                     (abs(mvQ1.x - mvP1.x) >= 4) || (abs(mvQ1.y - mvP1.y) >= 4)) &&
                    ((abs(mvQ1.x - mvP0.x) >= 4) || (abs(mvQ1.y - mvP0.y) >= 4) ||
                     (abs(mvQ0.x - mvP1.x) >= 4) || (abs(mvQ0.y - mvP1.y) >= 4))) ? 1 : 0;
        }
    }
        
    // for all different Ref_Idx
    return 1;
}

static inline int32_t calcDP(pixel* src, intptr_t offset)
{
    return abs(static_cast<int32_t>(src[-offset * 3]) - 2 * src[-offset * 2] + src[-offset]);
}

static inline int32_t calcDQ(pixel* src, intptr_t offset)
{
    return abs(static_cast<int32_t>(src[0]) - 2 * src[offset] + src[offset * 2]);
}

static inline bool useStrongFiltering(intptr_t offset, int32_t beta, int32_t tc, pixel* src)
{
    int16_t m4     = (int16_t)src[0];
    int16_t m3     = (int16_t)src[-offset];
    int16_t m7     = (int16_t)src[offset * 3];
    int16_t m0     = (int16_t)src[-offset * 4];
    int32_t strong = abs(m0 - m3) + abs(m7 - m4);

    return (strong < (beta >> 3)) && (abs(m3 - m4) < ((tc * 5 + 1) >> 1));
}

/* Deblocking for the luminance component with strong or weak filter
 * \param src     pointer to picture data
 * \param offset  offset value for picture data
 * \param tc      tc value
 * \param maskP   indicator to enable filtering on partP
 * \param maskQ   indicator to enable filtering on partQ
 * \param maskP1  decision weak filter/no filter for partP
 * \param maskQ1  decision weak filter/no filter for partQ */
static inline void pelFilterLumaStrong(pixel* src, intptr_t srcStep, intptr_t offset, int32_t tc, int32_t maskP, int32_t maskQ)
{
    int32_t tc2 = 2 * tc;
    int32_t tcP = (tc2 & maskP);
    int32_t tcQ = (tc2 & maskQ);
    for (int32_t i = 0; i < UNIT_SIZE; i++, src += srcStep)
    {
        int16_t m4  = (int16_t)src[0];
        int16_t m3  = (int16_t)src[-offset];
        int16_t m5  = (int16_t)src[offset];
        int16_t m2  = (int16_t)src[-offset * 2];
        int16_t m6  = (int16_t)src[offset * 2];
        int16_t m1  = (int16_t)src[-offset * 3];
        int16_t m7  = (int16_t)src[offset * 3];
        int16_t m0  = (int16_t)src[-offset * 4];
        src[-offset * 3] = (pixel)(x265_clip3(-tcP, tcP, ((2 * m0 + 3 * m1 + m2 + m3 + m4 + 4) >> 3) - m1) + m1);
        src[-offset * 2] = (pixel)(x265_clip3(-tcP, tcP, ((m1 + m2 + m3 + m4 + 2) >> 2) - m2) + m2);
        src[-offset]     = (pixel)(x265_clip3(-tcP, tcP, ((m1 + 2 * m2 + 2 * m3 + 2 * m4 + m5 + 4) >> 3) - m3) + m3);
        src[0]           = (pixel)(x265_clip3(-tcQ, tcQ, ((m2 + 2 * m3 + 2 * m4 + 2 * m5 + m6 + 4) >> 3) - m4) + m4);
        src[offset]      = (pixel)(x265_clip3(-tcQ, tcQ, ((m3 + m4 + m5 + m6 + 2) >> 2) - m5) + m5);
        src[offset * 2]  = (pixel)(x265_clip3(-tcQ, tcQ, ((m3 + m4 + m5 + 3 * m6 + 2 * m7 + 4) >> 3) - m6) + m6);
    }
}

/* Weak filter */
static inline void pelFilterLuma(pixel* src, intptr_t srcStep, intptr_t offset, int32_t tc, int32_t maskP, int32_t maskQ,
                                 int32_t maskP1, int32_t maskQ1)
{
    int32_t thrCut = tc * 10;
    int32_t tc2 = tc >> 1;
    maskP1 &= maskP;
    maskQ1 &= maskQ;

    for (int32_t i = 0; i < UNIT_SIZE; i++, src += srcStep)
    {
        int16_t m4  = (int16_t)src[0];
        int16_t m3  = (int16_t)src[-offset];
        int16_t m5  = (int16_t)src[offset];
        int16_t m2  = (int16_t)src[-offset * 2];

        int32_t delta = (9 * (m4 - m3) - 3 * (m5 - m2) + 8) >> 4;

        if (abs(delta) < thrCut)
        {
            delta = x265_clip3(-tc, tc, delta);

            src[-offset] = x265_clip(m3 + (delta & maskP));
            src[0] = x265_clip(m4 - (delta & maskQ));
            if (maskP1)
            {
                int16_t m1  = (int16_t)src[-offset * 3];
                int32_t delta1 = x265_clip3(-tc2, tc2, ((((m1 + m3 + 1) >> 1) - m2 + delta) >> 1));
                src[-offset * 2] = x265_clip(m2 + delta1);
            }
            if (maskQ1)
            {
                int16_t m6  = (int16_t)src[offset * 2];
                int32_t delta2 = x265_clip3(-tc2, tc2, ((((m6 + m4 + 1) >> 1) - m5 - delta) >> 1));
                src[offset] = x265_clip(m5 + delta2);
            }
        }
    }
}

/* Deblocking of one line/column for the chrominance component
 * \param src     pointer to picture data
 * \param offset  offset value for picture data
 * \param tc      tc value
 * \param maskP   indicator to disable filtering on partP
 * \param maskQ   indicator to disable filtering on partQ */
static inline void pelFilterChroma(pixel* src, intptr_t srcStep, intptr_t offset, int32_t tc, int32_t maskP, int32_t maskQ)
{
    for (int32_t i = 0; i < UNIT_SIZE; i++, src += srcStep)
    {
        int16_t m4  = (int16_t)src[0];
        int16_t m3  = (int16_t)src[-offset];
        int16_t m5  = (int16_t)src[offset];
        int16_t m2  = (int16_t)src[-offset * 2];

        int32_t delta = x265_clip3(-tc, tc, ((((m4 - m3) << 2) + m2 - m5 + 4) >> 3));
        src[-offset] = x265_clip(m3 + (delta & maskP));
        src[0] = x265_clip(m4 - (delta & maskQ));
    }
}

void Deblock::edgeFilterLuma(const CUData* cuQ, uint32_t absPartIdx, uint32_t depth, int32_t dir, int32_t edge, const uint8_t blockStrength[])
{
    PicYuv* reconPic = cuQ->m_encData->m_reconPic;
    pixel* src = reconPic->getLumaAddr(cuQ->m_cuAddr, absPartIdx);
    intptr_t stride = reconPic->m_stride;
    const PPS* pps = cuQ->m_slice->m_pps;

    intptr_t offset, srcStep;

    int32_t maskP = -1;
    int32_t maskQ = -1;
    int32_t betaOffset = pps->deblockingFilterBetaOffsetDiv2 << 1;
    int32_t tcOffset = pps->deblockingFilterTcOffsetDiv2 << 1;
    bool bCheckNoFilter = pps->bTransquantBypassEnabled;

    if (dir == EDGE_VER)
    {
        offset = 1;
        srcStep = stride;
        src += (edge << LOG2_UNIT_SIZE);
    }
    else // (dir == EDGE_HOR)
    {
        offset = stride;
        srcStep = 1;
        src += (edge << LOG2_UNIT_SIZE) * stride;
    }

    uint32_t numUnits = cuQ->m_slice->m_sps->numPartInCUSize >> depth;
    for (uint32_t idx = 0; idx < numUnits; idx++)
    {
        uint32_t partQ = calcBsIdx(cuQ, absPartIdx, dir, edge, idx);
        uint32_t bs = blockStrength[partQ];

        if (!bs)
            continue;

        int32_t qpQ = cuQ->m_qp[partQ];

        // Derive neighboring PU index
        uint32_t partP;
        const CUData* cuP = (dir == EDGE_VER ? cuQ->getPULeft(partP, partQ) : cuQ->getPUAbove(partP, partQ));

        int32_t qpP = cuP->m_qp[partP];
        int32_t qp = (qpP + qpQ + 1) >> 1;

        int32_t indexB = x265_clip3(0, QP_MAX_SPEC, qp + betaOffset);

        const int32_t bitdepthShift = X265_DEPTH - 8;
        int32_t beta = s_betaTable[indexB] << bitdepthShift;

        intptr_t unitOffset = idx * srcStep << LOG2_UNIT_SIZE;
        int32_t dp0 = calcDP(src + unitOffset              , offset);
        int32_t dq0 = calcDQ(src + unitOffset              , offset);
        int32_t dp3 = calcDP(src + unitOffset + srcStep * 3, offset);
        int32_t dq3 = calcDQ(src + unitOffset + srcStep * 3, offset);
        int32_t d0 = dp0 + dq0;
        int32_t d3 = dp3 + dq3;

        int32_t d =  d0 + d3;

        if (d >= beta)
            continue;

        if (bCheckNoFilter)
        {
            // check if each of PUs is lossless coded
            maskP = (cuP->m_tqBypass[partP] ? 0 : -1);
            maskQ = (cuQ->m_tqBypass[partQ] ? 0 : -1);
        }

        int32_t indexTC = x265_clip3(0, QP_MAX_SPEC + DEFAULT_INTRA_TC_OFFSET, int32_t(qp + DEFAULT_INTRA_TC_OFFSET * (bs - 1) + tcOffset));
        int32_t tc = s_tcTable[indexTC] << bitdepthShift;

        bool sw = (2 * d0 < (beta >> 2) &&
                   2 * d3 < (beta >> 2) &&
                   useStrongFiltering(offset, beta, tc, src + unitOffset              ) &&
                   useStrongFiltering(offset, beta, tc, src + unitOffset + srcStep * 3));

        if (sw)
            pelFilterLumaStrong(src + unitOffset, srcStep, offset, tc, maskP, maskQ);
        else
        {
            int32_t sideThreshold = (beta + (beta >> 1)) >> 3;
            int32_t dp = dp0 + dp3;
            int32_t dq = dq0 + dq3;
            int32_t maskP1 = (dp < sideThreshold ? -1 : 0);
            int32_t maskQ1 = (dq < sideThreshold ? -1 : 0);

            pelFilterLuma(src + unitOffset, srcStep, offset, tc, maskP, maskQ, maskP1, maskQ1);
        }
    }
}

void Deblock::edgeFilterChroma(const CUData* cuQ, uint32_t absPartIdx, uint32_t depth, int32_t dir, int32_t edge, const uint8_t blockStrength[])
{
    int32_t chFmt = cuQ->m_chromaFormat, chromaShift;
    intptr_t offset, srcStep;
    const PPS* pps = cuQ->m_slice->m_pps;

    int32_t maskP = -1;
    int32_t maskQ = -1;
    int32_t tcOffset = pps->deblockingFilterTcOffsetDiv2 << 1;

    X265_CHECK(((dir == EDGE_VER)
                ? ((g_zscanToPelX[absPartIdx] + edge * UNIT_SIZE) >> cuQ->m_hChromaShift)
                : ((g_zscanToPelY[absPartIdx] + edge * UNIT_SIZE) >> cuQ->m_vChromaShift)) % DEBLOCK_SMALLEST_BLOCK == 0,
               "invalid edge\n");

    PicYuv* reconPic = cuQ->m_encData->m_reconPic;
    intptr_t stride = reconPic->m_strideC;
    intptr_t srcOffset = reconPic->getChromaAddrOffset(cuQ->m_cuAddr, absPartIdx);
    bool bCheckNoFilter = pps->bTransquantBypassEnabled;

    if (dir == EDGE_VER)
    {
        chromaShift = cuQ->m_vChromaShift;
        srcOffset += (edge << (LOG2_UNIT_SIZE - cuQ->m_hChromaShift));
        offset     = 1;
        srcStep    = stride;
    }
    else // (dir == EDGE_HOR)
    {
        chromaShift = cuQ->m_hChromaShift;
        srcOffset += edge * stride << (LOG2_UNIT_SIZE - cuQ->m_vChromaShift);
        offset     = stride;
        srcStep    = 1;
    }

    pixel* srcChroma[2];
    srcChroma[0] = reconPic->m_picOrg[1] + srcOffset;
    srcChroma[1] = reconPic->m_picOrg[2] + srcOffset;

    uint32_t numUnits = cuQ->m_slice->m_sps->numPartInCUSize >> (depth + chromaShift);

    for (uint32_t idx = 0; idx < numUnits; idx++)
    {
        uint32_t partQ = calcBsIdx(cuQ, absPartIdx, dir, edge, idx << chromaShift);
        uint32_t bs = blockStrength[partQ];

        if (bs <= 1)
            continue;

        int32_t qpQ = cuQ->m_qp[partQ];

        // Derive neighboring PU index
        uint32_t partP;
        const CUData* cuP = (dir == EDGE_VER ? cuQ->getPULeft(partP, partQ) : cuQ->getPUAbove(partP, partQ));

        int32_t qpP = cuP->m_qp[partP];

        if (bCheckNoFilter)
        {
            // check if each of PUs is lossless coded
            maskP = (cuP->m_tqBypass[partP] ? 0 : -1);
            maskQ = (cuQ->m_tqBypass[partQ] ? 0 : -1);
        }

        intptr_t unitOffset = idx * srcStep << LOG2_UNIT_SIZE;
        for (uint32_t chromaIdx = 0; chromaIdx < 2; chromaIdx++)
        {
            int32_t chromaQPOffset  = pps->chromaQpOffset[chromaIdx];
            int32_t qp = ((qpP + qpQ + 1) >> 1) + chromaQPOffset;
            if (qp >= 30)
            {
                if (chFmt == X265_CSP_I420)
                    qp = g_chromaScale[qp];
                else
                    qp = X265_MIN(qp, 51);
            }

            int32_t indexTC = x265_clip3(0, QP_MAX_SPEC + DEFAULT_INTRA_TC_OFFSET, int32_t(qp + DEFAULT_INTRA_TC_OFFSET + tcOffset));
            const int32_t bitdepthShift = X265_DEPTH - 8;
            int32_t tc = s_tcTable[indexTC] << bitdepthShift;
            pixel* srcC = srcChroma[chromaIdx];

            pelFilterChroma(srcC + unitOffset, srcStep, offset, tc, maskP, maskQ);
        }
    }
}

const uint8_t Deblock::s_tcTable[54] =
{
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2,
    2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 5, 5, 6, 6, 7, 8, 9, 10, 11, 13, 14, 16, 18, 20, 22, 24
};

const uint8_t Deblock::s_betaTable[52] =
{
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17,
    18, 20, 22, 24, 26, 28, 30, 32, 34, 36, 38, 40, 42, 44, 46, 48, 50, 52, 54, 56, 58, 60, 62, 64
};

