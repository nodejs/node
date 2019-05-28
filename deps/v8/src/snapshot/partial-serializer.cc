// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/partial-serializer.h"
#include "src/snapshot/startup-serializer.h"

#include "src/api-inl.h"
#include "src/math-random.h"
#include "src/microtask-queue.h"
#include "src/objects-inl.h"
#include "src/objects/slots.h"

namespace v8 {
namespace internal {

PartialSerializer::PartialSerializer(
    Isolate* isolate, StartupSerializer* startup_serializer,
    v8::SerializeEmbedderFieldsCallback callback)
    : Serializer(isolate),
      startup_serializer_(startup_serializer),
      serialize_embedder_fields_(callback),
      can_be_rehashed_(true) {
  InitializeCodeAddressMap();
  allocator()->UseCustomChunkSize(FLAG_serialization_chunk_size);
}

PartialSerializer::~PartialSerializer() {
  OutputStatistics("PartialSerializer");
}

void PartialSerializer::Serialize(Context* o, bool include_global_proxy) {
  context_ = *o;
  DCHECK(context_->IsNativeContext());
  reference_map()->AddAttachedReference(
      reinterpret_cast<void*>(context_->global_proxy()->ptr()));
  // The bootstrap snapshot has a code-stub context. When serializing the
  // partial snapshot, it is chained into the weak context list on the isolate
  // and it's next context pointer may point to the code-stub context.  Clear
  // it before serializing, it will get re-added to the context list
  // explicitly when it's loaded.
  context_->set(Context::NEXT_CONTEXT_LINK,
                ReadOnlyRoots(isolate()).undefined_value());
  DCHECK(!context_->global_object()->IsUndefined());
  // Reset math random cache to get fresh random numbers.
  MathRandom::ResetContext(context_);

#ifdef DEBUG
  MicrotaskQueue* microtask_queue =
      context_->native_context()->microtask_queue();
  DCHECK_EQ(0, microtask_queue->size());
  DCHECK(!microtask_queue->HasMicrotasksSuppressions());
  DCHECK_EQ(0, microtask_queue->GetMicrotasksScopeDepth());
  DCHECK(microtask_queue->DebugMicrotasksScopeDepthIsZero());
#endif
  context_->native_context()->set_microtask_queue(nullptr);

  VisitRootPointer(Root::kPartialSnapshotCache, nullptr, FullObjectSlot(o));
  SerializeDeferredObjects();

  // Add section for embedder-serialized embedder fields.
  if (!embedder_fields_sink_.data()->empty()) {
    sink_.Put(kEmbedderFieldsData, "embedder fields data");
    sink_.Append(embedder_fields_sink_);
    sink_.Put(kSynchronize, "Finished with embedder fields data");
  }

  Pad();
}

void PartialSerializer::SerializeObject(HeapObject obj) {
  DCHECK(!ObjectIsBytecodeHandler(obj));  // Only referenced in dispatch table.

  if (SerializeHotObject(obj)) return;

  if (SerializeRoot(obj)) return;

  if (SerializeBackReference(obj)) return;

  if (startup_serializer_->SerializeUsingReadOnlyObjectCache(&sink_, obj)) {
    return;
  }

  if (ShouldBeInThePartialSnapshotCache(obj)) {
    startup_serializer_->SerializeUsingPartialSnapshotCache(&sink_, obj);
    return;
  }

  // Pointers from the partial snapshot to the objects in the startup snapshot
  // should go through the root array or through the partial snapshot cache.
  // If this is not the case you may have to add something to the root array.
  DCHECK(!startup_serializer_->ReferenceMapContains(obj));
  // All the internalized strings that the partial snapshot needs should be
  // either in the root table or in the partial snapshot cache.
  DCHECK(!obj->IsInternalizedString());
  // Function and object templates are not context specific.
  DCHECK(!obj->IsTemplateInfo());
  // We should not end up at another native context.
  DCHECK_IMPLIES(obj != context_, !obj->IsNativeContext());

  // Clear literal boilerplates and feedback.
  if (obj->IsFeedbackVector()) FeedbackVector::cast(obj)->ClearSlots(isolate());

  // Clear InterruptBudget when serializing FeedbackCell.
  if (obj->IsFeedbackCell()) {
    FeedbackCell::cast(obj)->set_interrupt_budget(
        FeedbackCell::GetInitialInterruptBudget());
  }

  if (SerializeJSObjectWithEmbedderFields(obj)) {
    return;
  }

  if (obj->IsJSFunction()) {
    // Unconditionally reset the JSFunction to its SFI's code, since we can't
    // serialize optimized code anyway.
    JSFunction closure = JSFunction::cast(obj);
    closure->ResetIfBytecodeFlushed();
    if (closure->is_compiled()) closure->set_code(closure->shared()->GetCode());
  }

  CheckRehashability(obj);

  // Object has not yet been serialized.  Serialize it here.
  ObjectSerializer serializer(this, obj, &sink_);
  serializer.Serialize();
}

bool PartialSerializer::ShouldBeInThePartialSnapshotCache(HeapObject o) {
  // Scripts should be referred only through shared function infos.  We can't
  // allow them to be part of the partial snapshot because they contain a
  // unique ID, and deserializing several partial snapshots containing script
  // would cause dupes.
  DCHECK(!o->IsScript());
  return o->IsName() || o->IsSharedFunctionInfo() || o->IsHeapNumber() ||
         o->IsCode() || o->IsScopeInfo() || o->IsAccessorInfo() ||
         o->IsTemplateInfo() || o->IsClassPositions() ||
         o->map() == ReadOnlyRoots(startup_serializer_->isolate())
                         .fixed_cow_array_map();
}

namespace {
bool DataIsEmpty(const StartupData& data) { return data.raw_size == 0; }
}  // anonymous namespace

bool PartialSerializer::SerializeJSObjectWithEmbedderFields(Object obj) {
  if (!obj->IsJSObject()) return false;
  JSObject js_obj = JSObject::cast(obj);
  int embedder_fields_count = js_obj->GetEmbedderFieldCount();
  if (embedder_fields_count == 0) return false;
  CHECK_GT(embedder_fields_count, 0);
  DCHECK(!js_obj->NeedsRehashing());

  DisallowHeapAllocation no_gc;
  DisallowJavascriptExecution no_js(isolate());
  DisallowCompilation no_compile(isolate());

  HandleScope scope(isolate());
  Handle<JSObject> obj_handle(js_obj, isolate());
  v8::Local<v8::Object> api_obj = v8::Utils::ToLocal(obj_handle);

  std::vector<EmbedderDataSlot::RawData> original_embedder_values;
  std::vector<StartupData> serialized_data;

  // 1) Iterate embedder fields. Hold onto the original value of the fields.
  //    Ignore references to heap objects since these are to be handled by the
  //    serializer. For aligned pointers, call the serialize callback. Hold
  //    onto the result.
  for (int i = 0; i < embedder_fields_count; i++) {
    EmbedderDataSlot embedder_data_slot(js_obj, i);
    original_embedder_values.emplace_back(embedder_data_slot.load_raw(no_gc));
    Object object = embedder_data_slot.load_tagged();
    if (object->IsHeapObject()) {
      DCHECK(isolate()->heap()->Contains(HeapObject::cast(object)));
      serialized_data.push_back({nullptr, 0});
    } else {
      // If no serializer is provided and the field was empty, we serialize it
      // by default to nullptr.
      if (serialize_embedder_fields_.callback == nullptr &&
          object->ptr() == 0) {
        serialized_data.push_back({nullptr, 0});
      } else {
        DCHECK_NOT_NULL(serialize_embedder_fields_.callback);
        StartupData data = serialize_embedder_fields_.callback(
            api_obj, i, serialize_embedder_fields_.data);
        serialized_data.push_back(data);
      }
    }
  }

  // 2) Embedder fields for which the embedder callback produced non-zero
  //    serialized data should be considered aligned pointers to objects owned
  //    by the embedder. Clear these memory addresses to avoid non-determism
  //    in the snapshot. This is done separately to step 1 to no not interleave
  //    with embedder callbacks.
  for (int i = 0; i < embedder_fields_count; i++) {
    if (!DataIsEmpty(serialized_data[i])) {
      EmbedderDataSlot(js_obj, i).store_raw(kNullAddress, no_gc);
    }
  }

  // 3) Serialize the object. References from embedder fields to heap objects or
  //    smis are serialized regularly.
  ObjectSerializer(this, js_obj, &sink_).Serialize();

  // 4) Obtain back reference for the serialized object.
  SerializerReference reference =
      reference_map()->LookupReference(reinterpret_cast<void*>(js_obj->ptr()));
  DCHECK(reference.is_back_reference());

  // 5) Write data returned by the embedder callbacks into a separate sink,
  //    headed by the back reference. Restore the original embedder fields.
  for (int i = 0; i < embedder_fields_count; i++) {
    StartupData data = serialized_data[i];
    if (DataIsEmpty(data)) continue;
    // Restore original values from cleared fields.
    EmbedderDataSlot(js_obj, i).store_raw(original_embedder_values[i], no_gc);
    embedder_fields_sink_.Put(kNewObject + reference.space(),
                              "embedder field holder");
    embedder_fields_sink_.PutInt(reference.chunk_index(), "BackRefChunkIndex");
    embedder_fields_sink_.PutInt(reference.chunk_offset(),
                                 "BackRefChunkOffset");
    embedder_fields_sink_.PutInt(i, "embedder field index");
    embedder_fields_sink_.PutInt(data.raw_size, "embedder fields data size");
    embedder_fields_sink_.PutRaw(reinterpret_cast<const byte*>(data.data),
                                 data.raw_size, "embedder fields data");
    delete[] data.data;
  }

  // 6) The content of the separate sink is appended eventually to the default
  //    sink. The ensures that during deserialization, we call the deserializer
  //    callback at the end, and can guarantee that the deserialized objects are
  //    in a consistent state. See PartialSerializer::Serialize.
  return true;
}

void PartialSerializer::CheckRehashability(HeapObject obj) {
  if (!can_be_rehashed_) return;
  if (!obj->NeedsRehashing()) return;
  if (obj->CanBeRehashed()) return;
  can_be_rehashed_ = false;
}

}  // namespace internal
}  // namespace v8
