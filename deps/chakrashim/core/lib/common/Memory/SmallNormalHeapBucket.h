//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Memory
{
template <typename TBlockType>
class SmallNormalHeapBucketBase : public HeapBucketT<TBlockType>
{
    typedef HeapBucketT<TBlockType> BaseT;
public:
    typedef typename TBlockType::HeapBlockAttributes TBlockAttributes;

    SmallNormalHeapBucketBase();

    CompileAssert(!IsLeafBucket);
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    friend class ScriptMemoryDumper;
#endif

#ifdef DUMP_FRAGMENTATION_STATS
    void AggregateBucketStats(HeapBucketStats& stats);
#endif
protected:
    template <class TBlockAttributes>
    friend class HeapBucketGroup;
    friend class HeapInfo;
    friend class HeapBlockMap32;

    void ScanInitialImplicitRoots(Recycler * recycler);
    void ScanNewImplicitRoots(Recycler * recycler);

#if defined(PARTIAL_GC_ENABLED) || defined(CONCURRENT_GC_ENABLED)
    static bool RescanObjectsOnPage(TBlockType * block, char * address, char * blockStartAddress, BVStatic<TBlockAttributes::BitVectorCount>* markBits, const uint localObjectSize, uint bucketIndex, __out_opt bool* anyObjectRescanned, Recycler* recycler);

    void SweepPendingObjects(RecyclerSweep& recyclerSweep);
    template <SweepMode mode>
    static TBlockType * SweepPendingObjects(Recycler * recycler, TBlockType * list);
#endif
#ifdef PARTIAL_GC_ENABLED
    ~SmallNormalHeapBucketBase();

    template<bool pageheap>
    void Sweep(RecyclerSweep& recyclerSweep);
    template <class Fn>
    static void SweepPartialReusePages(RecyclerSweep& recyclerSweep, TBlockType * heapBlockList,
        TBlockType *& reuseBlocklist, TBlockType *&unusedBlockList, Fn callBack);
    void SweepPartialReusePages(RecyclerSweep& recyclerSweep);
    void FinishPartialCollect(RecyclerSweep * recyclerSweep);

    void EnumerateObjects(ObjectInfoBits infoBits, void (*CallBackFunction)(void * address, size_t size));

#if DBG
    void ResetMarks(ResetMarkFlags flags);
    static void SweepVerifyPartialBlocks(Recycler * recycler, TBlockType * heapBlockList);
#endif
#if DBG || defined(RECYCLER_SLOW_CHECK_ENABLED)
    size_t GetNonEmptyHeapBlockCount(bool checkCount) const;
#endif
#ifdef RECYCLER_SLOW_CHECK_ENABLED
    size_t Check(bool checkCount = true);
    friend class HeapBucketT<TBlockType>;
#endif
#ifdef RECYCLER_MEMORY_VERIFY
    void Verify();
#endif
#ifdef RECYCLER_VERIFY_MARK
    void VerifyMark();
#endif
protected:
    TBlockType * partialHeapBlockList;      // list of blocks that is partially collected
                                            // This list exists to keep track of heap blocks that
                                            // are not full but don't have a large amount of free space
                                            // where allocating from it causing a write watch to be triggered
                                            // is not worth the effort
#ifdef CONCURRENT_GC_ENABLED
    TBlockType * partialSweptHeapBlockList; // list of blocks that is partially swept
#endif
#endif
};

template <typename TBlockAttributes>
class SmallNormalHeapBucketT : public SmallNormalHeapBucketBase<SmallNormalHeapBlockT<TBlockAttributes>> {};

typedef SmallNormalHeapBucketT<SmallAllocationBlockAttributes> SmallNormalHeapBucket;
typedef SmallNormalHeapBucketT<MediumAllocationBlockAttributes> MediumNormalHeapBucket;

#ifdef RECYCLER_WRITE_BARRIER
template <typename TBlockAttributes>
class SmallNormalWithBarrierHeapBucketT : public SmallNormalHeapBucketBase<SmallNormalWithBarrierHeapBlockT<TBlockAttributes>>
{
public:
    void Initialize(HeapInfo * heapInfo, uint sizeCat)
    {
        CompileAssert(SmallNormalWithBarrierHeapBucketT::IsLeafBucket == false);
        __super::Initialize(heapInfo, sizeCat);
    }
};

typedef SmallNormalWithBarrierHeapBucketT<SmallAllocationBlockAttributes> SmallNormalWithBarrierHeapBucket;
typedef SmallNormalWithBarrierHeapBucketT<MediumAllocationBlockAttributes> MediumNormalWithBarrierHeapBucket;

#endif

extern template class SmallNormalHeapBucketBase<SmallNormalHeapBlock>;
extern template class SmallNormalHeapBucketBase<MediumNormalHeapBlock>;

#ifdef RECYCLER_WRITE_BARRIER
extern template class SmallNormalHeapBucketBase<SmallNormalWithBarrierHeapBlock>;
extern template class SmallNormalHeapBucketBase<MediumNormalWithBarrierHeapBlock>;
#endif
}
