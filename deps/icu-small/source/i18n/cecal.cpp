// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 2003 - 2009, International Business Machines Corporation and  *
* others. All Rights Reserved.                                                *
*******************************************************************************
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "cecal.h"
#include "gregoimp.h"   //Math

U_NAMESPACE_BEGIN

static const int32_t LIMITS[UCAL_FIELD_COUNT][4] = {
    // Minimum  Greatest    Least  Maximum
    //           Minimum  Maximum
    {        0,        0,        1,        1}, // ERA
    {        1,        1,  5000000,  5000000}, // YEAR
    {        0,        0,       12,       12}, // MONTH
    {        1,        1,       52,       53}, // WEEK_OF_YEAR
    {/*N/A*/-1,/*N/A*/-1,/*N/A*/-1,/*N/A*/-1}, // WEEK_OF_MONTH
    {        1,        1,        5,       30}, // DAY_OF_MONTH
    {        1,        1,      365,      366}, // DAY_OF_YEAR
    {/*N/A*/-1,/*N/A*/-1,/*N/A*/-1,/*N/A*/-1}, // DAY_OF_WEEK
    {       -1,       -1,        1,        5}, // DAY_OF_WEEK_IN_MONTH
    {/*N/A*/-1,/*N/A*/-1,/*N/A*/-1,/*N/A*/-1}, // AM_PM
    {/*N/A*/-1,/*N/A*/-1,/*N/A*/-1,/*N/A*/-1}, // HOUR
    {/*N/A*/-1,/*N/A*/-1,/*N/A*/-1,/*N/A*/-1}, // HOUR_OF_DAY
    {/*N/A*/-1,/*N/A*/-1,/*N/A*/-1,/*N/A*/-1}, // MINUTE
    {/*N/A*/-1,/*N/A*/-1,/*N/A*/-1,/*N/A*/-1}, // SECOND
    {/*N/A*/-1,/*N/A*/-1,/*N/A*/-1,/*N/A*/-1}, // MILLISECOND
    {/*N/A*/-1,/*N/A*/-1,/*N/A*/-1,/*N/A*/-1}, // ZONE_OFFSET
    {/*N/A*/-1,/*N/A*/-1,/*N/A*/-1,/*N/A*/-1}, // DST_OFFSET
    { -5000000, -5000000,  5000000,  5000000}, // YEAR_WOY
    {/*N/A*/-1,/*N/A*/-1,/*N/A*/-1,/*N/A*/-1}, // DOW_LOCAL
    { -5000000, -5000000,  5000000,  5000000}, // EXTENDED_YEAR
    {/*N/A*/-1,/*N/A*/-1,/*N/A*/-1,/*N/A*/-1}, // JULIAN_DAY
    {/*N/A*/-1,/*N/A*/-1,/*N/A*/-1,/*N/A*/-1}, // MILLISECONDS_IN_DAY
    {/*N/A*/-1,/*N/A*/-1,/*N/A*/-1,/*N/A*/-1}, // IS_LEAP_MONTH
};

//-------------------------------------------------------------------------
// Constructors...
//-------------------------------------------------------------------------

CECalendar::CECalendar(const Locale& aLocale, UErrorCode& success)
:   Calendar(TimeZone::createDefault(), aLocale, success)
{
    setTimeInMillis(getNow(), success);
}

CECalendar::CECalendar (const CECalendar& other) 
:   Calendar(other)
{
}

CECalendar::~CECalendar()
{
}

CECalendar&
CECalendar::operator=(const CECalendar& right)
{
    Calendar::operator=(right);
    return *this;
}

//-------------------------------------------------------------------------
// Calendar framework
//-------------------------------------------------------------------------

int32_t
CECalendar::handleComputeMonthStart(int32_t eyear,int32_t emonth, UBool /*useMonth*/) const
{
    return ceToJD(eyear, emonth, 0, getJDEpochOffset());
}

int32_t
CECalendar::handleGetLimit(UCalendarDateFields field, ELimitType limitType) const
{
    return LIMITS[field][limitType];
}

UBool
CECalendar::inDaylightTime(UErrorCode& status) const
{
    if (U_FAILURE(status) || !getTimeZone().useDaylightTime()) {
        return FALSE;
    }

    // Force an update of the state of the Calendar.
    ((CECalendar*)this)->complete(status); // cast away const

    return (UBool)(U_SUCCESS(status) ? (internalGet(UCAL_DST_OFFSET) != 0) : FALSE);
}

UBool
CECalendar::haveDefaultCentury() const
{
    return TRUE;
}

//-------------------------------------------------------------------------
// Calendar system Conversion methods...
//-------------------------------------------------------------------------
int32_t
CECalendar::ceToJD(int32_t year, int32_t month, int32_t date, int32_t jdEpochOffset)
{
    // handle month > 12, < 0 (e.g. from add/set)
    if ( month >= 0 ) {
        year += month/13;
        month %= 13;
    } else {
        ++month;
        year += month/13 - 1;
        month = month%13 + 12;
    }
    return (int32_t) (
        jdEpochOffset                   // difference from Julian epoch to 1,1,1
        + 365 * year                    // number of days from years
        + ClockMath::floorDivide(year, 4)    // extra day of leap year
        + 30 * month                    // number of days from months (months are 0-based)
        + date - 1                      // number of days for present month (1 based)
        );
}

void
CECalendar::jdToCE(int32_t julianDay, int32_t jdEpochOffset, int32_t& year, int32_t& month, int32_t& day)
{
    int32_t c4; // number of 4 year cycle (1461 days)
    int32_t r4; // remainder of 4 year cycle, always positive

    c4 = ClockMath::floorDivide(julianDay - jdEpochOffset, 1461, r4);

    year = 4 * c4 + (r4/365 - r4/1460); // 4 * <number of 4year cycle> + <years within the last cycle>

    int32_t doy = (r4 == 1460) ? 365 : (r4 % 365); // days in present year

    month = doy / 30;       // 30 -> Coptic/Ethiopic month length up to 12th month
    day = (doy % 30) + 1;   // 1-based days in a month
}

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_FORMATTING */
//eof
