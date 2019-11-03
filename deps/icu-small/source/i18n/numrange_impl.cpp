// © 2018 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

// Allow implicit conversion from char16_t* to UnicodeString for this file:
// Helpful in toString methods and elsewhere.
#define UNISTR_FROM_STRING_EXPLICIT

#include "unicode/numberrangeformatter.h"
#include "numrange_impl.h"
#include "patternprops.h"
#include "uresimp.h"
#include "util.h"

using namespace icu;
using namespace icu::number;
using namespace icu::number::impl;

namespace {

// Helper function for 2-dimensional switch statement
constexpr int8_t identity2d(UNumberRangeIdentityFallback a, UNumberRangeIdentityResult b) {
    return static_cast<int8_t>(a) | (static_cast<int8_t>(b) << 4);
}


struct NumberRangeData {
    SimpleFormatter rangePattern;
    SimpleFormatter approximatelyPattern;
};

class NumberRangeDataSink : public ResourceSink {
  public:
    NumberRangeDataSink(NumberRangeData& data) : fData(data) {}

    void put(const char* key, ResourceValue& value, UBool /*noFallback*/, UErrorCode& status) U_OVERRIDE {
        ResourceTable miscTable = value.getTable(status);
        if (U_FAILURE(status)) { return; }
        for (int i = 0; miscTable.getKeyAndValue(i, key, value); i++) {
            if (uprv_strcmp(key, "range") == 0) {
                if (hasRangeData()) {
                    continue; // have already seen this pattern
                }
                fData.rangePattern = {value.getUnicodeString(status), status};
            } else if (uprv_strcmp(key, "approximately") == 0) {
                if (hasApproxData()) {
                    continue; // have already seen this pattern
                }
                fData.approximatelyPattern = {value.getUnicodeString(status), status};
            }
        }
    }

    bool hasRangeData() {
        return fData.rangePattern.getArgumentLimit() != 0;
    }

    bool hasApproxData() {
        return fData.approximatelyPattern.getArgumentLimit() != 0;
    }

    bool isComplete() {
        return hasRangeData() && hasApproxData();
    }

    void fillInDefaults(UErrorCode& status) {
        if (!hasRangeData()) {
            fData.rangePattern = {u"{0}–{1}", status};
        }
        if (!hasApproxData()) {
            fData.approximatelyPattern = {u"~{0}", status};
        }
    }

  private:
    NumberRangeData& fData;
};

void getNumberRangeData(const char* localeName, const char* nsName, NumberRangeData& data, UErrorCode& status) {
    if (U_FAILURE(status)) { return; }
    LocalUResourceBundlePointer rb(ures_open(NULL, localeName, &status));
    if (U_FAILURE(status)) { return; }
    NumberRangeDataSink sink(data);

    CharString dataPath;
    dataPath.append("NumberElements/", -1, status);
    dataPath.append(nsName, -1, status);
    dataPath.append("/miscPatterns", -1, status);
    if (U_FAILURE(status)) { return; }

    UErrorCode localStatus = U_ZERO_ERROR;
    ures_getAllItemsWithFallback(rb.getAlias(), dataPath.data(), sink, localStatus);
    if (U_FAILURE(localStatus) && localStatus != U_MISSING_RESOURCE_ERROR) {
        status = localStatus;
        return;
    }

    // Fall back to latn if necessary
    if (!sink.isComplete()) {
        ures_getAllItemsWithFallback(rb.getAlias(), "NumberElements/latn/miscPatterns", sink, status);
    }

    sink.fillInDefaults(status);
}

class PluralRangesDataSink : public ResourceSink {
  public:
    PluralRangesDataSink(StandardPluralRanges& output) : fOutput(output) {}

    void put(const char* /*key*/, ResourceValue& value, UBool /*noFallback*/, UErrorCode& status) U_OVERRIDE {
        ResourceArray entriesArray = value.getArray(status);
        if (U_FAILURE(status)) { return; }
        fOutput.setCapacity(entriesArray.getSize());
        for (int i = 0; entriesArray.getValue(i, value); i++) {
            ResourceArray pluralFormsArray = value.getArray(status);
            if (U_FAILURE(status)) { return; }
            pluralFormsArray.getValue(0, value);
            StandardPlural::Form first = StandardPlural::fromString(value.getUnicodeString(status), status);
            if (U_FAILURE(status)) { return; }
            pluralFormsArray.getValue(1, value);
            StandardPlural::Form second = StandardPlural::fromString(value.getUnicodeString(status), status);
            if (U_FAILURE(status)) { return; }
            pluralFormsArray.getValue(2, value);
            StandardPlural::Form result = StandardPlural::fromString(value.getUnicodeString(status), status);
            if (U_FAILURE(status)) { return; }
            fOutput.addPluralRange(first, second, result);
        }
    }

  private:
    StandardPluralRanges& fOutput;
};

void getPluralRangesData(const Locale& locale, StandardPluralRanges& output, UErrorCode& status) {
    if (U_FAILURE(status)) { return; }
    LocalUResourceBundlePointer rb(ures_openDirect(nullptr, "pluralRanges", &status));
    if (U_FAILURE(status)) { return; }

    CharString dataPath;
    dataPath.append("locales/", -1, status);
    dataPath.append(locale.getLanguage(), -1, status);
    if (U_FAILURE(status)) { return; }
    int32_t setLen;
    // Not all languages are covered: fail gracefully
    UErrorCode internalStatus = U_ZERO_ERROR;
    const UChar* set = ures_getStringByKeyWithFallback(rb.getAlias(), dataPath.data(), &setLen, &internalStatus);
    if (U_FAILURE(internalStatus)) { return; }

    dataPath.clear();
    dataPath.append("rules/", -1, status);
    dataPath.appendInvariantChars(set, setLen, status);
    if (U_FAILURE(status)) { return; }
    PluralRangesDataSink sink(output);
    ures_getAllItemsWithFallback(rb.getAlias(), dataPath.data(), sink, status);
    if (U_FAILURE(status)) { return; }
}

} // namespace


void StandardPluralRanges::initialize(const Locale& locale, UErrorCode& status) {
    getPluralRangesData(locale, *this, status);
}

void StandardPluralRanges::addPluralRange(
        StandardPlural::Form first,
        StandardPlural::Form second,
        StandardPlural::Form result) {
    U_ASSERT(fTriplesLen < fTriples.getCapacity());
    fTriples[fTriplesLen] = {first, second, result};
    fTriplesLen++;
}

void StandardPluralRanges::setCapacity(int32_t length) {
    if (length > fTriples.getCapacity()) {
        fTriples.resize(length, 0);
    }
}

StandardPlural::Form
StandardPluralRanges::resolve(StandardPlural::Form first, StandardPlural::Form second) const {
    for (int32_t i=0; i<fTriplesLen; i++) {
        const auto& triple = fTriples[i];
        if (triple.first == first && triple.second == second) {
            return triple.result;
        }
    }
    // Default fallback
    return StandardPlural::OTHER;
}


NumberRangeFormatterImpl::NumberRangeFormatterImpl(const RangeMacroProps& macros, UErrorCode& status)
    : formatterImpl1(macros.formatter1.fMacros, status),
      formatterImpl2(macros.formatter2.fMacros, status),
      fSameFormatters(macros.singleFormatter),
      fCollapse(macros.collapse),
      fIdentityFallback(macros.identityFallback) {

    const char* nsName = formatterImpl1.getRawMicroProps().nsName;
    if (uprv_strcmp(nsName, formatterImpl2.getRawMicroProps().nsName) != 0) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }

    NumberRangeData data;
    getNumberRangeData(macros.locale.getName(), nsName, data, status);
    if (U_FAILURE(status)) { return; }
    fRangeFormatter = data.rangePattern;
    fApproximatelyModifier = {data.approximatelyPattern, UNUM_FIELD_COUNT, false};

    // TODO: Get locale from PluralRules instead?
    fPluralRanges.initialize(macros.locale, status);
    if (U_FAILURE(status)) { return; }
}

void NumberRangeFormatterImpl::format(UFormattedNumberRangeData& data, bool equalBeforeRounding, UErrorCode& status) const {
    if (U_FAILURE(status)) {
        return;
    }

    MicroProps micros1;
    MicroProps micros2;
    formatterImpl1.preProcess(data.quantity1, micros1, status);
    if (fSameFormatters) {
        formatterImpl1.preProcess(data.quantity2, micros2, status);
    } else {
        formatterImpl2.preProcess(data.quantity2, micros2, status);
    }
    if (U_FAILURE(status)) {
        return;
    }

    // If any of the affixes are different, an identity is not possible
    // and we must use formatRange().
    // TODO: Write this as MicroProps operator==() ?
    // TODO: Avoid the redundancy of these equality operations with the
    // ones in formatRange?
    if (!micros1.modInner->semanticallyEquivalent(*micros2.modInner)
            || !micros1.modMiddle->semanticallyEquivalent(*micros2.modMiddle)
            || !micros1.modOuter->semanticallyEquivalent(*micros2.modOuter)) {
        formatRange(data, micros1, micros2, status);
        data.identityResult = UNUM_IDENTITY_RESULT_NOT_EQUAL;
        return;
    }

    // Check for identity
    if (equalBeforeRounding) {
        data.identityResult = UNUM_IDENTITY_RESULT_EQUAL_BEFORE_ROUNDING;
    } else if (data.quantity1 == data.quantity2) {
        data.identityResult = UNUM_IDENTITY_RESULT_EQUAL_AFTER_ROUNDING;
    } else {
        data.identityResult = UNUM_IDENTITY_RESULT_NOT_EQUAL;
    }

    switch (identity2d(fIdentityFallback, data.identityResult)) {
        case identity2d(UNUM_IDENTITY_FALLBACK_RANGE,
                        UNUM_IDENTITY_RESULT_NOT_EQUAL):
        case identity2d(UNUM_IDENTITY_FALLBACK_RANGE,
                        UNUM_IDENTITY_RESULT_EQUAL_AFTER_ROUNDING):
        case identity2d(UNUM_IDENTITY_FALLBACK_RANGE,
                        UNUM_IDENTITY_RESULT_EQUAL_BEFORE_ROUNDING):
        case identity2d(UNUM_IDENTITY_FALLBACK_APPROXIMATELY,
                        UNUM_IDENTITY_RESULT_NOT_EQUAL):
        case identity2d(UNUM_IDENTITY_FALLBACK_APPROXIMATELY_OR_SINGLE_VALUE,
                        UNUM_IDENTITY_RESULT_NOT_EQUAL):
        case identity2d(UNUM_IDENTITY_FALLBACK_SINGLE_VALUE,
                        UNUM_IDENTITY_RESULT_NOT_EQUAL):
            formatRange(data, micros1, micros2, status);
            break;

        case identity2d(UNUM_IDENTITY_FALLBACK_APPROXIMATELY,
                        UNUM_IDENTITY_RESULT_EQUAL_AFTER_ROUNDING):
        case identity2d(UNUM_IDENTITY_FALLBACK_APPROXIMATELY,
                        UNUM_IDENTITY_RESULT_EQUAL_BEFORE_ROUNDING):
        case identity2d(UNUM_IDENTITY_FALLBACK_APPROXIMATELY_OR_SINGLE_VALUE,
                        UNUM_IDENTITY_RESULT_EQUAL_AFTER_ROUNDING):
            formatApproximately(data, micros1, micros2, status);
            break;

        case identity2d(UNUM_IDENTITY_FALLBACK_APPROXIMATELY_OR_SINGLE_VALUE,
                        UNUM_IDENTITY_RESULT_EQUAL_BEFORE_ROUNDING):
        case identity2d(UNUM_IDENTITY_FALLBACK_SINGLE_VALUE,
                        UNUM_IDENTITY_RESULT_EQUAL_AFTER_ROUNDING):
        case identity2d(UNUM_IDENTITY_FALLBACK_SINGLE_VALUE,
                        UNUM_IDENTITY_RESULT_EQUAL_BEFORE_ROUNDING):
            formatSingleValue(data, micros1, micros2, status);
            break;

        default:
            UPRV_UNREACHABLE;
    }
}


void NumberRangeFormatterImpl::formatSingleValue(UFormattedNumberRangeData& data,
                                                 MicroProps& micros1, MicroProps& micros2,
                                                 UErrorCode& status) const {
    if (U_FAILURE(status)) { return; }
    if (fSameFormatters) {
        int32_t length = NumberFormatterImpl::writeNumber(micros1, data.quantity1, data.getStringRef(), 0, status);
        NumberFormatterImpl::writeAffixes(micros1, data.getStringRef(), 0, length, status);
    } else {
        formatRange(data, micros1, micros2, status);
    }
}


void NumberRangeFormatterImpl::formatApproximately (UFormattedNumberRangeData& data,
                                                    MicroProps& micros1, MicroProps& micros2,
                                                    UErrorCode& status) const {
    if (U_FAILURE(status)) { return; }
    if (fSameFormatters) {
        int32_t length = NumberFormatterImpl::writeNumber(micros1, data.quantity1, data.getStringRef(), 0, status);
        // HEURISTIC: Desired modifier order: inner, middle, approximately, outer.
        length += micros1.modInner->apply(data.getStringRef(), 0, length, status);
        length += micros1.modMiddle->apply(data.getStringRef(), 0, length, status);
        length += fApproximatelyModifier.apply(data.getStringRef(), 0, length, status);
        micros1.modOuter->apply(data.getStringRef(), 0, length, status);
    } else {
        formatRange(data, micros1, micros2, status);
    }
}


void NumberRangeFormatterImpl::formatRange(UFormattedNumberRangeData& data,
                                           MicroProps& micros1, MicroProps& micros2,
                                           UErrorCode& status) const {
    if (U_FAILURE(status)) { return; }

    // modInner is always notation (scientific); collapsable in ALL.
    // modOuter is always units; collapsable in ALL, AUTO, and UNIT.
    // modMiddle could be either; collapsable in ALL and sometimes AUTO and UNIT.
    // Never collapse an outer mod but not an inner mod.
    bool collapseOuter, collapseMiddle, collapseInner;
    switch (fCollapse) {
        case UNUM_RANGE_COLLAPSE_ALL:
        case UNUM_RANGE_COLLAPSE_AUTO:
        case UNUM_RANGE_COLLAPSE_UNIT:
        {
            // OUTER MODIFIER
            collapseOuter = micros1.modOuter->semanticallyEquivalent(*micros2.modOuter);

            if (!collapseOuter) {
                // Never collapse inner mods if outer mods are not collapsable
                collapseMiddle = false;
                collapseInner = false;
                break;
            }

            // MIDDLE MODIFIER
            collapseMiddle = micros1.modMiddle->semanticallyEquivalent(*micros2.modMiddle);

            if (!collapseMiddle) {
                // Never collapse inner mods if outer mods are not collapsable
                collapseInner = false;
                break;
            }

            // MIDDLE MODIFIER HEURISTICS
            // (could disable collapsing of the middle modifier)
            // The modifiers are equal by this point, so we can look at just one of them.
            const Modifier* mm = micros1.modMiddle;
            if (fCollapse == UNUM_RANGE_COLLAPSE_UNIT) {
                // Only collapse if the modifier is a unit.
                // TODO: Make a better way to check for a unit?
                // TODO: Handle case where the modifier has both notation and unit (compact currency)?
                if (!mm->containsField(UNUM_CURRENCY_FIELD) && !mm->containsField(UNUM_PERCENT_FIELD)) {
                    collapseMiddle = false;
                }
            } else if (fCollapse == UNUM_RANGE_COLLAPSE_AUTO) {
                // Heuristic as of ICU 63: collapse only if the modifier is more than one code point.
                if (mm->getCodePointCount() <= 1) {
                    collapseMiddle = false;
                }
            }

            if (!collapseMiddle || fCollapse != UNUM_RANGE_COLLAPSE_ALL) {
                collapseInner = false;
                break;
            }

            // INNER MODIFIER
            collapseInner = micros1.modInner->semanticallyEquivalent(*micros2.modInner);

            // All done checking for collapsability.
            break;
        }

        default:
            collapseOuter = false;
            collapseMiddle = false;
            collapseInner = false;
            break;
    }

    FormattedStringBuilder& string = data.getStringRef();
    int32_t lengthPrefix = 0;
    int32_t length1 = 0;
    int32_t lengthInfix = 0;
    int32_t length2 = 0;
    int32_t lengthSuffix = 0;

    // Use #define so that these are evaluated at the call site.
    #define UPRV_INDEX_0 (lengthPrefix)
    #define UPRV_INDEX_1 (lengthPrefix + length1)
    #define UPRV_INDEX_2 (lengthPrefix + length1 + lengthInfix)
    #define UPRV_INDEX_3 (lengthPrefix + length1 + lengthInfix + length2)

    int32_t lengthRange = SimpleModifier::formatTwoArgPattern(
        fRangeFormatter,
        string,
        0,
        &lengthPrefix,
        &lengthSuffix,
        UNUM_FIELD_COUNT,
        status);
    if (U_FAILURE(status)) { return; }
    lengthInfix = lengthRange - lengthPrefix - lengthSuffix;
    U_ASSERT(lengthInfix > 0);

    // SPACING HEURISTIC
    // Add spacing unless all modifiers are collapsed.
    // TODO: add API to control this?
    // TODO: Use a data-driven heuristic like currency spacing?
    // TODO: Use Unicode [:whitespace:] instead of PatternProps whitespace? (consider speed implications)
    {
        bool repeatInner = !collapseInner && micros1.modInner->getCodePointCount() > 0;
        bool repeatMiddle = !collapseMiddle && micros1.modMiddle->getCodePointCount() > 0;
        bool repeatOuter = !collapseOuter && micros1.modOuter->getCodePointCount() > 0;
        if (repeatInner || repeatMiddle || repeatOuter) {
            // Add spacing if there is not already spacing
            if (!PatternProps::isWhiteSpace(string.charAt(UPRV_INDEX_1))) {
                lengthInfix += string.insertCodePoint(UPRV_INDEX_1, u'\u0020', UNUM_FIELD_COUNT, status);
            }
            if (!PatternProps::isWhiteSpace(string.charAt(UPRV_INDEX_2 - 1))) {
                lengthInfix += string.insertCodePoint(UPRV_INDEX_2, u'\u0020', UNUM_FIELD_COUNT, status);
            }
        }
    }

    length1 += NumberFormatterImpl::writeNumber(micros1, data.quantity1, string, UPRV_INDEX_0, status);
    length2 += NumberFormatterImpl::writeNumber(micros2, data.quantity2, string, UPRV_INDEX_2, status);

    // TODO: Support padding?

    if (collapseInner) {
        // Note: this is actually a mix of prefix and suffix, but adding to infix length works
        const Modifier& mod = resolveModifierPlurals(*micros1.modInner, *micros2.modInner);
        lengthInfix += mod.apply(string, UPRV_INDEX_0, UPRV_INDEX_3, status);
    } else {
        length1 += micros1.modInner->apply(string, UPRV_INDEX_0, UPRV_INDEX_1, status);
        length2 += micros2.modInner->apply(string, UPRV_INDEX_2, UPRV_INDEX_3, status);
    }

    if (collapseMiddle) {
        // Note: this is actually a mix of prefix and suffix, but adding to infix length works
        const Modifier& mod = resolveModifierPlurals(*micros1.modMiddle, *micros2.modMiddle);
        lengthInfix += mod.apply(string, UPRV_INDEX_0, UPRV_INDEX_3, status);
    } else {
        length1 += micros1.modMiddle->apply(string, UPRV_INDEX_0, UPRV_INDEX_1, status);
        length2 += micros2.modMiddle->apply(string, UPRV_INDEX_2, UPRV_INDEX_3, status);
    }

    if (collapseOuter) {
        // Note: this is actually a mix of prefix and suffix, but adding to infix length works
        const Modifier& mod = resolveModifierPlurals(*micros1.modOuter, *micros2.modOuter);
        lengthInfix += mod.apply(string, UPRV_INDEX_0, UPRV_INDEX_3, status);
    } else {
        length1 += micros1.modOuter->apply(string, UPRV_INDEX_0, UPRV_INDEX_1, status);
        length2 += micros2.modOuter->apply(string, UPRV_INDEX_2, UPRV_INDEX_3, status);
    }
}


const Modifier&
NumberRangeFormatterImpl::resolveModifierPlurals(const Modifier& first, const Modifier& second) const {
    Modifier::Parameters parameters;
    first.getParameters(parameters);
    if (parameters.obj == nullptr) {
        // No plural form; return a fallback (e.g., the first)
        return first;
    }
    StandardPlural::Form firstPlural = parameters.plural;

    second.getParameters(parameters);
    if (parameters.obj == nullptr) {
        // No plural form; return a fallback (e.g., the first)
        return first;
    }
    StandardPlural::Form secondPlural = parameters.plural;

    // Get the required plural form from data
    StandardPlural::Form resultPlural = fPluralRanges.resolve(firstPlural, secondPlural);

    // Get and return the new Modifier
    const Modifier* mod = parameters.obj->getModifier(parameters.signum, resultPlural);
    U_ASSERT(mod != nullptr);
    return *mod;
}



#endif /* #if !UCONFIG_NO_FORMATTING */
