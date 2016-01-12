//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    class NumberConstants : public NumberConstantsBase
    {
    public:
        // Our float tagging scheme relies on NaNs to be of this value - changing the NaN value
        // will break float tagging for x64.
        static const uint64     k_PosInf    = 0x7FF0000000000000ull;
        static const uint64     k_NegInf    = 0xFFF0000000000000ull;
        static const uint64     k_PosMin    = 0x0000000000000001ull;
        static const uint64     k_PosMax    = 0x7FEFFFFFFFFFFFFFull;
        static const uint64     k_NegZero   = 0x8000000000000000ull;
        static const uint64     k_Zero      = 0x0000000000000000ull;
        static const uint64     k_PointFive = 0x3FE0000000000000ull;
        static const uint64     k_NegPointFive = 0xBFE0000000000000ull;
        static const uint64     k_NegOne    = 0xBFF0000000000000ull;

        static const uint32     k_Float32Zero      = 0x00000000ul;
        static const uint32     k_Float32PointFive = 0x3F000000ul;
        static const uint32     k_Float32NegPointFive = 0xBF000000ul;

        static const double     MAX_VALUE;
        static const double     MIN_VALUE;
        static const double     NaN;
        static const double     NEGATIVE_INFINITY;
        static const double     POSITIVE_INFINITY;
        static const double     NEG_ZERO;

        static const BYTE AbsDoubleCst[];
        static const BYTE AbsFloatCst[];
        static double const UIntConvertConst[];
        static double const MaskNegDouble[];
        static float const MaskNegFloat[];

    };

    class NumberUtilities : public NumberUtilitiesBase
    {
    public:
        static bool IsDigit(int ch);
        static BOOL NumberUtilities::FHexDigit(wchar_t ch, int *pw);
        static ulong MulLu(ulong lu1, ulong lu2, ulong *pluHi);
        static int AddLu(ulong *plu1, ulong lu2);

        static ulong &LuHiDbl(double &dbl);
        static ulong &LuLoDbl(double &dbl);
        static INT64 TryToInt64(double d);
        static bool IsValidTryToInt64(__int64 value);   // Whether TryToInt64 resulted in a valid value.

        static int CbitZeroLeft(ulong lu);

        static bool IsFinite(double value);
        static bool IsNan(double value);
        static bool IsSpecial(double value, uint64 nSpecial);
        static uint64 ToSpecial(double value);

        // Convert a given UINT16 into its corresponding string.
        // outBufferSize is in WCHAR elements (and used only for ASSERTs)
        // Returns the number of characters written to outBuffer (not including the \0)
        static charcount_t UInt16ToString(uint16 integer, __out __ecount(outBufferSize) WCHAR* outBuffer, charcount_t outBufferSize, char widthForPaddingZerosInsteadSpaces);

        // Try to parse an integer string to find out if the string contains an index property name.
        static BOOL TryConvertToUInt32(const wchar_t* str, int length, uint32* intVal);

        static double Modulus(double dblLeft, double dblRight);

        enum FormatType
        {
            FormatFixed,
            FormatExponential,
            FormatPrecision
        };

        // Implemented in lib\parser\common.  Should move to lib\common
        template<typename EncodedChar>
        static double StrToDbl(const EncodedChar *psz, const EncodedChar **ppchLim, bool& likelyInt);

        static BOOL FDblToStr(double dbl, __out_ecount(nDstBufSize) wchar_t *psz, int nDstBufSize);
        static int FDblToStr(double dbl, NumberUtilities::FormatType ft, int nDigits, __out_ecount(cchDst) wchar_t *pchDst, int cchDst);

        static BOOL FNonZeroFiniteDblToStr(double dbl, _Out_writes_(nDstBufSize) WCHAR* psz, int nDstBufSize);
        _Success_(return) static BOOL FNonZeroFiniteDblToStr(double dbl, _In_range_(2, 36) int radix, _Out_writes_(nDstBufSize) WCHAR* psz, int nDstBufSize);

        static double DblFromDecimal(DECIMAL * pdecIn);

        static void CodePointAsSurrogatePair(codepoint_t codePointValue, __out wchar_t* first, __out wchar_t* second);
        static codepoint_t SurrogatePairAsCodePoint(codepoint_t first, codepoint_t second);

        static bool IsSurrogateUpperPart(codepoint_t codePointValue);
        static bool IsSurrogateLowerPart(codepoint_t codePointValue);

        static bool IsInSupplementaryPlane(codepoint_t codePointValue);

        static long LwFromDblNearest(double dbl);
        static ulong LuFromDblNearest(double dbl);
        static BOOL FDblIsLong(double dbl, long *plw);

        template<typename EncodedChar>
        static double DblFromHex(const EncodedChar *psz, const EncodedChar **ppchLim);
        template <typename EncodedChar>
        static double DblFromBinary(const EncodedChar *psz, const EncodedChar **ppchLim);
        template<typename EncodedChar>
        static double DblFromOctal(const EncodedChar *psz, const EncodedChar **ppchLim);
        template<typename EncodedChar>
        static double StrToDbl(const EncodedChar *psz, const EncodedChar **ppchLim, Js::ScriptContext *const scriptContext);

        const NumberUtilitiesBase* GetNumberUtilitiesBase() const { return static_cast<const NumberUtilitiesBase*>(this); }

    };
}
