// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
******************************************************************************
* Copyright (C) 2003-2015, International Business Machines Corporation
* and others. All Rights Reserved.
******************************************************************************
*
* File ISLAMCAL.H
*
* Modification History:
*
*   Date        Name        Description
*   10/14/2003  srl         ported from java IslamicCalendar
*****************************************************************************
*/

#include "islamcal.h"

#if !UCONFIG_NO_FORMATTING

#include "umutex.h"
#include <float.h>
#include "gregoimp.h" // Math
#include "astro.h" // CalendarAstronomer
#include "uhash.h"
#include "ucln_in.h"
#include "uassert.h"

static const UDate HIJRA_MILLIS = -42521587200000.0;    // 7/16/622 AD 00:00

// Debugging
#ifdef U_DEBUG_ISLAMCAL
# include <stdio.h>
# include <stdarg.h>
static void debug_islamcal_loc(const char *f, int32_t l)
{
    fprintf(stderr, "%s:%d: ", f, l);
}

static void debug_islamcal_msg(const char *pat, ...)
{
    va_list ap;
    va_start(ap, pat);
    vfprintf(stderr, pat, ap);
    fflush(stderr);
}
// must use double parens, i.e.:  U_DEBUG_ISLAMCAL_MSG(("four is: %d",4));
#define U_DEBUG_ISLAMCAL_MSG(x) {debug_islamcal_loc(__FILE__,__LINE__);debug_islamcal_msg x;}
#else
#define U_DEBUG_ISLAMCAL_MSG(x)
#endif


// --- The cache --
// cache of months
static icu::CalendarCache *gMonthCache = NULL;
static icu::CalendarAstronomer *gIslamicCalendarAstro = NULL;

U_CDECL_BEGIN
static UBool calendar_islamic_cleanup(void) {
    if (gMonthCache) {
        delete gMonthCache;
        gMonthCache = NULL;
    }
    if (gIslamicCalendarAstro) {
        delete gIslamicCalendarAstro;
        gIslamicCalendarAstro = NULL;
    }
    return TRUE;
}
U_CDECL_END

U_NAMESPACE_BEGIN

// Implementation of the IslamicCalendar class

/**
 * Friday EPOC
 */
static const int32_t CIVIL_EPOC = 1948440; // CE 622 July 16 Friday (Julian calendar) / CE 622 July 19 (Gregorian calendar)

/**
  * Thursday EPOC
  */
static const int32_t ASTRONOMICAL_EPOC = 1948439; // CE 622 July 15 Thursday (Julian calendar)


static const int32_t UMALQURA_YEAR_START = 1300;
static const int32_t UMALQURA_YEAR_END = 1600;

static const int UMALQURA_MONTHLENGTH[] = {
    //* 1300 -1302 */ "1010 1010 1010", "1101 0101 0100", "1110 1100 1001",
                            0x0AAA,           0x0D54,           0x0EC9,
    //* 1303 -1307 */ "0110 1101 0100", "0110 1110 1010", "0011 0110 1100", "1010 1010 1101", "0101 0101 0101",
                            0x06D4,           0x06EA,           0x036C,           0x0AAD,           0x0555,
    //* 1308 -1312 */ "0110 1010 1001", "0111 1001 0010", "1011 1010 1001", "0101 1101 0100", "1010 1101 1010",
                            0x06A9,           0x0792,           0x0BA9,           0x05D4,           0x0ADA,
    //* 1313 -1317 */ "0101 0101 1100", "1101 0010 1101", "0110 1001 0101", "0111 0100 1010", "1011 0101 0100",
                            0x055C,           0x0D2D,           0x0695,           0x074A,           0x0B54,
    //* 1318 -1322 */ "1011 0110 1010", "0101 1010 1101", "0100 1010 1110", "1010 0100 1111", "0101 0001 0111",
                            0x0B6A,           0x05AD,           0x04AE,           0x0A4F,           0x0517,
    //* 1323 -1327 */ "0110 1000 1011", "0110 1010 0101", "1010 1101 0101", "0010 1101 0110", "1001 0101 1011",
                            0x068B,           0x06A5,           0x0AD5,           0x02D6,           0x095B,
    //* 1328 -1332 */ "0100 1001 1101", "1010 0100 1101", "1101 0010 0110", "1101 1001 0101", "0101 1010 1100",
                            0x049D,           0x0A4D,           0x0D26,           0x0D95,           0x05AC,
    //* 1333 -1337 */ "1001 1011 0110", "0010 1011 1010", "1010 0101 1011", "0101 0010 1011", "1010 1001 0101",
                            0x09B6,           0x02BA,           0x0A5B,           0x052B,           0x0A95,
    //* 1338 -1342 */ "0110 1100 1010", "1010 1110 1001", "0010 1111 0100", "1001 0111 0110", "0010 1011 0110",
                            0x06CA,           0x0AE9,           0x02F4,           0x0976,           0x02B6,
    //* 1343 -1347 */ "1001 0101 0110", "1010 1100 1010", "1011 1010 0100", "1011 1101 0010", "0101 1101 1001",
                            0x0956,           0x0ACA,           0x0BA4,           0x0BD2,           0x05D9,
    //* 1348 -1352 */ "0010 1101 1100", "1001 0110 1101", "0101 0100 1101", "1010 1010 0101", "1011 0101 0010",
                            0x02DC,           0x096D,           0x054D,           0x0AA5,           0x0B52,
    //* 1353 -1357 */ "1011 1010 0101", "0101 1011 0100", "1001 1011 0110", "0101 0101 0111", "0010 1001 0111",
                            0x0BA5,           0x05B4,           0x09B6,           0x0557,           0x0297,
    //* 1358 -1362 */ "0101 0100 1011", "0110 1010 0011", "0111 0101 0010", "1011 0110 0101", "0101 0110 1010",
                            0x054B,           0x06A3,           0x0752,           0x0B65,           0x056A,
    //* 1363 -1367 */ "1010 1010 1011", "0101 0010 1011", "1100 1001 0101", "1101 0100 1010", "1101 1010 0101",
                            0x0AAB,           0x052B,           0x0C95,           0x0D4A,           0x0DA5,
    //* 1368 -1372 */ "0101 1100 1010", "1010 1101 0110", "1001 0101 0111", "0100 1010 1011", "1001 0100 1011",
                            0x05CA,           0x0AD6,           0x0957,           0x04AB,           0x094B,
    //* 1373 -1377 */ "1010 1010 0101", "1011 0101 0010", "1011 0110 1010", "0101 0111 0101", "0010 0111 0110",
                            0x0AA5,           0x0B52,           0x0B6A,           0x0575,           0x0276,
    //* 1378 -1382 */ "1000 1011 0111", "0100 0101 1011", "0101 0101 0101", "0101 1010 1001", "0101 1011 0100",
                            0x08B7,           0x045B,           0x0555,           0x05A9,           0x05B4,
    //* 1383 -1387 */ "1001 1101 1010", "0100 1101 1101", "0010 0110 1110", "1001 0011 0110", "1010 1010 1010",
                            0x09DA,           0x04DD,           0x026E,           0x0936,           0x0AAA,
    //* 1388 -1392 */ "1101 0101 0100", "1101 1011 0010", "0101 1101 0101", "0010 1101 1010", "1001 0101 1011",
                            0x0D54,           0x0DB2,           0x05D5,           0x02DA,           0x095B,
    //* 1393 -1397 */ "0100 1010 1011", "1010 0101 0101", "1011 0100 1001", "1011 0110 0100", "1011 0111 0001",
                            0x04AB,           0x0A55,           0x0B49,           0x0B64,           0x0B71,
    //* 1398 -1402 */ "0101 1011 0100", "1010 1011 0101", "1010 0101 0101", "1101 0010 0101", "1110 1001 0010",
                            0x05B4,           0x0AB5,           0x0A55,           0x0D25,           0x0E92,
    //* 1403 -1407 */ "1110 1100 1001", "0110 1101 0100", "1010 1110 1001", "1001 0110 1011", "0100 1010 1011",
                            0x0EC9,           0x06D4,           0x0AE9,           0x096B,           0x04AB,
    //* 1408 -1412 */ "1010 1001 0011", "1101 0100 1001", "1101 1010 0100", "1101 1011 0010", "1010 1011 1001",
                            0x0A93,           0x0D49,         0x0DA4,           0x0DB2,           0x0AB9,
    //* 1413 -1417 */ "0100 1011 1010", "1010 0101 1011", "0101 0010 1011", "1010 1001 0101", "1011 0010 1010",
                            0x04BA,           0x0A5B,           0x052B,           0x0A95,           0x0B2A,
    //* 1418 -1422 */ "1011 0101 0101", "0101 0101 1100", "0100 1011 1101", "0010 0011 1101", "1001 0001 1101",
                            0x0B55,           0x055C,           0x04BD,           0x023D,           0x091D,
    //* 1423 -1427 */ "1010 1001 0101", "1011 0100 1010", "1011 0101 1010", "0101 0110 1101", "0010 1011 0110",
                            0x0A95,           0x0B4A,           0x0B5A,           0x056D,           0x02B6,
    //* 1428 -1432 */ "1001 0011 1011", "0100 1001 1011", "0110 0101 0101", "0110 1010 1001", "0111 0101 0100",
                            0x093B,           0x049B,           0x0655,           0x06A9,           0x0754,
    //* 1433 -1437 */ "1011 0110 1010", "0101 0110 1100", "1010 1010 1101", "0101 0101 0101", "1011 0010 1001",
                            0x0B6A,           0x056C,           0x0AAD,           0x0555,           0x0B29,
    //* 1438 -1442 */ "1011 1001 0010", "1011 1010 1001", "0101 1101 0100", "1010 1101 1010", "0101 0101 1010",
                            0x0B92,           0x0BA9,           0x05D4,           0x0ADA,           0x055A,
    //* 1443 -1447 */ "1010 1010 1011", "0101 1001 0101", "0111 0100 1001", "0111 0110 0100", "1011 1010 1010",
                            0x0AAB,           0x0595,           0x0749,           0x0764,           0x0BAA,
    //* 1448 -1452 */ "0101 1011 0101", "0010 1011 0110", "1010 0101 0110", "1110 0100 1101", "1011 0010 0101",
                            0x05B5,           0x02B6,           0x0A56,           0x0E4D,           0x0B25,
    //* 1453 -1457 */ "1011 0101 0010", "1011 0110 1010", "0101 1010 1101", "0010 1010 1110", "1001 0010 1111",
                            0x0B52,           0x0B6A,           0x05AD,           0x02AE,           0x092F,
    //* 1458 -1462 */ "0100 1001 0111", "0110 0100 1011", "0110 1010 0101", "0110 1010 1100", "1010 1101 0110",
                            0x0497,           0x064B,           0x06A5,           0x06AC,           0x0AD6,
    //* 1463 -1467 */ "0101 0101 1101", "0100 1001 1101", "1010 0100 1101", "1101 0001 0110", "1101 1001 0101",
                            0x055D,           0x049D,           0x0A4D,           0x0D16,           0x0D95,
    //* 1468 -1472 */ "0101 1010 1010", "0101 1011 0101", "0010 1101 1010", "1001 0101 1011", "0100 1010 1101",
                            0x05AA,           0x05B5,           0x02DA,           0x095B,           0x04AD,
    //* 1473 -1477 */ "0101 1001 0101", "0110 1100 1010", "0110 1110 0100", "1010 1110 1010", "0100 1111 0101",
                            0x0595,           0x06CA,           0x06E4,           0x0AEA,           0x04F5,
    //* 1478 -1482 */ "0010 1011 0110", "1001 0101 0110", "1010 1010 1010", "1011 0101 0100", "1011 1101 0010",
                            0x02B6,           0x0956,           0x0AAA,           0x0B54,           0x0BD2,
    //* 1483 -1487 */ "0101 1101 1001", "0010 1110 1010", "1001 0110 1101", "0100 1010 1101", "1010 1001 0101",
                            0x05D9,           0x02EA,           0x096D,           0x04AD,           0x0A95,
    //* 1488 -1492 */ "1011 0100 1010", "1011 1010 0101", "0101 1011 0010", "1001 1011 0101", "0100 1101 0110",
                            0x0B4A,           0x0BA5,           0x05B2,           0x09B5,           0x04D6,
    //* 1493 -1497 */ "1010 1001 0111", "0101 0100 0111", "0110 1001 0011", "0111 0100 1001", "1011 0101 0101",
                            0x0A97,           0x0547,           0x0693,           0x0749,           0x0B55,
    //* 1498 -1508 */ "0101 0110 1010", "1010 0110 1011", "0101 0010 1011", "1010 1000 1011", "1101 0100 0110", "1101 1010 0011", "0101 1100 1010", "1010 1101 0110", "0100 1101 1011", "0010 0110 1011", "1001 0100 1011",
                            0x056A,           0x0A6B,           0x052B,           0x0A8B,           0x0D46,           0x0DA3,           0x05CA,           0x0AD6,           0x04DB,           0x026B,           0x094B,
    //* 1509 -1519 */ "1010 1010 0101", "1011 0101 0010", "1011 0110 1001", "0101 0111 0101", "0001 0111 0110", "1000 1011 0111", "0010 0101 1011", "0101 0010 1011", "0101 0110 0101", "0101 1011 0100", "1001 1101 1010",
                            0x0AA5,           0x0B52,           0x0B69,           0x0575,           0x0176,           0x08B7,           0x025B,           0x052B,           0x0565,           0x05B4,           0x09DA,
    //* 1520 -1530 */ "0100 1110 1101", "0001 0110 1101", "1000 1011 0110", "1010 1010 0110", "1101 0101 0010", "1101 1010 1001", "0101 1101 0100", "1010 1101 1010", "1001 0101 1011", "0100 1010 1011", "0110 0101 0011",
                            0x04ED,           0x016D,           0x08B6,           0x0AA6,           0x0D52,           0x0DA9,           0x05D4,           0x0ADA,           0x095B,           0x04AB,           0x0653,
    //* 1531 -1541 */ "0111 0010 1001", "0111 0110 0010", "1011 1010 1001", "0101 1011 0010", "1010 1011 0101", "0101 0101 0101", "1011 0010 0101", "1101 1001 0010", "1110 1100 1001", "0110 1101 0010", "1010 1110 1001",
                            0x0729,           0x0762,           0x0BA9,           0x05B2,           0x0AB5,           0x0555,           0x0B25,           0x0D92,           0x0EC9,           0x06D2,           0x0AE9,
    //* 1542 -1552 */ "0101 0110 1011", "0100 1010 1011", "1010 0101 0101", "1101 0010 1001", "1101 0101 0100", "1101 1010 1010", "1001 1011 0101", "0100 1011 1010", "1010 0011 1011", "0100 1001 1011", "1010 0100 1101",
                            0x056B,           0x04AB,           0x0A55,           0x0D29,           0x0D54,           0x0DAA,           0x09B5,           0x04BA,           0x0A3B,           0x049B,           0x0A4D,
    //* 1553 -1563 */ "1010 1010 1010", "1010 1101 0101", "0010 1101 1010", "1001 0101 1101", "0100 0101 1110", "1010 0010 1110", "1100 1001 1010", "1101 0101 0101", "0110 1011 0010", "0110 1011 1001", "0100 1011 1010",
                            0x0AAA,           0x0AD5,           0x02DA,           0x095D,           0x045E,           0x0A2E,           0x0C9A,           0x0D55,           0x06B2,           0x06B9,           0x04BA,
    //* 1564 -1574 */ "1010 0101 1101", "0101 0010 1101", "1010 1001 0101", "1011 0101 0010", "1011 1010 1000", "1011 1011 0100", "0101 1011 1001", "0010 1101 1010", "1001 0101 1010", "1011 0100 1010", "1101 1010 0100",
                            0x0A5D,           0x052D,           0x0A95,           0x0B52,           0x0BA8,           0x0BB4,           0x05B9,           0x02DA,           0x095A,           0x0B4A,           0x0DA4,
    //* 1575 -1585 */ "1110 1101 0001", "0110 1110 1000", "1011 0110 1010", "0101 0110 1101", "0101 0011 0101", "0110 1001 0101", "1101 0100 1010", "1101 1010 1000", "1101 1101 0100", "0110 1101 1010", "0101 0101 1011",
                            0x0ED1,           0x06E8,           0x0B6A,           0x056D,           0x0535,           0x0695,           0x0D4A,           0x0DA8,           0x0DD4,           0x06DA,           0x055B,
    //* 1586 -1596 */ "0010 1001 1101", "0110 0010 1011", "1011 0001 0101", "1011 0100 1010", "1011 1001 0101", "0101 1010 1010", "1010 1010 1110", "1001 0010 1110", "1100 1000 1111", "0101 0010 0111", "0110 1001 0101",
                            0x029D,           0x062B,           0x0B15,           0x0B4A,           0x0B95,           0x05AA,           0x0AAE,           0x092E,           0x0C8F,           0x0527,           0x0695,
    //* 1597 -1600 */ "0110 1010 1010", "1010 1101 0110", "0101 0101 1101", "0010 1001 1101", };
                            0x06AA,           0x0AD6,           0x055D,           0x029D
};

int32_t getUmalqura_MonthLength(int32_t y, int32_t m) {
    int32_t mask = (int32_t) (0x01 << (11 - m));    // set mask for bit corresponding to month
    if((UMALQURA_MONTHLENGTH[y] & mask) == 0 )
        return 29;
    else
        return 30;

}

//-------------------------------------------------------------------------
// Constructors...
//-------------------------------------------------------------------------

const char *IslamicCalendar::getType() const {
    const char *sType = NULL;

    switch (cType) {
    case CIVIL:
        sType = "islamic-civil";
        break;
    case ASTRONOMICAL:
        sType = "islamic";
        break;
    case TBLA:
        sType = "islamic-tbla";
        break;
    case UMALQURA:
        sType = "islamic-umalqura";
        break;
    default:
        UPRV_UNREACHABLE; // out of range
    }
    return sType;
}

IslamicCalendar* IslamicCalendar::clone() const {
    return new IslamicCalendar(*this);
}

IslamicCalendar::IslamicCalendar(const Locale& aLocale, UErrorCode& success, ECalculationType type)
:   Calendar(TimeZone::createDefault(), aLocale, success),
cType(type)
{
    setTimeInMillis(getNow(), success); // Call this again now that the vtable is set up properly.
}

IslamicCalendar::IslamicCalendar(const IslamicCalendar& other) : Calendar(other), cType(other.cType) {
}

IslamicCalendar::~IslamicCalendar()
{
}

void IslamicCalendar::setCalculationType(ECalculationType type, UErrorCode &status)
{
    if (cType != type) {
        // The fields of the calendar will become invalid, because the calendar
        // rules are different
        UDate m = getTimeInMillis(status);
        cType = type;
        clear();
        setTimeInMillis(m, status);
    }
}

/**
* Returns <code>true</code> if this object is using the fixed-cycle civil
* calendar, or <code>false</code> if using the religious, astronomical
* calendar.
* @draft ICU 2.4
*/
UBool IslamicCalendar::isCivil() {
    return (cType == CIVIL);
}

//-------------------------------------------------------------------------
// Minimum / Maximum access functions
//-------------------------------------------------------------------------

// Note: Current IslamicCalendar implementation does not work
// well with negative years.

// TODO: In some cases the current ICU Islamic calendar implementation shows
// a month as having 31 days. Since date parsing now uses range checks based
// on the table below, we need to change the range for last day of month to
// include 31 as a workaround until the implementation is fixed.
static const int32_t LIMITS[UCAL_FIELD_COUNT][4] = {
    // Minimum  Greatest    Least  Maximum
    //           Minimum  Maximum
    {        0,        0,        0,        0}, // ERA
    {        1,        1,  5000000,  5000000}, // YEAR
    {        0,        0,       11,       11}, // MONTH
    {        1,        1,       50,       51}, // WEEK_OF_YEAR
    {/*N/A*/-1,/*N/A*/-1,/*N/A*/-1,/*N/A*/-1}, // WEEK_OF_MONTH
    {        1,        1,       29,       31}, // DAY_OF_MONTH - 31 to workaround for cal implementation bug, should be 30
    {        1,        1,      354,      355}, // DAY_OF_YEAR
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
    {        1,        1,  5000000,  5000000}, // YEAR_WOY
    {/*N/A*/-1,/*N/A*/-1,/*N/A*/-1,/*N/A*/-1}, // DOW_LOCAL
    {        1,        1,  5000000,  5000000}, // EXTENDED_YEAR
    {/*N/A*/-1,/*N/A*/-1,/*N/A*/-1,/*N/A*/-1}, // JULIAN_DAY
    {/*N/A*/-1,/*N/A*/-1,/*N/A*/-1,/*N/A*/-1}, // MILLISECONDS_IN_DAY
    {/*N/A*/-1,/*N/A*/-1,/*N/A*/-1,/*N/A*/-1}, // IS_LEAP_MONTH
};

/**
* @draft ICU 2.4
*/
int32_t IslamicCalendar::handleGetLimit(UCalendarDateFields field, ELimitType limitType) const {
    return LIMITS[field][limitType];
}

//-------------------------------------------------------------------------
// Assorted calculation utilities
//

// we could compress this down more if we need to
static const int8_t umAlQuraYrStartEstimateFix[] = {
     0,  0, -1,  0, -1,  0,  0,  0,  0,  0, // 1300..
    -1,  0,  0,  0,  0,  0,  0,  0, -1,  0, // 1310..
     1,  0,  1,  1,  0,  0,  0,  0,  1,  0, // 1320..
     0,  0,  0,  0,  0,  0,  1,  0,  0,  0, // 1330..
     0,  0,  1,  0,  0, -1, -1,  0,  0,  0, // 1340..
     1,  0,  0, -1,  0,  0,  0,  1,  1,  0, // 1350..
     0,  0,  0,  0,  0,  0,  0, -1,  0,  0, // 1360..
     0,  1,  1,  0,  0, -1,  0,  1,  0,  1, // 1370..
     1,  0,  0, -1,  0,  1,  0,  0,  0, -1, // 1380..
     0,  1,  0,  1,  0,  0,  0, -1,  0,  0, // 1390..
     0,  0, -1, -1,  0, -1,  0,  1,  0,  0, // 1400..
     0, -1,  0,  0,  0,  1,  0,  0,  0,  0, // 1410..
     0,  1,  0,  0, -1, -1,  0,  0,  0,  1, // 1420..
     0,  0, -1, -1,  0, -1,  0,  0, -1, -1, // 1430..
     0, -1,  0, -1,  0,  0, -1, -1,  0,  0, // 1440..
     0,  0,  0,  0, -1,  0,  1,  0,  1,  1, // 1450..
     0,  0, -1,  0,  1,  0,  0,  0,  0,  0, // 1460..
     1,  0,  1,  0,  0,  0, -1,  0,  1,  0, // 1470..
     0, -1, -1,  0,  0,  0,  1,  0,  0,  0, // 1480..
     0,  0,  0,  0,  1,  0,  0,  0,  0,  0, // 1490..
     1,  0,  0, -1,  0,  0,  0,  1,  1,  0, // 1500..
     0, -1,  0,  1,  0,  1,  1,  0,  0,  0, // 1510..
     0,  1,  0,  0,  0, -1,  0,  0,  0,  1, // 1520..
     0,  0,  0, -1,  0,  0,  0,  0,  0, -1, // 1530..
     0, -1,  0,  1,  0,  0,  0, -1,  0,  1, // 1540..
     0,  1,  0,  0,  0,  0,  0,  1,  0,  0, // 1550..
    -1,  0,  0,  0,  0,  1,  0,  0,  0, -1, // 1560..
     0,  0,  0,  0, -1, -1,  0, -1,  0,  1, // 1570..
     0,  0, -1, -1,  0,  0,  1,  1,  0,  0, // 1580..
    -1,  0,  0,  0,  0,  1,  0,  0,  0,  0, // 1590..
     1 // 1600
};

/**
* Determine whether a year is a leap year in the Islamic civil calendar
*/
UBool IslamicCalendar::civilLeapYear(int32_t year)
{
    return (14 + 11 * year) % 30 < 11;
}

/**
* Return the day # on which the given year starts.  Days are counted
* from the Hijri epoch, origin 0.
*/
int32_t IslamicCalendar::yearStart(int32_t year) const{
    if (cType == CIVIL || cType == TBLA ||
        (cType == UMALQURA && (year < UMALQURA_YEAR_START || year > UMALQURA_YEAR_END)))
    {
        return (year-1)*354 + ClockMath::floorDivide((3+11*year),30);
    } else if(cType==ASTRONOMICAL){
        return trueMonthStart(12*(year-1));
    } else {
        year -= UMALQURA_YEAR_START;
        // rounded least-squares fit of the dates previously calculated from UMALQURA_MONTHLENGTH iteration
        int32_t yrStartLinearEstimate = (int32_t)((354.36720 * (double)year) + 460322.05 + 0.5);
        // need a slight correction to some
        return yrStartLinearEstimate + umAlQuraYrStartEstimateFix[year];
    }
}

/**
* Return the day # on which the given month starts.  Days are counted
* from the Hijri epoch, origin 0.
*
* @param year  The hijri year
* @param month The hijri month, 0-based (assumed to be in range 0..11)
*/
int32_t IslamicCalendar::monthStart(int32_t year, int32_t month) const {
    if (cType == CIVIL || cType == TBLA) {
        // This does not handle months out of the range 0..11
        return (int32_t)uprv_ceil(29.5*month)
            + (year-1)*354 + (int32_t)ClockMath::floorDivide((3+11*year),30);
    } else if(cType==ASTRONOMICAL){
        return trueMonthStart(12*(year-1) + month);
    } else {
        int32_t ms = yearStart(year);
        for(int i=0; i< month; i++){
            ms+= handleGetMonthLength(year, i);
        }
        return ms;
    }
}

/**
* Find the day number on which a particular month of the true/lunar
* Islamic calendar starts.
*
* @param month The month in question, origin 0 from the Hijri epoch
*
* @return The day number on which the given month starts.
*/
int32_t IslamicCalendar::trueMonthStart(int32_t month) const
{
    UErrorCode status = U_ZERO_ERROR;
    int32_t start = CalendarCache::get(&gMonthCache, month, status);

    if (start==0) {
        // Make a guess at when the month started, using the average length
        UDate origin = HIJRA_MILLIS
            + uprv_floor(month * CalendarAstronomer::SYNODIC_MONTH) * kOneDay;

        // moonAge will fail due to memory allocation error
        double age = moonAge(origin, status);
        if (U_FAILURE(status)) {
            goto trueMonthStartEnd;
        }

        if (age >= 0) {
            // The month has already started
            do {
                origin -= kOneDay;
                age = moonAge(origin, status);
                if (U_FAILURE(status)) {
                    goto trueMonthStartEnd;
                }
            } while (age >= 0);
        }
        else {
            // Preceding month has not ended yet.
            do {
                origin += kOneDay;
                age = moonAge(origin, status);
                if (U_FAILURE(status)) {
                    goto trueMonthStartEnd;
                }
            } while (age < 0);
        }
        start = (int32_t)ClockMath::floorDivide((origin - HIJRA_MILLIS), (double)kOneDay) + 1;
        CalendarCache::put(&gMonthCache, month, start, status);
    }
trueMonthStartEnd :
    if(U_FAILURE(status)) {
        start = 0;
    }
    return start;
}

/**
* Return the "age" of the moon at the given time; this is the difference
* in ecliptic latitude between the moon and the sun.  This method simply
* calls CalendarAstronomer.moonAge, converts to degrees,
* and adjusts the result to be in the range [-180, 180].
*
* @param time  The time at which the moon's age is desired,
*              in millis since 1/1/1970.
*/
double IslamicCalendar::moonAge(UDate time, UErrorCode &status)
{
    double age = 0;

    static UMutex astroLock;      // pod bay door lock
    umtx_lock(&astroLock);
    if(gIslamicCalendarAstro == NULL) {
        gIslamicCalendarAstro = new CalendarAstronomer();
        if (gIslamicCalendarAstro == NULL) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return age;
        }
        ucln_i18n_registerCleanup(UCLN_I18N_ISLAMIC_CALENDAR, calendar_islamic_cleanup);
    }
    gIslamicCalendarAstro->setTime(time);
    age = gIslamicCalendarAstro->getMoonAge();
    umtx_unlock(&astroLock);

    // Convert to degrees and normalize...
    age = age * 180 / CalendarAstronomer::PI;
    if (age > 180) {
        age = age - 360;
    }

    return age;
}

//----------------------------------------------------------------------
// Calendar framework
//----------------------------------------------------------------------

/**
* Return the length (in days) of the given month.
*
* @param year  The hijri year
* @param year  The hijri month, 0-based
* @draft ICU 2.4
*/
int32_t IslamicCalendar::handleGetMonthLength(int32_t extendedYear, int32_t month) const {

    int32_t length = 0;

    if (cType == CIVIL || cType == TBLA ||
        (cType == UMALQURA && (extendedYear<UMALQURA_YEAR_START || extendedYear>UMALQURA_YEAR_END)) ) {
        length = 29 + (month+1) % 2;
        if (month == DHU_AL_HIJJAH && civilLeapYear(extendedYear)) {
            length++;
        }
    } else if(cType == ASTRONOMICAL){
        month = 12*(extendedYear-1) + month;
        length =  trueMonthStart(month+1) - trueMonthStart(month) ;
    } else {
        length = getUmalqura_MonthLength(extendedYear - UMALQURA_YEAR_START, month);
    }
    return length;
}

/**
* Return the number of days in the given Islamic year
* @draft ICU 2.4
*/
int32_t IslamicCalendar::handleGetYearLength(int32_t extendedYear) const {
    if (cType == CIVIL || cType == TBLA ||
        (cType == UMALQURA && (extendedYear<UMALQURA_YEAR_START || extendedYear>UMALQURA_YEAR_END)) ) {
        return 354 + (civilLeapYear(extendedYear) ? 1 : 0);
    } else if(cType == ASTRONOMICAL){
        int32_t month = 12*(extendedYear-1);
        return (trueMonthStart(month + 12) - trueMonthStart(month));
    } else {
        int len = 0;
        for(int i=0; i<12; i++) {
            len += handleGetMonthLength(extendedYear, i);
        }
        return len;
    }
}

//-------------------------------------------------------------------------
// Functions for converting from field values to milliseconds....
//-------------------------------------------------------------------------

// Return JD of start of given month/year
// Calendar says:
// Get the Julian day of the day BEFORE the start of this year.
// If useMonth is true, get the day before the start of the month.
// Hence the -1
/**
* @draft ICU 2.4
*/
int32_t IslamicCalendar::handleComputeMonthStart(int32_t eyear, int32_t month, UBool /* useMonth */) const {
    // This may be called by Calendar::handleComputeJulianDay with months out of the range
    // 0..11. Need to handle that here since monthStart requires months in the range 0.11.
    if (month > 11) {
        eyear += (month / 12);
        month %= 12;
    } else if (month < 0) {
        month++;
        eyear += (month / 12) - 1;
        month = (month % 12) + 11;
    }
    return monthStart(eyear, month) + ((cType == TBLA)? ASTRONOMICAL_EPOC: CIVIL_EPOC) - 1;
}

//-------------------------------------------------------------------------
// Functions for converting from milliseconds to field values
//-------------------------------------------------------------------------

/**
* @draft ICU 2.4
*/
int32_t IslamicCalendar::handleGetExtendedYear() {
    int32_t year;
    if (newerField(UCAL_EXTENDED_YEAR, UCAL_YEAR) == UCAL_EXTENDED_YEAR) {
        year = internalGet(UCAL_EXTENDED_YEAR, 1); // Default to year 1
    } else {
        year = internalGet(UCAL_YEAR, 1); // Default to year 1
    }
    return year;
}

/**
* Override Calendar to compute several fields specific to the Islamic
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
* method is called. The getGregorianXxx() methods return Gregorian
* calendar equivalents for the given Julian day.
* @draft ICU 2.4
*/
void IslamicCalendar::handleComputeFields(int32_t julianDay, UErrorCode &status) {
    int32_t year, month, dayOfMonth, dayOfYear;
    int32_t startDate;
    int32_t days = julianDay - CIVIL_EPOC;

    if (cType == CIVIL || cType == TBLA) {
        if(cType == TBLA) {
            days = julianDay - ASTRONOMICAL_EPOC;
        }
        // Use the civil calendar approximation, which is just arithmetic
        year  = (int32_t)ClockMath::floorDivide(30 * (int64_t)days + 10646, (int64_t)10631);
        month = (int32_t)uprv_ceil((days - 29 - yearStart(year)) / 29.5 );
        month = month<11?month:11;
        startDate = monthStart(year, month);
    } else if(cType == ASTRONOMICAL){
        // Guess at the number of elapsed full months since the epoch
        int32_t months = (int32_t)uprv_floor((double)days / CalendarAstronomer::SYNODIC_MONTH);

        startDate = (int32_t)uprv_floor(months * CalendarAstronomer::SYNODIC_MONTH);

        double age = moonAge(internalGetTime(), status);
        if (U_FAILURE(status)) {
            status = U_MEMORY_ALLOCATION_ERROR;
            return;
        }
        if ( days - startDate >= 25 && age > 0) {
            // If we're near the end of the month, assume next month and search backwards
            months++;
        }

        // Find out the last time that the new moon was actually visible at this longitude
        // This returns midnight the night that the moon was visible at sunset.
        while ((startDate = trueMonthStart(months)) > days) {
            // If it was after the date in question, back up a month and try again
            months--;
        }

        year = months / 12 + 1;
        month = months % 12;
    } else if(cType == UMALQURA) {
        int32_t umalquraStartdays = yearStart(UMALQURA_YEAR_START) ;
        if( days < umalquraStartdays){
                //Use Civil calculation
                year  = (int)ClockMath::floorDivide( (double)(30 * days + 10646) , 10631.0 );
                month = (int32_t)uprv_ceil((days - 29 - yearStart(year)) / 29.5 );
                month = month<11?month:11;
                startDate = monthStart(year, month);
            }else{
                int y =UMALQURA_YEAR_START-1, m =0;
                long d = 1;
                while(d > 0){
                    y++;
                    d = days - yearStart(y) +1;
                    if(d == handleGetYearLength(y)){
                        m=11;
                        break;
                    }else if(d < handleGetYearLength(y) ){
                        int monthLen = handleGetMonthLength(y, m);
                        m=0;
                        while(d > monthLen){
                            d -= monthLen;
                            m++;
                            monthLen = handleGetMonthLength(y, m);
                        }
                        break;
                    }
                }
                year = y;
                month = m;
            }
    } else { // invalid 'civil'
      UPRV_UNREACHABLE; // should not get here, out of range
    }

    dayOfMonth = (days - monthStart(year, month)) + 1;

    // Now figure out the day of the year.
    dayOfYear = (days - monthStart(year, 0)) + 1;


    internalSet(UCAL_ERA, 0);
    internalSet(UCAL_YEAR, year);
    internalSet(UCAL_EXTENDED_YEAR, year);
    internalSet(UCAL_MONTH, month);
    internalSet(UCAL_DAY_OF_MONTH, dayOfMonth);
    internalSet(UCAL_DAY_OF_YEAR, dayOfYear);
}

UBool
IslamicCalendar::inDaylightTime(UErrorCode& status) const
{
    // copied from GregorianCalendar
    if (U_FAILURE(status) || !getTimeZone().useDaylightTime())
        return FALSE;

    // Force an update of the state of the Calendar.
    ((IslamicCalendar*)this)->complete(status); // cast away const

    return (UBool)(U_SUCCESS(status) ? (internalGet(UCAL_DST_OFFSET) != 0) : FALSE);
}

/**
 * The system maintains a static default century start date and Year.  They are
 * initialized the first time they are used.  Once the system default century date
 * and year are set, they do not change.
 */
static UDate           gSystemDefaultCenturyStart       = DBL_MIN;
static int32_t         gSystemDefaultCenturyStartYear   = -1;
static icu::UInitOnce  gSystemDefaultCenturyInit        = U_INITONCE_INITIALIZER;


UBool IslamicCalendar::haveDefaultCentury() const
{
    return TRUE;
}

UDate IslamicCalendar::defaultCenturyStart() const
{
    // lazy-evaluate systemDefaultCenturyStart
    umtx_initOnce(gSystemDefaultCenturyInit, &initializeSystemDefaultCentury);
    return gSystemDefaultCenturyStart;
}

int32_t IslamicCalendar::defaultCenturyStartYear() const
{
    // lazy-evaluate systemDefaultCenturyStartYear
    umtx_initOnce(gSystemDefaultCenturyInit, &initializeSystemDefaultCentury);
    return gSystemDefaultCenturyStartYear;
}


U_CFUNC void U_CALLCONV
IslamicCalendar::initializeSystemDefaultCentury()
{
    // initialize systemDefaultCentury and systemDefaultCenturyYear based
    // on the current time.  They'll be set to 80 years before
    // the current time.
    UErrorCode status = U_ZERO_ERROR;
    IslamicCalendar calendar(Locale("@calendar=islamic-civil"),status);
    if (U_SUCCESS(status)) {
        calendar.setTime(Calendar::getNow(), status);
        calendar.add(UCAL_YEAR, -80, status);

        gSystemDefaultCenturyStart = calendar.getTime(status);
        gSystemDefaultCenturyStartYear = calendar.get(UCAL_YEAR, status);
    }
    // We have no recourse upon failure unless we want to propagate the failure
    // out.
}



UOBJECT_DEFINE_RTTI_IMPLEMENTATION(IslamicCalendar)

U_NAMESPACE_END

#endif
