//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "BackEnd.h"

// Conditionally-compiled on x64 and arm
#if PDATA_ENABLED

void PDataManager::RegisterPdata(RUNTIME_FUNCTION* pdataStart, _In_ const ULONG_PTR functionStart, _In_ const ULONG_PTR functionEnd, _Out_ PVOID* pdataTable, ULONG entryCount, ULONG maxEntryCount)
{
    BOOLEAN success = FALSE;
    if (AutoSystemInfo::Data.IsWin8OrLater())
    {
        Assert(pdataTable != NULL);

        // Since we do not expect many thunk functions to be created, we are using 1 table/function
        // for now. This can be optimized further if needed.
        DWORD status = NtdllLibrary::Instance->AddGrowableFunctionTable(pdataTable,
            pdataStart,
            entryCount,
            maxEntryCount,
            /*RangeBase*/ functionStart,
            /*RangeEnd*/ functionEnd);
        success = NT_SUCCESS(status);
        if (success)
        {
            Assert(pdataTable);
        }
    }
    else
    {
        *pdataTable = pdataStart;
        success = RtlAddFunctionTable(pdataStart, entryCount, functionStart);
    }
    Js::Throw::CheckAndThrowOutOfMemory(success);
}

void PDataManager::UnregisterPdata(RUNTIME_FUNCTION* pdata)
{
    if (AutoSystemInfo::Data.IsWin8OrLater())
    {
        NtdllLibrary::Instance->DeleteGrowableFunctionTable(pdata);
    }
    else
    {
        BOOLEAN success = RtlDeleteFunctionTable(pdata);
        Assert(success);
    }
}
#endif
