//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Memory
{
class HeapInfo;
class RecyclerSweep;

// CONSIDER: Templatizing this so that we can have separate leaf large allocations
// and finalizable allocations
// CONSIDER: Templatizing this so that we don't have free list support if we don't need it
class LargeHeapBucket: public HeapBucket
{
public:
    LargeHeapBucket():
        supportFreeList(false),
        freeList(nullptr),
        explicitFreeList(nullptr),
        fullLargeBlockList(nullptr),
        largeBlockList(nullptr),
#ifdef RECYCLER_PAGE_HEAP
        largePageHeapBlockList(nullptr),
#endif
        pendingDisposeLargeBlockList(nullptr)
#ifdef CONCURRENT_GC_ENABLED
        , pendingSweepLargeBlockList(nullptr)
#endif
#ifdef PARTIAL_GC_ENABLED
        , partialSweptLargeBlockList(nullptr)
#endif
    {
    }

    ~LargeHeapBucket();

    void Initialize(HeapInfo * heapInfo, uint sizeCat, bool supportFreeList = false);

    LargeHeapBlock* AddLargeHeapBlock(size_t size, bool nothrow);

    template <ObjectInfoBits attributes, bool nothrow>
    char* Alloc(Recycler * recycler, size_t sizeCat);
#ifdef RECYCLER_PAGE_HEAP
    char *PageHeapAlloc(Recycler * recycler, size_t sizeCat, ObjectInfoBits attributes, PageHeapMode mode, bool nothrow);
#endif
    void ExplicitFree(void * object, size_t sizeCat);

    void ResetMarks(ResetMarkFlags flags);
    void ScanInitialImplicitRoots(Recycler * recycler);
    void ScanNewImplicitRoots(Recycler * recycler);

    template<bool pageheap>
    void Sweep(RecyclerSweep& recyclerSweep);
    void ReinsertLargeHeapBlock(LargeHeapBlock * heapBlock);

    void RegisterFreeList(LargeHeapBlockFreeList* freeList);
    void UnregisterFreeList(LargeHeapBlockFreeList* freeList);

    void FinalizeAllObjects();
    void Finalize();
    void DisposeObjects();
    void TransferDisposedObjects();

    void EnumerateObjects(ObjectInfoBits infoBits, void (*CallBackFunction)(void * address, size_t size));

    void Verify();
    void VerifyMark();
    void VerifyLargeHeapBlockCount();

#if defined(PARTIAL_GC_ENABLED) || defined(CONCURRENT_GC_ENABLED)
    size_t Rescan(RescanFlags flags);
    void SweepPendingObjects(RecyclerSweep& recyclerSweep);
#ifdef PARTIAL_GC_ENABLED
    void FinishPartialCollect(RecyclerSweep * recyclerSweep);
#endif

#ifdef CONCURRENT_GC_ENABLED
    void ConcurrentTransferSweptObjects(RecyclerSweep& recyclerSweep);
#ifdef PARTIAL_GC_ENABLED
    void ConcurrentPartialTransferSweptObjects(RecyclerSweep& recyclerSweep);
#endif
#endif
#endif

#if DBG || defined(RECYCLER_SLOW_CHECK_ENABLED)
    size_t GetLargeHeapBlockCount(bool checkCount = false) const;
#ifdef RECYCLER_SLOW_CHECK_ENABLED
    size_t Check();
    template <typename TBlockType>
    size_t Check(bool expectFull, bool expectPending, TBlockType * list, TBlockType * tail = nullptr);
#endif
#endif

private:
    char * SnailAlloc(Recycler * recycler, size_t sizeCat, ObjectInfoBits attributes, bool nothrow);
    char * TryAlloc(Recycler * recycler, size_t sizeCat, ObjectInfoBits attributes);
    char * TryAllocFromNewHeapBlock(Recycler * recycler, size_t sizeCat, ObjectInfoBits attributes, bool nothrow);
    char * TryAllocFromFreeList(Recycler * recycler, size_t sizeCat, ObjectInfoBits attributes);
    char * TryAllocFromExplicitFreeList(Recycler * recycler, size_t sizeCat, ObjectInfoBits attributes);

    template <class Fn> void ForEachLargeHeapBlock(Fn fn);
    template <class Fn> void ForEachEditingLargeHeapBlock(Fn fn);
    void Finalize(Recycler* recycler, LargeHeapBlock* heapBlock);
    template<bool pageheap>
    void SweepLargeHeapBlockList(RecyclerSweep& recyclerSweep, LargeHeapBlock * heapBlockList);

    void ConstructFreelist(LargeHeapBlock * heapBlock);

#if defined(PARTIAL_GC_ENABLED) || defined(CONCURRENT_GC_ENABLED)
    size_t Rescan(LargeHeapBlock * list, Recycler * recycler, bool isPartialSwept, RescanFlags flags);
#endif

    LargeHeapBlock * fullLargeBlockList;
    LargeHeapBlock * largeBlockList;
#ifdef RECYCLER_PAGE_HEAP
    LargeHeapBlock * largePageHeapBlockList;
#endif
    LargeHeapBlock * pendingDisposeLargeBlockList;
#ifdef CONCURRENT_GC_ENABLED
    LargeHeapBlock * pendingSweepLargeBlockList;
#ifdef PARTIAL_GC_ENABLED
    LargeHeapBlock * partialSweptLargeBlockList;
#endif
#endif
    bool supportFreeList;
    LargeHeapBlockFreeList* freeList;
    FreeObject * explicitFreeList;


    friend class HeapInfo;
    friend class Recycler;
    friend class ScriptMemoryDumper;
};
}

