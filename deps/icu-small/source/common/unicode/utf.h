// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*
*   Copyright (C) 1999-2011, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  utf.h
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 1999sep09
*   created by: Markus W. Scherer
*/

/**
 * \file
 * \brief C API: Code point macros
 *
 * This file defines macros for checking whether a code point is
 * a surrogate or a non-character etc.
 *
 * The UChar and UChar32 data types for Unicode code units and code points
 * are defined in umachine.h because they can be machine-dependent.
 *
 * If U_NO_DEFAULT_INCLUDE_UTF_HEADERS is 0 then utf.h is included by utypes.h
 * and itself includes utf8.h and utf16.h after some
 * common definitions.
 * If U_NO_DEFAULT_INCLUDE_UTF_HEADERS is 1 then each of these headers must be
 * included explicitly if their definitions are used.
 *
 * utf8.h and utf16.h define macros for efficiently getting code points
 * in and out of UTF-8/16 strings.
 * utf16.h macros have "U16_" prefixes.
 * utf8.h defines similar macros with "U8_" prefixes for UTF-8 string handling.
 *
 * ICU mostly processes 16-bit Unicode strings.
 * Most of the time, such strings are well-formed UTF-16.
 * Single, unpaired surrogates must be handled as well, and are treated in ICU
 * like regular code points where possible.
 * (Pairs of surrogate code points are indistinguishable from supplementary
 * code points encoded as pairs of supplementary code units.)
 *
 * In fact, almost all Unicode code points in normal text (>99%)
 * are on the BMP (<=U+ffff) and even <=U+d7ff.
 * ICU functions handle supplementary code points (U+10000..U+10ffff)
 * but are optimized for the much more frequently occurring BMP code points.
 *
 * umachine.h defines UChar to be an unsigned 16-bit integer.
 * Where available, UChar is defined to be a char16_t
 * or a wchar_t (if that is an unsigned 16-bit type), otherwise uint16_t.
 *
 * UChar32 is defined to be a signed 32-bit integer (int32_t), large enough for a 21-bit
 * Unicode code point (Unicode scalar value, 0..0x10ffff).
 * Before ICU 2.4, the definition of UChar32 was similarly platform-dependent as
 * the definition of UChar. For details see the documentation for UChar32 itself.
 *
 * utf.h defines a small number of C macros for single Unicode code points.
 * These are simple checks for surrogates and non-characters.
 * For actual Unicode character properties see uchar.h.
 *
 * By default, string operations must be done with error checking in case
 * a string is not well-formed UTF-16.
 * The macros will detect if a surrogate code unit is unpaired
 * (lead unit without trail unit or vice versa) and just return the unit itself
 * as the code point.
 *
 * The regular "safe" macros require that the initial, passed-in string index
 * is within bounds. They only check the index when they read more than one
 * code unit. This is usually done with code similar to the following loop:
 * <pre>while(i<length) {
 *   U16_NEXT(s, i, length, c);
 *   // use c
 * }</pre>
 *
 * When it is safe to assume that text is well-formed UTF-16
 * (does not contain single, unpaired surrogates), then one can use
 * U16_..._UNSAFE macros.
 * These do not check for proper code unit sequences or truncated text and may
 * yield wrong results or even cause a crash if they are used with "malformed"
 * text.
 * In practice, U16_..._UNSAFE macros will produce slightly less code but
 * should not be faster because the processing is only different when a
 * surrogate code unit is detected, which will be rare.
 *
 * Similarly for UTF-8, there are "safe" macros without a suffix,
 * and U8_..._UNSAFE versions.
 * The performance differences are much larger here because UTF-8 provides so
 * many opportunities for malformed sequences.
 * The unsafe UTF-8 macros are entirely implemented inside the macro definitions
 * and are fast, while the safe UTF-8 macros call functions for all but the
 * trivial (ASCII) cases.
 * (ICU 3.6 optimizes U8_NEXT() and U8_APPEND() to handle most other common
 * characters inline as well.)
 *
 * Unlike with UTF-16, malformed sequences cannot be expressed with distinct
 * code point values (0..U+10ffff). They are indicated with negative values instead.
 *
 * For more information see the ICU User Guide Strings chapter
 * (http://userguide.icu-project.org/strings).
 *
 * <em>Usage:</em>
 * ICU coding guidelines for if() statements should be followed when using these macros.
 * Compound statements (curly braces {}) must be used  for if-else-while... 
 * bodies and all macro statements should be terminated with semicolon.
 *
 * @stable ICU 2.4
 */

#ifndef __UTF_H__
#define __UTF_H__

#include "unicode/umachine.h"
/* include the utfXX.h after the following definitions */

/* single-code point definitions -------------------------------------------- */

/**
 * Is this code point a Unicode noncharacter?
 * @param c 32-bit code point
 * @return TRUE or FALSE
 * @stable ICU 2.4
 */
#define U_IS_UNICODE_NONCHAR(c) \
    ((c)>=0xfdd0 && \
     ((uint32_t)(c)<=0xfdef || ((c)&0xfffe)==0xfffe) && \
     (uint32_t)(c)<=0x10ffff)

/**
 * Is c a Unicode code point value (0..U+10ffff)
 * that can be assigned a character?
 *
 * Code points that are not characters include:
 * - single surrogate code points (U+d800..U+dfff, 2048 code points)
 * - the last two code points on each plane (U+__fffe and U+__ffff, 34 code points)
 * - U+fdd0..U+fdef (new with Unicode 3.1, 32 code points)
 * - the highest Unicode code point value is U+10ffff
 *
 * This means that all code points below U+d800 are character code points,
 * and that boundary is tested first for performance.
 *
 * @param c 32-bit code point
 * @return TRUE or FALSE
 * @stable ICU 2.4
 */
#define U_IS_UNICODE_CHAR(c) \
    ((uint32_t)(c)<0xd800 || \
        ((uint32_t)(c)>0xdfff && \
         (uint32_t)(c)<=0x10ffff && \
         !U_IS_UNICODE_NONCHAR(c)))

/**
 * Is this code point a BMP code point (U+0000..U+ffff)?
 * @param c 32-bit code point
 * @return TRUE or FALSE
 * @stable ICU 2.8
 */
#define U_IS_BMP(c) ((uint32_t)(c)<=0xffff)

/**
 * Is this code point a supplementary code point (U+10000..U+10ffff)?
 * @param c 32-bit code point
 * @return TRUE or FALSE
 * @stable ICU 2.8
 */
#define U_IS_SUPPLEMENTARY(c) ((uint32_t)((c)-0x10000)<=0xfffff)
 
/**
 * Is this code point a lead surrogate (U+d800..U+dbff)?
 * @param c 32-bit code point
 * @return TRUE or FALSE
 * @stable ICU 2.4
 */
#define U_IS_LEAD(c) (((c)&0xfffffc00)==0xd800)

/**
 * Is this code point a trail surrogate (U+dc00..U+dfff)?
 * @param c 32-bit code point
 * @return TRUE or FALSE
 * @stable ICU 2.4
 */
#define U_IS_TRAIL(c) (((c)&0xfffffc00)==0xdc00)

/**
 * Is this code point a surrogate (U+d800..U+dfff)?
 * @param c 32-bit code point
 * @return TRUE or FALSE
 * @stable ICU 2.4
 */
#define U_IS_SURROGATE(c) (((c)&0xfffff800)==0xd800)

/**
 * Assuming c is a surrogate code point (U_IS_SURROGATE(c)),
 * is it a lead surrogate?
 * @param c 32-bit code point
 * @return TRUE or FALSE
 * @stable ICU 2.4
 */
#define U_IS_SURROGATE_LEAD(c) (((c)&0x400)==0)

/**
 * Assuming c is a surrogate code point (U_IS_SURROGATE(c)),
 * is it a trail surrogate?
 * @param c 32-bit code point
 * @return TRUE or FALSE
 * @stable ICU 4.2
 */
#define U_IS_SURROGATE_TRAIL(c) (((c)&0x400)!=0)

/* include the utfXX.h ------------------------------------------------------ */

#if !U_NO_DEFAULT_INCLUDE_UTF_HEADERS

#include "unicode/utf8.h"
#include "unicode/utf16.h"

/* utf_old.h contains deprecated, pre-ICU 2.4 definitions */
#include "unicode/utf_old.h"

#endif  /* !U_NO_DEFAULT_INCLUDE_UTF_HEADERS */

#endif  /* __UTF_H__ */
