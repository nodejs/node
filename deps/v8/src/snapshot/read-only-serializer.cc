// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/read-only-serializer.h"

#include "src/api/api.h"
#include "src/base/bounds.h"
#include "src/execution/v8threads.h"
#include "src/heap/factory-inl.h"
#include "src/heap/read-only-heap.h"
#include "src/objects/objects-inl.h"
#include "src/objects/slots.h"
#include "src/snapshot/serializer-inl.h"

namespace v8 {
namespace internal {

namespace {

class ReadOnlyHeapImageSerializer {
 public:
  struct MemoryRegion {
    Address start;
    size_t size;
  };

  using Bytecode = HeapImageSerializer::Bytecode;

  static void Serialize(ReadOnlySpace* space, SnapshotByteSink& sink,
                        Isolate* isolate,
                        const std::vector<MemoryRegion>& unmapped_regions) {
    // Memory dump serializer format, for pages of type T
    // --------------------------------------------------------------------
    // page_content_[1..n]     - content of each page
    // kSynchronize            - end mark
    // --------------------------------------------------------------------
    //   page_content:
    //   ------------------------------------------------------------------
    //   kTPage                  - page begin mark
    //   segment_content_[1..n]  - content of each segment
    //   kFinalizeTPage          - page end mark
    //   ------------------------------------------------------------------
    //     segment_content:
    //     ----------------------------------------------------------------
    //     kTSegment               - segment mark
    //     offset                  - start of segment rel. to area_start
    //     size                    - size of segment in bytes
    //     bytes_[1..size]         - content
    //     ----------------------------------------------------------------

    DCHECK_EQ(sink.Position(), 0);

    auto writeSegment = [&sink](const BasicMemoryChunk* p, Address pos,
                                size_t page_content_bytes) {
      sink.Put(Bytecode::kReadOnlySegment, "segment begin");
      sink.PutInt(pos - p->area_start(), "segment start offset");
      sink.PutInt(page_content_bytes, "segment byte size");
#ifdef MEMORY_SANITIZER
      __msan_check_mem_is_initialized(reinterpret_cast<void*>(pos),
                                      static_cast<int>(page_content_bytes));
#endif
      sink.PutRaw(reinterpret_cast<const uint8_t*>(pos),
                  static_cast<int>(page_content_bytes), "page");
    };

    for (const ReadOnlyPage* page : space->pages()) {
      sink.Put(Bytecode::kReadOnlyPage, "page begin");
      Tagged_t ptr =
          V8HeapCompressionScheme::CompressAny(reinterpret_cast<Address>(page));
      sink.PutInt(ptr, "page start offset");

      Address pos = page->area_start();

      // If this page contains unmapped regions split it into multiple segments.
      for (auto r = unmapped_regions.begin(); r != unmapped_regions.end();
           ++r) {
        // Regions must be sorted an non-overlapping
        if (r + 1 != unmapped_regions.end()) {
          CHECK(r->start < (r + 1)->start);
          CHECK(r->start + r->size < (r + 1)->start);
        }
        if (base::IsInRange(r->start, pos, page->area_end())) {
          size_t content_bytes = r->start - pos;
          writeSegment(page, pos, content_bytes);
          pos += content_bytes + r->size;
        }
      }

      // Pages are shrunk, but memory at the end of the area is still
      // uninitialized and we do not want to include it in the snapshot.
      size_t page_content_bytes = page->HighWaterMark() - pos;
      writeSegment(page, pos, page_content_bytes);
      sink.Put(Bytecode::kFinalizeReadOnlyPage, "page end");
    }
    sink.Put(Bytecode::kSynchronize, "space end");
  }
};

}  // namespace

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

void ReadOnlySerializer::SerializeObjectImpl(Handle<HeapObject> obj,
                                             SlotType slot_type) {
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
  object_serializer.Serialize(slot_type);
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

#if V8_STATIC_ROOTS_BOOL
  // STATIC_ROOTS builds do not iterate the roots for serialization. Instead
  // we use a memory snapshot format.

  // TODO(v8:13840, olivf): Integrate this into the ReadOnlyHeapImageSerializer
  // without actually having to wipe.
  isolate()->heap()->read_only_space()->Unseal();
  InstructionStartVector saved_entry_points;
  WipeInstructionStartsForDeterministicSerialization(saved_entry_points);

  // WasmNull's payload is aligned to the OS page and consists of
  // WasmNull::kPayloadSize bytes of unmapped memory. To avoid inflating the
  // snapshot size and access uninitialized and/or unmapped memory, the
  // serializer skipps the padding bytes and the payload.
  ReadOnlyRoots ro_roots(isolate());
  WasmNull wasm_null = ro_roots.wasm_null();
  HeapObject wasm_null_padding = ro_roots.wasm_null_padding();
  CHECK(wasm_null_padding.IsFreeSpace());
  Address wasm_null_padding_start =
      wasm_null_padding.address() + FreeSpace::kHeaderSize;
  std::vector<ReadOnlyHeapImageSerializer::MemoryRegion> unmapped;
  if (wasm_null.address() > wasm_null_padding_start) {
    unmapped.push_back({wasm_null_padding_start,
                        wasm_null.address() - wasm_null_padding_start});
  }
  unmapped.push_back({wasm_null.payload(), WasmNull::kPayloadSize});
  ReadOnlyHeapImageSerializer::Serialize(
      isolate()->read_only_heap()->read_only_space(), sink_, isolate(),
      unmapped);

  RestoreInstructionStarts(saved_entry_points);
  isolate()->heap()->read_only_space()->Seal(
      ReadOnlySpace::SealMode::kDoNotDetachFromHeap);

  if (v8_flags.serialization_statistics) {
    ReadOnlyHeapObjectIterator iterator(isolate()->read_only_heap());
    for (HeapObject object = iterator.Next(); !object.is_null();
         object = iterator.Next()) {
      CountAllocation(object.map(), object.Size(),
                      SnapshotSpace::kReadOnlyHeap);
    }
  }
#else
  ReadOnlyRoots(isolate()).Iterate(this);
  if (reconstruct_read_only_and_shared_object_caches_for_testing()) {
    ReconstructReadOnlyObjectCacheForTesting();
  }
#endif  // STATIC_ROOTS_BOOL
}

#ifdef V8_STATIC_ROOTS
void ReadOnlySerializer::WipeInstructionStartsForDeterministicSerialization(
    ReadOnlySerializer::InstructionStartVector& saved_entry_points) {
  // See also ObjectSerializer::OutputRawData.
  ReadOnlyHeapObjectIterator iterator(isolate()->read_only_heap());
  for (HeapObject object = iterator.Next(); !object.is_null();
       object = iterator.Next()) {
    if (!object.IsCode()) continue;
    Code code = Code::cast(object);
    saved_entry_points.push_back(code.instruction_start());
    code.SetInstructionStartForSerialization(isolate(), kNullAddress);
  }
}

void ReadOnlySerializer::RestoreInstructionStarts(
    const ReadOnlySerializer::InstructionStartVector& saved_entry_points) {
  int i = 0;
  ReadOnlyHeapObjectIterator iterator(isolate()->read_only_heap());
  for (HeapObject object = iterator.Next(); !object.is_null();
       object = iterator.Next()) {
    if (!object.IsCode()) continue;
    Code code = Code::cast(object);
    code.SetInstructionStartForSerialization(isolate(),
                                             saved_entry_points[i++]);
  }
}
#endif  // V8_STATIC_ROOTS

void ReadOnlySerializer::FinalizeSerialization() {
  if (V8_STATIC_ROOTS_BOOL) {
    DCHECK(object_cache_empty());
    DCHECK(deferred_objects_empty());
    return;
  }

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
  Pad();
}

bool ReadOnlySerializer::MustBeDeferred(HeapObject object) {
  if (Serializer::MustBeDeferred(object)) return true;
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
