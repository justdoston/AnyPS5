/* initPostProc, termPostProc, allocateQuantizer, freeQuantizer, FreeDescMetadata,
 * GetImageEncodeIID and GetImageDecodeIID are adapted for AnyPS5 from image/decode/postprocess.c,
 * image/sys/strcodec.c, jxrgluelib/JXRGlueJxr.c and jxrgluelib/JXRGlue.c of jxrlib
 * (https://github.com/4creators/jxrlib/tree/f7521879862b9085318e814c6157490dd9dbbdb4).
 * Altered: they take the guest LP64 layouts, allocate through the guest C library, return
 * the guest interface identifiers, and throw where the original asserts or dereferences null.
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

#include "prx/libSceJxr/libSceJxr.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

extern "C" {
void JxrSlideOneMBRow_nid_no_patch(Jxr::PostProcRows* info, std::size_t channels, std::size_t width, std::int32_t top, std::int32_t bottom);
void JxrUpdatePostProcInfo_nid_no_patch(Jxr::PostProcRows* info, std::int32_t* macroblock, std::size_t x, std::size_t channel);
void JxrPostProcMB_nid_no_patch(Jxr::PostProcRows* info, std::int32_t* first, std::int32_t* second, std::size_t x, std::size_t channel, std::int32_t threshold);
void JxrPostProcBlock_nid_no_patch(Jxr::PostProcRows* info, std::int32_t* first, std::int32_t* second, std::size_t x, std::size_t channel, std::int32_t threshold);
void JxrRemapQP_nid_no_patch(void* quantizer, std::int32_t shift, std::int32_t scaled);
void JxrFormatQuantizer_nid_no_patch(void** quantizers, std::uint8_t mode, std::size_t channels, std::size_t position, std::int32_t shiftedUv, std::int32_t scaled);
std::int32_t JxrACPredMode_nid_no_patch(void* info, std::int32_t colorFormat);
int JxrPixelFormatFromHash_nid_no_patch(std::uint8_t hash, std::uint8_t* guid);
}

namespace {

constexpr std::int32_t IcerrOk = 0;
constexpr std::int32_t IcerrError = -1;
constexpr std::int64_t Success = 0;
constexpr std::int64_t UnsupportedFormat = -106;
constexpr std::int32_t VariantEmpty = 0;
constexpr std::int32_t VariantShort = 18;
constexpr std::int32_t VariantLong = 19;
constexpr std::int32_t VariantString = 30;
constexpr std::int32_t VariantWideString = 31;

const std::uint32_t UnsupportedIid = 100;

const std::array<const JxrGuestGuid*, 84> PixelFormats = {
    &JxrGuest_GUID_PKPixelFormatDontCare_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormatBlackWhite_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat8bppGray_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat16bppRGB555_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat16bppRGB565_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat16bppGray_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat24bppBGR_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat24bppRGB_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat32bppBGR_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat32bppBGRA_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat32bppPBGRA_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat32bppGrayFloat_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat32bppRGB_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat32bppRGBA_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat32bppPRGBA_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat48bppRGBFixedPoint_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat16bppGrayFixedPoint_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat32bppRGB101010_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat48bppRGB_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat64bppRGBA_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat64bppPRGBA_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat96bppRGBFixedPoint_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat96bppRGBFloat_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat128bppRGBAFloat_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat128bppPRGBAFloat_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat128bppRGBFloat_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat32bppCMYK_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat64bppRGBAFixedPoint_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat64bppRGBFixedPoint_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat128bppRGBAFixedPoint_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat128bppRGBFixedPoint_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat64bppRGBAHalf_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat64bppRGBHalf_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat48bppRGBHalf_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat32bppRGBE_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat16bppGrayHalf_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat32bppGrayFixedPoint_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat64bppCMYK_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat24bpp3Channels_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat32bpp4Channels_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat40bpp5Channels_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat48bpp6Channels_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat56bpp7Channels_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat64bpp8Channels_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat48bpp3Channels_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat64bpp4Channels_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat80bpp5Channels_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat96bpp6Channels_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat112bpp7Channels_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat128bpp8Channels_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat40bppCMYKAlpha_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat80bppCMYKAlpha_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat32bpp3ChannelsAlpha_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat40bpp4ChannelsAlpha_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat48bpp5ChannelsAlpha_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat56bpp6ChannelsAlpha_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat64bpp7ChannelsAlpha_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat72bpp8ChannelsAlpha_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat64bpp3ChannelsAlpha_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat80bpp4ChannelsAlpha_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat96bpp5ChannelsAlpha_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat112bpp6ChannelsAlpha_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat128bpp7ChannelsAlpha_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat144bpp8ChannelsAlpha_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat12bppYCC420_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat16bppYCC422_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat20bppYCC422_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat32bppYCC422_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat24bppYCC444_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat30bppYCC444_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat48bppYCC444_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat16bpp48bppYCC444FixedPoint_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat20bppYCC420Alpha_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat24bppYCC422Alpha_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat30bppYCC422Alpha_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat48bppYCC422Alpha_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat32bppYCC444Alpha_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat40bppYCC444Alpha_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat64bppYCC444Alpha_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat64bppYCC444AlphaFixedPoint_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat32bppCMYKDIRECT_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat64bppCMYKDIRECT_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat40bppCMYKDIRECTAlpha_nid_no_patch,
    &JxrGuest_GUID_PKPixelFormat80bppCMYKDIRECTAlpha_nid_no_patch
};

struct IidInfo {
    const char* extension;
    const std::uint32_t* encode;
    const std::uint32_t* decode;
};

const IidInfo* FindIid(const char* extension) {
    if (!extension) APS5_INVALID_ARG_EX;
    static const std::array<IidInfo, 3> table = {{
        {".jxr", &JxrGuest_IID_PKImageWmpEncode_nid_no_patch, &JxrGuest_IID_PKImageWmpDecode_nid_no_patch},
        {".wdp", &UnsupportedIid, &JxrGuest_IID_PKImageWmpDecode_nid_no_patch},
        {".hdp", &UnsupportedIid, &JxrGuest_IID_PKImageWmpDecode_nid_no_patch},
    }};
    for (const IidInfo& info : table) {
        const char* left = extension;
        const char* right = info.extension;
        std::size_t count = std::strlen(info.extension);
        while (count && *left && *right &&
               std::tolower(static_cast<unsigned char>(*left)) == std::tolower(static_cast<unsigned char>(*right))) {
            ++left;
            ++right;
            --count;
        }
        if (count == 0) return &info;
    }
    return nullptr;
}

void Check(const void* pointer) {
    if (!pointer) APS5_INVALID_ARG_EX;
}

} // namespace

extern "C" {

std::int32_t APS5_VABI IDPEmpty_nid_postfix(void*) {
    return IcerrOk;
}

std::int64_t APS5_VABI GetImageEncodeIID_nid_postfix(const char* extension, const std::uint32_t** iid) {
    const IidInfo* info = FindIid(extension);
    if (!info) return UnsupportedFormat;
    Check(iid);
    *iid = info->encode;
    return Success;
}

std::int64_t APS5_VABI GetImageDecodeIID_nid_postfix(const char* extension, const std::uint32_t** iid) {
    const IidInfo* info = FindIid(extension);
    if (!info) return UnsupportedFormat;
    Check(iid);
    *iid = info->decode;
    return Success;
}

const JxrGuestGuid* APS5_VABI GetPixelFormatFromHash_nid_postfix(std::uint8_t hash) {
    std::uint8_t guid[16];
    if (!JxrPixelFormatFromHash_nid_no_patch(hash, guid)) return nullptr;
    for (const JxrGuestGuid* format : PixelFormats)
        if (std::memcmp(format, guid, sizeof(guid)) == 0) return format;
    throw std::runtime_error("GetPixelFormatFromHash: jxrlib returned a pixel format the module does not export");
}

void APS5_VABI FreeDescMetadata_nid_postfix(Jxr::PropVariant* value) {
    Check(value);
    switch (value->type) {
    case VariantString:
        PKFree_nid_postfix(reinterpret_cast<void**>(&value->value.string));
        break;
    case VariantWideString:
        PKFree_nid_postfix(reinterpret_cast<void**>(&value->value.wideString));
        break;
    case VariantEmpty:
    case VariantShort:
    case VariantLong:
        break;
    default:
        throw std::runtime_error("FreeDescMetadata: variant type " + std::to_string(value->type) + " is not handled by jxrlib");
    }
}

std::int32_t APS5_VABI initPostProc_nid_postfix(Jxr::PostProcRows* info, std::uint64_t macroblockWidth, std::uint64_t channels) {
    Check(info);
    for (std::uint64_t channel = 0; channel < channels; ++channel) {
        for (std::size_t row = 0; row < 2; ++row) {
            if ((((macroblockWidth + 2) >> 16) * sizeof(Jxr::PostProcInfo)) & 0xFFFF0000u) return IcerrError;
            auto* block = static_cast<Jxr::PostProcInfo*>(malloc_nid_postfix((macroblockWidth + 2) * sizeof(Jxr::PostProcInfo)));
            info[channel][row] = block;
            if (!block) return IcerrError;
            Jxr::PostProcInfo* entries = block + 1;
            info[channel][row] = entries;
            entries[-1].macroblockTexture = 3;
            for (auto& line : entries[-1].blockTexture)
                for (auto& texture : line) texture = 3;
            std::memcpy(&entries[macroblockWidth], &entries[-1], sizeof(Jxr::PostProcInfo));
        }
    }
    return IcerrOk;
}

void APS5_VABI termPostProc_nid_postfix(Jxr::PostProcRows* info, std::uint64_t channels) {
    if (channels) Check(info);
    for (std::uint64_t channel = 0; channel < channels; ++channel)
        for (std::size_t row = 0; row < 2; ++row)
            if (info[channel][row]) free_nid_postfix(info[channel][row] - 1);
}

void APS5_VABI slideOneMBRow_nid_postfix(Jxr::PostProcRows* info, std::uint64_t channels, std::uint64_t macroblockWidth, std::int32_t top, std::int32_t bottom) {
    Check(info);
    JxrSlideOneMBRow_nid_no_patch(info, channels, macroblockWidth, top, bottom);
}

void APS5_VABI updatePostProcInfo_nid_postfix(Jxr::PostProcRows* info, std::int32_t* macroblock, std::uint64_t macroblockX, std::uint64_t channel) {
    Check(info);
    Check(macroblock);
    JxrUpdatePostProcInfo_nid_no_patch(info, macroblock, macroblockX, channel);
}

void APS5_VABI postProcMB_nid_postfix(Jxr::PostProcRows* info, std::int32_t* first, std::int32_t* second, std::uint64_t macroblockX, std::uint64_t channel, std::int32_t threshold) {
    Check(info);
    Check(first);
    Check(second);
    JxrPostProcMB_nid_no_patch(info, first, second, macroblockX, channel, threshold);
}

void APS5_VABI postProcBlock_nid_postfix(Jxr::PostProcRows* info, std::int32_t* first, std::int32_t* second, std::uint64_t macroblockX, std::uint64_t channel, std::int32_t threshold) {
    Check(info);
    Check(first);
    Check(second);
    JxrPostProcBlock_nid_no_patch(info, first, second, macroblockX, channel, threshold);
}

std::int32_t APS5_VABI allocateQuantizer_nid_postfix(Jxr::Quantizer** quantizers, std::uint64_t channels, std::uint64_t count) {
    if (count > 16 || channels > Jxr::MaxChannels) return IcerrError;
    Check(quantizers);
    quantizers[0] = static_cast<Jxr::Quantizer*>(malloc_nid_postfix(count * sizeof(Jxr::Quantizer) * channels));
    if (!quantizers[0]) return IcerrError;
    for (std::uint64_t channel = 1; channel < channels; ++channel) quantizers[channel] = quantizers[channel - 1] + count;
    return IcerrOk;
}

void APS5_VABI freeQuantizer_nid_postfix(Jxr::Quantizer** quantizers) {
    Check(quantizers);
    if (quantizers[0]) free_nid_postfix(quantizers[0]);
}

void APS5_VABI formatQuantizer_nid_postfix(Jxr::Quantizer** quantizers, std::uint8_t mode, std::uint64_t channels, std::uint64_t position, std::int32_t shiftedUv, std::int32_t scaledArithmetic) {
    if (channels) Check(quantizers);
    JxrFormatQuantizer_nid_no_patch(reinterpret_cast<void**>(quantizers), mode, channels, position, shiftedUv, scaledArithmetic);
}

void APS5_VABI remapQP_nid_postfix(Jxr::Quantizer* quantizer, std::int32_t shift, std::int32_t scaledArithmetic) {
    Check(quantizer);
    JxrRemapQP_nid_no_patch(quantizer, shift, scaledArithmetic);
}

std::int32_t APS5_VABI getACPredMode_nid_postfix(Jxr::MacroblockInfo* info, std::int32_t colorFormat) {
    Check(info);
    return JxrACPredMode_nid_no_patch(info, colorFormat);
}

}
