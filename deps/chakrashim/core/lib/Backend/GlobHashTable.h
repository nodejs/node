//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#if PROFILE_DICTIONARY
#include "DictionaryStats.h"
#endif

template <typename TData, typename TElement>
class HashBucket
{
public:
    TElement element;
    TData   value;

public:
    HashBucket() : element(NULL), value(NULL) {}
    static void Copy(HashBucket const& bucket, HashBucket& newBucket)
    {
        newBucket.element = bucket.element;
        newBucket.value = bucket.value;
    }
};

class Key
{
public:
    static uint Get(Sym *sym) { return static_cast<uint>(sym->m_id); }
    static uint Get(ExprHash hash) { return static_cast<uint>(hash); }
};

#define FOREACH_GLOBHASHTABLE_ENTRY(bucket, hashTable) \
    for (uint _iterHash = 0; _iterHash < (hashTable)->tableSize; _iterHash++)  \
    {   \
        FOREACH_SLISTBASE_ENTRY(GlobHashBucket, bucket, &(hashTable)->table[_iterHash]) \
        {


#define NEXT_GLOBHASHTABLE_ENTRY \
        } \
        NEXT_SLISTBASE_ENTRY; \
    }

template<typename TData, typename TElement>
class ValueHashTable
{
private:
    typedef HashBucket<TData, TElement> HashBucket;

public:
    JitArenaAllocator *        alloc;
    uint                    tableSize;
    SListBase<HashBucket> * table;

public:
    static ValueHashTable * New(JitArenaAllocator *allocator, uint tableSize)
    {
        return AllocatorNewPlus(JitArenaAllocator, allocator, (tableSize*sizeof(SListBase<HashBucket>)), ValueHashTable, allocator, tableSize);
    }

    void Delete()
    {
        AllocatorDeletePlus(JitArenaAllocator, alloc, (tableSize*sizeof(SListBase<HashBucket>)), this);
    }

    ~ValueHashTable()
    {
        for (uint i = 0; i< tableSize; i++)
        {
            table[i].Clear(alloc);
        }
    }

    SListBase<HashBucket> * SwapBucket(SListBase<HashBucket> * newTable)
    {
        SListBase<HashBucket> * retTable = table;
        table = newTable;
        return retTable;
    }

    TElement * FindOrInsertNew(TData value)
    {
        uint key = Key::Get(value);
        uint hash = this->Hash(key);

#if PROFILE_DICTIONARY
        uint depth = 1;
#endif
        // Keep sorted
        FOREACH_SLISTBASE_ENTRY_EDITING(HashBucket, bucket, &this->table[hash], iter)
        {
            if (Key::Get(bucket.value) <= key)
            {
                if (Key::Get(bucket.value) == key)
                {
                    return &(bucket.element);
                }
                break;
            }
#if PROFILE_DICTIONARY
            ++depth;
#endif
        } NEXT_SLISTBASE_ENTRY_EDITING;

        HashBucket * newBucket = iter.InsertNodeBefore(this->alloc);
        newBucket->value = value;
#if PROFILE_DICTIONARY
        if (stats)
            stats->Insert(depth);
#endif
        return &newBucket->element;
    }

    TElement * FindOrInsertNewNoThrow(TData * value)
    {
        uint key = Key::Get(value);
        uint hash = this->Hash(key);

#if PROFILE_DICTIONARY
        uint depth = 1;
#endif
        // Keep sorted
        FOREACH_SLISTBASE_ENTRY_EDITING(HashBucket, bucket, &this->table[hash], iter)
        {
            if (Key::Get(bucket.value) <= key)
            {
                if (Key::Get(bucket.value) == key)
                {
                    return &(bucket.element);
                }
                break;
            }
#if PROFILE_DICTIONARY
            ++depth;
#endif
        } NEXT_SLISTBASE_ENTRY_EDITING;

        HashBucket * newBucket = iter.InsertNodeBeforeNoThrow(this->alloc);
        if (newBucket == nullptr)
        {
            return nullptr;
        }
        newBucket->value = value;
#if PROFILE_DICTIONARY
        if (stats)
            stats->Insert(depth);
#endif
        return &newBucket->element;
    }

    TElement * FindOrInsert(TElement element, TData value)
    {
        uint key = Key::Get(value);
        uint hash = this->Hash(key);

#if PROFILE_DICTIONARY
        uint depth = 1;
#endif
        // Keep sorted
        FOREACH_SLISTBASE_ENTRY_EDITING(HashBucket, bucket, &this->table[hash], iter)
        {
            if (Key::Get(bucket.value) <= key)
            {
                if (Key::Get(bucket.value) == key)
                {
                    return &(bucket.element);
                }
                break;
            }
#if PROFILE_DICTIONARY
            ++depth;
#endif
        } NEXT_SLISTBASE_ENTRY_EDITING;

        HashBucket * newBucket = iter.InsertNodeBefore(this->alloc);
        Assert(newBucket != nullptr);
        newBucket->value = value;
        newBucket->element = element;
#if PROFILE_DICTIONARY
        if (stats)
            stats->Insert(depth);
#endif
        return NULL;
    }

    TElement * Get(TData value)
    {
        uint key = Key::Get(value);
        return Get(key);
    }

    TElement * Get(uint key)
    {
        uint hash = this->Hash(key);
        // Assumes sorted lists
        FOREACH_SLISTBASE_ENTRY(HashBucket, bucket, &this->table[hash])
        {
            if (Key::Get(bucket.value) <= key)
            {
                if (Key::Get(bucket.value) == key)
                {
                    return &(bucket.element);
                }
                break;
            }
        } NEXT_SLISTBASE_ENTRY;

        return NULL;
    }

    TElement GetAndClear(TData * value)
    {
        uint key = Key::Get(value);
        uint hash = this->Hash(key);
        SListBase<HashBucket> * list = &this->table[hash];

#if PROFILE_DICTIONARY
        bool first = true;
#endif
        // Assumes sorted lists
        FOREACH_SLISTBASE_ENTRY_EDITING(HashBucket, bucket, list, iter)
        {
            if (Key::Get(bucket.value) <= key)
            {
                if (Key::Get(bucket.value) == key)
                {
                    TElement retVal = bucket.element;
                    iter.RemoveCurrent(this->alloc);
#if PROFILE_DICTIONARY
                    if (stats)
                        stats->Remove(first && !(iter.Next()));
#endif
                    return retVal;
                }
                break;
            }
#if PROFILE_DICTIONARY
            first = false;
#endif
        } NEXT_SLISTBASE_ENTRY_EDITING;
        return nullptr;
    }

    void Clear(uint key)
    {
        uint hash = this->Hash(key);
        SListBase<HashBucket> * list = &this->table[hash];

        // Assumes sorted lists
#if PROFILE_DICTIONARY
        bool first = true;
#endif
        FOREACH_SLISTBASE_ENTRY_EDITING(HashBucket, bucket, list, iter)
        {
            if (Key::Get(bucket.value) <= key)
            {
                if (Key::Get(bucket.value) == key)
                {
                    iter.RemoveCurrent(this->alloc);
#if PROFILE_DICTIONARY
                    if (stats)
                        stats->Remove(first && !(iter.Next()));
#endif
                }
                return;
            }
#if PROFILE_DICTIONARY
        first = false;
#endif
        } NEXT_SLISTBASE_ENTRY_EDITING;
    }

    void And(ValueHashTable *this2)
    {
        for (uint i = 0; i < this->tableSize; i++)
        {
            SListBase<HashBucket>::Iterator iter2(&this2->table[i]);
            iter2.Next();
            FOREACH_SLISTBASE_ENTRY_EDITING(HashBucket, bucket, &this->table[i], iter)
            {
                while (iter2.IsValid() && bucket.value < iter2.Data().value)
                {
                    iter2.Next();
                }

                if (!iter2.IsValid() || bucket.value != iter2.Data().value || bucket.element != iter2.Data().element)
                {
                    iter.RemoveCurrent(this->alloc);
#if PROFILE_DICTIONARY
                    if (stats)
                        stats->Remove(false);
#endif
                    continue;
                }
                else
                {
                    AssertMsg(bucket.value == iter2.Data().value && bucket.element == iter2.Data().element, "Huh??");
                }
                iter2.Next();
            } NEXT_SLISTBASE_ENTRY_EDITING;
        }
    }

    template <class Fn>
    void Or(ValueHashTable * this2, Fn fn)
    {
        for (uint i = 0; i < this->tableSize; i++)
        {
            SListBase<HashBucket>::Iterator iter2(&this2->table[i]);
            iter2.Next();
            FOREACH_SLISTBASE_ENTRY_EDITING((HashBucket), bucket, &this->table[i], iter)
            {
                while (iter2.IsValid() && bucket.value < iter2.Data().value)
                {
                    HashBucket * newBucket = iter.InsertNodeBefore(this->alloc);
                    newBucket->value = iter2.Data().value;
                    newBucket->element = fn(null, iter2.Data().element);
                    iter2.Next();
                }

                if (!iter2.IsValid())
                {
                    break;
                }
                if (bucket.value == iter2.Data().value)
                {
                    bucket.element = fn(bucket.element, iter2.Data().element);
                    iter2.Next();
                }
            } NEXT_SLISTBASE_ENTRY_EDITING;

            while (iter2.IsValid())
            {
                HashBucket * newBucket = iter.InsertNodeBefore(this->alloc);
                newBucket->value = iter2.Data().value;
                newBucket->element = fn(null, iter2.Data().element);
                iter2.Next();
            }
        }
    }

    ValueHashTable *Copy()
    {
        ValueHashTable *newTable = ValueHashTable::New(this->alloc, this->tableSize);

        for (uint i = 0; i < this->tableSize; i++)
        {
            this->table[i].CopyTo<HashBucket::Copy>(this->alloc, newTable->table[i]);
        }
#if PROFILE_DICTIONARY
        if (stats)
            newTable->stats = stats->Clone();
#endif
        return newTable;
    }

    void ClearAll()
    {
        for (uint i = 0; i < this->tableSize; i++)
        {
            this->table[i].Clear(this->alloc);
        }
#if PROFILE_DICTIONARY
        // To not lose previously collected data, we will treat cleared dictionary as a separate instance for stats tracking purpose
        stats = DictionaryStats::Create(typeid(this).name(), tableSize);
#endif
    }

#if DBG_DUMP
    void Dump()
    {
        FOREACH_GLOBHASHTABLE_ENTRY(bucket, this)
        {

            Output::Print(L"%4d  =>  ", bucket.value);
            bucket.element->Dump();
            Output::Print(L"\n");
            Output::Print(L"\n");
        }
        NEXT_GLOBHASHTABLE_ENTRY;
    }

    void Dump(void (*valueDump)(TData))
    {
        Output::Print(L"\n-------------------------------------------------------------------------------------------------\n");
        FOREACH_GLOBHASHTABLE_ENTRY(bucket, this)
        {
            valueDump(bucket.value);
            Output::Print(L"  =>  ", bucket.value);
            bucket.element->Dump();
            Output::Print(L"\n");
        }
        NEXT_GLOBHASHTABLE_ENTRY;
    }
#endif

protected:
    ValueHashTable(JitArenaAllocator * allocator, uint tableSize) : alloc(allocator), tableSize(tableSize)
    {
        Init();
#if PROFILE_DICTIONARY
        stats = DictionaryStats::Create(typeid(this).name(), tableSize);
#endif
    }
    void Init()
    {
        table = (SListBase<HashBucket> *)(((char *)this) + sizeof(ValueHashTable));
        for (uint i = 0; i < tableSize; i++)
        {
            // placement new
            ::new (&table[i]) SListBase<HashBucket>();
        }
    }
private:
    uint         Hash(uint key) { return (key % this->tableSize); }

#if PROFILE_DICTIONARY
    DictionaryStats *stats;
#endif
};

