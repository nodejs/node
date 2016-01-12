//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js {

    struct SIMDInt8x16Operation
    {
        // following are operation wrappers for SIMDInt8x16 general implementation
        // input and output are typically SIMDValue
        static SIMDValue OpInt8x16(int8 x0, int8 x1, int8 x2, int8 x3, int8 x4, int8 x5, int8 x6, int8 x7, int8 x8, int8 x9, int8 x10, int8 x11, int8 x12, int8 x13, int8 x14, int8 x15);
        static SIMDValue OpInt8x16(const SIMDValue& v);
        static SIMDValue OpZero();

        static SIMDValue OpSplat(int8 x);
        static SIMDValue OpSplat(const SIMDValue& v);

        //// conversion
        static SIMDValue OpFromInt32x4Bits(const SIMDValue& value);
        static SIMDValue OpFromFloat32x4Bits(const SIMDValue& value);

        //// Unary Ops
        static SIMDValue OpNeg(const SIMDValue& v);
        static SIMDValue OpNot(const SIMDValue& v);

        static SIMDValue OpAdd(const SIMDValue& aValue, const SIMDValue& bValue);
        static SIMDValue OpSub(const SIMDValue& aValue, const SIMDValue& bValue);
        static SIMDValue OpMul(const SIMDValue& aValue, const SIMDValue& bValue);
        static SIMDValue OpAnd(const SIMDValue& aValue, const SIMDValue& bValue);
        static SIMDValue OpOr(const SIMDValue& aValue, const SIMDValue& bValue);
        static SIMDValue OpXor(const SIMDValue& aValue, const SIMDValue& bValue);

        static SIMDValue OpLessThan(const SIMDValue& aValue, const SIMDValue& bValue);
        static SIMDValue OpEqual(const SIMDValue& aValue, const SIMDValue& bValue);
        static SIMDValue OpGreaterThan(const SIMDValue& aValue, const SIMDValue& bValue);

        static SIMDValue OpShiftLeftByScalar(const SIMDValue& value, int8 count);
        static SIMDValue OpShiftRightLogicalByScalar(const SIMDValue& value, int8 count);
        static SIMDValue OpShiftRightArithmeticByScalar(const SIMDValue& value, int8 count);
    };
} // namespace Js
