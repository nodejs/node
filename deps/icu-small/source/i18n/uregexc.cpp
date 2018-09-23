// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*   Copyright (C) 2003-2006, International Business Machines
*   Corporation and others.  All Rights Reserved.
*******************************************************************************
*   file name:   regexc.cpp
*   description: The purpose of this function is to separate the codepage
*       conversion from the rest of the uregex_ API. This can removes any
*       dependency on codepage conversion, which reduces the overhead of
*/

#include "unicode/uregex.h"
#include "unicode/unistr.h"

U_NAMESPACE_USE

//----------------------------------------------------------------------------------------
//
//    uregex_openC
//
//----------------------------------------------------------------------------------------
#if !UCONFIG_NO_CONVERSION && !UCONFIG_NO_REGULAR_EXPRESSIONS

U_CAPI URegularExpression * U_EXPORT2
uregex_openC( const char           *pattern,
                    uint32_t        flags,
                    UParseError    *pe,
                    UErrorCode     *status) {
    if (U_FAILURE(*status)) {
        return NULL;
    }
    if (pattern == NULL) {
        *status = U_ILLEGAL_ARGUMENT_ERROR;
        return NULL;
    }

    UnicodeString patString(pattern);
    return uregex_open(patString.getBuffer(), patString.length(), flags, pe, status);
}
#endif
