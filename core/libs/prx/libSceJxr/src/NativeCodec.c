#include "strcodec.h"
#include "JXRGlue.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

_Static_assert(sizeof(struct tagPostProcInfo) == 88, "post-processing record size");
_Static_assert(offsetof(struct tagPostProcInfo, iBlockDC) == 8, "post-processing DC offset");
_Static_assert(offsetof(struct tagPostProcInfo, ucBlockTexture) == 72, "post-processing texture offset");
_Static_assert(sizeof(CWMIQuantizer) == 20 && offsetof(CWMIQuantizer, iQP) == 4, "quantizer layout");
_Static_assert(offsetof(CWMIQuantizer, iExp) == 16, "quantizer exponent offset");
_Static_assert(sizeof(CWMIMBInfo) == 1160 && offsetof(CWMIMBInfo, iOrientation) == 1024, "macroblock layout");
_Static_assert(offsetof(CWMIMBInfo, iDiffCBP) == 1092 && offsetof(CWMIMBInfo, iQIndexLP) == 1156, "macroblock offsets");
_Static_assert(sizeof(PixelI) == 4 && sizeof(Bool) == 4 && sizeof(COLORFORMAT) == 4, "scalar sizes");
_Static_assert(sizeof(size_t) == 8, "LP64 sizes");

typedef struct tagPostProcInfo* PostProcRows[2];

void JxrSlideOneMBRow_nid_no_patch(PostProcRows* info, size_t channels, size_t width, int32_t top, int32_t bottom) {
    slideOneMBRow(info, channels, width, top, bottom);
}

void JxrUpdatePostProcInfo_nid_no_patch(PostProcRows* info, int32_t* macroblock, size_t x, size_t channel) {
    updatePostProcInfo(info, macroblock, x, channel);
}

void JxrPostProcMB_nid_no_patch(PostProcRows* info, int32_t* first, int32_t* second, size_t x, size_t channel, int32_t threshold) {
    postProcMB(info, first, second, x, channel, threshold);
}

void JxrPostProcBlock_nid_no_patch(PostProcRows* info, int32_t* first, int32_t* second, size_t x, size_t channel, int32_t threshold) {
    postProcBlock(info, first, second, x, channel, threshold);
}

void JxrRemapQP_nid_no_patch(void* quantizer, int32_t shift, int32_t scaled) {
    remapQP((CWMIQuantizer*)quantizer, shift, scaled);
}

void JxrFormatQuantizer_nid_no_patch(void** quantizers, uint8_t mode, size_t channels, size_t position, int32_t shiftedUv, int32_t scaled) {
    formatQuantizer((CWMIQuantizer**)quantizers, mode, channels, position, shiftedUv, scaled);
}

int32_t JxrACPredMode_nid_no_patch(void* info, int32_t colorFormat) {
    return getACPredMode((CWMIMBInfo*)info, (COLORFORMAT)colorFormat);
}

int JxrPixelFormatFromHash_nid_no_patch(uint8_t hash, uint8_t* guid) {
    const PKPixelFormatGUID* found = GetPixelFormatFromHash(hash);
    if (!found) return 0;
    memcpy(guid, found, 16);
    return 1;
}
