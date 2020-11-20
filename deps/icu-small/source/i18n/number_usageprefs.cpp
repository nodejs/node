// © 2020 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "number_usageprefs.h"
#include "cstring.h"
#include "number_decimalquantity.h"
#include "number_microprops.h"
#include "number_roundingutils.h"
#include "number_skeletons.h"
#include "unicode/char16ptr.h"
#include "unicode/currunit.h"
#include "unicode/fmtable.h"
#include "unicode/measure.h"
#include "unicode/numberformatter.h"
#include "unicode/platform.h"
#include "unicode/unum.h"
#include "unicode/urename.h"
#include "units_data.h"

using namespace icu;
using namespace icu::number;
using namespace icu::number::impl;
using icu::StringSegment;
using icu::units::ConversionRates;

// Copy constructor
Usage::Usage(const Usage &other) : Usage() {
    this->operator=(other);
}

// Copy assignment operator
Usage &Usage::operator=(const Usage &other) {
    fLength = 0;
    fError = other.fError;
    if (fUsage != nullptr) {
        uprv_free(fUsage);
        fUsage = nullptr;
    }
    if (other.fUsage == nullptr) {
        return *this;
    }
    if (U_FAILURE(other.fError)) {
        // We don't bother trying to allocating memory if we're in any case busy
        // copying an errored Usage.
        return *this;
    }
    fUsage = (char *)uprv_malloc(other.fLength + 1);
    if (fUsage == nullptr) {
        fError = U_MEMORY_ALLOCATION_ERROR;
        return *this;
    }
    fLength = other.fLength;
    uprv_strncpy(fUsage, other.fUsage, fLength + 1);
    return *this;
}

// Move constructor
Usage::Usage(Usage &&src) U_NOEXCEPT : fUsage(src.fUsage), fLength(src.fLength), fError(src.fError) {
    // Take ownership away from src if necessary
    src.fUsage = nullptr;
}

// Move assignment operator
Usage &Usage::operator=(Usage &&src) U_NOEXCEPT {
    if (this == &src) {
        return *this;
    }
    if (fUsage != nullptr) {
        uprv_free(fUsage);
    }
    fUsage = src.fUsage;
    fLength = src.fLength;
    fError = src.fError;
    // Take ownership away from src if necessary
    src.fUsage = nullptr;
    return *this;
}

Usage::~Usage() {
    if (fUsage != nullptr) {
        uprv_free(fUsage);
        fUsage = nullptr;
    }
}

void Usage::set(StringPiece value) {
    if (fUsage != nullptr) {
        uprv_free(fUsage);
        fUsage = nullptr;
    }
    fLength = value.length();
    fUsage = (char *)uprv_malloc(fLength + 1);
    if (fUsage == nullptr) {
        fLength = 0;
        fError = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    uprv_strncpy(fUsage, value.data(), fLength);
    fUsage[fLength] = 0;
}

// Populates micros.mixedMeasures and modifies quantity, based on the values in
// measures.
void mixedMeasuresToMicros(const MaybeStackVector<Measure> &measures, DecimalQuantity *quantity,
                           MicroProps *micros, UErrorCode status) {
    micros->mixedMeasuresCount = measures.length() - 1;
    if (micros->mixedMeasuresCount > 0) {
#ifdef U_DEBUG
        U_ASSERT(micros->outputUnit.getComplexity(status) == UMEASURE_UNIT_MIXED);
        U_ASSERT(U_SUCCESS(status));
        // Check that we received measurements with the expected MeasureUnits:
        MeasureUnitImpl temp;
        const MeasureUnitImpl& impl = MeasureUnitImpl::forMeasureUnit(micros->outputUnit, temp, status);
        U_ASSERT(U_SUCCESS(status));
        U_ASSERT(measures.length() == impl.units.length());
        for (int32_t i = 0; i < measures.length(); i++) {
            U_ASSERT(measures[i]->getUnit() == impl.units[i]->build(status));
        }
        (void)impl;
#endif
        // Mixed units: except for the last value, we pass all values to the
        // LongNameHandler via micros->mixedMeasures.
        if (micros->mixedMeasures.getCapacity() < micros->mixedMeasuresCount) {
            if (micros->mixedMeasures.resize(micros->mixedMeasuresCount) == nullptr) {
                status = U_MEMORY_ALLOCATION_ERROR;
                return;
            }
        }
        for (int32_t i = 0; i < micros->mixedMeasuresCount; i++) {
            micros->mixedMeasures[i] = measures[i]->getNumber().getInt64();
        }
    } else {
        micros->mixedMeasuresCount = 0;
    }
    // The last value (potentially the only value) gets passed on via quantity.
    quantity->setToDouble(measures[measures.length() - 1]->getNumber().getDouble());
}

UsagePrefsHandler::UsagePrefsHandler(const Locale &locale,
                                     const MeasureUnit &inputUnit,
                                     const StringPiece usage,
                                     const MicroPropsGenerator *parent,
                                     UErrorCode &status)
    : fUnitsRouter(inputUnit, StringPiece(locale.getCountry()), usage, status),
      fParent(parent) {
}

void UsagePrefsHandler::processQuantity(DecimalQuantity &quantity, MicroProps &micros,
                                        UErrorCode &status) const {
    fParent->processQuantity(quantity, micros, status);
    if (U_FAILURE(status)) {
        return;
    }

    quantity.roundToInfinity(); // Enables toDouble
    const units::RouteResult routed = fUnitsRouter.route(quantity.toDouble(), &micros.rounder, status);
    if (U_FAILURE(status)) {
        return;
    }
    const MaybeStackVector<Measure>& routedMeasures = routed.measures;
    micros.outputUnit = routed.outputUnit.copy(status).build(status);
    if (U_FAILURE(status)) {
        return;
    }

    mixedMeasuresToMicros(routedMeasures, &quantity, &micros, status);
}

UnitConversionHandler::UnitConversionHandler(const MeasureUnit &inputUnit, const MeasureUnit &outputUnit,
                                             const MicroPropsGenerator *parent, UErrorCode &status)
    : fOutputUnit(outputUnit), fParent(parent) {
    MeasureUnitImpl tempInput, tempOutput;
    const MeasureUnitImpl &inputUnitImpl = MeasureUnitImpl::forMeasureUnit(inputUnit, tempInput, status);
    const MeasureUnitImpl &outputUnitImpl =
        MeasureUnitImpl::forMeasureUnit(outputUnit, tempOutput, status);

    // TODO: this should become an initOnce thing? Review with other
    // ConversionRates usages.
    ConversionRates conversionRates(status);
    if (U_FAILURE(status)) {
        return;
    }
    fUnitConverter.adoptInsteadAndCheckErrorCode(
        new ComplexUnitsConverter(inputUnitImpl, outputUnitImpl, conversionRates, status), status);
}

void UnitConversionHandler::processQuantity(DecimalQuantity &quantity, MicroProps &micros,
                                            UErrorCode &status) const {
    fParent->processQuantity(quantity, micros, status);
    if (U_FAILURE(status)) {
        return;
    }
    quantity.roundToInfinity(); // Enables toDouble
    MaybeStackVector<Measure> measures =
        fUnitConverter->convert(quantity.toDouble(), &micros.rounder, status);
    micros.outputUnit = fOutputUnit;
    if (U_FAILURE(status)) {
        return;
    }

    mixedMeasuresToMicros(measures, &quantity, &micros, status);
}

#endif /* #if !UCONFIG_NO_FORMATTING */
