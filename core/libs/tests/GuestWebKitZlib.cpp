#include "prx/libSceNKWebKitRequirements/libSceNKWebKitRequirements.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

using NKWebKitRequirements::GuestZStream;

namespace {

constexpr int Z_OK = 0;
constexpr int Z_STREAM_END = 1;
constexpr int Z_STREAM_ERROR = -2;
constexpr int Z_DATA_ERROR = -3;
constexpr int Z_BUF_ERROR = -5;
constexpr int Z_VERSION_ERROR = -6;
constexpr int Z_NO_FLUSH = 0;
constexpr int Z_FINISH = 4;
constexpr int Z_DEFLATED = 8;
constexpr int STREAM_SIZE = static_cast<int>(sizeof(GuestZStream));

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "WebKit zlib assertion failed: %s\n", message);
    std::abort();
}

struct Allocations {
    int allocated;
    int freed;
};

void* APS5_VABI CountingAlloc(void* opaque, std::uint32_t items, std::uint32_t size) {
    ++static_cast<Allocations*>(opaque)->allocated;
    return std::calloc(items, size);
}

void APS5_VABI CountingFree(void* opaque, void* address) {
    ++static_cast<Allocations*>(opaque)->freed;
    std::free(address);
}

std::vector<std::uint8_t> Sample() {
    std::vector<std::uint8_t> data;
    for (int index = 0; index < 20000; ++index) data.push_back(static_cast<std::uint8_t>((index * 7 + index / 13) % 61 + 32));
    return data;
}

std::uint32_t Crc32(const std::vector<std::uint8_t>& data) {
    std::uint32_t crc = 0xFFFFFFFFu;
    for (const auto value : data) {
        crc ^= value;
        for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

std::uint32_t ReadLe32(const std::uint8_t* bytes) {
    return static_cast<std::uint32_t>(bytes[0]) | static_cast<std::uint32_t>(bytes[1]) << 8 |
        static_cast<std::uint32_t>(bytes[2]) << 16 | static_cast<std::uint32_t>(bytes[3]) << 24;
}

std::uint32_t Adler32(const std::vector<std::uint8_t>& data) {
    std::uint32_t low = 1;
    std::uint32_t high = 0;
    for (const auto value : data) {
        low = (low + value) % 65521;
        high = (high + low) % 65521;
    }
    return (high << 16) | low;
}

std::vector<std::uint8_t> Compress(GuestZStream& stream, const std::vector<std::uint8_t>& input, std::uint32_t chunk) {
    std::vector<std::uint8_t> output;
    std::vector<std::uint8_t> buffer(chunk);
    stream.next_in = input.data();
    stream.avail_in = static_cast<std::uint32_t>(input.size());
    int result = Z_OK;
    while (result == Z_OK) {
        stream.next_out = buffer.data();
        stream.avail_out = chunk;
        result = deflate_nid_postfix(&stream, Z_FINISH);
        Require(result == Z_OK || result == Z_STREAM_END, "deflate progresses");
        output.insert(output.end(), buffer.data(), buffer.data() + (chunk - stream.avail_out));
    }
    return output;
}

std::vector<std::uint8_t> Decompress(GuestZStream& stream, const std::vector<std::uint8_t>& input, std::uint32_t chunk) {
    std::vector<std::uint8_t> output;
    std::vector<std::uint8_t> buffer(chunk);
    std::size_t offset = 0;
    int result = Z_OK;
    while (result == Z_OK) {
        if (stream.avail_in == 0 && offset < input.size()) {
            stream.next_in = input.data() + offset;
            stream.avail_in = static_cast<std::uint32_t>(std::min<std::size_t>(chunk, input.size() - offset));
            offset += stream.avail_in;
        }
        stream.next_out = buffer.data();
        stream.avail_out = chunk;
        result = inflate_nid_postfix(&stream, Z_NO_FLUSH);
        Require(result == Z_OK || result == Z_STREAM_END, "inflate progresses");
        output.insert(output.end(), buffer.data(), buffer.data() + (chunk - stream.avail_out));
    }
    return output;
}

void RoundTrip(int windowBits, std::uint32_t chunk) {
    const auto data = Sample();
    Allocations counts{};
    GuestZStream deflater{};
    deflater.zalloc = CountingAlloc;
    deflater.zfree = CountingFree;
    deflater.opaque = &counts;
    Require(deflateInit2__nid_postfix(&deflater, 6, Z_DEFLATED, windowBits, 8, 0, "1.3.1", STREAM_SIZE) == Z_OK, "deflate init");
    Require(deflater.state != nullptr && counts.allocated > 0, "deflate allocates through the guest allocator");
    Require(deflater.total_in == 0 && deflater.total_out == 0, "deflate init clears totals");
    const auto compressed = Compress(deflater, data, chunk);
    Require(deflater.total_in == data.size() && deflater.total_out == compressed.size(), "deflate totals");
    if (windowBits > 0 && windowBits < 16) Require(deflater.adler == Adler32(data), "deflate adler");
    if (windowBits > 15) {
        const std::uint8_t header[] = {0x1f, 0x8b, 8, 0, 0, 0, 0, 0, 0, 3};
        Require(compressed.size() > 18 && std::memcmp(compressed.data(), header, sizeof(header)) == 0, "gzip header");
        Require(deflater.adler == Crc32(data), "gzip crc in adler");
        Require(ReadLe32(compressed.data() + compressed.size() - 8) == Crc32(data), "gzip trailer crc");
        Require(ReadLe32(compressed.data() + compressed.size() - 4) == data.size(), "gzip trailer size");
    }
    Require(compressed.size() <= deflateBound_nid_postfix(nullptr, data.size()), "conservative bound holds");

    Require(deflateReset_nid_postfix(&deflater) == Z_OK, "deflate reset");
    Require(deflater.total_in == 0 && deflater.total_out == 0, "deflate reset clears totals");
    Require(Compress(deflater, data, chunk) == compressed, "deflate after reset is identical");
    Require(deflateEnd_nid_postfix(&deflater) == Z_OK, "deflate end");
    Require(deflater.state == nullptr, "deflate end clears state");
    Require(counts.allocated == counts.freed, "deflate frees everything it allocated");
    Require(deflateEnd_nid_postfix(&deflater) == Z_STREAM_ERROR, "second deflate end");

    GuestZStream inflater{};
    Require(inflateInit2__nid_postfix(&inflater, windowBits, "1.2.11", STREAM_SIZE) == Z_OK, "inflate init");
    Require(inflater.zalloc != nullptr && inflater.zfree != nullptr && inflater.opaque == nullptr, "inflate fills default allocators");
    Require(Decompress(inflater, compressed, chunk) == data, "inflate restores data");
    Require(inflater.total_in == compressed.size() && inflater.total_out == data.size(), "inflate totals");
    Require(inflateReset_nid_postfix(&inflater) == Z_OK, "inflate reset");
    Require(inflater.total_in == 0 && inflater.total_out == 0, "inflate reset clears totals");
    Require(Decompress(inflater, compressed, chunk) == data, "inflate after reset");
    Require(inflateEnd_nid_postfix(&inflater) == Z_OK && inflater.state == nullptr, "inflate end");

    if (windowBits == 15 || windowBits == 31) {
        GuestZStream detector{};
        Require(inflateInit2__nid_postfix(&detector, 47, "1.3.1", STREAM_SIZE) == Z_OK, "detecting inflate init");
        Require(Decompress(detector, compressed, chunk) == data, "detected wrapper decodes");
        Require(detector.adler == (windowBits == 31 ? Crc32(data) : Adler32(data)), "detected check value");
        Require(inflateEnd_nid_postfix(&detector) == Z_OK, "detecting inflate end");
    }
}

void Errors() {
    GuestZStream stream{};
    Require(deflateInit2__nid_postfix(&stream, 6, Z_DEFLATED, 15, 8, 0, nullptr, STREAM_SIZE) == Z_VERSION_ERROR, "null version");
    Require(deflateInit2__nid_postfix(&stream, 6, Z_DEFLATED, 15, 8, 0, "2.0", STREAM_SIZE) == Z_VERSION_ERROR, "major version");
    Require(inflateInit2__nid_postfix(&stream, 15, "1.3.1", STREAM_SIZE - 8) == Z_VERSION_ERROR, "stream size");
    Require(stream.zalloc == nullptr && stream.state == nullptr, "version errors leave the stream");
    Require(inflateInit2__nid_postfix(nullptr, 15, "1.3.1", STREAM_SIZE) == Z_STREAM_ERROR, "null inflate stream");
    Require(deflate_nid_postfix(nullptr, Z_FINISH) == Z_STREAM_ERROR, "null deflate stream");
    Require(inflate_nid_postfix(&stream, Z_FINISH) == Z_STREAM_ERROR, "uninitialized inflate");

    Require(inflateInit2__nid_postfix(&stream, 20, "1.3.1", STREAM_SIZE) == Z_STREAM_ERROR, "invalid wrapped window");
    Require(deflateInit2__nid_postfix(&stream, 6, Z_DEFLATED, 24, 8, 0, "1.3.1", STREAM_SIZE) == Z_STREAM_ERROR, "gzip window 8");
    stream = GuestZStream{};

    int sentinel = 0;
    stream.state = &sentinel;
    Require(deflateInit2__nid_postfix(&stream, 10, Z_DEFLATED, 15, 8, 0, "1.3.1", STREAM_SIZE) == Z_STREAM_ERROR, "bad level");
    Require(stream.state == &sentinel, "rejected deflate parameters leave state");
    Require(inflateInit2__nid_postfix(&stream, 7, "1.3.1", STREAM_SIZE) == Z_STREAM_ERROR, "bad window");
    Require(stream.state == nullptr, "rejected inflate window clears state");

    GuestZStream deflater{};
    Require(deflateInit2__nid_postfix(&deflater, 6, Z_DEFLATED, 15, 8, 0, "1.3.1", STREAM_SIZE) == Z_OK, "deflate init");
    Require(deflate_nid_postfix(&deflater, 6) == Z_STREAM_ERROR, "bad flush");
    Require(inflate_nid_postfix(&deflater, Z_NO_FLUSH) == Z_STREAM_ERROR, "deflate stream passed to inflate");
    GuestZStream copy = deflater;
    Require(deflate_nid_postfix(&copy, Z_FINISH) == Z_STREAM_ERROR, "copied stream");
    Require(deflateBound_nid_postfix(&deflater, 1000) == 1013, "default parameter bound");
    Require(deflateBound_nid_postfix(nullptr, 1000) == 1151, "conservative bound");
    Require(deflateBound_nid_postfix(nullptr, ~0ULL) == ~0ULL, "saturated bound");
    auto saved = deflater.zfree;
    deflater.zfree = nullptr;
    Require(deflate_nid_postfix(&deflater, Z_FINISH) == Z_STREAM_ERROR, "missing free function");
    deflater.zfree = saved;
    Require(deflateEnd_nid_postfix(&deflater) == Z_OK, "deflate end");

    GuestZStream gzipDeflater{};
    Require(deflateInit2__nid_postfix(&gzipDeflater, 6, Z_DEFLATED, 31, 8, 0, "1.3.1", STREAM_SIZE) == Z_OK, "gzip deflate init");
    Require(deflateBound_nid_postfix(&gzipDeflater, 1000) == 1025, "gzip default parameter bound");
    Require(deflateEnd_nid_postfix(&gzipDeflater) == Z_OK, "gzip deflate end");

    const auto expectGzipError = [](std::vector<std::uint8_t> input, const char* message) {
        GuestZStream gzipInflater{};
        Require(inflateInit2__nid_postfix(&gzipInflater, 31, "1.3.1", STREAM_SIZE) == Z_OK, "gzip inflate init");
        std::uint8_t sink[256];
        gzipInflater.next_in = input.data();
        gzipInflater.avail_in = static_cast<std::uint32_t>(input.size());
        gzipInflater.next_out = sink;
        gzipInflater.avail_out = sizeof(sink);
        Require(inflate_nid_postfix(&gzipInflater, Z_NO_FLUSH) == Z_DATA_ERROR, message);
        Require(gzipInflater.msg != nullptr && std::string(gzipInflater.msg) == message, message);
        Require(inflateEnd_nid_postfix(&gzipInflater) == Z_OK, "gzip inflate end");
    };
    const std::vector<std::uint8_t> emptyGzip{0x1f, 0x8b, 8, 0, 0, 0, 0, 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    auto badMethod = emptyGzip;
    badMethod[2] = 9;
    expectGzipError(badMethod, "unknown compression method");
    auto badFlags = emptyGzip;
    badFlags[3] = 0xe0;
    expectGzipError(badFlags, "unknown header flags set");
    auto badCrc = emptyGzip;
    badCrc[12] = 1;
    expectGzipError(badCrc, "incorrect data check");
    auto badLength = emptyGzip;
    badLength[16] = 1;
    expectGzipError(badLength, "incorrect length check");
    expectGzipError({0x78, 0x9c, 3, 0, 0, 0, 0, 1}, "incorrect header check");

    GuestZStream inflater{};
    Require(inflateInit2__nid_postfix(&inflater, 15, "1.3.1", STREAM_SIZE) == Z_OK, "inflate init");
    const std::uint8_t corrupt[] = {0x78, 0x9c, 0xff, 0xff};
    std::uint8_t out[64];
    inflater.next_in = corrupt;
    inflater.avail_in = sizeof(corrupt);
    inflater.next_out = out;
    inflater.avail_out = sizeof(out);
    Require(inflate_nid_postfix(&inflater, Z_NO_FLUSH) == Z_DATA_ERROR && inflater.msg != nullptr, "corrupt data");
    Require(inflateEnd_nid_postfix(&inflater) == Z_OK, "inflate end after error");
}

void StaleChecksum() {
    GuestZStream deflater{};
    deflater.adler = 0x100000001ULL;
    Require(deflateInit2__nid_postfix(&deflater, 6, Z_DEFLATED, 15, 8, 0, "1.3.1", STREAM_SIZE) == Z_OK, "deflate init over a stale checksum");
    Require(deflater.adler == 1, "deflate init replaces every checksum bit");
    Require(deflateEnd_nid_postfix(&deflater) == Z_OK, "deflate end");

    GuestZStream inflater{};
    inflater.adler = 0xFFFFFFFF00000001ULL;
    Require(inflateInit2__nid_postfix(&inflater, 15, "1.3.1", STREAM_SIZE) == Z_OK, "inflate init over a stale checksum");
    Require(inflater.adler == 1, "inflate init replaces every checksum bit");
    Require(inflateEnd_nid_postfix(&inflater) == Z_OK, "inflate end");
}

void Uncompress() {
    const auto data = Sample();
    GuestZStream deflater{};
    Require(deflateInit2__nid_postfix(&deflater, 9, Z_DEFLATED, 15, 8, 0, "1.3.1", STREAM_SIZE) == Z_OK, "deflate init");
    const auto compressed = Compress(deflater, data, 4096);
    Require(deflateEnd_nid_postfix(&deflater) == Z_OK, "deflate end");

    std::vector<std::uint8_t> output(data.size() + 16, 0xA5);
    std::uint64_t length = output.size();
    Require(uncompress_nid_postfix(output.data(), &length, compressed.data(), compressed.size()) == Z_OK, "uncompress");
    Require(length == data.size() && std::memcmp(output.data(), data.data(), data.size()) == 0, "uncompress data");

    length = 10;
    Require(uncompress_nid_postfix(output.data(), &length, compressed.data(), compressed.size()) == Z_BUF_ERROR, "small output");
    Require(length == 10, "small output length");

    length = 0;
    Require(uncompress_nid_postfix(output.data(), &length, compressed.data(), compressed.size()) == Z_DATA_ERROR, "zero output");
    Require(length == 0, "zero output length");

    const std::uint8_t empty[] = {0x78, 0x9c, 0x03, 0x00, 0x00, 0x00, 0x00, 0x01};
    Require(uncompress_nid_postfix(output.data(), &length, empty, sizeof(empty)) == Z_OK && length == 0, "empty stream");

    length = output.size();
    Require(uncompress_nid_postfix(output.data(), &length, compressed.data(), compressed.size() - 6) == Z_DATA_ERROR, "truncated");
    Require(length > 0 && length < data.size(), "truncated length");

    bool threw = false;
    try { uncompress_nid_postfix(output.data(), nullptr, compressed.data(), compressed.size()); } catch (const std::runtime_error&) { threw = true; }
    Require(threw, "null length throws");
}

}

int main() {
    for (const int windowBits : {15, -15, 9, 31}) {
        for (const std::uint32_t chunk : {1u, 7u, 4096u}) RoundTrip(windowBits, chunk);
    }
    Errors();
    StaleChecksum();
    Uncompress();
    return 0;
}
