// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CollectionsWTF_h
#define CollectionsWTF_h

#include "wtf/Allocator.h"
#include "wtf/HashMap.h"
#include "wtf/PtrUtil.h"
#include "wtf/Vector.h"
#include "wtf/VectorTraits.h"

namespace blink {
namespace protocol {

template <typename T>
class Vector {
public:
    Vector() { }
    Vector(size_t capacity) : m_impl(capacity) { }
    typedef T* iterator;
    typedef const T* const_iterator;

    iterator begin() { return m_impl.begin(); }
    iterator end() { return m_impl.end(); }
    const_iterator begin() const { return m_impl.begin(); }
    const_iterator end() const { return m_impl.end(); }

    void resize(size_t s) { m_impl.resize(s); }
    size_t size() const { return m_impl.size(); }
    bool isEmpty() const { return m_impl.isEmpty(); }
    T& operator[](size_t i) { return at(i); }
    const T& operator[](size_t i) const { return at(i); }
    T& at(size_t i) { return m_impl.at(i); }
    const T& at(size_t i) const { return m_impl.at(i); }
    T& last() { return m_impl.last(); }
    const T& last() const { return m_impl.last(); }
    void append(const T& t) { m_impl.append(t); }
    void prepend(const T& t) { m_impl.prepend(t); }
    void remove(size_t i) { m_impl.remove(i); }
    void clear() { m_impl.clear(); }
    void swap(Vector<T>& other) { m_impl.swap(other.m_impl); }
    void removeLast() { m_impl.removeLast(); }

private:
    WTF::Vector<T> m_impl;
};

template <typename T>
class Vector<std::unique_ptr<T>> {
    WTF_MAKE_NONCOPYABLE(Vector);
public:
    Vector() { }
    Vector(size_t capacity) : m_impl(capacity) { }
    Vector(Vector<std::unique_ptr<T>>&& other) : m_impl(std::move(other.m_impl)) { }
    ~Vector() { }

    typedef std::unique_ptr<T>* iterator;
    typedef const std::unique_ptr<T>* const_iterator;

    iterator begin() { return m_impl.begin(); }
    iterator end() { return m_impl.end(); }
    const_iterator begin() const { return m_impl.begin(); }
    const_iterator end() const { return m_impl.end(); }

    void resize(size_t s) { m_impl.resize(s); }
    size_t size() const { return m_impl.size(); }
    bool isEmpty() const { return m_impl.isEmpty(); }
    T* operator[](size_t i) { return m_impl.at(i).get(); }
    const T* operator[](size_t i) const { return m_impl.at(i).get(); }
    T* at(size_t i) { return m_impl.at(i).get(); }
    const T* at(size_t i) const { return m_impl.at(i).get(); }
    T* last() { return m_impl.last().get(); }
    const T* last() const { return m_impl.last(); }
    void append(std::unique_ptr<T> t) { m_impl.append(std::move(t)); }
    void prepend(std::unique_ptr<T> t) { m_impl.prepend(std::move(t)); }
    void remove(size_t i) { m_impl.remove(i); }
    void clear() { m_impl.clear(); }
    void swap(Vector<std::unique_ptr<T>>& other) { m_impl.swap(other.m_impl); }
    void swap(Vector<std::unique_ptr<T>>&& other) { m_impl.swap(other.m_impl); }
    void removeLast() { m_impl.removeLast(); }

private:
    WTF::Vector<std::unique_ptr<T>> m_impl;
};

template <typename K, typename V, typename I>
class HashMapIterator {
    STACK_ALLOCATED();
public:
    HashMapIterator(const I& impl) : m_impl(impl) { }
    std::pair<K, V*>* get() const { m_pair = std::make_pair(m_impl->key, &m_impl->value); return &m_pair; }
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
    STACK_ALLOCATED();
public:
    HashMapIterator(const I& impl) : m_impl(impl) { }
    std::pair<K, V*>* get() const { m_pair = std::make_pair(m_impl->key, m_impl->value.get()); return &m_pair; }
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

    using iterator = HashMapIterator<K, V, typename WTF::HashMap<K, V>::iterator>;
    using const_iterator = HashMapIterator<K, const V, typename WTF::HashMap<K, V>::const_iterator>;

    iterator begin() { return iterator(m_impl.begin()); }
    iterator end() { return iterator(m_impl.end()); }
    iterator find(const K& k) { return iterator(m_impl.find(k)); }
    const_iterator begin() const { return const_iterator(m_impl.begin()); }
    const_iterator end() const { return const_iterator(m_impl.end()); }
    const_iterator find(const K& k) const { return const_iterator(m_impl.find(k)); }

    size_t size() const { return m_impl.size(); }
    bool isEmpty() const { return m_impl.isEmpty(); }
    bool set(const K& k, const V& v) { return m_impl.set(k, v).isNewEntry; }
    bool contains(const K& k) const { return m_impl.contains(k); }
    V get(const K& k) const { return m_impl.get(k); }
    void remove(const K& k) { m_impl.remove(k); }
    void clear() { m_impl.clear(); }
    V take(const K& k) { return m_impl.take(k); }

private:
    WTF::HashMap<K, V> m_impl;
};

template <typename K, typename V>
class HashMap<K, std::unique_ptr<V>> {
public:
    HashMap() { }
    ~HashMap() { }

    using iterator = HashMapIterator<K, std::unique_ptr<V>, typename WTF::HashMap<K, std::unique_ptr<V>>::iterator>;
    using const_iterator = HashMapIterator<K, std::unique_ptr<V>, typename WTF::HashMap<K, std::unique_ptr<V>>::const_iterator>;

    iterator begin() { return iterator(m_impl.begin()); }
    iterator end() { return iterator(m_impl.end()); }
    iterator find(const K& k) { return iterator(m_impl.find(k)); }
    const_iterator begin() const { return const_iterator(m_impl.begin()); }
    const_iterator end() const { return const_iterator(m_impl.end()); }
    const_iterator find(const K& k) const { return const_iterator(m_impl.find(k)); }

    size_t size() const { return m_impl.size(); }
    bool isEmpty() const { return m_impl.isEmpty(); }
    bool set(const K& k, std::unique_ptr<V> v) { return m_impl.set(k, std::move(v)).isNewEntry; }
    bool contains(const K& k) const { return m_impl.contains(k); }
    V* get(const K& k) const { return m_impl.get(k); }
    std::unique_ptr<V> take(const K& k) { return m_impl.take(k); }
    void remove(const K& k) { m_impl.remove(k); }
    void clear() { m_impl.clear(); }

private:
    WTF::HashMap<K, std::unique_ptr<V>> m_impl;
};

template <typename K>
class HashSet : public protocol::HashMap<K, K> {
public:
    void add(const K& k) { this->set(k, k); }
};

} // namespace platform
} // namespace blink

#endif // !defined(CollectionsWTF_h)
