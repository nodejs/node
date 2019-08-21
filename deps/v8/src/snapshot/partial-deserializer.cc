// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/partial-deserializer.h"

#include "src/api/api-inl.h"
#include "src/heap/heap-inl.h"
#include "src/objects/slots.h"
#include "src/snapshot/snapshot.h"

namespace v8 {
namespace internal {

MaybeHandle<Context> PartialDeserializer::DeserializeContext(
    Isolate* isolate, const SnapshotData* data, bool can_rehash,
    Handle<JSGlobalProxy> global_proxy,
    v8::DeserializeEmbedderFieldsCallback embedder_fields_deserializer) {
  PartialDeserializer d(data);
  d.SetRehashability(can_rehash);

  MaybeHandle<Object> maybe_result =
      d.Deserialize(isolate, global_proxy, embedder_fields_deserializer);

  Handle<Object> result;
  return maybe_result.ToHandle(&result) ? Handle<Context>::cast(result)
                                        : MaybeHandle<Context>();
}

MaybeHandle<Object> PartialDeserializer::Deserialize(
    Isolate* isolate, Handle<JSGlobalProxy> global_proxy,
    v8::DeserializeEmbedderFieldsCallback embedder_fields_deserializer) {
  Initialize(isolate);
  if (!allocator()->ReserveSpace()) {
    V8::FatalProcessOutOfMemory(isolate, "PartialDeserializer");
  }

  AddAttachedObject(global_proxy);

  DisallowHeapAllocation no_gc;
  // Keep track of the code space start and end pointers in case new
  // code objects were unserialized
  CodeSpace* code_space = isolate->heap()->code_space();
  Address start_address = code_space->top();
  Object root;
  VisitRootPointer(Root::kPartialSnapshotCache, nullptr, FullObjectSlot(&root));
  DeserializeDeferredObjects();
  DeserializeEmbedderFields(embedder_fields_deserializer);

  allocator()->RegisterDeserializedObjectsForBlackAllocation();

  // There's no code deserialized here. If this assert fires then that's
  // changed and logging should be added to notify the profiler et al of the
  // new code, which also has to be flushed from instruction cache.
  CHECK_EQ(start_address, code_space->top());

  if (FLAG_rehash_snapshot && can_rehash()) Rehash();
  LogNewMapEvents();

  return Handle<Object>(root, isolate);
}

void PartialDeserializer::DeserializeEmbedderFields(
    v8::DeserializeEmbedderFieldsCallback embedder_fields_deserializer) {
  if (!source()->HasMore() || source()->Get() != kEmbedderFieldsData) return;
  DisallowHeapAllocation no_gc;
  DisallowJavascriptExecution no_js(isolate());
  DisallowCompilation no_compile(isolate());
  DCHECK_NOT_NULL(embedder_fields_deserializer.callback);
  for (int code = source()->Get(); code != kSynchronize;
       code = source()->Get()) {
    HandleScope scope(isolate());
    int space = code & kSpaceMask;
    DCHECK_LE(space, kNumberOfSpaces);
    DCHECK_EQ(code - space, kNewObject);
    Handle<JSObject> obj(JSObject::cast(GetBackReferencedObject(
                             static_cast<SnapshotSpace>(space))),
                         isolate());
    int index = source()->GetInt();
    int size = source()->GetInt();
    // TODO(yangguo,jgruber): Turn this into a reusable shared buffer.
    byte* data = new byte[size];
    source()->CopyRaw(data, size);
    embedder_fields_deserializer.callback(v8::Utils::ToLocal(obj), index,
                                          {reinterpret_cast<char*>(data), size},
                                          embedder_fields_deserializer.data);
    delete[] data;
  }
}
}  // namespace internal
}  // namespace v8
