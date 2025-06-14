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
#include "src/heap/pretenuring-handler.h"
#include "src/heap/remembered-set-inl.h"
#include "src/heap/scavenger-inl.h"
#include "src/heap/slot-set.h"
#include "src/heap/sweeper.h"
#include "src/heap/zapping.h"
#include "src/objects/data-handler-inl.h"
#include "src/objects/embedder-data-array-inl.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/objects-body-descriptors-inl.h"
#include "src/objects/objects.h"
#include "src/objects/slots.h"
#include "src/objects/transitions-inl.h"
#include "src/utils/utils-inl.h"

namespace v8 {
namespace internal {

class IterateAndScavengePromotedObjectsVisitor final
    : public HeapVisitor<IterateAndScavengePromotedObjectsVisitor> {
 public:
  explicit IterateAndScavengePromotedObjectsVisitor(Scavenger* scavenger)
      : HeapVisitor(scavenger->heap()->isolate()), scavenger_(scavenger) {}

  V8_INLINE static constexpr bool ShouldUseUncheckedCast() { return true; }

  V8_INLINE static constexpr bool UsePrecomputedObjectSize() { return true; }

  V8_INLINE void VisitMapPointer(Tagged<HeapObject> host) final {}

  V8_INLINE void VisitPointers(Tagged<HeapObject> host, ObjectSlot start,
                               ObjectSlot end) final {
    VisitPointersImpl(host, start, end);
  }

  V8_INLINE void VisitPointers(Tagged<HeapObject> host, MaybeObjectSlot start,
                               MaybeObjectSlot end) final {
    VisitPointersImpl(host, start, end);
  }

  inline void VisitEphemeron(Tagged<HeapObject> obj, int entry, ObjectSlot key,
                             ObjectSlot value) override {
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

  void VisitExternalPointer(Tagged<HeapObject> host,
                            ExternalPointerSlot slot) override {
#ifdef V8_COMPRESS_POINTERS
    DCHECK(!slot.tag_range().IsEmpty());
    DCHECK(!IsSharedExternalPointerType(slot.tag_range()));
    // TODO(chromium:337580006): Remove when pointer compression always uses
    // EPT.
    if (!slot.HasExternalPointerHandle()) return;
    ExternalPointerHandle handle = slot.Relaxed_LoadHandle();
    Heap* heap = scavenger_->heap();
    ExternalPointerTable& table = heap->isolate()->external_pointer_table();

    // For survivor objects, the scavenger marks their EPT entries when they are
    // copied and then sweeps the young EPT space at the end of collection,
    // reclaiming unmarked EPT entries.
    //
    // However when promoting, we just evacuate the entry from new to old space.
    // Usually the entry will be unmarked, unless the slot was initialized since
    // the last GC (external pointer tags have the mark bit set), in which case
    // it may be marked already.  In any case, transfer the color from new to
    // old EPT space.
    table.Evacuate(heap->young_external_pointer_space(),
                   heap->old_external_pointer_space(), handle, slot.address(),
                   ExternalPointerTable::EvacuateMarkMode::kTransferMark);
#endif  // V8_COMPRESS_POINTERS
  }

  // Special cases: Unreachable visitors for objects that are never found in the
  // young generation and thus cannot be found when iterating promoted objects.
  void VisitInstructionStreamPointer(Tagged<Code>,
                                     InstructionStreamSlot) final {
    UNREACHABLE();
  }
  void VisitCodeTarget(Tagged<InstructionStream>, RelocInfo*) final {
    UNREACHABLE();
  }
  void VisitEmbeddedPointer(Tagged<InstructionStream>, RelocInfo*) final {
    UNREACHABLE();
  }

 private:
  template <typename TSlot>
  V8_INLINE void VisitPointersImpl(Tagged<HeapObject> host, TSlot start,
                                   TSlot end) {
    using THeapObjectSlot = typename TSlot::THeapObjectSlot;
    // Treat weak references as strong.
    // TODO(marja): Proper weakness handling in the young generation.
    for (TSlot slot = start; slot < end; ++slot) {
      typename TSlot::TObject object = *slot;
      Tagged<HeapObject> heap_object;
      if (object.GetHeapObject(&heap_object)) {
        HandleSlot(host, THeapObjectSlot(slot), heap_object);
      }
    }
  }

  template <typename THeapObjectSlot>
  V8_INLINE void HandleSlot(Tagged<HeapObject> host, THeapObjectSlot slot,
                            Tagged<HeapObject> target) {
    static_assert(
        std::is_same_v<THeapObjectSlot, FullHeapObjectSlot> ||
            std::is_same_v<THeapObjectSlot, HeapObjectSlot>,
        "Only FullHeapObjectSlot and HeapObjectSlot are expected here");
    scavenger_->SynchronizePageAccess(target);

    if (Heap::InFromPage(target)) {
      SlotCallbackResult result = scavenger_->ScavengeObject(slot, target);
      bool success = (*slot).GetHeapObject(&target);
      USE(success);
      DCHECK(success);

      if (result == KEEP_SLOT) {
        SLOW_DCHECK(IsHeapObject(target));
        MemoryChunk* chunk = MemoryChunk::FromHeapObject(host);
        MutablePageMetadata* page =
            MutablePageMetadata::cast(chunk->Metadata());

        // Sweeper is stopped during scavenge, so we can directly
        // insert into its remembered set here.
        RememberedSet<OLD_TO_NEW>::Insert<AccessMode::ATOMIC>(
            page, chunk->Offset(slot.address()));
      }
      DCHECK(!MarkCompactCollector::IsOnEvacuationCandidate(target));
    }

    if (HeapLayout::InWritableSharedSpace(target)) {
      MemoryChunk* chunk = MemoryChunk::FromHeapObject(host);
      MutablePageMetadata* page = MutablePageMetadata::cast(chunk->Metadata());
      RememberedSet<OLD_TO_SHARED>::Insert<AccessMode::ATOMIC>(
          page, chunk->Offset(slot.address()));
    }
  }

  Scavenger* const scavenger_;
};

namespace {

V8_INLINE bool IsUnscavengedHeapObject(Heap* heap, Tagged<Object> object) {
  return Heap::InFromPage(object) && !Cast<HeapObject>(object)
                                          ->map_word(kRelaxedLoad)
                                          .IsForwardingAddress();
}

// Same as IsUnscavengedHeapObject() above but specialized for HeapObjects.
V8_INLINE bool IsUnscavengedHeapObject(Heap* heap,
                                       Tagged<HeapObject> heap_object) {
  return Heap::InFromPage(heap_object) &&
         !heap_object->map_word(kRelaxedLoad).IsForwardingAddress();
}

bool IsUnscavengedHeapObjectSlot(Heap* heap, FullObjectSlot p) {
  return IsUnscavengedHeapObject(heap, *p);
}

}  // namespace

ScavengerCollector::JobTask::JobTask(
    ScavengerCollector* collector,
    std::vector<std::unique_ptr<Scavenger>>* scavengers,
    std::vector<std::pair<ParallelWorkItem, MutablePageMetadata*>>
        old_to_new_chunks,
    const Scavenger::CopiedList& copied_list,
    const Scavenger::PinnedList& pinned_list,
    const Scavenger::PromotedList& promoted_list)
    : collector_(collector),
      scavengers_(scavengers),
      old_to_new_chunks_(std::move(old_to_new_chunks)),
      remaining_memory_chunks_(old_to_new_chunks_.size()),
      generator_(old_to_new_chunks_.size()),
      copied_list_(copied_list),
      pinned_list_(pinned_list),
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
  size_t wanted_num_workers =
      std::max<size_t>(remaining_memory_chunks_.load(std::memory_order_relaxed),
                       worker_count + copied_list_.Size() +
                           pinned_list_.Size() + promoted_list_.Size());
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

    scavenger->VisitPinnedObjects();
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
      DCHECK(Heap::IsLargeObject(heap_object) ||
             MemoryChunk::FromHeapObject(heap_object)->IsQuarantined());
      return;
    }
    UpdateHeapObjectReferenceSlot(FullHeapObjectSlot(p), dest);
    // The destination object should be in the "to" space. However, it could
    // also be a large string if the original object was a shortcut candidate.
    DCHECK_IMPLIES(HeapLayout::InYoungGeneration(dest),
                   Heap::InToPage(dest) ||
                       (Heap::IsLargeObject(dest) && Heap::InFromPage(dest) &&
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
    DCHECK(!MemoryChunk::FromHeapObject(object)->IsLargePage());
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
    int object_size = object->SizeFromMap(map_word.ToMap());
    DCHECK_LT(0, object_size);
    pinned_objects_.push_back(
        {object_address, map_word, static_cast<size_t>(object_size)});
    MemoryChunk* chunk = MemoryChunk::FromHeapObject(object);
    if (!chunk->IsQuarantined()) {
      chunk->SetFlagNonExecutable(MemoryChunk::IS_QUARANTINED);
      if (v8_flags.scavenger_promote_quarantined_pages &&
          heap_->semi_space_new_space()->ShouldPageBePromoted(chunk)) {
        chunk->SetFlagNonExecutable(MemoryChunk::WILL_BE_PROMOTED);
      }
    }
    scavenger_.PinAndPushObject(chunk, object, map_word);
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
  for (const auto& [object_address, map_word, object_size] : pinned_objects) {
    DCHECK(!map_word.IsForwardingAddress());
    Tagged<HeapObject> object = HeapObject::FromAddress(object_address);
    DCHECK(HeapLayout::IsSelfForwarded(object));
    object->set_map_word(map_word.ToMap(), kRelaxedStore);
    DCHECK(!HeapLayout::IsSelfForwarded(object));
    MemoryChunk* chunk = MemoryChunk::FromHeapObject(object);
    DCHECK(chunk->IsQuarantined());
    if (!chunk->IsFlagSet(MemoryChunk::WILL_BE_PROMOTED)) {
      quarantined_objects_size += object_size;
    }
  }
  new_space.SetQuarantinedSize(quarantined_objects_size);
}

void QuarantinePinnedPages(SemiSpaceNewSpace& new_space) {
  PageMetadata* next_page = new_space.from_space().first_page();
  while (next_page) {
    PageMetadata* current_page = next_page;
    next_page = current_page->next_page();
    MemoryChunk* chunk = current_page->Chunk();
    DCHECK(chunk->IsFromPage());
    if (!chunk->IsQuarantined()) {
      continue;
    }
    if (chunk->IsFlagSet(MemoryChunk::WILL_BE_PROMOTED)) {
      // free list categories will be relinked by the quarantined page sweeper
      // after sweeping is done.
      new_space.PromotePageToOldSpace(current_page,
                                      FreeMode::kDoNotLinkCategory);
      DCHECK(!chunk->InYoungGeneration());
    } else {
      new_space.MoveQuarantinedPage(chunk);
      DCHECK(!chunk->IsFromPage());
      DCHECK(chunk->IsToPage());
    }
    DCHECK(current_page->marking_bitmap()->IsClean());
    DCHECK(!chunk->IsFromPage());
    DCHECK(!chunk->IsQuarantined());
    DCHECK(!chunk->IsFlagSet(MemoryChunk::WILL_BE_PROMOTED));
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
      DCHECK(!chunk->IsQuarantined());
      ObjectsAndSizes& objects_for_page = pinned_object_per_page_[chunk];
      DCHECK(!std::any_of(objects_for_page.begin(), objects_for_page.end(),
                          [entry](auto& object_and_size) {
                            return object_and_size.first == entry.address;
                          }));
      objects_for_page.emplace_back(entry.address, entry.size);
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
    PageMetadata* page = static_cast<PageMetadata*>(chunk->Metadata());
    DCHECK(!chunk->IsFromPage());
    if (chunk->IsToPage()) {
      SweepPage(CreateFillerFreeSpaceHandler, chunk, page,
                next_page_iterator_->second);
    } else {
      DCHECK_EQ(chunk->Metadata()->owner()->identity(), OLD_SPACE);
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
  DCHECK_EQ(OLD_SPACE, PageMetadata::FromAddress(address)->owner()->identity());
  DCHECK(PageMetadata::FromAddress(address)->SweepingDone());
  OldSpace* const old_space = heap->old_space();
  old_space->FreeDuringSweep(address, size);
}

size_t ScavengerCollector::QuarantinedPageSweeper::JobTask::SweepPage(
    FreeSpaceHandler free_space_handler, MemoryChunk* chunk, PageMetadata* page,
    ObjectsAndSizes& pinned_objects_on_page) {
  DCHECK_EQ(page, chunk->Metadata());
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
  Scavenger::CopiedList copied_list;
  Scavenger::PinnedList pinned_list;
  Scavenger::PromotedList promoted_list;
  EphemeronRememberedSet::TableList ephemeron_table_list;

  PinnedObjects pinned_objects;

  const int num_scavenge_tasks = NumberOfScavengeTasks();
  std::vector<std::unique_ptr<Scavenger>> scavengers;

  const bool is_logging = isolate_->log_object_relocation();
  for (int i = 0; i < num_scavenge_tasks; ++i) {
    scavengers.emplace_back(
        new Scavenger(this, heap_, is_logging, &empty_chunks, &copied_list,
                      &pinned_list, &promoted_list, &ephemeron_table_list));
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
    DCHECK_IMPLIES(stack_scan_mode == Heap::StackScanMode::kSelective,
                   heap_->IsGCWithStack());
    if ((stack_scan_mode != Heap::StackScanMode::kNone) &&
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

    auto job = std::make_unique<JobTask>(
        this, &scavengers, std::move(old_to_new_chunks), copied_list,
        pinned_list, promoted_list);
    TRACE_GC_NOTE_WITH_FLOW("Parallel scavenge started", job->trace_id(),
                            TRACE_EVENT_FLAG_FLOW_OUT);
    V8::GetCurrentPlatform()
        ->CreateJob(v8::TaskPriority::kUserBlocking, std::move(job))
        ->Join();
    DCHECK(copied_list.IsEmpty());
    DCHECK(pinned_list.IsEmpty());
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

    LargePageMetadata* page = LargePageMetadata::FromHeapObject(object);
    SBXCHECK(page->IsLargePage());
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
                     EmptyChunksList* empty_chunks, CopiedList* copied_list,
                     PinnedList* pinned_list, PromotedList* promoted_list,
                     EphemeronRememberedSet::TableList* ephemeron_table_list)
    : collector_(collector),
      heap_(heap),
      local_empty_chunks_(*empty_chunks),
      local_copied_list_(*copied_list),
      local_pinned_list_(*pinned_list),
      local_promoted_list_(*promoted_list),
      local_ephemeron_table_list_(*ephemeron_table_list),
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

void Scavenger::IterateAndScavengePromotedObject(Tagged<HeapObject> target,
                                                 Tagged<Map> map, int size) {
  // We are not collecting slots on new space objects during mutation thus we
  // have to scan for pointers to evacuation candidates when we promote
  // objects. But we should not record any slots in non-black objects. Grey
  // object's slots would be rescanned. White object might not survive until
  // the end of collection it would be a violation of the invariant to record
  // its slots.
  IterateAndScavengePromotedObjectsVisitor visitor(this);

  // Iterate all outgoing pointers including map word.
  visitor.Visit(map, target, size);

  if (IsJSArrayBufferMap(map)) {
    DCHECK(!MemoryChunk::FromHeapObject(target)->IsLargePage());
    GCSafeCast<JSArrayBuffer>(target, heap_)->YoungMarkExtensionPromoted();
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

  if (chunk->executable()) {
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
  ScavengeVisitor scavenge_visitor(this);

  bool done;
  size_t objects = 0;
  do {
    done = true;
    Tagged<HeapObject> object;
    while (!ShouldEagerlyProcessPromotedList() &&
           local_copied_list_.Pop(&object)) {
      scavenge_visitor.Visit(object);
      done = false;
      if (delegate && ((++objects % kInterruptThreshold) == 0)) {
        if (!local_copied_list_.IsLocalEmpty()) {
          delegate->NotifyConcurrencyIncrease();
        }
      }
    }

    struct PromotedListEntry entry;
    while (local_promoted_list_.Pop(&entry)) {
      Tagged<HeapObject> target = entry.heap_object;
      IterateAndScavengePromotedObject(target, entry.map, entry.size);
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
  ephemeron_table_list->Iterate([this](Tagged<EphemeronHashTable> table) {
    for (InternalIndex i : table->IterateEntries()) {
      // Keys in EphemeronHashTables must be heap objects.
      HeapObjectSlot key_slot(
          table->RawFieldOfElementAt(EphemeronHashTable::EntryToIndex(i)));
      Tagged<HeapObject> key = key_slot.ToHeapObject();
      if (IsUnscavengedHeapObject(heap_, key)) {
        table->RemoveEntry(i);
      } else {
        Tagged<HeapObject> forwarded = ForwardingAddress(key);
        key_slot.StoreHeapObject(forwarded);
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
      if (IsUnscavengedHeapObject(heap_, key)) {
        table->RemoveEntry(InternalIndex(*iti));
        iti = indices.erase(iti);
      } else {
        Tagged<HeapObject> forwarded = ForwardingAddress(key);
        key_slot.StoreHeapObject(forwarded);
        if (!HeapLayout::InYoungGeneration(forwarded)) {
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

void Scavenger::Finalize() {
  heap()->pretenuring_handler()->MergeAllocationSitePretenuringFeedback(
      local_pretenuring_feedback_);
  for (const auto& it : local_ephemeron_remembered_set_) {
    // The ephemeron objects in the remembered set should be either large
    // objects, promoted to old space, or pinned objects on quarantined pages
    // that will be promoted.
    DCHECK_IMPLIES(
        !MemoryChunk::FromHeapObject(it.first)->IsLargePage(),
        !HeapLayout::InYoungGeneration(it.first) ||
            (HeapLayout::IsSelfForwarded(it.first) &&
             MemoryChunk::FromHeapObject(it.first)->IsQuarantined() &&
             MemoryChunk::FromHeapObject(it.first)->IsFlagSet(
                 MemoryChunk::WILL_BE_PROMOTED)));
    heap()->ephemeron_remembered_set()->RecordEphemeronKeyWrites(
        it.first, std::move(it.second));
  }
  heap()->IncrementNewSpaceSurvivingObjectSize(copied_size_);
  heap()->IncrementPromotedObjectsSize(promoted_size_);
  collector_->MergeSurvivingNewLargeObjects(local_surviving_new_large_objects_);
  allocator_.Finalize();
  local_empty_chunks_.Publish();
  local_ephemeron_table_list_.Publish();
}

void Scavenger::Publish() {
  local_copied_list_.Publish();
  local_pinned_list_.Publish();
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
  return HandleLargeObject(map, object, object->SizeFromMap(map),
                           Map::ObjectFieldsFrom(map->visitor_id()));
}

void Scavenger::PinAndPushObject(MemoryChunk* chunk, Tagged<HeapObject> object,
                                 MapWord map_word) {
  DCHECK(chunk->Metadata()->Contains(object->address()));
  DCHECK_EQ(map_word, object->map_word(kRelaxedLoad));
  Tagged<Map> map = map_word.ToMap();
  int object_size = object->SizeFromMap(map);
  PretenuringHandler::UpdateAllocationSite(heap_, map, object, object_size,
                                           &local_pretenuring_feedback_);
  object->set_map_word_forwarded(object, kRelaxedStore);
  DCHECK(object->map_word(kRelaxedLoad).IsForwardingAddress());
  DCHECK(HeapLayout::IsSelfForwarded(object));
  if (chunk->IsFlagSet(MemoryChunk::WILL_BE_PROMOTED)) {
    PushPinnedPromotedObject(object, map, object_size);
  } else {
    PushPinnedObject(object, map, object_size);
  }
}

void Scavenger::PushPinnedObject(Tagged<HeapObject> object, Tagged<Map> map,
                                 int object_size) {
  DCHECK(HeapLayout::IsSelfForwarded(object));
  DCHECK(!MemoryChunk::FromHeapObject(object)->IsFlagSet(
      MemoryChunk::WILL_BE_PROMOTED));
  DCHECK_EQ(object_size, object->SizeFromMap(map));
  local_pinned_list_.Push(ObjectAndMap(object, map));
  copied_size_ += object_size;
}

void Scavenger::PushPinnedPromotedObject(Tagged<HeapObject> object,
                                         Tagged<Map> map, int object_size) {
  DCHECK(HeapLayout::IsSelfForwarded(object));
  DCHECK(MemoryChunk::FromHeapObject(object)->IsFlagSet(
      MemoryChunk::WILL_BE_PROMOTED));
  DCHECK_EQ(object_size, object->SizeFromMap(map));
  local_promoted_list_.Push({object, map, object_size});
  promoted_size_ += object_size;
}

void Scavenger::VisitPinnedObjects() {
  ScavengeVisitor scavenge_visitor(this);

  ObjectAndMap object_and_map;
  while (local_pinned_list_.Pop(&object_and_map)) {
    DCHECK(HeapLayout::IsSelfForwarded(object_and_map.first));
    scavenge_visitor.Visit(object_and_map.second, object_and_map.first);
  }
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

ScavengeVisitor::ScavengeVisitor(Scavenger* scavenger)
    : NewSpaceVisitor<ScavengeVisitor>(scavenger->heap()->isolate()),
      scavenger_(scavenger) {}

}  // namespace internal
}  // namespace v8
