#ifndef CORE_LIBS_PRX_LIBSCENKWEBKITREQUIREMENTS_INCLUDE_GUESTJPEG_HPP
#define CORE_LIBS_PRX_LIBSCENKWEBKITREQUIREMENTS_INCLUDE_GUESTJPEG_HPP

#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
#include <cstdint>

namespace NKWebKitRequirements {

struct GuestJpegDecompress;

union GuestJpegMessageParameter {
    std::int32_t i[8];
    char s[80];
};

struct GuestJpegErrorMgr {
    void (APS5_VABI *error_exit)(void*);
    void (APS5_VABI *emit_message)(void*, std::int32_t);
    void (APS5_VABI *output_message)(void*);
    void (APS5_VABI *format_message)(void*, char*);
    void (APS5_VABI *reset_error_mgr)(void*);
    std::int32_t msg_code;
    GuestJpegMessageParameter msg_parm;
    std::int32_t trace_level;
    std::int64_t num_warnings;
    const char* const* jpeg_message_table;
    std::int32_t last_jpeg_message;
    const char* const* addon_message_table;
    std::int32_t first_addon_message;
    std::int32_t last_addon_message;
};

struct GuestJpegSourceMgr {
    const std::uint8_t* next_input_byte;
    std::uint64_t bytes_in_buffer;
    void (APS5_VABI *init_source)(void*);
    std::int32_t (APS5_VABI *fill_input_buffer)(void*);
    void (APS5_VABI *skip_input_data)(void*, std::int64_t);
    std::int32_t (APS5_VABI *resync_to_restart)(void*, std::int32_t);
    void (APS5_VABI *term_source)(void*);
};

struct GuestJpegProgressMgr {
    void (APS5_VABI *progress_monitor)(void*);
    std::int64_t pass_counter;
    std::int64_t pass_limit;
    std::int32_t completed_passes;
    std::int32_t total_passes;
};

struct GuestJpegMemoryMgr {
    void* (APS5_VABI *alloc_small)(void*, std::int32_t, std::uint64_t);
    void* (APS5_VABI *alloc_large)(void*, std::int32_t, std::uint64_t);
    std::uint8_t** (APS5_VABI *alloc_sarray)(void*, std::int32_t, std::uint32_t, std::uint32_t);
    void* (APS5_VABI *alloc_barray)(void*, std::int32_t, std::uint32_t, std::uint32_t);
    void* (APS5_VABI *request_virt_sarray)(void*, std::int32_t, std::int32_t, std::uint32_t, std::uint32_t, std::uint32_t);
    void* (APS5_VABI *request_virt_barray)(void*, std::int32_t, std::int32_t, std::uint32_t, std::uint32_t, std::uint32_t);
    void (APS5_VABI *realize_virt_arrays)(void*);
    std::uint8_t** (APS5_VABI *access_virt_sarray)(void*, void*, std::uint32_t, std::uint32_t, std::int32_t);
    void* (APS5_VABI *access_virt_barray)(void*, void*, std::uint32_t, std::uint32_t, std::int32_t);
    void (APS5_VABI *free_pool)(void*, std::int32_t);
    void (APS5_VABI *self_destruct)(void*);
    std::int64_t max_memory_to_use;
    std::int64_t max_alloc_chunk;
};

static_assert(offsetof(GuestJpegErrorMgr, msg_code) == 40 && offsetof(GuestJpegErrorMgr, msg_parm) == 44);
static_assert(offsetof(GuestJpegErrorMgr, trace_level) == 124 && offsetof(GuestJpegErrorMgr, num_warnings) == 128);
static_assert(offsetof(GuestJpegErrorMgr, jpeg_message_table) == 136 && offsetof(GuestJpegErrorMgr, last_jpeg_message) == 144);
static_assert(offsetof(GuestJpegErrorMgr, addon_message_table) == 152 && offsetof(GuestJpegErrorMgr, first_addon_message) == 160);
static_assert(offsetof(GuestJpegErrorMgr, last_addon_message) == 164 && sizeof(GuestJpegErrorMgr) == 168);
static_assert(offsetof(GuestJpegSourceMgr, init_source) == 16 && offsetof(GuestJpegSourceMgr, term_source) == 48);
static_assert(sizeof(GuestJpegSourceMgr) == 56);
static_assert(offsetof(GuestJpegProgressMgr, pass_counter) == 8 && offsetof(GuestJpegProgressMgr, completed_passes) == 24);
static_assert(sizeof(GuestJpegProgressMgr) == 32);
static_assert(offsetof(GuestJpegMemoryMgr, self_destruct) == 80 && offsetof(GuestJpegMemoryMgr, max_memory_to_use) == 88);
static_assert(offsetof(GuestJpegMemoryMgr, max_alloc_chunk) == 96 && sizeof(GuestJpegMemoryMgr) == 104);

}

#endif
