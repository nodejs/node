//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLanguagePch.h"

namespace Js
{
    Var TaggedInt::Negate(Var aRight,ScriptContext* scriptContext)
    {
        int32 nValue = ToInt32(aRight);
        return JavascriptNumber::ToVar(-nValue,scriptContext);
    }

    Var TaggedInt::Not(Var aRight,ScriptContext* scriptContext)
    {
        int32 nValue = ToInt32(aRight);

        return JavascriptNumber::ToVar(~nValue,scriptContext);
    }

    // Explicitly marking noinline and stdcall since this is called from inline asm
    __declspec(noinline) Var __stdcall TaggedInt::OverflowHelper(int overflowValue, ScriptContext* scriptContext)
    {
        Assert( IsOverflow(overflowValue) );
        return JavascriptNumber::NewInlined(static_cast<double>(overflowValue), scriptContext);
    }

    // noinline since it's a rare edge case and we don't want to bloat mainline code
    __declspec(noinline) Var TaggedInt::DivideByZero(int nLeft, ScriptContext* scriptContext)
    {
        if (nLeft == 0)
        {
            return scriptContext->GetLibrary()->GetNaN();
        }

        if (nLeft < 0)
        {
            return scriptContext->GetLibrary()->GetNegativeInfinite();
        }

        return scriptContext->GetLibrary()->GetPositiveInfinite();
    }

    Var TaggedInt::Divide(Var aLeft,Var aRight,ScriptContext* scriptContext)
    {
        int nLeft   = ToInt32(aLeft);
        int nRight  = ToInt32(aRight);

        if (nRight == 0)
        {
            return DivideByZero(nLeft, scriptContext);
        }

        //
        // If the operands produce an integer, keep the result as an integer to improve performance:
        // - This also bypasses conversion to / from doubles, which is expensive and potentially
        //   lossy.
        //

#if INT32VAR
        __try
        {
#endif
            if ((nLeft % nRight) == 0)
            {
                //
                // Check that result is not -0. !(Dividend is 0 and Divisor is negative)
                //

                if ((nLeft != 0) || (nRight > 0))
                {
                    return JavascriptNumber::ToVar(nLeft/nRight, scriptContext);
                }
            }
#if INT32VAR
        }
        // 0x80000000 / -1 will trigger an integer overflow exception
        __except(GetExceptionCode() == STATUS_INTEGER_OVERFLOW)
        {}
#endif
        //
        // Fallback to creating a floating-point number to preserve the fractional portion.
        //

        double dblResult = (double) nLeft / (double) nRight;

        return JavascriptNumber::ToVarNoCheck(dblResult, scriptContext);
    }

    Var TaggedInt::Modulus(Var aLeft, Var aRight, ScriptContext* scriptContext)
    {
        int nLeft    = ToInt32(aLeft);
        int nRight   = ToInt32(aRight);

        // nLeft is positive and nRight is +2^i
        // Fast path for Power of 2 divisor
        if (nLeft > 0 && ::Math::IsPow2(nRight))
        {
            return ToVarUnchecked(nLeft & (nRight - 1));
        }

        if (nRight == 0)
        {
            return scriptContext->GetLibrary()->GetNaN();
        }

        if (nLeft == 0)
        {
          return ToVarUnchecked(0);
        }
        int result;
#if INT32VAR
        __try
        {
#endif
            result = nLeft % nRight;

#if INT32VAR
        }
        // 0x80000000 / -1 will trigger an integer overflow exception
        __except(GetExceptionCode() == STATUS_INTEGER_OVERFLOW)
        {
            int64 left64 = nLeft;
            int64 right64 = nRight;
            result = (int)(left64 % right64);
        }
#endif
        if (result != 0)
        {
            return ToVarUnchecked(result);
        }
        else
        {
            //
            // return -0 if left is negative
            //

            if (nLeft >= 0)
            {
                return ToVarUnchecked(0);
            }
            else
            {
                return scriptContext->GetLibrary()->GetNegativeZero();
            }
        }
    }
    Var TaggedInt::DivideInPlace(Var aLeft,Var aRight,ScriptContext* scriptContext, JavascriptNumber *result)
    {
        int nLeft   = ToInt32(aLeft);
        int nRight  = ToInt32(aRight);

        if (nRight == 0)
        {
            return DivideByZero(nLeft, scriptContext);
        }

        //
        // If the operands produce an integer, keep the result as an integer to improve performance:
        // - This also bypasses conversion to / from doubles, which is expensive and potentially
        //   lossy.
        //

        if ((nLeft % nRight) == 0)
        {
            //
            // Check that result is not -0. !(Dividend is 0 and Divisor is negative)
            //

            if ((nLeft != 0) || (nRight > 0))
            {
                return JavascriptNumber::ToVar(nLeft/nRight, scriptContext);
            }
        }


        //
        // Fallback to creating a floating-point number to preserve the fractional portion.
        //

        double dblResult = (double) nLeft / (double) nRight;

        return JavascriptNumber::InPlaceNew(dblResult, scriptContext, result);
    }

    Var TaggedInt::Multiply(Var aLeft, Var aRight, ScriptContext* scriptContext)
    {
        //
        // Perform the signed integer multiplication.
        //

        int nLeft       = ToInt32(aLeft);
        int nRight      = ToInt32(aRight);
        int nResult;
        __int64 int64Result = (__int64)nLeft * (__int64)nRight;
        nResult = (int)int64Result;

        if (((int64Result >> 32) == 0 && (nResult > 0 || nResult == 0 && nLeft+nRight >= 0))
            || ((int64Result >> 32) == -1 && nResult < 0))
        {
            return JavascriptNumber::ToVar(nResult,scriptContext);
        }
        else if (int64Result == 0)
        {
            return JavascriptNumber::ToVarNoCheck((double)nLeft * (double)nRight, scriptContext);
        }
        else
        {
            return JavascriptNumber::ToVarNoCheck((double)int64Result, scriptContext);
        }
    }

    Var TaggedInt::MultiplyInPlace(Var aLeft, Var aRight, ScriptContext* scriptContext, JavascriptNumber *result)
    {
        //
        // Perform the signed integer multiplication.
        //

        int nLeft       = ToInt32(aLeft);
        int nRight      = ToInt32(aRight);
        int nResult;
        __int64 int64Result = (__int64)nLeft * (__int64)nRight;
        nResult = (int)int64Result;

        if (((int64Result >> 32) == 0 && nResult > 0)
            || (int64Result >> 32) == -1 && nResult < 0)
        {
            if (!TaggedInt::IsOverflow(nResult))
            {
                return TaggedInt::ToVarUnchecked(nResult);
            }
            else
            {
                return JavascriptNumber::InPlaceNew((double)nResult, scriptContext, result);
            }
        }
        else if (int64Result == 0)
        {
            return JavascriptNumber::InPlaceNew((double)nLeft * (double)nRight, scriptContext, result);
        }
        else
        {
            return JavascriptNumber::InPlaceNew((double)int64Result, scriptContext, result);
        }
    }


    Var TaggedInt::Subtract(Var aLeft,Var aRight,ScriptContext* scriptContext)
#ifdef DBG
    {
        Var difference = DbgSubtract(aLeft, aRight, scriptContext);
        AssertMsg(JavascriptConversion::ToNumber(difference,scriptContext) == ToDouble(aLeft) - ToDouble(aRight), "TaggedInt fast subtraction is broken");
        return difference;
    }

    Var TaggedInt::DbgSubtract(Var aLeft,Var aRight,ScriptContext* scriptContext)
#endif
    {
#if _M_IX86

        //
        // Perform the signed, integer subtraction directly on Atoms without converting to integers:
        //
        // T        = AtomTag_Int32
        // nResult  = A1 - A2
        //  Step 1: (N1 * S + T) - (N2 * S + T)
        //  Step 2: ((N1 - N2) * S + T) - T
        //  Step 3: A3 - T
        //
        // NOTE: As demonstrated above, the FromVar() / ToVar() calls in (T) will cancel out,
        // enabling an optimized operation.
        //

        __asm
        {
            mov     eax, aLeft
            sub     eax, aRight
            jno     LblDone         // Check for overflow/underflow
                                    // The carry flag indicates whether the sum has
                                    // overflowed (>INT_MAX) or underflowed (< INT_MIN)
            push    scriptContext
            cmc                     // For subtraction, CF=1 indicates an overflow, so reverse the flag
            rcr     eax, 1          // Convert to int32 and set the sign to the carry bit
            push    eax
            call    TaggedInt::OverflowHelper
            dec     eax             // Adjust for the upcoming inc eax
LblDone:
            inc     eax
            // Difference is in eax
        }

#elif defined(_M_X64) || defined(_M_ARM32_OR_ARM64)

        //
        // Perform the signed, integer subtraction directly on Atoms using 64-bit math for overflow
        // checking.
        //

        int64 nResult64 = ToInt64(aLeft) - ToInt64(aRight);
        if (IsOverflow(nResult64))
        {
            //
            // Promote result to double.
            //

            return JavascriptNumber::ToVarNoCheck((double) nResult64, scriptContext);
        }
        else
        {
            //
            // Return A3
            //

            int nResult32 = (int) nResult64;
            return ToVarUnchecked(nResult32);
        }

#else

#error Unsupported processor type

#endif
    }

    // Without checking, bitwise 'and' the two values together. If the result passes the "Is" test
    // then both arguments were valid Int31s and the result is correct.
    Var TaggedInt::Speculative_And(Var aLeft, Var aRight)
    {
        return (Var) (((size_t) aLeft) & ((size_t) aRight));
    }

    Var TaggedInt::And(Var aLeft, Var aRight)
    {
        //
        // Perform the integer "bitwise and":
        //
        // A1       = N1'       | T
        // A2       = N2'       | T
        // A1 & A2  = N1' & N2' | T & T
        // Result   = N1' & N2' | T
        //

        Var aResult = Speculative_And(aLeft, aRight);
        AssertMsg(TaggedInt::Is(aResult), "Ensure result is properly marked");

        return aResult;
    }

    Var TaggedInt::Or(Var aLeft, Var aRight)
    {
        //
        // Perform the integer "bitwise or":
        //
        // A1       = N1'       | T
        // A2       = N2'       | T
        // A1 | A2  = N1' | N2' | T | T
        // Result   = N1' | N2' | T
        //

        Var aResult = (Var) (((size_t) aLeft) | ((size_t) aRight));
        AssertMsg(TaggedInt::Is(aResult), "Ensure result is properly marked");

        return aResult;
    }

#if INT32VAR
    Var TaggedInt::Xor(Var aLeft, Var aRight)
    {
        int32 nResult = ToInt32(aLeft) ^ ToInt32(aRight);
        return TaggedInt::ToVarUnchecked(nResult);
    }
#else
    Var TaggedInt::Xor(Var aLeft, Var aRight)
    {
        //
        // Perform the integer "bitwise xor":
        //
        // A1           = N1'       | T
        // A2           = N2'       | T
        // A1 ^ A2      = N1' ^ N2' | T ^ T
        // Result - T   = N1' ^ N2' | 0
        //

        size_t nResult = ((size_t) aLeft) ^ ((size_t) aRight);
        AssertMsg((nResult & AtomTag) == 0, "Ensure tag-bits cancelled out");

        return (Var) (nResult | AtomTag_IntPtr);
    }
#endif

    Var TaggedInt::ShiftLeft(Var aLeft,Var aRight,ScriptContext* scriptContext)
    {
        //
        // Shifting an integer left will always remain an integer, but it may overflow the Int31
        // range. Therefore, we must call JavascriptNumber::ToVar() to check.
        //

        int nValue      = ToInt32(aLeft);
        uint32 nShift   = ToUInt32(aRight);

        return JavascriptNumber::ToVar(nValue << (nShift & 0x1F),scriptContext);
    }

    Var TaggedInt::ShiftRight(Var aLeft, Var aRight)
    {
        //
        // If aLeft was a Int31 coming in, then the result must always be a Int31 going out because
        // shifting right only makes value smaller. Therefore, we may call ToVarUnchecked()
        // directly.
        //

        int nValue      = ToInt32(aLeft);
        uint32 nShift   = ToUInt32(aRight);

        return ToVarUnchecked(nValue >> (nShift & 0x1F));
    }

    Var TaggedInt::ShiftRightU(Var aLeft, Var aRight, ScriptContext* scriptContext)
    {
        //
        // If aLeft was a Int31 coming in, then the result must always be a Int31 going out because
        // shifting right only makes value smaller. Therefore, we may call ToVarUnchecked()
        // directly.
        //

        uint32 uValue   = ToUInt32(aLeft);
        uint32 nShift   = ToUInt32(aRight);

        return JavascriptNumber::ToVar(uValue >> (nShift & 0x1F), scriptContext);
    }

    void TaggedInt::ToBuffer(Var aValue, __out_ecount_z(bufSize) wchar_t * buffer, uint bufSize)
    {
        return ToBuffer(ToInt32(aValue), buffer, bufSize);
    }

    void TaggedInt::ToBuffer(int value, __out_ecount_z(bufSize) wchar_t * buffer, uint bufSize)
    {
        Assert(bufSize > 10);
        _itow_s(value, buffer, bufSize, 10);
    }

    void TaggedInt::ToBuffer(uint value, __out_ecount_z(bufSize) wchar_t * buffer, uint bufSize)
    {
        Assert(bufSize > 10);
        _ultow_s(value, buffer, bufSize, 10);
    }

    JavascriptString* TaggedInt::ToString(Var aValue,ScriptContext* scriptContext)
    {
        return ToString(ToInt32(aValue), scriptContext);
    }

    JavascriptString* TaggedInt::ToString(int value, ScriptContext* scriptContext)
    {
        wchar_t szBuffer[20];
        ToBuffer(value, szBuffer, _countof(szBuffer));

        return JavascriptString::NewCopySz(szBuffer, scriptContext);
    }
    JavascriptString* TaggedInt::ToString(uint value, ScriptContext* scriptContext)
    {
        wchar_t szBuffer[20];
        ToBuffer(value, szBuffer, _countof(szBuffer));

        return JavascriptString::NewCopySz(szBuffer, scriptContext);
    }

    Var TaggedInt::NegateUnchecked(Var aValue)
    {
        AssertMsg( Is(aValue), "Ensure var is actually an 'TaggedInt'");
        AssertMsg( aValue != ToVarUnchecked(0), "Do not use NegateUnchecked on zero because NegativeZero is special");
        AssertMsg( aValue != ToVarUnchecked(k_nMinValue), "Do not use NegateUnchecked on min value because it cannot be represented");

#if INT32VAR
        int n = ToInt32(aValue);
        Var result = ToVarUnchecked( 0 - n );
#else
        int n = reinterpret_cast<int>(aValue);

        // Negation can be done by subtracting from "zero". The following method
        // is just two operations: "load constant; sub"
        // The constant 2 in the following expression
        // a) adjusts for the bias of ToVarUnchecked(0) and
        // b) ensures the tag bit is set
        Var result = reinterpret_cast<Var>( 2 - n );
#endif

        // Check against the long way (shift, negate, shift, or)
        AssertMsg( result == ToVarUnchecked( -ToInt32(aValue) ), "Logic error in NegateUnchecked" );
        return result;
    }

    // Explicitly marking noinline and stdcall since this is called from inline asm
    __declspec(noinline) Var __stdcall TaggedInt::IncrementOverflowHelper(ScriptContext* scriptContext)
    {
        return JavascriptNumber::New( k_nMaxValue + 1.0, scriptContext );
    }

    Var TaggedInt::Increment(Var aValue, ScriptContext* scriptContext)
    {
#if _M_IX86


        __asm
        {
            mov     eax, aValue
            add     eax, 2
            jno     LblDone
            push    scriptContext
            call    TaggedInt::IncrementOverflowHelper
        LblDone:
            ; result is in eax
        }
#else

#if INT32VAR
        Var result = aValue;
        (*(int *)&result)++;
#else
        int n = reinterpret_cast<int>(aValue);
        n += 2;
        Var result = reinterpret_cast<Var>(n);
#endif

        // Wrap-around
        if( result == MinVal() )
        {
            // Use New instead of ToVar for this constant
            return IncrementOverflowHelper(scriptContext);
        }

        AssertMsg( result == ToVarUnchecked( ToInt32(aValue) + 1 ), "Logic error in Int31::Increment" );
        return result;
#endif
    }

    // Explicitly marking noinline and stdcall since this is called from inline asm
    __declspec(noinline) Var __stdcall TaggedInt::DecrementUnderflowHelper(ScriptContext* scriptContext)
    {
        return JavascriptNumber::New( k_nMinValue - 1.0, scriptContext );
    }

    Var TaggedInt::Decrement(Var aValue, ScriptContext* scriptContext)
    {
#if _M_IX86

        __asm
        {
            mov     eax, aValue
            sub     eax, 2
            jno     LblDone
            push    scriptContext
            call    TaggedInt::DecrementUnderflowHelper
        LblDone:
            ; result is in eax
        }
#else

#if INT32VAR
        Var result = aValue;
        (*(int *)&result)--;
#else
        int n = reinterpret_cast<int>(aValue);
        n -= 2;
        Var result = reinterpret_cast<Var>(n);
#endif

        // Wrap-around
        if( result == MaxVal() )
        {
            // Use New instead of ToVar for this constant
            return DecrementUnderflowHelper(scriptContext);
        }

        AssertMsg( result == ToVarUnchecked( ToInt32(aValue) - 1 ), "Logic error in Int31::Decrement" );
        return result;
#endif
    }
}
