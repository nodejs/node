//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js {

    struct SIMDInt32x4Operation
    {
        // following are operation wrappers for SIMDInt32x4 general implementation
        // input and output are typically SIMDValue
        static SIMDValue OpInt32x4(int x, int y, int z, int w);
        static SIMDValue OpInt32x4(const SIMDValue& v);

        static SIMDValue OpZero();

        static SIMDValue OpSplat(int x);
        static SIMDValue OpSplat(const SIMDValue& v);

        static SIMDValue OpBool(int x, int y, int z, int w);
        static SIMDValue OpBool(const SIMDValue& v);

        // conversion
        static SIMDValue OpFromBool(const SIMDValue& value);
        static SIMDValue OpFromFloat32x4(const SIMDValue& value);
        static SIMDValue OpFromFloat64x2(const SIMDValue& value);
        static SIMDValue OpFromFloat32x4Bits(const SIMDValue& value);
        static SIMDValue OpFromFloat64x2Bits(const SIMDValue& value);

        // Unary Ops
        static SIMDValue OpAbs(const SIMDValue& v);
        static SIMDValue OpNeg(const SIMDValue& v);
        static SIMDValue OpNot(const SIMDValue& v);

        static SIMDValue OpAdd(const SIMDValue& aValue, const SIMDValue& bValue);
        static SIMDValue OpSub(const SIMDValue& aValue, const SIMDValue& bValue);
        static SIMDValue OpMul(const SIMDValue& aValue, const SIMDValue& bValue);
        static SIMDValue OpAnd(const SIMDValue& aValue, const SIMDValue& bValue);
        static SIMDValue OpOr (const SIMDValue& aValue, const SIMDValue& bValue);
        static SIMDValue OpXor(const SIMDValue& aValue, const SIMDValue& bValue);
        static SIMDValue OpMin(const SIMDValue& aValue, const SIMDValue& bValue);
        static SIMDValue OpMax(const SIMDValue& aValue, const SIMDValue& bValue);

        static SIMDValue OpLessThan(const SIMDValue& aValue, const SIMDValue& bValue);
        static SIMDValue OpEqual(const SIMDValue& aValue, const SIMDValue& bValue);
        static SIMDValue OpGreaterThan(const SIMDValue& aValue, const SIMDValue& bValue);

        static SIMDValue OpShiftLeft(const SIMDValue& value, int count);
        static SIMDValue OpShiftRightLogical(const SIMDValue& value, int count);
        static SIMDValue OpShiftRightArithmetic(const SIMDValue& value, int count);

        static SIMDValue OpSelect(const SIMDValue& mV, const SIMDValue& tV, const SIMDValue& fV);

        // Get SignMask
        static int OpGetSignMask(const SIMDValue& mV);
    };

} // namespace Js
