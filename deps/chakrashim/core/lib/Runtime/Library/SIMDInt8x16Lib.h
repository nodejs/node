//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js {

    class SIMDInt8x16Lib
    {
    public:
        class EntryInfo
        {
        public:
            static FunctionInfo Int8x16;
            static FunctionInfo Check;
            static FunctionInfo Zero;
            static FunctionInfo Splat;
            //// Conversions
            static FunctionInfo FromFloat32x4Bits;
            static FunctionInfo FromInt32x4Bits;
            //// UnaryOps
            static FunctionInfo Neg;
            static FunctionInfo Not;
            //// BinaryOps
            static FunctionInfo Add;
            static FunctionInfo Sub;
            static FunctionInfo Mul;
            static FunctionInfo And;
            static FunctionInfo Or;
            static FunctionInfo Xor;
            //// CompareOps
            static FunctionInfo LessThan;
            static FunctionInfo Equal;
            static FunctionInfo GreaterThan;
            //// ShiftOps
            static FunctionInfo ShiftLeftByScalar;
            static FunctionInfo ShiftRightLogicalByScalar;
            static FunctionInfo ShiftRightArithmeticByScalar;
            //// Others
            static FunctionInfo ExtractLane;
            static FunctionInfo ReplaceLane;
        };

        // Entry points to library
        // constructor
        static Var EntryInt8x16(RecyclableObject* function, CallInfo callInfo, ...);
        // type-check
        static Var EntryCheck(RecyclableObject* function, CallInfo callInfo, ...);

        static Var EntryZero(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntrySplat(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryFromFloat32x4Bits(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryFromInt32x4Bits(RecyclableObject* function, CallInfo callInfo, ...);

        //// UnaryOps
        static Var EntryNeg(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryNot(RecyclableObject* function, CallInfo callInfo, ...);
        //// BinaryOps
        static Var EntryAdd(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntrySub(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryMul(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryAnd(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryOr(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryXor(RecyclableObject* function, CallInfo callInfo, ...);

        //// CompareOps
        static Var EntryLessThan(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryEqual(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryGreaterThan(RecyclableObject* function, CallInfo callInfo, ...);
        //// ShiftOps
        static Var EntryShiftLeftByScalar(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryShiftRightLogicalByScalar(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryShiftRightArithmeticByScalar(RecyclableObject* function, CallInfo callInfo, ...);

        // Lane Access
        static Var EntryExtractLane(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryReplaceLane(RecyclableObject* function, CallInfo callInfo, ...);
        // End entry points

    };

} // namespace Js
