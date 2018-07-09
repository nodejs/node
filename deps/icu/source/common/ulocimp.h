// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
**********************************************************************
*   Copyright (C) 2004-2016, International Business Machines
*   Corporation and others.  All Rights Reserved.
**********************************************************************
*/

#ifndef ULOCIMP_H
#define ULOCIMP_H

#include "unicode/uloc.h"

/**
 * Create an iterator over the specified keywords list
 * @param keywordList double-null terminated list. Will be copied.
 * @param keywordListSize size in bytes of keywordList
 * @param status err code
 * @return enumeration (owned by caller) of the keyword list.
 * @internal ICU 3.0
 */
U_CAPI UEnumeration* U_EXPORT2
uloc_openKeywordList(const char *keywordList, int32_t keywordListSize, UErrorCode* status);

/**
 * Look up a resource bundle table item with fallback on the table level.
 * This is accessible so it can be called by C++ code.
 */
U_CAPI const UChar * U_EXPORT2
uloc_getTableStringWithFallback(
    const char *path,
    const char *locale,
    const char *tableKey,
    const char *subTableKey,
    const char *itemKey,
    int32_t *pLength,
    UErrorCode *pErrorCode);

/*returns TRUE if a is an ID separator FALSE otherwise*/
#define _isIDSeparator(a) (a == '_' || a == '-')

U_CFUNC const char* 
uloc_getCurrentCountryID(const char* oldID);

U_CFUNC const char* 
uloc_getCurrentLanguageID(const char* oldID);

U_CFUNC int32_t
ulocimp_getLanguage(const char *localeID,
                    char *language, int32_t languageCapacity,
                    const char **pEnd);

U_CFUNC int32_t
ulocimp_getScript(const char *localeID,
                   char *script, int32_t scriptCapacity,
                   const char **pEnd);

U_CFUNC int32_t
ulocimp_getCountry(const char *localeID,
                   char *country, int32_t countryCapacity,
                   const char **pEnd);

/**
 * Get the region to use for supplemental data lookup. Uses
 * (1) any region specified by locale tag "rg"; if none then
 * (2) any unicode_region_tag in the locale ID; if none then
 * (3) if inferRegion is TRUE, the region suggested by
 * getLikelySubtags on the localeID.
 * If no region is found, returns length 0.
 * 
 * @param localeID
 *     The complete locale ID (with keywords) from which
 *     to get the region to use for supplemental data.
 * @param inferRegion
 *     If TRUE, will try to infer region from localeID if
 *     no other region is found.
 * @param region
 *     Buffer in which to put the region ID found; should
 *     have a capacity at least ULOC_COUNTRY_CAPACITY. 
 * @param regionCapacity
 *     The actual capacity of the region buffer.
 * @param status
 *     Pointer to in/out UErrorCode value for latest status.
 * @return
 *     The length of any region code found, or 0 if none.
 * @internal ICU 57
 */
U_CAPI int32_t U_EXPORT2
ulocimp_getRegionForSupplementalData(const char *localeID, UBool inferRegion,
                                     char *region, int32_t regionCapacity, UErrorCode* status);

U_CAPI const char * U_EXPORT2
locale_getKeywordsStart(const char *localeID);


U_CFUNC UBool
ultag_isUnicodeLocaleKey(const char* s, int32_t len);

U_CFUNC UBool
ultag_isUnicodeLocaleType(const char* s, int32_t len);

U_CFUNC const char*
ulocimp_toBcpKey(const char* key);

U_CFUNC const char*
ulocimp_toLegacyKey(const char* key);

U_CFUNC const char*
ulocimp_toBcpType(const char* key, const char* type, UBool* isKnownKey, UBool* isSpecialType);

U_CFUNC const char*
ulocimp_toLegacyType(const char* key, const char* type, UBool* isKnownKey, UBool* isSpecialType);

#endif
