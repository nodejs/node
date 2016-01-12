//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace JsUtil
{
    template <class TValue>
    class BaseValueEntry
    {
    protected:
        TValue value;        // data of entry
        void Set(TValue const& value)
        {
            this->value = value;
        }

    public:
        int next;        // Index of next entry, -1 if last

        static bool SupportsCleanup()
        {
            return false;
        }

        static bool NeedsCleanup(BaseValueEntry<TValue>&)
        {
            return false;
        }

        TValue const& Value() const { return value; }
        TValue& Value() { return value; }
        void SetValue(TValue const& value) { this->value = value; }
    };

    template <class TValue>
    class ValueEntry: public BaseValueEntry<TValue>
    {
    public:
        void Clear()
        {
        }
    };

    // Class specialization for pointer values to support clearing
    template <class TValue>
    class ValueEntry<TValue*>: public BaseValueEntry<TValue*>
    {
    public:
        void Clear()
        {
            this->value = nullptr;
        }
    };

    template <>
    class ValueEntry<bool>: public BaseValueEntry<bool>
    {
    public:
        void Clear()
        {
            this->value = false;
        }
    };

    template <>
    class ValueEntry<int>: public BaseValueEntry<int>
    {
    public:
        void Clear()
        {
            this->value = 0;
        }
    };

    template <>
    class ValueEntry<uint>: public BaseValueEntry<uint>
    {
    public:
        void Clear()
        {
            this->value = 0;
        }
    };

    template<class TKey, class TValue>
    struct ValueToKey
    {
        static TKey ToKey(const TValue &value) { return static_cast<TKey>(value); }
    };

    // Used by BaseHashSet,  the default is that the key is the same as the value
    template <class TKey, class TValue>
    class ImplicitKeyValueEntry : public ValueEntry<TValue>
    {
    public:
        TKey Key() const { return ValueToKey<TKey, TValue>::ToKey(value); }

        void Set(TKey const& key, TValue const& value)
        {
            __super::Set(value);
        }
    };

    template <class TKey, class TValue>
    class BaseKeyValueEntry : public ValueEntry<TValue>
    {
    protected:
        TKey key;    // key of entry
        void Set(TKey const& key, TValue const& value)
        {
            __super::Set(value);
            this->key = key;
        }

    public:
        TKey const& Key() const  { return key; }
    };

    template <class TKey, class TValue>
    class KeyValueEntry : public BaseKeyValueEntry<TKey, TValue>
    {
    };

    template <class TKey, class TValue>
    class KeyValueEntry<TKey*, TValue> : public BaseKeyValueEntry<TKey*, TValue>
    {
    public:
        void Clear()
        {
            __super::Clear();
            this->key = nullptr;
        }
    };

    template <class TValue>
    class KeyValueEntry<int, TValue> : public BaseKeyValueEntry<int, TValue>
    {
    public:
        void Clear()
        {
            __super::Clear();
            this->key = 0;
        }
    };

    template <class TKey, class TValue, template <class K, class V> class THashEntry>
    class DefaultHashedEntry : public THashEntry<TKey, TValue>
    {
    public:
        template<typename Comparer, typename TLookup>
        __inline bool KeyEquals(TLookup const& otherKey, hash_t otherHashCode)
        {
            return Comparer::Equals(Key(), otherKey);
        }

        template<typename Comparer>
        __inline hash_t GetHashCode()
        {
            return ((Comparer::GetHashCode(Key()) & 0x7fffffff) << 1) | 1;
        }

        void Set(TKey const& key, TValue const& value, int hashCode)
        {
            __super::Set(key, value);
        }
    };

    template <class TKey, class TValue, template <class K, class V> class THashEntry>
    class CacheHashedEntry : public THashEntry<TKey, TValue>
    {
        hash_t hashCode;    // Lower 31 bits of hash code << 1 | 1, 0 if unused
    public:
        static const int INVALID_HASH_VALUE = 0;
        template<typename Comparer, typename TLookup>
        __inline bool KeyEquals(TLookup const& otherKey, hash_t otherHashCode)
        {
            Assert(TAGHASH(Comparer::GetHashCode(Key())) == this->hashCode);
            return this->hashCode == otherHashCode && Comparer::Equals(Key(), otherKey);
        }

        template<typename Comparer>
        __inline hash_t GetHashCode()
        {
            Assert(TAGHASH(Comparer::GetHashCode(Key())) == this->hashCode);
            return hashCode;
        }

        void Set(TKey const& key, TValue const& value, hash_t hashCode)
        {
            __super::Set(key, value);
            this->hashCode = hashCode;
        }

        void Clear()
        {
            __super::Clear();
            this->hashCode = INVALID_HASH_VALUE;
        }
    };

    template <class TKey, class TValue>
    class SimpleHashedEntry : public DefaultHashedEntry<TKey, TValue, ImplicitKeyValueEntry> {};

    template <class TKey, class TValue>
    class HashedEntry : public CacheHashedEntry<TKey, TValue, ImplicitKeyValueEntry> {};

    template <class TKey, class TValue>
    class SimpleDictionaryEntry : public DefaultHashedEntry<TKey, TValue, KeyValueEntry>  {};

    template <class TKey, class TValue>
    class DictionaryEntry: public CacheHashedEntry<TKey, TValue, KeyValueEntry> {};

    template <class TKey, class TValue>
    class WeakRefValueDictionaryEntry: public SimpleDictionaryEntry<TKey, TValue>
    {
    public:
        void Clear()
        {
            // Assuming nullable keys for now
            // This might change in future
            this->key = NULL;
            this->value = NULL;
        }

        static bool SupportsCleanup()
        {
            return true;
        }

        static bool NeedsCleanup(WeakRefValueDictionaryEntry<TKey, TValue> const& entry)
        {
            TValue weakReference = entry.Value();

            return (weakReference == nullptr || weakReference->Get() == nullptr);
        }
    };
}
