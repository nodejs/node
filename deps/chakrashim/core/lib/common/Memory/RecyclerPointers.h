//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Memory
{
template <typename T>
class NoWriteBarrierField
{
public:
    NoWriteBarrierField() {}
    NoWriteBarrierField(T const& value) : value(value) {}

    // Getters
    operator T const&() const { return value; }
    operator T&() { return value; }

    T const* AddressOf() const { return &value; }
    T* AddressOf() { return &value; }

    // Setters
    NoWriteBarrierField& operator=(T const& value)
    {
        this->value = value;
        return *this;
    }
private:
    T value;
};

template <typename T>
class NoWriteBarrierPtr
{
public:
    NoWriteBarrierPtr() {}
    NoWriteBarrierPtr(T * value) : value(value) {}

    // Getters
    T * operator->() const { return this->value; }
    operator T*() const { return this->value; }

    // Setters
    NoWriteBarrierPtr& operator=(T const& value)
    {
        this->value = value;
        return *this;
    }
private:
    T * value;
};

template <typename T>
class WriteBarrierObjectConstructorTrigger
{
public:
    WriteBarrierObjectConstructorTrigger(T* object, Recycler* recycler):
        object((char*) object),
        recycler(recycler)
    {
    }

    ~WriteBarrierObjectConstructorTrigger()
    {
        // WriteBarrier-TODO: trigger write barrier if the GC is in concurrent mark state
    }

    operator T*()
    {
        return object;
    }

private:
    T* object;
    Recycler* recycler;
};

template <typename T>
class WriteBarrierPtr
{
public:
    WriteBarrierPtr() {}
    WriteBarrierPtr(T * ptr)
    {
        // WriteBarrier
        NoWriteBarrierSet(ptr);
    }

    // Getters
    T * operator->() const { return ptr; }
    operator T*() const { return ptr; }

    // Setters
    WriteBarrierPtr& operator=(T * ptr)
    {
        WriteBarrierSet(ptr);
        return *this;
    }
    void NoWriteBarrierSet(T * ptr)
    {
        this->ptr = ptr;
    }
    void WriteBarrierSet(T * ptr)
    {
        NoWriteBarrierSet(ptr);
#ifdef RECYCLER_WRITE_BARRIER
        RecyclerWriteBarrierManager::WriteBarrier(this);
#endif
    }

    WriteBarrierPtr& operator=(WriteBarrierPtr const& other)
    {
        WriteBarrierSet(other.ptr);
        return *this;
    }

    static void MoveArray(WriteBarrierPtr * dst, WriteBarrierPtr * src, size_t count)
    {
        memmove((void *)dst, src, sizeof(WriteBarrierPtr) * count);
#ifdef RECYCLER_WRITE_BARRIER
        RecyclerWriteBarrierManager::WriteBarrier(dst, count);
#endif
    }
    static void CopyArray(WriteBarrierPtr * dst, size_t dstCount, T const* src, size_t srcCount)
    {
        js_memcpy_s((void *)dst, sizeof(WriteBarrierPtr) * dstCount, src, sizeof(T *) * srcCount);
#ifdef RECYCLER_WRITE_BARRIER
        RecyclerWriteBarrierManager::WriteBarrier(dst, dstCount);
#endif
    }
    static void CopyArray(WriteBarrierPtr * dst, size_t dstCount, WriteBarrierPtr const* src, size_t srcCount)
    {
        js_memcpy_s((void *)dst, sizeof(WriteBarrierPtr) * dstCount, src, sizeof(WriteBarrierPtr) * srcCount);
#ifdef RECYCLER_WRITE_BARRIER
        RecyclerWriteBarrierManager::WriteBarrier(dst, dstCount);
#endif
    }
    static void ClearArray(WriteBarrierPtr * dst, size_t count)
    {
        // assigning NULL don't need write barrier, just cast it and null it out
        memset((void *)dst, 0, sizeof(WriteBarrierPtr<T>) * count);
    }
private:
    T * ptr;
};
}

template<class T> inline
const T& min(const T& a, const NoWriteBarrierField<T>& b) { return a < b ? a : b; }

template<class T> inline
const T& min(const NoWriteBarrierField<T>& a, const T& b) { return a < b ? a : b; }

template<class T> inline
const T& min(const NoWriteBarrierField<T>& a, const NoWriteBarrierField<T>& b) { return a < b ? a : b; }

template<class T> inline
const T& max(const NoWriteBarrierField<T>& a, const T& b) { return a > b ? a : b; }

// TODO: Add this method back once we figure out why OACR is tripping on it
template<class T> inline
const T& max(const T& a, const NoWriteBarrierField<T>& b) { return a > b ? a : b; }

template<class T> inline
const T& max(const NoWriteBarrierField<T>& a, const NoWriteBarrierField<T>& b) { return a > b ? a : b; }

// Disallow memcpy, memmove of WriteBarrierPtr

template <typename T>
void *  __cdecl memmove(_Out_writes_bytes_all_opt_(_Size) WriteBarrierPtr<T> * _Dst, _In_reads_bytes_opt_(_Size) const void * _Src, _In_ size_t _Size)
{
    CompileAssert(false);
}

template <typename T>
void __stdcall js_memcpy_s(__bcount(sizeInBytes) WriteBarrierPtr<T> *dst, size_t sizeInBytes, __bcount(count) const void *src, size_t count)
{
    CompileAssert(false);
}

template <typename T>
void *  __cdecl memset(_Out_writes_bytes_all_(_Size) WriteBarrierPtr<T> * _Dst, _In_ int _Val, _In_ size_t _Size)
{
    CompileAssert(false);
}
