// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
**********************************************************************
*   Copyright (C) 2001, International Business Machines
*   Corporation and others.  All Rights Reserved.
**********************************************************************
*   file name:  umsg_imp.h
*   encoding:   UTF-8
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2001jun22
*   created by: George Rhoten
*/

#ifndef UMISC_H
#define UMISC_H

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

/* global variables used by the C and C++ message formatting API. */

extern const char16_t  *g_umsgTypeList[];
extern const char16_t  *g_umsgModifierList[];
extern const char16_t  *g_umsgDateModifierList[];
extern const int32_t g_umsgListLength;

extern const char16_t g_umsg_number[];
extern const char16_t g_umsg_date[];
extern const char16_t g_umsg_time[];
extern const char16_t g_umsg_choice[];

extern const char16_t g_umsg_currency[];
extern const char16_t g_umsg_percent[];
extern const char16_t g_umsg_integer[];

extern const char16_t g_umsg_short[];
extern const char16_t g_umsg_medium[];
extern const char16_t g_umsg_long[];
extern const char16_t g_umsg_full[];

#endif /* #if !UCONFIG_NO_FORMATTING */

#endif
