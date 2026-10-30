// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/object-deserializer.h"

#include "src/execution/isolate.h"
#include "src/heap/heap-inl.h"
#include "src/heap/local-factory-inl.h"
#include "src/objects/allocation-site-inl.h"
#include "src/objects/allocation-site.h"
#include "src/objects/bytecode-array-inl.h"
#include "src/objects/objects.h"
#include "src/objects/trusted-object-inl.h"
#include "src/sandbox/bytecode-verifier.h"
#include "src/snapshot/code-serializer.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {

ObjectDeserializer::ObjectDeserializer(Isolate* isolate,
                                       const SerializedCodeData* data)
    : Deserializer(isolate, data->UntrustedPayload(), data->TrustedPayload(),
                   data->GetMagicNumber(), /* deserializing_user_code */ true,
                   /* can_rehash */ false) {}

MaybeDirectHandle<SharedFunctionInfo>
ObjectDeserializer::DeserializeSharedFunctionInfo(
    Isolate* isolate, const SerializedCodeData* data,
    DirectHandle<String> source) {
  ObjectDeserializer d(isolate, data);

  d.AddAttachedObject(source);

  DirectHandle<HeapObject> result;
  return d.Deserialize().ToHandle(&result)
             ? Cast<SharedFunctionInfo>(result)
             : MaybeDirectHandle<SharedFunctionInfo>();
}

MaybeDirectHandle<HeapObject> ObjectDeserializer::Deserialize() {
  DCHECK(deserializing_user_code());
  HandleScope scope(isolate());
  DirectHandle<HeapObject> result;
  {
    result = DeserializeUntrustedSection();
    DeserializeTrustedSection();
    CHECK(new_instruction_stream_objects().empty());
    CHECK(new_maps().empty());
  }

  Rehash();
  CommitPostProcessedObjects();
  return scope.CloseAndEscape(result);
}

void ObjectDeserializer::CommitPostProcessedObjects() {
  for (DirectHandle<Script> script : new_scripts()) {
    // Assign a new script id to avoid collision.
    script->set_id(isolate()->GetNextScriptId());
    LogScriptEvents(*script);
    // Add script to list.
    Handle<WeakArrayList> list = isolate()->factory()->script_list();
    list = WeakArrayList::AddToEnd(isolate(), list,
                                   MaybeObjectDirectHandle::Weak(script));
    isolate()->heap()->SetRootScriptList(*list);
  }
  PostProcessExposedTrustedObjects();
}

OffThreadObjectDeserializer::OffThreadObjectDeserializer(
    LocalIsolate* isolate, const SerializedCodeData* data)
    : Deserializer(isolate, data->UntrustedPayload(), data->TrustedPayload(),
                   data->GetMagicNumber(), /* deserializing_user_code */ true,
                   /* can_rehash */ false) {}

MaybeDirectHandle<SharedFunctionInfo>
OffThreadObjectDeserializer::DeserializeSharedFunctionInfo(
    LocalIsolate* isolate, const SerializedCodeData* data,
    std::vector<IndirectHandle<Script>>* deserialized_scripts) {
  OffThreadObjectDeserializer d(isolate, data);

  // Attach the empty string as the source.
  d.AddAttachedObject(isolate->factory()->empty_string());

  DirectHandle<HeapObject> result;
  if (!d.Deserialize(deserialized_scripts).ToHandle(&result)) {
    return MaybeDirectHandle<SharedFunctionInfo>();
  }
  return Cast<SharedFunctionInfo>(result);
}

MaybeDirectHandle<HeapObject> OffThreadObjectDeserializer::Deserialize(
    std::vector<IndirectHandle<Script>>* deserialized_scripts) {
  DCHECK(deserializing_user_code());
  LocalHandleScope scope(isolate());
  DirectHandle<HeapObject> result;
  {
    result = DeserializeUntrustedSection();
    DeserializeTrustedSection();
    CHECK(new_instruction_stream_objects().empty());
    CHECK(new_maps().empty());
  }

  Rehash();
  CommitPostProcessedObjects(deserialized_scripts);
  return scope.CloseAndEscape(result);
}

void OffThreadObjectDeserializer::CommitPostProcessedObjects(
    std::vector<IndirectHandle<Script>>* deserialized_scripts) {
  // TODO(leszeks): Figure out a better way of dealing with scripts.
  CHECK_EQ(new_scripts().size(), 1);
  for (DirectHandle<Script> script : new_scripts()) {
    // Assign a new script id to avoid collision.
    script->set_id(isolate()->GetNextScriptId());
    LogScriptEvents(*script);
    deserialized_scripts->push_back(
        isolate()->heap()->NewPersistentHandle(script));
  }
  PostProcessExposedTrustedObjects();
}

}  // namespace internal
}  // namespace v8
