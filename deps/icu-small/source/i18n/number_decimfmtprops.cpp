// © 2017 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "number_decimfmtprops.h"
#include "umutex.h"

using namespace icu;
using namespace icu::number;
using namespace icu::number::impl;


namespace {

alignas(DecimalFormatProperties)
char kRawDefaultProperties[sizeof(DecimalFormatProperties)];

icu::UInitOnce gDefaultPropertiesInitOnce = U_INITONCE_INITIALIZER;

void U_CALLCONV initDefaultProperties(UErrorCode&) {
    new(kRawDefaultProperties) DecimalFormatProperties(); // set to the default instance
}

}


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
    formatFailIfMoreThanMaxDigits = false;
    formatWidth = -1;
    groupingSize = -1;
    groupingUsed = true;
    magnitudeMultiplier = 0;
    maximumFractionDigits = -1;
    maximumIntegerDigits = -1;
    maximumSignificantDigits = -1;
    minimumExponentDigits = -1;
    minimumFractionDigits = -1;
    minimumGroupingDigits = -1;
    minimumIntegerDigits = -1;
    minimumSignificantDigits = -1;
    multiplier = 1;
    multiplierScale = 0;
    negativePrefix.setToBogus();
    negativePrefixPattern.setToBogus();
    negativeSuffix.setToBogus();
    negativeSuffixPattern.setToBogus();
    padPosition.nullify();
    padString.setToBogus();
    parseCaseSensitive = false;
    parseIntegerOnly = false;
    parseMode.nullify();
    parseNoExponent = false;
    parseToBigDecimal = false;
    parseAllInput = UNUM_MAYBE;
    positivePrefix.setToBogus();
    positivePrefixPattern.setToBogus();
    positiveSuffix.setToBogus();
    positiveSuffixPattern.setToBogus();
    roundingIncrement = 0.0;
    roundingMode.nullify();
    secondaryGroupingSize = -1;
    signAlwaysShown = false;
}

bool
DecimalFormatProperties::_equals(const DecimalFormatProperties& other, bool ignoreForFastFormat) const {
    bool eq = true;

    // Properties that must be equal both normally and for fast-path formatting
    eq = eq && compactStyle == other.compactStyle;
    eq = eq && currency == other.currency;
    eq = eq && currencyPluralInfo.fPtr.getAlias() == other.currencyPluralInfo.fPtr.getAlias();
    eq = eq && currencyUsage == other.currencyUsage;
    eq = eq && decimalSeparatorAlwaysShown == other.decimalSeparatorAlwaysShown;
    eq = eq && exponentSignAlwaysShown == other.exponentSignAlwaysShown;
    eq = eq && formatFailIfMoreThanMaxDigits == other.formatFailIfMoreThanMaxDigits;
    eq = eq && formatWidth == other.formatWidth;
    eq = eq && magnitudeMultiplier == other.magnitudeMultiplier;
    eq = eq && maximumSignificantDigits == other.maximumSignificantDigits;
    eq = eq && minimumExponentDigits == other.minimumExponentDigits;
    eq = eq && minimumGroupingDigits == other.minimumGroupingDigits;
    eq = eq && minimumSignificantDigits == other.minimumSignificantDigits;
    eq = eq && multiplier == other.multiplier;
    eq = eq && multiplierScale == other.multiplierScale;
    eq = eq && negativePrefix == other.negativePrefix;
    eq = eq && negativeSuffix == other.negativeSuffix;
    eq = eq && padPosition == other.padPosition;
    eq = eq && padString == other.padString;
    eq = eq && positivePrefix == other.positivePrefix;
    eq = eq && positiveSuffix == other.positiveSuffix;
    eq = eq && roundingIncrement == other.roundingIncrement;
    eq = eq && roundingMode == other.roundingMode;
    eq = eq && secondaryGroupingSize == other.secondaryGroupingSize;
    eq = eq && signAlwaysShown == other.signAlwaysShown;

    if (ignoreForFastFormat) {
        return eq;
    }

    // Properties ignored by fast-path formatting
    // Formatting (special handling required):
    eq = eq && groupingSize == other.groupingSize;
    eq = eq && groupingUsed == other.groupingUsed;
    eq = eq && minimumFractionDigits == other.minimumFractionDigits;
    eq = eq && maximumFractionDigits == other.maximumFractionDigits;
    eq = eq && maximumIntegerDigits == other.maximumIntegerDigits;
    eq = eq && minimumIntegerDigits == other.minimumIntegerDigits;
    eq = eq && negativePrefixPattern == other.negativePrefixPattern;
    eq = eq && negativeSuffixPattern == other.negativeSuffixPattern;
    eq = eq && positivePrefixPattern == other.positivePrefixPattern;
    eq = eq && positiveSuffixPattern == other.positiveSuffixPattern;

    // Parsing (always safe to ignore):
    eq = eq && decimalPatternMatchRequired == other.decimalPatternMatchRequired;
    eq = eq && parseCaseSensitive == other.parseCaseSensitive;
    eq = eq && parseIntegerOnly == other.parseIntegerOnly;
    eq = eq && parseMode == other.parseMode;
    eq = eq && parseNoExponent == other.parseNoExponent;
    eq = eq && parseToBigDecimal == other.parseToBigDecimal;
    eq = eq && parseAllInput == other.parseAllInput;

    return eq;
}

bool DecimalFormatProperties::equalsDefaultExceptFastFormat() const {
    UErrorCode localStatus = U_ZERO_ERROR;
    umtx_initOnce(gDefaultPropertiesInitOnce, &initDefaultProperties, localStatus);
    return _equals(*reinterpret_cast<DecimalFormatProperties*>(kRawDefaultProperties), true);
}

#endif /* #if !UCONFIG_NO_FORMATTING */
