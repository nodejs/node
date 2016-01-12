//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonDataStructuresPch.h"
#include "DataStructures\BigInt.h"
#include "common\NumberUtilitiesBase.h"
#include "common\NumberUtilities.h"

namespace Js
{
    BigInt & BigInt::operator= (BigInt &bi)
    {
        AssertMsg(false, "can't assign BigInts");
        return *this;
    }

#if DBG
    void BigInt::AssertValid(bool fCheckVal)
    {
        Assert(m_cluMax >= kcluMaxInit);
        Assert(m_prglu != 0);
        Assert(m_clu >= 0 && m_clu <= m_cluMax);
        Assert(!fCheckVal || 0 == m_clu || 0 != m_prglu[m_clu - 1]);
        Assert((m_prglu == m_rgluInit) == (m_cluMax == kcluMaxInit));
    }
#endif

    BigInt::BigInt(void)
    {
        m_cluMax = kcluMaxInit;
        m_clu = 0;
        m_prglu = m_rgluInit;
        AssertBi(this);
    }

    BigInt::~BigInt(void)
    {
        if (m_prglu != m_rgluInit)
            free(m_prglu);
    }

    long BigInt::Clu(void)
    {
        return m_clu;
    }

    ulong BigInt::Lu(long ilu)
    {
        AssertBi(this);
        Assert(ilu < m_clu);
        return m_prglu[ilu];
    }

    bool BigInt::FResize(long clu)
    {
        AssertBiNoVal(this);

        ulong *prglu;

        if (clu <= m_cluMax)
            return true;

        clu += clu;
        if (m_prglu == m_rgluInit)
        {
            if ((INT_MAX / sizeof(ulong) < clu) || (NULL == (prglu = (ulong *)malloc(clu * sizeof(ulong)))))
                return false;
            if (0 < m_clu)
                js_memcpy_s(prglu, clu * sizeof(ulong), m_prglu, m_clu * sizeof(ulong));
        }
        else if (NULL == (prglu = (ulong *)realloc(m_prglu, clu * sizeof(ulong))))
            return false;

        m_prglu = prglu;
        m_cluMax = clu;

        AssertBiNoVal(this);
        return true;
    }

    bool BigInt::FInitFromRglu(ulong *prglu, long clu)
    {
        AssertBi(this);
        Assert(clu >= 0);
        Assert(prglu != 0);

        if (clu > m_cluMax && !FResize(clu))
            return false;
        m_clu = clu;
        if (clu > 0)
            js_memcpy_s(m_prglu, m_clu * sizeof(ulong), prglu, clu * sizeof(ulong));

        AssertBi(this);
        return true;
    }

    bool BigInt::FInitFromBigint(BigInt *pbiSrc)
    {
        AssertBi(this);
        AssertBi(pbiSrc);
        Assert(this != pbiSrc);

        return FInitFromRglu(pbiSrc->m_prglu, pbiSrc->m_clu);
    }

    template <typename EncodedChar>
    bool BigInt::FInitFromDigits(const EncodedChar *prgch, long cch, long *pcchDig)
    {
        AssertBi(this);
        Assert(cch >= 0);
        Assert(prgch != 0);
        Assert(pcchDig != 0);

        ulong luAdd;
        ulong luMul;
        long clu = (cch + 8) / 9;
        const EncodedChar *pchLim = prgch + cch;

        if (clu > m_cluMax && !FResize(clu))
            return false;
        m_clu = 0;

        luAdd = 0;
        luMul = 1;
        for (*pcchDig = cch; prgch < pchLim; prgch++)
        {
            if (*prgch == '.')
            {
                (*pcchDig)--;
                continue;
            }
            Assert(NumberUtilities::IsDigit(*prgch));
            if (luMul == 1000000000)
            {
                AssertVerify(FMulAdd(luMul, luAdd));
                luMul = 1;
                luAdd = 0;
            }
            luMul *= 10;
            luAdd = luAdd * 10 + *prgch - '0';
        }
        Assert(1 < luMul);
        AssertVerify(FMulAdd(luMul, luAdd));

        AssertBi(this);
        return true;
    }

    bool BigInt::FMulAdd(ulong luMul, ulong luAdd)
    {
        AssertBi(this);
        Assert(luMul != 0);

        ulong luT;
        ulong *plu = m_prglu;
        ulong *pluLim = plu + m_clu;

        for (; plu < pluLim; plu++)
        {
            *plu = NumberUtilities::MulLu(*plu, luMul, &luT);
            if (luAdd)
                luT += NumberUtilities::AddLu(plu, luAdd);
            luAdd = luT;
        }
        if (0 == luAdd)
            goto LDone;
        if (m_clu >= m_cluMax && !FResize(m_clu + 1))
            return false;
        m_prglu[m_clu++] = luAdd;

LDone:
        AssertBi(this);
        return true;
    }

    bool BigInt::FMulPow5(long c5)
    {
        AssertBi(this);
        Assert(c5 >= 0);

        const ulong k5to13 = 1220703125;
        long clu = (c5 + 12) / 13;
        ulong luT;

        if (0 == m_clu || 0 == c5)
            return true;

        if (m_clu + clu > m_cluMax && !FResize(m_clu + clu))
            return false;

        for (; c5 >= 13; c5 -= 13)
            AssertVerify(FMulAdd(k5to13, 0));

        if (c5 > 0)
        {
            for (luT = 5; --c5 > 0; )
                luT *= 5;
            AssertVerify(FMulAdd(luT, 0));
        }

        AssertBi(this);
        return true;
    }

    bool BigInt::FShiftLeft(long cbit)
    {
        AssertBi(this);
        Assert(cbit >= 0);

        long ilu;
        long clu;
        ulong luExtra;

        if (0 == cbit || 0 == m_clu)
            return true;

        clu = cbit >> 5;
        cbit &= 0x001F;

        if (cbit > 0)
        {
            ilu = m_clu - 1;
            luExtra = m_prglu[ilu] >> (32 - cbit);

            for (; ; ilu--)
            {
                m_prglu[ilu] <<= cbit;
                if (0 == ilu)
                    break;
                m_prglu[ilu] |= m_prglu[ilu - 1] >> (32 - cbit);
            }
        }
        else
            luExtra = 0;

        if (clu > 0 || 0 != luExtra)
        {
            // Make sure there's enough room.
            ilu = m_clu + (0 != luExtra) + clu;
            if (ilu > m_cluMax && !FResize(ilu))
                return false;

            if (clu > 0)
            {
                // Shift the ulongs.
                memmove(m_prglu + clu, m_prglu, m_clu * sizeof(ulong));
                memset(m_prglu, 0, clu * sizeof(ulong));
                m_clu += clu;
            }

            // Throw on the extra one.
            if (0 != luExtra)
                m_prglu[m_clu++] = luExtra;
        }

        AssertBi(this);
        return true;
    }

    void BigInt::ShiftLusRight(long clu)
    {
        AssertBi(this);
        Assert(clu >= 0);

        if (clu >= m_clu)
        {
            m_clu = 0;
            AssertBi(this);
            return;
        }
        if (clu > 0)
        {
            memmove(m_prglu, m_prglu + clu, (m_clu - clu) * sizeof(ulong));
            m_clu -= clu;
        }

        AssertBi(this);
    }

    void BigInt::ShiftRight(long cbit)
    {
        AssertBi(this);
        Assert(cbit >= 0);

        long ilu;
        long clu = cbit >> 5;
        cbit &= 0x001F;

        if (clu > 0)
            ShiftLusRight(clu);

        if (cbit == 0 || m_clu == 0)
        {
            AssertBi(this);
            return;
        }

        for (ilu = 0; ; )
        {
            m_prglu[ilu] >>= cbit;
            if (++ilu >= m_clu)
            {
            // Last one.
                if (0 == m_prglu[ilu - 1])
                    m_clu--;
                break;
            }
            m_prglu[ilu - 1] |= m_prglu[ilu] << (32 - cbit);
        }

        AssertBi(this);
    }

    int BigInt::Compare(BigInt *pbi)
    {
        AssertBi(this);
        AssertBi(pbi);

        long ilu;

        if (m_clu > pbi->m_clu)
            return 1;
        if (m_clu < pbi->m_clu)
            return -1;
        if (0 == m_clu)
            return 0;

#pragma prefast(suppress:__WARNING_LOOP_ONLY_EXECUTED_ONCE,"noise")
        for (ilu = m_clu - 1; m_prglu[ilu] == pbi->m_prglu[ilu]; ilu--)
        {
            if (0 == ilu)
                return 0;
        }
        Assert(ilu >= 0 && ilu < m_clu);
        Assert(m_prglu[ilu] != pbi->m_prglu[ilu]);

        return (m_prglu[ilu] > pbi->m_prglu[ilu]) ? 1 : -1;
    }

    bool BigInt::FAdd(BigInt *pbi)
    {
        AssertBi(this);
        AssertBi(pbi);
        Assert(this != pbi);

        long cluMax, cluMin;
        long ilu;
        int wCarry;

        if ((cluMax = m_clu) < (cluMin = pbi->m_clu))
        {
            cluMax = pbi->m_clu;
            cluMin = m_clu;
            if (cluMax > m_cluMax && !FResize(cluMax + 1))
                return false;
        }

        wCarry = 0;
        for (ilu = 0; ilu < cluMin; ilu++)
        {
            if (0 != wCarry)
                wCarry = NumberUtilities::AddLu(&m_prglu[ilu], wCarry);
            wCarry += NumberUtilities::AddLu(&m_prglu[ilu], pbi->m_prglu[ilu]);
        }

        if (m_clu < pbi->m_clu)
        {
            for (; ilu < cluMax; ilu++)
            {
                m_prglu[ilu] = pbi->m_prglu[ilu];
                if (0 != wCarry)
                    wCarry = NumberUtilities::AddLu(&m_prglu[ilu], wCarry);
            }
            m_clu = cluMax;
        }
        else
        {
            for (; 0 != wCarry && ilu < cluMax; ilu++)
                wCarry = NumberUtilities::AddLu(&m_prglu[ilu], wCarry);
        }

        if (0 != wCarry)
        {
            if (m_clu >= m_cluMax && !FResize(m_clu + 1))
                return false;
            m_prglu[m_clu++] = wCarry;
        }

        AssertBi(this);
        return true;
    }

    void BigInt::Subtract(BigInt *pbi)
    {
        AssertBi(this);
        AssertBi(pbi);
        Assert(this != pbi);

        long ilu;
        int wCarry;
        ulong luT;

        if (m_clu < pbi->m_clu)
            goto LNegative;

        wCarry = 1;
        for (ilu = 0; (ilu < pbi->m_clu) && (ilu < pbi->m_cluMax); ilu++)
        {
            Assert(wCarry == 0 || wCarry == 1);
            luT = pbi->m_prglu[ilu];

            // NOTE: We should really do:
            //    wCarry = AddLu(&m_prglu[ilu], wCarry);
            //    wCarry += AddLu(&m_prglu[ilu], ~luT);
            // The only case where this is different than
            //    wCarry = AddLu(&m_prglu[ilu], ~luT + wCarry);
            // is when luT == 0 and 1 == wCarry, in which case we don't
            // need to add anything and wCarry should still be 1, so we can
            // just skip the operations.

            if (0 != luT || 0 == wCarry)
                wCarry = NumberUtilities::AddLu(&m_prglu[ilu], ~luT + wCarry);
        }
        while ((0 == wCarry) && (ilu < m_clu) && (ilu < m_cluMax))
            wCarry = NumberUtilities::AddLu(&m_prglu[ilu], 0xFFFFFFFF);

        if (0 == wCarry)
        {
LNegative:
            // pbi was bigger than this.
            AssertMsg(false, "Who's subtracting to negative?");
            m_clu = 0;
        }
        else if (ilu == m_clu)
        {
            // Trim off zeros.
            while (--ilu >= 0 && 0 == m_prglu[ilu])
                ;
            m_clu = ilu + 1;
        }

        AssertBi(this);
    }

    int BigInt::DivRem(BigInt *pbi)
    {
        AssertBi(this);
        AssertBi(pbi);
        Assert(this != pbi);

        long ilu, clu;
        int wCarry;
        int wQuo;
        int wT;
        ulong luT, luHi, luLo;

        clu = pbi->m_clu;
        Assert(m_clu <= clu);
        if ((m_clu < clu) || (clu <= 0))
            return 0;

        // Get a lower bound on the quotient.
        wQuo = (int)(m_prglu[clu - 1] / (pbi->m_prglu[clu - 1] + 1));
        Assert(wQuo >= 0 && wQuo <= 9);

        // Handle 0 and 1 as special cases.
        switch (wQuo)
        {
        case 0:
            break;
        case 1:
            Subtract(pbi);
            break;
        default:
            luHi = 0;
            wCarry = 1;
            for (ilu = 0; ilu < clu; ilu++)
            {
                Assert(wCarry == 0 || wCarry == 1);

                // Compute the product.
                luLo = NumberUtilities::MulLu(wQuo, pbi->m_prglu[ilu], &luT);
                luHi = luT + NumberUtilities::AddLu(&luLo, luHi);

                // Subtract the product. See note in BigInt::Subtract.
                if (0 != luLo || 0 == wCarry)
                    wCarry = NumberUtilities::AddLu(&m_prglu[ilu], ~luLo + wCarry);
            }
            Assert(1 == wCarry);
            Assert(ilu == clu);

            // Trim off zeros.
            while (--ilu >= 0 && 0 == m_prglu[ilu])
                ;
            m_clu = ilu + 1;
        }

        if (wQuo < 9 && (wT = Compare(pbi)) >= 0)
        {
            // Quotient was off too small (by one).
            wQuo++;
            if (wT == 0)
                m_clu = 0;
            else
                Subtract(pbi);
        }
        Assert(Compare(pbi) < 0);

        return wQuo;
    }

    double BigInt::GetDbl(void)
    {
        double dbl;
        ulong luHi, luLo;
        ulong lu1, lu2, lu3;
        long ilu;
        int cbit;

        switch (m_clu)
        {
        case 0:
            return 0;
        case 1:
            return m_prglu[0];
        case 2:
            dbl = m_prglu[1];
            NumberUtilities::LuHiDbl(dbl) += 0x02000000;
            return dbl + m_prglu[0];
        }

        Assert(3 <= m_clu);
        if (m_clu > 32)
        {
            // Result is infinite.
            NumberUtilities::LuHiDbl(dbl) = 0x7FF00000;
            NumberUtilities::LuLoDbl(dbl) = 0;
            return dbl;
        }

        lu1 = m_prglu[m_clu - 1];
        lu2 = m_prglu[m_clu - 2];
        lu3 = m_prglu[m_clu - 3];
        Assert(0 != lu1);
        cbit = 31 - NumberUtilities::CbitZeroLeft(lu1);

        if (cbit == 0)
        {
            luHi = lu2;
            luLo = lu3;
        }
        else
        {
            luHi = (lu1 << (32 - cbit)) | (lu2 >> cbit);
            // Or 1 if there are any remaining nonzero bits in lu3, so we take
            // them into account when rounding.
            luLo = (lu2 << (32 - cbit)) | (lu3 >> cbit) | (0 != (lu3 << (32 - cbit)));
        }

        // Set the mantissa bits.
        NumberUtilities::LuHiDbl(dbl) = luHi >> 12;
        NumberUtilities::LuLoDbl(dbl) = (luHi << 20) | (luLo >> 12);

        // Set the exponent field.
        NumberUtilities::LuHiDbl(dbl) |= (0x03FF + cbit + (m_clu - 1) * 0x0020) << 20;

        // Do IEEE rounding.
        if (luLo & 0x0800)
        {
            if ((luLo & 0x07FF) || (NumberUtilities::LuLoDbl(dbl) & 1))
            {
                if (0 == ++NumberUtilities::LuLoDbl(dbl))
                    ++NumberUtilities::LuHiDbl(dbl);
            }
            else
            {
                // If there are any non-zero bits in m_prglu from 0 to m_clu - 4, round up.
                for (ilu = m_clu - 4; ilu >= 0; ilu--)
                {
                    if (0 != m_prglu[ilu])
                    {
                        if (0 == ++NumberUtilities::LuLoDbl(dbl))
                            ++NumberUtilities::LuHiDbl(dbl);
                        break;
                    }
                }
            }
        }

        return dbl;
    }

    namespace // anonymous
    {
        template <typename EncodedChar>
        void Reference(void)
        {
            BigInt i;
            i.FInitFromDigits<EncodedChar>(NULL, 0, NULL);
        }

        void Instantiations(void)
        {
            Reference<wchar_t>();
            Reference<utf8char_t>();
        }
    }

}
