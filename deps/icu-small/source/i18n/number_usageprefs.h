// © 2020 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING
#ifndef __NUMBER_USAGEPREFS_H__
#define __NUMBER_USAGEPREFS_H__

#include "cmemory.h"
#include "number_types.h"
#include "unicode/listformatter.h"
#include "unicode/localpointer.h"
#include "unicode/locid.h"
#include "unicode/measunit.h"
#include "unicode/stringpiece.h"
#include "unicode/uobject.h"
#include "units_converter.h"
#include "units_router.h"

U_NAMESPACE_BEGIN

using ::icu::units::ComplexUnitsConverter;
using ::icu::units::UnitsRouter;

namespace number::impl {

/**
 * A MicroPropsGenerator which uses UnitsRouter to produce output converted to a
 * MeasureUnit appropriate for a particular localized usage: see
 * NumberFormatterSettings::usage().
 */
class UsagePrefsHandler : public MicroPropsGenerator, public UMemory {
  public:
    UsagePrefsHandler(const Locale &locale, const MeasureUnit &inputUnit, const StringPiece usage,
                      const MicroPropsGenerator *parent, UErrorCode &status);

    /**
     * Obtains the appropriate output value, MeasureUnit and
     * rounding/precision behaviour from the UnitsRouter.
     *
     * The output unit is passed on to the LongNameHandler via
     * micros.outputUnit.
     */
    void processQuantity(DecimalQuantity &quantity, MicroProps &micros,
                         UErrorCode &status) const override;

    /**
     * Returns the list of possible output units, i.e. the full set of
     * preferences, for the localized, usage-specific unit preferences.
     *
     * The returned pointer should be valid for the lifetime of the
     * UsagePrefsHandler instance.
     */
    const MaybeStackVector<MeasureUnit> *getOutputUnits() const {
        return fUnitsRouter.getOutputUnits();
    }

  private:
    UnitsRouter fUnitsRouter;
    const MicroPropsGenerator *fParent;
};

/**
 * A MicroPropsGenerator which converts a measurement from one MeasureUnit to
 * another. In particular, the output MeasureUnit may be a mixed unit. (The
 * input unit may not be a mixed unit.)
 */
class UnitConversionHandler : public MicroPropsGenerator, public UMemory {
  public:
    /**
     * Constructor.
     *
     * @param targetUnit Specifies the output MeasureUnit. The input MeasureUnit
     *     is derived from it: in case of a mixed unit, the biggest unit is
     *     taken as the input unit. If not a mixed unit, the input unit will be
     *     the same as the output unit and no unit conversion takes place.
     * @param parent The parent MicroPropsGenerator.
     * @param status Receives status.
     */
    UnitConversionHandler(const MeasureUnit &targetUnit, const MicroPropsGenerator *parent,
                          UErrorCode &status);

    /**
     * Obtains the appropriate output values from the Unit Converter.
     */
    void processQuantity(DecimalQuantity &quantity, MicroProps &micros,
                         UErrorCode &status) const override;
  private:
    MeasureUnit fOutputUnit;
    LocalPointer<ComplexUnitsConverter> fUnitConverter;
    const MicroPropsGenerator *fParent;
};

} // namespace number::impl

U_NAMESPACE_END

#endif // __NUMBER_USAGEPREFS_H__
#endif /* #if !UCONFIG_NO_FORMATTING */
