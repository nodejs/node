//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonMemoryPch.h"

//=====================================================================================================
// Initialization
//=====================================================================================================
LargeHeapBucket::~LargeHeapBucket()
{
    Recycler* recycler = this->heapInfo->recycler;
    HeapInfo* autoHeap = this->heapInfo;

    ForEachEditingLargeHeapBlock([recycler, autoHeap](LargeHeapBlock * heapBlock)
    {
        heapBlock->ReleasePagesShutdown(recycler);
        LargeHeapBlock::Delete(heapBlock);
        RECYCLER_SLOW_CHECK(autoHeap->heapBlockCount[HeapBlock::HeapBlockType::LargeBlockType]--);
    });
}

void
LargeHeapBucket::Initialize(HeapInfo * heapInfo, uint sizeCat, bool supportFreeList)
{
    this->heapInfo = heapInfo;
    this->sizeCat = sizeCat;
#ifdef RECYCLER_PAGE_HEAP
    this->isPageHeapEnabled = heapInfo->IsPageHeapEnabledForBlock<LargeAllocationBlockAttributes>(sizeCat);
#endif
    this->supportFreeList = supportFreeList;
}

//=====================================================================================================
// Allocation
//=====================================================================================================
char *
LargeHeapBucket::TryAllocFromNewHeapBlock(Recycler * recycler, size_t sizeCat, ObjectInfoBits attributes, bool nothrow)
{
    Assert((attributes & InternalObjectInfoBitMask) == attributes);

#ifdef RECYCLER_PAGE_HEAP
    if (IsPageHeapEnabled())
    {
        return this->PageHeapAlloc(recycler, sizeCat, attributes, this->heapInfo->pageHeapMode, true);
    }
#endif

    LargeHeapBlock * heapBlock = AddLargeHeapBlock(sizeCat, nothrow);
    if (heapBlock == nullptr)
    {
        return nullptr;
    }
    char * memBlock = heapBlock->Alloc(sizeCat, attributes);
    Assert(memBlock != nullptr);
    return memBlock;
}

char *
LargeHeapBucket::SnailAlloc(Recycler * recycler, size_t sizeCat, ObjectInfoBits attributes, bool nothrow)
{
    char * memBlock;

    Assert((attributes & InternalObjectInfoBitMask) == attributes);

    // No free memory, try to collect with allocated bytes and time heuristic, and concurrently
    BOOL collected = recycler->disableCollectOnAllocationHeuristics ? recycler->FinishConcurrent<FinishConcurrentOnAllocation>() :
        recycler->CollectNow<CollectOnAllocation>();
    if (!collected)
    {
        memBlock = TryAllocFromNewHeapBlock(recycler, sizeCat, attributes, nothrow);
        if (memBlock != nullptr)
        {
            return memBlock;
        }
        // Can't even allocate a new block, we need force a collection and
        // allocate some free memory, add a new heap block again, or throw out of memory
        AllocationVerboseTrace(recycler->GetRecyclerFlagsTable(), L"LargeHeapBucket::AddLargeHeapBlock failed, forcing in-thread collection\n");
        recycler->CollectNow<CollectNowForceInThread>();
    }

    memBlock = TryAlloc(recycler, sizeCat, attributes);
    if (memBlock != nullptr)
    {
        return memBlock;
    }

    memBlock = TryAllocFromNewHeapBlock(recycler, sizeCat, attributes, nothrow);
    if (memBlock != nullptr)
    {
        return memBlock;
    }

    if (nothrow == false)
    {
        // Can't add a heap block, we are out of memory
        // Since nothrow is false, we can throw right here
        recycler->OutOfMemory();
    }

    return nullptr;
}

#ifdef RECYCLER_PAGE_HEAP
char*
LargeHeapBucket::PageHeapAlloc(Recycler * recycler, size_t size, ObjectInfoBits attributes, PageHeapMode mode, bool nothrow)
{
    size_t sizeCat = HeapInfo::GetAlignedSizeNoCheck(size);

    Segment * segment;
    size_t pageCount = LargeHeapBlock::GetPagesNeeded(size, this->supportFreeList);
    if (pageCount == 0)
    {
        if (nothrow == false)
        {
            // overflow
            // Since nothrow is false here, it's okay to throw
            recycler->OutOfMemory();
        }

        return nullptr;
    }

    size_t actualPageCount = pageCount + 1; // for page heap

    char * baseAddress = recycler->GetRecyclerLargeBlockPageAllocator()->Alloc(&actualPageCount, &segment);
    if (baseAddress == nullptr)
    {
        return nullptr;
    }

    char* address = nullptr;
    char* guardPageAddress = nullptr;
    DWORD guardPageOldProtectFlags = PAGE_NOACCESS;

    if (heapInfo->pageHeapMode == PageHeapMode::PageHeapModeBlockStart)
    {
        address = baseAddress + AutoSystemInfo::PageSize;
        guardPageAddress = baseAddress;
    }
    else if (heapInfo->pageHeapMode == PageHeapMode::PageHeapModeBlockEnd)
    {
        address = baseAddress;
        guardPageAddress = baseAddress + pageCount* AutoSystemInfo::PageSize;
    }
    else
    {
        AnalysisAssert(false);
    }

    if (::VirtualProtect(static_cast<LPVOID>(guardPageAddress), AutoSystemInfo::PageSize, PAGE_NOACCESS, &guardPageOldProtectFlags) == FALSE)
    {
        AssertMsg(false, "Unable to set permission for guard page.");
        return nullptr;
    }

#ifdef RECYCLER_ZERO_MEM_CHECK
    recycler->VerifyZeroFill(address, pageCount * AutoSystemInfo::PageSize);
#endif

    LargeHeapBlock * heapBlock = LargeHeapBlock::New(address, pageCount, segment, 1, nullptr);
    if (!heapBlock)
    {
        recycler->GetRecyclerLargeBlockPageAllocator()->SuspendIdleDecommit();
        recycler->GetRecyclerLargeBlockPageAllocator()->Release(address, actualPageCount, segment);
        recycler->GetRecyclerLargeBlockPageAllocator()->ResumeIdleDecommit();
        return nullptr;
    }
    heapBlock->actualPageCount = actualPageCount;
    heapBlock->guardPageAddress = guardPageAddress;
    heapBlock->guardPageOldProtectFlags = guardPageOldProtectFlags;
    heapBlock->pageHeapMode = heapInfo->pageHeapMode;

    if (heapBlock->pageHeapMode == PageHeapMode::PageHeapModeBlockEnd)
    {
        // TODO: pad the address to close-most to the guard page to increase the chance to hit guard page when overflow
        // some Mark code need to be updated to support this
        // heapBlock->SetEndAllocAddress(address
        //    + AutoSystemInfo::PageSize - (((AllocSizeMath::Add(sizeCat, sizeof(LargeObjectHeader)) - 1) % AutoSystemInfo::PageSize) / HeapInfo::ObjectGranularity + 1) * HeapInfo::ObjectGranularity);
    }

#if DBG
    LargeAllocationVerboseTrace(recycler->GetRecyclerFlagsTable(), L"Allocated new large heap block 0x%p for sizeCat 0x%x\n", heapBlock, sizeCat);
#endif

#ifdef ENABLE_JS_ETW
#if ENABLE_DEBUG_CONFIG_OPTIONS
    if (segment->GetPageCount() > recycler->GetRecyclerLargeBlockPageAllocator()->GetMaxAllocPageCount())
    {
        EventWriteJSCRIPT_INTERNAL_RECYCLER_EXTRALARGE_OBJECT_ALLOC(size);
    }
#endif
#endif

#ifdef PARTIAL_GC_ENABLED
    recycler->autoHeap.uncollectedNewPageCount += pageCount;
#endif

    RECYCLER_SLOW_CHECK(this->heapInfo->heapBlockCount[HeapBlock::HeapBlockType::LargeBlockType]++);

    heapBlock->heapInfo = this->heapInfo;

    Assert(recycler->collectionState != CollectionStateMark);

    if (!recycler->heapBlockMap.SetHeapBlock(address, pageCount, heapBlock, HeapBlock::HeapBlockType::LargeBlockType, 0))
    {
        recycler->GetRecyclerLargeBlockPageAllocator()->SuspendIdleDecommit();
        heapBlock->ReleasePages<true>(recycler);
        recycler->GetRecyclerLargeBlockPageAllocator()->ResumeIdleDecommit();
        LargeHeapBlock::Delete(heapBlock);
        RECYCLER_SLOW_CHECK(this->heapInfo->heapBlockCount[HeapBlock::HeapBlockType::LargeBlockType]--);
        return nullptr;
    }

    heapBlock->ResetMarks(ResetMarkFlags_None, recycler);

    if (this->largePageHeapBlockList)
    {
        HeapBlockList::Tail(this->largePageHeapBlockList)->SetNextBlock(heapBlock);
    }
    else
    {
        this->largePageHeapBlockList = heapBlock;
    }

    RECYCLER_PERF_COUNTER_ADD(FreeObjectSize, heapBlock->GetPageCount() * AutoSystemInfo::PageSize);

    char * memBlock = heapBlock->Alloc(sizeCat, attributes);
    Assert(memBlock != nullptr);
    if (recycler->ShouldCapturePageHeapAllocStack())
    {
        heapBlock->CapturePageHeapAllocStack();
    }

    return memBlock;
}
#endif

LargeHeapBlock*
LargeHeapBucket::AddLargeHeapBlock(size_t size, bool nothrow)
{
    Recycler* recycler = this->heapInfo->recycler;
    Segment * segment;
    size_t pageCount = LargeHeapBlock::GetPagesNeeded(size, this->supportFreeList);
    if (pageCount == 0)
    {
        if (nothrow == false)
        {
            // overflow
            // Since nothrow is false here, it's okay to throw
            recycler->OutOfMemory();
        }

        return nullptr;
    }

    char * address = nullptr;

    address = recycler->GetRecyclerLargeBlockPageAllocator()->Alloc(&pageCount, &segment);

    if (address == nullptr)
    {
        return nullptr;
    }
#ifdef RECYCLER_ZERO_MEM_CHECK
    recycler->VerifyZeroFill(address, pageCount * AutoSystemInfo::PageSize);
#endif
    uint objectCount = LargeHeapBlock::GetMaxLargeObjectCount(pageCount, size);
    LargeHeapBlock * heapBlock = LargeHeapBlock::New(address, pageCount, segment, objectCount, supportFreeList ? this : nullptr);
#if DBG
    LargeAllocationVerboseTrace(recycler->GetRecyclerFlagsTable(), L"Allocated new large heap block 0x%p for sizeCat 0x%x\n", heapBlock, sizeCat);
#endif

#ifdef ENABLE_JS_ETW
#if ENABLE_DEBUG_CONFIG_OPTIONS
    if (segment->GetPageCount() > recycler->GetRecyclerLargeBlockPageAllocator()->GetMaxAllocPageCount())
    {
        EventWriteJSCRIPT_INTERNAL_RECYCLER_EXTRALARGE_OBJECT_ALLOC(size);
    }
#endif
#endif
    if (!heapBlock)
    {
        recycler->GetRecyclerLargeBlockPageAllocator()->SuspendIdleDecommit();
        recycler->GetRecyclerLargeBlockPageAllocator()->Release(address, pageCount, segment);
        recycler->GetRecyclerLargeBlockPageAllocator()->ResumeIdleDecommit();
        return nullptr;
    }
#ifdef PARTIAL_GC_ENABLED
    recycler->autoHeap.uncollectedNewPageCount += pageCount;
#endif

    RECYCLER_SLOW_CHECK(this->heapInfo->heapBlockCount[HeapBlock::HeapBlockType::LargeBlockType]++);

    heapBlock->heapInfo = this->heapInfo;

    heapBlock->lastCollectAllocCount = 0;

    Assert(recycler->collectionState != CollectionStateMark);

    if (!recycler->heapBlockMap.SetHeapBlock(address, pageCount, heapBlock, HeapBlock::HeapBlockType::LargeBlockType, 0))
    {
        recycler->GetRecyclerLargeBlockPageAllocator()->SuspendIdleDecommit();
        heapBlock->ReleasePages<false>(recycler);
        recycler->GetRecyclerLargeBlockPageAllocator()->ResumeIdleDecommit();
        LargeHeapBlock::Delete(heapBlock);
        RECYCLER_SLOW_CHECK(this->heapInfo->heapBlockCount[HeapBlock::HeapBlockType::LargeBlockType]--);
        return nullptr;
    }

    heapBlock->SetNextBlock(this->largeBlockList);
    this->largeBlockList = heapBlock;

    RECYCLER_PERF_COUNTER_ADD(FreeObjectSize, heapBlock->GetPageCount() * AutoSystemInfo::PageSize);
    return heapBlock;
}

char *
LargeHeapBucket::TryAllocFromFreeList(Recycler * recycler, size_t sizeCat, ObjectInfoBits attributes)
{
    Assert((attributes & InternalObjectInfoBitMask) == attributes);

    LargeHeapBlockFreeList* freeListEntry = this->freeList;

    // Walk through the free list, find the first entry that can fit our desired size
    while (freeListEntry)
    {
        LargeHeapBlock* heapBlock = freeListEntry->heapBlock;

        char * memBlock = heapBlock->TryAllocFromFreeList(sizeCat, attributes);
        if (memBlock)
        {
            // Don't need to verify zero fill here since we will do it in LargeHeapBucket::Alloc
            return memBlock;
        }
        else
        {
#if DBG
            LargeAllocationVerboseTrace(recycler->GetRecyclerFlagsTable(), L"Unable to allocate object of size 0x%x from freelist\n", sizeCat);
#endif
        }

        freeListEntry = freeListEntry->next;
    }
    return nullptr;
}

char *
LargeHeapBucket::TryAllocFromExplicitFreeList(Recycler * recycler, size_t sizeCat, ObjectInfoBits attributes)
{
    Assert((attributes & InternalObjectInfoBitMask) == attributes);

    FreeObject * currFreeObject = this->explicitFreeList;
    FreeObject * prevFreeObject = nullptr;
    while (currFreeObject != nullptr)
    {
        char * memBlock = (char *)currFreeObject;
        LargeObjectHeader * header = LargeHeapBlock::GetHeaderFromAddress(memBlock);
        Assert(header->isExplicitFreed);
        Assert(HeapInfo::GetMediumObjectAlignedSizeNoCheck(header->objectSize) == this->sizeCat);
        if (header->objectSize < sizeCat)
        {
            prevFreeObject = currFreeObject;
            currFreeObject = currFreeObject->GetNext();
            continue;
        }

        DebugOnly(header->isExplicitFreed = false);
        if (prevFreeObject)
        {
            prevFreeObject->SetNext(currFreeObject->GetNext());
        }
        else
        {
            this->explicitFreeList = currFreeObject->GetNext();
        }

#ifdef RECYCLER_MEMORY_VERIFY
        HeapBlock* heapBlock = recycler->FindHeapBlock(memBlock);
        Assert(heapBlock != nullptr);
        Assert(heapBlock->IsLargeHeapBlock());
        LargeHeapBlock * largeHeapBlock = (LargeHeapBlock *)heapBlock;
        LargeObjectHeader * dbgHeader;
        Assert(largeHeapBlock->GetObjectHeader(memBlock, &dbgHeader));
        Assert(dbgHeader == header);

        ((FreeObject *)memBlock)->DebugFillNext();
#endif
#ifdef RECYCLER_ZERO_MEM_CHECK
        // TODO: large heap block doesn't separate leaf object on to different page allocator.
        // so all the memory should still be zeroed.
        memset(memBlock, 0, sizeof(FreeObject));
#endif
        header->SetAttributes(recycler->Cookie, (attributes & StoredObjectInfoBitMask));

        if ((attributes & ObjectInfoBits::FinalizeBit) != 0)
        {
            LargeHeapBlock* heapBlock = (LargeHeapBlock *)recycler->FindHeapBlock(memBlock);
            heapBlock->finalizeCount++;
#ifdef RECYCLER_FINALIZE_CHECK
            heapInfo->liveFinalizableObjectCount++;
            heapInfo->newFinalizableObjectCount++;
#endif
        }
        return memBlock;
    }

    return nullptr;
}
//=====================================================================================================
// Free
//=====================================================================================================
void
LargeHeapBucket::ExplicitFree(void * object, size_t sizeCat)
{
    Assert(HeapInfo::GetMediumObjectAlignedSizeNoCheck(sizeCat) == this->sizeCat);
    LargeObjectHeader * header = LargeHeapBlock::GetHeaderFromAddress(object);
    Assert(header->GetAttributes(this->heapInfo->recycler->Cookie) == ObjectInfoBits::NoBit || header->GetAttributes(this->heapInfo->recycler->Cookie) == ObjectInfoBits::LeafBit);
    Assert(!header->isExplicitFreed);
    DebugOnly(header->isExplicitFreed = true);
    Assert(header->objectSize >= sizeCat);

#if DBG
    HeapBlock* heapBlock = this->GetRecycler()->FindHeapBlock(object);
    Assert(heapBlock != nullptr);
    Assert(heapBlock->IsLargeHeapBlock());

    LargeHeapBlock * largeHeapBlock = (LargeHeapBlock *)heapBlock;
    LargeObjectHeader * dbgHeader;
    Assert(largeHeapBlock->GetObjectHeader(object, &dbgHeader));
    Assert(dbgHeader == header);
#endif

    FreeObject * freeObject = (FreeObject *)object;
    freeObject->SetNext(this->explicitFreeList);
    this->explicitFreeList = freeObject;
    header->SetAttributes(this->heapInfo->recycler->Cookie, ObjectInfoBits::LeafBit);       // We can stop scanning it now.


}

//=====================================================================================================
// Collections
//=====================================================================================================
void
LargeHeapBucket::ResetMarks(ResetMarkFlags flags)
{
    Recycler* recycler = this->heapInfo->recycler;

    HeapBlockList::ForEach(largeBlockList, [flags, recycler](LargeHeapBlock * heapBlock)
    {
        heapBlock->ResetMarks(flags, recycler);
    });
#ifdef RECYCLER_PAGE_HEAP
    HeapBlockList::ForEach(largePageHeapBlockList, [flags, recycler](LargeHeapBlock * heapBlock)
    {
        heapBlock->ResetMarks(flags, recycler);
    });
#endif
    HeapBlockList::ForEach(fullLargeBlockList, [flags, recycler](LargeHeapBlock * heapBlock)
    {
        heapBlock->ResetMarks(flags, recycler);
    });
    HeapBlockList::ForEach(pendingDisposeLargeBlockList, [flags, recycler](LargeHeapBlock * heapBlock)
    {
        heapBlock->ResetMarks(flags, recycler);
    });
#ifdef CONCURRENT_GC_ENABLED
    Assert(pendingSweepLargeBlockList == nullptr);
#endif
}

void
LargeHeapBucket::ScanInitialImplicitRoots(Recycler * recycler)
{
    HeapBlockList::ForEach(largeBlockList, [recycler](LargeHeapBlock * heapBlock)
    {
        heapBlock->ScanInitialImplicitRoots(recycler);
    });
#ifdef RECYCLER_PAGE_HEAP
    HeapBlockList::ForEach(largePageHeapBlockList, [recycler](LargeHeapBlock * heapBlock)
    {
        heapBlock->ScanInitialImplicitRoots(recycler);
    });
#endif
    HeapBlockList::ForEach(fullLargeBlockList, [recycler](LargeHeapBlock * heapBlock)
    {
        heapBlock->ScanInitialImplicitRoots(recycler);
    });
    HeapBlockList::ForEach(pendingDisposeLargeBlockList, [recycler](LargeHeapBlock * heapBlock)
    {
        heapBlock->ScanInitialImplicitRoots(recycler);
    });
#ifdef CONCURRENT_GC_ENABLED
    Assert(pendingSweepLargeBlockList == nullptr);
#endif
}

void
LargeHeapBucket::ScanNewImplicitRoots(Recycler * recycler)
{
    HeapBlockList::ForEach(largeBlockList, [recycler](LargeHeapBlock * heapBlock)
    {
        heapBlock->ScanNewImplicitRoots(recycler);
    });
#ifdef RECYCLER_PAGE_HEAP
    HeapBlockList::ForEach(largePageHeapBlockList, [recycler](LargeHeapBlock * heapBlock)
    {
        heapBlock->ScanNewImplicitRoots(recycler);
    });
#endif
    HeapBlockList::ForEach(fullLargeBlockList, [recycler](LargeHeapBlock * heapBlock)
    {
        heapBlock->ScanNewImplicitRoots(recycler);
    });
    HeapBlockList::ForEach(pendingDisposeLargeBlockList, [recycler](LargeHeapBlock * heapBlock)
    {
        heapBlock->ScanNewImplicitRoots(recycler);
    });
#ifdef CONCURRENT_GC_ENABLED
    Assert(pendingSweepLargeBlockList == nullptr);
#endif
}

//=====================================================================================================
// Sweep
//=====================================================================================================
#pragma region Sweep
template void LargeHeapBucket::Sweep<true>(RecyclerSweep& recyclerSweep);
template void LargeHeapBucket::Sweep<false>(RecyclerSweep& recyclerSweep);

template<bool pageheap>
void
LargeHeapBucket::Sweep(RecyclerSweep& recyclerSweep)
{
    // CONCURRENT-TODO: large buckets are not swept in the background currently.
    Assert(!recyclerSweep.GetRecycler()->IsConcurrentExecutingState());

    LargeHeapBlock * currentLargeObjectBlocks = largeBlockList;
#ifdef RECYCLER_PAGE_HEAP
    LargeHeapBlock * currentLargePageHeapObjectBlocks = largePageHeapBlockList;
#endif
    LargeHeapBlock * currentFullLargeObjectBlocks = fullLargeBlockList;
    LargeHeapBlock * currentDisposeLargeBlockList = pendingDisposeLargeBlockList;
    this->largeBlockList = nullptr;
#ifdef RECYCLER_PAGE_HEAP
    this->largePageHeapBlockList = nullptr;
#endif
    this->fullLargeBlockList = nullptr;

    // Clear the free list before sweep
    // We'll reconstruct the free list during sweep
    if (this->supportFreeList)
    {
#if DBG
        LargeAllocationVerboseTrace(recyclerSweep.GetRecycler()->GetRecyclerFlagsTable(), L"Resetting free list for 0x%x bucket\n", this->sizeCat);
#endif
        this->freeList = nullptr;
        this->explicitFreeList = nullptr;
    }

#ifdef CONCURRENT_GC_ENABLED
    Assert(this->pendingSweepLargeBlockList == nullptr);
#endif
    SweepLargeHeapBlockList<pageheap>(recyclerSweep, currentLargeObjectBlocks);
#ifdef RECYCLER_PAGE_HEAP
    SweepLargeHeapBlockList<pageheap>(recyclerSweep, currentLargePageHeapObjectBlocks);
#endif
    SweepLargeHeapBlockList<pageheap>(recyclerSweep, currentFullLargeObjectBlocks);
    SweepLargeHeapBlockList<pageheap>(recyclerSweep, currentDisposeLargeBlockList);
}

template<bool pageheap>
void
LargeHeapBucket::SweepLargeHeapBlockList(RecyclerSweep& recyclerSweep, LargeHeapBlock * heapBlockList)
{
    Recycler * recycler = recyclerSweep.GetRecycler();
    HeapBlockList::ForEachEditing(heapBlockList, [this, &recyclerSweep, recycler](LargeHeapBlock * heapBlock)
    {
        this->UnregisterFreeList(heapBlock->GetFreeList());

        // CONCURRENT-TODO: Allow large block to be sweep in the background
        SweepState state = heapBlock->Sweep<pageheap>(recyclerSweep, false);

        // If the block is already in the pending dispose list (re-entrant GC scenario), do nothing, leave it there
        if (heapBlock->IsInPendingDisposeList()) return;

        switch (state)
        {
        case SweepStateEmpty:
            heapBlock->ReleasePagesSweep<pageheap>(recycler);
            LargeHeapBlock::Delete(heapBlock);
            RECYCLER_SLOW_CHECK(this->heapInfo->heapBlockCount[HeapBlock::HeapBlockType::LargeBlockType]--);
            break;
        case SweepStateFull:
            heapBlock->SetNextBlock(this->fullLargeBlockList);
            this->fullLargeBlockList = heapBlock;
            break;
        case SweepStateSwept:
            if (supportFreeList)
            {
                ConstructFreelist(heapBlock);
            }
            else
            {
                ReinsertLargeHeapBlock(heapBlock);
            }

            break;
        case SweepStatePendingDispose:
            Assert(!recyclerSweep.IsBackground());
            Assert(!recycler->hasPendingTransferDisposedObjects);
            heapBlock->SetNextBlock(this->pendingDisposeLargeBlockList);
            this->pendingDisposeLargeBlockList = heapBlock;
            heapBlock->SetIsInPendingDisposeList(true);
#if DBG
            heapBlock->SetHasDisposeBeenCalled(false);
#endif
            recycler->hasDisposableObject = true;
            break;
#ifdef CONCURRENT_GC_ENABLED
        case SweepStatePendingSweep:
            heapBlock->SetNextBlock(this->pendingSweepLargeBlockList);
            this->pendingSweepLargeBlockList = heapBlock;
            break;
#endif
        }
    });
}

void
LargeHeapBucket::ReinsertLargeHeapBlock(LargeHeapBlock * heapBlock)
{
    Assert(!heapBlock->hasPartialFreeObjects);
    Assert(!heapBlock->IsInPendingDisposeList());

    if (this->largeBlockList != nullptr && heapBlock->GetFreeSize() > this->largeBlockList->GetFreeSize())
    {
        heapBlock->SetNextBlock(this->largeBlockList->GetNextBlock());
        this->largeBlockList->SetNextBlock(this->fullLargeBlockList);
        this->fullLargeBlockList = this->largeBlockList;
        this->largeBlockList = heapBlock;
    }
    else
    {
        heapBlock->SetNextBlock(this->fullLargeBlockList);
        this->fullLargeBlockList = heapBlock;
    }
}

void
LargeHeapBucket::RegisterFreeList(LargeHeapBlockFreeList* freeList)
{
    Assert(freeList->next == nullptr);
    Assert(freeList->previous == nullptr);

    LargeHeapBlockFreeList* head = this->freeList;

    if (head)
    {
        head->previous = freeList;
    }

    freeList->next = head;
    this->freeList = freeList;
}

void
LargeHeapBucket::UnregisterFreeList(LargeHeapBlockFreeList* freeList)
{
    LargeHeapBlockFreeList* next = freeList->next;
    LargeHeapBlockFreeList* previous = freeList->previous;

    if (previous)
    {
        previous->next = next;
    }

    if (next)
    {
        next->previous = previous;
    }

    freeList->next = nullptr;
    freeList->previous = nullptr;

    if (freeList == this->freeList)
    {
        this->freeList = next;
    }
}

void
LargeHeapBucket::ConstructFreelist(LargeHeapBlock * heapBlock)
{
    Assert(!heapBlock->hasPartialFreeObjects);
    Assert(!heapBlock->IsInPendingDisposeList());

    // The free list is the only way we reuse heap block entries
    // so if the heap block is allocated from directly, it'll not
    // invalidate the free list
    LargeHeapBlockFreeList* freeList = heapBlock->GetFreeList();
    Assert(freeList);

    if (freeList->entries)
    {
        this->RegisterFreeList(freeList);

#if DBG
        LargeAllocationVerboseTrace(this->GetRecycler()->GetRecyclerFlagsTable(), L"Free list created for 0x%x bucket\n", this->sizeCat);
#endif
    }

    ReinsertLargeHeapBlock(heapBlock);
}

#pragma endregion

#if defined(PARTIAL_GC_ENABLED) || defined(CONCURRENT_GC_ENABLED)
size_t
LargeHeapBucket::Rescan(LargeHeapBlock * list, Recycler * recycler, bool isPartialSwept, RescanFlags flags)
{
    size_t scannedPageCount = 0;
    HeapBlockList::ForEach(list, [recycler, isPartialSwept, flags, &scannedPageCount](LargeHeapBlock * heapBlock)
    {
        scannedPageCount += heapBlock->Rescan(recycler, isPartialSwept, flags);
    });
    return scannedPageCount;
}

size_t
LargeHeapBucket::Rescan(RescanFlags flags)
{
#ifdef CONCURRENT_GC_ENABLED
    Assert(pendingSweepLargeBlockList == nullptr);
#endif

    size_t scannedPageCount = 0;
    Recycler* recycler = this->heapInfo->recycler;

    scannedPageCount += LargeHeapBucket::Rescan(largeBlockList, recycler, false, flags);
#ifdef RECYCLER_PAGE_HEAP
    scannedPageCount += LargeHeapBucket::Rescan(largePageHeapBlockList, recycler, false, flags);
#endif
    scannedPageCount += LargeHeapBucket::Rescan(fullLargeBlockList, recycler, false, flags);
    scannedPageCount += LargeHeapBucket::Rescan(pendingDisposeLargeBlockList, recycler, true, flags);

#if defined(PARTIAL_GC_ENABLED) && defined(CONCURRENT_GC_ENABLED)
    Assert(recycler->inPartialCollectMode || partialSweptLargeBlockList == nullptr);
    if (recycler->inPartialCollectMode)
    {
        scannedPageCount += LargeHeapBucket::Rescan(partialSweptLargeBlockList, recycler, true, flags);
    }
#endif
    return scannedPageCount;
}

void
LargeHeapBucket::SweepPendingObjects(RecyclerSweep& recyclerSweep)
{
    if (recyclerSweep.IsBackground())
    {
        Recycler * recycler = recyclerSweep.GetRecycler();
        if (recycler->inPartialCollectMode)
        {
            HeapBlockList::ForEach(this->pendingSweepLargeBlockList, [recycler](LargeHeapBlock * heapBlock)
            {
                // Page heap blocks are never swept concurrently
                heapBlock->SweepObjects<false, SweepMode_ConcurrentPartial>(recycler);
            });
        }
        else
        {
            HeapBlockList::ForEach(this->pendingSweepLargeBlockList, [recycler](LargeHeapBlock * heapBlock)
            {
                // Page heap blocks are never swept concurrently
                heapBlock->SweepObjects<false, SweepMode_Concurrent>(recycler);
            });
        }
    }
    else
    {
        Assert(this->pendingSweepLargeBlockList == nullptr);
    }
}

#ifdef PARTIAL_GC_ENABLED
void
LargeHeapBucket::ConcurrentPartialTransferSweptObjects(RecyclerSweep& recyclerSweep)
{
    Assert(recyclerSweep.InPartialCollectMode());
    Assert(!recyclerSweep.IsBackground());

    RECYCLER_SLOW_CHECK(this->VerifyLargeHeapBlockCount());

    LargeHeapBlock * list = this->pendingSweepLargeBlockList;
    this->pendingSweepLargeBlockList = nullptr;
    HeapBlockList::ForEachEditing(list, [this](LargeHeapBlock * heapBlock)
    {
        // GC-REVIEW: We could maybe reuse the large objects
        heapBlock->PartialTransferSweptObjects();
        heapBlock->SetNextBlock(this->partialSweptLargeBlockList);
        this->partialSweptLargeBlockList = heapBlock;
    });

    RECYCLER_SLOW_CHECK(this->VerifyLargeHeapBlockCount());
}

void
LargeHeapBucket::FinishPartialCollect(RecyclerSweep * recyclerSweep)
{
#ifdef CONCURRENT_GC_ENABLED
    Recycler* recycler = this->heapInfo->recycler;

    if (recyclerSweep && recyclerSweep->IsBackground())
    {
        // Leave it in the partialSweptLargeBlockList if we are processing it in the background
        // ConcurrentTransferSweptObjects will put it back.
        HeapBlockList::ForEachEditing(partialSweptLargeBlockList, [this, recycler](LargeHeapBlock * heapBlock)
        {
            heapBlock->FinishPartialCollect(recycler);
        });
    }
    else
    {
        HeapBlockList::ForEachEditing(partialSweptLargeBlockList, [this, recycler](LargeHeapBlock * heapBlock)
        {
            heapBlock->FinishPartialCollect(recycler);
            this->ReinsertLargeHeapBlock(heapBlock);
        });
        this->partialSweptLargeBlockList = nullptr;
    }
#endif
}
#endif

#ifdef CONCURRENT_GC_ENABLED
void
LargeHeapBucket::ConcurrentTransferSweptObjects(RecyclerSweep& recyclerSweep)
{
    Assert(!recyclerSweep.InPartialCollectMode());
    Assert(!recyclerSweep.IsBackground());

    HeapBlockList::ForEachEditing(this->pendingSweepLargeBlockList, [this](LargeHeapBlock * heapBlock)
    {
        heapBlock->TransferSweptObjects();
        ReinsertLargeHeapBlock(heapBlock);
    });
    this->pendingSweepLargeBlockList = nullptr;

#ifdef PARTIAL_GC_ENABLED
    // If we did a background finish partial collect, we have left the partialSweptLargeBlockList
    // there because can't reinsert the heap block in the background, do it here now.
    HeapBlockList::ForEachEditing(this->partialSweptLargeBlockList, [this](LargeHeapBlock * heapBlock)
    {
        ReinsertLargeHeapBlock(heapBlock);
    });
    this->partialSweptLargeBlockList = nullptr;
#endif
}

#endif

#endif

void
LargeHeapBucket::FinalizeAllObjects()
{
    ForEachLargeHeapBlock([](LargeHeapBlock * heapBlock) { heapBlock->FinalizeAllObjects(); });
}


void
LargeHeapBucket::Finalize(Recycler * recycler, LargeHeapBlock * heapBlockList)
{
    HeapBlockList::ForEachEditing(heapBlockList, [recycler](LargeHeapBlock * heapBlock)
    {
        heapBlock->FinalizeObjects(recycler);
    });
}

void
LargeHeapBucket::Finalize()
{
    Recycler* recycler = this->heapInfo->recycler;

    // Finalize any free objects in the non-filled large heap blocks
    Finalize(recycler, largeBlockList);

#ifdef RECYCLER_PAGE_HEAP
    Finalize(recycler, largePageHeapBlockList);
#endif

    // Finalize any free objects in the filled large heap blocks
    Finalize(recycler, fullLargeBlockList);

    // Finalize any free objects in the large heap blocks which have objects pending dispose
    // This is to handle the case where if during dispose, a GC is triggered, we might have
    // found more objects to free. These objects might reside in a block that was moved to the
    // pendingDisposeLargeBlockList during the outer GC. So we need to walk through this list
    // again and finalize any objects that need to be finalized. If we don't, they would
    // not get finalized
    Finalize(recycler, pendingDisposeLargeBlockList);
}

void
LargeHeapBucket::DisposeObjects()
{
    Recycler * recycler = this->heapInfo->recycler;
    HeapBlockList::ForEach(this->pendingDisposeLargeBlockList, [recycler](LargeHeapBlock * heapBlock)
    {
        heapBlock->DisposeObjects(recycler);
    });
}

void
LargeHeapBucket::TransferDisposedObjects()
{
    Recycler * recycler = this->heapInfo->recycler;
    Assert(!recycler->IsConcurrentExecutingState());

    HeapBlockList::ForEachEditing(this->pendingDisposeLargeBlockList, [this](LargeHeapBlock * heapBlock)
    {
        /* GC-TODO: large heap block doesn't support free list yet */
        heapBlock->SetIsInPendingDisposeList(false);
        ReinsertLargeHeapBlock(heapBlock);
    });

    this->pendingDisposeLargeBlockList = nullptr;
}

void
LargeHeapBucket::EnumerateObjects(ObjectInfoBits infoBits, void (*CallBackFunction)(void * address, size_t size))
{
    HeapBucket::EnumerateObjects(largeBlockList, infoBits, CallBackFunction);
#ifdef RECYCLER_PAGE_HEAP
    HeapBucket::EnumerateObjects(largePageHeapBlockList, infoBits, CallBackFunction);
#endif
    HeapBucket::EnumerateObjects(fullLargeBlockList, infoBits, CallBackFunction);

    // Pending dispose large block list need not be null
    // When we enumerate over this list, anything that has been swept/finalized won't be
    // enumerated since it needs to have the object header for enumeration
    // and we set the header to null upon sweep/finalize
    HeapBucket::EnumerateObjects(pendingDisposeLargeBlockList, infoBits, CallBackFunction);
#ifdef CONCURRENT_GC_ENABLED
    Assert(this->pendingSweepLargeBlockList == nullptr);
#ifdef PARTIAL_GC_ENABLED
    HeapBucket::EnumerateObjects(partialSweptLargeBlockList, infoBits, CallBackFunction);
#endif
#endif
}

#if DBG || defined(RECYCLER_SLOW_CHECK_ENABLED)

size_t
LargeHeapBucket::GetLargeHeapBlockCount(bool checkCount) const
{
    size_t currentLargeHeapBlockCount = HeapBlockList::Count(fullLargeBlockList);
    currentLargeHeapBlockCount += HeapBlockList::Count(largeBlockList);
#ifdef RECYCLER_PAGE_HEAP
    currentLargeHeapBlockCount += HeapBlockList::Count(largePageHeapBlockList);
#endif
    currentLargeHeapBlockCount += HeapBlockList::Count(pendingDisposeLargeBlockList);
#ifdef CONCURRENT_GC_ENABLED
    currentLargeHeapBlockCount += HeapBlockList::Count(pendingSweepLargeBlockList);
#ifdef PARTIAL_GC_ENABLED
    currentLargeHeapBlockCount += HeapBlockList::Count(partialSweptLargeBlockList);
#endif
#endif

    return currentLargeHeapBlockCount;
}
#endif

#ifdef RECYCLER_SLOW_CHECK_ENABLED
size_t
LargeHeapBucket::Check()
{
    size_t currentLargeHeapBlockCount = Check(false, false, largeBlockList);
#ifdef RECYCLER_PAGE_HEAP
    currentLargeHeapBlockCount += Check(true, false, largePageHeapBlockList);
#endif
    currentLargeHeapBlockCount += Check(true, false, fullLargeBlockList);

#ifdef CONCURRENT_GC_ENABLED
    Assert(pendingSweepLargeBlockList == nullptr);
#ifdef PARTIAL_GC_ENABLED
    currentLargeHeapBlockCount += Check(false, false, partialSweptLargeBlockList);
#endif
#endif
    currentLargeHeapBlockCount += Check(false, true, pendingDisposeLargeBlockList);
    return currentLargeHeapBlockCount;
}

template <typename TBlockType>
size_t
LargeHeapBucket::Check(bool expectFull, bool expectPending, TBlockType * list, TBlockType * tail)
{
    size_t heapBlockCount = 0;
    HeapBlockList::ForEach(list, tail, [&heapBlockCount, expectFull, expectPending](TBlockType * heapBlock)
    {
        heapBlock->Check(expectFull, expectPending);
        heapBlockCount++;
    });
    return heapBlockCount;
}

template size_t LargeHeapBucket::Check<LargeHeapBlock>(bool expectFull, bool expectPending, LargeHeapBlock * list, LargeHeapBlock * tail);

void
LargeHeapBucket::VerifyLargeHeapBlockCount()
{
    GetLargeHeapBlockCount(true);
}
#endif

#ifdef RECYCLER_MEMORY_VERIFY
void
LargeHeapBucket::Verify()
{
    Recycler * recycler = this->heapInfo->recycler;
    HeapBlockList::ForEach(largeBlockList, [recycler](LargeHeapBlock * largeHeapBlock)
    {
        largeHeapBlock->Verify(recycler);
    });
#ifdef RECYCLER_PAGE_HEAP
    HeapBlockList::ForEach(largePageHeapBlockList, [recycler](LargeHeapBlock * largeHeapBlock)
    {
        largeHeapBlock->Verify(recycler);
    });
#endif
    HeapBlockList::ForEach(fullLargeBlockList, [recycler](LargeHeapBlock * largeHeapBlock)
    {
        largeHeapBlock->Verify(recycler);
    });
    HeapBlockList::ForEach(pendingDisposeLargeBlockList, [recycler](LargeHeapBlock * largeHeapBlock)
    {
        largeHeapBlock->Verify(recycler);
    });
}
#endif

#ifdef RECYCLER_VERIFY_MARK
void
LargeHeapBucket::VerifyMark()
{
    HeapBlockList::ForEach(largeBlockList, [](LargeHeapBlock * largeHeapBlock)
    {
        largeHeapBlock->VerifyMark();
    });
    HeapBlockList::ForEach(fullLargeBlockList, [](LargeHeapBlock * largeHeapBlock)
    {
        largeHeapBlock->VerifyMark();
    });
    HeapBlockList::ForEach(pendingDisposeLargeBlockList, [](LargeHeapBlock * largeHeapBlock)
    {
        largeHeapBlock->VerifyMark();
    });
}
#endif

