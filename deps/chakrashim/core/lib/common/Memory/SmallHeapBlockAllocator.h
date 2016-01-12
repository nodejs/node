//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once
namespace Memory
{
template <typename TBlockType>
class SmallHeapBlockAllocator
{
public:
    typedef TBlockType BlockType;

    SmallHeapBlockAllocator();
    void Initialize();

    template <ObjectInfoBits attributes>
    __inline char * InlinedAlloc(Recycler * recycler, size_t sizeCat);

    // Pass through template parameter to InlinedAllocImpl
    template <bool canFaultInject>
    __inline char * SlowAlloc(Recycler * recycler, size_t sizeCat, ObjectInfoBits attributes);

    // There are paths where we simply can't OOM here, so we shouldn't fault inject as it creates a bit of a mess
    template <bool canFaultInject>
    __inline char* InlinedAllocImpl(Recycler * recycler, size_t sizeCat, ObjectInfoBits attributes);

#ifdef RECYCLER_PAGE_HEAP
    __inline char *PageHeapAlloc(Recycler * recycler, size_t sizeCat, ObjectInfoBits attributes, PageHeapMode mode);
#endif

    TBlockType * GetHeapBlock() const { return heapBlock; }
    SmallHeapBlockAllocator * GetNext() const { return next; }

    void Set(TBlockType * heapBlock);
    void SetNew(TBlockType * heapBlock);
    void Clear();
    void UpdateHeapBlock();
    void SetExplicitFreeList(FreeObject* list);

    static uint32 GetEndAddressOffset() { return offsetof(SmallHeapBlockAllocator, endAddress); }
    char *GetEndAddress() { return endAddress; }
    static uint32 GetFreeObjectListOffset() { return offsetof(SmallHeapBlockAllocator, freeObjectList); }
    FreeObject *GetFreeObjectList() { return freeObjectList; }
    void SetFreeObjectList(FreeObject *freeObject) { freeObjectList = freeObject; }

#if defined(PROFILE_RECYCLER_ALLOC) || defined(RECYCLER_MEMORY_VERIFY) || defined(MEMSPECT_TRACKING) || defined(ETW_MEMORY_TRACKING)
    void SetTrackNativeAllocatedObjectCallBack(void (*pfnCallBack)(Recycler *, void *, size_t))
    {
        pfnTrackNativeAllocatedObjectCallBack = pfnCallBack;
    }
#endif
#if DBG
    FreeObject * GetExplicitFreeList() const
    {
        Assert(IsExplicitFreeObjectListAllocMode());
        return this->freeObjectList;
    }
#endif

    bool IsBumpAllocMode() const
    {
        return endAddress != nullptr;
    }
    bool IsExplicitFreeObjectListAllocMode() const
    {
        return this->heapBlock == nullptr;
    }
    bool IsFreeListAllocMode() const
    {
        return !IsBumpAllocMode() && !IsExplicitFreeObjectListAllocMode();
    }
private:
    static bool NeedSetAttributes(ObjectInfoBits attributes)
    {
        return attributes != LeafBit && (attributes & InternalObjectInfoBitMask) != 0;
    }

    char * endAddress;
    FreeObject * freeObjectList;
    TBlockType * heapBlock;

    SmallHeapBlockAllocator * prev;
    SmallHeapBlockAllocator * next;

    friend class HeapBucketT<BlockType>;
#ifdef RECYCLER_SLOW_CHECK_ENABLED
    template <class TBlockAttributes>
    friend class SmallHeapBlockT;
#endif
#ifdef PROFILE_RECYCLER_ALLOC
    HeapBucket * bucket;
#endif

#ifdef RECYCLER_TRACK_NATIVE_ALLOCATED_OBJECTS
    char * lastNonNativeBumpAllocatedBlock;
    void TrackNativeAllocatedObjects();
#endif
#if defined(PROFILE_RECYCLER_ALLOC) || defined(RECYCLER_MEMORY_VERIFY) || defined(MEMSPECT_TRACKING) || defined(ETW_MEMORY_TRACKING)
    void (*pfnTrackNativeAllocatedObjectCallBack)(Recycler * recycler, void *, size_t sizeCat);
#endif
};

#ifdef RECYCLER_PAGE_HEAP
template <typename TBlockType>
__inline
char *
SmallHeapBlockAllocator<TBlockType>::PageHeapAlloc(Recycler * recycler, size_t sizeCat, ObjectInfoBits attributes, PageHeapMode mode)
{
    if (this->heapBlock == nullptr)
    {
        return nullptr;
    }

    // In integrated page, we can get a heapBlock is not pageheap enabled
    if (!heapBlock->InPageHeapMode())
    {
        return nullptr;
    }

    TBlockType* smallBlock = (TBlockType*) this->heapBlock;

    // Free list allocation not supported in page heap mode
    // since if we sweep, the block should be empty
    Assert(this->endAddress != nullptr);


    // We do one of two things:
    //  1) Either bump allocate
    //  2) Allocate from the explicit free list
    // Explicit free list will already be at the right point in the heap block
    if ((char*)freeObjectList == smallBlock->address && !IsExplicitFreeObjectListAllocMode())
    {
        if (mode == PageHeapMode::PageHeapModeBlockEnd)
        {
            // Allocate at the last valid object
            // This could cause some extra padding at the end
            // e.g. For a Heap block with the range 0x1000 to 0x2000, and sizeCat 48
            // This can fit 85 objects in it, the last object at 0x1FC0, causing the
            // last 16 bytes to be wasted. So if there is a buffer overflow of <= 16 bytes,
            // PageHeap as implemented today will not catch it.
            char* objectAddress = smallBlock->GetAddress() + ((smallBlock->GetObjectCount() - 1) * sizeCat);
            Assert(objectAddress <= this->endAddress - sizeCat);
            freeObjectList = (FreeObject*)objectAddress;
        }
    }

    char* memBlock = InlinedAllocImpl<true/* allow fault injection */>(recycler, sizeCat, (ObjectInfoBits)attributes);

    if (memBlock)
    {
#if DBG
        HeapBlock* block = recycler->FindHeapBlock(memBlock);
        Assert(!block->IsLargeHeapBlock());
        ((TBlockType*)block)->VerifyPageHeapAllocation(memBlock, mode);
#endif

        PageHeapVerboseTrace(recycler->GetRecyclerFlagsTable(), L"Allocated from block 0x%p\n", smallBlock);

        // Close off allocation from this block
        this->freeObjectList = (FreeObject*) this->endAddress;

#ifdef RECYCLER_TRACK_NATIVE_ALLOCATED_OBJECTS
        this->lastNonNativeBumpAllocatedBlock = (char*) this->freeObjectList - sizeCat;
#endif

        if (recycler->ShouldCapturePageHeapAllocStack())
        {
            smallBlock->CapturePageHeapAllocStack();
        }
    }

    return memBlock;
}

#endif

template <typename TBlockType>
template <bool canFaultInject>
__inline char*
SmallHeapBlockAllocator<TBlockType>::InlinedAllocImpl(Recycler * recycler, size_t sizeCat, ObjectInfoBits attributes)
{
    Assert((attributes & InternalObjectInfoBitMask) == attributes);

    AUTO_NO_EXCEPTION_REGION;
    if (canFaultInject)
    {
        FAULTINJECT_MEMORY_NOTHROW(L"InlinedAllocImpl", sizeCat);
    }

    char * memBlock = (char *)freeObjectList;
    char * nextCurrentAddress = memBlock + sizeCat;
    char * endAddress = this->endAddress;

    if (nextCurrentAddress <= endAddress)
    {
        // Bump Allocation
        Assert(this->IsBumpAllocMode());
#ifdef RECYCLER_TRACK_NATIVE_ALLOCATED_OBJECTS
        TrackNativeAllocatedObjects();
        lastNonNativeBumpAllocatedBlock = memBlock;
#endif
        freeObjectList = (FreeObject *)nextCurrentAddress;

        if (NeedSetAttributes(attributes))
        {
            heapBlock->SetAttributes(memBlock, (attributes & StoredObjectInfoBitMask));
        }

        return memBlock;
    }

    if (memBlock != nullptr && endAddress == nullptr)
    {
        // Free list allocation
        Assert(!this->IsBumpAllocMode());
        if (NeedSetAttributes(attributes))
        {
            TBlockType * allocationHeapBlock = this->heapBlock;
            if (allocationHeapBlock == nullptr)
            {
                Assert(this->IsExplicitFreeObjectListAllocMode());
                allocationHeapBlock = (TBlockType *)recycler->FindHeapBlock(memBlock);
                Assert(allocationHeapBlock != nullptr);
                Assert(!allocationHeapBlock->IsLargeHeapBlock());
            }
            allocationHeapBlock->SetAttributes(memBlock, (attributes & StoredObjectInfoBitMask));
        }
        freeObjectList = ((FreeObject *)memBlock)->GetNext();

#ifdef RECYCLER_MEMORY_VERIFY
        ((FreeObject *)memBlock)->DebugFillNext();

        if (this->IsExplicitFreeObjectListAllocMode())
        {
            HeapBlock* heapBlock = recycler->FindHeapBlock(memBlock);
            Assert(heapBlock != nullptr);
            Assert(!heapBlock->IsLargeHeapBlock());
            TBlockType* smallBlock = (TBlockType*)heapBlock;
            smallBlock->ClearExplicitFreeBitForObject(memBlock);
        }
#endif

#if DBG || defined(RECYCLER_STATS)
        if (!IsExplicitFreeObjectListAllocMode())
        {
            BOOL isSet = heapBlock->GetDebugFreeBitVector()->TestAndClear(heapBlock->GetAddressBitIndex(memBlock));
            Assert(isSet);
        }
#endif
        return memBlock;
    }

    return nullptr;
}


template <typename TBlockType>
template <ObjectInfoBits attributes>
__inline char *
SmallHeapBlockAllocator<TBlockType>::InlinedAlloc(Recycler * recycler, size_t sizeCat)
{
    return InlinedAllocImpl<true /* allow fault injection */>(recycler, sizeCat, attributes);
}

template <typename TBlockType>
template <bool canFaultInject>
__inline
char *
SmallHeapBlockAllocator<TBlockType>::SlowAlloc(Recycler * recycler, size_t sizeCat, ObjectInfoBits attributes)
{
    Assert((attributes & InternalObjectInfoBitMask) == attributes);

    return InlinedAllocImpl<canFaultInject>(recycler, sizeCat, attributes);
}
}
