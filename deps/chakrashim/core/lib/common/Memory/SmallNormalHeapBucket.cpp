//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonMemoryPch.h"


template <typename TBlockType>
SmallNormalHeapBucketBase<TBlockType>::SmallNormalHeapBucketBase() :
    partialHeapBlockList(nullptr),
    partialSweptHeapBlockList(nullptr)
{
}

#ifdef DUMP_FRAGMENTATION_STATS
template <typename TBlockType>
void
SmallNormalHeapBucketBase<TBlockType>::AggregateBucketStats(HeapBucketStats& stats)
{
    __super::AggregateBucketStats(stats);

    HeapBlockList::ForEach(partialHeapBlockList, [&stats](SmallHeapBlock* heapBlock) {
        heapBlock->AggregateBlockStats(stats);
    });
    HeapBlockList::ForEach(partialSweptHeapBlockList, [&stats](SmallHeapBlock* heapBlock) {
        heapBlock->AggregateBlockStats(stats);
    });
}
#endif

template <typename TBlockType>
void
SmallNormalHeapBucketBase<TBlockType>::ScanInitialImplicitRoots(Recycler * recycler)
{
    HeapBlockList::ForEach(fullBlockList, [recycler](TBlockType * heapBlock)
    {
        heapBlock->ScanInitialImplicitRoots(recycler);
    });

    HeapBlockList::ForEach(heapBlockList, [recycler](TBlockType * heapBlock)
    {
        heapBlock->ScanInitialImplicitRoots(recycler);
    });

#ifdef PARTIAL_GC_ENABLED
    Assert(recycler->inPartialCollectMode || partialHeapBlockList == nullptr);
    if (recycler->inPartialCollectMode)
    {
        HeapBlockList::ForEach(partialHeapBlockList, [recycler](TBlockType * heapBlock)
        {
            heapBlock->ScanInitialImplicitRoots(recycler);
        });
#ifdef CONCURRENT_GC_ENABLED
        HeapBlockList::ForEach(partialSweptHeapBlockList, [recycler](TBlockType * heapBlock)
        {
            heapBlock->ScanInitialImplicitRoots(recycler);
        });
#endif
    }
#endif
}

template <typename TBlockType>
void
SmallNormalHeapBucketBase<TBlockType>::ScanNewImplicitRoots(Recycler * recycler)
{
    __super::ScanNewImplicitRoots(recycler);

#ifdef PARTIAL_GC_ENABLED
    Assert(recycler->inPartialCollectMode || partialHeapBlockList == nullptr);
    // Don't need to scan the partial heap block list for new implicit root as we don't allocate from them
#endif
}


#if defined(PARTIAL_GC_ENABLED) || defined(CONCURRENT_GC_ENABLED)
template <typename TBlockType>
bool
SmallNormalHeapBucketBase<TBlockType>::RescanObjectsOnPage(TBlockType * block, char* pageAddress, char * blockStartAddress, BVStatic<TBlockAttributes::BitVectorCount> * heapBlockMarkBits, const uint localObjectSize, uint bucketIndex, __out_opt bool* anyObjectRescanned, Recycler * recycler)
{
    RECYCLER_STATS_ADD(recycler, markData.rescanPageCount, TBlockAttributes::PageCount);

    // By the time we get here, we should have ensured that there's a mark on any page somewhere.
    // REVIEW: Worth check on just the page's mark bits?
    Assert(!heapBlockMarkBits->IsAllClear());

    if (anyObjectRescanned != nullptr)
    {
        *anyObjectRescanned = false;
    }

    Assert((char*)pageAddress - blockStartAddress < TBlockAttributes::PageCount * AutoSystemInfo::PageSize);
    const uint pageByteOffset = static_cast<uint>((char*)pageAddress - blockStartAddress);
    uint firstObjectOnPageIndex = pageByteOffset / localObjectSize;

    // This is not necessarily the address on the first object that starts on the page
    // If the last object on the previous page spans two pages, this is the address of that object
    // We do it this way so that we can figure out if we need to rescan the first few bytes of the page
    // if the actual first object on this page is not located at the start of the page
    char* const startObjectAddress = blockStartAddress + (firstObjectOnPageIndex * localObjectSize);
    const uint startBitIndex = TBlockType::GetAddressBitIndex(startObjectAddress);
    const uint pageStartBitIndex = pageByteOffset >> HeapConstants::ObjectAllocationShift;

    Assert(pageByteOffset / AutoSystemInfo::PageSize < USHRT_MAX);
    const ushort pageNumber = static_cast<const ushort>(pageByteOffset / AutoSystemInfo::PageSize);
    const TBlockType::BlockInfo& blockInfoForPage = HeapInfo::GetBlockInfo<TBlockAttributes>(localObjectSize)[pageNumber];

    bool lastObjectOnPreviousPageMarked = false;
    // Calculate the mark count here since we no longer keep track during marking
    uint rescanMarkCount = TBlockType::CalculateMarkCountForPage(heapBlockMarkBits, bucketIndex, pageStartBitIndex);
    const uint pageObjectCount = blockInfoForPage.pageObjectCount;
    const uint localObjectCount = (TBlockAttributes::PageCount * AutoSystemInfo::PageSize) / localObjectSize;

    // If all objects are marked, rescan whole block at once
    if (TBlockType::CanRescanFullBlock() && rescanMarkCount == pageObjectCount)
    {
        // REVIEW: Can we optimize this more?
        if (!recycler->AddMark(pageAddress, AutoSystemInfo::PageSize))
        {
            // Failed to add to the mark stack due to OOM.
            return false;
        }

        RECYCLER_STATS_ADD(recycler, markData.rescanObjectCount, pageObjectCount);
        RECYCLER_STATS_ADD(recycler, markData.rescanObjectByteCount, localObjectSize * pageObjectCount);
        if (anyObjectRescanned != nullptr)
        {
            *anyObjectRescanned = true;
        }

        return true;
    }

    if (startObjectAddress != pageAddress)
    {
        // If the last object on the previous page that spans into the current page is marked,
        // we need to count that in the markCount for rescan
        Assert(startObjectAddress >= blockStartAddress && startObjectAddress < pageAddress);
        lastObjectOnPreviousPageMarked = (heapBlockMarkBits->Test(startBitIndex) == TRUE);
        if (lastObjectOnPreviousPageMarked)
        {
            rescanMarkCount++;
        }
    }

    const uint objectBitDelta = SmallHeapBlockT<TBlockAttributes>::GetObjectBitDeltaForBucketIndex(bucketIndex);

    uint rescanCount = 0;
    uint objectIndex = firstObjectOnPageIndex;

    for (uint bitIndex = startBitIndex; rescanCount < rescanMarkCount; objectIndex++, bitIndex += objectBitDelta)
    {
        Assert(objectIndex < localObjectCount);
        Assert(!HeapInfo::GetInvalidBitVectorForBucket<TBlockAttributes>(bucketIndex)->Test(bitIndex));

        if (heapBlockMarkBits->Test(bitIndex))
        {
            char * objectAddress = blockStartAddress + objectIndex * localObjectSize;
            if (!TBlockType::RescanObject(block, objectAddress, localObjectSize, objectIndex, recycler))
            {
                // Failed to add to the mark stack due to OOM.
                return false;
            }

            rescanCount++;
        }
    }

    // Mark bits should not have changed during the Rescan
    if (startObjectAddress != pageAddress && lastObjectOnPreviousPageMarked)
    {
        Assert(rescanMarkCount == TBlockType::CalculateMarkCountForPage(heapBlockMarkBits, bucketIndex, pageStartBitIndex) + 1);
    }
    else
    {
        Assert(rescanMarkCount == TBlockType::CalculateMarkCountForPage(heapBlockMarkBits, bucketIndex, pageStartBitIndex));
    }

#if DBG
    // We stopped when we hit the rescanMarkCount.
    // Make sure no other objects were marked, otherwise our rescanMarkCount was wrong.
    for (uint i = objectIndex + 1; i < blockInfoForPage.lastObjectIndexOnPage; i++)
    {
        Assert(!heapBlockMarkBits->Test(i * objectBitDelta));
    }
#endif

    // Let the caller know if we rescanned anything on this page
    if (anyObjectRescanned != nullptr)
    {
        (*anyObjectRescanned) = (rescanCount > 0);
    }

    return true;
}

template <typename TBlockType>
void
SmallNormalHeapBucketBase<TBlockType>::SweepPendingObjects(RecyclerSweep& recyclerSweep)
{
    RECYCLER_SLOW_CHECK(VerifyHeapBlockCount(recyclerSweep.IsBackground()));

    CompileAssert(!IsLeafBucket);
    TBlockType *& pendingSweepList = recyclerSweep.GetPendingSweepBlockList(this);
    TBlockType * const list = pendingSweepList;
    Recycler * const recycler = recyclerSweep.GetRecycler();
    bool const partialSweep = recycler->inPartialCollectMode;
    if (list)
    {
        pendingSweepList = nullptr;
        if (partialSweep)
        {
            // We did a partial sweep.
            // Blocks in the pendingSweepList are the ones we decided not to reuse.

            HeapBlockList::ForEachEditing(list, [this, recycler](TBlockType * heapBlock)
            {
                // We are not going to reuse this block.
                // SweepMode_ConcurrentPartial will not actually collect anything, it will just update some state.
                // The sweepable objects will be collected in a future Sweep.

                // Note, page heap blocks are never swept concurrently
                heapBlock->SweepObjects<false, SweepMode_ConcurrentPartial>(recycler);

                // page heap mode should never reach here, so don't check pageheap enabled or not
                if (heapBlock->HasFreeObject<false>())
                {
                    // We have pre-existing free objects, so put this in the partialSweptHeapBlockList
                    heapBlock->SetNextBlock(this->partialSweptHeapBlockList);
                    this->partialSweptHeapBlockList = heapBlock;
                }
                else
                {
                    // No free objects, so put in the fullBlockList
                    heapBlock->SetNextBlock(this->fullBlockList);
                    this->fullBlockList = heapBlock;
                }
            });
        }
        else
        {
            // We decided not to do a partial sweep.
            // Blocks in the pendingSweepList need to have a regular sweep.

            TBlockType * tail = SweepPendingObjects<SweepMode_Concurrent>(recycler, list);
            tail->SetNextBlock(this->heapBlockList);
            this->heapBlockList = list;

            this->StartAllocationAfterSweep();
        }

        RECYCLER_SLOW_CHECK(VerifyHeapBlockCount(recyclerSweep.IsBackground()));
    }

    Assert(!this->IsAllocationStopped());
}

template <typename TBlockType>
template <SweepMode mode>
TBlockType *
SmallNormalHeapBucketBase<TBlockType>::SweepPendingObjects(Recycler * recycler, TBlockType * list)
{
    TBlockType * tail;
    HeapBlockList::ForEach(list, [recycler, &tail](TBlockType * heapBlock)
    {
        // Note, page heap blocks are never swept concurrently
        heapBlock->SweepObjects<false, mode>(recycler);
        tail = heapBlock;
    });
    return tail;
}
#endif

#ifdef PARTIAL_GC_ENABLED
template <typename TBlockType>
SmallNormalHeapBucketBase<TBlockType>::~SmallNormalHeapBucketBase()
{
    DeleteHeapBlockList(this->partialHeapBlockList);
#ifdef CONCURRENT_GC_ENABLED
    DeleteHeapBlockList(this->partialSweptHeapBlockList);
#endif
}

template <typename TBlockType>
template <class Fn>
void
SmallNormalHeapBucketBase<TBlockType>::SweepPartialReusePages(RecyclerSweep& recyclerSweep, TBlockType * heapBlockList,
    TBlockType *& reuseBlocklist, TBlockType *&unusedBlockList, Fn callback)
{
    HeapBlockList::ForEachEditing(heapBlockList,
        [&recyclerSweep, &reuseBlocklist, &unusedBlockList, callback](TBlockType * heapBlock)
    {
        uint expectFreeByteCount;
        if (heapBlock->DoPartialReusePage(recyclerSweep, expectFreeByteCount))
        {
            callback(heapBlock, true);

            // Reuse the page
            heapBlock->SetNextBlock(reuseBlocklist);
            reuseBlocklist = heapBlock;

            RECYCLER_STATS_ADD(recyclerSweep.GetRecycler(), smallNonLeafHeapBlockPartialReuseBytes[heapBlock->GetHeapBlockType()], expectFreeByteCount);
            RECYCLER_STATS_INC(recyclerSweep.GetRecycler(), smallNonLeafHeapBlockPartialReuseCount[heapBlock->GetHeapBlockType()]);
        }
        else
        {
            // Don't not reuse the page if it don't have much free memory.
            callback(heapBlock, false);

            heapBlock->SetNextBlock(unusedBlockList);
            unusedBlockList = heapBlock;

            recyclerSweep.AddUnusedFreeByteCount(expectFreeByteCount);
            RECYCLER_STATS_ADD(recyclerSweep.GetRecycler(), smallNonLeafHeapBlockPartialUnusedBytes[heapBlock->GetHeapBlockType()], expectFreeByteCount);
            RECYCLER_STATS_INC(recyclerSweep.GetRecycler(), smallNonLeafHeapBlockPartialUnusedCount[heapBlock->GetHeapBlockType()]);
        }
    });
}

template <typename TBlockType>
void
SmallNormalHeapBucketBase<TBlockType>::SweepPartialReusePages(RecyclerSweep& recyclerSweep)
{
    RECYCLER_SLOW_CHECK(this->VerifyHeapBlockCount(recyclerSweep.IsBackground()));
    Assert(this->GetRecycler()->inPartialCollectMode);

    TBlockType * currentHeapBlockList = this->heapBlockList;
    this->heapBlockList = nullptr;
    SmallNormalHeapBucketBase<TBlockType>::SweepPartialReusePages(recyclerSweep, currentHeapBlockList, this->heapBlockList,
        this->partialHeapBlockList,
        [](TBlockType * heapBlock, bool isReused) {});

#ifdef CONCURRENT_GC_ENABLED
    // only collect data for pending sweep list but don't sweep yet
    // until we have adjusted the heuristics, and SweepPartialReusePages will
    // sweep the page that we are going to reuse in thread.
    TBlockType *& pendingSweepList = recyclerSweep.GetPendingSweepBlockList(this);
    currentHeapBlockList = pendingSweepList;
    pendingSweepList = nullptr;
    Recycler * recycler = recyclerSweep.GetRecycler();
    SmallNormalHeapBucketBase<TBlockType>::SweepPartialReusePages(recyclerSweep, currentHeapBlockList, this->heapBlockList,
        pendingSweepList,
        [recycler](TBlockType * heapBlock, bool isReused)
        {
            if (isReused)
            {
                // Finalizable blocks are always swept in thread, so shouldn't be here
                Assert(!heapBlock->IsAnyFinalizableBlock());

                // Page heap blocks are never swept concurrently
                heapBlock->SweepObjects<false, SweepMode_InThread>(recycler);

                // This block has been counted as concurrently swept, and now we changed our mind
                // and sweep it in thread. Remove the count
                RECYCLER_STATS_DEC(recycler, heapBlockConcurrentSweptCount[heapBlock->GetHeapBlockType()]);
            }
        }
    );
#endif

    RECYCLER_SLOW_CHECK(this->VerifyHeapBlockCount(recyclerSweep.IsBackground()));

    this->StartAllocationAfterSweep();

    // PARTIALGC-TODO: revisit partial heap blocks to see if they can be put back into use
    // since the heuristics limit may be been changed.
}

template <typename TBlockType>
void
SmallNormalHeapBucketBase<TBlockType>::FinishPartialCollect(RecyclerSweep * recyclerSweep)
{
    RECYCLER_SLOW_CHECK(this->VerifyHeapBlockCount(recyclerSweep != nullptr && recyclerSweep->IsBackground()));

    Assert(this->GetRecycler()->inPartialCollectMode);
    Assert(recyclerSweep == nullptr || this->IsAllocationStopped());

#ifdef CONCURRENT_GC_ENABLED
    // Process the partial Swept block and move it to the partial heap block list
    TBlockType * partialSweptList = this->partialSweptHeapBlockList;
    if (partialSweptList)
    {
        this->partialSweptHeapBlockList = nullptr;
        TBlockType *  tail = nullptr;
        HeapBlockList::ForEach(partialSweptList, [this, &tail](TBlockType * heapBlock)
        {
            heapBlock->FinishPartialCollect();
            Assert(heapBlock->HasFreeObject());
            tail = heapBlock;
        });
        Assert(tail != nullptr);
        tail->SetNextBlock(this->partialHeapBlockList);
        this->partialHeapBlockList = partialSweptList;
    }
#endif

    TBlockType * currentPartialHeapBlockList = this->partialHeapBlockList;
    if (recyclerSweep == nullptr)
    {
        if (currentPartialHeapBlockList != nullptr)
        {
            this->partialHeapBlockList = nullptr;
            this->AppendAllocableHeapBlockList(currentPartialHeapBlockList);
        }
    }
    else
    {
        if (currentPartialHeapBlockList != nullptr)
        {
            this->partialHeapBlockList = nullptr;
            TBlockType * list = this->heapBlockList;
            if (list == nullptr)
            {
                this->heapBlockList = currentPartialHeapBlockList;
            }
            else
            {
                // CONCURRENT-TODO: Optimize this?
                TBlockType * tail = HeapBlockList::Tail(this->heapBlockList);
                tail->SetNextBlock(currentPartialHeapBlockList);
            }
        }
        if (recyclerSweep->GetPendingSweepBlockList(this) == nullptr)
        {
            // nothing else to sweep now,  we can start allocating now.
            this->StartAllocationAfterSweep();
        }
    }

    RECYCLER_SLOW_CHECK(this->VerifyHeapBlockCount(recyclerSweep != nullptr && recyclerSweep->IsBackground()));
}

template <typename TBlockType>
void
SmallNormalHeapBucketBase<TBlockType>::EnumerateObjects(ObjectInfoBits infoBits, void (*CallBackFunction)(void * address, size_t size))
{
    __super::EnumerateObjects(infoBits, CallBackFunction);
    HeapBucket::EnumerateObjects(partialHeapBlockList, infoBits, CallBackFunction);
#ifdef CONCURRENT_GC_ENABLED
    HeapBucket::EnumerateObjects(partialSweptHeapBlockList, infoBits, CallBackFunction);
#endif
}

//------------------------------------------------------------------------------
// Debug and verify functions
//------------------------------------------------------------------------------
#if DBG
template <typename TBlockType>
void
SmallNormalHeapBucketBase<TBlockType>::ResetMarks(ResetMarkFlags flags)
{
    Assert(this->partialHeapBlockList == nullptr);
#ifdef CONCURRENT_GC_ENABLED
    Assert(this->partialSweptHeapBlockList == nullptr);
#endif
    __super::ResetMarks(flags);
}

template <typename TBlockType>
void
SmallNormalHeapBucketBase<TBlockType>::SweepVerifyPartialBlocks(Recycler * recycler, TBlockType * heapBlockList)
{
    // PARTIALGC-TODO: Add assert to ensure nothing in the partialHeapBlockList is free-able
    HeapBlockList::ForEach(heapBlockList, [recycler](TBlockType * heapBlock)
    {
        heapBlock->SweepVerifyPartialBlock(recycler);
    });
}
#endif // DBG


template <typename TBlockType>
template<bool pageheap>
void
SmallNormalHeapBucketBase<TBlockType>::Sweep(RecyclerSweep& recyclerSweep)
{
#if DBG
    Recycler * recycler = recyclerSweep.GetRecycler();
    // Don't need sweep the partialHeapBlockList, the partially collected heap block list.
    // There should be nothing there that is free-able since the last time we swept

    Assert(recyclerSweep.InPartialCollect() || partialHeapBlockList == nullptr);
#ifdef CONCURRENT_GC_ENABLED
    Assert(recyclerSweep.InPartialCollect() || partialSweptHeapBlockList == nullptr);
#endif
    this->SweepVerifyPartialBlocks(recycler, this->partialHeapBlockList);
#endif
    BaseT::SweepBucket<pageheap>(recyclerSweep, [](RecyclerSweep& recyclerSweep){});
}


#if DBG || defined(RECYCLER_SLOW_CHECK_ENABLED)
template <typename TBlockType>
size_t
SmallNormalHeapBucketBase<TBlockType>::GetNonEmptyHeapBlockCount(bool checkCount) const
{
    size_t currentHeapBlockCount = __super::GetNonEmptyHeapBlockCount(false);
    currentHeapBlockCount += HeapBlockList::Count(partialHeapBlockList);
#ifdef CONCURRENT_GC_ENABLED
    currentHeapBlockCount += HeapBlockList::Count(partialSweptHeapBlockList);
#endif
    RECYCLER_SLOW_CHECK(Assert(!checkCount || heapBlockCount == currentHeapBlockCount));
    return currentHeapBlockCount;
}
#endif
#ifdef RECYCLER_SLOW_CHECK_ENABLED
template <typename TBlockType>
size_t
SmallNormalHeapBucketBase<TBlockType>::Check(bool checkCount)
{
    size_t smallHeapBlockCount = __super::Check(false);
    Assert(partialHeapBlockList == nullptr || this->GetRecycler()->inPartialCollectMode);
    smallHeapBlockCount += HeapInfo::Check(false, false, this->partialHeapBlockList);

#ifdef CONCURRENT_GC_ENABLED
    Assert(partialSweptHeapBlockList == nullptr || this->GetRecycler()->inPartialCollectMode);
    smallHeapBlockCount += HeapInfo::Check(false, false, this->partialSweptHeapBlockList);
#endif
    Assert(!checkCount || this->heapBlockCount == smallHeapBlockCount);
    return smallHeapBlockCount;
}

#endif // RECYCLER_SLOW_CHECK_ENABLED

#ifdef RECYCLER_MEMORY_VERIFY
template <typename TBlockType>
void
SmallNormalHeapBucketBase<TBlockType>::Verify()
{
    __super::Verify();
    Assert(this->partialHeapBlockList == nullptr || this->GetRecycler()->inPartialCollectMode);
    HeapBlockList::ForEach(this->partialHeapBlockList, [](TBlockType * heapBlock)
    {
        Assert(heapBlock->HasFreeObject());
        heapBlock->Verify();
    });
#ifdef CONCURRENT_GC_ENABLED
    Assert(this->partialSweptHeapBlockList == nullptr || this->GetRecycler()->inPartialCollectMode);
    HeapBlockList::ForEach(this->partialSweptHeapBlockList, [](TBlockType * heapBlock)
    {
        heapBlock->Verify();
    });
#endif
}
#endif // RECYCLER_MEMORY_VERIFY
#ifdef RECYCLER_VERIFY_MARK
template <typename TBlockType>
void
SmallNormalHeapBucketBase<TBlockType>::VerifyMark()
{
    __super::VerifyMark();
    HeapBlockList::ForEach(this->partialHeapBlockList, [](TBlockType * heapBlock)
    {
        heapBlock->VerifyMark();
    });

#ifdef CONCURRENT_GC_ENABLED
    HeapBlockList::ForEach(this->partialSweptHeapBlockList, [](TBlockType * heapBlock)
    {
        heapBlock->VerifyMark();
    });
#endif
}
#endif // RECYCLER_VERIFY_MARK
#endif // PARTIAL_GC_ENABLED

template class SmallNormalHeapBucketBase<SmallNormalHeapBlock>;
template class SmallNormalHeapBucketBase<MediumNormalHeapBlock>;

#ifdef RECYCLER_WRITE_BARRIER
template class SmallNormalHeapBucketBase<SmallNormalWithBarrierHeapBlock>;
template class SmallNormalHeapBucketBase<MediumNormalWithBarrierHeapBlock>;
#endif

template class SmallNormalHeapBucketBase<SmallFinalizableHeapBlock>;
template class SmallNormalHeapBucketBase<MediumFinalizableHeapBlock>;

#ifdef RECYCLER_WRITE_BARRIER
template class SmallNormalHeapBucketBase<SmallFinalizableWithBarrierHeapBlock>;
template class SmallNormalHeapBucketBase<MediumFinalizableWithBarrierHeapBlock>;
#endif

template void SmallNormalHeapBucketBase<SmallNormalHeapBlock>::Sweep<true>(RecyclerSweep& recyclerSweep);
template void SmallNormalHeapBucketBase<SmallNormalHeapBlock>::Sweep<false>(RecyclerSweep& recyclerSweep);
template void SmallNormalHeapBucketBase<MediumNormalHeapBlock>::Sweep<true>(RecyclerSweep& recyclerSweep);
template void SmallNormalHeapBucketBase<MediumNormalHeapBlock>::Sweep<false>(RecyclerSweep& recyclerSweep);

#ifdef RECYCLER_WRITE_BARRIER
template void SmallNormalHeapBucketBase<SmallNormalWithBarrierHeapBlock>::Sweep<true>(RecyclerSweep& recyclerSweep);
template void SmallNormalHeapBucketBase<SmallNormalWithBarrierHeapBlock>::Sweep<false>(RecyclerSweep& recyclerSweep);
template void SmallNormalHeapBucketBase<MediumNormalWithBarrierHeapBlock>::Sweep<true>(RecyclerSweep& recyclerSweep);
template void SmallNormalHeapBucketBase<MediumNormalWithBarrierHeapBlock>::Sweep<false>(RecyclerSweep& recyclerSweep);
#endif
