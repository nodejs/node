//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Memory
{
template <typename T>
class RecyclerRootPtr
{
public:
    RecyclerRootPtr() : ptr(nullptr) {};
    ~RecyclerRootPtr() { Assert(ptr == nullptr); }
    void Root(T * ptr, Recycler * recycler) { Assert(this->ptr == nullptr); recycler->RootAddRef(ptr); this->ptr = ptr; }
    void Unroot(Recycler * recycler) { Assert(this->ptr != nullptr); recycler->RootRelease(this->ptr); this->ptr = nullptr; }

    T * operator->() const { Assert(ptr != nullptr); return ptr; }
    operator T*() const { return ptr; }
protected:
    T * ptr;
private:
    RecyclerRootPtr(const RecyclerRootPtr<T>& ptr); // Disable
    RecyclerRootPtr& operator=(RecyclerRootPtr<T> const& ptr); // Disable
};

typedef RecyclerRootPtr<void> RecyclerRootVar;

template <typename T>
class AutoRecyclerRootPtr : public RecyclerRootPtr<T>
{
public:
    AutoRecyclerRootPtr(T * ptr, Recycler * recycler) : recycler(recycler)
    {
        Root(ptr);
    }
    ~AutoRecyclerRootPtr()
    {
        Unroot();
    }

    void Root(T * ptr)
    {
        Unroot();
        __super::Root(ptr, recycler);
    }
    void Unroot()
    {
        if (ptr != nullptr)
        {
            __super::Unroot(recycler);
        }
    }
    Recycler * GetRecycler() const
    {
        return recycler;
    }
private:
    Recycler * const recycler;
};

typedef AutoRecyclerRootPtr<void> AutoRecyclerRootVar;
}
