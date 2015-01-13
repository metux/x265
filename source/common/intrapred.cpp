/*****************************************************************************
 * Copyright (C) 2013 x265 project
 *
 * Authors: Min Chen <chenm003@163.com>
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

using namespace x265;

namespace {
void dcPredFilter(pixel* above, pixel* left, pixel* dst, intptr_t dststride, int size)
{
    // boundary pixels processing
    dst[0] = (pixel)((above[0] + left[0] + 2 * dst[0] + 2) >> 2);

    for (int x = 1; x < size; x++)
        dst[x] = (pixel)((above[x] +  3 * dst[x] + 2) >> 2);

    dst += dststride;
    for (int y = 1; y < size; y++)
    {
        *dst = (pixel)((left[y] + 3 * *dst + 2) >> 2);
        dst += dststride;
    }
}

template<int width>
void intra_pred_dc_c_new(pixel* dst, intptr_t dstStride, pixel* srcPix, int /*dirMode*/, int bFilter)
{
    int k, l;

    int dcVal = width;
    for (int i = 0; i < width; i++)
        dcVal += srcPix[1 + i] + srcPix[2 * width + 1 + i];

    dcVal = dcVal / (width + width);
    for (k = 0; k < width; k++)
        for (l = 0; l < width; l++)
            dst[k * dstStride + l] = (pixel)dcVal;

    if (bFilter)
        dcPredFilter(srcPix + 1, srcPix + (2 * width + 1), dst, dstStride, width);

}

template<int log2Size>
void planar_pred_c_new(pixel* dst, intptr_t dstStride, pixel* srcPix, int /*dirMode*/, int /*bFilter*/)
{
    const int blkSize = 1 << log2Size;

    pixel* above = srcPix + 1;
    pixel* left  = srcPix + (2 * blkSize + 1);

    pixel topRight = above[blkSize];
    pixel bottomLeft = left[blkSize];
    for (int y = 0; y < blkSize; y++)
        for (int x = 0; x < blkSize; x++)
            dst[y * dstStride + x] = (pixel) (((blkSize - 1 - x) * left[y] + (blkSize - 1 -y) * above[x] + (x + 1) * topRight + (y + 1) * bottomLeft + blkSize) >> (log2Size + 1));
}
template<int width>
void intra_pred_ang_c(pixel* dst, intptr_t dstStride, pixel *refLeft, pixel *refAbove, int dirMode, int bFilter)
{
    // Map the mode index to main prediction direction and angle
    int k, l;
    bool modeHor       = (dirMode < 18);
    bool modeVer       = !modeHor;
    int intraPredAngle = modeVer ? (int)dirMode - VER_IDX : modeHor ? -((int)dirMode - HOR_IDX) : 0;
    int absAng         = abs(intraPredAngle);
    int signAng        = intraPredAngle < 0 ? -1 : 1;

    // Set bitshifts and scale the angle parameter to block size
    static const int angTable[9]    = { 0,    2,    5,   9,  13,  17,  21,  26,  32 };
    static const int invAngTable[9] = { 0, 4096, 1638, 910, 630, 482, 390, 315, 256 }; // (256 * 32) / Angle
    int invAngle       = invAngTable[absAng];

    absAng             = angTable[absAng];
    intraPredAngle     = signAng * absAng;

    // Do angular predictions
    {
        pixel* refMain;
        pixel* refSide;

        // Initialise the Main and Left reference array.
        if (intraPredAngle < 0)
        {
            refMain = (modeVer ? refAbove : refLeft); // + (width - 1);
            refSide = (modeVer ? refLeft : refAbove); // + (width - 1);

            // Extend the Main reference to the left.
            int invAngleSum    = 128; // rounding for (shift by 8)
            for (k = -1; k > width * intraPredAngle >> 5; k--)
            {
                invAngleSum += invAngle;
                refMain[k] = refSide[invAngleSum >> 8];
            }
        }
        else
        {
            refMain = modeVer ? refAbove : refLeft;
            refSide = modeVer ? refLeft  : refAbove;
        }

        if (intraPredAngle == 0)
        {
            for (k = 0; k < width; k++)
            {
                for (l = 0; l < width; l++)
                    dst[k * dstStride + l] = refMain[l + 1];
            }

            if (bFilter)
            {
                for (k = 0; k < width; k++)
                    dst[k * dstStride] = x265_clip((int16_t)((dst[k * dstStride]) + ((refSide[k + 1] - refSide[0]) >> 1)));
            }
        }
        else
        {
            int deltaPos = 0;
            int deltaInt;
            int deltaFract;
            int refMainIndex;

            for (k = 0; k < width; k++)
            {
                deltaPos += intraPredAngle;
                deltaInt   = deltaPos >> 5;
                deltaFract = deltaPos & (32 - 1);

                if (deltaFract)
                {
                    // Do linear filtering
                    for (l = 0; l < width; l++)
                    {
                        refMainIndex = l + deltaInt + 1;
                        dst[k * dstStride + l] = (pixel)(((32 - deltaFract) * refMain[refMainIndex] + deltaFract * refMain[refMainIndex + 1] + 16) >> 5);
                    }
                }
                else
                {
                    // Just copy the integer samples
                    for (l = 0; l < width; l++)
                        dst[k * dstStride + l] = refMain[l + deltaInt + 1];
                }
            }
        }

        // Flip the block if this is the horizontal mode
        if (modeHor)
        {
            for (k = 0; k < width - 1; k++)
            {
                for (l = k + 1; l < width; l++)
                {
                    pixel tmp              = dst[k * dstStride + l];
                    dst[k * dstStride + l] = dst[l * dstStride + k];
                    dst[l * dstStride + k] = tmp;
                }
            }
        }
    }
}

template<int width>
void intra_pred_ang_c_new(pixel* dst, intptr_t dstStride, pixel *srcPix, int dirMode, int bFilter)
{
    int width2 = width << 1;
    // Flip the neighbours in the horizontal case.
    int horMode = dirMode < 18;
    pixel neighbourBuf[129];

    if (horMode)
    {
        neighbourBuf[0] = srcPix[0];
        for (int i = 0; i < width << 1; i++)
        {
            neighbourBuf[1 + i] = srcPix[width2 + 1 + i];
            neighbourBuf[width2 + 1 + i] = srcPix[1 + i];
        }
        srcPix = neighbourBuf;
    }

    // Intra prediction angle and inverse angle tables.
    const int8_t angleTable[17] = { -32, -26, -21, -17, -13, -9, -5, -2, 0, 2, 5, 9, 13, 17, 21, 26, 32 };
    const int16_t invAngleTable[8] = { 4096, 1638, 910, 630, 482, 390, 315, 256 };

    // Get the prediction angle.
    int angleOffset = horMode ? 10 - dirMode : dirMode - 26;
    int angle = angleTable[8 + angleOffset];

    // Vertical Prediction.
    if (!angle)
    {
        for (int y = 0; y < width; y++)
            for (int x = 0; x < width; x++)
                dst[y * dstStride + x] = srcPix[1 + x];

        if (bFilter)
        {
            int topLeft = srcPix[0], top = srcPix[1];
            for (int y = 0; y < width; y++)
                dst[y * dstStride] = x265_clip((int16_t)(top + ((srcPix[width2 + 1 + y] - topLeft) >> 1)));
        }
    }
    else // Angular prediction.
    {
        // Get the reference pixels. The reference base is the first pixel to the top (neighbourBuf[1]).
        pixel refBuf[64], *ref;

        // Use the projected left neighbours and the top neighbours.
        if (angle < 0)
        {
            // Number of neighbours projected. 
            int nbProjected = -((width * angle) >> 5) - 1;
            ref = refBuf + nbProjected + 1;

            // Project the neighbours.
            int invAngle = invAngleTable[- angleOffset - 1];
            int invAngleSum = 128;
            for (int i = 0; i < nbProjected; i++)
            {
                invAngleSum += invAngle;
                ref[- 2 - i] = srcPix[width2 + (invAngleSum >> 8)];
            }

            // Copy the top-left and top pixels.
            for (int i = 0; i < width + 1; i++)
                ref[-1 + i] = srcPix[i];
        }
        else // Use the top and top-right neighbours.
            ref = srcPix + 1;

        // Pass every row.
        int angleSum = 0;
        for (int y = 0; y < width; y++)
        {
            angleSum += angle;
            int offset = angleSum >> 5;
            int fraction = angleSum & 31;

            if (fraction) // Interpolate
                for (int x = 0; x < width; x++)
                    dst[y * dstStride + x] = (pixel)(((32 - fraction) * ref[offset + x] + fraction * ref[offset + x + 1] + 16) >> 5);
            else // Copy.
                for (int x = 0; x < width; x++)
                    dst[y * dstStride + x] = ref[offset + x];
        }
    }

    // Flip for horizontal.
    if (horMode)
    {
        for (int y = 0; y < width - 1; y++)
        {
            for (int x = y + 1; x < width; x++)
            {
                pixel tmp              = dst[y * dstStride + x];
                dst[y * dstStride + x] = dst[x * dstStride + y];
                dst[x * dstStride + y] = tmp;
            }
        }
    }
}

template<int log2Size>
void all_angs_pred_c_new(pixel *dest, pixel *refPix, pixel *filtPix, int bLuma)
{
    const int size = 1 << log2Size;
    for (int mode = 2; mode <= 34; mode++)
    {
        pixel *srcPix  = (g_intraFilterFlags[mode] & size ? filtPix  : refPix);
        pixel *out = dest + ((mode - 2) << (log2Size * 2));

        intra_pred_ang_c_new<size>(out, size, srcPix, mode, bLuma);

        // Optimize code don't flip buffer
        bool modeHor = (mode < 18);

        // transpose the block if this is a horizontal mode
        if (modeHor)
        {
            for (int k = 0; k < size - 1; k++)
            {
                for (int l = k + 1; l < size; l++)
                {
                    pixel tmp         = out[k * size + l];
                    out[k * size + l] = out[l * size + k];
                    out[l * size + k] = tmp;
                }
            }
        }
    }
}
}

namespace x265 {
// x265 private namespace

void Setup_C_IPredPrimitives(EncoderPrimitives& p)
{
    p.intra_pred_new[0][BLOCK_4x4] = planar_pred_c_new<2>;
    p.intra_pred_new[0][BLOCK_8x8] = planar_pred_c_new<3>;
    p.intra_pred_new[0][BLOCK_16x16] = planar_pred_c_new<4>;
    p.intra_pred_new[0][BLOCK_32x32] = planar_pred_c_new<5>;

    p.intra_pred_new[1][BLOCK_4x4] = intra_pred_dc_c_new<4>;
    p.intra_pred_new[1][BLOCK_8x8] = intra_pred_dc_c_new<8>;
    p.intra_pred_new[1][BLOCK_16x16] = intra_pred_dc_c_new<16>;
    p.intra_pred_new[1][BLOCK_32x32] = intra_pred_dc_c_new<32>;

    for (int i = 2; i < NUM_INTRA_MODE; i++)
    {
        p.intra_pred[i][BLOCK_4x4] = intra_pred_ang_c<4>;
        p.intra_pred[i][BLOCK_8x8] = intra_pred_ang_c<8>;
        p.intra_pred[i][BLOCK_16x16] = intra_pred_ang_c<16>;
        p.intra_pred[i][BLOCK_32x32] = intra_pred_ang_c<32>;

        p.intra_pred_new[i][BLOCK_4x4] = intra_pred_ang_c_new<4>;
        p.intra_pred_new[i][BLOCK_8x8] = intra_pred_ang_c_new<8>;
        p.intra_pred_new[i][BLOCK_16x16] = intra_pred_ang_c_new<16>;
        p.intra_pred_new[i][BLOCK_32x32] = intra_pred_ang_c_new<32>;
    }

    p.intra_pred_allangs_new[BLOCK_4x4] = all_angs_pred_c_new<2>;
    p.intra_pred_allangs_new[BLOCK_8x8] = all_angs_pred_c_new<3>;
    p.intra_pred_allangs_new[BLOCK_16x16] = all_angs_pred_c_new<4>;
    p.intra_pred_allangs_new[BLOCK_32x32] = all_angs_pred_c_new<5>;
}
}
