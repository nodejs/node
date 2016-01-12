//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js {

    struct SIMDFloat32x4Operation
    {
        // following are operation wrappers of SIMD.Float32x4 general implementation
        static SIMDValue OpFloat32x4(float x, float y, float z, float w);
        static SIMDValue OpFloat32x4(const SIMDValue& v);

        static SIMDValue OpZero();

        static SIMDValue OpSplat(float x);
        static SIMDValue OpSplat(const SIMDValue& v);

        // conversion
        static SIMDValue OpFromFloat64x2(const SIMDValue& value);
        static SIMDValue OpFromFloat64x2Bits(const SIMDValue& value);
        static SIMDValue OpFromInt32x4(const SIMDValue& value);
        static SIMDValue OpFromInt32x4Bits(const SIMDValue& value);


        // Unary Ops
        static SIMDValue OpAbs(const SIMDValue& v);
        static SIMDValue OpNeg(const SIMDValue& v);
        static SIMDValue OpNot(const SIMDValue& v);

        static SIMDValue OpReciprocal(const SIMDValue& v);
        static SIMDValue OpReciprocalSqrt(const SIMDValue& v);
        static SIMDValue OpSqrt(const SIMDValue& v);

        // Binary Ops
        static SIMDValue OpAdd(const SIMDValue& aValue, const SIMDValue& bValue);
        static SIMDValue OpSub(const SIMDValue& aValue, const SIMDValue& bValue);
        static SIMDValue OpMul(const SIMDValue& aValue, const SIMDValue& bValue);

        static SIMDValue OpDiv(const SIMDValue& aValue, const SIMDValue& bValue);

        static SIMDValue OpAnd(const SIMDValue& aValue, const SIMDValue& bValue);
        static SIMDValue OpOr (const SIMDValue& aValue, const SIMDValue& bValue);
        static SIMDValue OpXor(const SIMDValue& aValue, const SIMDValue& bValue);
        static SIMDValue OpMin(const SIMDValue& aValue, const SIMDValue& bValue);
        static SIMDValue OpMax(const SIMDValue& aValue, const SIMDValue& bValue);

        static SIMDValue OpScale(const SIMDValue& Value, float scaleValue);

        static SIMDValue OpLessThan(const SIMDValue& aValue, const SIMDValue& bValue);
        static SIMDValue OpLessThanOrEqual(const SIMDValue& aValue, const SIMDValue& bValue);
        static SIMDValue OpEqual(const SIMDValue& aValue, const SIMDValue& bValue);
        static SIMDValue OpNotEqual(const SIMDValue& aValue, const SIMDValue& bValue);
        static SIMDValue OpGreaterThan(const SIMDValue& aValue, const SIMDValue& bValue);
        static SIMDValue OpGreaterThanOrEqual(const SIMDValue& aValue, const SIMDValue& bValue);

        static SIMDValue OpClamp(const SIMDValue& value, const SIMDValue& upper, const SIMDValue& lower);

        static SIMDValue OpSelect(const SIMDValue& mV, const SIMDValue& tV, const SIMDValue& fV);

        // Get SignMask
        static int OpGetSignMask(const SIMDValue& mV);
    };

} // namespace Js
