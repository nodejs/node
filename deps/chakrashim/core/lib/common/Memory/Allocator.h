//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

// Disable the warning about no matching operator delete found, we don't need those for the Arena and Recycler
#pragma warning(disable:4291)

// Page heap mode is supported currently only in the Recycler
// Defining here so that other allocators can take advantage of this
// in the future
enum PageHeapMode
{
    PageHeapModeOff = 0,   // No Page heap
    PageHeapModeBlockStart = 1,   // Allocate the object at the beginning of the page
    PageHeapModeBlockEnd = 2    // Allocate the object at the end of the page
};

#if PROFILE_DICTIONARY
#include "DictionaryStats.h"
#endif

#if DBG || defined(RECYCLER_FREE_MEM_FILL)
#define DbgMemFill 0XFE
#endif

namespace Memory
{
#ifdef TRACK_ALLOC
struct TrackAllocData
{
    void Clear() { typeinfo = nullptr; plusSize = 0; count = 0; }
    bool IsEmpty() { return typeinfo == nullptr && plusSize == 0 && count == 0; }
    type_info const * GetTypeInfo() const { return typeinfo; }
    size_t GetPlusSize() const { return plusSize; }
    size_t GetCount() const { return count; }

    static TrackAllocData CreateTrackAllocData(type_info const& typeinfo, size_t size, size_t count, char const * const filename, DWORD line)
    {
        TrackAllocData data;
        data.typeinfo = &typeinfo;
        data.plusSize = size;
        data.count = count;
        data.filename = filename;
        data.line = line;

        return data;
    };

    type_info const * typeinfo;
    size_t plusSize;
    size_t count;
    char const * filename;
    DWORD line;
};

#define TRACK_ALLOC_INFO(alloc, T, AllocatorType, size, count) static_cast<AllocatorType *>((alloc)->TrackAllocInfo(TrackAllocData::CreateTrackAllocData(typeid(T), size, count, __FILE__, __LINE__)))
#else
#define TRACK_ALLOC_INFO(alloc, T, AllocatorType, size, count) static_cast<AllocatorType *>(alloc)
#endif

#ifdef HEAP_ENUMERATION_VALIDATION
namespace Js {
    class DynamicObject;
};
extern void PostAllocationCallbackForHeapEnumValidation(const type_info&, Js::DynamicObject*);
template <typename T>
inline T* PostAllocationCallback(const type_info& objType, T *obj)
{
    if (__is_base_of(Js::DynamicObject, T))
    {
        PostAllocationCallbackForHeapEnumValidation(objType, (Js::DynamicObject*)obj);
    }
    return obj;
}
#define VALIDATE_OBJECT(T, obj) (PostAllocationCallback(typeid(T), obj))
#else
#define VALIDATE_OBJECT(T, obj) obj
#endif

// Any allocator
#define AllocatorNewBase(AllocatorType, alloc, AllocFunc, T, ...) VALIDATE_OBJECT(T, new (TRACK_ALLOC_INFO(static_cast<AllocatorType *>(alloc), T, AllocatorType, 0, (size_t)-1), &AllocatorType::AllocFunc) T(__VA_ARGS__))
#define AllocatorNewPlusBase(AllocatorType, alloc, AllocFunc, size, T, ...) VALIDATE_OBJECT(T, new (TRACK_ALLOC_INFO(static_cast<AllocatorType *>(alloc), T, AllocatorType, size, (size_t)-1), &AllocatorType::AllocFunc, size) T(__VA_ARGS__))
#define AllocatorNewArrayBase(AllocatorType, alloc, AllocFunc, T, count) AllocateArray<AllocatorType, T, false>(TRACK_ALLOC_INFO(alloc, T, AllocatorType, 0, count), &AllocatorType::AllocFunc, count)
#define AllocatorNewStructBase(AllocatorType, alloc, AllocFunc, T) new (TRACK_ALLOC_INFO(static_cast<AllocatorType *>(alloc), T, AllocatorType, 0, (size_t)-1), &AllocatorType::AllocFunc) T
#define AllocatorNewStructPlusBase(AllocatorType, alloc, AllocFunc, size, T) new (TRACK_ALLOC_INFO(static_cast<AllocatorType *>(alloc), T, AllocatorType, size, (size_t)-1), &AllocatorType::AllocFunc, size) T

#define AllocatorNew(AllocatorType, alloc, T, ...) AllocatorNewBase(AllocatorType, alloc, Alloc, T, __VA_ARGS__)
#define AllocatorNewLeaf(AllocatorType, alloc, T, ...) AllocatorNewBase(AllocatorType, alloc, AllocLeaf, T, __VA_ARGS__)
#define AllocatorNewZ(AllocatorType, alloc, T, ...) AllocatorNewBase(AllocatorType, alloc, AllocZero, T, __VA_ARGS__)
#define AllocatorNewLeafZ(AllocatorType, alloc, T, ...) AllocatorNewBase(AllocatorType, alloc, AllocLeafZero, T, __VA_ARGS__)
#define AllocatorNewPlus(AllocatorType, alloc, size, T, ...) AllocatorNewPlusBase(AllocatorType, alloc, Alloc, size, T, __VA_ARGS__)
#define AllocatorNewPlusLeaf(AllocatorType, alloc, size, T, ...) AllocatorNewPlusBase(AllocatorType, alloc, AllocLeaf, size, T, __VA_ARGS__)
#define AllocatorNewPlusLeafZ(AllocatorType, alloc, size, T, ...) AllocatorNewPlusBase(AllocatorType, alloc, AllocLeafZero, size, T, __VA_ARGS__)
#define AllocatorNewPlusZ(AllocatorType, alloc, size, T, ...)  AllocatorNewPlusBase(AllocatorType, alloc, AllocZero, size, T, __VA_ARGS__)
#define AllocatorNewStruct(AllocatorType, alloc, T) AllocatorNewStructBase(AllocatorType, alloc, Alloc, T)
#define AllocatorNewStructZ(AllocatorType, alloc, T) AllocatorNewStructBase(AllocatorType, alloc, AllocZero, T)
#define AllocatorNewStructLeaf(AllocatorType, alloc, T) AllocatorNewStructBase(AllocatorType, alloc, AllocLeaf, T)
#define AllocatorNewStructLeafZ(AllocatorType, alloc, T) AllocatorNewStructBase(AllocatorType, alloc, AllocLeafZero, T)
#define AllocatorNewStructPlus(AllocatorType, alloc, size, T) AllocatorNewStructPlusBase(AllocatorType, alloc, Alloc, size, T)
#define AllocatorNewStructPlusZ(AllocatorType, alloc, size, T) AllocatorNewStructPlusBase(AllocatorType, alloc, AllocZero, size, T)
#define AllocatorNewStructPlusLeaf(AllocatorType, alloc, size, T) AllocatorNewStructPlusBase(AllocatorType, alloc, AllocLeaf, size, T)
#define AllocatorNewStructPlusLeafZ(AllocatorType, alloc, size, T) AllocatorNewStructPlusBase(AllocatorType, alloc, AllocLeafZero, size, T);
#define AllocatorNewArray(AllocatorType, alloc, T, count) AllocatorNewArrayBase(AllocatorType, alloc, Alloc, T, count)
#define AllocatorNewArrayLeaf(AllocatorType, alloc, T, count) AllocatorNewArrayBase(AllocatorType, alloc, AllocLeaf, T, count)
#define AllocatorNewArrayZ(AllocatorType, alloc, T, count) AllocatorNewArrayBase(AllocatorType, alloc, AllocZero, T, count)
#define AllocatorNewArrayLeafZ(AllocatorType, alloc, T, count) AllocatorNewArrayBase(AllocatorType, alloc, AllocLeafZero, T, count)

#define AllocatorNewNoThrowBase(AllocatorType, alloc, AllocFunc, T, ...) new (TRACK_ALLOC_INFO(static_cast<AllocatorType *>(alloc), T, AllocatorType, 0, (size_t)-1), true, &AllocatorType::NoThrow ## AllocFunc) T(__VA_ARGS__)
#define AllocatorNewNoThrowPlusBase(AllocatorType, alloc, AllocFunc, size, T, ...) new (TRACK_ALLOC_INFO(static_cast<AllocatorType *>(alloc), T, AllocatorType, size, (size_t)-1), true, &AllocatorType::NoThrow ## AllocFunc, size) T(__VA_ARGS__)
#define AllocatorNewNoThrowArrayBase(AllocatorType, alloc, AllocFunc, T, count) AllocateArray<AllocatorType, T, true>(TRACK_ALLOC_INFO(alloc, T, AllocatorType, 0, count), &AllocatorType::NoThrow ## AllocFunc, count)
#define AllocatorNewNoThrowStructBase(AllocatorType, alloc, AllocFunc, T) new (TRACK_ALLOC_INFO(static_cast<AllocatorType *>(alloc), T, AllocatorType, 0, (size_t)-1), true, &AllocatorType::NoThrow ## AllocFunc) T
#define AllocatorNewNoThrowStructPlusBase(AllocatorType, alloc, AllocFunc, size, T) new (TRACK_ALLOC_INFO(static_cast<AllocatorType *>(alloc), T, AllocatorType, size, (size_t)-1), true, &AllocatorType::NoThrow ## AllocFunc, size) T

#define AllocatorNewNoThrowPlusPrefixBase(AllocatorType, alloc, AllocFunc, size, T, ...) new (TRACK_ALLOC_INFO(static_cast<AllocatorType *>(alloc), T, AllocatorType, size, (size_t)-1), true, &AllocatorType::NoThrow ## AllocFunc, size, true) T(__VA_ARGS__)

#define AllocatorNewNoThrow(AllocatorType, alloc, T, ...) AllocatorNewNoThrowBase(AllocatorType, alloc, Alloc, T, __VA_ARGS__)
#define AllocatorNewNoThrowLeaf(AllocatorType, alloc, T, ...) AllocatorNewNoThrowBase(AllocatorType, alloc, AllocLeaf, T, __VA_ARGS__)
#define AllocatorNewNoThrowZ(AllocatorType, alloc, T, ...) AllocatorNewNoThrowBase(AllocatorType, alloc, AllocZero, T, __VA_ARGS__)
#define AllocatorNewNoThrowLeafZ(AllocatorType, alloc, T, ...) AllocatorNewNoThrowBase(AllocatorType, alloc, AllocLeafZero, T, __VA_ARGS__)
#define AllocatorNewNoThrowPlus(AllocatorType, alloc, size, T, ...) AllocatorNewNoThrowPlusBase(AllocatorType, alloc, Alloc, size, T, __VA_ARGS__)
#define AllocatorNewNoThrowPlusZ(AllocatorType, alloc, size, T, ...)  AllocatorNewNoThrowPlusBase(AllocatorType, alloc, AllocZero, size, T, __VA_ARGS__)
#define AllocatorNewNoThrowPlusPrefixZ(AllocatorType, alloc, size, T, ...)  AllocatorNewNoThrowPlusPrefixBase(AllocatorType, alloc, AllocZero, size, T, __VA_ARGS__)
#define AllocatorNewNoThrowStruct(AllocatorType, alloc, T) AllocatorNewNoThrowStructBase(AllocatorType, alloc, Alloc, T)
#define AllocatorNewNoThrowStructZ(AllocatorType, alloc, T) AllocatorNewNoThrowStructBase(AllocatorType, alloc, AllocZero, T)
#define AllocatorNewNoThrowStructPlus(AllocatorType, alloc, size, T) AllocatorNewNoThrowStructPlusBase(AllocatorType, alloc, Alloc, size, T)
#define AllocatorNewNoThrowStructPlusZ(AllocatorType, alloc, size, T) AllocatorNewNoThrowStructPlusBase(AllocatorType, alloc, AllocZero, size, T)
#define AllocatorNewNoThrowStructPlusLeaf(AllocatorType, alloc, size, T) AllocatorNewNoThrowStructPlusBase(AllocatorType, alloc, AllocLeaf, size, T)
#define AllocatorNewNoThrowStructPlusLeafZ(AllocatorType, alloc, size, T) AllocatorNewNoThrowStructPlusBase(AllocatorType, alloc, AllocLeafZero, size, T);
#define AllocatorNewNoThrowArray(AllocatorType, alloc, T, count) AllocatorNewNoThrowArrayBase(AllocatorType, alloc, Alloc, T, count)
#define AllocatorNewNoThrowArrayLeaf(AllocatorType, alloc, T, count) AllocatorNewNoThrowArrayBase(AllocatorType, alloc, AllocLeaf, T, count)
#define AllocatorNewNoThrowArrayZ(AllocatorType, alloc, T, count) AllocatorNewNoThrowArrayBase(AllocatorType, alloc, AllocZero, T, count)

#define AllocatorNewNoThrowNoRecoveryArrayBase(AllocatorType, alloc, AllocFunc, T, count) AllocateArray<AllocatorType, T, true>(TRACK_ALLOC_INFO(alloc, T, AllocatorType, 0, count), &AllocatorType::NoThrowNoRecovery ## AllocFunc, count)

#define AllocatorNewNoThrowNoRecoveryArrayZ(AllocatorType, alloc, T, count) AllocatorNewNoThrowNoRecoveryArrayBase(AllocatorType, alloc, AllocZero, T, count)

#define AllocatorDelete(AllocatorType, alloc, obj) DeleteObject<AllocatorType>(alloc, obj)
#define AllocatorDeleteInline(AllocatorType, alloc, obj) DeleteObjectInline<AllocatorType>(alloc, obj)
#define AllocatorDeleteLeaf(TAllocator, alloc, obj) DeleteObject<ForceLeafAllocator<TAllocator>::AllocatorType>(alloc, obj)
#define AllocatorDeletePlus(AllocatorType, alloc, size,  obj)  DeleteObject<AllocatorType>(alloc, obj, size);
#define AllocatorDeletePlusLeaf(TAllocator, alloc, size,  obj)  DeleteObject<ForceLeafAllocator<TAllocator>::AllocatorType>(alloc, obj, size);
#define AllocatorDeletePlusPrefix(AllocatorType, alloc, size,  obj)  DeleteObject<AllocatorType>(alloc, obj, size, true);
#define AllocatorDeletePlusPrefixLeaf(TAllocator, alloc, size,  obj)  DeleteObject<ForceLeafAllocator<TAllocator>::AllocatorType>(alloc, obj, size, true);
#define AllocatorDeleteArray(AllocatorType, alloc, count, obj) DeleteArray<AllocatorType>(alloc, count, obj)
#define AllocatorDeleteArrayLeaf(TAllocator, alloc, count, obj) DeleteArray<ForceLeafAllocator<TAllocator>::AllocatorType>(alloc, count, obj)

// Free routine where we don't care about following C++ semantics (e.g. calling the destructor)
#define AllocatorFree(alloc, freeFunc, obj, size) (alloc->*freeFunc)(obj, size)

// default type allocator implementation
template <typename TAllocator, typename T>
class TypeAllocatorFunc
{
public:
    typedef char * (TAllocator::*AllocFuncType)(size_t);
    typedef void(TAllocator::*FreeFuncType)(void*, size_t);

    static AllocFuncType GetAllocFunc()
    {
        return &TAllocator::Alloc;
    }

    static AllocFuncType GetAllocZeroFunc()
    {
        return &TAllocator::AllocZero;
    }

    static FreeFuncType GetFreeFunc()
    {
        return &TAllocator::Free;
    }
};

// List specific allocator info
template <typename TAllocator, bool isLeaf>
class ListTypeAllocatorFunc
{
public:
    typedef void(TAllocator::*FreeFuncType)(void*, size_t);

    static FreeFuncType GetFreeFunc()
    {
        return &TAllocator::Free;
    }
};

// Default allocation policy
template <typename TAllocator, typename TAllocType>
struct AllocatorInfo
{
    typedef TAllocator AllocatorType;
    typedef TAllocator TemplateAllocatorType;
    typedef TypeAllocatorFunc<TAllocator, TAllocType> AllocatorFunc;    // allocate/free an array of given type
    typedef TypeAllocatorFunc<TAllocator, TAllocType> InstAllocatorFunc;// allocate/free an object instance itself of given type
};

// Allocator doesn't change for by default for forcing non leaf
template <typename TAllocator>
struct ForceNonLeafAllocator
{
    typedef TAllocator AllocatorType;
};

// Allocator doesn't change for by default for forcing leaf
template <typename TAllocator>
struct ForceLeafAllocator
{
    typedef TAllocator AllocatorType;
};

template <typename TAllocator, typename T>
void DeleteObject(typename AllocatorInfo<TAllocator, T>::AllocatorType * allocator, T * obj)
{
    obj->~T();

    auto freeFunc = AllocatorInfo<TAllocator, T>::InstAllocatorFunc::GetFreeFunc(); // Use InstAllocatorFunc
    (allocator->*freeFunc)(obj, sizeof(T));
}

template <typename TAllocator, typename T>
void DeleteObjectInline(typename TAllocator * allocator, T * obj)
{
    obj->~T();
    allocator->FreeInline(obj, sizeof(T));
}

template <typename TAllocator, typename T>
void DeleteObject(typename AllocatorInfo<TAllocator, T>::AllocatorType * allocator, T * obj, size_t plusSize)
{
    obj->~T();

    // DeleteObject can only be called when an object is allocated successfully.
    // So the add should never overflow
    Assert(sizeof(T) + plusSize >= sizeof(T));

    auto freeFunc = AllocatorInfo<TAllocator, T>::InstAllocatorFunc::GetFreeFunc(); // Use InstAllocatorFunc
    (allocator->*freeFunc)(obj, sizeof(T) + plusSize);
}

template <typename TAllocator, typename T>
void DeleteObject(typename AllocatorInfo<TAllocator, T>::AllocatorType * allocator, T * obj, size_t plusSize, bool prefix)
{
    Assert(prefix);
    obj->~T();

    // DeleteObject can only be called when an object is allocated successfully.
    // So the add should never overflow
    Assert(sizeof(T) + plusSize >= sizeof(T));

    // NOTE: This may cause the object not be double aligned
    Assert(plusSize == Math::Align<size_t>(plusSize, sizeof(size_t)));

    auto freeFunc = AllocatorInfo<TAllocator, T>::InstAllocatorFunc::GetFreeFunc(); // Use InstAllocatorFunc
    (allocator->*freeFunc)(((char *)obj) - plusSize, sizeof(T) + plusSize);
}

#define ZERO_LENGTH_ARRAY (void *)sizeof(void *)
template <typename TAllocator, typename T, bool nothrow>
_When_(nothrow, _Ret_writes_to_maybenull_(count, 0)) _When_(!nothrow, _Ret_writes_to_(count, 0))
__inline T * AllocateArray(TAllocator * allocator, char * (TAllocator::*AllocFunc)(size_t), size_t count)
{
    if (count == 0 && TAllocator::FakeZeroLengthArray)
    {
#ifdef TRACK_ALLOC
        allocator->ClearTrackAllocInfo();
#endif
        // C++ standard requires allocator to return non-null if it isn't out of memory
        // Just return some small number so we will still AV if someone try to use the memory
        return (T *)ZERO_LENGTH_ARRAY;
    }
    if (nothrow)
    {
        return new (allocator, nothrow, AllocFunc) T[count];
    }
    return new (allocator, AllocFunc) T[count];
}

template <typename TAllocator, typename T>
void DeleteArray(typename AllocatorInfo<TAllocator, T>::AllocatorType * allocator, size_t count, T * obj)
{
    if(count == 0)
    {
        return;
    }

    for (size_t i = 0; i < count; i++)
    {
        obj[i].~T();
    }

    // DeleteArray can only be called when an array is allocated successfully.
    // So the add should never overflow
    Assert(count * sizeof(T) / count == sizeof(T));

    auto freeFunc = AllocatorInfo<TAllocator, T>::AllocatorFunc::GetFreeFunc();
    (allocator->*freeFunc)((void *)obj, sizeof(T) * count);
}

#define AllocatorFieldMove(dest, src, field) \
    Assert(dest->field == 0); \
    dest->field = src->field; \
    src->field = 0;

class Allocator
{
public:
    Allocator(void (*outOfMemoryFunc)(), void (*recoverMemoryFunc)() = JsUtil::ExternalApi::RecoverUnusedMemory) : outOfMemoryFunc(outOfMemoryFunc), recoverMemoryFunc(recoverMemoryFunc) {}
    void Move(Allocator *srcAllocator)
    {
        Assert(srcAllocator != nullptr);
        AllocatorFieldMove(this, srcAllocator, outOfMemoryFunc);
    }

    void (*outOfMemoryFunc)();
    void (*recoverMemoryFunc)();
};


template <typename T>
void AssertValue(void * mem, T value, uint byteCount)
{
#if DBG
    Assert(byteCount % sizeof(T) == 0);
    for (uint i = 0; i < byteCount; i += sizeof(T))
    {
        Assert(*(T *)(((char *)mem) + i) == value);
    }
#endif
}
}

// For the debugger extension, we don't need the placement news
#ifndef __PLACEMENT_NEW_INLINE
#define __PLACEMENT_NEW_INLINE

_Ret_notnull_
__inline void * __cdecl
operator new(
size_t byteSize,
_In_ void * previousAllocation) throw()
{
    return previousAllocation;
}


__inline  void __cdecl
operator delete(
void * allocationToFree,                // Allocation to free
void * previousAllocation               // Previously allocated memory
) throw()
{

}

#endif

//----------------------------------------
// throwing operator new overrides
//----------------------------------------
template <typename TAllocator>
_Ret_notnull_ void * __cdecl
operator new(size_t byteSize, TAllocator * alloc, char * (TAllocator::*AllocFunc)(size_t))
{
    AssertCanHandleOutOfMemory();
    Assert(byteSize != 0);
    void * buffer = (alloc->*AllocFunc)(byteSize);
    Assume(buffer != nullptr);
    return buffer;
}

template <typename TAllocator>
_Ret_notnull_ __inline void * __cdecl
operator new[](size_t byteSize, TAllocator * alloc, char * (TAllocator::*AllocFunc)(size_t))
{
    AssertCanHandleOutOfMemory();
    Assert(byteSize != 0 || !TAllocator::FakeZeroLengthArray);
    void * buffer = (alloc->*AllocFunc)(byteSize);
    Assume(buffer != nullptr);
    return buffer;
}

template <typename TAllocator>
_Ret_notnull_ __inline void * __cdecl
operator new(size_t byteSize, TAllocator * alloc, char * (TAllocator::*AllocFunc)(size_t), size_t plusSize)
{
    AssertCanHandleOutOfMemory();
    Assert(byteSize != 0);
    //Assert(plusSize != 0);
    // byteSize is usually a compile-time constant, so put it on the RHS of the add for
    // slightly better (smaller and faster) code.
    void * buffer = (alloc->*AllocFunc)(AllocSizeMath::Add(plusSize, byteSize));
    Assume(buffer != nullptr);
    return buffer;
}

//----------------------------------------
// nothrow operator new overrides
//----------------------------------------
template <typename TAllocator>
_Ret_maybenull_ __inline void * __cdecl
operator new(size_t byteSize, TAllocator * alloc, bool nothrow, char * (TAllocator::*AllocFunc)(size_t))
{
    Assert(nothrow);
    Assert(byteSize != 0);
    void * buffer = (alloc->*AllocFunc)(byteSize);
    return buffer;
}


template <typename TAllocator>
_Ret_maybenull_ __inline void * __cdecl
operator new[](size_t byteSize, TAllocator * alloc, bool nothrow, char * (TAllocator::*AllocFunc)(size_t))
{
    Assert(nothrow);
    Assert(byteSize != 0 || !TAllocator::FakeZeroLengthArray);
    void * buffer = (alloc->*AllocFunc)(byteSize);
    return buffer;
}


template <typename TAllocator>
_Ret_maybenull_ __inline void * __cdecl
operator new(size_t byteSize, TAllocator * alloc, bool nothrow, char * (TAllocator::*AllocFunc)(size_t), size_t plusSize)
{
    Assert(nothrow);
    Assert(byteSize != 0);
    //Assert(plusSize != 0);
    // byteSize is usually a compile-time constant, so put it on the RHS of the add for
    // slightly better (smaller and faster) code.
    void * buffer = (alloc->*AllocFunc)(AllocSizeMath::Add(plusSize, byteSize));
    return buffer;
}


template <typename TAllocator>
_Ret_maybenull_ __inline void * __cdecl
operator new(size_t byteSize, TAllocator * alloc, bool nothrow, char * (TAllocator::*AllocFunc)(size_t), size_t plusSize, bool prefix)
{
    Assert(nothrow);
    Assert(prefix);
    Assert(byteSize != 0);
    Assert(plusSize != 0);

    // NOTE: This may cause the object not be double aligned
    Assert(plusSize == Math::Align<size_t>(plusSize, sizeof(size_t)));

    // byteSize is usually a compile-time constant, so put it on the RHS of the add for
    // slightly better (smaller and faster) code.
    char * buffer = (alloc->*AllocFunc)(AllocSizeMath::Add(plusSize, byteSize));

    // This seems to generate the most compact code
    return buffer + (buffer > 0 ? plusSize : (size_t)buffer);
}
