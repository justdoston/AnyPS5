#include "prx/libSceJxr/libSceJxr.hpp"
#include "prx/libc/include/Api.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

extern "C" {
std::int64_t JxrRefEnumConversions_nid_no_patch(const std::uint8_t* source, std::uint32_t index, std::uint8_t* target);
std::size_t JxrRefLayout_nid_no_patch(std::size_t* values);
}

namespace {

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "JXR glue: %s\n", message);
    std::abort();
}

template<typename TAction>
bool Rejects(TAction action) {
    try { action(); } catch (const std::invalid_argument&) { return true; }
    return false;
}

std::vector<char> calls;
std::int64_t positionResult;
std::size_t positionValue;

std::int64_t APS5_VABI GetPosition(Jxr::Stream*, std::size_t* position) {
    calls.push_back('p');
    *position = positionValue;
    return positionResult;
}

std::int64_t APS5_VABI CloseStream(Jxr::Stream** stream) {
    calls.push_back('c');
    *stream = nullptr;
    return 0;
}

std::int64_t APS5_VABI DecoderCopy(Jxr::ImageDecode*, const Jxr::Rect* rect, std::uint8_t* pixels, std::uint32_t stride) {
    calls.push_back('d');
    Require(rect->width == 7 && stride == 64, "copy arguments");
    pixels[0] = 1;
    return 0;
}

std::int64_t APS5_VABI Convert(Jxr::FormatConverter*, const Jxr::Rect*, std::uint8_t* pixels, std::uint32_t) {
    calls.push_back('v');
    pixels[0] += 1;
    return 5;
}

std::int64_t APS5_VABI FailingCopy(Jxr::ImageDecode*, const Jxr::Rect*, std::uint8_t*, std::uint32_t) {
    calls.push_back('f');
    return -104;
}

void Layout() {
    std::array<std::size_t, 64> values{};
    const std::size_t count = JxrRefLayout_nid_no_patch(values.data());
    const std::array<std::size_t, 29> expected = {
        sizeof(Jxr::ImageDecode), offsetof(Jxr::ImageDecode, release), offsetof(Jxr::ImageDecode, stream), offsetof(Jxr::ImageDecode, streamOwner),
        offsetof(Jxr::ImageDecode, startOffset), offsetof(Jxr::ImageDecode, pixelFormat), offsetof(Jxr::ImageDecode, width),
        offsetof(Jxr::ImageDecode, resolutionX), offsetof(Jxr::ImageDecode, frameCount), offsetof(Jxr::ImageDecode, codecState), 88,
        sizeof(Jxr::ImageEncode), offsetof(Jxr::ImageEncode, writePixelsBandedBegin), offsetof(Jxr::ImageEncode, createNewFrame),
        offsetof(Jxr::ImageEncode, release), offsetof(Jxr::ImageEncode, stream), offsetof(Jxr::ImageEncode, startOffset),
        offsetof(Jxr::ImageEncode, pixelFormat), offsetof(Jxr::ImageEncode, width), offsetof(Jxr::ImageEncode, resolutionX),
        offsetof(Jxr::ImageEncode, frameCount), offsetof(Jxr::ImageEncode, wmp), sizeof(Jxr::FormatConverter),
        offsetof(Jxr::FormatConverter, convert), offsetof(Jxr::FormatConverter, decoder), offsetof(Jxr::FormatConverter, pixelFormat),
        sizeof(Jxr::Factory), sizeof(Jxr::CodecFactory), sizeof(Jxr::PkStream)};
    Require(count == expected.size(), "layout count");
    for (std::size_t index = 0; index < count; ++index)
        if (values[index] != expected[index]) {
            std::fprintf(stderr, "layout value %zu: jxrlib %zu, guest %zu\n", index, values[index], expected[index]);
            Require(false, "guest layout matches jxrlib");
        }
}

void Decoder() {
    Jxr::ImageDecode* decoder = nullptr;
    Require(PKImageDecode_Create_nid_postfix(&decoder) == 0 && decoder, "PKImageDecode_Create");
    Require(decoder->initialize == PKImageDecode_Initialize_nid_postfix && decoder->release == PKImageDecode_Release_nid_postfix, "decoder methods");
    Require(decoder->getRawStream == nullptr && decoder->copy == PKImageDecode_Copy_nid_postfix, "decoder copy and raw stream");
    std::memset(decoder->codecState, 0xAB, sizeof(decoder->codecState));
    Jxr::Stream stream{};
    stream.getPosition = GetPosition;
    stream.close = CloseStream;
    positionResult = 0;
    positionValue = 37;
    Require(decoder->initialize(decoder, &stream) == 0, "decoder initialize");
    Require(decoder->stream == &stream && decoder->startOffset == 37 && decoder->frameCount == 1, "decoder fields");
    Require(decoder->resolutionX == 96 && decoder->resolutionY == 96, "decoder resolution");
    Require(std::memcmp(&decoder->pixelFormat, &JxrGuest_GUID_PKPixelFormatDontCare_nid_no_patch, 16) == 0, "decoder format");
    Require(decoder->codecState[0] == 0 && decoder->codecState[87] == 0 && decoder->codecState[88] == 0xAB, "decoder clears only the container record");
    decoder->codecState[0] = 0xCD;
    positionResult = -102;
    Require(decoder->initialize(decoder, &stream) == 0 && decoder->codecState[0] == 0xCD, "failed position still succeeds without clearing");
    decoder->width = 640;
    decoder->height = 480;
    decoder->resolutionX = 72;
    JxrGuestGuid format{};
    std::int32_t width = 0;
    std::int32_t height = 0;
    float x = 0;
    float y = 0;
    std::uint32_t frames = 0;
    Require(decoder->getPixelFormat(decoder, &format) == 0 && std::memcmp(&format, &decoder->pixelFormat, 16) == 0, "decoder pixel format");
    Require(decoder->getSize(decoder, &width, &height) == 0 && width == 640 && height == 480, "decoder size");
    Require(decoder->getResolution(decoder, &x, &y) == 0 && x == 72 && y == 96, "decoder resolution");
    Require(decoder->getFrameCount(decoder, &frames) == 0 && frames == 1, "decoder frames");
    Require(decoder->selectFrame(decoder, 3) == 0, "decoder select frame");
    Require(decoder->getColorContext(decoder, nullptr, nullptr) == -2 && decoder->getDescriptiveMetadata(decoder, nullptr) == -2, "decoder unimplemented queries");
    Require(decoder->copy(decoder, nullptr, nullptr, 0) == -3, "decoder abstract copy");
    calls.clear();
    decoder->streamOwner = 0;
    Jxr::ImageDecode* other = nullptr;
    Require(PKImageDecode_Create_nid_postfix(&other) == 0, "second decoder");
    other->stream = &stream;
    Require(PKImageDecode_Release_nid_postfix(&other) == 0 && other == nullptr && calls.empty(), "release without owning the stream");
    decoder->streamOwner = 1;
    Require(decoder->release(&decoder) == 0 && decoder == nullptr && calls == std::vector<char>{'c'}, "release closes an owned stream");
}

void Encoder() {
    Jxr::ImageEncode* encoder = nullptr;
    Require(PKImageEncode_Create_nid_postfix(&encoder) == 0 && encoder, "PKImageEncode_Create");
    Require(encoder->writeSource == nullptr && encoder->transcode == nullptr && encoder->wmp == 0, "encoder unset methods");
    Require(encoder->writePixelsBandedBegin(encoder, nullptr) == -3 && encoder->writePixelsBanded(encoder, 1, nullptr, 0, 1) == -3, "banded writes are abstract");
    Require(encoder->writePixelsBandedEnd(encoder) == -3 && encoder->writePixels(encoder, 1, nullptr, 0) == -3, "pixel writes are abstract");
    Jxr::Stream stream{};
    stream.getPosition = GetPosition;
    stream.close = CloseStream;
    positionResult = -102;
    positionValue = 11;
    Require(encoder->initialize(encoder, &stream, nullptr, 0) == -102, "encoder initialize returns the position error");
    positionResult = 0;
    Require(encoder->initialize(encoder, &stream, nullptr, 0) == 0 && encoder->startOffset == 11, "encoder initialize");
    Require(encoder->setPixelFormat(encoder, JxrGuest_GUID_PKPixelFormat24bppRGB_nid_no_patch) == 0, "encoder set format");
    Require(std::memcmp(&encoder->pixelFormat, &JxrGuest_GUID_PKPixelFormat24bppRGB_nid_no_patch, 16) == 0, "encoder format stored");
    Require(encoder->setSize(encoder, -5, 9) == 0 && encoder->width == 0xFFFFFFFBu && encoder->height == 9, "encoder size");
    Require(encoder->setResolution(encoder, 300.5f, 150.25f) == 0 && encoder->resolutionX == 300.5f && encoder->resolutionY == 150.25f, "encoder resolution");
    Require(encoder->setColorContext(encoder, nullptr, 0) == -2 && encoder->setDescriptiveMetadata(encoder, nullptr) == -2, "encoder unimplemented setters");
    Require(encoder->terminate(encoder) == 0 && encoder->createNewFrame(encoder, nullptr, 0) == 0, "encoder terminate and frame");
    calls.clear();
    Require(encoder->release(&encoder) == 0 && encoder == nullptr && calls == std::vector<char>{'c'}, "encoder release closes its stream");
    Jxr::ImageEncode* orphan = nullptr;
    Require(PKImageEncode_Create_nid_postfix(&orphan) == 0, "encoder without stream");
    Require(Rejects([&] { PKImageEncode_Release_nid_postfix(&orphan); }), "releasing an encoder without a stream throws");
    PKFree_nid_postfix(reinterpret_cast<void**>(&orphan));
}

void Converter() {
    Jxr::CodecFactory* codecs = nullptr;
    Require(PKCreateCodecFactory_nid_postfix(&codecs, 1) == 0 && codecs, "PKCreateCodecFactory");
    Require(codecs->createCodec == PKCodecFactory_CreateCodec_nid_postfix && codecs->createDecoderFromFile == PKCodecFactory_CreateDecoderFromFile_nid_postfix, "codec factory methods");
    Jxr::FormatConverter* converter = nullptr;
    Require(codecs->createFormatConverter(&converter) == 0 && converter, "create format converter");
    Require(converter->initialize == PKFormatConverter_Initialize_nid_postfix && converter->convert == PKFormatConverter_Convert_nid_postfix, "converter methods");
    Jxr::ImageDecode* decoder = nullptr;
    Require(PKImageDecode_Create_nid_postfix(&decoder) == 0, "decoder for converter");
    decoder->width = 8;
    decoder->height = 2;
    decoder->pixelFormat = JxrGuest_GUID_PKPixelFormat8bppGray_nid_no_patch;
    converter->decoder = decoder;
    converter->pixelFormat = JxrGuest_GUID_PKPixelFormat24bppRGB_nid_no_patch;
    JxrGuestGuid format{};
    std::int32_t width = 0;
    std::int32_t height = 0;
    float x = 0;
    float y = 0;
    Require(converter->getPixelFormat(converter, &format) == 0 && std::memcmp(&format, &JxrGuest_GUID_PKPixelFormat24bppRGB_nid_no_patch, 16) == 0, "converter format");
    Require(converter->getSourcePixelFormat(converter, &format) == 0 && std::memcmp(&format, &JxrGuest_GUID_PKPixelFormat8bppGray_nid_no_patch, 16) == 0, "converter source format");
    Require(converter->getSize(converter, &width, &height) == 0 && width == 8 && height == 2, "converter size");
    Require(converter->getResolution(converter, &x, &y) == 0 && x == 0 && y == 0, "converter resolution");
    Require(converter->convert(converter, nullptr, nullptr, 0) == 0, "default convert");
    decoder->copy = DecoderCopy;
    converter->convert = Convert;
    std::uint8_t pixels[64] = {};
    const Jxr::Rect rect{0, 0, 7, 1};
    calls.clear();
    Require(converter->copy(converter, &rect, pixels, 64) == 5 && pixels[0] == 2 && calls == std::vector<char>{'d', 'v'}, "converter copy then convert");
    decoder->copy = FailingCopy;
    calls.clear();
    Require(converter->copy(converter, &rect, pixels, 64) == -104 && calls == std::vector<char>{'f'}, "converter copy stops on a decoder error");
    Require(converter->release(&converter) == 0 && converter == nullptr, "converter release");
    PKImageDecode_Release_nid_postfix(&decoder);
    Require(codecs->release(&codecs) == 0 && codecs == nullptr, "codec factory release");

    int found = 0;
    for (const JxrGuestGuid* source : {&JxrGuest_GUID_PKPixelFormat24bppRGB_nid_no_patch, &JxrGuest_GUID_PKPixelFormat24bppBGR_nid_no_patch,
                                       &JxrGuest_GUID_PKPixelFormat32bppBGRA_nid_no_patch, &JxrGuest_GUID_PKPixelFormat8bppGray_nid_no_patch,
                                       &JxrGuest_GUID_PKPixelFormat48bppRGBFixedPoint_nid_no_patch, &JxrGuest_GUID_PKPixelFormatDontCare_nid_no_patch}) {
        for (std::uint32_t index = 0; index < 24; ++index) {
            std::uint8_t expected[16];
            const std::int64_t expectedResult = JxrRefEnumConversions_nid_no_patch(reinterpret_cast<const std::uint8_t*>(source), index, expected);
            const JxrGuestGuid* target = nullptr;
            Require(PKFormatConverter_EnumConversions_nid_postfix(source, index, &target) == expectedResult, "enumeration result matches jxrlib");
            Require(target && std::memcmp(target, expected, 16) == 0, "enumerated format matches jxrlib");
            if (expectedResult == 0) ++found;
            else Require(target == &JxrGuest_GUID_PKPixelFormatDontCare_nid_no_patch && expectedResult == -108, "missing index returns DontCare");
        }
    }
    Require(found > 10, "conversions enumerated");
}

void Factories() {
    Jxr::Factory* factory = nullptr;
    Require(PKCreateFactory_nid_postfix(&factory, 1) == 0 && factory, "PKCreateFactory");
    Require(factory->createStreamFromMemory == CreateWS_Memory_nid_postfix && factory->release == PKCreateFactory_Release_nid_postfix, "factory methods");
    Jxr::PkStream* pk = nullptr;
    Require(factory->createStream(&pk) == 0 && pk && !pk->initializeFromFilename && !pk->release && !pk->file, "factory creates a zeroed stream record");
    PKFree_nid_postfix(reinterpret_cast<void**>(&pk));
    const auto path = (std::filesystem::temp_directory_path() / "anyps5-jxr-glue.bin").string();
    {
        FileStream* file = fopen_nid_postfix(path.c_str(), "wb");
        fwrite_nid_postfix("jxrdata", 7, 1, file);
        fclose_nid_postfix(file);
    }
    Jxr::Stream* stream = nullptr;
    Require(factory->createStreamFromFilename(&stream, path.c_str(), "rb") == 0 && stream, "factory opens a file stream");
    char text[8] = {};
    Require(stream->read(stream, text, 7) == 0 && std::memcmp(text, "jxrdata", 7) == 0, "file stream reads");
    std::uint64_t position = 0;
    Require(stream->getPosition(stream, &position) == 0 && position == 7 && stream->end(stream) == 0, "file stream position");
    Require(stream->close(&stream) == 0 && stream == nullptr, "file stream closes");
    std::remove(path.c_str());
    Require(factory->createStreamFromFilename(&stream, (path + ".missing").c_str(), "rb") == -102 && stream, "missing file fails after allocating the stream");
    WMPFree_nid_postfix(reinterpret_cast<void**>(&stream));
    std::uint8_t memory[4] = {1, 2, 3, 4};
    Require(factory->createStreamFromMemory(&stream, memory, 4) == 0 && stream, "factory memory stream");
    stream->close(&stream);
    Require(factory->release(&factory) == 0 && factory == nullptr, "factory release");
}

}

int main() {
    Layout();
    Decoder();
    Encoder();
    Converter();
    Factories();
    return 0;
}
