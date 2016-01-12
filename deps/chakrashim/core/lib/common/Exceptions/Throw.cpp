//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#include "CommonExceptionsPch.h"
// === C Runtime Header Files ===
#pragma warning(push)
#pragma warning(disable: 4995) /* 'function': name was marked as #pragma deprecated */
#include <strsafe.h>
#pragma warning(pop)

#include "StackOverflowException.h"
#include "AsmJsParseException.h"
#include "InternalErrorException.h"
#include "OutOfMemoryException.h"
#include "NotImplementedException.h"

// Header files required before including ConfigFlagsTable.h

#include "EnumHelp.h"
#include "Common\MathUtil.h"
#include "Core\AllocSizeMath.h"
#include "Core\FaultInjection.h"

#include "core\BasePtr.h"
#include "core\AutoFILE.h"
#include "core\Output.h"

// Memory Management
namespace Memory {}
using namespace Memory;
#include "Memory\Allocator.h"
#include "Memory\HeapAllocator.h"

// Data structure
#include "DataStructures\Comparer.h"
#include "DataStructures\SizePolicy.h"
#include "DataStructures\SList.h"
#include "DataStructures\KeyValuePair.h"
#include "DataStructures\BaseDictionary.h"
#include "core\ConfigFlagsTable.h"

#include "core\StackBackTrace.h"



// dbghelp.h is not clean with warning 4091
#pragma warning(push)
#pragma warning(disable: 4091) /* warning C4091: 'typedef ': ignored on left of '' when no variable is declared */
#include <dbghelp.h>
#pragma warning(pop)

extern "C"{
    BOOLEAN IsMessageBoxWPresent();
}

namespace Js {
#ifdef GENERATE_DUMP
    StackBackTrace * Throw::stackBackTrace = nullptr;
#endif
    void Throw::FatalInternalError()
    {
        int scenario = 2;
        ReportFatalException(NULL, E_FAIL, Fatal_Internal_Error, scenario);
    }

    void Throw::FatalProjectionError()
    {
        RaiseException((DWORD)DBG_TERMINATE_PROCESS, EXCEPTION_NONCONTINUABLE, 0, NULL);
    }

    void Throw::InternalError()
    {
        AssertMsg(false, "Internal error!!");
        throw InternalErrorException();
    }

    void Throw::OutOfMemory()
    {
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        if (CONFIG_FLAG(PrintSystemException))
        {
            Output::Print(L"SystemException: OutOfMemory\n");
            Output::Flush();
        }
#endif
        if (JsUtil::ExternalApi::RaiseOutOfMemoryIfScriptActive())
        {
            AssertMsg(false, "We shouldn't be here");
        }
        throw OutOfMemoryException();
    }
    void Throw::CheckAndThrowOutOfMemory(BOOLEAN status)
    {
        if (!status)
        {
            OutOfMemory();
        }
    }
    void Throw::StackOverflow(ScriptContext *scriptContext, PVOID returnAddress)
    {
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        if (CONFIG_FLAG(PrintSystemException))
        {
            Output::Print(L"SystemException: StackOverflow\n");
            Output::Flush();
        }
#endif
        if (JsUtil::ExternalApi::RaiseStackOverflowIfScriptActive(scriptContext, returnAddress))
        {
            AssertMsg(false, "We shouldn't be here");
        }
        throw StackOverflowException();
    }

    void Throw::NotImplemented()
    {
        AssertMsg(false, "This functionality is not yet implemented");

        throw NotImplementedException();
    }

    // Returns true when the process is either TE.exe or TE.processhost.exe
    bool Throw::IsTEProcess()
    {
        wchar_t fileName[_MAX_PATH];
        wchar_t moduleName[_MAX_PATH];
        GetModuleFileName(0, moduleName, _MAX_PATH);
        errno_t err = _wsplitpath_s(moduleName, nullptr, 0, nullptr, 0, fileName, _MAX_PATH, nullptr, 0);

        return err == 0 && _wcsnicmp(fileName, L"TE", 2) == 0;
    }

    void Throw::GenerateDumpAndTerminateProcess(PEXCEPTION_POINTERS exceptInfo)
    {
        if (!Throw::IsTEProcess()
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
            && !Js::Configuration::Global.flags.IsEnabled(Js::DumpOnCrashFlag)
#endif
            )
        {
            return;
        }

#ifdef GENERATE_DUMP
        Js::Throw::GenerateDump(exceptInfo, Js::Configuration::Global.flags.DumpOnCrash);
#endif

        // For now let's terminate the process.
        TerminateProcess(GetCurrentProcess(), (UINT)DBG_TERMINATE_PROCESS);
    }

#ifdef GENERATE_DUMP
    CriticalSection Throw::csGenereateDump;
    void Throw::GenerateDump(LPCWSTR filePath, bool terminate, bool needLock)
    {
        __try
        {
            if (terminate)
            {
                RaiseException((DWORD)DBG_TERMINATE_PROCESS, EXCEPTION_NONCONTINUABLE, 0, NULL);
            }
            else
            {
                RaiseException(0, 0, 0, NULL);
            }
        }
        __except(Throw::GenerateDump(GetExceptionInformation(), filePath,
            terminate? EXCEPTION_CONTINUE_SEARCH : EXCEPTION_EXECUTE_HANDLER), needLock)
        {
            // we don't do anything interesting in this handler
        }
    }

    void Throw::GenerateDumpForAssert(LPCWSTR filePath)
    {
        __try
        {
            RaiseException(STATUS_ASSERTION_FAILURE, EXCEPTION_NONCONTINUABLE, 0, NULL);
        }
        __except (Throw::GenerateDump(GetExceptionInformation(), filePath, EXCEPTION_CONTINUE_SEARCH), false)
        {
            // no-op
        }
    }

    int Throw::GenerateDump(PEXCEPTION_POINTERS exceptInfo, LPCWSTR filePath, int ret, bool needLock)
    {
        WCHAR tempFilePath[MAX_PATH];
        WCHAR tempFileName[MAX_PATH];
        HANDLE hTempFile;
        DWORD retVal;

        if (filePath == NULL)
        {
            retVal = GetTempPath(MAX_PATH, tempFilePath);

            if (retVal > MAX_PATH || (retVal == 0))
            {
                return ret;
            }
            filePath = tempFilePath;
        }

        StringCchPrintf(tempFileName, _countof(tempFileName), L"%s\\CH_%u_%u.dmp", filePath, GetCurrentProcessId(), GetCurrentThreadId());
        Output::Print(L"dump filename %s \n", tempFileName);
        Output::Flush();

        hTempFile = CreateFile(tempFileName, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL, NULL);
        if (hTempFile == INVALID_HANDLE_VALUE)
        {
            return GetLastError();
        }

        MINIDUMP_EXCEPTION_INFORMATION dumpExceptInfo;
        dumpExceptInfo.ThreadId = GetCurrentThreadId();
        dumpExceptInfo.ExceptionPointers = exceptInfo;
        dumpExceptInfo.ClientPointers = FALSE;

        {
            MINIDUMP_TYPE dumpType = static_cast<MINIDUMP_TYPE>(MiniDumpWithDataSegs | MiniDumpWithPrivateReadWriteMemory);

            // Generating full dump for the TE process (reason : it contains both managed and native memory)
            if (CONFIG_FLAG(FullMemoryDump))
            {
                dumpType = static_cast<MINIDUMP_TYPE>(dumpType | MiniDumpWithFullMemory);
            }

            BOOL dumpGenerated = false;
            if (needLock)
            {
                // the critical section might have been destructed at process shutdown time. At that time we don't need
                // to lock.
                AutoCriticalSection autocs(&csGenereateDump);

                dumpGenerated = MiniDumpWriteDump(GetCurrentProcess(),
                    GetCurrentProcessId(),
                    hTempFile,
                    dumpType,
                    &dumpExceptInfo,
                    NULL,
                    NULL);
            }
            else
            {
                dumpGenerated = MiniDumpWriteDump(GetCurrentProcess(),
                    GetCurrentProcessId(),
                    hTempFile,
                    dumpType,
                    &dumpExceptInfo,
                    NULL,
                    NULL);
            }
            if (!dumpGenerated)
            {
                Output::Print(L"Unable to write minidump (0x%08X)\n", GetLastError());
                Output::Flush();
            }
        }
        FlushFileBuffers(hTempFile);
        CloseHandle(hTempFile);
        return ret;
    }
#endif // GENERATE_DUMP

#if DBG
    // After assert the program should terminate. Sometime we saw the program continue somehow
    // log the existence of assert for debugging.
    void Throw::LogAssert()
    {
        IsInAssert = true;
        // This should be the last thing to happen in the process. Therefore, leaks are not an issue.
        stackBackTrace = StackBackTrace::Capture(&NoCheckHeapAllocator::Instance, Throw::StackToSkip, Throw::StackTraceDepth);
    }

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    static const wchar_t * caption = L"CHAKRA ASSERT";
#endif

    bool Throw::ReportAssert(__in LPSTR fileName, uint lineNumber, __in LPSTR error, __in LPSTR message)
    {
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        if (Js::Configuration::Global.flags.IsEnabled(Js::AssertBreakFlag))
        {
            DebugBreak();
            return false;
        }
        if (Js::Configuration::Global.flags.IsEnabled(Js::AssertIgnoreFlag))
        {
            return true;
        }
#endif
        if (AssertsToConsole)
        {
            fprintf(stderr, "ASSERTION %u: (%s, line %u) %s\n Failure: %s\n", GetCurrentProcessId(), fileName, lineNumber, message, error);
            fflush(stderr);
#ifdef GENERATE_DUMP
            // force dump if we have assert in jc.exe. check build only.
            if (!Js::Configuration::Global.flags.IsEnabled(Js::DumpOnCrashFlag))
            {
                return false;
            }
            Throw::GenerateDumpForAssert(Js::Configuration::Global.flags.DumpOnCrash);
#else
            return false;
#endif
        }
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        // Then if DumpOncrashFlag is not specified it directly returns,
        // otherwise if will raise a non-continuable exception, generate the dump and terminate the process.
        // the popup message box might be useful when testing in IE
        if (Js::Configuration::Global.flags.AssertPopUp && IsMessageBoxWPresent())
        {
            wchar_t buff[1024];

            swprintf_s(buff, _countof(buff), L"%S (%u)\n%S\n%S", fileName, lineNumber, message, error);
            buff[_countof(buff)-1] = 0;

            int ret = MessageBox(nullptr, buff, caption, MB_ABORTRETRYIGNORE);

            switch (ret)
            {
            case IDIGNORE:
                return true;
            case IDABORT:
                Throw::FatalInternalError();
            default:
                return false;
            }
        }
#endif
        return false;
    }

#endif

} // namespace Js
