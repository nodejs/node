//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

//////////////////////////////////////////////////////////////////////////
// Template implementation of dictionary based on .NET BCL implementation.
//
// Buckets and entries are allocated as contiguous arrays to maintain good locality of reference.
//
// COLLISION STRATEGY
// This dictionary uses a chaining collision resolution strategy. Chains are implemented using indexes to the 'buckets' array.
//
// STORAGE (TAllocator)
// This dictionary works for both arena and recycler based allocation using TAllocator template parameter.
// It supports storing of both value and pointer types. Using template specialization, value types (built-in fundamental
// types and structs) are stored as leaf nodes by default.
//
// INITIAL SIZE and BUCKET MAPPING (SizePolicy)
// This can be specified using TSizePolicy template parameter. There are 2 implementations:
//         - PrimeSizePolicy (Better distribution): Initial size is a prime number. Mapping to bucket is done using modulus operation (costlier).
//         - PowerOf2SizePolicy (faster): Initial size is a power of 2. Mapping to bucket is done by a fast truncating the MSB bits up to the size of the table.
//
// COMPARISONS AND HASHCODE (Comparer)
// Enables custom comparisons for TKey and TValue. For example, for strings we use string comparison instead of comparing pointers.
//

#if PROFILE_DICTIONARY
#include "DictionaryStats.h"
#endif

namespace Js
{
    template <class TDictionary>
    class RemoteDictionary;
}

namespace JsDiag
{
    template <class TDictionary>
    struct RemoteDictionary;
}

namespace JsUtil
{
    class NoResizeLock
    {
    public:
        void BeginResize() {}
        void EndResize() {}
    };

    class AsymetricResizeLock
    {
    public:
        void BeginResize() { cs.Enter(); }
        void EndResize() { cs.Leave(); }
        void LockResize() { cs.Enter(); }
        void UnlockResize() { cs.Leave(); }
    private:
        CriticalSection cs;
    };

    template <class TKey, class TValue> class SimpleDictionaryEntry;

    template <
        class TKey,
        class TValue,
        class TAllocator,
        class SizePolicy = PowerOf2SizePolicy,
        template <typename ValueOrKey> class Comparer = DefaultComparer,
        template <typename K, typename V> class Entry = SimpleDictionaryEntry,
        typename Lock = NoResizeLock
    >
    class BaseDictionary : protected Lock
    {
    public:
        typedef TKey KeyType;
        typedef TValue ValueType;
        typedef typename AllocatorInfo<TAllocator, TValue>::AllocatorType AllocatorType;
        typedef SizePolicy CurrentSizePolicy;
        typedef Entry<TKey, TValue> EntryType;

        template<class TDictionary> class EntryIterator;
        template<class TDictionary> class BucketEntryIterator;

    protected:
        typedef typename AllocatorInfo<TAllocator, TValue>::AllocatorFunc EntryAllocatorFuncType;
        friend class Js::RemoteDictionary<BaseDictionary>;
        template <typename ValueOrKey> struct ComparerType { typedef Comparer<ValueOrKey> Type; }; // Used by diagnostics to access Comparer type

        int* buckets;
        EntryType* entries;
        AllocatorType* alloc;
        int size;
        uint bucketCount;
        int count;
        int freeList;
        int freeCount;

#if PROFILE_DICTIONARY
        DictionaryStats *stats;
#endif
        enum InsertOperations
        {
            Insert_Add   ,          // FatalInternalError if the item already exist in debug build
            Insert_AddNew,          // Ignore add if the item already exist
            Insert_Item             // Replace the item if it already exist
        };

        class AutoDoResize
        {
        public:
            AutoDoResize(Lock& lock) : lock(lock) { lock.BeginResize(); };
            ~AutoDoResize() { lock.EndResize(); };
        private:
            Lock& lock;
        };
    public:
        BaseDictionary(AllocatorType* allocator, int capacity = 0)
            : buckets (nullptr),
            size(0),
            bucketCount(0),
            entries(nullptr),
            count(0),
            freeCount(0),
            alloc(allocator)
        {
            Assert(allocator);
#if PROFILE_DICTIONARY
            stats = nullptr;
#endif
            // If initial capacity is negative or 0, lazy initialization on
            // the first insert operation is performed.
            if (capacity > 0)
            {
                Initialize(capacity);
            }
        }

        BaseDictionary(const BaseDictionary &other) : alloc(other.alloc)
        {
            if(other.Count() == 0)
            {
                size = 0;
                bucketCount = 0;
                buckets = nullptr;
                entries = nullptr;
                count = 0;
                freeCount = 0;

#if PROFILE_DICTIONARY
                stats = nullptr;
#endif
                return;
            }

            Assert(other.bucketCount != 0);
            Assert(other.size != 0);

            buckets = AllocateBuckets(other.bucketCount);
            Assert(buckets); // no-throw allocators are currently not supported

            try
            {
                entries = AllocateEntries(other.size, false /* zeroAllocate */);
                Assert(entries); // no-throw allocators are currently not supported
            }
            catch(...)
            {
                DeleteBuckets(buckets, other.bucketCount);
                throw;
            }

            size = other.size;
            bucketCount = other.bucketCount;
            count = other.count;
            freeList = other.freeList;
            freeCount = other.freeCount;

            size_t copySize = bucketCount * sizeof(buckets[0]);
            js_memcpy_s(buckets, copySize, other.buckets, copySize);

            copySize = size * sizeof(entries[0]);
            js_memcpy_s(entries, copySize, other.entries, copySize);

#if PROFILE_DICTIONARY
            stats = DictionaryStats::Create(typeid(this).name(), size);
#endif
        }

        ~BaseDictionary()
        {
            if (buckets)
            {
                DeleteBuckets(buckets, bucketCount);
            }

            if (entries)
            {
                DeleteEntries(entries, size);
            }
        }

        AllocatorType *GetAllocator() const
        {
            return alloc;
        }

        inline int Capacity() const
        {
            return size;
        }

        inline int Count() const
        {
            return count - freeCount;
        }

        TValue Item(const TKey& key)
        {
            int i = FindEntry(key);
            Assert(i >= 0);
            return entries[i].Value();
        }

        int Add(const TKey& key, const TValue& value)
        {
            return Insert<Insert_Add>(key, value);
        }

        int AddNew(const TKey& key, const TValue& value)
        {
            return Insert<Insert_AddNew>(key, value);
        }

        int Item(const TKey& key, const TValue& value)
        {
            return Insert<Insert_Item>(key, value);
        }

        bool Contains(KeyValuePair<TKey, TValue> keyValuePair)
        {
            int i = FindEntry(keyValuePair.Key());
            if( i >= 0 && Comparer<TValue>::Equals(entries[i].Value(), keyValuePair.Value()))
            {
                return true;
            }
            return false;
        }

        bool Remove(KeyValuePair<TKey, TValue> keyValuePair)
        {
            int i, last;
            uint targetBucket;
            if(FindEntryWithKey(keyValuePair.Key(), &i, &last, &targetBucket))
            {
                const TValue &value = entries[i].Value();
                if(Comparer<TValue>::Equals(value, keyValuePair.Value()))
                {
                    RemoveAt(i, last, targetBucket);
                    return true;
                }
            }
            return false;
        }

        void Clear()
        {
            if (count > 0)
            {
                memset(buckets, -1, bucketCount * sizeof(buckets[0]));
                memset(entries, 0, sizeof(EntryType) * size);
                count = 0;
                freeCount = 0;
#if PROFILE_DICTIONARY
                // To not loose previously collected data, we will treat cleared dictionary as a separate instance for stats tracking purpose
                stats = DictionaryStats::Create(typeid(this).name(), size);
#endif
            }
        }

        void ResetNoDelete()
        {
            this->size = 0;
            this->bucketCount = 0;
            this->buckets = nullptr;
            this->entries = nullptr;
            this->count = 0;
            this->freeCount = 0;
        }

        void Reset()
        {
            if(bucketCount != 0)
            {
                DeleteBuckets(buckets, bucketCount);
                buckets = nullptr;
                bucketCount = 0;
            }
            else
            {
                Assert(!buckets);
            }
            if(size != 0)
            {
                DeleteEntries(entries, size);
                entries = nullptr;
                freeCount = count = size = 0;
            }
            else
            {
                Assert(!entries);
                Assert(count == 0);
                Assert(freeCount == 0);
            }
        }

        bool ContainsKey(const TKey& key) const
        {
            return FindEntry(key) >= 0;
        }

        template <typename TLookup>
        inline const TValue& LookupWithKey(const TLookup& key, const TValue& defaultValue) const
        {
            int i = FindEntryWithKey(key);
            if (i >= 0)
            {
                return entries[i].Value();
            }
            return defaultValue;
        }

        inline const TValue& Lookup(const TKey& key, const TValue& defaultValue)
        {
            return LookupWithKey<TKey>(key, defaultValue);
        }

        template <typename TLookup>
        bool TryGetValue(const TLookup& key, TValue* value) const
        {
            int i = FindEntryWithKey(key);
            if (i >= 0)
            {
                *value = entries[i].Value();
                return true;
            }
            return false;
        }

        bool TryGetValueAndRemove(const TKey& key, TValue* value)
        {
            int i, last;
            uint targetBucket;
            if (FindEntryWithKey(key, &i, &last, &targetBucket))
            {
                *value = entries[i].Value();
                RemoveAt(i, last, targetBucket);
                return true;
            }
            return false;
        }

        template <typename TLookup>
        bool TryGetReference(const TLookup& key, const TValue** value) const
        {
            int i;
            return TryGetReference(key, value, &i);
        }

        template <typename TLookup>
        bool TryGetReference(const TLookup& key, TValue** value) const
        {
            return TryGetReference(key, const_cast<const TValue **>(value));
        }

        template <typename TLookup>
        bool TryGetReference(const TLookup& key, const TValue** value, int* index) const
        {
            int i = FindEntryWithKey(key);
            if (i >= 0)
            {
                *value = &entries[i].Value();
                *index = i;
                return true;
            }
            return false;
        }

        template <typename TLookup>
        bool TryGetReference(const TLookup& key, TValue** value, int* index) const
        {
            return TryGetReference(key, const_cast<const TValue **>(value), index);
        }

        const TValue& GetValueAt(const int index) const
        {
            Assert(index >= 0);
            Assert(index < count);

            return entries[index].Value();
        }

        TValue* GetReferenceAt(const int index) const
        {
            Assert(index >= 0);
            Assert(index < count);

            return &entries[index].Value();
        }

        TKey const& GetKeyAt(const int index) const
        {
            Assert(index >= 0);
            Assert(index < count);

            return entries[index].Key();
        }

        bool TryGetValueAt(const int index, TValue const ** value) const
        {
            if (index >= 0 && index < count)
            {
                *value = &entries[index].Value();
                return true;
            }
            return false;
        }

        bool TryGetValueAt(int index, TValue * value) const
        {
            if (index >= 0 && index < count)
            {
                *value = entries[index].Value();
                return true;
            }
            return false;
        }

        bool Remove(const TKey& key)
        {
            int i, last;
            uint targetBucket;
            if(FindEntryWithKey(key, &i, &last, &targetBucket))
            {
                RemoveAt(i, last, targetBucket);
                return true;
            }
            return false;
        }

        EntryIterator<const BaseDictionary> GetIterator() const
        {
            return EntryIterator<const BaseDictionary>(*this);
        }

        EntryIterator<BaseDictionary> GetIterator()
        {
            return EntryIterator<BaseDictionary>(*this);
        }

        BucketEntryIterator<BaseDictionary> GetIteratorWithRemovalSupport()
        {
            return BucketEntryIterator<BaseDictionary>(*this);
        }

        template<class Fn>
        bool AnyValue(Fn fn) const
        {
            for (uint i = 0; i < bucketCount; i++)
            {
                if(buckets[i] != -1)
                {
                    for (int currentIndex = buckets[i] ; currentIndex != -1 ; currentIndex = entries[currentIndex].next)
                    {
                        if (fn(entries[currentIndex].Value()))
                        {
                            return true;
                        }
                    }
                }
            }

            return false;
        }

        template<class Fn>
        void EachValue(Fn fn) const
        {
            for (uint i = 0; i < bucketCount; i++)
            {
                if(buckets[i] != -1)
                {
                    for (int currentIndex = buckets[i] ; currentIndex != -1 ; currentIndex = entries[currentIndex].next)
                    {
                        fn(entries[currentIndex].Value());
                    }
                }
            }
        }

        template<class Fn>
        void MapReference(Fn fn)
        {
            MapUntilReference([fn](TKey const& key, TValue& value)
            {
                fn(key, value);
                return false;
            });
        }

        template<class Fn>
        bool MapUntilReference(Fn fn) const
        {
            return MapEntryUntil([fn](EntryType &entry) -> bool
            {
                return fn(entry.Key(), entry.Value());
            });
        }

        template<class Fn>
        void MapAddress(Fn fn) const
        {
            MapUntilAddress([fn](TKey const& key, TValue * value) -> bool
            {
                fn(key, value);
                return false;
            });
        }

        template<class Fn>
        bool MapUntilAddress(Fn fn) const
        {
            return MapEntryUntil([fn](EntryType &entry) -> bool
            {
                return fn(entry.Key(), &entry.Value());
            });
        }

        template<class Fn>
        void Map(Fn fn) const
        {
            MapUntil([fn](TKey const& key, TValue const& value) -> bool
            {
                fn(key, value);
                return false;
            });
        }

        template<class Fn>
        bool MapUntil(Fn fn) const
        {
            return MapEntryUntil([fn](EntryType const& entry) -> bool
            {
                return fn(entry.Key(), entry.Value());
            });
        }

        template<class Fn>
        void MapAndRemoveIf(Fn fn)
        {
            for (uint i = 0; i < bucketCount; i++)
            {
                if (buckets[i] != -1)
                {
                    for (int currentIndex = buckets[i], lastIndex = -1; currentIndex != -1;)
                    {
                        // If the predicate says we should remove this item
                        if (fn(entries[currentIndex]) == true)
                        {
                            const int nextIndex = entries[currentIndex].next;
                            RemoveAt(currentIndex, lastIndex, i);
                            currentIndex = nextIndex;
                        }
                        else
                        {
                            lastIndex = currentIndex;
                            currentIndex = entries[currentIndex].next;
                        }
                    }
                }
            }
        }

        template <class Fn>
        bool RemoveIf(TKey const& key, Fn fn)
        {
            return RemoveIfWithKey<TKey>(key, fn);
        }

        template <typename LookupType, class Fn>
        bool RemoveIfWithKey(LookupType const& lookupKey, Fn fn)
        {
            int i, last;
            uint targetBucket;
            if (FindEntryWithKey<LookupType>(lookupKey, &i, &last, &targetBucket))
            {
                if (fn(entries[i].Key(), entries[i].Value()))
                {
                    RemoveAt(i, last, targetBucket);
                    return true;
                }
            }
            return false;
        }

        // Returns whether the dictionary was resized or not
        bool EnsureCapacity()
        {
            if (freeCount == 0 && count == size)
            {
                Resize();
                return true;
            }

            return false;
        }

        int GetNextIndex()
        {
            if (freeCount != 0)
            {
                Assert(freeCount > 0);
                Assert(freeList >= 0);
                Assert(freeList < count);
                return freeList;
            }

            return count;
        }

        int GetLastIndex()
        {
            return count - 1;
        }

        BaseDictionary *Clone()
        {
            return AllocatorNew(AllocatorType, alloc, BaseDictionary, *this);
        }

        void Copy(const BaseDictionary *const other)
        {
            DoCopy(other);
        }

        void LockResize()
        {
            __super::LockResize();
        }

        void UnlockResize()
        {
            __super::UnlockResize();
        }
    protected:
        template<class T>
        void DoCopy(const T *const other)
        {
            Assert(size == 0);
            Assert(bucketCount == 0);
            Assert(!buckets);
            Assert(!entries);
            Assert(count == 0);
            Assert(freeCount == 0);
#if PROFILE_DICTIONARY
            Assert(!stats);
#endif

            if(other->Count() == 0)
            {
                return;
            }

            Assert(other->bucketCount != 0);
            Assert(other->size != 0);

            buckets = AllocateBuckets(other->bucketCount);
            Assert(buckets); // no-throw allocators are currently not supported

            try
            {
                entries = AllocateEntries(other->size, false /* zeroAllocate */);
                Assert(entries); // no-throw allocators are currently not supported
            }
            catch(...)
            {
                DeleteBuckets(buckets, other->bucketCount);
                buckets = nullptr;
                throw;
            }

            size = other->size;
            bucketCount = other->bucketCount;
            count = other->count;
            freeList = other->freeList;
            freeCount = other->freeCount;

            size_t copySize = bucketCount * sizeof(buckets[0]);
            js_memcpy_s(buckets, copySize, other->buckets, copySize);

            copySize = size * sizeof(entries[0]);
            js_memcpy_s(entries, copySize, other->entries, copySize);

#if PROFILE_DICTIONARY
            stats = DictionaryStats::Create(typeid(this).name(), size);
#endif
        }

    protected:
        template<class Fn>
        bool MapEntryUntil(Fn fn) const
        {
            for (uint i = 0; i < bucketCount; i++)
            {
                if(buckets[i] != -1)
                {
                    for (int currentIndex = buckets[i] ; currentIndex != -1 ; currentIndex = entries[currentIndex].next)
                    {
                        if (fn(entries[currentIndex]))
                        {
                            return true; // fn condition succeeds
                        }
                    }
                }
            }

            return false;
        }

    private:
        template <typename TLookup>
        static hash_t GetHashCodeWithKey(const TLookup& key)
        {
            // set last bit to 1 to avoid false positive to make hash appears to be an valid recycler address.
            // In the same line, 0 should be use to indicate a non-existing entry.
            return TAGHASH(Comparer<TLookup>::GetHashCode(key));
        }

        static hash_t GetHashCode(const TKey& key)
        {
            return GetHashCodeWithKey<TKey>(key);
        }

        static uint GetBucket(hash_t hashCode, int bucketCount)
        {
            return SizePolicy::GetBucket(UNTAGHASH(hashCode), bucketCount);
        }

        uint GetBucket(uint hashCode) const
        {
            return GetBucket(hashCode, this->bucketCount);
        }

        static bool IsFreeEntry(const EntryType &entry)
        {
            // A free entry's next index will be (-2 - nextIndex), such that it is always <= -2, for fast entry iteration
            // allowing for skipping over free entries. -1 is reserved for the end-of-chain marker for a used entry.
            return entry.next <= -2;
        }

        void SetNextFreeEntryIndex(EntryType &freeEntry, const int nextFreeEntryIndex)
        {
            Assert(!IsFreeEntry(freeEntry));
            Assert(nextFreeEntryIndex >= -1);
            Assert(nextFreeEntryIndex < count);

            // The last entry in the free list chain will have a next of -2 to indicate that it is a free entry. The end of the
            // free list chain is identified using freeCount.
            freeEntry.next = nextFreeEntryIndex >= 0 ? -2 - nextFreeEntryIndex : -2;
        }

        static int GetNextFreeEntryIndex(const EntryType &freeEntry)
        {
            Assert(IsFreeEntry(freeEntry));
            return -2 - freeEntry.next;
        }

        template <typename LookupType>
        __inline int FindEntryWithKey(const LookupType& key) const
        {
#if PROFILE_DICTIONARY
            uint depth = 0;
#endif
            int * localBuckets = buckets;
            if (localBuckets != nullptr)
            {
                hash_t hashCode = GetHashCodeWithKey<LookupType>(key);
                uint targetBucket = this->GetBucket(hashCode);
                EntryType * localEntries = entries;
                for (int i = localBuckets[targetBucket]; i >= 0; i = localEntries[i].next)
                {
                    if (localEntries[i].KeyEquals<Comparer<TKey>>(key, hashCode))
                    {
#if PROFILE_DICTIONARY
                        if (stats)
                            stats->Lookup(depth);
#endif
                        return i;
                    }

#if PROFILE_DICTIONARY
                    depth += 1;
#endif
                }
            }

#if PROFILE_DICTIONARY
            if (stats)
                stats->Lookup(depth);
#endif
            return -1;
        }

        inline int FindEntry(const TKey& key) const
        {
            return FindEntryWithKey<TKey>(key);
        }

        template <typename LookupType>
        __inline bool FindEntryWithKey(const LookupType& key, int *const i, int *const last, uint *const targetBucket)
        {
#if PROFILE_DICTIONARY
            uint depth = 0;
#endif
            int * localBuckets = buckets;
            if (localBuckets != nullptr)
            {
                uint hashCode = GetHashCodeWithKey<LookupType>(key);
                *targetBucket = this->GetBucket(hashCode);
                *last = -1;
                EntryType * localEntries = entries;
                for (*i = localBuckets[*targetBucket]; *i >= 0; *last = *i, *i = localEntries[*i].next)
                {
                    if (localEntries[*i].KeyEquals<Comparer<TKey>>(key, hashCode))
                    {
#if PROFILE_DICTIONARY
                        if (stats)
                            stats->Lookup(depth);
#endif
                        return true;
                    }
#if PROFILE_DICTIONARY
                    depth += 1;
#endif
                }
            }
#if PROFILE_DICTIONARY
            if (stats)
                stats->Lookup(depth);
#endif
            return false;
        }

        void Initialize(int capacity)
        {
            // minimum capacity is 4
            int initSize = max(capacity, 4);
            uint initBucketCount = SizePolicy::GetBucketSize(initSize);
            AssertMsg(initBucketCount > 0, "Size returned by policy should be greater than 0");
            Allocate(&buckets, &entries, initBucketCount, initSize);

            // Allocation can throw - assign the size only after allocation has succeeded.
            this->bucketCount = initBucketCount;
            this->size = initSize;
            Assert(this->freeCount == 0);
#if PROFILE_DICTIONARY
            stats = DictionaryStats::Create(typeid(this).name(), size);
#endif
        }

        template <InsertOperations op>
        int Insert(TKey key, TValue value)
        {
            int * localBuckets = buckets;
            if (localBuckets == nullptr)
            {
                Initialize(0);
                localBuckets = buckets;
            }

#if DBG || PROFILE_DICTIONARY
            // Always search and verify
            const bool needSearch = true;
#else
            const bool needSearch = (op != Insert_Add);
#endif
            hash_t hashCode = GetHashCode(key);
            uint targetBucket = this->GetBucket(hashCode);
            if (needSearch)
            {
#if PROFILE_DICTIONARY
                uint depth = 0;
#endif
                EntryType * localEntries = entries;
                for (int i = localBuckets[targetBucket]; i >= 0; i = localEntries[i].next)
                {
                    if (localEntries[i].KeyEquals<Comparer<TKey>>(key, hashCode))
                    {
#if PROFILE_DICTIONARY
                        if (stats)
                            stats->Lookup(depth);
#endif
                        Assert(op != Insert_Add);
                        if (op == Insert_Item)
                        {
                            localEntries[i].SetValue(value);
                            return i;
                        }
                        return -1;
                    }
#if PROFILE_DICTIONARY
                    depth += 1;
#endif
                }

#if PROFILE_DICTIONARY
                if (stats)
                    stats->Lookup(depth);
#endif
            }

            // Ideally we'd do cleanup only if weak references have been collected since the last resize
            // but that would require us to use an additional field to store the last recycler cleanup id
            // that we saw
            // We can add that optimization later if we have to.
            if (EntryType::SupportsCleanup() && freeCount == 0 && count == size)
            {
                this->MapAndRemoveIf([](EntryType& entry)
                {
                    return EntryType::NeedsCleanup(entry);
                });
            }

            int index;
            if (freeCount != 0)
            {
                Assert(freeCount > 0);
                Assert(freeList >= 0);
                Assert(freeList < count);
                index = freeList;
                freeCount--;
                if(freeCount != 0)
                {
                    freeList = GetNextFreeEntryIndex(entries[index]);
                }
            }
            else
            {
                // If there's nothing free, then in general, we set index to count, and increment count
                // If we resize, we also need to recalculate the target
                // However, if cleanup is supported, then before resize, we should try and clean up and see
                // if something got freed, and if it did, reuse that index
                if (count == size)
                {
                    Resize();
                    targetBucket = this->GetBucket(hashCode);
                    index = count;
                    count++;
                }
                else
                {
                    index = count;
                    count++;
                }

                Assert(count <= size);
                Assert(index < size);
            }

            entries[index].Set(key, value, hashCode);
            entries[index].next = buckets[targetBucket];
            buckets[targetBucket] = index;

#if PROFILE_DICTIONARY
            int profileIndex = index;
            uint depth = 1;  // need to recalculate depth in case there was a resize (also 1-based for stats->Insert)
            while(entries[profileIndex].next != -1)
            {
                profileIndex = entries[profileIndex].next;
                ++depth;
            }
            if (stats)
                stats->Insert(depth);
#endif
            return index;
        }

        void Resize()
        {
            AutoDoResize autoDoResize(*this);

            int newSize = SizePolicy::GetNextSize(count);
            uint newBucketCount = SizePolicy::GetBucketSize(newSize);

            __analysis_assume(newSize > count);
            int* newBuckets = nullptr;
            EntryType* newEntries = nullptr;
            if (newBucketCount == bucketCount)
            {
                // no need to rehash
                newEntries = AllocateEntries(newSize);
                js_memcpy_s(newEntries, sizeof(EntryType) * newSize, entries, sizeof(EntryType) * count);

                DeleteEntries(entries, size);

                this->entries = newEntries;
                this->size = newSize;
                return;
            }

            Allocate(&newBuckets, &newEntries, newBucketCount, newSize);
            js_memcpy_s(newEntries, sizeof(EntryType) * newSize, entries, sizeof(EntryType) * count);

            // When TAllocator is of type Recycler, it is possible that the Allocate above causes a collection, which
            // in turn can cause entries in the dictionary to be removed - i.e. the dictionary contains weak references
            // that remove themselves when no longer valid. This means the free list might not be empty anymore.
            for (int i = 0; i < count; i++)
            {
                __analysis_assume(i < newSize);

                if (!IsFreeEntry(newEntries[i]))
                {
                    uint hashCode = newEntries[i].GetHashCode<Comparer<TKey>>();
                    int bucket = GetBucket(hashCode, newBucketCount);
                    newEntries[i].next = newBuckets[bucket];
                    newBuckets[bucket] = i;
                }
            }

            DeleteBuckets(buckets, bucketCount);
            DeleteEntries(entries, size);

#if PROFILE_DICTIONARY
            if (stats)
                stats->Resize(newSize, /*emptyBuckets=*/ newSize - size);
#endif
            buckets = newBuckets;
            bucketCount = newBucketCount;
            size = newSize;
            entries = newEntries;
        }

        __ecount(bucketCount) int *AllocateBuckets(const uint bucketCount)
        {
            return
                AllocateArray<AllocatorType, int, false>(
                    TRACK_ALLOC_INFO(alloc, int, AllocatorType, 0, bucketCount),
                    TypeAllocatorFunc<AllocatorType, int>::GetAllocFunc(),
                    bucketCount);
        }

        __ecount(size) EntryType * AllocateEntries(int size, const bool zeroAllocate = true)
        {
            // Note that the choice of leaf/non-leaf node is decided for the EntryType on the basis of TValue. By default, if
            // TValue is a pointer, a non-leaf allocation is done. This behavior can be overridden by specializing
            // TypeAllocatorFunc for TValue.
            return
                AllocateArray<AllocatorType, EntryType, false>(
                    TRACK_ALLOC_INFO(alloc, EntryType, AllocatorType, 0, size),
                    zeroAllocate ? EntryAllocatorFuncType::GetAllocZeroFunc() : EntryAllocatorFuncType::GetAllocFunc(),
                    size);
        }

        void DeleteBuckets(__in_ecount(bucketCount) int *const buckets, const uint bucketCount)
        {
            Assert(buckets);
            Assert(bucketCount != 0);

            AllocatorFree(alloc, (TypeAllocatorFunc<AllocatorType, int>::GetFreeFunc()), buckets, bucketCount * sizeof(int));
        }

        void DeleteEntries(__in_ecount(size) EntryType *const entries, const int size)
        {
            Assert(entries);
            Assert(size != 0);

            AllocatorFree(alloc, EntryAllocatorFuncType::GetFreeFunc(), entries, size * sizeof(EntryType));
        }

        void Allocate(__deref_out_ecount(bucketCount) int** ppBuckets, __deref_out_ecount(size) EntryType** ppEntries, uint bucketCount, int size)
        {
            int *const buckets = AllocateBuckets(bucketCount);
            Assert(buckets); // no-throw allocators are currently not supported

            EntryType *entries;
            try
            {
                entries = AllocateEntries(size);
                Assert(entries); // no-throw allocators are currently not supported
            }
            catch(...)
            {
                DeleteBuckets(buckets, bucketCount);
                throw;
            }

            memset(buckets, -1, bucketCount * sizeof(buckets[0]));

            *ppBuckets = buckets;
            *ppEntries = entries;
        }

        __inline void RemoveAt(const int i, const int last, const uint targetBucket)
        {
            if (last < 0)
            {
                buckets[targetBucket] = entries[i].next;
            }
            else
            {
                entries[last].next = entries[i].next;
            }
            entries[i].Clear();
            SetNextFreeEntryIndex(entries[i], freeCount == 0 ? -1 : freeList);
            freeList = i;
            freeCount++;
#if PROFILE_DICTIONARY
            if (stats)
                stats->Remove(buckets[targetBucket] == -1);
#endif
        }

#if DBG_DUMP
    public:
        void Dump()
        {
            printf("Dumping Dictionary\n");
            printf("-------------------\n");
            for (uint i = 0; i < bucketCount; i++)
            {
                printf("Bucket value: %d\n", buckets[i]);
                for (int j = buckets[i]; j >= 0; j = entries[j].next)
                {
                    printf("%d  => %d  Next: %d\n", entries[j].Key(), entries[j].Value(), entries[j].next);
                }
            }
        }
#endif

    protected:
        template<class TDictionary, class Leaf>
        class IteratorBase abstract
        {
        protected:
            EntryType *const entries;
            int entryIndex;

        #if DBG
        protected:
            TDictionary &dictionary;
        private:
            int usedEntryCount;
        #endif

        protected:
            IteratorBase(TDictionary &dictionary, const int entryIndex)
                : entries(dictionary.entries),
                entryIndex(entryIndex)
            #if DBG
                ,
                dictionary(dictionary),
                usedEntryCount(dictionary.Count())
            #endif
            {
            }

        protected:
            void OnEntryRemoved()
            {
                DebugOnly(--usedEntryCount);
            }

        private:
            bool IsValid_Virtual() const
            {
                return static_cast<const Leaf *>(this)->IsValid();
            }

        protected:
            bool IsValid() const
            {
                Assert(dictionary.entries == entries);
                Assert(dictionary.Count() == usedEntryCount);

                return true;
            }

        public:
            EntryType &Current() const
            {
                Assert(IsValid_Virtual());
                Assert(!IsFreeEntry(entries[entryIndex]));

                return entries[entryIndex];
            }

            TKey CurrentKey() const
            {
                return Current().Key();
            }

            const TValue &CurrentValue() const
            {
                return Current().Value();
            }

            TValue &CurrentValueReference() const
            {
                return Current().Value();
            }

            void SetCurrentValue(const TValue &value) const
            {
            #if DBG
                // For BaseHashSet, save the key before changing the value to verify that the key does not change
                const TKey previousKey = CurrentKey();
                const hash_t previousHashCode = GetHashCode(previousKey);
            #endif

                Current().SetValue(value);

                Assert(Current().KeyEquals<Comparer<TKey>>(previousKey, previousHashCode));
            }
        };

    public:
        template<class TDictionary>
        class EntryIterator sealed : public IteratorBase<TDictionary, EntryIterator<TDictionary>>
        {
        private:
            typedef IteratorBase<TDictionary, EntryIterator<TDictionary>> Base;

        private:
            const int entryCount;

        public:
            EntryIterator(TDictionary &dictionary) : Base(dictionary, 0), entryCount(dictionary.count)
            {
                if(IsValid() && IsFreeEntry(entries[entryIndex]))
                {
                    MoveNext();
                }
            }

        public:
            bool IsValid() const
            {
                Assert(dictionary.count == entryCount);
                Assert(entryIndex >= 0);
                Assert(entryIndex <= entryCount);

                return Base::IsValid() && entryIndex < entryCount;
            }

        public:
            void MoveNext()
            {
                Assert(IsValid());

                do
                {
                    ++entryIndex;
                } while(IsValid() && IsFreeEntry(entries[entryIndex]));
            }
        };

    public:
        template<class TDictionary>
        class BucketEntryIterator sealed : public IteratorBase<TDictionary, BucketEntryIterator<TDictionary>>
        {
        private:
            typedef IteratorBase<TDictionary, BucketEntryIterator<TDictionary>> Base;

        private:
            TDictionary &dictionary;
            int *const buckets;
            const uint bucketCount;
            uint bucketIndex;
            int previousEntryIndexInBucket;
            int indexOfEntryAfterRemovedEntry;

        public:
            BucketEntryIterator(TDictionary &dictionary)
                : Base(dictionary, -1),
                dictionary(dictionary),
                buckets(dictionary.buckets),
                bucketCount(dictionary.bucketCount),
                bucketIndex(0u - 1)
            #if DBG
                ,
                previousEntryIndexInBucket(-2),
                indexOfEntryAfterRemovedEntry(-2)
            #endif
            {
                if(dictionary.Count() != 0)
                {
                    MoveNextBucket();
                }
            }

        public:
            bool IsValid() const
            {
                Assert(dictionary.buckets == buckets);
                Assert(dictionary.bucketCount == bucketCount);
                Assert(entryIndex >= -1);
                Assert(entryIndex < dictionary.count);
                Assert(bucketIndex == 0u - 1 || bucketIndex <= bucketCount);
                Assert(previousEntryIndexInBucket >= -2);
                Assert(previousEntryIndexInBucket < dictionary.count);
                Assert(indexOfEntryAfterRemovedEntry >= -2);
                Assert(indexOfEntryAfterRemovedEntry < dictionary.count);

                return Base::IsValid() && entryIndex >= 0;
            }

        public:
            void MoveNext()
            {
                if(IsValid())
                {
                    previousEntryIndexInBucket = entryIndex;
                    entryIndex = Current().next;
                }
                else
                {
                    Assert(indexOfEntryAfterRemovedEntry >= -1);
                    entryIndex = indexOfEntryAfterRemovedEntry;
                }

                if(!IsValid())
                {
                    MoveNextBucket();
                }
            }

        private:
            void MoveNextBucket()
            {
                Assert(!IsValid());

                while(++bucketIndex < bucketCount)
                {
                    entryIndex = buckets[bucketIndex];
                    if(IsValid())
                    {
                        previousEntryIndexInBucket = -1;
                        break;
                    }
                }
            }

        public:
            void RemoveCurrent()
            {
                Assert(previousEntryIndexInBucket >= -1);

                indexOfEntryAfterRemovedEntry = Current().next;
                dictionary.RemoveAt(entryIndex, previousEntryIndexInBucket, bucketIndex);
                OnEntryRemoved();
                entryIndex = -1;
            }
        };

        template<class TDictionary, class Leaf> friend class IteratorBase;
        template<class TDictionary> friend class EntryIterator;
        template<class TDictionary> friend class BucketEntryIterator;

        PREVENT_ASSIGN(BaseDictionary);
    };

    template <class TKey, class TValue> class SimpleHashedEntry;

    template <
        class TElement,
        class TAllocator,
        class SizePolicy = PowerOf2SizePolicy,
        class TKey = TElement,
        template <typename ValueOrKey> class Comparer = DefaultComparer,
        template <typename, typename> class Entry = SimpleHashedEntry,
        typename Lock = NoResizeLock
    >
    class BaseHashSet : protected BaseDictionary<TKey, TElement, TAllocator, SizePolicy, Comparer, Entry, Lock>
    {
        typedef BaseDictionary<TKey, TElement, TAllocator, SizePolicy, Comparer, Entry, Lock> Base;
        typedef Entry<TKey, TElement> EntryType;

        friend struct JsDiag::RemoteDictionary<BaseHashSet<TElement, TAllocator, SizePolicy, TKey, Comparer, Entry, Lock>>;

    public:
        BaseHashSet(AllocatorType * allocator, int capacity = 0) : BaseDictionary(allocator, capacity) {}

        using Base::GetAllocator;

        int Count() const
        {
            return __super::Count();
        }

        int Add(TElement const& element)
        {
            return __super::Add(ValueToKey<TKey, TElement>::ToKey(element), element);
        }

        // Add only if there isn't an existing element
        int AddNew(TElement const& element)
        {
            return __super::AddNew(ValueToKey<TKey, TElement>::ToKey(element), element);
        }

        int Item(TElement const& element)
        {
            return __super::Item(ValueToKey<TKey, TElement>::ToKey(element), element);
        }

        void Clear()
        {
            __super::Clear();
        }

        using Base::Reset;

        TElement const& Lookup(TKey const& key)
        {
            // Use a static to pass the null default value, since the
            // default value may get returned out of the current scope by ref.
            static const TElement nullElement = nullptr;
            return __super::Lookup(key, nullElement);
        }

        template <typename KeyType>
        TElement const& LookupWithKey(KeyType const& key)
        {
            static const TElement nullElement = nullptr;

            return __super::LookupWithKey(key, nullElement);
        }

        bool Contains(TElement const& element) const
        {
            return ContainsKey(ValueToKey<TKey, TElement>::ToKey(element));
        }

        using Base::ContainsKey;
        using Base::TryGetValue;
        using Base::TryGetReference;

        bool RemoveKey(const TKey &key)
        {
            return Base::Remove(key);
        }

        bool Remove(TElement const& element)
        {
            return __super::Remove(ValueToKey<TKey, TElement>::ToKey(element));
        }

        EntryIterator<const BaseHashSet> GetIterator() const
        {
            return EntryIterator<const BaseHashSet>(*this);
        }

        EntryIterator<BaseHashSet> GetIterator()
        {
            return EntryIterator<BaseHashSet>(*this);
        }

        BucketEntryIterator<BaseHashSet> GetIteratorWithRemovalSupport()
        {
            return BucketEntryIterator<BaseHashSet>(*this);
        }

        template<class Fn>
        void Map(Fn fn)
        {
            MapUntil([fn](TElement const& value) -> bool
            {
                fn(value);
                return false;
            });
        }

        template<class Fn>
        void MapAndRemoveIf(Fn fn)
        {
            __super::MapAndRemoveIf([fn](EntryType const& entry) -> bool
            {
                return fn(entry.Value());
            });
        }

        template<class Fn>
        bool MapUntil(Fn fn)
        {
            return __super::MapEntryUntil([fn](EntryType const& entry) -> bool
            {
                return fn(entry.Value());
            });
        }

        bool EnsureCapacity()
        {
            return __super::EnsureCapacity();
        }

        int GetNextIndex()
        {
            return __super::GetNextIndex();
        }

        int GetLastIndex()
        {
            return __super::GetLastIndex();
        }

        using Base::GetValueAt;

        bool TryGetValueAt(int index, TElement * value) const
        {
            return __super::TryGetValueAt(index, value);
        }

        BaseHashSet *Clone()
        {
            return AllocatorNew(AllocatorType, alloc, BaseHashSet, *this);
        }

        void Copy(const BaseHashSet *const other)
        {
            DoCopy(other);
        }

        void LockResize()
        {
            __super::LockResize();
        }

        void UnlockResize()
        {
            __super::UnlockResize();
        }
    public:
        using Base::EntryIterator;
        using Base::BucketEntryIterator;

        friend class Base;

        // The following syntax works in BaseDictionary (where the classes are defined), but not in derived
        // classes such as BaseHashSet
        //     template<class TDictionary, class Leaf> friend class IteratorBase;
        //     template<class TDictionary> friend class EntryIterator;
        //     template<class TDictionary> friend class BucketEntryIterator;
        friend class Base::IteratorBase<const BaseHashSet, EntryIterator<const BaseHashSet>>;
        friend class Base::IteratorBase<const BaseHashSet, BucketEntryIterator<const BaseHashSet>>;
        friend class Base::IteratorBase<BaseHashSet, EntryIterator<BaseHashSet>>;
        friend class Base::IteratorBase<BaseHashSet, BucketEntryIterator<BaseHashSet>>;
        friend class EntryIterator<const BaseHashSet>;
        friend class EntryIterator<BaseHashSet>;
        friend class BucketEntryIterator<const BaseHashSet>;
        friend class BucketEntryIterator<BaseHashSet>;

        PREVENT_ASSIGN(BaseHashSet);
    };

    template <
        class TKey,
        class TValue,
        class TAllocator,
        class SizePolicy = PowerOf2SizePolicy,
        template <typename ValueOrKey> class Comparer = DefaultComparer,
        template <typename K, typename V> class Entry = SimpleDictionaryEntry,
        class LockPolicy = Js::DefaultListLockPolicy,   // Controls lock policy for read/map/write/add/remove items
        class SyncObject = CriticalSection
    >
    class SynchronizedDictionary: protected BaseDictionary<TKey, TValue, TAllocator, SizePolicy, Comparer, Entry>
    {
    private:
        SyncObject* syncObj;

    public:
        typedef TKey KeyType;
        typedef TValue ValueType;
        typedef BaseDictionary<TKey, TValue, TAllocator, SizePolicy, Comparer, Entry>::EntryType EntryType;
        typedef SynchronizedDictionary<TKey, TValue, TAllocator, SizePolicy, Comparer, Entry, LockPolicy, SyncObject> DictionaryType;
    private:
        friend class Js::RemoteDictionary<DictionaryType>;

    public:
        SynchronizedDictionary(AllocatorType * allocator, int capacity, SyncObject* syncObject):
            BaseDictionary(allocator, capacity),
            syncObj(syncObject)
        {}

#ifdef DBG
        void Dump()
        {
            LockPolicy::ReadLock autoLock(syncObj);

            __super::Dump();
        }
#endif

        TAllocator *GetAllocator() const
        {
            LockPolicy::ReadLock autoLock(syncObj);

            return __super::GetAllocator();
        }

        inline int Count() const
        {
            LockPolicy::ReadLock autoLock(syncObj);

            return __super::Count();
        }

        inline int Capacity() const
        {
            LockPolicy::ReadLock autoLock(syncObj);

            return __super::Capacity();
        }

        TValue Item(const TKey& key)
        {
            LockPolicy::ReadLock autoLock(syncObj);

            return __super::Item(key);
        }

        bool IsInAdd()
        {
            LockPolicy::ReadLock autoLock(syncObj);

            return __super::IsInAdd();
        }

        int Add(const TKey& key, const TValue& value)
        {
            LockPolicy::AddRemoveLock autoLock(syncObj);

            return __super::Add(key, value);
        }

        int AddNew(const TKey& key, const TValue& value)
        {
            LockPolicy::AddRemoveLock autoLock(syncObj);

            return __super::AddNew(key, value);
        }

        int Item(const TKey& key, const TValue& value)
        {
            LockPolicy::AddRemoveLock autoLock(syncObj);

            return __super::Item(key, value);
        }

        bool Contains(KeyValuePair<TKey, TValue> keyValuePair)
        {
            LockPolicy::ReadLock autoLock(syncObj);

            return __super::Contains(keyValuePair);
        }

        bool Remove(KeyValuePair<TKey, TValue> keyValuePair)
        {
            LockPolicy::AddRemoveLock autoLock(syncObj);

            return __super::Remove(keyValuePair);
        }

        void Clear()
        {
            LockPolicy::AddRemoveLock autoLock(syncObj);

            return __super::Clear();
        }

        void Reset()
        {
            LockPolicy::AddRemoveLock autoLock(syncObj);

            return __super::Reset();
        }

        bool ContainsKey(const TKey& key)
        {
            LockPolicy::ReadLock autoLock(syncObj);

            return __super::ContainsKey(key);
        }

        template <typename TLookup>
        inline const TValue& LookupWithKey(const TLookup& key, const TValue& defaultValue)
        {
            LockPolicy::ReadLock autoLock(syncObj);

            return __super::LookupWithKey(key, defaultValue);
        }

        inline const TValue& Lookup(const TKey& key, const TValue& defaultValue)
        {
            LockPolicy::ReadLock autoLock(syncObj);

            return __super::Lookup(key, defaultValue);
        }

        template <typename TLookup>
        bool TryGetValue(const TLookup& key, TValue* value)
        {
            LockPolicy::ReadLock autoLock(syncObj);

            return __super::TryGetValue(key, value);
        }

        bool TryGetValueAndRemove(const TKey& key, TValue* value)
        {
            LockPolicy::AddRemoveLock autoLock(syncObj);

            return __super::TryGetValueAndRemove(key, value);
        }

        template <typename TLookup>
        __inline bool TryGetReference(const TLookup& key, TValue** value)
        {
            LockPolicy::ReadLock autoLock(syncObj);

            return __super::TryGetReference(key, value);
        }

        template <typename TLookup>
        __inline bool TryGetReference(const TLookup& key, TValue** value, int* index)
        {
            LockPolicy::ReadLock autoLock(syncObj);

            return __super::TryGetReference(key, value, index);
        }

        const TValue& GetValueAt(const int& index) const
        {
            LockPolicy::ReadLock autoLock(syncObj);

            return __super::GetValueAt(index);
        }

        TValue* GetReferenceAt(const int& index)
        {
            LockPolicy::ReadLock autoLock(syncObj);

            return __super::GetReferenceAt(index);
        }

        TKey const& GetKeyAt(const int& index)
        {
            LockPolicy::ReadLock autoLock(syncObj);

            return __super::GetKeyAt(index);
        }

        bool Remove(const TKey& key)
        {
            LockPolicy::ReadLock autoLock(syncObj);

            return __super::Remove(key);
        }

        template<class Fn>
        void MapReference(Fn fn)
        {
            // TODO: Verify that Map doesn't actually modify the list
            LockPolicy::ReadLock autoLock(syncObj);

            return __super::MapReference(fn);
        }

        template<class Fn>
        bool MapUntilReference(Fn fn) const
        {
            LockPolicy::ReadLock autoLock(syncObj);

            return __super::MapUntilReference(fn);
        }

        template<class Fn>
        void MapAddress(Fn fn) const
        {
            LockPolicy::ReadLock autoLock(syncObj);

            return __super::MapAddress(fn);
        }

        template<class Fn>
        bool MapUntilAddress(Fn fn) const
        {
            LockPolicy::ReadLock autoLock(syncObj);

            return __super::MapUntilAddress(fn);
        }

        template<class Fn>
        void Map(Fn fn) const
        {
            LockPolicy::ReadLock autoLock(syncObj);

            return __super::Map(fn);
        }

        template<class Fn>
        bool MapUntil(Fn fn) const
        {
            LockPolicy::ReadLock autoLock(syncObj);

            return __super::MapUntil(fn);
        }

        template<class Fn>
        void MapAndRemoveIf(Fn fn)
        {
            LockPolicy::AddRemoveLock autoLock(syncObj);

            return __super::MapAndRemoveIf(fn);
        }

        PREVENT_COPY(SynchronizedDictionary);
    };
}

