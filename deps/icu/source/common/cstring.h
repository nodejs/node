// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
******************************************************************************
*
*   Copyright (C) 1997-2012, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
******************************************************************************
*
* File CSTRING.H
*
* Contains CString interface
*
* @author       Helena Shih
*
* Modification History:
*
*   Date        Name        Description
*   6/17/98     hshih       Created.
*  05/03/99     stephen     Changed from functions to macros.
*  06/14/99     stephen     Added icu_strncat, icu_strncmp, icu_tolower
*
******************************************************************************
*/

#ifndef CSTRING_H
#define CSTRING_H 1

#include "unicode/utypes.h"
#include "cmemory.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define uprv_strcpy(dst, src) U_STANDARD_CPP_NAMESPACE  strcpy(dst, src)
#define uprv_strlen(str) U_STANDARD_CPP_NAMESPACE strlen(str)
#define uprv_strcmp(s1, s2) U_STANDARD_CPP_NAMESPACE strcmp(s1, s2)
#define uprv_strcat(dst, src) U_STANDARD_CPP_NAMESPACE strcat(dst, src)
#define uprv_strchr(s, c) U_STANDARD_CPP_NAMESPACE strchr(s, c)
#define uprv_strstr(s, c) U_STANDARD_CPP_NAMESPACE strstr(s, c)
#define uprv_strrchr(s, c) U_STANDARD_CPP_NAMESPACE strrchr(s, c)
#define uprv_strncpy(dst, src, size) U_STANDARD_CPP_NAMESPACE strncpy(dst, src, size)
#define uprv_strncmp(s1, s2, n) U_STANDARD_CPP_NAMESPACE strncmp(s1, s2, n)
#define uprv_strncat(dst, src, n) U_STANDARD_CPP_NAMESPACE strncat(dst, src, n)

/**
 * Is c an ASCII-repertoire letter a-z or A-Z?
 * Note: The implementation is specific to whether ICU is compiled for
 * an ASCII-based or EBCDIC-based machine. There just does not seem to be a better name for this.
 */
U_CAPI UBool U_EXPORT2
uprv_isASCIILetter(char c);

U_CAPI char U_EXPORT2
uprv_toupper(char c);


U_CAPI char U_EXPORT2
uprv_asciitolower(char c);

U_CAPI char U_EXPORT2
uprv_ebcdictolower(char c);

#if U_CHARSET_FAMILY==U_ASCII_FAMILY
#   define uprv_tolower uprv_asciitolower
#elif U_CHARSET_FAMILY==U_EBCDIC_FAMILY
#   define uprv_tolower uprv_ebcdictolower
#else
#   error U_CHARSET_FAMILY is not valid
#endif

#define uprv_strtod(source, end) U_STANDARD_CPP_NAMESPACE strtod(source, end)
#define uprv_strtoul(str, end, base) U_STANDARD_CPP_NAMESPACE strtoul(str, end, base)
#define uprv_strtol(str, end, base) U_STANDARD_CPP_NAMESPACE strtol(str, end, base)

/* Conversion from a digit to the character with radix base from 2-19 */
/* May need to use U_UPPER_ORDINAL*/
#define T_CString_itosOffset(a) ((a)<=9?('0'+(a)):('A'+(a)-10))

U_CAPI char* U_EXPORT2
uprv_strdup(const char *src);

/**
 * uprv_malloc n+1 bytes, and copy n bytes from src into the new string.
 * Terminate with a null at offset n.   If n is -1, works like uprv_strdup
 * @param src
 * @param n length of the input string, not including null.
 * @return new string (owned by caller, use uprv_free to free).
 * @internal
 */
U_CAPI char* U_EXPORT2
uprv_strndup(const char *src, int32_t n);

U_CAPI char* U_EXPORT2
T_CString_toLowerCase(char* str);

U_CAPI char* U_EXPORT2
T_CString_toUpperCase(char* str);

U_CAPI int32_t U_EXPORT2
T_CString_integerToString(char *buffer, int32_t n, int32_t radix);

U_CAPI int32_t U_EXPORT2
T_CString_int64ToString(char *buffer, int64_t n, uint32_t radix);

U_CAPI int32_t U_EXPORT2
T_CString_stringToInteger(const char *integerString, int32_t radix);

/**
 * Case-insensitive, language-independent string comparison
 * limited to the ASCII character repertoire.
 */
U_CAPI int U_EXPORT2
uprv_stricmp(const char *str1, const char *str2);

/**
 * Case-insensitive, language-independent string comparison
 * limited to the ASCII character repertoire.
 */
U_CAPI int U_EXPORT2
uprv_strnicmp(const char *str1, const char *str2, uint32_t n);

#endif /* ! CSTRING_H */
