// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/read-only-serializer.h"

#include "src/api.h"
#include "src/code-tracer.h"
#include "src/global-handles.h"
#include "src/objects-inl.h"
#include "src/objects/slots.h"
#include "src/snapshot/startup-serializer.h"
#include "src/v8threads.h"

namespace v8 {
namespace internal {

ReadOnlySerializer::ReadOnlySerializer(Isolate* isolate)
    : RootsSerializer(isolate, RootIndex::kFirstReadOnlyRoot) {
  STATIC_ASSERT(RootIndex::kFirstReadOnlyRoot == RootIndex::kFirstRoot);
}

ReadOnlySerializer::~ReadOnlySerializer() {
  OutputStatistics("ReadOnlySerializer");
}

void ReadOnlySerializer::SerializeObject(HeapObject obj, HowToCode how_to_code,
                                         WhereToPoint where_to_point,
                                         int skip) {
  CHECK(isolate()->heap()->read_only_space()->Contains(obj));
  CHECK_IMPLIES(obj->IsString(), obj->IsInternalizedString());

  if (SerializeHotObject(obj, how_to_code, where_to_point, skip)) return;
  if (IsRootAndHasBeenSerialized(obj) &&
      SerializeRoot(obj, how_to_code, where_to_point, skip)) {
    return;
  }
  if (SerializeBackReference(obj, how_to_code, where_to_point, skip)) return;

  FlushSkip(skip);

  CheckRehashability(obj);

  // Object has not yet been serialized.  Serialize it here.
  ObjectSerializer object_serializer(this, obj, &sink_, how_to_code,
                                     where_to_point);
  object_serializer.Serialize();
}

void ReadOnlySerializer::SerializeReadOnlyRoots() {
  // No active threads.
  CHECK_NULL(isolate()->thread_manager()->FirstThreadStateInUse());
  // No active or weak handles.
  CHECK(isolate()->handle_scope_implementer()->blocks()->empty());

  ReadOnlyRoots(isolate()).Iterate(this);
}

void ReadOnlySerializer::FinalizeSerialization() {
  // This comes right after serialization of the other snapshots, where we
  // add entries to the read-only object cache. Add one entry with 'undefined'
  // to terminate the read-only object cache.
  Object undefined = ReadOnlyRoots(isolate()).undefined_value();
  VisitRootPointer(Root::kReadOnlyObjectCache, nullptr,
                   FullObjectSlot(&undefined));
  SerializeDeferredObjects();
  Pad();
}

bool ReadOnlySerializer::MustBeDeferred(HeapObject object) {
  if (root_has_been_serialized(RootIndex::kFreeSpaceMap) &&
      root_has_been_serialized(RootIndex::kOnePointerFillerMap) &&
      root_has_been_serialized(RootIndex::kTwoPointerFillerMap)) {
    // All required root objects are serialized, so any aligned objects can
    // be saved without problems.
    return false;
  }
  // Just defer everything except for Map objects until all required roots are
  // serialized. Some objects may have special alignment requirements, that may
  // not be fulfilled during deserialization until few first root objects are
  // serialized. But we must serialize Map objects since deserializer checks
  // that these root objects are indeed Maps.
  return !object->IsMap();
}

bool ReadOnlySerializer::SerializeUsingReadOnlyObjectCache(
    SnapshotByteSink* sink, HeapObject obj, HowToCode how_to_code,
    WhereToPoint where_to_point, int skip) {
  if (!isolate()->heap()->read_only_space()->Contains(obj)) return false;

  // Get the cache index and serialize it into the read-only snapshot if
  // necessary.
  int cache_index = SerializeInObjectCache(obj);

  // Writing out the cache entry into the calling serializer's sink.
  FlushSkip(sink, skip);
  sink->Put(kReadOnlyObjectCache + how_to_code + where_to_point,
            "ReadOnlyObjectCache");
  sink->PutInt(cache_index, "read_only_object_cache_index");

  return true;
}

}  // namespace internal
}  // namespace v8
