// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_LOOKUP_CACHE_H_
#define V8_OBJECTS_LOOKUP_CACHE_H_

#include "src/objects/map.h"
#include "src/objects/name.h"
#include "src/objects/objects.h"

namespace v8 {
namespace internal {

// Cache for mapping (map, property name) into descriptor index.
// The cache contains both positive and negative results.
// Descriptor index equals kNotFound means the property is absent.
// Cleared at startup and prior to any gc.
class DescriptorLookupCache {
 public:
  // Lookup descriptor index for (map, name).
  // If absent, kAbsent is returned.
  inline int Lookup(Map source, Name name);

  // Update an element in the cache.
  inline void Update(Map source, Name name, int result);

  // Clear the cache.
  void Clear();

  static const int kAbsent = -2;

 private:
  DescriptorLookupCache() {
    for (int i = 0; i < kLength; ++i) {
      keys_[i].source = Map();
      keys_[i].name = Name();
      results_[i] = kAbsent;
    }
  }

  static inline int Hash(Map source, Name name);

  static const int kLength = 64;
  struct Key {
    Map source;
    Name name;
  };

  Key keys_[kLength];
  int results_[kLength];

  friend class Isolate;
  DISALLOW_COPY_AND_ASSIGN(DescriptorLookupCache);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_LOOKUP_CACHE_H_
