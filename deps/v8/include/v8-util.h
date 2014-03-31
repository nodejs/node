// Copyright 2014 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_UTIL_H_
#define V8_UTIL_H_

#include "v8.h"
#include <map>

/**
 * Support for Persistent containers.
 *
 * C++11 embedders can use STL containers with UniquePersistent values,
 * but pre-C++11 does not support the required move semantic and hence
 * may want these container classes.
 */
namespace v8 {

typedef uintptr_t PersistentContainerValue;
static const uintptr_t kPersistentContainerNotFound = 0;


/**
 * A default trait implemenation for PersistentValueMap which uses std::map
 * as a backing map.
 *
 * Users will have to implement their own weak callbacks & dispose traits.
 */
template<typename K, typename V>
class StdMapTraits {
 public:
  // STL map & related:
  typedef std::map<K, PersistentContainerValue> Impl;
  typedef typename Impl::iterator Iterator;

  static bool Empty(Impl* impl) { return impl->empty(); }
  static size_t Size(Impl* impl) { return impl->size(); }
  static void Swap(Impl& a, Impl& b) { std::swap(a, b); }  // NOLINT
  static Iterator Begin(Impl* impl) { return impl->begin(); }
  static Iterator End(Impl* impl) { return impl->end(); }
  static K Key(Iterator it) { return it->first; }
  static PersistentContainerValue Value(Iterator it) { return it->second; }
  static PersistentContainerValue Set(Impl* impl, K key,
      PersistentContainerValue value) {
    std::pair<Iterator, bool> res = impl->insert(std::make_pair(key, value));
    PersistentContainerValue old_value = kPersistentContainerNotFound;
    if (!res.second) {
      old_value = res.first->second;
      res.first->second = value;
    }
    return old_value;
  }
  static PersistentContainerValue Get(Impl* impl, K key) {
    Iterator it = impl->find(key);
    if (it == impl->end()) return kPersistentContainerNotFound;
    return it->second;
  }
  static PersistentContainerValue Remove(Impl* impl, K key) {
    Iterator it = impl->find(key);
    if (it == impl->end()) return kPersistentContainerNotFound;
    PersistentContainerValue value = it->second;
    impl->erase(it);
    return value;
  }
};


/**
 * A default trait implementation for PersistentValueMap, which inherits
 * a std:map backing map from StdMapTraits and holds non-weak persistent
 * objects.
 *
 * Users have to implement their own dispose trait.
 */
template<typename K, typename V>
class StrongMapTraits : public StdMapTraits<K, V> {
 public:
  // Weak callback & friends:
  static const bool kIsWeak = false;
  typedef typename StdMapTraits<K, V>::Impl Impl;
  typedef void WeakCallbackDataType;
  static WeakCallbackDataType* WeakCallbackParameter(
      Impl* impl, const K& key, Local<V> value);
  static Impl* ImplFromWeakCallbackData(
      const WeakCallbackData<V, WeakCallbackDataType>& data);
  static K KeyFromWeakCallbackData(
      const WeakCallbackData<V, WeakCallbackDataType>& data);
  static void DisposeCallbackData(WeakCallbackDataType* data);
};


/**
 * A default trait implementation for PersistentValueMap, with a std::map
 * backing map, non-weak persistents as values, and no special dispose
 * handling. Can be used as-is.
 */
template<typename K, typename V>
class DefaultPersistentValueMapTraits : public StrongMapTraits<K, V> {
 public:
  typedef typename StrongMapTraits<K, V>::Impl Impl;
  static void Dispose(Isolate* isolate, UniquePersistent<V> value,
      Impl* impl, K key) { }
};


/**
 * A map wrapper that allows using UniquePersistent as a mapped value.
 * C++11 embedders don't need this class, as they can use UniquePersistent
 * directly in std containers.
 *
 * The map relies on a backing map, whose type and accessors are described
 * by the Traits class. The backing map will handle values of type
 * PersistentContainerValue, with all conversion into and out of V8
 * handles being transparently handled by this class.
 */
template<typename K, typename V, typename Traits>
class PersistentValueMap {
 public:
  V8_INLINE explicit PersistentValueMap(Isolate* isolate) : isolate_(isolate) {}

  V8_INLINE ~PersistentValueMap() { Clear(); }

  V8_INLINE Isolate* GetIsolate() { return isolate_; }

  /**
   * Return size of the map.
   */
  V8_INLINE size_t Size() { return Traits::Size(&impl_); }

  /**
   * Return whether the map holds weak persistents.
   */
  V8_INLINE bool IsWeak() { return Traits::kIsWeak; }

  /**
   * Get value stored in map.
   */
  V8_INLINE Local<V> Get(const K& key) {
    return Local<V>::New(isolate_, FromVal(Traits::Get(&impl_, key)));
  }

  /**
   * Check whether a value is contained in the map.
   */
  V8_INLINE bool Contains(const K& key) {
    return Traits::Get(&impl_, key) != 0;
  }

  /**
   * Get value stored in map and set it in returnValue.
   * Return true if a value was found.
   */
  V8_INLINE bool SetReturnValue(const K& key,
      ReturnValue<Value>& returnValue) {
    PersistentContainerValue value = Traits::Get(&impl_, key);
    bool hasValue = value != 0;
    if (hasValue) {
      returnValue.SetInternal(
          *reinterpret_cast<internal::Object**>(FromVal(value)));
    }
    return hasValue;
  }

  /**
   * Call Isolate::SetReference with the given parent and the map value.
   */
  V8_INLINE void SetReference(const K& key,
      const Persistent<Object>& parent) {
    GetIsolate()->SetReference(
      reinterpret_cast<internal::Object**>(parent.val_),
      reinterpret_cast<internal::Object**>(FromVal(Traits::Get(&impl_, key))));
  }

  /**
   * Put value into map. Depending on Traits::kIsWeak, the value will be held
   * by the map strongly or weakly.
   * Returns old value as UniquePersistent.
   */
  UniquePersistent<V> Set(const K& key, Local<V> value) {
    UniquePersistent<V> persistent(isolate_, value);
    return SetUnique(key, &persistent);
  }

  /**
   * Put value into map, like Set(const K&, Local<V>).
   */
  UniquePersistent<V> Set(const K& key, UniquePersistent<V> value) {
    return SetUnique(key, &value);
  }

  /**
   * Return value for key and remove it from the map.
   */
  V8_INLINE UniquePersistent<V> Remove(const K& key) {
    return Release(Traits::Remove(&impl_, key)).Pass();
  }

  /**
  * Traverses the map repeatedly,
  * in case side effects of disposal cause insertions.
  **/
  void Clear() {
    typedef typename Traits::Iterator It;
    HandleScope handle_scope(isolate_);
    // TODO(dcarney): figure out if this swap and loop is necessary.
    while (!Traits::Empty(&impl_)) {
      typename Traits::Impl impl;
      Traits::Swap(impl_, impl);
      for (It i = Traits::Begin(&impl); i != Traits::End(&impl); ++i) {
        Traits::Dispose(isolate_, Release(Traits::Value(i)).Pass(), &impl,
          Traits::Key(i));
      }
    }
  }

 private:
  PersistentValueMap(PersistentValueMap&);
  void operator=(PersistentValueMap&);

  /**
   * Put the value into the map, and set the 'weak' callback when demanded
   * by the Traits class.
   */
  UniquePersistent<V> SetUnique(const K& key, UniquePersistent<V>* persistent) {
    if (Traits::kIsWeak) {
      Local<V> value(Local<V>::New(isolate_, *persistent));
      persistent->template SetWeak<typename Traits::WeakCallbackDataType>(
        Traits::WeakCallbackParameter(&impl_, key, value), WeakCallback);
    }
    PersistentContainerValue old_value =
        Traits::Set(&impl_, key, ClearAndLeak(persistent));
    return Release(old_value).Pass();
  }

  static void WeakCallback(
      const WeakCallbackData<V, typename Traits::WeakCallbackDataType>& data) {
    if (Traits::kIsWeak) {
      typename Traits::Impl* impl = Traits::ImplFromWeakCallbackData(data);
      K key = Traits::KeyFromWeakCallbackData(data);
      PersistentContainerValue value = Traits::Remove(impl, key);
      Traits::Dispose(data.GetIsolate(), Release(value).Pass(), impl, key);
    }
  }

  V8_INLINE static V* FromVal(PersistentContainerValue v) {
    return reinterpret_cast<V*>(v);
  }

  V8_INLINE static PersistentContainerValue ClearAndLeak(
      UniquePersistent<V>* persistent) {
    V* v = persistent->val_;
    persistent->val_ = 0;
    return reinterpret_cast<PersistentContainerValue>(v);
  }

  /**
   * Return a container value as UniquePersistent and make sure the weak
   * callback is properly disposed of. All remove functionality should go
   * through this.
   */
  V8_INLINE static UniquePersistent<V> Release(PersistentContainerValue v) {
    UniquePersistent<V> p;
    p.val_ = FromVal(v);
    if (Traits::kIsWeak && !p.IsEmpty()) {
      Traits::DisposeCallbackData(
          p.template ClearWeak<typename Traits::WeakCallbackDataType>());
    }
    return p.Pass();
  }

  Isolate* isolate_;
  typename Traits::Impl impl_;
};


/**
 * A map that uses UniquePersistent as value and std::map as the backing
 * implementation. Persistents are held non-weak.
 *
 * C++11 embedders don't need this class, as they can use
 * UniquePersistent directly in std containers.
 */
template<typename K, typename V,
    typename Traits = DefaultPersistentValueMapTraits<K, V> >
class StdPersistentValueMap : public PersistentValueMap<K, V, Traits> {
 public:
  explicit StdPersistentValueMap(Isolate* isolate)
      : PersistentValueMap<K, V, Traits>(isolate) {}
};


/**
 * Empty default implementations for StrongTraits methods.
 *
 * These should not be necessary, since they're only used in code that
 * is surrounded by if(Traits::kIsWeak), which for StrongMapTraits is
 * compile-time false. Most compilers can live without them; however
 * the compiler we use from 64-bit Win differs.
 *
 * TODO(vogelheim): Remove these once they're no longer necessary.
 */
template<typename K, typename V>
typename StrongMapTraits<K, V>::WeakCallbackDataType*
    StrongMapTraits<K, V>::WeakCallbackParameter(
        Impl* impl, const K& key, Local<V> value) {
  return NULL;
}


template<typename K, typename V>
typename StrongMapTraits<K, V>::Impl*
    StrongMapTraits<K, V>::ImplFromWeakCallbackData(
        const WeakCallbackData<V, WeakCallbackDataType>& data) {
  return NULL;
}


template<typename K, typename V>
K StrongMapTraits<K, V>::KeyFromWeakCallbackData(
    const WeakCallbackData<V, WeakCallbackDataType>& data) {
  return K();
}


template<typename K, typename V>
void StrongMapTraits<K, V>::DisposeCallbackData(WeakCallbackDataType* data) {
}

}  // namespace v8

#endif  // V8_UTIL_H_
