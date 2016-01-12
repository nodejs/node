//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonMemoryPch.h"

#ifdef PARTIAL_GC_ENABLED
#define KILOBYTES * 1024
#define MEGABYTES * 1024 KILOBYTES
#define MEGABYTES_OF_PAGES * 1024 * 1024 / AutoSystemInfo::PageSize;

const uint RecyclerSweep::MinPartialUncollectedNewPageCount = 4 MEGABYTES_OF_PAGES;
const uint RecyclerSweep::MaxPartialCollectRescanRootBytes = 5 MEGABYTES;
static const uint MinPartialCollectRescanRootBytes = 128 KILOBYTES;

// Maximum unused partial collect free bytes before we get out of partial GC mode
static const uint MaxUnusedPartialCollectFreeBytes = 16 MEGABYTES;

// Have to collected at least 10% before we would partial GC
// CONSIDER: It may be good to do partial with low efficacy once we have concurrent partial
// because old object are not getting collected as well, but without concurrent partial, we will have to mark
// new objects in thread.
static const double MinPartialCollectEfficacy = 0.1;
#endif

bool
RecyclerSweep::IsMemProtectMode()
{
    return recycler->IsMemProtectMode();
}

void
RecyclerSweep::BeginSweep(Recycler * recycler, size_t rescanRootBytes, bool adjustPartialHeuristics)
{
    {
        // We are about to sweep, give the runtime a chance to see the now-immutable state of the world.
        // And clean up all the cache not monitor by the GC (e.g. inline caches)
        AUTO_NO_EXCEPTION_REGION;
        recycler->collectionWrapper->PreSweepCallback();
    }

    Assert(!recycler->IsSweeping());
    Assert(recycler->recyclerSweep == nullptr);

    memset(this, 0, sizeof(RecyclerSweep));
    this->recycler = recycler;
    recycler->recyclerSweep = this;

    // We might still have block that has disposed but not put back into the allocable
    // heap block list yet, which happens if we finish disposing object during concurrent
    // reset mark and can't
    // modify the heap block lists

    // CONCURRENT-TODO: Consider doing it during FinishDisposeObjects to get these block
    // available sooner as well.   We will still need it here as we only always get to
    // finish dispose before sweep.
    this->FlushPendingTransferDisposedObjects();

#ifdef CONCURRENT_GC_ENABLED
    // Take the small heap block new heap block list and store in RecyclerSweep temporary
    // We get merge later before we start sweeping the bucket.

    leafData.pendingMergeNewHeapBlockList = recycler->autoHeap.newLeafHeapBlockList;
    normalData.pendingMergeNewHeapBlockList = recycler->autoHeap.newNormalHeapBlockList;
#ifdef RECYCLER_WRITE_BARRIER
    withBarrierData.pendingMergeNewHeapBlockList = recycler->autoHeap.newNormalWithBarrierHeapBlockList;
    finalizableWithBarrierData.pendingMergeNewHeapBlockList = recycler->autoHeap.newFinalizableWithBarrierHeapBlockList;
#endif
    finalizableData.pendingMergeNewHeapBlockList = recycler->autoHeap.newFinalizableHeapBlockList;

    mediumLeafData.pendingMergeNewHeapBlockList = recycler->autoHeap.newMediumLeafHeapBlockList;
    mediumNormalData.pendingMergeNewHeapBlockList = recycler->autoHeap.newMediumNormalHeapBlockList;
#ifdef RECYCLER_WRITE_BARRIER
    mediumWithBarrierData.pendingMergeNewHeapBlockList = recycler->autoHeap.newMediumNormalWithBarrierHeapBlockList;
    mediumFinalizableWithBarrierData.pendingMergeNewHeapBlockList = recycler->autoHeap.newMediumFinalizableWithBarrierHeapBlockList;
#endif
    mediumFinalizableData.pendingMergeNewHeapBlockList = recycler->autoHeap.newMediumFinalizableHeapBlockList;


    recycler->autoHeap.newLeafHeapBlockList = nullptr;
    recycler->autoHeap.newNormalHeapBlockList = nullptr;
    recycler->autoHeap.newFinalizableHeapBlockList = nullptr;
#ifdef RECYCLER_WRITE_BARRIER
    recycler->autoHeap.newNormalWithBarrierHeapBlockList = nullptr;
    recycler->autoHeap.newFinalizableWithBarrierHeapBlockList = nullptr;
#endif

    recycler->autoHeap.newMediumLeafHeapBlockList = nullptr;
    recycler->autoHeap.newMediumNormalHeapBlockList = nullptr;
    recycler->autoHeap.newMediumFinalizableHeapBlockList = nullptr;
#ifdef RECYCLER_WRITE_BARRIER
    recycler->autoHeap.newMediumNormalWithBarrierHeapBlockList = nullptr;
    recycler->autoHeap.newMediumFinalizableWithBarrierHeapBlockList = nullptr;
#endif

#endif

#ifdef PARTIAL_GC_ENABLED
    Assert(recycler->clientTrackedObjectList.Empty());

    // We should not have partialUncollectedAllocBytes unless we are in partial collect at this point
    Assert(recycler->partialUncollectedAllocBytes == 0 || recycler->inPartialCollectMode);
    Assert(recycler->autoHeap.uncollectedAllocBytes >= recycler->partialUncollectedAllocBytes);

    // if the cost of rescan is too high, we want to disable partial GC starting from the
    // upcoming Sweep. We basically move the check up from AdjustPartialHeuristics to here
    // such that we can have the decision before sweep.

    this->rescanRootBytes = rescanRootBytes;

    RECYCLER_STATS_SET(recycler, rescanRootBytes, rescanRootBytes);

    if (this->DoPartialCollectMode())
    {
        // enable partial collect for sweep & next round of GC
        DebugOnly(this->partial = true);

        // REVIEW: is adjustPartialHeuristicsMode  the same as in PartialCollectMode?
        this->adjustPartialHeuristics = adjustPartialHeuristics;
        this->StartPartialCollectMode();
    }
    else
    {
        // disable partial collect
        if (recycler->inPartialCollectMode)
        {
            recycler->FinishPartialCollect();
        }

        Assert(recycler->partialUncollectedAllocBytes == 0);

        Assert(!recycler->inPartialCollectMode);
    }
#endif

    if (this->inPartialCollect)
    {
        // We just did a partial collect.
        // We only want to count objects that survived this collect towards the next full GC.
        // Thus, clear out uncollectedAllocBytes here; we will adjust to account for objects that
        // survived this partial collect in EndSweep.
        recycler->ResetHeuristicCounters();
    }
    else
    {
        // We just did a full collect.
        // We reset uncollectedAllocBytes when we kicked off the collection,
        // so don't reset it here (but do reset partial heuristics).
        recycler->ResetPartialHeuristicCounters();
    }
}

void
RecyclerSweep::FinishSweep()
{
#ifdef PARTIAL_GC_ENABLED
    Assert(this->partial == recycler->inPartialCollectMode);
    // Adjust heuristics
    if (recycler->inPartialCollectMode)
    {
        if (this->AdjustPartialHeuristics())
        {
            GCETW(GC_SWEEP_PARTIAL_REUSE_PAGE_START, (recycler));

            // If we are doing a full concurrent GC, all allocated bytes are consider "collected".
            // We only start accumulating uncollected allocate bytes during partial GC.
            // FinishPartialCollect will reset it to 0 if we are not doing a partial GC
            recycler->partialUncollectedAllocBytes = this->InPartialCollect()? this->nextPartialUncollectedAllocBytes : 0;

#ifdef RECYCLER_TRACE
            if (recycler->GetRecyclerFlagsTable().Trace.IsEnabled(Js::PartialCollectPhase))
            {
                Output::Print(L"AdjustPartialHeuristics returned true\n");
                Output::Print(L"  partialUncollectedAllocBytes = %d\n", recycler->partialUncollectedAllocBytes);
                Output::Print(L"  nextPartialUncollectedAllocBytes = %d\n", this->nextPartialUncollectedAllocBytes);
            }
#endif

            recycler->autoHeap.SweepPartialReusePages(*this);

            GCETW(GC_SWEEP_PARTIAL_REUSE_PAGE_STOP, (recycler));

            if (!this->IsBackground())
            {
                RECYCLER_PROFILE_EXEC_BEGIN(recycler, Js::ResetWriteWatchPhase);
                if (!recycler->recyclerPageAllocator.ResetWriteWatch() ||
                    !recycler->recyclerLargeBlockPageAllocator.ResetWriteWatch())
                {
                    // Shouldn't happen
                    Assert(false);
                    recycler->enablePartialCollect = false;
                    recycler->FinishPartialCollect(this);
                }
                RECYCLER_PROFILE_EXEC_END(recycler, Js::ResetWriteWatchPhase);
            }
        }
        else
        {
#ifdef RECYCLER_TRACE
            if (recycler->GetRecyclerFlagsTable().Trace.IsEnabled(Js::PartialCollectPhase))
            {
                Output::Print(L"AdjustPartialHeuristics returned false\n");
            }
#endif

            if (this->IsBackground())
            {
                recycler->BackgroundFinishPartialCollect(this);
            }
            else
            {
                recycler->FinishPartialCollect(this);
            }
        }
    }
    else
    {
        Assert(!this->adjustPartialHeuristics);

        // Initial value or Sweep should have called FinishPartialCollect to these if we are not doing partial
        Assert(recycler->partialUncollectedAllocBytes == 0);
    }

    recycler->SweepPendingObjects(*this);
#endif
}

void
RecyclerSweep::EndSweep()
{
#ifdef PARTIAL_GC_ENABLED
    // We clear out the old uncollectedAllocBytes, restore it now to get the adjustment for partial
    // We clear it again after we are done collecting and if we are not in partial collect
    if (this->inPartialCollect)
    {
        recycler->autoHeap.uncollectedAllocBytes += this->nextPartialUncollectedAllocBytes;

#ifdef RECYCLER_TRACE
        if (recycler->GetRecyclerFlagsTable().Trace.IsEnabled(Js::PartialCollectPhase))
        {
            Output::Print(L"EndSweep for partial sweep\n");
            Output::Print(L"  uncollectedAllocBytes = %d\n", recycler->autoHeap.uncollectedAllocBytes);
            Output::Print(L"  nextPartialUncollectedAllocBytes = %d\n", this->nextPartialUncollectedAllocBytes);
        }
#endif
    }
#endif

    recycler->recyclerSweep = nullptr;

    // Clean up the HeapBlockMap.
    // This will release any internal structures that are no longer needed after Sweep.
    recycler->heapBlockMap.Cleanup(!recycler->IsMemProtectMode());
}

void
RecyclerSweep::BackgroundSweep()
{
    this->BeginBackground(forceForeground);

    if (GetRecycler()->IsPageHeapEnabled())
    {
        // Finish the concurrent part of the first pass
        this->recycler->autoHeap.SweepSmallNonFinalizable<true>(*this);
    }
    else
    {
        // Finish the concurrent part of the first pass
        this->recycler->autoHeap.SweepSmallNonFinalizable<false>(*this);
    }

    // Finish the rest of the sweep
    this->FinishSweep();

    this->EndBackground();
}

Recycler *
RecyclerSweep::GetRecycler() const
{
    return recycler;
}

bool
RecyclerSweep::IsBackground() const
{
    return this->background;
}

bool
RecyclerSweep::HasSetupBackgroundSweep() const
{
    return this->IsBackground() || this->forceForeground;
}

void
RecyclerSweep::FlushPendingTransferDisposedObjects()
{
    if (recycler->hasPendingTransferDisposedObjects)
    {
        // If recycler->inResolveExternalWeakReferences is true, the recycler isn't really disposing anymore
        // so it's safe to call transferDisposedObjects
        Assert(!recycler->inDispose || recycler->inResolveExternalWeakReferences);
        Assert(!recycler->hasDisposableObject);
        recycler->autoHeap.TransferDisposedObjects();
    }
}

void
RecyclerSweep::ShutdownCleanup()
{
    SmallLeafHeapBucketT<SmallAllocationBlockAttributes>::DeleteHeapBlockList(this->leafData.pendingMergeNewHeapBlockList, recycler);
    SmallNormalHeapBucket::DeleteHeapBlockList(this->normalData.pendingMergeNewHeapBlockList, recycler);
#ifdef RECYCLER_WRITE_BARRIER
    SmallNormalWithBarrierHeapBucket::DeleteHeapBlockList(this->withBarrierData.pendingMergeNewHeapBlockList, recycler);
    SmallFinalizableWithBarrierHeapBucket::DeleteHeapBlockList(this->finalizableWithBarrierData.pendingMergeNewHeapBlockList, recycler);
#endif
    SmallFinalizableHeapBucket::DeleteHeapBlockList(this->finalizableData.pendingMergeNewHeapBlockList, recycler);
    for (uint i = 0; i < HeapConstants::BucketCount; i++)
    {
        // For leaf, we can always reuse the page as we don't need to rescan them for partial GC
        // It should have been swept immediately during Sweep
        Assert(this->leafData.bucketData[i].pendingSweepList == nullptr);
        SmallNormalHeapBucket::DeleteHeapBlockList(this->normalData.bucketData[i].pendingSweepList, recycler);
        SmallFinalizableHeapBucket::DeleteHeapBlockList(this->finalizableData.bucketData[i].pendingSweepList, recycler);
#ifdef RECYCLER_WRITE_BARRIER
        SmallFinalizableWithBarrierHeapBucket::DeleteHeapBlockList(this->finalizableWithBarrierData.bucketData[i].pendingSweepList, recycler);
#endif

        SmallLeafHeapBucket::DeleteEmptyHeapBlockList(this->leafData.bucketData[i].pendingEmptyBlockList);
        SmallNormalHeapBucket::DeleteEmptyHeapBlockList(this->normalData.bucketData[i].pendingEmptyBlockList);
#ifdef RECYCLER_WRITE_BARRIER
        SmallNormalWithBarrierHeapBucket::DeleteEmptyHeapBlockList(this->withBarrierData.bucketData[i].pendingEmptyBlockList);
        Assert(this->finalizableWithBarrierData.bucketData[i].pendingEmptyBlockList == nullptr);
#endif
        Assert(this->finalizableData.bucketData[i].pendingEmptyBlockList == nullptr);
    }

    MediumLeafHeapBucket::DeleteHeapBlockList(this->mediumLeafData.pendingMergeNewHeapBlockList, recycler);
    MediumNormalHeapBucket::DeleteHeapBlockList(this->mediumNormalData.pendingMergeNewHeapBlockList, recycler);
#ifdef RECYCLER_WRITE_BARRIER
    MediumNormalWithBarrierHeapBucket::DeleteHeapBlockList(this->mediumWithBarrierData.pendingMergeNewHeapBlockList, recycler);
    MediumFinalizableWithBarrierHeapBucket::DeleteHeapBlockList(this->mediumFinalizableWithBarrierData.pendingMergeNewHeapBlockList, recycler);
#endif
    MediumFinalizableHeapBucket::DeleteHeapBlockList(this->mediumFinalizableData.pendingMergeNewHeapBlockList, recycler);
    for (uint i = 0; i < HeapConstants::MediumBucketCount; i++)
    {
        // For leaf, we can always reuse the page as we don't need to rescan them for partial GC
        // It should have been swept immediately during Sweep
        Assert(this->mediumLeafData.bucketData[i].pendingSweepList == nullptr);
        MediumNormalHeapBucket::DeleteHeapBlockList(this->mediumNormalData.bucketData[i].pendingSweepList, recycler);
        MediumFinalizableHeapBucket::DeleteHeapBlockList(this->mediumFinalizableData.bucketData[i].pendingSweepList, recycler);
#ifdef RECYCLER_WRITE_BARRIER
        MediumFinalizableWithBarrierHeapBucket::DeleteHeapBlockList(this->mediumFinalizableWithBarrierData.bucketData[i].pendingSweepList, recycler);
#endif

        MediumLeafHeapBucket::DeleteEmptyHeapBlockList(this->mediumLeafData.bucketData[i].pendingEmptyBlockList);
        MediumNormalHeapBucket::DeleteEmptyHeapBlockList(this->mediumNormalData.bucketData[i].pendingEmptyBlockList);
#ifdef RECYCLER_WRITE_BARRIER
        MediumNormalWithBarrierHeapBucket::DeleteEmptyHeapBlockList(this->mediumWithBarrierData.bucketData[i].pendingEmptyBlockList);
        Assert(this->mediumFinalizableWithBarrierData.bucketData[i].pendingEmptyBlockList == nullptr);
#endif
        Assert(this->mediumFinalizableData.bucketData[i].pendingEmptyBlockList == nullptr);
    }
}

#ifdef CONCURRENT_GC_ENABLED
template <typename TBlockType>
void
RecyclerSweep::MergePendingNewHeapBlockList()
{
    TBlockType *& blockList = this->GetData<TBlockType>().pendingMergeNewHeapBlockList;
    TBlockType * list = blockList;
    blockList = nullptr;
    HeapInfo& heapInfo = recycler->autoHeap;
    HeapBlockList::ForEachEditing(list, [&heapInfo](TBlockType * heapBlock)
    {
        auto& bucket = heapInfo.GetBucket<TBlockType::RequiredAttributes>(heapBlock->GetObjectSize());
        bucket.MergeNewHeapBlock(heapBlock);
    });
}
template void RecyclerSweep::MergePendingNewHeapBlockList<SmallLeafHeapBlock>();
template void RecyclerSweep::MergePendingNewHeapBlockList<SmallNormalHeapBlock>();
template void RecyclerSweep::MergePendingNewHeapBlockList<SmallFinalizableHeapBlock>();
#ifdef RECYCLER_WRITE_BARRIER
template void RecyclerSweep::MergePendingNewHeapBlockList<SmallNormalWithBarrierHeapBlock>();
template void RecyclerSweep::MergePendingNewHeapBlockList<SmallFinalizableWithBarrierHeapBlock>();
#endif

template <typename TBlockType>
void
RecyclerSweep::MergePendingNewMediumHeapBlockList()
{
    TBlockType *& blockList = this->GetData<TBlockType>().pendingMergeNewHeapBlockList;
    TBlockType * list = blockList;
    blockList = nullptr;
    HeapInfo& heapInfo = recycler->autoHeap;
    HeapBlockList::ForEachEditing(list, [&heapInfo](TBlockType * heapBlock)
    {
        auto& bucket = heapInfo.GetMediumBucket<TBlockType::RequiredAttributes>(heapBlock->GetObjectSize());
        bucket.MergeNewHeapBlock(heapBlock);
    });
}

template void RecyclerSweep::MergePendingNewMediumHeapBlockList<MediumLeafHeapBlock>();
template void RecyclerSweep::MergePendingNewMediumHeapBlockList<MediumNormalHeapBlock>();
template void RecyclerSweep::MergePendingNewMediumHeapBlockList<MediumFinalizableHeapBlock>();
#ifdef RECYCLER_WRITE_BARRIER
template void RecyclerSweep::MergePendingNewMediumHeapBlockList<MediumNormalWithBarrierHeapBlock>();
template void RecyclerSweep::MergePendingNewMediumHeapBlockList<MediumFinalizableWithBarrierHeapBlock>();
#endif

bool
RecyclerSweep::HasPendingEmptyBlocks() const
{
    return this->hasPendingEmptyBlocks;
}

bool
RecyclerSweep::HasPendingSweepSmallHeapBlocks() const
{
    return this->hasPendingSweepSmallHeapBlocks;
}

void
RecyclerSweep::SetHasPendingSweepSmallHeapBlocks()
{
    this->hasPendingSweepSmallHeapBlocks = true;
}

void
RecyclerSweep::BeginBackground(bool forceForeground)
{
    Assert(!background);
    this->background = !forceForeground;
    this->forceForeground = forceForeground;
}

void
RecyclerSweep::EndBackground()
{
    Assert(this->background || this->forceForeground);
    this->background = false;
}

#if DBG
bool
RecyclerSweep::HasPendingNewHeapBlocks() const
{
    return leafData.pendingMergeNewHeapBlockList != nullptr
        || normalData.pendingMergeNewHeapBlockList != nullptr
        || finalizableData.pendingMergeNewHeapBlockList != nullptr
#ifdef RECYCLER_WRITE_BARRIER
        || withBarrierData.pendingMergeNewHeapBlockList != nullptr
        || finalizableWithBarrierData.pendingMergeNewHeapBlockList != nullptr
#endif
        || mediumLeafData.pendingMergeNewHeapBlockList != nullptr
        || mediumNormalData.pendingMergeNewHeapBlockList != nullptr
        || mediumFinalizableData.pendingMergeNewHeapBlockList != nullptr
#ifdef RECYCLER_WRITE_BARRIER
        || mediumWithBarrierData.pendingMergeNewHeapBlockList != nullptr
        || mediumFinalizableWithBarrierData.pendingMergeNewHeapBlockList != nullptr
#endif

        ;
}
#endif

#if DBG || defined(RECYCLER_SLOW_CHECK_ENABLED)
size_t
RecyclerSweep::SetPendingMergeNewHeapBlockCount()
{
    return HeapBlockList::Count(leafData.pendingMergeNewHeapBlockList)
        + HeapBlockList::Count(normalData.pendingMergeNewHeapBlockList)
        + HeapBlockList::Count(finalizableData.pendingMergeNewHeapBlockList)
#ifdef RECYCLER_WRITE_BARRIER
        + HeapBlockList::Count(withBarrierData.pendingMergeNewHeapBlockList)
        + HeapBlockList::Count(finalizableWithBarrierData.pendingMergeNewHeapBlockList)
#endif
        + HeapBlockList::Count(mediumLeafData.pendingMergeNewHeapBlockList)
        + HeapBlockList::Count(mediumNormalData.pendingMergeNewHeapBlockList)
        + HeapBlockList::Count(mediumFinalizableData.pendingMergeNewHeapBlockList)
#ifdef RECYCLER_WRITE_BARRIER
        + HeapBlockList::Count(mediumWithBarrierData.pendingMergeNewHeapBlockList)
        + HeapBlockList::Count(mediumFinalizableWithBarrierData.pendingMergeNewHeapBlockList)
#endif
        ;
}
#endif
#endif

#ifdef PARTIAL_GC_ENABLED
bool
RecyclerSweep::InPartialCollectMode() const
{
    return recycler->inPartialCollectMode;
}

bool
RecyclerSweep::InPartialCollect() const
{
    return this->inPartialCollect;
}

void
RecyclerSweep::StartPartialCollectMode()
{
    // Save the in partial collect, the main thread reset it after returning to the script
    // and the background thread still needs it
    this->inPartialCollect = recycler->inPartialCollectMode;
    recycler->inPartialCollectMode = true;

    // Tracks the unallocated alloc bytes for partial GC

    // Keep a copy Last collection's uncollected allocation bytes, so we can use it to calculate
    // the new object that is allocated since the last GC
    Assert(recycler->partialUncollectedAllocBytes == 0 || this->inPartialCollect);
    this->lastPartialUncollectedAllocBytes = recycler->partialUncollectedAllocBytes;

    size_t currentUncollectedAllocBytes = recycler->autoHeap.uncollectedAllocBytes;
    Assert(currentUncollectedAllocBytes >= this->lastPartialUncollectedAllocBytes);

    if (!this->inPartialCollect)
    {
        // If we did a full collect, then we need to include lastUncollectedAllocBytes
        // in the partialUncollectedAllocBytes calculation, because all objects allocated
        // since the previous GC are considered new, but we cleared uncollectedAllocBytes
        // when we kicked off the GC.
        currentUncollectedAllocBytes += recycler->autoHeap.lastUncollectedAllocBytes;
    }

    // Initially, the partial uncollected alloc bytes is the current uncollectedAllocBytes
    recycler->partialUncollectedAllocBytes = currentUncollectedAllocBytes;
    this->nextPartialUncollectedAllocBytes = currentUncollectedAllocBytes;

#ifdef RECYCLER_TRACE
    if (recycler->GetRecyclerFlagsTable().Trace.IsEnabled(Js::PartialCollectPhase))
    {
        Output::Print(L"StartPartialCollectMode\n");
        Output::Print(L"  was inPartialCollectMode = %d\n", this->inPartialCollect);
        Output::Print(L"  lastPartialUncollectedAllocBytes = %d\n", this->lastPartialUncollectedAllocBytes);
        Output::Print(L"  uncollectedAllocBytes = %d\n", recycler->autoHeap.uncollectedAllocBytes);
        Output::Print(L"  nextPartialUncollectedAllocBytes = %d\n", this->nextPartialUncollectedAllocBytes);
    }
#endif
}

// Called by prepare sweep to track the new allocated bytes on block that is not fully allocated yet.
template <typename TBlockAttributes>
void
RecyclerSweep::AddUnaccountedNewObjectAllocBytes(SmallHeapBlockT<TBlockAttributes> * heapBlock)
{
#ifdef PARTIAL_GC_ENABLED
    // Only need to update the unaccounted alloc bytes if we are in partial collect mode
    if (recycler->inPartialCollectMode)
    {
        uint unaccountedAllocBytes = heapBlock->GetAndClearUnaccountedAllocBytes();
        Assert(heapBlock->lastUncollectedAllocBytes == 0 || unaccountedAllocBytes == 0);
        DebugOnly(heapBlock->lastUncollectedAllocBytes += unaccountedAllocBytes);
        recycler->partialUncollectedAllocBytes += unaccountedAllocBytes;
        this->nextPartialUncollectedAllocBytes += unaccountedAllocBytes;
    }
    else
#endif
    {
        // We don't care, clear the unaccounted to start tracking for new object for next GC
        heapBlock->ClearAllAllocBytes();
    }
}

template void RecyclerSweep::AddUnaccountedNewObjectAllocBytes<SmallAllocationBlockAttributes>(SmallHeapBlock * heapBlock);
template void RecyclerSweep::AddUnaccountedNewObjectAllocBytes<MediumAllocationBlockAttributes>(MediumHeapBlock * heapBlock);

void
RecyclerSweep::SubtractSweepNewObjectAllocBytes(size_t newObjectExpectSweepByteCount)
{
    Assert(recycler->inPartialCollectMode);
    // We shouldn't free more then we allocated
    Assert(this->nextPartialUncollectedAllocBytes >= newObjectExpectSweepByteCount);
    Assert(this->nextPartialUncollectedAllocBytes >= this->lastPartialUncollectedAllocBytes + newObjectExpectSweepByteCount);
    this->nextPartialUncollectedAllocBytes -= newObjectExpectSweepByteCount;
}


/*--------------------------------------------------------------------------------------------
 * Determine we want to go into partial collect mode for the next GC before we sweep,
 * based on the number bytes needed to rescan (<= 5MB)
 *--------------------------------------------------------------------------------------------*/
bool
RecyclerSweep::DoPartialCollectMode()
{
    if (!recycler->enablePartialCollect)
    {
        return false;
    }

    // If we exceed 16MB of unused memory in partial blocks, get out of partial collect to avoid
    // memory fragmentation.
    if (recycler->autoHeap.unusedPartialCollectFreeBytes > MaxUnusedPartialCollectFreeBytes)
    {
        return false;
    }

    return this->rescanRootBytes <= MaxPartialCollectRescanRootBytes;
}

// Heuristic ratio is ((c * e + (1 - e)) * (1 - p)) + p and use that to linearly scale between min and max
// This give cost/efficacy/pressure equal weight, while each can push it pass where partial GC is not
// beneficial
bool
RecyclerSweep::AdjustPartialHeuristics()
{
    Assert(recycler->inPartialCollectMode);
    Assert(this->adjustPartialHeuristics);
    Assert(this->InPartialCollect() || recycler->autoHeap.unusedPartialCollectFreeBytes == 0);

    // DoPartialCollectMode should have rejected these already
    Assert(this->rescanRootBytes <= (size_t)MaxPartialCollectRescanRootBytes);
    Assert(recycler->autoHeap.unusedPartialCollectFreeBytes <= MaxUnusedPartialCollectFreeBytes);

    // Page reuse Heuristics
    double collectEfficacy;
    const size_t allocBytes = this->GetNewObjectAllocBytes();
    if (allocBytes == 0)
    {
        // We may get collections without allocating memory (e.g. unpin heuristics).
        collectEfficacy = 1.0;                                          // assume 100% efficacy
        this->partialCollectSmallHeapBlockReuseMinFreeBytes = 0;    // reuse all pages
    }
    else
    {
        const size_t freedBytes = this->GetNewObjectFreeBytes();
        Assert(freedBytes <= allocBytes);

        collectEfficacy = (double)freedBytes / (double)allocBytes;

        // If we collected less then 10% of the memory, let's not do partial GC.
        // CONSIDER: It may be good to do partial with low efficacy once we have concurrent partial
        // because old object are not getting collected as well, but without concurrent partial, we will have to mark
        // new objects in thread.
        if (collectEfficacy < MinPartialCollectEfficacy)
        {
            return false;
        }

        // Scale the efficacy linearly such that an efficacy of MinPartialCollectEfficacy translates to an adjusted efficacy of
        // 0.0, and an efficacy of 1.0 translates to an adjusted efficacy of 1.0
        collectEfficacy = (collectEfficacy - MinPartialCollectEfficacy) / (1.0 - MinPartialCollectEfficacy);

        Assert(collectEfficacy <= 1.0);
        this->partialCollectSmallHeapBlockReuseMinFreeBytes = (size_t)(AutoSystemInfo::PageSize * collectEfficacy);
    }
#ifdef RECYCLER_STATS
    recycler->collectionStats.collectEfficacy = collectEfficacy;
    recycler->collectionStats.partialCollectSmallHeapBlockReuseMinFreeBytes = this->partialCollectSmallHeapBlockReuseMinFreeBytes;
#endif

    // Blocks which are being reused are likely to be touched again from allocation and contribute to Rescan cost.
    // If there are many of these, adjust rescanRootBytes to account for this.
    const size_t estimatedPartialReuseBlocks = (size_t)((double)this->reuseHeapBlockCount * (1.0 - collectEfficacy));
    const size_t estimatedPartialReuseBytes = estimatedPartialReuseBlocks * AutoSystemInfo::PageSize;

    const size_t newRescanRootBytes = max(this->rescanRootBytes, estimatedPartialReuseBytes);

    RECYCLER_STATS_SET(recycler, estimatedPartialReuseBytes, estimatedPartialReuseBytes);

    // Recheck the rescanRootBytes
    if (newRescanRootBytes > MaxPartialCollectRescanRootBytes)
    {
        return false;
    }

    double collectCost = (double)newRescanRootBytes / MaxPartialCollectRescanRootBytes;

    RECYCLER_STATS_SET(recycler, collectCost, collectCost);

    // Include the efficacy in equal portion, which is related to the cost of marking through new objects.
    // r = c * e + 1 - e;
    const double reuseRatio = 1.0 - collectEfficacy;
    double ratio = collectCost * collectEfficacy + reuseRatio;

    if (this->InPartialCollect())
    {
        // Avoid ratio of uncollectedBytesPressure > 1.0
        if (this->nextPartialUncollectedAllocBytes > RecyclerHeuristic::Instance.MaxUncollectedAllocBytesPartialCollect)
        {
            return false;
        }

        // Only add full collect pressure if we are doing partial collect,
        // account for the amount of uncollected bytes and unused bytes to increase
        // pressure to do a full GC by rising the partial GC new page heuristic

        double uncollectedBytesPressure = (double)this->nextPartialUncollectedAllocBytes / (double)RecyclerHeuristic::Instance.MaxUncollectedAllocBytesPartialCollect;
        double collectFullCollectPressure =
            (double)recycler->autoHeap.unusedPartialCollectFreeBytes / (double)MaxUnusedPartialCollectFreeBytes
            * (1.0 - uncollectedBytesPressure) + uncollectedBytesPressure;

        ratio = ratio * (1.0 - collectFullCollectPressure) + collectFullCollectPressure;
    }
    Assert(0.0 <= ratio && ratio <= 1.0);

    // Linear scale the partial GC new page heuristic using the ratio calculated
    recycler->uncollectedNewPageCountPartialCollect = MinPartialUncollectedNewPageCount
        + (size_t)((double)(RecyclerHeuristic::Instance.MaxPartialUncollectedNewPageCount - MinPartialUncollectedNewPageCount) * ratio);

    Assert(recycler->uncollectedNewPageCountPartialCollect >= MinPartialUncollectedNewPageCount &&
        recycler->uncollectedNewPageCountPartialCollect <= RecyclerHeuristic::Instance.MaxPartialUncollectedNewPageCount);

    // If the number of new page to reach the partial heuristics plus the existing uncollectedAllocBytes
    // and the memory we are going to reuse (assume we use it all) is greater then the full GC max size heuristic
    // (with 1M fudge factor), we trigger a full GC anyways, so let's not get into partial GC
    const size_t estimatedPartialReusedFreeByteCount = (size_t)((double)this->reuseByteCount * reuseRatio);
    if (recycler->uncollectedNewPageCountPartialCollect * AutoSystemInfo::PageSize
        + this->nextPartialUncollectedAllocBytes + estimatedPartialReusedFreeByteCount >= RecyclerHeuristic::Instance.MaxUncollectedAllocBytesPartialCollect)
    {
         return false;
    }

    recycler->partialConcurrentNextCollection = RecyclerHeuristic::PartialConcurrentNextCollection(ratio, recycler->GetRecyclerFlagsTable());
    return true;
}

size_t
RecyclerSweep::GetNewObjectAllocBytes() const
{
    Assert(recycler->inPartialCollectMode);
    Assert(recycler->partialUncollectedAllocBytes >= this->lastPartialUncollectedAllocBytes);
    return recycler->partialUncollectedAllocBytes - this->lastPartialUncollectedAllocBytes;
}

size_t
RecyclerSweep::GetNewObjectFreeBytes() const
{
    Assert(recycler->inPartialCollectMode);
    Assert(recycler->partialUncollectedAllocBytes >= this->nextPartialUncollectedAllocBytes);
    return recycler->partialUncollectedAllocBytes - this->nextPartialUncollectedAllocBytes;
}

size_t
RecyclerSweep::GetPartialUnusedFreeByteCount() const
{
    return partialUnusedFreeByteCount;
}

size_t
RecyclerSweep::GetPartialCollectSmallHeapBlockReuseMinFreeBytes() const
{
    return partialCollectSmallHeapBlockReuseMinFreeBytes;
}

template <typename TBlockAttributes>
void
RecyclerSweep::NotifyAllocableObjects(SmallHeapBlockT<TBlockAttributes> * heapBlock)
{
    this->reuseByteCount += heapBlock->GetExpectedFreeBytes();

    if (!heapBlock->IsLeafBlock())
    {
        this->reuseHeapBlockCount++;
    }
}

template void RecyclerSweep::NotifyAllocableObjects<SmallAllocationBlockAttributes>(SmallHeapBlock* heapBlock);
template void RecyclerSweep::NotifyAllocableObjects<MediumAllocationBlockAttributes>(MediumHeapBlock* heapBlock);

void
RecyclerSweep::AddUnusedFreeByteCount(uint expectFreeByteCount)
{
    this->partialUnusedFreeByteCount += expectFreeByteCount;
}

bool
RecyclerSweep::DoAdjustPartialHeuristics() const
{
    return this->adjustPartialHeuristics;
}
#endif
