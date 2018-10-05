// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/address-map.h"
#include "src/heap/heap.h"
#include "src/isolate.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

RootIndexMap::RootIndexMap(Isolate* isolate) {
  map_ = isolate->root_index_map();
  if (map_ != nullptr) return;
  map_ = new HeapObjectToIndexHashMap();
  for (RootIndex root_index = RootIndex::kFirstStrongRoot;
       root_index <= RootIndex::kLastStrongRoot; ++root_index) {
    Object* root = isolate->heap()->root(root_index);
    if (!root->IsHeapObject()) continue;
    // Omit root entries that can be written after initialization. They must
    // not be referenced through the root list in the snapshot.
    // Since we map the raw address of an root item to its root list index, the
    // raw address must be constant, i.e. the object must be immovable.
    if (isolate->heap()->RootCanBeTreatedAsConstant(root_index)) {
      HeapObject* heap_object = HeapObject::cast(root);
      Maybe<uint32_t> maybe_index = map_->Get(heap_object);
      uint32_t index = static_cast<uint32_t>(root_index);
      if (maybe_index.IsJust()) {
        // Some are initialized to a previous value in the root list.
        DCHECK_LT(maybe_index.FromJust(), index);
      } else {
        map_->Set(heap_object, index);
      }
    } else {
      // Immortal immovable root objects are constant and allocated on the first
      // page of old space. Non-constant roots cannot be immortal immovable. The
      // root index map contains all immortal immmovable root objects.
      CHECK(!Heap::RootIsImmortalImmovable(root_index));
    }
  }
  isolate->set_root_index_map(map_);
}

}  // namespace internal
}  // namespace v8
