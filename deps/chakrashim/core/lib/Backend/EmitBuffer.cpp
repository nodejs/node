//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "BackEnd.h"

template class EmitBufferManager<FakeCriticalSection>;
template class EmitBufferManager<CriticalSection>;

//----------------------------------------------------------------------------
// EmitBufferManager::EmitBufferManager
//      Constructor
//----------------------------------------------------------------------------
template <typename SyncObject>
EmitBufferManager<SyncObject>::EmitBufferManager(AllocationPolicyManager * policyManager, ArenaAllocator * allocator,
    Js::ScriptContext * scriptContext, LPCWSTR name, bool allocXdata) :
    allocationHeap(policyManager, allocator, allocXdata),
    allocator(allocator),
    allocations(nullptr),
    scriptContext(scriptContext)
{
#if DBG_DUMP
    this->totalBytesCode = 0;
    this->totalBytesLoopBody = 0;
    this->totalBytesAlignment = 0;
    this->totalBytesCommitted = 0;
    this->totalBytesReserved = 0;
    this->name = name;
#endif
}

//----------------------------------------------------------------------------
// EmitBufferManager::~EmitBufferManager()
//      Free up all the VirtualAlloced memory
//----------------------------------------------------------------------------
template <typename SyncObject>
EmitBufferManager<SyncObject>::~EmitBufferManager()
{
    Clear();
}

template <typename SyncObject>
void
EmitBufferManager<SyncObject>::Decommit()
{
    FreeAllocations(false);
}
template <typename SyncObject>
void
EmitBufferManager<SyncObject>::Clear()
{
    FreeAllocations(true);
}

template <typename SyncObject>
void
EmitBufferManager<SyncObject>::FreeAllocations(bool release)
{
    AutoRealOrFakeCriticalSection<SyncObject> autoCs(&this->criticalSection);

#if DBG_DUMP
    if (!release && PHASE_STATS1(Js::EmitterPhase))
    {
        this->DumpAndResetStats(Js::Configuration::Global.flags.Filename);
    }
#endif

    EmitBufferAllocation * allocation = this->allocations;
    while (allocation != nullptr)
    {
        BOOL isFreed;
        // In case of ThunkEmitter the script context would be null and we don't want to track that as code size.
        if (!release && (scriptContext != nullptr) && allocation->recorded)
        {
            this->scriptContext->GetThreadContext()->SubCodeSize(allocation->bytesCommitted);
            allocation->recorded = false;
        }
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        if(CONFIG_FLAG(CheckEmitBufferPermissions))
        {
            CheckBufferPermissions(allocation);
        }
#endif
        if (release)
        {
            isFreed = this->allocationHeap.Free(allocation->allocation);
        }
        else
        {
            isFreed = this->allocationHeap.Decommit(allocation->allocation);
        }

        Assert(isFreed);
        allocation = allocation->nextAllocation;
    }
    if (release)
    {
        this->allocations = nullptr;
    }

}

class AutoCustomHeapPointer
{
public:
    AutoCustomHeapPointer(CustomHeap::Heap* allocationHeap, CustomHeap::Allocation* heapAllocation) :
        _allocationHeap(allocationHeap),
        _heapAllocation(heapAllocation)
    {
    }

    ~AutoCustomHeapPointer()
    {
        if (_heapAllocation)
        {
            _allocationHeap->Free(_heapAllocation);
        }
    }

    CustomHeap::Allocation* Detach()
    {
        CustomHeap::Allocation* allocation = _heapAllocation;
        Assert(allocation != nullptr);
        _heapAllocation = nullptr;
        return allocation;
    }

private:
    CustomHeap::Allocation* _heapAllocation;
    CustomHeap::Heap* _allocationHeap;
};

//----------------------------------------------------------------------------
// EmitBufferManager::NewAllocation
//      Create a new allocation
//----------------------------------------------------------------------------
template <typename SyncObject>
EmitBufferAllocation *
EmitBufferManager<SyncObject>::NewAllocation(size_t bytes, ushort pdataCount, ushort xdataSize, bool canAllocInPreReservedHeapPageSegment, bool isAnyJittedCode)
{
    FAULTINJECT_MEMORY_THROW(L"JIT", bytes);

    Assert(this->criticalSection.IsLocked());

    PreReservedVirtualAllocWrapper  * preReservedVirtualAllocator = nullptr;

    if (canAllocInPreReservedHeapPageSegment)
    {
        Assert(scriptContext && scriptContext->GetThreadContext());
        preReservedVirtualAllocator = this->scriptContext->GetThreadContext()->GetPreReservedVirtualAllocator();
        this->EnsurePreReservedPageAllocation(preReservedVirtualAllocator);
    }

    bool isAllJITCodeInPreReservedRegion = true;
    CustomHeap::Allocation* heapAllocation = this->allocationHeap.Alloc(bytes, pdataCount, xdataSize, canAllocInPreReservedHeapPageSegment, isAnyJittedCode, &isAllJITCodeInPreReservedRegion);

    if (!isAllJITCodeInPreReservedRegion)
    {
        this->scriptContext->GetThreadContext()->ResetIsAllJITCodeInPreReservedRegion();
    }

    if (heapAllocation  == nullptr)
    {
        // This is used in interpreter scenario, thus we need to try to recover memory, if possible.
        // Can't simply throw as in JIT scenario, for which throw is what we want in order to give more mem to interpreter.
        JsUtil::ExternalApi::RecoverUnusedMemory();
        heapAllocation = this->allocationHeap.Alloc(bytes, pdataCount, xdataSize, canAllocInPreReservedHeapPageSegment, isAnyJittedCode, &isAllJITCodeInPreReservedRegion);
    }

    if (heapAllocation  == nullptr)
    {
        Js::Throw::OutOfMemory();
    }

    AutoCustomHeapPointer allocatedMemory(&this->allocationHeap, heapAllocation);
    VerboseHeapTrace(L"New allocation: 0x%p, size: %p\n", heapAllocation->address, heapAllocation->size);
    EmitBufferAllocation * allocation = AnewStruct(this->allocator, EmitBufferAllocation);

    allocation->bytesCommitted = heapAllocation->size;
    allocation->allocation = allocatedMemory.Detach();
    allocation->bytesUsed = 0;
    allocation->nextAllocation = this->allocations;
    allocation->recorded = false;

    this->allocations = allocation;

#if DBG
    heapAllocation->isAllocationUsed = true;
#endif

#if DBG_DUMP
    this->totalBytesCommitted += heapAllocation->size;
#endif

    return allocation;
}

template <typename SyncObject>
bool
EmitBufferManager<SyncObject>::FreeAllocation(void* address)
{
    AutoRealOrFakeCriticalSection<SyncObject> autoCs(&this->criticalSection);

    EmitBufferAllocation* previous = nullptr;
    EmitBufferAllocation* allocation = allocations;
    while(allocation != nullptr)
    {
        if (address >= allocation->allocation->address && address < (allocation->allocation->address + allocation->bytesUsed))
        {
            if (previous == nullptr)
            {
                this->allocations = allocation->nextAllocation;
            }
            else
            {
                previous->nextAllocation = allocation->nextAllocation;
            }

            if ((scriptContext != nullptr) && allocation->recorded)
            {
                this->scriptContext->GetThreadContext()->SubCodeSize(allocation->bytesCommitted);
            }

            VerboseHeapTrace(L"Freeing 0x%p, allocation: 0x%p\n", address, allocation->allocation->address);

            this->allocationHeap.Free(allocation->allocation);
            this->allocator->Free(allocation, sizeof(EmitBufferAllocation));

            return true;
        }
        previous = allocation;
        allocation = allocation->nextAllocation;
    }
    return false;
}

//----------------------------------------------------------------------------
// EmitBufferManager::FinalizeAllocation
//      Fill the rest of the page with debugger breakpoint.
//----------------------------------------------------------------------------
template <typename SyncObject>
bool EmitBufferManager<SyncObject>::FinalizeAllocation(EmitBufferAllocation *allocation)
{
    Assert(this->criticalSection.IsLocked());

    DWORD bytes = allocation->BytesFree();
    if(bytes > 0)
    {
        BYTE* buffer;
        this->GetBuffer(allocation, bytes, &buffer, false /*readWrite*/);
        if (!this->CommitBuffer(allocation, buffer, 0, /*sourceBuffer=*/ nullptr, /*alignPad=*/ bytes))
        {
            return false;
        }

#if DBG_DUMP
        this->totalBytesCode -= bytes;
#endif
    }

    return true;
}

template <typename SyncObject>
EmitBufferAllocation* EmitBufferManager<SyncObject>::GetBuffer(EmitBufferAllocation *allocation, __in size_t bytes, __deref_bcount(bytes) BYTE** ppBuffer, bool readWrite)
{
    Assert(this->criticalSection.IsLocked());

    this->allocationHeap.EnsureAllocationProtection(allocation->allocation, readWrite);
    Assert(allocation->BytesFree() >= bytes);

    // In case of ThunkEmitter the script context would be null and we don't want to track that as code size.
    if (scriptContext && !allocation->recorded)
    {
        this->scriptContext->GetThreadContext()->AddCodeSize(allocation->bytesCommitted);
        allocation->recorded = true;
    }

    // The codegen buffer is beyond the alignment section - hence, we pass this pointer.
    *ppBuffer = allocation->GetUnused();
    return allocation;
}

//----------------------------------------------------------------------------
// EmitBufferManager::Allocate
//      Allocates an executable buffer with a certain alignment
//      NOTE: This buffer is not readable or writable. Use CommitBuffer
//      to modify this buffer one page at a time.
//----------------------------------------------------------------------------
template <typename SyncObject>
EmitBufferAllocation* EmitBufferManager<SyncObject>::AllocateBuffer(__in size_t bytes, __deref_bcount(bytes) BYTE** ppBuffer, bool readWrite /*= false*/, ushort pdataCount /*=0*/, ushort xdataSize  /*=0*/, bool canAllocInPreReservedHeapPageSegment /*=false*/,
    bool isAnyJittedCode /* = false*/)
{
    AutoRealOrFakeCriticalSection<SyncObject> autoCs(&this->criticalSection);

    Assert(ppBuffer != nullptr);

    EmitBufferAllocation * allocation = this->NewAllocation(bytes, pdataCount, xdataSize, canAllocInPreReservedHeapPageSegment, isAnyJittedCode);

    GetBuffer(allocation, bytes, ppBuffer, readWrite);

    return allocation;
}

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
template <typename SyncObject>
bool EmitBufferManager<SyncObject>::CheckCommitFaultInjection()
{
    if (Js::Configuration::Global.flags.ForceOOMOnEBCommit == 0)
    {
        return false;
    }

    commitCount++;

    if (Js::Configuration::Global.flags.ForceOOMOnEBCommit == -1)
    {
        Output::Print(L"Commit count: %d\n", commitCount);
    }
    else if (commitCount == Js::Configuration::Global.flags.ForceOOMOnEBCommit)
    {
        return true;
    }

    return false;
}

#endif

// Returns true if we successfully commit the buffer
// Returns false if we OOM
template <typename SyncObject>
bool EmitBufferManager<SyncObject>::CommitReadWriteBufferForInterpreter(EmitBufferAllocation* allocation, _In_reads_bytes_(bufferSize) BYTE* pBuffer, _In_ size_t bufferSize)
{
    Assert(this->criticalSection.IsLocked());

    Assert(allocation != nullptr);
    allocation->bytesUsed += bufferSize;
#ifdef DEBUG
    this->totalBytesCode += bufferSize;
#endif

    DWORD oldProtect;

    VerboseHeapTrace(L"Setting execute permissions on 0x%p, allocation: 0x%p\n", pBuffer, allocation->allocation->address);

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    if (CheckCommitFaultInjection())
    {
        return false;
    }
#endif

    if (!this->allocationHeap.ProtectAllocation(allocation->allocation, PAGE_EXECUTE, &oldProtect, PAGE_READWRITE))
    {
        return false;
    }

    FlushInstructionCache(AutoSystemInfo::Data.GetProcessHandle(), pBuffer, bufferSize);

    Assert(oldProtect == PAGE_READWRITE);
    return true;
}

//----------------------------------------------------------------------------
// EmitBufferManager::CommitBuffer
//      Aligns the buffer with DEBUG instructions.
//      Copies contents of source buffer to the destination buffer - at max of one page at a time.
//      This ensures that only 1 page is writable at any point of time.
//      Commit a buffer from the last AllocateBuffer call that is filled.
//----------------------------------------------------------------------------
template <typename SyncObject>
bool
EmitBufferManager<SyncObject>::CommitBuffer(EmitBufferAllocation* allocation, __out_bcount(bytes) BYTE* destBuffer, __in size_t bytes, __in_bcount(bytes) const BYTE* sourceBuffer, __in DWORD alignPad)
{
    AutoRealOrFakeCriticalSection<SyncObject> autoCs(&this->criticalSection);

    Assert(destBuffer != nullptr);
    Assert(allocation != nullptr);

    DWORD oldProtect;

    BYTE *currentDestBuffer = allocation->GetUnused();
    BYTE *bufferToFlush = currentDestBuffer;
    Assert(allocation->BytesFree() >= bytes + alignPad);

    size_t bytesLeft = bytes + alignPad;
    size_t sizeToFlush = bytesLeft;

    // Copy the contents and set the alignment pad
    while(bytesLeft != 0)
    {
        DWORD spaceInCurrentPage = AutoSystemInfo::PageSize - ((size_t)currentDestBuffer & (AutoSystemInfo::PageSize - 1));
        size_t bytesToChange = bytesLeft > spaceInCurrentPage ? spaceInCurrentPage : bytesLeft;


        // Buffer and the bytes that are marked RWX - these will eventually be marked as 'EXCEUTE' only.
        BYTE* readWriteBuffer = currentDestBuffer;
        size_t readWriteBytes = bytesToChange;

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        if (CheckCommitFaultInjection())
        {
            return false;
        }
#endif
        if (!this->allocationHeap.ProtectAllocationPage(allocation->allocation, (char*)readWriteBuffer, PAGE_EXECUTE_READWRITE, &oldProtect, PAGE_EXECUTE))
        {
            return false;
        }
        Assert(oldProtect == PAGE_EXECUTE);

        if (alignPad != 0)
        {
            DWORD alignBytes = alignPad < spaceInCurrentPage ? alignPad : spaceInCurrentPage;
            CustomHeap::FillDebugBreak(currentDestBuffer, alignBytes);

            alignPad -= alignBytes;
            currentDestBuffer += alignBytes;
            allocation->bytesUsed += alignBytes;
            bytesLeft -= alignBytes;
            bytesToChange -= alignBytes;

#if DBG_DUMP
            this->totalBytesAlignment += alignBytes;
#endif
        }

        // If there are bytes still left to be copied then we should do the copy.
        if(bytesToChange > 0)
        {
            AssertMsg(alignPad == 0, "If we are copying right now - we should be done with setting alignment.");

            memcpy_s(currentDestBuffer, allocation->BytesFree(), sourceBuffer, bytesToChange);

            currentDestBuffer += bytesToChange;
            sourceBuffer += bytesToChange;
            allocation->bytesUsed += bytesToChange;
            bytesLeft -= bytesToChange;
        }

        Assert(readWriteBuffer + readWriteBytes == currentDestBuffer);
        if (!this->allocationHeap.ProtectAllocationPage(allocation->allocation, (char*)readWriteBuffer, PAGE_EXECUTE, &oldProtect, PAGE_EXECUTE_READWRITE))
        {
            return false;
        }
        Assert(oldProtect == PAGE_EXECUTE_READWRITE);
    }

    FlushInstructionCache(AutoSystemInfo::Data.GetProcessHandle(), bufferToFlush, sizeToFlush);
#if DBG_DUMP
    this->totalBytesCode += bytes;
#endif

    //Finish the current EmitBufferAllocation
    return FinalizeAllocation(allocation);
}

template <typename SyncObject>
void
EmitBufferManager<SyncObject>::CompletePreviousAllocation(EmitBufferAllocation* allocation)
{
    AutoRealOrFakeCriticalSection<SyncObject> autoCs(&this->criticalSection);
    if (allocation != nullptr)
    {
        allocation->bytesUsed = allocation->bytesCommitted;
    }
}

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
template <typename SyncObject>
void
EmitBufferManager<SyncObject>::CheckBufferPermissions(EmitBufferAllocation *allocation)
{
    AutoRealOrFakeCriticalSection<SyncObject> autoCs(&this->criticalSection);

    if(allocation->bytesCommitted == 0)
        return;

    MEMORY_BASIC_INFORMATION memInfo;

    BYTE *buffer = (BYTE*) allocation->allocation->address;
    SIZE_T size = allocation->bytesCommitted;

    while(1)
    {
        SIZE_T result = VirtualQuery(buffer, &memInfo, sizeof(memInfo));
        if(result == 0)
        {
            // VirtualQuery failed.  This is not an expected condition, but it would be benign for the purposes of this check.  Seems
            // to occur occasionally on process shutdown.
            break;
        }
        else if(memInfo.Protect == PAGE_EXECUTE_READWRITE)
        {
            Output::Print(L"ERROR: Found PAGE_EXECUTE_READWRITE page!\n");
#ifdef DEBUG
            AssertMsg(FALSE, "Page was marked PAGE_EXECUTE_READWRITE");
#else
            Fatal();
#endif
        }

        // Figure out if we need to continue the query.  The returned size might be larger than the size we requested,
        // for instance if more pages were allocated directly afterward, with the same permissions.
        if(memInfo.RegionSize >= size)
        {
            break;
        }

        // recalculate size for next iteration
        buffer += memInfo.RegionSize;
        size -= memInfo.RegionSize;

        if(size <= 0)
        {
            AssertMsg(FALSE, "Last VirtualQuery left us with unmatched regions");
            break;
        }
    }
}
#endif

#if DBG_DUMP
template <typename SyncObject>
void
EmitBufferManager<SyncObject>::DumpAndResetStats(wchar_t const * filename)
{
    if (this->totalBytesCommitted != 0)
    {
        size_t wasted = this->totalBytesCommitted - this->totalBytesCode - this->totalBytesAlignment;
        Output::Print(L"Stats for %s: %s \n", name, filename);
        Output::Print(L"  Total code size      : %10d (%6.2f%% of committed)\n", this->totalBytesCode,
            (float)this->totalBytesCode * 100 / this->totalBytesCommitted);
        Output::Print(L"  Total LoopBody code  : %10d\n", this->totalBytesLoopBody);
        Output::Print(L"  Total alignment size : %10d (%6.2f%% of committed)\n", this->totalBytesAlignment,
            (float)this->totalBytesAlignment * 100 / this->totalBytesCommitted);
        Output::Print(L"  Total wasted size    : %10d (%6.2f%% of committed)\n", wasted,
            (float)wasted * 100 / this->totalBytesCommitted);
        Output::Print(L"  Total committed size : %10d (%6.2f%% of reserved)\n", this->totalBytesCommitted,
            (float)this->totalBytesCommitted * 100 / this->totalBytesReserved);
        Output::Print(L"  Total reserved size  : %10d\n", this->totalBytesReserved);
    }
    this->totalBytesCode = 0;
    this->totalBytesLoopBody = 0;
    this->totalBytesAlignment = 0;
    this->totalBytesCommitted = 0;
    this->totalBytesReserved = 0;
}
#endif
