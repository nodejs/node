//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonCommonPch.h"
#include "Common\UInt32Math.h"
#include "common\NumberUtilities.inl"

namespace Js
{
    const double NumberConstants::MAX_VALUE = *(double*)(&NumberConstants::k_PosMax);
    const double NumberConstants::MIN_VALUE = *(double*)(&NumberConstants::k_PosMin);
    const double NumberConstants::NaN = *(double*)(&NumberConstants::k_Nan);
    const double NumberConstants::NEGATIVE_INFINITY= *(double*)(&NumberConstants::k_NegInf);
    const double NumberConstants::POSITIVE_INFINITY= *(double*)(&NumberConstants::k_PosInf );
    const double NumberConstants::NEG_ZERO= *(double*)(&NumberConstants::k_NegZero );

    // These are used in 128-bit operations in the JIT and inline asm
    __declspec(align(16)) const BYTE NumberConstants::AbsDoubleCst[] =
    { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F,
      0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F };

    __declspec(align(16)) const BYTE NumberConstants::AbsFloatCst[] =
    { 0xFF, 0xFF, 0xFF, 0x7F,
      0xFF, 0xFF, 0xFF, 0x7F,
      0xFF, 0xFF, 0xFF, 0x7F,
      0xFF, 0xFF, 0xFF, 0x7F };

    __declspec(align(16)) double const NumberConstants::UIntConvertConst[2] = { 0, 4294967296.000000 };
    __declspec(align(16)) float const NumberConstants::MaskNegFloat[] = { -0.0f, -0.0f, -0.0f, -0.0f };
    __declspec(align(16)) double const NumberConstants::MaskNegDouble[] = { -0.0, -0.0 };

    bool NumberUtilities::IsDigit(int ch)
    {
        return ch >= '0' && ch <= '9';
    }

    BOOL NumberUtilities::FHexDigit(wchar_t ch, int *pw)
    {
        if ((ch -= '0') <= 9)
        {
            *pw = ch;
            return TRUE;
        }
        if ((ch -= 'A' - '0') <= 5 || (ch -= 'a' - 'A') <= 5)
        {
            *pw = 10 + ch;
            return TRUE;
        }
        return FALSE;
    }

    /***************************************************************************
    Multiply two unsigned longs. Return the low ulong and fill *pluHi with
    the high ulong.
    ***************************************************************************/
#pragma warning(push)
#pragma warning(disable:4035)   // Turn off warning that there is no return value
    ulong NumberUtilities::MulLu(ulong lu1, ulong lu2, ulong *pluHi)
    {
#if _WIN32 || _WIN64

#if I386_ASM
        __asm
        {
            mov eax, lu1
            mul lu2
                mov ebx, pluHi
                mov DWORD PTR[ebx], edx
        }
#else //!I386_ASM
        DWORDLONG llu = UInt32x32To64(lu1, lu2);

        *pluHi = (ulong)(llu >> 32);
        return (ulong)llu;
#endif //!I386_ASM

#else
#error Neither _WIN32, nor _WIN64 is defined
#endif
    }
#pragma warning(pop)

    /***************************************************************************
    Add two unsigned longs and return the carry bit.
    ***************************************************************************/
    int NumberUtilities::AddLu(ulong *plu1, ulong lu2)
    {
        *plu1 += lu2;
        return *plu1 < lu2;
    }

    bool NumberUtilities::IsFinite(double value)
    {
#if defined(_M_X64_OR_ARM64)
        return 0 != (~(ToSpecial(value)) & 0x7FF0000000000000ull);
#else
        return 0 != (~Js::NumberUtilities::LuHiDbl(value) & 0x7FF00000);
#endif
    }

    int NumberUtilities::CbitZeroLeft(ulong lu)
    {
        int cbit = 0;

        if (0 == (lu & 0xFFFF0000))
        {
            cbit += 16;
            lu <<= 16;
        }
        if (0 == (lu & 0xFF000000))
        {
            cbit += 8;
            lu <<= 8;
        }
        if (0 == (lu & 0xF0000000))
        {
            cbit += 4;
            lu <<= 4;
        }
        if (0 == (lu & 0xC0000000))
        {
            cbit += 2;
            lu <<= 2;
        }
        if (0 == (lu & 0x80000000))
        {
            cbit += 1;
            lu <<= 1;
        }
        Assert(lu & 0x80000000);

        return cbit;
    }

    charcount_t NumberUtilities::UInt16ToString(uint16 integer, __out __ecount(outBufferSize) WCHAR* outBuffer, charcount_t outBufferSize, char widthForPaddingZerosInsteadSpaces)
    {
        // inlined here
        WORD digit;
        charcount_t cchWritten = 0;

        Assert(cchWritten < outBufferSize);

        // word is 0 to 65,535 -- 5 digits max
        if (cchWritten < outBufferSize)
        {
            if (integer >= 10000)
            {
                digit = integer / 10000;
                integer %= 10000;
                *outBuffer = digit + L'0';
                outBuffer++;
                cchWritten++;
            }
            else if( widthForPaddingZerosInsteadSpaces > 4 )
            {
                *outBuffer = L'0';
                outBuffer++;
                cchWritten++;
            }
        }

        Assert(cchWritten < outBufferSize);
        if (cchWritten < outBufferSize)
        {
            if (integer >= 1000)
            {
                digit = integer / 1000;
                integer %= 1000;
                *outBuffer = digit + L'0';
                outBuffer++;
                cchWritten++;
            }
            else if( widthForPaddingZerosInsteadSpaces > 3 )
            {
                *outBuffer = L'0';
                outBuffer++;
                cchWritten++;
            }
        }

        Assert(cchWritten < outBufferSize);
        if (cchWritten < outBufferSize)
        {
            if (integer >= 100)
            {
                digit = integer / 100;
                integer %= 100;
                *outBuffer = digit + L'0';
                outBuffer++;
                cchWritten++;
            }
            else if( widthForPaddingZerosInsteadSpaces > 2 )
            {
                *outBuffer = L'0';
                outBuffer++;
                cchWritten++;
            }
        }

        Assert(cchWritten < outBufferSize);
        if (cchWritten < outBufferSize)
        {
            if (integer >= 10)
            {
                digit = integer / 10;
                integer %= 10;
                *outBuffer = digit + L'0';
                outBuffer++;
                cchWritten++;
            }
            else if( widthForPaddingZerosInsteadSpaces > 1 )
            {
                *outBuffer = L'0';
                outBuffer++;
                cchWritten++;
            }
        }

        Assert(cchWritten < outBufferSize);
        if (cchWritten < outBufferSize)
        {
            *outBuffer = integer + L'0';
            outBuffer++;
            cchWritten++;
        }

        Assert(cchWritten < outBufferSize);
        if (cchWritten < outBufferSize)
        {
            // cchWritten doesn't include the terminating char, like swprintf_s
            *outBuffer = 0;
        }

        return cchWritten;
    }

    BOOL NumberUtilities::TryConvertToUInt32(const wchar_t* str, int length, uint32* intVal)
    {
        if (length <= 0 || length > 10)
        {
            return false;
        }
        if (length == 1)
        {
            if (str[0] >= L'0' && str[0] <= L'9')
            {
                *intVal = (uint32)(str[0] - L'0');
                return true;
            }
            else
            {
                return false;
            }
        }
        if (str[0] < L'1' || str[0] > L'9')
        {
            return false;
        }
        uint32 val = (uint32)(str[0] - L'0');
        int calcLen = min(length, 9);
        for (int i = 1; i < calcLen; i++)
        {
            if ((str[i] < L'0')|| (str[i] > L'9'))
            {
                return false;
            }
            val = (val * 10) + (uint32)(str[i] - L'0');
        }
        if (length == 10)
        {
            // check for overflow 4294967295
            if (str[9] < L'0' || str[9] > L'9' ||
                UInt32Math::Mul(val, 10, &val) ||
                UInt32Math::Add(val, (uint32)(str[9] - L'0'), &val))
            {
                return false;
            }
        }
        *intVal = val;
        return true;
    }

    double NumberUtilities::Modulus(double dblLeft, double dblRight)
    {
        double value = 0;

        if (!Js::NumberUtilities::IsFinite(dblRight))
        {
            if (NumberUtilities::IsNan(dblRight) || !Js::NumberUtilities::IsFinite(dblLeft))
            {
                value = NumberConstants::NaN;
            }
            else
            {
                value =  dblLeft;
            }
        }
        else if (0 == dblRight || NumberUtilities::IsNan(dblLeft))
        {
            value =  NumberConstants::NaN;
        }
        else if (0 == dblLeft)
        {
            value =  dblLeft;
        }
        else
        {
            value = fmod(dblLeft, dblRight);
        }

        return value;
    }


    long NumberUtilities::LwFromDblNearest(double dbl)
    {
        if (Js::NumberUtilities::IsNan(dbl))
            return 0;
        if (dbl > 0x7FFFFFFFL)
            return 0x7FFFFFFFL;
        if (dbl < (long)0x80000000L)
            return (long)0x80000000L;
        return (long)dbl;
    }

    ulong NumberUtilities::LuFromDblNearest(double dbl)
    {
        if (Js::NumberUtilities::IsNan(dbl))
            return 0;
        if (dbl >(ulong)0xFFFFFFFFUL)
            return (ulong)0xFFFFFFFFUL;
        if (dbl < 0)
            return 0;
        return (ulong)dbl;
    }

    BOOL NumberUtilities::FDblIsLong(double dbl, long *plw)
    {
        AssertMem(plw);
        double dblT;

        *plw = (long)dbl;
        dblT = (double)*plw;
        return Js::NumberUtilities::LuHiDbl(dblT) == Js::NumberUtilities::LuHiDbl(dbl) && Js::NumberUtilities::LuLoDbl(dblT) == Js::NumberUtilities::LuLoDbl(dbl);
    }


    template<typename EncodedChar>
    double NumberUtilities::DblFromHex(const EncodedChar *psz, const EncodedChar **ppchLim)
    {
        double dbl;
        uint uT;
        byte bExtra;
        int cbit;

        // Skip leading zeros.
        while (*psz == '0')
            psz++;

        dbl = 0;
        Assert(Js::NumberUtilities::LuHiDbl(dbl) == 0);
        Assert(Js::NumberUtilities::LuLoDbl(dbl) == 0);

        // Get the first digit.
        if ((uT = *psz - '0') > 9)
        {
            if ((uT -= 'A' - '0') <= 5 || (uT -= 'a' - 'A') <= 5)
                uT += 10;
            else
            {
                *ppchLim = psz;
                return dbl;
            }
        }
        psz++;

        if (uT & 0x08)
        {
            cbit = 4;
            Js::NumberUtilities::LuHiDbl(dbl) |= (ulong)(uT & 0x07) << 17;
        }
        else if (uT & 0x04)
        {
            cbit = 3;
            Js::NumberUtilities::LuHiDbl(dbl) |= (ulong)(uT & 0x03) << 18;
        }
        else if (uT & 0x02)
        {
            cbit = 2;
            Js::NumberUtilities::LuHiDbl(dbl) |= (ulong)(uT & 0x01) << 19;
        }
        else
        {
            Assert(uT & 0x01);
            cbit = 1;
        }
        bExtra = 0;

        for (; ; psz++)
        {
            if ((uT = (*psz - '0')) > 9)
            {
                if ((uT -= 'A' - '0') <= 5 || (uT -= 'a' - 'A') <= 5)
                    uT += 10;
                else
                    break;
            }

            if (cbit <= 17)
                Js::NumberUtilities::LuHiDbl(dbl) |= (ulong)uT << (17 - cbit);
            else if (cbit < 21)
            {
                Js::NumberUtilities::LuHiDbl(dbl) |= (ulong)uT >> (cbit - 17);
                Js::NumberUtilities::LuLoDbl(dbl) |= (ulong)uT << (49 - cbit);
            }
            else if (cbit <= 49)
                Js::NumberUtilities::LuLoDbl(dbl) |= (ulong)uT << (49 - cbit);
            else if (cbit <= 53)
            {
                Js::NumberUtilities::LuLoDbl(dbl) |= (ulong)uT >> (cbit - 49);
                bExtra = (byte)(uT << (57 - cbit));
            }
            else if (0 != uT)
                bExtra |= 1;
            cbit += 4;
        }

        // Set the lim.
        *ppchLim = psz;

        // Set the exponent.
        cbit += 1022;
        if (cbit > 2046)
        {
            // Overflow to Infinity
            Js::NumberUtilities::LuHiDbl(dbl) = 0x7FF00000;
            Js::NumberUtilities::LuLoDbl(dbl) = 0;
            return dbl;
        }
        Js::NumberUtilities::LuHiDbl(dbl) |= (ulong)cbit << 20;

        // Use bExtra to round.
        if ((bExtra & 0x80) && ((bExtra & 0x7F) || (Js::NumberUtilities::LuLoDbl(dbl) & 1)))
        {
            // Round up. Note that this overflows the mantissa correctly,
            // even to Infinity.
            if (0 == ++Js::NumberUtilities::LuLoDbl(dbl))
                ++Js::NumberUtilities::LuHiDbl(dbl);
        }

        return dbl;
    }

    template <typename EncodedChar>
    double NumberUtilities::DblFromBinary(const EncodedChar *psz, const EncodedChar **ppchLim)
    {
        double dbl = 0;
        Assert(Js::NumberUtilities::LuHiDbl(dbl) == 0);
        Assert(Js::NumberUtilities::LuLoDbl(dbl) == 0);
        uint uT;
        byte bExtra = 0;
        int cbit = 0;
        // Skip leading zeros.
        while (*psz == '0')
            psz++;
        // Get the first digit.
        uT = *psz - '0';
        if (uT > 1)
        {
            *ppchLim = psz;
            return dbl;
        }
        //Now that leading zeros are skipped first bit should be one so lets
        //go ahead and count it and increment psz
        cbit = 1;
        psz++;

        // According to the existing implementations these numbers
        // should n bits away from 21 and 53. The n bits are determined by the
        // numerical type. for example since 4 bits are necessary to represent a
        // hexadecimal number and 3 bits to represent an octal you will see that
        // the hex case is represented by 21-4 = 17 and the octal case is represented
        // by 21-3 = 18, thus for binary where 1 bit is need to represent 2 numbers 21-1 = 20
        const uint rightShiftValue = 20;
        // Why 52? 52 is the last explicit bit and 1 bit away from 53 (max bits of precision
        // for double precision floating point)
        const uint leftShiftValue = 52;
        for (; (uT = (*psz - '0')) <= 1; psz++)
        {
            if (cbit <= rightShiftValue)
            {
                Js::NumberUtilities::LuHiDbl(dbl) |= (ulong)uT << (rightShiftValue - cbit);

            }
            else if (cbit <= leftShiftValue)
            {
                Js::NumberUtilities::LuLoDbl(dbl) |= (ulong)uT << (leftShiftValue - cbit);
            }
            else if (cbit == leftShiftValue + 1)//53 bits
            {
                Js::NumberUtilities::LuLoDbl(dbl) |= (ulong)uT >> (cbit - leftShiftValue);
                bExtra = (byte)(uT << (60 - cbit));
            }
            else if (0 != uT)
            {
                bExtra |= 1;
            }
            cbit++;
        }
        // Set the lim.
        *ppchLim = psz;

        // Set the exponent.
        cbit += 1022;
        if (cbit > 2046)
        {
            // Overflow to Infinity
            Js::NumberUtilities::LuHiDbl(dbl) = 0x7FF00000;
            Js::NumberUtilities::LuLoDbl(dbl) = 0;
            return dbl;
        }

        Js::NumberUtilities::LuHiDbl(dbl) |= (ulong)cbit << 20;

        // Use bExtra to round.
        if ((bExtra & 0x80) && ((bExtra & 0x7F) || (Js::NumberUtilities::LuLoDbl(dbl) & 1)))
        {
            // Round up. Note that this overflows the mantissa correctly,
            // even to Infinity.
            if (0 == ++Js::NumberUtilities::LuLoDbl(dbl))
                ++Js::NumberUtilities::LuHiDbl(dbl);
        }
        return dbl;
    }

    template <typename EncodedChar>
    double NumberUtilities::DblFromOctal(const EncodedChar *psz, const EncodedChar **ppchLim)
    {
        double dbl;
        uint uT;
        byte bExtra;
        int cbit;

        // Skip leading zeros.
        while (*psz == '0')
            psz++;

        dbl = 0;
        Assert(Js::NumberUtilities::LuHiDbl(dbl) == 0);
        Assert(Js::NumberUtilities::LuLoDbl(dbl) == 0);

        // Get the first digit.
        uT = *psz - '0';
        if (uT > 7)
        {
            *ppchLim = psz;
            return dbl;
        }
        psz++;

        if (uT & 0x04)//is the 3rd bit set
        {
            cbit = 3;
            Js::NumberUtilities::LuHiDbl(dbl) |= (ulong)(uT & 0x03) << 18;
        }
        else if (uT & 0x02)//is the 2nd bit set
        {
            cbit = 2;
            Js::NumberUtilities::LuHiDbl(dbl) |= (ulong)(uT & 0x01) << 19;
        }
        else// then is the first bit set
        {
            Assert(uT & 0x01);
            cbit = 1;
        }
        bExtra = 0;

        for (; (uT = (*psz - '0')) <= 7; psz++)
        {
            if (cbit <= 18)
                Js::NumberUtilities::LuHiDbl(dbl) |= (ulong)uT << (18 - cbit);
            else if (cbit < 21)
            {
                Js::NumberUtilities::LuHiDbl(dbl) |= (ulong)uT >> (cbit - 18);
                Js::NumberUtilities::LuLoDbl(dbl) |= (ulong)uT << (50 - cbit);
            }
            else if (cbit <= 50)
                Js::NumberUtilities::LuLoDbl(dbl) |= (ulong)uT << (50 - cbit);
            else if (cbit <= 53)
            {
                Js::NumberUtilities::LuLoDbl(dbl) |= (ulong)uT >> (cbit - 50);
                bExtra = (byte)(uT << (58 - cbit));
            }
            else if (0 != uT)
                bExtra |= 1;
            cbit += 3;
        }

        // Set the lim.
        *ppchLim = psz;

        // Set the exponent.
        cbit += 1022;
        if (cbit > 2046)
        {
            // Overflow to Infinity
            Js::NumberUtilities::LuHiDbl(dbl) = 0x7FF00000;
            Js::NumberUtilities::LuLoDbl(dbl) = 0;
            return dbl;

        }
        Js::NumberUtilities::LuHiDbl(dbl) |= (ulong)cbit << 20;

        // Use bExtra to round.
        if ((bExtra & 0x80) && ((bExtra & 0x7F) || (Js::NumberUtilities::LuLoDbl(dbl) & 1)))
        {
            // Round up. Note that this overflows the mantissa correctly,
            // even to Infinity.
            if (0 == ++Js::NumberUtilities::LuLoDbl(dbl))
                ++Js::NumberUtilities::LuHiDbl(dbl);
        }

        return dbl;
    }

    template <typename EncodedChar>
    double NumberUtilities::StrToDbl(const EncodedChar * psz, const EncodedChar **ppchLim, Js::ScriptContext *const scriptContext)
    {
        Assert(scriptContext);
        bool likelyInt = true;
        return Js::NumberUtilities::StrToDbl<EncodedChar>(psz, ppchLim, likelyInt);
    }

    template double NumberUtilities::StrToDbl<wchar_t>(const wchar_t * psz, const wchar_t **ppchLim, Js::ScriptContext *const scriptContext);
    template double NumberUtilities::StrToDbl<utf8char_t>(const utf8char_t * psz, const utf8char_t **ppchLim, Js::ScriptContext *const scriptContext);
    template double NumberUtilities::DblFromHex<wchar_t>(const wchar_t *psz, const wchar_t **ppchLim);
    template double NumberUtilities::DblFromHex<utf8char_t>(const utf8char_t *psz, const utf8char_t **ppchLim);
    template double NumberUtilities::DblFromBinary<wchar_t>(const wchar_t *psz, const wchar_t **ppchLim);
    template double NumberUtilities::DblFromBinary<utf8char_t>(const utf8char_t *psz, const utf8char_t **ppchLim);
    template double NumberUtilities::DblFromOctal<wchar_t>(const wchar_t *psz, const wchar_t **ppchLim);
    template double NumberUtilities::DblFromOctal<utf8char_t>(const utf8char_t *psz, const utf8char_t **ppchLim);
}
