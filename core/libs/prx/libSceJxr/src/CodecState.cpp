#include "prx/libSceJxr/libSceJxr.hpp"
#include "prx/libc/include/General.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

extern "C" {
std::int32_t JxrAllocateCodingContextDecBridge_nid_no_patch(void* codec, std::int32_t count);
void JxrAdvanceMRPtr_nid_no_patch(void* codec);
void JxrAdvanceOneMBRow_nid_no_patch(void* codec);
std::int32_t JxrAllocateBitIOInfoBridge_nid_no_patch(void* codec);
std::int32_t JxrAllocatePredInfoBridge_nid_no_patch(void* codec);
std::int32_t JxrAllocateTileInfoBridge_nid_no_patch(void* codec);
std::int32_t JxrCheckImageBuffer_nid_no_patch(void* codec, std::size_t width, std::size_t rows);
void JxrFreePredInfoBridge_nid_no_patch(void* codec);
void JxrFreeTileInfoBridge_nid_no_patch(void* codec);
std::int32_t JxrGetDCACPredMode_nid_no_patch(void* codec, std::size_t macroblockX);
void JxrGetTilePos_nid_no_patch(void* codec, std::size_t macroblockX, std::size_t macroblockY);
void JxrInitMRPtr_nid_no_patch(void* codec);
std::int32_t JxrSetBitIOPointers_nid_no_patch(void* codec);
void JxrSetUniformQuantizer_nid_no_patch(void* codec, std::size_t band);
void JxrSwapMRPtr_nid_no_patch(void* codec);
void JxrUpdatePredInfo_nid_no_patch(void* codec, void* info, std::size_t macroblockX, std::int32_t colorFormat);
void JxrUseDCQuantizer_nid_no_patch(void* codec, std::size_t tile);
void JxrUseLPQuantizer_nid_no_patch(void* codec, std::size_t count, std::size_t tile);
void JxrInitZigzagScan_nid_no_patch(void* context);
void JxrResetCodingContext_nid_no_patch(void* context);
}

namespace {

void Check(const void* pointer) {
    if (!pointer) APS5_INVALID_ARG_EX;
}

} // namespace

extern "C" {

std::int32_t APS5_VABI AllocateCodingContextDec_nid_postfix(void* codec, std::int32_t count) {
    return JxrAllocateCodingContextDecBridge_nid_no_patch(codec, count);
}

void APS5_VABI advanceMRPtr_nid_postfix(void* codec) {
    Check(codec);
    JxrAdvanceMRPtr_nid_no_patch(codec);
}

void APS5_VABI advanceOneMBRow_nid_postfix(void* codec) {
    Check(codec);
    JxrAdvanceOneMBRow_nid_no_patch(codec);
}

std::int32_t APS5_VABI allocateBitIOInfo_nid_postfix(void* codec) {
    Check(codec);
    return JxrAllocateBitIOInfoBridge_nid_no_patch(codec);
}

std::int32_t APS5_VABI allocatePredInfo_nid_postfix(void* codec) {
    Check(codec);
    return JxrAllocatePredInfoBridge_nid_no_patch(codec);
}

std::int32_t APS5_VABI allocateTileInfo_nid_postfix(void* codec) {
    Check(codec);
    return JxrAllocateTileInfoBridge_nid_no_patch(codec);
}

std::int32_t APS5_VABI checkImageBuffer_nid_postfix(void* codec, std::uint64_t width, std::uint64_t rows) {
    Check(codec);
    return JxrCheckImageBuffer_nid_no_patch(codec, width, rows);
}

void APS5_VABI freePredInfo_nid_postfix(void* codec) {
    Check(codec);
    JxrFreePredInfoBridge_nid_no_patch(codec);
}

void APS5_VABI freeTileInfo_nid_postfix(void* codec) {
    Check(codec);
    JxrFreeTileInfoBridge_nid_no_patch(codec);
}

std::int32_t APS5_VABI getDCACPredMode_nid_postfix(void* codec, std::uint64_t macroblockX) {
    Check(codec);
    return JxrGetDCACPredMode_nid_no_patch(codec, macroblockX);
}

void APS5_VABI getTilePos_nid_postfix(void* codec, std::uint64_t macroblockX, std::uint64_t macroblockY) {
    Check(codec);
    JxrGetTilePos_nid_no_patch(codec, macroblockX, macroblockY);
}

void APS5_VABI initMRPtr_nid_postfix(void* codec) {
    Check(codec);
    JxrInitMRPtr_nid_no_patch(codec);
}

std::int32_t APS5_VABI setBitIOPointers_nid_postfix(void* codec) {
    Check(codec);
    return JxrSetBitIOPointers_nid_no_patch(codec);
}

void APS5_VABI setUniformQuantizer_nid_postfix(void* codec, std::uint64_t band) {
    Check(codec);
    JxrSetUniformQuantizer_nid_no_patch(codec, band);
}

void APS5_VABI swapMRPtr_nid_postfix(void* codec) {
    Check(codec);
    JxrSwapMRPtr_nid_no_patch(codec);
}

void APS5_VABI updatePredInfo_nid_postfix(void* codec, Jxr::MacroblockInfo* info, std::uint64_t macroblockX, std::int32_t colorFormat) {
    Check(codec);
    JxrUpdatePredInfo_nid_no_patch(codec, info, macroblockX, colorFormat);
}

void APS5_VABI useDCQuantizer_nid_postfix(void* codec, std::uint64_t tile) {
    Check(codec);
    JxrUseDCQuantizer_nid_no_patch(codec, tile);
}

void APS5_VABI useLPQuantizer_nid_postfix(void* codec, std::uint64_t count, std::uint64_t tile) {
    Check(codec);
    JxrUseLPQuantizer_nid_no_patch(codec, count, tile);
}

void APS5_VABI InitZigzagScan_nid_postfix(void* context) {
    JxrInitZigzagScan_nid_no_patch(context);
}

void APS5_VABI ResetCodingContext_nid_postfix(void* context) {
    Check(context);
    JxrResetCodingContext_nid_no_patch(context);
}

}
