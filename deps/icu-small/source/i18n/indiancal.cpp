// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
 * Copyright (C) 2003-2014, International Business Machines Corporation
 * and others. All Rights Reserved.
 ******************************************************************************
 *
 * File INDIANCAL.CPP
 *****************************************************************************
 */

#include "indiancal.h"
#include <stdlib.h>
#if !UCONFIG_NO_FORMATTING

#include "mutex.h"
#include <float.h>
#include "gregoimp.h" // Math
#include "astro.h" // CalendarAstronomer
#include "uhash.h"

// Debugging
#ifdef U_DEBUG_INDIANCAL
#include <stdio.h>
#include <stdarg.h>

#endif

U_NAMESPACE_BEGIN

// Implementation of the IndianCalendar class

//-------------------------------------------------------------------------
// Constructors...
//-------------------------------------------------------------------------


Calendar* IndianCalendar::clone() const {
  return new IndianCalendar(*this);
}

IndianCalendar::IndianCalendar(const Locale& aLocale, UErrorCode& success)
  :   Calendar(TimeZone::createDefault(), aLocale, success)
{
  setTimeInMillis(getNow(), success); // Call this again now that the vtable is set up properly.
}

IndianCalendar::IndianCalendar(const IndianCalendar& other) : Calendar(other) {
}

IndianCalendar::~IndianCalendar()
{
}
const char *IndianCalendar::getType() const {
   return "indian";
}

static const int32_t LIMITS[UCAL_FIELD_COUNT][4] = {
    // Minimum  Greatest     Least   Maximum
    //           Minimum   Maximum
    {        0,        0,        0,        0}, // ERA
    { -5000000, -5000000,  5000000,  5000000}, // YEAR
    {        0,        0,       11,       11}, // MONTH
    {        1,        1,       52,       53}, // WEEK_OF_YEAR
    {/*N/A*/-1,/*N/A*/-1,/*N/A*/-1,/*N/A*/-1}, // WEEK_OF_MONTH
    {        1,        1,       30,       31}, // DAY_OF_MONTH
    {        1,        1,      365,      366}, // DAY_OF_YEAR
    {/*N/A*/-1,/*N/A*/-1,/*N/A*/-1,/*N/A*/-1}, // DAY_OF_WEEK
    {       -1,       -1,        5,        5}, // DAY_OF_WEEK_IN_MONTH
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

static const double JULIAN_EPOCH = 1721425.5;
static const int32_t INDIAN_ERA_START  = 78;
static const int32_t INDIAN_YEAR_START = 80;

int32_t IndianCalendar::handleGetLimit(UCalendarDateFields field, ELimitType limitType) const {
  return LIMITS[field][limitType];
}

/*
 * Determine whether the given gregorian year is a Leap year
 */
static UBool isGregorianLeap(int32_t year)
{
    return ((year % 4) == 0) && (!(((year % 100) == 0) && ((year % 400) != 0)));
}

//----------------------------------------------------------------------
// Calendar framework
//----------------------------------------------------------------------

/*
 * Return the length (in days) of the given month.
 *
 * @param eyear  The year in Saka Era
 * @param month  The month(0-based) in Indian calendar
 */
int32_t IndianCalendar::handleGetMonthLength(int32_t eyear, int32_t month) const {
   if (month < 0 || month > 11) {
      eyear += ClockMath::floorDivide(month, 12, month);
   }

   if (isGregorianLeap(eyear + INDIAN_ERA_START) && month == 0) {
       return 31;
   }

   if (month >= 1 && month <= 5) {
       return 31;
   }

   return 30;
}

/*
 * Return the number of days in the given Indian year
 *
 * @param eyear The year in Saka Era.
 */
int32_t IndianCalendar::handleGetYearLength(int32_t eyear) const {
    return isGregorianLeap(eyear + INDIAN_ERA_START) ? 366 : 365;
}
/*
 * Returns the Julian Day corresponding to gregorian date
 *
 * @param year The Gregorian year
 * @param month The month in Gregorian Year
 * @param date The date in Gregorian day in month
 */
static double gregorianToJD(int32_t year, int32_t month, int32_t date) {
   double julianDay = (JULIAN_EPOCH - 1) +
      (365 * (year - 1)) +
      uprv_floor((year - 1) / 4) +
      (-uprv_floor((year - 1) / 100)) +
      uprv_floor((year - 1) / 400) +
      uprv_floor((((367 * month) - 362) / 12) +
            ((month <= 2) ? 0 :
             (isGregorianLeap(year) ? -1 : -2)
            ) +
            date);

   return julianDay;
}

/*
 * Returns the Gregorian Date corresponding to a given Julian Day
 * @param jd The Julian Day
 */
static int32_t* jdToGregorian(double jd, int32_t gregorianDate[3]) {
   double wjd, depoch, quadricent, dqc, cent, dcent, quad, dquad, yindex, yearday, leapadj;
   int32_t year, month, day;
   wjd = uprv_floor(jd - 0.5) + 0.5;
   depoch = wjd - JULIAN_EPOCH;
   quadricent = uprv_floor(depoch / 146097);
   dqc = (int32_t)uprv_floor(depoch) % 146097;
   cent = uprv_floor(dqc / 36524);
   dcent = (int32_t)uprv_floor(dqc) % 36524;
   quad = uprv_floor(dcent / 1461);
   dquad = (int32_t)uprv_floor(dcent) % 1461;
   yindex = uprv_floor(dquad / 365);
   year = (int32_t)((quadricent * 400) + (cent * 100) + (quad * 4) + yindex);
   if (!((cent == 4) || (yindex == 4))) {
      year++;
   }
   yearday = wjd - gregorianToJD(year, 1, 1);
   leapadj = ((wjd < gregorianToJD(year, 3, 1)) ? 0
         :
         (isGregorianLeap(year) ? 1 : 2)
         );
   month = (int32_t)uprv_floor((((yearday + leapadj) * 12) + 373) / 367);
   day = (int32_t)(wjd - gregorianToJD(year, month, 1)) + 1;

   gregorianDate[0] = year;
   gregorianDate[1] = month;
   gregorianDate[2] = day;

   return gregorianDate;
}


//-------------------------------------------------------------------------
// Functions for converting from field values to milliseconds....
//-------------------------------------------------------------------------
static double IndianToJD(int32_t year, int32_t month, int32_t date) {
   int32_t leapMonth, gyear, m;
   double start, jd;

   gyear = year + INDIAN_ERA_START;


   if(isGregorianLeap(gyear)) {
      leapMonth = 31;
      start = gregorianToJD(gyear, 3, 21);
   }
   else {
      leapMonth = 30;
      start = gregorianToJD(gyear, 3, 22);
   }

   if (month == 1) {
      jd = start + (date - 1);
   } else {
      jd = start + leapMonth;
      m = month - 2;

      //m = Math.min(m, 5);
      if (m > 5) {
          m = 5;
      }

      jd += m * 31;

      if (month >= 8) {
         m = month - 7;
         jd += m * 30;
      }
      jd += date - 1;
   }

   return jd;
}

/*
 * Return JD of start of given month/year of Indian Calendar
 * @param eyear The year in Indian Calendar measured from Saka Era (78 AD).
 * @param month The month in Indian calendar
 */
int32_t IndianCalendar::handleComputeMonthStart(int32_t eyear, int32_t month, UBool /* useMonth */ ) const {

   //month is 0 based; converting it to 1-based
   int32_t imonth;

    // If the month is out of range, adjust it into range, and adjust the extended eyar accordingly
   if (month < 0 || month > 11) {
      eyear += (int32_t)ClockMath::floorDivide(month, 12, month);
   }

   if(month == 12){
       imonth = 1;
   } else {
       imonth = month + 1;
   }

   double jd = IndianToJD(eyear ,imonth, 1);

   return (int32_t)jd;
}

//-------------------------------------------------------------------------
// Functions for converting from milliseconds to field values
//-------------------------------------------------------------------------

int32_t IndianCalendar::handleGetExtendedYear() {
    int32_t year;

    if (newerField(UCAL_EXTENDED_YEAR, UCAL_YEAR) == UCAL_EXTENDED_YEAR) {
        year = internalGet(UCAL_EXTENDED_YEAR, 1); // Default to year 1
    } else {
        year = internalGet(UCAL_YEAR, 1); // Default to year 1
    }

    return year;
}

/*
 * Override Calendar to compute several fields specific to the Indian
 * calendar system.  These are:
 *
 * <ul><li>ERA
 * <li>YEAR
 * <li>MONTH
 * <li>DAY_OF_MONTH
 * <li>EXTENDED_YEAR</ul>
 *
 * The DAY_OF_WEEK and DOW_LOCAL fields are already set when this
 * method is called. The getGregorianXxx() methods return Gregorian
 * calendar equivalents for the given Julian day.
 */
void IndianCalendar::handleComputeFields(int32_t julianDay, UErrorCode&  /* status */) {
    double jdAtStartOfGregYear;
    int32_t leapMonth, IndianYear, yday, IndianMonth, IndianDayOfMonth, mday;
    int32_t gregorianYear;      // Stores gregorian date corresponding to Julian day;
    int32_t gd[3];

    gregorianYear = jdToGregorian(julianDay, gd)[0];          // Gregorian date for Julian day
    IndianYear = gregorianYear - INDIAN_ERA_START;            // Year in Saka era
    jdAtStartOfGregYear = gregorianToJD(gregorianYear, 1, 1); // JD at start of Gregorian year
    yday = (int32_t)(julianDay - jdAtStartOfGregYear);        // Day number in Gregorian year (starting from 0)

    if (yday < INDIAN_YEAR_START) {
        // Day is at the end of the preceding Saka year
        IndianYear -= 1;
        leapMonth = isGregorianLeap(gregorianYear - 1) ? 31 : 30; // Days in leapMonth this year, previous Gregorian year
        yday += leapMonth + (31 * 5) + (30 * 3) + 10;
    } else {
        leapMonth = isGregorianLeap(gregorianYear) ? 31 : 30; // Days in leapMonth this year
        yday -= INDIAN_YEAR_START;
    }

    if (yday < leapMonth) {
        IndianMonth = 0;
        IndianDayOfMonth = yday + 1;
    } else {
        mday = yday - leapMonth;
        if (mday < (31 * 5)) {
            IndianMonth = (int32_t)uprv_floor(mday / 31) + 1;
            IndianDayOfMonth = (mday % 31) + 1;
        } else {
            mday -= 31 * 5;
            IndianMonth = (int32_t)uprv_floor(mday / 30) + 6;
            IndianDayOfMonth = (mday % 30) + 1;
        }
   }

   internalSet(UCAL_ERA, 0);
   internalSet(UCAL_EXTENDED_YEAR, IndianYear);
   internalSet(UCAL_YEAR, IndianYear);
   internalSet(UCAL_MONTH, IndianMonth);
   internalSet(UCAL_DAY_OF_MONTH, IndianDayOfMonth);
   internalSet(UCAL_DAY_OF_YEAR, yday + 1); // yday is 0-based
}

UBool
IndianCalendar::inDaylightTime(UErrorCode& status) const
{
    // copied from GregorianCalendar
    if (U_FAILURE(status) || !getTimeZone().useDaylightTime()) {
        return FALSE;
    }

    // Force an update of the state of the Calendar.
    ((IndianCalendar*)this)->complete(status); // cast away const

    return (UBool)(U_SUCCESS(status) ? (internalGet(UCAL_DST_OFFSET) != 0) : FALSE);
}

// default century
const UDate     IndianCalendar::fgSystemDefaultCentury          = DBL_MIN;
const int32_t   IndianCalendar::fgSystemDefaultCenturyYear      = -1;

UDate           IndianCalendar::fgSystemDefaultCenturyStart     = DBL_MIN;
int32_t         IndianCalendar::fgSystemDefaultCenturyStartYear = -1;


UBool IndianCalendar::haveDefaultCentury() const
{
    return TRUE;
}

UDate IndianCalendar::defaultCenturyStart() const
{
    return internalGetDefaultCenturyStart();
}

int32_t IndianCalendar::defaultCenturyStartYear() const
{
    return internalGetDefaultCenturyStartYear();
}

UDate
IndianCalendar::internalGetDefaultCenturyStart() const
{
    // lazy-evaluate systemDefaultCenturyStart
    UBool needsUpdate;
    {
        Mutex m;
        needsUpdate = (fgSystemDefaultCenturyStart == fgSystemDefaultCentury);
    }

    if (needsUpdate) {
        initializeSystemDefaultCentury();
    }

    // use defaultCenturyStart unless it's the flag value;
    // then use systemDefaultCenturyStart

    return fgSystemDefaultCenturyStart;
}

int32_t
IndianCalendar::internalGetDefaultCenturyStartYear() const
{
    // lazy-evaluate systemDefaultCenturyStartYear
    UBool needsUpdate;
    {
        Mutex m;

        needsUpdate = (fgSystemDefaultCenturyStart == fgSystemDefaultCentury);
    }

    if (needsUpdate) {
        initializeSystemDefaultCentury();
    }

    // use defaultCenturyStart unless it's the flag value;
    // then use systemDefaultCenturyStartYear

    return    fgSystemDefaultCenturyStartYear;
}

void
IndianCalendar::initializeSystemDefaultCentury()
{
    // initialize systemDefaultCentury and systemDefaultCenturyYear based
    // on the current time.  They'll be set to 80 years before
    // the current time.
    // No point in locking as it should be idempotent.
    if (fgSystemDefaultCenturyStart == fgSystemDefaultCentury) {
        UErrorCode status = U_ZERO_ERROR;

        IndianCalendar calendar(Locale("@calendar=Indian"),status);
        if (U_SUCCESS(status)) {
            calendar.setTime(Calendar::getNow(), status);
            calendar.add(UCAL_YEAR, -80, status);

            UDate    newStart = calendar.getTime(status);
            int32_t  newYear  = calendar.get(UCAL_YEAR, status);

            {
                Mutex m;

                fgSystemDefaultCenturyStart = newStart;
                fgSystemDefaultCenturyStartYear = newYear;
            }
        }

        // We have no recourse upon failure unless we want to propagate the failure
        // out.
    }
}

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(IndianCalendar)

U_NAMESPACE_END

#endif
