// © 2017 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

// bytesinkutil.cpp
// created: 2017sep14 Markus W. Scherer

#include "unicode/utypes.h"
#include "unicode/bytestream.h"
#include "unicode/edits.h"
#include "unicode/stringoptions.h"
#include "unicode/utf8.h"
#include "unicode/utf16.h"
#include "bytesinkutil.h"
#include "cmemory.h"
#include "uassert.h"

U_NAMESPACE_BEGIN

UBool
ByteSinkUtil::appendChange(int32_t length, const char16_t *s16, int32_t s16Length,
                           ByteSink &sink, Edits *edits, UErrorCode &errorCode) {
    if (U_FAILURE(errorCode)) { return FALSE; }
    char scratch[200];
    int32_t s8Length = 0;
    for (int32_t i = 0; i < s16Length;) {
        int32_t capacity;
        int32_t desiredCapacity = s16Length - i;
        if (desiredCapacity < (INT32_MAX / 3)) {
            desiredCapacity *= 3;  // max 3 UTF-8 bytes per UTF-16 code unit
        } else if (desiredCapacity < (INT32_MAX / 2)) {
            desiredCapacity *= 2;
        } else {
            desiredCapacity = INT32_MAX;
        }
        char *buffer = sink.GetAppendBuffer(U8_MAX_LENGTH, desiredCapacity,
                                            scratch, UPRV_LENGTHOF(scratch), &capacity);
        capacity -= U8_MAX_LENGTH - 1;
        int32_t j = 0;
        for (; i < s16Length && j < capacity;) {
            UChar32 c;
            U16_NEXT_UNSAFE(s16, i, c);
            U8_APPEND_UNSAFE(buffer, j, c);
        }
        if (j > (INT32_MAX - s8Length)) {
            errorCode = U_INDEX_OUTOFBOUNDS_ERROR;
            return FALSE;
        }
        sink.Append(buffer, j);
        s8Length += j;
    }
    if (edits != nullptr) {
        edits->addReplace(length, s8Length);
    }
    return TRUE;
}

UBool
ByteSinkUtil::appendChange(const uint8_t *s, const uint8_t *limit,
                           const char16_t *s16, int32_t s16Length,
                           ByteSink &sink, Edits *edits, UErrorCode &errorCode) {
    if (U_FAILURE(errorCode)) { return FALSE; }
    if ((limit - s) > INT32_MAX) {
        errorCode = U_INDEX_OUTOFBOUNDS_ERROR;
        return FALSE;
    }
    return appendChange((int32_t)(limit - s), s16, s16Length, sink, edits, errorCode);
}

void
ByteSinkUtil::appendCodePoint(int32_t length, UChar32 c, ByteSink &sink, Edits *edits) {
    char s8[U8_MAX_LENGTH];
    int32_t s8Length = 0;
    U8_APPEND_UNSAFE(s8, s8Length, c);
    if (edits != nullptr) {
        edits->addReplace(length, s8Length);
    }
    sink.Append(s8, s8Length);
}

namespace {

// See unicode/utf8.h U8_APPEND_UNSAFE().
inline uint8_t getTwoByteLead(UChar32 c) { return (uint8_t)((c >> 6) | 0xc0); }
inline uint8_t getTwoByteTrail(UChar32 c) { return (uint8_t)((c & 0x3f) | 0x80); }

}  // namespace

void
ByteSinkUtil::appendTwoBytes(UChar32 c, ByteSink &sink) {
    U_ASSERT(0x80 <= c && c <= 0x7ff);  // 2-byte UTF-8
    char s8[2] = { (char)getTwoByteLead(c), (char)getTwoByteTrail(c) };
    sink.Append(s8, 2);
}

UBool
ByteSinkUtil::appendUnchanged(const uint8_t *s, int32_t length,
                              ByteSink &sink, uint32_t options, Edits *edits,
                              UErrorCode &errorCode) {
    if (U_FAILURE(errorCode)) { return FALSE; }
    if (length > 0) {
        if (edits != nullptr) {
            edits->addUnchanged(length);
        }
        if ((options & U_OMIT_UNCHANGED_TEXT) == 0) {
            sink.Append(reinterpret_cast<const char *>(s), length);
        }
    }
    return TRUE;
}

UBool
ByteSinkUtil::appendUnchanged(const uint8_t *s, const uint8_t *limit,
                              ByteSink &sink, uint32_t options, Edits *edits,
                              UErrorCode &errorCode) {
    if (U_FAILURE(errorCode)) { return FALSE; }
    if ((limit - s) > INT32_MAX) {
        errorCode = U_INDEX_OUTOFBOUNDS_ERROR;
        return FALSE;
    }
    return appendUnchanged(s, (int32_t)(limit - s), sink, options, edits, errorCode);
}

U_NAMESPACE_END
