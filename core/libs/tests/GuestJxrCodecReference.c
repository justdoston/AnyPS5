#include "strcodec.h"
#include "JXRGlue.h"
#include <stdint.h>
#include <string.h>

typedef struct tagPostProcInfo* ReferencePostProcRows[2];

int64_t JxrRefIid_nid_no_patch(const char* extension, int encode, uint32_t* value) {
    const PKIID* iid = NULL;
    ERR result = encode ? GetImageEncodeIID(extension, &iid) : GetImageDecodeIID(extension, &iid);
    if (result == WMP_errSuccess) *value = *iid;
    return result;
}

int JxrRefPixelFormatFromHash_nid_no_patch(uint8_t hash, uint8_t* guid) {
    const PKPixelFormatGUID* found = GetPixelFormatFromHash(hash);
    if (!found) return 0;
    memcpy(guid, found, 16);
    return 1;
}

int32_t JxrRefInitPostProc_nid_no_patch(ReferencePostProcRows* info, size_t width, size_t channels) {
    return initPostProc(info, width, channels);
}

void JxrRefTermPostProc_nid_no_patch(ReferencePostProcRows* info, size_t channels) {
    termPostProc(info, channels);
}

void JxrRefSlide_nid_no_patch(ReferencePostProcRows* info, size_t channels, size_t width, int32_t top, int32_t bottom) {
    slideOneMBRow(info, channels, width, top, bottom);
}

void JxrRefUpdate_nid_no_patch(ReferencePostProcRows* info, int32_t* macroblock, size_t x, size_t channel) {
    updatePostProcInfo(info, macroblock, x, channel);
}

void JxrRefPostMB_nid_no_patch(ReferencePostProcRows* info, int32_t* first, int32_t* second, size_t x, size_t channel, int32_t threshold) {
    postProcMB(info, first, second, x, channel, threshold);
}

void JxrRefPostBlock_nid_no_patch(ReferencePostProcRows* info, int32_t* first, int32_t* second, size_t x, size_t channel, int32_t threshold) {
    postProcBlock(info, first, second, x, channel, threshold);
}

void JxrRefRemap_nid_no_patch(void* quantizer, int32_t shift, int32_t scaled) {
    remapQP((CWMIQuantizer*)quantizer, shift, scaled);
}

void JxrRefFormat_nid_no_patch(void** quantizers, uint8_t mode, size_t channels, size_t position, int32_t shiftedUv, int32_t scaled) {
    formatQuantizer((CWMIQuantizer**)quantizers, mode, channels, position, shiftedUv, scaled);
}

int32_t JxrRefACPred_nid_no_patch(void* info, int32_t colorFormat) {
    return getACPredMode((CWMIMBInfo*)info, (COLORFORMAT)colorFormat);
}
