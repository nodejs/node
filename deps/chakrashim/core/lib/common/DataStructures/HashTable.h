//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#if PROFILE_DICTIONARY
#include "DictionaryStats.h"
#endif

template<typename T>
class Bucket
{
public:
    T       element;
    int     value;

public:
    Bucket() : element(), value(0) {}
    static void Copy(Bucket<T> const& bucket, Bucket<T>& newBucket)
    {
        bucket.element.Copy(&(newBucket.element));
        newBucket.value = bucket.value;
    }
};

template<typename T, typename TAllocator = ArenaAllocator>
class HashTable
{
public:
    TAllocator *        alloc;
    uint                tableSize;
    SListBase<Bucket<T>> *  table;

public:
    static HashTable<T, TAllocator> * New(TAllocator *allocator, uint tableSize)
    {
        return AllocatorNewPlus(TAllocator, allocator, (tableSize*sizeof(SListBase<Bucket<T>>)), HashTable, allocator, tableSize);
    }

    void Delete()
    {
        AllocatorDeletePlus(TAllocator, alloc, (tableSize*sizeof(SListBase<Bucket<T>>)), this);
    }

    ~HashTable()
    {
        for (uint i = 0; i< tableSize; i++)
        {
            table[i].Clear(alloc);
        }
    }

    SListBase<Bucket<T>> * SwapBucket(SListBase<Bucket<T>> * newTable)
    {
        SListBase<Bucket<T>> * retTable = table;
        table = newTable;
        return retTable;
    }

    T * FindOrInsertNew(int value)
    {
        uint hash = this->Hash(value);

#if PROFILE_DICTIONARY
        uint depth = 1;
#endif
        // Keep sorted
        FOREACH_SLISTBASE_ENTRY_EDITING(Bucket<T>, bucket, &this->table[hash], iter)
        {
            if (bucket.value <= value)
            {
                if (bucket.value == value)
                {
                    return &(bucket.element);
                }
                break;
            }
#if PROFILE_DICTIONARY
            ++depth;
#endif
        } NEXT_SLISTBASE_ENTRY_EDITING;

        Bucket<T> * newBucket = iter.InsertNodeBefore(this->alloc);
        newBucket->value = value;
#if PROFILE_DICTIONARY
        if (stats)
            stats->Insert(depth);
#endif
        return &newBucket->element;
    }

    T * FindOrInsertNewNoThrow(int value)
    {
        uint hash = this->Hash(value);

#if PROFILE_DICTIONARY
        uint depth = 1;
#endif
        // Keep sorted
        FOREACH_SLISTBASE_ENTRY_EDITING(Bucket<T>, bucket, &this->table[hash], iter)
        {
            if (bucket.value <= value)
            {
                if (bucket.value == value)
                {
                    return &(bucket.element);
                }
                break;
            }
#if PROFILE_DICTIONARY
            ++depth;
#endif
        } NEXT_SLISTBASE_ENTRY_EDITING;

        Bucket<T> * newBucket = iter.InsertNodeBeforeNoThrow(this->alloc);
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

    T * FindOrInsert(T element, int value)
    {
        uint hash = this->Hash(value);

#if PROFILE_DICTIONARY
        uint depth = 1;
#endif
        // Keep sorted
        FOREACH_SLISTBASE_ENTRY_EDITING(Bucket<T>, bucket, &this->table[hash], iter)
        {
            if (bucket.value <= value)
            {
                if (bucket.value == value)
                {
                    return &(bucket.element);
                }
                break;
            }
#if PROFILE_DICTIONARY
            ++depth;
#endif
        } NEXT_SLISTBASE_ENTRY_EDITING;

        Bucket<T> * newBucket = iter.InsertNodeBefore(this->alloc);
        Assert(newBucket != nullptr);
        newBucket->value = value;
        newBucket->element = element;
#if PROFILE_DICTIONARY
        if (stats)
            stats->Insert(depth);
#endif
        return nullptr;
    }

    T * Get(int value)
    {
        // Assumes sorted lists
        FOREACH_SLISTBASE_ENTRY(Bucket<T>, bucket, &this->table[this->Hash(value)])
        {
            if (bucket.value <= value)
            {
                if (bucket.value == value)
                {
                    return &(bucket.element);
                }
                break;
            }
        } NEXT_SLISTBASE_ENTRY;

        return nullptr;
    }

    T GetAndClear(int value)
    {
        SListBase<Bucket<T>> * list = &this->table[this->Hash(value)];

#if PROFILE_DICTIONARY
        bool first = true;
#endif
        // Assumes sorted lists
        FOREACH_SLISTBASE_ENTRY_EDITING(Bucket<T>, bucket, list, iter)
        {
            if (bucket.value <= value)
            {
                if (bucket.value == value)
                {
                    T retVal = bucket.element;
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
        return T();
    }

    void Clear(int value)
    {
        SListBase<Bucket<T>> * list = &this->table[this->Hash(value)];

        // Assumes sorted lists
#if PROFILE_DICTIONARY
        bool first = true;
#endif
        FOREACH_SLISTBASE_ENTRY_EDITING(Bucket<T>, bucket, list, iter)
        {
            if (bucket.value <= value)
            {
                if (bucket.value == value)
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

    void And(HashTable<T> *this2)
    {
        for (uint i = 0; i < this->tableSize; i++)
        {
            SListBase<Bucket<T>>::Iterator iter2(&this2->table[i]);
            iter2.Next();
            FOREACH_SLISTBASE_ENTRY_EDITING(Bucket<T>, bucket, &this->table[i], iter)
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

    // "And" with fixup actions to take when data don't make it to the result.
    template <class FnFrom, class FnTo>
    void AndWithFixup(HashTable<T> *this2, FnFrom fnFixupFrom, FnTo fnFixupTo)
    {
        for (uint i = 0; i < this->tableSize; i++)
        {
            SListBase<Bucket<T>>::Iterator iter2(&this2->table[i]);
            iter2.Next();
            FOREACH_SLISTBASE_ENTRY_EDITING(Bucket<T>, bucket, &this->table[i], iter)
            {
                while (iter2.IsValid() && bucket.value < iter2.Data().value)
                {
                    // Skipping a this2 value.
                    fnFixupTo(iter2.Data());
                    iter2.Next();
                }

                if (!iter2.IsValid() || bucket.value != iter2.Data().value || bucket.element != iter2.Data().element)
                {
                    // Skipping a this value.
                    fnFixupFrom(bucket);
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
            while (iter2.IsValid())
            {
                // Skipping a this2 value.
                fnFixupTo(iter2.Data());
                iter2.Next();
            }
        }
    }

    template <class Fn>
    void Or(HashTable<T> * this2, Fn fn)
    {
        for (uint i = 0; i < this->tableSize; i++)
        {
            SListBase<Bucket<T>>::Iterator iter2(&this2->table[i]);
            iter2.Next();
            FOREACH_SLISTBASE_ENTRY_EDITING(Bucket<T>, bucket, &this->table[i], iter)
            {
                while (iter2.IsValid() && bucket.value < iter2.Data().value)
                {
                    Bucket<T> * newBucket = iter.InsertNodeBefore(this->alloc);
                    newBucket->value = iter2.Data().value;
                    newBucket->element = fn(nullptr, iter2.Data().element);
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
                Bucket<T> * newBucket = iter.InsertNodeBefore(this->alloc);
                newBucket->value = iter2.Data().value;
                newBucket->element = fn(nullptr, iter2.Data().element);
                iter2.Next();
            }
        }
    }

    HashTable<T> *Copy()
    {
        HashTable<T> *newTable = HashTable<T>::New(this->alloc, this->tableSize);

        for (uint i = 0; i < this->tableSize; i++)
        {
            this->table[i].CopyTo<Bucket<T>::Copy>(this->alloc, newTable->table[i]);
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
    void Dump(uint newLinePerEntry = 2);
    void Dump(void (*valueDump)(int));
#endif

protected:
    HashTable(TAllocator * allocator, uint tableSize) : alloc(allocator), tableSize(tableSize)
    {
        Init();
#if PROFILE_DICTIONARY
        stats = DictionaryStats::Create(typeid(this).name(), tableSize);
#endif
    }
    void Init()
    {
        table = (SListBase<Bucket<T>> *)(((char *)this) + sizeof(HashTable<T>));
        for (uint i = 0; i < tableSize; i++)
        {
            // placement new
            ::new (&table[i]) SListBase<Bucket<T>>();
        }
    }
private:
    uint         Hash(int value) { return (value % this->tableSize); }

#if PROFILE_DICTIONARY
    DictionaryStats *stats;
#endif
};

template <typename T, uint size, typename TAllocator = ArenaAllocator>
class HashTableS : public HashTable<T, TAllocator>
{
public:
    HashTableS(TAllocator * allocator) : HashTable(allocator, size) {}
    void Reset()
    {
        __super::Init();
    }
private:
    char tableSpace[size * sizeof(SListBase<Bucket<T>>)];
};

#define FOREACH_HASHTABLE_ENTRY(T, bucket, hashTable) \
    for (uint _iterHash = 0; _iterHash < (hashTable)->tableSize; _iterHash++)  \
    {   \
        FOREACH_SLISTBASE_ENTRY(Bucket<T>, bucket, &(hashTable)->table[_iterHash]) \
        {

#define NEXT_HASHTABLE_ENTRY \
        } \
        NEXT_SLISTBASE_ENTRY; \
    }

#if DBG_DUMP
template <typename T, typename TAllocator>
inline void
HashTable<T, TAllocator>::Dump(uint newLinePerEntry)
{
    FOREACH_HASHTABLE_ENTRY(T, bucket, this)
    {

        Output::Print(L"%4d  =>  ", bucket.value);
        ::Dump<T>(bucket.element);
        for (uint i = 0; i < newLinePerEntry; i++)
        {
            Output::Print(L"\n");
        }
    }
    NEXT_HASHTABLE_ENTRY;
}

template <typename T, typename TAllocator>
inline void
HashTable<T, TAllocator>::Dump(void (*valueDump)(int))
{
    FOREACH_HASHTABLE_ENTRY(T, bucket, this)
    {
        valueDump(bucket.value);
        Output::Print(L"  =>  ", bucket.value);
        ::Dump<T>(bucket.element);
        Output::Print(L"\n");
    }
    NEXT_HASHTABLE_ENTRY;
}

template <typename T>
inline void Dump(T const& t)
{
    t.Dump();
}
#endif
