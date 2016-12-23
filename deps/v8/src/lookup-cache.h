// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LOOKUP_CACHE_H_
#define V8_LOOKUP_CACHE_H_

#include "src/objects.h"

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
  inline int Lookup(Map* source, Name* name);

  // Update an element in the cache.
  inline void Update(Map* source, Name* name, int result);

  // Clear the cache.
  void Clear();

  static const int kAbsent = -2;

 private:
  DescriptorLookupCache() {
    for (int i = 0; i < kLength; ++i) {
      keys_[i].source = NULL;
      keys_[i].name = NULL;
      results_[i] = kAbsent;
    }
  }

  static inline int Hash(Object* source, Name* name);

  static const int kLength = 64;
  struct Key {
    Map* source;
    Name* name;
  };

  Key keys_[kLength];
  int results_[kLength];

  friend class Isolate;
  DISALLOW_COPY_AND_ASSIGN(DescriptorLookupCache);
};

// Cache for mapping (map, property name) into field offset.
// Cleared at startup and prior to mark sweep collection.
class KeyedLookupCache {
 public:
  // Lookup field offset for (map, name). If absent, -1 is returned.
  int Lookup(Handle<Map> map, Handle<Name> name);

  // Update an element in the cache.
  void Update(Handle<Map> map, Handle<Name> name, int field_offset);

  // Clear the cache.
  void Clear();

  static const int kLength = 256;
  static const int kCapacityMask = kLength - 1;
  static const int kMapHashShift = 5;
  static const int kHashMask = -4;  // Zero the last two bits.
  static const int kEntriesPerBucket = 4;
  static const int kEntryLength = 2;
  static const int kMapIndex = 0;
  static const int kKeyIndex = 1;
  static const int kNotFound = -1;

  // kEntriesPerBucket should be a power of 2.
  STATIC_ASSERT((kEntriesPerBucket & (kEntriesPerBucket - 1)) == 0);
  STATIC_ASSERT(kEntriesPerBucket == -kHashMask);

 private:
  KeyedLookupCache() {
    for (int i = 0; i < kLength; ++i) {
      keys_[i].map = NULL;
      keys_[i].name = NULL;
      field_offsets_[i] = kNotFound;
    }
  }

  static inline int Hash(Handle<Map> map, Handle<Name> name);

  // Get the address of the keys and field_offsets arrays.  Used in
  // generated code to perform cache lookups.
  Address keys_address() { return reinterpret_cast<Address>(&keys_); }

  Address field_offsets_address() {
    return reinterpret_cast<Address>(&field_offsets_);
  }

  struct Key {
    Map* map;
    Name* name;
  };

  Key keys_[kLength];
  int field_offsets_[kLength];

  friend class ExternalReference;
  friend class Isolate;
  DISALLOW_COPY_AND_ASSIGN(KeyedLookupCache);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_LOOKUP_CACHE_H_
