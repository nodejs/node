//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonExceptionsPch.h"

__inline void ReportFatalException(
    __in ULONG_PTR context,
    __in HRESULT exceptionCode,
    __in ErrorReason reasonCode,
    __in ULONG scenario)
{
    // avoid the error text methods to be optimized out.
    UNREFERENCED_PARAMETER(scenario);

    if (IsDebuggerPresent())
    {
        DebugBreak();
    }
    __try
    {
        ULONG_PTR ExceptionInformation[2];
        ExceptionInformation[0] = (ULONG_PTR)reasonCode;
        ExceptionInformation[1] = (ULONG_PTR)context;
        RaiseException(exceptionCode, EXCEPTION_NONCONTINUABLE, 2, (ULONG_PTR*)ExceptionInformation);
    }
    __except(FatalExceptionFilter(GetExceptionInformation()))
    {
    }
}

// Disable optimization make sure all the frames are still available in Dr. Watson bug reports.
#pragma optimize("", off)
__declspec(noinline) void JavascriptDispatch_OOM_fatal_error(
    __in ULONG_PTR context)
{
    int scenario = 1;
    ReportFatalException(context, E_OUTOFMEMORY, JavascriptDispatch_OUTOFMEMORY, scenario);
};

__declspec(noinline) void CustomHeap_BadPageState_fatal_error(
    __in ULONG_PTR context)
{
    int scenario = 1;
    ReportFatalException(context, E_UNEXPECTED, CustomHeap_MEMORYCORRUPTION, scenario);
};

__declspec(noinline) void MarkStack_OOM_fatal_error()
{
    int scenario = 1;
    ReportFatalException(NULL, E_OUTOFMEMORY, MarkStack_OUTOFMEMORY, scenario);
};

__declspec(noinline) void Amd64StackWalkerOutOfContexts_fatal_error(
    __in ULONG_PTR context)
{
    int scenario = 1;
    ReportFatalException(context, E_UNEXPECTED, Fatal_Amd64StackWalkerOutOfContexts, scenario);
}

__declspec(noinline) void FailedToBox_OOM_fatal_error(
    __in ULONG_PTR context)
{
    int scenario = 1;
    ReportFatalException(context, E_UNEXPECTED, Fatal_FailedToBox_OUTOFMEMORY, scenario);
}

#if defined(RECYCLER_WRITE_BARRIER) && defined(_M_X64_OR_ARM64)
__declspec(noinline) void X64WriteBarrier_OOM_fatal_error()
{
    int scenario = 3;
    ReportFatalException(NULL, E_OUTOFMEMORY, WriteBarrier_OUTOFMEMORY, scenario);
}
#endif

__declspec(noinline) void DebugHeap_OOM_fatal_error()
{
    int scenario = 3;
    ReportFatalException(NULL, E_OUTOFMEMORY, Fatal_Debug_Heap_OUTOFMEMORY, scenario);
}

__declspec(noinline) void Binary_Inconsistency_fatal_error()
{
    int scenario = 4;
    ReportFatalException(NULL, E_UNEXPECTED, Fatal_Binary_Inconsistency, scenario);
}

__declspec(noinline) void Version_Inconsistency_fatal_error()
{
    int scenario = 4;
    ReportFatalException(NULL, E_UNEXPECTED, Fatal_Version_Inconsistency, scenario);
}

#ifdef LARGEHEAPBLOCK_ENCODING
__declspec(noinline) void LargeHeapBlock_Metadata_Corrupted(
    __in ULONG_PTR context, __in unsigned char calculatedChecksum)
{
    int scenario = calculatedChecksum; /* For debugging purpose if checksum mismatch happen*/
    ReportFatalException(context, E_UNEXPECTED, LargeHeapBlock_Metadata_Corrupt, scenario);
};
#endif

__declspec(noinline) void FromDOM_NoScriptScope_fatal_error()
{
    int scenario = 5;
    ReportFatalException(NULL, E_UNEXPECTED, EnterScript_FromDOM_NoScriptScope, scenario);
}

#pragma optimize("",on)
