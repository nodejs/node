// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/********************************************************************
 * COPYRIGHT: 
 * Copyright (c) 1997-2011, International Business Machines Corporation and
 * others. All Rights Reserved.
 * Copyright (C) 2010 , Yahoo! Inc. 
 ********************************************************************
 * File selectfmtimpl.h
 *
 *   Date        Name        Description
 *   11/11/09    kirtig      Finished first cut of implementation.
 *********************************************************************/


#ifndef SELFMTIMPL
#define SELFMTIMPL

#if !UCONFIG_NO_FORMATTING

#include "unicode/format.h"
#include "unicode/locid.h"
#include "unicode/parseerr.h"
#include "unicode/utypes.h"
#include "uvector.h"
#include "hash.h"

U_NAMESPACE_BEGIN

#define DOT               ((char16_t)0x002E)
#define SINGLE_QUOTE      ((char16_t)0x0027)
#define SLASH             ((char16_t)0x002F)
#define BACKSLASH         ((char16_t)0x005C)
#define SPACE             ((char16_t)0x0020)
#define TAB               ((char16_t)0x0009)
#define QUOTATION_MARK    ((char16_t)0x0022)
#define ASTERISK          ((char16_t)0x002A)
#define COMMA             ((char16_t)0x002C)
#define HYPHEN            ((char16_t)0x002D)
#define U_ZERO            ((char16_t)0x0030)
#define U_ONE             ((char16_t)0x0031)
#define U_TWO             ((char16_t)0x0032)
#define U_THREE           ((char16_t)0x0033)
#define U_FOUR            ((char16_t)0x0034)
#define U_FIVE            ((char16_t)0x0035)
#define U_SIX             ((char16_t)0x0036)
#define U_SEVEN           ((char16_t)0x0037)
#define U_EIGHT           ((char16_t)0x0038)
#define U_NINE            ((char16_t)0x0039)
#define COLON             ((char16_t)0x003A)
#define SEMI_COLON        ((char16_t)0x003B)
#define CAP_A             ((char16_t)0x0041)
#define CAP_B             ((char16_t)0x0042)
#define CAP_R             ((char16_t)0x0052)
#define CAP_Z             ((char16_t)0x005A)
#define LOWLINE           ((char16_t)0x005F)
#define LEFTBRACE         ((char16_t)0x007B)
#define RIGHTBRACE        ((char16_t)0x007D)

#define LOW_A             ((char16_t)0x0061)
#define LOW_B             ((char16_t)0x0062)
#define LOW_C             ((char16_t)0x0063)
#define LOW_D             ((char16_t)0x0064)
#define LOW_E             ((char16_t)0x0065)
#define LOW_F             ((char16_t)0x0066)
#define LOW_G             ((char16_t)0x0067)
#define LOW_H             ((char16_t)0x0068)
#define LOW_I             ((char16_t)0x0069)
#define LOW_J             ((char16_t)0x006a)
#define LOW_K             ((char16_t)0x006B)
#define LOW_L             ((char16_t)0x006C)
#define LOW_M             ((char16_t)0x006D)
#define LOW_N             ((char16_t)0x006E)
#define LOW_O             ((char16_t)0x006F)
#define LOW_P             ((char16_t)0x0070)
#define LOW_Q             ((char16_t)0x0071)
#define LOW_R             ((char16_t)0x0072)
#define LOW_S             ((char16_t)0x0073)
#define LOW_T             ((char16_t)0x0074)
#define LOW_U             ((char16_t)0x0075)
#define LOW_V             ((char16_t)0x0076)
#define LOW_W             ((char16_t)0x0077)
#define LOW_X             ((char16_t)0x0078)
#define LOW_Y             ((char16_t)0x0079)
#define LOW_Z             ((char16_t)0x007A)

class UnicodeSet;

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */

#endif // SELFMTIMPL
//eof
