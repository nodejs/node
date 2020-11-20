// © 2020 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING
#ifndef __UNITS_ROUTER_H__
#define __UNITS_ROUTER_H__

#include <limits>

#include "cmemory.h"
#include "measunit_impl.h"
#include "unicode/measunit.h"
#include "unicode/stringpiece.h"
#include "unicode/uobject.h"
#include "units_complexconverter.h"
#include "units_data.h"

U_NAMESPACE_BEGIN

// Forward declarations
class Measure;
namespace number {
class Precision;
}

namespace units {

struct RouteResult : UMemory {
    // A list of measures: a single measure for single units, multiple measures
    // for mixed units.
    //
    // TODO(icu-units/icu#21): figure out the right mixed unit API.
    MaybeStackVector<Measure> measures;

    // The output unit for this RouteResult. This may be a MIXED unit - for
    // example: "yard-and-foot-and-inch", for which `measures` will have three
    // elements.
    MeasureUnitImpl outputUnit;

    RouteResult(MaybeStackVector<Measure> measures, MeasureUnitImpl outputUnit)
        : measures(std::move(measures)), outputUnit(std::move(outputUnit)) {}
};

/**
 * Contains the complex unit converter and the limit which representing the smallest value that the
 * converter should accept. For example, if the converter is converting to `foot+inch` and the limit
 * equals 3.0, thus means the converter should not convert to a value less than `3.0 feet`.
 *
 * NOTE:
 *    if the limit doest not has a value `i.e. (std::numeric_limits<double>::lowest())`, this mean there
 *    is no limit for the converter.
 */
struct ConverterPreference : UMemory {
    ComplexUnitsConverter converter;
    double limit;
    UnicodeString precision;

    // The output unit for this ConverterPreference. This may be a MIXED unit -
    // for example: "yard-and-foot-and-inch".
    MeasureUnitImpl targetUnit;

    // In case there is no limit, the limit will be -inf.
    ConverterPreference(const MeasureUnitImpl &source, const MeasureUnitImpl &complexTarget,
                        UnicodeString precision, const ConversionRates &ratesInfo, UErrorCode &status)
        : ConverterPreference(source, complexTarget, std::numeric_limits<double>::lowest(), precision,
                              ratesInfo, status) {}

    ConverterPreference(const MeasureUnitImpl &source, const MeasureUnitImpl &complexTarget,
                        double limit, UnicodeString precision, const ConversionRates &ratesInfo,
                        UErrorCode &status)
        : converter(source, complexTarget, ratesInfo, status), limit(limit),
          precision(std::move(precision)), targetUnit(complexTarget.copy(status)) {}
};

} // namespace units

// Export explicit template instantiations of MaybeStackArray, MemoryPool and
// MaybeStackVector. This is required when building DLLs for Windows. (See
// datefmt.h, collationiterator.h, erarules.h and others for similar examples.)
//
// Note: These need to be outside of the units namespace, or Clang will generate
// a compile error.
#if U_PF_WINDOWS <= U_PLATFORM && U_PLATFORM <= U_PF_CYGWIN
template class U_I18N_API MaybeStackArray<units::ConverterPreference*, 8>;
template class U_I18N_API MemoryPool<units::ConverterPreference, 8>;
template class U_I18N_API MaybeStackVector<units::ConverterPreference, 8>;
#endif

namespace units {

/**
 * `UnitsRouter` responsible for converting from a single unit (such as `meter` or `meter-per-second`) to
 * one of the complex units based on the limits.
 * For example:
 *    if the input is `meter` and the output as following
 *    {`foot+inch`, limit: 3.0}
 *    {`inch`     , limit: no value (-inf)}
 *    Thus means if the input in `meter` is greater than or equal to `3.0 feet`, the output will be in
 *    `foot+inch`, otherwise, the output will be in `inch`.
 *
 * NOTE:
 *    the output units and the their limits MUST BE in order, for example, if the output units, from the
 *    previous example, are the following:
 *        {`inch`     , limit: no value (-inf)}
 *        {`foot+inch`, limit: 3.0}
 *     IN THIS CASE THE OUTPUT WILL BE ALWAYS IN `inch`.
 *
 * NOTE:
 *    the output units  and their limits will be extracted from the units preferences database by knowing
 *    the followings:
 *        - input unit
 *        - locale
 *        - usage
 *
 * DESIGN:
 *    `UnitRouter` uses internally `ComplexUnitConverter` in order to convert the input units to the
 *    desired complex units and to check the limit too.
 */
class U_I18N_API UnitsRouter {
  public:
    UnitsRouter(MeasureUnit inputUnit, StringPiece locale, StringPiece usage, UErrorCode &status);

    /**
     * Performs locale and usage sensitive unit conversion.
     * @param quantity The quantity to convert, expressed in terms of inputUnit.
     * @param rounder If not null, this RoundingImpl will be used to do rounding
     *     on the converted value. If the rounder lacks an fPrecision, the
     *     rounder will be modified to use the preferred precision for the usage
     *     and locale preference, alternatively with the default precision.
     * @param status Receives status.
     */
    RouteResult route(double quantity, icu::number::impl::RoundingImpl *rounder, UErrorCode &status) const;

    /**
     * Returns the list of possible output units, i.e. the full set of
     * preferences, for the localized, usage-specific unit preferences.
     *
     * The returned pointer should be valid for the lifetime of the
     * UnitsRouter instance.
     */
    const MaybeStackVector<MeasureUnit> *getOutputUnits() const;

  private:
    // List of possible output units. TODO: converterPreferences_ now also has
    // this data available. Maybe drop outputUnits_ and have getOutputUnits
    // construct a the list from data in converterPreferences_ instead?
    MaybeStackVector<MeasureUnit> outputUnits_;

    MaybeStackVector<ConverterPreference> converterPreferences_;

    static number::Precision parseSkeletonToPrecision(icu::UnicodeString precisionSkeleton,
                                                      UErrorCode &status);
};

} // namespace units
U_NAMESPACE_END

#endif //__UNITS_ROUTER_H__

#endif /* #if !UCONFIG_NO_FORMATTING */
