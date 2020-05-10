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


MicroPropsGenerator::~MicroPropsGenerator() = default;


NumberFormatterImpl::NumberFormatterImpl(const MacroProps& macros, UErrorCode& status)
    : NumberFormatterImpl(macros, true, status) {
}

int32_t NumberFormatterImpl::formatStatic(const MacroProps& macros, DecimalQuantity& inValue,
                                       FormattedStringBuilder& outString, UErrorCode& status) {
    NumberFormatterImpl impl(macros, false, status);
    MicroProps& micros = impl.preProcessUnsafe(inValue, status);
    if (U_FAILURE(status)) { return 0; }
    int32_t length = writeNumber(micros, inValue, outString, 0, status);
    length += writeAffixes(micros, outString, 0, length, status);
    return length;
}

int32_t NumberFormatterImpl::getPrefixSuffixStatic(const MacroProps& macros, Signum signum,
                                                   StandardPlural::Form plural,
                                                   FormattedStringBuilder& outString, UErrorCode& status) {
    NumberFormatterImpl impl(macros, false, status);
    return impl.getPrefixSuffixUnsafe(signum, plural, outString, status);
}

// NOTE: C++ SPECIFIC DIFFERENCE FROM JAVA:
// The "safe" apply method uses a new MicroProps. In the MicroPropsGenerator, fMicros is copied into the new instance.
// The "unsafe" method simply re-uses fMicros, eliminating the extra copy operation.
// See MicroProps::processQuantity() for details.

int32_t NumberFormatterImpl::format(DecimalQuantity& inValue, FormattedStringBuilder& outString,
                                UErrorCode& status) const {
    MicroProps micros;
    preProcess(inValue, micros, status);
    if (U_FAILURE(status)) { return 0; }
    int32_t length = writeNumber(micros, inValue, outString, 0, status);
    length += writeAffixes(micros, outString, 0, length, status);
    return length;
}

void NumberFormatterImpl::preProcess(DecimalQuantity& inValue, MicroProps& microsOut,
                                     UErrorCode& status) const {
    if (U_FAILURE(status)) { return; }
    if (fMicroPropsGenerator == nullptr) {
        status = U_INTERNAL_PROGRAM_ERROR;
        return;
    }
    fMicroPropsGenerator->processQuantity(inValue, microsOut, status);
    microsOut.integerWidth.apply(inValue, status);
}

MicroProps& NumberFormatterImpl::preProcessUnsafe(DecimalQuantity& inValue, UErrorCode& status) {
    if (U_FAILURE(status)) {
        return fMicros; // must always return a value
    }
    if (fMicroPropsGenerator == nullptr) {
        status = U_INTERNAL_PROGRAM_ERROR;
        return fMicros; // must always return a value
    }
    fMicroPropsGenerator->processQuantity(inValue, fMicros, status);
    fMicros.integerWidth.apply(inValue, status);
    return fMicros;
}

int32_t NumberFormatterImpl::getPrefixSuffix(Signum signum, StandardPlural::Form plural,
                                             FormattedStringBuilder& outString, UErrorCode& status) const {
    if (U_FAILURE(status)) { return 0; }
    // #13453: DecimalFormat wants the affixes from the pattern only (modMiddle, aka pattern modifier).
    // Safe path: use fImmutablePatternModifier.
    const Modifier* modifier = fImmutablePatternModifier->getModifier(signum, plural);
    modifier->apply(outString, 0, 0, status);
    if (U_FAILURE(status)) { return 0; }
    return modifier->getPrefixLength();
}

int32_t NumberFormatterImpl::getPrefixSuffixUnsafe(Signum signum, StandardPlural::Form plural,
                                                   FormattedStringBuilder& outString, UErrorCode& status) {
    if (U_FAILURE(status)) { return 0; }
    // #13453: DecimalFormat wants the affixes from the pattern only (modMiddle, aka pattern modifier).
    // Unsafe path: use fPatternModifier.
    fPatternModifier->setNumberProperties(signum, plural);
    fPatternModifier->apply(outString, 0, 0, status);
    if (U_FAILURE(status)) { return 0; }
    return fPatternModifier->getPrefixLength();
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
    bool isPercent = utils::unitIsPercent(macros.unit);
    bool isPermille = utils::unitIsPermille(macros.unit);
    bool isAccounting =
            macros.sign == UNUM_SIGN_ACCOUNTING || macros.sign == UNUM_SIGN_ACCOUNTING_ALWAYS ||
            macros.sign == UNUM_SIGN_ACCOUNTING_EXCEPT_ZERO;
    CurrencyUnit currency(u"", status);
    if (isCurrency) {
        currency = CurrencyUnit(macros.unit, status); // Restore CurrencyUnit from MeasureUnit
    }
    UNumberUnitWidth unitWidth = UNUM_UNIT_WIDTH_SHORT;
    if (macros.unitWidth != UNUM_UNIT_WIDTH_COUNT) {
        unitWidth = macros.unitWidth;
    }
    bool isCldrUnit = !isCurrency && !isNoUnit &&
        (unitWidth == UNUM_UNIT_WIDTH_FULL_NAME || !(isPercent || isPermille));

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
    uprv_strncpy(fMicros.nsName, nsName, 8);
    fMicros.nsName[8] = 0; // guarantee NUL-terminated

    // Resolve the symbols. Do this here because currency may need to customize them.
    if (macros.symbols.isDecimalFormatSymbols()) {
        fMicros.symbols = macros.symbols.getDecimalFormatSymbols();
    } else {
        LocalPointer<DecimalFormatSymbols> newSymbols(
            new DecimalFormatSymbols(macros.locale, *ns, status), status);
        if (U_FAILURE(status)) {
            return nullptr;
        }
        if (isCurrency) {
            newSymbols->setCurrency(currency.getISOCurrency(), status);
            if (U_FAILURE(status)) {
                return nullptr;
            }
        }
        fMicros.symbols = newSymbols.getAlias();
        fSymbols.adoptInstead(newSymbols.orphan());
    }

    // Load and parse the pattern string. It is used for grouping sizes and affixes only.
    // If we are formatting currency, check for a currency-specific pattern.
    const char16_t* pattern = nullptr;
    if (isCurrency && fMicros.symbols->getCurrencyPattern() != nullptr) {
        pattern = fMicros.symbols->getCurrencyPattern();
    }
    if (pattern == nullptr) {
        CldrPatternStyle patternStyle;
        if (isCldrUnit) {
            patternStyle = CLDR_PATTERN_STYLE_DECIMAL;
        } else if (isPercent || isPermille) {
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
        if (U_FAILURE(status)) {
            return nullptr;
        }
    }
    auto patternInfo = new ParsedPatternInfo();
    if (patternInfo == nullptr) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return nullptr;
    }
    fPatternInfo.adoptInstead(patternInfo);
    PatternParser::parseToPatternInfo(UnicodeString(pattern), *patternInfo, status);
    if (U_FAILURE(status)) {
        return nullptr;
    }

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
    if (U_FAILURE(status)) {
        return nullptr;
    }

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
        auto newScientificHandler = new ScientificHandler(&macros.notation, fMicros.symbols, chain);
        if (newScientificHandler == nullptr) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return nullptr;
        }
        fScientificHandler.adoptInstead(newScientificHandler);
        chain = fScientificHandler.getAlias();
    } else {
        // No inner modifier required
        fMicros.modInner = &fMicros.helpers.emptyStrongModifier;
    }

    // Middle modifier (patterns, positive/negative, currency symbols, percent)
    auto patternModifier = new MutablePatternModifier(false);
    if (patternModifier == nullptr) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return nullptr;
    }
    fPatternModifier.adoptInstead(patternModifier);
    patternModifier->setPatternInfo(
            macros.affixProvider != nullptr ? macros.affixProvider
                                            : static_cast<const AffixPatternProvider*>(fPatternInfo.getAlias()),
            kUndefinedField);
    patternModifier->setPatternAttributes(fMicros.sign, isPermille);
    if (patternModifier->needsPlurals()) {
        patternModifier->setSymbols(
                fMicros.symbols,
                currency,
                unitWidth,
                resolvePluralRules(macros.rules, macros.locale, status),
                status);
    } else {
        patternModifier->setSymbols(fMicros.symbols, currency, unitWidth, nullptr, status);
    }
    if (safe) {
        fImmutablePatternModifier.adoptInstead(patternModifier->createImmutable(status));
    }
    if (U_FAILURE(status)) {
        return nullptr;
    }

    // Outer modifier (CLDR units and currency long names)
    if (isCldrUnit) {
        fLongNameHandler.adoptInstead(
                LongNameHandler::forMeasureUnit(
                        macros.locale,
                        macros.unit,
                        macros.perUnit,
                        unitWidth,
                        resolvePluralRules(macros.rules, macros.locale, status),
                        chain,
                        status));
        chain = fLongNameHandler.getAlias();
    } else if (isCurrency && unitWidth == UNUM_UNIT_WIDTH_FULL_NAME) {
        fLongNameHandler.adoptInstead(
                LongNameHandler::forCurrencyLongNames(
                        macros.locale,
                        currency,
                        resolvePluralRules(macros.rules, macros.locale, status),
                        chain,
                        status));
        chain = fLongNameHandler.getAlias();
    } else {
        // No outer modifier required
        fMicros.modOuter = &fMicros.helpers.emptyWeakModifier;
    }
    if (U_FAILURE(status)) {
        return nullptr;
    }

    // Compact notation
    if (macros.notation.fType == Notation::NTN_COMPACT) {
        CompactType compactType = (isCurrency && unitWidth != UNUM_UNIT_WIDTH_FULL_NAME)
                                  ? CompactType::TYPE_CURRENCY : CompactType::TYPE_DECIMAL;
        auto newCompactHandler = new CompactHandler(
            macros.notation.fUnion.compactStyle,
            macros.locale,
            nsName,
            compactType,
            resolvePluralRules(macros.rules, macros.locale, status),
            patternModifier,
            safe,
            chain,
            status);
        if (newCompactHandler == nullptr) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return nullptr;
        }
        fCompactHandler.adoptInstead(newCompactHandler);
        chain = fCompactHandler.getAlias();
    }
    if (U_FAILURE(status)) {
        return nullptr;
    }

    // Always add the pattern modifier as the last element of the chain.
    if (safe) {
        fImmutablePatternModifier->addToChain(chain);
        chain = fImmutablePatternModifier.getAlias();
    } else {
        patternModifier->addToChain(chain);
        chain = patternModifier;
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

int32_t NumberFormatterImpl::writeAffixes(const MicroProps& micros, FormattedStringBuilder& string,
                                          int32_t start, int32_t end, UErrorCode& status) {
    // Always apply the inner modifier (which is "strong").
    int32_t length = micros.modInner->apply(string, start, end, status);
    if (micros.padding.isValid()) {
        length += micros.padding
                .padAndApply(*micros.modMiddle, *micros.modOuter, string, start, length + end, status);
    } else {
        length += micros.modMiddle->apply(string, start, length + end, status);
        length += micros.modOuter->apply(string, start, length + end, status);
    }
    return length;
}

int32_t NumberFormatterImpl::writeNumber(const MicroProps& micros, DecimalQuantity& quantity,
                                         FormattedStringBuilder& string, int32_t index,
                                         UErrorCode& status) {
    int32_t length = 0;
    if (quantity.isInfinite()) {
        length += string.insert(
                length + index,
                micros.symbols->getSymbol(DecimalFormatSymbols::ENumberFormatSymbol::kInfinitySymbol),
                {UFIELD_CATEGORY_NUMBER, UNUM_INTEGER_FIELD},
                status);

    } else if (quantity.isNaN()) {
        length += string.insert(
                length + index,
                micros.symbols->getSymbol(DecimalFormatSymbols::ENumberFormatSymbol::kNaNSymbol),
                {UFIELD_CATEGORY_NUMBER, UNUM_INTEGER_FIELD},
                status);

    } else {
        // Add the integer digits
        length += writeIntegerDigits(micros, quantity, string, length + index, status);

        // Add the decimal point
        if (quantity.getLowerDisplayMagnitude() < 0 || micros.decimal == UNUM_DECIMAL_SEPARATOR_ALWAYS) {
            length += string.insert(
                    length + index,
                    micros.useCurrency ? micros.symbols->getSymbol(
                            DecimalFormatSymbols::ENumberFormatSymbol::kMonetarySeparatorSymbol) : micros
                            .symbols
                            ->getSymbol(
                                    DecimalFormatSymbols::ENumberFormatSymbol::kDecimalSeparatorSymbol),
                    {UFIELD_CATEGORY_NUMBER, UNUM_DECIMAL_SEPARATOR_FIELD},
                    status);
        }

        // Add the fraction digits
        length += writeFractionDigits(micros, quantity, string, length + index, status);

        if (length == 0) {
            // Force output of the digit for value 0
            length += utils::insertDigitFromSymbols(
                    string,
                    index,
                    0,
                    *micros.symbols,
                    {UFIELD_CATEGORY_NUMBER, UNUM_INTEGER_FIELD},
                    status);
        }
    }

    return length;
}

int32_t NumberFormatterImpl::writeIntegerDigits(const MicroProps& micros, DecimalQuantity& quantity,
                                                FormattedStringBuilder& string, int32_t index,
                                                UErrorCode& status) {
    int length = 0;
    int integerCount = quantity.getUpperDisplayMagnitude() + 1;
    for (int i = 0; i < integerCount; i++) {
        // Add grouping separator
        if (micros.grouping.groupAtPosition(i, quantity)) {
            length += string.insert(
                    index,
                    micros.useCurrency ? micros.symbols->getSymbol(
                            DecimalFormatSymbols::ENumberFormatSymbol::kMonetaryGroupingSeparatorSymbol)
                                       : micros.symbols->getSymbol(
                            DecimalFormatSymbols::ENumberFormatSymbol::kGroupingSeparatorSymbol),
                    {UFIELD_CATEGORY_NUMBER, UNUM_GROUPING_SEPARATOR_FIELD},
                    status);
        }

        // Get and append the next digit value
        int8_t nextDigit = quantity.getDigit(i);
        length += utils::insertDigitFromSymbols(
                string,
                index,
                nextDigit,
                *micros.symbols,
                {UFIELD_CATEGORY_NUMBER,
                UNUM_INTEGER_FIELD},
                status);
    }
    return length;
}

int32_t NumberFormatterImpl::writeFractionDigits(const MicroProps& micros, DecimalQuantity& quantity,
                                                 FormattedStringBuilder& string, int32_t index,
                                                 UErrorCode& status) {
    int length = 0;
    int fractionCount = -quantity.getLowerDisplayMagnitude();
    for (int i = 0; i < fractionCount; i++) {
        // Get and append the next digit value
        int8_t nextDigit = quantity.getDigit(-i - 1);
        length += utils::insertDigitFromSymbols(
                string,
                length + index,
                nextDigit,
                *micros.symbols,
                {UFIELD_CATEGORY_NUMBER, UNUM_FRACTION_FIELD},
                status);
    }
    return length;
}

#endif /* #if !UCONFIG_NO_FORMATTING */
