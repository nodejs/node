//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#ifdef PROFILE_MEM

class MemoryProfiler;

struct ArenaMemoryData
{
    size_t allocatedBytes;         // memory allocated
    size_t alignmentBytes;         // memory for alignment
    size_t requestBytes;           // memory asked to be allocated
    size_t requestCount;           // count of alloc
    size_t reuseBytes;
    size_t reuseCount;

    size_t freelistBytes;             // memory asked to be freed.
    size_t freelistCount;             // count of freed

    size_t resetCount;

    ArenaMemoryData * prev;
    ArenaMemoryData * next;

    MemoryProfiler * profiler;
};

struct ArenaMemoryDataSummary
{
    size_t arenaCount;
    size_t outstandingCount;
    ArenaMemoryData total;
    ArenaMemoryData max;
    ArenaMemoryData * data;
};

struct PageMemoryData
{
    size_t allocSegmentCount;
    size_t allocSegmentBytes;
    size_t releaseSegmentCount;
    size_t releaseSegmentBytes;
    size_t allocPageCount;
    size_t releasePageCount;
    size_t decommitPageCount;
    size_t recommitPageCount;

    size_t currentCommittedPageCount;
    size_t peakCommittedPageCount;
};

struct RecyclerMemoryData
{
    size_t allocatedBytes;
    size_t alignmentBytes;
    size_t requestBytes;
    size_t requestCount;
};

class MemoryProfiler
{
public:
    MemoryProfiler();
    ~MemoryProfiler();
    static ArenaMemoryData * Begin(LPCWSTR name);
    static RecyclerMemoryData * GetRecyclerMemoryData();
    static PageMemoryData * GetPageMemoryData(PageAllocatorType type);
    static void Reset(LPCWSTR name, ArenaMemoryData * arena);
    static void End(LPCWSTR name, ArenaMemoryData * arena);
    static void PrintAll();

    static void PrintCurrentThread();
    static bool IsTraceEnabled(bool isRecycler = false);
    static bool IsEnabled();
    static bool DoTrackRecyclerAllocation();

    template<typename THandler>
    static void GetArenaMemoryUsage(THandler handler)
    {
        ForEachProfiler([&] (MemoryProfiler * memoryProfiler)
        {
            memoryProfiler->WithArenaUsageSummary(true, [&] (int count, _In_reads_(count) LPWSTR * name, _In_reads_(count) ArenaMemoryDataSummary * summaries)
            {
                for (int i = 0; i < count; i++)
                {
                    ArenaMemoryDataSummary * data = summaries[i];
                    if (data == nullptr)
                    {
                        continue;
                    }
                    handler(name[i], data);
                }
            });
        });
    }

private:
    int MemoryProfiler::CreateArenaUsageSummary(ArenaAllocator * alloc, bool liveOnly,
        _Outptr_result_buffer_(return) LPWSTR ** name_ptr, _Outptr_result_buffer_(return) ArenaMemoryDataSummary *** summaries_ptr);

    template<typename THandler>
    void WithArenaUsageSummary(bool liveOnly, THandler handler);

    template<typename THandler>
    static void ForEachProfiler(THandler handler)
    {
        MemoryProfiler * memoryProfiler = MemoryProfiler::profilers;
        while (memoryProfiler != nullptr)
        {
            handler(memoryProfiler);
            memoryProfiler = memoryProfiler->next;
        }
    }

    static MemoryProfiler * EnsureMemoryProfiler();
    static void AccumulateData(ArenaMemoryDataSummary *, ArenaMemoryData *, bool reset = false);
    void Print();
    void PrintArenaHeader(wchar_t const * title);
    static void PrintPageMemoryData(PageMemoryData const& pageMemoryData, char const * title);

private:

    void PrintArena(bool liveOnly);

    static __declspec(thread) MemoryProfiler * Instance;

    static CriticalSection s_cs;
    static AutoPtr<MemoryProfiler, NoCheckHeapAllocator> profilers;

    PageAllocator pageAllocator;
    ArenaAllocator alloc;
    JsUtil::BaseDictionary<LPWSTR, ArenaMemoryDataSummary *, ArenaAllocator, PrimeSizePolicy> arenaDataMap;
    PageMemoryData pageMemoryData[PageAllocatorType_Max];
    RecyclerMemoryData recyclerMemoryData;
    MemoryProfiler * next;
    DWORD threadId;

};

template<typename THandler>
void
MemoryProfiler::WithArenaUsageSummary(bool liveOnly, THandler handler)
{
    // This is debug only code, we don't care if we catch the right exception
    AUTO_NESTED_HANDLED_EXCEPTION_TYPE(ExceptionType_DisableCheck);

    PageAllocator tempPageAlloc(nullptr, Js::Configuration::Global.flags);
    ArenaAllocator tempAlloc(L"MemoryProfiler", &tempPageAlloc, Js::Throw::OutOfMemory);
    LPWSTR * name = nullptr;
    ArenaMemoryDataSummary ** summaries = nullptr;
    int count = CreateArenaUsageSummary(&tempAlloc, liveOnly, &name, &summaries);
    handler(count, name, summaries);
}

#endif
