// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/scavenger.h"

#include "src/heap/array-buffer-collector.h"
#include "src/heap/barrier.h"
#include "src/heap/gc-tracer.h"
#include "src/heap/heap-inl.h"
#include "src/heap/item-parallel-job.h"
#include "src/heap/mark-compact-inl.h"
#include "src/heap/objects-visiting-inl.h"
#include "src/heap/scavenger-inl.h"
#include "src/heap/sweeper.h"
#include "src/objects-body-descriptors-inl.h"
#include "src/utils-inl.h"

namespace v8 {
namespace internal {

class PageScavengingItem final : public ItemParallelJob::Item {
 public:
  explicit PageScavengingItem(MemoryChunk* chunk) : chunk_(chunk) {}
  ~PageScavengingItem() override = default;

  void Process(Scavenger* scavenger) { scavenger->ScavengePage(chunk_); }

 private:
  MemoryChunk* const chunk_;
};

class ScavengingTask final : public ItemParallelJob::Task {
 public:
  ScavengingTask(Heap* heap, Scavenger* scavenger, OneshotBarrier* barrier)
      : ItemParallelJob::Task(heap->isolate()),
        heap_(heap),
        scavenger_(scavenger),
        barrier_(barrier) {}

  void RunInParallel() final {
    TRACE_BACKGROUND_GC(
        heap_->tracer(),
        GCTracer::BackgroundScope::SCAVENGER_BACKGROUND_SCAVENGE_PARALLEL);
    double scavenging_time = 0.0;
    {
      barrier_->Start();
      TimedScope scope(&scavenging_time);
      PageScavengingItem* item = nullptr;
      while ((item = GetItem<PageScavengingItem>()) != nullptr) {
        item->Process(scavenger_);
        item->MarkFinished();
      }
      do {
        scavenger_->Process(barrier_);
      } while (!barrier_->Wait());
      scavenger_->Process();
    }
    if (FLAG_trace_parallel_scavenge) {
      PrintIsolate(heap_->isolate(),
                   "scavenge[%p]: time=%.2f copied=%zu promoted=%zu\n",
                   static_cast<void*>(this), scavenging_time,
                   scavenger_->bytes_copied(), scavenger_->bytes_promoted());
    }
  };

 private:
  Heap* const heap_;
  Scavenger* const scavenger_;
  OneshotBarrier* const barrier_;
};

class IterateAndScavengePromotedObjectsVisitor final : public ObjectVisitor {
 public:
  IterateAndScavengePromotedObjectsVisitor(Heap* heap, Scavenger* scavenger,
                                           bool record_slots)
      : heap_(heap), scavenger_(scavenger), record_slots_(record_slots) {}

  inline void VisitPointers(HeapObject* host, Object** start,
                            Object** end) final {
    for (Object** slot = start; slot < end; ++slot) {
      Object* target = *slot;
      DCHECK(!HasWeakHeapObjectTag(target));
      if (target->IsHeapObject()) {
        HandleSlot(host, reinterpret_cast<Address>(slot),
                   HeapObject::cast(target));
      }
    }
  }

  inline void VisitPointers(HeapObject* host, MaybeObject** start,
                            MaybeObject** end) final {
    // Treat weak references as strong. TODO(marja): Proper weakness handling in
    // the young generation.
    for (MaybeObject** slot = start; slot < end; ++slot) {
      MaybeObject* target = *slot;
      HeapObject* heap_object;
      if (target->GetHeapObject(&heap_object)) {
        HandleSlot(host, reinterpret_cast<Address>(slot), heap_object);
      }
    }
  }

  inline void HandleSlot(HeapObject* host, Address slot_address,
                         HeapObject* target) {
    HeapObjectReference** slot =
        reinterpret_cast<HeapObjectReference**>(slot_address);
    scavenger_->PageMemoryFence(reinterpret_cast<MaybeObject*>(target));

    if (Heap::InFromSpace(target)) {
      SlotCallbackResult result = scavenger_->ScavengeObject(slot, target);
      bool success = (*slot)->GetHeapObject(&target);
      USE(success);
      DCHECK(success);

      if (result == KEEP_SLOT) {
        SLOW_DCHECK(target->IsHeapObject());
        RememberedSet<OLD_TO_NEW>::Insert(Page::FromAddress(slot_address),
                                          slot_address);
      }
      SLOW_DCHECK(!MarkCompactCollector::IsOnEvacuationCandidate(
          HeapObject::cast(target)));
    } else if (record_slots_ && MarkCompactCollector::IsOnEvacuationCandidate(
                                    HeapObject::cast(target))) {
      heap_->mark_compact_collector()->RecordSlot(host, slot, target);
    }
  }

 private:
  Heap* const heap_;
  Scavenger* const scavenger_;
  const bool record_slots_;
};

static bool IsUnscavengedHeapObject(Heap* heap, Object** p) {
  return Heap::InFromSpace(*p) &&
         !HeapObject::cast(*p)->map_word().IsForwardingAddress();
}

class ScavengeWeakObjectRetainer : public WeakObjectRetainer {
 public:
  Object* RetainAs(Object* object) override {
    if (!Heap::InFromSpace(object)) {
      return object;
    }

    MapWord map_word = HeapObject::cast(object)->map_word();
    if (map_word.IsForwardingAddress()) {
      return map_word.ToForwardingAddress();
    }
    return nullptr;
  }
};

ScavengerCollector::ScavengerCollector(Heap* heap)
    : isolate_(heap->isolate()), heap_(heap), parallel_scavenge_semaphore_(0) {}

void ScavengerCollector::CollectGarbage() {
  ItemParallelJob job(isolate_->cancelable_task_manager(),
                      &parallel_scavenge_semaphore_);
  const int kMainThreadId = 0;
  Scavenger* scavengers[kMaxScavengerTasks];
  const bool is_logging = isolate_->LogObjectRelocation();
  const int num_scavenge_tasks = NumberOfScavengeTasks();
  OneshotBarrier barrier;
  Scavenger::CopiedList copied_list(num_scavenge_tasks);
  Scavenger::PromotionList promotion_list(num_scavenge_tasks);
  for (int i = 0; i < num_scavenge_tasks; i++) {
    scavengers[i] = new Scavenger(this, heap_, is_logging, &copied_list,
                                  &promotion_list, i);
    job.AddTask(new ScavengingTask(heap_, scavengers[i], &barrier));
  }

  {
    Sweeper* sweeper = heap_->mark_compact_collector()->sweeper();
    // Pause the concurrent sweeper.
    Sweeper::PauseOrCompleteScope pause_scope(sweeper);
    // Filter out pages from the sweeper that need to be processed for old to
    // new slots by the Scavenger. After processing, the Scavenger adds back
    // pages that are still unsweeped. This way the Scavenger has exclusive
    // access to the slots of a page and can completely avoid any locks on
    // the page itself.
    Sweeper::FilterSweepingPagesScope filter_scope(sweeper, pause_scope);
    filter_scope.FilterOldSpaceSweepingPages(
        [](Page* page) { return !page->ContainsSlots<OLD_TO_NEW>(); });
    RememberedSet<OLD_TO_NEW>::IterateMemoryChunks(
        heap_, [&job](MemoryChunk* chunk) {
          job.AddItem(new PageScavengingItem(chunk));
        });

    RootScavengeVisitor root_scavenge_visitor(scavengers[kMainThreadId]);

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
      heap_->IterateRoots(&root_scavenge_visitor, VISIT_ALL_IN_SCAVENGE);
    }
    {
      // Parallel phase scavenging all copied and promoted objects.
      TRACE_GC(heap_->tracer(), GCTracer::Scope::SCAVENGER_SCAVENGE_PARALLEL);
      job.Run(isolate_->async_counters());
      DCHECK(copied_list.IsEmpty());
      DCHECK(promotion_list.IsEmpty());
    }
    {
      // Scavenge weak global handles.
      TRACE_GC(heap_->tracer(),
               GCTracer::Scope::SCAVENGER_SCAVENGE_WEAK_GLOBAL_HANDLES_PROCESS);
      isolate_->global_handles()->MarkNewSpaceWeakUnmodifiedObjectsPending(
          &IsUnscavengedHeapObject);
      isolate_->global_handles()
          ->IterateNewSpaceWeakUnmodifiedRootsForFinalizers(
              &root_scavenge_visitor);
      scavengers[kMainThreadId]->Process();

      DCHECK(copied_list.IsEmpty());
      DCHECK(promotion_list.IsEmpty());
      isolate_->global_handles()
          ->IterateNewSpaceWeakUnmodifiedRootsForPhantomHandles(
              &root_scavenge_visitor, &IsUnscavengedHeapObject);
    }

    {
      // Finalize parallel scavenging.
      TRACE_GC(heap_->tracer(), GCTracer::Scope::SCAVENGER_SCAVENGE_FINALIZE);

      for (int i = 0; i < num_scavenge_tasks; i++) {
        scavengers[i]->Finalize();
        delete scavengers[i];
      }

      HandleSurvivingNewLargeObjects();
    }
  }

  {
    // Update references into new space
    TRACE_GC(heap_->tracer(), GCTracer::Scope::SCAVENGER_SCAVENGE_UPDATE_REFS);
    heap_->UpdateNewSpaceReferencesInExternalStringTable(
        &Heap::UpdateNewSpaceReferenceInExternalStringTableEntry);

    heap_->incremental_marking()->UpdateMarkingWorklistAfterScavenge();
  }

  if (FLAG_concurrent_marking) {
    // Ensure that concurrent marker does not track pages that are
    // going to be unmapped.
    for (Page* p :
         PageRange(heap_->new_space()->from_space().first_page(), nullptr)) {
      heap_->concurrent_marking()->ClearLiveness(p);
    }
  }

  ScavengeWeakObjectRetainer weak_object_retainer;
  heap_->ProcessYoungWeakReferences(&weak_object_retainer);

  // Set age mark.
  heap_->new_space_->set_age_mark(heap_->new_space()->top());

  {
    TRACE_GC(heap_->tracer(), GCTracer::Scope::SCAVENGER_PROCESS_ARRAY_BUFFERS);
    ArrayBufferTracker::PrepareToFreeDeadInNewSpace(heap_);
  }
  heap_->array_buffer_collector()->FreeAllocations();

  RememberedSet<OLD_TO_NEW>::IterateMemoryChunks(heap_, [](MemoryChunk* chunk) {
    if (chunk->SweepingDone()) {
      RememberedSet<OLD_TO_NEW>::FreeEmptyBuckets(chunk);
    } else {
      RememberedSet<OLD_TO_NEW>::PreFreeEmptyBuckets(chunk);
    }
  });

  // Update how much has survived scavenge.
  heap_->IncrementYoungSurvivorsCounter(heap_->SurvivedNewSpaceObjectSize());

  // Scavenger may find new wrappers by iterating objects promoted onto a black
  // page.
  heap_->local_embedder_heap_tracer()->RegisterWrappersWithRemoteTracer();
}

void ScavengerCollector::HandleSurvivingNewLargeObjects() {
  for (SurvivingNewLargeObjectMapEntry update_info :
       surviving_new_large_objects_) {
    HeapObject* object = update_info.first;
    Map* map = update_info.second;
    // Order is important here. We have to re-install the map to have access
    // to meta-data like size during page promotion.
    object->set_map_word(MapWord::FromMap(map));
    LargePage* page = LargePage::FromHeapObject(object);
    heap_->lo_space()->PromoteNewLargeObject(page);
  }
  DCHECK(heap_->new_lo_space()->IsEmpty());
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
      static_cast<int>(heap_->new_space()->TotalCapacity()) / MB;
  static int num_cores = V8::GetCurrentPlatform()->NumberOfWorkerThreads() + 1;
  int tasks =
      Max(1, Min(Min(num_scavenge_tasks, kMaxScavengerTasks), num_cores));
  if (!heap_->CanExpandOldGeneration(
          static_cast<size_t>(tasks * Page::kPageSize))) {
    // Optimize for memory usage near the heap limit.
    tasks = 1;
  }
  return tasks;
}

Scavenger::Scavenger(ScavengerCollector* collector, Heap* heap, bool is_logging,
                     CopiedList* copied_list, PromotionList* promotion_list,
                     int task_id)
    : collector_(collector),
      heap_(heap),
      promotion_list_(promotion_list, task_id),
      copied_list_(copied_list, task_id),
      local_pretenuring_feedback_(kInitialLocalPretenuringFeedbackCapacity),
      copied_size_(0),
      promoted_size_(0),
      allocator_(heap),
      is_logging_(is_logging),
      is_incremental_marking_(heap->incremental_marking()->IsMarking()),
      is_compacting_(heap->incremental_marking()->IsCompacting()) {}

void Scavenger::IterateAndScavengePromotedObject(HeapObject* target, Map* map,
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
  IterateAndScavengePromotedObjectsVisitor visitor(heap(), this, record_slots);
  target->IterateBodyFast(map, size, &visitor);
}

void Scavenger::AddPageToSweeperIfNecessary(MemoryChunk* page) {
  AllocationSpace space = page->owner()->identity();
  if ((space == OLD_SPACE) && !page->SweepingDone()) {
    heap()->mark_compact_collector()->sweeper()->AddPage(
        space, reinterpret_cast<Page*>(page),
        Sweeper::READD_TEMPORARY_REMOVED_PAGE);
  }
}

void Scavenger::ScavengePage(MemoryChunk* page) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.gc"), "Scavenger::ScavengePage");
  CodePageMemoryModificationScope memory_modification_scope(page);
  RememberedSet<OLD_TO_NEW>::Iterate(
      page,
      [this](Address addr) { return CheckAndScavengeObject(heap_, addr); },
      SlotSet::KEEP_EMPTY_BUCKETS);
  RememberedSet<OLD_TO_NEW>::IterateTyped(
      page, [this](SlotType type, Address host_addr, Address addr) {
        return UpdateTypedSlotHelper::UpdateTypedSlot(
            heap_, type, addr, [this](MaybeObject** addr) {
              return CheckAndScavengeObject(heap(),
                                            reinterpret_cast<Address>(addr));
            });
      });

  AddPageToSweeperIfNecessary(page);
}

void Scavenger::Process(OneshotBarrier* barrier) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.gc"), "Scavenger::Process");
  ScavengeVisitor scavenge_visitor(this);

  const bool have_barrier = barrier != nullptr;
  bool done;
  size_t objects = 0;
  do {
    done = true;
    ObjectAndSize object_and_size;
    while (promotion_list_.ShouldEagerlyProcessPromotionList() &&
           copied_list_.Pop(&object_and_size)) {
      scavenge_visitor.Visit(object_and_size.first);
      done = false;
      if (have_barrier && ((++objects % kInterruptThreshold) == 0)) {
        if (!copied_list_.IsGlobalPoolEmpty()) {
          barrier->NotifyAll();
        }
      }
    }

    struct PromotionListEntry entry;
    while (promotion_list_.Pop(&entry)) {
      HeapObject* target = entry.heap_object;
      DCHECK(!target->IsMap());
      IterateAndScavengePromotedObject(target, entry.map, entry.size);
      done = false;
      if (have_barrier && ((++objects % kInterruptThreshold) == 0)) {
        if (!promotion_list_.IsGlobalPoolEmpty()) {
          barrier->NotifyAll();
        }
      }
    }
  } while (!done);
}

void Scavenger::Finalize() {
  heap()->MergeAllocationSitePretenuringFeedback(local_pretenuring_feedback_);
  heap()->IncrementSemiSpaceCopiedObjectSize(copied_size_);
  heap()->IncrementPromotedObjectsSize(promoted_size_);
  collector_->MergeSurvivingNewLargeObjects(surviving_new_large_objects_);
  allocator_.Finalize();
}

void RootScavengeVisitor::VisitRootPointer(Root root, const char* description,
                                           Object** p) {
  DCHECK(!HasWeakHeapObjectTag(*p));
  ScavengePointer(p);
}

void RootScavengeVisitor::VisitRootPointers(Root root, const char* description,
                                            Object** start, Object** end) {
  // Copy all HeapObject pointers in [start, end)
  for (Object** p = start; p < end; p++) ScavengePointer(p);
}

void RootScavengeVisitor::ScavengePointer(Object** p) {
  Object* object = *p;
  DCHECK(!HasWeakHeapObjectTag(object));
  if (!Heap::InNewSpace(object)) return;

  scavenger_->ScavengeObject(reinterpret_cast<HeapObjectReference**>(p),
                             reinterpret_cast<HeapObject*>(object));
}

RootScavengeVisitor::RootScavengeVisitor(Scavenger* scavenger)
    : scavenger_(scavenger) {}

ScavengeVisitor::ScavengeVisitor(Scavenger* scavenger)
    : scavenger_(scavenger) {}

}  // namespace internal
}  // namespace v8
