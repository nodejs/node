// © 2017 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING && !UPRV_INCOMPLETE_CPP11_SUPPORT

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

using namespace icu;
using namespace icu::number;
using namespace icu::number::impl;

namespace {

// NOTE: In Java, the method to get a pattern from the resource bundle exists in NumberFormat.
// In C++, we have to implement that logic here.
// TODO: Make Java and C++ consistent?

enum CldrPatternStyle {
    CLDR_PATTERN_STYLE_DECIMAL,
    CLDR_PATTERN_STYLE_CURRENCY,
    CLDR_PATTERN_STYLE_ACCOUNTING,
    CLDR_PATTERN_STYLE_PERCENT
    // TODO: Consider scientific format.
};

const char16_t *
doGetPattern(UResourceBundle *res, const char *nsName, const char *patternKey, UErrorCode &publicStatus,
             UErrorCode &localStatus) {
    // Construct the path into the resource bundle
    CharString key;
    key.append("NumberElements/", publicStatus);
    key.append(nsName, publicStatus);
    key.append("/patterns/", publicStatus);
    key.append(patternKey, publicStatus);
    if (U_FAILURE(publicStatus)) {
        return u"";
    }
    return ures_getStringByKeyWithFallback(res, key.data(), nullptr, &localStatus);
}

const char16_t *getPatternForStyle(const Locale &locale, const char *nsName, CldrPatternStyle style,
                                   UErrorCode &status) {
    const char *patternKey;
    switch (style) {
        case CLDR_PATTERN_STYLE_DECIMAL:
            patternKey = "decimalFormat";
            break;
        case CLDR_PATTERN_STYLE_CURRENCY:
            patternKey = "currencyFormat";
            break;
        case CLDR_PATTERN_STYLE_ACCOUNTING:
            patternKey = "accountingFormat";
            break;
        case CLDR_PATTERN_STYLE_PERCENT:
        default:
            patternKey = "percentFormat";
            break;
    }
    LocalUResourceBundlePointer res(ures_open(nullptr, locale.getName(), &status));
    if (U_FAILURE(status)) { return u""; }

    // Attempt to get the pattern with the native numbering system.
    UErrorCode localStatus = U_ZERO_ERROR;
    const char16_t *pattern;
    pattern = doGetPattern(res.getAlias(), nsName, patternKey, status, localStatus);
    if (U_FAILURE(status)) { return u""; }

    // Fall back to latn if native numbering system does not have the right pattern
    if (U_FAILURE(localStatus) && uprv_strcmp("latn", nsName) != 0) {
        localStatus = U_ZERO_ERROR;
        pattern = doGetPattern(res.getAlias(), "latn", patternKey, status, localStatus);
        if (U_FAILURE(status)) { return u""; }
    }

    return pattern;
}

inline bool unitIsCurrency(const MeasureUnit &unit) {
    return uprv_strcmp("currency", unit.getType()) == 0;
}

inline bool unitIsNoUnit(const MeasureUnit &unit) {
    return uprv_strcmp("none", unit.getType()) == 0;
}

inline bool unitIsPercent(const MeasureUnit &unit) {
    return uprv_strcmp("percent", unit.getSubtype()) == 0;
}

inline bool unitIsPermille(const MeasureUnit &unit) {
    return uprv_strcmp("permille", unit.getSubtype()) == 0;
}

}  // namespace

NumberFormatterImpl *NumberFormatterImpl::fromMacros(const MacroProps &macros, UErrorCode &status) {
    return new NumberFormatterImpl(macros, true, status);
}

void NumberFormatterImpl::applyStatic(const MacroProps &macros, DecimalQuantity &inValue,
                                      NumberStringBuilder &outString, UErrorCode &status) {
    NumberFormatterImpl impl(macros, false, status);
    impl.applyUnsafe(inValue, outString, status);
}

// NOTE: C++ SPECIFIC DIFFERENCE FROM JAVA:
// The "safe" apply method uses a new MicroProps. In the MicroPropsGenerator, fMicros is copied into the new instance.
// The "unsafe" method simply re-uses fMicros, eliminating the extra copy operation.
// See MicroProps::processQuantity() for details.

void NumberFormatterImpl::apply(DecimalQuantity &inValue, NumberStringBuilder &outString,
                                UErrorCode &status) const {
    if (U_FAILURE(status)) { return; }
    MicroProps micros;
    fMicroPropsGenerator->processQuantity(inValue, micros, status);
    if (U_FAILURE(status)) { return; }
    microsToString(micros, inValue, outString, status);
}

void NumberFormatterImpl::applyUnsafe(DecimalQuantity &inValue, NumberStringBuilder &outString,
                                      UErrorCode &status) {
    if (U_FAILURE(status)) { return; }
    fMicroPropsGenerator->processQuantity(inValue, fMicros, status);
    if (U_FAILURE(status)) { return; }
    microsToString(fMicros, inValue, outString, status);
}

NumberFormatterImpl::NumberFormatterImpl(const MacroProps &macros, bool safe, UErrorCode &status) {
    fMicroPropsGenerator = macrosToMicroGenerator(macros, safe, status);
}

//////////

const MicroPropsGenerator *
NumberFormatterImpl::macrosToMicroGenerator(const MacroProps &macros, bool safe, UErrorCode &status) {
    const MicroPropsGenerator *chain = &fMicros;

    // Check that macros is error-free before continuing.
    if (macros.copyErrorTo(status)) {
        return nullptr;
    }

    // TODO: Accept currency symbols from DecimalFormatSymbols?

    // Pre-compute a few values for efficiency.
    bool isCurrency = unitIsCurrency(macros.unit);
    bool isNoUnit = unitIsNoUnit(macros.unit);
    bool isPercent = isNoUnit && unitIsPercent(macros.unit);
    bool isPermille = isNoUnit && unitIsPermille(macros.unit);
    bool isCldrUnit = !isCurrency && !isNoUnit;
    bool isAccounting =
            macros.sign == UNUM_SIGN_ACCOUNTING || macros.sign == UNUM_SIGN_ACCOUNTING_ALWAYS;
    CurrencyUnit currency(kDefaultCurrency, status);
    if (isCurrency) {
        currency = CurrencyUnit(macros.unit, status); // Restore CurrencyUnit from MeasureUnit
    }
    UNumberUnitWidth unitWidth = UNUM_UNIT_WIDTH_SHORT;
    if (macros.unitWidth != UNUM_UNIT_WIDTH_COUNT) {
        unitWidth = macros.unitWidth;
    }

    // Select the numbering system.
    LocalPointer<const NumberingSystem> nsLocal;
    const NumberingSystem *ns;
    if (macros.symbols.isNumberingSystem()) {
        ns = macros.symbols.getNumberingSystem();
    } else {
        // TODO: Is there a way to avoid creating the NumberingSystem object?
        ns = NumberingSystem::createInstance(macros.locale, status);
        // Give ownership to the function scope.
        nsLocal.adoptInstead(ns);
    }
    const char *nsName = U_SUCCESS(status) ? ns->getName() : "latn";

    // Load and parse the pattern string.  It is used for grouping sizes and affixes only.
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
    const char16_t *pattern = getPatternForStyle(macros.locale, nsName, patternStyle, status);
    auto patternInfo = new ParsedPatternInfo();
    fPatternInfo.adoptInstead(patternInfo);
    PatternParser::parseToPatternInfo(UnicodeString(pattern), *patternInfo, status);

    /////////////////////////////////////////////////////////////////////////////////////
    /// START POPULATING THE DEFAULT MICROPROPS AND BUILDING THE MICROPROPS GENERATOR ///
    /////////////////////////////////////////////////////////////////////////////////////

    // Symbols
    if (macros.symbols.isDecimalFormatSymbols()) {
        fMicros.symbols = macros.symbols.getDecimalFormatSymbols();
    } else {
        fMicros.symbols = new DecimalFormatSymbols(macros.locale, *ns, status);
        // Give ownership to the NumberFormatterImpl.
        fSymbols.adoptInstead(fMicros.symbols);
    }

    // Rounding strategy
    if (!macros.rounder.isBogus()) {
        fMicros.rounding = macros.rounder;
    } else if (macros.notation.fType == Notation::NTN_COMPACT) {
        fMicros.rounding = Rounder::integer().withMinDigits(2);
    } else if (isCurrency) {
        fMicros.rounding = Rounder::currency(UCURR_USAGE_STANDARD);
    } else {
        fMicros.rounding = Rounder::maxFraction(6);
    }
    fMicros.rounding.setLocaleData(currency, status);

    // Grouping strategy
    if (!macros.grouper.isBogus()) {
        fMicros.grouping = macros.grouper;
    } else if (macros.notation.fType == Notation::NTN_COMPACT) {
        // Compact notation uses minGrouping by default since ICU 59
        fMicros.grouping = Grouper::minTwoDigits();
    } else {
        fMicros.grouping = Grouper::defaults();
    }
    fMicros.grouping.setLocaleData(*fPatternInfo);

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
        fMicros.integerWidth = IntegerWidth::zeroFillTo(1);
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
    patternModifier->setPatternInfo(fPatternInfo.getAlias());
    patternModifier->setPatternAttributes(fMicros.sign, isPermille);
    if (patternModifier->needsPlurals()) {
        patternModifier->setSymbols(
                fMicros.symbols,
                currency,
                unitWidth,
                resolvePluralRules(macros.rules, macros.locale, status));
    } else {
        patternModifier->setSymbols(fMicros.symbols, currency, unitWidth, nullptr);
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

const PluralRules *
NumberFormatterImpl::resolvePluralRules(const PluralRules *rulesPtr, const Locale &locale,
                                        UErrorCode &status) {
    if (rulesPtr != nullptr) {
        return rulesPtr;
    }
    // Lazily create PluralRules
    if (fRules.isNull()) {
        fRules.adoptInstead(PluralRules::forLocale(locale, status));
    }
    return fRules.getAlias();
}

int32_t NumberFormatterImpl::microsToString(const MicroProps &micros, DecimalQuantity &quantity,
                                            NumberStringBuilder &string, UErrorCode &status) {
    micros.rounding.apply(quantity, status);
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

int32_t NumberFormatterImpl::writeNumber(const MicroProps &micros, DecimalQuantity &quantity,
                                         NumberStringBuilder &string, UErrorCode &status) {
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

int32_t NumberFormatterImpl::writeIntegerDigits(const MicroProps &micros, DecimalQuantity &quantity,
                                                NumberStringBuilder &string, UErrorCode &status) {
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
        length += string.insert(
                0, getDigitFromSymbols(nextDigit, *micros.symbols), UNUM_INTEGER_FIELD, status);
    }
    return length;
}

int32_t NumberFormatterImpl::writeFractionDigits(const MicroProps &micros, DecimalQuantity &quantity,
                                                 NumberStringBuilder &string, UErrorCode &status) {
    int length = 0;
    int fractionCount = -quantity.getLowerDisplayMagnitude();
    for (int i = 0; i < fractionCount; i++) {
        // Get and append the next digit value
        int8_t nextDigit = quantity.getDigit(-i - 1);
        length += string.append(
                getDigitFromSymbols(nextDigit, *micros.symbols), UNUM_FRACTION_FIELD, status);
    }
    return length;
}

#endif /* #if !UCONFIG_NO_FORMATTING */
