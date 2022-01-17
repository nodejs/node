// © 2018 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#ifndef __UNUMBERFORMATTER_H__
#define __UNUMBERFORMATTER_H__

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/parseerr.h"
#include "unicode/ufieldpositer.h"
#include "unicode/umisc.h"
#include "unicode/uformattedvalue.h"


/**
 * \file
 * \brief C-compatible API for localized number formatting; not recommended for C++.
 *
 * This is the C-compatible version of the NumberFormatter API introduced in ICU 60. C++ users should
 * include unicode/numberformatter.h and use the proper C++ APIs.
 *
 * The C API accepts a number skeleton string for specifying the settings for formatting, which covers a
 * very large subset of all possible number formatting features. For more information on number skeleton
 * strings, see unicode/numberformatter.h.
 *
 * When using UNumberFormatter, which is treated as immutable, the results are exported to a mutable
 * UFormattedNumber object, which you subsequently use for populating your string buffer or iterating over
 * the fields.
 *
 * Example code:
 * <pre>
 * // Setup:
 * UErrorCode ec = U_ZERO_ERROR;
 * UNumberFormatter* uformatter = unumf_openForSkeletonAndLocale(u"precision-integer", -1, "en", &ec);
 * UFormattedNumber* uresult = unumf_openResult(&ec);
 * if (U_FAILURE(ec)) { return; }
 *
 * // Format a double:
 * unumf_formatDouble(uformatter, 5142.3, uresult, &ec);
 * if (U_FAILURE(ec)) { return; }
 *
 * // Export the string to a malloc'd buffer:
 * int32_t len = unumf_resultToString(uresult, NULL, 0, &ec);
 * // at this point, ec == U_BUFFER_OVERFLOW_ERROR
 * ec = U_ZERO_ERROR;
 * UChar* buffer = (UChar*) malloc((len+1)*sizeof(UChar));
 * unumf_resultToString(uresult, buffer, len+1, &ec);
 * if (U_FAILURE(ec)) { return; }
 * // buffer should equal "5,142"
 *
 * // Cleanup:
 * unumf_close(uformatter);
 * unumf_closeResult(uresult);
 * free(buffer);
 * </pre>
 *
 * If you are a C++ user linking against the C libraries, you can use the LocalPointer versions of these
 * APIs. The following example uses LocalPointer with the decimal number and field position APIs:
 *
 * <pre>
 * // Setup:
 * LocalUNumberFormatterPointer uformatter(unumf_openForSkeletonAndLocale(u"percent", -1, "en", &ec));
 * LocalUFormattedNumberPointer uresult(unumf_openResult(&ec));
 * if (U_FAILURE(ec)) { return; }
 *
 * // Format a decimal number:
 * unumf_formatDecimal(uformatter.getAlias(), "9.87E-3", -1, uresult.getAlias(), &ec);
 * if (U_FAILURE(ec)) { return; }
 *
 * // Get the location of the percent sign:
 * UFieldPosition ufpos = {UNUM_PERCENT_FIELD, 0, 0};
 * unumf_resultNextFieldPosition(uresult.getAlias(), &ufpos, &ec);
 * // ufpos should contain beginIndex=7 and endIndex=8 since the string is "0.00987%"
 *
 * // No need to do any cleanup since we are using LocalPointer.
 * </pre>
 */

#ifndef U_FORCE_HIDE_DRAFT_API
/**
 * An enum declaring how to resolve conflicts between maximum fraction digits and maximum
 * significant digits.
 *
 * There are two modes, RELAXED and STRICT:
 *
 * - RELAXED: Relax one of the two constraints (fraction digits or significant digits) in order
 *   to round the number to a higher level of precision.
 * - STRICT: Enforce both constraints, resulting in the number being rounded to a lower
 *   level of precision.
 *
 * The default settings for compact notation rounding are Max-Fraction = 0 (round to the nearest
 * integer), Max-Significant = 2 (round to 2 significant digits), and priority RELAXED (choose
 * the constraint that results in more digits being displayed).
 *
 * Conflicting *minimum* fraction and significant digits are always resolved in the direction that
 * results in more trailing zeros.
 *
 * Example 1: Consider the number 3.141, with various different settings:
 *
 * - Max-Fraction = 1: "3.1"
 * - Max-Significant = 3: "3.14"
 *
 * The rounding priority determines how to resolve the conflict when both Max-Fraction and
 * Max-Significant are set. With RELAXED, the less-strict setting (the one that causes more digits
 * to be displayed) will be used; Max-Significant wins. With STRICT, the more-strict setting (the
 * one that causes fewer digits to be displayed) will be used; Max-Fraction wins.
 *
 * Example 2: Consider the number 8317, with various different settings:
 *
 * - Max-Fraction = 1: "8317"
 * - Max-Significant = 3: "8320"
 *
 * Here, RELAXED favors Max-Fraction and STRICT favors Max-Significant. Note that this larger
 * number caused the two modes to favor the opposite result.
 *
 * @draft ICU 69
 */
typedef enum UNumberRoundingPriority {
    /**
     * Favor greater precision by relaxing one of the rounding constraints.
     *
     * @draft ICU 69
     */
    UNUM_ROUNDING_PRIORITY_RELAXED,

    /**
     * Favor adherence to all rounding constraints by producing lower precision.
     *
     * @draft ICU 69
     */
    UNUM_ROUNDING_PRIORITY_STRICT,
} UNumberRoundingPriority;
#endif // U_FORCE_HIDE_DRAFT_API

/**
 * An enum declaring how to render units, including currencies. Example outputs when formatting 123 USD and 123
 * meters in <em>en-CA</em>:
 *
 * <p>
 * <ul>
 * <li>NARROW*: "$123.00" and "123 m"
 * <li>SHORT: "US$ 123.00" and "123 m"
 * <li>FULL_NAME: "123.00 US dollars" and "123 meters"
 * <li>ISO_CODE: "USD 123.00" and undefined behavior
 * <li>HIDDEN: "123.00" and "123"
 * </ul>
 *
 * <p>
 * This enum is similar to {@link UMeasureFormatWidth}.
 *
 * @stable ICU 60
 */
typedef enum UNumberUnitWidth {
    /**
     * Print an abbreviated version of the unit name. Similar to SHORT, but always use the shortest available
     * abbreviation or symbol. This option can be used when the context hints at the identity of the unit. For more
     * information on the difference between NARROW and SHORT, see SHORT.
     *
     * <p>
     * In CLDR, this option corresponds to the "Narrow" format for measure units and the "¤¤¤¤¤" placeholder for
     * currencies.
     *
     * @stable ICU 60
     */
            UNUM_UNIT_WIDTH_NARROW = 0,

    /**
     * Print an abbreviated version of the unit name. Similar to NARROW, but use a slightly wider abbreviation or
     * symbol when there may be ambiguity. This is the default behavior.
     *
     * <p>
     * For example, in <em>es-US</em>, the SHORT form for Fahrenheit is "{0} °F", but the NARROW form is "{0}°",
     * since Fahrenheit is the customary unit for temperature in that locale.
     *
     * <p>
     * In CLDR, this option corresponds to the "Short" format for measure units and the "¤" placeholder for
     * currencies.
     *
     * @stable ICU 60
     */
            UNUM_UNIT_WIDTH_SHORT = 1,

    /**
     * Print the full name of the unit, without any abbreviations.
     *
     * <p>
     * In CLDR, this option corresponds to the default format for measure units and the "¤¤¤" placeholder for
     * currencies.
     *
     * @stable ICU 60
     */
            UNUM_UNIT_WIDTH_FULL_NAME = 2,

    /**
     * Use the three-digit ISO XXX code in place of the symbol for displaying currencies. The behavior of this
     * option is currently undefined for use with measure units.
     *
     * <p>
     * In CLDR, this option corresponds to the "¤¤" placeholder for currencies.
     *
     * @stable ICU 60
     */
            UNUM_UNIT_WIDTH_ISO_CODE = 3,

    /**
     * Use the formal variant of the currency symbol; for example, "NT$" for the New Taiwan
     * dollar in zh-TW.
     *
     * <p>
     * Behavior of this option with non-currency units is not defined at this time.
     *
     * @stable ICU 68
     */
            UNUM_UNIT_WIDTH_FORMAL = 4,

    /**
     * Use the alternate variant of the currency symbol; for example, "TL" for the Turkish
     * lira (TRY).
     *
     * <p>
     * Behavior of this option with non-currency units is not defined at this time.
     *
     * @stable ICU 68
     */
            UNUM_UNIT_WIDTH_VARIANT = 5,

    /**
     * Format the number according to the specified unit, but do not display the unit. For currencies, apply
     * monetary symbols and formats as with SHORT, but omit the currency symbol. For measure units, the behavior is
     * equivalent to not specifying the unit at all.
     *
     * @stable ICU 60
     */
            UNUM_UNIT_WIDTH_HIDDEN = 6,

    // Do not conditionalize the following with #ifndef U_HIDE_INTERNAL_API,
    // needed for unconditionalized struct MacroProps
    /**
     * One more than the highest UNumberUnitWidth value.
     *
     * @internal ICU 60: The numeric value may change over time; see ICU ticket #12420.
     */
            UNUM_UNIT_WIDTH_COUNT = 7
} UNumberUnitWidth;

/**
 * An enum declaring the strategy for when and how to display grouping separators (i.e., the
 * separator, often a comma or period, after every 2-3 powers of ten). The choices are several
 * pre-built strategies for different use cases that employ locale data whenever possible. Example
 * outputs for 1234 and 1234567 in <em>en-IN</em>:
 *
 * <ul>
 * <li>OFF: 1234 and 12345
 * <li>MIN2: 1234 and 12,34,567
 * <li>AUTO: 1,234 and 12,34,567
 * <li>ON_ALIGNED: 1,234 and 12,34,567
 * <li>THOUSANDS: 1,234 and 1,234,567
 * </ul>
 *
 * <p>
 * The default is AUTO, which displays grouping separators unless the locale data says that grouping
 * is not customary. To force grouping for all numbers greater than 1000 consistently across locales,
 * use ON_ALIGNED. On the other hand, to display grouping less frequently than the default, use MIN2
 * or OFF. See the docs of each option for details.
 *
 * <p>
 * Note: This enum specifies the strategy for grouping sizes. To set which character to use as the
 * grouping separator, use the "symbols" setter.
 *
 * @stable ICU 63
 */
typedef enum UNumberGroupingStrategy {
    /**
     * Do not display grouping separators in any locale.
     *
     * @stable ICU 61
     */
            UNUM_GROUPING_OFF,

    /**
     * Display grouping using locale defaults, except do not show grouping on values smaller than
     * 10000 (such that there is a <em>minimum of two digits</em> before the first separator).
     *
     * <p>
     * Note that locales may restrict grouping separators to be displayed only on 1 million or
     * greater (for example, ee and hu) or disable grouping altogether (for example, bg currency).
     *
     * <p>
     * Locale data is used to determine whether to separate larger numbers into groups of 2
     * (customary in South Asia) or groups of 3 (customary in Europe and the Americas).
     *
     * @stable ICU 61
     */
            UNUM_GROUPING_MIN2,

    /**
     * Display grouping using the default strategy for all locales. This is the default behavior.
     *
     * <p>
     * Note that locales may restrict grouping separators to be displayed only on 1 million or
     * greater (for example, ee and hu) or disable grouping altogether (for example, bg currency).
     *
     * <p>
     * Locale data is used to determine whether to separate larger numbers into groups of 2
     * (customary in South Asia) or groups of 3 (customary in Europe and the Americas).
     *
     * @stable ICU 61
     */
            UNUM_GROUPING_AUTO,

    /**
     * Always display the grouping separator on values of at least 1000.
     *
     * <p>
     * This option ignores the locale data that restricts or disables grouping, described in MIN2 and
     * AUTO. This option may be useful to normalize the alignment of numbers, such as in a
     * spreadsheet.
     *
     * <p>
     * Locale data is used to determine whether to separate larger numbers into groups of 2
     * (customary in South Asia) or groups of 3 (customary in Europe and the Americas).
     *
     * @stable ICU 61
     */
            UNUM_GROUPING_ON_ALIGNED,

    /**
     * Use the Western defaults: groups of 3 and enabled for all numbers 1000 or greater. Do not use
     * locale data for determining the grouping strategy.
     *
     * @stable ICU 61
     */
            UNUM_GROUPING_THOUSANDS

#ifndef U_HIDE_INTERNAL_API
    ,
    /**
     * One more than the highest UNumberGroupingStrategy value.
     *
     * @internal ICU 62: The numeric value may change over time; see ICU ticket #12420.
     */
            UNUM_GROUPING_COUNT
#endif  /* U_HIDE_INTERNAL_API */

} UNumberGroupingStrategy;

/**
 * An enum declaring how to denote positive and negative numbers. Example outputs when formatting
 * 123, 0, and -123 in <em>en-US</em>:
 *
 * <ul>
 * <li>AUTO: "123", "0", and "-123"
 * <li>ALWAYS: "+123", "+0", and "-123"
 * <li>NEVER: "123", "0", and "123"
 * <li>ACCOUNTING: "$123", "$0", and "($123)"
 * <li>ACCOUNTING_ALWAYS: "+$123", "+$0", and "($123)"
 * <li>EXCEPT_ZERO: "+123", "0", and "-123"
 * <li>ACCOUNTING_EXCEPT_ZERO: "+$123", "$0", and "($123)"
 * </ul>
 *
 * <p>
 * The exact format, including the position and the code point of the sign, differ by locale.
 *
 * @stable ICU 60
 */
typedef enum UNumberSignDisplay {
    /**
     * Show the minus sign on negative numbers, and do not show the sign on positive numbers. This is the default
     * behavior.
     *
     * If using this option, a sign will be displayed on negative zero, including negative numbers
     * that round to zero. To hide the sign on negative zero, use the NEGATIVE option.
     *
     * @stable ICU 60
     */
    UNUM_SIGN_AUTO,

    /**
     * Show the minus sign on negative numbers and the plus sign on positive numbers, including zero.
     * To hide the sign on zero, see {@link UNUM_SIGN_EXCEPT_ZERO}.
     *
     * @stable ICU 60
     */
    UNUM_SIGN_ALWAYS,

    /**
     * Do not show the sign on positive or negative numbers.
     *
     * @stable ICU 60
     */
    UNUM_SIGN_NEVER,

    /**
     * Use the locale-dependent accounting format on negative numbers, and do not show the sign on positive numbers.
     *
     * <p>
     * The accounting format is defined in CLDR and varies by locale; in many Western locales, the format is a pair
     * of parentheses around the number.
     *
     * <p>
     * Note: Since CLDR defines the accounting format in the monetary context only, this option falls back to the
     * AUTO sign display strategy when formatting without a currency unit. This limitation may be lifted in the
     * future.
     *
     * @stable ICU 60
     */
    UNUM_SIGN_ACCOUNTING,

    /**
     * Use the locale-dependent accounting format on negative numbers, and show the plus sign on
     * positive numbers, including zero. For more information on the accounting format, see the
     * ACCOUNTING sign display strategy. To hide the sign on zero, see
     * {@link UNUM_SIGN_ACCOUNTING_EXCEPT_ZERO}.
     *
     * @stable ICU 60
     */
    UNUM_SIGN_ACCOUNTING_ALWAYS,

    /**
     * Show the minus sign on negative numbers and the plus sign on positive numbers. Do not show a
     * sign on zero, numbers that round to zero, or NaN.
     *
     * @stable ICU 61
     */
    UNUM_SIGN_EXCEPT_ZERO,

    /**
     * Use the locale-dependent accounting format on negative numbers, and show the plus sign on
     * positive numbers. Do not show a sign on zero, numbers that round to zero, or NaN. For more
     * information on the accounting format, see the ACCOUNTING sign display strategy.
     *
     * @stable ICU 61
     */
    UNUM_SIGN_ACCOUNTING_EXCEPT_ZERO,

#ifndef U_HIDE_DRAFT_API
    /**
     * Same as AUTO, but do not show the sign on negative zero.
     *
     * @draft ICU 69
     */
    UNUM_SIGN_NEGATIVE,

    /**
     * Same as ACCOUNTING, but do not show the sign on negative zero.
     *
     * @draft ICU 69
     */
    UNUM_SIGN_ACCOUNTING_NEGATIVE,
#endif // U_HIDE_DRAFT_API

    // Do not conditionalize the following with #ifndef U_HIDE_INTERNAL_API,
    // needed for unconditionalized struct MacroProps
    /**
     * One more than the highest UNumberSignDisplay value.
     *
     * @internal ICU 60: The numeric value may change over time; see ICU ticket #12420.
     */
    UNUM_SIGN_COUNT = 9,
} UNumberSignDisplay;

/**
 * An enum declaring how to render the decimal separator.
 *
 * <p>
 * <ul>
 * <li>UNUM_DECIMAL_SEPARATOR_AUTO: "1", "1.1"
 * <li>UNUM_DECIMAL_SEPARATOR_ALWAYS: "1.", "1.1"
 * </ul>
 *
 * @stable ICU 60
 */
typedef enum UNumberDecimalSeparatorDisplay {
    /**
     * Show the decimal separator when there are one or more digits to display after the separator, and do not show
     * it otherwise. This is the default behavior.
     *
     * @stable ICU 60
     */
            UNUM_DECIMAL_SEPARATOR_AUTO,

    /**
     * Always show the decimal separator, even if there are no digits to display after the separator.
     *
     * @stable ICU 60
     */
            UNUM_DECIMAL_SEPARATOR_ALWAYS,

    // Do not conditionalize the following with #ifndef U_HIDE_INTERNAL_API,
    // needed for unconditionalized struct MacroProps
    /**
     * One more than the highest UNumberDecimalSeparatorDisplay value.
     *
     * @internal ICU 60: The numeric value may change over time; see ICU ticket #12420.
     */
            UNUM_DECIMAL_SEPARATOR_COUNT
} UNumberDecimalSeparatorDisplay;

#ifndef U_FORCE_HIDE_DRAFT_API
/**
 * An enum declaring how to render trailing zeros.
 * 
 * - UNUM_TRAILING_ZERO_AUTO: 0.90, 1.00, 1.10
 * - UNUM_TRAILING_ZERO_HIDE_IF_WHOLE: 0.90, 1, 1.10
 * 
 * @draft ICU 69
 */
typedef enum UNumberTrailingZeroDisplay {
    /**
     * Display trailing zeros according to the settings for minimum fraction and significant digits.
     *
     * @draft ICU 69
     */
    UNUM_TRAILING_ZERO_AUTO,

    /**
     * Same as AUTO, but hide trailing zeros after the decimal separator if they are all zero.
     *
     * @draft ICU 69
     */
    UNUM_TRAILING_ZERO_HIDE_IF_WHOLE,
} UNumberTrailingZeroDisplay;
#endif // U_FORCE_HIDE_DRAFT_API

struct UNumberFormatter;
/**
 * C-compatible version of icu::number::LocalizedNumberFormatter.
 *
 * NOTE: This is a C-compatible API; C++ users should build against numberformatter.h instead.
 *
 * @stable ICU 62
 */
typedef struct UNumberFormatter UNumberFormatter;

struct UFormattedNumber;
/**
 * C-compatible version of icu::number::FormattedNumber.
 *
 * NOTE: This is a C-compatible API; C++ users should build against numberformatter.h instead.
 *
 * @stable ICU 62
 */
typedef struct UFormattedNumber UFormattedNumber;


/**
 * Creates a new UNumberFormatter for the given skeleton string and locale. This is currently the only
 * method for creating a new UNumberFormatter.
 *
 * Objects of type UNumberFormatter returned by this method are threadsafe.
 *
 * For more details on skeleton strings, see the documentation in numberformatter.h. For more details on
 * the usage of this API, see the documentation at the top of unumberformatter.h.
 *
 * For more information on number skeleton strings, see:
 * https://unicode-org.github.io/icu/userguide/format_parse/numbers/skeletons.html
 *
 * NOTE: This is a C-compatible API; C++ users should build against numberformatter.h instead.
 *
 * @param skeleton The skeleton string, like u"percent precision-integer"
 * @param skeletonLen The number of UChars in the skeleton string, or -1 if it is NUL-terminated.
 * @param locale The NUL-terminated locale ID.
 * @param ec Set if an error occurs.
 * @stable ICU 62
 */
U_CAPI UNumberFormatter* U_EXPORT2
unumf_openForSkeletonAndLocale(const UChar* skeleton, int32_t skeletonLen, const char* locale,
                               UErrorCode* ec);


/**
 * Like unumf_openForSkeletonAndLocale, but accepts a UParseError, which will be populated with the
 * location of a skeleton syntax error if such a syntax error exists.
 *
 * For more information on number skeleton strings, see:
 * https://unicode-org.github.io/icu/userguide/format_parse/numbers/skeletons.html
 *
 * @param skeleton The skeleton string, like u"percent precision-integer"
 * @param skeletonLen The number of UChars in the skeleton string, or -1 if it is NUL-terminated.
 * @param locale The NUL-terminated locale ID.
 * @param perror A parse error struct populated if an error occurs when parsing. Can be NULL.
 *               If no error occurs, perror->offset will be set to -1.
 * @param ec Set if an error occurs.
 * @stable ICU 64
 */
U_CAPI UNumberFormatter* U_EXPORT2
unumf_openForSkeletonAndLocaleWithError(
       const UChar* skeleton, int32_t skeletonLen, const char* locale, UParseError* perror, UErrorCode* ec);


/**
 * Creates an object to hold the result of a UNumberFormatter
 * operation. The object can be used repeatedly; it is cleared whenever
 * passed to a format function.
 *
 * @param ec Set if an error occurs.
 * @stable ICU 62
 */
U_CAPI UFormattedNumber* U_EXPORT2
unumf_openResult(UErrorCode* ec);


/**
 * Uses a UNumberFormatter to format an integer to a UFormattedNumber. A string, field position, and other
 * information can be retrieved from the UFormattedNumber.
 *
 * The UNumberFormatter can be shared between threads. Each thread should have its own local
 * UFormattedNumber, however, for storing the result of the formatting operation.
 *
 * NOTE: This is a C-compatible API; C++ users should build against numberformatter.h instead.
 *
 * @param uformatter A formatter object created by unumf_openForSkeletonAndLocale or similar.
 * @param value The number to be formatted.
 * @param uresult The object that will be mutated to store the result; see unumf_openResult.
 * @param ec Set if an error occurs.
 * @stable ICU 62
 */
U_CAPI void U_EXPORT2
unumf_formatInt(const UNumberFormatter* uformatter, int64_t value, UFormattedNumber* uresult,
                UErrorCode* ec);


/**
 * Uses a UNumberFormatter to format a double to a UFormattedNumber. A string, field position, and other
 * information can be retrieved from the UFormattedNumber.
 *
 * The UNumberFormatter can be shared between threads. Each thread should have its own local
 * UFormattedNumber, however, for storing the result of the formatting operation.
 *
 * NOTE: This is a C-compatible API; C++ users should build against numberformatter.h instead.
 *
 * @param uformatter A formatter object created by unumf_openForSkeletonAndLocale or similar.
 * @param value The number to be formatted.
 * @param uresult The object that will be mutated to store the result; see unumf_openResult.
 * @param ec Set if an error occurs.
 * @stable ICU 62
 */
U_CAPI void U_EXPORT2
unumf_formatDouble(const UNumberFormatter* uformatter, double value, UFormattedNumber* uresult,
                   UErrorCode* ec);


/**
 * Uses a UNumberFormatter to format a decimal number to a UFormattedNumber. A string, field position, and
 * other information can be retrieved from the UFormattedNumber.
 *
 * The UNumberFormatter can be shared between threads. Each thread should have its own local
 * UFormattedNumber, however, for storing the result of the formatting operation.
 *
 * The syntax of the unformatted number is a "numeric string" as defined in the Decimal Arithmetic
 * Specification, available at http://speleotrove.com/decimal
 *
 * NOTE: This is a C-compatible API; C++ users should build against numberformatter.h instead.
 *
 * @param uformatter A formatter object created by unumf_openForSkeletonAndLocale or similar.
 * @param value The numeric string to be formatted.
 * @param valueLen The length of the numeric string, or -1 if it is NUL-terminated.
 * @param uresult The object that will be mutated to store the result; see unumf_openResult.
 * @param ec Set if an error occurs.
 * @stable ICU 62
 */
U_CAPI void U_EXPORT2
unumf_formatDecimal(const UNumberFormatter* uformatter, const char* value, int32_t valueLen,
                    UFormattedNumber* uresult, UErrorCode* ec);

/**
 * Returns a representation of a UFormattedNumber as a UFormattedValue,
 * which can be subsequently passed to any API requiring that type.
 *
 * The returned object is owned by the UFormattedNumber and is valid
 * only as long as the UFormattedNumber is present and unchanged in memory.
 *
 * You can think of this method as a cast between types.
 *
 * @param uresult The object containing the formatted string.
 * @param ec Set if an error occurs.
 * @return A UFormattedValue owned by the input object.
 * @stable ICU 64
 */
U_CAPI const UFormattedValue* U_EXPORT2
unumf_resultAsValue(const UFormattedNumber* uresult, UErrorCode* ec);


/**
 * Extracts the result number string out of a UFormattedNumber to a UChar buffer if possible.
 * If bufferCapacity is greater than the required length, a terminating NUL is written.
 * If bufferCapacity is less than the required length, an error code is set.
 *
 * Also see ufmtval_getString, which returns a NUL-terminated string:
 *
 *     int32_t len;
 *     const UChar* str = ufmtval_getString(unumf_resultAsValue(uresult, &ec), &len, &ec);
 *
 * NOTE: This is a C-compatible API; C++ users should build against numberformatter.h instead.
 *
 * @param uresult The object containing the formatted number.
 * @param buffer Where to save the string output.
 * @param bufferCapacity The number of UChars available in the buffer.
 * @param ec Set if an error occurs.
 * @return The required length.
 * @stable ICU 62
 */
U_CAPI int32_t U_EXPORT2
unumf_resultToString(const UFormattedNumber* uresult, UChar* buffer, int32_t bufferCapacity,
                     UErrorCode* ec);


/**
 * Determines the start and end indices of the next occurrence of the given <em>field</em> in the
 * output string. This allows you to determine the locations of, for example, the integer part,
 * fraction part, or symbols.
 *
 * This is a simpler but less powerful alternative to {@link ufmtval_nextPosition}.
 *
 * If a field occurs just once, calling this method will find that occurrence and return it. If a
 * field occurs multiple times, this method may be called repeatedly with the following pattern:
 *
 * <pre>
 * UFieldPosition ufpos = {UNUM_GROUPING_SEPARATOR_FIELD, 0, 0};
 * while (unumf_resultNextFieldPosition(uresult, ufpos, &ec)) {
 *   // do something with ufpos.
 * }
 * </pre>
 *
 * This method is useful if you know which field to query. If you want all available field position
 * information, use unumf_resultGetAllFieldPositions().
 *
 * NOTE: All fields of the UFieldPosition must be initialized before calling this method.
 *
 * @param uresult The object containing the formatted number.
 * @param ufpos
 *            Input+output variable. On input, the "field" property determines which field to look up,
 *            and the "endIndex" property determines where to begin the search. On output, the
 *            "beginIndex" field is set to the beginning of the first occurrence of the field after the
 *            input "endIndex", and "endIndex" is set to the end of that occurrence of the field
 *            (exclusive index). If a field position is not found, the FieldPosition is not changed and
 *            the method returns false.
 * @param ec Set if an error occurs.
 * @stable ICU 62
 */
U_CAPI UBool U_EXPORT2
unumf_resultNextFieldPosition(const UFormattedNumber* uresult, UFieldPosition* ufpos, UErrorCode* ec);


/**
 * Populates the given iterator with all fields in the formatted output string. This allows you to
 * determine the locations of the integer part, fraction part, and sign.
 *
 * This is an alternative to the more powerful {@link ufmtval_nextPosition} API.
 *
 * If you need information on only one field, use {@link ufmtval_nextPosition} or
 * {@link unumf_resultNextFieldPosition}.
 *
 * @param uresult The object containing the formatted number.
 * @param ufpositer
 *         A pointer to a UFieldPositionIterator created by {@link #ufieldpositer_open}. Iteration
 *         information already present in the UFieldPositionIterator is deleted, and the iterator is reset
 *         to apply to the fields in the formatted string created by this function call. The field values
 *         and indexes returned by {@link #ufieldpositer_next} represent fields denoted by
 *         the UNumberFormatFields enum. Fields are not returned in a guaranteed order. Fields cannot
 *         overlap, but they may nest. For example, 1234 could format as "1,234" which might consist of a
 *         grouping separator field for ',' and an integer field encompassing the entire string.
 * @param ec Set if an error occurs.
 * @stable ICU 62
 */
U_CAPI void U_EXPORT2
unumf_resultGetAllFieldPositions(const UFormattedNumber* uresult, UFieldPositionIterator* ufpositer,
                                 UErrorCode* ec);


/**
 * Extracts the formatted number as a "numeric string" conforming to the
 * syntax defined in the Decimal Arithmetic Specification, available at
 * http://speleotrove.com/decimal
 *
 * This endpoint is useful for obtaining the exact number being printed
 * after scaling and rounding have been applied by the number formatter.
 *
 * @param uresult        The input object containing the formatted number.
 * @param  dest          the 8-bit char buffer into which the decimal number is placed
 * @param  destCapacity  The size, in chars, of the destination buffer.  May be zero
 *                       for precomputing the required size.
 * @param  ec            receives any error status.
 *                       If U_BUFFER_OVERFLOW_ERROR: Returns number of chars for
 *                       preflighting.
 * @return Number of chars in the data.  Does not include a trailing NUL.
 * @stable ICU 68
 */
U_CAPI int32_t U_EXPORT2
unumf_resultToDecimalNumber(
       const UFormattedNumber* uresult,
       char* dest,
       int32_t destCapacity,
       UErrorCode* ec);


/**
 * Releases the UNumberFormatter created by unumf_openForSkeletonAndLocale().
 *
 * @param uformatter An object created by unumf_openForSkeletonAndLocale().
 * @stable ICU 62
 */
U_CAPI void U_EXPORT2
unumf_close(UNumberFormatter* uformatter);


/**
 * Releases the UFormattedNumber created by unumf_openResult().
 *
 * @param uresult An object created by unumf_openResult().
 * @stable ICU 62
 */
U_CAPI void U_EXPORT2
unumf_closeResult(UFormattedNumber* uresult);


#if U_SHOW_CPLUSPLUS_API
U_NAMESPACE_BEGIN

/**
 * \class LocalUNumberFormatterPointer
 * "Smart pointer" class; closes a UNumberFormatter via unumf_close().
 * For most methods see the LocalPointerBase base class.
 *
 * Usage:
 * <pre>
 * LocalUNumberFormatterPointer uformatter(unumf_openForSkeletonAndLocale(...));
 * // no need to explicitly call unumf_close()
 * </pre>
 *
 * @see LocalPointerBase
 * @see LocalPointer
 * @stable ICU 62
 */
U_DEFINE_LOCAL_OPEN_POINTER(LocalUNumberFormatterPointer, UNumberFormatter, unumf_close);

/**
 * \class LocalUFormattedNumberPointer
 * "Smart pointer" class; closes a UFormattedNumber via unumf_closeResult().
 * For most methods see the LocalPointerBase base class.
 *
 * Usage:
 * <pre>
 * LocalUFormattedNumberPointer uformatter(unumf_openResult(...));
 * // no need to explicitly call unumf_closeResult()
 * </pre>
 *
 * @see LocalPointerBase
 * @see LocalPointer
 * @stable ICU 62
 */
U_DEFINE_LOCAL_OPEN_POINTER(LocalUFormattedNumberPointer, UFormattedNumber, unumf_closeResult);

U_NAMESPACE_END
#endif // U_SHOW_CPLUSPLUS_API

#endif /* #if !UCONFIG_NO_FORMATTING */
#endif //__UNUMBERFORMATTER_H__
