// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_REFS_MAP_H_
#define V8_COMPILER_REFS_MAP_H_

#include "src/base/hashmap.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {
namespace compiler {

class ObjectData;

class AddressMatcher : public base::KeyEqualityMatcher<Address> {
 public:
  bool operator()(uint32_t hash1, uint32_t hash2, const Address& key1,
                  const Address& key2) const {
    return key1 == key2;
  }
};

// This class employs our own implementation of hash map for the purpose of
// storing the mapping between canonical Addresses and allocated ObjectData.
// It's used as the refs map in JSHeapBroker and as the snapshot in
// PerIsolateCompilerCache, as we need a cheap copy between the two and
// std::unordered_map doesn't satisfy this requirement, as it rehashes the
// whole map and copies all entries one by one.
class RefsMap
    : public base::TemplateHashMapImpl<Address, ObjectData*, AddressMatcher,
                                       ZoneAllocationPolicy>,
      public ZoneObject {
 public:
  RefsMap(uint32_t capacity, AddressMatcher match, Zone* zone);
  RefsMap(const RefsMap* other, Zone* zone);

  bool IsEmpty() const { return occupancy() == 0; }

  // Wrappers around methods from UnderlyingMap
  Entry* Lookup(const Address& key) const;
  Entry* LookupOrInsert(const Address& key);
  ObjectData* Remove(const Address& key);

 private:
  static uint32_t Hash(Address addr);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_REFS_MAP_H_
