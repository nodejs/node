//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Memory
{
// a size bucket in the heap for small object

class HeapInfo;
class RecyclerSweep;
#if DBG
template <class TBlockAttributes>
class GenericRecyclerVerifyListConsistencyData
{
public:
    GenericRecyclerVerifyListConsistencyData() {};

    // Temporary data for Sweep list consistency checks
    bool expectFull;
    bool expectDispose;
    SmallHeapBlockT<TBlockAttributes> * nextAllocableBlockHead;
    bool hasSetupVerifyListConsistencyData;

    template <typename TBlockAttributes>
    void SetupVerifyListConsistencyData(SmallHeapBlockT<TBlockAttributes>* block, bool expectFull, bool expectDispose)
    {
        this->nextAllocableBlockHead = block;
        this->expectFull = expectFull;
        this->expectDispose = expectDispose;
        this->hasSetupVerifyListConsistencyData = true;
    }
};

class RecyclerVerifyListConsistencyData
{
public:
    RecyclerVerifyListConsistencyData() {};
    void SetupVerifyListConsistencyDataForSmallBlock(SmallHeapBlock* block, bool expectFull, bool expectDispose)
    {
        this->smallBlockVerifyListConsistencyData.nextAllocableBlockHead = block;
        this->smallBlockVerifyListConsistencyData.expectFull = expectFull;
        this->smallBlockVerifyListConsistencyData.expectDispose = expectDispose;
        this->smallBlockVerifyListConsistencyData.hasSetupVerifyListConsistencyData = true;
    }

    void SetupVerifyListConsistencyDataForMediumBlock(MediumHeapBlock* block, bool expectFull, bool expectDispose)
    {
        this->mediumBlockVerifyListConsistencyData.nextAllocableBlockHead = block;
        this->mediumBlockVerifyListConsistencyData.expectFull = expectFull;
        this->mediumBlockVerifyListConsistencyData.expectDispose = expectDispose;
        this->mediumBlockVerifyListConsistencyData.hasSetupVerifyListConsistencyData = true;
    }

    GenericRecyclerVerifyListConsistencyData<SmallAllocationBlockAttributes>  smallBlockVerifyListConsistencyData;
    GenericRecyclerVerifyListConsistencyData<MediumAllocationBlockAttributes> mediumBlockVerifyListConsistencyData;
};

#endif

// NOTE: HeapBucket can't have vtable, because we allocate them inline with recycler with custom initializer
class HeapBucket
{
public:
    HeapBucket();

    uint GetBucketIndex() const;
    uint GetMediumBucketIndex() const;
protected:
    template <typename TBlockType>
    static void EnumerateObjects(TBlockType * heapBlockList, ObjectInfoBits infoBits, void (*CallBackFunction)(void * address, size_t size));

    HeapInfo * heapInfo;
    uint sizeCat;

#ifdef RECYCLER_SLOW_CHECK_ENABLED
    size_t heapBlockCount;
    size_t newHeapBlockCount;       // count of heap bock that is in the heap info and not in the heap bucket yet
    size_t emptyHeapBlockCount;
#endif

#ifdef RECYCLER_PAGE_HEAP
    bool isPageHeapEnabled;
    __inline bool IsPageHeapEnabled() const { return isPageHeapEnabled; }
#endif
#if DBG || defined(RECYCLER_SLOW_CHECK_ENABLED)
    Recycler * GetRecycler() const;
#endif

    template <typename TBlockType>
    friend class SmallHeapBlockAllocator;

    template <typename TBlockAttributes>
    friend class SmallHeapBlockT;

    template <typename TBlockAttributes>
    friend class SmallFinalizableHeapBlockT;

    friend class LargeHeapBlock;
#ifdef RECYCLER_WRITE_BARRIER
    template <typename TBlockAttributes>
    friend class SmallFinalizableWithBarrierHeapBlockT;

    template <typename TBlockAttributes>
    friend class SmallNormalWithBarrierHeapBlockT;
#endif
};

template <typename TBlockType>
class HeapBucketT : public HeapBucket
{
    typedef typename SmallHeapBlockAllocator<TBlockType> TBlockAllocatorType;

public:
    HeapBucketT();
    ~HeapBucketT();

    bool IntegrateBlock(char * blockAddress, PageSegment * segment, Recycler * recycler);

    template <ObjectInfoBits attributes, bool nothrow>
    __inline char * RealAlloc(Recycler * recycler, size_t sizeCat);

#ifdef RECYCLER_PAGE_HEAP
    char * PageHeapAlloc(Recycler * recycler, size_t sizeCat, ObjectInfoBits attributes, PageHeapMode mode, bool nothrow);
    void PageHeapCheckSweepLists(RecyclerSweep& recyclerSweep);
#endif

    void ExplicitFree(void* object, size_t sizeCat);

    char * SnailAlloc(Recycler * recycler, TBlockAllocatorType * allocator, size_t sizeCat, ObjectInfoBits attributes, bool nothrow);

    void ResetMarks(ResetMarkFlags flags);
    void ScanNewImplicitRoots(Recycler * recycler);

#ifdef DUMP_FRAGMENTATION_STATS
    void AggregateBucketStats(HeapBucketStats& stats);
#endif
#if defined(PARTIAL_GC_ENABLED) || defined(CONCURRENT_GC_ENABLED)
    uint Rescan(Recycler * recycler, RescanFlags flags);
#endif
#ifdef CONCURRENT_GC_ENABLED
    void MergeNewHeapBlock(TBlockType * heapBlock);
    void PrepareSweep();
    void SetupBackgroundSweep(RecyclerSweep& recyclerSweep);
#endif
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    friend class ScriptMemoryDumper;
#endif

    TBlockAllocatorType * GetAllocator() { return &allocatorHead;}

protected:
    static bool const IsLeafBucket = TBlockType::RequiredAttributes == LeafBit;
    static bool const IsFinalizableBucket = TBlockType::RequiredAttributes == FinalizeBit;
    static bool const IsNormalBucket = TBlockType::RequiredAttributes == NoBit;
#ifdef RECYCLER_WRITE_BARRIER
    static bool const IsWriteBarrierBucket = TBlockType::RequiredAttributes == WithBarrierBit;
    static bool const IsFinalizableWriteBarrierBucket = TBlockType::RequiredAttributes == FinalizableWithBarrierBit;
#endif

    void Initialize(HeapInfo * heapInfo, uint sizeCat);
    void AppendAllocableHeapBlockList(TBlockType * list);
    void DeleteHeapBlockList(TBlockType * list);
    static void DeleteEmptyHeapBlockList(TBlockType * list);
    static void DeleteHeapBlockList(TBlockType * list, Recycler * recycler);

    // Small allocators
    void UpdateAllocators();
    void ClearAllocators();
    void AddAllocator(TBlockAllocatorType * allocator);
    void RemoveAllocator(TBlockAllocatorType * allocator);
    static void ClearAllocator(TBlockAllocatorType * allocator);
    template <class Fn> void ForEachAllocator(Fn fn);

    // Allocations
    char * TryAllocFromNewHeapBlock(Recycler * recycler, TBlockAllocatorType * allocator, size_t sizeCat, ObjectInfoBits attributes);
    char * TryAlloc(Recycler * recycler, TBlockAllocatorType * allocator, size_t sizeCat, ObjectInfoBits attributes);
    template<bool pageheap>
    TBlockType * CreateHeapBlock(Recycler * recycler);
    TBlockType * GetUnusedHeapBlock();

    void FreeHeapBlock(TBlockType * heapBlock);

    // GC
    template<bool pageheap>
    void SweepBucket(RecyclerSweep& recyclerSweep);
    template <bool pageheap, typename Fn>
    void SweepBucket(RecyclerSweep& recyclerSweep, Fn sweepFn);

    void StopAllocationBeforeSweep();
    void StartAllocationAfterSweep();
#if DBG
    bool IsAllocationStopped() const;
#endif
    template<bool pageheap>
    void SweepHeapBlockList(RecyclerSweep& recyclerSweep, TBlockType * heapBlockList, bool allocable);
#if defined(PARTIAL_GC_ENABLED)
    bool DoQueuePendingSweep(Recycler * recycler);
    bool DoPartialReuseSweep(Recycler * recycler);
#endif

    // Partial/Concurrent GC
    void EnumerateObjects(ObjectInfoBits infoBits, void (*CallBackFunction)(void * address, size_t size));

#if DBG
    bool AllocatorsAreEmpty() const;
    bool HasPendingDisposeHeapBlocks() const;

    static void VerifyBlockConsistencyInList(TBlockType * heapBlock, RecyclerVerifyListConsistencyData& recyclerSweep);
    static void VerifyBlockConsistencyInList(TBlockType * heapBlock, RecyclerVerifyListConsistencyData const& recyclerSweep, SweepState state);
#endif
#if DBG || defined(RECYCLER_SLOW_CHECK_ENABLED)
    // TODO-REFACTOR: This really should be virtual
    size_t GetNonEmptyHeapBlockCount(bool checkCount) const;
    size_t GetEmptyHeapBlockCount() const;
#endif
#ifdef RECYCLER_SLOW_CHECK_ENABLED
    size_t Check(bool checkCount = true);
    void VerifyHeapBlockCount(bool background);
#endif
#ifdef RECYCLER_MEMORY_VERIFY
    void Verify();
#endif
#ifdef RECYCLER_VERIFY_MARK
    void VerifyMark();
#endif
    TBlockAllocatorType allocatorHead;
    TBlockType * nextAllocableBlockHead;
    TBlockType * emptyBlockList;     // list of blocks that is empty and has it's page freed

    TBlockType * fullBlockList;      // list of blocks that are fully allocated
    TBlockType * heapBlockList;      // list of blocks that has free objects

    FreeObject* explicitFreeList; // List of objects that have been explicitly freed
    TBlockAllocatorType * lastExplicitFreeListAllocator;
#ifdef RECYCLER_PAGE_HEAP
    SmallHeapBlock* explicitFreeLockBlockList; // List of heap blocks which have been locked upon explicit free
#endif

#if DBG
    bool isAllocationStopped;                 // whether the bucket is the middle of sweeping, not including partial sweeping
#endif

    template <class TBlockAttributes>
    friend class HeapBucketGroup;

    friend class HeapInfo;
    friend typename TBlockType;

    template <class TBucketAttributes>
    friend class SmallHeapBlockT;
    friend class RecyclerSweep;
};

template <typename TBlockType>
void
HeapBucket::EnumerateObjects(TBlockType * heapBlockList, ObjectInfoBits infoBits, void (*CallBackFunction)(void * address, size_t size))
{
    HeapBlockList::ForEach(heapBlockList, [=](TBlockType * heapBlock)
    {
        heapBlock->EnumerateObjects(infoBits, CallBackFunction);
    });
}


template <typename TBlockType>
template <bool pageheap, typename Fn>
void
HeapBucketT<TBlockType>::SweepBucket(RecyclerSweep& recyclerSweep, Fn sweepFn)
{
    this->SweepBucket<pageheap>(recyclerSweep);

    // Continue to sweep other list from derived class
    sweepFn(recyclerSweep);

#if defined(PARTIAL_GC_ENABLED)
    if (!this->DoPartialReuseSweep(recyclerSweep.GetRecycler()))
#endif
    {
        // We should only queue up pending sweep if we are doing partial collect
        Assert(recyclerSweep.GetPendingSweepBlockList(this) == nullptr);

        // Every thing is swept immediately in non partial collect, so we can allocate
        // from the heap block list now
        StartAllocationAfterSweep();
    }

    RECYCLER_SLOW_CHECK(this->VerifyHeapBlockCount(recyclerSweep.IsBackground()));
}
}

