// Copyright (C) 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
********************************************************************************
*   Copyright (C) 2009-2013, International Business Machines
*   Corporation and others.  All Rights Reserved.
********************************************************************************
*
* File WINTZIMPL.CPP
*
********************************************************************************
*/

#include "unicode/utypes.h"

#if U_PLATFORM_HAS_WIN32_API && !UCONFIG_NO_FORMATTING 

#include "wintzimpl.h"

#include "unicode/unistr.h"
#include "unicode/timezone.h"
#include "unicode/basictz.h"
#include "putilimp.h"
#include "uassert.h"
#include "cmemory.h"

#   define WIN32_LEAN_AND_MEAN
#   define VC_EXTRALEAN
#   define NOUSER
#   define NOSERVICE
#   define NOIME
#   define NOMCX

#include <windows.h>

U_NAMESPACE_USE

static UBool getSystemTimeInformation(TimeZone *tz, SYSTEMTIME &daylightDate, SYSTEMTIME &standardDate, int32_t &bias, int32_t &daylightBias, int32_t &standardBias) {
    UErrorCode status = U_ZERO_ERROR;
    UBool result = TRUE;
    BasicTimeZone *btz = (BasicTimeZone*)tz; // we should check type
    InitialTimeZoneRule *initial = NULL;
    AnnualTimeZoneRule *std = NULL, *dst = NULL;

    btz->getSimpleRulesNear(uprv_getUTCtime(), initial, std, dst, status);
    if (U_SUCCESS(status)) {
        if (std == NULL || dst == NULL) {
            bias = -1 * (initial->getRawOffset()/60000);
            standardBias = 0;
            daylightBias = 0;
            // Do not use DST.  Set 0 to all stadardDate/daylightDate fields
            standardDate.wYear = standardDate.wMonth  = standardDate.wDayOfWeek = standardDate.wDay = 
            standardDate.wHour = standardDate.wMinute = standardDate.wSecond    = standardDate.wMilliseconds = 0;
            daylightDate.wYear = daylightDate.wMonth  = daylightDate.wDayOfWeek = daylightDate.wDay =
            daylightDate.wHour = daylightDate.wMinute = daylightDate.wSecond    = daylightDate.wMilliseconds = 0;
        } else {
            U_ASSERT(std->getRule()->getDateRuleType() == DateTimeRule::DOW);
            U_ASSERT(dst->getRule()->getDateRuleType() == DateTimeRule::DOW);

            bias = -1 * (std->getRawOffset()/60000);
            standardBias = 0;
            daylightBias = -1 * (dst->getDSTSavings()/60000);
            // Always use DOW type rule
            int32_t hour, min, sec, mil;
            standardDate.wYear = 0;
            standardDate.wMonth = std->getRule()->getRuleMonth() + 1;
            standardDate.wDay = std->getRule()->getRuleWeekInMonth();
            if (standardDate.wDay < 0) {
                standardDate.wDay = 5;
            }
            standardDate.wDayOfWeek = std->getRule()->getRuleDayOfWeek() - 1;

            mil = std->getRule()->getRuleMillisInDay();
            hour = mil/3600000;
            mil %= 3600000;
            min = mil/60000;
            mil %= 60000;
            sec = mil/1000;
            mil %= 1000;

            standardDate.wHour = hour;
            standardDate.wMinute = min;
            standardDate.wSecond = sec;
            standardDate.wMilliseconds = mil;

            daylightDate.wYear = 0;
            daylightDate.wMonth = dst->getRule()->getRuleMonth() + 1;
            daylightDate.wDay = dst->getRule()->getRuleWeekInMonth();
            if (daylightDate.wDay < 0) {
                daylightDate.wDay = 5;
            }
            daylightDate.wDayOfWeek = dst->getRule()->getRuleDayOfWeek() - 1;

            mil = dst->getRule()->getRuleMillisInDay();
            hour = mil/3600000;
            mil %= 3600000;
            min = mil/60000;
            mil %= 60000;
            sec = mil/1000;
            mil %= 1000;

            daylightDate.wHour = hour;
            daylightDate.wMinute = min;
            daylightDate.wSecond = sec;
            daylightDate.wMilliseconds = mil;
        }
    } else {
        result = FALSE;
    }

    delete initial;
    delete std;
    delete dst;

    return result;
}

static UBool getWindowsTimeZoneInfo(TIME_ZONE_INFORMATION *zoneInfo, const UChar *icuid, int32_t length) {
    UBool result = FALSE;
    UnicodeString id = UnicodeString(icuid, length);
    TimeZone *tz = TimeZone::createTimeZone(id);
    
    if (tz != NULL) {
        int32_t bias;
        int32_t daylightBias;
        int32_t standardBias;
        SYSTEMTIME daylightDate;
        SYSTEMTIME standardDate;

        if (getSystemTimeInformation(tz, daylightDate, standardDate, bias, daylightBias, standardBias)) {
            uprv_memset(zoneInfo, 0, sizeof(TIME_ZONE_INFORMATION)); // We do not set standard/daylight names, so nullify first.
            zoneInfo->Bias          = bias;
            zoneInfo->DaylightBias  = daylightBias;
            zoneInfo->StandardBias  = standardBias;
            zoneInfo->DaylightDate  = daylightDate;
            zoneInfo->StandardDate  = standardDate;

            result = TRUE;
        }
    }

    return result;
}

/*
 * Given the timezone icuid, fill in zoneInfo by calling auxillary functions that creates a timezone and extract the 
 * information to put into zoneInfo. This includes bias and standard time date and daylight saving date.
 */
U_CAPI UBool U_EXPORT2
uprv_getWindowsTimeZoneInfo(TIME_ZONE_INFORMATION *zoneInfo, const UChar *icuid, int32_t length)
{
    if (getWindowsTimeZoneInfo(zoneInfo, icuid, length)) {
        return TRUE;
    } else {
        return FALSE;
    }
}

#endif
