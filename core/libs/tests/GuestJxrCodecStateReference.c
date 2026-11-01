#include "strcodec.h"
#include "decode.h"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int32_t APS5_VABI AllocateCodingContextDec_nid_postfix(void* codec, int32_t count);
void APS5_VABI advanceMRPtr_nid_postfix(void* codec);
void APS5_VABI advanceOneMBRow_nid_postfix(void* codec);
int32_t APS5_VABI allocateBitIOInfo_nid_postfix(void* codec);
int32_t APS5_VABI allocatePredInfo_nid_postfix(void* codec);
int32_t APS5_VABI allocateTileInfo_nid_postfix(void* codec);
int32_t APS5_VABI checkImageBuffer_nid_postfix(void* codec, uint64_t width, uint64_t rows);
void APS5_VABI freePredInfo_nid_postfix(void* codec);
void APS5_VABI freeTileInfo_nid_postfix(void* codec);
int32_t APS5_VABI getDCACPredMode_nid_postfix(void* codec, uint64_t x);
void APS5_VABI getTilePos_nid_postfix(void* codec, uint64_t x, uint64_t y);
void APS5_VABI initMRPtr_nid_postfix(void* codec);
int32_t APS5_VABI setBitIOPointers_nid_postfix(void* codec);
void APS5_VABI setUniformQuantizer_nid_postfix(void* codec, uint64_t band);
void APS5_VABI swapMRPtr_nid_postfix(void* codec);
void APS5_VABI updatePredInfo_nid_postfix(void* codec, void* info, uint64_t x, int32_t colorFormat);
void APS5_VABI useDCQuantizer_nid_postfix(void* codec, uint64_t tile);
void APS5_VABI useLPQuantizer_nid_postfix(void* codec, uint64_t count, uint64_t tile);
void APS5_VABI InitZigzagScan_nid_postfix(void* context);
void APS5_VABI ResetCodingContext_nid_postfix(void* context);
int32_t APS5_VABI allocateQuantizer_nid_postfix(void* quantizers, uint64_t channels, uint64_t count);
void* APS5_VABI malloc_nid_postfix(size_t size);
void APS5_VABI free_nid_postfix(void* pointer);

#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (0)

static uint32_t randomState = 12345;

static uint32_t Next(void) {
    randomState ^= randomState << 13;
    randomState ^= randomState >> 17;
    randomState ^= randomState << 5;
    return randomState;
}

static void Setup(CWMImageStrCodec* codec, BITSTREAMFORMAT format, SUBBAND band) {
    memset(codec, 0, sizeof(*codec));
    codec->WMISCP.cNumOfSliceMinus1V = 2;
    codec->WMISCP.uiTileX[0] = 0;
    codec->WMISCP.uiTileX[1] = 3;
    codec->WMISCP.uiTileX[2] = 5;
    codec->WMISCP.cNumOfSliceMinus1H = 1;
    codec->WMISCP.uiTileY[0] = 0;
    codec->WMISCP.uiTileY[1] = 4;
    codec->WMISCP.bfBitstreamFormat = format;
    codec->WMISCP.sbSubband = band;
    codec->cmbWidth = 7;
    codec->cmbHeight = 6;
    codec->m_param.cNumChannels = 3;
    codec->m_param.cfColorFormat = YUV_444;
    codec->m_param.bIndexTable = TRUE;
    codec->m_param.uQPMode = 7;
}

static int SameCodec(const CWMImageStrCodec* left, const CWMImageStrCodec* right) {
    static CWMImageStrCodec a;
    static CWMImageStrCodec b;
    a = *left;
    b = *right;
    a.m_pNextSC = b.m_pNextSC = NULL;
    return memcmp(&a, &b, sizeof(a)) == 0;
}

static int TilePositions(void) {
    static CWMImageStrCodec native;
    static CWMImageStrCodec guest;
    size_t x, y;
    Setup(&native, SPATIAL, SB_ALL);
    guest = native;
    for (y = 0; y < 6; ++y) {
        for (x = 0; x < 7; ++x) {
            getTilePos(&native, x, y);
            getTilePos_nid_postfix(&guest, x, y);
            CHECK(SameCodec(&native, &guest));
        }
    }
    return 0;
}

static int RowPointers(void) {
    static CWMImageStrCodec native[2];
    static CWMImageStrCodec guest[2];
    static PixelI planes[2][MAX_CHANNELS][4096];
    static CWMIPredInfo rows[2][MAX_CHANNELS];
    size_t step, channel, side;
    Setup(&native[0], SPATIAL, SB_ALL);
    for (channel = 0; channel < 3; ++channel) {
        native[0].a0MBbuffer[channel] = planes[0][channel];
        native[0].a1MBbuffer[channel] = planes[1][channel];
        native[0].PredInfo[channel] = &rows[0][channel];
        native[0].PredInfoPrevRow[channel] = &rows[1][channel];
    }
    native[1] = native[0];
    native[1].m_bSecondary = TRUE;
    native[0].m_pNextSC = &native[1];
    guest[0] = native[0];
    guest[1] = native[1];
    guest[0].m_pNextSC = &guest[1];
    initMRPtr(&native[0]);
    initMRPtr_nid_postfix(&guest[0]);
    for (step = 0; step < 40; ++step) {
        switch (Next() % 3) {
        case 0:
            advanceMRPtr(&native[0]);
            advanceMRPtr_nid_postfix(&guest[0]);
            break;
        case 1:
            advanceOneMBRow(&native[0]);
            advanceOneMBRow_nid_postfix(&guest[0]);
            break;
        default:
            swapMRPtr(&native[0]);
            swapMRPtr_nid_postfix(&guest[0]);
            initMRPtr(&native[0]);
            initMRPtr_nid_postfix(&guest[0]);
            break;
        }
        for (side = 0; side < 2; ++side) CHECK(SameCodec(&native[side], &guest[side]));
    }
    return 0;
}

static int ImageBuffers(void) {
    static CWMImageStrCodec native;
    static CWMImageStrCodec guest;
    const COLORFORMAT formats[] = {Y_ONLY, YUV_420, YUV_422, YUV_444, CMYK, NCOMPONENT, CF_RGB};
    const BITDEPTH_BITS depths[] = {BD_1, BD_8, BD_16, BD_32F};
    size_t f, d, round;
    for (round = 0; round < 2000; ++round) {
        Setup(&native, SPATIAL, SB_ALL);
        native.WMISCP.bYUVData = Next() & 1;
        f = Next() % (sizeof(formats) / sizeof(formats[0]));
        d = Next() % (sizeof(depths) / sizeof(depths[0]));
        native.WMII.cfColorFormat = formats[f];
        native.m_param.cfColorFormat = formats[(f + 1) % (sizeof(formats) / sizeof(formats[0]))];
        native.WMII.bdBitDepth = depths[d];
        native.WMII.cBitsPerUnit = 1 + Next() % 128;
        native.WMIBI.cLine = Next() % 40;
        native.WMIBI.cbStride = Next() % 4096;
        guest = native;
        {
            const size_t width = Next() % 600;
            const size_t rows = Next() % 50;
            CHECK(checkImageBuffer(&native, width, rows) == checkImageBuffer_nid_postfix(&guest, width, rows));
        }
    }
    return 0;
}

static int SameQuantizers(CWMIQuantizer* const* left, CWMIQuantizer* const* right) {
    size_t channel;
    for (channel = 0; channel < 3; ++channel) {
        if ((left[channel] == NULL) != (right[channel] == NULL)) return 0;
        if (left[channel] && memcmp(left[channel], right[channel], 16 * sizeof(CWMIQuantizer)) != 0) return 0;
    }
    return 1;
}

static int Tiles(void) {
    static CWMImageStrCodec native;
    static CWMImageStrCodec guest;
    size_t tile, channel, round;
    for (round = 0; round < 8; ++round) {
        const U8 mode = (U8)(round & 7);
        Setup(&native, SPATIAL, round < 4 ? SB_ALL : (SUBBAND)(round % 4));
        native.m_param.uQPMode = mode;
        guest = native;
        CHECK(allocateTileInfo(&native) == ICERR_OK && allocateTileInfo_nid_postfix(&guest) == ICERR_OK);
        CHECK(memcmp(native.pTile, guest.pTile, 3 * sizeof(CWMITile)) == 0);
        for (tile = 0; tile < 3; ++tile) {
            CHECK(allocateQuantizer(native.pTile[tile].pQuantizerDC, 3, 16) == ICERR_OK);
            CHECK(allocateQuantizer(native.pTile[tile].pQuantizerLP, 3, 16) == ICERR_OK);
            CHECK(allocateQuantizer(native.pTile[tile].pQuantizerHP, 3, 16) == ICERR_OK);
            CHECK(allocateQuantizer_nid_postfix(guest.pTile[tile].pQuantizerDC, 3, 16) == ICERR_OK);
            CHECK(allocateQuantizer_nid_postfix(guest.pTile[tile].pQuantizerLP, 3, 16) == ICERR_OK);
            CHECK(allocateQuantizer_nid_postfix(guest.pTile[tile].pQuantizerHP, 3, 16) == ICERR_OK);
            for (channel = 0; channel < 3; ++channel) {
                size_t index;
                for (index = 0; index < 16; ++index) {
                    CWMIQuantizer value;
                    memset(&value, 0, sizeof(value));
                    value.iIndex = (U8)Next();
                    value.iQP = (I32)Next();
                    native.pTile[tile].pQuantizerDC[channel][index] = guest.pTile[tile].pQuantizerDC[channel][index] = value;
                    value.iQP = (I32)Next();
                    native.pTile[tile].pQuantizerLP[channel][index] = guest.pTile[tile].pQuantizerLP[channel][index] = value;
                    value.iQP = (I32)Next();
                    native.pTile[tile].pQuantizerHP[channel][index] = guest.pTile[tile].pQuantizerHP[channel][index] = value;
                }
            }
        }
        useDCQuantizer(&native, 1);
        useDCQuantizer_nid_postfix(&guest, 1);
        useLPQuantizer(&native, 5, 2);
        useLPQuantizer_nid_postfix(&guest, 5, 2);
        for (tile = 0; tile < 3; ++tile) {
            CHECK(SameQuantizers(native.pTile[tile].pQuantizerDC, guest.pTile[tile].pQuantizerDC));
            CHECK(SameQuantizers(native.pTile[tile].pQuantizerLP, guest.pTile[tile].pQuantizerLP));
            CHECK(SameQuantizers(native.pTile[tile].pQuantizerHP, guest.pTile[tile].pQuantizerHP));
        }
        {
            size_t band;
            for (band = 0; band < 3; ++band) {
                if (mode & (1u << band)) continue;
                for (tile = 1; tile < 3; ++tile) {
                    CWMIQuantizer** left = band == 0 ? native.pTile[tile].pQuantizerDC : band == 1 ? native.pTile[tile].pQuantizerLP : native.pTile[tile].pQuantizerHP;
                    CWMIQuantizer** right = band == 0 ? guest.pTile[tile].pQuantizerDC : band == 1 ? guest.pTile[tile].pQuantizerLP : guest.pTile[tile].pQuantizerHP;
                    free(left[0]);
                    free_nid_postfix(right[0]);
                }
                setUniformQuantizer(&native, band);
                setUniformQuantizer_nid_postfix(&guest, band);
                for (tile = 0; tile < 3; ++tile)
                    for (channel = 0; channel < 3; ++channel) {
                        CWMIQuantizer* const* left = band == 0 ? native.pTile[tile].pQuantizerDC : band == 1 ? native.pTile[tile].pQuantizerLP : native.pTile[tile].pQuantizerHP;
                        CWMIQuantizer* const* right = band == 0 ? guest.pTile[tile].pQuantizerDC : band == 1 ? guest.pTile[tile].pQuantizerLP : guest.pTile[tile].pQuantizerHP;
                        CWMIQuantizer* const* leftFirst = band == 0 ? native.pTile[0].pQuantizerDC : band == 1 ? native.pTile[0].pQuantizerLP : native.pTile[0].pQuantizerHP;
                        CWMIQuantizer* const* rightFirst = band == 0 ? guest.pTile[0].pQuantizerDC : band == 1 ? guest.pTile[0].pQuantizerLP : guest.pTile[0].pQuantizerHP;
                        CHECK(left[channel] == leftFirst[channel] && right[channel] == rightFirst[channel]);
                    }
            }
            for (band = 1; band < 3; ++band) {
                const int keptByFree = band == 1 ? native.WMISCP.sbSubband != SB_DC_ONLY
                                                 : native.WMISCP.sbSubband != SB_DC_ONLY && native.WMISCP.sbSubband != SB_NO_HIGHPASS;
                if (keptByFree) continue;
                for (tile = 0; tile < ((mode & (1u << band)) ? 3u : 1u); ++tile) {
                    free(band == 1 ? native.pTile[tile].pQuantizerLP[0] : native.pTile[tile].pQuantizerHP[0]);
                    free_nid_postfix(band == 1 ? guest.pTile[tile].pQuantizerLP[0] : guest.pTile[tile].pQuantizerHP[0]);
                }
            }
        }
        freeTileInfo(&native);
        freeTileInfo_nid_postfix(&guest);
    }
    return 0;
}

static int SamePrediction(const CWMIPredInfo* left, const CWMIPredInfo* right, size_t count) {
    size_t index;
    for (index = 0; index < count; ++index) {
        if (left[index].iQPIndex != right[index].iQPIndex || left[index].iCBP != right[index].iCBP || left[index].iDC != right[index].iDC) return 0;
        if (memcmp(left[index].iAD, right[index].iAD, sizeof(left[index].iAD)) != 0) return 0;
        if (right[index].piAD != right[index].iAD) return 0;
    }
    return 1;
}

static int Prediction(void) {
    static CWMImageStrCodec native;
    static CWMImageStrCodec guest;
    static CWMIMBInfo info;
    const COLORFORMAT formats[] = {Y_ONLY, YUV_420, YUV_422, YUV_444, CMYK, NCOMPONENT};
    size_t round, channel, x;
    for (round = 0; round < 300; ++round) {
        Setup(&native, SPATIAL, SB_ALL);
        native.m_param.cfColorFormat = formats[round % 6];
        native.m_param.cNumChannels = native.m_param.cfColorFormat == Y_ONLY ? 1 : 3;
        guest = native;
        CHECK(allocatePredInfo(&native) == ICERR_OK && allocatePredInfo_nid_postfix(&guest) == ICERR_OK);
        for (channel = 0; channel < native.m_param.cNumChannels; ++channel) {
            CHECK(guest.PredInfo[channel] - guest.pPredInfoMemory == native.PredInfo[channel] - native.pPredInfoMemory);
            CHECK(guest.PredInfoPrevRow[channel] - guest.pPredInfoMemory == native.PredInfoPrevRow[channel] - native.pPredInfoMemory);
            for (x = 0; x < 7; ++x) {
                CWMIPredInfo* sides[2][2] = {{&native.PredInfo[channel][x], &native.PredInfoPrevRow[channel][x]},
                                             {&guest.PredInfo[channel][x], &guest.PredInfoPrevRow[channel][x]}};
                size_t row, k;
                for (row = 0; row < 2; ++row) {
                    const Int qp = (Int)(Next() % 16), cbp = (Int)(Next() % 65536), dc = (Int)(Next() % 2001) - 1000;
                    sides[0][row]->iQPIndex = sides[1][row]->iQPIndex = qp;
                    sides[0][row]->iCBP = sides[1][row]->iCBP = cbp;
                    sides[0][row]->iDC = sides[1][row]->iDC = dc;
                    for (k = 0; k < 6; ++k) sides[0][row]->iAD[k] = sides[1][row]->iAD[k] = (Int)(Next() % 2001) - 1000;
                }
            }
        }
        for (x = 0; x < 7; ++x) {
            size_t k, c;
            getTilePos(&native, x, round % 6);
            getTilePos_nid_postfix(&guest, x, round % 6);
            native.MBInfo.iQIndexLP = guest.MBInfo.iQIndexLP = (U8)(Next() % 16);
            CHECK(getDCACPredMode(&native, x) == getDCACPredMode_nid_postfix(&guest, x));
            for (c = 0; c < MAX_CHANNELS; ++c)
                for (k = 0; k < 16; ++k) info.iBlockDC[c][k] = (I32)(Next() % 2001) - 1000;
            info.iQIndexLP = (U8)(Next() % 16);
            info.iCBP[0] = info.iCBP[1] = info.iCBP[2] = (Int)(Next() % 65536);
            {
                CWMIMBInfo copy = info;
                updatePredInfo(&native, &info, x, native.m_param.cfColorFormat);
                updatePredInfo_nid_postfix(&guest, &copy, x, guest.m_param.cfColorFormat);
            }
        }
        for (channel = 0; channel < native.m_param.cNumChannels; ++channel) {
            CHECK(SamePrediction(native.PredInfo[channel], guest.PredInfo[channel], 7));
            CHECK(SamePrediction(native.PredInfoPrevRow[channel], guest.PredInfoPrevRow[channel], 7));
        }
        advanceOneMBRow(&native);
        advanceOneMBRow_nid_postfix(&guest);
        freePredInfo(&native);
        freePredInfo_nid_postfix(&guest);
        CHECK(guest.pPredInfoMemory == NULL);
    }
    return 0;
}

static intptr_t Relative(const void* pointer, const void* anchor) {
    return pointer ? (intptr_t)((const char*)pointer - (const char*)anchor) : INTPTR_MIN;
}

static int SameHuffman(const CAdaptiveHuffman* left, const void* leftAnchor, const CAdaptiveHuffman* right, const void* rightAnchor) {
    CAdaptiveHuffman a = *left;
    CAdaptiveHuffman b = *right;
    if (Relative(a.m_pTable, leftAnchor) != Relative(b.m_pTable, rightAnchor)) return 0;
    if (Relative(a.m_pDelta, leftAnchor) != Relative(b.m_pDelta, rightAnchor)) return 0;
    if (Relative(a.m_pDelta1, leftAnchor) != Relative(b.m_pDelta1, rightAnchor)) return 0;
    if (Relative(a.m_hufDecTable, leftAnchor) != Relative(b.m_hufDecTable, rightAnchor)) return 0;
    a.m_pTable = b.m_pTable = a.m_pDelta = b.m_pDelta = a.m_pDelta1 = b.m_pDelta1 = NULL;
    a.m_hufDecTable = b.m_hufDecTable = NULL;
    return memcmp(&a, &b, sizeof(a)) == 0;
}

static int SameContext(const CCodingContext* left, const CCodingContext* right) {
    static CCodingContext a;
    static CCodingContext b;
    const void* leftAnchor = left->m_pAdaptHuffCBPCY1->m_pTable;
    const void* rightAnchor = right->m_pAdaptHuffCBPCY1->m_pTable;
    size_t k;
    if ((leftAnchor == NULL) != (rightAnchor == NULL)) return 0;
    if (!SameHuffman(left->m_pAdaptHuffCBPCY, leftAnchor, right->m_pAdaptHuffCBPCY, rightAnchor)) return 0;
    if (!SameHuffman(left->m_pAdaptHuffCBPCY1, leftAnchor, right->m_pAdaptHuffCBPCY1, rightAnchor)) return 0;
    for (k = 0; k < NUMVLCTABLES; ++k)
        if (!SameHuffman(left->m_pAHexpt[k], leftAnchor, right->m_pAHexpt[k], rightAnchor)) return 0;
    a = *left;
    b = *right;
    a.m_pAdaptHuffCBPCY = b.m_pAdaptHuffCBPCY = NULL;
    a.m_pAdaptHuffCBPCY1 = b.m_pAdaptHuffCBPCY1 = NULL;
    for (k = 0; k < NUMVLCTABLES; ++k) a.m_pAHexpt[k] = b.m_pAHexpt[k] = NULL;
    a.m_pIODC = a.m_pIOLP = a.m_pIOAC = a.m_pIOFL = NULL;
    b.m_pIODC = b.m_pIOLP = b.m_pIOAC = b.m_pIOFL = NULL;
    return memcmp(&a, &b, sizeof(a)) == 0;
}

static void FreeGuestContexts(CWMImageStrCodec* codec) {
    Int i, k;
    for (i = 0; i < codec->cNumCodingContext; ++i) {
        free_nid_postfix(codec->m_pCodingContext[i].m_pAdaptHuffCBPCY);
        free_nid_postfix(codec->m_pCodingContext[i].m_pAdaptHuffCBPCY1);
        for (k = 0; k < NUMVLCTABLES; ++k) free_nid_postfix(codec->m_pCodingContext[i].m_pAHexpt[k]);
    }
    free_nid_postfix(codec->m_pCodingContext);
}

static long Index(BitIOInfo* const* table, size_t count, const BitIOInfo* value) {
    size_t i;
    for (i = 0; i < count; ++i)
        if (table[i] == value) return (long)i;
    return -1;
}

static int Contexts(void) {
    static CWMImageStrCodec native;
    static CWMImageStrCodec guest;
    const BITSTREAMFORMAT formats[] = {SPATIAL, FREQUENCY};
    const SUBBAND bands[] = {SB_ALL, SB_NO_FLEXBITS, SB_NO_HIGHPASS, SB_DC_ONLY};
    size_t f, b, index, i;
    Setup(&native, SPATIAL, SB_ALL);
    CHECK(AllocateCodingContextDec_nid_postfix(&native, 0) == ICERR_ERROR);
    CHECK(AllocateCodingContextDec_nid_postfix(&native, MAX_TILES + 1) == ICERR_ERROR);
    CHECK(AllocateCodingContextDec_nid_postfix(NULL, 1) == ICERR_ERROR);
    InitZigzagScan_nid_postfix(NULL);
    for (f = 0; f < 2; ++f) {
        for (b = 0; b < 4; ++b) {
            for (index = 0; index < 2; ++index) {
                Setup(&native, formats[f], bands[b]);
                native.m_param.bIndexTable = index != 0 || formats[f] != SPATIAL;
                native.m_param.cfColorFormat = (COLORFORMAT)((f * 4 + b) % 7);
                if (!native.m_param.bIndexTable) native.WMISCP.cNumOfSliceMinus1V = native.WMISCP.cNumOfSliceMinus1H = 0;
                guest = native;
                CHECK(allocateBitIOInfo(&native) == ICERR_OK && allocateBitIOInfo_nid_postfix(&guest) == ICERR_OK);
                CHECK(native.cSB == guest.cSB && native.cNumBitIO == guest.cNumBitIO);
                for (i = 0; i < guest.cNumBitIO; ++i) {
                    const uintptr_t address = (uintptr_t)guest.m_ppBitIO[i];
                    const uint8_t* bytes = (const uint8_t*)guest.m_ppBitIO[i] - PACKETLENGTH * 2;
                    size_t k;
                    CHECK(address % (PACKETLENGTH * 4) == PACKETLENGTH * 2);
                    CHECK(i == 0 || address - (uintptr_t)guest.m_ppBitIO[i - 1] == PACKETLENGTH * 4);
                    CHECK((const uint8_t*)bytes >= (const uint8_t*)(guest.m_ppBitIO + guest.cNumBitIO));
                    for (k = 0; k < PACKETLENGTH * 2 + sizeof(BitIOInfo); ++k) CHECK(bytes[k] == 0);
                }
                CHECK((guest.cNumBitIO == 0) == (guest.pIndexTable == NULL));
                CHECK(AllocateCodingContextDec(&native, (Int)native.WMISCP.cNumOfSliceMinus1V + 1) == ICERR_OK);
                CHECK(AllocateCodingContextDec_nid_postfix(&guest, (Int)guest.WMISCP.cNumOfSliceMinus1V + 1) == ICERR_OK);
                CHECK(native.cNumCodingContext == guest.cNumCodingContext);
                CHECK(setBitIOPointers(&native) == setBitIOPointers_nid_postfix(&guest));
                for (i = 0; i < (size_t)guest.cNumCodingContext; ++i) {
                    CCodingContext* left = &native.m_pCodingContext[i];
                    CCodingContext* right = &guest.m_pCodingContext[i];
                    CHECK(SameContext(left, right));
                    if (guest.cNumBitIO) {
                        CHECK(Index(native.m_ppBitIO, native.cNumBitIO, left->m_pIODC) == Index(guest.m_ppBitIO, guest.cNumBitIO, right->m_pIODC));
                        CHECK(Index(native.m_ppBitIO, native.cNumBitIO, left->m_pIOLP) == Index(guest.m_ppBitIO, guest.cNumBitIO, right->m_pIOLP));
                        CHECK(Index(native.m_ppBitIO, native.cNumBitIO, left->m_pIOAC) == Index(guest.m_ppBitIO, guest.cNumBitIO, right->m_pIOAC));
                        CHECK(Index(native.m_ppBitIO, native.cNumBitIO, left->m_pIOFL) == Index(guest.m_ppBitIO, guest.cNumBitIO, right->m_pIOFL));
                    } else {
                        CHECK(right->m_pIODC == guest.pIOHeader && right->m_pIOFL == guest.pIOHeader);
                    }
                    left->m_aModelAC.m_iFlcState[0] = right->m_aModelAC.m_iFlcState[0] = (Int)Next();
                    left->m_aScanHoriz[3].uScan = right->m_aScanHoriz[3].uScan = (Int)Next();
                    ResetCodingContext(left);
                    ResetCodingContext_nid_postfix(right);
                    InitZigzagScan(left);
                    InitZigzagScan_nid_postfix(right);
                    CHECK(SameContext(left, right));
                }
                FreeCodingContextDec(&native);
                FreeGuestContexts(&guest);
                free(native.m_ppBitIO);
                free(native.pIndexTable);
                free_nid_postfix(guest.m_ppBitIO);
                free_nid_postfix(guest.pIndexTable);
            }
        }
    }
    return 0;
}

int JxrCodecStateTest_nid_no_patch(void) {
    int result;
    if ((result = TilePositions())) return result;
    if ((result = RowPointers())) return result;
    if ((result = ImageBuffers())) return result;
    if ((result = Tiles())) return result;
    if ((result = Prediction())) return result;
    if ((result = Contexts())) return result;
    return 0;
}
