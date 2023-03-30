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
  if (V8_STATIC_ROOTS_BOOL) {
    // .. since RO heap pages are serialized verbatim:
    set_serializer_tracks_serialization_statistics(false);
  }
}

ReadOnlySerializer::~ReadOnlySerializer() {
  OutputStatistics("ReadOnlySerializer");
}

void ReadOnlySerializer::SerializeObjectImpl(Handle<HeapObject> obj) {
  CHECK(ReadOnlyHeap::Contains(*obj));
  CHECK_IMPLIES(obj->IsString(), obj->IsInternalizedString());
  DCHECK(!V8_STATIC_ROOTS_BOOL);

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

  if (!V8_STATIC_ROOTS_BOOL) {
    ReadOnlyRoots(isolate()).Iterate(this);
    if (reconstruct_read_only_and_shared_object_caches_for_testing()) {
      ReconstructReadOnlyObjectCacheForTesting();
    }
  }
}

#ifdef V8_STATIC_ROOTS
void ReadOnlySerializer::WipeCodeEntryPointsForDeterministicSerialization(
    ReadOnlySerializer::CodeEntryPointVector& saved_entry_points) {
  // See also ObjectSerializer::OutputRawData.
  ReadOnlyHeapObjectIterator iterator(isolate()->read_only_heap());
  for (HeapObject object = iterator.Next(); !object.is_null();
       object = iterator.Next()) {
    if (!object.IsCode()) continue;
    Code code = Code::cast(object);
    saved_entry_points.push_back(code.code_entry_point());
    code.SetCodeEntryPointForSerialization(isolate(), kNullAddress);
  }
}

void ReadOnlySerializer::RestoreCodeEntryPoints(
    const ReadOnlySerializer::CodeEntryPointVector& saved_entry_points) {
  int i = 0;
  ReadOnlyHeapObjectIterator iterator(isolate()->read_only_heap());
  for (HeapObject object = iterator.Next(); !object.is_null();
       object = iterator.Next()) {
    if (!object.IsCode()) continue;
    Code code = Code::cast(object);
    code.SetCodeEntryPointForSerialization(isolate(), saved_entry_points[i++]);
  }
}
#endif  // V8_STATIC_ROOTS

void ReadOnlySerializer::FinalizeSerialization() {
#if V8_STATIC_ROOTS_BOOL
  DCHECK(object_cache_empty());
  DCHECK(deferred_objects_empty());
  DCHECK_EQ(sink_.Position(), 0);

  // Note the memcpy-based serialization done here must also guarantee a
  // deterministic serialized layout. See also ObjectSerializer::OutputRawData
  // which implements custom logic for this (but is not reached when
  // serializing memcpy-style).

  {
    DisallowGarbageCollection no_gc;

    isolate()->heap()->read_only_space()->Unseal();
    CodeEntryPointVector saved_entry_points;
    WipeCodeEntryPointsForDeterministicSerialization(saved_entry_points);

    auto space = isolate()->read_only_heap()->read_only_space();
    size_t num_pages = space->pages().size();
    sink_.PutInt(num_pages, "num pages");
    Tagged_t pos = V8HeapCompressionScheme::CompressAny(
        reinterpret_cast<Address>(space->pages()[0]));
    sink_.PutInt(pos, "first page offset");
    // Unprotect and reprotect the payload of wasm null. The header is not
    // protected.
    Address wasm_null_payload = isolate()->factory()->wasm_null()->payload();
    constexpr int kWasmNullPayloadSize = WasmNull::kSize - kTaggedSize;
    SetPermissions(isolate()->page_allocator(), wasm_null_payload,
                   kWasmNullPayloadSize, PageAllocator::kRead);
    for (auto p : space->pages()) {
      // Pages are shrunk, but memory at the end of the area is still
      // uninitialized and we do not want to include it in the snapshot.
      size_t page_content_bytes = p->HighWaterMark() - p->area_start();
      sink_.PutInt(page_content_bytes, "page content bytes");
#ifdef MEMORY_SANITIZER
      __msan_check_mem_is_initialized(reinterpret_cast<void*>(p->area_start()),
                                      static_cast<int>(page_content_bytes));
#endif
      sink_.PutRaw(reinterpret_cast<const byte*>(p->area_start()),
                   static_cast<int>(page_content_bytes), "page");
    }
    // Mark the virtual page range as inaccessible, and allow the OS to reclaim
    // the underlying physical pages. We do not want to protect the header (map
    // word), as it needs to remain accessible.
    isolate()->page_allocator()->DecommitPages(
        reinterpret_cast<void*>(wasm_null_payload), kWasmNullPayloadSize);

    RestoreCodeEntryPoints(saved_entry_points);
    isolate()->heap()->read_only_space()->Seal(
        ReadOnlySpace::SealMode::kDoNotDetachFromHeap);
  }
#else  // V8_STATIC_ROOTS_BOOL
  // This comes right after serialization of the other snapshots, where we
  // add entries to the read-only object cache. Add one entry with 'undefined'
  // to terminate the read-only object cache.
  Object undefined = ReadOnlyRoots(isolate()).undefined_value();
  VisitRootPointer(Root::kReadOnlyObjectCache, nullptr,
                   FullObjectSlot(&undefined));
  SerializeDeferredObjects();

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
#endif  // DEBUG
#endif  // V8_STATIC_ROOTS_BOOL

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
  // Defer objects with special alignment requirements until the filler roots
  // are serialized.
  return HeapObject::RequiredAlignment(object.map()) != kTaggedAligned;
}

bool ReadOnlySerializer::SerializeUsingReadOnlyObjectCache(
    SnapshotByteSink* sink, Handle<HeapObject> obj) {
  if (!ReadOnlyHeap::Contains(*obj)) return false;

  if (V8_STATIC_ROOTS_BOOL) {
    SerializeReadOnlyObjectReference(*obj, sink);
  } else {
    // Get the cache index and serialize it into the read-only snapshot if
    // necessary.
    int cache_index = SerializeInObjectCache(obj);
    // Writing out the cache entry into the calling serializer's sink.
    sink->Put(kReadOnlyObjectCache, "ReadOnlyObjectCache");
    sink->PutInt(cache_index, "read_only_object_cache_index");
  }

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
