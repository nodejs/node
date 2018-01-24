// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_HASH_TABLE_INL_H_
#define V8_OBJECTS_HASH_TABLE_INL_H_

#include "src/heap/heap.h"
#include "src/objects/hash-table.h"

namespace v8 {
namespace internal {

template <typename KeyT>
bool BaseShape<KeyT>::IsLive(Isolate* isolate, Object* k) {
  Heap* heap = isolate->heap();
  return k != heap->the_hole_value() && k != heap->undefined_value();
}

int OrderedHashSet::GetMapRootIndex() {
  return Heap::kOrderedHashSetMapRootIndex;
}

int OrderedHashMap::GetMapRootIndex() {
  return Heap::kOrderedHashMapMapRootIndex;
}

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_HASH_TABLE_INL_H_
