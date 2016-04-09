/*
*******************************************************************************
*
*   Copyright (C) 1999-2015, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  uinvchar.h
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:2
*
*   created on: 2004sep14
*   created by: Markus W. Scherer
*
*   Definitions for handling invariant characters, moved here from putil.c
*   for better modularization.
*/

#ifndef __UINVCHAR_H__
#define __UINVCHAR_H__

#include "unicode/utypes.h"
#ifdef __cplusplus
#include "unicode/unistr.h"
#endif

/**
 * Check if a char string only contains invariant characters.
 * See utypes.h for details.
 *
 * @param s Input string pointer.
 * @param length Length of the string, can be -1 if NUL-terminated.
 * @return TRUE if s contains only invariant characters.
 *
 * @internal (ICU 2.8)
 */
U_INTERNAL UBool U_EXPORT2
uprv_isInvariantString(const char *s, int32_t length);

/**
 * Check if a Unicode string only contains invariant characters.
 * See utypes.h for details.
 *
 * @param s Input string pointer.
 * @param length Length of the string, can be -1 if NUL-terminated.
 * @return TRUE if s contains only invariant characters.
 *
 * @internal (ICU 2.8)
 */
U_INTERNAL UBool U_EXPORT2
uprv_isInvariantUString(const UChar *s, int32_t length);

#ifdef __cplusplus

/**
 * Check if a UnicodeString only contains invariant characters.
 * See utypes.h for details.
 *
 * @param s Input string.
 * @return TRUE if s contains only invariant characters.
 */
U_INTERNAL inline UBool U_EXPORT2
uprv_isInvariantUnicodeString(const icu::UnicodeString &s) {
    return uprv_isInvariantUString(s.getBuffer(), s.length());
}

#endif  /* __cplusplus */

/**
 * \def U_UPPER_ORDINAL
 * Get the ordinal number of an uppercase invariant character
 * @internal
 */
#if U_CHARSET_FAMILY==U_ASCII_FAMILY
#   define U_UPPER_ORDINAL(x) ((x)-'A')
#elif U_CHARSET_FAMILY==U_EBCDIC_FAMILY
#   define U_UPPER_ORDINAL(x) (((x) < 'J') ? ((x)-'A') : \
                              (((x) < 'S') ? ((x)-'J'+9) : \
                               ((x)-'S'+18)))
#else
#   error Unknown charset family!
#endif

/**
 * Compare two EBCDIC invariant-character strings in ASCII order.
 * @internal
 */
U_INTERNAL int32_t U_EXPORT2
uprv_compareInvEbcdicAsAscii(const char *s1, const char *s2);

/**
 * \def uprv_compareInvCharsAsAscii
 * Compare two invariant-character strings in ASCII order.
 * @internal
 */
#if U_CHARSET_FAMILY==U_ASCII_FAMILY
#   define uprv_compareInvCharsAsAscii(s1, s2) uprv_strcmp(s1, s2)
#elif U_CHARSET_FAMILY==U_EBCDIC_FAMILY
#   define uprv_compareInvCharsAsAscii(s1, s2) uprv_compareInvEbcdicAsAscii(s1, s2)
#else
#   error Unknown charset family!
#endif

/**
 * Converts an EBCDIC invariant character to lowercase ASCII.
 * @internal
 */
U_INTERNAL char U_EXPORT2
uprv_ebcdicToLowercaseAscii(char c);

/**
 * \def uprv_invCharToLowercaseAscii
 * Converts an invariant character to lowercase ASCII.
 * @internal
 */
#if U_CHARSET_FAMILY==U_ASCII_FAMILY
#   define uprv_invCharToLowercaseAscii uprv_asciitolower
#elif U_CHARSET_FAMILY==U_EBCDIC_FAMILY
#   define uprv_invCharToLowercaseAscii uprv_ebcdicToLowercaseAscii
#else
#   error Unknown charset family!
#endif

/**
 * Copy EBCDIC to ASCII
 * @internal
 * @see uprv_strncpy
 */
U_INTERNAL uint8_t* U_EXPORT2
uprv_aestrncpy(uint8_t *dst, const uint8_t *src, int32_t n);


/**
 * Copy ASCII to EBCDIC
 * @internal
 * @see uprv_strncpy
 */
U_INTERNAL uint8_t* U_EXPORT2
uprv_eastrncpy(uint8_t *dst, const uint8_t *src, int32_t n);



#endif
