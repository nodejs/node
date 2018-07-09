// © 2017 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "cstring.h"
#include "unicode/ures.h"
#include "uresimp.h"
#include "charstr.h"
#include "number_formatimpl.h"
#include "unicode/numfmt.h"
#include "number_patternstring.h"
#include "number_utils.h"
#include "unicode/numberformatter.h"
#include "unicode/dcfmtsym.h"
#include "number_scientific.h"
#include "number_compact.h"
#include "uresimp.h"
#include "ureslocs.h"

using namespace icu;
using namespace icu::number;
using namespace icu::number::impl;

namespace {

struct CurrencyFormatInfoResult {
    bool exists;
    const char16_t* pattern;
    const char16_t* decimalSeparator;
    const char16_t* groupingSeparator;
};

CurrencyFormatInfoResult
getCurrencyFormatInfo(const Locale& locale, const char* isoCode, UErrorCode& status) {
    // TODO: Load this data in a centralized location like ICU4J?
    // TODO: Move this into the CurrencySymbols class?
    // TODO: Parts of this same data are loaded in dcfmtsym.cpp; should clean up.
    CurrencyFormatInfoResult result = {false, nullptr, nullptr, nullptr};
    if (U_FAILURE(status)) { return result; }
    CharString key;
    key.append("Currencies/", status);
    key.append(isoCode, status);
    UErrorCode localStatus = status;
    LocalUResourceBundlePointer bundle(ures_open(U_ICUDATA_CURR, locale.getName(), &localStatus));
    ures_getByKeyWithFallback(bundle.getAlias(), key.data(), bundle.getAlias(), &localStatus);
    if (U_SUCCESS(localStatus) &&
        ures_getSize(bundle.getAlias()) > 2) { // the length is 3 if more data is present
        ures_getByIndex(bundle.getAlias(), 2, bundle.getAlias(), &localStatus);
        int32_t dummy;
        result.exists = true;
        result.pattern = ures_getStringByIndex(bundle.getAlias(), 0, &dummy, &localStatus);
        result.decimalSeparator = ures_getStringByIndex(bundle.getAlias(), 1, &dummy, &localStatus);
        result.groupingSeparator = ures_getStringByIndex(bundle.getAlias(), 2, &dummy, &localStatus);
        status = localStatus;
    } else if (localStatus != U_MISSING_RESOURCE_ERROR) {
        status = localStatus;
    }
    return result;
}

}  // namespace


MicroPropsGenerator::~MicroPropsGenerator() = default;


NumberFormatterImpl* NumberFormatterImpl::fromMacros(const MacroProps& macros, UErrorCode& status) {
    return new NumberFormatterImpl(macros, true, status);
}

void NumberFormatterImpl::applyStatic(const MacroProps& macros, DecimalQuantity& inValue,
                                      NumberStringBuilder& outString, UErrorCode& status) {
    NumberFormatterImpl impl(macros, false, status);
    impl.applyUnsafe(inValue, outString, status);
}

int32_t NumberFormatterImpl::getPrefixSuffixStatic(const MacroProps& macros, int8_t signum,
                                                   StandardPlural::Form plural,
                                                   NumberStringBuilder& outString, UErrorCode& status) {
    NumberFormatterImpl impl(macros, false, status);
    return impl.getPrefixSuffixUnsafe(signum, plural, outString, status);
}

// NOTE: C++ SPECIFIC DIFFERENCE FROM JAVA:
// The "safe" apply method uses a new MicroProps. In the MicroPropsGenerator, fMicros is copied into the new instance.
// The "unsafe" method simply re-uses fMicros, eliminating the extra copy operation.
// See MicroProps::processQuantity() for details.

void NumberFormatterImpl::apply(DecimalQuantity& inValue, NumberStringBuilder& outString,
                                UErrorCode& status) const {
    if (U_FAILURE(status)) { return; }
    MicroProps micros;
    if (!fMicroPropsGenerator) { return; }
    fMicroPropsGenerator->processQuantity(inValue, micros, status);
    if (U_FAILURE(status)) { return; }
    microsToString(micros, inValue, outString, status);
}

void NumberFormatterImpl::applyUnsafe(DecimalQuantity& inValue, NumberStringBuilder& outString,
                                      UErrorCode& status) {
    if (U_FAILURE(status)) { return; }
    fMicroPropsGenerator->processQuantity(inValue, fMicros, status);
    if (U_FAILURE(status)) { return; }
    microsToString(fMicros, inValue, outString, status);
}

int32_t NumberFormatterImpl::getPrefixSuffix(int8_t signum, StandardPlural::Form plural,
                                             NumberStringBuilder& outString, UErrorCode& status) const {
    if (U_FAILURE(status)) { return 0; }
    // #13453: DecimalFormat wants the affixes from the pattern only (modMiddle, aka pattern modifier).
    // Safe path: use fImmutablePatternModifier.
    const Modifier* modifier = fImmutablePatternModifier->getModifier(signum, plural);
    modifier->apply(outString, 0, 0, status);
    if (U_FAILURE(status)) { return 0; }
    return modifier->getPrefixLength(status);
}

int32_t NumberFormatterImpl::getPrefixSuffixUnsafe(int8_t signum, StandardPlural::Form plural,
                                                   NumberStringBuilder& outString, UErrorCode& status) {
    if (U_FAILURE(status)) { return 0; }
    // #13453: DecimalFormat wants the affixes from the pattern only (modMiddle, aka pattern modifier).
    // Unsafe path: use fPatternModifier.
    fPatternModifier->setNumberProperties(signum, plural);
    fPatternModifier->apply(outString, 0, 0, status);
    if (U_FAILURE(status)) { return 0; }
    return fPatternModifier->getPrefixLength(status);
}

NumberFormatterImpl::NumberFormatterImpl(const MacroProps& macros, bool safe, UErrorCode& status) {
    fMicroPropsGenerator = macrosToMicroGenerator(macros, safe, status);
}

//////////

const MicroPropsGenerator*
NumberFormatterImpl::macrosToMicroGenerator(const MacroProps& macros, bool safe, UErrorCode& status) {
    if (U_FAILURE(status)) { return nullptr; }
    const MicroPropsGenerator* chain = &fMicros;

    // Check that macros is error-free before continuing.
    if (macros.copyErrorTo(status)) {
        return nullptr;
    }

    // TODO: Accept currency symbols from DecimalFormatSymbols?

    // Pre-compute a few values for efficiency.
    bool isCurrency = utils::unitIsCurrency(macros.unit);
    bool isNoUnit = utils::unitIsNoUnit(macros.unit);
    bool isPercent = isNoUnit && utils::unitIsPercent(macros.unit);
    bool isPermille = isNoUnit && utils::unitIsPermille(macros.unit);
    bool isCldrUnit = !isCurrency && !isNoUnit;
    bool isAccounting =
            macros.sign == UNUM_SIGN_ACCOUNTING || macros.sign == UNUM_SIGN_ACCOUNTING_ALWAYS ||
            macros.sign == UNUM_SIGN_ACCOUNTING_EXCEPT_ZERO;
    CurrencyUnit currency(nullptr, status);
    if (isCurrency) {
        currency = CurrencyUnit(macros.unit, status); // Restore CurrencyUnit from MeasureUnit
    }
    const CurrencySymbols* currencySymbols;
    if (macros.currencySymbols != nullptr) {
        // Used by the DecimalFormat code path
        currencySymbols = macros.currencySymbols;
    } else {
        fWarehouse.fCurrencySymbols = {currency, macros.locale, status};
        currencySymbols = &fWarehouse.fCurrencySymbols;
    }
    UNumberUnitWidth unitWidth = UNUM_UNIT_WIDTH_SHORT;
    if (macros.unitWidth != UNUM_UNIT_WIDTH_COUNT) {
        unitWidth = macros.unitWidth;
    }

    // Select the numbering system.
    LocalPointer<const NumberingSystem> nsLocal;
    const NumberingSystem* ns;
    if (macros.symbols.isNumberingSystem()) {
        ns = macros.symbols.getNumberingSystem();
    } else {
        // TODO: Is there a way to avoid creating the NumberingSystem object?
        ns = NumberingSystem::createInstance(macros.locale, status);
        // Give ownership to the function scope.
        nsLocal.adoptInstead(ns);
    }
    const char* nsName = U_SUCCESS(status) ? ns->getName() : "latn";

    // Resolve the symbols. Do this here because currency may need to customize them.
    if (macros.symbols.isDecimalFormatSymbols()) {
        fMicros.symbols = macros.symbols.getDecimalFormatSymbols();
    } else {
        fMicros.symbols = new DecimalFormatSymbols(macros.locale, *ns, status);
        // Give ownership to the NumberFormatterImpl.
        fSymbols.adoptInstead(fMicros.symbols);
    }

    // Load and parse the pattern string. It is used for grouping sizes and affixes only.
    // If we are formatting currency, check for a currency-specific pattern.
    const char16_t* pattern = nullptr;
    if (isCurrency) {
        CurrencyFormatInfoResult info = getCurrencyFormatInfo(
                macros.locale, currency.getSubtype(), status);
        if (info.exists) {
            pattern = info.pattern;
            // It's clunky to clone an object here, but this code is not frequently executed.
            auto* symbols = new DecimalFormatSymbols(*fMicros.symbols);
            fMicros.symbols = symbols;
            fSymbols.adoptInstead(symbols);
            symbols->setSymbol(
                    DecimalFormatSymbols::ENumberFormatSymbol::kMonetarySeparatorSymbol,
                    UnicodeString(info.decimalSeparator),
                    FALSE);
            symbols->setSymbol(
                    DecimalFormatSymbols::ENumberFormatSymbol::kMonetaryGroupingSeparatorSymbol,
                    UnicodeString(info.groupingSeparator),
                    FALSE);
        }
    }
    if (pattern == nullptr) {
        CldrPatternStyle patternStyle;
        if (isPercent || isPermille) {
            patternStyle = CLDR_PATTERN_STYLE_PERCENT;
        } else if (!isCurrency || unitWidth == UNUM_UNIT_WIDTH_FULL_NAME) {
            patternStyle = CLDR_PATTERN_STYLE_DECIMAL;
        } else if (isAccounting) {
            // NOTE: Although ACCOUNTING and ACCOUNTING_ALWAYS are only supported in currencies right now,
            // the API contract allows us to add support to other units in the future.
            patternStyle = CLDR_PATTERN_STYLE_ACCOUNTING;
        } else {
            patternStyle = CLDR_PATTERN_STYLE_CURRENCY;
        }
        pattern = utils::getPatternForStyle(macros.locale, nsName, patternStyle, status);
    }
    auto patternInfo = new ParsedPatternInfo();
    fPatternInfo.adoptInstead(patternInfo);
    PatternParser::parseToPatternInfo(UnicodeString(pattern), *patternInfo, status);

    /////////////////////////////////////////////////////////////////////////////////////
    /// START POPULATING THE DEFAULT MICROPROPS AND BUILDING THE MICROPROPS GENERATOR ///
    /////////////////////////////////////////////////////////////////////////////////////

    // Multiplier
    if (macros.scale.isValid()) {
        fMicros.helpers.multiplier.setAndChain(macros.scale, chain);
        chain = &fMicros.helpers.multiplier;
    }

    // Rounding strategy
    Precision precision;
    if (!macros.precision.isBogus()) {
        precision = macros.precision;
    } else if (macros.notation.fType == Notation::NTN_COMPACT) {
        precision = Precision::integer().withMinDigits(2);
    } else if (isCurrency) {
        precision = Precision::currency(UCURR_USAGE_STANDARD);
    } else {
        precision = Precision::maxFraction(6);
    }
    UNumberFormatRoundingMode roundingMode;
    if (macros.roundingMode != kDefaultMode) {
        roundingMode = macros.roundingMode;
    } else {
        // Temporary until ICU 64
        roundingMode = precision.fRoundingMode;
    }
    fMicros.rounder = {precision, roundingMode, currency, status};

    // Grouping strategy
    if (!macros.grouper.isBogus()) {
        fMicros.grouping = macros.grouper;
    } else if (macros.notation.fType == Notation::NTN_COMPACT) {
        // Compact notation uses minGrouping by default since ICU 59
        fMicros.grouping = Grouper::forStrategy(UNUM_GROUPING_MIN2);
    } else {
        fMicros.grouping = Grouper::forStrategy(UNUM_GROUPING_AUTO);
    }
    fMicros.grouping.setLocaleData(*fPatternInfo, macros.locale);

    // Padding strategy
    if (!macros.padder.isBogus()) {
        fMicros.padding = macros.padder;
    } else {
        fMicros.padding = Padder::none();
    }

    // Integer width
    if (!macros.integerWidth.isBogus()) {
        fMicros.integerWidth = macros.integerWidth;
    } else {
        fMicros.integerWidth = IntegerWidth::standard();
    }

    // Sign display
    if (macros.sign != UNUM_SIGN_COUNT) {
        fMicros.sign = macros.sign;
    } else {
        fMicros.sign = UNUM_SIGN_AUTO;
    }

    // Decimal mark display
    if (macros.decimal != UNUM_DECIMAL_SEPARATOR_COUNT) {
        fMicros.decimal = macros.decimal;
    } else {
        fMicros.decimal = UNUM_DECIMAL_SEPARATOR_AUTO;
    }

    // Use monetary separator symbols
    fMicros.useCurrency = isCurrency;

    // Inner modifier (scientific notation)
    if (macros.notation.fType == Notation::NTN_SCIENTIFIC) {
        fScientificHandler.adoptInstead(new ScientificHandler(&macros.notation, fMicros.symbols, chain));
        chain = fScientificHandler.getAlias();
    } else {
        // No inner modifier required
        fMicros.modInner = &fMicros.helpers.emptyStrongModifier;
    }

    // Middle modifier (patterns, positive/negative, currency symbols, percent)
    auto patternModifier = new MutablePatternModifier(false);
    fPatternModifier.adoptInstead(patternModifier);
    patternModifier->setPatternInfo(
            macros.affixProvider != nullptr ? macros.affixProvider
                                            : static_cast<const AffixPatternProvider*>(fPatternInfo.getAlias()));
    patternModifier->setPatternAttributes(fMicros.sign, isPermille);
    if (patternModifier->needsPlurals()) {
        patternModifier->setSymbols(
                fMicros.symbols,
                currencySymbols,
                unitWidth,
                resolvePluralRules(macros.rules, macros.locale, status));
    } else {
        patternModifier->setSymbols(fMicros.symbols, currencySymbols, unitWidth, nullptr);
    }
    if (safe) {
        fImmutablePatternModifier.adoptInstead(patternModifier->createImmutableAndChain(chain, status));
        chain = fImmutablePatternModifier.getAlias();
    } else {
        patternModifier->addToChain(chain);
        chain = patternModifier;
    }

    // Outer modifier (CLDR units and currency long names)
    if (isCldrUnit) {
        fLongNameHandler.adoptInstead(
                new LongNameHandler(
                        LongNameHandler::forMeasureUnit(
                                macros.locale,
                                macros.unit,
                                macros.perUnit,
                                unitWidth,
                                resolvePluralRules(macros.rules, macros.locale, status),
                                chain,
                                status)));
        chain = fLongNameHandler.getAlias();
    } else if (isCurrency && unitWidth == UNUM_UNIT_WIDTH_FULL_NAME) {
        fLongNameHandler.adoptInstead(
                new LongNameHandler(
                        LongNameHandler::forCurrencyLongNames(
                                macros.locale,
                                currency,
                                resolvePluralRules(macros.rules, macros.locale, status),
                                chain,
                                status)));
        chain = fLongNameHandler.getAlias();
    } else {
        // No outer modifier required
        fMicros.modOuter = &fMicros.helpers.emptyWeakModifier;
    }

    // Compact notation
    // NOTE: Compact notation can (but might not) override the middle modifier and rounding.
    // It therefore needs to go at the end of the chain.
    if (macros.notation.fType == Notation::NTN_COMPACT) {
        CompactType compactType = (isCurrency && unitWidth != UNUM_UNIT_WIDTH_FULL_NAME)
                                  ? CompactType::TYPE_CURRENCY : CompactType::TYPE_DECIMAL;
        fCompactHandler.adoptInstead(
                new CompactHandler(
                        macros.notation.fUnion.compactStyle,
                        macros.locale,
                        nsName,
                        compactType,
                        resolvePluralRules(macros.rules, macros.locale, status),
                        safe ? patternModifier : nullptr,
                        chain,
                        status));
        chain = fCompactHandler.getAlias();
    }

    return chain;
}

const PluralRules*
NumberFormatterImpl::resolvePluralRules(const PluralRules* rulesPtr, const Locale& locale,
                                        UErrorCode& status) {
    if (rulesPtr != nullptr) {
        return rulesPtr;
    }
    // Lazily create PluralRules
    if (fRules.isNull()) {
        fRules.adoptInstead(PluralRules::forLocale(locale, status));
    }
    return fRules.getAlias();
}

int32_t NumberFormatterImpl::microsToString(const MicroProps& micros, DecimalQuantity& quantity,
                                            NumberStringBuilder& string, UErrorCode& status) {
    micros.rounder.apply(quantity, status);
    micros.integerWidth.apply(quantity, status);
    int32_t length = writeNumber(micros, quantity, string, status);
    // NOTE: When range formatting is added, these modifiers can bubble up.
    // For now, apply them all here at once.
    // Always apply the inner modifier (which is "strong").
    length += micros.modInner->apply(string, 0, length, status);
    if (micros.padding.isValid()) {
        length += micros.padding
                .padAndApply(*micros.modMiddle, *micros.modOuter, string, 0, length, status);
    } else {
        length += micros.modMiddle->apply(string, 0, length, status);
        length += micros.modOuter->apply(string, 0, length, status);
    }
    return length;
}

int32_t NumberFormatterImpl::writeNumber(const MicroProps& micros, DecimalQuantity& quantity,
                                         NumberStringBuilder& string, UErrorCode& status) {
    int32_t length = 0;
    if (quantity.isInfinite()) {
        length += string.insert(
                length,
                micros.symbols->getSymbol(DecimalFormatSymbols::ENumberFormatSymbol::kInfinitySymbol),
                UNUM_INTEGER_FIELD,
                status);

    } else if (quantity.isNaN()) {
        length += string.insert(
                length,
                micros.symbols->getSymbol(DecimalFormatSymbols::ENumberFormatSymbol::kNaNSymbol),
                UNUM_INTEGER_FIELD,
                status);

    } else {
        // Add the integer digits
        length += writeIntegerDigits(micros, quantity, string, status);

        // Add the decimal point
        if (quantity.getLowerDisplayMagnitude() < 0 || micros.decimal == UNUM_DECIMAL_SEPARATOR_ALWAYS) {
            length += string.insert(
                    length,
                    micros.useCurrency ? micros.symbols->getSymbol(
                            DecimalFormatSymbols::ENumberFormatSymbol::kMonetarySeparatorSymbol) : micros
                            .symbols
                            ->getSymbol(
                                    DecimalFormatSymbols::ENumberFormatSymbol::kDecimalSeparatorSymbol),
                    UNUM_DECIMAL_SEPARATOR_FIELD,
                    status);
        }

        // Add the fraction digits
        length += writeFractionDigits(micros, quantity, string, status);
    }

    return length;
}

int32_t NumberFormatterImpl::writeIntegerDigits(const MicroProps& micros, DecimalQuantity& quantity,
                                                NumberStringBuilder& string, UErrorCode& status) {
    int length = 0;
    int integerCount = quantity.getUpperDisplayMagnitude() + 1;
    for (int i = 0; i < integerCount; i++) {
        // Add grouping separator
        if (micros.grouping.groupAtPosition(i, quantity)) {
            length += string.insert(
                    0,
                    micros.useCurrency ? micros.symbols->getSymbol(
                            DecimalFormatSymbols::ENumberFormatSymbol::kMonetaryGroupingSeparatorSymbol)
                                       : micros.symbols->getSymbol(
                            DecimalFormatSymbols::ENumberFormatSymbol::kGroupingSeparatorSymbol),
                    UNUM_GROUPING_SEPARATOR_FIELD,
                    status);
        }

        // Get and append the next digit value
        int8_t nextDigit = quantity.getDigit(i);
        length += utils::insertDigitFromSymbols(
                string, 0, nextDigit, *micros.symbols, UNUM_INTEGER_FIELD, status);
    }
    return length;
}

int32_t NumberFormatterImpl::writeFractionDigits(const MicroProps& micros, DecimalQuantity& quantity,
                                                 NumberStringBuilder& string, UErrorCode& status) {
    int length = 0;
    int fractionCount = -quantity.getLowerDisplayMagnitude();
    for (int i = 0; i < fractionCount; i++) {
        // Get and append the next digit value
        int8_t nextDigit = quantity.getDigit(-i - 1);
        length += utils::insertDigitFromSymbols(
                string, string.length(), nextDigit, *micros.symbols, UNUM_FRACTION_FIELD, status);
    }
    return length;
}

#endif /* #if !UCONFIG_NO_FORMATTING */
