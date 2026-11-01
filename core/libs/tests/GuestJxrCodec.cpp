#include "prx/libSceJxr/libSceJxr.hpp"
#include "prx/libc/include/Api.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

extern "C" {
std::int64_t JxrRefIid_nid_no_patch(const char* extension, int encode, std::uint32_t* value);
int JxrRefPixelFormatFromHash_nid_no_patch(std::uint8_t hash, std::uint8_t* guid);
std::int32_t JxrRefInitPostProc_nid_no_patch(Jxr::PostProcRows* info, std::size_t width, std::size_t channels);
void JxrRefTermPostProc_nid_no_patch(Jxr::PostProcRows* info, std::size_t channels);
void JxrRefSlide_nid_no_patch(Jxr::PostProcRows* info, std::size_t channels, std::size_t width, std::int32_t top, std::int32_t bottom);
void JxrRefUpdate_nid_no_patch(Jxr::PostProcRows* info, std::int32_t* macroblock, std::size_t x, std::size_t channel);
void JxrRefPostMB_nid_no_patch(Jxr::PostProcRows* info, std::int32_t* first, std::int32_t* second, std::size_t x, std::size_t channel, std::int32_t threshold);
void JxrRefPostBlock_nid_no_patch(Jxr::PostProcRows* info, std::int32_t* first, std::int32_t* second, std::size_t x, std::size_t channel, std::int32_t threshold);
void JxrRefRemap_nid_no_patch(void* quantizer, std::int32_t shift, std::int32_t scaled);
void JxrRefFormat_nid_no_patch(void** quantizers, std::uint8_t mode, std::size_t channels, std::size_t position, std::int32_t shiftedUv, std::int32_t scaled);
std::int32_t JxrRefACPred_nid_no_patch(void* info, std::int32_t colorFormat);
}

namespace {

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "JXR codec: %s\n", message);
    std::abort();
}

template<typename TException, typename TAction>
bool Throws(TAction action) {
    try { action(); } catch (const TException&) { return true; }
    return false;
}

struct Random {
    std::uint64_t state;
    std::uint32_t Next() {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        return static_cast<std::uint32_t>(state >> 16);
    }
    std::uint32_t Below(std::uint32_t limit) { return Next() % limit; }
};

void Identifiers() {
    Require(IDPEmpty_nid_postfix(nullptr) == 0, "IDPEmpty");
    const char* extensions[] = {".jxr", ".JXR", ".jXr", ".jxrz", ".wdp", ".WDP", ".hdp", ".hdpx", ".png", "", ".jx", "jxr", ".JX", ".hd"};
    for (const char* extension : extensions) {
        for (const int encode : {0, 1}) {
            std::uint32_t expected = 0;
            const std::int64_t expectedResult = JxrRefIid_nid_no_patch(extension, encode, &expected);
            const std::uint32_t sentinel = 0xDEAD;
            const std::uint32_t* iid = &sentinel;
            const std::int64_t result = encode ? GetImageEncodeIID_nid_postfix(extension, &iid) : GetImageDecodeIID_nid_postfix(extension, &iid);
            Require(result == expectedResult, "IID result matches jxrlib");
            if (result == 0) Require(*iid == expected, "IID value matches jxrlib");
            else Require(iid == &sentinel, "failed IID lookup leaves the output");
        }
    }
    const std::uint32_t* iid = nullptr;
    Require(GetImageEncodeIID_nid_postfix(".JxR", &iid) == 0 && iid == &JxrGuest_IID_PKImageWmpEncode_nid_no_patch, "encode IID is the exported object");
    Require(GetImageDecodeIID_nid_postfix(".hdp", &iid) == 0 && iid == &JxrGuest_IID_PKImageWmpDecode_nid_no_patch, "decode IID is the exported object");
    Require(GetImageEncodeIID_nid_postfix(".wdp", &iid) == 0 && *iid == 100, "unsupported encode IID");
    Require(GetImageEncodeIID_nid_postfix(".png", nullptr) == -106, "unknown extension does not write");
    Require(Throws<std::invalid_argument>([] { GetImageDecodeIID_nid_postfix(nullptr, nullptr); }), "null extension throws");
    Require(Throws<std::invalid_argument>([] { GetImageDecodeIID_nid_postfix(".jxr", nullptr); }), "null output throws");
}

void Hashes() {
    int found = 0;
    for (int hash = 0; hash < 256; ++hash) {
        std::uint8_t expected[16];
        const int present = JxrRefPixelFormatFromHash_nid_no_patch(static_cast<std::uint8_t>(hash), expected);
        const JxrGuestGuid* guid = GetPixelFormatFromHash_nid_postfix(static_cast<std::uint8_t>(hash));
        Require((guid != nullptr) == (present != 0), "hash presence matches jxrlib");
        if (guid) {
            Require(std::memcmp(guid, expected, sizeof(expected)) == 0, "hash GUID matches jxrlib");
            ++found;
        }
    }
    Require(found > 60, "most hashes resolve");
    for (const JxrGuestGuid* exported : {&JxrGuest_GUID_PKPixelFormat24bppRGB_nid_no_patch, &JxrGuest_GUID_PKPixelFormat8bppGray_nid_no_patch,
                                         &JxrGuest_GUID_PKPixelFormat16bppRGB565_nid_no_patch})
        Require(GetPixelFormatFromHash_nid_postfix(exported->data4[7]) == exported, "hash returns the exported GUID object");
}

struct Rows {
    Jxr::PostProcRows rows[Jxr::MaxChannels] = {};
};

void Baseline(Rows& guest, Rows& native, std::size_t channels, std::size_t width) {
    for (std::size_t channel = 0; channel < channels; ++channel) {
        for (std::size_t row = 0; row < 2; ++row) {
            for (Jxr::PostProcRows* side : {&guest.rows[channel], &native.rows[channel]}) {
                Jxr::PostProcInfo* entries = (*side)[row];
                Require(entries[-1].macroblockTexture == 3 && entries[width].macroblockTexture == 3, "boundary macroblocks are bumpy");
                for (int y = 0; y < 4; ++y)
                    for (int x = 0; x < 4; ++x)
                        Require(entries[-1].blockTexture[y][x] == 3 && entries[width].blockTexture[y][x] == 3, "boundary blocks are bumpy");
                std::memset(entries - 1, 0, (width + 2) * sizeof(Jxr::PostProcInfo));
                for (const std::size_t index : {std::size_t{0}, width + 1}) {
                    entries[static_cast<std::ptrdiff_t>(index) - 1].macroblockTexture = 3;
                    std::memset(entries[static_cast<std::ptrdiff_t>(index) - 1].blockTexture, 3, 16);
                }
            }
        }
    }
}

bool SameRows(Rows& guest, Rows& native, std::size_t channels, std::size_t width) {
    for (std::size_t channel = 0; channel < channels; ++channel)
        for (std::size_t row = 0; row < 2; ++row)
            if (std::memcmp(guest.rows[channel][row] - 1, native.rows[channel][row] - 1, (width + 2) * sizeof(Jxr::PostProcInfo)) != 0) return false;
    return true;
}

void PostProcessing() {
    constexpr std::size_t channels = 3;
    constexpr std::size_t width = 9;
    constexpr std::size_t rows = 6;
    Rows guest;
    Rows native;
    Require(initPostProc_nid_postfix(guest.rows, width, channels) == 0, "initPostProc");
    Require(JxrRefInitPostProc_nid_no_patch(native.rows, width, channels) == 0, "reference initPostProc");
    Baseline(guest, native, channels, width);
    Random random{2026};
    std::vector<std::int32_t> guestPixels(channels * 2 * (width + 2) * 256);
    std::vector<std::int32_t> nativePixels;
    auto plane = [&](std::vector<std::int32_t>& pixels, std::size_t channel, std::size_t row, std::size_t x) {
        return pixels.data() + ((channel * 2 + row) * (width + 2) + x + 1) * 256;
    };
    for (auto& value : guestPixels) value = static_cast<std::int32_t>(random.Below(64)) - 32;
    nativePixels = guestPixels;
    int smoothed = 0;
    for (std::size_t row = 0; row <= rows; ++row) {
        const std::int32_t top = row == 0;
        const std::int32_t bottom = row == rows;
        slideOneMBRow_nid_postfix(guest.rows, channels, width, top, bottom);
        JxrRefSlide_nid_no_patch(native.rows, channels, width, top, bottom);
        Require(SameRows(guest, native, channels, width), "slideOneMBRow matches jxrlib");
        for (std::size_t channel = 0; channel < channels; ++channel) {
            for (std::size_t x = 0; x < width && !bottom; ++x) {
                std::array<std::int32_t, 256> macroblock{};
                macroblock[0] = static_cast<std::int32_t>(random.Below(8));
                if (random.Below(3) == 0) macroblock[16 * (1 + random.Below(15))] = 5;
                for (int block = 0; block < 16; ++block)
                    if (random.Below(4) == 0) macroblock[block * 16 + 1 + random.Below(15)] = 1;
                std::array<std::int32_t, 256> copy = macroblock;
                updatePostProcInfo_nid_postfix(guest.rows, macroblock.data(), x, channel);
                JxrRefUpdate_nid_no_patch(native.rows, copy.data(), x, channel);
            }
            Require(SameRows(guest, native, channels, width), "updatePostProcInfo matches jxrlib");
            for (std::size_t x = 1; x < width; ++x) {
                const std::int32_t threshold = static_cast<std::int32_t>(random.Below(6));
                const std::size_t previous = row & 1;
                postProcMB_nid_postfix(guest.rows, plane(guestPixels, channel, previous, x), plane(guestPixels, channel, previous ^ 1, x), x, channel, threshold);
                JxrRefPostMB_nid_no_patch(native.rows, plane(nativePixels, channel, previous, x), plane(nativePixels, channel, previous ^ 1, x), x, channel, threshold);
                postProcBlock_nid_postfix(guest.rows, plane(guestPixels, channel, previous, x), plane(guestPixels, channel, previous ^ 1, x), x, channel, threshold);
                JxrRefPostBlock_nid_no_patch(native.rows, plane(nativePixels, channel, previous, x), plane(nativePixels, channel, previous ^ 1, x), x, channel, threshold);
            }
            Require(SameRows(guest, native, channels, width), "post-processing records match jxrlib");
            Require(guestPixels == nativePixels, "post-processed pixels match jxrlib");
        }
    }
    std::vector<std::int32_t> untouched(guestPixels.size());
    Random replay{2026};
    for (auto& value : untouched) value = static_cast<std::int32_t>(replay.Below(64)) - 32;
    smoothed = guestPixels != untouched;
    Require(smoothed, "post-processing changed pixels");
    termPostProc_nid_postfix(guest.rows, channels);
    JxrRefTermPostProc_nid_no_patch(native.rows, channels);

    Rows huge;
    Require(initPostProc_nid_postfix(huge.rows, std::uint64_t{1} << 40, 1) == -1 && huge.rows[0][0] == nullptr, "oversized width fails before allocating");
}

void Quantizers() {
    Jxr::Quantizer* quantizers[Jxr::MaxChannels] = {};
    Require(allocateQuantizer_nid_postfix(quantizers, 17, 1) == -1 && quantizers[0] == nullptr, "too many channels");
    Require(allocateQuantizer_nid_postfix(quantizers, 3, 17) == -1 && quantizers[0] == nullptr, "too many quantizers");
    Require(allocateQuantizer_nid_postfix(quantizers, 16, 16) == 0 && quantizers[0], "allocateQuantizer");
    for (std::size_t channel = 1; channel < Jxr::MaxChannels; ++channel)
        Require(quantizers[channel] == quantizers[0] + channel * 16, "quantizer rows are consecutive");
    Random random{99};
    std::vector<Jxr::Quantizer> native(16 * 16);
    Jxr::Quantizer* nativeRows[Jxr::MaxChannels];
    for (std::size_t channel = 0; channel < Jxr::MaxChannels; ++channel) nativeRows[channel] = native.data() + channel * 16;
    for (int round = 0; round < 3000; ++round) {
        for (std::size_t index = 0; index < 256; ++index) {
            Jxr::Quantizer value{};
            std::memset(&value, 0, sizeof(value));
            value.index = static_cast<std::uint8_t>(random.Next());
            value.qp = static_cast<std::int32_t>(random.Next());
            quantizers[0][index] = value;
            native[index] = value;
        }
        const auto mode = static_cast<std::uint8_t>(random.Below(4));
        const std::size_t channels = 1 + random.Below(16);
        const std::size_t position = random.Below(16);
        const std::int32_t shifted = static_cast<std::int32_t>(random.Below(2));
        const std::int32_t scaled = static_cast<std::int32_t>(random.Below(2));
        formatQuantizer_nid_postfix(quantizers, mode, channels, position, shifted, scaled);
        JxrRefFormat_nid_no_patch(reinterpret_cast<void**>(nativeRows), mode, channels, position, shifted, scaled);
        Require(std::memcmp(quantizers[0], native.data(), 256 * sizeof(Jxr::Quantizer)) == 0, "formatQuantizer matches jxrlib");
        Jxr::Quantizer single{};
        std::memset(&single, 0, sizeof(single));
        single.index = static_cast<std::uint8_t>(random.Next());
        Jxr::Quantizer reference = single;
        const std::int32_t shift = static_cast<std::int32_t>(random.Below(3));
        remapQP_nid_postfix(&single, shift, scaled);
        JxrRefRemap_nid_no_patch(&reference, shift, scaled);
        Require(std::memcmp(&single, &reference, sizeof(single)) == 0, "remapQP matches jxrlib");
    }
    freeQuantizer_nid_postfix(quantizers);
    Jxr::Quantizer* empty[Jxr::MaxChannels] = {};
    freeQuantizer_nid_postfix(empty);
}

void Prediction() {
    Random random{7};
    for (int round = 0; round < 20000; ++round) {
        Jxr::MacroblockInfo info{};
        for (auto& channel : info.blockDc)
            for (auto& value : channel) value = static_cast<std::int32_t>(random.Below(2001)) - 1000;
        const std::int32_t format = static_cast<std::int32_t>(random.Below(9));
        Jxr::MacroblockInfo copy = info;
        Require(getACPredMode_nid_postfix(&info, format) == JxrRefACPred_nid_no_patch(&copy, format), "getACPredMode matches jxrlib");
    }
}

void Metadata() {
    Jxr::PropVariant text{};
    text.type = 30;
    text.value.string = static_cast<char*>(malloc_nid_postfix(8));
    FreeDescMetadata_nid_postfix(&text);
    Require(text.value.string == nullptr, "string metadata freed");
    Jxr::PropVariant wide{};
    wide.type = 31;
    wide.value.wideString = static_cast<std::uint16_t*>(malloc_nid_postfix(8));
    FreeDescMetadata_nid_postfix(&wide);
    Require(wide.value.wideString == nullptr, "wide metadata freed");
    for (const std::int32_t type : {0, 18, 19}) {
        Jxr::PropVariant value{};
        value.type = type;
        value.value.longValue = 0x12345678;
        FreeDescMetadata_nid_postfix(&value);
        Require(value.value.longValue == 0x12345678, "scalar metadata untouched");
    }
    Jxr::PropVariant byte{};
    byte.type = 17;
    Require(Throws<std::runtime_error>([&] { FreeDescMetadata_nid_postfix(&byte); }), "unhandled variant throws");
    Require(Throws<std::invalid_argument>([] { FreeDescMetadata_nid_postfix(nullptr); }), "null variant throws");
}

}

int main() {
    Identifiers();
    Hashes();
    PostProcessing();
    Quantizers();
    Prediction();
    Metadata();
    return 0;
}
