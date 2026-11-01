#include "prx/libSceJxr/libSceJxr.hpp"
#include "prx/libc/include/Api.hpp"
#include <algorithm>
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
int JxrReferenceRead_nid_no_patch(const std::uint8_t* data, std::size_t size, std::size_t offset, const std::uint32_t* script, std::size_t steps, std::uint32_t* results, std::uint64_t* finalPosition);
int JxrReferenceWrite_nid_no_patch(const std::uint32_t* script, std::size_t steps, std::uint32_t* results, std::uint8_t* output, std::size_t capacity, std::size_t prefix, std::uint64_t* written);
int JxrReferenceSimple_nid_no_patch(const std::uint8_t* data, std::size_t size, const std::uint32_t* widths, std::size_t steps, std::uint32_t* results, std::uint32_t* bytesRead);
int JxrReferenceList_nid_no_patch(const std::uint8_t* data, const std::uint64_t* writes, std::size_t writeCount, const std::uint64_t* seeks, const std::uint64_t* reads, std::size_t readCount, std::uint8_t* readBack, std::uint64_t* positions, std::int64_t* errors);
std::uint8_t JxrReferenceDquant_nid_no_patch(std::uint8_t quantizers);
}

namespace {

constexpr std::int64_t Success = 0;
constexpr std::int64_t FileIo = -102;
constexpr std::int64_t BufferOverflow = -103;
constexpr std::size_t PacketLength = 4096;

std::uint8_t guestReadStorage[16384 * 3];
std::uint8_t guestWriteStorage[16384 * 3];

std::uint8_t* AlignedRing(std::uint8_t* storage) {
    return reinterpret_cast<std::uint8_t*>((reinterpret_cast<std::uintptr_t>(storage) + 16383) & ~std::uintptr_t{16383});
}

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "JXR stream I/O: %s\n", message);
    std::abort();
}

template<typename TAction>
bool Rejects(TAction action) {
    try { action(); } catch (const std::invalid_argument&) { return true; }
    return false;
}

template<typename TAction>
bool Fails(TAction action) {
    try { action(); } catch (const std::runtime_error&) { return true; }
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

std::vector<std::uint8_t> Bytes(std::size_t size, std::uint64_t seed) {
    Random random{seed};
    std::vector<std::uint8_t> data(size);
    for (auto& value : data) value = static_cast<std::uint8_t>(random.Next());
    return data;
}

Jxr::BitIo* RingInfo(std::uint8_t* ring) {
    return reinterpret_cast<Jxr::BitIo*>(ring + PacketLength * 2);
}

int GuestRead(const std::vector<std::uint8_t>& data, std::size_t offset, const std::vector<std::uint32_t>& script, std::vector<std::uint32_t>& results, std::uint64_t& finalPosition) {
    Jxr::Stream* stream = nullptr;
    std::uint8_t* ring = AlignedRing(guestReadStorage);
    Jxr::BitIo* io = RingInfo(ring);
    if (CreateWS_Memory_nid_postfix(&stream, const_cast<std::uint8_t*>(data.data()), data.size()) != Success) return -1;
    stream->setPosition(stream, offset);
    std::memset(ring, 0, 16384 * 2);
    if (attachISRead_nid_postfix(io, stream, nullptr) != Success) return -2;
    for (std::size_t index = 0; index < results.size(); ++index) {
        const std::uint32_t operation = script[index * 2];
        const std::uint32_t argument = script[index * 2 + 1];
        std::uint32_t value = 0;
        switch (operation) {
        case 0: value = peekBit16_nid_postfix(io, argument); break;
        case 1: value = flushBit16_nid_postfix(io, argument); break;
        case 2: value = getBit16_nid_postfix(io, argument); break;
        case 3: value = getBool16_nid_postfix(io); break;
        case 4: value = static_cast<std::uint32_t>(getBit16s_nid_postfix(io, argument)); break;
        case 5: value = getBit32_nid_postfix(io, argument); break;
        case 6: value = flushToByte_nid_postfix(io); break;
        case 7: value = static_cast<std::uint32_t>(readIS_nid_postfix(nullptr, io)); break;
        case 8: value = getPosRead_nid_postfix(io); break;
        case 9: value = getSizeRead_nid_postfix(io); break;
        case 10: value = getBit16_S_nid_postfix(nullptr, io, argument); break;
        default: return -3;
        }
        results[index] = value;
    }
    if (detachISRead_nid_postfix(nullptr, io) != Success) return -4;
    Require(io->stream == nullptr, "detachISRead clears the stream");
    std::uint64_t position = 0;
    stream->getPosition(stream, &position);
    finalPosition = position;
    stream->close(&stream);
    Require(stream == nullptr, "memory stream close clears the pointer");
    return 0;
}

int GuestWrite(const std::vector<std::uint32_t>& script, std::vector<std::uint32_t>& results, std::vector<std::uint8_t>& output, std::size_t prefix, std::uint64_t& written) {
    Jxr::Stream* stream = nullptr;
    std::uint8_t* ring = AlignedRing(guestWriteStorage);
    Jxr::BitIo* io = RingInfo(ring);
    if (CreateWS_Memory_nid_postfix(&stream, output.data(), output.size()) != Success) return -1;
    stream->setPosition(stream, prefix);
    std::memset(ring, 0, 16384 * 2);
    if (attachISWrite_nid_postfix(io, stream) != Success) return -2;
    for (std::size_t index = 0; index < results.size(); ++index) {
        const std::uint32_t operation = script[index * 3];
        const std::uint32_t first = script[index * 3 + 1];
        const std::uint32_t second = script[index * 3 + 2];
        std::uint32_t value = 0;
        switch (operation) {
        case 0: putBit16_nid_postfix(io, first, second); break;
        case 1: putBit16z_nid_postfix(io, first, second); break;
        case 2: putBit32_nid_postfix(io, first, second); break;
        case 3: fillToByte_nid_postfix(io); break;
        case 4: value = static_cast<std::uint32_t>(writeIS_nid_postfix(nullptr, io)); break;
        case 5: value = getSizeWrite_nid_postfix(io); break;
        default: return -3;
        }
        results[index] = value;
    }
    fillToByte_nid_postfix(io);
    if (detachISWrite_nid_postfix(nullptr, io) != Success) return -4;
    std::uint64_t position = 0;
    stream->getPosition(stream, &position);
    written = position;
    stream->close(&stream);
    return 0;
}

void BitReads() {
    const auto data = Bytes(65536, 0x1234567);
    for (std::uint64_t seed = 1; seed <= 6; ++seed) {
        Random random{seed * 7919};
        std::vector<std::uint32_t> script;
        for (int step = 0; step < 25000; ++step) {
            const std::uint32_t operation = random.Below(10);
            std::uint32_t argument = 0;
            const std::uint32_t chosen = operation == 7 ? 10 : operation;
            if (chosen == 0 || chosen == 1 || chosen == 2 || chosen == 10) argument = random.Below(17);
            else if (chosen == 4) argument = random.Below(16);
            else if (chosen == 5) argument = random.Below(33);
            script.push_back(chosen);
            script.push_back(argument);
            script.push_back(7);
            script.push_back(0);
        }
        const std::size_t steps = script.size() / 2;
        const std::size_t offset = seed * 3;
        std::vector<std::uint32_t> expected(steps);
        std::vector<std::uint32_t> actual(steps);
        std::uint64_t expectedPosition = 0;
        std::uint64_t actualPosition = 0;
        Require(JxrReferenceRead_nid_no_patch(data.data(), data.size(), offset, script.data(), steps, expected.data(), &expectedPosition) == 0, "reference read script");
        Require(GuestRead(data, offset, script, actual, actualPosition) == 0, "guest read script");
        Require(actual == expected, "bit reads match jxrlib");
        Require(actualPosition == expectedPosition, "detachISRead stream position matches jxrlib");
    }
}

void BitWrites() {
    for (std::uint64_t seed = 1; seed <= 6; ++seed) {
        Random random{seed * 104729};
        std::vector<std::uint32_t> script;
        for (int step = 0; step < 20000; ++step) {
            const std::uint32_t operation = random.Below(6);
            std::uint32_t first = random.Next();
            std::uint32_t second = 0;
            if (operation == 0) second = random.Below(17);
            else if (operation == 1) {
                second = random.Below(17);
                first = second == 0 ? 0 : first & (0xFFFFFFFFu >> (32 - second));
            } else if (operation == 2) second = random.Below(33);
            else if (operation == 4) first = 0;
            script.insert(script.end(), {operation == 4 ? 5u : operation, first, second});
            script.insert(script.end(), {4u, 0u, 0u});
        }
        const std::size_t steps = script.size() / 3;
        const std::size_t prefix = seed;
        std::vector<std::uint32_t> expected(steps);
        std::vector<std::uint32_t> actual(steps);
        std::vector<std::uint8_t> expectedOutput(65536, 0xA5);
        std::vector<std::uint8_t> actualOutput(65536, 0xA5);
        std::uint64_t expectedWritten = 0;
        std::uint64_t actualWritten = 0;
        Require(JxrReferenceWrite_nid_no_patch(script.data(), steps, expected.data(), expectedOutput.data(), expectedOutput.size(), prefix, &expectedWritten) == 0, "reference write script");
        Require(GuestWrite(script, actual, actualOutput, prefix, actualWritten) == 0, "guest write script");
        Require(actual == expected, "bit write queries match jxrlib");
        Require(actualWritten == expectedWritten && actualWritten > PacketLength * 3, "written length matches jxrlib");
        Require(actualOutput == expectedOutput, "written bytes match jxrlib");
    }
}

void SimpleBits() {
    const auto data = Bytes(8192, 0xBEEF);
    Random random{77};
    std::vector<std::uint32_t> widths;
    for (int step = 0; step < 1500; ++step) widths.push_back(random.Below(20) == 0 ? 99 : random.Below(33));
    std::vector<std::uint32_t> expected(widths.size());
    std::vector<std::uint32_t> actual(widths.size());
    std::uint32_t expectedRead = 0;
    Require(JxrReferenceSimple_nid_no_patch(data.data(), data.size(), widths.data(), widths.size(), expected.data(), &expectedRead) == 0, "reference simple reads");
    Jxr::Stream* stream = nullptr;
    Require(CreateWS_Memory_nid_postfix(&stream, const_cast<std::uint8_t*>(data.data()), data.size()) == Success, "simple stream");
    Jxr::SimpleBitIo io{};
    Require(attach_SB_nid_postfix(&io, stream) == Success && io.stream == stream, "attach_SB");
    for (std::size_t index = 0; index < widths.size(); ++index) {
        if (widths[index] == 99) {
            flushToByte_SB_nid_postfix(&io);
            actual[index] = 0;
        } else {
            actual[index] = getBit32_SB_nid_postfix(&io, widths[index]);
        }
    }
    Require(actual == expected, "simple bit reads match jxrlib");
    Require(getByteRead_SB_nid_postfix(&io) == expectedRead, "simple byte count matches jxrlib");
    flushToByte_SB_nid_postfix(&io);
    Require(detach_SB_nid_postfix(&io) == Success && io.stream == nullptr, "detach_SB");
    stream->close(&stream);
}

void Lists() {
    const auto data = Bytes(30000, 0xC0FFEE);
    Random random{4242};
    std::vector<std::uint64_t> writes;
    std::uint64_t total = 0;
    while (total < 21000) {
        const std::uint64_t size = std::min<std::uint64_t>(random.Below(4) == 0 ? PacketLength : random.Below(PacketLength) + 1, 21000 - total);
        writes.push_back(size);
        total += size;
    }
    writes.push_back(total + PacketLength * 8);
    std::vector<std::uint64_t> seeks;
    std::vector<std::uint64_t> reads;
    std::uint64_t readTotal = 0;
    for (int step = 0; step < 200; ++step) {
        const std::uint64_t seek = random.Below(static_cast<std::uint32_t>(total));
        const std::uint64_t size = random.Below(static_cast<std::uint32_t>(std::min<std::uint64_t>(total - seek, 9000) + 1));
        seeks.push_back(seek);
        reads.push_back(size);
        readTotal += size;
    }
    std::vector<std::uint8_t> expectedBack(readTotal);
    std::vector<std::uint64_t> expectedPositions(reads.size());
    std::vector<std::int64_t> expectedErrors(writes.size() + reads.size());
    Require(JxrReferenceList_nid_no_patch(data.data(), writes.data(), writes.size(), seeks.data(), reads.data(), reads.size(), expectedBack.data(), expectedPositions.data(), expectedErrors.data()) == 0, "reference list");

    Jxr::Stream* stream = nullptr;
    Require(CreateWS_List_nid_postfix(&stream) == Success && stream->end == nullptr, "CreateWS_List");
    std::vector<std::uint8_t> actualBack(readTotal);
    std::vector<std::uint64_t> actualPositions(reads.size());
    std::vector<std::int64_t> actualErrors(writes.size() + reads.size());
    std::size_t offset = 0;
    for (std::size_t index = 0; index < writes.size(); ++index) {
        actualErrors[index] = stream->write(stream, data.data() + std::min(offset, data.size() - 1), writes[index]);
        offset += writes[index];
    }
    std::size_t cursor = 0;
    for (std::size_t index = 0; index < reads.size(); ++index) {
        stream->setPosition(stream, seeks[index]);
        actualErrors[writes.size() + index] = stream->read(stream, actualBack.data() + cursor, reads[index]);
        cursor += reads[index];
        stream->getPosition(stream, &actualPositions[index]);
    }
    Require(actualErrors == expectedErrors, "list stream results match jxrlib");
    Require(std::all_of(actualErrors.begin(), actualErrors.begin() + static_cast<std::ptrdiff_t>(writes.size() - 1), [](std::int64_t error) { return error == Success; }), "packet-sized list writes succeed");
    Require(actualErrors[writes.size() - 1] == BufferOverflow, "list write beyond its capacity overflows");
    std::size_t checked = 0;
    for (std::size_t index = 0; index < reads.size(); ++index) {
        Require(std::equal(actualBack.begin() + static_cast<std::ptrdiff_t>(checked), actualBack.begin() + static_cast<std::ptrdiff_t>(checked + reads[index]),
            data.begin() + static_cast<std::ptrdiff_t>(seeks[index])), "list stream reads return the written bytes");
        Require(actualPositions[index] == seeks[index] + reads[index], "list stream position follows the read");
        checked += reads[index];
    }
    Require(actualBack == expectedBack, "list stream reads match jxrlib");
    Require(actualPositions == expectedPositions, "list stream positions match jxrlib");
    Require(CloseWS_List_nid_postfix(&stream) == Success && stream == nullptr, "CloseWS_List");
    Require(Rejects([] { CloseWS_List_nid_postfix(nullptr); }), "null list close throws");
}

void Memory() {
    std::uint8_t buffer[16] = {};
    Jxr::Stream* stream = nullptr;
    Require(CreateWS_Memory_nid_postfix(&stream, buffer, sizeof(buffer)) == Success, "CreateWS_Memory");
    Require(stream->end == EOSWS_Memory_nid_postfix && stream->read == ReadWS_Memory_nid_postfix && stream->memory == 0, "memory stream methods");
    const std::uint8_t text[] = "0123456789ABCDEF";
    Require(stream->write(stream, text, 10) == Success, "memory write");
    Require(stream->write(stream, text, 7) == BufferOverflow, "memory write overflow");
    Require(stream->write(stream, text, ~std::uint64_t{0}) == BufferOverflow, "memory write wraps");
    std::uint64_t position = 0;
    Require(stream->getPosition(stream, &position) == Success && position == 10, "memory position after write");
    Require(stream->setPosition(stream, 8) == Success && stream->end(stream) == 0, "memory seek");
    std::uint8_t out[16] = {};
    Require(stream->read(stream, out, 16) == Success && std::memcmp(out, "89", 2) == 0, "memory read is clamped");
    Require(stream->end(stream) != 0, "memory end of stream");
    Require(stream->setPosition(stream, 40) == Success && stream->read(stream, out, 4) == Success, "memory read past the end");
    stream->getPosition(stream, &position);
    Require(position == 40, "memory read past the end keeps the position");
    Require(stream->setPosition(stream, 4) == Success && stream->read(stream, out, ~std::uint64_t{0}) == BufferOverflow, "memory read wraps");
    stream->close(&stream);
}

void Files() {
    const auto path = (std::filesystem::temp_directory_path() / "anyps5-jxr-stream-io.bin").string();
    FileStream* file = fopen_nid_postfix(path.c_str(), "wb+");
    Require(file != nullptr, "temporary file");
    Jxr::Stream stream{};
    stream.state[0] = reinterpret_cast<std::uintptr_t>(file);
    Require(WriteWS_File_nid_postfix(&stream, "hello", 5) == Success, "file write");
    Require(WriteWS_File_nid_postfix(&stream, nullptr, 0) == Success, "empty file write");
    std::uint64_t position = 0;
    Require(GetPosWS_File_nid_postfix(&stream, &position) == Success && position == 5, "file position");
    Require(SetPosWS_File_nid_postfix(&stream, 1) == Success, "file seek");
    char text[8] = {};
    Require(ReadWS_File_nid_postfix(&stream, text, 4) == Success && std::memcmp(text, "ello", 4) == 0, "file read");
    Require(EOSWS_File_nid_postfix(&stream) == 0, "file not at end");
    Require(ReadWS_File_nid_postfix(&stream, text, 0) == FileIo, "empty file read fails as fread does");
    Require(ReadWS_File_nid_postfix(&stream, text, 1) == FileIo && EOSWS_File_nid_postfix(&stream) != 0, "file read past the end");
    Require(fclose_nid_postfix(file) == 0, "close temporary file");
    std::remove(path.c_str());
}

void Allocators() {
    void* memory = nullptr;
    Require(WMPAlloc_nid_postfix(&memory, 64) == Success && memory, "WMPAlloc");
    Require(std::all_of(static_cast<std::uint8_t*>(memory), static_cast<std::uint8_t*>(memory) + 64, [](std::uint8_t value) { return value == 0; }), "WMPAlloc zeroes");
    Require(WMPFree_nid_postfix(&memory) == Success && memory == nullptr, "WMPFree");
    Require(WMPFree_nid_postfix(&memory) == Success, "WMPFree of null");
    Require(PKAlloc_nid_postfix(&memory, 32) == Success && memory, "PKAlloc");
    Require(PKFree_nid_postfix(&memory) == Success && memory == nullptr, "PKFree");
    for (const std::uint64_t alignment : {4u, 7u, 8u, 16u, 24u, 128u, 4096u}) {
        Require(PKAllocAligned_nid_postfix(&memory, 100, alignment) == Success && memory, "PKAllocAligned");
        Require(reinterpret_cast<std::uintptr_t>(memory) % alignment == 0, "PKAllocAligned alignment");
        Require(std::all_of(static_cast<std::uint8_t*>(memory), static_cast<std::uint8_t*>(memory) + 100, [](std::uint8_t value) { return value == 0; }), "PKAllocAligned zeroes");
        Require(PKFreeAligned_nid_postfix(&memory) == Success && memory == nullptr, "PKFreeAligned");
    }
    for (const std::uint64_t alignment : {1u, 2u}) {
        Require(Fails([&] { PKAllocAligned_nid_postfix(&memory, 100, alignment); }), "alignment that would underflow the block throws");
        Require(memory == nullptr, "failed PKAllocAligned leaves null");
    }
    Require(PKFreeAligned_nid_postfix(nullptr) == Success && PKFreeAligned_nid_postfix(&memory) == Success, "PKFreeAligned of null");
    Require(Rejects([] { WMPAlloc_nid_postfix(nullptr, 1); }), "WMPAlloc null target throws");
    Require(Rejects([] { WMPFree_nid_postfix(nullptr); }), "WMPFree null target throws");
    Require(Rejects([&] { PKAllocAligned_nid_postfix(&memory, 1, 0); }), "zero alignment throws");
    for (int value = 0; value < 256; ++value)
        Require(dquantBits_nid_postfix(static_cast<std::uint8_t>(value)) == JxrReferenceDquant_nid_no_patch(static_cast<std::uint8_t>(value)), "dquantBits matches jxrlib");
}

}

int main() {
    BitReads();
    BitWrites();
    SimpleBits();
    Lists();
    Memory();
    Files();
    Allocators();
    return 0;
}
