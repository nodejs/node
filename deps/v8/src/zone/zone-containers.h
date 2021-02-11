// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ZONE_ZONE_CONTAINERS_H_
#define V8_ZONE_ZONE_CONTAINERS_H_

#include <deque>
#include <forward_list>
#include <list>
#include <map>
#include <queue>
#include <set>
#include <stack>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "src/base/functional.h"
#include "src/zone/zone-allocator.h"

namespace v8 {
namespace internal {

// A wrapper subclass for std::vector to make it easy to construct one
// that uses a zone allocator.
template <typename T>
class ZoneVector : public std::vector<T, ZoneAllocator<T>> {
 public:
  // Constructs an empty vector.
  explicit ZoneVector(Zone* zone)
      : std::vector<T, ZoneAllocator<T>>(ZoneAllocator<T>(zone)) {}

  // Constructs a new vector and fills it with {size} elements, each
  // constructed via the default constructor.
  ZoneVector(size_t size, Zone* zone)
      : std::vector<T, ZoneAllocator<T>>(size, T(), ZoneAllocator<T>(zone)) {}

  // Constructs a new vector and fills it with {size} elements, each
  // having the value {def}.
  ZoneVector(size_t size, T def, Zone* zone)
      : std::vector<T, ZoneAllocator<T>>(size, def, ZoneAllocator<T>(zone)) {}

  // Constructs a new vector and fills it with the contents of the given
  // initializer list.
  ZoneVector(std::initializer_list<T> list, Zone* zone)
      : std::vector<T, ZoneAllocator<T>>(list, ZoneAllocator<T>(zone)) {}

  // Constructs a new vector and fills it with the contents of the range
  // [first, last).
  template <class InputIt>
  ZoneVector(InputIt first, InputIt last, Zone* zone)
      : std::vector<T, ZoneAllocator<T>>(first, last, ZoneAllocator<T>(zone)) {}
};

// A wrapper subclass for std::deque to make it easy to construct one
// that uses a zone allocator.
template <typename T>
class ZoneDeque : public std::deque<T, RecyclingZoneAllocator<T>> {
 public:
  // Constructs an empty deque.
  explicit ZoneDeque(Zone* zone)
      : std::deque<T, RecyclingZoneAllocator<T>>(
            RecyclingZoneAllocator<T>(zone)) {}
};

// A wrapper subclass for std::list to make it easy to construct one
// that uses a zone allocator.
// TODO(all): This should be renamed to ZoneList once we got rid of our own
// home-grown ZoneList that actually is a ZoneVector.
template <typename T>
class ZoneLinkedList : public std::list<T, ZoneAllocator<T>> {
 public:
  // Constructs an empty list.
  explicit ZoneLinkedList(Zone* zone)
      : std::list<T, ZoneAllocator<T>>(ZoneAllocator<T>(zone)) {}
};

// A wrapper subclass for std::forward_list to make it easy to construct one
// that uses a zone allocator.
template <typename T>
class ZoneForwardList : public std::forward_list<T, ZoneAllocator<T>> {
 public:
  // Constructs an empty list.
  explicit ZoneForwardList(Zone* zone)
      : std::forward_list<T, ZoneAllocator<T>>(ZoneAllocator<T>(zone)) {}
};

// A wrapper subclass for std::priority_queue to make it easy to construct one
// that uses a zone allocator.
template <typename T, typename Compare = std::less<T>>
class ZonePriorityQueue
    : public std::priority_queue<T, ZoneVector<T>, Compare> {
 public:
  // Constructs an empty list.
  explicit ZonePriorityQueue(Zone* zone)
      : std::priority_queue<T, ZoneVector<T>, Compare>(Compare(),
                                                       ZoneVector<T>(zone)) {}
};

// A wrapper subclass for std::queue to make it easy to construct one
// that uses a zone allocator.
template <typename T>
class ZoneQueue : public std::queue<T, ZoneDeque<T>> {
 public:
  // Constructs an empty queue.
  explicit ZoneQueue(Zone* zone)
      : std::queue<T, ZoneDeque<T>>(ZoneDeque<T>(zone)) {}
};

// A wrapper subclass for std::stack to make it easy to construct one that uses
// a zone allocator.
template <typename T>
class ZoneStack : public std::stack<T, ZoneDeque<T>> {
 public:
  // Constructs an empty stack.
  explicit ZoneStack(Zone* zone)
      : std::stack<T, ZoneDeque<T>>(ZoneDeque<T>(zone)) {}
};

// A wrapper subclass for std::set to make it easy to construct one that uses
// a zone allocator.
template <typename K, typename Compare = std::less<K>>
class ZoneSet : public std::set<K, Compare, ZoneAllocator<K>> {
 public:
  // Constructs an empty set.
  explicit ZoneSet(Zone* zone)
      : std::set<K, Compare, ZoneAllocator<K>>(Compare(),
                                               ZoneAllocator<K>(zone)) {}
};

// A wrapper subclass for std::multiset to make it easy to construct one that
// uses a zone allocator.
template <typename K, typename Compare = std::less<K>>
class ZoneMultiset : public std::multiset<K, Compare, ZoneAllocator<K>> {
 public:
  // Constructs an empty set.
  explicit ZoneMultiset(Zone* zone)
      : std::multiset<K, Compare, ZoneAllocator<K>>(Compare(),
                                                    ZoneAllocator<K>(zone)) {}
};

// A wrapper subclass for std::map to make it easy to construct one that uses
// a zone allocator.
template <typename K, typename V, typename Compare = std::less<K>>
class ZoneMap
    : public std::map<K, V, Compare, ZoneAllocator<std::pair<const K, V>>> {
 public:
  // Constructs an empty map.
  explicit ZoneMap(Zone* zone)
      : std::map<K, V, Compare, ZoneAllocator<std::pair<const K, V>>>(
            Compare(), ZoneAllocator<std::pair<const K, V>>(zone)) {}
};

// A wrapper subclass for std::unordered_map to make it easy to construct one
// that uses a zone allocator.
template <typename K, typename V, typename Hash = base::hash<K>,
          typename KeyEqual = std::equal_to<K>>
class ZoneUnorderedMap
    : public std::unordered_map<K, V, Hash, KeyEqual,
                                ZoneAllocator<std::pair<const K, V>>> {
 public:
  // Constructs an empty map.
  explicit ZoneUnorderedMap(Zone* zone, size_t bucket_count = 100)
      : std::unordered_map<K, V, Hash, KeyEqual,
                           ZoneAllocator<std::pair<const K, V>>>(
            bucket_count, Hash(), KeyEqual(),
            ZoneAllocator<std::pair<const K, V>>(zone)) {}
};

// A wrapper subclass for std::unordered_set to make it easy to construct one
// that uses a zone allocator.
template <typename K, typename Hash = base::hash<K>,
          typename KeyEqual = std::equal_to<K>>
class ZoneUnorderedSet
    : public std::unordered_set<K, Hash, KeyEqual, ZoneAllocator<K>> {
 public:
  // Constructs an empty map.
  explicit ZoneUnorderedSet(Zone* zone, size_t bucket_count = 100)
      : std::unordered_set<K, Hash, KeyEqual, ZoneAllocator<K>>(
            bucket_count, Hash(), KeyEqual(), ZoneAllocator<K>(zone)) {}
};

// A wrapper subclass for std::multimap to make it easy to construct one that
// uses a zone allocator.
template <typename K, typename V, typename Compare = std::less<K>>
class ZoneMultimap
    : public std::multimap<K, V, Compare,
                           ZoneAllocator<std::pair<const K, V>>> {
 public:
  // Constructs an empty multimap.
  explicit ZoneMultimap(Zone* zone)
      : std::multimap<K, V, Compare, ZoneAllocator<std::pair<const K, V>>>(
            Compare(), ZoneAllocator<std::pair<const K, V>>(zone)) {}
};

// Typedefs to shorten commonly used vectors.
using BoolVector = ZoneVector<bool>;
using IntVector = ZoneVector<int>;

}  // namespace internal
}  // namespace v8

#endif  // V8_ZONE_ZONE_CONTAINERS_H_
