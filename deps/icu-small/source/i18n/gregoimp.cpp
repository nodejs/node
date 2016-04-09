/*
 **********************************************************************
 * Copyright (c) 2003-2008, International Business Machines
 * Corporation and others.  All Rights Reserved.
 **********************************************************************
 * Author: Alan Liu
 * Created: September 2 2003
 * Since: ICU 2.8
 **********************************************************************
 */

#include "gregoimp.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/ucal.h"
#include "uresimp.h"
#include "cstring.h"
#include "uassert.h"

#if defined(U_DEBUG_CALDATA)
#include <stdio.h>
#endif

U_NAMESPACE_BEGIN

int32_t ClockMath::floorDivide(int32_t numerator, int32_t denominator) {
    return (numerator >= 0) ?
        numerator / denominator : ((numerator + 1) / denominator) - 1;
}

int32_t ClockMath::floorDivide(double numerator, int32_t denominator,
                          int32_t& remainder) {
    double quotient;
    quotient = uprv_floor(numerator / denominator);
    remainder = (int32_t) (numerator - (quotient * denominator));
    return (int32_t) quotient;
}

double ClockMath::floorDivide(double dividend, double divisor,
                         double& remainder) {
    // Only designed to work for positive divisors
    U_ASSERT(divisor > 0);
    double quotient = floorDivide(dividend, divisor);
    remainder = dividend - (quotient * divisor);
    // N.B. For certain large dividends, on certain platforms, there
    // is a bug such that the quotient is off by one.  If you doubt
    // this to be true, set a breakpoint below and run cintltst.
    if (remainder < 0 || remainder >= divisor) {
        // E.g. 6.7317038241449352e+022 / 86400000.0 is wrong on my
        // machine (too high by one).  4.1792057231752762e+024 /
        // 86400000.0 is wrong the other way (too low).
        double q = quotient;
        quotient += (remainder < 0) ? -1 : +1;
        if (q == quotient) {
            // For quotients > ~2^53, we won't be able to add or
            // subtract one, since the LSB of the mantissa will be >
            // 2^0; that is, the exponent (base 2) will be larger than
            // the length, in bits, of the mantissa.  In that case, we
            // can't give a correct answer, so we set the remainder to
            // zero.  This has the desired effect of making extreme
            // values give back an approximate answer rather than
            // crashing.  For example, UDate values above a ~10^25
            // might all have a time of midnight.
            remainder = 0;
        } else {
            remainder = dividend - (quotient * divisor);
        }
    }
    U_ASSERT(0 <= remainder && remainder < divisor);
    return quotient;
}

const int32_t JULIAN_1_CE    = 1721426; // January 1, 1 CE Gregorian
const int32_t JULIAN_1970_CE = 2440588; // January 1, 1970 CE Gregorian

const int16_t Grego::DAYS_BEFORE[24] =
    {0,31,59,90,120,151,181,212,243,273,304,334,
     0,31,60,91,121,152,182,213,244,274,305,335};

const int8_t Grego::MONTH_LENGTH[24] =
    {31,28,31,30,31,30,31,31,30,31,30,31,
     31,29,31,30,31,30,31,31,30,31,30,31};

double Grego::fieldsToDay(int32_t year, int32_t month, int32_t dom) {

    int32_t y = year - 1;

    double julian = 365 * y + ClockMath::floorDivide(y, 4) + (JULIAN_1_CE - 3) + // Julian cal
        ClockMath::floorDivide(y, 400) - ClockMath::floorDivide(y, 100) + 2 + // => Gregorian cal
        DAYS_BEFORE[month + (isLeapYear(year) ? 12 : 0)] + dom; // => month/dom

    return julian - JULIAN_1970_CE; // JD => epoch day
}

void Grego::dayToFields(double day, int32_t& year, int32_t& month,
                        int32_t& dom, int32_t& dow, int32_t& doy) {

    // Convert from 1970 CE epoch to 1 CE epoch (Gregorian calendar)
    day += JULIAN_1970_CE - JULIAN_1_CE;

    // Convert from the day number to the multiple radix
    // representation.  We use 400-year, 100-year, and 4-year cycles.
    // For example, the 4-year cycle has 4 years + 1 leap day; giving
    // 1461 == 365*4 + 1 days.
    int32_t n400 = ClockMath::floorDivide(day, 146097, doy); // 400-year cycle length
    int32_t n100 = ClockMath::floorDivide(doy, 36524, doy); // 100-year cycle length
    int32_t n4   = ClockMath::floorDivide(doy, 1461, doy); // 4-year cycle length
    int32_t n1   = ClockMath::floorDivide(doy, 365, doy);
    year = 400*n400 + 100*n100 + 4*n4 + n1;
    if (n100 == 4 || n1 == 4) {
        doy = 365; // Dec 31 at end of 4- or 400-year cycle
    } else {
        ++year;
    }

    UBool isLeap = isLeapYear(year);

    // Gregorian day zero is a Monday.
    dow = (int32_t) uprv_fmod(day + 1, 7);
    dow += (dow < 0) ? (UCAL_SUNDAY + 7) : UCAL_SUNDAY;

    // Common Julian/Gregorian calculation
    int32_t correction = 0;
    int32_t march1 = isLeap ? 60 : 59; // zero-based DOY for March 1
    if (doy >= march1) {
        correction = isLeap ? 1 : 2;
    }
    month = (12 * (doy + correction) + 6) / 367; // zero-based month
    dom = doy - DAYS_BEFORE[month + (isLeap ? 12 : 0)] + 1; // one-based DOM
    doy++; // one-based doy
}

void Grego::timeToFields(UDate time, int32_t& year, int32_t& month,
                        int32_t& dom, int32_t& dow, int32_t& doy, int32_t& mid) {
    double millisInDay;
    double day = ClockMath::floorDivide((double)time, (double)U_MILLIS_PER_DAY, millisInDay);
    mid = (int32_t)millisInDay;
    dayToFields(day, year, month, dom, dow, doy);
}

int32_t Grego::dayOfWeek(double day) {
    int32_t dow;
    ClockMath::floorDivide(day + UCAL_THURSDAY, 7, dow);
    return (dow == 0) ? UCAL_SATURDAY : dow;
}

int32_t Grego::dayOfWeekInMonth(int32_t year, int32_t month, int32_t dom) {
    int32_t weekInMonth = (dom + 6)/7;
    if (weekInMonth == 4) {
        if (dom + 7 > monthLength(year, month)) {
            weekInMonth = -1;
        }
    } else if (weekInMonth == 5) {
        weekInMonth = -1;
    }
    return weekInMonth;
}

/* ---- CalendarData ------ */

#define U_CALENDAR_KEY "calendar"
#define U_GREGORIAN_KEY "gregorian"
#define U_FORMAT_KEY "format"
#define U_DEFAULT_KEY "default"
#define U_CALENDAR_DATA ((char*)0)


// CalendarData::CalendarData(const Locale& loc, UErrorCode& status)
//   : fFillin(NULL), fBundle(NULL), fFallback(NULL) {
//   initData(loc.getBaseName(), (char*) "???", status);
// }

CalendarData::CalendarData(const Locale& loc, const char *type, UErrorCode& status)
  : fFillin(NULL), fOtherFillin(NULL), fBundle(NULL), fFallback(NULL) {
  initData(loc.getBaseName(), type, status);
}

void CalendarData::initData(const char *locale, const char *type, UErrorCode& status) {
  fOtherFillin = ures_open(U_CALENDAR_DATA, locale, &status);
  fFillin = ures_getByKey(fOtherFillin, U_CALENDAR_KEY, fFillin, &status);

  if((type != NULL) &&
     (*type != '\0') &&
     (uprv_strcmp(type, U_GREGORIAN_KEY)))
  {
    fBundle = ures_getByKeyWithFallback(fFillin, type, NULL, &status);
    fFallback = ures_getByKeyWithFallback(fFillin, U_GREGORIAN_KEY, NULL, &status);

#if defined (U_DEBUG_CALDATA)
    fprintf(stderr, "%p: CalendarData(%s, %s, %s) -> main(%p, %s)=%s, fallback(%p, %s)=%s\n",
            this, locale, type, u_errorName(status), fBundle, type, fBundle?ures_getLocale(fBundle, &status):"",
            fFallback, U_GREGORIAN_KEY, fFallback?ures_getLocale(fFallback, &status):"");
#endif

  } else {
    fBundle = ures_getByKeyWithFallback(fFillin, U_GREGORIAN_KEY, NULL, &status);
#if defined (U_DEBUG_CALDATA)
    fprintf(stderr, "%p: CalendarData(%s, %s, %s) -> main(%p, %s)=%s, fallback = NULL\n",
            this, locale, type, u_errorName(status), fBundle, U_GREGORIAN_KEY, fBundle?ures_getLocale(fBundle, &status):"" );
#endif
  }
}

CalendarData::~CalendarData() {
    ures_close(fFillin);
    ures_close(fBundle);
    ures_close(fFallback);
    ures_close(fOtherFillin);
}

UResourceBundle*
CalendarData::getByKey(const char *key, UErrorCode& status) {
    if(U_FAILURE(status)) {
        return NULL;
    }

    if(fBundle) {
        fFillin = ures_getByKeyWithFallback(fBundle, key, fFillin, &status);
#if defined (U_DEBUG_CALDATA)
        fprintf(stderr, "%p: get %s -> %s - from MAIN %s\n",this, key, u_errorName(status), ures_getLocale(fFillin, &status));
#endif
    }
    if(fFallback && (status == U_MISSING_RESOURCE_ERROR)) {
        status = U_ZERO_ERROR; // retry with fallback (gregorian)
        fFillin = ures_getByKeyWithFallback(fFallback, key, fFillin, &status);
#if defined (U_DEBUG_CALDATA)
        fprintf(stderr, "%p: get %s -> %s - from FALLBACK %s\n",this, key, u_errorName(status), ures_getLocale(fFillin, &status));
#endif
    }
    return fFillin;
}

UResourceBundle* CalendarData::getByKey2(const char *key, const char *subKey, UErrorCode& status) {
    if(U_FAILURE(status)) {
        return NULL;
    }

    if(fBundle) {
#if defined (U_DEBUG_CALDATA)
        fprintf(stderr, "%p: //\n");
#endif
        fFillin = ures_getByKeyWithFallback(fBundle, key, fFillin, &status);
        fOtherFillin = ures_getByKeyWithFallback(fFillin, U_FORMAT_KEY, fOtherFillin, &status);
        fFillin = ures_getByKeyWithFallback(fOtherFillin, subKey, fFillin, &status);
#if defined (U_DEBUG_CALDATA)
        fprintf(stderr, "%p: get %s/format/%s -> %s - from MAIN %s\n", this, key, subKey, u_errorName(status), ures_getLocale(fFillin, &status));
#endif
    }
    if(fFallback && (status == U_MISSING_RESOURCE_ERROR)) {
        status = U_ZERO_ERROR; // retry with fallback (gregorian)
        fFillin = ures_getByKeyWithFallback(fFallback, key, fFillin, &status);
        fOtherFillin = ures_getByKeyWithFallback(fFillin, U_FORMAT_KEY, fOtherFillin, &status);
        fFillin = ures_getByKeyWithFallback(fOtherFillin, subKey, fFillin, &status);
#if defined (U_DEBUG_CALDATA)
        fprintf(stderr, "%p: get %s/format/%s -> %s - from FALLBACK %s\n",this, key, subKey, u_errorName(status), ures_getLocale(fFillin,&status));
#endif
    }

//// handling of 'default' keyword on failure: Commented out for 3.0.
//   if((status == U_MISSING_RESOURCE_ERROR) &&
//      uprv_strcmp(subKey,U_DEFAULT_KEY)) { // avoid recursion
// #if defined (U_DEBUG_CALDATA)
//     fprintf(stderr, "%p: - attempting fallback -\n", this);
//     fflush(stderr);
// #endif
//     UErrorCode subStatus = U_ZERO_ERROR;
//     int32_t len;
//     char kwBuf[128] = "";
//     const UChar *kw;
//     /* fFillin = */ getByKey2(key, U_DEFAULT_KEY, subStatus);
//     kw = ures_getString(fFillin, &len, &subStatus);
//     if(len>126) { // too big
//       len = 0;
//     }
//     if(U_SUCCESS(subStatus) && (len>0)) {
//       u_UCharsToChars(kw, kwBuf, len+1);
//       if(*kwBuf && uprv_strcmp(kwBuf,subKey)) {
// #if defined (U_DEBUG_CALDATA)
//         fprintf(stderr, "%p: trying  %s/format/default -> \"%s\"\n",this, key, kwBuf);
// #endif
//         // now try again with the default
//         status = U_ZERO_ERROR;
//         /* fFillin = */ getByKey2(key, kwBuf, status);
//       }
// #if defined (U_DEBUG_CALDATA)
//     } else {
//       fprintf(stderr, "%p: could not load  %s/format/default  - fail out (%s)\n",this, key, kwBuf, u_errorName(status));
// #endif
//     }
//   }

    return fFillin;
}

UResourceBundle* CalendarData::getByKey3(const char *key, const char *contextKey, const char *subKey, UErrorCode& status) {
    if(U_FAILURE(status)) {
        return NULL;
    }

    if(fBundle) {
#if defined (U_DEBUG_CALDATA)
        fprintf(stderr, "%p: //\n");
#endif
        fFillin = ures_getByKeyWithFallback(fBundle, key, fFillin, &status);
        fOtherFillin = ures_getByKeyWithFallback(fFillin, contextKey, fOtherFillin, &status);
        fFillin = ures_getByKeyWithFallback(fOtherFillin, subKey, fFillin, &status);
#if defined (U_DEBUG_CALDATA)
        fprintf(stderr, "%p: get %s/%s/%s -> %s - from MAIN %s\n", this, key, contextKey, subKey, u_errorName(status), ures_getLocale(fFillin, &status));
#endif
    }
    if(fFallback && (status == U_MISSING_RESOURCE_ERROR)) {
        status = U_ZERO_ERROR; // retry with fallback (gregorian)
        fFillin = ures_getByKeyWithFallback(fFallback, key, fFillin, &status);
        fOtherFillin = ures_getByKeyWithFallback(fFillin, contextKey, fOtherFillin, &status);
        fFillin = ures_getByKeyWithFallback(fOtherFillin, subKey, fFillin, &status);
#if defined (U_DEBUG_CALDATA)
        fprintf(stderr, "%p: get %s/%s/%s -> %s - from FALLBACK %s\n",this, key, contextKey, subKey, u_errorName(status), ures_getLocale(fFillin,&status));
#endif
    }

    return fFillin;
}

U_NAMESPACE_END

#endif
//eof
