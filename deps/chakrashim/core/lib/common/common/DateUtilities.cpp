//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonCommonPch.h"
#define ENABLE_INTSAFE_SIGNED_FUNCTIONS 1
#include <intsafe.h>

#include "Common\DaylightTimeHelper.h"
#include "Common\DateUtilities.h"

#include <Windows.Foundation.h>

namespace Js
{
    const INT64 DateUtilities::ticksPerMillisecond = 10000;
    const double DateUtilities::ticksPerMillisecondDouble = 10000.0;
    const INT64 DateUtilities::ticksPerSecond = ticksPerMillisecond * 1000;
    const INT64 DateUtilities::ticksPerMinute = ticksPerSecond * 60;
    const INT64 DateUtilities::ticksPerHour = ticksPerMinute * 60;
    const INT64 DateUtilities::ticksPerDay = ticksPerHour * 24;
    const INT64 DateUtilities::jsEpochMilliseconds = 11644473600000;
    const INT64 DateUtilities::jsEpochTicks = jsEpochMilliseconds * ticksPerMillisecond;

    // The day numbers for the months of a leap year.
    static const int g_rgday[12] =
    {
          0,  31,  60,  91, 121, 152,
        182, 213, 244, 274, 305, 335,
    };

    const double g_kdblJanuary1st1970 = 25569.0;

    const wchar_t g_rgpszDay[7][4] =
    {
        L"Sun",
        L"Mon",
        L"Tue",
        L"Wed",
        L"Thu",
        L"Fri",
        L"Sat"
    };

    const wchar_t g_rgpszMonth[12][4] =
    {
        L"Jan",
        L"Feb",
        L"Mar",
        L"Apr",
        L"May",
        L"Jun",
        L"Jul",
        L"Aug",
        L"Sep",
        L"Oct",
        L"Nov",
        L"Dec"
    };

    const wchar_t g_rgpszZone[8][4] =
    {
        L"EST",
        L"EDT",
        L"CST",
        L"CDT",
        L"MST",
        L"MDT",
        L"PST",
        L"PDT"
    };

    //
    // Convert a WinRT DateTime date (in 100ns precision ticks) to an ES5 date
    //
    // We convert ticks to milliseconds and shift by JS epoch to get the double date
    // We go in that order to skip doing underflow checks
    //
    HRESULT DateUtilities::WinRTDateToES5Date(INT64 ticks, __out double* pRet)
    {
        Assert(pRet != NULL);

        if (pRet == NULL)
        {
            return E_INVALIDARG;
        }

        // Divide as INT64 to ensure truncation of all decimal digits,
        // since any remaining after conversion will be truncated as a Date value.
        INT64 milliseconds = ticks / ticksPerMillisecond;

        (*pRet) = (double)(milliseconds - jsEpochMilliseconds);

        return S_OK;
    }

    //
    // Convert an ES5 date based on double to a WinRT DateTime
    // DateTime is the number of ticks that have elapsed since 1/1/1601 00:00:00 in 100ns precision
    // If we return a failure HRESULT other than E_INVALIDARG, the es5 date can't be expressed
    // in the WinRT scheme
    //
    HRESULT DateUtilities::ES5DateToWinRTDate(double es5Date, __out INT64* pRet)
    {
        Assert(pRet != NULL);

        if (pRet == NULL)
        {
            return E_INVALIDARG;
        }

        INT64 es5DateAsInt64 = NumberUtilities::TryToInt64(es5Date);

        if (!NumberUtilities::IsValidTryToInt64(es5DateAsInt64)) return INTSAFE_E_ARITHMETIC_OVERFLOW;

        INT64 numTicks;
        // We use the LongLong* functions since that's typedef'd to int64
        // First, we rebase it to the WinRT epoch, then we convert the time in milliseconds to ticks
        if (SUCCEEDED(::Int64Add(es5DateAsInt64, jsEpochMilliseconds, reinterpret_cast<LONGLONG*>(&numTicks))))
        {
            INT64 adjustedTicks = 0;
            if (SUCCEEDED(::Int64Mult(numTicks, ticksPerMillisecond, reinterpret_cast<LONGLONG*>(&adjustedTicks))))
            {
                (*pRet) = adjustedTicks;
                return S_OK;
            }
        }

         return INTSAFE_E_ARITHMETIC_OVERFLOW;
    }

    //
    // Version 6 Change:
    // Previously we would round the TimeSpan to ms precision in order to avoid having the double contain decimal digits.
    // Now we allow for the timespan to be represented with integers and digits (no truncation).
    //
    HRESULT DateUtilities::WinRTTimeSpanToNumberV6(INT64 ticks, __out double* pRet)
    {
        Assert(pRet != NULL);

        if (pRet == NULL)
        {
            return E_INVALIDARG;
        }

        // We want to preserve precision as best we could, and for low enough timespan values
        // Hence perform a division of doubles, to convert from ticks to milliseconds.
        double result = (double)ticks / ticksPerMillisecondDouble;
        *pRet = result;

        return S_OK;
    }

    //
    // Version 6 Change:
    // Same as for WinRTTimeSpanToNumberV6, remove truncation when converting between Number and WinRT TimeSpan.
    //
    HRESULT DateUtilities::NumberToWinRTTimeSpanV6(double span, __out INT64* pRet)
    {
        Assert(pRet != NULL);

        if (pRet == NULL)
        {
            return E_INVALIDARG;
        }

        //Otherwise the double multiplication might overflow
        if (span > MAXINT64 / ticksPerMillisecond)
        {
            return INTSAFE_E_ARITHMETIC_OVERFLOW;
        }

        //Multiply before converting to Int64, in order to get the 100-nanosecond precision which will get truncated
        INT64 spanAsInt64 = NumberUtilities::TryToInt64(span * ticksPerMillisecondDouble);

        if (!NumberUtilities::IsValidTryToInt64(spanAsInt64))
        {
            return INTSAFE_E_ARITHMETIC_OVERFLOW;
        }

        (*pRet) = spanAsInt64;
        return S_OK;
    }

    ///------------------------------------------------------------------------------
    /// Get a time value from SYSTEMTIME structure.
    ///
    /// Returns number of milliseconds since Jan 1, 1970
    ///------------------------------------------------------------------------------
    double
    DateUtilities::TimeFromSt(SYSTEMTIME *pst)
    {
        return TvFromDate(pst->wYear,pst->wMonth-1,pst->wDay-1, DayTimeFromSt(pst));
    }

    ///------------------------------------------------------------------------------
    /// Get a time value from SYSTEMTIME structure within a day
    ///
    /// Returns number of milliseconds since 12:00 AM
    ///------------------------------------------------------------------------------
    double
    DateUtilities::DayTimeFromSt(SYSTEMTIME *pst)
    {
        return (pst->wHour * 3600000.0) + (pst->wMinute * 60000.0) + (pst->wSecond * 1000.0) + pst->wMilliseconds;
    }

    ///------------------------------------------------------------------------------
    /// Get a time value from (year, mon, day, time) values.
    ///------------------------------------------------------------------------------
    double
    DateUtilities::TvFromDate(double year, double mon, double day, double time)
    {
        // For positive month, use fast path: '/' and '%' rather than 'floor()' and 'fmod()'.
        // But make sure there is no overflow when casting double -> int -- WOOB 1142298.
        if (mon >= 0 && mon <= INT_MAX)
        {
            year +=  ((int)mon) / 12;
            mon = ((int)mon) % 12;
        }
        else
        {
            year += floor(mon/12);
            mon = DblModPos(mon,12);
        }

        day += DayFromYear(year);

        AssertMsg(mon >= 0 && mon <= 11, "'mon' must be in the range of [0..11].");
        day += g_rgday[(int)mon];

        if (mon >= 2 && !FLeap((int)year))
        {
            day -= 1;
        }

        return day * 86400000 + time;
    }

    ///------------------------------------------------------------------------------
    /// Get the non-negative remainder.
    ///------------------------------------------------------------------------------
    double
    DateUtilities::DblModPos(double dbl, double dblDen)
    {
        AssertMsg(dblDen > 0, "value not positive");
        dbl = fmod(dbl, dblDen);
        if (dbl < 0)
        {
            dbl += dblDen;
        }
        AssertMsg(dbl >= 0 && dbl < dblDen, "");
        return dbl;
    }

    ///------------------------------------------------------------------------------
    /// DayFromYear is:
    ///    365 * y + floor((y+1)/4) - floor((y+69)/100) + floor((y+369)/400).
    /// where y is the calendar year minus 1970.
    ///------------------------------------------------------------------------------
    double
    DateUtilities::DayFromYear(double year)
    {
        double day = 365 * (year -= 1970);

        if (day > 0)
        {
            day += ((int)((year + 1) / 4)) - ((int)((year + 69) / 100)) +
                ((int)((year + 369) / 400));
        }
        else
        {
            day += floor((year + 1) / 4) - floor((year + 69) / 100) +
                floor((year + 369) / 400);
        }
        return day;
    }

    ///------------------------------------------------------------------------------
    /// Return whether the given year is a leap year.
    ///------------------------------------------------------------------------------
    bool
    DateUtilities::FLeap(int year)
    {
        return (0 == (year & 3)) && (0 != (year % 100) || 0 == (year % 400));
    }

    ///------------------------------------------------------------------------------
    /// Get the first day of the year, and if the first day is bigger than the passed day, then get the first day of the previous year.
    ///------------------------------------------------------------------------------
    /*static*/
    int DateUtilities::GetDayMinAndUpdateYear(int day, int &year)
    {
        int dayMin = (int)DayFromYear(year);
        if (day < dayMin)
        {
            year--;
            dayMin = (int)DayFromYear(year);
        }
        return dayMin;
    }

    ///------------------------------------------------------------------------------
    /// Converts the time value relative to Jan 1, 1970 into a YMD.
    ///
    /// The year number y and day number d relative to Jan 1, 1970 satisfy the
    /// inequalities:
    ///    floor((400*d-82)/146097) <= y <= floor((400*d+398)/146097)
    /// These inequalities get us within one of the correct answer for the year.
    /// We then use DayFromYear to adjust if necessary.
    ///------------------------------------------------------------------------------
    void
    DateUtilities::GetYmdFromTv(double tv, Js::YMD *pymd)
    {
//      AssertMem(pymd);

        int day;
        int dayMin;
        int yday;

        if (tv > 0)
        {
            day = (int)(tv / 86400000);
            pymd->time = (int)DblModPos(tv, 86400000);
            pymd->wday = (day + 4) % 7;

            pymd->year = 1970 + (int)((400 * (double)day + 398) / 146097);
            dayMin = GetDayMinAndUpdateYear(day, pymd->year);
            pymd->yt = (int)((dayMin + 4) % 7);

        }
        else
        {
            day = (int)floor(tv / 86400000);
            pymd->time = (int)DblModPos(tv, 86400000);
            pymd->wday = (int)DblModPos(day + 4, 7);

            pymd->year = 1970 + (int)floor(((400 * (double)day + 398) / 146097));
            dayMin = GetDayMinAndUpdateYear(day, pymd->year);
            pymd->yt = (int)DblModPos(dayMin + 4, 7);
        }
        yday = (int)(day - dayMin);
//      Assert(yday >= 0 && (yday < 365 || yday == 365 && FLeap(pymd->year)));
        pymd->yday = yday;

        if (FLeap(pymd->year))
        {
            pymd->yt += 7;
        }
        else if (yday >= 59)
        {
            yday++;
        }

        // Get the month.
        if (yday < 182)
        {
            if (yday < 60)
            {
                pymd->mon = 0 + ((yday >= 31) ? 1 : 0);
            }
            else if (yday < 121)
            {
                pymd->mon = 2 + ((yday >= 91) ? 1 : 0);
            }
            else
            {
                pymd->mon = 4 + ((yday >= 152) ? 1 : 0);
            }
        }
        else
        {
            if (yday < 244)
            {
                pymd->mon = 6 + ((yday >= 213) ? 1 : 0);
            }
            else if (yday < 305)
            {
                pymd->mon = 8 + ((yday >= 274) ? 1 : 0);
            }
            else
            {
                pymd->mon = 10 + ((yday >= 335) ? 1 : 0);
            }
        }
//      Assert(pymd->mon >= 0 && pymd->mon < 12);

        pymd->mday = yday - g_rgday[pymd->mon];
    }

    void DateUtilities::GetYearFromTv(double tv, int &year, int &yearType)
    {
        //      AssertMem(pymd);

        int day;
        int dayMin;

        if (tv > 0)
        {
            day = (int)(tv / 86400000);
            year = 1970 + (int)((400 * (double)day + 398) / 146097);
            dayMin = GetDayMinAndUpdateYear(day, year);
            yearType = (int)((dayMin + 4) % 7);

        }
        else
        {
            day = (int)floor(tv / 86400000);
            year = 1970 + (int)floor(((400 * (double)day + 398) / 146097));
            dayMin = GetDayMinAndUpdateYear(day, year);
            yearType = (int)DblModPos(dayMin + 4, 7);
        }

        if (FLeap(year))
        {
            yearType += 7;
        }
    }

    double DateUtilities::JsLocalTimeFromVarDate(double dbl)
    {
        // So that the arithmetic works even for negative dates, convert the
        // date to the _actual number of days_ since 0000h 12/30/1899.
        if (dbl < 0.0)
            dbl = 2.0 * ceil(dbl) - dbl;

        // Get the local time value.
        dbl = (dbl - g_kdblJanuary1st1970) * 86400000;
        if (NumberUtilities::IsNan(dbl))
        {
            return dbl;
        }
        return NumberUtilities::IsFinite(dbl) ? floor( dbl + 0.5) : dbl;
    }
}
