//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonCorePch.h"

#ifdef PROFILE_MEM
#include "DataStructures\QuickSort.h"
#include "Memory\AutoPtr.h"
#include "core\ProfileMemory.h"

__declspec(thread) MemoryProfiler * MemoryProfiler::Instance = nullptr;

CriticalSection MemoryProfiler::s_cs;
AutoPtr<MemoryProfiler, NoCheckHeapAllocator> MemoryProfiler::profilers(nullptr);

MemoryProfiler::MemoryProfiler() :
    pageAllocator(nullptr, Js::Configuration::Global.flags, PageAllocatorType_Max, 0, false, nullptr),
    alloc(L"MemoryProfiler", &pageAllocator, Js::Throw::OutOfMemory),
    arenaDataMap(&alloc, 10)
{
    threadId = ::GetCurrentThreadId();
    memset(&pageMemoryData, 0, sizeof(pageMemoryData));
    memset(&recyclerMemoryData, 0, sizeof(recyclerMemoryData));
}

MemoryProfiler::~MemoryProfiler()
{
#if DBG
    pageAllocator.SetDisableThreadAccessCheck();
#endif
    if (next != nullptr)
    {
        NoCheckHeapDelete(next);
    }
}

MemoryProfiler *
MemoryProfiler::EnsureMemoryProfiler()
{
    MemoryProfiler * memoryProfiler = MemoryProfiler::Instance;

    if (memoryProfiler == nullptr)
    {
        memoryProfiler = NoCheckHeapNew(MemoryProfiler);

        {
            AutoCriticalSection autocs(&s_cs);
            memoryProfiler->next = MemoryProfiler::profilers.Detach();
            MemoryProfiler::profilers = memoryProfiler;
        }

        MemoryProfiler::Instance = memoryProfiler;
    }
    return memoryProfiler;
}

PageMemoryData *
MemoryProfiler::GetPageMemoryData(PageAllocatorType type)
{
    if (!Js::Configuration::Global.flags.IsEnabled(Js::TraceMemoryFlag))
    {
        return nullptr;
    }
    if (type == PageAllocatorType_Max)
    {
        return nullptr;
    }
    MemoryProfiler * memoryProfiler = EnsureMemoryProfiler();
    return &memoryProfiler->pageMemoryData[type];
}

RecyclerMemoryData *
MemoryProfiler::GetRecyclerMemoryData()
{
    if (!Js::Configuration::Global.flags.IsEnabled(Js::TraceMemoryFlag))
    {
        return nullptr;
    }
    MemoryProfiler * memoryProfiler = EnsureMemoryProfiler();
    return &memoryProfiler->recyclerMemoryData;
}

ArenaMemoryData *
MemoryProfiler::Begin(LPCWSTR name)
{
    if (!Js::Configuration::Global.flags.IsEnabled(Js::TraceMemoryFlag))
    {
        return nullptr;
    }
    Assert(name != nullptr);
    if (wcscmp(name, L"MemoryProfiler") == 0)
    {
        // Don't profile memory profiler itself
        return nullptr;
    }

    // This is debug only code, we don't care if we catch the right exception
    AUTO_NESTED_HANDLED_EXCEPTION_TYPE(ExceptionType_DisableCheck);
    MemoryProfiler * memoryProfiler = EnsureMemoryProfiler();
    ArenaMemoryDataSummary * arenaTotalMemoryData;
    if (!memoryProfiler->arenaDataMap.TryGetValue((LPWSTR)name, &arenaTotalMemoryData))
    {
        arenaTotalMemoryData = AnewStructZ(&memoryProfiler->alloc, ArenaMemoryDataSummary);
        memoryProfiler->arenaDataMap.Add((LPWSTR)name, arenaTotalMemoryData);
    }
    arenaTotalMemoryData->arenaCount++;

    ArenaMemoryData * memoryData = AnewStructZ(&memoryProfiler->alloc, ArenaMemoryData);
    if (arenaTotalMemoryData->data == nullptr)
    {
        arenaTotalMemoryData->data = memoryData;
    }
    else
    {
        memoryData->next = arenaTotalMemoryData->data;
        arenaTotalMemoryData->data->prev = memoryData;
        arenaTotalMemoryData->data = memoryData;
    }
    memoryData->profiler = memoryProfiler;
    return memoryData;
}

void
MemoryProfiler::Reset(LPCWSTR name, ArenaMemoryData * memoryData)
{
    MemoryProfiler * memoryProfiler = memoryData->profiler;
    ArenaMemoryDataSummary * arenaMemoryDataSummary;
    bool hasItem = memoryProfiler->arenaDataMap.TryGetValue((LPWSTR)name, &arenaMemoryDataSummary);
    Assert(hasItem);


    AccumulateData(arenaMemoryDataSummary, memoryData, true);
    memoryData->allocatedBytes = 0;
    memoryData->alignmentBytes = 0;
    memoryData->requestBytes = 0;
    memoryData->requestCount = 0;
    memoryData->reuseBytes = 0;
    memoryData->reuseCount = 0;
    memoryData->freelistBytes = 0;
    memoryData->freelistCount = 0;
    memoryData->resetCount++;
}

void
MemoryProfiler::End(LPCWSTR name, ArenaMemoryData * memoryData)
{
    MemoryProfiler * memoryProfiler = memoryData->profiler;
    ArenaMemoryDataSummary * arenaMemoryDataSummary;
    bool hasItem = memoryProfiler->arenaDataMap.TryGetValue((LPWSTR)name, &arenaMemoryDataSummary);
    Assert(hasItem);

    if (memoryData->next != nullptr)
    {
        memoryData->next->prev = memoryData->prev;
    }

    if (memoryData->prev != nullptr)
    {
        memoryData->prev->next = memoryData->next;
    }
    else
    {
        Assert(arenaMemoryDataSummary->data == memoryData);
        arenaMemoryDataSummary->data = memoryData->next;
    }
    AccumulateData(arenaMemoryDataSummary, memoryData);
}

void
MemoryProfiler::AccumulateData(ArenaMemoryDataSummary * arenaMemoryDataSummary,  ArenaMemoryData * memoryData, bool reset)
{
    arenaMemoryDataSummary->total.alignmentBytes += memoryData->alignmentBytes;
    arenaMemoryDataSummary->total.allocatedBytes += memoryData->allocatedBytes;
    arenaMemoryDataSummary->total.freelistBytes += memoryData->freelistBytes;
    arenaMemoryDataSummary->total.freelistCount += memoryData->freelistCount;
    arenaMemoryDataSummary->total.requestBytes += memoryData->requestBytes;
    arenaMemoryDataSummary->total.requestCount += memoryData->requestCount;
    arenaMemoryDataSummary->total.reuseCount += memoryData->reuseCount;
    arenaMemoryDataSummary->total.reuseBytes += memoryData->reuseBytes;
    if (!reset)
    {
        arenaMemoryDataSummary->total.resetCount += memoryData->resetCount;
    }

    arenaMemoryDataSummary->max.alignmentBytes = max(arenaMemoryDataSummary->max.alignmentBytes, memoryData->alignmentBytes);
    arenaMemoryDataSummary->max.allocatedBytes = max(arenaMemoryDataSummary->max.allocatedBytes, memoryData->allocatedBytes);
    arenaMemoryDataSummary->max.freelistBytes = max(arenaMemoryDataSummary->max.freelistBytes, memoryData->freelistBytes);
    arenaMemoryDataSummary->max.freelistCount = max(arenaMemoryDataSummary->max.freelistCount, memoryData->freelistCount);
    arenaMemoryDataSummary->max.requestBytes = max(arenaMemoryDataSummary->max.requestBytes, memoryData->requestBytes);
    arenaMemoryDataSummary->max.requestCount = max(arenaMemoryDataSummary->max.requestCount, memoryData->requestCount);
    arenaMemoryDataSummary->max.reuseCount = max(arenaMemoryDataSummary->max.reuseCount, memoryData->reuseCount);
    arenaMemoryDataSummary->max.reuseBytes = max(arenaMemoryDataSummary->max.reuseBytes, memoryData->reuseBytes);
    if (!reset)
    {
        arenaMemoryDataSummary->max.resetCount = max(arenaMemoryDataSummary->max.resetCount, memoryData->resetCount);
    }
}

void
MemoryProfiler::PrintPageMemoryData(PageMemoryData const& pageMemoryData, char const * title)
{
    if (pageMemoryData.allocSegmentCount != 0)
    {
        Output::Print(L"%-10S:%9d %10d | %4d %10d | %4d %10d | %10d | %10d | %10d | %10d\n", title,
            pageMemoryData.currentCommittedPageCount * AutoSystemInfo::PageSize, pageMemoryData.peakCommittedPageCount * AutoSystemInfo::PageSize,
            pageMemoryData.allocSegmentCount, pageMemoryData.allocSegmentBytes,
            pageMemoryData.releaseSegmentCount, pageMemoryData.releaseSegmentBytes,
            pageMemoryData.allocPageCount * AutoSystemInfo::PageSize,
            pageMemoryData.releasePageCount * AutoSystemInfo::PageSize,
            pageMemoryData.decommitPageCount * AutoSystemInfo::PageSize,
            pageMemoryData.recommitPageCount * AutoSystemInfo::PageSize);
    }
}

void
MemoryProfiler::Print()
{
    Output::Print(L"-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=\n");
    Output::Print(L"Allocation for thread 0x%08X\n", threadId);
    Output::Print(L"-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=\n");

    bool hasData = false;
    for (int i = 0; i < PageAllocatorType_Max; i++)
    {
        if (pageMemoryData[i].allocSegmentCount != 0)
        {
            hasData = true;
            break;
        }
    }
    if (hasData)
    {
        Output::Print(L"%-10s:%-20s | %-15s | %-15s | %10s | %10s | %11s | %11s\n", L"", L"         Current", L"   Alloc Seg", L"    Free Seg",
            L"Request", L"Released", L"Decommitted", L"Recommitted");
        Output::Print(L"%-10s:%9s %10s | %4s %10s | %4s %10s | %10s | %10s | %10s | %10s\n", L"", L"Bytes", L"Peak", L"#", L"Bytes", L"#", L"Bytes",
             L"Bytes", L"Bytes", L"Bytes",  L"Bytes");
        Output::Print(L"------------------------------------------------------------------------------------------------------------------\n");

#define PAGEALLOCATOR_PRINT(i) PrintPageMemoryData(pageMemoryData[PageAllocatorType_ ## i], STRINGIZE(i));
        PAGE_ALLOCATOR_TYPE(PAGEALLOCATOR_PRINT);
        Output::Print(L"------------------------------------------------------------------------------------------------------------------\n");
    }

    if (recyclerMemoryData.requestCount != 0)
    {
        Output::Print(L"%-10s:%7s %10s %10s %10s\n",
                L"Recycler",
                L"#Alloc",
                L"AllocBytes",
                L"ReqBytes",
                L"AlignByte");
        Output::Print(L"--------------------------------------------------------------------------------------------------------\n");
        Output::Print(L"%-10s:%7d %10d %10d %10d\n",
            L"",
            recyclerMemoryData.requestCount,
            recyclerMemoryData.requestBytes + recyclerMemoryData.alignmentBytes,
            recyclerMemoryData.requestBytes,
            recyclerMemoryData.alignmentBytes);
        Output::Print(L"--------------------------------------------------------------------------------------------------------\n");
    }

    if (Js::Configuration::Global.flags.TraceMemory.IsEnabled(Js::AllPhase))
    {
        PrintArena(false);
    }
    PrintArena(true);
}

void
MemoryProfiler::PrintArenaHeader(wchar_t const * title)
{
    Output::Print(L"--------------------------------------------------------------------------------------------------------\n");

    Output::Print(L"%-20s:%7s %9s %9s %9s %6s %9s %6s %9s %5s | %5s\n",
            title,
            L"#Alloc",
            L"AllocByte",
            L"ReqBytes",
            L"AlignByte",
            L"#Reuse",
            L"ReuseByte",
            L"#Free",
            L"FreeBytes",
            L"Reset",
            L"Count");

    Output::Print(L"--------------------------------------------------------------------------------------------------------\n");
}

int MemoryProfiler::CreateArenaUsageSummary(ArenaAllocator * alloc, bool liveOnly,
    _Outptr_result_buffer_(return) LPWSTR ** name_ptr, _Outptr_result_buffer_(return) ArenaMemoryDataSummary *** summaries_ptr)
{
    Assert(alloc);

    LPWSTR *& name = *name_ptr;
    ArenaMemoryDataSummary **& summaries = *summaries_ptr;

    int count = arenaDataMap.Count();
    name = AnewArray(alloc, LPWSTR, count);
    int i = 0;
    arenaDataMap.Map([&i, name](LPWSTR key, ArenaMemoryDataSummary*)
    {
        name[i++] = key;
    });
    JsUtil::QuickSort<LPWSTR, DefaultComparer<LPWSTR>>::Sort(name, name + (count - 1));

    summaries = AnewArray(alloc, ArenaMemoryDataSummary *, count);

    for (int i = 0; i < count; i++)
    {
        ArenaMemoryDataSummary * summary = arenaDataMap.Item(name[i]);
        ArenaMemoryData * data = summary->data;

        ArenaMemoryDataSummary * localSummary;
        if (liveOnly)
        {
            if (data == nullptr)
            {
                summaries[i] = nullptr;
                continue;
            }
            localSummary = AnewStructZ(alloc, ArenaMemoryDataSummary);
        }
        else
        {
            localSummary = Anew(alloc, ArenaMemoryDataSummary, *summary);
        }

        while (data != nullptr)
        {
            localSummary->outstandingCount++;
            AccumulateData(localSummary, data);
            data = data->next;
        }

        if (liveOnly)
        {
            localSummary->arenaCount = localSummary->outstandingCount;
        }
        summaries[i] = localSummary;
    }

    return count;
}

void
MemoryProfiler::PrintArena(bool liveOnly)
{
    WithArenaUsageSummary(liveOnly, [&] (int count, _In_reads_(count) LPWSTR * name, _In_reads_(count) ArenaMemoryDataSummary ** summaries)
    {
        int i = 0;

        if (liveOnly)
        {
            Output::Print(L"Arena usage summary (live)\n");
        }
        else
        {
            Output::Print(L"Arena usage summary (all)\n");
        }

        bool header = false;

        for (i = 0; i < count; i++)
        {
            ArenaMemoryDataSummary * data = summaries[i];
            if (data == nullptr)
            {
                continue;
            }
            if (!header)
            {
                header = true;
                PrintArenaHeader(L"Arena Size");
            }

            Output::Print(L"%-20s %7d %9d %9d %9d %6d %9d %6d %9d %5d | %5d\n",
                name[i],
                data->total.requestCount,
                data->total.allocatedBytes,
                data->total.requestBytes,
                data->total.alignmentBytes,
                data->total.reuseCount,
                data->total.reuseBytes,
                data->total.freelistCount,
                data->total.freelistBytes,
                data->total.resetCount,
                data->arenaCount);
        }

        header = false;

        for (i = 0; i < count; i++)
        {
            ArenaMemoryDataSummary * data = summaries[i];
            if (data == nullptr)
            {
                continue;
            }
            if (!header)
            {
                header = true;
                PrintArenaHeader(L"Arena Max");
            }
            Output::Print(L"%-20s %7d %9d %9d %9d %6d %9d %6d %9d %5d | %5d\n",
                name[i],
                data->max.requestCount,
                data->max.allocatedBytes,
                data->max.requestBytes,
                data->max.alignmentBytes,
                data->max.reuseCount,
                data->max.reuseBytes,
                data->max.freelistCount, data->max.freelistBytes,
                data->max.resetCount, data->outstandingCount);
        }

        header = false;
        for (i = 0; i < count; i++)
        {
            ArenaMemoryDataSummary * data = summaries[i];
            if (data == nullptr)
            {
                continue;
            }
            if (!header)
            {
                header = true;
                PrintArenaHeader(L"Arena Average");
            }
            Output::Print(L"%-20s %7d %9d %9d %9d %6d %9d %6d %9d %5d\n", name[i],
                data->total.requestCount / data->arenaCount,
                data->total.allocatedBytes / data->arenaCount,
                data->total.requestBytes / data->arenaCount,
                data->total.alignmentBytes / data->arenaCount,
                data->total.reuseCount / data->arenaCount,
                data->total.reuseBytes / data->arenaCount,
                data->total.freelistCount / data->arenaCount,
                data->total.freelistBytes / data->arenaCount,
                data->total.resetCount / data->arenaCount);
        }

        Output::Print(L"--------------------------------------------------------------------------------------------------------\n");
    });
}

void
MemoryProfiler::PrintCurrentThread()
{
    MemoryProfiler* instance = NULL;
    instance = MemoryProfiler::Instance;

    Output::Print(L"========================================================================================================\n");
    Output::Print(L"Memory Profile (Current thread)\n");
    if (instance != nullptr)
    {
        instance->Print();
    }

    Output::Flush();
}

void
MemoryProfiler::PrintAll()
{
    Output::Print(L"========================================================================================================\n");
    Output::Print(L"Memory Profile (All threads)\n");

    ForEachProfiler([] (MemoryProfiler * memoryProfiler)
    {
        memoryProfiler->Print();
    });

    Output::Flush();
}

bool
MemoryProfiler::IsTraceEnabled(bool isRecycler)
{
    if (!Js::Configuration::Global.flags.IsEnabled(Js::TraceMemoryFlag))
    {
        return false;
    }

    if (Js::Configuration::Global.flags.TraceMemory.IsEnabled(Js::AllPhase))
    {
        return true;
    }

    if (!isRecycler)
    {
        return (Js::Configuration::Global.flags.TraceMemory.IsEnabled(Js::RunPhase)
            || Js::Configuration::Global.flags.TraceMemory.GetFirstPhase() == Js::InvalidPhase);
    }

    return Js::Configuration::Global.flags.TraceMemory.IsEnabled(Js::RecyclerPhase);
}

bool
MemoryProfiler::IsEnabled()
{
    return Js::Configuration::Global.flags.IsEnabled(Js::ProfileMemoryFlag);
}

bool
MemoryProfiler::DoTrackRecyclerAllocation()
{
    return MemoryProfiler::IsEnabled() || MemoryProfiler::IsTraceEnabled(true) || MemoryProfiler::IsTraceEnabled(false);
}
#endif
