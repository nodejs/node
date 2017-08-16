// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*   Copyright (C) 2011-2012, International Business Machines
*   Corporation and others.  All Rights Reserved.
*******************************************************************************
*   file name:  appendable.cpp
*   encoding:   UTF-8
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2010dec07
*   created by: Markus W. Scherer
*/

#include "unicode/utypes.h"
#include "unicode/appendable.h"
#include "unicode/utf16.h"

U_NAMESPACE_BEGIN

Appendable::~Appendable() {}

UBool
Appendable::appendCodePoint(UChar32 c) {
    if(c<=0xffff) {
        return appendCodeUnit((UChar)c);
    } else {
        return appendCodeUnit(U16_LEAD(c)) && appendCodeUnit(U16_TRAIL(c));
    }
}

UBool
Appendable::appendString(const UChar *s, int32_t length) {
    if(length<0) {
        UChar c;
        while((c=*s++)!=0) {
            if(!appendCodeUnit(c)) {
                return FALSE;
            }
        }
    } else if(length>0) {
        const UChar *limit=s+length;
        do {
            if(!appendCodeUnit(*s++)) {
                return FALSE;
            }
        } while(s<limit);
    }
    return TRUE;
}

UBool
Appendable::reserveAppendCapacity(int32_t /*appendCapacity*/) {
    return TRUE;
}

UChar *
Appendable::getAppendBuffer(int32_t minCapacity,
                            int32_t /*desiredCapacityHint*/,
                            UChar *scratch, int32_t scratchCapacity,
                            int32_t *resultCapacity) {
    if(minCapacity<1 || scratchCapacity<minCapacity) {
        *resultCapacity=0;
        return NULL;
    }
    *resultCapacity=scratchCapacity;
    return scratch;
}

// UnicodeStringAppendable is implemented in unistr.cpp.

U_NAMESPACE_END
