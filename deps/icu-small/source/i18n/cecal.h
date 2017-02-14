// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 2003 - 2008, International Business Machines Corporation and  *
* others. All Rights Reserved.                                                *
*******************************************************************************
*/

#ifndef CECAL_H
#define CECAL_H

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/calendar.h"

U_NAMESPACE_BEGIN

/**
 * Base class for EthiopicCalendar and CopticCalendar.
 * @internal
 */
class U_I18N_API CECalendar : public Calendar {

protected:
    //-------------------------------------------------------------------------
    // Constructors...
    //-------------------------------------------------------------------------

    /**
     * Constructs a CECalendar based on the current time in the default time zone
     * with the given locale with the Julian epoch offiset
     *
     * @param aLocale  The given locale.
     * @param success  Indicates the status of CECalendar object construction.
     *                 Returns U_ZERO_ERROR if constructed successfully.
     * @internal
     */
    CECalendar(const Locale& aLocale, UErrorCode& success);

    /**
     * Copy Constructor
     * @internal
     */
    CECalendar (const CECalendar& other);

    /**
     * Destructor.
     * @internal
     */
    virtual ~CECalendar();

    /**
     * Default assignment operator
     * @param right    Calendar object to be copied
     * @internal
     */
    CECalendar& operator=(const CECalendar& right);

protected:
    //-------------------------------------------------------------------------
    // Calendar framework
    //-------------------------------------------------------------------------

    /**
     * Return JD of start of given month/extended year
     * @internal
     */
    virtual int32_t handleComputeMonthStart(int32_t eyear, int32_t month, UBool useMonth) const;

    /**
     * Calculate the limit for a specified type of limit and field
     * @internal
     */
    virtual int32_t handleGetLimit(UCalendarDateFields field, ELimitType limitType) const;

    /**
     * (Overrides Calendar) Return true if the current date for this Calendar is in
     * Daylight Savings Time. Recognizes DST_OFFSET, if it is set.
     *
     * @param status Fill-in parameter which receives the status of this operation.
     * @return   True if the current date for this Calendar is in Daylight Savings Time,
     *           false, otherwise.
     * @internal
     */
    virtual UBool inDaylightTime(UErrorCode&) const;

    /**
     * Returns TRUE because Coptic/Ethiopic Calendar does have a default century
     * @internal
     */
    virtual UBool haveDefaultCentury() const;

protected:
    /**
     * The Coptic and Ethiopic calendars differ only in their epochs.
     * This method must be implemented by CECalendar subclasses to
     * return the date offset from Julian
     * @internal
     */
    virtual int32_t getJDEpochOffset() const = 0;

    /**
     * Convert an Coptic/Ethiopic year, month, and day to a Julian day.
     *
     * @param year the extended year
     * @param month the month
     * @param day the day
     * @param jdEpochOffset the epoch offset from Julian epoch
     * @return Julian day
     * @internal
     */
    static int32_t ceToJD(int32_t year, int32_t month, int32_t date,
        int32_t jdEpochOffset);

    /**
     * Convert a Julian day to an Coptic/Ethiopic year, month and day
     *
     * @param julianDay the Julian day
     * @param jdEpochOffset the epoch offset from Julian epoch
     * @param year receives the extended year
     * @param month receives the month
     * @param date receives the day
     * @internal
     */
    static void jdToCE(int32_t julianDay, int32_t jdEpochOffset,
        int32_t& year, int32_t& month, int32_t& day);
};

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */
#endif
//eof
