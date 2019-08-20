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

#include "unicode/utypes.h"
#include "unicode/putil.h"
#include "charstr.h"
#include "cmemory.h"
#include "cstring.h"
#include "uinvchar.h"

U_NAMESPACE_BEGIN

CharString::CharString(CharString&& src) U_NOEXCEPT
        : buffer(std::move(src.buffer)), len(src.len) {
    src.len = 0;  // not strictly necessary because we make no guarantees on the source string
}

CharString& CharString::operator=(CharString&& src) U_NOEXCEPT {
    buffer = std::move(src.buffer);
    len = src.len;
    src.len = 0;  // not strictly necessary because we make no guarantees on the source string
    return *this;
}

CharString &CharString::copyFrom(const CharString &s, UErrorCode &errorCode) {
    if(U_SUCCESS(errorCode) && this!=&s && ensureCapacity(s.len+1, 0, errorCode)) {
        len=s.len;
        uprv_memcpy(buffer.getAlias(), s.buffer.getAlias(), len+1);
    }
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
    if(sLength<-1 || (s==NULL && sLength!=0)) {
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

char *CharString::getAppendBuffer(int32_t minCapacity,
                                  int32_t desiredCapacityHint,
                                  int32_t &resultCapacity,
                                  UErrorCode &errorCode) {
    if(U_FAILURE(errorCode)) {
        resultCapacity=0;
        return NULL;
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
    return NULL;
}

CharString &CharString::appendInvariantChars(const UnicodeString &s, UErrorCode &errorCode) {
    return appendInvariantChars(s.getBuffer(), s.length(), errorCode);
}

CharString &CharString::appendInvariantChars(const UChar* uchars, int32_t ucharsLen, UErrorCode &errorCode) {
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
        return FALSE;
    }
    if(capacity>buffer.getCapacity()) {
        if(desiredCapacityHint==0) {
            desiredCapacityHint=capacity+buffer.getCapacity();
        }
        if( (desiredCapacityHint<=capacity || buffer.resize(desiredCapacityHint, len+1)==NULL) &&
            buffer.resize(capacity, len+1)==NULL
        ) {
            errorCode=U_MEMORY_ALLOCATION_ERROR;
            return FALSE;
        }
    }
    return TRUE;
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
        append(U_FILE_SEP_CHAR, errorCode);
    }
    append(s, errorCode);
    return *this;
}

CharString &CharString::ensureEndsWithFileSeparator(UErrorCode &errorCode) {
    char c;
    if(U_SUCCESS(errorCode) && len>0 &&
            (c=buffer[len-1])!=U_FILE_SEP_CHAR && c!=U_FILE_ALT_SEP_CHAR) {
        append(U_FILE_SEP_CHAR, errorCode);
    }
    return *this;
}

U_NAMESPACE_END
