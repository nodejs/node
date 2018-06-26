// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/object-deserializer.h"

#include "src/assembler-inl.h"
#include "src/code-stubs.h"
#include "src/isolate.h"
#include "src/objects.h"
#include "src/snapshot/code-serializer.h"
#include "src/wasm/wasm-objects.h"

namespace v8 {
namespace internal {

MaybeHandle<SharedFunctionInfo>
ObjectDeserializer::DeserializeSharedFunctionInfo(
    Isolate* isolate, const SerializedCodeData* data, Handle<String> source) {
  ObjectDeserializer d(data);

  d.AddAttachedObject(source);

  Vector<const uint32_t> code_stub_keys = data->CodeStubKeys();
  for (int i = 0; i < code_stub_keys.length(); i++) {
    d.AddAttachedObject(
        CodeStub::GetCode(isolate, code_stub_keys[i]).ToHandleChecked());
  }

  Handle<HeapObject> result;
  return d.Deserialize(isolate).ToHandle(&result)
             ? Handle<SharedFunctionInfo>::cast(result)
             : MaybeHandle<SharedFunctionInfo>();
}

MaybeHandle<WasmCompiledModule>
ObjectDeserializer::DeserializeWasmCompiledModule(
    Isolate* isolate, const SerializedCodeData* data,
    Vector<const byte> wire_bytes) {
  ObjectDeserializer d(data);

  d.AddAttachedObject(isolate->native_context());

  MaybeHandle<String> maybe_wire_bytes_as_string =
      isolate->factory()->NewStringFromOneByte(wire_bytes, TENURED);
  Handle<String> wire_bytes_as_string;
  if (!maybe_wire_bytes_as_string.ToHandle(&wire_bytes_as_string)) {
    return MaybeHandle<WasmCompiledModule>();
  }
  d.AddAttachedObject(wire_bytes_as_string);

  Vector<const uint32_t> code_stub_keys = data->CodeStubKeys();
  for (int i = 0; i < code_stub_keys.length(); i++) {
    d.AddAttachedObject(
        CodeStub::GetCode(isolate, code_stub_keys[i]).ToHandleChecked());
  }

  Handle<HeapObject> result;
  if (!d.Deserialize(isolate).ToHandle(&result))
    return MaybeHandle<WasmCompiledModule>();

  if (!result->IsWasmCompiledModule()) return MaybeHandle<WasmCompiledModule>();

  // Cast without type checks, as the module wrapper is not there yet.
  return handle(static_cast<WasmCompiledModule*>(*result), isolate);
}

MaybeHandle<HeapObject> ObjectDeserializer::Deserialize(Isolate* isolate) {
  Initialize(isolate);

  if (!allocator()->ReserveSpace()) return MaybeHandle<HeapObject>();

  DCHECK(deserializing_user_code());
  HandleScope scope(isolate);
  Handle<HeapObject> result;
  {
    DisallowHeapAllocation no_gc;
    Object* root;
    VisitRootPointer(Root::kPartialSnapshotCache, nullptr, &root);
    DeserializeDeferredObjects();
    FlushICacheForNewCodeObjectsAndRecordEmbeddedObjects();
    result = Handle<HeapObject>(HeapObject::cast(root));
    Rehash();
    allocator()->RegisterDeserializedObjectsForBlackAllocation();
  }
  CommitPostProcessedObjects();
  return scope.CloseAndEscape(result);
}

void ObjectDeserializer::
    FlushICacheForNewCodeObjectsAndRecordEmbeddedObjects() {
  DCHECK(deserializing_user_code());
  for (Code* code : new_code_objects()) {
    // Record all references to embedded objects in the new code object.
    isolate()->heap()->RecordWritesIntoCode(code);
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
    DCHECK_NULL(StringTable::ForwardStringIfExists(isolate(), &key, *string));
    StringTable::AddKeyNoResize(isolate(), &key);
  }

  Heap* heap = isolate()->heap();
  Factory* factory = isolate()->factory();
  for (Handle<Script> script : new_scripts()) {
    // Assign a new script id to avoid collision.
    script->set_id(isolate()->heap()->NextScriptId());
    // Add script to list.
    Handle<Object> list =
        FixedArrayOfWeakCells::Add(factory->script_list(), script);
    heap->SetRootScriptList(*list);
  }
}

}  // namespace internal
}  // namespace v8
