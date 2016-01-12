//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLanguagePch.h"

#if _M_IX86 || _M_AMD64

namespace Js
{
    // SIMD.Int32x4 operation wrappers that cover intrinsics for x86/x64 system
    SIMDValue SIMDInt32x4Operation::OpInt32x4(int x, int y, int z, int w)
    {
        X86SIMDValue x86Result;
        x86Result.m128i_value = _mm_set_epi32(w, z, y, x);
        // Sets the 4 signed 32-bit integer values, note in revised order: starts with W below

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDInt32x4Operation::OpInt32x4(const SIMDValue& v)
    {
        X86SIMDValue x86Result;
        // Sets the 4 signed 32-bit integer values, note in revised order: starts with W below
        x86Result.m128i_value = _mm_set_epi32(v.i32[SIMD_W], v.i32[SIMD_Z], v.i32[SIMD_Y], v.i32[SIMD_X]);

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDInt32x4Operation::OpZero()
    {
        X86SIMDValue x86Result;
        // Sets the 128-bit value to zero
        x86Result.m128i_value = _mm_setzero_si128();

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDInt32x4Operation::OpSplat(int x)
    {
        X86SIMDValue x86Result;
        // set 4 signed 32-bit integers values to input value x
        x86Result.m128i_value = _mm_set1_epi32(x);

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDInt32x4Operation::OpSplat(const SIMDValue& v)
    {
        X86SIMDValue x86Result;
        // set 4 signed 32-bit integers values to input value(v.i32[SIMD_X])
        x86Result.m128i_value = _mm_set1_epi32(v.i32[SIMD_X]);

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDInt32x4Operation::OpBool(int x, int y, int z, int w)
    {
        X86SIMDValue x86Result;
        x86Result.m128i_value = _mm_set_epi32(w, z, y, x); // Sets the 4 signed 32-bit int value

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDInt32x4Operation::OpBool(const SIMDValue& v)
    {
        X86SIMDValue x86Result;
        // Sets the 4 signed 32-bit integer value
        x86Result.m128i_value = _mm_set_epi32(v.i32[SIMD_W], v.i32[SIMD_Z], v.i32[SIMD_Y], v.i32[SIMD_X]);

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    // Conversions
    SIMDValue SIMDInt32x4Operation::OpFromBool(const SIMDValue& value)
    {
        X86SIMDValue x86Result;

        // assuming incoming value is any number, construct a new instance of SIMD.int32x4 with
        // 0xFFFFFFFF or 0x0 in each lane
        int x = value.i32[SIMD_X] ? 0xFFFFFFFF : 0x0;
        int y = value.i32[SIMD_Y] ? 0xFFFFFFFF : 0x0;
        int z = value.i32[SIMD_Z] ? 0xFFFFFFFF : 0x0;
        int w = value.i32[SIMD_W] ? 0xFFFFFFFF : 0x0;
        // Sets the 4 signed 32-bit integer value, starts with W
        x86Result.m128i_value = _mm_set_epi32(w, z, y, x);

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDInt32x4Operation::OpFromFloat32x4(const SIMDValue& value)
    {
        X86SIMDValue x86Result;
        X86SIMDValue v = X86SIMDValue::ToX86SIMDValue(value);

        // Converts the 4 single-precision, floating-point values to signed 32-bit integer values
        // using truncate, using truncate one instead of _mm_cvtps_epi32
        x86Result.m128i_value = _mm_cvttps_epi32(v.m128_value);

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDInt32x4Operation::OpFromFloat64x2(const SIMDValue& value)
    {
        X86SIMDValue x86Result;
        X86SIMDValue v = X86SIMDValue::ToX86SIMDValue(value);

        // Converts the 2 double-precision, floating-point values to 32-bit signed integers
        // using truncate. using truncate one instead of _mm_cvtpd_epi32
        x86Result.m128i_value = _mm_cvttpd_epi32(v.m128d_value);

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDInt32x4Operation::OpFromFloat32x4Bits(const SIMDValue& value)
    {
        X86SIMDValue x86Result;
        X86SIMDValue v = X86SIMDValue::ToX86SIMDValue(value);

        _mm_store_ps(x86Result.simdValue.f32, v.m128_value);

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDInt32x4Operation::OpFromFloat64x2Bits(const SIMDValue& value)
    {
        return OpFromFloat32x4Bits(value);
    }

    // Unary Ops
    SIMDValue SIMDInt32x4Operation::OpAbs(const SIMDValue& value)
    {
        SIMDValue result;

        X86SIMDValue x86Result;
        X86SIMDValue v = X86SIMDValue::ToX86SIMDValue(value);
        if (AutoSystemInfo::Data.SSE3Available())
        {
            x86Result.m128i_value = _mm_abs_epi32(v.m128i_value); // only available after SSE3
            result = X86SIMDValue::ToSIMDValue(x86Result);
        }
        else if (AutoSystemInfo::Data.SSE2Available())
        {
            X86SIMDValue  temp, SIGNMASK;
            SIGNMASK.m128i_value = _mm_srai_epi32(v.m128i_value, 31);                // mask = value >> 31
            temp.m128i_value = _mm_xor_si128(v.m128i_value, SIGNMASK.m128i_value);   // temp = value ^ mask
            x86Result.m128i_value = _mm_sub_epi32(temp.m128i_value, SIGNMASK.m128i_value);  // temp - mask
            result = X86SIMDValue::ToSIMDValue(x86Result);
        }
        else
        {
            result.i32[SIMD_X] = (value.i32[SIMD_X] < 0) ? -1 * value.i32[SIMD_X] : value.i32[SIMD_X];
            result.i32[SIMD_Y] = (value.i32[SIMD_Y] < 0) ? -1 * value.i32[SIMD_Y] : value.i32[SIMD_Y];
            result.i32[SIMD_Z] = (value.i32[SIMD_Z] < 0) ? -1 * value.i32[SIMD_Z] : value.i32[SIMD_Z];
            result.i32[SIMD_W] = (value.i32[SIMD_W] < 0) ? -1 * value.i32[SIMD_W] : value.i32[SIMD_W];
        }

        return result;
    }

    SIMDValue SIMDInt32x4Operation::OpNeg(const SIMDValue& value)
    {
        X86SIMDValue x86Result;

        X86SIMDValue SIGNMASK, temp;
        X86SIMDValue negativeOnes = { { -1, -1, -1, -1 } };
        X86SIMDValue v = X86SIMDValue::ToX86SIMDValue(value);

        temp.m128i_value = _mm_andnot_si128(v.m128i_value, negativeOnes.m128i_value); // (~value) & (negative ones)
        SIGNMASK.m128i_value = _mm_set1_epi32(0x00000001);                            // set SIGNMASK to 1
        x86Result.m128i_value = _mm_add_epi32(SIGNMASK.m128i_value, temp.m128i_value);// add 4 integers respectively

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDInt32x4Operation::OpNot(const SIMDValue& value)
    {
        X86SIMDValue x86Result;

        X86SIMDValue negativeOnes = { { -1, -1, -1, -1 } };
        X86SIMDValue temp = X86SIMDValue::ToX86SIMDValue(value);
        x86Result.m128i_value = _mm_andnot_si128(temp.m128i_value, negativeOnes.m128i_value);

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDInt32x4Operation::OpAdd(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        X86SIMDValue x86Result;
        X86SIMDValue tmpaValue = X86SIMDValue::ToX86SIMDValue(aValue);
        X86SIMDValue tmpbValue = X86SIMDValue::ToX86SIMDValue(bValue);

        x86Result.m128i_value = _mm_add_epi32(tmpaValue.m128i_value, tmpbValue.m128i_value); // a + b

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDInt32x4Operation::OpSub(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        X86SIMDValue x86Result;
        X86SIMDValue tmpaValue = X86SIMDValue::ToX86SIMDValue(aValue);
        X86SIMDValue tmpbValue = X86SIMDValue::ToX86SIMDValue(bValue);

        x86Result.m128i_value = _mm_sub_epi32(tmpaValue.m128i_value, tmpbValue.m128i_value); // a - b

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDInt32x4Operation::OpMul(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        SIMDValue result;
        X86SIMDValue x86Result;
        X86SIMDValue tmpaValue = X86SIMDValue::ToX86SIMDValue(aValue);
        X86SIMDValue tmpbValue = X86SIMDValue::ToX86SIMDValue(bValue);

        if (AutoSystemInfo::Data.SSE4_1Available())
        {   // a * b, only available in SSE4
            x86Result.m128i_value = _mm_mullo_epi32(tmpaValue.m128i_value, tmpbValue.m128i_value);
            result = X86SIMDValue::ToSIMDValue(x86Result);
        }
        else if (AutoSystemInfo::Data.SSE2Available())
        {
            // mul 2,0: r0 = a0*b0; r1 = a2*b2
            __m128i tmp1 = _mm_mul_epu32(tmpaValue.m128i_value, tmpbValue.m128i_value);
            // mul 3,1: r0=a1*b1; r1=a3*b3
            __m128i tmp2 = _mm_mul_epu32(_mm_srli_si128(tmpaValue.m128i_value, 4), _mm_srli_si128(tmpbValue.m128i_value, 4));
            // shuffle x86Results to [63..0] and pack
            x86Result.m128i_value = _mm_unpacklo_epi32(_mm_shuffle_epi32(tmp1, _MM_SHUFFLE(0, 0, 2, 0)), _mm_shuffle_epi32(tmp2, _MM_SHUFFLE(0, 0, 2, 0)));
            result = X86SIMDValue::ToSIMDValue(x86Result);
        }
        else
        {
            result.i32[SIMD_X] = aValue.i32[SIMD_X] * bValue.i32[SIMD_X];
            result.i32[SIMD_Y] = aValue.i32[SIMD_Y] * bValue.i32[SIMD_Y];
            result.i32[SIMD_Z] = aValue.i32[SIMD_Z] * bValue.i32[SIMD_Z];
            result.i32[SIMD_W] = aValue.i32[SIMD_W] * bValue.i32[SIMD_W];
        }

        return result;
    }

    SIMDValue SIMDInt32x4Operation::OpAnd(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        X86SIMDValue x86Result;
        X86SIMDValue tmpaValue = X86SIMDValue::ToX86SIMDValue(aValue);
        X86SIMDValue tmpbValue = X86SIMDValue::ToX86SIMDValue(bValue);

        x86Result.m128i_value = _mm_and_si128(tmpaValue.m128i_value, tmpbValue.m128i_value); // a & b

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDInt32x4Operation::OpOr(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        X86SIMDValue x86Result;
        X86SIMDValue tmpaValue = X86SIMDValue::ToX86SIMDValue(aValue);
        X86SIMDValue tmpbValue = X86SIMDValue::ToX86SIMDValue(bValue);

        x86Result.m128i_value = _mm_or_si128(tmpaValue.m128i_value, tmpbValue.m128i_value); // a | b

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDInt32x4Operation::OpXor(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        X86SIMDValue x86Result;
        X86SIMDValue tmpaValue = X86SIMDValue::ToX86SIMDValue(aValue);
        X86SIMDValue tmpbValue = X86SIMDValue::ToX86SIMDValue(bValue);

        x86Result.m128i_value = _mm_xor_si128(tmpaValue.m128i_value, tmpbValue.m128i_value); // a ^ b

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDInt32x4Operation::OpMin(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        SIMDValue result;
        X86SIMDValue x86Result;
        X86SIMDValue tmpaValue = X86SIMDValue::ToX86SIMDValue(aValue);
        X86SIMDValue tmpbValue = X86SIMDValue::ToX86SIMDValue(bValue);

        if (AutoSystemInfo::Data.SSE4_1Available())
        {   // choose the smaller value of the two parameters, only available after SSE4
            x86Result.m128i_value = _mm_min_epi32(tmpaValue.m128i_value, tmpbValue.m128i_value);
            result = X86SIMDValue::ToSIMDValue(x86Result);
        }
        else
        {
            result.i32[SIMD_X] = (aValue.i32[SIMD_X] < bValue.i32[SIMD_X]) ? aValue.i32[SIMD_X] : bValue.i32[SIMD_X];
            result.i32[SIMD_Y] = (aValue.i32[SIMD_Y] < bValue.i32[SIMD_Y]) ? aValue.i32[SIMD_Y] : bValue.i32[SIMD_Y];
            result.i32[SIMD_Z] = (aValue.i32[SIMD_Z] < bValue.i32[SIMD_Z]) ? aValue.i32[SIMD_Z] : bValue.i32[SIMD_Z];
            result.i32[SIMD_W] = (aValue.i32[SIMD_W] < bValue.i32[SIMD_W]) ? aValue.i32[SIMD_W] : bValue.i32[SIMD_W];
        }

        return result;
    }

    SIMDValue SIMDInt32x4Operation::OpMax(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        SIMDValue result;
        X86SIMDValue x86Result;
        X86SIMDValue tmpaValue = X86SIMDValue::ToX86SIMDValue(aValue);
        X86SIMDValue tmpbValue = X86SIMDValue::ToX86SIMDValue(bValue);

        if (AutoSystemInfo::Data.SSE4_1Available())
        {   // choose the larger value of the two parameters, only available after SSE4
            x86Result.m128i_value = _mm_max_epi32(tmpaValue.m128i_value, tmpbValue.m128i_value); // a ^ b
            result = X86SIMDValue::ToSIMDValue(x86Result);
        }
        else
        {
            result.i32[SIMD_X] = (aValue.i32[SIMD_X] > bValue.i32[SIMD_X]) ? aValue.i32[SIMD_X] : bValue.i32[SIMD_X];
            result.i32[SIMD_Y] = (aValue.i32[SIMD_Y] > bValue.i32[SIMD_Y]) ? aValue.i32[SIMD_Y] : bValue.i32[SIMD_Y];
            result.i32[SIMD_Z] = (aValue.i32[SIMD_Z] > bValue.i32[SIMD_Z]) ? aValue.i32[SIMD_Z] : bValue.i32[SIMD_Z];
            result.i32[SIMD_W] = (aValue.i32[SIMD_W] > bValue.i32[SIMD_W]) ? aValue.i32[SIMD_W] : bValue.i32[SIMD_W];
        }

        return result;
    }

    SIMDValue SIMDInt32x4Operation::OpLessThan(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        X86SIMDValue x86Result;
        X86SIMDValue tmpaValue = X86SIMDValue::ToX86SIMDValue(aValue);
        X86SIMDValue tmpbValue = X86SIMDValue::ToX86SIMDValue(bValue);
        x86Result.m128i_value = _mm_cmplt_epi32(tmpaValue.m128i_value, tmpbValue.m128i_value); // compare a < b?

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDInt32x4Operation::OpEqual(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        X86SIMDValue x86Result;
        X86SIMDValue tmpaValue = X86SIMDValue::ToX86SIMDValue(aValue);
        X86SIMDValue tmpbValue = X86SIMDValue::ToX86SIMDValue(bValue);
        x86Result.m128i_value = _mm_cmpeq_epi32(tmpaValue.m128i_value, tmpbValue.m128i_value); // compare a == b?

        return X86SIMDValue::ToSIMDValue(x86Result);
    }


    SIMDValue SIMDInt32x4Operation::OpGreaterThan(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        X86SIMDValue x86Result;
        X86SIMDValue tmpaValue = X86SIMDValue::ToX86SIMDValue(aValue);
        X86SIMDValue tmpbValue = X86SIMDValue::ToX86SIMDValue(bValue);
        x86Result.m128i_value = _mm_cmpgt_epi32(tmpaValue.m128i_value, tmpbValue.m128i_value); // compare a > b?

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDInt32x4Operation::OpShiftLeft(const SIMDValue& value, int count)
    {
        X86SIMDValue x86Result;
        X86SIMDValue tmpValue = X86SIMDValue::ToX86SIMDValue(value);
        // Shifts the 4 signed 32-bit integers in a left by count bits while shifting in zeros
        x86Result.m128i_value = _mm_slli_epi32(tmpValue.m128i_value, count);

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDInt32x4Operation::OpShiftRightLogical(const SIMDValue& value, int count)
    {
        X86SIMDValue x86Result;
        X86SIMDValue tmpValue = X86SIMDValue::ToX86SIMDValue(value);
        // Shifts the 4 signed 32-bit integers in a right by count bits while shifting in zeros
        x86Result.m128i_value = _mm_srli_epi32(tmpValue.m128i_value, count);

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDInt32x4Operation::OpShiftRightArithmetic(const SIMDValue& value, int count)
    {
        X86SIMDValue x86Result;
        X86SIMDValue tmpValue = X86SIMDValue::ToX86SIMDValue(value);
        // Shifts the 4 signed 32-bit integers right by count bits while shifting in the sign bit
        x86Result.m128i_value = _mm_srai_epi32(tmpValue.m128i_value, count);

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDInt32x4Operation::OpSelect(const SIMDValue& mV, const SIMDValue& tV, const SIMDValue& fV)
    {
        X86SIMDValue x86Result;
        X86SIMDValue maskValue  = X86SIMDValue::ToX86SIMDValue(mV);
        X86SIMDValue trueValue  = X86SIMDValue::ToX86SIMDValue(tV);
        X86SIMDValue falseValue = X86SIMDValue::ToX86SIMDValue(fV);

        X86SIMDValue tempTrue, tempFalse;
        tempTrue.m128i_value = _mm_and_si128(maskValue.m128i_value, trueValue.m128i_value); // mask & T
        tempFalse.m128i_value = _mm_andnot_si128(maskValue.m128i_value, falseValue.m128i_value); //!mask & F
        x86Result.m128i_value = _mm_or_si128(tempTrue.m128i_value, tempFalse.m128i_value);  // tempT | temp F

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    // Get SignMask
    int SIMDInt32x4Operation::OpGetSignMask(const SIMDValue& value)
    {
        X86SIMDValue v = X86SIMDValue::ToX86SIMDValue(value);

        // Creates a 4-bit mask from the most significant bits of
        // the 4 single-precision, floating-point values
        // SIMD review: no suitable integer intrinsics, the float version seems working fine
        return _mm_movemask_ps(v.m128_value);
    }
}
#endif
