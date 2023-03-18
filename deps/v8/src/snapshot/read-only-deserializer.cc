// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/read-only-deserializer.h"

#include "src/api/api.h"
#include "src/common/globals.h"
#include "src/execution/v8threads.h"
#include "src/heap/heap-inl.h"  // crbug.com/v8/8499
#include "src/heap/read-only-heap.h"
#include "src/objects/slots.h"
#include "src/roots/static-roots.h"

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
    if (V8_STATIC_ROOTS_BOOL) {
      ro_heap->read_only_space()->InitFromMemoryDump(isolate(), source());
      roots.InitFromStaticRootsTable(isolate()->cage_base());
      ro_heap->read_only_space()->RepairFreeSpacesAfterDeserialization();
    } else {
      roots.Iterate(this);

      // Deserialize the Read-only Object Cache.
      for (;;) {
        Object* object = ro_heap->ExtendReadOnlyObjectCache();
        // During deserialization, the visitor populates the read-only object
        // cache and eventually terminates the cache with undefined.
        VisitRootPointer(Root::kReadOnlyObjectCache, nullptr,
                         FullObjectSlot(object));
        if (object->IsUndefined(roots)) break;
      }
      DeserializeDeferredObjects();
    }

#ifdef DEBUG
    roots.VerifyNameForProtectors();
#endif
    roots.VerifyNameForProtectorsPages();
  }

  if (should_rehash()) {
    isolate()->heap()->InitializeHashSeed();
    RehashReadOnly();
  }
}

void ReadOnlyDeserializer::RehashReadOnly() {
  DCHECK(should_rehash());
  if (V8_STATIC_ROOTS_BOOL) {
    // Since we are not deserializing individual objects we need to scan the
    // heap and search for the ones that need rehashing.
    ReadOnlyHeapObjectIterator iterator(isolate()->read_only_heap());
    PtrComprCageBase cage_base(isolate());
    for (HeapObject object = iterator.Next(); !object.is_null();
         object = iterator.Next()) {
      auto instance_type = object.map(cage_base).instance_type();
      if (InstanceTypeChecker::IsInternalizedString(instance_type)) {
        auto str = String::cast(object);
        str.set_raw_hash_field(Name::kEmptyHashField);
        str.EnsureHash();
      } else if (object.NeedsRehashing(instance_type)) {
        object.RehashBasedOnMap(isolate());
      }
    }
  } else {
    Rehash();
  }
}

}  // namespace internal
}  // namespace v8
