//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
__inline
bool MarkContext::AddMarkedObject(void * objectAddress, size_t objectSize)
{
    Assert(objectAddress != nullptr);
    Assert(objectSize > 0);
    Assert(objectSize % sizeof(void *) == 0);

    FAULTINJECT_MEMORY_MARK_NOTHROW(L"AddMarkedObject", objectSize);

#if DBG_DUMP
    if (recycler->forceTraceMark || recycler->GetRecyclerFlagsTable().Trace.IsEnabled(Js::MarkPhase))
    {
        Output::Print(L" %p", objectAddress);
    }
#endif

    RECYCLER_STATS_INTERLOCKED_INC(recycler, scanCount);

    MarkCandidate markCandidate;
    markCandidate.obj = (void **) objectAddress;
    markCandidate.byteCount = objectSize;
    return markStack.Push(markCandidate);
}

__inline
bool MarkContext::AddTrackedObject(FinalizableObject * obj)
{
    Assert(obj != nullptr);
    Assert(recycler->DoQueueTrackedObject());
#ifdef PARTIAL_GC_ENABLED
    Assert(!recycler->inPartialCollectMode);
#endif

    FAULTINJECT_MEMORY_MARK_NOTHROW(L"AddTrackedObject", 0);

    return trackStack.Push(obj);
}

template <bool parallel, bool interior>
__inline
void MarkContext::ScanMemory(void ** obj, size_t byteCount)
{
    Assert(byteCount != 0);
    Assert(byteCount % sizeof(void *) == 0);

    void ** objEnd = obj + (byteCount / sizeof(void *));
    void * parentObject = (void*)obj;

#if DBG_DUMP
    if (recycler->forceTraceMark || recycler->GetRecyclerFlagsTable().Trace.IsEnabled(Js::MarkPhase))
    {
        Output::Print(L"Scanning %p(%8d): ", obj, byteCount);
    }
#endif

    do
    {
        // We need to ensure that the compiler does not reintroduce reads to the object after inlining.
        // This could cause the value to change after the marking checks (e.g., the null/low address check).
        // Intrinsics avoid the expensive memory barrier on ARM (due to /volatile:ms).
#if defined(_M_ARM64)
        void * candidate = reinterpret_cast<void *>(__iso_volatile_load64(reinterpret_cast<volatile __int64 *>(obj)));
#elif defined(_M_ARM)
        void * candidate = reinterpret_cast<void *>(__iso_volatile_load32(reinterpret_cast<volatile __int32 *>(obj)));
#else
        void * candidate = *(static_cast<void * volatile *>(obj));
#endif
        Mark<parallel, interior>(candidate, parentObject);
        obj++;
    } while (obj != objEnd);

#if DBG_DUMP
    if (recycler->forceTraceMark || recycler->GetRecyclerFlagsTable().Trace.IsEnabled(Js::MarkPhase))
    {
        Output::Print(L"\n");
        Output::Flush();
    }
#endif
}


template <bool parallel, bool interior>
__inline
void MarkContext::ScanObject(void ** obj, size_t byteCount)
{
    BEGIN_DUMP_OBJECT(recycler, obj);

    ScanMemory<parallel, interior>(obj, byteCount);

    END_DUMP_OBJECT(recycler);
}


template <bool parallel, bool interior>
__inline
void MarkContext::Mark(void * candidate, void * parentReference)
{
    // We should never reach here while we are processing Rescan.
    // Otherwise our rescanState could be out of sync with mark state.
    Assert(!recycler->isProcessingRescan);

    if ((size_t)candidate < 0x10000)
    {
        RECYCLER_STATS_INTERLOCKED_INC(recycler, tryMarkNullCount);
        return;
    }

    if (interior)
    {
        Assert(recycler->enableScanInteriorPointers
            || (!recycler->IsConcurrentState() && recycler->collectionState != CollectionStateParallelMark));
        recycler->heapBlockMap.MarkInterior<parallel>(candidate, this);
        return;
    }

    if (!HeapInfo::IsAlignedAddress(candidate))
    {
        RECYCLER_STATS_INTERLOCKED_INC(recycler, tryMarkUnalignedCount);
        return;
    }

    recycler->heapBlockMap.Mark<parallel>(candidate, this);

#ifdef RECYCLER_MARK_TRACK
    this->OnObjectMarked(candidate, parentReference);
#endif
}

__inline
void MarkContext::MarkTrackedObject(FinalizableObject * trackedObject)
{
    Assert(!recycler->queueTrackedObject);
#ifdef PARTIAL_GC_ENABLED
    Assert(!recycler->inPartialCollectMode);
#endif
    Assert(!recycler->IsConcurrentExecutingState());
    Assert(!(recycler->collectionState == CollectionStateParallelMark));

    // Mark is not expected to throw.
    BEGIN_NO_EXCEPTION
    {
        trackedObject->Mark(recycler);
    }
    END_NO_EXCEPTION
}

template <bool parallel, bool interior>
__inline
void MarkContext::ProcessMark()
{
#ifdef RECYCLER_STRESS
    if (recycler->GetRecyclerFlagsTable().RecyclerInduceFalsePositives)
    {
        // InduceFalsePositives logic doesn't support parallel marking
        if (!parallel)
        {
            recycler->heapBlockMap.InduceFalsePositives(recycler);
        }
    }
#endif

#if defined(_M_IX86) || defined(_M_X64)
    MarkCandidate current, next;

    while (markStack.Pop(&current))
    {
        // Process entries and prefetch as we go.
        while (markStack.Pop(&next))
        {
            // Prefetch the next entry so it's ready when we need it.
            _mm_prefetch((char *)next.obj, _MM_HINT_T0);

            // Process the previously retrieved entry.
            ScanObject<parallel, interior>(current.obj, current.byteCount);

            current = next;
        }

        // The stack is empty, but we still have a previously retrieved entry; process it now.
        ScanObject<parallel, interior>(current.obj, current.byteCount);

        // Processing that entry may have generated more entries in the mark stack, so continue the loop.
    }
#else
    // _mm_prefetch intrinsic is specific to Intel platforms.
    // CONSIDER: There does seem to be a compiler intrinsic for prefetch on ARM,
    // however, the information on this is scarce, so for now just don't do prefetch on ARM.
    MarkCandidate current;

    while (markStack.Pop(&current))
    {
        ScanObject<parallel, interior>(current.obj, current.byteCount);
    }
#endif

    Assert(markStack.IsEmpty());
}

