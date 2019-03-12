// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/startup-deserializer.h"

#include "src/api.h"
#include "src/assembler-inl.h"
#include "src/heap/heap-inl.h"
#include "src/snapshot/read-only-deserializer.h"
#include "src/snapshot/snapshot.h"

namespace v8 {
namespace internal {

void StartupDeserializer::DeserializeInto(Isolate* isolate) {
  Initialize(isolate);

  ReadOnlyDeserializer read_only_deserializer(read_only_data_);
  read_only_deserializer.SetRehashability(can_rehash());
  read_only_deserializer.DeserializeInto(isolate);

  if (!allocator()->ReserveSpace()) {
    V8::FatalProcessOutOfMemory(isolate, "StartupDeserializer");
  }

  // No active threads.
  DCHECK_NULL(isolate->thread_manager()->FirstThreadStateInUse());
  // No active handles.
  DCHECK(isolate->handle_scope_implementer()->blocks()->empty());
  // Partial snapshot cache is not yet populated.
  DCHECK(isolate->partial_snapshot_cache()->empty());
  // Builtins are not yet created.
  DCHECK(!isolate->builtins()->is_initialized());

  {
    DisallowHeapAllocation no_gc;
    isolate->heap()->IterateSmiRoots(this);
    isolate->heap()->IterateStrongRoots(this, VISIT_FOR_SERIALIZATION);
    Iterate(isolate, this);
    isolate->heap()->IterateWeakRoots(this, VISIT_FOR_SERIALIZATION);
    DeserializeDeferredObjects();
    RestoreExternalReferenceRedirectors(accessor_infos());
    RestoreExternalReferenceRedirectors(call_handler_infos());

    // Flush the instruction cache for the entire code-space. Must happen after
    // builtins deserialization.
    FlushICache();
  }

  isolate->heap()->set_native_contexts_list(
      ReadOnlyRoots(isolate).undefined_value());
  // The allocation site list is build during root iteration, but if no sites
  // were encountered then it needs to be initialized to undefined.
  if (isolate->heap()->allocation_sites_list() == Smi::kZero) {
    isolate->heap()->set_allocation_sites_list(
        ReadOnlyRoots(isolate).undefined_value());
  }


  isolate->builtins()->MarkInitialized();

  LogNewMapEvents();

  if (FLAG_rehash_snapshot && can_rehash()) {
    isolate->heap()->InitializeHashSeed();
    read_only_deserializer.RehashHeap();
    Rehash();
  }
}

void StartupDeserializer::LogNewMapEvents() {
  if (FLAG_trace_maps) LOG(isolate_, LogAllMaps());
}

void StartupDeserializer::FlushICache() {
  DCHECK(!deserializing_user_code());
  // The entire isolate is newly deserialized. Simply flush all code pages.
  for (Page* p : *isolate()->heap()->code_space()) {
    Assembler::FlushICache(p->area_start(), p->area_end() - p->area_start());
  }
}

}  // namespace internal
}  // namespace v8
