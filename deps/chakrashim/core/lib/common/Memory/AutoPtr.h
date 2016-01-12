//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

template <typename T, typename HeapAllocatorT = HeapAllocator>
class AutoPtr : public BasePtr<T>
{
public:
    AutoPtr(T * ptr) : BasePtr(ptr) {}
    ~AutoPtr()
    {
        Clear();
    }

    AutoPtr& operator=(T * ptr)
    {
        Clear();
        this->ptr = ptr;
        return *this;
    }

private:
    void Clear()
    {
        if (ptr != nullptr)
        {
            AllocatorDelete(HeapAllocatorT, &HeapAllocatorT::Instance, ptr);
            ptr = nullptr;
        }
    }
};

template <typename T>
class AutoArrayPtr : public BasePtr<T>
{
protected:
    size_t m_elementCount;
public:
    AutoArrayPtr(T * ptr, size_t elementCount) : BasePtr(ptr), m_elementCount(elementCount) {}
    ~AutoArrayPtr()
    {
        Clear();
    }

    void Set(T* ptr, int elementCount)
    {
        Clear();
        this->ptr = ptr;
        this->m_elementCount = elementCount;
    }

private:
    void Clear()
    {
        if (ptr != nullptr)
        {
            HeapDeleteArray(m_elementCount, ptr);
            ptr = nullptr;
        }
    }
};

template <typename T>
class AutoArrayAndItemsPtr : public AutoArrayPtr<T>
{
public:
    AutoArrayAndItemsPtr(T * ptr, size_t elementCount) : AutoArrayPtr(ptr, elementCount) {}

    ~AutoArrayAndItemsPtr()
    {
        Clear();
    }

private:
    void Clear()
    {
        if (ptr != nullptr){
            for (size_t i = 0; i < this->m_elementCount; i++)
            {
                if (ptr[i] != nullptr)
                {
                    ptr[i]->CleanUp();
                    ptr[i] = nullptr;
                }
            }

            HeapDeleteArray(m_elementCount, ptr);
            ptr = nullptr;
        }
    }
};

template  <typename T>
class AutoReleasePtr : public BasePtr<T>
{
public:
    AutoReleasePtr(T * ptr = nullptr) : BasePtr(ptr) {}
    ~AutoReleasePtr()
    {
        Release();
    }

    void Release()
    {
        if (ptr != nullptr)
        {
            ptr->Release();
            this->ptr = nullptr;
        }
    }
};

template < typename T>
class AutoCOMPtr : public AutoReleasePtr<T>
{
public:
    AutoCOMPtr(T * ptr = nullptr) : AutoReleasePtr(ptr)
    {
        if (ptr != nullptr)
        {
            ptr->AddRef();
        }
    }
};

class AutoBSTR : public BasePtr<OLECHAR>
{
public:
    AutoBSTR(BSTR ptr = nullptr) : BasePtr(ptr) {}
    ~AutoBSTR()
    {
        Release();
    }

    void Release()
    {
        if (ptr != nullptr)
        {
            ::SysFreeString(ptr);
            this->ptr = nullptr;
        }
    }
};

template <typename T>
class AutoDiscardPTR : public BasePtr < T >
{
public:
    AutoDiscardPTR(T * ptr) : BasePtr(ptr) {}
    ~AutoDiscardPTR()
    {
        Clear();
    }

    AutoDiscardPTR& operator=(T * ptr)
    {
        Clear();
        this->ptr = ptr;
        return *this;
    }

private:
    void Clear()
    {
        if (ptr != nullptr)
        {
            ptr->Discard();
            ptr = nullptr;
        }
    }
};
