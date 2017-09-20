// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*
*   Copyright (C) 2007-2015, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  udatpg.h
*   encoding:   UTF-8
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2007jul30
*   created by: Markus W. Scherer
*/

#ifndef __UDATPG_H__
#define __UDATPG_H__

#include "unicode/utypes.h"
#include "unicode/uenum.h"
#include "unicode/localpointer.h"

/**
 * \file
 * \brief C API: Wrapper for icu::DateTimePatternGenerator (unicode/dtptngen.h).
 *
 * UDateTimePatternGenerator provides flexible generation of date format patterns,
 * like "yy-MM-dd". The user can build up the generator by adding successive
 * patterns. Once that is done, a query can be made using a "skeleton", which is
 * a pattern which just includes the desired fields and lengths. The generator
 * will return the "best fit" pattern corresponding to that skeleton.
 * <p>The main method people will use is udatpg_getBestPattern, since normally
 * UDateTimePatternGenerator is pre-built with data from a particular locale.
 * However, generators can be built directly from other data as well.
 * <p><i>Issue: may be useful to also have a function that returns the list of
 * fields in a pattern, in order, since we have that internally.
 * That would be useful for getting the UI order of field elements.</i>
 */

/**
 * Opaque type for a date/time pattern generator object.
 * @stable ICU 3.8
 */
typedef void *UDateTimePatternGenerator;

/**
 * Field number constants for udatpg_getAppendItemFormats() and similar functions.
 * These constants are separate from UDateFormatField despite semantic overlap
 * because some fields are merged for the date/time pattern generator.
 * @stable ICU 3.8
 */
typedef enum UDateTimePatternField {
    /** @stable ICU 3.8 */
    UDATPG_ERA_FIELD,
    /** @stable ICU 3.8 */
    UDATPG_YEAR_FIELD,
    /** @stable ICU 3.8 */
    UDATPG_QUARTER_FIELD,
    /** @stable ICU 3.8 */
    UDATPG_MONTH_FIELD,
    /** @stable ICU 3.8 */
    UDATPG_WEEK_OF_YEAR_FIELD,
    /** @stable ICU 3.8 */
    UDATPG_WEEK_OF_MONTH_FIELD,
    /** @stable ICU 3.8 */
    UDATPG_WEEKDAY_FIELD,
    /** @stable ICU 3.8 */
    UDATPG_DAY_OF_YEAR_FIELD,
    /** @stable ICU 3.8 */
    UDATPG_DAY_OF_WEEK_IN_MONTH_FIELD,
    /** @stable ICU 3.8 */
    UDATPG_DAY_FIELD,
    /** @stable ICU 3.8 */
    UDATPG_DAYPERIOD_FIELD,
    /** @stable ICU 3.8 */
    UDATPG_HOUR_FIELD,
    /** @stable ICU 3.8 */
    UDATPG_MINUTE_FIELD,
    /** @stable ICU 3.8 */
    UDATPG_SECOND_FIELD,
    /** @stable ICU 3.8 */
    UDATPG_FRACTIONAL_SECOND_FIELD,
    /** @stable ICU 3.8 */
    UDATPG_ZONE_FIELD,

    /* Do not conditionalize the following with #ifndef U_HIDE_DEPRECATED_API,
     * it is needed for layout of DateTimePatternGenerator object. */
    /**
     * One more than the highest normal UDateTimePatternField value.
     * @deprecated ICU 58 The numeric value may change over time, see ICU ticket #12420.
     */
    UDATPG_FIELD_COUNT
} UDateTimePatternField;

/**
 * Masks to control forcing the length of specified fields in the returned
 * pattern to match those in the skeleton (when this would not happen
 * otherwise). These may be combined to force the length of multiple fields.
 * Used with udatpg_getBestPatternWithOptions, udatpg_replaceFieldTypesWithOptions.
 * @stable ICU 4.4
 */
typedef enum UDateTimePatternMatchOptions {
    /** @stable ICU 4.4 */
    UDATPG_MATCH_NO_OPTIONS = 0,
    /** @stable ICU 4.4 */
    UDATPG_MATCH_HOUR_FIELD_LENGTH = 1 << UDATPG_HOUR_FIELD,
#ifndef U_HIDE_INTERNAL_API
    /** @internal ICU 4.4 */
    UDATPG_MATCH_MINUTE_FIELD_LENGTH = 1 << UDATPG_MINUTE_FIELD,
    /** @internal ICU 4.4 */
    UDATPG_MATCH_SECOND_FIELD_LENGTH = 1 << UDATPG_SECOND_FIELD,
#endif  /* U_HIDE_INTERNAL_API */
    /** @stable ICU 4.4 */
    UDATPG_MATCH_ALL_FIELDS_LENGTH = (1 << UDATPG_FIELD_COUNT) - 1
} UDateTimePatternMatchOptions;

/**
 * Status return values from udatpg_addPattern().
 * @stable ICU 3.8
 */
typedef enum UDateTimePatternConflict {
    /** @stable ICU 3.8 */
    UDATPG_NO_CONFLICT,
    /** @stable ICU 3.8 */
    UDATPG_BASE_CONFLICT,
    /** @stable ICU 3.8 */
    UDATPG_CONFLICT,
#ifndef U_HIDE_DEPRECATED_API
    /**
     * One more than the highest normal UDateTimePatternConflict value.
     * @deprecated ICU 58 The numeric value may change over time, see ICU ticket #12420.
     */
    UDATPG_CONFLICT_COUNT
#endif  // U_HIDE_DEPRECATED_API
} UDateTimePatternConflict;

/**
  * Open a generator according to a given locale.
  * @param locale
  * @param pErrorCode a pointer to the UErrorCode which must not indicate a
  *                   failure before the function call.
  * @return a pointer to UDateTimePatternGenerator.
  * @stable ICU 3.8
  */
U_STABLE UDateTimePatternGenerator * U_EXPORT2
udatpg_open(const char *locale, UErrorCode *pErrorCode);

/**
  * Open an empty generator, to be constructed with udatpg_addPattern(...) etc.
  * @param pErrorCode a pointer to the UErrorCode which must not indicate a
  *                   failure before the function call.
  * @return a pointer to UDateTimePatternGenerator.
  * @stable ICU 3.8
  */
U_STABLE UDateTimePatternGenerator * U_EXPORT2
udatpg_openEmpty(UErrorCode *pErrorCode);

/**
  * Close a generator.
  * @param dtpg a pointer to UDateTimePatternGenerator.
  * @stable ICU 3.8
  */
U_STABLE void U_EXPORT2
udatpg_close(UDateTimePatternGenerator *dtpg);

#if U_SHOW_CPLUSPLUS_API

U_NAMESPACE_BEGIN

/**
 * \class LocalUDateTimePatternGeneratorPointer
 * "Smart pointer" class, closes a UDateTimePatternGenerator via udatpg_close().
 * For most methods see the LocalPointerBase base class.
 *
 * @see LocalPointerBase
 * @see LocalPointer
 * @stable ICU 4.4
 */
U_DEFINE_LOCAL_OPEN_POINTER(LocalUDateTimePatternGeneratorPointer, UDateTimePatternGenerator, udatpg_close);

U_NAMESPACE_END

#endif

/**
  * Create a copy pf a generator.
  * @param dtpg a pointer to UDateTimePatternGenerator to be copied.
  * @param pErrorCode a pointer to the UErrorCode which must not indicate a
  *                   failure before the function call.
  * @return a pointer to a new UDateTimePatternGenerator.
  * @stable ICU 3.8
 */
U_STABLE UDateTimePatternGenerator * U_EXPORT2
udatpg_clone(const UDateTimePatternGenerator *dtpg, UErrorCode *pErrorCode);

/**
 * Get the best pattern matching the input skeleton. It is guaranteed to
 * have all of the fields in the skeleton.
 *
 * Note that this function uses a non-const UDateTimePatternGenerator:
 * It uses a stateful pattern parser which is set up for each generator object,
 * rather than creating one for each function call.
 * Consecutive calls to this function do not affect each other,
 * but this function cannot be used concurrently on a single generator object.
 *
 * @param dtpg a pointer to UDateTimePatternGenerator.
 * @param skeleton
 *            The skeleton is a pattern containing only the variable fields.
 *            For example, "MMMdd" and "mmhh" are skeletons.
 * @param length the length of skeleton
 * @param bestPattern
 *            The best pattern found from the given skeleton.
 * @param capacity the capacity of bestPattern.
 * @param pErrorCode a pointer to the UErrorCode which must not indicate a
 *                   failure before the function call.
 * @return the length of bestPattern.
 * @stable ICU 3.8
 */
U_STABLE int32_t U_EXPORT2
udatpg_getBestPattern(UDateTimePatternGenerator *dtpg,
                      const UChar *skeleton, int32_t length,
                      UChar *bestPattern, int32_t capacity,
                      UErrorCode *pErrorCode);

/**
 * Get the best pattern matching the input skeleton. It is guaranteed to
 * have all of the fields in the skeleton.
 *
 * Note that this function uses a non-const UDateTimePatternGenerator:
 * It uses a stateful pattern parser which is set up for each generator object,
 * rather than creating one for each function call.
 * Consecutive calls to this function do not affect each other,
 * but this function cannot be used concurrently on a single generator object.
 *
 * @param dtpg a pointer to UDateTimePatternGenerator.
 * @param skeleton
 *            The skeleton is a pattern containing only the variable fields.
 *            For example, "MMMdd" and "mmhh" are skeletons.
 * @param length the length of skeleton
 * @param options
 *            Options for forcing the length of specified fields in the
 *            returned pattern to match those in the skeleton (when this
 *            would not happen otherwise). For default behavior, use
 *            UDATPG_MATCH_NO_OPTIONS.
 * @param bestPattern
 *            The best pattern found from the given skeleton.
 * @param capacity
 *            the capacity of bestPattern.
 * @param pErrorCode
 *            a pointer to the UErrorCode which must not indicate a
 *            failure before the function call.
 * @return the length of bestPattern.
 * @stable ICU 4.4
 */
U_STABLE int32_t U_EXPORT2
udatpg_getBestPatternWithOptions(UDateTimePatternGenerator *dtpg,
                                 const UChar *skeleton, int32_t length,
                                 UDateTimePatternMatchOptions options,
                                 UChar *bestPattern, int32_t capacity,
                                 UErrorCode *pErrorCode);

/**
  * Get a unique skeleton from a given pattern. For example,
  * both "MMM-dd" and "dd/MMM" produce the skeleton "MMMdd".
  *
  * Note that this function uses a non-const UDateTimePatternGenerator:
  * It uses a stateful pattern parser which is set up for each generator object,
  * rather than creating one for each function call.
  * Consecutive calls to this function do not affect each other,
  * but this function cannot be used concurrently on a single generator object.
  *
  * @param unusedDtpg     a pointer to UDateTimePatternGenerator.
  *    This parameter is no longer used. Callers may pass NULL.
  * @param pattern  input pattern, such as "dd/MMM".
  * @param length   the length of pattern.
  * @param skeleton such as "MMMdd"
  * @param capacity the capacity of skeleton.
  * @param pErrorCode a pointer to the UErrorCode which must not indicate a
  *                  failure before the function call.
  * @return the length of skeleton.
  * @stable ICU 3.8
  */
U_STABLE int32_t U_EXPORT2
udatpg_getSkeleton(UDateTimePatternGenerator *unusedDtpg,
                   const UChar *pattern, int32_t length,
                   UChar *skeleton, int32_t capacity,
                   UErrorCode *pErrorCode);

/**
 * Get a unique base skeleton from a given pattern. This is the same
 * as the skeleton, except that differences in length are minimized so
 * as to only preserve the difference between string and numeric form. So
 * for example, both "MMM-dd" and "d/MMM" produce the skeleton "MMMd"
 * (notice the single d).
 *
 * Note that this function uses a non-const UDateTimePatternGenerator:
 * It uses a stateful pattern parser which is set up for each generator object,
 * rather than creating one for each function call.
 * Consecutive calls to this function do not affect each other,
 * but this function cannot be used concurrently on a single generator object.
 *
 * @param unusedDtpg     a pointer to UDateTimePatternGenerator.
 *    This parameter is no longer used. Callers may pass NULL.
 * @param pattern  input pattern, such as "dd/MMM".
 * @param length   the length of pattern.
 * @param baseSkeleton such as "Md"
 * @param capacity the capacity of base skeleton.
 * @param pErrorCode a pointer to the UErrorCode which must not indicate a
 *                  failure before the function call.
 * @return the length of baseSkeleton.
 * @stable ICU 3.8
 */
U_STABLE int32_t U_EXPORT2
udatpg_getBaseSkeleton(UDateTimePatternGenerator *unusedDtpg,
                       const UChar *pattern, int32_t length,
                       UChar *baseSkeleton, int32_t capacity,
                       UErrorCode *pErrorCode);

/**
 * Adds a pattern to the generator. If the pattern has the same skeleton as
 * an existing pattern, and the override parameter is set, then the previous
 * value is overriden. Otherwise, the previous value is retained. In either
 * case, the conflicting status is set and previous vale is stored in
 * conflicting pattern.
 * <p>
 * Note that single-field patterns (like "MMM") are automatically added, and
 * don't need to be added explicitly!
 *
 * @param dtpg     a pointer to UDateTimePatternGenerator.
 * @param pattern  input pattern, such as "dd/MMM"
 * @param patternLength the length of pattern.
 * @param override  When existing values are to be overridden use true,
 *                  otherwise use false.
 * @param conflictingPattern  Previous pattern with the same skeleton.
 * @param capacity the capacity of conflictingPattern.
 * @param pLength a pointer to the length of conflictingPattern.
 * @param pErrorCode a pointer to the UErrorCode which must not indicate a
 *                  failure before the function call.
 * @return conflicting status. The value could be UDATPG_NO_CONFLICT,
 *                  UDATPG_BASE_CONFLICT or UDATPG_CONFLICT.
 * @stable ICU 3.8
 */
U_STABLE UDateTimePatternConflict U_EXPORT2
udatpg_addPattern(UDateTimePatternGenerator *dtpg,
                  const UChar *pattern, int32_t patternLength,
                  UBool override,
                  UChar *conflictingPattern, int32_t capacity, int32_t *pLength,
                  UErrorCode *pErrorCode);

/**
  * An AppendItem format is a pattern used to append a field if there is no
  * good match. For example, suppose that the input skeleton is "GyyyyMMMd",
  * and there is no matching pattern internally, but there is a pattern
  * matching "yyyyMMMd", say "d-MM-yyyy". Then that pattern is used, plus the
  * G. The way these two are conjoined is by using the AppendItemFormat for G
  * (era). So if that value is, say "{0}, {1}" then the final resulting
  * pattern is "d-MM-yyyy, G".
  * <p>
  * There are actually three available variables: {0} is the pattern so far,
  * {1} is the element we are adding, and {2} is the name of the element.
  * <p>
  * This reflects the way that the CLDR data is organized.
  *
  * @param dtpg   a pointer to UDateTimePatternGenerator.
  * @param field  UDateTimePatternField, such as UDATPG_ERA_FIELD
  * @param value  pattern, such as "{0}, {1}"
  * @param length the length of value.
  * @stable ICU 3.8
  */
U_STABLE void U_EXPORT2
udatpg_setAppendItemFormat(UDateTimePatternGenerator *dtpg,
                           UDateTimePatternField field,
                           const UChar *value, int32_t length);

/**
 * Getter corresponding to setAppendItemFormat. Values below 0 or at or
 * above UDATPG_FIELD_COUNT are illegal arguments.
 *
 * @param dtpg   A pointer to UDateTimePatternGenerator.
 * @param field  UDateTimePatternField, such as UDATPG_ERA_FIELD
 * @param pLength A pointer that will receive the length of appendItemFormat.
 * @return appendItemFormat for field.
 * @stable ICU 3.8
 */
U_STABLE const UChar * U_EXPORT2
udatpg_getAppendItemFormat(const UDateTimePatternGenerator *dtpg,
                           UDateTimePatternField field,
                           int32_t *pLength);

/**
   * Set the name of field, eg "era" in English for ERA. These are only
   * used if the corresponding AppendItemFormat is used, and if it contains a
   * {2} variable.
   * <p>
   * This reflects the way that the CLDR data is organized.
   *
   * @param dtpg   a pointer to UDateTimePatternGenerator.
   * @param field  UDateTimePatternField
   * @param value  name for the field.
   * @param length the length of value.
   * @stable ICU 3.8
   */
U_STABLE void U_EXPORT2
udatpg_setAppendItemName(UDateTimePatternGenerator *dtpg,
                         UDateTimePatternField field,
                         const UChar *value, int32_t length);

/**
 * Getter corresponding to setAppendItemNames. Values below 0 or at or above
 * UDATPG_FIELD_COUNT are illegal arguments.
 *
 * @param dtpg   a pointer to UDateTimePatternGenerator.
 * @param field  UDateTimePatternField, such as UDATPG_ERA_FIELD
 * @param pLength A pointer that will receive the length of the name for field.
 * @return name for field
 * @stable ICU 3.8
 */
U_STABLE const UChar * U_EXPORT2
udatpg_getAppendItemName(const UDateTimePatternGenerator *dtpg,
                         UDateTimePatternField field,
                         int32_t *pLength);

/**
 * The DateTimeFormat is a message format pattern used to compose date and
 * time patterns. The default pattern in the root locale is "{1} {0}", where
 * {1} will be replaced by the date pattern and {0} will be replaced by the
 * time pattern; however, other locales may specify patterns such as
 * "{1}, {0}" or "{1} 'at' {0}", etc.
 * <p>
 * This is used when the input skeleton contains both date and time fields,
 * but there is not a close match among the added patterns. For example,
 * suppose that this object was created by adding "dd-MMM" and "hh:mm", and
 * its DateTimeFormat is the default "{1} {0}". Then if the input skeleton
 * is "MMMdhmm", there is not an exact match, so the input skeleton is
 * broken up into two components "MMMd" and "hmm". There are close matches
 * for those two skeletons, so the result is put together with this pattern,
 * resulting in "d-MMM h:mm".
 *
 * @param dtpg a pointer to UDateTimePatternGenerator.
 * @param dtFormat
 *            message format pattern, here {1} will be replaced by the date
 *            pattern and {0} will be replaced by the time pattern.
 * @param length the length of dtFormat.
 * @stable ICU 3.8
 */
U_STABLE void U_EXPORT2
udatpg_setDateTimeFormat(const UDateTimePatternGenerator *dtpg,
                         const UChar *dtFormat, int32_t length);

/**
 * Getter corresponding to setDateTimeFormat.
 * @param dtpg   a pointer to UDateTimePatternGenerator.
 * @param pLength A pointer that will receive the length of the format
 * @return dateTimeFormat.
 * @stable ICU 3.8
 */
U_STABLE const UChar * U_EXPORT2
udatpg_getDateTimeFormat(const UDateTimePatternGenerator *dtpg,
                         int32_t *pLength);

/**
 * The decimal value is used in formatting fractions of seconds. If the
 * skeleton contains fractional seconds, then this is used with the
 * fractional seconds. For example, suppose that the input pattern is
 * "hhmmssSSSS", and the best matching pattern internally is "H:mm:ss", and
 * the decimal string is ",". Then the resulting pattern is modified to be
 * "H:mm:ss,SSSS"
 *
 * @param dtpg a pointer to UDateTimePatternGenerator.
 * @param decimal
 * @param length the length of decimal.
 * @stable ICU 3.8
 */
U_STABLE void U_EXPORT2
udatpg_setDecimal(UDateTimePatternGenerator *dtpg,
                  const UChar *decimal, int32_t length);

/**
 * Getter corresponding to setDecimal.
 *
 * @param dtpg a pointer to UDateTimePatternGenerator.
 * @param pLength A pointer that will receive the length of the decimal string.
 * @return corresponding to the decimal point.
 * @stable ICU 3.8
 */
U_STABLE const UChar * U_EXPORT2
udatpg_getDecimal(const UDateTimePatternGenerator *dtpg,
                  int32_t *pLength);

/**
 * Adjusts the field types (width and subtype) of a pattern to match what is
 * in a skeleton. That is, if you supply a pattern like "d-M H:m", and a
 * skeleton of "MMMMddhhmm", then the input pattern is adjusted to be
 * "dd-MMMM hh:mm". This is used internally to get the best match for the
 * input skeleton, but can also be used externally.
 *
 * Note that this function uses a non-const UDateTimePatternGenerator:
 * It uses a stateful pattern parser which is set up for each generator object,
 * rather than creating one for each function call.
 * Consecutive calls to this function do not affect each other,
 * but this function cannot be used concurrently on a single generator object.
 *
 * @param dtpg a pointer to UDateTimePatternGenerator.
 * @param pattern Input pattern
 * @param patternLength the length of input pattern.
 * @param skeleton
 * @param skeletonLength the length of input skeleton.
 * @param dest  pattern adjusted to match the skeleton fields widths and subtypes.
 * @param destCapacity the capacity of dest.
 * @param pErrorCode a pointer to the UErrorCode which must not indicate a
 *                  failure before the function call.
 * @return the length of dest.
 * @stable ICU 3.8
 */
U_STABLE int32_t U_EXPORT2
udatpg_replaceFieldTypes(UDateTimePatternGenerator *dtpg,
                         const UChar *pattern, int32_t patternLength,
                         const UChar *skeleton, int32_t skeletonLength,
                         UChar *dest, int32_t destCapacity,
                         UErrorCode *pErrorCode);

/**
 * Adjusts the field types (width and subtype) of a pattern to match what is
 * in a skeleton. That is, if you supply a pattern like "d-M H:m", and a
 * skeleton of "MMMMddhhmm", then the input pattern is adjusted to be
 * "dd-MMMM hh:mm". This is used internally to get the best match for the
 * input skeleton, but can also be used externally.
 *
 * Note that this function uses a non-const UDateTimePatternGenerator:
 * It uses a stateful pattern parser which is set up for each generator object,
 * rather than creating one for each function call.
 * Consecutive calls to this function do not affect each other,
 * but this function cannot be used concurrently on a single generator object.
 *
 * @param dtpg a pointer to UDateTimePatternGenerator.
 * @param pattern Input pattern
 * @param patternLength the length of input pattern.
 * @param skeleton
 * @param skeletonLength the length of input skeleton.
 * @param options
 *            Options controlling whether the length of specified fields in the
 *            pattern are adjusted to match those in the skeleton (when this
 *            would not happen otherwise). For default behavior, use
 *            UDATPG_MATCH_NO_OPTIONS.
 * @param dest  pattern adjusted to match the skeleton fields widths and subtypes.
 * @param destCapacity the capacity of dest.
 * @param pErrorCode a pointer to the UErrorCode which must not indicate a
 *                  failure before the function call.
 * @return the length of dest.
 * @stable ICU 4.4
 */
U_STABLE int32_t U_EXPORT2
udatpg_replaceFieldTypesWithOptions(UDateTimePatternGenerator *dtpg,
                                    const UChar *pattern, int32_t patternLength,
                                    const UChar *skeleton, int32_t skeletonLength,
                                    UDateTimePatternMatchOptions options,
                                    UChar *dest, int32_t destCapacity,
                                    UErrorCode *pErrorCode);

/**
 * Return a UEnumeration list of all the skeletons in canonical form.
 * Call udatpg_getPatternForSkeleton() to get the corresponding pattern.
 *
 * @param dtpg a pointer to UDateTimePatternGenerator.
 * @param pErrorCode a pointer to the UErrorCode which must not indicate a
 *                  failure before the function call
 * @return a UEnumeration list of all the skeletons
 *         The caller must close the object.
 * @stable ICU 3.8
 */
U_STABLE UEnumeration * U_EXPORT2
udatpg_openSkeletons(const UDateTimePatternGenerator *dtpg, UErrorCode *pErrorCode);

/**
 * Return a UEnumeration list of all the base skeletons in canonical form.
 *
 * @param dtpg a pointer to UDateTimePatternGenerator.
 * @param pErrorCode a pointer to the UErrorCode which must not indicate a
 *             failure before the function call.
 * @return a UEnumeration list of all the base skeletons
 *             The caller must close the object.
 * @stable ICU 3.8
 */
U_STABLE UEnumeration * U_EXPORT2
udatpg_openBaseSkeletons(const UDateTimePatternGenerator *dtpg, UErrorCode *pErrorCode);

/**
 * Get the pattern corresponding to a given skeleton.
 *
 * @param dtpg a pointer to UDateTimePatternGenerator.
 * @param skeleton
 * @param skeletonLength pointer to the length of skeleton.
 * @param pLength pointer to the length of return pattern.
 * @return pattern corresponding to a given skeleton.
 * @stable ICU 3.8
 */
U_STABLE const UChar * U_EXPORT2
udatpg_getPatternForSkeleton(const UDateTimePatternGenerator *dtpg,
                             const UChar *skeleton, int32_t skeletonLength,
                             int32_t *pLength);

#endif
