// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/heap-verifier.h"

#include "include/v8-locker.h"
#include "src/codegen/assembler-inl.h"
#include "src/codegen/reloc-info.h"
#include "src/common/globals.h"
#include "src/execution/isolate.h"
#include "src/heap/array-buffer-sweeper.h"
#include "src/heap/basic-memory-chunk.h"
#include "src/heap/combined-heap.h"
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

namespace {
thread_local HeapObject pending_layout_change_object = HeapObject();
}  // namespace

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

// Visitor class to verify interior pointers in spaces that do not contain
// or care about inter-generational references. All heap object pointers have to
// point into the heap to a location that has a map pointer at its first word.
// Caveat: Heap::Contains is an approximation because it can return true for
// objects in a heap space but above the allocation pointer.
class VerifyPointersVisitor : public ObjectVisitorWithCageBases,
                              public RootVisitor {
 public:
  V8_INLINE explicit VerifyPointersVisitor(Heap* heap)
      : ObjectVisitorWithCageBases(heap), heap_(heap) {}

  void VisitPointers(HeapObject host, ObjectSlot start,
                     ObjectSlot end) override;
  void VisitPointers(HeapObject host, MaybeObjectSlot start,
                     MaybeObjectSlot end) override;
  void VisitCodePointer(Code host, CodeObjectSlot slot) override;
  void VisitCodeTarget(RelocInfo* rinfo) override;
  void VisitEmbeddedPointer(RelocInfo* rinfo) override;

  void VisitRootPointers(Root root, const char* description,
                         FullObjectSlot start, FullObjectSlot end) override;
  void VisitRootPointers(Root root, const char* description,
                         OffHeapObjectSlot start,
                         OffHeapObjectSlot end) override;
  void VisitMapPointer(HeapObject host) override;

 protected:
  V8_INLINE void VerifyHeapObjectImpl(HeapObject heap_object);
  V8_INLINE void VerifyCodeObjectImpl(HeapObject heap_object);

  template <typename TSlot>
  V8_INLINE void VerifyPointersImpl(TSlot start, TSlot end);

  virtual void VerifyPointers(HeapObject host, MaybeObjectSlot start,
                              MaybeObjectSlot end);

  Heap* heap_;
};

void VerifyPointersVisitor::VisitPointers(HeapObject host, ObjectSlot start,
                                          ObjectSlot end) {
  VerifyPointers(host, MaybeObjectSlot(start), MaybeObjectSlot(end));
}

void VerifyPointersVisitor::VisitPointers(HeapObject host,
                                          MaybeObjectSlot start,
                                          MaybeObjectSlot end) {
  VerifyPointers(host, start, end);
}

void VerifyPointersVisitor::VisitCodePointer(Code host, CodeObjectSlot slot) {
  Object maybe_code = slot.load(code_cage_base());
  HeapObject code;
  // The slot might contain smi during Code creation.
  if (maybe_code.GetHeapObject(&code)) {
    VerifyCodeObjectImpl(code);
  } else {
    CHECK(maybe_code.IsSmi());
  }
}

void VerifyPointersVisitor::VisitRootPointers(Root root,
                                              const char* description,
                                              FullObjectSlot start,
                                              FullObjectSlot end) {
  VerifyPointersImpl(start, end);
}

void VerifyPointersVisitor::VisitRootPointers(Root root,
                                              const char* description,
                                              OffHeapObjectSlot start,
                                              OffHeapObjectSlot end) {
  VerifyPointersImpl(start, end);
}

void VerifyPointersVisitor::VisitMapPointer(HeapObject host) {
  VerifyHeapObjectImpl(host.map(cage_base()));
}

void VerifyPointersVisitor::VerifyHeapObjectImpl(HeapObject heap_object) {
  CHECK(IsValidHeapObject(heap_, heap_object));
  CHECK(heap_object.map(cage_base()).IsMap());
}

void VerifyPointersVisitor::VerifyCodeObjectImpl(HeapObject heap_object) {
  CHECK(IsValidCodeObject(heap_, heap_object));
  CHECK(heap_object.map(cage_base()).IsMap());
  CHECK(heap_object.map(cage_base()).instance_type() ==
        INSTRUCTION_STREAM_TYPE);
}

template <typename TSlot>
void VerifyPointersVisitor::VerifyPointersImpl(TSlot start, TSlot end) {
  for (TSlot slot = start; slot < end; ++slot) {
    typename TSlot::TObject object = slot.load(cage_base());
    HeapObject heap_object;
    if (object.GetHeapObject(&heap_object)) {
      VerifyHeapObjectImpl(heap_object);
    } else {
      CHECK(object.IsSmi() || object.IsCleared() ||
            MapWord::IsPacked(object.ptr()));
    }
  }
}

void VerifyPointersVisitor::VerifyPointers(HeapObject host,
                                           MaybeObjectSlot start,
                                           MaybeObjectSlot end) {
  // If this DCHECK fires then you probably added a pointer field
  // to one of objects in DATA_ONLY_VISITOR_ID_LIST. You can fix
  // this by moving that object to POINTER_VISITOR_ID_LIST.
  DCHECK_EQ(ObjectFields::kMaybePointers,
            Map::ObjectFieldsFrom(host.map(cage_base()).visitor_id()));
  VerifyPointersImpl(start, end);
}

void VerifyPointersVisitor::VisitCodeTarget(RelocInfo* rinfo) {
  InstructionStream target =
      InstructionStream::FromTargetAddress(rinfo->target_address());
  VerifyHeapObjectImpl(target);
}

void VerifyPointersVisitor::VisitEmbeddedPointer(RelocInfo* rinfo) {
  VerifyHeapObjectImpl(rinfo->target_object(cage_base()));
}

class VerifyReadOnlyPointersVisitor : public VerifyPointersVisitor {
 public:
  explicit VerifyReadOnlyPointersVisitor(Heap* heap)
      : VerifyPointersVisitor(heap) {}

 protected:
  void VerifyPointers(HeapObject host, MaybeObjectSlot start,
                      MaybeObjectSlot end) override {
    if (!host.is_null()) {
      CHECK(ReadOnlyHeap::Contains(host.map()));
    }
    VerifyPointersVisitor::VerifyPointers(host, start, end);

    for (MaybeObjectSlot current = start; current < end; ++current) {
      HeapObject heap_object;
      if ((*current)->GetHeapObject(&heap_object)) {
        CHECK(ReadOnlyHeap::Contains(heap_object));
      }
    }
  }
};

class VerifySharedHeapObjectVisitor : public VerifyPointersVisitor {
 public:
  explicit VerifySharedHeapObjectVisitor(Heap* heap)
      : VerifyPointersVisitor(heap),
        shared_space_(heap->shared_space()),
        shared_lo_space_(heap->shared_lo_space()) {
    DCHECK_NOT_NULL(shared_space_);
    DCHECK_NOT_NULL(shared_lo_space_);
  }

 protected:
  void VerifyPointers(HeapObject host, MaybeObjectSlot start,
                      MaybeObjectSlot end) override {
    if (!host.is_null()) {
      Map map = host.map();
      CHECK(ReadOnlyHeap::Contains(map) || shared_space_->Contains(map));
    }
    VerifyPointersVisitor::VerifyPointers(host, start, end);

    for (MaybeObjectSlot current = start; current < end; ++current) {
      HeapObject heap_object;
      if ((*current)->GetHeapObject(&heap_object)) {
        CHECK(ReadOnlyHeap::Contains(heap_object) ||
              shared_space_->Contains(heap_object) ||
              shared_lo_space_->Contains(heap_object));
      }
    }
  }

 private:
  SharedSpace* shared_space_;
  SharedLargeObjectSpace* shared_lo_space_;
};

class HeapVerification final : public SpaceVerificationVisitor {
 public:
  explicit HeapVerification(Heap* heap)
      : heap_(heap), isolate_(heap->isolate()), cage_base_(isolate_) {}

  void Verify();
  void VerifyReadOnlyHeap();
  void VerifySharedHeap(Isolate* initiator);

 private:
  void VerifySpace(BaseSpace* space);

  void VerifyPage(const BasicMemoryChunk* chunk) final;
  void VerifyPageDone(const BasicMemoryChunk* chunk) final;

  void VerifyObject(HeapObject object) final;
  void VerifyObjectMap(HeapObject object);
  void VerifyOutgoingPointers(HeapObject object);
  // Verifies OLD_TO_NEW and OLD_TO_SHARED remembered sets for this object.
  void VerifyRememberedSetFor(HeapObject object);

  void VerifyInvalidatedObjectSize();

  ReadOnlySpace* read_only_space() const { return heap_->read_only_space(); }
  NewSpace* new_space() const { return heap_->new_space(); }
  OldSpace* old_space() const { return heap_->old_space(); }
  SharedSpace* shared_space() const { return heap_->shared_space(); }

  CodeSpace* code_space() const { return heap_->code_space(); }
  LargeObjectSpace* lo_space() const { return heap_->lo_space(); }
  SharedLargeObjectSpace* shared_lo_space() const {
    return heap_->shared_lo_space();
  }
  CodeLargeObjectSpace* code_lo_space() const { return heap_->code_lo_space(); }
  NewLargeObjectSpace* new_lo_space() const { return heap_->new_lo_space(); }

  Isolate* isolate() const { return isolate_; }
  Heap* heap() const { return heap_; }

  AllocationSpace current_space_identity() const {
    return *current_space_identity_;
  }

  Heap* const heap_;
  Isolate* const isolate_;
  const PtrComprCageBase cage_base_;
  base::Optional<AllocationSpace> current_space_identity_;
  base::Optional<const BasicMemoryChunk*> current_chunk_;
};

void HeapVerification::Verify() {
  CHECK(heap()->HasBeenSetUp());
  AllowGarbageCollection allow_gc;
  IgnoreLocalGCRequests ignore_gc_requests(heap());
  SafepointKind safepoint_kind = isolate()->is_shared_space_isolate()
                                     ? SafepointKind::kGlobal
                                     : SafepointKind::kIsolate;
  SafepointScope safepoint_scope(isolate(), safepoint_kind);
  HandleScope scope(isolate());

  heap()->MakeHeapIterable();

  // TODO(v8:13257): Currently we don't iterate through the stack conservatively
  // when verifying the heap.
  VerifyPointersVisitor visitor(heap());
  heap()->IterateRoots(&visitor,
                       base::EnumSet<SkipRoot>{SkipRoot::kConservativeStack});

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

  VerifySpace(new_space());

  VerifySpace(old_space());
  VerifySpace(shared_space());
  VerifySpace(code_space());

  VerifySpace(lo_space());
  VerifySpace(new_lo_space());
  VerifySpace(shared_lo_space());
  VerifySpace(code_lo_space());

  isolate()->string_table()->VerifyIfOwnedBy(isolate());

  VerifyInvalidatedObjectSize();

#if DEBUG
  heap()->VerifyCommittedPhysicalMemory();
#endif  // DEBUG
}

void HeapVerification::VerifySpace(BaseSpace* space) {
  if (!space) return;
  current_space_identity_ = space->identity();
  space->Verify(isolate(), this);
  current_space_identity_.reset();
}

void HeapVerification::VerifyPage(const BasicMemoryChunk* chunk) {
  CHECK(!current_chunk_.has_value());
  CHECK(!chunk->IsFlagSet(Page::PAGE_NEW_OLD_PROMOTION));
  CHECK(!chunk->IsFlagSet(Page::PAGE_NEW_NEW_PROMOTION));
  if (V8_SHARED_RO_HEAP_BOOL && chunk->InReadOnlySpace()) {
    CHECK_NULL(chunk->owner());
  } else {
    CHECK_EQ(chunk->heap(), heap());
    CHECK_EQ(chunk->owner()->identity(), current_space_identity());
  }
  current_chunk_ = chunk;
}

void HeapVerification::VerifyPageDone(const BasicMemoryChunk* chunk) {
  CHECK_EQ(chunk, *current_chunk_);
  current_chunk_.reset();
}

void HeapVerification::VerifyObject(HeapObject object) {
  CHECK_EQ(BasicMemoryChunk::FromHeapObject(object), *current_chunk_);

  // Verify object map.
  VerifyObjectMap(object);

  // The object itself should look OK.
  object.ObjectVerify(isolate_);

  // Verify outgoing references.
  VerifyOutgoingPointers(object);

  // Verify remembered set.
  VerifyRememberedSetFor(object);
}

void HeapVerification::VerifyOutgoingPointers(HeapObject object) {
  switch (current_space_identity()) {
    case RO_SPACE: {
      VerifyReadOnlyPointersVisitor visitor(heap());
      object.Iterate(cage_base_, &visitor);
      break;
    }

    case SHARED_SPACE:
    case SHARED_LO_SPACE: {
      VerifySharedHeapObjectVisitor visitor(heap());
      object.Iterate(cage_base_, &visitor);
      break;
    }

    default: {
      VerifyPointersVisitor visitor(heap());
      object.Iterate(cage_base_, &visitor);
      break;
    }
  }
}

void HeapVerification::VerifyObjectMap(HeapObject object) {
  // The first word should be a map, and we expect all map pointers to be
  // in map space or read-only space.
  Map map = object.map(cage_base_);
  CHECK(map.IsMap(cage_base_));
  CHECK(ReadOnlyHeap::Contains(map) || old_space()->Contains(map) ||
        (shared_space() && shared_space()->Contains(map)));

  if (Heap::InYoungGeneration(object)) {
    // The object should not be code or a map.
    CHECK(!object.IsMap(cage_base_));
    CHECK(!object.IsAbstractCode(cage_base_));
  } else if (current_space_identity() == RO_SPACE) {
    CHECK(!object.IsExternalString());
    CHECK(!object.IsJSArrayBuffer());
  }
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
  VerifySpace(read_only_space());
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

  void VisitCodePointer(Code host, CodeObjectSlot slot) override {
    if (ShouldHaveBeenRecorded(
            host, MaybeObject::FromObject(slot.load(code_cage_base())))) {
      CHECK_GT(untyped_->count(slot.address()), 0);
    }
  }

  void VisitCodeTarget(RelocInfo* rinfo) override {
    Object target =
        InstructionStream::FromTargetAddress(rinfo->target_address());
    if (ShouldHaveBeenRecorded(rinfo->instruction_stream(),
                               MaybeObject::FromObject(target))) {
      CHECK(InTypedSet(SlotType::kCodeEntry, rinfo->pc()) ||
            (rinfo->IsInConstantPool() &&
             InTypedSet(SlotType::kConstPoolCodeEntry,
                        rinfo->constant_pool_entry_address())));
    }
  }

  void VisitEmbeddedPointer(RelocInfo* rinfo) override {
    Object target = rinfo->target_object(cage_base());
    if (ShouldHaveBeenRecorded(rinfo->instruction_stream(),
                               MaybeObject::FromObject(target))) {
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
    return target->IsStrongOrWeak() && Heap::InWritableSharedSpace(target) &&
           !Heap::InYoungGeneration(host) && !host.InWritableSharedSpace();
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

  void VisitCodePointer(Code host, CodeObjectSlot slot) override {
    CHECK(V8_EXTERNAL_CODE_SPACE_BOOL);
#ifdef V8_EXTERNAL_CODE_SPACE
    code_slots_.push_back(slot);
#endif
  }

  void VisitCodeTarget(RelocInfo* rinfo) final { UNREACHABLE(); }

  void VisitEmbeddedPointer(RelocInfo* rinfo) override { UNREACHABLE(); }

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

void HeapVerification::VerifyRememberedSetFor(HeapObject object) {
  if (current_space_identity() == RO_SPACE ||
      v8_flags.verify_heap_skip_remembered_set) {
    return;
  }

  MemoryChunk* chunk = MemoryChunk::FromHeapObject(object);

  Address start = object.address();
  Address end = start + object.Size(cage_base_);

  std::set<Address> old_to_new;
  std::set<std::pair<SlotType, Address>> typed_old_to_new;
  CollectSlots<OLD_TO_NEW>(chunk, start, end, &old_to_new, &typed_old_to_new);
  OldToNewSlotVerifyingVisitor old_to_new_visitor(
      isolate(), &old_to_new, &typed_old_to_new,
      &heap()->ephemeron_remembered_set_);
  object.IterateBody(cage_base_, &old_to_new_visitor);

  std::set<Address> old_to_shared;
  std::set<std::pair<SlotType, Address>> typed_old_to_shared;
  CollectSlots<OLD_TO_SHARED>(chunk, start, end, &old_to_shared,
                              &typed_old_to_shared);
  OldToSharedSlotVerifyingVisitor old_to_shared_visitor(
      isolate(), &old_to_shared, &typed_old_to_shared);
  object.IterateBody(cage_base_, &old_to_shared_visitor);

  if (object.InWritableSharedSpace()) {
    CHECK_NULL(chunk->slot_set<OLD_TO_SHARED>());
    CHECK_NULL(chunk->typed_slot_set<OLD_TO_SHARED>());

    CHECK_NULL(chunk->slot_set<OLD_TO_NEW>());
    CHECK_NULL(chunk->typed_slot_set<OLD_TO_NEW>());
  }

  if (Heap::InYoungGeneration(object)) {
    CHECK_NULL(chunk->slot_set<OLD_TO_NEW>());
    CHECK_NULL(chunk->typed_slot_set<OLD_TO_NEW>());

    CHECK_NULL(chunk->slot_set<OLD_TO_OLD>());
    CHECK_NULL(chunk->typed_slot_set<OLD_TO_OLD>());

    CHECK_NULL(chunk->slot_set<OLD_TO_SHARED>());
    CHECK_NULL(chunk->typed_slot_set<OLD_TO_SHARED>());
  }

  // TODO(v8:11797): Add old to old slot set verification once all weak objects
  // have their own instance types and slots are recorded for all weak fields.
}

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
void HeapVerifier::VerifyObjectLayoutChangeIsAllowed(Heap* heap,
                                                     HeapObject object) {
  if (object.InWritableSharedSpace()) {
    // Out of objects in the shared heap, only strings can change layout.
    DCHECK(object.IsString());
    // Shared strings only change layout under GC, never concurrently.
    if (object.IsShared()) {
      Isolate* isolate = heap->isolate();
      Isolate* shared_space_isolate = isolate->is_shared_space_isolate()
                                          ? isolate
                                          : isolate->shared_space_isolate();
      shared_space_isolate->global_safepoint()->AssertActive();
    }
    // Non-shared strings in the shared heap are allowed to change layout
    // outside of GC like strings in non-shared heaps.
  }
}

// static
void HeapVerifier::SetPendingLayoutChangeObject(Heap* heap, HeapObject object) {
  VerifyObjectLayoutChangeIsAllowed(heap, object);
  DCHECK(pending_layout_change_object.is_null());
  pending_layout_change_object = object;
}

// static
void HeapVerifier::VerifyObjectLayoutChange(Heap* heap, HeapObject object,
                                            Map new_map) {
  // Object layout changes are currently not supported on background threads.
  DCHECK_NULL(LocalHeap::Current());

  if (!v8_flags.verify_heap) return;

  VerifyObjectLayoutChangeIsAllowed(heap, object);

  PtrComprCageBase cage_base(heap->isolate());

  // Check that Heap::NotifyObjectLayoutChange was called for object transitions
  // that are not safe for concurrent marking.
  // If you see this check triggering for a freshly allocated object,
  // use object->set_map_after_allocation() to initialize its map.
  if (pending_layout_change_object.is_null()) {
    VerifySafeMapTransition(heap, object, new_map);
  } else {
    DCHECK_EQ(pending_layout_change_object, object);
    pending_layout_change_object = HeapObject();
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
      new_map == ReadOnlyRoots(heap).thin_string_map()) {
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
  object.set_map_word(new_map, kRelaxedStore);
  SlotCollectingVisitor new_visitor;
  object.IterateFast(cage_base, &new_visitor);
  // Restore the old map.
  object.set_map_word(old_map_word.ToMap(), kRelaxedStore);
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
