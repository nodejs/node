//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    // Global date constants
    extern const double g_kdblJanuary1st1970;
    extern const wchar_t g_rgpszDay[7][4];
    extern const wchar_t g_rgpszMonth[12][4];
    extern const wchar_t g_rgpszZone[8][4];

    // Utility methods to manipulate various date formats
    class DateUtilities
    {
        static const INT64 ticksPerMillisecond;
        static const double ticksPerMillisecondDouble;
        static const INT64 ticksPerSecond;
        static const INT64 ticksPerMinute;
        static const INT64 ticksPerHour;
        static const INT64 ticksPerDay;
        static const INT64 jsEpochTicks;
        static const INT64 jsEpochMilliseconds;

    public:
        static HRESULT WinRTDateToES5Date(INT64 winrtDate, __out double* pResult);
        static HRESULT ES5DateToWinRTDate(double es5Date, __out INT64* pResult);
        static HRESULT WinRTTimeSpanToNumberV6(INT64 span, __out double* pResult);
        static HRESULT NumberToWinRTTimeSpanV6(double span, __out INT64* pResult);       

        static double TimeFromSt(SYSTEMTIME *pst);
        static double DayTimeFromSt(SYSTEMTIME *pst);
        static double TvFromDate(double year, double mon, double day, double time);
        static double DblModPos(double dbl, double dblDen);
        static double DayFromYear(double year);
        static int GetDayMinAndUpdateYear(int day, int &year);
        static bool FLeap(int year);

        static void GetYmdFromTv(double tv, Js::YMD *pymd);
        static void GetYearFromTv(double tv, int &year, int &yearType);

        // Used for VT_DATE conversions
        static double JsLocalTimeFromVarDate(double dbl);
    };

}
