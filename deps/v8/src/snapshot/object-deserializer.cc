// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/object-deserializer.h"

#include "src/assembler-inl.h"
#include "src/isolate.h"
#include "src/objects.h"
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
  return d.Deserialize(isolate).ToHandle(&result)
             ? Handle<SharedFunctionInfo>::cast(result)
             : MaybeHandle<SharedFunctionInfo>();
}

MaybeHandle<HeapObject> ObjectDeserializer::Deserialize(Isolate* isolate) {
  Initialize(isolate);

  if (!allocator()->ReserveSpace()) return MaybeHandle<HeapObject>();

  DCHECK(deserializing_user_code());
  HandleScope scope(isolate);
  Handle<HeapObject> result;
  {
    DisallowHeapAllocation no_gc;
    Object root;
    VisitRootPointer(Root::kPartialSnapshotCache, nullptr,
                     FullObjectSlot(&root));
    DeserializeDeferredObjects();
    FlushICache();
    LinkAllocationSites();
    LogNewMapEvents();
    result = handle(HeapObject::cast(root), isolate);
    Rehash();
    allocator()->RegisterDeserializedObjectsForBlackAllocation();
  }
  CommitPostProcessedObjects();
  return scope.CloseAndEscape(result);
}

void ObjectDeserializer::FlushICache() {
  DCHECK(deserializing_user_code());
  for (Code code : new_code_objects()) {
    // Record all references to embedded objects in the new code object.
    WriteBarrierForCode(code);
    Assembler::FlushICache(code->raw_instruction_start(),
                           code->raw_instruction_size());
  }
}

void ObjectDeserializer::CommitPostProcessedObjects() {
  CHECK_LE(new_internalized_strings().size(), kMaxInt);
  StringTable::EnsureCapacityForDeserialization(
      isolate(), static_cast<int>(new_internalized_strings().size()));
  for (Handle<String> string : new_internalized_strings()) {
    DisallowHeapAllocation no_gc;
    StringTableInsertionKey key(*string);
    DCHECK(
        StringTable::ForwardStringIfExists(isolate(), &key, *string).is_null());
    StringTable::AddKeyNoResize(isolate(), &key);
  }

  Heap* heap = isolate()->heap();
  Factory* factory = isolate()->factory();
  for (Handle<Script> script : new_scripts()) {
    // Assign a new script id to avoid collision.
    script->set_id(isolate()->heap()->NextScriptId());
    LogScriptEvents(*script);
    // Add script to list.
    Handle<WeakArrayList> list = factory->script_list();
    list = WeakArrayList::AddToEnd(isolate(), list,
                                   MaybeObjectHandle::Weak(script));
    heap->SetRootScriptList(*list);
  }
}

void ObjectDeserializer::LinkAllocationSites() {
  DisallowHeapAllocation no_gc;
  Heap* heap = isolate()->heap();
  // Allocation sites are present in the snapshot, and must be linked into
  // a list at deserialization time.
  for (AllocationSite site : new_allocation_sites()) {
    if (!site->HasWeakNext()) continue;
    // TODO(mvstanton): consider treating the heap()->allocation_sites_list()
    // as a (weak) root. If this root is relocated correctly, this becomes
    // unnecessary.
    if (heap->allocation_sites_list() == Smi::kZero) {
      site->set_weak_next(ReadOnlyRoots(heap).undefined_value());
    } else {
      site->set_weak_next(heap->allocation_sites_list());
    }
    heap->set_allocation_sites_list(site);
  }
}

}  // namespace internal
}  // namespace v8
