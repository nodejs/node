// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
 * Copyright (C) 2015, International Business Machines
 * Corporation and others.  All Rights Reserved.
 *
 * file name: decimfmtimpl.cpp
 */

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include <math.h>
#include "unicode/numfmt.h"
#include "unicode/plurrule.h"
#include "unicode/ustring.h"
#include "decimalformatpattern.h"
#include "decimalformatpatternimpl.h"
#include "decimfmtimpl.h"
#include "fphdlimp.h"
#include "plurrule_impl.h"
#include "valueformatter.h"
#include "visibledigits.h"

U_NAMESPACE_BEGIN

static const int32_t kMaxScientificIntegerDigits = 8;

static const int32_t kFormattingPosPrefix = (1 << 0);
static const int32_t kFormattingNegPrefix = (1 << 1);
static const int32_t kFormattingPosSuffix = (1 << 2);
static const int32_t kFormattingNegSuffix = (1 << 3);
static const int32_t kFormattingSymbols = (1 << 4);
static const int32_t kFormattingCurrency = (1 << 5);
static const int32_t kFormattingUsesCurrency = (1 << 6);
static const int32_t kFormattingPluralRules = (1 << 7);
static const int32_t kFormattingAffixParser = (1 << 8);
static const int32_t kFormattingCurrencyAffixInfo = (1 << 9);
static const int32_t kFormattingAll = (1 << 10) - 1;
static const int32_t kFormattingAffixes =
        kFormattingPosPrefix | kFormattingPosSuffix |
        kFormattingNegPrefix | kFormattingNegSuffix;
static const int32_t kFormattingAffixParserWithCurrency =
        kFormattingAffixParser | kFormattingCurrencyAffixInfo;

DecimalFormatImpl::DecimalFormatImpl(
        NumberFormat *super,
        const Locale &locale,
        const UnicodeString &pattern,
        UErrorCode &status)
        : fSuper(super),
          fScale(0),
          fRoundingMode(DecimalFormat::kRoundHalfEven),
          fSymbols(NULL),
          fCurrencyUsage(UCURR_USAGE_STANDARD),
          fRules(NULL),
          fMonetary(FALSE) {
    if (U_FAILURE(status)) {
        return;
    }
    fSymbols = new DecimalFormatSymbols(
            locale, status);
    if (fSymbols == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    UParseError parseError;
    applyPattern(pattern, FALSE, parseError, status);
    updateAll(status);
}

DecimalFormatImpl::DecimalFormatImpl(
        NumberFormat *super,
        const UnicodeString &pattern,
        DecimalFormatSymbols *symbolsToAdopt,
        UParseError &parseError,
        UErrorCode &status)
        : fSuper(super),
          fScale(0),
          fRoundingMode(DecimalFormat::kRoundHalfEven),
          fSymbols(symbolsToAdopt),
          fCurrencyUsage(UCURR_USAGE_STANDARD),
          fRules(NULL),
          fMonetary(FALSE) {
    applyPattern(pattern, FALSE, parseError, status);
    updateAll(status);
}

DecimalFormatImpl::DecimalFormatImpl(
    NumberFormat *super, const DecimalFormatImpl &other, UErrorCode &status) :
          fSuper(super),
          fMultiplier(other.fMultiplier),
          fScale(other.fScale),
          fRoundingMode(other.fRoundingMode),
          fMinSigDigits(other.fMinSigDigits),
          fMaxSigDigits(other.fMaxSigDigits),
          fUseScientific(other.fUseScientific),
          fUseSigDigits(other.fUseSigDigits),
          fGrouping(other.fGrouping),
          fPositivePrefixPattern(other.fPositivePrefixPattern),
          fNegativePrefixPattern(other.fNegativePrefixPattern),
          fPositiveSuffixPattern(other.fPositiveSuffixPattern),
          fNegativeSuffixPattern(other.fNegativeSuffixPattern),
          fSymbols(other.fSymbols),
          fCurrencyUsage(other.fCurrencyUsage),
          fRules(NULL),
          fMonetary(other.fMonetary),
          fAffixParser(other.fAffixParser),
          fCurrencyAffixInfo(other.fCurrencyAffixInfo),
          fEffPrecision(other.fEffPrecision),
          fEffGrouping(other.fEffGrouping),
          fOptions(other.fOptions),
          fFormatter(other.fFormatter),
          fAffixes(other.fAffixes) {
    fSymbols = new DecimalFormatSymbols(*fSymbols);
    if (fSymbols == NULL && U_SUCCESS(status)) {
        status = U_MEMORY_ALLOCATION_ERROR;
    }
    if (other.fRules != NULL) {
        fRules = new PluralRules(*other.fRules);
        if (fRules == NULL && U_SUCCESS(status)) {
            status = U_MEMORY_ALLOCATION_ERROR;
        }
    }
}


DecimalFormatImpl &
DecimalFormatImpl::assign(const DecimalFormatImpl &other, UErrorCode &status) {
    if (U_FAILURE(status) || this == &other) {
        return (*this);
    }
    UObject::operator=(other);
    fMultiplier = other.fMultiplier;
    fScale = other.fScale;
    fRoundingMode = other.fRoundingMode;
    fMinSigDigits = other.fMinSigDigits;
    fMaxSigDigits = other.fMaxSigDigits;
    fUseScientific = other.fUseScientific;
    fUseSigDigits = other.fUseSigDigits;
    fGrouping = other.fGrouping;
    fPositivePrefixPattern = other.fPositivePrefixPattern;
    fNegativePrefixPattern = other.fNegativePrefixPattern;
    fPositiveSuffixPattern = other.fPositiveSuffixPattern;
    fNegativeSuffixPattern = other.fNegativeSuffixPattern;
    fCurrencyUsage = other.fCurrencyUsage;
    fMonetary = other.fMonetary;
    fAffixParser = other.fAffixParser;
    fCurrencyAffixInfo = other.fCurrencyAffixInfo;
    fEffPrecision = other.fEffPrecision;
    fEffGrouping = other.fEffGrouping;
    fOptions = other.fOptions;
    fFormatter = other.fFormatter;
    fAffixes = other.fAffixes;
    *fSymbols = *other.fSymbols;
    if (fRules != NULL && other.fRules != NULL) {
        *fRules = *other.fRules;
    } else {
        delete fRules;
        fRules = other.fRules;
        if (fRules != NULL) {
            fRules = new PluralRules(*fRules);
            if (fRules == NULL) {
                status = U_MEMORY_ALLOCATION_ERROR;
                return *this;
            }
        }
    }
    return *this;
}

UBool
DecimalFormatImpl::operator==(const DecimalFormatImpl &other) const {
    if (this == &other) {
        return TRUE;
    }
    return (fMultiplier == other.fMultiplier)
            && (fScale == other.fScale)
            && (fRoundingMode == other.fRoundingMode)
            && (fMinSigDigits == other.fMinSigDigits)
            && (fMaxSigDigits == other.fMaxSigDigits)
            && (fUseScientific == other.fUseScientific)
            && (fUseSigDigits == other.fUseSigDigits)
            && fGrouping.equals(other.fGrouping)
            && fPositivePrefixPattern.equals(other.fPositivePrefixPattern)
            && fNegativePrefixPattern.equals(other.fNegativePrefixPattern)
            && fPositiveSuffixPattern.equals(other.fPositiveSuffixPattern)
            && fNegativeSuffixPattern.equals(other.fNegativeSuffixPattern)
            && fCurrencyUsage == other.fCurrencyUsage
            && fAffixParser.equals(other.fAffixParser)
            && fCurrencyAffixInfo.equals(other.fCurrencyAffixInfo)
            && fEffPrecision.equals(other.fEffPrecision)
            && fEffGrouping.equals(other.fEffGrouping)
            && fOptions.equals(other.fOptions)
            && fFormatter.equals(other.fFormatter)
            && fAffixes.equals(other.fAffixes)
            && (*fSymbols == *other.fSymbols)
            && ((fRules == other.fRules) || (
                    (fRules != NULL) && (other.fRules != NULL)
                    && (*fRules == *other.fRules)))
            && (fMonetary == other.fMonetary);
}

DecimalFormatImpl::~DecimalFormatImpl() {
    delete fSymbols;
    delete fRules;
}

ValueFormatter &
DecimalFormatImpl::prepareValueFormatter(ValueFormatter &vf) const {
    if (fUseScientific) {
        vf.prepareScientificFormatting(
                fFormatter, fEffPrecision, fOptions);
        return vf;
    }
    vf.prepareFixedDecimalFormatting(
            fFormatter, fEffGrouping, fEffPrecision.fMantissa, fOptions.fMantissa);
    return vf;
}

int32_t
DecimalFormatImpl::getPatternScale() const {
    UBool usesPercent = fPositivePrefixPattern.usesPercent() ||
            fPositiveSuffixPattern.usesPercent() ||
            fNegativePrefixPattern.usesPercent() ||
            fNegativeSuffixPattern.usesPercent();
    if (usesPercent) {
        return 2;
    }
    UBool usesPermill = fPositivePrefixPattern.usesPermill() ||
            fPositiveSuffixPattern.usesPermill() ||
            fNegativePrefixPattern.usesPermill() ||
            fNegativeSuffixPattern.usesPermill();
    if (usesPermill) {
        return 3;
    }
    return 0;
}

void
DecimalFormatImpl::setMultiplierScale(int32_t scale) {
    if (scale == 0) {
        // Needed to preserve equality. fMultiplier == 0 means
        // multiplier is 1.
        fMultiplier.set((int32_t)0);
    } else {
        fMultiplier.set((int32_t)1);
        fMultiplier.shiftDecimalRight(scale);
    }
}

UnicodeString &
DecimalFormatImpl::format(
        int32_t number,
        UnicodeString &appendTo,
        FieldPosition &pos,
        UErrorCode &status) const {
    FieldPositionOnlyHandler handler(pos);
    return formatInt32(number, appendTo, handler, status);
}

UnicodeString &
DecimalFormatImpl::format(
        int32_t number,
        UnicodeString &appendTo,
        FieldPositionIterator *posIter,
        UErrorCode &status) const {
    FieldPositionIteratorHandler handler(posIter, status);
    return formatInt32(number, appendTo, handler, status);
}

template<class T>
UBool DecimalFormatImpl::maybeFormatWithDigitList(
        T number,
        UnicodeString &appendTo,
        FieldPositionHandler &handler,
        UErrorCode &status) const {
    if (!fMultiplier.isZero()) {
        DigitList digits;
        digits.set(number);
        digits.mult(fMultiplier, status);
        digits.shiftDecimalRight(fScale);
        formatAdjustedDigitList(digits, appendTo, handler, status);
        return TRUE;
    }
    if (fScale != 0) {
        DigitList digits;
        digits.set(number);
        digits.shiftDecimalRight(fScale);
        formatAdjustedDigitList(digits, appendTo, handler, status);
        return TRUE;
    }
    return FALSE;
}

template<class T>
UBool DecimalFormatImpl::maybeInitVisibleDigitsFromDigitList(
        T number,
        VisibleDigitsWithExponent &visibleDigits,
        UErrorCode &status) const {
    if (!fMultiplier.isZero()) {
        DigitList digits;
        digits.set(number);
        digits.mult(fMultiplier, status);
        digits.shiftDecimalRight(fScale);
        initVisibleDigitsFromAdjusted(digits, visibleDigits, status);
        return TRUE;
    }
    if (fScale != 0) {
        DigitList digits;
        digits.set(number);
        digits.shiftDecimalRight(fScale);
        initVisibleDigitsFromAdjusted(digits, visibleDigits, status);
        return TRUE;
    }
    return FALSE;
}

UnicodeString &
DecimalFormatImpl::formatInt32(
        int32_t number,
        UnicodeString &appendTo,
        FieldPositionHandler &handler,
        UErrorCode &status) const {
    if (maybeFormatWithDigitList(number, appendTo, handler, status)) {
        return appendTo;
    }
    ValueFormatter vf;
    return fAffixes.formatInt32(
            number,
            prepareValueFormatter(vf),
            handler,
            fRules,
            appendTo,
            status);
}

UnicodeString &
DecimalFormatImpl::formatInt64(
        int64_t number,
        UnicodeString &appendTo,
        FieldPositionHandler &handler,
        UErrorCode &status) const {
    if (number >= INT32_MIN && number <= INT32_MAX) {
        return formatInt32((int32_t) number, appendTo, handler, status);
    }
    VisibleDigitsWithExponent digits;
    initVisibleDigitsWithExponent(number, digits, status);
    return formatVisibleDigitsWithExponent(
            digits, appendTo, handler, status);
}

UnicodeString &
DecimalFormatImpl::formatDouble(
        double number,
        UnicodeString &appendTo,
        FieldPositionHandler &handler,
        UErrorCode &status) const {
    VisibleDigitsWithExponent digits;
    initVisibleDigitsWithExponent(number, digits, status);
    return formatVisibleDigitsWithExponent(
            digits, appendTo, handler, status);
}

UnicodeString &
DecimalFormatImpl::format(
        double number,
        UnicodeString &appendTo,
        FieldPosition &pos,
        UErrorCode &status) const {
    FieldPositionOnlyHandler handler(pos);
    return formatDouble(number, appendTo, handler, status);
}

UnicodeString &
DecimalFormatImpl::format(
        const DigitList &number,
        UnicodeString &appendTo,
        FieldPosition &pos,
        UErrorCode &status) const {
    DigitList dl(number);
    FieldPositionOnlyHandler handler(pos);
    return formatDigitList(dl, appendTo, handler, status);
}

UnicodeString &
DecimalFormatImpl::format(
        int64_t number,
        UnicodeString &appendTo,
        FieldPosition &pos,
        UErrorCode &status) const {
    FieldPositionOnlyHandler handler(pos);
    return formatInt64(number, appendTo, handler, status);
}

UnicodeString &
DecimalFormatImpl::format(
        int64_t number,
        UnicodeString &appendTo,
        FieldPositionIterator *posIter,
        UErrorCode &status) const {
    FieldPositionIteratorHandler handler(posIter, status);
    return formatInt64(number, appendTo, handler, status);
}

UnicodeString &
DecimalFormatImpl::format(
        double number,
        UnicodeString &appendTo,
        FieldPositionIterator *posIter,
        UErrorCode &status) const {
    FieldPositionIteratorHandler handler(posIter, status);
    return formatDouble(number, appendTo, handler, status);
}

UnicodeString &
DecimalFormatImpl::format(
        const DigitList &number,
        UnicodeString &appendTo,
        FieldPositionIterator *posIter,
        UErrorCode &status) const {
    DigitList dl(number);
    FieldPositionIteratorHandler handler(posIter, status);
    return formatDigitList(dl, appendTo, handler, status);
}

UnicodeString &
DecimalFormatImpl::format(
        StringPiece number,
        UnicodeString &appendTo,
        FieldPositionIterator *posIter,
        UErrorCode &status) const {
    DigitList dl;
    dl.set(number, status);
    FieldPositionIteratorHandler handler(posIter, status);
    return formatDigitList(dl, appendTo, handler, status);
}

UnicodeString &
DecimalFormatImpl::format(
        const VisibleDigitsWithExponent &digits,
        UnicodeString &appendTo,
        FieldPosition &pos,
        UErrorCode &status) const {
    FieldPositionOnlyHandler handler(pos);
    return formatVisibleDigitsWithExponent(
            digits, appendTo, handler, status);
}

UnicodeString &
DecimalFormatImpl::format(
        const VisibleDigitsWithExponent &digits,
        UnicodeString &appendTo,
        FieldPositionIterator *posIter,
        UErrorCode &status) const {
    FieldPositionIteratorHandler handler(posIter, status);
    return formatVisibleDigitsWithExponent(
            digits, appendTo, handler, status);
}

DigitList &
DecimalFormatImpl::adjustDigitList(
        DigitList &number, UErrorCode &status) const {
    number.setRoundingMode(fRoundingMode);
    if (!fMultiplier.isZero()) {
        number.mult(fMultiplier, status);
    }
    if (fScale != 0) {
        number.shiftDecimalRight(fScale);
    }
    number.reduce();
    return number;
}

UnicodeString &
DecimalFormatImpl::formatDigitList(
        DigitList &number,
        UnicodeString &appendTo,
        FieldPositionHandler &handler,
        UErrorCode &status) const {
    VisibleDigitsWithExponent digits;
    initVisibleDigitsWithExponent(number, digits, status);
    return formatVisibleDigitsWithExponent(
            digits, appendTo, handler, status);
}

UnicodeString &
DecimalFormatImpl::formatAdjustedDigitList(
        DigitList &number,
        UnicodeString &appendTo,
        FieldPositionHandler &handler,
        UErrorCode &status) const {
    ValueFormatter vf;
    return fAffixes.format(
            number,
            prepareValueFormatter(vf),
            handler,
            fRules,
            appendTo,
            status);
}

UnicodeString &
DecimalFormatImpl::formatVisibleDigitsWithExponent(
        const VisibleDigitsWithExponent &digits,
        UnicodeString &appendTo,
        FieldPositionHandler &handler,
        UErrorCode &status) const {
    ValueFormatter vf;
    return fAffixes.format(
            digits,
            prepareValueFormatter(vf),
            handler,
            fRules,
            appendTo,
            status);
}

static FixedDecimal &initFixedDecimal(
        const VisibleDigits &digits, FixedDecimal &result) {
    result.source = 0.0;
    result.isNegative = digits.isNegative();
    result.isNanOrInfinity = digits.isNaNOrInfinity();
    digits.getFixedDecimal(
            result.source, result.intValue, result.decimalDigits,
            result.decimalDigitsWithoutTrailingZeros,
            result.visibleDecimalDigitCount, result.hasIntegerValue);
    return result;
}

FixedDecimal &
DecimalFormatImpl::getFixedDecimal(double number, FixedDecimal &result, UErrorCode &status) const {
    if (U_FAILURE(status)) {
        return result;
    }
    VisibleDigits digits;
    fEffPrecision.fMantissa.initVisibleDigits(number, digits, status);
    return initFixedDecimal(digits, result);
}

FixedDecimal &
DecimalFormatImpl::getFixedDecimal(
        DigitList &number, FixedDecimal &result, UErrorCode &status) const {
    if (U_FAILURE(status)) {
        return result;
    }
    VisibleDigits digits;
    fEffPrecision.fMantissa.initVisibleDigits(number, digits, status);
    return initFixedDecimal(digits, result);
}

VisibleDigitsWithExponent &
DecimalFormatImpl::initVisibleDigitsWithExponent(
        int64_t number,
        VisibleDigitsWithExponent &digits,
        UErrorCode &status) const {
    if (maybeInitVisibleDigitsFromDigitList(
            number, digits, status)) {
        return digits;
    }
    if (fUseScientific) {
        fEffPrecision.initVisibleDigitsWithExponent(
                number, digits, status);
    } else {
        fEffPrecision.fMantissa.initVisibleDigitsWithExponent(
                number, digits, status);
    }
    return digits;
}

VisibleDigitsWithExponent &
DecimalFormatImpl::initVisibleDigitsWithExponent(
        double number,
        VisibleDigitsWithExponent &digits,
        UErrorCode &status) const {
    if (maybeInitVisibleDigitsFromDigitList(
            number, digits, status)) {
        return digits;
    }
    if (fUseScientific) {
        fEffPrecision.initVisibleDigitsWithExponent(
                number, digits, status);
    } else {
        fEffPrecision.fMantissa.initVisibleDigitsWithExponent(
                number, digits, status);
    }
    return digits;
}

VisibleDigitsWithExponent &
DecimalFormatImpl::initVisibleDigitsWithExponent(
        DigitList &number,
        VisibleDigitsWithExponent &digits,
        UErrorCode &status) const {
    adjustDigitList(number, status);
    return initVisibleDigitsFromAdjusted(number, digits, status);
}

VisibleDigitsWithExponent &
DecimalFormatImpl::initVisibleDigitsFromAdjusted(
        DigitList &number,
        VisibleDigitsWithExponent &digits,
        UErrorCode &status) const {
    if (fUseScientific) {
        fEffPrecision.initVisibleDigitsWithExponent(
                number, digits, status);
    } else {
        fEffPrecision.fMantissa.initVisibleDigitsWithExponent(
                number, digits, status);
    }
    return digits;
}

DigitList &
DecimalFormatImpl::round(
        DigitList &number, UErrorCode &status) const {
    if (number.isNaN() || number.isInfinite()) {
        return number;
    }
    adjustDigitList(number, status);
    ValueFormatter vf;
    prepareValueFormatter(vf);
    return vf.round(number, status);
}

void
DecimalFormatImpl::setMinimumSignificantDigits(int32_t newValue) {
    fMinSigDigits = newValue;
    fUseSigDigits = TRUE; // ticket 9936
    updatePrecision();
}

void
DecimalFormatImpl::setMaximumSignificantDigits(int32_t newValue) {
    fMaxSigDigits = newValue;
    fUseSigDigits = TRUE; // ticket 9936
    updatePrecision();
}

void
DecimalFormatImpl::setMinMaxSignificantDigits(int32_t min, int32_t max) {
    fMinSigDigits = min;
    fMaxSigDigits = max;
    fUseSigDigits = TRUE; // ticket 9936
    updatePrecision();
}

void
DecimalFormatImpl::setScientificNotation(UBool newValue) {
    fUseScientific = newValue;
    updatePrecision();
}

void
DecimalFormatImpl::setSignificantDigitsUsed(UBool newValue) {
    fUseSigDigits = newValue;
    updatePrecision();
}

void
DecimalFormatImpl::setGroupingSize(int32_t newValue) {
    fGrouping.fGrouping = newValue;
    updateGrouping();
}

void
DecimalFormatImpl::setSecondaryGroupingSize(int32_t newValue) {
    fGrouping.fGrouping2 = newValue;
    updateGrouping();
}

void
DecimalFormatImpl::setMinimumGroupingDigits(int32_t newValue) {
    fGrouping.fMinGrouping = newValue;
    updateGrouping();
}

void
DecimalFormatImpl::setCurrencyUsage(
        UCurrencyUsage currencyUsage, UErrorCode &status) {
    fCurrencyUsage = currencyUsage;
    updateFormatting(kFormattingCurrency, status);
}

void
DecimalFormatImpl::setRoundingIncrement(double d) {
    if (d > 0.0) {
        fEffPrecision.fMantissa.fRoundingIncrement.set(d);
    } else {
        fEffPrecision.fMantissa.fRoundingIncrement.set(0.0);
    }
}

double
DecimalFormatImpl::getRoundingIncrement() const {
    return fEffPrecision.fMantissa.fRoundingIncrement.getDouble();
}

int32_t
DecimalFormatImpl::getMultiplier() const {
    if (fMultiplier.isZero()) {
        return 1;
    }
    return (int32_t) fMultiplier.getDouble();
}

void
DecimalFormatImpl::setMultiplier(int32_t m) {
    if (m == 0 || m == 1) {
        fMultiplier.set((int32_t)0);
    } else {
        fMultiplier.set(m);
    }
}

void
DecimalFormatImpl::setPositivePrefix(const UnicodeString &str) {
    fPositivePrefixPattern.remove();
    fPositivePrefixPattern.addLiteral(str.getBuffer(), 0, str.length());
    UErrorCode status = U_ZERO_ERROR;
    updateFormatting(kFormattingPosPrefix, status);
}

void
DecimalFormatImpl::setPositiveSuffix(const UnicodeString &str) {
    fPositiveSuffixPattern.remove();
    fPositiveSuffixPattern.addLiteral(str.getBuffer(), 0, str.length());
    UErrorCode status = U_ZERO_ERROR;
    updateFormatting(kFormattingPosSuffix, status);
}

void
DecimalFormatImpl::setNegativePrefix(const UnicodeString &str) {
    fNegativePrefixPattern.remove();
    fNegativePrefixPattern.addLiteral(str.getBuffer(), 0, str.length());
    UErrorCode status = U_ZERO_ERROR;
    updateFormatting(kFormattingNegPrefix, status);
}

void
DecimalFormatImpl::setNegativeSuffix(const UnicodeString &str) {
    fNegativeSuffixPattern.remove();
    fNegativeSuffixPattern.addLiteral(str.getBuffer(), 0, str.length());
    UErrorCode status = U_ZERO_ERROR;
    updateFormatting(kFormattingNegSuffix, status);
}

UnicodeString &
DecimalFormatImpl::getPositivePrefix(UnicodeString &result) const {
    result = fAffixes.fPositivePrefix.getOtherVariant().toString();
    return result;
}

UnicodeString &
DecimalFormatImpl::getPositiveSuffix(UnicodeString &result) const {
    result = fAffixes.fPositiveSuffix.getOtherVariant().toString();
    return result;
}

UnicodeString &
DecimalFormatImpl::getNegativePrefix(UnicodeString &result) const {
    result = fAffixes.fNegativePrefix.getOtherVariant().toString();
    return result;
}

UnicodeString &
DecimalFormatImpl::getNegativeSuffix(UnicodeString &result) const {
    result = fAffixes.fNegativeSuffix.getOtherVariant().toString();
    return result;
}

void
DecimalFormatImpl::adoptDecimalFormatSymbols(DecimalFormatSymbols *symbolsToAdopt) {
    if (symbolsToAdopt == NULL) {
        return;
    }
    delete fSymbols;
    fSymbols = symbolsToAdopt;
    UErrorCode status = U_ZERO_ERROR;
    updateFormatting(kFormattingSymbols, status);
}

void
DecimalFormatImpl::applyPatternFavorCurrencyPrecision(
        const UnicodeString &pattern, UErrorCode &status) {
    UParseError perror;
    applyPattern(pattern, FALSE, perror, status);
    updateForApplyPatternFavorCurrencyPrecision(status);
}

void
DecimalFormatImpl::applyPattern(
        const UnicodeString &pattern, UErrorCode &status) {
    UParseError perror;
    applyPattern(pattern, FALSE, perror, status);
    updateForApplyPattern(status);
}

void
DecimalFormatImpl::applyPattern(
        const UnicodeString &pattern,
        UParseError &perror, UErrorCode &status) {
    applyPattern(pattern, FALSE, perror, status);
    updateForApplyPattern(status);
}

void
DecimalFormatImpl::applyLocalizedPattern(
        const UnicodeString &pattern, UErrorCode &status) {
    UParseError perror;
    applyPattern(pattern, TRUE, perror, status);
    updateForApplyPattern(status);
}

void
DecimalFormatImpl::applyLocalizedPattern(
        const UnicodeString &pattern,
        UParseError &perror,  UErrorCode &status) {
    applyPattern(pattern, TRUE, perror, status);
    updateForApplyPattern(status);
}

void
DecimalFormatImpl::applyPattern(
        const UnicodeString &pattern,
        UBool localized, UParseError &perror, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
    DecimalFormatPatternParser patternParser;
    if (localized) {
        patternParser.useSymbols(*fSymbols);
    }
    DecimalFormatPattern out;
    patternParser.applyPatternWithoutExpandAffix(
            pattern, out, perror, status);
    if (U_FAILURE(status)) {
        return;
    }
    fUseScientific = out.fUseExponentialNotation;
    fUseSigDigits = out.fUseSignificantDigits;
    fSuper->NumberFormat::setMinimumIntegerDigits(out.fMinimumIntegerDigits);
    fSuper->NumberFormat::setMaximumIntegerDigits(out.fMaximumIntegerDigits);
    fSuper->NumberFormat::setMinimumFractionDigits(out.fMinimumFractionDigits);
    fSuper->NumberFormat::setMaximumFractionDigits(out.fMaximumFractionDigits);
    fMinSigDigits = out.fMinimumSignificantDigits;
    fMaxSigDigits = out.fMaximumSignificantDigits;
    fEffPrecision.fMinExponentDigits = out.fMinExponentDigits;
    fOptions.fExponent.fAlwaysShowSign = out.fExponentSignAlwaysShown;
    fSuper->NumberFormat::setGroupingUsed(out.fGroupingUsed);
    fGrouping.fGrouping = out.fGroupingSize;
    fGrouping.fGrouping2 = out.fGroupingSize2;
    fOptions.fMantissa.fAlwaysShowDecimal = out.fDecimalSeparatorAlwaysShown;
    if (out.fRoundingIncrementUsed) {
        fEffPrecision.fMantissa.fRoundingIncrement = out.fRoundingIncrement;
    } else {
        fEffPrecision.fMantissa.fRoundingIncrement.clear();
    }
    fAffixes.fPadChar = out.fPad;
    fNegativePrefixPattern = out.fNegPrefixAffix;
    fNegativeSuffixPattern = out.fNegSuffixAffix;
    fPositivePrefixPattern = out.fPosPrefixAffix;
    fPositiveSuffixPattern = out.fPosSuffixAffix;

    // Work around. Pattern parsing code and DecimalFormat code don't agree
    // on the definition of field width, so we have to translate from
    // pattern field width to decimal format field width here.
    fAffixes.fWidth = out.fFormatWidth == 0 ? 0 :
            out.fFormatWidth + fPositivePrefixPattern.countChar32()
            + fPositiveSuffixPattern.countChar32();
    switch (out.fPadPosition) {
    case DecimalFormatPattern::kPadBeforePrefix:
        fAffixes.fPadPosition = DigitAffixesAndPadding::kPadBeforePrefix;
        break;
    case DecimalFormatPattern::kPadAfterPrefix:
        fAffixes.fPadPosition = DigitAffixesAndPadding::kPadAfterPrefix;
        break;
    case DecimalFormatPattern::kPadBeforeSuffix:
        fAffixes.fPadPosition = DigitAffixesAndPadding::kPadBeforeSuffix;
        break;
    case DecimalFormatPattern::kPadAfterSuffix:
        fAffixes.fPadPosition = DigitAffixesAndPadding::kPadAfterSuffix;
        break;
    default:
        break;
    }
}

void
DecimalFormatImpl::updatePrecision() {
    if (fUseScientific) {
        updatePrecisionForScientific();
    } else {
        updatePrecisionForFixed();
    }
}

static void updatePrecisionForScientificMinMax(
        const DigitInterval &min,
        const DigitInterval &max,
        DigitInterval &resultMin,
        DigitInterval &resultMax,
        SignificantDigitInterval &resultSignificant) {
    resultMin.setIntDigitCount(0);
    resultMin.setFracDigitCount(0);
    resultSignificant.clear();
    resultMax.clear();

    int32_t maxIntDigitCount = max.getIntDigitCount();
    int32_t minIntDigitCount = min.getIntDigitCount();
    int32_t maxFracDigitCount = max.getFracDigitCount();
    int32_t minFracDigitCount = min.getFracDigitCount();


    // Not in spec: maxIntDigitCount > 8 assume
    // maxIntDigitCount = minIntDigitCount. Current DecimalFormat API has
    // no provision for unsetting maxIntDigitCount which would be useful for
    // scientific notation. The best we can do is assume that if
    // maxIntDigitCount is the default of 2000000000 or is "big enough" then
    // user did not intend to explicitly set it. The 8 was derived emperically
    // by extensive testing of legacy code.
    if (maxIntDigitCount > 8) {
        maxIntDigitCount = minIntDigitCount;
    }

    // Per the spec, exponent grouping happens if maxIntDigitCount is more
    // than 1 and more than minIntDigitCount.
    UBool bExponentGrouping = maxIntDigitCount > 1 && minIntDigitCount < maxIntDigitCount;
    if (bExponentGrouping) {
        resultMax.setIntDigitCount(maxIntDigitCount);

        // For exponent grouping minIntDigits is always treated as 1 even
        // if it wasn't set to 1!
        resultMin.setIntDigitCount(1);
    } else {
        // Fixed digit count left of decimal. minIntDigitCount doesn't have
        // to equal maxIntDigitCount i.e minIntDigitCount == 0 while
        // maxIntDigitCount == 1.
        int32_t fixedIntDigitCount = maxIntDigitCount;

        // If fixedIntDigitCount is 0 but
        // min or max fraction count is 0 too then use 1. This way we can get
        // unlimited precision for X.XXXEX
        if (fixedIntDigitCount == 0 && (minFracDigitCount == 0 || maxFracDigitCount == 0)) {
            fixedIntDigitCount = 1;
        }
        resultMax.setIntDigitCount(fixedIntDigitCount);
        resultMin.setIntDigitCount(fixedIntDigitCount);
    }
    // Spec says this is how we compute significant digits. 0 means
    // unlimited significant digits.
    int32_t maxSigDigits = minIntDigitCount + maxFracDigitCount;
    if (maxSigDigits > 0) {
        int32_t minSigDigits = minIntDigitCount + minFracDigitCount;
        resultSignificant.setMin(minSigDigits);
        resultSignificant.setMax(maxSigDigits);
    }
}

void
DecimalFormatImpl::updatePrecisionForScientific() {
    FixedPrecision *result = &fEffPrecision.fMantissa;
    if (fUseSigDigits) {
        result->fMax.setFracDigitCount(-1);
        result->fMax.setIntDigitCount(1);
        result->fMin.setFracDigitCount(0);
        result->fMin.setIntDigitCount(1);
        result->fSignificant.clear();
        extractSigDigits(result->fSignificant);
        return;
    }
    DigitInterval max;
    DigitInterval min;
    extractMinMaxDigits(min, max);
    updatePrecisionForScientificMinMax(
            min, max,
            result->fMin, result->fMax, result->fSignificant);
}

void
DecimalFormatImpl::updatePrecisionForFixed() {
    FixedPrecision *result = &fEffPrecision.fMantissa;
    if (!fUseSigDigits) {
        extractMinMaxDigits(result->fMin, result->fMax);
        result->fSignificant.clear();
    } else {
        extractSigDigits(result->fSignificant);
        result->fMin.setIntDigitCount(1);
        result->fMin.setFracDigitCount(0);
        result->fMax.clear();
    }
}

void
 DecimalFormatImpl::extractMinMaxDigits(
        DigitInterval &min, DigitInterval &max) const {
    min.setIntDigitCount(fSuper->getMinimumIntegerDigits());
    max.setIntDigitCount(fSuper->getMaximumIntegerDigits());
    min.setFracDigitCount(fSuper->getMinimumFractionDigits());
    max.setFracDigitCount(fSuper->getMaximumFractionDigits());
}

void
 DecimalFormatImpl::extractSigDigits(
        SignificantDigitInterval &sig) const {
    sig.setMin(fMinSigDigits < 0 ? 0 : fMinSigDigits);
    sig.setMax(fMaxSigDigits < 0 ? 0 : fMaxSigDigits);
}

void
DecimalFormatImpl::updateGrouping() {
    if (fSuper->isGroupingUsed()) {
        fEffGrouping = fGrouping;
    } else {
        fEffGrouping.clear();
    }
}

void
DecimalFormatImpl::updateCurrency(UErrorCode &status) {
    updateFormatting(kFormattingCurrency, TRUE, status);
}

void
DecimalFormatImpl::updateFormatting(
        int32_t changedFormattingFields,
        UErrorCode &status) {
    updateFormatting(changedFormattingFields, TRUE, status);
}

void
DecimalFormatImpl::updateFormatting(
        int32_t changedFormattingFields,
        UBool updatePrecisionBasedOnCurrency,
        UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
    // Each function updates one field. Order matters. For instance,
    // updatePluralRules comes before updateCurrencyAffixInfo because the
    // fRules field is needed to update the fCurrencyAffixInfo field.
    updateFormattingUsesCurrency(changedFormattingFields);
    updateFormattingFixedPointFormatter(changedFormattingFields);
    updateFormattingAffixParser(changedFormattingFields);
    updateFormattingPluralRules(changedFormattingFields, status);
    updateFormattingCurrencyAffixInfo(
            changedFormattingFields,
            updatePrecisionBasedOnCurrency,
            status);
    updateFormattingLocalizedPositivePrefix(
            changedFormattingFields, status);
    updateFormattingLocalizedPositiveSuffix(
            changedFormattingFields, status);
    updateFormattingLocalizedNegativePrefix(
            changedFormattingFields, status);
    updateFormattingLocalizedNegativeSuffix(
            changedFormattingFields, status);
}

void
DecimalFormatImpl::updateFormattingUsesCurrency(
        int32_t &changedFormattingFields) {
    if ((changedFormattingFields & kFormattingAffixes) == 0) {
        // If no affixes changed, don't need to do any work
        return;
    }
    UBool newUsesCurrency =
            fPositivePrefixPattern.usesCurrency() ||
            fPositiveSuffixPattern.usesCurrency() ||
            fNegativePrefixPattern.usesCurrency() ||
            fNegativeSuffixPattern.usesCurrency();
    if (fMonetary != newUsesCurrency) {
        fMonetary = newUsesCurrency;
        changedFormattingFields |= kFormattingUsesCurrency;
    }
}

void
DecimalFormatImpl::updateFormattingPluralRules(
        int32_t &changedFormattingFields, UErrorCode &status) {
    if ((changedFormattingFields & (kFormattingSymbols | kFormattingUsesCurrency)) == 0) {
        // No work to do if both fSymbols and fMonetary
        // fields are unchanged
        return;
    }
    if (U_FAILURE(status)) {
        return;
    }
    PluralRules *newRules = NULL;
    if (fMonetary) {
        newRules = PluralRules::forLocale(fSymbols->getLocale(), status);
        if (U_FAILURE(status)) {
            return;
        }
    }
    // Its ok to say a field has changed when it really hasn't but not
    // the other way around. Here we assume the field changed unless it
    // was NULL before and is still NULL now
    if (fRules != newRules) {
        delete fRules;
        fRules = newRules;
        changedFormattingFields |= kFormattingPluralRules;
    }
}

void
DecimalFormatImpl::updateFormattingCurrencyAffixInfo(
        int32_t &changedFormattingFields,
        UBool updatePrecisionBasedOnCurrency,
        UErrorCode &status) {
    if ((changedFormattingFields & (
            kFormattingSymbols | kFormattingCurrency |
            kFormattingUsesCurrency | kFormattingPluralRules)) == 0) {
        // If all these fields are unchanged, no work to do.
        return;
    }
    if (U_FAILURE(status)) {
        return;
    }
    if (!fMonetary) {
        if (fCurrencyAffixInfo.isDefault()) {
            // In this case don't have to do any work
            return;
        }
        fCurrencyAffixInfo.set(NULL, NULL, NULL, status);
        if (U_FAILURE(status)) {
            return;
        }
        changedFormattingFields |= kFormattingCurrencyAffixInfo;
    } else {
        const UChar *currency = fSuper->getCurrency();
        UChar localeCurr[4];
        if (currency[0] == 0) {
            ucurr_forLocale(fSymbols->getLocale().getName(), localeCurr, UPRV_LENGTHOF(localeCurr), &status);
            if (U_SUCCESS(status)) {
                currency = localeCurr;
                fSuper->NumberFormat::setCurrency(currency, status);
            } else {
                currency = NULL;
                status = U_ZERO_ERROR;
            }
        }
        fCurrencyAffixInfo.set(
                fSymbols->getLocale().getName(), fRules, currency, status);
        if (U_FAILURE(status)) {
            return;
        }
        UBool customCurrencySymbol = FALSE;
        // If DecimalFormatSymbols has custom currency symbol, prefer
        // that over what we just read from the resource bundles
        if (fSymbols->isCustomCurrencySymbol()) {
            fCurrencyAffixInfo.setSymbol(
                    fSymbols->getConstSymbol(DecimalFormatSymbols::kCurrencySymbol));
            customCurrencySymbol = TRUE;
        }
        if (fSymbols->isCustomIntlCurrencySymbol()) {
            fCurrencyAffixInfo.setISO(
                    fSymbols->getConstSymbol(DecimalFormatSymbols::kIntlCurrencySymbol));
            customCurrencySymbol = TRUE;
        }
        changedFormattingFields |= kFormattingCurrencyAffixInfo;
        if (currency && !customCurrencySymbol && updatePrecisionBasedOnCurrency) {
            FixedPrecision precision;
            CurrencyAffixInfo::adjustPrecision(
                    currency, fCurrencyUsage, precision, status);
            if (U_FAILURE(status)) {
                return;
            }
            fSuper->NumberFormat::setMinimumFractionDigits(
                    precision.fMin.getFracDigitCount());
            fSuper->NumberFormat::setMaximumFractionDigits(
                    precision.fMax.getFracDigitCount());
            updatePrecision();
            fEffPrecision.fMantissa.fRoundingIncrement =
                    precision.fRoundingIncrement;
        }

    }
}

void
DecimalFormatImpl::updateFormattingFixedPointFormatter(
        int32_t &changedFormattingFields) {
    if ((changedFormattingFields & (kFormattingSymbols | kFormattingUsesCurrency)) == 0) {
        // No work to do if fSymbols is unchanged
        return;
    }
    if (fMonetary) {
        fFormatter.setDecimalFormatSymbolsForMonetary(*fSymbols);
    } else {
        fFormatter.setDecimalFormatSymbols(*fSymbols);
    }
}

void
DecimalFormatImpl::updateFormattingAffixParser(
        int32_t &changedFormattingFields) {
    if ((changedFormattingFields & kFormattingSymbols) == 0) {
        // No work to do if fSymbols is unchanged
        return;
    }
    fAffixParser.setDecimalFormatSymbols(*fSymbols);
    changedFormattingFields |= kFormattingAffixParser;
}

void
DecimalFormatImpl::updateFormattingLocalizedPositivePrefix(
        int32_t &changedFormattingFields, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
    if ((changedFormattingFields & (
            kFormattingPosPrefix | kFormattingAffixParserWithCurrency)) == 0) {
        // No work to do
        return;
    }
    fAffixes.fPositivePrefix.remove();
    fAffixParser.parse(
            fPositivePrefixPattern,
            fCurrencyAffixInfo,
            fAffixes.fPositivePrefix,
            status);
}

void
DecimalFormatImpl::updateFormattingLocalizedPositiveSuffix(
        int32_t &changedFormattingFields, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
    if ((changedFormattingFields & (
            kFormattingPosSuffix | kFormattingAffixParserWithCurrency)) == 0) {
        // No work to do
        return;
    }
    fAffixes.fPositiveSuffix.remove();
    fAffixParser.parse(
            fPositiveSuffixPattern,
            fCurrencyAffixInfo,
            fAffixes.fPositiveSuffix,
            status);
}

void
DecimalFormatImpl::updateFormattingLocalizedNegativePrefix(
        int32_t &changedFormattingFields, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
    if ((changedFormattingFields & (
            kFormattingNegPrefix | kFormattingAffixParserWithCurrency)) == 0) {
        // No work to do
        return;
    }
    fAffixes.fNegativePrefix.remove();
    fAffixParser.parse(
            fNegativePrefixPattern,
            fCurrencyAffixInfo,
            fAffixes.fNegativePrefix,
            status);
}

void
DecimalFormatImpl::updateFormattingLocalizedNegativeSuffix(
        int32_t &changedFormattingFields, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
    if ((changedFormattingFields & (
            kFormattingNegSuffix | kFormattingAffixParserWithCurrency)) == 0) {
        // No work to do
        return;
    }
    fAffixes.fNegativeSuffix.remove();
    fAffixParser.parse(
            fNegativeSuffixPattern,
            fCurrencyAffixInfo,
            fAffixes.fNegativeSuffix,
            status);
}

void
DecimalFormatImpl::updateForApplyPatternFavorCurrencyPrecision(
        UErrorCode &status) {
    updateAll(kFormattingAll & ~kFormattingSymbols, TRUE, status);
}

void
DecimalFormatImpl::updateForApplyPattern(UErrorCode &status) {
    updateAll(kFormattingAll & ~kFormattingSymbols, FALSE, status);
}

void
DecimalFormatImpl::updateAll(UErrorCode &status) {
    updateAll(kFormattingAll, TRUE, status);
}

void
DecimalFormatImpl::updateAll(
        int32_t formattingFlags,
        UBool updatePrecisionBasedOnCurrency,
        UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
    updatePrecision();
    updateGrouping();
    updateFormatting(
            formattingFlags, updatePrecisionBasedOnCurrency, status);
    setMultiplierScale(getPatternScale());
}


static int32_t
getMinimumLengthToDescribeGrouping(const DigitGrouping &grouping) {
    if (grouping.fGrouping <= 0) {
        return 0;
    }
    if (grouping.fGrouping2 <= 0) {
        return grouping.fGrouping + 1;
    }
    return grouping.fGrouping + grouping.fGrouping2 + 1;
}

/**
 * Given a grouping policy, calculates how many digits are needed left of
 * the decimal point to achieve a desired length left of the
 * decimal point.
 * @param grouping the grouping policy
 * @param desiredLength number of characters needed left of decimal point
 * @param minLeftDigits at least this many digits is returned
 * @param leftDigits the number of digits needed stored here
 *  which is >= minLeftDigits.
 * @return true if a perfect fit or false if having leftDigits would exceed
 *   desiredLength
 */
static UBool
getLeftDigitsForLeftLength(
        const DigitGrouping &grouping,
        int32_t desiredLength,
        int32_t minLeftDigits,
        int32_t &leftDigits) {
    leftDigits = minLeftDigits;
    int32_t lengthSoFar = leftDigits + grouping.getSeparatorCount(leftDigits);
    while (lengthSoFar < desiredLength) {
        lengthSoFar += grouping.isSeparatorAt(leftDigits + 1, leftDigits) ? 2 : 1;
        ++leftDigits;
    }
    return (lengthSoFar == desiredLength);
}

int32_t
DecimalFormatImpl::computeExponentPatternLength() const {
    if (fUseScientific) {
        return 1 + (fOptions.fExponent.fAlwaysShowSign ? 1 : 0) + fEffPrecision.fMinExponentDigits;
    }
    return 0;
}

int32_t
DecimalFormatImpl::countFractionDigitAndDecimalPatternLength(
        int32_t fracDigitCount) const {
    if (!fOptions.fMantissa.fAlwaysShowDecimal && fracDigitCount == 0) {
        return 0;
    }
    return fracDigitCount + 1;
}

UnicodeString&
DecimalFormatImpl::toNumberPattern(
        UBool hasPadding, int32_t minimumLength, UnicodeString& result) const {
    // Get a grouping policy like the one in this object that does not
    // have minimum grouping since toPattern doesn't support it.
    DigitGrouping grouping(fEffGrouping);
    grouping.fMinGrouping = 0;

    // Only for fixed digits, these are the digits that get 0's.
    DigitInterval minInterval;

    // Only for fixed digits, these are the digits that get #'s.
    DigitInterval maxInterval;

    // Only for significant digits
    int32_t sigMin;
    int32_t sigMax;

    // These are all the digits to be displayed. For significant digits,
    // this interval always starts at the 1's place an extends left.
    DigitInterval fullInterval;

    // Digit range of rounding increment. If rounding increment is .025.
    // then roundingIncrementLowerExp = -3 and roundingIncrementUpperExp = -1
    int32_t roundingIncrementLowerExp = 0;
    int32_t roundingIncrementUpperExp = 0;

    if (fUseSigDigits) {
        SignificantDigitInterval sigInterval;
        extractSigDigits(sigInterval);
        sigMax = sigInterval.getMax();
        sigMin = sigInterval.getMin();
        fullInterval.setFracDigitCount(0);
        fullInterval.setIntDigitCount(sigMax);
    } else {
        extractMinMaxDigits(minInterval, maxInterval);
        if (fUseScientific) {
           if (maxInterval.getIntDigitCount() > kMaxScientificIntegerDigits) {
               maxInterval.setIntDigitCount(1);
               minInterval.shrinkToFitWithin(maxInterval);
           }
        } else if (hasPadding) {
            // Make max int digits match min int digits for now, we
            // compute necessary padding later.
            maxInterval.setIntDigitCount(minInterval.getIntDigitCount());
        } else {
            // For some reason toPattern adds at least one leading '#'
            maxInterval.setIntDigitCount(minInterval.getIntDigitCount() + 1);
        }
        if (!fEffPrecision.fMantissa.fRoundingIncrement.isZero()) {
            roundingIncrementLowerExp =
                    fEffPrecision.fMantissa.fRoundingIncrement.getLowerExponent();
            roundingIncrementUpperExp =
                    fEffPrecision.fMantissa.fRoundingIncrement.getUpperExponent();
            // We have to include the rounding increment in what we display
            maxInterval.expandToContainDigit(roundingIncrementLowerExp);
            maxInterval.expandToContainDigit(roundingIncrementUpperExp - 1);
        }
        fullInterval = maxInterval;
    }
    // We have to include enough digits to show grouping strategy
    int32_t minLengthToDescribeGrouping =
           getMinimumLengthToDescribeGrouping(grouping);
    if (minLengthToDescribeGrouping > 0) {
        fullInterval.expandToContainDigit(
                getMinimumLengthToDescribeGrouping(grouping) - 1);
    }

    // If we have a minimum length, we have to add digits to the left to
    // depict padding.
    if (hasPadding) {
        // For non scientific notation,
        //  minimumLengthForMantissa = minimumLength
        int32_t minimumLengthForMantissa =
                minimumLength - computeExponentPatternLength();
        int32_t mininumLengthForMantissaIntPart =
                minimumLengthForMantissa
                - countFractionDigitAndDecimalPatternLength(
                        fullInterval.getFracDigitCount());
        // Because of grouping, we may need fewer than expected digits to
        // achieve the length we need.
        int32_t digitsNeeded;
        if (getLeftDigitsForLeftLength(
                grouping,
                mininumLengthForMantissaIntPart,
                fullInterval.getIntDigitCount(),
                digitsNeeded)) {

            // In this case, we achieved the exact length that we want.
            fullInterval.setIntDigitCount(digitsNeeded);
        } else if (digitsNeeded > fullInterval.getIntDigitCount()) {

            // Having digitsNeeded digits goes over desired length which
            // means that to have desired length would mean starting on a
            // grouping sepearator e.g ,###,### so add a '#' and use one
            // less digit. This trick gives ####,### but that is the best
            // we can do.
            result.append(kPatternDigit);
            fullInterval.setIntDigitCount(digitsNeeded - 1);
        }
    }
    int32_t maxDigitPos = fullInterval.getMostSignificantExclusive();
    int32_t minDigitPos = fullInterval.getLeastSignificantInclusive();
    for (int32_t i = maxDigitPos - 1; i >= minDigitPos; --i) {
        if (!fOptions.fMantissa.fAlwaysShowDecimal && i == -1) {
            result.append(kPatternDecimalSeparator);
        }
        if (fUseSigDigits) {
            // Use digit symbol
            if (i >= sigMax || i < sigMax - sigMin) {
                result.append(kPatternDigit);
            } else {
                result.append(kPatternSignificantDigit);
            }
        } else {
            if (i < roundingIncrementUpperExp && i >= roundingIncrementLowerExp) {
                result.append((UChar)(fEffPrecision.fMantissa.fRoundingIncrement.getDigitByExponent(i) + kPatternZeroDigit));
            } else if (minInterval.contains(i)) {
                result.append(kPatternZeroDigit);
            } else {
                result.append(kPatternDigit);
            }
        }
        if (grouping.isSeparatorAt(i + 1, i)) {
            result.append(kPatternGroupingSeparator);
        }
        if (fOptions.fMantissa.fAlwaysShowDecimal && i == 0) {
            result.append(kPatternDecimalSeparator);
        }
    }
    if (fUseScientific) {
        result.append(kPatternExponent);
        if (fOptions.fExponent.fAlwaysShowSign) {
            result.append(kPatternPlus);
        }
        for (int32_t i = 0; i < 1 || i < fEffPrecision.fMinExponentDigits; ++i) {
            result.append(kPatternZeroDigit);
        }
    }
    return result;
}

UnicodeString&
DecimalFormatImpl::toPattern(UnicodeString& result) const {
    result.remove();
    UnicodeString padSpec;
    if (fAffixes.fWidth > 0) {
        padSpec.append(kPatternPadEscape);
        padSpec.append(fAffixes.fPadChar);
    }
    if (fAffixes.fPadPosition == DigitAffixesAndPadding::kPadBeforePrefix) {
        result.append(padSpec);
    }
    fPositivePrefixPattern.toUserString(result);
    if (fAffixes.fPadPosition == DigitAffixesAndPadding::kPadAfterPrefix) {
        result.append(padSpec);
    }
    toNumberPattern(
            fAffixes.fWidth > 0,
            fAffixes.fWidth - fPositivePrefixPattern.countChar32() - fPositiveSuffixPattern.countChar32(),
            result);
    if (fAffixes.fPadPosition == DigitAffixesAndPadding::kPadBeforeSuffix) {
        result.append(padSpec);
    }
    fPositiveSuffixPattern.toUserString(result);
    if (fAffixes.fPadPosition == DigitAffixesAndPadding::kPadAfterSuffix) {
        result.append(padSpec);
    }
    AffixPattern withNegative;
    withNegative.add(AffixPattern::kNegative);
    withNegative.append(fPositivePrefixPattern);
    if (!fPositiveSuffixPattern.equals(fNegativeSuffixPattern) ||
            !withNegative.equals(fNegativePrefixPattern)) {
        result.append(kPatternSeparator);
        if (fAffixes.fPadPosition == DigitAffixesAndPadding::kPadBeforePrefix) {
            result.append(padSpec);
        }
        fNegativePrefixPattern.toUserString(result);
        if (fAffixes.fPadPosition == DigitAffixesAndPadding::kPadAfterPrefix) {
            result.append(padSpec);
        }
        toNumberPattern(
                fAffixes.fWidth > 0,
                fAffixes.fWidth - fNegativePrefixPattern.countChar32() - fNegativeSuffixPattern.countChar32(),
                result);
        if (fAffixes.fPadPosition == DigitAffixesAndPadding::kPadBeforeSuffix) {
            result.append(padSpec);
        }
        fNegativeSuffixPattern.toUserString(result);
        if (fAffixes.fPadPosition == DigitAffixesAndPadding::kPadAfterSuffix) {
            result.append(padSpec);
        }
    }
    return result;
}

int32_t
DecimalFormatImpl::getOldFormatWidth() const {
    if (fAffixes.fWidth == 0) {
        return 0;
    }
    return fAffixes.fWidth - fPositiveSuffixPattern.countChar32() - fPositivePrefixPattern.countChar32();
}

const UnicodeString &
DecimalFormatImpl::getConstSymbol(
        DecimalFormatSymbols::ENumberFormatSymbol symbol) const {
   return fSymbols->getConstSymbol(symbol);
}

UBool
DecimalFormatImpl::isParseFastpath() const {
    AffixPattern negative;
    negative.add(AffixPattern::kNegative);

    return fAffixes.fWidth == 0 &&
    fPositivePrefixPattern.countChar32() == 0 &&
    fNegativePrefixPattern.equals(negative) &&
    fPositiveSuffixPattern.countChar32() == 0 &&
    fNegativeSuffixPattern.countChar32() == 0;
}


U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */
