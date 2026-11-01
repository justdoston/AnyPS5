#include "prx/libSceNKWebKitRequirements/libSceNKWebKitRequirements.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#if defined(_WIN32) && !defined(__RPCNDR_H__)
#define __RPCNDR_H__
#endif
#ifdef _WIN32
#define boolean int
#endif
#include "jpeglib.h"

namespace {

using NKWebKitRequirements::GuestJpegDecompress;
using NKWebKitRequirements::GuestJpegErrorMgr;
using NKWebKitRequirements::GuestJpegMemoryMgr;
using NKWebKitRequirements::GuestJpegProgressMgr;
using NKWebKitRequirements::GuestJpegSourceMgr;

static_assert(JPEG_LIB_VERSION == 62 && sizeof(jpeg_decompress_struct) == 632);
static_assert(offsetof(jpeg_decompress_struct, err) == 0 && offsetof(jpeg_decompress_struct, mem) == 8);
static_assert(offsetof(jpeg_decompress_struct, progress) == 16 && offsetof(jpeg_decompress_struct, src) == 40);
static_assert(sizeof(boolean) == 4 && sizeof(JDIMENSION) == 4 && sizeof(std::size_t) == 8);
static_assert(sizeof(jpeg_marker_struct) == 32 && sizeof(jpeg_component_info) == 96);

struct Decompressor {
    jpeg_decompress_struct* cinfo;
    bool host;
    GuestJpegErrorMgr* guestErr;
    GuestJpegSourceMgr* guestSrc;
    GuestJpegProgressMgr* guestProgress;
    jpeg_memory_mgr* hostMem;
    jpeg_error_mgr hostErr;
    jpeg_source_mgr hostSrc;
    jpeg_progress_mgr hostProgress;
    GuestJpegMemoryMgr guestMem;
};

std::mutex registryMutex;
std::unordered_map<const void*, Decompressor*> registry;

const jpeg_error_mgr& HostDefaults() {
    static const jpeg_error_mgr defaults = [] {
        jpeg_error_mgr err{};
        jpeg_std_error(&err);
        return err;
    }();
    return defaults;
}

Decompressor* Lookup(const void* cinfo) {
    std::lock_guard lock(registryMutex);
    const auto found = registry.find(cinfo);
    return found == registry.end() ? nullptr : found->second;
}

Decompressor* Find(const void* cinfo) {
    Decompressor* decompressor = Lookup(cinfo);
    if (!decompressor) throw std::runtime_error("libjpeg: object was not created by jpeg_CreateDecompress");
    return decompressor;
}

Decompressor* FromHost(j_common_ptr cinfo) {
    return reinterpret_cast<Decompressor*>(reinterpret_cast<char*>(cinfo->err) - offsetof(Decompressor, hostErr));
}

Decompressor* FromGuestMemory(void* cinfo) {
    auto* memory = reinterpret_cast<char*>(static_cast<jpeg_common_struct*>(cinfo)->mem);
    return reinterpret_cast<Decompressor*>(memory - offsetof(Decompressor, guestMem));
}

void LoadError(jpeg_error_mgr& host, const GuestJpegErrorMgr& guest) {
    host.msg_code = guest.msg_code;
    std::memcpy(&host.msg_parm, &guest.msg_parm, sizeof(guest.msg_parm));
    host.trace_level = guest.trace_level;
    host.num_warnings = static_cast<long>(guest.num_warnings);
    host.jpeg_message_table = guest.jpeg_message_table;
    host.last_jpeg_message = guest.last_jpeg_message;
    host.addon_message_table = guest.addon_message_table;
    host.first_addon_message = guest.first_addon_message;
    host.last_addon_message = guest.last_addon_message;
}

void StoreError(GuestJpegErrorMgr& guest, const jpeg_error_mgr& host) {
    guest.msg_code = host.msg_code;
    std::memcpy(&guest.msg_parm, &host.msg_parm, sizeof(guest.msg_parm));
    guest.trace_level = host.trace_level;
    if (host.num_warnings != static_cast<long>(guest.num_warnings)) guest.num_warnings = host.num_warnings;
    guest.jpeg_message_table = host.jpeg_message_table;
    guest.last_jpeg_message = host.last_jpeg_message;
    guest.addon_message_table = host.addon_message_table;
    guest.first_addon_message = host.first_addon_message;
    guest.last_addon_message = host.last_addon_message;
}

void Enter(Decompressor* decompressor) {
    if (decompressor->host) throw std::runtime_error("libjpeg: object reentered while the library runs");
    jpeg_decompress_struct* cinfo = decompressor->cinfo;
    decompressor->guestErr = reinterpret_cast<GuestJpegErrorMgr*>(cinfo->err);
    if (!decompressor->guestErr) throw std::runtime_error("libjpeg: null error manager");
    LoadError(decompressor->hostErr, *decompressor->guestErr);
    cinfo->err = &decompressor->hostErr;
    decompressor->guestSrc = reinterpret_cast<GuestJpegSourceMgr*>(cinfo->src);
    if (decompressor->guestSrc) {
        decompressor->hostSrc.next_input_byte = decompressor->guestSrc->next_input_byte;
        decompressor->hostSrc.bytes_in_buffer = decompressor->guestSrc->bytes_in_buffer;
        cinfo->src = &decompressor->hostSrc;
    }
    decompressor->guestProgress = reinterpret_cast<GuestJpegProgressMgr*>(cinfo->progress);
    if (decompressor->guestProgress) {
        decompressor->hostProgress.pass_counter = static_cast<long>(decompressor->guestProgress->pass_counter);
        decompressor->hostProgress.pass_limit = static_cast<long>(decompressor->guestProgress->pass_limit);
        decompressor->hostProgress.completed_passes = decompressor->guestProgress->completed_passes;
        decompressor->hostProgress.total_passes = decompressor->guestProgress->total_passes;
        cinfo->progress = &decompressor->hostProgress;
    }
    if (cinfo->mem) {
        if (cinfo->mem != reinterpret_cast<jpeg_memory_mgr*>(&decompressor->guestMem))
            throw std::runtime_error("libjpeg: replaced memory manager is not implemented");
        decompressor->hostMem->max_memory_to_use = static_cast<long>(decompressor->guestMem.max_memory_to_use);
        decompressor->hostMem->max_alloc_chunk = static_cast<long>(decompressor->guestMem.max_alloc_chunk);
        cinfo->mem = decompressor->hostMem;
    }
    decompressor->host = true;
}

void Leave(Decompressor* decompressor) {
    jpeg_decompress_struct* cinfo = decompressor->cinfo;
    StoreError(*decompressor->guestErr, decompressor->hostErr);
    cinfo->err = reinterpret_cast<jpeg_error_mgr*>(decompressor->guestErr);
    if (decompressor->guestSrc) {
        decompressor->guestSrc->next_input_byte = decompressor->hostSrc.next_input_byte;
        decompressor->guestSrc->bytes_in_buffer = decompressor->hostSrc.bytes_in_buffer;
        cinfo->src = reinterpret_cast<jpeg_source_mgr*>(decompressor->guestSrc);
    }
    if (decompressor->guestProgress) {
        decompressor->guestProgress->pass_counter = decompressor->hostProgress.pass_counter;
        decompressor->guestProgress->pass_limit = decompressor->hostProgress.pass_limit;
        decompressor->guestProgress->completed_passes = decompressor->hostProgress.completed_passes;
        decompressor->guestProgress->total_passes = decompressor->hostProgress.total_passes;
        cinfo->progress = reinterpret_cast<jpeg_progress_mgr*>(decompressor->guestProgress);
    }
    if (cinfo->mem) {
        decompressor->hostMem = cinfo->mem;
        decompressor->guestMem.max_memory_to_use = decompressor->hostMem->max_memory_to_use;
        decompressor->guestMem.max_alloc_chunk = decompressor->hostMem->max_alloc_chunk;
        cinfo->mem = reinterpret_cast<jpeg_memory_mgr*>(&decompressor->guestMem);
    }
    decompressor->host = false;
}

void HostErrorExit(j_common_ptr cinfo) {
    Decompressor* decompressor = FromHost(cinfo);
    Leave(decompressor);
    decompressor->guestErr->error_exit(decompressor->cinfo);
    throw std::runtime_error("libjpeg: error_exit returned");
}

void HostEmitMessage(j_common_ptr cinfo, int level) {
    Decompressor* decompressor = FromHost(cinfo);
    Leave(decompressor);
    decompressor->guestErr->emit_message(decompressor->cinfo, level);
    Enter(decompressor);
}

void HostOutputMessage(j_common_ptr cinfo) {
    Decompressor* decompressor = FromHost(cinfo);
    Leave(decompressor);
    decompressor->guestErr->output_message(decompressor->cinfo);
    Enter(decompressor);
}

void HostFormatMessage(j_common_ptr cinfo, char* buffer) {
    Decompressor* decompressor = FromHost(cinfo);
    Leave(decompressor);
    decompressor->guestErr->format_message(decompressor->cinfo, buffer);
    Enter(decompressor);
}

void HostResetErrorMgr(j_common_ptr cinfo) {
    Decompressor* decompressor = FromHost(cinfo);
    Leave(decompressor);
    decompressor->guestErr->reset_error_mgr(decompressor->cinfo);
    Enter(decompressor);
}

void HostInitSource(j_decompress_ptr cinfo) {
    Decompressor* decompressor = FromHost(reinterpret_cast<j_common_ptr>(cinfo));
    Leave(decompressor);
    decompressor->guestSrc->init_source(decompressor->cinfo);
    Enter(decompressor);
}

boolean HostFillInputBuffer(j_decompress_ptr cinfo) {
    Decompressor* decompressor = FromHost(reinterpret_cast<j_common_ptr>(cinfo));
    Leave(decompressor);
    const std::int32_t result = decompressor->guestSrc->fill_input_buffer(decompressor->cinfo);
    Enter(decompressor);
    return result;
}

void HostSkipInputData(j_decompress_ptr cinfo, long count) {
    Decompressor* decompressor = FromHost(reinterpret_cast<j_common_ptr>(cinfo));
    Leave(decompressor);
    decompressor->guestSrc->skip_input_data(decompressor->cinfo, count);
    Enter(decompressor);
}

boolean HostResyncToRestart(j_decompress_ptr cinfo, int desired) {
    Decompressor* decompressor = FromHost(reinterpret_cast<j_common_ptr>(cinfo));
    Leave(decompressor);
    const std::int32_t result = decompressor->guestSrc->resync_to_restart(decompressor->cinfo, desired);
    Enter(decompressor);
    return result;
}

void HostTermSource(j_decompress_ptr cinfo) {
    Decompressor* decompressor = FromHost(reinterpret_cast<j_common_ptr>(cinfo));
    Leave(decompressor);
    decompressor->guestSrc->term_source(decompressor->cinfo);
    Enter(decompressor);
}

void HostProgressMonitor(j_common_ptr cinfo) {
    Decompressor* decompressor = FromHost(cinfo);
    Leave(decompressor);
    decompressor->guestProgress->progress_monitor(decompressor->cinfo);
    Enter(decompressor);
}

j_common_ptr Common(Decompressor* decompressor) {
    return reinterpret_cast<j_common_ptr>(decompressor->cinfo);
}

void* APS5_VABI AllocSmall_nid_no_patch(void* cinfo, std::int32_t pool, std::uint64_t size) {
    Decompressor* decompressor = FromGuestMemory(cinfo);
    Enter(decompressor);
    void* result = decompressor->hostMem->alloc_small(Common(decompressor), pool, size);
    Leave(decompressor);
    return result;
}

void* APS5_VABI AllocLarge_nid_no_patch(void* cinfo, std::int32_t pool, std::uint64_t size) {
    Decompressor* decompressor = FromGuestMemory(cinfo);
    Enter(decompressor);
    void* result = decompressor->hostMem->alloc_large(Common(decompressor), pool, size);
    Leave(decompressor);
    return result;
}

std::uint8_t** APS5_VABI AllocSarray_nid_no_patch(void* cinfo, std::int32_t pool, std::uint32_t width, std::uint32_t rows) {
    Decompressor* decompressor = FromGuestMemory(cinfo);
    Enter(decompressor);
    JSAMPARRAY result = decompressor->hostMem->alloc_sarray(Common(decompressor), pool, width, rows);
    Leave(decompressor);
    return result;
}

void* APS5_VABI AllocBarray_nid_no_patch(void* cinfo, std::int32_t pool, std::uint32_t width, std::uint32_t rows) {
    Decompressor* decompressor = FromGuestMemory(cinfo);
    Enter(decompressor);
    JBLOCKARRAY result = decompressor->hostMem->alloc_barray(Common(decompressor), pool, width, rows);
    Leave(decompressor);
    return result;
}

void* APS5_VABI RequestVirtSarray_nid_no_patch(void* cinfo, std::int32_t pool, std::int32_t preZero, std::uint32_t width, std::uint32_t rows, std::uint32_t access) {
    Decompressor* decompressor = FromGuestMemory(cinfo);
    Enter(decompressor);
    jvirt_sarray_ptr result = decompressor->hostMem->request_virt_sarray(Common(decompressor), pool, preZero, width, rows, access);
    Leave(decompressor);
    return result;
}

void* APS5_VABI RequestVirtBarray_nid_no_patch(void* cinfo, std::int32_t pool, std::int32_t preZero, std::uint32_t width, std::uint32_t rows, std::uint32_t access) {
    Decompressor* decompressor = FromGuestMemory(cinfo);
    Enter(decompressor);
    jvirt_barray_ptr result = decompressor->hostMem->request_virt_barray(Common(decompressor), pool, preZero, width, rows, access);
    Leave(decompressor);
    return result;
}

void APS5_VABI RealizeVirtArrays_nid_no_patch(void* cinfo) {
    Decompressor* decompressor = FromGuestMemory(cinfo);
    Enter(decompressor);
    decompressor->hostMem->realize_virt_arrays(Common(decompressor));
    Leave(decompressor);
}

std::uint8_t** APS5_VABI AccessVirtSarray_nid_no_patch(void* cinfo, void* array, std::uint32_t start, std::uint32_t rows, std::int32_t writable) {
    Decompressor* decompressor = FromGuestMemory(cinfo);
    Enter(decompressor);
    JSAMPARRAY result = decompressor->hostMem->access_virt_sarray(Common(decompressor), static_cast<jvirt_sarray_ptr>(array), start, rows, writable);
    Leave(decompressor);
    return result;
}

void* APS5_VABI AccessVirtBarray_nid_no_patch(void* cinfo, void* array, std::uint32_t start, std::uint32_t rows, std::int32_t writable) {
    Decompressor* decompressor = FromGuestMemory(cinfo);
    Enter(decompressor);
    JBLOCKARRAY result = decompressor->hostMem->access_virt_barray(Common(decompressor), static_cast<jvirt_barray_ptr>(array), start, rows, writable);
    Leave(decompressor);
    return result;
}

void APS5_VABI FreePool_nid_no_patch(void* cinfo, std::int32_t pool) {
    Decompressor* decompressor = FromGuestMemory(cinfo);
    Enter(decompressor);
    decompressor->hostMem->free_pool(Common(decompressor), pool);
    Leave(decompressor);
}

void APS5_VABI SelfDestruct_nid_no_patch(void* cinfo) {
    Decompressor* decompressor = FromGuestMemory(cinfo);
    Enter(decompressor);
    decompressor->hostMem->self_destruct(Common(decompressor));
    Leave(decompressor);
}

void APS5_VABI DefaultErrorExit_nid_no_patch(void* cinfo) {
    Decompressor* decompressor = Lookup(cinfo);
    if (!decompressor) {
        reinterpret_cast<GuestJpegErrorMgr*>(static_cast<jpeg_common_struct*>(cinfo)->err)->output_message(cinfo);
        std::exit(EXIT_FAILURE);
    }
    Enter(decompressor);
    HostDefaults().error_exit(Common(decompressor));
}

void APS5_VABI DefaultEmitMessage_nid_no_patch(void* cinfo, std::int32_t level) {
    Decompressor* decompressor = Lookup(cinfo);
    if (!decompressor) throw std::runtime_error("libjpeg: emit_message on an object without a decompressor");
    Enter(decompressor);
    HostDefaults().emit_message(Common(decompressor), level);
    Leave(decompressor);
}

void APS5_VABI DefaultFormatMessage_nid_no_patch(void* cinfo, char* buffer) {
    auto* guest = reinterpret_cast<GuestJpegErrorMgr*>(static_cast<jpeg_common_struct*>(cinfo)->err);
    jpeg_error_mgr host{};
    LoadError(host, *guest);
    jpeg_common_struct common{};
    common.err = &host;
    HostDefaults().format_message(&common, buffer);
}

void APS5_VABI DefaultOutputMessage_nid_no_patch(void* cinfo) {
    char buffer[JMSG_LENGTH_MAX];
    reinterpret_cast<GuestJpegErrorMgr*>(static_cast<jpeg_common_struct*>(cinfo)->err)->format_message(cinfo, buffer);
    std::fprintf(stderr, "%s\n", buffer);
}

void APS5_VABI DefaultResetErrorMgr_nid_no_patch(void* cinfo) {
    auto* guest = reinterpret_cast<GuestJpegErrorMgr*>(static_cast<jpeg_common_struct*>(cinfo)->err);
    jpeg_error_mgr host{};
    LoadError(host, *guest);
    jpeg_common_struct common{};
    common.err = &host;
    HostDefaults().reset_error_mgr(&common);
    StoreError(*guest, host);
}

Decompressor* Create(jpeg_decompress_struct* cinfo) {
    auto* decompressor = new Decompressor{};
    decompressor->cinfo = cinfo;
    decompressor->hostErr.error_exit = HostErrorExit;
    decompressor->hostErr.emit_message = HostEmitMessage;
    decompressor->hostErr.output_message = HostOutputMessage;
    decompressor->hostErr.format_message = HostFormatMessage;
    decompressor->hostErr.reset_error_mgr = HostResetErrorMgr;
    decompressor->hostSrc.init_source = HostInitSource;
    decompressor->hostSrc.fill_input_buffer = HostFillInputBuffer;
    decompressor->hostSrc.skip_input_data = HostSkipInputData;
    decompressor->hostSrc.resync_to_restart = HostResyncToRestart;
    decompressor->hostSrc.term_source = HostTermSource;
    decompressor->hostProgress.progress_monitor = HostProgressMonitor;
    GuestJpegMemoryMgr& memory = decompressor->guestMem;
    memory.alloc_small = AllocSmall_nid_no_patch;
    memory.alloc_large = AllocLarge_nid_no_patch;
    memory.alloc_sarray = AllocSarray_nid_no_patch;
    memory.alloc_barray = AllocBarray_nid_no_patch;
    memory.request_virt_sarray = RequestVirtSarray_nid_no_patch;
    memory.request_virt_barray = RequestVirtBarray_nid_no_patch;
    memory.realize_virt_arrays = RealizeVirtArrays_nid_no_patch;
    memory.access_virt_sarray = AccessVirtSarray_nid_no_patch;
    memory.access_virt_barray = AccessVirtBarray_nid_no_patch;
    memory.free_pool = FreePool_nid_no_patch;
    memory.self_destruct = SelfDestruct_nid_no_patch;
    Decompressor* previous = nullptr;
    {
        std::lock_guard lock(registryMutex);
        auto& slot = registry[cinfo];
        previous = slot;
        slot = decompressor;
    }
    delete previous;
    return decompressor;
}

void Remove(Decompressor* decompressor) {
    {
        std::lock_guard lock(registryMutex);
        const auto found = registry.find(decompressor->cinfo);
        if (found != registry.end() && found->second == decompressor) registry.erase(found);
    }
    delete decompressor;
}

jpeg_decompress_struct* Host(GuestJpegDecompress* cinfo) {
    return reinterpret_cast<jpeg_decompress_struct*>(cinfo);
}

} // namespace

extern "C" {

GuestJpegErrorMgr* APS5_VABI jpeg_std_error_nid_postfix(GuestJpegErrorMgr* err) {
    const jpeg_error_mgr& defaults = HostDefaults();
    err->error_exit = DefaultErrorExit_nid_no_patch;
    err->emit_message = DefaultEmitMessage_nid_no_patch;
    err->output_message = DefaultOutputMessage_nid_no_patch;
    err->format_message = DefaultFormatMessage_nid_no_patch;
    err->reset_error_mgr = DefaultResetErrorMgr_nid_no_patch;
    err->trace_level = 0;
    err->num_warnings = 0;
    err->msg_code = 0;
    err->jpeg_message_table = defaults.jpeg_message_table;
    err->last_jpeg_message = defaults.last_jpeg_message;
    err->addon_message_table = nullptr;
    err->first_addon_message = 0;
    err->last_addon_message = 0;
    return err;
}

void APS5_VABI jpeg_CreateDecompress_nid_postfix(GuestJpegDecompress* cinfo, std::int32_t version, std::uint64_t structSize) {
    if (version != JPEG_LIB_VERSION || structSize != sizeof(jpeg_decompress_struct))
        throw std::runtime_error("jpeg_CreateDecompress: only libjpeg API version 62 with a 632-byte object is implemented");
    jpeg_decompress_struct* host = Host(cinfo);
    if (!host->err) throw std::runtime_error("jpeg_CreateDecompress: null error manager");
    Decompressor* decompressor = Create(host);
    decompressor->guestErr = reinterpret_cast<GuestJpegErrorMgr*>(host->err);
    LoadError(decompressor->hostErr, *decompressor->guestErr);
    host->err = &decompressor->hostErr;
    decompressor->host = true;
    jpeg_CreateDecompress(host, JPEG_LIB_VERSION, sizeof(jpeg_decompress_struct));
    Leave(decompressor);
}

void APS5_VABI jpeg_destroy_decompress_nid_postfix(GuestJpegDecompress* cinfo) {
    jpeg_decompress_struct* host = Host(cinfo);
    Decompressor* decompressor = Lookup(host);
    if (!decompressor) {
        if (host->mem) throw std::runtime_error("jpeg_destroy_decompress: object was not created by jpeg_CreateDecompress");
        host->global_state = 0;
        return;
    }
    Enter(decompressor);
    jpeg_destroy_decompress(host);
    Leave(decompressor);
    Remove(decompressor);
}

std::int32_t APS5_VABI jpeg_read_header_nid_postfix(GuestJpegDecompress* cinfo, std::int32_t requireImage) {
    Decompressor* decompressor = Find(cinfo);
    Enter(decompressor);
    const int result = jpeg_read_header(decompressor->cinfo, requireImage);
    Leave(decompressor);
    return result;
}

void APS5_VABI jpeg_calc_output_dimensions_nid_postfix(GuestJpegDecompress* cinfo) {
    Decompressor* decompressor = Find(cinfo);
    Enter(decompressor);
    jpeg_calc_output_dimensions(decompressor->cinfo);
    Leave(decompressor);
}

std::int32_t APS5_VABI jpeg_start_decompress_nid_postfix(GuestJpegDecompress* cinfo) {
    Decompressor* decompressor = Find(cinfo);
    Enter(decompressor);
    const boolean result = jpeg_start_decompress(decompressor->cinfo);
    Leave(decompressor);
    return result;
}

std::uint32_t APS5_VABI jpeg_read_scanlines_nid_postfix(GuestJpegDecompress* cinfo, std::uint8_t** scanlines, std::uint32_t maxLines) {
    Decompressor* decompressor = Find(cinfo);
    Enter(decompressor);
    const JDIMENSION result = jpeg_read_scanlines(decompressor->cinfo, scanlines, maxLines);
    Leave(decompressor);
    return result;
}

std::int32_t APS5_VABI jpeg_finish_decompress_nid_postfix(GuestJpegDecompress* cinfo) {
    Decompressor* decompressor = Find(cinfo);
    Enter(decompressor);
    const boolean result = jpeg_finish_decompress(decompressor->cinfo);
    Leave(decompressor);
    return result;
}

std::int32_t APS5_VABI jpeg_consume_input_nid_postfix(GuestJpegDecompress* cinfo) {
    Decompressor* decompressor = Find(cinfo);
    Enter(decompressor);
    const int result = jpeg_consume_input(decompressor->cinfo);
    Leave(decompressor);
    return result;
}

std::int32_t APS5_VABI jpeg_has_multiple_scans_nid_postfix(GuestJpegDecompress* cinfo) {
    Decompressor* decompressor = Find(cinfo);
    Enter(decompressor);
    const boolean result = jpeg_has_multiple_scans(decompressor->cinfo);
    Leave(decompressor);
    return result;
}

std::int32_t APS5_VABI jpeg_input_complete_nid_postfix(GuestJpegDecompress* cinfo) {
    Decompressor* decompressor = Find(cinfo);
    Enter(decompressor);
    const boolean result = jpeg_input_complete(decompressor->cinfo);
    Leave(decompressor);
    return result;
}

std::int32_t APS5_VABI jpeg_start_output_nid_postfix(GuestJpegDecompress* cinfo, std::int32_t scanNumber) {
    Decompressor* decompressor = Find(cinfo);
    Enter(decompressor);
    const boolean result = jpeg_start_output(decompressor->cinfo, scanNumber);
    Leave(decompressor);
    return result;
}

std::int32_t APS5_VABI jpeg_finish_output_nid_postfix(GuestJpegDecompress* cinfo) {
    Decompressor* decompressor = Find(cinfo);
    Enter(decompressor);
    const boolean result = jpeg_finish_output(decompressor->cinfo);
    Leave(decompressor);
    return result;
}

void APS5_VABI jpeg_save_markers_nid_postfix(GuestJpegDecompress* cinfo, std::int32_t markerCode, std::uint32_t lengthLimit) {
    Decompressor* decompressor = Find(cinfo);
    Enter(decompressor);
    jpeg_save_markers(decompressor->cinfo, markerCode, lengthLimit);
    Leave(decompressor);
}

std::int32_t APS5_VABI jpeg_resync_to_restart_nid_postfix(GuestJpegDecompress* cinfo, std::int32_t desired) {
    Decompressor* decompressor = Find(cinfo);
    Enter(decompressor);
    const boolean result = jpeg_resync_to_restart(decompressor->cinfo, desired);
    Leave(decompressor);
    return result;
}

}
