//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLanguagePch.h"

#if defined(_M_ARM32_OR_ARM64)
namespace Js
{
    SIMDValue SIMDFloat32x4Operation::OpFloat32x4(float x, float y, float z, float w)
    {
        SIMDValue result;

        result.f32[SIMD_X] = x;
        result.f32[SIMD_Y] = y;
        result.f32[SIMD_Z] = z;
        result.f32[SIMD_W] = w;

        return result;
    }

    SIMDValue SIMDFloat32x4Operation::OpFloat32x4(const SIMDValue& v)
    {
        // overload function with input paramter as SIMDValue for completeness
        SIMDValue result;

        result = v;

        return result;
    }

    SIMDValue SIMDFloat32x4Operation::OpZero()
    {
        SIMDValue result;

        result.f32[SIMD_X] = result.f32[SIMD_Y] = result.f32[SIMD_Z] = result.f32[SIMD_W] = 0;

        return result;
    }

    SIMDValue SIMDFloat32x4Operation::OpSplat(float x)
    {
        SIMDValue result;

        result.f32[SIMD_X] = result.f32[SIMD_Y] = result.f32[SIMD_Z] = result.f32[SIMD_W] = x;

        return result;
    }

    SIMDValue SIMDFloat32x4Operation::OpSplat(const SIMDValue& v)
    {
        SIMDValue result;

        result.f32[SIMD_X] = result.f32[SIMD_Y] = result.f32[SIMD_Z] = result.f32[SIMD_W] = v.f32[SIMD_X];

        return result;
    }

    // Conversions
    SIMDValue SIMDFloat32x4Operation::OpFromFloat64x2(const SIMDValue& v)
    {
        SIMDValue result;

        result.f32[SIMD_X] = (float)(v.f64[SIMD_X]);
        result.f32[SIMD_Y] = (float)(v.f64[SIMD_Y]);
        result.f32[SIMD_Z] = result.f32[SIMD_W] = 0;

        return result;
    }

    SIMDValue SIMDFloat32x4Operation::OpFromFloat64x2Bits(const SIMDValue& v)
    {
        SIMDValue result;
        result.f64[SIMD_X] = v.f64[SIMD_X];
        result.f64[SIMD_Y] = v.f64[SIMD_Y];
        return result;
    }

    SIMDValue SIMDFloat32x4Operation::OpFromInt32x4(const SIMDValue& v)
    {
        SIMDValue result;

        result.f32[SIMD_X] = (float)(v.i32[SIMD_X]);
        result.f32[SIMD_Y] = (float)(v.i32[SIMD_Y]);
        result.f32[SIMD_Z] = (float)(v.i32[SIMD_Z]);
        result.f32[SIMD_W] = (float)(v.i32[SIMD_W]);

        return result;
    }

    SIMDValue SIMDFloat32x4Operation::OpFromInt32x4Bits(const SIMDValue& v)
    {
        return OpFromFloat64x2Bits(v);
    }

    // Unary Ops
    SIMDValue SIMDFloat32x4Operation::OpAbs(const SIMDValue& value)
    {
        SIMDValue result;

        result.f32[SIMD_X] = (value.f32[SIMD_X] < 0) ? -1 * value.f32[SIMD_X] : value.f32[SIMD_X];
        result.f32[SIMD_Y] = (value.f32[SIMD_Y] < 0) ? -1 * value.f32[SIMD_Y] : value.f32[SIMD_Y];
        result.f32[SIMD_Z] = (value.f32[SIMD_Z] < 0) ? -1 * value.f32[SIMD_Z] : value.f32[SIMD_Z];
        result.f32[SIMD_W] = (value.f32[SIMD_W] < 0) ? -1 * value.f32[SIMD_W] : value.f32[SIMD_W];

        return result;
    }

    SIMDValue SIMDFloat32x4Operation::OpNeg(const SIMDValue& value)
    {
        SIMDValue result;

        result.f32[SIMD_X] = -1 * value.f32[SIMD_X];
        result.f32[SIMD_Y] = -1 * value.f32[SIMD_Y];
        result.f32[SIMD_Z] = -1 * value.f32[SIMD_Z];
        result.f32[SIMD_W] = -1 * value.f32[SIMD_W];

        return result;
    }

    SIMDValue SIMDFloat32x4Operation::OpNot(const SIMDValue& value)
    {
        SIMDValue result;

        result = SIMDInt32x4Operation::OpNot(value);

        return result;
    }

    SIMDValue SIMDFloat32x4Operation::OpReciprocal(const SIMDValue& value)
    {
        SIMDValue result;

        result.f32[SIMD_X] = (float)(1.0 / (value.f32[SIMD_X]));
        result.f32[SIMD_Y] = (float)(1.0 / (value.f32[SIMD_Y]));
        result.f32[SIMD_Z] = (float)(1.0 / (value.f32[SIMD_Z]));
        result.f32[SIMD_W] = (float)(1.0 / (value.f32[SIMD_W]));

        return result;
    }

    SIMDValue SIMDFloat32x4Operation::OpReciprocalSqrt(const SIMDValue& value)
    {
        SIMDValue result;

        result.f32[SIMD_X] = (float)sqrt(1.0 / (value.f32[SIMD_X]));
        result.f32[SIMD_Y] = (float)sqrt(1.0 / (value.f32[SIMD_Y]));
        result.f32[SIMD_Z] = (float)sqrt(1.0 / (value.f32[SIMD_Z]));
        result.f32[SIMD_W] = (float)sqrt(1.0 / (value.f32[SIMD_W]));

        return result;
    }

    SIMDValue SIMDFloat32x4Operation::OpSqrt(const SIMDValue& value)
    {
        SIMDValue result;

        result.f32[SIMD_X] = sqrtf(value.f32[SIMD_X]);
        result.f32[SIMD_Y] = sqrtf(value.f32[SIMD_Y]);
        result.f32[SIMD_Z] = sqrtf(value.f32[SIMD_Z]);
        result.f32[SIMD_W] = sqrtf(value.f32[SIMD_W]);

        return result;
    }

    // Binary Ops
    SIMDValue SIMDFloat32x4Operation::OpAdd(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        SIMDValue result;

        result.f32[SIMD_X] = aValue.f32[SIMD_X] + bValue.f32[SIMD_X];
        result.f32[SIMD_Y] = aValue.f32[SIMD_Y] + bValue.f32[SIMD_Y];
        result.f32[SIMD_Z] = aValue.f32[SIMD_Z] + bValue.f32[SIMD_Z];
        result.f32[SIMD_W] = aValue.f32[SIMD_W] + bValue.f32[SIMD_W];

        return result;
    }

    SIMDValue SIMDFloat32x4Operation::OpSub(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        SIMDValue result;

        result.f32[SIMD_X] = aValue.f32[SIMD_X] - bValue.f32[SIMD_X];
        result.f32[SIMD_Y] = aValue.f32[SIMD_Y] - bValue.f32[SIMD_Y];
        result.f32[SIMD_Z] = aValue.f32[SIMD_Z] - bValue.f32[SIMD_Z];
        result.f32[SIMD_W] = aValue.f32[SIMD_W] - bValue.f32[SIMD_W];

        return result;
    }

    SIMDValue SIMDFloat32x4Operation::OpMul(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        SIMDValue result;

        result.f32[SIMD_X] = aValue.f32[SIMD_X] * bValue.f32[SIMD_X];
        result.f32[SIMD_Y] = aValue.f32[SIMD_Y] * bValue.f32[SIMD_Y];
        result.f32[SIMD_Z] = aValue.f32[SIMD_Z] * bValue.f32[SIMD_Z];
        result.f32[SIMD_W] = aValue.f32[SIMD_W] * bValue.f32[SIMD_W];

        return result;
    }

    SIMDValue SIMDFloat32x4Operation::OpDiv(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        SIMDValue result;

        result.f32[SIMD_X] = aValue.f32[SIMD_X] / bValue.f32[SIMD_X];
        result.f32[SIMD_Y] = aValue.f32[SIMD_Y] / bValue.f32[SIMD_Y];
        result.f32[SIMD_Z] = aValue.f32[SIMD_Z] / bValue.f32[SIMD_Z];
        result.f32[SIMD_W] = aValue.f32[SIMD_W] / bValue.f32[SIMD_W];

        return result;
    }

    SIMDValue SIMDFloat32x4Operation::OpAnd(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        SIMDValue result;

        result = SIMDInt32x4Operation::OpAnd(aValue, bValue);

        return result;
    }

    SIMDValue SIMDFloat32x4Operation::OpOr(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        SIMDValue result;

        result = SIMDInt32x4Operation::OpOr(aValue, bValue);

        return result;
    }

    SIMDValue SIMDFloat32x4Operation::OpXor(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        SIMDValue result;

        result = SIMDInt32x4Operation::OpXor(aValue, bValue);

        return result;
    }

    SIMDValue SIMDFloat32x4Operation::OpMin(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        SIMDValue result;

        result.f32[SIMD_X] = (aValue.f32[SIMD_X] < bValue.f32[SIMD_X]) ? aValue.f32[SIMD_X] : bValue.f32[SIMD_X];
        result.f32[SIMD_Y] = (aValue.f32[SIMD_Y] < bValue.f32[SIMD_Y]) ? aValue.f32[SIMD_Y] : bValue.f32[SIMD_Y];
        result.f32[SIMD_Z] = (aValue.f32[SIMD_Z] < bValue.f32[SIMD_Z]) ? aValue.f32[SIMD_Z] : bValue.f32[SIMD_Z];
        result.f32[SIMD_W] = (aValue.f32[SIMD_W] < bValue.f32[SIMD_W]) ? aValue.f32[SIMD_W] : bValue.f32[SIMD_W];

        return result;
    }

    SIMDValue SIMDFloat32x4Operation::OpMax(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        SIMDValue result;

        result.f32[SIMD_X] = (aValue.f32[SIMD_X] > bValue.f32[SIMD_X]) ? aValue.f32[SIMD_X] : bValue.f32[SIMD_X];
        result.f32[SIMD_Y] = (aValue.f32[SIMD_Y] > bValue.f32[SIMD_Y]) ? aValue.f32[SIMD_Y] : bValue.f32[SIMD_Y];
        result.f32[SIMD_Z] = (aValue.f32[SIMD_Z] > bValue.f32[SIMD_Z]) ? aValue.f32[SIMD_Z] : bValue.f32[SIMD_Z];
        result.f32[SIMD_W] = (aValue.f32[SIMD_W] > bValue.f32[SIMD_W]) ? aValue.f32[SIMD_W] : bValue.f32[SIMD_W];

        return result;
    }

    SIMDValue SIMDFloat32x4Operation::OpScale(const SIMDValue& Value, float scaleValue)
    {
        SIMDValue result;

        result.f32[SIMD_X] = Value.f32[SIMD_X] * scaleValue;
        result.f32[SIMD_Y] = Value.f32[SIMD_Y] * scaleValue;
        result.f32[SIMD_Z] = Value.f32[SIMD_Z] * scaleValue;
        result.f32[SIMD_W] = Value.f32[SIMD_W] * scaleValue;

        return result;
    }

    SIMDValue SIMDFloat32x4Operation::OpLessThan(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        SIMDValue result;

        int x = aValue.f32[SIMD_X] < bValue.f32[SIMD_X];
        int y = aValue.f32[SIMD_Y] < bValue.f32[SIMD_Y];
        int z = aValue.f32[SIMD_Z] < bValue.f32[SIMD_Z];
        int w = aValue.f32[SIMD_W] < bValue.f32[SIMD_W];

        result = SIMDInt32x4Operation::OpBool(x, y, z, w);

        return result;
    }

    SIMDValue SIMDFloat32x4Operation::OpLessThanOrEqual(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        SIMDValue result;

        int x = aValue.f32[SIMD_X] <= bValue.f32[SIMD_X];
        int y = aValue.f32[SIMD_Y] <= bValue.f32[SIMD_Y];
        int z = aValue.f32[SIMD_Z] <= bValue.f32[SIMD_Z];
        int w = aValue.f32[SIMD_W] <= bValue.f32[SIMD_W];

        result = SIMDInt32x4Operation::OpBool(x, y, z, w);

        return result;
    }

    SIMDValue SIMDFloat32x4Operation::OpEqual(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        SIMDValue result;

        int x = aValue.f32[SIMD_X] == bValue.f32[SIMD_X];
        int y = aValue.f32[SIMD_Y] == bValue.f32[SIMD_Y];
        int z = aValue.f32[SIMD_Z] == bValue.f32[SIMD_Z];
        int w = aValue.f32[SIMD_W] == bValue.f32[SIMD_W];

        result = SIMDInt32x4Operation::OpBool(x, y, z, w);

        return result;
    }

    SIMDValue SIMDFloat32x4Operation::OpNotEqual(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        SIMDValue result;

        int x = aValue.f32[SIMD_X] != bValue.f32[SIMD_X];
        int y = aValue.f32[SIMD_Y] != bValue.f32[SIMD_Y];
        int z = aValue.f32[SIMD_Z] != bValue.f32[SIMD_Z];
        int w = aValue.f32[SIMD_W] != bValue.f32[SIMD_W];

        result = SIMDInt32x4Operation::OpBool(x, y, z, w);

        return result;
    }

    SIMDValue SIMDFloat32x4Operation::OpGreaterThan(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        SIMDValue result;

        int x = aValue.f32[SIMD_X] > bValue.f32[SIMD_X];
        int y = aValue.f32[SIMD_Y] > bValue.f32[SIMD_Y];
        int z = aValue.f32[SIMD_Z] > bValue.f32[SIMD_Z];
        int w = aValue.f32[SIMD_W] > bValue.f32[SIMD_W];

        result = SIMDInt32x4Operation::OpBool(x, y, z, w);

        return result;
    }

    SIMDValue SIMDFloat32x4Operation::OpGreaterThanOrEqual(const SIMDValue& aValue, const SIMDValue& bValue)
    {
        SIMDValue result;

        int x = aValue.f32[SIMD_X] >= bValue.f32[SIMD_X];
        int y = aValue.f32[SIMD_Y] >= bValue.f32[SIMD_Y];
        int z = aValue.f32[SIMD_Z] >= bValue.f32[SIMD_Z];
        int w = aValue.f32[SIMD_W] >= bValue.f32[SIMD_W];

        result = SIMDInt32x4Operation::OpBool(x, y, z, w);

        return result;
    }

    SIMDValue SIMDFloat32x4Operation::OpClamp(const SIMDValue& value, const SIMDValue& lower, const SIMDValue& upper)
    {
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
        SIMDValue result;

        SIMDValue trueResult  = SIMDInt32x4Operation::OpAnd(mV, tV);
        SIMDValue notValue    = SIMDInt32x4Operation::OpNot(mV);
        SIMDValue falseResult = SIMDInt32x4Operation::OpAnd(notValue, fV);

        result = SIMDInt32x4Operation::OpOr(trueResult, falseResult);

        return result;
    }

    // Get SignMask
    int SIMDFloat32x4Operation::OpGetSignMask(const SIMDValue& v)
    {
        int result;

        int mx = (v.f32[SIMD_X] < 0.0 || 1 / v.f32[SIMD_X] == JavascriptNumber::NEGATIVE_INFINITY) ? 1 : 0;
        int my = (v.f32[SIMD_Y] < 0.0 || 1 / v.f32[SIMD_Y] == JavascriptNumber::NEGATIVE_INFINITY) ? 1 : 0;
        int mz = (v.f32[SIMD_Z] < 0.0 || 1 / v.f32[SIMD_Z] == JavascriptNumber::NEGATIVE_INFINITY) ? 1 : 0;
        int mw = (v.f32[SIMD_W] < 0.0 || 1 / v.f32[SIMD_W] == JavascriptNumber::NEGATIVE_INFINITY) ? 1 : 0;

        result = mx | my << 1 | mz << 2 | mw << 3;

        return result;
    }

}
#endif
