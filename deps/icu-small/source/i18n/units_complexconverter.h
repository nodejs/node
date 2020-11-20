// © 2020 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING
#ifndef __UNITS_COMPLEXCONVERTER_H__
#define __UNITS_COMPLEXCONVERTER_H__

#include "cmemory.h"
#include "measunit_impl.h"
#include "number_roundingutils.h"
#include "unicode/errorcode.h"
#include "unicode/measure.h"
#include "units_converter.h"
#include "units_data.h"

U_NAMESPACE_BEGIN

// Export explicit template instantiations of MaybeStackArray, MemoryPool and
// MaybeStackVector. This is required when building DLLs for Windows. (See
// datefmt.h, collationiterator.h, erarules.h and others for similar examples.)
//
// Note: These need to be outside of the units namespace, or Clang will generate
// a compile error.
#if U_PF_WINDOWS <= U_PLATFORM && U_PLATFORM <= U_PF_CYGWIN
template class U_I18N_API MaybeStackArray<units::UnitConverter*, 8>;
template class U_I18N_API MemoryPool<units::UnitConverter, 8>;
template class U_I18N_API MaybeStackVector<units::UnitConverter, 8>;
template class U_I18N_API MaybeStackArray<MeasureUnitImpl*, 8>;
template class U_I18N_API MemoryPool<MeasureUnitImpl, 8>;
template class U_I18N_API MaybeStackVector<MeasureUnitImpl, 8>;
template class U_I18N_API MaybeStackArray<MeasureUnit*, 8>;
template class U_I18N_API MemoryPool<MeasureUnit, 8>;
template class U_I18N_API MaybeStackVector<MeasureUnit, 8>;
#endif

namespace units {

/**
 *  Converts from single or compound unit to single, compound or mixed units.
 * For example, from `meter` to `foot+inch`.
 *
 *  DESIGN:
 *    This class uses `UnitConverter` in order to perform the single converter (i.e. converters from a
 *    single unit to another single unit). Therefore, `ComplexUnitsConverter` class contains multiple
 *    instances of the `UnitConverter` to perform the conversion.
 */
class U_I18N_API ComplexUnitsConverter : public UMemory {
  public:
    /**
     * Constructor of `ComplexUnitsConverter`.
     * NOTE:
     *   - inputUnit and outputUnits must be under the same category
     *      - e.g. meter to feet and inches --> all of them are length units.
     *
     * @param inputUnit represents the source unit. (should be single or compound unit).
     * @param outputUnits represents the output unit. could be any type. (single, compound or mixed).
     * @param status
     */
    ComplexUnitsConverter(const MeasureUnitImpl &inputUnit, const MeasureUnitImpl &outputUnits,
                          const ConversionRates &ratesInfo, UErrorCode &status);

    // Returns true if the specified `quantity` of the `inputUnit`, expressed in terms of the biggest
    // unit in the MeasureUnit `outputUnit`, is greater than or equal to `limit`.
    //    For example, if the input unit is `meter` and the target unit is `foot+inch`. Therefore, this
    //    function will convert the `quantity` from `meter` to `foot`, then, it will compare the value in
    //    `foot` with the `limit`.
    UBool greaterThanOrEqual(double quantity, double limit) const;

    // Returns outputMeasures which is an array with the corresponding values.
    //    - E.g. converting meters to feet and inches.
    //                  1 meter --> 3 feet, 3.3701 inches
    //         NOTE:
    //           the smallest element is the only element that could have fractional values. And all
    //           other elements are floored to the nearest integer
    MaybeStackVector<Measure>
    convert(double quantity, icu::number::impl::RoundingImpl *rounder, UErrorCode &status) const;

  private:
    MaybeStackVector<UnitConverter> unitConverters_;
    // Individual units of mixed units, sorted big to small
    MaybeStackVector<MeasureUnitImpl> units_;
    // Individual units of mixed units, sorted in desired output order
    MaybeStackVector<MeasureUnit> outputUnits_;
};

} // namespace units
U_NAMESPACE_END

#endif //__UNITS_COMPLEXCONVERTER_H__

#endif /* #if !UCONFIG_NO_FORMATTING */
