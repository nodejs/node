//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#if ENABLE_NATIVE_CODEGEN
namespace Js {

    class SIMDInt32x4Lib
    {
    public:
        class EntryInfo
        {
        public:
            static FunctionInfo Int32x4;
            static FunctionInfo Check;
            static FunctionInfo Zero;
            static FunctionInfo Splat;
            static FunctionInfo Bool;
            // Conversions
            static FunctionInfo FromBool;
            static FunctionInfo FromFloat64x2;
            static FunctionInfo FromFloat64x2Bits;
            static FunctionInfo FromFloat32x4;
            static FunctionInfo FromFloat32x4Bits;
            // UnaryOps
            static FunctionInfo Abs;
            static FunctionInfo Neg;
            static FunctionInfo Not;
            // BinaryOps
            static FunctionInfo Add;
            static FunctionInfo Sub;
            static FunctionInfo Mul;
            static FunctionInfo And;
            static FunctionInfo Or;
            static FunctionInfo Xor;
            static FunctionInfo Min;
            static FunctionInfo Max;
            // CompareOps
            static FunctionInfo LessThan;
            static FunctionInfo Equal;
            static FunctionInfo GreaterThan;

            // WithFlagX/Y/Z/W
            static FunctionInfo WithFlagX;
            static FunctionInfo WithFlagY;
            static FunctionInfo WithFlagZ;
            static FunctionInfo WithFlagW;
            //Lane access
            static FunctionInfo ExtractLane;
            static FunctionInfo ReplaceLane;
            // ShiftOps
            static FunctionInfo ShiftLeft;
            static FunctionInfo ShiftRightLogical;
            static FunctionInfo ShiftRightArithmetic;
            // Others
            static FunctionInfo Swizzle;
            static FunctionInfo Shuffle;
            static FunctionInfo Select;

            static FunctionInfo Load;
            static FunctionInfo Load1;
            static FunctionInfo Load2;
            static FunctionInfo Load3;

            static FunctionInfo Store;
            static FunctionInfo Store1;
            static FunctionInfo Store2;
            static FunctionInfo Store3;
        };

        // Entry points to library
        // constructor
        static Var EntryInt32x4(RecyclableObject* function, CallInfo callInfo, ...);
        // type-check
        static Var EntryCheck(RecyclableObject* function, CallInfo callInfo, ...);

        static Var EntryZero(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntrySplat(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryBool(RecyclableObject* function, CallInfo callInfo, ...);

        static Var EntryFromFloat64x2(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryFromFloat64x2Bits(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryFromFloat32x4(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryFromFloat32x4Bits(RecyclableObject* function, CallInfo callInfo, ...);

        // UnaryOps
        static Var EntryAbs(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryNeg(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryNot(RecyclableObject* function, CallInfo callInfo, ...);
        // BinaryOps
        static Var EntryAdd(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntrySub(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryMul(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryAnd(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryOr(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryXor(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryMin(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryMax(RecyclableObject* function, CallInfo callInfo, ...);
        // CompareOps
        static Var EntryLessThan(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryEqual(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryGreaterThan(RecyclableObject* function, CallInfo callInfo, ...);
        // WithFlagX/Y/Z/W
        static Var EntryWithFlagX(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryWithFlagY(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryWithFlagZ(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryWithFlagW(RecyclableObject* function, CallInfo callInfo, ...);
        // Lane Access
        static Var EntryExtractLane(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryReplaceLane(RecyclableObject* function, CallInfo callInfo, ...);
        // ShiftOps
        static Var EntryShiftLeft(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryShiftRightLogical(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryShiftRightArithmetic(RecyclableObject* function, CallInfo callInfo, ...);
        // Others
        static Var EntrySwizzle(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryShuffle(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntrySelect(RecyclableObject* function, CallInfo callInfo, ...);

        static Var EntryLoad(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryLoad1(RecyclableObject* function, CallInfo callInfo, ...); // load X
        static Var EntryLoad2(RecyclableObject* function, CallInfo callInfo, ...); // load XY
        static Var EntryLoad3(RecyclableObject* function, CallInfo callInfo, ...); // load XYZ

        static Var EntryStore(RecyclableObject* function, CallInfo callInfo, ...);
        static Var EntryStore1(RecyclableObject* function, CallInfo callInfo, ...); // store X
        static Var EntryStore2(RecyclableObject* function, CallInfo callInfo, ...); // store XY
        static Var EntryStore3(RecyclableObject* function, CallInfo callInfo, ...); // store XYZ
        // End entry points

    };

} // namespace Js
#endif
