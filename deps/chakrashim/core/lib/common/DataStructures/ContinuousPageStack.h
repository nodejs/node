//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

// -----------------------------------------------------------------------------------------------------------------------------
// ContinuousPageStack declaration
// -----------------------------------------------------------------------------------------------------------------------------

template<const size_t InitialPageCount = 1>
class ContinuousPageStack : protected Allocator
{
protected:
    class Iterator
    {
    protected:
        size_t nextTop;

    protected:
        __inline Iterator(const ContinuousPageStack &stack);

    public:
        __inline size_t Position() const;
        __inline operator bool() const;
    };

    friend class Iterator;

private:
    PageAllocator *const pageAllocator;
    PageAllocation *pageAllocation;
    size_t bufferSize;

protected:
    size_t nextTop;

protected:
    __inline ContinuousPageStack(PageAllocator *const pageAllocator, void (*const outOfMemoryFunc)());
    ~ContinuousPageStack();

    __inline char *Buffer() const;

public:
    __inline bool IsEmpty() const;
    __inline void Clear();

    __inline size_t Position() const;

    __inline char* Push(const size_t size);
    __inline char* Top(const size_t size) const;
    __inline char* Pop(const size_t size);
    __inline void UnPop(const size_t size);
    __inline void PopTo(const size_t position);

private:
    void Resize(size_t requestedSize);
};

// -----------------------------------------------------------------------------------------------------------------------------
// ContinuousPageStackOfFixedElements declaration
// -----------------------------------------------------------------------------------------------------------------------------

template<class T, const size_t InitialPageCount = 1>
class ContinuousPageStackOfFixedElements : public ContinuousPageStack<InitialPageCount>
{
public:
    class Iterator : public ContinuousPageStack<InitialPageCount>::Iterator
    {
    private:
        const ContinuousPageStackOfFixedElements &stack;

    public:
        __inline Iterator(const ContinuousPageStackOfFixedElements &stack);

        __inline T &operator *() const;
        __inline T *operator ->() const;
        __inline Iterator &operator ++(); // pre-increment
        __inline Iterator operator ++(int); // post-increment
    };

    friend class Iterator;

public:
    __inline ContinuousPageStackOfFixedElements(PageAllocator *const pageAllocator, void (*const outOfMemoryFunc)());

public:
    __inline char* Push();
    __inline T* Top() const;
    __inline T* Pop();
    __inline void UnPop();
    __inline void PopTo(const size_t position);
};

// -----------------------------------------------------------------------------------------------------------------------------
// ContinuousPageStackOfVariableElements declaration
// -----------------------------------------------------------------------------------------------------------------------------

template<class T, const size_t InitialPageCount = 1>
class ContinuousPageStackOfVariableElements : public ContinuousPageStack<InitialPageCount>
{
public:
    class Iterator : public ContinuousPageStack<InitialPageCount>::Iterator
    {
    private:
        size_t topElementSize;
        const ContinuousPageStackOfVariableElements &stack;

    public:
        __inline Iterator(const ContinuousPageStackOfVariableElements &stack);

        __inline T &operator *() const;
        __inline T *operator ->() const;
        __inline Iterator &operator ++(); // pre-increment
        __inline Iterator operator ++(int); // post-increment
    };

    friend class Iterator;

private:
    class VariableElement
    {
    private:
        const size_t previousElementSize;
        char data[0];

    public:
        __inline VariableElement(const size_t previousElementSize);

    public:
        template<class ActualT> __inline static size_t Size();
        __inline size_t PreviousElementSize() const;
        __inline char *Data();
    };

private:
    size_t topElementSize;

public:
    __inline ContinuousPageStackOfVariableElements(PageAllocator *const pageAllocator, void (*const outOfMemoryFunc)());

public:
    template<class ActualT> __inline char* Push();
    __inline T* Top() const;
    __inline T* Pop();
    template<class ActualT> __inline void UnPop();
    __inline void PopTo(const size_t position);
};

// -----------------------------------------------------------------------------------------------------------------------------
// ContinuousPageStack definition
// -----------------------------------------------------------------------------------------------------------------------------

// --------
// Iterator
// --------

template<const size_t InitialPageCount>
__inline ContinuousPageStack<InitialPageCount>::Iterator::Iterator(const ContinuousPageStack &stack)
    : nextTop(stack.nextTop)
{
}

template<const size_t InitialPageCount>
__inline size_t ContinuousPageStack<InitialPageCount>::Iterator::Position() const
{
    return nextTop;
}

template<const size_t InitialPageCount>
__inline ContinuousPageStack<InitialPageCount>::Iterator::operator bool() const
{
    return nextTop != 0;
}

// -------------------
// ContinuousPageStack
// -------------------

template<const size_t InitialPageCount>
__inline ContinuousPageStack<InitialPageCount>::ContinuousPageStack(
    PageAllocator *const pageAllocator,
    void (*const outOfMemoryFunc)())
    : Allocator(outOfMemoryFunc), pageAllocator(pageAllocator), bufferSize(0), nextTop(0)
{
    Assert(pageAllocator);
}

template<const size_t InitialPageCount>
ContinuousPageStack<InitialPageCount>::~ContinuousPageStack()
{
    if(bufferSize && !pageAllocator->IsClosed())
        pageAllocator->ReleaseAllocation(pageAllocation);
}

template<const size_t InitialPageCount>
__inline char *ContinuousPageStack<InitialPageCount>::Buffer() const
{
    Assert(bufferSize);
    return pageAllocation->GetAddress();
}

template<const size_t InitialPageCount>
__inline bool ContinuousPageStack<InitialPageCount>::IsEmpty() const
{
    return nextTop == 0;
}

template<const size_t InitialPageCount>
__inline void ContinuousPageStack<InitialPageCount>::Clear()
{
    nextTop = 0;
}

template<const size_t InitialPageCount>
__inline size_t ContinuousPageStack<InitialPageCount>::Position() const
{
    return nextTop;
}

template<const size_t InitialPageCount>
__inline char* ContinuousPageStack<InitialPageCount>::Push(const size_t size)
{
    Assert(size);
    if(bufferSize - nextTop < size)
        Resize(size);
    char* const res = Buffer() + nextTop;
    nextTop += size;
    return res;
}

template<const size_t InitialPageCount>
__inline char* ContinuousPageStack<InitialPageCount>::Top(const size_t size) const
{
    if (nextTop == 0)
        return 0;
    else
    {
        Assert(size != 0);
        Assert(size <= nextTop);
        return Buffer() + (nextTop - size);
    }
}

template<const size_t InitialPageCount>
__inline char* ContinuousPageStack<InitialPageCount>::Pop(const size_t size)
{
    if (nextTop == 0)
        return 0;
    else
    {
        Assert(size != 0);
        Assert(nextTop >= size);
        nextTop -= size;
        return Buffer() + nextTop;
    }
}

template<const size_t InitialPageCount>
__inline void ContinuousPageStack<InitialPageCount>::UnPop(const size_t size)
{
    Assert(size != 0);
    Assert(nextTop + size <= bufferSize);
    nextTop += size;
}

template<const size_t InitialPageCount>
void ContinuousPageStack<InitialPageCount>::Resize(size_t requestedSize)
{
    Assert(requestedSize);
    Assert(requestedSize <= InitialPageCount * AutoSystemInfo::PageSize);

    if(!bufferSize)
    {
        pageAllocation = pageAllocator->AllocAllocation(InitialPageCount);
        if (!pageAllocation)
        {
            outOfMemoryFunc();
            AnalysisAssert(false);
        }
        bufferSize = pageAllocation->GetSize();
        return;
    }

    PageAllocation *const newPageAllocation = pageAllocator->AllocAllocation(pageAllocation->GetPageCount() * 2);
    if (!newPageAllocation)
    {
        outOfMemoryFunc();
        AnalysisAssert(false);
    }
    js_memcpy_s(newPageAllocation->GetAddress(), newPageAllocation->GetSize(), Buffer(), nextTop);
    pageAllocator->ReleaseAllocation(pageAllocation);
    pageAllocation = newPageAllocation;
    bufferSize = newPageAllocation->GetSize();
}

template<const size_t InitialPageCount>
__inline void ContinuousPageStack<InitialPageCount>::PopTo(const size_t position)
{
    Assert(position <= nextTop);
    nextTop = position;
}

// -----------------------------------------------------------------------------------------------------------------------------
// ContinuousPageStackOfFixedElements definition
// -----------------------------------------------------------------------------------------------------------------------------

// --------
// Iterator
// --------

template<class T, const size_t InitialPageCount>
__inline ContinuousPageStackOfFixedElements<T, InitialPageCount>::Iterator::Iterator(
    const ContinuousPageStackOfFixedElements &stack)
    : ContinuousPageStack<InitialPageCount>::Iterator(stack), stack(stack)
{
}

template<class T, const size_t InitialPageCount>
__inline T &ContinuousPageStackOfFixedElements<T, InitialPageCount>::Iterator::operator *() const
{
    Assert(*this);
    Assert(nextTop <= stack.nextTop);
    return *reinterpret_cast<T *>(&stack.Buffer()[nextTop - sizeof(T)]);
}

template<class T, const size_t InitialPageCount>
__inline T *ContinuousPageStackOfFixedElements<T, InitialPageCount>::Iterator::operator ->() const
{
    return &**this;
}

template<class T, const size_t InitialPageCount>
__inline typename ContinuousPageStackOfFixedElements<T, InitialPageCount>::Iterator &ContinuousPageStackOfFixedElements<T, InitialPageCount>::Iterator::operator ++() // pre-increment
{
    Assert(*this);
    nextTop -= sizeof(T);
    return *this;
}

template<class T, const size_t InitialPageCount>
__inline typename ContinuousPageStackOfFixedElements<T, InitialPageCount>::Iterator ContinuousPageStackOfFixedElements<T, InitialPageCount>::Iterator::operator ++(int) // post-increment
{
    Iterator it(*this);
    ++*this;
    return it;
}

// ----------------------------------
// ContinuousPageStackOfFixedElements
// ----------------------------------

template<class T, const size_t InitialPageCount>
__inline ContinuousPageStackOfFixedElements<T, InitialPageCount>::ContinuousPageStackOfFixedElements(
    PageAllocator *const pageAllocator,
    void (*const outOfMemoryFunc)())
    : ContinuousPageStack(pageAllocator, outOfMemoryFunc)
{
}

template<class T, const size_t InitialPageCount>
__inline char* ContinuousPageStackOfFixedElements<T, InitialPageCount>::Push()
{
    return ContinuousPageStack::Push(sizeof(T));
}

template<class T, const size_t InitialPageCount>
__inline T* ContinuousPageStackOfFixedElements<T, InitialPageCount>::Top() const
{
    return reinterpret_cast<T*>(ContinuousPageStack::Top(sizeof(T)));
}

template<class T, const size_t InitialPageCount>
__inline T* ContinuousPageStackOfFixedElements<T, InitialPageCount>::Pop()
{
    return reinterpret_cast<T*>(ContinuousPageStack::Pop(sizeof(T)));
}

template<class T, const size_t InitialPageCount>
__inline void ContinuousPageStackOfFixedElements<T, InitialPageCount>::UnPop()
{
    return ContinuousPageStack::UnPop(sizeof(T));
}

template<class T, const size_t InitialPageCount>
__inline void ContinuousPageStackOfFixedElements<T, InitialPageCount>::PopTo(const size_t position)
{
    ContinuousPageStack::PopTo(position);
}

// -----------------------------------------------------------------------------------------------------------------------------
// ContinuousPageStackOfVariableElements definition
// -----------------------------------------------------------------------------------------------------------------------------

// --------
// Iterator
// --------

template<class T, const size_t InitialPageCount>
__inline ContinuousPageStackOfVariableElements<T, InitialPageCount>::Iterator::Iterator(
    const ContinuousPageStackOfVariableElements &stack)
    : ContinuousPageStack<InitialPageCount>::Iterator(stack), topElementSize(stack.topElementSize), stack(stack)
{
}

template<class T, const size_t InitialPageCount>
__inline T &ContinuousPageStackOfVariableElements<T, InitialPageCount>::Iterator::operator *() const
{
    Assert(*this);
    Assert(nextTop <= stack.nextTop);
    return *reinterpret_cast<T*>(reinterpret_cast<VariableElement *>(&stack.Buffer()[nextTop - topElementSize])->Data());
}

template<class T, const size_t InitialPageCount>
__inline T *ContinuousPageStackOfVariableElements<T, InitialPageCount>::Iterator::operator ->() const
{
    return &**this;
}

template<class T, const size_t InitialPageCount>
__inline typename ContinuousPageStackOfVariableElements<T, InitialPageCount>::Iterator &ContinuousPageStackOfVariableElements<T, InitialPageCount>::Iterator::operator ++() // pre-increment
{
    Assert(*this);
    Assert(nextTop <= stack.nextTop);
    topElementSize = reinterpret_cast<VariableElement *>(&stack.Buffer()[nextTop -= topElementSize])->PreviousElementSize();
    return *this;
}

template<class T, const size_t InitialPageCount>
__inline typename ContinuousPageStackOfVariableElements<T, InitialPageCount>::Iterator ContinuousPageStackOfVariableElements<T, InitialPageCount>::Iterator::operator ++(int) // post-increment
{
    Iterator it(*this);
    ++*this;
    return it;
}

// ---------------
// VariableElement
// ---------------

template<class T, const size_t InitialPageCount>
__inline ContinuousPageStackOfVariableElements<T, InitialPageCount>::VariableElement::VariableElement(
    const size_t previousElementSize)
    : previousElementSize(previousElementSize)
{
}

template<class T, const size_t InitialPageCount>
template<class ActualT>
__inline size_t ContinuousPageStackOfVariableElements<T, InitialPageCount>::VariableElement::Size()
{
    return sizeof(VariableElement) + sizeof(ActualT);
}

template<class T, const size_t InitialPageCount>
__inline size_t ContinuousPageStackOfVariableElements<T, InitialPageCount>::VariableElement::PreviousElementSize() const
{
    return previousElementSize;
}

template<class T, const size_t InitialPageCount>
__inline char* ContinuousPageStackOfVariableElements<T, InitialPageCount>::VariableElement::Data()
{
    return data;
}

// -------------------------------------
// ContinuousPageStackOfVariableElements
// -------------------------------------

template<class T, const size_t InitialPageCount>
__inline ContinuousPageStackOfVariableElements<T, InitialPageCount>::ContinuousPageStackOfVariableElements(
    PageAllocator *const pageAllocator,
    void (*const outOfMemoryFunc)())
    : ContinuousPageStack(pageAllocator, outOfMemoryFunc)
{
}

template<class T, const size_t InitialPageCount>
template<class ActualT>
__inline char* ContinuousPageStackOfVariableElements<T, InitialPageCount>::Push()
{
    TemplateParameter::SameOrDerivedFrom<ActualT, T>(); // ActualT must be the same type as, or a type derived from, T
    VariableElement *const element =
        new(ContinuousPageStack::Push(VariableElement::Size<ActualT>())) VariableElement(topElementSize);
    topElementSize = VariableElement::Size<ActualT>();
    return element->Data();
}

template<class T, const size_t InitialPageCount>
__inline T* ContinuousPageStackOfVariableElements<T, InitialPageCount>::Top() const
{
    VariableElement* const element = reinterpret_cast<VariableElement*>(ContinuousPageStack::Top(topElementSize));
    return element == 0 ? 0 : reinterpret_cast<T*>(element->Data());
}

template<class T, const size_t InitialPageCount>
__inline T* ContinuousPageStackOfVariableElements<T, InitialPageCount>::Pop()
{
    VariableElement *const element = reinterpret_cast<VariableElement*>(ContinuousPageStack::Pop(topElementSize));
    if (element == 0)
        return 0;
    else
    {
        topElementSize = element->PreviousElementSize();
        return reinterpret_cast<T*>(element->Data());
    }
}

template<class T, const size_t InitialPageCount>
template<class ActualT>
__inline void ContinuousPageStackOfVariableElements<T, InitialPageCount>::UnPop()
{
    TemplateParameter::SameOrDerivedFrom<ActualT, T>(); // ActualT must be the same type as, or a type derived from, T
    ContinuousPageStack::UnPop(VariableElement::Size<ActualT>());
    Assert(reinterpret_cast<VariableElement*>(ContinuousPageStack::Top(VariableElement::Size<ActualT>()))->PreviousElementSize() == topElementSize);
    topElementSize = VariableElement::Size<ActualT>();
}

template<class T, const size_t InitialPageCount>
__inline void ContinuousPageStackOfVariableElements<T, InitialPageCount>::PopTo(const size_t position)
{
    Assert(position <= nextTop);
    if(position != nextTop)
    {
        Assert(position + sizeof(VariableElement) <= nextTop);
        topElementSize = reinterpret_cast<VariableElement *>(&Buffer()[position])->PreviousElementSize();
    }
    ContinuousPageStack::PopTo(position);
}
