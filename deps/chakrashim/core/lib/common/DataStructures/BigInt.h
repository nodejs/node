//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    /***************************************************************************
        Big non-negative integer class.
    ***************************************************************************/
    class BigInt
    {
    private:
        // Make this big enough that we rarely have to call malloc.
        enum { kcluMaxInit = 30 };

        long m_cluMax;
        long m_clu;
        ulong *m_prglu;
        ulong m_rgluInit[kcluMaxInit];

        inline BigInt & operator= (BigInt &bi);
        bool FResize(long clu);

#if DBG
        #define AssertBi(pbi) Assert(pbi); (pbi)->AssertValid(true);
        #define AssertBiNoVal(pbi) Assert(pbi); (pbi)->AssertValid(false);
        inline void AssertValid(bool fCheckVal);
#else //!DBG
        #define AssertBi(pbi)
        #define AssertBiNoVal(pbi)
#endif //!DBG

    public:
        BigInt(void);
        ~BigInt(void);

        bool FInitFromRglu(ulong *prglu, long clu);
        bool FInitFromBigint(BigInt *pbiSrc);
        template <typename EncodedChar>
        bool FInitFromDigits(const EncodedChar *prgch, long cch, long *pcchDec);
        bool FMulAdd(ulong luMul, ulong luAdd);
        bool FMulPow5(long c5);
        bool FShiftLeft(long cbit);
        void ShiftLusRight(long clu);
        void ShiftRight(long cbit);
        int Compare(BigInt *pbi);
        bool FAdd(BigInt *pbi);
        void Subtract(BigInt *pbi);
        int DivRem(BigInt *pbi);

        long Clu(void);
        ulong Lu(long ilu);
        double GetDbl(void);
    };
}
