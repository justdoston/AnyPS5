#ifndef CORE_LIBS_PRX_LIBSCEJXR_CODECTYPES_HPP
#define CORE_LIBS_PRX_LIBSCEJXR_CODECTYPES_HPP

#include <cstddef>
#include <cstdint>

namespace Jxr {

constexpr std::size_t MaxChannels = 16;

struct PostProcInfo {
    std::int32_t macroblockDc;
    std::uint8_t macroblockTexture;
    std::int32_t blockDc[4][4];
    std::uint8_t blockTexture[4][4];
};

struct Quantizer {
    std::uint8_t index;
    std::int32_t qp;
    std::int32_t offset;
    std::int32_t mantissa;
    std::int32_t exponent;
};

struct MacroblockInfo {
    std::int32_t blockDc[MaxChannels][16];
    std::int32_t orientation;
    std::int32_t cbp[MaxChannels];
    std::int32_t diffCbp[MaxChannels];
    std::uint8_t lowpassIndex;
    std::uint8_t highpassIndex;
};

union PropVariantValue {
    std::uint8_t byteValue;
    std::uint16_t shortValue;
    std::uint32_t longValue;
    char* string;
    std::uint16_t* wideString;
    std::uint8_t* bytes;
};

struct PropVariant {
    std::int32_t type;
    PropVariantValue value;
};

using PostProcRows = PostProcInfo* [2];

static_assert(sizeof(PostProcInfo) == 88 && offsetof(PostProcInfo, blockDc) == 8 && offsetof(PostProcInfo, blockTexture) == 72);
static_assert(sizeof(Quantizer) == 20 && offsetof(Quantizer, qp) == 4 && offsetof(Quantizer, exponent) == 16);
static_assert(sizeof(MacroblockInfo) == 1160 && offsetof(MacroblockInfo, orientation) == 1024);
static_assert(offsetof(MacroblockInfo, diffCbp) == 1092 && offsetof(MacroblockInfo, lowpassIndex) == 1156);
static_assert(sizeof(PropVariant) == 16 && offsetof(PropVariant, value) == 8);

}

#endif
