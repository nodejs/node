//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace JsUtil
{
    template <typename T, uint size>
    class CircularBuffer
    {
    public:
        CircularBuffer():
          writeIndex(0),
          filled(false)
        {
        }

        void Clear()
        {
            this->writeIndex = 0;
            this->filled = false;
        }

        void Add(const T& value)
        {
            if (!Contains(value))
            {
                entries[writeIndex] = value;
                uint nextIndex = (writeIndex + 1) % size;
                if (nextIndex < writeIndex && !filled)
                {
                    filled = true;
                }

                writeIndex = nextIndex;
            }
        }

        bool Contains(const T& value)
        {
            for (uint i = 0; i < GetMaxIndex(); i++)
            {
                if (DefaultComparer<T>::Equals(entries[i], value))
                {
                    return true;
                }
            }

            return false;
        }

        uint GetMaxIndex()
        {
            return (filled ? size : writeIndex);
        }

        const T& Item(uint index)
        {
            Assert(index < GetMaxIndex());

            return entries[index];
        }

#ifdef VERBOSE_EVAL_MAP
        void Dump()
        {
            Output::Print(L"Length: %d, writeIndex: %d, filled: %d\n", size, writeIndex, filled);
            for (uint i = 0; i < GetMaxIndex(); i++)
            {
                Output::Print(L"Item %d: %s\n", i, entries[i].str.GetBuffer());
            }
            Output::Flush();
        }
#endif

        bool IsEmpty()
        {
            return (writeIndex == 0 && !filled);
        }

        int GetCount()
        {
            if (!filled) return writeIndex;

            return size;
        }

    private:
        uint writeIndex;
        bool filled;
        T entries[size];
    };

    template <class TKey, int MRUSize, class TAllocator = Recycler>
    class MRURetentionPolicy
    {
    public:
        typedef CircularBuffer<TKey, MRUSize> TMRUStoreType;
        MRURetentionPolicy(TAllocator* allocator)
        {
            store = AllocatorNew(TAllocator, allocator, TMRUStoreType);
        }

        void NotifyAdd(const TKey& key)
        {
            this->store->Add(key);
        }

        bool CanEvict(const TKey& key)
        {
            return !store->Contains(key);
        }

        void DumpKeepAlives()
        {
            store->Dump();
        }

    private:
        TMRUStoreType* store;
    };

    template <
        class TKey,
        class TValue,
        class TAllocator,
        class SizePolicy,
        class CacheRetentionPolicy,
        template <typename ValueOrKey> class Comparer = DefaultComparer,
        template <typename K, typename V> class Entry = SimpleDictionaryEntry
    >
    class Cache
    {
    private:
        typedef BaseDictionary<TKey, TValue, TAllocator, SizePolicy, Comparer, Entry> TCacheStoreType;
        typedef typename TCacheStoreType::AllocatorType AllocatorType;
        class CacheStore : public TCacheStoreType
        {
        public:
            CacheStore(AllocatorType* allocator, int capacity) : BaseDictionary(allocator, capacity), inAdd(false) {};
            bool IsInAdd()
            {
                return this->inAdd;
            }
            int Add(const TKey& key, const TValue& value)
            {
                AutoRestoreValue<bool> var(&this->inAdd, true);

                return __super::Add(key, value);
            }
            void SetIsInAdd(bool value) {inAdd = value; }
        private:
            bool inAdd;
        };
    public:
        typedef TKey KeyType;
        typedef TValue ValueType;
        typedef void (*OnItemEvictedCallback)(const TKey& key, TValue value);

        Cache(AllocatorType * allocator, int capacity = 0):
            cachePolicyType(allocator)
        {
            this->cacheStore = AllocatorNew(AllocatorType, allocator, CacheStore, allocator, capacity);
        }

        int Add(const TKey& key, const TValue& value)
        {
            int index = this->cacheStore->Add(key, value);
            this->cachePolicyType.NotifyAdd(key);

            return index;
        }

        void SetIsInAdd(bool value) {this->cacheStore->SetIsInAdd(value); }

        void NotifyAdd(const TKey& key)
        {
            this->cachePolicyType.NotifyAdd(key);
        }

        bool TryGetValue(const TKey& key, TValue* value)
        {
            return cacheStore->TryGetValue(key, value);
        }

        bool TryGetReference(const TKey& key, TValue** value, int* index)
        {
            return cacheStore->TryGetReference(key, value, index);
        }

        bool TryGetValueAndRemove(const TKey& key, TValue* value)
        {
            return cacheStore->TryGetValueAndRemove(key, value);
        }

        TKey const& GetKeyAt(const int& index)
        {
            return cacheStore->GetKeyAt(index);
        }

        template <class Fn>
        void Clean(Fn callback)
        {
            if (!this->cacheStore->IsInAdd())
            {
                // Queue up items to be removed
                // TODO: Don't use Contains since that's linear- store pointers to the eval map key instead, and set a bit indicating that its in the dictionary?
                cacheStore->MapAndRemoveIf([this, callback](const CacheStore::EntryType &entry) {
                    if (this->cachePolicyType.CanEvict(entry.Key()) || CONFIG_FLAG(ForceCleanCacheOnCollect))
                    {
                        callback(entry.Key(), entry.Value());

                        if (!CONFIG_FLAG(ForceCleanCacheOnCollect))
                        {
                            return true;
                        }
                    }
                    return false;
                });

                if (CONFIG_FLAG(ForceCleanCacheOnCollect))
                {
                    this->cacheStore->Clear();
                    Assert(this->cacheStore->Count() == 0);
                }
            }
        }

        template <class Fn>
        void CleanAll(Fn callback)
        {
            Assert(!this->cacheStore->IsInAdd());
            cacheStore->MapAndRemoveIf([this, callback](const CacheStore::EntryType &entry) -> bool {
                callback(entry.Key(), entry.Value());
                return true;
            });
        }

        void DumpKeepAlives()
        {
            cachePolicyType.DumpKeepAlives();
        }

    private:
        CacheStore* cacheStore;
        CacheRetentionPolicy cachePolicyType;
    };

}
