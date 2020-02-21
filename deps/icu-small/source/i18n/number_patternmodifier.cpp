// © 2017 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "cstring.h"
#include "number_patternmodifier.h"
#include "unicode/dcfmtsym.h"
#include "unicode/ucurr.h"
#include "unicode/unistr.h"
#include "number_microprops.h"

using namespace icu;
using namespace icu::number;
using namespace icu::number::impl;


AffixPatternProvider::~AffixPatternProvider() = default;


MutablePatternModifier::MutablePatternModifier(bool isStrong)
        : fStrong(isStrong) {}

void MutablePatternModifier::setPatternInfo(const AffixPatternProvider* patternInfo, Field field) {
    fPatternInfo = patternInfo;
    fField = field;
}

void MutablePatternModifier::setPatternAttributes(UNumberSignDisplay signDisplay, bool perMille) {
    fSignDisplay = signDisplay;
    fPerMilleReplacesPercent = perMille;
}

void MutablePatternModifier::setSymbols(const DecimalFormatSymbols* symbols,
                                        const CurrencySymbols* currencySymbols,
                                        const UNumberUnitWidth unitWidth, const PluralRules* rules) {
    U_ASSERT((rules != nullptr) == needsPlurals());
    fSymbols = symbols;
    fCurrencySymbols = currencySymbols;
    fUnitWidth = unitWidth;
    fRules = rules;
}

void MutablePatternModifier::setNumberProperties(Signum signum, StandardPlural::Form plural) {
    fSignum = signum;
    fPlural = plural;
}

bool MutablePatternModifier::needsPlurals() const {
    UErrorCode statusLocal = U_ZERO_ERROR;
    return fPatternInfo->containsSymbolType(AffixPatternType::TYPE_CURRENCY_TRIPLE, statusLocal);
    // Silently ignore any error codes.
}

ImmutablePatternModifier* MutablePatternModifier::createImmutable(UErrorCode& status) {
    return createImmutableAndChain(nullptr, status);
}

ImmutablePatternModifier*
MutablePatternModifier::createImmutableAndChain(const MicroPropsGenerator* parent, UErrorCode& status) {

    // TODO: Move StandardPlural VALUES to standardplural.h
    static const StandardPlural::Form STANDARD_PLURAL_VALUES[] = {
            StandardPlural::Form::ZERO,
            StandardPlural::Form::ONE,
            StandardPlural::Form::TWO,
            StandardPlural::Form::FEW,
            StandardPlural::Form::MANY,
            StandardPlural::Form::OTHER};

    auto pm = new AdoptingModifierStore();
    if (pm == nullptr) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return nullptr;
    }

    if (needsPlurals()) {
        // Slower path when we require the plural keyword.
        for (StandardPlural::Form plural : STANDARD_PLURAL_VALUES) {
            setNumberProperties(SIGNUM_POS, plural);
            pm->adoptModifier(SIGNUM_POS, plural, createConstantModifier(status));
            setNumberProperties(SIGNUM_ZERO, plural);
            pm->adoptModifier(SIGNUM_ZERO, plural, createConstantModifier(status));
            setNumberProperties(SIGNUM_NEG, plural);
            pm->adoptModifier(SIGNUM_NEG, plural, createConstantModifier(status));
        }
        if (U_FAILURE(status)) {
            delete pm;
            return nullptr;
        }
        return new ImmutablePatternModifier(pm, fRules, parent);  // adopts pm
    } else {
        // Faster path when plural keyword is not needed.
        setNumberProperties(SIGNUM_POS, StandardPlural::Form::COUNT);
        pm->adoptModifierWithoutPlural(SIGNUM_POS, createConstantModifier(status));
        setNumberProperties(SIGNUM_ZERO, StandardPlural::Form::COUNT);
        pm->adoptModifierWithoutPlural(SIGNUM_ZERO, createConstantModifier(status));
        setNumberProperties(SIGNUM_NEG, StandardPlural::Form::COUNT);
        pm->adoptModifierWithoutPlural(SIGNUM_NEG, createConstantModifier(status));
        if (U_FAILURE(status)) {
            delete pm;
            return nullptr;
        }
        return new ImmutablePatternModifier(pm, nullptr, parent);  // adopts pm
    }
}

ConstantMultiFieldModifier* MutablePatternModifier::createConstantModifier(UErrorCode& status) {
    FormattedStringBuilder a;
    FormattedStringBuilder b;
    insertPrefix(a, 0, status);
    insertSuffix(b, 0, status);
    if (fPatternInfo->hasCurrencySign()) {
        return new CurrencySpacingEnabledModifier(
                a, b, !fPatternInfo->hasBody(), fStrong, *fSymbols, status);
    } else {
        return new ConstantMultiFieldModifier(a, b, !fPatternInfo->hasBody(), fStrong);
    }
}

ImmutablePatternModifier::ImmutablePatternModifier(AdoptingModifierStore* pm, const PluralRules* rules,
                                                   const MicroPropsGenerator* parent)
        : pm(pm), rules(rules), parent(parent) {}

void ImmutablePatternModifier::processQuantity(DecimalQuantity& quantity, MicroProps& micros,
                                               UErrorCode& status) const {
    parent->processQuantity(quantity, micros, status);
    applyToMicros(micros, quantity, status);
}

void ImmutablePatternModifier::applyToMicros(
        MicroProps& micros, const DecimalQuantity& quantity, UErrorCode& status) const {
    if (rules == nullptr) {
        micros.modMiddle = pm->getModifierWithoutPlural(quantity.signum());
    } else {
        StandardPlural::Form pluralForm = utils::getPluralSafe(micros.rounder, rules, quantity, status);
        micros.modMiddle = pm->getModifier(quantity.signum(), pluralForm);
    }
}

const Modifier* ImmutablePatternModifier::getModifier(Signum signum, StandardPlural::Form plural) const {
    if (rules == nullptr) {
        return pm->getModifierWithoutPlural(signum);
    } else {
        return pm->getModifier(signum, plural);
    }
}


/** Used by the unsafe code path. */
MicroPropsGenerator& MutablePatternModifier::addToChain(const MicroPropsGenerator* parent) {
    fParent = parent;
    return *this;
}

void MutablePatternModifier::processQuantity(DecimalQuantity& fq, MicroProps& micros,
                                             UErrorCode& status) const {
    fParent->processQuantity(fq, micros, status);
    // The unsafe code path performs self-mutation, so we need a const_cast.
    // This method needs to be const because it overrides a const method in the parent class.
    auto nonConstThis = const_cast<MutablePatternModifier*>(this);
    if (needsPlurals()) {
        StandardPlural::Form pluralForm = utils::getPluralSafe(micros.rounder, fRules, fq, status);
        nonConstThis->setNumberProperties(fq.signum(), pluralForm);
    } else {
        nonConstThis->setNumberProperties(fq.signum(), StandardPlural::Form::COUNT);
    }
    micros.modMiddle = this;
}

int32_t MutablePatternModifier::apply(FormattedStringBuilder& output, int32_t leftIndex, int32_t rightIndex,
                                      UErrorCode& status) const {
    // The unsafe code path performs self-mutation, so we need a const_cast.
    // This method needs to be const because it overrides a const method in the parent class.
    auto nonConstThis = const_cast<MutablePatternModifier*>(this);
    int32_t prefixLen = nonConstThis->insertPrefix(output, leftIndex, status);
    int32_t suffixLen = nonConstThis->insertSuffix(output, rightIndex + prefixLen, status);
    // If the pattern had no decimal stem body (like #,##0.00), overwrite the value.
    int32_t overwriteLen = 0;
    if (!fPatternInfo->hasBody()) {
        overwriteLen = output.splice(
                leftIndex + prefixLen,
                rightIndex + prefixLen,
                UnicodeString(),
                0,
                0,
                UNUM_FIELD_COUNT,
                status);
    }
    CurrencySpacingEnabledModifier::applyCurrencySpacing(
            output,
            leftIndex,
            prefixLen,
            rightIndex + overwriteLen + prefixLen,
            suffixLen,
            *fSymbols,
            status);
    return prefixLen + overwriteLen + suffixLen;
}

int32_t MutablePatternModifier::getPrefixLength() const {
    // The unsafe code path performs self-mutation, so we need a const_cast.
    // This method needs to be const because it overrides a const method in the parent class.
    auto nonConstThis = const_cast<MutablePatternModifier*>(this);

    // Enter and exit CharSequence Mode to get the length.
    UErrorCode status = U_ZERO_ERROR; // status fails only with an iilegal argument exception
    nonConstThis->prepareAffix(true);
    int result = AffixUtils::unescapedCodePointCount(currentAffix, *this, status);  // prefix length
    return result;
}

int32_t MutablePatternModifier::getCodePointCount() const {
    // The unsafe code path performs self-mutation, so we need a const_cast.
    // This method needs to be const because it overrides a const method in the parent class.
    auto nonConstThis = const_cast<MutablePatternModifier*>(this);

    // Render the affixes to get the length
    UErrorCode status = U_ZERO_ERROR; // status fails only with an iilegal argument exception
    nonConstThis->prepareAffix(true);
    int result = AffixUtils::unescapedCodePointCount(currentAffix, *this, status);  // prefix length
    nonConstThis->prepareAffix(false);
    result += AffixUtils::unescapedCodePointCount(currentAffix, *this, status);  // suffix length
    return result;
}

bool MutablePatternModifier::isStrong() const {
    return fStrong;
}

bool MutablePatternModifier::containsField(UNumberFormatFields field) const {
    (void)field;
    // This method is not currently used.
    UPRV_UNREACHABLE;
}

void MutablePatternModifier::getParameters(Parameters& output) const {
    (void)output;
    // This method is not currently used.
    UPRV_UNREACHABLE;
}

bool MutablePatternModifier::semanticallyEquivalent(const Modifier& other) const {
    (void)other;
    // This method is not currently used.
    UPRV_UNREACHABLE;
}

int32_t MutablePatternModifier::insertPrefix(FormattedStringBuilder& sb, int position, UErrorCode& status) {
    prepareAffix(true);
    int32_t length = AffixUtils::unescape(currentAffix, sb, position, *this, fField, status);
    return length;
}

int32_t MutablePatternModifier::insertSuffix(FormattedStringBuilder& sb, int position, UErrorCode& status) {
    prepareAffix(false);
    int32_t length = AffixUtils::unescape(currentAffix, sb, position, *this, fField, status);
    return length;
}

/** This method contains the heart of the logic for rendering LDML affix strings. */
void MutablePatternModifier::prepareAffix(bool isPrefix) {
    PatternStringUtils::patternInfoToStringBuilder(
            *fPatternInfo, isPrefix, fSignum, fSignDisplay, fPlural, fPerMilleReplacesPercent, currentAffix);
}

UnicodeString MutablePatternModifier::getSymbol(AffixPatternType type) const {
    UErrorCode localStatus = U_ZERO_ERROR;
    switch (type) {
        case AffixPatternType::TYPE_MINUS_SIGN:
            return fSymbols->getSymbol(DecimalFormatSymbols::ENumberFormatSymbol::kMinusSignSymbol);
        case AffixPatternType::TYPE_PLUS_SIGN:
            return fSymbols->getSymbol(DecimalFormatSymbols::ENumberFormatSymbol::kPlusSignSymbol);
        case AffixPatternType::TYPE_PERCENT:
            return fSymbols->getSymbol(DecimalFormatSymbols::ENumberFormatSymbol::kPercentSymbol);
        case AffixPatternType::TYPE_PERMILLE:
            return fSymbols->getSymbol(DecimalFormatSymbols::ENumberFormatSymbol::kPerMillSymbol);
        case AffixPatternType::TYPE_CURRENCY_SINGLE: {
            // UnitWidth ISO and HIDDEN overrides the singular currency symbol.
            if (fUnitWidth == UNumberUnitWidth::UNUM_UNIT_WIDTH_ISO_CODE) {
                return fCurrencySymbols->getIntlCurrencySymbol(localStatus);
            } else if (fUnitWidth == UNumberUnitWidth::UNUM_UNIT_WIDTH_HIDDEN) {
                return UnicodeString();
            } else if (fUnitWidth == UNumberUnitWidth::UNUM_UNIT_WIDTH_NARROW) {
                return fCurrencySymbols->getNarrowCurrencySymbol(localStatus);
            } else {
                return fCurrencySymbols->getCurrencySymbol(localStatus);
            }
        }
        case AffixPatternType::TYPE_CURRENCY_DOUBLE:
            return fCurrencySymbols->getIntlCurrencySymbol(localStatus);
        case AffixPatternType::TYPE_CURRENCY_TRIPLE:
            // NOTE: This is the code path only for patterns containing "¤¤¤".
            // Plural currencies set via the API are formatted in LongNameHandler.
            // This code path is used by DecimalFormat via CurrencyPluralInfo.
            U_ASSERT(fPlural != StandardPlural::Form::COUNT);
            return fCurrencySymbols->getPluralName(fPlural, localStatus);
        case AffixPatternType::TYPE_CURRENCY_QUAD:
            return UnicodeString(u"\uFFFD");
        case AffixPatternType::TYPE_CURRENCY_QUINT:
            return UnicodeString(u"\uFFFD");
        default:
            UPRV_UNREACHABLE;
    }
}

UnicodeString MutablePatternModifier::toUnicodeString() const {
    // Never called by AffixUtils
    UPRV_UNREACHABLE;
}

#endif /* #if !UCONFIG_NO_FORMATTING */
