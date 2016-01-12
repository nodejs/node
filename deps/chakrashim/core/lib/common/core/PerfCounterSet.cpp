//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonCorePch.h"

#ifdef PERF_COUNTERS
// Initialization order
//  AB AutoSystemInfo
//  AD PerfCounter
//  AE PerfCounterSet
//  AM Output/Configuration
//  AN MemProtectHeap
//  AP DbgHelpSymbolManager
//  AQ CFGLogger
//  AR LeakReport
//  AS JavascriptDispatch/RecyclerObjectDumper
//  AT HeapAllocator/RecyclerHeuristic
//  AU RecyclerWriteBarrierManager
#pragma warning(disable:4075)       // initializers put in unrecognized initialization area on purpose
#pragma init_seg(".CRT$XCAE")

namespace PerfCounter
{
//===================================================================================
// PageAllocatorCounterSet
//===================================================================================
Counter&
PageAllocatorCounterSet::GetTotalReservedSizeCounter()
{
    return instance.GetCounter(PageAllocatorCounterSetDefinition::GetReservedCounterId(PageAllocatorType_Max));
}
Counter&
PageAllocatorCounterSet::GetTotalCommittedSizeCounter()
{
    return instance.GetCounter(PageAllocatorCounterSetDefinition::GetCommittedCounterId(PageAllocatorType_Max));
}
Counter&
PageAllocatorCounterSet::GetTotalUsedSizeCounter()
{
    return instance.GetCounter(PageAllocatorCounterSetDefinition::GetUsedCounterId(PageAllocatorType_Max));
}

DefaultCounterSetInstance<PageAllocatorCounterSetDefinition> PageAllocatorCounterSet::instance;
DefaultCounterSetInstance<BasicCounterSetDefinition> BasicCounterSet::instance;
DefaultCounterSetInstance<CodeCounterSetDefinition> CodeCounterSet::instance;
#ifdef HEAP_PERF_COUNTERS
DefaultCounterSetInstance<HeapCounterSetDefinition> HeapCounterSet::instance;
#endif
#ifdef RECYCLER_PERF_COUNTERS
DefaultCounterSetInstance<RecyclerCounterSetDefinition> RecyclerCounterSet::instance;
#endif
#ifdef PROFILE_RECYCLER_ALLOC
DefaultCounterSetInstance<RecyclerTrackerCounterSetDefinition> RecyclerTrackerCounterSet::instance;
type_info const * RecyclerTrackerCounterSet::CountIndexTypeInfoMap[RecyclerTrackerCounterSetDefinition::MaxCounter - RecyclerTrackerCounterSet::NumUnknownCounters];
type_info const * RecyclerTrackerCounterSet::SizeIndexTypeInfoMap[RecyclerTrackerCounterSetDefinition::MaxCounter - RecyclerTrackerCounterSet::NumUnknownCounters];
type_info const * RecyclerTrackerCounterSet::ArrayCountIndexTypeInfoMap[RecyclerTrackerCounterSetDefinition::MaxCounter - RecyclerTrackerCounterSet::NumUnknownCounters];
type_info const * RecyclerTrackerCounterSet::ArraySizeIndexTypeInfoMap[RecyclerTrackerCounterSetDefinition::MaxCounter - RecyclerTrackerCounterSet::NumUnknownCounters];
type_info const * RecyclerTrackerCounterSet::WeakRefIndexTypeInfoMap[RecyclerTrackerCounterSetDefinition::MaxCounter - RecyclerTrackerCounterSet::NumUnknownCounters];

RecyclerTrackerCounterSet::Map::Map(type_info const * type, bool isArray,uint counterIndex, uint sizeCounterIndex)
{
    Assert(counterIndex >= NumUnknownCounters && counterIndex < RecyclerTrackerCounterSetDefinition::MaxCounter);
    Assert(sizeCounterIndex >= NumUnknownCounters && sizeCounterIndex < RecyclerTrackerCounterSetDefinition::MaxCounter);

    __analysis_assume(counterIndex >= NumUnknownCounters && counterIndex < RecyclerTrackerCounterSetDefinition::MaxCounter);
    __analysis_assume(sizeCounterIndex >= NumUnknownCounters && sizeCounterIndex < RecyclerTrackerCounterSetDefinition::MaxCounter);
    if (isArray)
    {
        Assert(ArrayCountIndexTypeInfoMap[counterIndex - NumUnknownCounters] == nullptr);
        Assert(ArraySizeIndexTypeInfoMap[sizeCounterIndex - NumUnknownCounters] == nullptr);
        ArrayCountIndexTypeInfoMap[counterIndex - NumUnknownCounters] = type;
        ArraySizeIndexTypeInfoMap[sizeCounterIndex - NumUnknownCounters] = type;
    }
    else
    {
        Assert(CountIndexTypeInfoMap[counterIndex - NumUnknownCounters] == nullptr);
        Assert(SizeIndexTypeInfoMap[sizeCounterIndex - NumUnknownCounters] == nullptr);
        CountIndexTypeInfoMap[counterIndex - NumUnknownCounters] = type;
        SizeIndexTypeInfoMap[sizeCounterIndex - NumUnknownCounters] = type;
    }
}

RecyclerTrackerCounterSet::Map::Map(type_info const * type, uint weakRefCounterIndex)
{
    __analysis_assume(weakRefCounterIndex >= NumUnknownCounters && weakRefCounterIndex < RecyclerTrackerCounterSetDefinition::MaxCounter);
    Assert(weakRefCounterIndex >= NumUnknownCounters && weakRefCounterIndex < RecyclerTrackerCounterSetDefinition::MaxCounter);
    Assert(WeakRefIndexTypeInfoMap[weakRefCounterIndex - NumUnknownCounters] == nullptr);
    WeakRefIndexTypeInfoMap[weakRefCounterIndex - NumUnknownCounters] = type;
}

Counter&
RecyclerTrackerCounterSet::GetPerfCounter(type_info const * typeinfo, bool isArray)
{
    Counter& unknownCounter = isArray? GetUnknownArrayCounter() : GetUnknownCounter();
    type_info const **counters = isArray? ArrayCountIndexTypeInfoMap : CountIndexTypeInfoMap;

    for (uint i = 0; i < RecyclerTrackerCounterSetDefinition::MaxCounter - NumUnknownCounters; i++)
    {
        if (typeinfo == counters[i])
        {
            return instance.GetCounter(i + NumUnknownCounters);
        }
    }
    return unknownCounter;
}

Counter&
RecyclerTrackerCounterSet::GetPerfSizeCounter(type_info const * typeinfo, bool isArray)
{
    Counter& unknownCounter = isArray? GetUnknownArraySizeCounter() : GetUnknownSizeCounter();
    type_info const **counters = isArray? ArraySizeIndexTypeInfoMap : SizeIndexTypeInfoMap;

    for (uint i = 0; i < RecyclerTrackerCounterSetDefinition::MaxCounter - NumUnknownCounters; i++)
    {
        if (typeinfo == counters[i])
        {
            return instance.GetCounter(i + NumUnknownCounters);
        }
    }
    return unknownCounter;
}

Counter&
RecyclerTrackerCounterSet::GetWeakRefPerfCounter(type_info const * typeinfo)
{
    Counter& unknownCounter = GetUnknownWeakRefCounter();

    for (uint i = 0; i < RecyclerTrackerCounterSetDefinition::MaxCounter - NumUnknownCounters; i++)
    {
        if (typeinfo == WeakRefIndexTypeInfoMap[i])
        {
            return instance.GetCounter(i + NumUnknownCounters);
        }
    }
    return unknownCounter;
}

#endif
};
#endif

DEFINE_RECYCLER_TRACKER_ARRAY_PERF_COUNTER(wchar_t);
