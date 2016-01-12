//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#ifdef ENABLE_TEST_HOOKS

HRESULT OnChakraCoreLoaded();
interface ICustomConfigFlags;

struct TestHooks
{
    typedef HRESULT(__stdcall *SetConfigFlagsPtr)(int argc, LPWSTR argv[], ICustomConfigFlags* customConfigFlags);
    typedef HRESULT(__stdcall *PrintConfigFlagsUsageStringPtr)(void);
    typedef HRESULT(__stdcall *SetAssertToConsoleFlagPtr)(bool flag);
    typedef HRESULT(__stdcall *SetEnableCheckMemoryLeakOutputPtr)(bool flag);
    typedef void(__stdcall * NotifyUnhandledExceptionPtr)(PEXCEPTION_POINTERS exceptionInfo);

    SetConfigFlagsPtr pfSetConfigFlags;
    PrintConfigFlagsUsageStringPtr pfPrintConfigFlagsUsageString;
    SetAssertToConsoleFlagPtr pfSetAssertToConsoleFlag;
    SetEnableCheckMemoryLeakOutputPtr pfSetEnableCheckMemoryLeakOutput;

#define FLAG(type, name, description, defaultValue, ...) FLAG_##type##(name)
#define FLAG_String(name) \
    bool (__stdcall *pfIsEnabled##name##Flag)(); \
    HRESULT (__stdcall *pfGet##name##Flag)(BSTR *flag); \
    HRESULT (__stdcall *pfSet##name##Flag)(BSTR flag);
#define FLAG_Boolean(name) \
    bool (__stdcall *pfIsEnabled##name##Flag)(); \
    HRESULT (__stdcall *pfGet##name##Flag)(bool *flag); \
    HRESULT (__stdcall *pfSet##name##Flag)(bool flag);
#define FLAG_Number(name) \
    bool (__stdcall *pfIsEnabled##name##Flag)(); \
    HRESULT (__stdcall *pfGet##name##Flag)(int *flag); \
    HRESULT (__stdcall *pfSet##name##Flag)(int flag);
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

    NotifyUnhandledExceptionPtr pfnNotifyUnhandledException;
};

#endif
