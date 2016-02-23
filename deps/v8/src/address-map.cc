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
  if (map_ != NULL) return;
  map_ = new HashMap(HashMap::PointersMatch);
  for (uint32_t i = 0; i < Heap::kStrongRootListLength; i++) {
    Heap::RootListIndex root_index = static_cast<Heap::RootListIndex>(i);
    Object* root = isolate->heap()->root(root_index);
    // Omit root entries that can be written after initialization. They must
    // not be referenced through the root list in the snapshot.
    if (root->IsHeapObject() &&
        isolate->heap()->RootCanBeTreatedAsConstant(root_index)) {
      HeapObject* heap_object = HeapObject::cast(root);
      HashMap::Entry* entry = LookupEntry(map_, heap_object, false);
      if (entry != NULL) {
        // Some are initialized to a previous value in the root list.
        DCHECK_LT(GetValue(entry), i);
      } else {
        SetValue(LookupEntry(map_, heap_object, true), i);
      }
    }
  }
  isolate->set_root_index_map(map_);
}

}  // namespace internal
}  // namespace v8
