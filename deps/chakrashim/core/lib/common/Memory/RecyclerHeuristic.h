//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Memory
{
#define KILOBYTES * 1024
#define MEGABYTES * 1024 KILOBYTES
#define MEGABYTES_OF_PAGES * 1024 * 1024 / AutoSystemInfo::PageSize;

class RecyclerHeuristic
{
private:
    RecyclerHeuristic();

public:
    static RecyclerHeuristic Instance;

    // Heuristics that depend on hardware or environment (not constant).
    uint   MaxUncollectedAllocBytes;
    size_t UncollectedAllocBytesConcurrentPriorityBoost;
    uint   MaxPartialUncollectedNewPageCount;
    uint   MaxUncollectedAllocBytesOnExit;

    // If we are getting close to the full GC limit, let's just get out of partial GC mode.
    uint   MaxUncollectedAllocBytesPartialCollect;

    // Defines the PageSegment size for recycler small block page allocator.
    uint DefaultMaxAllocPageCount;

    // Used for RecyclerPageAllocator and determines how many free pages need to be there to trigger decommit
    // (most cases, not all).
    uint   DefaultMaxFreePageCount;

    // Constant heuristic that may be changed by switches
    static uint UncollectedAllocBytesCollection();
#ifdef CONCURRENT_GC_ENABLED
    static uint MaxBackgroundFinishMarkCount(Js::ConfigFlagsTable&);
    static DWORD BackgroundFinishMarkWaitTime(bool, Js::ConfigFlagsTable&);
    static size_t MinBackgroundRepeatMarkRescanBytes(Js::ConfigFlagsTable&);
    static DWORD FinishConcurrentCollectWaitTime(Js::ConfigFlagsTable&);
    static DWORD PriorityBoostTimeout(Js::ConfigFlagsTable&);
#endif
#if defined(PARTIAL_GC_ENABLED) && defined(CONCURRENT_GC_ENABLED)
    static bool PartialConcurrentNextCollection(double ratio, Js::ConfigFlagsTable& flags);
#endif

    // Constant heuristics
    static const uint IdleUncollectedAllocBytesCollection = 1 MEGABYTES;
    static const uint TickCountCollection = 1200;                                           // 1.2 second
    static const uint TickCountFinishCollection = 45;                                       // 45 milliseconds
    static const uint TickCountDoDispose = 300;                                             // Allow for 300 milliseconds of script before attempting dispose
                                                                                            // This heuristic is currently used for dispose on stack probes
    void ConfigureBaseFactor(uint baseFactor);

#ifdef CONCURRENT_GC_ENABLED
    static const uint MaxBackgroundRepeatMarkCount = 2;

    // If we rescan at least 128 pages in the first background repeat mark,
    // then trigger a second repeat mark pass.
    static const uint BackgroundSecondRepeatMarkThreshold = 128;
#endif
private:

#ifndef RECYCLER_HEURISTIC_VERSION
#define RECYCLER_HEURISTIC_VERSION 11
#endif
    static const uint DefaultUncollectedAllocBytesCollection = 1 MEGABYTES;

#ifdef CONCURRENT_GC_ENABLED
    static const uint TickCountConcurrentPriorityBoost = 5000;                              // 5 second
    static const DWORD DefaultFinishConcurrentCollectWaitTime = 1000;                       // 1 second
    static const uint DefaultMaxBackgroundFinishMarkCount = 1;
    static const DWORD DefaultBackgroundFinishMarkWaitTime = 15; // ms
    static const size_t DefaultMinBackgroundRepeatMarkRescanBytes = 1 MEGABYTES;
#endif
};
}
