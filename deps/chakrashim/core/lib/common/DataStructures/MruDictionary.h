//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace JsUtil
{
    // - Maintains an MRU linked list of fixed maximum length and a dictionary that contains all added values
    // - RemoveRecentlyUnusedItems should be called to remove values that are not in the MRU list from the dictionary
    // - Added values are linked to the beginning of the MRU list (most recently used) as part of an MruListEntry, and when the
    //   list is full, entries from the end of the list (least recently used) are reused for the new values
    // - TryGetValue, if the key exists in the dictionary, adds the value to the MRU list as the most recently used value, and
    //   removes the least recently used value as necessary
    template<
        class TKey,
        class TValue,
        class TAllocator = Recycler,
        class TSizePolicy = PowerOf2SizePolicy,
        template<class ValueOrKey> class TComparer = DefaultComparer,
        template<class K, class V> class TDictionaryEntry = SimpleDictionaryEntry>
    class MruDictionary
    {
    private:
        struct MruListEntry : public DoublyLinkedListElement<MruListEntry>
        {
            TValue value;
            TKey key;
            int dictionaryDataIndex;

            MruListEntry(const TKey &key, const TValue &value) : key(key), value(value), dictionaryDataIndex(0)
            {
            }

            PREVENT_COPY(MruListEntry);
        };

    public:
        class MruDictionaryData
        {
        private:
            MruListEntry *entry;
            TValue value;

        public:
            MruDictionaryData() : entry(nullptr)
            {
            }

            MruDictionaryData &operator =(const void *const nullValue)
            {
                // Needed to support KeyValueEntry::Clear for dictionaries
                Assert(!nullValue);

                entry = nullptr;
                value = nullptr; // TValue must also support this for the same reason
                return *this;
            }

            MruListEntry *Entry() const
            {
                return entry;
            }

            const TValue &Value() const
            {
                Assert(!entry);

                return value;
            }

            void OnAddedToMruList(MruListEntry *const entry)
            {
                Assert(!this->entry);

                this->entry = entry;
            }

            void OnRemovedFromMruList()
            {
                Assert(entry);

                value = entry->value;
                entry = nullptr;
            }
        };

    private:
        const int mruListCapacity;
        int mruListCount;
        DoublyLinkedList<MruListEntry> entries;


        typedef
            BaseDictionary<
                TKey,
                MruDictionaryData,
                // MruDictionaryData always has pointer to GC pointer (MruEntry)
                typename ForceNonLeafAllocator<TAllocator>::AllocatorType,
                TSizePolicy,
                TComparer,
                TDictionaryEntry>
            TDictionary;
        TDictionary dictionary;
        typedef typename TDictionary::AllocatorType AllocatorType;
    public:
        MruDictionary(AllocatorType *const allocator, const int mruListCapacity)
            : mruListCapacity(mruListCapacity), mruListCount(0), dictionary(allocator)
        {
            Assert(allocator);
            Assert(mruListCapacity > 0);
        }

        static MruDictionary *New(TAllocator *const allocator, const int mruListCapacity)
        {
            return AllocatorNew(TAllocator, allocator, MruDictionary, allocator, mruListCapacity);
        }

    private:
        void AddToDictionary(MruListEntry *const entry)
        {
            const auto dictionaryDataIndex = dictionary.Add(entry->key, MruDictionaryData());
            dictionary.GetReferenceAt(dictionaryDataIndex)->OnAddedToMruList(entry);
            entry->dictionaryDataIndex = dictionaryDataIndex;
        }

        void ReuseLeastRecentlyUsedEntry(const TKey &key, const TValue &value, const int dictionaryDataIndex)
        {
            Assert(mruListCount == mruListCapacity);

            // Reuse the least recently used entry for this key/value pair and make it the most recently used
            const auto entry = entries.Tail();
            dictionary.GetReferenceAt(dictionaryDataIndex)->OnAddedToMruList(entry);
            dictionary.GetReferenceAt(entry->dictionaryDataIndex)->OnRemovedFromMruList();
            entries.MoveToBeginning(entry);
            entry->key = key;
            entry->value = value;
            entry->dictionaryDataIndex = dictionaryDataIndex;
        }

    public:
        bool TryGetValue(const TKey &key, TValue *const value)
        {
            MruDictionaryData *dictionaryData;
            int dictionaryDataIndex;
            if(!dictionary.TryGetReference(key, &dictionaryData, &dictionaryDataIndex))
                return false;

            const auto entry = dictionaryData->Entry();
            if(entry)
            {
                // Make this the most recently used entry
                entries.MoveToBeginning(entry);
                *value = entry->value;
                return true;
            }

            *value = dictionaryData->Value();

            // The key passed into this function may be temporary, and should not be placed in the MRU list or dictionary. Get
            // the proper key to be used from the dictionary. That key should have the necessary lifetime.
            ReuseLeastRecentlyUsedEntry(dictionary.GetKeyAt(dictionaryDataIndex), dictionaryData->Value(), dictionaryDataIndex);

            return true;
        }

        void Add(const TKey &key, const TValue &value)
        {
            Assert(!dictionary.ContainsKey(key));
            Assert(mruListCount <= mruListCapacity);

            if(mruListCount == mruListCapacity)
            {
                ReuseLeastRecentlyUsedEntry(key, value, dictionary.Add(key, MruDictionaryData()));
                return;
            }

            const auto entry = AllocatorNew(TAllocator, dictionary.GetAllocator(), MruListEntry, key, value);
            AddToDictionary(entry);
            entries.LinkToBeginning(entry);
            ++mruListCount;
        }

        void RemoveRecentlyUnusedItems()
        {
            if(dictionary.Count() == mruListCount)
                return;

            if(dictionary.Count() / 2 <= mruListCount)
            {
                dictionary.MapAndRemoveIf(
                    [](const TDictionary::EntryType &dictionaryEntry) -> bool
                    {
                        return !dictionaryEntry.Value().Entry();
                    });
                return;
            }

            dictionary.Clear();
            for(auto entry = entries.Head(); entry; entry = entry->Next())
                AddToDictionary(entry);
        }

        PREVENT_COPY(MruDictionary);
    };
}
