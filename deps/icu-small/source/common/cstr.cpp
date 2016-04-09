/*
*******************************************************************************
*   Copyright (C) 2015-2016, International Business Machines
*   Corporation and others.  All Rights Reserved.
*******************************************************************************
*   file name:  charstr.cpp
*/
#include "unicode/utypes.h"
#include "unicode/unistr.h"

#include "charstr.h"
#include "cstr.h"

U_NAMESPACE_BEGIN

CStr::CStr(const UnicodeString &in) {
    UErrorCode status = U_ZERO_ERROR;
    int32_t length = in.extract(0, in.length(), NULL, (uint32_t)0);
    int32_t resultCapacity = 0;
    char *buf = s.getAppendBuffer(length, length, resultCapacity, status);
    if (U_SUCCESS(status)) {
        in.extract(0, in.length(), buf, resultCapacity);
        s.append(buf, length, status);
    }
}

CStr::~CStr() {
}

const char * CStr::operator ()() const {
    return s.data();
}

U_NAMESPACE_END
