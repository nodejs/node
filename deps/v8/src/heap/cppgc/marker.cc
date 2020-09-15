// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/marker.h"

#include <memory>

#include "include/cppgc/internal/process-heap.h"
#include "include/cppgc/platform.h"
#include "src/heap/cppgc/heap-object-header.h"
#include "src/heap/cppgc/heap-page.h"
#include "src/heap/cppgc/heap-visitor.h"
#include "src/heap/cppgc/heap.h"
#include "src/heap/cppgc/liveness-broker.h"
#include "src/heap/cppgc/marking-state.h"
#include "src/heap/cppgc/marking-visitor.h"
#include "src/heap/cppgc/stats-collector.h"

#if defined(CPPGC_CAGED_HEAP)
#include "include/cppgc/internal/caged-heap-local-data.h"
#endif

namespace cppgc {
namespace internal {

namespace {

bool EnterIncrementalMarkingIfNeeded(Marker::MarkingConfig config,
                                     HeapBase& heap) {
  if (config.marking_type == Marker::MarkingConfig::MarkingType::kIncremental ||
      config.marking_type ==
          Marker::MarkingConfig::MarkingType::kIncrementalAndConcurrent) {
    ProcessHeap::EnterIncrementalOrConcurrentMarking();
#if defined(CPPGC_CAGED_HEAP)
    heap.caged_heap().local_data().is_marking_in_progress = true;
#endif
    return true;
  }
  return false;
}

bool ExitIncrementalMarkingIfNeeded(Marker::MarkingConfig config,
                                    HeapBase& heap) {
  if (config.marking_type == Marker::MarkingConfig::MarkingType::kIncremental ||
      config.marking_type ==
          Marker::MarkingConfig::MarkingType::kIncrementalAndConcurrent) {
    ProcessHeap::ExitIncrementalOrConcurrentMarking();
#if defined(CPPGC_CAGED_HEAP)
    heap.caged_heap().local_data().is_marking_in_progress = false;
#endif
    return true;
  }
  return false;
}

// Visit remembered set that was recorded in the generational barrier.
void VisitRememberedSlots(HeapBase& heap, MarkingState& marking_state) {
#if defined(CPPGC_YOUNG_GENERATION)
  for (void* slot : heap.remembered_slots()) {
    auto& slot_header = BasePage::FromInnerAddress(&heap, slot)
                            ->ObjectHeaderFromInnerAddress(slot);
    if (slot_header.IsYoung()) continue;
    // The design of young generation requires collections to be executed at the
    // top level (with the guarantee that no objects are currently being in
    // construction). This can be ensured by running young GCs from safe points
    // or by reintroducing nested allocation scopes that avoid finalization.
    DCHECK(
        !header.IsInConstruction<HeapObjectHeader::AccessMode::kNonAtomic>());

    void* value = *reinterpret_cast<void**>(slot);
    marking_state.DynamicallyMarkAddress(static_cast<Address>(value));
  }
#endif
}

// Assumes that all spaces have their LABs reset.
void ResetRememberedSet(HeapBase& heap) {
#if defined(CPPGC_YOUNG_GENERATION)
  auto& local_data = heap.caged_heap().local_data();
  local_data.age_table.Reset(&heap.caged_heap().allocator());
  heap.remembered_slots().clear();
#endif
}

static constexpr size_t kDefaultDeadlineCheckInterval = 150u;

template <size_t kDeadlineCheckInterval = kDefaultDeadlineCheckInterval,
          typename WorklistLocal, typename Callback, typename Predicate>
bool DrainWorklistWithDeadline(Predicate should_yield,
                               WorklistLocal& worklist_local,
                               Callback callback) {
  size_t processed_callback_count = 0;
  typename WorklistLocal::ItemType item;
  while (worklist_local.Pop(&item)) {
    callback(item);
    if (processed_callback_count-- == 0) {
      if (should_yield()) {
        return false;
      }
      processed_callback_count = kDeadlineCheckInterval;
    }
  }
  return true;
}

template <size_t kDeadlineCheckInterval = kDefaultDeadlineCheckInterval,
          typename WorklistLocal, typename Callback>
bool DrainWorklistWithBytesAndTimeDeadline(MarkingState& marking_state,
                                           size_t marked_bytes_deadline,
                                           v8::base::TimeTicks time_deadline,
                                           WorklistLocal& worklist_local,
                                           Callback callback) {
  return DrainWorklistWithDeadline(
      [&marking_state, marked_bytes_deadline, time_deadline]() {
        return (marked_bytes_deadline <= marking_state.marked_bytes()) ||
               (time_deadline <= v8::base::TimeTicks::Now());
      },
      worklist_local, callback);
}

void TraceMarkedObject(Visitor* visitor, const HeapObjectHeader* header) {
  DCHECK(header);
  DCHECK(!header->IsInConstruction<HeapObjectHeader::AccessMode::kNonAtomic>());
  DCHECK(header->IsMarked<HeapObjectHeader::AccessMode::kNonAtomic>());
  const GCInfo& gcinfo =
      GlobalGCInfoTable::GCInfoFromIndex(header->GetGCInfoIndex());
  gcinfo.trace(visitor, header->Payload());
}

size_t GetNextIncrementalStepDuration(IncrementalMarkingSchedule& schedule,
                                      HeapBase& heap) {
  return schedule.GetNextIncrementalStepDuration(
      heap.stats_collector()->allocated_object_size());
}

}  // namespace

constexpr v8::base::TimeDelta MarkerBase::kMaximumIncrementalStepDuration;

MarkerBase::IncrementalMarkingTask::IncrementalMarkingTask(MarkerBase* marker)
    : marker_(marker), handle_(Handle::NonEmptyTag{}) {}

// static
MarkerBase::IncrementalMarkingTask::Handle
MarkerBase::IncrementalMarkingTask::Post(v8::TaskRunner* runner,
                                         MarkerBase* marker) {
  auto task = std::make_unique<IncrementalMarkingTask>(marker);
  auto handle = task->handle_;
  runner->PostNonNestableTask(std::move(task));
  return handle;
}

void MarkerBase::IncrementalMarkingTask::Run() {
  if (handle_.IsCanceled()) return;

  if (marker_->IncrementalMarkingStep(
          MarkingConfig::StackState::kNoHeapPointers)) {
    // Incremental marking is done so should finalize GC.
    marker_->heap().FinalizeIncrementalGarbageCollectionIfNeeded(
        MarkingConfig::StackState::kNoHeapPointers);
  }
}

MarkerBase::MarkerBase(Key, HeapBase& heap, cppgc::Platform* platform,
                       MarkingConfig config)
    : heap_(heap),
      config_(config),
      platform_(platform),
      foreground_task_runner_(platform_->GetForegroundTaskRunner()),
      mutator_marking_state_(heap, marking_worklists_) {}

MarkerBase::~MarkerBase() {
  // The fixed point iteration may have found not-fully-constructed objects.
  // Such objects should have already been found through the stack scan though
  // and should thus already be marked.
  if (!marking_worklists_.not_fully_constructed_worklist()->IsEmpty()) {
#if DEBUG
    DCHECK_NE(MarkingConfig::StackState::kNoHeapPointers, config_.stack_state);
    HeapObjectHeader* header;
    MarkingWorklists::NotFullyConstructedWorklist::Local& local =
        mutator_marking_state_.not_fully_constructed_worklist();
    while (local.Pop(&header)) {
      DCHECK(header->IsMarked());
    }
#else
    marking_worklists_.not_fully_constructed_worklist()->Clear();
#endif
  }
}

void MarkerBase::StartMarking() {
  heap().stats_collector()->NotifyMarkingStarted();

  is_marking_started_ = true;
  if (EnterIncrementalMarkingIfNeeded(config_, heap())) {
    // Performing incremental or concurrent marking.
    schedule_.NotifyIncrementalMarkingStart();
    // Scanning the stack is expensive so we only do it at the atomic pause.
    VisitRoots(MarkingConfig::StackState::kNoHeapPointers);
    ScheduleIncrementalMarkingTask();
  }
}

void MarkerBase::EnterAtomicPause(MarkingConfig::StackState stack_state) {
  if (ExitIncrementalMarkingIfNeeded(config_, heap())) {
    // Cancel remaining incremental tasks.
    if (incremental_marking_handle_) incremental_marking_handle_.Cancel();
  }
  config_.stack_state = stack_state;
  config_.marking_type = MarkingConfig::MarkingType::kAtomic;

  // VisitRoots also resets the LABs.
  VisitRoots(config_.stack_state);
  if (config_.stack_state == MarkingConfig::StackState::kNoHeapPointers) {
    mutator_marking_state_.FlushNotFullyConstructedObjects();
  } else {
    MarkNotFullyConstructedObjects();
  }
}

void MarkerBase::LeaveAtomicPause() {
  DCHECK(!incremental_marking_handle_);
  ResetRememberedSet(heap());
  heap().stats_collector()->NotifyMarkingCompleted(
      // GetOverallMarkedBytes also includes concurrently marked bytes.
      schedule_.GetOverallMarkedBytes());
}

void MarkerBase::FinishMarking(MarkingConfig::StackState stack_state) {
  DCHECK(is_marking_started_);
  EnterAtomicPause(stack_state);
  ProcessWorklistsWithDeadline(std::numeric_limits<size_t>::max(),
                               v8::base::TimeDelta::Max());
  mutator_marking_state_.Publish();
  LeaveAtomicPause();
  is_marking_started_ = false;
}

void MarkerBase::ProcessWeakness() {
  heap().GetWeakPersistentRegion().Trace(&visitor());

  // Call weak callbacks on objects that may now be pointing to dead objects.
  MarkingWorklists::WeakCallbackItem item;
  LivenessBroker broker = LivenessBrokerFactory::Create();
  MarkingWorklists::WeakCallbackWorklist::Local& local =
      mutator_marking_state_.weak_callback_worklist();
  while (local.Pop(&item)) {
    item.callback(broker, item.parameter);
  }
  // Weak callbacks should not add any new objects for marking.
  DCHECK(marking_worklists_.marking_worklist()->IsEmpty());
}

void MarkerBase::VisitRoots(MarkingConfig::StackState stack_state) {
  // Reset LABs before scanning roots. LABs are cleared to allow
  // ObjectStartBitmap handling without considering LABs.
  heap().object_allocator().ResetLinearAllocationBuffers();

  heap().GetStrongPersistentRegion().Trace(&visitor());
  if (stack_state != MarkingConfig::StackState::kNoHeapPointers) {
    heap().stack()->IteratePointers(&stack_visitor());
  }
  if (config_.collection_type == MarkingConfig::CollectionType::kMinor) {
    VisitRememberedSlots(heap(), mutator_marking_state_);
  }
}

void MarkerBase::ScheduleIncrementalMarkingTask() {
  if (!platform_ || !foreground_task_runner_ || incremental_marking_handle_)
    return;
  incremental_marking_handle_ =
      IncrementalMarkingTask::Post(foreground_task_runner_.get(), this);
}

bool MarkerBase::IncrementalMarkingStepForTesting(
    MarkingConfig::StackState stack_state) {
  return IncrementalMarkingStep(stack_state);
}

bool MarkerBase::IncrementalMarkingStep(MarkingConfig::StackState stack_state) {
  if (stack_state == MarkingConfig::StackState::kNoHeapPointers) {
    mutator_marking_state_.FlushNotFullyConstructedObjects();
  }
  config_.stack_state = stack_state;

  return AdvanceMarkingWithDeadline();
}

bool MarkerBase::AdvanceMarkingOnAllocation() {
  bool is_done = AdvanceMarkingWithDeadline();
  if (is_done) {
    // Schedule another incremental task for finalizing without a stack.
    ScheduleIncrementalMarkingTask();
  }
  return is_done;
}

bool MarkerBase::AdvanceMarkingWithMaxDuration(
    v8::base::TimeDelta max_duration) {
  return AdvanceMarkingWithDeadline(max_duration);
}

bool MarkerBase::AdvanceMarkingWithDeadline(v8::base::TimeDelta max_duration) {
  size_t step_size_in_bytes = GetNextIncrementalStepDuration(schedule_, heap_);
  bool is_done = ProcessWorklistsWithDeadline(step_size_in_bytes, max_duration);
  schedule_.UpdateIncrementalMarkedBytes(mutator_marking_state_.marked_bytes());
  if (!is_done) {
    // If marking is atomic, |is_done| should always be true.
    DCHECK_NE(MarkingConfig::MarkingType::kAtomic, config_.marking_type);
    ScheduleIncrementalMarkingTask();
  }
  mutator_marking_state_.Publish();
  return is_done;
}

bool MarkerBase::ProcessWorklistsWithDeadline(
    size_t expected_marked_bytes, v8::base::TimeDelta max_duration) {
  size_t marked_bytes_deadline =
      mutator_marking_state_.marked_bytes() + expected_marked_bytes;
  v8::base::TimeTicks time_deadline = v8::base::TimeTicks::Now() + max_duration;
  do {
    // Convert |previously_not_fully_constructed_worklist_| to
    // |marking_worklist_|. This merely re-adds items with the proper
    // callbacks.
    if (!DrainWorklistWithBytesAndTimeDeadline(
            mutator_marking_state_, marked_bytes_deadline, time_deadline,
            mutator_marking_state_.previously_not_fully_constructed_worklist(),
            [this](HeapObjectHeader* header) {
              TraceMarkedObject(&visitor(), header);
              mutator_marking_state_.AccountMarkedBytes(*header);
            })) {
      return false;
    }

    if (!DrainWorklistWithBytesAndTimeDeadline(
            mutator_marking_state_, marked_bytes_deadline, time_deadline,
            mutator_marking_state_.marking_worklist(),
            [this](const MarkingWorklists::MarkingItem& item) {
              const HeapObjectHeader& header =
                  HeapObjectHeader::FromPayload(item.base_object_payload);
              DCHECK(!header.IsInConstruction<
                      HeapObjectHeader::AccessMode::kNonAtomic>());
              DCHECK(
                  header.IsMarked<HeapObjectHeader::AccessMode::kNonAtomic>());
              item.callback(&visitor(), item.base_object_payload);
              mutator_marking_state_.AccountMarkedBytes(header);
            })) {
      return false;
    }

    if (!DrainWorklistWithBytesAndTimeDeadline(
            mutator_marking_state_, marked_bytes_deadline, time_deadline,
            mutator_marking_state_.write_barrier_worklist(),
            [this](HeapObjectHeader* header) {
              TraceMarkedObject(&visitor(), header);
              mutator_marking_state_.AccountMarkedBytes(*header);
            })) {
      return false;
    }
  } while (!mutator_marking_state_.marking_worklist().IsLocalAndGlobalEmpty());
  return true;
}

void MarkerBase::MarkNotFullyConstructedObjects() {
  HeapObjectHeader* header;
  MarkingWorklists::NotFullyConstructedWorklist::Local& local =
      mutator_marking_state_.not_fully_constructed_worklist();
  while (local.Pop(&header)) {
    DCHECK(header);
    DCHECK(header->IsMarked<HeapObjectHeader::AccessMode::kNonAtomic>());
    // TraceConservativelyIfNeeded will either push to a worklist
    // or trace conservatively and call AccountMarkedBytes.
    conservative_visitor().TraceConservativelyIfNeeded(*header);
  }
}

void MarkerBase::ClearAllWorklistsForTesting() {
  marking_worklists_.ClearForTesting();
}

Marker::Marker(Key key, HeapBase& heap, cppgc::Platform* platform,
               MarkingConfig config)
    : MarkerBase(key, heap, platform, config),
      marking_visitor_(heap, mutator_marking_state_),
      conservative_marking_visitor_(heap, mutator_marking_state_,
                                    marking_visitor_) {}

}  // namespace internal
}  // namespace cppgc
