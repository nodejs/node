//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#if defined(_M_ARM64) || defined(_M_X64)
typedef  BVUnit32 SparseBVUnit;
#else
typedef  BVUnit64 SparseBVUnit;
#endif

#define FOREACH_BITSET_IN_SPARSEBV(index, bv) \
{ \
    BVIndex index; \
    for(BVSparseNode * _curNode = (bv)->head; _curNode != 0 ; _curNode = _curNode->next) \
    { \
        BVIndex _offset; \
        BVIndex _startIndex = _curNode->startIndex; \
        SparseBVUnit  _unit = _curNode->data; \
        for(_offset = _unit.GetNextBit(); _offset != -1; _offset = _unit.GetNextBit()) \
        { \
            index = _startIndex + _offset; \
            _unit.Clear(_offset); \
        \

#define BREAK_BITSET_IN_SPARSEBV \
            _curNode = 0; \
            break;

#define NEXT_BITSET_IN_SPARSEBV \
        } \
        if(_curNode == 0) \
        { \
            break; \
        } \
    } \
}

#define FOREACH_BITSET_IN_SPARSEBV_EDITING(index, bv) \
{ \
    BVIndex index;  \
    BVSparseNode * _curNodeEdit = (bv)->head; \
    while (_curNodeEdit != nullptr) \
    { \
        BVSparseNode * _next = _curNodeEdit->next; \
        BVIndex _offset; \
        BVIndex _startIndex = _curNodeEdit->startIndex; \
        SparseBVUnit  _unit = _curNodeEdit->data; \
        for(_offset = _unit.GetNextBit(); _offset != -1; _offset = _unit.GetNextBit()) \
        { \
            index = _startIndex + _offset; \
            _unit.Clear(_offset); \
        \

#define NEXT_BITSET_IN_SPARSEBV_EDITING           \
        } \
        _curNodeEdit = _next; \
    } \
}

#define SPARSEBV_CLEAR_CURRENT_BIT() _curNodeEdit->data.Clear(_offset)

struct BVSparseNode
{
    BVIndex         startIndex;
#if defined(_M_ARM64) || defined(_M_X64)
    //64-bit: the order is changed to make sure it fits in 16 bytes
    SparseBVUnit    data;
    BVSparseNode *  next;
#else //_M_IX86 and _M_ARM32
    BVSparseNode *  next;
    SparseBVUnit    data;
#endif


    BVSparseNode(BVIndex beginIndex, BVSparseNode * nextNode);

    void init(BVIndex beginIndex, BVSparseNode * nextNode);
    bool ToString(
        __out_ecount(strSize) char *const str,
        const size_t strSize,
        size_t *const writtenLengthRef = nullptr,
        const bool isInSequence = false,
        const bool isFirstInSequence = false,
        const bool isLastInSequence = false) const;
};

CompileAssert(sizeof(BVSparseNode) == 16); // Performance assert, BVSparseNode is heavily used in the backend, do perf measurement before changing this.

template <class TAllocator>
class BVSparse
{
// Data
public:
            BVSparseNode *      head;

private:
            TAllocator *        alloc;
            BVSparseNode **     lastUsedNodePrevNextField;

    static const SparseBVUnit       s_EmptyUnit;

// Constructor
public:
    BVSparse(TAllocator* allocator);
    ~BVSparse();

// Implementation
protected:
    template <class TOtherAllocator>
    static  void            AssertBV(const BVSparse<TOtherAllocator> * bv);


            SparseBVUnit *      BitsFromIndex(BVIndex i, bool create = true);
            BVSparseNode*   NodeFromIndex(BVIndex i, BVSparseNode *** prevNextFieldOut, bool create = true);
            BVSparseNode *  DeleteNode(BVSparseNode *node, bool bResetLastUsed = true);
            void            QueueInFreeList(BVSparseNode* node);
            BVSparseNode *  Allocate(const BVIndex searchIndex, BVSparseNode *prevNode);

    template<void (SparseBVUnit::*callback)(SparseBVUnit)>
    void for_each(const BVSparse<TAllocator> *bv2);

    template<void (SparseBVUnit::*callback)(SparseBVUnit)>
    void for_each(const BVSparse<TAllocator> *bv1, const BVSparse<TAllocator> *bv2);

// Methods
public:

            BOOLEAN         operator[](BVIndex i) const;
            BOOLEAN         Test(BVIndex i);
            BVIndex         GetNextBit(BVIndex i) const;
            BVIndex         GetNextBit(BVSparseNode * node) const;

            BOOLEAN         TestEmpty() const;
            BOOLEAN         TestAndSet(BVIndex i);
            BOOLEAN         TestAndClear(BVIndex i);
            void            Set(BVIndex i);
            void            Clear(BVIndex i);
            void            Compliment(BVIndex i);


            // this |= bv;
            void            Or(const BVSparse<TAllocator> *bv);
            // this = bv1 | bv2;
            void            Or(const BVSparse<TAllocator> *bv1, const BVSparse<TAllocator> *bv2);
            // newBv = this | bv;
            BVSparse<TAllocator> *      OrNew(const BVSparse<TAllocator> *bv, TAllocator* allocator) const;
            BVSparse<TAllocator> *      OrNew(const BVSparse<TAllocator> *bv) const { return this->OrNew(bv, this->alloc); }

            // this &= bv;
            void            And(const BVSparse<TAllocator> *bv);
            // this = bv1 & bv2;
            void            And(const BVSparse<TAllocator> *bv1, const BVSparse<TAllocator> *bv2);
            // newBv = this & bv;
            BVSparse<TAllocator> *      AndNew(const BVSparse<TAllocator> *bv, TAllocator* allocator) const;
            BVSparse<TAllocator> *      AndNew(const BVSparse<TAllocator> *bv) const { return this->AndNew(bv, this->alloc); }

            // this ^= bv;
            void            Xor(const BVSparse<TAllocator> *bv);
            // this = bv1 ^ bv2;
            void            Xor(const BVSparse<TAllocator> *bv1, const BVSparse<TAllocator> *bv2);
            // newBv = this ^ bv;
            BVSparse<TAllocator> *      XorNew(const BVSparse<TAllocator> *bv, TAllocator* allocator) const;
            BVSparse<TAllocator> *      XorNew(const BVSparse<TAllocator> *bv) const { return this->XorNew(bv, this->alloc); }

            // this -= bv;
            void            Minus(const BVSparse<TAllocator> *bv);
            // this = bv1 - bv2;
            void            Minus(const BVSparse<TAllocator> *bv1, const BVSparse<TAllocator> *bv2);
            // newBv = this - bv;
            BVSparse<TAllocator> *      MinusNew(const BVSparse<TAllocator> *bv, TAllocator* allocator) const;
            BVSparse<TAllocator> *      MinusNew(const BVSparse<TAllocator> *bv) const { return this->MinusNew(bv, this->alloc); }

            template <class TSrcAllocator>
            void            Copy(const BVSparse<TSrcAllocator> *bv);
            BVSparse<TAllocator> *      CopyNew(TAllocator* allocator) const;
            BVSparse<TAllocator> *      CopyNew() const;
            void            ComplimentAll();
            void            ClearAll();

            BVIndex         Count() const;
            bool            IsEmpty() const;
            bool            Equal(BVSparse<TAllocator> const * bv) const;

            // this & bv != empty
            bool            Test(BVSparse const * bv) const;

            void            ToString(__out_ecount(strSize) char *const str, const size_t strSize) const;
            template<class F> void ToString(__out_ecount(strSize) char *const str, const size_t strSize, const F ReadNode) const;

            TAllocator *    GetAllocator() const { return alloc; }
#if DBG_DUMP
            void            Dump() const;
#endif
};

#if DBG_DUMP
template <typename T> void Dump(T const& t);

namespace Memory{ class JitArenaAllocator; }
template<>
inline void Dump(BVSparse<JitArenaAllocator> * const& bv)
{
    bv->Dump();
}

namespace Memory { class Recycler; }
template<>
inline void Dump(BVSparse<Recycler> * const& bv)
{
    bv->Dump();
}
#endif

template <class TAllocator>
const SparseBVUnit BVSparse<TAllocator>::s_EmptyUnit(0);

template <class TAllocator>
BVSparse<TAllocator>::BVSparse(TAllocator* allocator) :
   alloc(allocator),
   head(nullptr)
{
    this->lastUsedNodePrevNextField = &this->head;
}
template <class TAllocator>
void
BVSparse<TAllocator>::QueueInFreeList(BVSparseNode *curNode)
{
    AllocatorDelete(TAllocator, this->alloc, curNode);
}

template <class TAllocator>
BVSparseNode *
BVSparse<TAllocator>::Allocate(const BVIndex searchIndex, BVSparseNode *nextNode)
{
    return AllocatorNew(TAllocator, this->alloc, BVSparseNode, searchIndex, nextNode);
}

template <class TAllocator>
BVSparse<TAllocator>::~BVSparse()
{
    BVSparseNode * curNode = this->head;
    while (curNode != nullptr)
    {
        curNode = this->DeleteNode(curNode);
    }
}


// Searches for a node which would contain the required bit. If not found, then it inserts
// a new node in the appropriate position.
//
template <class TAllocator>
BVSparseNode *
BVSparse<TAllocator>::NodeFromIndex(BVIndex i, BVSparseNode *** prevNextFieldOut, bool create)
{
    const BVIndex searchIndex = SparseBVUnit::Floor(i);

    BVSparseNode ** prevNextField = this->lastUsedNodePrevNextField;
    BVSparseNode * curNode = (*prevNextField);
    if (curNode != nullptr)
    {
        if (curNode->startIndex == searchIndex)
        {
            *prevNextFieldOut = prevNextField;
            return curNode;
        }

        if (curNode->startIndex > searchIndex)
        {
            prevNextField = &this->head;
            curNode = this->head;
        }
    }
    else
    {
        prevNextField = &this->head;
        curNode = this->head;
    }

    for (; curNode && searchIndex > curNode->startIndex; curNode = curNode->next)
    {
        prevNextField = &curNode->next;
    }

    if(curNode && searchIndex == curNode->startIndex)
    {
        *prevNextFieldOut = prevNextField;
        this->lastUsedNodePrevNextField = prevNextField;
        return curNode;
    }

    if(!create)
    {
        return nullptr;
    }

    BVSparseNode * newNode = Allocate(searchIndex, *prevNextField);
    *prevNextField = newNode;
    *prevNextFieldOut = prevNextField;
    this->lastUsedNodePrevNextField = prevNextField;
    return newNode;
}



template <class TAllocator>
SparseBVUnit *
BVSparse<TAllocator>::BitsFromIndex(BVIndex i, bool create)
{
    BVSparseNode ** prevNextField;
    BVSparseNode * node = NodeFromIndex(i, &prevNextField, create);
    if (node)
    {
        return &node->data;
    }
    else
    {
        return (SparseBVUnit *)&BVSparse::s_EmptyUnit;
    }
}


template <class TAllocator>
BVSparseNode *
BVSparse<TAllocator>::DeleteNode(BVSparseNode *node, bool bResetLastUsed)
{
    BVSparseNode *next = node->next;
    QueueInFreeList(node);

    if (bResetLastUsed)
    {
        this->lastUsedNodePrevNextField = &this->head;
    }
    else
    {
        Assert(this->lastUsedNodePrevNextField != &node->next);
    }
    return next;
}

template <class TAllocator>
BVIndex
BVSparse<TAllocator>::GetNextBit(BVSparseNode *node) const
{
    while(0 != node)
    {
        BVIndex ret = node->data.GetNextBit();
        if(-1 != ret)
        {
            return ret + node->startIndex;
        }
    }
    return -1;
}

template <class TAllocator>
BVIndex
BVSparse<TAllocator>::GetNextBit(BVIndex i) const
{
    const BVIndex startIndex = SparseBVUnit::Floor(i);

    for(BVSparseNode * node = this->head; node != 0 ; node = node->next)
    {
        if(startIndex == node->startIndex)
        {
            BVIndex ret = node->data.GetNextBit(SparseBVUnit::Offset(i));
            if(-1 != ret)
            {
                return ret + node->startIndex;
            }
            else
            {
                return GetNextBit(node->next);
            }
        }
        else if(startIndex < node->startIndex)
        {
            return GetNextBit(node->next);
        }
    }

    return  -1;
}

template <class TAllocator>
template <class TOtherAllocator>
void
BVSparse<TAllocator>::AssertBV(const BVSparse<TOtherAllocator> *bv)
{
    AssertMsg(nullptr != bv, "Cannot operate on NULL bitvector");
}

template <class TAllocator>
void
BVSparse<TAllocator>::ClearAll()
{
    BVSparseNode* nextNode;
    for(BVSparseNode * node = this->head; node != 0 ; node = nextNode)
    {
        nextNode = node->next;
        QueueInFreeList(node);
    }
    this->head = nullptr;
    this->lastUsedNodePrevNextField = &this->head;
}

template <class TAllocator>
void
BVSparse<TAllocator>::Set(BVIndex i)
{
    this->BitsFromIndex(i)->Set(SparseBVUnit::Offset(i));
}

template <class TAllocator>
void
BVSparse<TAllocator>::Clear(BVIndex i)
{
    BVSparseNode ** prevNextField;
    BVSparseNode * current = this->NodeFromIndex(i, &prevNextField, false /* create */);
    if(current)
    {
        current->data.Clear(SparseBVUnit::Offset(i));
        if (current->data.IsEmpty())
        {
            *prevNextField = this->DeleteNode(current, false);
        }
    }
}

template <class TAllocator>
void
BVSparse<TAllocator>::Compliment(BVIndex i)
{
    this->BitsFromIndex(i)->Complement(SparseBVUnit::Offset(i));
}

template <class TAllocator>
BOOLEAN
BVSparse<TAllocator>::TestEmpty() const
{
    return this->head != nullptr;
}

template <class TAllocator>
BOOLEAN
BVSparse<TAllocator>::Test(BVIndex i)
{
    return this->BitsFromIndex(i, false)->Test(SparseBVUnit::Offset(i));
}

template <class TAllocator>
BOOLEAN
BVSparse<TAllocator>::TestAndSet(BVIndex i)
{
    SparseBVUnit * bvUnit = this->BitsFromIndex(i);
    BVIndex bvIndex = SparseBVUnit::Offset(i);
    BOOLEAN bit = bvUnit->Test(bvIndex);
    bvUnit->Set(bvIndex);
    return bit;
}

template <class TAllocator>
BOOLEAN
BVSparse<TAllocator>::TestAndClear(BVIndex i)
{
    BVSparseNode ** prevNextField;
    BVSparseNode * current = this->NodeFromIndex(i, &prevNextField);
    BVIndex bvIndex = SparseBVUnit::Offset(i);
    BOOLEAN bit = current->data.Test(bvIndex);
    current->data.Clear(bvIndex);
    if (current->data.IsEmpty())
    {
        *prevNextField = this->DeleteNode(current, false);
    }
    return bit;
}

template <class TAllocator>
BOOLEAN
BVSparse<TAllocator>::operator[](BVIndex i) const
{
    return this->Test(i);
}

template<class TAllocator>
template<void (SparseBVUnit::*callback)(SparseBVUnit)>
void BVSparse<TAllocator>::for_each(const BVSparse *bv2)
{
    Assert(callback == &SparseBVUnit::And || callback == &SparseBVUnit::Or || callback == &SparseBVUnit::Xor || callback == &SparseBVUnit::Minus);
    AssertBV(bv2);

          BVSparseNode * node1      = this->head;
    const BVSparseNode * node2      = bv2->head;
          BVSparseNode ** prevNodeNextField   = &this->head;

    while(node1 != nullptr && node2 != nullptr)
    {
        if(node2->startIndex == node1->startIndex)
        {
            (node1->data.*callback)(node2->data);
            prevNodeNextField = &node1->next;
            node1 = node1->next;
            node2 = node2->next;
        }
        else if(node2->startIndex > node1->startIndex)
        {

            if (callback == &SparseBVUnit::And)
            {
                node1 = this->DeleteNode(node1);
                *prevNodeNextField = node1;
            }
            else
            {
                prevNodeNextField = &node1->next;
                node1 = node1->next;
            }

        }
        else
        {
            if (callback == &SparseBVUnit::Or || callback == &SparseBVUnit::Xor)
            {
                BVSparseNode * newNode = Allocate(node2->startIndex, node1);
                (newNode->data.*callback)(node2->data);
                *prevNodeNextField = newNode;
                prevNodeNextField = &newNode->next;
            }
            node2 = node2->next;
        }
    }

    if (callback == &SparseBVUnit::And)
    {
        while (node1 != nullptr)
        {
            node1 = this->DeleteNode(node1);
        }
        *prevNodeNextField = nullptr;
    }
    else if (callback == &SparseBVUnit::Or || callback == &SparseBVUnit::Xor)
    {
        while(node2 != 0)
        {
            Assert(*prevNodeNextField == nullptr);
            BVSparseNode * newNode = Allocate(node2->startIndex, nullptr);
            *prevNodeNextField = newNode;

            (newNode->data.*callback)(node2->data);
            node2       = node2->next;
            prevNodeNextField    = &newNode->next;
        }
    }
}

template<class TAllocator>
template<void (SparseBVUnit::*callback)(SparseBVUnit)>
void BVSparse<TAllocator>::for_each(const BVSparse *bv1, const BVSparse *bv2)
{
    Assert(callback == &SparseBVUnit::And || callback == &SparseBVUnit::Or || callback == &SparseBVUnit::Xor || callback == &SparseBVUnit::Minus);
    Assert(this->IsEmpty());
    AssertBV(bv1);
    AssertBV(bv2);

          BVSparseNode * node1      = bv1->head;
    const BVSparseNode * node2      = bv2->head;
          BVSparseNode * lastNode   = nullptr;
          BVSparseNode ** prevNextField = &this->head;

    while(node1 != nullptr && node2 != nullptr)
    {
        lastNode = node1;
        BVIndex startIndex;
        SparseBVUnit  bvUnit1;
        SparseBVUnit  bvUnit2;

        if (node2->startIndex == node1->startIndex)
        {
            startIndex = node1->startIndex;
            bvUnit1 = node1->data;
            bvUnit2 = node2->data;
            node1 = node1->next;
            node2 = node2->next;
        }
        else if (node2->startIndex > node1->startIndex)
        {
            startIndex = node1->startIndex;
            bvUnit1 = node1->data;
            node1 = node1->next;
        }
        else
        {
            startIndex = node2->startIndex;
            bvUnit2 = node2->data;
            node2 = node2->next;
        }

        (bvUnit1.*callback)(bvUnit2);
        if (!bvUnit1.IsEmpty())
        {
            BVSparseNode * newNode = Allocate(startIndex, nullptr);
            newNode->data = bvUnit1;
            *prevNextField = newNode;
            prevNextField = &newNode->next;
        }
    }


    if (callback == &SparseBVUnit::Minus || callback == &SparseBVUnit::Or || callback == &SparseBVUnit::Xor)
    {
        BVSparseNode const * copyNode = (callback == &SparseBVUnit::Minus || node1 != nullptr)? node1 : node2;

        while (copyNode != nullptr)
        {
            if (!copyNode->data.IsEmpty())
            {
                BVSparseNode * newNode = Allocate(copyNode->startIndex, nullptr);
                newNode->data = copyNode->data;
                *prevNextField = newNode;
                prevNextField = &newNode->next;
            }
            copyNode = copyNode->next;
        }
    }
}

template <class TAllocator>
void
BVSparse<TAllocator>::Or(const BVSparse*bv)
{
    this->for_each<&SparseBVUnit::Or>(bv);
}

template <class TAllocator>
void
BVSparse<TAllocator>::Or(const BVSparse * bv1, const BVSparse * bv2)
{
    this->ClearAll();
    this->for_each<&SparseBVUnit::Or>(bv1, bv2);
}

template <class TAllocator>
BVSparse<TAllocator> *
BVSparse<TAllocator>::OrNew(const BVSparse* bv,  TAllocator* allocator) const
{
    BVSparse * newBv = AllocatorNew(TAllocator, allocator, BVSparse, allocator);
    newBv->for_each<&SparseBVUnit::Or>(this, bv);
    return newBv;
}

template <class TAllocator>
void
BVSparse<TAllocator>::And(const BVSparse*bv)
{
    this->for_each<&SparseBVUnit::And>(bv);
}

template <class TAllocator>
void
BVSparse<TAllocator>::And(const BVSparse * bv1, const BVSparse * bv2)
{
    this->ClearAll();
    this->for_each<&SparseBVUnit::And>(bv1, bv2);
}

template <class TAllocator>
BVSparse<TAllocator> *
BVSparse<TAllocator>::AndNew(const BVSparse* bv, TAllocator* allocator) const
{
    BVSparse * newBv = AllocatorNew(TAllocator, allocator, BVSparse, allocator);
    newBv->for_each<&SparseBVUnit::And>(this, bv);
    return newBv;
}

template <class TAllocator>
void
BVSparse<TAllocator>::Xor(const BVSparse*bv)
{
    this->for_each<&SparseBVUnit::Xor>(bv);
}

template <class TAllocator>
void
BVSparse<TAllocator>::Xor(const BVSparse * bv1, const BVSparse * bv2)
{
    this->ClearAll();
    this->for_each<&SparseBVUnit::Xor>(bv1, bv2);
}

template <class TAllocator>
BVSparse<TAllocator> *
BVSparse<TAllocator>::XorNew(const BVSparse* bv, TAllocator* allocator) const
{
    BVSparse * newBv = AllocatorNew(TAllocator, allocator, BVSparse, allocator);
    newBv->for_each<&SparseBVUnit::Xor>(this, bv);
    return newBv;
}

template <class TAllocator>
void
BVSparse<TAllocator>::Minus(const BVSparse*bv)
{
    this->for_each<&SparseBVUnit::Minus>(bv);
}

template <class TAllocator>
void
BVSparse<TAllocator>::Minus(const BVSparse * bv1, const BVSparse * bv2)
{
    this->ClearAll();
    this->for_each<&SparseBVUnit::Minus>(bv1, bv2);
}

template <class TAllocator>
BVSparse<TAllocator> *
BVSparse<TAllocator>::MinusNew(const BVSparse* bv, TAllocator* allocator) const
{
    BVSparse * newBv = AllocatorNew(TAllocator, allocator, BVSparse, allocator);
    newBv->for_each<&SparseBVUnit::Minus>(this, bv);
    return newBv;
}

template <class TAllocator>
template <class TSrcAllocator>
void
BVSparse<TAllocator>::Copy(const BVSparse<TSrcAllocator> * bv2)
{
    AssertBV(bv2);

          BVSparseNode * node1      = this->head;
    const BVSparseNode * node2      = bv2->head;
          BVSparseNode ** prevNextField = &this->head;

    while (node1 != nullptr && node2 != nullptr)
    {
        if (!node2->data.IsEmpty())
        {
            node1->startIndex = node2->startIndex;
            node1->data.Copy(node2->data);
            prevNextField = &node1->next;
            node1 = node1->next;
        }

        node2 = node2->next;
    }

    if (node1 != nullptr)
    {
        while (node1 != nullptr)
        {
            node1 = this->DeleteNode(node1);
        }
        *prevNextField = nullptr;
    }
    else
    {
        while (node2 != nullptr)
        {
            if (!node2->data.IsEmpty())
            {
                BVSparseNode * newNode = Allocate(node2->startIndex, nullptr);
                newNode->data.Copy(node2->data);
                *prevNextField = newNode;
                prevNextField = &newNode->next;
            }
            node2 = node2->next;
        }
    }
}

template <class TAllocator>
BVSparse<TAllocator> *
BVSparse<TAllocator>::CopyNew(TAllocator* allocator) const
{
    BVSparse * bv = AllocatorNew(TAllocator, allocator, BVSparse, allocator);
    bv->Copy(this);
    return bv;
}

template <class TAllocator>
BVSparse<TAllocator> *
BVSparse<TAllocator>::CopyNew() const
{
    return this->CopyNew(this->alloc);
}

template <class TAllocator>
void
BVSparse<TAllocator>::ComplimentAll()
{
    for(BVSparseNode * node = this->head; node != 0 ; node = node->next)
    {
        node->data.ComplimentAll();
    }
}

template <class TAllocator>
BVIndex
BVSparse<TAllocator>::Count() const
{
    BVIndex sum = 0;
    for(BVSparseNode * node = this->head; node != 0 ; node = node->next)
    {
        sum += node->data.Count();
    }
    return sum;
}

template <class TAllocator>
bool
BVSparse<TAllocator>::IsEmpty() const
{
    for(BVSparseNode * node = this->head; node != 0 ; node = node->next)
    {
        if (!node->data.IsEmpty())
        {
            return false;
        }
    }
    return true;
}

template <class TAllocator>
bool
BVSparse<TAllocator>::Equal(BVSparse const * bv) const
{
    BVSparseNode const * bvNode1 = this->head;
    BVSparseNode const * bvNode2 = bv->head;

    while (true)
    {
        while (bvNode1 != nullptr && bvNode1->data.IsEmpty())
        {
            bvNode1 = bvNode1->next;
        }
        while (bvNode2 != nullptr && bvNode2->data.IsEmpty())
        {
            bvNode2 = bvNode2->next;
        }
        if (bvNode1 == nullptr)
        {
            return (bvNode2 == nullptr);
        }
        if (bvNode2 == nullptr)
        {
            return false;
        }
        if (bvNode1->startIndex != bvNode2->startIndex)
        {
            return false;
        }
        if (!bvNode1->data.Equal(bvNode2->data))
        {
            return false;
        }
        bvNode1 = bvNode1->next;
        bvNode2 = bvNode2->next;
    }
}

template <class TAllocator>
bool
BVSparse<TAllocator>::Test(BVSparse const * bv) const
{
    BVSparseNode const * bvNode1 = this->head;
    BVSparseNode const * bvNode2 = bv->head;

    while (bvNode1 != nullptr && bvNode2 != nullptr)
    {
        if (bvNode1->data.IsEmpty() || bvNode1->startIndex < bvNode2->startIndex)
        {
            bvNode1 = bvNode1->next;
            continue;
        }
        if (bvNode2->data.IsEmpty() || bvNode1->startIndex > bvNode2->startIndex)
        {
            bvNode2 = bvNode2->next;
            continue;
        }
        Assert(bvNode1->startIndex == bvNode2->startIndex);
        if (bvNode1->data.Test(bvNode2->data))
        {
            return true;
        }
        bvNode1 = bvNode1->next;
        bvNode2 = bvNode2->next;
    }

    return false;
}

template<class TAllocator>
template<class F>
void BVSparse<TAllocator>::ToString(__out_ecount(strSize) char *const str, const size_t strSize, const F ReadNode) const
{
    Assert(str);

    if(strSize == 0)
    {
        return;
    }
    str[0] = '\0';

    bool empty = true;
    bool isFirstInSequence = true;
    size_t length = 0;
    BVSparseNode *nodePtr = head;
    while(nodePtr)
    {
        bool readSuccess;
        const BVSparseNode node(ReadNode(nodePtr, &readSuccess));
        if(!readSuccess)
        {
            str[0] = '\0';
            return;
        }
        if(node.data.IsEmpty())
        {
            nodePtr = node.next;
            continue;
        }
        empty = false;

        size_t writtenLength;
        if(!node.ToString(&str[length], strSize - length, &writtenLength, true, isFirstInSequence, !node.next))
        {
            return;
        }
        length += writtenLength;

        isFirstInSequence = false;
        nodePtr = node.next;
    }

    if(empty && _countof("{}") < strSize)
    {
        strcpy_s(str, strSize, "{}");
    }
}

template<class TAllocator>
void BVSparse<TAllocator>::ToString(__out_ecount(strSize) char *const str, const size_t strSize) const
{
    ToString(
        str,
        strSize,
        [](BVSparseNode *const nodePtr, bool *const successRef) -> BVSparseNode
        {
            Assert(nodePtr);
            Assert(successRef);

            *successRef = true;
            return *nodePtr;
        });
}

#if DBG_DUMP

template <class TAllocator>
void
BVSparse<TAllocator>::Dump() const
{
    bool hasBits = false;
    Output::Print(L"[  ");
    for(BVSparseNode * node = this->head; node != 0 ; node = node->next)
    {
        hasBits = node->data.Dump(node->startIndex, hasBits);
    }
    Output::Print(L"]\n");
}
#endif
