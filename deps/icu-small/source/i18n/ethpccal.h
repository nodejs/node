/*
*******************************************************************************
* Copyright (C) 2003 - 2013, International Business Machines Corporation and  *
* others. All Rights Reserved.                                                *
*******************************************************************************
*/

#ifndef ETHPCCAL_H
#define ETHPCCAL_H

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/calendar.h"
#include "cecal.h"

U_NAMESPACE_BEGIN

/**
 * Implement the Ethiopic calendar system.
 * @internal
 */
class EthiopicCalendar : public CECalendar {

public:
    /**
     * Calendar type - use Amete Alem era for all the time or not
     * @internal
     */
    enum EEraType {
        AMETE_MIHRET_ERA,
        AMETE_ALEM_ERA
    };

    /**
     * Useful constants for EthiopicCalendar.
     * @internal
     */
    enum EMonths {
        /**
         * Constant for &#x1218;&#x1235;&#x12a8;&#x1228;&#x121d;, the 1st month of the Ethiopic year.
         */
        MESKEREM,

        /**
         * Constant for &#x1325;&#x1245;&#x121d;&#x1275;, the 2nd month of the Ethiopic year.
         */
        TEKEMT,

        /**
         * Constant for &#x1285;&#x12f3;&#x122d;, the 3rd month of the Ethiopic year.
         */
        HEDAR,

        /**
         * Constant for &#x1273;&#x1285;&#x1223;&#x1225;, the 4th month of the Ethiopic year.
         */
        TAHSAS,

        /**
         * Constant for &#x1325;&#x122d;, the 5th month of the Ethiopic year.
         */
        TER,

        /**
         * Constant for &#x12e8;&#x12ab;&#x1272;&#x1275;, the 6th month of the Ethiopic year.
         */
        YEKATIT,

        /**
         * Constant for &#x1218;&#x130b;&#x1262;&#x1275;, the 7th month of the Ethiopic year.
         */
        MEGABIT,

        /**
         * Constant for &#x121a;&#x12eb;&#x12dd;&#x12eb;, the 8th month of the Ethiopic year.
         */
        MIAZIA,

        /**
         * Constant for &#x130d;&#x1295;&#x1266;&#x1275;, the 9th month of the Ethiopic year.
         */
        GENBOT,

        /**
         * Constant for &#x1230;&#x1294;, the 10th month of the Ethiopic year.
         */
        SENE,

        /**
         * Constant for &#x1210;&#x121d;&#x120c;, the 11th month of the Ethiopic year.
         */
        HAMLE,

        /**
         * Constant for &#x1290;&#x1210;&#x1234;, the 12th month of the Ethiopic year.
         */
        NEHASSA,

        /**
         * Constant for &#x1333;&#x1309;&#x121c;&#x1295;, the 13th month of the Ethiopic year.
         */
        PAGUMEN
    };

    enum EEras {
        AMETE_ALEM,     // Before the epoch
        AMETE_MIHRET    // After the epoch
    };

    /**
     * Constructs a EthiopicCalendar based on the current time in the default time zone
     * with the given locale.
     *
     * @param aLocale  The given locale.
     * @param success  Indicates the status of EthiopicCalendar object construction.
     *                 Returns U_ZERO_ERROR if constructed successfully.
     * @param type     Whether this Ethiopic calendar use Amete Mihrret (default) or
     *                 only use Amete Alem for all the time.
     * @internal
     */
    EthiopicCalendar(const Locale& aLocale, UErrorCode& success, EEraType type = AMETE_MIHRET_ERA);

    /**
     * Copy Constructor
     * @internal
     */
    EthiopicCalendar(const EthiopicCalendar& other);

    /**
     * Destructor.
     * @internal
     */
    virtual ~EthiopicCalendar();

    /**
     * Create and return a polymorphic copy of this calendar.
     * @return    return a polymorphic copy of this calendar.
     * @internal
     */
    virtual Calendar* clone() const;

    /**
     * return the calendar type, "ethiopic"
     * @return calendar type
     * @internal
     */
    virtual const char * getType() const;

    /**
     * Set Alem or Mihret era.
     * @param onOff Set Amete Alem era if true, otherwise set Amete Mihret era.
     * @internal
     */
    void setAmeteAlemEra (UBool onOff);

    /**
     * Return true if this calendar is set to the Amete Alem era.
     * @return true if set to the Amete Alem era.
     * @internal
     */
    UBool isAmeteAlemEra() const;

protected:
    //-------------------------------------------------------------------------
    // Calendar framework
    //-------------------------------------------------------------------------

    /**
     * Return the extended year defined by the current fields.
     * @internal
     */
    virtual int32_t handleGetExtendedYear();

    /**
     * Compute fields from the JD
     * @internal
     */
    virtual void handleComputeFields(int32_t julianDay, UErrorCode &status);

    /**
     * Calculate the limit for a specified type of limit and field
     * @internal
     */
    virtual int32_t handleGetLimit(UCalendarDateFields field, ELimitType limitType) const;

    /**
     * Returns the date of the start of the default century
     * @return start of century - in milliseconds since epoch, 1970
     * @internal
     */
    virtual UDate defaultCenturyStart() const;

    /**
     * Returns the year in which the default century begins
     * @internal
     */
    virtual int32_t defaultCenturyStartYear() const;

    /**
     * Return the date offset from Julian
     * @internal
     */
    virtual int32_t getJDEpochOffset() const;

private:
    /**
     * When eraType is AMETE_ALEM_ERA, then this calendar use only AMETE_ALEM
     * for the era. Otherwise (default), this calendar uses both AMETE_ALEM
     * and AMETE_MIHRET.
     *
     * EXTENDED_YEAR        AMETE_ALEM_ERA     AMETE_MIHRET_ERA
     *             0       Amete Alem 5500      Amete Alem 5500
     *             1        Amete Mihret 1      Amete Alem 5501
     */
    EEraType eraType;

public:
    /**
     * Override Calendar Returns a unique class ID POLYMORPHICALLY. Pure virtual
     * override. This method is to implement a simple version of RTTI, since not all C++
     * compilers support genuine RTTI. Polymorphic operator==() and clone() methods call
     * this method.
     *
     * @return   The class ID for this object. All objects of a given class have the
     *           same class ID. Objects of other classes have different class IDs.
     * @internal
     */
    virtual UClassID getDynamicClassID(void) const;

    /**
     * Return the class ID for this class. This is useful only for comparing to a return
     * value from getDynamicClassID(). For example:
     *
     *      Base* polymorphic_pointer = createPolymorphicObject();
     *      if (polymorphic_pointer->getDynamicClassID() ==
     *          Derived::getStaticClassID()) ...
     *
     * @return   The class ID for all objects of this class.
     * @internal
     */
    U_I18N_API static UClassID U_EXPORT2 getStaticClassID(void);

#if 0
// We do not want to introduce this API in ICU4C.
// It was accidentally introduced in ICU4J as a public API.

public:
    //-------------------------------------------------------------------------
    // Calendar system Conversion methods...
    //-------------------------------------------------------------------------

    /**
     * Convert an Ethiopic year, month, and day to a Julian day.
     *
     * @param year the extended year
     * @param month the month
     * @param day the day
     * @return Julian day
     * @internal
     */
    int32_t ethiopicToJD(int32_t year, int32_t month, int32_t day);
#endif
};

U_NAMESPACE_END
#endif /* #if !UCONFIG_NO_FORMATTING */
#endif /* ETHPCCAL_H */
//eof
