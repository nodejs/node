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

#include "unicode/bytestream.h"
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
 * Writes a well-formed language tag for this locale ID.
 *
 * **Note**: When `strict` is FALSE, any locale fields which do not satisfy the
 * BCP47 syntax requirement will be omitted from the result.  When `strict` is
 * TRUE, this function sets U_ILLEGAL_ARGUMENT_ERROR to the `err` if any locale
 * fields do not satisfy the BCP47 syntax requirement.
 *
 * @param localeID  the input locale ID
 * @param sink      the output sink receiving the BCP47 language
 *                  tag for this Locale.
 * @param strict    boolean value indicating if the function returns
 *                  an error for an ill-formed input locale ID.
 * @param err       error information if receiving the language
 *                  tag failed.
 * @return          The length of the BCP47 language tag.
 *
 * @internal ICU 64
 */
U_STABLE void U_EXPORT2
ulocimp_toLanguageTag(const char* localeID,
                      icu::ByteSink& sink,
                      UBool strict,
                      UErrorCode* err);

/**
 * Returns a locale ID for the specified BCP47 language tag string.
 * If the specified language tag contains any ill-formed subtags,
 * the first such subtag and all following subtags are ignored.
 * <p>
 * This implements the 'Language-Tag' production of BCP47, and so
 * supports grandfathered (regular and irregular) as well as private
 * use language tags.  Private use tags are represented as 'x-whatever',
 * and grandfathered tags are converted to their canonical replacements
 * where they exist.  Note that a few grandfathered tags have no modern
 * replacement, these will be converted using the fallback described in
 * the first paragraph, so some information might be lost.
 * @param langtag   the input BCP47 language tag.
 * @param tagLen    the length of langtag, or -1 to call uprv_strlen().
 * @param sink      the output sink receiving a locale ID for the
 *                  specified BCP47 language tag.
 * @param parsedLength  if not NULL, successfully parsed length
 *                      for the input language tag is set.
 * @param err       error information if receiving the locald ID
 *                  failed.
 * @internal ICU 63
 */
U_CAPI void U_EXPORT2
ulocimp_forLanguageTag(const char* langtag,
                       int32_t tagLen,
                       icu::ByteSink& sink,
                       int32_t* parsedLength,
                       UErrorCode* err);

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

/**
 * Add the likely subtags for a provided locale ID, per the algorithm described
 * in the following CLDR technical report:
 *
 *   http://www.unicode.org/reports/tr35/#Likely_Subtags
 *
 * If localeID is already in the maximal form, or there is no data available
 * for maximization, it will be copied to the output buffer.  For example,
 * "und-Zzzz" cannot be maximized, since there is no reasonable maximization.
 *
 * Examples:
 *
 * "en" maximizes to "en_Latn_US"
 *
 * "de" maximizes to "de_Latn_US"
 *
 * "sr" maximizes to "sr_Cyrl_RS"
 *
 * "sh" maximizes to "sr_Latn_RS" (Note this will not reverse.)
 *
 * "zh_Hani" maximizes to "zh_Hans_CN" (Note this will not reverse.)
 *
 * @param localeID The locale to maximize
 * @param sink The output sink receiving the maximized locale
 * @param err Error information if maximizing the locale failed.  If the length
 * of the localeID and the null-terminator is greater than the maximum allowed size,
 * or the localeId is not well-formed, the error code is U_ILLEGAL_ARGUMENT_ERROR.
 * @internal ICU 64
 */
U_STABLE void U_EXPORT2
ulocimp_addLikelySubtags(const char* localeID,
                         icu::ByteSink& sink,
                         UErrorCode* err);

/**
 * Minimize the subtags for a provided locale ID, per the algorithm described
 * in the following CLDR technical report:
 *
 *   http://www.unicode.org/reports/tr35/#Likely_Subtags
 *
 * If localeID is already in the minimal form, or there is no data available
 * for minimization, it will be copied to the output buffer.  Since the
 * minimization algorithm relies on proper maximization, see the comments
 * for ulocimp_addLikelySubtags for reasons why there might not be any data.
 *
 * Examples:
 *
 * "en_Latn_US" minimizes to "en"
 *
 * "de_Latn_US" minimizes to "de"
 *
 * "sr_Cyrl_RS" minimizes to "sr"
 *
 * "zh_Hant_TW" minimizes to "zh_TW" (The region is preferred to the
 * script, and minimizing to "zh" would imply "zh_Hans_CN".)
 *
 * @param localeID The locale to minimize
 * @param sink The output sink receiving the maximized locale
 * @param err Error information if minimizing the locale failed.  If the length
 * of the localeID and the null-terminator is greater than the maximum allowed size,
 * or the localeId is not well-formed, the error code is U_ILLEGAL_ARGUMENT_ERROR.
 * @internal ICU 64
 */
U_STABLE void U_EXPORT2
ulocimp_minimizeSubtags(const char* localeID,
                        icu::ByteSink& sink,
                        UErrorCode* err);

U_CAPI const char * U_EXPORT2
locale_getKeywordsStart(const char *localeID);

U_CFUNC UBool
ultag_isExtensionSubtags(const char* s, int32_t len);

U_CFUNC UBool
ultag_isLanguageSubtag(const char* s, int32_t len);

U_CFUNC UBool
ultag_isPrivateuseValueSubtags(const char* s, int32_t len);

U_CFUNC UBool
ultag_isRegionSubtag(const char* s, int32_t len);

U_CFUNC UBool
ultag_isScriptSubtag(const char* s, int32_t len);

U_CFUNC UBool
ultag_isTransformedExtensionSubtags(const char* s, int32_t len);

U_CFUNC UBool
ultag_isUnicodeExtensionSubtags(const char* s, int32_t len);

U_CFUNC UBool
ultag_isUnicodeLocaleAttribute(const char* s, int32_t len);

U_CFUNC UBool
ultag_isUnicodeLocaleAttributes(const char* s, int32_t len);

U_CFUNC UBool
ultag_isUnicodeLocaleKey(const char* s, int32_t len);

U_CFUNC UBool
ultag_isUnicodeLocaleType(const char* s, int32_t len);

U_CFUNC UBool
ultag_isVariantSubtags(const char* s, int32_t len);

U_CFUNC const char*
ulocimp_toBcpKey(const char* key);

U_CFUNC const char*
ulocimp_toLegacyKey(const char* key);

U_CFUNC const char*
ulocimp_toBcpType(const char* key, const char* type, UBool* isKnownKey, UBool* isSpecialType);

U_CFUNC const char*
ulocimp_toLegacyType(const char* key, const char* type, UBool* isKnownKey, UBool* isSpecialType);

#endif
