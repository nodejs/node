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
template class U_I18N_API MaybeStackArray<units::UnitsConverter*, 8>;
template class U_I18N_API MemoryPool<units::UnitsConverter, 8>;
template class U_I18N_API MaybeStackVector<units::UnitsConverter, 8>;
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
 *    This class uses `UnitsConverter` in order to perform the single converter (i.e. converters from a
 *    single unit to another single unit). Therefore, `ComplexUnitsConverter` class contains multiple
 *    instances of the `UnitsConverter` to perform the conversion.
 */
class U_I18N_API ComplexUnitsConverter : public UMemory {
  public:
    /**
     * Constructs `ComplexUnitsConverter` for an `targetUnit` that could be Single, Compound or Mixed.
     * In case of:
     * 1- Single and Compound units,
     *    the conversion will not perform anything, the input will be equal to the output.
     * 2- Mixed Unit
     *    the conversion will consider the input is the biggest unit. And will convert it to be spread
     *    through the target units. For example: if target unit is "inch-and-foot", and the input is 2.5.
     *    The converter will consider the input value in "foot", because foot is the biggest unit.
     *    Then, it will convert 2.5 feet to "inch-and-foot".
     *
     * @param targetUnit could be any units type (single, compound or mixed).
     * @param ratesInfo
     * @param status
     */
    ComplexUnitsConverter(const MeasureUnitImpl &targetUnit, const ConversionRates &ratesInfo,
                          UErrorCode &status);
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
    ComplexUnitsConverter(StringPiece inputUnitIdentifier, StringPiece outputUnitsIdentifier,
                          UErrorCode &status);

    /**
     * Constructor of `ComplexUnitsConverter`.
     * NOTE:
     *   - inputUnit and outputUnits must be under the same category
     *      - e.g. meter to feet and inches --> all of them are length units.
     *
     * @param inputUnit represents the source unit. (should be single or compound unit).
     * @param outputUnits represents the output unit. could be any type. (single, compound or mixed).
     * @param ratesInfo a ConversionRates instance containing the unit conversion rates.
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
    MaybeStackVector<UnitsConverter> unitsConverters_;

    // Individual units of mixed units, sorted big to small, with indices
    // indicating the requested output mixed unit order.
    MaybeStackVector<MeasureUnitImplWithIndex> units_;

    // Sorts units_, which must be populated before calling this, and populates
    // unitsConverters_.
    void init(const MeasureUnitImpl &inputUnit, const ConversionRates &ratesInfo, UErrorCode &status);

    // Applies the rounder to the quantity (last element) and bubble up any carried value to all the
    // intValues.
    // TODO(ICU-21288): get smarter about precision for mixed units.
    void applyRounder(MaybeStackArray<int64_t, 5> &intValues, double &quantity,
                      icu::number::impl::RoundingImpl *rounder, UErrorCode &status) const;
};

} // namespace units
U_NAMESPACE_END

#endif //__UNITS_COMPLEXCONVERTER_H__

#endif /* #if !UCONFIG_NO_FORMATTING */
