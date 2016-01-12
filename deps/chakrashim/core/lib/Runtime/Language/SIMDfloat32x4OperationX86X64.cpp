//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLanguagePch.h"

#if _M_IX86 || _M_AMD64

namespace Js
{
    SIMDValue SIMDFloat32x4Operation::OpFloat32x4(float x, float y, float z, float w)
    {
        X86SIMDValue x86Result;

        // Sets the 4 single-precision, floating-point values, note order starts with W below
        x86Result.m128_value = _mm_set_ps(w, z, y, x);

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDFloat32x4Operation::OpFloat32x4(const SIMDValue& v)
    {
        X86SIMDValue x86Result;
        // Sets the 4 single-precision, floating-point values, note in revised order: W, Z, Y, X
        x86Result.m128_value = _mm_set_ps(v.f32[SIMD_W], v.f32[SIMD_Z], v.f32[SIMD_Y], v.f32[SIMD_X]);

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDFloat32x4Operation::OpZero()
    {
        X86SIMDValue x86Result;
        // Sets the 128-bit value to zero
        x86Result.m128_value = _mm_setzero_ps();

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDFloat32x4Operation::OpSplat(float x)
    {
        X86SIMDValue x86Result;
        // Sets the four single-precision, floating-point values to x
        x86Result.m128_value = _mm_set1_ps(x);

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDFloat32x4Operation::OpSplat(const SIMDValue& v)
    {
        X86SIMDValue x86Result;
        // Sets the four single-precision, floating-point values to v.f32[SIMD_X]
        x86Result.m128_value = _mm_set1_ps(v.f32[SIMD_X]);

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    // Conversions
    SIMDValue SIMDFloat32x4Operation::OpFromFloat64x2(const SIMDValue& value)
    {
        X86SIMDValue x86Result;
        X86SIMDValue v = X86SIMDValue::ToX86SIMDValue(value);

        // Converts the two double-precision, floating-point values of v.m128d_value
        // to single-precision, floating-point values.
        x86Result.m128_value = _mm_cvtpd_ps(v.m128d_value);

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDFloat32x4Operation::OpFromFloat64x2Bits(const SIMDValue& value)
    {
        X86SIMDValue x86Result;
        X86SIMDValue v = X86SIMDValue::ToX86SIMDValue(value);

        _mm_store_ps(x86Result.simdValue.f32, v.m128_value);

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDFloat32x4Operation::OpFromInt32x4(const SIMDValue& value)
    {
        X86SIMDValue x86Result;
        X86SIMDValue v = X86SIMDValue::ToX86SIMDValue(value);

        // Converts the 4 signed 32-bit integer values of v.m128i_value
        // to single-precision, floating-point values.
        x86Result.m128_value = _mm_cvtepi32_ps(v.m128i_value);

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDFloat32x4Operation::OpFromInt32x4Bits(const SIMDValue& value)
    {
        return OpFromFloat64x2Bits(value);
    }

    // Unary Ops
    SIMDValue SIMDFloat32x4Operation::OpAbs(const SIMDValue& value)
    {
        X86SIMDValue x86Result, SIGNMASK;
        X86SIMDValue v = X86SIMDValue::ToX86SIMDValue(value);

        SIGNMASK.m128_value = _mm_castsi128_ps(_mm_set1_epi32(0x7FFFFFFF));
        // bitwise AND of the 4 single - precision, floats of SIGNMASK and v
        x86Result.m128_value = _mm_and_ps(SIGNMASK.m128_value, v.m128_value);

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDFloat32x4Operation::OpNeg(const SIMDValue& value)
    {
        X86SIMDValue x86Result;

        X86SIMDValue SIGNMASK;
        X86SIMDValue v = X86SIMDValue::ToX86SIMDValue(value);

        SIGNMASK.m128_value = _mm_castsi128_ps(_mm_set1_epi32(0x80000000));
        // bitwise EXOR (exclusive-or) of the 4 single-precision, floats of value and signmask
        x86Result.m128_value = _mm_xor_ps(v.m128_value, SIGNMASK.m128_value);

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDFloat32x4Operation::OpNot(const SIMDValue& value)
    {
        X86SIMDValue x86Result;

        X86SIMDValue negativeOnes = { { -1, -1, -1, -1 } };
        X86SIMDValue v = X86SIMDValue::ToX86SIMDValue(value);
        // bitwise EXOR (exclusive-or) of the 4 single-precision, floats of value and -1
        x86Result.m128_value = _mm_xor_ps(v.m128_value, negativeOnes.m128_value);

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDFloat32x4Operation::OpReciprocal(const SIMDValue& value)
    {
        X86SIMDValue x86Result;

        X86SIMDValue floatOnes;
        X86SIMDValue v = X86SIMDValue::ToX86SIMDValue(value);

        // RCPPS is not precise. Using DIVPS
        floatOnes.m128_value = _mm_set_ps(1.0, 1.0, 1.0, 1.0);
        // Divides the four single-precision, floating-point values of 1.0 and value
        x86Result.m128_value = _mm_div_ps(floatOnes.m128_value, v.m128_value); // result = 1.0/value

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDFloat32x4Operation::OpReciprocalSqrt(const SIMDValue& value)
    {
        X86SIMDValue x86Result;

        X86SIMDValue floatOnes, temp;
        X86SIMDValue v = X86SIMDValue::ToX86SIMDValue(value);

        floatOnes.m128_value = _mm_set_ps(1.0, 1.0, 1.0, 1.0);
        temp.m128_value = _mm_div_ps(floatOnes.m128_value, v.m128_value); // temp = 1.0/value
        x86Result.m128_value = _mm_sqrt_ps(temp.m128_value);              // result = sqrt(1.0/value)

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDFloat32x4Operation::OpSqrt(const SIMDValue& value)
    {
        X86SIMDValue x86Result;

        X86SIMDValue v = X86SIMDValue::ToX86SIMDValue(value);

        x86Result.m128_value = _mm_sqrt_ps(v.m128_value);   // result = sqrt(value)

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    // Binary Ops
    SIMDValue SIMDFloat32x4Operation::OpAdd(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        X86SIMDValue x86Result;
        X86SIMDValue tmpaValue = X86SIMDValue::ToX86SIMDValue(aValue);
        X86SIMDValue tmpbValue = X86SIMDValue::ToX86SIMDValue(bValue);

        x86Result.m128_value = _mm_add_ps(tmpaValue.m128_value, tmpbValue.m128_value); // a + b

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDFloat32x4Operation::OpSub(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        X86SIMDValue x86Result;
        X86SIMDValue tmpaValue = X86SIMDValue::ToX86SIMDValue(aValue);
        X86SIMDValue tmpbValue = X86SIMDValue::ToX86SIMDValue(bValue);

        x86Result.m128_value = _mm_sub_ps(tmpaValue.m128_value, tmpbValue.m128_value);  // a - b

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDFloat32x4Operation::OpMul(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        X86SIMDValue x86Result;
        X86SIMDValue tmpaValue = X86SIMDValue::ToX86SIMDValue(aValue);
        X86SIMDValue tmpbValue = X86SIMDValue::ToX86SIMDValue(bValue);

        x86Result.m128_value = _mm_mul_ps(tmpaValue.m128_value, tmpbValue.m128_value); // a * b

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDFloat32x4Operation::OpDiv(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        X86SIMDValue x86Result;
        X86SIMDValue tmpaValue = X86SIMDValue::ToX86SIMDValue(aValue);
        X86SIMDValue tmpbValue = X86SIMDValue::ToX86SIMDValue(bValue);

        x86Result.m128_value = _mm_div_ps(tmpaValue.m128_value, tmpbValue.m128_value); // a / b

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDFloat32x4Operation::OpAnd(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        X86SIMDValue x86Result;
        X86SIMDValue tmpaValue = X86SIMDValue::ToX86SIMDValue(aValue);
        X86SIMDValue tmpbValue = X86SIMDValue::ToX86SIMDValue(bValue);

        x86Result.m128_value = _mm_and_ps(tmpaValue.m128_value, tmpbValue.m128_value); // a & b

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDFloat32x4Operation::OpOr(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        X86SIMDValue x86Result;
        X86SIMDValue tmpaValue = X86SIMDValue::ToX86SIMDValue(aValue);
        X86SIMDValue tmpbValue = X86SIMDValue::ToX86SIMDValue(bValue);

        x86Result.m128_value = _mm_or_ps(tmpaValue.m128_value, tmpbValue.m128_value); // a | b

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDFloat32x4Operation::OpXor(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        X86SIMDValue x86Result;
        X86SIMDValue tmpaValue = X86SIMDValue::ToX86SIMDValue(aValue);
        X86SIMDValue tmpbValue = X86SIMDValue::ToX86SIMDValue(bValue);

        x86Result.m128_value = _mm_xor_ps(tmpaValue.m128_value, tmpbValue.m128_value); // a ^ b

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDFloat32x4Operation::OpMin(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        X86SIMDValue x86Result;
        X86SIMDValue tmpaValue = X86SIMDValue::ToX86SIMDValue(aValue);
        X86SIMDValue tmpbValue = X86SIMDValue::ToX86SIMDValue(bValue);

        // choose the smaller value of the two parameters a and b
        x86Result.m128_value = _mm_min_ps(tmpaValue.m128_value, tmpbValue.m128_value);

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDFloat32x4Operation::OpMax(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        X86SIMDValue x86Result;
        X86SIMDValue tmpaValue = X86SIMDValue::ToX86SIMDValue(aValue);
        X86SIMDValue tmpbValue = X86SIMDValue::ToX86SIMDValue(bValue);

        // choose the larger value of the two parameters a and b
        x86Result.m128_value = _mm_max_ps(tmpaValue.m128_value, tmpbValue.m128_value);

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDFloat32x4Operation::OpScale(const SIMDValue& Value, float scaleValue)
    {
        X86SIMDValue x86Result;
        X86SIMDValue v = X86SIMDValue::ToX86SIMDValue(Value);

        X86SIMDValue scaleVector;
        scaleVector.m128_value = _mm_set1_ps(scaleValue);
        x86Result.m128_value = _mm_mul_ps(v.m128_value, scaleVector.m128_value); // v * scale

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDFloat32x4Operation::OpLessThan(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        X86SIMDValue x86Result;
        X86SIMDValue tmpaValue = X86SIMDValue::ToX86SIMDValue(aValue);
        X86SIMDValue tmpbValue = X86SIMDValue::ToX86SIMDValue(bValue);
        x86Result.m128_value = _mm_cmplt_ps(tmpaValue.m128_value, tmpbValue.m128_value); // a < b?

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDFloat32x4Operation::OpLessThanOrEqual(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        X86SIMDValue x86Result;
        X86SIMDValue tmpaValue = X86SIMDValue::ToX86SIMDValue(aValue);
        X86SIMDValue tmpbValue = X86SIMDValue::ToX86SIMDValue(bValue);
        x86Result.m128_value = _mm_cmple_ps(tmpaValue.m128_value, tmpbValue.m128_value); // a <= b?

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDFloat32x4Operation::OpEqual(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        X86SIMDValue x86Result;
        X86SIMDValue tmpaValue = X86SIMDValue::ToX86SIMDValue(aValue);
        X86SIMDValue tmpbValue = X86SIMDValue::ToX86SIMDValue(bValue);
        x86Result.m128_value = _mm_cmpeq_ps(tmpaValue.m128_value, tmpbValue.m128_value); // a == b?

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDFloat32x4Operation::OpNotEqual(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        X86SIMDValue x86Result;
        X86SIMDValue tmpaValue = X86SIMDValue::ToX86SIMDValue(aValue);
        X86SIMDValue tmpbValue = X86SIMDValue::ToX86SIMDValue(bValue);
        x86Result.m128_value = _mm_cmpneq_ps(tmpaValue.m128_value, tmpbValue.m128_value); // a != b?

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDFloat32x4Operation::OpGreaterThan(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        X86SIMDValue x86Result;
        X86SIMDValue tmpaValue = X86SIMDValue::ToX86SIMDValue(aValue);
        X86SIMDValue tmpbValue = X86SIMDValue::ToX86SIMDValue(bValue);
        x86Result.m128_value = _mm_cmpgt_ps(tmpaValue.m128_value, tmpbValue.m128_value); // a > b?

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDFloat32x4Operation::OpGreaterThanOrEqual(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        X86SIMDValue x86Result;
        X86SIMDValue tmpaValue = X86SIMDValue::ToX86SIMDValue(aValue);
        X86SIMDValue tmpbValue = X86SIMDValue::ToX86SIMDValue(bValue);
        x86Result.m128_value = _mm_cmpge_ps(tmpaValue.m128_value, tmpbValue.m128_value); // a >= b?

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDFloat32x4Operation::OpClamp(const SIMDValue& value, const SIMDValue& lower, const SIMDValue& upper)
    { // SIMD review: do we have intrinsic for the implementation?
        SIMDValue result;

        // lower clamp
        result.f32[SIMD_X] = value.f32[SIMD_X] < lower.f32[SIMD_X] ? lower.f32[SIMD_X] : value.f32[SIMD_X];
        result.f32[SIMD_Y] = value.f32[SIMD_Y] < lower.f32[SIMD_Y] ? lower.f32[SIMD_Y] : value.f32[SIMD_Y];
        result.f32[SIMD_Z] = value.f32[SIMD_Z] < lower.f32[SIMD_Z] ? lower.f32[SIMD_Z] : value.f32[SIMD_Z];
        result.f32[SIMD_W] = value.f32[SIMD_W] < lower.f32[SIMD_W] ? lower.f32[SIMD_W] : value.f32[SIMD_W];

        // upper clamp
        result.f32[SIMD_X] = result.f32[SIMD_X] > upper.f32[SIMD_X] ? upper.f32[SIMD_X] : result.f32[SIMD_X];
        result.f32[SIMD_Y] = result.f32[SIMD_Y] > upper.f32[SIMD_Y] ? upper.f32[SIMD_Y] : result.f32[SIMD_Y];
        result.f32[SIMD_Z] = result.f32[SIMD_Z] > upper.f32[SIMD_Z] ? upper.f32[SIMD_Z] : result.f32[SIMD_Z];
        result.f32[SIMD_W] = result.f32[SIMD_W] > upper.f32[SIMD_W] ? upper.f32[SIMD_W] : result.f32[SIMD_W];

        return result;
    }

    SIMDValue SIMDFloat32x4Operation::OpSelect(const SIMDValue& mV, const SIMDValue& tV, const SIMDValue& fV)
    {
        X86SIMDValue x86Result;
        X86SIMDValue maskValue  = X86SIMDValue::ToX86SIMDValue(mV);
        X86SIMDValue trueValue  = X86SIMDValue::ToX86SIMDValue(tV);
        X86SIMDValue falseValue = X86SIMDValue::ToX86SIMDValue(fV);

        X86SIMDValue tempTrue, tempFalse;
        tempTrue.m128_value = _mm_and_ps(maskValue.m128_value, trueValue.m128_value);      // mask & True
        tempFalse.m128_value = _mm_andnot_ps(maskValue.m128_value, falseValue.m128_value); // !mask & False
        x86Result.m128_value = _mm_or_ps(tempTrue.m128_value, tempFalse.m128_value); // tempTrue | tempFalse

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    // Get SignMask
    int SIMDFloat32x4Operation::OpGetSignMask(const SIMDValue& value)
    {
        X86SIMDValue v = X86SIMDValue::ToX86SIMDValue(value);

        // Creates a 4-bit mask from the most significant bits of
        // the 4 single-precision, floating-point values
        return _mm_movemask_ps(v.m128_value);
    }
}

#endif
