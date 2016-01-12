//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "StdAfx.h"

HostConfigFlags HostConfigFlags::flags;
LPWSTR* HostConfigFlags::argsVal;
int HostConfigFlags::argsCount;
void(__stdcall *HostConfigFlags::pfnPrintUsage)();

template <>
void HostConfigFlags::Parse<bool>(ICmdLineArgsParser * parser, bool * value)
{
    *value = parser->GetCurrentBoolean();
}

template <>
void HostConfigFlags::Parse<int>(ICmdLineArgsParser * parser, int* value)
{
    try
    {
        *value = parser->GetCurrentInt();
    }
    catch (...)
    {
        // Don't do anything, *value will remain its default value.
    }
}

template <>
void HostConfigFlags::Parse<BSTR>(ICmdLineArgsParser * parser, BSTR * bstr)
{
    if (*bstr != NULL)
    {
        SysFreeString(*bstr);
    }
    *bstr = parser->GetCurrentString();
    if (*bstr == NULL)
    {
        *bstr = SysAllocString(L"");
    }
}

HostConfigFlags::HostConfigFlags() :
#define FLAG(Type, Name, Desc, Default) \
    Name##IsEnabled(false), \
    Name(Default),
#include "HostConfigFlagsList.h"
    nDummy(0)
{
}

bool HostConfigFlags::ParseFlag(LPCWSTR flagsString, ICmdLineArgsParser * parser)
{
#define FLAG(Type, Name, Desc, Default) \
    if (_wcsicmp(L ## #Name, flagsString) == 0) \
    { \
        this->Name##IsEnabled = true; \
        Parse<Type>(parser, &this->Name); \
        return true; \
    }
#include "HostConfigFlagsList.h"
    return false;
}

void HostConfigFlags::PrintUsageString()
{
#define FLAG(Type, Name, Desc, Default) \
    wprintf(L"%20ls          \t%ls\n", L ## #Name, L ## #Desc);
#include "HostConfigFlagsList.h"
}

void HostConfigFlags::PrintUsage()
{
    if (pfnPrintUsage)
    {
        pfnPrintUsage();
    }

    wprintf(L"\nFlag List : \n");
    HostConfigFlags::PrintUsageString();
    ChakraRTInterface::PrintConfigFlagsUsageString();
}

void HostConfigFlags::HandleArgsFlag(int& argc, _Inout_updates_to_(argc, argc) LPWSTR argv[])
{
    const LPCWSTR argsFlag = L"-args";
    const LPCWSTR endArgsFlag = L"-endargs";
    int argsFlagLen = static_cast<int>(wcslen(argsFlag));
    int i;
    for (i = 1; i < argc; i++)
    {
        if (_wcsnicmp(argv[i], argsFlag, argsFlagLen) == 0)
        {
            break;
        }
    }
    int argsStart = ++i;
    for (; i < argc; i++)
    {
        if (_wcsnicmp(argv[i], endArgsFlag, argsFlagLen) == 0)
        {
            break;
        }
    }
    int argsEnd = i;

    int argsCount = argsEnd - argsStart;
    if (argsCount == 0)
    {
        return;
    }
    HostConfigFlags::argsVal = new LPWSTR[argsCount];
    HostConfigFlags::argsCount = argsCount;
    int argIndex = argsStart;
    for (i = 0; i < argsCount; i++)
    {
        HostConfigFlags::argsVal[i] = argv[argIndex++];
    }

    argIndex = argsStart - 1;
    for (i = argsEnd + 1; i < argc; i++)
    {
        argv[argIndex++] = argv[i];
    }
    Assert(argIndex == argc - argsCount - 1 - (argsEnd < argc));
    argc = argIndex;
}
