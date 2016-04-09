/*
*****************************************************************************************
* Copyright (C) 2015-2016, International Business Machines
* Corporation and others. All Rights Reserved.
*****************************************************************************************
*/

#ifndef ULISTFORMATTER_H
#define ULISTFORMATTER_H

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/localpointer.h"

/**
 * \file
 * \brief C API: Format a list in a locale-appropriate way.
 *
 * A UListFormatter is used to format a list of items in a locale-appropriate way,
 * using data from CLDR.
 * Example: Input data ["Alice", "Bob", "Charlie", "Delta"] will be formatted
 * as "Alice, Bob, Charlie, and Delta" in English.
 */

/**
 * Opaque UListFormatter object for use in C
 * @stable ICU 55
 */
struct UListFormatter;
typedef struct UListFormatter UListFormatter;  /**< C typedef for struct UListFormatter. @stable ICU 55 */

/**
 * Open a new UListFormatter object using the rules for a given locale.
 * @param locale
 *            The locale whose rules should be used; may be NULL for
 *            default locale.
 * @param status
 *            A pointer to a standard ICU UErrorCode (input/output parameter).
 *            Its input value must pass the U_SUCCESS() test, or else the
 *            function returns immediately. The caller should check its output
 *            value with U_FAILURE(), or use with function chaining (see User
 *            Guide for details).
 * @return
 *            A pointer to a UListFormatter object for the specified locale,
 *            or NULL if an error occurred.
 * @stable ICU 55
 */
U_STABLE UListFormatter* U_EXPORT2
ulistfmt_open(const char*  locale,
              UErrorCode*  status);

/**
 * Close a UListFormatter object. Once closed it may no longer be used.
 * @param listfmt
 *            The UListFormatter object to close.
 * @stable ICU 55
 */
U_STABLE void U_EXPORT2
ulistfmt_close(UListFormatter *listfmt);


#if U_SHOW_CPLUSPLUS_API

U_NAMESPACE_BEGIN

/**
 * \class LocalUListFormatterPointer
 * "Smart pointer" class, closes a UListFormatter via ulistfmt_close().
 * For most methods see the LocalPointerBase base class.
 *
 * @see LocalPointerBase
 * @see LocalPointer
 * @stable ICU 55
 */
U_DEFINE_LOCAL_OPEN_POINTER(LocalUListFormatterPointer, UListFormatter, ulistfmt_close);

U_NAMESPACE_END

#endif

/**
 * Formats a list of strings using the conventions established for the
 * UListFormatter object.
 * @param listfmt
 *            The UListFormatter object specifying the list conventions.
 * @param strings
 *            An array of pointers to UChar strings; the array length is
 *            specified by stringCount. Must be non-NULL if stringCount > 0.
 * @param stringLengths
 *            An array of string lengths corresponding to the strings[]
 *            parameter; any individual length value may be negative to indicate
 *            that the corresponding strings[] entry is 0-terminated, or
 *            stringLengths itself may be NULL if all of the strings are
 *            0-terminated. If non-NULL, the stringLengths array must have
 *            stringCount entries.
 * @param stringCount
 *            the number of entries in strings[], and the number of entries
 *            in the stringLengths array if it is not NULL. Must be >= 0.
 * @param result
 *            A pointer to a buffer to receive the formatted list.
 * @param resultCapacity
 *            The maximum size of result.
 * @param status
 *            A pointer to a standard ICU UErrorCode (input/output parameter).
 *            Its input value must pass the U_SUCCESS() test, or else the
 *            function returns immediately. The caller should check its output
 *            value with U_FAILURE(), or use with function chaining (see User
 *            Guide for details).
 * @return
 *            The total buffer size needed; if greater than resultLength, the
 *            output was truncated. May be <=0 if unable to determine the
 *            total buffer size needed (e.g. for illegal arguments).
 * @stable ICU 55
 */
U_DRAFT int32_t U_EXPORT2
ulistfmt_format(const UListFormatter* listfmt,
                const UChar* const strings[],
                const int32_t *    stringLengths,
                int32_t            stringCount,
                UChar*             result,
                int32_t            resultCapacity,
                UErrorCode*        status);

#endif /* #if !UCONFIG_NO_FORMATTING */

#endif
