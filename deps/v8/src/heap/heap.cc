// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/heap.h"

#include <unordered_map>
#include <unordered_set>

#include "src/accessors.h"
#include "src/api.h"
#include "src/assembler-inl.h"
#include "src/ast/context-slot-cache.h"
#include "src/base/bits.h"
#include "src/base/once.h"
#include "src/base/utils/random-number-generator.h"
#include "src/bootstrapper.h"
#include "src/code-stubs.h"
#include "src/compilation-cache.h"
#include "src/compiler-dispatcher/optimizing-compile-dispatcher.h"
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
#include "src/heap/incremental-marking.h"
#include "src/heap/item-parallel-job.h"
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
#include "src/heap/sweeper.h"
#include "src/interpreter/interpreter.h"
#include "src/objects/object-macros.h"
#include "src/objects/shared-function-info.h"
#include "src/regexp/jsregexp.h"
#include "src/runtime-profiler.h"
#include "src/snapshot/natives.h"
#include "src/snapshot/serializer-common.h"
#include "src/snapshot/snapshot.h"
#include "src/tracing/trace-event.h"
#include "src/trap-handler/trap-handler.h"
#include "src/unicode-inl.h"
#include "src/utils-inl.h"
#include "src/utils.h"
#include "src/v8.h"
#include "src/vm-state-inl.h"

namespace v8 {
namespace internal {

void Heap::SetArgumentsAdaptorDeoptPCOffset(int pc_offset) {
  DCHECK_EQ(Smi::kZero, arguments_adaptor_deopt_pc_offset());
  set_arguments_adaptor_deopt_pc_offset(Smi::FromInt(pc_offset));
}

void Heap::SetConstructStubCreateDeoptPCOffset(int pc_offset) {
  // TODO(tebbi): Remove second half of DCHECK once
  // FLAG_harmony_restrict_constructor_return is gone.
  DCHECK(construct_stub_create_deopt_pc_offset() == Smi::kZero ||
         construct_stub_create_deopt_pc_offset() == Smi::FromInt(pc_offset));
  set_construct_stub_create_deopt_pc_offset(Smi::FromInt(pc_offset));
}

void Heap::SetConstructStubInvokeDeoptPCOffset(int pc_offset) {
  // TODO(tebbi): Remove second half of DCHECK once
  // FLAG_harmony_restrict_constructor_return is gone.
  DCHECK(construct_stub_invoke_deopt_pc_offset() == Smi::kZero ||
         construct_stub_invoke_deopt_pc_offset() == Smi::FromInt(pc_offset));
  set_construct_stub_invoke_deopt_pc_offset(Smi::FromInt(pc_offset));
}

void Heap::SetInterpreterEntryReturnPCOffset(int pc_offset) {
  DCHECK_EQ(Smi::kZero, interpreter_entry_return_pc_offset());
  set_interpreter_entry_return_pc_offset(Smi::FromInt(pc_offset));
}

void Heap::SetSerializedTemplates(FixedArray* templates) {
  DCHECK_EQ(empty_fixed_array(), serialized_templates());
  DCHECK(isolate()->serializer_enabled());
  set_serialized_templates(templates);
}

void Heap::SetSerializedGlobalProxySizes(FixedArray* sizes) {
  DCHECK_EQ(empty_fixed_array(), serialized_global_proxy_sizes());
  DCHECK(isolate()->serializer_enabled());
  set_serialized_global_proxy_sizes(sizes);
}

bool Heap::GCCallbackTuple::operator==(
    const Heap::GCCallbackTuple& other) const {
  return other.callback == callback && other.data == data;
}

Heap::GCCallbackTuple& Heap::GCCallbackTuple::operator=(
    const Heap::GCCallbackTuple& other) {
  callback = other.callback;
  gc_type = other.gc_type;
  data = other.data;
  return *this;
}

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
    : external_memory_(0),
      external_memory_limit_(kExternalAllocationSoftLimit),
      external_memory_at_last_mark_compact_(0),
      isolate_(nullptr),
      code_range_size_(0),
      // semispace_size_ should be a power of 2 and old_generation_size_ should
      // be a multiple of Page::kPageSize.
      max_semi_space_size_(8 * (kPointerSize / 4) * MB),
      initial_semispace_size_(kMinSemiSpaceSizeInKB * KB),
      max_old_generation_size_(700ul * (kPointerSize / 4) * MB),
      initial_max_old_generation_size_(max_old_generation_size_),
      initial_old_generation_size_(max_old_generation_size_ /
                                   kInitalOldGenerationLimitFactor),
      old_generation_size_configured_(false),
      // Variables set based on semispace_size_ and old_generation_size_ in
      // ConfigureHeap.
      // Will be 4 * reserved_semispace_size_ to ensure that young
      // generation can be aligned to its size.
      maximum_committed_(0),
      survived_since_last_expansion_(0),
      survived_last_scavenge_(0),
      always_allocate_scope_count_(0),
      memory_pressure_level_(MemoryPressureLevel::kNone),
      out_of_memory_callback_(nullptr),
      out_of_memory_callback_data_(nullptr),
      contexts_disposed_(0),
      number_of_disposed_maps_(0),
      new_space_(nullptr),
      old_space_(nullptr),
      code_space_(nullptr),
      map_space_(nullptr),
      lo_space_(nullptr),
      write_protect_code_memory_(false),
      code_space_memory_modification_scope_depth_(0),
      gc_state_(NOT_IN_GC),
      gc_post_processing_depth_(0),
      allocations_count_(0),
      raw_allocations_hash_(0),
      ms_count_(0),
      gc_count_(0),
      mmap_region_base_(0),
      remembered_unmapped_pages_index_(0),
#ifdef DEBUG
      allocation_timeout_(0),
#endif  // DEBUG
      old_generation_allocation_limit_(initial_old_generation_size_),
      inline_allocation_disabled_(false),
      tracer_(nullptr),
      promoted_objects_size_(0),
      promotion_ratio_(0),
      semi_space_copied_object_size_(0),
      previous_semi_space_copied_object_size_(0),
      semi_space_copied_rate_(0),
      nodes_died_in_new_space_(0),
      nodes_copied_in_new_space_(0),
      nodes_promoted_(0),
      maximum_size_scavenges_(0),
      last_idle_notification_time_(0.0),
      last_gc_time_(0.0),
      mark_compact_collector_(nullptr),
      minor_mark_compact_collector_(nullptr),
      array_buffer_collector_(nullptr),
      memory_allocator_(nullptr),
      store_buffer_(nullptr),
      incremental_marking_(nullptr),
      concurrent_marking_(nullptr),
      gc_idle_time_handler_(nullptr),
      memory_reducer_(nullptr),
      live_object_stats_(nullptr),
      dead_object_stats_(nullptr),
      scavenge_job_(nullptr),
      parallel_scavenge_semaphore_(0),
      idle_scavenge_observer_(nullptr),
      new_space_allocation_counter_(0),
      old_generation_allocation_counter_at_last_gc_(0),
      old_generation_size_at_last_gc_(0),
      global_pretenuring_feedback_(kInitialFeedbackCapacity),
      is_marking_flag_(false),
      ring_buffer_full_(false),
      ring_buffer_end_(0),
      configured_(false),
      current_gc_flags_(Heap::kNoGCFlags),
      current_gc_callback_flags_(GCCallbackFlags::kNoGCCallbackFlags),
      external_string_table_(this),
      gc_callbacks_depth_(0),
      deserialization_complete_(false),
      strong_roots_list_(nullptr),
      heap_iterator_depth_(0),
      local_embedder_heap_tracer_(nullptr),
      fast_promotion_mode_(false),
      use_tasks_(true),
      force_oom_(false),
      delay_sweeper_tasks_for_testing_(false),
      pending_layout_change_object_(nullptr) {
  // Ensure old_generation_size_ is a multiple of kPageSize.
  DCHECK_EQ(0, max_old_generation_size_ & (Page::kPageSize - 1));

  memset(roots_, 0, sizeof(roots_[0]) * kRootListLength);
  set_native_contexts_list(nullptr);
  set_allocation_sites_list(Smi::kZero);
  set_encountered_weak_collections(Smi::kZero);
  // Put a dummy entry in the remembered pages so we can find the list the
  // minidump even if there are no real unmapped pages.
  RememberUnmappedPage(nullptr, false);
}

size_t Heap::Capacity() {
  if (!HasBeenSetUp()) return 0;

  return new_space_->Capacity() + OldGenerationCapacity();
}

size_t Heap::OldGenerationCapacity() {
  if (!HasBeenSetUp()) return 0;
  return old_space_->Capacity() + code_space_->Capacity() +
         map_space_->Capacity() + lo_space_->SizeOfObjects();
}

size_t Heap::CommittedOldGenerationMemory() {
  if (!HasBeenSetUp()) return 0;

  return old_space_->CommittedMemory() + code_space_->CommittedMemory() +
         map_space_->CommittedMemory() + lo_space_->Size();
}

size_t Heap::CommittedMemory() {
  if (!HasBeenSetUp()) return 0;

  return new_space_->CommittedMemory() + CommittedOldGenerationMemory();
}


size_t Heap::CommittedPhysicalMemory() {
  if (!HasBeenSetUp()) return 0;

  return new_space_->CommittedPhysicalMemory() +
         old_space_->CommittedPhysicalMemory() +
         code_space_->CommittedPhysicalMemory() +
         map_space_->CommittedPhysicalMemory() +
         lo_space_->CommittedPhysicalMemory();
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
  AllSpaces spaces(this);
  for (Space* space = spaces.next(); space != nullptr; space = spaces.next()) {
    total += space->Available();
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
  return old_space_ != nullptr && code_space_ != nullptr &&
         map_space_ != nullptr && lo_space_ != nullptr;
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

// TODO(1238405): Combine the infrastructure for --heap-stats and
// --log-gc to avoid the complicated preprocessor and flag testing.
void Heap::ReportStatisticsBeforeGC() {
// Heap::ReportHeapStatistics will also log NewSpace statistics when
// compiled --log-gc is set.  The following logic is used to avoid
// double logging.
#ifdef DEBUG
  if (FLAG_heap_stats || FLAG_log_gc) new_space_->CollectStatistics();
  if (FLAG_heap_stats) {
    ReportHeapStatistics("Before GC");
  } else if (FLAG_log_gc) {
    new_space_->ReportStatistics();
  }
  if (FLAG_heap_stats || FLAG_log_gc) new_space_->ClearHistograms();
#else
  if (FLAG_log_gc) {
    new_space_->CollectStatistics();
    new_space_->ReportStatistics();
    new_space_->ClearHistograms();
  }
#endif  // DEBUG
}


void Heap::PrintShortHeapStatistics() {
  if (!FLAG_trace_gc_verbose) return;
  PrintIsolate(isolate_, "Memory allocator,   used: %6" PRIuS
                         " KB,"
                         " available: %6" PRIuS " KB\n",
               memory_allocator()->Size() / KB,
               memory_allocator()->Available() / KB);
  PrintIsolate(isolate_, "New space,          used: %6" PRIuS
                         " KB"
                         ", available: %6" PRIuS
                         " KB"
                         ", committed: %6" PRIuS " KB\n",
               new_space_->Size() / KB, new_space_->Available() / KB,
               new_space_->CommittedMemory() / KB);
  PrintIsolate(isolate_, "Old space,          used: %6" PRIuS
                         " KB"
                         ", available: %6" PRIuS
                         " KB"
                         ", committed: %6" PRIuS " KB\n",
               old_space_->SizeOfObjects() / KB, old_space_->Available() / KB,
               old_space_->CommittedMemory() / KB);
  PrintIsolate(isolate_, "Code space,         used: %6" PRIuS
                         " KB"
                         ", available: %6" PRIuS
                         " KB"
                         ", committed: %6" PRIuS "KB\n",
               code_space_->SizeOfObjects() / KB, code_space_->Available() / KB,
               code_space_->CommittedMemory() / KB);
  PrintIsolate(isolate_, "Map space,          used: %6" PRIuS
                         " KB"
                         ", available: %6" PRIuS
                         " KB"
                         ", committed: %6" PRIuS " KB\n",
               map_space_->SizeOfObjects() / KB, map_space_->Available() / KB,
               map_space_->CommittedMemory() / KB);
  PrintIsolate(isolate_, "Large object space, used: %6" PRIuS
                         " KB"
                         ", available: %6" PRIuS
                         " KB"
                         ", committed: %6" PRIuS " KB\n",
               lo_space_->SizeOfObjects() / KB, lo_space_->Available() / KB,
               lo_space_->CommittedMemory() / KB);
  PrintIsolate(isolate_, "All spaces,         used: %6" PRIuS
                         " KB"
                         ", available: %6" PRIuS
                         " KB"
                         ", committed: %6" PRIuS "KB\n",
               this->SizeOfObjects() / KB, this->Available() / KB,
               this->CommittedMemory() / KB);
  PrintIsolate(isolate_, "External memory reported: %6" PRId64 " KB\n",
               external_memory_ / KB);
  PrintIsolate(isolate_, "External memory global %zu KB\n",
               external_memory_callback_() / KB);
  PrintIsolate(isolate_, "Total time spent in GC  : %.1f ms\n",
               total_gc_time_ms_);
}

// TODO(1238405): Combine the infrastructure for --heap-stats and
// --log-gc to avoid the complicated preprocessor and flag testing.
void Heap::ReportStatisticsAfterGC() {
// Similar to the before GC, we use some complicated logic to ensure that
// NewSpace statistics are logged exactly once when --log-gc is turned on.
#if defined(DEBUG)
  if (FLAG_heap_stats) {
    new_space_->CollectStatistics();
    ReportHeapStatistics("After GC");
  } else if (FLAG_log_gc) {
    new_space_->ReportStatistics();
  }
#else
  if (FLAG_log_gc) new_space_->ReportStatistics();
#endif  // DEBUG
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

void Heap::AddRetainingPathTarget(Handle<HeapObject> object,
                                  RetainingPathOption option) {
  if (!FLAG_track_retaining_path) {
    PrintF("Retaining path tracking requires --trace-retaining-path\n");
  } else {
    int index = 0;
    Handle<WeakFixedArray> array = WeakFixedArray::Add(
        handle(retaining_path_targets(), isolate()), object, &index);
    set_retaining_path_targets(*array);
    retaining_path_target_option_[index] = option;
  }
}

bool Heap::IsRetainingPathTarget(HeapObject* object,
                                 RetainingPathOption* option) {
  if (!retaining_path_targets()->IsWeakFixedArray()) return false;
  WeakFixedArray* targets = WeakFixedArray::cast(retaining_path_targets());
  int length = targets->Length();
  for (int i = 0; i < length; i++) {
    if (targets->Get(i) == object) {
      DCHECK(retaining_path_target_option_.count(i));
      *option = retaining_path_target_option_[i];
      return true;
    }
  }
  return false;
}

namespace {
const char* RootToString(Root root) {
  switch (root) {
#define ROOT_CASE(root_id, ignore, description) \
  case Root::root_id:                           \
    return description;
    ROOT_ID_LIST(ROOT_CASE)
#undef ROOT_CASE
    case Root::kCodeFlusher:
      return "(Code flusher)";
    case Root::kPartialSnapshotCache:
      return "(Partial snapshot cache)";
    case Root::kWeakCollections:
      return "(Weak collections)";
    case Root::kWrapperTracing:
      return "(Wrapper tracing)";
    case Root::kUnknown:
      return "(Unknown)";
  }
  UNREACHABLE();
  return nullptr;
}
}  // namespace

void Heap::PrintRetainingPath(HeapObject* target, RetainingPathOption option) {
  PrintF("\n\n\n");
  PrintF("#################################################\n");
  PrintF("Retaining path for %p:\n", static_cast<void*>(target));
  HeapObject* object = target;
  std::vector<std::pair<HeapObject*, bool>> retaining_path;
  Root root = Root::kUnknown;
  bool ephemeral = false;
  while (true) {
    retaining_path.push_back(std::make_pair(object, ephemeral));
    if (option == RetainingPathOption::kTrackEphemeralPath &&
        ephemeral_retainer_.count(object)) {
      object = ephemeral_retainer_[object];
      ephemeral = true;
    } else if (retainer_.count(object)) {
      object = retainer_[object];
      ephemeral = false;
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
    bool ephemeral = node.second;
    PrintF("\n");
    PrintF("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");
    PrintF("Distance from root %d%s: ", distance,
           ephemeral ? " (ephemeral)" : "");
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
  PrintF("Root: %s\n", RootToString(root));
  PrintF("-------------------------------------------------\n");
}

void Heap::AddRetainer(HeapObject* retainer, HeapObject* object) {
  if (retainer_.count(object)) return;
  retainer_[object] = retainer;
  RetainingPathOption option = RetainingPathOption::kDefault;
  if (IsRetainingPathTarget(object, &option)) {
    // Check if the retaining path was already printed in
    // AddEphemeralRetainer().
    if (ephemeral_retainer_.count(object) == 0 ||
        option == RetainingPathOption::kDefault) {
      PrintRetainingPath(object, option);
    }
  }
}

void Heap::AddEphemeralRetainer(HeapObject* retainer, HeapObject* object) {
  if (ephemeral_retainer_.count(object)) return;
  ephemeral_retainer_[object] = retainer;
  RetainingPathOption option = RetainingPathOption::kDefault;
  if (IsRetainingPathTarget(object, &option) &&
      option == RetainingPathOption::kTrackEphemeralPath) {
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

  ReportStatisticsBeforeGC();
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
    ephemeral_retainer_.clear();
    retaining_root_.clear();
  }
}

size_t Heap::SizeOfObjects() {
  size_t total = 0;
  AllSpaces spaces(this);
  for (Space* space = spaces.next(); space != nullptr; space = spaces.next()) {
    total += space->SizeOfObjects();
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
    default:
      UNREACHABLE();
  }
  return nullptr;
}

void Heap::SetRootCodeStubs(NumberDictionary* value) {
  roots_[kCodeStubsRootIndex] = value;
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
      Object* list_element = allocation_sites_list();
      while (list_element->IsAllocationSite()) {
        site = AllocationSite::cast(list_element);
        DCHECK(site->IsAllocationSite());
        allocation_sites++;
        if (site->IsMaybeTenure()) {
          site->set_deopt_dependent_code(true);
          trigger_deoptimization = true;
        }
        list_element = site->weak_next();
      }
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
  code->InvalidateEmbeddedObjects();
}

void Heap::InvalidateCodeDeoptimizationData(Code* code) {
  MemoryChunk* chunk = MemoryChunk::FromAddress(code->address());
  CodePageMemoryModificationScope modification_scope(chunk);
  code->set_deoptimization_data(empty_fixed_array());
}

void Heap::DeoptMarkedAllocationSites() {
  // TODO(hpayer): If iterating over the allocation sites list becomes a
  // performance issue, use a cache data structure in heap instead.
  Object* list_element = allocation_sites_list();
  while (list_element->IsAllocationSite()) {
    AllocationSite* site = AllocationSite::cast(list_element);
    if (site->deopt_dependent_code()) {
      site->dependent_code()->MarkCodeForDeoptimization(
          isolate_, DependentCode::kAllocationSiteTenuringChangedGroup);
      site->set_deopt_dependent_code(false);
    }
    list_element = site->weak_next();
  }
  Deoptimizer::DeoptimizeMarkedCode(isolate_);
}


void Heap::GarbageCollectionEpilogue() {
  TRACE_GC(tracer(), GCTracer::Scope::HEAP_EPILOGUE);
  // In release mode, we only zap the from space under heap verification.
  if (Heap::ShouldZapGarbage()) {
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


void Heap::PreprocessStackTraces() {
  WeakFixedArray::Iterator iterator(weak_stack_trace_list());
  FixedArray* elements;
  while ((elements = iterator.Next<FixedArray>()) != nullptr) {
    for (int j = 1; j < elements->length(); j += 4) {
      Object* maybe_code = elements->get(j + 2);
      // If GC happens while adding a stack trace to the weak fixed array,
      // which has been copied into a larger backing store, we may run into
      // a stack trace that has already been preprocessed. Guard against this.
      if (!maybe_code->IsAbstractCode()) break;
      AbstractCode* abstract_code = AbstractCode::cast(maybe_code);
      int offset = Smi::ToInt(elements->get(j + 3));
      int pos = abstract_code->SourcePosition(offset);
      elements->set(j + 2, Smi::FromInt(pos));
    }
  }
  // We must not compact the weak fixed list here, as we may be in the middle
  // of writing to it, when the GC triggered. Instead, we reset the root value.
  set_weak_stack_trace_list(Smi::kZero);
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
  if (HighMemoryPressure()) {
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
    FinalizeIncrementalMarking(
        GarbageCollectionReason::kFinalizeMarkingViaStackGuard);
  }
}


void Heap::ScheduleIdleScavengeIfNeeded(int bytes_allocated) {
  scavenge_job_->ScheduleIdleTaskIfNeeded(this, bytes_allocated);
}

void Heap::FinalizeIncrementalMarking(GarbageCollectionReason gc_reason) {
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
    InvokeOutOfMemoryCallback();
  }
  RuntimeCallTimerScope runtime_timer(
      isolate(), &RuntimeCallStats::GC_Custom_AllAvailableGarbage);
  if (isolate()->concurrent_recompilation_enabled()) {
    // The optimizing compiler may be unnecessarily holding on to memory.
    DisallowHeapAllocation no_recursive_gc;
    isolate()->optimizing_compile_dispatcher()->Flush(
        OptimizingCompileDispatcher::BlockingBehavior::kDontBlock);
  }
  isolate()->ClearSerializerData();
  set_current_gc_flags(kMakeHeapIterableMask | kReduceMemoryFootprintMask);
  isolate_->compilation_cache()->Clear();
  const int kMaxNumberOfAttempts = 7;
  const int kMinNumberOfAttempts = 2;
  for (int attempt = 0; attempt < kMaxNumberOfAttempts; attempt++) {
    if (!CollectGarbage(OLD_SPACE, gc_reason,
                        v8::kGCCallbackFlagCollectAllAvailableGarbage) &&
        attempt + 1 >= kMinNumberOfAttempts) {
      break;
    }
  }

  set_current_gc_flags(kNoGCFlags);
  new_space_->Shrink();
  UncommitFromSpace();
}

void Heap::ReportExternalMemoryPressure() {
  const GCCallbackFlags kGCCallbackFlagsForExternalMemory =
      static_cast<GCCallbackFlags>(
          kGCCallbackFlagSynchronousPhantomCallbackProcessing |
          kGCCallbackFlagCollectAllExternalMemory);
  if (external_memory_ >
      (external_memory_at_last_mark_compact_ + external_memory_hard_limit())) {
    CollectAllGarbage(
        kReduceMemoryFootprintMask | kFinalizeIncrementalMarkingMask,
        GarbageCollectionReason::kExternalMemoryPressure,
        static_cast<GCCallbackFlags>(kGCCallbackFlagCollectAllAvailableGarbage |
                                     kGCCallbackFlagsForExternalMemory));
    return;
  }
  if (incremental_marking()->IsStopped()) {
    if (incremental_marking()->CanBeActivated()) {
      StartIncrementalMarking(i::Heap::kNoGCFlags,
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
  // The VM is in the GC state until exiting this function.
  VMState<GC> state(isolate());

  const char* collector_reason = nullptr;
  GarbageCollector collector = SelectGarbageCollector(space, &collector_reason);

#ifdef DEBUG
  // Reset the allocation timeout to the GC interval, but make sure to
  // allow at least a few allocations after a collection. The reason
  // for this is that we have a lot of allocation sequences and we
  // assume that a garbage collection will allow the subsequent
  // allocation attempts to go through.
  allocation_timeout_ = Max(6, FLAG_gc_interval);
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

      next_gc_likely_to_collect_more =
          PerformGarbageCollection(collector, gc_callback_flags);
    }

    GarbageCollectionEpilogue();
    if (collector == MARK_COMPACTOR && FLAG_track_detached_contexts) {
      isolate()->CheckDetachedContextsAfterGC();
    }

    if (collector == MARK_COMPACTOR) {
      size_t committed_memory_after = CommittedOldGenerationMemory();
      size_t used_memory_after = PromotedSpaceSizeOfObjects();
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
      memory_pressure_level_.SetValue(MemoryPressureLevel::kNone);
    }

    tracer()->Stop(collector);
  }

  if (collector == MARK_COMPACTOR &&
      (gc_callback_flags & (kGCCallbackFlagForced |
                            kGCCallbackFlagCollectAllAvailableGarbage)) != 0) {
    isolate()->CountUsage(v8::Isolate::kForcedGC);
  }

  // Start incremental marking for the next cycle. The heap snapshot
  // generator needs incremental marking to stay off after it aborted.
  // We do this only for scavenger to avoid a loop where mark-compact
  // causes another mark-compact.
  if (IsYoungGenerationCollector(collector) &&
      !ShouldAbortIncrementalMarking()) {
    StartIncrementalMarkingIfAllocationLimitIsReached(
        kNoGCFlags, kGCCallbackScheduleIdleGarbageCollection);
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
  if (isolate()->concurrent_recompilation_enabled()) {
    // Flush the queued recompilation tasks.
    isolate()->optimizing_compile_dispatcher()->Flush(
        OptimizingCompileDispatcher::BlockingBehavior::kDontBlock);
  }
  number_of_disposed_maps_ = retained_maps()->Length();
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
                        int len) {
  if (len == 0) return;

  DCHECK(array->map() != fixed_cow_array_map());
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
  FIXED_ARRAY_ELEMENTS_WRITE_BARRIER(this, array, dst_index, len);
}


#ifdef VERIFY_HEAP
// Helper class for verifying the string table.
class StringTableVerifier : public ObjectVisitor {
 public:
  void VisitPointers(HeapObject* host, Object** start, Object** end) override {
    // Visit all HeapObject pointers in [start, end).
    for (Object** p = start; p < end; p++) {
      if ((*p)->IsHeapObject()) {
        HeapObject* object = HeapObject::cast(*p);
        Isolate* isolate = object->GetIsolate();
        // Check that the string is actually internalized.
        CHECK(object->IsTheHole(isolate) || object->IsUndefined(isolate) ||
              object->IsInternalizedString());
      }
    }
  }
};


static void VerifyStringTable(Heap* heap) {
  StringTableVerifier verifier;
  heap->string_table()->IterateElements(&verifier);
}
#endif  // VERIFY_HEAP

bool Heap::ReserveSpace(Reservation* reservations, std::vector<Address>* maps) {
  bool gc_performed = true;
  int counter = 0;
  static const int kThreshold = 20;
  while (gc_performed && counter++ < kThreshold) {
    gc_performed = false;
    for (int space = NEW_SPACE; space < SerializerDeserializer::kNumberOfSpaces;
         space++) {
      Reservation* reservation = &reservations[space];
      DCHECK_LE(1, reservation->size());
      if (reservation->at(0).size == 0) continue;
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
              "insufficient memory to create an Isolate");
        }
        if (space == NEW_SPACE) {
          CollectGarbage(NEW_SPACE, GarbageCollectionReason::kDeserializer);
        } else {
          if (counter > 1) {
            CollectAllGarbage(
                kReduceMemoryFootprintMask | kAbortIncrementalMarkingMask,
                GarbageCollectionReason::kDeserializer);
          } else {
            CollectAllGarbage(kAbortIncrementalMarkingMask,
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
  V8::FatalProcessOutOfMemory("Committing semi space failed.");
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
    VerifyStringTable(this);
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
        old_generation_size_at_last_gc_ = PromotedSpaceSizeOfObjects();
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
    ComputeFastPromotionMode(promotion_ratio_ + semi_space_copied_rate_);
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

  isolate_->eternal_handles()->PostGarbageCollectionProcessing(this);

  // Update relocatables.
  Relocatable::PostGarbageCollectionProcessing(isolate_);

  double gc_speed = tracer()->CombinedMarkCompactSpeedInBytesPerMillisecond();
  double mutator_speed =
      tracer()->CurrentOldGenerationAllocationThroughputInBytesPerMillisecond();
  size_t old_gen_size = PromotedSpaceSizeOfObjects();
  if (collector == MARK_COMPACTOR) {
    // Register the amount of external allocated memory.
    external_memory_at_last_mark_compact_ = external_memory_;
    external_memory_limit_ = external_memory_ + kExternalAllocationSoftLimit;
    SetOldGenerationAllocationLimit(old_gen_size, gc_speed, mutator_speed);
  } else if (HasLowYoungGenerationAllocationRate() &&
             old_generation_size_configured_) {
    DampenOldGenerationAllocationLimit(old_gen_size, gc_speed, mutator_speed);
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
    VerifyStringTable(this);
  }
#endif

  return freed_global_handles > 0;
}


void Heap::CallGCPrologueCallbacks(GCType gc_type, GCCallbackFlags flags) {
  RuntimeCallTimerScope runtime_timer(isolate(),
                                      &RuntimeCallStats::GCPrologueCallback);
  for (const GCCallbackTuple& info : gc_prologue_callbacks_) {
    if (gc_type & info.gc_type) {
      v8::Isolate* isolate = reinterpret_cast<v8::Isolate*>(this->isolate());
      info.callback(isolate, gc_type, flags, info.data);
    }
  }
}

void Heap::CallGCEpilogueCallbacks(GCType gc_type, GCCallbackFlags flags) {
  RuntimeCallTimerScope runtime_timer(isolate(),
                                      &RuntimeCallStats::GCEpilogueCallback);
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
  DCHECK(FLAG_minor_mc);

  SetGCState(MINOR_MARK_COMPACT);
  LOG(isolate_, ResourceEvent("MinorMarkCompact", "begin"));

  TRACE_GC(tracer(), GCTracer::Scope::MINOR_MC);
  AlwaysAllocateScope always_allocate(isolate());
  PauseAllocationObserversScope pause_observers(this);
  IncrementalMarking::PauseBlackAllocationScope pause_black_allocation(
      incremental_marking());

  minor_mark_compact_collector()->CollectGarbage();

  LOG(isolate_, ResourceEvent("MinorMarkCompact", "end"));
  SetGCState(NOT_IN_GC);
}

void Heap::MarkCompactEpilogue() {
  TRACE_GC(tracer(), GCTracer::Scope::MC_EPILOGUE);
  SetGCState(NOT_IN_GC);

  isolate_->counters()->objs_since_last_full()->Set(0);

  incremental_marking()->Epilogue();

  PreprocessStackTraces();
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

static bool IsUnscavengedHeapObject(Heap* heap, Object** p) {
  return heap->InFromSpace(*p) &&
         !HeapObject::cast(*p)->map_word().IsForwardingAddress();
}

class ScavengeWeakObjectRetainer : public WeakObjectRetainer {
 public:
  explicit ScavengeWeakObjectRetainer(Heap* heap) : heap_(heap) {}

  virtual Object* RetainAs(Object* object) {
    if (!heap_->InFromSpace(object)) {
      return object;
    }

    MapWord map_word = HeapObject::cast(object)->map_word();
    if (map_word.IsForwardingAddress()) {
      return map_word.ToForwardingAddress();
    }
    return nullptr;
  }

 private:
  Heap* heap_;
};

void Heap::EvacuateYoungGeneration() {
  TRACE_GC(tracer(), GCTracer::Scope::SCAVENGER_FAST_PROMOTE);
  base::LockGuard<base::Mutex> guard(relocation_mutex());
  ConcurrentMarking::PauseScope pause_scope(concurrent_marking());
  if (!FLAG_concurrent_marking) {
    DCHECK(fast_promotion_mode_);
    DCHECK(CanExpandOldGeneration(new_space()->Size()));
  }

  mark_compact_collector()->sweeper()->EnsureNewSpaceCompleted();

  SetGCState(SCAVENGE);
  LOG(isolate_, ResourceEvent("scavenge", "begin"));

  // Move pages from new->old generation.
  PageRange range(new_space()->bottom(), new_space()->top());
  for (auto it = range.begin(); it != range.end();) {
    Page* p = (*++it)->prev_page();
    p->Unlink();
    Page::ConvertNewToOld(p);
    if (incremental_marking()->IsMarking())
      mark_compact_collector()->RecordLiveSlotsOnPage(p);
  }

  // Reset new space.
  if (!new_space()->Rebalance()) {
    FatalProcessOutOfMemory("NewSpace::Rebalance");
  }
  new_space()->ResetAllocationInfo();
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

static bool IsLogging(Isolate* isolate) {
  return FLAG_verify_predictable || isolate->logger()->is_logging() ||
         isolate->is_profiling() ||
         (isolate->heap_profiler() != nullptr &&
          isolate->heap_profiler()->is_tracking_object_moves());
}

class PageScavengingItem final : public ItemParallelJob::Item {
 public:
  explicit PageScavengingItem(MemoryChunk* chunk) : chunk_(chunk) {}
  virtual ~PageScavengingItem() {}

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

int Heap::NumberOfScavengeTasks() {
  if (!FLAG_parallel_scavenge) return 1;
  const int num_scavenge_tasks =
      static_cast<int>(new_space()->TotalCapacity()) / MB;
  return Max(
      1,
      Min(Min(num_scavenge_tasks, kMaxScavengerTasks),
          static_cast<int>(
              V8::GetCurrentPlatform()->NumberOfAvailableBackgroundThreads())));
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

  if (mark_compact_collector()->sweeper()->sweeping_in_progress() &&
      memory_allocator_->unmapper()->NumberOfDelayedChunks() >
          static_cast<int>(new_space_->MaximumCapacity() / Page::kPageSize)) {
    mark_compact_collector()->EnsureSweepingCompleted();
  }

  // TODO(mlippautz): Untangle the dependency of the unmapper from the sweeper.
  mark_compact_collector()->sweeper()->EnsureNewSpaceCompleted();

  SetGCState(SCAVENGE);

  // Implements Cheney's copying algorithm
  LOG(isolate_, ResourceEvent("scavenge", "begin"));

  // Flip the semispaces.  After flipping, to space is empty, from space has
  // live objects.
  new_space_->Flip();
  new_space_->ResetAllocationInfo();

  ItemParallelJob job(isolate()->cancelable_task_manager(),
                      &parallel_scavenge_semaphore_);
  const int kMainThreadId = 0;
  Scavenger* scavengers[kMaxScavengerTasks];
  const bool is_logging = IsLogging(isolate());
  const int num_scavenge_tasks = NumberOfScavengeTasks();
  OneshotBarrier barrier;
  Scavenger::CopiedList copied_list(num_scavenge_tasks);
  Scavenger::PromotionList promotion_list(num_scavenge_tasks);
  for (int i = 0; i < num_scavenge_tasks; i++) {
    scavengers[i] =
        new Scavenger(this, is_logging, &copied_list, &promotion_list, i);
    job.AddTask(new ScavengingTask(this, scavengers[i], &barrier));
  }

  {
    Sweeper* sweeper = mark_compact_collector()->sweeper();
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
        this, [&job](MemoryChunk* chunk) {
          job.AddItem(new PageScavengingItem(chunk));
        });

    RootScavengeVisitor root_scavenge_visitor(this, scavengers[kMainThreadId]);

    {
      // Identify weak unmodified handles. Requires an unmodified graph.
      TRACE_GC(
          tracer(),
          GCTracer::Scope::SCAVENGER_SCAVENGE_WEAK_GLOBAL_HANDLES_IDENTIFY);
      isolate()->global_handles()->IdentifyWeakUnmodifiedObjects(
          &JSObject::IsUnmodifiedApiObject);
    }
    {
      // Copy roots.
      TRACE_GC(tracer(), GCTracer::Scope::SCAVENGER_SCAVENGE_ROOTS);
      IterateRoots(&root_scavenge_visitor, VISIT_ALL_IN_SCAVENGE);
    }
    {
      // Weak collections are held strongly by the Scavenger.
      TRACE_GC(tracer(), GCTracer::Scope::SCAVENGER_SCAVENGE_WEAK);
      IterateEncounteredWeakCollections(&root_scavenge_visitor);
    }
    {
      // Parallel phase scavenging all copied and promoted objects.
      TRACE_GC(tracer(), GCTracer::Scope::SCAVENGER_SCAVENGE_PARALLEL);
      job.Run();
      DCHECK(copied_list.IsGlobalEmpty());
      DCHECK(promotion_list.IsGlobalEmpty());
    }
    {
      // Scavenge weak global handles.
      TRACE_GC(tracer(),
               GCTracer::Scope::SCAVENGER_SCAVENGE_WEAK_GLOBAL_HANDLES_PROCESS);
      isolate()->global_handles()->MarkNewSpaceWeakUnmodifiedObjectsPending(
          &IsUnscavengedHeapObject);
      isolate()
          ->global_handles()
          ->IterateNewSpaceWeakUnmodifiedRootsForFinalizers(
              &root_scavenge_visitor);
      scavengers[kMainThreadId]->Process();

      DCHECK(copied_list.IsGlobalEmpty());
      DCHECK(promotion_list.IsGlobalEmpty());
      isolate()
          ->global_handles()
          ->IterateNewSpaceWeakUnmodifiedRootsForPhantomHandles(
              &root_scavenge_visitor, &IsUnscavengedHeapObject);
    }

    for (int i = 0; i < num_scavenge_tasks; i++) {
      scavengers[i]->Finalize();
      delete scavengers[i];
    }
  }

  UpdateNewSpaceReferencesInExternalStringTable(
      &UpdateNewSpaceReferenceInExternalStringTableEntry);

  incremental_marking()->UpdateMarkingWorklistAfterScavenge();

  if (FLAG_concurrent_marking) {
    // Ensure that concurrent marker does not track pages that are
    // going to be unmapped.
    for (Page* p : PageRange(new_space()->FromSpaceStart(),
                             new_space()->FromSpaceEnd())) {
      concurrent_marking()->ClearLiveness(p);
    }
  }

  ScavengeWeakObjectRetainer weak_object_retainer(this);
  ProcessYoungWeakReferences(&weak_object_retainer);

  // Set age mark.
  new_space_->set_age_mark(new_space_->top());

  {
    TRACE_GC(tracer(), GCTracer::Scope::SCAVENGER_PROCESS_ARRAY_BUFFERS);
    ArrayBufferTracker::PrepareToFreeDeadInNewSpace(this);
  }
  array_buffer_collector()->FreeAllocationsOnBackgroundThread();

  RememberedSet<OLD_TO_NEW>::IterateMemoryChunks(this, [](MemoryChunk* chunk) {
    if (chunk->SweepingDone()) {
      RememberedSet<OLD_TO_NEW>::FreeEmptyBuckets(chunk);
    } else {
      RememberedSet<OLD_TO_NEW>::PreFreeEmptyBuckets(chunk);
    }
  });

  // Update how much has survived scavenge.
  IncrementYoungSurvivorsCounter(SurvivedNewSpaceObjectSize());

  // Scavenger may find new wrappers by iterating objects promoted onto a black
  // page.
  local_embedder_heap_tracer()->RegisterWrappersWithRemoteTracer();

  LOG(isolate_, ResourceEvent("scavenge", "end"));

  SetGCState(NOT_IN_GC);
}

void Heap::ComputeFastPromotionMode(double survival_rate) {
  const size_t survived_in_new_space =
      survived_last_scavenge_ * 100 / new_space_->Capacity();
  fast_promotion_mode_ =
      !FLAG_optimize_for_size && FLAG_fast_promotion_new_space &&
      !ShouldReduceMemory() && new_space_->IsAtMaximumCapacity() &&
      survived_in_new_space >= kMinPromotedPercentForFastPromotionMode;
  if (FLAG_trace_gc_verbose) {
    PrintIsolate(
        isolate(), "Fast promotion mode: %s survival rate: %" PRIuS "%%\n",
        fast_promotion_mode_ ? "true" : "false", survived_in_new_space);
  }
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
  String* string = String::cast(first_word.ToForwardingAddress());
  if (string->IsThinString()) string = ThinString::cast(string)->actual();
  // Internalization can replace external strings with non-external strings.
  return string->IsExternalString() ? string : nullptr;
}

void Heap::ExternalStringTable::Verify() {
#ifdef DEBUG
  for (size_t i = 0; i < new_space_strings_.size(); ++i) {
    Object* obj = Object::cast(new_space_strings_[i]);
    DCHECK(heap_->InNewSpace(obj));
    DCHECK(!obj->IsTheHole(heap_->isolate()));
  }
  for (size_t i = 0; i < old_space_strings_.size(); ++i) {
    Object* obj = Object::cast(old_space_strings_[i]);
    DCHECK(!heap_->InNewSpace(obj));
    DCHECK(!obj->IsTheHole(heap_->isolate()));
  }
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

    if (heap_->InNewSpace(target)) {
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
    Verify();
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
    v->VisitRootPointers(Root::kExternalStringsTable, new_space_strings_.data(),
                         new_space_strings_.data() + new_space_strings_.size());
  }
}

void Heap::ExternalStringTable::IterateAll(RootVisitor* v) {
  IterateNewSpaceStrings(v);
  if (!old_space_strings_.empty()) {
    v->VisitRootPointers(Root::kExternalStringsTable, old_space_strings_.data(),
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

void Heap::ResetAllAllocationSitesDependentCode(PretenureFlag flag) {
  DisallowHeapAllocation no_allocation_scope;
  Object* cur = allocation_sites_list();
  bool marked = false;
  while (cur->IsAllocationSite()) {
    AllocationSite* casted = AllocationSite::cast(cur);
    if (casted->GetPretenureMode() == flag) {
      casted->ResetPretenureDecision();
      casted->set_deopt_dependent_code(true);
      marked = true;
      RemoveAllocationSitePretenuringFeedback(casted);
    }
    cur = casted->weak_next();
  }
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
        v8::ExternalResourceVisitor* visitor)
        : visitor_(visitor) {}
    virtual void VisitRootPointers(Root root, Object** start, Object** end) {
      for (Object** p = start; p < end; p++) {
        DCHECK((*p)->IsExternalString());
        visitor_->VisitExternalString(
            Utils::ToLocal(Handle<String>(String::cast(*p))));
      }
    }

   private:
    v8::ExternalResourceVisitor* visitor_;
  } external_string_table_visitor(visitor);

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
  intptr_t offset = OffsetFrom(address);
  if (alignment == kDoubleAligned && (offset & kDoubleAlignmentMask) != 0)
    return kPointerSize;
  if (alignment == kDoubleUnaligned && (offset & kDoubleAlignmentMask) == 0)
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
        Max(MinimumAllocationLimitGrowingStep(),
            static_cast<size_t>(
                static_cast<double>(old_generation_allocation_limit_) *
                (tracer()->AverageSurvivalRatio() / 100)));
  }
}

AllocationResult Heap::AllocatePartialMap(InstanceType instance_type,
                                          int instance_size) {
  Object* result = nullptr;
  AllocationResult allocation = AllocateRaw(Map::kSize, MAP_SPACE);
  if (!allocation.To(&result)) return allocation;
  // Map::cast cannot be used due to uninitialized map field.
  Map* map = reinterpret_cast<Map*>(result);
  map->set_map_after_allocation(reinterpret_cast<Map*>(root(kMetaMapRootIndex)),
                                SKIP_WRITE_BARRIER);
  map->set_instance_type(instance_type);
  map->set_instance_size(instance_size);
  // Initialize to only containing tagged fields.
  if (FLAG_unbox_double_fields) {
    map->set_layout_descriptor(LayoutDescriptor::FastPointerLayout());
  }
  // GetVisitorId requires a properly initialized LayoutDescriptor.
  map->set_visitor_id(Map::GetVisitorId(map));
  map->set_inobject_properties_start_or_constructor_function_index(0);
  DCHECK(!map->IsJSObjectMap());
  map->SetInObjectUnusedPropertyFields(0);
  map->set_bit_field(0);
  map->set_bit_field2(0);
  int bit_field3 = Map::EnumLengthBits::encode(kInvalidEnumCacheSentinel) |
                   Map::OwnsDescriptors::encode(true) |
                   Map::ConstructionCounter::encode(Map::kNoSlackTracking);
  map->set_bit_field3(bit_field3);
  map->set_weak_cell_cache(Smi::kZero);
  map->set_elements_kind(TERMINAL_FAST_ELEMENTS_KIND);
  return map;
}

AllocationResult Heap::AllocateMap(InstanceType instance_type,
                                   int instance_size,
                                   ElementsKind elements_kind,
                                   int inobject_properties) {
  STATIC_ASSERT(LAST_JS_OBJECT_TYPE == LAST_TYPE);
  DCHECK_IMPLIES(instance_type >= FIRST_JS_OBJECT_TYPE &&
                     !Map::CanHaveFastTransitionableElementsKind(instance_type),
                 IsDictionaryElementsKind(elements_kind) ||
                     IsTerminalElementsKind(elements_kind));
  HeapObject* result = nullptr;
  AllocationResult allocation = AllocateRaw(Map::kSize, MAP_SPACE);
  if (!allocation.To(&result)) return allocation;

  isolate()->counters()->maps_created()->Increment();
  result->set_map_after_allocation(meta_map(), SKIP_WRITE_BARRIER);
  Map* map = Map::cast(result);
  map->set_instance_type(instance_type);
  map->set_prototype(null_value(), SKIP_WRITE_BARRIER);
  map->set_constructor_or_backpointer(null_value(), SKIP_WRITE_BARRIER);
  map->set_instance_size(instance_size);
  if (map->IsJSObjectMap()) {
    map->SetInObjectPropertiesStartInWords(instance_size / kPointerSize -
                                           inobject_properties);
    DCHECK_EQ(map->GetInObjectProperties(), inobject_properties);
  } else {
    DCHECK_EQ(inobject_properties, 0);
    map->set_inobject_properties_start_or_constructor_function_index(0);
  }
  map->set_dependent_code(DependentCode::cast(empty_fixed_array()),
                          SKIP_WRITE_BARRIER);
  map->set_weak_cell_cache(Smi::kZero);
  map->set_raw_transitions(Smi::kZero);
  map->SetInObjectUnusedPropertyFields(inobject_properties);
  map->set_instance_descriptors(empty_descriptor_array());
  if (FLAG_unbox_double_fields) {
    map->set_layout_descriptor(LayoutDescriptor::FastPointerLayout());
  }
  // Must be called only after |instance_type|, |instance_size| and
  // |layout_descriptor| are set.
  map->set_visitor_id(Map::GetVisitorId(map));
  map->set_bit_field(0);
  map->set_bit_field2(1 << Map::kIsExtensible);
  int bit_field3 = Map::EnumLengthBits::encode(kInvalidEnumCacheSentinel) |
                   Map::OwnsDescriptors::encode(true) |
                   Map::ConstructionCounter::encode(Map::kNoSlackTracking);
  map->set_bit_field3(bit_field3);
  map->set_elements_kind(elements_kind);
  map->set_new_target_is_base(true);

  return map;
}


AllocationResult Heap::AllocateFillerObject(int size, bool double_align,
                                            AllocationSpace space) {
  HeapObject* obj = nullptr;
  {
    AllocationAlignment align = double_align ? kDoubleAligned : kWordAligned;
    AllocationResult allocation = AllocateRaw(size, space, align);
    if (!allocation.To(&obj)) return allocation;
  }
#ifdef DEBUG
  MemoryChunk* chunk = MemoryChunk::FromAddress(obj->address());
  DCHECK(chunk->owner()->identity() == space);
#endif
  CreateFillerObjectAt(obj->address(), size, ClearRecordedSlots::kNo);
  return obj;
}


AllocationResult Heap::AllocateHeapNumber(MutableMode mode,
                                          PretenureFlag pretenure) {
  // Statically ensure that it is safe to allocate heap numbers in paged
  // spaces.
  int size = HeapNumber::kSize;
  STATIC_ASSERT(HeapNumber::kSize <= kMaxRegularHeapObjectSize);

  AllocationSpace space = SelectSpace(pretenure);

  HeapObject* result = nullptr;
  {
    AllocationResult allocation = AllocateRaw(size, space, kDoubleUnaligned);
    if (!allocation.To(&result)) return allocation;
  }

  Map* map = mode == MUTABLE ? mutable_heap_number_map() : heap_number_map();
  HeapObject::cast(result)->set_map_after_allocation(map, SKIP_WRITE_BARRIER);
  return result;
}

AllocationResult Heap::AllocateBigInt(int length) {
  if (length < 0 || length > BigInt::kMaxLength) {
    v8::internal::Heap::FatalProcessOutOfMemory("invalid BigInt length", true);
  }
  int size = BigInt::SizeFor(length);
  AllocationSpace space = SelectSpace(NOT_TENURED);
  HeapObject* result = nullptr;
  {
    AllocationResult allocation = AllocateRaw(size, space);
    if (!allocation.To(&result)) return allocation;
  }
  result->set_map_after_allocation(bigint_map(), SKIP_WRITE_BARRIER);
  return result;
}

AllocationResult Heap::AllocateCell(Object* value) {
  int size = Cell::kSize;
  STATIC_ASSERT(Cell::kSize <= kMaxRegularHeapObjectSize);

  HeapObject* result = nullptr;
  {
    AllocationResult allocation = AllocateRaw(size, OLD_SPACE);
    if (!allocation.To(&result)) return allocation;
  }
  result->set_map_after_allocation(cell_map(), SKIP_WRITE_BARRIER);
  Cell::cast(result)->set_value(value);
  return result;
}

AllocationResult Heap::AllocatePropertyCell(Name* name) {
  DCHECK(name->IsUniqueName());
  int size = PropertyCell::kSize;
  STATIC_ASSERT(PropertyCell::kSize <= kMaxRegularHeapObjectSize);

  HeapObject* result = nullptr;
  AllocationResult allocation = AllocateRaw(size, OLD_SPACE);
  if (!allocation.To(&result)) return allocation;

  result->set_map_after_allocation(global_property_cell_map(),
                                   SKIP_WRITE_BARRIER);
  PropertyCell* cell = PropertyCell::cast(result);
  cell->set_dependent_code(DependentCode::cast(empty_fixed_array()),
                           SKIP_WRITE_BARRIER);
  cell->set_property_details(PropertyDetails(Smi::kZero));
  cell->set_name(name);
  cell->set_value(the_hole_value());
  return result;
}


AllocationResult Heap::AllocateWeakCell(HeapObject* value) {
  int size = WeakCell::kSize;
  STATIC_ASSERT(WeakCell::kSize <= kMaxRegularHeapObjectSize);
  HeapObject* result = nullptr;
  {
    AllocationResult allocation = AllocateRaw(size, OLD_SPACE);
    if (!allocation.To(&result)) return allocation;
  }
  result->set_map_after_allocation(weak_cell_map(), SKIP_WRITE_BARRIER);
  WeakCell::cast(result)->initialize(value);
  return result;
}


AllocationResult Heap::AllocateTransitionArray(int capacity) {
  DCHECK_LT(0, capacity);
  HeapObject* raw_array = nullptr;
  {
    AllocationResult allocation = AllocateRawFixedArray(capacity, TENURED);
    if (!allocation.To(&raw_array)) return allocation;
  }
  raw_array->set_map_after_allocation(transition_array_map(),
                                      SKIP_WRITE_BARRIER);
  TransitionArray* array = TransitionArray::cast(raw_array);
  array->set_length(capacity);
  MemsetPointer(array->data_start(), undefined_value(), capacity);
  // Transition arrays are tenured. When black allocation is on we have to
  // add the transition array to the list of encountered_transition_arrays.
  if (incremental_marking()->black_allocation()) {
    mark_compact_collector()->AddTransitionArray(array);
  }
  return array;
}

void Heap::CreateJSEntryStub() {
  JSEntryStub stub(isolate(), StackFrame::ENTRY);
  set_js_entry_code(*stub.GetCode());
}


void Heap::CreateJSConstructEntryStub() {
  JSEntryStub stub(isolate(), StackFrame::CONSTRUCT_ENTRY);
  set_js_construct_entry_code(*stub.GetCode());
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

  // MacroAssembler::Abort calls (usually enabled with --debug-code) depend on
  // CEntryStub, so we need to call GenerateStubsAheadOfTime before JSEntryStub
  // is created.

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
}

bool Heap::RootCanBeWrittenAfterInitialization(Heap::RootListIndex root_index) {
  switch (root_index) {
    case kNumberStringCacheRootIndex:
    case kCodeStubsRootIndex:
    case kScriptListRootIndex:
    case kMaterializedObjectsRootIndex:
    case kMicrotaskQueueRootIndex:
    case kDetachedContextsRootIndex:
    case kWeakObjectToCodeTableRootIndex:
    case kWeakNewSpaceObjectToCodeListRootIndex:
    case kRetainedMapsRootIndex:
    case kRetainingPathTargetsRootIndex:
    case kFeedbackVectorsForProfilingToolsRootIndex:
    case kNoScriptSharedFunctionInfosRootIndex:
    case kWeakStackTraceListRootIndex:
    case kSerializedTemplatesRootIndex:
    case kSerializedGlobalProxySizesRootIndex:
    case kPublicSymbolTableRootIndex:
    case kApiSymbolTableRootIndex:
    case kApiPrivateSymbolTableRootIndex:
    case kMessageListenersRootIndex:
    case kDeserializeLazyHandlerRootIndex:
    case kDeserializeLazyHandlerWideRootIndex:
    case kDeserializeLazyHandlerExtraWideRootIndex:
// Smi values
#define SMI_ENTRY(type, name, Name) case k##Name##RootIndex:
      SMI_ROOT_LIST(SMI_ENTRY)
#undef SMI_ENTRY
    // String table
    case kStringTableRootIndex:
      return true;

    default:
      return false;
  }
}

bool Heap::RootCanBeTreatedAsConstant(RootListIndex root_index) {
  bool can_be = !RootCanBeWrittenAfterInitialization(root_index) &&
                !InNewSpace(root(root_index));
  DCHECK_IMPLIES(can_be, IsImmovable(HeapObject::cast(root(root_index))));
  return can_be;
}

int Heap::FullSizeNumberStringCacheLength() {
  // Compute the size of the number string cache based on the max newspace size.
  // The number string cache has a minimum size based on twice the initial cache
  // size to ensure that it is bigger after being made 'full size'.
  size_t number_string_cache_size = max_semi_space_size_ / 512;
  number_string_cache_size =
      Max(static_cast<size_t>(kInitialNumberStringCacheSize * 2),
          Min<size_t>(0x4000u, number_string_cache_size));
  // There is a string and a number per entry so the length is twice the number
  // of entries.
  return static_cast<int>(number_string_cache_size * 2);
}


void Heap::FlushNumberStringCache() {
  // Flush the number to string cache.
  int len = number_string_cache()->length();
  for (int i = 0; i < len; i++) {
    number_string_cache()->set_undefined(i);
  }
}


Map* Heap::MapForFixedTypedArray(ExternalArrayType array_type) {
  return Map::cast(roots_[RootIndexForFixedTypedArray(array_type)]);
}


Heap::RootListIndex Heap::RootIndexForFixedTypedArray(
    ExternalArrayType array_type) {
  switch (array_type) {
#define ARRAY_TYPE_TO_ROOT_INDEX(Type, type, TYPE, ctype, size) \
  case kExternal##Type##Array:                                  \
    return kFixed##Type##ArrayMapRootIndex;

    TYPED_ARRAYS(ARRAY_TYPE_TO_ROOT_INDEX)
#undef ARRAY_TYPE_TO_ROOT_INDEX

    default:
      UNREACHABLE();
  }
}


Heap::RootListIndex Heap::RootIndexForEmptyFixedTypedArray(
    ElementsKind elementsKind) {
  switch (elementsKind) {
#define ELEMENT_KIND_TO_ROOT_INDEX(Type, type, TYPE, ctype, size) \
  case TYPE##_ELEMENTS:                                           \
    return kEmptyFixed##Type##ArrayRootIndex;

    TYPED_ARRAYS(ELEMENT_KIND_TO_ROOT_INDEX)
#undef ELEMENT_KIND_TO_ROOT_INDEX
    default:
      UNREACHABLE();
  }
}

FixedTypedArrayBase* Heap::EmptyFixedTypedArrayForMap(const Map* map) {
  return FixedTypedArrayBase::cast(
      roots_[RootIndexForEmptyFixedTypedArray(map->elements_kind())]);
}


AllocationResult Heap::AllocateForeign(Address address,
                                       PretenureFlag pretenure) {
  // Statically ensure that it is safe to allocate foreigns in paged spaces.
  STATIC_ASSERT(Foreign::kSize <= kMaxRegularHeapObjectSize);
  AllocationSpace space = (pretenure == TENURED) ? OLD_SPACE : NEW_SPACE;
  Foreign* result = nullptr;
  AllocationResult allocation = Allocate(foreign_map(), space);
  if (!allocation.To(&result)) return allocation;
  result->set_foreign_address(address);
  return result;
}

AllocationResult Heap::AllocateSmallOrderedHashSet(int capacity,
                                                   PretenureFlag pretenure) {
  DCHECK_EQ(0, capacity % SmallOrderedHashSet::kLoadFactor);
  CHECK_GE(SmallOrderedHashSet::kMaxCapacity, capacity);

  int size = SmallOrderedHashSet::Size(capacity);
  AllocationSpace space = SelectSpace(pretenure);
  HeapObject* result = nullptr;
  {
    AllocationResult allocation = AllocateRaw(size, space);
    if (!allocation.To(&result)) return allocation;
  }

  result->set_map_after_allocation(small_ordered_hash_set_map(),
                                   SKIP_WRITE_BARRIER);
  Handle<SmallOrderedHashSet> table(SmallOrderedHashSet::cast(result));
  table->Initialize(isolate(), capacity);
  return result;
}

AllocationResult Heap::AllocateSmallOrderedHashMap(int capacity,
                                                   PretenureFlag pretenure) {
  DCHECK_EQ(0, capacity % SmallOrderedHashMap::kLoadFactor);
  CHECK_GE(SmallOrderedHashMap::kMaxCapacity, capacity);

  int size = SmallOrderedHashMap::Size(capacity);
  AllocationSpace space = SelectSpace(pretenure);
  HeapObject* result = nullptr;
  {
    AllocationResult allocation = AllocateRaw(size, space);
    if (!allocation.To(&result)) return allocation;
  }

  result->set_map_after_allocation(small_ordered_hash_map_map(),
                                   SKIP_WRITE_BARRIER);
  Handle<SmallOrderedHashMap> table(SmallOrderedHashMap::cast(result));
  table->Initialize(isolate(), capacity);
  return result;
}

AllocationResult Heap::AllocateByteArray(int length, PretenureFlag pretenure) {
  if (length < 0 || length > ByteArray::kMaxLength) {
    v8::internal::Heap::FatalProcessOutOfMemory("invalid array length", true);
  }
  int size = ByteArray::SizeFor(length);
  AllocationSpace space = SelectSpace(pretenure);
  HeapObject* result = nullptr;
  {
    AllocationResult allocation = AllocateRaw(size, space);
    if (!allocation.To(&result)) return allocation;
  }

  result->set_map_after_allocation(byte_array_map(), SKIP_WRITE_BARRIER);
  ByteArray::cast(result)->set_length(length);
  ByteArray::cast(result)->clear_padding();
  return result;
}


AllocationResult Heap::AllocateBytecodeArray(int length,
                                             const byte* const raw_bytecodes,
                                             int frame_size,
                                             int parameter_count,
                                             FixedArray* constant_pool) {
  if (length < 0 || length > BytecodeArray::kMaxLength) {
    v8::internal::Heap::FatalProcessOutOfMemory("invalid array length", true);
  }
  // Bytecode array is pretenured, so constant pool array should be to.
  DCHECK(!InNewSpace(constant_pool));

  int size = BytecodeArray::SizeFor(length);
  HeapObject* result = nullptr;
  {
    AllocationResult allocation = AllocateRaw(size, OLD_SPACE);
    if (!allocation.To(&result)) return allocation;
  }

  result->set_map_after_allocation(bytecode_array_map(), SKIP_WRITE_BARRIER);
  BytecodeArray* instance = BytecodeArray::cast(result);
  instance->set_length(length);
  instance->set_frame_size(frame_size);
  instance->set_parameter_count(parameter_count);
  instance->set_incoming_new_target_or_generator_register(
      interpreter::Register::invalid_value());
  instance->set_interrupt_budget(interpreter::Interpreter::kInterruptBudget);
  instance->set_osr_loop_nesting_level(0);
  instance->set_bytecode_age(BytecodeArray::kNoAgeBytecodeAge);
  instance->set_constant_pool(constant_pool);
  instance->set_handler_table(empty_fixed_array());
  instance->set_source_position_table(empty_byte_array());
  CopyBytes(instance->GetFirstBytecodeAddress(), raw_bytecodes, length);
  instance->clear_padding();

  return result;
}

HeapObject* Heap::CreateFillerObjectAt(Address addr, int size,
                                       ClearRecordedSlots mode) {
  if (size == 0) return nullptr;
  HeapObject* filler = HeapObject::FromAddress(addr);
  if (size == kPointerSize) {
    filler->set_map_after_allocation(
        reinterpret_cast<Map*>(root(kOnePointerFillerMapRootIndex)),
        SKIP_WRITE_BARRIER);
  } else if (size == 2 * kPointerSize) {
    filler->set_map_after_allocation(
        reinterpret_cast<Map*>(root(kTwoPointerFillerMapRootIndex)),
        SKIP_WRITE_BARRIER);
  } else {
    DCHECK_GT(size, 2 * kPointerSize);
    filler->set_map_after_allocation(
        reinterpret_cast<Map*>(root(kFreeSpaceMapRootIndex)),
        SKIP_WRITE_BARRIER);
    FreeSpace::cast(filler)->relaxed_write_size(size);
  }
  if (mode == ClearRecordedSlots::kYes) {
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

FixedArrayBase* Heap::LeftTrimFixedArray(FixedArrayBase* object,
                                         int elements_to_trim) {
  CHECK_NOT_NULL(object);
  DCHECK(CanMoveObjectStart(object));
  DCHECK(!object->IsFixedTypedArrayBase());
  DCHECK(!object->IsByteArray());
  const int element_size = object->IsFixedArray() ? kPointerSize : kDoubleSize;
  const int bytes_to_trim = elements_to_trim * element_size;
  Map* map = object->map();

  // For now this trick is only applied to objects in new and paged space.
  // In large object space the object's start must coincide with chunk
  // and thus the trick is just not applicable.
  DCHECK(!lo_space()->Contains(object));
  DCHECK(object->map() != fixed_cow_array_map());

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

  // Notify the heap profiler of change in object layout.
  OnMoveEvent(new_object, object, new_object->Size());
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
    bytes_to_trim = elements_to_trim * kPointerSize;
  } else {
    DCHECK(object->IsFixedDoubleArray());
    bytes_to_trim = elements_to_trim * kDoubleSize;
  }


  // For now this trick is only applied to objects in new and paged space.
  DCHECK(object->map() != fixed_cow_array_map());

  if (bytes_to_trim == 0) {
    // No need to create filler and update live bytes counters, just initialize
    // header of the trimmed array.
    object->synchronized_set_length(len - elements_to_trim);
    return;
  }

  // Calculate location of new array end.
  Address old_end = object->address() + object->Size();
  Address new_end = old_end - bytes_to_trim;

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
  object->synchronized_set_length(len - elements_to_trim);

  // Notify the heap profiler of change in object layout. The array may not be
  // moved during GC, and size has to be adjusted nevertheless.
  HeapProfiler* profiler = isolate()->heap_profiler();
  if (profiler->is_tracking_allocations()) {
    profiler->UpdateObjectSizeEvent(object->address(), object->Size());
  }
}


AllocationResult Heap::AllocateFixedTypedArrayWithExternalPointer(
    int length, ExternalArrayType array_type, void* external_pointer,
    PretenureFlag pretenure) {
  int size = FixedTypedArrayBase::kHeaderSize;
  AllocationSpace space = SelectSpace(pretenure);
  HeapObject* result = nullptr;
  {
    AllocationResult allocation = AllocateRaw(size, space);
    if (!allocation.To(&result)) return allocation;
  }

  result->set_map_after_allocation(MapForFixedTypedArray(array_type),
                                   SKIP_WRITE_BARRIER);
  FixedTypedArrayBase* elements = FixedTypedArrayBase::cast(result);
  elements->set_base_pointer(Smi::kZero, SKIP_WRITE_BARRIER);
  elements->set_external_pointer(external_pointer, SKIP_WRITE_BARRIER);
  elements->set_length(length);
  return elements;
}

static void ForFixedTypedArray(ExternalArrayType array_type, int* element_size,
                               ElementsKind* element_kind) {
  switch (array_type) {
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size) \
  case kExternal##Type##Array:                          \
    *element_size = size;                               \
    *element_kind = TYPE##_ELEMENTS;                    \
    return;

    TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE

    default:
      *element_size = 0;               // Bogus
      *element_kind = UINT8_ELEMENTS;  // Bogus
      UNREACHABLE();
  }
}


AllocationResult Heap::AllocateFixedTypedArray(int length,
                                               ExternalArrayType array_type,
                                               bool initialize,
                                               PretenureFlag pretenure) {
  int element_size;
  ElementsKind elements_kind;
  ForFixedTypedArray(array_type, &element_size, &elements_kind);
  int size = OBJECT_POINTER_ALIGN(length * element_size +
                                  FixedTypedArrayBase::kDataOffset);
  AllocationSpace space = SelectSpace(pretenure);

  HeapObject* object = nullptr;
  AllocationResult allocation = AllocateRaw(
      size, space,
      array_type == kExternalFloat64Array ? kDoubleAligned : kWordAligned);
  if (!allocation.To(&object)) return allocation;

  object->set_map_after_allocation(MapForFixedTypedArray(array_type),
                                   SKIP_WRITE_BARRIER);
  FixedTypedArrayBase* elements = FixedTypedArrayBase::cast(object);
  elements->set_base_pointer(elements, SKIP_WRITE_BARRIER);
  elements->set_external_pointer(
      ExternalReference::fixed_typed_array_base_data_offset().address(),
      SKIP_WRITE_BARRIER);
  elements->set_length(length);
  if (initialize) memset(elements->DataPtr(), 0, elements->DataSize());
  return elements;
}

AllocationResult Heap::AllocateCode(int object_size, Movability movability) {
  DCHECK(IsAligned(static_cast<intptr_t>(object_size), kCodeAlignment));
  AllocationResult allocation = AllocateRaw(object_size, CODE_SPACE);

  HeapObject* result = nullptr;
  if (!allocation.To(&result)) return allocation;
  if (movability == kImmovable) {
    Address address = result->address();
    MemoryChunk* chunk = MemoryChunk::FromAddress(address);
    // Code objects which should stay at a fixed address are allocated either
    // in the first page of code space, in large object space, or (during
    // snapshot creation) the containing page is marked as immovable.
    if (!Heap::IsImmovable(result)) {
      if (isolate()->serializer_enabled() ||
          code_space_->FirstPage()->Contains(address)) {
        chunk->MarkNeverEvacuate();
      } else {
        // Discard the first code allocation, which was on a page where it could
        // be moved.
        CreateFillerObjectAt(result->address(), object_size,
                             ClearRecordedSlots::kNo);
        allocation = lo_space_->AllocateRaw(object_size, EXECUTABLE);
        if (!allocation.To(&result)) return allocation;
        OnAllocationEvent(result, object_size);
      }
    }
  }

  result->set_map_after_allocation(code_map(), SKIP_WRITE_BARRIER);
  Code* code = Code::cast(result);
  DCHECK(IsAligned(bit_cast<intptr_t>(code->address()), kCodeAlignment));
  DCHECK(!memory_allocator()->code_range()->valid() ||
         memory_allocator()->code_range()->contains(code->address()) ||
         object_size <= code_space()->AreaSize());
  return code;
}

AllocationResult Heap::AllocateCode(
    const CodeDesc& desc, Code::Kind kind, Handle<Object> self_ref,
    int32_t builtin_index, ByteArray* reloc_info,
    CodeDataContainer* data_container, HandlerTable* handler_table,
    ByteArray* source_position_table, DeoptimizationData* deopt_data,
    Movability movability, uint32_t stub_key, bool is_turbofanned,
    int stack_slots, int safepoint_table_offset) {
  bool has_unwinding_info = desc.unwinding_info != nullptr;
  DCHECK((has_unwinding_info && desc.unwinding_info_size > 0) ||
         (!has_unwinding_info && desc.unwinding_info_size == 0));

  // Compute size.
  int body_size = desc.instr_size;
  int unwinding_info_size_field_size = kInt64Size;
  if (has_unwinding_info) {
    body_size = RoundUp(body_size, kInt64Size) + desc.unwinding_info_size +
                unwinding_info_size_field_size;
  }
  int object_size = Code::SizeFor(RoundUp(body_size, kObjectAlignment));

  Code* code = nullptr;
  CodeSpaceMemoryModificationScope code_allocation(this);
  AllocationResult allocation = AllocateCode(object_size, movability);
  if (!allocation.To(&code)) return allocation;

  // The code object has not been fully initialized yet.  We rely on the
  // fact that no allocation will happen from this point on.
  DisallowHeapAllocation no_gc;
  code->set_instruction_size(desc.instr_size);
  code->set_relocation_info(reloc_info);
  code->initialize_flags(kind, has_unwinding_info, is_turbofanned, stack_slots);
  code->set_safepoint_table_offset(safepoint_table_offset);
  code->set_code_data_container(data_container);
  code->set_has_tagged_params(true);
  code->set_deoptimization_data(deopt_data);
  code->set_stub_key(stub_key);
  code->set_handler_table(handler_table);
  code->set_source_position_table(source_position_table);
  code->set_protected_instructions(empty_fixed_array(), SKIP_WRITE_BARRIER);
  code->set_constant_pool_offset(desc.instr_size - desc.constant_pool_size);
  code->set_builtin_index(builtin_index);
  code->set_trap_handler_index(Smi::FromInt(-1));

  switch (code->kind()) {
    case Code::OPTIMIZED_FUNCTION:
      code->set_marked_for_deoptimization(false);
      break;
    case Code::JS_TO_WASM_FUNCTION:
    case Code::C_WASM_ENTRY:
    case Code::WASM_FUNCTION:
      code->set_has_tagged_params(false);
      break;
    default:
      break;
  }

  // Allow self references to created code object by patching the handle to
  // point to the newly allocated Code object.
  if (!self_ref.is_null()) *(self_ref.location()) = code;

  // Migrate generated code.
  // The generated code can contain Object** values (typically from handles)
  // that are dereferenced during the copy to point directly to the actual heap
  // objects. These pointers can include references to the code object itself,
  // through the self_reference parameter.
  code->CopyFrom(desc);

  code->clear_padding();

#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) code->ObjectVerify();
#endif
  DCHECK(IsAligned(bit_cast<intptr_t>(code->address()), kCodeAlignment));
  DCHECK(!memory_allocator()->code_range()->valid() ||
         memory_allocator()->code_range()->contains(code->address()) ||
         object_size <= code_space()->AreaSize());
  return code;
}

AllocationResult Heap::CopyCode(Code* code, CodeDataContainer* data_container) {
  AllocationResult allocation;

  HeapObject* result = nullptr;
  // Allocate an object the same size as the code object.
  int obj_size = code->Size();
  allocation = AllocateRaw(obj_size, CODE_SPACE);
  if (!allocation.To(&result)) return allocation;

  // Copy code object.
  Address old_addr = code->address();
  Address new_addr = result->address();
  CopyBlock(new_addr, old_addr, obj_size);
  Code* new_code = Code::cast(result);

  // Set the {CodeDataContainer}, it cannot be shared.
  new_code->set_code_data_container(data_container);

  // Clear the trap handler index since they can't be shared between code. We
  // have to do this before calling Relocate because relocate would adjust the
  // base pointer for the old code.
  new_code->set_trap_handler_index(Smi::FromInt(trap_handler::kInvalidIndex));

  // Relocate the copy.
  new_code->Relocate(new_addr - old_addr);
  // We have to iterate over the object and process its pointers when black
  // allocation is on.
  incremental_marking()->ProcessBlackAllocatedObject(new_code);
  // Record all references to embedded objects in the new code object.
  RecordWritesIntoCode(new_code);

#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) new_code->ObjectVerify();
#endif
  DCHECK(IsAligned(bit_cast<intptr_t>(new_code->address()), kCodeAlignment));
  DCHECK(!memory_allocator()->code_range()->valid() ||
         memory_allocator()->code_range()->contains(new_code->address()) ||
         obj_size <= code_space()->AreaSize());
  return new_code;
}

AllocationResult Heap::CopyBytecodeArray(BytecodeArray* bytecode_array) {
  int size = BytecodeArray::SizeFor(bytecode_array->length());
  HeapObject* result = nullptr;
  {
    AllocationResult allocation = AllocateRaw(size, OLD_SPACE);
    if (!allocation.To(&result)) return allocation;
  }

  result->set_map_after_allocation(bytecode_array_map(), SKIP_WRITE_BARRIER);
  BytecodeArray* copy = BytecodeArray::cast(result);
  copy->set_length(bytecode_array->length());
  copy->set_frame_size(bytecode_array->frame_size());
  copy->set_parameter_count(bytecode_array->parameter_count());
  copy->set_incoming_new_target_or_generator_register(
      bytecode_array->incoming_new_target_or_generator_register());
  copy->set_constant_pool(bytecode_array->constant_pool());
  copy->set_handler_table(bytecode_array->handler_table());
  copy->set_source_position_table(bytecode_array->source_position_table());
  copy->set_interrupt_budget(bytecode_array->interrupt_budget());
  copy->set_osr_loop_nesting_level(bytecode_array->osr_loop_nesting_level());
  copy->set_bytecode_age(bytecode_array->bytecode_age());
  bytecode_array->CopyBytecodesTo(copy);
  return copy;
}

void Heap::InitializeAllocationMemento(AllocationMemento* memento,
                                       AllocationSite* allocation_site) {
  memento->set_map_after_allocation(allocation_memento_map(),
                                    SKIP_WRITE_BARRIER);
  DCHECK(allocation_site->map() == allocation_site_map());
  memento->set_allocation_site(allocation_site, SKIP_WRITE_BARRIER);
  if (FLAG_allocation_site_pretenuring) {
    allocation_site->IncrementMementoCreateCount();
  }
}


AllocationResult Heap::Allocate(Map* map, AllocationSpace space,
                                AllocationSite* allocation_site) {
  DCHECK(gc_state_ == NOT_IN_GC);
  DCHECK(map->instance_type() != MAP_TYPE);
  int size = map->instance_size();
  if (allocation_site != nullptr) {
    size += AllocationMemento::kSize;
  }
  HeapObject* result = nullptr;
  AllocationResult allocation = AllocateRaw(size, space);
  if (!allocation.To(&result)) return allocation;
  // New space objects are allocated white.
  WriteBarrierMode write_barrier_mode =
      space == NEW_SPACE ? SKIP_WRITE_BARRIER : UPDATE_WRITE_BARRIER;
  result->set_map_after_allocation(map, write_barrier_mode);
  if (allocation_site != nullptr) {
    AllocationMemento* alloc_memento = reinterpret_cast<AllocationMemento*>(
        reinterpret_cast<Address>(result) + map->instance_size());
    InitializeAllocationMemento(alloc_memento, allocation_site);
  }
  return result;
}

void Heap::InitializeJSObjectFromMap(JSObject* obj, Object* properties,
                                     Map* map) {
  obj->set_raw_properties_or_hash(properties);
  obj->initialize_elements();
  // TODO(1240798): Initialize the object's body using valid initial values
  // according to the object's initial map.  For example, if the map's
  // instance type is JS_ARRAY_TYPE, the length field should be initialized
  // to a number (e.g. Smi::kZero) and the elements initialized to a
  // fixed array (e.g. Heap::empty_fixed_array()).  Currently, the object
  // verification code has to cope with (temporarily) invalid objects.  See
  // for example, JSArray::JSArrayVerify).
  InitializeJSObjectBody(obj, map, JSObject::kHeaderSize);
}


void Heap::InitializeJSObjectBody(JSObject* obj, Map* map, int start_offset) {
  if (start_offset == map->instance_size()) return;
  DCHECK_LT(start_offset, map->instance_size());

  // We cannot always fill with one_pointer_filler_map because objects
  // created from API functions expect their embedder fields to be initialized
  // with undefined_value.
  // Pre-allocated fields need to be initialized with undefined_value as well
  // so that object accesses before the constructor completes (e.g. in the
  // debugger) will not cause a crash.

  // In case of Array subclassing the |map| could already be transitioned
  // to different elements kind from the initial map on which we track slack.
  bool in_progress = map->IsInobjectSlackTrackingInProgress();
  Object* filler;
  if (in_progress) {
    filler = one_pointer_filler_map();
  } else {
    filler = undefined_value();
  }
  obj->InitializeBody(map, start_offset, Heap::undefined_value(), filler);
  if (in_progress) {
    map->FindRootMap()->InobjectSlackTrackingStep();
  }
}


AllocationResult Heap::AllocateJSObjectFromMap(
    Map* map, PretenureFlag pretenure, AllocationSite* allocation_site) {
  // JSFunctions should be allocated using AllocateFunction to be
  // properly initialized.
  DCHECK(map->instance_type() != JS_FUNCTION_TYPE);

  // Both types of global objects should be allocated using
  // AllocateGlobalObject to be properly initialized.
  DCHECK(map->instance_type() != JS_GLOBAL_OBJECT_TYPE);

  // Allocate the backing storage for the properties.
  FixedArray* properties = empty_fixed_array();

  // Allocate the JSObject.
  AllocationSpace space = SelectSpace(pretenure);
  JSObject* js_obj = nullptr;
  AllocationResult allocation = Allocate(map, space, allocation_site);
  if (!allocation.To(&js_obj)) return allocation;

  // Initialize the JSObject.
  InitializeJSObjectFromMap(js_obj, properties, map);
  DCHECK(js_obj->HasFastElements() || js_obj->HasFixedTypedArrayElements() ||
         js_obj->HasFastStringWrapperElements() ||
         js_obj->HasFastArgumentsElements());
  return js_obj;
}


AllocationResult Heap::AllocateJSObject(JSFunction* constructor,
                                        PretenureFlag pretenure,
                                        AllocationSite* allocation_site) {
  DCHECK(constructor->has_initial_map());

  // Allocate the object based on the constructors initial map.
  AllocationResult allocation = AllocateJSObjectFromMap(
      constructor->initial_map(), pretenure, allocation_site);
#ifdef DEBUG
  // Make sure result is NOT a global object if valid.
  HeapObject* obj = nullptr;
  DCHECK(!allocation.To(&obj) || !obj->IsJSGlobalObject());
#endif
  return allocation;
}


AllocationResult Heap::CopyJSObject(JSObject* source, AllocationSite* site) {
  // Make the clone.
  Map* map = source->map();

  // We can only clone regexps, normal objects, api objects, errors or arrays.
  // Copying anything else will break invariants.
  CHECK(map->instance_type() == JS_REGEXP_TYPE ||
        map->instance_type() == JS_OBJECT_TYPE ||
        map->instance_type() == JS_ERROR_TYPE ||
        map->instance_type() == JS_ARRAY_TYPE ||
        map->instance_type() == JS_API_OBJECT_TYPE ||
        map->instance_type() == WASM_INSTANCE_TYPE ||
        map->instance_type() == WASM_MEMORY_TYPE ||
        map->instance_type() == WASM_MODULE_TYPE ||
        map->instance_type() == WASM_TABLE_TYPE ||
        map->instance_type() == JS_SPECIAL_API_OBJECT_TYPE);

  int object_size = map->instance_size();
  HeapObject* clone = nullptr;

  DCHECK(site == nullptr || AllocationSite::CanTrack(map->instance_type()));

  int adjusted_object_size =
      site != nullptr ? object_size + AllocationMemento::kSize : object_size;
  AllocationResult allocation = AllocateRaw(adjusted_object_size, NEW_SPACE);
  if (!allocation.To(&clone)) return allocation;

  SLOW_DCHECK(InNewSpace(clone));
  // Since we know the clone is allocated in new space, we can copy
  // the contents without worrying about updating the write barrier.
  CopyBlock(clone->address(), source->address(), object_size);

  if (site != nullptr) {
    AllocationMemento* alloc_memento = reinterpret_cast<AllocationMemento*>(
        reinterpret_cast<Address>(clone) + object_size);
    InitializeAllocationMemento(alloc_memento, site);
  }

  SLOW_DCHECK(JSObject::cast(clone)->GetElementsKind() ==
              source->GetElementsKind());
  FixedArrayBase* elements = FixedArrayBase::cast(source->elements());
  // Update elements if necessary.
  if (elements->length() > 0) {
    FixedArrayBase* elem = nullptr;
    {
      AllocationResult allocation;
      if (elements->map() == fixed_cow_array_map()) {
        allocation = FixedArray::cast(elements);
      } else if (source->HasDoubleElements()) {
        allocation = CopyFixedDoubleArray(FixedDoubleArray::cast(elements));
      } else {
        allocation = CopyFixedArray(FixedArray::cast(elements));
      }
      if (!allocation.To(&elem)) return allocation;
    }
    JSObject::cast(clone)->set_elements(elem, SKIP_WRITE_BARRIER);
  }

  // Update properties if necessary.
  if (source->HasFastProperties()) {
    if (source->property_array()->length() > 0) {
      PropertyArray* properties = source->property_array();
      PropertyArray* prop = nullptr;
      {
        // TODO(gsathya): Do not copy hash code.
        AllocationResult allocation = CopyPropertyArray(properties);
        if (!allocation.To(&prop)) return allocation;
      }
      JSObject::cast(clone)->set_raw_properties_or_hash(prop,
                                                        SKIP_WRITE_BARRIER);
    }
  } else {
    FixedArray* properties = FixedArray::cast(source->property_dictionary());
    FixedArray* prop = nullptr;
    {
      AllocationResult allocation = CopyFixedArray(properties);
      if (!allocation.To(&prop)) return allocation;
    }
    JSObject::cast(clone)->set_raw_properties_or_hash(prop, SKIP_WRITE_BARRIER);
  }
  // Return the new clone.
  return clone;
}


static inline void WriteOneByteData(Vector<const char> vector, uint8_t* chars,
                                    int len) {
  // Only works for one byte strings.
  DCHECK(vector.length() == len);
  MemCopy(chars, vector.start(), len);
}

static inline void WriteTwoByteData(Vector<const char> vector, uint16_t* chars,
                                    int len) {
  const uint8_t* stream = reinterpret_cast<const uint8_t*>(vector.start());
  size_t stream_length = vector.length();
  while (stream_length != 0) {
    size_t consumed = 0;
    uint32_t c = unibrow::Utf8::ValueOf(stream, stream_length, &consumed);
    DCHECK_NE(unibrow::Utf8::kBadChar, c);
    DCHECK(consumed <= stream_length);
    stream_length -= consumed;
    stream += consumed;
    if (c > unibrow::Utf16::kMaxNonSurrogateCharCode) {
      len -= 2;
      if (len < 0) break;
      *chars++ = unibrow::Utf16::LeadSurrogate(c);
      *chars++ = unibrow::Utf16::TrailSurrogate(c);
    } else {
      len -= 1;
      if (len < 0) break;
      *chars++ = c;
    }
  }
  DCHECK_EQ(0, stream_length);
  DCHECK_EQ(0, len);
}


static inline void WriteOneByteData(String* s, uint8_t* chars, int len) {
  DCHECK(s->length() == len);
  String::WriteToFlat(s, chars, 0, len);
}


static inline void WriteTwoByteData(String* s, uint16_t* chars, int len) {
  DCHECK(s->length() == len);
  String::WriteToFlat(s, chars, 0, len);
}


template <bool is_one_byte, typename T>
AllocationResult Heap::AllocateInternalizedStringImpl(T t, int chars,
                                                      uint32_t hash_field) {
  DCHECK_LE(0, chars);
  // Compute map and object size.
  int size;
  Map* map;

  DCHECK_LE(0, chars);
  DCHECK_GE(String::kMaxLength, chars);
  if (is_one_byte) {
    map = one_byte_internalized_string_map();
    size = SeqOneByteString::SizeFor(chars);
  } else {
    map = internalized_string_map();
    size = SeqTwoByteString::SizeFor(chars);
  }

  // Allocate string.
  HeapObject* result = nullptr;
  {
    AllocationResult allocation = AllocateRaw(size, OLD_SPACE);
    if (!allocation.To(&result)) return allocation;
  }

  result->set_map_after_allocation(map, SKIP_WRITE_BARRIER);
  // Set length and hash fields of the allocated string.
  String* answer = String::cast(result);
  answer->set_length(chars);
  answer->set_hash_field(hash_field);

  DCHECK_EQ(size, answer->Size());

  if (is_one_byte) {
    WriteOneByteData(t, SeqOneByteString::cast(answer)->GetChars(), chars);
  } else {
    WriteTwoByteData(t, SeqTwoByteString::cast(answer)->GetChars(), chars);
  }
  return answer;
}


// Need explicit instantiations.
template AllocationResult Heap::AllocateInternalizedStringImpl<true>(String*,
                                                                     int,
                                                                     uint32_t);
template AllocationResult Heap::AllocateInternalizedStringImpl<false>(String*,
                                                                      int,
                                                                      uint32_t);
template AllocationResult Heap::AllocateInternalizedStringImpl<false>(
    Vector<const char>, int, uint32_t);


AllocationResult Heap::AllocateRawOneByteString(int length,
                                                PretenureFlag pretenure) {
  DCHECK_LE(0, length);
  DCHECK_GE(String::kMaxLength, length);
  int size = SeqOneByteString::SizeFor(length);
  DCHECK_GE(SeqOneByteString::kMaxSize, size);
  AllocationSpace space = SelectSpace(pretenure);

  HeapObject* result = nullptr;
  {
    AllocationResult allocation = AllocateRaw(size, space);
    if (!allocation.To(&result)) return allocation;
  }

  // Partially initialize the object.
  result->set_map_after_allocation(one_byte_string_map(), SKIP_WRITE_BARRIER);
  String::cast(result)->set_length(length);
  String::cast(result)->set_hash_field(String::kEmptyHashField);
  DCHECK_EQ(size, HeapObject::cast(result)->Size());

  return result;
}


AllocationResult Heap::AllocateRawTwoByteString(int length,
                                                PretenureFlag pretenure) {
  DCHECK_LE(0, length);
  DCHECK_GE(String::kMaxLength, length);
  int size = SeqTwoByteString::SizeFor(length);
  DCHECK_GE(SeqTwoByteString::kMaxSize, size);
  AllocationSpace space = SelectSpace(pretenure);

  HeapObject* result = nullptr;
  {
    AllocationResult allocation = AllocateRaw(size, space);
    if (!allocation.To(&result)) return allocation;
  }

  // Partially initialize the object.
  result->set_map_after_allocation(string_map(), SKIP_WRITE_BARRIER);
  String::cast(result)->set_length(length);
  String::cast(result)->set_hash_field(String::kEmptyHashField);
  DCHECK_EQ(size, HeapObject::cast(result)->Size());
  return result;
}


AllocationResult Heap::AllocateEmptyFixedArray() {
  int size = FixedArray::SizeFor(0);
  HeapObject* result = nullptr;
  {
    AllocationResult allocation = AllocateRaw(size, OLD_SPACE);
    if (!allocation.To(&result)) return allocation;
  }
  // Initialize the object.
  result->set_map_after_allocation(fixed_array_map(), SKIP_WRITE_BARRIER);
  FixedArray::cast(result)->set_length(0);
  return result;
}

AllocationResult Heap::AllocateEmptyScopeInfo() {
  int size = FixedArray::SizeFor(0);
  HeapObject* result = nullptr;
  {
    AllocationResult allocation = AllocateRaw(size, OLD_SPACE);
    if (!allocation.To(&result)) return allocation;
  }
  // Initialize the object.
  result->set_map_after_allocation(scope_info_map(), SKIP_WRITE_BARRIER);
  FixedArray::cast(result)->set_length(0);
  return result;
}

AllocationResult Heap::CopyAndTenureFixedCOWArray(FixedArray* src) {
  if (!InNewSpace(src)) {
    return src;
  }

  int len = src->length();
  HeapObject* obj = nullptr;
  {
    AllocationResult allocation = AllocateRawFixedArray(len, TENURED);
    if (!allocation.To(&obj)) return allocation;
  }
  obj->set_map_after_allocation(fixed_array_map(), SKIP_WRITE_BARRIER);
  FixedArray* result = FixedArray::cast(obj);
  result->set_length(len);

  // Copy the content.
  DisallowHeapAllocation no_gc;
  WriteBarrierMode mode = result->GetWriteBarrierMode(no_gc);
  for (int i = 0; i < len; i++) result->set(i, src->get(i), mode);

  // TODO(mvstanton): The map is set twice because of protection against calling
  // set() on a COW FixedArray. Issue v8:3221 created to track this, and
  // we might then be able to remove this whole method.
  HeapObject::cast(obj)->set_map_after_allocation(fixed_cow_array_map(),
                                                  SKIP_WRITE_BARRIER);
  return result;
}


AllocationResult Heap::AllocateEmptyFixedTypedArray(
    ExternalArrayType array_type) {
  return AllocateFixedTypedArray(0, array_type, false, TENURED);
}

namespace {
template <typename T>
void initialize_length(T* array, int length) {
  array->set_length(length);
}

template <>
void initialize_length<PropertyArray>(PropertyArray* array, int length) {
  array->initialize_length(length);
}

}  // namespace

template <typename T>
AllocationResult Heap::CopyArrayAndGrow(T* src, int grow_by,
                                        PretenureFlag pretenure) {
  int old_len = src->length();
  int new_len = old_len + grow_by;
  DCHECK(new_len >= old_len);
  HeapObject* obj = nullptr;
  {
    AllocationResult allocation = AllocateRawFixedArray(new_len, pretenure);
    if (!allocation.To(&obj)) return allocation;
  }

  obj->set_map_after_allocation(src->map(), SKIP_WRITE_BARRIER);
  T* result = T::cast(obj);
  initialize_length(result, new_len);

  // Copy the content.
  DisallowHeapAllocation no_gc;
  WriteBarrierMode mode = obj->GetWriteBarrierMode(no_gc);
  for (int i = 0; i < old_len; i++) result->set(i, src->get(i), mode);
  MemsetPointer(result->data_start() + old_len, undefined_value(), grow_by);
  return result;
}

template AllocationResult Heap::CopyArrayAndGrow(FixedArray* src, int grow_by,
                                                 PretenureFlag pretenure);
template AllocationResult Heap::CopyArrayAndGrow(PropertyArray* src,
                                                 int grow_by,
                                                 PretenureFlag pretenure);

AllocationResult Heap::CopyFixedArrayUpTo(FixedArray* src, int new_len,
                                          PretenureFlag pretenure) {
  if (new_len == 0) return empty_fixed_array();

  DCHECK_LE(new_len, src->length());

  HeapObject* obj = nullptr;
  {
    AllocationResult allocation = AllocateRawFixedArray(new_len, pretenure);
    if (!allocation.To(&obj)) return allocation;
  }
  obj->set_map_after_allocation(fixed_array_map(), SKIP_WRITE_BARRIER);

  FixedArray* result = FixedArray::cast(obj);
  result->set_length(new_len);

  // Copy the content.
  DisallowHeapAllocation no_gc;
  WriteBarrierMode mode = result->GetWriteBarrierMode(no_gc);
  for (int i = 0; i < new_len; i++) result->set(i, src->get(i), mode);
  return result;
}

template <typename T>
AllocationResult Heap::CopyArrayWithMap(T* src, Map* map) {
  int len = src->length();
  HeapObject* obj = nullptr;
  {
    AllocationResult allocation = AllocateRawFixedArray(len, NOT_TENURED);
    if (!allocation.To(&obj)) return allocation;
  }
  obj->set_map_after_allocation(map, SKIP_WRITE_BARRIER);

  T* result = T::cast(obj);
  DisallowHeapAllocation no_gc;
  WriteBarrierMode mode = result->GetWriteBarrierMode(no_gc);

  // Eliminate the write barrier if possible.
  if (mode == SKIP_WRITE_BARRIER) {
    CopyBlock(obj->address() + kPointerSize, src->address() + kPointerSize,
              T::SizeFor(len) - kPointerSize);
    return obj;
  }

  // Slow case: Just copy the content one-by-one.
  initialize_length(result, len);
  for (int i = 0; i < len; i++) result->set(i, src->get(i), mode);
  return result;
}

template AllocationResult Heap::CopyArrayWithMap(FixedArray* src, Map* map);
template AllocationResult Heap::CopyArrayWithMap(PropertyArray* src, Map* map);

AllocationResult Heap::CopyFixedArrayWithMap(FixedArray* src, Map* map) {
  return CopyArrayWithMap(src, map);
}

AllocationResult Heap::CopyPropertyArray(PropertyArray* src) {
  return CopyArrayWithMap(src, property_array_map());
}

AllocationResult Heap::CopyFixedDoubleArrayWithMap(FixedDoubleArray* src,
                                                   Map* map) {
  int len = src->length();
  HeapObject* obj = nullptr;
  {
    AllocationResult allocation = AllocateRawFixedDoubleArray(len, NOT_TENURED);
    if (!allocation.To(&obj)) return allocation;
  }
  obj->set_map_after_allocation(map, SKIP_WRITE_BARRIER);
  CopyBlock(obj->address() + FixedDoubleArray::kLengthOffset,
            src->address() + FixedDoubleArray::kLengthOffset,
            FixedDoubleArray::SizeFor(len) - FixedDoubleArray::kLengthOffset);
  return obj;
}

AllocationResult Heap::CopyFeedbackVector(FeedbackVector* src) {
  int len = src->length();
  HeapObject* obj = nullptr;
  {
    AllocationResult allocation = AllocateRawFeedbackVector(len, NOT_TENURED);
    if (!allocation.To(&obj)) return allocation;
  }
  obj->set_map_after_allocation(feedback_vector_map(), SKIP_WRITE_BARRIER);

  FeedbackVector* result = FeedbackVector::cast(obj);

  DisallowHeapAllocation no_gc;
  WriteBarrierMode mode = result->GetWriteBarrierMode(no_gc);

  // Eliminate the write barrier if possible.
  if (mode == SKIP_WRITE_BARRIER) {
    CopyBlock(result->address() + kPointerSize,
              result->address() + kPointerSize,
              FeedbackVector::SizeFor(len) - kPointerSize);
    return result;
  }

  // Slow case: Just copy the content one-by-one.
  result->set_shared_function_info(src->shared_function_info());
  result->set_optimized_code_cell(src->optimized_code_cell());
  result->set_invocation_count(src->invocation_count());
  result->set_profiler_ticks(src->profiler_ticks());
  result->set_deopt_count(src->deopt_count());
  for (int i = 0; i < len; i++) result->set(i, src->get(i), mode);
  return result;
}

AllocationResult Heap::AllocateRawFixedArray(int length,
                                             PretenureFlag pretenure) {
  if (length < 0 || length > FixedArray::kMaxLength) {
    v8::internal::Heap::FatalProcessOutOfMemory("invalid array length", true);
  }
  int size = FixedArray::SizeFor(length);
  AllocationSpace space = SelectSpace(pretenure);

  AllocationResult result = AllocateRaw(size, space);
  if (!result.IsRetry() && size > kMaxRegularHeapObjectSize &&
      FLAG_use_marking_progress_bar) {
    MemoryChunk* chunk =
        MemoryChunk::FromAddress(result.ToObjectChecked()->address());
    chunk->SetFlag<AccessMode::ATOMIC>(MemoryChunk::HAS_PROGRESS_BAR);
  }
  return result;
}

AllocationResult Heap::AllocateFixedArrayWithFiller(
    RootListIndex map_root_index, int length, PretenureFlag pretenure,
    Object* filler) {
  // Zero-length case must be handled outside, where the knowledge about
  // the map is.
  DCHECK_LT(0, length);
  DCHECK(!InNewSpace(filler));
  HeapObject* result = nullptr;
  {
    AllocationResult allocation = AllocateRawFixedArray(length, pretenure);
    if (!allocation.To(&result)) return allocation;
  }
  DCHECK(RootIsImmortalImmovable(map_root_index));
  Map* map = Map::cast(root(map_root_index));
  result->set_map_after_allocation(map, SKIP_WRITE_BARRIER);
  FixedArray* array = FixedArray::cast(result);
  array->set_length(length);
  MemsetPointer(array->data_start(), filler, length);
  return array;
}

AllocationResult Heap::AllocatePropertyArray(int length,
                                             PretenureFlag pretenure) {
  // Allow length = 0 for the empty_property_array singleton.
  DCHECK_LE(0, length);
  DCHECK_IMPLIES(length == 0, pretenure == TENURED);

  DCHECK(!InNewSpace(undefined_value()));
  HeapObject* result = nullptr;
  {
    AllocationResult allocation = AllocateRawFixedArray(length, pretenure);
    if (!allocation.To(&result)) return allocation;
  }

  result->set_map_after_allocation(property_array_map(), SKIP_WRITE_BARRIER);
  PropertyArray* array = PropertyArray::cast(result);
  array->initialize_length(length);
  MemsetPointer(array->data_start(), undefined_value(), length);
  return result;
}

AllocationResult Heap::AllocateUninitializedFixedArray(
    int length, PretenureFlag pretenure) {
  if (length == 0) return empty_fixed_array();

  HeapObject* obj = nullptr;
  {
    AllocationResult allocation = AllocateRawFixedArray(length, pretenure);
    if (!allocation.To(&obj)) return allocation;
  }

  obj->set_map_after_allocation(fixed_array_map(), SKIP_WRITE_BARRIER);
  FixedArray::cast(obj)->set_length(length);
  return obj;
}


AllocationResult Heap::AllocateUninitializedFixedDoubleArray(
    int length, PretenureFlag pretenure) {
  if (length == 0) return empty_fixed_array();

  HeapObject* elements = nullptr;
  AllocationResult allocation = AllocateRawFixedDoubleArray(length, pretenure);
  if (!allocation.To(&elements)) return allocation;

  elements->set_map_after_allocation(fixed_double_array_map(),
                                     SKIP_WRITE_BARRIER);
  FixedDoubleArray::cast(elements)->set_length(length);
  return elements;
}


AllocationResult Heap::AllocateRawFixedDoubleArray(int length,
                                                   PretenureFlag pretenure) {
  if (length < 0 || length > FixedDoubleArray::kMaxLength) {
    v8::internal::Heap::FatalProcessOutOfMemory("invalid array length", true);
  }
  int size = FixedDoubleArray::SizeFor(length);
  AllocationSpace space = SelectSpace(pretenure);

  HeapObject* object = nullptr;
  {
    AllocationResult allocation = AllocateRaw(size, space, kDoubleAligned);
    if (!allocation.To(&object)) return allocation;
  }

  return object;
}

AllocationResult Heap::AllocateRawFeedbackVector(int length,
                                                 PretenureFlag pretenure) {
  DCHECK_LE(0, length);

  int size = FeedbackVector::SizeFor(length);
  AllocationSpace space = SelectSpace(pretenure);

  HeapObject* object = nullptr;
  {
    AllocationResult allocation = AllocateRaw(size, space);
    if (!allocation.To(&object)) return allocation;
  }

  return object;
}

AllocationResult Heap::AllocateFeedbackVector(SharedFunctionInfo* shared,
                                              PretenureFlag pretenure) {
  int length = shared->feedback_metadata()->slot_count();

  HeapObject* result = nullptr;
  {
    AllocationResult allocation = AllocateRawFeedbackVector(length, pretenure);
    if (!allocation.To(&result)) return allocation;
  }

  // Initialize the object's map.
  result->set_map_after_allocation(feedback_vector_map(), SKIP_WRITE_BARRIER);
  FeedbackVector* vector = FeedbackVector::cast(result);
  vector->set_shared_function_info(shared);
  vector->set_optimized_code_cell(Smi::FromEnum(OptimizationMarker::kNone));
  vector->set_length(length);
  vector->set_invocation_count(0);
  vector->set_profiler_ticks(0);
  vector->set_deopt_count(0);
  // TODO(leszeks): Initialize based on the feedback metadata.
  MemsetPointer(vector->slots_start(), undefined_value(), length);
  return vector;
}

AllocationResult Heap::AllocateSymbol() {
  // Statically ensure that it is safe to allocate symbols in paged spaces.
  STATIC_ASSERT(Symbol::kSize <= kMaxRegularHeapObjectSize);

  HeapObject* result = nullptr;
  AllocationResult allocation = AllocateRaw(Symbol::kSize, OLD_SPACE);
  if (!allocation.To(&result)) return allocation;

  result->set_map_after_allocation(symbol_map(), SKIP_WRITE_BARRIER);

  // Generate a random hash value.
  int hash = isolate()->GenerateIdentityHash(Name::kHashBitMask);

  Symbol::cast(result)
      ->set_hash_field(Name::kIsNotArrayIndexMask | (hash << Name::kHashShift));
  Symbol::cast(result)->set_name(undefined_value());
  Symbol::cast(result)->set_flags(0);

  DCHECK(!Symbol::cast(result)->is_private());
  return result;
}

AllocationResult Heap::AllocateStruct(InstanceType type,
                                      PretenureFlag pretenure) {
  Map* map;
  switch (type) {
#define MAKE_CASE(NAME, Name, name) \
  case NAME##_TYPE:                 \
    map = name##_map();             \
    break;
    STRUCT_LIST(MAKE_CASE)
#undef MAKE_CASE
    default:
      UNREACHABLE();
  }
  int size = map->instance_size();
  Struct* result = nullptr;
  {
    AllocationSpace space = SelectSpace(pretenure);
    AllocationResult allocation = Allocate(map, space);
    if (!allocation.To(&result)) return allocation;
  }
  result->InitializeBody(size);
  return result;
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


bool Heap::HasHighFragmentation() {
  size_t used = PromotedSpaceSizeOfObjects();
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
  return FLAG_optimize_for_size || isolate()->IsIsolateInBackground() ||
         HighMemoryPressure();
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
    FinalizeIncrementalMarking(gc_reason);
  } else if (incremental_marking()->IsComplete() ||
             (mark_compact_collector()->marking_worklist()->IsEmpty() &&
              local_embedder_heap_tracer()
                  ->ShouldFinalizeIncrementalMarking())) {
    CollectAllGarbage(current_gc_flags_, gc_reason, current_gc_callback_flags_);
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
  DCHECK(InOldSpace(object) || InNewSpace(object) ||
         (lo_space()->Contains(object) && object->IsString()));
  if (FLAG_incremental_marking && incremental_marking()->IsMarking()) {
    incremental_marking()->MarkBlackAndPush(object);
    if (InOldSpace(object) && incremental_marking()->IsCompacting()) {
      // The concurrent marker might have recorded slots for the object.
      // Register this object as invalidated to filter out the slots.
      MemoryChunk* chunk = MemoryChunk::FromAddress(object->address());
      chunk->RegisterObjectWithInvalidatedSlots(object, size);
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
    for (Object** p = start; p < end; p++) {
      slots_.push_back(p);
    }
  }

  int number_of_slots() { return static_cast<int>(slots_.size()); }

  Object** slot(int i) { return slots_[i]; }

 private:
  std::vector<Object**> slots_;
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

  if (deadline_in_ms - start_ms >
      GCIdleTimeHandler::kMaxFrameRenderingIdleTime) {
    int committed_memory = static_cast<int>(CommittedMemory() / KB);
    int used_memory = static_cast<int>(heap_state.size_of_objects / KB);
    isolate()->counters()->aggregated_memory_heap_committed()->AddSample(
        start_ms, committed_memory);
    isolate()->counters()->aggregated_memory_heap_used()->AddSample(
        start_ms, used_memory);
  }

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

  virtual ~MemoryPressureInterruptTask() {}

 private:
  // v8::internal::CancelableTask overrides.
  void RunInternal() override { heap_->CheckMemoryPressure(); }

  Heap* heap_;
  DISALLOW_COPY_AND_ASSIGN(MemoryPressureInterruptTask);
};

void Heap::CheckMemoryPressure() {
  if (HighMemoryPressure()) {
    if (isolate()->concurrent_recompilation_enabled()) {
      // The optimizing compiler may be unnecessarily holding on to memory.
      DisallowHeapAllocation no_recursive_gc;
      isolate()->optimizing_compile_dispatcher()->Flush(
          OptimizingCompileDispatcher::BlockingBehavior::kDontBlock);
    }
  }
  if (memory_pressure_level_.Value() == MemoryPressureLevel::kCritical) {
    CollectGarbageOnMemoryPressure();
  } else if (memory_pressure_level_.Value() == MemoryPressureLevel::kModerate) {
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
  CollectAllGarbage(kReduceMemoryFootprintMask | kAbortIncrementalMarkingMask,
                    GarbageCollectionReason::kMemoryPressure,
                    kGCCallbackFlagCollectAllAvailableGarbage);
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
      CollectAllGarbage(
          kReduceMemoryFootprintMask | kAbortIncrementalMarkingMask,
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
  MemoryPressureLevel previous = memory_pressure_level_.Value();
  memory_pressure_level_.SetValue(level);
  if ((previous != MemoryPressureLevel::kCritical &&
       level == MemoryPressureLevel::kCritical) ||
      (previous == MemoryPressureLevel::kNone &&
       level == MemoryPressureLevel::kModerate)) {
    if (is_isolate_locked) {
      CheckMemoryPressure();
    } else {
      ExecutionAccess access(isolate());
      isolate()->stack_guard()->RequestGC();
      V8::GetCurrentPlatform()->CallOnForegroundThread(
          reinterpret_cast<v8::Isolate*>(isolate()),
          new MemoryPressureInterruptTask(this));
    }
  }
}

void Heap::SetOutOfMemoryCallback(v8::debug::OutOfMemoryCallback callback,
                                  void* data) {
  out_of_memory_callback_ = callback;
  out_of_memory_callback_data_ = data;
}

void Heap::InvokeOutOfMemoryCallback() {
  if (out_of_memory_callback_) {
    out_of_memory_callback_(out_of_memory_callback_data_);
  }
}

void Heap::CollectCodeStatistics() {
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
  AllSpaces spaces(this);
  for (Space* space = spaces.next(); space != nullptr; space = spaces.next()) {
    space->Print();
  }
}


void Heap::ReportCodeStatistics(const char* title) {
  PrintF(">>>>>> Code Stats (%s) >>>>>>\n", title);
  CollectCodeStatistics();
  CodeStatistics::ReportCodeStatistics(isolate());
}


// This function expects that NewSpace's allocated objects histogram is
// populated (via a call to CollectStatistics or else as a side effect of a
// just-completed scavenge collection).
void Heap::ReportHeapStatistics(const char* title) {
  USE(title);
  PrintF(">>>>>> =============== %s (%d) =============== >>>>>>\n", title,
         gc_count_);
  PrintF("old_generation_allocation_limit_ %" PRIuS "\n",
         old_generation_allocation_limit_);

  PrintF("\n");
  PrintF("Number of handles : %d\n", HandleScope::NumberOfHandles(isolate_));
  isolate_->global_handles()->PrintStats();
  PrintF("\n");

  PrintF("Heap statistics : ");
  memory_allocator()->ReportStatistics();
  PrintF("To space : ");
  new_space_->ReportStatistics();
  PrintF("Old space : ");
  old_space_->ReportStatistics();
  PrintF("Code space : ");
  code_space_->ReportStatistics();
  PrintF("Map space : ");
  map_space_->ReportStatistics();
  PrintF("Large object space : ");
  lo_space_->ReportStatistics();
  PrintF(">>>>>> ========================================= >>>>>>\n");
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
          lo_space_->Contains(value));
}

bool Heap::ContainsSlow(Address addr) {
  if (memory_allocator()->IsOutsideAllocatedSpace(addr)) {
    return false;
  }
  return HasBeenSetUp() &&
         (new_space_->ToSpaceContainsSlow(addr) ||
          old_space_->ContainsSlow(addr) || code_space_->ContainsSlow(addr) ||
          map_space_->ContainsSlow(addr) || lo_space_->ContainsSlow(addr));
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
      return true;
    default:
      return false;
  }
}


bool Heap::RootIsImmortalImmovable(int root_index) {
  switch (root_index) {
#define IMMORTAL_IMMOVABLE_ROOT(name) case Heap::k##name##RootIndex:
    IMMORTAL_IMMOVABLE_ROOT_LIST(IMMORTAL_IMMOVABLE_ROOT)
#undef IMMORTAL_IMMOVABLE_ROOT
#define INTERNALIZED_STRING(name, value) case Heap::k##name##RootIndex:
    INTERNALIZED_STRING_LIST(INTERNALIZED_STRING)
#undef INTERNALIZED_STRING
#define STRING_TYPE(NAME, size, name, Name) case Heap::k##Name##MapRootIndex:
    STRING_TYPE_LIST(STRING_TYPE)
#undef STRING_TYPE
    return true;
    default:
      return false;
  }
}

#ifdef VERIFY_HEAP
void Heap::Verify() {
  CHECK(HasBeenSetUp());
  HandleScope scope(isolate());

  // We have to wait here for the sweeper threads to have an iterable heap.
  mark_compact_collector()->EnsureSweepingCompleted();

  VerifyPointersVisitor visitor;
  IterateRoots(&visitor, VISIT_ONLY_STRONG);

  VerifySmisVisitor smis_visitor;
  IterateSmiRoots(&smis_visitor);

  new_space_->Verify();

  old_space_->Verify(&visitor);
  map_space_->Verify(&visitor);

  VerifyPointersVisitor no_dirty_regions_visitor;
  code_space_->Verify(&no_dirty_regions_visitor);

  lo_space_->Verify();

  mark_compact_collector()->VerifyWeakEmbeddedObjectsInCode();
}

class SlotVerifyingVisitor : public ObjectVisitor {
 public:
  SlotVerifyingVisitor(std::set<Address>* untyped,
                       std::set<std::pair<SlotType, Address> >* typed)
      : untyped_(untyped), typed_(typed) {}

  virtual bool ShouldHaveBeenRecorded(HeapObject* host, Object* target) = 0;

  void VisitPointers(HeapObject* host, Object** start, Object** end) override {
    for (Object** slot = start; slot < end; slot++) {
      if (ShouldHaveBeenRecorded(host, *slot)) {
        CHECK_GT(untyped_->count(reinterpret_cast<Address>(slot)), 0);
      }
    }
  }

  void VisitCodeTarget(Code* host, RelocInfo* rinfo) override {
    Object* target = Code::GetCodeFromTargetAddress(rinfo->target_address());
    if (ShouldHaveBeenRecorded(host, target)) {
      CHECK(
          InTypedSet(CODE_TARGET_SLOT, rinfo->pc()) ||
          (rinfo->IsInConstantPool() &&
           InTypedSet(CODE_ENTRY_SLOT, rinfo->constant_pool_entry_address())));
    }
  }

  void VisitEmbeddedPointer(Code* host, RelocInfo* rinfo) override {
    Object* target = rinfo->target_object();
    if (ShouldHaveBeenRecorded(host, target)) {
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
  OldToNewSlotVerifyingVisitor(Heap* heap, std::set<Address>* untyped,
                               std::set<std::pair<SlotType, Address> >* typed)
      : SlotVerifyingVisitor(untyped, typed), heap_(heap) {}

  bool ShouldHaveBeenRecorded(HeapObject* host, Object* target) override {
    DCHECK_IMPLIES(target->IsHeapObject() && heap_->InNewSpace(target),
                   heap_->InToSpace(target));
    return target->IsHeapObject() && heap_->InNewSpace(target) &&
           !heap_->InNewSpace(host);
  }

 private:
  Heap* heap_;
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
  base::LockGuard<base::Mutex> lock_guard(chunk->mutex());
  Address start = object->address();
  Address end = start + object->Size();
  std::set<Address> old_to_new;
  std::set<std::pair<SlotType, Address> > typed_old_to_new;
  if (!InNewSpace(object)) {
    store_buffer()->MoveAllEntriesToRememberedSet();
    CollectSlots<OLD_TO_NEW>(chunk, start, end, &old_to_new, &typed_old_to_new);
    OldToNewSlotVerifyingVisitor visitor(this, &old_to_new, &typed_old_to_new);
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
  for (Page* page :
       PageRange(new_space_->FromSpaceStart(), new_space_->FromSpaceEnd())) {
    for (Address cursor = page->area_start(), limit = page->area_end();
         cursor < limit; cursor += kPointerSize) {
      Memory::Address_at(cursor) = kFromSpaceZapValue;
    }
  }
}

void Heap::IterateRoots(RootVisitor* v, VisitMode mode) {
  IterateStrongRoots(v, mode);
  IterateWeakRoots(v, mode);
}

void Heap::IterateWeakRoots(RootVisitor* v, VisitMode mode) {
  const bool isMinorGC = mode == VISIT_ALL_IN_SCAVENGE ||
                         mode == VISIT_ALL_IN_MINOR_MC_MARK ||
                         mode == VISIT_ALL_IN_MINOR_MC_UPDATE;
  v->VisitRootPointer(Root::kStringTable, reinterpret_cast<Object**>(
                                              &roots_[kStringTableRootIndex]));
  v->Synchronize(VisitorSynchronization::kStringTable);
  if (!isMinorGC && mode != VISIT_ALL_IN_SWEEP_NEWSPACE) {
    // Scavenge collections have special processing for this.
    external_string_table_.IterateAll(v);
  }
  v->Synchronize(VisitorSynchronization::kExternalStringsTable);
}

void Heap::IterateSmiRoots(RootVisitor* v) {
  // Acquire execution access since we are going to read stack limit values.
  ExecutionAccess access(isolate());
  v->VisitRootPointers(Root::kSmiRootList, &roots_[kSmiRootsStart],
                       &roots_[kRootListLength]);
  v->Synchronize(VisitorSynchronization::kSmiRootList);
}

void Heap::IterateEncounteredWeakCollections(RootVisitor* visitor) {
  visitor->VisitRootPointer(Root::kWeakCollections,
                            &encountered_weak_collections_);
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

  void VisitRootPointer(Root root, Object** p) override { FixHandle(p); }

  void VisitRootPointers(Root root, Object** start, Object** end) override {
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
        if (current->map() == heap_->one_pointer_filler_map()) {
          next += kPointerSize;
        } else if (current->map() == heap_->two_pointer_filler_map()) {
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
  v->VisitRootPointers(Root::kStrongRootList, &roots_[0],
                       &roots_[kStrongRootListLength]);
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
    isolate_->builtins()->IterateBuiltins(v);
    v->Synchronize(VisitorSynchronization::kBuiltins);
    isolate_->interpreter()->IterateDispatchTable(v);
    v->Synchronize(VisitorSynchronization::kDispatchTable);
  }

  // Iterate over global handles.
  switch (mode) {
    case VISIT_ONLY_STRONG_FOR_SERIALIZATION:
    case VISIT_ONLY_STRONG:
      isolate_->global_handles()->IterateStrongRoots(v);
      break;
    case VISIT_ALL_IN_SCAVENGE:
      isolate_->global_handles()->IterateNewSpaceStrongAndDependentRoots(v);
      break;
    case VISIT_ALL_IN_MINOR_MC_MARK:
      // Global handles are processed manually be the minor MC.
      break;
    case VISIT_ALL_IN_MINOR_MC_UPDATE:
      // Global handles are processed manually be the minor MC.
      break;
    case VISIT_ALL_IN_SWEEP_NEWSPACE:
    case VISIT_ALL:
      isolate_->global_handles()->IterateAllRoots(v);
      break;
  }
  v->Synchronize(VisitorSynchronization::kGlobalHandles);

  // Iterate over eternal handles.
  if (isMinorGC) {
    isolate_->eternal_handles()->IterateNewSpaceRoots(v);
  } else {
    isolate_->eternal_handles()->IterateAllRoots(v);
  }
  v->Synchronize(VisitorSynchronization::kEternalHandles);

  // Iterate over pointers being held by inactive threads.
  isolate_->thread_manager()->Iterate(v);
  v->Synchronize(VisitorSynchronization::kThreadManager);

  // Iterate over other strong roots (currently only identity maps).
  for (StrongRootsList* list = strong_roots_list_; list; list = list->next) {
    v->VisitRootPointers(Root::kStrongRoots, list->start, list->end);
  }
  v->Synchronize(VisitorSynchronization::kStrongRoots);

  // Iterate over the partial snapshot cache unless serializing.
  if (mode != VISIT_ONLY_STRONG_FOR_SERIALIZATION) {
    SerializerDeserializer::Iterate(isolate_, v);
  }
  // We don't do a v->Synchronize call here, because in debug mode that will
  // output a flag to the snapshot.  However at this point the serializer and
  // deserializer are deliberately a little unsynchronized (see above) so the
  // checking of the sync flag in the snapshot would fail.
}


// TODO(1236194): Since the heap size is configurable on the command line
// and through the API, we should gracefully handle the case that the heap
// size is not big enough to fit all the initial objects.
bool Heap::ConfigureHeap(size_t max_semi_space_size_in_kb,
                         size_t max_old_generation_size_in_mb,
                         size_t code_range_size_in_mb) {
  if (HasBeenSetUp()) return false;

  // Overwrite default configuration.
  if (max_semi_space_size_in_kb != 0) {
    max_semi_space_size_ =
        ROUND_UP(max_semi_space_size_in_kb * KB, Page::kPageSize);
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
    max_semi_space_size_ = ROUND_UP(max_semi_space_size_, Page::kPageSize);
    max_old_generation_size_ =
        ROUND_UP(max_old_generation_size_, Page::kPageSize);
  }

  if (FLAG_stress_compaction) {
    // This will cause more frequent GCs when stressing.
    max_semi_space_size_ = MB;
  }

  // The new space size must be a power of two to support single-bit testing
  // for containment.
  max_semi_space_size_ = base::bits::RoundUpToPowerOfTwo32(
      static_cast<uint32_t>(max_semi_space_size_));

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
          ROUND_UP(initial_semispace_size, Page::kPageSize);
    }
  }

  initial_semispace_size_ = Min(initial_semispace_size_, max_semi_space_size_);

  if (FLAG_semi_space_growth_factor < 2) {
    FLAG_semi_space_growth_factor = 2;
  }

  // The old generation is paged and needs at least one page for each space.
  int paged_space_count = LAST_PAGED_SPACE - FIRST_PAGED_SPACE + 1;
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
  return true;
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

bool Heap::ConfigureHeapDefault() { return ConfigureHeap(0, 0, 0); }

void Heap::RecordStats(HeapStats* stats, bool take_snapshot) {
  *stats->start_marker = HeapStats::kStartMarker;
  *stats->end_marker = HeapStats::kEndMarker;
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

size_t Heap::PromotedSpaceSizeOfObjects() {
  return old_space_->SizeOfObjects() + code_space_->SizeOfObjects() +
         map_space_->SizeOfObjects() + lo_space_->SizeOfObjects();
}

uint64_t Heap::PromotedExternalMemorySize() {
  if (external_memory_ <= external_memory_at_last_mark_compact_) return 0;
  return static_cast<uint64_t>(external_memory_ -
                               external_memory_at_last_mark_compact_);
}


const double Heap::kMinHeapGrowingFactor = 1.1;
const double Heap::kMaxHeapGrowingFactor = 4.0;
const double Heap::kMaxHeapGrowingFactorMemoryConstrained = 2.0;
const double Heap::kMaxHeapGrowingFactorIdle = 1.5;
const double Heap::kConservativeHeapGrowingFactor = 1.3;
const double Heap::kTargetMutatorUtilization = 0.97;

// Given GC speed in bytes per ms, the allocation throughput in bytes per ms
// (mutator speed), this function returns the heap growing factor that will
// achieve the kTargetMutatorUtilisation if the GC speed and the mutator speed
// remain the same until the next GC.
//
// For a fixed time-frame T = TM + TG, the mutator utilization is the ratio
// TM / (TM + TG), where TM is the time spent in the mutator and TG is the
// time spent in the garbage collector.
//
// Let MU be kTargetMutatorUtilisation, the desired mutator utilization for the
// time-frame from the end of the current GC to the end of the next GC. Based
// on the MU we can compute the heap growing factor F as
//
// F = R * (1 - MU) / (R * (1 - MU) - MU), where R = gc_speed / mutator_speed.
//
// This formula can be derived as follows.
//
// F = Limit / Live by definition, where the Limit is the allocation limit,
// and the Live is size of live objects.
// Let’s assume that we already know the Limit. Then:
//   TG = Limit / gc_speed
//   TM = (TM + TG) * MU, by definition of MU.
//   TM = TG * MU / (1 - MU)
//   TM = Limit *  MU / (gc_speed * (1 - MU))
// On the other hand, if the allocation throughput remains constant:
//   Limit = Live + TM * allocation_throughput = Live + TM * mutator_speed
// Solving it for TM, we get
//   TM = (Limit - Live) / mutator_speed
// Combining the two equation for TM:
//   (Limit - Live) / mutator_speed = Limit * MU / (gc_speed * (1 - MU))
//   (Limit - Live) = Limit * MU * mutator_speed / (gc_speed * (1 - MU))
// substitute R = gc_speed / mutator_speed
//   (Limit - Live) = Limit * MU  / (R * (1 - MU))
// substitute F = Limit / Live
//   F - 1 = F * MU  / (R * (1 - MU))
//   F - F * MU / (R * (1 - MU)) = 1
//   F * (1 - MU / (R * (1 - MU))) = 1
//   F * (R * (1 - MU) - MU) / (R * (1 - MU)) = 1
//   F = R * (1 - MU) / (R * (1 - MU) - MU)
double Heap::HeapGrowingFactor(double gc_speed, double mutator_speed,
                               double max_factor) {
  DCHECK_LE(kMinHeapGrowingFactor, max_factor);
  DCHECK_GE(kMaxHeapGrowingFactor, max_factor);
  if (gc_speed == 0 || mutator_speed == 0) return max_factor;

  const double speed_ratio = gc_speed / mutator_speed;
  const double mu = kTargetMutatorUtilization;

  const double a = speed_ratio * (1 - mu);
  const double b = speed_ratio * (1 - mu) - mu;

  // The factor is a / b, but we need to check for small b first.
  double factor = (a < b * max_factor) ? a / b : max_factor;
  factor = Min(factor, max_factor);
  factor = Max(factor, kMinHeapGrowingFactor);
  return factor;
}

double Heap::MaxHeapGrowingFactor(size_t max_old_generation_size) {
  const double min_small_factor = 1.3;
  const double max_small_factor = 2.0;
  const double high_factor = 4.0;

  size_t max_old_generation_size_in_mb = max_old_generation_size / MB;
  max_old_generation_size_in_mb =
      Max(max_old_generation_size_in_mb,
          static_cast<size_t>(kMinOldGenerationSize));

  // If we are on a device with lots of memory, we allow a high heap
  // growing factor.
  if (max_old_generation_size_in_mb >= kMaxOldGenerationSize) {
    return high_factor;
  }

  DCHECK_GE(max_old_generation_size_in_mb, kMinOldGenerationSize);
  DCHECK_LT(max_old_generation_size_in_mb, kMaxOldGenerationSize);

  // On smaller devices we linearly scale the factor: (X-A)/(B-A)*(D-C)+C
  double factor = (max_old_generation_size_in_mb - kMinOldGenerationSize) *
                      (max_small_factor - min_small_factor) /
                      (kMaxOldGenerationSize - kMinOldGenerationSize) +
                  min_small_factor;
  return factor;
}

size_t Heap::CalculateOldGenerationAllocationLimit(double factor,
                                                   size_t old_gen_size) {
  CHECK_LT(1.0, factor);
  CHECK_LT(0, old_gen_size);
  uint64_t limit = static_cast<uint64_t>(old_gen_size * factor);
  limit = Max(limit, static_cast<uint64_t>(old_gen_size) +
                         MinimumAllocationLimitGrowingStep());
  limit += new_space_->Capacity();
  uint64_t halfway_to_the_max =
      (static_cast<uint64_t>(old_gen_size) + max_old_generation_size_) / 2;
  return static_cast<size_t>(Min(limit, halfway_to_the_max));
}

size_t Heap::MinimumAllocationLimitGrowingStep() {
  const size_t kRegularAllocationLimitGrowingStep = 8;
  const size_t kLowMemoryAllocationLimitGrowingStep = 2;
  size_t limit = (Page::kPageSize > MB ? Page::kPageSize : MB);
  return limit * (ShouldOptimizeForMemoryUsage()
                      ? kLowMemoryAllocationLimitGrowingStep
                      : kRegularAllocationLimitGrowingStep);
}

void Heap::SetOldGenerationAllocationLimit(size_t old_gen_size, double gc_speed,
                                           double mutator_speed) {
  double max_factor = MaxHeapGrowingFactor(max_old_generation_size_);
  double factor = HeapGrowingFactor(gc_speed, mutator_speed, max_factor);

  if (FLAG_trace_gc_verbose) {
    isolate_->PrintWithTimestamp(
        "Heap growing factor %.1f based on mu=%.3f, speed_ratio=%.f "
        "(gc=%.f, mutator=%.f)\n",
        factor, kTargetMutatorUtilization, gc_speed / mutator_speed, gc_speed,
        mutator_speed);
  }

  if (memory_reducer_->ShouldGrowHeapSlowly() ||
      ShouldOptimizeForMemoryUsage()) {
    factor = Min(factor, kConservativeHeapGrowingFactor);
  }

  if (FLAG_stress_compaction || ShouldReduceMemory()) {
    factor = kMinHeapGrowingFactor;
  }

  if (FLAG_heap_growing_percent > 0) {
    factor = 1.0 + FLAG_heap_growing_percent / 100.0;
  }

  old_generation_allocation_limit_ =
      CalculateOldGenerationAllocationLimit(factor, old_gen_size);

  if (FLAG_trace_gc_verbose) {
    isolate_->PrintWithTimestamp(
        "Grow: old size: %" PRIuS " KB, new limit: %" PRIuS " KB (%.1f)\n",
        old_gen_size / KB, old_generation_allocation_limit_ / KB, factor);
  }
}

void Heap::DampenOldGenerationAllocationLimit(size_t old_gen_size,
                                              double gc_speed,
                                              double mutator_speed) {
  double max_factor = MaxHeapGrowingFactor(max_old_generation_size_);
  double factor = HeapGrowingFactor(gc_speed, mutator_speed, max_factor);
  size_t limit = CalculateOldGenerationAllocationLimit(factor, old_gen_size);
  if (limit < old_generation_allocation_limit_) {
    if (FLAG_trace_gc_verbose) {
      isolate_->PrintWithTimestamp(
          "Dampen: old size: %" PRIuS " KB, old limit: %" PRIuS
          " KB, "
          "new limit: %" PRIuS " KB (%.1f)\n",
          old_gen_size / KB, old_generation_allocation_limit_ / KB, limit / KB,
          factor);
    }
    old_generation_allocation_limit_ = limit;
  }
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
  if (PromotedSpaceSizeOfObjects() <=
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
    double size_before_gc = PromotedTotalSize() - gained_since_last_gc;
    double bytes_to_limit = old_generation_allocation_limit_ - size_before_gc;
    if (bytes_to_limit > 0) {
      double current_percent = (gained_since_last_gc / bytes_to_limit) * 100.0;

      if (FLAG_trace_incremental_marking) {
        isolate()->PrintWithTimestamp(
            "[IncrementalMarking] %.2lf%% of the memory limit reached\n",
            current_percent);
      }

      if (static_cast<int>(current_percent) >= stress_marking_percentage_) {
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
    space->EmptyAllocationInfo();
  }
}

bool Heap::SetUp() {
#ifdef DEBUG
  allocation_timeout_ = FLAG_gc_interval;
#endif

  // Initialize heap spaces and initial maps and objects. Whenever something
  // goes wrong, just return false. The caller should check the results and
  // call Heap::TearDown() to release allocated memory.
  //
  // If the heap is not yet configured (e.g. through the API), configure it.
  // Configuration is based on the flags new-space-size (really the semispace
  // size) and old-space-size if set or the initial values of semispace_size_
  // and old_generation_size_ otherwise.
  if (!configured_) {
    if (!ConfigureHeapDefault()) return false;
  }

  mmap_region_base_ =
      reinterpret_cast<uintptr_t>(base::OS::GetRandomMmapAddr()) &
      ~kMmapRegionMask;

  // Set up memory allocator.
  memory_allocator_ = new MemoryAllocator(isolate_);
  if (!memory_allocator_->SetUp(MaxReserved(), code_range_size_)) return false;

  store_buffer_ = new StoreBuffer(this);

  mark_compact_collector_ = new MarkCompactCollector(this);
  incremental_marking_ =
      new IncrementalMarking(this, mark_compact_collector_->marking_worklist());

  if (FLAG_concurrent_marking) {
    MarkCompactCollector::MarkingWorklist* marking_worklist =
        mark_compact_collector_->marking_worklist();
    concurrent_marking_ = new ConcurrentMarking(
        this, marking_worklist->shared(), marking_worklist->bailout(),
        marking_worklist->on_hold(), mark_compact_collector_->weak_objects());
  } else {
    concurrent_marking_ =
        new ConcurrentMarking(this, nullptr, nullptr, nullptr, nullptr);
  }

  for (int i = 0; i <= LAST_SPACE; i++) {
    space_[i] = nullptr;
  }

  space_[NEW_SPACE] = new_space_ = new NewSpace(this);
  if (!new_space_->SetUp(initial_semispace_size_, max_semi_space_size_)) {
    return false;
  }

  space_[OLD_SPACE] = old_space_ =
      new OldSpace(this, OLD_SPACE, NOT_EXECUTABLE);
  if (!old_space_->SetUp()) return false;

  space_[CODE_SPACE] = code_space_ = new OldSpace(this, CODE_SPACE, EXECUTABLE);
  if (!code_space_->SetUp()) return false;

  space_[MAP_SPACE] = map_space_ = new MapSpace(this, MAP_SPACE);
  if (!map_space_->SetUp()) return false;

  // The large object code space may contain code or data.  We set the memory
  // to be non-executable here for safety, but this means we need to enable it
  // explicitly when allocating large code objects.
  space_[LO_SPACE] = lo_space_ = new LargeObjectSpace(this, LO_SPACE);
  if (!lo_space_->SetUp()) return false;

  // Set up the seed that is used to randomize the string hash function.
  DCHECK_EQ(Smi::kZero, hash_seed());
  if (FLAG_randomize_hashes) InitializeHashSeed();

  for (int i = 0; i < static_cast<int>(v8::Isolate::kUseCounterFeatureCount);
       i++) {
    deferred_counters_[i] = 0;
  }

  tracer_ = new GCTracer(this);
  minor_mark_compact_collector_ = new MinorMarkCompactCollector(this);
  array_buffer_collector_ = new ArrayBufferCollector(this);
  gc_idle_time_handler_ = new GCIdleTimeHandler();
  memory_reducer_ = new MemoryReducer(this);
  if (V8_UNLIKELY(FLAG_gc_stats)) {
    live_object_stats_ = new ObjectStats(this);
    dead_object_stats_ = new ObjectStats(this);
  }
  scavenge_job_ = new ScavengeJob();
  local_embedder_heap_tracer_ = new LocalEmbedderHeapTracer();

  LOG(isolate_, IntPtrTEvent("heap-capacity", Capacity()));
  LOG(isolate_, IntPtrTEvent("heap-available", Available()));

  store_buffer()->SetUp();

  mark_compact_collector()->SetUp();
  if (minor_mark_compact_collector() != nullptr) {
    minor_mark_compact_collector()->SetUp();
  }

  idle_scavenge_observer_ = new IdleScavengeObserver(
      *this, ScavengeJob::kBytesAllocatedBeforeNextIdleTask);
  new_space()->AddAllocationObserver(idle_scavenge_observer_);

  SetGetExternallyAllocatedMemoryInBytesCallback(
      DefaultGetExternallyAllocatedMemoryInBytesCallback);

  if (FLAG_stress_marking > 0) {
    stress_marking_percentage_ = NextStressMarkingLimit();
  }

  write_protect_code_memory_ = FLAG_write_protect_code_memory;

  return true;
}

void Heap::InitializeHashSeed() {
  if (FLAG_hash_seed == 0) {
    int rnd = isolate()->random_number_generator()->NextInt();
    set_hash_seed(Smi::FromInt(rnd & Name::kHashBitMask));
  } else {
    set_hash_seed(Smi::FromInt(FLAG_hash_seed));
  }
}

void Heap::SetStackLimits() {
  DCHECK_NOT_NULL(isolate_);
  DCHECK(isolate_ == isolate());
  // On 64 bit machines, pointers are generally out of range of Smis.  We write
  // something that looks like an out of range Smi to the GC.

  // Set up the special root array entries containing the stack limits.
  // These are actually addresses, but the tag makes the GC ignore it.
  roots_[kStackLimitRootIndex] = reinterpret_cast<Object*>(
      (isolate_->stack_guard()->jslimit() & ~kSmiTagMask) | kSmiTag);
  roots_[kRealStackLimitRootIndex] = reinterpret_cast<Object*>(
      (isolate_->stack_guard()->real_jslimit() & ~kSmiTagMask) | kSmiTag);
}

void Heap::ClearStackLimits() {
  roots_[kStackLimitRootIndex] = Smi::kZero;
  roots_[kRealStackLimitRootIndex] = Smi::kZero;
}

void Heap::PrintAllocationsHash() {
  uint32_t hash = StringHasher::GetHashCore(raw_allocations_hash_);
  PrintF("\n### Allocations = %u, hash = 0x%08x\n", allocations_count(), hash);
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

  deserialization_complete_ = true;
}

void Heap::SetEmbedderHeapTracer(EmbedderHeapTracer* tracer) {
  DCHECK_EQ(gc_state_, HeapState::NOT_IN_GC);
  local_embedder_heap_tracer()->SetRemoteTracer(tracer);
}

void Heap::TracePossibleWrapper(JSObject* js_object) {
  DCHECK(js_object->WasConstructedFromApiFunction());
  if (js_object->GetEmbedderFieldCount() >= 2 &&
      js_object->GetEmbedderField(0) &&
      js_object->GetEmbedderField(0) != undefined_value() &&
      js_object->GetEmbedderField(1) != undefined_value()) {
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

void Heap::TearDown() {
  use_tasks_ = false;
#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) {
    Verify();
  }
#endif

  UpdateMaximumCommitted();

  if (FLAG_verify_predictable) {
    PrintAllocationsHash();
  }

  new_space()->RemoveAllocationObserver(idle_scavenge_observer_);
  delete idle_scavenge_observer_;
  idle_scavenge_observer_ = nullptr;

  if (mark_compact_collector_ != nullptr) {
    mark_compact_collector_->TearDown();
    delete mark_compact_collector_;
    mark_compact_collector_ = nullptr;
  }

  if (minor_mark_compact_collector_ != nullptr) {
    minor_mark_compact_collector_->TearDown();
    delete minor_mark_compact_collector_;
    minor_mark_compact_collector_ = nullptr;
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

  delete tracer_;
  tracer_ = nullptr;

  new_space_->TearDown();
  delete new_space_;
  new_space_ = nullptr;

  if (old_space_ != nullptr) {
    delete old_space_;
    old_space_ = nullptr;
  }

  if (code_space_ != nullptr) {
    delete code_space_;
    code_space_ = nullptr;
  }

  if (map_space_ != nullptr) {
    delete map_space_;
    map_space_ = nullptr;
  }

  if (lo_space_ != nullptr) {
    lo_space_->TearDown();
    delete lo_space_;
    lo_space_ = nullptr;
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

// TODO(ishell): Find a better place for this.
void Heap::AddWeakNewSpaceObjectToCodeDependency(Handle<HeapObject> obj,
                                                 Handle<WeakCell> code) {
  DCHECK(InNewSpace(*obj));
  DCHECK(!InNewSpace(*code));
  Handle<ArrayList> list(weak_new_space_object_to_code_list(), isolate());
  list = ArrayList::Add(list, isolate()->factory()->NewWeakCell(obj), code);
  if (*list != weak_new_space_object_to_code_list()) {
    set_weak_new_space_object_to_code_list(*list);
  }
}

// TODO(ishell): Find a better place for this.
void Heap::AddWeakObjectToCodeDependency(Handle<HeapObject> obj,
                                         Handle<DependentCode> dep) {
  DCHECK(!InNewSpace(*obj));
  DCHECK(!InNewSpace(*dep));
  Handle<WeakHashTable> table(weak_object_to_code_table(), isolate());
  table = WeakHashTable::Put(table, obj, dep);
  if (*table != weak_object_to_code_table())
    set_weak_object_to_code_table(*table);
  DCHECK_EQ(*dep, LookupWeakObjectToCodeDependency(obj));
}


DependentCode* Heap::LookupWeakObjectToCodeDependency(Handle<HeapObject> obj) {
  Object* dep = weak_object_to_code_table()->Lookup(obj);
  if (dep->IsDependentCode()) return DependentCode::cast(dep);
  return DependentCode::cast(empty_fixed_array());
}

namespace {
void CompactWeakFixedArray(Object* object) {
  if (object->IsWeakFixedArray()) {
    WeakFixedArray* array = WeakFixedArray::cast(object);
    array->Compact<WeakFixedArray::NullCallback>();
  }
}
}  // anonymous namespace

void Heap::CompactWeakFixedArrays() {
  // Find known WeakFixedArrays and compact them.
  HeapIterator iterator(this);
  for (HeapObject* o = iterator.next(); o != nullptr; o = iterator.next()) {
    if (o->IsPrototypeInfo()) {
      Object* prototype_users = PrototypeInfo::cast(o)->prototype_users();
      if (prototype_users->IsWeakFixedArray()) {
        WeakFixedArray* array = WeakFixedArray::cast(prototype_users);
        array->Compact<JSObject::PrototypeRegistryCompactionCallback>();
      }
    }
  }
  CompactWeakFixedArray(noscript_shared_function_infos());
  CompactWeakFixedArray(script_list());
  CompactWeakFixedArray(weak_stack_trace_list());
}

void Heap::AddRetainedMap(Handle<Map> map) {
  Handle<WeakCell> cell = Map::WeakCellForMap(map);
  Handle<ArrayList> array(retained_maps(), isolate());
  if (array->IsFull()) {
    CompactRetainedMaps(*array);
  }
  array = ArrayList::Add(
      array, cell, handle(Smi::FromInt(FLAG_retain_maps_for_n_gc), isolate()),
      ArrayList::kReloadLengthAfterAllocation);
  if (*array != retained_maps()) {
    set_retained_maps(*array);
  }
}


void Heap::CompactRetainedMaps(ArrayList* retained_maps) {
  DCHECK_EQ(retained_maps, this->retained_maps());
  int length = retained_maps->Length();
  int new_length = 0;
  int new_number_of_disposed_maps = 0;
  // This loop compacts the array by removing cleared weak cells.
  for (int i = 0; i < length; i += 2) {
    DCHECK(retained_maps->Get(i)->IsWeakCell());
    WeakCell* cell = WeakCell::cast(retained_maps->Get(i));
    Object* age = retained_maps->Get(i + 1);
    if (cell->cleared()) continue;
    if (i != new_length) {
      retained_maps->Set(new_length, cell);
      retained_maps->Set(new_length + 1, age);
    }
    if (i < number_of_disposed_maps_) {
      new_number_of_disposed_maps += 2;
    }
    new_length += 2;
  }
  number_of_disposed_maps_ = new_number_of_disposed_maps;
  Object* undefined = undefined_value();
  for (int i = new_length; i < length; i++) {
    retained_maps->Clear(i, undefined);
  }
  if (new_length != length) retained_maps->SetLength(new_length);
}

void Heap::FatalProcessOutOfMemory(const char* location, bool is_heap_oom) {
  v8::internal::V8::FatalProcessOutOfMemory(location, is_heap_oom);
}

#ifdef DEBUG

class PrintHandleVisitor : public RootVisitor {
 public:
  void VisitRootPointers(Root root, Object** start, Object** end) override {
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
  void VisitRootPointers(Root root, Object** start, Object** end) override {
    handle_count_ += end - start;
  }

 private:
  ptrdiff_t handle_count_;
};


void Heap::CheckHandleCount() {
  CheckHandleCountVisitor v;
  isolate_->handle_scope_implementer()->Iterate(&v);
}

void Heap::ClearRecordedSlot(HeapObject* object, Object** slot) {
  Address slot_addr = reinterpret_cast<Address>(slot);
  Page* page = Page::FromAddress(slot_addr);
  if (!page->InNewSpace()) {
    DCHECK_EQ(page->owner()->identity(), OLD_SPACE);
    store_buffer()->DeleteEntry(slot_addr);
  }
}

bool Heap::HasRecordedSlot(HeapObject* object, Object** slot) {
  if (InNewSpace(object)) {
    return false;
  }
  Address slot_addr = reinterpret_cast<Address>(slot);
  Page* page = Page::FromAddress(slot_addr);
  DCHECK_EQ(page->owner()->identity(), OLD_SPACE);
  store_buffer()->MoveAllEntriesToRememberedSet();
  return RememberedSet<OLD_TO_NEW>::Contains(page, slot_addr) ||
         RememberedSet<OLD_TO_OLD>::Contains(page, slot_addr);
}

void Heap::ClearRecordedSlotRange(Address start, Address end) {
  Page* page = Page::FromAddress(start);
  if (!page->InNewSpace()) {
    DCHECK_EQ(page->owner()->identity(), OLD_SPACE);
    store_buffer()->DeleteEntry(start, end);
  }
}

void Heap::RecordWriteIntoCodeSlow(Code* host, RelocInfo* rinfo,
                                   Object* value) {
  DCHECK(InNewSpace(value));
  Page* source_page = Page::FromAddress(reinterpret_cast<Address>(host));
  RelocInfo::Mode rmode = rinfo->rmode();
  Address addr = rinfo->pc();
  SlotType slot_type = SlotTypeForRelocInfoMode(rmode);
  if (rinfo->IsInConstantPool()) {
    addr = rinfo->constant_pool_entry_address();
    if (RelocInfo::IsCodeTarget(rmode)) {
      slot_type = CODE_ENTRY_SLOT;
    } else {
      DCHECK(RelocInfo::IsEmbeddedObject(rmode));
      slot_type = OBJECT_SLOT;
    }
  }
  RememberedSet<OLD_TO_NEW>::InsertTyped(
      source_page, reinterpret_cast<Address>(host), slot_type, addr);
}

void Heap::RecordWritesIntoCode(Code* code) {
  for (RelocIterator it(code, RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT));
       !it.done(); it.next()) {
    RecordWriteIntoCode(code, it.rinfo(), it.rinfo()->target_object());
  }
}

Space* AllSpaces::next() {
  switch (counter_++) {
    case NEW_SPACE:
      return heap_->new_space();
    case OLD_SPACE:
      return heap_->old_space();
    case CODE_SPACE:
      return heap_->code_space();
    case MAP_SPACE:
      return heap_->map_space();
    case LO_SPACE:
      return heap_->lo_space();
    default:
      return nullptr;
  }
}

PagedSpace* PagedSpaces::next() {
  switch (counter_++) {
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


OldSpace* OldSpaces::next() {
  switch (counter_++) {
    case OLD_SPACE:
      return heap_->old_space();
    case CODE_SPACE:
      return heap_->code_space();
    default:
      return nullptr;
  }
}

SpaceIterator::SpaceIterator(Heap* heap)
    : heap_(heap), current_space_(FIRST_SPACE - 1) {}

SpaceIterator::~SpaceIterator() {
}


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
  virtual ~HeapObjectsFilter() {}
  virtual bool SkipObject(HeapObject* object) = 0;
};


class UnreachableObjectsFilter : public HeapObjectsFilter {
 public:
  explicit UnreachableObjectsFilter(Heap* heap) : heap_(heap) {
    MarkReachableObjects();
  }

  ~UnreachableObjectsFilter() {
    for (auto it : reachable_) {
      delete it.second;
      it.second = nullptr;
    }
  }

  bool SkipObject(HeapObject* object) {
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
      MarkPointers(start, end);
    }

    void VisitRootPointers(Root root, Object** start, Object** end) override {
      MarkPointers(start, end);
    }

    void TransitiveClosure() {
      while (!marking_stack_.empty()) {
        HeapObject* obj = marking_stack_.back();
        marking_stack_.pop_back();
        obj->Iterate(this);
      }
    }

   private:
    void MarkPointers(Object** start, Object** end) {
      for (Object** p = start; p < end; p++) {
        if (!(*p)->IsHeapObject()) continue;
        HeapObject* obj = HeapObject::cast(*p);
        if (filter_->MarkAsReachable(obj)) {
          marking_stack_.push_back(obj);
        }
      }
    }
    UnreachableObjectsFilter* filter_;
    std::vector<HeapObject*> marking_stack_;
  };

  friend class MarkingVisitor;

  void MarkReachableObjects() {
    MarkingVisitor visitor(this);
    heap_->IterateRoots(&visitor, VISIT_ALL);
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
    if (o->IsThinString()) {
      o = ThinString::cast(o)->actual();
      if (!o->IsExternalString()) continue;
    }
    DCHECK(o->IsExternalString());
    if (heap_->InNewSpace(o)) {
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
    if (o->IsThinString()) {
      o = ThinString::cast(o)->actual();
      if (!o->IsExternalString()) continue;
    }
    DCHECK(o->IsExternalString());
    DCHECK(!heap_->InNewSpace(o));
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
    if (o->IsThinString()) {
      o = ThinString::cast(o)->actual();
      if (!o->IsExternalString()) continue;
    }
    heap_->FinalizeExternalString(ExternalString::cast(o));
  }
  new_space_strings_.clear();
  for (size_t i = 0; i < old_space_strings_.size(); ++i) {
    Object* o = old_space_strings_[i];
    if (o->IsThinString()) {
      o = ThinString::cast(o)->actual();
      if (!o->IsExternalString()) continue;
    }
    heap_->FinalizeExternalString(ExternalString::cast(o));
  }
  old_space_strings_.clear();
}


void Heap::RememberUnmappedPage(Address page, bool compacted) {
  uintptr_t p = reinterpret_cast<uintptr_t>(page);
  // Tag the page pointer to make it findable in the dump file.
  if (compacted) {
    p ^= 0xc1ead & (Page::kPageSize - 1);  // Cleared.
  } else {
    p ^= 0x1d1ed & (Page::kPageSize - 1);  // I died.
  }
  remembered_unmapped_pages_[remembered_unmapped_pages_index_] =
      reinterpret_cast<Address>(p);
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

bool Heap::IsDeserializeLazyHandler(Code* code) {
  return (code == deserialize_lazy_handler() ||
          code == deserialize_lazy_handler_wide() ||
          code == deserialize_lazy_handler_extra_wide());
}

void Heap::SetDeserializeLazyHandler(Code* code) {
  set_deserialize_lazy_handler(code);
}

void Heap::SetDeserializeLazyHandlerWide(Code* code) {
  set_deserialize_lazy_handler_wide(code);
}

void Heap::SetDeserializeLazyHandlerExtraWide(Code* code) {
  set_deserialize_lazy_handler_extra_wide(code);
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
#define COMPARE_AND_RETURN_NAME(name)                      \
  case ObjectStats::FIRST_CODE_KIND_SUB_TYPE + Code::name: \
    *object_type = "CODE_TYPE";                            \
    *object_sub_type = "CODE_KIND/" #name;                 \
    return true;
    CODE_KIND_LIST(COMPARE_AND_RETURN_NAME)
#undef COMPARE_AND_RETURN_NAME
#define COMPARE_AND_RETURN_NAME(name)                  \
  case ObjectStats::FIRST_FIXED_ARRAY_SUB_TYPE + name: \
    *object_type = "FIXED_ARRAY_TYPE";                 \
    *object_sub_type = #name;                          \
    return true;
    FIXED_ARRAY_SUB_INSTANCE_TYPE_LIST(COMPARE_AND_RETURN_NAME)
#undef COMPARE_AND_RETURN_NAME
  }
  return false;
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
    default:
      UNREACHABLE();
  }
  return nullptr;
}

void VerifyPointersVisitor::VisitPointers(HeapObject* host, Object** start,
                                          Object** end) {
  VerifyPointers(start, end);
}

void VerifyPointersVisitor::VisitRootPointers(Root root, Object** start,
                                              Object** end) {
  VerifyPointers(start, end);
}

void VerifyPointersVisitor::VerifyPointers(Object** start, Object** end) {
  for (Object** current = start; current < end; current++) {
    if ((*current)->IsHeapObject()) {
      HeapObject* object = HeapObject::cast(*current);
      CHECK(object->GetIsolate()->heap()->Contains(object));
      CHECK(object->map()->IsMap());
    } else {
      CHECK((*current)->IsSmi());
    }
  }
}

void VerifySmisVisitor::VisitRootPointers(Root root, Object** start,
                                          Object** end) {
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
  if (obj->map() == one_pointer_filler_map()) return false;
  InstanceType type = obj->map()->instance_type();
  MemoryChunk* chunk = MemoryChunk::FromAddress(obj->address());
  AllocationSpace src = chunk->owner()->identity();
  switch (src) {
    case NEW_SPACE:
      return dst == src || dst == OLD_SPACE;
    case OLD_SPACE:
      return dst == src &&
             (dst == OLD_SPACE || obj->IsFiller() || obj->IsExternalString());
    case CODE_SPACE:
      return dst == src && type == CODE_TYPE;
    case MAP_SPACE:
    case LO_SPACE:
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
  DCHECK(map == code->GetHeap()->code_map());
  Address start = code->address();
  Address end = code->address() + code->SizeFromMap(map);
  return start <= addr && addr < end;
}

Code* Heap::GcSafeFindCodeForInnerPointer(Address inner_pointer) {
  // Check if the inner pointer points into a large object chunk.
  LargePage* large_page = lo_space()->FindPage(inner_pointer);
  if (large_page != nullptr) {
    return GcSafeCastToCode(this, large_page->GetObject(), inner_pointer);
  }

  if (!code_space()->Contains(inner_pointer)) {
    return nullptr;
  }

  // Iterate through the page until we reach the end or find an object starting
  // after the inner pointer.
  Page* page = Page::FromAddress(inner_pointer);
  DCHECK_EQ(page->owner(), code_space());
  mark_compact_collector()->sweeper()->SweepOrWaitUntilSweepingCompleted(page);

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

}  // namespace internal
}  // namespace v8
