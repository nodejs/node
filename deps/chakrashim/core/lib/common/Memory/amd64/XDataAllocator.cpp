//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonMemoryPch.h"

// This one works only for x64
#if !defined(_M_X64)
CompileAssert(false)
#endif

#include "XDataAllocator.h"
#include "core\DelayLoadLibrary.h"

XDataAllocator::XDataAllocator(BYTE* address, uint size) :
    freeList(nullptr),
    start(address),
    current(address),
    size(size),
    pdataEntries(nullptr),
    functionTableHandles(nullptr)
{
#ifdef RECYCLER_MEMORY_VERIFY
        memset(this->start, Recycler::VerifyMemFill, this->size);
#endif
    Assert(size > 0);
    Assert(address != nullptr);
}

bool XDataAllocator::Initialize(void* segmentStart, void* segmentEnd)
{
    Assert(segmentEnd > segmentStart);
    Assert(this->pdataEntries == nullptr);
    Assert(this->functionTableHandles == nullptr);

    bool success = true;

    this->pdataEntries = HeapNewNoThrowArrayZ(RUNTIME_FUNCTION, GetTotalPdataCount());
    success = this->pdataEntries != nullptr;
    if(success && AutoSystemInfo::Data.IsWin8OrLater())
    {
        this->functionTableHandles = HeapNewNoThrowArrayZ(FunctionTableHandle,  GetTotalPdataCount());
        success = this->functionTableHandles != nullptr;
    }
    return success;
}

XDataAllocator::~XDataAllocator()
{
    if(this->pdataEntries)
    {
        if(!AutoSystemInfo::Data.IsWin8OrLater())
        {
            ushort count = this->GetCurrentPdataCount();
            for(ushort i = 0; i < count; i++)
            {
                RUNTIME_FUNCTION* pdata = GetPdataEntry(i);
                if(pdata->UnwindInfoAddress != 0)
                {
                    BOOLEAN success = RtlDeleteFunctionTable(pdata);
                    Assert(success);
                }
            }
        }
        HeapDeleteArray(this->GetTotalPdataCount(), this->pdataEntries);
        this->pdataEntries = nullptr;
    }

    if(this->functionTableHandles)
    {
        Assert(AutoSystemInfo::Data.IsWin8OrLater());
        ushort count = this->GetCurrentPdataCount();
        for(ushort i = 0; i < count; i++)
        {
            FunctionTableHandle handle = GetFunctionTableHandle(i);
            if(handle)
            {
                NtdllLibrary::Instance->DeleteGrowableFunctionTable(handle);
            }
        }
        HeapDeleteArray(this->GetTotalPdataCount(), this->functionTableHandles);
        this->functionTableHandles = nullptr;
    }

    ClearFreeList();
}

void XDataAllocator::Delete()
{
    HeapDelete(this);
}

bool XDataAllocator::Alloc(ULONG_PTR functionStart, DWORD functionSize, ushort pdataCount, ushort xdataSize, SecondaryAllocation* allocation)
{
    XDataAllocation* xdata = static_cast<XDataAllocation*>(allocation);
    Assert(start != nullptr);
    Assert(current != nullptr);
    Assert(current >= start);
    Assert(xdataSize <= XDATA_SIZE);
    Assert(AutoSystemInfo::Data.IsWin8OrLater() || pdataCount == 1);

    // Allocate a new xdata entry
    if((End() - current) >= XDATA_SIZE)
    {
        xdata->address = current;
        GetNextPdataEntry(&xdata->pdataIndex);
        current += XDATA_SIZE;
    } // try allocating from the free list
    else if(freeList)
    {
        auto entry = freeList;
        xdata->address = entry->address;
        xdata->pdataIndex = entry->pdataIndex;
        this->freeList = entry->next;
        HeapDelete(entry);
    }
    else
    {
        OUTPUT_TRACE(Js::XDataAllocatorPhase, L"No space for XDATA.\n");
    }

    if(xdata->address != nullptr)
    {
        Register(xdata, functionStart, functionSize);
    }

    return xdata->address != nullptr;
}

void XDataAllocator::Release(const SecondaryAllocation& allocation)
{
    const XDataAllocation& xdata = static_cast<const XDataAllocation&>(allocation);
    Assert(allocation.address);
    // Add it to free list
    auto freed = HeapNewNoThrowStruct(XDataAllocationEntry);
    if(freed)
    {
        freed->address = xdata.address;
        freed->pdataIndex = xdata.pdataIndex;
        freed->next = this->freeList;
        this->freeList = freed;
    }

    Assert(this->pdataEntries != nullptr);

    // Delete the table
    if (AutoSystemInfo::Data.IsWin8OrLater())
    {
        FunctionTableHandle handle = GetFunctionTableHandle(xdata.pdataIndex);
        Assert(handle);
        NtdllLibrary::Instance->DeleteGrowableFunctionTable(handle);
        functionTableHandles[xdata.pdataIndex] = nullptr;
    }
    else
    {
        RUNTIME_FUNCTION* pdata = GetPdataEntry(xdata.pdataIndex);
        BOOLEAN success = RtlDeleteFunctionTable(pdata);
        memset(pdata, 0, sizeof(RUNTIME_FUNCTION));
        Assert(success);
    }

#ifdef RECYCLER_MEMORY_VERIFY
    memset(allocation.address, Recycler::VerifyMemFill, XDATA_SIZE);
#endif
}

bool XDataAllocator::CanAllocate()
{
    return ((End() - current) >= XDATA_SIZE) || this->freeList;
}

void XDataAllocator::ReleaseAll()
{
#ifdef RECYCLER_MEMORY_VERIFY
    memset(this->start, Recycler::VerifyMemFill, this->size);
#endif
    this->current = this->start;
    ClearFreeList();
}

void XDataAllocator::ClearFreeList()
{
    XDataAllocationEntry* next = this->freeList;
    XDataAllocationEntry* entry;
    while(next)
    {
        entry = next;
        next = entry->next;
        HeapDelete(entry);
    }
    this->freeList = NULL;
}

void XDataAllocator::Register(XDataAllocation* const xdata, ULONG_PTR functionStart, DWORD functionSize)
{
    RUNTIME_FUNCTION* pdata = this->GetPdataEntry(xdata->pdataIndex);

    ULONG_PTR baseAddress      = functionStart;
    pdata->BeginAddress        = (DWORD)(functionStart - baseAddress);
    pdata->EndAddress          = (DWORD)(pdata->BeginAddress + functionSize);
    pdata->UnwindInfoAddress   = (DWORD)((ULONG_PTR)xdata->address - baseAddress);
    BOOLEAN success = FALSE;
    if (AutoSystemInfo::Data.IsWin8OrLater())
    {
        Assert(this->functionTableHandles[xdata->pdataIndex] == NULL);
        DWORD status = NtdllLibrary::Instance->AddGrowableFunctionTable(&this->functionTableHandles[xdata->pdataIndex],
            pdata,
            /*MaxEntryCount*/ 1,
            /*Valid entry count*/ 1,
            /*RangeBase*/ functionStart,
            /*RangeEnd*/ functionStart + functionSize);
        success = NT_SUCCESS(status);
        if (success)
        {
            Assert(this->functionTableHandles[xdata->pdataIndex]);
        }
    }
    else
    {
        success = RtlAddFunctionTable(pdata, 1, functionStart);
    }
    Js::Throw::CheckAndThrowOutOfMemory(success);

#if DBG
    // Validate that the PDATA registration succeeded
    ULONG64            imageBase       = 0;
    RUNTIME_FUNCTION  *runtimeFunction = RtlLookupFunctionEntry((DWORD64)functionStart, &imageBase, nullptr);
    Assert(runtimeFunction != NULL);
#endif
}
