// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ZONE_CONTAINERS_H_
#define V8_ZONE_CONTAINERS_H_

#include <deque>
#include <list>
#include <map>
#include <queue>
#include <set>
#include <stack>
#include <vector>

#include "src/zone-allocator.h"

namespace v8 {
namespace internal {

// A wrapper subclass for std::vector to make it easy to construct one
// that uses a zone allocator.
template <typename T>
class ZoneVector : public std::vector<T, zone_allocator<T>> {
 public:
  // Constructs an empty vector.
  explicit ZoneVector(Zone* zone)
      : std::vector<T, zone_allocator<T>>(zone_allocator<T>(zone)) {}

  // Constructs a new vector and fills it with {size} elements, each
  // constructed via the default constructor.
  ZoneVector(size_t size, Zone* zone)
      : std::vector<T, zone_allocator<T>>(size, T(), zone_allocator<T>(zone)) {}

  // Constructs a new vector and fills it with {size} elements, each
  // having the value {def}.
  ZoneVector(size_t size, T def, Zone* zone)
      : std::vector<T, zone_allocator<T>>(size, def, zone_allocator<T>(zone)) {}
};


// A wrapper subclass std::deque to make it easy to construct one
// that uses a zone allocator.
template <typename T>
class ZoneDeque : public std::deque<T, zone_allocator<T>> {
 public:
  // Constructs an empty deque.
  explicit ZoneDeque(Zone* zone)
      : std::deque<T, zone_allocator<T>>(zone_allocator<T>(zone)) {}
};


// A wrapper subclass std::list to make it easy to construct one
// that uses a zone allocator.
// TODO(mstarzinger): This should be renamed to ZoneList once we got rid of our
// own home-grown ZoneList that actually is a ZoneVector.
template <typename T>
class ZoneLinkedList : public std::list<T, zone_allocator<T>> {
 public:
  // Constructs an empty list.
  explicit ZoneLinkedList(Zone* zone)
      : std::list<T, zone_allocator<T>>(zone_allocator<T>(zone)) {}
};


// A wrapper subclass std::priority_queue to make it easy to construct one
// that uses a zone allocator.
template <typename T, typename Compare = std::less<T>>
class ZonePriorityQueue : public std::priority_queue<T, ZoneVector<T>> {
 public:
  // Constructs an empty list.
  explicit ZonePriorityQueue(Zone* zone)
      : std::priority_queue<T, ZoneVector<T>>(Compare(), ZoneVector<T>(zone)) {}
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
class ZoneSet : public std::set<K, Compare, zone_allocator<K>> {
 public:
  // Constructs an empty set.
  explicit ZoneSet(Zone* zone)
      : std::set<K, Compare, zone_allocator<K>>(Compare(),
                                                zone_allocator<K>(zone)) {}
};


// A wrapper subclass for std::map to make it easy to construct one that uses
// a zone allocator.
template <typename K, typename V, typename Compare = std::less<K>>
class ZoneMap
    : public std::map<K, V, Compare, zone_allocator<std::pair<K, V>>> {
 public:
  // Constructs an empty map.
  explicit ZoneMap(Zone* zone)
      : std::map<K, V, Compare, zone_allocator<std::pair<K, V>>>(
            Compare(), zone_allocator<std::pair<K, V>>(zone)) {}
};


// Typedefs to shorten commonly used vectors.
typedef ZoneVector<bool> BoolVector;
typedef ZoneVector<int> IntVector;

}  // namespace internal
}  // namespace v8

#endif  // V8_ZONE_CONTAINERS_H_
