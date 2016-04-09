/*
**********************************************************************
* Copyright (c) 2004-2016, International Business Machines
* Corporation and others.  All Rights Reserved.
**********************************************************************
* Author: Alan Liu
* Created: April 20, 2004
* Since: ICU 3.0
**********************************************************************
*/
#ifndef MEASUREFORMAT_H
#define MEASUREFORMAT_H

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/format.h"
#include "unicode/udat.h"

/**
 * \file 
 * \brief C++ API: Formatter for measure objects.
 */

/**
 * Constants for various widths.
 * There are 4 widths: Wide, Short, Narrow, Numeric.
 * For example, for English, when formatting "3 hours"
 * Wide is "3 hours"; short is "3 hrs"; narrow is "3h";
 * formatting "3 hours 17 minutes" as numeric give "3:17"
 * @stable ICU 53
 */
enum UMeasureFormatWidth {

    // Wide, short, and narrow must be first and in this order.
    /**
     * Spell out measure units.
     * @stable ICU 53 
     */
    UMEASFMT_WIDTH_WIDE,
 
    /**
     * Abbreviate measure units.
     * @stable ICU 53
     */
    UMEASFMT_WIDTH_SHORT,

    /**
     * Use symbols for measure units when possible.
     * @stable ICU 53
     */
    UMEASFMT_WIDTH_NARROW,

    /**
     * Completely omit measure units when possible. For example, format
     * '5 hours, 37 minutes' as '5:37'
     * @stable ICU 53
     */
    UMEASFMT_WIDTH_NUMERIC,

    /**
     * Count of values in this enum.
     * @stable ICU 53
     */
    UMEASFMT_WIDTH_COUNT = 4
};
/** @stable ICU 53 */
typedef enum UMeasureFormatWidth UMeasureFormatWidth; 

U_NAMESPACE_BEGIN

class Measure;
class MeasureUnit;
class NumberFormat;
class PluralRules;
class MeasureFormatCacheData;
class SharedNumberFormat;
class SharedPluralRules;
class QuantityFormatter;
class SimpleFormatter;
class ListFormatter;
class DateFormat;

/**
 * 
 * A formatter for measure objects.
 *
 * @see Format
 * @author Alan Liu
 * @stable ICU 3.0
 */
class U_I18N_API MeasureFormat : public Format {
 public:
    using Format::parseObject;
    using Format::format;

    /**
     * Constructor.
     * @stable ICU 53
     */
    MeasureFormat(
            const Locale &locale, UMeasureFormatWidth width, UErrorCode &status);

    /**
     * Constructor.
     * @stable ICU 53
     */
    MeasureFormat(
            const Locale &locale,
            UMeasureFormatWidth width,
            NumberFormat *nfToAdopt,
            UErrorCode &status);

    /**
     * Copy constructor.
     * @stable ICU 3.0
     */
    MeasureFormat(const MeasureFormat &other);

    /**
     * Assignment operator.
     * @stable ICU 3.0
     */
    MeasureFormat &operator=(const MeasureFormat &rhs);

    /**
     * Destructor.
     * @stable ICU 3.0
     */
    virtual ~MeasureFormat();

    /**
     * Return true if given Format objects are semantically equal.
     * @stable ICU 53
     */
    virtual UBool operator==(const Format &other) const;

    /**
     * Clones this object polymorphically.
     * @stable ICU 53
     */
    virtual Format *clone() const;

    /**
     * Formats object to produce a string.
     * @stable ICU 53
     */
    virtual UnicodeString &format(
            const Formattable &obj,
            UnicodeString &appendTo,
            FieldPosition &pos,
            UErrorCode &status) const;

    /**
     * Parse a string to produce an object. This implementation sets
     * status to U_UNSUPPORTED_ERROR.
     *
     * @draft ICU 53
     */
    virtual void parseObject(
            const UnicodeString &source,
            Formattable &reslt,
            ParsePosition &pos) const;

    /**
     * Formats measure objects to produce a string. An example of such a
     * formatted string is 3 meters, 3.5 centimeters. Measure objects appear
     * in the formatted string in the same order they appear in the "measures"
     * array. The NumberFormat of this object is used only to format the amount
     * of the very last measure. The other amounts are formatted with zero
     * decimal places while rounding toward zero.
     * @param measures array of measure objects.
     * @param measureCount the number of measure objects.
     * @param appendTo formatted string appended here.
     * @param pos the field position.
     * @param status the error.
     * @return appendTo reference
     *
     * @stable ICU 53
     */
    UnicodeString &formatMeasures(
            const Measure *measures,
            int32_t measureCount,
            UnicodeString &appendTo,
            FieldPosition &pos,
            UErrorCode &status) const;

    /**
     * Formats a single measure per unit. An example of such a
     * formatted string is 3.5 meters per second.
     * @param measure The measure object. In above example, 3.5 meters.
     * @param perUnit The per unit. In above example, it is
     *        *MeasureUnit::createSecond(status).
     * @param appendTo formatted string appended here.
     * @param pos the field position.
     * @param status the error.
     * @return appendTo reference
     *
     * @stable ICU 55
     */
    UnicodeString &formatMeasurePerUnit(
            const Measure &measure,
            const MeasureUnit &perUnit,
            UnicodeString &appendTo,
            FieldPosition &pos,
            UErrorCode &status) const;


    /**
     * Return a formatter for CurrencyAmount objects in the given
     * locale.
     * @param locale desired locale
     * @param ec input-output error code
     * @return a formatter object, or NULL upon error
     * @stable ICU 3.0
     */
    static MeasureFormat* U_EXPORT2 createCurrencyFormat(const Locale& locale,
                                               UErrorCode& ec);

    /**
     * Return a formatter for CurrencyAmount objects in the default
     * locale.
     * @param ec input-output error code
     * @return a formatter object, or NULL upon error
     * @stable ICU 3.0
     */
    static MeasureFormat* U_EXPORT2 createCurrencyFormat(UErrorCode& ec);

    /**
     * Return the class ID for this class. This is useful only for comparing to
     * a return value from getDynamicClassID(). For example:
     * <pre>
     * .   Base* polymorphic_pointer = createPolymorphicObject();
     * .   if (polymorphic_pointer->getDynamicClassID() ==
     * .       erived::getStaticClassID()) ...
     * </pre>
     * @return          The class ID for all objects of this class.
     * @stable ICU 53
     */
    static UClassID U_EXPORT2 getStaticClassID(void);

    /**
     * Returns a unique class ID POLYMORPHICALLY. Pure virtual override. This
     * method is to implement a simple version of RTTI, since not all C++
     * compilers support genuine RTTI. Polymorphic operator==() and clone()
     * methods call this method.
     *
     * @return          The class ID for this object. All objects of a
     *                  given class have the same class ID.  Objects of
     *                  other classes have different class IDs.
     * @stable ICU 53
     */
    virtual UClassID getDynamicClassID(void) const;

 protected:
    /**
     * Default constructor.
     * @stable ICU 3.0
     */
    MeasureFormat();

#ifndef U_HIDE_INTERNAL_API 

    /**
     * ICU use only.
     * Initialize or change MeasureFormat class from subclass.
     * @internal.
     */
    void initMeasureFormat(
            const Locale &locale,
            UMeasureFormatWidth width,
            NumberFormat *nfToAdopt,
            UErrorCode &status);
    /**
     * ICU use only.
     * Allows subclass to change locale. Note that this method also changes
     * the NumberFormat object. Returns TRUE if locale changed; FALSE if no
     * change was made.
     * @internal.
     */
    UBool setMeasureFormatLocale(const Locale &locale, UErrorCode &status);

    /**
     * ICU use only.
     * Let subclass change NumberFormat.
     * @internal.
     */
    void adoptNumberFormat(NumberFormat *nfToAdopt, UErrorCode &status);

    /**
     * ICU use only.
     * @internal.
     */
    const NumberFormat &getNumberFormat() const;

    /**
     * ICU use only.
     * @internal.
     */
    const PluralRules &getPluralRules() const;

    /**
     * ICU use only.
     * @internal.
     */
    Locale getLocale(UErrorCode &status) const;

    /**
     * ICU use only.
     * @internal.
     */
    const char *getLocaleID(UErrorCode &status) const;

#endif /* U_HIDE_INTERNAL_API */

 private:
    const MeasureFormatCacheData *cache;
    const SharedNumberFormat *numberFormat;
    const SharedPluralRules *pluralRules;
    UMeasureFormatWidth width;    

    // Declared outside of MeasureFormatSharedData because ListFormatter
    // objects are relatively cheap to copy; therefore, they don't need to be
    // shared across instances.
    ListFormatter *listFormatter;

    const SimpleFormatter *getFormatterOrNull(
            const MeasureUnit &unit, UMeasureFormatWidth width, int32_t index) const;

    const SimpleFormatter *getFormatter(
            const MeasureUnit &unit, UMeasureFormatWidth width, int32_t index,
            UErrorCode &errorCode) const;

    const SimpleFormatter *getPluralFormatter(
            const MeasureUnit &unit, UMeasureFormatWidth width, int32_t index,
            UErrorCode &errorCode) const;

    const SimpleFormatter *getPerFormatter(
            UMeasureFormatWidth width,
            UErrorCode &status) const;

    int32_t withPerUnitAndAppend(
        const UnicodeString &formatted,
        const MeasureUnit &perUnit,
        UnicodeString &appendTo,
        UErrorCode &status) const;

    UnicodeString &formatMeasure(
        const Measure &measure,
        const NumberFormat &nf,
        UnicodeString &appendTo,
        FieldPosition &pos,
        UErrorCode &status) const;

    UnicodeString &formatMeasuresSlowTrack(
        const Measure *measures,
        int32_t measureCount,
        UnicodeString& appendTo,
        FieldPosition& pos,
        UErrorCode& status) const;

    UnicodeString &formatNumeric(
        const Formattable *hms,  // always length 3: [0] is hour; [1] is
                                 // minute; [2] is second.
        int32_t bitMap,   // 1=hour set, 2=minute set, 4=second set
        UnicodeString &appendTo,
        UErrorCode &status) const;

    UnicodeString &formatNumeric(
        UDate date,
        const DateFormat &dateFmt,
        UDateFormatField smallestField,
        const Formattable &smallestAmount,
        UnicodeString &appendTo,
        UErrorCode &status) const;
};

U_NAMESPACE_END

#endif // #if !UCONFIG_NO_FORMATTING
#endif // #ifndef MEASUREFORMAT_H
