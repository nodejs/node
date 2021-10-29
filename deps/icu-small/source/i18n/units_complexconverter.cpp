// © 2020 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include <cmath>

#include "cmemory.h"
#include "number_decimalquantity.h"
#include "number_roundingutils.h"
#include "putilimp.h"
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
ComplexUnitsConverter::ComplexUnitsConverter(const MeasureUnitImpl &targetUnit,
                                             const ConversionRates &ratesInfo, UErrorCode &status)
    : units_(targetUnit.extractIndividualUnitsWithIndices(status)) {
    if (U_FAILURE(status)) {
        return;
    }
    U_ASSERT(units_.length() != 0);

    // Just borrowing a pointer to the instance
    MeasureUnitImpl *biggestUnit = &units_[0]->unitImpl;
    for (int32_t i = 1; i < units_.length(); i++) {
        if (UnitsConverter::compareTwoUnits(units_[i]->unitImpl, *biggestUnit, ratesInfo, status) > 0 &&
            U_SUCCESS(status)) {
            biggestUnit = &units_[i]->unitImpl;
        }

        if (U_FAILURE(status)) {
            return;
        }
    }

    this->init(*biggestUnit, ratesInfo, status);
}

ComplexUnitsConverter::ComplexUnitsConverter(StringPiece inputUnitIdentifier,
                                             StringPiece outputUnitsIdentifier, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
    MeasureUnitImpl inputUnit = MeasureUnitImpl::forIdentifier(inputUnitIdentifier, status);
    MeasureUnitImpl outputUnits = MeasureUnitImpl::forIdentifier(outputUnitsIdentifier, status);

    this->units_ = outputUnits.extractIndividualUnitsWithIndices(status);
    U_ASSERT(units_.length() != 0);

    this->init(inputUnit, ConversionRates(status), status);
}

ComplexUnitsConverter::ComplexUnitsConverter(const MeasureUnitImpl &inputUnit,
                                             const MeasureUnitImpl &outputUnits,
                                             const ConversionRates &ratesInfo, UErrorCode &status)
    : units_(outputUnits.extractIndividualUnitsWithIndices(status)) {
    if (U_FAILURE(status)) {
        return;
    }

    U_ASSERT(units_.length() != 0);

    this->init(inputUnit, ratesInfo, status);
}

void ComplexUnitsConverter::init(const MeasureUnitImpl &inputUnit,
                                 const ConversionRates &ratesInfo,
                                 UErrorCode &status) {
    // Sorts units in descending order. Therefore, we return -1 if
    // the left is bigger than right and so on.
    auto descendingCompareUnits = [](const void *context, const void *left, const void *right) {
        UErrorCode status = U_ZERO_ERROR;

        const auto *leftPointer = static_cast<const MeasureUnitImplWithIndex *const *>(left);
        const auto *rightPointer = static_cast<const MeasureUnitImplWithIndex *const *>(right);

        // Multiply by -1 to sort in descending order
        return (-1) * UnitsConverter::compareTwoUnits((**leftPointer).unitImpl,                       //
                                                      (**rightPointer).unitImpl,                      //
                                                      *static_cast<const ConversionRates *>(context), //
                                                      status);
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
            unitsConverters_.emplaceBackAndCheckErrorCode(status, inputUnit, units_[i]->unitImpl,
                                                          ratesInfo, status);
        } else {
            unitsConverters_.emplaceBackAndCheckErrorCode(status, units_[i - 1]->unitImpl,
                                                          units_[i]->unitImpl, ratesInfo, status);
        }

        if (U_FAILURE(status)) {
            return;
        }
    }
}

UBool ComplexUnitsConverter::greaterThanOrEqual(double quantity, double limit) const {
    U_ASSERT(unitsConverters_.length() > 0);

    // First converter converts to the biggest quantity.
    double newQuantity = unitsConverters_[0]->convert(quantity);
    return newQuantity >= limit;
}

MaybeStackVector<Measure> ComplexUnitsConverter::convert(double quantity,
                                                         icu::number::impl::RoundingImpl *rounder,
                                                         UErrorCode &status) const {
    // TODO: return an error for "foot-and-foot"?
    MaybeStackVector<Measure> result;
    int sign = 1;
    if (quantity < 0) {
        quantity *= -1;
        sign = -1;
    }

    // For N converters:
    // - the first converter converts from the input unit to the largest unit,
    // - the following N-2 converters convert to bigger units for which we want integers,
    // - the Nth converter (index N-1) converts to the smallest unit, for which
    //   we keep a double.
    MaybeStackArray<int64_t, 5> intValues(unitsConverters_.length() - 1, status);
    if (U_FAILURE(status)) {
        return result;
    }
    uprv_memset(intValues.getAlias(), 0, (unitsConverters_.length() - 1) * sizeof(int64_t));

    for (int i = 0, n = unitsConverters_.length(); i < n; ++i) {
        quantity = (*unitsConverters_[i]).convert(quantity);
        if (i < n - 1) {
            // If quantity is at the limits of double's precision from an
            // integer value, we take that integer value.
            int64_t flooredQuantity = static_cast<int64_t>(floor(quantity * (1 + DBL_EPSILON)));
            if (uprv_isNaN(quantity)) {
                // With clang on Linux: floor does not support NaN, resulting in
                // a giant negative number. For now, we produce "0 feet, NaN
                // inches". TODO(icu-units#131): revisit desired output.
                flooredQuantity = 0;
            }
            intValues[i] = flooredQuantity;

            // Keep the residual of the quantity.
            //   For example: `3.6 feet`, keep only `0.6 feet`
            double remainder = quantity - flooredQuantity;
            if (remainder < 0) {
                // Because we nudged flooredQuantity up by eps, remainder may be
                // negative: we must treat such a remainder as zero.
                quantity = 0;
            } else {
                quantity = remainder;
            }
        }   
    }

    applyRounder(intValues, quantity, rounder, status);

    // Initialize empty result. We use a MaybeStackArray directly so we can
    // assign pointers - for this privilege we have to take care of cleanup.
    MaybeStackArray<Measure *, 4> tmpResult(unitsConverters_.length(), status);
    if (U_FAILURE(status)) {
        return result;
    }

    // Package values into temporary Measure instances in tmpResult:
    for (int i = 0, n = unitsConverters_.length(); i < n; ++i) {
        if (i < n - 1) {
            Formattable formattableQuantity(intValues[i] * sign);
            // Measure takes ownership of the MeasureUnit*
            MeasureUnit *type = new MeasureUnit(units_[i]->unitImpl.copy(status).build(status));
            tmpResult[units_[i]->index] = new Measure(formattableQuantity, type, status);
        } else { // LAST ELEMENT
            Formattable formattableQuantity(quantity * sign);
            // Measure takes ownership of the MeasureUnit*
            MeasureUnit *type = new MeasureUnit(units_[i]->unitImpl.copy(status).build(status));
            tmpResult[units_[i]->index] = new Measure(formattableQuantity, type, status);
        }
    }


    // Transfer values into result and return:
    for(int32_t i = 0, n = unitsConverters_.length(); i < n; ++i) {
        U_ASSERT(tmpResult[i] != nullptr);
        result.emplaceBackAndCheckErrorCode(status, *tmpResult[i]);
        delete tmpResult[i];
    }

    return result;
}

void ComplexUnitsConverter::applyRounder(MaybeStackArray<int64_t, 5> &intValues, double &quantity,
                                         icu::number::impl::RoundingImpl *rounder,
                                         UErrorCode &status) const {
    if (rounder == nullptr) {
        // Nothing to do for the quantity.
        return;
    }

    number::impl::DecimalQuantity decimalQuantity;
    decimalQuantity.setToDouble(quantity);
    rounder->apply(decimalQuantity, status);
    if (U_FAILURE(status)) {
        return;
    }
    quantity = decimalQuantity.toDouble();

    int32_t lastIndex = unitsConverters_.length() - 1;
    if (lastIndex == 0) {
        // Only one element, no need to bubble up the carry
        return;
    }

    // Check if there's a carry, and bubble it back up the resulting intValues.
    int64_t carry = static_cast<int64_t>(floor(unitsConverters_[lastIndex]->convertInverse(quantity) * (1 + DBL_EPSILON)));
    if (carry <= 0) {
        return;
    }
    quantity -= unitsConverters_[lastIndex]->convert(static_cast<double>(carry));
    intValues[lastIndex - 1] += carry;

    // We don't use the first converter: that one is for the input unit
    for (int32_t j = lastIndex - 1; j > 0; j--) {
        carry = static_cast<int64_t>(floor(unitsConverters_[j]->convertInverse(static_cast<double>(intValues[j])) * (1 + DBL_EPSILON)));
        if (carry <= 0) {
            return;
        }
        intValues[j] -= static_cast<int64_t>(round(unitsConverters_[j]->convert(static_cast<double>(carry))));
        intValues[j - 1] += carry;
    }
}

} // namespace units
U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */
