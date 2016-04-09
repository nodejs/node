/*
**********************************************************************
* Copyright (c) 2004-2016, International Business Machines
* Corporation and others.  All Rights Reserved.
**********************************************************************
* Author: Alan Liu
* Created: April 26, 2004
* Since: ICU 3.0
**********************************************************************
*/
#ifndef __MEASUREUNIT_H__
#define __MEASUREUNIT_H__

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/unistr.h"

/**
 * \file 
 * \brief C++ API: A unit for measuring a quantity.
 */
 
U_NAMESPACE_BEGIN

class StringEnumeration;

/**
 * A unit such as length, mass, volume, currency, etc.  A unit is
 * coupled with a numeric amount to produce a Measure.
 *
 * @author Alan Liu
 * @stable ICU 3.0
 */
class U_I18N_API MeasureUnit: public UObject {
 public:

    /**
     * Default constructor.
     * @stable ICU 3.0
     */
    MeasureUnit() : fTypeId(0), fSubTypeId(0) { 
        fCurrency[0] = 0;
    }
    
    /**
     * Copy constructor.
     * @stable ICU 3.0
     */
    MeasureUnit(const MeasureUnit &other);
        
    /**
     * Assignment operator.
     * @stable ICU 3.0
     */
    MeasureUnit &operator=(const MeasureUnit &other);

    /**
     * Returns a polymorphic clone of this object.  The result will
     * have the same class as returned by getDynamicClassID().
     * @stable ICU 3.0
     */
    virtual UObject* clone() const;

    /**
     * Destructor
     * @stable ICU 3.0
     */
    virtual ~MeasureUnit();

    /**
     * Equality operator.  Return true if this object is equal
     * to the given object.
     * @stable ICU 3.0
     */
    virtual UBool operator==(const UObject& other) const;

    /**
     * Inequality operator.  Return true if this object is not equal
     * to the given object.
     * @stable ICU 53
     */
    UBool operator!=(const UObject& other) const {
        return !(*this == other);
    }

    /**
     * Get the type.
     * @stable ICU 53
     */
    const char *getType() const;

    /**
     * Get the sub type.
     * @stable ICU 53
     */
    const char *getSubtype() const;

    /**
     * getAvailable gets all of the available units.
     * If there are too many units to fit into destCapacity then the
     * error code is set to U_BUFFER_OVERFLOW_ERROR.
     *
     * @param destArray destination buffer.
     * @param destCapacity number of MeasureUnit instances available at dest.
     * @param errorCode ICU error code.
     * @return number of available units.
     * @stable ICU 53
     */
    static int32_t getAvailable(
            MeasureUnit *destArray,
            int32_t destCapacity,
            UErrorCode &errorCode);

    /**
     * getAvailable gets all of the available units for a specific type.
     * If there are too many units to fit into destCapacity then the
     * error code is set to U_BUFFER_OVERFLOW_ERROR.
     *
     * @param type the type
     * @param destArray destination buffer.
     * @param destCapacity number of MeasureUnit instances available at dest.
     * @param errorCode ICU error code.
     * @return number of available units for type.
     * @stable ICU 53
     */
    static int32_t getAvailable(
            const char *type,
            MeasureUnit *destArray,
            int32_t destCapacity,
            UErrorCode &errorCode);

    /**
     * getAvailableTypes gets all of the available types. Caller owns the
     * returned StringEnumeration and must delete it when finished using it.
     *
     * @param errorCode ICU error code.
     * @return the types.
     * @stable ICU 53
     */
    static StringEnumeration* getAvailableTypes(UErrorCode &errorCode);

    /**
     * Return the class ID for this class. This is useful only for comparing to
     * a return value from getDynamicClassID(). For example:
     * <pre>
     * .   Base* polymorphic_pointer = createPolymorphicObject();
     * .   if (polymorphic_pointer->getDynamicClassID() ==
     * .       erived::getStaticClassID()) ...
     * </pre>
     * @return          The class ID for all objects of this class.
     * @stable ICU 53
     */
    static UClassID U_EXPORT2 getStaticClassID(void);

    /**
     * Returns a unique class ID POLYMORPHICALLY. Pure virtual override. This
     * method is to implement a simple version of RTTI, since not all C++
     * compilers support genuine RTTI. Polymorphic operator==() and clone()
     * methods call this method.
     *
     * @return          The class ID for this object. All objects of a
     *                  given class have the same class ID.  Objects of
     *                  other classes have different class IDs.
     * @stable ICU 53
     */
    virtual UClassID getDynamicClassID(void) const;

#ifndef U_HIDE_INTERNAL_API
    /**
     * ICU use only.
     * Returns associated array index for this measure unit. Only valid for
     * non-currency measure units.
     * @internal
     */
    int32_t getIndex() const;

    /**
     * ICU use only.
     * Returns maximum value from getIndex plus 1.
     * @internal
     */
    static int32_t getIndexCount();

    /**
     * ICU use only.
     * @return the unit.getIndex() of the unit which has this unit.getType() and unit.getSubtype(),
     *         or a negative value if there is no such unit
     * @internal
     */
    static int32_t internalGetIndexForTypeAndSubtype(const char *type, const char *subtype);

    /**
     * ICU use only.
     * @internal
     */
    static MeasureUnit *resolveUnitPerUnit(
            const MeasureUnit &unit, const MeasureUnit &perUnit);
#endif /* U_HIDE_INTERNAL_API */

// All code between the "Start generated createXXX methods" comment and
// the "End generated createXXX methods" comment is auto generated code
// and must not be edited manually. For instructions on how to correctly
// update this code, refer to:
// http://site.icu-project.org/design/formatting/measureformat/updating-measure-unit
//
// Start generated createXXX methods

    /**
     * Returns unit of acceleration: g-force.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 53
     */
    static MeasureUnit *createGForce(UErrorCode &status);

    /**
     * Returns unit of acceleration: meter-per-second-squared.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createMeterPerSecondSquared(UErrorCode &status);

    /**
     * Returns unit of angle: arc-minute.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 53
     */
    static MeasureUnit *createArcMinute(UErrorCode &status);

    /**
     * Returns unit of angle: arc-second.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 53
     */
    static MeasureUnit *createArcSecond(UErrorCode &status);

    /**
     * Returns unit of angle: degree.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 53
     */
    static MeasureUnit *createDegree(UErrorCode &status);

    /**
     * Returns unit of angle: radian.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createRadian(UErrorCode &status);

#ifndef U_HIDE_DRAFT_API
    /**
     * Returns unit of angle: revolution.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @draft ICU 56
     */
    static MeasureUnit *createRevolutionAngle(UErrorCode &status);
#endif  /* U_HIDE_DRAFT_API */

    /**
     * Returns unit of area: acre.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 53
     */
    static MeasureUnit *createAcre(UErrorCode &status);

    /**
     * Returns unit of area: hectare.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 53
     */
    static MeasureUnit *createHectare(UErrorCode &status);

    /**
     * Returns unit of area: square-centimeter.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createSquareCentimeter(UErrorCode &status);

    /**
     * Returns unit of area: square-foot.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 53
     */
    static MeasureUnit *createSquareFoot(UErrorCode &status);

    /**
     * Returns unit of area: square-inch.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createSquareInch(UErrorCode &status);

    /**
     * Returns unit of area: square-kilometer.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 53
     */
    static MeasureUnit *createSquareKilometer(UErrorCode &status);

    /**
     * Returns unit of area: square-meter.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 53
     */
    static MeasureUnit *createSquareMeter(UErrorCode &status);

    /**
     * Returns unit of area: square-mile.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 53
     */
    static MeasureUnit *createSquareMile(UErrorCode &status);

    /**
     * Returns unit of area: square-yard.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createSquareYard(UErrorCode &status);

    /**
     * Returns unit of concentr: karat.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createKarat(UErrorCode &status);

#ifndef U_HIDE_DRAFT_API
    /**
     * Returns unit of concentr: milligram-per-deciliter.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @draft ICU 57
     */
    static MeasureUnit *createMilligramPerDeciliter(UErrorCode &status);
#endif  /* U_HIDE_DRAFT_API */

#ifndef U_HIDE_DRAFT_API
    /**
     * Returns unit of concentr: millimole-per-liter.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @draft ICU 57
     */
    static MeasureUnit *createMillimolePerLiter(UErrorCode &status);
#endif  /* U_HIDE_DRAFT_API */

#ifndef U_HIDE_DRAFT_API
    /**
     * Returns unit of concentr: part-per-million.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @draft ICU 57
     */
    static MeasureUnit *createPartPerMillion(UErrorCode &status);
#endif  /* U_HIDE_DRAFT_API */

#ifndef U_HIDE_DRAFT_API
    /**
     * Returns unit of consumption: liter-per-100kilometers.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @draft ICU 56
     */
    static MeasureUnit *createLiterPer100Kilometers(UErrorCode &status);
#endif  /* U_HIDE_DRAFT_API */

    /**
     * Returns unit of consumption: liter-per-kilometer.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createLiterPerKilometer(UErrorCode &status);

    /**
     * Returns unit of consumption: mile-per-gallon.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createMilePerGallon(UErrorCode &status);

#ifndef U_HIDE_DRAFT_API
    /**
     * Returns unit of consumption: mile-per-gallon-imperial.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @draft ICU 57
     */
    static MeasureUnit *createMilePerGallonImperial(UErrorCode &status);
#endif  /* U_HIDE_DRAFT_API */

    /**
     * Returns unit of digital: bit.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createBit(UErrorCode &status);

    /**
     * Returns unit of digital: byte.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createByte(UErrorCode &status);

    /**
     * Returns unit of digital: gigabit.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createGigabit(UErrorCode &status);

    /**
     * Returns unit of digital: gigabyte.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createGigabyte(UErrorCode &status);

    /**
     * Returns unit of digital: kilobit.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createKilobit(UErrorCode &status);

    /**
     * Returns unit of digital: kilobyte.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createKilobyte(UErrorCode &status);

    /**
     * Returns unit of digital: megabit.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createMegabit(UErrorCode &status);

    /**
     * Returns unit of digital: megabyte.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createMegabyte(UErrorCode &status);

    /**
     * Returns unit of digital: terabit.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createTerabit(UErrorCode &status);

    /**
     * Returns unit of digital: terabyte.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createTerabyte(UErrorCode &status);

#ifndef U_HIDE_DRAFT_API
    /**
     * Returns unit of duration: century.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @draft ICU 56
     */
    static MeasureUnit *createCentury(UErrorCode &status);
#endif  /* U_HIDE_DRAFT_API */

    /**
     * Returns unit of duration: day.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 53
     */
    static MeasureUnit *createDay(UErrorCode &status);

    /**
     * Returns unit of duration: hour.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 53
     */
    static MeasureUnit *createHour(UErrorCode &status);

    /**
     * Returns unit of duration: microsecond.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createMicrosecond(UErrorCode &status);

    /**
     * Returns unit of duration: millisecond.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 53
     */
    static MeasureUnit *createMillisecond(UErrorCode &status);

    /**
     * Returns unit of duration: minute.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 53
     */
    static MeasureUnit *createMinute(UErrorCode &status);

    /**
     * Returns unit of duration: month.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 53
     */
    static MeasureUnit *createMonth(UErrorCode &status);

    /**
     * Returns unit of duration: nanosecond.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createNanosecond(UErrorCode &status);

    /**
     * Returns unit of duration: second.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 53
     */
    static MeasureUnit *createSecond(UErrorCode &status);

    /**
     * Returns unit of duration: week.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 53
     */
    static MeasureUnit *createWeek(UErrorCode &status);

    /**
     * Returns unit of duration: year.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 53
     */
    static MeasureUnit *createYear(UErrorCode &status);

    /**
     * Returns unit of electric: ampere.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createAmpere(UErrorCode &status);

    /**
     * Returns unit of electric: milliampere.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createMilliampere(UErrorCode &status);

    /**
     * Returns unit of electric: ohm.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createOhm(UErrorCode &status);

    /**
     * Returns unit of electric: volt.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createVolt(UErrorCode &status);

    /**
     * Returns unit of energy: calorie.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createCalorie(UErrorCode &status);

    /**
     * Returns unit of energy: foodcalorie.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createFoodcalorie(UErrorCode &status);

    /**
     * Returns unit of energy: joule.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createJoule(UErrorCode &status);

    /**
     * Returns unit of energy: kilocalorie.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createKilocalorie(UErrorCode &status);

    /**
     * Returns unit of energy: kilojoule.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createKilojoule(UErrorCode &status);

    /**
     * Returns unit of energy: kilowatt-hour.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createKilowattHour(UErrorCode &status);

    /**
     * Returns unit of frequency: gigahertz.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createGigahertz(UErrorCode &status);

    /**
     * Returns unit of frequency: hertz.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createHertz(UErrorCode &status);

    /**
     * Returns unit of frequency: kilohertz.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createKilohertz(UErrorCode &status);

    /**
     * Returns unit of frequency: megahertz.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createMegahertz(UErrorCode &status);

    /**
     * Returns unit of length: astronomical-unit.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createAstronomicalUnit(UErrorCode &status);

    /**
     * Returns unit of length: centimeter.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 53
     */
    static MeasureUnit *createCentimeter(UErrorCode &status);

    /**
     * Returns unit of length: decimeter.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createDecimeter(UErrorCode &status);

    /**
     * Returns unit of length: fathom.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createFathom(UErrorCode &status);

    /**
     * Returns unit of length: foot.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 53
     */
    static MeasureUnit *createFoot(UErrorCode &status);

    /**
     * Returns unit of length: furlong.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createFurlong(UErrorCode &status);

    /**
     * Returns unit of length: inch.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 53
     */
    static MeasureUnit *createInch(UErrorCode &status);

    /**
     * Returns unit of length: kilometer.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 53
     */
    static MeasureUnit *createKilometer(UErrorCode &status);

    /**
     * Returns unit of length: light-year.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 53
     */
    static MeasureUnit *createLightYear(UErrorCode &status);

    /**
     * Returns unit of length: meter.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 53
     */
    static MeasureUnit *createMeter(UErrorCode &status);

    /**
     * Returns unit of length: micrometer.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createMicrometer(UErrorCode &status);

    /**
     * Returns unit of length: mile.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 53
     */
    static MeasureUnit *createMile(UErrorCode &status);

#ifndef U_HIDE_DRAFT_API
    /**
     * Returns unit of length: mile-scandinavian.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @draft ICU 56
     */
    static MeasureUnit *createMileScandinavian(UErrorCode &status);
#endif  /* U_HIDE_DRAFT_API */

    /**
     * Returns unit of length: millimeter.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 53
     */
    static MeasureUnit *createMillimeter(UErrorCode &status);

    /**
     * Returns unit of length: nanometer.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createNanometer(UErrorCode &status);

    /**
     * Returns unit of length: nautical-mile.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createNauticalMile(UErrorCode &status);

    /**
     * Returns unit of length: parsec.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createParsec(UErrorCode &status);

    /**
     * Returns unit of length: picometer.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 53
     */
    static MeasureUnit *createPicometer(UErrorCode &status);

    /**
     * Returns unit of length: yard.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 53
     */
    static MeasureUnit *createYard(UErrorCode &status);

    /**
     * Returns unit of light: lux.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createLux(UErrorCode &status);

    /**
     * Returns unit of mass: carat.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createCarat(UErrorCode &status);

    /**
     * Returns unit of mass: gram.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 53
     */
    static MeasureUnit *createGram(UErrorCode &status);

    /**
     * Returns unit of mass: kilogram.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 53
     */
    static MeasureUnit *createKilogram(UErrorCode &status);

    /**
     * Returns unit of mass: metric-ton.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createMetricTon(UErrorCode &status);

    /**
     * Returns unit of mass: microgram.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createMicrogram(UErrorCode &status);

    /**
     * Returns unit of mass: milligram.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createMilligram(UErrorCode &status);

    /**
     * Returns unit of mass: ounce.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 53
     */
    static MeasureUnit *createOunce(UErrorCode &status);

    /**
     * Returns unit of mass: ounce-troy.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createOunceTroy(UErrorCode &status);

    /**
     * Returns unit of mass: pound.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 53
     */
    static MeasureUnit *createPound(UErrorCode &status);

    /**
     * Returns unit of mass: stone.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createStone(UErrorCode &status);

    /**
     * Returns unit of mass: ton.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createTon(UErrorCode &status);

    /**
     * Returns unit of power: gigawatt.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createGigawatt(UErrorCode &status);

    /**
     * Returns unit of power: horsepower.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 53
     */
    static MeasureUnit *createHorsepower(UErrorCode &status);

    /**
     * Returns unit of power: kilowatt.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 53
     */
    static MeasureUnit *createKilowatt(UErrorCode &status);

    /**
     * Returns unit of power: megawatt.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createMegawatt(UErrorCode &status);

    /**
     * Returns unit of power: milliwatt.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createMilliwatt(UErrorCode &status);

    /**
     * Returns unit of power: watt.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 53
     */
    static MeasureUnit *createWatt(UErrorCode &status);

    /**
     * Returns unit of pressure: hectopascal.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 53
     */
    static MeasureUnit *createHectopascal(UErrorCode &status);

    /**
     * Returns unit of pressure: inch-hg.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 53
     */
    static MeasureUnit *createInchHg(UErrorCode &status);

    /**
     * Returns unit of pressure: millibar.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 53
     */
    static MeasureUnit *createMillibar(UErrorCode &status);

    /**
     * Returns unit of pressure: millimeter-of-mercury.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createMillimeterOfMercury(UErrorCode &status);

    /**
     * Returns unit of pressure: pound-per-square-inch.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createPoundPerSquareInch(UErrorCode &status);

    /**
     * Returns unit of speed: kilometer-per-hour.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 53
     */
    static MeasureUnit *createKilometerPerHour(UErrorCode &status);

#ifndef U_HIDE_DRAFT_API
    /**
     * Returns unit of speed: knot.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @draft ICU 56
     */
    static MeasureUnit *createKnot(UErrorCode &status);
#endif  /* U_HIDE_DRAFT_API */

    /**
     * Returns unit of speed: meter-per-second.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 53
     */
    static MeasureUnit *createMeterPerSecond(UErrorCode &status);

    /**
     * Returns unit of speed: mile-per-hour.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 53
     */
    static MeasureUnit *createMilePerHour(UErrorCode &status);

    /**
     * Returns unit of temperature: celsius.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 53
     */
    static MeasureUnit *createCelsius(UErrorCode &status);

    /**
     * Returns unit of temperature: fahrenheit.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 53
     */
    static MeasureUnit *createFahrenheit(UErrorCode &status);

#ifndef U_HIDE_DRAFT_API
    /**
     * Returns unit of temperature: generic.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @draft ICU 56
     */
    static MeasureUnit *createGenericTemperature(UErrorCode &status);
#endif  /* U_HIDE_DRAFT_API */

    /**
     * Returns unit of temperature: kelvin.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createKelvin(UErrorCode &status);

    /**
     * Returns unit of volume: acre-foot.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createAcreFoot(UErrorCode &status);

    /**
     * Returns unit of volume: bushel.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createBushel(UErrorCode &status);

    /**
     * Returns unit of volume: centiliter.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createCentiliter(UErrorCode &status);

    /**
     * Returns unit of volume: cubic-centimeter.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createCubicCentimeter(UErrorCode &status);

    /**
     * Returns unit of volume: cubic-foot.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createCubicFoot(UErrorCode &status);

    /**
     * Returns unit of volume: cubic-inch.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createCubicInch(UErrorCode &status);

    /**
     * Returns unit of volume: cubic-kilometer.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 53
     */
    static MeasureUnit *createCubicKilometer(UErrorCode &status);

    /**
     * Returns unit of volume: cubic-meter.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createCubicMeter(UErrorCode &status);

    /**
     * Returns unit of volume: cubic-mile.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 53
     */
    static MeasureUnit *createCubicMile(UErrorCode &status);

    /**
     * Returns unit of volume: cubic-yard.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createCubicYard(UErrorCode &status);

    /**
     * Returns unit of volume: cup.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createCup(UErrorCode &status);

#ifndef U_HIDE_DRAFT_API
    /**
     * Returns unit of volume: cup-metric.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @draft ICU 56
     */
    static MeasureUnit *createCupMetric(UErrorCode &status);
#endif  /* U_HIDE_DRAFT_API */

    /**
     * Returns unit of volume: deciliter.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createDeciliter(UErrorCode &status);

    /**
     * Returns unit of volume: fluid-ounce.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createFluidOunce(UErrorCode &status);

    /**
     * Returns unit of volume: gallon.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createGallon(UErrorCode &status);

#ifndef U_HIDE_DRAFT_API
    /**
     * Returns unit of volume: gallon-imperial.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @draft ICU 57
     */
    static MeasureUnit *createGallonImperial(UErrorCode &status);
#endif  /* U_HIDE_DRAFT_API */

    /**
     * Returns unit of volume: hectoliter.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createHectoliter(UErrorCode &status);

    /**
     * Returns unit of volume: liter.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 53
     */
    static MeasureUnit *createLiter(UErrorCode &status);

    /**
     * Returns unit of volume: megaliter.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createMegaliter(UErrorCode &status);

    /**
     * Returns unit of volume: milliliter.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createMilliliter(UErrorCode &status);

    /**
     * Returns unit of volume: pint.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createPint(UErrorCode &status);

#ifndef U_HIDE_DRAFT_API
    /**
     * Returns unit of volume: pint-metric.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @draft ICU 56
     */
    static MeasureUnit *createPintMetric(UErrorCode &status);
#endif  /* U_HIDE_DRAFT_API */

    /**
     * Returns unit of volume: quart.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createQuart(UErrorCode &status);

    /**
     * Returns unit of volume: tablespoon.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createTablespoon(UErrorCode &status);

    /**
     * Returns unit of volume: teaspoon.
     * Caller owns returned value and must free it.
     * @param status ICU error code.
     * @stable ICU 54
     */
    static MeasureUnit *createTeaspoon(UErrorCode &status);


// End generated createXXX methods

 protected:

#ifndef U_HIDE_INTERNAL_API
    /**
     * For ICU use only.
     * @internal
     */
    void initTime(const char *timeId);

    /**
     * For ICU use only.
     * @internal
     */
    void initCurrency(const char *isoCurrency);

#endif  /* U_HIDE_INTERNAL_API */

private:
    int32_t fTypeId;
    int32_t fSubTypeId;
    char fCurrency[4];

    MeasureUnit(int32_t typeId, int32_t subTypeId) : fTypeId(typeId), fSubTypeId(subTypeId) {
        fCurrency[0] = 0;
    }
    void setTo(int32_t typeId, int32_t subTypeId);
    int32_t getOffset() const;
    static MeasureUnit *create(int typeId, int subTypeId, UErrorCode &status);
};

U_NAMESPACE_END

#endif // !UNCONFIG_NO_FORMATTING
#endif // __MEASUREUNIT_H__
