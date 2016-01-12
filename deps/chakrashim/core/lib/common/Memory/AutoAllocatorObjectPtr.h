//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Memory
{
// This object ensures DeleteObject (AllocatorDelete) for an allocator allocated object.
template <typename T, typename TAllocator>
class AutoAllocatorObjectPtr : public BasePtr<T>
{
private:
    typedef typename AllocatorInfo<TAllocator, T>::AllocatorType AllocatorType;
    AllocatorType* m_allocator;

public:
    AutoAllocatorObjectPtr(T* ptr, AllocatorType* allocator) : BasePtr(ptr), m_allocator(allocator)
    {
        Assert(allocator);
    }

    ~AutoAllocatorObjectPtr()
    {
        Clear();
    }

private:
    void Clear()
    {
        if (ptr != nullptr)
        {
            DeleteObject<TAllocator>(m_allocator, ptr);
            ptr = nullptr;
        }
    }
};

// The version of AutoArrayPtr that uses allocator to release the memory.
template <typename T, typename TAllocator>
class AutoAllocatorArrayPtr : public BasePtr<T>
{
protected:
    typedef typename AllocatorInfo<TAllocator, T>::AllocatorType AllocatorType;
    size_t m_elementCount;
    AllocatorType* m_allocator;

public:
    AutoAllocatorArrayPtr(T * ptr, size_t elementCount, AllocatorType* allocator) : BasePtr(ptr), m_elementCount(elementCount), m_allocator(allocator)
    {
        Assert(allocator);
    }

    ~AutoAllocatorArrayPtr()
    {
        Clear();
    }

    // Do not support "operator=(T* ptr)". The new ptr may have a different elementCount.

private:
    void Clear()
    {
        if (ptr != nullptr)
        {
            DeleteArray<TAllocator>(m_allocator, m_elementCount, ptr);
            ptr = nullptr;
        }
    }
};

// This version of AutoArrayPtr points to an array of AllocatorObject pointers (T*). It ensures AllocatorDelete
// each AllocatorObject pointer contained in the array, before deleting the array itself.
//
// Template parameter:
//      T               The object type allocated from allocator. The array contains T*.
//      TAllocator      The allocator type used to allocate/free the objects.
//      ArrayAllocator  The allocator type used to allocate/free the array.
//
template <typename T, typename TAllocator, typename ArrayAllocator = ForceNonLeafAllocator<TAllocator>::AllocatorType>
class AutoAllocatorObjectArrayPtr : public AutoAllocatorArrayPtr<T*, ArrayAllocator>
{
public:
    AutoAllocatorObjectArrayPtr(T** ptr, size_t elementCount, AllocatorType* allocator) :
        AutoAllocatorArrayPtr(ptr, elementCount, allocator)
    {
    }

    ~AutoAllocatorObjectArrayPtr()
    {
        Clear();
    }

    // Do not support "operator=(T* ptr)". The new ptr may have a different elementCount.

private:
    void Clear()
    {
        if (ptr != nullptr)
        {
            for (size_t i = 0; i < m_elementCount; i++)
            {
                if (ptr[i] != nullptr)
                {
                    DeleteObject<TAllocator>(m_allocator, ptr[i]);
                    ptr[i] = nullptr;
                }
            }
        }
    }
};
}
