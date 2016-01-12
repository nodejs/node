//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#undef AssertMsg
#undef Assert

#if defined(DBG) && !defined(DIAG_DAC)

// AutoDebug functions that are only available in DEBUG builds
_declspec(selectany) int AssertCount = 0;
_declspec(selectany) int AssertsToConsole = false;
_declspec(thread, selectany) int IsInAssert = false;

#if !defined(USED_IN_STATIC_LIB)
#define REPORT_ASSERT(f, comment) Js::Throw::ReportAssert(__FILE__, __LINE__, STRINGIZE((f)), comment)
#define LOG_ASSERT() Js::Throw::LogAssert()
#else
#define REPORT_ASSERT(f, comment) FALSE
#define LOG_ASSERT()
#endif

#ifdef NTBUILD
#include <ntassert.h>
#define RAISE_ASSERTION(comment) NT_ASSERTMSG(comment, FALSE)
#else
#include <assert.h>
#define RAISE_ASSERTION(comment) DbgRaiseAssertionFailure()
#endif

#define AssertMsg(f, comment) \
    { \
        if (!(f)) \
        { \
            AssertCount++; \
            LOG_ASSERT(); \
            IsInAssert = TRUE; \
            if (!REPORT_ASSERT(f, comment)) \
            { \
                RAISE_ASSERTION(comment); \
            } \
            IsInAssert = FALSE; \
            __analysis_assume(false); \
        } \
    }

#define Assert(exp)           AssertMsg(exp, #exp)
#define AssertVerify(exp)     Assert(exp)
#define Assume(x)             Assert(x)
#define DebugOnly(x)          x
#else // DBG

#define AssertMsg(f, comment) ((void) 0)
#define Assert(exp)           ((void) 0)
#ifdef NTBUILD
#include <ntassert.h>
#define AssertVerify(exp)     NT_VERIFY(exp) // Execute the expression but don't do anything with the result in non-debug builds
#else
#define AssertVerify(exp)     (exp)
#endif
#define Assume(x)             __assume(x)
#define DebugOnly(x)
#endif // DBG

#define AnalysisAssert(x)               Assert(x); __analysis_assume(x)
#define AnalysisAssertMsg(x, comment)   AssertMsg(x, comment); __analysis_assume(x)

#define Unused(var) var;

#define UNREACHED   (0)

#ifndef CompileAssert
#define CompileAssert(e) static_assert(e, #e)
#endif

// We set IsPointer<T>::IsTrue to true if T is a pointer type
// Otherwise, it's set to false
template <class T>
struct IsPointer
{
    enum
    {
        IsTrue = false
    };
};

template <class T>
struct IsPointer<T*>
{
    enum
    {
        IsTrue = true
    };
};

// Trick adopted from WinRT/WinTypes/Value.h
template <class T1, class T2>
struct IsSame
{
    enum
    {
        IsTrue = false
    };
};

template <class T1>
struct IsSame<T1, T1>
{
    enum
    {
        IsTrue = true
    };
};

// From Legacy engine - don't use

#define AssertPvCb(pv, cb)       AssertMsg(0 != (pv) || 0 == (cb), "bad ptr")
#define AssertPvCbN(pv, cb)      //NO-OP
#define AssertPvCbR(pv, cb)      AssertMsg(0 != (pv) || 0 == (cb), "bad ptr")
#define AssertPsz(psz)           AssertMsg(0 != (psz), "bad psz")
#define AssertPszN(psz)          //NO-OP

#define AssertMem(pvar)          AssertPvCb(pvar, sizeof(*(pvar)))
#define AssertMemN(pvar)         AssertPvCbN(pvar, sizeof(*(pvar)))
#define AssertMemR(pvar)         AssertPvCbR(pvar, sizeof(*(pvar)))
#define AssertArrMem(prgv, cv)   AssertPvCb(prgv, (cv) * sizeof(*(prgv)))
#define AssertArrMemR(prgv, cv)  AssertPvCbR(prgv, (cv) * sizeof(*(prgv)))
#define AssertThis()             Assert(0 != (this) && (this)->AssertValid())
