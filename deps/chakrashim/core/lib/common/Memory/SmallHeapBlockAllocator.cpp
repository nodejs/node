//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonMemoryPch.h"

EXPLICIT_INSTANTIATE_WITH_SMALL_HEAP_BLOCK_TYPE(SmallHeapBlockAllocator)

template __forceinline char* SmallHeapBlockAllocator<SmallNormalHeapBlock>::InlinedAllocImpl</*canFaultInject*/true>(Recycler * recycler, size_t sizeCat, ObjectInfoBits attributes);

template <typename TBlockType>
SmallHeapBlockAllocator<TBlockType>::SmallHeapBlockAllocator() :
    freeObjectList(nullptr),
    endAddress(nullptr),
    heapBlock(nullptr),
    prev(nullptr),
    next(nullptr)
{
#ifdef RECYCLER_TRACK_NATIVE_ALLOCATED_OBJECTS
    this->lastNonNativeBumpAllocatedBlock = nullptr;
#endif
}

template <typename TBlockType>
void
SmallHeapBlockAllocator<TBlockType>::Initialize()
{
    Assert(this->freeObjectList == nullptr);
    Assert(this->endAddress == nullptr);
    Assert(this->heapBlock == nullptr);

    this->prev = this;
    this->next = this;
}

template <typename TBlockType>
void
SmallHeapBlockAllocator<TBlockType>::UpdateHeapBlock()
{
    if (heapBlock != nullptr)
    {
        if (this->endAddress == nullptr)
        {
            heapBlock->freeObjectList = this->freeObjectList;
        }
        else
        {
            Assert(heapBlock->freeObjectList == nullptr);
        }
    }
}

template <typename TBlockType>
void
SmallHeapBlockAllocator<TBlockType>::Clear()
{
    TBlockType * heapBlock = this->heapBlock;
    if (heapBlock != nullptr)
    {
        Assert(heapBlock->isInAllocator);
        heapBlock->isInAllocator = false;
        FreeObject * remainingFreeObjectList = nullptr;
        if (this->endAddress != nullptr)
        {
#ifdef RECYCLER_TRACK_NATIVE_ALLOCATED_OBJECTS
            TrackNativeAllocatedObjects();
            lastNonNativeBumpAllocatedBlock = nullptr;
#endif
#ifdef PROFILE_RECYCLER_ALLOC
            // Need to tell the tracker
            this->bucket->heapInfo->recycler->TrackUnallocated((char *)this->freeObjectList, this->endAddress, this->bucket->sizeCat);
#endif
            RecyclerMemoryTracking::ReportUnallocated(this->heapBlock->heapBucket->heapInfo->recycler, (char *)this->freeObjectList, this->endAddress, heapBlock->heapBucket->sizeCat);
#ifdef RECYCLER_PERF_COUNTERS
            size_t unallocatedObjects = heapBlock->objectCount - ((char *)this->freeObjectList - heapBlock->address) / heapBlock->objectSize;
            size_t unallocatedObjectBytes = unallocatedObjects * heapBlock->GetObjectSize();
            RECYCLER_PERF_COUNTER_ADD(LiveObject, unallocatedObjects);
            RECYCLER_PERF_COUNTER_ADD(LiveObjectSize, unallocatedObjectBytes);
            RECYCLER_PERF_COUNTER_SUB(FreeObjectSize, unallocatedObjectBytes);
            RECYCLER_PERF_COUNTER_ADD(SmallHeapBlockLiveObject, unallocatedObjects);
            RECYCLER_PERF_COUNTER_ADD(SmallHeapBlockLiveObjectSize, unallocatedObjectBytes);
            RECYCLER_PERF_COUNTER_SUB(SmallHeapBlockFreeObjectSize, unallocatedObjectBytes);
#endif
            Assert(heapBlock->freeObjectList == nullptr);
            this->endAddress = nullptr;
        }
        else
        {
            remainingFreeObjectList = this->freeObjectList;
            heapBlock->freeObjectList = remainingFreeObjectList;
        }
        this->freeObjectList = nullptr;

        // this->freeObjectList and this->lastFreeCount are accessed in SmallHeapBlock::ResetMarks
        // the order of access there is first we see if lastFreeCount = 0, and if it is, we assert
        // that freeObjectList = null. Because of ARM's memory model, we need to insert barriers
        // so that the two variables can be accessed correctly across threads. Here, after we write
        // to this->freeObjectList, we insert a write barrier so that if this->lastFreeCount is 0,
        // this->freeObjectList must have been set to null. On the other end, we stick a read barrier
        // We use the MemoryBarrier macro because of ARMs lack of a separate read barrier
#if defined(_M_ARM32_OR_ARM64)
#if DBG
        MemoryBarrier();
#endif
#endif

        if (remainingFreeObjectList == nullptr)
        {
            uint lastFreeCount = heapBlock->GetAndClearLastFreeCount();
            heapBlock->heapBucket->heapInfo->uncollectedAllocBytes += lastFreeCount * heapBlock->GetObjectSize();
            Assert(heapBlock->lastUncollectedAllocBytes == 0);
            DebugOnly(heapBlock->lastUncollectedAllocBytes = lastFreeCount * heapBlock->GetObjectSize());
        }
        else
        {
            DebugOnly(heapBlock->SetIsClearedFromAllocator(true));
        }
        this->heapBlock = nullptr;

        RECYCLER_SLOW_CHECK(heapBlock->CheckDebugFreeBitVector(false));
    }
    else if (this->freeObjectList != nullptr)
    {
        // Explicit Free Object List
#ifdef RECYCLER_MEMORY_VERIFY
        FreeObject* freeObject = this->freeObjectList;

        while (freeObject)
        {
            HeapBlock* heapBlock = this->bucket->GetRecycler()->FindHeapBlock((void*) freeObject);
            Assert(heapBlock != nullptr);
            Assert(!heapBlock->IsLargeHeapBlock());
            TBlockType* smallBlock = (TBlockType*)heapBlock;

            smallBlock->ClearExplicitFreeBitForObject((void*) freeObject);
            freeObject = freeObject->GetNext();
        }
#endif
        this->freeObjectList = nullptr;
    }

}

template <typename TBlockType>
void
SmallHeapBlockAllocator<TBlockType>::SetNew(BlockType * heapBlock)
{
    Assert(this->endAddress == nullptr);
    Assert(this->heapBlock == nullptr);
    Assert(this->freeObjectList == nullptr);

    Assert(heapBlock != nullptr);
    Assert(heapBlock->freeObjectList == nullptr);
    Assert(heapBlock->lastFreeCount != 0);

    Assert(!heapBlock->isInAllocator);
    heapBlock->isInAllocator = true;

    this->heapBlock = heapBlock;
    this->freeObjectList = (FreeObject *)heapBlock->GetAddress();
    this->endAddress = heapBlock->GetEndAddress();
}

template <typename TBlockType>
void
SmallHeapBlockAllocator<TBlockType>::Set(BlockType * heapBlock)
{
    Assert(this->endAddress == nullptr);
    Assert(this->heapBlock == nullptr);
    Assert(this->freeObjectList == nullptr);

    Assert(heapBlock != nullptr);
    Assert(heapBlock->freeObjectList != nullptr);
    Assert(heapBlock->lastFreeCount != 0);

    Assert(!heapBlock->isInAllocator);
    heapBlock->isInAllocator = true;

    this->heapBlock = heapBlock;
    RECYCLER_SLOW_CHECK(this->heapBlock->CheckDebugFreeBitVector(true));
    this->freeObjectList = this->heapBlock->freeObjectList;
}


template <typename TBlockType>
void
SmallHeapBlockAllocator<TBlockType>::SetExplicitFreeList(FreeObject* list)
{
    Assert(list != nullptr);
    Assert(this->heapBlock == nullptr);
    Assert(this->freeObjectList == nullptr);

    this->freeObjectList = list;
}

#ifdef RECYCLER_TRACK_NATIVE_ALLOCATED_OBJECTS
template <typename TBlockType>
void
SmallHeapBlockAllocator<TBlockType>::TrackNativeAllocatedObjects()
{
    Assert(this->freeObjectList != nullptr && endAddress != nullptr);
    Assert(this->heapBlock != nullptr);

#if defined(PROFILE_RECYCLER_ALLOC) || defined(RECYCLER_MEMORY_VERIFY) || defined(MEMSPECT_TRACKING) || defined(ETW_MEMORY_TRACKING)
    if (pfnTrackNativeAllocatedObjectCallBack == nullptr)
    {
        return;
    }

    if (lastNonNativeBumpAllocatedBlock == nullptr)
    {
#ifdef RECYCLER_PAGE_HEAP
        Assert((char *)this->freeObjectList == this->heapBlock->GetAddress() || ((SmallHeapBlock*) this->heapBlock)->InPageHeapMode());
#else
        Assert((char *)this->freeObjectList == this->heapBlock->GetAddress());
#endif
        return;
    }

    Recycler * recycler = this->heapBlock->heapBucket->heapInfo->recycler;
    size_t sizeCat = this->heapBlock->heapBucket->sizeCat;
    char * curr = lastNonNativeBumpAllocatedBlock + sizeCat;
    Assert(curr <= (char *)this->freeObjectList);

#if DBG_DUMP
    AllocationVerboseTrace(recycler->GetRecyclerFlagsTable(), L"TrackNativeAllocatedObjects: recycler = 0x%p, sizeCat = %u, lastRuntimeAllocatedBlock = 0x%p, freeObjectList = 0x%p, nativeAllocatedObjectCount = %u\n",
        recycler, sizeCat, this->lastNonNativeBumpAllocatedBlock, this->freeObjectList, ((char *)this->freeObjectList - curr) / sizeCat);
#endif

    while (curr < (char *)this->freeObjectList)
    {
        pfnTrackNativeAllocatedObjectCallBack(recycler, curr, sizeCat);
        curr += sizeCat;
    }
#elif defined(RECYCLER_PERF_COUNTERS)
    if (lastNonNativeBumpAllocatedBlock == nullptr)
    {
        return;
    }

    size_t sizeCat = this->heapBlock->heapBucket->sizeCat;
    char * curr = lastNonNativeBumpAllocatedBlock + sizeCat;
    Assert(curr <= (char *)this->freeObjectList);
    size_t byteCount = ((char *)this->freeObjectList - curr);

#if DBG_DUMP
    AllocationVerboseTrace(L"TrackNativeAllocatedObjects: recycler = 0x%p, sizeCat = %u, lastRuntimeAllocatedBlock = 0x%p, freeObjectList = 0x%p, nativeAllocatedObjectCount = %u\n",
        recycler, sizeCat, this->lastNonNativeBumpAllocatedBlock, this->freeObjectList, ((char *)this->freeObjectList - curr) / sizeCat);
#endif

    RECYCLER_PERF_COUNTER_ADD(LiveObject, byteCount / sizeCat);
    RECYCLER_PERF_COUNTER_ADD(LiveObjectSize, byteCount);
    RECYCLER_PERF_COUNTER_SUB(FreeObjectSize, byteCount);
    RECYCLER_PERF_COUNTER_ADD(SmallHeapBlockLiveObject, byteCount / sizeCat);
    RECYCLER_PERF_COUNTER_ADD(SmallHeapBlockLiveObjectSize, byteCount);
    RECYCLER_PERF_COUNTER_SUB(SmallHeapBlockFreeObjectSize, byteCount);
#else
#error Not implemented
#endif
}
#endif
