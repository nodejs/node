//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#define HeapNew(T, ...) AllocatorNew(HeapAllocator, &HeapAllocator::Instance, T, __VA_ARGS__)
#define HeapNewZ(T, ...) AllocatorNewZ(HeapAllocator, &HeapAllocator::Instance, T, __VA_ARGS__)
#define HeapNewPlus(size, T, ...) AllocatorNewPlus(HeapAllocator, &HeapAllocator::Instance, size, T, __VA_ARGS__)
#define HeapNewPlusZ(size, T, ...) AllocatorNewPlusZ(HeapAllocator, &HeapAllocator::Instance, size, T, __VA_ARGS__)
#define HeapNewStruct(T) AllocatorNewStruct(HeapAllocator, &HeapAllocator::Instance, T);
#define HeapNewStructZ(T) AllocatorNewStructZ(HeapAllocator, &HeapAllocator::Instance, T);
#define HeapNewStructPlus(size, T, ...) AllocatorNewStructPlus(HeapAllocator, &HeapAllocator::Instance, size, T)
#define HeapNewStructPlusZ(size, T, ...) AllocatorNewStructPlusZ(HeapAllocator, &HeapAllocator::Instance, size, T)
#define HeapNewArray(T, count, ...) AllocatorNewArray(HeapAllocator, &HeapAllocator::Instance, T, count)
#define HeapNewArrayZ(T, count, ...) AllocatorNewArrayZ(HeapAllocator, &HeapAllocator::Instance, T, count)
#define HeapDelete(obj) AllocatorDelete(HeapAllocator, &HeapAllocator::Instance, obj)
#define HeapDeletePlus(size, obj) AllocatorDeletePlus(HeapAllocator, &HeapAllocator::Instance, size, obj)
#define HeapDeletePlusPrefix(size, obj) AllocatorDeletePlusPrefix(HeapAllocator, &HeapAllocator::Instance, size, obj)
#define HeapDeleteArray(count, obj) AllocatorDeleteArray(HeapAllocator, &HeapAllocator::Instance, count, obj)


#define HeapNewNoThrow(T, ...) AllocatorNewNoThrow(HeapAllocator, &HeapAllocator::Instance, T, __VA_ARGS__)
#define HeapNewNoThrowZ(T, ...) AllocatorNewNoThrowZ(HeapAllocator, &HeapAllocator::Instance, T, __VA_ARGS__)
#define HeapNewNoThrowPlus(size, T, ...) AllocatorNewNoThrowPlus(HeapAllocator, &HeapAllocator::Instance, size, T, __VA_ARGS__)
#define HeapNewNoThrowPlusZ(size, T, ...) AllocatorNewNoThrowPlusZ(HeapAllocator, &HeapAllocator::Instance, size, T, __VA_ARGS__)
#define HeapNewNoThrowPlusPrefixZ(size, T, ...) AllocatorNewNoThrowPlusPrefixZ(HeapAllocator, &HeapAllocator::Instance, size, T, __VA_ARGS__)
#define HeapNewNoThrowStruct(T) AllocatorNewNoThrowStruct(HeapAllocator, &HeapAllocator::Instance, T)
#define HeapNewNoThrowStructZ(T) AllocatorNewNoThrowStructZ(HeapAllocator, &HeapAllocator::Instance, T)
#define HeapNewNoThrowArray(T, count, ...) AllocatorNewNoThrowArray(HeapAllocator, &HeapAllocator::Instance, T, count)
#define HeapNewNoThrowArrayZ(T, count, ...) AllocatorNewNoThrowArrayZ(HeapAllocator, &HeapAllocator::Instance, T, count)

#define NoMemProtectHeapNewNoThrow(T, ...) AllocatorNewNoThrow(HeapAllocator, HeapAllocator::GetNoMemProtectInstance(), T, __VA_ARGS__)
#define NoMemProtectHeapNewNoThrowZ(T, ...) AllocatorNewNoThrowZ(HeapAllocator, HeapAllocator::GetNoMemProtectInstance(), T, __VA_ARGS__)
#define NoMemProtectHeapNewNoThrowPlus(size, T, ...) AllocatorNewNoThrowPlus(HeapAllocator, HeapAllocator::GetNoMemProtectInstance(), size, T, __VA_ARGS__)
#define NoMemProtectHeapNewNoThrowPlusZ(size, T, ...) AllocatorNewNoThrowPlusZ(HeapAllocator, HeapAllocator::GetNoMemProtectInstance(), size, T, __VA_ARGS__)
#define NoMemProtectHeapNewNoThrowPlusPrefixZ(size, T, ...) AllocatorNewNoThrowPlusPrefixZ(HeapAllocator, HeapAllocator::GetNoMemProtectInstance(), size, T, __VA_ARGS__)
#define NoMemProtectHeapNewNoThrowStruct(T) AllocatorNewNoThrowStruct(HeapAllocator, HeapAllocator::GetNoMemProtectInstance(), T)
#define NoMemProtectHeapNewNoThrowStructZ(T) AllocatorNewNoThrowStructZ(HeapAllocator, HeapAllocator::GetNoMemProtectInstance(), T)
#define NoMemProtectHeapNewNoThrowArray(T, count, ...) AllocatorNewNoThrowArray(HeapAllocator, HeapAllocator::GetNoMemProtectInstance(), T, count)
#define NoMemProtectHeapNewNoThrowArrayZ(T, count, ...) AllocatorNewNoThrowArrayZ(HeapAllocator, HeapAllocator::GetNoMemProtectInstance(), T, count)
#define NoMemProtectHeapDelete(obj) AllocatorDelete(HeapAllocator, HeapAllocator::GetNoMemProtectInstance(), obj)
#define NoMemProtectHeapDeletePlus(size, obj) AllocatorDeletePlus(HeapAllocator, HeapAllocator::GetNoMemProtectInstance(), size, obj)
#define NoMemProtectHeapDeletePlusPrefix(size, obj) AllocatorDeletePlusPrefix(HeapAllocator, HeapAllocator::GetNoMemProtectInstance(), size, obj)
#define NoMemProtectHeapDeleteArray(count, obj) AllocatorDeleteArray(HeapAllocator, HeapAllocator::GetNoMemProtectInstance(), count, obj)

#define NoCheckHeapNew(T, ...) AllocatorNew(NoCheckHeapAllocator, &NoCheckHeapAllocator::Instance, T, __VA_ARGS__)
#define NoCheckHeapNewZ(T, ...) AllocatorNewZ(NoCheckHeapAllocator, &NoCheckHeapAllocator::Instance, T, __VA_ARGS__)
#define NoCheckHeapNewPlus(size, T, ...) AllocatorNewPlus(NoCheckHeapAllocator, &NoCheckHeapAllocator::Instance, size, T, __VA_ARGS__)
#define NoCheckHeapNewPlusZ(size, T, ...) AllocatorNewPlusZ(NoCheckHeapAllocator, &NoCheckHeapAllocator::Instance, size, T, __VA_ARGS__)
#define NoCheckHeapNewStruct(T) AllocatorNewStruct(NoCheckHeapAllocator, &NoCheckHeapAllocator::Instance, T)
#define NoCheckHeapNewStructZ(T) AllocatorNewStructZ(NoCheckHeapAllocator, &NoCheckHeapAllocator::Instance, T)
#define NoCheckHeapNewArray(T, count, ...) AllocatorNewArray(NoCheckHeapAllocator, &NoCheckHeapAllocator::Instance, T, count)
#define NoCheckHeapNewArrayZ(T, count, ...) AllocatorNewArrayZ(NoCheckHeapAllocator, &NoCheckHeapAllocator::Instance, T, count)
#define NoCheckHeapDelete(obj) AllocatorDelete(NoCheckHeapAllocator, &NoCheckHeapAllocator::Instance, obj)
#define NoCheckHeapDeletePlus(size, obj) AllocatorDeletePlus(NoCheckHeapAllocator, &NoCheckHeapAllocator::Instance, size, obj)
#define NoCheckHeapDeleteArray(count, obj) AllocatorDeleteArray(NoCheckHeapAllocator, &NoCheckHeapAllocator::Instance, count, obj)

namespace Memory
{
#ifdef HEAP_TRACK_ALLOC

struct HeapAllocatorData;
struct HeapAllocRecord
{
    HeapAllocRecord * prev;
    HeapAllocRecord * next;
    size_t            allocId;
    size_t            size;
    TrackAllocData    allocData;
    HeapAllocatorData* data;
#if defined(CHECK_MEMORY_LEAK) || defined(LEAK_REPORT)
    StackBackTrace * stacktrace;
#endif
};
struct HeapAllocatorData
{
    void LogAlloc(HeapAllocRecord * record, size_t requestedBytes, TrackAllocData const& data);
    void LogFree(HeapAllocRecord * record);

    bool CheckLeaks();

    HeapAllocRecord * head;
    size_t allocCount;
    size_t deleteCount;
    size_t outstandingBytes;

    static uint const StackTraceDepth = 10;
};
#endif

struct HeapAllocator
{
    static const bool FakeZeroLengthArray = false;

    char * Alloc(size_t byteSize)
    {
        return AllocT<false>(byteSize);
    }
    template <bool noThrow>
    char * AllocT(size_t byteSize);

    // This exists solely to make the AllocateXXX macros more polymorphic
    char * AllocLeaf(size_t byteSize)
    {
        return Alloc(byteSize);
    }

    char * NoThrowAlloc(size_t byteSize)
    {
        return AllocT<true>(byteSize);
    }

    char * AllocZero(size_t byteSize)
    {
        char * buffer = Alloc(byteSize);
        memset(buffer, 0, byteSize);
        return buffer;
    }

    char * NoThrowAllocZero(size_t byteSize)
    {
        char * buffer = NoThrowAlloc(byteSize);
        if (buffer != nullptr)
        {
            memset(buffer, 0, byteSize);
        }
        return buffer;
    }
    void Free(void * buffer, size_t byteSize);

    static HeapAllocator Instance;
    static HeapAllocator * GetNoMemProtectInstance();


#ifdef TRACK_ALLOC
    // Doesn't support tracking information, dummy implementation
    HeapAllocator * TrackAllocInfo(TrackAllocData const& data);
    void ClearTrackAllocInfo(TrackAllocData* data = NULL);

#ifdef HEAP_TRACK_ALLOC
#ifndef INTERNAL_MEM_PROTECT_HEAP_ALLOC
    ~HeapAllocator();
#endif

    static void InitializeThread()
    {
        memset(&nextAllocData, 0, sizeof(nextAllocData));
    }

    static bool CheckLeaks();

    __declspec(thread) static TrackAllocData nextAllocData;
    HeapAllocatorData data;
    static CriticalSection cs;
#endif
#endif

#ifdef INTERNAL_MEM_PROTECT_HEAP_ALLOC
    HeapAllocator(bool allowMemProtect = true);
    ~HeapAllocator();

    void FinishMemProtectHeapCollect();

private:
    bool DoUseMemProtectHeap();
    static HeapAllocator NoMemProtectInstance;
#if DBG
    bool isUsed;
    bool allocMemProtect;
    void * memProtectHeapHandle;
#endif
#endif
}; // HeapAllocator.

class NoThrowHeapAllocator
{
public:
    static const bool FakeZeroLengthArray = false;
    char * Alloc(size_t byteSize);
    char * AllocZero(size_t byteSize);
    void Free(void * buffer, size_t byteSize);
    static NoThrowHeapAllocator Instance;

#ifdef TRACK_ALLOC
    // Doesn't support tracking information, dummy implementation
    NoThrowHeapAllocator * TrackAllocInfo(TrackAllocData const& data);
    void ClearTrackAllocInfo(TrackAllocData* data = NULL);
#endif
};

#ifdef INTERNAL_MEM_PROTECT_HEAP_ALLOC


class NoThrowNoMemProtectHeapAllocator
{
public:
    static const bool FakeZeroLengthArray = false;
    char * Alloc(size_t byteSize);
    char * AllocZero(size_t byteSize);
    void Free(void * buffer, size_t byteSize);
    static NoThrowNoMemProtectHeapAllocator Instance;

#ifdef TRACK_ALLOC
    // Doesn't support tracking information, dummy implementation
    NoThrowNoMemProtectHeapAllocator * TrackAllocInfo(TrackAllocData const& data);
    void ClearTrackAllocInfo(TrackAllocData* data = NULL);
#endif
};
#endif

class NoCheckHeapAllocator
{
public:
    static const bool FakeZeroLengthArray = false;
    char * Alloc(size_t byteSize)
    {
        if (processHeap == NULL)
        {
            processHeap = GetProcessHeap();
        }
        char * buffer = (char*)HeapAlloc(processHeap, 0, byteSize);
        if (buffer == nullptr)
        {
            // NoCheck heap allocator is only used by debug only code, and if we fail to allocate
            // memory, we will just raise an exception and kill the process
            DebugHeap_OOM_fatal_error();
        }
        return buffer;
    }
    char * AllocZero(size_t byteSize)
    {
        if (processHeap == NULL)
        {
            processHeap = GetProcessHeap();
        }
        char * buffer = (char*)HeapAlloc(processHeap, HEAP_ZERO_MEMORY, byteSize);
        if (buffer == nullptr)
        {
            // NoCheck heap allocator is only used by debug only code, and if we fail to allocate
            // memory, we will just raise an exception and kill the process
            DebugHeap_OOM_fatal_error();
        }
        return buffer;
    }
    void Free(void * buffer, size_t byteSize)
    {
        Assert(processHeap != NULL);
        HeapFree(processHeap, 0, buffer);
    }

#ifdef TRACK_ALLOC
    // Doesn't support tracking information, dummy implementation
    NoCheckHeapAllocator * TrackAllocInfo(TrackAllocData const& data) { return this; }
    void ClearTrackAllocInfo(TrackAllocData* data = NULL) {}
#endif
    static NoCheckHeapAllocator Instance;
    static HANDLE processHeap;
};

#ifdef CHECK_MEMORY_LEAK
class MemoryLeakCheck
{
public:
    MemoryLeakCheck() : head(NULL), tail(NULL), leakedBytes(0), leakedCount(0), enableOutput(true) {}
    ~MemoryLeakCheck();
    static void AddLeakDump(wchar_t const * dump, size_t bytes, size_t count);
    static void SetEnableOutput(bool flag) { leakCheck.enableOutput = flag; }
    static bool IsEnableOutput() { return leakCheck.enableOutput; }
private:
    static MemoryLeakCheck leakCheck;

    struct LeakRecord
    {
        wchar_t const * dump;
        LeakRecord * next;
    };

    CriticalSection cs;
    LeakRecord * head;
    LeakRecord * tail;
    size_t leakedBytes;
    size_t leakedCount;

    bool enableOutput;
};
#endif
} // namespace Memory

#ifdef INTERNAL_MEM_PROTECT_HEAP_ALLOC
//----------------------------------------
// NoThrowNoMemProtectHeapAllocator overrides
//----------------------------------------
template <>
_Ret_maybenull_ __inline void * __cdecl
operator new(size_t byteSize, NoThrowNoMemProtectHeapAllocator * alloc, char * (NoThrowNoMemProtectHeapAllocator::*AllocFunc)(size_t))
{
    return ::operator new(byteSize, alloc, true, AllocFunc);
}

template <>
_Ret_maybenull_ __inline void * __cdecl
operator new[](size_t byteSize, NoThrowNoMemProtectHeapAllocator * alloc, char * (NoThrowNoMemProtectHeapAllocator::*AllocFunc)(size_t))
{
    return ::operator new[](byteSize, alloc, true, AllocFunc);
}

template <>
_Ret_maybenull_ __inline void * __cdecl
operator new(size_t byteSize, NoThrowNoMemProtectHeapAllocator * alloc, char * (NoThrowNoMemProtectHeapAllocator::*AllocFunc)(size_t), size_t plusSize)
{
    return ::operator new(byteSize, alloc, true, AllocFunc, plusSize);
}

inline void __cdecl
operator delete(void * obj, NoThrowNoMemProtectHeapAllocator * alloc, char * (NoThrowNoMemProtectHeapAllocator::*AllocFunc)(size_t))
{
    alloc->Free(obj, (size_t)-1);
}

inline void __cdecl
operator delete(void * obj, NoThrowNoMemProtectHeapAllocator * alloc, char * (NoThrowNoMemProtectHeapAllocator::*AllocFunc)(size_t), size_t plusSize)
{
    alloc->Free(obj, (size_t)-1);
}
#else
typedef NoThrowHeapAllocator NoThrowNoMemProtectHeapAllocator;
#endif

//----------------------------------------
// Default operator new/delete overrides
//----------------------------------------
#if !defined(USED_IN_STATIC_LIB)
_Ret_maybenull_ void * __cdecl operator new(size_t byteSize);
_Ret_maybenull_ void * __cdecl operator new[](size_t byteSize);
void __cdecl operator delete(void * obj);
void __cdecl operator delete[](void * obj);
#endif

//----------------------------------------
// HeapAllocator overrides
//----------------------------------------
inline void __cdecl
operator delete(void * obj, HeapAllocator * alloc, char * (HeapAllocator::*AllocFunc)(size_t))
{
    alloc->Free(obj, (size_t)-1);
}

inline void __cdecl
operator delete(void * obj, HeapAllocator * alloc, char * (HeapAllocator::*AllocFunc)(size_t), size_t plusSize)
{
    alloc->Free(obj, (size_t)-1);
}

//----------------------------------------
// NoThrowHeapAllocator overrides
//----------------------------------------
template <>
_Ret_maybenull_ __inline void * __cdecl
operator new(size_t byteSize, NoThrowHeapAllocator * alloc, char * (NoThrowHeapAllocator::*AllocFunc)(size_t))
{
    return ::operator new(byteSize, alloc, true, AllocFunc);
}

template <>
_Ret_maybenull_ __inline void * __cdecl
operator new[](size_t byteSize, NoThrowHeapAllocator * alloc, char * (NoThrowHeapAllocator::*AllocFunc)(size_t))
{
    return ::operator new[](byteSize, alloc, true, AllocFunc);
}

template <>
_Ret_maybenull_ __inline void * __cdecl
operator new(size_t byteSize, NoThrowHeapAllocator * alloc, char * (NoThrowHeapAllocator::*AllocFunc)(size_t), size_t plusSize)
{
    return ::operator new(byteSize, alloc, true, AllocFunc, plusSize);
}

inline void __cdecl
operator delete(void * obj, NoThrowHeapAllocator * alloc, char * (NoThrowHeapAllocator::*AllocFunc)(size_t))
{
    alloc->Free(obj, (size_t)-1);
}

inline void __cdecl
operator delete(void * obj, NoThrowHeapAllocator * alloc, char * (NoThrowHeapAllocator::*AllocFunc)(size_t), size_t plusSize)
{
    alloc->Free(obj, (size_t)-1);
}


template <>
_Ret_notnull_ __inline void * __cdecl
operator new(size_t byteSize, NoCheckHeapAllocator * alloc, char * (NoCheckHeapAllocator::*AllocFunc)(size_t))
{
    Assert(byteSize != 0);
    void * buffer = (alloc->*AllocFunc)(byteSize);
    return buffer;
}


template <>
_Ret_notnull_ __inline void * __cdecl
operator new(size_t byteSize, NoCheckHeapAllocator * alloc, char * (NoCheckHeapAllocator::*AllocFunc)(size_t), size_t plusSize)
{
    Assert(byteSize != 0);
    Assert(plusSize != 0);
    void * buffer = (alloc->*AllocFunc)(AllocSizeMath::Add(byteSize, plusSize));
    return buffer;
}


_Ret_notnull_ __inline void * __cdecl
operator new[](size_t byteSize, NoCheckHeapAllocator * alloc, char * (NoCheckHeapAllocator::*AllocFunc)(size_t))
{
    void * buffer = (alloc->*AllocFunc)(byteSize);
    return buffer;
}

inline void __cdecl
operator delete(void * obj, NoCheckHeapAllocator * alloc, char * (NoCheckHeapAllocator::*AllocFunc)(size_t))
{
    alloc->Free(obj, (size_t)-1);
}

inline void __cdecl
operator delete(void * obj, NoCheckHeapAllocator * alloc, char * (NoCheckHeapAllocator::*AllocFunc)(size_t), size_t plusSize)
{
    alloc->Free(obj, (size_t)-1);
}
