// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
**********************************************************************
*   Copyright (C) 1999-2015, International Business Machines
*   Corporation and others.  All Rights Reserved.
**********************************************************************
*   file name:  ustr_imp.h
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2001jan30
*   created by: Markus W. Scherer
*/

#ifndef __USTR_IMP_H__
#define __USTR_IMP_H__

#include "unicode/utypes.h"
#include "unicode/uiter.h"
#include "ucase.h"

/** Simple declaration to avoid including unicode/ubrk.h. */
#ifndef UBRK_TYPEDEF_UBREAK_ITERATOR
#   define UBRK_TYPEDEF_UBREAK_ITERATOR
    typedef struct UBreakIterator UBreakIterator;
#endif

#ifndef U_COMPARE_IGNORE_CASE
/* see also unorm.h */
/**
 * Option bit for unorm_compare:
 * Perform case-insensitive comparison.
 */
#define U_COMPARE_IGNORE_CASE       0x10000
#endif

/**
 * Internal option for unorm_cmpEquivFold() for strncmp style.
 * If set, checks for both string length and terminating NUL.
 */
#define _STRNCMP_STYLE 0x1000

/**
 * Compare two strings in code point order or code unit order.
 * Works in strcmp style (both lengths -1),
 * strncmp style (lengths equal and >=0, flag TRUE),
 * and memcmp/UnicodeString style (at least one length >=0).
 */
U_CFUNC int32_t U_EXPORT2
uprv_strCompare(const UChar *s1, int32_t length1,
                const UChar *s2, int32_t length2,
                UBool strncmpStyle, UBool codePointOrder);

/**
 * Internal API, used by u_strcasecmp() etc.
 * Compare strings case-insensitively,
 * in code point order or code unit order.
 */
U_CFUNC int32_t
u_strcmpFold(const UChar *s1, int32_t length1,
             const UChar *s2, int32_t length2,
             uint32_t options,
             UErrorCode *pErrorCode);

/**
 * Interanl API, used for detecting length of
 * shared prefix case-insensitively.
 * @param s1            input string 1
 * @param length1       length of string 1, or -1 (NULL terminated)
 * @param s2            input string 2
 * @param length2       length of string 2, or -1 (NULL terminated)
 * @param options       compare options
 * @param matchLen1     (output) length of partial prefix match in s1
 * @param matchLen2     (output) length of partial prefix match in s2
 * @param pErrorCode    receives error status
 */
U_CAPI void
u_caseInsensitivePrefixMatch(const UChar *s1, int32_t length1,
                             const UChar *s2, int32_t length2,
                             uint32_t options,
                             int32_t *matchLen1, int32_t *matchLen2,
                             UErrorCode *pErrorCode);

/**
 * Are the Unicode properties loaded?
 * This must be used before internal functions are called that do
 * not perform this check.
 * Generate a debug assertion failure if data is not loaded.
 */
U_CFUNC UBool
uprv_haveProperties(UErrorCode *pErrorCode);

/**
  * Load the Unicode property data.
  * Intended primarily for use from u_init().
  * Has no effect if property data is already loaded.
  * NOT thread safe.
  */
/*U_CFUNC int8_t
uprv_loadPropsData(UErrorCode *errorCode);*/

/*
 * Internal string casing functions implementing
 * ustring.h/ustrcase.c and UnicodeString case mapping functions.
 */

struct UCaseMap {
    const UCaseProps *csp;
#if !UCONFIG_NO_BREAK_ITERATION
    UBreakIterator *iter;  /* We adopt the iterator, so we own it. */
#endif
    char locale[32];
    int32_t locCache;
    uint32_t options;
};

#ifndef __UCASEMAP_H__
typedef struct UCaseMap UCaseMap;
#endif

#if UCONFIG_NO_BREAK_ITERATION
#   define UCASEMAP_INITIALIZER { NULL, { 0 }, 0, 0 }
#else
#   define UCASEMAP_INITIALIZER { NULL, NULL, { 0 }, 0, 0 }
#endif

U_CFUNC void
ustrcase_setTempCaseMapLocale(UCaseMap *csm, const char *locale);

#ifndef U_STRING_CASE_MAPPER_DEFINED
#define U_STRING_CASE_MAPPER_DEFINED

/**
 * String case mapping function type, used by ustrcase_map().
 * All error checking must be done.
 * The UCaseMap must be fully initialized, with locale and/or iter set as needed.
 * src and dest must not overlap.
 */
typedef int32_t U_CALLCONV
UStringCaseMapper(const UCaseMap *csm,
                  UChar *dest, int32_t destCapacity,
                  const UChar *src, int32_t srcLength,
                  UErrorCode *pErrorCode);

#endif

/** Implements UStringCaseMapper. */
U_CFUNC int32_t U_CALLCONV
ustrcase_internalToLower(const UCaseMap *csm,
                         UChar *dest, int32_t destCapacity,
                         const UChar *src, int32_t srcLength,
                         UErrorCode *pErrorCode);

/** Implements UStringCaseMapper. */
U_CFUNC int32_t U_CALLCONV
ustrcase_internalToUpper(const UCaseMap *csm,
                         UChar *dest, int32_t destCapacity,
                         const UChar *src, int32_t srcLength,
                         UErrorCode *pErrorCode);

#if !UCONFIG_NO_BREAK_ITERATION

/** Implements UStringCaseMapper. */
U_CFUNC int32_t U_CALLCONV
ustrcase_internalToTitle(const UCaseMap *csm,
                         UChar *dest, int32_t destCapacity,
                         const UChar *src, int32_t srcLength,
                         UErrorCode *pErrorCode);

#endif

/** Implements UStringCaseMapper. */
U_CFUNC int32_t U_CALLCONV
ustrcase_internalFold(const UCaseMap *csm,
                      UChar *dest, int32_t destCapacity,
                      const UChar *src, int32_t srcLength,
                      UErrorCode *pErrorCode);

/**
 * Implements argument checking and buffer handling
 * for string case mapping as a common function.
 */
U_CFUNC int32_t
ustrcase_map(const UCaseMap *csm,
             UChar *dest, int32_t destCapacity,
             const UChar *src, int32_t srcLength,
             UStringCaseMapper *stringCaseMapper,
             UErrorCode *pErrorCode);

/**
 * UTF-8 string case mapping function type, used by ucasemap_mapUTF8().
 * UTF-8 version of UStringCaseMapper.
 * All error checking must be done.
 * The UCaseMap must be fully initialized, with locale and/or iter set as needed.
 * src and dest must not overlap.
 */
typedef int32_t U_CALLCONV
UTF8CaseMapper(const UCaseMap *csm,
               uint8_t *dest, int32_t destCapacity,
               const uint8_t *src, int32_t srcLength,
               UErrorCode *pErrorCode);

/** Implements UTF8CaseMapper. */
U_CFUNC int32_t U_CALLCONV
ucasemap_internalUTF8ToTitle(const UCaseMap *csm,
         uint8_t *dest, int32_t destCapacity,
         const uint8_t *src, int32_t srcLength,
         UErrorCode *pErrorCode);

/**
 * Implements argument checking and buffer handling
 * for UTF-8 string case mapping as a common function.
 */
U_CFUNC int32_t
ucasemap_mapUTF8(const UCaseMap *csm,
                 uint8_t *dest, int32_t destCapacity,
                 const uint8_t *src, int32_t srcLength,
                 UTF8CaseMapper *stringCaseMapper,
                 UErrorCode *pErrorCode);

#ifdef __cplusplus

U_NAMESPACE_BEGIN
namespace GreekUpper {

// Data bits.
static const uint32_t UPPER_MASK = 0x3ff;
static const uint32_t HAS_VOWEL = 0x1000;
static const uint32_t HAS_YPOGEGRAMMENI = 0x2000;
static const uint32_t HAS_ACCENT = 0x4000;
static const uint32_t HAS_DIALYTIKA = 0x8000;
// Further bits during data building and processing, not stored in the data map.
static const uint32_t HAS_COMBINING_DIALYTIKA = 0x10000;
static const uint32_t HAS_OTHER_GREEK_DIACRITIC = 0x20000;

static const uint32_t HAS_VOWEL_AND_ACCENT = HAS_VOWEL | HAS_ACCENT;
static const uint32_t HAS_VOWEL_AND_ACCENT_AND_DIALYTIKA =
        HAS_VOWEL_AND_ACCENT | HAS_DIALYTIKA;
static const uint32_t HAS_EITHER_DIALYTIKA = HAS_DIALYTIKA | HAS_COMBINING_DIALYTIKA;

// State bits.
static const uint32_t AFTER_CASED = 1;
static const uint32_t AFTER_VOWEL_WITH_ACCENT = 2;

uint32_t getLetterData(UChar32 c);

/**
 * Returns a non-zero value for each of the Greek combining diacritics
 * listed in The Unicode Standard, version 8, chapter 7.2 Greek,
 * plus some perispomeni look-alikes.
 */
uint32_t getDiacriticData(UChar32 c);

}  // namespace GreekUpper
U_NAMESPACE_END

#endif  // __cplusplus

U_CAPI int32_t U_EXPORT2
ustr_hashUCharsN(const UChar *str, int32_t length);

U_CAPI int32_t U_EXPORT2
ustr_hashCharsN(const char *str, int32_t length);

U_CAPI int32_t U_EXPORT2
ustr_hashICharsN(const char *str, int32_t length);

/**
 * NUL-terminate a UChar * string if possible.
 * If length  < destCapacity then NUL-terminate.
 * If length == destCapacity then do not terminate but set U_STRING_NOT_TERMINATED_WARNING.
 * If length  > destCapacity then do not terminate but set U_BUFFER_OVERFLOW_ERROR.
 *
 * @param dest Destination buffer, can be NULL if destCapacity==0.
 * @param destCapacity Number of UChars available at dest.
 * @param length Number of UChars that were (to be) written to dest.
 * @param pErrorCode ICU error code.
 * @return length
 */
U_CAPI int32_t U_EXPORT2
u_terminateUChars(UChar *dest, int32_t destCapacity, int32_t length, UErrorCode *pErrorCode);

/**
 * NUL-terminate a char * string if possible.
 * Same as u_terminateUChars() but for a different string type.
 */
U_CAPI int32_t U_EXPORT2
u_terminateChars(char *dest, int32_t destCapacity, int32_t length, UErrorCode *pErrorCode);

/**
 * NUL-terminate a UChar32 * string if possible.
 * Same as u_terminateUChars() but for a different string type.
 */
U_CAPI int32_t U_EXPORT2
u_terminateUChar32s(UChar32 *dest, int32_t destCapacity, int32_t length, UErrorCode *pErrorCode);

/**
 * NUL-terminate a wchar_t * string if possible.
 * Same as u_terminateUChars() but for a different string type.
 */
U_CAPI int32_t U_EXPORT2
u_terminateWChars(wchar_t *dest, int32_t destCapacity, int32_t length, UErrorCode *pErrorCode);

#endif
