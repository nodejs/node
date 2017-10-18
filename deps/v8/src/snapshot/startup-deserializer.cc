// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/startup-deserializer.h"

#include "src/assembler-inl.h"
#include "src/heap/heap-inl.h"
#include "src/snapshot/snapshot.h"

namespace v8 {
namespace internal {

void StartupDeserializer::DeserializeInto(Isolate* isolate) {
  Initialize(isolate);
  if (!ReserveSpace()) V8::FatalProcessOutOfMemory("StartupDeserializer");

  // No active threads.
  DCHECK_NULL(isolate->thread_manager()->FirstThreadStateInUse());
  // No active handles.
  DCHECK(isolate->handle_scope_implementer()->blocks()->is_empty());
  // Partial snapshot cache is not yet populated.
  DCHECK(isolate->partial_snapshot_cache()->empty());
  // Builtins are not yet created.
  DCHECK(!isolate->builtins()->is_initialized());

  {
    DisallowHeapAllocation no_gc;
    isolate->heap()->IterateStrongRoots(this, VISIT_ONLY_STRONG_ROOT_LIST);
    isolate->heap()->IterateSmiRoots(this);
    isolate->heap()->IterateStrongRoots(this, VISIT_ONLY_STRONG);
    isolate->heap()->RepairFreeListsAfterDeserialization();
    isolate->heap()->IterateWeakRoots(this, VISIT_ALL);
    DeserializeDeferredObjects();
    FlushICacheForNewIsolate();
    RestoreExternalReferenceRedirectors(accessor_infos());
  }

  isolate->heap()->set_native_contexts_list(isolate->heap()->undefined_value());
  // The allocation site list is build during root iteration, but if no sites
  // were encountered then it needs to be initialized to undefined.
  if (isolate->heap()->allocation_sites_list() == Smi::kZero) {
    isolate->heap()->set_allocation_sites_list(
        isolate->heap()->undefined_value());
  }

  // Issue code events for newly deserialized code objects.
  LOG_CODE_EVENT(isolate, LogCodeObjects());
  LOG_CODE_EVENT(isolate, LogBytecodeHandlers());
  LOG_CODE_EVENT(isolate, LogCompiledFunctions());

  isolate->builtins()->MarkInitialized();

  // If needed, print the dissassembly of deserialized code objects.
  // Needs to be called after the builtins are marked as initialized, in order
  // to display the builtin names.
  PrintDisassembledCodeObjects();

  if (FLAG_rehash_snapshot && can_rehash()) Rehash();
}

void StartupDeserializer::FlushICacheForNewIsolate() {
  DCHECK(!deserializing_user_code());
  // The entire isolate is newly deserialized. Simply flush all code pages.
  for (Page* p : *isolate()->heap()->code_space()) {
    Assembler::FlushICache(isolate(), p->area_start(),
                           p->area_end() - p->area_start());
  }
}

void StartupDeserializer::PrintDisassembledCodeObjects() {
#ifdef ENABLE_DISASSEMBLER
  if (FLAG_print_builtin_code) {
    Heap* heap = isolate()->heap();
    HeapIterator iterator(heap);
    DisallowHeapAllocation no_gc;

    CodeTracer::Scope tracing_scope(isolate()->GetCodeTracer());
    OFStream os(tracing_scope.file());

    for (HeapObject* obj = iterator.next(); obj != NULL;
         obj = iterator.next()) {
      if (obj->IsCode()) {
        Code::cast(obj)->Disassemble(nullptr, os);
      }
    }
  }
#endif
}

void StartupDeserializer::Rehash() {
  DCHECK(FLAG_rehash_snapshot && can_rehash());
  isolate()->heap()->InitializeHashSeed();
  isolate()->heap()->string_table()->Rehash();
  isolate()->heap()->weak_object_to_code_table()->Rehash();
  SortMapDescriptors();
}

}  // namespace internal
}  // namespace v8
