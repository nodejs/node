//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once
struct _SYSTEMTIME;

namespace Js {
    struct SZS;

    class DateImplementation:
        public DateUtilities
    {
        friend class JavascriptDate;
        friend class JavascriptVariantDate;

        typedef struct tm TM;
        static const short kpstDstRuleChangeYear = 2007;
        static const double ktvMax;
        static const double ktvMin;
        static const int g_mpytyear[14];
        static const int g_mpytyearpost2006[14] ;

        double GetMilliSeconds();
        static double NowInMilliSeconds(ScriptContext* scriptContext);
        static double NowFromHiResTimer(ScriptContext * scriptContext);
        static double UtcTimeFromStr(ScriptContext *scriptContext, JavascriptString *pParseString);
        static double DoubleToTvUtc(double tv);
    private:
        DateImplementation(VirtualTableInfoCtorEnum) { m_modified = false; }
        DateImplementation(double value, ScriptContext* scriptContext);

        BEGIN_ENUM_BYTE(DateStringFormat)
            Default,
            Locale,
            GMT,
            Lim,
            END_ENUM_BYTE()

            BEGIN_ENUM_BYTE(DateValueType)
            Local             = 1, // Whether tvLcl and tzd are valid.
            YearMonthDayLocal = 2,
            YearMonthDayUTC   = 4,
            NotNaN            = 8,
            END_ENUM_BYTE()

            // date data
            BEGIN_ENUM_BYTE(DateData)
            // WARNING: There are tables that depend on this order.
            Year,
            FullYear,
            Month,
            Date,
            Hours,
            Minutes,
            Seconds,
            Milliseconds,
            Day,
            TimezoneOffset,
            Lim,
            END_ENUM_BYTE()

    public:

        // Flags which control if date or time or both need to be included.
        BEGIN_ENUM_BYTE(DateTimeFlag)
            None   = 0x00,
            NoTime = 0x01,
            NoDate = 0x02,
            END_ENUM_BYTE()

        // Time zone descriptor.
        struct TZD
        {
            int minutes;
            int offset;

            // Indicates whether Daylight savings
            bool fDst;
        };

        template <class ScriptContext>
        static long GetDaylightBias(const TIME_ZONE_INFORMATION *const pTz, const ScriptContext *const scriptContext);
        template <class ScriptContext>
        static long GetStandardBias(const TIME_ZONE_INFORMATION *const pTz, const ScriptContext *const scriptContext);

        template <class ScriptContext>
        static double GetTvLcl(double tv, ScriptContext * scriptContext, TZD *ptzd = nullptr);
        template <class ScriptContext>
        static double GetTvUtc(double tv, ScriptContext * scriptContext);
        static boolean UtcTimeFromStrCore(
            __in_ecount_z(ulength) const wchar_t *psz,
            unsigned int ulength,
            double &retVal,
            ScriptContext * const scriptContext);

        // Used for VT_DATE conversions
        //------------------------------------
        //Convert a utc time to a variant date.
        //------------------------------------
        static double VarDateFromJsUtcTime(double dbl, ScriptContext * scriptContext);
        static double JsUtcTimeFromVarDate(double dbl, ScriptContext *scriptContext);

        void SetTvUtc(double tv);
        bool IsModified() { return m_modified; }
        void ClearModified() { m_modified = false; }

    private:
        JavascriptString* GetString(DateStringFormat dsf, DateTimeFlag noDateTime = DateTimeFlag::None);

        JavascriptString* GetISOString();
        void GetDateComponent(CompoundString *bs, DateData componentType, int adjust);

        void SetTvLcl(double tv);

        double GetDateData(DateData dd, bool fUtc, ScriptContext* scriptContext);
        double SetDateData(Arguments args, DateData dd, bool fUtc, ScriptContext* scriptContext);

        static bool TryParseDecimalDigits(
            const wchar_t *const str,
            const size_t length,
            const size_t startIndex,
            size_t numDigits,
            int &value);
        static bool TryParseMilliseconds(
            const wchar_t *const str,
            const size_t length,
            const size_t startIndex,
            int &value,
            size_t &foundDigits);
        static bool TryParseTwoDecimalDigits(
            const wchar_t *const str,
            const size_t length,
            const size_t startIndex,
            int &value,
            bool canHaveTrailingDigit = false);

        // Tries to parse the string as an ISO-format date/time as defined in the spec. Returns false if the string is not in
        // ISO format.
        static bool TryParseIsoString(const wchar_t *const str, const size_t length, double &timeValue, ScriptContext *scriptContext);

        static JavascriptString* ConvertVariantDateToString(double variantDateDouble, ScriptContext* scriptContext);
        static JavascriptString* GetDateDefaultString(Js::YMD *pymd, TZD *ptzd,DateTimeFlag noDateTime,ScriptContext* scriptContext);
        static JavascriptString* GetDateGmtString(Js::YMD *pymd,ScriptContext* scriptContext);
        static JavascriptString* GetDateLocaleString(Js::YMD *pymd, TZD *ptzd, DateTimeFlag noDateTime,ScriptContext* scriptContext);

        static double DateFncUTC(ScriptContext* scriptContext, Arguments args);
        static bool FBig(wchar_t ch);
        static bool FDateDelimiter(wchar_t ch);

        bool IsNaN()
        {
            if (m_grfval & DateValueType::NotNaN)
            {
                return false;
            }

            if (JavascriptNumber::IsNan(m_tvUtc))
            {
                return true;
            }
            m_grfval |= DateValueType::NotNaN;
            return false;
        }

        ///------------------------------------------------------------------------------
        ///    Make sure m_tvLcl is valid. (Shared with hybrid debugging, which may use a fake scriptContext.)
        ///------------------------------------------------------------------------------
        template <class ScriptContext>
        __inline void EnsureTvLcl(ScriptContext* scriptContext)
        {
            if (!(m_grfval & DateValueType::Local))
            {
                m_tvLcl = GetTvLcl(m_tvUtc, scriptContext, &m_tzd);
                m_grfval |= DateValueType::Local;
            }
        }

        __inline void EnsureTvLcl(void)
        {
            EnsureTvLcl(m_scriptContext);
        }

        ///------------------------------------------------------------------------------
        /// Make sure m_ymdLcl is valid. (Shared with hybrid debugging, which may use a fake scriptContext.)
        ///------------------------------------------------------------------------------
        template <class ScriptContext>
        __inline void EnsureYmdLcl(ScriptContext* scriptContext)
        {
            if (m_grfval & DateValueType::YearMonthDayLocal)
            {
                return;
            }
            EnsureTvLcl(scriptContext);
            GetYmdFromTv(m_tvLcl, &m_ymdLcl);
            m_grfval |= DateValueType::YearMonthDayLocal;
        }

        __inline void EnsureYmdLcl(void)
        {
            EnsureYmdLcl(m_scriptContext);
        }

        ///------------------------------------------------------------------------------
        /// Make sure m_ymdUtc is valid.
        ///------------------------------------------------------------------------------
        __inline void EnsureYmdUtc(void)
        {
            if (m_grfval & DateValueType::YearMonthDayUTC)
            {
                return;
            }
            GetYmdFromTv(m_tvUtc, &m_ymdUtc);
            m_grfval |= DateValueType::YearMonthDayUTC;
        }


        __inline Var GetFullYear()
        {
            EnsureYmdLcl();
            return JavascriptNumber::ToVar(m_ymdLcl.year, m_scriptContext);
        }

        __inline Var GetYear()
        {
            EnsureYmdLcl();
            // WOOB bug 1099381: ES5 spec B.2.4: getYear() must return YearFromTime() - 1900.
            // Note that negative value is OK for the spec.
            int value = m_ymdLcl.year - 1900;
            return JavascriptNumber::ToVar(value, m_scriptContext);
        }

        __inline Var GetMonth()
        {
            EnsureYmdLcl();
            return JavascriptNumber::ToVar(m_ymdLcl.mon, m_scriptContext);
        }

        __inline Var GetDate()
        {
            EnsureYmdLcl();
            return JavascriptNumber::ToVar(m_ymdLcl.mday + 1, m_scriptContext);
        }

        __inline Var GetDay()
        {
            EnsureYmdLcl();
            return JavascriptNumber::ToVar(m_ymdLcl.wday, m_scriptContext);
        }

        __inline Var GetHours()
        {
            EnsureYmdLcl();
            return JavascriptNumber::ToVar((m_ymdLcl.time / 3600000)%24, m_scriptContext);
        }

        __inline Var GetMinutes()
        {
            EnsureYmdLcl();
            return JavascriptNumber::ToVar((m_ymdLcl.time / 60000) % 60, m_scriptContext);
        }

        __inline Var GetSeconds()
        {
            EnsureYmdLcl();
            return JavascriptNumber::ToVar((m_ymdLcl.time / 1000) % 60, m_scriptContext);
        }

        __inline Var GetDateMilliSeconds()
        {
            EnsureYmdLcl();
            return JavascriptNumber::ToVar(m_ymdLcl.time % 1000, m_scriptContext);
        }

        template <class StringBuilder, class ScriptContext, class NewStringBuilderFunc>
        StringBuilder* GetDiagValueString(ScriptContext* scriptContext, NewStringBuilderFunc newStringBuilder);

        template <class StringBuilder, class ScriptContext, class NewStringBuilderFunc>
        static StringBuilder* ConvertVariantDateToString(double dbl, ScriptContext* scriptContext, NewStringBuilderFunc newStringBuilder);

        template <class StringBuilder, class ScriptContext, class NewStringBuilderFunc>
        static StringBuilder* GetDateDefaultString(Js::YMD *pymd, TZD *ptzd, DateTimeFlag noDateTime, ScriptContext* scriptContext, NewStringBuilderFunc newStringBuilder);

    private:
        double                  m_tvUtc;
        double                  m_tvLcl;
        Js::YMD                 m_ymdUtc;
        Js::YMD                 m_ymdLcl;
        TZD                     m_tzd;
        unsigned long           m_grfval; // Which fields are valid. m_tvUtc is always valid.
        ScriptContext *         m_scriptContext;
        bool                    m_modified : 1; // Whether SetDateData was called on this class

        friend JavascriptDate;
        friend HiResTimer;
    };

    ///
    /// Gets the daylight bias to use, in minutes. (Shared with hybrid debugging, which may use a fake scriptContext.)
    ///
    template <class ScriptContext>
    long DateImplementation::GetDaylightBias(const TIME_ZONE_INFORMATION *const pTz, const ScriptContext *const scriptContext)
    {
        Assert(pTz);
        Assert(scriptContext);
        return pTz->DaylightBias;
    }

    ///
    /// Gets the standard bias to use, in minutes. (Shared with hybrid debugging, which may use a fake scriptContext.)
    ///
    template <class ScriptContext>
    long DateImplementation::GetStandardBias(const TIME_ZONE_INFORMATION *const pTz, const ScriptContext *const scriptContext)
    {
        Assert(pTz);
        Assert(scriptContext);
        return pTz->StandardBias;
    }

    ///
    /// Use tv as the UTC time and return the corresponding local time. (Shared with hybrid debugging, which may use a fake scriptContext.)
    ///
    template <class ScriptContext>
    double DateImplementation::GetTvLcl(double tv, ScriptContext *scriptContext, TZD *ptzd)
    {
        Assert(scriptContext);

        double tvLcl;

        if (nullptr != ptzd)
        {
            ptzd->minutes = 0;
            ptzd->fDst = FALSE;
        }

        // See if we're out of range before conversion (UTC time value must be within this range)
        if (JavascriptNumber::IsNan(tv) || tv < ktvMin || tv > ktvMax)
        {
            return JavascriptNumber::NaN;
        }

        int bias;
        int offset;
        bool isDaylightSavings;
        tvLcl = scriptContext->GetDaylightTimeHelper()->UtcToLocal(tv, bias, offset, isDaylightSavings);
        if (nullptr != ptzd)
        {
            ptzd->minutes = -bias;
            ptzd->offset = offset;
            ptzd->fDst = isDaylightSavings;
        }
        return tvLcl;
    }

    ///
    /// Use tv as the local time and return the corresponding UTC time. (Shared with hybrid debugging, which may use a fake scriptContext.)
    ///
    template <class ScriptContext>
    double DateImplementation::GetTvUtc(double tv, ScriptContext *scriptContext)
    {
        Assert(scriptContext);

        double tvUtc;

        if (JavascriptNumber::IsNan(tv) || !NumberUtilities::IsFinite(tv))
        {
            return JavascriptNumber::NaN;
        }

        tvUtc = scriptContext->GetDaylightTimeHelper()->LocalToUtc(tv);
        // See if we're out of range after conversion (UTC time value must be within this range)
        if (JavascriptNumber::IsNan(tvUtc) || !NumberUtilities::IsFinite(tv) || tvUtc < ktvMin || tvUtc > ktvMax)
        {
            return JavascriptNumber::NaN;
        }
        return tvUtc;
    }

    //
    // Get diag value display for hybrid debugging.
    //  StringBuilder: A Js::StringBuilder/CompoundString like class, used to build strings.
    //  ScriptContext: A Js::ScriptContext like class, which provides fields needed by Date stringify.
    //  NewStringBuilderFunc: A function that returns a StringBuilder*, used to create a StringBuilder.
    //
    template <class StringBuilder, class ScriptContext, class NewStringBuilderFunc>
    StringBuilder* DateImplementation::GetDiagValueString(ScriptContext* scriptContext, NewStringBuilderFunc newStringBuilder)
    {
        if (JavascriptNumber::IsNan(m_tvUtc))
        {
            StringBuilder* bs = newStringBuilder(0);
            bs->Append(JS_DISPLAY_STRING_INVALID_DATE);
            return bs;
        }

        EnsureYmdLcl(scriptContext);
        return GetDateDefaultString<StringBuilder>(&m_ymdLcl, &m_tzd, DateTimeFlag::None, scriptContext, newStringBuilder);
    }

    template <class StringBuilder, class ScriptContext, class NewStringBuilderFunc>
    StringBuilder* DateImplementation::ConvertVariantDateToString(double dbl, ScriptContext* scriptContext, NewStringBuilderFunc newStringBuilder)
    {
        TZD tzd;
        YMD ymd;
        double tv = GetTvUtc(JsLocalTimeFromVarDate(dbl), scriptContext);

        tv = GetTvLcl(tv, scriptContext, &tzd);
        if (JavascriptNumber::IsNan(tv))
        {
            StringBuilder* bs = newStringBuilder(0);
            bs->Append(JS_DISPLAY_STRING_NAN);
            return bs;
        }

        GetYmdFromTv(tv, &ymd);
        return GetDateDefaultString<StringBuilder>(&ymd, &tzd, DateTimeFlag::None, scriptContext, newStringBuilder);
    }

    //
    // Get default date string, shared with hybrid debugging.
    //  StringBuilder: A Js::StringBuilder/CompoundString like class, used to build strings.
    //  ScriptContext: A Js::ScriptContext like class, which provides fields needed by Date stringify.
    //  NewStringBuilderFunc: A function that returns a StringBuilder*, used to create a StringBuilder.
    //
    template <class StringBuilder, class ScriptContext, class NewStringBuilderFunc>
    StringBuilder* DateImplementation::GetDateDefaultString(Js::YMD *pymd, TZD *ptzd, DateTimeFlag noDateTime, ScriptContext* scriptContext, NewStringBuilderFunc newStringBuilder)
    {
        int hour, min;

        StringBuilder* const bs = newStringBuilder(72);

        const auto ConvertUInt16ToString_ZeroPad_2 = [](const uint16 value, wchar_t *const buffer, const CharCount charCapacity)
        {
            const charcount_t cchWritten = NumberUtilities::UInt16ToString(value, buffer, charCapacity, 2);
            Assert(cchWritten != 0);
        };
        const auto ConvertLongToString = [](const long value, wchar_t *const buffer, const CharCount charCapacity)
        {
            const errno_t err = _ltow_s(value, buffer, charCapacity, 10);
            Assert(err == 0);
        };

        // PART 1 - DATE part
        if( !(noDateTime & DateTimeFlag::NoDate))
        {
            bs->AppendChars(g_rgpszDay[pymd->wday]);
            bs->AppendChars(L' ');
            bs->AppendChars(g_rgpszMonth[pymd->mon]);
            bs->AppendChars(L' ');
            // sz - as %02d - output is "01" to "31"
            bs->AppendChars(static_cast<WORD>(pymd->mday + 1), 2, ConvertUInt16ToString_ZeroPad_2);
            bs->AppendChars(L' ');

            // year is directly after day, month, daydigit for IE11+
            bs->AppendChars(pymd->year, 10, ConvertLongToString);

            if(!(noDateTime & DateTimeFlag::NoTime))
            {
                // append a space to delimit PART 2 - if to be outputted
                bs->AppendChars(L' ');
            }
        }

        // PART 2 - TIME part
        if(!(noDateTime & DateTimeFlag::NoTime))
        {
            // sz - as %02d - HOUR
            bs->AppendChars(static_cast<WORD>(pymd->time / 3600000), 2, ConvertUInt16ToString_ZeroPad_2);
            bs->AppendChars(L':');
            // sz - as %02d - MINUTE
            bs->AppendChars(static_cast<WORD>((pymd->time / 60000) % 60), 2, ConvertUInt16ToString_ZeroPad_2);
            bs->AppendChars(L':');
            // sz - as %02d - SECOND
            bs->AppendChars(static_cast<WORD>((pymd->time / 1000) % 60), 2, ConvertUInt16ToString_ZeroPad_2);

            bs->AppendChars(L" GMT");

            // IE11+
            min = ptzd->offset;
            if (min < 0)
            {
                bs->AppendChars(L'-');
                min = -min;
            }
            else
            {
                bs->AppendChars(L'+');
            }

            hour = min / 60;
            min %= 60;
            // sz - as %02d - HOUR
            bs->AppendChars(static_cast<WORD>(hour), 2, ConvertUInt16ToString_ZeroPad_2);

            // sz - as %02d - MIN
            bs->AppendChars(static_cast<WORD>(min), 2, ConvertUInt16ToString_ZeroPad_2);

            bs->AppendChars(L" (");

            // check the IsDaylightSavings?
            if (ptzd->fDst == false)
            {
                const wchar_t *const standardName = scriptContext->GetTimeZoneInfo()->StandardName;
                bs->AppendChars(standardName, static_cast<CharCount>(wcslen(standardName)));
            }
            else
            {
                const wchar_t *const daylightName = scriptContext->GetTimeZoneInfo()->DaylightName;
                bs->AppendChars(daylightName, static_cast<CharCount>(wcslen(daylightName)));
            }

            bs->AppendChars(L')');
        }

        return bs;
    }

} // namespace Js
