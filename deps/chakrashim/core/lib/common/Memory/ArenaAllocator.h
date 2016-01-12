//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#ifdef PROFILE_MEM
struct ArenaMemoryData;
#endif

namespace Memory
{
// Arena allocator
#define Anew(alloc,T,...) AllocatorNew(ArenaAllocator, alloc, T, __VA_ARGS__)
#define AnewZ(alloc,T,...) AllocatorNewZ(ArenaAllocator, alloc, T, __VA_ARGS__)
#define AnewPlus(alloc, size, T, ...) AllocatorNewPlus(ArenaAllocator, alloc, size, T, __VA_ARGS__)
#define AnewPlusZ(alloc, size, T, ...) AllocatorNewPlusZ(ArenaAllocator, alloc, size, T, __VA_ARGS__)
#define AnewStruct(alloc,T) AllocatorNewStruct(ArenaAllocator, alloc, T)
#define AnewStructZ(alloc,T) AllocatorNewStructZ(ArenaAllocator, alloc, T)
#define AnewStructPlus(alloc, size, T) AllocatorNewStructPlus(ArenaAllocator, alloc, size, T)
#define AnewArray(alloc, T, count) AllocatorNewArray(ArenaAllocator, alloc, T, count)
#define AnewArrayZ(alloc, T, count) AllocatorNewArrayZ(ArenaAllocator, alloc, T, count)
#define Adelete(alloc, obj) AllocatorDelete(ArenaAllocator, alloc, obj)
#define AdeletePlus(alloc, size, obj) AllocatorDeletePlus(ArenaAllocator, alloc, size, obj)
#define AdeleteArray(alloc, count, obj) AllocatorDeleteArray(ArenaAllocator, alloc, count, obj)


#define AnewNoThrow(alloc,T,...) AllocatorNewNoThrow(ArenaAllocator, alloc, T, __VA_ARGS__)
#define AnewNoThrowZ(alloc,T,...) AllocatorNewNoThrowZ(ArenaAllocator, alloc, T, __VA_ARGS__)
#define AnewNoThrowPlus(alloc, size, T, ...) AllocatorNewNoThrowPlus(ArenaAllocator, alloc, size, T, __VA_ARGS__)
#define AnewNoThrowPlusZ(alloc, size, T, ...) AllocatorNewNoThrowPlusZ(ArenaAllocator, alloc, size, T, __VA_ARGS__)
#define AnewNoThrowStruct(alloc,T) AllocatorNewNoThrowStruct(ArenaAllocator, alloc, T)
#define AnewNoThrowStructZ(alloc,T) AllocatorNewNoThrowStructZ(ArenaAllocator, alloc, T)
#define AnewNoThrowArray(alloc, T, count) AllocatorNewNoThrowArray(ArenaAllocator, alloc, T, count)
#define AnewNoThrowArrayZ(alloc, T, count) AllocatorNewNoThrowArrayZ(ArenaAllocator, alloc, T, count)


#define JitAnew(alloc,T,...) AllocatorNew(JitArenaAllocator, alloc, T, __VA_ARGS__)
#define JitAnewZ(alloc,T,...) AllocatorNewZ(JitArenaAllocator, alloc, T, __VA_ARGS__)
#define JitAnewPlus(alloc, size, T, ...) AllocatorNewPlus(JitArenaAllocator, alloc, size, T, __VA_ARGS__)
#define JitAnewPlusZ(alloc, size, T, ...) AllocatorNewPlusZ(JitArenaAllocator, alloc, size, T, __VA_ARGS__)
#define JitAnewStruct(alloc,T) AllocatorNewStruct(JitArenaAllocator, alloc, T)
#define JitAnewStructZ(alloc,T) AllocatorNewStructZ(JitArenaAllocator, alloc, T)
#define JitAnewStructPlus(alloc, size, T) AllocatorNewStructPlus(JitArenaAllocator, alloc, size, T)
#define JitAnewArray(alloc, T, count) AllocatorNewArray(JitArenaAllocator, alloc, T, count)
#define JitAnewArrayZ(alloc, T, count) AllocatorNewArrayZ(JitArenaAllocator, alloc, T, count)
#define JitAdelete(alloc, obj) AllocatorDelete(JitArenaAllocator, alloc, obj)
#define JitAdeletePlus(alloc, size, obj) AllocatorDeletePlus(JitArenaAllocator, alloc, size, obj)
#define JitAdeleteArray(alloc, count, obj) AllocatorDeleteArray(JitArenaAllocator, alloc, count, obj)


#define JitAnewNoThrow(alloc,T,...) AllocatorNewNoThrow(JitArenaAllocator, alloc, T, __VA_ARGS__)
#define JitAnewNoThrowZ(alloc,T,...) AllocatorNewNoThrowZ(JitArenaAllocator, alloc, T, __VA_ARGS__)
#define JitAnewNoThrowPlus(alloc, size, T, ...) AllocatorNewNoThrowPlus(JitArenaAllocator, alloc, size, T, __VA_ARGS__)
#define JitAnewNoThrowPlusZ(alloc, size, T, ...) AllocatorNewNoThrowPlusZ(JitArenaAllocator, alloc, size, T, __VA_ARGS__)
#define JitAnewNoThrowStruct(alloc,T) AllocatorNewNoThrowStruct(JitArenaAllocator, alloc, T)
#define JitAnewNoThrowArray(alloc, T, count) AllocatorNewNoThrowArray(JitArenaAllocator, alloc, T, count)
#define JitAnewNoThrowArrayZ(alloc, T, count) AllocatorNewNoThrowArrayZ(JitArenaAllocator, alloc, T, count)


struct BigBlock;
struct ArenaMemoryBlock
{
    union
    {
        ArenaMemoryBlock * next;
        BigBlock * nextBigBlock;
    };
    size_t nbytes;

    char * GetBytes() const
    {
        return ((char *)this) + sizeof(ArenaMemoryBlock);
    }
};

struct BigBlock : public ArenaMemoryBlock
{
public:
    PageAllocation * allocation;
    size_t currentByte;

    char * GetBytes() const
    {
        return ((char *)this) + sizeof(BigBlock);
    }
};


#define ASSERT_THREAD() AssertMsg(this->pageAllocator->ValidThreadAccess(), "Arena allocation should only be used by a single thread")

// Basic data layout of arena allocators. This data should be all that is needed
// to perform operations that traverse all allocated memory in the arena:
// the recycler used this to mark through registered arenas and inline cache
// allocator uses this to zero out allocated memory.
class ArenaData
{
protected:
    ArenaData(PageAllocator * pageAllocator);

protected:
    BigBlock * bigBlocks;
    BigBlock * fullBlocks;
    ArenaMemoryBlock * mallocBlocks;
    PageAllocator * pageAllocator;
    char * cacheBlockCurrent;
    bool lockBlockList;

public:
    BigBlock* GetBigBlocks(bool background)
    {
        if (!background)
        {
            UpdateCacheBlock();
        }
        return bigBlocks;
    }
    BigBlock* GetFullBlocks() { return fullBlocks; }
    ArenaMemoryBlock * GetMemoryBlocks() { return mallocBlocks; }
    PageAllocator * GetPageAllocator() const
    {
        return pageAllocator;
    }

    bool IsBlockListLocked() { return lockBlockList; }
    void SetLockBlockList(bool lock) { lockBlockList = lock; }

protected:
    void UpdateCacheBlock() const;
};

// Implements most of memory management operations over ArenaData.
// The TFreeListPolicy handles free-listing for "small objects". There
// is no support for free-listing for "large objects".
#if defined(_M_X64_OR_ARM64)
// Some data structures such as jmp_buf expect to be 16 byte aligned on AMD64.
template <class TFreeListPolicy, size_t ObjectAlignmentBitShiftArg = 4, bool RequireObjectAlignment = false, size_t MaxObjectSize = 0>
#else
template <class TFreeListPolicy, size_t ObjectAlignmentBitShiftArg = 3, bool RequireObjectAlignment = false, size_t MaxObjectSize = 0>
#endif
class ArenaAllocatorBase : public Allocator, public ArenaData
{
private:
    char * cacheBlockEnd;
    size_t largestHole;
    uint blockState;        // 0 = no block, 1 = one big block, other more then one big block or have malloc blocks

#ifdef PROFILE_MEM
    LPCWSTR name;
#endif

#ifdef PROFILE_MEM
    struct ArenaMemoryData * memoryData;
#endif

public:
    static const size_t ObjectAlignmentBitShift = ObjectAlignmentBitShiftArg;
    static const size_t ObjectAlignment = 1 << ObjectAlignmentBitShift;
    static const size_t ObjectAlignmentMask = ObjectAlignment - 1;
    static const bool FakeZeroLengthArray = true;
    static const size_t MaxSmallObjectSize = 1024;

    ArenaAllocatorBase(__in LPCWSTR name, PageAllocator * pageAllocator, void (*outOfMemoryFunc)(), void (*recoverMemoryFunc)() = JsUtil::ExternalApi::RecoverUnusedMemory);
    ~ArenaAllocatorBase();

    void Reset()
    {
        ASSERT_THREAD();
        Assert(!lockBlockList);

        freeList = TFreeListPolicy::Reset(freeList);
#ifdef PROFILE_MEM
        LogReset();
#endif

        ArenaMemoryTracking::ReportFreeAll(this);

        if (this->blockState == 1)
        {
            Assert(this->bigBlocks != nullptr && this->fullBlocks == nullptr && this->mallocBlocks == nullptr && this->bigBlocks->nextBigBlock == nullptr);
            Assert(this->largestHole == 0);
            Assert(cacheBlockEnd == bigBlocks->GetBytes() + bigBlocks->nbytes);
            Assert(bigBlocks->GetBytes() <= cacheBlockCurrent && cacheBlockCurrent <= cacheBlockEnd);
            cacheBlockCurrent = bigBlocks->GetBytes();
#ifdef PROFILE_MEM
            LogRealAlloc(bigBlocks->allocation->GetSize() + sizeof(PageAllocation));
#endif
            return;
        }
        FullReset();
    }

    void Move(ArenaAllocatorBase *srcAllocator);

    void Clear()
    {
        ASSERT_THREAD();
        Assert(!lockBlockList);
        ArenaMemoryTracking::ReportFreeAll(this);

        freeList = TFreeListPolicy::Reset(freeList);
#ifdef ARENA_ALLOCATOR_FREE_LIST_SIZE
        this->freeListSize = 0;
#endif
        ReleaseMemory();
        this->cacheBlockCurrent = nullptr;
        this->cacheBlockEnd = nullptr;
        this->bigBlocks = nullptr;
        this->fullBlocks = nullptr;
        this->largestHole = 0;
        this->mallocBlocks = nullptr;
        this->blockState = 0;
    }

    size_t AllocatedSize();     // amount of memory allocated
    size_t Size();              // amount of allocated memory is used.

    size_t FreeListSize()
    {
#ifdef ARENA_ALLOCATOR_FREE_LIST_SIZE
        return this->freeListSize;
#else
        return 0;
#endif
    }

    static size_t GetAlignedSize(size_t size) { return AllocSizeMath::Align(size, ArenaAllocatorBase::ObjectAlignment); }

    char * AllocInternal(size_t requestedBytes);

    char* Realloc(void* buffer, size_t existingBytes, size_t requestedBytes);
    void Free(void * buffer, size_t byteSize);
#ifdef TRACK_ALLOC
    // Doesn't support tracking information, dummy implementation
    ArenaAllocatorBase * TrackAllocInfo(TrackAllocData const& data) { return this; }
    void ClearTrackAllocInfo(TrackAllocData* data = nullptr) {}
#endif

protected:
    char * RealAlloc(size_t nbytes);
    __forceinline char * RealAllocInlined(size_t nbytes);
private:
#ifdef PROFILE_MEM
    void LogBegin();
    void LogReset();
    void LogEnd();
    void LogAlloc(size_t requestedBytes, size_t allocateBytes);
    void LogRealAlloc(size_t size);
#endif

    static size_t AllocatedSize(ArenaMemoryBlock * blockList);
    static size_t Size(BigBlock * blockList);
    void FullReset();
    void SetCacheBlock(BigBlock * cacheBlock);
    template <bool DoRecoverMemory> char * AllocFromHeap(size_t nbytes);
    void ReleaseMemory();
    void ReleasePageMemory();
    void ReleaseHeapMemory();
    char * SnailAlloc(size_t nbytes);
    BigBlock * AddBigBlock(size_t pages);

#ifdef ARENA_ALLOCATOR_FREE_LIST_SIZE
    size_t freeListSize;
#endif
    void * freeList;

#ifdef PROFILE_MEM
    void LogFree(size_t size);
    void LogReuse(size_t size);
#endif
};


// Implements free-listing in-place. Bucketizes allocation sizes, there is a free list
// per bucket. The freed memory is treated as nodes in the lists.
class InPlaceFreeListPolicy
{
private:
    // Free list support;
    struct FreeObject
    {
        FreeObject * next;
    };

private:
    static const uint buckets =
        ArenaAllocatorBase<InPlaceFreeListPolicy>::MaxSmallObjectSize >> ArenaAllocatorBase<InPlaceFreeListPolicy>::ObjectAlignmentBitShift;

public:
#ifdef DBG
    static const unsigned char DbgFreeMemFill = DbgMemFill;
#endif
    static void * New(ArenaAllocatorBase<InPlaceFreeListPolicy> * allocator);
    static void * Allocate(void * policy, size_t size);
    static void * Free(void * policy, void * object, size_t size);
    static void * Reset(void * policy);
    static void PrepareFreeObject(__out_bcount(size) void * object, _In_ size_t size)
    {
#ifdef ARENA_MEMORY_VERIFY
        memset(object, InPlaceFreeListPolicy::DbgFreeMemFill, size);
#endif
    }
#ifdef ARENA_MEMORY_VERIFY
    static void VerifyFreeObjectIsFreeMemFilled(void * object, size_t size);
#endif
    static void Release(void * policy) {}
};

// Implements free-listing in separate memory. Bucketizes allocation sizes, there is a free list
// per bucket. Space for the free lists is allocated from the heap. This is used by
// InlineCacheAllocator to quickly zero out the entire arena w/o loosing the free lists.
class StandAloneFreeListPolicy
{

private:
    struct FreeObjectListEntry
    {
        void * object;
        uint next;
    };

    uint allocated;
    uint used;
    uint freeList;
    uint* freeObjectLists;
    FreeObjectListEntry* entries;

    static const uint buckets =
        ArenaAllocatorBase<StandAloneFreeListPolicy>::MaxSmallObjectSize >> ArenaAllocatorBase<StandAloneFreeListPolicy>::ObjectAlignmentBitShift;
    static const uint InitialEntries = 64;
    static const uint MaxEntriesGrowth = 1024;

    static StandAloneFreeListPolicy * NewInternal(uint entriesPerBucket);
    static bool TryEnsureFreeListEntry(StandAloneFreeListPolicy *& _this);
    static uint GetPlusSize(const StandAloneFreeListPolicy * policy)
    {
        return buckets * sizeof(uint) + policy->allocated * sizeof(FreeObjectListEntry);
    }

public:
#ifdef DBG
    // TODO: Consider making DbgFreeMemFill == DbgFill, now that we have ArenaAllocatorBase properly handling filling when free-listing and asserting debug fill at re-allocation.
    static const char DbgFreeMemFill = 0x0;
#endif
    static void * New(ArenaAllocatorBase<StandAloneFreeListPolicy> * allocator);
    static void * Allocate(void * policy, size_t size);
    static void * Free(void * policy, void * object, size_t size);
    static void * Reset(void * policy);
    static void PrepareFreeObject(_Out_writes_bytes_all_(size) void * object, _In_ size_t size)
    {
#ifdef ARENA_MEMORY_VERIFY
        memset(object, StandAloneFreeListPolicy::DbgFreeMemFill, size);
#endif
    }
#ifdef ARENA_MEMORY_VERIFY
    static void VerifyFreeObjectIsFreeMemFilled(void * object, size_t size);
#endif
    static void Release(void * policy);
};

#define ARENA_FAULTINJECT_MEMORY(name, size) { \
    if (outOfMemoryFunc) \
    { \
        FAULTINJECT_MEMORY_THROW(name, size); \
    } \
    else \
    { \
        FAULTINJECT_MEMORY_NOTHROW(name, size); \
    } \
}

// This allocator by default on OOM makes an attempt to recover memory from Recycler and further throws if that doesn't help the allocation.
class ArenaAllocator : public ArenaAllocatorBase<InPlaceFreeListPolicy>
{
public:
    ArenaAllocator(__in LPCWSTR name, PageAllocator * pageAllocator, void (*outOfMemoryFunc)(), void (*recoverMemoryFunc)() = JsUtil::ExternalApi::RecoverUnusedMemory) :
        ArenaAllocatorBase<InPlaceFreeListPolicy>(name, pageAllocator, outOfMemoryFunc, recoverMemoryFunc)
    {
    }

    __forceinline
    char * Alloc(size_t requestedBytes)
    {
        return AllocInternal(requestedBytes);
    }

    char * AllocZero(size_t nbytes)
    {
        char * buffer = Alloc(nbytes);
        memset(buffer, 0, nbytes);
#if DBG
        // Since we successfully allocated, we shouldn't have integer overflow here
        memset(buffer + nbytes, 0, Math::Align(nbytes, ArenaAllocatorBase::ObjectAlignment) - nbytes);
#endif
        return buffer;
    }

    char * AllocLeaf(size_t requestedBytes)
    {
        // Leaf allocation is not meaningful here, but needed by Allocator-templatized classes that may call one of the Leaf versions of AllocatorNew
        return Alloc(requestedBytes);
    }

    char * NoThrowAlloc(size_t requestedBytes)
    {
        void (*tempOutOfMemoryFunc)() = outOfMemoryFunc;
        outOfMemoryFunc = nullptr;
        char * buffer = AllocInternal(requestedBytes);
        outOfMemoryFunc = tempOutOfMemoryFunc;
        return buffer;
    }

    char * NoThrowAllocZero(size_t requestedBytes)
    {
        char * buffer = NoThrowAlloc(requestedBytes);
        if (buffer != nullptr)
        {
            memset(buffer, 0, requestedBytes);
        }
        return buffer;
    }

    char * NoThrowNoRecoveryAlloc(size_t requestedBytes)
    {
        void (*tempRecoverMemoryFunc)() = recoverMemoryFunc;
        recoverMemoryFunc = nullptr;
        char * buffer = NoThrowAlloc(requestedBytes);
        recoverMemoryFunc = tempRecoverMemoryFunc;
        return buffer;
    }

    char * NoThrowNoRecoveryAllocZero(size_t requestedBytes)
    {
        char * buffer = NoThrowNoRecoveryAlloc(requestedBytes);
        if (buffer != nullptr)
        {
            memset(buffer, 0, requestedBytes);
        }
        return buffer;
    }
};

class JitArenaAllocator : public ArenaAllocator
{
    // The only difference between ArenaAllocator and the JitArenaAllocator is it has fast path of anything of size BVSparseNode (16 bytes)
    // Throughput improvement in the backend is substantial with this freeList.

private:
    BVSparseNode *bvFreeList;

public:

    JitArenaAllocator(__in LPCWSTR name, PageAllocator * pageAllocator, void(*outOfMemoryFunc)(), void(*recoverMemoryFunc)() = JsUtil::ExternalApi::RecoverUnusedMemory) :
        bvFreeList(nullptr), ArenaAllocator(name, pageAllocator, outOfMemoryFunc, recoverMemoryFunc)
    {
    }

    char * Alloc(size_t requestedBytes)
    {
        // Fast path
        if (sizeof(BVSparseNode) == requestedBytes)
        {
            AssertMsg(Math::Align(requestedBytes, ArenaAllocatorBase::ObjectAlignment) == requestedBytes, "Assert for Perf, T should always be aligned");
            // Fast path for BVSparseNode allocation
            if (bvFreeList)
            {
                BVSparseNode *node = bvFreeList;
                bvFreeList = bvFreeList->next;
                return (char*)node;
            }

            // If the free list is empty, then do the allocation right away for the BVSparseNode size.
            // You could call ArenaAllocator::Alloc here, but direct RealAlloc avoids unnecessary checks.
            return ArenaAllocatorBase::RealAllocInlined(requestedBytes);
        }
        return ArenaAllocator::Alloc(requestedBytes);

    }

    void Free(void * buffer, size_t byteSize)
    {
        return FreeInline(buffer, byteSize);
    }

    __forceinline void FreeInline(void * buffer, size_t byteSize)
    {
        if (sizeof(BVSparseNode) == byteSize)
        {
            //FastPath
            ((BVSparseNode*)buffer)->next = bvFreeList;
            bvFreeList = (BVSparseNode*)buffer;
            return;
        }
        return ArenaAllocator::Free(buffer, byteSize);
    }

    char * AllocZero(size_t nbytes)
    {
        return ArenaAllocator::AllocZero(nbytes);
    }

    char * AllocLeaf(size_t requestedBytes)
    {
        return ArenaAllocator::AllocLeaf(requestedBytes);
    }

    char * NoThrowAlloc(size_t requestedBytes)
    {
        return ArenaAllocator::NoThrowAlloc(requestedBytes);
    }

    char * NoThrowAllocZero(size_t requestedBytes)
    {
        return ArenaAllocator::NoThrowAllocZero(requestedBytes);
    }

    void Reset()
    {
        bvFreeList = nullptr;
        ArenaAllocator::Reset();
    }

    void Clear()
    {
        bvFreeList = nullptr;
        ArenaAllocator::Clear();
    }
};

// This allocator by default on OOM does not attempt to recover memory from Recycler, just throws OOM.
class NoRecoverMemoryJitArenaAllocator : public JitArenaAllocator
{
public:
    NoRecoverMemoryJitArenaAllocator(__in LPCWSTR name, PageAllocator * pageAllocator, void(*outOfMemoryFunc)()) :
        JitArenaAllocator(name, pageAllocator, outOfMemoryFunc, NULL)
    {
    }
};


// This allocator by default on OOM does not attempt to recover memory from Recycler, just throws OOM.
class NoRecoverMemoryArenaAllocator : public ArenaAllocator
{
public:
    NoRecoverMemoryArenaAllocator(__in LPCWSTR name, PageAllocator * pageAllocator, void (*outOfMemoryFunc)()) :
        ArenaAllocator(name, pageAllocator, outOfMemoryFunc, NULL)
    {
    }
};

#define InlineCacheAuxSlotTypeTag 4
#define MinPolymorphicInlineCacheSize 4
#define MaxPolymorphicInlineCacheSize 32

#ifdef PERSISTENT_INLINE_CACHES

class InlineCacheAllocatorInfo
{
public:
    struct CacheLayout
    {
        intptr weakRefs[3];
        intptr strongRef;
    };

    struct FreeObject
    {
        intptr blankSlots[3];
        FreeObject * next;
    };

    CompileAssert(sizeof(CacheLayout) == sizeof(FreeObject));
    CompileAssert(offsetof(CacheLayout, strongRef) == offsetof(FreeObject, next));

#if defined(_M_X64_OR_ARM64)
    CompileAssert(sizeof(CacheLayout) == 32);
    static const size_t ObjectAlignmentBitShift = 5;
#else
    CompileAssert(sizeof(CacheLayout) == 16);
    static const size_t ObjectAlignmentBitShift = 4;
#endif

    static const size_t ObjectAlignment = 1 << ObjectAlignmentBitShift;
    static const size_t MaxObjectSize = MaxPolymorphicInlineCacheSize * sizeof(CacheLayout);
};


#define InlineCacheFreeListTag 0x01
#define InlineCacheAllocatorTraits InlineCacheFreeListPolicy, InlineCacheAllocatorInfo::ObjectAlignmentBitShift, true, InlineCacheAllocatorInfo::MaxObjectSize

class InlineCacheFreeListPolicy : public InlineCacheAllocatorInfo
{
public:
#ifdef DBG
    static const unsigned char DbgFreeMemFill = DbgMemFill;
#endif
    static void * New(ArenaAllocatorBase<InlineCacheAllocatorTraits> * allocator);
    static void * Allocate(void * policy, size_t size);
    static void * Free(void * policy, void * object, size_t size);
    static void * Reset(void * policy);
    static void Release(void * policy);
    static void PrepareFreeObject(_Out_writes_bytes_all_(size) void * object, _In_ size_t size)
    {
#ifdef ARENA_MEMORY_VERIFY
        // In debug builds if we're verifying arena memory to avoid "use after free" problems, we want to fill the whole object with the debug pattern here.
        // There is a very subtle point here.  Inline caches can be allocated and freed in batches. This happens commonly when a PolymorphicInlineCache grows,
        // frees up its old array of inline caches, and allocates a bigger one.  ArenaAllocatorBase::AllocInternal when allocating an object from the free list
        // will verify that the entire object - not just its first sizeof(InlineCache) worth of bytes - is filled with the debug pattern.
        memset(object, StandAloneFreeListPolicy::DbgFreeMemFill, size);
#else
        // On the other hand, in retail builds when we don't do arena memory validation we want to zero out the whole object, so that during every subsequent garbage collection
        // we don't try to trace pointers from freed objects (inside ClearCacheIfHasDeadWeakRefs) and check if they are still reachable.  Note that in ClearCacheIfHasDeadWeakRefs
        // we cannot distinguish between live inline caches and portions of free objects.  That's again because inline caches may be allocated and freed in batches, in which case
        // only the first cache in the batch gets the free object's next pointer tag.  The rest of the batch is indistinguishable from a batch of live caches.  Hence, we scan them
        // all for pointers to unreachable objects, and it makes good sense to zero these bytes out, to avoid unnecessary Recycler::IsObjectMarked calls.
        memset(object, NULL, size);
#endif
    }
#ifdef ARENA_MEMORY_VERIFY
    static void VerifyFreeObjectIsFreeMemFilled(void * object, size_t size);
#endif


private:
    static const uint bucketCount = MaxObjectSize >> ObjectAlignmentBitShift;

    FreeObject* freeListBuckets[bucketCount];

    static InlineCacheFreeListPolicy * NewInternal();

    InlineCacheFreeListPolicy();

    bool AreFreeListBucketsEmpty();
};

class InlineCacheAllocator : public InlineCacheAllocatorInfo, public ArenaAllocatorBase<InlineCacheAllocatorTraits>
{
#ifdef POLY_INLINE_CACHE_SIZE_STATS
private:
    size_t polyCacheAllocSize;
#endif

public:
    // Zeroing and freeing w/o leaking is not implemented for large objects
    CompileAssert(MaxObjectSize <= MaxSmallObjectSize);

    InlineCacheAllocator(__in LPCWSTR name, PageAllocator * pageAllocator, void(*outOfMemoryFunc)(), void(*recoverMemoryFunc)() = JsUtil::ExternalApi::RecoverUnusedMemory) :
        ArenaAllocatorBase<InlineCacheAllocatorTraits>(name, pageAllocator, outOfMemoryFunc, recoverMemoryFunc)
#ifdef POLY_INLINE_CACHE_SIZE_STATS
        , polyCacheAllocSize(0)
#endif
    {}

    char * Alloc(size_t requestedBytes)
    {
        return AllocInternal(requestedBytes);
    }

    char * AllocZero(size_t nbytes)
    {
        char * buffer = Alloc(nbytes);
        memset(buffer, 0, nbytes);
#if DBG
        // Since we successfully allocated, we shouldn't have integer overflow here
        memset(buffer + nbytes, 0, Math::Align(nbytes, ArenaAllocatorBase::ObjectAlignment) - nbytes);
#endif
        return buffer;
    }

#if DBG
    bool IsAllZero();
#endif
    void ZeroAll();

    bool IsDeadWeakRef(Recycler* recycler, void* ptr);
    bool CacheHasDeadWeakRefs(Recycler* recycler, CacheLayout* cache);
    bool HasNoDeadWeakRefs(Recycler* recycler);
    void ClearCacheIfHasDeadWeakRefs(Recycler* recycler, CacheLayout* cache);
    void ClearCachesWithDeadWeakRefs(Recycler* recycler);

#ifdef POLY_INLINE_CACHE_SIZE_STATS
    size_t GetPolyInlineCacheSize() { return this->polyCacheAllocSize; }
    void LogPolyCacheAlloc(size_t size) { this->polyCacheAllocSize += size;  }
    void LogPolyCacheFree(size_t size) { this->polyCacheAllocSize -= size; }
#endif
};

#else

#define InlineCacheAllocatorTraits StandAloneFreeListPolicy, InlineCacheAllocatorInfo::ObjectAlignmentBitShift, true, InlineCacheAllocatorInfo::MaxObjectSize

class InlineCacheAllocator : public ArenaAllocatorBase<InlineCacheAllocatorTraits>
{
public:
    struct CacheLayout
    {
        intptr weakRefs[3];
        intptr strongRef;
    };

#ifdef POLY_INLINE_CACHE_SIZE_STATS
private:
    size_t polyCacheAllocSize;
#endif

public:
    InlineCacheAllocator(__in LPCWSTR name, PageAllocator * pageAllocator, void(*outOfMemoryFunc)()) :
        ArenaAllocatorBase<InlineCacheAllocatorTraits>(name, pageAllocator, outOfMemoryFunc) {}

    char * Alloc(size_t requestedBytes)
    {
        return AllocInternal(requestedBytes);
    }

    char * AllocZero(size_t nbytes)
    {
        char * buffer = Alloc(nbytes);
        memset(buffer, 0, nbytes);
#if DBG
        // Since we successfully allocated, we shouldn't have integer overflow here
        memset(buffer + nbytes, 0, Math::Align(nbytes, ArenaAllocatorBase::ObjectAlignment) - nbytes);
#endif
        return buffer;
    }

#if DBG
    bool IsAllZero();
#endif
    void ZeroAll();

#ifdef POLY_INLINE_CACHE_SIZE_STATS
    size_t GetPolyInlineCacheSize() { return this->polyCacheAllocSize; }
    void LogPolyCacheAlloc(size_t size) { this->polyCacheAllocSize += size;  }
    void LogPolyCacheFree(size_t size) { this->polyCacheAllocSize -= size; }
#endif
};

#endif

class IsInstInlineCacheAllocatorInfo
{
public:
    struct CacheLayout
    {
        char bytes[4 * sizeof(intptr)];
    };

#if _M_X64 || _M_ARM64
    CompileAssert(sizeof(CacheLayout) == 32);
    static const size_t ObjectAlignmentBitShift = 5;
#else
    CompileAssert(sizeof(CacheLayout) == 16);
    static const size_t ObjectAlignmentBitShift = 4;
#endif

    static const size_t ObjectAlignment = 1 << ObjectAlignmentBitShift;
    static const size_t MaxObjectSize = sizeof(CacheLayout);
};


#define IsInstInlineCacheAllocatorTraits StandAloneFreeListPolicy

class IsInstInlineCacheAllocator : public IsInstInlineCacheAllocatorInfo, public ArenaAllocatorBase<IsInstInlineCacheAllocatorTraits>
{

#ifdef POLY_INLINE_CACHE_SIZE_STATS
private:
    size_t polyCacheAllocSize;
#endif

public:
    IsInstInlineCacheAllocator(__in LPCWSTR name, PageAllocator * pageAllocator, void(*outOfMemoryFunc)()) :
        ArenaAllocatorBase<IsInstInlineCacheAllocatorTraits>(name, pageAllocator, outOfMemoryFunc) {}

    char * Alloc(size_t requestedBytes)
    {
        return AllocInternal(requestedBytes);
    }

    char * AllocZero(size_t nbytes)
    {
        char * buffer = Alloc(nbytes);
        memset(buffer, 0, nbytes);
#if DBG
        // Since we successfully allocated, we shouldn't have integer overflow here
        memset(buffer + nbytes, 0, Math::Align(nbytes, ArenaAllocatorBase::ObjectAlignment) - nbytes);
#endif
        return buffer;
    }

#if DBG
    bool IsAllZero();
#endif
    void ZeroAll();

#ifdef POLY_INLINE_CACHE_SIZE_STATS
    size_t GetPolyInlineCacheSize() { return this->polyCacheAllocSize; }
    void LogPolyCacheAlloc(size_t size) { this->polyCacheAllocSize += size; }
    void LogPolyCacheFree(size_t size) { this->polyCacheAllocSize -= size; }
#endif
};



#undef ASSERT_THREAD

class RefCounted
{
    volatile long refCount;

protected:
    virtual ~RefCounted()
    {
    }

public:
    RefCounted()
        : refCount(1)
    {
    }

    ulong AddRef(void)
    {
        return (ulong)InterlockedIncrement(&refCount);
    }

    ulong Release(void)
    {
        ulong refs = (ulong)InterlockedDecrement(&refCount);

        if (0 == refs)
        {
            delete this;
        }

        return refs;
    }
};


class ReferencedArenaAdapter;

template<class T>
class WeakArenaReference
{
    ReferencedArenaAdapter* adapter;
    T* p;

public:
    WeakArenaReference(ReferencedArenaAdapter* _adapter,T* _p)
        : adapter(_adapter),
          p(_p)
    {
        adapter->AddRef();
    }

    ~WeakArenaReference()
    {
        adapter->Release();
        adapter = NULL;
    }

    T* GetStrongReference()
    {
        if(adapter->AddStrongReference())
        {
            return p;
        }
        else
        {
            return NULL;
        }
    }
    void ReleaseStrongReference()
    {
        adapter->ReleaseStrongReference();
    }
};

// This class enables WeakArenaReferences to track whether
// the arena has been deleted, and allows for extending
// the lifetime of the arena for StrongReferences
// Strong references should be short lived
class ReferencedArenaAdapter : public RefCounted
{
    CRITICAL_SECTION adapterLock;
    ulong strongRefCount;
    ArenaAllocator* arena;
    bool deleteFlag;

public:

    ~ReferencedArenaAdapter()
    {
        if (this->arena)
        {
            HeapDelete(this->arena);
        }
        DeleteCriticalSection(&adapterLock);
    }

    ReferencedArenaAdapter(ArenaAllocator* _arena)
        : RefCounted(),
          strongRefCount(0),
          arena(_arena),
          deleteFlag(false)
    {
        InitializeCriticalSection(&adapterLock);
    }

    bool AddStrongReference()
    {
        EnterCriticalSection(&adapterLock);

        if (deleteFlag)
        {
            // Arena exists and is marked deleted, we must fail to acquire a new reference
            if (arena && 0 == strongRefCount)
            {
                // All strong references are gone, delete the arena
                HeapDelete(this->arena);
                this->arena = nullptr;
            }
            LeaveCriticalSection(&adapterLock);
            return false;
        }
        else
        {
            // Succeed at acquiring a Strong Reference into the Arena
            strongRefCount++;
            LeaveCriticalSection(&adapterLock);
            return true;
        }
    }

    void ReleaseStrongReference()
    {
        EnterCriticalSection(&adapterLock);
        strongRefCount--;

        if (deleteFlag && this->arena && 0 == strongRefCount)
        {
            // All strong references are gone, delete the arena
            HeapDelete(this->arena);
            this->arena = NULL;
        }

        LeaveCriticalSection(&adapterLock);
    }

    void DeleteArena()
    {
        deleteFlag = true;
        if (TryEnterCriticalSection(&adapterLock))
        {
            if (0 == strongRefCount)
            {
                // All strong references are gone, delete the arena
                HeapDelete(this->arena);
                this->arena = NULL;
            }
            LeaveCriticalSection(&adapterLock);
        }
    }

    ArenaAllocator* Arena()
    {
        if (!deleteFlag)
        {
            return this->arena;
        }
        return NULL;
    }
};
}

//we don't need these for the ArenaAllocator
#if 0
inline void __cdecl
operator delete(void * obj, ArenaAllocator * alloc, char * (ArenaAllocator::*AllocFunc)(size_t))
{
    alloc->Free(obj, (size_t)-1);
}

inline void __cdecl
operator delete(void * obj, ArenaAllocator * alloc, char * (ArenaAllocator::*AllocFunc)(size_t), size_t plusSize)
{
    alloc->Free(obj, (size_t)-1);
}
#endif
