// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CollectionsSTL_h
#define CollectionsSTL_h

#include "platform/inspector_protocol/String16.h"
#include "wtf/Compiler.h"
#include "wtf/PtrUtil.h"

#include <algorithm>
#include <map>
#include <vector>

namespace blink {
namespace protocol {

template <typename T>
class Vector {
public:
    Vector() { }
    Vector(size_t capacity) : m_impl(capacity) { }
    typedef typename std::vector<T>::iterator iterator;
    typedef typename std::vector<T>::const_iterator const_iterator;

    iterator begin() { return m_impl.begin(); }
    iterator end() { return m_impl.end(); }
    const_iterator begin() const { return m_impl.begin(); }
    const_iterator end() const { return m_impl.end(); }

    void resize(size_t s) { m_impl.resize(s); }
    size_t size() const { return m_impl.size(); }
    bool isEmpty() const { return !m_impl.size(); }
    T& operator[](size_t i) { return at(i); }
    const T& operator[](size_t i) const { return at(i); }
    T& at(size_t i) { return m_impl[i]; }
    const T& at(size_t i) const { return m_impl.at(i); }
    T& last() { return m_impl[m_impl.size() - 1]; }
    const T& last() const { return m_impl[m_impl.size() - 1]; }
    void append(const T& t) { m_impl.push_back(t); }
    void prepend(const T& t) { m_impl.insert(m_impl.begin(), t); }
    void remove(size_t i) { m_impl.erase(m_impl.begin() + i); }
    void clear() { m_impl.clear(); }
    void swap(Vector& other) { m_impl.swap(other.m_impl); }
    void removeLast() { m_impl.pop_back(); }

private:
    std::vector<T> m_impl;
};

template <typename T>
class Vector<std::unique_ptr<T>> {
public:
    Vector() { }
    Vector(size_t capacity) : m_impl(capacity) { }
    Vector(Vector&& other) { m_impl.swap(other.m_impl); }
    ~Vector() { clear(); }

    typedef typename std::vector<T*>::iterator iterator;
    typedef typename std::vector<T*>::const_iterator const_iterator;

    iterator begin() { return m_impl.begin(); }
    iterator end() { return m_impl.end(); }
    const_iterator begin() const { return m_impl.begin(); }
    const_iterator end() const { return m_impl.end(); }

    void resize(size_t s) { m_impl.resize(s); }
    size_t size() const { return m_impl.size(); }
    bool isEmpty() const { return !m_impl.size(); }
    T* operator[](size_t i) { return at(i); }
    const T* operator[](size_t i) const { return at(i); }
    T* at(size_t i) { return m_impl[i]; }
    const T* at(size_t i) const { return m_impl.at(i); }
    T* last() { return m_impl[m_impl.size() - 1]; }
    const T* last() const { return m_impl[m_impl.size() - 1]; }
    void append(std::unique_ptr<T> t) { m_impl.push_back(t.release()); }
    void prepend(std::unique_ptr<T> t) { m_impl.insert(m_impl.begin(), t.release()); }

    void remove(size_t i)
    {
        delete m_impl[i];
        m_impl.erase(m_impl.begin() + i);
    }

    void clear()
    {
        for (auto t : m_impl)
            delete t;
        m_impl.clear();
    }

    void swap(Vector& other) { m_impl.swap(other.m_impl); }
    void swap(Vector&& other) { m_impl.swap(other.m_impl); }
    void removeLast()
    {
        delete last();
        m_impl.pop_back();
    }

private:
    Vector(const Vector&) = delete;
    Vector& operator=(const Vector&) = delete;
    std::vector<T*> m_impl;
};

template <typename K, typename V, typename I>
class HashMapIterator {
public:
    HashMapIterator(const I& impl) : m_impl(impl) { }
    std::pair<K, V*>* get() const { m_pair.first = m_impl->first; m_pair.second = &m_impl->second; return &m_pair; }
    std::pair<K, V*>& operator*() const { return *get(); }
    std::pair<K, V*>* operator->() const { return get(); }

    bool operator==(const HashMapIterator<K, V, I>& other) const { return m_impl == other.m_impl; }
    bool operator!=(const HashMapIterator<K, V, I>& other) const { return m_impl != other.m_impl; }

    HashMapIterator<K, V, I>& operator++() { ++m_impl; return *this; }

private:
    mutable std::pair<K, V*> m_pair;
    I m_impl;
};

template <typename K, typename V, typename I>
class HashMapIterator<K, std::unique_ptr<V>, I> {
public:
    HashMapIterator(const I& impl) : m_impl(impl) { }
    std::pair<K, V*>* get() const { m_pair.first = m_impl->first; m_pair.second = m_impl->second; return &m_pair; }
    std::pair<K, V*>& operator*() const { return *get(); }
    std::pair<K, V*>* operator->() const { return get(); }

    bool operator==(const HashMapIterator<K, std::unique_ptr<V>, I>& other) const { return m_impl == other.m_impl; }
    bool operator!=(const HashMapIterator<K, std::unique_ptr<V>, I>& other) const { return m_impl != other.m_impl; }

    HashMapIterator<K, std::unique_ptr<V>, I>& operator++() { ++m_impl; return *this; }

private:
    mutable std::pair<K, V*> m_pair;
    I m_impl;
};

template <typename K, typename V>
class HashMap {
public:
    HashMap() { }
    ~HashMap() { }

    using iterator = HashMapIterator<K, V, typename std::map<K, V>::iterator>;
    using const_iterator = HashMapIterator<K, const V, typename std::map<K, V>::const_iterator>;

    iterator begin() { return iterator(m_impl.begin()); }
    iterator end() { return iterator(m_impl.end()); }
    iterator find(const K& k) { return iterator(m_impl.find(k)); }
    const_iterator begin() const { return const_iterator(m_impl.begin()); }
    const_iterator end() const { return const_iterator(m_impl.end()); }
    const_iterator find(const K& k) const { return const_iterator(m_impl.find(k)); }

    size_t size() const { return m_impl.size(); }
    bool isEmpty() const { return !m_impl.size(); }
    bool set(const K& k, const V& v)
    {
        bool isNew = m_impl.find(k) == m_impl.end();
        m_impl[k] = v;
        return isNew;
    }
    bool contains(const K& k) const { return m_impl.find(k) != m_impl.end(); }
    V get(const K& k) const { auto it = m_impl.find(k); return it == m_impl.end() ? V() : it->second; }
    void remove(const K& k) { m_impl.erase(k); }
    void clear() { m_impl.clear(); }
    V take(const K& k)
    {
        V result = m_impl[k];
        m_impl.erase(k);
        return result;
    }

private:
    std::map<K, V> m_impl;
};

template <typename K, typename V>
class HashMap<K, std::unique_ptr<V>> {
public:
    HashMap() { }
    ~HashMap() { clear(); }

    using iterator = HashMapIterator<K, std::unique_ptr<V>, typename std::map<K, V*>::iterator>;
    using const_iterator = HashMapIterator<K, std::unique_ptr<V>, typename std::map<K, V*>::const_iterator>;

    iterator begin() { return iterator(m_impl.begin()); }
    iterator end() { return iterator(m_impl.end()); }
    iterator find(const K& k) { return iterator(m_impl.find(k)); }
    const_iterator begin() const { return const_iterator(m_impl.begin()); }
    const_iterator end() const { return const_iterator(m_impl.end()); }
    const_iterator find(const K& k) const { return const_iterator(m_impl.find(k)); }

    size_t size() const { return m_impl.size(); }
    bool isEmpty() const { return !m_impl.size(); }
    bool set(const K& k, std::unique_ptr<V> v)
    {
        bool isNew = m_impl.find(k) == m_impl.end();
        if (!isNew)
            delete m_impl[k];
        m_impl[k] = v.release();
        return isNew;
    }
    bool contains(const K& k) const { return m_impl.find(k) != m_impl.end(); }
    V* get(const K& k) const { auto it = m_impl.find(k); return it == m_impl.end() ? nullptr : it->second; }
    std::unique_ptr<V> take(const K& k)
    {
        if (!contains(k))
            return nullptr;
        std::unique_ptr<V> result(m_impl[k]);
        delete m_impl[k];
        m_impl.erase(k);
        return result;
    }
    void remove(const K& k)
    {
        delete m_impl[k];
        m_impl.erase(k);
    }

    void clear()
    {
        for (auto pair : m_impl)
            delete pair.second;
        m_impl.clear();
    }

private:
    std::map<K, V*> m_impl;
};

template <typename K>
class HashSet : public protocol::HashMap<K, K> {
public:
    void add(const K& k) { this->set(k, k); }
};

} // namespace platform
} // namespace blink

// Macro that returns a compile time constant with the length of an array, but gives an error if passed a non-array.
template<typename T, size_t Size> char (&ArrayLengthHelperFunction(T (&)[Size]))[Size];
// GCC needs some help to deduce a 0 length array.
#if COMPILER(GCC)
template<typename T> char (&ArrayLengthHelperFunction(T (&)[0]))[0];
#endif
#define PROTOCOL_ARRAY_LENGTH(array) sizeof(::ArrayLengthHelperFunction(array))

#endif // !defined(CollectionsSTL_h)
