//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#ifdef PERF_COUNTERS

// Forward declaration from perflib.h
struct _PERF_COUNTERSET_INSTANCE;
typedef struct _PERF_COUNTERSET_INSTANCE *PPERF_COUNTERSET_INSTANCE;

enum PageAllocatorType;

#define MAX_OBJECT_NAME_PREFIX 1024

namespace PerfCounter
{
    class Provider;
    class InstanceBase
    {
    protected:
        InstanceBase(Provider& provider, GUID const& guid);
        ~InstanceBase();

        bool IsProviderInitialized() const;
        bool Initialize(wchar_t const * wszInstanceName, DWORD id);
        DWORD * InitializeSharedMemory(DWORD numCounter, HANDLE& handle);
        DWORD * OpenSharedMemory(__in_ecount(MAX_OBJECT_NAME_PREFIX) wchar_t const wszObjectNamePrefix[MAX_OBJECT_NAME_PREFIX], DWORD pid, DWORD numCounter, HANDLE& handle);
        void UninitializeSharedMemory(DWORD * data, HANDLE handle);
        bool IsEnabled() const;
    private:
        Provider& GetProvider() { return provider; }
        PPERF_COUNTERSET_INSTANCE GetData() { return instanceData; }

        Provider& provider;
        GUID const& guid;
        PPERF_COUNTERSET_INSTANCE instanceData;
        friend class Counter;
    };

    class Counter
    {
    public:
        Counter() : count(NULL) {};
        void Initialize(InstanceBase& instance, DWORD id, DWORD * count);
        void Uninitialize(InstanceBase& instance, DWORD id);

        Counter& operator+=(size_t value);
        Counter& operator-=(size_t value);
        Counter& operator++();
        Counter& operator--();
        DWORD GetValue() { return *count; }

    private:
        /* TODO: 64-bit */
        DWORD * count;
    };

    class PageAllocatorCounterSetDefinition
    {
    public:
        static DWORD const MaxCounter = 24;
        static GUID const& GetGuid();
        static Provider& GetProvider();

        static uint GetReservedCounterId(PageAllocatorType type);
        static uint GetCommittedCounterId(PageAllocatorType type);
        static uint GetUsedCounterId(PageAllocatorType type);
    };

    class BasicCounterSetDefinition
    {
    public:
        static DWORD const MaxCounter = 4;
        static GUID const& GetGuid();
        static Provider& GetProvider();
    };

    class CodeCounterSetDefinition
    {
    public:
        static DWORD const MaxCounter = 17;
        static GUID const& GetGuid();
        static Provider& GetProvider();
    };

#ifdef HEAP_PERF_COUNTERS
    class HeapCounterSetDefinition
    {
    public:
        static DWORD const MaxCounter = 2;
        static GUID const& GetGuid();
        static Provider& GetProvider();
    };
#endif

#ifdef RECYCLER_PERF_COUNTERS
    class RecyclerCounterSetDefinition
    {
    public:
        static DWORD const MaxCounter = 14;
        static GUID const& GetGuid();
        static Provider& GetProvider();
    };
#endif

#ifdef PROFILE_RECYCLER_ALLOC

#define RECYCLER_TRACKER_PERF_COUNTER_TYPE(MACRO) \
    MACRO(JavascriptNumber); \
    MACRO(ConcatString); \
    MACRO(LiteralString); \
    MACRO(SubString); \
    MACRO(PropertyString); \
    MACRO(PropertyRecord); \
    MACRO(DynamicObject); \
    MACRO(CustomExternalObject); \
    MACRO(DynamicType); \
    MACRO(JavascriptFunction); \
    MACRO(JavascriptArray); \
    MACRO(SingleCharString); \
    MACRO(FrameDisplay); \
    MACRO(CompoundString); \
    MACRO(RecyclerWeakReferenceBase); \
    MACRO(ProjectionObjectInstance); \

#define RECYCLER_TRACKER_ARRAY_PERF_COUNTER_TYPE(MACRO) \
    MACRO(Var); \
    MACRO(wchar_t); \

#define RECYCLER_TRACKER_WEAKREF_PERF_COUNTER_TYPE(MACRO) \
    MACRO(PropertyRecord); \
    MACRO(DynamicType); \
    MACRO(PropertyString); \
    MACRO(DynamicObject); \
    MACRO(Type); \

#define DECLARE_RECYCLER_TRACKER_PERF_COUNTER_INDEX(type) \
    static uint const type##CounterIndex; \
    static uint const type##SizeCounterIndex;

#define DECLARE_RECYCLER_TRACKER_ARRAY_PERF_COUNTER_INDEX(type) \
    static uint const type##ArrayCounterIndex; \
    static uint const type##ArraySizeCounterIndex;

#define DECLARE_RECYCLER_TRACKER_WEAKREF_PERF_COUNTER_INDEX(type) \
    static uint const type##WeakRefCounterIndex;

    class RecyclerTrackerCounterSetDefinition
    {
    public:
        static DWORD const MaxCounter = 46;
        static GUID const& GetGuid();
        static Provider& GetProvider();

        RECYCLER_TRACKER_PERF_COUNTER_TYPE(DECLARE_RECYCLER_TRACKER_PERF_COUNTER_INDEX);
        RECYCLER_TRACKER_ARRAY_PERF_COUNTER_TYPE(DECLARE_RECYCLER_TRACKER_ARRAY_PERF_COUNTER_INDEX);
        RECYCLER_TRACKER_WEAKREF_PERF_COUNTER_TYPE(DECLARE_RECYCLER_TRACKER_WEAKREF_PERF_COUNTER_INDEX);
    };

#undef DECLARE_RECYCLER_TRACKER_PERF_COUNTER_INDEX
#undef DECLARE_RECYCLER_TRACKER_ARRAY_PERF_COUNTER_INDEX
#undef DECLARE_RECYCLER_TRACKER_WEAKREF_PERF_COUNTER_INDEX

#endif
};
#endif
