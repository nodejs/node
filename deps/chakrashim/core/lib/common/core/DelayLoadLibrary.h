//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

class DelayLoadLibrary
{
protected:
    HMODULE m_hModule;
    bool m_isInit;

public:
    DelayLoadLibrary();
    virtual ~DelayLoadLibrary();

    virtual LPCTSTR GetLibraryName() const = 0;

    FARPROC GetFunction(__in LPCSTR lpFunctionName);

    void EnsureFromSystemDirOnly();
    bool IsAvailable();
private:
    void Ensure(DWORD dwFlags = 0);

};

#if PDATA_ENABLED

// This needs to be delay loaded because it is available on
// Win8 only
class NtdllLibrary : protected DelayLoadLibrary
{
private:
    typedef _Success_(return == 0) DWORD (NTAPI *PFnRtlAddGrowableFunctionTable)(_Out_ PVOID * DynamicTable,
        _In_reads_(MaximumEntryCount) PRUNTIME_FUNCTION FunctionTable,
        _In_ DWORD EntryCount,
        _In_ DWORD MaximumEntryCount,
        _In_ ULONG_PTR RangeBase,
        _In_ ULONG_PTR RangeEnd);
    PFnRtlAddGrowableFunctionTable addGrowableFunctionTable;

    typedef VOID (NTAPI *PFnRtlDeleteGrowableFunctionTable)(_In_ PVOID DynamicTable);
    PFnRtlDeleteGrowableFunctionTable deleteGrowableFunctionTable;

    typedef VOID (NTAPI *PFnRtlGrowFunctionTable)(_Inout_ PVOID DynamicTable, _In_ ULONG NewEntryCount);
    PFnRtlGrowFunctionTable growFunctionTable;

public:
    static NtdllLibrary* Instance;

    NtdllLibrary() : DelayLoadLibrary(),
        addGrowableFunctionTable(NULL),
        deleteGrowableFunctionTable(NULL),
        growFunctionTable(NULL)
    {
        this->EnsureFromSystemDirOnly();
    }

    LPCTSTR GetLibraryName() const;

    _Success_(return == 0)
    DWORD AddGrowableFunctionTable(_Out_ PVOID * DynamicTable,
        _In_reads_(MaximumEntryCount) PRUNTIME_FUNCTION FunctionTable,
        _In_ DWORD EntryCount,
        _In_ DWORD MaximumEntryCount,
        _In_ ULONG_PTR RangeBase,
        _In_ ULONG_PTR RangeEnd);
    VOID DeleteGrowableFunctionTable(_In_ PVOID DynamicTable);
    VOID GrowFunctionTable(__inout PVOID DynamicTable, __in ULONG NewEntryCount);
};
#endif
