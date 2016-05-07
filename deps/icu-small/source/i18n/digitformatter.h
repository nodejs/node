/*
*******************************************************************************
* Copyright (C) 2015, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* digitformatter.h
*
* created on: 2015jan06
* created by: Travis Keep
*/

#ifndef __DIGITFORMATTER_H__
#define __DIGITFORMATTER_H__

#include "unicode/uobject.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/utypes.h"
#include "unicode/unistr.h"
#include "digitaffix.h"

U_NAMESPACE_BEGIN

class DecimalFormatSymbols;
class DigitList;
class DigitGrouping;
class DigitInterval;
class UnicodeString;
class FieldPositionHandler;
class IntDigitCountRange;
class VisibleDigits;
class VisibleDigitsWithExponent;

/**
 * Various options for formatting in fixed point.
 */
class U_I18N_API DigitFormatterOptions : public UMemory {
    public:
    DigitFormatterOptions() : fAlwaysShowDecimal(FALSE) { }

    /**
     * Returns TRUE if this object equals rhs.
     */
    UBool equals(const DigitFormatterOptions &rhs) const {
        return (
            fAlwaysShowDecimal == rhs.fAlwaysShowDecimal);
    }

    /**
     * Returns TRUE if these options allow for fast formatting of
     * integers.
     */
    UBool isFastFormattable() const {
        return (fAlwaysShowDecimal == FALSE);
    }

    /**
     * If TRUE, show the decimal separator even when there are no fraction
     * digits. default is FALSE.
     */
    UBool fAlwaysShowDecimal;
};

/**
 * Various options for formatting an integer.
 */
class U_I18N_API DigitFormatterIntOptions : public UMemory {
    public:
    DigitFormatterIntOptions() : fAlwaysShowSign(FALSE) { }

    /**
     * Returns TRUE if this object equals rhs.
     */
    UBool equals(const DigitFormatterIntOptions &rhs) const {
        return (fAlwaysShowSign == rhs.fAlwaysShowSign);
    }

    /**
     * If TRUE, always prefix the integer with its sign even if the number is
     * positive. Default is FALSE.
     */
    UBool fAlwaysShowSign;
};

/**
 * Options for formatting in scientific notation.
 */
class U_I18N_API SciFormatterOptions : public UMemory {
    public:

    /**
     * Returns TRUE if this object equals rhs.
     */
    UBool equals(const SciFormatterOptions &rhs) const {
        return (fMantissa.equals(rhs.fMantissa) &&
                fExponent.equals(rhs.fExponent));
    }

    /**
     * Options for formatting the mantissa.
     */
    DigitFormatterOptions fMantissa;

    /**
     * Options for formatting the exponent.
     */
    DigitFormatterIntOptions fExponent;
};


/**
 * Does fixed point formatting.
 *
 * This class only does fixed point formatting. It does no rounding before
 * formatting.
 */
class U_I18N_API DigitFormatter : public UMemory {
public:

/**
 * Decimal separator is period (.), Plus sign is plus (+),
 * minus sign is minus (-), grouping separator is comma (,), digits are 0-9.
 */
DigitFormatter();

/**
 * Let symbols determine the digits, decimal separator,
 * plus and mius sign, grouping separator, and possibly other settings.
 */
DigitFormatter(const DecimalFormatSymbols &symbols);

/**
 * Change what this instance uses for digits, decimal separator,
 * plus and mius sign, grouping separator, and possibly other settings
 * according to symbols.
 */
void setDecimalFormatSymbols(const DecimalFormatSymbols &symbols);

/**
 * Change what this instance uses for digits, decimal separator,
 * plus and mius sign, grouping separator, and possibly other settings
 * according to symbols in the context of monetary amounts.
 */
void setDecimalFormatSymbolsForMonetary(const DecimalFormatSymbols &symbols);

/**
 * Fixed point formatting.
 *
 * @param positiveDigits the value to format
 *  Negative sign can be present, but it won't show.
 * @param grouping controls how digit grouping is done
 * @param options formatting options
 * @param handler records field positions
 * @param appendTo formatted value appended here.
 * @return appendTo
 */
UnicodeString &format(
        const VisibleDigits &positiveDigits,
        const DigitGrouping &grouping,
        const DigitFormatterOptions &options,
        FieldPositionHandler &handler,
        UnicodeString &appendTo) const;

/**
 * formats in scientifc notation.
 * @param positiveDigits the value to format.
 *  Negative sign can be present, but it won't show.
 * @param options formatting options
 * @param handler records field positions.
 * @param appendTo formatted value appended here.
 */
UnicodeString &format(
        const VisibleDigitsWithExponent &positiveDigits,
        const SciFormatterOptions &options,
        FieldPositionHandler &handler,
        UnicodeString &appendTo) const;

/**
 * Fixed point formatting of integers.
 * Always performed with no grouping and no decimal point.
 *
 * @param positiveValue the value to format must be positive.
 * @param range specifies minimum and maximum number of digits.
 * @param handler records field positions
 * @param appendTo formatted value appended here.
 * @return appendTo
 */
UnicodeString &formatPositiveInt32(
        int32_t positiveValue,
        const IntDigitCountRange &range,
        FieldPositionHandler &handler,
        UnicodeString &appendTo) const;

/**
 * Counts how many code points are needed for fixed formatting.
 *   If digits is negative, the negative sign is not included in the count.
 */
int32_t countChar32(
        const VisibleDigits &digits,
        const DigitGrouping &grouping,
        const DigitFormatterOptions &options) const;

/**
 * Counts how many code points are needed for scientific formatting.
 *   If digits is negative, the negative sign is not included in the count.
 */
int32_t countChar32(
        const VisibleDigitsWithExponent &digits,
        const SciFormatterOptions &options) const;

/**
 * Returns TRUE if this object equals rhs.
 */
UBool equals(const DigitFormatter &rhs) const;

private:
UChar32 fLocalizedDigits[10];
UnicodeString fGroupingSeparator;
UnicodeString fDecimal;
UnicodeString fNegativeSign;
UnicodeString fPositiveSign;
DigitAffix fInfinity;
DigitAffix fNan;
UBool fIsStandardDigits;
UnicodeString fExponent;
UBool isStandardDigits() const;

UnicodeString &formatDigits(
        const uint8_t *digits,
        int32_t count,
        const IntDigitCountRange &range,
        int32_t intField,
        FieldPositionHandler &handler,
        UnicodeString &appendTo) const;

void setOtherDecimalFormatSymbols(const DecimalFormatSymbols &symbols);

int32_t countChar32(
        const VisibleDigits &exponent,
        const DigitInterval &mantissaInterval,
        const SciFormatterOptions &options) const;

UnicodeString &formatNaN(
        FieldPositionHandler &handler,
        UnicodeString &appendTo) const {
    return fNan.format(handler, appendTo);
}

int32_t countChar32ForNaN() const {
    return fNan.toString().countChar32();
}

UnicodeString &formatInfinity(
        FieldPositionHandler &handler,
        UnicodeString &appendTo) const {
    return fInfinity.format(handler, appendTo);
}

int32_t countChar32ForInfinity() const {
    return fInfinity.toString().countChar32();
}

UnicodeString &formatExponent(
        const VisibleDigits &digits,
        const DigitFormatterIntOptions &options,
        int32_t signField,
        int32_t intField,
        FieldPositionHandler &handler,
        UnicodeString &appendTo) const;

int32_t countChar32(
        const DigitGrouping &grouping,
        const DigitInterval &interval,
        const DigitFormatterOptions &options) const;

int32_t countChar32ForExponent(
        const VisibleDigits &exponent,
        const DigitFormatterIntOptions &options) const;

};


U_NAMESPACE_END
#endif /* #if !UCONFIG_NO_FORMATTING */
#endif  // __DIGITFORMATTER_H__
