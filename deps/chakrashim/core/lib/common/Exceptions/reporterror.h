//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

enum ErrorReason
{
    JavascriptDispatch_OUTOFMEMORY = 1,
    Fatal_Internal_Error = 2,
    Fatal_Debug_Heap_OUTOFMEMORY = 3,
    Fatal_Amd64StackWalkerOutOfContexts = 4,
    // Unused = 5,
    Fatal_Binary_Inconsistency = 6,
    WriteBarrier_OUTOFMEMORY = 7,
    CustomHeap_MEMORYCORRUPTION = 8,
    LargeHeapBlock_Metadata_Corrupt = 9,
    Fatal_Version_Inconsistency = 10,
    MarkStack_OUTOFMEMORY = 11,
    Fatal_FailedToBox_OUTOFMEMORY = 12,
    EnterScript_FromDOM_NoScriptScope = 13
};

extern "C" void ReportFatalException(
    __in ULONG_PTR context,
    __in HRESULT exceptionCode,
    __in ErrorReason reasonCode,
    __in ULONG scenario);

// We can have other error handle code path with
// unique call stack so we can collect data in Dr. Watson.
void JavascriptDispatch_OOM_fatal_error(
    __in ULONG_PTR context);

void CustomHeap_BadPageState_fatal_error(
    __in ULONG_PTR context);

void Amd64StackWalkerOutOfContexts_fatal_error(
    __in ULONG_PTR context);

void FailedToBox_OOM_fatal_error(
    __in ULONG_PTR context);

#if defined(RECYCLER_WRITE_BARRIER) && defined(_M_X64_OR_ARM64)
void X64WriteBarrier_OOM_fatal_error();
#endif

void DebugHeap_OOM_fatal_error();

void MarkStack_OOM_fatal_error();

void Binary_Inconsistency_fatal_error();
void Version_Inconsistency_fatal_error();

#ifdef LARGEHEAPBLOCK_ENCODING
void LargeHeapBlock_Metadata_Corrupted(
    __in ULONG_PTR context, __in unsigned char calculatedCheckSum);
#endif

void FromDOM_NoScriptScope_fatal_error();

// RtlReportException is available on Vista and up, but we cannot use it for OOB release.
// Use UnhandleExceptionFilter to let the default handler handles it.
__inline LONG FatalExceptionFilter(
    __in LPEXCEPTION_POINTERS lpep)
{
    LONG rc = UnhandledExceptionFilter(lpep);

    // re == EXCEPTION_EXECUTE_HANDLER means there is no debugger attached, let's terminate
    // the process. Otherwise give control to the debugger.
    // Note: in case when postmortem debugger is registered but no actual debugger attached,
    //       rc will be 0 (and EXCEPTION_EXECUTE_HANDLER is 1), so it acts as if there is debugger attached.
    if (rc == EXCEPTION_EXECUTE_HANDLER)
    {
        TerminateProcess(GetCurrentProcess(), (UINT)DBG_TERMINATE_PROCESS);
    }
    else
    {
        Assert(IsDebuggerPresent());
        DebugBreak();
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

template<class Fn>
static STDMETHODIMP DebugApiWrapper(Fn fn)
{
    // If an assertion or AV is hit, it triggers a SEH exception. SEH exceptions escaped here will be eaten by PDM. To prevent assertions
    // from getting unnoticed, we install a SEH exception filter and crash the process.
#if ENABLE_DEBUG_API_WRAPPER
    __try
    {
#endif
        return fn();
#if ENABLE_DEBUG_API_WRAPPER
    }
    __except(FatalExceptionFilter(GetExceptionInformation()))
    {
    }
    return E_FAIL;
#endif
}
