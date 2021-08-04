// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/context-deserializer.h"

#include "src/api/api-inl.h"
#include "src/common/assert-scope.h"
#include "src/heap/heap-inl.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/slots.h"
#include "src/snapshot/snapshot.h"

namespace v8 {
namespace internal {

MaybeHandle<Context> ContextDeserializer::DeserializeContext(
    Isolate* isolate, const SnapshotData* data, bool can_rehash,
    Handle<JSGlobalProxy> global_proxy,
    v8::DeserializeEmbedderFieldsCallback embedder_fields_deserializer) {
  ContextDeserializer d(isolate, data, can_rehash);

  MaybeHandle<Object> maybe_result =
      d.Deserialize(isolate, global_proxy, embedder_fields_deserializer);

  Handle<Object> result;
  return maybe_result.ToHandle(&result) ? Handle<Context>::cast(result)
                                        : MaybeHandle<Context>();
}

MaybeHandle<Object> ContextDeserializer::Deserialize(
    Isolate* isolate, Handle<JSGlobalProxy> global_proxy,
    v8::DeserializeEmbedderFieldsCallback embedder_fields_deserializer) {
  // Replace serialized references to the global proxy and its map with the
  // given global proxy and its map.
  AddAttachedObject(global_proxy);
  AddAttachedObject(handle(global_proxy->map(), isolate));

  Handle<Object> result;
  {
    // There's no code deserialized here. If this assert fires then that's
    // changed and logging should be added to notify the profiler et al. of
    // the new code, which also has to be flushed from instruction cache.
    DisallowCodeAllocation no_code_allocation;

    result = ReadObject();
    DeserializeDeferredObjects();
    DeserializeEmbedderFields(embedder_fields_deserializer);

    LogNewMapEvents();
    WeakenDescriptorArrays();
  }

  if (FLAG_rehash_snapshot && can_rehash()) Rehash();
  SetupOffHeapArrayBufferBackingStores();

  return result;
}

void ContextDeserializer::SetupOffHeapArrayBufferBackingStores() {
  for (Handle<JSArrayBuffer> buffer : new_off_heap_array_buffers()) {
    uint32_t store_index = buffer->GetBackingStoreRefForDeserialization();
    auto bs = backing_store(store_index);
    buffer->AllocateExternalPointerEntries(isolate());
    // TODO(v8:11111): Support RAB / GSAB.
    CHECK(!buffer->is_resizable());
    SharedFlag shared =
        bs && bs->is_shared() ? SharedFlag::kShared : SharedFlag::kNotShared;
    buffer->Setup(shared, ResizableFlag::kNotResizable, bs);
  }
}

void ContextDeserializer::DeserializeEmbedderFields(
    v8::DeserializeEmbedderFieldsCallback embedder_fields_deserializer) {
  if (!source()->HasMore() || source()->Get() != kEmbedderFieldsData) return;
  DisallowGarbageCollection no_gc;
  DisallowJavascriptExecution no_js(isolate());
  DisallowCompilation no_compile(isolate());
  DCHECK_NOT_NULL(embedder_fields_deserializer.callback);
  for (int code = source()->Get(); code != kSynchronize;
       code = source()->Get()) {
    HandleScope scope(isolate());
    Handle<JSObject> obj = Handle<JSObject>::cast(GetBackReferencedObject());
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
