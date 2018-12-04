// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_ORDERED_HASH_TABLE_INL_H_
#define V8_OBJECTS_ORDERED_HASH_TABLE_INL_H_

#include "src/objects/ordered-hash-table.h"

#include "src/heap/heap.h"
#include "src/objects/fixed-array-inl.h"

namespace v8 {
namespace internal {

RootIndex OrderedHashSet::GetMapRootIndex() {
  return RootIndex::kOrderedHashSetMap;
}

RootIndex OrderedHashMap::GetMapRootIndex() {
  return RootIndex::kOrderedHashMapMap;
}

RootIndex SmallOrderedHashMap::GetMapRootIndex() {
  return RootIndex::kSmallOrderedHashMapMap;
}

RootIndex SmallOrderedHashSet::GetMapRootIndex() {
  return RootIndex::kSmallOrderedHashSetMap;
}

inline Object* OrderedHashMap::ValueAt(int entry) {
  DCHECK_LT(entry, this->UsedCapacity());
  return get(EntryToIndex(entry) + kValueOffset);
}

inline bool OrderedHashSet::Is(Handle<HeapObject> table) {
  return table->IsOrderedHashSet();
}

inline bool OrderedHashMap::Is(Handle<HeapObject> table) {
  return table->IsOrderedHashMap();
}

inline bool SmallOrderedHashSet::Is(Handle<HeapObject> table) {
  return table->IsSmallOrderedHashSet();
}

inline bool SmallOrderedHashMap::Is(Handle<HeapObject> table) {
  return table->IsSmallOrderedHashMap();
}
}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_ORDERED_HASH_TABLE_INL_H_
