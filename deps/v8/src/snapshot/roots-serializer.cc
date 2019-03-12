// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/roots-serializer.h"

#include "src/heap/heap.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/objects/slots.h"

namespace v8 {
namespace internal {

RootsSerializer::RootsSerializer(Isolate* isolate,
                                 RootIndex first_root_to_be_serialized)
    : Serializer(isolate),
      first_root_to_be_serialized_(first_root_to_be_serialized),
      can_be_rehashed_(true) {
  for (size_t i = 0; i < static_cast<size_t>(first_root_to_be_serialized);
       ++i) {
    root_has_been_serialized_[i] = true;
  }
}

int RootsSerializer::SerializeInObjectCache(HeapObject heap_object) {
  int index;
  if (!object_cache_index_map_.LookupOrInsert(heap_object, &index)) {
    // This object is not part of the object cache yet. Add it to the cache so
    // we can refer to it via cache index from the delegating snapshot.
    SerializeObject(heap_object, kPlain, kStartOfObject, 0);
  }
  return index;
}

void RootsSerializer::Synchronize(VisitorSynchronization::SyncTag tag) {
  sink_.Put(kSynchronize, "Synchronize");
}

void RootsSerializer::VisitRootPointers(Root root, const char* description,
                                        FullObjectSlot start,
                                        FullObjectSlot end) {
  RootsTable& roots_table = isolate()->heap()->roots_table();
  if (start ==
      roots_table.begin() + static_cast<int>(first_root_to_be_serialized_)) {
    // Serializing the root list needs special handling:
    // - Only root list elements that have been fully serialized can be
    //   referenced using kRootArray bytecodes.
    for (FullObjectSlot current = start; current < end; ++current) {
      SerializeRootObject(*current);
      size_t root_index = current - roots_table.begin();
      root_has_been_serialized_.set(root_index);
    }
  } else {
    Serializer::VisitRootPointers(root, description, start, end);
  }
}

void RootsSerializer::CheckRehashability(HeapObject obj) {
  if (!can_be_rehashed_) return;
  if (!obj->NeedsRehashing()) return;
  if (obj->CanBeRehashed()) return;
  can_be_rehashed_ = false;
}

}  // namespace internal
}  // namespace v8
