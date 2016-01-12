//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLanguagePch.h"

#if _M_IX86 || _M_AMD64

namespace Js
{
    // SIMD.Int8x16 operation wrappers that cover intrinsics for x86/x64 system
    SIMDValue SIMDInt8x16Operation::OpInt8x16(int8 x0, int8 x1, int8 x2, int8 x3
        , int8 x4, int8 x5, int8 x6, int8 x7
        , int8 x8, int8 x9, int8 x10, int8 x11
        , int8 x12, int8 x13, int8 x14, int8 x15)
    {
        X86SIMDValue x86Result;
        // Sets the 16 signed 8-bit integer values, note in revised order: starts with x15 below
        x86Result.m128i_value = _mm_set_epi8((int8)x15, (int8)x14, (int8)x13, (int8)x12, (int8)x11, (int8)x10, (int8)x9, (int8)x8, (int8)x7, (int8)x6, (int8)x5, (int8)x4, (int8)x3, (int8)x2, (int8)x1, (int8)x0);

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDInt8x16Operation::OpInt8x16(const SIMDValue& v)
    {
        X86SIMDValue x86Result;
        // Sets the 16 signed 8-bit integer values, note in revised order: starts with x15 below
        x86Result.m128i_value = _mm_set_epi8(v.i8[15], v.i8[14], v.i8[13], v.i8[12], v.i8[11]
                                            , v.i8[10], v.i8[9], v.i8[8], v.i8[7], v.i8[6]
                                            , v.i8[5], v.i8[4], v.i8[3], v.i8[2], v.i8[1], v.i8[0]);

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDInt8x16Operation::OpZero()
    {
        X86SIMDValue x86Result;
        // Sets the 128-bit value to zero
        x86Result.m128i_value = _mm_setzero_si128();

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDInt8x16Operation::OpSplat(int8 x)
    {
        X86SIMDValue x86Result;
        // set 16 signed 8-bit integers values to input value x
        x86Result.m128i_value = _mm_set1_epi8(x);

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDInt8x16Operation::OpSplat(const SIMDValue& v) // TODO arun: not in spec
    {
        X86SIMDValue x86Result;
        // set 16 signed 8-bit integers values to input value(v.i8[SIMD_X])
        x86Result.m128i_value = _mm_set1_epi8(v.i8[0]);

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDInt8x16Operation::OpFromFloat32x4Bits(const SIMDValue& value)
    {
        X86SIMDValue x86Result;
        X86SIMDValue v = X86SIMDValue::ToX86SIMDValue(value);

        _mm_store_ps(x86Result.simdValue.f32, v.m128_value);

        return X86SIMDValue::ToSIMDValue(x86Result);
    }
    SIMDValue SIMDInt8x16Operation::OpFromInt32x4Bits(const SIMDValue& value)
    {
        X86SIMDValue x86Result;

        x86Result.m128i_value = _mm_set_epi8(value.i8[15], value.i8[14], value.i8[13], value.i8[12]
            , value.i8[11], value.i8[10], value.i8[9], value.i8[8]
            , value.i8[7], value.i8[6], value.i8[5], value.i8[4]
            , value.i8[5], value.i8[2], value.i8[1], value.i8[0]);

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    //// Unary Ops

    SIMDValue SIMDInt8x16Operation::OpNeg(const SIMDValue& value)
    {
        X86SIMDValue x86Result;

        X86SIMDValue SIGNMASK, temp;
        X86SIMDValue negativeOnes = { { -1, -1, -1, -1 } };
        X86SIMDValue v = X86SIMDValue::ToX86SIMDValue(value);

        temp.m128i_value = _mm_andnot_si128(v.m128i_value, negativeOnes.m128i_value); // (~value) & (negative ones)
        SIGNMASK.m128i_value = _mm_set1_epi8(0x00000001);                            // set SIGNMASK to 1
        x86Result.m128i_value = _mm_add_epi8(SIGNMASK.m128i_value, temp.m128i_value);// add 16 integers respectively

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDInt8x16Operation::OpNot(const SIMDValue& value)
    {
        X86SIMDValue x86Result;

        X86SIMDValue negativeOnes = { { -1, -1, -1, -1 } };
        X86SIMDValue temp = X86SIMDValue::ToX86SIMDValue(value);
        x86Result.m128i_value = _mm_andnot_si128(temp.m128i_value, negativeOnes.m128i_value);

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDInt8x16Operation::OpAdd(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        X86SIMDValue x86Result;
        X86SIMDValue tmpaValue = X86SIMDValue::ToX86SIMDValue(aValue);
        X86SIMDValue tmpbValue = X86SIMDValue::ToX86SIMDValue(bValue);

        x86Result.m128i_value = _mm_add_epi8(tmpaValue.m128i_value, tmpbValue.m128i_value); // a + b

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDInt8x16Operation::OpSub(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        X86SIMDValue x86Result;
        X86SIMDValue tmpaValue = X86SIMDValue::ToX86SIMDValue(aValue);
        X86SIMDValue tmpbValue = X86SIMDValue::ToX86SIMDValue(bValue);

        x86Result.m128i_value = _mm_sub_epi8(tmpaValue.m128i_value, tmpbValue.m128i_value); // a - b

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDInt8x16Operation::OpMul(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        SIMDValue result;
        X86SIMDValue x86Result;
        X86SIMDValue x86tmp1;
        X86SIMDValue x86tmp2;
        X86SIMDValue x86tmp3;
        const _x86_SIMDValue X86_LOWBYTE_MASK  = { 0x00ff00ff, 0x00ff00ff, 0x00ff00ff, 0x00ff00ff };
        X86SIMDValue tmpaValue = X86SIMDValue::ToX86SIMDValue(aValue);
        X86SIMDValue tmpbValue = X86SIMDValue::ToX86SIMDValue(bValue);

        if (AutoSystemInfo::Data.SSE2Available())
        {
            // (ah* 2^8 + al) * (bh *2^8 + bl) = (ah*bh* 2^8 + al*bh + ah* bl) * 2^8 + al * bl
            x86tmp1.m128i_value = _mm_mullo_epi16(tmpaValue.m128i_value, tmpbValue.m128i_value);
            x86tmp2.m128i_value = _mm_and_si128(x86tmp1.m128i_value, X86_LOWBYTE_MASK.m128i_value);

            tmpaValue.m128i_value = _mm_srli_epi16(tmpaValue.m128i_value, 8);
            tmpbValue.m128i_value = _mm_srli_epi16(tmpbValue.m128i_value, 8);
            x86tmp3.m128i_value   = _mm_mullo_epi16(tmpaValue.m128i_value, tmpbValue.m128i_value);
            x86tmp3.m128i_value   = _mm_slli_epi16(x86tmp3.m128i_value, 8);

            x86Result.m128i_value = _mm_or_si128(x86tmp2.m128i_value, x86tmp3.m128i_value);

            return X86SIMDValue::ToSIMDValue(x86Result);
        }
        else
        {
            for (uint idx = 0; idx < 16; ++idx)
            {
                result.i8[idx] = aValue.i8[idx] * bValue.i8[idx];
            }
        }

        return result;
    }

    SIMDValue SIMDInt8x16Operation::OpAnd(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        X86SIMDValue x86Result;
        X86SIMDValue tmpaValue = X86SIMDValue::ToX86SIMDValue(aValue);
        X86SIMDValue tmpbValue = X86SIMDValue::ToX86SIMDValue(bValue);

        x86Result.m128i_value = _mm_and_si128(tmpaValue.m128i_value, tmpbValue.m128i_value); // a & b

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDInt8x16Operation::OpOr(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        X86SIMDValue x86Result;
        X86SIMDValue tmpaValue = X86SIMDValue::ToX86SIMDValue(aValue);
        X86SIMDValue tmpbValue = X86SIMDValue::ToX86SIMDValue(bValue);

        x86Result.m128i_value = _mm_or_si128(tmpaValue.m128i_value, tmpbValue.m128i_value); // a | b

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDInt8x16Operation::OpXor(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        X86SIMDValue x86Result;
        X86SIMDValue tmpaValue = X86SIMDValue::ToX86SIMDValue(aValue);
        X86SIMDValue tmpbValue = X86SIMDValue::ToX86SIMDValue(bValue);

        x86Result.m128i_value = _mm_xor_si128(tmpaValue.m128i_value, tmpbValue.m128i_value); // a ^ b

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDInt8x16Operation::OpLessThan(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        X86SIMDValue x86Result;
        X86SIMDValue tmpaValue = X86SIMDValue::ToX86SIMDValue(aValue);
        X86SIMDValue tmpbValue = X86SIMDValue::ToX86SIMDValue(bValue);
        x86Result.m128i_value = _mm_cmplt_epi8(tmpaValue.m128i_value, tmpbValue.m128i_value); // compare a < b?

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDInt8x16Operation::OpEqual(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        X86SIMDValue x86Result;
        X86SIMDValue tmpaValue = X86SIMDValue::ToX86SIMDValue(aValue);
        X86SIMDValue tmpbValue = X86SIMDValue::ToX86SIMDValue(bValue);
        x86Result.m128i_value = _mm_cmpeq_epi8(tmpaValue.m128i_value, tmpbValue.m128i_value); // compare a == b?

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDInt8x16Operation::OpGreaterThan(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        X86SIMDValue x86Result;
        X86SIMDValue tmpaValue = X86SIMDValue::ToX86SIMDValue(aValue);
        X86SIMDValue tmpbValue = X86SIMDValue::ToX86SIMDValue(bValue);
        x86Result.m128i_value = _mm_cmpgt_epi8(tmpaValue.m128i_value, tmpbValue.m128i_value); // compare a > b?

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDInt8x16Operation::OpShiftLeftByScalar(const SIMDValue& value, int8 count)
    {
        X86SIMDValue x86Result;
        X86SIMDValue tmpaValue = X86SIMDValue::ToX86SIMDValue(value);
        X86SIMDValue x86tmp1;

        const _x86_SIMDValue X86_LOWBYTE_MASK  = { 0x00ff00ff, 0x00ff00ff, 0x00ff00ff, 0x00ff00ff };
        const _x86_SIMDValue X86_HIGHBYTE_MASK = { 0xff00ff00, 0xff00ff00, 0xff00ff00, 0xff00ff00 };

        if (count < 0 || count > 8)
            count = 8;

        if (AutoSystemInfo::Data.SSE2Available())
        {
            x86tmp1.m128i_value   = _mm_and_si128(tmpaValue.m128i_value, X86_HIGHBYTE_MASK.m128i_value);
            x86tmp1.m128i_value   = _mm_slli_epi16(x86tmp1.m128i_value, count);

            tmpaValue.m128i_value = _mm_slli_epi16(tmpaValue.m128i_value, count);
            tmpaValue.m128i_value = _mm_and_si128(tmpaValue.m128i_value, X86_LOWBYTE_MASK.m128i_value);

            x86Result.m128i_value = _mm_or_si128(tmpaValue.m128i_value, x86tmp1.m128i_value);

            return X86SIMDValue::ToSIMDValue(x86Result);
        }
        else
        {
            SIMDValue result;
            for (uint idx = 0; idx < 16; ++idx)
            {
                result.i8[idx] = value.i8[idx] << count;
            }

            return result;
        }
    }

    SIMDValue SIMDInt8x16Operation::OpShiftRightLogicalByScalar(const SIMDValue& value, int8 count)
    {
        X86SIMDValue x86Result;
        X86SIMDValue tmpaValue = X86SIMDValue::ToX86SIMDValue(value);
        X86SIMDValue x86tmp1;

        const _x86_SIMDValue X86_LOWBYTE_MASK = { 0x00ff00ff, 0x00ff00ff, 0x00ff00ff, 0x00ff00ff };
        const _x86_SIMDValue X86_HIGHBYTE_MASK = { 0xff00ff00, 0xff00ff00, 0xff00ff00, 0xff00ff00 };

        if (count < 0 || count > 8)
            count = 8;

        if (AutoSystemInfo::Data.SSE2Available())
        {
            x86tmp1.m128i_value = _mm_and_si128(tmpaValue.m128i_value, X86_LOWBYTE_MASK.m128i_value);
            x86tmp1.m128i_value = _mm_srli_epi16(x86tmp1.m128i_value, count);

            tmpaValue.m128i_value = _mm_srli_epi16(tmpaValue.m128i_value, count);
            tmpaValue.m128i_value = _mm_and_si128(tmpaValue.m128i_value, X86_HIGHBYTE_MASK.m128i_value);

            x86Result.m128i_value = _mm_or_si128(tmpaValue.m128i_value, x86tmp1.m128i_value);

            return X86SIMDValue::ToSIMDValue(x86Result);
        }
        else
        {
            SIMDValue result;

            int nIntMin = INT_MIN; // INT_MIN = -2147483648 = 0x80000000
            int mask = ~((nIntMin >> count) << 1); // now first count bits are 0
            // right shift count bits and shift in with 0

            result.i8[7] = (value.i8[7] >> count) & mask;
            for (uint idx = 0; idx < 16; ++idx)
            {
                result.i8[idx] = (value.i8[idx] >> count) & mask;
            }

            return result;
        }
    }

    SIMDValue SIMDInt8x16Operation::OpShiftRightArithmeticByScalar(const SIMDValue& value, int8 count)
    {
        X86SIMDValue x86Result;
        X86SIMDValue tmpaValue = X86SIMDValue::ToX86SIMDValue(value);
        X86SIMDValue x86tmp1;

        const _x86_SIMDValue X86_LOWBYTE_MASK  = { 0x00ff00ff, 0x00ff00ff, 0x00ff00ff, 0x00ff00ff };
        const _x86_SIMDValue X86_HIGHBYTE_MASK = { 0xff00ff00, 0xff00ff00, 0xff00ff00, 0xff00ff00 };

        if (count < 0 || count > 8)
        {
            count = 8;
        }

        if (AutoSystemInfo::Data.SSE2Available())
        {
            x86tmp1.m128i_value = _mm_slli_epi16(tmpaValue.m128i_value, 8);
            x86tmp1.m128i_value = _mm_srai_epi16(x86tmp1.m128i_value, count + 8);

            x86tmp1.m128i_value = _mm_and_si128(x86tmp1.m128i_value, X86_LOWBYTE_MASK.m128i_value);

            tmpaValue.m128i_value = _mm_srai_epi16(tmpaValue.m128i_value, count);
            tmpaValue.m128i_value = _mm_and_si128(tmpaValue.m128i_value, X86_HIGHBYTE_MASK.m128i_value);

            x86Result.m128i_value = _mm_or_si128(tmpaValue.m128i_value, x86tmp1.m128i_value);

            return X86SIMDValue::ToSIMDValue(x86Result);
        }
        else
        {
            SIMDValue result;
            for (uint idx = 0; idx < 16; ++idx)
            {
                result.i8[idx] = value.i8[idx] >> count;
            }

            return result;
        }
    }
}
#endif
