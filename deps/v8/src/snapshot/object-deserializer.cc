// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/object-deserializer.h"

#include "src/codegen/assembler-inl.h"
#include "src/execution/isolate.h"
#include "src/execution/local-isolate-wrapper-inl.h"
#include "src/heap/heap-inl.h"
#include "src/objects/allocation-site-inl.h"
#include "src/objects/objects.h"
#include "src/objects/slots.h"
#include "src/snapshot/code-serializer.h"

namespace v8 {
namespace internal {

ObjectDeserializer::ObjectDeserializer(const SerializedCodeData* data)
    : Deserializer(data, true) {}

MaybeHandle<SharedFunctionInfo>
ObjectDeserializer::DeserializeSharedFunctionInfo(
    Isolate* isolate, const SerializedCodeData* data, Handle<String> source) {
  ObjectDeserializer d(data);

  d.AddAttachedObject(source);

  Handle<HeapObject> result;
  return d.Deserialize(LocalIsolateWrapper(isolate)).ToHandle(&result)
             ? Handle<SharedFunctionInfo>::cast(result)
             : MaybeHandle<SharedFunctionInfo>();
}

MaybeHandle<SharedFunctionInfo>
ObjectDeserializer::DeserializeSharedFunctionInfoOffThread(
    OffThreadIsolate* isolate, const SerializedCodeData* data,
    Handle<String> source) {
  DCHECK(ReadOnlyHeap::Contains(*source) || Heap::InOffThreadSpace(*source));

  ObjectDeserializer d(data);

  d.AddAttachedObject(source);

  Handle<HeapObject> result;
  return d.Deserialize(LocalIsolateWrapper(isolate)).ToHandle(&result)
             ? Handle<SharedFunctionInfo>::cast(result)
             : MaybeHandle<SharedFunctionInfo>();
}

MaybeHandle<HeapObject> ObjectDeserializer::Deserialize(
    LocalIsolateWrapper local_isolate) {
  Initialize(local_isolate);
  if (!allocator()->ReserveSpace()) return MaybeHandle<HeapObject>();

  DCHECK(deserializing_user_code());
  LocalHandleScopeWrapper scope(local_isolate);
  Handle<HeapObject> result;
  {
    DisallowHeapAllocation no_gc;
    Object root;
    VisitRootPointer(Root::kStartupObjectCache, nullptr, FullObjectSlot(&root));
    DeserializeDeferredObjects();
    CHECK(new_code_objects().empty());
    if (is_main_thread()) {
      LinkAllocationSites();
      LogNewMapEvents();
    }
    result = handle(HeapObject::cast(root), local_isolate);
    if (is_main_thread()) {
      allocator()->RegisterDeserializedObjectsForBlackAllocation();
    }
  }

  Rehash();
  CommitPostProcessedObjects();
  return scope.CloseAndEscape(result);
}

void ObjectDeserializer::CommitPostProcessedObjects() {
  if (is_main_thread()) {
    CHECK_LE(new_internalized_strings().size(), kMaxInt);
    StringTable::EnsureCapacityForDeserialization(
        isolate(), static_cast<int>(new_internalized_strings().size()));
    for (Handle<String> string : new_internalized_strings()) {
      DisallowHeapAllocation no_gc;
      StringTableInsertionKey key(*string);
      StringTable::AddKeyNoResize(isolate(), &key);
    }

    for (Handle<JSArrayBuffer> buffer : new_off_heap_array_buffers()) {
      uint32_t store_index = buffer->GetBackingStoreRefForDeserialization();
      auto bs = backing_store(store_index);
      SharedFlag shared =
          bs && bs->is_shared() ? SharedFlag::kShared : SharedFlag::kNotShared;
      buffer->Setup(shared, bs);
    }
  } else {
    CHECK_EQ(new_internalized_strings().size(), 0);
    CHECK_EQ(new_off_heap_array_buffers().size(), 0);
  }

  for (Handle<Script> script : new_scripts()) {
    // Assign a new script id to avoid collision.
    script->set_id(local_isolate()->GetNextScriptId());
    LogScriptEvents(*script);
    // Add script to list.
    if (is_main_thread()) {
      Handle<WeakArrayList> list = isolate()->factory()->script_list();
      list = WeakArrayList::AddToEnd(isolate(), list,
                                     MaybeObjectHandle::Weak(script));
      isolate()->heap()->SetRootScriptList(*list);
    } else {
      local_isolate().off_thread()->heap()->AddToScriptList(script);
    }
  }
}

void ObjectDeserializer::LinkAllocationSites() {
  DisallowHeapAllocation no_gc;
  Heap* heap = isolate()->heap();
  // Allocation sites are present in the snapshot, and must be linked into
  // a list at deserialization time.
  for (AllocationSite site : new_allocation_sites()) {
    if (!site.HasWeakNext()) continue;
    // TODO(mvstanton): consider treating the heap()->allocation_sites_list()
    // as a (weak) root. If this root is relocated correctly, this becomes
    // unnecessary.
    if (heap->allocation_sites_list() == Smi::zero()) {
      site.set_weak_next(ReadOnlyRoots(heap).undefined_value());
    } else {
      site.set_weak_next(heap->allocation_sites_list());
    }
    heap->set_allocation_sites_list(site);
  }
}

}  // namespace internal
}  // namespace v8
