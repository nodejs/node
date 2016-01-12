//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"
#include <time.h>
#include "DateImplementationData.h"

#include "CharClassifier.h"

namespace Js {

    static double ConvertToInteger(double dbl)
    {
        Assert(Js::NumberUtilities::IsFinite(dbl));
        if (Js::NumberUtilities::LuHiDbl(dbl) & 0x80000000)
        {
            Js::NumberUtilities::LuHiDbl(dbl) &= 0x7FFFFFFF;
            dbl = floor(dbl);
            Js::NumberUtilities::LuHiDbl(dbl) |= 0x80000000;
        }
        else
        {
            dbl = floor(dbl);
            // We have to do this because some implementations map 0.5 to -0.
            Js::NumberUtilities::LuHiDbl(dbl) &= 0x7FFFFFFF;
        }
        return dbl;
    }

    static const double kdblHalfSecond = 0.5 / 86400.0;

    struct SZS
    {
        wchar_t *psz;           // string
        short cch;              // length of string
        short szst;             // type of entry
        long lwVal;             // value
    };

    BEGIN_ENUM_BYTE(ParseStringTokenType)
        AmPm,
        Month,
        Day,
        Zone,
        BcAd,
    END_ENUM_BYTE()

    const static SZS g_rgszs[] =
    {
#define Szs(sz, val) { L##sz, sizeof(L##sz) - 1, ParseStringTokenType::##szst, val }

        // bc and ad
#undef szst
#define szst BcAd
        Szs("bc", -1),
        Szs("b.c", -1),
        Szs("ad", +1),
        Szs("a.d", +1),

        // am and pm
#undef szst
#define szst AmPm
        Szs("am", -1),
        Szs("a.m", -1),
        Szs("pm", +1),
        Szs("p.m", +1),

        // time zones
#undef szst
#define szst Zone
        Szs("est", -5 * 60),
        Szs("edt", -4 * 60),
        Szs("cst", -6 * 60),
        Szs("cdt", -5 * 60),
        Szs("mst", -7 * 60),
        Szs("mdt", -6 * 60),
        Szs("pst", -8 * 60),
        Szs("pdt", -7 * 60),
        Szs("gmt", 0),
        Szs("utc", 0),

        // week days
#undef szst
#define szst Day
        Szs("sunday", 0),
        Szs("monday", 1),
        Szs("tuesday", 2),
        Szs("wednesday", 3),
        Szs("thursday", 4),
        Szs("friday", 5),
        Szs("saturday", 6),

        // months
#undef szst
#define szst Month
        Szs("january", 0),
        Szs("february", 1),
        Szs("march", 2),
        Szs("april", 3),
        Szs("may", 4),
        Szs("june", 5),
        Szs("july", 6),
        Szs("august", 7),
        Szs("september", 8),
        Szs("october", 9),
        Szs("november", 10),
        Szs("december", 11),

#undef szst
#undef Szs
    };
    const long kcszs = sizeof(g_rgszs) / sizeof(SZS);

    // Moved DaylightTimeHelper to common.lib to share with hybrid debugging, but this function depends on runtime.
    bool DaylightTimeHelper::ForceOldDateAPIFlag()
    {
        // The following flag was added to support Date unit test on win7.
        return CONFIG_ISENABLED(Js::ForceOldDateAPIFlag);
    }

    ///----------------------------------------------------------------------------
    ///----------------------------------------------------------------------------
    ///
    /// class DateImplementation
    ///
    ///----------------------------------------------------------------------------
    ///----------------------------------------------------------------------------

    DateImplementation::DateImplementation(double value, ScriptContext* scriptContext)
    {
        // Assume DateImplementation is allocated in the recycler and is zero initialized
        // Do not stack allocate of this struct, as it doesn't initialize all fields.
        // If the stack allocated struct is copied in to recycler allocated ones, it
        // many introduce false reference from the stack

        Assert(!ThreadContext::IsOnStack(this));
        AssertValue<byte>(this, 0, sizeof(DateImplementation));

        m_scriptContext = scriptContext;
        m_modified = false;

        SetTvUtc(value);
    };

    double
    DateImplementation::GetMilliSeconds()
    {
        return m_tvUtc;
    }

    double
    DateImplementation::NowFromHiResTimer(ScriptContext* scriptContext)
    {
        // Use current time.
        return scriptContext->GetThreadContext()->GetHiResTimer()->Now();
    }

    double
    DateImplementation::NowInMilliSeconds(ScriptContext * scriptContext)
    {
        return DoubleToTvUtc(DateImplementation::NowFromHiResTimer(scriptContext));
    }

    JavascriptString*
    DateImplementation::GetString(DateStringFormat dsf, DateTimeFlag noDateTime)
    {
        if (JavascriptNumber::IsNan(m_tvUtc))
        {
            return m_scriptContext->GetLibrary()->GetInvalidDateString();
        }

        switch (dsf)
         {
            default:
                EnsureYmdLcl();
                return GetDateDefaultString(&m_ymdLcl, &m_tzd, noDateTime, m_scriptContext);

            case DateStringFormat::Locale:
                EnsureYmdLcl();

                if( m_ymdLcl.year > 1600 && m_ymdLcl.year < 10000 )
                {
                    // The year falls in the range which can be handled by both the Win32
                    // function GetDateFormat and the COM+ date type
                    // - the latter is for forward compatibility with JS 7.
                    JavascriptString *bs = GetDateLocaleString(&m_ymdLcl, &m_tzd, noDateTime, m_scriptContext);
                    if (bs != nullptr)
                    {
                        return bs;
                    }
                    else
                    {
                        return GetDateDefaultString(&m_ymdLcl, &m_tzd, noDateTime, m_scriptContext);
                    }
                }
                else
                {
                    return GetDateDefaultString(&m_ymdLcl, &m_tzd, noDateTime, m_scriptContext);
                }

            case DateStringFormat::GMT:
                EnsureYmdUtc();
                return GetDateGmtString(&m_ymdUtc, m_scriptContext);
        }
    }

    JavascriptString*
    DateImplementation::GetISOString()
    {
        // ES5 15.9.5.43: throw RangeError if time value is not a finite number
        if (!Js::NumberUtilities::IsFinite(m_tvUtc))
        {
            JavascriptError::ThrowRangeError(m_scriptContext, JSERR_NeedNumber);
        }

        CompoundString *const bs = CompoundString::NewWithCharCapacity(30, m_scriptContext->GetLibrary());

        GetDateComponent(bs, DateData::FullYear, 0);
        bs->AppendChars(L'-');
        // month
        GetDateComponent(bs, DateData::Month, 1/*adjustment*/);
        bs->AppendChars(L'-');
        // date
        GetDateComponent(bs, DateData::Date, 0);
        bs->AppendChars(L'T');
        // hours
        GetDateComponent(bs, DateData::Hours, 0);
        bs->AppendChars(L':');
        // minutes
        GetDateComponent(bs, DateData::Minutes, 0);
        bs->AppendChars(L':');
        // seconds
        GetDateComponent(bs, DateData::Seconds, 0);

        // ES5 fill in milliseconds but v5.8 does not
        bs->AppendChars(L'.');
        // milliseconds
        GetDateComponent(bs, DateData::Milliseconds, 0);

        bs->AppendChars(L'Z');

        return bs;
    }

    void
    DateImplementation::GetDateComponent(CompoundString *bs, DateData componentType, int adjust)
    {
        double value = this->GetDateData(componentType, true /* fUTC */, m_scriptContext);
        if(Js::NumberUtilities::IsFinite(value))
        {
            const int ival = (int)value + adjust;
            const int ivalAbs = ival < 0 ? -ival : ival;

            switch(componentType)
            {
                case DateData::FullYear:
                    if(ival < 0 || ival > 9999)
                    {
                        // ES5 spec section 15.9.1.15.1 states that for years outside the range 0-9999, the expanded year
                        // representation should:
                        //     - always include the sign
                        //     - have 2 extra digits (6 digits total)
                        bs->AppendChars(ival < 0 ? L'-' : L'+');
                        if(ivalAbs < 100000)
                        {
                            bs->AppendChars(L'0');
                            if(ivalAbs < 10000)
                            {
                                bs->AppendChars(L'0');
                            }
                        }
                    }

                    // Years are zero-padded to at least 4 digits in ES5
                    if(ivalAbs < 1000)
                    {
                        bs->AppendChars(L'0');
                        // will fall through to next case for additional padding
                    }
                    else
                    {
                        break;
                    }
                    // fall through

                case DateData::Milliseconds:
                    if (ivalAbs < 100)
                    {
                        bs->AppendChars(L'0');
                        // will fall through to next case for additional padding
                    }
                    else
                    {
                        break;
                    }
                    // fall through

                default:
                    if (ivalAbs < 10)
                    {
                        bs->AppendChars(L'0');
                    }
            }

            // _itow_s makes use of max 12 bytes for a base-10 32-bit int (L"-2147483648\0"), although we don't need the sign
            // and our numbers shouldn't be that big anyway
            bs->AppendChars(
                ivalAbs,
                10,
                [](const int value, wchar_t *const buffer, const CharCount charCapacity)
                {
                    errno_t err = _itow_s(value, buffer, charCapacity, 10);
                    Assert(err == 0);
                });
        }
    }
    ///----------------------------------------------------------------------------
    ///
    /// Use tv as the UTC time.
    ///
    ///----------------------------------------------------------------------------

    void
    DateImplementation::SetTvUtc(double tv)
    {
        m_grfval = 0;
        m_tvUtc = DoubleToTvUtc(tv);
    }

    double
    DateImplementation::DoubleToTvUtc(double tv)
    {
        if (JavascriptNumber::IsNan(tv) || tv < ktvMin || tv > ktvMax)
        {
            return JavascriptNumber::NaN;
        }
        return CONFIG_FLAG(HighPrecisionDate)? tv : ConvertToInteger(tv);
    }

    ///----------------------------------------------------------------------------
    ///
    /// Use tv as the local time and set m_tvUtc appropriately.
    ///
    ///----------------------------------------------------------------------------

    void
    DateImplementation::SetTvLcl(double tv)
    {
        m_grfval = 0;
        m_tvUtc  = GetTvUtc(tv, m_scriptContext);
    }

    JavascriptString*
    DateImplementation::ConvertVariantDateToString(double dbl, ScriptContext* scriptContext)
    {
        Js::DateImplementation::TZD tzd;
        Js::YMD ymd;
        double tv = Js::DateImplementation::GetTvUtc(Js::DateImplementation::JsLocalTimeFromVarDate(dbl), scriptContext);

        tv = Js::DateImplementation::GetTvLcl(tv, scriptContext, &tzd);
        if (Js::JavascriptNumber::IsNan(tv))
        {
            return JavascriptNumber::ToStringNan(scriptContext);
        }

        Js::DateImplementation::GetYmdFromTv(tv, &ymd);

        return DateImplementation::GetDateDefaultString(&ymd, &tzd, 0, scriptContext);
    }

    JavascriptString*
    DateImplementation::GetDateDefaultString(Js::YMD *pymd, TZD *ptzd,DateTimeFlag noDateTime,ScriptContext* scriptContext)
    {
        return GetDateDefaultString<CompoundString>(pymd, ptzd, noDateTime, scriptContext,
            [=](CharCount capacity) -> CompoundString*
        {
            return CompoundString::NewWithCharCapacity(capacity, scriptContext->GetLibrary());
        });
    }

    JavascriptString*
    DateImplementation::GetDateGmtString(Js::YMD *pymd,ScriptContext* scriptContext)
    {
        // toUTCString() or toGMTString() will return for example:
        //  "Thu, 02 Feb 2012 09:02:03 GMT" for versions IE11 or above

        CompoundString *const bs = CompoundString::NewWithCharCapacity(30, scriptContext->GetLibrary());

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

        bs->AppendChars(g_rgpszDay[pymd->wday]);
        bs->AppendChars(L", ");
        // sz - as %02d - output is "01" to "31"
        bs->AppendChars(static_cast<WORD>(pymd->mday + 1), 2, ConvertUInt16ToString_ZeroPad_2);
        bs->AppendChars(L' ');
        bs->AppendChars(g_rgpszMonth[pymd->mon]);
        bs->AppendChars(L' ');

        // Add the year.
        if (pymd->year > 0)
        {
            bs->AppendChars(pymd->year, 10, ConvertLongToString);
        }
        else
        {
            bs->AppendChars(1 - pymd->year, 10, ConvertLongToString);
            bs->AppendChars(L" B.C.");
        }

        // Add the time.
        bs->AppendChars(L' ');
        // sz - as %02d - HOUR
        bs->AppendChars(static_cast<WORD>(pymd->time / 3600000), 2, ConvertUInt16ToString_ZeroPad_2);
        bs->AppendChars(L':');
        // sz - as %02d - MINUTE
        bs->AppendChars(static_cast<WORD>((pymd->time / 60000) % 60), 2, ConvertUInt16ToString_ZeroPad_2);
        bs->AppendChars(L':');
        // sz - as %02d - SECOND
        bs->AppendChars(static_cast<WORD>((pymd->time / 1000) % 60), 2, ConvertUInt16ToString_ZeroPad_2);
        bs->AppendChars(L' ');

        bs->AppendChars(L"GMT");

        return bs;
    }

    JavascriptString*
    DateImplementation::GetDateLocaleString(Js::YMD *pymd, TZD *ptzd, DateTimeFlag noDateTime,ScriptContext* scriptContext)
    {
        SYSTEMTIME st;
        int cch;
        int count = 0;
        const int kcchMax = 256;
        WCHAR wszBuf[kcchMax];
        WCHAR *pwszBuf, *pToBeFreed = NULL, *p;
        JavascriptString *bs = nullptr;

         // the caller of this function should ensure that the range of pymd->year is such that the following conversion works.
        st.wYear = (WORD)pymd->year;
        st.wMonth = (WORD)pymd->mon + 1;
        st.wDayOfWeek = (WORD)pymd->wday;
        st.wDay = (WORD)pymd->mday + 1;
        st.wHour = (WORD)(pymd->time / 3600000);
        st.wMinute = (WORD)((pymd->time / 60000) % 60);
        st.wSecond = (WORD)((pymd->time / 1000) % 60);
        st.wMilliseconds = (WORD)(pymd->time % 60);

        cch = 0;

        LCID lcid = GetUserDefaultLCID();
        if( !(noDateTime & DateTimeFlag::NoDate))
        {
            DWORD dwFormat = DATE_LONGDATE;

            if ((PRIMARYLANGID(LANGIDFROMLCID(lcid)) == LANG_ARABIC) ||
                (PRIMARYLANGID(LANGIDFROMLCID(lcid)) == LANG_HEBREW))
            {
                dwFormat |= DATE_RTLREADING;
            }
            int c = GetDateFormatW( lcid, dwFormat, &st, NULL, NULL, 0 );

            if( c <= 0 )
            {
                if (PRIMARYLANGID(LANGIDFROMLCID(lcid)) == LANG_HEBREW)
                {
                    // Can't support some Hebrew dates - current limit is 1 Jan AD 2240
                    Js::JavascriptError::ThrowRangeError(scriptContext, VBSERR_CantDisplayDate);
                }

                AssertMsg(false, "GetDateFormat failed");
                goto Error;
            }
            cch += c;
        }

        if( !(noDateTime & DateTimeFlag::NoTime))
        {
            int c = GetTimeFormatW( lcid, 0, &st, NULL, NULL, 0 );
            if( c <= 0 )
            {
                AssertMsg(false, "GetTimeFormat failed");
                goto Error;
            }
            cch += c;
        }
        cch++; // For the space between the date and the time.

        if( cch > kcchMax )
        {
            pwszBuf = pToBeFreed = (WCHAR *)malloc( cch * sizeof(WCHAR) );
            if(!pwszBuf)
            {
                Js::JavascriptError::ThrowOutOfMemoryError(scriptContext);
            }
        }
        else
        {
            wszBuf[0] = '\0';
            pwszBuf = wszBuf;
        }

        count = cch;
        p = pwszBuf;

        if( !(noDateTime & DateTimeFlag::NoDate))
        {
            DWORD dwFormat = DATE_LONGDATE;

            if ((PRIMARYLANGID(LANGIDFROMLCID(lcid)) == LANG_ARABIC) ||
                (PRIMARYLANGID(LANGIDFROMLCID(lcid)) == LANG_HEBREW))
            {
                dwFormat |= DATE_RTLREADING;
            }
            int c = GetDateFormatW( lcid, dwFormat, &st, NULL, p, cch );

            if( c <= 0 || c > cch)
            {
                if (PRIMARYLANGID(LANGIDFROMLCID(lcid)) == LANG_HEBREW)
                {
                    // Can't support some Hebrew dates - current limit is 1 Jan AD 2240
                    Js::JavascriptError::ThrowRangeError(scriptContext, VBSERR_CantDisplayDate);
                }

                AssertMsg(false, "GetDateFormat failed");
                goto Error;
            }

            p += (c-1);
            cch -= (c-1);
            if( !(noDateTime & DateTimeFlag::NoTime))
            {
                *p++ = L' ';
                cch--;
            }

        }

        if( !(noDateTime & DateTimeFlag::NoTime))
        {
            int c = GetTimeFormatW( lcid, 0, &st, NULL, p, cch );
            Assert( c > 0 );
            if( c <= 0 )
            {
                AssertMsg(false, "GetTimeFormat failed");
                goto Error;
            }
            cch -= (c-1);
        }

        bs = JavascriptString::NewCopyBuffer(pwszBuf, count-cch, scriptContext);

Error:
        if( pToBeFreed )
            free(pToBeFreed);

        return bs;
    }

    double
    DateImplementation::GetDateData(DateData dd, bool fUtc, ScriptContext* scriptContext)
    {
        Js::YMD *pymd;
        double value = 0;

        if (JavascriptNumber::IsNan(m_tvUtc))
        {
            return m_tvUtc;
        }

        if (fUtc)
        {
            EnsureYmdUtc();
            pymd = &m_ymdUtc;
        }
        else
        {
            EnsureYmdLcl();
            pymd = &m_ymdLcl;
        }

        switch (dd)
        {
        case DateData::Year:
            Assert(scriptContext);

            // WOOB bug 1099381: ES5 spec B.2.4: getYear() must return YearFromTime() - 1900.
            // Note that negative value is OK for the spec.
            value = pymd->year - 1900;
            break;
        case DateData::FullYear:
            value = pymd->year;
            break;
        case DateData::Month:
            value = pymd->mon;
            break;
        case DateData::Date:
            value = pymd->mday + 1;
            break;
        case DateData::Day:
            value = pymd->wday;
            break;
        case DateData::Hours:
            value = (pymd->time / 3600000);
            break;
        case DateData::Minutes:
            value = (pymd->time / 60000) % 60;
            break;
        case DateData::Seconds:
            value = (pymd->time / 1000) % 60;
            break;
        case DateData::Milliseconds:
            value = pymd->time % 1000;
            break;
        case DateData::TimezoneOffset:
            //Assert(!fUtc);
            value = (m_tvUtc - m_tvLcl) / 60000;
            break;
        default:
            // Shouldn't come here
            AssertMsg(false, "DateData type invalid");
        }

        return value;
    }

    inline bool
    DateImplementation::FBig(wchar_t ch)
    {
        return (unsigned int)ch >= 128;
    }

    inline bool
    DateImplementation::FDateDelimiter(wchar_t ch)
    {
        return (ch == '/' || ch == '-');
    }

    ///------------------------------------------------------------------------------
    //  Parse a string as a date and return the number of milliseconds since
    //  January 1, 1970 GMT.
    //
    //  TODO: This function is ported from IE8 jscript engine. This lengthy
    //        and needs a cleanup - break into smaller functions.
    ///------------------------------------------------------------------------------

    double DateImplementation::UtcTimeFromStr(ScriptContext *scriptContext, JavascriptString *pParseString)
    {
        Assert(pParseString != nullptr);
        double dbl;
        if (scriptContext->GetLastUtcTimeFromStr(pParseString, dbl))
        {
            return dbl;
        }
        unsigned int ulength = pParseString->GetLength();
        const wchar_t *psz =  pParseString->GetSz();

        if(UtcTimeFromStrCore(psz, ulength, dbl, scriptContext))
        {
            scriptContext->SetLastUtcTimeFromStr(pParseString, dbl);
            return dbl;
        }

        Js::JavascriptError::ThrowOutOfMemoryError(scriptContext);
    }

    bool DateImplementation::TryParseDecimalDigits(
        const wchar_t *const str,
        const size_t length,
        const size_t startIndex,
        const size_t numDigits,
        int &value)
    {
        Assert(str);
        Assert(length);
        Assert(startIndex <= length);
        Assert(numDigits != 0 && numDigits <= 9); // will fit in an 'int'

        if(numDigits > length - startIndex)
            return false;

        size_t i = 0;

        // Skip leading zeroes
        while(str[startIndex + i] == L'0' && ++i < numDigits);

        // Parse remaining digits
        int v = 0;
        for(; i < numDigits; ++i)
        {
            const unsigned short d = str[startIndex + i] - L'0';
            if(d > 9)
                break;
            v = v * 10 + d;
        }
        if(i < numDigits)
            return false;
        Assert(i == numDigits);
        value = v;

        // The next character must not be a digit
        return !(i < length - startIndex && static_cast<unsigned short>(str[startIndex + i] - L'0') <= 9);
    }

    // Either 1 digit or 2 digits or 3 digits
    // Ignore any digits after the third
    bool DateImplementation::TryParseMilliseconds(
        const wchar_t *const str,
        const size_t length,
        const size_t startIndex,
        int &value,
        size_t &foundDigits)
    {
        const size_t minNumDigits = 1;

        Assert(str);
        Assert(length);
        Assert(startIndex <= length);

        size_t allDigits = length - startIndex;
        if(allDigits < minNumDigits)
            return false;

        size_t i = 0;

        // Skip leading zeroes
        while(str[startIndex + i] == L'0' && ++i < allDigits);

        // Parse remaining digits
        int v = 0;
        for(; i < allDigits ; ++i)
        {
            const unsigned short d = str[startIndex + i] - L'0';
            if(d > 9)
                break;
            if (i < 3) // not past the 3rd digit in the milliseconds, don't ignore
                v = v * 10 + d;
        }
        if(i < minNumDigits)
            return false;

        foundDigits = i;
        if (foundDigits == 1)
                v = v * 100;
        else if (foundDigits == 2)
                v = v * 10;

        value = v;

        // The next character must not be a digit
        return !(i < length - startIndex && static_cast<unsigned short>(str[startIndex + i] - L'0') <= 9);
    }

    bool DateImplementation::TryParseTwoDecimalDigits(
        const wchar_t *const str,
        const size_t length,
        const size_t startIndex,
        int &value,
        bool canHaveTrailingDigit /* = false */)
    {
        Assert(str);
        Assert(length);
        Assert(startIndex <= length);

        if(length - startIndex < 2)
            return false;

        unsigned short d = str[startIndex] - L'0';
        if(d > 9)
            return false;
        short v = d * 10;
        d = str[startIndex + 1] - L'0';
        if(d > 9)
            return false;
        value = v + d;

        // The next character must not be a digit if canHaveTrailingDigit is false
        bool hasNoTrailingDigit = !(length - startIndex > 2 && static_cast<unsigned short>(str[startIndex + 2] - L'0') <= 9);
        return canHaveTrailingDigit || hasNoTrailingDigit;
    }

    bool DateImplementation::TryParseIsoString(const wchar_t *const str, const size_t length, double &timeValue, ScriptContext *scriptContext)
    {
        Assert(str);

        size_t i = 0;
        const Js::CharClassifier *classifier = scriptContext->GetCharClassifier();
        // Skip leading whitespace (for cross-browser compatibility)
        // Also skip bidirectional characters, for Round tripping locale formatted date
        while ((classifier->IsWhiteSpace(str[i]) || classifier->IsBiDirectionalChar(str[i])) && ++i < length);

        // Minimum length must be 4 (YYYY)
        if(length - i < 4)
            return false;

        // YYYY|(+|-)YYYYYY
        int year;
        switch(str[i])
        {
            case L'+':
                ++i;
                if(!TryParseDecimalDigits(str, length, i, 6, year))
                    return false;
                i += 6;
                break;

            case L'-':
                ++i;
                if(!TryParseDecimalDigits(str, length, i, 6, year) || year == 0)
                    return false;
                year = -year;
                i += 6;
                break;

            case L'0':
            case L'1':
            case L'2':
            case L'3':
            case L'4':
            case L'5':
            case L'6':
            case L'7':
            case L'8':
            case L'9':
                if(!TryParseDecimalDigits(str, length, i, 4, year))
                    return false;
                i += 4;
                break;

            default:
                return false;
        }

        // Skip bidirectional characters, for Round tripping locale formatted date
        i += classifier->SkipBiDirectionalChars(str, i, length);

        int month = 0,
            day = 0,
            timePortionMilliseconds = 0,
            utcOffsetMilliseconds = 0;
        bool isLocalTime = false;

        do // while(false);
        {
            do // while(false);
            {
                // -MM
                if(i >= length || str[i] != L'-')
                    break;
                ++i;
                if(!TryParseTwoDecimalDigits(str, length, i, month))
                    return false;
                --month;
                if(month < 0 || month > 11)
                    return false;
                i += 2;

                // Skip bidirectional characters, for Round tripping locale formatted date
                i += classifier->SkipBiDirectionalChars(str, i, length);

                // -DD
                if(i >= length || str[i] != L'-')
                    break;
                ++i;
                if(!TryParseTwoDecimalDigits(str, length, i, day))
                    return false;
                --day;
                if(day < 0 || day > 30)
                    return false;
                i += 2;

                // Skip bidirectional characters, for Round tripping locale formatted date
                i += classifier->SkipBiDirectionalChars(str, i, length);
            } while(false);

            // THH:mm
            if(i >= length || str[i] != L'T')
                break;
            ++i;
            int t;
            if(!TryParseTwoDecimalDigits(str, length, i, t) || t > 24)
                return false;
            timePortionMilliseconds += t * (60 * 60 * 1000);
            i += 2;

            // Skip bidirectional characters, for Round tripping locale formatted date
            i += classifier->SkipBiDirectionalChars(str, i, length);

            if(i >= length || str[i] != L':')
                return false;
            ++i;
            if(!TryParseTwoDecimalDigits(str, length, i, t) || t > 59)
                return false;
            timePortionMilliseconds += t * (60 * 1000);
            i += 2;

            // Skip bidirectional characters, for Round tripping locale formatted date
            i += classifier->SkipBiDirectionalChars(str, i, length);

            do // while(false);
            {
                // :ss
                if(i >= length || str[i] != L':')
                    break;
                ++i;
                if(!TryParseTwoDecimalDigits(str, length, i, t) || t > 59)
                    return false;
                timePortionMilliseconds += t * 1000;
                i += 2;

                // Skip bidirectional characters, for Round tripping locale formatted date
                i += classifier->SkipBiDirectionalChars(str, i, length);

                // .sss
                if(i >= length || str[i] != L'.')
                    break;
                ++i;
                // Require one or more decimal digits. Ignore digits beyond the third
                size_t foundDigits = 0;
                if(!TryParseMilliseconds(str, length, i, t, foundDigits))
                    return false;
                timePortionMilliseconds += t;
                i += foundDigits;

                // Skip bidirectional characters, for Round tripping locale formatted date
                i += classifier->SkipBiDirectionalChars(str, i, length);
            } while(false);

            // Z|(+|-)HH:mm
            if(i >= length)
            {
                isLocalTime = true;
                break;
            }
            const wchar_t utcOffsetSign = str[i];
            if(utcOffsetSign == L'Z')
            {
                ++i;
                break;
            }
            if(utcOffsetSign != L'+' && utcOffsetSign != L'-')
            {
                isLocalTime = true;
                break;
            }
            ++i;
            // In -version:6 we allow optional colons in the timezone offset
            if (!TryParseTwoDecimalDigits(str, length, i, t, scriptContext->GetConfig()->IsES6DateParseFixEnabled() /* Timezone may be 4 sequential digits */) || t > 24)
                return false;
            utcOffsetMilliseconds += t * (60 * 60 * 1000);
            i += 2;

            // Skip bidirectional characters, for Round tripping locale formatted date
            i += classifier->SkipBiDirectionalChars(str, i, length);

            if(i >= length)
                return false;
            // The ':' is optional in ISO 8601
            if (str[i] == L':')
            {
                ++i;
            }
            if(!TryParseTwoDecimalDigits(str, length, i, t) || t > 59)
                return false;
            utcOffsetMilliseconds += t * (60 * 1000);
            i += 2;

            // Skip bidirectional characters, for Round tripping locale formatted date
            i += classifier->SkipBiDirectionalChars(str, i, length);

            if(utcOffsetSign == L'-')
                utcOffsetMilliseconds = -utcOffsetMilliseconds;
        } while(false);

        // Skip trailing whitespace (for cross-browser compatibility)
        // Skip bidirectional characters, for Round tripping locale formatted date
        while (i < length && (classifier->IsWhiteSpace(str[i]) || classifier->IsBiDirectionalChar(str[i])))
            ++i;

        // There should only have been whitespace remaining, if any
        if(i < length)
            return false;
        Assert(i == length);

        // Compute the time value
        timeValue = TvFromDate(year, month, day, timePortionMilliseconds - utcOffsetMilliseconds);
        if (isLocalTime)
        {
            // Compatibility note:
            // In ES5, it was unspecified how to handle date strings without the trailing time zone offset "Z|(+|-)HH:mm".
            // In ES5.1, an absent time zone offset defaulted to "Z", which contradicted ISO8601:2004(E).
            // This was corrected in an ES5.1 errata note. Moreover, the ES6 draft now follows ISO8601.
            timeValue = GetTvUtc(timeValue, scriptContext);
        }
        return true;
    }

    boolean DateImplementation::UtcTimeFromStrCore(
        __in_ecount_z(ulength) const wchar_t *psz,
        unsigned int ulength,
        double &retVal,
        ScriptContext *const scriptContext)
    {
        Assert(scriptContext);

        if (ulength >= 0x7fffffff)
        {
            //Prevent unreasonable requests from causing overflows.
            return false;
        }

        if (nullptr == psz)
        {
            retVal = JavascriptNumber::NaN;
            return true;
        }

        // Try to parse the string as the ISO format first
        if(TryParseIsoString(psz, ulength, retVal, scriptContext))
        {
            return true;
        }

        enum
        {
            ssNil,
            ssMinutes,
            ssSeconds,
            ssAddOffset,
            ssSubOffset,
            ssDate,
            ssMonth,
            ssYear
        };

        wchar_t *pchBase;
        wchar_t *pch;
        wchar_t ch;
        wchar_t *pszSrc = nullptr;

        const long lwNil = 0x80000000;
        long cch;
        long depth;
        long lwT;
        long lwYear = lwNil;
        long lwMonth = lwNil;
        long lwDate = lwNil;
        long lwTime = lwNil;
        long lwZone = lwNil;
        long lwOffset = lwNil;

        long ss = ssNil;
        const SZS *pszs;

        bool fUtc;

        int tAmPm = 0;
        int tBcAd = 0;

        double tv = JavascriptNumber::NaN; // Initialized for error handling.

        //Create a copy to analyze
        BEGIN_TEMP_ALLOCATOR(tempAllocator, scriptContext, L"UtcTimeFromStr");

        pszSrc = AnewArray(tempAllocator, wchar_t, ulength + 1);

        size_t size = sizeof(wchar_t) * (ulength + 1);
        js_memcpy_s(pszSrc, size, psz, size);

        _wcslwr_s(pszSrc,ulength+1);
        bool isDateNegativeVersion5 = false;
        bool isNextFieldDateNegativeVersion5 = false;
        const Js::CharClassifier *classifier = scriptContext->GetCharClassifier();
        #pragma prefast(suppress: __WARNING_INCORRECT_VALIDATION, "pch is guaranteed to be null terminated by __in_z on psz and js_memcpy_s copying the null byte")
        for (pch = pszSrc; 0 != (ch = classifier->SkipBiDirectionalChars(pch));)
        {
            pch++;
            if (ch <= ' ')
            {
                continue;
            }

            switch (ch)
            {
                case '(':
                {
                    // skip stuff in parens
                    for (depth = 1; 0 != (ch = *pch); )
                    {
                        pch++;
                        if (ch == '(')
                        {
                            depth++;
                        }
                        else if (ch == ')' && --depth <= 0)
                        {
                            break;
                        }
                    }
                    continue;
                }
                case ',':
                case ':':
                case '/':
                {
                    // ignore these
                    continue;
                }
                case '+':
                {
                    if (lwNil != lwYear)
                    {
                        ss = ssAddOffset;
                    }
                    continue;
                }
                case '-':
                {
                    if (lwNil != lwYear)
                    {
                        ss = ssSubOffset;
                    }
                    continue;
                }
            }


            pchBase = pch - 1;
            if (!FBig(ch) && isalpha(ch))
            {
                for ( ; !FBig(*pch) && (isalpha(*pch) || '.' == *pch); pch++)
                    ;

                cch = (long)(pch - pchBase);

                if ('.' == pchBase[cch - 1])
                {
                    cch--;
                }
                //Assert(cch > 0);

                // skip to the next real character
                while (0 != (*pch) && (*pch <= ' ' || classifier->IsBiDirectionalChar(*pch)))
                {
                    pch++;
                }

                // have an alphabetic token - look it up
                if (cch == 1)
                {
                    AssertMsg(isNextFieldDateNegativeVersion5 == false, "isNextFieldDateNegativeVersion5 == false");

                    // military version of time zone
                    // z = GMT
                    // j isn't used
                    // a to m are -1 to -12
                    // n to y are 1 to 12
                    if (lwNil != lwZone)
                    {
                        goto LError;
                    }
                    if (ch <= 'm')
                    {
                        if (ch == 'j' || ch < 'a')
                        {
                            goto LError;
                        }
                        lwZone = -(long)(ch - 'a' + (ch < 'j')) * 60;
                    }
                    else if (ch <= 'y')
                    {
                        lwZone = (long)(ch - 'm') * 60;
                    }
                    else if (ch == 'z')
                    {
                        lwZone = 0;
                    }
                    else
                    {
                        goto LError;
                    }

                    // look for a time zone offset
                    ss = ('+' == *pch) ? (pch++, ssAddOffset) :
                        ('-' == *pch) ? (pch++, ssSubOffset) : ssNil;
                    continue;
                }

                // look for a token
                for (pszs = g_rgszs + kcszs; ; )
                {
                    if (pszs-- <= g_rgszs)
                        goto LError;
                    if (cch <= pszs->cch &&
                        0 == memcmp(pchBase, pszs->psz, cch * sizeof(wchar_t)))
                    {
                        break;
                    }
                }

                switch (pszs->szst)
                {
                    case ParseStringTokenType::BcAd:
                    {
                        if (tBcAd != 0)
                        {
                            goto LError;
                        }
                        tBcAd = (int)pszs->lwVal;
                        break;
                    }
                    case ParseStringTokenType::AmPm:
                    {
                        if (tAmPm != 0)
                        {
                            goto LError;
                        }
                        tAmPm = (int)pszs->lwVal;
                        break;
                    }
                    case ParseStringTokenType::Month:
                    {
                        if (lwNil != lwMonth)
                        {
                            goto LError;
                        }
                        lwMonth = pszs->lwVal;
                        break;
                    }
                    case ParseStringTokenType::Zone:
                    {
                        if (lwNil != lwZone)
                        {
                            goto LError;
                        }
                        lwZone = pszs->lwVal;

                        // look for a time zone offset
                        ss = ('+' == *pch) ? (pch++, ssAddOffset) :
                            ('-' == *pch) ? (pch++, ssSubOffset) : ssNil;
                        break;
                    }
                }
                continue;
            }

            if (FBig(ch) || !isdigit(ch))
            {
                goto LError;
            }

            for (lwT = ch - '0'; !FBig(*pch) && isdigit(*pch); pch++)
            {
                lwT = lwT * 10 + *pch - '0';
            }

            // to avoid overflow
            if (pch - pchBase > 6)
            {
                goto LError;
            }

            // skip to the next real character
            while (0 != (ch = *pch) && (ch <= ' ' || classifier->IsBiDirectionalChar(ch)))
            {
                pch++;
            }

            switch (ss)
            {
                case ssAddOffset:
                case ssSubOffset:
                {
                    AssertMsg(isNextFieldDateNegativeVersion5 == false, "isNextFieldDateNegativeVersion5 == false");

                    if (lwNil != lwOffset)
                        goto LError;
                    lwOffset = lwT < 24 ? lwT * 60 :
                        (lwT % 100) + (lwT / 100) * 60;
                    if (ssSubOffset == ss)
                        lwOffset = -lwOffset;
                    lwZone = 0; // An offset is always with respect to UTC
                    ss = ssNil;
                    break;
                }
                case ssMinutes:
                {
                    AssertMsg(isNextFieldDateNegativeVersion5 == false, "isNextFieldDateNegativeVersion5 == false");

                    if (lwT >= 60)
                        goto LError;
                    lwTime += lwT * 60;
                    ss = (ch == ':') ? (pch++, ssSeconds) : ssNil;
                    break;
                }
                case ssSeconds:
                {
                    AssertMsg(isNextFieldDateNegativeVersion5 == false, "isNextFieldDateNegativeVersion5 == false");

                    if (lwT >= 60)
                        goto LError;
                    lwTime += lwT;
                    ss = ssNil;
                    break;
                }
                case ssDate:
                {
                    AssertMsg(isNextFieldDateNegativeVersion5 == false, "isNextFieldDateNegativeVersion5 == false");

                    if (lwNil != lwDate)
                        goto LError;
                    lwDate = lwT;

                    if ((lwNil == lwYear) && FDateDelimiter(ch))
                    {
                        // We have already parsed the year if the date is specified as YYYY/MM/DD,
                        // but not when it is specified as MM/DD/YYYY.
                        ss = ssYear;
                        pch++;
                    }
                    else
                    {
                        ss = ssNil;
                    }
                    break;
                }
                case ssMonth:
                {
                    AssertMsg(isNextFieldDateNegativeVersion5 == false, "isNextFieldDateNegativeVersion5 == false");

                    if (lwNil != lwMonth)
                    {
                        goto LError;
                    }

                    lwMonth = lwT - 1;

                    if (FDateDelimiter(ch))
                    {
                        // Mark the next token as the date so that it won't be confused with another token.
                        // For example, if the next character is '-' as in "2015-1-1", then it'd be used as
                        // the time offset without this.
                        ss = ssDate;
                        pch++;
                    }

                    break;
                }
                case ssYear:
                {
                    AssertMsg(isNextFieldDateNegativeVersion5 == false, "isNextFieldDateNegativeVersion5 == false");

                    if (lwNil != lwYear)
                        goto LError;

                    AssertMsg(isDateNegativeVersion5 == false, "lwYear should be positive as pre-version:5 parsing");
                    lwYear = lwT;
                    ss = ssNil;
                    break;
                }
                default:
                {
                    // assumptions for getting a YEAR:
                    //    - an absolute value greater or equal than 70 (thus not hour!)
                    //    - wasn't preceded by negative sign for -version:5 year format
                    if (lwT >= 70 || isNextFieldDateNegativeVersion5)
                    {
                        // assume it's a year - this is used particularly as version:5 year parsing
                        if (lwNil != lwYear)
                            goto LError;

                        // handle the case date is negative for "Tue Feb 02 -2012 01:02:03 GMT-0800"
                        lwYear = isDateNegativeVersion5 ? -lwT : lwT;
                        isNextFieldDateNegativeVersion5 = false;

                        if (FDateDelimiter(ch))
                        {
                            // Mark the next token as the month so that it won't be confused with another token.
                            // For example, if the next character is '-' as in "2015-1-1", then it'd be used as
                            // the time offset without this.
                            ss = ssMonth;
                            pch++;
                        }

                        break;
                    }

                    switch (ch)
                    {
                        case ':':
                        {
                            // hour
                            if (lwNil != lwTime)
                                goto LError;
                            if (lwT >= 24)
                                goto LError;
                            lwTime = lwT * 3600;
                            ss = ssMinutes;
                            pch++;
                            break;
                        }
                        case '/':
                        case '-':
                        {
                            // month
                            if (lwNil != lwMonth)
                            {
                                // can be year
                                if (lwNil != lwYear)
                                {
                                    // both were already parsed!
                                    goto LError;
                                }
                                else
                                {
                                    // this is a day - with the negative sign for the date (version 5+)
                                    lwDate = lwT;
                                    isDateNegativeVersion5 = true;

                                    // mark the next field to be year (version 5+)
                                    isNextFieldDateNegativeVersion5 = true;
                                }
                            }
                            else
                            {
                                // this is a month
                                lwMonth = lwT - 1;
                                ss = ssDate;
                            }
                            pch++;
                            break;
                        }
                        default:
                        {
                            // date
                            if (lwNil != lwDate)
                                goto LError;
                            lwDate = lwT;
                            break;
                        }
                    }
                    break;
                }
            }
            continue;
        }

        if (lwNil == lwYear || lwNil == lwMonth || lwNil == lwDate)
        {
            goto LError;
        }

        if (tBcAd != 0)
        {
            if (tBcAd < 0)
            {
                // BC. Note that 1 BC is year 0 and 2 BC is year -1.
                lwYear = -lwYear + 1;
            }
        }
        else if (lwYear < 100 && isDateNegativeVersion5 == false)
        {
            lwYear += 1900;
        }


        if (tAmPm != 0)
        {
            if (lwNil == lwTime)
            {
                goto LError;
            }
            if (lwTime >= 12 * 3600L && lwTime < 13 * 3600L)
            {
                // In the 12:00 hour. AM means subtract 12 hours and PM means
                // do nothing.
                if (tAmPm < 0)
                {
                    lwTime -= 12 * 3600L;
                }
            }
            else
            {
                // Not in the 12:00 hour. AM means do nothing and PM means
                // add 12 hours.
                if (tAmPm > 0)
                {
                    if (lwTime >= 12 * 3600L)
                    {
                        goto LError;
                    }
                    lwTime += 12 * 3600L;
                }
            }
        }
        else if (lwNil == lwTime)
        {
            lwTime = 0;
        }

        if (lwNil != lwZone)
        {
            lwTime -= lwZone * 60;
            fUtc = TRUE;
        }
        else
        {
            fUtc = FALSE;
        }
        if (lwNil != lwOffset)
        {
            lwTime -= lwOffset * 60;
        }

        // Rebuild time.
        tv = TvFromDate(lwYear, lwMonth, lwDate - 1, (double)lwTime * 1000);
        if (!fUtc)
        {
            tv = GetTvUtc(tv, scriptContext);
        }

LError:
        END_TEMP_ALLOCATOR(tempAllocator, scriptContext);
        retVal = tv;
        return true;
    }

    //------------------------------------
    //Convert a utc time to a variant date.
    //------------------------------------
    double DateImplementation::VarDateFromJsUtcTime(double dbl, ScriptContext * scriptContext)
    {
        Assert(scriptContext);

        // Convert to local time.
        dbl = Js::DateImplementation::GetTvLcl(dbl, scriptContext);
        if (!Js::NumberUtilities::IsFinite(dbl))
            return Js::JavascriptNumber::NaN;

        // Convert to an automation date.
        dbl = dbl / 86400000 + g_kdblJanuary1st1970;

        // dbl is the actual number of days since 0000h 12/30/1899.
        // Convert this to a true Automation-style date.

        if (dbl < 0.0)
        {
            // This works around a bug in OLE Automation.
            // If a date is negative _and_ less than 500
            // milliseconds before midnight then Automation will
            // "round" it to two days earlier.  To work around this
            // bug, round dates just before midnight to midnight.
            double dblT;

            dbl = 2.0 * floor(dbl) - dbl;
            dblT = dbl - floor(dbl);
            if (dblT <= kdblHalfSecond && 0.0 < dblT)
                dbl = ceil(dbl) + 1.0;
        }

        return dbl;
    }

    double DateImplementation::JsUtcTimeFromVarDate(double dbl, ScriptContext * scriptContext)
    {
        Assert(scriptContext);

        return GetTvUtc(JsLocalTimeFromVarDate(dbl), scriptContext);
    }

    double DateImplementation::DateFncUTC(ScriptContext* scriptContext, Arguments args)
    {
        const int kcvarMax = 7;
        double rgdbl[kcvarMax];
        double tv;
        double dblT;
        uint ivar;

        for (ivar = 0; (ivar < (args.Info.Count-1)) && ivar < kcvarMax; ++ivar)
        {
            rgdbl[ivar] = JavascriptConversion::ToNumber(args[ivar+1],scriptContext);

        }
        for (ivar = 0; ivar < kcvarMax; ivar++)
        {
            // Unspecified parameters are treated like zero, except date, which
            // is treated as 1.
            if (ivar >= (args.Info.Count - 1))
            {
                rgdbl[ivar] = (ivar == 2);
                continue;
            }
#pragma prefast(suppress:6001, "rgdbl index ivar < args.Info.Count - 1 are initialized")
            dblT = rgdbl[ivar];
            if (!Js::NumberUtilities::IsFinite(dblT))
            {
                return JavascriptNumber::NaN;
            }
            rgdbl[ivar] = ConvertToInteger(dblT);
        }

        // adjust the year
        if (rgdbl[0] < 100 && rgdbl[0] >= 0)
        {
            rgdbl[0] += 1900;
        }

        // REVIEW : do we need to explicitly handle overflow or will the compiler
        // do the right thing (produce Infinity or NaN).
        // Get the local time value.
        tv = TvFromDate(rgdbl[0], rgdbl[1], rgdbl[2] - 1,
            rgdbl[3] * 3600000 + rgdbl[4] * 60000 + rgdbl[5] * 1000 + rgdbl[6]);

        return tv;
    }

    // Maximum number of arguments used by this set operation.
    static const int mpddcvar[] =
    {
        1, // Year
        3, // FullYear
        2, // Month
        1, // Date
        4, // Hours
        3, // Minutes
        2, // Seconds
        1, // Milliseconds
        0, // Day (shouldn't happen)
        0, // TimezoneOffset (shouldn't happen)
    };

    double DateImplementation::SetDateData(Arguments args, DateData dd, bool fUtc, ScriptContext* scriptContext)
    {
        // This must accommodate the largest cvar in mpddcvar.
        double rgdbl[5];

        double tv = 0;
        Js::YMD *pymd = NULL;
        Js::YMD emptyYMD = {0};
        uint count = 0;

        uint cvarMax;
        uint ivar;

        // PREFAST: check limits
        if (dd < 0 || dd >= DateData::Lim)
        {
            AssertMsg(false, "DateData type invalid");
            Js::JavascriptError::ThrowError(scriptContext, VBSERR_InternalError);
        }

        // Get the parameters.
        cvarMax = mpddcvar[dd];

        __analysis_assume(cvarMax <= 4);

        //
        // arg[0] would be the date object itself
        //
        for (ivar = 0; (ivar < (args.Info.Count-1)) && ivar < cvarMax; ++ivar)
        {
            rgdbl[ivar] = JavascriptConversion::ToNumber(args[ivar+1],scriptContext);

            if (!Js::NumberUtilities::IsFinite(rgdbl[ivar]))
                goto LSetNan;

            rgdbl[ivar] = ConvertToInteger(rgdbl[ivar]);
        }

        if ((count = ivar) < 1)
        {
            goto LSetNan;
        }

        if (JavascriptNumber::IsNan(m_tvUtc))
        {


            // If the current time is not finite, the only way we can end up
            // with non-NaN is for setFullYear/setYear.
            // See ES5 15.9.5.40, ES5 B.2.5.
            if (!(DateData::FullYear == dd || DateData::Year == dd))
            {
                goto LSetNan;
            }
            pymd = &emptyYMD;           // We need mon, mday, time to be 0.
            // Fall through to DateData::Year and DataData::FullYear cases below.
        }
        else
        {
            if (fUtc)
            {
                EnsureYmdUtc();
                pymd = &m_ymdUtc;
                tv = m_tvUtc;
            }
            else
            {
                EnsureYmdLcl();
                pymd = &m_ymdLcl;
                tv = m_tvLcl;
            }
        }

        ivar = 0;
        switch (dd)
        {
        case DateData::Year:
            if (rgdbl[0] < 100 && rgdbl[0] >= 0)
                rgdbl[0] += 1900;
            // fall-through
        case DateData::FullYear:
    LFullYear:
            if (count < 3)
            {
                // Only {year} or {year, month} is specified. Day is not specified.
                rgdbl[2] = pymd->mday + 1;
                if (count < 2)
                {
                    // Month is not specified.
                    rgdbl[1] = pymd->mon;
                }
            }
            tv = TvFromDate(rgdbl[0], rgdbl[1], rgdbl[2] - 1, pymd->time);
            break;
        case DateData::Month:
            memmove(rgdbl + 1, rgdbl, count * sizeof(double));
            count++;
            rgdbl[0] = pymd->year;
            goto LFullYear;
        case DateData::Date:
            tv += (rgdbl[ivar] - pymd->mday - 1) * 86400000;
            if (++ivar >= count)
            {
                break;
            }
            // fall-through
        case DateData::Hours:
            tv += (rgdbl[ivar] - (pymd->time / 3600000)) * 3600000;
            if (++ivar >= count)
            {
                break;
            }
            // fall-through
        case DateData::Minutes:
            tv += (rgdbl[ivar] - (pymd->time / 60000) % 60) * 60000;
            if (++ivar >= count)
            {
                break;
            }
            // fall-through
        case DateData::Seconds:
            tv += (rgdbl[ivar] - (pymd->time / 1000) % 60) * 1000;
            if (++ivar >= count)
            {
                break;
            }
            // fall-through
        case DateData::Milliseconds:
            tv += rgdbl[ivar] - pymd->time % 1000;
            break;
        default:
            AssertMsg(false, "DataData type invalid");
        }

        if (fUtc)
        {
            SetTvUtc(tv);
        }
        else
        {
            SetTvLcl(tv);
        }

        m_modified = true;
        return m_tvUtc;

    LSetNan:
        m_grfval = 0;
        m_tvUtc = JavascriptNumber::NaN;
        m_modified = true;

        return m_tvUtc;
    }

} // namespace Js
