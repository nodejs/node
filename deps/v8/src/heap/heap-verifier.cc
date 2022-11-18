// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/heap-verifier.h"

#include "include/v8-locker.h"
#include "src/codegen/assembler-inl.h"
#include "src/codegen/reloc-info.h"
#include "src/execution/isolate.h"
#include "src/heap/array-buffer-sweeper.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/heap/heap.h"
#include "src/heap/large-spaces.h"
#include "src/heap/new-spaces.h"
#include "src/heap/objects-visiting-inl.h"
#include "src/heap/paged-spaces.h"
#include "src/heap/read-only-heap.h"
#include "src/heap/read-only-spaces.h"
#include "src/heap/remembered-set.h"
#include "src/heap/safepoint.h"
#include "src/objects/code-inl.h"
#include "src/objects/code.h"
#include "src/objects/maybe-object.h"
#include "src/objects/slots-inl.h"
#include "src/objects/string-table.h"

#ifdef VERIFY_HEAP
namespace v8 {
namespace internal {

// Verify that all objects are Smis.
class VerifySmisVisitor final : public RootVisitor {
 public:
  void VisitRootPointers(Root root, const char* description,
                         FullObjectSlot start, FullObjectSlot end) override {
    for (FullObjectSlot current = start; current < end; ++current) {
      CHECK((*current).IsSmi());
    }
  }
};

class HeapVerification final {
 public:
  explicit HeapVerification(Heap* heap) : heap_(heap) {}

  void Verify();
  void VerifyReadOnlyHeap();
  void VerifySharedHeap(Isolate* initiator);

 private:
  void VerifyInvalidatedObjectSize();

  ReadOnlySpace* read_only_space() const { return heap_->read_only_space(); }
  NewSpace* new_space() const { return heap_->new_space(); }
  OldSpace* old_space() const { return heap_->old_space(); }
  MapSpace* map_space() const { return heap_->map_space(); }
  CodeSpace* code_space() const { return heap_->code_space(); }
  LargeObjectSpace* lo_space() const { return heap_->lo_space(); }
  CodeLargeObjectSpace* code_lo_space() const { return heap_->code_lo_space(); }
  NewLargeObjectSpace* new_lo_space() const { return heap_->new_lo_space(); }

  Isolate* isolate() const { return heap_->isolate(); }
  Heap* heap() const { return heap_; }

  Heap* heap_;
};

void HeapVerification::Verify() {
  CHECK(heap()->HasBeenSetUp());
  AllowGarbageCollection allow_gc;
  IgnoreLocalGCRequests ignore_gc_requests(heap());
  SafepointScope safepoint_scope(heap());
  HandleScope scope(isolate());

  heap()->MakeHeapIterable();

  heap()->array_buffer_sweeper()->EnsureFinished();

  VerifyPointersVisitor visitor(heap());
  heap()->IterateRoots(&visitor, {});

  if (!isolate()->context().is_null() &&
      !isolate()->raw_native_context().is_null()) {
    Object normalized_map_cache =
        isolate()->raw_native_context().normalized_map_cache();

    if (normalized_map_cache.IsNormalizedMapCache()) {
      NormalizedMapCache::cast(normalized_map_cache)
          .NormalizedMapCacheVerify(isolate());
    }
  }

  // The heap verifier can't deal with partially deserialized objects, so
  // disable it if a deserializer is active.
  // TODO(leszeks): Enable verification during deserialization, e.g. by only
  // blocklisting objects that are in a partially deserialized state.
  if (isolate()->has_active_deserializer()) return;

  VerifySmisVisitor smis_visitor;
  heap()->IterateSmiRoots(&smis_visitor);

  if (new_space()) new_space()->Verify(isolate());

  old_space()->Verify(isolate(), &visitor);
  if (map_space()) {
    map_space()->Verify(isolate(), &visitor);
  }

  VerifyPointersVisitor no_dirty_regions_visitor(heap());
  code_space()->Verify(isolate(), &no_dirty_regions_visitor);

  lo_space()->Verify(isolate());
  code_lo_space()->Verify(isolate());
  if (new_lo_space()) new_lo_space()->Verify(isolate());
  isolate()->string_table()->VerifyIfOwnedBy(isolate());

  VerifyInvalidatedObjectSize();

#if DEBUG
  heap()->VerifyCommittedPhysicalMemory();
#endif  // DEBUG
}

namespace {
void VerifyInvalidatedSlots(InvalidatedSlots* invalidated_slots) {
  if (!invalidated_slots) return;
  for (std::pair<HeapObject, int> object_and_size : *invalidated_slots) {
    HeapObject object = object_and_size.first;
    int size = object_and_size.second;
    CHECK_EQ(object.Size(), size);
  }
}
}  // namespace

void HeapVerification::VerifyInvalidatedObjectSize() {
  OldGenerationMemoryChunkIterator chunk_iterator(heap());
  MemoryChunk* chunk;

  while ((chunk = chunk_iterator.next()) != nullptr) {
    VerifyInvalidatedSlots(chunk->invalidated_slots<OLD_TO_NEW>());
    VerifyInvalidatedSlots(chunk->invalidated_slots<OLD_TO_OLD>());
    VerifyInvalidatedSlots(chunk->invalidated_slots<OLD_TO_SHARED>());
  }
}

void HeapVerification::VerifyReadOnlyHeap() {
  CHECK(!read_only_space()->writable());
  read_only_space()->Verify(isolate());
}

class SlotVerifyingVisitor : public ObjectVisitorWithCageBases {
 public:
  SlotVerifyingVisitor(Isolate* isolate, std::set<Address>* untyped,
                       std::set<std::pair<SlotType, Address>>* typed)
      : ObjectVisitorWithCageBases(isolate), untyped_(untyped), typed_(typed) {}

  virtual bool ShouldHaveBeenRecorded(HeapObject host, MaybeObject target) = 0;

  void VisitPointers(HeapObject host, ObjectSlot start,
                     ObjectSlot end) override {
#ifdef DEBUG
    for (ObjectSlot slot = start; slot < end; ++slot) {
      Object obj = slot.load(cage_base());
      CHECK(!MapWord::IsPacked(obj.ptr()) || !HasWeakHeapObjectTag(obj));
    }
#endif  // DEBUG
    VisitPointers(host, MaybeObjectSlot(start), MaybeObjectSlot(end));
  }

  void VisitPointers(HeapObject host, MaybeObjectSlot start,
                     MaybeObjectSlot end) final {
    for (MaybeObjectSlot slot = start; slot < end; ++slot) {
      if (ShouldHaveBeenRecorded(host, slot.load(cage_base()))) {
        CHECK_GT(untyped_->count(slot.address()), 0);
      }
    }
  }

  void VisitCodePointer(HeapObject host, CodeObjectSlot slot) override {
    CHECK(V8_EXTERNAL_CODE_SPACE_BOOL);
    if (ShouldHaveBeenRecorded(
            host, MaybeObject::FromObject(slot.load(code_cage_base())))) {
      CHECK_GT(untyped_->count(slot.address()), 0);
    }
  }

  void VisitCodeTarget(Code host, RelocInfo* rinfo) override {
    Object target = Code::GetCodeFromTargetAddress(rinfo->target_address());
    if (ShouldHaveBeenRecorded(host, MaybeObject::FromObject(target))) {
      CHECK(InTypedSet(SlotType::kCodeEntry, rinfo->pc()) ||
            (rinfo->IsInConstantPool() &&
             InTypedSet(SlotType::kConstPoolCodeEntry,
                        rinfo->constant_pool_entry_address())));
    }
  }

  void VisitEmbeddedPointer(Code host, RelocInfo* rinfo) override {
    Object target = rinfo->target_object(cage_base());
    if (ShouldHaveBeenRecorded(host, MaybeObject::FromObject(target))) {
      CHECK(InTypedSet(SlotType::kEmbeddedObjectFull, rinfo->pc()) ||
            InTypedSet(SlotType::kEmbeddedObjectCompressed, rinfo->pc()) ||
            (rinfo->IsInConstantPool() &&
             InTypedSet(SlotType::kConstPoolEmbeddedObjectCompressed,
                        rinfo->constant_pool_entry_address())) ||
            (rinfo->IsInConstantPool() &&
             InTypedSet(SlotType::kConstPoolEmbeddedObjectFull,
                        rinfo->constant_pool_entry_address())));
    }
  }

 protected:
  bool InUntypedSet(ObjectSlot slot) {
    return untyped_->count(slot.address()) > 0;
  }

 private:
  bool InTypedSet(SlotType type, Address slot) {
    return typed_->count(std::make_pair(type, slot)) > 0;
  }
  std::set<Address>* untyped_;
  std::set<std::pair<SlotType, Address>>* typed_;
};

class OldToNewSlotVerifyingVisitor : public SlotVerifyingVisitor {
 public:
  OldToNewSlotVerifyingVisitor(Isolate* isolate, std::set<Address>* untyped,
                               std::set<std::pair<SlotType, Address>>* typed,
                               EphemeronRememberedSet* ephemeron_remembered_set)
      : SlotVerifyingVisitor(isolate, untyped, typed),
        ephemeron_remembered_set_(ephemeron_remembered_set) {}

  bool ShouldHaveBeenRecorded(HeapObject host, MaybeObject target) override {
    DCHECK_IMPLIES(target->IsStrongOrWeak() && Heap::InYoungGeneration(target),
                   Heap::InToPage(target));
    return target->IsStrongOrWeak() && Heap::InYoungGeneration(target) &&
           !Heap::InYoungGeneration(host);
  }

  void VisitEphemeron(HeapObject host, int index, ObjectSlot key,
                      ObjectSlot target) override {
    VisitPointer(host, target);
    if (v8_flags.minor_mc) return;
    // Keys are handled separately and should never appear in this set.
    CHECK(!InUntypedSet(key));
    Object k = *key;
    if (!ObjectInYoungGeneration(host) && ObjectInYoungGeneration(k)) {
      EphemeronHashTable table = EphemeronHashTable::cast(host);
      auto it = ephemeron_remembered_set_->find(table);
      CHECK(it != ephemeron_remembered_set_->end());
      int slot_index =
          EphemeronHashTable::SlotToIndex(table.address(), key.address());
      InternalIndex entry = EphemeronHashTable::IndexToEntry(slot_index);
      CHECK(it->second.find(entry.as_int()) != it->second.end());
    }
  }

 private:
  EphemeronRememberedSet* ephemeron_remembered_set_;
};

class OldToSharedSlotVerifyingVisitor : public SlotVerifyingVisitor {
 public:
  OldToSharedSlotVerifyingVisitor(Isolate* isolate, std::set<Address>* untyped,
                                  std::set<std::pair<SlotType, Address>>* typed)
      : SlotVerifyingVisitor(isolate, untyped, typed) {}

  bool ShouldHaveBeenRecorded(HeapObject host, MaybeObject target) override {
    return target->IsStrongOrWeak() && Heap::InSharedWritableHeap(target) &&
           !Heap::InYoungGeneration(host) && !host.InSharedWritableHeap();
  }
};

template <RememberedSetType direction>
void CollectSlots(MemoryChunk* chunk, Address start, Address end,
                  std::set<Address>* untyped,
                  std::set<std::pair<SlotType, Address>>* typed) {
  RememberedSet<direction>::Iterate(
      chunk,
      [start, end, untyped](MaybeObjectSlot slot) {
        if (start <= slot.address() && slot.address() < end) {
          untyped->insert(slot.address());
        }
        return KEEP_SLOT;
      },
      SlotSet::FREE_EMPTY_BUCKETS);
  RememberedSet<direction>::IterateTyped(
      chunk, [=](SlotType type, Address slot) {
        if (start <= slot && slot < end) {
          typed->insert(std::make_pair(type, slot));
        }
        return KEEP_SLOT;
      });
}

// Helper class for collecting slot addresses.
class SlotCollectingVisitor final : public ObjectVisitor {
 public:
  void VisitPointers(HeapObject host, ObjectSlot start,
                     ObjectSlot end) override {
    VisitPointers(host, MaybeObjectSlot(start), MaybeObjectSlot(end));
  }
  void VisitPointers(HeapObject host, MaybeObjectSlot start,
                     MaybeObjectSlot end) final {
    for (MaybeObjectSlot p = start; p < end; ++p) {
      slots_.push_back(p);
    }
  }

  void VisitCodePointer(HeapObject host, CodeObjectSlot slot) override {
    CHECK(V8_EXTERNAL_CODE_SPACE_BOOL);
#ifdef V8_EXTERNAL_CODE_SPACE
    code_slots_.push_back(slot);
#endif
  }

  void VisitCodeTarget(Code host, RelocInfo* rinfo) final { UNREACHABLE(); }

  void VisitEmbeddedPointer(Code host, RelocInfo* rinfo) override {
    UNREACHABLE();
  }

  void VisitMapPointer(HeapObject object) override {}  // do nothing by default

  int number_of_slots() { return static_cast<int>(slots_.size()); }

  MaybeObjectSlot slot(int i) { return slots_[i]; }
#ifdef V8_EXTERNAL_CODE_SPACE
  CodeObjectSlot code_slot(int i) { return code_slots_[i]; }
  int number_of_code_slots() { return static_cast<int>(code_slots_.size()); }
#endif

 private:
  std::vector<MaybeObjectSlot> slots_;
#ifdef V8_EXTERNAL_CODE_SPACE
  std::vector<CodeObjectSlot> code_slots_;
#endif
};

// static
void HeapVerifier::VerifyHeap(Heap* heap) {
  HeapVerification verifier(heap);
  verifier.Verify();
}

// static
void HeapVerifier::VerifyReadOnlyHeap(Heap* heap) {
  HeapVerification verifier(heap);
  verifier.VerifyReadOnlyHeap();
}

// static
void HeapVerifier::VerifySharedHeap(Heap* heap, Isolate* initiator) {
  DCHECK(heap->IsShared());
  Isolate* isolate = heap->isolate();

  // Stop all client isolates attached to this isolate.
  GlobalSafepointScope global_safepoint(initiator);

  // Migrate shared isolate to the main thread of the initiator isolate.
  v8::Locker locker(reinterpret_cast<v8::Isolate*>(isolate));
  v8::Isolate::Scope isolate_scope(reinterpret_cast<v8::Isolate*>(isolate));

  DCHECK_NOT_NULL(isolate->global_safepoint());

  // Free all shared LABs to make the shared heap iterable.
  isolate->global_safepoint()->IterateClientIsolates([](Isolate* client) {
    client->heap()->FreeSharedLinearAllocationAreas();
  });

  HeapVerifier::VerifyHeap(heap);
}

// static
void HeapVerifier::VerifyRememberedSetFor(Heap* heap, HeapObject object) {
  MemoryChunk* chunk = MemoryChunk::FromHeapObject(object);
  DCHECK_IMPLIES(chunk->mutex() == nullptr, ReadOnlyHeap::Contains(object));
  // In RO_SPACE chunk->mutex() may be nullptr, so just ignore it.
  base::LockGuard<base::Mutex, base::NullBehavior::kIgnoreIfNull> lock_guard(
      chunk->mutex());
  PtrComprCageBase cage_base(heap->isolate());
  Address start = object.address();
  Address end = start + object.Size(cage_base);

  if (chunk->InSharedHeap() || Heap::InYoungGeneration(object)) {
    CHECK_NULL(chunk->slot_set<OLD_TO_NEW>());
    CHECK_NULL(chunk->typed_slot_set<OLD_TO_NEW>());

    CHECK_NULL(chunk->slot_set<OLD_TO_OLD>());
    CHECK_NULL(chunk->typed_slot_set<OLD_TO_OLD>());
  }

  if (!Heap::InYoungGeneration(object)) {
    std::set<Address> old_to_new;
    std::set<std::pair<SlotType, Address>> typed_old_to_new;
    CollectSlots<OLD_TO_NEW>(chunk, start, end, &old_to_new, &typed_old_to_new);
    OldToNewSlotVerifyingVisitor old_to_new_visitor(
        heap->isolate(), &old_to_new, &typed_old_to_new,
        &heap->ephemeron_remembered_set_);
    object.IterateBody(cage_base, &old_to_new_visitor);

    std::set<Address> old_to_shared;
    std::set<std::pair<SlotType, Address>> typed_old_to_shared;
    CollectSlots<OLD_TO_SHARED>(chunk, start, end, &old_to_shared,
                                &typed_old_to_shared);
    OldToSharedSlotVerifyingVisitor old_to_shared_visitor(
        heap->isolate(), &old_to_shared, &typed_old_to_shared);
    object.IterateBody(cage_base, &old_to_shared_visitor);
  }
  // TODO(v8:11797): Add old to old slot set verification once all weak objects
  // have their own instance types and slots are recorded for all weak fields.
}

// static
void HeapVerifier::VerifyObjectLayoutChange(Heap* heap, HeapObject object,
                                            Map new_map) {
  // Object layout changes are currently not supported on background threads.
  DCHECK_NULL(LocalHeap::Current());

  if (!v8_flags.verify_heap) return;

  PtrComprCageBase cage_base(heap->isolate());

  // Check that Heap::NotifyObjectLayoutChange was called for object transitions
  // that are not safe for concurrent marking.
  // If you see this check triggering for a freshly allocated object,
  // use object->set_map_after_allocation() to initialize its map.
  if (heap->pending_layout_change_object_.is_null()) {
    VerifySafeMapTransition(heap, object, new_map);
  } else {
    DCHECK_EQ(heap->pending_layout_change_object_, object);
    heap->pending_layout_change_object_ = HeapObject();
  }
}

// static
void HeapVerifier::VerifySafeMapTransition(Heap* heap, HeapObject object,
                                           Map new_map) {
  PtrComprCageBase cage_base(heap->isolate());

  if (object.IsJSObject(cage_base)) {
    // Without double unboxing all in-object fields of a JSObject are tagged.
    return;
  }

  if (object.IsString(cage_base) &&
      (new_map == ReadOnlyRoots(heap).thin_string_map() ||
       new_map == ReadOnlyRoots(heap).thin_one_byte_string_map() ||
       new_map == ReadOnlyRoots(heap).shared_thin_string_map() ||
       new_map == ReadOnlyRoots(heap).shared_thin_one_byte_string_map())) {
    // When transitioning a string to ThinString,
    // Heap::NotifyObjectLayoutChange doesn't need to be invoked because only
    // tagged fields are introduced.
    return;
  }

  if (v8_flags.shared_string_table && object.IsString(cage_base) &&
      InstanceTypeChecker::IsInternalizedString(new_map.instance_type())) {
    // In-place internalization does not change a string's fields.
    //
    // When sharing the string table, the setting and re-setting of maps below
    // can race when there are parallel internalization operations, causing
    // DCHECKs to fail.
    return;
  }

  // Check that the set of slots before and after the transition match.
  SlotCollectingVisitor old_visitor;
  object.IterateFast(cage_base, &old_visitor);
  MapWord old_map_word = object.map_word(cage_base, kRelaxedLoad);
  // Temporarily set the new map to iterate new slots.
  object.set_map_word(MapWord::FromMap(new_map), kRelaxedStore);
  SlotCollectingVisitor new_visitor;
  object.IterateFast(cage_base, &new_visitor);
  // Restore the old map.
  object.set_map_word(old_map_word, kRelaxedStore);
  DCHECK_EQ(new_visitor.number_of_slots(), old_visitor.number_of_slots());
  for (int i = 0; i < new_visitor.number_of_slots(); i++) {
    DCHECK_EQ(new_visitor.slot(i), old_visitor.slot(i));
  }
#ifdef V8_EXTERNAL_CODE_SPACE
  DCHECK_EQ(new_visitor.number_of_code_slots(),
            old_visitor.number_of_code_slots());
  for (int i = 0; i < new_visitor.number_of_code_slots(); i++) {
    DCHECK_EQ(new_visitor.code_slot(i), old_visitor.code_slot(i));
  }
#endif  // V8_EXTERNAL_CODE_SPACE
}

}  // namespace internal
}  // namespace v8
#endif  // VERIFY_HEAP
