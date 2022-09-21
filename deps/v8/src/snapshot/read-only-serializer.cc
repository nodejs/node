// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/read-only-serializer.h"

#include "src/api/api.h"
#include "src/execution/v8threads.h"
#include "src/heap/read-only-heap.h"
#include "src/objects/objects-inl.h"
#include "src/objects/slots.h"
#include "src/snapshot/serializer-inl.h"

namespace v8 {
namespace internal {

ReadOnlySerializer::ReadOnlySerializer(Isolate* isolate,
                                       Snapshot::SerializerFlags flags)
    : RootsSerializer(isolate, flags, RootIndex::kFirstReadOnlyRoot)
#ifdef DEBUG
      ,
      serialized_objects_(isolate->heap()),
      did_serialize_not_mapped_symbol_(false)
#endif
{
  static_assert(RootIndex::kFirstReadOnlyRoot == RootIndex::kFirstRoot);
}

ReadOnlySerializer::~ReadOnlySerializer() {
  OutputStatistics("ReadOnlySerializer");
}

void ReadOnlySerializer::SerializeObjectImpl(Handle<HeapObject> obj) {
  CHECK(ReadOnlyHeap::Contains(*obj));
  CHECK_IMPLIES(obj->IsString(), obj->IsInternalizedString());

  // There should be no references to the not_mapped_symbol except for the entry
  // in the root table, so don't try to serialize a reference and rely on the
  // below CHECK(!did_serialize_not_mapped_symbol_) to make sure it doesn't
  // serialize twice.
  {
    DisallowGarbageCollection no_gc;
    HeapObject raw = *obj;
    if (!IsNotMappedSymbol(raw)) {
      if (SerializeHotObject(raw)) return;
      if (IsRootAndHasBeenSerialized(raw) && SerializeRoot(raw)) return;
      if (SerializeBackReference(raw)) return;
    }

    CheckRehashability(raw);
  }

  // Object has not yet been serialized.  Serialize it here.
  ObjectSerializer object_serializer(this, obj, &sink_);
  object_serializer.Serialize();
#ifdef DEBUG
  if (IsNotMappedSymbol(*obj)) {
    CHECK(!did_serialize_not_mapped_symbol_);
    did_serialize_not_mapped_symbol_ = true;
  } else {
    CHECK_NULL(serialized_objects_.Find(obj));
    // There's no "IdentitySet", so use an IdentityMap with a value that is
    // later ignored.
    serialized_objects_.Insert(obj, 0);
  }
#endif
}

void ReadOnlySerializer::SerializeReadOnlyRoots() {
  // No active threads.
  CHECK_NULL(isolate()->thread_manager()->FirstThreadStateInUse());
  // No active or weak handles.
  CHECK_IMPLIES(!allow_active_isolate_for_testing(),
                isolate()->handle_scope_implementer()->blocks()->empty());

  ReadOnlyRoots(isolate()).Iterate(this);

  if (reconstruct_read_only_and_shared_object_caches_for_testing()) {
    ReconstructReadOnlyObjectCacheForTesting();
  }
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

#ifdef DEBUG
  // Check that every object on read-only heap is reachable (and was
  // serialized).
  ReadOnlyHeapObjectIterator iterator(isolate()->read_only_heap());
  for (HeapObject object = iterator.Next(); !object.is_null();
       object = iterator.Next()) {
    if (IsNotMappedSymbol(object)) {
      CHECK(did_serialize_not_mapped_symbol_);
    } else {
      CHECK_NOT_NULL(serialized_objects_.Find(object));
    }
  }
#endif
}

bool ReadOnlySerializer::MustBeDeferred(HeapObject object) {
  if (root_has_been_serialized(RootIndex::kFreeSpaceMap) &&
      root_has_been_serialized(RootIndex::kOnePointerFillerMap) &&
      root_has_been_serialized(RootIndex::kTwoPointerFillerMap)) {
    // All required root objects are serialized, so any aligned objects can
    // be saved without problems.
    return false;
  }
  // Defer objects with special alignment requirements until the filler roots
  // are serialized.
  return HeapObject::RequiredAlignment(object.map()) != kTaggedAligned;
}

bool ReadOnlySerializer::SerializeUsingReadOnlyObjectCache(
    SnapshotByteSink* sink, Handle<HeapObject> obj) {
  if (!ReadOnlyHeap::Contains(*obj)) return false;

  // Get the cache index and serialize it into the read-only snapshot if
  // necessary.
  int cache_index = SerializeInObjectCache(obj);

  // Writing out the cache entry into the calling serializer's sink.
  sink->Put(kReadOnlyObjectCache, "ReadOnlyObjectCache");
  sink->PutInt(cache_index, "read_only_object_cache_index");

  return true;
}

void ReadOnlySerializer::ReconstructReadOnlyObjectCacheForTesting() {
  ReadOnlyHeap* ro_heap = isolate()->read_only_heap();
  DCHECK(ro_heap->read_only_object_cache_is_initialized());
  for (size_t i = 0, size = ro_heap->read_only_object_cache_size(); i < size;
       i++) {
    Handle<HeapObject> obj(
        HeapObject::cast(ro_heap->cached_read_only_object(i)), isolate());
    int cache_index = SerializeInObjectCache(obj);
    USE(cache_index);
    DCHECK_EQ(cache_index, i);
  }
}

}  // namespace internal
}  // namespace v8
