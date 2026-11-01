/* JxrAllocateBitIOInfo, JxrAllocateTileInfo, JxrFreeTileInfo, JxrAllocatePredInfo, JxrFreePredInfo
 * and JxrAllocateCodingContextDec are adapted for AnyPS5 from allocateBitIOInfo, allocateTileInfo,
 * freeTileInfo and freeQuantizer in image/sys/strcodec.c, allocatePredInfo and freePredInfo in
 * image/sys/strPredQuant.c, AllocateCodingContextDec and InitializeAH in image/decode/decode.c and
 * Allocate in image/sys/adapthuff.c of jxrlib
 * (https://github.com/4creators/jxrlib/tree/f7521879862b9085318e814c6157490dd9dbbdb4).
 * Altered: they allocate and free through the guest C library.
 */

//*@@@+++@@@@******************************************************************
//
// Copyright (c) Microsoft Corp.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//*@@@---@@@@******************************************************************

#include "strcodec.h"
#include "decode.h"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

_Static_assert(sizeof(CWMImageStrCodec) == 35944, "codec size");
_Static_assert(offsetof(CWMImageStrCodec, m_param) == 34208 && offsetof(CWMImageStrCodec, pIOHeader) == 34376, "codec parameter offsets");
_Static_assert(offsetof(CWMImageStrCodec, pTile) == 34448 && offsetof(CWMImageStrCodec, m_pCodingContext) == 34480, "codec tile offsets");
_Static_assert(offsetof(CWMImageStrCodec, PredInfo) == 35392 && offsetof(CWMImageStrCodec, m_pNextSC) == 35672, "codec prediction offsets");
_Static_assert(offsetof(CWMImageStrCodec, m_bSecondary) == 35680 && offsetof(CWMImageStrCodec, pPostProcInfo) == 35688, "codec tail offsets");
_Static_assert(sizeof(CCodingContext) == 704 && sizeof(CAdaptiveHuffman) == 72, "context sizes");
_Static_assert(sizeof(CWMITile) == 432 && sizeof(CWMIPredInfo) == 48 && sizeof(BitIOInfo) == 48, "record sizes");

void* APS5_VABI malloc_nid_postfix(size_t size);
void APS5_VABI free_nid_postfix(void* pointer);

Int JxrAllocateBitIOInfo_nid_no_patch(CWMImageStrCodec* pSC)
{
    U32 cNumBitIO;
    SUBBAND sbSubband = pSC->WMISCP.sbSubband;

    pSC->cSB = (sbSubband == SB_DC_ONLY ? 1 : (sbSubband == SB_NO_HIGHPASS ? 2 : (sbSubband == SB_NO_FLEXBITS ? 3 : 4)));

    if (!pSC->m_param.bIndexTable) {
        cNumBitIO = 0;
    }
    else if(pSC->WMISCP.bfBitstreamFormat == SPATIAL)
        cNumBitIO = pSC->WMISCP.cNumOfSliceMinus1V + 1;
    else
        cNumBitIO = (pSC->WMISCP.cNumOfSliceMinus1V + 1) * pSC->cSB;

    if(cNumBitIO > MAX_TILES * 4)
        return ICERR_ERROR;

    if(cNumBitIO > 0){
        U32 i = 0;
        size_t cb = sizeof(BitIOInfo) * cNumBitIO + (PACKETLENGTH * 4 - 1) + PACKETLENGTH * 4 * cNumBitIO;
        U8* pb = (U8*)malloc_nid_postfix(cb);

        if (NULL == pb) return ICERR_ERROR;
        memset(pb, 0, cb);

        pSC->m_ppBitIO = (BitIOInfo**)pb;
        pb += sizeof(BitIOInfo) * cNumBitIO;

        pb = (U8*)ALIGNUP(pb, PACKETLENGTH * 4) + PACKETLENGTH * 2;
        for (i = 0; i < cNumBitIO; ++i){
            pSC->m_ppBitIO[i] = (BitIOInfo*)pb;
            pb += PACKETLENGTH * 4;
        }

        if(cNumBitIO > MAX_TILES * 4 || pSC->WMISCP.cNumOfSliceMinus1H >= MAX_TILES)
            return ICERR_ERROR;
        pSC->pIndexTable = malloc_nid_postfix(cNumBitIO * (pSC->WMISCP.cNumOfSliceMinus1H + 1) * sizeof(size_t));
        if(NULL == pSC->pIndexTable) return ICERR_ERROR;
    }

    pSC->cNumBitIO = cNumBitIO;

    return ICERR_OK;
}

Int JxrAllocateTileInfo_nid_no_patch(CWMImageStrCodec * pSC)
{
    size_t i;

    if(pSC->WMISCP.cNumOfSliceMinus1V >= MAX_TILES)
        return ICERR_ERROR;

    pSC->pTile = (CWMITile *)malloc_nid_postfix((pSC->WMISCP.cNumOfSliceMinus1V + 1) * sizeof(CWMITile));
    if(pSC->pTile == NULL)
        return ICERR_ERROR;
    memset(pSC->pTile, 0, (pSC->WMISCP.cNumOfSliceMinus1V + 1) * sizeof(CWMITile));

    for(i = 0; i <= pSC->WMISCP.cNumOfSliceMinus1V; i ++)
        pSC->pTile[i].cNumQPHP = pSC->pTile[i].cNumQPLP = 1, pSC->pTile[i].cBitsHP = pSC->pTile[i].cBitsLP = 0;

    return ICERR_OK;
}

static Void FreeQuantizer(CWMIQuantizer * pQuantizer[MAX_CHANNELS])
{
    if(pQuantizer[0] != NULL)
        free_nid_postfix(pQuantizer[0]);
}

Void JxrFreeTileInfo_nid_no_patch(CWMImageStrCodec * pSC)
{
    size_t iTile;

    if((pSC->m_param.uQPMode & 1) != 0)
        for(iTile = 0; iTile <= pSC->WMISCP.cNumOfSliceMinus1V; iTile ++)
            FreeQuantizer(pSC->pTile[iTile].pQuantizerDC);
    else
        FreeQuantizer(pSC->pTile[0].pQuantizerDC);

    if(pSC->WMISCP.sbSubband != SB_DC_ONLY) {
        if((pSC->m_param.uQPMode & 2) != 0)
            for(iTile = 0; iTile <= pSC->WMISCP.cNumOfSliceMinus1V; iTile ++)
                FreeQuantizer(pSC->pTile[iTile].pQuantizerLP);
        else
            FreeQuantizer(pSC->pTile[0].pQuantizerLP);
    }

    if(pSC->WMISCP.sbSubband != SB_DC_ONLY && pSC->WMISCP.sbSubband != SB_NO_HIGHPASS) {
        if((pSC->m_param.uQPMode & 4) != 0)
            for(iTile = 0; iTile <= pSC->WMISCP.cNumOfSliceMinus1V; iTile ++)
                FreeQuantizer(pSC->pTile[iTile].pQuantizerHP);
        else
            FreeQuantizer(pSC->pTile[0].pQuantizerHP);
    }

    if(pSC->pTile != NULL)
        free_nid_postfix(pSC->pTile);
}

Int JxrAllocatePredInfo_nid_no_patch(CWMImageStrCodec *pSC)
{
    size_t i, j;
    const size_t mbWidth = pSC->cmbWidth;
    const size_t iChannels = pSC->m_param.cNumChannels;
    CWMIPredInfo* pMemory;

    pMemory = (CWMIPredInfo *)malloc_nid_postfix(mbWidth * iChannels * 2 * sizeof(CWMIPredInfo));
    if (pMemory == NULL)
        return ICERR_ERROR;

    pSC->pPredInfoMemory = pMemory;
    for(i = 0; i < iChannels; i ++){
        pSC->PredInfo[i] = pMemory;
        pMemory += mbWidth;
        pSC->PredInfoPrevRow[i] = pMemory;
        pMemory += mbWidth;

        for(j = 0; j < mbWidth; j ++){
            pSC->PredInfo[i][j].piAD = pSC->PredInfo[i][j].iAD;
            pSC->PredInfoPrevRow[i][j].piAD = pSC->PredInfoPrevRow[i][j].iAD;
        }
    }

    return ICERR_OK;
}

Void JxrFreePredInfo_nid_no_patch(CWMImageStrCodec *pSC)
{
    if (pSC->pPredInfoMemory)
        free_nid_postfix(pSC->pPredInfoMemory);
    pSC->pPredInfoMemory = NULL;
}

static Int InitializeAH(CAdaptiveHuffman **ppAdHuff, Int iSym)
{
    CAdaptiveHuffman *pAdHuff = (CAdaptiveHuffman *) malloc_nid_postfix(sizeof (CAdaptiveHuffman));

    if (pAdHuff != NULL && (iSym > 255 || iSym <= 0)) {
        free_nid_postfix(pAdHuff);
        pAdHuff = NULL;
    }
    if (pAdHuff == NULL) {
        *ppAdHuff = NULL;
        printf("Insufficient memory to init decoder.\n");
        return ICERR_ERROR;
    }

    memset (pAdHuff, 0, sizeof (CAdaptiveHuffman));
    pAdHuff->m_iNSymbols = iSym;
    pAdHuff->m_pDelta = NULL;
    pAdHuff->m_iDiscriminant = pAdHuff->m_iUpperBound = pAdHuff->m_iLowerBound = 0;

    *ppAdHuff = pAdHuff;
    return ICERR_OK;
}

Int JxrAllocateCodingContextDec_nid_no_patch(CWMImageStrCodec *pSC, Int iNumContexts)
{
    Int i, iCBPSize, k;
    static const Int aAlphabet[] = {5,4,8,7,7,  12,6,6,12,6,6,7,7,  12,6,6,12,6,6,7,7};

    if (iNumContexts > MAX_TILES || iNumContexts < 1)
        return ICERR_ERROR;

    if (pSC == NULL)
        return ICERR_ERROR;

    pSC->m_pCodingContext = malloc_nid_postfix (iNumContexts * sizeof (CCodingContext));
    if (pSC->m_pCodingContext == NULL) {
        pSC->cNumCodingContext = 0;
        return ICERR_ERROR;
    }
    memset (pSC->m_pCodingContext, 0, iNumContexts * sizeof (CCodingContext));

    pSC->cNumCodingContext = iNumContexts;
    iCBPSize = (pSC->m_param.cfColorFormat == Y_ONLY || pSC->m_param.cfColorFormat == NCOMPONENT
        || pSC->m_param.cfColorFormat == CMYK) ? 5 : 9;

    for (i = 0; i < iNumContexts; i++) {
        CCodingContext *pContext = &(pSC->m_pCodingContext[i]);

        if (InitializeAH(&pContext->m_pAdaptHuffCBPCY, iCBPSize) != ICERR_OK) {
            return ICERR_ERROR;
        }
        if (InitializeAH(&pContext->m_pAdaptHuffCBPCY1, 5) != ICERR_OK) {
            return ICERR_ERROR;
        }

        for(k = 0; k < NUMVLCTABLES; k ++){
            if (InitializeAH(&pContext->m_pAHexpt[k], aAlphabet[k]) != ICERR_OK) {
                return ICERR_ERROR;
            }
        }

        ResetCodingContextDec(pContext);
    }

    return ICERR_OK;
}

Int JxrCheckImageBuffer_nid_no_patch(void* codec, size_t width, size_t rows) { return checkImageBuffer((CWMImageStrCodec*)codec, width, rows); }
Void JxrGetTilePos_nid_no_patch(void* codec, size_t x, size_t y) { getTilePos((CWMImageStrCodec*)codec, x, y); }
Void JxrInitMRPtr_nid_no_patch(void* codec) { initMRPtr((CWMImageStrCodec*)codec); }
Void JxrAdvanceMRPtr_nid_no_patch(void* codec) { advanceMRPtr((CWMImageStrCodec*)codec); }
Void JxrAdvanceOneMBRow_nid_no_patch(void* codec) { advanceOneMBRow((CWMImageStrCodec*)codec); }
Void JxrSwapMRPtr_nid_no_patch(void* codec) { swapMRPtr((CWMImageStrCodec*)codec); }
Int JxrSetBitIOPointers_nid_no_patch(void* codec) { return setBitIOPointers((CWMImageStrCodec*)codec); }
Void JxrSetUniformQuantizer_nid_no_patch(void* codec, size_t band) { setUniformQuantizer((CWMImageStrCodec*)codec, band); }
Void JxrUseDCQuantizer_nid_no_patch(void* codec, size_t tile) { useDCQuantizer((CWMImageStrCodec*)codec, tile); }
Void JxrUseLPQuantizer_nid_no_patch(void* codec, size_t count, size_t tile) { useLPQuantizer((CWMImageStrCodec*)codec, count, tile); }
Int JxrGetDCACPredMode_nid_no_patch(void* codec, size_t x) { return getDCACPredMode((CWMImageStrCodec*)codec, x); }
Void JxrUpdatePredInfo_nid_no_patch(void* codec, void* info, size_t x, int32_t colorFormat) { updatePredInfo((CWMImageStrCodec*)codec, (CWMIMBInfo*)info, x, (COLORFORMAT)colorFormat); }
Void JxrInitZigzagScan_nid_no_patch(void* context) { InitZigzagScan((CCodingContext*)context); }
Void JxrResetCodingContext_nid_no_patch(void* context) { ResetCodingContext((CCodingContext*)context); }

Int JxrAllocateBitIOInfoBridge_nid_no_patch(void* codec) { return JxrAllocateBitIOInfo_nid_no_patch((CWMImageStrCodec*)codec); }
Int JxrAllocateTileInfoBridge_nid_no_patch(void* codec) { return JxrAllocateTileInfo_nid_no_patch((CWMImageStrCodec*)codec); }
Void JxrFreeTileInfoBridge_nid_no_patch(void* codec) { JxrFreeTileInfo_nid_no_patch((CWMImageStrCodec*)codec); }
Int JxrAllocatePredInfoBridge_nid_no_patch(void* codec) { return JxrAllocatePredInfo_nid_no_patch((CWMImageStrCodec*)codec); }
Void JxrFreePredInfoBridge_nid_no_patch(void* codec) { JxrFreePredInfo_nid_no_patch((CWMImageStrCodec*)codec); }
Int JxrAllocateCodingContextDecBridge_nid_no_patch(void* codec, int32_t count) { return JxrAllocateCodingContextDec_nid_no_patch((CWMImageStrCodec*)codec, count); }
