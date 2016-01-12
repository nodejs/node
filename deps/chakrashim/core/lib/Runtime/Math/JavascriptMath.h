//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
namespace Js
{
#ifdef SSE2MATH
    namespace SSE2
    {
#endif
        class JavascriptMath
        {
        public:
            //
            // Some frequently-used operations have three flavors optimized for different situations.
            //
            // 1. Op      : Called from the interpreter loop only. Must handle all cases (may call Op_Full)
            // 2. Op_Full : Called from generated code or from Op (case 1). TaggedInt cases already handled.
            // 3. Op_InPlace : Called from generated code only where result may be "in-place new'd"
            //
            static Var Increment(Var aRight,ScriptContext* scriptContext);
            static Var Increment_Full(Var aRight,ScriptContext* scriptContext);
            static Var Increment_InPlace(Var aRight,ScriptContext* scriptContext, JavascriptNumber* result);

            static Var Decrement(Var aRight,ScriptContext* scriptContext);
            static Var Decrement_Full(Var aRight,ScriptContext* scriptContext);
            static Var Decrement_InPlace(Var aRight,ScriptContext* scriptContext, JavascriptNumber* result);

            static Var Negate(Var aRight,ScriptContext* scriptContext);
            static Var Negate_Full(Var aRight,ScriptContext* scriptContext);
            static Var Negate_InPlace(Var aRight,ScriptContext* scriptContext, JavascriptNumber* result);

            static Var Not(Var aRight,ScriptContext* scriptContext);
            static Var Not_Full(Var aRight,ScriptContext* scriptContext);
            static Var Not_InPlace(Var aRight,ScriptContext* scriptContext, JavascriptNumber* result);

            static Var Add(Var aLeft, Var aRight,ScriptContext* scriptContext);
            static Var Add_Full(Var aLeft, Var aRight,ScriptContext* scriptContext);
            static Var Add_InPlace(Var aLeft, Var aRight,ScriptContext* scriptContext, JavascriptNumber *result);
            static Var AddLeftDead(Var aLeft, Var aRight,ScriptContext* scriptContext, JavascriptNumber *result);

            static Var MulAddLeft(Var mulLeft, Var mulRight, Var addLeft, ScriptContext* scriptContext, JavascriptNumber* result);
            static Var MulAddRight(Var mulLeft, Var mulRight, Var addRight, ScriptContext* scriptContext, JavascriptNumber* result);
            static Var MulSubLeft(Var mulLeft, Var mulRight, Var subLeft, ScriptContext* scriptContext, JavascriptNumber* result);
            static Var MulSubRight(Var mulLeft, Var mulRight, Var subRight, ScriptContext* scriptContext, JavascriptNumber* result);

            static Var Subtract(Var aLeft, Var aRight,ScriptContext* scriptContext);
            static Var Subtract_Full(Var aLeft, Var aRight,ScriptContext* scriptContext);
            static Var Subtract_InPlace(Var aLeft, Var aRight,ScriptContext* scriptContext, JavascriptNumber *result);

            static Var Multiply(Var aLeft, Var aRight,ScriptContext* scriptContext);
            static Var Multiply_Full(Var aLeft, Var aRight,ScriptContext* scriptContext);
            static Var Multiply_InPlace(Var aLeft, Var aRight,ScriptContext* scriptContext, JavascriptNumber *result);

            static Var Divide(Var aLeft, Var aRight,ScriptContext* scriptContext);
            static Var Divide_Full(Var aLeft, Var aRight,ScriptContext* scriptContext);
            static Var Divide_InPlace(Var aLeft, Var aRight,ScriptContext* scriptContext, JavascriptNumber *result);

            static Var Exponentiation(Var aLeft, Var aRight, ScriptContext* scriptContext);
            static Var Exponentiation_Full(Var aLeft, Var aRight, ScriptContext* scriptContext);
            static Var Exponentiation_InPlace(Var aLeft, Var aRight, ScriptContext* scriptContext, JavascriptNumber *result);

            static Var Modulus(Var aLeft, Var aRight,ScriptContext* scriptContext);
            static Var Modulus_Full(Var aLeft, Var aRight,ScriptContext* scriptContext);
            static Var Modulus_InPlace(Var aLeft, Var aRight,ScriptContext* scriptContext, JavascriptNumber *result);

            static Var And(Var aLeft, Var aRight,ScriptContext* scriptContext);
            static Var And_Full(Var aLeft, Var aRight,ScriptContext* scriptContext);
            static Var And_InPlace(Var aLeft, Var aRight,ScriptContext* scriptContext, JavascriptNumber *result);

            static Var Or(Var aLeft, Var aRight,ScriptContext* scriptContext);
            static Var Or_Full(Var aLeft, Var aRight,ScriptContext* scriptContext);
            static Var Or_InPlace(Var aLeft, Var aRight,ScriptContext* scriptContext, JavascriptNumber *result);

            static Var Xor(Var aLeft, Var aRight,ScriptContext* scriptContext);
            static Var Xor_Full(Var aLeft, Var aRight,ScriptContext* scriptContext);
            static Var Xor_InPlace(Var aLeft, Var aRight,ScriptContext* scriptContext, JavascriptNumber *result);

            static Var ShiftLeft(Var aLeft, Var aRight,ScriptContext* scriptContext);
            static Var ShiftLeft_Full(Var aLeft, Var aRight,ScriptContext* scriptContext);
            static Var ShiftRight(Var aLeft, Var aRight,ScriptContext* scriptContext);
            static Var ShiftRight_Full(Var aLeft, Var aRight,ScriptContext* scriptContext);
            static Var ShiftRightU(Var aLeft, Var aRight,ScriptContext* scriptContext);
            static Var ShiftRightU_Full(Var aLeft, Var aRight,ScriptContext* scriptContext);

            static Var FinishOddDivByPow2(int32 value, ScriptContext *scriptContext);
            static Var FinishOddDivByPow2_InPlace(int32 value, ScriptContext *scriptContext, JavascriptNumber* result);
            static Var MaxInAnArray(RecyclableObject * function, CallInfo callInfo, ...);
            static Var MinInAnArray(RecyclableObject * function, CallInfo callInfo, ...);

            static double Random(ScriptContext *scriptContext);
            static int32 ToInt32Core(double T1);
            static uint32 ToUInt32(double value);
            static int64 TryToInt64(double T1);
            static int32 ToInt32(Var aValue, ScriptContext* scriptContext);
            static int32 ToInt32(double value);
            static int32 ToInt32_Full(Var aValue, ScriptContext* scriptContext);

        private:
            static Var Add_FullHelper(Var aLeft, Var aRight, ScriptContext* scriptContext, JavascriptNumber* result, bool leftIsDead);
            static Var Add_FullHelper_Wrapper(Var aLeft, Var aRight, ScriptContext* scriptContext, JavascriptNumber* result, bool leftIsDead);

            static double Add_Helper(Var aLeft, Var aRight,ScriptContext* scriptContext);
            static double Subtract_Helper(Var aLeft, Var aRight,ScriptContext* scriptContext);
            static double Multiply_Helper(Var aLeft, Var aRight,ScriptContext* scriptContext);
            static double Divide_Helper(Var aLeft, Var aRight,ScriptContext* scriptContext);
            static double Modulus_Helper(Var aLeft, Var aRight,ScriptContext* scriptContext);

            static Var Add_DoubleHelper(double dblLeft, Var addRight, ScriptContext* scriptContext, JavascriptNumber* result);
            static Var Add_DoubleHelper(Var addLeft, double dblRight, ScriptContext* scriptContext, JavascriptNumber* result);
            static Var Subtract_DoubleHelper(double dblLeft, Var subRight, ScriptContext* scriptContext, JavascriptNumber* result);
            static Var Subtract_DoubleHelper(Var subLeft, double dblRight, ScriptContext* scriptContext, JavascriptNumber* result);


            static int32 And_Helper(Var aLeft, Var aRight,ScriptContext* scriptContext);
            static double Decrement_Helper(Var aLeft, ScriptContext* scriptContext);
            static double Increment_Helper(Var aLeft, ScriptContext* scriptContext);
            static double Negate_Helper(Var aRight,ScriptContext* scriptContext);
            static int32 Or_Helper(Var aLeft, Var aRight,ScriptContext* scriptContext);

            static BOOL IsNanInfZero(double v);
            static __int64 ToInt32ES5OverflowHelper(double d);

        };
#ifdef SSE2MATH
    }
#endif
}
