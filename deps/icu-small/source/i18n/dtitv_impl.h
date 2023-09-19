// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 2007-2016, International Business Machines Corporation and
* others. All Rights Reserved.
*******************************************************************************
*
* File DTITV_IMPL.H
*
*******************************************************************************
*/


#ifndef DTITV_IMPL_H__
#define DTITV_IMPL_H__

/**
 * \file
 * \brief C++ API: Defines macros for interval format implementation
 */
 
#if !UCONFIG_NO_FORMATTING

#include "unicode/unistr.h"


#define QUOTE             ((char16_t)0x0027)
#define LOW_LINE          ((char16_t)0x005F)
#define COLON             ((char16_t)0x003A)
#define LEFT_CURLY_BRACKET  ((char16_t)0x007B)
#define RIGHT_CURLY_BRACKET ((char16_t)0x007D)
#define SPACE             ((char16_t)0x0020)
#define EN_DASH           ((char16_t)0x2013)
#define SOLIDUS           ((char16_t)0x002F)

#define DIGIT_ZERO        ((char16_t)0x0030)
#define DIGIT_ONE         ((char16_t)0x0031)

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
#define LOW_Y             ((char16_t)0x0079)
#define LOW_Z             ((char16_t)0x007A)

#define CAP_A             ((char16_t)0x0041)
#define CAP_B             ((char16_t)0x0042)
#define CAP_C             ((char16_t)0x0043)
#define CAP_D             ((char16_t)0x0044)
#define CAP_E             ((char16_t)0x0045)
#define CAP_F             ((char16_t)0x0046)
#define CAP_G             ((char16_t)0x0047)
#define CAP_J             ((char16_t)0x004A)
#define CAP_H             ((char16_t)0x0048)
#define CAP_K             ((char16_t)0x004B)
#define CAP_L             ((char16_t)0x004C)
#define CAP_M             ((char16_t)0x004D)
#define CAP_O             ((char16_t)0x004F)
#define CAP_Q             ((char16_t)0x0051)
#define CAP_S             ((char16_t)0x0053)
#define CAP_T             ((char16_t)0x0054)
#define CAP_U             ((char16_t)0x0055)
#define CAP_V             ((char16_t)0x0056)
#define CAP_W             ((char16_t)0x0057)
#define CAP_Y             ((char16_t)0x0059)
#define CAP_Z             ((char16_t)0x005A)

//#define MINIMUM_SUPPORTED_CALENDAR_FIELD    UCAL_MINUTE

#define MAX_E_COUNT      5
#define MAX_M_COUNT      5
//#define MAX_INTERVAL_INDEX 4
#define MAX_POSITIVE_INT  56632


#endif /* #if !UCONFIG_NO_FORMATTING */

#endif 
//eof
