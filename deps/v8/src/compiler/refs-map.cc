// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/refs-map.h"

#include "src/base/hashing.h"

namespace v8 {
namespace internal {
namespace compiler {

using UnderlyingMap =
    base::TemplateHashMapImpl<Address, ObjectData*, AddressMatcher,
                              ZoneAllocationPolicy>;

RefsMap::RefsMap(uint32_t capacity, AddressMatcher match, Zone* zone)
    : UnderlyingMap(capacity, match, ZoneAllocationPolicy(zone)) {}

RefsMap::RefsMap(const RefsMap* other, Zone* zone)
    : UnderlyingMap(other, ZoneAllocationPolicy(zone)) {}

RefsMap::Entry* RefsMap::Lookup(const Address& key) const {
  return UnderlyingMap::Lookup(key, Hash(key));
}

RefsMap::Entry* RefsMap::InsertNew(const Address& key) {
  return UnderlyingMap::InsertNew(key, RefsMap::Hash(key));
}

ObjectData* RefsMap::Remove(const Address& key) {
  return UnderlyingMap::Remove(key, RefsMap::Hash(key));
}

uint32_t RefsMap::Hash(Address addr) {
  // Don't use a plain cast: keys are handle locations, which are
  // pointer-aligned and consecutive within a handle block, so they would
  // fill every eighth bucket in a contiguous run and linear probing would
  // degrade to a scan.
  return static_cast<uint32_t>(base::hash64(addr));
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
