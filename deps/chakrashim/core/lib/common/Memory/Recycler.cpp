//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonMemoryPch.h"
#ifdef CONCURRENT_GC_ENABLED
#include <process.h>
#endif

#ifdef _M_AMD64
#include "amd64.h"
#endif

#ifdef _M_ARM
#include "arm.h"
#endif

#ifdef _M_ARM64
#include "arm64.h"
#endif

#include "core\BinaryFeatureControl.h"
#include "Common\ThreadService.h"
#include "Memory\AutoAllocatorObjectPtr.h"

DEFINE_RECYCLER_TRACKER_PERF_COUNTER(RecyclerWeakReferenceBase);

#ifdef PROFILE_RECYCLER_ALLOC
struct UnallocatedPortionOfBumpAllocatedBlock
{
};

struct ExplicitFreeListedObject
{
};

Recycler::TrackerData Recycler::TrackerData::EmptyData(&typeid(UnallocatedPortionOfBumpAllocatedBlock), false);
Recycler::TrackerData Recycler::TrackerData::ExplicitFreeListObjectData(&typeid(ExplicitFreeListedObject), false);
#endif

enum ETWEventGCActivationKind : unsigned
{
    ETWEvent_GarbageCollect          = 0,      // force in-thread GC
    ETWEvent_ThreadCollect           = 1,      // thread GC with wait
    ETWEvent_ConcurrentCollect       = 2,
    ETWEvent_PartialCollect          = 3,

    ETWEvent_ConcurrentMark          = 11,
    ETWEvent_ConcurrentRescan        = 12,
    ETWEvent_ConcurrentSweep         = 13,
    ETWEvent_ConcurrentTransferSwept = 14,
    ETWEvent_ConcurrentFinishMark    = 15,
};

DefaultRecyclerCollectionWrapper DefaultRecyclerCollectionWrapper::Instance;

__inline bool
DefaultRecyclerCollectionWrapper::IsCollectionDisabled(Recycler * recycler)
{
    // GC shouldn't be triggered during heap enum, unless we missed a case where it allocate memory (which
    // shouldn't happen during heap enum) or for the case we explicitly allow allocation
    // REVIEW: isHeapEnumInProgress should have been a collection state and checked before to avoid a check here.
    // Collection will be disabled in VarDispEx because it could be called from projection re-entrance as ASTA allows
    // QI/AddRef/Release to come back.
    bool collectionDisabled = recycler->IsCollectionDisabled();
#if DBG
    if (collectionDisabled)
    {
        // disabled collection should only happen if we allowed allocation during heap enum
        if (recycler->IsHeapEnumInProgress())
        {
            Assert(recycler->AllowAllocationDuringHeapEnum());
        }
        else
        {
#ifdef ENABLE_PROJECTION
            Assert(recycler->IsInRefCountTrackingForProjection());
#else
            Assert(false);
#endif
        }
    }
#endif
    return collectionDisabled;
}


BOOL DefaultRecyclerCollectionWrapper::ExecuteRecyclerCollectionFunction(Recycler * recycler, CollectionFunction function, CollectionFlags flags)
{
    if (IsCollectionDisabled(recycler))
    {
        return FALSE;
    }
    BOOL ret = FALSE;
    BEGIN_NO_EXCEPTION
    {
        ret = (recycler->*(function))(flags);
    }
    END_NO_EXCEPTION;
    return ret;
}

void
DefaultRecyclerCollectionWrapper::DisposeObjects(Recycler * recycler)
{
    if (IsCollectionDisabled(recycler))
    {
        return;
    }

    BEGIN_NO_EXCEPTION
    {
        recycler->DisposeObjects();
    }
    END_NO_EXCEPTION;
}

static void* GetStackBase();

template __forceinline char * Recycler::AllocWithAttributesInlined<NoBit, false>(size_t size);
template __forceinline char* Recycler::RealAlloc<NoBit, false>(HeapInfo* heap, size_t size);
template __forceinline _Ret_notnull_ void * __cdecl operator new<Recycler>(size_t byteSize, Recycler * alloc, char * (Recycler::*AllocFunc)(size_t));

Recycler::Recycler(AllocationPolicyManager * policyManager, IdleDecommitPageAllocator * pageAllocator, void (*outOfMemoryFunc)(), Js::ConfigFlagsTable& configFlagsTable) :
    collectionState(CollectionStateNotCollecting),
    recyclerFlagsTable(configFlagsTable),
    recyclerPageAllocator(this, policyManager, configFlagsTable, RecyclerHeuristic::Instance.DefaultMaxFreePageCount, RecyclerHeuristic::Instance.DefaultMaxAllocPageCount),
    recyclerLargeBlockPageAllocator(this, policyManager, configFlagsTable, RecyclerHeuristic::Instance.DefaultMaxFreePageCount),
    threadService(nullptr),
#ifdef RECYCLER_WRITE_BARRIER_ALLOC_SEPARATE_PAGE
    recyclerWithBarrierPageAllocator(this, policyManager, configFlagsTable, RecyclerHeuristic::Instance.DefaultMaxFreePageCount),
#endif
    threadPageAllocator(pageAllocator),
    markPagePool(configFlagsTable),
    parallelMarkPagePool1(configFlagsTable),
    parallelMarkPagePool2(configFlagsTable),
    parallelMarkPagePool3(configFlagsTable),
    markContext(this, &this->markPagePool),
    parallelMarkContext1(this, &this->parallelMarkPagePool1),
    parallelMarkContext2(this, &this->parallelMarkPagePool2),
    parallelMarkContext3(this, &this->parallelMarkPagePool3),
    clientTrackedObjectAllocator(L"CTO-List", GetPageAllocator(), Js::Throw::OutOfMemory),
    outOfMemoryFunc(outOfMemoryFunc),
#ifdef RECYCLER_TEST_SUPPORT
    checkFn(NULL),
#endif
    externalRootMarker(NULL),
    externalRootMarkerContext(NULL),
    recyclerSweep(nullptr),
    inEndMarkOnLowMemory(false),
    enableScanInteriorPointers(CUSTOM_CONFIG_FLAG(configFlagsTable, RecyclerForceMarkInterior)),
    enableScanImplicitRoots(false),
    disableCollectOnAllocationHeuristics(false),
#ifdef CONCURRENT_GC_ENABLED
    backgroundFinishMarkCount(0),
    hasPendingUnpinnedObject(false),
    hasPendingConcurrentFindRoot(false),
    queueTrackedObject(false),
    enableConcurrentMark(false),  // Default to non-concurrent
    enableParallelMark(false),
    enableConcurrentSweep(false),
    concurrentThread(NULL),
    concurrentWorkReadyEvent(NULL),
    concurrentWorkDoneEvent(NULL),
    mainThreadHandle(NULL),
    parallelThread1(this, &Recycler::ParallelWorkFunc<0>),
    parallelThread2(this, &Recycler::ParallelWorkFunc<1>),
    priorityBoost(false),
    skipStack(false),
    isAborting(false),
#if DBG
    concurrentThreadExited(true),
    isProcessingTrackedObjects(false),
    hasIncompletedDoCollect(false),
    isConcurrentGCOnIdle(false),
    isFinishGCOnIdle(false),
    isExternalStackSkippingGC(false),
    isProcessingRescan(false),
#endif
#ifdef IDLE_DECOMMIT_ENABLED
    concurrentIdleDecommitEvent(nullptr),
#endif
#endif
#ifdef PARTIAL_GC_ENABLED
    inPartialCollectMode(false),
    scanPinnedObjectMap(false),
    partialUncollectedAllocBytes(0),
    uncollectedNewPageCountPartialCollect((size_t)-1),
    partialConcurrentNextCollection(false),
#ifdef RECYCLER_STRESS
    forcePartialScanStack(false),
#endif
#endif
#if defined(RECYCLER_DUMP_OBJECT_GRAPH) || defined(LEAK_REPORT) || defined(CHECK_MEMORY_LEAK)
    isPrimaryMarkContextInitialized(false),
#endif
    allowDispose(false),
    hasDisposableObject(false),
    tickCountNextDispose(0),
    hasPendingTransferDisposedObjects(false),
    transientPinnedObject(nullptr),
    pinnedObjectMap(1024, HeapAllocator::GetNoMemProtectInstance()),
    weakReferenceMap(1024, HeapAllocator::GetNoMemProtectInstance()),
    weakReferenceCleanupId(0),
    collectionWrapper(&DefaultRecyclerCollectionWrapper::Instance),
    isScriptActive(false),
    isInScript(false),
    isShuttingDown(false),
    inExhaustiveCollection(false),
    hasExhaustiveCandidate(false),
    inDecommitNowCollection(false),
    inCacheCleanupCollection(false),
    hasPendingDeleteGuestArena(false),
    needOOMRescan(false),
    hasBackgroundFinishPartial(false),
    decommitOnFinish(false)
#ifdef PROFILE_EXEC
    , profiler(nullptr)
    , backgroundProfiler(nullptr)
    , backgroundProfilerPageAllocator(nullptr, configFlagsTable, PageAllocatorType_GCThread)
    , backgroundProfilerArena()
#endif
#ifdef PROFILE_MEM
    , memoryData(nullptr)
#endif
#ifdef RECYCLER_DUMP_OBJECT_GRAPH
    , objectGraphDumper(nullptr)
    , dumpObjectOnceOnCollect(false)
#endif
#ifdef PROFILE_RECYCLER_ALLOC
    , trackerDictionary(nullptr)
#endif
#ifdef HEAP_ENUMERATION_VALIDATION
    ,pfPostHeapEnumScanCallback(nullptr)
#endif
    , telemetryBlock(&localTelemetryBlock)
#ifdef ENABLE_JS_ETW
    ,bulkFreeMemoryWrittenCount(0)
#endif
#ifdef RECYCLER_PAGE_HEAP
    , isPageHeapEnabled(false)
    , capturePageHeapAllocStack(false)
    , capturePageHeapFreeStack(false)
#endif
    , objectBeforeCollectCallbackMap(nullptr)
    , objectBeforeCollectCallbackState(ObjectBeforeCollectCallback_None)
{
#ifdef RECYCLER_MARK_TRACK
    this->markMap = NoCheckHeapNew(MarkMap, &NoCheckHeapAllocator::Instance, 163, &markMapCriticalSection);
    markContext.SetMarkMap(markMap);
    parallelMarkContext1.SetMarkMap(markMap);
    parallelMarkContext2.SetMarkMap(markMap);
    parallelMarkContext3.SetMarkMap(markMap);
#endif

#ifdef RECYCLER_MEMORY_VERIFY
    verifyPad =  GetRecyclerFlagsTable().RecyclerVerifyPadSize;
    verifyEnabled =  GetRecyclerFlagsTable().IsEnabled(Js::RecyclerVerifyFlag);
    if (verifyEnabled)
    {
        ForRecyclerPageAllocator(EnableVerify());
    }
#endif

#ifdef RECYCLER_NO_PAGE_REUSE
    if (GetRecyclerFlagsTable().IsEnabled(Js::RecyclerNoPageReuseFlag))
    {
        ForRecyclerPageAllocator(DisablePageReuse());
    }
#endif

    this->inDispose = false;

#if DBG
    this->heapBlockCount = 0;
    this->collectionCount = 0;
    this->disableThreadAccessCheck = false;
    this->disableConcurentThreadExitedCheck = false;
#endif
#if DBG || defined RECYCLER_TRACE
    this->inResolveExternalWeakReferences = false;
#endif
#if DBG || defined(RECYCLER_STATS)
    isForceSweeping = false;
#endif
#ifdef RECYCLER_FINALIZE_CHECK
    collectionStats.finalizeCount = 0;
#endif
    RecyclerMemoryTracking::ReportRecyclerCreate(this);
#if DBG_DUMP
    forceTraceMark = false;
    recyclerPageAllocator.debugName = L"Recycler";
    recyclerLargeBlockPageAllocator.debugName = L"RecyclerLargeBlock";
#ifdef RECYCLER_WRITE_BARRIER_ALLOC_SEPARATE_PAGE
    recyclerWithBarrierPageAllocator.debugName = L"RecyclerWithBarrier";
#endif
#endif
    isHeapEnumInProgress = false;
    isCollectionDisabled = false;
#if DBG
    allowAllocationDuringRenentrance = false;
    allowAllocationDuringHeapEnum = false;
#ifdef ENABLE_PROJECTION
    isInRefCountTrackingForProjection = false;
#endif
#endif
    ScheduleNextCollection();
#if defined(RECYCLER_DUMP_OBJECT_GRAPH) ||  defined(LEAK_REPORT) || defined(CHECK_MEMORY_LEAK)
    this->inDllCanUnloadNow = false;
    this->inDetachProcess = false;
#endif

    memset(&localTelemetryBlock, 0, sizeof(localTelemetryBlock));

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    // recycler requires at least Recycler::PrimaryMarkStackReservedPageCount to function properly for the main mark context
    this->markContext.SetMaxPageCount(max<size_t>(GetRecyclerFlagsTable().MaxMarkStackPageCount, Recycler::PrimaryMarkStackReservedPageCount));
    this->parallelMarkContext1.SetMaxPageCount(GetRecyclerFlagsTable().MaxMarkStackPageCount);
    this->parallelMarkContext2.SetMaxPageCount(GetRecyclerFlagsTable().MaxMarkStackPageCount);
    this->parallelMarkContext3.SetMaxPageCount(GetRecyclerFlagsTable().MaxMarkStackPageCount);

    if (GetRecyclerFlagsTable().IsEnabled(Js::GCMemoryThresholdFlag))
    {
        // Note, we can't do this in the constructor for RecyclerHeuristic::Instance because it runs before config is processed
        RecyclerHeuristic::Instance.ConfigureBaseFactor(GetRecyclerFlagsTable().GCMemoryThreshold);
    }
#endif
}

#if DBG
void
Recycler::SetDisableThreadAccessCheck()
{
    recyclerPageAllocator.SetDisableThreadAccessCheck();
    recyclerLargeBlockPageAllocator.SetDisableThreadAccessCheck();
#ifdef RECYCLER_WRITE_BARRIER_ALLOC_SEPARATE_PAGE
    recyclerWithBarrierPageAllocator.SetDisableThreadAccessCheck();
#endif
    disableThreadAccessCheck = true;
}
#endif

void
Recycler::SetMemProtectMode()
{
    this->enableScanInteriorPointers = true;
    this->enableScanImplicitRoots = true;
    this->disableCollectOnAllocationHeuristics = true;
#ifdef RECYCLER_STRESS
    this->recyclerStress = GetRecyclerFlagsTable().MemProtectHeapStress;
#ifdef CONCURRENT_GC_ENABLED
    this->recyclerBackgroundStress = GetRecyclerFlagsTable().MemProtectHeapBackgroundStress;
    this->recyclerConcurrentStress = GetRecyclerFlagsTable().MemProtectHeapConcurrentStress;
    this->recyclerConcurrentRepeatStress = GetRecyclerFlagsTable().MemProtectHeapConcurrentRepeatStress;
#endif
#ifdef PARTIAL_GC_ENABLED
    this->recyclerPartialStress = GetRecyclerFlagsTable().MemProtectHeapPartialStress;
#endif
#endif
}

void
Recycler::LogMemProtectHeapSize(bool fromGC)
{
    Assert(IsMemProtectMode());
#ifdef ENABLE_JS_ETW
    if (IS_JS_ETW(EventEnabledMEMPROTECT_GC_HEAP_SIZE()))
    {
        IdleDecommitPageAllocator* recyclerPageAllocator = GetRecyclerPageAllocator();
        IdleDecommitPageAllocator* recyclerLeafPageAllocator = GetRecyclerLeafPageAllocator();
        IdleDecommitPageAllocator* recyclerLargeBlockPageAllocator = GetRecyclerLargeBlockPageAllocator();
#ifdef RECYCLER_WRITE_BARRIER_ALLOC_SEPARATE_PAGE
        IdleDecommitPageAllocator* recyclerWithBarrierPageAllocator = GetRecyclerWithBarrierPageAllocator();
#endif

        size_t usedBytes = (recyclerPageAllocator->usedBytes + recyclerLeafPageAllocator->usedBytes +
                            recyclerLargeBlockPageAllocator->usedBytes);
        size_t reservedBytes = (recyclerPageAllocator->reservedBytes + recyclerLeafPageAllocator->reservedBytes +
                                recyclerLargeBlockPageAllocator->reservedBytes);
        size_t committedBytes = (recyclerPageAllocator->committedBytes + recyclerLeafPageAllocator->committedBytes +
                                 recyclerLargeBlockPageAllocator->committedBytes);
        size_t numberOfSegments = (recyclerPageAllocator->numberOfSegments +
                                   recyclerLeafPageAllocator->numberOfSegments +
                                   recyclerLargeBlockPageAllocator->numberOfSegments);

#ifdef RECYCLER_WRITE_BARRIER_ALLOC_SEPARATE_PAGE
        usedBytes += recyclerWithBarrierPageAllocator->usedBytes;
        reservedBytes += recyclerWithBarrierPageAllocator->reservedBytes;
        committedBytes += recyclerWithBarrierPageAllocator->committedBytes;
        numberOfSegments += recyclerWithBarrierPageAllocator->numberOfSegments;
#endif

        JS_ETW(EventWriteMEMPROTECT_GC_HEAP_SIZE(this, usedBytes, reservedBytes, committedBytes, numberOfSegments, fromGC));
    }
#endif
}

#if DBG
void
Recycler::SetDisableConcurentThreadExitedCheck()
{
    disableConcurentThreadExitedCheck = true;
#ifdef RECYCLER_STRESS
    this->recyclerStress = false;
#ifdef CONCURRENT_GC_ENABLED
    this->recyclerBackgroundStress = false;
    this->recyclerConcurrentStress = false;
    this->recyclerConcurrentRepeatStress = false;
#endif
#ifdef PARTIAL_GC_ENABLED
    this->recyclerPartialStress = false;
#endif
#endif
}
#endif

#if DBG
void
Recycler::ResetThreadId()
{
    // Transfer all the page allocator to the current thread id
    ForRecyclerPageAllocator(ClearConcurrentThreadId());
#ifdef CONCURRENT_GC_ENABLED
    if (this->IsConcurrentEnabled())
    {
        markContext.GetPageAllocator()->ClearConcurrentThreadId();
    }
#endif
#if defined(DBG) && defined(PROFILE_EXEC)
    this->backgroundProfilerPageAllocator.ClearConcurrentThreadId();
#endif
}
#endif

Recycler::~Recycler()
{
    Assert(!this->isAborting);

    // Stop any further collection
    this->isShuttingDown = true;

#if DBG
    this->ResetThreadId();
#endif

#ifdef ENABLE_JS_ETW
    FlushFreeRecord();
#endif

    ClearObjectBeforeCollectCallbacks();

#ifdef RECYCLER_DUMP_OBJECT_GRAPH
    if (GetRecyclerFlagsTable().DumpObjectGraphOnExit)
    {
        // Always skip stack here, as we may be running the dtor on another thread.
        RecyclerObjectGraphDumper::Param param = { 0 };
        param.skipStack = true;
        this->DumpObjectGraph(&param);
    }
#endif

    AUTO_LEAK_REPORT_SECTION(this->GetRecyclerFlagsTable(), L"Recycler (%p): %s", this, this->IsInDllCanUnloadNow()? L"DllCanUnloadNow" :
        this->IsInDetachProcess()? L"DetachProcess" : L"Destructor");
#ifdef LEAK_REPORT
    ReportLeaks();
#endif

#ifdef CHECK_MEMORY_LEAK
    CheckLeaks(this->IsInDllCanUnloadNow()? L"DllCanUnloadNow" : this->IsInDetachProcess()? L"DetachProcess" : L"Destructor");
#endif

    AUTO_LEAK_REPORT_SECTION(this->GetRecyclerFlagsTable(), L"Skipped finalizers");

#ifdef CONCURRENT_GC_ENABLED
    Assert(concurrentThread == nullptr);

    // We only sometime clean up the state after abort concurrent to not collection
    // Still need to delete heap block that is held by the recyclerSweep
    if (recyclerSweep != nullptr)
    {
        recyclerSweep->ShutdownCleanup();
        recyclerSweep = nullptr;
    }

    if (mainThreadHandle != nullptr)
    {
        CloseHandle(mainThreadHandle);
    }
#endif

    recyclerPageAllocator.Close();
    recyclerLargeBlockPageAllocator.Close();
#ifdef RECYCLER_WRITE_BARRIER_ALLOC_SEPARATE_PAGE
    recyclerWithBarrierPageAllocator.Close();
#endif

    markContext.Release();
    parallelMarkContext1.Release();
    parallelMarkContext2.Release();
    parallelMarkContext3.Release();

    // Clean up the weak reference map so that
    // objects being finalized can safely refer to weak references
    // (this could otherwise become a problem for weak references held
    // to large objects since their block would be destroyed before
    // the finalizer was run)
    // When the recycler is shutting down, all objects are going to be reclaimed
    // so null out the weak references so that anyone relying on weak
    // references simply thinks the object has been reclaimed
    weakReferenceMap.Map([](RecyclerWeakReferenceBase * weakRef) -> bool
    {
        weakRef->strongRef = nullptr;

        // Put in a dummy heap block so that we can still do the isPendingConcurrentSweep check first.
        weakRef->strongRefHeapBlock = &CollectedRecyclerWeakRefHeapBlock::Instance;

        // Remove
        return false;
    });

#ifdef PARTIAL_GC_ENABLED
    clientTrackedObjectList.Clear(&this->clientTrackedObjectAllocator);
#endif

#ifdef PROFILE_RECYCLER_ALLOC
    if (trackerDictionary != nullptr)
    {
        this->trackerDictionary->Map([](type_info const *, TrackerItem * item)
        {
            NoCheckHeapDelete(item);
        });
        NoCheckHeapDelete(this->trackerDictionary);
        this->trackerDictionary = nullptr;
        ::DeleteCriticalSection(&trackerCriticalSection);
    }
#endif

#ifdef RECYCLER_MARK_TRACK
    NoCheckHeapDelete(this->markMap);
    this->markMap = nullptr;
#endif

#if DBG
    // Disable idle decommit asserts
    ForRecyclerPageAllocator(ShutdownIdleDecommit());
#endif
    Assert(this->collectionState == CollectionStateExit || this->collectionState == CollectionStateNotCollecting);
#ifdef CONCURRENT_GC_ENABLED
    Assert(this->disableConcurentThreadExitedCheck || this->concurrentThreadExited == true);
#endif
}

void
Recycler::SetIsThreadBound()
{
    Assert(mainThreadHandle == nullptr);
    ::DuplicateHandle(::GetCurrentProcess(), ::GetCurrentThread(), ::GetCurrentProcess(),  &mainThreadHandle,
        0, FALSE, DUPLICATE_SAME_ACCESS);
    stackBase = GetStackBase();
}

void
Recycler::RootAddRef(void* obj, uint *count)
{
    Assert(this->IsValidObject(obj));

    if (transientPinnedObject)
    {
        PinRecord& refCount = pinnedObjectMap.GetReference(transientPinnedObject);
        ++refCount;
        if (refCount == 1)
        {
            this->scanPinnedObjectMap = true;
            RECYCLER_PERF_COUNTER_INC(PinnedObject);
        }
#if defined(CHECK_MEMORY_LEAK) || defined(LEAK_REPORT)
        if (GetRecyclerFlagsTable().LeakStackTrace)
        {
            StackBackTraceNode::Prepend(&NoCheckHeapAllocator::Instance, refCount.stackBackTraces,
                transientPinnedObjectStackBackTrace);
        }
#endif
    }

    if (count != nullptr)
    {
        PinRecord* refCount = pinnedObjectMap.TryGetReference(obj);
        *count = (refCount != nullptr) ? (*refCount + 1) : 1;
    }

    transientPinnedObject = obj;

#if defined(CHECK_MEMORY_LEAK) || defined(LEAK_REPORT)
    if (GetRecyclerFlagsTable().LeakStackTrace)
    {
        transientPinnedObjectStackBackTrace = StackBackTrace::Capture(&NoCheckHeapAllocator::Instance);
    }
#endif
}

void
Recycler::RootRelease(void* obj, uint *count)
{
    Assert(this->IsValidObject(obj));

    if (transientPinnedObject == obj)
    {
        transientPinnedObject = nullptr;

        if (count != nullptr)
        {
            PinRecord *refCount = pinnedObjectMap.TryGetReference(obj);
            *count = (refCount != nullptr) ? *refCount : 0;
        }

#if defined(CHECK_MEMORY_LEAK) || defined(LEAK_REPORT)
        if (GetRecyclerFlagsTable().LeakStackTrace)
        {
            transientPinnedObjectStackBackTrace->Delete(&NoCheckHeapAllocator::Instance);
        }
#endif
    }
    else
    {
        PinRecord *refCount = pinnedObjectMap.TryGetReference(obj);
        if (refCount == nullptr)
        {
            if (count != nullptr)
            {
                *count = (uint)-1;
            }
            // REVIEW: throw if not found
            Assert(false);
            return;
        }

        uint newRefCount = (--(*refCount));

        if (count != nullptr)
        {
            *count = newRefCount;
        }

        if (newRefCount != 0)
        {
#if defined(CHECK_MEMORY_LEAK) || defined(LEAK_REPORT)
            if (GetRecyclerFlagsTable().LeakStackTrace)
            {
                StackBackTraceNode::Prepend(&NoCheckHeapAllocator::Instance, refCount->stackBackTraces,
                    StackBackTrace::Capture(&NoCheckHeapAllocator::Instance));
            }
#endif
            return;
        }
#if defined(CHECK_MEMORY_LEAK) || defined(LEAK_REPORT)
        StackBackTraceNode::DeleteAll(&NoCheckHeapAllocator::Instance, refCount->stackBackTraces);
        refCount->stackBackTraces = nullptr;
#endif
        // Don't delete the entry if we are in concurrent find root state
        // We will delete it later on in-thread find root
        if (this->hasPendingConcurrentFindRoot)
        {
            this->hasPendingUnpinnedObject = true;
        }
        else
        {
            pinnedObjectMap.Remove(obj);
        }

        RECYCLER_PERF_COUNTER_DEC(PinnedObject);
    }

    // Not a real collection. This doesn't activate GC.
    // This tell the GC that we have an exhaustive candidate, and should trigger
    // another GC if there is an exhaustive GC going on.
    this->CollectNow<CollectExhaustiveCandidate>();
}

void
Recycler::Initialize(const bool forceInThread, JsUtil::ThreadService *threadService, const bool deferThreadStartup
#ifdef RECYCLER_PAGE_HEAP
    , PageHeapMode pageheapmode
    , bool captureAllocCallStack
    , bool captureFreeCallStack
#endif
)
{
#ifdef PROFILE_RECYCLER_ALLOC
    this->InitializeProfileAllocTracker();
#endif
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    this->disableCollection = CUSTOM_PHASE_OFF1(GetRecyclerFlagsTable(), Js::RecyclerPhase);
#endif
#ifdef CONCURRENT_GC_ENABLED
    this->skipStack = false;
#endif

#ifdef PARTIAL_GC_ENABLED
#if ENABLE_DEBUG_CONFIG_OPTIONS
    this->enablePartialCollect = !CUSTOM_PHASE_OFF1(GetRecyclerFlagsTable(), Js::PartialCollectPhase);
#else
    this->enablePartialCollect = true;
#endif
#endif

#ifdef PROFILE_MEM
    this->memoryData = MemoryProfiler::GetRecyclerMemoryData();
#endif

#if DBG || DBG_DUMP || defined(RECYCLER_TRACE)
    mainThreadId = GetCurrentThreadContextId();
#endif

#ifdef RECYCLER_TRACE
    collectionParam.domCollect = false;
#endif

#if defined(PROFILE_RECYCLER_ALLOC) || defined(RECYCLER_MEMORY_VERIFY) || defined(MEMSPECT_TRACKING) || defined(ETW_MEMORY_TRACKING)
    bool dontNeedDetailedTracking = false;

#if defined(PROFILE_RECYCLER_ALLOC)
    dontNeedDetailedTracking = dontNeedDetailedTracking || this->trackerDictionary == nullptr;
#endif

#if defined(RECYCLER_MEMORY_VERIFY)
    dontNeedDetailedTracking = dontNeedDetailedTracking || !this->verifyEnabled;
#endif

    // If we need detailed tracking we force allocation fast path in the JIT to fail and go to the helper, so there is no
    // need for the TrackNativeAllocatedMemoryBlock callback.
    if (dontNeedDetailedTracking)
    {
        autoHeap.Initialize(this, TrackNativeAllocatedMemoryBlock
#ifdef RECYCLER_PAGE_HEAP
            , pageheapmode
            , captureAllocCallStack
            , captureFreeCallStack
#endif
            );
    }
    else
    {
        autoHeap.Initialize(this
#ifdef RECYCLER_PAGE_HEAP
            , pageheapmode
            , captureAllocCallStack
            , captureFreeCallStack
#endif
            );
    }
#else
    autoHeap.Initialize(this
#ifdef RECYCLER_PAGE_HEAP
        , pageheapmode
        , captureAllocCallStack
        , captureFreeCallStack
#endif
        );
#endif

    markContext.Init(Recycler::PrimaryMarkStackReservedPageCount);

#if defined(RECYCLER_DUMP_OBJECT_GRAPH) || defined(LEAK_REPORT) || defined(CHECK_MEMORY_LEAK)
    isPrimaryMarkContextInitialized = true;
#endif

#ifdef RECYCLER_PAGE_HEAP
    isPageHeapEnabled = autoHeap.IsPageHeapEnabled();
    if (IsPageHeapEnabled())
    {
        capturePageHeapAllocStack = autoHeap.captureAllocCallStack;
        capturePageHeapFreeStack = autoHeap.captureFreeCallStack;
    }
#endif

#ifdef RECYCLER_STRESS
    if (GetRecyclerFlagsTable().RecyclerTrackStress)
    {
        // Disable partial if we are doing track stress, since partial relies on ClientTracked processing
        // and track stress doesn't support this.
        this->enablePartialCollect = false;
    }

    this->recyclerStress = GetRecyclerFlagsTable().RecyclerStress;
#ifdef CONCURRENT_GC_ENABLED
    this->recyclerBackgroundStress = GetRecyclerFlagsTable().RecyclerBackgroundStress;
    this->recyclerConcurrentStress = GetRecyclerFlagsTable().RecyclerConcurrentStress;
    this->recyclerConcurrentRepeatStress = GetRecyclerFlagsTable().RecyclerConcurrentRepeatStress;
#endif
#ifdef PARTIAL_GC_ENABLED
    this->recyclerPartialStress = GetRecyclerFlagsTable().RecyclerPartialStress;
#endif
#endif

    bool needWriteWatch = false;

#ifdef CONCURRENT_GC_ENABLED
    // Default to non-concurrent
    uint numProcs = (uint)AutoSystemInfo::Data.GetNumberOfPhysicalProcessors();
    this->maxParallelism = (numProcs > 4) || CUSTOM_PHASE_FORCE1(GetRecyclerFlagsTable(), Js::ParallelMarkPhase) ? 4 : numProcs;

    if (forceInThread)
    {
        // Requested a non-concurrent recycler
        this->disableConcurrent = true;
    }
#if ENABLE_DEBUG_CONFIG_OPTIONS
    else if (CUSTOM_PHASE_OFF1(GetRecyclerFlagsTable(), Js::ConcurrentCollectPhase))
    {
        // Concurrent collection disabled
        this->disableConcurrent = true;
    }
    else if (CUSTOM_PHASE_OFF1(GetRecyclerFlagsTable(), Js::ConcurrentMarkPhase) &&
             CUSTOM_PHASE_OFF1(GetRecyclerFlagsTable(), Js::ParallelMarkPhase) &&
             CUSTOM_PHASE_OFF1(GetRecyclerFlagsTable(), Js::ConcurrentSweepPhase))
    {
        // All concurrent collection phases disabled
        this->disableConcurrent = true;
    }
#endif
    else
    {
        this->disableConcurrent = false;

        if (deferThreadStartup || EnableConcurrent(threadService, false))
        {
            needWriteWatch = true;
        }
    }
#endif

#ifdef PARTIAL_GC_ENABLED
    if (this->enablePartialCollect)
    {
        needWriteWatch = true;
    }
#endif

    if (needWriteWatch)
    {
        // need write watch to support concurrent and/or partial collection
        recyclerPageAllocator.EnableWriteWatch();
        recyclerLargeBlockPageAllocator.EnableWriteWatch();
    }
}

#if DBG
BOOL
Recycler::IsFreeObject(void * candidate)
{
    HeapBlock * heapBlock = this->FindHeapBlock(candidate);
    if (heapBlock != NULL)
    {
        return heapBlock->IsFreeObject(candidate);
    }
    return false;
}
#endif

BOOL
Recycler::IsValidObject(void* candidate, size_t minimumSize)
{
    HeapBlock * heapBlock = this->FindHeapBlock(candidate);
    if (heapBlock != NULL)
    {
        return heapBlock->IsValidObject(candidate) && (minimumSize == 0 || heapBlock->GetObjectSize(candidate) >= minimumSize);
    }

    return false;
}

void
Recycler::Prime()
{
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    if (GetRecyclerFlagsTable().IsEnabled(Js::ForceFragmentAddressSpaceFlag))
    {
        // Never prime the recycler if we are forced to fragment address space
        return;
    }
#endif
    ForRecyclerPageAllocator(Prime(RecyclerPageAllocator::DefaultPrimePageCount));
}

void
Recycler::AddExternalMemoryUsage(size_t size)
{
    this->autoHeap.uncollectedAllocBytes += size;
    this->autoHeap.uncollectedExternalBytes += size;
    // Generally normal GC can cleanup the uncollectedAllocBytes. But if external components
    // do fast large allocations in a row, normal GC might not kick in. Let's force the GC
    // here if we need to collect anyhow.
    CollectNow<CollectOnAllocation>();
}

BOOL Recycler::ReportExternalMemoryAllocation(size_t size)
{
    return recyclerPageAllocator.RequestAlloc(size);
}

void Recycler::ReportExternalMemoryFailure(size_t size)
{
    recyclerPageAllocator.ReportFailure(size);
}

void Recycler::ReportExternalMemoryFree(size_t size)
{
    recyclerPageAllocator.ReportFree(size);
}


/*------------------------------------------------------------------------------------------------
 * Idle Decommit
 *------------------------------------------------------------------------------------------------*/

void
Recycler::EnterIdleDecommit()
{
    ForRecyclerPageAllocator(EnterIdleDecommit());
#ifdef IDLE_DECOMMIT_ENABLED
    ::InterlockedCompareExchange(&needIdleDecommitSignal, IdleDecommitSignal_None, IdleDecommitSignal_NeedTimer);
#endif
}

void
Recycler::LeaveIdleDecommit()
{
#ifdef IDLE_DECOMMIT_ENABLED
    bool allowTimer = (this->concurrentIdleDecommitEvent != nullptr);
    IdleDecommitSignal idleDecommitSignalRecycler = recyclerPageAllocator.LeaveIdleDecommit(allowTimer);
    IdleDecommitSignal idleDecommitSignalRecyclerLargeBlock = recyclerLargeBlockPageAllocator.LeaveIdleDecommit(allowTimer);
    IdleDecommitSignal idleDecommitSignal = max(idleDecommitSignalRecycler, idleDecommitSignalRecyclerLargeBlock);
    IdleDecommitSignal idleDecommitSignalThread = threadPageAllocator->LeaveIdleDecommit(allowTimer);
    idleDecommitSignal = max(idleDecommitSignal, idleDecommitSignalThread);

#ifdef RECYCLER_WRITE_BARRIER_ALLOC_SEPARATE_PAGE
    IdleDecommitSignal idleDecommitSignalRecyclerWithBarrier = recyclerWithBarrierPageAllocator.LeaveIdleDecommit(allowTimer);
    idleDecommitSignal = max(idleDecommitSignal, idleDecommitSignalRecyclerWithBarrier);
#endif
    if (idleDecommitSignal != IdleDecommitSignal_None)
    {
        Assert(allowTimer);
        // Reduce the number of times we need to signal the background thread
        // by detecting whether the thread is waiting on a time out or not
        if (idleDecommitSignal == IdleDecommitSignal_NeedSignal ||
            ::InterlockedCompareExchange(&needIdleDecommitSignal, IdleDecommitSignal_NeedTimer, IdleDecommitSignal_None) == IdleDecommitSignal_NeedSignal)
        {
#if DBG
            if (GetRecyclerFlagsTable().Trace.IsEnabled(Js::IdleDecommitPhase))
            {
                Output::Print(L"Recycler Thread IdleDecommit Need Signal\n");
                Output::Flush();
            }
#endif
            SetEvent(this->concurrentIdleDecommitEvent);
        }
    }
#else
    ForEachRecyclerPageAllocatorIn(this, LeaveIdleDecommit(false));
#endif
}

/*------------------------------------------------------------------------------------------------
* Freeing
*------------------------------------------------------------------------------------------------*/
bool Recycler::ExplicitFreeLeaf(void* buffer, size_t size)
{
    return ExplicitFreeInternalWrapper<ObjectInfoBits::LeafBit>(buffer, size);
}

bool Recycler::ExplicitFreeNonLeaf(void* buffer, size_t size)
{
    return ExplicitFreeInternalWrapper<ObjectInfoBits::NoBit>(buffer, size);
}

size_t Recycler::GetAllocSize(size_t size)
{
    size_t allocSize = size;
#ifdef RECYCLER_MEMORY_VERIFY
    if (this->VerifyEnabled())
    {
        allocSize += verifyPad + sizeof(size_t);
        Assert(allocSize > size);
    }
#endif

    return allocSize;
}

template <typename TBlockAttributes>
void Recycler::SetExplicitFreeBitOnSmallBlock(HeapBlock* heapBlock, size_t sizeCat, void* buffer, ObjectInfoBits attributes)
{
    Assert(!heapBlock->IsLargeHeapBlock());
    Assert(heapBlock->GetObjectSize(buffer) == sizeCat);
    SmallHeapBlockT<TBlockAttributes>* smallBlock = (SmallHeapBlockT<TBlockAttributes>*)heapBlock;
    if ((attributes & ObjectInfoBits::LeafBit) == LeafBit)
    {
        Assert(smallBlock->IsLeafBlock());
    }
    else
    {
        Assert(smallBlock->IsAnyNormalBlock());
    }


#ifdef RECYCLER_MEMORY_VERIFY
    smallBlock->SetExplicitFreeBitForObject(buffer);
#endif
}

template <ObjectInfoBits attributes>
bool Recycler::ExplicitFreeInternalWrapper(void* buffer, size_t size)
{
    Assert(buffer != nullptr);
    Assert(size > 0);

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    if (CUSTOM_PHASE_OFF1(GetRecyclerFlagsTable(), Js::ExplicitFreePhase))
    {
        return false;
    }
#endif

    size_t allocSize = GetAllocSize(size);

    if (HeapInfo::IsSmallObject(allocSize))
    {
        return ExplicitFreeInternal<attributes, SmallAllocationBlockAttributes>(buffer, size, HeapInfo::GetAlignedSizeNoCheck(allocSize));
    }

    if (HeapInfo::IsMediumObject(allocSize))
    {
        return ExplicitFreeInternal<attributes, MediumAllocationBlockAttributes>(buffer, size, HeapInfo::GetMediumObjectAlignedSizeNoCheck(allocSize));
    }

    return false;
}

template <ObjectInfoBits attributes, typename TBlockAttributes>
bool Recycler::ExplicitFreeInternal(void* buffer, size_t size, size_t sizeCat)
{
    // If the GC is in sweep state while FreeInternal is called, we might be executing a finalizer
    // which called Free, which would cause a "sweepable" buffer to be free-listed. Don't allow this.
    // Also don't allow freeing while we're shutting down the recycler since finalizers get executed
    // at this stage too
    if (this->IsSweeping() || this->IsExiting())
    {
        return false;
    }

    // We shouldn't be freeing object when we are running GC in thread
    Assert(this->IsConcurrentState() || !this->CollectionInProgress() || this->collectionState == CollectionStatePostCollectionCallback);

    DebugOnly(RecyclerHeapObjectInfo info);
    Assert(this->FindHeapObject(buffer, FindHeapObjectFlags_NoFreeBitVerify, info));
    Assert((info.GetAttributes() & ~ObjectInfoBits::LeafBit) == 0);          // Only NoBit or LeafBit

#if DBG || defined(RECYCLER_MEMORY_VERIFY) || defined(RECYCLER_PAGE_HEAP)
    // Either the mainThreadHandle is null (we're not thread bound)
    // or we should be calling this function on the main script thread
    Assert(this->mainThreadHandle == NULL ||
        ::GetCurrentThreadId() == ::GetThreadId(this->mainThreadHandle));

    HeapBlock* heapBlock = this->FindHeapBlock(buffer);

    Assert(heapBlock != nullptr);
#ifdef RECYCLER_PAGE_HEAP
    if (this->IsPageHeapEnabled() && this->ShouldCapturePageHeapFreeStack())
    {
        heapBlock->CapturePageHeapFreeStack();

        // Don't do actual explicit free in page heap mode
        return false;
    }
#endif

    SetExplicitFreeBitOnSmallBlock<TBlockAttributes>(heapBlock, sizeCat, buffer, attributes);

#endif

    if (TBlockAttributes::IsMediumBlock)
    {
        autoHeap.FreeMediumObject<attributes>(buffer, sizeCat);
    }
    else
    {
        autoHeap.FreeSmallObject<attributes>(buffer, sizeCat);
    }

    if (size > sizeof(FreeObject) || TBlockAttributes::IsMediumBlock)
    {
        // Do this on the background somehow?
        byte expectedFill = 0;
        size_t fillSize = size - sizeof(FreeObject);

#ifdef RECYCLER_MEMORY_VERIFY
        if (this->VerifyEnabled())
        {
            expectedFill = Recycler::VerifyMemFill;
        }
#endif

        memset(((char*)buffer) + sizeof(FreeObject), expectedFill, fillSize);
    }

#ifdef PROFILE_RECYCLER_ALLOC
    if (this->trackerDictionary != nullptr)
    {
        this->SetTrackerData(buffer, &TrackerData::ExplicitFreeListObjectData);
    }
#endif

    return true;
}

/*------------------------------------------------------------------------------------------------
 * Allocation
 *------------------------------------------------------------------------------------------------*/

char *
Recycler::TryLargeAlloc(HeapInfo * heap, size_t size, ObjectInfoBits attributes, bool nothrow)
{
    Assert((attributes & InternalObjectInfoBitMask) == attributes);
    Assert(size != 0);

    size_t sizeCat = HeapInfo::GetAlignedSizeNoCheck(size);
    if (sizeCat == 0)
    {
        // overflow scenario
        // if onthrow is false, throw out of memory
        // otherwise, return null
        if (nothrow == false)
        {
            this->OutOfMemory();
        }
        return nullptr;
    }

    char * memBlock;
    if (heap->largeObjectBucket.largeBlockList != nullptr)
    {
        memBlock = heap->largeObjectBucket.largeBlockList->Alloc(sizeCat, attributes);
        if (memBlock != nullptr)
        {
#ifdef RECYCLER_ZERO_MEM_CHECK
            VerifyZeroFill(memBlock, sizeCat);
#endif
            return memBlock;
        }
    }

    // We don't care whether a GC happened here or not, because we are not reusing freed
    // large objects. We might try to allocate from existing block if we implement
    // large object reuse.
    if (!this->disableCollectOnAllocationHeuristics)
    {
        CollectNow<CollectOnAllocation>();
    }

#ifdef RECYCLER_PAGE_HEAP
    if (IsPageHeapEnabled())
    {
        if (heap->largeObjectBucket.IsPageHeapEnabled())
        {
            memBlock = heap->largeObjectBucket.PageHeapAlloc(this, size, (ObjectInfoBits)attributes, autoHeap.pageHeapMode, nothrow);
            if (memBlock != nullptr)
            {
#ifdef RECYCLER_ZERO_MEM_CHECK
                VerifyZeroFill(memBlock, sizeCat);
#endif
                return memBlock;
            }
        }
    }
#endif

    LargeHeapBlock * heapBlock = heap->AddLargeHeapBlock(sizeCat);
    if (heapBlock == nullptr)
    {
        return nullptr;
    }
    memBlock = heapBlock->Alloc(sizeCat, attributes);
    Assert(memBlock != nullptr);
#ifdef RECYCLER_ZERO_MEM_CHECK
    VerifyZeroFill(memBlock, sizeCat);
#endif
    return memBlock;
}

template <bool nothrow>
char*
Recycler::LargeAlloc(HeapInfo* heap, size_t size, ObjectInfoBits attributes)
{
    Assert((attributes & InternalObjectInfoBitMask) == attributes);

    char * addr = TryLargeAlloc(heap, size, attributes, nothrow);
    if (addr == nullptr)
    {
        // Force a collection and try to allocate again.
        this->CollectNow<CollectNowForceInThread>();
        addr = TryLargeAlloc(heap, size, attributes, nothrow);
        if (addr == nullptr)
        {
            if (nothrow == false)
            {
                // Still fails, we are out of memory
                // Since nothrow is false, it's okay to throw here
                this->OutOfMemory();
            }
            else
            {
                return nullptr;
            }
        }
    }
    autoHeap.uncollectedAllocBytes += size;
    return addr;
}

// Explicitly instantiate both versions of LargeAlloc
template char* Recycler::LargeAlloc<true>(HeapInfo* heap, size_t size, ObjectInfoBits attributes);
template char* Recycler::LargeAlloc<false>(HeapInfo* heap, size_t size, ObjectInfoBits attributes);

void
Recycler::OutOfMemory()
{
    outOfMemoryFunc();
}

void Recycler::GetNormalHeapBlockAllocatorInfoForNativeAllocation(size_t allocSize, void*& allocatorAddress, uint32& endAddressOffset, uint32& freeListOffset)
{
    Assert(HeapInfo::IsAlignedSize(allocSize));
    Assert(HeapInfo::IsSmallObject(allocSize));

    allocatorAddress = GetAddressOfAllocator<NoBit>(allocSize);
    endAddressOffset = GetEndAddressOffset<NoBit>(allocSize);
    freeListOffset = GetFreeObjectListOffset<NoBit>(allocSize);

    if (!AllowNativeCodeBumpAllocation())
    {
        freeListOffset = endAddressOffset;
    }
}

bool Recycler::AllowNativeCodeBumpAllocation()
{
    // In debug builds, if we need to track allocation info, we pretend there is no pointer-bump-allocation space
    // on this page, so that we always fail the check in native code and go to helper, which does the tracking.
#ifdef PROFILE_RECYCLER_ALLOC
    if (this->trackerDictionary != nullptr)
    {
        return false;
    }
#endif

#ifdef RECYCLER_MEMORY_VERIFY
    if (this->verifyEnabled)
    {
        return false;
    }
#endif

#ifdef RECYCLER_PAGE_HEAP
    // Don't allow bump allocation in the JIT when page heap is turned on
    if (this->IsPageHeapEnabled())
    {
        return false;
    }
#endif

    return true;
}


void Recycler::TrackNativeAllocatedMemoryBlock(Recycler * recycler, void * memBlock, size_t sizeCat)
{
    Assert(HeapInfo::IsAlignedSize(sizeCat));
    Assert(HeapInfo::IsSmallObject(sizeCat));

#ifdef PROFILE_RECYCLER_ALLOC
    AssertMsg(!Recycler::DoProfileAllocTracker(), "Why did we register allocation tracking callback if all allocations are forced to slow path?");
#endif

    RecyclerMemoryTracking::ReportAllocation(recycler, memBlock, sizeCat);
    RECYCLER_PERF_COUNTER_INC(LiveObject);
    RECYCLER_PERF_COUNTER_ADD(LiveObjectSize, sizeCat);
    RECYCLER_PERF_COUNTER_SUB(FreeObjectSize, sizeCat);

#ifdef RECYCLER_MEMORY_VERIFY
    AssertMsg(!recycler->VerifyEnabled(), "Why did we register allocation tracking callback if all allocations are forced to slow path?");
#endif
}

/*------------------------------------------------------------------------------------------------
 * FindRoots
 *------------------------------------------------------------------------------------------------*/


static void* GetStackBase()
{
    return ((NT_TIB *)NtCurrentTeb())->StackBase;
}

#if _M_IX86
// REVIEW: For x86, do we care about scanning esp/ebp?
// At GC time, they shouldn't be pointing to GC memory.
#define SAVE_THREAD_CONTEXT() \
    void** targetBuffer = this->savedThreadContext.GetRegisters(); \
    __asm { push eax } \
    __asm { mov eax, targetBuffer } \
    __asm { mov [eax], esp} \
    __asm { mov [eax+0x4], eax} \
    __asm { mov [eax+0x8], ebx} \
    __asm { mov [eax+0xc], ecx} \
    __asm { mov [eax+0x10], edx} \
    __asm { mov [eax+0x14], ebp} \
    __asm { mov [eax+0x18], esi} \
    __asm { mov [eax+0x1c], edi} \
    __asm { pop eax }

#elif _M_ARM
#define SAVE_THREAD_CONTEXT() arm_SAVE_REGISTERS(this->savedThreadContext.GetRegisters());
#elif _M_ARM64
#define SAVE_THREAD_CONTEXT() arm64_SAVE_REGISTERS(this->savedThreadContext.GetRegisters());
#elif _M_AMD64
#define SAVE_THREAD_CONTEXT() amd64_SAVE_REGISTERS(this->savedThreadContext.GetRegisters());
#else
#error Unexpected architecture
#endif

size_t
Recycler::ScanArena(ArenaData * alloc, bool background)
{
#if DBG_DUMP
    if (GetRecyclerFlagsTable().Trace.IsEnabled(Js::MarkPhase)
        || GetRecyclerFlagsTable().Trace.IsEnabled(Js::FindRootPhase))
    {
        this->forceTraceMark = true;
        Output::Print(L"Scanning Guest Arena %p: ", alloc);
    }
#endif

    size_t scanRootBytes = 0;
    BEGIN_DUMP_OBJECT_ADDRESS(L"Guest Arena", alloc);

#if defined(PARTIAL_GC_ENABLED) || defined(CONCURRENT_GC_ENABLED)
// The new write watch batching logic broke the write watch handling here.
// For now, just disable write watch for guest arenas.
// Re-enable this in the future.
#if FALSE
    // Note, guest arenas are allocated out of the large block page allocator.
    bool writeWatch = alloc->GetPageAllocator() == &this->recyclerLargeBlockPageAllocator;

    // Only use write watch when we are doing rescan (Partial collect or finish concurrent)
    if (writeWatch && this->collectionState == CollectionStateRescanFindRoots)
    {
        scanRootBytes += TryMarkBigBlockListWithWriteWatch(alloc->GetBigBlocks(background));
        scanRootBytes += TryMarkBigBlockListWithWriteWatch(alloc->GetFullBlocks());
    }
    else
#endif
#endif
    {
        scanRootBytes += TryMarkBigBlockList(alloc->GetBigBlocks(background));
        scanRootBytes += TryMarkBigBlockList(alloc->GetFullBlocks());
    }
    scanRootBytes += TryMarkArenaMemoryBlockList(alloc->GetMemoryBlocks());
    END_DUMP_OBJECT(this);
#if DBG_DUMP
    if (GetRecyclerFlagsTable().Trace.IsEnabled(Js::MarkPhase)
        || GetRecyclerFlagsTable().Trace.IsEnabled(Js::FindRootPhase))
    {
        this->forceTraceMark = false;
        Output::Print(L"\n");
        Output::Flush();
    }
#endif

    // The arena has been scanned so the full blocks can be rearranged at this point
#if ENABLE_DEBUG_CONFIG_OPTIONS
    if (background || !GetRecyclerFlagsTable().RecyclerProtectPagesOnRescan)
#endif
    {
        alloc->SetLockBlockList(false);
    }

    return scanRootBytes;
}

#if DBG
bool
Recycler::ExpectStackSkip() const
{
    // Okay to skip the stack scan if we're in leak check mode
    bool expectStackSkip = false;

#ifdef LEAK_REPORT
    expectStackSkip = expectStackSkip || GetRecyclerFlagsTable().IsEnabled(Js::LeakReportFlag);
#endif
#ifdef CHECK_MEMORY_LEAK
    expectStackSkip = expectStackSkip || GetRecyclerFlagsTable().CheckMemoryLeak;
#endif
#ifdef RECYCLER_DUMP_OBJECT_GRAPH
    expectStackSkip = expectStackSkip || (this->objectGraphDumper != nullptr);
#endif

#if defined(INTERNAL_MEM_PROTECT_HEAP_ALLOC)
    expectStackSkip = expectStackSkip || GetRecyclerFlagsTable().MemProtectHeap;
#endif
    return expectStackSkip || isExternalStackSkippingGC;
}
#endif

#pragma warning(push)
#pragma warning(disable:4731) // 'pointer' : frame pointer register 'register' modified by inline assembly code
size_t
Recycler::ScanStack()
{
#ifdef CONCURRENT_GC_ENABLED
    if (this->skipStack)
    {
#ifdef RECYCLER_TRACE
        CUSTOM_PHASE_PRINT_VERBOSE_TRACE1(GetRecyclerFlagsTable(), Js::ScanStackPhase, L"[%04X] Skipping the stack scan\n", ::GetCurrentThreadId());
#endif

        Assert(this->isFinishGCOnIdle || this->isConcurrentGCOnIdle || this->ExpectStackSkip());
        return 0;
    }
#endif

#ifdef RECYCLER_STATS
    size_t lastMarkCount = this->collectionStats.markData.markCount;
#endif

    GCETW(GC_SCANSTACK_START, (this));

    RECYCLER_PROFILE_EXEC_BEGIN(this, Js::ScanStackPhase);

    SAVE_THREAD_CONTEXT();
    void * stackTop = this->savedThreadContext.GetStackTop();

    void * stackStart = GetStackBase();
    Assert(stackStart > stackTop);
    size_t stackScanned = (size_t)((char *)stackStart - (char *)stackTop);

#if DBG_DUMP
    if (GetRecyclerFlagsTable().Trace.IsEnabled(Js::MarkPhase)
        || GetRecyclerFlagsTable().Trace.IsEnabled(Js::ScanStackPhase))
    {
        this->forceTraceMark = true;
        Output::Print(L"Scanning Stack %p(%8d): ", stackTop, (char *)stackStart - (char *)stackTop);
    }
#endif

    BEGIN_DUMP_OBJECT(this, L"Registers");
    ScanMemoryInline(this->savedThreadContext.GetRegisters(), sizeof(void*) * SavedRegisterState::NumRegistersToSave);
    END_DUMP_OBJECT(this);

    BEGIN_DUMP_OBJECT(this, L"Stack");
    ScanMemoryInline((void**) stackTop, stackScanned);
    END_DUMP_OBJECT(this);

#if DBG_DUMP
    if (GetRecyclerFlagsTable().Trace.IsEnabled(Js::MarkPhase)
        || GetRecyclerFlagsTable().Trace.IsEnabled(Js::ScanStackPhase))
    {
        this->forceTraceMark = false;
        Output::Print(L"\n");
        Output::Flush();
    }
#endif

    RECYCLER_PROFILE_EXEC_END(this, Js::ScanStackPhase);
    RECYCLER_STATS_ADD(this, stackCount, this->collectionStats.markData.markCount - lastMarkCount);
    GCETW(GC_SCANSTACK_STOP, (this));

    return stackScanned;
}
#pragma warning(pop)

template <bool background>
size_t Recycler::ScanPinnedObjects()
{
    size_t scanRootBytes = 0;
    BEGIN_DUMP_OBJECT(this, L"Pinned");
    {
        this->TryMarkNonInterior(transientPinnedObject, &transientPinnedObject /* parentReference */);
        if (this->scanPinnedObjectMap)
        {
            // We are scanning the pinned object map now, we don't need to rescan unless
            // we reset mark or we add stuff to the map in Recycler::AddRef
            this->scanPinnedObjectMap = false;
            pinnedObjectMap.MapAndRemoveIf([this, &scanRootBytes](void * obj, PinRecord const& refCount)
            {
                if (refCount == 0)
                {
#if defined(CHECK_MEMORY_LEAK) || defined(LEAK_REPORT)
                    Assert(refCount.stackBackTraces == nullptr);
#endif
                    // Only remove if we are not doing this in the background.
                    return !background;
                }
                this->TryMarkNonInterior(obj, static_cast<void*>(const_cast<PinRecord*>(&refCount)) /* parentReference */);
                scanRootBytes += sizeof(void *);
                return false;
            });

            if (!background)
            {
                this->hasPendingUnpinnedObject = false;
            }
        }
    }

    END_DUMP_OBJECT(this);

    if (background)
    {
        // Re-enable resize now that we are done
        pinnedObjectMap.EnableResize();
    }
    return scanRootBytes;
}

void
RecyclerScanMemoryCallback::operator()(void** obj, size_t byteCount)
{
    this->recycler->ScanMemoryInline(obj, byteCount);
}

size_t
Recycler::FindRoots()
{
    size_t scanRootBytes = 0;
#ifdef RECYCLER_STATS
    size_t lastMarkCount = this->collectionStats.markData.markCount;
#endif

    GCETW(GC_SCANROOTS_START, (this));

    RECYCLER_PROFILE_EXEC_BEGIN(this, Js::FindRootPhase);

#ifdef ENABLE_PROJECTION
    {
        AUTO_TIMESTAMP(externalWeakReferenceObjectResolve);
        BEGIN_DUMP_OBJECT(this, L"External Weak Referenced Roots");
        Assert(!this->IsInRefCountTrackingForProjection());
#if DBG
        AutoIsInRefCountTrackingForProjection autoIsInRefCountTrackingForProjection(this);
#endif
        collectionWrapper->MarkExternalWeakReferencedObjects(this->inPartialCollectMode);
        END_DUMP_OBJECT(this);
    }
#endif

    // go through ITracker* stuff. Don't need to do it if we are doing a partial collection
    // as we keep track and mark all trackable objects.
    // Do this first because the host might unpin stuff in the process
    if (externalRootMarker != NULL)
    {
#ifdef PARTIAL_GC_ENABLED
        if (!this->inPartialCollectMode)
#endif
        {
            RECYCLER_PROFILE_EXEC_BEGIN(this, Js::FindRootExtPhase);
#if DBG_DUMP
            if (GetRecyclerFlagsTable().Trace.IsEnabled(Js::MarkPhase)
                || GetRecyclerFlagsTable().Trace.IsEnabled(Js::FindRootPhase))
            {
                this->forceTraceMark = true;
                Output::Print(L"Scanning External Roots: ");
            }
#endif
            BEGIN_DUMP_OBJECT(this, L"External Roots");

            // PARTIALGC-TODO: How do we count external roots?
            externalRootMarker(externalRootMarkerContext);
            END_DUMP_OBJECT(this);
#if DBG_DUMP
            if (GetRecyclerFlagsTable().Trace.IsEnabled(Js::MarkPhase)
                || GetRecyclerFlagsTable().Trace.IsEnabled(Js::FindRootPhase))
            {
                this->forceTraceMark = false;
                Output::Print(L"\n");
                Output::Flush();
            }
#endif
            RECYCLER_PROFILE_EXEC_END(this, Js::FindRootExtPhase);
        }
    }

#if DBG_DUMP
    if (GetRecyclerFlagsTable().Trace.IsEnabled(Js::MarkPhase)
        || GetRecyclerFlagsTable().Trace.IsEnabled(Js::FindRootPhase))
    {
        this->forceTraceMark = true;
        Output::Print(L"Scanning Pinned Objects: ");
    }
#endif

    scanRootBytes += this->ScanPinnedObjects</*background = */false>();

#if DBG_DUMP
    if (GetRecyclerFlagsTable().Trace.IsEnabled(Js::MarkPhase)
        || GetRecyclerFlagsTable().Trace.IsEnabled(Js::FindRootPhase))
    {
        this->forceTraceMark = false;
        Output::Print(L"\n");
        Output::Flush();
    }
#endif

    Assert(!this->hasPendingConcurrentFindRoot);
    RECYCLER_PROFILE_EXEC_BEGIN(this, Js::FindRootArenaPhase);
    DListBase<GuestArenaAllocator>::EditingIterator guestArenaIter(&guestArenaList);
    while (guestArenaIter.Next())
    {
        GuestArenaAllocator& allocator = guestArenaIter.Data();
        if (allocator.pendingDelete)
        {
            Assert(this->hasPendingDeleteGuestArena);
            allocator.SetLockBlockList(false);

            guestArenaIter.RemoveCurrent(&HeapAllocator::Instance);
        }
        else if (this->backgroundFinishMarkCount == 0)
        {
            // Only scan arena if we haven't finished mark in the background
            scanRootBytes += ScanArena(&allocator, false);
        }
    }
    this->hasPendingDeleteGuestArena = false;

    DList<ArenaData *, HeapAllocator>::Iterator externalGuestArenaIter(&externalGuestArenaList);
    while (externalGuestArenaIter.Next())
    {
        scanRootBytes += ScanArena(externalGuestArenaIter.Data(), false);
    }
    RECYCLER_PROFILE_EXEC_END(this, Js::FindRootArenaPhase);

    this->ScanImplicitRoots();

    RECYCLER_PROFILE_EXEC_END(this, Js::FindRootPhase);
    GCETW(GC_SCANROOTS_STOP, (this));
    RECYCLER_STATS_ADD(this, rootCount, this->collectionStats.markData.markCount - lastMarkCount);
    return scanRootBytes;
}

void
Recycler::ScanImplicitRoots()
{
    if (this->enableScanImplicitRoots)
    {
        RECYCLER_PROFILE_EXEC_BEGIN(this, Js::FindImplicitRootPhase);
        if (!this->hasScannedInitialImplicitRoots)
        {
            this->ScanInitialImplicitRoots();
            this->hasScannedInitialImplicitRoots = true;
        }
        else
        {
            this->ScanNewImplicitRoots();
        }
        RECYCLER_PROFILE_EXEC_END(this, Js::FindImplicitRootPhase);
    }
}

size_t
Recycler::TryMarkArenaMemoryBlockList(ArenaMemoryBlock * memoryBlocks)
{
    size_t scanRootBytes = 0;
    ArenaMemoryBlock *blockp = memoryBlocks;
    while (blockp != NULL)
    {
        void** base=(void**)blockp->GetBytes();
        size_t byteCount = blockp->nbytes;
        scanRootBytes += byteCount;
        this->ScanMemory(base, byteCount);
        blockp = blockp->next;
    }
    return scanRootBytes;
}

size_t
Recycler::TryMarkBigBlockListWithWriteWatch(BigBlock * memoryBlocks)
{
    DWORD pageSize = AutoSystemInfo::PageSize;
    size_t scanRootBytes = 0;
    BigBlock *blockp = memoryBlocks;

    // Reset the write watch bit if we are scanning this in the background thread
    DWORD const writeWatchFlags = this->IsConcurrentFindRootState()? WRITE_WATCH_FLAG_RESET : 0;
    while (blockp != NULL)
    {
        char * currentAddress = (char *)blockp->GetBytes();
        char * endAddress = currentAddress + blockp->currentByte;
        char * currentPageStart = (char *)blockp->allocation;
        while (currentAddress < endAddress)
        {
            void * written;
            ULONG_PTR count = 1;
            if (::GetWriteWatch(writeWatchFlags, currentPageStart, AutoSystemInfo::PageSize, &written, &count, &pageSize) != 0 || count == 1)
            {
                char * currentEnd = min(currentPageStart + pageSize, endAddress);
                size_t byteCount = (size_t)(currentEnd - currentAddress);
                scanRootBytes += byteCount;
                this->ScanMemory((void **)currentAddress, byteCount);
            }

            currentPageStart += pageSize;
            currentAddress = currentPageStart;
        }
        blockp = blockp->nextBigBlock;
    }
    return scanRootBytes;
}

size_t
Recycler::TryMarkBigBlockList(BigBlock * memoryBlocks)
{
    size_t scanRootBytes = 0;
    BigBlock *blockp = memoryBlocks;
    while (blockp != NULL)
    {
        void** base = (void**)blockp->GetBytes();
        size_t byteCount = blockp->currentByte;
        scanRootBytes +=  byteCount;
        this->ScanMemory(base, byteCount);
        blockp = blockp->nextBigBlock;
    }
    return scanRootBytes;
}

void
Recycler::ScanInitialImplicitRoots()
{
    autoHeap.ScanInitialImplicitRoots();
}

void
Recycler::ScanNewImplicitRoots()
{
    autoHeap.ScanNewImplicitRoots();
}

/*------------------------------------------------------------------------------------------------
 * Mark
 *------------------------------------------------------------------------------------------------*/
void
Recycler::ResetMarks(ResetMarkFlags flags)
{
    Assert(!this->CollectionInProgress());
    collectionState = CollectionStateResetMarks;

    RecyclerVerboseTrace(GetRecyclerFlagsTable(), L"Reset marks\n");
    GCETW(GC_RESETMARKS_START, (this));
    RECYCLER_PROFILE_EXEC_BEGIN(this, Js::ResetMarksPhase);

    Assert(IsMarkStackEmpty());
    this->scanPinnedObjectMap = true;
    this->hasScannedInitialImplicitRoots = false;

    heapBlockMap.ResetMarks();

    autoHeap.ResetMarks(flags);

    RECYCLER_PROFILE_EXEC_END(this, Js::ResetMarksPhase);
    GCETW(GC_RESETMARKS_STOP, (this));

#ifdef RECYCLER_MARK_TRACK
    this->ClearMarkMap();
#endif
}

#ifdef RECYCLER_MARK_TRACK
void Recycler::ClearMarkMap()
{
    this->markMap->Clear();
}

void Recycler::PrintMarkMap()
{
    this->markMap->Map([](void* key, void* value)
    {
        Output::Print(L"0x%P => 0x%P\n", key, value);
    });
}
#endif

#if DBG
void
Recycler::CheckAllocExternalMark() const
{
    Assert(!disableThreadAccessCheck);
    Assert(GetCurrentThreadContextId() == mainThreadId);
#ifdef CONCURRENT_GC_ENABLED
  #ifdef HEAP_ENUMERATION_VALIDATION
    Assert((this->IsMarkState() || this->IsPostEnumHeapValidationInProgress()) && collectionState != CollectionStateConcurrentMark);
  #else
    Assert(this->IsMarkState()  && collectionState != CollectionStateConcurrentMark);
  #endif
#else
    Assert(this->IsMarkState());
#endif
}
#endif

void
Recycler::TryMarkNonInterior(void* candidate, void* parentReference)
{
#ifdef HEAP_ENUMERATION_VALIDATION
    Assert(!isHeapEnumInProgress || this->IsPostEnumHeapValidationInProgress());
#else
    Assert(!isHeapEnumInProgress);
#endif
    Assert(this->collectionState != CollectionStateParallelMark);
    markContext.Mark</*parallel */ false, /* interior */ false>(candidate, parentReference);
}


void
Recycler::TryMarkInterior(void* candidate, void* parentReference)
{
#ifdef HEAP_ENUMERATION_VALIDATION
    Assert(!isHeapEnumInProgress || this->IsPostEnumHeapValidationInProgress());
#else
    Assert(!isHeapEnumInProgress);
#endif
    Assert(this->collectionState != CollectionStateParallelMark);
    markContext.Mark</*parallel */ false, /* interior */ true>(candidate, parentReference);
}

template <bool parallel, bool interior>
void
Recycler::ProcessMarkContext(MarkContext * markContext)
{
    // Copying the markContext onto the stack messes up tracked object handling, because
    // the tracked object will call TryMark[Non]Interior to report its references.
    // These functions implicitly use the main markContext on the Recycler, but this will
    // be overridden if we're processing the main markContext here.
    // So, don't do this if we are going to process tracked objects.
    // (This will be the case if we're not queuing and we're not in partial mode, which ignores tracked objects.)
    // In this case we shouldn't be parallel anyway, so we don't need to worry about cache behavior.
    // We should revisit how we manage markContexts in general in the future, and clean this up
    // by passing the MarkContext through to the tracked object's Mark method.
    if (this->inPartialCollectMode || DoQueueTrackedObject())
    {
        // The markContext as passed is one of the markContexts that lives on the Recycler.
        // Copy it locally for processing.
        // This serves two purposes:
        // (1) Allow for better codegen because the markContext is local and we don't need to track the this pointer separately
        //      (because all the key processing is inlined into this function).
        // (2) Ensure we don't have weird cache behavior because we're accidentally writing to the same cache line from
        //      multiple threads during parallel marking.

        MarkContext localMarkContext = *markContext;

        // Do the actual marking.
        localMarkContext.ProcessMark<parallel, interior>();

        // Copy back to the original location.
        *markContext = localMarkContext;

        // Clear the local mark context.
        localMarkContext.Clear();
    }
    else
    {
        Assert(!parallel);
        markContext->ProcessMark<parallel, interior>();
    }
}

void
Recycler::ProcessMark(bool background)
{
    if (background)
    {
        GCETW(GC_BACKGROUNDMARK_START, (this, backgroundRescanCount));
    }
    else
    {
        GCETW(GC_MARK_START, (this));
    }

    RECYCLER_PROFILE_EXEC_THREAD_BEGIN(background, this, Js::MarkPhase);

    if (this->enableScanInteriorPointers)
    {
        this->ProcessMarkContext</* parallel */ false, /* interior */ true>(&markContext);
    }
    else
    {
        this->ProcessMarkContext</* parallel */ false, /* interior */ false>(&markContext);
    }

    RECYCLER_PROFILE_EXEC_THREAD_END(background, this, Js::MarkPhase);

    if (background)
    {
        GCETW(GC_BACKGROUNDMARK_STOP, (this, backgroundRescanCount));
    }
    else
    {
        GCETW(GC_MARK_STOP, (this));
    }

    DebugOnly(this->markContext.VerifyPostMarkState());
}


void
Recycler::ProcessParallelMark(bool background, MarkContext * markContext)
{
    if (background)
    {
        GCETW(GC_BACKGROUNDPARALLELMARK_START, (this, backgroundRescanCount));
    }
    else
    {
        GCETW(GC_PARALLELMARK_START, (this));
    }

    RECYCLER_PROFILE_EXEC_THREAD_BEGIN(background, this, Js::MarkPhase);

    if (this->enableScanInteriorPointers)
    {
        this->ProcessMarkContext</* parallel */ true, /* interior */ true>(markContext);
    }
    else
    {
        this->ProcessMarkContext</* parallel */ true, /* interior */ false>(markContext);
    }

    RECYCLER_PROFILE_EXEC_THREAD_END(background, this, Js::MarkPhase);

    if (background)
    {
        GCETW(GC_BACKGROUNDPARALLELMARK_STOP, (this, backgroundRescanCount));
    }
    else
    {
        GCETW(GC_PARALLELMARK_STOP, (this));
    }
}

void
Recycler::Mark()
{
    // Marking in thread, we can just pre-mark them
    ResetMarks(this->enableScanImplicitRoots ? ResetMarkFlags_InThreadImplicitRoots : ResetMarkFlags_InThread);
    collectionState = CollectionStateFindRoots;
    RootMark(CollectionStateMark);
}

#ifdef CONCURRENT_GC_ENABLED
void
Recycler::StartQueueTrackedObject()
{
    Assert(!this->queueTrackedObject);
    Assert(!this->HasPendingTrackObjects());
    Assert(this->clientTrackedObjectList.Empty());
    Assert(!this->inPartialCollectMode);
    this->queueTrackedObject = true;
}

bool
Recycler::DoQueueTrackedObject() const
{
    Assert(this->queueTrackedObject || !this->IsConcurrentMarkState());
    Assert(this->queueTrackedObject || this->inPartialCollectMode || !(this->collectionState == CollectionStateParallelMark));
    Assert(this->queueTrackedObject || this->isProcessingTrackedObjects || !this->HasPendingTrackObjects());
#ifdef PARTIAL_GC_ENABLED
    Assert(!this->queueTrackedObject || (this->clientTrackedObjectList.Empty() && !this->inPartialCollectMode));
#endif
    return this->queueTrackedObject;
}


#endif

void
Recycler::ResetCollectionState()
{
    Assert(IsMarkStackEmpty());

    this->collectionState = CollectionStateNotCollecting;
    this->backgroundFinishMarkCount = 0;
    this->inExhaustiveCollection = false;
    this->inDecommitNowCollection = false;

    CleanupPendingUnroot();

#ifdef PARTIAL_GC_ENABLED
    if (inPartialCollectMode)
    {
        FinishPartialCollect();
    }
#endif
#ifdef CONCURRENT_GC_ENABLED
    Assert(!this->DoQueueTrackedObject());
#endif
#ifdef RECYCLER_FINALIZE_CHECK
    // Reset the collection stats.
    this->collectionStats.finalizeCount = this->autoHeap.liveFinalizableObjectCount - this->autoHeap.newFinalizableObjectCount - this->autoHeap.pendingDisposableObjectCount;
#endif
}

void
Recycler::ResetMarkCollectionState()
{
    // If we aborted after doing a background Rescan, there will be entries in the markContext.
    // Abort these entries and reset the markContext state.
    markContext.Abort();

    // If we aborted after doing a background parallel Mark, we wouldn't have cleaned up the
    // parallel markContexts yet. Clean these up now.
    // Note parallelMarkContext1 is not used in background parallel (see DoBackgroundParallelMark)
    parallelMarkContext2.Cleanup();
    parallelMarkContext3.Cleanup();

    this->ClearNeedOOMRescan();
    DebugOnly(this->isProcessingRescan = false);

#ifdef CONCURRENT_GC_ENABLED
    // If we're reseting the mark collection state, we need to unlock the block list
    DListBase<GuestArenaAllocator>::EditingIterator guestArenaIter(&guestArenaList);
    while (guestArenaIter.Next())
    {
        GuestArenaAllocator& allocator = guestArenaIter.Data();
        allocator.SetLockBlockList(false);
    }

    this->queueTrackedObject = false;
#endif
    ResetCollectionState();
}

void
Recycler::ResetHeuristicCounters()
{
    autoHeap.lastUncollectedAllocBytes = autoHeap.uncollectedAllocBytes;
    autoHeap.uncollectedAllocBytes = 0;
    autoHeap.uncollectedExternalBytes = 0;
    ResetPartialHeuristicCounters();
}

void Recycler::ResetPartialHeuristicCounters()
{
#ifdef PARTIAL_GC_ENABLED
    autoHeap.uncollectedNewPageCount = 0;
#endif
}

void
Recycler::ScheduleNextCollection()
{
    this->tickCountNextCollection = ::GetTickCount() + RecyclerHeuristic::TickCountCollection;
    this->tickCountNextFinishCollection = ::GetTickCount() + RecyclerHeuristic::TickCountFinishCollection;
}

#ifdef CONCURRENT_GC_ENABLED
void
Recycler::PrepareSweep()
{
    autoHeap.PrepareSweep();
}
#endif

size_t
Recycler::RescanMark(DWORD waitTime)
{
    bool const onLowMemory = this->NeedOOMRescan();
    Assert(this->inPartialCollectMode || DoQueueTrackedObject());

    {
        // We are about to do a rescan mark, which for consistency requires the runtime to stop any additional mutator threads
        AUTO_NO_EXCEPTION_REGION;
        collectionWrapper->PreRescanMarkCallback();
    }

    // Always called in-thread
    Assert(collectionState == CollectionStateRescanFindRoots);
#ifdef CONCURRENT_GC_ENABLED
    if (!onLowMemory && // Don't do background finish mark if we are low on memory
        // Only do background finish mark if we have a time limit or it is forced
        (CUSTOM_PHASE_FORCE1(GetRecyclerFlagsTable(), Js::BackgroundFinishMarkPhase) || waitTime != INFINITE) &&
        // Don't do background finish mark if we failed to finish mark too many times
        (this->backgroundFinishMarkCount < RecyclerHeuristic::MaxBackgroundFinishMarkCount(this->GetRecyclerFlagsTable())))
    {
        this->PrepareBackgroundFindRoots();
        if (StartConcurrent(CollectionStateConcurrentFinishMark))
        {
            this->backgroundFinishMarkCount++;
            this->PrepareSweep();
            GCETW(GC_RESCANMARKWAIT_START, (this, waitTime));
            const BOOL waited = WaitForConcurrentThread(waitTime);
            GCETW(GC_RESCANMARKWAIT_STOP, (this, !waited));
            if (!waited)
            {
                CUSTOM_PHASE_PRINT_TRACE1(GetRecyclerFlagsTable(), Js::BackgroundFinishMarkPhase, L"Finish mark timed out\n");

                {
                    // We timed out doing the finish mark, notify the runtime
                    AUTO_NO_EXCEPTION_REGION;
                    collectionWrapper->RescanMarkTimeoutCallback();
                }

                return Recycler::InvalidScanRootBytes;
            }
            Assert(collectionState == CollectionStateRescanWait);
            collectionState = CollectionStateRescanFindRoots;
            Assert(recyclerPageAllocator.GetWriteWatchPageCount() == 0);
            Assert(recyclerLargeBlockPageAllocator.GetWriteWatchPageCount() == 0);
            return this->backgroundRescanRootBytes;
        }
        this->RevertPrepareBackgroundFindRoots();
    }
#endif
    this->backgroundFinishMarkCount = 0;
    return FinishMarkRescan(false) * AutoSystemInfo::PageSize;
}

#if defined(PARTIAL_GC_ENABLED) || defined(CONCURRENT_GC_ENABLED)
size_t
Recycler::FinishMark(DWORD waitTime)
{
    size_t scannedRootBytes = RescanMark(waitTime);
    Assert(waitTime != INFINITE || scannedRootBytes != Recycler::InvalidScanRootBytes);
    if (scannedRootBytes != Recycler::InvalidScanRootBytes)
    {
#if DBG
        RecyclerVerboseTrace(GetRecyclerFlagsTable(), L"CTO: %d\n", this->clientTrackedObjectList.Count());
#endif

#ifdef PARTIAL_GC_ENABLED
        if (this->inPartialCollectMode)
        {
            RecyclerVerboseTrace(GetRecyclerFlagsTable(), L"Processing client tracked objects\n");
            ProcessClientTrackedObjects();
        }
        else
#endif
        if (DoQueueTrackedObject())
        {
            RecyclerVerboseTrace(GetRecyclerFlagsTable(), L"Processing regular tracked objects\n");

            ProcessTrackedObjects();
            Assert(this->backgroundFinishMarkCount == 0 ||
                (this->recyclerPageAllocator.GetWriteWatchPageCount() == 0 &&
                this->recyclerLargeBlockPageAllocator.GetWriteWatchPageCount() == 0));
        }

        // Continue to mark from root one more time
        scannedRootBytes += RootMark(CollectionStateRescanMark);
    }
    return scannedRootBytes;
}
#endif

void
Recycler::DoParallelMark()
{
    Assert(this->enableParallelMark);
    Assert(this->maxParallelism > 1 && this->maxParallelism <= 4);

    // Split the mark stack into [this->maxParallelism] equal pieces.
    // The actual # of splits is returned, in case the stack was too small to split that many ways.
    MarkContext * splitContexts[3] = { &parallelMarkContext1, &parallelMarkContext2, &parallelMarkContext3 };
    uint actualSplitCount = markContext.Split(this->maxParallelism - 1, splitContexts);

    Assert(actualSplitCount <= 3);

    // If we failed to split at all, just mark in thread with no parallelism.
    if (actualSplitCount == 0)
    {
        this->ProcessMark(false);
        return;
    }

    // We need to queue tracked objects while we mark in parallel.
    // (Unless it's a partial collect, in which case we don't process tracked objects at all)
    if (!this->inPartialCollectMode)
    {
        StartQueueTrackedObject();
    }

    // Kick off marking on the background thread
    bool concurrentSuccess = StartConcurrent(CollectionStateParallelMark);

    // If there's enough work to split, then kick off marking on parallel threads too.
    // If the threads haven't been created yet, this will create them (or fail).
    bool parallelSuccess1 = false;
    bool parallelSuccess2 = false;
    if (concurrentSuccess && actualSplitCount >= 2)
    {
        parallelSuccess1 = parallelThread1.StartConcurrent();
        if (parallelSuccess1 && actualSplitCount == 3)
        {
            parallelSuccess2 = parallelThread2.StartConcurrent();
        }
    }

    // Process our portion of the split.
    this->ProcessParallelMark(false, &parallelMarkContext1);

    // If we successfully launched parallel work, wait for it to complete.
    // If we failed, then process the work in-thread now.
    if (concurrentSuccess)
    {
        WaitForConcurrentThread(INFINITE);
    }
    else
    {
        this->ProcessParallelMark(false, &markContext);
    }

    if (actualSplitCount >= 2)
    {
        if (parallelSuccess1)
        {
            parallelThread1.WaitForConcurrent();
        }
        else
        {
            this->ProcessParallelMark(false, &parallelMarkContext2);
        }

        if (actualSplitCount == 3)
        {
            if (parallelSuccess2)
            {
                parallelThread2.WaitForConcurrent();
            }
            else
            {
                this->ProcessParallelMark(false, &parallelMarkContext3);
            }
        }
    }

    this->collectionState = CollectionStateMark;

    // Process tracked objects, if any, then do one final mark phase in case they marked any new objects.
    // (Unless it's a partial collect, in which case we don't process tracked objects at all)
    if (!this->inPartialCollectMode)
    {
        this->ProcessTrackedObjects();
        this->ProcessMark(false);
    }
    else
    {
        Assert(!this->HasPendingTrackObjects());
    }
}


void
Recycler::DoBackgroundParallelMark()
{
    // Split the mark stack into [this->maxParallelism - 1] equal pieces (thus, "- 2" below).
    // The actual # of splits is returned, in case the stack was too small to split that many ways.
    // The parallel threads are hardwired to use parallelMarkContext2/3, so we split using those.
    uint actualSplitCount = 0;
    MarkContext * splitContexts[2] = { &parallelMarkContext2, &parallelMarkContext3 };
    if (this->enableParallelMark)
    {
        Assert(this->maxParallelism > 1 && this->maxParallelism <= 4);
        if (this->maxParallelism > 2)
        {
            actualSplitCount = markContext.Split(this->maxParallelism - 2, splitContexts);
        }
    }

    Assert(actualSplitCount <= 2);

    // If we failed to split at all, just mark in thread with no parallelism.
    if (actualSplitCount == 0)
    {
        this->ProcessMark(true);
        return;
    }

    // We should already be set up to queue tracked objects, unless this is a partial collect
    Assert(this->DoQueueTrackedObject() || this->inPartialCollectMode);

    this->collectionState = CollectionStateBackgroundParallelMark;

    // Kick off marking on parallel threads too, if there is work for them
    // If the threads haven't been created yet, this will create them (or fail).
    bool parallelSuccess1 = false;
    bool parallelSuccess2 = false;
    parallelSuccess1 = parallelThread1.StartConcurrent();
    if (parallelSuccess1 && actualSplitCount == 2)
    {
        parallelSuccess2 = parallelThread2.StartConcurrent();
    }

    // Process our portion of the split.
    this->ProcessParallelMark(true, &markContext);

    // If we successfully launched parallel work, wait for it to complete.
    // If we failed, then process the work in-thread now.
    if (parallelSuccess1)
    {
        parallelThread1.WaitForConcurrent();
    }
    else
    {
        this->ProcessParallelMark(true, &parallelMarkContext2);
    }

    if (actualSplitCount == 2)
    {
        if (parallelSuccess2)
        {
            parallelThread2.WaitForConcurrent();
        }
        else
        {
            this->ProcessParallelMark(true, &parallelMarkContext3);
        }
    }

    this->collectionState = CollectionStateConcurrentMark;
}

size_t
Recycler::RootMark(CollectionState markState)
{
    size_t scannedRootBytes = 0;
    Assert(!this->NeedOOMRescan() || markState == CollectionStateRescanMark);
    RecyclerVerboseTrace(GetRecyclerFlagsTable(), L"PreMark done, partial collect: %d\n", this->inPartialCollectMode);

    Assert(collectionState == (markState == CollectionStateMark? CollectionStateFindRoots : CollectionStateRescanFindRoots));

    BOOL stacksScannedByRuntime = FALSE;
    {
        // We are about to scan roots in thread, notify the runtime first so it can stop threads if necessary and also provide additional roots
        AUTO_NO_EXCEPTION_REGION;
        RecyclerScanMemoryCallback scanMemory(this);
        scannedRootBytes += collectionWrapper->RootMarkCallback(scanMemory, &stacksScannedByRuntime);
    }

    scannedRootBytes += FindRoots();

    if (!stacksScannedByRuntime)
    {
        // The runtime did not scan the stack(s) for us, so we use the normal Recycler code.
        scannedRootBytes += ScanStack();
    }

    this->collectionState = markState;

    if (this->enableParallelMark)
    {
        this->DoParallelMark();
    }
    else
    {
        this->ProcessMark(false);
    }

    if (this->EndMark())
    {
        // return large root scanned byte to not get into partial mode if we are low on memory
        scannedRootBytes = RecyclerSweep::MaxPartialCollectRescanRootBytes + 1;
    }

    return scannedRootBytes;
}

bool
Recycler::EndMarkCheckOOMRescan()
{
    bool oomRescan = false;
    if (this->NeedOOMRescan())
    {
#ifdef RECYCLER_DUMP_OBJECT_GRAPH
        if (this->objectGraphDumper)
        {
            // Do not complete the mark if we are just dumping the object graph
            // Just report out of memory
            this->objectGraphDumper->isOutOfMemory = true;
            this->ClearNeedOOMRescan();
        }
        else
#endif
        {
            EndMarkOnLowMemory();
            oomRescan = true;
        }
    }

    // Done with the mark stack, it should be empty.
    // Release pages it is holding.
    Assert(!HasPendingMarkObjects());
    Assert(!HasPendingTrackObjects());
    return oomRescan;
}

bool
Recycler::EndMark()
{
    Assert(!this->DoQueueTrackedObject());
    Assert(this->clientTrackedObjectList.Empty());

    {
        // We have finished marking
        AUTO_NO_EXCEPTION_REGION;
        collectionWrapper->EndMarkCallback();
    }

    bool oomRescan = EndMarkCheckOOMRescan();

    if (ProcessObjectBeforeCollectCallbacks())
    {
        // callbacks may trigger additional marking, need to check OOMRescan again
        oomRescan |= EndMarkCheckOOMRescan();
    }

    // GC-CONSIDER: Consider keeping some page around
    GCETW(GC_DECOMMIT_CONCURRENT_COLLECT_PAGE_ALLOCATOR_START, (this));

    // Clean up mark contexts, which will release held free pages
    // Do this for all contexts before we decommit, to make sure all pages are freed
    markContext.Cleanup();
    parallelMarkContext1.Cleanup();
    parallelMarkContext2.Cleanup();
    parallelMarkContext3.Cleanup();

    // Decommit all pages
    markContext.DecommitPages();
    parallelMarkContext1.DecommitPages();
    parallelMarkContext2.DecommitPages();
    parallelMarkContext3.DecommitPages();

    GCETW(GC_DECOMMIT_CONCURRENT_COLLECT_PAGE_ALLOCATOR_STOP, (this));

    return oomRescan;
}

void
Recycler::EndMarkOnLowMemory()
{
    GCETW(GC_ENDMARKONLOWMEMORY_START, (this));
    Assert(this->NeedOOMRescan());
    this->inEndMarkOnLowMemory = true;

    // Treat this as a concurrent mark reset so that we don't invalidate the allocators
    RecyclerVerboseTrace(GetRecyclerFlagsTable(), L"OOM during mark- rerunning mark\n");

    // Try to release as much memory as possible
    ForRecyclerPageAllocator(DecommitNow());

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    uint iterations = 0;
#endif

    do
    {
        Assert(this->clientTrackedObjectList.Empty());

        // Always queue tracked objects during rescan, to avoid changes to mark state.
        // (Unless we're in a partial, in which case we ignore tracked objects)
        Assert(!this->DoQueueTrackedObject());
#ifdef PARTIAL_GC_ENABLED
        if (!this->inPartialCollectMode)
#endif
        {
            this->StartQueueTrackedObject();
        }

        this->collectionState = CollectionStateRescanFindRoots;

        this->ClearNeedOOMRescan();

#if DBG
        Assert(!this->isProcessingRescan);
        this->isProcessingRescan = true;
#endif

        if (!heapBlockMap.OOMRescan(this))
        {
            // Kill the process- we couldn't even rescan a single block
            // We are in pretty low memory state at this point
            // The fail-fast is present for two reasons:
            // 1) Defense-in-depth for cases we hadn't thought about
            // 2) Deal with cases like -MaxMarkStackPageCount:1 which can still hang without the fail-fast
            MarkStack_OOM_fatal_error();
        }

        autoHeap.Rescan(RescanFlags_None);

        DebugOnly(this->isProcessingRescan = false);

        this->ProcessMark(false);

        // Process any tracked objects we found
#ifdef PARTIAL_GC_ENABLED
        if (!this->inPartialCollectMode)
#endif
        {
            ProcessTrackedObjects();
        }

        // Drain the mark stack
        ProcessMark(false);

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        iterations++;
#endif
    }
    while (this->NeedOOMRescan());

    Assert(!markContext.GetPageAllocator()->DisableAllocationOutOfMemory());
    Assert(!parallelMarkContext1.GetPageAllocator()->DisableAllocationOutOfMemory());
    Assert(!parallelMarkContext2.GetPageAllocator()->DisableAllocationOutOfMemory());
    Assert(!parallelMarkContext3.GetPageAllocator()->DisableAllocationOutOfMemory());
    CUSTOM_PHASE_PRINT_TRACE1(GetRecyclerFlagsTable(), Js::RecyclerPhase, L"EndMarkOnLowMemory iterations: %d\n", iterations);

    Assert(this->clientTrackedObjectList.Empty());
    Assert(!this->DoQueueTrackedObject());
    this->inEndMarkOnLowMemory = false;
#ifdef PARTIAL_GC_ENABLED
    if (this->inPartialCollectMode)
    {
        this->FinishPartialCollect();
    }
#endif

    GCETW(GC_ENDMARKONLOWMEMORY_STOP, (this));
}


#if DBG
bool
Recycler::IsMarkStackEmpty()
{
    return (markContext.IsEmpty() && parallelMarkContext1.IsEmpty() && parallelMarkContext2.IsEmpty() && parallelMarkContext3.IsEmpty());
}
#endif

#ifdef HEAP_ENUMERATION_VALIDATION
void
Recycler::PostHeapEnumScan(PostHeapEnumScanCallback callback, void *data)
{
    this->pfPostHeapEnumScanCallback = callback;
    this->postHeapEnunScanData = data;

    FindRoots();
    ProcessMark(false);

    this->pfPostHeapEnumScanCallback = NULL;
    this->postHeapEnunScanData = NULL;
}
#endif

bool
Recycler::QueueTrackedObject(FinalizableObject * trackableObject)
{
    return markContext.AddTrackedObject(trackableObject);
}

bool
Recycler::FindImplicitRootObject(void* candidate, RecyclerHeapObjectInfo& heapObject)
{
    HeapBlock* heapBlock = FindHeapBlock(candidate);
    if (heapBlock == nullptr)
    {
        return false;
    }

    if (heapBlock->GetHeapBlockType() < HeapBlock::HeapBlockType::SmallAllocBlockTypeCount)
    {
        return ((SmallHeapBlock*)heapBlock)->FindImplicitRootObject(candidate, this, heapObject);
    }
    else if (!heapBlock->IsLargeHeapBlock())
    {
        return ((MediumHeapBlock*)heapBlock)->FindImplicitRootObject(candidate, this, heapObject);
    }
    else
    {
        return ((LargeHeapBlock*)heapBlock)->FindImplicitRootObject(candidate, this, heapObject);
    }
}

bool
Recycler::FindHeapObject(void* candidate, FindHeapObjectFlags flags, RecyclerHeapObjectInfo& heapObject)
{
    HeapBlock* heapBlock = FindHeapBlock(candidate);
    return heapBlock && heapBlock->FindHeapObject(candidate, this, flags, heapObject);
}

bool
Recycler::FindHeapObjectWithClearedAllocators(void* candidate, RecyclerHeapObjectInfo& heapObject)
{
    // Heap enum has some case where it allocates, so we can't assert
    Assert(autoHeap.AllocatorsAreEmpty() || this->isHeapEnumInProgress);
    return FindHeapObject(candidate, FindHeapObjectFlags_ClearedAllocators, heapObject);
}

void*
Recycler::GetRealAddressFromInterior(void* candidate)
{
    HeapBlock * heapBlock = heapBlockMap.GetHeapBlock(candidate);
    if (heapBlock == NULL)
    {
        return NULL;
    }
    return heapBlock->GetRealAddressFromInterior(candidate);
}

/*------------------------------------------------------------------------------------------------
 * Sweep
 *------------------------------------------------------------------------------------------------*/

bool
Recycler::Sweep(size_t rescanRootBytes, bool concurrent, bool adjustPartialHeuristics)
{
    Assert(!this->hasBackgroundFinishPartial);

#ifdef CONCURRENT_GC_ENABLED
    if (!this->enableConcurrentSweep)
#endif
    {
        concurrent = false;
    }

    RECYCLER_PROFILE_EXEC_BEGIN(this, concurrent? Js::ConcurrentSweepPhase : Js::SweepPhase);

    recyclerSweepInstance.BeginSweep(this, rescanRootBytes, adjustPartialHeuristics);
    this->SweepHeap(concurrent, *recyclerSweep);
    if (concurrent)
    {
        // If we finished mark in the background, all the relevant write watches should already be reset
        // Only reset write watch if we didn't finish mark in the background
        if (this->backgroundFinishMarkCount == 0)
        {
            if (this->inPartialCollectMode)
            {
                RECYCLER_PROFILE_EXEC_BEGIN(this, Js::ResetWriteWatchPhase);
                if (!recyclerPageAllocator.ResetWriteWatch() || !recyclerLargeBlockPageAllocator.ResetWriteWatch())
                {
                    // Shouldn't happen
                    Assert(false);
                    // Disable partial collect
                    this->enablePartialCollect = false;

                    // We haven't done any partial collection yet, just get out of partial collect mode
                    this->inPartialCollectMode = false;
                }
                RECYCLER_PROFILE_EXEC_END(this, Js::ResetWriteWatchPhase);
            }
        }
    }
    else
    {
        recyclerSweep->FinishSweep();
        recyclerSweep->EndSweep();
    }

    RECYCLER_PROFILE_EXEC_END(this, concurrent? Js::ConcurrentSweepPhase : Js::SweepPhase);

#ifdef CONCURRENT_GC_ENABLED
    if (concurrent)
    {
        if (!StartConcurrent(CollectionStateConcurrentSweep))
        {
           // Failed to spawn the concurrent sweep.
           // Instead, force the concurrent sweep to happen right here in thread.
           this->collectionState = CollectionStateConcurrentSweep;

           DoBackgroundWork(true);
           // Continue as if the concurrent sweep were executing
           // Next time we check for completion, we will finish the sweep just as if it had happened out of thread.
        }
        return true;
    }
#endif
    return false;
}

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
void Recycler::DisplayMemStats()
{
#ifdef PERF_COUNTERS
#if DBG_DUMP
    printf("Recycler Live Object Count  %u\n", PerfCounter::RecyclerCounterSet::GetLiveObjectCounter().GetValue());
    printf("Recycler Live Object Size   %u\n", PerfCounter::RecyclerCounterSet::GetLiveObjectSizeCounter().GetValue());
#endif

    printf("Recycler Used Page Size %u\n", PerfCounter::PageAllocatorCounterSet::GetUsedSizeCounter(PageAllocatorType::PageAllocatorType_Recycler).GetValue());
#endif
}
#endif

CollectedRecyclerWeakRefHeapBlock CollectedRecyclerWeakRefHeapBlock::Instance;

void
Recycler::SweepWeakReference()
{
    RECYCLER_PROFILE_EXEC_BEGIN(this, Js::SweepWeakPhase);
    GCETW(GC_SWEEP_WEAKREF_START, (this));

    // REVIEW: Clean up the weak reference map concurrently?
    bool hasCleanup = false;
    weakReferenceMap.Map([&hasCleanup](RecyclerWeakReferenceBase * weakRef) -> bool
    {
        if (!weakRef->weakRefHeapBlock->TestObjectMarkedBit(weakRef))
        {
            hasCleanup = true;

            // Remove
            return false;
        }

        if (!weakRef->strongRefHeapBlock->TestObjectMarkedBit(weakRef->strongRef))
        {
            hasCleanup = true;
            weakRef->strongRef = nullptr;

            // Put in a dummy heap block so that we can still do the isPendingConcurrentSweep check first.
            weakRef->strongRefHeapBlock = &CollectedRecyclerWeakRefHeapBlock::Instance;

            // Remove
            return false;
        }

        // Keep
        return true;
    });
    this->weakReferenceCleanupId += hasCleanup;

    GCETW(GC_SWEEP_WEAKREF_STOP, (this));
    RECYCLER_PROFILE_EXEC_END(this, Js::SweepWeakPhase);
}

void
Recycler::SweepHeap(bool concurrent, RecyclerSweep& recyclerSweep)
{
    Assert(!this->hasPendingDeleteGuestArena);
    Assert(!this->isHeapEnumInProgress);

#ifdef CONCURRENT_GC_ENABLED
    Assert(!this->DoQueueTrackedObject());
    if (concurrent)
    {
        collectionState = CollectionStateSetupConcurrentSweep;

        // Only queue up non-leaf pages- leaf pages don't need to be zeroed out
        recyclerPageAllocator.StartQueueZeroPage();
        recyclerLargeBlockPageAllocator.StartQueueZeroPage();
#ifdef RECYCLER_WRITE_BARRIER_ALLOC_SEPARATE_PAGE
        recyclerWithBarrierPageAllocator.StartQueueZeroPage();
#endif
    }
    else
#endif
    {
        Assert(!concurrent);
        collectionState = CollectionStateSweep;
    }

    this->SweepWeakReference();

#ifdef CONCURRENT_GC_ENABLED
    if (concurrent)
    {
        GCETW(GC_SETUPBACKGROUNDSWEEP_START, (this));
    }
    else
#endif
    {
        GCETW(GC_SWEEP_START, (this));
    }
    recyclerPageAllocator.SuspendIdleDecommit();
#ifdef RECYCLER_WRITE_BARRIER_ALLOC_SEPARATE_PAGE
    recyclerWithBarrierPageAllocator.SuspendIdleDecommit();
#endif
    recyclerLargeBlockPageAllocator.SuspendIdleDecommit();
    autoHeap.Sweep(recyclerSweep, concurrent);
#ifdef RECYCLER_WRITE_BARRIER_ALLOC_SEPARATE_PAGE
    recyclerWithBarrierPageAllocator.ResumeIdleDecommit();
#endif
    recyclerPageAllocator.ResumeIdleDecommit();
    recyclerLargeBlockPageAllocator.ResumeIdleDecommit();

#ifdef CONCURRENT_GC_ENABLED
    if (concurrent)
    {
        recyclerPageAllocator.StopQueueZeroPage();
        recyclerLargeBlockPageAllocator.StopQueueZeroPage();
#ifdef RECYCLER_WRITE_BARRIER_ALLOC_SEPARATE_PAGE
        recyclerWithBarrierPageAllocator.StopQueueZeroPage();
#endif

        GCETW(GC_SETUPBACKGROUNDSWEEP_STOP, (this));
    }
    else
    {
        Assert(!recyclerPageAllocator.HasZeroQueuedPages());
        Assert(!recyclerLargeBlockPageAllocator.HasZeroQueuedPages());
#ifdef RECYCLER_WRITE_BARRIER_ALLOC_SEPARATE_PAGE
        Assert(!recyclerWithBarrierPageAllocator.HasZeroQueuedPages());
#endif

        uint sweptBytes = 0;
#ifdef RECYCLER_STATS
        sweptBytes = (uint)collectionStats.objectSweptBytes;
#endif

        GCETW(GC_SWEEP_STOP, (this, sweptBytes));
    }
#endif
}

void
Recycler::BackgroundFinishPartialCollect(RecyclerSweep * recyclerSweep)
{
    Assert(this->inPartialCollectMode);
    Assert(recyclerSweep != nullptr && recyclerSweep->IsBackground());
    this->hasBackgroundFinishPartial = true;
    this->autoHeap.FinishPartialCollect(recyclerSweep);
    this->inPartialCollectMode = false;
}

void
Recycler::DisposeObjects()
{
    Assert(this->allowDispose && this->hasDisposableObject && !this->inDispose);
    Assert(!isHeapEnumInProgress);

    GCETW(GC_DISPOSE_START, (this));
    ASYNC_HOST_OPERATION_START(collectionWrapper);

    this->inDispose = true;

#ifdef PROFILE_RECYCLER_ALLOC
    // finalizer may allocate memory and dispose object can happen in the middle of allocation
    // save and restore the tracked object info
    TrackAllocData oldAllocData = { 0 };
    if (trackerDictionary != nullptr)
    {
        oldAllocData = nextAllocData;
        nextAllocData.Clear();
    }
#endif

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    if (GetRecyclerFlagsTable().Trace.IsEnabled(Js::RecyclerPhase))
    {
        Output::Print(L"Disposing objects\n");
    }
#endif

    // Disable dispose within this method, restore it when we're done
    AutoRestoreValue<bool> disableDispose(&this->allowDispose, false);

#ifdef FAULT_INJECTION
    this->collectionWrapper->DisposeScriptContextByFaultInjectionCallBack();
#endif

    // Scope timestamp to just dispose
    {
        AUTO_TIMESTAMP(dispose);
        autoHeap.DisposeObjects();
    }

#ifdef PROFILE_RECYCLER_ALLOC
    if (trackerDictionary != nullptr)
    {
        Assert(nextAllocData.IsEmpty());
        nextAllocData = oldAllocData;
    }
#endif

#ifdef ENABLE_PROJECTION
    {
        Assert(!this->inResolveExternalWeakReferences);
        Assert(!this->allowDispose);
#if DBG || defined RECYCLER_TRACE
        AutoRestoreValue<bool> inResolveExternalWeakReferencedObjects(&this->inResolveExternalWeakReferences, true);
#endif
        AUTO_TIMESTAMP(externalWeakReferenceObjectResolve);

        // This is where it is safe to resolve external weak references as they can lead to new script entry
        collectionWrapper->ResolveExternalWeakReferencedObjects();
    }
#endif

    Assert(!this->inResolveExternalWeakReferences);
    Assert(this->inDispose);

    this->inDispose = false;

    ASYNC_HOST_OPERATION_END(collectionWrapper);

    uint sweptBytes = 0;
#ifdef RECYCLER_STATS
    sweptBytes = (uint)collectionStats.objectSweptBytes;
#endif

    GCETW(GC_DISPOSE_STOP, (this, sweptBytes));
}

bool
Recycler::FinishDisposeObjects()
{
    CUSTOM_PHASE_PRINT_TRACE1(GetRecyclerFlagsTable(), Js::DisposePhase, L"[Dispose] AllowDispose in FinishDisposeObject: %d\n", this->allowDispose);

    if (this->hasDisposableObject && this->allowDispose)
    {
        CUSTOM_PHASE_PRINT_TRACE1(GetRecyclerFlagsTable(), Js::DisposePhase, L"[Dispose] FinishDisposeObject, calling Dispose: %d\n", this->allowDispose);
#ifdef RECYCLER_TRACE
        CollectionParam savedCollectionParam = collectionParam;
#endif
        DisposeObjects();
#ifdef RECYCLER_TRACE
        collectionParam = savedCollectionParam;
#endif
        // FinishDisposeObjects is always called either during a collection,
        // or we will check the NeedExhaustiveRepeatCollect(), so no need to check it here
        return true;
    }


#ifdef RECYCLER_TRACE
    if (!this->inDispose && this->hasDisposableObject
        && GetRecyclerFlagsTable().Trace.IsEnabled(Js::RecyclerPhase))
    {
        Output::Print(L"%04X> RC(%p): %s\n", this->mainThreadId, this, L"Dispose object delayed");
    }
#endif
    return false;
}

template bool Recycler::FinishDisposeObjectsNow<FinishDispose>();
template bool Recycler::FinishDisposeObjectsNow<FinishDisposeTimed>();

template <CollectionFlags flags>
bool
Recycler::FinishDisposeObjectsNow()
{
    if (inDisposeWrapper)
    {
        return false;
    }

    return FinishDisposeObjectsWrapped<flags>();
}

template <CollectionFlags flags>
__inline
bool
Recycler::FinishDisposeObjectsWrapped()
{
    const BOOL allowDisposeFlag = flags & CollectOverride_AllowDispose;
    if (allowDisposeFlag && this->NeedDispose())
    {
        if ((flags & CollectHeuristic_TimeIfScriptActive) == CollectHeuristic_TimeIfScriptActive)
        {
            if (!this->NeedDisposeTimed())
            {
                return false;
            }
        }

        this->allowDispose = true;
        this->inDisposeWrapper = true;

#ifdef RECYCLER_TRACE
        if (GetRecyclerFlagsTable().Trace.IsEnabled(Js::RecyclerPhase))
        {
            Output::Print(L"%04X> RC(%p): %s\n", this->mainThreadId, this, L"Process delayed dispose object");
        }
#endif

        collectionWrapper->DisposeObjects(this);

        // Dispose may get into message loop and cause a reentrant GC. If those don't allow reentrant
        // it will get added to a pending collect request.

        // FinishDipsosedObjectsWrapped/DisposeObjectsWrapped is called at a place that might not be during a collection
        // and won't check NeedExhaustiveRepeatCollect(), need to check it here to honor those requests

         if (!this->CollectionInProgress() && NeedExhaustiveRepeatCollect() && ((flags & CollectOverride_NoExhaustiveCollect) != CollectOverride_NoExhaustiveCollect))
        {
#ifdef RECYCLER_TRACE
            CaptureCollectionParam((CollectionFlags)(flags & ~CollectMode_Partial), true);
#endif
            DoCollectWrapped((CollectionFlags)(flags & ~CollectMode_Partial));
        }

        this->inDisposeWrapper = false;
        return true;
    }
    return false;
}

/*------------------------------------------------------------------------------------------------
 * Collect
 *------------------------------------------------------------------------------------------------*/
BOOL
Recycler::CollectOnAllocatorThread()
{
#ifdef PARTIAL_GC_ENABLED
    Assert(!inPartialCollectMode);
#endif
#ifdef RECYCLER_TRACE
    PrintCollectTrace(Js::GarbageCollectPhase);
#endif

    this->CollectionBegin<Js::GarbageCollectPhase>();
    this->Mark();

    // Partial collect mode is not re-enabled after a non-partial in-thread GC because partial GC heuristics are not adjusted
    // after a full in-thread GC. Enabling partial collect mode causes partial GC heuristics to be reset before the next full
    // in-thread GC, thereby allowing partial GC to kick in more easily without being able to adjust heuristics after the full
    // GCs. Until we have a way of adjusting partial GC heuristics after a full in-thread GC, once partial collect mode is
    // turned off, it will remain off until a concurrent GC happens
    this->Sweep();

    this->CollectionEnd<Js::GarbageCollectPhase>();

    FinishCollection();
    return true;
}

// Explicitly instantiate all possible modes

template BOOL Recycler::CollectNow<CollectOnScriptIdle>();
template BOOL Recycler::CollectNow<CollectOnScriptExit>();
template BOOL Recycler::CollectNow<CollectOnAllocation>();
template BOOL Recycler::CollectNow<CollectOnTypedArrayAllocation>();
template BOOL Recycler::CollectNow<CollectOnScriptCloseNonPrimary>();
template BOOL Recycler::CollectNow<CollectExhaustiveCandidate>();
template BOOL Recycler::CollectNow<CollectNowConcurrent>();
template BOOL Recycler::CollectNow<CollectNowExhaustive>();
template BOOL Recycler::CollectNow<CollectNowDecommitNowExplicit>();
template BOOL Recycler::CollectNow<CollectNowPartial>();
template BOOL Recycler::CollectNow<CollectNowConcurrentPartial>();
template BOOL Recycler::CollectNow<CollectNowForceInThread>();
template BOOL Recycler::CollectNow<CollectNowForceInThreadExternal>();
template BOOL Recycler::CollectNow<CollectNowForceInThreadExternalNoStack>();
template BOOL Recycler::CollectNow<CollectOnRecoverFromOutOfMemory>();
template BOOL Recycler::CollectNow<CollectNowDefault>();
template BOOL Recycler::CollectNow<CollectOnSuspendCleanup>();
template BOOL Recycler::CollectNow<CollectNowDefaultLSCleanup>();

#if defined(CHECK_MEMORY_LEAK) || defined(LEAK_REPORT)
template BOOL Recycler::CollectNow<CollectNowFinalGC>();
#endif

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
template BOOL Recycler::CollectNow<CollectNowExhaustiveSkipStack>();
#endif

template <CollectionFlags flags>
BOOL
Recycler::CollectNow()
{
    // Force-in-thread cannot be concurrent or partial
    CompileAssert((flags & CollectOverride_ForceInThread) == 0 || (flags & (CollectMode_Concurrent | CollectMode_Partial)) == 0);

    // Collections not allowed when the recycler is currently executing the PostCollectionCallback
    if (collectionState == CollectionStatePostCollectionCallback)
    {
        return false;
    }

#if ENABLE_DEBUG_CONFIG_OPTIONS
    if ((disableCollection && (flags & CollectOverride_Explicit) == 0) || isShuttingDown)
#else
    if (isShuttingDown)
#endif
    {
        Assert(collectionState == CollectionStateNotCollecting
            || collectionState == CollectionStateExit
            || this->isShuttingDown);
        return false;
    }

    if (flags & CollectOverride_ExhaustiveCandidate)
    {
        return CollectWithExhaustiveCandidate<flags>();
    }

    return CollectInternal<flags>();
}

template <CollectionFlags flags>
BOOL
Recycler::GetPartialFlag()
{
#ifdef PARTIAL_GC_ENABLED
#pragma prefast(suppress:6313, "flags is a template parameter and can be 0")
    return(flags & CollectMode_Partial) && inPartialCollectMode;
#else
    return false;
#endif
}

template <CollectionFlags flags>
BOOL
Recycler::CollectWithExhaustiveCandidate()
{
    Assert(flags & CollectOverride_ExhaustiveCandidate);

    // Currently we don't have any exhaustive candidate that has heuristic.
    Assert((flags & CollectHeuristic_Mask & ~CollectHeuristic_Never) == 0);

    this->hasExhaustiveCandidate = true;

    if (flags & CollectHeuristic_Never)
    {
        // This is just an exhaustive candidate notification. Don't trigger a GC.
        return false;
    }

    // Continue with the GC heuristic
    return CollectInternal<flags>();
 }


template <CollectionFlags flags>
BOOL
Recycler::CollectInternal()
{
    // CollectHeuristic_Never flag should only be used with exhaustive candidate
    Assert((flags & CollectHeuristic_Never) == 0);

    // If we're in a re-entrant state, we want to allow GC to be triggered only
    // from allocation (or trigger points with AllowReentrant). This is to minimize
    // the number of reentrant GCs
    if ((flags & CollectOverride_AllowReentrant) == 0 && this->inDispose)
    {
        return false;
    }

#ifdef RECYCLER_TRACE
    CaptureCollectionParam(flags);
#endif

#ifdef CONCURRENT_GC_ENABLED
    const BOOL concurrent = flags & CollectMode_Concurrent;
    const BOOL finishConcurrent = flags & CollectOverride_FinishConcurrent;

    // If we priority boosted, we should try to finish it every chance we get
    // Otherwise, we should finishing it if we are not doing a concurrent GC,
    // or the flags tell us to always try to finish a concurrent GC (CollectOverride_FinishConcurrent)
    if ((!concurrent || finishConcurrent || priorityBoost) && this->CollectionInProgress())
    {
        return TryFinishConcurrentCollect<flags>();
    }
#endif

    if (flags & CollectHeuristic_Mask)
    {
        // Check some heuristics first before starting a collection
        return CollectWithHeuristic<flags>();
    }

    // Start a collection now.
    return Collect<flags>();
}

template <CollectionFlags flags>
BOOL
Recycler::CollectWithHeuristic()
{
    // CollectHeuristic_Never flag should only be used with exhaustive candidate
    Assert((flags & CollectHeuristic_Never) == 0);

    const BOOL allocSize = flags & CollectHeuristic_AllocSize;
    const BOOL timedIfScriptActive = flags & CollectHeuristic_TimeIfScriptActive;
    const BOOL timedIfInScript = flags & CollectHeuristic_TimeIfInScript;
    const BOOL timed = (timedIfScriptActive && isScriptActive) || (timedIfInScript && isInScript) || (flags & CollectHeuristic_Time);

#ifdef PARTIAL_GC_ENABLED
    if (GetPartialFlag<flags>())
    {
        Assert(enablePartialCollect);
        Assert(allocSize);
        Assert(this->uncollectedNewPageCountPartialCollect >= RecyclerSweep::MinPartialUncollectedNewPageCount
            && this->uncollectedNewPageCountPartialCollect <= RecyclerHeuristic::Instance.MaxPartialUncollectedNewPageCount);

        // PARTIAL-GC-REVIEW: For now, we have only alloc size heuristic
        // Maybe improve this heuristic by looking at how many free pages are in the page allocator.
        if (autoHeap.uncollectedNewPageCount > this->uncollectedNewPageCountPartialCollect)
        {
            return Collect<flags>();
        }
    }
#endif

    // allocation byte count heuristic, collect every 1 MB allocated
    if (allocSize && (autoHeap.uncollectedAllocBytes < RecyclerHeuristic::UncollectedAllocBytesCollection()))
    {
        return FinishDisposeObjectsWrapped<flags>();
    }

    // time heuristic, allocate every 1000 clock tick, or 64 MB is allocated in a short time
    if (timed && (autoHeap.uncollectedAllocBytes < RecyclerHeuristic::Instance.MaxUncollectedAllocBytes))
    {
        uint currentTickCount = GetTickCount();
#ifdef RECYCLER_TRACE
        collectionParam.timeDiff = currentTickCount - tickCountNextCollection;
#endif
        if ((int)(tickCountNextCollection - currentTickCount) >= 0)
        {
            return FinishDisposeObjectsWrapped<flags>();
        }
    }
#ifdef RECYCLER_TRACE
    else
    {
        uint currentTickCount = GetTickCount();
        collectionParam.timeDiff = currentTickCount - tickCountNextCollection;
    }
#endif

    // Passed all the heuristic, do some GC work, maybe
    return Collect<(CollectionFlags)(flags & ~CollectMode_Partial)>();
}

template <CollectionFlags flags>
BOOL
Recycler::Collect()
{
#ifdef CONCURRENT_GC_ENABLED
    if (this->CollectionInProgress())
    {
        // If we are forced in thread, we can't be concurrent
        // If we are not concurrent we should have been handled before in CollectInternal and we shouldn't be here
        Assert((flags & CollectOverride_ForceInThread) == 0);
        Assert((flags & CollectMode_Concurrent) != 0);
        return TryFinishConcurrentCollect<flags>();
    }
#endif

    SetupPostCollectionFlags<flags>();

    const BOOL partial = GetPartialFlag<flags>();
    CollectionFlags finalFlags = flags;
    if (!partial)
    {
        finalFlags = (CollectionFlags)(flags & ~CollectMode_Partial);
    }

    // ExecuteRecyclerCollectionFunction may cause exception. In which case, we may trigger the assert
    // in SetupPostCollectionFlags because we didn't reset the inExhausitvECollection variable if
    // an exception. Use this flag to disable it the assertion if exception occur
    DebugOnly(this->hasIncompletedDoCollect = true);

    {
        RECORD_TIMESTAMP(initialCollectionStartTime);
        this->telemetryBlock->initialCollectionStartProcessUsedBytes = PageAllocator::GetProcessUsedBytes();
        this->telemetryBlock->exhaustiveRepeatedCount = 0;

        return DoCollectWrapped(finalFlags);
    }
}

template <CollectionFlags flags>
void Recycler::SetupPostCollectionFlags()
{
    // If we are not in a collection (collection in progress or in dispose), inExhastivecollection should not be set
    // Otherwise, we have missed an exhaustive collection.
    Assert(this->hasIncompletedDoCollect ||
        this->CollectionInProgress() || this->inDispose || (!this->inExhaustiveCollection && !this->inDecommitNowCollection));

    // Record whether we want to start exhaustive detection or do decommit now after GC
    const BOOL exhaustive = flags & CollectMode_Exhaustive;
    const BOOL decommitNow = flags & CollectMode_DecommitNow;
    const BOOL cacheCleanup = flags & CollectMode_CacheCleanup;

    if (decommitNow)
    {
        this->inDecommitNowCollection = true;
    }
    if (exhaustive)
    {
        this->inExhaustiveCollection = true;
    }
    if (cacheCleanup)
    {
        this->inCacheCleanupCollection = true;
    }
}

BOOL
Recycler::DoCollectWrapped(CollectionFlags flags)
{
#ifdef CONCURRENT_GC_ENABLED
    this->skipStack = ((flags & CollectOverride_SkipStack) != 0);
    DebugOnly(this->isConcurrentGCOnIdle = (flags == CollectOnScriptIdle));
#endif

    this->allowDispose = (flags & CollectOverride_AllowDispose) == CollectOverride_AllowDispose;
    BOOL collected = collectionWrapper->ExecuteRecyclerCollectionFunction(this, &Recycler::DoCollect, flags);

#ifdef CONCURRENT_GC_ENABLED
    Assert(IsConcurrentExecutingState() || IsConcurrentFinishedState() || !CollectionInProgress());
#else
    Assert(!CollectionInProgress());
#endif

    return collected;
}


bool
Recycler::NeedExhaustiveRepeatCollect() const
{
    return this->inExhaustiveCollection && this->hasExhaustiveCandidate;
}

BOOL
Recycler::DoCollect(CollectionFlags flags)
{
    // ExecuteRecyclerCollectionFunction may cause exception. In which case, we may trigger the assert
    // in SetupPostCollectionFlags because we didn't reset the inExhausitvECollection variable if
    // an exception. We are not in DoCollect, there shouldn't be any more exception. Reset the flag
    DebugOnly(this->hasIncompletedDoCollect = false);

#ifdef RECYCLER_MEMORY_VERIFY
    this->Verify(Js::RecyclerPhase);
#endif
#ifdef RECYCLER_FINALIZE_CHECK
    autoHeap.VerifyFinalize();
#endif
#ifdef PARTIAL_GC_ENABLED
    BOOL partial = flags & CollectMode_Partial;

#if DBG && defined(RECYCLER_DUMP_OBJECT_GRAPH)
    // Can't pass in RecyclerPartialStress and DumpObjectGraphOnCollect or call CollectGarbage with DumpObjectGraph
    if (GetRecyclerFlagsTable().RecyclerPartialStress) {
        Assert(!GetRecyclerFlagsTable().DumpObjectGraphOnCollect && !this->dumpObjectOnceOnCollect);
    } else if (GetRecyclerFlagsTable().DumpObjectGraphOnCollect || this->dumpObjectOnceOnCollect) {
        Assert(!GetRecyclerFlagsTable().RecyclerPartialStress);
    }
#endif

#ifdef RECYCLER_STRESS
    if (partial && GetRecyclerFlagsTable().RecyclerPartialStress)
    {
        this->inPartialCollectMode = true;
        this->forcePartialScanStack = true;
    }
#endif

#endif
#ifdef RECYCLER_DUMP_OBJECT_GRAPH
    if (dumpObjectOnceOnCollect || GetRecyclerFlagsTable().DumpObjectGraphOnCollect)
    {
        DumpObjectGraph();
        dumpObjectOnceOnCollect = false;

        // Can't do a partial collect if DumpObjectGraph is set since it'll call FinishPartial
        // which will set inPartialCollectMode to false.
        partial = false;
    }
#endif
#ifdef CONCURRENT_GC_ENABLED
    const bool concurrent = (flags & CollectMode_Concurrent) != 0;
    const BOOL forceInThread = flags & CollectOverride_ForceInThread;
#endif

    // Flush the pending dispose objects first if dispose is allowed
    Assert(!this->CollectionInProgress());
    Assert(this->backgroundFinishMarkCount == 0);

    bool collected = FinishDisposeObjects();

    do
    {
        INC_TIMESTAMP_FIELD(exhaustiveRepeatedCount);
        RECORD_TIMESTAMP(currentCollectionStartTime);
        this->telemetryBlock->currentCollectionStartProcessUsedBytes = PageAllocator::GetProcessUsedBytes();

#ifdef CONCURRENT_GC_ENABLED
        // DisposeObject may call script again and start another GC, so we may still be in concurrent GC state

        if (this->CollectionInProgress())
        {
            Assert(this->IsConcurrentState());
            Assert(collected);

            if (forceInThread)
            {
                return this->FinishConcurrentCollect(flags);
            }

            return true;
        }
#endif
        Assert(this->backgroundFinishMarkCount == 0);

#if DBG
        collectionCount++;
#endif
        collectionState = Collection_PreCollection;
        collectionWrapper->PreCollectionCallBack(flags);
        collectionState = CollectionStateNotCollecting;

        hasExhaustiveCandidate = false;         // reset the candidate detection

#ifdef RECYCLER_STATS
#ifdef PARTIAL_GC_ENABLED
        RecyclerCollectionStats oldCollectionStats = collectionStats;
#endif
        memset(&collectionStats, 0, sizeof(RecyclerCollectionStats));
        this->collectionStats.startCollectAllocBytes = autoHeap.uncollectedAllocBytes;
#ifdef PARTIAL_GC_ENABLED
        this->collectionStats.startCollectNewPageCount = autoHeap.uncollectedNewPageCount;
        this->collectionStats.uncollectedNewPageCountPartialCollect = this->uncollectedNewPageCountPartialCollect;
#endif
#endif

#ifdef PARTIAL_GC_ENABLED
        if (partial)
        {
#ifdef CONCURRENT_GC_ENABLED
            Assert(!forceInThread);
#endif
#ifdef RECYCLER_STATS
            // We are only doing a partial GC, copy some old stats
            collectionStats.finalizeCount = oldCollectionStats.finalizeCount;
            memcpy(collectionStats.heapBlockCount, oldCollectionStats.smallNonLeafHeapBlockPartialUnusedCount,
                sizeof(oldCollectionStats.smallNonLeafHeapBlockPartialUnusedCount));
            memcpy(collectionStats.heapBlockFreeByteCount, oldCollectionStats.smallNonLeafHeapBlockPartialUnusedBytes,
                sizeof(oldCollectionStats.smallNonLeafHeapBlockPartialUnusedBytes));
            memcpy(collectionStats.smallNonLeafHeapBlockPartialUnusedCount, oldCollectionStats.smallNonLeafHeapBlockPartialUnusedCount,
                sizeof(oldCollectionStats.smallNonLeafHeapBlockPartialUnusedCount));
            memcpy(collectionStats.smallNonLeafHeapBlockPartialUnusedBytes, oldCollectionStats.smallNonLeafHeapBlockPartialUnusedBytes,
                sizeof(oldCollectionStats.smallNonLeafHeapBlockPartialUnusedBytes));
#endif
            Assert(enablePartialCollect && inPartialCollectMode);

            if (!this->PartialCollect(concurrent))
            {
                return collected;
            }
            // This disable partial if we do a repeated exhaustive GC
            partial = false;
            collected = true;
            continue;
        }

        // Not doing partial collect, we should decommit on finish collect
        decommitOnFinish = true;

        if (inPartialCollectMode)
        {
            // finish the partial collect first
            FinishPartialCollect();

            // Old heap block with free object is made available, count that as being collected
            collected = true;
            // PARTIAL-GC-CONSIDER: should we just pretend we did a GC, since we have made the free listed object
            // available to be used, instead of starting off another GC?
        }
#endif

#ifdef CONCURRENT_GC_ENABLED

        bool skipConcurrent = false;
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS

        // If the below flag is passed in, skip doing a non-blocking concurrent collect. Instead,
        // we will do a blocking concurrent collect, which is basically an in-thread GC
        skipConcurrent = GetRecyclerFlagsTable().ForceBlockingConcurrentCollect;
#endif

        // We are about to start a collection. Reset our heuristic counters now, so that
        // any allocations that occur during concurrent collection count toward the next collection's threshold.
        ResetHeuristicCounters();

        if (concurrent && !skipConcurrent)
        {
            Assert(!forceInThread);
            if (enableConcurrentMark)
            {
                if (StartBackgroundMarkCollect())
                {
                    // Tell the caller whether we have finish a collection and there maybe free object to reuse
                    return collected;
                }

                // Either ResetWriteWatch failed or the thread service failed
                // So concurrent mark is disabled, at least for now
            }

            if (enableConcurrentSweep)
            {
                if (StartConcurrentSweepCollect())
                {
                    collected = true;
                    continue;
                }

                // out of memory during collection
                return collected;
            }

            // concurrent collection failed, default back to non-concurrent collection
        }

        if (!forceInThread && enableConcurrentMark)
        {
            if (!CollectOnConcurrentThread())
            {
                // time out or out of memory during collection
                return collected;
            }
         }
        else
#endif
        {
            if (!CollectOnAllocatorThread())
            {
                // out of memory during collection
                return collected;
            }
        }

        collected = true;
#ifdef RECYCLER_TRACE
        collectionParam.repeat = true;
#endif
    }
    while (this->NeedExhaustiveRepeatCollect());

#ifdef CONCURRENT_GC_ENABLED
    // DisposeObject may call script again and start another GC, so we may still be in concurrent GC state

    if (this->CollectionInProgress())
    {
        Assert(this->IsConcurrentState());
        Assert(collected);
        return true;
    }
#endif

    EndCollection();

    // Tell the caller whether we have finish a collection and there maybe free object to reuse
    return collected;
}

void
Recycler::EndCollection()
{
    Assert(this->backgroundFinishMarkCount == 0);
    Assert(!this->CollectionInProgress());

    // no more collection is requested, we can turn exhaustive back off
    this->inExhaustiveCollection = false;
    if (this->inDecommitNowCollection || CUSTOM_CONFIG_FLAG(GetRecyclerFlagsTable(), ForceDecommitOnCollect))
    {
#ifdef RECYCLER_TRACE
        if (GetRecyclerFlagsTable().Trace.IsEnabled(Js::RecyclerPhase))
        {
            Output::Print(L"%04X> RC(%p): %s\n", this->mainThreadId, this, L"Decommit now");
        }
#endif
        ForRecyclerPageAllocator(DecommitNow());
        this->inDecommitNowCollection = false;
    }

    RECORD_TIMESTAMP(lastCollectionEndTime);
}


#ifdef PARTIAL_GC_ENABLED

bool
Recycler::PartialCollect(bool concurrent)
{
    Assert(IsMarkStackEmpty());
    Assert(this->inPartialCollectMode);
    Assert(collectionState == CollectionStateNotCollecting);
    // Rescan again
    collectionState = CollectionStateRescanFindRoots;
    if (concurrent && enableConcurrentMark && this->partialConcurrentNextCollection)
    {
        this->PrepareBackgroundFindRoots();
        if (StartConcurrent(CollectionStateConcurrentFinishMark))
        {
#ifdef RECYCLER_TRACE
            PrintCollectTrace(Js::ConcurrentPartialCollectPhase);
#endif
            return false;
        }
        this->RevertPrepareBackgroundFindRoots();
    }

#ifdef RECYCLER_STRESS
    if (forcePartialScanStack)
    {
        // Mark the roots since they need not have been marked
        // in RecyclerPartialStress mode
        this->RootMark(collectionState);
    }
#endif

#ifdef RECYCLER_TRACE
    PrintCollectTrace(Js::PartialCollectPhase);
#endif

    bool needConcurrentSweep = false;
    this->CollectionBegin<Js::PartialCollectPhase>();
    size_t rescanRootBytes = FinishMark(INFINITE);
    Assert(rescanRootBytes != Recycler::InvalidScanRootBytes);

    needConcurrentSweep = this->Sweep(rescanRootBytes, concurrent, true);

    this->CollectionEnd<Js::PartialCollectPhase>();

    // Only reset the new page counter
    autoHeap.uncollectedNewPageCount = 0;

    // Finish collection
    FinishCollection(needConcurrentSweep);
    return true;
}

void
Recycler::ProcessClientTrackedObjects()
{
    GCETW(GC_PROCESS_CLIENT_TRACKED_OBJECT_START, (this));

    Assert(this->inPartialCollectMode);
#ifdef CONCURRENT_GC_ENABLED
    Assert(!this->DoQueueTrackedObject());
#endif

    if (!this->clientTrackedObjectList.Empty())
    {
        SListBase<void *>::Iterator iter(&this->clientTrackedObjectList);
        while (iter.Next())
        {
            auto& reference = iter.Data();
            this->TryMarkNonInterior(reference, &reference /* parentReference */);  // Reference to inside the node
            RECYCLER_STATS_INC(this, clientTrackedObjectCount);
        }

        this->clientTrackedObjectList.Clear(&this->clientTrackedObjectAllocator);
    }

    GCETW(GC_PROCESS_CLIENT_TRACKED_OBJECT_STOP, (this));
}

void
Recycler::ClearPartialCollect()
{
    Assert(!this->DoQueueTrackedObject());
    this->autoHeap.unusedPartialCollectFreeBytes = 0;
    this->partialUncollectedAllocBytes = 0;
    this->clientTrackedObjectList.Clear(&this->clientTrackedObjectAllocator);
    this->uncollectedNewPageCountPartialCollect = (size_t)-1;
}

void
Recycler::FinishPartialCollect(RecyclerSweep * recyclerSweep)
{
    Assert(recyclerSweep == nullptr || !recyclerSweep->IsBackground());
    RECYCLER_PROFILE_EXEC_BEGIN(this, Js::FinishPartialPhase);
    Assert(inPartialCollectMode);
#ifdef CONCURRENT_GC_ENABLED
    Assert(!this->DoQueueTrackedObject());
#endif

    autoHeap.FinishPartialCollect(recyclerSweep);
    this->inPartialCollectMode = false;
    ClearPartialCollect();
    RECYCLER_PROFILE_EXEC_END(this, Js::FinishPartialPhase);
}
#endif
void
Recycler::EnsureNotCollecting()
{
#ifdef CONCURRENT_GC_ENABLED
    FinishConcurrent<ForceFinishCollection>();
#endif
    Assert(!this->CollectionInProgress());
}

void Recycler::EnumerateObjects(ObjectInfoBits infoBits, void (*CallBackFunction)(void * address, size_t size))
{
    // Make sure we are not collecting
    EnsureNotCollecting();

#ifdef PARTIAL_GC_ENABLED
    // We are updating the free bit vector, messing up the partial collection state.
    // Just get out of partial collect mode
    // GC-CONSIDER: consider adding an option in FinishConcurrent to not get into partial collect mode during sweep.
    if (inPartialCollectMode)
    {
        FinishPartialCollect();
    }
#endif

    autoHeap.EnumerateObjects(infoBits, CallBackFunction);
    // GC-TODO: Explicit heap?
}

BOOL
Recycler::IsMarkState() const
{
    return (collectionState & Collection_Mark);
}

BOOL
Recycler::IsFindRootsState() const
{
    return (collectionState & Collection_FindRoots);
}

#if DBG
BOOL
Recycler::IsReentrantState() const
{
#ifdef CONCURRENT_GC_ENABLED
    return !this->CollectionInProgress() || this->IsConcurrentState();
#else
    return !this->CollectionInProgress();
#endif
}
#endif

#ifdef ENABLE_JS_ETW
template <Js::Phase phase> static ETWEventGCActivationKind GetETWEventGCActivationKind();
template <> static ETWEventGCActivationKind GetETWEventGCActivationKind<Js::GarbageCollectPhase>() { return ETWEvent_GarbageCollect; }
template <> static ETWEventGCActivationKind GetETWEventGCActivationKind<Js::ThreadCollectPhase>() { return ETWEvent_ThreadCollect; }
template <> static ETWEventGCActivationKind GetETWEventGCActivationKind<Js::ConcurrentCollectPhase>() { return ETWEvent_ConcurrentCollect; }
template <> static ETWEventGCActivationKind GetETWEventGCActivationKind<Js::PartialCollectPhase>() { return ETWEvent_PartialCollect; }
#endif

template <Js::Phase phase>
void
Recycler::CollectionBegin()
{
    RECYCLER_PROFILE_EXEC_BEGIN2(this, Js::RecyclerPhase, phase);
    GCETW(GC_START, (this, GetETWEventGCActivationKind<phase>()));
#ifdef ENABLE_BASIC_TELEMETRY
    if (this->IsMemProtectMode() == false)
    {
        gcTel.LogGCPauseStartTime();
    }
#endif
}

template <Js::Phase phase>
void
Recycler::CollectionEnd()
{
    GCETW(GC_STOP, (this, GetETWEventGCActivationKind<phase>()));
#ifdef ENABLE_BASIC_TELEMETRY
    if (GetCurrentThreadContextId() == mainThreadId && IsMemProtectMode() == false)
    {
        gcTel.LogGCPauseEndTime();
    }
#endif
    RECYCLER_PROFILE_EXEC_END2(this, phase, Js::RecyclerPhase);
}

#ifdef CONCURRENT_GC_ENABLED
size_t
Recycler::BackgroundRescan(RescanFlags rescanFlags)
{
    Assert(!this->isProcessingRescan);

    DebugOnly(this->isProcessingRescan = true);

    GCETW(GC_BACKGROUNDRESCAN_START, (this, backgroundRescanCount));
    RECYCLER_PROFILE_EXEC_BACKGROUND_BEGIN(this, Js::BackgroundRescanPhase);

    size_t rescannedPageCount = heapBlockMap.Rescan(this, ((rescanFlags & RescanFlags_ResetWriteWatch) != 0));

    rescannedPageCount += autoHeap.Rescan(rescanFlags);

    RECYCLER_PROFILE_EXEC_BACKGROUND_END(this, Js::BackgroundRescanPhase);
    GCETW(GC_BACKGROUNDRESCAN_STOP, (this, backgroundRescanCount));
    this->backgroundRescanCount++;

    if (!this->NeedOOMRescan())
    {
        if ((rescanFlags & RescanFlags_ResetWriteWatch) != 0)
        {
            DebugOnly(this->isProcessingRescan = false);
        }

        return rescannedPageCount;
    }

    DebugOnly(this->isProcessingRescan = false);

    return Recycler::InvalidScanRootBytes;
}

void
Recycler::BackgroundResetWriteWatchAll()
{
    GCETW(GC_BACKGROUNDRESETWRITEWATCH_START, (this, -1));
    heapBlockMap.ResetWriteWatch(this);
    GCETW(GC_BACKGROUNDRESETWRITEWATCH_STOP, (this, -1));
}
#endif

#if defined(PARTIAL_GC_ENABLED) || defined(CONCURRENT_GC_ENABLED)
size_t
Recycler::FinishMarkRescan(bool background)
{
    if (background)
    {
        GCETW(GC_BACKGROUNDRESCAN_START, (this, 0));
    }
    else
    {
        GCETW(GC_RESCAN_START, (this));
    }

    RECYCLER_PROFILE_EXEC_THREAD_BEGIN(background, this, Js::RescanPhase);

#ifdef CONCURRENT_GC_ENABLED
    RescanFlags const flags = (background ? RescanFlags_ResetWriteWatch : RescanFlags_None);
#else
    Assert(!background);
    RescanFlags const flags = RescanFlags_None;
#endif

#if DBG
    Assert(!this->isProcessingRescan);
    this->isProcessingRescan = true;
#endif

    size_t scannedPageCount = heapBlockMap.Rescan(this, ((flags & RescanFlags_ResetWriteWatch) != 0));

    scannedPageCount += autoHeap.Rescan(flags);

    DebugOnly(this->isProcessingRescan = false);

    RECYCLER_PROFILE_EXEC_THREAD_END(background, this, Js::RescanPhase);

    if (background)
    {
        GCETW(GC_BACKGROUNDRESCAN_STOP, (this, 0));
    }
    else
    {
        GCETW(GC_RESCAN_STOP, (this));
    }

    return scannedPageCount;
}


void
Recycler::ProcessTrackedObjects()
{
    GCETW(GC_PROCESS_TRACKED_OBJECT_START, (this));

#ifdef PARTIAL_GC_ENABLED
    Assert(this->clientTrackedObjectList.Empty());
    Assert(!this->inPartialCollectMode);
#endif
#ifdef CONCURRENT_GC_ENABLED
    Assert(this->DoQueueTrackedObject());

    this->queueTrackedObject = false;
    DebugOnly(this->isProcessingTrackedObjects = true);

    markContext.ProcessTracked();

    // If we did a parallel mark, we need to process any queued tracked objects from the parallel mark stack as well.
    // If we didn't, this will do nothing.
    parallelMarkContext1.ProcessTracked();
    parallelMarkContext2.ProcessTracked();
    parallelMarkContext3.ProcessTracked();

    DebugOnly(this->isProcessingTrackedObjects = false);
#endif

    GCETW(GC_PROCESS_TRACKED_OBJECT_STOP, (this));
}

#endif // defined(PARTIAL_GC_ENABLED) || defined(CONCURRENT_GC_ENABLED)

BOOL
Recycler::RequestConcurrentWrapperCallback()
{
#ifdef CONCURRENT_GC_ENABLED
    Assert(!IsConcurrentExecutingState());

    // Save the original collection state
    CollectionState oldState = this->collectionState;

    // Get the background thread to start the callback
    if (StartConcurrent(CollectionStateConcurrentWrapperCallback))
    {
        // Wait for the callback to complete
        WaitForConcurrentThread(INFINITE);

        // The state must not change back until we restore the original state
        Assert(collectionState == CollectionStateConcurrentWrapperCallback);
        this->collectionState = oldState;

        return true;
    }
#endif
    return false;
}

#ifdef CONCURRENT_GC_ENABLED
/*------------------------------------------------------------------------------------------------
 * Concurrent
 *------------------------------------------------------------------------------------------------*/
BOOL
Recycler::CollectOnConcurrentThread()
{
#ifdef PARTIAL_GC_ENABLED
    Assert(!inPartialCollectMode);
#endif
#ifdef RECYCLER_TRACE
    PrintCollectTrace(Js::ThreadCollectPhase);
#endif
    this->CollectionBegin<Js::ThreadCollectPhase>();
    // Synchronous concurrent mark
    if (!StartSynchronousBackgroundMark())
    {
        this->CollectionEnd<Js::ThreadCollectPhase>();
        return false;
    }

    const DWORD waitTime = RecyclerHeuristic::FinishConcurrentCollectWaitTime(this->GetRecyclerFlagsTable());
    GCETW(GC_SYNCHRONOUSMARKWAIT_START, (this, waitTime));
    const BOOL waited = WaitForConcurrentThread(waitTime);
    GCETW(GC_SYNCHRONOUSMARKWAIT_STOP, (this, !waited));
    if (!waited)
    {
#ifdef RECYCLER_TRACE
        if (GetRecyclerFlagsTable().Trace.IsEnabled(Js::RecyclerPhase)
            || GetRecyclerFlagsTable().Trace.IsEnabled(Js::ThreadCollectPhase))
        {
            Output::Print(L"%04X> RC(%p): %s: %s\n", this->mainThreadId, this, Js::PhaseNames[Js::ThreadCollectPhase], L"Timeout");
        }
#endif
        this->CollectionEnd<Js::ThreadCollectPhase>();

        return false;
    }

    // If the concurrent thread was done within the time limit, there shouldn't be
    // any object needs to be rescanned
    // CONCURRENT-TODO: Optimize it so we don't rescan in the background if we are still waiting
    // GC-TODO: Unfortunately we can't assert this, as the background code gen thread may still
    // touch GC memory (e.g. FunctionBody), causing write watch and rescan
    // in the background.
    // Assert(markContext.Empty());
    DebugOnly(this->isProcessingRescan = false);

    this->collectionState = CollectionStateMark;
    this->ProcessTrackedObjects();
    this->ProcessMark(false);
    this->EndMark();

    // Partial collect mode is not re-enabled after a non-partial in-thread GC because partial GC heuristics are not adjusted
    // after a full in-thread GC. Enabling partial collect mode causes partial GC heuristics to be reset before the next full
    // in-thread GC, thereby allowing partial GC to kick in more easily without being able to adjust heuristics after the full
    // GCs. Until we have a way of adjusting partial GC heuristics after a full in-thread GC, once partial collect mode is
    // turned off, it will remain off until a concurrent GC happens
    this->Sweep();
    this->CollectionEnd<Js::ThreadCollectPhase>();
    FinishCollection();
    return true;
}

// explicit instantiation
template BOOL Recycler::FinishConcurrent<FinishConcurrentOnIdle>();
template BOOL Recycler::FinishConcurrent<FinishConcurrentOnIdleAtRoot>();
template BOOL Recycler::FinishConcurrent<FinishConcurrentOnExitScript>();
template BOOL Recycler::FinishConcurrent<FinishConcurrentOnEnterScript>();
template BOOL Recycler::FinishConcurrent<ForceFinishCollection>();

template <CollectionFlags flags>
BOOL
Recycler::FinishConcurrent()
{
    CompileAssert((flags & ~(CollectOverride_AllowDispose | CollectOverride_ForceFinish | CollectOverride_ForceInThread
        | CollectMode_Concurrent | CollectOverride_DisableIdleFinish | CollectOverride_BackgroundFinishMark
        | CollectOverride_SkipStack | CollectOverride_FinishConcurrentTimeout)) == 0);

    if (this->CollectionInProgress())
    {
        Assert(this->IsConcurrentEnabled());
        Assert(IsConcurrentState());

        const BOOL forceFinish = flags & CollectOverride_ForceFinish;

        if (forceFinish || !IsConcurrentExecutingState())
        {
            if (this->collectionState == CollectionStateConcurrentSweep)
            {
                // Help with the background thread to zero and flush zero pages
                // if we are going to wait anyways.
                recyclerPageAllocator.ZeroQueuedPages();
                recyclerLargeBlockPageAllocator.ZeroQueuedPages();
#ifdef RECYCLER_WRITE_BARRIER_ALLOC_SEPARATE_PAGE
                recyclerWithBarrierPageAllocator.ZeroQueuedPages();
#endif

                this->FlushBackgroundPages();
            }
#ifdef RECYCLER_TRACE
            collectionParam.finishOnly = true;
            collectionParam.flags = flags;
#endif
#ifdef CONCURRENT_GC_ENABLED
            // If SkipStack is provided, and we're not forcing the finish (i.e we're not in concurrent executing state)
            // then, it's fine to set the skipStack flag to true, so that during the in-thread find-roots, we'll skip
            // the stack scan
            this->skipStack = ((flags & CollectOverride_SkipStack) != 0) && !forceFinish;
#if DBG
            this->isFinishGCOnIdle = (flags == FinishConcurrentOnIdleAtRoot);
#endif
#endif

            return FinishConcurrentCollectWrapped(flags);
        }
    }

    return false;
}


template <CollectionFlags flags>
BOOL
Recycler::TryFinishConcurrentCollect()
{
    Assert(this->CollectionInProgress());

    RECYCLER_STATS_INC(this, finishCollectTryCount);

    SetupPostCollectionFlags<flags>();
    const BOOL concurrent = flags & CollectMode_Concurrent;
    const BOOL forceInThread = flags & CollectOverride_ForceInThread;

    Assert(this->IsConcurrentEnabled());
    Assert(IsConcurrentState() || IsCollectionDisabled());
    Assert(!concurrent || !forceInThread);
    if (concurrent && concurrentThread != NULL)
    {
        if (IsConcurrentExecutingState())
        {
            if (!this->priorityBoost)
            {
                uint tickCount = GetTickCount();
                if ((autoHeap.uncollectedAllocBytes > RecyclerHeuristic::Instance.UncollectedAllocBytesConcurrentPriorityBoost)
                    || (tickCount - this->tickCountStartConcurrent > RecyclerHeuristic::PriorityBoostTimeout(this->GetRecyclerFlagsTable())))
                {

    #ifdef RECYCLER_TRACE
                    if (GetRecyclerFlagsTable().Trace.IsEnabled(Js::RecyclerPhase))
                    {
                        Output::Print(L"%04X> RC(%p): %s: ", this->mainThreadId, this, L"Set priority normal");
                        if (autoHeap.uncollectedAllocBytes > RecyclerHeuristic::Instance.UncollectedAllocBytesConcurrentPriorityBoost)
                        {
                            Output::Print(L"AllocBytes=%d (Time=%d)\n", autoHeap.uncollectedAllocBytes, tickCount - this->tickCountStartConcurrent);
                        }
                        else
                        {
                            Output::Print(L"Time=%d (AllocBytes=%d\n", tickCount - this->tickCountStartConcurrent, autoHeap.uncollectedAllocBytes);
                        }
                    }
    #endif
                    // Set it to a large number so we don't set the thread priority again
                    this->priorityBoost = true;

                    // The recycler thread hasn't come back in 5 seconds
                    // It either has a large object graph, or it is starving.
                    // Set the priority back to normal
                    SetThreadPriority(this->concurrentThread, THREAD_PRIORITY_NORMAL);
                }
            }

            return FinishDisposeObjectsWrapped<flags>();
        }
        else if ((flags & CollectOverride_FinishConcurrentTimeout) != 0)
        {
            uint tickCount = GetTickCount();

            // If we haven't gone past the time to call finish collection,
            // simply call FinishDisposeObjects and return
            // Otherwise, actually go ahead and call FinishConcurrentCollectWrapped
            // We do this only if this is a collection that allows finish concurrent to timeout
            // If not, by default, we finish the collection
            if (tickCount <= this->tickCountNextFinishCollection)
            {
                return FinishDisposeObjectsWrapped<flags>();
            }
        }
    }

    return FinishConcurrentCollectWrapped(flags);
}

BOOL
Recycler::IsConcurrentMarkState() const
{
    return (collectionState & Collection_ConcurrentMark) == Collection_ConcurrentMark;
}

BOOL
Recycler::IsConcurrentMarkExecutingState() const
{
    return (collectionState & (Collection_ConcurrentMark | Collection_ExecutingConcurrent)) == (Collection_ConcurrentMark | Collection_ExecutingConcurrent);
}

BOOL
Recycler::IsConcurrentResetMarksState() const
{
    return collectionState == CollectionStateConcurrentResetMarks;
}

BOOL
Recycler::IsInThreadFindRootsState() const
{
    CollectionState currentCollectionState = collectionState;
    return (currentCollectionState & Collection_FindRoots) && (currentCollectionState != CollectionStateConcurrentFindRoots);
}

BOOL
Recycler::IsConcurrentFindRootState() const
{
    return collectionState == CollectionStateConcurrentFindRoots;
}

BOOL
Recycler::IsConcurrentExecutingState() const
{
    return (collectionState & Collection_ExecutingConcurrent);
}

BOOL
Recycler::IsConcurrentSweepExecutingState() const
{
    return (collectionState & (Collection_ConcurrentSweep | Collection_ExecutingConcurrent)) == (Collection_ConcurrentSweep | Collection_ExecutingConcurrent);
}

BOOL
Recycler::IsConcurrentState() const
{
    return (collectionState & Collection_Concurrent);
}

#if DBG
BOOL
Recycler::IsConcurrentFinishedState() const
{
    return (collectionState & Collection_FinishConcurrent);
}
#endif

bool
Recycler::InitializeConcurrent(JsUtil::ThreadService *threadService)
{
    try
    {
        AUTO_NESTED_HANDLED_EXCEPTION_TYPE(ExceptionType_OutOfMemory);

        concurrentWorkDoneEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
        if (concurrentWorkDoneEvent == nullptr)
        {
            throw Js::OutOfMemoryException();
        }

#if DBG_DUMP
        markContext.GetPageAllocator()->debugName = L"ConcurrentCollect";
#endif
        if (!threadService->HasCallback())
        {
#ifdef IDLE_DECOMMIT_ENABLED
            concurrentIdleDecommitEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
            if (concurrentIdleDecommitEvent == nullptr)
            {
                throw Js::OutOfMemoryException();
            }
#endif

            concurrentWorkReadyEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
            if (concurrentWorkReadyEvent == nullptr)
            {
                throw Js::OutOfMemoryException();
            }
        }
    }
    catch (Js::OutOfMemoryException)
    {
        Assert(concurrentWorkReadyEvent == nullptr);
        if (concurrentWorkDoneEvent)
        {
            CloseHandle(concurrentWorkDoneEvent);
            concurrentWorkDoneEvent = nullptr;
        }
#ifdef IDLE_DECOMMIT_ENABLED
        if (concurrentIdleDecommitEvent)
        {
            CloseHandle(concurrentIdleDecommitEvent);
            concurrentIdleDecommitEvent = nullptr;
        }
#endif

        return false;
    }

    return true;
}

#pragma prefast(suppress:6262, "Where this function is call should have ample of stack space")
bool Recycler::AbortConcurrent(bool restoreState)
{
    Assert(!this->CollectionInProgress() || this->IsConcurrentState());

    // In case the thread already died, wait for that too
    HANDLE handle[2] = { concurrentWorkDoneEvent, concurrentThread };

    // Note, concurrentThread will be null if we have a threadService.
    Assert(concurrentThread != NULL || threadService->HasCallback());
    DWORD handleCount = (concurrentThread == NULL ? 1 : 2);

    DWORD ret = WAIT_OBJECT_0;
    if (this->IsConcurrentState())
    {
        this->isAborting = true;

        if (this->concurrentThread != NULL)
        {
            SetThreadPriority(this->concurrentThread, THREAD_PRIORITY_NORMAL);
        }

        ret = WaitForMultipleObjectsEx(handleCount, handle, FALSE, INFINITE, FALSE);

        this->isAborting = false;

        Assert(this->IsConcurrentFinishedState() || ret == WAIT_OBJECT_0 + 1);

        if (ret == WAIT_OBJECT_0 && restoreState)
        {
            if (collectionState == CollectionStateRescanWait)
            {
                this->ResetMarkCollectionState();
            }
            else if (collectionState == CollectionStateTransferSweptWait)
            {
                // Make sure we don't do another GC after finishing this one.
                this->inExhaustiveCollection = false;

                // Let's just finish the sweep so that GC is in a consistent state, but don't run dispose

                // AbortConcurrent already consumed the event from the concurrent thread, just signal it so
                // FinishConcurrentCollect can wait for it again.
                SetEvent(this->concurrentWorkDoneEvent);

                EnsureNotCollecting();
            }
            else
            {
                Assert(UNREACHED);
            }

            Assert(collectionState == CollectionStateNotCollecting);
            Assert(this->isProcessingRescan == false);
        }
        else
        {
            // Even if we weren't asked to restore states, we need to clean up the pending guest arena
            CleanupPendingUnroot();

            // Also need to release any pages held by the mark stack, if we abandoned it
            markContext.Abort();
        }
    }

    Assert(!this->hasPendingDeleteGuestArena);
    return ret == WAIT_OBJECT_0;
}

void
Recycler::CleanupPendingUnroot()
{
    Assert(!this->hasPendingConcurrentFindRoot);
    if (hasPendingUnpinnedObject)
    {
        pinnedObjectMap.MapAndRemoveIf([](void * obj, PinRecord const &refCount)
        {
#if defined(CHECK_MEMORY_LEAK) || defined(LEAK_REPORT)
            Assert(refCount != 0 || refCount.stackBackTraces == nullptr);
#endif
            return refCount == 0;
        });
        hasPendingUnpinnedObject = false;
    }

    if (hasPendingDeleteGuestArena)
    {
        DebugOnly(bool foundPendingDelete = false);
        DListBase<GuestArenaAllocator>::EditingIterator guestArenaIter(&guestArenaList);
        while (guestArenaIter.Next())
        {
            GuestArenaAllocator& allocator = guestArenaIter.Data();
            if (allocator.pendingDelete)
            {
                allocator.SetLockBlockList(false);
                guestArenaIter.RemoveCurrent(&HeapAllocator::Instance);
                DebugOnly(foundPendingDelete = true);
            }
        }
        hasPendingDeleteGuestArena = false;
        Assert(foundPendingDelete);
    }
#if DBG
    else
    {
        DListBase<GuestArenaAllocator>::Iterator guestArenaIter(&guestArenaList);
        while (guestArenaIter.Next())
        {
            GuestArenaAllocator& allocator = guestArenaIter.Data();
            Assert(!allocator.pendingDelete);
        }
    }
#endif
}

void
Recycler::FinalizeConcurrent(bool restoreState)
{
    bool needCleanExitState = restoreState;
#if defined(RECYCLER_DUMP_OBJECT_GRAPH)
    needCleanExitState = needCleanExitState || GetRecyclerFlagsTable().DumpObjectGraphOnExit;
#endif
#ifdef LEAK_REPORT
    needCleanExitState = needCleanExitState || GetRecyclerFlagsTable().IsEnabled(Js::LeakReportFlag);
#endif
#ifdef CHECK_MEMORY_LEAK
    needCleanExitState = needCleanExitState || GetRecyclerFlagsTable().CheckMemoryLeak;
#endif

    bool aborted = AbortConcurrent(needCleanExitState);
    collectionState = CollectionStateExit;
    if (aborted && this->concurrentThread != NULL)
    {
        // In case the thread already died, wait for that too
        HANDLE handle[2] = { concurrentWorkDoneEvent, concurrentThread };

        SetEvent(concurrentWorkReadyEvent);

        SetThreadPriority(this->concurrentThread, THREAD_PRIORITY_NORMAL);
        // In case the thread already died, wait for that too
        DWORD fRet = WaitForMultipleObjectsEx(2, handle, FALSE, INFINITE, FALSE);
        AssertMsg(fRet != WAIT_FAILED, "Check handles passed to WaitForMultipleObjectsEx.");
    }

    // Shutdown parallel threads and return the handle for them so the caller can
    // close it.
    parallelThread1.Shutdown();
    parallelThread2.Shutdown();

#ifdef IDLE_DECOMMIT_ENABLED
    if (concurrentIdleDecommitEvent != nullptr)
    {
        CloseHandle(concurrentIdleDecommitEvent);
        concurrentIdleDecommitEvent = nullptr;
    }
#endif

    CloseHandle(concurrentWorkDoneEvent);
    concurrentWorkDoneEvent = nullptr;

    if (concurrentWorkReadyEvent != NULL)
    {
        CloseHandle(concurrentWorkReadyEvent);
        concurrentWorkReadyEvent = nullptr;
    }

    if (needCleanExitState)
    {
        // We may do another marking pass to look for memory leaks;
        // Since we have shut down the concurrent thread, don't do a parallel mark.
        this->enableConcurrentMark = false;
        this->enableParallelMark = false;
        this->enableConcurrentSweep = false;
    }

    this->threadService = nullptr;
    this->concurrentThread = nullptr;
}

bool
Recycler::EnableConcurrent(JsUtil::ThreadService *threadService, bool startAllThreads)
{
    if (this->disableConcurrent)
    {
        return false;
    }

    if (!this->InitializeConcurrent(threadService))
    {
        return false;
    }

#if ENABLE_DEBUG_CONFIG_OPTIONS
    this->enableConcurrentMark = !CUSTOM_PHASE_OFF1(GetRecyclerFlagsTable(), Js::ConcurrentMarkPhase);
    this->enableParallelMark = !CUSTOM_PHASE_OFF1(GetRecyclerFlagsTable(), Js::ParallelMarkPhase);
    this->enableConcurrentSweep = !CUSTOM_PHASE_OFF1(GetRecyclerFlagsTable(), Js::ConcurrentSweepPhase);
#else
    this->enableConcurrentMark = true;
    this->enableParallelMark = true;
    this->enableConcurrentSweep = true;
#endif

    if (this->enableParallelMark && this->maxParallelism == 1)
    {
        // Disable parallel mark if only 1 CPU
        this->enableParallelMark = false;
    }

    if (threadService->HasCallback())
    {
        this->threadService = threadService;
        return true;
    }
    else
    {
        bool startConcurrentThread = true;
        bool startedParallelThread1 = false;
        bool startedParallelThread2 = false;

        if (startAllThreads)
        {
            if (this->enableParallelMark && this->maxParallelism > 2)
            {
                if (!parallelThread1.EnableConcurrent(true))
                {
                    startConcurrentThread = false;
                }
                else
                {
                    startedParallelThread1 = true;
                    if (this->maxParallelism > 3)
                    {
                        if (!parallelThread2.EnableConcurrent(true))
                        {
                            startConcurrentThread = false;
                        }
                        else
                        {
                            startedParallelThread2 = true;
                        }
                    }
                }
            }
        }

        if (startConcurrentThread)
        {
            HANDLE concurrentThread = (HANDLE)_beginthreadex(NULL, Recycler::ConcurrentThreadStackSize, &Recycler::StaticThreadProc, this, STACK_SIZE_PARAM_IS_A_RESERVATION, NULL);
            if (concurrentThread != nullptr)
            {
                // Wait for recycler thread to initialize
                HANDLE handle[2] = { this->concurrentWorkDoneEvent, concurrentThread };
                DWORD ret = WaitForMultipleObjectsEx(2, handle, FALSE, INFINITE, FALSE);
                if (ret == WAIT_OBJECT_0)
                {
                    this->threadService = threadService;
                    this->concurrentThread = concurrentThread;
                    return true;
                }

                CloseHandle(concurrentThread);
            }
        }

        if (startedParallelThread1)
        {
            parallelThread1.Shutdown();
            if (startedParallelThread2)
            {
                parallelThread2.Shutdown();
            }
        }
    }

    // We failed to start a concurrent thread so we set these back to false and clean up
    this->enableConcurrentMark = false;
    this->enableParallelMark = false;
    this->enableConcurrentSweep = false;

    if (concurrentWorkReadyEvent)
    {
        CloseHandle(concurrentWorkReadyEvent);
        concurrentWorkReadyEvent = nullptr;
    }
    if (concurrentWorkDoneEvent)
    {
        CloseHandle(concurrentWorkDoneEvent);
        concurrentWorkDoneEvent = nullptr;
    }
#ifdef IDLE_DECOMMIT_ENABLED
    if (concurrentIdleDecommitEvent)
    {
        CloseHandle(concurrentIdleDecommitEvent);
        concurrentIdleDecommitEvent = nullptr;
    }
#endif

    return false;
}

void
Recycler::ShutdownThread()
{
    if (this->IsConcurrentEnabled())
    {
        Assert(concurrentThread != NULL || threadService->HasCallback());

        FinalizeConcurrent(false);
        if (concurrentThread)
        {
            CloseHandle(concurrentThread);
        }
    }
}

void
Recycler::DisableConcurrent()
{
    if (this->IsConcurrentEnabled())
    {
        Assert(concurrentThread != NULL || threadService->HasCallback());

        FinalizeConcurrent(true);
        if (concurrentThread)
        {
            CloseHandle(concurrentThread);
        }
        this->collectionState = CollectionStateNotCollecting;
    }
}

bool
Recycler::StartConcurrent(CollectionState const state)
{
    // Reset the tick count to detect if the concurrent thread is taking too long
    tickCountStartConcurrent = GetTickCount();

    CollectionState oldState = this->collectionState;
    this->collectionState = state;

    if (threadService->HasCallback())
    {
        Assert(concurrentThread == NULL);
        Assert(concurrentWorkReadyEvent == NULL);

        if (!threadService->Invoke(Recycler::StaticBackgroundWorkCallback, this))
        {
            this->collectionState = oldState;
            return false;
        }

        return true;
    }
    else
    {
        Assert(concurrentThread != NULL);
        Assert(concurrentWorkReadyEvent != NULL);

        SetEvent(concurrentWorkReadyEvent);
        return true;
    }
}

BOOL
Recycler::StartBackgroundMarkCollect()
{
#ifdef RECYCLER_TRACE
    PrintCollectTrace(Js::ConcurrentMarkPhase);
#endif
    this->CollectionBegin<Js::ConcurrentCollectPhase>();

    // Asynchronous concurrent mark
    BOOL success = StartAsynchronousBackgroundMark();

    this->CollectionEnd<Js::ConcurrentCollectPhase>();
    return success;
}

BOOL
Recycler::StartBackgroundMark(bool foregroundResetMark, bool foregroundFindRoots)
{
    Assert(!this->CollectionInProgress());

    CollectionState backgroundState = CollectionStateConcurrentResetMarks;

    bool doBackgroundFindRoots = true;
    if (foregroundResetMark || foregroundFindRoots)
    {
        RECYCLER_PROFILE_EXEC_BEGIN(this, Js::ResetWriteWatchPhase);
        bool hasWriteWatch = (recyclerPageAllocator.ResetWriteWatch() && recyclerLargeBlockPageAllocator.ResetWriteWatch());
        RECYCLER_PROFILE_EXEC_END(this, Js::ResetWriteWatchPhase);

        if (!hasWriteWatch)
        {
            // Disable concurrent mark
            this->enableConcurrentMark = false;
            return false;
        }

        // In-thread synchronized GC on the concurrent thread
        ResetMarks(this->enableScanImplicitRoots ? ResetMarkFlags_SynchronizedImplicitRoots : ResetMarkFlags_Synchronized);

        if (foregroundFindRoots)
        {
            this->collectionState = CollectionStateFindRoots;
            FindRoots();
            ScanStack();
            Assert(collectionState == CollectionStateFindRoots);
            backgroundState = CollectionStateConcurrentMark;
            doBackgroundFindRoots = false;
        }
        else
        {
            // Do find roots in the background
            backgroundState = CollectionStateConcurrentFindRoots;
        }
    }

    if (doBackgroundFindRoots)
    {
        this->PrepareBackgroundFindRoots();
    }

    if (!StartConcurrent(backgroundState))
    {
        if (doBackgroundFindRoots)
        {
            this->RevertPrepareBackgroundFindRoots();
        }
        this->collectionState = CollectionStateNotCollecting;
        return false;
    }

    return true;
}


BOOL
Recycler::StartAsynchronousBackgroundMark()
{
    // Debug flags to turn off background reset mark or background find roots, default to doing every concurrently
    return StartBackgroundMark(CUSTOM_PHASE_OFF1(GetRecyclerFlagsTable(), Js::BackgroundResetMarksPhase), CUSTOM_PHASE_OFF1(GetRecyclerFlagsTable(), Js::BackgroundFindRootsPhase));
}

BOOL
Recycler::StartSynchronousBackgroundMark()
{
    return StartBackgroundMark(true, true);
}

BOOL
Recycler::StartConcurrentSweepCollect()
{
    Assert(collectionState == CollectionStateNotCollecting);

#ifdef RECYCLER_TRACE
    PrintCollectTrace(Js::ConcurrentSweepPhase);
#endif
    this->CollectionBegin<Js::ConcurrentCollectPhase>();
    this->Mark();

    // We don't have rescan data if we disabled concurrent mark, assume the worst
    // (which means it is harder to get into partial collect mode)
    bool needConcurrentSweep = this->Sweep(RecyclerSweep::MaxPartialCollectRescanRootBytes, true, true);
    this->CollectionEnd<Js::ConcurrentCollectPhase>();
    FinishCollection(needConcurrentSweep);
    return true;
}

size_t
Recycler::BackgroundRepeatMark()
{
    RECYCLER_PROFILE_EXEC_BACKGROUND_BEGIN(this, Js::BackgroundRepeatMarkPhase);
    Assert(this->backgroundRescanCount <= RecyclerHeuristic::MaxBackgroundRepeatMarkCount - 1);

    size_t rescannedPageCount = this->BackgroundRescan(RescanFlags_ResetWriteWatch);

    if (this->NeedOOMRescan() || this->isAborting)
    {
        // OOM'ed. Let's not continue
        RECYCLER_PROFILE_EXEC_BACKGROUND_END(this, Js::BackgroundRepeatMarkPhase);
        return Recycler::InvalidScanRootBytes;
    }

    // Rescan the stack
    this->BackgroundScanStack();

    // Process mark stack
    this->DoBackgroundParallelMark();

    if (this->NeedOOMRescan())
    {
        RECYCLER_PROFILE_EXEC_BACKGROUND_END(this, Js::BackgroundRepeatMarkPhase);
        return Recycler::InvalidScanRootBytes;
    }

#ifdef RECYCLER_STATS
    Assert(this->backgroundRescanCount >= 1 && this->backgroundRescanCount <= RecyclerHeuristic::MaxBackgroundRepeatMarkCount);
    this->collectionStats.backgroundMarkData[this->backgroundRescanCount - 1] = this->collectionStats.markData;
#endif

    RECYCLER_PROFILE_EXEC_BACKGROUND_END(this, Js::BackgroundRepeatMarkPhase);

    return rescannedPageCount;
}

char* Recycler::GetScriptThreadStackTop()
{
    // We should have already checked if the recycler is thread bound or not
    Assert(mainThreadHandle != NULL);

    return (char*) savedThreadContext.GetStackTop();
}

size_t
Recycler::BackgroundScanStack()
{
    if (this->skipStack)
    {
#ifdef RECYCLER_TRACE
        CUSTOM_PHASE_PRINT_VERBOSE_TRACE1(GetRecyclerFlagsTable(), Js::ScanStackPhase, L"[%04X] Skipping the stack scan\n", ::GetCurrentThreadId());
#endif
        Output::Print(Js::ScanStackPhase, L"[%04X] Skipping the stack scan\n", ::GetCurrentThreadId());

        return 0;
    }

    if (!this->isInScript || mainThreadHandle == nullptr)
    {
        // No point in scanning the main thread's stack if we are not in script
        // We also can't scan the main thread's stack if we are not thread bounded, and didn't create the main thread's handle
        return 0;
    }

    char* stackTop = this->GetScriptThreadStackTop();

    if (stackTop != nullptr)
    {
        size_t size = (char *)stackBase - stackTop;
        ScanMemoryInline((void **)stackTop, size);
        return size;
    }

    return 0;
}

void
Recycler::BackgroundMark()
{
    Assert(this->DoQueueTrackedObject());
    this->backgroundRescanCount = 0;

    this->DoBackgroundParallelMark();

    if (this->NeedOOMRescan() || this->isAborting)
    {
        return;
    }

#ifdef RECYCLER_STATS
    this->collectionStats.backgroundMarkData[0] = this->collectionStats.markData;
#endif

    if (PHASE_OFF1(Js::BackgroundRepeatMarkPhase))
    {
        return;
    }

    // We always do one repeat mark pass.
    size_t rescannedPageCount = this->BackgroundRepeatMark();

    if (this->NeedOOMRescan() || this->isAborting)
    {
        // OOM'ed. Let's not continue
        return;
    }

    Assert(rescannedPageCount != Recycler::InvalidScanRootBytes);

    // If we rescanned enough pages in the previous repeat mark pass, then do one more
    // to try to reduce the amount of work we need to do in-thread
    if (rescannedPageCount >= RecyclerHeuristic::BackgroundSecondRepeatMarkThreshold)
    {
        this->BackgroundRepeatMark();

        if (this->NeedOOMRescan() || this->isAborting)
        {
            // OOM'ed. Let's not continue
            return;
        }
    }
}

void
Recycler::BackgroundResetMarks()
{
    RECYCLER_PROFILE_EXEC_BACKGROUND_BEGIN(this, Js::BackgroundResetMarksPhase);
    GCETW(GC_BACKGROUNDRESETMARKS_START, (this));
    Assert(IsMarkStackEmpty());
    this->scanPinnedObjectMap = true;
    this->hasScannedInitialImplicitRoots = false;

    heapBlockMap.ResetMarks();

    autoHeap.ResetMarks(this->enableScanImplicitRoots ? ResetMarkFlags_InBackgroundThreadImplicitRoots : ResetMarkFlags_InBackgroundThread);

    GCETW(GC_BACKGROUNDRESETMARKS_STOP, (this));
    RECYCLER_PROFILE_EXEC_BACKGROUND_END(this, Js::BackgroundResetMarksPhase);
}

void
Recycler::PrepareBackgroundFindRoots()
{
    Assert(!this->hasPendingConcurrentFindRoot);
    this->hasPendingConcurrentFindRoot = true;

    // Save the thread context here. The background thread
    // will use this saved context for the marking instead of
    // trying to get the live thread context of the thread
    SAVE_THREAD_CONTEXT();

    // Temporarily disable resize so the background can scan without
    // the memory being freed from under it
    pinnedObjectMap.DisableResize();

    // Update the cached info for big blocks in the guest arena

    DListBase<GuestArenaAllocator>::EditingIterator guestArenaIter(&guestArenaList);
    while (guestArenaIter.Next())
    {
        GuestArenaAllocator& allocator = guestArenaIter.Data();
        allocator.SetLockBlockList(true);

        if (allocator.pendingDelete)
        {
            Assert(this->hasPendingDeleteGuestArena);
            allocator.SetLockBlockList(false);
            guestArenaIter.RemoveCurrent(&HeapAllocator::Instance);
        }
        else if (this->backgroundFinishMarkCount == 0)
        {
            // Update the cached info for big block
            allocator.GetBigBlocks(false);
        }
    }
    this->hasPendingDeleteGuestArena = false;
}

void
Recycler::RevertPrepareBackgroundFindRoots()
{
    Assert(this->hasPendingConcurrentFindRoot);
    this->hasPendingConcurrentFindRoot = false;
    pinnedObjectMap.EnableResize();
}

size_t
Recycler::BackgroundFindRoots()
{
#ifdef RECYCLER_STATS
    size_t lastMarkCount = this->collectionStats.markData.markCount;
#endif

    size_t scanRootBytes = 0;
    Assert(this->IsConcurrentFindRootState());
    Assert(this->hasPendingConcurrentFindRoot);
    Assert(this->inPartialCollectMode || this->DoQueueTrackedObject());

    // Only mark pinned object and guest arenas, which is where most of the roots are.
    // When we go back to the main thread to rescan, we will scan the rest of the root.

    // NOTE: purposefully not marking the transientPinnedObject there. as it is transient :)

    // background mark the pinned object. Since we are in concurrent find root state
    // the main thread won't delete any entries from the map, so concurrent read
    // to the map safe.

    GCETW(GC_BACKGROUNDSCANROOTS_START, (this));
    RECYCLER_PROFILE_EXEC_BACKGROUND_BEGIN(this, Js::BackgroundFindRootsPhase);

    scanRootBytes += this->ScanPinnedObjects</*background = */true>();

    RECYCLER_PROFILE_EXEC_BACKGROUND_BEGIN(this, Js::FindRootArenaPhase);
    // background mark the guest arenas. Since we are in concurrent find root state
    // the main thread won't delete any arena, so concurrent reads to them are ok.
    DListBase<GuestArenaAllocator>::EditingIterator guestArenaIter(&guestArenaList);
    while (guestArenaIter.Next())
    {
        GuestArenaAllocator& allocator = guestArenaIter.Data();
        if (allocator.pendingDelete)
        {
            // Skip guest arena that are already marked for delete
            Assert(this->hasPendingDeleteGuestArena);
            continue;
        }
        scanRootBytes += ScanArena(&allocator, true);
    }
    RECYCLER_PROFILE_EXEC_BACKGROUND_END(this, Js::FindRootArenaPhase);

    this->ScanImplicitRoots();

    RECYCLER_PROFILE_EXEC_BACKGROUND_END(this, Js::BackgroundFindRootsPhase);
    this->hasPendingConcurrentFindRoot = false;
    this->collectionState = CollectionStateConcurrentMark;

    GCETW(GC_BACKGROUNDSCANROOTS_STOP, (this));
    RECYCLER_STATS_ADD(this, rootCount, this->collectionStats.markData.markCount - lastMarkCount);

    return scanRootBytes;
}

size_t
Recycler::BackgroundFinishMark()
{
    Assert(this->inPartialCollectMode || this->DoQueueTrackedObject());
    Assert(collectionState == CollectionStateConcurrentFinishMark);
    size_t rescannedRootBytes = FinishMarkRescan(true) * AutoSystemInfo::PageSize;
    this->collectionState = CollectionStateConcurrentFindRoots;
    rescannedRootBytes += this->BackgroundFindRoots();
    this->collectionState = CollectionStateConcurrentFinishMark;
    RECYCLER_PROFILE_EXEC_BACKGROUND_BEGIN(this, Js::MarkPhase);
    ProcessMark(true);
    RECYCLER_PROFILE_EXEC_BACKGROUND_END(this, Js::MarkPhase);
    return rescannedRootBytes;
}

void
Recycler::SweepPendingObjects(RecyclerSweep& recyclerSweep)
{
    autoHeap.SweepPendingObjects(recyclerSweep);
}

void
Recycler::ConcurrentTransferSweptObjects(RecyclerSweep& recyclerSweep)
{
    Assert(!recyclerSweep.IsBackground());
    Assert((this->collectionState & Collection_TransferSwept) == Collection_TransferSwept);
    if (this->hasBackgroundFinishPartial)
    {
        this->hasBackgroundFinishPartial = false;
        this->ClearPartialCollect();
    }
    autoHeap.ConcurrentTransferSweptObjects(recyclerSweep);
}

#ifdef PARTIAL_GC_ENABLED
void
Recycler::ConcurrentPartialTransferSweptObjects(RecyclerSweep& recyclerSweep)
{
    Assert(!recyclerSweep.IsBackground());
    Assert(!this->hasBackgroundFinishPartial);
    autoHeap.ConcurrentPartialTransferSweptObjects(recyclerSweep);
}
#endif

BOOL
Recycler::FinishConcurrentCollectWrapped(CollectionFlags flags)
{
    this->allowDispose = (flags & CollectOverride_AllowDispose) == CollectOverride_AllowDispose;
#ifdef CONCURRENT_GC_ENABLED
    this->skipStack = ((flags & CollectOverride_SkipStack) != 0);
    DebugOnly(this->isConcurrentGCOnIdle = (flags == CollectOnScriptIdle));
#endif
    BOOL collected = collectionWrapper->ExecuteRecyclerCollectionFunction(this, &Recycler::FinishConcurrentCollect, flags);
    return collected;
}

BOOL
Recycler::WaitForConcurrentThread(DWORD waitTime)
{
    Assert(this->IsConcurrentState() || this->collectionState == CollectionStateParallelMark);

    RECYCLER_PROFILE_EXEC_BEGIN(this, Js::ConcurrentWaitPhase);

    if (concurrentThread != NULL)
    {
        // Set the priority back to normal before we wait to ensure it doesn't starve
        SetThreadPriority(this->concurrentThread, THREAD_PRIORITY_NORMAL);
    }

    DWORD ret = WaitForSingleObject(concurrentWorkDoneEvent, waitTime);

    if (concurrentThread != NULL)
    {
        if (ret == WAIT_TIMEOUT)
        {
            // Keep the priority boost.
            priorityBoost = true;
        }
        else
        {
            Assert(ret == WAIT_OBJECT_0);

            // Back to below normal
            SetThreadPriority(this->concurrentThread, THREAD_PRIORITY_BELOW_NORMAL);
            priorityBoost = false;
        }
    }

    RECYCLER_PROFILE_EXEC_END(this, Js::ConcurrentWaitPhase);

    return (ret == WAIT_OBJECT_0);
}

void
Recycler::FlushBackgroundPages()
{
    recyclerPageAllocator.SuspendIdleDecommit();
    recyclerPageAllocator.FlushBackgroundPages();
    recyclerPageAllocator.ResumeIdleDecommit();

    recyclerLargeBlockPageAllocator.SuspendIdleDecommit();
    recyclerLargeBlockPageAllocator.FlushBackgroundPages();
    recyclerLargeBlockPageAllocator.ResumeIdleDecommit();

    this->threadPageAllocator->SuspendIdleDecommit();
    this->threadPageAllocator->FlushBackgroundPages();
    this->threadPageAllocator->ResumeIdleDecommit();

#ifdef RECYCLER_WRITE_BARRIER_ALLOC_SEPARATE_PAGE
    recyclerWithBarrierPageAllocator.SuspendIdleDecommit();
    recyclerWithBarrierPageAllocator.FlushBackgroundPages();
    recyclerWithBarrierPageAllocator.ResumeIdleDecommit();
#endif
}

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
AutoProtectPages::AutoProtectPages(Recycler* recycler, bool protectEnabled) :
    isReadOnly(false),
    recycler(recycler)
{
    if (protectEnabled)
    {
        recycler->heapBlockMap.MakeAllPagesReadOnly(recycler);
        isReadOnly = true;
    }
}

AutoProtectPages::~AutoProtectPages()
{
    Unprotect();
}

void AutoProtectPages::Unprotect()
{
    if (isReadOnly)
    {
        recycler->heapBlockMap.MakeAllPagesReadWrite(recycler);
        isReadOnly = false;
    }
}
#endif

BOOL
Recycler::FinishConcurrentCollect(CollectionFlags flags)
{
    if (!this->IsConcurrentState())
    {
        Assert(false);
        return false;
    }

#ifdef PROFILE_EXEC
    Js::Phase concurrentPhase = Js::ConcurrentCollectPhase;
#endif
    RECYCLER_PROFILE_EXEC_BEGIN2(this, Js::RecyclerPhase,
        (concurrentPhase = ((this->inPartialCollectMode && this->IsConcurrentMarkState())?
            Js::ConcurrentPartialCollectPhase : Js::ConcurrentCollectPhase)));

    // Don't do concurrent sweep if we have priority boosted.
    const BOOL forceInThread = flags & CollectOverride_ForceInThread;
    bool concurrent = (flags & CollectMode_Concurrent) != 0;
    concurrent = concurrent && (!priorityBoost || this->backgroundRescanCount != 1);
#ifdef RECYCLER_TRACE
    collectionParam.priorityBoostConcurentSweepOverride = priorityBoost;
#endif

    const DWORD waitTime = forceInThread? INFINITE : RecyclerHeuristic::FinishConcurrentCollectWaitTime(this->GetRecyclerFlagsTable());
    GCETW(GC_FINISHCONCURRENTWAIT_START, (this, waitTime));
    const BOOL waited = WaitForConcurrentThread(waitTime);
    GCETW(GC_FINISHCONCURRENTWAIT_STOP, (this, !waited));
    if (!waited)
    {
        RECYCLER_PROFILE_EXEC_END2(this, concurrentPhase, Js::RecyclerPhase);
        return false;
    }

    bool needConcurrentSweep = false;
    if (collectionState == CollectionStateRescanWait)
    {
        GCETW(GC_START, (this, ETWEvent_ConcurrentRescan));
#ifdef ENABLE_BASIC_TELEMETRY
        if(GetCurrentThreadContextId()==mainThreadId && IsMemProtectMode()==false)
        {
            gcTel.LogGCPauseStartTime();
        }
#endif

#ifdef RECYCLER_TRACE
        PrintCollectTrace(this->inPartialCollectMode ? Js::ConcurrentPartialCollectPhase : Js::ConcurrentMarkPhase, true);
#endif
        collectionState = CollectionStateRescanFindRoots;

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        // TODO: Change this behavior
        // ProtectPagesOnRescan is not supported in PageHeap mode because the page protection is changed
        // outside the PageAllocator in PageHeap mode and so pages are not in the state that the
        // PageAllocator expects when it goes to change the page protection
        // One viable fix is to move the guard page protection logic outside of the heap blocks
        // and into the page allocator
        AssertMsg(!(IsPageHeapEnabled() && GetRecyclerFlagsTable().RecyclerProtectPagesOnRescan), "ProtectPagesOnRescan not supported in page heap mode");
        AutoProtectPages protectPages(this, GetRecyclerFlagsTable().RecyclerProtectPagesOnRescan);
#endif

        const bool backgroundFinishMark = !forceInThread && concurrent && ((flags & CollectOverride_BackgroundFinishMark) != 0);
        const DWORD finishMarkWaitTime = RecyclerHeuristic::BackgroundFinishMarkWaitTime(backgroundFinishMark, GetRecyclerFlagsTable());
        size_t rescanRootBytes = FinishMark(finishMarkWaitTime);

        if (rescanRootBytes == Recycler::InvalidScanRootBytes)
        {
            Assert(this->IsMarkState());
            RECYCLER_PROFILE_EXEC_END2(this, concurrentPhase, Js::RecyclerPhase);
            GCETW(GC_STOP, (this, ETWEvent_ConcurrentRescan));
#ifdef ENABLE_BASIC_TELEMETRY
            if (GetCurrentThreadContextId() == mainThreadId && IsMemProtectMode() == false)
            {
               gcTel.LogGCPauseEndTime();
            }
#endif
            // we timeout trying to mark.
            return false;
        }

#ifdef RECYCLER_STATS
        collectionStats.continueCollectAllocBytes = autoHeap.uncollectedAllocBytes;
#endif

#ifdef RECYCLER_VERIFY_MARK
        if (GetRecyclerFlagsTable().RecyclerVerifyMark)
        {
            this->VerifyMark();
        }
#endif

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        protectPages.Unprotect();
#endif

        needConcurrentSweep = this->Sweep(rescanRootBytes, concurrent, true);
        GCETW(GC_STOP, (this, ETWEvent_ConcurrentRescan));
    }
    else
    {
        GCETW(GC_START, (this, ETWEvent_ConcurrentTransferSwept));
#ifdef ENABLE_BASIC_TELEMETRY
        if(GetCurrentThreadContextId()==mainThreadId && IsMemProtectMode()==false)
        {
            gcTel.LogGCPauseStartTime();
        }
#endif
        GCETW(GC_FLUSHZEROPAGE_START, (this));

        Assert(collectionState == CollectionStateTransferSweptWait);
#ifdef RECYCLER_TRACE
        PrintCollectTrace(Js::ConcurrentSweepPhase, true);
#endif
        collectionState = CollectionStateTransferSwept;

        // We should have zeroed all the pages in the background thread
        Assert(!recyclerPageAllocator.HasZeroQueuedPages());
        Assert(!recyclerLargeBlockPageAllocator.HasZeroQueuedPages());
        this->FlushBackgroundPages();

        GCETW(GC_FLUSHZEROPAGE_STOP, (this));
        GCETW(GC_TRANSFERSWEPTOBJECTS_START, (this));

        Assert(this->recyclerSweep != nullptr);
        Assert(!this->recyclerSweep->IsBackground());
#ifdef PARTIAL_GC_ENABLED
        if (this->inPartialCollectMode)
        {
            ConcurrentPartialTransferSweptObjects(*this->recyclerSweep);
        }
        else
#endif
        {
            ConcurrentTransferSweptObjects(*this->recyclerSweep);
        }
        recyclerSweep->EndSweep();

        GCETW(GC_TRANSFERSWEPTOBJECTS_STOP, (this));

        GCETW(GC_STOP, (this, ETWEvent_ConcurrentTransferSwept));
#ifdef ENABLE_BASIC_TELEMETRY
        if (GetCurrentThreadContextId() == mainThreadId && IsMemProtectMode() == false)
        {
            gcTel.LogGCPauseEndTime();
        }
#endif
    }

    RECYCLER_PROFILE_EXEC_END2(this, concurrentPhase, Js::RecyclerPhase);

    FinishCollection(needConcurrentSweep);

    if (!this->CollectionInProgress())
    {
        if (NeedExhaustiveRepeatCollect())
        {
            DoCollect((CollectionFlags)(flags & ~CollectMode_Partial));
        }
        else
        {
            EndCollection();
        }
    }

    return true;
}

int
Recycler::ExceptFilter(LPEXCEPTION_POINTERS pEP)
{
#if DBG
    // Assert exception code
    if (pEP->ExceptionRecord->ExceptionCode == STATUS_ASSERTION_FAILURE)
    {
        return EXCEPTION_CONTINUE_SEARCH;
    }
#endif

#ifdef GENERATE_DUMP
    if (Js::Configuration::Global.flags.IsEnabled(Js::DumpOnCrashFlag))
    {
        Js::Throw::GenerateDump(pEP, Js::Configuration::Global.flags.DumpOnCrash);
    }
#endif

#if DBG && _M_IX86
    int callerEBP = *((int*)pEP->ContextRecord->Ebp);

    Output::Print(L"Recycler Concurrent Thread: Uncaught exception: EIP: 0x%X  ExceptionCode: 0x%X  EBP: 0x%X  ReturnAddress: 0x%X  ReturnAddress2: 0x%X\n",
        pEP->ExceptionRecord->ExceptionAddress, pEP->ExceptionRecord->ExceptionCode, pEP->ContextRecord->Eip,
        pEP->ContextRecord->Ebp, *((int*)pEP->ContextRecord->Ebp + 1), *((int*) callerEBP + 1));
#endif

    Output::Flush();
    return EXCEPTION_CONTINUE_SEARCH;

}

unsigned int
Recycler::StaticThreadProc(LPVOID lpParameter)
{
    DWORD ret = (DWORD)-1;
    __try
    {
        Recycler * recycler = (Recycler *)lpParameter;

#if DBG
        recycler->concurrentThreadExited = false;
#endif
        ret = recycler->ThreadProc();
    }
    __except(Recycler::ExceptFilter(GetExceptionInformation()))
    {
        Assert(false);
    }

    return ret;
}

void
Recycler::StaticBackgroundWorkCallback(void * callbackData)
{
    Recycler * recycler = (Recycler *) callbackData;
    recycler->DoBackgroundWork(true);
}

#ifdef ENABLE_JS_ETW
static ETWEventGCActivationKind
BackgroundMarkETWEventGCActivationKind(CollectionState collectionState)
{
    return collectionState == CollectionStateConcurrentFinishMark?
        ETWEvent_ConcurrentFinishMark : ETWEvent_ConcurrentMark;
}
#endif

void
Recycler::DoBackgroundWork(bool forceForeground)
{
    if (this->collectionState == CollectionStateConcurrentWrapperCallback)
    {
        this->collectionWrapper->ConcurrentCallback();
    }
    else if (this->collectionState == CollectionStateParallelMark)
    {
        this->ProcessParallelMark(false, &this->markContext);
    }
    else if (this->IsConcurrentMarkState())
    {
        RECYCLER_PROFILE_EXEC_BACKGROUND_BEGIN(this, this->collectionState == CollectionStateConcurrentFinishMark?
            Js::BackgroundFinishMarkPhase : Js::ConcurrentMarkPhase);
        GCETW(GC_START, (this, BackgroundMarkETWEventGCActivationKind(this->collectionState)));
#ifdef ENABLE_BASIC_TELEMETRY
        if (GetCurrentThreadContextId() == mainThreadId && IsMemProtectMode() == false)
        {
            gcTel.LogGCPauseStartTime();
        }
#endif
        DebugOnly(this->markContext.GetPageAllocator()->SetConcurrentThreadId(::GetCurrentThreadId()));
        Assert(this->enableConcurrentMark);
        if (this->collectionState != CollectionStateConcurrentFinishMark)
        {
            this->StartQueueTrackedObject();
        }
        switch (this->collectionState)
        {
        case CollectionStateConcurrentResetMarks:
            this->BackgroundResetMarks();
            this->BackgroundResetWriteWatchAll();
            this->collectionState = CollectionStateConcurrentFindRoots;
            // fall-through
        case CollectionStateConcurrentFindRoots:
            this->BackgroundFindRoots();
            this->BackgroundScanStack();
            this->collectionState = CollectionStateConcurrentMark;
            // fall-through
        case CollectionStateConcurrentMark:
            this->BackgroundMark();
            Assert(this->collectionState == CollectionStateConcurrentMark);
            RECORD_TIMESTAMP(concurrentMarkFinishTime);
            break;
        case CollectionStateConcurrentFinishMark:
            this->backgroundRescanRootBytes = this->BackgroundFinishMark();
            Assert(!HasPendingMarkObjects());
            break;
        default:
            Assert(false);
            break;
        };
        GCETW(GC_STOP, (this, BackgroundMarkETWEventGCActivationKind(this->collectionState)));
#ifdef ENABLE_BASIC_TELEMETRY
        if (GetCurrentThreadContextId() == mainThreadId && IsMemProtectMode() == false)
        {
            gcTel.LogGCPauseEndTime();
        }
#endif
        RECYCLER_PROFILE_EXEC_BACKGROUND_END(this, this->collectionState == CollectionStateConcurrentFinishMark?
            Js::BackgroundFinishMarkPhase : Js::ConcurrentMarkPhase);

        this->collectionState = CollectionStateRescanWait;
        DebugOnly(this->markContext.GetPageAllocator()->ClearConcurrentThreadId());
    }
    else
    {
        RECYCLER_PROFILE_EXEC_BACKGROUND_BEGIN(this, Js::ConcurrentSweepPhase);
        GCETW(GC_START, (this, ETWEvent_ConcurrentSweep));
#ifdef ENABLE_BASIC_TELEMETRY
        if (GetCurrentThreadContextId() == mainThreadId && IsMemProtectMode() == false)
        {
            gcTel.LogGCPauseStartTime();
        }
#endif
        GCETW(GC_BACKGROUNDZEROPAGE_START, (this));

        Assert(this->enableConcurrentSweep);
        Assert(this->collectionState == CollectionStateConcurrentSweep);

        // Zero the queued pages first so they are available to be allocated
        recyclerPageAllocator.BackgroundZeroQueuedPages();
        recyclerLargeBlockPageAllocator.BackgroundZeroQueuedPages();
#ifdef RECYCLER_WRITE_BARRIER_ALLOC_SEPARATE_PAGE
        recyclerWithBarrierPageAllocator.BackgroundZeroQueuedPages();
#endif

        GCETW(GC_BACKGROUNDZEROPAGE_STOP, (this));
        GCETW(GC_BACKGROUNDSWEEP_START, (this));

        Assert(this->recyclerSweep != nullptr);
        this->recyclerSweep->BackgroundSweep();
        uint sweptBytes = 0;
#ifdef RECYCLER_STATS
        sweptBytes = (uint)collectionStats.objectSweptBytes;
#endif

        GCETW(GC_BACKGROUNDSWEEP_STOP, (this, sweptBytes));

        // Drain the zero queue again as we might have free more during sweep
        // in the background
        GCETW(GC_BACKGROUNDZEROPAGE_START, (this));
        recyclerPageAllocator.BackgroundZeroQueuedPages();
#ifdef RECYCLER_WRITE_BARRIER_ALLOC_SEPARATE_PAGE
        recyclerWithBarrierPageAllocator.BackgroundZeroQueuedPages();
#endif
        recyclerLargeBlockPageAllocator.BackgroundZeroQueuedPages();
        GCETW(GC_BACKGROUNDZEROPAGE_STOP, (this));
        GCETW(GC_STOP, (this, ETWEvent_ConcurrentSweep));
#ifdef ENABLE_BASIC_TELEMETRY
        if (GetCurrentThreadContextId() == mainThreadId && IsMemProtectMode() == false)
        {
            gcTel.LogGCPauseEndTime();
        }
#endif

        Assert(this->collectionState == CollectionStateConcurrentSweep);
        this->collectionState = CollectionStateTransferSweptWait;
        RECYCLER_PROFILE_EXEC_BACKGROUND_END(this, Js::ConcurrentSweepPhase);
    }

    SetEvent(this->concurrentWorkDoneEvent);

    collectionWrapper->WaitCollectionCallBack();
}

DWORD
Recycler::ThreadProc()
{
    Assert(this->IsConcurrentEnabled());

#if !defined(_UCRT)
    // We do this before we set the concurrentWorkDoneEvent because GetModuleHandleEx requires
    // getting the loader lock. We could have the following case:
    //    Thread A => Initialize Concurrent Thread (C)
    //    C signals Signal Done
    //    C yields since its lower priority
    //    Thread A starts running- and is told to shut down.
    //    Thread A grabs loader lock as part of the shutdown sequence
    //    Thread A waits for C to be done
    //    C wakes up now- and tries to grab loader lock.
    // To prevent this deadlock, we call GetModuleHandleEx first and then set the concurrentWorkDoneEvent
    HMODULE dllHandle = NULL;
    if (!GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCTSTR)&Recycler::StaticThreadProc, &dllHandle))
    {
        dllHandle = NULL;
    }
#endif

#ifdef ENABLE_JS_ETW
    // Create an ETW ActivityId for this thread, to help tools correlate ETW events we generate
    GUID activityId = { 0 };
    auto result = EventActivityIdControl(EVENT_ACTIVITY_CTRL_CREATE_SET_ID, &activityId);
    Assert(result == ERROR_SUCCESS);
#endif

    // Signal that the thread has started
    SetEvent(this->concurrentWorkDoneEvent);

    SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);

#if defined(DBG) && defined(PROFILE_EXEC)
    this->backgroundProfilerPageAllocator.SetConcurrentThreadId(::GetCurrentThreadId());
#endif
#ifdef IDLE_DECOMMIT_ENABLED
    DWORD handleCount = this->concurrentIdleDecommitEvent? 2 : 1;
    HANDLE handles[2] = { this->concurrentWorkReadyEvent, this->concurrentIdleDecommitEvent };
#endif
    do
    {
#ifdef IDLE_DECOMMIT_ENABLED
        needIdleDecommitSignal = IdleDecommitSignal_None;

        DWORD threadPageAllocatorWaitTime = threadPageAllocator->IdleDecommit();
        DWORD recyclerPageAllocatorWaitTime = recyclerPageAllocator.IdleDecommit();
        DWORD waitTime = min(threadPageAllocatorWaitTime, recyclerPageAllocatorWaitTime);
        DWORD recyclerLargeBlockPageAllocatorWaitTime = recyclerLargeBlockPageAllocator.IdleDecommit();
        waitTime = min(waitTime, recyclerLargeBlockPageAllocatorWaitTime);

#ifdef RECYCLER_WRITE_BARRIER_ALLOC_SEPARATE_PAGE
        DWORD recyclerWithBarrierPageAllocatorWaitTime = recyclerWithBarrierPageAllocator.IdleDecommit();
        waitTime = min(waitTime, recyclerWithBarrierPageAllocatorWaitTime);
#endif
        if (waitTime == INFINITE)
        {
            DWORD ret = ::InterlockedCompareExchange(&needIdleDecommitSignal, IdleDecommitSignal_NeedSignal, IdleDecommitSignal_None);
            if (ret == IdleDecommitSignal_NeedTimer)
            {
#if DBG
                if (GetRecyclerFlagsTable().Trace.IsEnabled(Js::IdleDecommitPhase))
                {
                    Output::Print(L"Recycler Thread IdleDecommit Need Timer\n");
                    Output::Flush();
                }
#endif
                continue;
            }
        }
#if DBG
        else
        {
            if (GetRecyclerFlagsTable().Trace.IsEnabled(Js::IdleDecommitPhase))
            {
                Output::Print(L"Recycler Thread IdleDecommit Wait %d\n", waitTime);
                Output::Flush();
            }
        }
#endif

        DWORD result = WaitForMultipleObjectsEx(handleCount, handles, FALSE, waitTime, FALSE);

        if (result != WAIT_OBJECT_0)
        {
            Assert((handleCount == 2 && result == WAIT_OBJECT_0 + 1) || (waitTime != INFINITE && result == WAIT_TIMEOUT));
#if DBG
            if (GetRecyclerFlagsTable().Trace.IsEnabled(Js::IdleDecommitPhase))
            {
                if (result == WAIT_TIMEOUT)
                {
                    Output::Print(L"Recycler Thread IdleDecommit Timeout: %d\n", waitTime);
                }
                else
                {
                    Output::Print(L"Recycler Thread IdleDecommit Signaled\n");
                }
                Output::Flush();
            }
#endif
            continue;
        }
#else
        DWORD result = WaitForSingleObject(this->concurrentWorkReadyEvent, INFINITE);
        Assert(result == WAIT_OBJECT_0);
#endif
        if (this->collectionState == CollectionStateExit)
        {
#if DBG
            this->concurrentThreadExited = true;
#endif
            break;
        }

        DoBackgroundWork();
    }
    while (true);
    SetEvent(this->concurrentWorkDoneEvent);

#if !defined(_UCRT)
    if (dllHandle)
    {
        FreeLibraryAndExitThread(dllHandle, 0);
    }
    else
#endif
    {
        return 0;
    }
}

#endif //defined(CONCURRENT_GC_ENABLED)

void
Recycler::FinishCollection(bool needConcurrentSweep)
{
    Assert(!!this->InConcurrentSweep() == needConcurrentSweep);
    if (!needConcurrentSweep)
    {
        FinishCollection();
    }
    else
    {
        FinishDisposeObjects();
    }
}

void
Recycler::FinishCollection()
{
    Assert(!this->hasBackgroundFinishPartial);
    Assert(!this->hasPendingDeleteGuestArena);

    // Reset the time heuristics
    ScheduleNextCollection();

    {
        AutoSwitchCollectionStates collectionState(this,
            /* entry  state */ CollectionStatePostCollectionCallback,
            /* exit   state */ CollectionStateNotCollecting);

        collectionWrapper->PostCollectionCallBack();
    }

    this->backgroundFinishMarkCount = 0;

    // Do a partial page decommit now
    if (decommitOnFinish)
    {
        ForRecyclerPageAllocator(DecommitNow(false));
        this->decommitOnFinish = false;
    }

    RECYCLER_SLOW_CHECK(autoHeap.Check());

#ifdef RECYCLER_MEMORY_VERIFY
    this->Verify(Js::RecyclerPhase);
#endif
#ifdef RECYCLER_FINALIZE_CHECK
    autoHeap.VerifyFinalize();
#endif

#ifdef ENABLE_JS_ETW
    FlushFreeRecord();
#endif

    FinishDisposeObjects();

#ifdef RECYCLER_FINALIZE_CHECK
    if (!this->IsMarkState())
    {
        autoHeap.VerifyFinalize();
    }
#endif

#ifdef RECYCLER_STATS
    if (CUSTOM_PHASE_STATS1(this->GetRecyclerFlagsTable(), Js::RecyclerPhase))
    {
        PrintCollectStats();
    }
#endif
#ifdef PROFILE_RECYCLER_ALLOC
    if (MemoryProfiler::IsTraceEnabled(true))
    {
        PrintAllocStats();
    }
#endif

#ifdef DUMP_FRAGMENTATION_STATS
    if (GetRecyclerFlagsTable().DumpFragmentationStats)
    {
        autoHeap.DumpFragmentationStats();
    }
#endif

    RECORD_TIMESTAMP(currentCollectionEndTime);
}


char *
Recycler::Realloc(void* buffer, size_t existingBytes, size_t requestedBytes, bool truncate)
{
    Assert(requestedBytes > 0);

    if (existingBytes == 0)
    {
        Assert(buffer == nullptr);
        return Alloc(requestedBytes);
    }

    Assert(buffer != nullptr);

    size_t nbytes = AllocSizeMath::Align(requestedBytes, HeapConstants::ObjectGranularity);

    // Since we successfully allocated, we shouldn't have integer overflow here
    size_t nbytesExisting = AllocSizeMath::Align(existingBytes, HeapConstants::ObjectGranularity);
    Assert(nbytesExisting >= existingBytes);

    if (nbytes == nbytesExisting)
    {
        return (char *)buffer;
    }

    char* replacementBuf = this->Alloc(requestedBytes);
    if (replacementBuf != nullptr)
    {
        // Truncate
        if (existingBytes > requestedBytes && truncate)
        {
            js_memcpy_s(replacementBuf, requestedBytes, buffer, requestedBytes);
        }
        else
        {
            js_memcpy_s(replacementBuf, requestedBytes, buffer, existingBytes);
        }
    }

    if (nbytesExisting > 0)
    {
        this->Free(buffer, nbytesExisting);
    }

    return replacementBuf;
}


bool
Recycler::ForceSweepObject()
{
#ifdef RECYCLER_TEST_SUPPORT
    if (BinaryFeatureControl::RecyclerTest())
    {
        if (checkFn != nullptr)
        {
            return true;
        }
    }
#endif

#ifdef PROFILE_RECYCLER_ALLOC
    if (trackerDictionary != nullptr)
    {
        // Need to sweep object if we are tracing recycler allocs
        return true;
    }
#endif

#ifdef RECYCLER_STATS
    if (CUSTOM_PHASE_STATS1(this->GetRecyclerFlagsTable(), Js::RecyclerPhase))
    {
        return true;
    }
#endif

#if DBG
    // Force sweeping the object so we can assert that we are not sweeping objects that are still implicit roots
    if (this->enableScanImplicitRoots)
    {
        return true;
    }
#endif
    return false;
}

bool
Recycler::ShouldIdleCollectOnExit()
{
    // Always reset partial heuristics even if we are not doing idle collecting
    // So we don't carry the heuristics to the next script activation
    this->ResetPartialHeuristicCounters();

    if (this->CollectionInProgress())
    {
#ifdef RECYCLER_TRACE
        CUSTOM_PHASE_PRINT_VERBOSE_TRACE1(GetRecyclerFlagsTable(), Js::IdleCollectPhase, L"%04X> Skipping scheduling Idle Collect. Reason: Collection in progress\n", ::GetCurrentThreadId());
#endif

        // Don't schedule a idle collect if there is a collection going on already
        // IDLE-GC-TODO: Fix ResetHeuristics in the GC so we can detect memory allocation during
        // the concurrent collect and still schedule an idle collect
        return false;
    }

    if (CUSTOM_PHASE_FORCE1(GetRecyclerFlagsTable(), Js::IdleCollectPhase))
    {
        return true;
    }

    ulong nextTime = tickCountNextCollection - tickDiffToNextCollect;
    // We will try to start a concurrent collect if we are within .9 ms to next scheduled collection, AND,
    // the size of allocation is larger than 32M. This is similar to CollectionAllocation logic, just
    // earlier in both time heuristic and size heuristic, so we can do some concurrent GC while we are
    // not in script.
    if (autoHeap.uncollectedAllocBytes >= RecyclerHeuristic::Instance.MaxUncollectedAllocBytesOnExit
        && GetTickCount() > nextTime)
    {
#ifdef RECYCLER_TRACE
        if (CUSTOM_PHASE_TRACE1(GetRecyclerFlagsTable(), Js::IdleCollectPhase))
        {
            if (autoHeap.uncollectedAllocBytes >= RecyclerHeuristic::Instance.MaxUncollectedAllocBytesOnExit)
            {
                Output::Print(L"%04X> Idle collect on exit: alloc %d\n", ::GetCurrentThreadId(), autoHeap.uncollectedAllocBytes);
            }
            else
            {
                Output::Print(L"%04X> Idle collect on exit: time %d\n", ::GetCurrentThreadId(), tickCountNextCollection - GetTickCount());
            }
            Output::Flush();
        }
#endif

        this->CollectNow<CollectNowConcurrent>();
        return false;
    }

    Assert(!this->CollectionInProgress());
    // Idle GC use the size heuristic. Only need to schedule on if we passed it.
    return (autoHeap.uncollectedAllocBytes >= RecyclerHeuristic::IdleUncollectedAllocBytesCollection);
}

bool
RecyclerParallelThread::StartConcurrent()
{
    if (this->recycler->threadService->HasCallback())
    {
        // This may be the first time.  If so, initialize by creating the doneEvent.
        if (this->concurrentWorkDoneEvent == NULL)
        {
            this->concurrentWorkDoneEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
            if (this->concurrentWorkDoneEvent == nullptr)
            {
                return false;
            }
        }

        Assert(concurrentThread == NULL);
        Assert(concurrentWorkReadyEvent == NULL);

        // Invoke thread service to process work
        if (!this->recycler->threadService->Invoke(RecyclerParallelThread::StaticBackgroundWorkCallback, this))
        {
            return false;
        }
    }
    else
    {
        // This may be the first time.  If so, initialize and create thread.
        if (this->concurrentWorkDoneEvent == NULL)
        {
            return this->EnableConcurrent(false);
        }
        else
        {
            Assert(this->concurrentThread != NULL);
            Assert(this->concurrentWorkReadyEvent != NULL);

            // signal that thread has been initialized
            SetEvent(this->concurrentWorkReadyEvent);
        }
    }
    return true;
}

bool
RecyclerParallelThread::EnableConcurrent(bool waitForThread)
{
    this->synchronizeOnStartup = waitForThread;

    Assert(this->concurrentWorkDoneEvent == NULL);
    Assert(this->concurrentWorkReadyEvent == NULL);
    Assert(this->concurrentThread == NULL);

    this->concurrentWorkDoneEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (this->concurrentWorkDoneEvent == nullptr)
    {
        return false;
    }

    this->concurrentWorkReadyEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (this->concurrentWorkReadyEvent == nullptr)
    {
        CloseHandle(this->concurrentWorkDoneEvent);
        this->concurrentWorkDoneEvent = NULL;
        return false;
    }

    this->concurrentThread = (HANDLE)_beginthreadex(NULL, Recycler::ConcurrentThreadStackSize, &RecyclerParallelThread::StaticThreadProc, this, STACK_SIZE_PARAM_IS_A_RESERVATION, NULL);

    if (this->concurrentThread != nullptr && waitForThread)
    {
        // Wait for thread to initialize
        HANDLE handle[2] = { this->concurrentWorkDoneEvent, this->concurrentThread };
        DWORD ret = WaitForMultipleObjectsEx(2, handle, FALSE, INFINITE, FALSE);
        if (ret == WAIT_OBJECT_0)
        {
            return true;
        }

        CloseHandle(concurrentThread);
        concurrentThread = nullptr;
    }

    if (this->concurrentThread == nullptr)
    {
        CloseHandle(this->concurrentWorkDoneEvent);
        this->concurrentWorkDoneEvent = NULL;
        CloseHandle(this->concurrentWorkReadyEvent);
        this->concurrentWorkReadyEvent = NULL;
        return false;
    }

    return true;
}


template <uint parallelId>
void
Recycler::ParallelWorkFunc()
{
    Assert(parallelId == 0 || parallelId == 1);

    MarkContext * markContext = (parallelId == 0 ? &this->parallelMarkContext2 : &this->parallelMarkContext3);

    switch (this->collectionState)
    {
        case CollectionStateParallelMark:
            this->ProcessParallelMark(false, markContext);
            break;

        case CollectionStateBackgroundParallelMark:
            this->ProcessParallelMark(true, markContext);
            break;

        default:
            Assert(false);
    }
}

void
RecyclerParallelThread::WaitForConcurrent()
{
    Assert(this->concurrentThread != NULL || this->recycler->threadService->HasCallback());
    Assert(this->concurrentWorkDoneEvent != NULL);

    DWORD ret = WaitForSingleObject(concurrentWorkDoneEvent, INFINITE);
    Assert(ret == WAIT_OBJECT_0);
}

void
RecyclerParallelThread::Shutdown()
{
    Assert(this->recycler->collectionState == CollectionStateExit);

    if (this->recycler->threadService->HasCallback())
    {
        if (this->concurrentWorkDoneEvent != NULL)
        {
            CloseHandle(this->concurrentWorkDoneEvent);
            this->concurrentWorkDoneEvent = NULL;
        }
    }
    else
    {
        if (this->concurrentThread != NULL)
        {
            HANDLE handles[2] = { concurrentWorkDoneEvent, concurrentThread };

            SetEvent(concurrentWorkReadyEvent);

            // During process shutdown, OS might kill this (recycler parallel i.e. concurrent) thread and it will not get chance to signal concurrentWorkDoneEvent.
            // When we are performing shutdown of main (recycler) thread here, if we wait on concurrentWorkDoneEvent, WaitForObject() will never return.
            // Hence wait for concurrentWorkDoneEvent + concurrentThread so if concurrentThread got killed, WaitForObject() will return and we will
            // proceed further.
            DWORD fRet = WaitForMultipleObjectsEx(2, handles, FALSE, INFINITE, FALSE);
            AssertMsg(fRet != WAIT_FAILED, "Check handles passed to WaitForMultipleObjectsEx.");

            CloseHandle(this->concurrentWorkDoneEvent);
            this->concurrentWorkDoneEvent = NULL;
            CloseHandle(this->concurrentWorkReadyEvent);
            this->concurrentWorkReadyEvent = NULL;
            CloseHandle(this->concurrentThread);
            this->concurrentThread = NULL;
        }
    }

    Assert(this->concurrentThread == NULL);
    Assert(this->concurrentWorkReadyEvent == NULL);
    Assert(this->concurrentWorkDoneEvent == NULL);
}

// static
unsigned int
RecyclerParallelThread::StaticThreadProc(LPVOID lpParameter)
{
    DWORD ret = (DWORD)-1;
    __try
    {
        RecyclerParallelThread * parallelThread = (RecyclerParallelThread *)lpParameter;
        Recycler * recycler = parallelThread->recycler;
        RecyclerParallelThread::WorkFunc workFunc = parallelThread->workFunc;

        Assert(recycler->IsConcurrentEnabled());

#if !defined(_UCRT)
        HMODULE dllHandle = NULL;
        if (!GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCTSTR)&RecyclerParallelThread::StaticThreadProc, &dllHandle))
        {
            dllHandle = NULL;
        }
#endif
#ifdef ENABLE_JS_ETW
        // Create an ETW ActivityId for this thread, to help tools correlate ETW events we generate
        GUID activityId = { 0 };
        auto result = EventActivityIdControl(EVENT_ACTIVITY_CTRL_CREATE_SET_ID, &activityId);
        Assert(result == ERROR_SUCCESS);
#endif

        // If this thread is created on demand we already have work to process and do not need to wait
        bool mustWait = parallelThread->synchronizeOnStartup;

        do
        {
            if (mustWait)
            {
                // Signal completion and wait for next work
                SetEvent(parallelThread->concurrentWorkDoneEvent);
                DWORD result = WaitForSingleObject(parallelThread->concurrentWorkReadyEvent, INFINITE);
                Assert(result == WAIT_OBJECT_0);
            }

            if (recycler->collectionState == CollectionStateExit)
            {
                // Exit thread
                break;
            }

            // Invoke the workFunc to do real work
            (recycler->*workFunc)();

            // We always wait after the first time
            mustWait = true;
        }
        while (true);

        // Signal to main thread that we have stopped processing and will shut down.
        // Note that after this point, we cannot access anything on the Recycler instance
        // because the main thread may have torn it down already.
        SetEvent(parallelThread->concurrentWorkDoneEvent);

#if !defined(_UCRT)
        if (dllHandle)
        {
            FreeLibraryAndExitThread(dllHandle, 0);
        }
#endif
        ret = 0;
    }
    __except(Recycler::ExceptFilter(GetExceptionInformation()))
    {
        Assert(false);
    }

    return ret;
}


// static
void
RecyclerParallelThread::StaticBackgroundWorkCallback(void * callbackData)
{
    RecyclerParallelThread * parallelThread = (RecyclerParallelThread *)callbackData;
    Recycler * recycler = parallelThread->recycler;
    RecyclerParallelThread::WorkFunc workFunc = parallelThread->workFunc;

    (recycler->*workFunc)();

    SetEvent(parallelThread->concurrentWorkDoneEvent);
}


#ifdef RECYCLER_TRACE
void
Recycler::CaptureCollectionParam(CollectionFlags flags, bool repeat)
{
    collectionParam.priorityBoostConcurentSweepOverride = false;
    collectionParam.repeat = repeat;
    collectionParam.finishOnly = false;
    collectionParam.flags = flags;
    collectionParam.uncollectedAllocBytes = autoHeap.uncollectedAllocBytes;
#ifdef PARTIAL_GC_ENABLED
    collectionParam.uncollectedNewPageCountPartialCollect = this->uncollectedNewPageCountPartialCollect;
    collectionParam.inPartialCollectMode = inPartialCollectMode;
    collectionParam.uncollectedNewPageCount = autoHeap.uncollectedNewPageCount;
    collectionParam.unusedPartialCollectFreeBytes = autoHeap.unusedPartialCollectFreeBytes;
#endif
}

void
Recycler::PrintCollectTrace(Js::Phase phase, bool finish, bool noConcurrentWork)
{
    if (GetRecyclerFlagsTable().Trace.IsEnabled(Js::RecyclerPhase) ||
        GetRecyclerFlagsTable().Trace.IsEnabled(phase))
    {
        const BOOL allocSize = collectionParam.flags & CollectHeuristic_AllocSize;
        const BOOL timedIfScriptActive = collectionParam.flags & CollectHeuristic_TimeIfScriptActive;
        const BOOL timedIfInScript = collectionParam.flags & CollectHeuristic_TimeIfInScript;
        const BOOL timed = (timedIfScriptActive && isScriptActive) || (timedIfInScript && isInScript) || (collectionParam.flags & CollectHeuristic_Time);
        const BOOL concurrent = collectionParam.flags & CollectMode_Concurrent;
        const BOOL finishConcurrent = collectionParam.flags & CollectOverride_FinishConcurrent;
        const BOOL exhaustive = collectionParam.flags & CollectMode_Exhaustive;
        const BOOL forceInThread = collectionParam.flags & CollectOverride_ForceInThread;
        const BOOL forceFinish = collectionParam.flags & CollectOverride_ForceFinish;

#ifdef PARTIAL_GC_ENABLED
        BOOL partial = collectionParam.flags & CollectMode_Partial ;
#endif

        Output::Print(L"%04X> RC(%p): %s%s%s%s%s%s%s:", this->mainThreadId, this,
            collectionParam.domCollect? L"[DOM] " : L"",
            collectionParam.repeat? L"[Repeat] ": L"",
            this->inDispose? L"[Nested]" : L"",
            forceInThread? L"Force In thread " : L"",
            finish? L"Finish " : L"",
            exhaustive? L"Exhaustive " : L"",
            Js::PhaseNames[phase]);

        if (noConcurrentWork)
        {
            Assert(finish);
            Output::Print(L" No concurrent work");
        }
        else if (collectionParam.finishOnly)
        {
            Assert(!collectionParam.repeat);
            Assert(finish);
#ifdef CONCURRENT_GC_ENABLED
            if (collectionState == CollectionStateRescanWait)
            {
                if (forceFinish)
                {
                    Output::Print(L" Force finish mark and sweep");
                }
                else if (concurrent && this->enableConcurrentSweep)
                {
                    if (!collectionParam.priorityBoostConcurentSweepOverride)
                    {
                        Output::Print(L" Finish mark and start concurrent sweep");
                    }
                    else
                    {
                        Output::Print(L" Finish mark and sweep (priority boost overrided concurrent sweep)");
                    }
                }
                else
                {
                    Output::Print(L" Finish mark and sweep");
                }
            }
            else
            {
                Assert(collectionState == CollectionStateTransferSweptWait);
                if (forceFinish)
                {
                    Output::Print(L" Force finish sweep");
                }
                else
                {
                    Output::Print(L" Finish sweep");
                }
            }
#endif // CONCURRENT_GC_ENABLED
        }
        else
        {
            if (finish && !concurrent)
            {
                Output::Print(L" Not concurrent collect");
            }
            if ((finish && finishConcurrent))
            {
                Output::Print(L" No heuristic");
            }
#ifdef CONCURRENT_GC_ENABLED
            else if (finish && priorityBoost)
            {
                Output::Print(L" Priority boost no heuristic");
            }
#endif
            else
            {
                Output::SkipToColumn(50);
                bool byteCountUsed = false;
                bool timeUsed = false;

#ifdef PARTIAL_GC_ENABLED
                bool newPageUsed = false;
                if (phase == Js::PartialCollectPhase || phase == Js::ConcurrentPartialCollectPhase)
                {
                    Assert(collectionParam.flags & CollectMode_Partial);
                    newPageUsed = !!allocSize;
                }
                else if (partial && collectionParam.inPartialCollectMode && collectionParam.uncollectedNewPageCount > collectionParam.uncollectedNewPageCountPartialCollect)
                {
                    newPageUsed = true;
                }
                else
#endif // PARTIAL_GC_ENABLED
                {
                    byteCountUsed = !!allocSize;
                    timeUsed = !!timed;
                }

                Output::Print(byteCountUsed? L"*" : (allocSize? L" " : L"~"));
                Output::Print(L"B:%8d ", collectionParam.uncollectedAllocBytes);
                Output::Print(timeUsed? L"*" : (timed? L" " : L"~"));
                Output::Print(L"T:%4d ", -collectionParam.timeDiff);
#ifdef PARTIAL_GC_ENABLED
                if (collectionParam.inPartialCollectMode)
                {
                    Output::Print(L"L:%5d ", collectionParam.uncollectedNewPageCountPartialCollect);
                }
                else
                {
                    Output::Print(L"L:----- ");
                }
                Output::Print(newPageUsed? L"*" : (partial? L" " : L"~"));
                Output::Print(L"P:%5d(%9d) ", collectionParam.uncollectedNewPageCount, collectionParam.uncollectedNewPageCount * AutoSystemInfo::PageSize);
                Output::Print(L"U:%8d", collectionParam.unusedPartialCollectFreeBytes);
#endif // PARTIAL_GC_ENABLED
            }
        }
        Output::Print(L"\n");
        Output::Flush();
    }
}
#endif

#ifdef RECYCLER_STATS
void
Recycler::PrintHeapBlockStats(wchar_t const * name, HeapBlock::HeapBlockType type)
{
    size_t liveCount = collectionStats.heapBlockCount[type] - collectionStats.heapBlockFreeCount[type];

    Output::Print(L" %6s : %5d %5d %5d %5.1f", name,
        liveCount, collectionStats.heapBlockFreeCount[type], collectionStats.heapBlockCount[type],
        (double)collectionStats.heapBlockFreeCount[type] / (double)collectionStats.heapBlockCount[type] * 100);

    if (type < HeapBlock::SmallBlockTypeCount)
    {
        Output::Print(L" : %5d %6.1f : %5d %6.1f",
            collectionStats.heapBlockSweptCount[type],
            (double)collectionStats.heapBlockSweptCount[type] / (double)liveCount * 100,
            collectionStats.heapBlockConcurrentSweptCount[type],
            (double)collectionStats.heapBlockConcurrentSweptCount[type] / (double)collectionStats.heapBlockSweptCount[type] * 100);
    }
}

void
Recycler::PrintHeapBlockMemoryStats(wchar_t const * name, HeapBlock::HeapBlockType type)
{
    size_t allocableFreeByteCount = collectionStats.heapBlockFreeByteCount[type];
    size_t partialUnusedBytes = 0;
    if (this->enablePartialCollect)
    {
        partialUnusedBytes = allocableFreeByteCount
            - collectionStats.smallNonLeafHeapBlockPartialReuseBytes[type];
        allocableFreeByteCount -= partialUnusedBytes;
    }
    size_t totalByteCount = (collectionStats.heapBlockCount[type] - collectionStats.heapBlockFreeCount[type]) * AutoSystemInfo::PageSize;
    size_t liveByteCount = totalByteCount - collectionStats.heapBlockFreeByteCount[type];
    Output::Print(L" %6s: %10d %10d", name, liveByteCount, allocableFreeByteCount);

#ifdef PARTIAL_GC_ENABLED
    if (this->enablePartialCollect &&
        (type == HeapBlock::HeapBlockType::SmallNormalBlockType
      || type == HeapBlock::HeapBlockType::SmallFinalizableBlockType
#ifdef RECYCLER_WRITE_BARRIER
      || type == HeapBlock::HeapBlockType::SmallNormalBlockWithBarrierType
      || type == HeapBlock::HeapBlockType::SmallFinalizableBlockWithBarrierType
#endif
      || type == HeapBlock::HeapBlockType::MediumNormalBlockType
      || type == HeapBlock::HeapBlockType::MediumFinalizableBlockType
#ifdef RECYCLER_WRITE_BARRIER
      || type == HeapBlock::HeapBlockType::MediumNormalBlockWithBarrierType
      || type == HeapBlock::HeapBlockType::MediumFinalizableBlockWithBarrierType
#endif
      ))
    {
        Output::Print(L" %10d", partialUnusedBytes);
    }
    else
#endif
    {
        Output::Print(L"           ");
    }

    Output::Print(L" %10d %6.1f", totalByteCount,
        (double)allocableFreeByteCount / (double)totalByteCount * 100);

#ifdef PARTIAL_GC_ENABLED
    if (this->enablePartialCollect &&
        (type == HeapBlock::HeapBlockType::SmallNormalBlockType
        || type == HeapBlock::HeapBlockType::SmallFinalizableBlockType
#ifdef RECYCLER_WRITE_BARRIER
        || type == HeapBlock::HeapBlockType::SmallNormalBlockWithBarrierType
        || type == HeapBlock::HeapBlockType::SmallFinalizableBlockWithBarrierType
#endif
        || type == HeapBlock::HeapBlockType::MediumNormalBlockType
        || type == HeapBlock::HeapBlockType::MediumFinalizableBlockType
#ifdef RECYCLER_WRITE_BARRIER
        || type == HeapBlock::HeapBlockType::MediumNormalBlockWithBarrierType
        || type == HeapBlock::HeapBlockType::MediumFinalizableBlockWithBarrierType
#endif
        ))
    {
        Output::Print(L" %6.1f", (double)partialUnusedBytes / (double)totalByteCount * 100);
    }
#endif
}

void
Recycler::PrintHeuristicCollectionStats()
{
    Output::Print(L"---------------------------------------------------------------------------------------------------------------\n");
    Output::Print(L"GC Trigger   : %10s %10s %10s", L"Start", L"Continue", L"Finish");
#ifdef PARTIAL_GC_ENABLED
    if (this->enablePartialCollect)
    {
        Output::Print(L" | Heuristics                   : %10s %10s %5s", L"", L"", L"%");
    }
#endif
    Output::Print(L"\n");

    Output::Print(L"---------------------------------------------------------------------------------------------------------------\n");
    Output::Print(L" Alloc bytes : %10d %10d %10d", collectionStats.startCollectAllocBytes, collectionStats.continueCollectAllocBytes, this->autoHeap.uncollectedAllocBytes);
#ifdef PARTIAL_GC_ENABLED
    if (this->enablePartialCollect)
    {
        Output::Print(L" | Cost                         : %10d %10d %5.1f", collectionStats.rescanRootBytes, collectionStats.estimatedPartialReuseBytes, collectionStats.collectCost * 100);
    }
#endif
    Output::Print(L"\n");

#ifdef PARTIAL_GC_ENABLED
    if (this->enablePartialCollect)
    {
        Output::Print(L"                                                | Efficacy                     : %10s %10s %5.1f\n", L"", L"", collectionStats.collectEfficacy * 100);
    }
#endif

#ifdef PARTIAL_GC_ENABLED
    if (this->enablePartialCollect)
    {
        Output::Print(L" New page    : %10d %10s %10d", collectionStats.startCollectNewPageCount, L"", autoHeap.uncollectedNewPageCount);
        Output::Print(L" | Partial Uncollect New Page   : %10d %10d", collectionStats.uncollectedNewPageCountPartialCollect * AutoSystemInfo::PageSize, this->uncollectedNewPageCountPartialCollect * AutoSystemInfo::PageSize);
        Output::Print(L"\n");
    }
#endif

    Output::Print(L" Finish try  : %10d %10s %10s", collectionStats.finishCollectTryCount, L"", L"");
#ifdef PARTIAL_GC_ENABLED
    if (this->enablePartialCollect)
    {
        Output::Print(L" | Partial Reuse Min Free Bytes :            %10d", collectionStats.partialCollectSmallHeapBlockReuseMinFreeBytes * AutoSystemInfo::PageSize);
    }
#endif
    Output::Print(L"\n");
}

void
Recycler::PrintMarkCollectionStats()
{
    size_t nonMark = collectionStats.tryMarkCount + collectionStats.tryMarkInteriorCount - collectionStats.remarkCount - collectionStats.markData.markCount;
    size_t invalidCount = nonMark - collectionStats.tryMarkNullCount - collectionStats.tryMarkUnalignedCount
        - collectionStats.tryMarkNonRecyclerMemoryCount
        - collectionStats.tryMarkInteriorNonRecyclerMemoryCount
        - collectionStats.tryMarkInteriorNullCount;
    size_t leafCount = collectionStats.markData.markCount - collectionStats.scanCount;
    Output::Print(L"---------------------------------------------------------------------------------------------------------------\n");
    Output::Print(L"Try Mark    :%9s %5s %10s | Non-Mark  : %9s %5s | Mark    :%9s %5s \n", L"Count", L"%", L"Bytes", L"Count", L"%", L"Count", L"%");
    Output::Print(L"---------------------------------------------------------------------------------------------------------------\n");
    Output::Print(L" TryMark    :%9d       %10d | Null      : %9d %5.1f | Scan    :%9d %5.1f\n",
        collectionStats.tryMarkCount, collectionStats.tryMarkCount * sizeof(void *),
        collectionStats.tryMarkNullCount, (double)collectionStats.tryMarkNullCount / (double)nonMark * 100,
        collectionStats.scanCount, (double)collectionStats.scanCount / (double)collectionStats.markData.markCount * 100);
    Output::Print(L"   Non-Mark :%9d %5.1f            | Unaligned : %9d %5.1f | Leaf    :%9d %5.1f\n",
        nonMark, (double)nonMark / (double)collectionStats.tryMarkCount * 100,
        collectionStats.tryMarkUnalignedCount, (double)collectionStats.tryMarkUnalignedCount / (double)nonMark * 100,
        leafCount, (double)leafCount / (double)collectionStats.markData.markCount * 100);
    Output::Print(L"   Mark     :%9d %5.1f %10d | Non GC    : %9d %5.1f | Track   :%9d\n",
        collectionStats.markData.markCount, (double)collectionStats.markData.markCount / (double)collectionStats.tryMarkCount * 100, collectionStats.markData.markBytes,
        collectionStats.tryMarkNonRecyclerMemoryCount, (double)collectionStats.tryMarkNonRecyclerMemoryCount / (double)nonMark * 100,
        collectionStats.trackCount);
    Output::Print(L"   Remark   :%9d %5.1f            | Invalid   : %9d %5.1f \n",
        collectionStats.remarkCount, (double)collectionStats.remarkCount / (double)collectionStats.tryMarkCount * 100,
        invalidCount, (double)invalidCount / (double)nonMark * 100);
    Output::Print(L" TryMark Int:%9d       %10d | Null Int  : %9d %5.1f | Root    :%9d | New     :%9d\n",
        collectionStats.tryMarkInteriorCount, collectionStats.tryMarkInteriorCount * sizeof(void *),
        collectionStats.tryMarkInteriorNullCount, (double)collectionStats.tryMarkInteriorNullCount / (double)nonMark * 100,
        collectionStats.rootCount, collectionStats.markThruNewObjCount);
    Output::Print(L"                                        | Non GC Int: %9d %5.1f | Stack   :%9d | NewFalse:%9d\n",
        collectionStats.tryMarkInteriorNonRecyclerMemoryCount, (double)collectionStats.tryMarkInteriorNonRecyclerMemoryCount / (double)nonMark * 100,
        collectionStats.stackCount, collectionStats.markThruFalseNewObjCount);
}

void
Recycler::PrintBackgroundCollectionStat(RecyclerCollectionStats::MarkData const& markData)
{
    Output::Print(L"BgSmall : %5d %6d %10d | BgLarge : %5d %6d %10d | BgMark :%9d ",
        markData.rescanPageCount,
        markData.rescanObjectCount,
        markData.rescanObjectByteCount,
        markData.rescanLargePageCount,
        markData.rescanLargeObjectCount,
        markData.rescanLargeByteCount,
        markData.markCount);
    double markRatio = (double)markData.markCount / (double)collectionStats.markData.markCount * 100;
    if (markRatio == 100.0)
    {
        Output::Print(L" 100");
    }
    else
    {
        Output::Print(L"%4.1f", markRatio);
    }
    Output::Print(L"\n");
}

void
Recycler::PrintBackgroundCollectionStats()
{
#ifdef CONCURRENT_GC_ENABLED
    Output::Print(L"---------------------------------------------------------------------------------------------------------------\n");
    Output::Print(L"BgSmall : %5s %6s %10s | BgLarge : %5s %6s %10s | BgMark :%9s %4s %s\n",
        L"Pages", L"Count", L"Bytes", L"Pages", L"Count", L"Bytes", L"Count", L"%", L"NonLeafBytes   %");
    Output::Print(L"---------------------------------------------------------------------------------------------------------------\n");

    this->PrintBackgroundCollectionStat(collectionStats.backgroundMarkData[0]);
    for (uint repeatCount = 1; repeatCount < RecyclerHeuristic::MaxBackgroundRepeatMarkCount; repeatCount++)
    {
        if (collectionStats.backgroundMarkData[repeatCount].markCount == 0)
        {
            break;
        }
        collectionStats.backgroundMarkData[repeatCount].rescanPageCount -= collectionStats.backgroundMarkData[repeatCount - 1].rescanPageCount;
        collectionStats.backgroundMarkData[repeatCount].rescanObjectCount -= collectionStats.backgroundMarkData[repeatCount - 1].rescanObjectCount;
        collectionStats.backgroundMarkData[repeatCount].rescanObjectByteCount -= collectionStats.backgroundMarkData[repeatCount - 1].rescanObjectByteCount;
        collectionStats.backgroundMarkData[repeatCount].rescanLargePageCount -= collectionStats.backgroundMarkData[repeatCount - 1].rescanLargePageCount;
        collectionStats.backgroundMarkData[repeatCount].rescanLargeObjectCount -= collectionStats.backgroundMarkData[repeatCount - 1].rescanLargeObjectCount;
        collectionStats.backgroundMarkData[repeatCount].rescanLargeByteCount -= collectionStats.backgroundMarkData[repeatCount - 1].rescanLargeByteCount;
        this->PrintBackgroundCollectionStat(collectionStats.backgroundMarkData[repeatCount]);
    }
#endif
}

void
Recycler::PrintMemoryStats()
{
    Output::Print(L"----------------------------------------------------------------------------------------------------------------\n");
    Output::Print(L"Memory (Bytes) %4s %10s %10s %10s %6s %6s\n", L"Live", L"Free", L"Unused", L"Total", L"Free%", L"Unused%");
    Output::Print(L"----------------------------------------------------------------------------------------------------------------\n");

    PrintHeapBlockMemoryStats(L"Small", HeapBlock::SmallNormalBlockType);
    Output::Print(L"\n");
    PrintHeapBlockMemoryStats(L"SmFin", HeapBlock::SmallFinalizableBlockType);
    Output::Print(L"\n");
#ifdef RECYCLER_WRITE_BARRIER
    PrintHeapBlockMemoryStats(L"SmSWB", HeapBlock::SmallNormalBlockWithBarrierType);
    Output::Print(L"\n");
    PrintHeapBlockMemoryStats(L"SmFinSWB", HeapBlock::SmallFinalizableBlockWithBarrierType);
    Output::Print(L"\n");
#endif
    PrintHeapBlockMemoryStats(L"SmLeaf", HeapBlock::SmallLeafBlockType);
    Output::Print(L"\n");
    PrintHeapBlockMemoryStats(L"Medium", HeapBlock::MediumNormalBlockType);
    Output::Print(L"\n");
    PrintHeapBlockMemoryStats(L"MdFin", HeapBlock::MediumFinalizableBlockType);
    Output::Print(L"\n");
#ifdef RECYCLER_WRITE_BARRIER
    PrintHeapBlockMemoryStats(L"MdSWB", HeapBlock::MediumNormalBlockWithBarrierType);
    Output::Print(L"\n");
    PrintHeapBlockMemoryStats(L"MdFinSWB", HeapBlock::MediumFinalizableBlockWithBarrierType);
    Output::Print(L"\n");
#endif
    PrintHeapBlockMemoryStats(L"MdLeaf", HeapBlock::MediumLeafBlockType);
    Output::Print(L"\n");

    size_t largeHeapBlockUnusedByteCount = collectionStats.largeHeapBlockTotalByteCount - collectionStats.largeHeapBlockUsedByteCount
        - collectionStats.heapBlockFreeByteCount[HeapBlock::LargeBlockType];
    Output::Print(L"  Large: %10d %10d %10d %10d %6.1f %6.1f\n",
        collectionStats.largeHeapBlockUsedByteCount,
        collectionStats.heapBlockFreeByteCount[HeapBlock::LargeBlockType],
        largeHeapBlockUnusedByteCount,
        collectionStats.largeHeapBlockTotalByteCount,
        (double)collectionStats.heapBlockFreeByteCount[HeapBlock::LargeBlockType] / (double)collectionStats.largeHeapBlockTotalByteCount * 100,
        (double)largeHeapBlockUnusedByteCount / (double)collectionStats.largeHeapBlockTotalByteCount * 100);

    Output::Print(L"\nSmall heap block zeroing stats since last GC\n");
    Output::Print(L"Number of blocks with sweep state empty: normal=%d finalizable=%d leaf=%d\nNumber of blocks zeroed: %d\n",
        collectionStats.numEmptySmallBlocks[HeapBlock::SmallNormalBlockType]
#ifdef RECYCLER_WRITE_BARRIER
        + collectionStats.numEmptySmallBlocks[HeapBlock::SmallNormalBlockWithBarrierType]
#endif
        , collectionStats.numEmptySmallBlocks[HeapBlock::SmallFinalizableBlockType]
#ifdef RECYCLER_WRITE_BARRIER
        + collectionStats.numEmptySmallBlocks[HeapBlock::SmallFinalizableBlockWithBarrierType]
#endif
        + collectionStats.numEmptySmallBlocks[HeapBlock::MediumNormalBlockType]
#ifdef RECYCLER_WRITE_BARRIER
        + collectionStats.numEmptySmallBlocks[HeapBlock::MediumNormalBlockWithBarrierType]
#endif
        , collectionStats.numEmptySmallBlocks[HeapBlock::MediumFinalizableBlockType]
#ifdef RECYCLER_WRITE_BARRIER
        + collectionStats.numEmptySmallBlocks[HeapBlock::MediumFinalizableBlockWithBarrierType]
#endif
        , collectionStats.numEmptySmallBlocks[HeapBlock::SmallLeafBlockType]
        + collectionStats.numEmptySmallBlocks[HeapBlock::MediumLeafBlockType],
        collectionStats.numZeroedOutSmallBlocks);
}

void
Recycler::PrintCollectStats()
{
    Output::Print(L"Collection Stats:\n");

    PrintHeuristicCollectionStats();
    PrintMarkCollectionStats();
    PrintBackgroundCollectionStats();

    size_t freeCount = collectionStats.objectSweptCount - collectionStats.objectSweptFreeListCount;
    size_t freeBytes = collectionStats.objectSweptBytes - collectionStats.objectSweptFreeListBytes;

    Output::Print(L"---------------------------------------------------------------------------------------------------------------\n");
#if defined(PARTIAL_GC_ENABLED) || defined(CONCURRENT_GC_ENABLED)
    Output::Print(L"Rescan  : %5s %6s %10s | Track   : %5s | ", L"Pages", L"Count", L"Bytes", L"Count");
#endif
    Output::Print(L"Sweep     : %7s | SweptObj  : %5s %5s %10s\n", L"Count", L"Count", L"%%", L"Bytes");
    Output::Print(L"---------------------------------------------------------------------------------------------------------------\n");
    Output::Print(L"  Small : ");
#if defined(PARTIAL_GC_ENABLED) || defined(CONCURRENT_GC_ENABLED)
    Output::Print(L"%5d %6d %10d | ", collectionStats.markData.rescanPageCount, collectionStats.markData.rescanObjectCount, collectionStats.markData.rescanObjectByteCount);
#endif
#ifdef CONCURRENT_GC_ENABLED
    Output::Print(L"Process : %5d | ", collectionStats.trackedObjectCount);
#else
    Output::Print(L"              | ");
#endif
    Output::Print(L" Scan     : %7d |  Free     : %6d %5.1f %10d\n",
        collectionStats.objectSweepScanCount,
        freeCount, (double)freeCount / (double) collectionStats.objectSweptCount * 100, freeBytes);

    Output::Print(L"  Large : ");
#if defined(PARTIAL_GC_ENABLED) || defined(CONCURRENT_GC_ENABLED)
    Output::Print(L"%5d %6d %10d | ",
        collectionStats.markData.rescanLargePageCount, collectionStats.markData.rescanLargeObjectCount, collectionStats.markData.rescanLargeByteCount);
#endif
#ifdef PARTIAL_GC_ENABLED
    Output::Print(L"Client  : %5d | ", collectionStats.clientTrackedObjectCount);
#else
    Output::Print(L"                | ");
#endif
    Output::Print(L" Finalize : %7d |  Free List: %6d %5.1f %10d\n",
        collectionStats.finalizeSweepCount,
        collectionStats.objectSweptFreeListCount, (double)collectionStats.objectSweptFreeListCount / (double) collectionStats.objectSweptCount * 100, collectionStats.objectSweptFreeListBytes);

    Output::Print(L"---------------------------------------------------------------------------------------------------------------\n");
    Output::Print(L"SweptBlk:  Live  Free Total Free%% : Swept Swept%% : CSwpt CSwpt%%");
#ifdef PARTIAL_GC_ENABLED
    if (this->enablePartialCollect)
    {
        Output::Print(L" | Partial    : Count      Bytes     Existing");
   }
#endif
    Output::Print(L"\n");
    Output::Print(L"---------------------------------------------------------------------------------------------------------------\n");

    PrintHeapBlockStats(L"Small", HeapBlock::SmallNormalBlockType);
#ifdef PARTIAL_GC_ENABLED
    if (this->enablePartialCollect)
    {
        Output::Print(L" |  Reuse     : %5d %10d %10d",
            collectionStats.smallNonLeafHeapBlockPartialReuseCount[HeapBlock::SmallNormalBlockType],
            collectionStats.smallNonLeafHeapBlockPartialReuseBytes[HeapBlock::MediumNormalBlockType],
            collectionStats.smallNonLeafHeapBlockPartialReuseCount[HeapBlock::SmallNormalBlockType] * AutoSystemInfo::PageSize
            - collectionStats.smallNonLeafHeapBlockPartialReuseBytes[HeapBlock::SmallNormalBlockType]);
    }
#endif
    Output::Print(L"\n");
    PrintHeapBlockStats(L"SmFin", HeapBlock::SmallFinalizableBlockType);
#ifdef PARTIAL_GC_ENABLED
    if (this->enablePartialCollect)
    {
        Output::Print(L" |  Unused    : %5d %10d %10d",
            collectionStats.smallNonLeafHeapBlockPartialUnusedCount[HeapBlock::SmallFinalizableBlockType],
            collectionStats.smallNonLeafHeapBlockPartialUnusedBytes[HeapBlock::SmallFinalizableBlockType],
            collectionStats.smallNonLeafHeapBlockPartialUnusedCount[HeapBlock::SmallFinalizableBlockType] * AutoSystemInfo::PageSize
                - collectionStats.smallNonLeafHeapBlockPartialUnusedBytes[HeapBlock::SmallFinalizableBlockType]);
    }
#endif
    Output::Print(L"\n");

#ifdef RECYCLER_WRITE_BARRIER
    PrintHeapBlockStats(L"SmSWB", HeapBlock::SmallNormalBlockWithBarrierType);
#ifdef PARTIAL_GC_ENABLED
    if (this->enablePartialCollect)
    {
        Output::Print(L" |  Unused    : %5d %10d %10d",
            collectionStats.smallNonLeafHeapBlockPartialUnusedCount[HeapBlock::SmallNormalBlockWithBarrierType],
            collectionStats.smallNonLeafHeapBlockPartialUnusedBytes[HeapBlock::SmallNormalBlockWithBarrierType],
            collectionStats.smallNonLeafHeapBlockPartialUnusedCount[HeapBlock::SmallNormalBlockWithBarrierType] * AutoSystemInfo::PageSize
            - collectionStats.smallNonLeafHeapBlockPartialUnusedBytes[HeapBlock::SmallNormalBlockWithBarrierType]);
    }
#endif
    Output::Print(L"\n");
    PrintHeapBlockStats(L"SmFin", HeapBlock::SmallFinalizableBlockWithBarrierType);
#ifdef PARTIAL_GC_ENABLED
    if (this->enablePartialCollect)
    {
        Output::Print(L" |  Unused    : %5d %10d %10d",
            collectionStats.smallNonLeafHeapBlockPartialUnusedCount[HeapBlock::SmallFinalizableBlockWithBarrierType],
            collectionStats.smallNonLeafHeapBlockPartialUnusedBytes[HeapBlock::SmallFinalizableBlockWithBarrierType],
            collectionStats.smallNonLeafHeapBlockPartialUnusedCount[HeapBlock::SmallFinalizableBlockWithBarrierType] * AutoSystemInfo::PageSize
            - collectionStats.smallNonLeafHeapBlockPartialUnusedBytes[HeapBlock::SmallFinalizableBlockWithBarrierType]);
    }
#endif
    Output::Print(L"\n");
#endif

    // TODO: This seems suspicious- why are we looking at smallNonLeaf while print out leaf...
    PrintHeapBlockStats(L"SmLeaf", HeapBlock::SmallLeafBlockType);
#ifdef PARTIAL_GC_ENABLED
    if (this->enablePartialCollect)
    {
        Output::Print(L" |  ReuseFin  : %5d %10d %10d",
            collectionStats.smallNonLeafHeapBlockPartialReuseCount[HeapBlock::SmallFinalizableBlockType],
            collectionStats.smallNonLeafHeapBlockPartialReuseBytes[HeapBlock::SmallFinalizableBlockType],
            collectionStats.smallNonLeafHeapBlockPartialReuseCount[HeapBlock::SmallFinalizableBlockType] * AutoSystemInfo::PageSize
                - collectionStats.smallNonLeafHeapBlockPartialReuseBytes[HeapBlock::SmallFinalizableBlockType]);
    }
#endif
    Output::Print(L"\n");

    PrintHeapBlockStats(L"Medium", HeapBlock::MediumNormalBlockType);
#ifdef PARTIAL_GC_ENABLED
    if (this->enablePartialCollect)
    {
        Output::Print(L" |  Reuse     : %5d %10d %10d",
            collectionStats.smallNonLeafHeapBlockPartialReuseCount[HeapBlock::MediumNormalBlockType],
            collectionStats.smallNonLeafHeapBlockPartialReuseBytes[HeapBlock::MediumNormalBlockType],
            collectionStats.smallNonLeafHeapBlockPartialReuseCount[HeapBlock::MediumNormalBlockType] * AutoSystemInfo::PageSize
            - collectionStats.smallNonLeafHeapBlockPartialReuseBytes[HeapBlock::MediumNormalBlockType]);
    }
#endif
    Output::Print(L"\n");
    PrintHeapBlockStats(L"MdFin", HeapBlock::MediumFinalizableBlockType);
#ifdef PARTIAL_GC_ENABLED
    if (this->enablePartialCollect)
    {
        Output::Print(L" |  Unused    : %5d %10d %10d",
            collectionStats.smallNonLeafHeapBlockPartialUnusedCount[HeapBlock::MediumFinalizableBlockType],
            collectionStats.smallNonLeafHeapBlockPartialUnusedBytes[HeapBlock::MediumFinalizableBlockType],
            collectionStats.smallNonLeafHeapBlockPartialUnusedCount[HeapBlock::MediumFinalizableBlockType] * AutoSystemInfo::PageSize
            - collectionStats.smallNonLeafHeapBlockPartialUnusedBytes[HeapBlock::MediumFinalizableBlockType]);
    }
#endif
    Output::Print(L"\n");

#ifdef RECYCLER_WRITE_BARRIER
    PrintHeapBlockStats(L"MdSWB", HeapBlock::MediumNormalBlockWithBarrierType);
#ifdef PARTIAL_GC_ENABLED
    if (this->enablePartialCollect)
    {
        Output::Print(L" |  Unused    : %5d %10d %10d",
            collectionStats.smallNonLeafHeapBlockPartialUnusedCount[HeapBlock::MediumNormalBlockWithBarrierType],
            collectionStats.smallNonLeafHeapBlockPartialUnusedBytes[HeapBlock::MediumNormalBlockWithBarrierType],
            collectionStats.smallNonLeafHeapBlockPartialUnusedCount[HeapBlock::MediumNormalBlockWithBarrierType] * AutoSystemInfo::PageSize
            - collectionStats.smallNonLeafHeapBlockPartialUnusedBytes[HeapBlock::MediumNormalBlockWithBarrierType]);
    }
#endif
    Output::Print(L"\n");
    PrintHeapBlockStats(L"MdFin", HeapBlock::MediumFinalizableBlockWithBarrierType);
#ifdef PARTIAL_GC_ENABLED
    if (this->enablePartialCollect)
    {
        Output::Print(L" |  Unused    : %5d %10d %10d",
            collectionStats.smallNonLeafHeapBlockPartialUnusedCount[HeapBlock::MediumFinalizableBlockWithBarrierType],
            collectionStats.smallNonLeafHeapBlockPartialUnusedBytes[HeapBlock::MediumFinalizableBlockWithBarrierType],
            collectionStats.smallNonLeafHeapBlockPartialUnusedCount[HeapBlock::MediumFinalizableBlockWithBarrierType] * AutoSystemInfo::PageSize
            - collectionStats.smallNonLeafHeapBlockPartialUnusedBytes[HeapBlock::MediumFinalizableBlockWithBarrierType]);
    }
#endif
    Output::Print(L"\n");
#endif

    // TODO: This seems suspicious- why are we looking at smallNonLeaf while print out leaf...
    PrintHeapBlockStats(L"MdLeaf", HeapBlock::MediumNormalBlockType);
#ifdef PARTIAL_GC_ENABLED
    if (this->enablePartialCollect)
    {
        Output::Print(L" |  ReuseFin  : %5d %10d %10d",
            collectionStats.smallNonLeafHeapBlockPartialReuseCount[HeapBlock::MediumFinalizableBlockType],
            collectionStats.smallNonLeafHeapBlockPartialReuseBytes[HeapBlock::MediumFinalizableBlockType],
            collectionStats.smallNonLeafHeapBlockPartialReuseCount[HeapBlock::MediumFinalizableBlockType] * AutoSystemInfo::PageSize
            - collectionStats.smallNonLeafHeapBlockPartialReuseBytes[HeapBlock::MediumFinalizableBlockType]);
    }
#endif
    Output::Print(L"\n");

    // TODO: This can't possibly be correct...check on this later
    PrintHeapBlockStats(L"Large", HeapBlock::LargeBlockType);
#ifdef PARTIAL_GC_ENABLED
    if (this->enablePartialCollect)
    {
        Output::Print(L"                               |  UnusedFin : %5d %10d %10d",
            collectionStats.smallNonLeafHeapBlockPartialUnusedCount[HeapBlock::SmallFinalizableBlockType],
            collectionStats.smallNonLeafHeapBlockPartialUnusedBytes[HeapBlock::SmallFinalizableBlockType],
            collectionStats.smallNonLeafHeapBlockPartialUnusedCount[HeapBlock::SmallFinalizableBlockType] * AutoSystemInfo::PageSize
                - collectionStats.smallNonLeafHeapBlockPartialUnusedBytes[HeapBlock::SmallFinalizableBlockType]);
    }
#endif
    Output::Print(L"\n");

    PrintMemoryStats();

    Output::Flush();
}
#endif

#ifdef RECYCLER_ZERO_MEM_CHECK
void
Recycler::VerifyZeroFill(void * address, size_t size)
{
    byte expectedFill = 0;
#ifdef RECYCLER_MEMORY_VERIFY
    if (this->VerifyEnabled())
    {
        expectedFill = Recycler::VerifyMemFill;
    }
#endif
    for (uint i = 0; i < size; i++)
    {
        Assert(((byte *)address)[i] == expectedFill);
    }
}
#endif

#ifdef RECYCLER_MEMORY_VERIFY
void
Recycler::FillCheckPad(void * address, size_t size, size_t alignedAllocSize, bool objectAlreadyInitialized)
{
    if (this->VerifyEnabled())
    {
        void* addressToVerify = address;
        size_t sizeToVerify = alignedAllocSize;

        if (objectAlreadyInitialized)
        {
            addressToVerify = ((char*) address + size);
            sizeToVerify = (alignedAllocSize - size);
        }

        // Actually this is filling the non-pad to zero
        VerifyCheckFill(addressToVerify, sizeToVerify - sizeof(size_t));

        // Ignore the first word
        if (!objectAlreadyInitialized && size > sizeof(FreeObject))
        {
            memset((char *)address + sizeof(FreeObject), 0, size - sizeof(FreeObject));
        }

        // write the pad size at the end;
        *(size_t *)((char *)address + alignedAllocSize - sizeof(size_t)) = alignedAllocSize - size;
    }
}

void Recycler::Verify(Js::Phase phase)
{
    if (verifyEnabled && (!this->CollectionInProgress()))
    {
        if (GetRecyclerFlagsTable().RecyclerVerify.IsEnabled(phase))
        {
            autoHeap.Verify();
        }
    }
}

void Recycler::VerifyCheck(BOOL cond, wchar_t const * msg, void * address, void * corruptedAddress)
{
    if (!(cond))
    {
        fwprintf(stderr, L"RECYCLER CORRUPTION: StartAddress=%p CorruptedAddress=%p: %s", address, corruptedAddress, msg);
        Js::Throw::FatalInternalError();
    }
}

void Recycler::VerifyCheckFill(void * address, size_t size)
{
    for (byte * i = (byte *)address; i < (byte *)address + size; i++)
    {
        Recycler::VerifyCheck(*i == Recycler::VerifyMemFill, L"memory written after freed", address, i);
    }
}

void Recycler::VerifyCheckPadExplicitFreeList(void * address, size_t size)
{
    size_t * paddingAddress = (size_t *)((byte *)address + size - sizeof(size_t));
    size_t padding = *paddingAddress;

#pragma warning(suppress:4310)
    Assert(padding != (size_t)0xCACACACACACACACA);  // Explicit free objects have to have been initialized at some point before they were freed

    Recycler::VerifyCheck(padding >= verifyPad + sizeof(size_t) &&  padding < size, L"Invalid padding size", address, paddingAddress);
    for (byte * i = (byte *)address + size - padding; i < (byte *)paddingAddress; i++)
    {
        Recycler::VerifyCheck(*i == Recycler::VerifyMemFill, L"buffer overflow", address, i);
    }
}
void Recycler::VerifyCheckPad(void * address, size_t size)
{
    size_t * paddingAddress = (size_t *)((byte *)address + size - sizeof(size_t));
    size_t padding = *paddingAddress;

#pragma warning(suppress:4310)
    if (padding == (size_t)0xCACACACACACACACA)
    {
        // Nascent block have objects that are not initialized with pad size
        Recycler::VerifyCheckFill(address, size);
        return;
    }
    Recycler::VerifyCheck(padding >= verifyPad + sizeof(size_t) &&  padding < size, L"Invalid padding size", address, paddingAddress);
    for (byte * i = (byte *)address + size - padding; i < (byte *)paddingAddress; i++)
    {
        Recycler::VerifyCheck(*i == Recycler::VerifyMemFill, L"buffer overflow", address, i);
    }
}
#endif

Recycler::AutoSetupRecyclerForNonCollectingMark::AutoSetupRecyclerForNonCollectingMark(Recycler& recycler, bool setupForHeapEnumeration)
    : m_recycler(recycler), m_setupDone(false)
{
    if (! setupForHeapEnumeration)
    {
        DoCommonSetup();
    }
}

void Recycler::AutoSetupRecyclerForNonCollectingMark::DoCommonSetup()
{
    Assert(m_recycler.collectionState == CollectionStateNotCollecting || m_recycler.collectionState == CollectionStateExit);
    Assert(!m_recycler.DoQueueTrackedObject());
#ifdef PARTIAL_GC_ENABLED
    // We need to get out of partial collect before we do the mark because we
    // will mess with the free bit vector state
    // GC-CONSIDER: don't mess with the free bit vector?
    if (m_recycler.inPartialCollectMode)
    {
        m_recycler.FinishPartialCollect();
    }
#endif
    m_previousCollectionState = m_recycler.collectionState;
#ifdef RECYCLER_STATS
    m_previousCollectionStats = m_recycler.collectionStats;
    memset(&m_recycler.collectionStats, 0, sizeof(RecyclerCollectionStats));
#endif
    m_setupDone = true;
}

void Recycler::AutoSetupRecyclerForNonCollectingMark::SetupForHeapEnumeration()
{
    Assert(!m_recycler.isHeapEnumInProgress);
    Assert(!m_recycler.allowAllocationDuringHeapEnum);
    m_recycler.EnsureNotCollecting();
    DoCommonSetup();
    m_recycler.ResetMarks(ResetMarkFlags_HeapEnumeration);
    m_recycler.collectionState = CollectionStateNotCollecting;
    m_recycler.isHeapEnumInProgress = true;
    m_recycler.isCollectionDisabled = true;
}

Recycler::AutoSetupRecyclerForNonCollectingMark::~AutoSetupRecyclerForNonCollectingMark()
{
    Assert(m_setupDone);
    Assert(!m_recycler.allowAllocationDuringHeapEnum);
#ifdef RECYCLER_STATS
    m_recycler.collectionStats = m_previousCollectionStats;
#endif
    m_recycler.collectionState = m_previousCollectionState;
    m_recycler.isHeapEnumInProgress = false;
    m_recycler.isCollectionDisabled = false;
}

#ifdef RECYCLER_DUMP_OBJECT_GRAPH
bool Recycler::DumpObjectGraph(RecyclerObjectGraphDumper::Param * param)
{
    bool succeeded = false;
    bool isExited = (this->collectionState == CollectionStateExit);
    if (isExited)
    {
        this->collectionState = CollectionStateNotCollecting;
    }
    if (this->collectionState != CollectionStateNotCollecting)
    {
        Output::Print(L"Can't dump object graph when collecting\n");
        Output::Flush();
        return succeeded;
    }
    BEGIN_NO_EXCEPTION
    {
        RecyclerObjectGraphDumper objectGraphDumper(this, param);

        Recycler::AutoSetupRecyclerForNonCollectingMark AutoSetupRecyclerForNonCollectingMark(*this);
        AutoRestoreValue<bool> skipStackToggle(&this->skipStack, this->skipStack || (param && param->skipStack));

        this->Mark();

        this->objectGraphDumper = nullptr;
#ifdef RECYCLER_STATS
        if (param)
        {
            param->stats = this->collectionStats;
        }
#endif
        succeeded = !objectGraphDumper.isOutOfMemory;
    }
    END_NO_EXCEPTION

    if (isExited)
    {
        this->collectionState = CollectionStateExit;
    }

    if (!succeeded)
    {
        Output::Print(L"Out of memory dumping object graph\n");
    }
    Output::Flush();
    return succeeded;
}

void
Recycler::DumpObjectDescription(void *objectAddress)
{
#ifdef PROFILE_RECYCLER_ALLOC
    type_info const * typeinfo = nullptr;
    bool isArray = false;

    if (this->trackerDictionary)
    {
        TrackerData * trackerData = GetTrackerData(objectAddress);
        if (trackerData != nullptr)
        {
            typeinfo = trackerData->typeinfo;
            isArray = trackerData->isArray;
        }
        else
        {
            Assert(false);
        }
    }
    RecyclerObjectDumper::DumpObject(typeinfo, isArray, objectAddress);
#else
    Output::Print(L"Address %p", objectAddress);
#endif
}
#endif

#ifdef RECYCLER_STRESS
// All stress mode collect art implicitly instantiate here
bool
Recycler::StressCollectNow()
{
    if (this->recyclerStress)
    {
        this->CollectNow<CollectStress>();
        return true;
    }
#ifdef CONCURRENT_GC_ENABLED
    else if (this->recyclerBackgroundStress)
    {
        this->CollectNow<CollectBackgroundStress>();
        return true;
    }
    else if ((this->enableConcurrentMark || this->enableConcurrentSweep)
        && (this->recyclerConcurrentStress
        || this->recyclerConcurrentRepeatStress))
    {
#ifdef PARTIAL_GC_ENABLED
        if (this->recyclerPartialStress)
        {
            this->CollectNow<CollectConcurrentPartialStress>();
            return true;
        }
        else
#endif // PARTIAL_GC_ENABLED
        {
            this->CollectNow<CollectConcurrentStress>();
            return true;
        }
    }
#endif // CONCURRENT_GC_ENABLED
#ifdef PARTIAL_GC_ENABLED
    else if (this->recyclerPartialStress)
    {
        this->CollectNow<CollectPartialStress>();
        return true;
    }
#endif // PARTIAL_GC_ENABLED
    return false;
}
#endif // RECYCLER_STRESS

#ifdef TRACK_ALLOC

Recycler *
Recycler::TrackAllocInfo(TrackAllocData const& data)
{
#ifdef PROFILE_RECYCLER_ALLOC
    if (this->trackerDictionary != nullptr)
    {
        Assert(nextAllocData.IsEmpty());
        nextAllocData = data;
    }
#endif
    return this;
}

void
Recycler::ClearTrackAllocInfo(TrackAllocData* data/* = NULL*/)
{
#ifdef PROFILE_RECYCLER_ALLOC
    if (this->trackerDictionary != nullptr)
    {
        AssertMsg(!nextAllocData.IsEmpty(), "Missing tracking information for this allocation, are you not using the macros?");
        if (data)
        {
            *data = nextAllocData;
        }
        nextAllocData.Clear();
    }
#endif
}

#ifdef PROFILE_RECYCLER_ALLOC
bool
Recycler::DoProfileAllocTracker()
{
    bool doTracker = false;
#ifdef RECYCLER_DUMP_OBJECT_GRAPH
    doTracker = Js::Configuration::Global.flags.DumpObjectGraphOnExit
        || Js::Configuration::Global.flags.DumpObjectGraphOnCollect
        || Js::Configuration::Global.flags.DumpObjectGraphOnEnum;
#endif
#ifdef LEAK_REPORT
    if (Js::Configuration::Global.flags.IsEnabled(Js::LeakReportFlag))
    {
        doTracker = true;
    }
#endif
#ifdef CHECK_MEMORY_LEAK
    if (Js::Configuration::Global.flags.CheckMemoryLeak)
    {
        doTracker = true;
    }
#endif
    return doTracker || MemoryProfiler::DoTrackRecyclerAllocation();
}

void
Recycler::InitializeProfileAllocTracker()
{
    if (DoProfileAllocTracker())
    {
        trackerDictionary = NoCheckHeapNew(TypeInfotoTrackerItemMap, &NoCheckHeapAllocator::Instance, 163);

#pragma prefast(suppress:6031, "InitializeCriticalSectionAndSpinCount always succeed since Vista. No need to check return value");
        InitializeCriticalSectionAndSpinCount(&trackerCriticalSection, 1000);
    }

    nextAllocData.Clear();
}

void
Recycler::TrackAllocCore(void * object, size_t size, const TrackAllocData& trackAllocData, bool traceLifetime)
{
    Assert(GetTrackerData(object) == nullptr || GetTrackerData(object) == &TrackerData::ExplicitFreeListObjectData);
    Assert(trackAllocData.GetTypeInfo() != nullptr);
    TrackerItem * item;
    size_t allocCount = trackAllocData.GetCount();
    size_t itemSize = (size - trackAllocData.GetPlusSize());
    bool isArray;
    if (allocCount != (size_t)-1)
    {
        isArray = true;
        itemSize = itemSize / allocCount;
    }
    else
    {
        isArray = false;
        allocCount = 1;
    }
    if (!trackerDictionary->TryGetValue(trackAllocData.GetTypeInfo(), &item))
    {
        item = NoCheckHeapNew(TrackerItem, trackAllocData.GetTypeInfo());
        item->instanceData.ItemSize = itemSize;
        item->arrayData.ItemSize = itemSize;
        trackerDictionary->Item(trackAllocData.GetTypeInfo(), item);
    }
    else
    {
        Assert(item->instanceData.typeinfo == trackAllocData.GetTypeInfo());
        Assert(item->instanceData.ItemSize == itemSize);
        Assert(item->arrayData.ItemSize == itemSize);
    }
    TrackerData& data = (isArray)? item->arrayData : item->instanceData;
        data.ItemCount += allocCount;
        data.AllocCount++;
        data.ReqSize += size;
        data.AllocSize += HeapInfo::GetAlignedSizeNoCheck(size);
#ifdef TRACE_OBJECT_LIFETIME
    data.TraceLifetime = traceLifetime;

    if (traceLifetime)
    {
        Output::Print(data.isArray ? L"Allocated %S[] %p\n" : L"Allocated %S %p\n", data.typeinfo->name(), object);
    }
#endif
#ifdef PERF_COUNTERS
    ++data.counter;
    data.sizeCounter += HeapInfo::GetAlignedSizeNoCheck(size);
#endif

    SetTrackerData(object, &data);
}

void* Recycler::TrackAlloc(void* object, size_t size, const TrackAllocData& trackAllocData, bool traceLifetime)
{
    if (this->trackerDictionary != nullptr)
    {
        Assert(nextAllocData.IsEmpty()); // should have been cleared
        EnterCriticalSection(&trackerCriticalSection);
        TrackAllocCore(object, size, trackAllocData);
        LeaveCriticalSection(&trackerCriticalSection);
    }
    return object;
}

void
Recycler::TrackIntegrate(__in_ecount(blockSize) char * blockAddress, size_t blockSize, size_t allocSize, size_t objectSize, const TrackAllocData& trackAllocData)
{
    if (this->trackerDictionary != nullptr)
    {
        Assert(nextAllocData.IsEmpty()); // should have been cleared
        EnterCriticalSection(&trackerCriticalSection);

        char * address = blockAddress;
        char * blockEnd = blockAddress + blockSize;
        while (address + allocSize <= blockEnd)
        {
            TrackAllocCore(address, objectSize, trackAllocData);
            address += allocSize;
        }

        LeaveCriticalSection(&trackerCriticalSection);
    }
}

BOOL Recycler::TrackFree(const char* address, size_t size)
{
    if (this->trackerDictionary != nullptr)
    {
        EnterCriticalSection(&trackerCriticalSection);
        TrackerData * data = GetTrackerData((char *)address);
        if (data != nullptr)
        {
            if (data != &TrackerData::EmptyData)
            {
#ifdef PERF_COUNTERS
                --data->counter;
                data->sizeCounter -= size;
#endif
                if (data->typeinfo == &typeid(RecyclerWeakReferenceBase))
                {
                    TrackFreeWeakRef((RecyclerWeakReferenceBase *)address);
                }
                data->FreeSize += size;
                data->FreeCount++;
#ifdef TRACE_OBJECT_LIFETIME
                if (data->TraceLifetime)
                {
                    Output::Print(data->isArray ? L"Freed %S[] %p\n" : L"Freed %S %p\n", data->typeinfo->name(), address);
                }
#endif
            }
            SetTrackerData((char *)address, nullptr);
        }
        else
        {
            Assert(false);
        }
        LeaveCriticalSection(&trackerCriticalSection);
    }
    return true;
}


Recycler::TrackerData *
Recycler::GetTrackerData(void * address)
{
    HeapBlock * heapBlock = this->FindHeapBlock(address);
    Assert(heapBlock != nullptr);
    return (Recycler::TrackerData *)heapBlock->GetTrackerData(address);
}

void
Recycler::SetTrackerData(void * address, TrackerData * data)
{
    HeapBlock * heapBlock = this->FindHeapBlock(address);
    Assert(heapBlock != nullptr);
    heapBlock->SetTrackerData(address, data);
}

void
Recycler::TrackUnallocated(__in char* address, __in  char *endAddress, size_t sizeCat)
{
    if (this->trackerDictionary != nullptr)
    {
        EnterCriticalSection(&trackerCriticalSection);
        while (address + sizeCat <= endAddress)
        {
            Assert(GetTrackerData(address) == nullptr);
            SetTrackerData(address, &TrackerData::EmptyData);
            address += sizeCat;
        }
        LeaveCriticalSection(&trackerCriticalSection);
    }
}

void
Recycler::TrackAllocWeakRef(RecyclerWeakReferenceBase * weakRef)
{
    Assert(weakRef->typeInfo != nullptr);
#if DBG && defined(PERF_COUNTERS)
    if (this->trackerDictionary != nullptr)
    {
        TrackerItem * item;
        if (trackerDictionary->TryGetValue(weakRef->typeInfo, &item))
        {
            weakRef->counter = &item->weakRefCounter;
        }
        else
        {
            weakRef->counter = &PerfCounter::RecyclerTrackerCounterSet::GetWeakRefPerfCounter(weakRef->typeInfo);
        }
        ++(*weakRef->counter);
    }
#endif
}

void
Recycler::TrackFreeWeakRef(RecyclerWeakReferenceBase * weakRef)
{
#if DBG && defined(PERF_COUNTERS)
    if (weakRef->counter != nullptr)
    {
        --(*weakRef->counter);
    }
#endif
}

void
Recycler::PrintAllocStats()
{
    if (this->trackerDictionary == nullptr)
    {
        return;
    }
    size_t itemCount = 0;
    int allocCount = 0;
    int64 reqSize = 0;
    int64 allocSize = 0;
    int freeCount = 0;
    int64 freeSize = 0;
    Output::Print(L"=================================================================================================================\n");
    Output::Print(L"Recycler Allocations\n");
    Output::Print(L"=================================================================================================================\n");
    Output::Print(L"ItemSize  ItemCount   AllocCount  RequestSize      AllocSize        FreeCount   FreeSize         DiffCount   DiffSize        \n");
    Output::Print(L"--------  ----------  ----------  ---------------  ---------------  ----------  ---------------  ----------  ---------------\n");
    for (int i = 0; i < trackerDictionary->Count(); i++)
    {
        TrackerItem * item = trackerDictionary->GetValueAt(i);
        type_info const * typeinfo = trackerDictionary->GetKeyAt(i);
        if (item->instanceData.AllocCount != 0)
        {
            Output::Print(L"%8d  %10d  %10d  %15I64d  %15I64d  %10d  %15I64d  %10d  %15I64d  %S\n",
                item->instanceData.ItemSize, item->instanceData.ItemCount, item->instanceData.AllocCount, item->instanceData.ReqSize,
                item->instanceData.AllocSize, item->instanceData.FreeCount, item->instanceData.FreeSize,
                item->instanceData.AllocCount - item->instanceData.FreeCount,  item->instanceData.AllocSize - item->instanceData.FreeSize, typeinfo->name());
            itemCount += item->instanceData.ItemCount;
            allocCount += item->instanceData.AllocCount;
            reqSize += item->instanceData.ReqSize;
            allocSize += item->instanceData.AllocSize;
            freeCount += item->instanceData.FreeCount;
            freeSize += item->instanceData.FreeSize;
        }

        if (item->arrayData.AllocCount != 0)
        {
            Output::Print(L"%8d  %10d  %10d  %15I64d  %15I64d  %10d  %15I64d  %10d  %15I64d  %S[]\n",
                item->arrayData.ItemSize, item->arrayData.ItemCount, item->arrayData.AllocCount, item->arrayData.ReqSize,
                item->arrayData.AllocSize, item->arrayData.FreeCount, item->arrayData.FreeSize,
                item->instanceData.AllocCount - item->instanceData.FreeCount, item->arrayData.AllocSize - item->arrayData.FreeSize, typeinfo->name());
            itemCount += item->arrayData.ItemCount;
            allocCount += item->arrayData.AllocCount;
            reqSize += item->arrayData.ReqSize;
            allocSize += item->arrayData.AllocSize;
            freeCount += item->arrayData.FreeCount;
            freeSize += item->arrayData.FreeSize;
        }
    }
    Output::Print(L"--------  ----------  ----------  ---------------  ---------------  ----------  ---------------  ----------  ---------------\n");
    Output::Print(L"            %8d  %10d  %15I64d  %15I64d  %10d  %15I64d  %10d  %15I64d  **Total**\n",
        itemCount, allocCount, reqSize, allocSize, freeCount, freeSize, allocCount - freeCount, allocSize - freeSize);

#ifdef EXCEL_FRIENDLY_DUMP
    Output::Print(L"\nExcel friendly version\nItemSize\tItemCount\tAllocCount\tRequestSize\tAllocSize\tFreeCount\tFreeSize\tDiffCount\tDiffSize\tType\n");
    for (int i = 0; i < trackerDictionary->Count(); i++)
    {
        TrackerItem * item = trackerDictionary->GetValueAt(i);
        type_info const * typeinfo = trackerDictionary->GetKeyAt(i);
        if (item->instanceData.AllocCount != 0)
        {
            Output::Print(L"%d\t%d\t%d\t%I64d\t%I64d\t%d\t%I64d\t%d\t%I64d\t%S\n",
                item->instanceData.ItemSize, item->instanceData.ItemCount, item->instanceData.AllocCount, item->instanceData.ReqSize,
                item->instanceData.AllocSize, item->instanceData.FreeCount, item->instanceData.FreeSize,
                item->instanceData.AllocCount - item->instanceData.FreeCount,  item->instanceData.AllocSize - item->instanceData.FreeSize, typeinfo->name());
        }
        if (item->arrayData.AllocCount != 0)
        {
            Output::Print(L"%d\t%d\t%d\t%I64d\t%I64d\t%d\t%I64d\t%d\t%I64d\t%S[]\n",
                item->arrayData.ItemSize, item->arrayData.ItemCount, item->arrayData.AllocCount, item->arrayData.ReqSize,
                item->arrayData.AllocSize, item->arrayData.FreeCount, item->arrayData.FreeSize,
                item->instanceData.AllocCount - item->instanceData.FreeCount, item->arrayData.AllocSize - item->arrayData.FreeSize, typeinfo->name());
        }
    }
#endif // EXCEL_FRIENDLY_DUMP
    Output::Flush();
}
#endif // PROFILE_RECYCLER_ALLOC
#endif // TRACK_ALLOC

#ifdef RECYCLER_VERIFY_MARK
void
Recycler::VerifyMark()
{
    VerifyMarkRoots();
    // Can't really verify stack since the recycler code between ScanStack to now may have introduce false references.
    // VerifyMarkStack();
    autoHeap.VerifyMark();
}

void
Recycler::VerifyMarkRoots()
{
    {
        this->VerifyMark(transientPinnedObject);
        pinnedObjectMap.Map([this](void * obj, PinRecord const &refCount)
        {
            if (refCount == 0)
            {
                Assert(this->hasPendingUnpinnedObject);
            }
            else
            {
                // Use the pinrecord as the source reference
                this->VerifyMark(obj);
            }
        });
    }

    DList<GuestArenaAllocator, HeapAllocator>::Iterator guestArenaIter(&guestArenaList);
    while (guestArenaIter.Next())
    {
        if (guestArenaIter.Data().pendingDelete)
        {
            Assert(this->hasPendingDeleteGuestArena);
        }
        else
        {
            VerifyMarkArena(&guestArenaIter.Data());
        }
    }

    DList<ArenaData *, HeapAllocator>::Iterator externalGuestArenaIter(&externalGuestArenaList);
    while (externalGuestArenaIter.Next())
    {
        VerifyMarkArena(externalGuestArenaIter.Data());
    }

    // We can't check external roots here
}

void
Recycler::VerifyMarkArena(ArenaData * alloc)
{
    VerifyMarkBigBlockList(alloc->GetBigBlocks(false));
    VerifyMarkBigBlockList(alloc->GetFullBlocks());
    VerifyMarkArenaMemoryBlockList(alloc->GetMemoryBlocks());
}

void
Recycler::VerifyMarkBigBlockList(BigBlock * memoryBlocks)
{
    size_t scanRootBytes = 0;
    BigBlock *blockp = memoryBlocks;
    while (blockp != NULL)
    {
        void** base=(void**)blockp->GetBytes();
        size_t slotCount = blockp->currentByte / sizeof(void*);
        scanRootBytes +=  blockp->currentByte;
        for (size_t i=0; i < slotCount; i++)
        {
            VerifyMark(base[i]);
        }
        blockp = blockp->nextBigBlock;
    }
}

void
Recycler::VerifyMarkArenaMemoryBlockList(ArenaMemoryBlock * memoryBlocks)
{
    size_t scanRootBytes = 0;
    ArenaMemoryBlock *blockp = memoryBlocks;
    while (blockp != NULL)
    {
        void** base=(void**)blockp->GetBytes();
        size_t slotCount = blockp->nbytes / sizeof(void*);
        scanRootBytes += blockp->nbytes;
        for (size_t i=0; i< slotCount; i++)
        {
            VerifyMark(base[i]);
        }
        blockp = blockp->next;
    }
}

void
Recycler::VerifyMarkStack()
{
    SAVE_THREAD_CONTEXT();
    void ** stackTop = (void**) this->savedThreadContext.GetStackTop();

    void * stackStart = GetStackBase();
    Assert(stackStart > stackTop);

    for (;stackTop < stackStart; stackTop++)
    {
        void* candidate = *stackTop;
        VerifyMark(candidate);
    }

    void** registers = this->savedThreadContext.GetRegisters();
    for (int i = 0; i < SavedRegisterState::NumRegistersToSave; i++)
    {
        VerifyMark(registers[i]);
    }
}

void
Recycler::VerifyMark(void * candidate)
{
    void * realAddress;
    HeapBlock * heapBlock;
    if (this->enableScanInteriorPointers)
    {
        heapBlock = heapBlockMap.GetHeapBlock(candidate);
        if (heapBlock == nullptr)
        {
            return;
        }
        realAddress = heapBlock->GetRealAddressFromInterior(candidate);
        if (realAddress == nullptr)
        {
            return;
        }
    }
    else
    {
        heapBlock = this->FindHeapBlock(candidate);
        if (heapBlock == nullptr)
        {
            return;
        }
        realAddress = candidate;
    }
    heapBlock->VerifyMark(realAddress);
}
#endif

ArenaAllocator *
Recycler::CreateGuestArena(wchar_t const * name, void (*outOfMemoryFunc)())
{
    // Note, guest arenas use the large block allocator.
    return guestArenaList.PrependNode(&HeapAllocator::Instance, name, &recyclerLargeBlockPageAllocator, outOfMemoryFunc);
}

void
Recycler::DeleteGuestArena(ArenaAllocator * arenaAllocator)
{
    GuestArenaAllocator * guestArenaAllocator = static_cast<GuestArenaAllocator *>(arenaAllocator);
    if (this->hasPendingConcurrentFindRoot)
    {
        // We are doing concurrent find root, don't modify the list and mark the arena to be delete
        // later when we do find root in thread.
        Assert(guestArenaList.HasElement(guestArenaAllocator));
        this->hasPendingDeleteGuestArena = true;
        guestArenaAllocator->pendingDelete = true;
    }
    else
    {
        guestArenaList.RemoveElement(&HeapAllocator::Instance, guestArenaAllocator);
    }
}

#ifdef LEAK_REPORT
void
Recycler::ReportLeaks()
{
    if (GetRecyclerFlagsTable().IsEnabled(Js::LeakReportFlag))
    {
        if (GetRecyclerFlagsTable().ForceMemoryLeak)
        {
            AUTO_HANDLED_EXCEPTION_TYPE(ExceptionType_DisableCheck);
            struct FakeMemory { int f; };
            FakeMemory * f = RecyclerNewStruct(this, FakeMemory);
            this->RootAddRef(f);
        }

        LeakReport::StartSection(L"Object Graph");
        LeakReport::StartRedirectOutput();

        RecyclerObjectGraphDumper::Param param = { 0 };
        param.skipStack = true;
        if (!this->DumpObjectGraph(&param))
        {
            LeakReport::Print(L"--------------------------------------------------------------------------------\n");
            LeakReport::Print(L"ERROR: Out of memory generating leak report\n");
            param.stats.markData.markCount = 0;
        }

        LeakReport::EndRedirectOutput();

        if (param.stats.markData.markCount != 0)
        {
            LeakReport::Print(L"--------------------------------------------------------------------------------\n");
            LeakReport::Print(L"Recycler Leaked Object: %d bytes (%d objects)\n",
                param.stats.markData.markBytes, param.stats.markData.markCount);

            if (GetRecyclerFlagsTable().LeakStackTrace)
            {
                LeakReport::StartSection(L"Pinned object stack traces");
                LeakReport::StartRedirectOutput();
                this->PrintPinnedObjectStackTraces();
                LeakReport::EndRedirectOutput();
                LeakReport::EndSection();
            }
        }
        LeakReport::EndSection();
    }
}

void
Recycler::ReportLeaksOnProcessDetach()
{
    if (GetRecyclerFlagsTable().IsEnabled(Js::LeakReportFlag))
    {
        AUTO_LEAK_REPORT_SECTION(this->GetRecyclerFlagsTable(), L"Recycler (%p): Process Termination", this);
        LeakReport::StartRedirectOutput();
        ReportOnProcessDetach([=]() { this->ReportLeaks(); });
        LeakReport::EndRedirectOutput();
    }
}
#endif

#ifdef CHECK_MEMORY_LEAK
void
Recycler::CheckLeaks(wchar_t const * header)
{
    if (GetRecyclerFlagsTable().CheckMemoryLeak && this->isPrimaryMarkContextInitialized)
    {
        if (GetRecyclerFlagsTable().ForceMemoryLeak)
        {
            AUTO_HANDLED_EXCEPTION_TYPE(ExceptionType_DisableCheck);
            struct FakeMemory { int f; };
            FakeMemory * f = RecyclerNewStruct(this, FakeMemory);
            this->RootAddRef(f);
        }

        Output::CaptureStart();
        Output::Print(L"-------------------------------------------------------------------------------------\n");
        Output::Print(L"Recycler (%p): %s Leaked Roots\n", this, header);
        Output::Print(L"-------------------------------------------------------------------------------------\n");
        RecyclerObjectGraphDumper::Param param = { 0 };
        param.dumpRootOnly = true;
        param.skipStack = true;
        if (!this->DumpObjectGraph(&param))
        {
            free(Output::CaptureEnd());
            Output::Print(L"ERROR: Out of memory generating leak report\n");
            return;
        }

        if (param.stats.markData.markCount != 0)
        {
            if (GetRecyclerFlagsTable().LeakStackTrace)
            {
                Output::Print(L"-------------------------------------------------------------------------------------\n");
                Output::Print(L"Pinned object stack traces");
                Output::Print(L"-------------------------------------------------------------------------------------\n");
                this->PrintPinnedObjectStackTraces();
            }

            Output::Print(L"-------------------------------------------------------------------------------------\n");
            Output::Print(L"Recycler Leaked Object: %d bytes (%d objects)\n",
                param.stats.markData.markBytes, param.stats.markData.markCount);

            wchar_t * buffer = Output::CaptureEnd();
            MemoryLeakCheck::AddLeakDump(buffer, param.stats.markData.markBytes, param.stats.markData.markCount);
#ifdef GENERATE_DUMP
            if (GetRecyclerFlagsTable().IsEnabled(Js::DumpOnLeakFlag))
            {
                Js::Throw::GenerateDump(GetRecyclerFlagsTable().DumpOnLeak);
            }
#endif
        }
        else
        {
            free(Output::CaptureEnd());
        }

    }
}


void
Recycler::CheckLeaksOnProcessDetach(wchar_t const * header)
{
    if (GetRecyclerFlagsTable().CheckMemoryLeak)
    {
        ReportOnProcessDetach([=]() { this->CheckLeaks(header); });
    }
}
#endif

#if defined(LEAK_REPORT) || defined(CHECK_MEMORY_LEAK)
template <class Fn>
void
Recycler::ReportOnProcessDetach(Fn fn)
{
#if DBG
    // Process detach can be done on any thread, just disable the thread check
    this->markContext.GetPageAllocator()->SetDisableThreadAccessCheck();
#endif

    if (this->IsConcurrentState())
    {
        this->AbortConcurrent(true);
    }

    if (this->CollectionInProgress())
    {
        Output::Print(L"WARNING: Thread terminated during GC.  Can't dump object graph\n");
        return;
    }
    // Don't mark external roots on another thread
    this->SetExternalRootMarker(NULL, NULL);
#if DBG
    this->ResetThreadId();
#endif
    fn();
}

void
Recycler::PrintPinnedObjectStackTraces()
{
    pinnedObjectMap.Map([this](void * object, PinRecord const& pinRecord)
        {
            this->DumpObjectDescription(object);
            Output::Print(L"\n");
            StackBackTraceNode::PrintAll(pinRecord.stackBackTraces);
        }
    );
}
#endif

#if defined(RECYCLER_DUMP_OBJECT_GRAPH) ||  defined(LEAK_REPORT) || defined(CHECK_MEMORY_LEAK)
void
Recycler::SetInDllCanUnloadNow()
{
    inDllCanUnloadNow = true;

    // Just clear out the root marker for the dump graph and report leaks
    SetExternalRootMarker(NULL, NULL);
}
void
Recycler::SetInDetachProcess()
{
    inDetachProcess = true;

    // Just clear out the root marker for the dump graph and report leaks
    SetExternalRootMarker(NULL, NULL);
}
#endif

#ifdef ENABLE_JS_ETW

ULONG Recycler::EventWriteFreeMemoryBlock(HeapBlock* heapBlock)
{
    if (EventEnabledJSCRIPT_RECYCLER_FREE_MEMORY_BLOCK())
    {
        char* memoryAddress = NULL;
        ULONG objectSize = 0;
        ULONG blockSize = 0;
        switch (heapBlock->GetHeapBlockType())
        {
        case HeapBlock::HeapBlockType::SmallFinalizableBlockType:
        case HeapBlock::HeapBlockType::SmallNormalBlockType:
#ifdef RECYCLER_WRITE_BARRIER
        case HeapBlock::HeapBlockType::SmallFinalizableBlockWithBarrierType:
        case HeapBlock::HeapBlockType::SmallNormalBlockWithBarrierType:
#endif
        case HeapBlock::HeapBlockType::SmallLeafBlockType:
            {
                SmallHeapBlock* smallHeapBlock = static_cast<SmallHeapBlock*>(heapBlock);
                memoryAddress = smallHeapBlock->GetAddress();
                blockSize = (ULONG)(smallHeapBlock->GetEndAddress() - memoryAddress);
                objectSize = smallHeapBlock->GetObjectSize();
            }
            break;
        case HeapBlock::HeapBlockType::MediumFinalizableBlockType:
        case HeapBlock::HeapBlockType::MediumNormalBlockType:
#ifdef RECYCLER_WRITE_BARRIER
        case HeapBlock::HeapBlockType::MediumFinalizableBlockWithBarrierType:
        case HeapBlock::HeapBlockType::MediumNormalBlockWithBarrierType:
#endif
        case HeapBlock::HeapBlockType::MediumLeafBlockType:
            {
                MediumHeapBlock* mediumHeapBlock = static_cast<MediumHeapBlock*>(heapBlock);
                memoryAddress = mediumHeapBlock->GetAddress();
                blockSize = (ULONG)(mediumHeapBlock->GetEndAddress() - memoryAddress);
                objectSize = mediumHeapBlock->GetObjectSize();
            }
        case HeapBlock::HeapBlockType::LargeBlockType:
                {
                LargeHeapBlock* largeHeapBlock = static_cast<LargeHeapBlock*>(heapBlock);
                memoryAddress = largeHeapBlock->GetBeginAddress();
                blockSize = (ULONG)(largeHeapBlock->GetEndAddress() - memoryAddress);
                objectSize = blockSize;
            }
            break;
         default:
             AssertMsg(FALSE, "invalid heapblock type");
       }

        EventWriteJSCRIPT_RECYCLER_FREE_MEMORY_BLOCK(memoryAddress, blockSize, objectSize);

    }
    return S_OK;
}

void Recycler::FlushFreeRecord()
{
    Assert(bulkFreeMemoryWrittenCount <= Recycler::BulkFreeMemoryCount);
    JS_ETW(EventWriteJSCRIPT_RECYCLER_FREE_MEMORY(bulkFreeMemoryWrittenCount, sizeof(Recycler::ETWFreeRecord), etwFreeRecords));
    bulkFreeMemoryWrittenCount = 0;
}

void Recycler::AppendFreeMemoryETWRecord(__in char *address, size_t size)
{
    Assert(bulkFreeMemoryWrittenCount < Recycler::BulkFreeMemoryCount);
    __analysis_assume(bulkFreeMemoryWrittenCount < Recycler::BulkFreeMemoryCount);
    etwFreeRecords[bulkFreeMemoryWrittenCount].memoryAddress = address;
    // TODO: change to size_t or uint64?
    etwFreeRecords[bulkFreeMemoryWrittenCount].objectSize = (uint)size;
    bulkFreeMemoryWrittenCount++;
    if (bulkFreeMemoryWrittenCount == Recycler::BulkFreeMemoryCount)
    {
        FlushFreeRecord();
        Assert(bulkFreeMemoryWrittenCount == 0);
    }
}

#endif

#ifdef PROFILE_EXEC
ArenaAllocator *
Recycler::AddBackgroundProfilerArena()
{
    return this->backgroundProfilerArena.PrependNode(&HeapAllocator::Instance,
        L"BgGCProfiler", &this->backgroundProfilerPageAllocator, Js::Throw::OutOfMemory);
}

void
Recycler::ReleaseBackgroundProfilerArena(ArenaAllocator * arena)
{
    this->backgroundProfilerArena.RemoveElement(&HeapAllocator::Instance, arena);
}

void
Recycler::SetProfiler(Js::Profiler * profiler, Js::Profiler * backgroundProfiler)
{
    this->profiler = profiler;
    this->backgroundProfiler = backgroundProfiler;
}
#endif

void Recycler::SetObjectBeforeCollectCallback(void* object, ObjectBeforeCollectCallback callback, void* callbackState)
{
    if (objectBeforeCollectCallbackState == ObjectBeforeCollectCallback_Shutdown)
    {
        return; // NOP at shutdown
    }

    if (objectBeforeCollectCallbackMap == nullptr)
    {
        if (callback == nullptr) return;
        objectBeforeCollectCallbackMap = HeapNew(ObjectBeforeCollectCallbackMap, &HeapAllocator::Instance);
    }

    // only allow 1 callback per object
    objectBeforeCollectCallbackMap->Item(object, ObjectBeforeCollectCallbackData(callback, callbackState));

    if (callback != nullptr && this->IsInObjectBeforeCollectCallback()) // revive
    {
        this->ScanMemory(&object, sizeof(object));
        this->ProcessMark(/*background*/false);
    }
}

bool Recycler::ProcessObjectBeforeCollectCallbacks(bool atShutdown/*= false*/)
{
    if (this->objectBeforeCollectCallbackMap == nullptr)
    {
        return false; // no callbacks
    }
    Assert(atShutdown || this->IsMarkState());

    Assert(!this->IsInObjectBeforeCollectCallback());
    AutoRestoreValue<ObjectBeforeCollectCallbackState> autoInObjectBeforeCollectCallback(&objectBeforeCollectCallbackState,
        atShutdown ? ObjectBeforeCollectCallback_Shutdown: ObjectBeforeCollectCallback_Normal);

    // The callbacks may register/unregister callbacks while we are enumerating the current map. To avoid
    // conflicting usage of the callback map, we swap it out. New registration will go to a new map.
    AutoAllocatorObjectPtr<ObjectBeforeCollectCallbackMap, HeapAllocator> oldCallbackMap(
        this->objectBeforeCollectCallbackMap, &HeapAllocator::Instance);
    this->objectBeforeCollectCallbackMap = nullptr;

    bool hasRemainingCallbacks = false;
    oldCallbackMap->MapAndRemoveIf([&](const ObjectBeforeCollectCallbackMap::EntryType& entry)
    {
        const ObjectBeforeCollectCallbackData& data = entry.Value();
        if (data.callback != nullptr)
        {
            void* object = entry.Key();
            if (atShutdown || !this->IsObjectMarked(object))
            {
                data.callback(object, data.callbackState);
            }
            else
            {
                hasRemainingCallbacks = true;
                return false; // Do not remove this entry, remaining callback for future
            }
        }

        return true; // Remove this entry
    });

    // Merge back remaining callbacks if any
    if (hasRemainingCallbacks)
    {
        if (this->objectBeforeCollectCallbackMap == nullptr)
        {
            this->objectBeforeCollectCallbackMap = oldCallbackMap.Detach();
        }
        else
        {
            if (oldCallbackMap->Count() > this->objectBeforeCollectCallbackMap->Count())
            {
                // Swap so that oldCallbackMap is the smaller one
                ObjectBeforeCollectCallbackMap* tmp = oldCallbackMap.Detach();
                *&oldCallbackMap = this->objectBeforeCollectCallbackMap;
                this->objectBeforeCollectCallbackMap = tmp;
            }

            oldCallbackMap->Map([&](void* object, const ObjectBeforeCollectCallbackData& data)
            {
                this->objectBeforeCollectCallbackMap->Item(object, data);
            });
        }
    }

    return true; // maybe called callbacks
}

void Recycler::ClearObjectBeforeCollectCallbacks()
{
    // This is called at shutting down. All objects will be gone. Invoke each registered callback if any.
    ProcessObjectBeforeCollectCallbacks(/*atShutdown*/true);
    Assert(objectBeforeCollectCallbackMap == nullptr);
}

#ifdef RECYCLER_TEST_SUPPORT
void Recycler::SetCheckFn(BOOL(*checkFn)(char* addr, size_t size))
{
    Assert(BinaryFeatureControl::RecyclerTest());
    this->EnsureNotCollecting();
    this->checkFn = checkFn;
}
#endif

void
Recycler::NotifyFree(__in char *address, size_t size)
{
    RecyclerVerboseTrace(GetRecyclerFlagsTable(), L"Sweeping object %p\n", address);

#ifdef RECYCLER_TEST_SUPPORT
    if (BinaryFeatureControl::RecyclerTest())
    {
        if (checkFn != NULL)
            checkFn(address, size);
    }
#endif

#ifdef ENABLE_JS_ETW
    if (EventEnabledJSCRIPT_RECYCLER_FREE_MEMORY())
    {
        AppendFreeMemoryETWRecord(address, (UINT)size);
    }
#endif

    RecyclerMemoryTracking::ReportFree(this, address, size);
    RECYCLER_PERF_COUNTER_DEC(LiveObject);
    RECYCLER_PERF_COUNTER_SUB(LiveObjectSize, size);
    RECYCLER_PERF_COUNTER_ADD(FreeObjectSize, size);

    if (HeapInfo::IsSmallBlockAllocation(HeapInfo::GetAlignedSizeNoCheck(size)))
    {
        RECYCLER_PERF_COUNTER_DEC(SmallHeapBlockLiveObject);
        RECYCLER_PERF_COUNTER_SUB(SmallHeapBlockLiveObjectSize, size);
        RECYCLER_PERF_COUNTER_ADD(SmallHeapBlockFreeObjectSize, size);
    }
    else
    {
        RECYCLER_PERF_COUNTER_DEC(LargeHeapBlockLiveObject);
        RECYCLER_PERF_COUNTER_SUB(LargeHeapBlockLiveObjectSize, size);
        RECYCLER_PERF_COUNTER_ADD(LargeHeapBlockFreeObjectSize, size);
    }

#ifdef RECYCLER_MEMORY_VERIFY
    if (this->VerifyEnabled())
    {
        VerifyCheckPad(address, size);
    }
#endif

#ifdef PROFILE_RECYCLER_ALLOC
    TrackFree(address, size);
#endif

#ifdef RECYCLER_STATS
    collectionStats.objectSweptCount++;
    collectionStats.objectSweptBytes += size;

    if (!isForceSweeping)
    {
        collectionStats.objectSweptFreeListCount++;
        collectionStats.objectSweptFreeListBytes += size;
    }
#endif
}

size_t
RecyclerHeapObjectInfo::GetSize() const
{
    Assert(m_heapBlock);

    size_t size;
#if LARGEHEAPBLOCK_ENCODING
    if (isUsingLargeHeapBlock)
    {
        size = m_largeHeapBlockHeader->objectSize;
    }
#else
    if (m_heapBlock->IsLargeHeapBlock())
    {
        size = ((LargeHeapBlock*)m_heapBlock)->GetObjectSize(m_address);
    }
#endif
    else
    {
        // All small heap block types have the same layout for the object size field.
        size = ((SmallHeapBlock*)m_heapBlock)->GetObjectSize();
    }

#ifdef RECYCLER_MEMORY_VERIFY
    if (m_recycler->VerifyEnabled())
    {
        size -= *(size_t *)(((char *)m_address) + size - sizeof(size_t));
    }
#endif
    return size;
}
