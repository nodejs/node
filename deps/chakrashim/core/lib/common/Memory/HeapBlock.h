//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
class ScriptMemoryDumper;

namespace Memory
{
#ifdef RECYCLER_PAGE_HEAP
enum class PageHeapBlockTypeFilter;

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
#define PageHeapVerboseTrace(flags, ...) \
if (flags.Verbose && flags.Trace.IsEnabled(Js::PageHeapPhase)) \
    { \
        Output::Print(__VA_ARGS__); \
    }
#define PageHeapTrace(flags, ...) \
if (flags.Trace.IsEnabled(Js::PageHeapPhase)) \
    { \
        Output::Print(__VA_ARGS__); \
    }
#else
#define PageHeapVerboseTrace(...)
#define PageHeapTrace(...)
#endif

#else
#define PageHeapVerboseTrace(...)
#define PageHeapTrace(...)
#endif


class Recycler;
class HeapBucket;
template <typename TBlockType> class HeapBucketT;
class  RecyclerSweep;
class MarkContext;

#ifdef DUMP_FRAGMENTATION_STATS
struct HeapBucketStats
{
    uint totalBlockCount;
    uint emptyBlockCount;
    uint finalizeBlockCount;
    uint objectCount;
    uint finalizeCount;
    uint objectByteCount;
    uint totalByteCount;
};
#endif

#if defined(PROFILE_RECYCLER_ALLOC) || defined(RECYCLER_MEMORY_VERIFY) || defined(MEMSPECT_TRACKING) || defined(RECYCLER_PERF_COUNTERS) || defined(ETW_MEMORY_TRACKING)
#define RECYCLER_TRACK_NATIVE_ALLOCATED_OBJECTS
#endif

#ifdef RECYCLER_SLOW_CHECK_ENABLED
#define RECYCLER_SLOW_CHECK(x) x
#define RECYCLER_SLOW_CHECK_IF(cond, x) if (cond) { x; }
#else
#define RECYCLER_SLOW_CHECK(x)
#define RECYCLER_SLOW_CHECK_IF(cond, x)
#endif

// ObjectInfoBits is unsigned short, but only the lower byte is stored as the object attribute
// The upper bits are used to pass other information about allocation (e.g. NoDisposeBit)
//
enum ObjectInfoBits : unsigned short
{
    // Bits that are actually stored in ObjectInfo

    NoBit                       = 0x00,    // assume an allocation is not leaf unless LeafBit is specified.
    FinalizeBit                 = 0x80,    // Indicates that the object has a finalizer
    PendingDisposeBit           = 0x40,    // Indicates that the object is pending dispose
    LeafBit                     = 0x20,    // Indicates that the object is a leaf-object (objects without this bit need to be scanned)
    TrackBit                    = 0x10,    // Indicates that the object is a TrackableObject
    ImplicitRootBit             = 0x08,
    NewTrackBit                 = 0x04,    // Tracked object is newly allocated and hasn't been process by concurrent GC
    MemoryProfilerOldObjectBit  = 0x02,
    EnumClass_1_Bit             = 0x01,    // This can be extended to add more enumerable classes (if we still have bits left)

    // Mask for above bits
    StoredObjectInfoBitMask     = 0xFF,

    // Bits that implied by the block type, and thus don't need to be stored (for small blocks)
    // Note, LeafBit is used in finalizable blocks, thus is not always implied by the block type
    // GC-TODO: FinalizeBit doesn't need to be stored since we have separate bucket for them.
    // We can move it the upper byte.

#ifdef RECYCLER_WRITE_BARRIER
    WithBarrierBit              = 0x0100,
#endif

    // Mask for above bits
    InternalObjectInfoBitMask   = 0x01FF,

    // Bits that only affect allocation behavior, not mark/sweep/etc

    ClientTrackedBit            = 0x0200,       // This allocation is client tracked
    TraceBit                    = 0x0400,

    // Additional definitions based on above

#ifdef RECYCLER_STATS
    NewFinalizeBit              = NewTrackBit,  // Use to detect if the background thread has counted the finalizable object in stats
#else
    NewFinalizeBit              = 0x00,
#endif

#ifdef RECYCLER_WRITE_BARRIER
    FinalizableWithBarrierBit   = WithBarrierBit | FinalizeBit,
#endif

    // Allocation bits
    FinalizableLeafBits         = NewFinalizeBit | FinalizeBit | LeafBit,
    FinalizableObjectBits       = NewFinalizeBit | FinalizeBit ,
#ifdef RECYCLER_WRITE_BARRIER
    FinalizableWithBarrierObjectBits = NewFinalizeBit | FinalizableWithBarrierBit,
#endif
    ClientFinalizableObjectBits = NewFinalizeBit | ClientTrackedBit | FinalizeBit,

    ClientTrackableLeafBits     = NewTrackBit | ClientTrackedBit | TrackBit | FinalizeBit | LeafBit,
    ClientTrackableObjectBits   = NewTrackBit | ClientTrackedBit | TrackBit | FinalizeBit,

    WeakReferenceEntryBits      = LeafBit,

    ImplicitRootLeafBits        = LeafBit | ImplicitRootBit,

    // Pending dispose objects should have LeafBit set and no others
    PendingDisposeObjectBits    = PendingDisposeBit | LeafBit,

#ifdef RECYCLER_WRITE_BARRIER
    GetBlockTypeBitMask = FinalizeBit | LeafBit | WithBarrierBit,
#else
    GetBlockTypeBitMask = FinalizeBit | LeafBit,
#endif

    CollectionBitMask           = LeafBit | FinalizeBit | TrackBit | NewTrackBit,  // Bits relevant to collection

    EnumClassMask               = EnumClass_1_Bit,
};


enum ResetMarkFlags
{
    ResetMarkFlags_None = 0x0,
    ResetMarkFlags_Background = 0x1,
    ResetMarkFlags_ScanImplicitRoot = 0x2,


    // For in thread GC
    ResetMarkFlags_InThread = ResetMarkFlags_None,
    ResetMarkFlags_InThreadImplicitRoots = ResetMarkFlags_None | ResetMarkFlags_ScanImplicitRoot,

    // For background GC
    ResetMarkFlags_InBackgroundThread = ResetMarkFlags_Background,
    ResetMarkFlags_InBackgroundThreadImplicitRoots = ResetMarkFlags_Background | ResetMarkFlags_ScanImplicitRoot,

    // For blocking synchronized GC
    ResetMarkFlags_Synchronized = ResetMarkFlags_None,
    ResetMarkFlags_SynchronizedImplicitRoots = ResetMarkFlags_None | ResetMarkFlags_ScanImplicitRoot,

    // For heap enumeration
    ResetMarkFlags_HeapEnumeration = ResetMarkFlags_None,
};

enum RescanFlags
{
    RescanFlags_None             = 0x0,
    RescanFlags_ResetWriteWatch  = 0x1
};

enum FindHeapObjectFlags
{
    FindHeapObjectFlags_NoFlags                     = 0x0,
    FindHeapObjectFlags_ClearedAllocators           = 0x1,   // Assumes that the allocator is already cleared
    FindHeapObjectFlags_VerifyFreeBitForAttribute   = 0x2,   // Don't recompute the free bit vector if there is no pending objects, the attributes will always be correct
    FindHeapObjectFlags_NoFreeBitVerify             = 0x4,   // No checking whether the address is free or not.
    FindHeapObjectFlags_AllowInterior               = 0x8,   // Allow finding heap objects for interior pointers.
};

template <class TBlockAttributes> class SmallNormalHeapBlockT;
template <class TBlockAttributes> class SmallLeafHeapBlockT;
template <class TBlockAttributes> class SmallFinalizableHeapBlockT;
#ifdef RECYCLER_WRITE_BARRIER
template <class TBlockAttributes> class SmallNormalWithBarrierHeapBlockT;
template <class TBlockAttributes> class SmallFinalizableWithBarrierHeapBlockT;

#define EXPLICIT_INSTANTIATE_WITH_SMALL_HEAP_BLOCK_TYPE(TemplateType) \
    template class TemplateType<SmallNormalHeapBlock>; \
    template class TemplateType<SmallLeafHeapBlock>; \
    template class TemplateType<SmallFinalizableHeapBlock>; \
    template class TemplateType<SmallNormalWithBarrierHeapBlock>; \
    template class TemplateType<SmallFinalizableWithBarrierHeapBlock>; \
    template class TemplateType<MediumNormalHeapBlock>; \
    template class TemplateType<MediumLeafHeapBlock>; \
    template class TemplateType<MediumFinalizableHeapBlock>; \
    template class TemplateType<MediumNormalWithBarrierHeapBlock>; \
    template class TemplateType<MediumFinalizableWithBarrierHeapBlock>; \

#else
#define EXPLICIT_INSTANTIATE_WITH_SMALL_HEAP_BLOCK_TYPE(TemplateType) \
    template class TemplateType<SmallNormalHeapBlock>; \
    template class TemplateType<SmallLeafHeapBlock>; \
    template class TemplateType<SmallFinalizableHeapBlock>; \
    template class TemplateType<MediumNormalHeapBlock>; \
    template class TemplateType<MediumLeafHeapBlock>; \
    template class TemplateType<MediumFinalizableHeapBlock>; \

#endif

class RecyclerHeapObjectInfo;
class HeapBlock
{
public:
    enum HeapBlockType : byte
    {
        FreeBlockType = 0,                  // Only used in HeapBlockMap.  Actual HeapBlock structures should never have this.
        SmallNormalBlockType = 1,
        SmallLeafBlockType = 2,
        SmallFinalizableBlockType = 3,
#ifdef RECYCLER_WRITE_BARRIER
        SmallNormalBlockWithBarrierType = 4,
        SmallFinalizableBlockWithBarrierType = 5,
#endif
        MediumNormalBlockType = 6,
        MediumLeafBlockType = 7,
        MediumFinalizableBlockType = 8,
#ifdef RECYCLER_WRITE_BARRIER
        MediumNormalBlockWithBarrierType = 9,
        MediumFinalizableBlockWithBarrierType = 10,
#endif
        LargeBlockType = 11,

        SmallAllocBlockTypeCount = 6,  // Actual number of types for blocks containing small allocations
        MediumAllocBlockTypeCount = 5, // Actual number of types for blocks containing medium allocations
        SmallBlockTypeCount = 11,      // Distinct block types independent of allocation size using SmallHeapBlockT

        BlockTypeCount = 12,
    };
    bool IsNormalBlock() const { return this->GetHeapBlockType() == SmallNormalBlockType || this->GetHeapBlockType() == MediumNormalBlockType; }
    bool IsLeafBlock() const { return this->GetHeapBlockType() == SmallLeafBlockType || this->GetHeapBlockType() == MediumLeafBlockType; }
    bool IsFinalizableBlock() const { return this->GetHeapBlockType() == SmallFinalizableBlockType || this->GetHeapBlockType() == MediumFinalizableBlockType; }

#ifdef RECYCLER_WRITE_BARRIER
    bool IsAnyNormalBlock() const { return IsNormalBlock() || IsNormalWriteBarrierBlock(); }
    bool IsAnyFinalizableBlock() const { return IsFinalizableBlock() || IsFinalizableWriteBarrierBlock(); }
    bool IsNormalWriteBarrierBlock() const { return this->GetHeapBlockType() == SmallNormalBlockWithBarrierType || this->GetHeapBlockType() == MediumNormalBlockWithBarrierType; }
    bool IsFinalizableWriteBarrierBlock() const { return this->GetHeapBlockType() == SmallFinalizableBlockWithBarrierType || this->GetHeapBlockType() == MediumFinalizableBlockWithBarrierType; }
#else
    bool IsAnyFinalizableBlock() const { return IsFinalizableBlock(); }
    bool IsAnyNormalBlock() const { return IsNormalBlock(); }
#endif

    bool IsLargeHeapBlock() const { return this->GetHeapBlockType() == LargeBlockType; }
    char * GetAddress() const { return address; }
    Segment * GetSegment() const { return segment; }

    template <typename TBlockAttributes>
    SmallNormalHeapBlockT<TBlockAttributes> * AsNormalBlock();

    template <typename TBlockAttributes>
    SmallLeafHeapBlockT<TBlockAttributes> * AsLeafBlock();

    template <typename TBlockAttributes>
    SmallFinalizableHeapBlockT<TBlockAttributes> * AsFinalizableBlock();
#ifdef RECYCLER_WRITE_BARRIER
    template <typename TBlockAttributes>
    SmallNormalWithBarrierHeapBlockT<TBlockAttributes> * AsNormalWriteBarrierBlock();

    template <typename TBlockAttributes>
    SmallFinalizableWithBarrierHeapBlockT<TBlockAttributes> * AsFinalizableWriteBarrierBlock();
#endif

protected:
    char * address;
    Segment * segment;
    HeapBlockType const heapBlockType;
    bool needOOMRescan;                             // Set if we OOMed while marking a particular object
#ifdef CONCURRENT_GC_ENABLED
    bool isPendingConcurrentSweep;
#endif

#ifdef RECYCLER_PAGE_HEAP
    PageHeapMode pageHeapMode;
    DWORD guardPageOldProtectFlags;
    char* guardPageAddress;
    StackBackTrace* pageHeapAllocStack;
    StackBackTrace* pageHeapFreeStack;

public:
    __inline bool InPageHeapMode() const { return pageHeapMode != PageHeapMode::PageHeapModeOff; }
    void CapturePageHeapAllocStack();
    void CapturePageHeapFreeStack();
#endif

public:
    template <typename Fn>
    bool UpdateAttributesOfMarkedObjects(MarkContext * markContext, void * objectAddress, size_t objectSize, unsigned char attributes, Fn fn);
    void SetNeedOOMRescan(Recycler * recycler);
public:
    HeapBlock(HeapBlockType heapBlockType) :
        heapBlockType(heapBlockType),
        needOOMRescan(false)
#ifdef RECYCLER_PAGE_HEAP
        , pageHeapAllocStack(nullptr), pageHeapFreeStack(nullptr)
#endif
    {
        Assert(GetHeapBlockType() <= HeapBlock::HeapBlockType::BlockTypeCount);
    }

    HeapBlockType const GetHeapBlockType() const
    {
        return (heapBlockType);
    }

    IdleDecommitPageAllocator* GetPageAllocator(Recycler* recycler);

    bool GetAndClearNeedOOMRescan()
    {
        if (this->needOOMRescan)
        {
            this->needOOMRescan = false;
            return true;
        }

        return false;
    }

#if DBG
    virtual BOOL IsFreeObject(void* objectAddress) = 0;
#endif
    virtual BOOL IsValidObject(void* objectAddress) = 0;

    virtual byte* GetRealAddressFromInterior(void* interiorAddress) = 0;
    virtual size_t GetObjectSize(void* object) = 0;
    virtual bool FindHeapObject(void* objectAddress, Recycler * recycler, FindHeapObjectFlags flags, RecyclerHeapObjectInfo& heapObject) = 0;
    virtual bool TestObjectMarkedBit(void* objectAddress) = 0;
    virtual void SetObjectMarkedBit(void* objectAddress) = 0;

#ifdef RECYCLER_VERIFY_MARK
    virtual void VerifyMark(void * objectAddress) = 0;
#endif
#ifdef PROFILE_RECYCLER_ALLOC
    virtual void * GetTrackerData(void * address) = 0;
    virtual void SetTrackerData(void * address, void * data) = 0;
#endif
#if DBG || defined(RECYCLER_STATS) || defined(RECYCLER_PAGE_HEAP)
    bool isForceSweeping;
#endif
#ifdef RECYCLER_PERF_COUNTERS
    virtual void UpdatePerfCountersOnFree() = 0;
#endif
};

enum SweepMode
{
    SweepMode_InThread,
#ifdef CONCURRENT_GC_ENABLED
    SweepMode_Concurrent,
#ifdef PARTIAL_GC_ENABLED
    SweepMode_ConcurrentPartial
#endif
#endif
};

// enum indicating the result of a sweep
enum SweepState
{
    SweepStateEmpty,                // the block is completely empty and can be released
    SweepStateSwept,                // the block is partially allocated, no object needs to be swept or finalized
    SweepStateFull,                 // the block is full, no object needs to be swept or finalized
    SweepStatePendingDispose,       // the block has object that needs to be finalized
#ifdef CONCURRENT_GC_ENABLED
    SweepStatePendingSweep,         // the block has object that needs to be swept
#endif
};

template <class TBlockAttributes>
class ValidPointers
{
public:
    ValidPointers(ushort const * validPointers);
    ushort GetInteriorAddressIndex(uint index) const;
    ushort GetAddressIndex(uint index) const;
private:
    ushort const * validPointers;
};

template <class TBlockAttributes>
class SmallHeapBlockT : public HeapBlock
{
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    friend class ScriptMemoryDumper;
#endif

    template <typename TBlockType>
    friend class SmallHeapBlockAllocator;
    friend class HeapInfo;
    friend class RecyclerSweep;

    template <typename TBlockType>
    friend class SmallNormalHeapBucketBase;

#ifdef JD_PRIVATE
    friend class HeapBlockHelper;
    friend class EXT_CLASS;
#endif

public:
    static const ushort InvalidAddressBit = 0xFFFF;

    typedef BVStatic<TBlockAttributes::BitVectorCount> SmallHeapBlockBitVector;
    struct  BlockInfo
    {
        ushort lastObjectIndexOnPage;
        ushort pageObjectCount;
    };

    bool FindImplicitRootObject(void* candidate, Recycler* recycler, RecyclerHeapObjectInfo& heapObject);

    SmallHeapBlockT* next;

    FreeObject* freeObjectList;
    FreeObject* lastFreeObjectHead;

    ValidPointers<TBlockAttributes> validPointers;
    HeapBucket * heapBucket;

    // Review: Should GetBucketIndex return a short instead of an int?
    const uint bucketIndex;
    const ushort objectSize; // size in bytes
    const ushort objectCount;

    ushort freeCount;
    ushort lastFreeCount;
    ushort markCount;

#ifdef PARTIAL_GC_ENABLED
    ushort oldFreeCount;
#endif
    bool   isInAllocator;
#if DBG
    bool   isClearedFromAllocator;

    bool   isIntegratedBlock;

    uint   lastUncollectedAllocBytes;
#endif
    SmallHeapBlockBitVector* markBits;
    SmallHeapBlockBitVector  freeBits;

#if DBG || defined(RECYCLER_STATS)
    SmallHeapBlockBitVector debugFreeBits;
#endif
#ifdef RECYCLER_MEMORY_VERIFY
    SmallHeapBlockBitVector explicitFreeBits;
#endif
    bool IsFreeBitsValid() const
    {
        return this->freeObjectList == this->lastFreeObjectHead;
    }

    PageSegment * GetPageSegment() const { return (PageSegment *)GetSegment(); }
public:
    ~SmallHeapBlockT();


#ifdef RECYCLER_WRITE_BARRIER
    bool IsWithBarrier() const;
#endif
    void RemoveFromHeapBlockMap(Recycler* recycler);
    char* GetAddress() const { return address; }
    char * GetEndAddress() const { return address + (this->GetPageCount() * AutoSystemInfo::PageSize);  }
    uint GetObjectWordCount() const { return this->objectSize / sizeof(void *); }
    uint GetPageCount() const;

    template<bool checkPageHeap=true>
    bool HasFreeObject() const
    {
#ifdef RECYCLER_PAGE_HEAP
        // in pageheap, we point freeObjectList to end of the allocable block to cheat the system.
        // but sometimes we need to know if it's really no free block or not.
        if (checkPageHeap)
        {
            if (this->pageHeapMode != PageHeapMode::PageHeapModeOff)
            {
                return false;
            }
        }
#endif
        return freeObjectList != nullptr;
    }

    bool IsInAllocator() const;

    bool HasPendingDisposeObjects();
    bool HasAnyDisposeObjects();

#if DBG
    void VerifyMarkBitVector();
    bool IsClearedFromAllocator() const;
    void SetIsClearedFromAllocator(bool value);
    void SetIsIntegratedBlock() { this->isIntegratedBlock = true; }
#endif
#ifdef RECYCLER_MEMORY_VERIFY
    void SetExplicitFreeBitForObject(void* object);
    void ClearExplicitFreeBitForObject(void* object);
#endif
#ifdef RECYCLER_STRESS
    void InduceFalsePositive(Recycler * recycler);
#endif

#ifdef DUMP_FRAGMENTATION_STATS
    void AggregateBlockStats(HeapBucketStats& stats, bool isAllocatorBlock = false, FreeObject* freeObjectList = nullptr, bool isBumpAllocated = false);
#endif

    /*
    * Quick description of the bit vectors
    *
    * The free bit vector is created by EnsureFreeBitVector. It's created by walking through the free list
    * for the heap block and setting the corresponding bit indices in the bit vector.
    *
    * The mark bit vector is more complicated. In most cases, it represents the objects that are alive (marked)
    * + the objects the are free (i.e in the free list). This is so that when we sweep, we don't bother sweeping over objects
    * that are already in the free list, we sweep over objects that were allocated and no longer alive since the last GC.
    * However, during rescan, the mark bit vector represents the objects that are actually alive. We set the marked bit
    * vector to this state before calling RescanObjects, so that we scan through only the objects that are actually alive.
    * This means that we don't rescan newly allocated objects during rescan, because rescan doesn't change add new mark bits.
    * Instead, these objects are marked after rescan during in-thread mark if they're actually alive.
    */
    SmallHeapBlockBitVector * GetMarkedBitVector() { return markBits; }
    SmallHeapBlockBitVector * GetFreeBitVector() { return &freeBits; }

    SmallHeapBlockBitVector const * GetInvalidBitVector();
    BlockInfo const * GetBlockInfo();

    ushort GetObjectBitDelta();

    static uint GetObjectBitDeltaForBucketIndex(uint bucketIndex);

    static char* GetBlockStartAddress(char* address)
    {
        uintptr_t mask = ~((TBlockAttributes::PageCount * AutoSystemInfo::PageSize) - 1);
        return (char*)((uintptr_t)address & mask);
    }

    bool IsValidBitIndex(uint bitIndex)
    {
        Assert(bitIndex < TBlockAttributes::BitVectorCount);
        return bitIndex % GetObjectBitDelta() == 0;
    }

    void MarkImplicitRoots();

    void SetNextBlock(SmallHeapBlockT * next) { this->next=next; }
    SmallHeapBlockT * GetNextBlock() const { return next; }

    uint GetObjectSize() const { return objectSize; }
    uint GetObjectCount() const { return objectCount; }
    uint GetMarkedCount() const { return markCount; }

    // Valid during sweep time
    ushort GetExpectedFreeObjectCount() const;
    uint GetExpectedFreeBytes() const;
    ushort GetExpectedSweepObjectCount() const;

#if DBG || defined(RECYCLER_STATS)
    SmallHeapBlockBitVector * GetDebugFreeBitVector() { return &debugFreeBits; }
#endif
#if DBG
    virtual BOOL IsFreeObject(void* objectAddress) override;
#endif
    virtual BOOL IsValidObject(void* objectAddress) override;
    byte* GetRealAddressFromInterior(void* interiorAddress) override sealed;
    bool TestObjectMarkedBit(void* objectAddress) override sealed;
    void SetObjectMarkedBit(void* objectAddress) override;
    virtual size_t GetObjectSize(void* object) override { return objectSize; }

#ifdef RECYCLER_PAGE_HEAP
    char * GetPageHeapObjectAddress();
#endif

    template <bool pageheap>
    uint GetMarkCountForSweep();

    template <bool pageheap>
    SweepState Sweep(RecyclerSweep& recyclerSweep, bool queuePendingSweep, bool allocable, ushort finalizeCount = 0, bool hasPendingDispose = false);

    template <bool pageheap, SweepMode mode>
    void SweepObjects(Recycler * recycler);

    uint GetAndClearLastFreeCount();
#ifdef PARTIAL_GC_ENABLED
    void ClearAllAllocBytes();      // Reset all unaccounted alloc bytes and the new alloc count
    uint GetAndClearUnaccountedAllocBytes();
    void AdjustPartialUncollectedAllocBytes(RecyclerSweep& recyclerSweep, uint const expectSweepCount);
    bool DoPartialReusePage(RecyclerSweep const& recyclerSweep, uint& expectFreeByteCount);
#if DBG || defined(RECYCLER_STATS)
    void SweepVerifyPartialBlock(Recycler * recycler);
#endif
#endif
    void TransferProcessedObjects(FreeObject * list, FreeObject * tail);

    template<bool pageheap>
    BOOL ReassignPages(Recycler * recycler);

    template<bool pageheap>
    __inline const uint GetPageHeapModePageCount() const;

#ifdef RECYCLER_PAGE_HEAP
    void ClearPageHeapState();
#endif

    template<bool pageheap>
    BOOL SetPage(__in_ecount_pagesize char * baseAddress, PageSegment * pageSegment, Recycler * recycler);

    template<bool pageheap>
    void ReleasePages(Recycler * recycler);
    template<bool pageheap>
    void ReleasePagesSweep(Recycler * recycler);
    void ReleasePagesShutdown(Recycler * recycler);
    template<bool pageheap>
    void BackgroundReleasePagesSweep(Recycler* recycler);

    void Reset();

    void EnumerateObjects(ObjectInfoBits infoBits, void (*CallBackFunction)(void * address, size_t size));

    bool IsImplicitRoot(uint objectIndex)
    {
        return (this->ObjectInfo(objectIndex) & ImplicitRootBit) != 0;
    }

#ifdef RECYCLER_SLOW_CHECK_ENABLED
    void Check(bool expectFull, bool expectPending);
#endif
#ifdef RECYCLER_PAGE_HEAP
    void VerifyPageHeapAllocation(_In_ char* allocation, PageHeapMode mode);
    void EnablePageHeap();
    void ClearPageHeap();
#endif
#ifdef RECYCLER_MEMORY_VERIFY
    void Verify(bool pendingDispose = false);
    void VerifyBumpAllocated(_In_ char * bumpAllocatedAddres);
#endif
#ifdef RECYCLER_VERIFY_MARK
    void VerifyMark();
    virtual void VerifyMark(void * objectAddress) override;
#endif
#ifdef RECYCLER_PERF_COUNTERS
    virtual void UpdatePerfCountersOnFree() override sealed;
#endif
#ifdef PROFILE_RECYCLER_ALLOC
    virtual void * GetTrackerData(void * address) override;
    virtual void SetTrackerData(void * address, void * data) override;
#endif

    static ushort GetAddressBitIndex(void * objectAddress);
    static void * GetRealAddressFromInterior(void * objectAddress, uint objectSize, byte bucketIndex);

protected:
    static size_t GetAllocPlusSize(uint objectCount);
    __inline void SetAttributes(void * address, unsigned char attributes);

    SmallHeapBlockT(HeapBucket * bucket, ushort objectSize, ushort objectCount, HeapBlockType heapBlockType);

    ushort GetAddressIndex(void * objectAddress);
    ushort GetInteriorAddressIndex(void * interorAddress);
    ushort GetObjectIndexFromBitIndex(ushort bitIndex);

    template <SweepMode mode>
    void SweepObject(Recycler * recycler, uint index, void * addr);


    void EnqueueProcessedObject(FreeObject **list, void * objectAddress, uint index);
    void EnqueueProcessedObject(FreeObject **list, FreeObject ** tail, void * objectAddress, uint index);

#ifdef RECYCLER_SLOW_CHECK_ENABLED
    template <typename TBlockType>
    bool GetFreeObjectListOnAllocatorImpl(FreeObject ** freeObjectList);
    virtual bool GetFreeObjectListOnAllocator(FreeObject ** freeObjectList) = 0;
    void CheckDebugFreeBitVector(bool isCollecting);
    void CheckFreeBitVector(bool isCollecting);
#endif

    SmallHeapBlockBitVector * EnsureFreeBitVector();
    SmallHeapBlockBitVector * BuildFreeBitVector();
    ushort BuildFreeBitVector(SmallHeapBlockBitVector * bv);

    BOOL IsInFreeObjectList(void * objectAddress);

    void ClearObjectInfoList();
    byte& ObjectInfo(uint index);

    __inline void FillFreeMemory(__in_bcount(size) void * address, size_t size);

    template <typename TBlockType>
    bool FindHeapObjectImpl(void* objectAddress, Recycler * recycler, FindHeapObjectFlags flags, RecyclerHeapObjectInfo& heapObject);
protected:
    void Init(ushort objectSize, ushort objectCount);
    void ConstructorCommon(HeapBucket * bucket, ushort objectSize, ushort objectCount, HeapBlockType heapBlockType);

    template <typename Fn>
    void ForEachAllocatedObject(Fn fn);

    template <typename Fn>
    void ForEachAllocatedObject(ObjectInfoBits attributes, Fn fn);

    template <typename Fn>
    void ScanNewImplicitRootsBase(Fn fn);

    // This is public for code readability but this
    // returns a value only on debug builds. On retail builds
    // this returns null
    Recycler * GetRecycler() const;

#if DBG
    uint GetMarkCountOnHeapBlockMap() const;
#endif

private:
#ifdef PROFILE_RECYCLER_ALLOC
    void ** GetTrackerDataArray();
#endif
};

// Declare the class templates
typedef SmallHeapBlockT<SmallAllocationBlockAttributes>  SmallHeapBlock;
typedef SmallHeapBlockT<MediumAllocationBlockAttributes> MediumHeapBlock;

extern template class SmallHeapBlockT<SmallAllocationBlockAttributes>;
extern template class SmallHeapBlockT<MediumAllocationBlockAttributes>;

extern template class ValidPointers<SmallAllocationBlockAttributes>;
extern template class ValidPointers<MediumAllocationBlockAttributes>;


class HeapBlockList
{
public:
    template <typename TBlockType, typename Fn>
    static void ForEach(TBlockType * list, Fn fn)
    {
        ForEach<TBlockType, Fn>(list, nullptr, fn);
    }

    template <typename TBlockType, typename Fn>
    static void ForEach(TBlockType * list, TBlockType * tail, Fn fn)
    {
        TBlockType * heapBlock = list;
        while (heapBlock != tail)
        {
            fn(heapBlock);
            heapBlock = heapBlock->GetNextBlock();
        }
    }

    template <typename TBlockType, typename Fn>
    static void ForEachEditing(TBlockType * list, Fn fn)
    {
        ForEachEditing<TBlockType, Fn>(list, nullptr, fn);
    };

    template <typename TBlockType, typename Fn>
    static void ForEachEditing(TBlockType * list, TBlockType * tail,  Fn fn)
    {
        TBlockType * heapBlock = list;
        while (heapBlock != tail)
        {
            TBlockType * nextBlock = heapBlock->GetNextBlock();
            fn(heapBlock);
            heapBlock = nextBlock;
        }
    };

    template <typename TBlockType>
    static size_t Count(TBlockType * list)
    {
        size_t currentHeapBlockCount = 0;
        HeapBlockList::ForEach(list, [&currentHeapBlockCount](TBlockType * heapBlock)
        {
            currentHeapBlockCount++;
        });
        return currentHeapBlockCount;
    };

    template <typename TBlockType>
    static TBlockType * Tail(TBlockType * list)
    {
        TBlockType * tail = nullptr;
        HeapBlockList::ForEach(list, [&tail](TBlockType * heapBlock)
        {
            tail = heapBlock;
        });
        return tail;
    }

#if DBG
    template <typename TBlockType>
    static bool Contains(TBlockType * block, TBlockType * list, TBlockType * tail = nullptr)
    {
        TBlockType * heapBlock = list;
        while (heapBlock != tail)
        {
            if (heapBlock == block)
            {
                return true;
            }
            heapBlock = heapBlock->GetNextBlock();
        }
        return false;
    }
#endif
};
}

