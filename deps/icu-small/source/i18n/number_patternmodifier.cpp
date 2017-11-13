// © 2017 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING && !UPRV_INCOMPLETE_CPP11_SUPPORT

#include "cstring.h"
#include "number_patternmodifier.h"
#include "unicode/dcfmtsym.h"
#include "unicode/ucurr.h"
#include "unicode/unistr.h"

using namespace icu;
using namespace icu::number;
using namespace icu::number::impl;

MutablePatternModifier::MutablePatternModifier(bool isStrong) : fStrong(isStrong) {}

void MutablePatternModifier::setPatternInfo(const AffixPatternProvider *patternInfo) {
    this->patternInfo = patternInfo;
}

void MutablePatternModifier::setPatternAttributes(UNumberSignDisplay signDisplay, bool perMille) {
    this->signDisplay = signDisplay;
    this->perMilleReplacesPercent = perMille;
}

void
MutablePatternModifier::setSymbols(const DecimalFormatSymbols *symbols, const CurrencyUnit &currency,
                                   const UNumberUnitWidth unitWidth, const PluralRules *rules) {
    U_ASSERT((rules != nullptr) == needsPlurals());
    this->symbols = symbols;
    uprv_memcpy(static_cast<char16_t *>(this->currencyCode),
            currency.getISOCurrency(),
            sizeof(char16_t) * 4);
    this->unitWidth = unitWidth;
    this->rules = rules;
}

void MutablePatternModifier::setNumberProperties(bool isNegative, StandardPlural::Form plural) {
    this->isNegative = isNegative;
    this->plural = plural;
}

bool MutablePatternModifier::needsPlurals() const {
    UErrorCode statusLocal = U_ZERO_ERROR;
    return patternInfo->containsSymbolType(AffixPatternType::TYPE_CURRENCY_TRIPLE, statusLocal);
    // Silently ignore any error codes.
}

ImmutablePatternModifier *MutablePatternModifier::createImmutable(UErrorCode &status) {
    return createImmutableAndChain(nullptr, status);
}

ImmutablePatternModifier *
MutablePatternModifier::createImmutableAndChain(const MicroPropsGenerator *parent, UErrorCode &status) {

    // TODO: Move StandardPlural VALUES to standardplural.h
    static const StandardPlural::Form STANDARD_PLURAL_VALUES[] = {
            StandardPlural::Form::ZERO,
            StandardPlural::Form::ONE,
            StandardPlural::Form::TWO,
            StandardPlural::Form::FEW,
            StandardPlural::Form::MANY,
            StandardPlural::Form::OTHER};

    auto pm = new ParameterizedModifier();
    if (pm == nullptr) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return nullptr;
    }

    if (needsPlurals()) {
        // Slower path when we require the plural keyword.
        for (StandardPlural::Form plural : STANDARD_PLURAL_VALUES) {
            setNumberProperties(false, plural);
            pm->adoptSignPluralModifier(false, plural, createConstantModifier(status));
            setNumberProperties(true, plural);
            pm->adoptSignPluralModifier(true, plural, createConstantModifier(status));
        }
        if (U_FAILURE(status)) {
            delete pm;
            return nullptr;
        }
        return new ImmutablePatternModifier(pm, rules, parent);  // adopts pm
    } else {
        // Faster path when plural keyword is not needed.
        setNumberProperties(false, StandardPlural::Form::COUNT);
        Modifier *positive = createConstantModifier(status);
        setNumberProperties(true, StandardPlural::Form::COUNT);
        Modifier *negative = createConstantModifier(status);
        pm->adoptPositiveNegativeModifiers(positive, negative);
        if (U_FAILURE(status)) {
            delete pm;
            return nullptr;
        }
        return new ImmutablePatternModifier(pm, nullptr, parent);  // adopts pm
    }
}

ConstantMultiFieldModifier *MutablePatternModifier::createConstantModifier(UErrorCode &status) {
    NumberStringBuilder a;
    NumberStringBuilder b;
    insertPrefix(a, 0, status);
    insertSuffix(b, 0, status);
    if (patternInfo->hasCurrencySign()) {
        return new CurrencySpacingEnabledModifier(a, b, fStrong, *symbols, status);
    } else {
        return new ConstantMultiFieldModifier(a, b, fStrong);
    }
}

ImmutablePatternModifier::ImmutablePatternModifier(ParameterizedModifier *pm, const PluralRules *rules,
                                                   const MicroPropsGenerator *parent)
        : pm(pm), rules(rules), parent(parent) {}

void ImmutablePatternModifier::processQuantity(DecimalQuantity &quantity, MicroProps &micros,
                                               UErrorCode &status) const {
    parent->processQuantity(quantity, micros, status);
    applyToMicros(micros, quantity);
}

void ImmutablePatternModifier::applyToMicros(MicroProps &micros, DecimalQuantity &quantity) const {
    if (rules == nullptr) {
        micros.modMiddle = pm->getModifier(quantity.isNegative());
    } else {
        // TODO: Fix this. Avoid the copy.
        DecimalQuantity copy(quantity);
        copy.roundToInfinity();
        StandardPlural::Form plural = copy.getStandardPlural(rules);
        micros.modMiddle = pm->getModifier(quantity.isNegative(), plural);
    }
}

/** Used by the unsafe code path. */
MicroPropsGenerator &MutablePatternModifier::addToChain(const MicroPropsGenerator *parent) {
    this->parent = parent;
    return *this;
}

void MutablePatternModifier::processQuantity(DecimalQuantity &fq, MicroProps &micros,
                                             UErrorCode &status) const {
    parent->processQuantity(fq, micros, status);
    // The unsafe code path performs self-mutation, so we need a const_cast.
    // This method needs to be const because it overrides a const method in the parent class.
    auto nonConstThis = const_cast<MutablePatternModifier *>(this);
    if (needsPlurals()) {
        // TODO: Fix this. Avoid the copy.
        DecimalQuantity copy(fq);
        micros.rounding.apply(copy, status);
        nonConstThis->setNumberProperties(fq.isNegative(), copy.getStandardPlural(rules));
    } else {
        nonConstThis->setNumberProperties(fq.isNegative(), StandardPlural::Form::COUNT);
    }
    micros.modMiddle = this;
}

int32_t MutablePatternModifier::apply(NumberStringBuilder &output, int32_t leftIndex, int32_t rightIndex,
                                      UErrorCode &status) const {
    // The unsafe code path performs self-mutation, so we need a const_cast.
    // This method needs to be const because it overrides a const method in the parent class.
    auto nonConstThis = const_cast<MutablePatternModifier *>(this);
    int32_t prefixLen = nonConstThis->insertPrefix(output, leftIndex, status);
    int32_t suffixLen = nonConstThis->insertSuffix(output, rightIndex + prefixLen, status);
    CurrencySpacingEnabledModifier::applyCurrencySpacing(
            output, leftIndex, prefixLen, rightIndex + prefixLen, suffixLen, *symbols, status);
    return prefixLen + suffixLen;
}

int32_t MutablePatternModifier::getPrefixLength(UErrorCode &status) const {
    // The unsafe code path performs self-mutation, so we need a const_cast.
    // This method needs to be const because it overrides a const method in the parent class.
    auto nonConstThis = const_cast<MutablePatternModifier *>(this);

    // Enter and exit CharSequence Mode to get the length.
    nonConstThis->enterCharSequenceMode(true);
    int result = AffixUtils::unescapedCodePointCount(*this, *this, status);  // prefix length
    nonConstThis->exitCharSequenceMode();
    return result;
}

int32_t MutablePatternModifier::getCodePointCount(UErrorCode &status) const {
    // The unsafe code path performs self-mutation, so we need a const_cast.
    // This method needs to be const because it overrides a const method in the parent class.
    auto nonConstThis = const_cast<MutablePatternModifier *>(this);

    // Enter and exit CharSequence Mode to get the length.
    nonConstThis->enterCharSequenceMode(true);
    int result = AffixUtils::unescapedCodePointCount(*this, *this, status);  // prefix length
    nonConstThis->exitCharSequenceMode();
    nonConstThis->enterCharSequenceMode(false);
    result += AffixUtils::unescapedCodePointCount(*this, *this, status);  // suffix length
    nonConstThis->exitCharSequenceMode();
    return result;
}

bool MutablePatternModifier::isStrong() const {
    return fStrong;
}

int32_t MutablePatternModifier::insertPrefix(NumberStringBuilder &sb, int position, UErrorCode &status) {
    enterCharSequenceMode(true);
    int length = AffixUtils::unescape(*this, sb, position, *this, status);
    exitCharSequenceMode();
    return length;
}

int32_t MutablePatternModifier::insertSuffix(NumberStringBuilder &sb, int position, UErrorCode &status) {
    enterCharSequenceMode(false);
    int length = AffixUtils::unescape(*this, sb, position, *this, status);
    exitCharSequenceMode();
    return length;
}

UnicodeString MutablePatternModifier::getSymbol(AffixPatternType type) const {
    switch (type) {
        case AffixPatternType::TYPE_MINUS_SIGN:
            return symbols->getSymbol(DecimalFormatSymbols::ENumberFormatSymbol::kMinusSignSymbol);
        case AffixPatternType::TYPE_PLUS_SIGN:
            return symbols->getSymbol(DecimalFormatSymbols::ENumberFormatSymbol::kPlusSignSymbol);
        case AffixPatternType::TYPE_PERCENT:
            return symbols->getSymbol(DecimalFormatSymbols::ENumberFormatSymbol::kPercentSymbol);
        case AffixPatternType::TYPE_PERMILLE:
            return symbols->getSymbol(DecimalFormatSymbols::ENumberFormatSymbol::kPerMillSymbol);
        case AffixPatternType::TYPE_CURRENCY_SINGLE: {
            // UnitWidth ISO and HIDDEN overrides the singular currency symbol.
            if (unitWidth == UNumberUnitWidth::UNUM_UNIT_WIDTH_ISO_CODE) {
                return UnicodeString(currencyCode, 3);
            } else if (unitWidth == UNumberUnitWidth::UNUM_UNIT_WIDTH_HIDDEN) {
                return UnicodeString();
            } else {
                UErrorCode status = U_ZERO_ERROR;
                UBool isChoiceFormat = FALSE;
                int32_t symbolLen = 0;
                const char16_t *symbol = ucurr_getName(
                        currencyCode,
                        symbols->getLocale().getName(),
                        UCurrNameStyle::UCURR_SYMBOL_NAME,
                        &isChoiceFormat,
                        &symbolLen,
                        &status);
                return UnicodeString(symbol, symbolLen);
            }
        }
        case AffixPatternType::TYPE_CURRENCY_DOUBLE:
            return UnicodeString(currencyCode, 3);
        case AffixPatternType::TYPE_CURRENCY_TRIPLE: {
            // NOTE: This is the code path only for patterns containing "¤¤¤".
            // Plural currencies set via the API are formatted in LongNameHandler.
            // This code path is used by DecimalFormat via CurrencyPluralInfo.
            U_ASSERT(plural != StandardPlural::Form::COUNT);
            UErrorCode status = U_ZERO_ERROR;
            UBool isChoiceFormat = FALSE;
            int32_t symbolLen = 0;
            const char16_t *symbol = ucurr_getPluralName(
                    currencyCode,
                    symbols->getLocale().getName(),
                    &isChoiceFormat,
                    StandardPlural::getKeyword(plural),
                    &symbolLen,
                    &status);
            return UnicodeString(symbol, symbolLen);
        }
        case AffixPatternType::TYPE_CURRENCY_QUAD:
            return UnicodeString(u"\uFFFD");
        case AffixPatternType::TYPE_CURRENCY_QUINT:
            return UnicodeString(u"\uFFFD");
        default:
            U_ASSERT(false);
            return UnicodeString();
    }
}

/** This method contains the heart of the logic for rendering LDML affix strings. */
void MutablePatternModifier::enterCharSequenceMode(bool isPrefix) {
    U_ASSERT(!inCharSequenceMode);
    inCharSequenceMode = true;

    // Should the output render '+' where '-' would normally appear in the pattern?
    plusReplacesMinusSign = !isNegative && (
            signDisplay == UNUM_SIGN_ALWAYS ||
            signDisplay == UNUM_SIGN_ACCOUNTING_ALWAYS) &&
                            patternInfo->positiveHasPlusSign() == false;

    // Should we use the affix from the negative subpattern? (If not, we will use the positive subpattern.)
    bool useNegativeAffixPattern = patternInfo->hasNegativeSubpattern() && (
            isNegative || (patternInfo->negativeHasMinusSign() && plusReplacesMinusSign));

    // Resolve the flags for the affix pattern.
    fFlags = 0;
    if (useNegativeAffixPattern) {
        fFlags |= AffixPatternProvider::AFFIX_NEGATIVE_SUBPATTERN;
    }
    if (isPrefix) {
        fFlags |= AffixPatternProvider::AFFIX_PREFIX;
    }
    if (plural != StandardPlural::Form::COUNT) {
        U_ASSERT(plural == (AffixPatternProvider::AFFIX_PLURAL_MASK & plural));
        fFlags |= plural;
    }

    // Should we prepend a sign to the pattern?
    if (!isPrefix || useNegativeAffixPattern) {
        prependSign = false;
    } else if (isNegative) {
        prependSign = signDisplay != UNUM_SIGN_NEVER;
    } else {
        prependSign = plusReplacesMinusSign;
    }

    // Finally, compute the length of the affix pattern.
    fLength = patternInfo->length(fFlags) + (prependSign ? 1 : 0);
}

void MutablePatternModifier::exitCharSequenceMode() {
    U_ASSERT(inCharSequenceMode);
    inCharSequenceMode = false;
}

int32_t MutablePatternModifier::length() const {
    U_ASSERT(inCharSequenceMode);
    return fLength;
}

char16_t MutablePatternModifier::charAt(int32_t index) const {
    U_ASSERT(inCharSequenceMode);
    char16_t candidate;
    if (prependSign && index == 0) {
        candidate = u'-';
    } else if (prependSign) {
        candidate = patternInfo->charAt(fFlags, index - 1);
    } else {
        candidate = patternInfo->charAt(fFlags, index);
    }
    if (plusReplacesMinusSign && candidate == u'-') {
        return u'+';
    }
    if (perMilleReplacesPercent && candidate == u'%') {
        return u'‰';
    }
    return candidate;
}

UnicodeString MutablePatternModifier::toUnicodeString() const {
    // Never called by AffixUtils
    U_ASSERT(false);
    return UnicodeString();
}

#endif /* #if !UCONFIG_NO_FORMATTING */
