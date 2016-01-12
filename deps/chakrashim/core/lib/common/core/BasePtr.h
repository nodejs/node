//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once


template <typename T>
class BasePtr
{
public:
    BasePtr(T * ptr = nullptr) : ptr(ptr) {}
    T ** operator&() { Assert(ptr == nullptr); return &ptr; }
    T * operator->() const { Assert(ptr != nullptr); return ptr; }
    operator T*() const { return ptr; }

    // Detach currently owned ptr. WARNING: This object no longer owns/manages the ptr.
    T * Detach()
    {
        T * ret = ptr;
        ptr = nullptr;
        return ret;
    }
protected:
    T * ptr;
private:
    BasePtr(const BasePtr<T>& ptr); // Disable
    BasePtr& operator=(BasePtr<T> const& ptr); // Disable
};
