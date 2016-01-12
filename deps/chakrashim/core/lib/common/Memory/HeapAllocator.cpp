//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonMemoryPch.h"

#ifdef INTERNAL_MEM_PROTECT_HEAP_ALLOC
// Not enabled in ChakraCore
#include "MemProtectHeap.h"
#endif

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

#ifdef HEAP_TRACK_ALLOC
CriticalSection HeapAllocator::cs;
#endif

#ifdef CHECK_MEMORY_LEAK
MemoryLeakCheck MemoryLeakCheck::leakCheck;
#endif

HeapAllocator HeapAllocator::Instance;
NoThrowHeapAllocator NoThrowHeapAllocator::Instance;
NoCheckHeapAllocator NoCheckHeapAllocator::Instance;
HANDLE NoCheckHeapAllocator::processHeap = NULL;

template <bool noThrow>
char * HeapAllocator::AllocT(size_t byteSize)
{
#ifdef HEAP_TRACK_ALLOC
    size_t requestedBytes = byteSize;
    byteSize = AllocSizeMath::Add(requestedBytes, ::Math::Align<size_t>(sizeof(HeapAllocRecord), MEMORY_ALLOCATION_ALIGNMENT));
    TrackAllocData allocData;
    ClearTrackAllocInfo(&allocData);
#elif defined(HEAP_PERF_COUNTERS)
    size_t requestedBytes = byteSize;
    byteSize = AllocSizeMath::Add(requestedBytes, ::Math::Align<size_t>(sizeof(size_t), MEMORY_ALLOCATION_ALIGNMENT));
#endif

    if (noThrow)
    {
        FAULTINJECT_MEMORY_NOTHROW(L"Heap", byteSize);
    }
    else
    {
        FAULTINJECT_MEMORY_THROW(L"Heap", byteSize);
    }

    char * buffer;
#ifdef INTERNAL_MEM_PROTECT_HEAP_ALLOC
    if (DoUseMemProtectHeap())
    {
        void * memory = MemProtectHeapRootAlloc(memProtectHeapHandle, byteSize);
        if (memory == nullptr)
        {
            if (noThrow)
            {
                return nullptr;
            }
            Js::Throw::OutOfMemory();
        }
        buffer = (char *)memory;
    }
    else
#endif
    {
        buffer = (char *)malloc(byteSize);
    }

    if (!noThrow && buffer == nullptr)
    {
        Js::Throw::OutOfMemory();
    }

#if defined(HEAP_TRACK_ALLOC) || defined(HEAP_PERF_COUNTERS)
    if (!noThrow || buffer != nullptr)
    {
#ifdef HEAP_TRACK_ALLOC
        cs.Enter();
        data.LogAlloc((HeapAllocRecord *)buffer, requestedBytes, allocData);
        cs.Leave();
        buffer += ::Math::Align<size_t>(sizeof(HeapAllocRecord), MEMORY_ALLOCATION_ALIGNMENT);
#else
        *(size_t *)buffer = requestedBytes;
        buffer += ::Math::Align<size_t>(sizeof(size_t), MEMORY_ALLOCATION_ALIGNMENT);

#endif
        HEAP_PERF_COUNTER_INC(LiveObject);
        HEAP_PERF_COUNTER_ADD(LiveObjectSize, requestedBytes);
    }
#endif
    return buffer;
}

template char * HeapAllocator::AllocT<true>(size_t byteSize);
template char * HeapAllocator::AllocT<false>(size_t byteSize);


void HeapAllocator::Free(void * buffer, size_t byteSize)
{
#ifdef HEAP_TRACK_ALLOC
    if (buffer != nullptr)
    {
        HeapAllocRecord * record = (HeapAllocRecord *)(((char *)buffer) - ::Math::Align<size_t>(sizeof(HeapAllocRecord), MEMORY_ALLOCATION_ALIGNMENT));
        Assert(byteSize == (size_t)-1 || record->size == byteSize);

        HEAP_PERF_COUNTER_DEC(LiveObject);
        HEAP_PERF_COUNTER_SUB(LiveObjectSize, record->size);

        cs.Enter();
        data.LogFree(record);
        cs.Leave();

        buffer = record;
#if DBG
        memset(buffer, DbgMemFill, record->size + ::Math::Align<size_t>(sizeof(HeapAllocRecord), MEMORY_ALLOCATION_ALIGNMENT));
#endif
    }
#elif defined(HEAP_PERF_COUNTERS)
    if (buffer != nullptr)
    {
        HEAP_PERF_COUNTER_DEC(LiveObject);
        size_t * allocSize = (size_t *)(((char *)buffer) - ::Math::Align<size_t>(sizeof(size_t), MEMORY_ALLOCATION_ALIGNMENT));
        HEAP_PERF_COUNTER_SUB(LiveObjectSize, *allocSize);
        buffer = allocSize;
    }
#endif
#ifdef INTERNAL_MEM_PROTECT_HEAP_ALLOC
    if (DoUseMemProtectHeap())
    {
        HRESULT hr = MemProtectHeapUnrootAndZero(memProtectHeapHandle, buffer);
        Assert(SUCCEEDED(hr));
        return;
    }
#endif
    free(buffer);
}

#ifdef TRACK_ALLOC
#ifdef HEAP_TRACK_ALLOC
__declspec(thread) TrackAllocData HeapAllocator::nextAllocData;
#endif

HeapAllocator * HeapAllocator::TrackAllocInfo(TrackAllocData const& data)
{
#ifdef HEAP_TRACK_ALLOC
    Assert(nextAllocData.IsEmpty());
    nextAllocData = data;
#endif
    return this;
}

void HeapAllocator::ClearTrackAllocInfo(TrackAllocData* data/* = NULL*/)
{
#ifdef HEAP_TRACK_ALLOC
    Assert(!nextAllocData.IsEmpty());
    if (data)
    {
        *data = nextAllocData;
    }
    nextAllocData.Clear();
#endif
}
#endif

#ifdef HEAP_TRACK_ALLOC
//static
bool HeapAllocator::CheckLeaks()
{
    return Instance.data.CheckLeaks();
}
#endif HEAP_TRACK_ALLOC

char * NoThrowHeapAllocator::AllocZero(size_t byteSize)
{
    return HeapAllocator::Instance.NoThrowAllocZero(byteSize);
}

char * NoThrowHeapAllocator::Alloc(size_t byteSize)
{
    return HeapAllocator::Instance.NoThrowAlloc(byteSize);
}

void NoThrowHeapAllocator::Free(void * buffer, size_t byteSize)
{
    HeapAllocator::Instance.Free(buffer, byteSize);
}

#ifdef TRACK_ALLOC
NoThrowHeapAllocator * NoThrowHeapAllocator::TrackAllocInfo(TrackAllocData const& data)
{
    HeapAllocator::Instance.TrackAllocInfo(data);
    return this;
}
#endif TRACK_ALLOC

#ifdef TRACK_ALLOC
void NoThrowHeapAllocator::ClearTrackAllocInfo(TrackAllocData* data /*= NULL*/)
{
    HeapAllocator::Instance.ClearTrackAllocInfo(data);
}
#endif TRACK_ALLOC

HeapAllocator * HeapAllocator::GetNoMemProtectInstance()
{
#ifdef INTERNAL_MEM_PROTECT_HEAP_ALLOC
    // Used only in Chakra, no need to use CUSTOM_CONFIG_FLAG
    if (CONFIG_FLAG(MemProtectHeap))
    {
        return &NoMemProtectInstance;
    }
#endif
    return &Instance;
}
#ifdef INTERNAL_MEM_PROTECT_HEAP_ALLOC
HeapAllocator HeapAllocator::NoMemProtectInstance(false);

HeapAllocator::HeapAllocator(bool allocMemProtect) : isUsed(false), memProtectHeapHandle(nullptr), allocMemProtect(allocMemProtect)
{
}

bool HeapAllocator::DoUseMemProtectHeap()
{
    if (!allocMemProtect)
    {
        return false;
    }

    if (memProtectHeapHandle != nullptr)
    {
        return true;
    }

    DebugOnly(bool wasUsed = isUsed);
    isUsed = true;

    // Flag is used only in Chakra, no need to use CUSTOM_CONFIG_FLAG
    if (CONFIG_FLAG(MemProtectHeap))
    {
        Assert(!wasUsed);
        if (FAILED(MemProtectHeapCreate(&memProtectHeapHandle, MemProtectHeapCreateFlags_ProtectCurrentStack)))
        {
            Assert(false);
        }
        return true;
    }

    return false;
}

void HeapAllocator::FinishMemProtectHeapCollect()
{
    if (memProtectHeapHandle)
    {
        MemProtectHeapCollect(memProtectHeapHandle, MemProtectHeap_ForceFinishCollect);
        DebugOnly(MemProtectHeapSetDisableConcurrentThreadExitedCheck(memProtectHeapHandle));
    }
}

NoThrowNoMemProtectHeapAllocator NoThrowNoMemProtectHeapAllocator::Instance;

char * NoThrowNoMemProtectHeapAllocator::AllocZero(size_t byteSize)
{
    return HeapAllocator::GetNoMemProtectInstance()->NoThrowAllocZero(byteSize);
}

char * NoThrowNoMemProtectHeapAllocator::Alloc(size_t byteSize)
{
    return HeapAllocator::GetNoMemProtectInstance()->NoThrowAlloc(byteSize);
}

void NoThrowNoMemProtectHeapAllocator::Free(void * buffer, size_t byteSize)
{
    HeapAllocator::GetNoMemProtectInstance()->Free(buffer, byteSize);
}

#ifdef TRACK_ALLOC
NoThrowNoMemProtectHeapAllocator * NoThrowNoMemProtectHeapAllocator::TrackAllocInfo(TrackAllocData const& data)
{
    HeapAllocator::GetNoMemProtectInstance()->TrackAllocInfo(data);
    return this;
}
#endif TRACK_ALLOC

#ifdef TRACK_ALLOC
void NoThrowNoMemProtectHeapAllocator::ClearTrackAllocInfo(TrackAllocData* data /*= NULL*/)
{
    HeapAllocator::GetNoMemProtectInstance()->ClearTrackAllocInfo(data);
}
#endif TRACK_ALLOC
#endif

#if defined(HEAP_TRACK_ALLOC) || defined(INTERNAL_MEM_PROTECT_HEAP_ALLOC)

HeapAllocator::~HeapAllocator()
{
#ifdef HEAP_TRACK_ALLOC
    bool hasFakeHeapLeak = false;
    auto fakeHeapLeak = [&]()
    {
        // REVIEW: Okay to use global flags?
        if (Js::Configuration::Global.flags.ForceMemoryLeak && !hasFakeHeapLeak)
        {
            AUTO_HANDLED_EXCEPTION_TYPE(ExceptionType_DisableCheck);
            struct FakeMemory { int f; };
            HeapNewStruct(FakeMemory);
            hasFakeHeapLeak = true;
        }
    };

#ifdef LEAK_REPORT
    // REVIEW: Okay to use global flags?
    if (Js::Configuration::Global.flags.IsEnabled(Js::LeakReportFlag))
    {
        fakeHeapLeak();
        LeakReport::StartSection(L"Heap Leaks");
        LeakReport::StartRedirectOutput();
        bool leaked = !HeapAllocator::CheckLeaks();
        LeakReport::EndRedirectOutput();
        LeakReport::EndSection();

        LeakReport::Print(L"--------------------------------------------------------------------------------\n");
        if (leaked)
        {
            LeakReport::Print(L"Heap Leaked Object: %d bytes (%d objects)\n",
                data.outstandingBytes, data.allocCount - data.deleteCount);
        }
    }
#endif // LEAK_REPORT

#ifdef CHECK_MEMORY_LEAK
    // REVIEW: Okay to use global flags?
    if (Js::Configuration::Global.flags.CheckMemoryLeak)
    {
        fakeHeapLeak();
        Output::CaptureStart();
        Output::Print(L"-------------------------------------------------------------------------------------\n");
        Output::Print(L"Heap Leaks\n");
        Output::Print(L"-------------------------------------------------------------------------------------\n");
        if (!HeapAllocator::CheckLeaks())
        {
            Output::Print(L"-------------------------------------------------------------------------------------\n");
            Output::Print(L"Heap Leaked Object: %d bytes (%d objects)\n",
                data.outstandingBytes, data.allocCount - data.deleteCount);
            wchar_t * buffer = Output::CaptureEnd();
            MemoryLeakCheck::AddLeakDump(buffer, data.outstandingBytes, data.allocCount - data.deleteCount);
        }
        else
        {
            free(Output::CaptureEnd());
        }
    }
#endif // CHECK_MEMORY_LEAK
#endif // HEAP_TRACK_ALLOC

#ifdef INTERNAL_MEM_PROTECT_HEAP_ALLOC
    if (memProtectHeapHandle != nullptr)
    {
        MemProtectHeapDestroy(memProtectHeapHandle);
    }
#endif // INTERNAL_MEM_PROTECT_HEAP_ALLOC
}
#endif // defined(HEAP_TRACK_ALLOC) || defined(INTERNAL_MEM_PROTECT_HEAP_ALLOC)

#ifdef HEAP_TRACK_ALLOC
void
HeapAllocatorData::LogAlloc(HeapAllocRecord * record, size_t requestedBytes, TrackAllocData const& data)
{
    record->prev = nullptr;
    record->size = requestedBytes;

    record->data = this;
    record->next = head;
    record->allocId = allocCount;
    record->allocData = data;
    if (head != nullptr)
    {
        head->prev = record;
    }
    head = record;
    outstandingBytes += requestedBytes;
    allocCount++;

#if defined(CHECK_MEMORY_LEAK) || defined(LEAK_REPORT)
    // REVIEW: Okay to use global flags?
    if (Js::Configuration::Global.flags.LeakStackTrace)
    {
        // Allocation done before the flags is parse doesn't get a stack trace
        record->stacktrace = StackBackTrace::Capture(&NoCheckHeapAllocator::Instance, 1, StackTraceDepth);
    }
    else
    {
        record->stacktrace = nullptr;
    }
#endif
}

void
HeapAllocatorData::LogFree(HeapAllocRecord * record)
{
    Assert(record->data == this);

    // This is an expensive check for double free
#if 0
    HeapAllocRecord * curr = head;
    while (curr != nullptr)
    {
        if (curr == record)
        {
            break;
        }
        curr = curr->next;
    }
    Assert(curr != nullptr);
#endif
    if (record->next != nullptr)
    {
        record->next->prev = record->prev;
    }
    if (record->prev == nullptr)
    {
        head = record->next;
    }
    else
    {
        record->prev->next = record->next;
    }

    deleteCount++;
    outstandingBytes -= record->size;
#if defined(CHECK_MEMORY_LEAK) || defined(LEAK_REPORT)
    if (record->stacktrace != nullptr)
    {
        record->stacktrace->Delete(&NoCheckHeapAllocator::Instance);
    }
#endif
}

bool
HeapAllocatorData::CheckLeaks()
{
    bool needPause = false;
    if (allocCount != deleteCount)
    {
        needPause = true;

        HeapAllocRecord * current = head;
        while (current != nullptr)
        {
            Output::Print(L"%S%s", current->allocData.GetTypeInfo()->name(),
                current->allocData.GetCount() == (size_t)-1? L"" : L"[]");
            Output::SkipToColumn(50);
            Output::Print(L"- %p - %10d bytes\n",
                ((char*)current) + ::Math::Align<size_t>(sizeof(HeapAllocRecord), MEMORY_ALLOCATION_ALIGNMENT),
                current->size);
#if defined(CHECK_MEMORY_LEAK) || defined(LEAK_REPORT)
            // REVIEW: Okay to use global flags?
            if (Js::Configuration::Global.flags.LeakStackTrace && current->stacktrace)
            {
                // Allocation done before the flags is parse doesn't get a stack trace
                Output::Print(L" Allocation Stack:\n");
                current->stacktrace->Print();
            }
#endif
            current = current->next;
        }
    }
    else if (outstandingBytes != 0)
    {
        needPause = true;
        Output::Print(L"Unbalanced new/delete size: %d\n", outstandingBytes);
    }

    Output::Flush();

#if defined(ENABLE_DEBUG_CONFIG_OPTIONS) && !DBG
    // REVIEW: Okay to use global flags?
    if (needPause && Js::Configuration::Global.flags.Console)
    {
        //This is not defined for WinCE
        HANDLE handle = GetStdHandle( STD_INPUT_HANDLE );

        FlushConsoleInputBuffer(handle);

        Output::Print(L"Press any key to continue...\n");
        Output::Flush();

        WaitForSingleObject(handle, INFINITE);

    }
#endif
    return allocCount == deleteCount && outstandingBytes == 0;
}

#endif


#ifdef CHECK_MEMORY_LEAK
MemoryLeakCheck::~MemoryLeakCheck()
{
    if (head != nullptr)
    {
        if (enableOutput)
        {
            Output::Print(L"FATAL ERROR: Memory Leak Detected\n");
        }
        LeakRecord * current = head;
        do
        {

            if (enableOutput)
            {
                Output::PrintBuffer(current->dump, wcslen(current->dump));
            }
            LeakRecord * prev = current;
            current = current->next;
            free((void *)prev->dump);
            NoCheckHeapDelete(prev);
        }
        while (current != nullptr);
        if (enableOutput)
        {
            Output::Print(L"-------------------------------------------------------------------------------------\n");
            Output::Print(L"Total leaked: %d bytes (%d objects)\n", leakedBytes, leakedCount);
            Output::Flush();
        }
        if (enableOutput)
        {
            Js::Throw::GenerateDump(Js::Configuration::Global.flags.DumpOnCrash, true, true);
        }
    }
}

void
MemoryLeakCheck::AddLeakDump(wchar_t const * dump, size_t bytes, size_t count)
{
    AutoCriticalSection autocs(&leakCheck.cs);
    LeakRecord * record = NoCheckHeapNewStruct(LeakRecord);
    record->dump = dump;
    record->next = nullptr;
    if (leakCheck.tail == nullptr)
    {
        leakCheck.head = record;
        leakCheck.tail = record;
    }
    else
    {
        leakCheck.tail->next = record;
        leakCheck.tail = record;
    }
    leakCheck.leakedBytes += bytes;
    leakCheck.leakedCount += count;
}
#endif
