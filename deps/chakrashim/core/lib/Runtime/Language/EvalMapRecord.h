//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    // Use as the top level comparer for two level dictionary. Note that two
    // values are equal as long as their fastHash is the same (and moduleID/isStrict is the same).
    // This comparer is used for the top level dictionary in two level evalmap dictionary.
    template <class T>
    struct FastEvalMapStringComparer
    {
        static bool Equals(T left, T right)
        {
            return (left.hash == right.hash) &&
                (left.moduleID == right.moduleID) &&
                (left.IsStrict() == right.IsStrict());
        }

        static hash_t GetHashCode(T t)
        {
            return (hash_t)t;
        }
    };

    // The value in top level of two level dictionary. It might contain only the single value
    // (TValue), or a second level dictionary.
    template <class TKey, class TValue, class SecondaryDictionary, class NestedKey>
    class TwoLevelHashRecord
    {
    public:
        TwoLevelHashRecord(TValue newValue) :
            singleValue(true), value(newValue) {}

        TwoLevelHashRecord() :
            singleValue(true), value(nullptr) {}

        SecondaryDictionary* GetDictionary()
        {
            Assert(!singleValue);
            return nestedMap;
        }

        bool TryGetValue(TKey& key, TValue* value)
        {
            if (IsValue())
            {
                *value = GetValue();
                return true;
            }
            return GetDictionary()->TryGetValue(key, value);
        }

        void Add(const TKey& key, TValue& newValue)
        {
            Assert(!singleValue);
            NestedKey nestedKey;
            ConvertKey(key, nestedKey);
            nestedMap->Item(nestedKey, newValue);
#ifdef PROFILE_EVALMAP
            if (Configuration::Global.flags.ProfileEvalMap)
            {
                Output::Print(L"EvalMap fastcache collision:\t key = %d count = %d\n", (hash_t)key, nestedMap->Count());
            }
#endif
        }

        void Remove(const TKey& key)
        {
            Assert(!singleValue);
            NestedKey nestedKey;
            ConvertKey(key, nestedKey);
            nestedMap->Remove(nestedKey);
        }

        void ConvertToDictionary(TKey& key, Recycler* recycler)
        {
            Assert(singleValue);
            SecondaryDictionary* dictionary = RecyclerNew(recycler, SecondaryDictionary, recycler);
            auto newValue = value;
            nestedMap = dictionary;
            singleValue = false;
            Add(key, newValue);
        }

        bool IsValue() const { return singleValue; }
        TValue GetValue() const { Assert(singleValue); return value; }
        bool IsDictionaryEntry() const { return !singleValue; }

    private:
        bool singleValue;
        union
        {
            TValue value;
            SecondaryDictionary* nestedMap;
        };
    };

    // The two level dictionary. top level needs to be either simple hash value, or
    // key needs to be equals for all nested values.
    template <class Key, class Value, class EntryRecord, class TopLevelDictionary, class NestedKey>
    class TwoLevelHashDictionary
    {
        template <class T, class Value>
        class AutoRestoreSetInAdd
        {
        public:
            AutoRestoreSetInAdd(T* instance, Value value) :
                instance(instance), value(value)
            {
                instance->SetIsInAdd(value);
            }
            ~AutoRestoreSetInAdd()
            {
                instance->SetIsInAdd(!value);
            }

        private:
            T* instance;
            Value value;
        };

    public:
        TwoLevelHashDictionary(TopLevelDictionary* cache, Recycler* recycler) :
            dictionary(cache),
            recycler(recycler)
        {
        }

        bool TryGetValue(const Key& key, Value* value)
        {
            EntryRecord** entryRecord;
            Key cachedKey;
            int index;
            bool success = dictionary->TryGetReference(key, &entryRecord, &index);
            if (success && ((*entryRecord) != nullptr))
            {
                cachedKey = dictionary->GetKeyAt(index);
                if ((*entryRecord)->IsValue())
                {
                    success = (cachedKey == key);
                    if (success)
                    {
                        *value = (*entryRecord)->GetValue();
                    }
                }
                else
                {
                    NestedKey nestedKey;
                    ConvertKey(key, nestedKey);
                    success = (*entryRecord)->GetDictionary()->TryGetValue(nestedKey, value);
                }
            }
            else
            {
                success = false;
            }
            return success;
        }

        TopLevelDictionary* GetDictionary() const { return dictionary; }
        void NotifyAdd(const Key& key)
        {
            dictionary->NotifyAdd(key);
        }

        void Add(const Key& key, Value value)
        {
            EntryRecord** entryRecord;
            int index;
            bool success = dictionary->TryGetReference(key, &entryRecord, &index);
            if (success && ((*entryRecord) != nullptr))
            {
                AutoRestoreSetInAdd<TopLevelDictionary, bool> autoRestoreSetInAdd(this->dictionary, true);
                if ((*entryRecord)->IsValue())
                {
                    Key oldKey = dictionary->GetKeyAt(index);
                    (*entryRecord)->ConvertToDictionary(oldKey, recycler);
                }
                (*entryRecord)->Add(key, value);
            }
            else
            {
                EntryRecord* newRecord = RecyclerNew(recycler, EntryRecord, value);
                dictionary->Add(key, newRecord);
#ifdef PROFILE_EVALMAP
                if (Configuration::Global.flags.ProfileEvalMap)
                {
                    Output::Print(L"EvalMap fastcache set:\t key = %d \n", (hash_t)key);
                }
#endif
            }
        }

    private:
        TopLevelDictionary* dictionary;
        Recycler* recycler;
    };
}
