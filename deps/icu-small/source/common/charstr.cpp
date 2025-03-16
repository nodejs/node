// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*   Copyright (C) 2010-2015, International Business Machines
*   Corporation and others.  All Rights Reserved.
*******************************************************************************
*   file name:  charstr.cpp
*   encoding:   UTF-8
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2010may19
*   created by: Markus W. Scherer
*/

#include <cstdlib>

#include "unicode/utypes.h"
#include "unicode/putil.h"
#include "charstr.h"
#include "cmemory.h"
#include "cstring.h"
#include "uinvchar.h"
#include "ustr_imp.h"

U_NAMESPACE_BEGIN

CharString::CharString(CharString&& src) noexcept
        : buffer(std::move(src.buffer)), len(src.len) {
    src.len = 0;  // not strictly necessary because we make no guarantees on the source string
}

CharString& CharString::operator=(CharString&& src) noexcept {
    buffer = std::move(src.buffer);
    len = src.len;
    src.len = 0;  // not strictly necessary because we make no guarantees on the source string
    return *this;
}

char *CharString::cloneData(UErrorCode &errorCode) const {
    if (U_FAILURE(errorCode)) { return nullptr; }
    char *p = static_cast<char *>(uprv_malloc(len + 1));
    if (p == nullptr) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        return nullptr;
    }
    uprv_memcpy(p, buffer.getAlias(), len + 1);
    return p;
}

int32_t CharString::extract(char *dest, int32_t capacity, UErrorCode &errorCode) const {
    if (U_FAILURE(errorCode)) { return len; }
    if (capacity < 0 || (capacity > 0 && dest == nullptr)) {
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
        return len;
    }
    const char *src = buffer.getAlias();
    if (0 < len && len <= capacity && src != dest) {
        uprv_memcpy(dest, src, len);
    }
    return u_terminateChars(dest, capacity, len, &errorCode);
}

CharString &CharString::copyFrom(const CharString &s, UErrorCode &errorCode) {
    if(U_SUCCESS(errorCode) && this!=&s && ensureCapacity(s.len+1, 0, errorCode)) {
        len=s.len;
        uprv_memcpy(buffer.getAlias(), s.buffer.getAlias(), len+1);
    }
    return *this;
}

CharString &CharString::copyFrom(StringPiece s, UErrorCode &errorCode) {
    if (U_FAILURE(errorCode)) {
        return *this;
    }
    len = 0;
    append(s, errorCode);
    return *this;
}

int32_t CharString::lastIndexOf(char c) const {
    for(int32_t i=len; i>0;) {
        if(buffer[--i]==c) {
            return i;
        }
    }
    return -1;
}

bool CharString::contains(StringPiece s) const {
    if (s.empty()) { return false; }
    const char *p = buffer.getAlias();
    int32_t lastStart = len - s.length();
    for (int32_t i = 0; i <= lastStart; ++i) {
        if (uprv_memcmp(p + i, s.data(), s.length()) == 0) {
            return true;
        }
    }
    return false;
}

CharString &CharString::truncate(int32_t newLength) {
    if(newLength<0) {
        newLength=0;
    }
    if(newLength<len) {
        buffer[len=newLength]=0;
    }
    return *this;
}

CharString &CharString::append(char c, UErrorCode &errorCode) {
    if(ensureCapacity(len+2, 0, errorCode)) {
        buffer[len++]=c;
        buffer[len]=0;
    }
    return *this;
}

CharString &CharString::append(const char *s, int32_t sLength, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) {
        return *this;
    }
    if(sLength<-1 || (s==nullptr && sLength!=0)) {
        errorCode=U_ILLEGAL_ARGUMENT_ERROR;
        return *this;
    }
    if(sLength<0) {
        sLength= static_cast<int32_t>(uprv_strlen(s));
    }
    if(sLength>0) {
        if(s==(buffer.getAlias()+len)) {
            // The caller wrote into the getAppendBuffer().
            if(sLength>=(buffer.getCapacity()-len)) {
                // The caller wrote too much.
                errorCode=U_INTERNAL_PROGRAM_ERROR;
            } else {
                buffer[len+=sLength]=0;
            }
        } else if(buffer.getAlias()<=s && s<(buffer.getAlias()+len) &&
                  sLength>=(buffer.getCapacity()-len)
        ) {
            // (Part of) this string is appended to itself which requires reallocation,
            // so we have to make a copy of the substring and append that.
            return append(CharString(s, sLength, errorCode), errorCode);
        } else if(ensureCapacity(len+sLength+1, 0, errorCode)) {
            uprv_memcpy(buffer.getAlias()+len, s, sLength);
            buffer[len+=sLength]=0;
        }
    }
    return *this;
}

CharString &CharString::appendNumber(int64_t number, UErrorCode &status) {
    if (number < 0) {
        this->append('-', status);
        if (U_FAILURE(status)) {
            return *this;
        }
    }

    if (number == 0) {
        this->append('0', status);
        return *this;
    }

    int32_t numLen = 0;
    while (number != 0) {
        int32_t residue = number % 10;
        number /= 10;
        this->append(std::abs(residue) + '0', status);
        numLen++;
        if (U_FAILURE(status)) {
            return *this;
        }
    }

    int32_t start = this->length() - numLen, end = this->length() - 1;
    while(start < end) {
        std::swap(this->data()[start++], this->data()[end--]);
    }

    return *this;
}

char *CharString::getAppendBuffer(int32_t minCapacity,
                                  int32_t desiredCapacityHint,
                                  int32_t &resultCapacity,
                                  UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) {
        resultCapacity=0;
        return nullptr;
    }
    int32_t appendCapacity=buffer.getCapacity()-len-1;  // -1 for NUL
    if(appendCapacity>=minCapacity) {
        resultCapacity=appendCapacity;
        return buffer.getAlias()+len;
    }
    if(ensureCapacity(len+minCapacity+1, len+desiredCapacityHint+1, errorCode)) {
        resultCapacity=buffer.getCapacity()-len-1;
        return buffer.getAlias()+len;
    }
    resultCapacity=0;
    return nullptr;
}

CharString &CharString::appendInvariantChars(const UnicodeString &s, UErrorCode &errorCode) {
    return appendInvariantChars(s.getBuffer(), s.length(), errorCode);
}

CharString &CharString::appendInvariantChars(const char16_t* uchars, int32_t ucharsLen, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) {
        return *this;
    }
    if (!uprv_isInvariantUString(uchars, ucharsLen)) {
        errorCode = U_INVARIANT_CONVERSION_ERROR;
        return *this;
    }
    if(ensureCapacity(len+ucharsLen+1, 0, errorCode)) {
        u_UCharsToChars(uchars, buffer.getAlias()+len, ucharsLen);
        len += ucharsLen;
        buffer[len] = 0;
    }
    return *this;
}

UBool CharString::ensureCapacity(int32_t capacity,
                                 int32_t desiredCapacityHint,
                                 UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) {
        return false;
    }
    if(capacity>buffer.getCapacity()) {
        if(desiredCapacityHint==0) {
            desiredCapacityHint=capacity+buffer.getCapacity();
        }
        if( (desiredCapacityHint<=capacity || buffer.resize(desiredCapacityHint, len+1)==nullptr) &&
            buffer.resize(capacity, len+1)==nullptr
        ) {
            errorCode=U_MEMORY_ALLOCATION_ERROR;
            return false;
        }
    }
    return true;
}

CharString &CharString::appendPathPart(StringPiece s, UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) {
        return *this;
    }
    if(s.length()==0) {
        return *this;
    }
    char c;
    if(len>0 && (c=buffer[len-1])!=U_FILE_SEP_CHAR && c!=U_FILE_ALT_SEP_CHAR) {
        append(getDirSepChar(), errorCode);
    }
    append(s, errorCode);
    return *this;
}

CharString &CharString::ensureEndsWithFileSeparator(UErrorCode &errorCode) {
    char c;
    if(U_SUCCESS(errorCode) && len>0 &&
            (c=buffer[len-1])!=U_FILE_SEP_CHAR && c!=U_FILE_ALT_SEP_CHAR) {
        append(getDirSepChar(), errorCode);
    }
    return *this;
}

char CharString::getDirSepChar() const {
    char dirSepChar = U_FILE_SEP_CHAR;
#if (U_FILE_SEP_CHAR != U_FILE_ALT_SEP_CHAR)
    // We may need to return a different directory separator when building for Cygwin or MSYS2.
    if(len>0 && !uprv_strchr(data(), U_FILE_SEP_CHAR) && uprv_strchr(data(), U_FILE_ALT_SEP_CHAR))
        dirSepChar = U_FILE_ALT_SEP_CHAR;
#endif
    return dirSepChar;
}

U_NAMESPACE_END
