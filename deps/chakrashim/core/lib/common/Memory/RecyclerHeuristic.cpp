//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonMemoryPch.h"

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
#pragma init_seg(".CRT$XCAT")

RecyclerHeuristic RecyclerHeuristic::Instance;

// static
RecyclerHeuristic::RecyclerHeuristic()
{
    ::MEMORYSTATUSEX mem;
    mem.dwLength = sizeof(mem);
    BOOL isSuccess = ::GlobalMemoryStatusEx(&mem);
    Assert(isSuccess);

    DWORDLONG physicalMemoryBytes = mem.ullTotalPhys;
    uint baseFactor;

    if (isSuccess && AutoSystemInfo::IsLowMemoryDevice() && physicalMemoryBytes <= 512 MEGABYTES)
    {
        // Low-end Apollo (512MB RAM) scenario.
        // Note that what's specific about Apollo is that IE runs in physical memory,
        //      that's one reason to distinguish 512MB Apollo from 512MB desktop.
        baseFactor = 16;
        this->DefaultMaxFreePageCount = 16 MEGABYTES_OF_PAGES;
        this->DefaultMaxAllocPageCount = 32;
    }
    else if (isSuccess && physicalMemoryBytes <= 1024 MEGABYTES)
    {
        // Tablet/slate/high-end Apollo scenario, including 512MB non-Apollo.
        baseFactor = 64;
        this->DefaultMaxFreePageCount = 64 MEGABYTES_OF_PAGES;
        this->DefaultMaxAllocPageCount = 64;
    }
    else
    {
        // Regular desktop scenario.
        baseFactor = 192;
        this->DefaultMaxFreePageCount = 512 MEGABYTES_OF_PAGES;
        this->DefaultMaxAllocPageCount = 256;
    }

    this->ConfigureBaseFactor(baseFactor);
}

void
RecyclerHeuristic::ConfigureBaseFactor(uint baseFactor)
{
    this->MaxUncollectedAllocBytes = baseFactor MEGABYTES;
    this->UncollectedAllocBytesConcurrentPriorityBoost = baseFactor MEGABYTES;
    this->MaxPartialUncollectedNewPageCount = baseFactor MEGABYTES_OF_PAGES;
    this->MaxUncollectedAllocBytesOnExit = (baseFactor / 2) MEGABYTES;

    this->MaxUncollectedAllocBytesPartialCollect = this->MaxUncollectedAllocBytes - 1 MEGABYTES;
}

uint
RecyclerHeuristic::UncollectedAllocBytesCollection()
{
    return DefaultUncollectedAllocBytesCollection;
}

#ifdef CONCURRENT_GC_ENABLED
uint
RecyclerHeuristic::MaxBackgroundFinishMarkCount(Js::ConfigFlagsTable& flags)
{
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    if (flags.IsEnabled(Js::MaxBackgroundFinishMarkCountFlag))
    {
        return flags.MaxBackgroundFinishMarkCount;
    }
#endif
    return DefaultMaxBackgroundFinishMarkCount;
}

DWORD
RecyclerHeuristic::BackgroundFinishMarkWaitTime(bool backgroundFinishMarkWaitTime, Js::ConfigFlagsTable& flags)
{
    if (RECYCLER_HEURISTIC_VERSION == 10)
    {
        backgroundFinishMarkWaitTime = backgroundFinishMarkWaitTime && CUSTOM_PHASE_ON1(flags, Js::BackgroundFinishMarkPhase);
    }
    else
    {
        backgroundFinishMarkWaitTime = backgroundFinishMarkWaitTime && !CUSTOM_PHASE_OFF1(flags, Js::BackgroundFinishMarkPhase);
    }
    if (!backgroundFinishMarkWaitTime && !CUSTOM_PHASE_FORCE1(flags, Js::BackgroundFinishMarkPhase))
    {
        return INFINITE;
    }
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    if (flags.IsEnabled(Js::BackgroundFinishMarkWaitTimeFlag))
    {
        return flags.BackgroundFinishMarkWaitTime;
    }
#endif
    if (CUSTOM_PHASE_FORCE1(flags, Js::BackgroundFinishMarkPhase))
    {
        return INFINITE;
    }
    return DefaultBackgroundFinishMarkWaitTime;
}

size_t
RecyclerHeuristic::MinBackgroundRepeatMarkRescanBytes(Js::ConfigFlagsTable& flags)
{
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    if (flags.IsEnabled(Js::MinBackgroundRepeatMarkRescanBytesFlag))
    {
        return flags.MinBackgroundRepeatMarkRescanBytes;
    }
#endif
    return DefaultMinBackgroundRepeatMarkRescanBytes;
}

DWORD
RecyclerHeuristic::FinishConcurrentCollectWaitTime(Js::ConfigFlagsTable& flags)
{
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    if (flags.IsEnabled(Js::RecyclerThreadCollectTimeoutFlag))
    {
        return flags.RecyclerThreadCollectTimeout;
    }
#endif
    return DefaultFinishConcurrentCollectWaitTime;
}


DWORD
RecyclerHeuristic::PriorityBoostTimeout(Js::ConfigFlagsTable& flags)
{
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    if (flags.IsEnabled(Js::RecyclerPriorityBoostTimeoutFlag))
    {
        return flags.RecyclerPriorityBoostTimeout;
    }
#endif
    return TickCountConcurrentPriorityBoost;
}
#endif

#if defined(PARTIAL_GC_ENABLED) && defined(CONCURRENT_GC_ENABLED)
bool
RecyclerHeuristic::PartialConcurrentNextCollection(double ratio, Js::ConfigFlagsTable& flags)
{
    if (CUSTOM_PHASE_FORCE1(flags, Js::ConcurrentPartialCollectPhase))
    {
        return true;
    }

    if (RECYCLER_HEURISTIC_VERSION == 10)
    {
        // Default off for version == 10
        if (!CUSTOM_PHASE_ON1(flags, Js::ConcurrentPartialCollectPhase))
        {
            return false;
        }
    }
    else
    {
        // Default on
        if (CUSTOM_PHASE_OFF1(flags, Js::ConcurrentPartialCollectPhase))
        {
            return false;
        }
    }
    return ratio >= 0.5;
}
#endif
