/*
 ******************************************************************************
 * Copyright (C) 2007-2014, International Business Machines Corporation
 * and others. All Rights Reserved.
 ******************************************************************************
 *
 * File CHNSECAL.CPP
 *
 * Modification History:
 *
 *   Date        Name        Description
 *   9/18/2007  ajmacher         ported from java ChineseCalendar
 *****************************************************************************
 */

#include "chnsecal.h"

#if !UCONFIG_NO_FORMATTING

#include "umutex.h"
#include <float.h>
#include "gregoimp.h" // Math
#include "astro.h" // CalendarAstronomer
#include "unicode/simpletz.h"
#include "uhash.h"
#include "ucln_in.h"

// Debugging
#ifdef U_DEBUG_CHNSECAL
# include <stdio.h>
# include <stdarg.h>
static void debug_chnsecal_loc(const char *f, int32_t l)
{
    fprintf(stderr, "%s:%d: ", f, l);
}

static void debug_chnsecal_msg(const char *pat, ...)
{
    va_list ap;
    va_start(ap, pat);
    vfprintf(stderr, pat, ap);
    fflush(stderr);
}
// must use double parens, i.e.:  U_DEBUG_CHNSECAL_MSG(("four is: %d",4));
#define U_DEBUG_CHNSECAL_MSG(x) {debug_chnsecal_loc(__FILE__,__LINE__);debug_chnsecal_msg x;}
#else
#define U_DEBUG_CHNSECAL_MSG(x)
#endif


// --- The cache --
static UMutex astroLock = U_MUTEX_INITIALIZER;  // Protects access to gChineseCalendarAstro.
static icu::CalendarAstronomer *gChineseCalendarAstro = NULL;

// Lazy Creation & Access synchronized by class CalendarCache with a mutex.
static icu::CalendarCache *gChineseCalendarWinterSolsticeCache = NULL;
static icu::CalendarCache *gChineseCalendarNewYearCache = NULL;

static icu::TimeZone *gChineseCalendarZoneAstroCalc = NULL;
static icu::UInitOnce gChineseCalendarZoneAstroCalcInitOnce = U_INITONCE_INITIALIZER;

/**
 * The start year of the Chinese calendar, the 61st year of the reign
 * of Huang Di.  Some sources use the first year of his reign,
 * resulting in EXTENDED_YEAR values 60 years greater and ERA (cycle)
 * values one greater.
 */
static const int32_t CHINESE_EPOCH_YEAR = -2636; // Gregorian year

/**
 * The offset from GMT in milliseconds at which we perform astronomical
 * computations.  Some sources use a different historically accurate
 * offset of GMT+7:45:40 for years before 1929; we do not do this.
 */
static const int32_t CHINA_OFFSET = 8 * kOneHour;

/**
 * Value to be added or subtracted from the local days of a new moon to
 * get close to the next or prior new moon, but not cross it.  Must be
 * >= 1 and < CalendarAstronomer.SYNODIC_MONTH.
 */
static const int32_t SYNODIC_GAP = 25;


U_CDECL_BEGIN
static UBool calendar_chinese_cleanup(void) {
    if (gChineseCalendarAstro) {
        delete gChineseCalendarAstro;
        gChineseCalendarAstro = NULL;
    }
    if (gChineseCalendarWinterSolsticeCache) {
        delete gChineseCalendarWinterSolsticeCache;
        gChineseCalendarWinterSolsticeCache = NULL;
    }
    if (gChineseCalendarNewYearCache) {
        delete gChineseCalendarNewYearCache;
        gChineseCalendarNewYearCache = NULL;
    }
    if (gChineseCalendarZoneAstroCalc) {
        delete gChineseCalendarZoneAstroCalc;
        gChineseCalendarZoneAstroCalc = NULL;
    }
    gChineseCalendarZoneAstroCalcInitOnce.reset();
    return TRUE;
}
U_CDECL_END

U_NAMESPACE_BEGIN


// Implementation of the ChineseCalendar class


//-------------------------------------------------------------------------
// Constructors...
//-------------------------------------------------------------------------


Calendar* ChineseCalendar::clone() const {
    return new ChineseCalendar(*this);
}

ChineseCalendar::ChineseCalendar(const Locale& aLocale, UErrorCode& success)
:   Calendar(TimeZone::createDefault(), aLocale, success),
    isLeapYear(FALSE),
    fEpochYear(CHINESE_EPOCH_YEAR),
    fZoneAstroCalc(getChineseCalZoneAstroCalc())
{
    setTimeInMillis(getNow(), success); // Call this again now that the vtable is set up properly.
}

ChineseCalendar::ChineseCalendar(const Locale& aLocale, int32_t epochYear,
                                const TimeZone* zoneAstroCalc, UErrorCode &success)
:   Calendar(TimeZone::createDefault(), aLocale, success),
    isLeapYear(FALSE),
    fEpochYear(epochYear),
    fZoneAstroCalc(zoneAstroCalc)
{
    setTimeInMillis(getNow(), success); // Call this again now that the vtable is set up properly.
}

ChineseCalendar::ChineseCalendar(const ChineseCalendar& other) : Calendar(other) {
    isLeapYear = other.isLeapYear;
    fEpochYear = other.fEpochYear;
    fZoneAstroCalc = other.fZoneAstroCalc;
}

ChineseCalendar::~ChineseCalendar()
{
}

const char *ChineseCalendar::getType() const { 
    return "chinese";
}

static void U_CALLCONV initChineseCalZoneAstroCalc() {
    gChineseCalendarZoneAstroCalc = new SimpleTimeZone(CHINA_OFFSET, UNICODE_STRING_SIMPLE("CHINA_ZONE") );
    ucln_i18n_registerCleanup(UCLN_I18N_CHINESE_CALENDAR, calendar_chinese_cleanup);
}

const TimeZone* ChineseCalendar::getChineseCalZoneAstroCalc(void) const {
    umtx_initOnce(gChineseCalendarZoneAstroCalcInitOnce, &initChineseCalZoneAstroCalc);
    return gChineseCalendarZoneAstroCalc;
}

//-------------------------------------------------------------------------
// Minimum / Maximum access functions
//-------------------------------------------------------------------------


static const int32_t LIMITS[UCAL_FIELD_COUNT][4] = {
    // Minimum  Greatest     Least    Maximum
    //           Minimum   Maximum
    {        1,        1,    83333,    83333}, // ERA
    {        1,        1,       60,       60}, // YEAR
    {        0,        0,       11,       11}, // MONTH
    {        1,        1,       50,       55}, // WEEK_OF_YEAR
    {/*N/A*/-1,/*N/A*/-1,/*N/A*/-1,/*N/A*/-1}, // WEEK_OF_MONTH
    {        1,        1,       29,       30}, // DAY_OF_MONTH
    {        1,        1,      353,      385}, // DAY_OF_YEAR
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
    {        0,        0,        1,        1}, // IS_LEAP_MONTH
};


/**
* @draft ICU 2.4
*/
int32_t ChineseCalendar::handleGetLimit(UCalendarDateFields field, ELimitType limitType) const {
    return LIMITS[field][limitType];
}


//----------------------------------------------------------------------
// Calendar framework
//----------------------------------------------------------------------

/**
 * Implement abstract Calendar method to return the extended year
 * defined by the current fields.  This will use either the ERA and
 * YEAR field as the cycle and year-of-cycle, or the EXTENDED_YEAR
 * field as the continuous year count, depending on which is newer.
 * @stable ICU 2.8
 */
int32_t ChineseCalendar::handleGetExtendedYear() {
    int32_t year;
    if (newestStamp(UCAL_ERA, UCAL_YEAR, kUnset) <= fStamp[UCAL_EXTENDED_YEAR]) {
        year = internalGet(UCAL_EXTENDED_YEAR, 1); // Default to year 1
    } else {
        int32_t cycle = internalGet(UCAL_ERA, 1) - 1; // 0-based cycle
        // adjust to the instance specific epoch
        year = cycle * 60 + internalGet(UCAL_YEAR, 1) - (fEpochYear - CHINESE_EPOCH_YEAR);
    }
    return year;
}

/**
 * Override Calendar method to return the number of days in the given
 * extended year and month.
 *
 * <p>Note: This method also reads the IS_LEAP_MONTH field to determine
 * whether or not the given month is a leap month.
 * @stable ICU 2.8
 */
int32_t ChineseCalendar::handleGetMonthLength(int32_t extendedYear, int32_t month) const {
    int32_t thisStart = handleComputeMonthStart(extendedYear, month, TRUE) -
        kEpochStartAsJulianDay + 1; // Julian day -> local days
    int32_t nextStart = newMoonNear(thisStart + SYNODIC_GAP, TRUE);
    return nextStart - thisStart;
}

/**
 * Override Calendar to compute several fields specific to the Chinese
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
 * method is called.  The getGregorianXxx() methods return Gregorian
 * calendar equivalents for the given Julian day.
 *
 * <p>Compute the ChineseCalendar-specific field IS_LEAP_MONTH.
 * @stable ICU 2.8
 */
void ChineseCalendar::handleComputeFields(int32_t julianDay, UErrorCode &/*status*/) {

    computeChineseFields(julianDay - kEpochStartAsJulianDay, // local days
                         getGregorianYear(), getGregorianMonth(),
                         TRUE); // set all fields
}

/**
 * Field resolution table that incorporates IS_LEAP_MONTH.
 */
const UFieldResolutionTable ChineseCalendar::CHINESE_DATE_PRECEDENCE[] =
{
    {
        { UCAL_DAY_OF_MONTH, kResolveSTOP },
        { UCAL_WEEK_OF_YEAR, UCAL_DAY_OF_WEEK, kResolveSTOP },
        { UCAL_WEEK_OF_MONTH, UCAL_DAY_OF_WEEK, kResolveSTOP },
        { UCAL_DAY_OF_WEEK_IN_MONTH, UCAL_DAY_OF_WEEK, kResolveSTOP },
        { UCAL_WEEK_OF_YEAR, UCAL_DOW_LOCAL, kResolveSTOP },
        { UCAL_WEEK_OF_MONTH, UCAL_DOW_LOCAL, kResolveSTOP },
        { UCAL_DAY_OF_WEEK_IN_MONTH, UCAL_DOW_LOCAL, kResolveSTOP },
        { UCAL_DAY_OF_YEAR, kResolveSTOP },
        { kResolveRemap | UCAL_DAY_OF_MONTH, UCAL_IS_LEAP_MONTH, kResolveSTOP },
        { kResolveSTOP }
    },
    {
        { UCAL_WEEK_OF_YEAR, kResolveSTOP },
        { UCAL_WEEK_OF_MONTH, kResolveSTOP },
        { UCAL_DAY_OF_WEEK_IN_MONTH, kResolveSTOP },
        { kResolveRemap | UCAL_DAY_OF_WEEK_IN_MONTH, UCAL_DAY_OF_WEEK, kResolveSTOP },
        { kResolveRemap | UCAL_DAY_OF_WEEK_IN_MONTH, UCAL_DOW_LOCAL, kResolveSTOP },
        { kResolveSTOP }
    },
    {{kResolveSTOP}}
};

/**
 * Override Calendar to add IS_LEAP_MONTH to the field resolution
 * table.
 * @stable ICU 2.8
 */
const UFieldResolutionTable* ChineseCalendar::getFieldResolutionTable() const {
    return CHINESE_DATE_PRECEDENCE;
}

/**
 * Return the Julian day number of day before the first day of the
 * given month in the given extended year.
 * 
 * <p>Note: This method reads the IS_LEAP_MONTH field to determine
 * whether the given month is a leap month.
 * @param eyear the extended year
 * @param month the zero-based month.  The month is also determined
 * by reading the IS_LEAP_MONTH field.
 * @return the Julian day number of the day before the first
 * day of the given month and year
 * @stable ICU 2.8
 */
int32_t ChineseCalendar::handleComputeMonthStart(int32_t eyear, int32_t month, UBool useMonth) const {

    ChineseCalendar *nonConstThis = (ChineseCalendar*)this; // cast away const

    // If the month is out of range, adjust it into range, and
    // modify the extended year value accordingly.
    if (month < 0 || month > 11) {
        double m = month;
        eyear += (int32_t)ClockMath::floorDivide(m, 12.0, m);
        month = (int32_t)m;
    }

    int32_t gyear = eyear + fEpochYear - 1; // Gregorian year
    int32_t theNewYear = newYear(gyear);
    int32_t newMoon = newMoonNear(theNewYear + month * 29, TRUE);
    
    int32_t julianDay = newMoon + kEpochStartAsJulianDay;

    // Save fields for later restoration
    int32_t saveMonth = internalGet(UCAL_MONTH);
    int32_t saveIsLeapMonth = internalGet(UCAL_IS_LEAP_MONTH);

    // Ignore IS_LEAP_MONTH field if useMonth is false
    int32_t isLeapMonth = useMonth ? saveIsLeapMonth : 0;

    UErrorCode status = U_ZERO_ERROR;
    nonConstThis->computeGregorianFields(julianDay, status);
    if (U_FAILURE(status))
        return 0;
    
    // This will modify the MONTH and IS_LEAP_MONTH fields (only)
    nonConstThis->computeChineseFields(newMoon, getGregorianYear(),
                         getGregorianMonth(), FALSE);        

    if (month != internalGet(UCAL_MONTH) ||
        isLeapMonth != internalGet(UCAL_IS_LEAP_MONTH)) {
        newMoon = newMoonNear(newMoon + SYNODIC_GAP, TRUE);
        julianDay = newMoon + kEpochStartAsJulianDay;
    }

    nonConstThis->internalSet(UCAL_MONTH, saveMonth);
    nonConstThis->internalSet(UCAL_IS_LEAP_MONTH, saveIsLeapMonth);

    return julianDay - 1;
}


/**
 * Override Calendar to handle leap months properly.
 * @stable ICU 2.8
 */
void ChineseCalendar::add(UCalendarDateFields field, int32_t amount, UErrorCode& status) {
    switch (field) {
    case UCAL_MONTH:
        if (amount != 0) {
            int32_t dom = get(UCAL_DAY_OF_MONTH, status);
            if (U_FAILURE(status)) break;
            int32_t day = get(UCAL_JULIAN_DAY, status) - kEpochStartAsJulianDay; // Get local day
            if (U_FAILURE(status)) break;
            int32_t moon = day - dom + 1; // New moon 
            offsetMonth(moon, dom, amount);
        }
        break;
    default:
        Calendar::add(field, amount, status);
        break;
    }
}

/**
 * Override Calendar to handle leap months properly.
 * @stable ICU 2.8
 */
void ChineseCalendar::add(EDateFields field, int32_t amount, UErrorCode& status) {
    add((UCalendarDateFields)field, amount, status);
}

/**
 * Override Calendar to handle leap months properly.
 * @stable ICU 2.8
 */
void ChineseCalendar::roll(UCalendarDateFields field, int32_t amount, UErrorCode& status) {
    switch (field) {
    case UCAL_MONTH:
        if (amount != 0) {
            int32_t dom = get(UCAL_DAY_OF_MONTH, status);
            if (U_FAILURE(status)) break;
            int32_t day = get(UCAL_JULIAN_DAY, status) - kEpochStartAsJulianDay; // Get local day
            if (U_FAILURE(status)) break;
            int32_t moon = day - dom + 1; // New moon (start of this month)

            // Note throughout the following:  Months 12 and 1 are never
            // followed by a leap month (D&R p. 185).

            // Compute the adjusted month number m.  This is zero-based
            // value from 0..11 in a non-leap year, and from 0..12 in a
            // leap year.
            int32_t m = get(UCAL_MONTH, status); // 0-based month
            if (U_FAILURE(status)) break;
            if (isLeapYear) { // (member variable)
                if (get(UCAL_IS_LEAP_MONTH, status) == 1) {
                    ++m;
                } else {
                    // Check for a prior leap month.  (In the
                    // following, month 0 is the first month of the
                    // year.)  Month 0 is never followed by a leap
                    // month, and we know month m is not a leap month.
                    // moon1 will be the start of month 0 if there is
                    // no leap month between month 0 and month m;
                    // otherwise it will be the start of month 1.
                    int moon1 = moon -
                        (int) (CalendarAstronomer::SYNODIC_MONTH * (m - 0.5));
                    moon1 = newMoonNear(moon1, TRUE);
                    if (isLeapMonthBetween(moon1, moon)) {
                        ++m;
                    }
                }
                if (U_FAILURE(status)) break;
            }

            // Now do the standard roll computation on m, with the
            // allowed range of 0..n-1, where n is 12 or 13.
            int32_t n = isLeapYear ? 13 : 12; // Months in this year
            int32_t newM = (m + amount) % n;
            if (newM < 0) {
                newM += n;
            }

            if (newM != m) {
                offsetMonth(moon, dom, newM - m);
            }
        }
        break;
    default:
        Calendar::roll(field, amount, status);
        break;
    }
}

void ChineseCalendar::roll(EDateFields field, int32_t amount, UErrorCode& status) {
    roll((UCalendarDateFields)field, amount, status);
}


//------------------------------------------------------------------
// Support methods and constants
//------------------------------------------------------------------

/**
 * Convert local days to UTC epoch milliseconds.
 * This is not an accurate conversion in that getTimezoneOffset 
 * takes the milliseconds in GMT (not local time). In theory, more 
 * accurate algorithm can be implemented but practically we do not need 
 * to go through that complication as long as the historical timezone 
 * changes did not happen around the 'tricky' new moon (new moon around 
 * midnight). 
 *  
 * @param days days after January 1, 1970 0:00 in the astronomical base zone
 * @return milliseconds after January 1, 1970 0:00 GMT
 */
double ChineseCalendar::daysToMillis(double days) const {
    double millis = days * (double)kOneDay;
    if (fZoneAstroCalc != NULL) {
        int32_t rawOffset, dstOffset;
        UErrorCode status = U_ZERO_ERROR;
        fZoneAstroCalc->getOffset(millis, FALSE, rawOffset, dstOffset, status);
        if (U_SUCCESS(status)) {
        	return millis - (double)(rawOffset + dstOffset);
        }
    }
    return millis - (double)CHINA_OFFSET;
}

/**
 * Convert UTC epoch milliseconds to local days.
 * @param millis milliseconds after January 1, 1970 0:00 GMT
 * @return days after January 1, 1970 0:00 in the astronomical base zone
 */
double ChineseCalendar::millisToDays(double millis) const {
    if (fZoneAstroCalc != NULL) {
        int32_t rawOffset, dstOffset;
        UErrorCode status = U_ZERO_ERROR;
        fZoneAstroCalc->getOffset(millis, FALSE, rawOffset, dstOffset, status);
        if (U_SUCCESS(status)) {
        	return ClockMath::floorDivide(millis + (double)(rawOffset + dstOffset), kOneDay);
        }
    }
    return ClockMath::floorDivide(millis + (double)CHINA_OFFSET, kOneDay);
}

//------------------------------------------------------------------
// Astronomical computations
//------------------------------------------------------------------


/**
 * Return the major solar term on or after December 15 of the given
 * Gregorian year, that is, the winter solstice of the given year.
 * Computations are relative to Asia/Shanghai time zone.
 * @param gyear a Gregorian year
 * @return days after January 1, 1970 0:00 Asia/Shanghai of the
 * winter solstice of the given year
 */
int32_t ChineseCalendar::winterSolstice(int32_t gyear) const {

    UErrorCode status = U_ZERO_ERROR;
    int32_t cacheValue = CalendarCache::get(&gChineseCalendarWinterSolsticeCache, gyear, status);

    if (cacheValue == 0) {
        // In books December 15 is used, but it fails for some years
        // using our algorithms, e.g.: 1298 1391 1492 1553 1560.  That
        // is, winterSolstice(1298) starts search at Dec 14 08:00:00
        // PST 1298 with a final result of Dec 14 10:31:59 PST 1299.
        double ms = daysToMillis(Grego::fieldsToDay(gyear, UCAL_DECEMBER, 1));

        umtx_lock(&astroLock);
        if(gChineseCalendarAstro == NULL) {
            gChineseCalendarAstro = new CalendarAstronomer();
            ucln_i18n_registerCleanup(UCLN_I18N_CHINESE_CALENDAR, calendar_chinese_cleanup);
        }
        gChineseCalendarAstro->setTime(ms);
        UDate solarLong = gChineseCalendarAstro->getSunTime(CalendarAstronomer::WINTER_SOLSTICE(), TRUE);
        umtx_unlock(&astroLock);

        // Winter solstice is 270 degrees solar longitude aka Dongzhi
        cacheValue = (int32_t)millisToDays(solarLong);
        CalendarCache::put(&gChineseCalendarWinterSolsticeCache, gyear, cacheValue, status);
    }
    if(U_FAILURE(status)) {
        cacheValue = 0;
    }
    return cacheValue;
}

/**
 * Return the closest new moon to the given date, searching either
 * forward or backward in time.
 * @param days days after January 1, 1970 0:00 Asia/Shanghai
 * @param after if true, search for a new moon on or after the given
 * date; otherwise, search for a new moon before it
 * @return days after January 1, 1970 0:00 Asia/Shanghai of the nearest
 * new moon after or before <code>days</code>
 */
int32_t ChineseCalendar::newMoonNear(double days, UBool after) const {
    
    umtx_lock(&astroLock);
    if(gChineseCalendarAstro == NULL) {
        gChineseCalendarAstro = new CalendarAstronomer();
        ucln_i18n_registerCleanup(UCLN_I18N_CHINESE_CALENDAR, calendar_chinese_cleanup);
    }
    gChineseCalendarAstro->setTime(daysToMillis(days));
    UDate newMoon = gChineseCalendarAstro->getMoonTime(CalendarAstronomer::NEW_MOON(), after);
    umtx_unlock(&astroLock);
    
    return (int32_t) millisToDays(newMoon);
}

/**
 * Return the nearest integer number of synodic months between
 * two dates.
 * @param day1 days after January 1, 1970 0:00 Asia/Shanghai
 * @param day2 days after January 1, 1970 0:00 Asia/Shanghai
 * @return the nearest integer number of months between day1 and day2
 */
int32_t ChineseCalendar::synodicMonthsBetween(int32_t day1, int32_t day2) const {
    double roundme = ((day2 - day1) / CalendarAstronomer::SYNODIC_MONTH);
    return (int32_t) (roundme + (roundme >= 0 ? .5 : -.5));
}

/**
 * Return the major solar term on or before a given date.  This
 * will be an integer from 1..12, with 1 corresponding to 330 degrees,
 * 2 to 0 degrees, 3 to 30 degrees,..., and 12 to 300 degrees.
 * @param days days after January 1, 1970 0:00 Asia/Shanghai
 */
int32_t ChineseCalendar::majorSolarTerm(int32_t days) const {
    
    umtx_lock(&astroLock);
    if(gChineseCalendarAstro == NULL) {
        gChineseCalendarAstro = new CalendarAstronomer();
        ucln_i18n_registerCleanup(UCLN_I18N_CHINESE_CALENDAR, calendar_chinese_cleanup);
    }
    gChineseCalendarAstro->setTime(daysToMillis(days));
    UDate solarLongitude = gChineseCalendarAstro->getSunLongitude();
    umtx_unlock(&astroLock);

    // Compute (floor(solarLongitude / (pi/6)) + 2) % 12
    int32_t term = ( ((int32_t)(6 * solarLongitude / CalendarAstronomer::PI)) + 2 ) % 12;
    if (term < 1) {
        term += 12;
    }
    return term;
}

/**
 * Return true if the given month lacks a major solar term.
 * @param newMoon days after January 1, 1970 0:00 Asia/Shanghai of a new
 * moon
 */
UBool ChineseCalendar::hasNoMajorSolarTerm(int32_t newMoon) const {
    return majorSolarTerm(newMoon) ==
        majorSolarTerm(newMoonNear(newMoon + SYNODIC_GAP, TRUE));
}


//------------------------------------------------------------------
// Time to fields
//------------------------------------------------------------------

/**
 * Return true if there is a leap month on or after month newMoon1 and
 * at or before month newMoon2.
 * @param newMoon1 days after January 1, 1970 0:00 astronomical base zone
 * of a new moon
 * @param newMoon2 days after January 1, 1970 0:00 astronomical base zone
 * of a new moon
 */
UBool ChineseCalendar::isLeapMonthBetween(int32_t newMoon1, int32_t newMoon2) const {

#ifdef U_DEBUG_CHNSECAL
    // This is only needed to debug the timeOfAngle divergence bug.
    // Remove this later. Liu 11/9/00
    if (synodicMonthsBetween(newMoon1, newMoon2) >= 50) {
        U_DEBUG_CHNSECAL_MSG((
            "isLeapMonthBetween(%d, %d): Invalid parameters", newMoon1, newMoon2
            ));
    }
#endif

    return (newMoon2 >= newMoon1) &&
        (isLeapMonthBetween(newMoon1, newMoonNear(newMoon2 - SYNODIC_GAP, FALSE)) ||
         hasNoMajorSolarTerm(newMoon2));
}

/**
 * Compute fields for the Chinese calendar system.  This method can
 * either set all relevant fields, as required by
 * <code>handleComputeFields()</code>, or it can just set the MONTH and
 * IS_LEAP_MONTH fields, as required by
 * <code>handleComputeMonthStart()</code>.
 *
 * <p>As a side effect, this method sets {@link #isLeapYear}.
 * @param days days after January 1, 1970 0:00 astronomical base zone
 * of the date to compute fields for
 * @param gyear the Gregorian year of the given date
 * @param gmonth the Gregorian month of the given date
 * @param setAllFields if true, set the EXTENDED_YEAR, ERA, YEAR,
 * DAY_OF_MONTH, and DAY_OF_YEAR fields.  In either case set the MONTH
 * and IS_LEAP_MONTH fields.
 */
void ChineseCalendar::computeChineseFields(int32_t days, int32_t gyear, int32_t gmonth,
                                  UBool setAllFields) {

    // Find the winter solstices before and after the target date.
    // These define the boundaries of this Chinese year, specifically,
    // the position of month 11, which always contains the solstice.
    // We want solsticeBefore <= date < solsticeAfter.
    int32_t solsticeBefore;
    int32_t solsticeAfter = winterSolstice(gyear);
    if (days < solsticeAfter) {
        solsticeBefore = winterSolstice(gyear - 1);
    } else {
        solsticeBefore = solsticeAfter;
        solsticeAfter = winterSolstice(gyear + 1);
    }

    // Find the start of the month after month 11.  This will be either
    // the prior month 12 or leap month 11 (very rare).  Also find the
    // start of the following month 11.
    int32_t firstMoon = newMoonNear(solsticeBefore + 1, TRUE);
    int32_t lastMoon = newMoonNear(solsticeAfter + 1, FALSE);
    int32_t thisMoon = newMoonNear(days + 1, FALSE); // Start of this month
    // Note: isLeapYear is a member variable
    isLeapYear = synodicMonthsBetween(firstMoon, lastMoon) == 12;

    int32_t month = synodicMonthsBetween(firstMoon, thisMoon);
    if (isLeapYear && isLeapMonthBetween(firstMoon, thisMoon)) {
        month--;
    }
    if (month < 1) {
        month += 12;
    }

    UBool isLeapMonth = isLeapYear &&
        hasNoMajorSolarTerm(thisMoon) &&
        !isLeapMonthBetween(firstMoon, newMoonNear(thisMoon - SYNODIC_GAP, FALSE));

    internalSet(UCAL_MONTH, month-1); // Convert from 1-based to 0-based
    internalSet(UCAL_IS_LEAP_MONTH, isLeapMonth?1:0);

    if (setAllFields) {

        // Extended year and cycle year is based on the epoch year
        
        int32_t extended_year = gyear - fEpochYear;
        int cycle_year = gyear - CHINESE_EPOCH_YEAR;
        if (month < 11 ||
            gmonth >= UCAL_JULY) {
            extended_year++;
            cycle_year++;
        }
        int32_t dayOfMonth = days - thisMoon + 1;

        internalSet(UCAL_EXTENDED_YEAR, extended_year);

        // 0->0,60  1->1,1  60->1,60  61->2,1  etc.
        int32_t yearOfCycle;
        int32_t cycle = ClockMath::floorDivide(cycle_year - 1, 60, yearOfCycle);
        internalSet(UCAL_ERA, cycle + 1);
        internalSet(UCAL_YEAR, yearOfCycle + 1);

        internalSet(UCAL_DAY_OF_MONTH, dayOfMonth);

        // Days will be before the first new year we compute if this
        // date is in month 11, leap 11, 12.  There is never a leap 12.
        // New year computations are cached so this should be cheap in
        // the long run.
        int32_t theNewYear = newYear(gyear);
        if (days < theNewYear) {
            theNewYear = newYear(gyear-1);
        }
        internalSet(UCAL_DAY_OF_YEAR, days - theNewYear + 1);
    }
}


//------------------------------------------------------------------
// Fields to time
//------------------------------------------------------------------

/**
 * Return the Chinese new year of the given Gregorian year.
 * @param gyear a Gregorian year
 * @return days after January 1, 1970 0:00 astronomical base zone of the
 * Chinese new year of the given year (this will be a new moon)
 */
int32_t ChineseCalendar::newYear(int32_t gyear) const {
    UErrorCode status = U_ZERO_ERROR;
    int32_t cacheValue = CalendarCache::get(&gChineseCalendarNewYearCache, gyear, status);

    if (cacheValue == 0) {

        int32_t solsticeBefore= winterSolstice(gyear - 1);
        int32_t solsticeAfter = winterSolstice(gyear);
        int32_t newMoon1 = newMoonNear(solsticeBefore + 1, TRUE);
        int32_t newMoon2 = newMoonNear(newMoon1 + SYNODIC_GAP, TRUE);
        int32_t newMoon11 = newMoonNear(solsticeAfter + 1, FALSE);
        
        if (synodicMonthsBetween(newMoon1, newMoon11) == 12 &&
            (hasNoMajorSolarTerm(newMoon1) || hasNoMajorSolarTerm(newMoon2))) {
            cacheValue = newMoonNear(newMoon2 + SYNODIC_GAP, TRUE);
        } else {
            cacheValue = newMoon2;
        }

        CalendarCache::put(&gChineseCalendarNewYearCache, gyear, cacheValue, status);
    }
    if(U_FAILURE(status)) {
        cacheValue = 0;
    }
    return cacheValue;
}

/**
 * Adjust this calendar to be delta months before or after a given
 * start position, pinning the day of month if necessary.  The start
 * position is given as a local days number for the start of the month
 * and a day-of-month.  Used by add() and roll().
 * @param newMoon the local days of the first day of the month of the
 * start position (days after January 1, 1970 0:00 Asia/Shanghai)
 * @param dom the 1-based day-of-month of the start position
 * @param delta the number of months to move forward or backward from
 * the start position
 */
void ChineseCalendar::offsetMonth(int32_t newMoon, int32_t dom, int32_t delta) {
    UErrorCode status = U_ZERO_ERROR;

    // Move to the middle of the month before our target month.
    newMoon += (int32_t) (CalendarAstronomer::SYNODIC_MONTH * (delta - 0.5));

    // Search forward to the target month's new moon
    newMoon = newMoonNear(newMoon, TRUE);

    // Find the target dom
    int32_t jd = newMoon + kEpochStartAsJulianDay - 1 + dom;

    // Pin the dom.  In this calendar all months are 29 or 30 days
    // so pinning just means handling dom 30.
    if (dom > 29) {
        set(UCAL_JULIAN_DAY, jd-1);
        // TODO Fix this.  We really shouldn't ever have to
        // explicitly call complete().  This is either a bug in
        // this method, in ChineseCalendar, or in
        // Calendar.getActualMaximum().  I suspect the last.
        complete(status);
        if (U_FAILURE(status)) return;
        if (getActualMaximum(UCAL_DAY_OF_MONTH, status) >= dom) {
            if (U_FAILURE(status)) return;
            set(UCAL_JULIAN_DAY, jd);
        }
    } else {
        set(UCAL_JULIAN_DAY, jd);
    }
}


UBool
ChineseCalendar::inDaylightTime(UErrorCode& status) const
{
    // copied from GregorianCalendar
    if (U_FAILURE(status) || !getTimeZone().useDaylightTime()) 
        return FALSE;

    // Force an update of the state of the Calendar.
    ((ChineseCalendar*)this)->complete(status); // cast away const

    return (UBool)(U_SUCCESS(status) ? (internalGet(UCAL_DST_OFFSET) != 0) : FALSE);
}

// default century

static UDate     gSystemDefaultCenturyStart       = DBL_MIN;
static int32_t   gSystemDefaultCenturyStartYear   = -1;
static icu::UInitOnce gSystemDefaultCenturyInitOnce = U_INITONCE_INITIALIZER;


UBool ChineseCalendar::haveDefaultCentury() const
{
    return TRUE;
}

UDate ChineseCalendar::defaultCenturyStart() const
{
    return internalGetDefaultCenturyStart();
}

int32_t ChineseCalendar::defaultCenturyStartYear() const
{
    return internalGetDefaultCenturyStartYear();
}

static void U_CALLCONV initializeSystemDefaultCentury()
{
    // initialize systemDefaultCentury and systemDefaultCenturyYear based
    // on the current time.  They'll be set to 80 years before
    // the current time.
    UErrorCode status = U_ZERO_ERROR;
    ChineseCalendar calendar(Locale("@calendar=chinese"),status);
    if (U_SUCCESS(status)) {
        calendar.setTime(Calendar::getNow(), status);
        calendar.add(UCAL_YEAR, -80, status);
        gSystemDefaultCenturyStart     = calendar.getTime(status);
        gSystemDefaultCenturyStartYear = calendar.get(UCAL_YEAR, status);
    }
    // We have no recourse upon failure unless we want to propagate the failure
    // out.
}

UDate
ChineseCalendar::internalGetDefaultCenturyStart() const
{
    // lazy-evaluate systemDefaultCenturyStart
    umtx_initOnce(gSystemDefaultCenturyInitOnce, &initializeSystemDefaultCentury);
    return gSystemDefaultCenturyStart;
}

int32_t
ChineseCalendar::internalGetDefaultCenturyStartYear() const
{
    // lazy-evaluate systemDefaultCenturyStartYear
    umtx_initOnce(gSystemDefaultCenturyInitOnce, &initializeSystemDefaultCentury);
    return    gSystemDefaultCenturyStartYear;
}

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(ChineseCalendar)

U_NAMESPACE_END

#endif

