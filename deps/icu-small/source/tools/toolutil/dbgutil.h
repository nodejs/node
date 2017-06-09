// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

/*
************************************************************************
* Copyright (c) 2007-2012, International Business Machines
* Corporation and others.  All Rights Reserved.
************************************************************************
*/

/** C++ Utilities to aid in debugging **/

#ifndef _DBGUTIL_H
#define _DBGUTIL_H

#include "unicode/utypes.h"
#include "udbgutil.h"
#include "unicode/unistr.h"

#if !UCONFIG_NO_FORMATTING

U_TOOLUTIL_API const icu::UnicodeString& U_EXPORT2
udbg_enumString(UDebugEnumType type, int32_t field);

/**
 * @return enum offset, or UDBG_INVALID_ENUM on error
 */
U_CAPI int32_t U_EXPORT2
udbg_enumByString(UDebugEnumType type, const icu::UnicodeString& string);

/**
 * Convert a UnicodeString (with ascii digits) into a number.
 * @param s string
 * @return numerical value, or 0 on error
 */
U_CAPI int32_t U_EXPORT2 udbg_stoi(const icu::UnicodeString &s);

U_CAPI double U_EXPORT2 udbg_stod(const icu::UnicodeString &s);

U_CAPI icu::UnicodeString * U_EXPORT2
udbg_escape(const icu::UnicodeString &s, icu::UnicodeString *dst);

#endif

#endif
