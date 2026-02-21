// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/scavenger.h"

#include <algorithm>
#include <atomic>
#include <optional>
#include <unordered_map>

#include "src/base/utils/random-number-generator.h"
#include "src/common/globals.h"
#include "src/execution/isolate-inl.h"
#include "src/flags/flags.h"
#include "src/handles/global-handles.h"
#include "src/heap/array-buffer-sweeper.h"
#include "src/heap/concurrent-marking.h"
#include "src/heap/conservative-stack-visitor-inl.h"
#include "src/heap/ephemeron-remembered-set.h"
#include "src/heap/evacuation-allocator-inl.h"
#include "src/heap/gc-tracer-inl.h"
#include "src/heap/gc-tracer.h"
#include "src/heap/heap-inl.h"
#include "src/heap/heap-layout-inl.h"
#include "src/heap/heap-layout.h"
#include "src/heap/heap-visitor-inl.h"
#include "src/heap/heap.h"
#include "src/heap/large-page-metadata-inl.h"
#include "src/heap/mark-compact-inl.h"
#include "src/heap/mark-compact.h"
#include "src/heap/memory-chunk-layout.h"
#include "src/heap/memory-chunk.h"
#include "src/heap/mutable-page-metadata-inl.h"
#include "src/heap/mutable-page-metadata.h"
#include "src/heap/new-spaces.h"
#include "src/heap/page-metadata.h"
#include "src/heap/pretenuring-handler-inl.h"
#include "src/heap/remembered-set-inl.h"
#include "src/heap/slot-set.h"
#include "src/heap/sweeper.h"
#include "src/heap/zapping.h"
#include "src/objects/casting-inl.h"
#include "src/objects/data-handler-inl.h"
#include "src/objects/embedder-data-array-inl.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/js-objects.h"
#include "src/objects/js-weak-refs.h"
#include "src/objects/map.h"
#include "src/objects/objects-body-descriptors-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/slots-inl.h"
#include "src/utils/utils-inl.h"

namespace v8 {
namespace internal {

namespace {

template <RememberedSetType kType>
void AddToRememberedSet(const Heap* heap, const Tagged<HeapObject> host,
                        Address slot) {
  MemoryChunk* chunk = MemoryChunk::FromHeapObject(host);
  MutablePageMetadata* page =
      MutablePageMetadata::cast(chunk->Metadata(heap->isolate()));
  RememberedSet<kType>::template Insert<AccessMode::ATOMIC>(
      page, chunk->Offset(slot));
}

bool HeapObjectWillBeOld(const Heap* heap, Tagged<HeapObject> object) {
  if (!HeapLayout::InYoungGeneration(object)) {
    return true;
  }
  if (HeapLayout::InAnyLargeSpace(object)) {
    return true;
  }
  if (HeapLayout::IsSelfForwarded(object) &&
      MemoryChunkMetadata::FromHeapObject(heap->isolate(), object)
          ->will_be_promoted()) {
    return true;
  }
  return false;
}

}  // namespace

template <typename ConcreteVisitor, ObjectAge kExpectedObjectAge>
class ScavengerObjectVisitorBase : public NewSpaceVisitor<ConcreteVisitor> {
  using Base = NewSpaceVisitor<ConcreteVisitor>;

 public:
  explicit ScavengerObjectVisitorBase(Scavenger* scavenger)
      : Base(scavenger->heap()->isolate()), scavenger_(scavenger) {}

  V8_INLINE static constexpr bool ShouldUseUncheckedCast() { return true; }
  V8_INLINE static constexpr bool UsePrecomputedObjectSize() { return true; }

  V8_INLINE void VisitPointers(Tagged<HeapObject> host, ObjectSlot start,
                               ObjectSlot end) override {
    return VisitPointersImpl(host, start, end);
  }

  V8_INLINE void VisitPointers(Tagged<HeapObject> host, MaybeObjectSlot start,
                               MaybeObjectSlot end) override {
    return VisitPointersImpl(host, start, end);
  }

  V8_INLINE void VisitCustomWeakPointers(Tagged<HeapObject> host,
                                         ObjectSlot start,
                                         ObjectSlot end) override {
    DCHECK(GCAwareObjectTypeCheck<JSWeakRef>(host, scavenger_->heap_) ||
           GCAwareObjectTypeCheck<WeakCell>(host, scavenger_->heap_) ||
           GCAwareObjectTypeCheck<JSFinalizationRegistry>(host,
                                                          scavenger_->heap_));
    if (v8_flags.handle_weak_ref_weakly_in_minor_gc) {
      return;
    }
    // Strongify the weak pointers.
    VisitPointersImpl(host, start, end);
  }

  V8_INLINE void VisitExternalPointer(Tagged<HeapObject> host,
                                      ExternalPointerSlot slot) override {
#ifdef V8_COMPRESS_POINTERS
    DCHECK(!slot.tag_range().IsEmpty());
    DCHECK(!IsSharedExternalPointerType(slot.tag_range()));
    DCHECK_IMPLIES(kExpectedObjectAge == ObjectAge::kYoung,
                   HeapLayout::InYoungGeneration(host));

    // TODO(chromium:337580006): Remove when pointer compression always uses
    // EPT.
    if (!slot.HasExternalPointerHandle()) return;

    ExternalPointerHandle handle = slot.Relaxed_LoadHandle();
    Heap* heap = scavenger_->heap();
    ExternalPointerTable& table = heap->isolate()->external_pointer_table();
    if constexpr (kExpectedObjectAge == ObjectAge::kYoung) {
      // For survivor objects, mark their EPT entries when they are
      // copied. Scavenger then sweeps the young EPT space at the end of
      // collection, reclaiming unmarked EPT entries.
      table.Mark(heap->young_external_pointer_space(), handle, slot.address());
    } else {
      // When promoting, we just evacuate the entry from new to old space.
      // Usually the entry will be unmarked, unless the slot was initialized
      // since the last GC (external pointer tags have the mark bit set), in
      // which case it may be marked already. In any case, transfer the color
      // from new to old EPT space.
      table.Evacuate(heap->young_external_pointer_space(),
                     heap->old_external_pointer_space(), handle, slot.address(),
                     ExternalPointerTable::EvacuateMarkMode::kTransferMark);
    }
#endif  // V8_COMPRESS_POINTERS
  }

  V8_INLINE size_t VisitJSArrayBuffer(Tagged<Map> map,
                                      Tagged<JSArrayBuffer> object,
                                      MaybeObjectSize) {
    if constexpr (kExpectedObjectAge == ObjectAge::kYoung) {
      object->YoungMarkExtension();
    } else {
      object->YoungMarkExtensionPromoted();
    }
    int size = JSArrayBuffer::BodyDescriptor::SizeOf(map, object);
    JSArrayBuffer::BodyDescriptor::IterateBody(map, object, size, this);
    return size;
  }

  V8_INLINE size_t VisitJSWeakRef(Tagged<Map> map, Tagged<JSWeakRef> object,
                                  MaybeObjectSize maybe_size) {
    scavenger_->RecordJSWeakRefIfNeeded<kExpectedObjectAge>(object);
    return Base::VisitJSWeakRef(map, object, maybe_size);
  }
  V8_INLINE size_t VisitWeakCell(Tagged<Map> map, Tagged<WeakCell> object,
                                 MaybeObjectSize maybe_size) {
    scavenger_->RecordWeakCellIfNeeded<kExpectedObjectAge>(object);
    return Base::VisitWeakCell(map, object, maybe_size);
  }

  V8_INLINE static constexpr bool CanEncounterFillerOrFreeSpace() {
    return false;
  }

  template <typename T>
  static V8_INLINE Tagged<T> Cast(Tagged<HeapObject> object, const Heap* heap) {
    return GCSafeCast<T>(object, heap);
  }

 protected:
  template <typename TSlot>
  V8_INLINE void VisitPointersImpl(Tagged<HeapObject> host, TSlot start,
                                   TSlot end) {
    using THeapObjectSlot = typename TSlot::THeapObjectSlot;
    CheckObjectAge(host);
    for (TSlot slot = start; slot < end; ++slot) {
      const std::optional<Tagged<Object>> optional_object =
          this->GetObjectFilterReadOnlyAndSmiFast(slot);
      if (!optional_object) {
        continue;
      }
      typename TSlot::TObject object = *optional_object;
      Tagged<HeapObject> heap_object;
      // Treat weak references as strong.
      if (object.GetHeapObject(&heap_object)) {
        static_cast<ConcreteVisitor*>(this)->HandleSlot(
            host, THeapObjectSlot(slot), heap_object);
      }
    }
  }

  void CheckObjectAge(Tagged<HeapObject> object) {
    DCHECK_EQ(kExpectedObjectAge == ObjectAge::kOld,
              HeapObjectWillBeOld(scavenger_->heap_, object));
  }

  Scavenger* const scavenger_;
};

class ScavengerCopiedObjectVisitor final
    : public ScavengerObjectVisitorBase<ScavengerCopiedObjectVisitor,
                                        ObjectAge::kYoung> {
 public:
  explicit ScavengerCopiedObjectVisitor(Scavenger* scavenger)
      : ScavengerObjectVisitorBase(scavenger) {}

  V8_INLINE size_t VisitEphemeronHashTable(Tagged<Map> map,
                                           Tagged<EphemeronHashTable> table,
                                           MaybeObjectSize) {
    // Register table with the scavenger, so it can take care of the weak keys
    // later. This allows to only iterate the tables' values, which are treated
    // as strong independently of whether the key is live.
    scavenger_->AddEphemeronHashTable(table);
    for (InternalIndex i : table->IterateEntries()) {
      ObjectSlot value_slot =
          table->RawFieldOfElementAt(EphemeronHashTable::EntryToValueIndex(i));
      VisitPointer(table, value_slot);
    }

    return table->SafeSizeFromMap(map).value();
  }

 private:
  template <typename THeapObjectSlot>
  V8_INLINE void HandleSlot(Tagged<HeapObject> host, THeapObjectSlot slot,
                            Tagged<HeapObject> heap_object) {
    DCHECK(!HeapObjectWillBeOld(scavenger_->heap_, host));
    if (HeapLayout::InYoungGeneration(heap_object)) {
      scavenger_->ScavengeObject(slot, heap_object);
    }
  }

  friend class ScavengerObjectVisitorBase<ScavengerCopiedObjectVisitor,
                                          ObjectAge::kYoung>;
  friend class Scavenger;
};

class ScavengerPromotedObjectVisitor final
    : public ScavengerObjectVisitorBase<ScavengerPromotedObjectVisitor,
                                        ObjectAge::kOld> {
 public:
  explicit ScavengerPromotedObjectVisitor(Scavenger* scavenger)
      : ScavengerObjectVisitorBase(scavenger) {}

  V8_INLINE void VisitEphemeron(Tagged<HeapObject> obj, int entry,
                                ObjectSlot key, ObjectSlot value) final {
    DCHECK(HeapLayout::IsSelfForwarded(obj) || IsEphemeronHashTable(obj));
    VisitPointer(obj, value);

    if (HeapLayout::InYoungGeneration(*key)) {
      // We cannot check the map here, as it might be a large object.
      scavenger_->RememberPromotedEphemeron(
          UncheckedCast<EphemeronHashTable>(obj), entry);
    } else {
      VisitPointer(obj, key);
    }
  }

 private:
  template <typename THeapObjectSlot>
  V8_INLINE void HandleSlot(Tagged<HeapObject> host, THeapObjectSlot slot,
                            Tagged<HeapObject> target) {
    static_assert(
        std::is_same_v<THeapObjectSlot, FullHeapObjectSlot> ||
            std::is_same_v<THeapObjectSlot, HeapObjectSlot>,
        "Only FullHeapObjectSlot and HeapObjectSlot are expected here");
    DCHECK(HeapObjectWillBeOld(scavenger_->heap_, host));
    scavenger_->SynchronizePageAccess(target);

    if (Heap::InFromPage(target)) {
      SlotCallbackResult result = scavenger_->ScavengeObject(slot, target);
      bool success = (*slot).GetHeapObject(&target);
      USE(success);
      DCHECK(success);

      if (result == KEEP_SLOT) {
        SLOW_DCHECK(IsHeapObject(target));
        // Sweeper is stopped during scavenge, so we can directly
        // insert into its remembered set here.
        AddToRememberedSet<OLD_TO_NEW>(heap_, host, slot.address());
      }
      DCHECK(!MarkCompactCollector::IsOnEvacuationCandidate(target));
    }

    if (HeapLayout::InWritableSharedSpace(target)) {
      AddToRememberedSet<OLD_TO_SHARED>(heap_, host, slot.address());
    }
  }

  friend class ScavengerObjectVisitorBase<ScavengerPromotedObjectVisitor,
                                          ObjectAge::kOld>;
  friend class Scavenger;
};

namespace {

V8_INLINE bool IsUnscavengedHeapObject(Tagged<Object> object) {
  return Heap::InFromPage(object) && !Cast<HeapObject>(object)
                                          ->map_word(kRelaxedLoad)
                                          .IsForwardingAddress();
}

bool IsUnscavengedHeapObjectSlot(Heap* heap, FullObjectSlot p) {
  return IsUnscavengedHeapObject(*p);
}

}  // namespace

ScavengerCollector::JobTask::JobTask(
    ScavengerCollector* collector,
    std::vector<std::unique_ptr<Scavenger>>* scavengers,
    std::vector<std::pair<ParallelWorkItem, MutablePageMetadata*>>
        old_to_new_chunks,
    const Scavenger::ScavengedObjectList& copied_list,
    const Scavenger::ScavengedObjectList& promoted_list)
    : collector_(collector),
      scavengers_(scavengers),
      old_to_new_chunks_(std::move(old_to_new_chunks)),
      remaining_memory_chunks_(old_to_new_chunks_.size()),
      generator_(old_to_new_chunks_.size()),
      copied_list_(copied_list),
      promoted_list_(promoted_list),
      trace_id_(reinterpret_cast<uint64_t>(this) ^
                collector_->heap_->tracer()->CurrentEpoch(
                    GCTracer::Scope::SCAVENGER)) {}

void ScavengerCollector::JobTask::Run(JobDelegate* delegate) {
  DCHECK_LT(delegate->GetTaskId(), scavengers_->size());
  // Set the current isolate such that trusted pointer tables etc are
  // available and the cage base is set correctly for multi-cage mode.
  SetCurrentIsolateScope isolate_scope(collector_->heap_->isolate());

  collector_->estimate_concurrency_.fetch_add(1, std::memory_order_relaxed);

  Scavenger* scavenger = (*scavengers_)[delegate->GetTaskId()].get();
  if (delegate->IsJoiningThread()) {
    TRACE_GC_WITH_FLOW(collector_->heap_->tracer(),
                       GCTracer::Scope::SCAVENGER_SCAVENGE_PARALLEL, trace_id_,
                       TRACE_EVENT_FLAG_FLOW_IN);
    ProcessItems(delegate, scavenger);
  } else {
    TRACE_GC_EPOCH_WITH_FLOW(
        collector_->heap_->tracer(),
        GCTracer::Scope::SCAVENGER_BACKGROUND_SCAVENGE_PARALLEL,
        ThreadKind::kBackground, trace_id_, TRACE_EVENT_FLAG_FLOW_IN);
    ProcessItems(delegate, scavenger);
  }
}

size_t ScavengerCollector::JobTask::GetMaxConcurrency(
    size_t worker_count) const {
  // We need to account for local segments held by worker_count in addition to
  // GlobalPoolSize() of copied_list_, pinned_list_ and promoted_list_.
  size_t wanted_num_workers = std::max<size_t>(
      remaining_memory_chunks_.load(std::memory_order_relaxed),
      worker_count + copied_list_.Size() + promoted_list_.Size());
  if (!collector_->heap_->ShouldUseBackgroundThreads() ||
      collector_->heap_->ShouldOptimizeForBattery()) {
    return std::min<size_t>(wanted_num_workers, 1);
  }
  return std::min<size_t>(scavengers_->size(), wanted_num_workers);
}

void ScavengerCollector::JobTask::ProcessItems(JobDelegate* delegate,
                                               Scavenger* scavenger) {
  double scavenging_time = 0.0;
  {
    TimedScope scope(&scavenging_time);

    ConcurrentScavengePages(scavenger);
    scavenger->Process(delegate);
  }
  if (V8_UNLIKELY(v8_flags.trace_parallel_scavenge)) {
    PrintIsolate(collector_->heap_->isolate(),
                 "scavenge[%p]: time=%.2f copied=%zu promoted=%zu\n",
                 static_cast<void*>(this), scavenging_time,
                 scavenger->bytes_copied(), scavenger->bytes_promoted());
  }
}

void ScavengerCollector::JobTask::ConcurrentScavengePages(
    Scavenger* scavenger) {
  while (remaining_memory_chunks_.load(std::memory_order_relaxed) > 0) {
    std::optional<size_t> index = generator_.GetNext();
    if (!index) {
      return;
    }
    for (size_t i = *index; i < old_to_new_chunks_.size(); ++i) {
      auto& work_item = old_to_new_chunks_[i];
      if (!work_item.first.TryAcquire()) {
        break;
      }
      scavenger->ScavengePage(work_item.second);
      if (remaining_memory_chunks_.fetch_sub(1, std::memory_order_relaxed) <=
          1) {
        return;
      }
    }
  }
}

ScavengerCollector::ScavengerCollector(Heap* heap)
    : isolate_(heap->isolate()), heap_(heap), quarantined_page_sweeper_(heap) {}

namespace {

// Helper class for updating weak global handles. There's no additional scavenge
// processing required here as this phase runs after actual scavenge.
class GlobalHandlesWeakRootsUpdatingVisitor final : public RootVisitor {
 public:
  void VisitRootPointer(Root root, const char* description,
                        FullObjectSlot p) final {
    UpdatePointer(p);
  }
  void VisitRootPointers(Root root, const char* description,
                         FullObjectSlot start, FullObjectSlot end) final {
    for (FullObjectSlot p = start; p < end; ++p) {
      UpdatePointer(p);
    }
  }

 private:
  void UpdatePointer(FullObjectSlot p) {
    Tagged<Object> object = *p;
    DCHECK(!HasWeakHeapObjectTag(object));
    // The object may be in the old generation as global handles over
    // approximates the list of young nodes. This checks also bails out for
    // Smis.
    if (!HeapLayout::InYoungGeneration(object)) {
      return;
    }

    Tagged<HeapObject> heap_object = Cast<HeapObject>(object);
    // TODO(chromium:1336158): Turn the following CHECKs into DCHECKs after
    // flushing out potential issues.
    CHECK(Heap::InFromPage(heap_object));
    MapWord first_word = heap_object->map_word(kRelaxedLoad);
    CHECK(first_word.IsForwardingAddress());
    Tagged<HeapObject> dest = first_word.ToForwardingAddress(heap_object);
    if (heap_object == dest) {
      DCHECK(
          HeapLayout::InAnyLargeSpace(heap_object) ||
          MemoryChunkMetadata::FromHeapObject(Isolate::Current(), heap_object)
              ->is_quarantined());
      return;
    }
    UpdateHeapObjectReferenceSlot(FullHeapObjectSlot(p), dest);
    // The destination object should be in the "to" space. However, it could
    // also be a large string if the original object was a shortcut candidate.
    DCHECK_IMPLIES(
        HeapLayout::InYoungGeneration(dest),
        Heap::InToPage(dest) ||
            (HeapLayout::InAnyLargeSpace(dest) && Heap::InFromPage(dest) &&
             dest->map_word(kRelaxedLoad).IsForwardingAddress()));
  }
};

}  // namespace

// Remove this crashkey after chromium:1010312 is fixed.
class V8_NODISCARD ScopedFullHeapCrashKey {
 public:
  explicit ScopedFullHeapCrashKey(Isolate* isolate) : isolate_(isolate) {
    isolate_->AddCrashKey(v8::CrashKeyId::kDumpType, "heap");
  }
  ~ScopedFullHeapCrashKey() {
    isolate_->AddCrashKey(v8::CrashKeyId::kDumpType, "");
  }

 private:
  Isolate* isolate_ = nullptr;
};

namespace {

// A conservative stack scanning visitor implementation that:
// 1) Filters out non-young objects, and
// 2) Use the marking bitmap as a temporary object start bitmap.
class YoungGenerationConservativeStackVisitor
    : public ConservativeStackVisitorBase<
          YoungGenerationConservativeStackVisitor> {
 public:
  YoungGenerationConservativeStackVisitor(Isolate* isolate,
                                          RootVisitor* root_visitor)
      : ConservativeStackVisitorBase(isolate, root_visitor), isolate_(isolate) {
    DCHECK_NE(isolate->heap()->ConservativeStackScanningModeForMinorGC(),
              Heap::StackScanMode::kNone);
    DCHECK(!v8_flags.minor_ms);
    DCHECK(!v8_flags.sticky_mark_bits);
    DCHECK(std::all_of(
        isolate_->heap()->semi_space_new_space()->to_space().begin(),
        isolate_->heap()->semi_space_new_space()->to_space().end(),
        [](const PageMetadata* page) {
          return page->marking_bitmap()->IsClean();
        }));
    DCHECK(std::all_of(
        isolate_->heap()->semi_space_new_space()->from_space().begin(),
        isolate_->heap()->semi_space_new_space()->from_space().end(),
        [](const PageMetadata* page) {
          return page->marking_bitmap()->IsClean();
        }));
  }

  ~YoungGenerationConservativeStackVisitor() {
    DCHECK(std::all_of(
        isolate_->heap()->semi_space_new_space()->to_space().begin(),
        isolate_->heap()->semi_space_new_space()->to_space().end(),
        [](const PageMetadata* page) {
          return page->marking_bitmap()->IsClean();
        }));
    for (PageMetadata* page :
         isolate_->heap()->semi_space_new_space()->from_space()) {
      page->marking_bitmap()->Clear<AccessMode::NON_ATOMIC>();
    }
  }

 private:
  static constexpr bool kOnlyVisitMainV8Cage [[maybe_unused]] = true;

  static bool FilterPage(const MemoryChunk* chunk) {
    return chunk->IsFromPage();
  }

  static bool FilterLargeObject(Tagged<HeapObject> object, MapWord map_word) {
    DCHECK_EQ(map_word, object->map_word(kRelaxedLoad));
    return !HeapLayout::IsSelfForwarded(object, map_word);
  }

  static bool FilterNormalObject(Tagged<HeapObject> object, MapWord map_word,
                                 MarkingBitmap* bitmap) {
    DCHECK_EQ(map_word, object->map_word(kRelaxedLoad));
    if (map_word.IsForwardingAddress()) {
      DCHECK(HeapLayout::IsSelfForwarded(object));
      DCHECK(
          MarkingBitmap::MarkBitFromAddress(bitmap, object->address()).Get());
      return false;
    }
    MarkingBitmap::MarkBitFromAddress(bitmap, object->address())
        .Set<AccessMode::NON_ATOMIC>();
    return true;
  }

  static void HandleObjectFound(Tagged<HeapObject> object, size_t object_size,
                                MarkingBitmap* bitmap) {
    DCHECK_EQ(object_size, object->Size());
    Address object_address = object->address();
    if (object_address + object_size <
        PageMetadata::FromHeapObject(object)->area_end()) {
      MarkingBitmap::MarkBitFromAddress(bitmap, object_address + object_size)
          .Set<AccessMode::NON_ATOMIC>();
    }
  }

  Isolate* const isolate_;

  friend class ConservativeStackVisitorBase<
      YoungGenerationConservativeStackVisitor>;
};

template <typename ConcreteVisitor>
class ObjectPinningVisitorBase : public RootVisitor {
 public:
  ObjectPinningVisitorBase(const Heap* heap, Scavenger& scavenger,
                           ScavengerCollector::PinnedObjects& pinned_objects)
      : RootVisitor(),
        heap_(heap),
        scavenger_(scavenger),
        pinned_objects_(pinned_objects) {}

  void VisitRootPointer(Root root, const char* description,
                        FullObjectSlot p) final {
    DCHECK(root == Root::kStackRoots || root == Root::kHandleScope);
    static_cast<ConcreteVisitor*>(this)->HandlePointer(p);
  }

  void VisitRootPointers(Root root, const char* description,
                         FullObjectSlot start, FullObjectSlot end) final {
    DCHECK(root == Root::kStackRoots || root == Root::kHandleScope);
    for (FullObjectSlot p = start; p < end; ++p) {
      static_cast<ConcreteVisitor*>(this)->HandlePointer(p);
    }
  }

 protected:
  void HandleHeapObject(Tagged<HeapObject> object) {
    DCHECK(!HasWeakHeapObjectTag(object));
    DCHECK(!MapWord::IsPacked(object.ptr()));
    DCHECK(!HeapLayout::IsSelfForwarded(object));
    if (IsAllocationMemento(object)) {
      // Don't pin allocation mementos since they should not survive a GC.
      return;
    }
    if (scavenger_.PromoteIfLargeObject(object)) {
      // Large objects are not moved and thus don't require pinning. Instead,
      // we scavenge large pages eagerly to keep them from being reclaimed (if
      // the page is only reachable from stack).
      return;
    }
    MemoryChunk* chunk = MemoryChunk::FromHeapObject(object);
    MutablePageMetadata* metadata =
        MutablePageMetadata::cast(chunk->Metadata(heap_->isolate()));
    DCHECK(!metadata->is_large());
    DCHECK(HeapLayout::InYoungGeneration(object));
    DCHECK(Heap::InFromPage(object));
    Address object_address = object.address();
    MapWord map_word = object->map_word(kRelaxedLoad);
    DCHECK(!map_word.IsForwardingAddress());
    DCHECK(std::all_of(
        pinned_objects_.begin(), pinned_objects_.end(),
        [object_address](ScavengerCollector::PinnedObjectEntry& entry) {
          return entry.address != object_address;
        }));
    const auto object_size = object->SafeSizeFromMap(map_word.ToMap());
    DCHECK_LT(0, object_size.value());
    pinned_objects_.push_back(
        {object_address, map_word, object_size, metadata});
    if (!metadata->is_quarantined()) {
      metadata->set_is_quarantined(true);
      if (v8_flags.scavenger_promote_quarantined_pages &&
          heap_->semi_space_new_space()->ShouldPageBePromoted(chunk)) {
        metadata->set_will_be_promoted(true);
      }
    }
    scavenger_.PinAndPushObject(metadata, object, map_word);
  }

 private:
  const Heap* const heap_;
  Scavenger& scavenger_;
  ScavengerCollector::PinnedObjects& pinned_objects_;
};

class ConservativeObjectPinningVisitor final
    : public ObjectPinningVisitorBase<ConservativeObjectPinningVisitor> {
 public:
  ConservativeObjectPinningVisitor(
      const Heap* heap, Scavenger& scavenger,
      ScavengerCollector::PinnedObjects& pinned_objects)
      : ObjectPinningVisitorBase<ConservativeObjectPinningVisitor>(
            heap, scavenger, pinned_objects) {}

 private:
  void HandlePointer(FullObjectSlot p) {
    HandleHeapObject(Cast<HeapObject>(*p));
  }

  friend class ObjectPinningVisitorBase<ConservativeObjectPinningVisitor>;
};

class PreciseObjectPinningVisitor final
    : public ObjectPinningVisitorBase<PreciseObjectPinningVisitor> {
 public:
  PreciseObjectPinningVisitor(const Heap* heap, Scavenger& scavenger,
                              ScavengerCollector::PinnedObjects& pinned_objects)
      : ObjectPinningVisitorBase<PreciseObjectPinningVisitor>(heap, scavenger,
                                                              pinned_objects) {}

 private:
  void HandlePointer(FullObjectSlot p) {
    Tagged<Object> object = *p;
    if (!object.IsHeapObject()) {
      return;
    }
    Tagged<HeapObject> heap_object = Cast<HeapObject>(object);
    if (!MemoryChunk::FromHeapObject(heap_object)->IsFromPage()) {
      return;
    }
    if (HeapLayout::IsSelfForwarded(heap_object)) {
      return;
    }
    HandleHeapObject(heap_object);
  }

  friend class ObjectPinningVisitorBase<PreciseObjectPinningVisitor>;
};

// A visitor for treating precise references conservatively (by passing them to
// the conservative stack visitor). This visitor is used for streesing object
// pinning in Scavenger.
class TreatConservativelyVisitor final : public RootVisitor {
 public:
  TreatConservativelyVisitor(YoungGenerationConservativeStackVisitor* v,
                             Heap* heap)
      : RootVisitor(),
        stack_visitor_(v),
        rng_(heap->isolate()->fuzzer_rng()),
        stressing_threshold_(
            v8_flags.stress_scavenger_conservative_object_pinning_random
                ? rng_->NextDouble()
                : 0) {}

  void VisitRootPointer(Root root, const char* description,
                        FullObjectSlot p) final {
    HandlePointer(p);
  }

  void VisitRootPointers(Root root, const char* description,
                         FullObjectSlot start, FullObjectSlot end) final {
    for (FullObjectSlot p = start; p < end; ++p) {
      HandlePointer(p);
    }
  }

 private:
  void HandlePointer(FullObjectSlot p) {
    if (rng_->NextDouble() < stressing_threshold_) {
      return;
    }
    Tagged<Object> object = *p;
    stack_visitor_->VisitPointer(reinterpret_cast<void*>(object.ptr()));
  }

  YoungGenerationConservativeStackVisitor* const stack_visitor_;
  base::RandomNumberGenerator* const rng_;
  double stressing_threshold_;
};

void RestorePinnedObjects(
    SemiSpaceNewSpace& new_space,
    const ScavengerCollector::PinnedObjects& pinned_objects) {
  // Restore the maps of quarantined objects. We use the iteration over
  // quarantined objects to split them based on pages. This will be used below
  // for sweeping the quarantined pages (since there are no markbits).
  DCHECK_EQ(0, new_space.QuarantinedPageCount());
  size_t quarantined_objects_size = 0;
  for (const auto& [object_address, map_word, object_size, metadata] :
       pinned_objects) {
    DCHECK(metadata->Contains(object_address));
    DCHECK(!map_word.IsForwardingAddress());
    Tagged<HeapObject> object = HeapObject::FromAddress(object_address);
    DCHECK(HeapLayout::IsSelfForwarded(object));
    object->set_map_word(map_word.ToMap(), kRelaxedStore);
    DCHECK(!HeapLayout::IsSelfForwarded(object));
    DCHECK(metadata->is_quarantined());
    if (!metadata->will_be_promoted()) {
      quarantined_objects_size += object_size.value();
    }
  }
  new_space.SetQuarantinedSize(quarantined_objects_size);
}

void QuarantinePinnedPages(SemiSpaceNewSpace& new_space) {
  PageMetadata* next_page = new_space.from_space().first_page();
  while (next_page) {
    PageMetadata* current_page = next_page;
    next_page = current_page->next_page();
#ifdef DEBUG
    MemoryChunk* chunk = current_page->Chunk();
#endif  // DEBUG
    DCHECK(chunk->IsFromPage());
    if (!current_page->is_quarantined()) {
      continue;
    }
    if (current_page->will_be_promoted()) {
      // free list categories will be relinked by the quarantined page sweeper
      // after sweeping is done.
      new_space.PromotePageToOldSpace(current_page,
                                      FreeMode::kDoNotLinkCategory);
      DCHECK(!chunk->InYoungGeneration());
    } else {
      new_space.MoveQuarantinedPage(current_page);
      DCHECK(!chunk->IsFromPage());
      DCHECK(chunk->IsToPage());
    }
    DCHECK(current_page->marking_bitmap()->IsClean());
    DCHECK(!chunk->IsFromPage());
    DCHECK(!current_page->is_quarantined());
    DCHECK(!current_page->will_be_promoted());
  }
}

}  // namespace

ScavengerCollector::QuarantinedPageSweeper::JobTask::JobTask(
    Heap* heap, const PinnedObjects&& pinned_objects)
    : heap_(heap),
      trace_id_(reinterpret_cast<uint64_t>(this) ^
                heap_->tracer()->CurrentEpoch(GCTracer::Scope::SCAVENGER)),
      should_zap_(heap::ShouldZapGarbage()),
      pinned_objects_(std::move(pinned_objects)) {
  DCHECK(!pinned_objects.empty());
}

void ScavengerCollector::QuarantinedPageSweeper::JobTask::Run(
    JobDelegate* delegate) {
#ifdef V8_COMPRESS_POINTERS_IN_MULTIPLE_CAGES
  SetCurrentIsolateScope current_isolate_scope(heap_->isolate());
#endif  // V8_COMPRESS_POINTERS_IN_MULTIPLE_CAGES
  DCHECK_IMPLIES(
      delegate->IsJoiningThread(),
      heap_->IsMainThread() ||
          (!heap_->isolate()->is_shared_space_isolate() &&
           heap_->isolate()->shared_space_isolate()->heap()->IsMainThread()));
  const bool is_main_thread =
      delegate->IsJoiningThread() && heap_->IsMainThread();
  TRACE_GC_EPOCH_WITH_FLOW(
      heap_->tracer(),
      GCTracer::Scope::SCAVENGER_BACKGROUND_QUARANTINED_PAGE_SWEEPING,
      is_main_thread ? ThreadKind::kMain : ThreadKind::kBackground, trace_id(),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  DCHECK(!is_done_.load(std::memory_order_relaxed));
  DCHECK(!pinned_objects_.empty());
  if (pinned_object_per_page_.empty()) {
    // Populate the per page map.
    for (const PinnedObjectEntry& entry : pinned_objects_) {
      DCHECK(!HeapLayout::IsSelfForwarded(
          HeapObject::FromAddress(entry.address), heap_->isolate()));
      MemoryChunk* chunk = MemoryChunk::FromAddress(entry.address);
      DCHECK(!chunk->Metadata(heap_->isolate())->is_quarantined());
      ObjectsAndSizes& objects_for_page = pinned_object_per_page_[chunk];
      DCHECK(!std::any_of(objects_for_page.begin(), objects_for_page.end(),
                          [entry](auto& object_and_size) {
                            return object_and_size.first == entry.address;
                          }));
      objects_for_page.emplace_back(entry.address, entry.size.value());
    }
    // Initialize the iterator.
    next_page_iterator_ = pinned_object_per_page_.begin();
    DCHECK_NE(next_page_iterator_, pinned_object_per_page_.end());
  }
  // Sweep all quarantined pages.
  while (next_page_iterator_ != pinned_object_per_page_.end()) {
    if (delegate->ShouldYield()) {
      TRACE_GC_NOTE("Quarantined page sweeping preempted");
      return;
    }
    MemoryChunk* chunk = next_page_iterator_->first;
    PageMetadata* page =
        static_cast<PageMetadata*>(chunk->Metadata(heap_->isolate()));
    DCHECK(!chunk->IsFromPage());
    if (chunk->IsToPage()) {
      SweepPage(CreateFillerFreeSpaceHandler, chunk, page,
                next_page_iterator_->second);
    } else {
      DCHECK_EQ(chunk->Metadata(heap_->isolate())->owner()->identity(),
                OLD_SPACE);
      base::MutexGuard guard(page->mutex());
      // If for some reason the page is swept twice, this DCHECK will fail.
      DCHECK_EQ(page->area_size(), page->allocated_bytes());
      size_t filler_size_on_page =
          SweepPage(AddToFreeListFreeSpaceHandler, chunk, page,
                    next_page_iterator_->second);
      DCHECK_EQ(page->owner()->identity(), OLD_SPACE);
      OldSpace* old_space = static_cast<OldSpace*>(page->owner());
      old_space->RelinkQuarantinedPageFreeList(page, filler_size_on_page);
    }
    next_page_iterator_++;
  }
  is_done_.store(true, std::memory_order_relaxed);
  pinned_object_per_page_.clear();
  pinned_objects_.clear();
}

// static
void ScavengerCollector::QuarantinedPageSweeper::JobTask::
    CreateFillerFreeSpaceHandler(Heap* heap, Address address, size_t size,
                                 bool should_zap) {
  if (should_zap) {
    heap::ZapBlock(address, size, heap::ZapValue());
  }
  heap->CreateFillerObjectAt(address, static_cast<int>(size));
}

// static
void ScavengerCollector::QuarantinedPageSweeper::JobTask::
    AddToFreeListFreeSpaceHandler(Heap* heap, Address address, size_t size,
                                  bool should_zap) {
  if (should_zap) {
    heap::ZapBlock(address, size, heap::ZapValue());
  }
  DCHECK_EQ(
      OLD_SPACE,
      PageMetadata::FromAddress(heap->isolate(), address)->owner()->identity());
  DCHECK(PageMetadata::FromAddress(heap->isolate(), address)->SweepingDone());
  OldSpace* const old_space = heap->old_space();
  old_space->FreeDuringSweep(address, size);
}

size_t ScavengerCollector::QuarantinedPageSweeper::JobTask::SweepPage(
    FreeSpaceHandler free_space_handler, MemoryChunk* chunk, PageMetadata* page,
    ObjectsAndSizes& pinned_objects_on_page) {
  DCHECK_EQ(page, chunk->Metadata(heap_->isolate()));
  DCHECK(!pinned_objects_on_page.empty());
  Address start = page->area_start();
  std::sort(pinned_objects_on_page.begin(), pinned_objects_on_page.end());
  size_t filler_size_on_page = 0;
  for (const auto& [object_adress, object_size] : pinned_objects_on_page) {
    DCHECK_LE(start, object_adress);
    if (start != object_adress) {
      size_t filler_size = object_adress - start;
      free_space_handler(heap_, start, filler_size, should_zap_);
      filler_size_on_page += filler_size;
    }
    start = object_adress + object_size;
  }
  Address end = page->area_end();
  if (start != end) {
    size_t filler_size = end - start;
    free_space_handler(heap_, start, filler_size, should_zap_);
    filler_size_on_page += filler_size;
  }
  return filler_size_on_page;
}

void ScavengerCollector::QuarantinedPageSweeper::StartSweeping(
    const ScavengerCollector::PinnedObjects&& pinned_objects) {
  DCHECK_NULL(job_handle_);
  DCHECK(!pinned_objects.empty());
  auto job = std::make_unique<JobTask>(heap_, std::move(pinned_objects));
  TRACE_GC_NOTE_WITH_FLOW("Quarantined page sweeper started", job->trace_id(),
                          TRACE_EVENT_FLAG_FLOW_OUT);
  job_handle_ = V8::GetCurrentPlatform()->PostJob(
      v8::TaskPriority::kUserVisible, std::move(job));
}

void ScavengerCollector::QuarantinedPageSweeper::FinishSweeping() {
  job_handle_->Join();
  job_handle_.reset();
}

void ScavengerCollector::CollectGarbage() {
  ScopedFullHeapCrashKey collect_full_heap_dump_if_crash(isolate_);

  DCHECK(surviving_new_large_objects_.empty());
  DCHECK(!quarantined_page_sweeper_.IsSweeping());

  SemiSpaceNewSpace* new_space = SemiSpaceNewSpace::From(heap_->new_space());
  new_space->GarbageCollectionPrologue();
  new_space->SwapSemiSpaces();

  // We also flip the young generation large object space. All large objects
  // will be in the from space.
  heap_->new_lo_space()->Flip();
  heap_->new_lo_space()->ResetPendingObject();

  DCHECK(!heap_->allocator()->new_space_allocator()->IsLabValid());

  Scavenger::EmptyChunksList empty_chunks;
  Scavenger::ScavengedObjectList copied_list;
  Scavenger::ScavengedObjectList promoted_list;
  EphemeronRememberedSet::TableList ephemeron_table_list;
  Scavenger::JSWeakRefsList js_weak_refs_list;
  Scavenger::WeakCellsList weak_cells_list;

  PinnedObjects pinned_objects;

  const int num_scavenge_tasks = NumberOfScavengeTasks();
  std::vector<std::unique_ptr<Scavenger>> scavengers;

  const bool is_logging = isolate_->log_object_relocation();
  for (int i = 0; i < num_scavenge_tasks; ++i) {
    scavengers.emplace_back(new Scavenger(
        this, heap_, is_logging, &empty_chunks, &copied_list, &promoted_list,
        &ephemeron_table_list, &js_weak_refs_list, &weak_cells_list));
  }
  Scavenger& main_thread_scavenger = *scavengers[kMainThreadId].get();

  {
    // Identify weak unmodified handles. Requires an unmodified graph.
    TRACE_GC(heap_->tracer(),
             GCTracer::Scope::SCAVENGER_SCAVENGE_WEAK_GLOBAL_HANDLES_IDENTIFY);
    isolate_->traced_handles()->ComputeWeaknessForYoungObjects();
  }

  std::vector<std::pair<ParallelWorkItem, MutablePageMetadata*>>
      old_to_new_chunks;
  {
    // Copy roots.
    TRACE_GC(heap_->tracer(), GCTracer::Scope::SCAVENGER_SCAVENGE_ROOTS);

    // We must collect old-to-new pages before starting Scavenge because
    // pages could be removed from the old generation for allocation which
    // hides them from the iteration.
    OldGenerationMemoryChunkIterator::ForAll(
        heap_, [&old_to_new_chunks](MutablePageMetadata* chunk) {
          if (chunk->slot_set<OLD_TO_NEW>() ||
              chunk->typed_slot_set<OLD_TO_NEW>() ||
              chunk->slot_set<OLD_TO_NEW_BACKGROUND>()) {
            old_to_new_chunks.emplace_back(ParallelWorkItem{}, chunk);
          }
        });

    const Heap::StackScanMode stack_scan_mode =
        heap_->ConservativeStackScanningModeForMinorGC();
    if (stack_scan_mode != Heap::StackScanMode::kNone &&
        heap_->IsGCWithStack()) {
      // Pinning objects must be the first step and must happen before
      // scavenging any objects. Specifically we must all pin all objects
      // before visiting other pinned objects. If we scavenge some object X
      // and move it before all stack-reachable objects are pinned, and we
      // later find that we need to pin X, it will be too late to undo the
      // moving.
      TRACE_GC(heap_->tracer(),
               GCTracer::Scope::SCAVENGER_SCAVENGE_PIN_OBJECTS);
      ConservativeObjectPinningVisitor conservative_pinning_visitor(
          heap_, main_thread_scavenger, pinned_objects);
      // Scavenger reuses the page's marking bitmap as a temporary object
      // start bitmap. Stack scanning will incrementally build the map as it
      // searches through pages.
      YoungGenerationConservativeStackVisitor stack_visitor(
          isolate_, &conservative_pinning_visitor);
      // Marker was already set by Heap::CollectGarbage.
      heap_->IterateConservativeStackRoots(&stack_visitor, stack_scan_mode);
      if (V8_UNLIKELY(v8_flags.stress_scavenger_conservative_object_pinning)) {
        TreatConservativelyVisitor handles_visitor(&stack_visitor, heap_);
        heap_->IterateRootsForPrecisePinning(&handles_visitor);
      }
    }
    const bool is_using_precise_pinning =
        heap_->ShouldUsePrecisePinningForMinorGC();
    if (is_using_precise_pinning) {
      PreciseObjectPinningVisitor precise_pinning_visitor(
          heap_, main_thread_scavenger, pinned_objects);
      ClearStaleLeftTrimmedPointerVisitor left_trim_visitor(
          heap_, &precise_pinning_visitor);
      heap_->IterateRootsForPrecisePinning(&left_trim_visitor);
    }

    // Scavenger treats all weak roots except for global handles as strong.
    // That is why we don't set skip_weak = true here and instead visit
    // global handles separately.
    base::EnumSet<SkipRoot> options(
        {SkipRoot::kExternalStringTable, SkipRoot::kGlobalHandles,
         SkipRoot::kTracedHandles, SkipRoot::kOldGeneration,
         SkipRoot::kConservativeStack, SkipRoot::kReadOnlyBuiltins});
    if (is_using_precise_pinning) {
      options.Add({SkipRoot::kMainThreadHandles, SkipRoot::kStack});
    }
    RootScavengeVisitor root_scavenge_visitor(main_thread_scavenger);

    heap_->IterateRoots(&root_scavenge_visitor, options);
    isolate_->global_handles()->IterateYoungStrongAndDependentRoots(
        &root_scavenge_visitor);
    isolate_->traced_handles()->IterateYoungRoots(&root_scavenge_visitor);
  }
  {
    // Parallel phase scavenging all copied and promoted objects.
    TRACE_GC_ARG1(heap_->tracer(),
                  GCTracer::Scope::SCAVENGER_SCAVENGE_PARALLEL_PHASE,
                  "UseBackgroundThreads", heap_->ShouldUseBackgroundThreads());

    auto job = std::make_unique<JobTask>(this, &scavengers,
                                         std::move(old_to_new_chunks),
                                         copied_list, promoted_list);
    TRACE_GC_NOTE_WITH_FLOW("Parallel scavenge started", job->trace_id(),
                            TRACE_EVENT_FLAG_FLOW_OUT);
    V8::GetCurrentPlatform()
        ->CreateJob(v8::TaskPriority::kUserBlocking, std::move(job))
        ->Join();
    DCHECK(copied_list.IsEmpty());
    DCHECK(promoted_list.IsEmpty());
  }

  {
    // Scavenge weak global handles.
    TRACE_GC(heap_->tracer(),
             GCTracer::Scope::SCAVENGER_SCAVENGE_WEAK_GLOBAL_HANDLES_PROCESS);
    GlobalHandlesWeakRootsUpdatingVisitor visitor;
    isolate_->global_handles()->ProcessWeakYoungObjects(
        &visitor, &IsUnscavengedHeapObjectSlot);
    isolate_->traced_handles()->ProcessWeakYoungObjects(
        &visitor, &IsUnscavengedHeapObjectSlot);
  }

  {
    // Finalize parallel scavenging.
    TRACE_GC(heap_->tracer(), GCTracer::Scope::SCAVENGER_SCAVENGE_FINALIZE);

    DCHECK(surviving_new_large_objects_.empty());

    for (auto& scavenger : scavengers) {
      scavenger->Finalize();
    }
    scavengers.clear();

#ifdef V8_COMPRESS_POINTERS
    // Sweep the external pointer table.
    DCHECK(heap_->concurrent_marking()->IsStopped());
    DCHECK(!heap_->incremental_marking()->IsMajorMarking());
    heap_->isolate()->external_pointer_table().Sweep(
        heap_->young_external_pointer_space(), heap_->isolate()->counters());
#endif  // V8_COMPRESS_POINTERS

    HandleSurvivingNewLargeObjects();

    heap_->tracer()->SampleConcurrencyEsimate(
        FetchAndResetConcurrencyEstimate());
  }

  {
    // Update references into new space
    TRACE_GC(heap_->tracer(), GCTracer::Scope::SCAVENGER_SCAVENGE_UPDATE_REFS);
    heap_->UpdateYoungReferencesInExternalStringTable(
        &Heap::UpdateYoungReferenceInExternalStringTableEntry);

    if (V8_UNLIKELY(v8_flags.always_use_string_forwarding_table)) {
      isolate_->string_forwarding_table()->UpdateAfterYoungEvacuation();
    }
  }

  ProcessWeakReferences(&ephemeron_table_list);
  ProcessWeakObjects(js_weak_refs_list, weak_cells_list);

  {
    TRACE_GC(heap_->tracer(),
             GCTracer::Scope::SCAVENGER_SCAVENGE_RESTORE_AND_QUARANTINE_PINNED);
    RestorePinnedObjects(*new_space, pinned_objects);
    QuarantinePinnedPages(*new_space);
  }

  // Need to free new space LAB that was allocated during scavenge.
  heap_->allocator()->new_space_allocator()->FreeLinearAllocationArea();

  // Since we promote all surviving large objects immediately, all remaining
  // large objects must be dead.
  // TODO(hpayer): Don't free all as soon as we have an intermediate generation.
  heap_->new_lo_space()->FreeDeadObjects(
      [](Tagged<HeapObject>) { return true; });

  new_space->GarbageCollectionEpilogue();

  // Start sweeping quarantined pages.
  if (!pinned_objects.empty()) {
    quarantined_page_sweeper_.StartSweeping(std::move(pinned_objects));
  } else {
    // Sweeping is not started since there are no pages to sweep. Mark sweeping
    // as completed so that the current GC cycle can be stopped since there is
    // no sweeper to mark it has completed later.
    heap_->tracer()->NotifyYoungSweepingCompleted();
  }

  {
    TRACE_GC(heap_->tracer(), GCTracer::Scope::SCAVENGER_FREE_REMEMBERED_SET);
    Scavenger::EmptyChunksList::Local empty_chunks_local(empty_chunks);
    MutablePageMetadata* chunk;
    while (empty_chunks_local.Pop(&chunk)) {
      RememberedSet<OLD_TO_NEW>::CheckPossiblyEmptyBuckets(chunk);
      RememberedSet<OLD_TO_NEW_BACKGROUND>::CheckPossiblyEmptyBuckets(chunk);
    }

#ifdef DEBUG
    OldGenerationMemoryChunkIterator::ForAll(
        heap_, [](MutablePageMetadata* chunk) {
          if (chunk->slot_set<OLD_TO_NEW>() ||
              chunk->typed_slot_set<OLD_TO_NEW>() ||
              chunk->slot_set<OLD_TO_NEW_BACKGROUND>()) {
            DCHECK(chunk->possibly_empty_buckets()->IsEmpty());
          }
        });
#endif
  }

  SweepArrayBufferExtensions();

  isolate_->global_handles()->UpdateListOfYoungNodes();
  isolate_->traced_handles()->UpdateListOfYoungNodes();

  // Update how much has survived scavenge.
  heap_->IncrementYoungSurvivorsCounter(heap_->SurvivedYoungObjectSize());

  {
    TRACE_GC(heap_->tracer(), GCTracer::Scope::SCAVENGER_RESIZE_NEW_SPACE);
    heap_->ResizeNewSpace();
  }
}

void ScavengerCollector::SweepArrayBufferExtensions() {
  DCHECK_EQ(0, heap_->new_lo_space()->Size());
  heap_->array_buffer_sweeper()->RequestSweep(
      ArrayBufferSweeper::SweepingType::kYoung,
      (heap_->new_space()->SizeOfObjects() == 0)
          ? ArrayBufferSweeper::TreatAllYoungAsPromoted::kYes
          : ArrayBufferSweeper::TreatAllYoungAsPromoted::kNo);
}

void ScavengerCollector::HandleSurvivingNewLargeObjects() {
  DCHECK(!heap_->incremental_marking()->IsMarking());

  for (SurvivingNewLargeObjectMapEntry update_info :
       surviving_new_large_objects_) {
    Tagged<HeapObject> object = update_info.first;
    Tagged<Map> map = update_info.second;
    // Order is important here. We have to re-install the map to have access
    // to meta-data like size during page promotion.
    object->set_map_word(map, kRelaxedStore);

    LargePageMetadata* page =
        LargePageMetadata::FromHeapObject(heap_->isolate(), object);
    SBXCHECK(page->is_large());
    SBXCHECK_EQ(page->owner_identity(), NEW_LO_SPACE);
    heap_->lo_space()->PromoteNewLargeObject(page);
  }
  surviving_new_large_objects_.clear();
  heap_->new_lo_space()->set_objects_size(0);
}

void ScavengerCollector::MergeSurvivingNewLargeObjects(
    const SurvivingNewLargeObjectsMap& objects) {
  for (SurvivingNewLargeObjectMapEntry object : objects) {
    bool success = surviving_new_large_objects_.insert(object).second;
    USE(success);
    DCHECK(success);
  }
}

void ScavengerCollector::CompleteSweepingQuarantinedPagesIfNeeded() {
  if (!quarantined_page_sweeper_.IsSweeping()) {
    return;
  }
  quarantined_page_sweeper_.FinishSweeping();
  heap_->tracer()->NotifyYoungSweepingCompletedAndStopCycleIfFinished();
}

int ScavengerCollector::NumberOfScavengeTasks() {
  if (!v8_flags.parallel_scavenge) {
    return 1;
  }
  const int num_scavenge_tasks =
      static_cast<int>(
          SemiSpaceNewSpace::From(heap_->new_space())->TotalCapacity()) /
          MB +
      1;
  static int num_cores = V8::GetCurrentPlatform()->NumberOfWorkerThreads() + 1;
  int tasks = std::max(
      1, std::min({num_scavenge_tasks, kMaxScavengerTasks, num_cores}));
  if (!heap_->CanPromoteYoungAndExpandOldGeneration(
          static_cast<size_t>(tasks * PageMetadata::kPageSize))) {
    // Optimize for memory usage near the heap limit.
    tasks = 1;
  }
  return tasks;
}

Scavenger::Scavenger(ScavengerCollector* collector, Heap* heap, bool is_logging,
                     EmptyChunksList* empty_chunks,
                     ScavengedObjectList* copied_list,
                     ScavengedObjectList* promoted_list,
                     EphemeronRememberedSet::TableList* ephemeron_table_list,
                     JSWeakRefsList* js_weak_refs_list,
                     WeakCellsList* weak_cells_list)
    : collector_(collector),
      heap_(heap),
      local_empty_chunks_(*empty_chunks),
      local_copied_list_(*copied_list),
      local_promoted_list_(*promoted_list),
      local_ephemeron_table_list_(*ephemeron_table_list),
      local_js_weak_refs_list_(*js_weak_refs_list),
      local_weak_cells_list_(*weak_cells_list),
      local_pretenuring_feedback_(PretenuringHandler::kInitialFeedbackCapacity),
      allocator_(heap, CompactionSpaceKind::kCompactionSpaceForScavenge),
      is_logging_(is_logging),
      shared_string_table_(v8_flags.shared_string_table &&
                           heap->isolate()->has_shared_space()),
      mark_shared_heap_(heap->isolate()->is_shared_space_isolate()),
      shortcut_strings_(
          heap->CanShortcutStringsDuringGC(GarbageCollector::SCAVENGER)) {
  DCHECK(!heap->incremental_marking()->IsMarking());
}

bool Scavenger::ShouldEagerlyProcessPromotedList() const {
  // Threshold when to prioritize processing of the promoted list. Right
  // now we only look into the regular object list.
  static constexpr int kProcessPromotedListThreshold =
      ScavengedObjectList::kMinSegmentSize / 2;
  return local_promoted_list_.PushSegmentSize() >=
         kProcessPromotedListThreshold;
}

void Scavenger::SynchronizePageAccess(Tagged<MaybeObject> object) const {
#ifdef THREAD_SANITIZER
  // Perform a dummy acquire load to tell TSAN that there is no data race
  // with  page initialization.
  Tagged<HeapObject> heap_object;
  if (object.GetHeapObject(&heap_object)) {
    MemoryChunk::FromHeapObject(heap_object)->SynchronizedLoad();
  }
#endif
}

bool Scavenger::MigrateObject(Tagged<Map> map, Tagged<HeapObject> source,
                              Tagged<HeapObject> target,
                              SafeHeapObjectSize size,
                              PromotionHeapChoice promotion_heap_choice) {
  // This CAS can be relaxed because we do not access the object body if the
  // object was already copied by another thread. We only access the page header
  // of such objects and this is safe because of the memory fence after page
  // header initialization.
  if (!source->relaxed_compare_and_swap_map_word_forwarded(
          MapWord::FromMap(map), target)) {
    // Other task migrated the object.
    return false;
  }

  // Copy the content of source to target. Note that we do this on purpose
  // *after* the CAS. This avoids copying of the object in the (unlikely)
  // failure case. It also helps us to ensure that we do not rely on non-relaxed
  // memory ordering for the CAS above.
  target->set_map_word(map, kRelaxedStore);
  heap()->CopyBlock(target.address() + kTaggedSize,
                    source.address() + kTaggedSize, size.value() - kTaggedSize);

  if (V8_UNLIKELY(is_logging_)) {
    // TODO(425150995): We should have uint versions for allocation to avoid
    // introducing OOBs via sign-extended ints along the way.
    heap()->OnMoveEvent(source, target, size.value());
  }

  PretenuringHandler::UpdateAllocationSite(heap_, map, source, size,
                                           &local_pretenuring_feedback_);

  return true;
}

template <typename THeapObjectSlot, typename OnSuccessCallback>
bool Scavenger::TryMigrateObject(Tagged<Map> map, THeapObjectSlot slot,
                                 Tagged<HeapObject> object,
                                 SafeHeapObjectSize object_size,
                                 AllocationSpace space,
                                 PromotionHeapChoice promotion_heap_choice,
                                 OnSuccessCallback on_success) {
  AllocationAlignment alignment = HeapObject::RequiredAlignment(space, map);
  AllocationResult allocation =
      allocator_.Allocate(space, object_size, alignment);

  Tagged<HeapObject> target;
  if (allocation.To(&target)) {
    DCHECK(heap()->marking_state()->IsUnmarked(target));
    const bool self_success =
        MigrateObject(map, object, target, object_size, promotion_heap_choice);
    if (!self_success) {
      allocator_.FreeLast(space, target, object_size);
      MapWord map_word = object->map_word(kRelaxedLoad);
      UpdateHeapObjectReferenceSlot(slot, map_word.ToForwardingAddress(object));
      SynchronizePageAccess(*slot);
      DCHECK(!Heap::InFromPage(*slot));
      return true;
    }
    UpdateHeapObjectReferenceSlot(slot, target);
    on_success(target);
    return true;
  }
  return false;
}

template <typename THeapObjectSlot>
CopyAndForwardResult Scavenger::SemiSpaceCopyObject(
    Tagged<Map> map, THeapObjectSlot slot, Tagged<HeapObject> object,
    SafeHeapObjectSize object_size, ObjectFields object_fields) {
  static_assert(std::is_same_v<THeapObjectSlot, FullHeapObjectSlot> ||
                    std::is_same_v<THeapObjectSlot, HeapObjectSlot>,
                "Only FullHeapObjectSlot and HeapObjectSlot are expected here");
  DCHECK(heap()->AllowedToBeMigrated(map, object, NEW_SPACE));
  if (TryMigrateObject(
          map, slot, object, object_size, NEW_SPACE, kPromoteIntoLocalHeap,
          [this, map, object_fields, object_size](Tagged<HeapObject> target) {
            copied_size_ += object_size.value();
            if (object_fields == ObjectFields::kMaybePointers) {
              local_copied_list_.Push({target, map, object_size});
            }
          })) {
    return CopyAndForwardResult::SUCCESS_YOUNG_GENERATION;
  }
  return CopyAndForwardResult::FAILURE;
}

template <typename THeapObjectSlot,
          Scavenger::PromotionHeapChoice promotion_heap_choice>
CopyAndForwardResult Scavenger::PromoteObject(Tagged<Map> map,
                                              THeapObjectSlot slot,
                                              Tagged<HeapObject> object,
                                              SafeHeapObjectSize object_size,
                                              ObjectFields object_fields) {
  static_assert(std::is_same_v<THeapObjectSlot, FullHeapObjectSlot> ||
                    std::is_same_v<THeapObjectSlot, HeapObjectSlot>,
                "Only FullHeapObjectSlot and HeapObjectSlot are expected here");
  DCHECK_GE(object_size.value(),
            Heap::kMinObjectSizeInTaggedWords * kTaggedSize);
  AllocationSpace target_space =
      promotion_heap_choice == kPromoteIntoLocalHeap ? OLD_SPACE : SHARED_SPACE;
  if (TryMigrateObject(
          map, slot, object, object_size, target_space, promotion_heap_choice,
          [this, map, object_fields, object_size](Tagged<HeapObject> target) {
            promoted_size_ += object_size.value();
            if (object_fields == ObjectFields::kMaybePointers) {
              local_promoted_list_.Push({target, map, object_size});
            }
          })) {
    return Heap::InToPage(*slot)
               ? CopyAndForwardResult::SUCCESS_YOUNG_GENERATION
               : CopyAndForwardResult::SUCCESS_OLD_GENERATION;
  }
  return CopyAndForwardResult::FAILURE;
}

SlotCallbackResult Scavenger::RememberedSetEntryNeeded(
    CopyAndForwardResult result) {
  DCHECK_NE(CopyAndForwardResult::FAILURE, result);
  return result == CopyAndForwardResult::SUCCESS_YOUNG_GENERATION ? KEEP_SLOT
                                                                  : REMOVE_SLOT;
}

bool Scavenger::HandleLargeObject(Tagged<Map> map, Tagged<HeapObject> object,
                                  SafeHeapObjectSize object_size,
                                  ObjectFields object_fields) {
  // Quick check first: A large object is the first (and only) object on a data
  // page (code pages have a different offset but are never young). This check
  // avoids the page flag access.
  if (MemoryChunk::AddressToOffset(object.address()) !=
      MemoryChunkLayout::ObjectStartOffsetInDataPage()) {
    return false;
  }

  // Most objects are already filtered out. Use page flags to filter out regular
  // objects at the beginning of a page.
  if (!MemoryChunk::FromHeapObject(object)->InNewLargeObjectSpace()) {
    return false;
  }

  DCHECK_EQ(NEW_LO_SPACE,
            MutablePageMetadata::FromHeapObject(heap_->isolate(), object)
                ->owner_identity());
  if (object->relaxed_compare_and_swap_map_word_forwarded(MapWord::FromMap(map),
                                                          object)) {
    local_surviving_new_large_objects_.insert({object, map});
    promoted_size_ += object_size.value();
    if (object_fields == ObjectFields::kMaybePointers) {
      local_promoted_list_.Push({object, map, object_size});
    }
  }
  return true;
}

template <typename THeapObjectSlot,
          Scavenger::PromotionHeapChoice promotion_heap_choice>
SlotCallbackResult Scavenger::EvacuateObjectDefault(
    Tagged<Map> map, THeapObjectSlot slot, Tagged<HeapObject> object,
    SafeHeapObjectSize object_size, ObjectFields object_fields) {
  static_assert(std::is_same_v<THeapObjectSlot, FullHeapObjectSlot> ||
                    std::is_same_v<THeapObjectSlot, HeapObjectSlot>,
                "Only FullHeapObjectSlot and HeapObjectSlot are expected here");
  SLOW_DCHECK(object->SafeSizeFromMap(map).value() == object_size.value());
  CopyAndForwardResult result;

  if (HandleLargeObject(map, object, object_size, object_fields)) {
    return REMOVE_SLOT;
  }

  SLOW_DCHECK(static_cast<size_t>(object_size.value()) <=
              MemoryChunkLayout::AllocatableMemoryInDataPage());

  if (!heap()->semi_space_new_space()->ShouldBePromoted(object.address())) {
    // A semi-space copy may fail due to fragmentation. In that case, we
    // try to promote the object.
    result = SemiSpaceCopyObject(map, slot, object, object_size, object_fields);
    if (result != CopyAndForwardResult::FAILURE) {
      return RememberedSetEntryNeeded(result);
    }
  }

  // We may want to promote this object if the object was already semi-space
  // copied in a previous young generation GC or if the semi-space copy above
  // failed.
  result = PromoteObject<THeapObjectSlot, promotion_heap_choice>(
      map, slot, object, object_size, object_fields);
  if (result != CopyAndForwardResult::FAILURE) {
    return RememberedSetEntryNeeded(result);
  }

  // If promotion failed, we try to copy the object to the other semi-space.
  result = SemiSpaceCopyObject(map, slot, object, object_size, object_fields);
  if (result != CopyAndForwardResult::FAILURE) {
    return RememberedSetEntryNeeded(result);
  }

  heap()->FatalProcessOutOfMemory("Scavenger: semi-space copy");
  UNREACHABLE();
}

template <typename THeapObjectSlot>
SlotCallbackResult Scavenger::EvacuateThinString(
    Tagged<Map> map, THeapObjectSlot slot, Tagged<ThinString> object,
    SafeHeapObjectSize object_size) {
  static_assert(std::is_same_v<THeapObjectSlot, FullHeapObjectSlot> ||
                    std::is_same_v<THeapObjectSlot, HeapObjectSlot>,
                "Only FullHeapObjectSlot and HeapObjectSlot are expected here");
  if (shortcut_strings_) {
    // The ThinString should die after Scavenge, so avoid writing the proper
    // forwarding pointer and instead just signal the actual object as forwarded
    // reference.
    Tagged<String> actual = object->actual();
    // ThinStrings always refer to internalized strings, which are always in old
    // space.
    DCHECK(!HeapLayout::InYoungGeneration(actual));
    UpdateHeapObjectReferenceSlot(slot, actual);
    return REMOVE_SLOT;
  }

  DCHECK_EQ(ObjectFields::kMaybePointers,
            Map::ObjectFieldsFrom(map->visitor_id()));
  return EvacuateObjectDefault(map, slot, object, object_size,
                               ObjectFields::kMaybePointers);
}

template <typename THeapObjectSlot>
SlotCallbackResult Scavenger::EvacuateShortcutCandidate(
    Tagged<Map> map, THeapObjectSlot slot, Tagged<ConsString> object,
    SafeHeapObjectSize object_size) {
  static_assert(std::is_same_v<THeapObjectSlot, FullHeapObjectSlot> ||
                    std::is_same_v<THeapObjectSlot, HeapObjectSlot>,
                "Only FullHeapObjectSlot and HeapObjectSlot are expected here");
  DCHECK(IsShortcutCandidate(map->instance_type()));

  if (shortcut_strings_ &&
      object->unchecked_second() == ReadOnlyRoots(heap()).empty_string()) {
    Tagged<HeapObject> first = Cast<HeapObject>(object->unchecked_first());

    UpdateHeapObjectReferenceSlot(slot, first);

    if (!HeapLayout::InYoungGeneration(first)) {
      object->set_map_word_forwarded(first, kRelaxedStore);
      return REMOVE_SLOT;
    }

    MapWord first_word = first->map_word(kRelaxedLoad);
    if (first_word.IsForwardingAddress()) {
      Tagged<HeapObject> target = first_word.ToForwardingAddress(first);

      UpdateHeapObjectReferenceSlot(slot, target);
      SynchronizePageAccess(target);
      object->set_map_word_forwarded(target, kRelaxedStore);
      return HeapLayout::InYoungGeneration(target) ? KEEP_SLOT : REMOVE_SLOT;
    }
    Tagged<Map> first_map = first_word.ToMap();
    SlotCallbackResult result = EvacuateObjectDefault(
        first_map, slot, first, first->SafeSizeFromMap(first_map),
        Map::ObjectFieldsFrom(first_map->visitor_id()));
    object->set_map_word_forwarded(slot.ToHeapObject(), kRelaxedStore);
    return result;
  }
  DCHECK_EQ(ObjectFields::kMaybePointers,
            Map::ObjectFieldsFrom(map->visitor_id()));
  return EvacuateObjectDefault(map, slot, object, object_size,
                               ObjectFields::kMaybePointers);
}

template <typename THeapObjectSlot>
SlotCallbackResult Scavenger::EvacuateInPlaceInternalizableString(
    Tagged<Map> map, THeapObjectSlot slot, Tagged<String> object,
    SafeHeapObjectSize object_size, ObjectFields object_fields) {
  DCHECK(String::IsInPlaceInternalizable(map->instance_type()));
  DCHECK_EQ(object_fields, Map::ObjectFieldsFrom(map->visitor_id()));
  if (shared_string_table_) {
    return EvacuateObjectDefault<THeapObjectSlot, kPromoteIntoSharedHeap>(
        map, slot, object, object_size, object_fields);
  }
  return EvacuateObjectDefault(map, slot, object, object_size, object_fields);
}

template <typename THeapObjectSlot>
SlotCallbackResult Scavenger::EvacuateObject(THeapObjectSlot slot,
                                             Tagged<Map> map,
                                             Tagged<HeapObject> source) {
  static_assert(std::is_same_v<THeapObjectSlot, FullHeapObjectSlot> ||
                    std::is_same_v<THeapObjectSlot, HeapObjectSlot>,
                "Only FullHeapObjectSlot and HeapObjectSlot are expected here");
  SLOW_DCHECK(Heap::InFromPage(source));
  SLOW_DCHECK(!MapWord::FromMap(map).IsForwardingAddress());
  const auto size = source->SafeSizeFromMap(map);
  // Cannot use ::cast() below because that would add checks in debug mode
  // that require re-reading the map.
  VisitorId visitor_id = map->visitor_id();
  switch (visitor_id) {
    case kVisitThinString:
      // At the moment we don't allow weak pointers to thin strings.
      DCHECK(!(*slot).IsWeak());
      return EvacuateThinString(map, slot, UncheckedCast<ThinString>(source),
                                size);
    case kVisitShortcutCandidate:
      DCHECK(!(*slot).IsWeak());
      // At the moment we don't allow weak pointers to cons strings.
      return EvacuateShortcutCandidate(map, slot,
                                       UncheckedCast<ConsString>(source), size);
    case kVisitSeqOneByteString:
    case kVisitSeqTwoByteString:
      DCHECK(String::IsInPlaceInternalizable(map->instance_type()));
      static_assert(Map::ObjectFieldsFrom(kVisitSeqOneByteString) ==
                    Map::ObjectFieldsFrom(kVisitSeqTwoByteString));
      return EvacuateInPlaceInternalizableString(
          map, slot, UncheckedCast<String>(source), size,
          Map::ObjectFieldsFrom(kVisitSeqOneByteString));
    default:
      return EvacuateObjectDefault(map, slot, source, size,
                                   Map::ObjectFieldsFrom(visitor_id));
  }
}

template <typename THeapObjectSlot>
SlotCallbackResult Scavenger::ScavengeObject(THeapObjectSlot p,
                                             Tagged<HeapObject> object) {
  static_assert(std::is_same_v<THeapObjectSlot, FullHeapObjectSlot> ||
                    std::is_same_v<THeapObjectSlot, HeapObjectSlot>,
                "Only FullHeapObjectSlot and HeapObjectSlot are expected here");
  DCHECK(Heap::InFromPage(object));

  // Check whether object was already successfully forwarded by the CAS in
  // MigrateObject. No memory ordering required because we only access the page
  // header of a relocated object. Page header initialization uses a memory
  // fence.
  MapWord first_word = object->map_word(kRelaxedLoad);

  // If the first word is a forwarding address, the object has already been
  // copied.
  if (first_word.IsForwardingAddress()) {
    Tagged<HeapObject> dest = first_word.ToForwardingAddress(object);
    DCHECK_IMPLIES(HeapLayout::IsSelfForwarded(object, first_word),
                   dest == object);
    if (dest == object) {
      const auto* metadata =
          MemoryChunk::FromHeapObject(object)->Metadata(heap()->isolate());
      return metadata->will_be_promoted() || metadata->is_large() ? REMOVE_SLOT
                                                                  : KEEP_SLOT;
    }

    UpdateHeapObjectReferenceSlot(p, dest);
    SynchronizePageAccess(dest);
    // A forwarded object in new space is either in the second (to) semi space,
    // a large object, or a pinned object on a quarantined page.
    // Pinned objects have a self forwarding map word. However, since forwarding
    // addresses are set with relaxed atomics and before the object is actually
    // copied, it is unfortunately not safe to access `dest` to check whether it
    // is pinned or not.
#ifdef DEBUG
    const auto* metadata =
        MemoryChunkMetadata::FromHeapObject(heap()->isolate(), dest);
    DCHECK_IMPLIES(HeapLayout::InYoungGeneration(dest),
                   Heap::InToPage(dest) || metadata->is_large() ||
                       metadata->is_quarantined());
#endif  // DEBUG

    // This load forces us to have memory ordering for the map load above. We
    // need to have the page header properly initialized.
    MemoryChunk* chunk = MemoryChunk::FromHeapObject(dest);
    return !chunk->InYoungGeneration() || chunk->IsLargePage() ? REMOVE_SLOT
                                                               : KEEP_SLOT;
  }

  Tagged<Map> map = first_word.ToMap();
  // AllocationMementos are unrooted and shouldn't survive a scavenge
  DCHECK_NE(ReadOnlyRoots(heap()).allocation_memento_map(), map);
  // Call the slow part of scavenge object.
  return EvacuateObject(p, map, object);
}

template <typename TSlot>
SlotCallbackResult Scavenger::CheckAndScavengeObject(Heap* heap, TSlot slot) {
  static_assert(
      std::is_same_v<TSlot, FullMaybeObjectSlot> ||
          std::is_same_v<TSlot, MaybeObjectSlot>,
      "Only FullMaybeObjectSlot and MaybeObjectSlot are expected here");
  using THeapObjectSlot = typename TSlot::THeapObjectSlot;
  Tagged<MaybeObject> object = *slot;
  if (Heap::InFromPage(object)) {
    Tagged<HeapObject> heap_object = object.GetHeapObject();

    SlotCallbackResult result =
        ScavengeObject(THeapObjectSlot(slot), heap_object);
    DCHECK_IMPLIES(result == REMOVE_SLOT,
                   !HeapLayout::InYoungGeneration((*slot).GetHeapObject()) ||
                       MemoryChunk::FromHeapObject((*slot).GetHeapObject())
                           ->IsLargePage() ||
                       MemoryChunkMetadata::FromHeapObject(
                           heap->isolate(), (*slot).GetHeapObject())
                           ->will_be_promoted());
    return result;
  } else if (Heap::InToPage(object)) {
    // Already updated slot. This can happen when processing of the work list
    // is interleaved with processing roots.
    return KEEP_SLOT;
  }
  // Slots can point to "to" space if the slot has been recorded multiple
  // times in the remembered set. We remove the redundant slot now.
  return REMOVE_SLOT;
}

template <ObjectAge Age>
V8_INLINE bool Scavenger::ShouldRecordWeakObject(Tagged<HeapObject> host,
                                                 ObjectSlot slot) {
  DCHECK(v8_flags.handle_weak_ref_weakly_in_minor_gc);
  Tagged<HeapObject> object = Cast<HeapObject>(slot.load());
  DCHECK_NE(kNullAddress, object.ptr());
  SynchronizePageAccess(object);
  if (!HeapLayout::InYoungGeneration(object)) {
    return false;
  }
  DCHECK(Heap::InFromPage(object));
  MapWord map_word = object->map_word(kRelaxedLoad);
  if (!map_word.IsForwardingAddress()) {
    // Not scavenged yet, add to worklist for processing after scavenging.
    return true;
  }
  Tagged<HeapObject> new_object = map_word.ToForwardingAddress(object);
  SynchronizePageAccess(new_object);
  DCHECK_IMPLIES(!HeapLayout::IsSelfForwarded(object),
                 !Heap::InFromPage(new_object));
  slot.store(new_object);
  DCHECK_EQ(Age == ObjectAge::kOld, HeapObjectWillBeOld(heap_, host));
  if constexpr (Age == ObjectAge::kYoung) {
    return false;
  }
  DCHECK(!HeapLayout::InWritableSharedSpace(new_object));
  if (V8_UNLIKELY(HeapLayout::InYoungGeneration(new_object))) {
    // `host` is younger than `object`, but in rare cases `host`
    // could be promoted to old gen while `object` remains in young gen.
    // For such cases it is needed to update the old-to-new remembered set.
    AddToRememberedSet<OLD_TO_NEW>(heap_, host, slot.address());
  }
  return false;
}

template void Scavenger::RecordJSWeakRefIfNeeded<ObjectAge::kYoung>(
    Tagged<JSWeakRef>);
template void Scavenger::RecordJSWeakRefIfNeeded<ObjectAge::kOld>(
    Tagged<JSWeakRef>);

template <ObjectAge Age>
void Scavenger::RecordJSWeakRefIfNeeded(Tagged<JSWeakRef> js_weak_ref) {
  if (!v8_flags.handle_weak_ref_weakly_in_minor_gc) {
    return;
  }

  if (ShouldRecordWeakObject<Age>(
          js_weak_ref, js_weak_ref->RawField(JSWeakRef::kTargetOffset))) {
    local_js_weak_refs_list_.Push(js_weak_ref);
  }
}

template void Scavenger::RecordWeakCellIfNeeded<ObjectAge::kYoung>(
    Tagged<WeakCell>);
template void Scavenger::RecordWeakCellIfNeeded<ObjectAge::kOld>(
    Tagged<WeakCell>);

template <ObjectAge Age>
void Scavenger::RecordWeakCellIfNeeded(Tagged<WeakCell> weak_cell) {
  if (!v8_flags.handle_weak_ref_weakly_in_minor_gc) {
    return;
  }

  const bool should_record_for_target =
      ShouldRecordWeakObject<Age>(weak_cell, ObjectSlot(&weak_cell->target_));
  const bool should_record_for_unregister_token_ = ShouldRecordWeakObject<Age>(
      weak_cell, ObjectSlot(&weak_cell->unregister_token_));
  if (should_record_for_target || should_record_for_unregister_token_) {
    local_weak_cells_list_.Push(weak_cell);
  }
}

void Scavenger::RememberPromotedEphemeron(Tagged<EphemeronHashTable> table,
                                          int index) {
  auto indices = local_ephemeron_remembered_set_.insert(
      {table, std::unordered_set<int>()});
  indices.first->second.insert(index);
}

void Scavenger::ScavengePage(MutablePageMetadata* page) {
  const bool record_old_to_shared_slots = heap_->isolate()->has_shared_space();

  MemoryChunk* chunk = page->Chunk();

  if (page->slot_set<OLD_TO_NEW, AccessMode::ATOMIC>() != nullptr) {
    RememberedSet<OLD_TO_NEW>::IterateAndTrackEmptyBuckets(
        page,
        [this, chunk, page, record_old_to_shared_slots](MaybeObjectSlot slot) {
          SlotCallbackResult result = CheckAndScavengeObject(heap_, slot);
          // A new space string might have been promoted into the shared heap
          // during GC.
          if (result == REMOVE_SLOT && record_old_to_shared_slots) {
            CheckOldToNewSlotForSharedUntyped(chunk, page, slot);
          }
          return result;
        },
        &local_empty_chunks_);
  }

  if (page->is_executable()) {
    std::vector<std::tuple<Tagged<HeapObject>, SlotType, Address>> slot_updates;

    // The code running write access to executable memory poses CFI attack
    // surface and needs to be kept to a minimum. So we do the the iteration in
    // two rounds. First we iterate the slots and scavenge objects and in the
    // second round with write access, we only perform the pointer updates.
    const auto typed_slot_count = RememberedSet<OLD_TO_NEW>::IterateTyped(
        page, [this, chunk, page, record_old_to_shared_slots, &slot_updates](
                  SlotType slot_type, Address slot_address) {
          Tagged<HeapObject> old_target =
              UpdateTypedSlotHelper::GetTargetObject(heap_, slot_type,
                                                     slot_address);
          Tagged<HeapObject> new_target = old_target;
          FullMaybeObjectSlot slot(&new_target);
          SlotCallbackResult result = CheckAndScavengeObject(heap(), slot);
          if (result == REMOVE_SLOT && record_old_to_shared_slots) {
            CheckOldToNewSlotForSharedTyped(chunk, page, slot_type,
                                            slot_address, *slot);
          }
          if (new_target != old_target) {
            slot_updates.emplace_back(new_target, slot_type, slot_address);
          }
          return result;
        });
    // Typed slots only exist in code objects. Since code is never young, it is
    // safe to release an empty typed slot set as no other scavenge thread will
    // attempt to promote to the page and write to the slot set.
    if (typed_slot_count == 0) {
      page->ReleaseTypedSlotSet(OLD_TO_NEW);
    }

    WritableJitPage jit_page = ThreadIsolation::LookupWritableJitPage(
        page->area_start(), page->area_size());
    for (auto& slot_update : slot_updates) {
      Tagged<HeapObject> new_target = std::get<0>(slot_update);
      SlotType slot_type = std::get<1>(slot_update);
      Address slot_address = std::get<2>(slot_update);

      WritableJitAllocation jit_allocation =
          jit_page.LookupAllocationContaining(slot_address);
      UpdateTypedSlotHelper::UpdateTypedSlot(
          jit_allocation, heap_, slot_type, slot_address,
          [new_target](FullMaybeObjectSlot slot) {
            slot.store(new_target);
            return KEEP_SLOT;
          });
    }
  } else {
    DCHECK_NULL(page->typed_slot_set<OLD_TO_NEW>());
  }

  if (page->slot_set<OLD_TO_NEW_BACKGROUND, AccessMode::ATOMIC>() != nullptr) {
    RememberedSet<OLD_TO_NEW_BACKGROUND>::IterateAndTrackEmptyBuckets(
        page,
        [this, chunk, page, record_old_to_shared_slots](MaybeObjectSlot slot) {
          SlotCallbackResult result = CheckAndScavengeObject(heap_, slot);
          // A new space string might have been promoted into the shared heap
          // during GC.
          if (result == REMOVE_SLOT && record_old_to_shared_slots) {
            CheckOldToNewSlotForSharedUntyped(chunk, page, slot);
          }
          return result;
        },
        &local_empty_chunks_);
  }
}

void Scavenger::Process(JobDelegate* delegate) {
  ScavengerCopiedObjectVisitor copied_object_visitor(this);
  ScavengerPromotedObjectVisitor promoted_object_visitor(this);

  bool done;
  size_t objects = 0;
  do {
    done = true;
    ScavengedObjectListEntry entry;
    while (!ShouldEagerlyProcessPromotedList() &&
           local_copied_list_.Pop(&entry)) {
      copied_object_visitor.Visit(entry.map, entry.heap_object, entry.size);
      done = false;
      if (delegate && ((++objects % kInterruptThreshold) == 0)) {
        if (!local_copied_list_.IsLocalEmpty()) {
          delegate->NotifyConcurrencyIncrease();
        }
      }
    }

    while (local_promoted_list_.Pop(&entry)) {
      promoted_object_visitor.Visit(entry.map, entry.heap_object, entry.size);
      done = false;
      if (delegate && ((++objects % kInterruptThreshold) == 0)) {
        if (!local_promoted_list_.IsGlobalEmpty()) {
          delegate->NotifyConcurrencyIncrease();
        }
      }
    }
  } while (!done);
}

void ScavengerCollector::ProcessWeakReferences(
    EphemeronRememberedSet::TableList* ephemeron_table_list) {
  ClearYoungEphemerons(ephemeron_table_list);
  ClearOldEphemerons();
}

// Clear ephemeron entries from EphemeronHashTables in new-space whenever the
// entry has a dead new-space key.
void ScavengerCollector::ClearYoungEphemerons(
    EphemeronRememberedSet::TableList* ephemeron_table_list) {
  ephemeron_table_list->Iterate([](Tagged<EphemeronHashTable> table) {
    for (InternalIndex i : table->IterateEntries()) {
      // Keys in EphemeronHashTables must be heap objects.
      HeapObjectSlot key_slot(
          table->RawFieldOfElementAt(EphemeronHashTable::EntryToIndex(i)));
      Tagged<HeapObject> key = key_slot.ToHeapObject();
      // If the key is not in the from page, it's not being scavenged.
      if (!Heap::InFromPage(key)) continue;
      DCHECK(!IsAnyHole(key));
      MapWord map_word = key->map_word(kRelaxedLoad);
      if (!map_word.IsForwardingAddress()) {
        // If the key is not forwarded, then it's dead.
        DCHECK(IsUnscavengedHeapObject(key));
        table->RemoveEntry(i);
      } else {
        // Otherwise, we need to update the key slot to the forwarded address.
        DCHECK(!IsUnscavengedHeapObject(key));
        key_slot.StoreHeapObject(map_word.ToForwardingAddress(key));
      }
    }
  });
  ephemeron_table_list->Clear();
}

// Clear ephemeron entries from EphemeronHashTables in old-space whenever the
// entry has a dead new-space key.
void ScavengerCollector::ClearOldEphemerons() {
  auto* table_map = heap_->ephemeron_remembered_set_->tables();
  for (auto it = table_map->begin(); it != table_map->end();) {
    Tagged<EphemeronHashTable> table = it->first;
    auto& indices = it->second;
    for (auto iti = indices.begin(); iti != indices.end();) {
      // Keys in EphemeronHashTables must be heap objects.
      HeapObjectSlot key_slot(table->RawFieldOfElementAt(
          EphemeronHashTable::EntryToIndex(InternalIndex(*iti))));
      Tagged<HeapObject> key = key_slot.ToHeapObject();
      // If the key is not young, we don't need it in the remembered set.
      if (!HeapLayout::InYoungGeneration(key)) {
        iti = indices.erase(iti);
      }
      // If the key is not in the from page, it's not being scavenged.
      if (!Heap::InFromPage(key)) continue;
      DCHECK(!IsAnyHole(key));
      MapWord map_word = key->map_word(kRelaxedLoad);
      DCHECK_IMPLIES(Heap::InToPage(key), !map_word.IsForwardingAddress());
      if (!map_word.IsForwardingAddress()) {
        // If the key is not forwarded, then it's dead.
        DCHECK(IsUnscavengedHeapObject(key));
        table->RemoveEntry(InternalIndex(*iti));
        iti = indices.erase(iti);
      } else {
        // Otherwise, we need to update the key slot to the forwarded address.
        DCHECK(!IsUnscavengedHeapObject(key));
        Tagged<HeapObject> forwarded = map_word.ToForwardingAddress(key);
        key_slot.StoreHeapObject(forwarded);
        if (!HeapLayout::InYoungGeneration(forwarded)) {
          // If the key was promoted out of new space, we don't need to keep it
          // in the remembered set.
          iti = indices.erase(iti);
        } else {
          ++iti;
        }
      }
    }

    if (indices.empty()) {
      it = table_map->erase(it);
    } else {
      ++it;
    }
  }
}

namespace {
class ScavengerWeakObjectRetainer : public WeakObjectRetainer {
 public:
  Tagged<Object> RetainAs(Tagged<Object> object) override {
    Tagged<HeapObject> heap_object = Cast<HeapObject>(object);
    if (IsUnscavengedHeapObject(heap_object)) {
      return Smi::zero();
    }
    DCHECK(!Heap::InToPage(heap_object));
    if (!HeapLayout::InYoungGeneration(heap_object)) {
      return object;
    }
    MapWord map_word = heap_object->map_word(kRelaxedLoad);
    DCHECK(map_word.IsForwardingAddress());
    return map_word.ToForwardingAddress(heap_object);
  }
};
}  // namespace

void ScavengerCollector::ProcessWeakObjects(
    Scavenger::JSWeakRefsList& js_weak_refs,
    Scavenger::WeakCellsList& weak_cells) {
  ScavengerWeakObjectRetainer weak_object_retainer;
  // Iterate the weak list of dirty finalization registries. Dead registries are
  // dropped from the list, and addresses of live scavenged registries are
  // updated.
  heap_->ProcessDirtyJSFinalizationRegistries(&weak_object_retainer);
  // Clear the target field of JSWeakRef if the target is dead.
  ProcessJSWeakRefs(js_weak_refs);
  // For each WeakCell:
  // 1) If the target is dead, clear the target field, mark the finalization
  // registry as dirty, and schedule it to be cleaned up (in a task). 2) If the
  // unregister token is dead, clear the token field and remove the cell from
  // the registry's token-to-cell map.
  ProcessWeakCells(weak_cells);
}

void ProcessWeakObjectField(const Heap* heap, Tagged<HeapObject> host,
                            ObjectSlot slot, auto dead_callback) {
  Tagged<HeapObject> object = Cast<HeapObject>(slot.load());
  DCHECK(!Heap::InFromPage(host) || HeapLayout::IsSelfForwarded(host));
  if (Heap::InFromPage(object)) {
    DCHECK(!IsUndefined(object));
    if (IsUnscavengedHeapObject(object)) {
      DCHECK(Object::CanBeHeldWeakly(object));
      // The object is dead.
      dead_callback(host, object);
    } else {
      // The object is alive.
      MapWord map_word = object->map_word(kRelaxedLoad);
      DCHECK(map_word.IsForwardingAddress());
      Tagged<HeapObject> new_object = map_word.ToForwardingAddress(object);
      DCHECK(!Heap::InFromPage(new_object) ||
             HeapLayout::IsSelfForwarded(new_object));
      // `host` is younger than `object`, but in rare cases
      // `host` could be promoted to old gen while `object` remains in
      // young gen. For such cases a write barrier is needed to update the
      // old-to-new remembered set.
      DCHECK(!HeapLayout::InWritableSharedSpace(new_object));
      slot.store(new_object);
      if (V8_UNLIKELY(HeapObjectWillBeOld(heap, host) &&
                      !HeapObjectWillBeOld(heap, new_object))) {
        AddToRememberedSet<OLD_TO_NEW>(heap, host, slot.address());
      }
    }
  }
}

void ScavengerCollector::ProcessJSWeakRefs(
    Scavenger::JSWeakRefsList& js_weak_refs) {
  const auto on_dead_target_callback = [this](Tagged<HeapObject> host,
                                              Tagged<HeapObject>) {
    GCSafeCast<JSWeakRef>(host, heap_)
        ->set_target(ReadOnlyRoots(heap_->isolate()).undefined_value(),
                     SKIP_WRITE_BARRIER);
  };

  Scavenger::JSWeakRefsList::Local local_js_weak_refs(js_weak_refs);
  Tagged<JSWeakRef> js_weak_ref;
  while (local_js_weak_refs.Pop(&js_weak_ref)) {
    ProcessWeakObjectField(heap_, js_weak_ref,
                           js_weak_ref->RawField(JSWeakRef::kTargetOffset),
                           on_dead_target_callback);
  }
}

void ScavengerCollector::ProcessWeakCells(
    Scavenger::WeakCellsList& weak_cells) {
  const auto on_slot_updated_callback = [this](Tagged<HeapObject> object,
                                               ObjectSlot slot,
                                               Tagged<Object> target) {
    DCHECK(!IsUnscavengedHeapObject(target));
    DCHECK(!Cast<HeapObject>(target)
                ->map_word(kRelaxedLoad)
                .IsForwardingAddress() ||
           HeapLayout::IsSelfForwarded(Cast<HeapObject>(target)));
    if (V8_UNLIKELY(HeapObjectWillBeOld(heap_, object) &&
                    !HeapObjectWillBeOld(heap_, Cast<HeapObject>(target)))) {
      AddToRememberedSet<OLD_TO_NEW>(heap_, object, slot.address());
    }
  };
  const auto on_dead_target_callback = [this, on_slot_updated_callback](
                                           Tagged<HeapObject> host,
                                           Tagged<HeapObject>) {
    Tagged<WeakCell> weak_cell = GCSafeCast<WeakCell>(host, heap_);
    // The WeakCell is liove but its value is dead. WeakCell retains the
    // JSFinalizationRegistry, so it's also guaranteed to be live.
    Tagged<JSFinalizationRegistry> finalization_registry =
        GCSafeCast<JSFinalizationRegistry>(weak_cell->finalization_registry(),
                                           heap_);
    if (!finalization_registry->scheduled_for_cleanup()) {
      heap_->EnqueueDirtyJSFinalizationRegistry(finalization_registry,
                                                on_slot_updated_callback);
    }
    // We're modifying the pointers in WeakCell and JSFinalizationRegistry
    // during GC; thus we need to record the slots it writes. The normal
    // write barrier is not enough, since it's disabled before GC.
    weak_cell->GCSafeNullify(heap_->isolate(), on_slot_updated_callback);
    DCHECK(GCAwareObjectTypeCheck<WeakCell>(
        finalization_registry
            ->RawField(JSFinalizationRegistry::kClearedCellsOffset)
            .load(),
        heap_));
    DCHECK(finalization_registry->scheduled_for_cleanup());
  };
  const auto on_dead_unregister_token_callback =
      [this, on_slot_updated_callback](
          Tagged<HeapObject> host, Tagged<HeapObject> dead_unregister_token) {
        Tagged<WeakCell> weak_cell = GCSafeCast<WeakCell>(host, heap_);
        // The unregister token is dead. Remove any corresponding entries in the
        // key map. Multiple WeakCell with the same token will have all their
        // unregister_token field set to undefined when processing the first
        // WeakCell. Like above, we're modifying pointers during GC, so record
        // the slots.
        Tagged<JSFinalizationRegistry> finalization_registry =
            GCSafeCast<JSFinalizationRegistry>(
                weak_cell->finalization_registry(), heap_);
        finalization_registry->RemoveUnregisterToken(
            dead_unregister_token, heap_->isolate(),
            JSFinalizationRegistry::kKeepMatchedCellsInRegistry,
            on_slot_updated_callback);
      };

  Scavenger::WeakCellsList::Local local_weak_cells(weak_cells);
  Tagged<WeakCell> weak_cell;
  while (local_weak_cells.Pop(&weak_cell)) {
    ProcessWeakObjectField(heap_, weak_cell, ObjectSlot(&weak_cell->target_),
                           on_dead_target_callback);
    ProcessWeakObjectField(heap_, weak_cell,
                           ObjectSlot(&weak_cell->unregister_token_),
                           on_dead_unregister_token_callback);
  }
  heap_->PostFinalizationRegistryCleanupTaskIfNeeded();
}

void Scavenger::Finalize() {
  heap()->pretenuring_handler()->MergeAllocationSitePretenuringFeedback(
      local_pretenuring_feedback_);
  for (const auto& it : local_ephemeron_remembered_set_) {
    // The ephemeron objects in the remembered set should be either large
    // objects, promoted to old space, or pinned objects on quarantined pages
    // that will be promoted.
#ifdef DEBUG
    auto* chunk = MemoryChunk::FromHeapObject(it.first);
    auto* metadata = chunk->Metadata(heap_->isolate());
    DCHECK_IMPLIES(
        !metadata->is_large(),
        !HeapLayout::InYoungGeneration(it.first) ||
            (HeapLayout::IsSelfForwarded(it.first) &&
             metadata->is_quarantined() && metadata->will_be_promoted()));
#endif  // DEBUG
    heap()->ephemeron_remembered_set()->RecordEphemeronKeyWrites(
        it.first, std::move(it.second));
  }
  heap()->IncrementNewSpaceSurvivingObjectSize(copied_size_);
  heap()->IncrementPromotedObjectsSize(promoted_size_);
  collector_->MergeSurvivingNewLargeObjects(local_surviving_new_large_objects_);
  allocator_.Finalize();
  local_empty_chunks_.Publish();
  local_ephemeron_table_list_.Publish();
  local_js_weak_refs_list_.Publish();
  local_weak_cells_list_.Publish();
}

void Scavenger::Publish() {
  local_copied_list_.Publish();
  local_promoted_list_.Publish();
}

void Scavenger::AddEphemeronHashTable(Tagged<EphemeronHashTable> table) {
  local_ephemeron_table_list_.Push(table);
}

template <typename TSlot>
void Scavenger::CheckOldToNewSlotForSharedUntyped(MemoryChunk* chunk,
                                                  MutablePageMetadata* page,
                                                  TSlot slot) {
  Tagged<MaybeObject> object = *slot;
  Tagged<HeapObject> heap_object;

  if (object.GetHeapObject(&heap_object) &&
      HeapLayout::InWritableSharedSpace(heap_object)) {
    RememberedSet<OLD_TO_SHARED>::Insert<AccessMode::ATOMIC>(
        page, chunk->Offset(slot.address()));
  }
}

void Scavenger::CheckOldToNewSlotForSharedTyped(
    MemoryChunk* chunk, MutablePageMetadata* page, SlotType slot_type,
    Address slot_address, Tagged<MaybeObject> new_target) {
  Tagged<HeapObject> heap_object;

  if (new_target.GetHeapObject(&heap_object) &&
      HeapLayout::InWritableSharedSpace(heap_object)) {
    const uintptr_t offset = chunk->Offset(slot_address);
    DCHECK_LT(offset, static_cast<uintptr_t>(TypedSlotSet::kMaxOffset));

    base::MutexGuard guard(page->mutex());
    RememberedSet<OLD_TO_SHARED>::InsertTyped(page, slot_type,
                                              static_cast<uint32_t>(offset));
  }
}

bool Scavenger::PromoteIfLargeObject(Tagged<HeapObject> object) {
  Tagged<Map> map = object->map();
  return HandleLargeObject(map, object, object->SafeSizeFromMap(map),
                           Map::ObjectFieldsFrom(map->visitor_id()));
}

void Scavenger::PinAndPushObject(MutablePageMetadata* metadata,
                                 Tagged<HeapObject> object, MapWord map_word) {
  DCHECK(metadata->Contains(object->address()));
  DCHECK_EQ(map_word, object->map_word(kRelaxedLoad));
  Tagged<Map> map = map_word.ToMap();
  const auto object_size = object->SafeSizeFromMap(map);
  PretenuringHandler::UpdateAllocationSite(heap_, map, object, object_size,
                                           &local_pretenuring_feedback_);
  object->set_map_word_forwarded(object, kRelaxedStore);
  DCHECK(object->map_word(kRelaxedLoad).IsForwardingAddress());
  DCHECK(HeapLayout::IsSelfForwarded(object));
  if (metadata->will_be_promoted()) {
    PushPinnedPromotedObject(object, map, object_size);
  } else {
    PushPinnedObject(object, map, object_size);
  }
}

void Scavenger::PushPinnedObject(Tagged<HeapObject> object, Tagged<Map> map,
                                 SafeHeapObjectSize object_size) {
  DCHECK(HeapLayout::IsSelfForwarded(object));
  DCHECK(!MemoryChunkMetadata::FromHeapObject(heap_->isolate(), object)
              ->will_be_promoted());
  DCHECK_EQ(object_size.value(), object->SafeSizeFromMap(map).value());
  local_copied_list_.Push({object, map, object_size});
  copied_size_ += object_size.value();
}

void Scavenger::PushPinnedPromotedObject(Tagged<HeapObject> object,
                                         Tagged<Map> map,
                                         SafeHeapObjectSize object_size) {
  DCHECK(HeapLayout::IsSelfForwarded(object));
  DCHECK(MemoryChunkMetadata::FromHeapObject(heap_->isolate(), object)
             ->will_be_promoted());
  DCHECK_EQ(object_size.value(), object->SafeSizeFromMap(map).value());
  local_promoted_list_.Push({object, map, object_size});
  promoted_size_ += object_size.value();
}

void RootScavengeVisitor::VisitRootPointer(Root root, const char* description,
                                           FullObjectSlot p) {
  DCHECK(!HasWeakHeapObjectTag(*p));
  DCHECK(!MapWord::IsPacked((*p).ptr()));
  ScavengePointer(p);
}

void RootScavengeVisitor::VisitRootPointers(Root root, const char* description,
                                            FullObjectSlot start,
                                            FullObjectSlot end) {
  // Copy all HeapObject pointers in [start, end)
  for (FullObjectSlot p = start; p < end; ++p) {
    ScavengePointer(p);
  }
}

void RootScavengeVisitor::ScavengePointer(FullObjectSlot p) {
  Tagged<Object> object = *p;
#ifdef V8_ENABLE_DIRECT_HANDLE
  if (object.ptr() == kTaggedNullAddress) return;
#endif
  DCHECK(!HasWeakHeapObjectTag(object));
  DCHECK(!MapWord::IsPacked(object.ptr()));
  if (HeapLayout::InYoungGeneration(object)) {
    scavenger_.ScavengeObject(FullHeapObjectSlot(p), Cast<HeapObject>(object));
  }
}

RootScavengeVisitor::RootScavengeVisitor(Scavenger& scavenger)
    : scavenger_(scavenger) {}

RootScavengeVisitor::~RootScavengeVisitor() { scavenger_.Publish(); }

}  // namespace internal
}  // namespace v8
