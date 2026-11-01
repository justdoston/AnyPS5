#ifndef CORE_LIBS_PRX_LIBSCEJXR_GLUETYPES_HPP
#define CORE_LIBS_PRX_LIBSCEJXR_GLUETYPES_HPP

#include "prx/libSceJxr/Stream.hpp"
#include "prx/libSceJxr/include/GuidExports.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
#include <cstdint>

namespace Jxr {

struct Rect {
    std::int32_t x;
    std::int32_t y;
    std::int32_t width;
    std::int32_t height;
};

struct ImageDecode {
    std::int64_t (APS5_VABI* initialize)(ImageDecode*, Stream*);
    std::int64_t (APS5_VABI* getPixelFormat)(ImageDecode*, JxrGuestGuid*);
    std::int64_t (APS5_VABI* getSize)(ImageDecode*, std::int32_t*, std::int32_t*);
    std::int64_t (APS5_VABI* getResolution)(ImageDecode*, float*, float*);
    std::int64_t (APS5_VABI* getColorContext)(ImageDecode*, std::uint8_t*, std::uint32_t*);
    std::int64_t (APS5_VABI* getDescriptiveMetadata)(ImageDecode*, void*);
    std::int64_t (APS5_VABI* getRawStream)(ImageDecode*, Stream**);
    std::int64_t (APS5_VABI* copy)(ImageDecode*, const Rect*, std::uint8_t*, std::uint32_t);
    std::int64_t (APS5_VABI* getFrameCount)(ImageDecode*, std::uint32_t*);
    std::int64_t (APS5_VABI* selectFrame)(ImageDecode*, std::uint32_t);
    std::int64_t (APS5_VABI* release)(ImageDecode**);
    Stream* stream;
    std::int32_t streamOwner;
    std::size_t startOffset;
    JxrGuestGuid pixelFormat;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t currentLine;
    float resolutionX;
    float resolutionY;
    std::uint32_t frameCount;
    std::uint8_t codecState[66432];
};

struct FormatConverter;

struct ImageEncode {
    std::int64_t (APS5_VABI* initialize)(ImageEncode*, Stream*, void*, std::size_t);
    std::int64_t (APS5_VABI* terminate)(ImageEncode*);
    std::int64_t (APS5_VABI* setPixelFormat)(ImageEncode*, JxrGuestGuid);
    std::int64_t (APS5_VABI* setSize)(ImageEncode*, std::int32_t, std::int32_t);
    std::int64_t (APS5_VABI* setResolution)(ImageEncode*, float, float);
    std::int64_t (APS5_VABI* setColorContext)(ImageEncode*, const std::uint8_t*, std::uint32_t);
    std::int64_t (APS5_VABI* setDescriptiveMetadata)(ImageEncode*, const void*);
    std::int64_t (APS5_VABI* writePixels)(ImageEncode*, std::uint32_t, std::uint8_t*, std::uint32_t);
    std::int64_t (APS5_VABI* writeSource)(ImageEncode*, FormatConverter*, Rect*);
    std::int64_t (APS5_VABI* writePixelsBandedBegin)(ImageEncode*, Stream*);
    std::int64_t (APS5_VABI* writePixelsBanded)(ImageEncode*, std::uint32_t, std::uint8_t*, std::uint32_t, std::int32_t);
    std::int64_t (APS5_VABI* writePixelsBandedEnd)(ImageEncode*);
    std::int64_t (APS5_VABI* transcode)(ImageEncode*, ImageDecode*, void*);
    std::int64_t (APS5_VABI* createNewFrame)(ImageEncode*, void*, std::size_t);
    std::int64_t (APS5_VABI* release)(ImageEncode**);
    Stream* stream;
    std::size_t startOffset;
    JxrGuestGuid pixelFormat;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t currentLine;
    float resolutionX;
    float resolutionY;
    std::uint32_t frameCount;
    std::uint8_t encoderState[344];
    std::int32_t wmp;
    std::uint8_t containerState[66164];
};

struct FormatConverter {
    std::int64_t (APS5_VABI* initialize)(FormatConverter*, ImageDecode*, char*, JxrGuestGuid);
    std::int64_t (APS5_VABI* initializeConvert)(FormatConverter*, JxrGuestGuid, char*, JxrGuestGuid);
    std::int64_t (APS5_VABI* getPixelFormat)(FormatConverter*, JxrGuestGuid*);
    std::int64_t (APS5_VABI* getSourcePixelFormat)(FormatConverter*, JxrGuestGuid*);
    std::int64_t (APS5_VABI* getSize)(FormatConverter*, std::int32_t*, std::int32_t*);
    std::int64_t (APS5_VABI* getResolution)(FormatConverter*, float*, float*);
    std::int64_t (APS5_VABI* copy)(FormatConverter*, const Rect*, std::uint8_t*, std::uint32_t);
    std::int64_t (APS5_VABI* convert)(FormatConverter*, const Rect*, std::uint8_t*, std::uint32_t);
    std::int64_t (APS5_VABI* release)(FormatConverter**);
    ImageDecode* decoder;
    JxrGuestGuid pixelFormat;
};

struct PkStream {
    std::int64_t (APS5_VABI* initializeFromFilename)(const char*, std::uint64_t);
    std::int64_t (APS5_VABI* release)();
    void* file;
};

struct Factory {
    std::int64_t (APS5_VABI* createStream)(PkStream**);
    std::int64_t (APS5_VABI* createStreamFromFilename)(Stream**, const char*, const char*);
    std::int64_t (APS5_VABI* createStreamFromMemory)(Stream**, void*, std::size_t);
    std::int64_t (APS5_VABI* release)(Factory**);
};

struct CodecFactory {
    std::int64_t (APS5_VABI* createCodec)(const std::uint32_t*, void**);
    std::int64_t (APS5_VABI* createDecoderFromFile)(const char*, ImageDecode**);
    std::int64_t (APS5_VABI* createFormatConverter)(FormatConverter**);
    std::int64_t (APS5_VABI* release)(CodecFactory**);
};

static_assert(sizeof(Rect) == 16);
static_assert(offsetof(ImageDecode, getRawStream) == 48 && offsetof(ImageDecode, release) == 80 && offsetof(ImageDecode, stream) == 88);
static_assert(offsetof(ImageDecode, streamOwner) == 96 && offsetof(ImageDecode, startOffset) == 104 && offsetof(ImageDecode, pixelFormat) == 112);
static_assert(offsetof(ImageDecode, width) == 128 && offsetof(ImageDecode, resolutionX) == 140 && offsetof(ImageDecode, frameCount) == 148);
static_assert(offsetof(ImageDecode, codecState) == 152 && sizeof(ImageDecode) == 66584);
static_assert(offsetof(ImageEncode, release) == 112 && offsetof(ImageEncode, stream) == 120 && offsetof(ImageEncode, startOffset) == 128);
static_assert(offsetof(ImageEncode, pixelFormat) == 136 && offsetof(ImageEncode, width) == 152 && offsetof(ImageEncode, resolutionX) == 164);
static_assert(offsetof(ImageEncode, frameCount) == 172 && offsetof(ImageEncode, wmp) == 520 && sizeof(ImageEncode) == 66688);
static_assert(offsetof(FormatConverter, release) == 64 && offsetof(FormatConverter, decoder) == 72);
static_assert(offsetof(FormatConverter, pixelFormat) == 80 && sizeof(FormatConverter) == 96);
static_assert(sizeof(PkStream) == 24 && sizeof(Factory) == 32 && sizeof(CodecFactory) == 32);

}

#endif
