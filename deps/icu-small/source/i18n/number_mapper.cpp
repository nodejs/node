// © 2018 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

// Allow implicit conversion from char16_t* to UnicodeString for this file:
// Helpful in toString methods and elsewhere.
#define UNISTR_FROM_STRING_EXPLICIT

#include "number_mapper.h"
#include "number_patternstring.h"
#include "unicode/errorcode.h"
#include "number_utils.h"
#include "number_currencysymbols.h"

using namespace icu;
using namespace icu::number;
using namespace icu::number::impl;


UnlocalizedNumberFormatter NumberPropertyMapper::create(const DecimalFormatProperties& properties,
                                                        const DecimalFormatSymbols& symbols,
                                                        DecimalFormatWarehouse& warehouse,
                                                        UErrorCode& status) {
    return NumberFormatter::with().macros(oldToNew(properties, symbols, warehouse, nullptr, status));
}

UnlocalizedNumberFormatter NumberPropertyMapper::create(const DecimalFormatProperties& properties,
                                                        const DecimalFormatSymbols& symbols,
                                                        DecimalFormatWarehouse& warehouse,
                                                        DecimalFormatProperties& exportedProperties,
                                                        UErrorCode& status) {
    return NumberFormatter::with().macros(
            oldToNew(
                    properties, symbols, warehouse, &exportedProperties, status));
}

MacroProps NumberPropertyMapper::oldToNew(const DecimalFormatProperties& properties,
                                          const DecimalFormatSymbols& symbols,
                                          DecimalFormatWarehouse& warehouse,
                                          DecimalFormatProperties* exportedProperties,
                                          UErrorCode& status) {
    MacroProps macros;
    Locale locale = symbols.getLocale();

    /////////////
    // SYMBOLS //
    /////////////

    macros.symbols.setTo(symbols);

    //////////////////
    // PLURAL RULES //
    //////////////////

    if (!properties.currencyPluralInfo.fPtr.isNull()) {
        macros.rules = properties.currencyPluralInfo.fPtr->getPluralRules();
    }

    /////////////
    // AFFIXES //
    /////////////

    AffixPatternProvider* affixProvider;
    if (properties.currencyPluralInfo.fPtr.isNull()) {
        warehouse.currencyPluralInfoAPP.setToBogus();
        warehouse.propertiesAPP.setTo(properties, status);
        affixProvider = &warehouse.propertiesAPP;
    } else {
        warehouse.currencyPluralInfoAPP.setTo(*properties.currencyPluralInfo.fPtr, properties, status);
        warehouse.propertiesAPP.setToBogus();
        affixProvider = &warehouse.currencyPluralInfoAPP;
    }
    macros.affixProvider = affixProvider;

    ///////////
    // UNITS //
    ///////////

    bool useCurrency = (
            !properties.currency.isNull() ||
            !properties.currencyPluralInfo.fPtr.isNull() ||
            !properties.currencyUsage.isNull() ||
            affixProvider->hasCurrencySign());
    CurrencyUnit currency = resolveCurrency(properties, locale, status);
    UCurrencyUsage currencyUsage = properties.currencyUsage.getOrDefault(UCURR_USAGE_STANDARD);
    if (useCurrency) {
        // NOTE: Slicing is OK.
        macros.unit = currency; // NOLINT
    }
    warehouse.currencySymbols = {currency, locale, symbols, status};
    macros.currencySymbols = &warehouse.currencySymbols;

    ///////////////////////
    // ROUNDING STRATEGY //
    ///////////////////////

    int32_t maxInt = properties.maximumIntegerDigits;
    int32_t minInt = properties.minimumIntegerDigits;
    int32_t maxFrac = properties.maximumFractionDigits;
    int32_t minFrac = properties.minimumFractionDigits;
    int32_t minSig = properties.minimumSignificantDigits;
    int32_t maxSig = properties.maximumSignificantDigits;
    double roundingIncrement = properties.roundingIncrement;
    RoundingMode roundingMode = properties.roundingMode.getOrDefault(UNUM_ROUND_HALFEVEN);
    bool explicitMinMaxFrac = minFrac != -1 || maxFrac != -1;
    bool explicitMinMaxSig = minSig != -1 || maxSig != -1;
    // Resolve min/max frac for currencies, required for the validation logic and for when minFrac or
    // maxFrac was
    // set (but not both) on a currency instance.
    // NOTE: Increments are handled in "Precision.constructCurrency()".
    if (useCurrency && (minFrac == -1 || maxFrac == -1)) {
        int32_t digits = ucurr_getDefaultFractionDigitsForUsage(
                currency.getISOCurrency(), currencyUsage, &status);
        if (minFrac == -1 && maxFrac == -1) {
            minFrac = digits;
            maxFrac = digits;
        } else if (minFrac == -1) {
            minFrac = std::min(maxFrac, digits);
        } else /* if (maxFrac == -1) */ {
            maxFrac = std::max(minFrac, digits);
        }
    }
    // Validate min/max int/frac.
    // For backwards compatibility, minimum overrides maximum if the two conflict.
    // The following logic ensures that there is always a minimum of at least one digit.
    if (minInt == 0 && maxFrac != 0) {
        // Force a digit after the decimal point.
        minFrac = minFrac <= 0 ? 1 : minFrac;
        maxFrac = maxFrac < 0 ? -1 : maxFrac < minFrac ? minFrac : maxFrac;
        minInt = 0;
        maxInt = maxInt < 0 ? -1 : maxInt > kMaxIntFracSig ? -1 : maxInt;
    } else {
        // Force a digit before the decimal point.
        minFrac = minFrac < 0 ? 0 : minFrac;
        maxFrac = maxFrac < 0 ? -1 : maxFrac < minFrac ? minFrac : maxFrac;
        minInt = minInt <= 0 ? 1 : minInt > kMaxIntFracSig ? 1 : minInt;
        maxInt = maxInt < 0 ? -1 : maxInt < minInt ? minInt : maxInt > kMaxIntFracSig ? -1 : maxInt;
    }
    Precision precision;
    if (!properties.currencyUsage.isNull()) {
        precision = Precision::constructCurrency(currencyUsage).withCurrency(currency);
    } else if (roundingIncrement != 0.0) {
        if (PatternStringUtils::ignoreRoundingIncrement(roundingIncrement, maxFrac)) {
            precision = Precision::constructFraction(minFrac, maxFrac);
        } else {
            precision = Precision::constructIncrement(roundingIncrement, minFrac);
        }
    } else if (explicitMinMaxSig) {
        minSig = minSig < 1 ? 1 : minSig > kMaxIntFracSig ? kMaxIntFracSig : minSig;
        maxSig = maxSig < 0 ? kMaxIntFracSig : maxSig < minSig ? minSig : maxSig > kMaxIntFracSig
                                                                          ? kMaxIntFracSig : maxSig;
        precision = Precision::constructSignificant(minSig, maxSig);
    } else if (explicitMinMaxFrac) {
        precision = Precision::constructFraction(minFrac, maxFrac);
    } else if (useCurrency) {
        precision = Precision::constructCurrency(currencyUsage);
    }
    if (!precision.isBogus()) {
        precision.fRoundingMode = roundingMode;
        macros.precision = precision;
    }

    ///////////////////
    // INTEGER WIDTH //
    ///////////////////

    macros.integerWidth = IntegerWidth(
            static_cast<digits_t>(minInt),
            static_cast<digits_t>(maxInt),
            properties.formatFailIfMoreThanMaxDigits);

    ///////////////////////
    // GROUPING STRATEGY //
    ///////////////////////

    macros.grouper = Grouper::forProperties(properties);

    /////////////
    // PADDING //
    /////////////

    if (properties.formatWidth > 0) {
        macros.padder = Padder::forProperties(properties);
    }

    ///////////////////////////////
    // DECIMAL MARK ALWAYS SHOWN //
    ///////////////////////////////

    macros.decimal = properties.decimalSeparatorAlwaysShown ? UNUM_DECIMAL_SEPARATOR_ALWAYS
                                                            : UNUM_DECIMAL_SEPARATOR_AUTO;

    ///////////////////////
    // SIGN ALWAYS SHOWN //
    ///////////////////////

    macros.sign = properties.signAlwaysShown ? UNUM_SIGN_ALWAYS : UNUM_SIGN_AUTO;

    /////////////////////////
    // SCIENTIFIC NOTATION //
    /////////////////////////

    if (properties.minimumExponentDigits != -1) {
        // Scientific notation is required.
        // This whole section feels like a hack, but it is needed for regression tests.
        // The mapping from property bag to scientific notation is nontrivial due to LDML rules.
        if (maxInt > 8) {
            // But #13110: The maximum of 8 digits has unknown origins and is not in the spec.
            // If maxInt is greater than 8, it is set to minInt, even if minInt is greater than 8.
            maxInt = minInt;
            macros.integerWidth = IntegerWidth::zeroFillTo(minInt).truncateAt(maxInt);
        } else if (maxInt > minInt && minInt > 1) {
            // Bug #13289: if maxInt > minInt > 1, then minInt should be 1.
            minInt = 1;
            macros.integerWidth = IntegerWidth::zeroFillTo(minInt).truncateAt(maxInt);
        }
        int engineering = maxInt < 0 ? -1 : maxInt;
        macros.notation = ScientificNotation(
                // Engineering interval:
                static_cast<int8_t>(engineering),
                // Enforce minimum integer digits (for patterns like "000.00E0"):
                (engineering == minInt),
                // Minimum exponent digits:
                static_cast<digits_t>(properties.minimumExponentDigits),
                // Exponent sign always shown:
                properties.exponentSignAlwaysShown ? UNUM_SIGN_ALWAYS : UNUM_SIGN_AUTO);
        // Scientific notation also involves overriding the rounding mode.
        // TODO: Overriding here is a bit of a hack. Should this logic go earlier?
        if (macros.precision.fType == Precision::PrecisionType::RND_FRACTION) {
            // For the purposes of rounding, get the original min/max int/frac, since the local
            // variables have been manipulated for display purposes.
            int maxInt_ = properties.maximumIntegerDigits;
            int minInt_ = properties.minimumIntegerDigits;
            int minFrac_ = properties.minimumFractionDigits;
            int maxFrac_ = properties.maximumFractionDigits;
            if (minInt_ == 0 && maxFrac_ == 0) {
                // Patterns like "#E0" and "##E0", which mean no rounding!
                macros.precision = Precision::unlimited();
            } else if (minInt_ == 0 && minFrac_ == 0) {
                // Patterns like "#.##E0" (no zeros in the mantissa), which mean round to maxFrac+1
                macros.precision = Precision::constructSignificant(1, maxFrac_ + 1);
            } else {
                int maxSig_ = minInt_ + maxFrac_;
                // Bug #20058: if maxInt_ > minInt_ > 1, then minInt_ should be 1.
                if (maxInt_ > minInt_ && minInt_ > 1) {
                    minInt_ = 1;
                }
                int minSig_ = minInt_ + minFrac_;
                // To avoid regression, maxSig is not reset when minInt_ set to 1.
                // TODO: Reset maxSig_ = 1 + minFrac_ to follow the spec.
                macros.precision = Precision::constructSignificant(minSig_, maxSig_);
            }
            macros.precision.fRoundingMode = roundingMode;
        }
    }

    //////////////////////
    // COMPACT NOTATION //
    //////////////////////

    if (!properties.compactStyle.isNull()) {
        if (properties.compactStyle.getNoError() == UNumberCompactStyle::UNUM_LONG) {
            macros.notation = Notation::compactLong();
        } else {
            macros.notation = Notation::compactShort();
        }
        // Do not forward the affix provider.
        macros.affixProvider = nullptr;
    }

    /////////////////
    // MULTIPLIERS //
    /////////////////

    macros.scale = scaleFromProperties(properties);

    //////////////////////
    // PROPERTY EXPORTS //
    //////////////////////

    if (exportedProperties != nullptr) {

        exportedProperties->currency = currency;
        exportedProperties->roundingMode = roundingMode;
        exportedProperties->minimumIntegerDigits = minInt;
        exportedProperties->maximumIntegerDigits = maxInt == -1 ? INT32_MAX : maxInt;

        Precision rounding_;
        if (precision.fType == Precision::PrecisionType::RND_CURRENCY) {
            rounding_ = precision.withCurrency(currency, status);
        } else {
            rounding_ = precision;
        }
        int minFrac_ = minFrac;
        int maxFrac_ = maxFrac;
        int minSig_ = minSig;
        int maxSig_ = maxSig;
        double increment_ = 0.0;
        if (rounding_.fType == Precision::PrecisionType::RND_FRACTION) {
            minFrac_ = rounding_.fUnion.fracSig.fMinFrac;
            maxFrac_ = rounding_.fUnion.fracSig.fMaxFrac;
        } else if (rounding_.fType == Precision::PrecisionType::RND_INCREMENT
                || rounding_.fType == Precision::PrecisionType::RND_INCREMENT_ONE
                || rounding_.fType == Precision::PrecisionType::RND_INCREMENT_FIVE) {
            increment_ = rounding_.fUnion.increment.fIncrement;
            minFrac_ = rounding_.fUnion.increment.fMinFrac;
            maxFrac_ = rounding_.fUnion.increment.fMinFrac;
        } else if (rounding_.fType == Precision::PrecisionType::RND_SIGNIFICANT) {
            minSig_ = rounding_.fUnion.fracSig.fMinSig;
            maxSig_ = rounding_.fUnion.fracSig.fMaxSig;
        }

        exportedProperties->minimumFractionDigits = minFrac_;
        exportedProperties->maximumFractionDigits = maxFrac_;
        exportedProperties->minimumSignificantDigits = minSig_;
        exportedProperties->maximumSignificantDigits = maxSig_;
        exportedProperties->roundingIncrement = increment_;
    }

    return macros;
}


void PropertiesAffixPatternProvider::setTo(const DecimalFormatProperties& properties, UErrorCode& status) {
    fBogus = false;

    // There are two ways to set affixes in DecimalFormat: via the pattern string (applyPattern), and via the
    // explicit setters (setPositivePrefix and friends).  The way to resolve the settings is as follows:
    //
    // 1) If the explicit setting is present for the field, use it.
    // 2) Otherwise, follows UTS 35 rules based on the pattern string.
    //
    // Importantly, the explicit setters affect only the one field they override.  If you set the positive
    // prefix, that should not affect the negative prefix.

    // Convenience: Extract the properties into local variables.
    // Variables are named with three chars: [p/n][p/s][o/p]
    // [p/n] => p for positive, n for negative
    // [p/s] => p for prefix, s for suffix
    // [o/p] => o for escaped custom override string, p for pattern string
    UnicodeString ppo = AffixUtils::escape(properties.positivePrefix);
    UnicodeString pso = AffixUtils::escape(properties.positiveSuffix);
    UnicodeString npo = AffixUtils::escape(properties.negativePrefix);
    UnicodeString nso = AffixUtils::escape(properties.negativeSuffix);
    const UnicodeString& ppp = properties.positivePrefixPattern;
    const UnicodeString& psp = properties.positiveSuffixPattern;
    const UnicodeString& npp = properties.negativePrefixPattern;
    const UnicodeString& nsp = properties.negativeSuffixPattern;

    if (!properties.positivePrefix.isBogus()) {
        posPrefix = ppo;
    } else if (!ppp.isBogus()) {
        posPrefix = ppp;
    } else {
        // UTS 35: Default positive prefix is empty string.
        posPrefix = u"";
    }

    if (!properties.positiveSuffix.isBogus()) {
        posSuffix = pso;
    } else if (!psp.isBogus()) {
        posSuffix = psp;
    } else {
        // UTS 35: Default positive suffix is empty string.
        posSuffix = u"";
    }

    if (!properties.negativePrefix.isBogus()) {
        negPrefix = npo;
    } else if (!npp.isBogus()) {
        negPrefix = npp;
    } else {
        // UTS 35: Default negative prefix is "-" with positive prefix.
        // Important: We prepend the "-" to the pattern, not the override!
        negPrefix = ppp.isBogus() ? u"-" : u"-" + ppp;
    }

    if (!properties.negativeSuffix.isBogus()) {
        negSuffix = nso;
    } else if (!nsp.isBogus()) {
        negSuffix = nsp;
    } else {
        // UTS 35: Default negative prefix is the positive prefix.
        negSuffix = psp.isBogus() ? u"" : psp;
    }

    // For declaring if this is a currency pattern, we need to look at the
    // original pattern, not at any user-specified overrides.
    isCurrencyPattern = (
        AffixUtils::hasCurrencySymbols(ppp, status) ||
        AffixUtils::hasCurrencySymbols(psp, status) ||
        AffixUtils::hasCurrencySymbols(npp, status) ||
        AffixUtils::hasCurrencySymbols(nsp, status));
}

char16_t PropertiesAffixPatternProvider::charAt(int flags, int i) const {
    return getStringInternal(flags).charAt(i);
}

int PropertiesAffixPatternProvider::length(int flags) const {
    return getStringInternal(flags).length();
}

UnicodeString PropertiesAffixPatternProvider::getString(int32_t flags) const {
    return getStringInternal(flags);
}

const UnicodeString& PropertiesAffixPatternProvider::getStringInternal(int32_t flags) const {
    bool prefix = (flags & AFFIX_PREFIX) != 0;
    bool negative = (flags & AFFIX_NEGATIVE_SUBPATTERN) != 0;
    if (prefix && negative) {
        return negPrefix;
    } else if (prefix) {
        return posPrefix;
    } else if (negative) {
        return negSuffix;
    } else {
        return posSuffix;
    }
}

bool PropertiesAffixPatternProvider::positiveHasPlusSign() const {
    // TODO: Change the internal APIs to propagate out the error?
    ErrorCode localStatus;
    return AffixUtils::containsType(posPrefix, TYPE_PLUS_SIGN, localStatus) ||
           AffixUtils::containsType(posSuffix, TYPE_PLUS_SIGN, localStatus);
}

bool PropertiesAffixPatternProvider::hasNegativeSubpattern() const {
    return (
        (negSuffix != posSuffix) ||
        negPrefix.tempSubString(1) != posPrefix ||
        negPrefix.charAt(0) != u'-'
    );
}

bool PropertiesAffixPatternProvider::negativeHasMinusSign() const {
    ErrorCode localStatus;
    return AffixUtils::containsType(negPrefix, TYPE_MINUS_SIGN, localStatus) ||
           AffixUtils::containsType(negSuffix, TYPE_MINUS_SIGN, localStatus);
}

bool PropertiesAffixPatternProvider::hasCurrencySign() const {
    return isCurrencyPattern;
}

bool PropertiesAffixPatternProvider::containsSymbolType(AffixPatternType type, UErrorCode& status) const {
    return AffixUtils::containsType(posPrefix, type, status) ||
           AffixUtils::containsType(posSuffix, type, status) ||
           AffixUtils::containsType(negPrefix, type, status) ||
           AffixUtils::containsType(negSuffix, type, status);
}

bool PropertiesAffixPatternProvider::hasBody() const {
    return true;
}


void CurrencyPluralInfoAffixProvider::setTo(const CurrencyPluralInfo& cpi,
                                            const DecimalFormatProperties& properties,
                                            UErrorCode& status) {
    // We need to use a PropertiesAffixPatternProvider, not the simpler version ParsedPatternInfo,
    // because user-specified affix overrides still need to work.
    fBogus = false;
    DecimalFormatProperties pluralProperties(properties);
    for (int32_t plural = 0; plural < StandardPlural::COUNT; plural++) {
        const char* keyword = StandardPlural::getKeyword(static_cast<StandardPlural::Form>(plural));
        UnicodeString patternString;
        patternString = cpi.getCurrencyPluralPattern(keyword, patternString);
        PatternParser::parseToExistingProperties(
                patternString,
                pluralProperties,
                IGNORE_ROUNDING_NEVER,
                status);
        affixesByPlural[plural].setTo(pluralProperties, status);
    }
}

char16_t CurrencyPluralInfoAffixProvider::charAt(int32_t flags, int32_t i) const {
    int32_t pluralOrdinal = (flags & AFFIX_PLURAL_MASK);
    return affixesByPlural[pluralOrdinal].charAt(flags, i);
}

int32_t CurrencyPluralInfoAffixProvider::length(int32_t flags) const {
    int32_t pluralOrdinal = (flags & AFFIX_PLURAL_MASK);
    return affixesByPlural[pluralOrdinal].length(flags);
}

UnicodeString CurrencyPluralInfoAffixProvider::getString(int32_t flags) const {
    int32_t pluralOrdinal = (flags & AFFIX_PLURAL_MASK);
    return affixesByPlural[pluralOrdinal].getString(flags);
}

bool CurrencyPluralInfoAffixProvider::positiveHasPlusSign() const {
    return affixesByPlural[StandardPlural::OTHER].positiveHasPlusSign();
}

bool CurrencyPluralInfoAffixProvider::hasNegativeSubpattern() const {
    return affixesByPlural[StandardPlural::OTHER].hasNegativeSubpattern();
}

bool CurrencyPluralInfoAffixProvider::negativeHasMinusSign() const {
    return affixesByPlural[StandardPlural::OTHER].negativeHasMinusSign();
}

bool CurrencyPluralInfoAffixProvider::hasCurrencySign() const {
    return affixesByPlural[StandardPlural::OTHER].hasCurrencySign();
}

bool CurrencyPluralInfoAffixProvider::containsSymbolType(AffixPatternType type, UErrorCode& status) const {
    return affixesByPlural[StandardPlural::OTHER].containsSymbolType(type, status);
}

bool CurrencyPluralInfoAffixProvider::hasBody() const {
    return affixesByPlural[StandardPlural::OTHER].hasBody();
}


#endif /* #if !UCONFIG_NO_FORMATTING */
