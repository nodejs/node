// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/refs-map.h"

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

RefsMap::Entry* RefsMap::LookupOrInsert(const Address& key, Zone* zone) {
  return UnderlyingMap::LookupOrInsert(key, RefsMap::Hash(key),
                                       []() { return nullptr; },
                                       ZoneAllocationPolicy(zone));
}

uint32_t RefsMap::Hash(Address addr) { return static_cast<uint32_t>(addr); }

}  // namespace compiler
}  // namespace internal
}  // namespace v8
