// © 2017 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/simpleformatter.h"
#include "unicode/ures.h"
#include "ureslocs.h"
#include "charstr.h"
#include "uresimp.h"
#include "measunit_impl.h"
#include "number_longnames.h"
#include "number_microprops.h"
#include <algorithm>
#include "cstring.h"
#include "util.h"

using namespace icu;
using namespace icu::number;
using namespace icu::number::impl;

namespace {

/**
 * Display Name (this format has no placeholder).
 *
 * Used as an index into the LongNameHandler::simpleFormats array. Units
 * resources cover the normal set of PluralRules keys, as well as `dnam` and
 * `per` forms.
 */
constexpr int32_t DNAM_INDEX = StandardPlural::Form::COUNT;
/**
 * "per" form (e.g. "{0} per day" is day's "per" form).
 *
 * Used as an index into the LongNameHandler::simpleFormats array. Units
 * resources cover the normal set of PluralRules keys, as well as `dnam` and
 * `per` forms.
 */
constexpr int32_t PER_INDEX = StandardPlural::Form::COUNT + 1;
// Number of keys in the array populated by PluralTableSink.
constexpr int32_t ARRAY_LENGTH = StandardPlural::Form::COUNT + 2;

static int32_t getIndex(const char* pluralKeyword, UErrorCode& status) {
    // pluralKeyword can also be "dnam" or "per"
    if (uprv_strcmp(pluralKeyword, "dnam") == 0) {
        return DNAM_INDEX;
    } else if (uprv_strcmp(pluralKeyword, "per") == 0) {
        return PER_INDEX;
    } else {
        StandardPlural::Form plural = StandardPlural::fromString(pluralKeyword, status);
        return plural;
    }
}

// Selects a string out of the `strings` array which corresponds to the
// specified plural form, with fallback to the OTHER form.
//
// The `strings` array must have ARRAY_LENGTH items: one corresponding to each
// of the plural forms, plus a display name ("dnam") and a "per" form.
static UnicodeString getWithPlural(
        const UnicodeString* strings,
        StandardPlural::Form plural,
        UErrorCode& status) {
    UnicodeString result = strings[plural];
    if (result.isBogus()) {
        result = strings[StandardPlural::Form::OTHER];
    }
    if (result.isBogus()) {
        // There should always be data in the "other" plural variant.
        status = U_INTERNAL_PROGRAM_ERROR;
    }
    return result;
}


//////////////////////////
/// BEGIN DATA LOADING ///
//////////////////////////

class PluralTableSink : public ResourceSink {
  public:
    explicit PluralTableSink(UnicodeString *outArray) : outArray(outArray) {
        // Initialize the array to bogus strings.
        for (int32_t i = 0; i < ARRAY_LENGTH; i++) {
            outArray[i].setToBogus();
        }
    }

    void put(const char *key, ResourceValue &value, UBool /*noFallback*/, UErrorCode &status) U_OVERRIDE {
        ResourceTable pluralsTable = value.getTable(status);
        if (U_FAILURE(status)) { return; }
        for (int32_t i = 0; pluralsTable.getKeyAndValue(i, key, value); ++i) {
            int32_t index = getIndex(key, status);
            if (U_FAILURE(status)) { return; }
            if (!outArray[index].isBogus()) {
                continue;
            }
            outArray[index] = value.getUnicodeString(status);
            if (U_FAILURE(status)) { return; }
        }
    }

  private:
    UnicodeString *outArray;
};

// NOTE: outArray MUST have room for all StandardPlural values.  No bounds checking is performed.

/**
 * Populates outArray with `locale`-specific values for `unit` through use of
 * PluralTableSink. Only the set of basic units are supported!
 *
 * Reading from resources *unitsNarrow* and *unitsShort* (for width
 * UNUM_UNIT_WIDTH_NARROW), or just *unitsShort* (for width
 * UNUM_UNIT_WIDTH_SHORT). For other widths, it reads just "units".
 *
 * @param unit must have a type and subtype (i.e. it must be a unit listed in
 *     gTypes and gSubTypes in measunit.cpp).
 * @param outArray must be of fixed length ARRAY_LENGTH.
 */
void getMeasureData(const Locale &locale, const MeasureUnit &unit, const UNumberUnitWidth &width,
                    UnicodeString *outArray, UErrorCode &status) {
    PluralTableSink sink(outArray);
    LocalUResourceBundlePointer unitsBundle(ures_open(U_ICUDATA_UNIT, locale.getName(), &status));
    if (U_FAILURE(status)) { return; }

    // Map duration-year-person, duration-week-person, etc. to duration-year, duration-week, ...
    // TODO(ICU-20400): Get duration-*-person data properly with aliases.
    StringPiece subtypeForResource;
    int32_t subtypeLen = static_cast<int32_t>(uprv_strlen(unit.getSubtype()));
    if (subtypeLen > 7 && uprv_strcmp(unit.getSubtype() + subtypeLen - 7, "-person") == 0) {
        subtypeForResource = {unit.getSubtype(), subtypeLen - 7};
    } else {
        subtypeForResource = unit.getSubtype();
    }

    CharString key;
    key.append("units", status);
    if (width == UNUM_UNIT_WIDTH_NARROW) {
        key.append("Narrow", status);
    } else if (width == UNUM_UNIT_WIDTH_SHORT) {
        key.append("Short", status);
    }
    key.append("/", status);
    key.append(unit.getType(), status);
    key.append("/", status);
    key.append(subtypeForResource, status);

    UErrorCode localStatus = U_ZERO_ERROR;
    ures_getAllItemsWithFallback(unitsBundle.getAlias(), key.data(), sink, localStatus);
    if (width == UNUM_UNIT_WIDTH_SHORT) {
        if (U_FAILURE(localStatus)) {
            status = localStatus;
        }
        return;
    }

    // TODO(ICU-13353): The fallback to short does not work in ICU4C.
    // Manually fall back to short (this is done automatically in Java).
    key.clear();
    key.append("unitsShort/", status);
    key.append(unit.getType(), status);
    key.append("/", status);
    key.append(subtypeForResource, status);
    ures_getAllItemsWithFallback(unitsBundle.getAlias(), key.data(), sink, status);
}

void getCurrencyLongNameData(const Locale &locale, const CurrencyUnit &currency, UnicodeString *outArray,
                             UErrorCode &status) {
    // In ICU4J, this method gets a CurrencyData from CurrencyData.provider.
    // TODO(ICU4J): Implement this without going through CurrencyData, like in ICU4C?
    PluralTableSink sink(outArray);
    LocalUResourceBundlePointer unitsBundle(ures_open(U_ICUDATA_CURR, locale.getName(), &status));
    if (U_FAILURE(status)) { return; }
    ures_getAllItemsWithFallback(unitsBundle.getAlias(), "CurrencyUnitPatterns", sink, status);
    if (U_FAILURE(status)) { return; }
    for (int32_t i = 0; i < StandardPlural::Form::COUNT; i++) {
        UnicodeString &pattern = outArray[i];
        if (pattern.isBogus()) {
            continue;
        }
        int32_t longNameLen = 0;
        const char16_t *longName = ucurr_getPluralName(
                currency.getISOCurrency(),
                locale.getName(),
                nullptr /* isChoiceFormat */,
                StandardPlural::getKeyword(static_cast<StandardPlural::Form>(i)),
                &longNameLen,
                &status);
        // Example pattern from data: "{0} {1}"
        // Example output after find-and-replace: "{0} US dollars"
        pattern.findAndReplace(UnicodeString(u"{1}"), UnicodeString(longName, longNameLen));
    }
}

UnicodeString getPerUnitFormat(const Locale& locale, const UNumberUnitWidth &width, UErrorCode& status) {
    LocalUResourceBundlePointer unitsBundle(ures_open(U_ICUDATA_UNIT, locale.getName(), &status));
    if (U_FAILURE(status)) { return {}; }
    CharString key;
    key.append("units", status);
    if (width == UNUM_UNIT_WIDTH_NARROW) {
        key.append("Narrow", status);
    } else if (width == UNUM_UNIT_WIDTH_SHORT) {
        key.append("Short", status);
    }
    key.append("/compound/per", status);
    int32_t len = 0;
    const UChar* ptr = ures_getStringByKeyWithFallback(unitsBundle.getAlias(), key.data(), &len, &status);
    return UnicodeString(ptr, len);
}

////////////////////////
/// END DATA LOADING ///
////////////////////////

} // namespace

void LongNameHandler::forMeasureUnit(const Locale &loc, const MeasureUnit &unitRef,
                                     const MeasureUnit &perUnit, const UNumberUnitWidth &width,
                                     const PluralRules *rules, const MicroPropsGenerator *parent,
                                     LongNameHandler *fillIn, UErrorCode &status) {
    // Not valid for mixed units that aren't built-in units, and there should
    // not be any built-in mixed units!
    U_ASSERT(uprv_strcmp(unitRef.getType(), "") != 0 ||
             unitRef.getComplexity(status) != UMEASURE_UNIT_MIXED);
    U_ASSERT(fillIn != nullptr);

    MeasureUnit unit = unitRef;
    if (uprv_strcmp(perUnit.getType(), "none") != 0) {
        // Compound unit: first try to simplify (e.g., meters per second is its own unit).
        MeasureUnit simplified = unit.product(perUnit.reciprocal(status), status);
        if (uprv_strcmp(simplified.getType(), "") != 0) {
            unit = simplified;
        } else {
            // No simplified form is available.
            forCompoundUnit(loc, unit, perUnit, width, rules, parent, fillIn, status);
            return;
        }
    }

    if (uprv_strcmp(unit.getType(), "") == 0) {
        // TODO(ICU-20941): Unsanctioned unit. Not yet fully supported. Set an
        // error code. Once we support not-built-in units here, unitRef may be
        // anything, but if not built-in, perUnit has to be "none".
        status = U_UNSUPPORTED_ERROR;
        return;
    }

    UnicodeString simpleFormats[ARRAY_LENGTH];
    getMeasureData(loc, unit, width, simpleFormats, status);
    if (U_FAILURE(status)) {
        return;
    }
    fillIn->rules = rules;
    fillIn->parent = parent;
    fillIn->simpleFormatsToModifiers(simpleFormats, {UFIELD_CATEGORY_NUMBER, UNUM_MEASURE_UNIT_FIELD},
                                     status);
}

void LongNameHandler::forCompoundUnit(const Locale &loc, const MeasureUnit &unit,
                                      const MeasureUnit &perUnit, const UNumberUnitWidth &width,
                                      const PluralRules *rules, const MicroPropsGenerator *parent,
                                      LongNameHandler *fillIn, UErrorCode &status) {
    if (uprv_strcmp(unit.getType(), "") == 0 || uprv_strcmp(perUnit.getType(), "") == 0) {
        // TODO(ICU-20941): Unsanctioned unit. Not yet fully supported. Set an
        // error code. Once we support not-built-in units here, unitRef may be
        // anything, but if not built-in, perUnit has to be "none".
        status = U_UNSUPPORTED_ERROR;
        return;
    }
    if (fillIn == nullptr) {
        status = U_INTERNAL_PROGRAM_ERROR;
        return;
    }
    UnicodeString primaryData[ARRAY_LENGTH];
    getMeasureData(loc, unit, width, primaryData, status);
    if (U_FAILURE(status)) {
        return;
    }
    UnicodeString secondaryData[ARRAY_LENGTH];
    getMeasureData(loc, perUnit, width, secondaryData, status);
    if (U_FAILURE(status)) {
        return;
    }

    UnicodeString perUnitFormat;
    if (!secondaryData[PER_INDEX].isBogus()) {
        perUnitFormat = secondaryData[PER_INDEX];
    } else {
        UnicodeString rawPerUnitFormat = getPerUnitFormat(loc, width, status);
        if (U_FAILURE(status)) {
            return;
        }
        // rawPerUnitFormat is something like "{0}/{1}"; we need to substitute in the secondary unit.
        SimpleFormatter compiled(rawPerUnitFormat, 2, 2, status);
        if (U_FAILURE(status)) {
            return;
        }
        UnicodeString secondaryFormat = getWithPlural(secondaryData, StandardPlural::Form::ONE, status);
        if (U_FAILURE(status)) {
            return;
        }
        // Some "one" pattern may not contain "{0}". For example in "ar" or "ne" locale.
        SimpleFormatter secondaryCompiled(secondaryFormat, 0, 1, status);
        if (U_FAILURE(status)) {
            return;
        }
        UnicodeString secondaryString = secondaryCompiled.getTextWithNoArguments().trim();
        // TODO: Why does UnicodeString need to be explicit in the following line?
        compiled.format(UnicodeString(u"{0}"), secondaryString, perUnitFormat, status);
        if (U_FAILURE(status)) {
            return;
        }
    }
    fillIn->rules = rules;
    fillIn->parent = parent;
    fillIn->multiSimpleFormatsToModifiers(primaryData, perUnitFormat,
                                          {UFIELD_CATEGORY_NUMBER, UNUM_MEASURE_UNIT_FIELD}, status);
}

UnicodeString LongNameHandler::getUnitDisplayName(
        const Locale& loc,
        const MeasureUnit& unit,
        UNumberUnitWidth width,
        UErrorCode& status) {
    if (U_FAILURE(status)) {
        return ICU_Utility::makeBogusString();
    }
    UnicodeString simpleFormats[ARRAY_LENGTH];
    getMeasureData(loc, unit, width, simpleFormats, status);
    return simpleFormats[DNAM_INDEX];
}

UnicodeString LongNameHandler::getUnitPattern(
        const Locale& loc,
        const MeasureUnit& unit,
        UNumberUnitWidth width,
        StandardPlural::Form pluralForm,
        UErrorCode& status) {
    if (U_FAILURE(status)) {
        return ICU_Utility::makeBogusString();
    }
    UnicodeString simpleFormats[ARRAY_LENGTH];
    getMeasureData(loc, unit, width, simpleFormats, status);
    // The above already handles fallback from other widths to short
    if (U_FAILURE(status)) {
        return ICU_Utility::makeBogusString();
    }
    // Now handle fallback from other plural forms to OTHER
    return (!(simpleFormats[pluralForm]).isBogus())? simpleFormats[pluralForm]:
            simpleFormats[StandardPlural::Form::OTHER];
}

LongNameHandler* LongNameHandler::forCurrencyLongNames(const Locale &loc, const CurrencyUnit &currency,
                                                      const PluralRules *rules,
                                                      const MicroPropsGenerator *parent,
                                                      UErrorCode &status) {
    auto* result = new LongNameHandler(rules, parent);
    if (result == nullptr) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return nullptr;
    }
    UnicodeString simpleFormats[ARRAY_LENGTH];
    getCurrencyLongNameData(loc, currency, simpleFormats, status);
    if (U_FAILURE(status)) { return nullptr; }
    result->simpleFormatsToModifiers(simpleFormats, {UFIELD_CATEGORY_NUMBER, UNUM_CURRENCY_FIELD}, status);
    return result;
}

void LongNameHandler::simpleFormatsToModifiers(const UnicodeString *simpleFormats, Field field,
                                               UErrorCode &status) {
    for (int32_t i = 0; i < StandardPlural::Form::COUNT; i++) {
        StandardPlural::Form plural = static_cast<StandardPlural::Form>(i);
        UnicodeString simpleFormat = getWithPlural(simpleFormats, plural, status);
        if (U_FAILURE(status)) { return; }
        SimpleFormatter compiledFormatter(simpleFormat, 0, 1, status);
        if (U_FAILURE(status)) { return; }
        fModifiers[i] = SimpleModifier(compiledFormatter, field, false, {this, SIGNUM_POS_ZERO, plural});
    }
}

void LongNameHandler::multiSimpleFormatsToModifiers(const UnicodeString *leadFormats, UnicodeString trailFormat,
                                                    Field field, UErrorCode &status) {
    SimpleFormatter trailCompiled(trailFormat, 1, 1, status);
    if (U_FAILURE(status)) { return; }
    for (int32_t i = 0; i < StandardPlural::Form::COUNT; i++) {
        StandardPlural::Form plural = static_cast<StandardPlural::Form>(i);
        UnicodeString leadFormat = getWithPlural(leadFormats, plural, status);
        if (U_FAILURE(status)) { return; }
        UnicodeString compoundFormat;
        trailCompiled.format(leadFormat, compoundFormat, status);
        if (U_FAILURE(status)) { return; }
        SimpleFormatter compoundCompiled(compoundFormat, 0, 1, status);
        if (U_FAILURE(status)) { return; }
        fModifiers[i] = SimpleModifier(compoundCompiled, field, false, {this, SIGNUM_POS_ZERO, plural});
    }
}

void LongNameHandler::processQuantity(DecimalQuantity &quantity, MicroProps &micros,
                                      UErrorCode &status) const {
    if (parent != NULL) {
        parent->processQuantity(quantity, micros, status);
    }
    StandardPlural::Form pluralForm = utils::getPluralSafe(micros.rounder, rules, quantity, status);
    micros.modOuter = &fModifiers[pluralForm];
}

const Modifier* LongNameHandler::getModifier(Signum /*signum*/, StandardPlural::Form plural) const {
    return &fModifiers[plural];
}

void MixedUnitLongNameHandler::forMeasureUnit(const Locale &loc, const MeasureUnit &mixedUnit,
                                              const UNumberUnitWidth &width, const PluralRules *rules,
                                              const MicroPropsGenerator *parent,
                                              MixedUnitLongNameHandler *fillIn, UErrorCode &status) {
    U_ASSERT(mixedUnit.getComplexity(status) == UMEASURE_UNIT_MIXED);
    U_ASSERT(fillIn != nullptr);

    MeasureUnitImpl temp;
    const MeasureUnitImpl& impl = MeasureUnitImpl::forMeasureUnit(mixedUnit, temp, status);
    fillIn->fMixedUnitCount = impl.units.length();
    fillIn->fMixedUnitData.adoptInstead(new UnicodeString[fillIn->fMixedUnitCount * ARRAY_LENGTH]);
    for (int32_t i = 0; i < fillIn->fMixedUnitCount; i++) {
        // Grab data for each of the components.
        UnicodeString *unitData = &fillIn->fMixedUnitData[i * ARRAY_LENGTH];
        getMeasureData(loc, impl.units[i]->build(status), width, unitData, status);
    }

    UListFormatterWidth listWidth = ULISTFMT_WIDTH_SHORT;
    if (width == UNUM_UNIT_WIDTH_NARROW) {
        listWidth = ULISTFMT_WIDTH_NARROW;
    } else if (width == UNUM_UNIT_WIDTH_FULL_NAME) {
        // This might be the same as SHORT in most languages:
        listWidth = ULISTFMT_WIDTH_WIDE;
    }
    fillIn->fListFormatter.adoptInsteadAndCheckErrorCode(
        ListFormatter::createInstance(loc, ULISTFMT_TYPE_UNITS, listWidth, status), status);
    fillIn->rules = rules;
    fillIn->parent = parent;

    // We need a localised NumberFormatter for the integers of the bigger units
    // (providing Arabic numerals, for example).
    fillIn->fIntegerFormatter = NumberFormatter::withLocale(loc);
}

void MixedUnitLongNameHandler::processQuantity(DecimalQuantity &quantity, MicroProps &micros,
                                               UErrorCode &status) const {
    U_ASSERT(fMixedUnitCount > 1);
    if (parent != nullptr) {
        parent->processQuantity(quantity, micros, status);
    }
    micros.modOuter = getMixedUnitModifier(quantity, micros, status);
}

const Modifier *MixedUnitLongNameHandler::getMixedUnitModifier(DecimalQuantity &quantity,
                                                               MicroProps &micros,
                                                               UErrorCode &status) const {
    if (micros.mixedMeasuresCount == 0) {
        U_ASSERT(micros.mixedMeasuresCount > 0); // Mixed unit: we must have more than one unit value
        status = U_UNSUPPORTED_ERROR;
        return &micros.helpers.emptyWeakModifier;
    }
    // If we don't have at least one mixedMeasure, the LongNameHandler would be
    // sufficient and we shouldn't be running MixedUnitLongNameHandler code:
    U_ASSERT(micros.mixedMeasuresCount > 0);
    // mixedMeasures does not contain the last value:
    U_ASSERT(fMixedUnitCount == micros.mixedMeasuresCount + 1);
    U_ASSERT(fListFormatter.isValid());

    // Algorithm:
    //
    // For the mixed-units measurement of: "3 yard, 1 foot, 2.6 inch", we should
    // find "3 yard" and "1 foot" in micros.mixedMeasures.
    //
    // Obtain long-names with plural forms corresponding to measure values:
    //   * {0} yards, {0} foot, {0} inches
    //
    // Format the integer values appropriately and modify with the format
    // strings:
    //   - 3 yards, 1 foot
    //
    // Use ListFormatter to combine, with one placeholder:
    //   - 3 yards, 1 foot and {0} inches
    //
    // Return a SimpleModifier for this pattern, letting the rest of the
    // pipeline take care of the remaining inches.

    LocalArray<UnicodeString> outputMeasuresList(new UnicodeString[fMixedUnitCount], status);
    if (U_FAILURE(status)) {
        return &micros.helpers.emptyWeakModifier;
    }

    for (int32_t i = 0; i < micros.mixedMeasuresCount; i++) {
        DecimalQuantity fdec;
        fdec.setToLong(micros.mixedMeasures[i]);
        if (i > 0 && fdec.isNegative()) {
            // If numbers are negative, only the first number needs to have its
            // negative sign formatted.
            fdec.negate();
        }
        StandardPlural::Form pluralForm = utils::getStandardPlural(rules, fdec);

        UnicodeString simpleFormat =
            getWithPlural(&fMixedUnitData[i * ARRAY_LENGTH], pluralForm, status);
        SimpleFormatter compiledFormatter(simpleFormat, 0, 1, status);

        UnicodeString num;
        auto appendable = UnicodeStringAppendable(num);
        fIntegerFormatter.formatDecimalQuantity(fdec, status).appendTo(appendable, status);
        compiledFormatter.format(num, outputMeasuresList[i], status);
        // TODO(icu-units#67): fix field positions
    }

    // Reiterated: we have at least one mixedMeasure:
    U_ASSERT(micros.mixedMeasuresCount > 0);
    // Thus if negative, a negative has already been formatted:
    if (quantity.isNegative()) {
        quantity.negate();
    }

    UnicodeString *finalSimpleFormats = &fMixedUnitData[(fMixedUnitCount - 1) * ARRAY_LENGTH];
    StandardPlural::Form finalPlural = utils::getPluralSafe(micros.rounder, rules, quantity, status);
    UnicodeString finalSimpleFormat = getWithPlural(finalSimpleFormats, finalPlural, status);
    SimpleFormatter finalFormatter(finalSimpleFormat, 0, 1, status);
    finalFormatter.format(UnicodeString(u"{0}"), outputMeasuresList[fMixedUnitCount - 1], status);

    // Combine list into a "premixed" pattern
    UnicodeString premixedFormatPattern;
    fListFormatter->format(outputMeasuresList.getAlias(), fMixedUnitCount, premixedFormatPattern,
                           status);
    SimpleFormatter premixedCompiled(premixedFormatPattern, 0, 1, status);
    if (U_FAILURE(status)) {
        return &micros.helpers.emptyWeakModifier;
    }

    // TODO(icu-units#67): fix field positions
    // Return a SimpleModifier for the "premixed" pattern
    micros.helpers.mixedUnitModifier =
        SimpleModifier(premixedCompiled, kUndefinedField, false, {this, SIGNUM_POS_ZERO, finalPlural});
    return &micros.helpers.mixedUnitModifier;
}

const Modifier *MixedUnitLongNameHandler::getModifier(Signum /*signum*/,
                                                      StandardPlural::Form /*plural*/) const {
    // TODO(units): investigate this method when investigating where
    // LongNameHandler::getModifier() gets used. To be sure it remains
    // unreachable:
    UPRV_UNREACHABLE;
    return nullptr;
}

LongNameMultiplexer *
LongNameMultiplexer::forMeasureUnits(const Locale &loc, const MaybeStackVector<MeasureUnit> &units,
                                     const UNumberUnitWidth &width, const PluralRules *rules,
                                     const MicroPropsGenerator *parent, UErrorCode &status) {
    LocalPointer<LongNameMultiplexer> result(new LongNameMultiplexer(parent), status);
    if (U_FAILURE(status)) {
        return nullptr;
    }
    U_ASSERT(units.length() > 0);
    if (result->fHandlers.resize(units.length()) == nullptr) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return nullptr;
    }
    result->fMeasureUnits.adoptInstead(new MeasureUnit[units.length()]);
    for (int32_t i = 0, length = units.length(); i < length; i++) {
        const MeasureUnit& unit = *units[i];
        result->fMeasureUnits[i] = unit;
        if (unit.getComplexity(status) == UMEASURE_UNIT_MIXED) {
            MixedUnitLongNameHandler *mlnh = result->fMixedUnitHandlers.createAndCheckErrorCode(status);
            MixedUnitLongNameHandler::forMeasureUnit(loc, unit, width, rules, NULL, mlnh, status);
            result->fHandlers[i] = mlnh;
        } else {
            LongNameHandler *lnh = result->fLongNameHandlers.createAndCheckErrorCode(status);
            LongNameHandler::forMeasureUnit(loc, unit, MeasureUnit(), width, rules, NULL, lnh, status);
            result->fHandlers[i] = lnh;
        }
        if (U_FAILURE(status)) {
            return nullptr;
        }
    }
    return result.orphan();
}

void LongNameMultiplexer::processQuantity(DecimalQuantity &quantity, MicroProps &micros,
                                          UErrorCode &status) const {
    // We call parent->processQuantity() from the Multiplexer, instead of
    // letting LongNameHandler handle it: we don't know which LongNameHandler to
    // call until we've called the parent!
    fParent->processQuantity(quantity, micros, status);

    // Call the correct LongNameHandler based on outputUnit
    for (int i = 0; i < fHandlers.getCapacity(); i++) {
        if (fMeasureUnits[i] == micros.outputUnit) {
            fHandlers[i]->processQuantity(quantity, micros, status);
            return;
        }
    }
    if (U_FAILURE(status)) {
        return;
    }
    // We shouldn't receive any outputUnit for which we haven't already got a
    // LongNameHandler:
    status = U_INTERNAL_PROGRAM_ERROR;
}

#endif /* #if !UCONFIG_NO_FORMATTING */
