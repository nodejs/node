//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"
#include "Language\JavascriptMathOperators.h"
#include "Math\CrtSSE2Math.h"

#if defined(_M_IX86) || defined(_M_X64)
#pragma intrinsic(_mm_round_sd)
#endif

void UCrtC99MathApis::Ensure()
{
    if (m_isInit)
    {
        return;
    }

    DelayLoadLibrary::EnsureFromSystemDirOnly();

    if (IsAvailable())
    {
        m_pfnlog2  = (PFNMathFn)GetFunction("log2");
        m_pfnlog1p = (PFNMathFn)GetFunction("log1p");
        m_pfnexpm1 = (PFNMathFn)GetFunction("expm1");
        m_pfnacosh = (PFNMathFn)GetFunction("acosh");
        m_pfnasinh = (PFNMathFn)GetFunction("asinh");
        m_pfnatanh = (PFNMathFn)GetFunction("atanh");
        m_pfntrunc = (PFNMathFn)GetFunction("trunc");
        m_pfncbrt  = (PFNMathFn)GetFunction("cbrt");

        if (m_pfnlog2 == nullptr ||
            m_pfnlog1p == nullptr ||
            m_pfnexpm1 == nullptr ||
            m_pfnacosh == nullptr ||
            m_pfnasinh == nullptr ||
            m_pfnatanh == nullptr ||
            m_pfntrunc == nullptr ||
            m_pfncbrt == nullptr)
        {
            // If any of the APIs fail to load then presume the entire module is bogus and free it
            FreeLibrary(m_hModule);
            m_hModule = nullptr;
        }
    }
}

namespace Js
{
    const double Math::PI = 3.1415926535897931;
    const double Math::E = 2.7182818284590451;
    const double Math::LN10 = 2.3025850929940459;
    const double Math::LN2 = 0.69314718055994529;
    const double Math::LOG2E = 1.4426950408889634;
    const double Math::LOG10E = 0.43429448190325182;
    const double Math::SQRT1_2 = 0.70710678118654757;
    const double Math::SQRT2 = 1.4142135623730951;
    const double Math::EPSILON = 2.2204460492503130808472633361816e-16;
    const double Math::MAX_SAFE_INTEGER = 9007199254740991.0;
    const double Math::MIN_SAFE_INTEGER = -9007199254740991.0;


    ///----------------------------------------------------------------------------
    /// Abs() returns the absolute value of the given number, as described in
    /// (ES5.0: S15.8.2.1).
    ///----------------------------------------------------------------------------

    Var Math::Abs(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count >= 2)
        {
            Var arg = args[1];

            if (TaggedInt::Is(arg))
            {
#if defined(_M_X64_OR_ARM64)
                __int64 result = ::_abs64(TaggedInt::ToInt32(arg));
#else
                __int32 result = ::abs(TaggedInt::ToInt32(arg));
#endif
                return JavascriptNumber::ToVar(result, scriptContext);
            }

            double x = JavascriptConversion::ToNumber(args[1], scriptContext);

            double result = Math::Abs(x);
            return JavascriptNumber::ToVarNoCheck(result, scriptContext);
        }
        else
        {
            return scriptContext->GetLibrary()->GetNaN();
        }
    }

    double Math::Abs(double x)
    {
        // ::fabs if linked from UCRT changes FPU ctrl word for NaN input
        if (NumberUtilities::IsNan(x))
        {
            // canonicalize to 0xFFF8000..., so we can tag correctly on x64.
            return NumberConstants::NaN;
        }
        return ::fabs(x);
    }

    ///----------------------------------------------------------------------------
    /// Acos() returns the arc-cosine of the given angle in radians, as described
    /// in (ES5.0: S15.8.2.2).
    ///----------------------------------------------------------------------------

    Var Math::Acos(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");

        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count >= 2)
        {
            double x = JavascriptConversion::ToNumber(args[1], scriptContext);

            double result = Math::Acos(x);

            return JavascriptNumber::ToVarNoCheck(result, scriptContext);
        }
        else
        {
            return scriptContext->GetLibrary()->GetNaN();
        }
    }

    double Math::Acos(double x)
    {
        double result;

#if defined(_M_IX86)
        if (AutoSystemInfo::Data.SSE2Available())
        {
            _asm {
                movsd xmm0, x
                call dword ptr [__libm_sse2_acos]
                movsd result, xmm0
            }
        }
        else
#endif
        {
            result = ::acos(x);
        }

        return result;
    }

    ///----------------------------------------------------------------------------
    /// Asin() returns the arc-sine of the given angle in radians, as described in
    /// (ES5.0: S15.8.2.3).
    ///----------------------------------------------------------------------------

    Var Math::Asin(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count >= 2)
        {
            double x = JavascriptConversion::ToNumber(args[1], scriptContext);

            double result = Math::Asin(x);

            return JavascriptNumber::ToVarNoCheck(result, scriptContext);
        }
        else
        {
            return scriptContext->GetLibrary()->GetNaN();
        }
    }

    double Math::Asin(double x)
    {
        double result;

#if defined(_M_IX86)
        if (AutoSystemInfo::Data.SSE2Available())
        {
            _asm {
                movsd xmm0, x
                call dword ptr [__libm_sse2_asin]
                movsd result, xmm0
            }
        }
        else
#endif
        {
            result = ::asin(x);
        }

        return result;
    }

    ///----------------------------------------------------------------------------
    /// Atan() returns the arc-tangent of the given angle in radians, as described
    /// in (ES5.0: S15.8.2.4).
    ///----------------------------------------------------------------------------

    Var Math::Atan(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count >= 2)
        {
            double x = JavascriptConversion::ToNumber(args[1], scriptContext);

            double result = Math::Atan(x);

            return JavascriptNumber::ToVarNoCheck(result, scriptContext);
        }
        else
        {
            return scriptContext->GetLibrary()->GetNaN();
        }
    }

    double Math::Atan(double x)
    {
        double result;
#if defined(_M_IX86)
        if (AutoSystemInfo::Data.SSE2Available())
        {
            _asm {
                movsd xmm0, x
                call dword ptr [__libm_sse2_atan]
                movsd result, xmm0
            }
        }
        else
#endif
        {
            result = ::atan(x);
        }

        return result;
    }

    ///----------------------------------------------------------------------------
    /// Atan2() returns the arc-tangent of the angle described by the given x and y
    /// lengths, as described in (ES5.0: S15.8.2.5).
    ///----------------------------------------------------------------------------

    Var Math::Atan2(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count >= 3)
        {
            double x = JavascriptConversion::ToNumber(args[1], scriptContext);
            double y = JavascriptConversion::ToNumber(args[2], scriptContext);
            double result = Math::Atan2(x,y);
            return JavascriptNumber::ToVarNoCheck(result, scriptContext);
        }
        else
        {
            return scriptContext->GetLibrary()->GetNaN();
        }
    }

    double Math::Atan2( double x, double y )
    {
        double result = 0;
        enum
        {
            kntP0,
            kntP1,
            kntPInf,
            kntN0,
            kntN1,
            kntNInf,
            kntLim
        };

        int nt1, nt2;

        if( NumberUtilities::IsFinite( x ) )
        {
            nt1 = ( 0 == x ) ? kntP0 : kntP1;
        }
        else if( JavascriptNumber::IsNan( x ) )
        {
            result = x;
            goto LDone;
        }
        else
        {
            nt1 = kntPInf;
        }

        // TODO (abchatra): Remove old macros LuHiDbl and LuLoDbl.
        if( NumberUtilities::LuHiDbl( x ) & 0x80000000 )
        {
            nt1 += kntN0;
        }

        if( NumberUtilities::IsFinite( y ) )
        {
            nt2 = ( 0 == y ) ? kntP0 : kntP1;
        }
        else if( JavascriptNumber::IsNan( y ) )
        {
            result = y;
            goto LDone;
        }
        else
        {
            nt2 = kntPInf;
        }

        if( NumberUtilities::LuHiDbl( y ) & 0x80000000 )
        {
            nt2 += kntN0;
        }

        // There are a bunch of cases that the CRT atan2 doesn't do correctly.
#define NTP(a, b) ((a) * kntLim + (b))
        switch( NTP( nt1, nt2 ) )
        {
        default:
#if 0
            // This Atan2 gives slightly different results.  Disable as it doesn't appear to
            // be an important perf scenario.
            if( AutoSystemInfo::Data.SSE2Available() )
            {
                _asm {
                    movsd xmm0, x
                        movsd xmm1, y
                        call dword ptr[__libm_sse2_atan2]
                        movsd result, xmm0
                }
            }
            else
#endif
            {
                result = ::atan2( x, y );
            }
            break;

        case NTP( kntPInf, kntPInf ):
            // +PI / 4 (+0.7853981633974483) : some compilers have problems with
            // long decimal constants!
            NumberUtilities::LuHiDbl( result ) = 0x3FE921FB;
            NumberUtilities::LuLoDbl( result ) = 0x54442D18;
            break;
        case NTP( kntPInf, kntNInf ):
            // +3 * PI / 4 (+2.356194490192345)
            NumberUtilities::LuHiDbl( result ) = 0x4002D97C;
            NumberUtilities::LuLoDbl( result ) = 0x7F3321D2;
            break;
        case NTP( kntNInf, kntPInf ):
            // -PI / 4 (-0.7853981633974483)
            NumberUtilities::LuHiDbl( result ) = 0xBFE921FB;
            NumberUtilities::LuLoDbl( result ) = 0x54442D18;
            break;
        case NTP( kntNInf, kntNInf ):
            // -3 * PI / 4 (-2.356194490192345)
            NumberUtilities::LuHiDbl( result ) = 0xC002D97C;
            NumberUtilities::LuLoDbl( result ) = 0x7F3321D2;
            break;

        case NTP( kntP0, kntP0 ):
            // +0
            result = 0;
            break;
        case NTP( kntP0, kntN0 ):
            // +PI (+3.141592653589793)
            NumberUtilities::LuHiDbl( result ) = 0x400921FB;
            NumberUtilities::LuLoDbl( result ) = 0x54442D18;
            break;
        case NTP( kntN1, kntPInf ):
        case NTP( kntN0, kntPInf ):
        case NTP( kntN0, kntP1 ):
        case NTP( kntN0, kntP0 ):
            // -0
            NumberUtilities::LuHiDbl( result ) = 0x80000000;
            NumberUtilities::LuLoDbl( result ) = 0;
            break;
        case NTP( kntN0, kntN0 ):
            // -PI (-3.141592653589793)
            NumberUtilities::LuHiDbl( result ) = 0xC00921FB;
            NumberUtilities::LuLoDbl( result ) = 0x54442D18;
            break;

        case NTP( kntPInf, kntN1 ):
        case NTP( kntPInf, kntN0 ):
            // +PI / 2 (+1.5707963267948966)
            NumberUtilities::LuHiDbl( result ) = 0x3FF921FB;
            NumberUtilities::LuLoDbl( result ) = 0x54442D18;
            break;
        case NTP( kntNInf, kntP1 ):
        case NTP( kntNInf, kntP0 ):
            // -PI / 2 (-1.5707963267948966)
            NumberUtilities::LuHiDbl( result ) = 0xBFF921FB;
            NumberUtilities::LuLoDbl( result ) = 0x54442D18;
            break;
        case NTP( kntN1, kntNInf ):
        case NTP( kntN0, kntNInf ):
        case NTP( kntN0, kntN1 ):
            // -PI (-3.141592653589793)
            NumberUtilities::LuHiDbl( result ) = 0xC00921FB;
            NumberUtilities::LuLoDbl( result ) = 0x54442D18;
            break;
        }
LDone:
        return result;
    }

    ///----------------------------------------------------------------------------
    /// Ceil() returns the smallest (closest to -Inf) number value that is not less
    /// than x and is equal to an integer, as described in (ES5.0: S15.8.2.6).
    ///----------------------------------------------------------------------------

#pragma warning(push)
#pragma warning(disable:4700)    // uninitialized local variable 'output' used,  for call to _mm_ceil_sd

    Var Math::Ceil(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count >= 2)
        {
            Var input = args[1];

            if (TaggedInt::Is(input))
            {
                return input;
            }

            double x = JavascriptConversion::ToNumber(args[1], scriptContext);
#if defined(_M_ARM32_OR_ARM64)
            if (Js::JavascriptNumber::IsNan(x))
            {
                return scriptContext->GetLibrary()->GetNaN();
            }
#endif

#if defined(_M_IX86) || defined(_M_X64)
            if (AutoSystemInfo::Data.SSE4_1Available())
            {
                __m128d input, output;
                input = _mm_load_sd(&x);
#pragma prefast(suppress:6001, "Signature of _mm_ceil_sd intrinsic confuses prefast, output in the parameter list is not used, it is the dst of the intrinsic")
                output = _mm_ceil_sd(output, input);
                int intResult = _mm_cvtsd_si32(output);

                if (TaggedInt::IsOverflow(intResult) || intResult == 0 || intResult == 0x80000000)
                {
                    double dblResult;
                    _mm_store_sd(&dblResult, output);

                    if (intResult == 0 && !JavascriptNumber::IsNegZero(dblResult))
                    {
                        return JavascriptNumber::ToVar(0, scriptContext);
                    }

                    Assert(dblResult == ::ceil(x) || Js::JavascriptNumber::IsNan(dblResult));

                    return JavascriptNumber::ToVarNoCheck(dblResult, scriptContext);
                }
                else
                {
                    return JavascriptNumber::ToVar(intResult, scriptContext);
                }
            }
            else
#endif
            {
                double result = ::ceil(x);

                intptr_t intResult = (intptr_t)result;

                if (TaggedInt::IsOverflow(intResult) || JavascriptNumber::IsNegZero(result))
                {
                    return JavascriptNumber::ToVarNoCheck(result, scriptContext);
                }
                else
                {
                    return JavascriptNumber::ToVar(intResult, scriptContext);
                }
            }
        }
        else
        {
            return scriptContext->GetLibrary()->GetNaN();
        }
    }
#pragma warning(pop)

    ///----------------------------------------------------------------------------
    /// Cos() returns the cosine of the given angle in radians, as described in
    /// (ES5.0: S15.8.2.7).
    ///----------------------------------------------------------------------------

    Var Math::Cos(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count >= 2)
        {
            double x = JavascriptConversion::ToNumber(args[1], scriptContext);

            double result = Math::Cos(x);

            return JavascriptNumber::ToVarNoCheck(result, scriptContext);
        }
        else
        {
            return scriptContext->GetLibrary()->GetNaN();
        }
    }

    double Math::Cos(double x)
    {
        double result;

#if defined(_M_IX86)
        if (AutoSystemInfo::Data.SSE2Available())
        {
            _asm {
                movsd xmm0, x
                call dword ptr [__libm_sse2_cos]
                movsd result, xmm0
            }
        }
        else
#endif
        {
            result = ::cos(x);
        }

        return result;
    }

    ///----------------------------------------------------------------------------
    /// Exp() returns e^x, as described in (ES5.0: S15.8.2.8).
    ///----------------------------------------------------------------------------

    Var Math::Exp(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count >= 2)
        {
            double x = JavascriptConversion::ToNumber(args[1], scriptContext);

            double result = Math::Exp(x);

            return JavascriptNumber::ToVarNoCheck(result, scriptContext);
        }
        else
        {
            return scriptContext->GetLibrary()->GetNaN();
        }
    }

    double Math::Exp(double x)
    {
        double result;

#if defined(_M_IX86)
        if (AutoSystemInfo::Data.SSE2Available())
        {
            _asm {
                movsd xmm0, x
                call dword ptr [__libm_sse2_exp]
                movsd result, xmm0
            }
        }
        else
#endif
        {
            result = ::exp(x);
        }

        return result;
    }

    ///----------------------------------------------------------------------------
    /// Floor() returns the greatest (closest to +Inf) number value that is not
    /// greater than x and is equal to an integer, as described in
    /// (ES5.0: S15.8.2.9).
    ///----------------------------------------------------------------------------

    Var Math::Floor(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count >= 2)
        {
            Var input = args[1];

            if (TaggedInt::Is(input))
            {
                return input;
            }

            double x = JavascriptConversion::ToNumber(input, scriptContext);
#if defined(_M_ARM32_OR_ARM64)
            if (Js::JavascriptNumber::IsNan(x))
            {
                return scriptContext->GetLibrary()->GetNaN();
            }
#endif
            return Math::FloorDouble(x, scriptContext);
        }
        else
        {
            return scriptContext->GetLibrary()->GetNaN();
        }
    }

#pragma warning(push)
#pragma warning(disable:4700)  // // uninitialized local variable 'output' used, for call to _mm_floor_sd
    Var __inline Math::FloorDouble(double d, ScriptContext *scriptContext)
    {
#if defined(_M_IX86) || defined(_M_X64)
        if (AutoSystemInfo::Data.SSE4_1Available())
        {
            __m128d input, output;

            int intResult;

            input = _mm_load_sd(&d);
            if (d >= 0.0)
            {
                output = input;
            }
            else
            {
#pragma prefast(suppress:6001, "Signature of _mm_floor_sd intrinsic confuses prefast, output in the parameter list is not used, it is the dst of the intrinsic")
                output = _mm_floor_sd(output, input);
            }
            intResult = _mm_cvttsd_si32(output);

            if (TaggedInt::IsOverflow(intResult) || intResult == 0x80000000 || JavascriptNumber::IsNegZero(d))
            {
                double dblResult;
                if (d >= 0.0)
                {
                    output = _mm_floor_sd(output, input);
                }
                _mm_store_sd(&dblResult, output);

                Assert(dblResult == ::floor(d) || Js::JavascriptNumber::IsNan(dblResult));

                return JavascriptNumber::ToVarNoCheck(dblResult, scriptContext);
            }
            else
            {
                Assert(intResult == (int)::floor(d));
                return JavascriptNumber::ToVar(intResult, scriptContext);
            }
        }
        else
#endif
        {
            intptr_t intResult;

            if (d >= 0.0)
            {
                intResult = (intptr_t)d;
            }
            else
            {
                d = ::floor(d);
                intResult = (intptr_t)d;
            }

            if (TaggedInt::IsOverflow(intResult) || JavascriptNumber::IsNegZero(d))
            {
                return JavascriptNumber::ToVarNoCheck(::floor(d), scriptContext);
            }
            else
            {
                return JavascriptNumber::ToVar(intResult, scriptContext);
            }
        }
    }
#pragma warning(pop)
    ///----------------------------------------------------------------------------
    /// Log() returns the natural logarithm of x, as described in
    /// (ES5.0: S15.8.2.10).
    ///----------------------------------------------------------------------------

    Var Math::Log(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count >= 2)
        {
            double x = JavascriptConversion::ToNumber(args[1], scriptContext);

            double result = Math::Log(x);

            return JavascriptNumber::ToVarNoCheck(result, scriptContext);
        }
        else
        {
            return scriptContext->GetLibrary()->GetNaN();
        }
    }

    double Math::Log(double x)
    {
        double result;

#if defined(_M_IX86)
        if (AutoSystemInfo::Data.SSE2Available())
        {
            _asm {
                movsd xmm0, x
                call dword ptr [__libm_sse2_log]
                movsd result, xmm0
            }
        }
        else
#endif
        {
            result = ::log(x);
        }

        return result;
    }

    ///----------------------------------------------------------------------------
    /// Max() returns the maximum of a series of numbers, as described in
    /// (ES5.0: S15.8.2.11):
    /// - Arg:0 = "this"
    /// - Arg:1-n = Values to compare
    ///----------------------------------------------------------------------------

    Var Math::Max(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count <= 1)
        {
            return scriptContext->GetLibrary()->GetNegativeInfinite();
        }
        else if (args.Info.Count == 2)
        {
            double result = JavascriptConversion::ToNumber(args[1], scriptContext);
            return JavascriptNumber::ToVarNoCheck(result, scriptContext);
        }
        else if (args.Info.Count == 3)
        {
            if (TaggedInt::Is(args[1]) && TaggedInt::Is(args[2]))
            {
                return TaggedInt::ToVarUnchecked(max(TaggedInt::ToInt32(args[1]), TaggedInt::ToInt32(args[2])));
            }
        }

        double current = JavascriptConversion::ToNumber(args[1], scriptContext);
        if(JavascriptNumber::IsNan(current))
        {
            return scriptContext->GetLibrary()->GetNaN();
        }

        for (uint idxArg = 2; idxArg < args.Info.Count; idxArg++)
        {
            double compare = JavascriptConversion::ToNumber(args[idxArg], scriptContext);
            if(JavascriptNumber::IsNan(compare))
            {
                return scriptContext->GetLibrary()->GetNaN();
            }
            if((JavascriptNumber::IsNegZero(current) && compare == 0) ||
                current < compare )
            {
                current = compare;
            }
        }

        return JavascriptNumber::ToVarNoCheck(current, scriptContext);
    }


    ///----------------------------------------------------------------------------
    /// Min() returns the minimum of a series of numbers, as described in
    /// (ES5.0: S15.8.2.12):
    /// - Arg:0 = "this"
    /// - Arg:1-n = Values to compare
    ///----------------------------------------------------------------------------

    Var Math::Min(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count <= 1)
        {
            return scriptContext->GetLibrary()->GetPositiveInfinite();
        }
        else if (args.Info.Count == 2)
        {
            double result = JavascriptConversion::ToNumber(args[1], scriptContext);
            return JavascriptNumber::ToVarNoCheck(result, scriptContext);
        }
        else if (args.Info.Count == 3)
        {
            if (TaggedInt::Is(args[1]) && TaggedInt::Is(args[2]))
            {
                return TaggedInt::ToVarUnchecked(min(TaggedInt::ToInt32(args[1]), TaggedInt::ToInt32(args[2])));
            }
        }

        double current = JavascriptConversion::ToNumber(args[1], scriptContext);
        if(JavascriptNumber::IsNan(current))
        {
            return scriptContext->GetLibrary()->GetNaN();
        }

        for (uint idxArg = 2; idxArg < args.Info.Count; idxArg++)
        {
            double compare = JavascriptConversion::ToNumber(args[idxArg], scriptContext);
            if(JavascriptNumber::IsNan(compare))
            {
                return scriptContext->GetLibrary()->GetNaN();
            }
            if((JavascriptNumber::IsNegZero(compare) && current == 0) ||
                current > compare )
            {
                current = compare;
            }
        }

        return JavascriptNumber::ToVarNoCheck(current, scriptContext);
    }


    ///----------------------------------------------------------------------------
    /// Pow() returns x ^ y, as described in (ES5.0: S15.8.2.13).
    ///----------------------------------------------------------------------------

    Var Math::Pow(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count >= 3)
        {
            double x = JavascriptConversion::ToNumber(args[1], scriptContext);
            double y = JavascriptConversion::ToNumber(args[2], scriptContext);
            double result = Math::Pow( x, y );
            return JavascriptNumber::ToVarNoCheck(result, scriptContext);
        }
        else
        {
            return scriptContext->GetLibrary()->GetNaN();
        }
    }

    double Math::Pow( double x, double y )
    {
        double result = 0;

#if defined(_M_IX86)

        // We can't just use "if (0 == y)" because NaN compares
        // equal to 0 according to our compilers.
        if( 0 == NumberUtilities::LuLoDbl( y ) && 0 == ( NumberUtilities::LuHiDbl( y ) & 0x7FFFFFFF ) )
        {
            // Result is 1 even if x is NaN.
            result = 1;
        }
        else if( 1.0 == Math::Abs( x ) && !NumberUtilities::IsFinite( y ) )
        {
            result = JavascriptNumber::NaN;
        }
        else
        {
            if( AutoSystemInfo::Data.SSE2Available() )
            {
                _asm {
                    movsd xmm0, x
                        movsd xmm1, y
                        call dword ptr[__libm_sse2_pow]
                        movsd result, xmm0
                }
            }
            else
            {
                result = ::pow( x, y );
            }
        }
#else
        result = JavascriptNumber::DirectPow( x, y );
#endif
        return result;
    }



    ///----------------------------------------------------------------------------
    /// Random() returns a random number 0 <= n < 1.0, as described in
    /// (ES5.0: S15.8.2.14).
    ///----------------------------------------------------------------------------

    Var Math::Random(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        AssertMsg(callInfo.Count > 0, "Should always have implicit 'this'");
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        double res = JavascriptMath::Random(scriptContext);

        return JavascriptNumber::ToVarNoCheck(res, scriptContext);
    }


    ///----------------------------------------------------------------------------
    /// Round() returns the number value that is closest to x and is equal to an
    /// integer, as described in (ES5.0: S15.8.2.15).
    ///----------------------------------------------------------------------------

    Var Math::Round(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count >= 2)
        {
            Var input = args[1];

            if (TaggedInt::Is(input))
            {
                return input;
            }

            double x = JavascriptConversion::ToNumber(args[1], scriptContext);
            if(JavascriptNumber::IsNan(x))
            {
                return scriptContext->GetLibrary()->GetNaN();
            }
            if(x == 0.0 || !NumberUtilities::IsFinite(x) )
            {
                // 0.0 catches the -0 case...
                return JavascriptNumber::ToVarNoCheck(x, scriptContext);
            }

            if(x < 0 && x >= -0.5)
            {
                return scriptContext->GetLibrary()->GetNegativeZero();
            }

            return Math::FloorDouble(x+0.5, scriptContext);
        }
        else
        {
            return scriptContext->GetLibrary()->GetNaN();
        }
    }

    ///----------------------------------------------------------------------------
    /// Sin() returns the sine of the given angle in radians, as described in
    /// (ES5.0: S15.8.2.16).
    ///----------------------------------------------------------------------------

    Var Math::Sin(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count >= 2)
        {
            double x = JavascriptConversion::ToNumber(args[1], scriptContext);

            double result = Math::Sin(x);

            return JavascriptNumber::ToVarNoCheck(result, scriptContext);
        }
        else
        {
            return scriptContext->GetLibrary()->GetNaN();
        }
    }

    double Math::Sin(double x)
    {
        double result;

#if defined(_M_IX86)
        if (AutoSystemInfo::Data.SSE2Available())
        {
            _asm {
                movsd xmm0, x
                call dword ptr [__libm_sse2_sin]
                movsd result, xmm0
            }
        }
        else
#endif
        {
            result = ::sin(x);
        }

        return result;
    }

    ///----------------------------------------------------------------------------
    /// Sqrt() returns the square-root of the given number, as described in
    /// (ES5.0: S15.8.2.17).
    ///----------------------------------------------------------------------------

    Var Math::Sqrt(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count >= 2)
        {
            Var arg = args[1];
            double x = JavascriptConversion::ToNumber(arg, scriptContext);
            double result = ::sqrt(x);
            if (TaggedInt::Is(arg))
            {
                return JavascriptNumber::ToVarIntCheck(result, scriptContext);
            }
            else
            {
                return JavascriptNumber::ToVarNoCheck(result, scriptContext);
            }
        }
        else
        {
            return scriptContext->GetLibrary()->GetNaN();
        }
    }

    ///----------------------------------------------------------------------------
    /// Tan() returns the tangent of the given angle, in radians, as described in
    /// (ES5.0: S15.8.2.18).
    ///----------------------------------------------------------------------------

    Var Math::Tan(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count >= 2)
        {
            double x = JavascriptConversion::ToNumber(args[1], scriptContext);
            double result = Math::Tan( x );
            return JavascriptNumber::ToVarNoCheck(result, scriptContext);
        }
        else
        {
            return scriptContext->GetLibrary()->GetNaN();
        }
    }

    double Math::Tan( double x )
    {
        double result = 0;
#if defined(_M_IX86)
        if( AutoSystemInfo::Data.SSE2Available() )
        {
            _asm {
                movsd xmm0, x
                    call dword ptr[__libm_sse2_tan]
                    movsd result, xmm0
            }
        }
        else
#endif
        {
            result = ::tan( x );
        }
        return result;
    }


    ///----------------------------------------------------------------------------
    /// Log10() returns the base 10 logarithm of the given number, as described in
    /// (ES6.0: S20.2.2.21).
    ///----------------------------------------------------------------------------
    Var Math::Log10(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(Log10Count);

        if (args.Info.Count >= 2)
        {
            double x = JavascriptConversion::ToNumber(args[1], scriptContext);

            double result = Math::Log10(x);

            return JavascriptNumber::ToVarNoCheck(result, scriptContext);
        }
        else
        {
            return scriptContext->GetLibrary()->GetNaN();
        }
    }

    double Math::Log10(double x)
    {
        double result;

#if defined(_M_IX86)
        if (AutoSystemInfo::Data.SSE2Available())
        {
            _asm {
                movsd xmm0, x
                call dword ptr [__libm_sse2_log10]
                movsd result, xmm0
            }
        }
        else
#endif
        {
            result = ::log10(x);
        }

        return result;
    }

    ///----------------------------------------------------------------------------
    /// Log2() returns the base 2 logarithm of the given number, as described in
    /// (ES6.0: S20.2.2.22).
    ///----------------------------------------------------------------------------
    Var Math::Log2(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(Log2Count);

        if (args.Info.Count >= 2)
        {
            double x = JavascriptConversion::ToNumber(args[1], scriptContext);
            double result = Math::Log2(x, scriptContext);
            return JavascriptNumber::ToVarNoCheck(result, scriptContext);
        }
        else
        {
            return scriptContext->GetLibrary()->GetNaN();
        }
    }

    double Math::Log2(double x, ScriptContext* scriptContext)
    {
#ifdef WHEN_UCRT_IS_LINKED_IN_BUILD
        return ::log2( x );
#else
        // TODO: THE FALLBACK IS NOT ACCURATE; Universal CRT is available on Threshold so we should never fallback but ideally we would link at build time to these APIs instead of loading them at runtime
        UCrtC99MathApis* ucrtC99MathApis = scriptContext->GetThreadContext()->GetUCrtC99MathApis();
        return ucrtC99MathApis->IsAvailable() ?
            ucrtC99MathApis->log2(x) :
            Math::Log( x ) / Math::LN2;
#endif
    }


    ///----------------------------------------------------------------------------
    /// Log1p() returns the natural logarithm of one plus the given number, as
    /// described in (ES6.0: S20.2.2.20).
    ///----------------------------------------------------------------------------
    Var Math::Log1p(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(Log1pCount);

        if (args.Info.Count >= 2)
        {
            double x = JavascriptConversion::ToNumber(args[1], scriptContext);

#ifdef WHEN_UCRT_IS_LINKED_IN_BUILD
            double result = ::log1p(x);
#else
            // TODO: THE FALLBACK IS NOT ACCURATE; Universal CRT is available on Threshold so we should never fallback but ideally we would link at build time to these APIs instead of loading them at runtime
            UCrtC99MathApis* ucrtC99MathApis = scriptContext->GetThreadContext()->GetUCrtC99MathApis();
            double result = ucrtC99MathApis->IsAvailable() ?
                ucrtC99MathApis->log1p(x):
                (JavascriptNumber::IsNegZero(x)) ? x : Math::Log(1.0 + x);
#endif

            return JavascriptNumber::ToVarNoCheck(result, scriptContext);
        }
        else
        {
            return scriptContext->GetLibrary()->GetNaN();
        }
    }

    ///----------------------------------------------------------------------------
    /// Expm1() returns the result of subtracting one from the exponential
    /// function of the given number, as described in (ES6.0: S20.2.2.14).
    ///----------------------------------------------------------------------------
    Var Math::Expm1(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(Expm1Count);

        if (args.Info.Count >= 2)
        {
            double x = JavascriptConversion::ToNumber(args[1], scriptContext);

#ifdef WHEN_UCRT_IS_LINKED_IN_BUILD
            double result = ::expm1(x);
#else
            // TODO: THE FALLBACK IS NOT ACCURATE; Universal CRT is available on Threshold so we should never fallback but ideally we would link at build time to these APIs instead of loading them at runtime
            UCrtC99MathApis* ucrtC99MathApis = scriptContext->GetThreadContext()->GetUCrtC99MathApis();
            double result = ucrtC99MathApis->IsAvailable() ?
                ucrtC99MathApis->expm1(x) :
                (JavascriptNumber::IsNegZero(x)) ? x : Math::Exp(x) - 1.0;
#endif

            return JavascriptNumber::ToVarNoCheck(result, scriptContext);
        }
        else
        {
            return scriptContext->GetLibrary()->GetNaN();
        }
    }

    ///----------------------------------------------------------------------------
    /// Cosh() returns the hyperbolic cosine of the given number, as described in
    /// (ES6.0: S20.2.2.12).
    ///----------------------------------------------------------------------------
    Var Math::Cosh(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(CoshCount);

        if (args.Info.Count >= 2)
        {
            double x = JavascriptConversion::ToNumber(args[1], scriptContext);

            double result = ::cosh(x);

            return JavascriptNumber::ToVarNoCheck(result, scriptContext);
        }
        else
        {
            return scriptContext->GetLibrary()->GetNaN();
        }
    }

    ///----------------------------------------------------------------------------
    /// Sinh() returns the hyperbolic sine of the given number, as described in
    /// (ES6.0: S20.2.2.30).
    ///----------------------------------------------------------------------------
    Var Math::Sinh(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(SinhCount);

        if (args.Info.Count >= 2)
        {
            double x = JavascriptConversion::ToNumber(args[1], scriptContext);

            double result = ::sinh(x);

            return JavascriptNumber::ToVarNoCheck(result, scriptContext);
        }
        else
        {
            return scriptContext->GetLibrary()->GetNaN();
        }
    }

    ///----------------------------------------------------------------------------
    /// Tanh() returns the hyperbolic tangent of the given number, as described in
    /// (ES6.0: S20.2.2.33).
    ///----------------------------------------------------------------------------
    Var Math::Tanh(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(TanhCount);

        if (args.Info.Count >= 2)
        {
            double x = JavascriptConversion::ToNumber(args[1], scriptContext);

            double result = ::tanh(x);

            return JavascriptNumber::ToVarNoCheck(result, scriptContext);
        }
        else
        {
            return scriptContext->GetLibrary()->GetNaN();
        }
    }

    ///----------------------------------------------------------------------------
    /// Acosh() returns the inverse hyperbolic cosine of the given number, as
    /// described in (ES6.0: S20.2.2.3).
    ///----------------------------------------------------------------------------
    Var Math::Acosh(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(AcoshCount);

        if (args.Info.Count >= 2)
        {
            double x = JavascriptConversion::ToNumber(args[1], scriptContext);

#ifdef WHEN_UCRT_IS_LINKED_IN_BUILD
            double result = ::acosh(x);

            return JavascriptNumber::ToVarNoCheck(result, scriptContext);
#else
            // TODO: THE FALLBACK IS NOT ACCURATE; Universal CRT is available on Threshold so we should never fallback but ideally we would link at build time to these APIs instead of loading them at runtime
            UCrtC99MathApis* ucrtC99MathApis = scriptContext->GetThreadContext()->GetUCrtC99MathApis();
            if (ucrtC99MathApis->IsAvailable())
            {
                return JavascriptNumber::ToVarNoCheck(ucrtC99MathApis->acosh(x), scriptContext);
            }
            else if (x >= 1.0)
            {
                // Can be smarter about large values of x, e.g. as x -> Infinity, sqrt(x^2 - 1) -> x
                // Therefore for large x, log(x+x) is sufficient, but how to decide what a large x is?
                // Also ln(x+x) = ln 2x = ln 2 + ln x
                double result = (x == 1.0) ? 0.0 : Math::Log(x + ::sqrt(x*x - 1.0));

                return JavascriptNumber::ToVarNoCheck(result, scriptContext);
            }
            else
            {
                return scriptContext->GetLibrary()->GetNaN();
            }
#endif
        }
        else
        {
            return scriptContext->GetLibrary()->GetNaN();
        }
    }

    ///----------------------------------------------------------------------------
    /// Asinh() returns the inverse hyperbolic sine of the given number, as
    /// described in (ES6.0: S20.2.2.5).
    ///----------------------------------------------------------------------------
    Var Math::Asinh(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(AsinhCount);

        if (args.Info.Count >= 2)
        {
            double x = JavascriptConversion::ToNumber(args[1], scriptContext);

#ifdef WHEN_UCRT_IS_LINKED_IN_BUILD
            double result = ::asinh(x);

            return JavascriptNumber::ToVarNoCheck(result, scriptContext);
#else
            // TODO: THE FALLBACK IS NOT ACCURATE; Universal CRT is available on Threshold so we should never fallback but ideally we would link at build time to these APIs instead of loading them at runtime
            UCrtC99MathApis* ucrtC99MathApis = scriptContext->GetThreadContext()->GetUCrtC99MathApis();
            if (ucrtC99MathApis->IsAvailable())
            {
                return JavascriptNumber::ToVarNoCheck(ucrtC99MathApis->asinh(x), scriptContext);
            }
            else
            {
                double result = JavascriptNumber::IsNegZero(x) ? x :
                    JavascriptNumber::IsPosInf(x) ? JavascriptNumber::POSITIVE_INFINITY :
                    JavascriptNumber::IsNegInf(x) ? JavascriptNumber::NEGATIVE_INFINITY :
                    Math::Log(x + ::sqrt(x*x + 1.0));

                return JavascriptNumber::ToVarNoCheck(result, scriptContext);
            }
#endif
        }
        else
        {
            return scriptContext->GetLibrary()->GetNaN();
        }
    }

    ///----------------------------------------------------------------------------
    /// Atanh() returns the inverse hyperbolic tangent of the given number, as
    /// described in (ES6.0: S20.2.2.7).
    ///----------------------------------------------------------------------------
    Var Math::Atanh(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(AtanhCount);

        if (args.Info.Count >= 2)
        {
            double x = JavascriptConversion::ToNumber(args[1], scriptContext);

#ifdef WHEN_UCRT_IS_LINKED_IN_BUILD
            double result = ::atanh(x);

            return JavascriptNumber::ToVarNoCheck(result, scriptContext);
#else
            // TODO: THE FALLBACK IS NOT ACCURATE; Universal CRT is available on Threshold so we should never fallback but ideally we would link at build time to these APIs instead of loading them at runtime
            UCrtC99MathApis* ucrtC99MathApis = scriptContext->GetThreadContext()->GetUCrtC99MathApis();
            if (ucrtC99MathApis->IsAvailable())
            {
                return JavascriptNumber::ToVarNoCheck(ucrtC99MathApis->atanh(x), scriptContext);
            }
            else if (Math::Abs(x) < 1.0)
            {
                double result = (JavascriptNumber::IsNegZero(x)) ? x : Math::Log((1.0 + x) / (1.0 - x)) / 2.0;

                return JavascriptNumber::ToVarNoCheck(result, scriptContext);
            }
            else
            {
                if (x == -1.0)
                {
                    return scriptContext->GetLibrary()->GetNegativeInfinite();
                }
                else if (x == 1.0)
                {
                    return scriptContext->GetLibrary()->GetPositiveInfinite();
                }

                return scriptContext->GetLibrary()->GetNaN();
            }
#endif
        }
        else
        {
            return scriptContext->GetLibrary()->GetNaN();
        }
    }

    ///----------------------------------------------------------------------------
    /// Hypot() returns the square root of the sum of squares of the two or three
    /// given numbers, as described in (ES6.0: S20.2.2.17).
    ///----------------------------------------------------------------------------
    Var Math::Hypot(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(HypotCount);

        // ES6 20.2.2.18 Math.hypot(value1, value2, ...values)
        // If no arguments are passed, the result is +0.
        // If any argument is +∞, the result is +∞.
        // If any argument is -∞, the result is +∞.
        // If no argument is +∞ or -∞, and any argument is NaN, the result is NaN.
        // If all arguments are either +0 or -0, the result is +0.

        double result = JavascriptNumber::k_Zero; // If there are no arguments return value is positive zero.

        if (args.Info.Count == 2)
        {
            // Special case for one argument
            double x1 = JavascriptConversion::ToNumber(args[1], scriptContext);

            if (JavascriptNumber::IsPosInf(x1) || JavascriptNumber::IsNegInf(x1))
            {
                result = JavascriptNumber::POSITIVE_INFINITY;
            }
            else
            {
                result = Math::Abs(x1);
            }
        }
        else if (args.Info.Count == 3)
        {
            // CRT hypot call
            double x1 = JavascriptConversion::ToNumber(args[1], scriptContext);
            double x2 = JavascriptConversion::ToNumber(args[2], scriptContext);

            if (JavascriptNumber::IsPosInf(x1) || JavascriptNumber::IsNegInf(x1) ||
                JavascriptNumber::IsPosInf(x2) || JavascriptNumber::IsNegInf(x2))
            {
                result = JavascriptNumber::POSITIVE_INFINITY;
            }
            else if (JavascriptNumber::IsNan(x1) || JavascriptNumber::IsNan(x2))
            {
                result = JavascriptNumber::NaN;
            }
            else
            {
                result = ::hypot(x1, x2);
            }
        }
        else if (args.Info.Count > 3)
        {
            // Uncommon case of more than 2 arguments for hypot
            result = Math::HypotHelper(args, scriptContext);
        }

        return JavascriptNumber::ToVarNoCheck(result, scriptContext);
    }

    double Math::HypotHelper(Arguments args, ScriptContext *scriptContext)
    {
        // CRT does not have a multiple version of hypot, so we implement it here ourselves.
        bool foundNaN = false;
        double scale = 0;
        double sum = 0;

        //Ignore first argument which is this pointer
        for (uint counter = 1; counter < args.Info.Count; counter++)
        {
            double doubleVal = JavascriptConversion::ToNumber(args[counter], scriptContext);

            if (JavascriptNumber::IsPosInf(doubleVal) || JavascriptNumber::IsNegInf(doubleVal))
            {
                return JavascriptNumber::POSITIVE_INFINITY;
            }

            if (!foundNaN)
            {
                if (JavascriptNumber::IsNan(doubleVal))
                {
                    //Even though we found NaN, we still need to validate none of the other arguments are +∞ or -∞
                    foundNaN = true;
                }
                else
                {
                    doubleVal = Math::Abs(doubleVal);
                    if (scale < doubleVal)
                    {
                        sum = sum * (scale / doubleVal) * (scale / doubleVal) + 1; /* scale/scale === 1*/
                        //change the scale to new max value
                        scale = doubleVal;
                    }
                    else if (scale != 0)
                    {
                        sum += (doubleVal / scale) * (doubleVal / scale);
                    }
                }
            }
        }

        if (foundNaN)
        {
            return JavascriptNumber::NaN;
        }

        return scale * ::sqrt(sum);
    }

    ///----------------------------------------------------------------------------
    /// Trunc() returns the integral part of the given number, removing any
    /// fractional digits, as described in (ES6.0: S20.2.2.34).
    ///----------------------------------------------------------------------------
    Var Math::Trunc(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(TruncCount);

        if (args.Info.Count >= 2)
        {
            double x = JavascriptConversion::ToNumber(args[1], scriptContext);

#ifdef WHEN_UCRT_IS_LINKED_IN_BUILD
            double result = ::trunc(x);
#else
            // TODO: THE FALLBACK IS NOT ACCURATE; Universal CRT is available on Threshold so we should never fallback but ideally we would link at build time to these APIs instead of loading them at runtime
            UCrtC99MathApis* ucrtC99MathApis = scriptContext->GetThreadContext()->GetUCrtC99MathApis();
            double result = ucrtC99MathApis->IsAvailable() ?
                ucrtC99MathApis->trunc(x) :
                (x < 0.0) ? ::ceil(x) : ::floor(x);
#endif

            return JavascriptNumber::ToVarNoCheck(result, scriptContext);
        }
        else
        {
            return scriptContext->GetLibrary()->GetNaN();
        }
    }

    ///----------------------------------------------------------------------------
    /// Sign() returns the sign of the given number, indicating whether it is
    /// positive, negative, or zero, as described in (ES6.0: S20.2.2.28).
    ///----------------------------------------------------------------------------
    Var Math::Sign(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(SignCount);

        if (args.Info.Count >= 2)
        {
            double x = JavascriptConversion::ToNumber(args[1], scriptContext);

            if (JavascriptNumber::IsNan(x))
            {
                return scriptContext->GetLibrary()->GetNaN();
            }
            else if (JavascriptNumber::IsNegZero(x))
            {
                return scriptContext->GetLibrary()->GetNegativeZero();
            }
            else
            {
                return TaggedInt::ToVarUnchecked(x == 0.0 ? 0 : x < 0.0 ? -1 : 1);
            }
        }
        else
        {
            return scriptContext->GetLibrary()->GetNaN();
        }
    }

    ///----------------------------------------------------------------------------
    /// Cbrt() returns the cube root of the given number, as described in
    /// (ES6.0: S20.2.2.9).
    ///----------------------------------------------------------------------------
    Var Math::Cbrt(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(CbrtCount);

        if (args.Info.Count >= 2)
        {
            double x = JavascriptConversion::ToNumber(args[1], scriptContext);

#ifdef WHEN_UCRT_IS_LINKED_IN_BUILD
            double result = ::cbrt(x);
#else
            // TODO: THE FALLBACK IS NOT ACCURATE; Universal CRT is available on Threshold so we should never fallback but ideally we would link at build time to these APIs instead of loading them at runtime
            UCrtC99MathApis* ucrtC99MathApis = scriptContext->GetThreadContext()->GetUCrtC99MathApis();
            if (ucrtC99MathApis->IsAvailable())
            {
                return JavascriptNumber::ToVarNoCheck(ucrtC99MathApis->cbrt(x), scriptContext);
            }

            bool isNeg = x < 0.0;

            if (isNeg)
            {
                x = -x;
            }

            double result = (x == 0.0) ? x : Math::Exp(Math::Log(x) / 3.0);

            if (isNeg)
            {
                result = -result;
            }
#endif

            return JavascriptNumber::ToVarNoCheck(result, scriptContext);
        }
        else
        {
            return scriptContext->GetLibrary()->GetNaN();
        }
    }

    ///----------------------------------------------------------------------------
    /// Imul() returns the 32-bit integer multiplication of two given numbers, as
    /// described in (ES6.0: S20.2.2.18).
    ///----------------------------------------------------------------------------
    Var Math::Imul(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(ImulCount);

        if (args.Info.Count >= 3)
        {
            int32 x = JavascriptConversion::ToInt32(args[1], scriptContext);
            int32 y = JavascriptConversion::ToInt32(args[2], scriptContext);

            int64 int64Result = (int64)x * (int64)y;
            int32 result = (int32)int64Result;

            return JavascriptNumber::ToVar(result, scriptContext);
        }
        else
        {
            // The arguments, if left unspecified default to undefined, and ToUint32(undefined) produces +0.
            // Therefore we return +0, not NaN here like the other Math functions.
            return TaggedInt::ToVarUnchecked(0);
        }
    }

    ///----------------------------------------------------------------------------
    /// Clz32() returns the leading number of zero bits of ToUint32(x), as
    /// described in (ES6.0: S20.2.2.11).
    ///----------------------------------------------------------------------------
    Var Math::Clz32(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        Var value = args.Info.Count > 1 ? args[1] : scriptContext->GetLibrary()->GetUndefined();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(Clz32Count);

        uint32 uint32value = JavascriptConversion::ToUInt32(value, scriptContext);
        DWORD index;

        if (!_BitScanReverse(&index, uint32value))
        {
            return TaggedInt::ToVarUnchecked(32);
        }
        return TaggedInt::ToVarUnchecked(31 - index);
    }

    Var Math::Fround(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(FroundCount);

        if (args.Info.Count >= 2)
        {
            float x = (float) JavascriptConversion::ToNumber(args[1], scriptContext);

            return JavascriptNumber::ToVarNoCheck((double) x, scriptContext);
        }
        else
        {
            return scriptContext->GetLibrary()->GetNaN();
        }
    }
} // namespace Js
