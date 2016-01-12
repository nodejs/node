//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonMemoryPch.h"

template <class TBlockType>
SmallFinalizableHeapBucketBaseT<TBlockType>::SmallFinalizableHeapBucketBaseT() :
    pendingDisposeList(nullptr)
{
#if DBG || defined(RECYCLER_SLOW_CHECK_ENABLED)
    tempPendingDisposeList = nullptr;
#endif
}

template <class TBlockType>
SmallFinalizableHeapBucketBaseT<TBlockType>::~SmallFinalizableHeapBucketBaseT()
{
    Assert(this->AllocatorsAreEmpty());
    DeleteHeapBlockList(this->pendingDisposeList);
    Assert(this->tempPendingDisposeList == nullptr);
}

template <class TBlockType>
void
SmallFinalizableHeapBucketBaseT<TBlockType>::FinalizeAllObjects()
{
    // Finalize all objects on shutdown.

    // Clear allocators to update the information on the heapblock
    // Walk through the allocated object and call finalize and dispose on them
    this->ClearAllocators();
    FinalizeHeapBlockList(this->pendingDisposeList);
#ifdef PARTIAL_GC_ENABLED
    FinalizeHeapBlockList(this->partialHeapBlockList);
#endif
    FinalizeHeapBlockList(this->heapBlockList);
    FinalizeHeapBlockList(this->fullBlockList);

#if defined(PARTIAL_GC_ENABLED) && defined(CONCURRENT_GC_ENABLED)
    FinalizeHeapBlockList(this->partialSweptHeapBlockList);
#endif
}

template <class TBlockType>
void
SmallFinalizableHeapBucketBaseT<TBlockType>::FinalizeHeapBlockList(TBlockType * list)
{
    HeapBlockList::ForEach(list, [](TBlockType * heapBlock)
    {
        heapBlock->FinalizeAllObjects();
    });
}

#if DBG || defined(RECYCLER_SLOW_CHECK_ENABLED)
template <class TBlockType>
size_t
SmallFinalizableHeapBucketBaseT<TBlockType>::GetNonEmptyHeapBlockCount(bool checkCount) const
{
    size_t currentHeapBlockCount =  __super::GetNonEmptyHeapBlockCount(false)
        + HeapBlockList::Count(pendingDisposeList)
        + HeapBlockList::Count(tempPendingDisposeList);
    RECYCLER_SLOW_CHECK(Assert(!checkCount || heapBlockCount == currentHeapBlockCount));
    return currentHeapBlockCount;
}
#endif

template <class TBlockType>
void
SmallFinalizableHeapBucketBaseT<TBlockType>::ResetMarks(ResetMarkFlags flags)
{
    __super::ResetMarks(flags);

    if ((flags & ResetMarkFlags_ScanImplicitRoot) != 0)
    {
        HeapBlockList::ForEach(this->pendingDisposeList, [flags](TBlockType * heapBlock)
        {
            heapBlock->MarkImplicitRoots();
        });
    }
}

#ifdef DUMP_FRAGMENTATION_STATS
template <class TBlockType>
void
SmallFinalizableHeapBucketBaseT<TBlockType>::AggregateBucketStats(HeapBucketStats& stats)
{
    __super::AggregateBucketStats(stats);

    HeapBlockList::ForEach(pendingDisposeList, [&stats](TBlockType* heapBlock) {
        heapBlock->AggregateBlockStats(stats);
    });
}
#endif

template void SmallFinalizableHeapBucketBaseT<SmallFinalizableHeapBlock>::Sweep<true>(RecyclerSweep& recyclerSweep);
template void SmallFinalizableHeapBucketBaseT<SmallFinalizableHeapBlock>::Sweep<false>(RecyclerSweep& recyclerSweep);
template void SmallFinalizableHeapBucketBaseT<MediumFinalizableHeapBlock>::Sweep<true>(RecyclerSweep& recyclerSweep);
template void SmallFinalizableHeapBucketBaseT<MediumFinalizableHeapBlock>::Sweep<false>(RecyclerSweep& recyclerSweep);

#ifdef RECYCLER_WRITE_BARRIER
template void SmallFinalizableHeapBucketBaseT<SmallFinalizableWithBarrierHeapBlock>::Sweep<true>(RecyclerSweep& recyclerSweep);
template void SmallFinalizableHeapBucketBaseT<SmallFinalizableWithBarrierHeapBlock>::Sweep<false>(RecyclerSweep& recyclerSweep);
template void SmallFinalizableHeapBucketBaseT<MediumFinalizableWithBarrierHeapBlock>::Sweep<true>(RecyclerSweep& recyclerSweep);
template void SmallFinalizableHeapBucketBaseT<MediumFinalizableWithBarrierHeapBlock>::Sweep<false>(RecyclerSweep& recyclerSweep);
#endif


template<class TBlockType>
template<bool pageheap>
void
SmallFinalizableHeapBucketBaseT<TBlockType>::Sweep(RecyclerSweep& recyclerSweep)
{
    Assert(!recyclerSweep.IsBackground());

#if DBG || defined(RECYCLER_SLOW_CHECK_ENABLED)
    Assert(this->tempPendingDisposeList == nullptr);
    this->tempPendingDisposeList = pendingDisposeList;
#endif

    TBlockType * currentDisposeList = pendingDisposeList;
    this->pendingDisposeList = nullptr;

    BaseT::SweepBucket<pageheap>(recyclerSweep, [=](RecyclerSweep& recyclerSweep)
    {
#if DBG
        if (TBlockType::HeapBlockAttributes::IsSmallBlock)
        {
            recyclerSweep.SetupVerifyListConsistencyDataForSmallBlock(nullptr, false, true);
        }
        else if (TBlockType::HeapBlockAttributes::IsMediumBlock)
        {
            recyclerSweep.SetupVerifyListConsistencyDataForMediumBlock(nullptr, false, true);
        }
        else
        {
            Assert(false);
        }
#endif

        HeapBucketT<TBlockType>::SweepHeapBlockList<pageheap>(recyclerSweep, currentDisposeList, false);

#if DBG || defined(RECYCLER_SLOW_CHECK_ENABLED)
        Assert(this->tempPendingDisposeList == currentDisposeList);
        this->tempPendingDisposeList = nullptr;
#endif
        RECYCLER_SLOW_CHECK(this->VerifyHeapBlockCount(recyclerSweep.IsBackground()));
    });

}

template <class TBlockType>
void
SmallFinalizableHeapBucketBaseT<TBlockType>::DisposeObjects()
{
    HeapBlockList::ForEach(this->pendingDisposeList, [](TBlockType * heapBlock)
    {
        Assert(heapBlock->HasAnyDisposeObjects());
        heapBlock->DisposeObjects();
    });
}

template <class TBlockType>
void
SmallFinalizableHeapBucketBaseT<TBlockType>::TransferDisposedObjects()
{
    Assert(!this->IsAllocationStopped());
    TBlockType * currentPendingDisposeList = this->pendingDisposeList;
    if (currentPendingDisposeList != nullptr)
    {
        this->pendingDisposeList = nullptr;

        HeapBlockList::ForEach(currentPendingDisposeList, [=](TBlockType * heapBlock)
        {
            heapBlock->TransferDisposedObjects();

            // in pageheap, we actually always have free object
            Assert(heapBlock->HasFreeObject<false>());
        });

#ifdef RECYCLER_PAGE_HEAP
        if (this->IsPageHeapEnabled())
        {
            // In pageheap mode, we can't reuse the empty blocks since they are not close enough to the guard page
            // treat it as full and ready to be released

            HeapBlockList::Tail(currentPendingDisposeList)->SetNextBlock(this->fullBlockList);
            this->fullBlockList = currentPendingDisposeList;
        }
        else
#endif
        {
            // For partial collect, dispose will modify the object, and we
            // also touch the page by chaining the object through the free list
            // might as well reuse the block for partial collect
            this->AppendAllocableHeapBlockList(currentPendingDisposeList);
        }
    }

    RECYCLER_SLOW_CHECK(this->VerifyHeapBlockCount(false));
}

template <class TBlockType>
void
SmallFinalizableHeapBucketBaseT<TBlockType>::EnumerateObjects(ObjectInfoBits infoBits, void(*CallBackFunction)(void * address, size_t size))
{
    __super::EnumerateObjects(infoBits, CallBackFunction);
    HeapBucket::EnumerateObjects(this->pendingDisposeList, infoBits, CallBackFunction);
}

#ifdef RECYCLER_SLOW_CHECK_ENABLED
template <class TBlockType>
size_t
SmallFinalizableHeapBucketBaseT<TBlockType>::Check()
{
    size_t smallHeapBlockCount = __super::Check(false) + HeapInfo::Check(false, true, this->pendingDisposeList);
    Assert(this->heapBlockCount == smallHeapBlockCount);
    return smallHeapBlockCount;
}
#endif

#ifdef RECYCLER_MEMORY_VERIFY
template <class TBlockType>
void
SmallFinalizableHeapBucketBaseT<TBlockType>::Verify()
{
    BaseT::Verify();

#if DBG
    RecyclerVerifyListConsistencyData recyclerVerifyListConsistencyData;
    if (TBlockType::HeapBlockAttributes::IsSmallBlock)
    {
        recyclerVerifyListConsistencyData.SetupVerifyListConsistencyDataForSmallBlock(nullptr, false, true);
    }
    else if (TBlockType::HeapBlockAttributes::IsMediumBlock)
    {
        recyclerVerifyListConsistencyData.SetupVerifyListConsistencyDataForMediumBlock(nullptr, false, true);
    }
    else
    {
        Assert(false);
    }

    HeapBlockList::ForEach(this->pendingDisposeList, [&recyclerVerifyListConsistencyData](TBlockType * heapBlock)
    {
        DebugOnly(VerifyBlockConsistencyInList(heapBlock, recyclerVerifyListConsistencyData));
        heapBlock->Verify(true);
    });
#endif

}
#endif

#ifdef RECYCLER_VERIFY_MARK
template <class TBlockType>
void
SmallFinalizableHeapBucketBaseT<TBlockType>::VerifyMark()
{
    __super::VerifyMark();
    HeapBlockList::ForEach(this->pendingDisposeList, [](TBlockType * heapBlock)
    {
        Assert(heapBlock->HasAnyDisposeObjects());
        heapBlock->VerifyMark();
    });
}
#endif

template class SmallFinalizableHeapBucketBaseT<SmallFinalizableHeapBlock>;
#ifdef RECYCLER_WRITE_BARRIER
template class SmallFinalizableHeapBucketBaseT<SmallFinalizableWithBarrierHeapBlock>;
#endif

template class SmallFinalizableHeapBucketBaseT<MediumFinalizableHeapBlock>;
#ifdef RECYCLER_WRITE_BARRIER
template class SmallFinalizableHeapBucketBaseT<MediumFinalizableWithBarrierHeapBlock>;
#endif

