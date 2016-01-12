//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once


namespace Js {
            // Decomposed date (Year-Month-Date).
    struct YMD
    {
        int                 year; // year
        int                 yt;   // year type: wkd of Jan 1 (plus 7 if a leap year).
        int                 mon;  // month (0 to 11)
        int                 mday; // day in month (0 to 30)
        int                 yday; // day in year (0 to 365)
        int                 wday; // week day (0 to 6)
        int                 time; // time of day (in milliseconds: 0 to 86399999)
    };

    class DaylightTimeHelper
    {
    private:
        static const uint updatePeriod = 1000;
        class TimeZoneInfo
        {
        public:
            double daylightDate;
            double standardDate;
            double january1;
            double nextJanuary1;
            LONG daylightBias;
            LONG standardBias;
            LONG bias;
            uint lastUpdateTickCount;
            bool isDaylightTimeApplicable;
            bool isJanuary1Critical;
            TimeZoneInfo();
            bool IsValid(double time);
            void Update(double time);
        };
        TimeZoneInfo cache1, cache2;
        bool useFirstCache;

        static HINSTANCE TryLoadLibrary();

        static BOOL SysLocalToUtc(SYSTEMTIME *local, SYSTEMTIME *utc);
        static BOOL SysUtcToLocal(SYSTEMTIME *utc, SYSTEMTIME *local);

        static inline int DayNumber(int yearType, const SYSTEMTIME &date);
        static bool IsDaylightSavings(double time, bool isUtcTime, TimeZoneInfo *timeZoneInfo);
        static bool IsDaylightSavings(double utcTime, double localTime, int bias);
        static bool IsDaylightSavingsUnsafe(double time, TimeZoneInfo *timeZoneInfo);
        static inline bool IsCritical(double time, TimeZoneInfo *timeZoneInfo);
        TimeZoneInfo *GetTimeZoneInfo(double time);

    public:
        double UtcToLocal(double utcTime, int &bias, int &offset, bool &isDaylightSavings);
        double LocalToUtc(double time);
        static void YmdToSystemTime(YMD* ymd, SYSTEMTIME *sys);

        // Moved DaylightTimeHelper to common.lib to share with hybrid debugging. However this function depends
        // on runtime. Runtime and hybrid debugging needs to provide implementation.
        static bool ForceOldDateAPIFlag();

    private:
        static inline double UtcToLocalFast(double utcTime, TimeZoneInfo *timeZoneInfo, int &bias, int &offset, bool &isDaylightSavings);
               inline double UtcToLocalCritical(double utcTime, TimeZoneInfo *timeZoneInfo, int &bias, int &offset, bool &isDaylightSavings);
        static inline double LocalToUtcFast(double localTime, TimeZoneInfo *timeZoneInfo);
        static inline double LocalToUtcCritical(double localTime, TimeZoneInfo *timeZoneInfo);
    };
} // namespace Js
