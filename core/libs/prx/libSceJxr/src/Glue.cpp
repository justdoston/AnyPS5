/* The generic PKImageDecode, PKImageEncode, PKFormatConverter, PKFactory and PKCodecFactory
 * functions are adapted for AnyPS5 from jxrgluelib/JXRGlue.c and jxrgluelib/JXRGluePFC.c of jxrlib
 * (https://github.com/4creators/jxrlib/tree/f7521879862b9085318e814c6157490dd9dbbdb4).
 * Altered: they take the guest LP64 object layouts, fill their method tables with the module's
 * own exports, allocate and open files through the guest C library, and throw where the
 * original dereferences a null pointer.
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
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

extern "C" std::int64_t JxrEnumConversions_nid_no_patch(const std::uint8_t* source, std::uint32_t index, std::uint8_t* target);

namespace {

using Jxr::CodecFactory;
using Jxr::Factory;
using Jxr::FormatConverter;
using Jxr::ImageDecode;
using Jxr::ImageEncode;
using Jxr::PkStream;
using Jxr::Rect;
using Jxr::Stream;

constexpr std::int64_t Success = 0;
constexpr std::int64_t NotYetImplemented = -2;
constexpr std::int64_t AbstractMethod = -3;
constexpr std::int64_t FileIo = -102;

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

template<typename T>
T& Require(T* pointer) {
    if (!pointer) APS5_INVALID_ARG_EX;
    return *pointer;
}

template<typename T>
T Method(T method) {
    if (!method) APS5_INVALID_ARG_EX;
    return method;
}

const JxrGuestGuid* Exported(const std::uint8_t* guid) {
    for (const JxrGuestGuid* format : PixelFormats)
        if (std::memcmp(format, guid, sizeof(JxrGuestGuid)) == 0) return format;
    throw std::runtime_error("PKFormatConverter_EnumConversions: jxrlib returned a pixel format the module does not export");
}

std::int64_t APS5_VABI WritePixelsBandedBegin_nid_no_patch(ImageEncode*, Stream*) {
    return AbstractMethod;
}

std::int64_t APS5_VABI WritePixelsBanded_nid_no_patch(ImageEncode*, std::uint32_t, std::uint8_t*, std::uint32_t, std::int32_t) {
    return AbstractMethod;
}

std::int64_t APS5_VABI WritePixelsBandedEnd_nid_no_patch(ImageEncode*) {
    return AbstractMethod;
}

std::int64_t APS5_VABI CloseWsFile_nid_no_patch(Stream** stream) {
    Stream& self = Require(Require(stream));
    fclose_nid_postfix(reinterpret_cast<FileStream*>(self.state[0]));
    return WMPFree_nid_postfix(reinterpret_cast<void**>(stream));
}

std::int64_t APS5_VABI CreateWsFile_nid_no_patch(Stream** stream, const char* filename, const char* mode) {
    const std::int64_t result = WMPAlloc_nid_postfix(reinterpret_cast<void**>(stream), sizeof(Stream));
    if (result < 0) return result;
    Stream& self = **stream;
    self.close = CloseWsFile_nid_no_patch;
    self.end = EOSWS_File_nid_postfix;
    self.read = ReadWS_File_nid_postfix;
    self.write = WriteWS_File_nid_postfix;
    self.setPosition = SetPosWS_File_nid_postfix;
    self.getPosition = GetPosWS_File_nid_postfix;
    FileStream* file = fopen_nid_postfix(filename, mode);
    self.state[0] = reinterpret_cast<std::uintptr_t>(file);
    return file ? Success : FileIo;
}

} // namespace

extern "C" {

std::int64_t APS5_VABI PKImageDecode_Initialize_nid_postfix(ImageDecode* decoder, Stream* stream) {
    ImageDecode& self = Require(decoder);
    self.stream = stream;
    self.pixelFormat = JxrGuest_GUID_PKPixelFormatDontCare_nid_no_patch;
    self.resolutionX = 96;
    self.resolutionY = 96;
    self.frameCount = 1;
    Stream& source = Require(stream);
    if (Method(source.getPosition)(&source, &self.startOffset) < 0) return Success;
    std::memset(self.codecState, 0, 88);
    return Success;
}

std::int64_t APS5_VABI PKImageDecode_GetPixelFormat_nid_postfix(ImageDecode* decoder, JxrGuestGuid* format) {
    Require(format) = Require(decoder).pixelFormat;
    return Success;
}

std::int64_t APS5_VABI PKImageDecode_GetSize_nid_postfix(ImageDecode* decoder, std::int32_t* width, std::int32_t* height) {
    ImageDecode& self = Require(decoder);
    Require(width) = static_cast<std::int32_t>(self.width);
    Require(height) = static_cast<std::int32_t>(self.height);
    return Success;
}

std::int64_t APS5_VABI PKImageDecode_GetResolution_nid_postfix(ImageDecode* decoder, float* resolutionX, float* resolutionY) {
    ImageDecode& self = Require(decoder);
    Require(resolutionX) = self.resolutionX;
    Require(resolutionY) = self.resolutionY;
    return Success;
}

std::int64_t APS5_VABI PKImageDecode_GetColorContext_nid_postfix(ImageDecode*, std::uint8_t*, std::uint32_t*) {
    return NotYetImplemented;
}

std::int64_t APS5_VABI PKImageDecode_GetDescriptiveMetadata_nid_postfix(ImageDecode*, void*) {
    return NotYetImplemented;
}

std::int64_t APS5_VABI PKImageDecode_Copy_nid_postfix(ImageDecode*, const Rect*, std::uint8_t*, std::uint32_t) {
    return AbstractMethod;
}

std::int64_t APS5_VABI PKImageDecode_GetFrameCount_nid_postfix(ImageDecode* decoder, std::uint32_t* count) {
    Require(count) = Require(decoder).frameCount;
    return Success;
}

std::int64_t APS5_VABI PKImageDecode_SelectFrame_nid_postfix(ImageDecode*, std::uint32_t) {
    return Success;
}

std::int64_t APS5_VABI PKImageDecode_Release_nid_postfix(ImageDecode** decoder) {
    ImageDecode& self = Require(Require(decoder));
    if (self.streamOwner) Method(Require(self.stream).close)(&self.stream);
    return PKFree_nid_postfix(reinterpret_cast<void**>(decoder));
}

std::int64_t APS5_VABI PKImageDecode_Create_nid_postfix(ImageDecode** decoder) {
    const std::int64_t result = PKAlloc_nid_postfix(reinterpret_cast<void**>(decoder), sizeof(ImageDecode));
    if (result < 0) return result;
    ImageDecode& self = **decoder;
    self.initialize = PKImageDecode_Initialize_nid_postfix;
    self.getPixelFormat = PKImageDecode_GetPixelFormat_nid_postfix;
    self.getSize = PKImageDecode_GetSize_nid_postfix;
    self.getResolution = PKImageDecode_GetResolution_nid_postfix;
    self.getColorContext = PKImageDecode_GetColorContext_nid_postfix;
    self.getDescriptiveMetadata = PKImageDecode_GetDescriptiveMetadata_nid_postfix;
    self.copy = PKImageDecode_Copy_nid_postfix;
    self.getFrameCount = PKImageDecode_GetFrameCount_nid_postfix;
    self.selectFrame = PKImageDecode_SelectFrame_nid_postfix;
    self.release = PKImageDecode_Release_nid_postfix;
    return Success;
}

std::int64_t APS5_VABI PKImageEncode_Initialize_nid_postfix(ImageEncode* encoder, Stream* stream, void*, std::uint64_t) {
    ImageEncode& self = Require(encoder);
    self.stream = stream;
    self.pixelFormat = JxrGuest_GUID_PKPixelFormatDontCare_nid_no_patch;
    self.resolutionX = 96;
    self.resolutionY = 96;
    self.frameCount = 1;
    Stream& target = Require(stream);
    return Method(target.getPosition)(&target, &self.startOffset);
}

std::int64_t APS5_VABI PKImageEncode_Terminate_nid_postfix(ImageEncode*) {
    return Success;
}

std::int64_t APS5_VABI PKImageEncode_SetPixelFormat_nid_postfix(ImageEncode* encoder, JxrGuestGuid format) {
    Require(encoder).pixelFormat = format;
    return Success;
}

std::int64_t APS5_VABI PKImageEncode_SetSize_nid_postfix(ImageEncode* encoder, std::int32_t width, std::int32_t height) {
    ImageEncode& self = Require(encoder);
    self.width = static_cast<std::uint32_t>(width);
    self.height = static_cast<std::uint32_t>(height);
    return Success;
}

std::int64_t APS5_VABI PKImageEncode_SetResolution_nid_postfix(ImageEncode* encoder, float resolutionX, float resolutionY) {
    ImageEncode& self = Require(encoder);
    self.resolutionX = resolutionX;
    self.resolutionY = resolutionY;
    return Success;
}

std::int64_t APS5_VABI PKImageEncode_SetColorContext_nid_postfix(ImageEncode*, const std::uint8_t*, std::uint32_t) {
    return NotYetImplemented;
}

std::int64_t APS5_VABI PKImageEncode_SetDescriptiveMetadata_nid_postfix(ImageEncode*, const void*) {
    return NotYetImplemented;
}

std::int64_t APS5_VABI PKImageEncode_WritePixels_nid_postfix(ImageEncode*, std::uint32_t, std::uint8_t*, std::uint32_t) {
    return AbstractMethod;
}

std::int64_t APS5_VABI PKImageEncode_CreateNewFrame_nid_postfix(ImageEncode*, void*, std::uint64_t) {
    return Success;
}

std::int64_t APS5_VABI PKImageEncode_Release_nid_postfix(ImageEncode** encoder) {
    ImageEncode& self = Require(Require(encoder));
    Method(Require(self.stream).close)(&self.stream);
    return PKFree_nid_postfix(reinterpret_cast<void**>(encoder));
}

std::int64_t APS5_VABI PKImageEncode_Create_nid_postfix(ImageEncode** encoder) {
    const std::int64_t result = PKAlloc_nid_postfix(reinterpret_cast<void**>(encoder), sizeof(ImageEncode));
    if (result < 0) return result;
    ImageEncode& self = **encoder;
    self.initialize = PKImageEncode_Initialize_nid_postfix;
    self.terminate = PKImageEncode_Terminate_nid_postfix;
    self.setPixelFormat = PKImageEncode_SetPixelFormat_nid_postfix;
    self.setSize = PKImageEncode_SetSize_nid_postfix;
    self.setResolution = PKImageEncode_SetResolution_nid_postfix;
    self.setColorContext = PKImageEncode_SetColorContext_nid_postfix;
    self.setDescriptiveMetadata = PKImageEncode_SetDescriptiveMetadata_nid_postfix;
    self.writePixels = PKImageEncode_WritePixels_nid_postfix;
    self.writePixelsBandedBegin = WritePixelsBandedBegin_nid_no_patch;
    self.writePixelsBanded = WritePixelsBanded_nid_no_patch;
    self.writePixelsBandedEnd = WritePixelsBandedEnd_nid_no_patch;
    self.createNewFrame = PKImageEncode_CreateNewFrame_nid_postfix;
    self.release = PKImageEncode_Release_nid_postfix;
    self.wmp = 0;
    return Success;
}

std::int64_t APS5_VABI PKFormatConverter_EnumConversions_nid_postfix(const JxrGuestGuid* source, std::uint32_t index, const JxrGuestGuid** target) {
    Require(source);
    Require(target);
    std::uint8_t found[16];
    const std::int64_t result = JxrEnumConversions_nid_no_patch(reinterpret_cast<const std::uint8_t*>(source), index, found);
    *target = Exported(found);
    return result;
}

std::int64_t APS5_VABI PKFormatConverter_GetPixelFormat_nid_postfix(FormatConverter* converter, JxrGuestGuid* format) {
    Require(format) = Require(converter).pixelFormat;
    return Success;
}

std::int64_t APS5_VABI PKFormatConverter_GetSourcePixelFormat_nid_postfix(FormatConverter* converter, JxrGuestGuid* format) {
    ImageDecode& decoder = Require(Require(converter).decoder);
    return Method(decoder.getPixelFormat)(&decoder, format);
}

std::int64_t APS5_VABI PKFormatConverter_GetSize_nid_postfix(FormatConverter* converter, std::int32_t* width, std::int32_t* height) {
    ImageDecode& decoder = Require(Require(converter).decoder);
    return Method(decoder.getSize)(&decoder, width, height);
}

std::int64_t APS5_VABI PKFormatConverter_GetResolution_nid_postfix(FormatConverter* converter, float* resolutionX, float* resolutionY) {
    ImageDecode& decoder = Require(Require(converter).decoder);
    return Method(decoder.getResolution)(&decoder, resolutionX, resolutionY);
}

std::int64_t APS5_VABI PKFormatConverter_Copy_nid_postfix(FormatConverter* converter, const Rect* rect, std::uint8_t* pixels, std::uint32_t stride) {
    FormatConverter& self = Require(converter);
    ImageDecode& decoder = Require(self.decoder);
    std::int64_t result = Method(decoder.copy)(&decoder, rect, pixels, stride);
    if (result < 0) return result;
    result = Method(self.convert)(&self, rect, pixels, stride);
    return result;
}

std::int64_t APS5_VABI PKFormatConverter_Convert_nid_postfix(FormatConverter*, const Rect*, std::uint8_t*, std::uint32_t) {
    return Success;
}

std::int64_t APS5_VABI PKFormatConverter_Release_nid_postfix(FormatConverter** converter) {
    return PKFree_nid_postfix(reinterpret_cast<void**>(converter));
}

std::int64_t APS5_VABI PKCodecFactory_CreateFormatConverter_nid_postfix(FormatConverter** converter) {
    const std::int64_t result = PKAlloc_nid_postfix(reinterpret_cast<void**>(converter), sizeof(FormatConverter));
    if (result < 0) return result;
    FormatConverter& self = **converter;
    self.initialize = PKFormatConverter_Initialize_nid_postfix;
    self.initializeConvert = PKFormatConverter_InitializeConvert_nid_postfix;
    self.getPixelFormat = PKFormatConverter_GetPixelFormat_nid_postfix;
    self.getSourcePixelFormat = PKFormatConverter_GetSourcePixelFormat_nid_postfix;
    self.getSize = PKFormatConverter_GetSize_nid_postfix;
    self.getResolution = PKFormatConverter_GetResolution_nid_postfix;
    self.copy = PKFormatConverter_Copy_nid_postfix;
    self.convert = PKFormatConverter_Convert_nid_postfix;
    self.release = PKFormatConverter_Release_nid_postfix;
    return Success;
}

std::int64_t APS5_VABI PKCreateFactory_CreateStream_nid_postfix(PkStream** stream) {
    return PKAlloc_nid_postfix(reinterpret_cast<void**>(stream), sizeof(PkStream));
}

std::int64_t APS5_VABI PKCreateFactory_Release_nid_postfix(Factory** factory) {
    return PKFree_nid_postfix(reinterpret_cast<void**>(factory));
}

std::int64_t APS5_VABI PKCreateFactory_nid_postfix(Factory** factory, std::uint32_t) {
    const std::int64_t result = PKAlloc_nid_postfix(reinterpret_cast<void**>(factory), sizeof(Factory));
    if (result < 0) return result;
    Factory& self = **factory;
    self.createStream = PKCreateFactory_CreateStream_nid_postfix;
    self.createStreamFromFilename = CreateWsFile_nid_no_patch;
    self.createStreamFromMemory = CreateWS_Memory_nid_postfix;
    self.release = PKCreateFactory_Release_nid_postfix;
    return Success;
}

std::int64_t APS5_VABI PKCreateCodecFactory_Release_nid_postfix(CodecFactory** factory) {
    return PKFree_nid_postfix(reinterpret_cast<void**>(factory));
}

std::int64_t APS5_VABI PKCreateCodecFactory_nid_postfix(CodecFactory** factory, std::uint32_t) {
    const std::int64_t result = PKAlloc_nid_postfix(reinterpret_cast<void**>(factory), sizeof(CodecFactory));
    if (result < 0) return result;
    CodecFactory& self = **factory;
    self.createCodec = PKCodecFactory_CreateCodec_nid_postfix;
    self.createDecoderFromFile = PKCodecFactory_CreateDecoderFromFile_nid_postfix;
    self.createFormatConverter = PKCodecFactory_CreateFormatConverter_nid_postfix;
    self.release = PKCreateCodecFactory_Release_nid_postfix;
    return Success;
}

}
