//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "Runtime.h"
#include "TestHooks.h"

#ifdef ENABLE_TEST_HOOKS

HRESULT __stdcall SetConfigFlags(__in int argc, __in_ecount(argc) LPWSTR argv[], ICustomConfigFlags* customConfigFlags)
{
    CmdLineArgsParser parser(customConfigFlags);
    if (parser.Parse(argc, argv) != 0)
    {
        return E_FAIL;
    }

    return S_OK;
}

HRESULT __stdcall PrintConfigFlagsUsageString()
{
    Js::ConfigFlagsTable::PrintUsageString();
    return S_OK;
}

HRESULT __stdcall SetAssertToConsoleFlag(bool flag)
{
#ifdef DBG
    AssertsToConsole = flag;
#endif
    return S_OK;
}

HRESULT __stdcall SetEnableCheckMemoryLeakOutput(bool flag)
{
#if defined(CHECK_MEMORY_LEAK)
    MemoryLeakCheck::SetEnableOutput(flag);
#endif
    return S_OK;
}

void __stdcall NotifyUnhandledException(PEXCEPTION_POINTERS exceptionInfo)
{
#ifdef GENERATE_DUMP
    // We already reported assert at the assert point, don't do it here again
    if (exceptionInfo->ExceptionRecord->ExceptionCode != STATUS_ASSERTION_FAILURE)
    {
        if (Js::Configuration::Global.flags.IsEnabled(Js::DumpOnCrashFlag))
        {
            Js::Throw::GenerateDump(exceptionInfo, Js::Configuration::Global.flags.DumpOnCrash);
        }
    }
#endif
}

#define FLAG(type, name, description, defaultValue, ...) FLAG_##type##(name)
#define FLAG_String(name) \
    bool IsEnabled##name##Flag() \
    { \
        return Js::Configuration::Global.flags.IsEnabled(Js::##name##Flag); \
    } \
    HRESULT __stdcall Get##name##Flag(BSTR *flag) \
    { \
        *flag = SysAllocString(Js::Configuration::Global.flags.##name##); \
        return (*flag == NULL ? E_OUTOFMEMORY : S_OK); \
    } \
    HRESULT __stdcall Set##name##Flag(BSTR flag) \
    { \
        Js::Configuration::Global.flags.##name = flag; \
        return S_OK; \
    }
#define FLAG_Boolean(name) \
    bool IsEnabled##name##Flag() \
    { \
        return Js::Configuration::Global.flags.IsEnabled(Js::##name##Flag); \
    } \
    HRESULT __stdcall Get##name##Flag(bool *flag) \
    { \
        *flag = Js::Configuration::Global.flags.##name##; \
        return S_OK; \
    } \
    HRESULT __stdcall Set##name##Flag(bool flag) \
    { \
        Js::Configuration::Global.flags.##name = flag; \
        return S_OK; \
    }
#define FLAG_Number(name) \
    bool IsEnabled##name##Flag() \
    { \
        return Js::Configuration::Global.flags.IsEnabled(Js::##name##Flag); \
    } \
    HRESULT __stdcall Get##name##Flag(int *flag) \
    { \
        *flag = Js::Configuration::Global.flags.##name##; \
        return S_OK; \
    } \
    HRESULT __stdcall Set##name##Flag(int flag) \
    { \
        Js::Configuration::Global.flags.##name = flag; \
        return S_OK; \
    }
// skipping other types
#define FLAG_Phases(name)
#define FLAG_NumberSet(name)
#define FLAG_NumberPairSet(name)
#define FLAG_NumberRange(name)
#include "ConfigFlagsList.h"
#undef FLAG
#undef FLAG_String
#undef FLAG_Boolean
#undef FLAG_Number
#undef FLAG_Phases
#undef FLAG_NumberSet
#undef FLAG_NumberPairSet
#undef FLAG_NumberRange

HRESULT OnChakraCoreLoaded()
{
    typedef HRESULT(__stdcall *OnChakraCoreLoadedPtr)(TestHooks &testHooks);
    OnChakraCoreLoadedPtr pfChakraCoreLoaded = (OnChakraCoreLoadedPtr)GetProcAddress(GetModuleHandle(NULL), "OnChakraCoreLoadedEntry");
    if (pfChakraCoreLoaded == nullptr)
    {
        return S_OK;
    }

    TestHooks testHooks =
    {
        SetConfigFlags,
        PrintConfigFlagsUsageString,
        SetAssertToConsoleFlag,
        SetEnableCheckMemoryLeakOutput,

#define FLAG(type, name, description, defaultValue, ...) FLAG_##type##(name)
#define FLAGINCLUDE(name) \
    IsEnabled##name##Flag, \
    Get##name##Flag, \
    Set##name##Flag,
#define FLAG_String(name) FLAGINCLUDE(name)
#define FLAG_Boolean(name) FLAGINCLUDE(name)
#define FLAG_Number(name) FLAGINCLUDE(name)
#define FLAG_Phases(name)
#define FLAG_NumberSet(name)
#define FLAG_NumberPairSet(name)
#define FLAG_NumberRange(name)
#include "ConfigFlagsList.h"
#undef FLAG
#undef FLAG_String
#undef FLAG_Boolean
#undef FLAG_Number
#undef FLAG_Phases
#undef FLAG_NumberSet
#undef FLAG_NumberPairSet
#undef FLAG_NumberRange

        NotifyUnhandledException
    };

    return pfChakraCoreLoaded(testHooks);
}

#endif // ENABLE_TEST_HOOKS
