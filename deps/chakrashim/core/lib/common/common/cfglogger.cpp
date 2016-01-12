//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonCommonPch.h"

#ifdef CONTROL_FLOW_GUARD_LOGGER
#include "common\CFGLogger.h"
#include "core\DbgHelpSymbolManager.h"

// Initialization order
//  AB AutoSystemInfo
//  AD PerfCounter
//  AE PerfCounterSet
//  AM Output/Configuration
//  AN MemProtectHeap
//  AP DbgHelpSymbolManager
//  AQ CFGLogger
//  AR LeakReport
//  AS JavascriptDispatch/RecyclerObjectDumper
//  AT HeapAllocator/RecyclerHeuristic
//  AU RecyclerWriteBarrierManager
#pragma warning(disable:4075)       // initializers put in unrecognized initialization area on purpose
#pragma init_seg(".CRT$XCAQ")

bool CFGLogger::inGuard;
CriticalSection CFGLogger::cs;
JsUtil::BaseDictionary<uintptr_t, uint, NoCheckHeapAllocator> CFGLogger::guardCheckRecord(&NoCheckHeapAllocator::Instance);
CFGLogger CFGLogger::Instance;
CFGLogger::PfnGuardCheckFunction CFGLogger::oldGuardCheck;

extern "C" PVOID __guard_check_icall_fptr;

void
CFGLogger::Enable()
{
    DWORD oldProtect;
    ::VirtualProtect(&__guard_check_icall_fptr, sizeof(void *), PAGE_READWRITE, &oldProtect);
    oldGuardCheck = (PfnGuardCheckFunction)__guard_check_icall_fptr;
    __guard_check_icall_fptr = &CFGLogger::GuardCheck;
    ::VirtualProtect(&__guard_check_icall_fptr, sizeof(void *), oldProtect, &oldProtect);
}

CFGLogger::~CFGLogger()
{
    if (oldGuardCheck)
    {
        DWORD oldProtect;
        ::VirtualProtect(&__guard_check_icall_fptr, sizeof(void *), PAGE_READWRITE, &oldProtect);
        __guard_check_icall_fptr = (PVOID)oldGuardCheck;
        ::VirtualProtect(&__guard_check_icall_fptr, sizeof(void *), oldProtect, &oldProtect);

        DbgHelpSymbolManager::EnsureInitialized();
        size_t total = 0;
        guardCheckRecord.Map([&total](uintptr_t Target, uint count)
        {
            DbgHelpSymbolManager::PrintSymbol((PVOID)Target);
            Output::Print(L"%8d\n", count);
            total += count;
        });
        Output::Print(L"Total: %d\n", total);
        Output::Flush();
    }
}

#ifdef _CONTROL_FLOW_GUARD
__declspec(guard(ignore))
#endif
void __fastcall
CFGLogger::GuardCheck(_In_ uintptr_t Target)
{
    if (Target >= AutoSystemInfo::Data.dllLoadAddress && Target < AutoSystemInfo::Data.dllHighAddress)
    {
        AutoCriticalSection autocs(&cs);
        if (inGuard) { return; }
        inGuard = true;
        uint * count;
        if (guardCheckRecord.TryGetReference(Target, &count))
        {
            (*count)++;
        }
        else
        {
            guardCheckRecord.AddNew(Target, 1);
        }
        inGuard = false;
    }
    oldGuardCheck(Target);
}
#endif

