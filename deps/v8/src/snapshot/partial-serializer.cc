// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/partial-serializer.h"
#include "src/snapshot/startup-serializer.h"

#include "src/objects-inl.h"

namespace v8 {
namespace internal {

PartialSerializer::PartialSerializer(
    Isolate* isolate, StartupSerializer* startup_serializer,
    v8::SerializeInternalFieldsCallback callback)
    : Serializer(isolate),
      startup_serializer_(startup_serializer),
      serialize_internal_fields_(callback) {
  InitializeCodeAddressMap();
}

PartialSerializer::~PartialSerializer() {
  OutputStatistics("PartialSerializer");
}

void PartialSerializer::Serialize(Object** o, bool include_global_proxy) {
  if ((*o)->IsContext()) {
    Context* context = Context::cast(*o);
    reference_map()->AddAttachedReference(context->global_proxy());
    // The bootstrap snapshot has a code-stub context. When serializing the
    // partial snapshot, it is chained into the weak context list on the isolate
    // and it's next context pointer may point to the code-stub context.  Clear
    // it before serializing, it will get re-added to the context list
    // explicitly when it's loaded.
    if (context->IsNativeContext()) {
      context->set(Context::NEXT_CONTEXT_LINK,
                   isolate_->heap()->undefined_value());
      DCHECK(!context->global_object()->IsUndefined(context->GetIsolate()));
      // Reset math random cache to get fresh random numbers.
      context->set_math_random_index(Smi::kZero);
      context->set_math_random_cache(isolate_->heap()->undefined_value());
    }
  }
  VisitPointer(o);
  SerializeDeferredObjects();
  SerializeInternalFields();
  Pad();
}

void PartialSerializer::SerializeObject(HeapObject* obj, HowToCode how_to_code,
                                        WhereToPoint where_to_point, int skip) {
  if (obj->IsMap()) {
    // The code-caches link to context-specific code objects, which
    // the startup and context serializes cannot currently handle.
    DCHECK(Map::cast(obj)->code_cache() == obj->GetHeap()->empty_fixed_array());
  }

  // Replace typed arrays by undefined.
  if (obj->IsJSTypedArray()) obj = isolate_->heap()->undefined_value();

  if (SerializeHotObject(obj, how_to_code, where_to_point, skip)) return;

  int root_index = root_index_map_.Lookup(obj);
  if (root_index != RootIndexMap::kInvalidRootIndex) {
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
  DCHECK(!startup_serializer_->reference_map()->Lookup(obj).is_valid());
  // All the internalized strings that the partial snapshot needs should be
  // either in the root table or in the partial snapshot cache.
  DCHECK(!obj->IsInternalizedString());
  // Function and object templates are not context specific.
  DCHECK(!obj->IsTemplateInfo());

  FlushSkip(skip);

  // Clear literal boilerplates.
  if (obj->IsJSFunction()) {
    JSFunction* function = JSFunction::cast(obj);
    LiteralsArray* literals = function->literals();
    for (int i = 0; i < literals->literals_count(); i++) {
      literals->set_literal_undefined(i);
    }
    function->ClearTypeFeedbackInfo();
  }

  if (obj->IsJSObject()) {
    JSObject* jsobj = JSObject::cast(obj);
    if (jsobj->GetInternalFieldCount() > 0) {
      DCHECK_NOT_NULL(serialize_internal_fields_.callback);
      internal_field_holders_.Add(jsobj);
    }
  }

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
         o->map() ==
             startup_serializer_->isolate()->heap()->fixed_cow_array_map();
}

void PartialSerializer::SerializeInternalFields() {
  int count = internal_field_holders_.length();
  if (count == 0) return;
  DisallowHeapAllocation no_gc;
  DisallowJavascriptExecution no_js(isolate());
  DisallowCompilation no_compile(isolate());
  DCHECK_NOT_NULL(serialize_internal_fields_.callback);
  sink_.Put(kInternalFieldsData, "internal fields data");
  while (internal_field_holders_.length() > 0) {
    HandleScope scope(isolate());
    Handle<JSObject> obj(internal_field_holders_.RemoveLast(), isolate());
    SerializerReference reference = reference_map_.Lookup(*obj);
    DCHECK(reference.is_back_reference());
    int internal_fields_count = obj->GetInternalFieldCount();
    for (int i = 0; i < internal_fields_count; i++) {
      if (obj->GetInternalField(i)->IsHeapObject()) continue;
      StartupData data = serialize_internal_fields_.callback(
          v8::Utils::ToLocal(obj), i, serialize_internal_fields_.data);
      sink_.Put(kNewObject + reference.space(), "internal field holder");
      PutBackReference(*obj, reference);
      sink_.PutInt(i, "internal field index");
      sink_.PutInt(data.raw_size, "internal fields data size");
      sink_.PutRaw(reinterpret_cast<const byte*>(data.data), data.raw_size,
                   "internal fields data");
      delete[] data.data;
    }
  }
  sink_.Put(kSynchronize, "Finished with internal fields data");
}

}  // namespace internal
}  // namespace v8
