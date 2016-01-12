//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

template<typename TKey, typename TData>
struct SimpleHashEntry {
    TKey key;
    TData value;
    SimpleHashEntry *next;
};

// Size should be a power of 2 for optimal performance
template<
    typename TKey,
    typename TData,
    typename TAllocator = ArenaAllocator,
    template <typename DataOrKey> class Comparer = DefaultComparer,
    bool resize = false,
    typename SizePolicy = PowerOf2Policy>
class SimpleHashTable
{
    typedef SimpleHashEntry<TKey, TData> EntryType;

    // REVIEW: Consider 5 or 7 as multiplication of these might be faster.
    static const int MaxAverageChainLength = 6;

    TAllocator *allocator;
    EntryType **table;
    EntryType *free;
    uint count;
    uint size;
    uint freecount;
    bool disableResize;
#if PROFILE_DICTIONARY
    DictionaryStats *stats;
#endif
public:
    SimpleHashTable(TAllocator *allocator) :
        allocator(allocator),
        count(0),
        freecount(0)
    {
        this->size = SizePolicy::GetSize(64);
        Initialize();
    }

    SimpleHashTable(uint size, TAllocator* allocator) :
        allocator(allocator),
        count(0),
        freecount(0)
    {
        this->size = SizePolicy::GetSize(size);
        Initialize();
    }

    void Initialize()
    {
        disableResize = false;
        free = nullptr;
        table = AllocatorNewArrayZ(TAllocator, allocator, EntryType*, size);
#if PROFILE_DICTIONARY
        stats = DictionaryStats::Create(typeid(this).name(), size);
#endif
    }

    ~SimpleHashTable()
    {
        for (uint i = 0; i < size; i++)
        {
            EntryType * entry = table[i];
            while (entry != nullptr)
            {
                EntryType * next = entry->next;
                AllocatorDelete(TAllocator, allocator, entry);
                entry = next;
            }
        }

        while(free)
        {
            EntryType* current = free;
            free = current->next;
            AllocatorDelete(TAllocator, allocator, current);
        }
        AllocatorDeleteArray(TAllocator, allocator,  size, table);
    }

    void DisableResize()
    {
        Assert(!resize || !disableResize);
        disableResize = true;
    }

    void EnableResize()
    {
        Assert(!resize || disableResize);
        disableResize = false;
    }

    void Set(TKey key, TData data)
    {
        EntryType* entry = FindOrAddEntry(key);
        entry->value = data;
    }

    bool Add(TKey key, TData data)
    {
        uint targetBucket = HashKeyToBucket(key);

        if(FindEntry(key, targetBucket) != nullptr)
        {
            return false;
        }

        AddInternal(key, data, targetBucket);
        return true;
    }

    void ReplaceValue(TKey key,TData data)
    {
        EntryType *current = FindEntry(key);
        if (current != nullptr)
        {
            current->value = data;
        }
    }

    void Remove(TKey key)
    {
        Remove(key, nullptr);
    }

    void Remove(TKey key, TData* pOut)
    {
        uint val = HashKeyToBucket(key);
        EntryType **prev=&table[val];
        for (EntryType * current = *prev ; current != nullptr; current = current->next)
        {
            if (Comparer<TKey>::Equals(key, current->key))
            {
                *prev = current->next;
                if (pOut != nullptr)
                {
                    (*pOut) = current->value;
                }

                count--;
                FreeEntry(current);
#if PROFILE_DICTIONARY
                if (stats)
                    stats->Remove(table[val] == nullptr);
#endif
                break;
            }
            prev = &current->next;
        }
    }

    BOOL HasEntry(TKey key)
    {
        return (FindEntry(key) != nullptr);
    }

    uint Count() const
    {
        return(count);
    }

    // If density is a compile-time constant, then we can optimize (avoids division)
    // Sometimes the compiler can also make this optimization, but this way it is guaranteed.
    template< uint density > bool IsDenserThan() const
    {
        return count > (size * density);
    }

    TData Lookup(TKey key)
    {
        EntryType *current = FindEntry(key);
        if (current != nullptr)
        {
            return current->value;
        }
        return TData();
    }

    TData LookupIndex(int index)
    {
        EntryType *current;
        int j=0;
        for (uint i=0; i < size; i++)
        {
            for (current = table[i] ; current != nullptr; current = current->next)
            {
                if (j==index)
                {
                    return current->value;
                }
                j++;
            }
        }
        return nullptr;
    }

    bool TryGetValue(TKey key, TData *dataReference)
    {
        EntryType *current = FindEntry(key);
        if (current != nullptr)
        {
            *dataReference = current->value;
            return true;
        }
        return false;
    }

    TData& GetReference(TKey key)
    {
        EntryType * current = FindOrAddEntry(key);
        return current->value;
    }

    TData * TryGetReference(TKey key)
    {
        EntryType * current = FindEntry(key);
        if (current != nullptr)
        {
            return &current->value;
        }
        return nullptr;
    }


    template <class Fn>
    void Map(Fn fn)
    {
        EntryType *current;
        for (uint i=0;i<size;i++) {
            for (current = table[i] ; current != nullptr; current = current->next) {
                fn(current->key,current->value);
            }
        }
    }

    template <class Fn>
    void MapAndRemoveIf(Fn fn)
    {
        for (uint i=0; i<size; i++)
        {
            EntryType ** prev = &table[i];
            while (EntryType * current = *prev)
            {
                if (fn(current->key,current->value))
                {
                    *prev = current->next;
                    FreeEntry(current);
                }
                else
                {
                    prev = &current->next;
                }
            }
        }
    }
private:
    uint HashKeyToBucket(TKey hashKey)
    {
        return HashKeyToBucket(hashKey, size);
    }

    uint HashKeyToBucket(TKey hashKey, int size)
    {
        uint hashCode = Comparer<TKey>::GetHashCode(hashKey);
        return SizePolicy::GetBucket(hashCode, size);
    }

    EntryType * FindEntry(TKey key)
    {
        uint targetBucket = HashKeyToBucket(key);
        return FindEntry(key, targetBucket);
    }

    EntryType * FindEntry(TKey key, uint targetBucket)
    {
        for (EntryType * current = table[targetBucket] ; current != nullptr; current = current->next)
        {
            if (Comparer<TKey>::Equals(key, current->key))
            {
                return current;
            }
        }
        return nullptr;
    }

    EntryType * FindOrAddEntry(TKey key)
    {
         uint targetBucket = HashKeyToBucket(key);
         EntryType * entry = FindEntry(key, targetBucket);
         if (entry == nullptr)
         {
            entry = AddInternal(key, TData(), targetBucket);
         }
         return entry;
    }

    void FreeEntry(EntryType* current)
    {
        if ( freecount < 10 )
        {
            current->key = nullptr;
            current->value = NULL;
            current->next = free;
            free = current;
            freecount++;
        }
        else
        {
            AllocatorDelete(TAllocator, allocator, current);
        }
    }

    EntryType* GetFreeEntry()
    {
        EntryType* retFree = free;
        if (nullptr == retFree )
        {
            retFree = AllocatorNewStruct(TAllocator, allocator, EntryType);
        }
        else
        {
            free = retFree->next;
            freecount--;
        }
        return retFree;
    }

    EntryType* AddInternal(TKey key, TData data, uint targetBucket)
    {
        if(resize && !disableResize && IsDenserThan<MaxAverageChainLength>())
        {
            Resize(SizePolicy::GetSize(size*2));
            // After resize - we will need to recalculate the bucket
            targetBucket = HashKeyToBucket(key);
        }

        EntryType* entry = GetFreeEntry();
        entry->key = key;
        entry->value = data;
        entry->next = table[targetBucket];
        table[targetBucket] = entry;
        count++;

#if PROFILE_DICTIONARY
        uint depth = 0;
        for (EntryType * current = table[targetBucket] ; current != nullptr; current = current->next)
        {
            ++depth;
        }
        if (stats)
            stats->Insert(depth);
#endif
        return entry;
    }

    void Resize(int newSize)
    {
        Assert(!this->disableResize);
        EntryType** newTable = AllocatorNewArrayZ(TAllocator, allocator, EntryType*, newSize);

        for (uint i=0; i < size; i++)
        {
            EntryType* current = table[i];
            while (current != nullptr)
            {
                int targetBucket = HashKeyToBucket(current->key, newSize);
                EntryType* next = current->next; // Cache the next pointer
                current->next = newTable[targetBucket];
                newTable[targetBucket] = current;
                current = next;
            }
        }

        AllocatorDeleteArray(TAllocator, allocator, this->size, this->table);
        this->size = newSize;
        this->table = newTable;
#if PROFILE_DICTIONARY
        if (stats)
        {
            uint emptyBuckets  = 0 ;
            for (uint i=0; i < size; i++)
            {
                if(table[i] == nullptr)
                {
                    emptyBuckets++;
                }
            }
            stats->Resize(newSize, emptyBuckets);
        }
#endif

    }
};
