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


#define QUOTE             ((UChar)0x0027)
#define LOW_LINE          ((UChar)0x005F)
#define COLON             ((UChar)0x003A)
#define LEFT_CURLY_BRACKET  ((UChar)0x007B)
#define RIGHT_CURLY_BRACKET ((UChar)0x007D)
#define SPACE             ((UChar)0x0020)
#define EN_DASH           ((UChar)0x2013)
#define SOLIDUS           ((UChar)0x002F)

#define DIGIT_ZERO        ((UChar)0x0030)
#define DIGIT_ONE         ((UChar)0x0031)

#define LOW_A             ((UChar)0x0061)
#define LOW_B             ((UChar)0x0062)
#define LOW_C             ((UChar)0x0063)
#define LOW_D             ((UChar)0x0064)
#define LOW_E             ((UChar)0x0065)
#define LOW_F             ((UChar)0x0066)
#define LOW_G             ((UChar)0x0067)
#define LOW_H             ((UChar)0x0068)
#define LOW_I             ((UChar)0x0069)
#define LOW_J             ((UChar)0x006a)
#define LOW_K             ((UChar)0x006B)
#define LOW_L             ((UChar)0x006C)
#define LOW_M             ((UChar)0x006D)
#define LOW_N             ((UChar)0x006E)
#define LOW_O             ((UChar)0x006F)
#define LOW_P             ((UChar)0x0070)
#define LOW_Q             ((UChar)0x0071)
#define LOW_R             ((UChar)0x0072)
#define LOW_S             ((UChar)0x0073)
#define LOW_T             ((UChar)0x0074)
#define LOW_U             ((UChar)0x0075)
#define LOW_V             ((UChar)0x0076)
#define LOW_W             ((UChar)0x0077)
#define LOW_Y             ((UChar)0x0079)
#define LOW_Z             ((UChar)0x007A)

#define CAP_A             ((UChar)0x0041)
#define CAP_C             ((UChar)0x0043)
#define CAP_D             ((UChar)0x0044)
#define CAP_E             ((UChar)0x0045)
#define CAP_F             ((UChar)0x0046)
#define CAP_G             ((UChar)0x0047)
#define CAP_H             ((UChar)0x0048)
#define CAP_K             ((UChar)0x004B)
#define CAP_L             ((UChar)0x004C)
#define CAP_M             ((UChar)0x004D)
#define CAP_O             ((UChar)0x004F)
#define CAP_Q             ((UChar)0x0051)
#define CAP_S             ((UChar)0x0053)
#define CAP_T             ((UChar)0x0054)
#define CAP_U             ((UChar)0x0055)
#define CAP_V             ((UChar)0x0056)
#define CAP_W             ((UChar)0x0057)
#define CAP_Y             ((UChar)0x0059)
#define CAP_Z             ((UChar)0x005A)

//#define MINIMUM_SUPPORTED_CALENDAR_FIELD    UCAL_MINUTE

#define MAX_E_COUNT      5
#define MAX_M_COUNT      5
//#define MAX_INTERVAL_INDEX 4
#define MAX_POSITIVE_INT  56632;


#endif /* #if !UCONFIG_NO_FORMATTING */

#endif 
//eof
