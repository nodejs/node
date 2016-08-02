// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/partial-serializer.h"

#include "src/objects-inl.h"

namespace v8 {
namespace internal {

PartialSerializer::PartialSerializer(Isolate* isolate,
                                     Serializer* startup_snapshot_serializer,
                                     SnapshotByteSink* sink)
    : Serializer(isolate, sink),
      startup_serializer_(startup_snapshot_serializer),
      next_partial_cache_index_(0) {
  InitializeCodeAddressMap();
}

PartialSerializer::~PartialSerializer() {
  OutputStatistics("PartialSerializer");
}

void PartialSerializer::Serialize(Object** o) {
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
      DCHECK(!context->global_object()->IsUndefined());
    }
  }
  VisitPointer(o);
  SerializeDeferredObjects();
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

  int root_index = root_index_map_.Lookup(obj);
  if (root_index != RootIndexMap::kInvalidRootIndex) {
    PutRoot(root_index, obj, how_to_code, where_to_point, skip);
    return;
  }

  if (ShouldBeInThePartialSnapshotCache(obj)) {
    FlushSkip(skip);

    int cache_index = PartialSnapshotCacheIndex(obj);
    sink_->Put(kPartialSnapshotCache + how_to_code + where_to_point,
               "PartialSnapshotCache");
    sink_->PutInt(cache_index, "partial_snapshot_cache_index");
    return;
  }

  // Pointers from the partial snapshot to the objects in the startup snapshot
  // should go through the root array or through the partial snapshot cache.
  // If this is not the case you may have to add something to the root array.
  DCHECK(!startup_serializer_->reference_map()->Lookup(obj).is_valid());
  // All the internalized strings that the partial snapshot needs should be
  // either in the root table or in the partial snapshot cache.
  DCHECK(!obj->IsInternalizedString());

  if (SerializeKnownObject(obj, how_to_code, where_to_point, skip)) return;

  FlushSkip(skip);

  // Clear literal boilerplates.
  if (obj->IsJSFunction()) {
    FixedArray* literals = JSFunction::cast(obj)->literals();
    for (int i = 0; i < literals->length(); i++) literals->set_undefined(i);
  }

  // Object has not yet been serialized.  Serialize it here.
  ObjectSerializer serializer(this, obj, sink_, how_to_code, where_to_point);
  serializer.Serialize();
}

int PartialSerializer::PartialSnapshotCacheIndex(HeapObject* heap_object) {
  int index = partial_cache_index_map_.LookupOrInsert(
      heap_object, next_partial_cache_index_);
  if (index == PartialCacheIndexMap::kInvalidIndex) {
    // This object is not part of the partial snapshot cache yet. Add it to the
    // startup snapshot so we can refer to it via partial snapshot index from
    // the partial snapshot.
    startup_serializer_->VisitPointer(reinterpret_cast<Object**>(&heap_object));
    return next_partial_cache_index_++;
  }
  return index;
}

bool PartialSerializer::ShouldBeInThePartialSnapshotCache(HeapObject* o) {
  // Scripts should be referred only through shared function infos.  We can't
  // allow them to be part of the partial snapshot because they contain a
  // unique ID, and deserializing several partial snapshots containing script
  // would cause dupes.
  DCHECK(!o->IsScript());
  return o->IsName() || o->IsSharedFunctionInfo() || o->IsHeapNumber() ||
         o->IsCode() || o->IsScopeInfo() || o->IsAccessorInfo() ||
         o->map() ==
             startup_serializer_->isolate()->heap()->fixed_cow_array_map();
}

}  // namespace internal
}  // namespace v8
