// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/object-deserializer.h"

#include "src/codegen/assembler-inl.h"
#include "src/execution/isolate.h"
#include "src/heap/heap-inl.h"
#include "src/objects/allocation-site-inl.h"
#include "src/objects/objects.h"
#include "src/objects/slots.h"
#include "src/snapshot/code-serializer.h"

namespace v8 {
namespace internal {

ObjectDeserializer::ObjectDeserializer(Isolate* isolate,
                                       const SerializedCodeData* data)
    : Deserializer(isolate, data->Payload(), data->GetMagicNumber(), true,
                   false) {}

MaybeHandle<SharedFunctionInfo>
ObjectDeserializer::DeserializeSharedFunctionInfo(
    Isolate* isolate, const SerializedCodeData* data, Handle<String> source) {
  ObjectDeserializer d(isolate, data);

  d.AddAttachedObject(source);

  Handle<HeapObject> result;
  return d.Deserialize().ToHandle(&result)
             ? Handle<SharedFunctionInfo>::cast(result)
             : MaybeHandle<SharedFunctionInfo>();
}

MaybeHandle<SharedFunctionInfo>
ObjectDeserializer::DeserializeSharedFunctionInfoOffThread(
    LocalIsolate* isolate, const SerializedCodeData* data,
    Handle<String> source) {
  // TODO(leszeks): Add LocalHeap support to deserializer
  UNREACHABLE();
}

MaybeHandle<HeapObject> ObjectDeserializer::Deserialize() {
  DCHECK(deserializing_user_code());
  HandleScope scope(isolate());
  Handle<HeapObject> result;
  {
    result = ReadObject();
    DeserializeDeferredObjects();
    CHECK(new_code_objects().empty());
    LinkAllocationSites();
    CHECK(new_maps().empty());
    WeakenDescriptorArrays();
  }

  Rehash();
  CommitPostProcessedObjects();
  return scope.CloseAndEscape(result);
}

void ObjectDeserializer::CommitPostProcessedObjects() {
  for (Handle<JSArrayBuffer> buffer : new_off_heap_array_buffers()) {
    uint32_t store_index = buffer->GetBackingStoreRefForDeserialization();
    auto bs = backing_store(store_index);
    SharedFlag shared =
        bs && bs->is_shared() ? SharedFlag::kShared : SharedFlag::kNotShared;
    buffer->Setup(shared, bs);
  }

  for (Handle<Script> script : new_scripts()) {
    // Assign a new script id to avoid collision.
    script->set_id(isolate()->GetNextScriptId());
    LogScriptEvents(*script);
    // Add script to list.
    Handle<WeakArrayList> list = isolate()->factory()->script_list();
    list = WeakArrayList::AddToEnd(isolate(), list,
                                   MaybeObjectHandle::Weak(script));
    isolate()->heap()->SetRootScriptList(*list);
  }
}

void ObjectDeserializer::LinkAllocationSites() {
  DisallowGarbageCollection no_gc;
  Heap* heap = isolate()->heap();
  // Allocation sites are present in the snapshot, and must be linked into
  // a list at deserialization time.
  for (Handle<AllocationSite> site : new_allocation_sites()) {
    if (!site->HasWeakNext()) continue;
    // TODO(mvstanton): consider treating the heap()->allocation_sites_list()
    // as a (weak) root. If this root is relocated correctly, this becomes
    // unnecessary.
    if (heap->allocation_sites_list() == Smi::zero()) {
      site->set_weak_next(ReadOnlyRoots(heap).undefined_value());
    } else {
      site->set_weak_next(heap->allocation_sites_list());
    }
    heap->set_allocation_sites_list(*site);
  }
}

}  // namespace internal
}  // namespace v8
