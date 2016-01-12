//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLanguagePch.h"

#if defined(_M_ARM32_OR_ARM64)
namespace Js
{
    SIMDValue SIMDFloat64x2Operation::OpFloat64x2(double x, double y)
    {
        SIMDValue result;

        result.f64[SIMD_X] = x;
        result.f64[SIMD_Y] = y;

        return result;
    }

    SIMDValue SIMDFloat64x2Operation::OpFloat64x2(const SIMDValue& v)
    {// overload function with input parameter as SIMDValue for completeness
        SIMDValue result;

        result = v;

        return result;
    }

    SIMDValue SIMDFloat64x2Operation::OpZero()
    {
        SIMDValue result;

        result.f64[SIMD_X] = result.f64[SIMD_Y] = 0;

        return result;
    }

    SIMDValue SIMDFloat64x2Operation::OpSplat(double x)
    {
        SIMDValue result;

        result.f64[SIMD_X] = result.f64[SIMD_Y] = x;

        return result;
    }

    SIMDValue SIMDFloat64x2Operation::OpSplat(const SIMDValue& v)
    {
        SIMDValue result;

        result.f64[SIMD_X] = result.f64[SIMD_Y] = v.f64[SIMD_X];

        return result;
    }

    // Conversions
    SIMDValue SIMDFloat64x2Operation::OpFromFloat32x4(const SIMDValue& v)
    {
        SIMDValue result;

        result.f64[SIMD_X] = (double)(v.f32[SIMD_X]);
        result.f64[SIMD_Y] = (double)(v.f32[SIMD_Y]);

        return result;
    }

    SIMDValue SIMDFloat64x2Operation::OpFromInt32x4(const SIMDValue& v)
    {
        SIMDValue result;

        result.f64[SIMD_X] = (double)(v.i32[SIMD_X]);
        result.f64[SIMD_Y] = (double)(v.i32[SIMD_Y]);

        return result;
    }

    SIMDValue SIMDFloat64x2Operation::OpFromFloat32x4Bits(const SIMDValue& v)
    {
        SIMDValue result;

        result.f64[SIMD_X] = v.f64[SIMD_X];
        result.f64[SIMD_Y] = v.f64[SIMD_Y];

        return result;
    }

    SIMDValue SIMDFloat64x2Operation::OpFromInt32x4Bits(const SIMDValue& v)
    {
        return OpFromFloat32x4Bits(v);
    }

    // Unary Ops
    SIMDValue SIMDFloat64x2Operation::OpAbs(const SIMDValue& value)
    {
        SIMDValue result;

        result.f64[SIMD_X] = (value.f64[SIMD_X] < 0) ? -1 * value.f64[SIMD_X] : value.f64[SIMD_X];
        result.f64[SIMD_Y] = (value.f64[SIMD_Y] < 0) ? -1 * value.f64[SIMD_Y] : value.f64[SIMD_Y];

        return result;
    }

    SIMDValue SIMDFloat64x2Operation::OpNeg(const SIMDValue& value)
    {
        SIMDValue result;

        result.f64[SIMD_X] = -1 * value.f64[SIMD_X];
        result.f64[SIMD_Y] = -1 * value.f64[SIMD_Y];

        return result;
    }

    SIMDValue SIMDFloat64x2Operation::OpNot(const SIMDValue& value)
    {
        SIMDValue result;

        result = SIMDInt32x4Operation::OpNot(value);

        return result;
    }

    SIMDValue SIMDFloat64x2Operation::OpReciprocal(const SIMDValue& value)
    {
        SIMDValue result;

        result.f64[SIMD_X] = 1.0/(value.f64[SIMD_X]);
        result.f64[SIMD_Y] = 1.0/(value.f64[SIMD_Y]);

        return result;
    }

    SIMDValue SIMDFloat64x2Operation::OpReciprocalSqrt(const SIMDValue& value)
    {
        SIMDValue result;

        result.f64[SIMD_X] = sqrt(1.0 / (value.f64[SIMD_X]));
        result.f64[SIMD_Y] = sqrt(1.0 / (value.f64[SIMD_Y]));

        return result;
    }

    SIMDValue SIMDFloat64x2Operation::OpSqrt(const SIMDValue& value)
    {
        SIMDValue result;

        result.f64[SIMD_X] = sqrt(value.f64[SIMD_X]);
        result.f64[SIMD_Y] = sqrt(value.f64[SIMD_Y]);

        return result;
    }

    // Binary Ops
    SIMDValue SIMDFloat64x2Operation::OpAdd(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        SIMDValue result;

        result.f64[SIMD_X] = aValue.f64[SIMD_X] + bValue.f64[SIMD_X];
        result.f64[SIMD_Y] = aValue.f64[SIMD_Y] + bValue.f64[SIMD_Y];

        return result;
    }

    SIMDValue SIMDFloat64x2Operation::OpSub(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        SIMDValue result;

        result.f64[SIMD_X] = aValue.f64[SIMD_X] - bValue.f64[SIMD_X];
        result.f64[SIMD_Y] = aValue.f64[SIMD_Y] - bValue.f64[SIMD_Y];

        return result;
    }

    SIMDValue SIMDFloat64x2Operation::OpMul(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        SIMDValue result;

        result.f64[SIMD_X] = aValue.f64[SIMD_X] * bValue.f64[SIMD_X];
        result.f64[SIMD_Y] = aValue.f64[SIMD_Y] * bValue.f64[SIMD_Y];

        return result;
    }

    SIMDValue SIMDFloat64x2Operation::OpDiv(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        SIMDValue result;

        result.f64[SIMD_X] = aValue.f64[SIMD_X] / bValue.f64[SIMD_X];
        result.f64[SIMD_Y] = aValue.f64[SIMD_Y] / bValue.f64[SIMD_Y];

        return result;
    }

    SIMDValue SIMDFloat64x2Operation::OpAnd(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        SIMDValue result;

        result = SIMDInt32x4Operation::OpAnd(aValue, bValue);

        return result;
    }

    SIMDValue SIMDFloat64x2Operation::OpOr(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        SIMDValue result;

        result = SIMDInt32x4Operation::OpOr(aValue, bValue);

        return result;
    }

    SIMDValue SIMDFloat64x2Operation::OpXor(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        SIMDValue result;

        result = SIMDInt32x4Operation::OpXor(aValue, bValue);

        return result;
    }

    SIMDValue SIMDFloat64x2Operation::OpMin(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        SIMDValue result;

        result.f64[SIMD_X] = (aValue.f64[SIMD_X] < bValue.f64[SIMD_X]) ? aValue.f64[SIMD_X] : bValue.f64[SIMD_X];
        result.f64[SIMD_Y] = (aValue.f64[SIMD_Y] < bValue.f64[SIMD_Y]) ? aValue.f64[SIMD_Y] : bValue.f64[SIMD_Y];

        return result;
    }

    SIMDValue SIMDFloat64x2Operation::OpMax(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        SIMDValue result;

        result.f64[SIMD_X] = (aValue.f64[SIMD_X] > bValue.f64[SIMD_X]) ? aValue.f64[SIMD_X] : bValue.f64[SIMD_X];
        result.f64[SIMD_Y] = (aValue.f64[SIMD_Y] > bValue.f64[SIMD_Y]) ? aValue.f64[SIMD_Y] : bValue.f64[SIMD_Y];

        return result;
    }

    SIMDValue SIMDFloat64x2Operation::OpScale(const SIMDValue& Value, double scaleValue)
    {
        SIMDValue result;

        result.f64[SIMD_X] = Value.f64[SIMD_X] * scaleValue;
        result.f64[SIMD_Y] = Value.f64[SIMD_Y] * scaleValue;

        return result;
    }

    SIMDValue SIMDFloat64x2Operation::OpLessThan(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        SIMDValue result;

        int x = aValue.f64[SIMD_X] < bValue.f64[SIMD_X];
        int y = aValue.f64[SIMD_Y] < bValue.f64[SIMD_Y];

        result = SIMDInt32x4Operation::OpBool(x, x, y, y);

        return result;
    }

    SIMDValue SIMDFloat64x2Operation::OpLessThanOrEqual(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        SIMDValue result;

        int x = aValue.f64[SIMD_X] <= bValue.f64[SIMD_X];
        int y = aValue.f64[SIMD_Y] <= bValue.f64[SIMD_Y];

        result = SIMDInt32x4Operation::OpBool(x, x, y, y);

        return result;
    }

    SIMDValue SIMDFloat64x2Operation::OpEqual(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        SIMDValue result;

        int x = aValue.f64[SIMD_X] == bValue.f64[SIMD_X];
        int y = aValue.f64[SIMD_Y] == bValue.f64[SIMD_Y];

        result = SIMDInt32x4Operation::OpBool(x, x, y, y);

        return result;
    }

    SIMDValue SIMDFloat64x2Operation::OpNotEqual(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        SIMDValue result;

        int x = aValue.f64[SIMD_X] != bValue.f64[SIMD_X];
        int y = aValue.f64[SIMD_Y] != bValue.f64[SIMD_Y];

        result = SIMDInt32x4Operation::OpBool(x, x, y, y);

        return result;
    }


    SIMDValue SIMDFloat64x2Operation::OpGreaterThan(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        SIMDValue result;

        int x = aValue.f64[SIMD_X] > bValue.f64[SIMD_X];
        int y = aValue.f64[SIMD_Y] > bValue.f64[SIMD_Y];

        result = SIMDInt32x4Operation::OpBool(x, x, y, y);

        return result;
    }

    SIMDValue SIMDFloat64x2Operation::OpGreaterThanOrEqual(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        SIMDValue result;

        int x = aValue.f64[SIMD_X] >= bValue.f64[SIMD_X];
        int y = aValue.f64[SIMD_Y] >= bValue.f64[SIMD_Y];

        result = SIMDInt32x4Operation::OpBool(x, x, y, y);

        return result;
    }

    SIMDValue SIMDFloat64x2Operation::OpClamp(const SIMDValue& value, const SIMDValue& lower, const SIMDValue& upper)
    {
        SIMDValue result;

        // lower clamp
        result.f64[SIMD_X] = value.f64[SIMD_X] < lower.f64[SIMD_X] ? lower.f64[SIMD_X] : value.f64[SIMD_X];
        result.f64[SIMD_Y] = value.f64[SIMD_Y] < lower.f64[SIMD_Y] ? lower.f64[SIMD_Y] : value.f64[SIMD_Y];

        // upper clamp
        result.f64[SIMD_X] = result.f64[SIMD_X] > upper.f64[SIMD_X] ? upper.f64[SIMD_X] : result.f64[SIMD_X];
        result.f64[SIMD_Y] = result.f64[SIMD_Y] > upper.f64[SIMD_Y] ? upper.f64[SIMD_Y] : result.f64[SIMD_Y];

        return result;
    }

    SIMDValue SIMDFloat64x2Operation::OpSelect(const SIMDValue& mV, const SIMDValue& tV, const SIMDValue& fV)
    {
        SIMDValue result;

        SIMDValue trueResult  = SIMDInt32x4Operation::OpAnd(mV, tV);
        SIMDValue notValue    = SIMDInt32x4Operation::OpNot(mV);
        SIMDValue falseResult = SIMDInt32x4Operation::OpAnd(notValue, fV);

        result = SIMDInt32x4Operation::OpOr(trueResult, falseResult);

        return result;
    }

    // Get SignMask
    int SIMDFloat64x2Operation::OpGetSignMask(const SIMDValue& v)
    {
        int result;

        int mx = (v.f64[SIMD_X] < 0.0 || 1 / v.f64[SIMD_X] == JavascriptNumber::NEGATIVE_INFINITY) ? 1 : 0;
        int my = (v.f64[SIMD_Y] < 0.0 || 1 / v.f64[SIMD_Y] == JavascriptNumber::NEGATIVE_INFINITY) ? 1 : 0;

        result = mx | my << 1;

        return result;
    }
}
#endif
