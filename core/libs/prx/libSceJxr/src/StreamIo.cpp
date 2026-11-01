/* Adapted for AnyPS5 from image/sys/strcodec.c and jxrgluelib/JXRGlue.c of jxrlib
 * (https://github.com/4creators/jxrlib/tree/f7521879862b9085318e814c6157490dd9dbbdb4).
 * Altered: the functions take the guest LP64 WMPStream, BitIOInfo and SimpleBitIO
 * layouts, allocate and use files through the guest C library, apply shift counts
 * modulo 32 as x86 does, and throw where the original dereferences a null pointer.
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
#include "prx/libc/include/GuestArena.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

using Jxr::BitIo;
using Jxr::SimpleBitIo;
using Jxr::Stream;

constexpr std::int64_t Success = 0;
constexpr std::int64_t OutOfMemory = -101;
constexpr std::int64_t FileIo = -102;
constexpr std::int64_t BufferOverflow = -103;
constexpr std::size_t PacketLength = 4096;
constexpr int SeekSet = 0;

static_assert(sizeof(std::uintptr_t) == sizeof(std::size_t) && sizeof(std::size_t) == 8);

std::uint8_t* Data(const Stream& stream) {
    return reinterpret_cast<std::uint8_t*>(stream.state[0]);
}

void SetData(Stream& stream, std::uint8_t* data) {
    stream.state[0] = reinterpret_cast<std::uintptr_t>(data);
}

std::size_t& Size(Stream& stream) {
    return stream.state[1];
}

std::size_t& Current(Stream& stream) {
    return stream.state[2];
}

std::size_t& Count(Stream& stream) {
    return stream.state[3];
}

FileStream* File(const Stream& stream) {
    return reinterpret_cast<FileStream*>(stream.state[0]);
}

std::uint32_t ShiftLeft(std::uint32_t value, std::uint32_t count) {
    return value << (count & 31);
}

std::uint32_t ShiftRight(std::uint32_t value, std::uint32_t count) {
    return value >> (count & 31);
}

std::uint32_t SwapBytes(std::uint32_t value) {
    return (value << 24) | ((value << 8) & 0xFF0000u) | ((value >> 8) & 0xFF00u) | (value >> 24);
}

std::uint32_t LoadBigEndian(const std::uint8_t* address) {
    std::uint32_t value;
    std::memcpy(&value, address, sizeof(value));
    return SwapBytes(value);
}

std::uint8_t* Mask(std::uint8_t* pointer, std::int32_t mask) {
    const auto extended = static_cast<std::uintptr_t>(static_cast<std::intptr_t>(mask));
    return reinterpret_cast<std::uint8_t*>(reinterpret_cast<std::uintptr_t>(pointer) & extended);
}

std::int64_t Allocate(void** memory, std::size_t size) {
    if (!memory) APS5_INVALID_ARG_EX;
    *memory = calloc_nid_postfix(1, size);
    return *memory ? Success : OutOfMemory;
}

std::int64_t Release(void** memory) {
    if (!memory) APS5_INVALID_ARG_EX;
    if (*memory) {
        free_nid_postfix(*memory);
        *memory = nullptr;
    }
    return Success;
}

Stream& Require(Stream* stream) {
    if (!stream) APS5_INVALID_ARG_EX;
    return *stream;
}

BitIo& Require(BitIo* io) {
    if (!io) APS5_INVALID_ARG_EX;
    return *io;
}

SimpleBitIo& Require(SimpleBitIo* io) {
    if (!io) APS5_INVALID_ARG_EX;
    return *io;
}

std::int64_t APS5_VABI CloseWsMemory_nid_no_patch(Stream** stream) {
    return Release(reinterpret_cast<void**>(stream));
}

} // namespace

extern "C" {

std::int64_t APS5_VABI WMPAlloc_nid_postfix(void** memory, std::uint64_t size) {
    return Allocate(memory, size);
}

std::int64_t APS5_VABI WMPFree_nid_postfix(void** memory) {
    return Release(memory);
}

std::int64_t APS5_VABI PKAlloc_nid_postfix(void** memory, std::uint64_t size) {
    return Allocate(memory, size);
}

std::int64_t APS5_VABI PKAllocAligned_nid_postfix(void** memory, std::uint64_t size, std::uint64_t alignment) {
    if (!memory || alignment == 0) APS5_INVALID_ARG_EX;
    *memory = nullptr;
    if (size > std::numeric_limits<std::size_t>::max() - sizeof(void*) - (alignment - 1))
        throw std::runtime_error("PKAllocAligned: block size overflows");
    auto* original = static_cast<std::uint8_t*>(calloc_nid_postfix(1, size + sizeof(void*) + alignment - 1));
    if (!original) return OutOfMemory;
    std::size_t correction = alignment - reinterpret_cast<std::uintptr_t>(original) % alignment;
    if (correction < sizeof(void*)) correction += alignment;
    if (correction < sizeof(void*)) {
        free_nid_postfix(original);
        throw std::runtime_error("PKAllocAligned: the original pointer would be stored before the block");
    }
    std::uint8_t* aligned = original + correction;
    std::memcpy(aligned - sizeof(void*), &original, sizeof(void*));
    *memory = aligned;
    return Success;
}

std::int64_t APS5_VABI PKFreeAligned_nid_postfix(void** memory) {
    if (memory && *memory) {
        void* original;
        std::memcpy(&original, static_cast<std::uint8_t*>(*memory) - sizeof(void*), sizeof(void*));
        free_nid_postfix(original);
        *memory = nullptr;
    }
    return Success;
}

std::int32_t APS5_VABI EOSWS_Memory_nid_postfix(Stream* stream) {
    Stream& self = Require(stream);
    return Size(self) <= Current(self);
}

std::int64_t APS5_VABI ReadWS_Memory_nid_postfix(Stream* stream, void* buffer, std::uint64_t size) {
    Stream& self = Require(stream);
    if (Size(self) < Current(self)) return Success;
    if (Current(self) + size < Current(self)) return BufferOverflow;
    if (Size(self) < Current(self) + size) size = Size(self) - Current(self);
    std::memcpy(buffer, Data(self) + Current(self), size);
    Current(self) += size;
    return Success;
}

std::int64_t APS5_VABI WriteWS_Memory_nid_postfix(Stream* stream, const void* buffer, std::uint64_t size) {
    Stream& self = Require(stream);
    if (Current(self) + size < Current(self)) return BufferOverflow;
    if (Size(self) < Current(self) + size) return BufferOverflow;
    std::memcpy(Data(self) + Current(self), buffer, size);
    Current(self) += size;
    return Success;
}

std::int64_t APS5_VABI SetPosWS_Memory_nid_postfix(Stream* stream, std::uint64_t position) {
    Current(Require(stream)) = position;
    return Success;
}

std::int64_t APS5_VABI GetPosWS_Memory_nid_postfix(Stream* stream, std::uint64_t* position) {
    Stream& self = Require(stream);
    if (!position) APS5_INVALID_ARG_EX;
    *position = Current(self);
    return Success;
}

std::int64_t APS5_VABI CreateWS_Memory_nid_postfix(Stream** stream, void* buffer, std::uint64_t size) {
    const std::int64_t result = Allocate(reinterpret_cast<void**>(stream), sizeof(Stream));
    if (result < 0) return result;
    Stream& self = **stream;
    SetData(self, static_cast<std::uint8_t*>(buffer));
    Size(self) = size;
    Current(self) = 0;
    self.close = CloseWsMemory_nid_no_patch;
    self.end = EOSWS_Memory_nid_postfix;
    self.read = ReadWS_Memory_nid_postfix;
    self.write = WriteWS_Memory_nid_postfix;
    self.setPosition = SetPosWS_Memory_nid_postfix;
    self.getPosition = GetPosWS_Memory_nid_postfix;
    return Success;
}

std::int64_t APS5_VABI CloseWS_List_nid_postfix(Stream** stream) {
    if (!stream || !*stream) APS5_INVALID_ARG_EX;
    void* next;
    std::memcpy(&next, reinterpret_cast<std::uint8_t*>(*stream) + sizeof(Stream), sizeof(void*));
    while (next) {
        void* packet = next;
        std::memcpy(&next, packet, sizeof(void*));
        const std::int64_t result = Release(&packet);
        if (result < 0) return result;
    }
    return Release(reinterpret_cast<void**>(stream));
}

std::int64_t APS5_VABI ReadWS_List_nid_postfix(Stream* stream, void* buffer, std::uint64_t size) {
    Stream& self = Require(stream);
    if (Current(self) + size < Current(self)) return BufferOverflow;
    if (Size(self) < Current(self) + PacketLength * Count(self) + size)
        size = Size(self) - Current(self) - PacketLength * Count(self);
    auto* target = static_cast<std::uint8_t*>(buffer);
    while (size) {
        std::size_t chunk = PacketLength - Current(self);
        if (chunk > size) chunk = size;
        std::memcpy(target, Data(self) + Current(self), chunk);
        Current(self) += chunk;
        target += chunk;
        size -= chunk;
        if (Current(self) == PacketLength) {
            std::uint8_t* next;
            std::memcpy(&next, Data(self) - sizeof(void*), sizeof(void*));
            SetData(self, next + sizeof(void*));
            Current(self) = 0;
            ++Count(self);
        }
    }
    return Success;
}

std::int64_t APS5_VABI WriteWS_List_nid_postfix(Stream* stream, const void* buffer, std::uint64_t size) {
    Stream& self = Require(stream);
    if (Current(self) + size < Current(self)) return BufferOverflow;
    if (Size(self) < Current(self) + size) return BufferOverflow;
    const auto* source = static_cast<const std::uint8_t*>(buffer);
    while (size) {
        std::size_t chunk = PacketLength - Current(self);
        if (chunk > size) chunk = size;
        std::memcpy(Data(self) + Current(self), source, chunk);
        Current(self) += chunk;
        source += chunk;
        size -= chunk;
        if (Current(self) == PacketLength) {
            void* packet = nullptr;
            const std::int64_t result = Allocate(&packet, PacketLength + sizeof(void*));
            if (result < 0) return result;
            std::memcpy(Data(self) - sizeof(void*), &packet, sizeof(void*));
            SetData(self, static_cast<std::uint8_t*>(packet) + sizeof(void*));
            Size(self) += PacketLength;
            Current(self) = 0;
            ++Count(self);
        }
    }
    return Success;
}

std::int64_t APS5_VABI SetPosWS_List_nid_postfix(Stream* stream, std::uint64_t position) {
    Stream& self = Require(stream);
    auto* packet = reinterpret_cast<std::uint8_t*>(&self) + sizeof(Stream);
    Current(self) = 0;
    Count(self) = 0;
    while (position >= PacketLength && packet) {
        std::memcpy(&packet, packet, sizeof(void*));
        position -= PacketLength;
        ++Count(self);
    }
    if (!packet) return Success;
    Current(self) = position;
    SetData(self, packet + sizeof(void*));
    return Success;
}

std::int64_t APS5_VABI GetPosWS_List_nid_postfix(Stream* stream, std::uint64_t* position) {
    Stream& self = Require(stream);
    if (!position) APS5_INVALID_ARG_EX;
    *position = Current(self) + PacketLength * Count(self);
    return Success;
}

std::int64_t APS5_VABI CreateWS_List_nid_postfix(Stream** stream) {
    const std::int64_t result = Allocate(reinterpret_cast<void**>(stream), sizeof(Stream) + PacketLength + sizeof(void*));
    if (result < 0) return result;
    Stream& self = **stream;
    SetData(self, reinterpret_cast<std::uint8_t*>(&self) + sizeof(Stream) + sizeof(void*));
    std::memset(Data(self) - sizeof(void*), 0, sizeof(void*));
    Size(self) = PacketLength;
    Current(self) = 0;
    Count(self) = 0;
    self.close = CloseWS_List_nid_postfix;
    self.end = nullptr;
    self.read = ReadWS_List_nid_postfix;
    self.write = WriteWS_List_nid_postfix;
    self.setPosition = SetPosWS_List_nid_postfix;
    self.getPosition = GetPosWS_List_nid_postfix;
    return Success;
}

std::int32_t APS5_VABI EOSWS_File_nid_postfix(Stream* stream) {
    return feof_nid_postfix(File(Require(stream)));
}

std::int64_t APS5_VABI ReadWS_File_nid_postfix(Stream* stream, void* buffer, std::uint64_t size) {
    return fread_nid_postfix(buffer, size, 1, File(Require(stream))) == 1 ? Success : FileIo;
}

std::int64_t APS5_VABI WriteWS_File_nid_postfix(Stream* stream, const void* buffer, std::uint64_t size) {
    FileStream* file = File(Require(stream));
    if (size != 0 && fwrite_nid_postfix(buffer, size, 1, file) != 1) return FileIo;
    return Success;
}

std::int64_t APS5_VABI SetPosWS_File_nid_postfix(Stream* stream, std::uint64_t position) {
    return fseek_nid_postfix(File(Require(stream)), static_cast<std::int64_t>(position), SeekSet) != 0 ? FileIo : Success;
}

std::int64_t APS5_VABI GetPosWS_File_nid_postfix(Stream* stream, std::uint64_t* position) {
    FileStream* file = File(Require(stream));
    if (!position) APS5_INVALID_ARG_EX;
    const std::int64_t offset = ftell_nid_postfix(file);
    if (offset == -1) return FileIo;
    *position = static_cast<std::uint64_t>(offset);
    return Success;
}

std::uint8_t APS5_VABI dquantBits_nid_postfix(std::uint8_t quantizers) {
    return quantizers < 2 ? 0 : quantizers < 4 ? 1 : quantizers < 6 ? 2 : quantizers < 10 ? 3 : 4;
}

std::uint32_t APS5_VABI peekBit16_nid_postfix(BitIo* io, std::uint32_t bits) {
    return ShiftRight(Require(io).accumulator, 32 - bits);
}

std::uint32_t APS5_VABI flushBit16_nid_postfix(BitIo* io, std::uint32_t bits) {
    BitIo& self = Require(io);
    self.bitsUsed += bits;
    self.current = Mask(self.current + (self.bitsUsed >> 3), self.mask);
    self.bitsUsed &= 15;
    self.accumulator = ShiftLeft(LoadBigEndian(self.current), self.bitsUsed);
    return 0;
}

std::uint32_t APS5_VABI getBit16_nid_postfix(BitIo* io, std::uint32_t bits) {
    const std::uint32_t value = peekBit16_nid_postfix(io, bits);
    flushBit16_nid_postfix(io, bits);
    return value;
}

std::uint32_t APS5_VABI getBool16_nid_postfix(BitIo* io) {
    return getBit16_nid_postfix(io, 1);
}

std::int32_t APS5_VABI getBit16s_nid_postfix(BitIo* io, std::uint32_t bits) {
    const std::uint32_t value = peekBit16_nid_postfix(io, bits + 1);
    if (value < 2) {
        flushBit16_nid_postfix(io, bits);
        return 0;
    }
    flushBit16_nid_postfix(io, bits + 1);
    const auto magnitude = static_cast<std::int32_t>(value >> 1);
    return (value & 1) ? -magnitude : magnitude;
}

std::uint32_t APS5_VABI getBit32_nid_postfix(BitIo* io, std::uint32_t bits) {
    std::uint32_t value = 0;
    if (16 < bits) {
        value = getBit16_nid_postfix(io, 16);
        bits -= 16;
        value = ShiftLeft(value, bits);
    }
    return value | getBit16_nid_postfix(io, bits);
}

std::uint32_t APS5_VABI flushToByte_nid_postfix(BitIo* io) {
    return flushBit16_nid_postfix(io, (16 - Require(io).bitsUsed) & 7);
}

void APS5_VABI putBit16z_nid_postfix(BitIo* io, std::uint32_t value, std::uint32_t bits) {
    BitIo& self = Require(io);
    self.accumulator = ShiftLeft(self.accumulator, bits) | value;
    self.bitsUsed += bits;
    const auto halfword = static_cast<std::uint16_t>(SwapBytes(ShiftLeft(self.accumulator, 32 - self.bitsUsed)));
    std::memcpy(self.current, &halfword, sizeof(halfword));
    self.current = Mask(self.current + ((self.bitsUsed >> 3) & 2), self.mask);
    self.bitsUsed &= 15;
}

void APS5_VABI putBit16_nid_postfix(BitIo* io, std::uint32_t value, std::uint32_t bits) {
    putBit16z_nid_postfix(io, value & ~ShiftLeft(0xFFFFFFFFu, bits), bits);
}

void APS5_VABI putBit32_nid_postfix(BitIo* io, std::uint32_t value, std::uint32_t bits) {
    if (16 < bits) {
        putBit16_nid_postfix(io, ShiftRight(value, bits - 16), 16);
        bits -= 16;
    }
    putBit16_nid_postfix(io, value, bits);
}

void APS5_VABI fillToByte_nid_postfix(BitIo* io) {
    putBit16z_nid_postfix(io, 0, (16 - Require(io).bitsUsed) & 7);
}

std::uint32_t APS5_VABI getBit16_S_nid_postfix(void* codec, BitIo* io, std::uint32_t bits) {
    const std::uint32_t value = getBit16_nid_postfix(io, bits);
    readIS_nid_postfix(codec, io);
    return value;
}

std::uint32_t APS5_VABI getSizeRead_nid_postfix(BitIo* io) {
    BitIo& self = Require(io);
    const auto cached = reinterpret_cast<std::uintptr_t>(self.start) + PacketLength * 2 - reinterpret_cast<std::uintptr_t>(self.current);
    return static_cast<std::uint32_t>(cached) - self.bitsUsed / 8;
}

std::uint32_t APS5_VABI getSizeWrite_nid_postfix(BitIo* io) {
    BitIo& self = Require(io);
    const std::uintptr_t start = reinterpret_cast<std::uintptr_t>(self.start);
    const std::uintptr_t current = reinterpret_cast<std::uintptr_t>(self.current);
    return static_cast<std::uint32_t>(current + (start <= current ? 0 : PacketLength * 2) - start) + self.bitsUsed / 8;
}

std::uint32_t APS5_VABI getPosRead_nid_postfix(BitIo* io) {
    BitIo& self = Require(io);
    const std::size_t cached = reinterpret_cast<std::uintptr_t>(self.start) + PacketLength * 2 - reinterpret_cast<std::uintptr_t>(self.current) - self.bitsUsed / 8;
    return static_cast<std::uint32_t>(self.referenceOffset - cached);
}

std::int64_t APS5_VABI attach_SB_nid_postfix(SimpleBitIo* io, Stream* stream) {
    SimpleBitIo& self = Require(io);
    self.stream = stream;
    self.bytesRead = 0;
    self.accumulator = 0;
    self.bitsLeft = 0;
    return Success;
}

std::uint32_t APS5_VABI getBit32_SB_nid_postfix(SimpleBitIo* io, std::uint32_t bits) {
    SimpleBitIo& self = Require(io);
    std::uint32_t value = 0;
    while (self.bitsLeft < bits) {
        value = ShiftLeft(value, self.bitsLeft);
        value |= ShiftRight(self.accumulator, 8 - self.bitsLeft);
        bits -= self.bitsLeft;
        Stream& stream = Require(self.stream);
        if (!stream.read) APS5_INVALID_ARG_EX;
        stream.read(&stream, &self.accumulator, 1);
        ++self.bytesRead;
        self.bitsLeft = 8;
    }
    value = ShiftLeft(value, bits);
    value |= ShiftRight(self.accumulator, 8 - bits);
    self.accumulator = static_cast<std::uint8_t>(ShiftLeft(self.accumulator, bits));
    self.bitsLeft -= bits;
    return value;
}

void APS5_VABI flushToByte_SB_nid_postfix(SimpleBitIo* io) {
    SimpleBitIo& self = Require(io);
    self.accumulator = 0;
    self.bitsLeft = 0;
}

std::uint32_t APS5_VABI getByteRead_SB_nid_postfix(SimpleBitIo* io) {
    return Require(io).bytesRead;
}

std::int64_t APS5_VABI detach_SB_nid_postfix(SimpleBitIo* io) {
    Require(io).stream = nullptr;
    return Success;
}

std::int64_t APS5_VABI attachISRead_nid_postfix(BitIo* io, Stream* stream, void*) {
    BitIo& self = Require(io);
    Stream& source = Require(stream);
    if (!source.getPosition || !source.setPosition || !source.read) APS5_INVALID_ARG_EX;
    source.getPosition(&source, &self.referenceOffset);
    self.start = reinterpret_cast<std::uint8_t*>(&self) - PacketLength * 2;
    self.current = self.start;
    source.setPosition(&source, self.referenceOffset);
    {
        GuestArena::HostWrite access(self.start, PacketLength * 2);
        if (!access.Open()) APS5_INVALID_ARG_EX;
        source.read(&source, self.start, PacketLength * 2);
    }
    self.referenceOffset += PacketLength * 2;
    self.accumulator = LoadBigEndian(self.start);
    self.bitsUsed = 0;
    self.mask = static_cast<std::int32_t>(~static_cast<std::uint32_t>(PacketLength * 2) & ~1u);
    self.stream = &source;
    return Success;
}

std::int64_t APS5_VABI detachISRead_nid_postfix(void* codec, BitIo* io) {
    BitIo& self = Require(io);
    Stream& source = Require(self.stream);
    if (!source.setPosition) APS5_INVALID_ARG_EX;
    flushToByte_nid_postfix(io);
    const std::int64_t result = readIS_nid_postfix(codec, io);
    if (result < 0) return result;
    const std::size_t remaining = reinterpret_cast<std::uintptr_t>(self.start) + PacketLength * 2 -
        (reinterpret_cast<std::uintptr_t>(self.current) + self.bitsUsed / 8);
    source.setPosition(&source, self.referenceOffset - remaining);
    self.stream = nullptr;
    return Success;
}

std::int64_t APS5_VABI attachISWrite_nid_postfix(BitIo* io, Stream* stream) {
    BitIo& self = Require(io);
    Stream& target = Require(stream);
    if (!target.getPosition) APS5_INVALID_ARG_EX;
    target.getPosition(&target, &self.referenceOffset);
    self.start = reinterpret_cast<std::uint8_t*>(&self) - PacketLength * 2;
    self.current = self.start;
    self.accumulator = 0;
    self.bitsUsed = 0;
    self.mask = static_cast<std::int32_t>(~static_cast<std::uint32_t>(PacketLength * 2));
    self.stream = &target;
    return Success;
}

std::int64_t APS5_VABI detachISWrite_nid_postfix(void* codec, BitIo* io) {
    BitIo& self = Require(io);
    std::int64_t result = writeIS_nid_postfix(codec, io);
    if (result < 0) return result;
    Stream& target = Require(self.stream);
    if (!target.write) APS5_INVALID_ARG_EX;
    const std::size_t size = reinterpret_cast<std::uintptr_t>(self.current) + self.bitsUsed / 8 - reinterpret_cast<std::uintptr_t>(self.start);
    result = target.write(&target, self.start, size);
    if (result < 0) return result;
    self.stream = nullptr;
    return result;
}

}
