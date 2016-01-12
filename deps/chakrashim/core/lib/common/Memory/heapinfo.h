//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
namespace Memory
{
class HeapInfo
{
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    friend class ScriptMemoryDumper;
#endif
public:
    HeapInfo();
    ~HeapInfo();

    void Initialize(Recycler * recycler
#ifdef RECYCLER_PAGE_HEAP
        , PageHeapMode pageheapmode = PageHeapMode::PageHeapModeOff
        , bool captureAllocCallStack = false
        , bool captureFreeCallStack = false
#endif
        );
#if defined(PROFILE_RECYCLER_ALLOC) || defined(RECYCLER_MEMORY_VERIFY) || defined(MEMSPECT_TRACKING) || defined(ETW_MEMORY_TRACKING)
    void Initialize(Recycler * recycler, void(*trackNativeAllocCallBack)(Recycler *, void *, size_t)
#ifdef RECYCLER_PAGE_HEAP
        , PageHeapMode pageheapmode = PageHeapMode::PageHeapModeOff
        , bool captureAllocCallStack = false
        , bool captureFreeCallStack = false
#endif
        );
#endif

    void ResetMarks(ResetMarkFlags flags);
    void EnumerateObjects(ObjectInfoBits infoBits, void(*CallBackFunction)(void * address, size_t size));
#ifdef RECYCLER_PAGE_HEAP
    bool IsPageHeapEnabled() const{ return isPageHeapEnabled; }

    template <typename TBlockAttributes>
    bool IsPageHeapEnabledForBlock(const size_t objectSize);
#else
    const bool IsPageHeapEnabled() const{ return false; }
#endif

#ifdef DUMP_FRAGMENTATION_STATS
    void DumpFragmentationStats();
#endif

    template <ObjectInfoBits attributes, bool nothrow>
    char * MediumAlloc(Recycler * recycler, size_t sizeCat);

    // Small allocator
    template <ObjectInfoBits attributes, bool nothrow>
    char * RealAlloc(Recycler * recycler, size_t sizeCat);
    template <ObjectInfoBits attributes>
    bool IntegrateBlock(char * blockAddress, PageSegment * segment, Recycler * recycler, size_t sizeCat);
    template <typename SmallHeapBlockAllocatorType>
    void AddSmallAllocator(SmallHeapBlockAllocatorType * allocator, size_t sizeCat);
    template <typename SmallHeapBlockAllocatorType>
    void RemoveSmallAllocator(SmallHeapBlockAllocatorType * allocator, size_t sizeCat);
    template <ObjectInfoBits attributes, typename SmallHeapBlockAllocatorType>
    char * SmallAllocatorAlloc(Recycler * recycler, SmallHeapBlockAllocatorType * allocator, size_t sizeCat);

    // collection functions
    void ScanInitialImplicitRoots();
    void ScanNewImplicitRoots();

#if defined(PARTIAL_GC_ENABLED) || defined(CONCURRENT_GC_ENABLED)
    size_t Rescan(RescanFlags flags);
    void SweepPendingObjects(RecyclerSweep& recyclerSweep);
#endif
    void Sweep(RecyclerSweep& recyclerSweep, bool concurrent);

    template <ObjectInfoBits attributes>
    void FreeSmallObject(void* object, size_t bytes);

    template <ObjectInfoBits attributes>
    void FreeMediumObject(void* object, size_t bytes);

#ifdef PARTIAL_GC_ENABLED
    void SweepPartialReusePages(RecyclerSweep& recyclerSweep);
    void FinishPartialCollect(RecyclerSweep * recyclerSweep);
#endif
#ifdef CONCURRENT_GC_ENABLED
    void PrepareSweep();

    void TransferPendingHeapBlocks(RecyclerSweep& recyclerSweep);
    void ConcurrentTransferSweptObjects(RecyclerSweep& recyclerSweep);

#ifdef PARTIAL_GC_ENABLED
    void ConcurrentPartialTransferSweptObjects(RecyclerSweep& recyclerSweep);
#endif
#endif
    void DisposeObjects();
    void TransferDisposedObjects();

#ifdef RECYCLER_SLOW_CHECK_ENABLED
    void Check();
    template <typename TBlockType>
    static size_t Check(bool expectFull, bool expectPending, TBlockType * list, TBlockType * tail = nullptr);

    void VerifySmallHeapBlockCount();
    void VerifyLargeHeapBlockCount();
#endif

#ifdef RECYCLER_MEMORY_VERIFY
    void Verify();
#endif
#ifdef RECYCLER_VERIFY_MARK
    void VerifyMark();
#endif

public:
    static bool IsSmallObject(size_t nBytes) { return nBytes <= HeapConstants::MaxSmallObjectSize; }
    static bool IsMediumObject(size_t nBytes)
    {
#ifdef BUCKETIZE_MEDIUM_ALLOCATIONS
        return nBytes > HeapConstants::MaxSmallObjectSize && nBytes <= HeapConstants::MaxMediumObjectSize;
#else
        return false;
#endif
    }

    static bool IsSmallBlockAllocation(size_t nBytes)
    {
#if SMALLBLOCK_MEDIUM_ALLOC
        return HeapInfo::IsSmallObject(nBytes) || HeapInfo::IsMediumObject(nBytes);
#else
        return HeapInfo::IsSmallObject(nBytes);
#endif
    }

    static bool IsLargeObject(size_t nBytes)
    {
#ifdef BUCKETIZE_MEDIUM_ALLOCATIONS
        return nBytes > HeapConstants::MaxMediumObjectSize;
#else
        return nBytes > HeapConstants::MaxSmallObjectSize;
#endif
    }

    static BOOL IsAlignedSize(size_t sizeCat) { return (sizeCat != 0) && (0 == (sizeCat & HeapInfo::ObjectAlignmentMask)); }
    static BOOL IsAlignedSmallObjectSize(size_t sizeCat) { return (sizeCat != 0) && (HeapInfo::IsSmallObject(sizeCat) && (0 == (sizeCat & HeapInfo::ObjectAlignmentMask))); }
    static BOOL IsAlignedMediumObjectSize(size_t sizeCat) { return (sizeCat != 0) && (HeapInfo::IsMediumObject(sizeCat) && (0 == (sizeCat & HeapInfo::ObjectAlignmentMask))); }

    static size_t GetAlignedSize(size_t size) { return AllocSizeMath::Align(size, HeapConstants::ObjectGranularity); }
    static size_t GetAlignedSizeNoCheck(size_t size) { return Math::Align<size_t>(size, HeapConstants::ObjectGranularity); }

#ifdef BUCKETIZE_MEDIUM_ALLOCATIONS
    static size_t GetMediumObjectAlignedSize(size_t size) { return AllocSizeMath::Align(size, HeapConstants::MediumObjectGranularity); }
    static size_t GetMediumObjectAlignedSizeNoCheck(size_t size) { return Math::Align<size_t>(size, HeapConstants::MediumObjectGranularity); }
#endif

    static uint GetBucketIndex(size_t sizeCat) { Assert(IsAlignedSmallObjectSize(sizeCat)); return (uint)(sizeCat >> HeapConstants::ObjectAllocationShift) - 1; }

    template <typename TBlockAttributes>
    static uint GetObjectSizeForBucketIndex(uint bucketIndex) { return (bucketIndex + 1) << HeapConstants::ObjectAllocationShift; }

#ifdef BUCKETIZE_MEDIUM_ALLOCATIONS
    static uint GetMediumBucketIndex(size_t sizeCat) { Assert(IsAlignedMediumObjectSize(sizeCat)); return (uint)((sizeCat - HeapConstants::MaxSmallObjectSize - 1) / HeapConstants::MediumObjectGranularity); }

    template <>
    static uint GetObjectSizeForBucketIndex<MediumAllocationBlockAttributes>(uint bucketIndex)
    {
        Assert(IsMediumObject(HeapConstants::MaxSmallObjectSize + ((bucketIndex + 1) * HeapConstants::MediumObjectGranularity)));
        return HeapConstants::MaxSmallObjectSize + ((bucketIndex + 1) * HeapConstants::MediumObjectGranularity);
    }
#endif

    static BOOL IsAlignedAddress(void * address) { return (0 == (((size_t)address) & HeapInfo::ObjectAlignmentMask)); }
private:
    template <ObjectInfoBits attributes>
    typename SmallHeapBlockType<attributes, SmallAllocationBlockAttributes>::BucketType& GetBucket(size_t sizeCat);

    template<bool pageheap>
    void Sweep(RecyclerSweep& recyclerSweep, bool concurrent);

#ifdef BUCKETIZE_MEDIUM_ALLOCATIONS
#if SMALLBLOCK_MEDIUM_ALLOC
    template <ObjectInfoBits attributes>
    typename SmallHeapBlockType<attributes, MediumAllocationBlockAttributes>::BucketType& GetMediumBucket(size_t sizeCat);
#else
    LargeHeapBucket& GetMediumBucket(size_t sizeCat);
#endif
#endif

    LargeHeapBlock * AddLargeHeapBlock(size_t pageCount);

    template <typename TBlockType>
    void AppendNewHeapBlock(TBlockType * heapBlock, HeapBucketT<TBlockType> * heapBucket)
    {
        TBlockType *& list = this->GetNewHeapBlockList<TBlockType>(heapBucket);
        heapBlock->SetNextBlock(list);
        list = heapBlock;
    }
#ifdef CONCURRENT_GC_ENABLED
    template <typename TBlockType> TBlockType *& GetNewHeapBlockList(HeapBucketT<TBlockType> * heapBucket);
    template <>
    SmallLeafHeapBlock *& GetNewHeapBlockList<SmallLeafHeapBlock>(HeapBucketT<SmallLeafHeapBlock> * heapBucket)
    {
        return this->newLeafHeapBlockList;
    }
    template <>
    SmallNormalHeapBlock *& GetNewHeapBlockList<SmallNormalHeapBlock>(HeapBucketT<SmallNormalHeapBlock> * heapBucket)
    {
        return this->newNormalHeapBlockList;
    }
    template <>
    SmallFinalizableHeapBlock *& GetNewHeapBlockList<SmallFinalizableHeapBlock>(HeapBucketT<SmallFinalizableHeapBlock> * heapBucket)
    {
        // Even though we don't concurrent sweep finalizable heap block, the background thread may
        // find some partial swept block to be reused, thus modifying the heapBlockList in the background
        // so new block can't go into heapBlockList
        return this->newFinalizableHeapBlockList;
    }

#ifdef RECYCLER_WRITE_BARRIER
    template <>
    SmallNormalWithBarrierHeapBlock *& GetNewHeapBlockList<SmallNormalWithBarrierHeapBlock>(HeapBucketT<SmallNormalWithBarrierHeapBlock> * heapBucket)
    {
        return this->newNormalWithBarrierHeapBlockList;
    }

    template <>
    SmallFinalizableWithBarrierHeapBlock *& GetNewHeapBlockList<SmallFinalizableWithBarrierHeapBlock>(HeapBucketT<SmallFinalizableWithBarrierHeapBlock> * heapBucket)
    {
        return this->newFinalizableWithBarrierHeapBlockList;
    }
#endif

    template <>
    MediumLeafHeapBlock *& GetNewHeapBlockList<MediumLeafHeapBlock>(HeapBucketT<MediumLeafHeapBlock> * heapBucket)
    {
        return this->newMediumLeafHeapBlockList;
    }
    template <>
    MediumNormalHeapBlock *& GetNewHeapBlockList<MediumNormalHeapBlock>(HeapBucketT<MediumNormalHeapBlock> * heapBucket)
    {
        return this->newMediumNormalHeapBlockList;
    }
    template <>
    MediumFinalizableHeapBlock *& GetNewHeapBlockList<MediumFinalizableHeapBlock>(HeapBucketT<MediumFinalizableHeapBlock> * heapBucket)
    {
        // Even though we don't concurrent sweep finalizable heap block, the background thread may
        // find some partial swept block to be reused, thus modifying the heapBlockList in the background
        // so new block can't go into heapBlockList
        return this->newMediumFinalizableHeapBlockList;
    }

#ifdef RECYCLER_WRITE_BARRIER
    template <>
    MediumNormalWithBarrierHeapBlock *& GetNewHeapBlockList<MediumNormalWithBarrierHeapBlock>(HeapBucketT<MediumNormalWithBarrierHeapBlock> * heapBucket)
    {
        return this->newMediumNormalWithBarrierHeapBlockList;
    }

    template <>
    MediumFinalizableWithBarrierHeapBlock *& GetNewHeapBlockList<MediumFinalizableWithBarrierHeapBlock>(HeapBucketT<MediumFinalizableWithBarrierHeapBlock> * heapBucket)
    {
        return this->newMediumFinalizableWithBarrierHeapBlockList;
    }
#endif

    void SetupBackgroundSweep(RecyclerSweep& recyclerSweep);
    template<bool pageheap>
    void SweepSmallNonFinalizable(RecyclerSweep& recyclerSweep);
    void SweepLargeNonFinalizable(RecyclerSweep& recyclerSweep);
#else
    template <typename TBlockType> TBlockType *& GetNewHeapBlockList(HeapBucketT<TBlockType> * heapBucket)
    {
        return heapBucket->heapBlockList;
    }
#endif

#if DBG || defined(RECYCLER_SLOW_CHECK_ENABLED)
    size_t GetSmallHeapBlockCount(bool checkCount = false) const;
    size_t GetLargeHeapBlockCount(bool checkCount = false) const;
#endif

#if DBG
    bool AllocatorsAreEmpty();
#endif
private:
    template <typename TBlockAttributes>
    class ValidPointersMap
    {
#define USE_STATIC_VPM 1 // Disable to force generation at runtime
    private:
        static const uint rowSize = TBlockAttributes::MaxSmallObjectCount * 2;
        typedef ushort ValidPointersMapRow[rowSize];
        typedef ValidPointersMapRow ValidPointersMapTable[TBlockAttributes::BucketCount];
        typedef typename SmallHeapBlockT<TBlockAttributes>::SmallHeapBlockBitVector InvalidBitsTable[TBlockAttributes::BucketCount];
        typedef typename SmallHeapBlockT<TBlockAttributes>::BlockInfo BlockInfoMapRow[TBlockAttributes::PageCount];
        typedef BlockInfoMapRow BlockInfoMapTable[TBlockAttributes::BucketCount];

        // Architecture-dependent initialization done in ValidPointersMap/vpm.(32b|64b).h
#if USE_STATIC_VPM
        static const
#endif
            ValidPointersMapTable validPointersBuffer;

#if USE_STATIC_VPM
        static const
#endif
            BlockInfoMapTable blockInfoBuffer;

#if USE_STATIC_VPM
        static const BVUnit invalidBitsData[TBlockAttributes::BucketCount][SmallHeapBlockT<TBlockAttributes>::SmallHeapBlockBitVector::wordCount];
        static const InvalidBitsTable * const invalidBitsBuffers;
#endif

    public:
#if !USE_STATIC_VPM
        InvalidBitsTable invalidBitsBuffers;
        ValidPointersMap() { GenerateValidPointersMap(validPointersBuffer, invalidBitsBuffers, blockInfoBuffer); }
#endif
        static void GenerateValidPointersMap(ValidPointersMapTable& validTable, InvalidBitsTable& invalidTable, BlockInfoMapTable& blockInfoTable);

        const ValidPointers<TBlockAttributes> GetValidPointersForIndex(uint index) const;

        const typename SmallHeapBlockT<TBlockAttributes>::SmallHeapBlockBitVector * GetInvalidBitVector(uint index) const;
        const typename SmallHeapBlockT<TBlockAttributes>::BlockInfo * GetBlockInfo(uint index) const;

        static HRESULT GenerateValidPointersMapHeader(LPCWSTR vpmFullPath);

        static HRESULT GenerateValidPointersMapForBlockType(FILE* file);
    };

    static ValidPointersMap<SmallAllocationBlockAttributes>  smallAllocValidPointersMap;
    static ValidPointersMap<MediumAllocationBlockAttributes> mediumAllocValidPointersMap;

public:
    static HRESULT GenerateValidPointersMapHeader(LPCWSTR vpmFullPath)
    {
        return smallAllocValidPointersMap.GenerateValidPointersMapHeader(vpmFullPath);
    }

    template <typename TBlockAttributes>
    static typename SmallHeapBlockT<TBlockAttributes>::SmallHeapBlockBitVector const * GetInvalidBitVector(uint objectSize)
    {
        return smallAllocValidPointersMap.GetInvalidBitVector(GetBucketIndex(objectSize));
    }

    template <typename TBlockAttributes>
    static typename SmallHeapBlockT<TBlockAttributes>::SmallHeapBlockBitVector const * GetInvalidBitVectorForBucket(uint bucketIndex)
    {
        return smallAllocValidPointersMap.GetInvalidBitVector(bucketIndex);
    }

    template <typename TBlockAttributes>
    static typename ValidPointers<TBlockAttributes> const GetValidPointersMapForBucket(uint bucketIndex)
    {
        return smallAllocValidPointersMap.GetValidPointersForIndex(bucketIndex);
    }

    template <>
    static typename SmallHeapBlockT<MediumAllocationBlockAttributes>::SmallHeapBlockBitVector const * GetInvalidBitVector<MediumAllocationBlockAttributes>(uint objectSize)
    {
        return mediumAllocValidPointersMap.GetInvalidBitVector(GetMediumBucketIndex(objectSize));
    }

    template <>
    static typename SmallHeapBlockT<MediumAllocationBlockAttributes>::SmallHeapBlockBitVector const * GetInvalidBitVectorForBucket<MediumAllocationBlockAttributes>(uint bucketIndex)
    {
        return mediumAllocValidPointersMap.GetInvalidBitVector(bucketIndex);
    }

    template <>
    static typename ValidPointers<MediumAllocationBlockAttributes> const GetValidPointersMapForBucket<MediumAllocationBlockAttributes>(uint bucketIndex)
    {
        return mediumAllocValidPointersMap.GetValidPointersForIndex(bucketIndex);
    }

    Recycler* GetRecycler(){ return recycler; }

    template <typename TBlockAttributes>
    static typename SmallHeapBlockT<TBlockAttributes>::BlockInfo const * GetBlockInfo(uint objectSize)
    {
        return smallAllocValidPointersMap.GetBlockInfo(GetBucketIndex(objectSize));
    }

    template <>
    static typename SmallHeapBlockT<MediumAllocationBlockAttributes>::BlockInfo const * GetBlockInfo<MediumAllocationBlockAttributes>(uint objectSize)
    {
        return mediumAllocValidPointersMap.GetBlockInfo(GetMediumBucketIndex(objectSize));
    }

private:
    size_t uncollectedAllocBytes;
    size_t lastUncollectedAllocBytes;
    size_t uncollectedExternalBytes;
    uint pendingZeroPageCount;
#ifdef PARTIAL_GC_ENABLED
    size_t uncollectedNewPageCount;
    size_t unusedPartialCollectFreeBytes;
#endif

    Recycler * recycler;

#ifdef CONCURRENT_GC_ENABLED
    SmallLeafHeapBlock * newLeafHeapBlockList;
    SmallNormalHeapBlock * newNormalHeapBlockList;
    SmallFinalizableHeapBlock * newFinalizableHeapBlockList;

#ifdef RECYCLER_WRITE_BARRIER
    SmallNormalWithBarrierHeapBlock * newNormalWithBarrierHeapBlockList;
    SmallFinalizableWithBarrierHeapBlock * newFinalizableWithBarrierHeapBlockList;
#endif
#endif

#ifdef CONCURRENT_GC_ENABLED
    MediumLeafHeapBlock * newMediumLeafHeapBlockList;
    MediumNormalHeapBlock * newMediumNormalHeapBlockList;
    MediumFinalizableHeapBlock * newMediumFinalizableHeapBlockList;

#ifdef RECYCLER_WRITE_BARRIER
    MediumNormalWithBarrierHeapBlock * newMediumNormalWithBarrierHeapBlockList;
    MediumFinalizableWithBarrierHeapBlock * newMediumFinalizableWithBarrierHeapBlockList;
#endif
#endif

#ifdef RECYCLER_PAGE_HEAP
    PageHeapMode pageHeapMode;
    bool isPageHeapEnabled;
    BVStatic<HeapConstants::BucketCount> smallBlockPageHeapBucketFilter;
    BVStatic<HeapConstants::MediumBucketCount> mediumBlockPageHeapBucketFilter;
    bool captureAllocCallStack;
    bool captureFreeCallStack;
    PageHeapBlockTypeFilter pageHeapBlockType;
#endif

    HeapBucketGroup<SmallAllocationBlockAttributes> heapBuckets[HeapConstants::BucketCount];

#ifdef BUCKETIZE_MEDIUM_ALLOCATIONS
#if SMALLBLOCK_MEDIUM_ALLOC
    HeapBucketGroup<MediumAllocationBlockAttributes> mediumHeapBuckets[HeapConstants::MediumBucketCount];
#else
    LargeHeapBucket mediumHeapBuckets[HeapConstants::MediumBucketCount];
#endif
#endif
    LargeHeapBucket largeObjectBucket;

    static const size_t ObjectAlignmentMask = HeapConstants::ObjectGranularity - 1;         // 0xF
#ifdef RECYCLER_SLOW_CHECK_ENABLED
    size_t heapBlockCount[HeapBlock::BlockTypeCount];
#endif
#ifdef RECYCLER_FINALIZE_CHECK
    size_t liveFinalizableObjectCount;
    size_t newFinalizableObjectCount;
    size_t pendingDisposableObjectCount;
    void VerifyFinalize();
#endif

    friend class Recycler;
    friend class HeapBucket;
    friend class HeapBlockMap32;
    friend class LargeHeapBucket;
    template <typename TBlockType>
    friend class HeapBucketT;
    template <typename TBlockType>
    friend class SmallHeapBlockAllocator;
    template <typename TBlockAttributes>
    friend class SmallFinalizableHeapBucketT;

    template <typename TBlockAttributes>
    friend class SmallLeafHeapBucketBaseT;

    template <typename TBlockAttributes>
    friend class SmallHeapBlockT;
    template <typename TBlockAttributes>
    friend class SmallLeafHeapBlockT;
    template <typename TBlockAttributes>
    friend class SmallFinalizableHeapBlockT;
    friend class LargeHeapBlock;
    friend class RecyclerSweep;
};

template <ObjectInfoBits attributes>
typename SmallHeapBlockType<attributes, SmallAllocationBlockAttributes>::BucketType&
HeapInfo::GetBucket(size_t sizeCat)
{
    uint bucket = HeapInfo::GetBucketIndex(sizeCat);
    return this->heapBuckets[bucket].GetBucket<attributes>();
}

#ifdef BUCKETIZE_MEDIUM_ALLOCATIONS
#if SMALLBLOCK_MEDIUM_ALLOC
template <ObjectInfoBits attributes>
typename SmallHeapBlockType<attributes, MediumAllocationBlockAttributes>::BucketType&
HeapInfo::GetMediumBucket(size_t sizeCat)
{
    uint bucket = HeapInfo::GetMediumBucketIndex(sizeCat);
    return this->mediumHeapBuckets[bucket].GetBucket<attributes>();
}

#else
LargeHeapBucket&
HeapInfo::GetMediumBucket(size_t sizeCat)
{
    uint bucket = HeapInfo::GetMediumBucketIndex(sizeCat);
    return this->mediumHeapBuckets[bucket];
}
#endif
#endif

template <ObjectInfoBits attributes, bool nothrow>
__inline char *
HeapInfo::RealAlloc(Recycler * recycler, size_t sizeCat)
{
    Assert(HeapInfo::IsAlignedSmallObjectSize(sizeCat));
    auto& bucket = this->GetBucket<(ObjectInfoBits)(attributes & GetBlockTypeBitMask)>(sizeCat);
    return bucket.RealAlloc<attributes, nothrow>(recycler, sizeCat);
}

#if defined(BUCKETIZE_MEDIUM_ALLOCATIONS)
#if SMALLBLOCK_MEDIUM_ALLOC
template <ObjectInfoBits attributes, bool nothrow>
__inline char *
HeapInfo::MediumAlloc(Recycler * recycler, size_t sizeCat)
{
    auto& bucket = this->GetMediumBucket<(ObjectInfoBits)(attributes & GetBlockTypeBitMask)>(sizeCat);

    return bucket.RealAlloc<attributes, nothrow>(recycler, sizeCat);
}

#else
template <ObjectInfoBits attributes, bool nothrow>
__forceinline char *
HeapInfo::MediumAlloc(Recycler * recycler, size_t sizeCat)
{
    Assert(HeapInfo::IsAlignedMediumObjectSize(sizeCat));
    return this->GetMediumBucket<attributes>(sizeCat).Alloc<attributes, nothrow>(recycler, sizeCat);
}
#endif
#endif

template <ObjectInfoBits attributes>
__inline void
HeapInfo::FreeSmallObject(void* object, size_t sizeCat)
{
    Assert(HeapInfo::IsAlignedSmallObjectSize(sizeCat));
    return this->GetBucket<(ObjectInfoBits)(attributes & GetBlockTypeBitMask)>(sizeCat).ExplicitFree(object, sizeCat);
}

template <ObjectInfoBits attributes>
__inline void
HeapInfo::FreeMediumObject(void* object, size_t sizeCat)
{
    Assert(HeapInfo::IsAlignedMediumObjectSize(sizeCat));
    return this->GetMediumBucket<(ObjectInfoBits)(attributes & GetBlockTypeBitMask)>(sizeCat).ExplicitFree(object, sizeCat);
}

template <ObjectInfoBits attributes>
bool
HeapInfo::IntegrateBlock(char * blockAddress, PageSegment * segment, Recycler * recycler, size_t sizeCat)
{
    // We only support no bit and leaf bit right now, where we don't need to set the object info in either case
    CompileAssert(attributes == NoBit || attributes == LeafBit);
    Assert(HeapInfo::IsAlignedSmallObjectSize(sizeCat));

    return this->GetBucket<(ObjectInfoBits)(attributes & GetBlockTypeBitMask)>(sizeCat).IntegrateBlock(blockAddress, segment, recycler);
}

template <typename SmallHeapBlockAllocatorType>
void
HeapInfo::AddSmallAllocator(SmallHeapBlockAllocatorType * allocator, size_t sizeCat)
{
    Assert(HeapInfo::IsAlignedSmallObjectSize(sizeCat));
    this->GetBucket<SmallHeapBlockAllocatorType::BlockType::RequiredAttributes>(sizeCat).AddAllocator(allocator);
}

template <typename SmallHeapBlockAllocatorType>
void
HeapInfo::RemoveSmallAllocator(SmallHeapBlockAllocatorType * allocator, size_t sizeCat)
{
    Assert(HeapInfo::IsAlignedSmallObjectSize(sizeCat));
    this->GetBucket<SmallHeapBlockAllocatorType::BlockType::RequiredAttributes>(sizeCat).RemoveAllocator(allocator);
}

template <ObjectInfoBits attributes, typename SmallHeapBlockAllocatorType>
char *
HeapInfo::SmallAllocatorAlloc(Recycler * recycler, SmallHeapBlockAllocatorType * allocator, size_t sizeCat)
{
    Assert(HeapInfo::IsAlignedSmallObjectSize(sizeCat));
    CompileAssert((attributes & SmallHeapBlockAllocatorType::BlockType::RequiredAttributes) == SmallHeapBlockAllocatorType::BlockType::RequiredAttributes);

    auto& bucket = this->GetBucket<SmallHeapBlockAllocatorType::BlockType::RequiredAttributes>(sizeCat);

    // For now, SmallAllocatorAlloc is always throwing- but it's pretty easy to switch it if it's needed
    return bucket.SnailAlloc(recycler, allocator, sizeCat, attributes, /* nothrow = */ false);
}

extern template class HeapInfo::ValidPointersMap<SmallAllocationBlockAttributes>;
extern template class HeapInfo::ValidPointersMap<MediumAllocationBlockAttributes>;
}
