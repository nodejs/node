//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonCorePch.h"
#include "core\DelayLoadLibrary.h"

DelayLoadLibrary::DelayLoadLibrary()
{
    m_hModule = nullptr;
    m_isInit = false;
}

DelayLoadLibrary::~DelayLoadLibrary()
{
    if (m_hModule)
    {
        FreeLibrary(m_hModule);
        m_hModule = nullptr;
    }
}

void DelayLoadLibrary::Ensure(DWORD dwFlags)
{
    if (!m_isInit)
    {
        m_hModule = LoadLibraryEx(GetLibraryName(), nullptr, dwFlags);
        m_isInit = true;
    }
}

void DelayLoadLibrary::EnsureFromSystemDirOnly()
{
    Ensure(LOAD_LIBRARY_SEARCH_SYSTEM32);
}


FARPROC DelayLoadLibrary::GetFunction(__in LPCSTR lpFunctionName)
{
    if (m_hModule)
    {
        return GetProcAddress(m_hModule, lpFunctionName);
    }

    return nullptr;
}

bool DelayLoadLibrary::IsAvailable()
{
    return m_hModule != nullptr;
}

#if PDATA_ENABLED

static NtdllLibrary NtdllLibraryObject;
NtdllLibrary* NtdllLibrary::Instance = &NtdllLibraryObject;

LPCTSTR NtdllLibrary::GetLibraryName() const
{
    return L"ntdll.dll";
}

_Success_(return == 0)
DWORD NtdllLibrary::AddGrowableFunctionTable( _Out_ PVOID * DynamicTable,
    _In_reads_(MaximumEntryCount) PRUNTIME_FUNCTION FunctionTable,
    _In_ DWORD EntryCount,
    _In_ DWORD MaximumEntryCount,
    _In_ ULONG_PTR RangeBase,
    _In_ ULONG_PTR RangeEnd )
{
    if(m_hModule)
    {
        if(addGrowableFunctionTable == NULL)
        {
            addGrowableFunctionTable = (PFnRtlAddGrowableFunctionTable)GetFunction("RtlAddGrowableFunctionTable");
            if(addGrowableFunctionTable == NULL)
            {
                Assert(false);
                return 1;
            }
        }
        return addGrowableFunctionTable(DynamicTable,
            FunctionTable,
            EntryCount,
            MaximumEntryCount,
            RangeBase,
            RangeEnd);
    }
    return 1;
}

VOID NtdllLibrary::DeleteGrowableFunctionTable( _In_ PVOID DynamicTable )
{
    if(m_hModule)
    {
        if(deleteGrowableFunctionTable == NULL)
        {
            deleteGrowableFunctionTable = (PFnRtlDeleteGrowableFunctionTable)GetFunction("RtlDeleteGrowableFunctionTable");
            if(deleteGrowableFunctionTable == NULL)
            {
                Assert(false);
                return;
            }
        }
        deleteGrowableFunctionTable(DynamicTable);
    }
}

VOID NtdllLibrary::GrowFunctionTable(_Inout_ PVOID DynamicTable, _In_ ULONG NewEntryCount)
{
    if (m_hModule)
    {
        if (growFunctionTable == nullptr)
        {
            growFunctionTable = (PFnRtlGrowFunctionTable)GetFunction("RtlGrowFunctionTable");
            if (growFunctionTable == nullptr)
            {
                Assert(false);
                return;
            }
        }

        growFunctionTable(DynamicTable, NewEntryCount);
    }
}
#endif
