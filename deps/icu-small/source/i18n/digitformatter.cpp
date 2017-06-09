// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
 * Copyright (C) 2015, International Business Machines
 * Corporation and others.  All Rights Reserved.
 *
 * file name: digitformatter.cpp
 */

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/dcfmtsym.h"
#include "unicode/unum.h"

#include "digitformatter.h"
#include "digitgrouping.h"
#include "digitinterval.h"
#include "digitlst.h"
#include "fphdlimp.h"
#include "smallintformatter.h"
#include "unistrappender.h"
#include "visibledigits.h"

U_NAMESPACE_BEGIN

DigitFormatter::DigitFormatter()
        : fGroupingSeparator(",", -1, US_INV), fDecimal(".", -1, US_INV),
          fNegativeSign("-", -1, US_INV), fPositiveSign("+", -1, US_INV),
          fIsStandardDigits(TRUE), fExponent("E", -1, US_INV) {
    for (int32_t i = 0; i < 10; ++i) {
        fLocalizedDigits[i] = (UChar32) (0x30 + i);
    }
    fInfinity.setTo(UnicodeString("Inf", -1, US_INV), UNUM_INTEGER_FIELD);
    fNan.setTo(UnicodeString("Nan", -1, US_INV), UNUM_INTEGER_FIELD);
}

DigitFormatter::DigitFormatter(const DecimalFormatSymbols &symbols) {
    setDecimalFormatSymbols(symbols);
}

void
DigitFormatter::setOtherDecimalFormatSymbols(
        const DecimalFormatSymbols &symbols) {
    fLocalizedDigits[0] = symbols.getConstSymbol(DecimalFormatSymbols::kZeroDigitSymbol).char32At(0);
    fLocalizedDigits[1] = symbols.getConstSymbol(DecimalFormatSymbols::kOneDigitSymbol).char32At(0);
    fLocalizedDigits[2] = symbols.getConstSymbol(DecimalFormatSymbols::kTwoDigitSymbol).char32At(0);
    fLocalizedDigits[3] = symbols.getConstSymbol(DecimalFormatSymbols::kThreeDigitSymbol).char32At(0);
    fLocalizedDigits[4] = symbols.getConstSymbol(DecimalFormatSymbols::kFourDigitSymbol).char32At(0);
    fLocalizedDigits[5] = symbols.getConstSymbol(DecimalFormatSymbols::kFiveDigitSymbol).char32At(0);
    fLocalizedDigits[6] = symbols.getConstSymbol(DecimalFormatSymbols::kSixDigitSymbol).char32At(0);
    fLocalizedDigits[7] = symbols.getConstSymbol(DecimalFormatSymbols::kSevenDigitSymbol).char32At(0);
    fLocalizedDigits[8] = symbols.getConstSymbol(DecimalFormatSymbols::kEightDigitSymbol).char32At(0);
    fLocalizedDigits[9] = symbols.getConstSymbol(DecimalFormatSymbols::kNineDigitSymbol).char32At(0);
    fIsStandardDigits = isStandardDigits();
    fNegativeSign = symbols.getConstSymbol(DecimalFormatSymbols::kMinusSignSymbol);
    fPositiveSign = symbols.getConstSymbol(DecimalFormatSymbols::kPlusSignSymbol);
    fInfinity.setTo(symbols.getConstSymbol(DecimalFormatSymbols::kInfinitySymbol), UNUM_INTEGER_FIELD);
    fNan.setTo(symbols.getConstSymbol(DecimalFormatSymbols::kNaNSymbol), UNUM_INTEGER_FIELD);
    fExponent = symbols.getConstSymbol(DecimalFormatSymbols::kExponentialSymbol);
}

void
DigitFormatter::setDecimalFormatSymbolsForMonetary(
        const DecimalFormatSymbols &symbols) {
    setOtherDecimalFormatSymbols(symbols);
    fGroupingSeparator = symbols.getConstSymbol(DecimalFormatSymbols::kMonetaryGroupingSeparatorSymbol);
    fDecimal = symbols.getConstSymbol(DecimalFormatSymbols::kMonetarySeparatorSymbol);
}

void
DigitFormatter::setDecimalFormatSymbols(
        const DecimalFormatSymbols &symbols) {
    setOtherDecimalFormatSymbols(symbols);
    fGroupingSeparator = symbols.getConstSymbol(DecimalFormatSymbols::kGroupingSeparatorSymbol);
    fDecimal = symbols.getConstSymbol(DecimalFormatSymbols::kDecimalSeparatorSymbol);
}

static void appendField(
        int32_t fieldId,
        const UnicodeString &value,
        FieldPositionHandler &handler,
        UnicodeString &appendTo) {
    int32_t currentLength = appendTo.length();
    appendTo.append(value);
    handler.addAttribute(
            fieldId,
            currentLength,
            appendTo.length());
}

int32_t DigitFormatter::countChar32(
        const DigitGrouping &grouping,
        const DigitInterval &interval,
        const DigitFormatterOptions &options) const {
    int32_t result = interval.length();

    // We always emit '0' in lieu of no digits.
    if (result == 0) {
        result = 1;
    }
    if (options.fAlwaysShowDecimal || interval.getLeastSignificantInclusive() < 0) {
        result += fDecimal.countChar32();
    }
    result += grouping.getSeparatorCount(interval.getIntDigitCount()) * fGroupingSeparator.countChar32();
    return result;
}

int32_t
DigitFormatter::countChar32(
        const VisibleDigits &digits,
        const DigitGrouping &grouping,
        const DigitFormatterOptions &options) const {
    if (digits.isNaN()) {
        return countChar32ForNaN();
    }
    if (digits.isInfinite()) {
        return countChar32ForInfinity();
    }
    return countChar32(
            grouping,
            digits.getInterval(),
            options);
}

int32_t
DigitFormatter::countChar32(
        const VisibleDigitsWithExponent &digits,
        const SciFormatterOptions &options) const {
    if (digits.isNaN()) {
        return countChar32ForNaN();
    }
    if (digits.isInfinite()) {
        return countChar32ForInfinity();
    }
    const VisibleDigits *exponent = digits.getExponent();
    if (exponent == NULL) {
        DigitGrouping grouping;
        return countChar32(
                grouping,
                digits.getMantissa().getInterval(),
                options.fMantissa);
    }
    return countChar32(
            *exponent, digits.getMantissa().getInterval(), options);
}

int32_t
DigitFormatter::countChar32(
        const VisibleDigits &exponent,
        const DigitInterval &mantissaInterval,
        const SciFormatterOptions &options) const {
    DigitGrouping grouping;
    int32_t count = countChar32(
            grouping, mantissaInterval, options.fMantissa);
    count += fExponent.countChar32();
    count += countChar32ForExponent(
            exponent, options.fExponent);
    return count;
}

UnicodeString &DigitFormatter::format(
        const VisibleDigits &digits,
        const DigitGrouping &grouping,
        const DigitFormatterOptions &options,
        FieldPositionHandler &handler,
        UnicodeString &appendTo) const {
    if (digits.isNaN()) {
        return formatNaN(handler, appendTo);
    }
    if (digits.isInfinite()) {
        return formatInfinity(handler, appendTo);
    }

    const DigitInterval &interval = digits.getInterval();
    int32_t digitsLeftOfDecimal = interval.getMostSignificantExclusive();
    int32_t lastDigitPos = interval.getLeastSignificantInclusive();
    int32_t intBegin = appendTo.length();
    int32_t fracBegin;

    // Emit "0" instead of empty string.
    if (digitsLeftOfDecimal == 0 && lastDigitPos == 0) {
        appendTo.append(fLocalizedDigits[0]);
        handler.addAttribute(UNUM_INTEGER_FIELD, intBegin, appendTo.length());
        if (options.fAlwaysShowDecimal) {
            appendField(
                    UNUM_DECIMAL_SEPARATOR_FIELD,
                    fDecimal,
                    handler,
                    appendTo);
        }
        return appendTo;
    }
    {
        UnicodeStringAppender appender(appendTo);
        for (int32_t i = interval.getMostSignificantExclusive() - 1;
                i >= interval.getLeastSignificantInclusive(); --i) {
            if (i == -1) {
                appender.flush();
                appendField(
                        UNUM_DECIMAL_SEPARATOR_FIELD,
                        fDecimal,
                        handler,
                        appendTo);
                fracBegin = appendTo.length();
            }
            appender.append(fLocalizedDigits[digits.getDigitByExponent(i)]);
            if (grouping.isSeparatorAt(digitsLeftOfDecimal, i)) {
                appender.flush();
                appendField(
                        UNUM_GROUPING_SEPARATOR_FIELD,
                        fGroupingSeparator,
                        handler,
                        appendTo);
            }
            if (i == 0) {
                appender.flush();
                if (digitsLeftOfDecimal > 0) {
                    handler.addAttribute(UNUM_INTEGER_FIELD, intBegin, appendTo.length());
                }
            }
        }
        if (options.fAlwaysShowDecimal && lastDigitPos == 0) {
            appender.flush();
            appendField(
                    UNUM_DECIMAL_SEPARATOR_FIELD,
                    fDecimal,
                    handler,
                    appendTo);
        }
    }
    // lastDigitPos is never > 0 so we are guaranteed that kIntegerField
    // is already added.
    if (lastDigitPos < 0) {
        handler.addAttribute(UNUM_FRACTION_FIELD, fracBegin, appendTo.length());
    }
    return appendTo;
}

UnicodeString &
DigitFormatter::format(
        const VisibleDigitsWithExponent &digits,
        const SciFormatterOptions &options,
        FieldPositionHandler &handler,
        UnicodeString &appendTo) const {
    DigitGrouping grouping;
    format(
            digits.getMantissa(),
            grouping,
            options.fMantissa,
            handler,
            appendTo);
    const VisibleDigits *exponent = digits.getExponent();
    if (exponent == NULL) {
        return appendTo;
    }
    int32_t expBegin = appendTo.length();
    appendTo.append(fExponent);
    handler.addAttribute(
            UNUM_EXPONENT_SYMBOL_FIELD, expBegin, appendTo.length());
    return formatExponent(
            *exponent,
            options.fExponent,
            UNUM_EXPONENT_SIGN_FIELD,
            UNUM_EXPONENT_FIELD,
            handler,
            appendTo);
}

static int32_t formatInt(
        int32_t value, uint8_t *digits) {
    int32_t idx = 0;
    while (value > 0) {
        digits[idx++] = (uint8_t) (value % 10);
        value /= 10;
    }
    return idx;
}

UnicodeString &
DigitFormatter::formatDigits(
        const uint8_t *digits,
        int32_t count,
        const IntDigitCountRange &range,
        int32_t intField,
        FieldPositionHandler &handler,
        UnicodeString &appendTo) const {
    int32_t i = range.pin(count) - 1;
    int32_t begin = appendTo.length();

    // Always emit '0' as placeholder for empty string.
    if (i == -1) {
        appendTo.append(fLocalizedDigits[0]);
        handler.addAttribute(intField, begin, appendTo.length());
        return appendTo;
    }
    {
        UnicodeStringAppender appender(appendTo);
        for (; i >= count; --i) {
            appender.append(fLocalizedDigits[0]);
        }
        for (; i >= 0; --i) {
            appender.append(fLocalizedDigits[digits[i]]);
        }
    }
    handler.addAttribute(intField, begin, appendTo.length());
    return appendTo;
}

UnicodeString &
DigitFormatter::formatExponent(
        const VisibleDigits &digits,
        const DigitFormatterIntOptions &options,
        int32_t signField,
        int32_t intField,
        FieldPositionHandler &handler,
        UnicodeString &appendTo) const {
    UBool neg = digits.isNegative();
    if (neg || options.fAlwaysShowSign) {
        appendField(
                signField,
                neg ? fNegativeSign : fPositiveSign,
                handler,
                appendTo);
    }
    int32_t begin = appendTo.length();
    DigitGrouping grouping;
    DigitFormatterOptions expOptions;
    FieldPosition fpos(FieldPosition::DONT_CARE);
    FieldPositionOnlyHandler noHandler(fpos);
    format(
            digits,
            grouping,
            expOptions,
            noHandler,
            appendTo);
    handler.addAttribute(intField, begin, appendTo.length());
    return appendTo;
}

int32_t
DigitFormatter::countChar32ForExponent(
        const VisibleDigits &exponent,
        const DigitFormatterIntOptions &options) const {
    int32_t result = 0;
    UBool neg = exponent.isNegative();
    if (neg || options.fAlwaysShowSign) {
        result += neg ? fNegativeSign.countChar32() : fPositiveSign.countChar32();
    }
    DigitGrouping grouping;
    DigitFormatterOptions expOptions;
    result += countChar32(grouping, exponent.getInterval(), expOptions);
    return result;
}

UnicodeString &
DigitFormatter::formatPositiveInt32(
        int32_t positiveValue,
        const IntDigitCountRange &range,
        FieldPositionHandler &handler,
        UnicodeString &appendTo) const {
    // super fast path
    if (fIsStandardDigits && SmallIntFormatter::canFormat(positiveValue, range)) {
        int32_t begin = appendTo.length();
        SmallIntFormatter::format(positiveValue, range, appendTo);
        handler.addAttribute(UNUM_INTEGER_FIELD, begin, appendTo.length());
        return appendTo;
    }
    uint8_t digits[10];
    int32_t count = formatInt(positiveValue, digits);
    return formatDigits(
            digits,
            count,
            range,
            UNUM_INTEGER_FIELD,
            handler,
            appendTo);
}

UBool DigitFormatter::isStandardDigits() const {
    UChar32 cdigit = 0x30;
    for (int32_t i = 0; i < UPRV_LENGTHOF(fLocalizedDigits); ++i) {
        if (fLocalizedDigits[i] != cdigit) {
            return FALSE;
        }
        ++cdigit;
    }
    return TRUE;
}

UBool
DigitFormatter::equals(const DigitFormatter &rhs) const {
    UBool result = (fGroupingSeparator == rhs.fGroupingSeparator) &&
                   (fDecimal == rhs.fDecimal) &&
                   (fNegativeSign == rhs.fNegativeSign) &&
                   (fPositiveSign == rhs.fPositiveSign) &&
                   (fInfinity.equals(rhs.fInfinity)) &&
                   (fNan.equals(rhs.fNan)) &&
                   (fIsStandardDigits == rhs.fIsStandardDigits) &&
                   (fExponent == rhs.fExponent);

    if (!result) {
        return FALSE;
    }
    for (int32_t i = 0; i < UPRV_LENGTHOF(fLocalizedDigits); ++i) {
        if (fLocalizedDigits[i] != rhs.fLocalizedDigits[i]) {
            return FALSE;
        }
    }
    return TRUE;
}


U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */
