/*****************************************************************************
* Copyright (C) 2013 x265 project
*
* Authors: Deepthi Nandakumar <deepthi@multicorewareinc.com>
*          Steve Borho <steve@borho.org>
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

#ifndef X265_ANALYSIS_H
#define X265_ANALYSIS_H

#include "common.h"
#include "predict.h"
#include "quant.h"
#include "yuv.h"
#include "shortyuv.h"
#include "TLibCommon/TComDataCU.h"

#include "entropy.h"
#include "search.h"

namespace x265 {
// private namespace

class Entropy;

class Analysis : public Search
{
public:

    enum {
        PRED_MERGE,
        PRED_SKIP,
        PRED_INTRA,
        PRED_2Nx2N,
        PRED_Nx2N,
        PRED_2NxN,
        PRED_SPLIT,     /* prediction modes up to here may be used for RD 0..4 */
        PRED_2NxnU,     /* prediction modes below here only used for RD 5,6 */
        PRED_2NxnD,
        PRED_nLx2N,
        PRED_nRx2N,
        PRED_INTRA_NxN, /* 4x4 intra PU blocks for 8x8 CU */
        MAX_PRED_TYPES
    };

    struct ModeDepth
    {
        Mode           pred[MAX_PRED_TYPES];
        Mode*          bestMode;
        Yuv            fencYuv;
        DataCUMemPool  cuMemPool;
        MVFieldMemPool mvFieldMemPool;
    };

    ModeDepth m_modeDepth[NUM_CU_DEPTH];

    Analysis();
    bool create(uint32_t totalDepth, uint32_t maxWidth, ThreadLocalData* tld);
    void destroy();
    Search::Mode& compressCTU(TComDataCU& ctu, Frame& frame, const Entropy& initialContext);

protected:

    /* mode analysis distribution */
    int           m_totalNumJobs;
    volatile int  m_numAcquiredJobs;
    volatile int  m_numCompletedJobs;
    Event         m_modeCompletionEvent;
    bool findJob(int threadId);
    void parallelModeAnalysis(int threadId, int jobId);
    void parallelME(int threadId, int meId);

    /* full analysis for an I-slice CU */
    void compressIntraCU(const TComDataCU& parentCTU, const CU& cuData, x265_intra_data* sdata, uint32_t &zOrder);

    /* full analysis for a P or B slice CU */
    void compressInterCU_dist(const TComDataCU& parentCTU, const CU& cuData);
    void compressInterCU_rd0_4(const TComDataCU& parentCTU, const CU& cuData);
    void compressInterCU_rd5_6(const TComDataCU& parentCTU, const CU& cuData);

    /* measure merge and skip */
    void checkMerge2Nx2N_rd0_4(Mode& skip, Mode& merge, const CU& cuData);
    void checkMerge2Nx2N_rd5_6(Mode& skip, Mode& merge, const CU& cuData);

    /* measure inter options */
    void checkInter_rd0_4(Mode& interMode, const CU& cuData, PartSize partSize);
    void checkInter_rd5_6(Mode& interMode, const CU& cuData, PartSize partSize, bool bMergeOnly);

    /* measure intra options */
    void checkIntra(Mode& intraMode, const CU& cuData, PartSize partSize, uint8_t* sharedModes);
    void checkIntraInInter_rd0_4(Mode& intraMode, const CU& cuData);
    void encodeIntraInInter(Mode& intraMode, const CU& cuData);

    void checkDQP(TComDataCU& cu, const CU& cuData);
    void addSplitFlagCost(Mode& mode, uint32_t depth);
    void checkBestMode(Mode& mode, uint32_t depth);
    uint32_t topSkipMinDepth(const TComDataCU& parentCTU, const CU& cuData);
    bool recursionDepthCheck(const TComDataCU& parentCTU, const CU& cuData, const Mode& bestMode);

    void encodeResidue(const TComDataCU& parentCTU, const CU& cuData);
};

struct ThreadLocalData
{
    Analysis analysis;
    NoiseReduction* nr; // Array of NR structures, one for each frameEncoder

    void destroy()
    {
        analysis.destroy();
        X265_FREE(nr);
    }
};

}

#endif // ifndef X265_ANALYSIS_H
