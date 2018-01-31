// © 2017 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING && !UPRV_INCOMPLETE_CPP11_SUPPORT

#include "number_decimfmtprops.h"

using namespace icu;
using namespace icu::number;
using namespace icu::number::impl;

DecimalFormatProperties::DecimalFormatProperties() {
    clear();
}

void DecimalFormatProperties::clear() {
    compactStyle.nullify();
    currency.nullify();
    currencyPluralInfo.fPtr.adoptInstead(nullptr);
    currencyUsage.nullify();
    decimalPatternMatchRequired = false;
    decimalSeparatorAlwaysShown = false;
    exponentSignAlwaysShown = false;
    formatWidth = -1;
    groupingSize = -1;
    magnitudeMultiplier = 0;
    maximumFractionDigits = -1;
    maximumIntegerDigits = -1;
    maximumSignificantDigits = -1;
    minimumExponentDigits = -1;
    minimumFractionDigits = -1;
    minimumGroupingDigits = -1;
    minimumIntegerDigits = -1;
    minimumSignificantDigits = -1;
    multiplier = 0;
    negativePrefix.setToBogus();
    negativePrefixPattern.setToBogus();
    negativeSuffix.setToBogus();
    negativeSuffixPattern.setToBogus();
    padPosition.nullify();
    padString.setToBogus();
    parseCaseSensitive = false;
    parseIntegerOnly = false;
    parseLenient = false;
    parseNoExponent = false;
    parseToBigDecimal = false;
    positivePrefix.setToBogus();
    positivePrefixPattern.setToBogus();
    positiveSuffix.setToBogus();
    positiveSuffixPattern.setToBogus();
    roundingIncrement = 0.0;
    roundingMode.nullify();
    secondaryGroupingSize = -1;
    signAlwaysShown = false;
}

bool DecimalFormatProperties::operator==(const DecimalFormatProperties &other) const {
    bool eq = true;
    eq = eq && compactStyle == other.compactStyle;
    eq = eq && currency == other.currency;
    eq = eq && currencyPluralInfo.fPtr.getAlias() == other.currencyPluralInfo.fPtr.getAlias();
    eq = eq && currencyUsage == other.currencyUsage;
    eq = eq && decimalPatternMatchRequired == other.decimalPatternMatchRequired;
    eq = eq && decimalSeparatorAlwaysShown == other.decimalSeparatorAlwaysShown;
    eq = eq && exponentSignAlwaysShown == other.exponentSignAlwaysShown;
    eq = eq && formatWidth == other.formatWidth;
    eq = eq && groupingSize == other.groupingSize;
    eq = eq && magnitudeMultiplier == other.magnitudeMultiplier;
    eq = eq && maximumFractionDigits == other.maximumFractionDigits;
    eq = eq && maximumIntegerDigits == other.maximumIntegerDigits;
    eq = eq && maximumSignificantDigits == other.maximumSignificantDigits;
    eq = eq && minimumExponentDigits == other.minimumExponentDigits;
    eq = eq && minimumFractionDigits == other.minimumFractionDigits;
    eq = eq && minimumGroupingDigits == other.minimumGroupingDigits;
    eq = eq && minimumIntegerDigits == other.minimumIntegerDigits;
    eq = eq && minimumSignificantDigits == other.minimumSignificantDigits;
    eq = eq && multiplier == other.multiplier;
    eq = eq && negativePrefix == other.negativePrefix;
    eq = eq && negativePrefixPattern == other.negativePrefixPattern;
    eq = eq && negativeSuffix == other.negativeSuffix;
    eq = eq && negativeSuffixPattern == other.negativeSuffixPattern;
    eq = eq && padPosition == other.padPosition;
    eq = eq && padString == other.padString;
    eq = eq && parseCaseSensitive == other.parseCaseSensitive;
    eq = eq && parseIntegerOnly == other.parseIntegerOnly;
    eq = eq && parseLenient == other.parseLenient;
    eq = eq && parseNoExponent == other.parseNoExponent;
    eq = eq && parseToBigDecimal == other.parseToBigDecimal;
    eq = eq && positivePrefix == other.positivePrefix;
    eq = eq && positivePrefixPattern == other.positivePrefixPattern;
    eq = eq && positiveSuffix == other.positiveSuffix;
    eq = eq && positiveSuffixPattern == other.positiveSuffixPattern;
    eq = eq && roundingIncrement == other.roundingIncrement;
    eq = eq && roundingMode == other.roundingMode;
    eq = eq && secondaryGroupingSize == other.secondaryGroupingSize;
    eq = eq && signAlwaysShown == other.signAlwaysShown;
    return eq;
}

#endif /* #if !UCONFIG_NO_FORMATTING */
