#include "prx/libSceNKWebKitRequirements/libSceNKWebKitRequirements.hpp"
#include <algorithm>
#include <csetjmp>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#if defined(_WIN32) && !defined(__RPCNDR_H__)
#define __RPCNDR_H__
#endif
#ifdef _WIN32
#define boolean int
#endif
#include "jpeglib.h"
#include "jerror.h"

using NKWebKitRequirements::GuestJpegDecompress;
using NKWebKitRequirements::GuestJpegErrorMgr;
using NKWebKitRequirements::GuestJpegMemoryMgr;
using NKWebKitRequirements::GuestJpegProgressMgr;
using NKWebKitRequirements::GuestJpegSourceMgr;

namespace {

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "WebKit JPEG assertion failed: %s\n", message);
    std::abort();
}

template<typename TAction>
bool Throws(TAction action) {
    try { action(); } catch (const std::runtime_error&) { return true; }
    return false;
}

struct Options {
    int width = 37;
    int height = 23;
    bool progressive = false;
    int restartRows = 0;
    J_COLOR_SPACE space = JCS_RGB;
};

std::vector<std::uint8_t> Encode(const Options& options, const std::string& comment, const std::vector<std::uint8_t>& profile) {
    jpeg_compress_struct compress{};
    jpeg_error_mgr err{};
    compress.err = jpeg_std_error(&err);
    jpeg_create_compress(&compress);
    unsigned char* buffer = nullptr;
    unsigned long size = 0;
    jpeg_mem_dest(&compress, &buffer, &size);
    const int components = options.space == JCS_GRAYSCALE ? 1 : 3;
    compress.image_width = options.width;
    compress.image_height = options.height;
    compress.input_components = components;
    compress.in_color_space = options.space;
    jpeg_set_defaults(&compress);
    jpeg_set_quality(&compress, 85, TRUE);
    if (options.progressive) jpeg_simple_progression(&compress);
    compress.restart_in_rows = options.restartRows;
    jpeg_start_compress(&compress, TRUE);
    if (!comment.empty()) jpeg_write_marker(&compress, JPEG_COM, reinterpret_cast<const JOCTET*>(comment.data()), static_cast<unsigned>(comment.size()));
    if (!profile.empty()) jpeg_write_marker(&compress, JPEG_APP0 + 2, profile.data(), static_cast<unsigned>(profile.size()));
    std::vector<JSAMPLE> row(static_cast<std::size_t>(options.width) * components);
    while (compress.next_scanline < compress.image_height) {
        for (int x = 0; x < options.width; ++x)
            for (int c = 0; c < components; ++c)
                row[static_cast<std::size_t>(x) * components + c] = static_cast<JSAMPLE>((x * 7 + compress.next_scanline * 11 + c * 50) & 0xFF);
        JSAMPROW pointer = row.data();
        jpeg_write_scanlines(&compress, &pointer, 1);
    }
    jpeg_finish_compress(&compress);
    std::vector<std::uint8_t> result(buffer, buffer + size);
    jpeg_destroy_compress(&compress);
    std::free(buffer);
    return result;
}

struct Output {
    unsigned width;
    unsigned height;
    int components;
    std::vector<std::uint8_t> pixels;
    std::vector<std::pair<int, std::vector<std::uint8_t>>> markers;
    bool multipleScans;
};

Output Reference(const std::vector<std::uint8_t>& data, unsigned scaleDenominator, J_COLOR_SPACE space) {
    jpeg_decompress_struct cinfo{};
    jpeg_error_mgr err{};
    cinfo.err = jpeg_std_error(&err);
    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, data.data(), static_cast<unsigned long>(data.size()));
    jpeg_read_header(&cinfo, TRUE);
    cinfo.scale_denom = scaleDenominator;
    cinfo.out_color_space = space;
    jpeg_start_decompress(&cinfo);
    Output output{cinfo.output_width, cinfo.output_height, cinfo.output_components, {}, {}, false};
    output.pixels.resize(static_cast<std::size_t>(output.width) * output.height * output.components);
    while (cinfo.output_scanline < cinfo.output_height) {
        JSAMPROW row = output.pixels.data() + static_cast<std::size_t>(cinfo.output_scanline) * output.width * output.components;
        jpeg_read_scanlines(&cinfo, &row, 1);
    }
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    return output;
}

struct GuestSource {
    GuestJpegSourceMgr pub;
    const std::uint8_t* data;
    std::size_t size;
    std::size_t chunk;
    bool suspending;
    int inits;
    int fills;
    int suspensions;
    int skips;
    int terms;
    void* scratch;
};

struct GuestError {
    GuestJpegErrorMgr pub;
    std::jmp_buf jump;
    int exits;
    char message[JMSG_LENGTH_MAX];
};

constexpr std::uint8_t FAKE_EOI[] = {0xFF, JPEG_EOI};
int progressCalls = 0;

jpeg_decompress_struct* Info(void* cinfo) {
    return static_cast<jpeg_decompress_struct*>(cinfo);
}

GuestSource* Source(void* cinfo) {
    return reinterpret_cast<GuestSource*>(Info(cinfo)->src);
}

GuestJpegMemoryMgr* Memory(void* cinfo) {
    return reinterpret_cast<GuestJpegMemoryMgr*>(Info(cinfo)->mem);
}

void APS5_VABI InitSource(void* cinfo) {
    GuestSource* source = Source(cinfo);
    ++source->inits;
    source->scratch = Memory(cinfo)->alloc_small(cinfo, JPOOL_IMAGE, 64);
    Require(source->scratch != nullptr, "guest alloc_small from init_source");
    std::memset(source->scratch, 0x5A, 64);
}

std::int32_t APS5_VABI FillInputBuffer(void* cinfo) {
    GuestSource* source = Source(cinfo);
    ++source->fills;
    const std::uint8_t* next = source->pub.next_input_byte;
    const bool inData = next >= source->data && next <= source->data + source->size;
    const std::size_t consumed = inData ? static_cast<std::size_t>(next - source->data) + source->pub.bytes_in_buffer : source->size;
    const std::size_t available = next ? std::min(source->size, consumed) : 0;
    if (available >= source->size) {
        source->pub.next_input_byte = FAKE_EOI;
        source->pub.bytes_in_buffer = sizeof(FAKE_EOI);
        return TRUE;
    }
    if (source->suspending) {
        ++source->suspensions;
        return FALSE;
    }
    source->pub.next_input_byte = source->data + available;
    source->pub.bytes_in_buffer = std::min(source->chunk, source->size - available);
    return TRUE;
}

void APS5_VABI SkipInputData(void* cinfo, std::int64_t count) {
    GuestSource* source = Source(cinfo);
    ++source->skips;
    if (count <= 0) return;
    while (count > static_cast<std::int64_t>(source->pub.bytes_in_buffer)) {
        count -= static_cast<std::int64_t>(source->pub.bytes_in_buffer);
        source->pub.next_input_byte += source->pub.bytes_in_buffer;
        source->pub.bytes_in_buffer = 0;
        const bool suspending = source->suspending;
        source->suspending = false;
        FillInputBuffer(cinfo);
        source->suspending = suspending;
    }
    source->pub.next_input_byte += count;
    source->pub.bytes_in_buffer -= static_cast<std::size_t>(count);
}

void APS5_VABI TermSource(void* cinfo) {
    ++Source(cinfo)->terms;
}

void APS5_VABI ProgressMonitor(void*) {
    ++progressCalls;
}

void APS5_VABI LongJumpErrorExit(void* cinfo) {
    auto* error = reinterpret_cast<GuestError*>(Info(cinfo)->err);
    ++error->exits;
    error->pub.format_message(cinfo, error->message);
    std::longjmp(error->jump, 1);
}

void Feed(GuestSource& source) {
    if (!source.suspending) return;
    const std::uint8_t* next = source.pub.next_input_byte;
    if (!next) {
        source.pub.next_input_byte = source.data;
        source.pub.bytes_in_buffer = std::min(source.chunk, source.size);
        return;
    }
    if (next < source.data || next > source.data + source.size) return;
    const std::size_t available = static_cast<std::size_t>(next - source.data) + source.pub.bytes_in_buffer;
    source.pub.bytes_in_buffer += std::min(source.chunk, source.size - std::min(source.size, available));
}

GuestSource MakeSource(const std::vector<std::uint8_t>& data, std::size_t chunk, bool suspending) {
    GuestSource source{};
    source.pub.init_source = InitSource;
    source.pub.fill_input_buffer = FillInputBuffer;
    source.pub.skip_input_data = SkipInputData;
    source.pub.resync_to_restart = reinterpret_cast<std::int32_t (APS5_VABI *)(void*, std::int32_t)>(jpeg_resync_to_restart_nid_postfix);
    source.pub.term_source = TermSource;
    source.data = data.data();
    source.size = data.size();
    source.chunk = chunk;
    source.suspending = suspending;
    return source;
}

GuestJpegDecompress* Guest(jpeg_decompress_struct& cinfo) {
    return reinterpret_cast<GuestJpegDecompress*>(&cinfo);
}

void RequireGuestView(jpeg_decompress_struct& cinfo, GuestError& error, GuestSource* source) {
    Require(cinfo.err == reinterpret_cast<jpeg_error_mgr*>(&error.pub), "guest sees its own error manager");
    if (source) Require(cinfo.src == reinterpret_cast<jpeg_source_mgr*>(&source->pub), "guest sees its own source manager");
    if (cinfo.mem) Require(reinterpret_cast<GuestJpegMemoryMgr*>(cinfo.mem)->alloc_small != nullptr, "guest memory manager");
}

void Create(jpeg_decompress_struct& cinfo, GuestError& error) {
    cinfo = jpeg_decompress_struct{};
    error = GuestError{};
    jpeg_std_error_nid_postfix(&error.pub);
    error.pub.error_exit = LongJumpErrorExit;
    cinfo.err = reinterpret_cast<jpeg_error_mgr*>(&error.pub);
    jpeg_CreateDecompress_nid_postfix(Guest(cinfo), JPEG_LIB_VERSION, sizeof(jpeg_decompress_struct));
    RequireGuestView(cinfo, error, nullptr);
    Require(cinfo.mem != nullptr && cinfo.src == nullptr && cinfo.global_state != 0, "created object");
}

Output Decode(jpeg_decompress_struct& cinfo, GuestError& error, GuestSource& source, unsigned scaleDenominator, J_COLOR_SPACE space, unsigned rowsPerCall) {
    cinfo.src = reinterpret_cast<jpeg_source_mgr*>(&source.pub);
    int header = JPEG_SUSPENDED;
    while ((header = jpeg_read_header_nid_postfix(Guest(cinfo), TRUE)) == JPEG_SUSPENDED) Feed(source);
    Require(header == JPEG_HEADER_OK, "header read");
    RequireGuestView(cinfo, error, &source);
    std::vector<std::pair<int, std::vector<std::uint8_t>>> markers;
    for (jpeg_saved_marker_ptr marker = cinfo.marker_list; marker; marker = marker->next)
        markers.emplace_back(marker->marker, std::vector<std::uint8_t>(marker->data, marker->data + marker->data_length));
    const bool multipleScans = jpeg_has_multiple_scans_nid_postfix(Guest(cinfo)) == TRUE;
    cinfo.scale_denom = scaleDenominator;
    cinfo.out_color_space = space;
    jpeg_calc_output_dimensions_nid_postfix(Guest(cinfo));
    const unsigned expectedWidth = cinfo.output_width;
    while (!jpeg_start_decompress_nid_postfix(Guest(cinfo))) Feed(source);
    Require(cinfo.output_width == expectedWidth, "calculated dimensions match");
    Output output{cinfo.output_width, cinfo.output_height, cinfo.output_components, {}, markers, multipleScans};
    const std::size_t stride = static_cast<std::size_t>(output.width) * output.components;
    output.pixels.resize(stride * output.height);
    while (cinfo.output_scanline < cinfo.output_height) {
        std::vector<JSAMPROW> rows;
        for (unsigned index = 0; index < rowsPerCall; ++index)
            rows.push_back(output.pixels.data() + std::min<std::size_t>(cinfo.output_scanline + index, output.height - 1) * stride);
        if (jpeg_read_scanlines_nid_postfix(Guest(cinfo), rows.data(), rowsPerCall) == 0) Feed(source);
        RequireGuestView(cinfo, error, &source);
    }
    while (!jpeg_finish_decompress_nid_postfix(Guest(cinfo))) Feed(source);
    return output;
}

bool Same(const Output& left, const Output& right) {
    return left.width == right.width && left.height == right.height && left.components == right.components && left.pixels == right.pixels;
}

void Defaults() {
    GuestError error{};
    Require(jpeg_std_error_nid_postfix(&error.pub) == &error.pub, "std_error returns its argument");
    Require(error.pub.error_exit && error.pub.emit_message && error.pub.output_message && error.pub.format_message && error.pub.reset_error_mgr, "default methods");
    Require(error.pub.jpeg_message_table != nullptr && error.pub.last_jpeg_message > 0 && error.pub.trace_level == 0, "default tables");
    jpeg_decompress_struct cinfo{};
    cinfo.err = reinterpret_cast<jpeg_error_mgr*>(&error.pub);
    error.pub.msg_code = JWRN_JPEG_EOF;
    char message[JMSG_LENGTH_MAX];
    error.pub.format_message(&cinfo, message);
    Require(std::string(message) == "Premature end of JPEG file", "default format_message");
    error.pub.msg_code = JERR_BAD_PRECISION;
    error.pub.msg_parm.i[0] = 13;
    error.pub.format_message(&cinfo, message);
    Require(std::string(message) == "Unsupported JPEG data precision 13", "formatted parameter");
    error.pub.num_warnings = 4;
    error.pub.reset_error_mgr(&cinfo);
    Require(error.pub.num_warnings == 0 && error.pub.msg_code == 0, "default reset_error_mgr");

    Require(Throws([&] { jpeg_CreateDecompress_nid_postfix(Guest(cinfo), 80, sizeof(jpeg_decompress_struct)); }), "other API version throws");
    Require(Throws([&] { jpeg_CreateDecompress_nid_postfix(Guest(cinfo), JPEG_LIB_VERSION, 656); }), "other object size throws");
    Require(Throws([&] { jpeg_read_header_nid_postfix(Guest(cinfo), TRUE); }), "uncreated object throws");
    jpeg_destroy_decompress_nid_postfix(Guest(cinfo));
    Require(cinfo.global_state == 0, "destroying an uncreated object is a no-op");
}

void Baseline() {
    const std::string comment = "AnyPS5 guest comment";
    std::vector<std::uint8_t> profile(300);
    for (std::size_t index = 0; index < profile.size(); ++index) profile[index] = static_cast<std::uint8_t>(index * 3);
    const auto data = Encode(Options{37, 23, false, 1, JCS_RGB}, comment, profile);

    for (const unsigned scale : {1u, 2u, 8u}) {
        for (const J_COLOR_SPACE space : {JCS_RGB, JCS_GRAYSCALE}) {
            jpeg_decompress_struct cinfo;
            GuestError error;
            Create(cinfo, error);
            GuestJpegProgressMgr progress{ProgressMonitor, 0, 0, 0, 0};
            cinfo.progress = reinterpret_cast<jpeg_progress_mgr*>(&progress);
            auto* memory = reinterpret_cast<GuestJpegMemoryMgr*>(cinfo.mem);
            memory->max_memory_to_use = 123456789;
            jpeg_save_markers_nid_postfix(Guest(cinfo), JPEG_COM, 0xFFFF);
            jpeg_save_markers_nid_postfix(Guest(cinfo), JPEG_APP0 + 2, 0xFFFF);
            GuestSource source = MakeSource(data, 7, false);
            progressCalls = 0;
            const Output output = Decode(cinfo, error, source, scale, space, 2);
            Require(Same(output, Reference(data, scale, space)), "baseline output matches libjpeg");
            Require(cinfo.image_width == 37 && cinfo.image_height == 23 && cinfo.num_components == 3, "header fields");
            Require(cinfo.progress == reinterpret_cast<jpeg_progress_mgr*>(&progress) && progressCalls > 0 && progress.pass_limit > 0, "progress monitor");
            Require(memory->max_memory_to_use == 123456789, "memory limit round trip");
            Require(source.inits == 1 && source.terms == 1 && source.fills > 1 && source.scratch != nullptr, "source callbacks");
            Require(output.markers.size() == 2, "both markers saved");
            Require(output.markers[0].first == JPEG_COM && output.markers[0].second == std::vector<std::uint8_t>(comment.begin(), comment.end()), "saved comment");
            Require(output.markers[1].first == JPEG_APP0 + 2 && output.markers[1].second == profile, "saved APP2");
            Require(cinfo.marker_list == nullptr, "finish releases saved markers");
            Require(!output.multipleScans, "baseline has one scan");
            jpeg_destroy_decompress_nid_postfix(Guest(cinfo));
            Require(cinfo.mem == nullptr && cinfo.global_state == 0, "destroyed object");
            Require(cinfo.err == reinterpret_cast<jpeg_error_mgr*>(&error.pub) && error.exits == 0, "no errors");
        }
    }
}

void Suspending() {
    const auto data = Encode(Options{64, 48, false, 2, JCS_YCbCr}, "", {});
    jpeg_decompress_struct cinfo;
    GuestError error;
    Create(cinfo, error);
    GuestSource source = MakeSource(data, 5, true);
    const Output output = Decode(cinfo, error, source, 1, JCS_RGB, 3);
    Require(Same(output, Reference(data, 1, JCS_RGB)), "suspending source output matches libjpeg");
    Require(source.suspensions > 0, "source suspended");
    jpeg_destroy_decompress_nid_postfix(Guest(cinfo));
}

void Progressive() {
    const auto data = Encode(Options{45, 31, true, 0, JCS_RGB}, "", {});
    jpeg_decompress_struct cinfo;
    GuestError error;
    Create(cinfo, error);
    GuestSource source = MakeSource(data, 64, false);
    cinfo.src = reinterpret_cast<jpeg_source_mgr*>(&source.pub);
    Require(jpeg_read_header_nid_postfix(Guest(cinfo), TRUE) == JPEG_HEADER_OK, "progressive header");
    Require(jpeg_has_multiple_scans_nid_postfix(Guest(cinfo)) == TRUE, "progressive has several scans");
    cinfo.buffered_image = TRUE;
    Require(jpeg_start_decompress_nid_postfix(Guest(cinfo)) == TRUE, "buffered start");
    Require(jpeg_input_complete_nid_postfix(Guest(cinfo)) == FALSE, "input not complete yet");
    const std::size_t stride = static_cast<std::size_t>(cinfo.output_width) * cinfo.output_components;
    std::vector<std::uint8_t> pixels(stride * cinfo.output_height);
    int passes = 0;
    while (true) {
        int status;
        do status = jpeg_consume_input_nid_postfix(Guest(cinfo));
        while (status != JPEG_SUSPENDED && status != JPEG_REACHED_EOI && status != JPEG_REACHED_SOS);
        Require(jpeg_start_output_nid_postfix(Guest(cinfo), cinfo.input_scan_number) == TRUE, "start output");
        while (cinfo.output_scanline < cinfo.output_height) {
            JSAMPROW row = pixels.data() + static_cast<std::size_t>(cinfo.output_scanline) * stride;
            jpeg_read_scanlines_nid_postfix(Guest(cinfo), &row, 1);
        }
        Require(jpeg_finish_output_nid_postfix(Guest(cinfo)) == TRUE, "finish output");
        ++passes;
        if (jpeg_input_complete_nid_postfix(Guest(cinfo))) break;
    }
    Require(passes > 1, "several output passes");
    Require(jpeg_finish_decompress_nid_postfix(Guest(cinfo)) == TRUE, "progressive finish");
    const Output reference = Reference(data, 1, JCS_RGB);
    Require(pixels == reference.pixels, "progressive final pass matches libjpeg");
    jpeg_destroy_decompress_nid_postfix(Guest(cinfo));
}

void VirtualArrays() {
    jpeg_decompress_struct cinfo;
    GuestError error;
    Create(cinfo, error);
    auto* memory = reinterpret_cast<GuestJpegMemoryMgr*>(cinfo.mem);
    void* large = memory->alloc_large(&cinfo, JPOOL_IMAGE, 4096);
    std::uint8_t** samples = memory->alloc_sarray(&cinfo, JPOOL_IMAGE, 16, 3);
    void* blocks = memory->alloc_barray(&cinfo, JPOOL_IMAGE, 2, 2);
    Require(large && samples && samples[2] && blocks, "guest allocations");
    samples[2][15] = 7;
    void* virtualSamples = memory->request_virt_sarray(&cinfo, JPOOL_IMAGE, TRUE, 16, 8, 4);
    void* virtualBlocks = memory->request_virt_barray(&cinfo, JPOOL_IMAGE, TRUE, 2, 8, 4);
    memory->realize_virt_arrays(&cinfo);
    std::uint8_t** rows = memory->access_virt_sarray(&cinfo, virtualSamples, 0, 4, TRUE);
    Require(rows && rows[0][0] == 0 && rows[3][15] == 0, "zeroed virtual samples");
    Require(memory->access_virt_barray(&cinfo, virtualBlocks, 0, 4, TRUE) != nullptr, "virtual blocks");
    memory->free_pool(&cinfo, JPOOL_IMAGE);
    RequireGuestView(cinfo, error, nullptr);
    memory->self_destruct(&cinfo);
    Require(cinfo.mem == nullptr, "self_destruct clears the memory manager");
    jpeg_destroy_decompress_nid_postfix(Guest(cinfo));
}

void Errors() {
    const auto data = Encode(Options{40, 40, false, 0, JCS_RGB}, "", {});
    std::vector<std::uint8_t> corrupt = {0xFF, 0xD8, 0xFF, 0xC0, 0x00, 0x02, 0x01, 0x02};
    jpeg_decompress_struct cinfo;
    GuestError error;
    Create(cinfo, error);
    GuestSource source = MakeSource(corrupt, 4, false);
    cinfo.src = reinterpret_cast<jpeg_source_mgr*>(&source.pub);
    if (setjmp(error.jump) == 0) {
        jpeg_read_header_nid_postfix(Guest(cinfo), TRUE);
        Require(false, "corrupt header returned");
    }
    Require(error.exits == 1 && std::strlen(error.message) > 0, "error_exit called with a message");
    RequireGuestView(cinfo, error, &source);
    jpeg_destroy_decompress_nid_postfix(Guest(cinfo));
    Require(cinfo.mem == nullptr, "destroy after a long jump");

    std::vector<std::uint8_t> truncated(data.begin(), data.begin() + static_cast<std::ptrdiff_t>(data.size() * 2 / 3));
    Create(cinfo, error);
    GuestSource partial = MakeSource(truncated, 16, false);
    const Output output = Decode(cinfo, error, partial, 1, JCS_RGB, 4);
    Require(output.width == 40 && output.height == 40, "truncated image decodes");
    Require(error.pub.num_warnings > 0 && error.exits == 0, "premature end is a warning");
    jpeg_destroy_decompress_nid_postfix(Guest(cinfo));
}

}

int main() {
    Defaults();
    Baseline();
    Suspending();
    Progressive();
    VirtualArrays();
    Errors();
    return 0;
}
