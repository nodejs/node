// © 2017 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

// casemap.h
// created: 2017jan12 Markus W. Scherer

#ifndef __CASEMAP_H__
#define __CASEMAP_H__

#include "unicode/utypes.h"
#include "unicode/uobject.h"

/**
 * \file
 * \brief C++ API: Low-level C++ case mapping functions.
 */

U_NAMESPACE_BEGIN

#ifndef U_HIDE_DRAFT_API

class BreakIterator;
class Edits;

/**
 * Low-level C++ case mapping functions.
 *
 * @draft ICU 59
 */
class U_COMMON_API CaseMap U_FINAL : public UMemory {
public:
    /**
     * Lowercases a UTF-16 string and optionally records edits.
     * Casing is locale-dependent and context-sensitive.
     * The result may be longer or shorter than the original.
     * The source string and the destination buffer must not overlap.
     *
     * @param locale    The locale ID. ("" = root locale, NULL = default locale.)
     * @param options   Options bit set, usually 0. See UCASEMAP_OMIT_UNCHANGED_TEXT.
     * @param src       The original string.
     * @param srcLength The length of the original string. If -1, then src must be NUL-terminated.
     * @param dest      A buffer for the result string. The result will be NUL-terminated if
     *                  the buffer is large enough.
     *                  The contents is undefined in case of failure.
     * @param destCapacity The size of the buffer (number of char16_ts). If it is 0, then
     *                  dest may be NULL and the function will only return the length of the result
     *                  without writing any of the result string.
     * @param edits     Records edits for index mapping, working with styled text,
     *                  and getting only changes (if any).
     *                  The Edits contents is undefined if any error occurs.
     *                  This function calls edits->reset() first. edits can be NULL.
     * @param errorCode Reference to an in/out error code value
     *                  which must not indicate a failure before the function call.
     * @return The length of the result string, if successful.
     *         When the result would be longer than destCapacity,
     *         the full length is returned and a U_BUFFER_OVERFLOW_ERROR is set.
     *
     * @see u_strToLower
     * @draft ICU 59
     */
     static int32_t toLower(
            const char *locale, uint32_t options,
            const char16_t *src, int32_t srcLength,
            char16_t *dest, int32_t destCapacity, Edits *edits,
            UErrorCode &errorCode);

    /**
     * Uppercases a UTF-16 string and optionally records edits.
     * Casing is locale-dependent and context-sensitive.
     * The result may be longer or shorter than the original.
     * The source string and the destination buffer must not overlap.
     *
     * @param locale    The locale ID. ("" = root locale, NULL = default locale.)
     * @param options   Options bit set, usually 0. See UCASEMAP_OMIT_UNCHANGED_TEXT.
     * @param src       The original string.
     * @param srcLength The length of the original string. If -1, then src must be NUL-terminated.
     * @param dest      A buffer for the result string. The result will be NUL-terminated if
     *                  the buffer is large enough.
     *                  The contents is undefined in case of failure.
     * @param destCapacity The size of the buffer (number of char16_ts). If it is 0, then
     *                  dest may be NULL and the function will only return the length of the result
     *                  without writing any of the result string.
     * @param edits     Records edits for index mapping, working with styled text,
     *                  and getting only changes (if any).
     *                  The Edits contents is undefined if any error occurs.
     *                  This function calls edits->reset() first. edits can be NULL.
     * @param errorCode Reference to an in/out error code value
     *                  which must not indicate a failure before the function call.
     * @return The length of the result string, if successful.
     *         When the result would be longer than destCapacity,
     *         the full length is returned and a U_BUFFER_OVERFLOW_ERROR is set.
     *
     * @see u_strToUpper
     * @draft ICU 59
     */
    static int32_t toUpper(
            const char *locale, uint32_t options,
            const char16_t *src, int32_t srcLength,
            char16_t *dest, int32_t destCapacity, Edits *edits,
            UErrorCode &errorCode);

#if !UCONFIG_NO_BREAK_ITERATION

    /**
     * Titlecases a UTF-16 string and optionally records edits.
     * Casing is locale-dependent and context-sensitive.
     * The result may be longer or shorter than the original.
     * The source string and the destination buffer must not overlap.
     *
     * Titlecasing uses a break iterator to find the first characters of words
     * that are to be titlecased. It titlecases those characters and lowercases
     * all others. (This can be modified with options bits.)
     *
     * @param locale    The locale ID. ("" = root locale, NULL = default locale.)
     * @param options   Options bit set, usually 0. See UCASEMAP_OMIT_UNCHANGED_TEXT,
     *                  U_TITLECASE_NO_LOWERCASE, U_TITLECASE_NO_BREAK_ADJUSTMENT.
     * @param iter      A break iterator to find the first characters of words that are to be titlecased.
     *                  It is set to the source string (setText())
     *                  and used one or more times for iteration (first() and next()).
     *                  If NULL, then a word break iterator for the locale is used
     *                  (or something equivalent).
     * @param src       The original string.
     * @param srcLength The length of the original string. If -1, then src must be NUL-terminated.
     * @param dest      A buffer for the result string. The result will be NUL-terminated if
     *                  the buffer is large enough.
     *                  The contents is undefined in case of failure.
     * @param destCapacity The size of the buffer (number of char16_ts). If it is 0, then
     *                  dest may be NULL and the function will only return the length of the result
     *                  without writing any of the result string.
     * @param edits     Records edits for index mapping, working with styled text,
     *                  and getting only changes (if any).
     *                  The Edits contents is undefined if any error occurs.
     *                  This function calls edits->reset() first. edits can be NULL.
     * @param errorCode Reference to an in/out error code value
     *                  which must not indicate a failure before the function call.
     * @return The length of the result string, if successful.
     *         When the result would be longer than destCapacity,
     *         the full length is returned and a U_BUFFER_OVERFLOW_ERROR is set.
     *
     * @see u_strToTitle
     * @see ucasemap_toTitle
     * @draft ICU 59
     */
    static int32_t toTitle(
            const char *locale, uint32_t options, BreakIterator *iter,
            const char16_t *src, int32_t srcLength,
            char16_t *dest, int32_t destCapacity, Edits *edits,
            UErrorCode &errorCode);

#endif  // UCONFIG_NO_BREAK_ITERATION

    /**
     * Case-folds a UTF-16 string and optionally records edits.
     *
     * Case folding is locale-independent and not context-sensitive,
     * but there is an option for whether to include or exclude mappings for dotted I
     * and dotless i that are marked with 'T' in CaseFolding.txt.
     *
     * The result may be longer or shorter than the original.
     * The source string and the destination buffer must not overlap.
     *
     * @param options   Options bit set, usually 0. See UCASEMAP_OMIT_UNCHANGED_TEXT,
     *                  U_FOLD_CASE_DEFAULT, U_FOLD_CASE_EXCLUDE_SPECIAL_I.
     * @param src       The original string.
     * @param srcLength The length of the original string. If -1, then src must be NUL-terminated.
     * @param dest      A buffer for the result string. The result will be NUL-terminated if
     *                  the buffer is large enough.
     *                  The contents is undefined in case of failure.
     * @param destCapacity The size of the buffer (number of char16_ts). If it is 0, then
     *                  dest may be NULL and the function will only return the length of the result
     *                  without writing any of the result string.
     * @param edits     Records edits for index mapping, working with styled text,
     *                  and getting only changes (if any).
     *                  The Edits contents is undefined if any error occurs.
     *                  This function calls edits->reset() first. edits can be NULL.
     * @param errorCode Reference to an in/out error code value
     *                  which must not indicate a failure before the function call.
     * @return The length of the result string, if successful.
     *         When the result would be longer than destCapacity,
     *         the full length is returned and a U_BUFFER_OVERFLOW_ERROR is set.
     *
     * @see u_strFoldCase
     * @draft ICU 59
     */
    static int32_t fold(
            uint32_t options,
            const char16_t *src, int32_t srcLength,
            char16_t *dest, int32_t destCapacity, Edits *edits,
            UErrorCode &errorCode);

    /**
     * Lowercases a UTF-8 string and optionally records edits.
     * Casing is locale-dependent and context-sensitive.
     * The result may be longer or shorter than the original.
     * The source string and the destination buffer must not overlap.
     *
     * @param locale    The locale ID. ("" = root locale, NULL = default locale.)
     * @param options   Options bit set, usually 0. See UCASEMAP_OMIT_UNCHANGED_TEXT.
     * @param src       The original string.
     * @param srcLength The length of the original string. If -1, then src must be NUL-terminated.
     * @param dest      A buffer for the result string. The result will be NUL-terminated if
     *                  the buffer is large enough.
     *                  The contents is undefined in case of failure.
     * @param destCapacity The size of the buffer (number of bytes). If it is 0, then
     *                  dest may be NULL and the function will only return the length of the result
     *                  without writing any of the result string.
     * @param edits     Records edits for index mapping, working with styled text,
     *                  and getting only changes (if any).
     *                  The Edits contents is undefined if any error occurs.
     *                  This function calls edits->reset() first. edits can be NULL.
     * @param errorCode Reference to an in/out error code value
     *                  which must not indicate a failure before the function call.
     * @return The length of the result string, if successful.
     *         When the result would be longer than destCapacity,
     *         the full length is returned and a U_BUFFER_OVERFLOW_ERROR is set.
     *
     * @see ucasemap_utf8ToLower
     * @draft ICU 59
     */
     static int32_t utf8ToLower(
            const char *locale, uint32_t options,
            const char *src, int32_t srcLength,
            char *dest, int32_t destCapacity, Edits *edits,
            UErrorCode &errorCode);

    /**
     * Uppercases a UTF-8 string and optionally records edits.
     * Casing is locale-dependent and context-sensitive.
     * The result may be longer or shorter than the original.
     * The source string and the destination buffer must not overlap.
     *
     * @param locale    The locale ID. ("" = root locale, NULL = default locale.)
     * @param options   Options bit set, usually 0. See UCASEMAP_OMIT_UNCHANGED_TEXT.
     * @param src       The original string.
     * @param srcLength The length of the original string. If -1, then src must be NUL-terminated.
     * @param dest      A buffer for the result string. The result will be NUL-terminated if
     *                  the buffer is large enough.
     *                  The contents is undefined in case of failure.
     * @param destCapacity The size of the buffer (number of bytes). If it is 0, then
     *                  dest may be NULL and the function will only return the length of the result
     *                  without writing any of the result string.
     * @param edits     Records edits for index mapping, working with styled text,
     *                  and getting only changes (if any).
     *                  The Edits contents is undefined if any error occurs.
     *                  This function calls edits->reset() first. edits can be NULL.
     * @param errorCode Reference to an in/out error code value
     *                  which must not indicate a failure before the function call.
     * @return The length of the result string, if successful.
     *         When the result would be longer than destCapacity,
     *         the full length is returned and a U_BUFFER_OVERFLOW_ERROR is set.
     *
     * @see ucasemap_utf8ToUpper
     * @draft ICU 59
     */
    static int32_t utf8ToUpper(
            const char *locale, uint32_t options,
            const char *src, int32_t srcLength,
            char *dest, int32_t destCapacity, Edits *edits,
            UErrorCode &errorCode);

#if !UCONFIG_NO_BREAK_ITERATION

    /**
     * Titlecases a UTF-8 string and optionally records edits.
     * Casing is locale-dependent and context-sensitive.
     * The result may be longer or shorter than the original.
     * The source string and the destination buffer must not overlap.
     *
     * Titlecasing uses a break iterator to find the first characters of words
     * that are to be titlecased. It titlecases those characters and lowercases
     * all others. (This can be modified with options bits.)
     *
     * @param locale    The locale ID. ("" = root locale, NULL = default locale.)
     * @param options   Options bit set, usually 0. See UCASEMAP_OMIT_UNCHANGED_TEXT,
     *                  U_TITLECASE_NO_LOWERCASE, U_TITLECASE_NO_BREAK_ADJUSTMENT.
     * @param iter      A break iterator to find the first characters of words that are to be titlecased.
     *                  It is set to the source string (setText())
     *                  and used one or more times for iteration (first() and next()).
     *                  If NULL, then a word break iterator for the locale is used
     *                  (or something equivalent).
     * @param src       The original string.
     * @param srcLength The length of the original string. If -1, then src must be NUL-terminated.
     * @param dest      A buffer for the result string. The result will be NUL-terminated if
     *                  the buffer is large enough.
     *                  The contents is undefined in case of failure.
     * @param destCapacity The size of the buffer (number of bytes). If it is 0, then
     *                  dest may be NULL and the function will only return the length of the result
     *                  without writing any of the result string.
     * @param edits     Records edits for index mapping, working with styled text,
     *                  and getting only changes (if any).
     *                  The Edits contents is undefined if any error occurs.
     *                  This function calls edits->reset() first. edits can be NULL.
     * @param errorCode Reference to an in/out error code value
     *                  which must not indicate a failure before the function call.
     * @return The length of the result string, if successful.
     *         When the result would be longer than destCapacity,
     *         the full length is returned and a U_BUFFER_OVERFLOW_ERROR is set.
     *
     * @see ucasemap_utf8ToTitle
     * @draft ICU 59
     */
    static int32_t utf8ToTitle(
            const char *locale, uint32_t options, BreakIterator *iter,
            const char *src, int32_t srcLength,
            char *dest, int32_t destCapacity, Edits *edits,
            UErrorCode &errorCode);

#endif  // UCONFIG_NO_BREAK_ITERATION

    /**
     * Case-folds a UTF-8 string and optionally records edits.
     *
     * Case folding is locale-independent and not context-sensitive,
     * but there is an option for whether to include or exclude mappings for dotted I
     * and dotless i that are marked with 'T' in CaseFolding.txt.
     *
     * The result may be longer or shorter than the original.
     * The source string and the destination buffer must not overlap.
     *
     * @param options   Options bit set, usually 0. See UCASEMAP_OMIT_UNCHANGED_TEXT,
     *                  U_FOLD_CASE_DEFAULT, U_FOLD_CASE_EXCLUDE_SPECIAL_I.
     * @param src       The original string.
     * @param srcLength The length of the original string. If -1, then src must be NUL-terminated.
     * @param dest      A buffer for the result string. The result will be NUL-terminated if
     *                  the buffer is large enough.
     *                  The contents is undefined in case of failure.
     * @param destCapacity The size of the buffer (number of bytes). If it is 0, then
     *                  dest may be NULL and the function will only return the length of the result
     *                  without writing any of the result string.
     * @param edits     Records edits for index mapping, working with styled text,
     *                  and getting only changes (if any).
     *                  The Edits contents is undefined if any error occurs.
     *                  This function calls edits->reset() first. edits can be NULL.
     * @param errorCode Reference to an in/out error code value
     *                  which must not indicate a failure before the function call.
     * @return The length of the result string, if successful.
     *         When the result would be longer than destCapacity,
     *         the full length is returned and a U_BUFFER_OVERFLOW_ERROR is set.
     *
     * @see ucasemap_utf8FoldCase
     * @draft ICU 59
     */
    static int32_t utf8Fold(
            uint32_t options,
            const char *src, int32_t srcLength,
            char *dest, int32_t destCapacity, Edits *edits,
            UErrorCode &errorCode);

private:
    CaseMap() = delete;
    CaseMap(const CaseMap &other) = delete;
    CaseMap &operator=(const CaseMap &other) = delete;
};

#endif  // U_HIDE_DRAFT_API

U_NAMESPACE_END

#endif  // __CASEMAP_H__
