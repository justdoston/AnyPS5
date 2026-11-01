#include "strcodec.h"
#include "JXRGlue.h"
#include <stdint.h>
#include <string.h>

_Static_assert(sizeof(BitIOInfo) == 48, "BitIOInfo size");
_Static_assert(sizeof(SimpleBitIO) == 24, "SimpleBitIO size");

static U8 readStorage[16384 * 3];
static U8 writeStorage[16384 * 3];

static U8* AlignedRing(U8* storage) {
    return (U8*)(((uintptr_t)storage + 16383) & ~(uintptr_t)16383);
}

static BitIOInfo* RingInfo(U8* ring) {
    return (BitIOInfo*)(ring + PACKETLENGTH * 2);
}

int JxrReferenceRead_nid_no_patch(const uint8_t* data, size_t size, size_t offset, const uint32_t* script, size_t steps, uint32_t* results, uint64_t* finalPosition) {
    struct WMPStream* stream = NULL;
    U8* readRing = AlignedRing(readStorage);
    BitIOInfo* io = RingInfo(readRing);
    size_t index;
    if (CreateWS_Memory(&stream, (void*)data, size) != WMP_errSuccess) return -1;
    stream->SetPos(stream, offset);
    memset(readRing, 0, 16384 * 2);
    if (attachISRead(io, stream, NULL) != WMP_errSuccess) return -2;
    for (index = 0; index < steps; ++index) {
        const uint32_t operation = script[index * 2];
        const uint32_t argument = script[index * 2 + 1];
        uint32_t value = 0;
        switch (operation) {
        case 0: value = peekBit16(io, argument); break;
        case 1: value = flushBit16(io, argument); break;
        case 2: value = getBit16(io, argument); break;
        case 3: value = getBool16(io); break;
        case 4: value = (uint32_t)getBit16s(io, argument); break;
        case 5: value = getBit32(io, argument); break;
        case 6: value = flushToByte(io); break;
        case 7: value = (uint32_t)readIS(NULL, io); break;
        case 8: value = getPosRead(io); break;
        case 9: value = getSizeRead(io); break;
        case 10: value = getBit16_S(NULL, io, argument); break;
        default: return -3;
        }
        results[index] = value;
    }
    if (detachISRead(NULL, io) != WMP_errSuccess) return -4;
    {
        size_t position = 0;
        stream->GetPos(stream, &position);
        *finalPosition = position;
    }
    CloseWS_Memory(&stream);
    return 0;
}

int JxrReferenceWrite_nid_no_patch(const uint32_t* script, size_t steps, uint32_t* results, uint8_t* output, size_t capacity, size_t prefix, uint64_t* written) {
    struct WMPStream* stream = NULL;
    U8* writeRing = AlignedRing(writeStorage);
    BitIOInfo* io = RingInfo(writeRing);
    size_t index;
    if (CreateWS_Memory(&stream, output, capacity) != WMP_errSuccess) return -1;
    stream->SetPos(stream, prefix);
    memset(writeRing, 0, 16384 * 2);
    if (attachISWrite(io, stream) != WMP_errSuccess) return -2;
    for (index = 0; index < steps; ++index) {
        const uint32_t operation = script[index * 3];
        const uint32_t first = script[index * 3 + 1];
        const uint32_t second = script[index * 3 + 2];
        uint32_t value = 0;
        switch (operation) {
        case 0: putBit16(io, first, second); break;
        case 1: putBit16z(io, first, second); break;
        case 2: putBit32(io, first, second); break;
        case 3: fillToByte(io); break;
        case 4: value = (uint32_t)writeIS(NULL, io); break;
        case 5: value = getSizeWrite(io); break;
        default: return -3;
        }
        results[index] = value;
    }
    fillToByte(io);
    if (detachISWrite(NULL, io) != WMP_errSuccess) return -4;
    {
        size_t position = 0;
        stream->GetPos(stream, &position);
        *written = position;
    }
    CloseWS_Memory(&stream);
    return 0;
}

int JxrReferenceSimple_nid_no_patch(const uint8_t* data, size_t size, const uint32_t* widths, size_t steps, uint32_t* results, uint32_t* bytesRead) {
    struct WMPStream* stream = NULL;
    SimpleBitIO io;
    size_t index;
    if (CreateWS_Memory(&stream, (void*)data, size) != WMP_errSuccess) return -1;
    attach_SB(&io, stream);
    for (index = 0; index < steps; ++index) {
        if (widths[index] == 99) {
            flushToByte_SB(&io);
            results[index] = 0;
        } else {
            results[index] = getBit32_SB(&io, widths[index]);
        }
    }
    *bytesRead = getByteRead_SB(&io);
    flushToByte_SB(&io);
    detach_SB(&io);
    CloseWS_Memory(&stream);
    return 0;
}

int JxrReferenceList_nid_no_patch(const uint8_t* data, const uint64_t* writes, size_t writeCount, const uint64_t* seeks, const uint64_t* reads, size_t readCount, uint8_t* readBack, uint64_t* positions, int64_t* errors) {
    struct WMPStream* stream = NULL;
    size_t index;
    size_t offset = 0;
    size_t cursor = 0;
    if (CreateWS_List(&stream) != WMP_errSuccess) return -1;
    for (index = 0; index < writeCount; ++index) {
        errors[index] = stream->Write(stream, data + offset, writes[index]);
        offset += writes[index];
    }
    for (index = 0; index < readCount; ++index) {
        size_t position = 0;
        stream->SetPos(stream, seeks[index]);
        errors[writeCount + index] = stream->Read(stream, readBack + cursor, reads[index]);
        cursor += reads[index];
        stream->GetPos(stream, &position);
        positions[index] = position;
    }
    CloseWS_List(&stream);
    return 0;
}

uint8_t JxrReferenceDquant_nid_no_patch(uint8_t quantizers) {
    return dquantBits(quantizers);
}
