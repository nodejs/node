//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonCommonPch.h"

#include <time.h>

#include "Common\DaylightTimeHelper.h"
#include "Common\DateUtilities.h"

namespace Js {

    static const double TicksPerMinute = 60000.0;
    static const double TicksPerDay = TicksPerMinute * 60 * 24;
    static const double TicksPerlargestTZOffset = TicksPerMinute * 60 * 24 + 1;
    static const double TicksPerNonLeapYear = TicksPerDay * 365;
    static const double TicksPerSafeEndOfYear = TicksPerNonLeapYear - TicksPerlargestTZOffset;
    static const int daysInMonthLeap[] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    static const double criticalMin = DateUtilities::TvFromDate(1601, 0, 1, 0); // minimal year for which windows has time zone information
    static const double criticalMax = DateUtilities::TvFromDate(USHRT_MAX-1, 0, 0, 0);

    // The day numbers for the months of a leap year.
    static const int daysUpToMonthLeap[12] =
    {
          0,  31,  60,  91, 121, 152,
        182, 213, 244, 274, 305, 335,
    };

    typedef BOOL(*DateConversionFunction)(
        _In_opt_ CONST PVOID lpTimeZoneInformation,
        _In_ CONST SYSTEMTIME * lpLocalTime,
        _Out_ LPSYSTEMTIME lpUniversalTime
        );

    static HINSTANCE g_timezonedll = NULL;
    static DateConversionFunction sysLocalToUtc = NULL;
    static DateConversionFunction sysUtcToLocal = NULL;


    // Cache should be invalid at the moment of creation
    // if january1 > nextJanuary1 cache is always invalid, so we don't care about other fields, because cache will be updated.
    DaylightTimeHelper::TimeZoneInfo::TimeZoneInfo()
    {
        january1 = 1;
        nextJanuary1 = 0;
    }

    // Cache is valid for given time if this time is within a year for which cache was created, and cache was updated within 1 second of current moment
    bool DaylightTimeHelper::TimeZoneInfo::IsValid(double time)
    {
        return GetTickCount() - lastUpdateTickCount < updatePeriod && time >= january1 && time < nextJanuary1;
    }

    void DaylightTimeHelper::TimeZoneInfo::Update(double inputTime)
    {
        int year, yearType;
        DateUtilities::GetYearFromTv(inputTime, year, yearType);
        int yearForInfo = year;

        // GetTimeZoneInformationForYear() works only with years > 1600, but JS works with wider range of years. So we take year closest to given.
        if (year < 1601)
        {
            yearForInfo = 1601;
        }
        else if (year > 2100)
        {
            yearForInfo = 2100;
        }
        TIME_ZONE_INFORMATION timeZoneInfo;
        GetTimeZoneInformationForYear((USHORT)yearForInfo, NULL, &timeZoneInfo);
        isDaylightTimeApplicable = timeZoneInfo.StandardDate.wMonth != 0 && timeZoneInfo.DaylightDate.wMonth != 0;

        bias = timeZoneInfo.Bias;
        daylightBias = timeZoneInfo.DaylightBias;
        standardBias = timeZoneInfo.StandardBias;

        double day = DaylightTimeHelper::DayNumber(yearType, timeZoneInfo.DaylightDate);
        double time = DateUtilities::DayTimeFromSt(&timeZoneInfo.DaylightDate);
        daylightDate = DateUtilities::TvFromDate(year, timeZoneInfo.DaylightDate.wMonth-1, day-1, time);

        day = DayNumber(yearType, timeZoneInfo.StandardDate);
        time = DateUtilities::DayTimeFromSt(&timeZoneInfo.StandardDate);
        standardDate = DateUtilities::TvFromDate(year, timeZoneInfo.StandardDate.wMonth-1, day-1, time);

        GetTimeZoneInformationForYear((USHORT)yearForInfo-1, NULL, &timeZoneInfo);
        isJanuary1Critical = timeZoneInfo.Bias + timeZoneInfo.DaylightBias + timeZoneInfo.StandardBias != bias + daylightBias + standardBias;

        january1 = DateUtilities::TvFromDate(year, 0, 0, 0);
        nextJanuary1 = january1 + TicksPerNonLeapYear + DateUtilities::FLeap(year) * TicksPerDay;
        lastUpdateTickCount = GetTickCount();
    }

    DaylightTimeHelper::TimeZoneInfo* DaylightTimeHelper::GetTimeZoneInfo(double time)
    {
        if (cache1.IsValid(time)) return &cache1;
        if (cache2.IsValid(time)) return &cache2;

        if (useFirstCache)
        {
            cache1.Update(time);
            useFirstCache = false;
            return &cache1;
        }
        else
        {
            cache2.Update(time);
            useFirstCache = true;
            return &cache2;
        }
    }

    HINSTANCE DaylightTimeHelper::TryLoadLibrary()
    {
        if (g_timezonedll == NULL)
        {
            HMODULE hLocal = LoadLibraryExW(L"api-ms-win-core-timezone-l1-1-0.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
            if (hLocal != NULL)
            {
                if (InterlockedCompareExchangePointer((PVOID*) &g_timezonedll, hLocal, NULL) != NULL)
                {
                    FreeLibrary(hLocal);
                }
            }
        }

        if (g_timezonedll == NULL)
        {
            HMODULE hLocal = LoadLibraryExW(L"kernel32.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
            if (hLocal != NULL)
            {
                if (InterlockedCompareExchangePointer((PVOID*) &g_timezonedll, hLocal, NULL) != NULL)
                {
                    FreeLibrary(hLocal);
                }
            }
        }
        return g_timezonedll;
    }

    BOOL DaylightTimeHelper::SysLocalToUtc(SYSTEMTIME *local, SYSTEMTIME *utc)
    {
        if (sysLocalToUtc == NULL)
        {
            HINSTANCE library = TryLoadLibrary();
            if (library != NULL && !ForceOldDateAPIFlag())
            {
                sysLocalToUtc = (DateConversionFunction)GetProcAddress(library, "TzSpecificLocalTimeToSystemTimeEx");
            }
            if (sysLocalToUtc == NULL)
            {
                sysLocalToUtc = (DateConversionFunction)TzSpecificLocalTimeToSystemTime;
            }
        }
        return sysLocalToUtc(NULL, local, utc);
    }

    BOOL DaylightTimeHelper::SysUtcToLocal(SYSTEMTIME *utc, SYSTEMTIME *local)
    {
        if (sysUtcToLocal == NULL)
        {
            HINSTANCE library = TryLoadLibrary();
            if (library != NULL)
            {
                sysUtcToLocal = (DateConversionFunction)GetProcAddress(library, "SystemTimeToTzSpecificLocalTimeEx");
            }
            if (sysUtcToLocal == NULL)
            {
                sysUtcToLocal = (DateConversionFunction)SystemTimeToTzSpecificLocalTime;
            }
        }
        return sysUtcToLocal(NULL, utc, local);
    }

    int DaylightTimeHelper::DayNumber(int yearType, const SYSTEMTIME &date)
    {
        if (date.wYear == 0)
        {
            BOOL isLeap = yearType / 7; // yearType is a day of week of January 1st (number within range [0,6]) (+ 7 if year is a leap)
            int dayOfWeekOf1stOfMonth = (yearType + daysUpToMonthLeap[date.wMonth-1] - (int)(!isLeap && date.wMonth >= 3)) % 7;
            int numberOfDaysInThisMonth = daysInMonthLeap[date.wMonth-1] - (int)(!isLeap && date.wMonth == 2);
            int delta = date.wDayOfWeek - dayOfWeekOf1stOfMonth;
            return min((numberOfDaysInThisMonth - delta - 1) / 7, date.wDay - (int)(delta >= 0)) * 7 + delta + 1;
        }
        else
        {
            return date.wDay;
        }
    }

    bool DaylightTimeHelper::IsDaylightSavings(double time, bool utcTime, TimeZoneInfo *timeZoneInfo)
    {
        if (!timeZoneInfo->isDaylightTimeApplicable)
        {
            return false;
        }

        double localDaylight = timeZoneInfo->daylightDate;
        double localStandard = timeZoneInfo->standardDate;

        if (utcTime)
        {
            double biasInTicks = timeZoneInfo->bias * TicksPerMinute;
            localDaylight += biasInTicks + (timeZoneInfo->standardBias * TicksPerMinute);
            localStandard += biasInTicks + (timeZoneInfo->daylightBias * TicksPerMinute);
        }
        else
        {
            localDaylight -= (timeZoneInfo->daylightBias * TicksPerMinute);
            localStandard -= (timeZoneInfo->standardBias * TicksPerMinute);
        }

        return (localDaylight < localStandard)
                ? localDaylight <= time && time < localStandard
                : time < localStandard || localDaylight <= time;
    }

    // in slow path we use system API to perform conversion, but we still need to know whether current time is
    // standard or daylight savings in order to create a string representation of a date.
    // So just compare whether difference between local and utc time equal to bias.
    bool DaylightTimeHelper::IsDaylightSavings(double utcTime, double localTime, int bias)
    {
        return ((int)(utcTime - localTime)) / ((int)(TicksPerMinute)) != bias;
    }


    // This function does not properly handle boundary cases.
    // But while we use IsCritical we don't care about it.
    bool DaylightTimeHelper::IsDaylightSavingsUnsafe(double time, TimeZoneInfo *timeZoneInfo)
    {
        return timeZoneInfo->isDaylightTimeApplicable && ((timeZoneInfo->daylightDate < timeZoneInfo->standardDate)
            ? timeZoneInfo->daylightDate <= time && time < timeZoneInfo->standardDate
            : time < timeZoneInfo->standardDate || timeZoneInfo->daylightDate <= time);
    }

    void DaylightTimeHelper::YmdToSystemTime(YMD* ymd, SYSTEMTIME *sys)
    {
        sys->wYear = (WORD)ymd->year;
        sys->wMonth = (WORD)(ymd->mon + 1);
        sys->wDay =(WORD)(ymd->mday + 1);
        int time = ymd->time;
        sys->wMilliseconds = (WORD)(time % 1000);
        time /= 1000;
        sys->wSecond = (WORD)(time % 60);
        time /= 60;
        sys->wMinute = (WORD)(time % 60);
        time /= 60;
        sys->wHour = (WORD)time;
    }

    double DaylightTimeHelper::UtcToLocalFast(double utcTime, TimeZoneInfo *timeZoneInfo, int &bias, int &offset, bool &isDaylightSavings)
    {
        double localTime;
        localTime = utcTime - TicksPerMinute * timeZoneInfo->bias;
        isDaylightSavings = IsDaylightSavingsUnsafe(utcTime, timeZoneInfo);
        if (isDaylightSavings)
        {
            localTime -= TicksPerMinute * timeZoneInfo->daylightBias;
        } else {
            localTime -= TicksPerMinute * timeZoneInfo->standardBias;
        }

        bias = timeZoneInfo->bias;
        offset = ((int)(localTime - utcTime)) / ((int)(TicksPerMinute));

        return localTime;
    }

    double DaylightTimeHelper::UtcToLocalCritical(double utcTime, TimeZoneInfo *timeZoneInfo, int &bias, int &offset, bool &isDaylightSavings)
    {
        double localTime;
        SYSTEMTIME utcSystem, localSystem;
        YMD ymd;

        DateUtilities::GetYmdFromTv(utcTime, &ymd);
        YmdToSystemTime(&ymd, &utcSystem);

        if (!SysUtcToLocal(&utcSystem, &localSystem))
        {
            // SysUtcToLocal can fail if the date is beyond extreme internal boundaries (e.g. > ~30000 years).
            // Fall back to our fast (but less accurate) version if the call fails.
            return UtcToLocalFast(utcTime, timeZoneInfo, bias, offset, isDaylightSavings);
        }

        localTime = Js::DateUtilities::TimeFromSt(&localSystem);
        if (localSystem.wYear != utcSystem.wYear)
        {
            timeZoneInfo = GetTimeZoneInfo(localTime);
        }

        bias = timeZoneInfo->bias;
        isDaylightSavings = IsDaylightSavings(utcTime, localTime, timeZoneInfo->bias + timeZoneInfo->standardBias);
        offset = ((int)(localTime - utcTime)) / ((int)(TicksPerMinute));

        return localTime;
    }

    double DaylightTimeHelper::UtcToLocal(double utcTime, int &bias, int &offset, bool &isDaylightSavings)
    {
        TimeZoneInfo *timeZoneInfo = GetTimeZoneInfo(utcTime);

        if (IsCritical(utcTime, timeZoneInfo))
        {
            return UtcToLocalCritical(utcTime, timeZoneInfo, bias, offset, isDaylightSavings);
        }
        else
        {
            return UtcToLocalFast(utcTime, timeZoneInfo, bias, offset, isDaylightSavings);
        }
    }


    double DaylightTimeHelper::LocalToUtcFast(double localTime, TimeZoneInfo *timeZoneInfo)
    {
        double utcTime = localTime + TicksPerMinute * timeZoneInfo->bias;
        bool isDaylightSavings = IsDaylightSavingsUnsafe(localTime, timeZoneInfo);

        if (isDaylightSavings)
        {
            utcTime += TicksPerMinute * timeZoneInfo->daylightBias;
        } else {
            utcTime += TicksPerMinute * timeZoneInfo->standardBias;
        }

        return utcTime;
    }

    double DaylightTimeHelper::LocalToUtcCritical(double localTime, TimeZoneInfo *timeZoneInfo)
    {
        SYSTEMTIME localSystem, utcSystem;
        YMD ymd;

        DateUtilities::GetYmdFromTv(localTime, &ymd);
        YmdToSystemTime(&ymd, &localSystem);

        if (!SysLocalToUtc(&localSystem, &utcSystem))
        {
            // Fall back to our fast (but less accurate) version if the call fails.
            return LocalToUtcFast(localTime, timeZoneInfo);
        }

        return Js::DateUtilities::TimeFromSt(&utcSystem);
    }

    double DaylightTimeHelper::LocalToUtc(double localTime)
    {
        TimeZoneInfo *timeZoneInfo = GetTimeZoneInfo(localTime);

        if (IsCritical(localTime, timeZoneInfo))
        {
            return LocalToUtcCritical(localTime, timeZoneInfo);
        }
        else
        {
            return LocalToUtcFast(localTime, timeZoneInfo);
        }
    }

    // we consider January 1st, December 31st and days when daylight savings time starts and ands to be critical,
    // because there might be ambiguous cases in local->utc->local conversions,
    // so in order to be consistent with Windows we rely on it to perform conversions. But it is slow.
    bool DaylightTimeHelper::IsCritical(double time, TimeZoneInfo *timeZoneInfo)
    {
        return time > criticalMin && time < criticalMax &&
            (abs(time - timeZoneInfo->daylightDate) < TicksPerlargestTZOffset ||
            abs(time - timeZoneInfo->standardDate) < TicksPerlargestTZOffset ||
            time > timeZoneInfo->january1 + TicksPerSafeEndOfYear ||
            (timeZoneInfo->isJanuary1Critical && time - timeZoneInfo->january1 < TicksPerlargestTZOffset));
     }


} // namespace Js
