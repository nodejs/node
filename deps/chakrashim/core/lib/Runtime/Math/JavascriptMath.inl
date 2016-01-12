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
        __inline Var JavascriptMath::Increment(Var aRight, ScriptContext* scriptContext)
        {
            return Increment_Full(aRight, scriptContext);
        }

        __inline Var JavascriptMath::Decrement(Var aRight, ScriptContext* scriptContext)
        {
            return Decrement_Full(aRight, scriptContext);
        }

        __inline Var JavascriptMath::Negate(Var aRight, ScriptContext* scriptContext)
        {
            return
                (TaggedInt::Is(aRight) && aRight != TaggedInt::ToVarUnchecked(0) && aRight != TaggedInt::MinVal()) ?
                    TaggedInt::NegateUnchecked(aRight) :
                    Negate_Full(aRight,scriptContext);
        }

        __inline Var JavascriptMath::Not(Var aRight, ScriptContext* scriptContext)
        {
            return
                TaggedInt::Is(aRight) ?
                TaggedInt::Not(aRight,scriptContext) :
                Not_Full(aRight,scriptContext);
        }


        __inline Var JavascriptMath::Or(Var aLeft, Var aRight, ScriptContext* scriptContext)
        {
            return
                TaggedInt::IsPair(aLeft,aRight) ?
                TaggedInt::Or(aLeft,aRight) :
                Or_Full(aLeft,aRight,scriptContext);
        }

        __inline Var JavascriptMath::And(Var aLeft, Var aRight, ScriptContext* scriptContext)
        {
#if FLOATVAR
            return
                TaggedInt::IsPair(aLeft,aRight) ?
                TaggedInt::And(aLeft,aRight) :
                And_Full(aLeft,aRight,scriptContext);
#else
            Var varSpeculative = TaggedInt::Speculative_And(aLeft, aRight);
            if (TaggedInt::Is(varSpeculative))
            {
                return varSpeculative;
            }

            return And_Full(aLeft, aRight, scriptContext);
#endif
        }

        __inline Var JavascriptMath::ShiftLeft(Var aLeft,Var aRight,ScriptContext* scriptContext)
        {
            return
                TaggedInt::IsPair(aLeft, aRight) ?
                TaggedInt::ShiftLeft(aLeft, aRight,scriptContext) :
                ShiftLeft_Full(aLeft, aRight,scriptContext);
        }

        __inline Var JavascriptMath::ShiftRight(Var aLeft, Var aRight, ScriptContext* scriptContext)
        {
            return
                TaggedInt::IsPair(aLeft, aRight) ?
                TaggedInt::ShiftRight(aLeft, aRight) :
                ShiftRight_Full(aLeft, aRight,scriptContext);
        }

        __inline Var JavascriptMath::ShiftRightU(Var aLeft, Var aRight, ScriptContext* scriptContext)
        {
            return
                TaggedInt::IsPair(aLeft, aRight) ?
                TaggedInt::ShiftRightU(aLeft, aRight, scriptContext) :
                ShiftRightU_Full(aLeft, aRight,scriptContext);
        }

        __inline Var JavascriptMath::Xor(Var aLeft, Var aRight, ScriptContext* scriptContext)
        {
            return
                TaggedInt::IsPair(aLeft, aRight) ?
                TaggedInt::Xor(aLeft, aRight) :
                Xor_Full(aLeft, aRight,scriptContext);
        }

        __inline double JavascriptMath::Decrement_Helper(Var aRight, ScriptContext* scriptContext)
        {
    #if defined(DBG)
            if (TaggedInt::Is(aRight))
            {
                // The only reason to be here is if TaggedInt increment underflowed
                AssertMsg(aRight == TaggedInt::MinVal(), "TaggedInt decrement should be handled in generated code.");
            }
    #endif

            double value = JavascriptConversion::ToNumber(aRight, scriptContext);
            return --value;
        }

        __inline double JavascriptMath::Increment_Helper(Var aRight, ScriptContext* scriptContext)
        {
    #if defined(DBG)
            if (TaggedInt::Is(aRight))
            {
                // The only reason to be here is if TaggedInt increment overflowed
                AssertMsg(aRight == TaggedInt::MaxVal(), "TaggedInt increment should be handled in generated code.");
            }
    #endif

            double value = JavascriptConversion::ToNumber(aRight, scriptContext);
            return ++value;
        }

        __inline double JavascriptMath::Negate_Helper(Var aRight,ScriptContext* scriptContext)
        {
            Assert(aRight != nullptr);
            Assert(scriptContext != nullptr);

            double value = JavascriptConversion::ToNumber(aRight, scriptContext);
            return -value;
        }

        __inline int32 JavascriptMath::And_Helper(Var aLeft, Var aRight, ScriptContext* scriptContext)
        {
            Assert(aLeft != nullptr);
            Assert(aRight != nullptr);
            Assert(scriptContext != nullptr);
#if _M_IX86
            AssertMsg(!TaggedInt::IsPair(aLeft, aRight), "TaggedInt bitwise and should have been handled already");
#endif

            int32 nLeft = JavascriptConversion::ToInt32(aLeft, scriptContext);
            int32 nRight = JavascriptConversion::ToInt32(aRight, scriptContext);
            return nLeft & nRight;
        }

        __inline int32 JavascriptMath::Or_Helper(Var aLeft, Var aRight, ScriptContext* scriptContext)
        {
            Assert(aLeft != nullptr);
            Assert(aRight != nullptr);
            Assert(scriptContext != nullptr);
#if _M_IX86
            AssertMsg(!TaggedInt::IsPair(aLeft, aRight), "TaggedInt bitwise or should have been handled already");
#endif
            int32 nLeft = JavascriptConversion::ToInt32(aLeft, scriptContext);
            int32 nRight = JavascriptConversion::ToInt32(aRight, scriptContext);
            return nLeft | nRight;
        }


        __inline double JavascriptMath::Add_Helper(Var aLeft, Var aRight, ScriptContext* scriptContext)
        {
            AssertMsg( !JavascriptString::Is(aLeft), "Strings should have been handled already" );
            AssertMsg( !JavascriptString::Is(aRight), "Strings should have been handled already" );

            double dblLeft = JavascriptConversion::ToNumber(aLeft, scriptContext);
            double dblRight = JavascriptConversion::ToNumber(aRight, scriptContext);
            return dblLeft + dblRight;
        }

        __inline Var JavascriptMath::Add(Var aLeft, Var aRight, ScriptContext* scriptContext)
        {
            return
                TaggedInt::IsPair(aLeft,aRight) ?
                TaggedInt::Add(aLeft, aRight, scriptContext) :
                Add_Full(aLeft, aRight, scriptContext);
        }

        __inline Var JavascriptMath::Subtract(Var aLeft, Var aRight, ScriptContext* scriptContext)
        {
            return
                TaggedInt::IsPair(aLeft,aRight) ?
                TaggedInt::Subtract(aLeft, aRight, scriptContext) :
                Subtract_Full(aLeft, aRight, scriptContext);
        }

        __inline double JavascriptMath::Subtract_Helper(Var aLeft, Var aRight, ScriptContext* scriptContext)
        {
            Assert(aLeft != nullptr);
            Assert(aRight != nullptr);
            Assert(scriptContext != nullptr);

            // The IEEE 754 floating point spec ensures that NaNs are preserved in all operations
            double dblLeft = JavascriptConversion::ToNumber(aLeft, scriptContext);
            double dblRight = JavascriptConversion::ToNumber(aRight, scriptContext);
            return dblLeft - dblRight;
        }

        __inline Var JavascriptMath::Multiply(Var aLeft, Var aRight, ScriptContext* scriptContext)
        {
            return
                TaggedInt::IsPair(aLeft,aRight) ?
                TaggedInt::Multiply(aLeft, aRight, scriptContext) :
                Multiply_Full(aLeft, aRight, scriptContext);
        }

        __inline Var JavascriptMath::Exponentiation(Var aLeft, Var aRight, ScriptContext* scriptContext)
        {
            return Exponentiation_Full(aLeft, aRight, scriptContext);
        }


        __inline double JavascriptMath::Multiply_Helper(Var aLeft, Var aRight, ScriptContext* scriptContext)
        {
            Assert(aLeft != nullptr);
            Assert(aRight != nullptr);
            Assert(scriptContext != nullptr);

            // The IEEE 754 floating point spec ensures that NaNs are preserved in all operations
            return JavascriptConversion::ToNumber(aLeft, scriptContext) * JavascriptConversion::ToNumber(aRight, scriptContext);
        }

        __inline Var JavascriptMath::Divide(Var aLeft, Var aRight, ScriptContext* scriptContext)
        {
            // The TaggedInt,TaggedInt case is handled within Divide_Full
            return Divide_Full(aLeft, aRight,scriptContext);
        }

        __inline double JavascriptMath::Divide_Helper(Var aLeft, Var aRight, ScriptContext* scriptContext)
        {
            Assert(aLeft != nullptr);
            Assert(aRight != nullptr);
            Assert(scriptContext != nullptr);

#if !defined(_M_ARM32_OR_ARM64)
            AssertMsg(!TaggedInt::IsPair(aLeft, aRight), "Integer division should have been handled already");
#endif

            // The IEEE 754 floating point spec ensures that NaNs are preserved in all operations
            return JavascriptConversion::ToNumber(aLeft, scriptContext) / JavascriptConversion::ToNumber(aRight, scriptContext);
        }

        __inline Var JavascriptMath::Modulus(Var aLeft, Var aRight, ScriptContext* scriptContext)
        {
            return Modulus_Full(aLeft, aRight, scriptContext);
        }

        __inline double JavascriptMath::Modulus_Helper(Var aLeft, Var aRight, ScriptContext* scriptContext)
        {
            double dblLeft = JavascriptConversion::ToNumber(aLeft, scriptContext);
            double dblRight = JavascriptConversion::ToNumber(aRight, scriptContext);
            return NumberUtilities::Modulus(dblLeft, dblRight);
        }

#if defined(_M_ARM32_OR_ARM64)
        __inline int32 JavascriptMath::ToInt32Core(double T1)
        {
            // Try the int32 conversion first and only do the more expensive (& closer to spec)
            // i64 conversion if it fails.
            __int32 i32 = (__int32)T1;
            if ((i32 != 0x80000000) && (i32 != 0x7fffffff))
                return i32;     //No overflow so just return i32

            int64 T4_64 = TryToInt64(T1);
            if (!NumberUtilities::IsValidTryToInt64(T4_64)) // overflow
            {
                T4_64 = ToInt32ES5OverflowHelper(T1);
            }

            return static_cast<int32>(T4_64);
        }
#else
        __inline int32 JavascriptMath::ToInt32Core(double T1)
        {
            // ES5 Spec for ToUInt32
            //
            //  T3 = sign(T1) * floor(abs(T1))
            //  T4 = T3 % 2^32
            //
            // Casting gives equivalent result, except when T1 > INT64_MAX, or T1 < INT64_MIN (or NaN Inf Zero),
            // in which case we'll use slow path.

            // Try casting to int32 first. Results in 0x80000000 if it overflows.
            int32 T4_32 = static_cast<int32>(T1);
            if (T4_32 != 0x80000000)
            {
                return T4_32;
            }

            int64 T4_64 = TryToInt64(T1);
            if (T4_64 == 0x8000000000000000) // overflow && ES5
            {
                T4_64 = ToInt32ES5OverflowHelper(T1);
            }

            return static_cast<int32>(T4_64);
        }
#endif

        // Implements platform-agnostic part of handling overflow when converting Number to int32, ES5 version.
        __inline __int64 JavascriptMath::ToInt32ES5OverflowHelper(double d)
        {
            if (IsNanInfZero(d)) // ShortCut NaN Inf Zero
            {
                return 0;
            }
            const double k_2to32 = 4294967296.0;
            double floored;

#pragma prefast(suppress:6031, "We don't care about the fraction part")
            modf(d, &floored);                      // take out the floating point part.
            double m2to32 = fmod(floored, k_2to32); // divide modulo 2^32.
            __int64 result = TryToInt64(m2to32);

            AssertMsg(NumberUtilities::IsValidTryToInt64(result), "No more overflow expected");

            return result;
        }

        __inline BOOL JavascriptMath::IsNanInfZero(double v)
        {
            return JavascriptNumber::IsNan(v) || JavascriptNumber::IsZero(v) || JavascriptNumber::IsPosInf(v) || JavascriptNumber::IsNegInf(v);
        }

        __inline int64 JavascriptMath::TryToInt64(double T1)
        {
            return Js::NumberUtilities::TryToInt64(T1);
        }

        __inline int32 JavascriptMath::ToInt32(Var aValue, ScriptContext* scriptContext)
        {
            return
                TaggedInt::Is(aValue) ?
                TaggedInt::ToInt32(aValue) :
                ToInt32_Full(aValue, scriptContext);
        }
#ifdef SSE2MATH
    }
#endif
}
