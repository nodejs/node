//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#include "CollectionState.h"

namespace Js
{
    class Profiler;
    enum Phase;
};

namespace JsUtil
{
    class ThreadService;
};

class StackBackTraceNode;
class ScriptEngineBase;
class JavascriptThreadService;

#ifdef PROFILE_MEM
struct RecyclerMemoryData;
#endif

namespace Memory
{
template <typename T> class RecyclerRootPtr;

class AutoBooleanToggle
{
public:
    AutoBooleanToggle(bool * b, bool value = true, bool valueMayChange = false)
        : b(b)
    {
        Assert(!(*b));
        *b = value;
#if DBG
        this->value = value;
        this->valueMayChange = valueMayChange;
#endif
    }

    ~AutoBooleanToggle()
    {
        if (b)
        {
            Assert(valueMayChange || *b == value);
            *b = false;
        }
    }

    void Leave()
    {
        Assert(valueMayChange || *b == value);
        *b = false;
        b = nullptr;
    }

private:
    bool * b;
#if DBG
    bool value;
    bool valueMayChange;
#endif
};

template <class T>
class AutoRestoreValue
{
public:
    AutoRestoreValue(T* var, const T& val):
        variable(var)
    {
        Assert(var);
        oldValue = (*variable);
        (*variable) = val;
#ifdef DEBUG
        debugSetValue = val;
#endif
    }

    ~AutoRestoreValue()
    {
        Assert((*variable) == debugSetValue);
        (*variable) = oldValue;
    }

private:
#ifdef DEBUG
    T  debugSetValue;
#endif

    T* variable;
    T  oldValue;
};

class Recycler;

class RecyclerScanMemoryCallback
{
public:
    RecyclerScanMemoryCallback(Recycler* recycler) : recycler(recycler) {}
    void operator()(void** obj, size_t byteCount);
private:
    Recycler* recycler;
};

template<ObjectInfoBits infoBits>
struct InfoBitsWrapper{};

// Allocation macro
#define RecyclerNew(recycler,T,...) AllocatorNewBase(Recycler, recycler, AllocInlined, T, __VA_ARGS__)
#define RecyclerNewPlus(recycler,size,T,...) AllocatorNewPlus(Recycler, recycler, size, T, __VA_ARGS__)
#define RecyclerNewPlusLeaf(recycler,size,T,...) AllocatorNewPlusLeaf(Recycler, recycler, size, T, __VA_ARGS__)
#define RecyclerNewPlusZ(recycler,size,T,...) AllocatorNewPlusZ(Recycler, recycler, size, T, __VA_ARGS__)
#define RecyclerNewPlusLeafZ(recycler,size,T,...) AllocatorNewPlusLeafZ(Recycler, recycler, size, T, __VA_ARGS__)
#define RecyclerNewZ(recycler,T,...) AllocatorNewBase(Recycler, recycler, AllocZeroInlined, T, __VA_ARGS__)
#define RecyclerNewStruct(recycler,T) AllocatorNewStructBase(Recycler, recycler, AllocInlined, T)
#define RecyclerNewStructZ(recycler,T) AllocatorNewStructBase(Recycler, recycler, AllocZeroInlined, T)
#define RecyclerNewStructPlus(recycler,size,T) AllocatorNewStructPlus(Recycler, recycler, size, T)
#define RecyclerNewStructLeaf(recycler,T) AllocatorNewStructBase(Recycler, recycler, AllocLeafInlined, T)
#define RecyclerNewStructLeafZ(recycler,T) AllocatorNewStructBase(Recycler, recycler, AllocLeafZeroInlined, T)
#define RecyclerNewLeaf(recycler,T,...) AllocatorNewBase(Recycler, recycler, AllocLeafInlined, T, __VA_ARGS__)
#define RecyclerNewLeafZ(recycler,T,...) AllocatorNewBase(Recycler, recycler, AllocLeafZeroInlined, T, __VA_ARGS__)
#define RecyclerNewArrayLeafZ(recycler,T,count) AllocatorNewArrayBase(Recycler, recycler, AllocLeafZero, T, count)
#define RecyclerNewArray(recycler,T,count) AllocatorNewArrayBase(Recycler, recycler, Alloc, T, count)
#define RecyclerNewArrayZ(recycler,T,count) AllocatorNewArrayBase(Recycler, recycler, AllocZero, T, count)
#define RecyclerNewArrayLeaf(recycler,T,count) AllocatorNewArrayBase(Recycler, recycler, AllocLeaf, T, count)
// Use static_cast to make sure the finalized and tracked object have the right base class
#define RecyclerNewFinalized(recycler,T,...) static_cast<T *>(static_cast<FinalizableObject *>(AllocatorNewBase(Recycler, recycler, AllocFinalizedInlined, T, __VA_ARGS__)))
#define RecyclerNewFinalizedLeaf(recycler,T,...) static_cast<T *>(static_cast<FinalizableObject *>(AllocatorNewBase(Recycler, recycler, AllocFinalizedLeafInlined, T, __VA_ARGS__)))
#define RecyclerNewFinalizedPlus(recycler, size, T,...) static_cast<T *>(static_cast<FinalizableObject *>(AllocatorNewPlusBase(Recycler, recycler, AllocFinalized, size, T, __VA_ARGS__)))
#define RecyclerNewFinalizedLeafPlus(recycler, size, T,...) static_cast<T *>(static_cast<FinalizableObject *>(AllocatorNewPlusBase(Recycler, recycler, AllocFinalizedLeaf, size, T, __VA_ARGS__)))
#define RecyclerNewTracked(recycler,T,...) static_cast<T *>(static_cast<FinalizableObject *>(AllocatorNewBase(Recycler, recycler, AllocTrackedInlined, T, __VA_ARGS__)))
#define RecyclerNewTrackedLeaf(recycler,T,...) static_cast<T *>(static_cast<FinalizableObject *>(AllocatorNewBase(Recycler, recycler, AllocTrackedLeafInlined, T, __VA_ARGS__)))
#define RecyclerNewTrackedLeafPlusZ(recycler,size,T,...) static_cast<T *>(static_cast<FinalizableObject *>(AllocatorNewPlusBase(Recycler, recycler, AllocZeroTrackedLeafInlined, size, T, __VA_ARGS__)))
#define RecyclerNewEnumClass(recycler, enumClass, T, ...) new (TRACK_ALLOC_INFO(static_cast<Recycler *>(recycler), T, Recycler, 0, (size_t)-1), enumClass) T(__VA_ARGS__)
#define RecyclerNewWithInfoBits(recycler, infoBits, T, ...) new (TRACK_ALLOC_INFO(static_cast<Recycler *>(recycler), T, Recycler, 0, (size_t)-1), InfoBitsWrapper<infoBits>()) T(__VA_ARGS__)
#define RecyclerNewFinalizedClientTracked(recycler,T,...) static_cast<T *>(static_cast<FinalizableObject *>(AllocatorNewBase(Recycler, recycler, AllocFinalizedClientTrackedInlined, T, __VA_ARGS__)))

#ifdef RECYCLER_WRITE_BARRIER_ALLOC
#define RecyclerNewWithBarrier(recycler,T,...) AllocatorNewBase(Recycler, recycler, AllocWithBarrier, T, __VA_ARGS__)
#define RecyclerNewWithBarrierPlus(recycler,size,T,...) AllocatorNewPlusBase(Recycler, recycler, AllocWithBarrier, size, T, __VA_ARGS__)
#define RecyclerNewWithBarrierPlusZ(recycler,size,T,...) AllocatorNewPlusBase(Recycler, recycler, AllocZeroWithBarrier, size, T, __VA_ARGS__)
#define RecyclerNewWithBarrierArray(recycler,T,count) AllocatorNewArrayBase(Recycler, recycler, AllocWithBarrier, T, count)
#define RecyclerNewWithBarrierArrayZ(recycler,T,count) AllocatorNewArrayBase(Recycler, recycler, AllocZeroWithBarrier, T, count)
#define RecyclerNewWithBarrierStruct(recycler,T) AllocatorNewStructBase(Recycler, recycler, AllocWithBarrier, T)
#define RecyclerNewWithBarrierStructZ(recycler,T) AllocatorNewStructBase(Recycler, recycler, AllocZeroWithBarrier, T)
#define RecyclerNewWithBarrierFinalized(recycler,T,...) static_cast<T *>(static_cast<FinalizableObject *>(AllocatorNewBase(Recycler, recycler, AllocFinalizedWithBarrierInlined, T, __VA_ARGS__)))
#define RecyclerNewWithBarrierFinalizedPlus(recycler, size, T,...) static_cast<T *>(static_cast<FinalizableObject *>(AllocatorNewPlusBase(Recycler, recycler, AllocFinalizedWithBarrier, size, T, __VA_ARGS__)))
#else
#define RecyclerNewWithBarrier RecyclerNew
#define RecyclerNewWithBarrierPlus RecyclerNewPlus
#define RecyclerNewWithBarrierPlusZ RecyclerNewPlusZ
#define RecyclerNewWithBarrierArray RecyclerNewArray
#define RecyclerNewWithBarrierArrayZ RecyclerNewArrayZ
#define RecyclerNewWithBarrierStruct RecyclerNewStruct
#define RecyclerNewWithBarrierStructZ RecyclerNewStructZ
#define RecyclerNewWithBarrierFinalized RecyclerNewFinalized
#define RecyclerNewWithBarrierFinalizedPlus RecyclerNewFinalizedPlus
#endif

#ifdef TRACE_OBJECT_LIFETIME
#define RecyclerNewLeafTrace(recycler,T,...) AllocatorNewBase(Recycler, recycler, AllocLeafTrace, T, __VA_ARGS__)
#define RecyclerNewLeafZTrace(recycler,T,...) AllocatorNewBase(Recycler, recycler, AllocLeafZeroTrace, T, __VA_ARGS__)
#define RecyclerNewPlusLeafTrace(recycler,size,T,...) AllocatorNewPlusBase(Recycler, recycler, AllocLeafTrace, size, T, __VA_ARGS__)
#define RecyclerNewArrayLeafZTrace(recycler,T,count) AllocatorNewArrayBase(Recycler, recycler, AllocLeafZeroTrace, T, count)
#define RecyclerNewArrayTrace(recycler,T,count) AllocatorNewArrayBase(Recycler, recycler, AllocTrace, T, count)
#define RecyclerNewArrayZTrace(recycler,T,count) AllocatorNewArrayBase(Recycler, recycler, AllocZeroTrace, T, count)
#define RecyclerNewArrayLeafTrace(recycler,T,count) AllocatorNewArrayBase(Recycler, recycler, AllocLeafTrace, T, count)
// Use static_cast to make sure the finalized and tracked object have the right base class
#define RecyclerNewFinalizedTrace(recycler,T,...) static_cast<T *>(static_cast<FinalizableObject *>(AllocatorNewBase(Recycler, recycler, AllocFinalizedTrace, T, __VA_ARGS__)))
#define RecyclerNewFinalizedLeafTrace(recycler,T,...) static_cast<T *>(static_cast<FinalizableObject *>(AllocatorNewBase(Recycler, recycler, AllocFinalizedLeafTrace, T, __VA_ARGS__)))
#define RecyclerNewFinalizedPlusTrace(recycler, size, T,...) static_cast<T *>(static_cast<FinalizableObject *>(AllocatorNewPlusBase(Recycler, recycler, AllocFinalizedTrace, size, T, __VA_ARGS__)))
#define RecyclerNewTrackedTrace(recycler,T,...) static_cast<T *>(static_cast<FinalizableObject *>(AllocatorNewBase(Recycler, recycler, AllocTrackedTrace, T, __VA_ARGS__)))
#define RecyclerNewTrackedLeafTrace(recycler,T,...) static_cast<T *>(static_cast<FinalizableObject *>(AllocatorNewBase(Recycler, recycler, AllocTrackedLeafTrace, T, __VA_ARGS__)))
#else
#define RecyclerNewLeafTrace RecyclerNewLeaf
#define RecyclerNewLeafZTrace  RecyclerNewLeafZ
#define RecyclerNewPlusLeafTrace RecyclerNewPlusLeaf
#define RecyclerNewArrayLeafZTrace RecyclerNewArrayLeafZ
#define RecyclerNewArrayTrace RecyclerNewArray
#define RecyclerNewArrayZTrace RecyclerNewArrayZ
#define RecyclerNewArrayLeafTrace RecyclerNewArrayLeaf
#define RecyclerNewFinalizedTrace RecyclerNewFinalized
#define RecyclerNewFinalizedLeafTrace RecyclerNewFinalizedLeaf
#define RecyclerNewFinalizedPlusTrace RecyclerNewFinalizedPlus
#define RecyclerNewTrackedTrace RecyclerNewTracked
#define RecyclerNewTrackedLeafTrace RecyclerNewTrackedLeaf
#endif

#ifdef RECYCLER_TRACE
#define RecyclerVerboseTrace(flags, ...) \
    if (flags.Verbose && flags.Trace.IsEnabled(Js::RecyclerPhase)) \
        { \
        Output::Print(__VA_ARGS__); \
        }
#define AllocationVerboseTrace(flags, ...) \
    if (flags.Verbose && flags.Trace.IsEnabled(Js::MemoryAllocationPhase)) \
        { \
        Output::Print(__VA_ARGS__); \
        }

#define LargeAllocationVerboseTrace(flags, ...) \
    if (flags.Verbose && \
        (flags.Trace.IsEnabled(Js::MemoryAllocationPhase) || \
         flags.Trace.IsEnabled(Js::LargeMemoryAllocationPhase))) \
        { \
        Output::Print(__VA_ARGS__); \
        }
#define PageAllocatorAllocationVerboseTrace(flags, ...) \
    if (flags.Verbose && flags.Trace.IsEnabled(Js::PageAllocatorAllocPhase)) \
        { \
        Output::Print(__VA_ARGS__); \
        }

#else
#define RecyclerVerboseTrace(...)
#define AllocationVerboseTrace(...)
#define LargeAllocationVerboseTrace(...)

#endif

#define RecyclerHeapNew(recycler,heapInfo,T,...) new (recycler, heapInfo) T(__VA_ARGS__)
#define RecyclerHeapDelete(recycler,heapInfo,addr) (static_cast<Recycler *>(recycler)->HeapFree(heapInfo,addr))

typedef void (__cdecl* ExternalRootMarker)(void *);

enum CollectionFlags
{
    CollectHeuristic_AllocSize          = 0x00000001,
    CollectHeuristic_Time               = 0x00000002,
    CollectHeuristic_TimeIfScriptActive = 0x00000004,
    CollectHeuristic_TimeIfInScript     = 0x00000008,
    CollectHeuristic_Never              = 0x00000080,
    CollectHeuristic_Mask               = 0x000000FF,

    CollectOverride_FinishConcurrent    = 0x00001000,
    CollectOverride_ExhaustiveCandidate = 0x00002000,
    CollectOverride_ForceInThread       = 0x00004000,
    CollectOverride_AllowDispose        = 0x00008000,
    CollectOverride_AllowReentrant      = 0x00010000,
    CollectOverride_ForceFinish         = 0x00020000,
    CollectOverride_Explicit            = 0x00040000,
    CollectOverride_DisableIdleFinish   = 0x00080000,
    CollectOverride_BackgroundFinishMark= 0x00100000,
    CollectOverride_FinishConcurrentTimeout = 0x00200000,
    CollectOverride_NoExhaustiveCollect = 0x00400000,
    CollectOverride_SkipStack           = 0x01000000,

    CollectMode_Partial                 = 0x08000000,
    CollectMode_Concurrent              = 0x10000000,
    CollectMode_Exhaustive              = 0x20000000,
    CollectMode_DecommitNow             = 0x40000000,
    CollectMode_CacheCleanup            = 0x80000000,

    CollectNowForceInThread         = CollectOverride_ForceInThread,
    CollectNowForceInThreadExternal = CollectOverride_ForceInThread | CollectOverride_AllowDispose,
    CollectNowForceInThreadExternalNoStack = CollectOverride_ForceInThread | CollectOverride_AllowDispose | CollectOverride_SkipStack,
    CollectNowDefault               = CollectOverride_FinishConcurrent,
    CollectNowDefaultLSCleanup      = CollectOverride_FinishConcurrent | CollectOverride_AllowDispose,
    CollectNowDecommitNowExplicit   = CollectNowDefault | CollectMode_DecommitNow | CollectMode_CacheCleanup | CollectOverride_Explicit | CollectOverride_AllowDispose,
    CollectNowConcurrent            = CollectOverride_FinishConcurrent | CollectMode_Concurrent,
    CollectNowExhaustive            = CollectOverride_FinishConcurrent | CollectMode_Exhaustive | CollectOverride_AllowDispose,
    CollectNowPartial               = CollectOverride_FinishConcurrent | CollectMode_Partial,
    CollectNowConcurrentPartial     = CollectMode_Concurrent | CollectNowPartial,

    CollectOnAllocation             = CollectHeuristic_AllocSize | CollectHeuristic_Time | CollectMode_Concurrent | CollectMode_Partial | CollectOverride_FinishConcurrent | CollectOverride_AllowReentrant | CollectOverride_FinishConcurrentTimeout,
    CollectOnTypedArrayAllocation   = CollectHeuristic_AllocSize | CollectHeuristic_Time | CollectMode_Concurrent | CollectMode_Partial | CollectOverride_FinishConcurrent | CollectOverride_AllowReentrant | CollectOverride_FinishConcurrentTimeout | CollectOverride_AllowDispose,
    CollectOnScriptIdle             = CollectOverride_FinishConcurrent | CollectMode_Concurrent | CollectMode_CacheCleanup | CollectOverride_SkipStack,
    CollectOnScriptExit             = CollectHeuristic_AllocSize | CollectOverride_FinishConcurrent | CollectMode_Concurrent | CollectMode_CacheCleanup,
    CollectExhaustiveCandidate      = CollectHeuristic_Never | CollectOverride_ExhaustiveCandidate,
    CollectOnScriptCloseNonPrimary  = CollectNowConcurrent | CollectOverride_ExhaustiveCandidate | CollectOverride_AllowDispose,
    CollectOnRecoverFromOutOfMemory = CollectOverride_ForceInThread | CollectMode_DecommitNow,
    CollectOnSuspendCleanup         = CollectNowConcurrent | CollectMode_Exhaustive | CollectMode_DecommitNow | CollectOverride_DisableIdleFinish,

    FinishConcurrentOnIdle          = CollectMode_Concurrent | CollectOverride_DisableIdleFinish,
    FinishConcurrentOnIdleAtRoot    = CollectMode_Concurrent | CollectOverride_DisableIdleFinish | CollectOverride_SkipStack,
    FinishConcurrentOnExitScript    = CollectMode_Concurrent | CollectOverride_DisableIdleFinish | CollectOverride_BackgroundFinishMark,
    FinishConcurrentOnEnterScript   = CollectMode_Concurrent | CollectOverride_DisableIdleFinish | CollectOverride_BackgroundFinishMark,
    FinishConcurrentOnAllocation    = CollectMode_Concurrent | CollectOverride_DisableIdleFinish | CollectOverride_BackgroundFinishMark,
    FinishDispose                   = CollectOverride_AllowDispose,
    FinishDisposeTimed              = CollectOverride_AllowDispose | CollectHeuristic_TimeIfScriptActive,
    ForceFinishCollection           = CollectOverride_ForceFinish | CollectOverride_ForceInThread,

#ifdef RECYCLER_STRESS
    CollectStress                   = CollectNowForceInThread,
#ifdef PARTIAL_GC_ENABLED
    CollectPartialStress            = CollectMode_Partial,
#endif
#ifdef CONCURRENT_GC_ENABLED
    CollectBackgroundStress         = CollectNowDefault,
    CollectConcurrentStress         = CollectNowConcurrent,
#ifdef PARTIAL_GC_ENABLED
    CollectConcurrentPartialStress  = CollectConcurrentStress | CollectPartialStress,
#endif
#endif
#endif

#if defined(CHECK_MEMORY_LEAK) || defined(LEAK_REPORT)
    CollectNowFinalGC                   = CollectNowExhaustive | CollectOverride_ForceInThread | CollectOverride_SkipStack | CollectOverride_Explicit | CollectOverride_AllowDispose,
#endif
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    CollectNowExhaustiveSkipStack   = CollectNowExhaustive | CollectOverride_SkipStack, // Used by test
#endif
};

class RecyclerCollectionWrapper
{
public:
    typedef BOOL (Recycler::*CollectionFunction)(CollectionFlags flags);
    virtual void PreCollectionCallBack(CollectionFlags flags) = 0;
    virtual void PreSweepCallback() = 0;
    virtual void PreRescanMarkCallback() = 0;
    virtual size_t RootMarkCallback(RecyclerScanMemoryCallback& scanMemoryCallback, BOOL * stacksScannedByRuntime) = 0;
    virtual void RescanMarkTimeoutCallback() = 0;
    virtual void EndMarkCallback() = 0;
    virtual void ConcurrentCallback() = 0;
    virtual void WaitCollectionCallBack() = 0;
    virtual void PostCollectionCallBack() = 0;
    virtual BOOL ExecuteRecyclerCollectionFunction(Recycler * recycler, CollectionFunction function, CollectionFlags flags) = 0;
    virtual uint GetRandomNumber() = 0;
#ifdef FAULT_INJECTION
    virtual void DisposeScriptContextByFaultInjectionCallBack() = 0;
#endif
    virtual void DisposeObjects(Recycler * recycler) = 0;
#ifdef ENABLE_PROJECTION
    virtual void MarkExternalWeakReferencedObjects(bool inPartialCollect) = 0;
    virtual void ResolveExternalWeakReferencedObjects() = 0;
#endif
#if DBG || defined(PROFILE_EXEC)
    virtual bool AsyncHostOperationStart(void *) = 0;
    virtual void AsyncHostOperationEnd(bool wasInAsync, void *) = 0;
#endif
};

class DefaultRecyclerCollectionWrapper : public RecyclerCollectionWrapper
{
public:
    virtual void PreCollectionCallBack(CollectionFlags flags) override {}
    virtual void PreSweepCallback() override {}
    virtual void PreRescanMarkCallback() override {}
    virtual void RescanMarkTimeoutCallback() override {}
    virtual void EndMarkCallback() override {}
    virtual size_t RootMarkCallback(RecyclerScanMemoryCallback& scanMemoryCallback, BOOL * stacksScannedByRuntime) override { *stacksScannedByRuntime = FALSE; return 0; }
    virtual void ConcurrentCallback() override {}
    virtual void WaitCollectionCallBack() override {}
    virtual void PostCollectionCallBack() override {}
    virtual BOOL ExecuteRecyclerCollectionFunction(Recycler * recycler, CollectionFunction function, CollectionFlags flags) override;
    virtual uint GetRandomNumber() override { return 0; }
#ifdef FAULT_INJECTION
    virtual void DisposeScriptContextByFaultInjectionCallBack() override {};
#endif
    virtual void DisposeObjects(Recycler * recycler) override;

#ifdef ENABLE_PROJECTION
    virtual void MarkExternalWeakReferencedObjects(bool inPartialCollect) override {};
    virtual void ResolveExternalWeakReferencedObjects() override {};
#endif
#if DBG || defined(PROFILE_EXEC)
    virtual bool AsyncHostOperationStart(void *) override { return false; };
    virtual void AsyncHostOperationEnd(bool wasInAsync, void *) override {};
#endif
    static DefaultRecyclerCollectionWrapper Instance;

private:
    static bool IsCollectionDisabled(Recycler * recycler);
};


#ifdef RECYCLER_STATS
struct RecyclerCollectionStats
{
    size_t startCollectAllocBytes;
#ifdef PARTIAL_GC_ENABLED
    size_t startCollectNewPageCount;
#endif
    size_t continueCollectAllocBytes;

    size_t finishCollectTryCount;

    // Heuristic Stats
#ifdef PARTIAL_GC_ENABLED
    size_t rescanRootBytes;
    size_t estimatedPartialReuseBytes;
    size_t uncollectedNewPageCountPartialCollect;
    size_t partialCollectSmallHeapBlockReuseMinFreeBytes;
    double collectEfficacy;
    double collectCost;
#endif

    // Mark stats
    size_t tryMarkCount;        // # of pointer try mark (* pointer size to get total number byte looked at)
    size_t tryMarkNullCount;
    size_t tryMarkUnalignedCount;
    size_t tryMarkNonRecyclerMemoryCount;
    size_t tryMarkInteriorCount;
    size_t tryMarkInteriorNullCount;
    size_t tryMarkInteriorNonRecyclerMemoryCount;
    size_t rootCount;
    size_t stackCount;
    size_t remarkCount;

    size_t scanCount;           // non-leaf objects marked.
    size_t trackCount;
    size_t finalizeCount;
    size_t markThruNewObjCount;
    size_t markThruFalseNewObjCount;

    struct MarkData
    {
        // Rescan stats
#if defined(PARTIAL_GC_ENABLED) || defined(CONCURRENT_GC_ENABLED)
        size_t rescanPageCount;
        size_t rescanObjectCount;
        size_t rescanObjectByteCount;
        size_t rescanLargePageCount;
        size_t rescanLargeObjectCount;
        size_t rescanLargeByteCount;
#endif
        size_t markCount;           // total number of object marked
        size_t markBytes;           // size of all objects marked.
    } markData;

#ifdef CONCURRENT_GC_ENABLED
    MarkData backgroundMarkData[RecyclerHeuristic::MaxBackgroundRepeatMarkCount];
    size_t trackedObjectCount;
#endif

#ifdef PARTIAL_GC_ENABLED
    size_t clientTrackedObjectCount;
#endif

    // Sweep stats
    size_t heapBlockCount[HeapBlock::BlockTypeCount];                       // number of heap blocks (processed during swept)
    size_t heapBlockFreeCount[HeapBlock::BlockTypeCount];                   // number of heap blocks deleted
    size_t heapBlockConcurrentSweptCount[HeapBlock::SmallBlockTypeCount];
    size_t heapBlockSweptCount[HeapBlock::SmallBlockTypeCount];             // number of heap blocks swept

    size_t objectSweptCount;                // objects freed (free list + whole page freed)
    size_t objectSweptBytes;
    size_t objectSweptFreeListCount;        // objects freed (free list)
    size_t objectSweptFreeListBytes;
    size_t objectSweepScanCount;            // number of objects walked for sweeping (exclude whole page freed)
    size_t finalizeSweepCount;              // number of objects finalizer/dispose called

#ifdef PARTIAL_GC_ENABLED
    size_t smallNonLeafHeapBlockPartialReuseCount[HeapBlock::SmallBlockTypeCount];
    size_t smallNonLeafHeapBlockPartialReuseBytes[HeapBlock::SmallBlockTypeCount];
    size_t smallNonLeafHeapBlockPartialUnusedCount[HeapBlock::SmallBlockTypeCount];
    size_t smallNonLeafHeapBlockPartialUnusedBytes[HeapBlock::SmallBlockTypeCount];
#endif

    // Memory Stats
    size_t heapBlockFreeByteCount[HeapBlock::BlockTypeCount]; // The remaining usable free byte count

    size_t largeHeapBlockUsedByteCount;     // Used byte count
    size_t largeHeapBlockTotalByteCount;    // Total byte count

    // Empty/zero heap block stats
    uint numEmptySmallBlocks[HeapBlock::SmallBlockTypeCount];
    uint numZeroedOutSmallBlocks;
};
#define RECYCLER_STATS_INC_IF(cond, r, f) if (cond) { RECYCLER_STATS_INC(r, f); }
#define RECYCLER_STATS_INC(r, f) ++r->collectionStats.f
#define RECYCLER_STATS_INTERLOCKED_INC(r, f) { InterlockedIncrement((LONG *)&r->collectionStats.f); }
#define RECYCLER_STATS_DEC(r, f) --r->collectionStats.f
#define RECYCLER_STATS_ADD(r, f, v) r->collectionStats.f += (v)
#define RECYCLER_STATS_INTERLOCKED_ADD(r, f, v) { InterlockedAdd((LONG *)&r->collectionStats.f, (LONG)(v)); }
#define RECYCLER_STATS_SUB(r, f, v) r->collectionStats.f -= (v)
#define RECYCLER_STATS_SET(r, f, v) r->collectionStats.f = v
#else
#define RECYCLER_STATS_INC_IF(cond, r, f)
#define RECYCLER_STATS_INC(r, f)
#define RECYCLER_STATS_INTERLOCKED_INC(r, f)
#define RECYCLER_STATS_DEC(r, f)
#define RECYCLER_STATS_ADD(r, f, v)
#define RECYCLER_STATS_INTERLOCKED_ADD(r, f, v)
#define RECYCLER_STATS_SUB(r, f, v)
#define RECYCLER_STATS_SET(r, f, v)
#endif
#ifdef RECYCLER_TRACE
struct CollectionParam
{
    CollectionFlags flags;
    bool finishOnly;
    bool repeat;
    bool priorityBoostConcurentSweepOverride;
    bool domCollect;
    int timeDiff;
    size_t uncollectedAllocBytes;
    size_t uncollectedPinnedObjects;
#ifdef PARTIAL_GC_ENABLED
    size_t uncollectedNewPageCountPartialCollect;
    size_t uncollectedNewPageCount;
    size_t unusedPartialCollectFreeBytes;
    bool inPartialCollectMode;
#endif
};
#endif

#include "RecyclerObjectGraphDumper.h"

#ifdef RECYCLER_WRITE_BARRIER_ALLOC_SEPARATE_PAGE
// Macro to be used within the recycler
#define ForRecyclerPageAllocator(action) { \
    this->recyclerPageAllocator.##action; \
    this->recyclerLargeBlockPageAllocator.##action; \
    this->recyclerWithBarrierPageAllocator.##action; \
    this->threadPageAllocator->##action; \
}

// Macro that external objects referencing the recycler can use
#define ForEachRecyclerPageAllocatorIn(recycler, action) { \
    recycler->GetRecyclerPageAllocator()->##action; \
    recycler->GetRecyclerLargeBlockPageAllocator()->##action; \
    recycler->GetRecyclerWithBarrierPageAllocator()->##action; \
    recycler->GetRecyclerLeafPageAllocator()->##action; \
}

#else

// Macro to be used within the recycler
#define ForRecyclerPageAllocator(action) { \
    this->recyclerPageAllocator.##action; \
    this->recyclerLargeBlockPageAllocator.##action; \
    this->threadPageAllocator->##action; \
}

// Macro that external objects referencing the recycler can use
#define ForEachRecyclerPageAllocatorIn(recycler, action) { \
    recycler->GetRecyclerPageAllocator()->##action; \
    recycler->GetRecyclerLargeBlockPageAllocator()->##action; \
    recycler->GetRecyclerLeafPageAllocator()->##action; \
}

#endif


class RecyclerParallelThread
{
public:
    typedef void (Recycler::* WorkFunc)();

    RecyclerParallelThread(Recycler * recycler, WorkFunc workFunc) :
        recycler(recycler),
        workFunc(workFunc),
        concurrentWorkReadyEvent(NULL),
        concurrentWorkDoneEvent(NULL),
        concurrentThread(NULL)
    {
    }

    ~RecyclerParallelThread()
    {
        Assert(concurrentThread == NULL);
        Assert(concurrentWorkReadyEvent == NULL);
        Assert(concurrentWorkDoneEvent == NULL);
    }

    bool StartConcurrent();
    void WaitForConcurrent();
    void Shutdown();
    bool EnableConcurrent(bool synchronizeOnStartup);

private:
    // Static entry point for thread creation
    static unsigned int StaticThreadProc(LPVOID lpParameter);

    // Static entry point for thread service usage
    static void StaticBackgroundWorkCallback(void * callbackData);

private:
    WorkFunc workFunc;
    Recycler * recycler;
    HANDLE concurrentWorkReadyEvent;// main thread uses this event to tell concurrent threads that the work is ready
    HANDLE concurrentWorkDoneEvent;// concurrent threads use this event to tell main thread that the work allocated is done
    HANDLE concurrentThread;
    bool synchronizeOnStartup;
};

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
class AutoProtectPages
{
public:
    AutoProtectPages(Recycler* recycler, bool protectEnabled);
    ~AutoProtectPages();
    void Unprotect();

private:
    Recycler* recycler;
    bool isReadOnly;
};
#endif


class Recycler
{
    friend class RecyclerScanMemoryCallback;
    friend class RecyclerSweep;
    friend class MarkContext;
    friend class HeapBlock;
    friend class HeapBlockMap32;
    friend class RecyclerParallelThread;
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    friend class AutoProtectPages;
#endif

    template <typename T> friend class RecyclerWeakReference;
    template <typename T> friend class WeakReferenceHashTable;
    template <typename TBlockType>
    friend class SmallHeapBlockAllocator;       // Needed for FindHeapBlock
#if defined(RECYCLER_TRACE)
    friend class JavascriptThreadService;
#endif
#ifdef HEAP_ENUMERATION_VALIDATION
    friend class ActiveScriptProfilerHeapEnum;
#endif
    friend class ScriptEngineBase;  // This is for disabling GC for certain Host operations.
    friend class CodeGenNumberThreadAllocator;
public:
    static const uint ConcurrentThreadStackSize = 300000;
    static const bool FakeZeroLengthArray = true;

#ifdef RECYCLER_PAGE_HEAP
    // Keeping as constant in case we want to tweak the value here
    // Set to 0 so that the tool can do the filtering instead of the runtime
    static const int s_numFramesToSkipForPageHeapAlloc = 0;
    static const int s_numFramesToSkipForPageHeapFree = 0;

    static const int s_numFramesToCaptureForPageHeap = 20;
#endif

    uint Cookie;

    class AutoEnterExternalStackSkippingGCMode
    {
    public:
        AutoEnterExternalStackSkippingGCMode(Recycler* recycler):
            _recycler(recycler)
        {
            // Setting this in a re-entrant mode is not allowed
            Assert(!recycler->isExternalStackSkippingGC);

#if DBG
            _recycler->isExternalStackSkippingGC = true;
#endif
        }

        ~AutoEnterExternalStackSkippingGCMode()
        {
#if DBG
            _recycler->isExternalStackSkippingGC = false;
#endif
        }

    private:
        Recycler* _recycler;
    };

private:
    class AutoSwitchCollectionStates
    {
    public:
        AutoSwitchCollectionStates(Recycler* recycler, CollectionState entryState, CollectionState exitState):
            _recycler(recycler),
            _exitState(exitState)
        {
            _recycler->collectionState = entryState;
        }

        ~AutoSwitchCollectionStates()
        {
            _recycler->collectionState = _exitState;
        }

    private:
        Recycler* _recycler;
        CollectionState _exitState;
    };

    CollectionState collectionState;
    IdleDecommitPageAllocator * threadPageAllocator;
#ifdef RECYCLER_WRITE_BARRIER_ALLOC_SEPARATE_PAGE
    RecyclerPageAllocator recyclerWithBarrierPageAllocator;
#endif
    RecyclerPageAllocator recyclerPageAllocator;
    RecyclerPageAllocator recyclerLargeBlockPageAllocator;

    JsUtil::ThreadService *threadService;

    HeapBlockMap heapBlockMap;

#if defined(CHECK_MEMORY_LEAK) || defined(LEAK_REPORT)
    struct PinRecord
    {
        PinRecord() : refCount(0), stackBackTraces(nullptr) {}
        PinRecord& operator=(uint newRefCount)
        {
            Assert(stackBackTraces == nullptr); Assert(newRefCount == 0); refCount = 0; return *this;
        }
        PinRecord& operator++() { ++refCount; return *this; }
        PinRecord& operator--() { --refCount; return *this; }
        operator uint() const { return refCount; }
        StackBackTraceNode * stackBackTraces;
    private:
        uint refCount;
    };
#else
    typedef uint PinRecord;
#endif

    typedef SimpleHashTable<void *, PinRecord, HeapAllocator, DefaultComparer, true, PrimePolicy> PinnedObjectHashTable;
    PinnedObjectHashTable pinnedObjectMap;

    WeakReferenceHashTable<PrimePolicy> weakReferenceMap;
    uint weakReferenceCleanupId;

    void * transientPinnedObject;
#if defined(CHECK_MEMORY_LEAK) || defined(LEAK_REPORT)
    StackBackTrace * transientPinnedObjectStackBackTrace;
#endif

    struct GuestArenaAllocator : public ArenaAllocator
    {
        GuestArenaAllocator(__in LPCWSTR name, PageAllocator * pageAllocator, void (*outOfMemoryFunc)())
            : ArenaAllocator(name, pageAllocator, outOfMemoryFunc), pendingDelete(false)
        {
        }
        bool pendingDelete;
    };
    DListBase<GuestArenaAllocator> guestArenaList;
    DListBase<ArenaData*> externalGuestArenaList;    // guest arenas are scanned for roots
    HeapInfo autoHeap;
#ifdef RECYCLER_PAGE_HEAP
    __inline bool IsPageHeapEnabled() const { return isPageHeapEnabled; }
    __inline bool ShouldCapturePageHeapAllocStack() const { return capturePageHeapAllocStack; }
    bool isPageHeapEnabled;
    bool capturePageHeapAllocStack;
    bool capturePageHeapFreeStack;
#else
    __inline const bool IsPageHeapEnabled() const { return false; }
    __inline bool ShouldCapturePageHeapAllocStack() const { return false; }
#endif


#ifdef RECYCLER_MARK_TRACK
    MarkMap* markMap;
    CriticalSection markMapCriticalSection;

    void PrintMarkMap();
    void ClearMarkMap();
#endif

    // Number of pages to reserve for the primary mark stack
    // This is the minimum number of pages to guarantee that a single heap block
    // can be rescanned in the worst possible case where every object in a heap block
    // in the smallest bucket needs to be rescanned
    // These many pages being reserved guarantees that in OOM Rescan, we can make progress
    // on every rescan iteration
    // We add one because there is a small amount of the page reserved for page pool metadata
    // so we need to allocate an additional page to be sure
    // Currently, this works out to 2 pages on 32-bit and 5 pages on 64-bit
    static const int PrimaryMarkStackReservedPageCount =
        ((SmallAllocationBlockAttributes::PageCount * MarkContext::MarkCandidateSize) / SmallAllocationBlockAttributes::MinObjectSize) + 1;

    MarkContext markContext;

    // Contexts for parallel marking.
    // We support up to 4 way parallelism, main context + 3 additional parallel contexts.
    MarkContext parallelMarkContext1;
    MarkContext parallelMarkContext2;
    MarkContext parallelMarkContext3;

    // Page pools for above markContexts
    PagePool markPagePool;
    PagePool parallelMarkPagePool1;
    PagePool parallelMarkPagePool2;
    PagePool parallelMarkPagePool3;

    bool IsMarkStackEmpty();
    bool HasPendingMarkObjects() const { return markContext.HasPendingMarkObjects() || parallelMarkContext1.HasPendingMarkObjects() || parallelMarkContext2.HasPendingMarkObjects() || parallelMarkContext3.HasPendingMarkObjects(); }
    bool HasPendingTrackObjects() const { return markContext.HasPendingTrackObjects() || parallelMarkContext1.HasPendingTrackObjects() || parallelMarkContext2.HasPendingTrackObjects() || parallelMarkContext3.HasPendingTrackObjects(); }

    RecyclerCollectionWrapper * collectionWrapper;

    bool inDispose;

#if DBG
    uint collectionCount;
#endif
#if DBG || defined RECYCLER_TRACE
    bool inResolveExternalWeakReferences;
#endif

    bool allowDispose;
    bool inDisposeWrapper;
    bool needOOMRescan;
    bool hasDisposableObject;
    DWORD tickCountNextDispose;
    bool hasPendingTransferDisposedObjects;
    bool inExhaustiveCollection;
    bool hasExhaustiveCandidate;
    bool inCacheCleanupCollection;
    bool inDecommitNowCollection;
    bool isScriptActive;
    bool isInScript;
    bool isShuttingDown;
    bool scanPinnedObjectMap;
    bool hasScannedInitialImplicitRoots;
    bool hasPendingUnpinnedObject;
    bool hasPendingDeleteGuestArena;
    bool inEndMarkOnLowMemory;
    bool decommitOnFinish;
    bool enableScanInteriorPointers;
    bool enableScanImplicitRoots;
    bool disableCollectOnAllocationHeuristics;
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    bool disableCollection;
#endif

#ifdef PARTIAL_GC_ENABLED
    bool enablePartialCollect;
    bool inPartialCollectMode;
    bool hasBackgroundFinishPartial;
    bool partialConcurrentNextCollection;
#ifdef RECYCLER_STRESS
    bool forcePartialScanStack;
    bool recyclerStress;
#ifdef CONCURRENT_GC_ENABLED
    bool recyclerBackgroundStress;
    bool recyclerConcurrentStress;
    bool recyclerConcurrentRepeatStress;
#endif
#ifdef PARTIAL_GC_ENABLED
    bool recyclerPartialStress;
#endif
#endif
#endif
#ifdef CONCURRENT_GC_ENABLED
    bool skipStack;
#if DBG
    bool isConcurrentGCOnIdle;
    bool isFinishGCOnIdle;
    bool isExternalStackSkippingGC;
#endif

    bool queueTrackedObject;
    bool hasPendingConcurrentFindRoot;
    bool priorityBoost;
    bool disableConcurrent;
    bool enableConcurrentMark;
    bool enableParallelMark;
    bool enableConcurrentSweep;

    uint maxParallelism;        // Max # of total threads to run in parallel

    byte backgroundRescanCount;             // for ETW events and stats
    byte backgroundFinishMarkCount;
    size_t backgroundRescanRootBytes;
    HANDLE concurrentWorkReadyEvent; // main thread uses this event to tell concurrent threads that the work is ready
    HANDLE concurrentWorkDoneEvent; // concurrent threads use this event to tell main thread that the work allocated is done
    HANDLE concurrentThread;
    HANDLE mainThreadHandle;

    class SavedRegisterState
    {
    public:
#if _M_IX86
        static const int NumRegistersToSave = 8;
#elif _M_ARM
        static const int NumRegistersToSave = 13;
#elif _M_ARM64
        static const int NumRegistersToSave = 13;
#elif _M_AMD64
        static const int NumRegistersToSave = 16;
#endif

        SavedRegisterState()
        {
            memset(registers, 0, sizeof(void*) * NumRegistersToSave);
        }

        void** GetRegisters()
        {
            return registers;
        }

        void*  GetStackTop()
        {
            // By convention, our register-saving routine will always
            // save the stack pointer as the first item in the array
            return registers[0];
        }

    private:
        void* registers[NumRegistersToSave];
    };

    void * stackBase;
    SavedRegisterState savedThreadContext;

    template <uint parallelId>
    void ParallelWorkFunc();

    RecyclerParallelThread parallelThread1;
    RecyclerParallelThread parallelThread2;
    Js::ConfigFlagsTable&  recyclerFlagsTable;

#if DBG
    // Variable indicating if the concurrent thread has exited or not
    // If the concurrent thread hasn't started yet, this is set to true
    // Once the concurrent thread starts, it sets this to false,
    // and when the concurrent thread exits, it sets this to true.
    bool concurrentThreadExited;
    bool disableConcurentThreadExitedCheck;
    bool isProcessingTrackedObjects;
    bool hasIncompletedDoCollect;

    // This is set to true when we begin a Rescan, and set to false when either:
    // (1) We finish the final in-thread Rescan and are about to Mark
    // (2) We do a conditional ResetWriteWatch and are about to Mark
    // When this flag is true, we should not be modifying existing mark-related state,
    // including markBits and rescanState.
    bool isProcessingRescan;
#endif

    uint tickCountStartConcurrent;

    bool isAborting;
#endif

    RecyclerSweep recyclerSweepInstance;
    RecyclerSweep * recyclerSweep;

    static const uint tickDiffToNextCollect = 300;

#ifdef IDLE_DECOMMIT_ENABLED
    HANDLE concurrentIdleDecommitEvent;
    DWORD needIdleDecommitSignal;
#endif

#ifdef PARTIAL_GC_ENABLED
    SListBase<void *> clientTrackedObjectList;
    ArenaAllocator clientTrackedObjectAllocator;

    size_t partialUncollectedAllocBytes;

    // Dynamic Heuristics for partial GC
    size_t uncollectedNewPageCountPartialCollect;
#endif

    uint tickCountNextCollection;
    uint tickCountNextFinishCollection;

    void (*outOfMemoryFunc)();
#ifdef RECYCLER_TEST_SUPPORT
    BOOL (*checkFn)(char* addr, size_t size);
#endif
    ExternalRootMarker externalRootMarker;
    void * externalRootMarkerContext;

#ifdef PROFILE_EXEC
    Js::Profiler * profiler;
    Js::Profiler * backgroundProfiler;
    PageAllocator backgroundProfilerPageAllocator;
    DListBase<ArenaAllocator> backgroundProfilerArena;
#endif
#ifdef PROFILE_MEM
    RecyclerMemoryData * memoryData;
#endif
    ThreadContextId mainThreadId;
#ifdef ENABLE_BASIC_TELEMETRY
    Js::GCTelemetry gcTel;
#endif

#if DBG
    uint heapBlockCount;
    bool disableThreadAccessCheck;
#endif
#if DBG || defined(RECYCLER_STATS)
    bool isForceSweeping;
#endif
    RecyclerWatsonTelemetryBlock localTelemetryBlock;
    RecyclerWatsonTelemetryBlock * telemetryBlock;

#ifdef RECYCLER_STATS
    RecyclerCollectionStats collectionStats;
    void PrintHeapBlockStats(wchar_t const * name, HeapBlock::HeapBlockType type);
    void PrintHeapBlockMemoryStats(wchar_t const * name, HeapBlock::HeapBlockType type);
    void PrintCollectStats();
    void PrintHeuristicCollectionStats();
    void PrintMarkCollectionStats();
    void PrintBackgroundCollectionStats();
    void PrintMemoryStats();
    void PrintBackgroundCollectionStat(RecyclerCollectionStats::MarkData const& markData);
#endif
#ifdef RECYCLER_TRACE
    CollectionParam collectionParam;
#endif
#ifdef RECYCLER_MEMORY_VERIFY
    uint verifyPad;
    bool verifyEnabled;
#endif

#ifdef RECYCLER_DUMP_OBJECT_GRAPH
    friend class RecyclerObjectGraphDumper;
    RecyclerObjectGraphDumper * objectGraphDumper;
public:
    bool dumpObjectOnceOnCollect;
#endif
public:

    Recycler(AllocationPolicyManager * policyManager, IdleDecommitPageAllocator * pageAllocator, void(*outOfMemoryFunc)(), Js::ConfigFlagsTable& flags);
    ~Recycler();

    void Initialize(const bool forceInThread, JsUtil::ThreadService *threadService, const bool deferThreadStartup = false
#ifdef RECYCLER_PAGE_HEAP
        , PageHeapMode pageheapmode = PageHeapMode::PageHeapModeOff
        , bool captureAllocCallStack = false
        , bool captureFreeCallStack = false
#endif
        );

    Js::ConfigFlagsTable& GetRecyclerFlagsTable() const { return this->recyclerFlagsTable; }
    void SetMemProtectMode();

    bool IsMemProtectMode()
    {
        return this->enableScanImplicitRoots;
    }

    size_t GetUsedBytes()
    {
        size_t usedBytes = threadPageAllocator->usedBytes;
#ifdef RECYCLER_WRITE_BARRIER_ALLOC_SEPARATE_PAGE
        usedBytes += recyclerWithBarrierPageAllocator.usedBytes;
#endif
        usedBytes += recyclerPageAllocator.usedBytes;
        usedBytes += recyclerLargeBlockPageAllocator.usedBytes;
        return usedBytes;
    }

    void LogMemProtectHeapSize(bool fromGC);

    char* Realloc(void* buffer, size_t existingBytes, size_t requestedBytes, bool truncate = true);
    void SetTelemetryBlock(RecyclerWatsonTelemetryBlock * telemetryBlock) { this->telemetryBlock = telemetryBlock; }

    void Prime();

    void* GetOwnerContext() { return (void*) this->collectionWrapper; }
    PageAllocator * GetPageAllocator() { return threadPageAllocator; }

    bool NeedOOMRescan() const
    {
        return this->needOOMRescan;
    }

    void SetNeedOOMRescan()
    {
        this->needOOMRescan = true;
    }

    void ClearNeedOOMRescan()
    {
        this->needOOMRescan = false;
        markContext.GetPageAllocator()->ResetDisableAllocationOutOfMemory();
        parallelMarkContext1.GetPageAllocator()->ResetDisableAllocationOutOfMemory();
        parallelMarkContext2.GetPageAllocator()->ResetDisableAllocationOutOfMemory();
        parallelMarkContext3.GetPageAllocator()->ResetDisableAllocationOutOfMemory();
    }

    BOOL RequestConcurrentWrapperCallback();

    BOOL CollectionInProgress() const
    {
        return collectionState != CollectionStateNotCollecting;
    }

    BOOL IsExiting() const
    {
        return (collectionState == Collection_Exit);
    }

    BOOL IsSweeping() const
    {
        return ((collectionState & Collection_Sweep) == Collection_Sweep);
    }

#ifdef RECYCLER_PAGE_HEAP
    __inline bool ShouldCapturePageHeapFreeStack() const { return capturePageHeapFreeStack; }
#else
    __inline bool ShouldCapturePageHeapFreeStack() const { return false; }
#endif

    void SetIsThreadBound();
    void SetIsScriptActive(bool isScriptActive)
    {
        Assert(this->isInScript);
        Assert(this->isScriptActive != isScriptActive);
        this->isScriptActive = isScriptActive;
        if (isScriptActive)
        {
            this->tickCountNextDispose = ::GetTickCount() + RecyclerHeuristic::TickCountFinishCollection;
        }
    }
    void SetIsInScript(bool isInScript)
    {
        Assert(this->isInScript != isInScript);
        this->isInScript = isInScript;
    }

    bool ShouldIdleCollectOnExit();
    void ScheduleNextCollection();

    IdleDecommitPageAllocator * GetRecyclerLeafPageAllocator()
    {
        return this->threadPageAllocator;
    }

    IdleDecommitPageAllocator * GetRecyclerPageAllocator()
    {
        return &this->recyclerPageAllocator;
    }

    IdleDecommitPageAllocator * GetRecyclerLargeBlockPageAllocator()
    {
        return &this->recyclerLargeBlockPageAllocator;
    }

#ifdef RECYCLER_WRITE_BARRIER_ALLOC_SEPARATE_PAGE
    IdleDecommitPageAllocator * GetRecyclerWithBarrierPageAllocator()
    {
        return &this->recyclerWithBarrierPageAllocator;
    }
#endif

    BOOL IsShuttingDown() const { return this->isShuttingDown; }
#ifdef CONCURRENT_GC_ENABLED
#if DBG
    BOOL IsConcurrentMarkEnabled() const { return enableConcurrentMark; }
    BOOL IsConcurrentSweepEnabled() const { return enableConcurrentSweep; }
#endif
    template <CollectionFlags flags>
    BOOL FinishConcurrent();
    void ShutdownThread();

    bool EnableConcurrent(JsUtil::ThreadService *threadService, bool startAllThreads);
    void DisableConcurrent();

    void StartQueueTrackedObject();
    bool DoQueueTrackedObject() const;
    void PrepareSweep();
#endif

    template <CollectionFlags flags>
    void SetupPostCollectionFlags();
    void EnsureNotCollecting();

    bool QueueTrackedObject(FinalizableObject * trackableObject);

    // FindRoots
    void TryMarkNonInterior(void* candidate, void* parentReference = nullptr);
    void TryMarkInterior(void *candidate, void* parentReference = nullptr);

    bool InCacheCleanupCollection() { return inCacheCleanupCollection; }
    void ClearCacheCleanupCollection() { Assert(inCacheCleanupCollection); inCacheCleanupCollection = false; }

    // Finalizer support
    void SetExternalRootMarker(ExternalRootMarker fn, void * context)
    {
        externalRootMarker = fn;
        externalRootMarkerContext = context;
    }

    HeapInfo* CreateHeap();
    void DestroyHeap(HeapInfo* heapInfo);

    ArenaAllocator * CreateGuestArena(wchar_t const * name, void (*outOfMemoryFunc)());
    void DeleteGuestArena(ArenaAllocator * arenaAllocator);

    ArenaData ** RegisterExternalGuestArena(ArenaData* guestArena)
    {
        return externalGuestArenaList.PrependNode(&NoThrowHeapAllocator::Instance, guestArena);
    }

    void UnregisterExternalGuestArena(ArenaData* guestArena)
    {
        externalGuestArenaList.Remove(&NoThrowHeapAllocator::Instance, guestArena);
    }

    void UnregisterExternalGuestArena(ArenaData** guestArena)
    {
        externalGuestArenaList.RemoveElement(&NoThrowHeapAllocator::Instance, guestArena);
    }

#ifdef RECYCLER_TEST_SUPPORT
    void SetCheckFn(BOOL(*checkFn)(char* addr, size_t size));
#endif

    void SetCollectionWrapper(RecyclerCollectionWrapper * wrapper)
    {
        this->collectionWrapper = wrapper;
#if LARGEHEAPBLOCK_ENCODING
        this->Cookie = wrapper->GetRandomNumber();
#else
        this->Cookie = 0;
#endif
    }

    static size_t GetAlignedSize(size_t size) { return HeapInfo::GetAlignedSize(size); }

    HeapInfo* GetAutoHeap() { return &autoHeap; }

    template <CollectionFlags flags>
    BOOL CollectNow();

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    void DisplayMemStats();
#endif

    void AddExternalMemoryUsage(size_t size);

    bool NeedDispose()
    {
        return this->hasDisposableObject;
    }

    template <CollectionFlags flags>
    bool FinishDisposeObjectsNow();

    BOOL ReportExternalMemoryAllocation(size_t size);
    void ReportExternalMemoryFailure(size_t size);
    void ReportExternalMemoryFree(size_t size);

#ifdef TRACE_OBJECT_LIFETIME
#define DEFINE_RECYCLER_ALLOC_TRACE(AllocFunc, AllocWithAttributesFunc, attributes) \
    __inline char* AllocFunc##Trace(size_t size) \
    { \
        return AllocWithAttributesFunc<(ObjectInfoBits)(attributes | TraceBit), /* nothrow = */ false>(size); \
    }
#else
#define DEFINE_RECYCLER_ALLOC_TRACE(AllocFunc, AllocWithAttributeFunc, attributes)
#endif
#define DEFINE_RECYCLER_ALLOC_BASE(AllocFunc, AllocWithAttributesFunc, attributes) \
    __inline char * AllocFunc(size_t size) \
    { \
        return AllocWithAttributesFunc<attributes, /* nothrow = */ false>(size); \
    } \
    __forceinline char * AllocFunc##Inlined(size_t size) \
    { \
        return AllocWithAttributesFunc##Inlined<attributes, /* nothrow = */ false>(size);  \
    } \
    DEFINE_RECYCLER_ALLOC_TRACE(AllocFunc, AllocWithAttributesFunc, attributes);

#define DEFINE_RECYCLER_NOTHROW_ALLOC_BASE(AllocFunc, AllocWithAttributesFunc, attributes) \
    __inline char * NoThrow##AllocFunc(size_t size) \
    { \
        return AllocWithAttributesFunc<attributes, /* nothrow = */ true>(size); \
    } \
    __inline char * NoThrow##AllocFunc##Inlined(size_t size) \
    { \
        return AllocWithAttributesFunc##Inlined<attributes, /* nothrow = */ true>(size);  \
    } \
    DEFINE_RECYCLER_ALLOC_TRACE(AllocFunc, AllocWithAttributesFunc, attributes);

#define DEFINE_RECYCLER_ALLOC(AllocFunc, attributes) DEFINE_RECYCLER_ALLOC_BASE(AllocFunc, AllocWithAttributes, attributes)
#define DEFINE_RECYCLER_ALLOC_ZERO(AllocFunc, attributes) DEFINE_RECYCLER_ALLOC_BASE(AllocFunc, AllocZeroWithAttributes, attributes)

#define DEFINE_RECYCLER_NOTHROW_ALLOC(AllocFunc, attributes) DEFINE_RECYCLER_NOTHROW_ALLOC_BASE(AllocFunc, AllocWithAttributes, attributes)
#define DEFINE_RECYCLER_NOTHROW_ALLOC_ZERO(AllocFunc, attributes) DEFINE_RECYCLER_NOTHROW_ALLOC_BASE(AllocFunc, AllocZeroWithAttributes, attributes)

    DEFINE_RECYCLER_ALLOC(Alloc, NoBit);
#ifdef RECYCLER_WRITE_BARRIER_ALLOC
    DEFINE_RECYCLER_ALLOC(AllocWithBarrier, WithBarrierBit);
    DEFINE_RECYCLER_ALLOC(AllocFinalizedWithBarrier, FinalizableWithBarrierObjectBits);
#endif
    DEFINE_RECYCLER_ALLOC(AllocFinalized, FinalizableObjectBits);
    DEFINE_RECYCLER_ALLOC(AllocFinalizedClientTracked, ClientFinalizableObjectBits);
    // All trackable object are client trackable
    DEFINE_RECYCLER_ALLOC(AllocTracked, ClientTrackableObjectBits);
    DEFINE_RECYCLER_ALLOC(AllocLeaf, LeafBit);
    DEFINE_RECYCLER_ALLOC(AllocFinalizedLeaf, FinalizableLeafBits);
    DEFINE_RECYCLER_ALLOC(AllocTrackedLeaf, ClientTrackableLeafBits);
    DEFINE_RECYCLER_ALLOC_ZERO(AllocZero, NoBit);
#ifdef RECYCLER_WRITE_BARRIER_ALLOC
    DEFINE_RECYCLER_ALLOC_ZERO(AllocZeroWithBarrier, WithBarrierBit);
#endif
    DEFINE_RECYCLER_ALLOC_ZERO(AllocLeafZero, LeafBit);

    DEFINE_RECYCLER_ALLOC_ZERO(AllocZeroTrackedLeaf, ClientTrackableLeafBits);

    DEFINE_RECYCLER_NOTHROW_ALLOC_ZERO(AllocImplicitRootLeaf, ImplicitRootLeafBits);
    DEFINE_RECYCLER_NOTHROW_ALLOC_ZERO(AllocImplicitRoot, ImplicitRootBit);

    template <ObjectInfoBits enumClass>
    char * AllocEnumClass(size_t size)
    {
        Assert((enumClass & EnumClassMask) != 0);
        Assert((enumClass & ~EnumClassMask) == 0);
        return AllocWithAttributes<(ObjectInfoBits)(enumClass), /* nothrow = */ false>(size);
    }

    template <ObjectInfoBits infoBits>
    char * AllocWithInfoBits(size_t size)
    {
        return AllocWithAttributes<infoBits, /* nothrow = */ false>(size);
    }

    template<typename T>
    RecyclerWeakReference<T>* CreateWeakReferenceHandle(T* pStrongReference);
    uint GetWeakReferenceCleanupId() const { return weakReferenceCleanupId; }

    template<typename T>
    bool FindOrCreateWeakReferenceHandle(T* pStrongReference, RecyclerWeakReference<T> **ppWeakRef);

    template<typename T>
    bool TryGetWeakReferenceHandle(T* pStrongReference, RecyclerWeakReference<T> **weakReference);

    template <ObjectInfoBits attributes>
    char* GetAddressOfAllocator(size_t sizeCat)
    {
        Assert(HeapInfo::IsAlignedSmallObjectSize(sizeCat));
        return (char*)this->autoHeap.GetBucket<attributes>(sizeCat).GetAllocator();
    }

    template <ObjectInfoBits attributes>
    uint32 GetEndAddressOffset(size_t sizeCat)
    {
        Assert(HeapInfo::IsAlignedSmallObjectSize(sizeCat));
        return this->autoHeap.GetBucket<attributes>(sizeCat).GetAllocator()->GetEndAddressOffset();
    }

    template <ObjectInfoBits attributes>
    uint32 GetFreeObjectListOffset(size_t sizeCat)
    {
        Assert(HeapInfo::IsAlignedSmallObjectSize(sizeCat));
        return this->autoHeap.GetBucket<attributes>(sizeCat).GetAllocator()->GetFreeObjectListOffset();
    }

    void GetNormalHeapBlockAllocatorInfoForNativeAllocation(size_t sizeCat, void*& allocatorAddress, uint32& endAddressOffset, uint32& freeListOffset);
    bool AllowNativeCodeBumpAllocation();
    static void TrackNativeAllocatedMemoryBlock(Recycler * recycler, void * memBlock, size_t sizeCat);

    void Free(void* buffer, size_t size)
    {
        Assert(false);
    }

    bool ExplicitFreeLeaf(void* buffer, size_t size);
    bool ExplicitFreeNonLeaf(void* buffer, size_t size);

    template <ObjectInfoBits attributes>
    bool ExplicitFreeInternalWrapper(void* buffer, size_t allocSize);

    template <ObjectInfoBits attributes, typename TBlockAttributes>
    bool ExplicitFreeInternal(void* buffer, size_t size, size_t sizeCat);

    size_t GetAllocSize(size_t size);

    template <typename TBlockAttributes>
    void SetExplicitFreeBitOnSmallBlock(HeapBlock* heapBlock, size_t sizeCat, void* buffer, ObjectInfoBits attributes);

    char* HeapAllocR(HeapInfo* eHeap, size_t size)
    {
        return RealAlloc<LeafBit, /* nothrow = */ false>(eHeap, size);
    }

    void HeapFree(HeapInfo* eHeap,void* candidate);

    void EnumerateObjects(ObjectInfoBits infoBits, void (*CallBackFunction)(void * address, size_t size));

    void RootAddRef(void* obj, uint *count = nullptr);
    void RootRelease(void* obj, uint *count = nullptr);

    template <ObjectInfoBits attributes, bool nothrow>
    __inline char* RealAlloc(HeapInfo* heap, size_t size);

    template <ObjectInfoBits attributes, bool isSmallAlloc, bool nothrow>
    __inline char* RealAllocFromBucket(HeapInfo* heap, size_t size);

    void EnterIdleDecommit();
    void LeaveIdleDecommit();

    void DisposeObjects();
    BOOL IsValidObject(void* candidate, size_t minimumSize = 0);

#if DBG
    void SetDisableThreadAccessCheck();
    void SetDisableConcurentThreadExitedCheck();
    void CheckAllocExternalMark() const;
    BOOL IsFreeObject(void * candidate);
    BOOL IsReentrantState() const;
#endif
#if DBG_DUMP
    void PrintMarkStack();
#endif

#ifdef PROFILE_EXEC
    Js::Profiler * GetProfiler() const { return this->profiler; }
    ArenaAllocator * AddBackgroundProfilerArena();
    void ReleaseBackgroundProfilerArena(ArenaAllocator * arena);
    void SetProfiler(Js::Profiler * profiler, Js::Profiler * backgroundProfiler);
#endif
#ifdef RECYCLER_MEMORY_VERIFY
    BOOL VerifyEnabled() const { return verifyEnabled; }
    void Verify(Js::Phase phase);

    static void VerifyCheck(BOOL cond, wchar_t const * msg, void * address, void * corruptedAddress);
    static void VerifyCheckFill(void * address, size_t size);
    void FillCheckPad(void * address, size_t size, size_t alignedAllocSize, bool objectAlreadyInitialized);
    void FillCheckPad(void * address, size_t size, size_t alignedAllocSize)
    {
        FillCheckPad(address, size, alignedAllocSize, false);
    }

    void VerifyCheckPad(void * address, size_t size);
    void VerifyCheckPadExplicitFreeList(void * address, size_t size);
    static const byte VerifyMemFill = 0xCA;
#endif
#ifdef RECYCLER_ZERO_MEM_CHECK
    void VerifyZeroFill(void * address, size_t size);
#endif
#ifdef RECYCLER_DUMP_OBJECT_GRAPH
    bool DumpObjectGraph(RecyclerObjectGraphDumper::Param * param = nullptr);
    void DumpObjectDescription(void *object);
#endif
#ifdef LEAK_REPORT
    void ReportLeaks();
    void ReportLeaksOnProcessDetach();
#endif
#ifdef CHECK_MEMORY_LEAK
    void CheckLeaks(wchar_t const * header);
    void CheckLeaksOnProcessDetach(wchar_t const * header);
#endif
#ifdef RECYCLER_TRACE
    void SetDomCollect(bool isDomCollect)
    {
        collectionParam.domCollect = isDomCollect;
    }
    void CaptureCollectionParam(CollectionFlags flags, bool repeat = false);
#endif
#ifdef ENABLE_BASIC_TELEMETRY
    Js::GCPauseStats GetGCPauseStats()
    {
        return gcTel.GetGCPauseStats(); // returns the maxGCpause time in ms
    }

    void ResetGCPauseStats()
    {
        gcTel.Reset();
    }

    void SetIsScriptSiteCloseGC(bool val)
    {
        gcTel.SetIsScriptSiteCloseGC(val);
    }
#endif
private:
    // RecyclerRootPtr has implicit conversion to pointers, prevent it to be
    // passed to RootAddRef/RootRelease directly
    template <typename T>
    void RootAddRef(RecyclerRootPtr<T>& ptr, uint *count = nullptr);
    template <typename T>
    void RootRelease(RecyclerRootPtr<T>& ptr, uint *count = nullptr);

    template <CollectionFlags flags>
    BOOL CollectInternal();
    template <CollectionFlags flags>
    BOOL Collect();
    template <CollectionFlags flags>
    BOOL CollectWithHeuristic();
    template <CollectionFlags flags>
    BOOL CollectWithExhaustiveCandidate();
    template <CollectionFlags flags>
    BOOL GetPartialFlag();

    bool NeedExhaustiveRepeatCollect() const;

#if DBG
    bool ExpectStackSkip() const;
#endif

    static size_t const InvalidScanRootBytes = (size_t)-1;

    // Small Allocator
    template <typename SmallHeapBlockAllocatorType>
    void AddSmallAllocator(SmallHeapBlockAllocatorType * allocator, size_t sizeCat);
    template <typename SmallHeapBlockAllocatorType>
    void RemoveSmallAllocator(SmallHeapBlockAllocatorType * allocator, size_t sizeCat);
    template <ObjectInfoBits attributes, typename SmallHeapBlockAllocatorType>
    char * SmallAllocatorAlloc(SmallHeapBlockAllocatorType * allocator, size_t sizeCat);

    // Allocation
    template <ObjectInfoBits attributes, bool nothrow>
    __inline char * AllocWithAttributesInlined(size_t size);
    template <ObjectInfoBits attributes, bool nothrow>
    char * AllocWithAttributes(size_t size)
    {
        return AllocWithAttributesInlined<attributes, nothrow>(size);
    }

    template <ObjectInfoBits attributes, bool nothrow>
    __inline char* AllocZeroWithAttributesInlined(size_t size);

    template <ObjectInfoBits attributes, bool nothrow>
    char* AllocZeroWithAttributes(size_t size)
    {
        return AllocZeroWithAttributesInlined<attributes, nothrow>(size);
    }

    char* AllocWeakReferenceEntry(size_t size)
    {
        return AllocWithAttributes<WeakReferenceEntryBits, /* nothrow = */ false>(size);
    }

    bool NeedDisposeTimed()
    {
        DWORD ticks = ::GetTickCount();
        return (ticks > tickCountNextDispose && this->hasDisposableObject);
    }

    char* TryLargeAlloc(HeapInfo* heap, size_t size, ObjectInfoBits attributes, bool nothrow);

    template <bool nothrow>
    char* LargeAlloc(HeapInfo* heap, size_t size, ObjectInfoBits attributes);
    void OutOfMemory();

    // Collection
    BOOL DoCollect(CollectionFlags flags);
    BOOL DoCollectWrapped(CollectionFlags flags);
    BOOL CollectOnAllocatorThread();

#if DBG
    void ResetThreadId();
#endif

    template <bool background>
    size_t ScanPinnedObjects();
    size_t ScanStack();
    size_t ScanArena(ArenaData * alloc, bool background);
    void ScanImplicitRoots();
    void ScanInitialImplicitRoots();
    void ScanNewImplicitRoots();
    size_t FindRoots();
    size_t TryMarkArenaMemoryBlockList(ArenaMemoryBlock * memoryBlocks);
    size_t TryMarkBigBlockList(BigBlock * memoryBlocks);
    size_t TryMarkBigBlockListWithWriteWatch(BigBlock * memoryBlocks);

    // Mark
    void ResetMarks(ResetMarkFlags flags);
    void Mark();
    bool EndMark();
    bool EndMarkCheckOOMRescan();
    void EndMarkOnLowMemory();
    void DoParallelMark();
    void DoBackgroundParallelMark();

    size_t RootMark(CollectionState markState);

    void ProcessMark(bool background);
    void ProcessParallelMark(bool background, MarkContext * markContext);
    template <bool parallel, bool interior>
    void ProcessMarkContext(MarkContext * markContext);

public:
    bool IsObjectMarked(void* candidate) { return this->heapBlockMap.IsMarked(candidate); }
#ifdef RECYCLER_STRESS
    bool StressCollectNow();
#endif
private:
    HeapBlock* FindHeapBlock(void * candidate);

    struct FindBlockCache
    {
        FindBlockCache():
            heapBlock(nullptr),
            candidate(nullptr)
        {
        }

        HeapBlock* heapBlock;
        void* candidate;
    } blockCache;

    __inline void ScanObjectInline(void ** obj, size_t byteCount);
    __inline void ScanObjectInlineInterior(void ** obj, size_t byteCount);
    __inline void ScanMemoryInline(void ** obj, size_t byteCount);
    void ScanMemory(void ** obj, size_t byteCount) { if (byteCount != 0) { ScanMemoryInline(obj, byteCount); } }
    bool AddMark(void * candidate, size_t byteCount);

    // Sweep
    bool Sweep(size_t rescanRootBytes = (size_t)-1, bool concurrent = false, bool adjustPartialHeuristics = false);
    void SweepWeakReference();
    void SweepHeap(bool concurrent, RecyclerSweep& recyclerSweep);
    void FinishSweep(RecyclerSweep& recyclerSweep);

    bool FinishDisposeObjects();
    template <CollectionFlags flags>
    bool FinishDisposeObjectsWrapped();

    // end collection
    void FinishCollection();
    void FinishCollection(bool needConcurrentSweep);
    void EndCollection();

    void ResetCollectionState();
    void ResetMarkCollectionState();
    void ResetHeuristicCounters();
    void ResetPartialHeuristicCounters();
    BOOL IsMarkState() const;
    BOOL IsFindRootsState() const;
    BOOL IsInThreadFindRootsState() const;

    template <Js::Phase phase>
    void CollectionBegin();
    template <Js::Phase phase>
    void CollectionEnd();

#ifdef PARTIAL_GC_ENABLED
    void ProcessClientTrackedObjects();
    bool PartialCollect(bool concurrent);
    void FinishPartialCollect(RecyclerSweep * recyclerSweep = nullptr);
    void ClearPartialCollect();
    void BackgroundFinishPartialCollect(RecyclerSweep * recyclerSweep);
#endif

#if defined(PARTIAL_GC_ENABLED) || defined(CONCURRENT_GC_ENABLED)
    size_t RescanMark(DWORD waitTime);
    size_t FinishMark(DWORD waitTime);
    size_t FinishMarkRescan(bool background);
    void ProcessTrackedObjects();
#endif

#ifdef CONCURRENT_GC_ENABLED
    // Concurrent GC
    BOOL IsConcurrentEnabled() const { return this->enableConcurrentMark || this->enableParallelMark || this->enableConcurrentSweep; }
    BOOL IsConcurrentMarkState() const;
    BOOL IsConcurrentMarkExecutingState() const;
    BOOL IsConcurrentResetMarksState() const;
    BOOL IsConcurrentFindRootState() const;
    BOOL IsConcurrentExecutingState() const;
    BOOL IsConcurrentSweepExecutingState() const;
    BOOL IsConcurrentState() const;
    BOOL InConcurrentSweep()
    {
        return ((collectionState & Collection_ConcurrentSweep) == Collection_ConcurrentSweep);
    }
#if DBG
    BOOL IsConcurrentFinishedState() const;
#endif // DBG

    bool InitializeConcurrent(JsUtil::ThreadService* threadService);
    bool AbortConcurrent(bool restoreState);
    void FinalizeConcurrent(bool restoreState);

    static unsigned int  StaticThreadProc(LPVOID lpParameter);
    static int ExceptFilter(LPEXCEPTION_POINTERS pEP);
    DWORD ThreadProc();

    void DoBackgroundWork(bool forceForeground = false);
    static void StaticBackgroundWorkCallback(void * callbackData);

    BOOL CollectOnConcurrentThread();
    bool StartConcurrent(CollectionState const state);
    BOOL StartBackgroundMarkCollect();
    BOOL StartSynchronousBackgroundMark();
    BOOL StartAsynchronousBackgroundMark();
    BOOL StartBackgroundMark(bool foregroundResetMark, bool foregroundFindRoots);
    BOOL StartConcurrentSweepCollect();

    template <CollectionFlags flags>
    BOOL TryFinishConcurrentCollect();
    BOOL WaitForConcurrentThread(DWORD waitTime);
    void FlushBackgroundPages();
    BOOL FinishConcurrentCollect(CollectionFlags flags);
    BOOL FinishConcurrentCollectWrapped(CollectionFlags flags);
    void BackgroundMark();
    void BackgroundResetMarks();
    void PrepareBackgroundFindRoots();
    void RevertPrepareBackgroundFindRoots();
    size_t BackgroundFindRoots();
    size_t BackgroundScanStack();
    size_t BackgroundRepeatMark();
    size_t BackgroundRescan(RescanFlags rescanFlags);
    void BackgroundResetWriteWatchAll();
    size_t BackgroundFinishMark();

    char* GetScriptThreadStackTop();

    void SweepPendingObjects(RecyclerSweep& recyclerSweep);
    void ConcurrentTransferSweptObjects(RecyclerSweep& recyclerSweep);
#ifdef PARTIAL_GC_ENABLED
    void ConcurrentPartialTransferSweptObjects(RecyclerSweep& recyclerSweep);
#endif // PARTIAL_GC_ENABLED
#endif // CONCURRENT_GC_ENABLED

    bool ForceSweepObject();
    void NotifyFree(__in char * address, size_t size);
    template <bool pageheap, typename T>
    void NotifyFree(T * heapBlock);

    void CleanupPendingUnroot();

#ifdef ENABLE_JS_ETW
    ULONG EventWriteFreeMemoryBlock(HeapBlock* heapBlock);
    void FlushFreeRecord();
    void AppendFreeMemoryETWRecord(__in char *address, size_t size);
    static const uint BulkFreeMemoryCount = 400;
    uint bulkFreeMemoryWrittenCount;
    struct ETWFreeRecord {
        char* memoryAddress;
        uint32 objectSize;
    };
    ETWFreeRecord etwFreeRecords[BulkFreeMemoryCount];
#endif

    template <ObjectInfoBits attributes>
    bool IntegrateBlock(char * blockAddress, PageSegment * segment, size_t allocSize, size_t objectSize);

    template <class TBlockAttributes> friend class SmallHeapBlockT;
    template <class TBlockAttributes> friend class SmallNormalHeapBlockT;
    template <class TBlockAttributes> friend class SmallLeafHeapBlockT;
    template <class TBlockAttributes> friend class SmallFinalizableHeapBlockT;
    friend class LargeHeapBlock;
    friend class HeapInfo;
    friend class LargeHeapBucket;

    template <typename TBlockType>
    friend class HeapBucketT;
    template <typename TBlockType>
    friend class SmallNormalHeapBucketBase;
    template <typename T, ObjectInfoBits attributes = LeafBit>
    friend class RecyclerFastAllocator;

#ifdef RECYCLER_TRACE
    void PrintCollectTrace(Js::Phase phase, bool finish = false, bool noConcurrentWork = false);
#endif
#ifdef RECYCLER_VERIFY_MARK
    void VerifyMark();
    void VerifyMarkRoots();
    void VerifyMarkStack();
    void VerifyMarkArena(ArenaData * arena);
    void VerifyMarkBigBlockList(BigBlock * memoryBlocks);
    void VerifyMarkArenaMemoryBlockList(ArenaMemoryBlock * memoryBlocks);
    void VerifyMark(void * address);
#endif
#if DBG_DUMP
    bool forceTraceMark;
#endif
    bool isHeapEnumInProgress;
#if DBG
    bool allowAllocationDuringHeapEnum;
    bool allowAllocationDuringRenentrance;
#ifdef ENABLE_PROJECTION
    bool isInRefCountTrackingForProjection;
#endif
#endif
    // There are two scenarios we allow limited allocation but disallow GC during those allocations:
    // in heapenum when we allocate PropertyRecord, and
    // in projection ExternalMark allowing allocating VarToDispEx. This is the common flag
    // while we have debug only flag for each of the two scenarios.
    bool isCollectionDisabled;

#ifdef TRACK_ALLOC
public:
    Recycler * TrackAllocInfo(TrackAllocData const& data);
    void ClearTrackAllocInfo(TrackAllocData* data = NULL);

#ifdef PROFILE_RECYCLER_ALLOC
    void PrintAllocStats();
private:
    static bool DoProfileAllocTracker();
    void InitializeProfileAllocTracker();
    void TrackUnallocated(__in  char* address, __in char *endAddress, size_t sizeCat);
    void TrackAllocCore(void * object, size_t size, const TrackAllocData& trackAllocData, bool traceLifetime = false);
    void* TrackAlloc(void * object, size_t size, const TrackAllocData& trackAllocData, bool traceLifetime = false);

    void TrackIntegrate(__in_ecount(blockSize) char * blockAddress, size_t blockSize, size_t allocSize, size_t objectSize, const TrackAllocData& trackAllocData);
    BOOL TrackFree(const char* address, size_t size);

    void TrackAllocWeakRef(RecyclerWeakReferenceBase * weakRef);
    void TrackFreeWeakRef(RecyclerWeakReferenceBase * weakRef);

    struct TrackerData
    {
        TrackerData(type_info const * typeinfo, bool isArray) : typeinfo(typeinfo), isArray(isArray),
            ItemSize(0), ItemCount(0), AllocCount(0), ReqSize(0), AllocSize(0), FreeCount(0), FreeSize(0), TraceLifetime(false)
#ifdef PERF_COUNTERS
            , counter(PerfCounter::RecyclerTrackerCounterSet::GetPerfCounter(typeinfo, isArray))
            , sizeCounter(PerfCounter::RecyclerTrackerCounterSet::GetPerfSizeCounter(typeinfo, isArray))
#endif
        {
        }

        type_info const * typeinfo;
        bool isArray;
#ifdef TRACE_OBJECT_LIFETIME
        bool TraceLifetime;
#endif

        size_t ItemSize;
        size_t ItemCount;
        int AllocCount;
        int64 ReqSize;
        int64 AllocSize;
        int FreeCount;
        int64 FreeSize;
#ifdef PERF_COUNTERS
        PerfCounter::Counter& counter;
        PerfCounter::Counter& sizeCounter;
#endif

        static TrackerData EmptyData;
        static TrackerData ExplicitFreeListObjectData;
    };
    TrackerData * GetTrackerData(void * address);
    void SetTrackerData(void * address, TrackerData * data);

    struct TrackerItem
    {
        TrackerItem(type_info const * typeinfo) : instanceData(typeinfo, false), arrayData(typeinfo, true)
#ifdef PERF_COUNTERS
            , weakRefCounter(PerfCounter::RecyclerTrackerCounterSet::GetWeakRefPerfCounter(typeinfo))
#endif
        {}
        TrackerData instanceData;
        TrackerData arrayData;
#ifdef PERF_COUNTERS
        PerfCounter::Counter& weakRefCounter;
#endif
    };

    typedef JsUtil::BaseDictionary<type_info const *, TrackerItem *, NoCheckHeapAllocator, PrimeSizePolicy, DefaultComparer, JsUtil::SimpleDictionaryEntry, JsUtil::NoResizeLock> TypeInfotoTrackerItemMap;
    typedef JsUtil::BaseDictionary<void *, TrackerData *, NoCheckHeapAllocator, PrimeSizePolicy, RecyclerPointerComparer, JsUtil::SimpleDictionaryEntry, JsUtil::NoResizeLock> PointerToTrackerDataMap;

    TypeInfotoTrackerItemMap * trackerDictionary;
    CRITICAL_SECTION trackerCriticalSection;
#endif
    TrackAllocData nextAllocData;
#endif

public:
    // Enumeration
    class AutoSetupRecyclerForNonCollectingMark
    {
    private:
        Recycler& m_recycler;
        bool m_setupDone;
        CollectionState m_previousCollectionState;
#ifdef RECYCLER_STATS
        RecyclerCollectionStats m_previousCollectionStats;
#endif
    public:
        AutoSetupRecyclerForNonCollectingMark(Recycler& recycler, bool setupForHeapEnumeration = false);
        ~AutoSetupRecyclerForNonCollectingMark();
        void DoCommonSetup();
        void SetupForHeapEnumeration();
    };

    friend class RecyclerHeapObjectInfo;

    bool FindImplicitRootObject(void* candidate, RecyclerHeapObjectInfo& heapObject);
    bool FindHeapObject(void* candidate, FindHeapObjectFlags flags, RecyclerHeapObjectInfo& heapObject);
    bool FindHeapObjectWithClearedAllocators(void* candidate, RecyclerHeapObjectInfo& heapObject);
    bool IsCollectionDisabled() const { return isCollectionDisabled; }
    bool IsHeapEnumInProgress() const { Assert(isHeapEnumInProgress ? isCollectionDisabled : true); return isHeapEnumInProgress; }

#if DBG
    // There are limited cases that we have to allow allocation during heap enumeration. GC is explicitly
    // disabled during heap enumeration for these limited cases. (See DefaultRecyclerCollectionWrapper)
    // The only case of allocation right now is allocating property record for string based type handler
    // so we can use the propertyId as the relation Id.
    // Allocation during enumeration is still frown upon and should still be avoid if possible.
    bool AllowAllocationDuringHeapEnum() const { return allowAllocationDuringHeapEnum; }
    class AutoAllowAllocationDuringHeapEnum : public AutoBooleanToggle
    {
    public:
        AutoAllowAllocationDuringHeapEnum(Recycler * recycler) : AutoBooleanToggle(&recycler->allowAllocationDuringHeapEnum) {};
    };

#ifdef ENABLE_PROJECTION
    bool IsInRefCountTrackingForProjection() const { return isInRefCountTrackingForProjection;}
    class AutoIsInRefCountTrackingForProjection : public AutoBooleanToggle
    {
    public:
        AutoIsInRefCountTrackingForProjection(Recycler * recycler) : AutoBooleanToggle(&recycler->isInRefCountTrackingForProjection) {};
    };
#endif
#endif

    class AutoAllowAllocationDuringReentrance : public AutoBooleanToggle
    {
    public:
        AutoAllowAllocationDuringReentrance(Recycler * recycler) :
            AutoBooleanToggle(&recycler->isCollectionDisabled)
#if DBG
            , allowAllocationDuringRenentrance(&recycler->allowAllocationDuringRenentrance)
#endif
        {};
#if DBG
    private:
        AutoBooleanToggle allowAllocationDuringRenentrance;
#endif
    };
#ifdef HEAP_ENUMERATION_VALIDATION
    typedef void(*PostHeapEnumScanCallback)(const HeapObject& heapObject, void *data);
    PostHeapEnumScanCallback pfPostHeapEnumScanCallback;
    void *postHeapEnunScanData;
    void PostHeapEnumScan(PostHeapEnumScanCallback callback, void*data);
    bool IsPostEnumHeapValidationInProgress() const { return pfPostHeapEnumScanCallback != NULL; }
#endif

private:
    void* GetRealAddressFromInterior(void* candidate);
    void BeginNonCollectingMark();
    void EndNonCollectingMark();

#if defined(RECYCLER_DUMP_OBJECT_GRAPH) || defined(LEAK_REPORT) || defined(CHECK_MEMORY_LEAK)
public:
    bool IsInDllCanUnloadNow() const { return inDllCanUnloadNow; }
    bool IsInDetachProcess() const { return inDetachProcess; }
    void SetInDllCanUnloadNow();
    void SetInDetachProcess();
private:
    bool inDllCanUnloadNow;
    bool inDetachProcess;
    bool isPrimaryMarkContextInitialized;
#endif
#if defined(LEAK_REPORT) || defined(CHECK_MEMORY_LEAK)
    template <class Fn>
    void ReportOnProcessDetach(Fn fn);
    void PrintPinnedObjectStackTraces();
#endif

public:
    typedef void (CALLBACK *ObjectBeforeCollectCallback)(void* object, void* callbackState); // same as jsrt JsObjectBeforeCollectCallback
    void SetObjectBeforeCollectCallback(void* object, ObjectBeforeCollectCallback callback, void* callbackState);
    void ClearObjectBeforeCollectCallbacks();
    bool IsInObjectBeforeCollectCallback() const { return objectBeforeCollectCallbackState != ObjectBeforeCollectCallback_None; }
private:
    struct ObjectBeforeCollectCallbackData
    {
        ObjectBeforeCollectCallback callback;
        void* callbackState;

        ObjectBeforeCollectCallbackData() {}
        ObjectBeforeCollectCallbackData(ObjectBeforeCollectCallback callback, void* callbackState) : callback(callback), callbackState(callbackState) {}
    };
    typedef JsUtil::BaseDictionary<void*, ObjectBeforeCollectCallbackData, HeapAllocator,
        PrimeSizePolicy, RecyclerPointerComparer, JsUtil::SimpleDictionaryEntry, JsUtil::NoResizeLock> ObjectBeforeCollectCallbackMap;
    ObjectBeforeCollectCallbackMap* objectBeforeCollectCallbackMap;

    enum ObjectBeforeCollectCallbackState
    {
      ObjectBeforeCollectCallback_None,
      ObjectBeforeCollectCallback_Normal,   // Normal GC BeforeCollect callback
      ObjectBeforeCollectCallback_Shutdown, // At shutdown invoke all BeforeCollect callback
    } objectBeforeCollectCallbackState;

    bool ProcessObjectBeforeCollectCallbacks(bool atShutdown = false);
};


class RecyclerHeapObjectInfo
{
    void* m_address;
    Recycler * m_recycler;
    HeapBlock* m_heapBlock;

#if LARGEHEAPBLOCK_ENCODING
    union
    {
        byte * m_attributes;
        LargeObjectHeader * m_largeHeapBlockHeader;
    };
    bool isUsingLargeHeapBlock = false;
#else
    byte * m_attributes;
#endif


public:
    RecyclerHeapObjectInfo() : m_address(NULL), m_recycler(NULL), m_heapBlock(NULL), m_attributes(NULL) {}
    RecyclerHeapObjectInfo(void* address, Recycler * recycler, HeapBlock* heapBlock, byte * attributes) :
        m_address(address), m_recycler(recycler), m_heapBlock(heapBlock), m_attributes(attributes) { }

    void* GetObjectAddress() const { return m_address; }

    bool IsLeaf() const
    {
#if LARGEHEAPBLOCK_ENCODING
        if (isUsingLargeHeapBlock)
        {
            return (m_largeHeapBlockHeader->GetAttributes(m_recycler->Cookie) & LeafBit) != 0;
        }
#endif
        return ((*m_attributes & LeafBit) != 0 || this->m_heapBlock->IsLeafBlock());
    }

    bool IsImplicitRoot() const
    {
#if LARGEHEAPBLOCK_ENCODING
        if (isUsingLargeHeapBlock)
        {
            return (m_largeHeapBlockHeader->GetAttributes(m_recycler->Cookie) & ImplicitRootBit) != 0;
        }
#endif
        return (*m_attributes & ImplicitRootBit) != 0;
    }
    bool IsObjectMarked() const { Assert(m_recycler); return m_recycler->heapBlockMap.IsMarked(m_address); }
    void SetObjectMarked()  { Assert(m_recycler); m_recycler->heapBlockMap.SetMark(m_address); }
    ObjectInfoBits GetAttributes() const
    {
#if LARGEHEAPBLOCK_ENCODING
        if (isUsingLargeHeapBlock)
        {
            return (ObjectInfoBits)m_largeHeapBlockHeader->GetAttributes(m_recycler->Cookie);
        }
#endif
        return (ObjectInfoBits)*m_attributes;
    }
    size_t GetSize() const;

#if LARGEHEAPBLOCK_ENCODING
    void SetLargeHeapBlockHeader(LargeObjectHeader * largeHeapBlockHeader)
    {
        m_largeHeapBlockHeader = largeHeapBlockHeader;
        isUsingLargeHeapBlock = true;
    }
#endif

    bool SetMemoryProfilerHasEnumerated()
    {
        Assert(m_heapBlock);
#if LARGEHEAPBLOCK_ENCODING
        if (isUsingLargeHeapBlock)
        {
            return SetMemoryProfilerHasEnumeratedForLargeHeapBlock();
        }
#endif
        bool wasMemoryProfilerOldObject = (*m_attributes & MemoryProfilerOldObjectBit) != 0;
        *m_attributes |= MemoryProfilerOldObjectBit;
        return wasMemoryProfilerOldObject;
    }

    bool ClearImplicitRootBit()
    {
        // This can only be called on the main thread for non-finalizable block
        // As finalizable block requires that the bit not be change during concurrent mark
        // since the background thread change the NewTrackBit
        Assert(!m_heapBlock->IsAnyFinalizableBlock());

#ifdef RECYCLER_PAGE_HEAP
        Recycler* recycler = this->m_recycler;
        if (recycler->ShouldCapturePageHeapFreeStack())
        {
            Assert(recycler->IsPageHeapEnabled());

            this->m_heapBlock->CapturePageHeapFreeStack();
        }
#endif

#if LARGEHEAPBLOCK_ENCODING
        if (isUsingLargeHeapBlock)
        {
            return ClearImplicitRootBitsForLargeHeapBlock();
        }
#endif
        Assert(m_attributes);
        bool wasImplicitRoot = (*m_attributes & ImplicitRootBit) != 0;
        *m_attributes &= ~ImplicitRootBit;

        return wasImplicitRoot;
    }

    void ExplicitFree()
    {
        if (*m_attributes == ObjectInfoBits::LeafBit)
        {
            m_recycler->ExplicitFreeLeaf(m_address, GetSize());
        }
        else
        {
            Assert(*m_attributes == ObjectInfoBits::NoBit);
            m_recycler->ExplicitFreeNonLeaf(m_address, GetSize());
        }
    }

#if LARGEHEAPBLOCK_ENCODING
    bool ClearImplicitRootBitsForLargeHeapBlock()
    {
        Assert(m_largeHeapBlockHeader);
        byte attributes = m_largeHeapBlockHeader->GetAttributes(m_recycler->Cookie);
        bool wasImplicitRoot = (attributes & ImplicitRootBit) != 0;
        m_largeHeapBlockHeader->SetAttributes(m_recycler->Cookie, attributes & ~ImplicitRootBit);
        return wasImplicitRoot;
    }

    bool SetMemoryProfilerHasEnumeratedForLargeHeapBlock()
    {
        Assert(m_largeHeapBlockHeader);
        byte attributes = m_largeHeapBlockHeader->GetAttributes(m_recycler->Cookie);
        bool wasMemoryProfilerOldObject = (attributes & MemoryProfilerOldObjectBit) != 0;
        m_largeHeapBlockHeader->SetAttributes(m_recycler->Cookie, attributes | MemoryProfilerOldObjectBit);
        return wasMemoryProfilerOldObject;
    }

#endif
};
// A fake heap block to replace the original heap block where the strong ref is when it has been collected
// as the original heap block may have been freed
class CollectedRecyclerWeakRefHeapBlock : public HeapBlock
{
public:
#if DBG
    virtual BOOL IsFreeObject(void* objectAddress) override { Assert(false); return false; }
#endif
    virtual BOOL IsValidObject(void* objectAddress) override { Assert(false); return false; }
    virtual byte* GetRealAddressFromInterior(void* interiorAddress) override { Assert(false); return nullptr; }
    virtual size_t GetObjectSize(void* object) override { Assert(false); return 0; }
    virtual bool FindHeapObject(void* objectAddress, Recycler * recycler, FindHeapObjectFlags flags, RecyclerHeapObjectInfo& heapObject) override { Assert(false); return false; }
    virtual bool TestObjectMarkedBit(void* objectAddress) override { Assert(false); return false; }
    virtual void SetObjectMarkedBit(void* objectAddress) override { Assert(false); }

#ifdef RECYCLER_VERIFY_MARK
    virtual void VerifyMark(void * objectAddress) override { Assert(false); }
#endif
#ifdef RECYCLER_PERF_COUNTERS
    virtual void UpdatePerfCountersOnFree() override { Assert(false); }
#endif
#ifdef PROFILE_RECYCLER_ALLOC
    virtual void * GetTrackerData(void * address) override { Assert(false); return nullptr; }
    virtual void SetTrackerData(void * address, void * data) override { Assert(false); }
#endif
    static CollectedRecyclerWeakRefHeapBlock Instance;
private:

    CollectedRecyclerWeakRefHeapBlock() : HeapBlock(BlockTypeCount) { isPendingConcurrentSweep = false; }
};

class AutoIdleDecommit
{
public:
    AutoIdleDecommit(Recycler * recycler) : recycler(recycler) { recycler->EnterIdleDecommit(); }
    ~AutoIdleDecommit() { recycler->LeaveIdleDecommit(); }
private:
    Recycler * recycler;
};

template <typename SmallHeapBlockAllocatorType>
void
Recycler::AddSmallAllocator(SmallHeapBlockAllocatorType * allocator, size_t sizeCat)
{
    autoHeap.AddSmallAllocator(allocator, sizeCat);
}

template <typename SmallHeapBlockAllocatorType>
void
Recycler::RemoveSmallAllocator(SmallHeapBlockAllocatorType * allocator, size_t sizeCat)
{
    autoHeap.RemoveSmallAllocator(allocator, sizeCat);
}

template <ObjectInfoBits attributes, typename SmallHeapBlockAllocatorType>
char *
Recycler::SmallAllocatorAlloc(SmallHeapBlockAllocatorType * allocator, size_t sizeCat)
{
    return autoHeap.SmallAllocatorAlloc<attributes>(this, allocator, sizeCat);
}

// Dummy recycler allocator policy classes to choose the allocation function
class _RecyclerLeafPolicy;
class _RecyclerNonLeafPolicy;

template <typename Policy>
class _RecyclerAllocatorFunc
{};

template <>
class _RecyclerAllocatorFunc<_RecyclerLeafPolicy>
{
public:
    typedef char * (Recycler::*AllocFuncType)(size_t);
    typedef bool (Recycler::*FreeFuncType)(void*, size_t);

    static AllocFuncType GetAllocFunc()
    {
        return &Recycler::AllocLeaf;
    }

    static AllocFuncType GetAllocZeroFunc()
    {
        return &Recycler::AllocLeafZero;
    }

    static FreeFuncType GetFreeFunc()
    {
        return &Recycler::ExplicitFreeLeaf;
    }
};

template <>
class _RecyclerAllocatorFunc<_RecyclerNonLeafPolicy>
{
public:
    typedef char * (Recycler::*AllocFuncType)(size_t);
    typedef bool (Recycler::*FreeFuncType)(void*, size_t);

    static AllocFuncType GetAllocFunc()
    {
        return &Recycler::Alloc;
    }

    static AllocFuncType GetAllocZeroFunc()
    {
        return &Recycler::AllocZero;
    }

    static FreeFuncType GetFreeFunc()
    {
        return &Recycler::ExplicitFreeNonLeaf;
    }
};

// This is used by the compiler; when T is NOT a pointer i.e. a value type - it causes leaf allocation
template <typename T>
class TypeAllocatorFunc<Recycler, T> : public _RecyclerAllocatorFunc<_RecyclerLeafPolicy>
{
};

// Partial template specialization; applies to T when it is a pointer
template <typename T>
class TypeAllocatorFunc<Recycler, T *> : public _RecyclerAllocatorFunc<_RecyclerNonLeafPolicy>
{
};

template <bool isLeaf>
class ListTypeAllocatorFunc<Recycler, isLeaf>
{
public:
    typedef bool (Recycler::*FreeFuncType)(void*, size_t);

    static FreeFuncType GetFreeFunc()
    {
        if (isLeaf)
        {
            return &Recycler::ExplicitFreeLeaf;
        }
        else
        {
            return &Recycler::ExplicitFreeNonLeaf;
        }
    }
};

// Dummy class to choose the allocation function
class RecyclerLeafAllocator;
class RecyclerNonLeafAllocator;

// Partial template specialization to allocate as non leaf
template <typename T>
class TypeAllocatorFunc<RecyclerNonLeafAllocator, T> : public _RecyclerAllocatorFunc<_RecyclerNonLeafPolicy>
{
};

template <typename T>
class TypeAllocatorFunc<RecyclerLeafAllocator, T> : public _RecyclerAllocatorFunc<_RecyclerLeafPolicy>
{
};

template <typename TAllocType>
struct AllocatorInfo<Recycler, TAllocType>
{
    typedef Recycler AllocatorType;
    typedef TypeAllocatorFunc<Recycler, TAllocType> AllocatorFunc;
    typedef _RecyclerAllocatorFunc<_RecyclerNonLeafPolicy> InstAllocatorFunc; // By default any instance considered non-leaf
};

template <typename TAllocType>
struct AllocatorInfo<RecyclerNonLeafAllocator, TAllocType>
{
    typedef Recycler AllocatorType;
    typedef TypeAllocatorFunc<RecyclerNonLeafAllocator, TAllocType> AllocatorFunc;
    typedef TypeAllocatorFunc<RecyclerNonLeafAllocator, TAllocType> InstAllocatorFunc; // Same as TypeAllocatorFunc
};

template <typename TAllocType>
struct AllocatorInfo<RecyclerLeafAllocator, TAllocType>
{
    typedef Recycler AllocatorType;
    typedef TypeAllocatorFunc<RecyclerLeafAllocator, TAllocType> AllocatorFunc;
    typedef TypeAllocatorFunc<RecyclerLeafAllocator, TAllocType> InstAllocatorFunc; // Same as TypeAllocatorFunc
};

template <>
struct ForceNonLeafAllocator<Recycler>
{
    typedef RecyclerNonLeafAllocator AllocatorType;
};

template <>
struct ForceNonLeafAllocator<RecyclerLeafAllocator>
{
    typedef RecyclerNonLeafAllocator AllocatorType;
};

template <>
struct ForceLeafAllocator<Recycler>
{
    typedef RecyclerLeafAllocator AllocatorType;
};

template <>
struct ForceLeafAllocator<RecyclerNonLeafAllocator>
{
    typedef RecyclerLeafAllocator AllocatorType;
};

#ifdef PROFILE_EXEC
#define RECYCLER_PROFILE_EXEC_BEGIN(recycler, phase) if (recycler->profiler != nullptr) { recycler->profiler->Begin(phase); }
#define RECYCLER_PROFILE_EXEC_END(recycler, phase) if (recycler->profiler != nullptr) { recycler->profiler->End(phase); }

#define RECYCLER_PROFILE_EXEC_BEGIN2(recycler, phase1, phase2) if (recycler->profiler != nullptr) { recycler->profiler->Begin(phase1); recycler->profiler->Begin(phase2);}
#define RECYCLER_PROFILE_EXEC_END2(recycler, phase1, phase2) if (recycler->profiler != nullptr) { recycler->profiler->End(phase1); recycler->profiler->End(phase2);}
#define RECYCLER_PROFILE_EXEC_CHANGE(recydler, phase1, phase2) if  (recycler->profiler != nullptr) { recycler->profiler->End(phase1); recycler->profiler->Begin(phase2); }
#define RECYCLER_PROFILE_EXEC_BACKGROUND_BEGIN(recycler, phase) if (recycler->backgroundProfiler != nullptr) { recycler->backgroundProfiler->Begin(phase); }
#define RECYCLER_PROFILE_EXEC_BACKGROUND_END(recycler, phase) if (recycler->backgroundProfiler != nullptr) { recycler->backgroundProfiler->End(phase); }

#define RECYCLER_PROFILE_EXEC_THREAD_BEGIN(background, recycler, phase) if (background) { RECYCLER_PROFILE_EXEC_BACKGROUND_BEGIN(recycler, phase); } else { RECYCLER_PROFILE_EXEC_BEGIN(recycler, phase); }
#define RECYCLER_PROFILE_EXEC_THREAD_END(background, recycler, phase) if (background) { RECYCLER_PROFILE_EXEC_BACKGROUND_END(recycler, phase); } else { RECYCLER_PROFILE_EXEC_END(recycler, phase); }
#else
#define RECYCLER_PROFILE_EXEC_BEGIN(recycler, phase)
#define RECYCLER_PROFILE_EXEC_END(recycler, phase)
#define RECYCLER_PROFILE_EXEC_BEGIN2(recycler, phase1, phase2)
#define RECYCLER_PROFILE_EXEC_END2(recycler, phase1, phase2)
#define RECYCLER_PROFILE_EXEC_CHANGE(recydler, phase1, phase2)
#define RECYCLER_PROFILE_EXEC_BACKGROUND_BEGIN(recycler, phase)
#define RECYCLER_PROFILE_EXEC_BACKGROUND_END(recycler, phase)
#define RECYCLER_PROFILE_EXEC_THREAD_BEGIN(background, recycler, phase)
#define RECYCLER_PROFILE_EXEC_THREAD_END(background, recycler, phase)
#endif
}

_Ret_notnull_ inline void * __cdecl
operator new(size_t byteSize, Recycler * alloc, HeapInfo * heapInfo)
{
    return alloc->HeapAllocR(heapInfo, byteSize);
}

inline void __cdecl
operator delete(void * obj, Recycler * alloc, HeapInfo * heapInfo)
{
    alloc->HeapFree(heapInfo, obj);
}

_Ret_notnull_ inline void * __cdecl
operator new(size_t byteSize, Recycler * recycler, ObjectInfoBits enumClassBits)
{
    AssertCanHandleOutOfMemory();
    Assert(byteSize != 0);
    Assert(enumClassBits == EnumClass_1_Bit);
    void * buffer = recycler->AllocEnumClass<EnumClass_1_Bit>(byteSize);
    // All of our allocation should throw on out of memory
    Assume(buffer != nullptr);
    return buffer;
}

template<ObjectInfoBits infoBits>
_Ret_notnull_ inline void * __cdecl
operator new(size_t byteSize, Recycler * recycler, const InfoBitsWrapper<infoBits>&)
{
    AssertCanHandleOutOfMemory();
    Assert(byteSize != 0);
    void * buffer = recycler->AllocWithInfoBits<infoBits>(byteSize);
    // All of our allocation should throw on out of memory
    Assume(buffer != nullptr);
    return buffer;
}
