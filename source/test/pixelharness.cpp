/*****************************************************************************
 * Copyright (C) 2013 x265 project
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

#include "pixelharness.h"
#include "primitives.h"

using namespace x265;

PixelHarness::PixelHarness()
{
    /* [0] --- Random values
     * [1] --- Minimum
     * [2] --- Maximum */
    for (int i = 0; i < BUFFSIZE; i++)
    {
        pixel_test_buff[0][i]   = rand() % PIXEL_MAX;
        short_test_buff[0][i]   = (rand() % (2 * SMAX + 1)) - SMAX - 1; // max(SHORT_MIN, min(rand(), SMAX));
        short_test_buff1[0][i]  = rand() & PIXEL_MAX;                   // For block copy only
        short_test_buff2[0][i]  = rand() % 16383;                       // for addAvg
        int_test_buff[0][i]     = rand() % SHORT_MAX;
        ushort_test_buff[0][i]  = rand() % ((1 << 16) - 1);
        uchar_test_buff[0][i]   = rand() % ((1 << 8) - 1);

        pixel_test_buff[1][i]   = PIXEL_MIN;
        short_test_buff[1][i]   = SMIN;
        short_test_buff1[1][i]  = PIXEL_MIN;
        short_test_buff2[1][i]  = -16384;
        int_test_buff[1][i]     = SHORT_MIN;
        ushort_test_buff[1][i]  = PIXEL_MIN;
        uchar_test_buff[1][i]   = PIXEL_MIN;

        pixel_test_buff[2][i]   = PIXEL_MAX;
        short_test_buff[2][i]   = SMAX;
        short_test_buff1[2][i]  = PIXEL_MAX;
        short_test_buff2[2][i]  = 16383;
        int_test_buff[2][i]     = SHORT_MAX;
        ushort_test_buff[2][i]  = ((1 << 16) - 1);
        uchar_test_buff[2][i]   = 255;

        pbuf1[i] = rand() & PIXEL_MAX;
        pbuf2[i] = rand() & PIXEL_MAX;
        pbuf3[i] = rand() & PIXEL_MAX;
        pbuf4[i] = rand() & PIXEL_MAX;

        sbuf1[i] = (rand() % (2 * SMAX + 1)) - SMAX - 1; //max(SHORT_MIN, min(rand(), SMAX));
        sbuf2[i] = (rand() % (2 * SMAX + 1)) - SMAX - 1; //max(SHORT_MIN, min(rand(), SMAX));
        ibuf1[i] = (rand() % (2 * SMAX + 1)) - SMAX - 1;
        psbuf1[i] = psbuf4[i] = (rand() % 65) - 32;                   // range is between -32 to 32
        psbuf2[i] = psbuf5[i] = (rand() % 3) - 1;                     // possible values {-1,0,1}
        psbuf3[i] = (rand() % 129) - 128;
        sbuf3[i] = rand() % PIXEL_MAX; // for blockcopy only
    }
}

bool PixelHarness::check_pixelcmp(pixelcmp_t ref, pixelcmp_t opt)
{
    int j = 0;
    intptr_t stride = STRIDE;

    for (int i = 0; i < ITERS; i++)
    {
        int index1 = rand() % TEST_CASES;
        int index2 = rand() % TEST_CASES;
        int vres = (int)checked(opt, pixel_test_buff[index1], stride, pixel_test_buff[index2] + j, stride);
        int cres = ref(pixel_test_buff[index1], stride, pixel_test_buff[index2] + j, stride);
        if (vres != cres)
            return false;

        reportfail();
        j += INCR;
    }

    return true;
}

bool PixelHarness::check_pixelcmp_sp(pixelcmp_sp_t ref, pixelcmp_sp_t opt)
{
    int j = 0;
    intptr_t stride = STRIDE;

    for (int i = 0; i < ITERS; i++)
    {
        int index1 = rand() % TEST_CASES;
        int index2 = rand() % TEST_CASES;
        int vres = (int)checked(opt, short_test_buff[index1], stride, pixel_test_buff[index2] + j, stride);
        int cres = ref(short_test_buff[index1], stride, pixel_test_buff[index2] + j, stride);
        if (vres != cres)
            return false;

        reportfail();
        j += INCR;
    }

    return true;
}

bool PixelHarness::check_pixelcmp_ss(pixelcmp_ss_t ref, pixelcmp_ss_t opt)
{
    int j = 0;
    intptr_t stride = STRIDE;

    for (int i = 0; i < ITERS; i++)
    {
        int index1 = rand() % TEST_CASES;
        int index2 = rand() % TEST_CASES;
        int vres = (int)checked(opt, short_test_buff[index1], stride, short_test_buff[index2] + j, stride);
        int cres = ref(short_test_buff[index1], stride, short_test_buff[index2] + j, stride);
        if (vres != cres)
            return false;

        reportfail();
        j += INCR;
    }

    return true;
}

bool PixelHarness::check_pixelcmp_x3(pixelcmp_x3_t ref, pixelcmp_x3_t opt)
{
    ALIGN_VAR_16(int, cres[16]);
    ALIGN_VAR_16(int, vres[16]);
    int j = 0;
    intptr_t stride = FENC_STRIDE - 5;
    for (int i = 0; i < ITERS; i++)
    {
        int index1 = rand() % TEST_CASES;
        int index2 = rand() % TEST_CASES;
        checked(opt, pixel_test_buff[index1],
                pixel_test_buff[index2] + j,
                pixel_test_buff[index2] + j + 1,
                pixel_test_buff[index2] + j + 2, stride, &vres[0]);
        ref(pixel_test_buff[index1],
            pixel_test_buff[index2] + j,
            pixel_test_buff[index2] + j + 1,
            pixel_test_buff[index2] + j + 2, stride, &cres[0]);
        if ((vres[0] != cres[0]) || ((vres[1] != cres[1])) || ((vres[2] != cres[2])))
            return false;

        reportfail();
        j += INCR;
    }

    return true;
}

bool PixelHarness::check_pixelcmp_x4(pixelcmp_x4_t ref, pixelcmp_x4_t opt)
{
    ALIGN_VAR_16(int, cres[16]);
    ALIGN_VAR_16(int, vres[16]);
    int j = 0;
    intptr_t stride = FENC_STRIDE - 5;
    for (int i = 0; i < ITERS; i++)
    {
        int index1 = rand() % TEST_CASES;
        int index2 = rand() % TEST_CASES;
        checked(opt, pixel_test_buff[index1],
                pixel_test_buff[index2] + j,
                pixel_test_buff[index2] + j + 1,
                pixel_test_buff[index2] + j + 2,
                pixel_test_buff[index2] + j + 3, stride, &vres[0]);
        ref(pixel_test_buff[index1],
            pixel_test_buff[index2] + j,
            pixel_test_buff[index2] + j + 1,
            pixel_test_buff[index2] + j + 2,
            pixel_test_buff[index2] + j + 3, stride, &cres[0]);

        if ((vres[0] != cres[0]) || ((vres[1] != cres[1])) || ((vres[2] != cres[2])) || ((vres[3] != cres[3])))
            return false;

        reportfail();
        j += INCR;
    }

    return true;
}

bool PixelHarness::check_calresidual(calcresidual_t ref, calcresidual_t opt)
{
    ALIGN_VAR_16(int16_t, ref_dest[64 * 64]);
    ALIGN_VAR_16(int16_t, opt_dest[64 * 64]);
    memset(ref_dest, 0, 64 * 64 * sizeof(int16_t));
    memset(opt_dest, 0, 64 * 64 * sizeof(int16_t));

    int j = 0;
    intptr_t stride = STRIDE;
    for (int i = 0; i < ITERS; i++)
    {
        int index = i % TEST_CASES;
        checked(opt, pbuf1 + j, pixel_test_buff[index] + j, opt_dest, stride);
        ref(pbuf1 + j, pixel_test_buff[index] + j, ref_dest, stride);

        if (memcmp(ref_dest, opt_dest, 64 * 64 * sizeof(int16_t)))
            return false;

        reportfail();
        j += INCR;
    }

    return true;
}

bool PixelHarness::check_ssd_s(pixel_ssd_s_t ref, pixel_ssd_s_t opt)
{
    int j = 0;
    for (int i = 0; i < ITERS; i++)
    {
        // NOTE: stride must be multiple of 16, because minimum block is 4x4
        int stride = (STRIDE + (rand() % STRIDE)) & ~15;
        int cres = ref(sbuf1 + j, stride);
        int vres = (int)checked(opt, sbuf1 + j, (intptr_t)stride);

        if (cres != vres)
        {
            return false;
        }

        reportfail();
        j += INCR;
    }

    return true;
}

bool PixelHarness::check_weightp(weightp_sp_t ref, weightp_sp_t opt)
{
    ALIGN_VAR_16(pixel, ref_dest[64 * 64]);
    ALIGN_VAR_16(pixel, opt_dest[64 * 64]);

    memset(ref_dest, 0, 64 * 64 * sizeof(pixel));
    memset(opt_dest, 0, 64 * 64 * sizeof(pixel));
    int j = 0;
    int width = 2 * (rand() % 32 + 1);
    int height = 8;
    int w0 = rand() % 128;
    int shift = rand() % 8; // maximum is 7, see setFromWeightAndOffset()
    int round = shift ? (1 << (shift - 1)) : 0;
    int offset = (rand() % 256) - 128;
    intptr_t stride = 64;
    const int correction = (IF_INTERNAL_PREC - X265_DEPTH);
    for (int i = 0; i < ITERS; i++)
    {
        int index = i % TEST_CASES;
        checked(opt, short_test_buff[index] + j, opt_dest, stride, stride, width, height, w0, round << correction, shift + correction, offset);
        ref(short_test_buff[index] + j, ref_dest, stride, stride, width, height, w0, round << correction, shift + correction, offset);

        if (memcmp(ref_dest, opt_dest, 64 * 64 * sizeof(pixel)))
            return false;

        reportfail();
        j += INCR;
    }

    return true;
}

bool PixelHarness::check_weightp(weightp_pp_t ref, weightp_pp_t opt)
{
    ALIGN_VAR_16(pixel, ref_dest[64 * 64]);
    ALIGN_VAR_16(pixel, opt_dest[64 * 64]);

    memset(ref_dest, 0, 64 * 64 * sizeof(pixel));
    memset(opt_dest, 0, 64 * 64 * sizeof(pixel));
    int j = 0;
    int width = 16 * (rand() % 4 + 1);
    int height = 8;
    int w0 = rand() % 128;
    int shift = rand() % 8; // maximum is 7, see setFromWeightAndOffset()
    int round = shift ? (1 << (shift - 1)) : 0;
    int offset = (rand() % 256) - 128;
    intptr_t stride = 64;
    const int correction = (IF_INTERNAL_PREC - X265_DEPTH);
    for (int i = 0; i < ITERS; i++)
    {
        int index = i % TEST_CASES;
        checked(opt, pixel_test_buff[index] + j, opt_dest, stride, width, height, w0, round << correction, shift + correction, offset);
        ref(pixel_test_buff[index] + j, ref_dest, stride, width, height, w0, round << correction, shift + correction, offset);

        if (memcmp(ref_dest, opt_dest, 64 * 64 * sizeof(pixel)))
            return false;

        reportfail();
        j += INCR;
    }

    return true;
}

bool PixelHarness::check_downscale_t(downscale_t ref, downscale_t opt)
{
    ALIGN_VAR_16(pixel, ref_destf[32 * 32]);
    ALIGN_VAR_16(pixel, opt_destf[32 * 32]);

    ALIGN_VAR_16(pixel, ref_desth[32 * 32]);
    ALIGN_VAR_16(pixel, opt_desth[32 * 32]);

    ALIGN_VAR_16(pixel, ref_destv[32 * 32]);
    ALIGN_VAR_16(pixel, opt_destv[32 * 32]);

    ALIGN_VAR_16(pixel, ref_destc[32 * 32]);
    ALIGN_VAR_16(pixel, opt_destc[32 * 32]);

    intptr_t src_stride = 64;
    intptr_t dst_stride = 32;
    int bx = 32;
    int by = 32;
    int j = 0;
    for (int i = 0; i < ITERS; i++)
    {
        int index = i % TEST_CASES;
        ref(pixel_test_buff[index] + j, ref_destf, ref_desth, ref_destv,
            ref_destc, src_stride, dst_stride, bx, by);
        checked(opt, pixel_test_buff[index] + j, opt_destf, opt_desth, opt_destv,
                opt_destc, src_stride, dst_stride, bx, by);

        if (memcmp(ref_destf, opt_destf, 32 * 32 * sizeof(pixel)))
            return false;
        if (memcmp(ref_desth, opt_desth, 32 * 32 * sizeof(pixel)))
            return false;
        if (memcmp(ref_destv, opt_destv, 32 * 32 * sizeof(pixel)))
            return false;
        if (memcmp(ref_destc, opt_destc, 32 * 32 * sizeof(pixel)))
            return false;

        reportfail();
        j += INCR;
    }

    return true;
}

bool PixelHarness::check_cpy2Dto1D_shl_t(cpy2Dto1D_shl_t ref, cpy2Dto1D_shl_t opt)
{
    ALIGN_VAR_16(int16_t, ref_dest[64 * 64]);
    ALIGN_VAR_16(int16_t, opt_dest[64 * 64]);

    memset(ref_dest, 0xCD, sizeof(ref_dest));
    memset(opt_dest, 0xCD, sizeof(opt_dest));

    int j = 0;
    intptr_t stride = STRIDE;
    for (int i = 0; i < ITERS; i++)
    {
        int shift = (rand() % 7 + 1);

        int index = i % TEST_CASES;
        checked(opt, opt_dest, short_test_buff[index] + j, stride, shift);
        ref(ref_dest, short_test_buff[index] + j, stride, shift);

        if (memcmp(ref_dest, opt_dest, 64 * 64 * sizeof(int16_t)))
            return false;

        reportfail();
        j += INCR;
    }

    return true;
}

bool PixelHarness::check_cpy2Dto1D_shr_t(cpy2Dto1D_shr_t ref, cpy2Dto1D_shr_t opt)
{
    ALIGN_VAR_16(int16_t, ref_dest[64 * 64]);
    ALIGN_VAR_16(int16_t, opt_dest[64 * 64]);

    memset(ref_dest, 0xCD, sizeof(ref_dest));
    memset(opt_dest, 0xCD, sizeof(opt_dest));

    int j = 0;
    intptr_t stride = STRIDE;
    for (int i = 0; i < ITERS; i++)
    {
        int shift = (rand() % 7 + 1);

        int index = i % TEST_CASES;
        checked(opt, opt_dest, short_test_buff[index] + j, stride, shift);
        ref(ref_dest, short_test_buff[index] + j, stride, shift);

        if (memcmp(ref_dest, opt_dest, 64 * 64 * sizeof(int16_t)))
            return false;

        reportfail();
        j += INCR;
    }

    return true;
}

bool PixelHarness::check_copy_cnt_t(copy_cnt_t ref, copy_cnt_t opt)
{
    ALIGN_VAR_16(int16_t, ref_dest[64 * 64]);
    ALIGN_VAR_16(int16_t, opt_dest[64 * 64]);

    memset(ref_dest, 0xCD, sizeof(ref_dest));
    memset(opt_dest, 0xCD, sizeof(opt_dest));

    int j = 0;
    intptr_t stride = STRIDE;
    for (int i = 0; i < ITERS; i++)
    {
        int index = i % TEST_CASES;
        int opt_cnt = (int)checked(opt, opt_dest, short_test_buff1[index] + j, stride);
        int ref_cnt = ref(ref_dest, short_test_buff1[index] + j, stride);

        if ((ref_cnt != opt_cnt) || memcmp(ref_dest, opt_dest, 64 * 64 * sizeof(int16_t)))
            return false;

        reportfail();
        j += INCR;
    }

    return true;
}

bool PixelHarness::check_cpy1Dto2D_shl_t(cpy1Dto2D_shl_t ref, cpy1Dto2D_shl_t opt)
{
    ALIGN_VAR_16(int16_t, ref_dest[64 * 64]);
    ALIGN_VAR_16(int16_t, opt_dest[64 * 64]);

    memset(ref_dest, 0xCD, sizeof(ref_dest));
    memset(opt_dest, 0xCD, sizeof(opt_dest));

    int j = 0;
    intptr_t stride = STRIDE;
    for (int i = 0; i < ITERS; i++)
    {
        int shift = (rand() % 7 + 1);

        int index = i % TEST_CASES;
        checked(opt, opt_dest, short_test_buff[index] + j, stride, shift);
        ref(ref_dest, short_test_buff[index] + j, stride, shift);

        if (memcmp(ref_dest, opt_dest, 64 * 64 * sizeof(int16_t)))
            return false;

        reportfail();
        j += INCR;
    }

    return true;
}

bool PixelHarness::check_cpy1Dto2D_shr_t(cpy1Dto2D_shr_t ref, cpy1Dto2D_shr_t opt)
{
    ALIGN_VAR_16(int16_t, ref_dest[64 * 64]);
    ALIGN_VAR_16(int16_t, opt_dest[64 * 64]);

    memset(ref_dest, 0xCD, sizeof(ref_dest));
    memset(opt_dest, 0xCD, sizeof(opt_dest));

    int j = 0;
    intptr_t stride = STRIDE;
    for (int i = 0; i < ITERS; i++)
    {
        int shift = (rand() % 7 + 1);

        int index = i % TEST_CASES;
        checked(opt, opt_dest, short_test_buff[index] + j, stride, shift);
        ref(ref_dest, short_test_buff[index] + j, stride, shift);

        if (memcmp(ref_dest, opt_dest, 64 * 64 * sizeof(int16_t)))
            return false;

        reportfail();
        j += INCR;
    }

    return true;
}

bool PixelHarness::check_pixelavg_pp(pixelavg_pp_t ref, pixelavg_pp_t opt)
{
    ALIGN_VAR_16(pixel, ref_dest[64 * 64]);
    ALIGN_VAR_16(pixel, opt_dest[64 * 64]);

    int j = 0;

    memset(ref_dest, 0xCD, sizeof(ref_dest));
    memset(opt_dest, 0xCD, sizeof(opt_dest));

    intptr_t stride = STRIDE;
    for (int i = 0; i < ITERS; i++)
    {
        int index1 = rand() % TEST_CASES;
        int index2 = rand() % TEST_CASES;
        checked(ref, ref_dest, stride, pixel_test_buff[index1] + j,
                stride, pixel_test_buff[index2] + j, stride, 32);
        opt(opt_dest, stride, pixel_test_buff[index1] + j,
            stride, pixel_test_buff[index2] + j, stride, 32);

        if (memcmp(ref_dest, opt_dest, 64 * 64 * sizeof(pixel)))
            return false;

        reportfail();
        j += INCR;
    }

    return true;
}

bool PixelHarness::check_copy_pp(copy_pp_t ref, copy_pp_t opt)
{
    ALIGN_VAR_16(pixel, ref_dest[64 * 64]);
    ALIGN_VAR_16(pixel, opt_dest[64 * 64]);

    // we don't know the partition size so we are checking the entire output buffer so
    // we must initialize the buffers
    memset(ref_dest, 0, sizeof(ref_dest));
    memset(opt_dest, 0, sizeof(opt_dest));

    int j = 0;
    intptr_t stride = STRIDE;
    for (int i = 0; i < ITERS; i++)
    {
        int index = i % TEST_CASES;
        checked(opt, opt_dest, stride, pixel_test_buff[index] + j, stride);
        ref(ref_dest, stride, pixel_test_buff[index] + j, stride);

        if (memcmp(ref_dest, opt_dest, 64 * 64 * sizeof(pixel)))
            return false;

        reportfail();
        j += INCR;
    }

    return true;
}

bool PixelHarness::check_copy_sp(copy_sp_t ref, copy_sp_t opt)
{
    ALIGN_VAR_16(pixel, ref_dest[64 * 64]);
    ALIGN_VAR_16(pixel, opt_dest[64 * 64]);

    // we don't know the partition size so we are checking the entire output buffer so
    // we must initialize the buffers
    memset(ref_dest, 0xCD, sizeof(ref_dest));
    memset(opt_dest, 0xCD, sizeof(opt_dest));

    int j = 0;
    intptr_t stride1 = 64, stride2 = STRIDE;
    for (int i = 0; i < ITERS; i++)
    {
        int index = i % TEST_CASES;
        checked(opt, opt_dest, stride1, short_test_buff1[index] + j, stride2);
        ref(ref_dest, stride1, short_test_buff1[index] + j, stride2);

        if (memcmp(ref_dest, opt_dest, 64 * 64 * sizeof(pixel)))
            return false;

        reportfail();
        j += INCR;
    }

    return true;
}

bool PixelHarness::check_copy_ps(copy_ps_t ref, copy_ps_t opt)
{
    ALIGN_VAR_16(int16_t, ref_dest[64 * 64]);
    ALIGN_VAR_16(int16_t, opt_dest[64 * 64]);

    // we don't know the partition size so we are checking the entire output buffer so
    // we must initialize the buffers
    memset(ref_dest, 0xCD, sizeof(ref_dest));
    memset(opt_dest, 0xCD, sizeof(opt_dest));

    int j = 0;
    intptr_t stride = STRIDE;
    for (int i = 0; i < ITERS; i++)
    {
        int index = i % TEST_CASES;
        checked(opt, opt_dest, stride, pixel_test_buff[index] + j, stride);
        ref(ref_dest, stride, pixel_test_buff[index] + j, stride);

        if (memcmp(ref_dest, opt_dest, 64 * 64 * sizeof(int16_t)))
            return false;

        reportfail();
        j += INCR;
    }

    return true;
}

bool PixelHarness::check_copy_ss(copy_ss_t ref, copy_ss_t opt)
{
    ALIGN_VAR_16(int16_t, ref_dest[64 * 64]);
    ALIGN_VAR_16(int16_t, opt_dest[64 * 64]);

    // we don't know the partition size so we are checking the entire output buffer so
    // we must initialize the buffers
    memset(ref_dest, 0xCD, sizeof(ref_dest));
    memset(opt_dest, 0xCD, sizeof(opt_dest));

    int j = 0;
    intptr_t stride = STRIDE;
    for (int i = 0; i < ITERS; i++)
    {
        int index = i % TEST_CASES;
        checked(opt, opt_dest, stride, short_test_buff1[index] + j, stride);
        ref(ref_dest, stride, short_test_buff1[index] + j, stride);

        if (memcmp(ref_dest, opt_dest, 64 * 64 * sizeof(int16_t)))
            return false;

        reportfail();
        j += INCR;
    }

    return true;
}

bool PixelHarness::check_blockfill_s(blockfill_s_t ref, blockfill_s_t opt)
{
    ALIGN_VAR_16(int16_t, ref_dest[64 * 64]);
    ALIGN_VAR_16(int16_t, opt_dest[64 * 64]);

    memset(ref_dest, 0xCD, sizeof(ref_dest));
    memset(opt_dest, 0xCD, sizeof(opt_dest));

    intptr_t stride = 64;
    for (int i = 0; i < ITERS; i++)
    {
        int16_t value = (rand() % SHORT_MAX) + 1;

        checked(opt, opt_dest, stride, value);
        ref(ref_dest, stride, value);

        if (memcmp(ref_dest, opt_dest, 64 * 64 * sizeof(int16_t)))
            return false;

        reportfail();
    }

    return true;
}

bool PixelHarness::check_pixel_sub_ps(pixel_sub_ps_t ref, pixel_sub_ps_t opt)
{
    ALIGN_VAR_16(int16_t, ref_dest[64 * 64]);
    ALIGN_VAR_16(int16_t, opt_dest[64 * 64]);

    memset(ref_dest, 0xCD, sizeof(ref_dest));
    memset(opt_dest, 0xCD, sizeof(opt_dest));

    int j = 0;
    intptr_t stride2 = 64, stride = STRIDE;
    for (int i = 0; i < 1; i++)
    {
        int index1 = rand() % TEST_CASES;
        int index2 = rand() % TEST_CASES;
        checked(opt, opt_dest, stride2, pixel_test_buff[index1] + j,
                pixel_test_buff[index2] + j, stride, stride);
        ref(ref_dest, stride2, pixel_test_buff[index1] + j,
            pixel_test_buff[index2] + j, stride, stride);

        if (memcmp(ref_dest, opt_dest, 64 * 64 * sizeof(int16_t)))
            return false;

        reportfail();
        j += INCR;
    }

    return true;
}

bool PixelHarness::check_scale_pp(scale_t ref, scale_t opt)
{
    ALIGN_VAR_16(pixel, ref_dest[64 * 64]);
    ALIGN_VAR_16(pixel, opt_dest[64 * 64]);

    memset(ref_dest, 0, sizeof(ref_dest));
    memset(opt_dest, 0, sizeof(opt_dest));

    int j = 0;
    intptr_t stride = STRIDE;
    for (int i = 0; i < ITERS; i++)
    {
        int index = i % TEST_CASES;
        checked(opt, opt_dest, pixel_test_buff[index] + j, stride);
        ref(ref_dest, pixel_test_buff[index] + j, stride);

        if (memcmp(ref_dest, opt_dest, 64 * 64 * sizeof(pixel)))
            return false;

        reportfail();
        j += INCR;
    }

    return true;
}

bool PixelHarness::check_transpose(transpose_t ref, transpose_t opt)
{
    ALIGN_VAR_16(pixel, ref_dest[64 * 64]);
    ALIGN_VAR_16(pixel, opt_dest[64 * 64]);

    memset(ref_dest, 0, sizeof(ref_dest));
    memset(opt_dest, 0, sizeof(opt_dest));

    int j = 0;
    intptr_t stride = STRIDE;
    for (int i = 0; i < ITERS; i++)
    {
        int index = i % TEST_CASES;
        checked(opt, opt_dest, pixel_test_buff[index] + j, stride);
        ref(ref_dest, pixel_test_buff[index] + j, stride);

        if (memcmp(ref_dest, opt_dest, 64 * 64 * sizeof(pixel)))
            return false;

        reportfail();
        j += INCR;
    }

    return true;
}

bool PixelHarness::check_pixel_add_ps(pixel_add_ps_t ref, pixel_add_ps_t opt)
{
    ALIGN_VAR_16(pixel, ref_dest[64 * 64]);
    ALIGN_VAR_16(pixel, opt_dest[64 * 64]);

    memset(ref_dest, 0xCD, sizeof(ref_dest));
    memset(opt_dest, 0xCD, sizeof(opt_dest));

    int j = 0;
    intptr_t stride2 = 64, stride = STRIDE;
    for (int i = 0; i < ITERS; i++)
    {
        int index1 = rand() % TEST_CASES;
        int index2 = rand() % TEST_CASES;
        checked(opt, opt_dest, stride2, pixel_test_buff[index1] + j, short_test_buff[index2] + j, stride, stride);
        ref(ref_dest, stride2, pixel_test_buff[index1] + j, short_test_buff[index2] + j, stride, stride);

        if (memcmp(ref_dest, opt_dest, 64 * 64 * sizeof(pixel)))
            return false;

        reportfail();
        j += INCR;
    }

    return true;
}

bool PixelHarness::check_pixel_var(var_t ref, var_t opt)
{
    int j = 0;

    intptr_t stride = STRIDE;

    for (int i = 0; i < ITERS; i++)
    {
        int index = i % TEST_CASES;
        uint64_t vres = checked(opt, pixel_test_buff[index], stride);
        uint64_t cres = ref(pixel_test_buff[index], stride);
        if (vres != cres)
            return false;

        reportfail();
        j += INCR;
    }

    return true;
}

bool PixelHarness::check_ssim_4x4x2_core(ssim_4x4x2_core_t ref, ssim_4x4x2_core_t opt)
{
    ALIGN_VAR_32(int, sum0[2][4]);
    ALIGN_VAR_32(int, sum1[2][4]);

    for (int i = 0; i < ITERS; i++)
    {
        intptr_t stride = rand() % 64;
        int index1 = rand() % TEST_CASES;
        int index2 = rand() % TEST_CASES;
        ref(pixel_test_buff[index1] + i, stride, pixel_test_buff[index2] + i, stride, sum0);
        checked(opt, pixel_test_buff[index1] + i, stride, pixel_test_buff[index2] + i, stride, sum1);

        if (memcmp(sum0, sum1, sizeof(sum0)))
            return false;

        reportfail();
    }

    return true;
}

/* TODO: This function causes crashes when checked. Is this a real bug? */
bool PixelHarness::check_ssim_end(ssim_end4_t ref, ssim_end4_t opt)
{
    ALIGN_VAR_32(int, sum0[5][4]);
    ALIGN_VAR_32(int, sum1[5][4]);

    for (int i = 0; i < ITERS; i++)
    {
        for (int j = 0; j < 5; j++)
        {
            for (int k = 0; k < 4; k++)
            {
                sum0[j][k] = rand() % (1 << 12);
                sum1[j][k] = rand() % (1 << 12);
            }
        }

        int width = (rand() % 4) + 1; // range[1-4]
        float cres = ref(sum0, sum1, width);
        float vres = checked_float(opt, sum0, sum1, width);
        if (fabs(vres - cres) > 0.00001)
            return false;

        reportfail();
    }

    return true;
}

bool PixelHarness::check_addAvg(addAvg_t ref, addAvg_t opt)
{
    ALIGN_VAR_16(pixel, ref_dest[64 * 64]);
    ALIGN_VAR_16(pixel, opt_dest[64 * 64]);

    int j = 0;

    memset(ref_dest, 0xCD, sizeof(ref_dest));
    memset(opt_dest, 0xCD, sizeof(opt_dest));
    intptr_t stride = STRIDE;

    for (int i = 0; i < ITERS; i++)
    {
        int index1 = rand() % TEST_CASES;
        int index2 = rand() % TEST_CASES;
        ref(short_test_buff2[index1] + j, short_test_buff2[index2] + j, ref_dest, stride, stride, stride);
        checked(opt, short_test_buff2[index1] + j, short_test_buff2[index2] + j, opt_dest, stride, stride, stride);
        if (memcmp(ref_dest, opt_dest, 64 * 64 * sizeof(pixel)))
            return false;

        reportfail();
        j += INCR;
    }

    return true;
}

bool PixelHarness::check_calSign(sign_t ref, sign_t opt)
{
    ALIGN_VAR_16(int8_t, ref_dest[64 * 64]);
    ALIGN_VAR_16(int8_t, opt_dest[64 * 64]);

    memset(ref_dest, 0xCD, sizeof(ref_dest));
    memset(opt_dest, 0xCD, sizeof(opt_dest));

    int j = 0;

    for (int i = 0; i < ITERS; i++)
    {
        int width = 16 * (rand() % 4 + 1);

        ref(ref_dest, pbuf2 + j, pbuf3 + j, width);
        checked(opt, opt_dest, pbuf2 + j, pbuf3 + j, width);

        if (memcmp(ref_dest, opt_dest, 64 * 64 * sizeof(int8_t)))
            return false;

        reportfail();
        j += INCR;
    }

    return true;
}

bool PixelHarness::check_saoCuOrgE0_t(saoCuOrgE0_t ref, saoCuOrgE0_t opt)
{
    ALIGN_VAR_16(pixel, ref_dest[64 * 64]);
    ALIGN_VAR_16(pixel, opt_dest[64 * 64]);

    memset(ref_dest, 0xCD, sizeof(ref_dest));
    memset(opt_dest, 0xCD, sizeof(opt_dest));

    int j = 0;

    for (int i = 0; i < ITERS; i++)
    {
        int width = 16 * (rand() % 4 + 1);
        int8_t sign = rand() % 3;
        if (sign == 2)
        {
            sign = -1;
        }

        ref(ref_dest, psbuf1 + j, width, sign);
        checked(opt, opt_dest, psbuf1 + j, width, sign);

        if (memcmp(ref_dest, opt_dest, 64 * 64 * sizeof(pixel)))
            return false;

        reportfail();
        j += INCR;
    }

    return true;
}

bool PixelHarness::check_saoCuOrgE1_t(saoCuOrgE1_t ref, saoCuOrgE1_t opt)
{
    ALIGN_VAR_16(pixel, ref_dest[64 * 64]);
    ALIGN_VAR_16(pixel, opt_dest[64 * 64]);

    memset(ref_dest, 0xCD, sizeof(ref_dest));
    memset(opt_dest, 0xCD, sizeof(opt_dest));

    int j = 0;

    for (int i = 0; i < ITERS; i++)
    {
        int width = 16 * (rand() % 4 + 1);
        int stride = width + 1;

        ref(ref_dest, psbuf2 + j, psbuf1 + j, stride, width);
        checked(opt, opt_dest, psbuf5 + j, psbuf1 + j, stride, width);

        if (memcmp(ref_dest, opt_dest, 64 * 64 * sizeof(pixel)) || memcmp(psbuf2, psbuf5, BUFFSIZE))
            return false;

        reportfail();
        j += INCR;
    }

    return true;
}

bool PixelHarness::check_saoCuOrgE2_t(saoCuOrgE2_t ref, saoCuOrgE2_t opt)
{
    ALIGN_VAR_16(pixel, ref_dest[64 * 64]);
    ALIGN_VAR_16(pixel, opt_dest[64 * 64]);

    memset(ref_dest, 0xCD, sizeof(ref_dest));
    memset(opt_dest, 0xCD, sizeof(opt_dest));

    int j = 0;

    for (int i = 0; i < ITERS; i++)
    {
        int width = 16 * (rand() % 4 + 1);
        int stride = width + 1;

        ref(ref_dest, psbuf1 + j, psbuf2 + j, psbuf3 + j, width, stride);
        checked(opt, opt_dest, psbuf4 + j, psbuf2 + j, psbuf3 + j, width, stride);

        if (memcmp(psbuf1 + j, psbuf4 + j, width * sizeof(int8_t)))
            return false;

        if (memcmp(ref_dest, opt_dest, 64 * 64 * sizeof(pixel)))
            return false;

        reportfail();
        j += INCR;
    }

    return true;
}

bool PixelHarness::check_saoCuOrgE3_t(saoCuOrgE3_t ref, saoCuOrgE3_t opt)
{
    ALIGN_VAR_16(pixel, ref_dest[64 * 64]);
    ALIGN_VAR_16(pixel, opt_dest[64 * 64]);

    memset(ref_dest, 0xCD, sizeof(ref_dest));
    memset(opt_dest, 0xCD, sizeof(opt_dest));

    int j = 0;

    for (int i = 0; i < ITERS; i++)
    {
        int stride = 16 * (rand() % 4 + 1);
        int start = rand() % 2;
        int end = (16 * (rand() % 4 + 1)) - rand() % 2;

        ref(ref_dest, psbuf2 + j, psbuf1 + j, stride, start, end);
        checked(opt, opt_dest, psbuf5 + j, psbuf1 + j, stride, start, end);

        if (memcmp(ref_dest, opt_dest, 64 * 64 * sizeof(pixel)) || memcmp(psbuf2, psbuf5, BUFFSIZE))
            return false;

        reportfail();
        j += INCR;
    }

    return true;
}

bool PixelHarness::check_planecopy_sp(planecopy_sp_t ref, planecopy_sp_t opt)
{
    ALIGN_VAR_16(pixel, ref_dest[64 * 64]);
    ALIGN_VAR_16(pixel, opt_dest[64 * 64]);

    memset(ref_dest, 0xCD, sizeof(ref_dest));
    memset(opt_dest, 0xCD, sizeof(opt_dest));

    int width = 16 + rand() % 48;
    int height = 16 + rand() % 48;
    intptr_t srcStride = 64;
    intptr_t dstStride = width;
    int j = 0;

    for (int i = 0; i < ITERS; i++)
    {
        int index = i % TEST_CASES;
        checked(opt, ushort_test_buff[index] + j, srcStride, opt_dest, dstStride, width, height, (int)8, (uint16_t)255);
        ref(ushort_test_buff[index] + j, srcStride, ref_dest, dstStride, width, height, (int)8, (uint16_t)255);

        if (memcmp(ref_dest, opt_dest, width * height * sizeof(pixel)))
            return false;

        reportfail();
        j += INCR;
    }

    return true;
}

bool PixelHarness::check_planecopy_cp(planecopy_cp_t ref, planecopy_cp_t opt)
{
    ALIGN_VAR_16(pixel, ref_dest[64 * 64]);
    ALIGN_VAR_16(pixel, opt_dest[64 * 64]);

    memset(ref_dest, 0xCD, sizeof(ref_dest));
    memset(opt_dest, 0xCD, sizeof(opt_dest));

    int width = 16 + rand() % 48;
    int height = 16 + rand() % 48;
    intptr_t srcStride = 64;
    intptr_t dstStride = width;
    int j = 0;

    for (int i = 0; i < ITERS; i++)
    {
        int index = i % TEST_CASES;
        checked(opt, uchar_test_buff[index] + j, srcStride, opt_dest, dstStride, width, height, (int)2);
        ref(uchar_test_buff[index] + j, srcStride, ref_dest, dstStride, width, height, (int)2);

        if (memcmp(ref_dest, opt_dest, width * height * sizeof(pixel)))
            return false;

        reportfail();
        j += INCR;
    }

    return true;
}

bool PixelHarness::check_psyCost_pp(pixelcmp_t ref, pixelcmp_t opt)
{
    int j = 0, index1, index2, optres, refres;
    intptr_t stride = STRIDE;

    for (int i = 0; i < ITERS; i++)
    {
        index1 = rand() % TEST_CASES;
        index2 = rand() % TEST_CASES;
        optres = (int)checked(opt, pixel_test_buff[index1], stride, pixel_test_buff[index2] + j, stride);
        refres = ref(pixel_test_buff[index1], stride, pixel_test_buff[index2] + j, stride);

        if (optres != refres)
            return false;

        reportfail();
        j += INCR;
    }

    return true;
}

bool PixelHarness::check_psyCost_ss(pixelcmp_ss_t ref, pixelcmp_ss_t opt)
{
    int j = 0, index1, index2, optres, refres;
    intptr_t stride = STRIDE;

    for (int i = 0; i < ITERS; i++)
    {
        index1 = rand() % TEST_CASES;
        index2 = rand() % TEST_CASES;
        optres = (int)checked(opt, short_test_buff[index1], stride, short_test_buff[index2] + j, stride);
        refres = ref(short_test_buff[index1], stride, short_test_buff[index2] + j, stride);

        if (optres != refres)
            return false;

        reportfail();
        j += INCR;
    }

    return true;
}

bool PixelHarness::check_saoCuOrgB0_t(saoCuOrgB0_t ref, saoCuOrgB0_t opt)
{
    ALIGN_VAR_16(pixel, ref_dest[64 * 64]);
    ALIGN_VAR_16(pixel, opt_dest[64 * 64]);

    memset(ref_dest, 0xCD, sizeof(ref_dest));
    memset(opt_dest, 0xCD, sizeof(opt_dest));

    int j = 0;

    for (int i = 0; i < ITERS; i++)
    {
        int width = 16 * (rand() % 4 + 1);
        int height = rand() % 64 +1;
        int stride = rand() % 65;

        ref(ref_dest, psbuf1 + j, width, height, stride);
        checked(opt, opt_dest, psbuf1 + j, width, height, stride);

        if (memcmp(ref_dest, opt_dest, 64 * 64 * sizeof(pixel)))
            return false;

        reportfail();
        j += INCR;
    }

    return true;
}


bool PixelHarness::testPartition(int part, const EncoderPrimitives& ref, const EncoderPrimitives& opt)
{
    if (opt.pu[part].satd)
    {
        if (!check_pixelcmp(ref.pu[part].satd, opt.pu[part].satd))
        {
            printf("satd[%s]: failed!\n", lumaPartStr[part]);
            return false;
        }
    }

    if (opt.pu[part].sad)
    {
        if (!check_pixelcmp(ref.pu[part].sad, opt.pu[part].sad))
        {
            printf("sad[%s]: failed!\n", lumaPartStr[part]);
            return false;
        }
    }

    if (opt.pu[part].sse_pp)
    {
        if (!check_pixelcmp(ref.pu[part].sse_pp, opt.pu[part].sse_pp))
        {
            printf("sse_pp[%s]: failed!\n", lumaPartStr[part]);
            return false;
        }
    }

    if (opt.pu[part].sse_sp)
    {
        if (!check_pixelcmp_sp(ref.pu[part].sse_sp, opt.pu[part].sse_sp))
        {
            printf("sse_sp[%s]: failed!\n", lumaPartStr[part]);
            return false;
        }
    }

    if (opt.pu[part].sad_x3)
    {
        if (!check_pixelcmp_x3(ref.pu[part].sad_x3, opt.pu[part].sad_x3))
        {
            printf("sad_x3[%s]: failed!\n", lumaPartStr[part]);
            return false;
        }
    }

    if (opt.pu[part].sad_x4)
    {
        if (!check_pixelcmp_x4(ref.pu[part].sad_x4, opt.pu[part].sad_x4))
        {
            printf("sad_x4[%s]: failed!\n", lumaPartStr[part]);
            return false;
        }
    }

    if (opt.pu[part].pixelavg_pp)
    {
        if (!check_pixelavg_pp(ref.pu[part].pixelavg_pp, opt.pu[part].pixelavg_pp))
        {
            printf("pixelavg_pp[%s]: failed!\n", lumaPartStr[part]);
            return false;
        }
    }

    if (opt.pu[part].luma_copy_pp)
    {
        if (!check_copy_pp(ref.pu[part].luma_copy_pp, opt.pu[part].luma_copy_pp))
        {
            printf("luma_copy_pp[%s] failed\n", lumaPartStr[part]);
            return false;
        }
    }

    if (opt.pu[part].luma_copy_ss)
    {
        if (!check_copy_ss(ref.pu[part].luma_copy_ss, opt.pu[part].luma_copy_ss))
        {
            printf("luma_copy_ss[%s] failed\n", lumaPartStr[part]);
            return false;
        }
    }

    if (opt.pu[part].luma_addAvg)
    {
        if (!check_addAvg(ref.pu[part].luma_addAvg, opt.pu[part].luma_addAvg))
        {
            printf("luma_addAvg[%s] failed\n", lumaPartStr[part]);
            return false;
        }
    }

    if (part < NUM_SQUARE_BLOCKS)
    {
        if (opt.cu[part].sse_ss)
        {
            if (!check_pixelcmp_ss(ref.cu[part].sse_ss, opt.cu[part].sse_ss))
            {
                printf("sse_ss[%s]: failed!\n", lumaPartStr[part]);
                return false;
            }
        }

        if (opt.cu[part].luma_sub_ps)
        {
            if (!check_pixel_sub_ps(ref.cu[part].luma_sub_ps, opt.cu[part].luma_sub_ps))
            {
                printf("luma_sub_ps[%s] failed\n", lumaPartStr[part]);
                return false;
            }
        }

        if (opt.cu[part].luma_add_ps)
        {
            if (!check_pixel_add_ps(ref.cu[part].luma_add_ps, opt.cu[part].luma_add_ps))
            {
                printf("luma_add_ps[%s] failed\n", lumaPartStr[part]);
                return false;
            }
        }

        if (opt.cu[part].luma_copy_sp)
        {
            if (!check_copy_sp(ref.cu[part].luma_copy_sp, opt.cu[part].luma_copy_sp))
            {
                printf("luma_copy_sp[%s] failed\n", lumaPartStr[part]);
                return false;
            }
        }

        if (opt.cu[part].luma_copy_ps)
        {
            if (!check_copy_ps(ref.cu[part].luma_copy_ps, opt.cu[part].luma_copy_ps))
            {
                printf("luma_copy_ps[%s] failed\n", lumaPartStr[part]);
                return false;
            }
        }
    }

    for (int i = 0; i < X265_CSP_COUNT; i++)
    {
        if (opt.chroma[i].pu[part].copy_pp)
        {
            if (!check_copy_pp(ref.chroma[i].pu[part].copy_pp, opt.chroma[i].pu[part].copy_pp))
            {
                printf("chroma_copy_pp[%s][%s] failed\n", x265_source_csp_names[i], chromaPartStr[i][part]);
                return false;
            }
        }
        if (opt.chroma[i].pu[part].addAvg)
        {
            if (!check_addAvg(ref.chroma[i].pu[part].addAvg, opt.chroma[i].pu[part].addAvg))
            {
                printf("chroma_addAvg[%s][%s] failed\n", x265_source_csp_names[i], chromaPartStr[i][part]);
                return false;
            }
        }
        if (part < NUM_SQUARE_BLOCKS)
        {
            if (opt.chroma[i].cu[part].sub_ps)
            {
                if (!check_pixel_sub_ps(ref.chroma[i].cu[part].sub_ps, opt.chroma[i].cu[part].sub_ps))
                {
                    printf("chroma_sub_ps[%s][%s] failed\n", x265_source_csp_names[i], chromaPartStr[i][part]);
                    return false;
                }
            }
            if (opt.chroma[i].cu[part].add_ps)
            {
                if (!check_pixel_add_ps(ref.chroma[i].cu[part].add_ps, opt.chroma[i].cu[part].add_ps))
                {
                    printf("chroma_add_ps[%s][%s] failed\n", x265_source_csp_names[i], chromaPartStr[i][part]);
                    return false;
                }
            }
            if (opt.chroma[i].cu[part].copy_sp)
            {
                if (!check_copy_sp(ref.chroma[i].cu[part].copy_sp, opt.chroma[i].cu[part].copy_sp))
                {
                    printf("chroma_copy_sp[%s][%s] failed\n", x265_source_csp_names[i], chromaPartStr[i][part]);
                    return false;
                }
            }
            if (opt.chroma[i].cu[part].copy_ps)
            {
                if (!check_copy_ps(ref.chroma[i].cu[part].copy_ps, opt.chroma[i].cu[part].copy_ps))
                {
                    printf("chroma_copy_ps[%s][%s] failed\n", x265_source_csp_names[i], chromaPartStr[i][part]);
                    return false;
                }
            }
            if (opt.chroma[i].cu[part].copy_ss)
            {
                if (!check_copy_ss(ref.chroma[i].cu[part].copy_ss, opt.chroma[i].cu[part].copy_ss))
                {
                    printf("chroma_copy_ss[%s][%s] failed\n", x265_source_csp_names[i], chromaPartStr[i][part]);
                    return false;
                }
            }
        }
    }

    return true;
}

bool PixelHarness::testCorrectness(const EncoderPrimitives& ref, const EncoderPrimitives& opt)
{
    for (int size = 4; size <= 64; size *= 2)
    {
        int part = partitionFromSizes(size, size); // 2Nx2N
        if (!testPartition(part, ref, opt)) return false;

        if (size > 4)
        {
            part = partitionFromSizes(size, size >> 1); // 2NxN
            if (!testPartition(part, ref, opt)) return false;
            part = partitionFromSizes(size >> 1, size); // Nx2N
            if (!testPartition(part, ref, opt)) return false;
        }
        if (size > 8)
        {
            // 4 AMP modes
            part = partitionFromSizes(size, size >> 2);
            if (!testPartition(part, ref, opt)) return false;
            part = partitionFromSizes(size, 3 * (size >> 2));
            if (!testPartition(part, ref, opt)) return false;

            part = partitionFromSizes(size >> 2, size);
            if (!testPartition(part, ref, opt)) return false;
            part = partitionFromSizes(3 * (size >> 2), size);
            if (!testPartition(part, ref, opt)) return false;
        }
    }

    for (int i = 0; i < NUM_SQUARE_BLOCKS; i++)
    {
        if (opt.cu[i].calcresidual)
        {
            if (!check_calresidual(ref.cu[i].calcresidual, opt.cu[i].calcresidual))
            {
                printf("calcresidual width: %d failed!\n", 4 << i);
                return false;
            }
        }
        if (opt.cu[i].sa8d)
        {
            if (!check_pixelcmp(ref.cu[i].sa8d, opt.cu[i].sa8d))
            {
                printf("sa8d[%dx%d]: failed!\n", 4 << i, 4 << i);
                return false;
            }
        }

        if ((i <= BLOCK_32x32) && opt.cu[i].ssd_s)
        {
            if (!check_ssd_s(ref.cu[i].ssd_s, opt.cu[i].ssd_s))
            {
                printf("ssd_s[%dx%d]: failed!\n", 4 << i, 4 << i);
                return false;
            }
        }

        if (opt.cu[i].blockfill_s)
        {
            if (!check_blockfill_s(ref.cu[i].blockfill_s, opt.cu[i].blockfill_s))
            {
                printf("blockfill_s[%dx%d]: failed!\n", 4 << i, 4 << i);
                return false;
            }
        }
        if (opt.cu[i].transpose)
        {
            if (!check_transpose(ref.cu[i].transpose, opt.cu[i].transpose))
            {
                printf("transpose[%dx%d] failed\n", 4 << i, 4 << i);
                return false;
            }
        }

        if (opt.cu[i].var)
        {
            if (!check_pixel_var(ref.cu[i].var, opt.cu[i].var))
            {
                printf("var[%dx%d] failed\n", 4 << i, 4 << i);
                return false;
            }
        }

        if ((i < BLOCK_64x64) && opt.cu[i].copy_cnt)
        {
            if (!check_copy_cnt_t(ref.cu[i].copy_cnt, opt.cu[i].copy_cnt))
            {
                printf("copy_cnt[%dx%d] failed!\n", 4 << i, 4 << i);
                return false;
            }
        }

        if ((i < BLOCK_64x64) && opt.cu[i].cpy2Dto1D_shl)
        {
            if (!check_cpy2Dto1D_shl_t(ref.cu[i].cpy2Dto1D_shl, opt.cu[i].cpy2Dto1D_shl))
            {
                printf("cpy2Dto1D_shl failed!\n");
                return false;
            }
        }

        if ((i < BLOCK_64x64) && opt.cu[i].cpy2Dto1D_shr)
        {
            if (!check_cpy2Dto1D_shr_t(ref.cu[i].cpy2Dto1D_shr, opt.cu[i].cpy2Dto1D_shr))
            {
                printf("cpy2Dto1D_shr failed!\n");
                return false;
            }
        }

        if ((i < BLOCK_64x64) && opt.cu[i].cpy1Dto2D_shl)
        {
            if (!check_cpy1Dto2D_shl_t(ref.cu[i].cpy1Dto2D_shl, opt.cu[i].cpy1Dto2D_shl))
            {
                printf("cpy1Dto2D_shl[%dx%d] failed!\n", 4 << i, 4 << i);
                return false;
            }
        }

        if ((i < BLOCK_64x64) && opt.cu[i].cpy1Dto2D_shr)
        {
            if (!check_cpy1Dto2D_shr_t(ref.cu[i].cpy1Dto2D_shr, opt.cu[i].cpy1Dto2D_shr))
            {
                printf("cpy1Dto2D_shr[%dx%d] failed!\n", 4 << i, 4 << i);
                return false;
            }
        }

        if (opt.cu[i].psy_cost_pp)
        {
            if (!check_psyCost_pp(ref.cu[i].psy_cost_pp, opt.cu[i].psy_cost_pp))
            {
                printf("\npsy_cost_pp[%dx%d] failed!\n", 4 << i, 4 << i);
                return false;
            }
        }

        if (opt.cu[i].psy_cost_ss)
        {
            if (!check_psyCost_ss(ref.cu[i].psy_cost_ss, opt.cu[i].psy_cost_ss))
            {
                printf("\npsy_cost_ss[%dx%d] failed!\n", 4 << i, 4 << i);
                return false;
            }
        }
    }

    if (opt.weight_pp)
    {
        if (!check_weightp(ref.weight_pp, opt.weight_pp))
        {
            printf("Weighted Prediction (pixel) failed!\n");
            return false;
        }
    }

    if (opt.weight_sp)
    {
        if (!check_weightp(ref.weight_sp, opt.weight_sp))
        {
            printf("Weighted Prediction (short) failed!\n");
            return false;
        }
    }

    if (opt.frameInitLowres)
    {
        if (!check_downscale_t(ref.frameInitLowres, opt.frameInitLowres))
        {
            printf("downscale failed!\n");
            return false;
        }
    }

    if (opt.scale1D_128to64)
    {
        if (!check_scale_pp(ref.scale1D_128to64, opt.scale1D_128to64))
        {
            printf("scale1D_128to64 failed!\n");
            return false;
        }
    }

    if (opt.scale2D_64to32)
    {
        if (!check_scale_pp(ref.scale2D_64to32, opt.scale2D_64to32))
        {
            printf("scale2D_64to32 failed!\n");
            return false;
        }
    }

    if (opt.ssim_4x4x2_core)
    {
        if (!check_ssim_4x4x2_core(ref.ssim_4x4x2_core, opt.ssim_4x4x2_core))
        {
            printf("ssim_end_4 failed!\n");
            return false;
        }
    }

    if (opt.ssim_end_4)
    {
        if (!check_ssim_end(ref.ssim_end_4, opt.ssim_end_4))
        {
            printf("ssim_end_4 failed!\n");
            return false;
        }
    }

    if (opt.sign)
    {
        if (!check_calSign(ref.sign, opt.sign))
        {
            printf("calSign failed\n");
            return false;
        }
    }

    if (opt.saoCuOrgE0)
    {
        if (!check_saoCuOrgE0_t(ref.saoCuOrgE0, opt.saoCuOrgE0))
        {
            printf("SAO_EO_0 failed\n");
            return false;
        }
    }

    if (opt.saoCuOrgE1)
    {
        if (!check_saoCuOrgE1_t(ref.saoCuOrgE1, opt.saoCuOrgE1))
        {
            printf("SAO_EO_1 failed\n");
            return false;
        }
    }

    if (opt.saoCuOrgE2)
    {
        if (!check_saoCuOrgE2_t(ref.saoCuOrgE2, opt.saoCuOrgE2))
        {
            printf("SAO_EO_2 failed\n");
            return false;
        }
    }

    if (opt.saoCuOrgE3)
    {
        if (!check_saoCuOrgE3_t(ref.saoCuOrgE3, opt.saoCuOrgE3))
        {
            printf("SAO_EO_3 failed\n");
            return false;
        }
    }

    if (opt.saoCuOrgB0)
    {
        if (!check_saoCuOrgB0_t(ref.saoCuOrgB0, opt.saoCuOrgB0))
        {
            printf("SAO_BO_0 failed\n");
            return false;
        }
    }

    if (opt.planecopy_sp)
    {
        if (!check_planecopy_sp(ref.planecopy_sp, opt.planecopy_sp))
        {
            printf("planecopy_sp failed\n");
            return false;
        }
    }

    if (opt.planecopy_cp)
    {
        if (!check_planecopy_cp(ref.planecopy_cp, opt.planecopy_cp))
        {
            printf("planecopy_cp failed\n");
            return false;
        }
    }

    return true;
}

void PixelHarness::measurePartition(int part, const EncoderPrimitives& ref, const EncoderPrimitives& opt)
{
    ALIGN_VAR_16(int, cres[16]);
    pixel *fref = pbuf2 + 2 * INCR;
    char header[128];
#define HEADER(str, ...) sprintf(header, str, __VA_ARGS__); printf("%22s", header);

    if (opt.pu[part].satd)
    {
        HEADER("satd[%s]", lumaPartStr[part]);
        REPORT_SPEEDUP(opt.pu[part].satd, ref.pu[part].satd, pbuf1, STRIDE, fref, STRIDE);
    }

    if (opt.pu[part].pixelavg_pp)
    {
        HEADER("avg_pp[%s]", lumaPartStr[part]);
        REPORT_SPEEDUP(opt.pu[part].pixelavg_pp, ref.pu[part].pixelavg_pp, pbuf1, STRIDE, pbuf2, STRIDE, pbuf3, STRIDE, 32);
    }

    if (opt.pu[part].sad)
    {
        HEADER("sad[%s]", lumaPartStr[part]);
        REPORT_SPEEDUP(opt.pu[part].sad, ref.pu[part].sad, pbuf1, STRIDE, fref, STRIDE);
    }

    if (opt.pu[part].sad_x3)
    {
        HEADER("sad_x3[%s]", lumaPartStr[part]);
        REPORT_SPEEDUP(opt.pu[part].sad_x3, ref.pu[part].sad_x3, pbuf1, fref, fref + 1, fref - 1, FENC_STRIDE + 5, &cres[0]);
    }

    if (opt.pu[part].sad_x4)
    {
        HEADER("sad_x4[%s]", lumaPartStr[part]);
        REPORT_SPEEDUP(opt.pu[part].sad_x4, ref.pu[part].sad_x4, pbuf1, fref, fref + 1, fref - 1, fref - INCR, FENC_STRIDE + 5, &cres[0]);
    }

    if (opt.pu[part].sse_pp)
    {
        HEADER("sse_pp[%s]", lumaPartStr[part]);
        REPORT_SPEEDUP(opt.pu[part].sse_pp, ref.pu[part].sse_pp, pbuf1, STRIDE, fref, STRIDE);
    }

    if (opt.pu[part].sse_sp)
    {
        HEADER("sse_sp[%s]", lumaPartStr[part]);
        REPORT_SPEEDUP(opt.pu[part].sse_sp, ref.pu[part].sse_sp, (int16_t*)pbuf1, STRIDE, fref, STRIDE);
    }

    if (opt.pu[part].luma_copy_pp)
    {
        HEADER("luma_copy_pp[%s]", lumaPartStr[part]);
        REPORT_SPEEDUP(opt.pu[part].luma_copy_pp, ref.pu[part].luma_copy_pp, pbuf1, 64, pbuf2, 128);
    }

    if (opt.pu[part].luma_copy_ss)
    {
        HEADER("luma_copy_ss[%s]", lumaPartStr[part]);
        REPORT_SPEEDUP(opt.pu[part].luma_copy_ss, ref.pu[part].luma_copy_ss, sbuf1, 64, sbuf2, 128);
    }

    if (opt.pu[part].luma_addAvg)
    {
        HEADER("luma_addAvg[%s]", lumaPartStr[part]);
        REPORT_SPEEDUP(opt.pu[part].luma_addAvg, ref.pu[part].luma_addAvg, sbuf1, sbuf2, pbuf1, STRIDE, STRIDE, STRIDE);
    }

    if (part < NUM_SQUARE_BLOCKS)
    {
        if (opt.cu[part].sse_ss)
        {
            HEADER("sse_ss[%s]", lumaPartStr[part]);
            REPORT_SPEEDUP(opt.cu[part].sse_ss, ref.cu[part].sse_ss, (int16_t*)pbuf1, STRIDE, (int16_t*)fref, STRIDE);
        }
        if (opt.cu[part].luma_sub_ps)
        {
            HEADER("luma_sub_ps[%s]", lumaPartStr[part]);
            REPORT_SPEEDUP(opt.cu[part].luma_sub_ps, ref.cu[part].luma_sub_ps, (int16_t*)pbuf1, FENC_STRIDE, pbuf2, pbuf1, STRIDE, STRIDE);
        }
        if (opt.cu[part].luma_add_ps)
        {
            HEADER("luma_add_ps[%s]", lumaPartStr[part]);
            REPORT_SPEEDUP(opt.cu[part].luma_add_ps, ref.cu[part].luma_add_ps, pbuf1, FENC_STRIDE, pbuf2, sbuf1, STRIDE, STRIDE);
        }
        if (opt.cu[part].luma_copy_sp)
        {
            HEADER("luma_copy_sp[%s]", lumaPartStr[part]);
            REPORT_SPEEDUP(opt.cu[part].luma_copy_sp, ref.cu[part].luma_copy_sp, pbuf1, 64, sbuf3, 128);
        }
        if (opt.cu[part].luma_copy_ps)
        {
            HEADER("luma_copy_ps[%s]", lumaPartStr[part]);
            REPORT_SPEEDUP(opt.cu[part].luma_copy_ps, ref.cu[part].luma_copy_ps, sbuf1, 64, pbuf1, 128);
        }
    }

    for (int i = 0; i < X265_CSP_COUNT; i++)
    {
        if (opt.chroma[i].pu[part].copy_pp)
        {
            HEADER("[%s] copy_pp[%s]", x265_source_csp_names[i], chromaPartStr[i][part]);
            REPORT_SPEEDUP(opt.chroma[i].pu[part].copy_pp, ref.chroma[i].pu[part].copy_pp, pbuf1, 64, pbuf2, 128);
        }
        if (opt.chroma[i].pu[part].addAvg)
        {
            HEADER("[%s]  addAvg[%s]", x265_source_csp_names[i], chromaPartStr[i][part]);
            REPORT_SPEEDUP(opt.chroma[i].pu[part].addAvg, ref.chroma[i].pu[part].addAvg, sbuf1, sbuf2, pbuf1, STRIDE, STRIDE, STRIDE);
        }
        if (part < NUM_SQUARE_BLOCKS)
        {
            if (opt.chroma[i].cu[part].copy_ss)
            {
                HEADER("[%s] copy_ss[%s]", x265_source_csp_names[i], chromaPartStr[i][part]);
                REPORT_SPEEDUP(opt.chroma[i].cu[part].copy_ss, ref.chroma[i].cu[part].copy_ss, sbuf1, 64, sbuf2, 128);
            }
            if (opt.chroma[i].cu[part].copy_ps)
            {
                HEADER("[%s] copy_ps[%s]", x265_source_csp_names[i], chromaPartStr[i][part]);
                REPORT_SPEEDUP(opt.chroma[i].cu[part].copy_ps, ref.chroma[i].cu[part].copy_ps, sbuf1, 64, pbuf1, 128);
            }
            if (opt.chroma[i].cu[part].copy_sp)
            {
                HEADER("[%s] copy_sp[%s]", x265_source_csp_names[i], chromaPartStr[i][part]);
                REPORT_SPEEDUP(opt.chroma[i].cu[part].copy_sp, ref.chroma[i].cu[part].copy_sp, pbuf1, 64, sbuf3, 128);
            }
            if (opt.chroma[i].cu[part].sub_ps)
            {
                HEADER("[%s]  sub_ps[%s]", x265_source_csp_names[i], chromaPartStr[i][part]);
                REPORT_SPEEDUP(opt.chroma[i].cu[part].sub_ps, ref.chroma[i].cu[part].sub_ps, (int16_t*)pbuf1, FENC_STRIDE, pbuf2, pbuf1, STRIDE, STRIDE);
            }
            if (opt.chroma[i].cu[part].add_ps)
            {
                HEADER("[%s]  add_ps[%s]", x265_source_csp_names[i], chromaPartStr[i][part]);
                REPORT_SPEEDUP(opt.chroma[i].cu[part].add_ps, ref.chroma[i].cu[part].add_ps, pbuf1, FENC_STRIDE, pbuf2, sbuf1, STRIDE, STRIDE);
            }
        }
    }

#undef HEADER
}

void PixelHarness::measureSpeed(const EncoderPrimitives& ref, const EncoderPrimitives& opt)
{
    char header[128];

#define HEADER(str, ...) sprintf(header, str, __VA_ARGS__); printf("%22s", header);
#define HEADER0(str) printf("%22s", str);

    for (int size = 4; size <= 64; size *= 2)
    {
        int part = partitionFromSizes(size, size); // 2Nx2N
        measurePartition(part, ref, opt);

        if (size > 4)
        {
            part = partitionFromSizes(size, size >> 1); // 2NxN
            measurePartition(part, ref, opt);
            part = partitionFromSizes(size >> 1, size); // Nx2N
            measurePartition(part, ref, opt);
        }
        if (size > 8)
        {
            // 4 AMP modes
            part = partitionFromSizes(size, size >> 2);
            measurePartition(part, ref, opt);
            part = partitionFromSizes(size, 3 * (size >> 2));
            measurePartition(part, ref, opt);

            part = partitionFromSizes(size >> 2, size);
            measurePartition(part, ref, opt);
            part = partitionFromSizes(3 * (size >> 2), size);
            measurePartition(part, ref, opt);
        }
    }

    for (int i = 0; i < NUM_SQUARE_BLOCKS; i++)
    {
        if ((i <= BLOCK_32x32) && opt.cu[i].ssd_s)
        {
            HEADER("ssd_s[%dx%d]", 4 << i, 4 << i);
            REPORT_SPEEDUP(opt.cu[i].ssd_s, ref.cu[i].ssd_s, sbuf1, STRIDE);
        }
        if (opt.cu[i].sa8d)
        {
            HEADER("sa8d[%dx%d]", 4 << i, 4 << i);
            REPORT_SPEEDUP(opt.cu[i].sa8d, ref.cu[i].sa8d, pbuf1, STRIDE, pbuf2, STRIDE);
        }
        if (opt.cu[i].calcresidual)
        {
            HEADER("residual[%dx%d]", 4 << i, 4 << i);
            REPORT_SPEEDUP(opt.cu[i].calcresidual, ref.cu[i].calcresidual, pbuf1, pbuf2, sbuf1, 64);
        }

        if (opt.cu[i].blockfill_s)
        {
            HEADER("blkfill[%dx%d]", 4 << i, 4 << i);
            REPORT_SPEEDUP(opt.cu[i].blockfill_s, ref.cu[i].blockfill_s, sbuf1, 64, SHORT_MAX);
        }

        if (opt.cu[i].transpose)
        {
            HEADER("transpose[%dx%d]", 4 << i, 4 << i);
            REPORT_SPEEDUP(opt.cu[i].transpose, ref.cu[i].transpose, pbuf1, pbuf2, STRIDE);
        }

        if (opt.cu[i].var)
        {
            HEADER("var[%dx%d]", 4 << i, 4 << i);
            REPORT_SPEEDUP(opt.cu[i].var, ref.cu[i].var, pbuf1, STRIDE);
        }

        if ((i < BLOCK_64x64) && opt.cu[i].cpy2Dto1D_shl)
        {
            HEADER("cpy2Dto1D_shl[%dx%d]", 4 << i, 4 << i);
            REPORT_SPEEDUP(opt.cu[i].cpy2Dto1D_shl, ref.cu[i].cpy2Dto1D_shl, sbuf1, sbuf2, STRIDE, MAX_TR_DYNAMIC_RANGE - X265_DEPTH - (i + 2));
        }

        if ((i < BLOCK_64x64) && opt.cu[i].cpy2Dto1D_shr)
        {
            HEADER("cpy2Dto1D_shr[%dx%d]", 4 << i, 4 << i);
            REPORT_SPEEDUP(opt.cu[i].cpy2Dto1D_shr, ref.cu[i].cpy2Dto1D_shr, sbuf1, sbuf2, STRIDE, 3);
        }

        if ((i < BLOCK_64x64) && opt.cu[i].cpy1Dto2D_shl)
        {
            HEADER("cpy1Dto2D_shl[%dx%d]", 4 << i, 4 << i);
            REPORT_SPEEDUP(opt.cu[i].cpy1Dto2D_shl, ref.cu[i].cpy1Dto2D_shl, sbuf1, sbuf2, STRIDE, 64);
        }

        if ((i < BLOCK_64x64) && opt.cu[i].cpy1Dto2D_shr)
        {
            HEADER("cpy1Dto2D_shr[%dx%d]", 4 << i, 4 << i);
            REPORT_SPEEDUP(opt.cu[i].cpy1Dto2D_shr, ref.cu[i].cpy1Dto2D_shr, sbuf1, sbuf2, STRIDE, 64);
        }

        if ((i < BLOCK_64x64) && opt.cu[i].copy_cnt)
        {
            HEADER("copy_cnt[%dx%d]", 4 << i, 4 << i);
            REPORT_SPEEDUP(opt.cu[i].copy_cnt, ref.cu[i].copy_cnt, sbuf1, sbuf2, STRIDE);
        }

        if (opt.cu[i].psy_cost_pp)
        {
            HEADER("psy_cost_pp[%dx%d]", 4 << i, 4 << i);
            REPORT_SPEEDUP(opt.cu[i].psy_cost_pp, ref.cu[i].psy_cost_pp, pbuf1, STRIDE, pbuf2, STRIDE);
        }

        if (opt.cu[i].psy_cost_ss)
        {
            HEADER("psy_cost_ss[%dx%d]", 4 << i, 4 << i);
            REPORT_SPEEDUP(opt.cu[i].psy_cost_ss, ref.cu[i].psy_cost_ss, sbuf1, STRIDE, sbuf2, STRIDE);
        }
    }

    if (opt.weight_pp)
    {
        HEADER0("weight_pp");
        REPORT_SPEEDUP(opt.weight_pp, ref.weight_pp, pbuf1, pbuf2, 64, 32, 32, 128, 1 << 9, 10, 100);
    }

    if (opt.weight_sp)
    {
        HEADER0("weight_sp");
        REPORT_SPEEDUP(opt.weight_sp, ref.weight_sp, (int16_t*)sbuf1, pbuf1, 64, 64, 32, 32, 128, 1 << 9, 10, 100);
    }

    if (opt.frameInitLowres)
    {
        HEADER0("downscale");
        REPORT_SPEEDUP(opt.frameInitLowres, ref.frameInitLowres, pbuf2, pbuf1, pbuf2, pbuf3, pbuf4, 64, 64, 64, 64);
    }

    if (opt.scale1D_128to64)
    {
        HEADER0("scale1D_128to64");
        REPORT_SPEEDUP(opt.scale1D_128to64, ref.scale1D_128to64, pbuf2, pbuf1, 64);
    }

    if (opt.scale2D_64to32)
    {
        HEADER0("scale2D_64to32");
        REPORT_SPEEDUP(opt.scale2D_64to32, ref.scale2D_64to32, pbuf2, pbuf1, 64);
    }

    if (opt.ssim_4x4x2_core)
    {
        HEADER0("ssim_4x4x2_core");
        REPORT_SPEEDUP(opt.ssim_4x4x2_core, ref.ssim_4x4x2_core, pbuf1, 64, pbuf2, 64, (int(*)[4])sbuf1);
    }

    if (opt.ssim_end_4)
    {
        HEADER0("ssim_end_4");
        REPORT_SPEEDUP(opt.ssim_end_4, ref.ssim_end_4, (int(*)[4])pbuf2, (int(*)[4])pbuf1, 4);
    }

    if (opt.sign)
    {
        HEADER0("calSign");
        REPORT_SPEEDUP(opt.sign, ref.sign, psbuf1, pbuf1, pbuf2, 64);
    }

    if (opt.saoCuOrgE0)
    {
        HEADER0("SAO_EO_0");
        REPORT_SPEEDUP(opt.saoCuOrgE0, ref.saoCuOrgE0, pbuf1, psbuf1, 64, 1);
    }

    if (opt.saoCuOrgE1)
    {
        HEADER0("SAO_EO_1");
        REPORT_SPEEDUP(opt.saoCuOrgE1, ref.saoCuOrgE1, pbuf1, psbuf2, psbuf1, 64, 64);
    }

    if (opt.saoCuOrgE2)
    {
        HEADER0("SAO_EO_2");
        REPORT_SPEEDUP(opt.saoCuOrgE2, ref.saoCuOrgE2, pbuf1, psbuf1, psbuf2, psbuf3, 64, 64);
    }

    if (opt.saoCuOrgE3)
    {
        HEADER0("SAO_EO_3");
        REPORT_SPEEDUP(opt.saoCuOrgE3, ref.saoCuOrgE3, pbuf1, psbuf2, psbuf1, 64, 0, 64);
    }

    if (opt.saoCuOrgB0)
    {
        HEADER0("SAO_BO_0");
        REPORT_SPEEDUP(opt.saoCuOrgB0, ref.saoCuOrgB0, pbuf1, psbuf1, 64, 64, 64);
    }

    if (opt.planecopy_sp)
    {
        HEADER0("planecopy_sp");
        REPORT_SPEEDUP(opt.planecopy_sp, ref.planecopy_sp, ushort_test_buff[0], 64, pbuf1, 64, 64, 64, 8, 255);
    }

    if (opt.planecopy_cp)
    {
        HEADER0("planecopy_cp");
        REPORT_SPEEDUP(opt.planecopy_cp, ref.planecopy_cp, uchar_test_buff[0], 64, pbuf1, 64, 64, 64, 2);
    }
}
