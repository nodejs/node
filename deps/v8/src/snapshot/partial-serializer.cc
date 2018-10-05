// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/partial-serializer.h"
#include "src/snapshot/startup-serializer.h"

#include "src/api-inl.h"
#include "src/math-random.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

PartialSerializer::PartialSerializer(
    Isolate* isolate, StartupSerializer* startup_serializer,
    v8::SerializeEmbedderFieldsCallback callback)
    : Serializer(isolate),
      startup_serializer_(startup_serializer),
      serialize_embedder_fields_(callback),
      can_be_rehashed_(true),
      context_(nullptr) {
  InitializeCodeAddressMap();
  allocator()->UseCustomChunkSize(FLAG_serialization_chunk_size);
}

PartialSerializer::~PartialSerializer() {
  OutputStatistics("PartialSerializer");
}

void PartialSerializer::Serialize(Context** o, bool include_global_proxy) {
  context_ = *o;
  DCHECK(context_->IsNativeContext());
  reference_map()->AddAttachedReference(context_->global_proxy());
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

  VisitRootPointer(Root::kPartialSnapshotCache, nullptr,
                   reinterpret_cast<Object**>(o));
  SerializeDeferredObjects();
  SerializeEmbedderFields();
  Pad();
}

void PartialSerializer::SerializeObject(HeapObject* obj, HowToCode how_to_code,
                                        WhereToPoint where_to_point, int skip) {
  DCHECK(!ObjectIsBytecodeHandler(obj));  // Only referenced in dispatch table.

  if (SerializeBuiltinReference(obj, how_to_code, where_to_point, skip)) {
    return;
  }
  if (SerializeHotObject(obj, how_to_code, where_to_point, skip)) return;

  RootIndex root_index;
  if (root_index_map()->Lookup(obj, &root_index)) {
    PutRoot(root_index, obj, how_to_code, where_to_point, skip);
    return;
  }

  if (SerializeBackReference(obj, how_to_code, where_to_point, skip)) return;

  if (ShouldBeInThePartialSnapshotCache(obj)) {
    FlushSkip(skip);

    int cache_index = startup_serializer_->PartialSnapshotCacheIndex(obj);
    sink_.Put(kPartialSnapshotCache + how_to_code + where_to_point,
              "PartialSnapshotCache");
    sink_.PutInt(cache_index, "partial_snapshot_cache_index");
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

  FlushSkip(skip);

  // Clear literal boilerplates and feedback.
  if (obj->IsFeedbackVector()) FeedbackVector::cast(obj)->ClearSlots(isolate());

  if (obj->IsJSObject()) {
    JSObject* jsobj = JSObject::cast(obj);
    if (jsobj->GetEmbedderFieldCount() > 0) {
      DCHECK_NOT_NULL(serialize_embedder_fields_.callback);
      embedder_field_holders_.push_back(jsobj);
    }
  }

  if (obj->IsJSFunction()) {
    // Unconditionally reset the JSFunction to its SFI's code, since we can't
    // serialize optimized code anyway.
    JSFunction* closure = JSFunction::cast(obj);
    if (closure->is_compiled()) closure->set_code(closure->shared()->GetCode());
  }

  CheckRehashability(obj);

  // Object has not yet been serialized.  Serialize it here.
  ObjectSerializer serializer(this, obj, &sink_, how_to_code, where_to_point);
  serializer.Serialize();
}

bool PartialSerializer::ShouldBeInThePartialSnapshotCache(HeapObject* o) {
  // Scripts should be referred only through shared function infos.  We can't
  // allow them to be part of the partial snapshot because they contain a
  // unique ID, and deserializing several partial snapshots containing script
  // would cause dupes.
  DCHECK(!o->IsScript());
  return o->IsName() || o->IsSharedFunctionInfo() || o->IsHeapNumber() ||
         o->IsCode() || o->IsScopeInfo() || o->IsAccessorInfo() ||
         o->IsTemplateInfo() ||
         o->map() == ReadOnlyRoots(startup_serializer_->isolate())
                         .fixed_cow_array_map();
}

void PartialSerializer::SerializeEmbedderFields() {
  if (embedder_field_holders_.empty()) return;
  DisallowHeapAllocation no_gc;
  DisallowJavascriptExecution no_js(isolate());
  DisallowCompilation no_compile(isolate());
  DCHECK_NOT_NULL(serialize_embedder_fields_.callback);
  sink_.Put(kEmbedderFieldsData, "embedder fields data");
  while (!embedder_field_holders_.empty()) {
    HandleScope scope(isolate());
    Handle<JSObject> obj(embedder_field_holders_.back(), isolate());
    embedder_field_holders_.pop_back();
    SerializerReference reference = reference_map()->LookupReference(*obj);
    DCHECK(reference.is_back_reference());
    int embedder_fields_count = obj->GetEmbedderFieldCount();
    for (int i = 0; i < embedder_fields_count; i++) {
      if (obj->GetEmbedderField(i)->IsHeapObject()) continue;

      StartupData data = serialize_embedder_fields_.callback(
          v8::Utils::ToLocal(obj), i, serialize_embedder_fields_.data);
      sink_.Put(kNewObject + reference.space(), "embedder field holder");
      PutBackReference(*obj, reference);
      sink_.PutInt(i, "embedder field index");
      sink_.PutInt(data.raw_size, "embedder fields data size");
      sink_.PutRaw(reinterpret_cast<const byte*>(data.data), data.raw_size,
                   "embedder fields data");
      delete[] data.data;
    }
  }
  sink_.Put(kSynchronize, "Finished with embedder fields data");
}

void PartialSerializer::CheckRehashability(HeapObject* obj) {
  if (!can_be_rehashed_) return;
  if (!obj->NeedsRehashing()) return;
  if (obj->CanBeRehashed()) return;
  can_be_rehashed_ = false;
}

}  // namespace internal
}  // namespace v8
