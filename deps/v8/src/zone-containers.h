// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ZONE_CONTAINERS_H_
#define V8_ZONE_CONTAINERS_H_

#include <deque>
#include <queue>
#include <vector>

#include "src/zone-allocator.h"

namespace v8 {
namespace internal {

// A wrapper subclass for std::vector to make it easy to construct one
// that uses a zone allocator.
template <typename T>
class ZoneVector : public std::vector<T, zone_allocator<T> > {
 public:
  // Constructs an empty vector.
  explicit ZoneVector(Zone* zone)
      : std::vector<T, zone_allocator<T> >(zone_allocator<T>(zone)) {}

  // Constructs a new vector and fills it with {size} elements, each
  // constructed via the default constructor.
  ZoneVector(int size, Zone* zone)
      : std::vector<T, zone_allocator<T> >(size, T(), zone_allocator<T>(zone)) {
  }

  // Constructs a new vector and fills it with {size} elements, each
  // having the value {def}.
  ZoneVector(int size, T def, Zone* zone)
      : std::vector<T, zone_allocator<T> >(size, def, zone_allocator<T>(zone)) {
  }
};

// A wrapper subclass std::deque to make it easy to construct one
// that uses a zone allocator.
template <typename T>
class ZoneDeque : public std::deque<T, zone_allocator<T> > {
 public:
  explicit ZoneDeque(Zone* zone)
      : std::deque<T, zone_allocator<T> >(zone_allocator<T>(zone)) {}
};

// A wrapper subclass for std::queue to make it easy to construct one
// that uses a zone allocator.
template <typename T>
class ZoneQueue : public std::queue<T, std::deque<T, zone_allocator<T> > > {
 public:
  // Constructs an empty queue.
  explicit ZoneQueue(Zone* zone)
      : std::queue<T, std::deque<T, zone_allocator<T> > >(
            std::deque<T, zone_allocator<T> >(zone_allocator<T>(zone))) {}
};

// Typedefs to shorten commonly used vectors.
typedef ZoneVector<bool> BoolVector;
typedef ZoneVector<int> IntVector;
} }  // namespace v8::internal

#endif  // V8_ZONE_CONTAINERS_H_
