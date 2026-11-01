#include "strcodec.h"
#include "JXRGlue.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

int64_t JxrRefEnumConversions_nid_no_patch(const uint8_t* source, uint32_t index, uint8_t* target) {
    GUID from;
    const PKPixelFormatGUID* to = NULL;
    ERR result;
    memcpy(&from, source, sizeof(from));
    result = PKFormatConverter_EnumConversions(&from, index, &to);
    memcpy(target, to, 16);
    return result;
}

size_t JxrRefLayout_nid_no_patch(size_t* values) {
    size_t count = 0;
    values[count++] = sizeof(PKImageDecode);
    values[count++] = offsetof(PKImageDecode, Release);
    values[count++] = offsetof(PKImageDecode, pStream);
    values[count++] = offsetof(PKImageDecode, fStreamOwner);
    values[count++] = offsetof(PKImageDecode, offStart);
    values[count++] = offsetof(PKImageDecode, guidPixFormat);
    values[count++] = offsetof(PKImageDecode, uWidth);
    values[count++] = offsetof(PKImageDecode, fResX);
    values[count++] = offsetof(PKImageDecode, cFrame);
    values[count++] = offsetof(PKImageDecode, WMP);
    values[count++] = sizeof(((PKImageDecode*)0)->WMP.wmiDEMisc);
    values[count++] = sizeof(PKImageEncode);
    values[count++] = offsetof(PKImageEncode, WritePixelsBandedBegin);
    values[count++] = offsetof(PKImageEncode, CreateNewFrame);
    values[count++] = offsetof(PKImageEncode, Release);
    values[count++] = offsetof(PKImageEncode, pStream);
    values[count++] = offsetof(PKImageEncode, offStart);
    values[count++] = offsetof(PKImageEncode, guidPixFormat);
    values[count++] = offsetof(PKImageEncode, uWidth);
    values[count++] = offsetof(PKImageEncode, fResX);
    values[count++] = offsetof(PKImageEncode, cFrame);
    values[count++] = offsetof(PKImageEncode, bWMP);
    values[count++] = sizeof(PKFormatConverter);
    values[count++] = offsetof(PKFormatConverter, Convert);
    values[count++] = offsetof(PKFormatConverter, pDecoder);
    values[count++] = offsetof(PKFormatConverter, enPixelFormat);
    values[count++] = sizeof(PKFactory);
    values[count++] = sizeof(PKCodecFactory);
    values[count++] = sizeof(PKStream);
    return count;
}
