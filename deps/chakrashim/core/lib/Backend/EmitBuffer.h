//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

//---------------------------------------------------------------------------------
// One allocation chunk from CustomHeap + PData if needed, tracked as a linked list
//---------------------------------------------------------------------------------
struct EmitBufferAllocation
{
    CustomHeap::Allocation* allocation;
    size_t bytesUsed;
    size_t bytesCommitted;
    bool   recorded;
    EmitBufferAllocation * nextAllocation;

    BYTE * GetUnused() const            { return (BYTE*) allocation->address + bytesUsed; }
    BYTE * GetUncommitted() const       { return (BYTE*) allocation->address + bytesCommitted; }

    // Truncation to DWORD okay here
    DWORD BytesFree() const    { return static_cast<DWORD>(this->bytesCommitted - this->bytesUsed); }
};
typedef void* NativeMethod;

//----------------------------------------------------------------------------
// Emit buffer manager - manages allocation chunks from VirtualAlloc
//----------------------------------------------------------------------------
template <class SyncObject = FakeCriticalSection>
class EmitBufferManager
{
public:
    EmitBufferManager(AllocationPolicyManager * policyManager, ArenaAllocator * allocator, Js::ScriptContext * scriptContext, LPCWSTR name, bool allocXdata);
    ~EmitBufferManager();

    // All the following methods are guarded with the SyncObject
    void Decommit();
    void Clear();

    EmitBufferAllocation* AllocateBuffer(__in size_t bytes, __deref_bcount(bytes) BYTE** ppBuffer, bool readWrite = false, ushort pdataCount = 0, ushort xdataSize = 0, bool canAllocInPreReservedHeapPageSegment = false, bool isAnyJittedCode = false);
    bool CommitBuffer(EmitBufferAllocation* allocation, __out_bcount(bytes) BYTE* destBuffer, __in size_t bytes, __in_bcount(bytes) const BYTE* sourceBuffer, __in DWORD alignPad = 0);
    bool CommitReadWriteBufferForInterpreter(EmitBufferAllocation* allocation, _In_reads_bytes_(bufferSize) BYTE* pBuffer, _In_ size_t bufferSize);
    void CompletePreviousAllocation(EmitBufferAllocation* allocation);
    bool FreeAllocation(void* address);
    //Ends here

    bool IsInRange(void* address)
    {
        return this->allocationHeap.IsInRange(address);
    }

    HeapPageAllocator<VirtualAllocWrapper>* GetHeapPageAllocator()
    {
        return this->allocationHeap.GetHeapPageAllocator();
    }

    HeapPageAllocator<PreReservedVirtualAllocWrapper>* GetPreReservedHeapPageAllocator()
    {
        return this->allocationHeap.GetPreReservedHeapPageAllocator();
    }

    char * EnsurePreReservedPageAllocation(PreReservedVirtualAllocWrapper * preReservedVirtualAllocator)
    {
#if !_M_X64_OR_ARM64 && _CONTROL_FLOW_GUARD
        bool canPreReserveSegmentForCustomHeap = scriptContext && scriptContext->GetThreadContext()->CanPreReserveSegmentForCustomHeap();
#endif
        AssertMsg(preReservedVirtualAllocator, "Virtual Allocator for pre reserved Segment should not be null when EnsurePreReservedPageAllocation is called");

        if (this->GetPreReservedHeapPageAllocator()->GetVirtualAllocator() == nullptr)
        {
            this->GetPreReservedHeapPageAllocator()->SetVirtualAllocator(preReservedVirtualAllocator);
        }

        if (preReservedVirtualAllocator->IsPreReservedRegionPresent())
        {
            return (char*) preReservedVirtualAllocator->GetPreReservedStartAddress();
        }

#if !_M_X64_OR_ARM64 && _CONTROL_FLOW_GUARD
        if (!canPreReserveSegmentForCustomHeap)
        {
            VerboseHeapTrace(L"PRE-RESERVE: Upper Cap for PreReservedSegment reached.\n");
            return nullptr;
        }
#endif
        char * startAddressOfPreReservedRegion = this->allocationHeap.EnsurePreReservedPageAllocation(preReservedVirtualAllocator);

        //We have newly reserved a segment at this point
        if (startAddressOfPreReservedRegion != nullptr)
        {
#if !_M_X64_OR_ARM64 && _CONTROL_FLOW_GUARD
            Assert(canPreReserveSegmentForCustomHeap);
            this->scriptContext->GetThreadContext()->IncrementThreadContextsWithPreReservedSegment();
#endif
        }

        return startAddressOfPreReservedRegion;
    }

#if DBG_DUMP
    void DumpAndResetStats(wchar_t const * source);
#endif

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    void CheckBufferPermissions(EmitBufferAllocation *allocation);
#endif

    EmitBufferAllocation * allocations;

private:
    void FreeAllocations(bool release);

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    bool CheckCommitFaultInjection();

    int commitCount;
#endif
    ArenaAllocator * allocator;
    Js::ScriptContext * scriptContext;

    EmitBufferAllocation * NewAllocation(size_t bytes, ushort pdataCount, ushort xdataSize, bool canAllocInPreReservedHeapPageSegment, bool isAnyJittedCode);
    EmitBufferAllocation* GetBuffer(EmitBufferAllocation *allocation, __in size_t bytes, __deref_bcount(bytes) BYTE** ppBuffer, bool readWrite);

    bool FinalizeAllocation(EmitBufferAllocation *allocation);
    CustomHeap::Heap allocationHeap;

    SyncObject  criticalSection;
#if DBG_DUMP

public:
    LPCWSTR name;
    size_t totalBytesCode;
    size_t totalBytesLoopBody;
    size_t totalBytesAlignment;
    size_t totalBytesCommitted;
    size_t totalBytesReserved;
#endif
};

