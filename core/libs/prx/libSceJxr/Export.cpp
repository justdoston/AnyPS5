#include "prx/libc/include/Api.hpp"
#include "prx/libSceJxr/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "Stream.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {
constexpr std::int64_t WmpErrSuccess = 0;
}

extern "C" {

APS5_DUMMY_FUN
std::int64_t APS5_VABI PKFree_nid_postfix(void** memory) {
    if (memory != nullptr) {
        free_nid_postfix(*memory);

        *memory = nullptr;
    }
    return WmpErrSuccess;
}

std::int64_t APS5_VABI readIS_nid_postfix(void*, Jxr::BitIo* io) {
    if (!io) APS5_INVALID_ARG_EX;
    constexpr std::uintptr_t packetSize = 4096;
    const auto start = reinterpret_cast<std::uintptr_t>(io->start);
    if (((start ^ reinterpret_cast<std::uintptr_t>(io->current)) & packetSize) == 0) return WmpErrSuccess;
    auto* stream = io->stream;
    if (!stream || !stream->setPosition) APS5_INVALID_ARG_EX;
    stream->setPosition(stream, io->referenceOffset);
    if (!stream->read) APS5_INVALID_ARG_EX;
    {
        GuestArena::HostWrite access(io->start, packetSize);
        if (!access.Open()) APS5_INVALID_ARG_EX;
        stream->read(stream, io->start, packetSize);
    }
    io->referenceOffset += packetSize;
    if (!io->start) APS5_INVALID_ARG_EX;
    std::memcpy(&io->shadow, io->start, sizeof(io->shadow));
    const auto mask = static_cast<std::uintptr_t>(static_cast<std::intptr_t>(io->mask));
    io->start = reinterpret_cast<std::uint8_t*>((reinterpret_cast<std::uintptr_t>(io->start) + packetSize) & mask);
    return WmpErrSuccess;
}

std::int64_t APS5_VABI writeIS_nid_postfix(void*, Jxr::BitIo* io) {
    if (!io) APS5_INVALID_ARG_EX;
    constexpr std::uintptr_t packetSize = 4096;
    const auto start = reinterpret_cast<std::uintptr_t>(io->start);
    if (((start ^ reinterpret_cast<std::uintptr_t>(io->current)) & packetSize) == 0) return WmpErrSuccess;
    if (!io->stream || !io->stream->write) APS5_INVALID_ARG_EX;
    const auto result = io->stream->write(io->stream, io->start, packetSize);
    if (result < 0) return result;
    if (!io->start) APS5_INVALID_ARG_EX;
    const auto mask = static_cast<std::uintptr_t>(static_cast<std::intptr_t>(io->mask));
    io->start = reinterpret_cast<std::uint8_t*>((reinterpret_cast<std::uintptr_t>(io->start) + packetSize) & mask);
    return result;
}

int APS5_VABI PKCodecFactory_CreateCodec_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKCodecFactory_CreateDecoderFromFile_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKCodecFactory_CreateFormatConverter_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKCreateCodecFactory_Release_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKCreateCodecFactory_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKCreateFactory_CreateStream_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKCreateFactory_Release_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKCreateFactory_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKFormatConverter_Convert_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKFormatConverter_Copy_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKFormatConverter_EnumConversions_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKFormatConverter_GetPixelFormat_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKFormatConverter_GetResolution_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKFormatConverter_GetSize_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKFormatConverter_GetSourcePixelFormat_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKFormatConverter_InitializeConvert_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKFormatConverter_Initialize_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKFormatConverter_Release_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageDecode_Copy_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageDecode_Create_WMP_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageDecode_Create_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageDecode_GetColorContext_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageDecode_GetDescriptiveMetadata_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageDecode_GetFrameCount_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageDecode_GetPixelFormat_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageDecode_GetResolution_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageDecode_GetSize_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageDecode_Initialize_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageDecode_Release_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageDecode_SelectFrame_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageEncode_CreateNewFrame_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageEncode_Create_WMP_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageEncode_Create_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageEncode_Initialize_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageEncode_Release_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageEncode_SetColorContext_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageEncode_SetDescriptiveMetadata_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageEncode_SetEXIFMetadata_WMP_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageEncode_SetGPSInfoMetadata_WMP_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageEncode_SetIPTCNAAMetadata_WMP_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageEncode_SetPhotoshopMetadata_WMP_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageEncode_SetPixelFormat_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageEncode_SetResolution_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageEncode_SetSize_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageEncode_SetXMPMetadata_WMP_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageEncode_Terminate_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageEncode_Transcode_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageEncode_WritePixels_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKImageEncode_WriteSource_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

}
