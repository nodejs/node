// © 2020 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include <cmath>

#include "cmemory.h"
#include "number_decimalquantity.h"
#include "number_roundingutils.h"
#include "uarrsort.h"
#include "uassert.h"
#include "unicode/fmtable.h"
#include "unicode/localpointer.h"
#include "unicode/measunit.h"
#include "unicode/measure.h"
#include "units_complexconverter.h"
#include "units_converter.h"

U_NAMESPACE_BEGIN
namespace units {

ComplexUnitsConverter::ComplexUnitsConverter(const MeasureUnitImpl &inputUnit,
                                             const MeasureUnitImpl &outputUnits,
                                             const ConversionRates &ratesInfo, UErrorCode &status)
    : units_(outputUnits.extractIndividualUnits(status)) {
    if (U_FAILURE(status)) {
        return;
    }

    U_ASSERT(units_.length() != 0);

    // Save the desired order of output units before we sort units_
    for (int32_t i = 0; i < units_.length(); i++) {
        outputUnits_.emplaceBackAndCheckErrorCode(status, units_[i]->copy(status).build(status));
    }

    // NOTE:
    //  This comparator is used to sort the units in a descending order. Therefore, we return -1 if
    //  the left is bigger than right and so on.
    auto descendingCompareUnits = [](const void *context, const void *left, const void *right) {
        UErrorCode status = U_ZERO_ERROR;

        const auto *leftPointer = static_cast<const MeasureUnitImpl *const *>(left);
        const auto *rightPointer = static_cast<const MeasureUnitImpl *const *>(right);

        UnitConverter fromLeftToRight(**leftPointer,                                  //
                                      **rightPointer,                                 //
                                      *static_cast<const ConversionRates *>(context), //
                                      status);

        double rightFromOneLeft = fromLeftToRight.convert(1.0);
        if (std::abs(rightFromOneLeft - 1.0) < 0.0000000001) { // Equals To
            return 0;
        } else if (rightFromOneLeft > 1.0) { // Greater Than
            return -1;
        }

        return 1; // Less Than
    };

    uprv_sortArray(units_.getAlias(),                                                                  //
                   units_.length(),                                                                    //
                   sizeof units_[0], /* NOTE: we have already asserted that the units_ is not empty.*/ //
                   descendingCompareUnits,                                                             //
                   &ratesInfo,                                                                         //
                   false,                                                                              //
                   &status                                                                             //
    );

    // In case the `outputUnits` are `UMEASURE_UNIT_MIXED` such as `foot+inch`. In this case we need more
    // converters to convert from the `inputUnit` to the first unit in the `outputUnits`. Then, a
    // converter from the first unit in the `outputUnits` to the second unit and so on.
    //      For Example:
    //          - inputUnit is `meter`
    //          - outputUnits is `foot+inch`
    //              - Therefore, we need to have two converters:
    //                      1. a converter from `meter` to `foot`
    //                      2. a converter from `foot` to `inch`
    //          - Therefore, if the input is `2 meter`:
    //              1. convert `meter` to `foot` --> 2 meter to 6.56168 feet
    //              2. convert the residual of 6.56168 feet (0.56168) to inches, which will be (6.74016
    //              inches)
    //              3. then, the final result will be (6 feet and 6.74016 inches)
    for (int i = 0, n = units_.length(); i < n; i++) {
        if (i == 0) { // first element
            unitConverters_.emplaceBackAndCheckErrorCode(status, inputUnit, *units_[i], ratesInfo,
                                                         status);
        } else {
            unitConverters_.emplaceBackAndCheckErrorCode(status, *units_[i - 1], *units_[i], ratesInfo,
                                                         status);
        }

        if (U_FAILURE(status)) {
            return;
        }
    }
}

UBool ComplexUnitsConverter::greaterThanOrEqual(double quantity, double limit) const {
    U_ASSERT(unitConverters_.length() > 0);

    // First converter converts to the biggest quantity.
    double newQuantity = unitConverters_[0]->convert(quantity);
    return newQuantity >= limit;
}

MaybeStackVector<Measure> ComplexUnitsConverter::convert(double quantity,
                                                         icu::number::impl::RoundingImpl *rounder,
                                                         UErrorCode &status) const {
    // TODO(hugovdm): return an error for "foot-and-foot"?
    MaybeStackVector<Measure> result;
    int sign = 1;
    if (quantity < 0) {
        quantity *= -1;
        sign = -1;
    }

    // For N converters:
    // - the first converter converts from the input unit to the largest unit,
    // - N-1 converters convert to bigger units for which we want integers,
    // - the Nth converter (index N-1) converts to the smallest unit, for which
    //   we keep a double.
    MaybeStackArray<int64_t, 5> intValues(unitConverters_.length() - 1, status);
    if (U_FAILURE(status)) {
        return result;
    }
    uprv_memset(intValues.getAlias(), 0, (unitConverters_.length() - 1) * sizeof(int64_t));

    for (int i = 0, n = unitConverters_.length(); i < n; ++i) {
        quantity = (*unitConverters_[i]).convert(quantity);
        if (i < n - 1) {
            // The double type has 15 decimal digits of precision. For choosing
            // whether to use the current unit or the next smaller unit, we
            // therefore nudge up the number with which the thresholding
            // decision is made. However after the thresholding, we use the
            // original values to ensure unbiased accuracy (to the extent of
            // double's capabilities).
            int64_t roundedQuantity = floor(quantity * (1 + DBL_EPSILON));
            intValues[i] = roundedQuantity;

            // Keep the residual of the quantity.
            //   For example: `3.6 feet`, keep only `0.6 feet`
            //
            // When the calculation is near enough +/- DBL_EPSILON, we round to
            // zero. (We also ensure no negative values here.)
            if ((quantity - roundedQuantity) / quantity < DBL_EPSILON) {
                quantity = 0;
            } else {
                quantity -= roundedQuantity;
            }
        } else { // LAST ELEMENT
            if (rounder == nullptr) {
                // Nothing to do for the last element.
                break;
            }

            // Round the last value
            // TODO(ICU-21288): get smarter about precision for mixed units.
            number::impl::DecimalQuantity quant;
            quant.setToDouble(quantity);
            rounder->apply(quant, status);
            if (U_FAILURE(status)) {
                return result;
            }
            quantity = quant.toDouble();
            if (i == 0) {
                // Last element is also the first element, so we're done
                break;
            }

            // Check if there's a carry, and bubble it back up the resulting intValues.
            int64_t carry = floor(unitConverters_[i]->convertInverse(quantity) * (1 + DBL_EPSILON));
            if (carry <= 0) {
                break;
            }
            quantity -= unitConverters_[i]->convert(carry);
            intValues[i - 1] += carry;

            // We don't use the first converter: that one is for the input unit
            for (int32_t j = i - 1; j > 0; j--) {
                carry = floor(unitConverters_[j]->convertInverse(intValues[j]) * (1 + DBL_EPSILON));
                if (carry <= 0) {
                    break;
                }
                intValues[j] -= round(unitConverters_[j]->convert(carry));
                intValues[j - 1] += carry;
            }
        }
    }

    // Package values into Measure instances in result:
    for (int i = 0, n = unitConverters_.length(); i < n; ++i) {
        if (i < n - 1) {
            Formattable formattableQuantity(intValues[i] * sign);
            // Measure takes ownership of the MeasureUnit*
            MeasureUnit *type = new MeasureUnit(units_[i]->copy(status).build(status));
            if (result.emplaceBackAndCheckErrorCode(status, formattableQuantity, type, status) ==
                nullptr) {
                // Ownership wasn't taken
                U_ASSERT(U_FAILURE(status));
                delete type;
            }
            if (U_FAILURE(status)) {
                return result;
            }
        } else { // LAST ELEMENT
            // Add the last element, not an integer:
            Formattable formattableQuantity(quantity * sign);
            // Measure takes ownership of the MeasureUnit*
            MeasureUnit *type = new MeasureUnit(units_[i]->copy(status).build(status));
            if (result.emplaceBackAndCheckErrorCode(status, formattableQuantity, type, status) ==
                nullptr) {
                // Ownership wasn't taken
                U_ASSERT(U_FAILURE(status));
                delete type;
            }
            if (U_FAILURE(status)) {
                return result;
            }
            U_ASSERT(result.length() == i + 1);
            U_ASSERT(result[i] != nullptr);
        }
    }

    MaybeStackVector<Measure> orderedResult;
    int32_t unitsCount = outputUnits_.length();
    U_ASSERT(unitsCount == units_.length());
    Measure **arr = result.getAlias();
    // O(N^2) is fine: mixed units' unitsCount is usually 2 or 3.
    for (int32_t i = 0; i < unitsCount; i++) {
        for (int32_t j = i; j < unitsCount; j++) {
            // Find the next expected unit, and swap it into place.
            U_ASSERT(result[j] != nullptr);
            if (result[j]->getUnit() == *outputUnits_[i]) {
                if (j != i) {
                    Measure *tmp = arr[j];
                    arr[j] = arr[i];
                    arr[i] = tmp;
                }
            }
        }
    }

    return result;
}

} // namespace units
U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */
