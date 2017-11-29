// © 2017 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING && !UPRV_INCOMPLETE_CPP11_SUPPORT

#include "unicode/ures.h"
#include "ureslocs.h"
#include "charstr.h"
#include "uresimp.h"
#include "number_longnames.h"
#include <algorithm>
#include "cstring.h"

using namespace icu;
using namespace icu::number;
using namespace icu::number::impl;

namespace {


//////////////////////////
/// BEGIN DATA LOADING ///
//////////////////////////

class PluralTableSink : public ResourceSink {
  public:
    explicit PluralTableSink(UnicodeString *outArray) : outArray(outArray) {
        // Initialize the array to bogus strings.
        for (int32_t i = 0; i < StandardPlural::Form::COUNT; i++) {
            outArray[i].setToBogus();
        }
    }

    void put(const char *key, ResourceValue &value, UBool /*noFallback*/, UErrorCode &status) U_OVERRIDE {
        ResourceTable pluralsTable = value.getTable(status);
        if (U_FAILURE(status)) { return; }
        for (int i = 0; pluralsTable.getKeyAndValue(i, key, value); ++i) {
            // In MeasureUnit data, ignore dnam and per units for now.
            if (uprv_strcmp(key, "dnam") == 0 || uprv_strcmp(key, "per") == 0) {
                continue;
            }
            StandardPlural::Form plural = StandardPlural::fromString(key, status);
            if (U_FAILURE(status)) { return; }
            if (!outArray[plural].isBogus()) {
                continue;
            }
            outArray[plural] = value.getUnicodeString(status);
            if (U_FAILURE(status)) { return; }
        }
    }

  private:
    UnicodeString *outArray;
};

// NOTE: outArray MUST have room for all StandardPlural values.  No bounds checking is performed.

void getMeasureData(const Locale &locale, const MeasureUnit &unit, const UNumberUnitWidth &width,
                    UnicodeString *outArray, UErrorCode &status) {
    PluralTableSink sink(outArray);
    LocalUResourceBundlePointer unitsBundle(ures_open(U_ICUDATA_UNIT, locale.getName(), &status));
    if (U_FAILURE(status)) { return; }
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
    key.append(unit.getSubtype(), status);
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
        UBool isChoiceFormat = FALSE;
        int32_t longNameLen = 0;
        const char16_t *longName = ucurr_getPluralName(
                currency.getISOCurrency(),
                locale.getName(),
                &isChoiceFormat,
                StandardPlural::getKeyword(static_cast<StandardPlural::Form>(i)),
                &longNameLen,
                &status);
        // Example pattern from data: "{0} {1}"
        // Example output after find-and-replace: "{0} US dollars"
        pattern.findAndReplace(UnicodeString(u"{1}"), UnicodeString(longName, longNameLen));
    }
}

////////////////////////
/// END DATA LOADING ///
////////////////////////

} // namespace

LongNameHandler
LongNameHandler::forMeasureUnit(const Locale &loc, const MeasureUnit &unit, const UNumberUnitWidth &width,
                                const PluralRules *rules, const MicroPropsGenerator *parent,
                                UErrorCode &status) {
    LongNameHandler result(rules, parent);
    UnicodeString simpleFormats[StandardPlural::Form::COUNT];
    getMeasureData(loc, unit, width, simpleFormats, status);
    if (U_FAILURE(status)) { return result; }
    // TODO: What field to use for units?
    simpleFormatsToModifiers(simpleFormats, UNUM_FIELD_COUNT, result.fModifiers, status);
    return result;
}

LongNameHandler LongNameHandler::forCurrencyLongNames(const Locale &loc, const CurrencyUnit &currency,
                                                      const PluralRules *rules,
                                                      const MicroPropsGenerator *parent,
                                                      UErrorCode &status) {
    LongNameHandler result(rules, parent);
    UnicodeString simpleFormats[StandardPlural::Form::COUNT];
    getCurrencyLongNameData(loc, currency, simpleFormats, status);
    if (U_FAILURE(status)) { return result; }
    simpleFormatsToModifiers(simpleFormats, UNUM_CURRENCY_FIELD, result.fModifiers, status);
    return result;
}

void LongNameHandler::simpleFormatsToModifiers(const UnicodeString *simpleFormats, Field field,
                                               SimpleModifier *output, UErrorCode &status) {
    for (int32_t i = 0; i < StandardPlural::Form::COUNT; i++) {
        UnicodeString simpleFormat = simpleFormats[i];
        if (simpleFormat.isBogus()) {
            simpleFormat = simpleFormats[StandardPlural::Form::OTHER];
        }
        if (simpleFormat.isBogus()) {
            // There should always be data in the "other" plural variant.
            status = U_INTERNAL_PROGRAM_ERROR;
            return;
        }
        SimpleFormatter compiledFormatter(simpleFormat, 1, 1, status);
        output[i] = SimpleModifier(compiledFormatter, field, false);
    }
}

void LongNameHandler::processQuantity(DecimalQuantity &quantity, MicroProps &micros,
                                      UErrorCode &status) const {
    parent->processQuantity(quantity, micros, status);
    // TODO: Avoid the copy here?
    DecimalQuantity copy(quantity);
    micros.rounding.apply(copy, status);
    micros.modOuter = &fModifiers[copy.getStandardPlural(rules)];
}

#endif /* #if !UCONFIG_NO_FORMATTING */
