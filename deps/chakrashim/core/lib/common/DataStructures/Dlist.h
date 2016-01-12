//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------
//
// File: DList.h
//
// Template for Doubly Linked List
//----------------------------------------------------------------------------

template <typename TData, typename TCount = DefaultCount> class DListBase;
template <typename TData> class DListNode;
template <typename TData>
class DListNodeBase
{
public:
    DListNodeBase<TData> * Next() const { return next.base; }
    DListNodeBase<TData> *& Next() { return next.base; }
    DListNodeBase<TData> * Prev() const { return prev.base; }
    DListNodeBase<TData> *& Prev() { return prev.base; }
private:
    // The next node can be a real node with data, or  it point back to the start of the list
    // Use a union to show it in the debugger (instead of casting everywhere)
    union
    {
        DListNodeBase<TData> * base;
        DListNode<TData> * node;
        DListBase<TData>  * list;
    } next;

    union
    {
        DListNodeBase<TData> * base;
        DListNode<TData> * node;
        DListBase<TData>  * list;
    } prev;
};

template <typename TData>
class DListNode : public DListNodeBase<TData>
{
public:
    DListNode() : data() {}

    // Constructing with parameter
    template <typename TParam1>
    DListNode(TParam1 param1) : data(param1) {}

    // Constructing with parameter
    template <typename TParam1, typename TParam2>
    DListNode(TParam1 param1, TParam2 param2) : data(param1, param2) {}

    // Constructing with parameter
    template <typename TParam1, typename TParam2, typename TParam3>
    DListNode(TParam1 param1, TParam2 param2, TParam3 param3) : data(param1, param2, param3) {}

    // Constructing with parameter
    template <typename TParam1, typename TParam2, typename TParam3, typename TParam4>
    DListNode(TParam1 param1, TParam2 param2, TParam3 param3, TParam4 param4) : data(param1, param2, param3, param4) {}

    // Constructing using copy constructor
    DListNode(TData const& data) : data(data) {};
    TData data;
};


template<typename TData, typename TCount>
class DListBase : protected DListNodeBase<TData>, public TCount
{
private:
    typedef DListNodeBase<TData> NodeBase;
    typedef DListNode<TData> Node;
    bool IsHead(NodeBase const * node) const
    {
        return (node == this);
    }
public:
    class Iterator
    {
    public:
        Iterator() : list(nullptr), current(nullptr) {}
        Iterator(DListBase const * list) : list(list), current(list) {};

        bool IsValid() const
        {
            return (current != nullptr && !list->IsHead(current));
        }
        void Reset()
        {
            current = list;
        }

        // TODO: only need inline for DListBase<Segment, FakeCount>::Iterator::Next
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
        DListBase const * list;
        NodeBase const * current;
    };

    class EditingIterator : public Iterator
    {
    public:
        EditingIterator() : Iterator() {};
        EditingIterator(DListBase  * list) : Iterator(list) {};

        template <typename TAllocator>
        void RemoveCurrent(TAllocator * allocator)
        {
            Assert(current != nullptr);
            Assert(!list->IsHead(current));

            NodeBase * last = current->Prev();
            NodeBase * node = const_cast<NodeBase *>(current);
            DListBase::RemoveNode(node);
            AllocatorDelete(TAllocator, allocator, (Node *)node);
            current = last;
            const_cast<DListBase *>(list)->DecrementCount();
        }

        template <typename TAllocator>
        TData * InsertNodeBefore(TAllocator * allocator)
        {
            Node * newNode = AllocatorNew(TAllocator, allocator, Node);
            if (newNode)
            {
                NodeBase * node = const_cast<NodeBase *>(current);
                DListBase::InsertNodeBefore(node, newNode);
                const_cast<DListBase *>(list)->IncrementCount();
                return newNode->data;
            }
        }

        template <typename TAllocator>
        bool InsertBefore(TAllocator * allocator, TData const& data)
        {
            Node * newNode = AllocatorNew(TAllocator, allocator, Node, data);
            if (newNode)
            {
                NodeBase * node = const_cast<NodeBase *>(current);
                DListBase::InsertNodeBefore(node, newNode);
                const_cast<DListBase *>(list)->IncrementCount();
                return true;
            }
            return false;
        }

        void MoveCurrentTo(DListBase * toList)
        {
            NodeBase * last = current->Prev();
            NodeBase * node = const_cast<NodeBase *>(current);
            DListBase::RemoveNode(node);
            DListBase::InsertNodeBefore(toList->Next(), node);
            current = last;
            const_cast<DListBase *>(list)->DecrementCount();
            toList->IncrementCount();
        }
    };

    explicit DListBase()
    {
        Reset();
    }

    ~DListBase()
    {
        AssertMsg(this->Empty(), "DListBase need to be cleared explicitly with an allocator");
    }

    void Reset()
    {
        this->Next() = this;
        this->Prev() = this;
        this->SetCount(0);
    }

    template <typename TAllocator>
    void Clear(TAllocator * allocator)
    {
        NodeBase * current = this->Next();
        while (!this->IsHead(current))
        {
            NodeBase * next = current->Next();
            AllocatorDelete(TAllocator, allocator, (Node *)current);
            current = next;
        }

        this->Next() = this;
        this->Prev() = this;
        this->SetCount(0);
    }
    bool Empty() const { return this->IsHead(this->Next()); }
    bool HasOne() const { return !Empty() && this->IsHead(this->Next()->Next()); }
    TData const& Head() const { Assert(!Empty()); return ((Node *)this->Next())->data; }
    TData& Head() { Assert(!Empty()); return ((Node *)this->Next())->data; }
    TData const& Tail() const { Assert(!Empty()); return ((Node *)this->Prev())->data; }
    TData & Tail()  { Assert(!Empty()); return ((Node *)this->Prev())->data; }

    template <typename TAllocator>
    bool Append(TAllocator * allocator, TData const& data)
    {
        Node * newNode = AllocatorNew(TAllocator, allocator, Node, data);
        if (newNode)
        {
            DListBase::InsertNodeAfter(this->Prev(), newNode);
            this->IncrementCount();
            return true;
        }
        return false;
    }

    template <typename TAllocator>
    bool Prepend(TAllocator * allocator, TData const& data)
    {
        Node * newNode = AllocatorNew(TAllocator, allocator, Node, data);
        if (newNode)
        {
            DListBase::InsertNodeBefore(this->Next(), newNode);
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
            DListBase::InsertNodeBefore(this->Next(), newNode);
            this->IncrementCount();
            return &newNode->data;
        }
        return nullptr;
    }

    template <typename TAllocator, typename TParam1>
    TData * PrependNode(TAllocator * allocator, TParam1 param1)
    {
        Node * newNode = AllocatorNew(TAllocator, allocator, Node, param1);
        if (newNode)
        {
            DListBase::InsertNodeBefore(this->Next(), newNode);
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
            DListBase::InsertNodeBefore(this->Next(), newNode);
            this->IncrementCount();
            return &newNode->data;
        }
        return nullptr;
    }

    template <typename TAllocator, typename TParam1, typename TParam2, typename TParam3>
    TData * PrependNode(TAllocator * allocator, TParam1 param1, TParam2 param2, TParam3 param3)
    {
        Node * newNode = AllocatorNew(TAllocator, allocator, Node, param1, param2, param3);
        if (newNode)
        {
            DListBase::InsertNodeBefore(this->Next(), newNode);
            this->IncrementCount();
            return &newNode->data;
        }
        return nullptr;
    }

    template <typename TAllocator, typename TParam1, typename TParam2, typename TParam3, typename TParam4>
    TData * PrependNode(TAllocator * allocator, TParam1 param1, TParam2 param2, TParam3 param3, TParam4 param4)
    {
        Node * newNode = AllocatorNew(TAllocator, allocator, Node, param1, param2, param3, param4);
        if (newNode)
        {
            DListBase::InsertNodeBefore(this->Next(), newNode);
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
        DListBase::RemoveNode(node);
        AllocatorDelete(TAllocator, allocator, (Node *)node);

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

    template <typename TAllocator>
    void RemoveElement(TAllocator * allocator, TData * element)
    {
        Node * node = CONTAINING_RECORD(element, Node, data);
#if DBG_DUMP
        Assert(HasNode(node));
#endif
        DListBase::RemoveNode(node);
        AllocatorDelete(TAllocator, allocator, node);

        this->DecrementCount();

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

    void MoveTo(DListBase * list)
    {

        list->Prev()->Next() = this->Next();
        this->Next()->Prev() = list->Prev();

        list->Prev() = this->Prev();
        this->Prev()->Next() = list;

        this->Prev() = this;
        this->Next() = this;

        list->AddCount(*this);
        this->SetCount(0);
    }

    void MoveHeadTo(DListBase * list)
    {
        Assert(!this->Empty());
        NodeBase * node = this->Next();
        DListBase::RemoveNode(node);
        DListBase::InsertNodeBefore(list->Next(), node);
        this->DecrementCount();
        list->IncrementCount();
    }

    void MoveElementTo(TData * element, DListBase * list)
    {
        Node * node = CONTAINING_RECORD(element, Node, data);
#if DBG_DUMP
        Assert(HasNode(node));
#endif
        DListBase::RemoveNode(node);
        DListBase::InsertNodeBefore(list->Next(), node);
        this->DecrementCount();
        list->IncrementCount();
    }

#if DBG_DUMP
    bool HasElement(TData const * element) const
    {
        Node * node = CONTAINING_RECORD(element, Node, data);
        return HasNode(node);
    }
#endif

private:
#if DBG_DUMP
    bool HasNode(NodeBase * node) const
    {
        NodeBase * current = this->Next();
        while (!this->IsHead(current))
        {
            if (node == current)
            {
                return true;
            }
            current = current->Next();
        }
        return false;
    }
#endif

    // disable copy constructor
    DListBase(DListBase const& list);

    static void InsertNodeAfter(NodeBase * node, NodeBase * newNode)
    {
        newNode->Prev() = node;
        newNode->Next() = node->Next();
        node->Next()->Prev() = newNode;
        node->Next() = newNode;
    }

    static void InsertNodeBefore(NodeBase * node, NodeBase * newNode)
    {
        newNode->Prev() = node->Prev();
        newNode->Next() = node;
        node->Prev()->Next() = newNode;
        node->Prev() = newNode;
    }

    static void RemoveNode(NodeBase * node)
    {
        node->Prev()->Next() = node->Next();
        node->Next()->Prev() = node->Prev();
    }
};

#define FOREACH_DLISTBASE_ENTRY(T, data, list) \
{ \
    DListBase<T>::Iterator __iter(list); \
    while (__iter.Next()) \
    { \
        T& data = __iter.Data();

#define NEXT_DLISTBASE_ENTRY \
    }  \
}

#define FOREACH_DLISTBASE_ENTRY_EDITING(T, data, list, iter) \
    DListBase<T>::EditingIterator iter(list); \
    while (iter.Next()) \
    { \
        T& data = iter.Data();

#define NEXT_DLISTBASE_ENTRY_EDITING \
    }

template <typename TData, typename TAllocator, typename TCount = DefaultCount>
class DList : public DListBase<TData, TCount>
{
public:
    class EditingIterator : public DListBase::EditingIterator
    {
    public:
        EditingIterator() : DListBase::EditingIterator() {}
        EditingIterator(DList * list) : DListBase::EditingIterator(list) {}
        void RemoveCurrent()
        {
            __super::RemoveCurrent(Allocator());
        }
        TData& InsertNodeBefore()
        {
            return __super::InsertNodeBefore(Allocator());
        }
        void InsertBefore(TData const& data)
        {
            __super::InsertBefore(Allocator(), data);
        }

    private:
        TAllocator * Allocator() const
        {
            return ((DList const *)list)->allocator;
        }
    };

    explicit DList(TAllocator * allocator) : allocator(allocator) {}
    ~DList()
    {
        Clear();
    }
    void Clear()
    {
        __super::Clear(allocator);
    }
    bool Append(TData const& data)
    {
        return __super::Append(allocator, data);
    }
    bool Prepend(TData const& data)
    {
        return __super::Prepend(allocator, data);
    }
    TData * PrependNode()
    {
        return __super::PrependNode(allocator);
    }
    template <typename TParam1>
    TData * PrependNode(TParam1 param1)
    {
        return __super::PrependNode(allocator, param1);
    }
    template <typename TParam1, typename TParam2>
    TData * PrependNode(TParam1 param1, TParam2 param2)
    {
        return __super::PrependNode(allocator, param1, param2);
    }
    template <typename TParam1, typename TParam2, typename TParam3>
    TData * PrependNode(TParam1 param1, TParam2 param2, TParam3 param3)
    {
        return __super::PrependNode(allocator, param1, param2, param3);
    }
    template <typename TParam1, typename TParam2, typename TParam3, typename TParam4>
    TData * PrependNode(TParam1 param1, TParam2 param2, TParam3 param3, TParam4 param4)
    {
        return __super::PrependNode(allocator, param1, param2, param3, param4);
    }
    void RemoveHead()
    {
        __super::RemoveHead(allocator);
    }
    bool Remove(TData const& data)
    {
        return __super::Remove(allocator, data);
    }

    void RemoveElement(TData * data)
    {
        return __super::RemoveElement(allocator, data);
    }

private:
    TAllocator * allocator;
};

template <typename TData, typename TAllocator = ArenaAllocator>
class DListCounted : public DList<TData, TAllocator, RealCount>
{
public:
    explicit DListCounted(TAllocator * allocator) : DList(allocator) {}
};

#define FOREACH_DLIST_ENTRY(T, alloc, data, list) \
{ \
    DList<T, alloc>::Iterator __iter(list); \
    while (__iter.Next()) \
    { \
        T& data = __iter.Data();

#define NEXT_DLIST_ENTRY \
    }  \
}

#define FOREACH_DLIST_ENTRY_EDITING(T, alloc, data, list, iter) \
    DList<T, alloc>::EditingIterator iter(list); \
    while (iter.Next()) \
    { \
        T& data = iter.Data();

#define NEXT_DLIST_ENTRY_EDITING \
    }
