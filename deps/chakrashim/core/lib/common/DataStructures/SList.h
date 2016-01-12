//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------
//
// File: SList.h
//
// Template for Singly Linked List
//
//----------------------------------------------------------------------------

class FakeCount
{
protected:
    void IncrementCount() {}
    void DecrementCount() {}
    void SetCount(uint count) {}
    void AddCount(FakeCount& c) {}
};

class RealCount
{
protected:
    RealCount() : count(0) {}
    void IncrementCount() { count++; }
    void DecrementCount() { count--; }
    void SetCount(uint count) { this->count = count; }
    void AddCount(RealCount const& c) { this->count += c.Count(); }
public:
    uint Count() const { return count; }
private:
    uint count;
};

#if DBG
typedef RealCount DefaultCount;
#else
typedef FakeCount DefaultCount;
#endif

template <typename TData, typename TCount = DefaultCount> class SListBase;
template <typename TData> class SListNode;
template <typename TData>
class SListNodeBase
{
public:
    SListNodeBase<TData> * Next() const { return next.base; }
    SListNodeBase<TData> *& Next() { return next.base; }
protected:
    // The next node can be a real node with data, or it point back to the start of the list
    // Use a union to show it in the debugger (instead of casting everywhere)
    union
    {
        SListNodeBase<TData> * base;
        SListNode<TData> * node;
        SListBase<TData> * list;
    } next;
};

template <typename TData>
class SListNode : public SListNodeBase<TData>
{
    friend class SListBase<TData, FakeCount>;
    friend class SListBase<TData, RealCount>;
private:

    SListNode() : data() {}

    // Constructing with parameter
    template <typename TParam>
    SListNode(TParam param) : data(param) {}

    // Constructing with parameter
    template <typename TParam1, typename TParam2>
    SListNode(TParam1 param1, TParam2 param2) : data(param1, param2) {}

    // Constructing using copy constructor
    SListNode(TData const& data) : data(data) {};
    TData data;
};

template<typename TData, typename TCount>
class SListBase : protected SListNodeBase<TData>, public TCount
{
private:
    typedef SListNodeBase<TData> NodeBase;
    typedef SListNode<TData> Node;
    bool IsHead(NodeBase const * node) const
    {
        return (node == this);
    }
public:
    class Iterator
    {
    public:
        Iterator() : list(nullptr), current(nullptr) {}
        Iterator(SListBase const * list) : list(list), current(list) {};

        bool IsValid() const
        {
            return (current != nullptr && !list->IsHead(current));
        }
        void Reset()
        {
            current = list;
        }

        // forceinline only needed for SListBase<FlowEdge *, RealCount>::Iterator::Next()
        __forceinline
        bool Next()
        {
            Assert(current != nullptr);
            if (list->IsHead(current->Next()))
            {
                current = nullptr;
                return false;
            }
            current = current->Next();
            return true;
        }
        TData const& Data() const
        {
            Assert(this->IsValid());
            return ((Node *)current)->data;
        }
        TData& Data()
        {
            Assert(this->IsValid());
            return ((Node *)current)->data;
        }
    protected:
        SListBase const * list;
        NodeBase const * current;
    };

    class EditingIterator : public Iterator
    {
    public:
        EditingIterator() : Iterator(), last(nullptr) {};
        EditingIterator(SListBase  * list) : Iterator(list), last(nullptr) {};

        bool Next()
        {
            if (last != nullptr && last->Next() != current)
            {
                current = last;
            }
            else
            {
                last = current;
            }
            return Iterator::Next();
        }

        void UnlinkCurrent()
        {
            UnlinkCurrentNode();
        }

        template <typename TAllocator>
        void RemoveCurrent(TAllocator * allocator)
        {
            const NodeBase *dead = current;
            UnlinkCurrent();

            auto freeFunc = TypeAllocatorFunc<TAllocator, TData>::GetFreeFunc();

            AllocatorFree(allocator, freeFunc, (Node *) dead, sizeof(Node));
        }

        template <typename TAllocator>
        TData * InsertNodeBefore(TAllocator * allocator)
        {
            Assert(last != nullptr);
            Node * newNode = AllocatorNew(TAllocator, allocator, Node);
            if (newNode)
            {
                newNode->Next() = last->Next();
                const_cast<NodeBase *>(last)->Next() = newNode;
                const_cast<SListBase *>(list)->IncrementCount();
                last = newNode;
                return &newNode->data;
            }
            return nullptr;
        }

        template <typename TAllocator>
        TData * InsertNodeBeforeNoThrow(TAllocator * allocator)
        {
            Assert(last != nullptr);
            Node * newNode = AllocatorNewNoThrow(TAllocator, allocator, Node);
            if (newNode)
            {
                newNode->Next() = last->Next();
                const_cast<NodeBase *>(last)->Next() = newNode;
                const_cast<SListBase *>(list)->IncrementCount();
                last = newNode;
                return &newNode->data;
            }
            return nullptr;
        }

        template <typename TAllocator>
        bool InsertBefore(TAllocator * allocator, TData const& data)
        {
            Assert(last != nullptr);
            Node * newNode = AllocatorNew(TAllocator, allocator, Node, data);
            if (newNode)
            {
                newNode->Next() = last->Next();
                const_cast<NodeBase *>(last)->Next() = newNode;
                const_cast<SListBase *>(list)->IncrementCount();
                last = newNode;
                return true;
            }
            return false;
        }

        void MoveCurrentTo(SListBase * toList)
        {
            NodeBase * node = UnlinkCurrentNode();
            node->Next() = toList->Next();
            toList->Next() = node;
            toList->IncrementCount();
        }

    private:
        NodeBase const * last;

        NodeBase * UnlinkCurrentNode()
        {
            NodeBase * unlinkedNode = const_cast<NodeBase *>(current);
            Assert(current != nullptr);
            Assert(!list->IsHead(current));
            Assert(last != nullptr);

            const_cast<NodeBase *>(last)->Next() = current->Next();
            current = last;
            last = nullptr;
            const_cast<SListBase *>(list)->DecrementCount();
            return unlinkedNode;
        }
    };

    explicit SListBase()
    {
        Reset();
    }

    ~SListBase()
    {
        AssertMsg(this->Empty(), "SListBase need to be cleared explicitly with an allocator");
    }

    void Reset()
    {
        this->Next() = this;
        this->SetCount(0);
    }

    template <typename TAllocator>
    __forceinline
    void Clear(TAllocator * allocator)
    {
        NodeBase * current = this->Next();
        while (!this->IsHead(current))
        {
            NodeBase * next = current->Next();

            auto freeFunc = TypeAllocatorFunc<TAllocator, TData>::GetFreeFunc();

            AllocatorFree(allocator, freeFunc, (Node *)current, sizeof(Node));
            current = next;
        }

        this->Reset();
    }
    bool Empty() const { return this->IsHead(this->Next()); }
    bool HasOne() const { return !Empty() && this->IsHead(this->Next()->Next()); }
    bool HasTwo() const { return !Empty() && this->IsHead(this->Next()->Next()->Next()); }
    TData const& Head() const { Assert(!Empty()); return ((Node *)this->Next())->data; }
    TData& Head()
    {
        Assert(!Empty());
        Node * node = this->next.node;
        return node->data;
    }

    template <typename TAllocator>
    bool Prepend(TAllocator * allocator, TData const& data)
    {
        Node * newNode = AllocatorNew(TAllocator, allocator, Node, data);
        if (newNode)
        {
            newNode->Next() = this->Next();
            this->Next() = newNode;
            this->IncrementCount();
            return true;
        }
        return false;
    }

    template <typename TAllocator>
    bool PrependNoThrow(TAllocator * allocator, TData const& data)
    {
        Node * newNode = AllocatorNewNoThrow(TAllocator, allocator, Node, data);
        if (newNode)
        {
            newNode->Next() = this->Next();
            this->Next() = newNode;
            this->IncrementCount();
            return true;
        }
        return false;
    }

    template <typename TAllocator>
    TData * PrependNode(TAllocator * allocator)
    {
        Node * newNode = AllocatorNew(TAllocator, allocator, Node);
        if (newNode)
        {
            newNode->Next() = this->Next();
            this->Next() = newNode;
            this->IncrementCount();
            return &newNode->data;
        }
        return nullptr;
    }

    template <typename TAllocator, typename TParam>
    TData * PrependNode(TAllocator * allocator, TParam param)
    {
        Node * newNode = AllocatorNew(TAllocator, allocator, Node, param);
        if (newNode)
        {
            newNode->Next() = this->Next();
            this->Next() = newNode;
            this->IncrementCount();
            return &newNode->data;
        }
        return nullptr;
    }

    template <typename TAllocator, typename TParam1, typename TParam2>
    TData * PrependNode(TAllocator * allocator, TParam1 param1, TParam2 param2)
    {
        Node * newNode = AllocatorNew(TAllocator, allocator, Node, param1, param2);
        if (newNode)
        {
            newNode->Next() = this->Next();
            this->Next() = newNode;
            this->IncrementCount();
            return &newNode->data;
        }
        return nullptr;
    }

    template <typename TAllocator>
    void RemoveHead(TAllocator * allocator)
    {
        Assert(!this->Empty());

        NodeBase * node = this->Next();
        this->Next() = node->Next();

        auto freeFunc = TypeAllocatorFunc<TAllocator, TData>::GetFreeFunc();
        AllocatorFree(allocator, freeFunc, (Node *) node, sizeof(Node));
        this->DecrementCount();
    }

    template <typename TAllocator>
    bool Remove(TAllocator * allocator, TData const& data)
    {
        EditingIterator iter(this);
        while (iter.Next())
        {
            if (iter.Data() == data)
            {
                iter.RemoveCurrent(allocator);
                return true;
            }
        }
        return false;
    }

    bool Has(TData data) const
    {
        Iterator iter(this);
        while (iter.Next())
        {
            if (iter.Data() == data)
            {
                return true;
            }
        }
        return false;
    }

    void MoveTo(SListBase * list)
    {
        while (!Empty())
        {
            this->MoveHeadTo(list);
        }
    }

    void MoveHeadTo(SListBase * list)
    {
        Assert(!this->Empty());
        NodeBase * node = this->Next();
        this->Next() = node->Next();
        node->Next() = list->Next();
        list->Next() = node;

        list->IncrementCount();
        this->DecrementCount();
    }

    // Moves the first element that satisfies the predicate to the toList
    template<class Fn>
    TData* MoveTo(SListBase* toList, Fn predicate)
    {
        Assert(this != toList);

        EditingIterator iter(this);
        while (iter.Next())
        {
            if (predicate(iter.Data()))
            {
                TData* data = &iter.Data();
                iter.MoveCurrentTo(toList);
                return data;
            }
        }
        return nullptr;
    }

    template<class Fn>
    TData* Find(Fn predicate)
    {
        Iterator iter(this);
        while(iter.Next())
        {
            if(predicate(iter.Data()))
            {
                return &iter.Data();
            }
        }
        return nullptr;
    }

    template<class Fn>
    void Iterate(Fn fn)
    {
        Iterator iter(this);
        while(iter.Next())
        {
            fn(iter.Data());
        }
    }

    void Reverse()
    {
        NodeBase * prev = this;
        NodeBase * current = this->Next();
        while (!this->IsHead(current))
        {
            NodeBase * next = current->Next();
            current->Next() = prev;
            prev = current;
            current = next;
        }
        current->Next() = prev;
    }

    bool Equals(SListBase const& other)
    {
        SListBase<TData>::Iterator iter(this);
        SListBase<TData>::Iterator iter2(&other);
        while (iter.Next())
        {
            if (!iter2.Next() || iter.Data() != iter2.Data())
            {
                return false;
            }
        }
        return !iter2.Next();
    }

    template <typename TAllocator>
    bool CopyTo(TAllocator * allocator, SListBase& to) const
    {
        return CopyTo<DefaultCopyElement>(allocator, to);
    }

    template <void (*CopyElement)(TData const& from, TData& to), typename TAllocator>
    bool CopyTo(TAllocator * allocator, SListBase& to) const
    {
        to.Clear(allocator);
        SListBase::Iterator iter(this);
        NodeBase ** next = &to.Next();
        while (iter.Next())
        {
            Node * node = AllocatorNew(TAllocator, allocator, Node);
            if (node == nullptr)
            {
                return false;
            }
            CopyElement(iter.Data(), node->data);
            *next = node;
            next = &node->Next();
            *next = &to;            // Do this every time, in case an OOM exception occurs, to keep the list correct
            to.IncrementCount();
        }
        return true;
    }

    template <class Fn>
    void Map(Fn fn) const
    {
        MapUntil([fn](TData& data) { fn(data); return false; });
    }

    template <class Fn>
    bool MapUntil(Fn fn) const
    {
        Iterator iter(this);
        while (iter.Next())
        {
            if (fn(iter.Data()))
            {
                return true;
            }
        }
        return false;
    }
private:

    static void DefaultCopyElement(TData const& from, TData& to) { to = from; }
    // disable copy constructor
    SListBase(SListBase const& list);
};


template <typename TData>
class SListBaseCounted : public SListBase<TData, RealCount>
{
};

template <typename TData, typename TAllocator = ArenaAllocator, typename TCount = DefaultCount>
class SList : public SListBase<TData, TCount>
{
public:
    class EditingIterator : public SListBase::EditingIterator
    {
    public:
        EditingIterator() : SListBase::EditingIterator() {}
        EditingIterator(SList * list) : SListBase::EditingIterator(list) {}
        void RemoveCurrent()
        {
            __super::RemoveCurrent(Allocator());
        }
        TData * InsertNodeBefore()
        {
            return __super::InsertNodeBefore(Allocator());
        }
        bool InsertBefore(TData const& data)
        {
            return __super::InsertBefore(Allocator(), data);
        }
    private:
        TAllocator * Allocator() const
        {
            return ((SList const *)list)->allocator;
        }
    };

    explicit SList(TAllocator * allocator) : allocator(allocator) {}
    ~SList()
    {
        Clear();
    }
    void Clear()
    {
        __super::Clear(allocator);
    }
    bool Prepend(TData const& data)
    {
        return __super::Prepend(allocator, data);
    }
    TData * PrependNode()
    {
        return __super::PrependNode(allocator);
    }
    template <typename TParam>
    TData * PrependNode(TParam param)
    {
        return __super::PrependNode(allocator, param);
    }
    template <typename TParam1, typename TParam2>
    TData * PrependNode(TParam1 param1, TParam2 param2)
    {
        return __super::PrependNode(allocator, param1, param2);
    }
    void RemoveHead()
    {
        __super::RemoveHead(allocator);
    }
    bool Remove(TData const& data)
    {
        return __super::Remove(allocator, data);
    }


    // Stack like interface
    bool Push(TData const& data)
    {
        return Prepend(data);
    }

    TData Pop()
    {
        TData data = Head();
        RemoveHead();
        return data;
    }

    TData const& Top() const
    {
        return Head();
    }
    TData& Top()
    {
        return Head();
    }
private:
    TAllocator * allocator;
};

template <typename TData, typename TAllocator = ArenaAllocator>
class SListCounted : public SList<TData, TAllocator, RealCount>
{
    public:
        explicit SListCounted(TAllocator * allocator) : SList(allocator) {}

};

#define _FOREACH_LIST_ENTRY_EX(List, T, Iterator, iter, data, list) \
    List<T>::Iterator iter(list); \
    while (iter.Next()) \
    { \
        T& data = iter.Data();

#define _NEXT_LIST_ENTRY_EX  \
    }

#define _FOREACH_LIST_ENTRY(List, T, data, list) { _FOREACH_LIST_ENTRY_EX(List, T, Iterator, __iter, data, list)
#define _NEXT_LIST_ENTRY _NEXT_LIST_ENTRY_EX }

#define FOREACH_SLISTBASE_ENTRY(T, data, list) _FOREACH_LIST_ENTRY(SListBase, T, data, list)
#define NEXT_SLISTBASE_ENTRY _NEXT_LIST_ENTRY

#define FOREACH_SLISTBASE_ENTRY_EDITING(T, data, list, iter) _FOREACH_LIST_ENTRY_EX(SListBase, T, EditingIterator, iter, data, list)
#define NEXT_SLISTBASE_ENTRY_EDITING _NEXT_LIST_ENTRY_EX

#define FOREACH_SLISTBASECOUNTED_ENTRY(T, data, list) _FOREACH_LIST_ENTRY(SListBaseCounted, T, data, list)
#define NEXT_SLISTBASECOUNTED_ENTRY _NEXT_LIST_ENTRY

#define FOREACH_SLISTBASECOUNTED_ENTRY_EDITING(T, data, list, iter) _FOREACH_LIST_ENTRY_EX(SListBaseCounted, T, EditingIterator, iter, data, list)
#define NEXT_SLISTBASECOUNTED_ENTRY_EDITING _NEXT_LIST_ENTRY_EX

#define FOREACH_SLIST_ENTRY(T, data, list) _FOREACH_LIST_ENTRY(SList, T, data, list)
#define NEXT_SLIST_ENTRY _NEXT_LIST_ENTRY

#define FOREACH_SLIST_ENTRY_EDITING(T, data, list, iter) _FOREACH_LIST_ENTRY_EX(SList, T, EditingIterator, iter, data, list)
#define NEXT_SLIST_ENTRY_EDITING _NEXT_LIST_ENTRY_EX

#define FOREACH_SLISTCOUNTED_ENTRY(T, data, list) _FOREACH_LIST_ENTRY(SListCounted, T, data, list)
#define NEXT_SLISTCOUNTED_ENTRY _NEXT_LIST_ENTRY

#define FOREACH_SLISTCOUNTED_ENTRY_EDITING(T, data, list, iter) _FOREACH_LIST_ENTRY_EX(SListCounted, T, EditingIterator, iter, data, list)
#define NEXT_SLISTCOUNTED_ENTRY_EDITING _NEXT_LIST_ENTRY_EX
