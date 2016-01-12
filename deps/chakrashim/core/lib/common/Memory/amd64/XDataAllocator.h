//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#if !defined(_M_X64)
CompileAssert(false)
#endif
#pragma once

namespace Memory
{
#define XDATA_SIZE (72)

struct XDataAllocation : public SecondaryAllocation
{
    XDataAllocation() :
        pdataIndex(0)
    {}

    bool IsFreed() const
    {
        return address == nullptr;
    }
    void Free()
    {
        address = nullptr;
    }
    // ---- Data members ---- //
    ushort pdataIndex;
};

//
// Allocates xdata and pdata entries for x64 architecture.
//
// xdata
// ------
// x64 architecture requires the xdata to be within 32-bit address range of the jitted code itself
// Hence, for every page segment we have an instance of the xdata allocator that allocates
// xdata entries in some specified non-executable region at the end of the page segment.
//
// pdata
// -------
// XDataAllocator also manages the pdata entries for a the page segment range. It allocates the table of pdata entries
// on the heap to do that.
//
class XDataAllocator sealed : public SecondaryAllocator
{
// -------- Private members ---------/
private:
    struct XDataAllocationEntry : XDataAllocation
    {
        XDataAllocationEntry* next;
    };
    BYTE* start;
    BYTE* current;
    uint  size;

    XDataAllocationEntry* freeList;
    RUNTIME_FUNCTION* pdataEntries;
    FunctionTableHandle* functionTableHandles;

// --------- Public functions ---------/
public:
    XDataAllocator(BYTE* address, uint size);

    virtual ~XDataAllocator();

    bool Initialize(void* segmentStart, void* segmentEnd);
    void Delete();
    bool Alloc(ULONG_PTR functionStart, DWORD functionSize, ushort pdataCount, ushort xdataSize, SecondaryAllocation* allocation);
    void Release(const SecondaryAllocation& address);
    void ReleaseAll();
    bool CanAllocate();

// -------- Private helpers ---------/
private:
    BYTE* End() { return start + size; }
    ushort GetTotalPdataCount()
    {
        return (ushort)(this->size / XDATA_SIZE);
    }

    RUNTIME_FUNCTION* GetNextPdataEntry(ushort* pdataIndex)
    {
        Assert(this->pdataEntries);
        *pdataIndex = GetCurrentPdataCount();
        Assert(*pdataIndex < GetTotalPdataCount());
        RUNTIME_FUNCTION* pdata = &(this->pdataEntries[*pdataIndex]);
        return pdata;
    }

    ushort GetCurrentPdataCount()
    {
        return (ushort)((current - start) / XDATA_SIZE);
    }

    RUNTIME_FUNCTION* GetPdataEntry(ushort pdataIndex)
    {
        Assert(pdataIndex < GetTotalPdataCount());
        Assert(this->pdataEntries);
        return &(pdataEntries[pdataIndex]);
    }

    FunctionTableHandle GetFunctionTableHandle(ushort pdataIndex)
    {
        Assert(pdataIndex < GetTotalPdataCount());
        Assert(functionTableHandles);
        return functionTableHandles[pdataIndex];
    }

    void ClearFreeList();
    void Register(XDataAllocation* const xdata, ULONG_PTR functionStart, DWORD functionSize);
};
}
