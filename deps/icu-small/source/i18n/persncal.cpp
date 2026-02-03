// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
 ******************************************************************************
 * Copyright (C) 2003-2013, International Business Machines Corporation
 * and others. All Rights Reserved.
 ******************************************************************************
 *
 * File PERSNCAL.CPP
 *
 * Modification History:
 *
 *   Date        Name        Description
 *   9/23/2003   mehran      posted to icu-design
 *   10/1/2012   roozbeh     Fixed algorithm and heavily refactored and rewrote
 *                           based on the implementation of Gregorian
 *****************************************************************************
 */

#include "persncal.h"

#if !UCONFIG_NO_FORMATTING

#include "uassert.h"
#include "umutex.h"
#include "gregoimp.h" // Math
#include <float.h>
#include "cmemory.h"
#include "ucln_in.h"
#include "unicode/uniset.h"

static const int16_t kPersianNumDays[]
= {0,31,62,93,124,155,186,216,246,276,306,336}; // 0-based, for day-in-year
static const int8_t kPersianMonthLength[]
= {31,31,31,31,31,31,30,30,30,30,30,29}; // 0-based
static const int8_t kPersianLeapMonthLength[]
= {31,31,31,31,31,31,30,30,30,30,30,30}; // 0-based

static const int32_t kPersianCalendarLimits[UCAL_FIELD_COUNT][4] = {
    // Minimum  Greatest     Least   Maximum
    //           Minimum   Maximum
    {        0,        0,        0,        0}, // ERA
    { -5000000, -5000000,  5000000,  5000000}, // YEAR
    {        0,        0,       11,       11}, // MONTH
    {        1,        1,       52,       53}, // WEEK_OF_YEAR
    {/*N/A*/-1,/*N/A*/-1,/*N/A*/-1,/*N/A*/-1}, // WEEK_OF_MONTH
    {        1,       1,        29,       31}, // DAY_OF_MONTH
    {        1,       1,       365,      366}, // DAY_OF_YEAR
    {/*N/A*/-1,/*N/A*/-1,/*N/A*/-1,/*N/A*/-1}, // DAY_OF_WEEK
    {        1,       1,         5,        5}, // DAY_OF_WEEK_IN_MONTH
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
    {        0,        0,       11,       11}, // ORDINAL_MONTH
};

namespace { // anonymous

static const icu::UnicodeSet *gLeapCorrection = nullptr;
static icu::UInitOnce gCorrectionInitOnce {};
static int32_t gMinCorrection;
}  // namespace
U_CDECL_BEGIN
static UBool calendar_persian_cleanup() {
    if (gLeapCorrection) {
        delete gLeapCorrection;
        gLeapCorrection = nullptr;
    }
    gCorrectionInitOnce.reset();
    return true;
}
U_CDECL_END

namespace { // anonymous
static void U_CALLCONV initLeapCorrection() {
    static int16_t nonLeapYears[] = {
       1502, 1601, 1634, 1667, 1700, 1733, 1766, 1799, 1832, 1865, 1898, 1931, 1964, 1997, 2030, 2059,
       2063, 2096, 2129, 2158, 2162, 2191, 2195, 2224, 2228, 2257, 2261, 2290, 2294, 2323, 2327, 2356,
       2360, 2389, 2393, 2422, 2426, 2455, 2459, 2488, 2492, 2521, 2525, 2554, 2558, 2587, 2591, 2620,
       2624, 2653, 2657, 2686, 2690, 2719, 2723, 2748, 2752, 2756, 2781, 2785, 2789, 2818, 2822, 2847,
       2851, 2855, 2880, 2884, 2888, 2913, 2917, 2921, 2946, 2950, 2954, 2979, 2983, 2987,
    };
    gMinCorrection = nonLeapYears[0];
    icu::UnicodeSet prefab;
    for (auto year : nonLeapYears) {
        prefab.add(year);
    }
    gLeapCorrection = prefab.cloneAsThawed()->freeze();
    ucln_i18n_registerCleanup(UCLN_I18N_PERSIAN_CALENDAR, calendar_persian_cleanup);
}
const icu::UnicodeSet* getLeapCorrection() {
    umtx_initOnce(gCorrectionInitOnce, &initLeapCorrection);
    return gLeapCorrection;
}
} // namespace anonymous
U_NAMESPACE_BEGIN

static const int32_t PERSIAN_EPOCH = 1948320;

// Implementation of the PersianCalendar class

//-------------------------------------------------------------------------
// Constructors...
//-------------------------------------------------------------------------

const char *PersianCalendar::getType() const { 
    return "persian";
}

PersianCalendar* PersianCalendar::clone() const {
    return new PersianCalendar(*this);
}

PersianCalendar::PersianCalendar(const Locale& aLocale, UErrorCode& success)
  :   Calendar(TimeZone::forLocaleOrDefault(aLocale), aLocale, success)
{
}

PersianCalendar::PersianCalendar(const PersianCalendar& other) : Calendar(other) {
}

PersianCalendar::~PersianCalendar()
{
}

//-------------------------------------------------------------------------
// Minimum / Maximum access functions
//-------------------------------------------------------------------------


int32_t PersianCalendar::handleGetLimit(UCalendarDateFields field, ELimitType limitType) const {
    return kPersianCalendarLimits[field][limitType];
}

//-------------------------------------------------------------------------
// Assorted calculation utilities
//

/**
 * Determine whether a year is a leap year in the Persian calendar
 */
UBool PersianCalendar::isLeapYear(int32_t year)
{
    if (year >= gMinCorrection && getLeapCorrection()->contains(year)) {
        return false;
    }
    if (year > gMinCorrection && getLeapCorrection()->contains(year-1)) {
        return true;
    }
    int64_t y = static_cast<int64_t>(year) * 25LL + 11LL;
    bool res = (y % 33L < 8);
    return res;
}
    
/**
 * Return the day # on which the given year starts.  Days are counted
 * from the Persian epoch, origin 0.
 */
int32_t PersianCalendar::yearStart(int32_t year, UErrorCode& status) {
    return handleComputeMonthStart(year,0,false, status);
}
    
/**
 * Return the day # on which the given month starts.  Days are counted
 * from the Persian epoch, origin 0.
 *
 * @param year  The Persian year
 * @param year  The Persian month, 0-based
 */
int32_t PersianCalendar::monthStart(int32_t year, int32_t month, UErrorCode& status) const {
    return handleComputeMonthStart(year,month,true, status);
}
    
//----------------------------------------------------------------------
// Calendar framework
//----------------------------------------------------------------------

/**
 * Return the length (in days) of the given month.
 *
 * @param year  The Persian year
 * @param year  The Persian month, 0-based
 */
int32_t PersianCalendar::handleGetMonthLength(int32_t extendedYear, int32_t month, UErrorCode& /*status*/) const {
    // If the month is out of range, adjust it into range, and
    // modify the extended year value accordingly.
    if (month < 0 || month > 11) {
        extendedYear += ClockMath::floorDivide(month, 12, &month);
    }

    return isLeapYear(extendedYear) ? kPersianLeapMonthLength[month] : kPersianMonthLength[month];
}

/**
 * Return the number of days in the given Persian year
 */
int32_t PersianCalendar::handleGetYearLength(int32_t extendedYear, UErrorCode& status) const {
    if (U_FAILURE(status)) return 0;
    return isLeapYear(extendedYear) ? 366 : 365;
}
    
//-------------------------------------------------------------------------
// Functions for converting from field values to milliseconds....
//-------------------------------------------------------------------------

static int64_t firstJulianOfYear(int64_t year) {
    int64_t julianDay = 365LL * (year - 1LL) + ClockMath::floorDivide(8LL * year + 21, 33);
    if (year > gMinCorrection && getLeapCorrection()->contains(year-1)) {
        julianDay--;
    }
    return julianDay;
}


// Return JD of start of given month/year
int64_t PersianCalendar::handleComputeMonthStart(int32_t eyear, int32_t month, UBool /*useMonth*/, UErrorCode& status) const {
    if (U_FAILURE(status)) {
        return 0;
    }
    // If the month is out of range, adjust it into range, and
    // modify the extended year value accordingly.
    if (month < 0 || month > 11) {
        if (uprv_add32_overflow(eyear, ClockMath::floorDivide(month, 12, &month), &eyear)) {
            status = U_ILLEGAL_ARGUMENT_ERROR;
            return 0;
        }
    }

    int64_t julianDay = PERSIAN_EPOCH - 1LL + firstJulianOfYear(eyear);

    if (month != 0) {
        julianDay += kPersianNumDays[month];
    }

    return julianDay;
}

//-------------------------------------------------------------------------
// Functions for converting from milliseconds to field values
//-------------------------------------------------------------------------

int32_t PersianCalendar::handleGetExtendedYear(UErrorCode& status) {
    if (U_FAILURE(status)) {
        return 0;
    }
    if (newerField(UCAL_EXTENDED_YEAR, UCAL_YEAR) == UCAL_EXTENDED_YEAR) {
        return internalGet(UCAL_EXTENDED_YEAR, 1); // Default to year 1
    }
    return internalGet(UCAL_YEAR, 1); // Default to year 1
}

/**
 * Override Calendar to compute several fields specific to the Persian
 * calendar system.  These are:
 *
 * <ul><li>ERA
 * <li>YEAR
 * <li>MONTH
 * <li>DAY_OF_MONTH
 * <li>DAY_OF_YEAR
 * <li>EXTENDED_YEAR</ul>
 * 
 * The DAY_OF_WEEK and DOW_LOCAL fields are already set when this
 * method is called.
 */
void PersianCalendar::handleComputeFields(int32_t julianDay, UErrorCode& status) {
    int64_t daysSinceEpoch = julianDay;
    daysSinceEpoch -= PERSIAN_EPOCH;

    int64_t year = ClockMath::floorDivideInt64(
        33LL * daysSinceEpoch + 3LL, 12053LL) + 1LL;
    if (year > INT32_MAX || year < INT32_MIN) {
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return;
    }

    int64_t farvardin1 = firstJulianOfYear(year);

    int32_t dayOfYear = daysSinceEpoch - farvardin1; // 0-based
    U_ASSERT(dayOfYear >= 0);
    U_ASSERT(dayOfYear < 366);

    if (dayOfYear == 365 && year >= gMinCorrection && getLeapCorrection()->contains(year)) {
        year++;
        dayOfYear = 0;
    }
    int32_t month;
    if (dayOfYear < 216) { // Compute 0-based month
        month = dayOfYear / 31;
    } else {
        month = (dayOfYear - 6) / 30;
    }
    U_ASSERT(month >= 0);
    U_ASSERT(month < 12);

    ++dayOfYear; // Make it 1-based now
    int32_t dayOfMonth = dayOfYear - kPersianNumDays[month];
    U_ASSERT(dayOfMonth > 0);
    U_ASSERT(dayOfMonth <= 31);


    internalSet(UCAL_ERA, 0);
    internalSet(UCAL_YEAR, year);
    internalSet(UCAL_EXTENDED_YEAR, year);
    internalSet(UCAL_MONTH, month);
    internalSet(UCAL_ORDINAL_MONTH, month);
    internalSet(UCAL_DAY_OF_MONTH, dayOfMonth);
    internalSet(UCAL_DAY_OF_YEAR, dayOfYear);
}    

IMPL_SYSTEM_DEFAULT_CENTURY(PersianCalendar, "@calendar=persian")

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(PersianCalendar)
int32_t
PersianCalendar::getRelatedYearDifference() const {
    constexpr int32_t kPersianCalendarRelatedYearDifference = 622;
    return kPersianCalendarRelatedYearDifference;
}

U_NAMESPACE_END

#endif
