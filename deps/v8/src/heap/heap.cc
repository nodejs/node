// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/heap.h"

#include <unordered_map>
#include <unordered_set>

#include "src/accessors.h"
#include "src/api-inl.h"
#include "src/assembler-inl.h"
#include "src/ast/context-slot-cache.h"
#include "src/base/bits.h"
#include "src/base/once.h"
#include "src/base/utils/random-number-generator.h"
#include "src/bootstrapper.h"
#include "src/code-stubs.h"
#include "src/compilation-cache.h"
#include "src/conversions.h"
#include "src/debug/debug.h"
#include "src/deoptimizer.h"
#include "src/feedback-vector.h"
#include "src/global-handles.h"
#include "src/heap/array-buffer-collector.h"
#include "src/heap/array-buffer-tracker-inl.h"
#include "src/heap/barrier.h"
#include "src/heap/code-stats.h"
#include "src/heap/concurrent-marking.h"
#include "src/heap/embedder-tracing.h"
#include "src/heap/gc-idle-time-handler.h"
#include "src/heap/gc-tracer.h"
#include "src/heap/heap-controller.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/heap/incremental-marking.h"
#include "src/heap/mark-compact-inl.h"
#include "src/heap/mark-compact.h"
#include "src/heap/memory-reducer.h"
#include "src/heap/object-stats.h"
#include "src/heap/objects-visiting-inl.h"
#include "src/heap/objects-visiting.h"
#include "src/heap/remembered-set.h"
#include "src/heap/scavenge-job.h"
#include "src/heap/scavenger-inl.h"
#include "src/heap/store-buffer.h"
#include "src/heap/stress-marking-observer.h"
#include "src/heap/stress-scavenge-observer.h"
#include "src/heap/sweeper.h"
#include "src/instruction-stream.h"
#include "src/interpreter/interpreter.h"
#include "src/objects/data-handler.h"
#include "src/objects/hash-table-inl.h"
#include "src/objects/maybe-object.h"
#include "src/objects/shared-function-info.h"
#include "src/regexp/jsregexp.h"
#include "src/runtime-profiler.h"
#include "src/snapshot/natives.h"
#include "src/snapshot/serializer-common.h"
#include "src/snapshot/snapshot.h"
#include "src/tracing/trace-event.h"
#include "src/unicode-decoder.h"
#include "src/unicode-inl.h"
#include "src/utils-inl.h"
#include "src/utils.h"
#include "src/v8.h"
#include "src/vm-state-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

void Heap::SetArgumentsAdaptorDeoptPCOffset(int pc_offset) {
  DCHECK_EQ(Smi::kZero, arguments_adaptor_deopt_pc_offset());
  set_arguments_adaptor_deopt_pc_offset(Smi::FromInt(pc_offset));
}

void Heap::SetConstructStubCreateDeoptPCOffset(int pc_offset) {
  DCHECK(construct_stub_create_deopt_pc_offset() == Smi::kZero);
  set_construct_stub_create_deopt_pc_offset(Smi::FromInt(pc_offset));
}

void Heap::SetConstructStubInvokeDeoptPCOffset(int pc_offset) {
  DCHECK(construct_stub_invoke_deopt_pc_offset() == Smi::kZero);
  set_construct_stub_invoke_deopt_pc_offset(Smi::FromInt(pc_offset));
}

void Heap::SetInterpreterEntryReturnPCOffset(int pc_offset) {
  DCHECK_EQ(Smi::kZero, interpreter_entry_return_pc_offset());
  set_interpreter_entry_return_pc_offset(Smi::FromInt(pc_offset));
}

void Heap::SetSerializedObjects(FixedArray* objects) {
  DCHECK(isolate()->serializer_enabled());
  set_serialized_objects(objects);
}

void Heap::SetSerializedGlobalProxySizes(FixedArray* sizes) {
  DCHECK(isolate()->serializer_enabled());
  set_serialized_global_proxy_sizes(sizes);
}

bool Heap::GCCallbackTuple::operator==(
    const Heap::GCCallbackTuple& other) const {
  return other.callback == callback && other.data == data;
}

Heap::GCCallbackTuple& Heap::GCCallbackTuple::operator=(
    const Heap::GCCallbackTuple& other) = default;

struct Heap::StrongRootsList {
  Object** start;
  Object** end;
  StrongRootsList* next;
};

class IdleScavengeObserver : public AllocationObserver {
 public:
  IdleScavengeObserver(Heap& heap, intptr_t step_size)
      : AllocationObserver(step_size), heap_(heap) {}

  void Step(int bytes_allocated, Address, size_t) override {
    heap_.ScheduleIdleScavengeIfNeeded(bytes_allocated);
  }

 private:
  Heap& heap_;
};

Heap::Heap()
    : initial_max_old_generation_size_(max_old_generation_size_),
      initial_old_generation_size_(max_old_generation_size_ /
                                   kInitalOldGenerationLimitFactor),
      memory_pressure_level_(MemoryPressureLevel::kNone),
      old_generation_allocation_limit_(initial_old_generation_size_),
      global_pretenuring_feedback_(kInitialFeedbackCapacity),
      current_gc_callback_flags_(GCCallbackFlags::kNoGCCallbackFlags),
      external_string_table_(this) {
  // Ensure old_generation_size_ is a multiple of kPageSize.
  DCHECK_EQ(0, max_old_generation_size_ & (Page::kPageSize - 1));

  set_native_contexts_list(nullptr);
  set_allocation_sites_list(Smi::kZero);
  // Put a dummy entry in the remembered pages so we can find the list the
  // minidump even if there are no real unmapped pages.
  RememberUnmappedPage(kNullAddress, false);
}

size_t Heap::MaxReserved() {
  const double kFactor = Page::kPageSize * 1.0 / Page::kAllocatableMemory;
  return static_cast<size_t>(
      (2 * max_semi_space_size_ + max_old_generation_size_) * kFactor);
}

size_t Heap::ComputeMaxOldGenerationSize(uint64_t physical_memory) {
  const size_t old_space_physical_memory_factor = 4;
  size_t computed_size = static_cast<size_t>(physical_memory / i::MB /
                                             old_space_physical_memory_factor *
                                             kPointerMultiplier);
  return Max(Min(computed_size, HeapController::kMaxSize),
             HeapController::kMinSize);
}

size_t Heap::Capacity() {
  if (!HasBeenSetUp()) return 0;

  return new_space_->Capacity() + OldGenerationCapacity();
}

size_t Heap::OldGenerationCapacity() {
  if (!HasBeenSetUp()) return 0;
  PagedSpaces spaces(this, PagedSpaces::SpacesSpecifier::kAllPagedSpaces);
  size_t total = 0;
  for (PagedSpace* space = spaces.next(); space != nullptr;
       space = spaces.next()) {
    total += space->Capacity();
  }
  return total + lo_space_->SizeOfObjects();
}

size_t Heap::CommittedOldGenerationMemory() {
  if (!HasBeenSetUp()) return 0;

  PagedSpaces spaces(this, PagedSpaces::SpacesSpecifier::kAllPagedSpaces);
  size_t total = 0;
  for (PagedSpace* space = spaces.next(); space != nullptr;
       space = spaces.next()) {
    total += space->CommittedMemory();
  }
  return total + lo_space_->Size();
}

size_t Heap::CommittedMemoryOfHeapAndUnmapper() {
  if (!HasBeenSetUp()) return 0;

  return CommittedMemory() +
         memory_allocator()->unmapper()->CommittedBufferedMemory();
}

size_t Heap::CommittedMemory() {
  if (!HasBeenSetUp()) return 0;

  return new_space_->CommittedMemory() + CommittedOldGenerationMemory();
}


size_t Heap::CommittedPhysicalMemory() {
  if (!HasBeenSetUp()) return 0;

  size_t total = 0;
  for (SpaceIterator it(this); it.has_next();) {
    total += it.next()->CommittedPhysicalMemory();
  }

  return total;
}

size_t Heap::CommittedMemoryExecutable() {
  if (!HasBeenSetUp()) return 0;

  return static_cast<size_t>(memory_allocator()->SizeExecutable());
}


void Heap::UpdateMaximumCommitted() {
  if (!HasBeenSetUp()) return;

  const size_t current_committed_memory = CommittedMemory();
  if (current_committed_memory > maximum_committed_) {
    maximum_committed_ = current_committed_memory;
  }
}

size_t Heap::Available() {
  if (!HasBeenSetUp()) return 0;

  size_t total = 0;

  for (SpaceIterator it(this); it.has_next();) {
    total += it.next()->Available();
  }
  return total;
}

bool Heap::CanExpandOldGeneration(size_t size) {
  if (force_oom_) return false;
  if (OldGenerationCapacity() + size > MaxOldGenerationSize()) return false;
  // The OldGenerationCapacity does not account compaction spaces used
  // during evacuation. Ensure that expanding the old generation does push
  // the total allocated memory size over the maximum heap size.
  return memory_allocator()->Size() + size <= MaxReserved();
}

bool Heap::HasBeenSetUp() {
  // We will always have a new space when the heap is set up.
  return new_space_ != nullptr;
}


GarbageCollector Heap::SelectGarbageCollector(AllocationSpace space,
                                              const char** reason) {
  // Is global GC requested?
  if (space != NEW_SPACE) {
    isolate_->counters()->gc_compactor_caused_by_request()->Increment();
    *reason = "GC in old space requested";
    return MARK_COMPACTOR;
  }

  if (FLAG_gc_global || (FLAG_stress_compaction && (gc_count_ & 1) != 0)) {
    *reason = "GC in old space forced by flags";
    return MARK_COMPACTOR;
  }

  if (incremental_marking()->NeedsFinalization() &&
      AllocationLimitOvershotByLargeMargin()) {
    *reason = "Incremental marking needs finalization";
    return MARK_COMPACTOR;
  }

  // Over-estimate the new space size using capacity to allow some slack.
  if (!CanExpandOldGeneration(new_space_->TotalCapacity())) {
    isolate_->counters()
        ->gc_compactor_caused_by_oldspace_exhaustion()
        ->Increment();
    *reason = "scavenge might not succeed";
    return MARK_COMPACTOR;
  }

  // Default
  *reason = nullptr;
  return YoungGenerationCollector();
}

void Heap::SetGCState(HeapState state) {
  gc_state_ = state;
}

void Heap::PrintShortHeapStatistics() {
  if (!FLAG_trace_gc_verbose) return;
  PrintIsolate(isolate_,
               "Memory allocator,       used: %6" PRIuS
               " KB,"
               " available: %6" PRIuS " KB\n",
               memory_allocator()->Size() / KB,
               memory_allocator()->Available() / KB);
  PrintIsolate(isolate_,
               "Read-only space,        used: %6" PRIuS
               " KB"
               ", available: %6" PRIuS
               " KB"
               ", committed: %6" PRIuS " KB\n",
               read_only_space_->Size() / KB,
               read_only_space_->Available() / KB,
               read_only_space_->CommittedMemory() / KB);
  PrintIsolate(isolate_,
               "New space,              used: %6" PRIuS
               " KB"
               ", available: %6" PRIuS
               " KB"
               ", committed: %6" PRIuS " KB\n",
               new_space_->Size() / KB, new_space_->Available() / KB,
               new_space_->CommittedMemory() / KB);
  PrintIsolate(isolate_,
               "New large object space, used: %6" PRIuS
               " KB"
               ", available: %6" PRIuS
               " KB"
               ", committed: %6" PRIuS " KB\n",
               new_lo_space_->SizeOfObjects() / KB,
               new_lo_space_->Available() / KB,
               new_lo_space_->CommittedMemory() / KB);
  PrintIsolate(isolate_,
               "Old space,              used: %6" PRIuS
               " KB"
               ", available: %6" PRIuS
               " KB"
               ", committed: %6" PRIuS " KB\n",
               old_space_->SizeOfObjects() / KB, old_space_->Available() / KB,
               old_space_->CommittedMemory() / KB);
  PrintIsolate(isolate_,
               "Code space,             used: %6" PRIuS
               " KB"
               ", available: %6" PRIuS
               " KB"
               ", committed: %6" PRIuS "KB\n",
               code_space_->SizeOfObjects() / KB, code_space_->Available() / KB,
               code_space_->CommittedMemory() / KB);
  PrintIsolate(isolate_,
               "Map space,              used: %6" PRIuS
               " KB"
               ", available: %6" PRIuS
               " KB"
               ", committed: %6" PRIuS " KB\n",
               map_space_->SizeOfObjects() / KB, map_space_->Available() / KB,
               map_space_->CommittedMemory() / KB);
  PrintIsolate(isolate_,
               "Large object space,     used: %6" PRIuS
               " KB"
               ", available: %6" PRIuS
               " KB"
               ", committed: %6" PRIuS " KB\n",
               lo_space_->SizeOfObjects() / KB, lo_space_->Available() / KB,
               lo_space_->CommittedMemory() / KB);
  PrintIsolate(isolate_,
               "All spaces,             used: %6" PRIuS
               " KB"
               ", available: %6" PRIuS
               " KB"
               ", committed: %6" PRIuS "KB\n",
               this->SizeOfObjects() / KB, this->Available() / KB,
               this->CommittedMemory() / KB);
  PrintIsolate(isolate_,
               "Unmapper buffering %d chunks of committed: %6" PRIuS " KB\n",
               memory_allocator()->unmapper()->NumberOfChunks(),
               CommittedMemoryOfHeapAndUnmapper() / KB);
  PrintIsolate(isolate_, "External memory reported: %6" PRId64 " KB\n",
               external_memory_ / KB);
  PrintIsolate(isolate_, "Backing store memory: %6" PRIuS " KB\n",
               backing_store_bytes_ / KB);
  PrintIsolate(isolate_, "External memory global %zu KB\n",
               external_memory_callback_() / KB);
  PrintIsolate(isolate_, "Total time spent in GC  : %.1f ms\n",
               total_gc_time_ms_);
}

void Heap::ReportStatisticsAfterGC() {
  for (int i = 0; i < static_cast<int>(v8::Isolate::kUseCounterFeatureCount);
       ++i) {
    int count = deferred_counters_[i];
    deferred_counters_[i] = 0;
    while (count > 0) {
      count--;
      isolate()->CountUsage(static_cast<v8::Isolate::UseCounterFeature>(i));
    }
  }
}

void Heap::AddHeapObjectAllocationTracker(
    HeapObjectAllocationTracker* tracker) {
  if (allocation_trackers_.empty()) DisableInlineAllocation();
  allocation_trackers_.push_back(tracker);
}

void Heap::RemoveHeapObjectAllocationTracker(
    HeapObjectAllocationTracker* tracker) {
  allocation_trackers_.erase(std::remove(allocation_trackers_.begin(),
                                         allocation_trackers_.end(), tracker),
                             allocation_trackers_.end());
  if (allocation_trackers_.empty()) EnableInlineAllocation();
}

void Heap::AddRetainingPathTarget(Handle<HeapObject> object,
                                  RetainingPathOption option) {
  if (!FLAG_track_retaining_path) {
    PrintF("Retaining path tracking requires --track-retaining-path\n");
  } else {
    Handle<WeakArrayList> array(retaining_path_targets(), isolate());
    int index = array->length();
    array = WeakArrayList::AddToEnd(isolate(), array,
                                    MaybeObjectHandle::Weak(object));
    set_retaining_path_targets(*array);
    DCHECK_EQ(array->length(), index + 1);
    retaining_path_target_option_[index] = option;
  }
}

bool Heap::IsRetainingPathTarget(HeapObject* object,
                                 RetainingPathOption* option) {
  WeakArrayList* targets = retaining_path_targets();
  int length = targets->length();
  MaybeObject* object_to_check = HeapObjectReference::Weak(object);
  for (int i = 0; i < length; i++) {
    MaybeObject* target = targets->Get(i);
    DCHECK(target->IsWeakOrCleared());
    if (target == object_to_check) {
      DCHECK(retaining_path_target_option_.count(i));
      *option = retaining_path_target_option_[i];
      return true;
    }
  }
  return false;
}

void Heap::PrintRetainingPath(HeapObject* target, RetainingPathOption option) {
  PrintF("\n\n\n");
  PrintF("#################################################\n");
  PrintF("Retaining path for %p:\n", static_cast<void*>(target));
  HeapObject* object = target;
  std::vector<std::pair<HeapObject*, bool>> retaining_path;
  Root root = Root::kUnknown;
  bool ephemeron = false;
  while (true) {
    retaining_path.push_back(std::make_pair(object, ephemeron));
    if (option == RetainingPathOption::kTrackEphemeronPath &&
        ephemeron_retainer_.count(object)) {
      object = ephemeron_retainer_[object];
      ephemeron = true;
    } else if (retainer_.count(object)) {
      object = retainer_[object];
      ephemeron = false;
    } else {
      if (retaining_root_.count(object)) {
        root = retaining_root_[object];
      }
      break;
    }
  }
  int distance = static_cast<int>(retaining_path.size());
  for (auto node : retaining_path) {
    HeapObject* object = node.first;
    bool ephemeron = node.second;
    PrintF("\n");
    PrintF("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");
    PrintF("Distance from root %d%s: ", distance,
           ephemeron ? " (ephemeron)" : "");
    object->ShortPrint();
    PrintF("\n");
#ifdef OBJECT_PRINT
    object->Print();
    PrintF("\n");
#endif
    --distance;
  }
  PrintF("\n");
  PrintF("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");
  PrintF("Root: %s\n", RootVisitor::RootName(root));
  PrintF("-------------------------------------------------\n");
}

void Heap::AddRetainer(HeapObject* retainer, HeapObject* object) {
  if (retainer_.count(object)) return;
  retainer_[object] = retainer;
  RetainingPathOption option = RetainingPathOption::kDefault;
  if (IsRetainingPathTarget(object, &option)) {
    // Check if the retaining path was already printed in
    // AddEphemeronRetainer().
    if (ephemeron_retainer_.count(object) == 0 ||
        option == RetainingPathOption::kDefault) {
      PrintRetainingPath(object, option);
    }
  }
}

void Heap::AddEphemeronRetainer(HeapObject* retainer, HeapObject* object) {
  if (ephemeron_retainer_.count(object)) return;
  ephemeron_retainer_[object] = retainer;
  RetainingPathOption option = RetainingPathOption::kDefault;
  if (IsRetainingPathTarget(object, &option) &&
      option == RetainingPathOption::kTrackEphemeronPath) {
    // Check if the retaining path was already printed in AddRetainer().
    if (retainer_.count(object) == 0) {
      PrintRetainingPath(object, option);
    }
  }
}

void Heap::AddRetainingRoot(Root root, HeapObject* object) {
  if (retaining_root_.count(object)) return;
  retaining_root_[object] = root;
  RetainingPathOption option = RetainingPathOption::kDefault;
  if (IsRetainingPathTarget(object, &option)) {
    PrintRetainingPath(object, option);
  }
}

void Heap::IncrementDeferredCount(v8::Isolate::UseCounterFeature feature) {
  deferred_counters_[feature]++;
}

bool Heap::UncommitFromSpace() { return new_space_->UncommitFromSpace(); }

void Heap::GarbageCollectionPrologue() {
  TRACE_GC(tracer(), GCTracer::Scope::HEAP_PROLOGUE);
  {
    AllowHeapAllocation for_the_first_part_of_prologue;
    gc_count_++;

#ifdef VERIFY_HEAP
    if (FLAG_verify_heap) {
      Verify();
    }
#endif
  }

  // Reset GC statistics.
  promoted_objects_size_ = 0;
  previous_semi_space_copied_object_size_ = semi_space_copied_object_size_;
  semi_space_copied_object_size_ = 0;
  nodes_died_in_new_space_ = 0;
  nodes_copied_in_new_space_ = 0;
  nodes_promoted_ = 0;

  UpdateMaximumCommitted();

#ifdef DEBUG
  DCHECK(!AllowHeapAllocation::IsAllowed() && gc_state_ == NOT_IN_GC);

  if (FLAG_gc_verbose) Print();
#endif  // DEBUG

  if (new_space_->IsAtMaximumCapacity()) {
    maximum_size_scavenges_++;
  } else {
    maximum_size_scavenges_ = 0;
  }
  CheckNewSpaceExpansionCriteria();
  UpdateNewSpaceAllocationCounter();
  if (FLAG_track_retaining_path) {
    retainer_.clear();
    ephemeron_retainer_.clear();
    retaining_root_.clear();
  }
}

size_t Heap::SizeOfObjects() {
  size_t total = 0;

  for (SpaceIterator it(this); it.has_next();) {
    total += it.next()->SizeOfObjects();
  }
  return total;
}


const char* Heap::GetSpaceName(int idx) {
  switch (idx) {
    case NEW_SPACE:
      return "new_space";
    case OLD_SPACE:
      return "old_space";
    case MAP_SPACE:
      return "map_space";
    case CODE_SPACE:
      return "code_space";
    case LO_SPACE:
      return "large_object_space";
    case NEW_LO_SPACE:
      return "new_large_object_space";
    case RO_SPACE:
      return "read_only_space";
    default:
      UNREACHABLE();
  }
  return nullptr;
}

void Heap::SetRootCodeStubs(SimpleNumberDictionary* value) {
  roots_[RootIndex::kCodeStubs] = value;
}

void Heap::RepairFreeListsAfterDeserialization() {
  PagedSpaces spaces(this);
  for (PagedSpace* space = spaces.next(); space != nullptr;
       space = spaces.next()) {
    space->RepairFreeListsAfterDeserialization();
  }
}

void Heap::MergeAllocationSitePretenuringFeedback(
    const PretenuringFeedbackMap& local_pretenuring_feedback) {
  AllocationSite* site = nullptr;
  for (auto& site_and_count : local_pretenuring_feedback) {
    site = site_and_count.first;
    MapWord map_word = site_and_count.first->map_word();
    if (map_word.IsForwardingAddress()) {
      site = AllocationSite::cast(map_word.ToForwardingAddress());
    }

    // We have not validated the allocation site yet, since we have not
    // dereferenced the site during collecting information.
    // This is an inlined check of AllocationMemento::IsValid.
    if (!site->IsAllocationSite() || site->IsZombie()) continue;

    const int value = static_cast<int>(site_and_count.second);
    DCHECK_LT(0, value);
    if (site->IncrementMementoFoundCount(value)) {
      // For sites in the global map the count is accessed through the site.
      global_pretenuring_feedback_.insert(std::make_pair(site, 0));
    }
  }
}

void Heap::AddAllocationObserversToAllSpaces(
    AllocationObserver* observer, AllocationObserver* new_space_observer) {
  DCHECK(observer && new_space_observer);

  for (SpaceIterator it(this); it.has_next();) {
    Space* space = it.next();
    if (space == new_space()) {
      space->AddAllocationObserver(new_space_observer);
    } else {
      space->AddAllocationObserver(observer);
    }
  }
}

void Heap::RemoveAllocationObserversFromAllSpaces(
    AllocationObserver* observer, AllocationObserver* new_space_observer) {
  DCHECK(observer && new_space_observer);

  for (SpaceIterator it(this); it.has_next();) {
    Space* space = it.next();
    if (space == new_space()) {
      space->RemoveAllocationObserver(new_space_observer);
    } else {
      space->RemoveAllocationObserver(observer);
    }
  }
}

class Heap::SkipStoreBufferScope {
 public:
  explicit SkipStoreBufferScope(StoreBuffer* store_buffer)
      : store_buffer_(store_buffer) {
    store_buffer_->MoveAllEntriesToRememberedSet();
    store_buffer_->SetMode(StoreBuffer::IN_GC);
  }

  ~SkipStoreBufferScope() {
    DCHECK(store_buffer_->Empty());
    store_buffer_->SetMode(StoreBuffer::NOT_IN_GC);
  }

 private:
  StoreBuffer* store_buffer_;
};

namespace {
inline bool MakePretenureDecision(
    AllocationSite* site, AllocationSite::PretenureDecision current_decision,
    double ratio, bool maximum_size_scavenge) {
  // Here we just allow state transitions from undecided or maybe tenure
  // to don't tenure, maybe tenure, or tenure.
  if ((current_decision == AllocationSite::kUndecided ||
       current_decision == AllocationSite::kMaybeTenure)) {
    if (ratio >= AllocationSite::kPretenureRatio) {
      // We just transition into tenure state when the semi-space was at
      // maximum capacity.
      if (maximum_size_scavenge) {
        site->set_deopt_dependent_code(true);
        site->set_pretenure_decision(AllocationSite::kTenure);
        // Currently we just need to deopt when we make a state transition to
        // tenure.
        return true;
      }
      site->set_pretenure_decision(AllocationSite::kMaybeTenure);
    } else {
      site->set_pretenure_decision(AllocationSite::kDontTenure);
    }
  }
  return false;
}

inline bool DigestPretenuringFeedback(Isolate* isolate, AllocationSite* site,
                                      bool maximum_size_scavenge) {
  bool deopt = false;
  int create_count = site->memento_create_count();
  int found_count = site->memento_found_count();
  bool minimum_mementos_created =
      create_count >= AllocationSite::kPretenureMinimumCreated;
  double ratio = minimum_mementos_created || FLAG_trace_pretenuring_statistics
                     ? static_cast<double>(found_count) / create_count
                     : 0.0;
  AllocationSite::PretenureDecision current_decision =
      site->pretenure_decision();

  if (minimum_mementos_created) {
    deopt = MakePretenureDecision(site, current_decision, ratio,
                                  maximum_size_scavenge);
  }

  if (FLAG_trace_pretenuring_statistics) {
    PrintIsolate(isolate,
                 "pretenuring: AllocationSite(%p): (created, found, ratio) "
                 "(%d, %d, %f) %s => %s\n",
                 static_cast<void*>(site), create_count, found_count, ratio,
                 site->PretenureDecisionName(current_decision),
                 site->PretenureDecisionName(site->pretenure_decision()));
  }

  // Clear feedback calculation fields until the next gc.
  site->set_memento_found_count(0);
  site->set_memento_create_count(0);
  return deopt;
}
}  // namespace

void Heap::RemoveAllocationSitePretenuringFeedback(AllocationSite* site) {
  global_pretenuring_feedback_.erase(site);
}

bool Heap::DeoptMaybeTenuredAllocationSites() {
  return new_space_->IsAtMaximumCapacity() && maximum_size_scavenges_ == 0;
}

void Heap::ProcessPretenuringFeedback() {
  bool trigger_deoptimization = false;
  if (FLAG_allocation_site_pretenuring) {
    int tenure_decisions = 0;
    int dont_tenure_decisions = 0;
    int allocation_mementos_found = 0;
    int allocation_sites = 0;
    int active_allocation_sites = 0;

    AllocationSite* site = nullptr;

    // Step 1: Digest feedback for recorded allocation sites.
    bool maximum_size_scavenge = MaximumSizeScavenge();
    for (auto& site_and_count : global_pretenuring_feedback_) {
      allocation_sites++;
      site = site_and_count.first;
      // Count is always access through the site.
      DCHECK_EQ(0, site_and_count.second);
      int found_count = site->memento_found_count();
      // An entry in the storage does not imply that the count is > 0 because
      // allocation sites might have been reset due to too many objects dying
      // in old space.
      if (found_count > 0) {
        DCHECK(site->IsAllocationSite());
        active_allocation_sites++;
        allocation_mementos_found += found_count;
        if (DigestPretenuringFeedback(isolate_, site, maximum_size_scavenge)) {
          trigger_deoptimization = true;
        }
        if (site->GetPretenureMode() == TENURED) {
          tenure_decisions++;
        } else {
          dont_tenure_decisions++;
        }
      }
    }

    // Step 2: Deopt maybe tenured allocation sites if necessary.
    bool deopt_maybe_tenured = DeoptMaybeTenuredAllocationSites();
    if (deopt_maybe_tenured) {
      ForeachAllocationSite(
          allocation_sites_list(),
          [&allocation_sites, &trigger_deoptimization](AllocationSite* site) {
            DCHECK(site->IsAllocationSite());
            allocation_sites++;
            if (site->IsMaybeTenure()) {
              site->set_deopt_dependent_code(true);
              trigger_deoptimization = true;
            }
          });
    }

    if (trigger_deoptimization) {
      isolate_->stack_guard()->RequestDeoptMarkedAllocationSites();
    }

    if (FLAG_trace_pretenuring_statistics &&
        (allocation_mementos_found > 0 || tenure_decisions > 0 ||
         dont_tenure_decisions > 0)) {
      PrintIsolate(isolate(),
                   "pretenuring: deopt_maybe_tenured=%d visited_sites=%d "
                   "active_sites=%d "
                   "mementos=%d tenured=%d not_tenured=%d\n",
                   deopt_maybe_tenured ? 1 : 0, allocation_sites,
                   active_allocation_sites, allocation_mementos_found,
                   tenure_decisions, dont_tenure_decisions);
    }

    global_pretenuring_feedback_.clear();
    global_pretenuring_feedback_.reserve(kInitialFeedbackCapacity);
  }
}

void Heap::InvalidateCodeEmbeddedObjects(Code* code) {
  MemoryChunk* chunk = MemoryChunk::FromAddress(code->address());
  CodePageMemoryModificationScope modification_scope(chunk);
  code->InvalidateEmbeddedObjects(this);
}

void Heap::InvalidateCodeDeoptimizationData(Code* code) {
  MemoryChunk* chunk = MemoryChunk::FromAddress(code->address());
  CodePageMemoryModificationScope modification_scope(chunk);
  code->set_deoptimization_data(ReadOnlyRoots(this).empty_fixed_array());
}

void Heap::DeoptMarkedAllocationSites() {
  // TODO(hpayer): If iterating over the allocation sites list becomes a
  // performance issue, use a cache data structure in heap instead.

  ForeachAllocationSite(allocation_sites_list(), [this](AllocationSite* site) {
    if (site->deopt_dependent_code()) {
      site->dependent_code()->MarkCodeForDeoptimization(
          isolate_, DependentCode::kAllocationSiteTenuringChangedGroup);
      site->set_deopt_dependent_code(false);
    }
  });

  Deoptimizer::DeoptimizeMarkedCode(isolate_);
}


void Heap::GarbageCollectionEpilogue() {
  TRACE_GC(tracer(), GCTracer::Scope::HEAP_EPILOGUE);
  if (Heap::ShouldZapGarbage() || FLAG_clear_free_memory) {
    ZapFromSpace();
  }

#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) {
    Verify();
  }
#endif

  AllowHeapAllocation for_the_rest_of_the_epilogue;

#ifdef DEBUG
  if (FLAG_print_global_handles) isolate_->global_handles()->Print();
  if (FLAG_print_handles) PrintHandles();
  if (FLAG_gc_verbose) Print();
  if (FLAG_code_stats) ReportCodeStatistics("After GC");
  if (FLAG_check_handle_count) CheckHandleCount();
#endif

  UpdateMaximumCommitted();

  isolate_->counters()->alive_after_last_gc()->Set(
      static_cast<int>(SizeOfObjects()));

  isolate_->counters()->string_table_capacity()->Set(
      string_table()->Capacity());
  isolate_->counters()->number_of_symbols()->Set(
      string_table()->NumberOfElements());

  if (CommittedMemory() > 0) {
    isolate_->counters()->external_fragmentation_total()->AddSample(
        static_cast<int>(100 - (SizeOfObjects() * 100.0) / CommittedMemory()));

    isolate_->counters()->heap_sample_total_committed()->AddSample(
        static_cast<int>(CommittedMemory() / KB));
    isolate_->counters()->heap_sample_total_used()->AddSample(
        static_cast<int>(SizeOfObjects() / KB));
    isolate_->counters()->heap_sample_map_space_committed()->AddSample(
        static_cast<int>(map_space()->CommittedMemory() / KB));
    isolate_->counters()->heap_sample_code_space_committed()->AddSample(
        static_cast<int>(code_space()->CommittedMemory() / KB));

    isolate_->counters()->heap_sample_maximum_committed()->AddSample(
        static_cast<int>(MaximumCommittedMemory() / KB));
  }

#define UPDATE_COUNTERS_FOR_SPACE(space)                \
  isolate_->counters()->space##_bytes_available()->Set( \
      static_cast<int>(space()->Available()));          \
  isolate_->counters()->space##_bytes_committed()->Set( \
      static_cast<int>(space()->CommittedMemory()));    \
  isolate_->counters()->space##_bytes_used()->Set(      \
      static_cast<int>(space()->SizeOfObjects()));
#define UPDATE_FRAGMENTATION_FOR_SPACE(space)                          \
  if (space()->CommittedMemory() > 0) {                                \
    isolate_->counters()->external_fragmentation_##space()->AddSample( \
        static_cast<int>(100 -                                         \
                         (space()->SizeOfObjects() * 100.0) /          \
                             space()->CommittedMemory()));             \
  }
#define UPDATE_COUNTERS_AND_FRAGMENTATION_FOR_SPACE(space) \
  UPDATE_COUNTERS_FOR_SPACE(space)                         \
  UPDATE_FRAGMENTATION_FOR_SPACE(space)

  UPDATE_COUNTERS_FOR_SPACE(new_space)
  UPDATE_COUNTERS_AND_FRAGMENTATION_FOR_SPACE(old_space)
  UPDATE_COUNTERS_AND_FRAGMENTATION_FOR_SPACE(code_space)
  UPDATE_COUNTERS_AND_FRAGMENTATION_FOR_SPACE(map_space)
  UPDATE_COUNTERS_AND_FRAGMENTATION_FOR_SPACE(lo_space)
#undef UPDATE_COUNTERS_FOR_SPACE
#undef UPDATE_FRAGMENTATION_FOR_SPACE
#undef UPDATE_COUNTERS_AND_FRAGMENTATION_FOR_SPACE

#ifdef DEBUG
  ReportStatisticsAfterGC();
#endif  // DEBUG

  last_gc_time_ = MonotonicallyIncreasingTimeInMs();

  {
    TRACE_GC(tracer(), GCTracer::Scope::HEAP_EPILOGUE_REDUCE_NEW_SPACE);
    ReduceNewSpaceSize();
  }
}

class GCCallbacksScope {
 public:
  explicit GCCallbacksScope(Heap* heap) : heap_(heap) {
    heap_->gc_callbacks_depth_++;
  }
  ~GCCallbacksScope() { heap_->gc_callbacks_depth_--; }

  bool CheckReenter() { return heap_->gc_callbacks_depth_ == 1; }

 private:
  Heap* heap_;
};


void Heap::HandleGCRequest() {
  if (FLAG_stress_scavenge > 0 && stress_scavenge_observer_->HasRequestedGC()) {
    CollectAllGarbage(NEW_SPACE, GarbageCollectionReason::kTesting);
    stress_scavenge_observer_->RequestedGCDone();
  } else if (HighMemoryPressure()) {
    incremental_marking()->reset_request_type();
    CheckMemoryPressure();
  } else if (incremental_marking()->request_type() ==
             IncrementalMarking::COMPLETE_MARKING) {
    incremental_marking()->reset_request_type();
    CollectAllGarbage(current_gc_flags_,
                      GarbageCollectionReason::kFinalizeMarkingViaStackGuard,
                      current_gc_callback_flags_);
  } else if (incremental_marking()->request_type() ==
                 IncrementalMarking::FINALIZATION &&
             incremental_marking()->IsMarking() &&
             !incremental_marking()->finalize_marking_completed()) {
    incremental_marking()->reset_request_type();
    FinalizeIncrementalMarkingIncrementally(
        GarbageCollectionReason::kFinalizeMarkingViaStackGuard);
  }
}


void Heap::ScheduleIdleScavengeIfNeeded(int bytes_allocated) {
  scavenge_job_->ScheduleIdleTaskIfNeeded(this, bytes_allocated);
}

HistogramTimer* Heap::GCTypePriorityTimer(GarbageCollector collector) {
  if (IsYoungGenerationCollector(collector)) {
    if (isolate_->IsIsolateInBackground()) {
      return isolate_->counters()->gc_scavenger_background();
    }
    return isolate_->counters()->gc_scavenger_foreground();
  } else {
    if (!incremental_marking()->IsStopped()) {
      if (ShouldReduceMemory()) {
        if (isolate_->IsIsolateInBackground()) {
          return isolate_->counters()->gc_finalize_reduce_memory_background();
        }
        return isolate_->counters()->gc_finalize_reduce_memory_foreground();
      } else {
        if (isolate_->IsIsolateInBackground()) {
          return isolate_->counters()->gc_finalize_background();
        }
        return isolate_->counters()->gc_finalize_foreground();
      }
    } else {
      if (isolate_->IsIsolateInBackground()) {
        return isolate_->counters()->gc_compactor_background();
      }
      return isolate_->counters()->gc_compactor_foreground();
    }
  }
}

HistogramTimer* Heap::GCTypeTimer(GarbageCollector collector) {
  if (IsYoungGenerationCollector(collector)) {
    return isolate_->counters()->gc_scavenger();
  } else {
    if (!incremental_marking()->IsStopped()) {
      if (ShouldReduceMemory()) {
        return isolate_->counters()->gc_finalize_reduce_memory();
      } else {
        return isolate_->counters()->gc_finalize();
      }
    } else {
      return isolate_->counters()->gc_compactor();
    }
  }
}

void Heap::CollectAllGarbage(int flags, GarbageCollectionReason gc_reason,
                             const v8::GCCallbackFlags gc_callback_flags) {
  // Since we are ignoring the return value, the exact choice of space does
  // not matter, so long as we do not specify NEW_SPACE, which would not
  // cause a full GC.
  set_current_gc_flags(flags);
  CollectGarbage(OLD_SPACE, gc_reason, gc_callback_flags);
  set_current_gc_flags(kNoGCFlags);
}

namespace {

intptr_t CompareWords(int size, HeapObject* a, HeapObject* b) {
  int words = size / kPointerSize;
  DCHECK_EQ(a->Size(), size);
  DCHECK_EQ(b->Size(), size);
  intptr_t* slot_a = reinterpret_cast<intptr_t*>(a->address());
  intptr_t* slot_b = reinterpret_cast<intptr_t*>(b->address());
  for (int i = 0; i < words; i++) {
    if (*slot_a != *slot_b) {
      return *slot_a - *slot_b;
    }
    slot_a++;
    slot_b++;
  }
  return 0;
}

void ReportDuplicates(int size, std::vector<HeapObject*>& objects) {
  if (objects.size() == 0) return;

  sort(objects.begin(), objects.end(), [size](HeapObject* a, HeapObject* b) {
    intptr_t c = CompareWords(size, a, b);
    if (c != 0) return c < 0;
    return a < b;
  });

  std::vector<std::pair<int, HeapObject*>> duplicates;
  HeapObject* current = objects[0];
  int count = 1;
  for (size_t i = 1; i < objects.size(); i++) {
    if (CompareWords(size, current, objects[i]) == 0) {
      count++;
    } else {
      if (count > 1) {
        duplicates.push_back(std::make_pair(count - 1, current));
      }
      count = 1;
      current = objects[i];
    }
  }
  if (count > 1) {
    duplicates.push_back(std::make_pair(count - 1, current));
  }

  int threshold = FLAG_trace_duplicate_threshold_kb * KB;

  sort(duplicates.begin(), duplicates.end());
  for (auto it = duplicates.rbegin(); it != duplicates.rend(); ++it) {
    int duplicate_bytes = it->first * size;
    if (duplicate_bytes < threshold) break;
    PrintF("%d duplicates of size %d each (%dKB)\n", it->first, size,
           duplicate_bytes / KB);
    PrintF("Sample object: ");
    it->second->Print();
    PrintF("============================\n");
  }
}
}  // anonymous namespace

void Heap::CollectAllAvailableGarbage(GarbageCollectionReason gc_reason) {
  // Since we are ignoring the return value, the exact choice of space does
  // not matter, so long as we do not specify NEW_SPACE, which would not
  // cause a full GC.
  // Major GC would invoke weak handle callbacks on weakly reachable
  // handles, but won't collect weakly reachable objects until next
  // major GC.  Therefore if we collect aggressively and weak handle callback
  // has been invoked, we rerun major GC to release objects which become
  // garbage.
  // Note: as weak callbacks can execute arbitrary code, we cannot
  // hope that eventually there will be no weak callbacks invocations.
  // Therefore stop recollecting after several attempts.
  if (gc_reason == GarbageCollectionReason::kLastResort) {
    InvokeNearHeapLimitCallback();
  }
  RuntimeCallTimerScope runtime_timer(
      isolate(), RuntimeCallCounterId::kGC_Custom_AllAvailableGarbage);

  // The optimizing compiler may be unnecessarily holding on to memory.
  isolate()->AbortConcurrentOptimization(BlockingBehavior::kDontBlock);
  isolate()->ClearSerializerData();
  set_current_gc_flags(kReduceMemoryFootprintMask);
  isolate_->compilation_cache()->Clear();
  const int kMaxNumberOfAttempts = 7;
  const int kMinNumberOfAttempts = 2;
  const v8::GCCallbackFlags callback_flags =
      gc_reason == GarbageCollectionReason::kLowMemoryNotification
          ? v8::kGCCallbackFlagForced
          : v8::kGCCallbackFlagCollectAllAvailableGarbage;
  for (int attempt = 0; attempt < kMaxNumberOfAttempts; attempt++) {
    if (!CollectGarbage(OLD_SPACE, gc_reason, callback_flags) &&
        attempt + 1 >= kMinNumberOfAttempts) {
      break;
    }
  }

  set_current_gc_flags(kNoGCFlags);
  new_space_->Shrink();
  UncommitFromSpace();
  EagerlyFreeExternalMemory();

  if (FLAG_trace_duplicate_threshold_kb) {
    std::map<int, std::vector<HeapObject*>> objects_by_size;
    PagedSpaces spaces(this);
    for (PagedSpace* space = spaces.next(); space != nullptr;
         space = spaces.next()) {
      HeapObjectIterator it(space);
      for (HeapObject* obj = it.Next(); obj != nullptr; obj = it.Next()) {
        objects_by_size[obj->Size()].push_back(obj);
      }
    }
    {
      LargeObjectIterator it(lo_space());
      for (HeapObject* obj = it.Next(); obj != nullptr; obj = it.Next()) {
        objects_by_size[obj->Size()].push_back(obj);
      }
    }
    for (auto it = objects_by_size.rbegin(); it != objects_by_size.rend();
         ++it) {
      ReportDuplicates(it->first, it->second);
    }
  }
}

void Heap::PreciseCollectAllGarbage(int flags,
                                    GarbageCollectionReason gc_reason,
                                    const GCCallbackFlags gc_callback_flags) {
  if (!incremental_marking()->IsStopped()) {
    FinalizeIncrementalMarkingAtomically(gc_reason);
  }
  CollectAllGarbage(flags, gc_reason, gc_callback_flags);
}

void Heap::ReportExternalMemoryPressure() {
  const GCCallbackFlags kGCCallbackFlagsForExternalMemory =
      static_cast<GCCallbackFlags>(
          kGCCallbackFlagSynchronousPhantomCallbackProcessing |
          kGCCallbackFlagCollectAllExternalMemory);
  if (external_memory_ >
      (external_memory_at_last_mark_compact_ + external_memory_hard_limit())) {
    CollectAllGarbage(
        kReduceMemoryFootprintMask,
        GarbageCollectionReason::kExternalMemoryPressure,
        static_cast<GCCallbackFlags>(kGCCallbackFlagCollectAllAvailableGarbage |
                                     kGCCallbackFlagsForExternalMemory));
    return;
  }
  if (incremental_marking()->IsStopped()) {
    if (incremental_marking()->CanBeActivated()) {
      StartIncrementalMarking(GCFlagsForIncrementalMarking(),
                              GarbageCollectionReason::kExternalMemoryPressure,
                              kGCCallbackFlagsForExternalMemory);
    } else {
      CollectAllGarbage(i::Heap::kNoGCFlags,
                        GarbageCollectionReason::kExternalMemoryPressure,
                        kGCCallbackFlagsForExternalMemory);
    }
  } else {
    // Incremental marking is turned on an has already been started.
    const double kMinStepSize = 5;
    const double kMaxStepSize = 10;
    const double ms_step =
        Min(kMaxStepSize,
            Max(kMinStepSize, static_cast<double>(external_memory_) /
                                  external_memory_limit_ * kMinStepSize));
    const double deadline = MonotonicallyIncreasingTimeInMs() + ms_step;
    // Extend the gc callback flags with external memory flags.
    current_gc_callback_flags_ = static_cast<GCCallbackFlags>(
        current_gc_callback_flags_ | kGCCallbackFlagsForExternalMemory);
    incremental_marking()->AdvanceIncrementalMarking(
        deadline, IncrementalMarking::GC_VIA_STACK_GUARD, StepOrigin::kV8);
  }
}

void Heap::EnsureFillerObjectAtTop() {
  // There may be an allocation memento behind objects in new space. Upon
  // evacuation of a non-full new space (or if we are on the last page) there
  // may be uninitialized memory behind top. We fill the remainder of the page
  // with a filler.
  Address to_top = new_space_->top();
  Page* page = Page::FromAddress(to_top - kPointerSize);
  if (page->Contains(to_top)) {
    int remaining_in_page = static_cast<int>(page->area_end() - to_top);
    CreateFillerObjectAt(to_top, remaining_in_page, ClearRecordedSlots::kNo);
  }
}

bool Heap::CollectGarbage(AllocationSpace space,
                          GarbageCollectionReason gc_reason,
                          const v8::GCCallbackFlags gc_callback_flags) {
  const char* collector_reason = nullptr;
  GarbageCollector collector = SelectGarbageCollector(space, &collector_reason);

  if (!CanExpandOldGeneration(new_space()->Capacity())) {
    InvokeNearHeapLimitCallback();
  }

  // Ensure that all pending phantom callbacks are invoked.
  isolate()->global_handles()->InvokeSecondPassPhantomCallbacks();

  // The VM is in the GC state until exiting this function.
  VMState<GC> state(isolate());

#ifdef V8_ENABLE_ALLOCATION_TIMEOUT
  // Reset the allocation timeout, but make sure to allow at least a few
  // allocations after a collection. The reason for this is that we have a lot
  // of allocation sequences and we assume that a garbage collection will allow
  // the subsequent allocation attempts to go through.
  if (FLAG_random_gc_interval > 0 || FLAG_gc_interval >= 0) {
    allocation_timeout_ = Max(6, NextAllocationTimeout(allocation_timeout_));
  }
#endif

  EnsureFillerObjectAtTop();

  if (IsYoungGenerationCollector(collector) &&
      !incremental_marking()->IsStopped()) {
    if (FLAG_trace_incremental_marking) {
      isolate()->PrintWithTimestamp(
          "[IncrementalMarking] Scavenge during marking.\n");
    }
  }

  bool next_gc_likely_to_collect_more = false;
  size_t committed_memory_before = 0;

  if (collector == MARK_COMPACTOR) {
    committed_memory_before = CommittedOldGenerationMemory();
  }

  {
    tracer()->Start(collector, gc_reason, collector_reason);
    DCHECK(AllowHeapAllocation::IsAllowed());
    DisallowHeapAllocation no_allocation_during_gc;
    GarbageCollectionPrologue();

    {
      HistogramTimer* gc_type_timer = GCTypeTimer(collector);
      HistogramTimerScope histogram_timer_scope(gc_type_timer);
      TRACE_EVENT0("v8", gc_type_timer->name());

      HistogramTimer* gc_type_priority_timer = GCTypePriorityTimer(collector);
      OptionalHistogramTimerScopeMode mode =
          isolate_->IsMemorySavingsModeActive()
              ? OptionalHistogramTimerScopeMode::DONT_TAKE_TIME
              : OptionalHistogramTimerScopeMode::TAKE_TIME;
      OptionalHistogramTimerScope histogram_timer_priority_scope(
          gc_type_priority_timer, mode);

      next_gc_likely_to_collect_more =
          PerformGarbageCollection(collector, gc_callback_flags);
      if (collector == MARK_COMPACTOR || collector == SCAVENGER) {
        tracer()->RecordGCPhasesHistograms(gc_type_timer);
      }
    }

    GarbageCollectionEpilogue();
    if (collector == MARK_COMPACTOR && FLAG_track_detached_contexts) {
      isolate()->CheckDetachedContextsAfterGC();
    }

    if (collector == MARK_COMPACTOR) {
      size_t committed_memory_after = CommittedOldGenerationMemory();
      size_t used_memory_after = OldGenerationSizeOfObjects();
      MemoryReducer::Event event;
      event.type = MemoryReducer::kMarkCompact;
      event.time_ms = MonotonicallyIncreasingTimeInMs();
      // Trigger one more GC if
      // - this GC decreased committed memory,
      // - there is high fragmentation,
      // - there are live detached contexts.
      event.next_gc_likely_to_collect_more =
          (committed_memory_before > committed_memory_after + MB) ||
          HasHighFragmentation(used_memory_after, committed_memory_after) ||
          (detached_contexts()->length() > 0);
      event.committed_memory = committed_memory_after;
      if (deserialization_complete_) {
        memory_reducer_->NotifyMarkCompact(event);
      }
    }

    tracer()->Stop(collector);
  }

  if (collector == MARK_COMPACTOR &&
      (gc_callback_flags & (kGCCallbackFlagForced |
                            kGCCallbackFlagCollectAllAvailableGarbage)) != 0) {
    isolate()->CountUsage(v8::Isolate::kForcedGC);
  }

  // Start incremental marking for the next cycle. We do this only for scavenger
  // to avoid a loop where mark-compact causes another mark-compact.
  if (IsYoungGenerationCollector(collector)) {
    StartIncrementalMarkingIfAllocationLimitIsReached(
        GCFlagsForIncrementalMarking(),
        kGCCallbackScheduleIdleGarbageCollection);
  }

  return next_gc_likely_to_collect_more;
}


int Heap::NotifyContextDisposed(bool dependant_context) {
  if (!dependant_context) {
    tracer()->ResetSurvivalEvents();
    old_generation_size_configured_ = false;
    MemoryReducer::Event event;
    event.type = MemoryReducer::kPossibleGarbage;
    event.time_ms = MonotonicallyIncreasingTimeInMs();
    memory_reducer_->NotifyPossibleGarbage(event);
  }
  isolate()->AbortConcurrentOptimization(BlockingBehavior::kDontBlock);

  number_of_disposed_maps_ = retained_maps()->length();
  tracer()->AddContextDisposalTime(MonotonicallyIncreasingTimeInMs());
  return ++contexts_disposed_;
}

void Heap::StartIncrementalMarking(int gc_flags,
                                   GarbageCollectionReason gc_reason,
                                   GCCallbackFlags gc_callback_flags) {
  DCHECK(incremental_marking()->IsStopped());
  set_current_gc_flags(gc_flags);
  current_gc_callback_flags_ = gc_callback_flags;
  incremental_marking()->Start(gc_reason);
}

void Heap::StartIncrementalMarkingIfAllocationLimitIsReached(
    int gc_flags, const GCCallbackFlags gc_callback_flags) {
  if (incremental_marking()->IsStopped()) {
    IncrementalMarkingLimit reached_limit = IncrementalMarkingLimitReached();
    if (reached_limit == IncrementalMarkingLimit::kSoftLimit) {
      incremental_marking()->incremental_marking_job()->ScheduleTask(this);
    } else if (reached_limit == IncrementalMarkingLimit::kHardLimit) {
      StartIncrementalMarking(gc_flags,
                              GarbageCollectionReason::kAllocationLimit,
                              gc_callback_flags);
    }
  }
}

void Heap::StartIdleIncrementalMarking(
    GarbageCollectionReason gc_reason,
    const GCCallbackFlags gc_callback_flags) {
  gc_idle_time_handler_->ResetNoProgressCounter();
  StartIncrementalMarking(kReduceMemoryFootprintMask, gc_reason,
                          gc_callback_flags);
}

void Heap::MoveElements(FixedArray* array, int dst_index, int src_index,
                        int len, WriteBarrierMode mode) {
  if (len == 0) return;

  DCHECK(array->map() != ReadOnlyRoots(this).fixed_cow_array_map());
  Object** dst = array->data_start() + dst_index;
  Object** src = array->data_start() + src_index;
  if (FLAG_concurrent_marking && incremental_marking()->IsMarking()) {
    if (dst < src) {
      for (int i = 0; i < len; i++) {
        base::AsAtomicPointer::Relaxed_Store(
            dst + i, base::AsAtomicPointer::Relaxed_Load(src + i));
      }
    } else {
      for (int i = len - 1; i >= 0; i--) {
        base::AsAtomicPointer::Relaxed_Store(
            dst + i, base::AsAtomicPointer::Relaxed_Load(src + i));
      }
    }
  } else {
    MemMove(dst, src, len * kPointerSize);
  }
  if (mode == SKIP_WRITE_BARRIER) return;
  FIXED_ARRAY_ELEMENTS_WRITE_BARRIER(this, array, dst_index, len);
}


#ifdef VERIFY_HEAP
// Helper class for verifying the string table.
class StringTableVerifier : public ObjectVisitor {
 public:
  explicit StringTableVerifier(Isolate* isolate) : isolate_(isolate) {}

  void VisitPointers(HeapObject* host, Object** start, Object** end) override {
    // Visit all HeapObject pointers in [start, end).
    for (Object** p = start; p < end; p++) {
      DCHECK(!HasWeakHeapObjectTag(*p));
      if ((*p)->IsHeapObject()) {
        HeapObject* object = HeapObject::cast(*p);
        // Check that the string is actually internalized.
        CHECK(object->IsTheHole(isolate_) || object->IsUndefined(isolate_) ||
              object->IsInternalizedString());
      }
    }
  }
  void VisitPointers(HeapObject* host, MaybeObject** start,
                     MaybeObject** end) override {
    UNREACHABLE();
  }

 private:
  Isolate* isolate_;
};

static void VerifyStringTable(Isolate* isolate) {
  StringTableVerifier verifier(isolate);
  isolate->heap()->string_table()->IterateElements(&verifier);
}
#endif  // VERIFY_HEAP

bool Heap::ReserveSpace(Reservation* reservations, std::vector<Address>* maps) {
  bool gc_performed = true;
  int counter = 0;
  static const int kThreshold = 20;
  while (gc_performed && counter++ < kThreshold) {
    gc_performed = false;
    for (int space = FIRST_SPACE;
         space < SerializerDeserializer::kNumberOfSpaces; space++) {
      Reservation* reservation = &reservations[space];
      DCHECK_LE(1, reservation->size());
      if (reservation->at(0).size == 0) {
        DCHECK_EQ(1, reservation->size());
        continue;
      }
      bool perform_gc = false;
      if (space == MAP_SPACE) {
        // We allocate each map individually to avoid fragmentation.
        maps->clear();
        DCHECK_LE(reservation->size(), 2);
        int reserved_size = 0;
        for (const Chunk& c : *reservation) reserved_size += c.size;
        DCHECK_EQ(0, reserved_size % Map::kSize);
        int num_maps = reserved_size / Map::kSize;
        for (int i = 0; i < num_maps; i++) {
          // The deserializer will update the skip list.
          AllocationResult allocation = map_space()->AllocateRawUnaligned(
              Map::kSize, PagedSpace::IGNORE_SKIP_LIST);
          HeapObject* free_space = nullptr;
          if (allocation.To(&free_space)) {
            // Mark with a free list node, in case we have a GC before
            // deserializing.
            Address free_space_address = free_space->address();
            CreateFillerObjectAt(free_space_address, Map::kSize,
                                 ClearRecordedSlots::kNo);
            maps->push_back(free_space_address);
          } else {
            perform_gc = true;
            break;
          }
        }
      } else if (space == LO_SPACE) {
        // Just check that we can allocate during deserialization.
        DCHECK_LE(reservation->size(), 2);
        int reserved_size = 0;
        for (const Chunk& c : *reservation) reserved_size += c.size;
        perform_gc = !CanExpandOldGeneration(reserved_size);
      } else {
        for (auto& chunk : *reservation) {
          AllocationResult allocation;
          int size = chunk.size;
          DCHECK_LE(static_cast<size_t>(size),
                    MemoryAllocator::PageAreaSize(
                        static_cast<AllocationSpace>(space)));
          if (space == NEW_SPACE) {
            allocation = new_space()->AllocateRawUnaligned(size);
          } else {
            // The deserializer will update the skip list.
            allocation = paged_space(space)->AllocateRawUnaligned(
                size, PagedSpace::IGNORE_SKIP_LIST);
          }
          HeapObject* free_space = nullptr;
          if (allocation.To(&free_space)) {
            // Mark with a free list node, in case we have a GC before
            // deserializing.
            Address free_space_address = free_space->address();
            CreateFillerObjectAt(free_space_address, size,
                                 ClearRecordedSlots::kNo);
            DCHECK_GT(SerializerDeserializer::kNumberOfPreallocatedSpaces,
                      space);
            chunk.start = free_space_address;
            chunk.end = free_space_address + size;
          } else {
            perform_gc = true;
            break;
          }
        }
      }
      if (perform_gc) {
        // We cannot perfom a GC with an uninitialized isolate. This check
        // fails for example if the max old space size is chosen unwisely,
        // so that we cannot allocate space to deserialize the initial heap.
        if (!deserialization_complete_) {
          V8::FatalProcessOutOfMemory(
              isolate(), "insufficient memory to create an Isolate");
        }
        if (space == NEW_SPACE) {
          CollectGarbage(NEW_SPACE, GarbageCollectionReason::kDeserializer);
        } else {
          if (counter > 1) {
            CollectAllGarbage(kReduceMemoryFootprintMask,
                              GarbageCollectionReason::kDeserializer);
          } else {
            CollectAllGarbage(kNoGCFlags,
                              GarbageCollectionReason::kDeserializer);
          }
        }
        gc_performed = true;
        break;  // Abort for-loop over spaces and retry.
      }
    }
  }

  return !gc_performed;
}


void Heap::EnsureFromSpaceIsCommitted() {
  if (new_space_->CommitFromSpaceIfNeeded()) return;

  // Committing memory to from space failed.
  // Memory is exhausted and we will die.
  FatalProcessOutOfMemory("Committing semi space failed.");
}


void Heap::UpdateSurvivalStatistics(int start_new_space_size) {
  if (start_new_space_size == 0) return;

  promotion_ratio_ = (static_cast<double>(promoted_objects_size_) /
                      static_cast<double>(start_new_space_size) * 100);

  if (previous_semi_space_copied_object_size_ > 0) {
    promotion_rate_ =
        (static_cast<double>(promoted_objects_size_) /
         static_cast<double>(previous_semi_space_copied_object_size_) * 100);
  } else {
    promotion_rate_ = 0;
  }

  semi_space_copied_rate_ =
      (static_cast<double>(semi_space_copied_object_size_) /
       static_cast<double>(start_new_space_size) * 100);

  double survival_rate = promotion_ratio_ + semi_space_copied_rate_;
  tracer()->AddSurvivalRatio(survival_rate);
}

bool Heap::PerformGarbageCollection(
    GarbageCollector collector, const v8::GCCallbackFlags gc_callback_flags) {
  int freed_global_handles = 0;

  if (!IsYoungGenerationCollector(collector)) {
    PROFILE(isolate_, CodeMovingGCEvent());
  }

#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) {
    VerifyStringTable(this->isolate());
  }
#endif

  GCType gc_type =
      collector == MARK_COMPACTOR ? kGCTypeMarkSweepCompact : kGCTypeScavenge;

  {
    GCCallbacksScope scope(this);
    if (scope.CheckReenter()) {
      AllowHeapAllocation allow_allocation;
      TRACE_GC(tracer(), GCTracer::Scope::HEAP_EXTERNAL_PROLOGUE);
      VMState<EXTERNAL> state(isolate_);
      HandleScope handle_scope(isolate_);
      CallGCPrologueCallbacks(gc_type, kNoGCCallbackFlags);
    }
  }

  EnsureFromSpaceIsCommitted();

  size_t start_new_space_size = Heap::new_space()->Size();

  {
    Heap::SkipStoreBufferScope skip_store_buffer_scope(store_buffer_);

    switch (collector) {
      case MARK_COMPACTOR:
        UpdateOldGenerationAllocationCounter();
        // Perform mark-sweep with optional compaction.
        MarkCompact();
        old_generation_size_configured_ = true;
        // This should be updated before PostGarbageCollectionProcessing, which
        // can cause another GC. Take into account the objects promoted during
        // GC.
        old_generation_allocation_counter_at_last_gc_ +=
            static_cast<size_t>(promoted_objects_size_);
        old_generation_size_at_last_gc_ = OldGenerationSizeOfObjects();
        break;
      case MINOR_MARK_COMPACTOR:
        MinorMarkCompact();
        break;
      case SCAVENGER:
        if ((fast_promotion_mode_ &&
             CanExpandOldGeneration(new_space()->Size()))) {
          tracer()->NotifyYoungGenerationHandling(
              YoungGenerationHandling::kFastPromotionDuringScavenge);
          EvacuateYoungGeneration();
        } else {
          tracer()->NotifyYoungGenerationHandling(
              YoungGenerationHandling::kRegularScavenge);

          Scavenge();
        }
        break;
    }

    ProcessPretenuringFeedback();
  }

  UpdateSurvivalStatistics(static_cast<int>(start_new_space_size));
  ConfigureInitialOldGenerationSize();

  if (collector != MARK_COMPACTOR) {
    // Objects that died in the new space might have been accounted
    // as bytes marked ahead of schedule by the incremental marker.
    incremental_marking()->UpdateMarkedBytesAfterScavenge(
        start_new_space_size - SurvivedNewSpaceObjectSize());
  }

  if (!fast_promotion_mode_ || collector == MARK_COMPACTOR) {
    ComputeFastPromotionMode();
  }

  isolate_->counters()->objs_since_last_young()->Set(0);

  gc_post_processing_depth_++;
  {
    AllowHeapAllocation allow_allocation;
    TRACE_GC(tracer(), GCTracer::Scope::HEAP_EXTERNAL_WEAK_GLOBAL_HANDLES);
    freed_global_handles =
        isolate_->global_handles()->PostGarbageCollectionProcessing(
            collector, gc_callback_flags);
  }
  gc_post_processing_depth_--;

  isolate_->eternal_handles()->PostGarbageCollectionProcessing();

  // Update relocatables.
  Relocatable::PostGarbageCollectionProcessing(isolate_);

  double gc_speed = tracer()->CombinedMarkCompactSpeedInBytesPerMillisecond();
  double mutator_speed =
      tracer()->CurrentOldGenerationAllocationThroughputInBytesPerMillisecond();
  size_t old_gen_size = OldGenerationSizeOfObjects();
  if (collector == MARK_COMPACTOR) {
    // Register the amount of external allocated memory.
    external_memory_at_last_mark_compact_ = external_memory_;
    external_memory_limit_ = external_memory_ + kExternalAllocationSoftLimit;

    double max_factor =
        heap_controller()->MaxGrowingFactor(max_old_generation_size_);
    size_t new_limit = heap_controller()->CalculateAllocationLimit(
        old_gen_size, max_old_generation_size_, max_factor, gc_speed,
        mutator_speed, new_space()->Capacity(), CurrentHeapGrowingMode());
    old_generation_allocation_limit_ = new_limit;

    CheckIneffectiveMarkCompact(
        old_gen_size, tracer()->AverageMarkCompactMutatorUtilization());
  } else if (HasLowYoungGenerationAllocationRate() &&
             old_generation_size_configured_) {
    double max_factor =
        heap_controller()->MaxGrowingFactor(max_old_generation_size_);
    size_t new_limit = heap_controller()->CalculateAllocationLimit(
        old_gen_size, max_old_generation_size_, max_factor, gc_speed,
        mutator_speed, new_space()->Capacity(), CurrentHeapGrowingMode());
    if (new_limit < old_generation_allocation_limit_) {
      old_generation_allocation_limit_ = new_limit;
    }
  }

  {
    GCCallbacksScope scope(this);
    if (scope.CheckReenter()) {
      AllowHeapAllocation allow_allocation;
      TRACE_GC(tracer(), GCTracer::Scope::HEAP_EXTERNAL_EPILOGUE);
      VMState<EXTERNAL> state(isolate_);
      HandleScope handle_scope(isolate_);
      CallGCEpilogueCallbacks(gc_type, gc_callback_flags);
    }
  }

#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) {
    VerifyStringTable(this->isolate());
  }
#endif

  return freed_global_handles > 0;
}


void Heap::CallGCPrologueCallbacks(GCType gc_type, GCCallbackFlags flags) {
  RuntimeCallTimerScope runtime_timer(
      isolate(), RuntimeCallCounterId::kGCPrologueCallback);
  for (const GCCallbackTuple& info : gc_prologue_callbacks_) {
    if (gc_type & info.gc_type) {
      v8::Isolate* isolate = reinterpret_cast<v8::Isolate*>(this->isolate());
      info.callback(isolate, gc_type, flags, info.data);
    }
  }
}

void Heap::CallGCEpilogueCallbacks(GCType gc_type, GCCallbackFlags flags) {
  RuntimeCallTimerScope runtime_timer(
      isolate(), RuntimeCallCounterId::kGCEpilogueCallback);
  for (const GCCallbackTuple& info : gc_epilogue_callbacks_) {
    if (gc_type & info.gc_type) {
      v8::Isolate* isolate = reinterpret_cast<v8::Isolate*>(this->isolate());
      info.callback(isolate, gc_type, flags, info.data);
    }
  }
}


void Heap::MarkCompact() {
  PauseAllocationObserversScope pause_observers(this);

  SetGCState(MARK_COMPACT);

  LOG(isolate_, ResourceEvent("markcompact", "begin"));

  uint64_t size_of_objects_before_gc = SizeOfObjects();

  CodeSpaceMemoryModificationScope code_modifcation(this);

  mark_compact_collector()->Prepare();

  ms_count_++;

  MarkCompactPrologue();

  mark_compact_collector()->CollectGarbage();

  LOG(isolate_, ResourceEvent("markcompact", "end"));

  MarkCompactEpilogue();

  if (FLAG_allocation_site_pretenuring) {
    EvaluateOldSpaceLocalPretenuring(size_of_objects_before_gc);
  }
}

void Heap::MinorMarkCompact() {
#ifdef ENABLE_MINOR_MC
  DCHECK(FLAG_minor_mc);

  PauseAllocationObserversScope pause_observers(this);
  SetGCState(MINOR_MARK_COMPACT);
  LOG(isolate_, ResourceEvent("MinorMarkCompact", "begin"));

  TRACE_GC(tracer(), GCTracer::Scope::MINOR_MC);
  AlwaysAllocateScope always_allocate(isolate());
  IncrementalMarking::PauseBlackAllocationScope pause_black_allocation(
      incremental_marking());
  ConcurrentMarking::PauseScope pause_scope(concurrent_marking());

  minor_mark_compact_collector()->CollectGarbage();

  LOG(isolate_, ResourceEvent("MinorMarkCompact", "end"));
  SetGCState(NOT_IN_GC);
#else
  UNREACHABLE();
#endif  // ENABLE_MINOR_MC
}

void Heap::MarkCompactEpilogue() {
  TRACE_GC(tracer(), GCTracer::Scope::MC_EPILOGUE);
  SetGCState(NOT_IN_GC);

  isolate_->counters()->objs_since_last_full()->Set(0);

  incremental_marking()->Epilogue();

  DCHECK(incremental_marking()->IsStopped());
}


void Heap::MarkCompactPrologue() {
  TRACE_GC(tracer(), GCTracer::Scope::MC_PROLOGUE);
  isolate_->context_slot_cache()->Clear();
  isolate_->descriptor_lookup_cache()->Clear();
  RegExpResultsCache::Clear(string_split_cache());
  RegExpResultsCache::Clear(regexp_multiple_cache());

  isolate_->compilation_cache()->MarkCompactPrologue();

  FlushNumberStringCache();
}


void Heap::CheckNewSpaceExpansionCriteria() {
  if (FLAG_experimental_new_space_growth_heuristic) {
    if (new_space_->TotalCapacity() < new_space_->MaximumCapacity() &&
        survived_last_scavenge_ * 100 / new_space_->TotalCapacity() >= 10) {
      // Grow the size of new space if there is room to grow, and more than 10%
      // have survived the last scavenge.
      new_space_->Grow();
      survived_since_last_expansion_ = 0;
    }
  } else if (new_space_->TotalCapacity() < new_space_->MaximumCapacity() &&
             survived_since_last_expansion_ > new_space_->TotalCapacity()) {
    // Grow the size of new space if there is room to grow, and enough data
    // has survived scavenge since the last expansion.
    new_space_->Grow();
    survived_since_last_expansion_ = 0;
  }
}

void Heap::EvacuateYoungGeneration() {
  TRACE_GC(tracer(), GCTracer::Scope::SCAVENGER_FAST_PROMOTE);
  base::LockGuard<base::Mutex> guard(relocation_mutex());
  ConcurrentMarking::PauseScope pause_scope(concurrent_marking());
  if (!FLAG_concurrent_marking) {
    DCHECK(fast_promotion_mode_);
    DCHECK(CanExpandOldGeneration(new_space()->Size()));
  }

  mark_compact_collector()->sweeper()->EnsureIterabilityCompleted();

  SetGCState(SCAVENGE);
  LOG(isolate_, ResourceEvent("scavenge", "begin"));

  // Move pages from new->old generation.
  PageRange range(new_space()->first_allocatable_address(), new_space()->top());
  for (auto it = range.begin(); it != range.end();) {
    Page* p = (*++it)->prev_page();
    new_space()->from_space().RemovePage(p);
    Page::ConvertNewToOld(p);
    if (incremental_marking()->IsMarking())
      mark_compact_collector()->RecordLiveSlotsOnPage(p);
  }

  // Reset new space.
  if (!new_space()->Rebalance()) {
    FatalProcessOutOfMemory("NewSpace::Rebalance");
  }
  new_space()->ResetLinearAllocationArea();
  new_space()->set_age_mark(new_space()->top());

  // Fix up special trackers.
  external_string_table_.PromoteAllNewSpaceStrings();
  // GlobalHandles are updated in PostGarbageCollectonProcessing

  IncrementYoungSurvivorsCounter(new_space()->Size());
  IncrementPromotedObjectsSize(new_space()->Size());
  IncrementSemiSpaceCopiedObjectSize(0);

  LOG(isolate_, ResourceEvent("scavenge", "end"));
  SetGCState(NOT_IN_GC);
}

void Heap::Scavenge() {
  TRACE_GC(tracer(), GCTracer::Scope::SCAVENGER_SCAVENGE);
  base::LockGuard<base::Mutex> guard(relocation_mutex());
  ConcurrentMarking::PauseScope pause_scope(concurrent_marking());
  // There are soft limits in the allocation code, designed to trigger a mark
  // sweep collection by failing allocations. There is no sense in trying to
  // trigger one during scavenge: scavenges allocation should always succeed.
  AlwaysAllocateScope scope(isolate());

  // Bump-pointer allocations done during scavenge are not real allocations.
  // Pause the inline allocation steps.
  PauseAllocationObserversScope pause_observers(this);
  IncrementalMarking::PauseBlackAllocationScope pause_black_allocation(
      incremental_marking());


  mark_compact_collector()->sweeper()->EnsureIterabilityCompleted();

  SetGCState(SCAVENGE);

  // Flip the semispaces.  After flipping, to space is empty, from space has
  // live objects.
  new_space()->Flip();
  new_space()->ResetLinearAllocationArea();

  // We also flip the young generation large object space. All large objects
  // will be in the from space.
  new_lo_space()->Flip();

  // Implements Cheney's copying algorithm
  LOG(isolate_, ResourceEvent("scavenge", "begin"));

  scavenger_collector_->CollectGarbage();

  LOG(isolate_, ResourceEvent("scavenge", "end"));

  SetGCState(NOT_IN_GC);
}

void Heap::ComputeFastPromotionMode() {
  const size_t survived_in_new_space =
      survived_last_scavenge_ * 100 / new_space_->Capacity();
  fast_promotion_mode_ =
      !FLAG_optimize_for_size && FLAG_fast_promotion_new_space &&
      !ShouldReduceMemory() && new_space_->IsAtMaximumCapacity() &&
      survived_in_new_space >= kMinPromotedPercentForFastPromotionMode;
  if (FLAG_trace_gc_verbose && !FLAG_trace_gc_ignore_scavenger) {
    PrintIsolate(
        isolate(), "Fast promotion mode: %s survival rate: %" PRIuS "%%\n",
        fast_promotion_mode_ ? "true" : "false", survived_in_new_space);
  }
}

void Heap::UnprotectAndRegisterMemoryChunk(MemoryChunk* chunk) {
  if (unprotected_memory_chunks_registry_enabled_) {
    base::LockGuard<base::Mutex> guard(&unprotected_memory_chunks_mutex_);
    if (unprotected_memory_chunks_.insert(chunk).second) {
      chunk->SetReadAndWritable();
    }
  }
}

void Heap::UnprotectAndRegisterMemoryChunk(HeapObject* object) {
  UnprotectAndRegisterMemoryChunk(MemoryChunk::FromAddress(object->address()));
}

void Heap::UnregisterUnprotectedMemoryChunk(MemoryChunk* chunk) {
  unprotected_memory_chunks_.erase(chunk);
}

void Heap::ProtectUnprotectedMemoryChunks() {
  DCHECK(unprotected_memory_chunks_registry_enabled_);
  for (auto chunk = unprotected_memory_chunks_.begin();
       chunk != unprotected_memory_chunks_.end(); chunk++) {
    CHECK(memory_allocator()->IsMemoryChunkExecutable(*chunk));
    (*chunk)->SetReadAndExecutable();
  }
  unprotected_memory_chunks_.clear();
}

bool Heap::ExternalStringTable::Contains(HeapObject* obj) {
  for (size_t i = 0; i < new_space_strings_.size(); ++i) {
    if (new_space_strings_[i] == obj) return true;
  }
  for (size_t i = 0; i < old_space_strings_.size(); ++i) {
    if (old_space_strings_[i] == obj) return true;
  }
  return false;
}

String* Heap::UpdateNewSpaceReferenceInExternalStringTableEntry(Heap* heap,
                                                                Object** p) {
  MapWord first_word = HeapObject::cast(*p)->map_word();

  if (!first_word.IsForwardingAddress()) {
    // Unreachable external string can be finalized.
    String* string = String::cast(*p);
    if (!string->IsExternalString()) {
      // Original external string has been internalized.
      DCHECK(string->IsThinString());
      return nullptr;
    }
    heap->FinalizeExternalString(string);
    return nullptr;
  }

  // String is still reachable.
  String* new_string = String::cast(first_word.ToForwardingAddress());
  if (new_string->IsThinString()) {
    // Filtering Thin strings out of the external string table.
    return nullptr;
  } else if (new_string->IsExternalString()) {
    MemoryChunk::MoveExternalBackingStoreBytes(
        ExternalBackingStoreType::kExternalString,
        Page::FromAddress(reinterpret_cast<Address>(*p)),
        Page::FromHeapObject(new_string),
        ExternalString::cast(new_string)->ExternalPayloadSize());
    return new_string;
  }

  // Internalization can replace external strings with non-external strings.
  return new_string->IsExternalString() ? new_string : nullptr;
}

void Heap::ExternalStringTable::VerifyNewSpace() {
#ifdef DEBUG
  std::set<String*> visited_map;
  std::map<MemoryChunk*, size_t> size_map;
  ExternalBackingStoreType type = ExternalBackingStoreType::kExternalString;
  for (size_t i = 0; i < new_space_strings_.size(); ++i) {
    String* obj = String::cast(new_space_strings_[i]);
    MemoryChunk* mc = MemoryChunk::FromHeapObject(obj);
    DCHECK(mc->InNewSpace());
    DCHECK(heap_->InNewSpace(obj));
    DCHECK(!obj->IsTheHole(heap_->isolate()));
    DCHECK(obj->IsExternalString());
    // Note: we can have repeated elements in the table.
    DCHECK_EQ(0, visited_map.count(obj));
    visited_map.insert(obj);
    size_map[mc] += ExternalString::cast(obj)->ExternalPayloadSize();
  }
  for (std::map<MemoryChunk*, size_t>::iterator it = size_map.begin();
       it != size_map.end(); it++)
    DCHECK_EQ(it->first->ExternalBackingStoreBytes(type), it->second);
#endif
}

void Heap::ExternalStringTable::Verify() {
#ifdef DEBUG
  std::set<String*> visited_map;
  std::map<MemoryChunk*, size_t> size_map;
  ExternalBackingStoreType type = ExternalBackingStoreType::kExternalString;
  VerifyNewSpace();
  for (size_t i = 0; i < old_space_strings_.size(); ++i) {
    String* obj = String::cast(old_space_strings_[i]);
    MemoryChunk* mc = MemoryChunk::FromHeapObject(obj);
    DCHECK(!mc->InNewSpace());
    DCHECK(!heap_->InNewSpace(obj));
    DCHECK(!obj->IsTheHole(heap_->isolate()));
    DCHECK(obj->IsExternalString());
    // Note: we can have repeated elements in the table.
    DCHECK_EQ(0, visited_map.count(obj));
    visited_map.insert(obj);
    size_map[mc] += ExternalString::cast(obj)->ExternalPayloadSize();
  }
  for (std::map<MemoryChunk*, size_t>::iterator it = size_map.begin();
       it != size_map.end(); it++)
    DCHECK_EQ(it->first->ExternalBackingStoreBytes(type), it->second);
#endif
}

void Heap::ExternalStringTable::UpdateNewSpaceReferences(
    Heap::ExternalStringTableUpdaterCallback updater_func) {
  if (new_space_strings_.empty()) return;

  Object** start = new_space_strings_.data();
  Object** end = start + new_space_strings_.size();
  Object** last = start;

  for (Object** p = start; p < end; ++p) {
    String* target = updater_func(heap_, p);

    if (target == nullptr) continue;

    DCHECK(target->IsExternalString());

    if (InNewSpace(target)) {
      // String is still in new space. Update the table entry.
      *last = target;
      ++last;
    } else {
      // String got promoted. Move it to the old string list.
      old_space_strings_.push_back(target);
    }
  }

  DCHECK_LE(last, end);
  new_space_strings_.resize(static_cast<size_t>(last - start));
#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) {
    VerifyNewSpace();
  }
#endif
}

void Heap::ExternalStringTable::PromoteAllNewSpaceStrings() {
  old_space_strings_.reserve(old_space_strings_.size() +
                             new_space_strings_.size());
  std::move(std::begin(new_space_strings_), std::end(new_space_strings_),
            std::back_inserter(old_space_strings_));
  new_space_strings_.clear();
}

void Heap::ExternalStringTable::IterateNewSpaceStrings(RootVisitor* v) {
  if (!new_space_strings_.empty()) {
    v->VisitRootPointers(Root::kExternalStringsTable, nullptr,
                         new_space_strings_.data(),
                         new_space_strings_.data() + new_space_strings_.size());
  }
}

void Heap::ExternalStringTable::IterateAll(RootVisitor* v) {
  IterateNewSpaceStrings(v);
  if (!old_space_strings_.empty()) {
    v->VisitRootPointers(Root::kExternalStringsTable, nullptr,
                         old_space_strings_.data(),
                         old_space_strings_.data() + old_space_strings_.size());
  }
}

void Heap::UpdateNewSpaceReferencesInExternalStringTable(
    ExternalStringTableUpdaterCallback updater_func) {
  external_string_table_.UpdateNewSpaceReferences(updater_func);
}

void Heap::ExternalStringTable::UpdateReferences(
    Heap::ExternalStringTableUpdaterCallback updater_func) {
  if (old_space_strings_.size() > 0) {
    Object** start = old_space_strings_.data();
    Object** end = start + old_space_strings_.size();
    for (Object** p = start; p < end; ++p) *p = updater_func(heap_, p);
  }

  UpdateNewSpaceReferences(updater_func);
}

void Heap::UpdateReferencesInExternalStringTable(
    ExternalStringTableUpdaterCallback updater_func) {
  external_string_table_.UpdateReferences(updater_func);
}


void Heap::ProcessAllWeakReferences(WeakObjectRetainer* retainer) {
  ProcessNativeContexts(retainer);
  ProcessAllocationSites(retainer);
}


void Heap::ProcessYoungWeakReferences(WeakObjectRetainer* retainer) {
  ProcessNativeContexts(retainer);
}


void Heap::ProcessNativeContexts(WeakObjectRetainer* retainer) {
  Object* head = VisitWeakList<Context>(this, native_contexts_list(), retainer);
  // Update the head of the list of contexts.
  set_native_contexts_list(head);
}


void Heap::ProcessAllocationSites(WeakObjectRetainer* retainer) {
  Object* allocation_site_obj =
      VisitWeakList<AllocationSite>(this, allocation_sites_list(), retainer);
  set_allocation_sites_list(allocation_site_obj);
}

void Heap::ProcessWeakListRoots(WeakObjectRetainer* retainer) {
  set_native_contexts_list(retainer->RetainAs(native_contexts_list()));
  set_allocation_sites_list(retainer->RetainAs(allocation_sites_list()));
}

void Heap::ForeachAllocationSite(
    Object* list, const std::function<void(AllocationSite*)>& visitor) {
  DisallowHeapAllocation disallow_heap_allocation;
  Object* current = list;
  while (current->IsAllocationSite()) {
    AllocationSite* site = AllocationSite::cast(current);
    visitor(site);
    Object* current_nested = site->nested_site();
    while (current_nested->IsAllocationSite()) {
      AllocationSite* nested_site = AllocationSite::cast(current_nested);
      visitor(nested_site);
      current_nested = nested_site->nested_site();
    }
    current = site->weak_next();
  }
}

void Heap::ResetAllAllocationSitesDependentCode(PretenureFlag flag) {
  DisallowHeapAllocation no_allocation_scope;
  bool marked = false;

  ForeachAllocationSite(allocation_sites_list(),
                        [&marked, flag, this](AllocationSite* site) {
                          if (site->GetPretenureMode() == flag) {
                            site->ResetPretenureDecision();
                            site->set_deopt_dependent_code(true);
                            marked = true;
                            RemoveAllocationSitePretenuringFeedback(site);
                            return;
                          }
                        });
  if (marked) isolate_->stack_guard()->RequestDeoptMarkedAllocationSites();
}


void Heap::EvaluateOldSpaceLocalPretenuring(
    uint64_t size_of_objects_before_gc) {
  uint64_t size_of_objects_after_gc = SizeOfObjects();
  double old_generation_survival_rate =
      (static_cast<double>(size_of_objects_after_gc) * 100) /
      static_cast<double>(size_of_objects_before_gc);

  if (old_generation_survival_rate < kOldSurvivalRateLowThreshold) {
    // Too many objects died in the old generation, pretenuring of wrong
    // allocation sites may be the cause for that. We have to deopt all
    // dependent code registered in the allocation sites to re-evaluate
    // our pretenuring decisions.
    ResetAllAllocationSitesDependentCode(TENURED);
    if (FLAG_trace_pretenuring) {
      PrintF(
          "Deopt all allocation sites dependent code due to low survival "
          "rate in the old generation %f\n",
          old_generation_survival_rate);
    }
  }
}


void Heap::VisitExternalResources(v8::ExternalResourceVisitor* visitor) {
  DisallowHeapAllocation no_allocation;
  // All external strings are listed in the external string table.

  class ExternalStringTableVisitorAdapter : public RootVisitor {
   public:
    explicit ExternalStringTableVisitorAdapter(
        Isolate* isolate, v8::ExternalResourceVisitor* visitor)
        : isolate_(isolate), visitor_(visitor) {}
    void VisitRootPointers(Root root, const char* description,
                                   Object** start, Object** end) override {
      for (Object** p = start; p < end; p++) {
        DCHECK((*p)->IsExternalString());
        visitor_->VisitExternalString(
            Utils::ToLocal(Handle<String>(String::cast(*p), isolate_)));
      }
    }

   private:
    Isolate* isolate_;
    v8::ExternalResourceVisitor* visitor_;
  } external_string_table_visitor(isolate(), visitor);

  external_string_table_.IterateAll(&external_string_table_visitor);
}

STATIC_ASSERT((FixedDoubleArray::kHeaderSize & kDoubleAlignmentMask) ==
              0);  // NOLINT
STATIC_ASSERT((FixedTypedArrayBase::kDataOffset & kDoubleAlignmentMask) ==
              0);  // NOLINT
#ifdef V8_HOST_ARCH_32_BIT
STATIC_ASSERT((HeapNumber::kValueOffset & kDoubleAlignmentMask) !=
              0);  // NOLINT
#endif


int Heap::GetMaximumFillToAlign(AllocationAlignment alignment) {
  switch (alignment) {
    case kWordAligned:
      return 0;
    case kDoubleAligned:
    case kDoubleUnaligned:
      return kDoubleSize - kPointerSize;
    default:
      UNREACHABLE();
  }
  return 0;
}


int Heap::GetFillToAlign(Address address, AllocationAlignment alignment) {
  if (alignment == kDoubleAligned && (address & kDoubleAlignmentMask) != 0)
    return kPointerSize;
  if (alignment == kDoubleUnaligned && (address & kDoubleAlignmentMask) == 0)
    return kDoubleSize - kPointerSize;  // No fill if double is always aligned.
  return 0;
}


HeapObject* Heap::PrecedeWithFiller(HeapObject* object, int filler_size) {
  CreateFillerObjectAt(object->address(), filler_size, ClearRecordedSlots::kNo);
  return HeapObject::FromAddress(object->address() + filler_size);
}


HeapObject* Heap::AlignWithFiller(HeapObject* object, int object_size,
                                  int allocation_size,
                                  AllocationAlignment alignment) {
  int filler_size = allocation_size - object_size;
  DCHECK_LT(0, filler_size);
  int pre_filler = GetFillToAlign(object->address(), alignment);
  if (pre_filler) {
    object = PrecedeWithFiller(object, pre_filler);
    filler_size -= pre_filler;
  }
  if (filler_size)
    CreateFillerObjectAt(object->address() + object_size, filler_size,
                         ClearRecordedSlots::kNo);
  return object;
}

void Heap::RegisterNewArrayBuffer(JSArrayBuffer* buffer) {
  ArrayBufferTracker::RegisterNew(this, buffer);
}


void Heap::UnregisterArrayBuffer(JSArrayBuffer* buffer) {
  ArrayBufferTracker::Unregister(this, buffer);
}

void Heap::ConfigureInitialOldGenerationSize() {
  if (!old_generation_size_configured_ && tracer()->SurvivalEventsRecorded()) {
    old_generation_allocation_limit_ =
        Max(heap_controller()->MinimumAllocationLimitGrowingStep(
                CurrentHeapGrowingMode()),
            static_cast<size_t>(
                static_cast<double>(old_generation_allocation_limit_) *
                (tracer()->AverageSurvivalRatio() / 100)));
  }
}

void Heap::CreateJSEntryStub() {
  JSEntryStub stub(isolate(), StackFrame::ENTRY);
  set_js_entry_code(*stub.GetCode());
}


void Heap::CreateJSConstructEntryStub() {
  JSEntryStub stub(isolate(), StackFrame::CONSTRUCT_ENTRY);
  set_js_construct_entry_code(*stub.GetCode());
}

void Heap::CreateJSRunMicrotasksEntryStub() {
  JSEntryStub stub(isolate(), JSEntryStub::SpecialTarget::kRunMicrotasks);
  set_js_run_microtasks_entry_code(*stub.GetCode());
}

void Heap::CreateFixedStubs() {
  // Here we create roots for fixed stubs. They are needed at GC
  // for cooking and uncooking (check out frames.cc).
  // The eliminates the need for doing dictionary lookup in the
  // stub cache for these stubs.
  HandleScope scope(isolate());
  // Canonicalize handles, so that we can share constant pool entries pointing
  // to code targets without dereferencing their handles.
  CanonicalHandleScope canonical(isolate());

  // Create stubs that should be there, so we don't unexpectedly have to
  // create them if we need them during the creation of another stub.
  // Stub creation mixes raw pointers and handles in an unsafe manner so
  // we cannot create stubs while we are creating stubs.
  CodeStub::GenerateStubsAheadOfTime(isolate());

  // gcc-4.4 has problem generating correct code of following snippet:
  // {  JSEntryStub stub;
  //    js_entry_code_ = *stub.GetCode();
  // }
  // {  JSConstructEntryStub stub;
  //    js_construct_entry_code_ = *stub.GetCode();
  // }
  // To workaround the problem, make separate functions without inlining.
  Heap::CreateJSEntryStub();
  Heap::CreateJSConstructEntryStub();
  Heap::CreateJSRunMicrotasksEntryStub();
}

bool Heap::RootCanBeWrittenAfterInitialization(RootIndex root_index) {
  switch (root_index) {
    case RootIndex::kNumberStringCache:
    case RootIndex::kCodeStubs:
    case RootIndex::kScriptList:
    case RootIndex::kMaterializedObjects:
    case RootIndex::kDetachedContexts:
    case RootIndex::kRetainedMaps:
    case RootIndex::kRetainingPathTargets:
    case RootIndex::kFeedbackVectorsForProfilingTools:
    case RootIndex::kNoScriptSharedFunctionInfos:
    case RootIndex::kSerializedObjects:
    case RootIndex::kSerializedGlobalProxySizes:
    case RootIndex::kPublicSymbolTable:
    case RootIndex::kApiSymbolTable:
    case RootIndex::kApiPrivateSymbolTable:
    case RootIndex::kMessageListeners:
// Smi values
#define SMI_ENTRY(type, name, Name) case RootIndex::k##Name:
      SMI_ROOT_LIST(SMI_ENTRY)
#undef SMI_ENTRY
    // String table
    case RootIndex::kStringTable:
      return true;

    default:
      return false;
  }
}

bool Heap::RootCanBeTreatedAsConstant(RootIndex root_index) {
  bool can_be = !RootCanBeWrittenAfterInitialization(root_index) &&
                !InNewSpace(root(root_index));
  DCHECK_IMPLIES(can_be, IsImmovable(HeapObject::cast(root(root_index))));
  return can_be;
}


void Heap::FlushNumberStringCache() {
  // Flush the number to string cache.
  int len = number_string_cache()->length();
  for (int i = 0; i < len; i++) {
    number_string_cache()->set_undefined(i);
  }
}

HeapObject* Heap::CreateFillerObjectAt(Address addr, int size,
                                       ClearRecordedSlots clear_slots_mode,
                                       ClearFreedMemoryMode clear_memory_mode) {
  if (size == 0) return nullptr;
  HeapObject* filler = HeapObject::FromAddress(addr);
  if (size == kPointerSize) {
    filler->set_map_after_allocation(
        reinterpret_cast<Map*>(root(RootIndex::kOnePointerFillerMap)),
        SKIP_WRITE_BARRIER);
  } else if (size == 2 * kPointerSize) {
    filler->set_map_after_allocation(
        reinterpret_cast<Map*>(root(RootIndex::kTwoPointerFillerMap)),
        SKIP_WRITE_BARRIER);
    if (clear_memory_mode == ClearFreedMemoryMode::kClearFreedMemory) {
      Memory<Address>(addr + kPointerSize) =
          static_cast<Address>(kClearedFreeMemoryValue);
    }
  } else {
    DCHECK_GT(size, 2 * kPointerSize);
    filler->set_map_after_allocation(
        reinterpret_cast<Map*>(root(RootIndex::kFreeSpaceMap)),
        SKIP_WRITE_BARRIER);
    FreeSpace::cast(filler)->relaxed_write_size(size);
    if (clear_memory_mode == ClearFreedMemoryMode::kClearFreedMemory) {
      memset(reinterpret_cast<void*>(addr + 2 * kPointerSize),
             kClearedFreeMemoryValue, size - 2 * kPointerSize);
    }
  }
  if (clear_slots_mode == ClearRecordedSlots::kYes) {
    ClearRecordedSlotRange(addr, addr + size);
  }

  // At this point, we may be deserializing the heap from a snapshot, and
  // none of the maps have been created yet and are nullptr.
  DCHECK((filler->map() == nullptr && !deserialization_complete_) ||
         filler->map()->IsMap());
  return filler;
}


bool Heap::CanMoveObjectStart(HeapObject* object) {
  if (!FLAG_move_object_start) return false;

  // Sampling heap profiler may have a reference to the object.
  if (isolate()->heap_profiler()->is_sampling_allocations()) return false;

  Address address = object->address();

  if (lo_space()->Contains(object)) return false;

  // We can move the object start if the page was already swept.
  return Page::FromAddress(address)->SweepingDone();
}

bool Heap::IsImmovable(HeapObject* object) {
  MemoryChunk* chunk = MemoryChunk::FromAddress(object->address());
  return chunk->NeverEvacuate() || chunk->owner()->identity() == LO_SPACE;
}

#ifdef ENABLE_SLOW_DCHECKS
namespace {

class LeftTrimmerVerifierRootVisitor : public RootVisitor {
 public:
  explicit LeftTrimmerVerifierRootVisitor(FixedArrayBase* to_check)
      : to_check_(to_check) {}

  void VisitRootPointers(Root root, const char* description,
                                 Object** start, Object** end) override {
    for (Object** p = start; p < end; ++p) {
      DCHECK_NE(*p, to_check_);
    }
  }

 private:
  FixedArrayBase* to_check_;

  DISALLOW_COPY_AND_ASSIGN(LeftTrimmerVerifierRootVisitor);
};
}  // namespace
#endif  // ENABLE_SLOW_DCHECKS

namespace {
bool MayContainRecordedSlots(HeapObject* object) {
  // New space object do not have recorded slots.
  if (MemoryChunk::FromHeapObject(object)->InNewSpace()) return false;
  // Whitelist objects that definitely do not have pointers.
  if (object->IsByteArray() || object->IsFixedDoubleArray()) return false;
  // Conservatively return true for other objects.
  return true;
}
}  // namespace

FixedArrayBase* Heap::LeftTrimFixedArray(FixedArrayBase* object,
                                         int elements_to_trim) {
  if (elements_to_trim == 0) {
    // This simplifies reasoning in the rest of the function.
    return object;
  }
  CHECK_NOT_NULL(object);
  DCHECK(CanMoveObjectStart(object));
  // Add custom visitor to concurrent marker if new left-trimmable type
  // is added.
  DCHECK(object->IsFixedArray() || object->IsFixedDoubleArray());
  const int element_size = object->IsFixedArray() ? kPointerSize : kDoubleSize;
  const int bytes_to_trim = elements_to_trim * element_size;
  Map* map = object->map();

  // For now this trick is only applied to objects in new and paged space.
  // In large object space the object's start must coincide with chunk
  // and thus the trick is just not applicable.
  DCHECK(!lo_space()->Contains(object));
  DCHECK(object->map() != ReadOnlyRoots(this).fixed_cow_array_map());

  STATIC_ASSERT(FixedArrayBase::kMapOffset == 0);
  STATIC_ASSERT(FixedArrayBase::kLengthOffset == kPointerSize);
  STATIC_ASSERT(FixedArrayBase::kHeaderSize == 2 * kPointerSize);

  const int len = object->length();
  DCHECK(elements_to_trim <= len);

  // Calculate location of new array start.
  Address old_start = object->address();
  Address new_start = old_start + bytes_to_trim;

  if (incremental_marking()->IsMarking()) {
    incremental_marking()->NotifyLeftTrimming(
        object, HeapObject::FromAddress(new_start));
  }

  // Technically in new space this write might be omitted (except for
  // debug mode which iterates through the heap), but to play safer
  // we still do it.
  HeapObject* filler =
      CreateFillerObjectAt(old_start, bytes_to_trim, ClearRecordedSlots::kYes);

  // Initialize header of the trimmed array. Since left trimming is only
  // performed on pages which are not concurrently swept creating a filler
  // object does not require synchronization.
  RELAXED_WRITE_FIELD(object, bytes_to_trim, map);
  RELAXED_WRITE_FIELD(object, bytes_to_trim + kPointerSize,
                      Smi::FromInt(len - elements_to_trim));

  FixedArrayBase* new_object =
      FixedArrayBase::cast(HeapObject::FromAddress(new_start));

  // Remove recorded slots for the new map and length offset.
  ClearRecordedSlot(new_object, HeapObject::RawField(new_object, 0));
  ClearRecordedSlot(new_object, HeapObject::RawField(
                                    new_object, FixedArrayBase::kLengthOffset));

  // Handle invalidated old-to-old slots.
  if (incremental_marking()->IsCompacting() &&
      MayContainRecordedSlots(new_object)) {
    // If the array was right-trimmed before, then it is registered in
    // the invalidated_slots.
    MemoryChunk::FromHeapObject(new_object)
        ->MoveObjectWithInvalidatedSlots(filler, new_object);
    // We have to clear slots in the free space to avoid stale old-to-old slots.
    // Note we cannot use ClearFreedMemoryMode of CreateFillerObjectAt because
    // we need pointer granularity writes to avoid race with the concurrent
    // marking.
    if (filler->Size() > FreeSpace::kSize) {
      MemsetPointer(HeapObject::RawField(filler, FreeSpace::kSize),
                    ReadOnlyRoots(this).undefined_value(),
                    (filler->Size() - FreeSpace::kSize) / kPointerSize);
    }
  }
  // Notify the heap profiler of change in object layout.
  OnMoveEvent(new_object, object, new_object->Size());

#ifdef ENABLE_SLOW_DCHECKS
  if (FLAG_enable_slow_asserts) {
    // Make sure the stack or other roots (e.g., Handles) don't contain pointers
    // to the original FixedArray (which is now the filler object).
    LeftTrimmerVerifierRootVisitor root_visitor(object);
    IterateRoots(&root_visitor, VISIT_ALL);
  }
#endif  // ENABLE_SLOW_DCHECKS

  return new_object;
}

void Heap::RightTrimFixedArray(FixedArrayBase* object, int elements_to_trim) {
  const int len = object->length();
  DCHECK_LE(elements_to_trim, len);
  DCHECK_GE(elements_to_trim, 0);

  int bytes_to_trim;
  DCHECK(!object->IsFixedTypedArrayBase());
  if (object->IsByteArray()) {
    int new_size = ByteArray::SizeFor(len - elements_to_trim);
    bytes_to_trim = ByteArray::SizeFor(len) - new_size;
    DCHECK_GE(bytes_to_trim, 0);
  } else if (object->IsFixedArray()) {
    CHECK_NE(elements_to_trim, len);
    bytes_to_trim = elements_to_trim * kPointerSize;
  } else {
    DCHECK(object->IsFixedDoubleArray());
    CHECK_NE(elements_to_trim, len);
    bytes_to_trim = elements_to_trim * kDoubleSize;
  }

  CreateFillerForArray<FixedArrayBase>(object, elements_to_trim, bytes_to_trim);
}

void Heap::RightTrimWeakFixedArray(WeakFixedArray* object,
                                   int elements_to_trim) {
  // This function is safe to use only at the end of the mark compact
  // collection: When marking, we record the weak slots, and shrinking
  // invalidates them.
  DCHECK_EQ(gc_state(), MARK_COMPACT);
  CreateFillerForArray<WeakFixedArray>(object, elements_to_trim,
                                       elements_to_trim * kPointerSize);
}

template <typename T>
void Heap::CreateFillerForArray(T* object, int elements_to_trim,
                                int bytes_to_trim) {
  DCHECK(object->IsFixedArrayBase() || object->IsByteArray() ||
         object->IsWeakFixedArray());

  // For now this trick is only applied to objects in new and paged space.
  DCHECK(object->map() != ReadOnlyRoots(this).fixed_cow_array_map());

  if (bytes_to_trim == 0) {
    DCHECK_EQ(elements_to_trim, 0);
    // No need to create filler and update live bytes counters.
    return;
  }

  // Calculate location of new array end.
  int old_size = object->Size();
  Address old_end = object->address() + old_size;
  Address new_end = old_end - bytes_to_trim;

  // Register the array as an object with invalidated old-to-old slots. We
  // cannot use NotifyObjectLayoutChange as it would mark the array black,
  // which is not safe for left-trimming because left-trimming re-pushes
  // only grey arrays onto the marking worklist.
  if (incremental_marking()->IsCompacting() &&
      MayContainRecordedSlots(object)) {
    // Ensure that the object survives because the InvalidatedSlotsFilter will
    // compute its size from its map during pointers updating phase.
    incremental_marking()->WhiteToGreyAndPush(object);
    MemoryChunk::FromHeapObject(object)->RegisterObjectWithInvalidatedSlots(
        object, old_size);
  }

  // Technically in new space this write might be omitted (except for
  // debug mode which iterates through the heap), but to play safer
  // we still do it.
  // We do not create a filler for objects in large object space.
  // TODO(hpayer): We should shrink the large object page if the size
  // of the object changed significantly.
  if (!lo_space()->Contains(object)) {
    HeapObject* filler =
        CreateFillerObjectAt(new_end, bytes_to_trim, ClearRecordedSlots::kYes);
    DCHECK_NOT_NULL(filler);
    // Clear the mark bits of the black area that belongs now to the filler.
    // This is an optimization. The sweeper will release black fillers anyway.
    if (incremental_marking()->black_allocation() &&
        incremental_marking()->marking_state()->IsBlackOrGrey(filler)) {
      Page* page = Page::FromAddress(new_end);
      incremental_marking()->marking_state()->bitmap(page)->ClearRange(
          page->AddressToMarkbitIndex(new_end),
          page->AddressToMarkbitIndex(new_end + bytes_to_trim));
    }
  }

  // Initialize header of the trimmed array. We are storing the new length
  // using release store after creating a filler for the left-over space to
  // avoid races with the sweeper thread.
  object->synchronized_set_length(object->length() - elements_to_trim);

  // Notify the heap object allocation tracker of change in object layout. The
  // array may not be moved during GC, and size has to be adjusted nevertheless.
  for (auto& tracker : allocation_trackers_) {
    tracker->UpdateObjectSizeEvent(object->address(), object->Size());
  }
}

void Heap::MakeHeapIterable() {
  mark_compact_collector()->EnsureSweepingCompleted();
}


static double ComputeMutatorUtilization(double mutator_speed, double gc_speed) {
  const double kMinMutatorUtilization = 0.0;
  const double kConservativeGcSpeedInBytesPerMillisecond = 200000;
  if (mutator_speed == 0) return kMinMutatorUtilization;
  if (gc_speed == 0) gc_speed = kConservativeGcSpeedInBytesPerMillisecond;
  // Derivation:
  // mutator_utilization = mutator_time / (mutator_time + gc_time)
  // mutator_time = 1 / mutator_speed
  // gc_time = 1 / gc_speed
  // mutator_utilization = (1 / mutator_speed) /
  //                       (1 / mutator_speed + 1 / gc_speed)
  // mutator_utilization = gc_speed / (mutator_speed + gc_speed)
  return gc_speed / (mutator_speed + gc_speed);
}


double Heap::YoungGenerationMutatorUtilization() {
  double mutator_speed = static_cast<double>(
      tracer()->NewSpaceAllocationThroughputInBytesPerMillisecond());
  double gc_speed =
      tracer()->ScavengeSpeedInBytesPerMillisecond(kForSurvivedObjects);
  double result = ComputeMutatorUtilization(mutator_speed, gc_speed);
  if (FLAG_trace_mutator_utilization) {
    isolate()->PrintWithTimestamp(
        "Young generation mutator utilization = %.3f ("
        "mutator_speed=%.f, gc_speed=%.f)\n",
        result, mutator_speed, gc_speed);
  }
  return result;
}


double Heap::OldGenerationMutatorUtilization() {
  double mutator_speed = static_cast<double>(
      tracer()->OldGenerationAllocationThroughputInBytesPerMillisecond());
  double gc_speed = static_cast<double>(
      tracer()->CombinedMarkCompactSpeedInBytesPerMillisecond());
  double result = ComputeMutatorUtilization(mutator_speed, gc_speed);
  if (FLAG_trace_mutator_utilization) {
    isolate()->PrintWithTimestamp(
        "Old generation mutator utilization = %.3f ("
        "mutator_speed=%.f, gc_speed=%.f)\n",
        result, mutator_speed, gc_speed);
  }
  return result;
}


bool Heap::HasLowYoungGenerationAllocationRate() {
  const double high_mutator_utilization = 0.993;
  return YoungGenerationMutatorUtilization() > high_mutator_utilization;
}


bool Heap::HasLowOldGenerationAllocationRate() {
  const double high_mutator_utilization = 0.993;
  return OldGenerationMutatorUtilization() > high_mutator_utilization;
}


bool Heap::HasLowAllocationRate() {
  return HasLowYoungGenerationAllocationRate() &&
         HasLowOldGenerationAllocationRate();
}

bool Heap::IsIneffectiveMarkCompact(size_t old_generation_size,
                                    double mutator_utilization) {
  const double kHighHeapPercentage = 0.8;
  const double kLowMutatorUtilization = 0.4;
  return old_generation_size >=
             kHighHeapPercentage * max_old_generation_size_ &&
         mutator_utilization < kLowMutatorUtilization;
}

void Heap::CheckIneffectiveMarkCompact(size_t old_generation_size,
                                       double mutator_utilization) {
  const int kMaxConsecutiveIneffectiveMarkCompacts = 4;
  if (!FLAG_detect_ineffective_gcs_near_heap_limit) return;
  if (!IsIneffectiveMarkCompact(old_generation_size, mutator_utilization)) {
    consecutive_ineffective_mark_compacts_ = 0;
    return;
  }
  ++consecutive_ineffective_mark_compacts_;
  if (consecutive_ineffective_mark_compacts_ ==
      kMaxConsecutiveIneffectiveMarkCompacts) {
    if (InvokeNearHeapLimitCallback()) {
      // The callback increased the heap limit.
      consecutive_ineffective_mark_compacts_ = 0;
      return;
    }
    FatalProcessOutOfMemory("Ineffective mark-compacts near heap limit");
  }
}

bool Heap::HasHighFragmentation() {
  size_t used = OldGenerationSizeOfObjects();
  size_t committed = CommittedOldGenerationMemory();
  return HasHighFragmentation(used, committed);
}

bool Heap::HasHighFragmentation(size_t used, size_t committed) {
  const size_t kSlack = 16 * MB;
  // Fragmentation is high if committed > 2 * used + kSlack.
  // Rewrite the exression to avoid overflow.
  DCHECK_GE(committed, used);
  return committed - used > used + kSlack;
}

bool Heap::ShouldOptimizeForMemoryUsage() {
  const size_t kOldGenerationSlack = max_old_generation_size_ / 8;
  return FLAG_optimize_for_size || isolate()->IsIsolateInBackground() ||
         isolate()->IsMemorySavingsModeActive() || HighMemoryPressure() ||
         !CanExpandOldGeneration(kOldGenerationSlack);
}

void Heap::ActivateMemoryReducerIfNeeded() {
  // Activate memory reducer when switching to background if
  // - there was no mark compact since the start.
  // - the committed memory can be potentially reduced.
  // 2 pages for the old, code, and map space + 1 page for new space.
  const int kMinCommittedMemory = 7 * Page::kPageSize;
  if (ms_count_ == 0 && CommittedMemory() > kMinCommittedMemory &&
      isolate()->IsIsolateInBackground()) {
    MemoryReducer::Event event;
    event.type = MemoryReducer::kPossibleGarbage;
    event.time_ms = MonotonicallyIncreasingTimeInMs();
    memory_reducer_->NotifyPossibleGarbage(event);
  }
}

void Heap::ReduceNewSpaceSize() {
  // TODO(ulan): Unify this constant with the similar constant in
  // GCIdleTimeHandler once the change is merged to 4.5.
  static const size_t kLowAllocationThroughput = 1000;
  const double allocation_throughput =
      tracer()->CurrentAllocationThroughputInBytesPerMillisecond();

  if (FLAG_predictable) return;

  if (ShouldReduceMemory() ||
      ((allocation_throughput != 0) &&
       (allocation_throughput < kLowAllocationThroughput))) {
    new_space_->Shrink();
    UncommitFromSpace();
  }
}

void Heap::FinalizeIncrementalMarkingIfComplete(
    GarbageCollectionReason gc_reason) {
  if (incremental_marking()->IsMarking() &&
      (incremental_marking()->IsReadyToOverApproximateWeakClosure() ||
       (!incremental_marking()->finalize_marking_completed() &&
        mark_compact_collector()->marking_worklist()->IsEmpty() &&
        local_embedder_heap_tracer()->ShouldFinalizeIncrementalMarking()))) {
    FinalizeIncrementalMarkingIncrementally(gc_reason);
  } else if (incremental_marking()->IsComplete() ||
             (mark_compact_collector()->marking_worklist()->IsEmpty() &&
              local_embedder_heap_tracer()
                  ->ShouldFinalizeIncrementalMarking())) {
    CollectAllGarbage(current_gc_flags_, gc_reason, current_gc_callback_flags_);
  }
}

void Heap::FinalizeIncrementalMarkingAtomically(
    GarbageCollectionReason gc_reason) {
  DCHECK(!incremental_marking()->IsStopped());
  CollectAllGarbage(current_gc_flags_, gc_reason, current_gc_callback_flags_);
}

void Heap::FinalizeIncrementalMarkingIncrementally(
    GarbageCollectionReason gc_reason) {
  if (FLAG_trace_incremental_marking) {
    isolate()->PrintWithTimestamp(
        "[IncrementalMarking] (%s).\n",
        Heap::GarbageCollectionReasonToString(gc_reason));
  }

  HistogramTimerScope incremental_marking_scope(
      isolate()->counters()->gc_incremental_marking_finalize());
  TRACE_EVENT0("v8", "V8.GCIncrementalMarkingFinalize");
  TRACE_GC(tracer(), GCTracer::Scope::MC_INCREMENTAL_FINALIZE);

  {
    GCCallbacksScope scope(this);
    if (scope.CheckReenter()) {
      AllowHeapAllocation allow_allocation;
      TRACE_GC(tracer(), GCTracer::Scope::MC_INCREMENTAL_EXTERNAL_PROLOGUE);
      VMState<EXTERNAL> state(isolate_);
      HandleScope handle_scope(isolate_);
      CallGCPrologueCallbacks(kGCTypeIncrementalMarking, kNoGCCallbackFlags);
    }
  }
  incremental_marking()->FinalizeIncrementally();
  {
    GCCallbacksScope scope(this);
    if (scope.CheckReenter()) {
      AllowHeapAllocation allow_allocation;
      TRACE_GC(tracer(), GCTracer::Scope::MC_INCREMENTAL_EXTERNAL_EPILOGUE);
      VMState<EXTERNAL> state(isolate_);
      HandleScope handle_scope(isolate_);
      CallGCEpilogueCallbacks(kGCTypeIncrementalMarking, kNoGCCallbackFlags);
    }
  }
}

void Heap::RegisterDeserializedObjectsForBlackAllocation(
    Reservation* reservations, const std::vector<HeapObject*>& large_objects,
    const std::vector<Address>& maps) {
  // TODO(ulan): pause black allocation during deserialization to avoid
  // iterating all these objects in one go.

  if (!incremental_marking()->black_allocation()) return;

  // Iterate black objects in old space, code space, map space, and large
  // object space for side effects.
  IncrementalMarking::MarkingState* marking_state =
      incremental_marking()->marking_state();
  for (int i = OLD_SPACE; i < Serializer<>::kNumberOfSpaces; i++) {
    const Heap::Reservation& res = reservations[i];
    for (auto& chunk : res) {
      Address addr = chunk.start;
      while (addr < chunk.end) {
        HeapObject* obj = HeapObject::FromAddress(addr);
        // Objects can have any color because incremental marking can
        // start in the middle of Heap::ReserveSpace().
        if (marking_state->IsBlack(obj)) {
          incremental_marking()->ProcessBlackAllocatedObject(obj);
        }
        addr += obj->Size();
      }
    }
  }
  // We potentially deserialized wrappers which require registering with the
  // embedder as the marker will not find them.
  local_embedder_heap_tracer()->RegisterWrappersWithRemoteTracer();

  // Large object space doesn't use reservations, so it needs custom handling.
  for (HeapObject* object : large_objects) {
    incremental_marking()->ProcessBlackAllocatedObject(object);
  }

  // Map space doesn't use reservations, so it needs custom handling.
  for (Address addr : maps) {
    incremental_marking()->ProcessBlackAllocatedObject(
        HeapObject::FromAddress(addr));
  }
}

void Heap::NotifyObjectLayoutChange(HeapObject* object, int size,
                                    const DisallowHeapAllocation&) {
  if (incremental_marking()->IsMarking()) {
    incremental_marking()->MarkBlackAndPush(object);
    if (incremental_marking()->IsCompacting() &&
        MayContainRecordedSlots(object)) {
      MemoryChunk::FromHeapObject(object)->RegisterObjectWithInvalidatedSlots(
          object, size);
    }
  }
#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) {
    DCHECK_NULL(pending_layout_change_object_);
    pending_layout_change_object_ = object;
  }
#endif
}

#ifdef VERIFY_HEAP
// Helper class for collecting slot addresses.
class SlotCollectingVisitor final : public ObjectVisitor {
 public:
  void VisitPointers(HeapObject* host, Object** start, Object** end) override {
    VisitPointers(host, reinterpret_cast<MaybeObject**>(start),
                  reinterpret_cast<MaybeObject**>(end));
  }
  void VisitPointers(HeapObject* host, MaybeObject** start,
                     MaybeObject** end) final {
    for (MaybeObject** p = start; p < end; p++) {
      slots_.push_back(p);
    }
  }

  int number_of_slots() { return static_cast<int>(slots_.size()); }

  MaybeObject** slot(int i) { return slots_[i]; }

 private:
  std::vector<MaybeObject**> slots_;
};

void Heap::VerifyObjectLayoutChange(HeapObject* object, Map* new_map) {
  if (!FLAG_verify_heap) return;

  // Check that Heap::NotifyObjectLayout was called for object transitions
  // that are not safe for concurrent marking.
  // If you see this check triggering for a freshly allocated object,
  // use object->set_map_after_allocation() to initialize its map.
  if (pending_layout_change_object_ == nullptr) {
    if (object->IsJSObject()) {
      DCHECK(!object->map()->TransitionRequiresSynchronizationWithGC(new_map));
    } else {
      // Check that the set of slots before and after the transition match.
      SlotCollectingVisitor old_visitor;
      object->IterateFast(&old_visitor);
      MapWord old_map_word = object->map_word();
      // Temporarily set the new map to iterate new slots.
      object->set_map_word(MapWord::FromMap(new_map));
      SlotCollectingVisitor new_visitor;
      object->IterateFast(&new_visitor);
      // Restore the old map.
      object->set_map_word(old_map_word);
      DCHECK_EQ(new_visitor.number_of_slots(), old_visitor.number_of_slots());
      for (int i = 0; i < new_visitor.number_of_slots(); i++) {
        DCHECK_EQ(new_visitor.slot(i), old_visitor.slot(i));
      }
    }
  } else {
    DCHECK_EQ(pending_layout_change_object_, object);
    pending_layout_change_object_ = nullptr;
  }
}
#endif

GCIdleTimeHeapState Heap::ComputeHeapState() {
  GCIdleTimeHeapState heap_state;
  heap_state.contexts_disposed = contexts_disposed_;
  heap_state.contexts_disposal_rate =
      tracer()->ContextDisposalRateInMilliseconds();
  heap_state.size_of_objects = static_cast<size_t>(SizeOfObjects());
  heap_state.incremental_marking_stopped = incremental_marking()->IsStopped();
  return heap_state;
}


bool Heap::PerformIdleTimeAction(GCIdleTimeAction action,
                                 GCIdleTimeHeapState heap_state,
                                 double deadline_in_ms) {
  bool result = false;
  switch (action.type) {
    case DONE:
      result = true;
      break;
    case DO_INCREMENTAL_STEP: {
      const double remaining_idle_time_in_ms =
          incremental_marking()->AdvanceIncrementalMarking(
              deadline_in_ms, IncrementalMarking::NO_GC_VIA_STACK_GUARD,
              StepOrigin::kTask);
      if (remaining_idle_time_in_ms > 0.0) {
        FinalizeIncrementalMarkingIfComplete(
            GarbageCollectionReason::kFinalizeMarkingViaTask);
      }
      result = incremental_marking()->IsStopped();
      break;
    }
    case DO_FULL_GC: {
      DCHECK_LT(0, contexts_disposed_);
      HistogramTimerScope scope(isolate_->counters()->gc_context());
      TRACE_EVENT0("v8", "V8.GCContext");
      CollectAllGarbage(kNoGCFlags, GarbageCollectionReason::kContextDisposal);
      break;
    }
    case DO_NOTHING:
      break;
  }

  return result;
}

void Heap::IdleNotificationEpilogue(GCIdleTimeAction action,
                                    GCIdleTimeHeapState heap_state,
                                    double start_ms, double deadline_in_ms) {
  double idle_time_in_ms = deadline_in_ms - start_ms;
  double current_time = MonotonicallyIncreasingTimeInMs();
  last_idle_notification_time_ = current_time;
  double deadline_difference = deadline_in_ms - current_time;

  contexts_disposed_ = 0;

  if ((FLAG_trace_idle_notification && action.type > DO_NOTHING) ||
      FLAG_trace_idle_notification_verbose) {
    isolate_->PrintWithTimestamp(
        "Idle notification: requested idle time %.2f ms, used idle time %.2f "
        "ms, deadline usage %.2f ms [",
        idle_time_in_ms, idle_time_in_ms - deadline_difference,
        deadline_difference);
    action.Print();
    PrintF("]");
    if (FLAG_trace_idle_notification_verbose) {
      PrintF("[");
      heap_state.Print();
      PrintF("]");
    }
    PrintF("\n");
  }
}


double Heap::MonotonicallyIncreasingTimeInMs() {
  return V8::GetCurrentPlatform()->MonotonicallyIncreasingTime() *
         static_cast<double>(base::Time::kMillisecondsPerSecond);
}


bool Heap::IdleNotification(int idle_time_in_ms) {
  return IdleNotification(
      V8::GetCurrentPlatform()->MonotonicallyIncreasingTime() +
      (static_cast<double>(idle_time_in_ms) /
       static_cast<double>(base::Time::kMillisecondsPerSecond)));
}


bool Heap::IdleNotification(double deadline_in_seconds) {
  CHECK(HasBeenSetUp());
  double deadline_in_ms =
      deadline_in_seconds *
      static_cast<double>(base::Time::kMillisecondsPerSecond);
  HistogramTimerScope idle_notification_scope(
      isolate_->counters()->gc_idle_notification());
  TRACE_EVENT0("v8", "V8.GCIdleNotification");
  double start_ms = MonotonicallyIncreasingTimeInMs();
  double idle_time_in_ms = deadline_in_ms - start_ms;

  tracer()->SampleAllocation(start_ms, NewSpaceAllocationCounter(),
                             OldGenerationAllocationCounter());

  GCIdleTimeHeapState heap_state = ComputeHeapState();

  GCIdleTimeAction action =
      gc_idle_time_handler_->Compute(idle_time_in_ms, heap_state);

  bool result = PerformIdleTimeAction(action, heap_state, deadline_in_ms);

  IdleNotificationEpilogue(action, heap_state, start_ms, deadline_in_ms);
  return result;
}


bool Heap::RecentIdleNotificationHappened() {
  return (last_idle_notification_time_ +
          GCIdleTimeHandler::kMaxScheduledIdleTime) >
         MonotonicallyIncreasingTimeInMs();
}

class MemoryPressureInterruptTask : public CancelableTask {
 public:
  explicit MemoryPressureInterruptTask(Heap* heap)
      : CancelableTask(heap->isolate()), heap_(heap) {}

  ~MemoryPressureInterruptTask() override = default;

 private:
  // v8::internal::CancelableTask overrides.
  void RunInternal() override { heap_->CheckMemoryPressure(); }

  Heap* heap_;
  DISALLOW_COPY_AND_ASSIGN(MemoryPressureInterruptTask);
};

void Heap::CheckMemoryPressure() {
  if (HighMemoryPressure()) {
    // The optimizing compiler may be unnecessarily holding on to memory.
    isolate()->AbortConcurrentOptimization(BlockingBehavior::kDontBlock);
  }
  MemoryPressureLevel memory_pressure_level = memory_pressure_level_;
  // Reset the memory pressure level to avoid recursive GCs triggered by
  // CheckMemoryPressure from AdjustAmountOfExternalMemory called by
  // the finalizers.
  memory_pressure_level_ = MemoryPressureLevel::kNone;
  if (memory_pressure_level == MemoryPressureLevel::kCritical) {
    CollectGarbageOnMemoryPressure();
  } else if (memory_pressure_level == MemoryPressureLevel::kModerate) {
    if (FLAG_incremental_marking && incremental_marking()->IsStopped()) {
      StartIncrementalMarking(kReduceMemoryFootprintMask,
                              GarbageCollectionReason::kMemoryPressure);
    }
  }
  if (memory_reducer_) {
    MemoryReducer::Event event;
    event.type = MemoryReducer::kPossibleGarbage;
    event.time_ms = MonotonicallyIncreasingTimeInMs();
    memory_reducer_->NotifyPossibleGarbage(event);
  }
}

void Heap::CollectGarbageOnMemoryPressure() {
  const int kGarbageThresholdInBytes = 8 * MB;
  const double kGarbageThresholdAsFractionOfTotalMemory = 0.1;
  // This constant is the maximum response time in RAIL performance model.
  const double kMaxMemoryPressurePauseMs = 100;

  double start = MonotonicallyIncreasingTimeInMs();
  CollectAllGarbage(kReduceMemoryFootprintMask,
                    GarbageCollectionReason::kMemoryPressure,
                    kGCCallbackFlagCollectAllAvailableGarbage);
  EagerlyFreeExternalMemory();
  double end = MonotonicallyIncreasingTimeInMs();

  // Estimate how much memory we can free.
  int64_t potential_garbage =
      (CommittedMemory() - SizeOfObjects()) + external_memory_;
  // If we can potentially free large amount of memory, then start GC right
  // away instead of waiting for memory reducer.
  if (potential_garbage >= kGarbageThresholdInBytes &&
      potential_garbage >=
          CommittedMemory() * kGarbageThresholdAsFractionOfTotalMemory) {
    // If we spent less than half of the time budget, then perform full GC
    // Otherwise, start incremental marking.
    if (end - start < kMaxMemoryPressurePauseMs / 2) {
      CollectAllGarbage(kReduceMemoryFootprintMask,
                        GarbageCollectionReason::kMemoryPressure,
                        kGCCallbackFlagCollectAllAvailableGarbage);
    } else {
      if (FLAG_incremental_marking && incremental_marking()->IsStopped()) {
        StartIncrementalMarking(kReduceMemoryFootprintMask,
                                GarbageCollectionReason::kMemoryPressure);
      }
    }
  }
}

void Heap::MemoryPressureNotification(MemoryPressureLevel level,
                                      bool is_isolate_locked) {
  MemoryPressureLevel previous = memory_pressure_level_;
  memory_pressure_level_ = level;
  if ((previous != MemoryPressureLevel::kCritical &&
       level == MemoryPressureLevel::kCritical) ||
      (previous == MemoryPressureLevel::kNone &&
       level == MemoryPressureLevel::kModerate)) {
    if (is_isolate_locked) {
      CheckMemoryPressure();
    } else {
      ExecutionAccess access(isolate());
      isolate()->stack_guard()->RequestGC();
      auto taskrunner = V8::GetCurrentPlatform()->GetForegroundTaskRunner(
          reinterpret_cast<v8::Isolate*>(isolate()));
      taskrunner->PostTask(
          base::make_unique<MemoryPressureInterruptTask>(this));
    }
  }
}

void Heap::EagerlyFreeExternalMemory() {
  for (Page* page : *old_space()) {
    if (!page->SweepingDone()) {
      base::LockGuard<base::Mutex> guard(page->mutex());
      if (!page->SweepingDone()) {
        ArrayBufferTracker::FreeDead(
            page, mark_compact_collector()->non_atomic_marking_state());
      }
    }
  }
  memory_allocator()->unmapper()->EnsureUnmappingCompleted();
}

void Heap::AddNearHeapLimitCallback(v8::NearHeapLimitCallback callback,
                                    void* data) {
  const size_t kMaxCallbacks = 100;
  CHECK_LT(near_heap_limit_callbacks_.size(), kMaxCallbacks);
  for (auto callback_data : near_heap_limit_callbacks_) {
    CHECK_NE(callback_data.first, callback);
  }
  near_heap_limit_callbacks_.push_back(std::make_pair(callback, data));
}

void Heap::RemoveNearHeapLimitCallback(v8::NearHeapLimitCallback callback,
                                       size_t heap_limit) {
  for (size_t i = 0; i < near_heap_limit_callbacks_.size(); i++) {
    if (near_heap_limit_callbacks_[i].first == callback) {
      near_heap_limit_callbacks_.erase(near_heap_limit_callbacks_.begin() + i);
      if (heap_limit) {
        RestoreHeapLimit(heap_limit);
      }
      return;
    }
  }
  UNREACHABLE();
}

bool Heap::InvokeNearHeapLimitCallback() {
  if (near_heap_limit_callbacks_.size() > 0) {
    HandleScope scope(isolate());
    v8::NearHeapLimitCallback callback =
        near_heap_limit_callbacks_.back().first;
    void* data = near_heap_limit_callbacks_.back().second;
    size_t heap_limit = callback(data, max_old_generation_size_,
                                 initial_max_old_generation_size_);
    if (heap_limit > max_old_generation_size_) {
      max_old_generation_size_ = heap_limit;
      return true;
    }
  }
  return false;
}

void Heap::CollectCodeStatistics() {
  TRACE_EVENT0("v8", "Heap::CollectCodeStatistics");
  CodeStatistics::ResetCodeAndMetadataStatistics(isolate());
  // We do not look for code in new space, or map space.  If code
  // somehow ends up in those spaces, we would miss it here.
  CodeStatistics::CollectCodeStatistics(code_space_, isolate());
  CodeStatistics::CollectCodeStatistics(old_space_, isolate());
  CodeStatistics::CollectCodeStatistics(lo_space_, isolate());
}

#ifdef DEBUG

void Heap::Print() {
  if (!HasBeenSetUp()) return;
  isolate()->PrintStack(stdout);

  for (SpaceIterator it(this); it.has_next();) {
    it.next()->Print();
  }
}


void Heap::ReportCodeStatistics(const char* title) {
  PrintF(">>>>>> Code Stats (%s) >>>>>>\n", title);
  CollectCodeStatistics();
  CodeStatistics::ReportCodeStatistics(isolate());
}

#endif  // DEBUG

const char* Heap::GarbageCollectionReasonToString(
    GarbageCollectionReason gc_reason) {
  switch (gc_reason) {
    case GarbageCollectionReason::kAllocationFailure:
      return "allocation failure";
    case GarbageCollectionReason::kAllocationLimit:
      return "allocation limit";
    case GarbageCollectionReason::kContextDisposal:
      return "context disposal";
    case GarbageCollectionReason::kCountersExtension:
      return "counters extension";
    case GarbageCollectionReason::kDebugger:
      return "debugger";
    case GarbageCollectionReason::kDeserializer:
      return "deserialize";
    case GarbageCollectionReason::kExternalMemoryPressure:
      return "external memory pressure";
    case GarbageCollectionReason::kFinalizeMarkingViaStackGuard:
      return "finalize incremental marking via stack guard";
    case GarbageCollectionReason::kFinalizeMarkingViaTask:
      return "finalize incremental marking via task";
    case GarbageCollectionReason::kFullHashtable:
      return "full hash-table";
    case GarbageCollectionReason::kHeapProfiler:
      return "heap profiler";
    case GarbageCollectionReason::kIdleTask:
      return "idle task";
    case GarbageCollectionReason::kLastResort:
      return "last resort";
    case GarbageCollectionReason::kLowMemoryNotification:
      return "low memory notification";
    case GarbageCollectionReason::kMakeHeapIterable:
      return "make heap iterable";
    case GarbageCollectionReason::kMemoryPressure:
      return "memory pressure";
    case GarbageCollectionReason::kMemoryReducer:
      return "memory reducer";
    case GarbageCollectionReason::kRuntime:
      return "runtime";
    case GarbageCollectionReason::kSamplingProfiler:
      return "sampling profiler";
    case GarbageCollectionReason::kSnapshotCreator:
      return "snapshot creator";
    case GarbageCollectionReason::kTesting:
      return "testing";
    case GarbageCollectionReason::kExternalFinalize:
      return "external finalize";
    case GarbageCollectionReason::kUnknown:
      return "unknown";
  }
  UNREACHABLE();
}

bool Heap::Contains(HeapObject* value) {
  if (memory_allocator()->IsOutsideAllocatedSpace(value->address())) {
    return false;
  }
  return HasBeenSetUp() &&
         (new_space_->ToSpaceContains(value) || old_space_->Contains(value) ||
          code_space_->Contains(value) || map_space_->Contains(value) ||
          lo_space_->Contains(value) || read_only_space_->Contains(value));
}

bool Heap::ContainsSlow(Address addr) {
  if (memory_allocator()->IsOutsideAllocatedSpace(addr)) {
    return false;
  }
  return HasBeenSetUp() &&
         (new_space_->ToSpaceContainsSlow(addr) ||
          old_space_->ContainsSlow(addr) || code_space_->ContainsSlow(addr) ||
          map_space_->ContainsSlow(addr) || lo_space_->ContainsSlow(addr) ||
          read_only_space_->Contains(addr));
}

bool Heap::InSpace(HeapObject* value, AllocationSpace space) {
  if (memory_allocator()->IsOutsideAllocatedSpace(value->address())) {
    return false;
  }
  if (!HasBeenSetUp()) return false;

  switch (space) {
    case NEW_SPACE:
      return new_space_->ToSpaceContains(value);
    case OLD_SPACE:
      return old_space_->Contains(value);
    case CODE_SPACE:
      return code_space_->Contains(value);
    case MAP_SPACE:
      return map_space_->Contains(value);
    case LO_SPACE:
      return lo_space_->Contains(value);
    case NEW_LO_SPACE:
      return new_lo_space_->Contains(value);
    case RO_SPACE:
      return read_only_space_->Contains(value);
  }
  UNREACHABLE();
}

bool Heap::InSpaceSlow(Address addr, AllocationSpace space) {
  if (memory_allocator()->IsOutsideAllocatedSpace(addr)) {
    return false;
  }
  if (!HasBeenSetUp()) return false;

  switch (space) {
    case NEW_SPACE:
      return new_space_->ToSpaceContainsSlow(addr);
    case OLD_SPACE:
      return old_space_->ContainsSlow(addr);
    case CODE_SPACE:
      return code_space_->ContainsSlow(addr);
    case MAP_SPACE:
      return map_space_->ContainsSlow(addr);
    case LO_SPACE:
      return lo_space_->ContainsSlow(addr);
    case NEW_LO_SPACE:
      return new_lo_space_->ContainsSlow(addr);
    case RO_SPACE:
      return read_only_space_->ContainsSlow(addr);
  }
  UNREACHABLE();
}

bool Heap::IsValidAllocationSpace(AllocationSpace space) {
  switch (space) {
    case NEW_SPACE:
    case OLD_SPACE:
    case CODE_SPACE:
    case MAP_SPACE:
    case LO_SPACE:
    case NEW_LO_SPACE:
    case RO_SPACE:
      return true;
    default:
      return false;
  }
}

bool Heap::RootIsImmortalImmovable(RootIndex root_index) {
  switch (root_index) {
#define IMMORTAL_IMMOVABLE_ROOT(name) case RootIndex::k##name:
    IMMORTAL_IMMOVABLE_ROOT_LIST(IMMORTAL_IMMOVABLE_ROOT)
#undef IMMORTAL_IMMOVABLE_ROOT
#define INTERNALIZED_STRING(_, name, value) case RootIndex::k##name:
    INTERNALIZED_STRING_LIST_GENERATOR(INTERNALIZED_STRING, /* not used */)
#undef INTERNALIZED_STRING
#define STRING_TYPE(NAME, size, name, Name) case RootIndex::k##Name##Map:
    STRING_TYPE_LIST(STRING_TYPE)
#undef STRING_TYPE
    return true;
    default:
      return false;
  }
}

#ifdef VERIFY_HEAP
class VerifyReadOnlyPointersVisitor : public VerifyPointersVisitor {
 public:
  explicit VerifyReadOnlyPointersVisitor(Heap* heap)
      : VerifyPointersVisitor(heap) {}

 protected:
  void VerifyPointers(HeapObject* host, MaybeObject** start,
                      MaybeObject** end) override {
    if (host != nullptr) {
      CHECK(heap_->InReadOnlySpace(host->map()));
    }
    VerifyPointersVisitor::VerifyPointers(host, start, end);

    for (MaybeObject** current = start; current < end; current++) {
      HeapObject* object;
      if ((*current)->GetHeapObject(&object)) {
        CHECK(heap_->InReadOnlySpace(object));
      }
    }
  }
};

void Heap::Verify() {
  CHECK(HasBeenSetUp());
  HandleScope scope(isolate());

  // We have to wait here for the sweeper threads to have an iterable heap.
  mark_compact_collector()->EnsureSweepingCompleted();

  VerifyPointersVisitor visitor(this);
  IterateRoots(&visitor, VISIT_ONLY_STRONG);

  VerifySmisVisitor smis_visitor;
  IterateSmiRoots(&smis_visitor);

  new_space_->Verify(isolate());

  old_space_->Verify(isolate(), &visitor);
  map_space_->Verify(isolate(), &visitor);

  VerifyPointersVisitor no_dirty_regions_visitor(this);
  code_space_->Verify(isolate(), &no_dirty_regions_visitor);

  lo_space_->Verify(isolate());

  VerifyReadOnlyPointersVisitor read_only_visitor(this);
  read_only_space_->Verify(isolate(), &read_only_visitor);
}

class SlotVerifyingVisitor : public ObjectVisitor {
 public:
  SlotVerifyingVisitor(std::set<Address>* untyped,
                       std::set<std::pair<SlotType, Address> >* typed)
      : untyped_(untyped), typed_(typed) {}

  virtual bool ShouldHaveBeenRecorded(HeapObject* host,
                                      MaybeObject* target) = 0;

  void VisitPointers(HeapObject* host, Object** start, Object** end) override {
#ifdef DEBUG
    for (Object** slot = start; slot < end; slot++) {
      DCHECK(!HasWeakHeapObjectTag(*slot));
    }
#endif  // DEBUG
    VisitPointers(host, reinterpret_cast<MaybeObject**>(start),
                  reinterpret_cast<MaybeObject**>(end));
  }

  void VisitPointers(HeapObject* host, MaybeObject** start,
                     MaybeObject** end) final {
    for (MaybeObject** slot = start; slot < end; slot++) {
      if (ShouldHaveBeenRecorded(host, *slot)) {
        CHECK_GT(untyped_->count(reinterpret_cast<Address>(slot)), 0);
      }
    }
  }

  void VisitCodeTarget(Code* host, RelocInfo* rinfo) override {
    Object* target = Code::GetCodeFromTargetAddress(rinfo->target_address());
    if (ShouldHaveBeenRecorded(host, MaybeObject::FromObject(target))) {
      CHECK(
          InTypedSet(CODE_TARGET_SLOT, rinfo->pc()) ||
          (rinfo->IsInConstantPool() &&
           InTypedSet(CODE_ENTRY_SLOT, rinfo->constant_pool_entry_address())));
    }
  }

  void VisitEmbeddedPointer(Code* host, RelocInfo* rinfo) override {
    Object* target = rinfo->target_object();
    if (ShouldHaveBeenRecorded(host, MaybeObject::FromObject(target))) {
      CHECK(InTypedSet(EMBEDDED_OBJECT_SLOT, rinfo->pc()) ||
            (rinfo->IsInConstantPool() &&
             InTypedSet(OBJECT_SLOT, rinfo->constant_pool_entry_address())));
    }
  }

 private:
  bool InTypedSet(SlotType type, Address slot) {
    return typed_->count(std::make_pair(type, slot)) > 0;
  }
  std::set<Address>* untyped_;
  std::set<std::pair<SlotType, Address> >* typed_;
};

class OldToNewSlotVerifyingVisitor : public SlotVerifyingVisitor {
 public:
  OldToNewSlotVerifyingVisitor(std::set<Address>* untyped,
                               std::set<std::pair<SlotType, Address>>* typed)
      : SlotVerifyingVisitor(untyped, typed) {}

  bool ShouldHaveBeenRecorded(HeapObject* host, MaybeObject* target) override {
    DCHECK_IMPLIES(target->IsStrongOrWeak() && Heap::InNewSpace(target),
                   Heap::InToSpace(target));
    return target->IsStrongOrWeak() && Heap::InNewSpace(target) &&
           !Heap::InNewSpace(host);
  }
};

template <RememberedSetType direction>
void CollectSlots(MemoryChunk* chunk, Address start, Address end,
                  std::set<Address>* untyped,
                  std::set<std::pair<SlotType, Address> >* typed) {
  RememberedSet<direction>::Iterate(chunk,
                                    [start, end, untyped](Address slot) {
                                      if (start <= slot && slot < end) {
                                        untyped->insert(slot);
                                      }
                                      return KEEP_SLOT;
                                    },
                                    SlotSet::PREFREE_EMPTY_BUCKETS);
  RememberedSet<direction>::IterateTyped(
      chunk, [start, end, typed](SlotType type, Address host, Address slot) {
        if (start <= slot && slot < end) {
          typed->insert(std::make_pair(type, slot));
        }
        return KEEP_SLOT;
      });
}

void Heap::VerifyRememberedSetFor(HeapObject* object) {
  MemoryChunk* chunk = MemoryChunk::FromAddress(object->address());
  DCHECK_IMPLIES(chunk->mutex() == nullptr, InReadOnlySpace(object));
  // In RO_SPACE chunk->mutex() may be nullptr, so just ignore it.
  base::LockGuard<base::Mutex, base::NullBehavior::kIgnoreIfNull> lock_guard(
      chunk->mutex());
  Address start = object->address();
  Address end = start + object->Size();
  std::set<Address> old_to_new;
  std::set<std::pair<SlotType, Address> > typed_old_to_new;
  if (!InNewSpace(object)) {
    store_buffer()->MoveAllEntriesToRememberedSet();
    CollectSlots<OLD_TO_NEW>(chunk, start, end, &old_to_new, &typed_old_to_new);
    OldToNewSlotVerifyingVisitor visitor(&old_to_new, &typed_old_to_new);
    object->IterateBody(&visitor);
  }
  // TODO(ulan): Add old to old slot set verification once all weak objects
  // have their own instance types and slots are recorded for all weal fields.
}
#endif

#ifdef DEBUG
void Heap::VerifyCountersAfterSweeping() {
  PagedSpaces spaces(this);
  for (PagedSpace* space = spaces.next(); space != nullptr;
       space = spaces.next()) {
    space->VerifyCountersAfterSweeping();
  }
}

void Heap::VerifyCountersBeforeConcurrentSweeping() {
  PagedSpaces spaces(this);
  for (PagedSpace* space = spaces.next(); space != nullptr;
       space = spaces.next()) {
    space->VerifyCountersBeforeConcurrentSweeping();
  }
}
#endif

void Heap::ZapFromSpace() {
  if (!new_space_->IsFromSpaceCommitted()) return;
  for (Page* page : PageRange(new_space_->from_space().first_page(), nullptr)) {
    memory_allocator()->ZapBlock(page->area_start(),
                                 page->HighWaterMark() - page->area_start(),
                                 ZapValue());
  }
}

void Heap::ZapCodeObject(Address start_address, int size_in_bytes) {
#ifdef DEBUG
  for (int i = 0; i < size_in_bytes / kPointerSize; i++) {
    reinterpret_cast<Object**>(start_address)[i] = Smi::FromInt(kCodeZapValue);
  }
#endif
}

Code* Heap::builtin(int index) {
  DCHECK(Builtins::IsBuiltinId(index));
  // Code::cast cannot be used here since we access builtins
  // during the marking phase of mark sweep. See IC::Clear.
  return reinterpret_cast<Code*>(builtins_[index]);
}

Address Heap::builtin_address(int index) {
  DCHECK(Builtins::IsBuiltinId(index) || index == Builtins::builtin_count);
  return reinterpret_cast<Address>(&builtins_[index]);
}

void Heap::set_builtin(int index, HeapObject* builtin) {
  DCHECK(Builtins::IsBuiltinId(index));
  DCHECK(Internals::HasHeapObjectTag(builtin));
  // The given builtin may be completely uninitialized thus we cannot check its
  // type here.
  builtins_[index] = builtin;
}

void Heap::IterateRoots(RootVisitor* v, VisitMode mode) {
  IterateStrongRoots(v, mode);
  IterateWeakRoots(v, mode);
}

void Heap::IterateWeakRoots(RootVisitor* v, VisitMode mode) {
  const bool isMinorGC = mode == VISIT_ALL_IN_SCAVENGE ||
                         mode == VISIT_ALL_IN_MINOR_MC_MARK ||
                         mode == VISIT_ALL_IN_MINOR_MC_UPDATE;
  v->VisitRootPointer(Root::kStringTable, nullptr,
                      &roots_[RootIndex::kStringTable]);
  v->Synchronize(VisitorSynchronization::kStringTable);
  if (!isMinorGC && mode != VISIT_ALL_IN_SWEEP_NEWSPACE &&
      mode != VISIT_FOR_SERIALIZATION) {
    // Scavenge collections have special processing for this.
    // Do not visit for serialization, since the external string table will
    // be populated from scratch upon deserialization.
    external_string_table_.IterateAll(v);
  }
  v->Synchronize(VisitorSynchronization::kExternalStringsTable);
}

void Heap::IterateSmiRoots(RootVisitor* v) {
  // Acquire execution access since we are going to read stack limit values.
  ExecutionAccess access(isolate());
  v->VisitRootPointers(Root::kSmiRootList, nullptr, roots_.smi_roots_begin(),
                       roots_.smi_roots_end());
  v->Synchronize(VisitorSynchronization::kSmiRootList);
}

// We cannot avoid stale handles to left-trimmed objects, but can only make
// sure all handles still needed are updated. Filter out a stale pointer
// and clear the slot to allow post processing of handles (needed because
// the sweeper might actually free the underlying page).
class FixStaleLeftTrimmedHandlesVisitor : public RootVisitor {
 public:
  explicit FixStaleLeftTrimmedHandlesVisitor(Heap* heap) : heap_(heap) {
    USE(heap_);
  }

  void VisitRootPointer(Root root, const char* description,
                        Object** p) override {
    FixHandle(p);
  }

  void VisitRootPointers(Root root, const char* description, Object** start,
                         Object** end) override {
    for (Object** p = start; p < end; p++) FixHandle(p);
  }

 private:
  inline void FixHandle(Object** p) {
    if (!(*p)->IsHeapObject()) return;
    HeapObject* current = reinterpret_cast<HeapObject*>(*p);
    const MapWord map_word = current->map_word();
    if (!map_word.IsForwardingAddress() && current->IsFiller()) {
#ifdef DEBUG
      // We need to find a FixedArrayBase map after walking the fillers.
      while (current->IsFiller()) {
        Address next = reinterpret_cast<Address>(current);
        if (current->map() == ReadOnlyRoots(heap_).one_pointer_filler_map()) {
          next += kPointerSize;
        } else if (current->map() ==
                   ReadOnlyRoots(heap_).two_pointer_filler_map()) {
          next += 2 * kPointerSize;
        } else {
          next += current->Size();
        }
        current = reinterpret_cast<HeapObject*>(next);
      }
      DCHECK(current->IsFixedArrayBase());
#endif  // DEBUG
      *p = nullptr;
    }
  }

  Heap* heap_;
};

void Heap::IterateStrongRoots(RootVisitor* v, VisitMode mode) {
  const bool isMinorGC = mode == VISIT_ALL_IN_SCAVENGE ||
                         mode == VISIT_ALL_IN_MINOR_MC_MARK ||
                         mode == VISIT_ALL_IN_MINOR_MC_UPDATE;
  // Garbage collection can skip over the read-only roots.
  const bool isGC = mode != VISIT_ALL && mode != VISIT_FOR_SERIALIZATION &&
                    mode != VISIT_ONLY_STRONG_FOR_SERIALIZATION;
  Object** start =
      isGC ? roots_.read_only_roots_end() : roots_.strong_roots_begin();
  v->VisitRootPointers(Root::kStrongRootList, nullptr, start,
                       roots_.strong_roots_end());
  v->Synchronize(VisitorSynchronization::kStrongRootList);

  isolate_->bootstrapper()->Iterate(v);
  v->Synchronize(VisitorSynchronization::kBootstrapper);
  isolate_->Iterate(v);
  v->Synchronize(VisitorSynchronization::kTop);
  Relocatable::Iterate(isolate_, v);
  v->Synchronize(VisitorSynchronization::kRelocatable);
  isolate_->debug()->Iterate(v);
  v->Synchronize(VisitorSynchronization::kDebug);

  isolate_->compilation_cache()->Iterate(v);
  v->Synchronize(VisitorSynchronization::kCompilationCache);

  // Iterate over local handles in handle scopes.
  FixStaleLeftTrimmedHandlesVisitor left_trim_visitor(this);
  isolate_->handle_scope_implementer()->Iterate(&left_trim_visitor);
  isolate_->handle_scope_implementer()->Iterate(v);
  isolate_->IterateDeferredHandles(v);
  v->Synchronize(VisitorSynchronization::kHandleScope);

  // Iterate over the builtin code objects and code stubs in the
  // heap. Note that it is not necessary to iterate over code objects
  // on scavenge collections.
  if (!isMinorGC) {
    IterateBuiltins(v);
    v->Synchronize(VisitorSynchronization::kBuiltins);
    isolate_->interpreter()->IterateDispatchTable(v);
    v->Synchronize(VisitorSynchronization::kDispatchTable);
  }

  // Iterate over global handles.
  switch (mode) {
    case VISIT_FOR_SERIALIZATION:
      // Global handles are not iterated by the serializer. Values referenced by
      // global handles need to be added manually.
      break;
    case VISIT_ONLY_STRONG:
    case VISIT_ONLY_STRONG_FOR_SERIALIZATION:
      isolate_->global_handles()->IterateStrongRoots(v);
      break;
    case VISIT_ALL_IN_SCAVENGE:
      isolate_->global_handles()->IterateNewSpaceStrongAndDependentRoots(v);
      break;
    case VISIT_ALL_IN_MINOR_MC_MARK:
      // Global handles are processed manually by the minor MC.
      break;
    case VISIT_ALL_IN_MINOR_MC_UPDATE:
      // Global handles are processed manually by the minor MC.
      break;
    case VISIT_ALL_BUT_READ_ONLY:
    case VISIT_ALL_IN_SWEEP_NEWSPACE:
    case VISIT_ALL:
      isolate_->global_handles()->IterateAllRoots(v);
      break;
  }
  v->Synchronize(VisitorSynchronization::kGlobalHandles);

  // Iterate over eternal handles. Eternal handles are not iterated by the
  // serializer. Values referenced by eternal handles need to be added manually.
  if (mode != VISIT_FOR_SERIALIZATION) {
    if (isMinorGC) {
      isolate_->eternal_handles()->IterateNewSpaceRoots(v);
    } else {
      isolate_->eternal_handles()->IterateAllRoots(v);
    }
  }
  v->Synchronize(VisitorSynchronization::kEternalHandles);

  // Iterate over pointers being held by inactive threads.
  isolate_->thread_manager()->Iterate(v);
  v->Synchronize(VisitorSynchronization::kThreadManager);

  // Iterate over other strong roots (currently only identity maps).
  for (StrongRootsList* list = strong_roots_list_; list; list = list->next) {
    v->VisitRootPointers(Root::kStrongRoots, nullptr, list->start, list->end);
  }
  v->Synchronize(VisitorSynchronization::kStrongRoots);

  // Iterate over the partial snapshot cache unless serializing.
  if (mode != VISIT_FOR_SERIALIZATION) {
    SerializerDeserializer::Iterate(isolate_, v);
    // We don't do a v->Synchronize call here because the serializer and the
    // deserializer are deliberately out of sync here.
  }
}

void Heap::IterateWeakGlobalHandles(RootVisitor* v) {
  isolate_->global_handles()->IterateWeakRoots(v);
}

void Heap::IterateBuiltins(RootVisitor* v) {
  for (int i = 0; i < Builtins::builtin_count; i++) {
    v->VisitRootPointer(Root::kBuiltins, Builtins::name(i), &builtins_[i]);
  }
}

// TODO(1236194): Since the heap size is configurable on the command line
// and through the API, we should gracefully handle the case that the heap
// size is not big enough to fit all the initial objects.
void Heap::ConfigureHeap(size_t max_semi_space_size_in_kb,
                         size_t max_old_generation_size_in_mb,
                         size_t code_range_size_in_mb) {
  // Overwrite default configuration.
  if (max_semi_space_size_in_kb != 0) {
    max_semi_space_size_ =
        RoundUp<Page::kPageSize>(max_semi_space_size_in_kb * KB);
  }
  if (max_old_generation_size_in_mb != 0) {
    max_old_generation_size_ = max_old_generation_size_in_mb * MB;
  }

  // If max space size flags are specified overwrite the configuration.
  if (FLAG_max_semi_space_size > 0) {
    max_semi_space_size_ = static_cast<size_t>(FLAG_max_semi_space_size) * MB;
  }
  if (FLAG_max_old_space_size > 0) {
    max_old_generation_size_ =
        static_cast<size_t>(FLAG_max_old_space_size) * MB;
  }

  if (Page::kPageSize > MB) {
    max_semi_space_size_ = RoundUp<Page::kPageSize>(max_semi_space_size_);
    max_old_generation_size_ =
        RoundUp<Page::kPageSize>(max_old_generation_size_);
  }

  if (FLAG_stress_compaction) {
    // This will cause more frequent GCs when stressing.
    max_semi_space_size_ = MB;
  }

  // The new space size must be a power of two to support single-bit testing
  // for containment.
  max_semi_space_size_ = static_cast<size_t>(base::bits::RoundUpToPowerOfTwo64(
      static_cast<uint64_t>(max_semi_space_size_)));

  if (max_semi_space_size_ == kMaxSemiSpaceSizeInKB * KB) {
    // Start with at least 1*MB semi-space on machines with a lot of memory.
    initial_semispace_size_ =
        Max(initial_semispace_size_, static_cast<size_t>(1 * MB));
  }

  if (FLAG_min_semi_space_size > 0) {
    size_t initial_semispace_size =
        static_cast<size_t>(FLAG_min_semi_space_size) * MB;
    if (initial_semispace_size > max_semi_space_size_) {
      initial_semispace_size_ = max_semi_space_size_;
      if (FLAG_trace_gc) {
        PrintIsolate(isolate_,
                     "Min semi-space size cannot be more than the maximum "
                     "semi-space size of %" PRIuS " MB\n",
                     max_semi_space_size_ / MB);
      }
    } else {
      initial_semispace_size_ =
          RoundUp<Page::kPageSize>(initial_semispace_size);
    }
  }

  initial_semispace_size_ = Min(initial_semispace_size_, max_semi_space_size_);

  if (FLAG_semi_space_growth_factor < 2) {
    FLAG_semi_space_growth_factor = 2;
  }

  // The old generation is paged and needs at least one page for each space.
  int paged_space_count =
      LAST_GROWABLE_PAGED_SPACE - FIRST_GROWABLE_PAGED_SPACE + 1;
  initial_max_old_generation_size_ = max_old_generation_size_ =
      Max(static_cast<size_t>(paged_space_count * Page::kPageSize),
          max_old_generation_size_);

  if (FLAG_initial_old_space_size > 0) {
    initial_old_generation_size_ = FLAG_initial_old_space_size * MB;
  } else {
    initial_old_generation_size_ =
        max_old_generation_size_ / kInitalOldGenerationLimitFactor;
  }
  old_generation_allocation_limit_ = initial_old_generation_size_;

  // We rely on being able to allocate new arrays in paged spaces.
  DCHECK(kMaxRegularHeapObjectSize >=
         (JSArray::kSize +
          FixedArray::SizeFor(JSArray::kInitialMaxFastElementArray) +
          AllocationMemento::kSize));

  code_range_size_ = code_range_size_in_mb * MB;

  configured_ = true;
}


void Heap::AddToRingBuffer(const char* string) {
  size_t first_part =
      Min(strlen(string), kTraceRingBufferSize - ring_buffer_end_);
  memcpy(trace_ring_buffer_ + ring_buffer_end_, string, first_part);
  ring_buffer_end_ += first_part;
  if (first_part < strlen(string)) {
    ring_buffer_full_ = true;
    size_t second_part = strlen(string) - first_part;
    memcpy(trace_ring_buffer_, string + first_part, second_part);
    ring_buffer_end_ = second_part;
  }
}


void Heap::GetFromRingBuffer(char* buffer) {
  size_t copied = 0;
  if (ring_buffer_full_) {
    copied = kTraceRingBufferSize - ring_buffer_end_;
    memcpy(buffer, trace_ring_buffer_ + ring_buffer_end_, copied);
  }
  memcpy(buffer + copied, trace_ring_buffer_, ring_buffer_end_);
}

void Heap::ConfigureHeapDefault() { ConfigureHeap(0, 0, 0); }

void Heap::RecordStats(HeapStats* stats, bool take_snapshot) {
  *stats->start_marker = HeapStats::kStartMarker;
  *stats->end_marker = HeapStats::kEndMarker;
  *stats->ro_space_size = read_only_space_->Size();
  *stats->ro_space_capacity = read_only_space_->Capacity();
  *stats->new_space_size = new_space_->Size();
  *stats->new_space_capacity = new_space_->Capacity();
  *stats->old_space_size = old_space_->SizeOfObjects();
  *stats->old_space_capacity = old_space_->Capacity();
  *stats->code_space_size = code_space_->SizeOfObjects();
  *stats->code_space_capacity = code_space_->Capacity();
  *stats->map_space_size = map_space_->SizeOfObjects();
  *stats->map_space_capacity = map_space_->Capacity();
  *stats->lo_space_size = lo_space_->Size();
  isolate_->global_handles()->RecordStats(stats);
  *stats->memory_allocator_size = memory_allocator()->Size();
  *stats->memory_allocator_capacity =
      memory_allocator()->Size() + memory_allocator()->Available();
  *stats->os_error = base::OS::GetLastError();
  *stats->malloced_memory = isolate_->allocator()->GetCurrentMemoryUsage();
  *stats->malloced_peak_memory = isolate_->allocator()->GetMaxMemoryUsage();
  if (take_snapshot) {
    HeapIterator iterator(this);
    for (HeapObject* obj = iterator.next(); obj != nullptr;
         obj = iterator.next()) {
      InstanceType type = obj->map()->instance_type();
      DCHECK(0 <= type && type <= LAST_TYPE);
      stats->objects_per_type[type]++;
      stats->size_per_type[type] += obj->Size();
    }
  }
  if (stats->last_few_messages != nullptr)
    GetFromRingBuffer(stats->last_few_messages);
  if (stats->js_stacktrace != nullptr) {
    FixedStringAllocator fixed(stats->js_stacktrace, kStacktraceBufferSize - 1);
    StringStream accumulator(&fixed, StringStream::kPrintObjectConcise);
    if (gc_state() == Heap::NOT_IN_GC) {
      isolate()->PrintStack(&accumulator, Isolate::kPrintStackVerbose);
    } else {
      accumulator.Add("Cannot get stack trace in GC.");
    }
  }
}

size_t Heap::OldGenerationSizeOfObjects() {
  PagedSpaces spaces(this, PagedSpaces::SpacesSpecifier::kAllPagedSpaces);
  size_t total = 0;
  for (PagedSpace* space = spaces.next(); space != nullptr;
       space = spaces.next()) {
    total += space->SizeOfObjects();
  }
  return total + lo_space_->SizeOfObjects();
}

uint64_t Heap::PromotedExternalMemorySize() {
  if (external_memory_ <= external_memory_at_last_mark_compact_) return 0;
  return static_cast<uint64_t>(external_memory_ -
                               external_memory_at_last_mark_compact_);
}

bool Heap::ShouldOptimizeForLoadTime() {
  return isolate()->rail_mode() == PERFORMANCE_LOAD &&
         !AllocationLimitOvershotByLargeMargin() &&
         MonotonicallyIncreasingTimeInMs() <
             isolate()->LoadStartTimeMs() + kMaxLoadTimeMs;
}

// This predicate is called when an old generation space cannot allocated from
// the free list and is about to add a new page. Returning false will cause a
// major GC. It happens when the old generation allocation limit is reached and
// - either we need to optimize for memory usage,
// - or the incremental marking is not in progress and we cannot start it.
bool Heap::ShouldExpandOldGenerationOnSlowAllocation() {
  if (always_allocate() || OldGenerationSpaceAvailable() > 0) return true;
  // We reached the old generation allocation limit.

  if (ShouldOptimizeForMemoryUsage()) return false;

  if (ShouldOptimizeForLoadTime()) return true;

  if (incremental_marking()->NeedsFinalization()) {
    return !AllocationLimitOvershotByLargeMargin();
  }

  if (incremental_marking()->IsStopped() &&
      IncrementalMarkingLimitReached() == IncrementalMarkingLimit::kNoLimit) {
    // We cannot start incremental marking.
    return false;
  }
  return true;
}

Heap::HeapGrowingMode Heap::CurrentHeapGrowingMode() {
  if (ShouldReduceMemory() || FLAG_stress_compaction) {
    return Heap::HeapGrowingMode::kMinimal;
  }

  if (ShouldOptimizeForMemoryUsage()) {
    return Heap::HeapGrowingMode::kConservative;
  }

  if (memory_reducer()->ShouldGrowHeapSlowly()) {
    return Heap::HeapGrowingMode::kSlow;
  }

  return Heap::HeapGrowingMode::kDefault;
}

// This function returns either kNoLimit, kSoftLimit, or kHardLimit.
// The kNoLimit means that either incremental marking is disabled or it is too
// early to start incremental marking.
// The kSoftLimit means that incremental marking should be started soon.
// The kHardLimit means that incremental marking should be started immediately.
Heap::IncrementalMarkingLimit Heap::IncrementalMarkingLimitReached() {
  // Code using an AlwaysAllocateScope assumes that the GC state does not
  // change; that implies that no marking steps must be performed.
  if (!incremental_marking()->CanBeActivated() || always_allocate()) {
    // Incremental marking is disabled or it is too early to start.
    return IncrementalMarkingLimit::kNoLimit;
  }
  if (FLAG_stress_incremental_marking) {
    return IncrementalMarkingLimit::kHardLimit;
  }
  if (OldGenerationSizeOfObjects() <=
      IncrementalMarking::kActivationThreshold) {
    // Incremental marking is disabled or it is too early to start.
    return IncrementalMarkingLimit::kNoLimit;
  }
  if ((FLAG_stress_compaction && (gc_count_ & 1) != 0) ||
      HighMemoryPressure()) {
    // If there is high memory pressure or stress testing is enabled, then
    // start marking immediately.
    return IncrementalMarkingLimit::kHardLimit;
  }

  if (FLAG_stress_marking > 0) {
    double gained_since_last_gc =
        PromotedSinceLastGC() +
        (external_memory_ - external_memory_at_last_mark_compact_);
    double size_before_gc =
        OldGenerationObjectsAndPromotedExternalMemorySize() -
        gained_since_last_gc;
    double bytes_to_limit = old_generation_allocation_limit_ - size_before_gc;
    if (bytes_to_limit > 0) {
      double current_percent = (gained_since_last_gc / bytes_to_limit) * 100.0;

      if (FLAG_trace_stress_marking) {
        isolate()->PrintWithTimestamp(
            "[IncrementalMarking] %.2lf%% of the memory limit reached\n",
            current_percent);
      }

      if (FLAG_fuzzer_gc_analysis) {
        // Skips values >=100% since they already trigger marking.
        if (current_percent < 100.0) {
          max_marking_limit_reached_ =
              std::max(max_marking_limit_reached_, current_percent);
        }
      } else if (static_cast<int>(current_percent) >=
                 stress_marking_percentage_) {
        stress_marking_percentage_ = NextStressMarkingLimit();
        return IncrementalMarkingLimit::kHardLimit;
      }
    }
  }

  size_t old_generation_space_available = OldGenerationSpaceAvailable();

  if (old_generation_space_available > new_space_->Capacity()) {
    return IncrementalMarkingLimit::kNoLimit;
  }
  if (ShouldOptimizeForMemoryUsage()) {
    return IncrementalMarkingLimit::kHardLimit;
  }
  if (ShouldOptimizeForLoadTime()) {
    return IncrementalMarkingLimit::kNoLimit;
  }
  if (old_generation_space_available == 0) {
    return IncrementalMarkingLimit::kHardLimit;
  }
  return IncrementalMarkingLimit::kSoftLimit;
}

void Heap::EnableInlineAllocation() {
  if (!inline_allocation_disabled_) return;
  inline_allocation_disabled_ = false;

  // Update inline allocation limit for new space.
  new_space()->UpdateInlineAllocationLimit(0);
}


void Heap::DisableInlineAllocation() {
  if (inline_allocation_disabled_) return;
  inline_allocation_disabled_ = true;

  // Update inline allocation limit for new space.
  new_space()->UpdateInlineAllocationLimit(0);

  // Update inline allocation limit for old spaces.
  PagedSpaces spaces(this);
  CodeSpaceMemoryModificationScope modification_scope(this);
  for (PagedSpace* space = spaces.next(); space != nullptr;
       space = spaces.next()) {
    space->FreeLinearAllocationArea();
  }
}

HeapObject* Heap::EnsureImmovableCode(HeapObject* heap_object,
                                      int object_size) {
  // Code objects which should stay at a fixed address are allocated either
  // in the first page of code space, in large object space, or (during
  // snapshot creation) the containing page is marked as immovable.
  DCHECK(heap_object);
  DCHECK(code_space_->Contains(heap_object));
  DCHECK_GE(object_size, 0);
  if (!Heap::IsImmovable(heap_object)) {
    if (isolate()->serializer_enabled() ||
        code_space_->first_page()->Contains(heap_object->address())) {
      MemoryChunk::FromAddress(heap_object->address())->MarkNeverEvacuate();
    } else {
      // Discard the first code allocation, which was on a page where it could
      // be moved.
      CreateFillerObjectAt(heap_object->address(), object_size,
                           ClearRecordedSlots::kNo);
      heap_object = AllocateRawCodeInLargeObjectSpace(object_size);
      UnprotectAndRegisterMemoryChunk(heap_object);
      ZapCodeObject(heap_object->address(), object_size);
      OnAllocationEvent(heap_object, object_size);
    }
  }
  return heap_object;
}

HeapObject* Heap::AllocateRawWithLightRetry(int size, AllocationSpace space,
                                            AllocationAlignment alignment) {
  HeapObject* result;
  AllocationResult alloc = AllocateRaw(size, space, alignment);
  if (alloc.To(&result)) {
    DCHECK(result != ReadOnlyRoots(this).exception());
    return result;
  }
  // Two GCs before panicking. In newspace will almost always succeed.
  for (int i = 0; i < 2; i++) {
    CollectGarbage(alloc.RetrySpace(),
                   GarbageCollectionReason::kAllocationFailure);
    alloc = AllocateRaw(size, space, alignment);
    if (alloc.To(&result)) {
      DCHECK(result != ReadOnlyRoots(this).exception());
      return result;
    }
  }
  return nullptr;
}

HeapObject* Heap::AllocateRawWithRetryOrFail(int size, AllocationSpace space,
                                             AllocationAlignment alignment) {
  AllocationResult alloc;
  HeapObject* result = AllocateRawWithLightRetry(size, space, alignment);
  if (result) return result;

  isolate()->counters()->gc_last_resort_from_handles()->Increment();
  CollectAllAvailableGarbage(GarbageCollectionReason::kLastResort);
  {
    AlwaysAllocateScope scope(isolate());
    alloc = AllocateRaw(size, space, alignment);
  }
  if (alloc.To(&result)) {
    DCHECK(result != ReadOnlyRoots(this).exception());
    return result;
  }
  // TODO(1181417): Fix this.
  FatalProcessOutOfMemory("CALL_AND_RETRY_LAST");
  return nullptr;
}

// TODO(jkummerow): Refactor this. AllocateRaw should take an "immovability"
// parameter and just do what's necessary.
HeapObject* Heap::AllocateRawCodeInLargeObjectSpace(int size) {
  AllocationResult alloc = lo_space()->AllocateRaw(size, EXECUTABLE);
  HeapObject* result;
  if (alloc.To(&result)) {
    DCHECK(result != ReadOnlyRoots(this).exception());
    return result;
  }
  // Two GCs before panicking.
  for (int i = 0; i < 2; i++) {
    CollectGarbage(alloc.RetrySpace(),
                   GarbageCollectionReason::kAllocationFailure);
    alloc = lo_space()->AllocateRaw(size, EXECUTABLE);
    if (alloc.To(&result)) {
      DCHECK(result != ReadOnlyRoots(this).exception());
      return result;
    }
  }
  isolate()->counters()->gc_last_resort_from_handles()->Increment();
  CollectAllAvailableGarbage(GarbageCollectionReason::kLastResort);
  {
    AlwaysAllocateScope scope(isolate());
    alloc = lo_space()->AllocateRaw(size, EXECUTABLE);
  }
  if (alloc.To(&result)) {
    DCHECK(result != ReadOnlyRoots(this).exception());
    return result;
  }
  // TODO(1181417): Fix this.
  FatalProcessOutOfMemory("CALL_AND_RETRY_LAST");
  return nullptr;
}

void Heap::SetUp() {
#ifdef V8_ENABLE_ALLOCATION_TIMEOUT
  allocation_timeout_ = NextAllocationTimeout();
#endif

  // Initialize heap spaces and initial maps and objects.
  //
  // If the heap is not yet configured (e.g. through the API), configure it.
  // Configuration is based on the flags new-space-size (really the semispace
  // size) and old-space-size if set or the initial values of semispace_size_
  // and old_generation_size_ otherwise.
  if (!configured_) ConfigureHeapDefault();

  mmap_region_base_ =
      reinterpret_cast<uintptr_t>(v8::internal::GetRandomMmapAddr()) &
      ~kMmapRegionMask;

  // Set up memory allocator.
  memory_allocator_ =
      new MemoryAllocator(isolate_, MaxReserved(), code_range_size_);

  store_buffer_ = new StoreBuffer(this);

  heap_controller_ = new HeapController(this);

  mark_compact_collector_ = new MarkCompactCollector(this);

  scavenger_collector_ = new ScavengerCollector(this);

  incremental_marking_ =
      new IncrementalMarking(this, mark_compact_collector_->marking_worklist(),
                             mark_compact_collector_->weak_objects());

  if (FLAG_concurrent_marking) {
    MarkCompactCollector::MarkingWorklist* marking_worklist =
        mark_compact_collector_->marking_worklist();
    concurrent_marking_ = new ConcurrentMarking(
        this, marking_worklist->shared(), marking_worklist->bailout(),
        marking_worklist->on_hold(), mark_compact_collector_->weak_objects(),
        marking_worklist->embedder());
  } else {
    concurrent_marking_ = new ConcurrentMarking(this, nullptr, nullptr, nullptr,
                                                nullptr, nullptr);
  }

  for (int i = FIRST_SPACE; i <= LAST_SPACE; i++) {
    space_[i] = nullptr;
  }

  space_[RO_SPACE] = read_only_space_ = new ReadOnlySpace(this);
  space_[NEW_SPACE] = new_space_ =
      new NewSpace(this, memory_allocator_->data_page_allocator(),
                   initial_semispace_size_, max_semi_space_size_);
  space_[OLD_SPACE] = old_space_ = new OldSpace(this);
  space_[CODE_SPACE] = code_space_ = new CodeSpace(this);
  space_[MAP_SPACE] = map_space_ = new MapSpace(this);
  space_[LO_SPACE] = lo_space_ = new LargeObjectSpace(this);
  space_[NEW_LO_SPACE] = new_lo_space_ = new NewLargeObjectSpace(this);

  for (int i = 0; i < static_cast<int>(v8::Isolate::kUseCounterFeatureCount);
       i++) {
    deferred_counters_[i] = 0;
  }

  tracer_ = new GCTracer(this);
#ifdef ENABLE_MINOR_MC
  minor_mark_compact_collector_ = new MinorMarkCompactCollector(this);
#else
  minor_mark_compact_collector_ = nullptr;
#endif  // ENABLE_MINOR_MC
  array_buffer_collector_ = new ArrayBufferCollector(this);
  gc_idle_time_handler_ = new GCIdleTimeHandler();
  memory_reducer_ = new MemoryReducer(this);
  if (V8_UNLIKELY(FLAG_gc_stats)) {
    live_object_stats_ = new ObjectStats(this);
    dead_object_stats_ = new ObjectStats(this);
  }
  scavenge_job_ = new ScavengeJob();
  local_embedder_heap_tracer_ = new LocalEmbedderHeapTracer(isolate());

  LOG(isolate_, IntPtrTEvent("heap-capacity", Capacity()));
  LOG(isolate_, IntPtrTEvent("heap-available", Available()));

  store_buffer()->SetUp();

  mark_compact_collector()->SetUp();
#ifdef ENABLE_MINOR_MC
  if (minor_mark_compact_collector() != nullptr) {
    minor_mark_compact_collector()->SetUp();
  }
#endif  // ENABLE_MINOR_MC

  idle_scavenge_observer_ = new IdleScavengeObserver(
      *this, ScavengeJob::kBytesAllocatedBeforeNextIdleTask);
  new_space()->AddAllocationObserver(idle_scavenge_observer_);

  SetGetExternallyAllocatedMemoryInBytesCallback(
      DefaultGetExternallyAllocatedMemoryInBytesCallback);

  if (FLAG_stress_marking > 0) {
    stress_marking_percentage_ = NextStressMarkingLimit();
    stress_marking_observer_ = new StressMarkingObserver(*this);
    AddAllocationObserversToAllSpaces(stress_marking_observer_,
                                      stress_marking_observer_);
  }
  if (FLAG_stress_scavenge > 0) {
    stress_scavenge_observer_ = new StressScavengeObserver(*this);
    new_space()->AddAllocationObserver(stress_scavenge_observer_);
  }

  write_protect_code_memory_ = FLAG_write_protect_code_memory;

  external_reference_table_.Init(isolate_);
}

void Heap::InitializeHashSeed() {
  uint64_t new_hash_seed;
  if (FLAG_hash_seed == 0) {
    int64_t rnd = isolate()->random_number_generator()->NextInt64();
    new_hash_seed = static_cast<uint64_t>(rnd);
  } else {
    new_hash_seed = static_cast<uint64_t>(FLAG_hash_seed);
  }
  hash_seed()->copy_in(0, reinterpret_cast<byte*>(&new_hash_seed), kInt64Size);
}

void Heap::SetStackLimits() {
  DCHECK_NOT_NULL(isolate_);
  DCHECK(isolate_ == isolate());
  // On 64 bit machines, pointers are generally out of range of Smis.  We write
  // something that looks like an out of range Smi to the GC.

  // Set up the special root array entries containing the stack limits.
  // These are actually addresses, but the tag makes the GC ignore it.
  roots_[RootIndex::kStackLimit] = reinterpret_cast<Object*>(
      (isolate_->stack_guard()->jslimit() & ~kSmiTagMask) | kSmiTag);
  roots_[RootIndex::kRealStackLimit] = reinterpret_cast<Object*>(
      (isolate_->stack_guard()->real_jslimit() & ~kSmiTagMask) | kSmiTag);
}

void Heap::ClearStackLimits() {
  roots_[RootIndex::kStackLimit] = Smi::kZero;
  roots_[RootIndex::kRealStackLimit] = Smi::kZero;
}

int Heap::NextAllocationTimeout(int current_timeout) {
  if (FLAG_random_gc_interval > 0) {
    // If current timeout hasn't reached 0 the GC was caused by something
    // different than --stress-atomic-gc flag and we don't update the timeout.
    if (current_timeout <= 0) {
      return isolate()->fuzzer_rng()->NextInt(FLAG_random_gc_interval + 1);
    } else {
      return current_timeout;
    }
  }
  return FLAG_gc_interval;
}

void Heap::PrintAllocationsHash() {
  uint32_t hash = StringHasher::GetHashCore(raw_allocations_hash_);
  PrintF("\n### Allocations = %u, hash = 0x%08x\n", allocations_count(), hash);
}

void Heap::PrintMaxMarkingLimitReached() {
  PrintF("\n### Maximum marking limit reached = %.02lf\n",
         max_marking_limit_reached_);
}

void Heap::PrintMaxNewSpaceSizeReached() {
  PrintF("\n### Maximum new space size reached = %.02lf\n",
         stress_scavenge_observer_->MaxNewSpaceSizeReached());
}

int Heap::NextStressMarkingLimit() {
  return isolate()->fuzzer_rng()->NextInt(FLAG_stress_marking + 1);
}

void Heap::NotifyDeserializationComplete() {
  PagedSpaces spaces(this);
  for (PagedSpace* s = spaces.next(); s != nullptr; s = spaces.next()) {
    if (isolate()->snapshot_available()) s->ShrinkImmortalImmovablePages();
#ifdef DEBUG
    // All pages right after bootstrapping must be marked as never-evacuate.
    for (Page* p : *s) {
      DCHECK(p->NeverEvacuate());
    }
#endif  // DEBUG
  }

  read_only_space()->MarkAsReadOnly();
  deserialization_complete_ = true;
}

void Heap::SetEmbedderHeapTracer(EmbedderHeapTracer* tracer) {
  DCHECK_EQ(gc_state_, HeapState::NOT_IN_GC);
  local_embedder_heap_tracer()->SetRemoteTracer(tracer);
}

EmbedderHeapTracer* Heap::GetEmbedderHeapTracer() const {
  return local_embedder_heap_tracer()->remote_tracer();
}

void Heap::TracePossibleWrapper(JSObject* js_object) {
  DCHECK(js_object->IsApiWrapper());
  if (js_object->GetEmbedderFieldCount() >= 2 &&
      js_object->GetEmbedderField(0) &&
      js_object->GetEmbedderField(0) != ReadOnlyRoots(this).undefined_value() &&
      js_object->GetEmbedderField(1) != ReadOnlyRoots(this).undefined_value()) {
    DCHECK_EQ(0,
              reinterpret_cast<intptr_t>(js_object->GetEmbedderField(0)) % 2);
    local_embedder_heap_tracer()->AddWrapperToTrace(std::pair<void*, void*>(
        reinterpret_cast<void*>(js_object->GetEmbedderField(0)),
        reinterpret_cast<void*>(js_object->GetEmbedderField(1))));
  }
}

void Heap::RegisterExternallyReferencedObject(Object** object) {
  // The embedder is not aware of whether numbers are materialized as heap
  // objects are just passed around as Smis.
  if (!(*object)->IsHeapObject()) return;
  HeapObject* heap_object = HeapObject::cast(*object);
  DCHECK(Contains(heap_object));
  if (FLAG_incremental_marking_wrappers && incremental_marking()->IsMarking()) {
    incremental_marking()->WhiteToGreyAndPush(heap_object);
  } else {
    DCHECK(mark_compact_collector()->in_use());
    mark_compact_collector()->MarkExternallyReferencedObject(heap_object);
  }
}

void Heap::StartTearDown() { SetGCState(TEAR_DOWN); }

void Heap::TearDown() {
  DCHECK_EQ(gc_state_, TEAR_DOWN);
#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) {
    Verify();
  }
#endif

  UpdateMaximumCommitted();

  if (FLAG_verify_predictable || FLAG_fuzzer_gc_analysis) {
    PrintAllocationsHash();
  }

  if (FLAG_fuzzer_gc_analysis) {
    if (FLAG_stress_marking > 0) {
      PrintMaxMarkingLimitReached();
    }
    if (FLAG_stress_scavenge > 0) {
      PrintMaxNewSpaceSizeReached();
    }
  }

  new_space()->RemoveAllocationObserver(idle_scavenge_observer_);
  delete idle_scavenge_observer_;
  idle_scavenge_observer_ = nullptr;

  if (FLAG_stress_marking > 0) {
    RemoveAllocationObserversFromAllSpaces(stress_marking_observer_,
                                           stress_marking_observer_);
    delete stress_marking_observer_;
    stress_marking_observer_ = nullptr;
  }
  if (FLAG_stress_scavenge > 0) {
    new_space()->RemoveAllocationObserver(stress_scavenge_observer_);
    delete stress_scavenge_observer_;
    stress_scavenge_observer_ = nullptr;
  }

  if (heap_controller_ != nullptr) {
    delete heap_controller_;
    heap_controller_ = nullptr;
  }

  if (mark_compact_collector_ != nullptr) {
    mark_compact_collector_->TearDown();
    delete mark_compact_collector_;
    mark_compact_collector_ = nullptr;
  }

#ifdef ENABLE_MINOR_MC
  if (minor_mark_compact_collector_ != nullptr) {
    minor_mark_compact_collector_->TearDown();
    delete minor_mark_compact_collector_;
    minor_mark_compact_collector_ = nullptr;
  }
#endif  // ENABLE_MINOR_MC

  if (scavenger_collector_ != nullptr) {
    delete scavenger_collector_;
    scavenger_collector_ = nullptr;
  }

  if (array_buffer_collector_ != nullptr) {
    delete array_buffer_collector_;
    array_buffer_collector_ = nullptr;
  }

  delete incremental_marking_;
  incremental_marking_ = nullptr;

  delete concurrent_marking_;
  concurrent_marking_ = nullptr;

  delete gc_idle_time_handler_;
  gc_idle_time_handler_ = nullptr;

  if (memory_reducer_ != nullptr) {
    memory_reducer_->TearDown();
    delete memory_reducer_;
    memory_reducer_ = nullptr;
  }

  if (live_object_stats_ != nullptr) {
    delete live_object_stats_;
    live_object_stats_ = nullptr;
  }

  if (dead_object_stats_ != nullptr) {
    delete dead_object_stats_;
    dead_object_stats_ = nullptr;
  }

  delete local_embedder_heap_tracer_;
  local_embedder_heap_tracer_ = nullptr;

  delete scavenge_job_;
  scavenge_job_ = nullptr;

  isolate_->global_handles()->TearDown();

  external_string_table_.TearDown();

  // Tear down all ArrayBuffers before tearing down the heap since  their
  // byte_length may be a HeapNumber which is required for freeing the backing
  // store.
  ArrayBufferTracker::TearDown(this);

  delete tracer_;
  tracer_ = nullptr;

  for (int i = FIRST_SPACE; i <= LAST_SPACE; i++) {
    delete space_[i];
    space_[i] = nullptr;
  }

  store_buffer()->TearDown();

  memory_allocator()->TearDown();

  StrongRootsList* next = nullptr;
  for (StrongRootsList* list = strong_roots_list_; list; list = next) {
    next = list->next;
    delete list;
  }
  strong_roots_list_ = nullptr;

  delete store_buffer_;
  store_buffer_ = nullptr;

  delete memory_allocator_;
  memory_allocator_ = nullptr;
}

void Heap::AddGCPrologueCallback(v8::Isolate::GCCallbackWithData callback,
                                 GCType gc_type, void* data) {
  DCHECK_NOT_NULL(callback);
  DCHECK(gc_prologue_callbacks_.end() ==
         std::find(gc_prologue_callbacks_.begin(), gc_prologue_callbacks_.end(),
                   GCCallbackTuple(callback, gc_type, data)));
  gc_prologue_callbacks_.emplace_back(callback, gc_type, data);
}

void Heap::RemoveGCPrologueCallback(v8::Isolate::GCCallbackWithData callback,
                                    void* data) {
  DCHECK_NOT_NULL(callback);
  for (size_t i = 0; i < gc_prologue_callbacks_.size(); i++) {
    if (gc_prologue_callbacks_[i].callback == callback &&
        gc_prologue_callbacks_[i].data == data) {
      gc_prologue_callbacks_[i] = gc_prologue_callbacks_.back();
      gc_prologue_callbacks_.pop_back();
      return;
    }
  }
  UNREACHABLE();
}

void Heap::AddGCEpilogueCallback(v8::Isolate::GCCallbackWithData callback,
                                 GCType gc_type, void* data) {
  DCHECK_NOT_NULL(callback);
  DCHECK(gc_epilogue_callbacks_.end() ==
         std::find(gc_epilogue_callbacks_.begin(), gc_epilogue_callbacks_.end(),
                   GCCallbackTuple(callback, gc_type, data)));
  gc_epilogue_callbacks_.emplace_back(callback, gc_type, data);
}

void Heap::RemoveGCEpilogueCallback(v8::Isolate::GCCallbackWithData callback,
                                    void* data) {
  DCHECK_NOT_NULL(callback);
  for (size_t i = 0; i < gc_epilogue_callbacks_.size(); i++) {
    if (gc_epilogue_callbacks_[i].callback == callback &&
        gc_epilogue_callbacks_[i].data == data) {
      gc_epilogue_callbacks_[i] = gc_epilogue_callbacks_.back();
      gc_epilogue_callbacks_.pop_back();
      return;
    }
  }
  UNREACHABLE();
}

namespace {
Handle<WeakArrayList> CompactWeakArrayList(Heap* heap,
                                           Handle<WeakArrayList> array,
                                           PretenureFlag pretenure) {
  if (array->length() == 0) {
    return array;
  }
  int new_length = array->CountLiveWeakReferences();
  if (new_length == array->length()) {
    return array;
  }

  Handle<WeakArrayList> new_array = WeakArrayList::EnsureSpace(
      heap->isolate(),
      handle(ReadOnlyRoots(heap).empty_weak_array_list(), heap->isolate()),
      new_length, pretenure);
  // Allocation might have caused GC and turned some of the elements into
  // cleared weak heap objects. Count the number of live references again and
  // fill in the new array.
  int copy_to = 0;
  for (int i = 0; i < array->length(); i++) {
    MaybeObject* element = array->Get(i);
    if (element->IsCleared()) continue;
    new_array->Set(copy_to++, element);
  }
  new_array->set_length(copy_to);
  return new_array;
}

}  // anonymous namespace

void Heap::CompactWeakArrayLists(PretenureFlag pretenure) {
  // Find known PrototypeUsers and compact them.
  std::vector<Handle<PrototypeInfo>> prototype_infos;
  {
    HeapIterator iterator(this);
    for (HeapObject* o = iterator.next(); o != nullptr; o = iterator.next()) {
      if (o->IsPrototypeInfo()) {
        PrototypeInfo* prototype_info = PrototypeInfo::cast(o);
        if (prototype_info->prototype_users()->IsWeakArrayList()) {
          prototype_infos.emplace_back(handle(prototype_info, isolate()));
        }
      }
    }
  }
  for (auto& prototype_info : prototype_infos) {
    Handle<WeakArrayList> array(
        WeakArrayList::cast(prototype_info->prototype_users()), isolate());
    DCHECK_IMPLIES(pretenure == TENURED,
                   InOldSpace(*array) ||
                       *array == ReadOnlyRoots(this).empty_weak_array_list());
    WeakArrayList* new_array = PrototypeUsers::Compact(
        array, this, JSObject::PrototypeRegistryCompactionCallback, pretenure);
    prototype_info->set_prototype_users(new_array);
  }

  // Find known WeakArrayLists and compact them.
  Handle<WeakArrayList> scripts(script_list(), isolate());
  DCHECK_IMPLIES(pretenure == TENURED, InOldSpace(*scripts));
  scripts = CompactWeakArrayList(this, scripts, pretenure);
  set_script_list(*scripts);

  Handle<WeakArrayList> no_script_list(noscript_shared_function_infos(),
                                       isolate());
  DCHECK_IMPLIES(pretenure == TENURED, InOldSpace(*no_script_list));
  no_script_list = CompactWeakArrayList(this, no_script_list, pretenure);
  set_noscript_shared_function_infos(*no_script_list);
}

void Heap::AddRetainedMap(Handle<Map> map) {
  if (map->is_in_retained_map_list()) {
    return;
  }
  Handle<WeakArrayList> array(retained_maps(), isolate());
  if (array->IsFull()) {
    CompactRetainedMaps(*array);
  }
  array =
      WeakArrayList::AddToEnd(isolate(), array, MaybeObjectHandle::Weak(map));
  array = WeakArrayList::AddToEnd(
      isolate(), array,
      MaybeObjectHandle(Smi::FromInt(FLAG_retain_maps_for_n_gc), isolate()));
  if (*array != retained_maps()) {
    set_retained_maps(*array);
  }
  map->set_is_in_retained_map_list(true);
}

void Heap::CompactRetainedMaps(WeakArrayList* retained_maps) {
  DCHECK_EQ(retained_maps, this->retained_maps());
  int length = retained_maps->length();
  int new_length = 0;
  int new_number_of_disposed_maps = 0;
  // This loop compacts the array by removing cleared weak cells.
  for (int i = 0; i < length; i += 2) {
    MaybeObject* maybe_object = retained_maps->Get(i);
    if (maybe_object->IsCleared()) {
      continue;
    }

    DCHECK(maybe_object->IsWeak());

    MaybeObject* age = retained_maps->Get(i + 1);
    DCHECK(age->IsSmi());
    if (i != new_length) {
      retained_maps->Set(new_length, maybe_object);
      retained_maps->Set(new_length + 1, age);
    }
    if (i < number_of_disposed_maps_) {
      new_number_of_disposed_maps += 2;
    }
    new_length += 2;
  }
  number_of_disposed_maps_ = new_number_of_disposed_maps;
  HeapObject* undefined = ReadOnlyRoots(this).undefined_value();
  for (int i = new_length; i < length; i++) {
    retained_maps->Set(i, HeapObjectReference::Strong(undefined));
  }
  if (new_length != length) retained_maps->set_length(new_length);
}

void Heap::FatalProcessOutOfMemory(const char* location) {
  v8::internal::V8::FatalProcessOutOfMemory(isolate(), location, true);
}

#ifdef DEBUG

class PrintHandleVisitor : public RootVisitor {
 public:
  void VisitRootPointers(Root root, const char* description, Object** start,
                         Object** end) override {
    for (Object** p = start; p < end; p++)
      PrintF("  handle %p to %p\n", reinterpret_cast<void*>(p),
             reinterpret_cast<void*>(*p));
  }
};


void Heap::PrintHandles() {
  PrintF("Handles:\n");
  PrintHandleVisitor v;
  isolate_->handle_scope_implementer()->Iterate(&v);
}

#endif

class CheckHandleCountVisitor : public RootVisitor {
 public:
  CheckHandleCountVisitor() : handle_count_(0) {}
  ~CheckHandleCountVisitor() override {
    CHECK_GT(HandleScope::kCheckHandleThreshold, handle_count_);
  }
  void VisitRootPointers(Root root, const char* description, Object** start,
                         Object** end) override {
    handle_count_ += end - start;
  }

 private:
  ptrdiff_t handle_count_;
};


void Heap::CheckHandleCount() {
  CheckHandleCountVisitor v;
  isolate_->handle_scope_implementer()->Iterate(&v);
}

Address* Heap::store_buffer_top_address() {
  return store_buffer()->top_address();
}

// static
intptr_t Heap::store_buffer_mask_constant() {
  return StoreBuffer::kStoreBufferMask;
}

// static
Address Heap::store_buffer_overflow_function_address() {
  return FUNCTION_ADDR(StoreBuffer::StoreBufferOverflow);
}

void Heap::ClearRecordedSlot(HeapObject* object, Object** slot) {
  Address slot_addr = reinterpret_cast<Address>(slot);
  Page* page = Page::FromAddress(slot_addr);
  if (!page->InNewSpace()) {
    DCHECK_EQ(page->owner()->identity(), OLD_SPACE);
    store_buffer()->DeleteEntry(slot_addr);
  }
}

#ifdef DEBUG
void Heap::VerifyClearedSlot(HeapObject* object, Object** slot) {
  if (InNewSpace(object)) return;
  Address slot_addr = reinterpret_cast<Address>(slot);
  Page* page = Page::FromAddress(slot_addr);
  DCHECK_EQ(page->owner()->identity(), OLD_SPACE);
  store_buffer()->MoveAllEntriesToRememberedSet();
  CHECK(!RememberedSet<OLD_TO_NEW>::Contains(page, slot_addr));
  // Old to old slots are filtered with invalidated slots.
  CHECK_IMPLIES(RememberedSet<OLD_TO_OLD>::Contains(page, slot_addr),
                page->RegisteredObjectWithInvalidatedSlots(object));
}
#endif

void Heap::ClearRecordedSlotRange(Address start, Address end) {
  Page* page = Page::FromAddress(start);
  if (!page->InNewSpace()) {
    DCHECK_EQ(page->owner()->identity(), OLD_SPACE);
    store_buffer()->DeleteEntry(start, end);
  }
}

PagedSpace* PagedSpaces::next() {
  switch (counter_++) {
    case RO_SPACE:
      // skip NEW_SPACE
      counter_++;
      return heap_->read_only_space();
    case OLD_SPACE:
      return heap_->old_space();
    case CODE_SPACE:
      return heap_->code_space();
    case MAP_SPACE:
      return heap_->map_space();
    default:
      return nullptr;
  }
}

SpaceIterator::SpaceIterator(Heap* heap)
    : heap_(heap), current_space_(FIRST_SPACE - 1) {}

SpaceIterator::~SpaceIterator() = default;

bool SpaceIterator::has_next() {
  // Iterate until no more spaces.
  return current_space_ != LAST_SPACE;
}

Space* SpaceIterator::next() {
  DCHECK(has_next());
  return heap_->space(++current_space_);
}


class HeapObjectsFilter {
 public:
  virtual ~HeapObjectsFilter() = default;
  virtual bool SkipObject(HeapObject* object) = 0;
};


class UnreachableObjectsFilter : public HeapObjectsFilter {
 public:
  explicit UnreachableObjectsFilter(Heap* heap) : heap_(heap) {
    MarkReachableObjects();
  }

  ~UnreachableObjectsFilter() override {
    for (auto it : reachable_) {
      delete it.second;
      it.second = nullptr;
    }
  }

  bool SkipObject(HeapObject* object) override {
    if (object->IsFiller()) return true;
    MemoryChunk* chunk = MemoryChunk::FromAddress(object->address());
    if (reachable_.count(chunk) == 0) return true;
    return reachable_[chunk]->count(object) == 0;
  }

 private:
  bool MarkAsReachable(HeapObject* object) {
    MemoryChunk* chunk = MemoryChunk::FromAddress(object->address());
    if (reachable_.count(chunk) == 0) {
      reachable_[chunk] = new std::unordered_set<HeapObject*>();
    }
    if (reachable_[chunk]->count(object)) return false;
    reachable_[chunk]->insert(object);
    return true;
  }

  class MarkingVisitor : public ObjectVisitor, public RootVisitor {
   public:
    explicit MarkingVisitor(UnreachableObjectsFilter* filter)
        : filter_(filter) {}

    void VisitPointers(HeapObject* host, Object** start,
                       Object** end) override {
      MarkPointers(reinterpret_cast<MaybeObject**>(start),
                   reinterpret_cast<MaybeObject**>(end));
    }

    void VisitPointers(HeapObject* host, MaybeObject** start,
                       MaybeObject** end) final {
      MarkPointers(start, end);
    }

    void VisitRootPointers(Root root, const char* description, Object** start,
                           Object** end) override {
      MarkPointers(reinterpret_cast<MaybeObject**>(start),
                   reinterpret_cast<MaybeObject**>(end));
    }

    void TransitiveClosure() {
      while (!marking_stack_.empty()) {
        HeapObject* obj = marking_stack_.back();
        marking_stack_.pop_back();
        obj->Iterate(this);
      }
    }

   private:
    void MarkPointers(MaybeObject** start, MaybeObject** end) {
      // Treat weak references as strong.
      for (MaybeObject** p = start; p < end; p++) {
        HeapObject* heap_object;
        if ((*p)->GetHeapObject(&heap_object)) {
          if (filter_->MarkAsReachable(heap_object)) {
            marking_stack_.push_back(heap_object);
          }
        }
      }
    }
    UnreachableObjectsFilter* filter_;
    std::vector<HeapObject*> marking_stack_;
  };

  friend class MarkingVisitor;

  void MarkReachableObjects() {
    MarkingVisitor visitor(this);
    heap_->IterateRoots(&visitor, VISIT_ALL_BUT_READ_ONLY);
    visitor.TransitiveClosure();
  }

  Heap* heap_;
  DisallowHeapAllocation no_allocation_;
  std::unordered_map<MemoryChunk*, std::unordered_set<HeapObject*>*> reachable_;
};

HeapIterator::HeapIterator(Heap* heap,
                           HeapIterator::HeapObjectsFiltering filtering)
    : no_heap_allocation_(),
      heap_(heap),
      filtering_(filtering),
      filter_(nullptr),
      space_iterator_(nullptr),
      object_iterator_(nullptr) {
  heap_->MakeHeapIterable();
  heap_->heap_iterator_start();
  // Start the iteration.
  space_iterator_ = new SpaceIterator(heap_);
  switch (filtering_) {
    case kFilterUnreachable:
      filter_ = new UnreachableObjectsFilter(heap_);
      break;
    default:
      break;
  }
  object_iterator_ = space_iterator_->next()->GetObjectIterator();
}


HeapIterator::~HeapIterator() {
  heap_->heap_iterator_end();
#ifdef DEBUG
  // Assert that in filtering mode we have iterated through all
  // objects. Otherwise, heap will be left in an inconsistent state.
  if (filtering_ != kNoFiltering) {
    DCHECK_NULL(object_iterator_);
  }
#endif
  delete space_iterator_;
  delete filter_;
}


HeapObject* HeapIterator::next() {
  if (filter_ == nullptr) return NextObject();

  HeapObject* obj = NextObject();
  while ((obj != nullptr) && (filter_->SkipObject(obj))) obj = NextObject();
  return obj;
}


HeapObject* HeapIterator::NextObject() {
  // No iterator means we are done.
  if (object_iterator_.get() == nullptr) return nullptr;

  if (HeapObject* obj = object_iterator_.get()->Next()) {
    // If the current iterator has more objects we are fine.
    return obj;
  } else {
    // Go though the spaces looking for one that has objects.
    while (space_iterator_->has_next()) {
      object_iterator_ = space_iterator_->next()->GetObjectIterator();
      if (HeapObject* obj = object_iterator_.get()->Next()) {
        return obj;
      }
    }
  }
  // Done with the last space.
  object_iterator_.reset(nullptr);
  return nullptr;
}


void Heap::UpdateTotalGCTime(double duration) {
  if (FLAG_trace_gc_verbose) {
    total_gc_time_ms_ += duration;
  }
}

void Heap::ExternalStringTable::CleanUpNewSpaceStrings() {
  int last = 0;
  Isolate* isolate = heap_->isolate();
  for (size_t i = 0; i < new_space_strings_.size(); ++i) {
    Object* o = new_space_strings_[i];
    if (o->IsTheHole(isolate)) {
      continue;
    }
    // The real external string is already in one of these vectors and was or
    // will be processed. Re-processing it will add a duplicate to the vector.
    if (o->IsThinString()) continue;
    DCHECK(o->IsExternalString());
    if (InNewSpace(o)) {
      new_space_strings_[last++] = o;
    } else {
      old_space_strings_.push_back(o);
    }
  }
  new_space_strings_.resize(last);
}

void Heap::ExternalStringTable::CleanUpAll() {
  CleanUpNewSpaceStrings();
  int last = 0;
  Isolate* isolate = heap_->isolate();
  for (size_t i = 0; i < old_space_strings_.size(); ++i) {
    Object* o = old_space_strings_[i];
    if (o->IsTheHole(isolate)) {
      continue;
    }
    // The real external string is already in one of these vectors and was or
    // will be processed. Re-processing it will add a duplicate to the vector.
    if (o->IsThinString()) continue;
    DCHECK(o->IsExternalString());
    DCHECK(!InNewSpace(o));
    old_space_strings_[last++] = o;
  }
  old_space_strings_.resize(last);
#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) {
    Verify();
  }
#endif
}

void Heap::ExternalStringTable::TearDown() {
  for (size_t i = 0; i < new_space_strings_.size(); ++i) {
    Object* o = new_space_strings_[i];
    // Dont finalize thin strings.
    if (o->IsThinString()) continue;
    heap_->FinalizeExternalString(ExternalString::cast(o));
  }
  new_space_strings_.clear();
  for (size_t i = 0; i < old_space_strings_.size(); ++i) {
    Object* o = old_space_strings_[i];
    // Dont finalize thin strings.
    if (o->IsThinString()) continue;
    heap_->FinalizeExternalString(ExternalString::cast(o));
  }
  old_space_strings_.clear();
}


void Heap::RememberUnmappedPage(Address page, bool compacted) {
  // Tag the page pointer to make it findable in the dump file.
  if (compacted) {
    page ^= 0xC1EAD & (Page::kPageSize - 1);  // Cleared.
  } else {
    page ^= 0x1D1ED & (Page::kPageSize - 1);  // I died.
  }
  remembered_unmapped_pages_[remembered_unmapped_pages_index_] = page;
  remembered_unmapped_pages_index_++;
  remembered_unmapped_pages_index_ %= kRememberedUnmappedPages;
}

void Heap::RegisterStrongRoots(Object** start, Object** end) {
  StrongRootsList* list = new StrongRootsList();
  list->next = strong_roots_list_;
  list->start = start;
  list->end = end;
  strong_roots_list_ = list;
}


void Heap::UnregisterStrongRoots(Object** start) {
  StrongRootsList* prev = nullptr;
  StrongRootsList* list = strong_roots_list_;
  while (list != nullptr) {
    StrongRootsList* next = list->next;
    if (list->start == start) {
      if (prev) {
        prev->next = next;
      } else {
        strong_roots_list_ = next;
      }
      delete list;
    } else {
      prev = list;
    }
    list = next;
  }
}

void Heap::SetBuiltinsConstantsTable(FixedArray* cache) {
  set_builtins_constants_table(cache);
}

size_t Heap::NumberOfTrackedHeapObjectTypes() {
  return ObjectStats::OBJECT_STATS_COUNT;
}


size_t Heap::ObjectCountAtLastGC(size_t index) {
  if (live_object_stats_ == nullptr || index >= ObjectStats::OBJECT_STATS_COUNT)
    return 0;
  return live_object_stats_->object_count_last_gc(index);
}


size_t Heap::ObjectSizeAtLastGC(size_t index) {
  if (live_object_stats_ == nullptr || index >= ObjectStats::OBJECT_STATS_COUNT)
    return 0;
  return live_object_stats_->object_size_last_gc(index);
}


bool Heap::GetObjectTypeName(size_t index, const char** object_type,
                             const char** object_sub_type) {
  if (index >= ObjectStats::OBJECT_STATS_COUNT) return false;

  switch (static_cast<int>(index)) {
#define COMPARE_AND_RETURN_NAME(name) \
  case name:                          \
    *object_type = #name;             \
    *object_sub_type = "";            \
    return true;
    INSTANCE_TYPE_LIST(COMPARE_AND_RETURN_NAME)
#undef COMPARE_AND_RETURN_NAME

#define COMPARE_AND_RETURN_NAME(name)                       \
  case ObjectStats::FIRST_VIRTUAL_TYPE + ObjectStats::name: \
    *object_type = #name;                                   \
    *object_sub_type = "";                                  \
    return true;
    VIRTUAL_INSTANCE_TYPE_LIST(COMPARE_AND_RETURN_NAME)
#undef COMPARE_AND_RETURN_NAME
  }
  return false;
}

size_t Heap::NumberOfNativeContexts() {
  int result = 0;
  Object* context = native_contexts_list();
  while (!context->IsUndefined(isolate())) {
    ++result;
    Context* native_context = Context::cast(context);
    context = native_context->next_context_link();
  }
  return result;
}

size_t Heap::NumberOfDetachedContexts() {
  // The detached_contexts() array has two entries per detached context.
  return detached_contexts()->length() / 2;
}

const char* AllocationSpaceName(AllocationSpace space) {
  switch (space) {
    case NEW_SPACE:
      return "NEW_SPACE";
    case OLD_SPACE:
      return "OLD_SPACE";
    case CODE_SPACE:
      return "CODE_SPACE";
    case MAP_SPACE:
      return "MAP_SPACE";
    case LO_SPACE:
      return "LO_SPACE";
    case NEW_LO_SPACE:
      return "NEW_LO_SPACE";
    case RO_SPACE:
      return "RO_SPACE";
    default:
      UNREACHABLE();
  }
  return nullptr;
}

void VerifyPointersVisitor::VisitPointers(HeapObject* host, Object** start,
                                          Object** end) {
  VerifyPointers(host, reinterpret_cast<MaybeObject**>(start),
                 reinterpret_cast<MaybeObject**>(end));
}

void VerifyPointersVisitor::VisitPointers(HeapObject* host, MaybeObject** start,
                                          MaybeObject** end) {
  VerifyPointers(host, start, end);
}

void VerifyPointersVisitor::VisitRootPointers(Root root,
                                              const char* description,
                                              Object** start, Object** end) {
  VerifyPointers(nullptr, reinterpret_cast<MaybeObject**>(start),
                 reinterpret_cast<MaybeObject**>(end));
}

void VerifyPointersVisitor::VerifyPointers(HeapObject* host,
                                           MaybeObject** start,
                                           MaybeObject** end) {
  for (MaybeObject** current = start; current < end; current++) {
    HeapObject* object;
    if ((*current)->GetHeapObject(&object)) {
      CHECK(heap_->Contains(object));
      CHECK(object->map()->IsMap());
    } else {
      CHECK((*current)->IsSmi() || (*current)->IsCleared());
    }
  }
}

void VerifySmisVisitor::VisitRootPointers(Root root, const char* description,
                                          Object** start, Object** end) {
  for (Object** current = start; current < end; current++) {
    CHECK((*current)->IsSmi());
  }
}

bool Heap::AllowedToBeMigrated(HeapObject* obj, AllocationSpace dst) {
  // Object migration is governed by the following rules:
  //
  // 1) Objects in new-space can be migrated to the old space
  //    that matches their target space or they stay in new-space.
  // 2) Objects in old-space stay in the same space when migrating.
  // 3) Fillers (two or more words) can migrate due to left-trimming of
  //    fixed arrays in new-space or old space.
  // 4) Fillers (one word) can never migrate, they are skipped by
  //    incremental marking explicitly to prevent invalid pattern.
  //
  // Since this function is used for debugging only, we do not place
  // asserts here, but check everything explicitly.
  if (obj->map() == ReadOnlyRoots(this).one_pointer_filler_map()) return false;
  InstanceType type = obj->map()->instance_type();
  MemoryChunk* chunk = MemoryChunk::FromAddress(obj->address());
  AllocationSpace src = chunk->owner()->identity();
  switch (src) {
    case NEW_SPACE:
      return dst == NEW_SPACE || dst == OLD_SPACE;
    case OLD_SPACE:
      return dst == OLD_SPACE;
    case CODE_SPACE:
      return dst == CODE_SPACE && type == CODE_TYPE;
    case MAP_SPACE:
    case LO_SPACE:
    case NEW_LO_SPACE:
    case RO_SPACE:
      return false;
  }
  UNREACHABLE();
}

void Heap::CreateObjectStats() {
  if (V8_LIKELY(FLAG_gc_stats == 0)) return;
  if (!live_object_stats_) {
    live_object_stats_ = new ObjectStats(this);
  }
  if (!dead_object_stats_) {
    dead_object_stats_ = new ObjectStats(this);
  }
}

void AllocationObserver::AllocationStep(int bytes_allocated,
                                        Address soon_object, size_t size) {
  DCHECK_GE(bytes_allocated, 0);
  bytes_to_next_step_ -= bytes_allocated;
  if (bytes_to_next_step_ <= 0) {
    Step(static_cast<int>(step_size_ - bytes_to_next_step_), soon_object, size);
    step_size_ = GetNextStepSize();
    bytes_to_next_step_ = step_size_;
  }
  DCHECK_GE(bytes_to_next_step_, 0);
}

namespace {

Map* GcSafeMapOfCodeSpaceObject(HeapObject* object) {
  MapWord map_word = object->map_word();
  return map_word.IsForwardingAddress() ? map_word.ToForwardingAddress()->map()
                                        : map_word.ToMap();
}

int GcSafeSizeOfCodeSpaceObject(HeapObject* object) {
  return object->SizeFromMap(GcSafeMapOfCodeSpaceObject(object));
}

Code* GcSafeCastToCode(Heap* heap, HeapObject* object, Address inner_pointer) {
  Code* code = reinterpret_cast<Code*>(object);
  DCHECK_NOT_NULL(code);
  DCHECK(heap->GcSafeCodeContains(code, inner_pointer));
  return code;
}

}  // namespace

bool Heap::GcSafeCodeContains(HeapObject* code, Address addr) {
  Map* map = GcSafeMapOfCodeSpaceObject(code);
  DCHECK(map == ReadOnlyRoots(this).code_map());
  if (InstructionStream::TryLookupCode(isolate(), addr) == code) return true;
  Address start = code->address();
  Address end = code->address() + code->SizeFromMap(map);
  return start <= addr && addr < end;
}

Code* Heap::GcSafeFindCodeForInnerPointer(Address inner_pointer) {
  Code* code = InstructionStream::TryLookupCode(isolate(), inner_pointer);
  if (code != nullptr) return code;

  // Check if the inner pointer points into a large object chunk.
  LargePage* large_page = lo_space()->FindPage(inner_pointer);
  if (large_page != nullptr) {
    return GcSafeCastToCode(this, large_page->GetObject(), inner_pointer);
  }

  DCHECK(code_space()->Contains(inner_pointer));

  // Iterate through the page until we reach the end or find an object starting
  // after the inner pointer.
  Page* page = Page::FromAddress(inner_pointer);
  DCHECK_EQ(page->owner(), code_space());
  mark_compact_collector()->sweeper()->EnsurePageIsIterable(page);

  Address addr = page->skip_list()->StartFor(inner_pointer);
  Address top = code_space()->top();
  Address limit = code_space()->limit();

  while (true) {
    if (addr == top && addr != limit) {
      addr = limit;
      continue;
    }

    HeapObject* obj = HeapObject::FromAddress(addr);
    int obj_size = GcSafeSizeOfCodeSpaceObject(obj);
    Address next_addr = addr + obj_size;
    if (next_addr > inner_pointer)
      return GcSafeCastToCode(this, obj, inner_pointer);
    addr = next_addr;
  }
}

void Heap::WriteBarrierForCodeSlow(Code* code) {
  for (RelocIterator it(code, RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT));
       !it.done(); it.next()) {
    GenerationalBarrierForCode(code, it.rinfo(), it.rinfo()->target_object());
    MarkingBarrierForCode(code, it.rinfo(), it.rinfo()->target_object());
  }
}

void Heap::GenerationalBarrierSlow(HeapObject* object, Address slot,
                                   HeapObject* value) {
  Heap* heap = Heap::FromWritableHeapObject(object);
  heap->store_buffer()->InsertEntry(slot);
}

void Heap::GenerationalBarrierForElementsSlow(Heap* heap, FixedArray* array,
                                              int offset, int length) {
  for (int i = 0; i < length; i++) {
    if (!InNewSpace(array->get(offset + i))) continue;
    heap->store_buffer()->InsertEntry(
        reinterpret_cast<Address>(array->RawFieldOfElementAt(offset + i)));
  }
}

void Heap::GenerationalBarrierForCodeSlow(Code* host, RelocInfo* rinfo,
                                          HeapObject* object) {
  DCHECK(InNewSpace(object));
  Page* source_page = Page::FromAddress(reinterpret_cast<Address>(host));
  RelocInfo::Mode rmode = rinfo->rmode();
  Address addr = rinfo->pc();
  SlotType slot_type = SlotTypeForRelocInfoMode(rmode);
  if (rinfo->IsInConstantPool()) {
    addr = rinfo->constant_pool_entry_address();
    if (RelocInfo::IsCodeTargetMode(rmode)) {
      slot_type = CODE_ENTRY_SLOT;
    } else {
      DCHECK(RelocInfo::IsEmbeddedObject(rmode));
      slot_type = OBJECT_SLOT;
    }
  }
  RememberedSet<OLD_TO_NEW>::InsertTyped(
      source_page, reinterpret_cast<Address>(host), slot_type, addr);
}

void Heap::MarkingBarrierSlow(HeapObject* object, Address slot,
                              HeapObject* value) {
  Heap* heap = Heap::FromWritableHeapObject(object);
  heap->incremental_marking()->RecordWriteSlow(
      object, reinterpret_cast<HeapObjectReference**>(slot), value);
}

void Heap::MarkingBarrierForElementsSlow(Heap* heap, HeapObject* object) {
  if (FLAG_concurrent_marking ||
      heap->incremental_marking()->marking_state()->IsBlack(object)) {
    heap->incremental_marking()->RevisitObject(object);
  }
}

void Heap::MarkingBarrierForCodeSlow(Code* host, RelocInfo* rinfo,
                                     HeapObject* object) {
  Heap* heap = Heap::FromWritableHeapObject(host);
  DCHECK(heap->incremental_marking()->IsMarking());
  heap->incremental_marking()->RecordWriteIntoCode(host, rinfo, object);
}

bool Heap::PageFlagsAreConsistent(HeapObject* object) {
  Heap* heap = Heap::FromWritableHeapObject(object);
  MemoryChunk* chunk = MemoryChunk::FromHeapObject(object);
  heap_internals::MemoryChunk* slim_chunk =
      heap_internals::MemoryChunk::FromHeapObject(object);

  const bool generation_consistency =
      chunk->owner()->identity() != NEW_SPACE ||
      (chunk->InNewSpace() && slim_chunk->InNewSpace());
  const bool marking_consistency =
      !heap->incremental_marking()->IsMarking() ||
      (chunk->IsFlagSet(MemoryChunk::INCREMENTAL_MARKING) &&
       slim_chunk->IsMarking());

  return generation_consistency && marking_consistency;
}

static_assert(MemoryChunk::Flag::INCREMENTAL_MARKING ==
                  heap_internals::MemoryChunk::kMarkingBit,
              "Incremental marking flag inconsistent");
static_assert(MemoryChunk::Flag::IN_FROM_SPACE ==
                  heap_internals::MemoryChunk::kFromSpaceBit,
              "From space flag inconsistent");
static_assert(MemoryChunk::Flag::IN_TO_SPACE ==
                  heap_internals::MemoryChunk::kToSpaceBit,
              "To space flag inconsistent");
static_assert(MemoryChunk::kFlagsOffset ==
                  heap_internals::MemoryChunk::kFlagsOffset,
              "Flag offset inconsistent");

void Heap::SetEmbedderStackStateForNextFinalizaton(
    EmbedderHeapTracer::EmbedderStackState stack_state) {
  local_embedder_heap_tracer()->SetEmbedderStackStateForNextFinalization(
      stack_state);
}

}  // namespace internal
}  // namespace v8
