// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/scavenger.h"

#include "src/heap/array-buffer-sweeper.h"
#include "src/heap/barrier.h"
#include "src/heap/gc-tracer.h"
#include "src/heap/heap-inl.h"
#include "src/heap/invalidated-slots-inl.h"
#include "src/heap/item-parallel-job.h"
#include "src/heap/mark-compact-inl.h"
#include "src/heap/memory-chunk-inl.h"
#include "src/heap/objects-visiting-inl.h"
#include "src/heap/remembered-set-inl.h"
#include "src/heap/scavenger-inl.h"
#include "src/heap/sweeper.h"
#include "src/objects/data-handler-inl.h"
#include "src/objects/embedder-data-array-inl.h"
#include "src/objects/objects-body-descriptors-inl.h"
#include "src/objects/transitions-inl.h"
#include "src/utils/utils-inl.h"

namespace v8 {
namespace internal {

class IterateAndScavengePromotedObjectsVisitor final : public ObjectVisitor {
 public:
  IterateAndScavengePromotedObjectsVisitor(Scavenger* scavenger,
                                           bool record_slots)
      : scavenger_(scavenger), record_slots_(record_slots) {}

  V8_INLINE void VisitPointers(HeapObject host, ObjectSlot start,
                               ObjectSlot end) final {
    VisitPointersImpl(host, start, end);
  }

  V8_INLINE void VisitPointers(HeapObject host, MaybeObjectSlot start,
                               MaybeObjectSlot end) final {
    VisitPointersImpl(host, start, end);
  }

  V8_INLINE void VisitCodeTarget(Code host, RelocInfo* rinfo) final {
    Code target = Code::GetCodeFromTargetAddress(rinfo->target_address());
    HandleSlot(host, FullHeapObjectSlot(&target), target);
  }
  V8_INLINE void VisitEmbeddedPointer(Code host, RelocInfo* rinfo) final {
    HeapObject heap_object = rinfo->target_object();
    HandleSlot(host, FullHeapObjectSlot(&heap_object), heap_object);
  }

  inline void VisitEphemeron(HeapObject obj, int entry, ObjectSlot key,
                             ObjectSlot value) override {
    DCHECK(Heap::IsLargeObject(obj) || obj.IsEphemeronHashTable());
    VisitPointer(obj, value);

    if (ObjectInYoungGeneration(*key)) {
      // We cannot check the map here, as it might be a large object.
      scavenger_->RememberPromotedEphemeron(
          EphemeronHashTable::unchecked_cast(obj), entry);
    } else {
      VisitPointer(obj, key);
    }
  }

 private:
  template <typename TSlot>
  V8_INLINE void VisitPointersImpl(HeapObject host, TSlot start, TSlot end) {
    using THeapObjectSlot = typename TSlot::THeapObjectSlot;
    // Treat weak references as strong.
    // TODO(marja): Proper weakness handling in the young generation.
    for (TSlot slot = start; slot < end; ++slot) {
      typename TSlot::TObject object = *slot;
      HeapObject heap_object;
      if (object.GetHeapObject(&heap_object)) {
        HandleSlot(host, THeapObjectSlot(slot), heap_object);
      }
    }
  }

  template <typename THeapObjectSlot>
  V8_INLINE void HandleSlot(HeapObject host, THeapObjectSlot slot,
                            HeapObject target) {
    static_assert(
        std::is_same<THeapObjectSlot, FullHeapObjectSlot>::value ||
            std::is_same<THeapObjectSlot, HeapObjectSlot>::value,
        "Only FullHeapObjectSlot and HeapObjectSlot are expected here");
    scavenger_->PageMemoryFence(MaybeObject::FromObject(target));

    if (Heap::InFromPage(target)) {
      SlotCallbackResult result = scavenger_->ScavengeObject(slot, target);
      bool success = (*slot)->GetHeapObject(&target);
      USE(success);
      DCHECK(success);

      if (result == KEEP_SLOT) {
        SLOW_DCHECK(target.IsHeapObject());
        MemoryChunk* chunk = MemoryChunk::FromHeapObject(host);

        // Sweeper is stopped during scavenge, so we can directly
        // insert into its remembered set here.
        if (chunk->sweeping_slot_set()) {
          RememberedSetSweeping::Insert<AccessMode::ATOMIC>(chunk,
                                                            slot.address());
        } else {
          RememberedSet<OLD_TO_NEW>::Insert<AccessMode::ATOMIC>(chunk,
                                                                slot.address());
        }
      }
      SLOW_DCHECK(!MarkCompactCollector::IsOnEvacuationCandidate(
          HeapObject::cast(target)));
    } else if (record_slots_ && MarkCompactCollector::IsOnEvacuationCandidate(
                                    HeapObject::cast(target))) {
      // We should never try to record off-heap slots.
      DCHECK((std::is_same<THeapObjectSlot, HeapObjectSlot>::value));
      // We cannot call MarkCompactCollector::RecordSlot because that checks
      // that the host page is not in young generation, which does not hold
      // for pending large pages.
      RememberedSet<OLD_TO_OLD>::Insert<AccessMode::ATOMIC>(
          MemoryChunk::FromHeapObject(host), slot.address());
    }
  }

  Scavenger* const scavenger_;
  const bool record_slots_;
};

namespace {

V8_INLINE bool IsUnscavengedHeapObject(Heap* heap, Object object) {
  return Heap::InFromPage(object) &&
         !HeapObject::cast(object).map_word().IsForwardingAddress();
}

// Same as IsUnscavengedHeapObject() above but specialized for HeapObjects.
V8_INLINE bool IsUnscavengedHeapObject(Heap* heap, HeapObject heap_object) {
  return Heap::InFromPage(heap_object) &&
         !heap_object.map_word().IsForwardingAddress();
}

bool IsUnscavengedHeapObjectSlot(Heap* heap, FullObjectSlot p) {
  return IsUnscavengedHeapObject(heap, *p);
}

}  // namespace

class ScavengeWeakObjectRetainer : public WeakObjectRetainer {
 public:
  Object RetainAs(Object object) override {
    if (!Heap::InFromPage(object)) {
      return object;
    }

    MapWord map_word = HeapObject::cast(object).map_word();
    if (map_word.IsForwardingAddress()) {
      return map_word.ToForwardingAddress();
    }
    return Object();
  }
};

ScavengerCollector::JobTask::JobTask(
    ScavengerCollector* outer,
    std::vector<std::unique_ptr<Scavenger>>* scavengers,
    std::vector<std::pair<ParallelWorkItem, MemoryChunk*>> memory_chunks,
    Scavenger::CopiedList* copied_list,
    Scavenger::PromotionList* promotion_list)
    : outer_(outer),
      scavengers_(scavengers),
      memory_chunks_(std::move(memory_chunks)),
      remaining_memory_chunks_(memory_chunks_.size()),
      generator_(memory_chunks_.size()),
      copied_list_(copied_list),
      promotion_list_(promotion_list) {}

void ScavengerCollector::JobTask::Run(JobDelegate* delegate) {
  DCHECK_LT(delegate->GetTaskId(), scavengers_->size());
  Scavenger* scavenger = (*scavengers_)[delegate->GetTaskId()].get();
  if (delegate->IsJoiningThread()) {
    TRACE_GC(outer_->heap_->tracer(),
             GCTracer::Scope::SCAVENGER_SCAVENGE_PARALLEL);
    ProcessItems(delegate, scavenger);
  } else {
    TRACE_GC_EPOCH(outer_->heap_->tracer(),
                   GCTracer::Scope::SCAVENGER_BACKGROUND_SCAVENGE_PARALLEL,
                   ThreadKind::kBackground);
    ProcessItems(delegate, scavenger);
  }
}

size_t ScavengerCollector::JobTask::GetMaxConcurrency(
    size_t worker_count) const {
  // We need to account for local segments held by worker_count in addition to
  // GlobalPoolSize() of copied_list_ and promotion_list_.
  return std::min<size_t>(
      scavengers_->size(),
      std::max<size_t>(remaining_memory_chunks_.load(std::memory_order_relaxed),
                       worker_count + copied_list_->GlobalPoolSize() +
                           promotion_list_->GlobalPoolSize()));
}

void ScavengerCollector::JobTask::ProcessItems(JobDelegate* delegate,
                                               Scavenger* scavenger) {
  double scavenging_time = 0.0;
  {
    TimedScope scope(&scavenging_time);
    ConcurrentScavengePages(scavenger);
    scavenger->Process(delegate);
  }
  if (FLAG_trace_parallel_scavenge) {
    PrintIsolate(outer_->heap_->isolate(),
                 "scavenge[%p]: time=%.2f copied=%zu promoted=%zu\n",
                 static_cast<void*>(this), scavenging_time,
                 scavenger->bytes_copied(), scavenger->bytes_promoted());
  }
}

void ScavengerCollector::JobTask::ConcurrentScavengePages(
    Scavenger* scavenger) {
  while (remaining_memory_chunks_.load(std::memory_order_relaxed) > 0) {
    base::Optional<size_t> index = generator_.GetNext();
    if (!index) return;
    for (size_t i = *index; i < memory_chunks_.size(); ++i) {
      auto& work_item = memory_chunks_[i];
      if (!work_item.first.TryAcquire()) break;
      scavenger->ScavengePage(work_item.second);
      if (remaining_memory_chunks_.fetch_sub(1, std::memory_order_relaxed) <=
          1) {
        return;
      }
    }
  }
}

ScavengerCollector::ScavengerCollector(Heap* heap)
    : isolate_(heap->isolate()), heap_(heap) {}

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

void ScavengerCollector::CollectGarbage() {
  ScopedFullHeapCrashKey collect_full_heap_dump_if_crash(isolate_);

  DCHECK(surviving_new_large_objects_.empty());
  std::vector<std::unique_ptr<Scavenger>> scavengers;
  Worklist<MemoryChunk*, 64> empty_chunks;
  const int num_scavenge_tasks = NumberOfScavengeTasks();
  Scavenger::CopiedList copied_list(num_scavenge_tasks);
  Scavenger::PromotionList promotion_list(num_scavenge_tasks);
  EphemeronTableList ephemeron_table_list(num_scavenge_tasks);

  {
    Sweeper* sweeper = heap_->mark_compact_collector()->sweeper();

    // Try to finish sweeping here, such that the following code doesn't need to
    // pause & resume sweeping.
    if (sweeper->sweeping_in_progress() && FLAG_concurrent_sweeping &&
        !sweeper->AreSweeperTasksRunning()) {
      // At this point we know that all concurrent sweeping tasks have run
      // out-of-work and quit: all pages are swept. The main thread still needs
      // to complete sweeping though.
      heap_->mark_compact_collector()->EnsureSweepingCompleted();
    }

    // Pause the concurrent sweeper.
    Sweeper::PauseOrCompleteScope pause_scope(sweeper);
    // Filter out pages from the sweeper that need to be processed for old to
    // new slots by the Scavenger. After processing, the Scavenger adds back
    // pages that are still unsweeped. This way the Scavenger has exclusive
    // access to the slots of a page and can completely avoid any locks on
    // the page itself.
    Sweeper::FilterSweepingPagesScope filter_scope(sweeper, pause_scope);
    filter_scope.FilterOldSpaceSweepingPages([](Page* page) {
      return !page->ContainsSlots<OLD_TO_NEW>() && !page->sweeping_slot_set();
    });

    const bool is_logging = isolate_->LogObjectRelocation();
    for (int i = 0; i < num_scavenge_tasks; ++i) {
      scavengers.emplace_back(
          new Scavenger(this, heap_, is_logging, &empty_chunks, &copied_list,
                        &promotion_list, &ephemeron_table_list, i));
    }

    std::vector<std::pair<ParallelWorkItem, MemoryChunk*>> memory_chunks;
    RememberedSet<OLD_TO_NEW>::IterateMemoryChunks(
        heap_, [&memory_chunks](MemoryChunk* chunk) {
          memory_chunks.emplace_back(ParallelWorkItem{}, chunk);
        });

    RootScavengeVisitor root_scavenge_visitor(scavengers[kMainThreadId].get());

    {
      // Identify weak unmodified handles. Requires an unmodified graph.
      TRACE_GC(
          heap_->tracer(),
          GCTracer::Scope::SCAVENGER_SCAVENGE_WEAK_GLOBAL_HANDLES_IDENTIFY);
      isolate_->global_handles()->IdentifyWeakUnmodifiedObjects(
          &JSObject::IsUnmodifiedApiObject);
    }
    {
      // Copy roots.
      TRACE_GC(heap_->tracer(), GCTracer::Scope::SCAVENGER_SCAVENGE_ROOTS);
      // Scavenger treats all weak roots except for global handles as strong.
      // That is why we don't set skip_weak = true here and instead visit
      // global handles separately.
      base::EnumSet<SkipRoot> options({SkipRoot::kExternalStringTable,
                                       SkipRoot::kGlobalHandles,
                                       SkipRoot::kOldGeneration});
      if (V8_UNLIKELY(FLAG_scavenge_separate_stack_scanning)) {
        options.Add(SkipRoot::kStack);
      }
      heap_->IterateRoots(&root_scavenge_visitor, options);
      isolate_->global_handles()->IterateYoungStrongAndDependentRoots(
          &root_scavenge_visitor);
      scavengers[kMainThreadId]->Flush();
    }
    {
      // Parallel phase scavenging all copied and promoted objects.
      TRACE_GC(heap_->tracer(), GCTracer::Scope::SCAVENGER_SCAVENGE_PARALLEL);
      V8::GetCurrentPlatform()
          ->PostJob(v8::TaskPriority::kUserBlocking,
                    std::make_unique<JobTask>(this, &scavengers,
                                              std::move(memory_chunks),
                                              &copied_list, &promotion_list))
          ->Join();
      DCHECK(copied_list.IsEmpty());
      DCHECK(promotion_list.IsEmpty());
    }

    if (V8_UNLIKELY(FLAG_scavenge_separate_stack_scanning)) {
      IterateStackAndScavenge(&root_scavenge_visitor, &scavengers,
                              kMainThreadId);
      DCHECK(copied_list.IsEmpty());
      DCHECK(promotion_list.IsEmpty());
    }

    {
      // Scavenge weak global handles.
      TRACE_GC(heap_->tracer(),
               GCTracer::Scope::SCAVENGER_SCAVENGE_WEAK_GLOBAL_HANDLES_PROCESS);
      isolate_->global_handles()->MarkYoungWeakDeadObjectsPending(
          &IsUnscavengedHeapObjectSlot);
      isolate_->global_handles()->IterateYoungWeakDeadObjectsForFinalizers(
          &root_scavenge_visitor);
      scavengers[kMainThreadId]->Process();

      DCHECK(copied_list.IsEmpty());
      DCHECK(promotion_list.IsEmpty());
      isolate_->global_handles()->IterateYoungWeakObjectsForPhantomHandles(
          &root_scavenge_visitor, &IsUnscavengedHeapObjectSlot);
    }

    {
      // Finalize parallel scavenging.
      TRACE_GC(heap_->tracer(), GCTracer::Scope::SCAVENGER_SCAVENGE_FINALIZE);

      DCHECK(surviving_new_large_objects_.empty());

      for (auto& scavenger : scavengers) {
        scavenger->Finalize();
      }
      scavengers.clear();

      HandleSurvivingNewLargeObjects();
    }
  }

  {
    // Update references into new space
    TRACE_GC(heap_->tracer(), GCTracer::Scope::SCAVENGER_SCAVENGE_UPDATE_REFS);
    heap_->UpdateYoungReferencesInExternalStringTable(
        &Heap::UpdateYoungReferenceInExternalStringTableEntry);

    heap_->incremental_marking()->UpdateMarkingWorklistAfterScavenge();
  }

  if (FLAG_concurrent_marking) {
    // Ensure that concurrent marker does not track pages that are
    // going to be unmapped.
    for (Page* p :
         PageRange(heap_->new_space()->from_space().first_page(), nullptr)) {
      heap_->concurrent_marking()->ClearMemoryChunkData(p);
    }
  }

  ProcessWeakReferences(&ephemeron_table_list);

  // Set age mark.
  heap_->new_space_->set_age_mark(heap_->new_space()->top());

  // Since we promote all surviving large objects immediatelly, all remaining
  // large objects must be dead.
  // TODO(hpayer): Don't free all as soon as we have an intermediate generation.
  heap_->new_lo_space()->FreeDeadObjects([](HeapObject) { return true; });

  {
    TRACE_GC(heap_->tracer(), GCTracer::Scope::SCAVENGER_FREE_REMEMBERED_SET);
    MemoryChunk* chunk;

    while (empty_chunks.Pop(kMainThreadId, &chunk)) {
      // Since sweeping was already restarted only check chunks that already got
      // swept.
      if (chunk->SweepingDone()) {
        RememberedSet<OLD_TO_NEW>::CheckPossiblyEmptyBuckets(chunk);
      } else {
        chunk->possibly_empty_buckets()->Release();
      }
    }

#ifdef DEBUG
    RememberedSet<OLD_TO_NEW>::IterateMemoryChunks(
        heap_, [](MemoryChunk* chunk) {
          DCHECK(chunk->possibly_empty_buckets()->IsEmpty());
        });
#endif
  }

  {
    TRACE_GC(heap_->tracer(), GCTracer::Scope::SCAVENGER_SWEEP_ARRAY_BUFFERS);
    SweepArrayBufferExtensions();
  }

  // Update how much has survived scavenge.
  heap_->IncrementYoungSurvivorsCounter(heap_->SurvivedYoungObjectSize());
}

void ScavengerCollector::IterateStackAndScavenge(

    RootScavengeVisitor* root_scavenge_visitor,
    std::vector<std::unique_ptr<Scavenger>>* scavengers, int main_thread_id) {
  // Scan the stack, scavenge the newly discovered objects, and report
  // the survival statistics before and afer the stack scanning.
  // This code is not intended for production.
  TRACE_GC(heap_->tracer(), GCTracer::Scope::SCAVENGER_SCAVENGE_STACK_ROOTS);
  size_t survived_bytes_before = 0;
  for (auto& scavenger : *scavengers) {
    survived_bytes_before +=
        scavenger->bytes_copied() + scavenger->bytes_promoted();
  }
  heap_->IterateStackRoots(root_scavenge_visitor);
  (*scavengers)[main_thread_id]->Process();
  size_t survived_bytes_after = 0;
  for (auto& scavenger : *scavengers) {
    survived_bytes_after +=
        scavenger->bytes_copied() + scavenger->bytes_promoted();
  }
  TRACE_EVENT2(TRACE_DISABLED_BY_DEFAULT("v8.gc"),
               "V8.GCScavengerStackScanning", "survived_bytes_before",
               survived_bytes_before, "survived_bytes_after",
               survived_bytes_after);
  if (FLAG_trace_gc_verbose && !FLAG_trace_gc_ignore_scavenger) {
    isolate_->PrintWithTimestamp(
        "Scavenge stack scanning: survived_before=%4zuKB, "
        "survived_after=%4zuKB delta=%.1f%%\n",
        survived_bytes_before / KB, survived_bytes_after / KB,
        (survived_bytes_after - survived_bytes_before) * 100.0 /
            survived_bytes_after);
  }
}

void ScavengerCollector::SweepArrayBufferExtensions() {
  heap_->array_buffer_sweeper()->RequestSweepYoung();
}

void ScavengerCollector::HandleSurvivingNewLargeObjects() {
  for (SurvivingNewLargeObjectMapEntry update_info :
       surviving_new_large_objects_) {
    HeapObject object = update_info.first;
    Map map = update_info.second;
    // Order is important here. We have to re-install the map to have access
    // to meta-data like size during page promotion.
    object.set_map_word(MapWord::FromMap(map));
    LargePage* page = LargePage::FromHeapObject(object);
    heap_->lo_space()->PromoteNewLargeObject(page);
  }
  surviving_new_large_objects_.clear();
}

void ScavengerCollector::MergeSurvivingNewLargeObjects(
    const SurvivingNewLargeObjectsMap& objects) {
  for (SurvivingNewLargeObjectMapEntry object : objects) {
    bool success = surviving_new_large_objects_.insert(object).second;
    USE(success);
    DCHECK(success);
  }
}

int ScavengerCollector::NumberOfScavengeTasks() {
  if (!FLAG_parallel_scavenge) return 1;
  const int num_scavenge_tasks =
      static_cast<int>(heap_->new_space()->TotalCapacity()) / MB + 1;
  static int num_cores = V8::GetCurrentPlatform()->NumberOfWorkerThreads() + 1;
  int tasks = std::max(
      1, std::min({num_scavenge_tasks, kMaxScavengerTasks, num_cores}));
  if (!heap_->CanPromoteYoungAndExpandOldGeneration(
          static_cast<size_t>(tasks * Page::kPageSize))) {
    // Optimize for memory usage near the heap limit.
    tasks = 1;
  }
  return tasks;
}

Scavenger::Scavenger(ScavengerCollector* collector, Heap* heap, bool is_logging,
                     Worklist<MemoryChunk*, 64>* empty_chunks,
                     CopiedList* copied_list, PromotionList* promotion_list,
                     EphemeronTableList* ephemeron_table_list, int task_id)
    : collector_(collector),
      heap_(heap),
      empty_chunks_(empty_chunks, task_id),
      promotion_list_(promotion_list, task_id),
      copied_list_(copied_list, task_id),
      ephemeron_table_list_(ephemeron_table_list, task_id),
      local_pretenuring_feedback_(kInitialLocalPretenuringFeedbackCapacity),
      copied_size_(0),
      promoted_size_(0),
      allocator_(heap, LocalSpaceKind::kCompactionSpaceForScavenge),
      is_logging_(is_logging),
      is_incremental_marking_(heap->incremental_marking()->IsMarking()),
      is_compacting_(heap->incremental_marking()->IsCompacting()) {}

void Scavenger::IterateAndScavengePromotedObject(HeapObject target, Map map,
                                                 int size) {
  // We are not collecting slots on new space objects during mutation thus we
  // have to scan for pointers to evacuation candidates when we promote
  // objects. But we should not record any slots in non-black objects. Grey
  // object's slots would be rescanned. White object might not survive until
  // the end of collection it would be a violation of the invariant to record
  // its slots.
  const bool record_slots =
      is_compacting_ &&
      heap()->incremental_marking()->atomic_marking_state()->IsBlack(target);

  IterateAndScavengePromotedObjectsVisitor visitor(this, record_slots);
  target.IterateBodyFast(map, size, &visitor);

  if (map.IsJSArrayBufferMap()) {
    DCHECK(!BasicMemoryChunk::FromHeapObject(target)->IsLargePage());
    JSArrayBuffer::cast(target).YoungMarkExtensionPromoted();
  }
}

void Scavenger::RememberPromotedEphemeron(EphemeronHashTable table, int entry) {
  auto indices =
      ephemeron_remembered_set_.insert({table, std::unordered_set<int>()});
  indices.first->second.insert(entry);
}

void Scavenger::AddPageToSweeperIfNecessary(MemoryChunk* page) {
  AllocationSpace space = page->owner_identity();
  if ((space == OLD_SPACE) && !page->SweepingDone()) {
    heap()->mark_compact_collector()->sweeper()->AddPage(
        space, reinterpret_cast<Page*>(page),
        Sweeper::READD_TEMPORARY_REMOVED_PAGE);
  }
}

void Scavenger::ScavengePage(MemoryChunk* page) {
  CodePageMemoryModificationScope memory_modification_scope(page);

  if (page->slot_set<OLD_TO_NEW, AccessMode::ATOMIC>() != nullptr) {
    InvalidatedSlotsFilter filter = InvalidatedSlotsFilter::OldToNew(page);
    RememberedSet<OLD_TO_NEW>::IterateAndTrackEmptyBuckets(
        page,
        [this, &filter](MaybeObjectSlot slot) {
          if (!filter.IsValid(slot.address())) return REMOVE_SLOT;
          return CheckAndScavengeObject(heap_, slot);
        },
        empty_chunks_);
  }

  if (page->sweeping_slot_set<AccessMode::NON_ATOMIC>() != nullptr) {
    InvalidatedSlotsFilter filter = InvalidatedSlotsFilter::OldToNew(page);
    RememberedSetSweeping::Iterate(
        page,
        [this, &filter](MaybeObjectSlot slot) {
          if (!filter.IsValid(slot.address())) return REMOVE_SLOT;
          return CheckAndScavengeObject(heap_, slot);
        },
        SlotSet::KEEP_EMPTY_BUCKETS);
  }

  if (page->invalidated_slots<OLD_TO_NEW>() != nullptr) {
    // The invalidated slots are not needed after old-to-new slots were
    // processed.
    page->ReleaseInvalidatedSlots<OLD_TO_NEW>();
  }

  RememberedSet<OLD_TO_NEW>::IterateTyped(
      page, [=](SlotType type, Address addr) {
        return UpdateTypedSlotHelper::UpdateTypedSlot(
            heap_, type, addr, [this](FullMaybeObjectSlot slot) {
              return CheckAndScavengeObject(heap(), slot);
            });
      });

  AddPageToSweeperIfNecessary(page);
}

void Scavenger::Process(JobDelegate* delegate) {
  ScavengeVisitor scavenge_visitor(this);

  bool done;
  size_t objects = 0;
  do {
    done = true;
    ObjectAndSize object_and_size;
    while (promotion_list_.ShouldEagerlyProcessPromotionList() &&
           copied_list_.Pop(&object_and_size)) {
      scavenge_visitor.Visit(object_and_size.first);
      done = false;
      if (delegate && ((++objects % kInterruptThreshold) == 0)) {
        if (!copied_list_.IsGlobalPoolEmpty()) {
          delegate->NotifyConcurrencyIncrease();
        }
      }
    }

    struct PromotionListEntry entry;
    while (promotion_list_.Pop(&entry)) {
      HeapObject target = entry.heap_object;
      IterateAndScavengePromotedObject(target, entry.map, entry.size);
      done = false;
      if (delegate && ((++objects % kInterruptThreshold) == 0)) {
        if (!promotion_list_.IsGlobalPoolEmpty()) {
          delegate->NotifyConcurrencyIncrease();
        }
      }
    }
  } while (!done);
}

void ScavengerCollector::ProcessWeakReferences(
    EphemeronTableList* ephemeron_table_list) {
  ScavengeWeakObjectRetainer weak_object_retainer;
  heap_->ProcessYoungWeakReferences(&weak_object_retainer);
  ClearYoungEphemerons(ephemeron_table_list);
  ClearOldEphemerons();
}

// Clear ephemeron entries from EphemeronHashTables in new-space whenever the
// entry has a dead new-space key.
void ScavengerCollector::ClearYoungEphemerons(
    EphemeronTableList* ephemeron_table_list) {
  ephemeron_table_list->Iterate([this](EphemeronHashTable table) {
    for (InternalIndex i : table.IterateEntries()) {
      // Keys in EphemeronHashTables must be heap objects.
      HeapObjectSlot key_slot(
          table.RawFieldOfElementAt(EphemeronHashTable::EntryToIndex(i)));
      HeapObject key = key_slot.ToHeapObject();
      if (IsUnscavengedHeapObject(heap_, key)) {
        table.RemoveEntry(i);
      } else {
        HeapObject forwarded = ForwardingAddress(key);
        key_slot.StoreHeapObject(forwarded);
      }
    }
  });
  ephemeron_table_list->Clear();
}

// Clear ephemeron entries from EphemeronHashTables in old-space whenever the
// entry has a dead new-space key.
void ScavengerCollector::ClearOldEphemerons() {
  for (auto it = heap_->ephemeron_remembered_set_.begin();
       it != heap_->ephemeron_remembered_set_.end();) {
    EphemeronHashTable table = it->first;
    auto& indices = it->second;
    for (auto iti = indices.begin(); iti != indices.end();) {
      // Keys in EphemeronHashTables must be heap objects.
      HeapObjectSlot key_slot(table.RawFieldOfElementAt(
          EphemeronHashTable::EntryToIndex(InternalIndex(*iti))));
      HeapObject key = key_slot.ToHeapObject();
      if (IsUnscavengedHeapObject(heap_, key)) {
        table.RemoveEntry(InternalIndex(*iti));
        iti = indices.erase(iti);
      } else {
        HeapObject forwarded = ForwardingAddress(key);
        key_slot.StoreHeapObject(forwarded);
        if (!Heap::InYoungGeneration(forwarded)) {
          iti = indices.erase(iti);
        } else {
          ++iti;
        }
      }
    }

    if (indices.size() == 0) {
      it = heap_->ephemeron_remembered_set_.erase(it);
    } else {
      ++it;
    }
  }
}

void Scavenger::Finalize() {
  heap()->MergeAllocationSitePretenuringFeedback(local_pretenuring_feedback_);
  heap()->IncrementSemiSpaceCopiedObjectSize(copied_size_);
  heap()->IncrementPromotedObjectsSize(promoted_size_);
  collector_->MergeSurvivingNewLargeObjects(surviving_new_large_objects_);
  allocator_.Finalize();
  empty_chunks_.FlushToGlobal();
  ephemeron_table_list_.FlushToGlobal();
  for (auto it = ephemeron_remembered_set_.begin();
       it != ephemeron_remembered_set_.end(); ++it) {
    auto insert_result = heap()->ephemeron_remembered_set_.insert(
        {it->first, std::unordered_set<int>()});
    for (int entry : it->second) {
      insert_result.first->second.insert(entry);
    }
  }
}

void Scavenger::Flush() {
  copied_list_.FlushToGlobal();
  promotion_list_.FlushToGlobal();
}

void Scavenger::AddEphemeronHashTable(EphemeronHashTable table) {
  ephemeron_table_list_.Push(table);
}

void RootScavengeVisitor::VisitRootPointer(Root root, const char* description,
                                           FullObjectSlot p) {
  DCHECK(!HasWeakHeapObjectTag(*p));
  ScavengePointer(p);
}

void RootScavengeVisitor::VisitRootPointers(Root root, const char* description,
                                            FullObjectSlot start,
                                            FullObjectSlot end) {
  // Copy all HeapObject pointers in [start, end)
  for (FullObjectSlot p = start; p < end; ++p) ScavengePointer(p);
}

void RootScavengeVisitor::ScavengePointer(FullObjectSlot p) {
  Object object = *p;
  DCHECK(!HasWeakHeapObjectTag(object));
  if (Heap::InYoungGeneration(object)) {
    scavenger_->ScavengeObject(FullHeapObjectSlot(p), HeapObject::cast(object));
  }
}

RootScavengeVisitor::RootScavengeVisitor(Scavenger* scavenger)
    : scavenger_(scavenger) {}

ScavengeVisitor::ScavengeVisitor(Scavenger* scavenger)
    : scavenger_(scavenger) {}

}  // namespace internal
}  // namespace v8
