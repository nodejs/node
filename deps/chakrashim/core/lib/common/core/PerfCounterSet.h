//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#ifdef PERF_COUNTERS

namespace PerfCounter
{
    template <typename TCounter>
    class DefaultCounterSetInstance : public InstanceBase
    {
    public:
        DefaultCounterSetInstance() : InstanceBase(TCounter::GetProvider(), TCounter::GetGuid())
        {
            if (Initialize())
            {
                data = defaultData;
            }
            else
            {
                // if for any reason perf counter failed to initialize, try to create
                // shared memory instead. This will happen for sure if running under
                // Win8 AppContainer because they don't support v2 perf counters.
                // See comments in WWAHostJSCounterProvider for details.
                data = __super::InitializeSharedMemory(TCounter::MaxCounter, handle);
                if (data == nullptr)
                {
                    data = defaultData;
                }
            }
            for (uint i = 0; i < TCounter::MaxCounter; i++)
            {
                data[i] = 0;
                counters[i].Initialize(*this, i, &data[i]);
            }
        }
        ~DefaultCounterSetInstance()
        {
            for (uint i = 0; i < TCounter::MaxCounter; i++)
            {
                counters[i].Uninitialize(*this, i);
            }

            if (data != defaultData)
            {
                __super::UninitializeSharedMemory(data, handle);
            }
        }
        Counter& GetCounter(uint id) { Assert(id < TCounter::MaxCounter); return counters[id]; }

        bool Initialize()
        {
            if (IsProviderInitialized())
            {
                wchar_t wszModuleName[_MAX_PATH];
                if (!GetModuleFileName(NULL, wszModuleName, _MAX_PATH))
                {
                    return false;
                }
                wchar_t wszFilename[_MAX_FNAME];
                _wsplitpath_s(wszModuleName, NULL, 0, NULL, 0, wszFilename, _MAX_FNAME, NULL, 0);

                return __super::Initialize(wszFilename, GetCurrentProcessId());
            }
            return false;
        }
    private:
        DWORD defaultData[TCounter::MaxCounter];
        DWORD * data;
        HANDLE handle;
        Counter counters[TCounter::MaxCounter];
    };

    class PageAllocatorCounterSet
    {
    public:
        static Counter& GetReservedSizeCounter(PageAllocatorType type)
            { return instance.GetCounter(PageAllocatorCounterSetDefinition::GetReservedCounterId(type)); }
        static Counter& GetTotalReservedSizeCounter();
        static Counter& GetCommittedSizeCounter(PageAllocatorType type)
            { return instance.GetCounter(PageAllocatorCounterSetDefinition::GetCommittedCounterId(type)); }
        static Counter& GetTotalCommittedSizeCounter();
        static Counter& GetUsedSizeCounter(PageAllocatorType type)
            { return instance.GetCounter(PageAllocatorCounterSetDefinition::GetUsedCounterId(type)); }
        static Counter& GetTotalUsedSizeCounter();
    private:
        static DefaultCounterSetInstance<PageAllocatorCounterSetDefinition> instance;
    };

    class BasicCounterSet
    {
    public:
        static Counter& GetThreadContextCounter() { return instance.GetCounter(0); }
        static Counter& GetScriptContextCounter() { return instance.GetCounter(1); }
        static Counter& GetScriptContextActiveCounter() { return instance.GetCounter(2); }
        static Counter& GetScriptCodeBufferCountCounter() { return instance.GetCounter(3); }
    private:
        static DefaultCounterSetInstance<BasicCounterSetDefinition> instance;
    };


    class CodeCounterSet
    {
    public:
        static Counter& GetTotalByteCodeSizeCounter() { return instance.GetCounter(0); }
        static Counter& GetTotalNativeCodeSizeCounter() { return instance.GetCounter(1); }
        static Counter& GetTotalNativeCodeDataSizeCounter() { return instance.GetCounter(2); }
        static Counter& GetStaticByteCodeSizeCounter() { return instance.GetCounter(3); }
        static Counter& GetStaticNativeCodeSizeCounter() { return instance.GetCounter(4); }
        static Counter& GetStaticNativeCodeDataSizeCounter() { return instance.GetCounter(5); }
        static Counter& GetDynamicByteCodeSizeCounter() { return instance.GetCounter(6); }
        static Counter& GetDynamicNativeCodeSizeCounter() { return instance.GetCounter(7); }
        static Counter& GetDynamicNativeCodeDataSizeCounter() { return instance.GetCounter(8); }
        static Counter& GetTotalFunctionCounter() { return instance.GetCounter(9); }
        static Counter& GetStaticFunctionCounter() { return instance.GetCounter(10); }
        static Counter& GetDynamicFunctionCounter() { return instance.GetCounter(11); }
        static Counter& GetLoopNativeCodeSizeCounter() { return instance.GetCounter(12); }
        static Counter& GetFunctionNativeCodeSizeCounter() { return instance.GetCounter(13); }
        static Counter& GetDeferDeserializeFunctionProxyCounter() { return instance.GetCounter(14); }
        static Counter& GetDeserializedFunctionBodyCounter() { return instance.GetCounter(15); }
        static Counter& GetDeferedFunctionCounter() { return instance.GetCounter(16); }

    private:
        static DefaultCounterSetInstance<CodeCounterSetDefinition> instance;
    };

#ifdef HEAP_PERF_COUNTERS
    class HeapCounterSet
    {
    public:
        static Counter& GetLiveObjectCounter() { return instance.GetCounter(0); }
        static Counter& GetLiveObjectSizeCounter() { return instance.GetCounter(1); }
    private:
        static DefaultCounterSetInstance<HeapCounterSetDefinition> instance;
    };
#endif
#ifdef RECYCLER_PERF_COUNTERS
    class RecyclerCounterSet
    {
    public:
        static Counter& GetLiveObjectSizeCounter() { return instance.GetCounter(0); }
        static Counter& GetLiveObjectCounter() { return instance.GetCounter(1); }
        static Counter& GetFreeObjectSizeCounter() { return instance.GetCounter(2); }
        static Counter& GetPinnedObjectCounter() { return instance.GetCounter(3); }
        static Counter& GetBindReferenceCounter() { return instance.GetCounter(4); }
        static Counter& GetPropertyRecordBindReferenceCounter() { return instance.GetCounter(5); }
        static Counter& GetLargeHeapBlockLiveObjectSizeCounter() { return instance.GetCounter(6); }
        static Counter& GetLargeHeapBlockLiveObjectCounter() { return instance.GetCounter(7); }
        static Counter& GetLargeHeapBlockFreeObjectSizeCounter() { return instance.GetCounter(8); }
        static Counter& GetSmallHeapBlockLiveObjectSizeCounter() { return instance.GetCounter(9); }
        static Counter& GetSmallHeapBlockLiveObjectCounter() { return instance.GetCounter(10); }
        static Counter& GetSmallHeapBlockFreeObjectSizeCounter() { return instance.GetCounter(11); }
        static Counter& GetLargeHeapBlockCountCounter() { return instance.GetCounter(12); }
        static Counter& GetLargeHeapBlockPageSizeCounter() { return instance.GetCounter(13); }

    private:
        static DefaultCounterSetInstance<RecyclerCounterSetDefinition> instance;
    };
#endif

#ifdef PROFILE_RECYCLER_ALLOC
    class RecyclerTrackerCounterSet
    {
    public:
        static Counter& GetPerfCounter(type_info const * typeinfo, bool isArray);
        static Counter& GetPerfSizeCounter(type_info const * typeinfo, bool isArray);
        static Counter& GetWeakRefPerfCounter(type_info const * typeinfo);

        class Map
        {
        public:
            Map(type_info const * type, bool isArray, uint counterIndex, uint sizeCounterIndex);
            Map(type_info const * type, uint weakRefCounterIndex);
        };

    private:
        static Counter& GetUnknownCounter() { return instance.GetCounter(0); }
        static Counter& GetUnknownSizeCounter() { return instance.GetCounter(1); }
        static Counter& GetUnknownArrayCounter() { return instance.GetCounter(2); }
        static Counter& GetUnknownArraySizeCounter() { return instance.GetCounter(3); }
        static Counter& GetUnknownWeakRefCounter() { return instance.GetCounter(4); }
        static DefaultCounterSetInstance<RecyclerTrackerCounterSetDefinition> instance;

        static uint const NumUnknownCounters = 5;
        static type_info const * CountIndexTypeInfoMap[RecyclerTrackerCounterSetDefinition::MaxCounter - NumUnknownCounters];
        static type_info const * SizeIndexTypeInfoMap[RecyclerTrackerCounterSetDefinition::MaxCounter - NumUnknownCounters];
        static type_info const * ArrayCountIndexTypeInfoMap[RecyclerTrackerCounterSetDefinition::MaxCounter - NumUnknownCounters];
        static type_info const * ArraySizeIndexTypeInfoMap[RecyclerTrackerCounterSetDefinition::MaxCounter - NumUnknownCounters];
        static type_info const * WeakRefIndexTypeInfoMap[RecyclerTrackerCounterSetDefinition::MaxCounter - NumUnknownCounters];
    };

#define DEFINE_RECYCLER_TRACKER_PERF_COUNTER(type) \
    static PerfCounter::RecyclerTrackerCounterSet::Map RecyclerTrackerCounter##id(&typeid(type), false, \
        PerfCounter::RecyclerTrackerCounterSetDefinition::##type##CounterIndex, \
        PerfCounter::RecyclerTrackerCounterSetDefinition::##type##SizeCounterIndex)

#define DEFINE_RECYCLER_TRACKER_ARRAY_PERF_COUNTER(type) \
    static PerfCounter::RecyclerTrackerCounterSet::Map RecyclerTrackerArrayCounter##id(&typeid(type), true, \
        PerfCounter::RecyclerTrackerCounterSetDefinition::##type##ArrayCounterIndex, \
        PerfCounter::RecyclerTrackerCounterSetDefinition::##type##ArraySizeCounterIndex)

#define DEFINE_RECYCLER_TRACKER_WEAKREF_PERF_COUNTER(type) \
    static PerfCounter::RecyclerTrackerCounterSet::Map RecyclerTrackerWeakRefCounter##id(&typeid(type), \
        PerfCounter::RecyclerTrackerCounterSetDefinition::##type##WeakRefCounterIndex);

#else
#define DEFINE_RECYCLER_TRACKER_PERF_COUNTER(type)
#define DEFINE_RECYCLER_TRACKER_ARRAY_PERF_COUNTER(type)
#define DEFINE_RECYCLER_TRACKER_WEAKREF_PERF_COUNTER(type)
#endif
};

#define PERF_COUNTER_INC(CounterSetName, CounterName) ++PerfCounter::CounterSetName##CounterSet::Get##CounterName##Counter()
#define PERF_COUNTER_DEC(CounterSetName, CounterName) --PerfCounter::CounterSetName##CounterSet::Get##CounterName##Counter()

#define PERF_COUNTER_ADD(CounterSetName, CounterName, value) PerfCounter::CounterSetName##CounterSet::Get##CounterName##Counter() += value
#define PERF_COUNTER_SUB(CounterSetName, CounterName, value) PerfCounter::CounterSetName##CounterSet::Get##CounterName##Counter() -= value
#else
#define PERF_COUNTER_INC(CounterSetName, CounterName)
#define PERF_COUNTER_DEC(CounterSetName, CounterName)
#define PERF_COUNTER_ADD(CounterSetName, CounterName, value)
#define PERF_COUNTER_SUB(CounterSetName, CounterName, value)
#define DEFINE_RECYCLER_TRACKER_PERF_COUNTER(type)
#define DEFINE_RECYCLER_TRACKER_ARRAY_PERF_COUNTER(type)
#define DEFINE_RECYCLER_TRACKER_WEAKREF_PERF_COUNTER(type)
#endif

#ifdef HEAP_PERF_COUNTERS
#define HEAP_PERF_COUNTER_INC(CounterName) PERF_COUNTER_INC(Heap, CounterName)
#define HEAP_PERF_COUNTER_DEC(CounterName) PERF_COUNTER_DEC(Heap, CounterName)
#define HEAP_PERF_COUNTER_ADD(CounterName, value) PERF_COUNTER_ADD(Heap, CounterName, value)
#define HEAP_PERF_COUNTER_SUB(CounterName, value) PERF_COUNTER_SUB(Heap, CounterName, value)
#else
#define HEAP_PERF_COUNTER_INC(CounterName)
#define HEAP_PERF_COUNTER_DEC(CounterName)
#define HEAP_PERF_COUNTER_ADD(CounterName, value)
#define HEAP_PERF_COUNTER_SUB(CounterName, value)
#endif

#ifdef RECYCLER_PERF_COUNTERS
#define RECYCLER_PERF_COUNTER_INC(CounterName) PERF_COUNTER_INC(Recycler, CounterName)
#define RECYCLER_PERF_COUNTER_DEC(CounterName) PERF_COUNTER_DEC(Recycler, CounterName)
#define RECYCLER_PERF_COUNTER_ADD(CounterName, value) PERF_COUNTER_ADD(Recycler, CounterName, value)
#define RECYCLER_PERF_COUNTER_SUB(CounterName, value) PERF_COUNTER_SUB(Recycler, CounterName, value)
#else
#define RECYCLER_PERF_COUNTER_INC(CounterName)
#define RECYCLER_PERF_COUNTER_DEC(CounterName)
#define RECYCLER_PERF_COUNTER_ADD(CounterName, value)
#define RECYCLER_PERF_COUNTER_SUB(CounterName, value)
#endif
