//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once


template <ObjectInfoBits attributes>
bool
Recycler::IntegrateBlock(char * blockAddress, PageSegment * segment, size_t allocSize, size_t objectSize)
{
    // We only support no bit and leaf bit right now, where we don't need to set the object info in either case
    CompileAssert(attributes == NoBit || attributes == LeafBit);

    // Currently only small object is supported
    Assert(HeapInfo::IsSmallObject(allocSize));
    Assert(HeapInfo::GetAlignedSizeNoCheck(allocSize) == allocSize);

    // This should only happen during a pre-collection callback.
    Assert(this->collectionState == Collection_PreCollection);

    bool success = autoHeap.IntegrateBlock<attributes>(blockAddress, segment, this, allocSize);
#ifdef PROFILE_RECYCLER_ALLOC
    if (success)
    {
        TrackAllocData trackAllocData;
        ClearTrackAllocInfo(&trackAllocData);

        TrackIntegrate(blockAddress, SmallAllocationBlockAttributes::PageCount * AutoSystemInfo::PageSize, allocSize, objectSize, trackAllocData);
    }
#endif
    return success;
}

namespace Memory
{
class DummyVTableObject : public FinalizableObject
{
public:
    virtual void Finalize(bool isShutdown) {}
    virtual void Dispose(bool isShutdown) {}
    virtual void Mark(Recycler * recycler) {}
};
}

template <ObjectInfoBits attributes, bool nothrow>
__inline char *
Recycler::AllocWithAttributesInlined(size_t size)
{
    // All tracked objects are client tracked objects
    CompileAssert((attributes & TrackBit) == 0 || (attributes & ClientTrackedBit) != 0);
    Assert(this->enableScanImplicitRoots || (attributes & ImplicitRootBit) == 0);
    AssertMsg(this->disableThreadAccessCheck || this->mainThreadId == GetCurrentThreadContextId(),
        "Allocating from the recycler can only be done on the main thread");
    Assert(size != 0);
    AssertMsg(collectionState != Collection_PreCollection, "we cannot have allocation in precollection callback");

    // We shouldn't be allocating memory when we are running GC in thread, including finalizers
    Assert(this->IsConcurrentState() || !this->CollectionInProgress() || this->collectionState == CollectionStatePostCollectionCallback);

    // There are some cases where we allow allocation during heap enum that doesn't affect the enumeration
    // Those should be really rare and not rely upon.
    Assert(!isHeapEnumInProgress || allowAllocationDuringHeapEnum);
#ifdef PROFILE_RECYCLER_ALLOC
    TrackAllocData trackAllocData;
    ClearTrackAllocInfo(&trackAllocData);
#endif

    size_t allocSize = size;
#ifdef RECYCLER_MEMORY_VERIFY
    if (this->VerifyEnabled())
    {
        allocSize += verifyPad + sizeof(size_t);
        if (allocSize < size)
        {
            // An overflow occurred- if nothrow is false, we can throw here
            // Otherwise, return null
            if (nothrow == false)
            {
                this->OutOfMemory();
            }
            else
            {
                return nullptr;
            }
        }
    }
#endif

    char* memBlock = RealAlloc<(ObjectInfoBits)(attributes & InternalObjectInfoBitMask), nothrow>(&autoHeap, allocSize);

    if (nothrow)
    {
        // If we aren't allowed to throw, then the memblock returned could be null
        // so we should check for that and bail out early here
        if (memBlock == nullptr)
        {
            return nullptr;
        }
    }

#ifdef PROFILE_RECYCLER_ALLOC
    TrackAlloc(memBlock, size, trackAllocData, (CUSTOM_CONFIG_ISENABLED(GetRecyclerFlagsTable(), Js::TraceObjectAllocationFlag) && (attributes & TraceBit) == TraceBit));
#endif
    RecyclerMemoryTracking::ReportAllocation(this, memBlock, size);
    RECYCLER_PERF_COUNTER_INC(LiveObject);
    RECYCLER_PERF_COUNTER_ADD(LiveObjectSize, HeapInfo::GetAlignedSizeNoCheck(allocSize));
    RECYCLER_PERF_COUNTER_SUB(FreeObjectSize, HeapInfo::GetAlignedSizeNoCheck(allocSize));

    if (HeapInfo::IsSmallBlockAllocation(HeapInfo::GetAlignedSizeNoCheck(allocSize)))
    {
        RECYCLER_PERF_COUNTER_INC(SmallHeapBlockLiveObject);
        RECYCLER_PERF_COUNTER_ADD(SmallHeapBlockLiveObjectSize, HeapInfo::GetAlignedSizeNoCheck(allocSize));
        RECYCLER_PERF_COUNTER_SUB(SmallHeapBlockFreeObjectSize, HeapInfo::GetAlignedSizeNoCheck(allocSize));
    }
    else
    {
        RECYCLER_PERF_COUNTER_INC(LargeHeapBlockLiveObject);
        RECYCLER_PERF_COUNTER_ADD(LargeHeapBlockLiveObjectSize, HeapInfo::GetAlignedSizeNoCheck(allocSize));
        RECYCLER_PERF_COUNTER_SUB(LargeHeapBlockFreeObjectSize, HeapInfo::GetAlignedSizeNoCheck(allocSize));
    }

#ifdef RECYCLER_MEMORY_VERIFY
    size_t alignedSize = HeapInfo::GetAlignedSizeNoCheck(allocSize);

    if (HeapInfo::IsMediumObject(allocSize))
    {
#if SMALLBLOCK_MEDIUM_ALLOC
        alignedSize = HeapInfo::GetMediumObjectAlignedSizeNoCheck(allocSize);
#else
        HeapBlock* heapBlock = this->FindHeapBlock(memBlock);
        Assert(heapBlock->IsLargeHeapBlock());
        LargeHeapBlock* largeHeapBlock = (LargeHeapBlock*) heapBlock;
        LargeObjectHeader* header = nullptr;
        if (largeHeapBlock->GetObjectHeader(memBlock, &header))
        {
            size = header->objectSize - (verifyPad + sizeof(size_t));
            alignedSize = HeapInfo::GetAlignedSizeNoCheck(header->objectSize);
        }
#endif
    }

    this->FillCheckPad(memBlock, size, alignedSize);
#endif


#pragma prefast(suppress:6313, "attributes is a template parameter and can be 0")
    if (attributes & (FinalizeBit | TrackBit))
    {
        // Make sure a valid vtable is installed in case of OOM before the real vtable is set
        memBlock = (char *)new (memBlock) DummyVTableObject();
    }

#ifdef RECYCLER_WRITE_BARRIER
    SwbVerboseTrace(this->GetRecyclerFlagsTable(), L"Allocated SWB memory: 0x%p\n", memBlock);

#pragma prefast(suppress:6313, "attributes is a template parameter and can be 0")
    if (attributes & (NewTrackBit))
    {
        // For objects allocated with NewTrackBit, we need to trigger the write barrier since
        // there could be a GC triggered by an allocation in the constructor, and we'd miss
        // calling track on the partially constructed object. To deal with this, we set the write
        // barrier on all the pages of objects allocated with the NewTrackBit
        RecyclerWriteBarrierManager::WriteBarrier(memBlock, size / sizeof(void*));
    }
#endif

#ifdef PARTIAL_GC_ENABLED
#pragma prefast(suppress:6313, "attributes is a template parameter and can be 0")
    if (attributes & ClientTrackedBit)
    {
        if (this->inPartialCollectMode)
        {
            // with partial GC, we don't traverse ITrackable
            // So we have to mark all objects that could be in the ITrackable graph
            // This includes JavascriptDispatch and HostVariant
            this->clientTrackedObjectList.Prepend(&this->clientTrackedObjectAllocator, memBlock);
        }
        else
        {
            Assert(this->hasBackgroundFinishPartial || this->clientTrackedObjectList.Empty());
        }
    }
#endif
    return memBlock;
}

template <ObjectInfoBits attributes, bool nothrow>
__inline char *
Recycler::AllocZeroWithAttributesInlined(size_t size)
{
    char* obj = AllocWithAttributesInlined<attributes, nothrow>(size);

    if (nothrow)
    {
        // If we aren't allowed to throw, then the obj returned could be null
        // so we should check for that and bail out early here
        if (obj == nullptr)
        {
            return nullptr;
        }
    }

#ifdef RECYCLER_MEMORY_VERIFY
    if (this->VerifyEnabled())
    {
        memset(obj, 0, min(size, sizeof(FreeObject *)));
    }
    else
#endif
#ifdef RECYCLER_WRITE_BARRIER_ALLOC_THREAD_PAGE
    if ((((attributes & LeafBit) == LeafBit) || ((attributes & WithBarrierBit) == WithBarrierBit)) && HeapInfo::IsSmallObjectAllocation(size))
#else
    if (((attributes & LeafBit) == LeafBit) && HeapInfo::IsSmallBlockAllocation(size))
#endif
    {
        // If this was allocated from the small heap block, it's not
        // guaranteed to be zero so we should zero out here.
        memset((void*) obj, 0, size);
    }
    else
    {
        // All recycler memory are allocated with zero except for the first word,
        // which store the next pointer for the free list.  Just zero that one out
        ((FreeObject *)obj)->ZeroNext();
    }
    return obj;
}

template <ObjectInfoBits attributes, bool isSmallAlloc, bool nothrow>
__inline char*
Recycler::RealAllocFromBucket(HeapInfo* heap, size_t size)
{
    // Align the size
    Assert(HeapInfo::GetAlignedSizeNoCheck(size) <= UINT_MAX);
    uint sizeCat;
    char * memBlock;

    if (isSmallAlloc)
    {
        sizeCat = (uint)HeapInfo::GetAlignedSizeNoCheck(size);
        memBlock = heap->RealAlloc<attributes, nothrow>(this, sizeCat);
    }
#ifdef BUCKETIZE_MEDIUM_ALLOCATIONS
    else
    {
        sizeCat = (uint)HeapInfo::GetMediumObjectAlignedSizeNoCheck(size);
        memBlock = heap->MediumAlloc<attributes, nothrow>(this, sizeCat);
    }
#endif

    // If we are not allowed to throw, then the memory returned here could be null so check for that
    // If we are allowed to throw, then memBlock is not allowed to null so assert that
    if (nothrow)
    {
        if (memBlock == nullptr)
        {
            return nullptr;
        }
    }
    else
    {
        Assert(memBlock != nullptr);
    }

#ifdef RECYCLER_ZERO_MEM_CHECK
    // Don't bother checking leaf allocations for zeroing out- they're not guaranteed to be so
    if ((attributes & LeafBit) == 0
#ifdef RECYCLER_WRITE_BARRIER_ALLOC_THREAD_PAGE
        && (attributes & WithBarrierBit) == 0
#endif
        )
    {
        VerifyZeroFill(memBlock + sizeof(FreeObject), sizeCat - (2 * sizeof(FreeObject)));
    }
#endif
#ifdef PROFILE_MEM
    if (this->memoryData)
    {
        this->memoryData->requestCount++;
        this->memoryData->requestBytes += size;
        this->memoryData->alignmentBytes += sizeCat - size;
    }
#endif

    return memBlock;

}

template <ObjectInfoBits attributes, bool nothrow>
__inline char*
Recycler::RealAlloc(HeapInfo* heap, size_t size)
{
#ifdef RECYCLER_STRESS
    this->StressCollectNow();
#endif

    if (nothrow)
    {
        FAULTINJECT_MEMORY_NOTHROW(L"Recycler", size);
    }
    else
    {
        FAULTINJECT_MEMORY_THROW(L"Recycler", size);
    }

    if (HeapInfo::IsSmallObject(size))
    {
        return RealAllocFromBucket<attributes, /* isSmallAlloc = */ true, nothrow>(heap, size);
    }

#ifdef BUCKETIZE_MEDIUM_ALLOCATIONS
    if (HeapInfo::IsMediumObject(size))
    {
        return RealAllocFromBucket<attributes, /* isSmallAlloc = */ false, nothrow>(heap, size);
    }
#endif

    return LargeAlloc<nothrow>(heap, size, attributes);
}

template<typename T>
__inline RecyclerWeakReference<T>* Recycler::CreateWeakReferenceHandle(T* pStrongReference)
{
    // Return the weak reference that calling Add on the WR map returns
    // The entry returned is recycler-allocated memory
    RecyclerWeakReference<T>* weakRef = (RecyclerWeakReference<T>*) this->weakReferenceMap.Add((char*) pStrongReference, this);
#if DBG
    if (weakRef->typeInfo == nullptr)
    {
        weakRef->typeInfo = &typeid(T);
#ifdef TRACK_ALLOC
        TrackAllocWeakRef(weakRef);
#endif
}
#endif
    return weakRef;
}

template<typename T>
__inline bool Recycler::FindOrCreateWeakReferenceHandle(T* pStrongReference, RecyclerWeakReference<T> **ppWeakRef)
{
    // Ensure that the given strong ref has a weak ref in the map.
    // Return a result to indicate whether a new weak ref was created.
    bool ret = this->weakReferenceMap.FindOrAdd((char*) pStrongReference, this, (RecyclerWeakReferenceBase**)ppWeakRef);
#if DBG
    if (!ret)
    {
        (*ppWeakRef)->typeInfo = &typeid(T);
#ifdef TRACK_ALLOC
        TrackAllocWeakRef(*ppWeakRef);
#endif
    }
#endif
    return ret;
}

template<typename T>
__inline bool Recycler::TryGetWeakReferenceHandle(T* pStrongReference, RecyclerWeakReference<T> **weakReference)
{
    return this->weakReferenceMap.TryGetValue((char*) pStrongReference, (RecyclerWeakReferenceBase**)weakReference);
}

__inline HeapBlock*
Recycler::FindHeapBlock(void* candidate)
{
    if ((size_t)candidate < 0x10000)
    {
        return nullptr;
    }

    if (!HeapInfo::IsAlignedAddress(candidate))
    {
        return nullptr;
    }
    return heapBlockMap.GetHeapBlock(candidate);
}

__inline void
Recycler::ScanObjectInline(void ** obj, size_t byteCount)
{
    // This is never called during parallel marking
    Assert(this->collectionState != CollectionStateParallelMark);
    if (this->enableScanInteriorPointers)
    {
        ScanObjectInlineInterior(obj, byteCount);
    }
    else
    {
        markContext.ScanObject<false, false>(obj, byteCount);
    }
}

__inline void
Recycler::ScanObjectInlineInterior(void ** obj, size_t byteCount)
{
    // This is never called during parallel marking
    Assert(this->collectionState != CollectionStateParallelMark);
    Assert(this->enableScanInteriorPointers);
    markContext.ScanObject<false, true>(obj, byteCount);
}

__inline void
Recycler::ScanMemoryInline(void ** obj, size_t byteCount)
{
    // This is never called during parallel marking
    Assert(this->collectionState != CollectionStateParallelMark);
    if (this->enableScanInteriorPointers)
    {
        markContext.ScanMemory<false, true>(obj, byteCount);
    }
    else
    {
        markContext.ScanMemory<false, false>(obj, byteCount);
    }
}

__inline bool
Recycler::AddMark(void * candidate, size_t byteCount) throw()
{
    // This is never called during parallel marking
    Assert(this->collectionState != CollectionStateParallelMark);
    return markContext.AddMarkedObject(candidate, byteCount);
}


template <bool pageheap, typename T>
void
Recycler::NotifyFree(T * heapBlock)
{
    bool forceSweepObject = ForceSweepObject();

    if (forceSweepObject)
    {
#if DBG || defined(RECYCLER_STATS)
        this->isForceSweeping = true;
        heapBlock->isForceSweeping = true;
#endif
        heapBlock->SweepObjects<pageheap, SweepMode_InThread>(this);
#if DBG || defined(RECYCLER_STATS)
        heapBlock->isForceSweeping = false;
        this->isForceSweeping = false;
#endif
        RECYCLER_STATS_INC(this, heapBlockFreeCount[heapBlock->GetHeapBlockType()]);
    }
    JS_ETW(EventWriteFreeMemoryBlock(heapBlock));
#ifdef RECYCLER_PERF_COUNTERS
    if (forceSweepObject)
    {
        RECYCLER_PERF_COUNTER_SUB(FreeObjectSize, heapBlock->GetPageCount() * AutoSystemInfo::PageSize);

        if (heapBlock->IsLargeHeapBlock())
        {
            RECYCLER_PERF_COUNTER_SUB(LargeHeapBlockFreeObjectSize, heapBlock->GetPageCount() * AutoSystemInfo::PageSize);
        }
        else
        {
            RECYCLER_PERF_COUNTER_SUB(SmallHeapBlockFreeObjectSize, heapBlock->GetPageCount() * AutoSystemInfo::PageSize);
        }
    }
    else
    {
        heapBlock->UpdatePerfCountersOnFree();
    }
#endif
}

template <class TBlockAttributes>
__inline ushort
SmallHeapBlockT<TBlockAttributes>::GetObjectBitDelta()
{
    return this->objectSize / HeapConstants::ObjectGranularity;
}

// Map any object address to it's bit index in the heap block bit vectors.
// static
template <class TBlockAttributes>
__forceinline ushort
SmallHeapBlockT<TBlockAttributes>::GetAddressBitIndex(void * objectAddress)
{
    Assert(HeapInfo::IsAlignedAddress(objectAddress));

    ushort offset = (ushort)(::Math::PointerCastToIntegralTruncate<uint>(objectAddress) % (TBlockAttributes::PageCount * AutoSystemInfo::PageSize));
    offset = offset >> HeapConstants::ObjectAllocationShift;

    Assert(offset <= USHRT_MAX);
    Assert(offset <= TBlockAttributes::MaxAddressBit);
    return (ushort) offset;
}

template <class TBlockAttributes>
__forceinline ushort
SmallHeapBlockT<TBlockAttributes>::GetObjectIndexFromBitIndex(ushort bitIndex)
{
    Assert(bitIndex <= TBlockAttributes::MaxAddressBit);

    ushort objectIndex = validPointers.GetAddressIndex(bitIndex);
    Assert(objectIndex == SmallHeapBlockT<TBlockAttributes>::InvalidAddressBit ||
        objectIndex <= TBlockAttributes::MaxAddressBit);
    return objectIndex;
}

template <class TBlockAttributes>
__forceinline void *
SmallHeapBlockT<TBlockAttributes>::GetRealAddressFromInterior(void * interiorAddress, uint objectSize, byte bucketIndex)
{
    const ValidPointers<TBlockAttributes> validPointers = HeapInfo::GetValidPointersMapForBucket<TBlockAttributes>(bucketIndex);
    size_t rawInteriorAddress = reinterpret_cast<size_t>(interiorAddress);
    size_t baseAddress = rawInteriorAddress & ~(TBlockAttributes::PageCount * AutoSystemInfo::PageSize - 1);
    ushort offset = (ushort)(rawInteriorAddress - baseAddress);
    offset = validPointers.GetInteriorAddressIndex(offset >> HeapConstants::ObjectAllocationShift);

    if (offset == SmallHeapBlockT<TBlockAttributes>::InvalidAddressBit)
    {
        return nullptr;
    }

    return reinterpret_cast<void*>(baseAddress + offset * objectSize);
}
