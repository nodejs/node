//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
// This one works only for ARM64
#include "StdAfx.h"
#if !defined(_M_ARM64)
CompileAssert(false)
#endif

XDataAllocator::XDataAllocator(BYTE* address, uint size)
{
    Assert(size == 0);
}


void XDataAllocator::Delete()
{
    HeapDelete(this);
}

bool XDataAllocator::Initialize(void* segmentStart, void* segmentEnd)
{
    return true;
}

bool XDataAllocator::Alloc(ULONG_PTR functionStart, DWORD functionSize, ushort pdataCount, ushort xdataSize, SecondaryAllocation* allocation)
{
    XDataAllocation* xdata = static_cast<XDataAllocation*>(allocation);
    Assert(pdataCount > 0);
    Assert(xdataSize >= 0);
    Assert(xdata);
    bool success = true;

    DWORD size = GetAllocSize(pdataCount, xdataSize);
    BYTE* alloc = HeapNewNoThrowArray(BYTE, size);
    if(alloc != null)
    {
        xdata->address = alloc;
        xdata->xdataSize = xdataSize;
        xdata->pdataCount = pdataCount;
    }
    return success;
}

void XDataAllocator::Register(XDataAllocation& allocation, ULONG_PTR functionStart, DWORD functionSize)
{
    RUNTIME_FUNCTION* pdataArray = allocation.GetPdataArray();
    for(ushort i = 0; i < allocation.pdataCount; i++)
    {
        RUNTIME_FUNCTION* pdata = pdataArray + i;
        Assert(pdata->UnwindData != 0);
        Assert(pdata->BeginAddress != 0);
        pdata->BeginAddress = (DWORD)((ULONG_PTR)pdata->BeginAddress - functionStart);
        if(pdata->Flag != 1) // if it is not packed unwind data
        {
            pdata->UnwindData = (DWORD)((ULONG_PTR)pdata->UnwindData - functionStart);
        }
    }
    Assert(allocation.functionTable == null);

    // Since we do not expect many thunk functions to be created, we are using 1 table/function
    // for now. This can be optimized further if needed.
    DWORD status = NtdllLibrary::Instance->AddGrowableFunctionTable(&allocation.functionTable,
        pdataArray,
        /*MaxEntryCount*/ allocation.pdataCount,
        /*Valid entry count*/ allocation.pdataCount,
        /*RangeBase*/ functionStart,
        /*RangeEnd*/ functionStart + functionSize);
    Assert(NT_SUCCESS(status));
}

void XDataAllocator::Release(const SecondaryAllocation& allocation)
{
    const XDataAllocation& xdata = static_cast<const XDataAllocation&>(allocation);
    if(xdata.address  != null)
    {
        if(xdata.functionTable)
        {
            NtdllLibrary::Instance->DeleteGrowableFunctionTable(xdata.functionTable);
        }
        HeapDeleteArray(GetAllocSize(xdata.pdataCount, xdata.xdataSize), xdata.address);
    }
}

bool XDataAllocator::CanAllocate()
{
    return true;
}
