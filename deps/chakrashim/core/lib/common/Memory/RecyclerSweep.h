//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Memory
{
// RecyclerSweep - Sweeping algorithm and state
class RecyclerSweep
#if DBG
    : public RecyclerVerifyListConsistencyData
#endif
{
public:
    void BeginSweep(Recycler * recycler, size_t rescanRootBytes, bool adjustPartialHeuristics);
    void FinishSweep();
    void EndSweep();
    void ShutdownCleanup();

    Recycler * GetRecycler() const;
    bool IsBackground() const;
    bool HasSetupBackgroundSweep() const;
    void FlushPendingTransferDisposedObjects();

#if defined(PARTIAL_GC_ENABLED) || defined(CONCURRENT_GC_ENABLED)
    bool HasPendingSweepSmallHeapBlocks() const;
    void SetHasPendingSweepSmallHeapBlocks();
    template <typename TBlockType>
    TBlockType *& GetPendingSweepBlockList(HeapBucketT<TBlockType> const * heapBucket);
#endif
#ifdef CONCURRENT_GC_ENABLED
    bool HasPendingEmptyBlocks() const;
    template <typename TBlockType, bool pageheap> void QueueEmptyHeapBlock(HeapBucketT<TBlockType> const *heapBucket, TBlockType * heapBlock);
    template <typename TBlockType> void TransferPendingEmptyHeapBlocks(HeapBucketT<TBlockType> * heapBucket);

    void BackgroundSweep();
    void BeginBackground(bool forceForeground);
    void EndBackground();

    template <typename TBlockType> void SetPendingMergeNewHeapBlockList(TBlockType * heapBlockList);
    template <typename TBlockType> void MergePendingNewHeapBlockList();
    template <typename TBlockType> void MergePendingNewMediumHeapBlockList();
#if DBG
    bool HasPendingNewHeapBlocks() const;
    template <typename TBlockType> TBlockType * GetSavedNextAllocableBlockHead(HeapBucketT<TBlockType> const * heapBucket);
    template <typename TBlockType> void SaveNextAllocableBlockHead(HeapBucketT<TBlockType> const * heapBucket);
#endif
#if DBG || defined(RECYCLER_SLOW_CHECK_ENABLED)
    template < typename TBlockType> size_t GetHeapBlockCount(HeapBucketT<TBlockType> const * heapBucket);
    size_t SetPendingMergeNewHeapBlockCount();
#endif
#endif

#ifdef PARTIAL_GC_ENABLED
    bool InPartialCollectMode() const;
    bool InPartialCollect() const;
    void StartPartialCollectMode();
    bool DoPartialCollectMode();
    bool DoAdjustPartialHeuristics() const;
    bool AdjustPartialHeuristics();
    template <typename TBlockAttributes>
    void AddUnaccountedNewObjectAllocBytes(SmallHeapBlockT<TBlockAttributes> * smallHeapBlock);
    void SubtractSweepNewObjectAllocBytes(size_t newObjectExpectSweepByteCount);
    size_t GetNewObjectAllocBytes() const;
    size_t GetNewObjectFreeBytes() const;
    size_t GetPartialUnusedFreeByteCount() const;
    size_t GetPartialCollectSmallHeapBlockReuseMinFreeBytes() const;

    template <typename TBlockAttributes>
    void NotifyAllocableObjects(SmallHeapBlockT<TBlockAttributes> * smallHeapBlock);
    void AddUnusedFreeByteCount(uint expectedFreeByteCount);

    static const uint MinPartialUncollectedNewPageCount; // 4MB pages
    static const uint MaxPartialCollectRescanRootBytes; // 5MB
#endif

private:
    template <typename TBlockType>
    struct BucketData
    {
#if defined(PARTIAL_GC_ENABLED) || defined(CONCURRENT_GC_ENABLED)
        TBlockType * pendingSweepList;
        TBlockType * pendingFinalizableSweptList;
#endif
#ifdef CONCURRENT_GC_ENABLED
        TBlockType * pendingEmptyBlockList;
        TBlockType * pendingEmptyBlockListTail;
#if DBG

        TBlockType * savedNextAllocableBlockHead;
#endif
#endif
    };

    template <typename TBlockType>
    BucketData<TBlockType>& GetBucketData(HeapBucketT<TBlockType> const * bucket)
    {
        if (TBlockType::HeapBlockAttributes::IsSmallBlock)
        {
            return this->GetData<TBlockType>().bucketData[bucket->GetBucketIndex()];
        }
        else
        {
            Assert(TBlockType::HeapBlockAttributes::IsMediumBlock);
            return this->GetData<TBlockType>().bucketData[bucket->GetMediumBucketIndex()];
        }
    }

    template <typename TBlockType>
    struct Data
    {
        BucketData<TBlockType> bucketData[TBlockType::HeapBlockAttributes::BucketCount];
#ifdef CONCURRENT_GC_ENABLED
        TBlockType * pendingMergeNewHeapBlockList;
#endif
    };

    template <typename TBlockType> Data<TBlockType>& GetData();
    template <> Data<SmallLeafHeapBlock>& GetData<SmallLeafHeapBlock>() { return leafData; }
    template <> Data<SmallNormalHeapBlock>& GetData<SmallNormalHeapBlock>() { return normalData; }
    template <> Data<SmallFinalizableHeapBlock>& GetData<SmallFinalizableHeapBlock>() { return finalizableData; }
#ifdef RECYCLER_WRITE_BARRIER
    template <> Data<SmallNormalWithBarrierHeapBlock>& GetData<SmallNormalWithBarrierHeapBlock>() { return withBarrierData; }
    template <> Data<SmallFinalizableWithBarrierHeapBlock>& GetData<SmallFinalizableWithBarrierHeapBlock>() { return finalizableWithBarrierData; }
#endif

    template <> Data<MediumLeafHeapBlock>& GetData<MediumLeafHeapBlock>() { return mediumLeafData; }
    template <> Data<MediumNormalHeapBlock>& GetData<MediumNormalHeapBlock>() { return mediumNormalData; }
    template <> Data<MediumFinalizableHeapBlock>& GetData<MediumFinalizableHeapBlock>() { return mediumFinalizableData; }
#ifdef RECYCLER_WRITE_BARRIER
    template <> Data<MediumNormalWithBarrierHeapBlock>& GetData<MediumNormalWithBarrierHeapBlock>() { return mediumWithBarrierData; }
    template <> Data<MediumFinalizableWithBarrierHeapBlock>& GetData<MediumFinalizableWithBarrierHeapBlock>() { return mediumFinalizableWithBarrierData; }
#endif

private:
    bool IsMemProtectMode();

    Recycler * recycler;
    Data<SmallLeafHeapBlock> leafData;
    Data<SmallNormalHeapBlock> normalData;
    Data<SmallFinalizableHeapBlock> finalizableData;
#ifdef RECYCLER_WRITE_BARRIER
    Data<SmallNormalWithBarrierHeapBlock> withBarrierData;
    Data<SmallFinalizableWithBarrierHeapBlock> finalizableWithBarrierData;
#endif
    Data<MediumLeafHeapBlock> mediumLeafData;
    Data<MediumNormalHeapBlock> mediumNormalData;
    Data<MediumFinalizableHeapBlock> mediumFinalizableData;
#ifdef RECYCLER_WRITE_BARRIER
    Data<MediumNormalWithBarrierHeapBlock> mediumWithBarrierData;
    Data<MediumFinalizableWithBarrierHeapBlock> mediumFinalizableWithBarrierData;
#endif

    bool background;
    bool forceForeground;
    bool hasPendingSweepSmallHeapBlocks;
    bool hasPendingEmptyBlocks;
    bool inPartialCollect;
#ifdef PARTIAL_GC_ENABLED
    bool adjustPartialHeuristics;
    size_t lastPartialUncollectedAllocBytes;
    size_t nextPartialUncollectedAllocBytes;

    // Sweep data for partial activation heuristic
    size_t rescanRootBytes;
    size_t reuseHeapBlockCount;
    size_t reuseByteCount;

    // Partial reuse Heuristic
    size_t partialCollectSmallHeapBlockReuseMinFreeBytes;

    // Data to update unusedPartialCollectFreeBytes
    size_t partialUnusedFreeByteCount;

#if DBG
    bool partial;
#endif
#endif
};

#if defined(PARTIAL_GC_ENABLED) || defined(CONCURRENT_GC_ENABLED)
template <typename TBlockType>
TBlockType *&
RecyclerSweep::GetPendingSweepBlockList(HeapBucketT<TBlockType> const * heapBucket)
{
    return this->GetBucketData<TBlockType>(heapBucket).pendingSweepList;
}
#endif

#ifdef CONCURRENT_GC_ENABLED

template <typename TBlockType, bool pageheap>
void
RecyclerSweep::QueueEmptyHeapBlock(HeapBucketT<TBlockType> const *heapBucket, TBlockType * heapBlock)
{
    auto& bucketData = this->GetBucketData(heapBucket);
    Assert(heapBlock->heapBucket == heapBucket);
    heapBlock->BackgroundReleasePagesSweep<pageheap>(recycler);
    TBlockType * list = bucketData.pendingEmptyBlockList;
    if (list == nullptr)
    {
        Assert(bucketData.pendingEmptyBlockListTail == nullptr);
        bucketData.pendingEmptyBlockListTail = heapBlock;
        this->hasPendingEmptyBlocks = true;
    }
    heapBlock->SetNextBlock(list);
    bucketData.pendingEmptyBlockList = heapBlock;
}

template <typename TBlockType>
void
RecyclerSweep::TransferPendingEmptyHeapBlocks(HeapBucketT<TBlockType> * heapBucket)
{
    Assert(!this->IsBackground());
    Assert(!heapBucket->IsAllocationStopped());
    RECYCLER_SLOW_CHECK(heapBucket->VerifyHeapBlockCount(false));

    auto& bucketData = this->GetBucketData(heapBucket);
    Assert(bucketData.pendingSweepList == nullptr);
    TBlockType * list = bucketData.pendingEmptyBlockList;
    if (list)
    {
        TBlockType * tail = bucketData.pendingEmptyBlockListTail;
#if DBG || defined(RECYCLER_SLOW_CHECK_ENABLED)
        size_t count = 0;
        HeapBlockList::ForEach(list, [tail, &count](TBlockType * heapBlock)
        {
            Assert(heapBlock->GetAddress() == nullptr);
            Assert(heapBlock->GetSegment() == nullptr);
            Assert(heapBlock->GetNextBlock() != nullptr || heapBlock == tail);
            count++;
        });
        RECYCLER_SLOW_CHECK(heapBucket->emptyHeapBlockCount += count);
        RECYCLER_SLOW_CHECK(heapBucket->heapBlockCount -= count);
#endif


        tail->SetNextBlock(heapBucket->emptyBlockList);
        heapBucket->emptyBlockList = list;

        bucketData.pendingEmptyBlockList = nullptr;
        RECYCLER_SLOW_CHECK(heapBucket->VerifyHeapBlockCount(false));
    }
    else
    {
        Assert(bucketData.pendingEmptyBlockListTail == nullptr);
    }
}

template <typename TBlockType>
void
RecyclerSweep::SetPendingMergeNewHeapBlockList(TBlockType * heapBlockList)
{
    this->GetData<TBlockType>().pendingMergeNewHeapBlockList = heapBlockList;
}

#if DBG
template <typename TBlockType>
TBlockType *
RecyclerSweep::GetSavedNextAllocableBlockHead(HeapBucketT<TBlockType> const * heapBucket)
{
    return this->GetBucketData(heapBucket).savedNextAllocableBlockHead;
}
template <typename TBlockType>
void
RecyclerSweep::SaveNextAllocableBlockHead(HeapBucketT<TBlockType> const * heapBucket)
{
    this->GetBucketData(heapBucket).savedNextAllocableBlockHead = heapBucket->nextAllocableBlockHead;
}
#endif
#if DBG || defined(RECYCLER_SLOW_CHECK_ENABLED)
template < typename TBlockType>
size_t
RecyclerSweep::GetHeapBlockCount(HeapBucketT<TBlockType> const * heapBucket)
{
    auto& bucketData = this->GetBucketData(heapBucket);
    return HeapBlockList::Count(bucketData.pendingSweepList)
        + HeapBlockList::Count(bucketData.pendingEmptyBlockList);
}
#endif
#endif
}
