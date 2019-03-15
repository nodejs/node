// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/read-only-deserializer.h"

#include "src/api.h"
#include "src/heap/heap-inl.h"  // crbug.com/v8/8499
#include "src/heap/read-only-heap.h"
#include "src/objects/slots.h"
#include "src/snapshot/snapshot.h"
#include "src/v8threads.h"

namespace v8 {
namespace internal {

void ReadOnlyDeserializer::DeserializeInto(Isolate* isolate) {
  Initialize(isolate);

  if (!allocator()->ReserveSpace()) {
    V8::FatalProcessOutOfMemory(isolate, "ReadOnlyDeserializer");
  }

  // No active threads.
  DCHECK_NULL(isolate->thread_manager()->FirstThreadStateInUse());
  // No active handles.
  DCHECK(isolate->handle_scope_implementer()->blocks()->empty());
  // Partial snapshot cache is not yet populated.
  DCHECK(isolate->heap()->read_only_heap()->read_only_object_cache()->empty());
  DCHECK(isolate->partial_snapshot_cache()->empty());
  // Builtins are not yet created.
  DCHECK(!isolate->builtins()->is_initialized());

  {
    DisallowHeapAllocation no_gc;
    ReadOnlyRoots roots(isolate);

    roots.Iterate(this);
    isolate->heap()
        ->read_only_heap()
        ->read_only_space()
        ->RepairFreeListsAfterDeserialization();

    // Deserialize the Read-only Object Cache.
    std::vector<Object>* cache =
        isolate->heap()->read_only_heap()->read_only_object_cache();
    for (size_t i = 0;; ++i) {
      // Extend the array ready to get a value when deserializing.
      if (cache->size() <= i) cache->push_back(Smi::kZero);
      // During deserialization, the visitor populates the read-only object
      // cache and eventually terminates the cache with undefined.
      VisitRootPointer(Root::kReadOnlyObjectCache, nullptr,
                       FullObjectSlot(&cache->at(i)));
      if (cache->at(i)->IsUndefined(roots)) break;
    }
    DeserializeDeferredObjects();
  }

  if (FLAG_rehash_snapshot && can_rehash()) {
    isolate_->heap()->InitializeHashSeed();
    Rehash();
  }
}

}  // namespace internal
}  // namespace v8
