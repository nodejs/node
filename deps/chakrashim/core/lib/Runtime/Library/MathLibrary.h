//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

class UCrtC99MathApis : protected DelayLoadLibrary
{
private:
    typedef double (__cdecl FNMathFn)(double);
    typedef FNMathFn* PFNMathFn;
    PFNMathFn m_pfnlog2;
    PFNMathFn m_pfnlog1p;
    PFNMathFn m_pfnexpm1;
    PFNMathFn m_pfnacosh;
    PFNMathFn m_pfnasinh;
    PFNMathFn m_pfnatanh;
    PFNMathFn m_pfntrunc;
    PFNMathFn m_pfncbrt;

    void Ensure();

public:
    UCrtC99MathApis() : m_pfnlog2(nullptr), m_pfnlog1p(nullptr), m_pfnexpm1(nullptr), m_pfnacosh(nullptr), m_pfnasinh(nullptr), m_pfnatanh(nullptr), m_pfntrunc(nullptr), m_pfncbrt(nullptr) { }
    virtual ~UCrtC99MathApis() { }

    virtual LPCWSTR GetLibraryName() const override { return L"api-ms-win-crt-math-l1-1-0.dll"; }

    bool IsAvailable() { Ensure(); return DelayLoadLibrary::IsAvailable(); }

    double log2 (_In_ double x) { Assert(IsAvailable()); return m_pfnlog2 (x); }
    double log1p(_In_ double x) { Assert(IsAvailable()); return m_pfnlog1p(x); }
    double expm1(_In_ double x) { Assert(IsAvailable()); return m_pfnexpm1(x); }
    double acosh(_In_ double x) { Assert(IsAvailable()); return m_pfnacosh(x); }
    double asinh(_In_ double x) { Assert(IsAvailable()); return m_pfnasinh(x); }
    double atanh(_In_ double x) { Assert(IsAvailable()); return m_pfnatanh(x); }
    double trunc(_In_ double x) { Assert(IsAvailable()); return m_pfntrunc(x); }
    double cbrt (_In_ double x) { Assert(IsAvailable()); return m_pfncbrt (x); }
};

namespace Js {

    class Math  /* TODO: Determine actual object */
    {
    public:
        class EntryInfo
        {
        public:
            static FunctionInfo Abs;
            static FunctionInfo Acos;
            static FunctionInfo Asin;
            static FunctionInfo Atan;
            static FunctionInfo Atan2;
            static FunctionInfo Ceil;
            static FunctionInfo Cos;
            static FunctionInfo Exp;
            static FunctionInfo Floor;
            static FunctionInfo Log;
            static FunctionInfo Max;
            static FunctionInfo Min;
            static FunctionInfo Pow;
            static FunctionInfo Random;
            static FunctionInfo Round;
            static FunctionInfo Sin;
            static FunctionInfo Sqrt;
            static FunctionInfo Tan;
            // ES6 additions
            static FunctionInfo Log10;
            static FunctionInfo Log2;
            static FunctionInfo Log1p;
            static FunctionInfo Expm1;
            static FunctionInfo Cosh;
            static FunctionInfo Sinh;
            static FunctionInfo Tanh;
            static FunctionInfo Acosh;
            static FunctionInfo Asinh;
            static FunctionInfo Atanh;
            static FunctionInfo Hypot;
            static FunctionInfo Trunc;
            static FunctionInfo Sign;
            static FunctionInfo Cbrt;
            static FunctionInfo Imul;
            static FunctionInfo Clz32;
            static FunctionInfo Fround;
        };

        static Var Abs(RecyclableObject* function, CallInfo callInfo, ...);
        static double Abs(double x);
        static Var Acos(RecyclableObject* function, CallInfo callInfo, ...);
        static double Acos(double x);
        static Var Asin(RecyclableObject* function, CallInfo callInfo, ...);
        static double Asin(double x);
        static Var Atan(RecyclableObject* function, CallInfo callInfo, ...);
        static double Atan(double x);
        static Var Atan2( RecyclableObject* function, CallInfo callInfo, ... );
        static double Atan2( double x, double y );
        static Var Ceil(RecyclableObject* function, CallInfo callInfo, ...);
        static Var Cos(RecyclableObject* function, CallInfo callInfo, ...);
        static double Cos(double x);
        static Var Exp(RecyclableObject* function, CallInfo callInfo, ...);
        static double Exp(double x);
        static Var Floor(RecyclableObject* function, CallInfo callInfo, ...);
        static Var Log(RecyclableObject* function, CallInfo callInfo, ...);
        static double Log(double x);
        static Var Max(RecyclableObject* function, CallInfo callInfo, ...);
        static Var Min(RecyclableObject* function, CallInfo callInfo, ...);
        static Var Pow( RecyclableObject* function, CallInfo callInfo, ... );
        static double Pow( double x, double y);
        static Var Random(RecyclableObject* function, CallInfo callInfo, ...);
        static Var Round(RecyclableObject* function, CallInfo callInfo, ...);
        static Var Sin(RecyclableObject* function, CallInfo callInfo, ...);
        static double Sin(double x);
        static Var Sqrt(RecyclableObject* function, CallInfo callInfo, ...);
        static Var Tan( RecyclableObject* function, CallInfo callInfo, ... );
        static double Tan( double x );
        // ES6 Additions
        static Var Log10(RecyclableObject* function, CallInfo callInfo, ...);
        static double Log10(double x);
        static Var Log2(RecyclableObject* function, CallInfo callInfo, ...);
        static double Log2(double x, ScriptContext* scriptContext);
        static Var Log1p( RecyclableObject* function, CallInfo callInfo, ... );
        static Var Expm1(RecyclableObject* function, CallInfo callInfo, ...);
        static Var Cosh(RecyclableObject* function, CallInfo callInfo, ...);
        static Var Sinh(RecyclableObject* function, CallInfo callInfo, ...);
        static Var Tanh(RecyclableObject* function, CallInfo callInfo, ...);
        static Var Acosh(RecyclableObject* function, CallInfo callInfo, ...);
        static Var Asinh(RecyclableObject* function, CallInfo callInfo, ...);
        static Var Atanh(RecyclableObject* function, CallInfo callInfo, ...);
        static Var Hypot(RecyclableObject* function, CallInfo callInfo, ...);
        static double HypotHelper(Arguments args, ScriptContext* scriptContext);
        static Var Trunc(RecyclableObject* function, CallInfo callInfo, ...);
        static Var Sign(RecyclableObject* function, CallInfo callInfo, ...);
        static Var Cbrt(RecyclableObject* function, CallInfo callInfo, ...);
        static Var Imul(RecyclableObject* function, CallInfo callInfo, ...);
        static Var Clz32(RecyclableObject* function, CallInfo callInfo, ...);
        static Var Fround(RecyclableObject* function, CallInfo callInfo, ...);

        static double NonZeroMin(double x1, double x2, double x3);

        static const double PI;
        static const double E;
        static const double LN10;
        static const double LN2;
        static const double LOG2E;
        static const double LOG10E;
        static const double SQRT1_2;
        static const double SQRT2;
        static const double EPSILON;
        static const double MAX_SAFE_INTEGER;
        static const double MIN_SAFE_INTEGER;

    private:
        static Var Math::FloorDouble(double d, ScriptContext *scriptContext);
    };

} // namespace Js
