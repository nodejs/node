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
#include "src/heap/cppgc/process-heap.h"
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
void VisitRememberedSlots(HeapBase& heap,
                          MutatorMarkingState& mutator_marking_state) {
#if defined(CPPGC_YOUNG_GENERATION)
  StatsCollector::EnabledScope stats_scope(
      heap, StatsCollector::kMarkVisitRememberedSets);
  for (void* slot : heap.remembered_slots()) {
    auto& slot_header = BasePage::FromInnerAddress(&heap, slot)
                            ->ObjectHeaderFromInnerAddress(slot);
    if (slot_header.IsYoung()) continue;
    // The design of young generation requires collections to be executed at the
    // top level (with the guarantee that no objects are currently being in
    // construction). This can be ensured by running young GCs from safe points
    // or by reintroducing nested allocation scopes that avoid finalization.
    DCHECK(!slot_header.template IsInConstruction<AccessMode::kNonAtomic>());

    void* value = *reinterpret_cast<void**>(slot);
    mutator_marking_state.DynamicallyMarkAddress(static_cast<Address>(value));
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
          typename WorklistLocal, typename Callback>
bool DrainWorklistWithBytesAndTimeDeadline(MarkingStateBase& marking_state,
                                           size_t marked_bytes_deadline,
                                           v8::base::TimeTicks time_deadline,
                                           WorklistLocal& worklist_local,
                                           Callback callback) {
  return DrainWorklistWithPredicate<kDeadlineCheckInterval>(
      [&marking_state, marked_bytes_deadline, time_deadline]() {
        return (marked_bytes_deadline <= marking_state.marked_bytes()) ||
               (time_deadline <= v8::base::TimeTicks::Now());
      },
      worklist_local, callback);
}

size_t GetNextIncrementalStepDuration(IncrementalMarkingSchedule& schedule,
                                      HeapBase& heap) {
  return schedule.GetNextIncrementalStepDuration(
      heap.stats_collector()->allocated_object_size());
}

}  // namespace

constexpr v8::base::TimeDelta MarkerBase::kMaximumIncrementalStepDuration;

MarkerBase::IncrementalMarkingTask::IncrementalMarkingTask(
    MarkerBase* marker, MarkingConfig::StackState stack_state)
    : marker_(marker),
      stack_state_(stack_state),
      handle_(Handle::NonEmptyTag{}) {}

// static
MarkerBase::IncrementalMarkingTask::Handle
MarkerBase::IncrementalMarkingTask::Post(cppgc::TaskRunner* runner,
                                         MarkerBase* marker) {
  // Incremental GC is possible only via the GCInvoker, so getting here
  // guarantees that either non-nestable tasks or conservative stack
  // scanning are supported. This is required so that the incremental
  // task can safely finalize GC if needed.
  DCHECK_IMPLIES(marker->heap().stack_support() !=
                     HeapBase::StackSupport::kSupportsConservativeStackScan,
                 runner->NonNestableTasksEnabled());
  MarkingConfig::StackState stack_state_for_task =
      runner->NonNestableTasksEnabled()
          ? MarkingConfig::StackState::kNoHeapPointers
          : MarkingConfig::StackState::kMayContainHeapPointers;
  auto task =
      std::make_unique<IncrementalMarkingTask>(marker, stack_state_for_task);
  auto handle = task->handle_;
  if (runner->NonNestableTasksEnabled()) {
    runner->PostNonNestableTask(std::move(task));
  } else {
    runner->PostTask(std::move(task));
  }
  return handle;
}

void MarkerBase::IncrementalMarkingTask::Run() {
  if (handle_.IsCanceled()) return;

  StatsCollector::EnabledScope stats_scope(marker_->heap(),
                                           StatsCollector::kIncrementalMark);

  if (marker_->IncrementalMarkingStep(stack_state_)) {
    // Incremental marking is done so should finalize GC.
    marker_->heap().FinalizeIncrementalGarbageCollectionIfNeeded(stack_state_);
  }
}

MarkerBase::MarkerBase(Key, HeapBase& heap, cppgc::Platform* platform,
                       MarkingConfig config)
    : heap_(heap),
      config_(config),
      platform_(platform),
      foreground_task_runner_(platform_->GetForegroundTaskRunner()),
      mutator_marking_state_(heap, marking_worklists_,
                             heap.compactor().compaction_worklists()) {}

MarkerBase::~MarkerBase() {
  // The fixed point iteration may have found not-fully-constructed objects.
  // Such objects should have already been found through the stack scan though
  // and should thus already be marked.
  if (!marking_worklists_.not_fully_constructed_worklist()->IsEmpty()) {
#if DEBUG
    DCHECK_NE(MarkingConfig::StackState::kNoHeapPointers, config_.stack_state);
    std::unordered_set<HeapObjectHeader*> objects =
        mutator_marking_state_.not_fully_constructed_worklist().Extract();
    for (HeapObjectHeader* object : objects) DCHECK(object->IsMarked());
#else
    marking_worklists_.not_fully_constructed_worklist()->Clear();
#endif
  }

  // |discovered_ephemeron_pairs_worklist_| may still hold ephemeron pairs with
  // dead keys.
  if (!marking_worklists_.discovered_ephemeron_pairs_worklist()->IsEmpty()) {
#if DEBUG
    MarkingWorklists::EphemeronPairItem item;
    while (mutator_marking_state_.discovered_ephemeron_pairs_worklist().Pop(
        &item)) {
      DCHECK(!HeapObjectHeader::FromPayload(item.key).IsMarked());
    }
#else
    marking_worklists_.discovered_ephemeron_pairs_worklist()->Clear();
#endif
  }

  marking_worklists_.weak_containers_worklist()->Clear();
}

void MarkerBase::StartMarking() {
  DCHECK(!is_marking_started_);
  StatsCollector::EnabledScope stats_scope(
      heap(), config_.marking_type == MarkingConfig::MarkingType::kAtomic
                  ? StatsCollector::kAtomicMark
                  : StatsCollector::kIncrementalMark);

  heap().stats_collector()->NotifyMarkingStarted(config_.collection_type,
                                                 config_.is_forced_gc);

  is_marking_started_ = true;
  if (EnterIncrementalMarkingIfNeeded(config_, heap())) {
    StatsCollector::EnabledScope stats_scope(
        heap(), StatsCollector::kMarkIncrementalStart);

    // Performing incremental or concurrent marking.
    schedule_.NotifyIncrementalMarkingStart();
    // Scanning the stack is expensive so we only do it at the atomic pause.
    VisitRoots(MarkingConfig::StackState::kNoHeapPointers);
    ScheduleIncrementalMarkingTask();
    if (config_.marking_type ==
        MarkingConfig::MarkingType::kIncrementalAndConcurrent) {
      mutator_marking_state_.Publish();
      concurrent_marker_->Start();
    }
  }
}

void MarkerBase::EnterAtomicPause(MarkingConfig::StackState stack_state) {
  StatsCollector::EnabledScope stats_scope(heap(),
                                           StatsCollector::kMarkAtomicPrologue);

  if (ExitIncrementalMarkingIfNeeded(config_, heap())) {
    // Cancel remaining concurrent/incremental tasks.
    concurrent_marker_->Cancel();
    incremental_marking_handle_.Cancel();
  }
  config_.stack_state = stack_state;
  config_.marking_type = MarkingConfig::MarkingType::kAtomic;

  // Lock guards against changes to {Weak}CrossThreadPersistent handles, that
  // may conflict with marking. E.g., a WeakCrossThreadPersistent may be
  // converted into a CrossThreadPersistent which requires that the handle
  // is either cleared or the object is retained.
  g_process_mutex.Pointer()->Lock();

  {
    // VisitRoots also resets the LABs.
    VisitRoots(config_.stack_state);
    if (config_.stack_state == MarkingConfig::StackState::kNoHeapPointers) {
      mutator_marking_state_.FlushNotFullyConstructedObjects();
      DCHECK(marking_worklists_.not_fully_constructed_worklist()->IsEmpty());
    } else {
      MarkNotFullyConstructedObjects();
    }
  }
}

void MarkerBase::LeaveAtomicPause() {
  StatsCollector::EnabledScope stats_scope(heap(),
                                           StatsCollector::kMarkAtomicEpilogue);
  DCHECK(!incremental_marking_handle_);
  ResetRememberedSet(heap());
  heap().stats_collector()->NotifyMarkingCompleted(
      // GetOverallMarkedBytes also includes concurrently marked bytes.
      schedule_.GetOverallMarkedBytes());
  is_marking_started_ = false;
  {
    // Weakness callbacks are forbidden from allocating objects.
    ObjectAllocator::NoAllocationScope no_allocation_scope_(
        heap_.object_allocator());
    ProcessWeakness();
  }
  g_process_mutex.Pointer()->Unlock();
}

void MarkerBase::FinishMarking(MarkingConfig::StackState stack_state) {
  DCHECK(is_marking_started_);
  StatsCollector::EnabledScope stats_scope(heap(), StatsCollector::kAtomicMark);
  EnterAtomicPause(stack_state);
  CHECK(ProcessWorklistsWithDeadline(std::numeric_limits<size_t>::max(),
                                     v8::base::TimeTicks::Max()));
  mutator_marking_state_.Publish();
  LeaveAtomicPause();
}

void MarkerBase::ProcessWeakness() {
  DCHECK_EQ(MarkingConfig::MarkingType::kAtomic, config_.marking_type);

  StatsCollector::DisabledScope stats_scope(
      heap(), StatsCollector::kWeakInvokeCallbacks);

  heap().GetWeakPersistentRegion().Trace(&visitor());
  // Processing cross-thread handles requires taking the process lock.
  g_process_mutex.Get().AssertHeld();
  heap().GetWeakCrossThreadPersistentRegion().Trace(&visitor());

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
  StatsCollector::EnabledScope stats_scope(heap(),
                                           StatsCollector::kMarkVisitRoots);

  // Reset LABs before scanning roots. LABs are cleared to allow
  // ObjectStartBitmap handling without considering LABs.
  heap().object_allocator().ResetLinearAllocationBuffers();

  {
    {
      StatsCollector::DisabledScope inner_stats_scope(
          heap(), StatsCollector::kMarkVisitPersistents);
      heap().GetStrongPersistentRegion().Trace(&visitor());
    }
    if (config_.marking_type == MarkingConfig::MarkingType::kAtomic) {
      StatsCollector::DisabledScope inner_stats_scope(
          heap(), StatsCollector::kMarkVisitCrossThreadPersistents);
      g_process_mutex.Get().AssertHeld();
      heap().GetStrongCrossThreadPersistentRegion().Trace(&visitor());
    }
  }

  if (stack_state != MarkingConfig::StackState::kNoHeapPointers) {
    StatsCollector::DisabledScope stack_stats_scope(
        heap(), StatsCollector::kMarkVisitStack);
    heap().stack()->IteratePointers(&stack_visitor());
  }
  if (config_.collection_type == MarkingConfig::CollectionType::kMinor) {
    VisitRememberedSlots(heap(), mutator_marking_state_);
  }
}

void MarkerBase::ScheduleIncrementalMarkingTask() {
  DCHECK(platform_);
  if (!foreground_task_runner_ || incremental_marking_handle_) return;
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

void MarkerBase::AdvanceMarkingOnAllocation() {
  if (AdvanceMarkingWithDeadline()) {
    // Schedule another incremental task for finalizing without a stack.
    ScheduleIncrementalMarkingTask();
  }
}

bool MarkerBase::AdvanceMarkingWithMaxDuration(
    v8::base::TimeDelta max_duration) {
  return AdvanceMarkingWithDeadline(max_duration);
}

bool MarkerBase::AdvanceMarkingWithDeadline(v8::base::TimeDelta max_duration) {
  bool is_done = false;
  if (!incremental_marking_disabled_for_testing_) {
    size_t step_size_in_bytes =
        GetNextIncrementalStepDuration(schedule_, heap_);
    StatsCollector::EnabledScope deadline_scope(
        heap(), StatsCollector::kMarkTransitiveClosureWithDeadline,
        "deadline_ms", max_duration.InMillisecondsF());
    is_done = ProcessWorklistsWithDeadline(
        mutator_marking_state_.marked_bytes() + step_size_in_bytes,
        v8::base::TimeTicks::Now() + max_duration);
    schedule_.UpdateIncrementalMarkedBytes(
        mutator_marking_state_.marked_bytes());
  }
  mutator_marking_state_.Publish();
  if (!is_done) {
    // If marking is atomic, |is_done| should always be true.
    DCHECK_NE(MarkingConfig::MarkingType::kAtomic, config_.marking_type);
    ScheduleIncrementalMarkingTask();
    if (config_.marking_type ==
        MarkingConfig::MarkingType::kIncrementalAndConcurrent) {
      concurrent_marker_->NotifyIncrementalMutatorStepCompleted();
    }
  }
  return is_done;
}

bool MarkerBase::ProcessWorklistsWithDeadline(
    size_t marked_bytes_deadline, v8::base::TimeTicks time_deadline) {
  StatsCollector::EnabledScope stats_scope(
      heap(), StatsCollector::kMarkTransitiveClosure);
  do {
    if ((config_.marking_type == MarkingConfig::MarkingType::kAtomic) ||
        schedule_.ShouldFlushEphemeronPairs()) {
      mutator_marking_state_.FlushDiscoveredEphemeronPairs();
    }

    // Bailout objects may be complicated to trace and thus might take longer
    // than other objects. Therefore we reduce the interval between deadline
    // checks to guarantee the deadline is not exceeded.
    {
      StatsCollector::EnabledScope inner_scope(
          heap(), StatsCollector::kMarkProcessBailOutObjects);
      if (!DrainWorklistWithBytesAndTimeDeadline<kDefaultDeadlineCheckInterval /
                                                 5>(
              mutator_marking_state_, marked_bytes_deadline, time_deadline,
              mutator_marking_state_.concurrent_marking_bailout_worklist(),
              [this](
                  const MarkingWorklists::ConcurrentMarkingBailoutItem& item) {
                mutator_marking_state_.AccountMarkedBytes(item.bailedout_size);
                item.callback(&visitor(), item.parameter);
              })) {
        return false;
      }
    }

    {
      StatsCollector::EnabledScope inner_scope(
          heap(), StatsCollector::kMarkProcessNotFullyconstructedWorklist);
      if (!DrainWorklistWithBytesAndTimeDeadline(
              mutator_marking_state_, marked_bytes_deadline, time_deadline,
              mutator_marking_state_
                  .previously_not_fully_constructed_worklist(),
              [this](HeapObjectHeader* header) {
                mutator_marking_state_.AccountMarkedBytes(*header);
                DynamicallyTraceMarkedObject<AccessMode::kNonAtomic>(visitor(),
                                                                     *header);
              })) {
        return false;
      }
    }

    {
      StatsCollector::EnabledScope inner_scope(
          heap(), StatsCollector::kMarkProcessMarkingWorklist);
      if (!DrainWorklistWithBytesAndTimeDeadline(
              mutator_marking_state_, marked_bytes_deadline, time_deadline,
              mutator_marking_state_.marking_worklist(),
              [this](const MarkingWorklists::MarkingItem& item) {
                const HeapObjectHeader& header =
                    HeapObjectHeader::FromPayload(item.base_object_payload);
                DCHECK(!header.IsInConstruction<AccessMode::kNonAtomic>());
                DCHECK(header.IsMarked<AccessMode::kNonAtomic>());
                mutator_marking_state_.AccountMarkedBytes(header);
                item.callback(&visitor(), item.base_object_payload);
              })) {
        return false;
      }
    }

    {
      StatsCollector::EnabledScope inner_scope(
          heap(), StatsCollector::kMarkProcessWriteBarrierWorklist);
      if (!DrainWorklistWithBytesAndTimeDeadline(
              mutator_marking_state_, marked_bytes_deadline, time_deadline,
              mutator_marking_state_.write_barrier_worklist(),
              [this](HeapObjectHeader* header) {
                mutator_marking_state_.AccountMarkedBytes(*header);
                DynamicallyTraceMarkedObject<AccessMode::kNonAtomic>(visitor(),
                                                                     *header);
              })) {
        return false;
      }
    }

    {
      StatsCollector::EnabledScope stats_scope(
          heap(), StatsCollector::kMarkProcessEphemerons);
      if (!DrainWorklistWithBytesAndTimeDeadline(
              mutator_marking_state_, marked_bytes_deadline, time_deadline,
              mutator_marking_state_.ephemeron_pairs_for_processing_worklist(),
              [this](const MarkingWorklists::EphemeronPairItem& item) {
                mutator_marking_state_.ProcessEphemeron(item.key,
                                                        item.value_desc);
              })) {
        return false;
      }
    }
  } while (!mutator_marking_state_.marking_worklist().IsLocalAndGlobalEmpty());
  return true;
}

void MarkerBase::MarkNotFullyConstructedObjects() {
  StatsCollector::DisabledScope stats_scope(
      heap(), StatsCollector::kMarkVisitNotFullyConstructedObjects);
  std::unordered_set<HeapObjectHeader*> objects =
      mutator_marking_state_.not_fully_constructed_worklist().Extract();
  for (HeapObjectHeader* object : objects) {
    DCHECK(object);
    if (!mutator_marking_state_.MarkNoPush(*object)) continue;
    // TraceConservativelyIfNeeded will either push to a worklist
    // or trace conservatively and call AccountMarkedBytes.
    conservative_visitor().TraceConservativelyIfNeeded(*object);
  }
}

void MarkerBase::ClearAllWorklistsForTesting() {
  marking_worklists_.ClearForTesting();
  auto* compaction_worklists = heap_.compactor().compaction_worklists();
  if (compaction_worklists) compaction_worklists->ClearForTesting();
}

void MarkerBase::DisableIncrementalMarkingForTesting() {
  incremental_marking_disabled_for_testing_ = true;
}

void MarkerBase::WaitForConcurrentMarkingForTesting() {
  concurrent_marker_->JoinForTesting();
}

void MarkerBase::NotifyCompactionCancelled() {
  // Compaction cannot be cancelled while concurrent marking is active.
  DCHECK_EQ(MarkingConfig::MarkingType::kAtomic, config_.marking_type);
  DCHECK_IMPLIES(concurrent_marker_, !concurrent_marker_->IsActive());
  mutator_marking_state_.NotifyCompactionCancelled();
}

Marker::Marker(Key key, HeapBase& heap, cppgc::Platform* platform,
               MarkingConfig config)
    : MarkerBase(key, heap, platform, config),
      marking_visitor_(heap, mutator_marking_state_),
      conservative_marking_visitor_(heap, mutator_marking_state_,
                                    marking_visitor_) {
  concurrent_marker_ = std::make_unique<ConcurrentMarker>(
      heap_, marking_worklists_, schedule_, platform_);
}

}  // namespace internal
}  // namespace cppgc
