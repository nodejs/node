//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#if PDATA_ENABLED

class PDataManager
{
public:
    static void RegisterPdata(RUNTIME_FUNCTION* pdataStart, _In_ const ULONG_PTR functionStart, _In_ const ULONG_PTR functionEnd, _Out_ PVOID* pdataTable, ULONG entryCount = 1, ULONG maxEntryCount = 1);
    static void UnregisterPdata(RUNTIME_FUNCTION* pdata);
};

#endif
