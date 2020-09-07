// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/startup-deserializer.h"

#include "src/api/api.h"
#include "src/codegen/assembler-inl.h"
#include "src/execution/v8threads.h"
#include "src/heap/heap-inl.h"
#include "src/logging/log.h"
#include "src/snapshot/snapshot.h"

namespace v8 {
namespace internal {

void StartupDeserializer::DeserializeInto(Isolate* isolate) {
  Initialize(isolate);

  if (!allocator()->ReserveSpace()) {
    V8::FatalProcessOutOfMemory(isolate, "StartupDeserializer");
  }

  // No active threads.
  DCHECK_NULL(isolate->thread_manager()->FirstThreadStateInUse());
  // No active handles.
  DCHECK(isolate->handle_scope_implementer()->blocks()->empty());
  // Startup object cache is not yet populated.
  DCHECK(isolate->startup_object_cache()->empty());
  // Builtins are not yet created.
  DCHECK(!isolate->builtins()->is_initialized());

  {
    DisallowHeapAllocation no_gc;
    isolate->heap()->IterateSmiRoots(this);
    isolate->heap()->IterateRoots(
        this,
        base::EnumSet<SkipRoot>{SkipRoot::kUnserializable, SkipRoot::kWeak});
    Iterate(isolate, this);
    isolate->heap()->IterateWeakRoots(
        this, base::EnumSet<SkipRoot>{SkipRoot::kUnserializable});
    DeserializeDeferredObjects();
    RestoreExternalReferenceRedirectors(isolate, accessor_infos());
    RestoreExternalReferenceRedirectors(isolate, call_handler_infos());

    // Flush the instruction cache for the entire code-space. Must happen after
    // builtins deserialization.
    FlushICache();
  }

  CheckNoArrayBufferBackingStores();

  isolate->heap()->set_native_contexts_list(
      ReadOnlyRoots(isolate).undefined_value());
  // The allocation site list is build during root iteration, but if no sites
  // were encountered then it needs to be initialized to undefined.
  if (isolate->heap()->allocation_sites_list() == Smi::zero()) {
    isolate->heap()->set_allocation_sites_list(
        ReadOnlyRoots(isolate).undefined_value());
  }
  isolate->heap()->set_dirty_js_finalization_registries_list(
      ReadOnlyRoots(isolate).undefined_value());
  isolate->heap()->set_dirty_js_finalization_registries_list_tail(
      ReadOnlyRoots(isolate).undefined_value());

  isolate->builtins()->MarkInitialized();

  LogNewMapEvents();

  if (FLAG_rehash_snapshot && can_rehash()) {
    // Hash seed was initalized in ReadOnlyDeserializer.
    Rehash();
  }
}

void StartupDeserializer::LogNewMapEvents() {
  if (FLAG_trace_maps) LOG(isolate(), LogAllMaps());
}

void StartupDeserializer::FlushICache() {
  DCHECK(!deserializing_user_code());
  // The entire isolate is newly deserialized. Simply flush all code pages.
  for (Page* p : *isolate()->heap()->code_space()) {
    FlushInstructionCache(p->area_start(), p->area_end() - p->area_start());
  }
}

}  // namespace internal
}  // namespace v8
