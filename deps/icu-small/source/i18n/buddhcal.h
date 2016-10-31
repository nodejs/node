// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
 ********************************************************************************
 * Copyright (C) 2003-2013, International Business Machines Corporation
 * and others. All Rights Reserved.
 ********************************************************************************
 *
 * File BUDDHCAL.H
 *
 * Modification History:
 *
 *   Date        Name        Description
 *   05/13/2003  srl          copied from gregocal.h
 ********************************************************************************
 */

#ifndef BUDDHCAL_H
#define BUDDHCAL_H

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/calendar.h"
#include "unicode/gregocal.h"

U_NAMESPACE_BEGIN

/**
 * Concrete class which provides the Buddhist calendar.
 * <P>
 * <code>BuddhistCalendar</code> is a subclass of <code>GregorianCalendar</code>
 * that numbers years since the birth of the Buddha.  This is the civil calendar
 * in some predominantly Buddhist countries such as Thailand, and it is used for
 * religious purposes elsewhere.
 * <p>
 * The Buddhist calendar is identical to the Gregorian calendar in all respects
 * except for the year and era.  Years are numbered since the birth of the
 * Buddha in 543 BC (Gregorian), so that 1 AD (Gregorian) is equivalent to 544
 * BE (Buddhist Era) and 1998 AD is 2541 BE.
 * <p>
 * The Buddhist Calendar has only one allowable era: <code>BE</code>.  If the
 * calendar is not in lenient mode (see <code>setLenient</code>), dates before
 * 1/1/1 BE are rejected as an illegal argument.
 * <p>
 * @internal
 */
class BuddhistCalendar : public GregorianCalendar {
public:

    /**
     * Useful constants for BuddhistCalendar.  Only one Era.
     * @internal
     */
    enum EEras {
       BE
    };

    /**
     * Constructs a BuddhistCalendar based on the current time in the default time zone
     * with the given locale.
     *
     * @param aLocale  The given locale.
     * @param success  Indicates the status of BuddhistCalendar object construction.
     *                 Returns U_ZERO_ERROR if constructed successfully.
     * @internal
     */
    BuddhistCalendar(const Locale& aLocale, UErrorCode& success);


    /**
     * Destructor
     * @internal
     */
    virtual ~BuddhistCalendar();

    /**
     * Copy constructor
     * @param source    the object to be copied.
     * @internal
     */
    BuddhistCalendar(const BuddhistCalendar& source);

    /**
     * Default assignment operator
     * @param right    the object to be copied.
     * @internal
     */
    BuddhistCalendar& operator=(const BuddhistCalendar& right);

    /**
     * Create and return a polymorphic copy of this calendar.
     * @return    return a polymorphic copy of this calendar.
     * @internal
     */
    virtual Calendar* clone(void) const;

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

    /**
     * return the calendar type, "buddhist".
     *
     * @return calendar type
     * @internal
     */
    virtual const char * getType() const;

private:
    BuddhistCalendar(); // default constructor not implemented

 protected:
    /**
     * Return the extended year defined by the current fields.  This will
     * use the UCAL_EXTENDED_YEAR field or the UCAL_YEAR and supra-year fields (such
     * as UCAL_ERA) specific to the calendar system, depending on which set of
     * fields is newer.
     * @return the extended year
     * @internal
     */
    virtual int32_t handleGetExtendedYear();
    /**
     * Subclasses may override this method to compute several fields
     * specific to each calendar system.  
     * @internal
     */
    virtual void handleComputeFields(int32_t julianDay, UErrorCode& status);
    /**
     * Subclass API for defining limits of different types.
     * @param field one of the field numbers
     * @param limitType one of <code>MINIMUM</code>, <code>GREATEST_MINIMUM</code>,
     * <code>LEAST_MAXIMUM</code>, or <code>MAXIMUM</code>
     * @internal
     */
    virtual int32_t handleGetLimit(UCalendarDateFields field, ELimitType limitType) const;
        /**
     * Return the Julian day number of day before the first day of the
     * given month in the given extended year.  Subclasses should override
     * this method to implement their calendar system.
     * @param eyear the extended year
     * @param month the zero-based month, or 0 if useMonth is false
     * @param useMonth if false, compute the day before the first day of
     * the given year, otherwise, compute the day before the first day of
     * the given month
     * @param return the Julian day number of the day before the first
     * day of the given month and year
     * @internal
     */
    virtual int32_t handleComputeMonthStart(int32_t eyear, int32_t month,
                                            UBool useMonth) const;

    /**
     * Returns TRUE because the Buddhist Calendar does have a default century
     * @internal
     */
    virtual UBool haveDefaultCentury() const;

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
};

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */

#endif // _GREGOCAL
//eof

