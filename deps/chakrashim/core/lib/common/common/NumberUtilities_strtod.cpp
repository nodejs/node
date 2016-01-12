//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include <CommonCommonPch.h>
#include "DataStructures\BigInt.h"

namespace Js
{
static inline BOOL FNzDigit(int ch)
{ return ch >= '1' && ch <= '9'; }

static const long klwMaxExp10 = 310;     // Upper bound on base 10 exponent
static const long klwMinExp10 = -325;    // Lower bound on base 10 exponent
static const int kcbMaxRgb = 50;
static const int kcchMaxSig = 20;        // 20 significant digits. ECMA allows this.

// Small powers of ten. These are all the powers of ten that have an exact
// representation in IEEE double precision format.
static const double g_rgdblTens[] =
{
    1e00, 1e01, 1e02, 1e03, 1e04, 1e05, 1e06, 1e07, 1e08, 1e09,
    1e10, 1e11, 1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19,
    1e20, 1e21, 1e22, 1e23, 1e24, 1e25, 1e26, 1e27, 1e28
};

static inline wchar_t ToDigit(long wVal)
{
    //return reinterpret_cast<wchar_t>((wVal < 10) ? '0' + (ushort) wVal : 'a' - 10 + (ushort) wVal);
    return (ushort)((wVal < 10) ? '0' + (ushort) wVal : 'a' - 10 + (ushort) wVal);
}

/***************************************************************************
Big floating point number.
***************************************************************************/
struct BIGNUM
{
    // WARNING: some asm code below assumes the ordering of these fields.
    ulong m_lu0;
    ulong m_lu1;
    ulong m_lu2;
    int m_wExp;

    // This is a bound on the absolute value of the error. It is based at
    // one bit before the least significant bit of m_lu0.
    ulong m_luError;

    // Test to see if the num is zero. This works even if we're not normalized.
    BOOL FZero(void)
    { return m_lu2 == 0 && m_lu1 == 0 && m_lu0 == 0; }

    // Normalize the big number - make sure the high bit is 1 or
    // everything is zero (including the exponent).
    void Normalize(void);

    // Round based on the given extra data using IEEE round to nearest rule.
    void Round(ulong luExtra)
    {
        if (0 == (luExtra & 0x80000000) ||
            0 == (luExtra & 0x7FFFFFFF) && 0 == (m_lu0 & 1))
        {
            if (luExtra)
                m_luError++;
            return;
        }
        m_luError++;

        // Round up.
        if (Js::NumberUtilities::AddLu(&m_lu0, 1) &&
            Js::NumberUtilities::AddLu(&m_lu1, 1) &&
            Js::NumberUtilities::AddLu(&m_lu2, 1))
        {
            Assert(0 == m_lu0);
            Assert(0 == m_lu1);
            Assert(0 == m_lu2);
            m_lu2 = 0x80000000;
            m_wExp++;
        }
    }

    // Multiply by ten and add a base 10 digit.
    void MulTenAdd(byte bAdd, ulong *pluExtra);

    // Set the value from decimal digits and decimal exponent
    template <typename EncodedChar>
    void SetFromRgchExp(const EncodedChar *pch, long cch, long lwExp);

    // Multiply by a BIGNUM
    void Mul(const BIGNUM *pnumOp);

    // Get the double value.
    double GetDbl(void);

    // Lop off the integer part and return it.
    ulong LuMod1(void)
    {
        if (m_wExp <= 0)
            return 0;
        Assert(m_wExp <= 32);
        ulong luT = m_lu2 >> (32 - m_wExp);
        m_lu2 &= 0x7FFFFFFF >> (m_wExp - 1);
        Normalize();
        return luT;
    }

    // If m_luError is not zero, add it and set m_luError to zero.
    void MakeUpperBound(void)
    {
        Assert(m_luError < 0xFFFFFFFF);
        ulong luT = (m_luError + 1) >> 1;

        if (luT &&
            Js::NumberUtilities::AddLu(&m_lu0, luT) &&
            Js::NumberUtilities::AddLu(&m_lu1, 1) &&
            Js::NumberUtilities::AddLu(&m_lu2, 1))
        {
            Assert(m_lu2 == 0);
            Assert(m_lu1 == 0);
            m_lu2 = 0x80000000;
            m_lu0 = (m_lu0 >> 1) + (m_lu0 & 1);
            m_wExp++;
        }
        m_luError = 0;
    }

    // If m_luError is not zero, subtract it and set m_luError to zero.
    void MakeLowerBound(void)
    {
        Assert(m_luError < 0xFFFFFFFF);
        ulong luT = (m_luError + 1) >> 1;

        if (luT &&
            !Js::NumberUtilities::AddLu(&m_lu0, (ulong)-(long)luT) &&
            !Js::NumberUtilities::AddLu(&m_lu1, 0xFFFFFFFF))
        {
            Js::NumberUtilities::AddLu(&m_lu2, 0xFFFFFFFF);
            if (0 == (0x80000000 & m_lu2))
                Normalize();
        }
        m_luError = 0;
    }
};


// Positive powers of 10 to 96 bits precision.
static const BIGNUM g_rgnumPos[46] =
{
    { 0x00000000, 0x00000000, 0xA0000000,     4, 0 }, // 10^1
    { 0x00000000, 0x00000000, 0xC8000000,     7, 0 }, // 10^2
    { 0x00000000, 0x00000000, 0xFA000000,    10, 0 }, // 10^3
    { 0x00000000, 0x00000000, 0x9C400000,    14, 0 }, // 10^4
    { 0x00000000, 0x00000000, 0xC3500000,    17, 0 }, // 10^5
    { 0x00000000, 0x00000000, 0xF4240000,    20, 0 }, // 10^6
    { 0x00000000, 0x00000000, 0x98968000,    24, 0 }, // 10^7
    { 0x00000000, 0x00000000, 0xBEBC2000,    27, 0 }, // 10^8
    { 0x00000000, 0x00000000, 0xEE6B2800,    30, 0 }, // 10^9
    { 0x00000000, 0x00000000, 0x9502F900,    34, 0 }, // 10^10
    { 0x00000000, 0x00000000, 0xBA43B740,    37, 0 }, // 10^11
    { 0x00000000, 0x00000000, 0xE8D4A510,    40, 0 }, // 10^12
    { 0x00000000, 0x00000000, 0x9184E72A,    44, 0 }, // 10^13
    { 0x00000000, 0x80000000, 0xB5E620F4,    47, 0 }, // 10^14
    { 0x00000000, 0xA0000000, 0xE35FA931,    50, 0 }, // 10^15
    { 0x00000000, 0x04000000, 0x8E1BC9BF,    54, 0 }, // 10^16
    { 0x00000000, 0xC5000000, 0xB1A2BC2E,    57, 0 }, // 10^17
    { 0x00000000, 0x76400000, 0xDE0B6B3A,    60, 0 }, // 10^18
    { 0x00000000, 0x89E80000, 0x8AC72304,    64, 0 }, // 10^19
    { 0x00000000, 0xAC620000, 0xAD78EBC5,    67, 0 }, // 10^20
    { 0x00000000, 0x177A8000, 0xD8D726B7,    70, 0 }, // 10^21
    { 0x00000000, 0x6EAC9000, 0x87867832,    74, 0 }, // 10^22
    { 0x00000000, 0x0A57B400, 0xA968163F,    77, 0 }, // 10^23
    { 0x00000000, 0xCCEDA100, 0xD3C21BCE,    80, 0 }, // 10^24
    { 0x00000000, 0x401484A0, 0x84595161,    84, 0 }, // 10^25
    { 0x00000000, 0x9019A5C8, 0xA56FA5B9,    87, 0 }, // 10^26
    { 0x00000000, 0xF4200F3A, 0xCECB8F27,    90, 0 }, // 10^27
    { 0x40000000, 0xF8940984, 0x813F3978,    94, 0 }, // 10^28
    { 0x50000000, 0x36B90BE5, 0xA18F07D7,    97, 0 }, // 10^29
    { 0xA4000000, 0x04674EDE, 0xC9F2C9CD,   100, 0 }, // 10^30
    { 0x4D000000, 0x45812296, 0xFC6F7C40,   103, 0 }, // 10^31
    { 0xF0200000, 0x2B70B59D, 0x9DC5ADA8,   107, 0 }, // 10^32
    { 0x3CBF6B72, 0xFFCFA6D5, 0xC2781F49,   213, 1 }, // 10^64   (rounded up)
    { 0xC5CFE94F, 0xC59B14A2, 0xEFB3AB16,   319, 1 }, // 10^96   (rounded up)
    { 0xC66F336C, 0x80E98CDF, 0x93BA47C9,   426, 1 }, // 10^128
    { 0x577B986B, 0x7FE617AA, 0xB616A12B,   532, 1 }, // 10^160
    { 0x85BBE254, 0x3927556A, 0xE070F78D,   638, 1 }, // 10^192  (rounded up)
    { 0x82BD6B71, 0xE33CC92F, 0x8A5296FF,   745, 1 }, // 10^224  (rounded up)
    { 0xDDBB901C, 0x9DF9DE8D, 0xAA7EEBFB,   851, 1 }, // 10^256  (rounded up)
    { 0x73832EEC, 0x5C6A2F8C, 0xD226FC19,   957, 1 }, // 10^288
    { 0xE6A11583, 0xF2CCE375, 0x81842F29,  1064, 1 }, // 10^320
    { 0x5EBF18B7, 0xDB900AD2, 0x9FA42700,  1170, 1 }, // 10^352  (rounded up)
    { 0x1027FFF5, 0xAEF8AA17, 0xC4C5E310,  1276, 1 }, // 10^384
    { 0xB5E54F71, 0xE9B09C58, 0xF28A9C07,  1382, 1 }, // 10^416
    { 0xA7EA9C88, 0xEBF7F3D3, 0x957A4AE1,  1489, 1 }, // 10^448
    { 0x7DF40A74, 0x0795A262, 0xB83ED8DC,  1595, 1 }, // 10^480
};

// Negative powers of 10 to 96 bits precision.
static const BIGNUM g_rgnumNeg[46] =
{
    { 0xCCCCCCCD, 0xCCCCCCCC, 0xCCCCCCCC,    -3, 1 }, // 10^-1   (rounded up)
    { 0x3D70A3D7, 0x70A3D70A, 0xA3D70A3D,    -6, 1 }, // 10^-2
    { 0x645A1CAC, 0x8D4FDF3B, 0x83126E97,    -9, 1 }, // 10^-3
    { 0xD3C36113, 0xE219652B, 0xD1B71758,   -13, 1 }, // 10^-4
    { 0x0FCF80DC, 0x1B478423, 0xA7C5AC47,   -16, 1 }, // 10^-5
    { 0xA63F9A4A, 0xAF6C69B5, 0x8637BD05,   -19, 1 }, // 10^-6   (rounded up)
    { 0x3D329076, 0xE57A42BC, 0xD6BF94D5,   -23, 1 }, // 10^-7
    { 0xFDC20D2B, 0x8461CEFC, 0xABCC7711,   -26, 1 }, // 10^-8
    { 0x31680A89, 0x36B4A597, 0x89705F41,   -29, 1 }, // 10^-9   (rounded up)
    { 0xB573440E, 0xBDEDD5BE, 0xDBE6FECE,   -33, 1 }, // 10^-10
    { 0xF78F69A5, 0xCB24AAFE, 0xAFEBFF0B,   -36, 1 }, // 10^-11
    { 0xF93F87B7, 0x6F5088CB, 0x8CBCCC09,   -39, 1 }, // 10^-12
    { 0x2865A5F2, 0x4BB40E13, 0xE12E1342,   -43, 1 }, // 10^-13
    { 0x538484C2, 0x095CD80F, 0xB424DC35,   -46, 1 }, // 10^-14  (rounded up)
    { 0x0F9D3701, 0x3AB0ACD9, 0x901D7CF7,   -49, 1 }, // 10^-15
    { 0x4C2EBE68, 0xC44DE15B, 0xE69594BE,   -53, 1 }, // 10^-16
    { 0x09BEFEBA, 0x36A4B449, 0xB877AA32,   -56, 1 }, // 10^-17  (rounded up)
    { 0x3AFF322E, 0x921D5D07, 0x9392EE8E,   -59, 1 }, // 10^-18
    { 0x2B31E9E4, 0xB69561A5, 0xEC1E4A7D,   -63, 1 }, // 10^-19  (rounded up)
    { 0x88F4BB1D, 0x92111AEA, 0xBCE50864,   -66, 1 }, // 10^-20  (rounded up)
    { 0xD3F6FC17, 0x74DA7BEE, 0x971DA050,   -69, 1 }, // 10^-21  (rounded up)
    { 0x5324C68B, 0xBAF72CB1, 0xF1C90080,   -73, 1 }, // 10^-22
    { 0x75B7053C, 0x95928A27, 0xC16D9A00,   -76, 1 }, // 10^-23
    { 0xC4926A96, 0x44753B52, 0x9ABE14CD,   -79, 1 }, // 10^-24
    { 0x3A83DDBE, 0xD3EEC551, 0xF79687AE,   -83, 1 }, // 10^-25  (rounded up)
    { 0x95364AFE, 0x76589DDA, 0xC6120625,   -86, 1 }, // 10^-26
    { 0x775EA265, 0x91E07E48, 0x9E74D1B7,   -89, 1 }, // 10^-27  (rounded up)
    { 0x8BCA9D6E, 0x8300CA0D, 0xFD87B5F2,   -93, 1 }, // 10^-28
    { 0x096EE458, 0x359A3B3E, 0xCAD2F7F5,   -96, 1 }, // 10^-29
    { 0xA125837A, 0x5E14FC31, 0xA2425FF7,   -99, 1 }, // 10^-30  (rounded up)
    { 0x80EACF95, 0x4B43FCF4, 0x81CEB32C,  -102, 1 }, // 10^-31  (rounded up)
    { 0x67DE18EE, 0x453994BA, 0xCFB11EAD,  -106, 1 }, // 10^-32  (rounded up)
    { 0x3F2398D7, 0xA539E9A5, 0xA87FEA27,  -212, 1 }, // 10^-64
    { 0x11DBCB02, 0xFD75539B, 0x88B402F7,  -318, 1 }, // 10^-96
    { 0xAC7CB3F7, 0x64BCE4A0, 0xDDD0467C,  -425, 1 }, // 10^-128 (rounded up)
    { 0x59ED2167, 0xDB73A093, 0xB3F4E093,  -531, 1 }, // 10^-160
    { 0x7B6306A3, 0x5423CC06, 0x91FF8377,  -637, 1 }, // 10^-192
    { 0xA4F8BF56, 0x4A314EBD, 0xECE53CEC,  -744, 1 }, // 10^-224
    { 0xFA911156, 0x637A1939, 0xC0314325,  -850, 1 }, // 10^-256 (rounded up)
    { 0x4EE367F9, 0x836AC577, 0x9BECCE62,  -956, 1 }, // 10^-288
    { 0x8920B099, 0x478238D0, 0xFD00B897, -1063, 1 }, // 10^-320 (rounded up)
    { 0x0092757C, 0x46F34F7D, 0xCD42A113, -1169, 1 }, // 10^-352 (rounded up)
    { 0x88DBA000, 0xB11B0857, 0xA686E3E8, -1275, 1 }, // 10^-384 (rounded up)
    { 0x1A4EB007, 0x3FFC68A6, 0x871A4981, -1381, 1 }, // 10^-416 (rounded up)
    { 0x84C663CF, 0xB6074244, 0xDB377599, -1488, 1 }, // 10^-448 (rounded up)
    { 0x61EB52E2, 0x79007736, 0xB1D983B4, -1594, 1 }, // 10^-480
};


void BIGNUM::Normalize(void)
{
    int w1, w2;

    // Normalize mantissa
    if (m_lu2 == 0)
    {
        if (m_lu1 == 0)
        {
            if (m_lu0 == 0)
            {
                m_wExp = 0;
                return;
            }
            m_lu2 = m_lu0;
            m_lu0 = 0;
            m_wExp -= 64;
        }
        else
        {
            m_lu2 = m_lu1;
            m_lu1 = m_lu0;
            m_lu0 = 0;
            m_wExp -= 32;
        }
    }

    if (0 != (w1 = Js::NumberUtilities::CbitZeroLeft(m_lu2)))
    {
        w2 = 32 - w1;
        m_lu2 = (m_lu2 << w1) | (m_lu1 >> w2);
        m_lu1 = (m_lu1 << w1) | (m_lu0 >> w2);
        m_lu0 = (m_lu0 << w1);
        m_wExp -= w1;
    }
}


void BIGNUM::MulTenAdd(byte bAdd, ulong *pluExtra)
{
    Assert(bAdd <= 9);
    Assert(m_lu2 & 0x80000000);

    ulong rglu[5];

    // First "multiply" by eight
    m_wExp += 3;
    Assert(m_wExp >= 4);

    // Initialize the carry values based on bAdd and m_wExp.
    memset(rglu, 0, sizeof(rglu));
    if (0 != bAdd)
    {
        int ilu = 3 - (m_wExp >> 5);
        if (ilu < 0)
            rglu[0] = 1;
        else
        {
            int ibit = m_wExp & 0x1F;
            if (ibit < 4)
            {
                Assert(ilu < 4);
                rglu[ilu + 1] = bAdd >> ibit;
                if (ibit > 0)
                    rglu[ilu] = (ulong)bAdd << (32 - ibit);
            }
            else
            {
                Assert(ilu < 5);
                rglu[ilu] = (ulong)bAdd << (32 - ibit);
            }
        }
    }

    // Shift and add to multiply by ten.
    rglu[1] += Js::NumberUtilities::AddLu(&rglu[0], m_lu0 << 30);
    rglu[2] += Js::NumberUtilities::AddLu(&m_lu0, (m_lu0 >> 2) + (m_lu1 << 30));
    if (rglu[1])
        rglu[2] += Js::NumberUtilities::AddLu(&m_lu0, rglu[1]);
    rglu[3] += Js::NumberUtilities::AddLu(&m_lu1, (m_lu1 >> 2) + (m_lu2 << 30));
    if (rglu[2])
        rglu[3] += Js::NumberUtilities::AddLu(&m_lu1, rglu[2]);
    rglu[4] = Js::NumberUtilities::AddLu(&m_lu2, (m_lu2 >> 2) + rglu[3]);

    // Handle the final carry.
    if (rglu[4])
    {
        Assert(rglu[4] == 1);
        rglu[0] = (rglu[0] >> 1) | (rglu[0] & 1) | (m_lu0 << 31);
        m_lu0 = (m_lu0 >> 1) | (m_lu1 << 31);
        m_lu1 = (m_lu1 >> 1) | (m_lu2 << 31);
        m_lu2 = (m_lu2 >> 1) | 0x80000000;
        m_wExp++;
    }

    *pluExtra = rglu[0];
}

template<typename EncodedChar>
void BIGNUM::SetFromRgchExp(const EncodedChar *prgch, long cch, long lwExp)
{
    Assert(cch > 0);
    AssertArrMemR(prgch, cch);

    const BIGNUM *prgnum;
    int wT;
    ulong luExtra;
    const EncodedChar *pchLim = prgch + cch;

    // Record the first digit
    Assert(FNzDigit(prgch[0]));
    m_lu2 = (ulong)(prgch[0] - '0') << 28;
    m_lu1 = 0;
    m_lu0 = 0;
    m_wExp = 4;
    m_luError = 0;
    lwExp--;
    Normalize();

    while (++prgch < pchLim)
    {
        if (*prgch == '.')
            continue;
        Assert(Js::NumberUtilities::IsDigit(*prgch));
        MulTenAdd((byte) (*prgch - '0'), &luExtra);
        lwExp--;
        if (0 != luExtra)
        {
            // We've filled up our precision.
            Round(luExtra);
            if (prgch < pchLim + 1)
            {
                // There are more digits, so add another error bit just for
                // safety's sake.
                m_luError++;
            }
            break;
        }
    }

    // Now multiply by 10^lwExp
    if (0 == lwExp)
        return;

    if (lwExp < 0)
    {
        prgnum = g_rgnumNeg;
        lwExp = -lwExp;
    }
    else
        prgnum = g_rgnumPos;

    Assert(lwExp > 0 && lwExp < 512);
    wT = (int)lwExp & 0x1F;
    if (wT > 0)
        Mul(&prgnum[wT - 1]);

    wT = ((int)lwExp >> 5) & 0x0F;
    if (wT > 0)
        Mul(&prgnum[wT + 30]);
}


void BIGNUM::Mul(const BIGNUM *pnumOp)
{
    ulong rglu[6];

    Assert(m_lu2 & 0x80000000);
    Assert(pnumOp->m_lu2 & 0x80000000);

    memset(rglu, 0, sizeof(rglu));

#if I386_ASM
    __asm
    {
        mov edi,this
        mov esi,pnumOp
        lea ebx,rglu

        // first "digit" of pnumOp
        mov ecx,DWORD PTR [esi]
        cmp ecx,0
        jz  LDigit2

        mov eax,DWORD PTR [edi]
        mul ecx
        mov DWORD PTR [ebx],eax
        mov DWORD PTR [ebx+4],edx

        mov eax,DWORD PTR [edi+4]
        mul ecx
        add DWORD PTR [ebx+4],eax
        adc DWORD PTR [ebx+8],edx

        mov eax,DWORD PTR [edi+8]
        mul ecx
        add DWORD PTR [ebx+8],eax
        adc DWORD PTR [ebx+12],edx

        // second "digit" of pnumOp
LDigit2:
        mov ecx,DWORD PTR [esi+4]
        cmp ecx,0
        jz  LDigit3

        mov eax,DWORD PTR [edi]
        mul ecx
        add DWORD PTR [ebx+4],eax
        adc DWORD PTR [ebx+8],edx
        adc DWORD PTR [ebx+12],0
        adc DWORD PTR [ebx+16],0

        mov eax,DWORD PTR [edi+4]
        mul ecx
        add DWORD PTR [ebx+8],eax
        adc DWORD PTR [ebx+12],edx
        adc DWORD PTR [ebx+16],0

        mov eax,DWORD PTR [edi+8]
        mul ecx
        add DWORD PTR [ebx+12],eax
        adc DWORD PTR [ebx+16],edx

        // third "digit" of pnumOp
LDigit3:
        mov ecx,DWORD PTR [esi+8]

        mov eax,DWORD PTR [edi]
        mul ecx
        add DWORD PTR [ebx+8],eax
        adc DWORD PTR [ebx+12],edx
        adc DWORD PTR [ebx+16],0
        adc DWORD PTR [ebx+20],0

        mov eax,DWORD PTR [edi+4]
        mul ecx
        add DWORD PTR [ebx+12],eax
        adc DWORD PTR [ebx+16],edx
        adc DWORD PTR [ebx+20],0

        mov eax,DWORD PTR [edi+8]
        mul ecx
        add DWORD PTR [ebx+16],eax
        adc DWORD PTR [ebx+20],edx
    }
#else //!I386_ASM
    ulong luLo, luHi, luT;
    int wCarry;

    if (0 != (luT = m_lu0))
    {
        luLo = Js::NumberUtilities::MulLu(luT, pnumOp->m_lu0, &luHi);
        rglu[0] = luLo;
        rglu[1] = luHi;

        luLo = Js::NumberUtilities::MulLu(luT, pnumOp->m_lu1, &luHi);
        Assert(luHi < 0xFFFFFFFF);
        wCarry = Js::NumberUtilities::AddLu(&rglu[1], luLo);
        Js::NumberUtilities::AddLu(&rglu[2], luHi + wCarry);

        luLo = Js::NumberUtilities::MulLu(luT, pnumOp->m_lu2, &luHi);
        Assert(luHi < 0xFFFFFFFF);
        wCarry = Js::NumberUtilities::AddLu(&rglu[2], luLo);
        Js::NumberUtilities::AddLu(&rglu[3], luHi + wCarry);
    }

    if (0 != (luT = m_lu1))
    {
        luLo = Js::NumberUtilities::MulLu(luT, pnumOp->m_lu0, &luHi);
        Assert(luHi < 0xFFFFFFFF);
        wCarry = Js::NumberUtilities::AddLu(&rglu[1], luLo);
        wCarry = Js::NumberUtilities::AddLu(&rglu[2], luHi + wCarry);
        if (wCarry && Js::NumberUtilities::AddLu(&rglu[3], 1))
            Js::NumberUtilities::AddLu(&rglu[4], 1);

        luLo = Js::NumberUtilities::MulLu(luT, pnumOp->m_lu1, &luHi);
        Assert(luHi < 0xFFFFFFFF);
        wCarry = Js::NumberUtilities::AddLu(&rglu[2], luLo);
        wCarry = Js::NumberUtilities::AddLu(&rglu[3], luHi + wCarry);
        if (wCarry)
            Js::NumberUtilities::AddLu(&rglu[4], 1);

        luLo = Js::NumberUtilities::MulLu(luT, pnumOp->m_lu2, &luHi);
        Assert(luHi < 0xFFFFFFFF);
        wCarry = Js::NumberUtilities::AddLu(&rglu[3], luLo);
        Js::NumberUtilities::AddLu(&rglu[4], luHi + wCarry);
    }

    luT = m_lu2;
    Assert(0 != luT);
    luLo = Js::NumberUtilities::MulLu(luT, pnumOp->m_lu0, &luHi);
    Assert(luHi < 0xFFFFFFFF);
    wCarry = Js::NumberUtilities::AddLu(&rglu[2], luLo);
    wCarry = Js::NumberUtilities::AddLu(&rglu[3], luHi + wCarry);
    if (wCarry && Js::NumberUtilities::AddLu(&rglu[4], 1))
        Js::NumberUtilities::AddLu(&rglu[5], 1);

    luLo = Js::NumberUtilities::MulLu(luT, pnumOp->m_lu1, &luHi);
    Assert(luHi < 0xFFFFFFFF);
    wCarry = Js::NumberUtilities::AddLu(&rglu[3], luLo);
    wCarry = Js::NumberUtilities::AddLu(&rglu[4], luHi + wCarry);
    if (wCarry)
        Js::NumberUtilities::AddLu(&rglu[5], 1);

    luLo = Js::NumberUtilities::MulLu(luT, pnumOp->m_lu2, &luHi);
    Assert(luHi < 0xFFFFFFFF);
    wCarry = Js::NumberUtilities::AddLu(&rglu[4], luLo);
    Js::NumberUtilities::AddLu(&rglu[5], luHi + wCarry);
#endif //!I386_ASM

    // Compute the new exponent
    m_wExp += pnumOp->m_wExp;

    // Accumulate the error. Adding doesn't necessarily give an accurate
    // bound if both of the errors are bigger than 2.
    Assert(m_luError <= 2 || pnumOp->m_luError <= 2);
    m_luError += pnumOp->m_luError;

    // Handle rounding and normalize.
    if (0 == (rglu[5] & 0x80000000))
    {
        if (0 != (rglu[2] & 0x40000000) &&
            (0 != (rglu[2] & 0xBFFFFFFF) || 0 != rglu[1] || 0 != rglu[0]))
        {
            // Round up by 1
            if (Js::NumberUtilities::AddLu(&rglu[2], 0x40000000) &&
                Js::NumberUtilities::AddLu(&rglu[3], 1) &&
                Js::NumberUtilities::AddLu(&rglu[4], 1))
            {
                Js::NumberUtilities::AddLu(&rglu[5], 1);
                if (rglu[5] & 0x80000000)
                    goto LNormalized;
            }
        }

        // have to shift by one
        Assert(0 != (rglu[5] & 0x40000000));
        m_lu2 = (rglu[5] << 1) | (rglu[4] >> 31);
        m_lu1 = (rglu[4] << 1) | (rglu[3] >> 31);
        m_lu0 = (rglu[3] << 1) | (rglu[2] >> 31);
        m_wExp--;
        m_luError <<= 1;

        // Add one for the error.
        if ((rglu[2] & 0x7FFFFFFF) || rglu[1] || rglu[0])
            m_luError++;
    }
    else
    {
        if (0 != (rglu[2] & 0x80000000) &&
            (0 != (rglu[3] & 1) || 0 != (rglu[2] & 0x7FFFFFFF) ||
            0 != rglu[1] || 0 != rglu[0]))
        {
            // Round up by 1
            if (Js::NumberUtilities::AddLu(&rglu[3], 1) &&
                Js::NumberUtilities::AddLu(&rglu[4], 1) &&
                Js::NumberUtilities::AddLu(&rglu[5], 1))
            {
                Assert(0 == rglu[3]);
                Assert(0 == rglu[4]);
                Assert(0 == rglu[5]);
                rglu[5] = 0x80000000;
                m_wExp++;
            }
        }
LNormalized:
        m_lu2 = rglu[5];
        m_lu1 = rglu[4];
        m_lu0 = rglu[3];

        // Add one for the error.
        if (rglu[2] || rglu[1] || rglu[0])
            m_luError++;
    }
}



double BIGNUM::GetDbl(void)
{
    double dbl;
    ulong luEx;
    int wExp;

    Assert(m_lu2 & 0x80000000);

    wExp = m_wExp + 1022;
    if (wExp >= 2047)
    {
        Js::NumberUtilities::LuHiDbl(dbl) = 0x7FF00000;
        Js::NumberUtilities::LuLoDbl(dbl) = 0;
        return dbl;
    }

    // Round after filling in the bits. In the extra ulong, we set the low bit
    // if there are any extra non-zero bits. This is for breaking the tie when
    // deciding whether to round up or down.
    if (wExp > 0)
    {
        // Normalized.
        Js::NumberUtilities::LuHiDbl(dbl) = ((ulong)wExp << 20) | ((m_lu2 & 0x7FFFFFFF) >> 11);
        Js::NumberUtilities::LuLoDbl(dbl) = m_lu2 << 21 | m_lu1 >> 11;
        luEx = m_lu1 << 21 | (m_lu0 != 0);
    }
    else if (wExp > -20)
    {
        // Denormal with some high bits.
        int wT = 12 - wExp;
        Assert(wT >= 12 && wT < 32);

        Js::NumberUtilities::LuHiDbl(dbl) = m_lu2 >> wT;
        Js::NumberUtilities::LuLoDbl(dbl) = (m_lu2 << (32 - wT)) | (m_lu1 >> wT);
        luEx = (m_lu1 << (32 - wT)) | (m_lu0 != 0);
    }
    else if (wExp == -20)
    {
        // Denormal with no high bits.
        Js::NumberUtilities::LuHiDbl(dbl) = 0;
        Js::NumberUtilities::LuLoDbl(dbl) = m_lu2;
        luEx = m_lu1 | (m_lu0 != 0);
    }
    else if (wExp > -52)
    {
        // Denormal with no high bits.
        int wT = -wExp - 20;
        Assert(wT > 0 && wT < 32);

        Js::NumberUtilities::LuHiDbl(dbl) = 0;
        Js::NumberUtilities::LuLoDbl(dbl) = m_lu2 >> wT;
        luEx = m_lu2 << (32 - wT) | (m_lu1 != 0) |
            (m_lu0 != 0);
    }
    else if (wExp == -52)
    {
        // Zero unless we round up below.
        Js::NumberUtilities::LuHiDbl(dbl) = 0;
        Js::NumberUtilities::LuLoDbl(dbl) = 0;
        luEx = m_lu2 | (m_lu1 != 0) | (m_lu0 != 0);
    }
    else
        return (double)0;

    // Handle rounding
    if ((luEx & 0x80000000) && ((luEx & 0x7FFFFFFF) || (Js::NumberUtilities::LuLoDbl(dbl) & 1)))
    {
        // Round up. Note that this works even when we overflow into the
        // exponent.
        if (Js::NumberUtilities::AddLu(&Js::NumberUtilities::LuLoDbl(dbl), 1))
            Js::NumberUtilities::AddLu(&Js::NumberUtilities::LuHiDbl(dbl), 1);
    }

    return dbl;
}


/***************************************************************************
The double contains a binary value, M * 2^n, which is off by at most 1
in the least significant bit; (prgch, cch, lwExp) represents a decimal
value, D * 10^e. Note that (prgch, cch) may contain a decimal point and
lwExp is as if there is an implied decimal point immediately preceding
the digits.

The general scheme is to find an integer N (the smaller the better) such
that N * M * 2^n and N * D * 10^e are both integers. We then compare
N * M * 2^n to N * D * 10^e (at full precision). If the binary value is
greater, we adjust it to be exactly half way to the next value that can
come from a double. We then compare again to decided whether to bump the
double up to the next value. Similarly if the binary value is smaller,
we adjust it to be exactly half way to the previous representable value
and re-compare.
***************************************************************************/
template <typename EncodedChar>
static double AdjustDbl(double dbl, const EncodedChar *prgch, long cch, long lwExp)
{
    Js::BigInt biDec, biDbl;
    long c2Dec, c2Dbl;
    long c5Dec, c5Dbl;
    int wAddHi, wT;
    long iT;
    long wExp2;
    ulong rglu[2];
    ulong luT;
    BOOL f;
    long clu;

    if (!biDec.FInitFromDigits(prgch, cch, &cch))
        goto LFail;
    lwExp -= cch;

    // lwExp is a base 10 exponent.
    if (lwExp >= 0)
    {
        c5Dec = c2Dec = lwExp;
        c5Dbl = c2Dbl = 0;
    }
    else
    {
        c5Dec = c2Dec = 0;
        c5Dbl = c2Dbl = -lwExp;
    }

    rglu[1] = Js::NumberUtilities::LuHiDbl(dbl);
    wExp2 = (rglu[1] >> 20) & 0x07FF;
    rglu[1] &= 0x000FFFFF;
    rglu[0] = Js::NumberUtilities::LuLoDbl(dbl);
    wAddHi = 1;
    if (0 != wExp2)
    {
        // Normal, so add implicit bit.
        if (0 == rglu[1] && 0 == rglu[0] && 1 != wExp2)
        {
            // Power of 2 (and not adjacent to the first denormal), so the
            // adjacent low value is closer than the high value.
            wAddHi = 2;
            rglu[1] = 0x00200000;
            wExp2--;
        }
        else
            rglu[1] |= 0x00100000;
        wExp2 -= 1076;
    }
    else
        wExp2 = -1075;

    // Shift left by 1 bit : the adjustment values need the next lower bit.
    rglu[1] = (rglu[1] << 1) | (rglu[0] >> 31);
    rglu[0] <<= 1;

    // We must determine how many words of significant digits this requires.
    if (0 == rglu[0] && 0 == rglu[1])
        clu = 0;
    else if (0 == rglu[1])
        clu = 1;
    else
        clu = 2;

    f = biDbl.FInitFromRglu(rglu, clu);
    Assert(f);

    if (wExp2 >= 0)
        c2Dbl += wExp2;
    else
        c2Dec += -wExp2;

    // Eliminate common powers of 2.
    if (c2Dbl > c2Dec)
    {
        c2Dbl -= c2Dec;
        c2Dec = 0;

        // See if biDec has some powers of 2 that we can get rid of.
        for (iT = 0; c2Dbl >= 32 && 0 == biDec.Lu(iT); iT++)
            c2Dbl -= 32;
        if (iT > 0)
            biDec.ShiftLusRight(iT);
        Assert(c2Dbl < 32 || biDec.Lu(0) != 0);
        luT = biDec.Lu(0);
        for (iT = 0; iT < c2Dbl && 0 == (luT & (1L << iT)); iT++)
            ;
        if (iT > 0)
        {
            c2Dbl -= iT;
            biDec.ShiftRight(iT);
        }
    }
    else
    {
        c2Dec -= c2Dbl;
        c2Dbl = 0;
    }

    // There are no common powers of 2 or common powers of 5.
    Assert(0 == c2Dbl || 0 == c2Dec);
    Assert(0 == c5Dbl || 0 == c5Dec);

    // Fold in the powers of 5.
    if (c5Dbl > 0)
    {
        if (!biDbl.FMulPow5(c5Dbl))
            goto LFail;
    }
    else if (c5Dec > 0 && !biDec.FMulPow5(c5Dec))
        goto LFail;

    // Fold in the powers of 2.
    if (c2Dbl > 0)
    {
        if (!biDbl.FShiftLeft(c2Dbl))
            goto LFail;
    }
    else if (c2Dec > 0 && !biDec.FShiftLeft(c2Dec))
        goto LFail;

    // Now determine whether biDbl is above or below biDec.
    wT = biDbl.Compare(&biDec);

    if (0 == wT)
        return dbl;
    if (wT > 0)
    {
        // biDbl is greater. Recompute with the dbl minus half the distance
        // to the next smaller double.
        if (!Js::NumberUtilities::AddLu(&rglu[0], 0xFFFFFFFF))
            Js::NumberUtilities::AddLu(&rglu[1], 0xFFFFFFFF);
        AssertVerify(biDbl.FInitFromRglu(rglu, 1 + (0 != rglu[1])));
        if (c5Dbl > 0 && !biDbl.FMulPow5(c5Dbl))
            goto LFail;
        if (c2Dbl > 0 && !biDbl.FShiftLeft(c2Dbl))
            goto LFail;

        wT = biDbl.Compare(&biDec);
        if (wT > 0 || 0 == wT && 0 != (Js::NumberUtilities::LuLoDbl(dbl) & 1))
        {
            // Return the next lower value.
            if (!Js::NumberUtilities::AddLu(&Js::NumberUtilities::LuLoDbl(dbl), 0xFFFFFFFF))
                Js::NumberUtilities::AddLu(&Js::NumberUtilities::LuHiDbl(dbl), 0xFFFFFFFF);
        }
    }
    else
    {
        // biDbl is smaller. Recompute with the dbl plus half the distance
        // to the next larger double.
        if (Js::NumberUtilities::AddLu(&rglu[0], wAddHi))
            Js::NumberUtilities::AddLu(&rglu[1], 1);
        AssertVerify(biDbl.FInitFromRglu(rglu, 1 + (0 != rglu[1])));
        if (c5Dbl > 0 && !biDbl.FMulPow5(c5Dbl))
            goto LFail;
        if (c2Dbl > 0 && !biDbl.FShiftLeft(c2Dbl))
            goto LFail;

        wT = biDbl.Compare(&biDec);
        if (wT < 0 || 0 == wT && 0 != (Js::NumberUtilities::LuLoDbl(dbl) & 1))
        {
            // Return the next higher value.
            if (Js::NumberUtilities::AddLu(&Js::NumberUtilities::LuLoDbl(dbl), 1))
                Js::NumberUtilities::AddLu(&Js::NumberUtilities::LuHiDbl(dbl), 1);
        }
    }

LFail:
    return dbl;
}


/***************************************************************************
String to Double.
***************************************************************************/
template <typename EncodedChar>
double Js::NumberUtilities::StrToDbl( const EncodedChar *psz, const EncodedChar **ppchLim, bool& likelyInt )
{
    ulong lu;
    BIGNUM num;
    BIGNUM numHi;
    BIGNUM numLo;
    double dbl;
    double dblLo;
#if DBG
    bool canUseLowPrec = false;
    double dblLowPrec;
    Js::NumberUtilities::LuHiDbl(dblLowPrec) = 0x7FFFFFFF;
    Js::NumberUtilities::LuLoDbl(dblLowPrec) = 0xFFFFFFFF;
    Assert(Js::NumberUtilities::IsNan(dblLowPrec));
#endif //DBG

    // For the mantissa digits. After leaving the state machine, pchMinDig
    // points to the first digit and pchLimDig points just past the last
    // digit. cchDig is the number of digits. pchLimDig - pchMinDig may be
    // cchDig + 1 (if there is a decimal point).
    long cchDig = 0;
    const EncodedChar *pchMinDig = NULL;
    const EncodedChar *pchLimDig = NULL;

    int signExp = 1;    // sign of the exponent
    int signMan = 0;    // sign of the mantissa
    long lwAdj = 0;        // exponent adjustment
    long lwExp = 0;        // the exponent

    const EncodedChar *pchSave;
    const EncodedChar *pch = psz;

    // Enter the state machine

    // Initialization
LRestart:
    switch (*pch)
    {
    default:
        if (FNzDigit(*pch))
            goto LGetLeftDig;
        break;
    case 'I':
        // Check for the special case of [+|-]Infinity.
        if (pch[1] == 'n' && pch[2] == 'f' && pch[3] == 'i' && pch[4] == 'n' && pch[5] == 'i' && pch[6] == 't' && pch[7] == 'y')
        {
            *ppchLim = pch + 8;
            Js::NumberUtilities::LuHiDbl(dbl) = 0x7FF00000;
            Js::NumberUtilities::LuLoDbl(dbl) = 0;
            goto LDone;
        }
        break;
    case '-':
        if (signMan)
            break;
        pch++;
        signMan = -1;
        goto LRestart;
    case '+':
        if (signMan)
            break;
        pch++;
        signMan = +1;
        goto LRestart;
    case '0':
        while ('0' == *++pch)
            ;
        goto LGetLeft;
    case '.':
        if (Js::NumberUtilities::IsDigit(pch[1]))
            goto LGetRight;
        break;
    }

    // Nothing digested - set the result to NaN and exit.
    // We cannot use any other NaN value because of our tagged float encoding.
    *ppchLim = psz;
    Js::NumberUtilities::LuHiDbl(dbl) = 0xFFF80000;
    Js::NumberUtilities::LuLoDbl(dbl) = 0x00000000;
    goto LDone;

LGetLeft:
    // Get digits to the left of the decimal point
    if (Js::NumberUtilities::IsDigit(*pch))
    {
LGetLeftDig:
        pchMinDig = pch;
        for (cchDig = 1; Js::NumberUtilities::IsDigit(*++pch); cchDig++)
            ;
    }
    switch (*pch)
    {
    case '.':
        goto LGetRight;
    case 'E':
    case 'e':
        goto LGetExp;
    }
    goto LEnd;

LGetRight:
    Assert(*pch == '.');
    likelyInt = false;
    pch++;
    if (NULL == pchMinDig)
    {
        for ( ; *pch == '0'; pch++)
            lwAdj--;
        pchMinDig = pch;
    }
    for( ; Js::NumberUtilities::IsDigit(*pch); pch++)
    {
        cchDig++;
        lwAdj--;
    }
    switch (*pch)
    {
    case 'E':
    case 'e':
        goto LGetExp;
    }
    goto LEnd;

LGetExp:
    pchLimDig = pch;
    pchSave = pch++; // points to 'E'
    if (Js::NumberUtilities::IsDigit(*pch))
        goto LGetExpDigits;
    switch (*pch)
    {
    case '-':
        signExp = -1;
        // fall-through
    case '+':
        pch++;
        if (Js::NumberUtilities::IsDigit(*pch))
            goto LGetExpDigits;
        break;
    }
    // back up to the 'E'
    pch = pchSave;
    goto LEnd;

LGetExpDigits:
    for( ; Js::NumberUtilities::IsDigit(*pch); pch++)
    {
        lwExp = lwExp * 10 + (*pch - '0');
        if (lwExp > 100000000)
            lwExp = 100000000;
    }

LEnd:
    *ppchLim = pch;
    if (cchDig == 0)
    {
        dbl = 0;
        goto LDone;
    }

    if (NULL == pchLimDig)
        pchLimDig = pch;
    Assert(pchMinDig != NULL);
    Assert(pchLimDig - pchMinDig == cchDig ||
        pchLimDig - pchMinDig == cchDig + 1);

    // Limit to kcchMaxSig digits.
    if (cchDig > kcchMaxSig)
    {
        // cchDig - number of digits from the first nonzero digit in the input
        // pchLimDig - at this point, this points to the character after the last digit in the input, and after the decimal
        //             point if the number ends with a decimal point
        // lwAdj - considering that the decimal point is initially after the last digit in the input, this contains the number
        //         of digits the decimal point should be moved to the right to put the decimal point in the correct place,
        //         excluding the exponent component. Since this excludes the exponent component, at this point lwAdj is either
        //         zero or negative.

        // Here, we are going to consider that there are only kcchMaxSig digits (effectively the same as replacing excessive
        // digits with a '0' to make them insignificant, as the spec requests). So, the following need to be done:
        const long numExcessiveDigits = cchDig - kcchMaxSig;

        // Move pchLimDig to the left over numExcessiveDigits digits. Note that it needs to move over digits; the decimal point
        // does not count as a digit. We determine if pchLimDig would jump over the decimal point by using the lwAdj value and
        // if so, jump over one more character. Note also that if lwAdj is zero, there may or may not be a decimal point, and
        // pchLimDig would only need to jump over a decimal point if it exists.
        if (-lwAdj <= numExcessiveDigits &&
            (lwAdj != 0 || pchLimDig[-1] == L'.'))
        {
            // Need to jump over the decimal point
            --pchLimDig;
        }
        pchLimDig -= numExcessiveDigits;

        // We previously considered that the decimal point is initially after the last digit in the input, and is moved to the
        // right by lwAdj digits. Now, we're going to consider that the decimal point is initially after the kcchMaxSig'th
        // digit. So, the decimal point needs to be moved to the right by numExcessiveDigits digits, so add that to lwAdj.
        lwAdj += numExcessiveDigits;

        cchDig = kcchMaxSig;
    }

    // Remove trailing zero's from mantissa
    Assert(FNzDigit(*pchMinDig));
    for (;;)
    {
        if (*--pchLimDig == '0')
        {
            cchDig--;
            lwAdj++;
        }
        else if (*pchLimDig != '.')
        {
            Assert(FNzDigit(*pchLimDig));
            pchLimDig++;
            break;
        }
    }
    Assert(pchLimDig - pchMinDig == cchDig ||
        pchLimDig - pchMinDig == cchDig + 1);

    if (signExp < 0)
        lwExp = -lwExp;
    lwExp += lwAdj;

    // See if we can just use IEEE double arithmetic.
    if (cchDig <= 15 && lwExp >= -22 && lwExp + cchDig <= 37)
    {
        // These calculations are all exact since cchDig <= 15.
        if (cchDig <= 9)
        {
            // Can use the ALU.
            for (lu = 0, pch = pchMinDig; pch < pchLimDig; pch++)
            {
                if (*pch != '.')
                {
                    Assert(Js::NumberUtilities::IsDigit(*pch));
                    lu = lu * 10 + (*pch - '0');
                }
            }
            dbl = lu;
        }
        else
        {
            for (dbl = 0, pch = pchMinDig; pch < pchLimDig; pch++)
            {
                if (*pch != '.')
                {
                    Assert(Js::NumberUtilities::IsDigit(*pch));
                    dbl = dbl * 10 + (*pch - '0');
                }
            }
        }

        // This is the only (potential) rounding operation and we assume
        // the compiler does the correct IEEE rounding.
        if (lwExp > 0)
        {
            if (lwExp > 22)
            {
                // This one is exact. We're using the fact that cchDig < 15
                // to handle exponents bigger than 22.
                dbl *= g_rgdblTens[15 - cchDig];
                Assert(lwExp - (15 - cchDig) <= 22);
                dbl *= g_rgdblTens[lwExp - (15 - cchDig)];
            }
            else
                dbl *= g_rgdblTens[lwExp];
        }
        else if (lwExp < 0)
            dbl /= g_rgdblTens[-lwExp];

#if DBG
        // In the debug version, execute the high precision code also and
        // verify that the results are the same.
        canUseLowPrec = true;
        dblLowPrec = dbl;
#else //!DBG
        goto LDone;
#endif //!DBG
    }

    lwExp += cchDig;
    if (lwExp >= klwMaxExp10)
    {
        // Overflow to infinity.
        Js::NumberUtilities::LuHiDbl(dbl) = 0x7FF00000;
        Js::NumberUtilities::LuLoDbl(dbl) = 0;
        goto LDone;
    }

    if (lwExp <= klwMinExp10)
    {
        // Underflow to 0.
        dbl = 0;
        goto LDone;
    }

    // Convert to a big number.
    Assert(pchLimDig - pchMinDig >= 0 && pchLimDig - pchMinDig <= LONG_MAX);
    num.SetFromRgchExp(pchMinDig, (long)(pchLimDig - pchMinDig), lwExp);

    // If there is no error in the big number, just convert it to a double.
    if (0 == num.m_luError)
    {
        dbl = num.GetDbl();
#if DBG
        Assert(pchLimDig - pchMinDig >= 0 && pchLimDig - pchMinDig <= LONG_MAX);
        dblLo = AdjustDbl(dbl, pchMinDig, (long)(pchLimDig - pchMinDig), lwExp);
        Assert(dbl == dblLo);
#endif //DBG
        goto LDone;
    }

    // The big number has error in it, so see if the error matters.
    // Get the upper bound and lower bound. If they convert to the same
    // double we're done.
    numHi = num;
    numHi.MakeUpperBound();
    numLo = num;
    numLo.MakeLowerBound();

    dbl = numHi.GetDbl();
    dblLo = numLo.GetDbl();
    if (dbl == dblLo)
    {
#if DBG
        Assert(dbl == num.GetDbl());
        Assert(pchLimDig - pchMinDig >= 0 && pchLimDig - pchMinDig <= LONG_MAX);
        dblLo = AdjustDbl(dbl, pchMinDig, (long)(pchLimDig - pchMinDig), lwExp);
        Assert(dbl == dblLo || Js::NumberUtilities::IsNan(dblLo));
#endif //DBG
        goto LDone;
    }

    // Need to use big integer arithmetic. There's too much error in
    // our result and it's close to a boundary value. This is rare,
    // but does happen, e.g.:
    // x = 1.2345678901234568347913049445e+200;
    //
    Assert(pchLimDig - pchMinDig >= 0 && pchLimDig - pchMinDig <= LONG_MAX);
    dbl = AdjustDbl(num.GetDbl(), pchMinDig, (long)(pchLimDig - pchMinDig), lwExp);

LDone:
    // This assert was removed because it would fire on VERY rare occasions. Not
    // repro on all machines and very hard to repro even on machines that could repro it.
    // The numbers (dblLowPrec and dbl) were different in their two least sig bits only
    // which is _probably_ within expected error. I did not take the time to fully
    // investigate whether this really does meet the ECMA spec...
    //
    //    Assert(Js::NumberUtilities::IsNan(dblLowPrec) || dblLowPrec == dbl);

#if DBG
    if(canUseLowPrec)
    {
        // Use the same final behavior in debug builds as for non-debug builds by using the low-precision value
        dbl = dblLowPrec;
    }
#endif

    if (signMan < 0)
        dbl = -dbl;
    return dbl;
}

template double Js::NumberUtilities::StrToDbl<wchar_t>( const wchar_t * psz, const wchar_t **ppchLim, bool& likelyInt );
template double Js::NumberUtilities::StrToDbl<utf8char_t>(const utf8char_t * psz, const utf8char_t **ppchLim, bool& likelyInt);

/***************************************************************************
Uses big integer arithmetic to get the sequence of digits.
***************************************************************************/
_Success_(return)
static BOOL FDblToRgbPrecise(double dbl, __out_ecount(kcbMaxRgb) byte *prgb, int *pwExp10, byte **ppbLim)
{
    byte bT;
    BOOL fPow2;
    int ib;
    int clu;
    int wExp10, wExp2, w1, w2;
    int c2Num, c2Den, c5Num, c5Den;
    double dblT;
    Js::BigInt biNum, biDen, biHi, biLo;
    Js::BigInt *pbiLo;
    Js::BigInt biT;
    ulong rglu[2];

    // Caller should take care of 0, negative and non-finite values.
    Assert(Js::NumberUtilities::IsFinite(dbl));
    Assert(0 < dbl);

    // Init the Denominator, Hi error and Lo error bigints.
    rglu[0] = 1;
    AssertVerify(biDen.FInitFromRglu(rglu, 1));
    AssertVerify(biHi.FInitFromRglu(rglu, 1));

    wExp2 = (int)(((Js::NumberUtilities::LuHiDbl(dbl) & 0x7FF00000) >> 20) - 1075);
    rglu[1] = Js::NumberUtilities::LuHiDbl(dbl) & 0x000FFFFF;
    rglu[0] = Js::NumberUtilities::LuLoDbl(dbl);
    clu = 2;
    fPow2 = FALSE;
    if (wExp2 == -1075)
    {
        // dbl is denormalized.
        Assert(0 == (Js::NumberUtilities::LuHiDbl(dbl) & 0x7FF00000));
        if (0 == rglu[1])
            clu = 1;

        // Get dblT such that dbl / dblT is a power of 2 and 1 <= dblT < 2.
        // First multiply by a power of 2 to get a normalized value.
        Js::NumberUtilities::LuHiDbl(dblT) = 0x4FF00000;
        Js::NumberUtilities::LuLoDbl(dblT) = 0;
        dblT *= dbl;
        Assert(0 != (Js::NumberUtilities::LuHiDbl(dblT) & 0x7FF00000));

        // This is the power of 2.
        w1 = (int)((Js::NumberUtilities::LuHiDbl(dblT) & 0x7FF00000) >> 20) - (256 + 1023);

        Js::NumberUtilities::LuHiDbl(dblT) &= 0x000FFFFF;
        Js::NumberUtilities::LuHiDbl(dblT) |= 0x3FF00000;

        // Adjust wExp2 because we don't have the implicit bit.
        wExp2++;
    }
    else
    {
        // Get dblT such that dbl / dblT is a power of 2 and 1 <= dblT < 2.
        // First multiply by a power of 2 to get a normalized value.
        dblT = dbl;
        Js::NumberUtilities::LuHiDbl(dblT) &= 0x000FFFFF;
        Js::NumberUtilities::LuHiDbl(dblT) |= 0x3FF00000;

        // This is the power of 2.
        w1 = wExp2 + 52;

        if (0 == rglu[0] && 0 == rglu[1] && wExp2 > -1074)
        {
            // Power of 2 bigger than smallest normal. The next smaller
            // representable value is closer than the next larger value.
            rglu[1] = 0x00200000;
            wExp2--;
            fPow2 = TRUE;
        }
        else
        {
            // Normalized and not a power of 2 or the smallest normal. The
            // representable values on either side are the same distance away.
            rglu[1] |= 0x00100000;
        }
    }

    // Compute an approximation to the base 10 log. This is borrowed from
    // David Gay's paper.
    Assert(1 <= dblT && dblT < 2);
    dblT = (dblT - 1.5) * 0.289529654602168 + 0.1760912590558 +
        w1 * 0.301029995663981;
    wExp10 = (int)dblT;
    if (dblT < 0 && dblT != wExp10)
        wExp10--;

    if (wExp2 >= 0)
    {
        c2Num = wExp2;
        c2Den = 0;
    }
    else
    {
        c2Num = 0;
        c2Den = -wExp2;
    }

    if (wExp10 >= 0)
    {
        c5Num = 0;
        c5Den = wExp10;
        c2Den += wExp10;
    }
    else
    {
        c2Num -= wExp10;
        c5Num = -wExp10;
        c5Den = 0;
    }

    if (c2Num > 0 && c2Den > 0)
    {
        w1 = c2Num < c2Den ? c2Num : c2Den;
        c2Num -= w1;
        c2Den -= w1;
    }
    // We need a bit for the Hi and Lo values.
    c2Num++;
    c2Den++;

    // Initialize biNum and multiply by powers of 5.
    if (c5Num > 0)
    {
        Assert(0 == c5Den);
        if (!biHi.FMulPow5(c5Num))
            goto LFail;
        if (!biNum.FInitFromBigint(&biHi))
            goto LFail;
        if (clu == 1)
        {
            if (!biNum.FMulAdd(rglu[0], 0))
                goto LFail;
        }
        else
        {
            if (!biNum.FMulAdd(rglu[1], 0))
                goto LFail;
            if (!biNum.FShiftLeft(32))
                goto LFail;
            if (rglu[0] != 0)
            {
                if (!biT.FInitFromBigint(&biHi))
                    goto LFail;
                if (!biT.FMulAdd(rglu[0], 0))
                    goto LFail;
                if (!biNum.FAdd(&biT))
                    goto LFail;
            }
        }
    }
    else
    {
        Assert(clu <= 2);
        AssertVerify(biNum.FInitFromRglu(rglu, clu));
        if (c5Den > 0 && !biDen.FMulPow5(c5Den))
            goto LFail;
    }

    // BIGINT::DivRem only works if the 4 high bits of the divisor are 0.
    // It works most efficiently if there are exactly 4 zero high bits.
    // Adjust c2Den and c2Num to guarantee this.
    w1 = Js::NumberUtilities::CbitZeroLeft(biDen.Lu(biDen.Clu() - 1));
    w1 = (w1 + 28 - c2Den) & 0x1F;
    c2Num += w1;
    c2Den += w1;

    // Multiply by powers of 2.
    Assert(c2Num > 0 && c2Den > 0);
    if (!biNum.FShiftLeft(c2Num))
        goto LFail;
    if (c2Num > 1 && !biHi.FShiftLeft(c2Num - 1))
        goto LFail;
    if (!biDen.FShiftLeft(c2Den))
        goto LFail;
    Assert(0 == (biDen.Lu(biDen.Clu() - 1) & 0xF0000000));
    Assert(0 != (biDen.Lu(biDen.Clu() - 1) & 0x08000000));

    // Get pbiLo and handle the power of 2 case where biHi needs to be doubled.
    if (fPow2)
    {
        pbiLo = &biLo;
        if (!pbiLo->FInitFromBigint(&biHi))
            goto LFail;
        if (!biHi.FShiftLeft(1))
            goto LFail;
    }
    else
        pbiLo = &biHi;

    for (ib = 0; ib < kcbMaxRgb; )
    {
        bT = (byte)biNum.DivRem(&biDen);
        if (ib == 0 && bT == 0)
        {
            // Our estimate of wExp10 was too big. Oh well.
            wExp10--;
            goto LSkip;
        }

        // w1 = sign(biNum - *pbiLo).
        w1 = biNum.Compare(pbiLo);

        // w2 = sign(biNum + biHi - biDen).
        if (biDen.Compare(&biHi) < 0)
            w2 = 1;
        else
        {
            // REVIEW : is there a faster way to do this?
            biT.FInitFromBigint(&biDen);
            biT.Subtract(&biHi);
            w2 = biNum.Compare(&biT);
        }

        // if (biNum + biHi == biDen && even)
        if (0 == w2 && 0 == (Js::NumberUtilities::LuLoDbl(dbl) & 1))
        {
            // Rounding up this digit produces exactly (biNum + biHi) which
            // StrToDbl will round down to dbl.
            if (bT == 9)
                goto LRoundUp9;
            if (w1 > 0)
                bT++;
            Assert(ib < kcbMaxRgb);
            prgb[ib++] = bT;
            break;
        }

        // if (biNum < *pbiLo || biNum == *pbiLo && even)
        if (w1 < 0 || 0 == w1 && 0 == (Js::NumberUtilities::LuLoDbl(dbl) & 1))
        {
            // if (biNum + biHi > biDen)
            if (w2 > 0)
            {
                // Decide whether to round up.
                if (!biNum.FShiftLeft(1))
                    goto LFail;
                w2 = biNum.Compare(&biDen);
                if ((w2 > 0 || w2 == 0 && (bT & 1)) && bT++ == 9)
                    goto LRoundUp9;
            }
            Assert(ib < kcbMaxRgb);
            prgb[ib++] = bT;
            break;
        }

        // if (biNum + biHi > biDen)
        if (w2 > 0)
        {
            // Round up and be done with it.
            if (bT != 9)
            {
                Assert(ib < kcbMaxRgb);
                prgb[ib++] = bT + 1;
                break;
            }
LRoundUp9:
            while (ib > 0)
            {
                if (prgb[--ib] != 9)
                {
                    prgb[ib++]++;
                    goto LReturn;
                }
            }
            wExp10++;
            Assert(ib < kcbMaxRgb);
            prgb[ib++] = 1;
            break;
        }

        // Save the digit.
        Assert(ib < kcbMaxRgb);
        prgb[ib++] = bT;

LSkip:
        if (!biNum.FMulAdd(10, 0))
            goto LFail;
        if (!biHi.FMulAdd(10, 0))
            goto LFail;
        if (pbiLo != &biHi && !pbiLo->FMulAdd(10, 0))
            goto LFail;
    }

LReturn:
    *pwExp10 = wExp10 + 1;
    AnalysisAssert(ib <= kcbMaxRgb);
    *ppbLim = &prgb[ib];
    return TRUE;

LFail:
    return FALSE;
}


/***************************************************************************
Get mantissa bytes (BCD).
***************************************************************************/
_Success_(return)
static BOOL FDblToRgbFast(double dbl, _Out_writes_to_(kcbMaxRgb, (*ppbLim - prgb)) byte *prgb, int *pwExp10, byte **ppbLim)
{
    int ib;
    int iT;
    ulong luT;
    ulong luScale;
    const BIGNUM *pnum;
    byte bHH = 0, bHL = 0, bLH = 0, bLL = 0;
    ulong luHH, luHL, luLH, luLL;
    BIGNUM numHH, numHL, numLH, numLL, numBase;
    int wExp2;
    int wExp10 = 0;

    // Caller should take care of 0, negative and non-finite values.
    Assert(Js::NumberUtilities::IsFinite(dbl));
    Assert(0 < dbl);

    // Get numHH and numLL such that numLL < dbl < numHH and the
    // difference between adjacent values is half the distance to the next
    // representable value (in a double).
    wExp2 = (int)((Js::NumberUtilities::LuHiDbl(dbl) >> 20) & 0x07FF);
    if (wExp2 > 0)
    {
        // See if dbl is a small integer.
        if (wExp2 >= 1023 && wExp2 <= 1075 && dbl == floor(dbl))
            goto LSmallInt;

        // Normalized
        numBase.m_lu2 = 0x80000000 | ((Js::NumberUtilities::LuHiDbl(dbl) & 0x000FFFFFF) << 11) |
            (Js::NumberUtilities::LuLoDbl(dbl) >> 21);
        numBase.m_lu1 = Js::NumberUtilities::LuLoDbl(dbl) << 11;
        numBase.m_lu0 = 0;
        numBase.m_wExp = wExp2 - 1022;
        numBase.m_luError = 0;

        // Get the upper bound
        numHH = numBase;
        numHH.m_lu1 |= (1 << 10);

        // Get the lower bound. A power of 2 must be special cased.
        numLL = numBase;
        if (0x80000000 == numLL.m_lu2 && 0 == numLL.m_lu1)
        {
            // Subtract (0x00000000, 0x00000200, 0x00000000). Same as adding
            // (0xFFFFFFFF, 0xFFFFFE00, 0x00000000)
            luT = 0xFFFFFE00;
        }
        else
        {
            // Subtract (0x00000000, 0x00000400, 0x00000000). Same as adding
            // (0xFFFFFFFF, 0xFFFFFC00, 0x00000000)
            luT = 0xFFFFFC00;
        }
        if (!Js::NumberUtilities::AddLu(&numLL.m_lu1, luT))
        {
            Js::NumberUtilities::AddLu(&numLL.m_lu2, 0xFFFFFFFF);
            if (0 == (0x80000000 & numLL.m_lu2))
                numLL.Normalize();
        }
    }
    else
    {
        // Denormal
        numBase.m_lu2 = Js::NumberUtilities::LuHiDbl(dbl) & 0x000FFFFF;
        numBase.m_lu1 = Js::NumberUtilities::LuLoDbl(dbl);
        numBase.m_lu0 = 0;
        numBase.m_wExp = -1010;
        numBase.m_luError = 0;

        // Get the upper bound
        numHH = numBase;
        numHH.m_lu0 = 0x80000000;

        // Get the lower bound
        numLL = numHH;
        if (!Js::NumberUtilities::AddLu(&numLL.m_lu1, 0xFFFFFFFF))
            Js::NumberUtilities::AddLu(&numLL.m_lu2, 0xFFFFFFFF);

        numBase.Normalize();
        numHH.Normalize();
        numLL.Normalize();
    }

    // Multiply by powers of ten until 0 < numHH.m_wExp < 32.
    if (numHH.m_wExp >= 32)
    {
        iT = (numHH.m_wExp - 25) * 15 / -g_rgnumNeg[45].m_wExp;
        Assert(iT >= 0 && iT < 16);
        __analysis_assume(iT >= 0 && iT < 16);
        if (iT > 0)
        {
            pnum = &g_rgnumNeg[30 + iT];
            Assert(numHH.m_wExp + pnum->m_wExp > 1);
            numHH.Mul(pnum);
            numLL.Mul(pnum);
            wExp10 += iT * 32;
        }

        if (numHH.m_wExp >= 32)
        {
            iT = (numHH.m_wExp - 25) * 32 / -g_rgnumNeg[31].m_wExp;
            Assert(iT > 0 && iT <= 32);
            pnum = &g_rgnumNeg[iT - 1];
            Assert(numHH.m_wExp + pnum->m_wExp > 1);
            numHH.Mul(pnum);
            numLL.Mul(pnum);
            wExp10 += iT;
        }
    }
    else if (numHH.m_wExp < 1)
    {
        iT = (25 - numHH.m_wExp) * 15 / g_rgnumPos[45].m_wExp;
        Assert(iT >= 0 && iT < 16);
        __analysis_assume(iT >= 0 && iT < 16);
        if (iT > 0)
        {
            pnum = &g_rgnumPos[30 + iT];
            Assert(numHH.m_wExp + pnum->m_wExp <= 32);
            numHH.Mul(pnum);
            numLL.Mul(pnum);
            wExp10 -= iT * 32;
        }

        if (numHH.m_wExp < 1)
        {
            iT = (25 - numHH.m_wExp) * 32 / g_rgnumPos[31].m_wExp;
            Assert(iT > 0 && iT <= 32);
            pnum = &g_rgnumPos[iT - 1];
            Assert(numHH.m_wExp + pnum->m_wExp <= 32);
            numHH.Mul(pnum);
            numLL.Mul(pnum);
            wExp10 -= iT;
        }
    }

    Assert(numHH.m_wExp > 0 && numHH.m_wExp < 32);

    // Get the upper and lower bounds for these.
    numHL = numHH;
    numHH.MakeUpperBound();
    numHL.MakeLowerBound();
    luHH = numHH.LuMod1();
    luHL = numHL.LuMod1();
    numLH = numLL;
    numLH.MakeUpperBound();
    numLL.MakeLowerBound();
    luLH = numLH.LuMod1();
    luLL = numLL.LuMod1();
    Assert(luLL <= luLH && luLH <= luHL && luHL <= luHH);

    // Find the starting scale
    luScale = 1;
    if (luHH >= 100000000)
    {
        luScale = 100000000;
        wExp10 += 8;
    }
    else
    {
        if (luHH >= 10000)
        {
            luScale = 10000;
            wExp10 += 4;
        }
        if (luHH >= 100 * luScale)
        {
            luScale *= 100;
            wExp10 += 2;
        }
    }
    if (luHH >= 10 * luScale)
    {
        luScale *= 10;
        wExp10++;
    }
    wExp10++;
    Assert(luHH >= luScale && luHH / luScale < 10);

    for (ib = 0; ib < kcbMaxRgb; )
    {
        Assert(luLL <= luHH);
        bHH = (byte)(luHH / luScale);
        luHH %= luScale;
        bLL = (byte)(luLL / luScale);
        luLL %= luScale;

        if (bHH != bLL)
            break;

        Assert(luHH != 0 || !numHH.FZero());
        Assert(ib < kcbMaxRgb);
        prgb[ib++] = bHH;

        if (1 == luScale)
        {
            // Multiply by 10^8.
            luScale = 10000000;

            numHH.Mul(&g_rgnumPos[7]);
            numHH.MakeUpperBound();
            luHH = numHH.LuMod1();
            if (luHH >= 100000000)
                goto LFail;
            numHL.Mul(&g_rgnumPos[7]);
            numHL.MakeLowerBound();
            luHL = numHL.LuMod1();

            numLH.Mul(&g_rgnumPos[7]);
            numLH.MakeUpperBound();
            luLH = numLH.LuMod1();
            numLL.Mul(&g_rgnumPos[7]);
            numLL.MakeLowerBound();
            luLL = numLL.LuMod1();
        }
        else
            luScale /= 10;
    }

    // LL and HH diverged. Get the digit values for LH and HL.
    Assert(0 <= bLL && bLL < bHH && bHH <= 9);
    bLH = (byte)((luLH / luScale) % 10);
    luLH %= luScale;
    bHL = (byte)((luHL / luScale) % 10);
    luHL %= luScale;

    if (bLH >= bHL)
        goto LFail;

    // LH and HL also diverged.

    // We can get by with one fewer digit if: LL == LH and bLH is zero
    // and the current value of LH is zero and the least significant bit of
    // the double is zero. In this case, we have exactly the digit sequence
    // for the original numLL and IEEE and will rounds numLL up to the double.
    if (0 == bLH && 0 == luLH && numLH.FZero() && 0 == (Js::NumberUtilities::LuLoDbl(dbl) & 1))
        ;
    else if (bHL - bLH > 1)
    {
        Assert(ib < kcbMaxRgb);
        if(!(ib < kcbMaxRgb))
            goto LFail;

        // HL and LH differ by at least two in this digit, so split
        // the difference.
        prgb[ib++] = (bHL + bLH + 1) / 2;
    }
    else if (0 != luHL || !numHL.FZero() || 0 == (Js::NumberUtilities::LuLoDbl(dbl) & 1))
    {
        Assert(ib < kcbMaxRgb);
        if(!(ib < kcbMaxRgb))
            goto LFail;

        // We can just use bHL because this guarantees that we're bigger than
        // LH and less than HL, so must convert to the double.
        prgb[ib++] = bHL;
    }
    else
        goto LFail;

    Assert(ib <= kcbMaxRgb);

    *pwExp10 = wExp10;
    *ppbLim = &prgb[ib];
    return TRUE;

LSmallInt:
    // dbl should be an integer from 1 to (2^53 - 1).
    Assert(dbl == floor(dbl) && 1 <= dbl && dbl <= 9007199254740991.0L);

    iT = 0;
    if (dbl >= g_rgdblTens[iT + 8])
        iT += 8;
    if (dbl >= g_rgdblTens[iT + 4])
        iT += 4;
    if (dbl >= g_rgdblTens[iT + 2])
        iT += 2;
    if (dbl >= g_rgdblTens[iT + 1])
        iT += 1;
    Assert(iT >= 0 && iT <= 15);
    Assert(dbl >= g_rgdblTens[iT] && dbl < g_rgdblTens[iT + 1]);
    *pwExp10 = iT + 1;

    for (ib = 0; 0 != dbl && ib < kcbMaxRgb && 0 <= iT; iT--)
    {
        Assert(iT >= 0);
        bHH = (byte)(dbl / g_rgdblTens[iT]);
        dbl -= bHH * g_rgdblTens[iT];
        Assert(dbl == floor(dbl) && 0 <= dbl && dbl < g_rgdblTens[iT]);
        prgb[ib++] = bHH;
    }
    *ppbLim = &prgb[ib];
    return TRUE;

LFail:
    return FALSE;
}


static BOOL FormatDigits(_In_reads_(pbLim - pbSrc) byte *pbSrc, byte *pbLim, int wExp10, _Out_writes_(cchDst) OLECHAR *pchDst, int cchDst)
{
    AssertArrMem(pbSrc, pbLim - pbSrc);
    AnalysisAssert(pbLim > pbSrc);

    if (pbLim <= pbSrc)
    {
        Assert(0);
        return FALSE;
    }

    // check the expected size of the resulting string...
    size_t nCount;
    if ((wExp10 <= -6) ||(wExp10 > 21))
    {
        nCount = (pbLim - pbSrc) + 6;
        if (wExp10 >= 100)
            nCount += 2;
        else if (wExp10 >= 10)
            nCount += 1;
    }
    else if (wExp10 <= 0)
        nCount = (pbLim - pbSrc) + 3 + abs(wExp10);
    else
        nCount = (pbLim - pbSrc) + 1 + wExp10;
    if ((int)nCount >= cchDst)
    {
        Assert(0);
        return FALSE;
    }


    if (wExp10 <= -6 || wExp10 > 21)
    {
        // Exponential notation - first digit
        *pchDst++ = *pbSrc++ + '0';

        if (pbSrc < pbLim)
        {
            // Decimal point and remaining digits
            *pchDst++ = '.';
            while (pbSrc < pbLim)
                *pchDst++ = *pbSrc++ + '0';
        }

        // 'e' and exponent sign
        *pchDst++ = 'e';
        if (--wExp10 < 0)
        {
            *pchDst++ = '-';
            wExp10 = -wExp10;
        }
        else
            *pchDst++ = '+';

        // Exponent Digits
        Assert(wExp10 < 1000);
        if (wExp10 >= 100)
        {
            *pchDst++ = (wchar_t)('0' + wExp10 / 100);
            wExp10 %= 100;
            *pchDst++ = (wchar_t)('0' + wExp10 / 10);
            wExp10 %= 10;
        }
        else if (wExp10 >= 10)
        {
            *pchDst++ = (wchar_t)('0' + wExp10 / 10);
            wExp10 %= 10;
        }
#pragma prefast(suppress:26014, "We have calculate the check the buffer size above already")
        *pchDst++ = (wchar_t)('0' + wExp10);
        *pchDst = 0;
    }
    else if (wExp10 <= 0)
    {
        // Just fractional stuff
        *pchDst++ = '0';
#pragma prefast(suppress:26014, "We have calculate the check the buffer size above already")
        *pchDst++ = '.';
        for( ; wExp10 < 0; wExp10++)
            *pchDst++ = '0';
        while (pbSrc < pbLim)
            *pchDst++ = *pbSrc++ + '0';
        *pchDst = 0;
    }
    else
    {
        // Stuff to the left of the decimal point
        while (pbSrc < pbLim)
        {
            *pchDst++ = *pbSrc++ + '0';
            if (--wExp10 == 0 && pbSrc < pbLim)
                *pchDst++ = '.';
        }
        for( ; wExp10 > 0; wExp10--)
            *pchDst++ = '0';
        *pchDst = 0;
    }
    return TRUE;
}

__success(return <= nDstBufSize)
#pragma prefast(suppress:6101, "when return value is > nDstBufSize, the pchDst is not initialized.  Prefast doesn't seems to pick that up in the annotation")
static int FormatDigitsFixed(byte *pbSrc, byte *pbLim, int wExp10, int nFractionDigits, __out_ecount_part(nDstBufSize, return) wchar_t *pchDst, int nDstBufSize)
{
    AnalysisAssert(pbLim > pbSrc);
    AssertArrMem(pbSrc, pbLim - pbSrc);
    AnalysisAssert(nFractionDigits >= -1);
    // nFractionDigits == -1 => print exactly as many fractional digits as necessary : no trailing 0's.

    int n = 1; // the no. of chars. in the result.

    if (wExp10 <= 0)
    {
        // Just fractional stuff
        if( nFractionDigits < 0 )
        {
            // Set nFractionDigits such that we get all the significant digits and no trailing zeros
            AnalysisAssert(pbLim - pbSrc < INT_MAX);
            nFractionDigits = -wExp10 + (int)(pbLim - pbSrc);
        }

        n++; // for '0'
        if( nFractionDigits > 0 )
        {
            n += nFractionDigits + 1;
        }

        if( nDstBufSize >= n )
        {
            *pchDst++ = '0';

            if( nFractionDigits > 0 )
            {
                *pchDst++ = '.';
                for( ; wExp10 < 0 && nFractionDigits > 0; wExp10++, nFractionDigits--)
                    *pchDst++ = '0';
                for( ;pbSrc < pbLim && nFractionDigits > 0; nFractionDigits--)
                    *pchDst++ = *pbSrc++ + '0';
                while(nFractionDigits-- > 0)
                    *pchDst++ = '0';
            }
            *pchDst = 0;
        }
    }
    else
    {
        n += wExp10; // chars to the left of the decimal point.
        if( nFractionDigits < 0 )
        {
            // Set nFractionDigits such that we get all the significant digits and no trailing zeros
            nFractionDigits = (pbLim - pbSrc <= wExp10) ? 0 : (int)(pbLim - pbSrc) - wExp10;
        }
        if( nFractionDigits > 0)
            n += nFractionDigits + 1;

        if( nDstBufSize >= n )
        {
            // Stuff to the left of the decimal point
            for (;pbSrc < pbLim && wExp10 > 0; wExp10-- )
            {
                *pchDst++ = *pbSrc++ + '0';
            }

            if(wExp10 > 0)
            {
                for( ; wExp10 > 0; wExp10--)
                    *pchDst++ = '0';
            }

            //Stuff to the right of the decimal point
            if (nFractionDigits > 0)
            {
                *pchDst++ = '.';
                for (;pbSrc < pbLim && nFractionDigits > 0; nFractionDigits-- )
                {
                    *pchDst++ = *pbSrc++ + '0';
                }
                // Pad with 0's at the end to get the required number of fractional digits
                while( nFractionDigits-- > 0)
                    *pchDst++ = '0';
            }
            *pchDst = 0;
        }
    }
    return n;
}

__success(return <= cchDst)
static int FormatDigitsExponential(
                            byte *  pbSrc,
                            byte *  pbLim,
                            int     wExp10,
                            int     nFractionDigits,
                            __out_ecount_part(cchDst,return) wchar_t * pchDst,
                            int     cchDst
                            )
{
    AnalysisAssert(pbLim > pbSrc);
    Assert(pbLim - pbSrc <= kcbMaxRgb);
    AssertArrMem(pbSrc, pbLim - pbSrc);
    AssertArrMem(pchDst, cchDst);
    AnalysisAssert(wExp10 < 1000);

    __analysis_assume(pbLim > pbSrc);
    __analysis_assume(pbLim - pbSrc <= kcbMaxRgb);
    __analysis_assume(wExp10 < 1000);

    int n = 1; // first digit

    if (nFractionDigits < 0) // output as many fractional digits as we can
    {
        int cch = (int)(pbLim - (1 + pbSrc)); // 1 == first digit
        if (cch > 0)
        {
            n += (1 + cch); // 1 == '.'
        }
    }
    else if (nFractionDigits > 0)
    {
        n += (1 + nFractionDigits); // 1 == '.'
    }

    // 'e' and exponent sign
    n += 2;

    // Exponent Digits
    int wExp10Abs = ((wExp10-1) >= 0) ? (wExp10-1) : -(wExp10-1);
    if (wExp10Abs >= 100)
    {
        n += 3;
    }
    else if (wExp10Abs >= 10)
    {
        n += 2;
    }
    else
    {
        n += 1;
    }
    n++; // null terminator

    if (cchDst < n) return n;

#if DBG // save pchDst to validate n
    wchar_t * pchDstStart = pchDst;
#endif

    // First digit
    *pchDst++ = '0' + *pbSrc++;

    if (nFractionDigits < 0) // output as many fractional digits as we can
    {
        if (pbSrc < pbLim)
        {
            // Decimal point and remaining digits
            *pchDst++ = '.';
            do
            {
                *pchDst++ = '0' + *pbSrc++;
            } while (pbSrc < pbLim);
        }
    }
    else if (nFractionDigits > 0)
    {
        // Decimal point and remaining digits
        *pchDst++ = '.';
        for ( ; pbSrc < pbLim && nFractionDigits > 0; nFractionDigits--)
            *pchDst++ = '0' + *pbSrc++;

        while (nFractionDigits-- > 0)
            *pchDst++ = '0';
    }

    // 'e' and exponent sign
    *pchDst++ = 'e';
    if (--wExp10 < 0)
    {
#pragma prefast(suppress:26014, "We have calculate the check the buffer size above already")
        *pchDst++ = '-';
        wExp10 = -wExp10;
    }
    else
        *pchDst++ = '+';

    // Exponent Digits
    if (wExp10 >= 100)
    {
        *pchDst++ = (wchar_t)('0' + wExp10 / 100);
        wExp10 %= 100;
        *pchDst++ = (wchar_t)('0' + wExp10 / 10);
        wExp10 %= 10;
    }
    else if (wExp10 >= 10)
    {
        *pchDst++ = (wchar_t)('0' + wExp10 / 10);
        wExp10 %= 10;
    }
    *pchDst++ = (wchar_t)('0' + wExp10);

    *pchDst = 0;
    Assert(1 + pchDst - pchDstStart == n);
    Assert(1 + pchDst <= pchDstStart + cchDst);
    return n;
}

/*
*RoundTo:Rounds off the BCD representation of a number to a specified number of digits
* The input number is contained in  [pbSrc .. pbSrc + pbLim -1] and the output number
* will be contained in [pbDst .. *ppbLimRes - 1]. 'nDigits' is the number of digits
* to which the number should be rounded up.
*
* Return value: 1 if an extra leading 1 needed to be added, 0 otherwise.
*/
#pragma prefast(suppress:6101)
static int RoundTo(byte *pbSrc, byte *pbLim, int nDigits, __out_bcount(nDigits+1) byte *pbDst, byte **ppbLimRes )
{
    AnalysisAssert(pbLim > pbSrc);
    AssertArrMem(pbSrc, pbLim - pbSrc);
    AnalysisAssert(nDigits >= 0);

    int retVal = 0;

    if ((pbLim - pbSrc) < 0)
    {
        AnalysisAssert(FALSE);
        return 0;
    }

    if( pbLim - pbSrc <= nDigits )
    {
        // no change required
        js_memcpy_s( pbDst, nDigits + 1, pbSrc, pbLim - pbSrc );
        *ppbLimRes = pbDst + (pbLim - pbSrc);
    }
    else
    {
        int i = nDigits;

        if( pbSrc[i] >= 5 )
        {
            // Add 1 to the BCD representation.
            for( i = nDigits - 1; i >= 0; i-- )
            {

                if( pbSrc[i] + 1 > 9 )
                    pbDst[i] = 0;
                else
                {
                    pbDst[i] = pbSrc[i] + 1;
                    break;
                }
            }
            if( i < 0 && pbDst[0] == 0 )
            {
                // An extra leading '1' is required. Move the number in pbDst to the right
                // and tack it on.
                memmove(pbDst + 1, pbDst, nDigits);
                pbDst[0] = 1;
                retVal = 1;
            }
        }
        if( i > 0 )
            js_memcpy_s( pbDst, nDigits + 1, pbSrc, i );

        *ppbLimRes = pbDst + nDigits;
    }

    return retVal;
}

/*
* Format a number according to the given 'FormatType'. Used by
* the toFixed, toExponential and toPrecision methods.
* If 'ft' is FormatFixed or FormatExponential then nDigits is the
* number of fractional digits. If ft is FormatPrecision then nDigits
* is the precision.
*
* Returns the number of chars. in the result. If 'nDstBufSize'
* is less than this number, no data is written to the buffer 'pchDst'.
*/
int Js::NumberUtilities::FDblToStr(double dbl, Js::NumberUtilities::FormatType ft, int nDigits, __out_ecount(cchDst) wchar_t *pchDst, int cchDst)
{
    int n = 0; // the no. of chars in the result.
    int wExp10;
    byte rgb[kcbMaxRgb];
    byte *pbLim;

    if (!Js::NumberUtilities::IsFinite(dbl))
    {
        if (Js::NumberUtilities::IsNan(dbl))
        {
            n = 4; //(int)wcslen(OLESTR("NaN")) + 1;
            if( cchDst >= n )
                wcscpy_s(pchDst, cchDst, L"NaN");
        }
        else
        {
            n = 9; //(int)wcslen(OLESTR("Infinity")) + 1;
            int neg = 0;
            if (dbl < 0 )
            {
                neg = 1;
                n++;
            }
            if( cchDst >= n )
            {
                if (neg)
                    *pchDst++ = '-';
                wcscpy_s(pchDst, cchDst - neg, L"Infinity");
            }
        }
        return n;
    }
    if (0 == dbl)
    {
        rgb[0] = 0;
        pbLim = &rgb[1];
        wExp10 = 1;
    }
    else
    {
        // Handle the sign.
        if (Js::NumberUtilities::LuHiDbl(dbl) & 0x80000000)
        {
            n++;
            if( cchDst >= n)
            {
                *pchDst++ = '-';
                cchDst--;
            }
            Js::NumberUtilities::LuHiDbl(dbl) &= 0x7FFFFFFF;
        }

        if (!FDblToRgbFast(dbl, rgb, &wExp10, &pbLim) &&
            !FDblToRgbPrecise(dbl, rgb, &wExp10, &pbLim))
        {
            AssertMsg(FALSE, "Failure in FDblToRgbPrecise");
            return FALSE;
        }

    }

    // We have to round up and truncate the BCD representation of the mantissa
    // to the length required by the format.
    byte rgbAdj[kcbMaxRgb];
    byte *pbLimAdj = NULL;

    switch(ft)
    {
    case Js::NumberUtilities::FormatFixed:
        if( nDigits >= 0 )
        {
            //Either session pointer is null or session is in compat mode switch to compat handling
            if ((wExp10 + nDigits) > 0)
            {
                Assert(wExp10 + nDigits + 1 <= kcbMaxRgb);
                wExp10 += RoundTo(rgb, pbLim, wExp10 + nDigits, rgbAdj, &pbLimAdj);
            }
            else
            {
                //Special case: When negative power of 10 is more than most significant digit.
                if( rgb[0] >= 5 )
                {
                    rgbAdj[0] = 1;
                    wExp10 += 1;
                }
                else
                    rgbAdj[0] = 0;
                pbLimAdj = rgbAdj + 1;
            }
        }
        else
            RoundTo( rgb, pbLim, kcbMaxRgb-1, rgbAdj, &pbLimAdj );
        n += FormatDigitsFixed(rgbAdj, pbLimAdj, wExp10, nDigits, pchDst, cchDst);
        break;

    case Js::NumberUtilities::FormatExponential:
        if (nDigits >= 0)
        {
            Assert(nDigits + 2 <= kcbMaxRgb);
            wExp10 += RoundTo(rgb, pbLim, nDigits + 1, rgbAdj, &pbLimAdj);
        }
        else
            RoundTo( rgb, pbLim, kcbMaxRgb-1, rgbAdj, &pbLimAdj );
        n += FormatDigitsExponential(rgbAdj, pbLimAdj, wExp10 , nDigits, pchDst, cchDst);
        break;

    case Js::NumberUtilities::FormatPrecision:
        Assert(nDigits + 1 <= kcbMaxRgb);
        wExp10 += RoundTo( rgb, pbLim, nDigits, rgbAdj, &pbLimAdj );

        // NOTE: the 'e' in the toPrecision algorithm in the ECMA standard is equal to wExp - 1.
        if( wExp10 - 1 < -6 || wExp10 - 1 >= nDigits )
            n += FormatDigitsExponential(rgbAdj, pbLimAdj, wExp10, nDigits - 1, pchDst, cchDst);
        else
            n += FormatDigitsFixed(rgbAdj, pbLimAdj, wExp10, nDigits - wExp10, pchDst, cchDst);
        break;
    }
    return n;
}

BOOL Js::NumberUtilities::FDblToStr(double dbl, __out_ecount(cchDst) wchar_t *pchDst, int cchDst)
{
    if (!Js::NumberUtilities::IsFinite(dbl))
    {
        if (Js::NumberUtilities::IsNan(dbl))
            return 0 == wcscpy_s(pchDst, cchDst, L"NaN");
        else
        {
            if (dbl < 0)
            {
                if (cchDst < 10) return FALSE;
                *pchDst++ = '-';
                cchDst--;
            }

            return 0 == wcscpy_s(pchDst, cchDst, L"Infinity");
        }
    }

    if (0 == dbl)
    {
        if (cchDst < 2) return FALSE;
        *pchDst++ = '0';
        *pchDst = 0;
        return TRUE;
    }

    return FNonZeroFiniteDblToStr(dbl, pchDst, cchDst);
}

BOOL Js::NumberUtilities::FNonZeroFiniteDblToStr(double dbl, __out_ecount(cchDst) OLECHAR *pchDst, int cchDst)
{
    int wExp10;
    byte rgb[kcbMaxRgb];
    byte *pbLim;

    Assert(Js::NumberUtilities::IsFinite(dbl));
    Assert(dbl != 0);

    // Handle the sign.
    if (Js::NumberUtilities::LuHiDbl(dbl) & 0x80000000)
    {
        if (cchDst < 2) return FALSE;
        *pchDst++ = '-';
        cchDst--;
        Js::NumberUtilities::LuHiDbl(dbl) &= 0x7FFFFFFF;
    }

#if DBG
    double dblT;
    const wchar_t *pch;

    // In Debug, always call FDblToRgbPrecise and verify that it converts back.
    if (FDblToRgbPrecise(dbl, rgb, &wExp10, &pbLim))
    {
        if (FormatDigits(rgb, pbLim, wExp10, pchDst, cchDst))
        {
            bool likelyInt = true;
            dblT = StrToDbl<wchar_t>(pchDst, &pch,likelyInt);
            Assert(0 == *pch);
            Assert(dblT == dbl);
        }
        else
            AssertMsg(FALSE, "Failure in FormatDigits");
    }
    else
        AssertMsg(FALSE, "Failure in FDblToRgbPrecise");
#endif //DBG

    if (!FDblToRgbFast(dbl, rgb, &wExp10, &pbLim) &&
        !FDblToRgbPrecise(dbl, rgb, &wExp10, &pbLim))
    {
        AssertMsg(FALSE, "Failure in FDblToRgbPrecise");
        return FALSE;
    }

    if (!FormatDigits(rgb, pbLim, wExp10, pchDst, cchDst))
    {
        AssertMsg(FALSE, "Failure in FormatDigits");
        return FALSE;
    }

#if DBG
    bool likelyInt = true;
    dblT = StrToDbl<wchar_t>(pchDst, &pch, likelyInt);
    Assert(0 == *pch);
    Assert(dblT == dbl);
#endif //DBG

    return TRUE;
}

// Maximum number of digits to show for each base.
static const int g_rgcchSig[] =
{
    00,00,53,34,27,24,22,20,19,18,
    17,17,16,16,15,15,14,14,14,14,
    14,13,13,13,13,13,13,12,12,12,
    12,12,12,12,12,12,12
};

//
// Convert a non-Nan, non-Zero, non-Infinite double value to string. (Moved from JavascriptNumber.cpp).
//
_Success_(return)
BOOL Js::NumberUtilities::FNonZeroFiniteDblToStr(double dbl, _In_range_(2, 36) int radix, _Out_writes_(nDstBufSize) WCHAR* psz, int nDstBufSize)
{
    Assert(!Js::NumberUtilities::IsNan(dbl));
    Assert(dbl != 0);
    Assert(Js::NumberUtilities::IsFinite(dbl));

    Assert(radix != 10);
    Assert(radix >= 2 && radix <= 36);

    // convert to string with radix
    //( back compat port of FDblToStrRadix())

    int cbitDigit;
    double valueDen, valueT;
    int wExp2, wExp, wDig;
    int maxOutDigits, cchSig, cch;
    int len = nDstBufSize;
    wchar_t * ppsz = psz;

    if (0x80000000 & Js::NumberUtilities::LuHiDbl(dbl))
    {
        *ppsz++ = '-';
        len--;
        Js::NumberUtilities::LuHiDbl(dbl) &= 0x7FFFFFFF;
    }
    switch (radix)
    {
        // We special case log computations for powers of 2.
        case  2: cbitDigit = 1; break;
        case  4: cbitDigit = 2; break;
        case  8: cbitDigit = 3; break;
        case 16: cbitDigit = 4; break;
        case 32: cbitDigit = 5; break;
        default: cbitDigit = 0; break;
    }

    // REVIEW : fix this to do more accurate conversions? This is exact for
    // powers of 2, but not for other radixes.
    // REVIEW : round?
    wExp2 = (int)((Js::NumberUtilities::LuHiDbl(dbl) & 0x7FF00000) >> 20) - 0x03FF;
    maxOutDigits = g_rgcchSig[radix];
    __analysis_assume(maxOutDigits > 0);

    if (wExp2 < -60 || wExp2 > 60)
    {
        // Use exponential notation. Get the exponent and normalize.
        if (cbitDigit != 0)
        {
            // Power of 2. These computations are exact.
            wExp = wExp2 / cbitDigit;
            wExp2 = wExp * cbitDigit;

            // Avoid overflow and underflow.
            if (wExp2 > 0)
            {
                wExp2 -= cbitDigit;
                dbl /= radix;
            }
            else
            {
                wExp2 += cbitDigit;
                dbl *= radix;
            }

            Js::NumberUtilities::LuHiDbl(valueT) = (ulong)(0x03FF + wExp2) << 20;
            Js::NumberUtilities::LuLoDbl(valueT) = 0;
        }
        else
        {
            wExp = (int)floor(log(dbl) / log((double)radix) + 1.0);
            valueT = pow((double)radix, wExp);
            if (!Js::NumberUtilities::IsFinite(valueT))
            {
                valueT = pow((double)radix, --wExp);
            }
            else if (0 == valueT)
            {
                valueT = pow((double)radix, ++wExp);
            }
        }
        dbl = dbl / valueT;

        while (dbl < 1)
        {
            dbl *= radix;
            wExp--;
        }
        AssertMsg(1 <= dbl && dbl < radix, "malformed computation in radix toString()");

        // First digit.
        wDig = (int)dbl;
        if (len < 2)
        {
            return FALSE; //We run out of buffer size.
        }
        len--;
        *ppsz++ = ToDigit(wDig);
        maxOutDigits--;
        dbl -= wDig;

        // Radix point and remaining digits.
        if (0 != dbl)
        {
            if (len < maxOutDigits + 2)
            {
                return FALSE; //We run out of buffer size.
            }
            len -= maxOutDigits + 1;
            *ppsz++ = '.';
            while (dbl != 0 && maxOutDigits-- > 0)
            {
                dbl *= radix;
                wDig = (int)dbl;
                if (wDig >= radix)
                {
                    wDig = radix - 1;
                }
                *ppsz++ = ToDigit(wDig);
                dbl -= wDig;
            }
        }

        // Exponent.
        if (len < 9) // NOTE: may actually need less room
        {
            return FALSE; //We run out of buffer size.
        }
        *ppsz++ = '(';
        *ppsz++ = 'e';
        if (wExp < 0)
        {
            *ppsz++ = '-';
            wExp = -wExp;
        }
        else
        {
            *ppsz++ = '+';
        }
        if (wExp >= 10)
        {
            if (wExp >= 100)
            {
                if (wExp >= 1000)
                {
                    *ppsz++ = (wchar_t)('0' + wExp / 1000);
                    wExp %= 1000;
                }
                *ppsz++ = (wchar_t)('0' + wExp / 100);
                wExp %= 100;
            }
            *ppsz++ = (wchar_t)('0' + wExp / 10);
            wExp %= 10;
        }
        *ppsz++ = (wchar_t)('0' + wExp);
        *ppsz++ = ')';
        *ppsz = 0;

        return TRUE;
    }

    // Output the integer portion.
    if (1 <= dbl)
    {
        if (0 != cbitDigit)
        {
            wExp = wExp2 / cbitDigit;
            wExp2 = wExp * cbitDigit;

            Js::NumberUtilities::LuHiDbl(valueDen) = (ulong)(0x03FF + wExp2) << 20;
            Js::NumberUtilities::LuLoDbl(valueDen) = 0;
            cchSig = abs(wExp) + 1;
        }
        else
        {
            cchSig = 1;
            for (valueDen = 1; (valueT = valueDen * radix) <= dbl; valueDen = valueT)
            {
                cchSig++;
            }
        }
        AssertMsg(valueDen <= dbl && dbl < valueDen * radix, "Bad floating point format");

        __analysis_assume(cchSig >= 0);

        if (len < cchSig + 1)
        {
            return FALSE; //We run out of buffer size.
        }
        len -= cchSig;
        for (cch = 0; cch < cchSig; cch++)
        {
            wDig = (int)(dbl / valueDen);
            if (wDig >= radix)
            {
                wDig = radix - 1;
            }
            *ppsz++ = ToDigit(wDig);
            dbl -= wDig * valueDen;
            valueDen /= radix;
        }
    }
    else
    {
        if (len < 2)
        {
            return FALSE; //We run out of buffer size.
        }
        len--;
        *ppsz++ = '0';
        cchSig = 0;
    }

    // Output the fractional portion.
    if (0 != dbl && cchSig < maxOutDigits)
    {
        // Output the radix point.
        if (len < 3)
        {
            return FALSE; //We run out of buffer size.
        }
        len--;
        *ppsz++ = '.';
        do
        {
            dbl *= radix;
            wDig = (int)dbl;
            if (wDig >= radix)
            {
                wDig = radix - 1;
            }
            if (len < 2)
            {
                return FALSE; //We run out of buffer size.
            }
            len--;
            *ppsz++ = ToDigit(wDig);
            dbl -= wDig;
            if (0 != wDig || 0 != cchSig)
            {
                cchSig++;
            }
        } while (0 != dbl && cchSig < maxOutDigits);
    }

    if (len < 1)
    {
        return FALSE; //We run out of buffer size.
    }
    *ppsz = 0;

    return TRUE;
}


static const int64 ci64_2to64 = 0x43F0000000000000;
static const double cdbl_2to64 = *(double*)&ci64_2to64;
double Js::NumberUtilities::DblFromDecimal(DECIMAL * pdecIn)
{
    double dblRet;

    Assert(pdecIn->scale >= 0 && pdecIn->scale < 29);
    __analysis_assume(pdecIn->scale >= 0 && pdecIn->scale < 29);
    if ((LONG)pdecIn->Mid32 < 0)
    {

        dblRet = (cdbl_2to64 + (double)(LONGLONG)pdecIn->Lo64 +
            (double)pdecIn->Hi32 * cdbl_2to64) / g_rgdblTens[pdecIn->scale];
    }
    else
    {
        dblRet = ((double)(LONGLONG)pdecIn->Lo64 +
            (double)pdecIn->Hi32 * cdbl_2to64) / g_rgdblTens[pdecIn->scale];
    }

    if (pdecIn->sign != 0)
        dblRet = -dblRet;

    return dblRet;
}

void Js::NumberUtilities::CodePointAsSurrogatePair(codepoint_t codePointValue, __out wchar_t* first, __out wchar_t* second)
{
    AssertMsg(first != nullptr && second != nullptr, "Null ptr's passed in for out.");
    AssertMsg(IsInSupplementaryPlane(codePointValue), "Code point is not a surrogate pair.");
    codePointValue -= 0x10000;
    *first = (wchar_t)(codePointValue >> 10) + 0xD800;
    *second = (wchar_t)(codePointValue & 0x3FF /* This is same as cpv % 0x400 */) + 0xDC00;
}

codepoint_t Js::NumberUtilities::SurrogatePairAsCodePoint(codepoint_t first, codepoint_t second)
{
    AssertMsg(IsSurrogateLowerPart(first) && IsSurrogateUpperPart(second), "Characters don't form a surrogate pair.");
    return ((first - 0xD800) << 10) + (second - 0xDC00) + 0x10000;
}

bool Js::NumberUtilities::IsSurrogateUpperPart(codepoint_t codePointValue)
{
    return codePointValue >= 0xDC00 && codePointValue <= 0xDFFF;
}

bool Js::NumberUtilities::IsSurrogateLowerPart(codepoint_t codePointValue)
{
    return codePointValue >= 0xD800 && codePointValue <= 0xDBFF;
}

bool Js::NumberUtilities::IsInSupplementaryPlane(codepoint_t codePointValue)
{
    Assert(codePointValue <= 0x10FFFF);
    return codePointValue >= 0x10000;
}

} // namespace Js
