// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/read-only-deserializer.h"

#include "src/api/api.h"
#include "src/execution/v8threads.h"
#include "src/heap/heap-inl.h"  // crbug.com/v8/8499
#include "src/heap/read-only-heap.h"
#include "src/objects/slots.h"
#include "src/snapshot/snapshot.h"

namespace v8 {
namespace internal {

void ReadOnlyDeserializer::DeserializeIntoIsolate() {
  HandleScope scope(isolate());

  ReadOnlyHeap* ro_heap = isolate()->read_only_heap();

  // No active threads.
  DCHECK_NULL(isolate()->thread_manager()->FirstThreadStateInUse());
  // No active handles.
  DCHECK(isolate()->handle_scope_implementer()->blocks()->empty());
  // Read-only object cache is not yet populated.
  DCHECK(!ro_heap->read_only_object_cache_is_initialized());
  // Startup object cache is not yet populated.
  DCHECK(isolate()->startup_object_cache()->empty());
  // Builtins are not yet created.
  DCHECK(!isolate()->builtins()->is_initialized());

  {
    ReadOnlyRoots roots(isolate());

    roots.Iterate(this);
    ro_heap->read_only_space()->RepairFreeSpacesAfterDeserialization();

    // Deserialize the Read-only Object Cache.
    for (size_t i = 0;; ++i) {
      Object* object = ro_heap->ExtendReadOnlyObjectCache();
      // During deserialization, the visitor populates the read-only object
      // cache and eventually terminates the cache with undefined.
      VisitRootPointer(Root::kReadOnlyObjectCache, nullptr,
                       FullObjectSlot(object));
      if (object->IsUndefined(roots)) break;
    }
    DeserializeDeferredObjects();
    CheckNoArrayBufferBackingStores();
  }

  if (FLAG_rehash_snapshot && can_rehash()) {
    isolate()->heap()->InitializeHashSeed();
    Rehash();
  }
}

}  // namespace internal
}  // namespace v8
