/*
 * Copyright (C) 2015, International Business Machines
 * Corporation and others.  All Rights Reserved.
 *
 * file name: digitaffix.cpp
 */

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "digitaffix.h"
#include "fphdlimp.h"
#include "uassert.h"
#include "unistrappender.h"

U_NAMESPACE_BEGIN

DigitAffix::DigitAffix() : fAffix(), fAnnotations() {
}

DigitAffix::DigitAffix(
        const UChar *value, int32_t charCount, int32_t fieldId)
        : fAffix(value, charCount),
          fAnnotations(charCount, (UChar) fieldId, charCount) {
}

void
DigitAffix::remove() {
    fAffix.remove();
    fAnnotations.remove();
}

void
DigitAffix::appendUChar(UChar value, int32_t fieldId) {
    fAffix.append(value);
    fAnnotations.append((UChar) fieldId);
}

void
DigitAffix::append(const UnicodeString &value, int32_t fieldId) {
    fAffix.append(value);
    {
        UnicodeStringAppender appender(fAnnotations);
        int32_t len = value.length();
        for (int32_t i = 0; i < len; ++i) {
            appender.append((UChar) fieldId);
        }
    }
}

void
DigitAffix::setTo(const UnicodeString &value, int32_t fieldId) {
    fAffix = value;
    fAnnotations.remove();
    {
        UnicodeStringAppender appender(fAnnotations);
        int32_t len = value.length();
        for (int32_t i = 0; i < len; ++i) {
            appender.append((UChar) fieldId);
        }
    }
}

void
DigitAffix::append(const UChar *value, int32_t charCount, int32_t fieldId) {
    fAffix.append(value, charCount);
    {
        UnicodeStringAppender appender(fAnnotations);
        for (int32_t i = 0; i < charCount; ++i) {
            appender.append((UChar) fieldId);
        }
    }
}

UnicodeString &
DigitAffix::format(FieldPositionHandler &handler, UnicodeString &appendTo) const {
    int32_t len = fAffix.length();
    if (len == 0) {
        return appendTo;
    }
    if (!handler.isRecording()) {
        return appendTo.append(fAffix);
    }
    U_ASSERT(fAffix.length() == fAnnotations.length());
    int32_t appendToStart = appendTo.length();
    int32_t lastId = (int32_t) fAnnotations.charAt(0);
    int32_t lastIdStart = 0;
    for (int32_t i = 1; i < len; ++i) {
        int32_t id = (int32_t) fAnnotations.charAt(i);
        if (id != lastId) {
            if (lastId != UNUM_FIELD_COUNT) {
                handler.addAttribute(lastId, appendToStart + lastIdStart, appendToStart + i);
            }
            lastId = id;
            lastIdStart = i;
        }
    }
    if (lastId != UNUM_FIELD_COUNT) {
        handler.addAttribute(lastId, appendToStart + lastIdStart, appendToStart + len);
    }
    return appendTo.append(fAffix);
}

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */
