// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/accessors.h"
#include "src/api.h"
#include "src/base/bits.h"
#include "src/base/once.h"
#include "src/base/utils/random-number-generator.h"
#include "src/bootstrapper.h"
#include "src/codegen.h"
#include "src/compilation-cache.h"
#include "src/conversions.h"
#include "src/cpu-profiler.h"
#include "src/debug.h"
#include "src/deoptimizer.h"
#include "src/global-handles.h"
#include "src/heap/gc-idle-time-handler.h"
#include "src/heap/incremental-marking.h"
#include "src/heap/mark-compact.h"
#include "src/heap/objects-visiting-inl.h"
#include "src/heap/objects-visiting.h"
#include "src/heap/store-buffer.h"
#include "src/heap-profiler.h"
#include "src/isolate-inl.h"
#include "src/natives.h"
#include "src/runtime-profiler.h"
#include "src/scopeinfo.h"
#include "src/snapshot.h"
#include "src/utils.h"
#include "src/v8threads.h"
#include "src/vm-state-inl.h"

#if V8_TARGET_ARCH_ARM && !V8_INTERPRETED_REGEXP
#include "src/regexp-macro-assembler.h"          // NOLINT
#include "src/arm/regexp-macro-assembler-arm.h"  // NOLINT
#endif
#if V8_TARGET_ARCH_MIPS && !V8_INTERPRETED_REGEXP
#include "src/regexp-macro-assembler.h"            // NOLINT
#include "src/mips/regexp-macro-assembler-mips.h"  // NOLINT
#endif
#if V8_TARGET_ARCH_MIPS64 && !V8_INTERPRETED_REGEXP
#include "src/regexp-macro-assembler.h"
#include "src/mips64/regexp-macro-assembler-mips64.h"
#endif

namespace v8 {
namespace internal {


Heap::Heap()
    : amount_of_external_allocated_memory_(0),
      amount_of_external_allocated_memory_at_last_global_gc_(0),
      isolate_(NULL),
      code_range_size_(0),
      // semispace_size_ should be a power of 2 and old_generation_size_ should
      // be a multiple of Page::kPageSize.
      reserved_semispace_size_(8 * (kPointerSize / 4) * MB),
      max_semi_space_size_(8 * (kPointerSize / 4) * MB),
      initial_semispace_size_(Page::kPageSize),
      max_old_generation_size_(700ul * (kPointerSize / 4) * MB),
      max_executable_size_(256ul * (kPointerSize / 4) * MB),
      // Variables set based on semispace_size_ and old_generation_size_ in
      // ConfigureHeap.
      // Will be 4 * reserved_semispace_size_ to ensure that young
      // generation can be aligned to its size.
      maximum_committed_(0),
      survived_since_last_expansion_(0),
      sweep_generation_(0),
      always_allocate_scope_depth_(0),
      contexts_disposed_(0),
      global_ic_age_(0),
      flush_monomorphic_ics_(false),
      scan_on_scavenge_pages_(0),
      new_space_(this),
      old_pointer_space_(NULL),
      old_data_space_(NULL),
      code_space_(NULL),
      map_space_(NULL),
      cell_space_(NULL),
      property_cell_space_(NULL),
      lo_space_(NULL),
      gc_state_(NOT_IN_GC),
      gc_post_processing_depth_(0),
      allocations_count_(0),
      raw_allocations_hash_(0),
      dump_allocations_hash_countdown_(FLAG_dump_allocations_digest_at_alloc),
      ms_count_(0),
      gc_count_(0),
      remembered_unmapped_pages_index_(0),
      unflattened_strings_length_(0),
#ifdef DEBUG
      allocation_timeout_(0),
#endif  // DEBUG
      old_generation_allocation_limit_(kMinimumOldGenerationAllocationLimit),
      old_gen_exhausted_(false),
      inline_allocation_disabled_(false),
      store_buffer_rebuilder_(store_buffer()),
      hidden_string_(NULL),
      gc_safe_size_of_old_object_(NULL),
      total_regexp_code_generated_(0),
      tracer_(this),
      high_survival_rate_period_length_(0),
      promoted_objects_size_(0),
      promotion_rate_(0),
      semi_space_copied_object_size_(0),
      semi_space_copied_rate_(0),
      nodes_died_in_new_space_(0),
      nodes_copied_in_new_space_(0),
      nodes_promoted_(0),
      maximum_size_scavenges_(0),
      max_gc_pause_(0.0),
      total_gc_time_ms_(0.0),
      max_alive_after_gc_(0),
      min_in_mutator_(kMaxInt),
      marking_time_(0.0),
      sweeping_time_(0.0),
      mark_compact_collector_(this),
      store_buffer_(this),
      marking_(this),
      incremental_marking_(this),
      gc_count_at_last_idle_gc_(0),
      full_codegen_bytes_generated_(0),
      crankshaft_codegen_bytes_generated_(0),
      gcs_since_last_deopt_(0),
#ifdef VERIFY_HEAP
      no_weak_object_verification_scope_depth_(0),
#endif
      allocation_sites_scratchpad_length_(0),
      promotion_queue_(this),
      configured_(false),
      external_string_table_(this),
      chunks_queued_for_free_(NULL),
      gc_callbacks_depth_(0) {
// Allow build-time customization of the max semispace size. Building
// V8 with snapshots and a non-default max semispace size is much
// easier if you can define it as part of the build environment.
#if defined(V8_MAX_SEMISPACE_SIZE)
  max_semi_space_size_ = reserved_semispace_size_ = V8_MAX_SEMISPACE_SIZE;
#endif

  // Ensure old_generation_size_ is a multiple of kPageSize.
  DCHECK(MB >= Page::kPageSize);

  memset(roots_, 0, sizeof(roots_[0]) * kRootListLength);
  set_native_contexts_list(NULL);
  set_array_buffers_list(Smi::FromInt(0));
  set_allocation_sites_list(Smi::FromInt(0));
  set_encountered_weak_collections(Smi::FromInt(0));
  // Put a dummy entry in the remembered pages so we can find the list the
  // minidump even if there are no real unmapped pages.
  RememberUnmappedPage(NULL, false);

  ClearObjectStats(true);
}


intptr_t Heap::Capacity() {
  if (!HasBeenSetUp()) return 0;

  return new_space_.Capacity() + old_pointer_space_->Capacity() +
         old_data_space_->Capacity() + code_space_->Capacity() +
         map_space_->Capacity() + cell_space_->Capacity() +
         property_cell_space_->Capacity();
}


intptr_t Heap::CommittedMemory() {
  if (!HasBeenSetUp()) return 0;

  return new_space_.CommittedMemory() + old_pointer_space_->CommittedMemory() +
         old_data_space_->CommittedMemory() + code_space_->CommittedMemory() +
         map_space_->CommittedMemory() + cell_space_->CommittedMemory() +
         property_cell_space_->CommittedMemory() + lo_space_->Size();
}


size_t Heap::CommittedPhysicalMemory() {
  if (!HasBeenSetUp()) return 0;

  return new_space_.CommittedPhysicalMemory() +
         old_pointer_space_->CommittedPhysicalMemory() +
         old_data_space_->CommittedPhysicalMemory() +
         code_space_->CommittedPhysicalMemory() +
         map_space_->CommittedPhysicalMemory() +
         cell_space_->CommittedPhysicalMemory() +
         property_cell_space_->CommittedPhysicalMemory() +
         lo_space_->CommittedPhysicalMemory();
}


intptr_t Heap::CommittedMemoryExecutable() {
  if (!HasBeenSetUp()) return 0;

  return isolate()->memory_allocator()->SizeExecutable();
}


void Heap::UpdateMaximumCommitted() {
  if (!HasBeenSetUp()) return;

  intptr_t current_committed_memory = CommittedMemory();
  if (current_committed_memory > maximum_committed_) {
    maximum_committed_ = current_committed_memory;
  }
}


intptr_t Heap::Available() {
  if (!HasBeenSetUp()) return 0;

  return new_space_.Available() + old_pointer_space_->Available() +
         old_data_space_->Available() + code_space_->Available() +
         map_space_->Available() + cell_space_->Available() +
         property_cell_space_->Available();
}


bool Heap::HasBeenSetUp() {
  return old_pointer_space_ != NULL && old_data_space_ != NULL &&
         code_space_ != NULL && map_space_ != NULL && cell_space_ != NULL &&
         property_cell_space_ != NULL && lo_space_ != NULL;
}


int Heap::GcSafeSizeOfOldObject(HeapObject* object) {
  if (IntrusiveMarking::IsMarked(object)) {
    return IntrusiveMarking::SizeOfMarkedObject(object);
  }
  return object->SizeFromMap(object->map());
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

  // Is enough data promoted to justify a global GC?
  if (OldGenerationAllocationLimitReached()) {
    isolate_->counters()->gc_compactor_caused_by_promoted_data()->Increment();
    *reason = "promotion limit reached";
    return MARK_COMPACTOR;
  }

  // Have allocation in OLD and LO failed?
  if (old_gen_exhausted_) {
    isolate_->counters()
        ->gc_compactor_caused_by_oldspace_exhaustion()
        ->Increment();
    *reason = "old generations exhausted";
    return MARK_COMPACTOR;
  }

  // Is there enough space left in OLD to guarantee that a scavenge can
  // succeed?
  //
  // Note that MemoryAllocator->MaxAvailable() undercounts the memory available
  // for object promotion. It counts only the bytes that the memory
  // allocator has not yet allocated from the OS and assigned to any space,
  // and does not count available bytes already in the old space or code
  // space.  Undercounting is safe---we may get an unrequested full GC when
  // a scavenge would have succeeded.
  if (isolate_->memory_allocator()->MaxAvailable() <= new_space_.Size()) {
    isolate_->counters()
        ->gc_compactor_caused_by_oldspace_exhaustion()
        ->Increment();
    *reason = "scavenge might not succeed";
    return MARK_COMPACTOR;
  }

  // Default
  *reason = NULL;
  return SCAVENGER;
}


// TODO(1238405): Combine the infrastructure for --heap-stats and
// --log-gc to avoid the complicated preprocessor and flag testing.
void Heap::ReportStatisticsBeforeGC() {
// Heap::ReportHeapStatistics will also log NewSpace statistics when
// compiled --log-gc is set.  The following logic is used to avoid
// double logging.
#ifdef DEBUG
  if (FLAG_heap_stats || FLAG_log_gc) new_space_.CollectStatistics();
  if (FLAG_heap_stats) {
    ReportHeapStatistics("Before GC");
  } else if (FLAG_log_gc) {
    new_space_.ReportStatistics();
  }
  if (FLAG_heap_stats || FLAG_log_gc) new_space_.ClearHistograms();
#else
  if (FLAG_log_gc) {
    new_space_.CollectStatistics();
    new_space_.ReportStatistics();
    new_space_.ClearHistograms();
  }
#endif  // DEBUG
}


void Heap::PrintShortHeapStatistics() {
  if (!FLAG_trace_gc_verbose) return;
  PrintPID("Memory allocator,   used: %6" V8_PTR_PREFIX
           "d KB"
           ", available: %6" V8_PTR_PREFIX "d KB\n",
           isolate_->memory_allocator()->Size() / KB,
           isolate_->memory_allocator()->Available() / KB);
  PrintPID("New space,          used: %6" V8_PTR_PREFIX
           "d KB"
           ", available: %6" V8_PTR_PREFIX
           "d KB"
           ", committed: %6" V8_PTR_PREFIX "d KB\n",
           new_space_.Size() / KB, new_space_.Available() / KB,
           new_space_.CommittedMemory() / KB);
  PrintPID("Old pointers,       used: %6" V8_PTR_PREFIX
           "d KB"
           ", available: %6" V8_PTR_PREFIX
           "d KB"
           ", committed: %6" V8_PTR_PREFIX "d KB\n",
           old_pointer_space_->SizeOfObjects() / KB,
           old_pointer_space_->Available() / KB,
           old_pointer_space_->CommittedMemory() / KB);
  PrintPID("Old data space,     used: %6" V8_PTR_PREFIX
           "d KB"
           ", available: %6" V8_PTR_PREFIX
           "d KB"
           ", committed: %6" V8_PTR_PREFIX "d KB\n",
           old_data_space_->SizeOfObjects() / KB,
           old_data_space_->Available() / KB,
           old_data_space_->CommittedMemory() / KB);
  PrintPID("Code space,         used: %6" V8_PTR_PREFIX
           "d KB"
           ", available: %6" V8_PTR_PREFIX
           "d KB"
           ", committed: %6" V8_PTR_PREFIX "d KB\n",
           code_space_->SizeOfObjects() / KB, code_space_->Available() / KB,
           code_space_->CommittedMemory() / KB);
  PrintPID("Map space,          used: %6" V8_PTR_PREFIX
           "d KB"
           ", available: %6" V8_PTR_PREFIX
           "d KB"
           ", committed: %6" V8_PTR_PREFIX "d KB\n",
           map_space_->SizeOfObjects() / KB, map_space_->Available() / KB,
           map_space_->CommittedMemory() / KB);
  PrintPID("Cell space,         used: %6" V8_PTR_PREFIX
           "d KB"
           ", available: %6" V8_PTR_PREFIX
           "d KB"
           ", committed: %6" V8_PTR_PREFIX "d KB\n",
           cell_space_->SizeOfObjects() / KB, cell_space_->Available() / KB,
           cell_space_->CommittedMemory() / KB);
  PrintPID("PropertyCell space, used: %6" V8_PTR_PREFIX
           "d KB"
           ", available: %6" V8_PTR_PREFIX
           "d KB"
           ", committed: %6" V8_PTR_PREFIX "d KB\n",
           property_cell_space_->SizeOfObjects() / KB,
           property_cell_space_->Available() / KB,
           property_cell_space_->CommittedMemory() / KB);
  PrintPID("Large object space, used: %6" V8_PTR_PREFIX
           "d KB"
           ", available: %6" V8_PTR_PREFIX
           "d KB"
           ", committed: %6" V8_PTR_PREFIX "d KB\n",
           lo_space_->SizeOfObjects() / KB, lo_space_->Available() / KB,
           lo_space_->CommittedMemory() / KB);
  PrintPID("All spaces,         used: %6" V8_PTR_PREFIX
           "d KB"
           ", available: %6" V8_PTR_PREFIX
           "d KB"
           ", committed: %6" V8_PTR_PREFIX "d KB\n",
           this->SizeOfObjects() / KB, this->Available() / KB,
           this->CommittedMemory() / KB);
  PrintPID("External memory reported: %6" V8_PTR_PREFIX "d KB\n",
           static_cast<intptr_t>(amount_of_external_allocated_memory_ / KB));
  PrintPID("Total time spent in GC  : %.1f ms\n", total_gc_time_ms_);
}


// TODO(1238405): Combine the infrastructure for --heap-stats and
// --log-gc to avoid the complicated preprocessor and flag testing.
void Heap::ReportStatisticsAfterGC() {
// Similar to the before GC, we use some complicated logic to ensure that
// NewSpace statistics are logged exactly once when --log-gc is turned on.
#if defined(DEBUG)
  if (FLAG_heap_stats) {
    new_space_.CollectStatistics();
    ReportHeapStatistics("After GC");
  } else if (FLAG_log_gc) {
    new_space_.ReportStatistics();
  }
#else
  if (FLAG_log_gc) new_space_.ReportStatistics();
#endif  // DEBUG
}


void Heap::GarbageCollectionPrologue() {
  {
    AllowHeapAllocation for_the_first_part_of_prologue;
    ClearJSFunctionResultCaches();
    gc_count_++;
    unflattened_strings_length_ = 0;

    if (FLAG_flush_code && FLAG_flush_code_incrementally) {
      mark_compact_collector()->EnableCodeFlushing(true);
    }

#ifdef VERIFY_HEAP
    if (FLAG_verify_heap) {
      Verify();
    }
#endif
  }

  // Reset GC statistics.
  promoted_objects_size_ = 0;
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

  store_buffer()->GCPrologue();

  if (isolate()->concurrent_osr_enabled()) {
    isolate()->optimizing_compiler_thread()->AgeBufferedOsrJobs();
  }

  if (new_space_.IsAtMaximumCapacity()) {
    maximum_size_scavenges_++;
  } else {
    maximum_size_scavenges_ = 0;
  }
  CheckNewSpaceExpansionCriteria();
}


intptr_t Heap::SizeOfObjects() {
  intptr_t total = 0;
  AllSpaces spaces(this);
  for (Space* space = spaces.next(); space != NULL; space = spaces.next()) {
    total += space->SizeOfObjects();
  }
  return total;
}


void Heap::ClearAllICsByKind(Code::Kind kind) {
  HeapObjectIterator it(code_space());

  for (Object* object = it.Next(); object != NULL; object = it.Next()) {
    Code* code = Code::cast(object);
    Code::Kind current_kind = code->kind();
    if (current_kind == Code::FUNCTION ||
        current_kind == Code::OPTIMIZED_FUNCTION) {
      code->ClearInlineCaches(kind);
    }
  }
}


void Heap::RepairFreeListsAfterBoot() {
  PagedSpaces spaces(this);
  for (PagedSpace* space = spaces.next(); space != NULL;
       space = spaces.next()) {
    space->RepairFreeListsAfterBoot();
  }
}


void Heap::ProcessPretenuringFeedback() {
  if (FLAG_allocation_site_pretenuring) {
    int tenure_decisions = 0;
    int dont_tenure_decisions = 0;
    int allocation_mementos_found = 0;
    int allocation_sites = 0;
    int active_allocation_sites = 0;

    // If the scratchpad overflowed, we have to iterate over the allocation
    // sites list.
    // TODO(hpayer): We iterate over the whole list of allocation sites when
    // we grew to the maximum semi-space size to deopt maybe tenured
    // allocation sites. We could hold the maybe tenured allocation sites
    // in a seperate data structure if this is a performance problem.
    bool deopt_maybe_tenured = DeoptMaybeTenuredAllocationSites();
    bool use_scratchpad =
        allocation_sites_scratchpad_length_ < kAllocationSiteScratchpadSize &&
        !deopt_maybe_tenured;

    int i = 0;
    Object* list_element = allocation_sites_list();
    bool trigger_deoptimization = false;
    bool maximum_size_scavenge = MaximumSizeScavenge();
    while (use_scratchpad ? i < allocation_sites_scratchpad_length_
                          : list_element->IsAllocationSite()) {
      AllocationSite* site =
          use_scratchpad
              ? AllocationSite::cast(allocation_sites_scratchpad()->get(i))
              : AllocationSite::cast(list_element);
      allocation_mementos_found += site->memento_found_count();
      if (site->memento_found_count() > 0) {
        active_allocation_sites++;
        if (site->DigestPretenuringFeedback(maximum_size_scavenge)) {
          trigger_deoptimization = true;
        }
        if (site->GetPretenureMode() == TENURED) {
          tenure_decisions++;
        } else {
          dont_tenure_decisions++;
        }
        allocation_sites++;
      }

      if (deopt_maybe_tenured && site->IsMaybeTenure()) {
        site->set_deopt_dependent_code(true);
        trigger_deoptimization = true;
      }

      if (use_scratchpad) {
        i++;
      } else {
        list_element = site->weak_next();
      }
    }

    if (trigger_deoptimization) {
      isolate_->stack_guard()->RequestDeoptMarkedAllocationSites();
    }

    FlushAllocationSitesScratchpad();

    if (FLAG_trace_pretenuring_statistics &&
        (allocation_mementos_found > 0 || tenure_decisions > 0 ||
         dont_tenure_decisions > 0)) {
      PrintF(
          "GC: (mode, #visited allocation sites, #active allocation sites, "
          "#mementos, #tenure decisions, #donttenure decisions) "
          "(%s, %d, %d, %d, %d, %d)\n",
          use_scratchpad ? "use scratchpad" : "use list", allocation_sites,
          active_allocation_sites, allocation_mementos_found, tenure_decisions,
          dont_tenure_decisions);
    }
  }
}


void Heap::DeoptMarkedAllocationSites() {
  // TODO(hpayer): If iterating over the allocation sites list becomes a
  // performance issue, use a cache heap data structure instead (similar to the
  // allocation sites scratchpad).
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
  store_buffer()->GCEpilogue();

  // In release mode, we only zap the from space under heap verification.
  if (Heap::ShouldZapGarbage()) {
    ZapFromSpace();
  }

  // Process pretenuring feedback and update allocation sites.
  ProcessPretenuringFeedback();

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
#endif
  if (FLAG_deopt_every_n_garbage_collections > 0) {
    // TODO(jkummerow/ulan/jarin): This is not safe! We can't assume that
    // the topmost optimized frame can be deoptimized safely, because it
    // might not have a lazy bailout point right after its current PC.
    if (++gcs_since_last_deopt_ == FLAG_deopt_every_n_garbage_collections) {
      Deoptimizer::DeoptimizeAll(isolate());
      gcs_since_last_deopt_ = 0;
    }
  }

  UpdateMaximumCommitted();

  isolate_->counters()->alive_after_last_gc()->Set(
      static_cast<int>(SizeOfObjects()));

  isolate_->counters()->string_table_capacity()->Set(
      string_table()->Capacity());
  isolate_->counters()->number_of_symbols()->Set(
      string_table()->NumberOfElements());

  if (full_codegen_bytes_generated_ + crankshaft_codegen_bytes_generated_ > 0) {
    isolate_->counters()->codegen_fraction_crankshaft()->AddSample(
        static_cast<int>((crankshaft_codegen_bytes_generated_ * 100.0) /
                         (crankshaft_codegen_bytes_generated_ +
                          full_codegen_bytes_generated_)));
  }

  if (CommittedMemory() > 0) {
    isolate_->counters()->external_fragmentation_total()->AddSample(
        static_cast<int>(100 - (SizeOfObjects() * 100.0) / CommittedMemory()));

    isolate_->counters()->heap_fraction_new_space()->AddSample(static_cast<int>(
        (new_space()->CommittedMemory() * 100.0) / CommittedMemory()));
    isolate_->counters()->heap_fraction_old_pointer_space()->AddSample(
        static_cast<int>((old_pointer_space()->CommittedMemory() * 100.0) /
                         CommittedMemory()));
    isolate_->counters()->heap_fraction_old_data_space()->AddSample(
        static_cast<int>((old_data_space()->CommittedMemory() * 100.0) /
                         CommittedMemory()));
    isolate_->counters()->heap_fraction_code_space()->AddSample(
        static_cast<int>((code_space()->CommittedMemory() * 100.0) /
                         CommittedMemory()));
    isolate_->counters()->heap_fraction_map_space()->AddSample(static_cast<int>(
        (map_space()->CommittedMemory() * 100.0) / CommittedMemory()));
    isolate_->counters()->heap_fraction_cell_space()->AddSample(
        static_cast<int>((cell_space()->CommittedMemory() * 100.0) /
                         CommittedMemory()));
    isolate_->counters()->heap_fraction_property_cell_space()->AddSample(
        static_cast<int>((property_cell_space()->CommittedMemory() * 100.0) /
                         CommittedMemory()));
    isolate_->counters()->heap_fraction_lo_space()->AddSample(static_cast<int>(
        (lo_space()->CommittedMemory() * 100.0) / CommittedMemory()));

    isolate_->counters()->heap_sample_total_committed()->AddSample(
        static_cast<int>(CommittedMemory() / KB));
    isolate_->counters()->heap_sample_total_used()->AddSample(
        static_cast<int>(SizeOfObjects() / KB));
    isolate_->counters()->heap_sample_map_space_committed()->AddSample(
        static_cast<int>(map_space()->CommittedMemory() / KB));
    isolate_->counters()->heap_sample_cell_space_committed()->AddSample(
        static_cast<int>(cell_space()->CommittedMemory() / KB));
    isolate_->counters()
        ->heap_sample_property_cell_space_committed()
        ->AddSample(
            static_cast<int>(property_cell_space()->CommittedMemory() / KB));
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
  UPDATE_COUNTERS_AND_FRAGMENTATION_FOR_SPACE(old_pointer_space)
  UPDATE_COUNTERS_AND_FRAGMENTATION_FOR_SPACE(old_data_space)
  UPDATE_COUNTERS_AND_FRAGMENTATION_FOR_SPACE(code_space)
  UPDATE_COUNTERS_AND_FRAGMENTATION_FOR_SPACE(map_space)
  UPDATE_COUNTERS_AND_FRAGMENTATION_FOR_SPACE(cell_space)
  UPDATE_COUNTERS_AND_FRAGMENTATION_FOR_SPACE(property_cell_space)
  UPDATE_COUNTERS_AND_FRAGMENTATION_FOR_SPACE(lo_space)
#undef UPDATE_COUNTERS_FOR_SPACE
#undef UPDATE_FRAGMENTATION_FOR_SPACE
#undef UPDATE_COUNTERS_AND_FRAGMENTATION_FOR_SPACE

#ifdef DEBUG
  ReportStatisticsAfterGC();
#endif  // DEBUG

  // Remember the last top pointer so that we can later find out
  // whether we allocated in new space since the last GC.
  new_space_top_after_last_gc_ = new_space()->top();
}


void Heap::CollectAllGarbage(int flags, const char* gc_reason,
                             const v8::GCCallbackFlags gc_callback_flags) {
  // Since we are ignoring the return value, the exact choice of space does
  // not matter, so long as we do not specify NEW_SPACE, which would not
  // cause a full GC.
  mark_compact_collector_.SetFlags(flags);
  CollectGarbage(OLD_POINTER_SPACE, gc_reason, gc_callback_flags);
  mark_compact_collector_.SetFlags(kNoGCFlags);
}


void Heap::CollectAllAvailableGarbage(const char* gc_reason) {
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
  if (isolate()->concurrent_recompilation_enabled()) {
    // The optimizing compiler may be unnecessarily holding on to memory.
    DisallowHeapAllocation no_recursive_gc;
    isolate()->optimizing_compiler_thread()->Flush();
  }
  mark_compact_collector()->SetFlags(kMakeHeapIterableMask |
                                     kReduceMemoryFootprintMask);
  isolate_->compilation_cache()->Clear();
  const int kMaxNumberOfAttempts = 7;
  const int kMinNumberOfAttempts = 2;
  for (int attempt = 0; attempt < kMaxNumberOfAttempts; attempt++) {
    if (!CollectGarbage(MARK_COMPACTOR, gc_reason, NULL) &&
        attempt + 1 >= kMinNumberOfAttempts) {
      break;
    }
  }
  mark_compact_collector()->SetFlags(kNoGCFlags);
  new_space_.Shrink();
  UncommitFromSpace();
  incremental_marking()->UncommitMarkingDeque();
}


void Heap::EnsureFillerObjectAtTop() {
  // There may be an allocation memento behind every object in new space.
  // If we evacuate a not full new space or if we are on the last page of
  // the new space, then there may be uninitialized memory behind the top
  // pointer of the new space page. We store a filler object there to
  // identify the unused space.
  Address from_top = new_space_.top();
  Address from_limit = new_space_.limit();
  if (from_top < from_limit) {
    int remaining_in_page = static_cast<int>(from_limit - from_top);
    CreateFillerObjectAt(from_top, remaining_in_page);
  }
}


bool Heap::CollectGarbage(GarbageCollector collector, const char* gc_reason,
                          const char* collector_reason,
                          const v8::GCCallbackFlags gc_callback_flags) {
  // The VM is in the GC state until exiting this function.
  VMState<GC> state(isolate_);

#ifdef DEBUG
  // Reset the allocation timeout to the GC interval, but make sure to
  // allow at least a few allocations after a collection. The reason
  // for this is that we have a lot of allocation sequences and we
  // assume that a garbage collection will allow the subsequent
  // allocation attempts to go through.
  allocation_timeout_ = Max(6, FLAG_gc_interval);
#endif

  EnsureFillerObjectAtTop();

  if (collector == SCAVENGER && !incremental_marking()->IsStopped()) {
    if (FLAG_trace_incremental_marking) {
      PrintF("[IncrementalMarking] Scavenge during marking.\n");
    }
  }

  if (collector == MARK_COMPACTOR &&
      !mark_compact_collector()->abort_incremental_marking() &&
      !incremental_marking()->IsStopped() &&
      !incremental_marking()->should_hurry() &&
      FLAG_incremental_marking_steps) {
    // Make progress in incremental marking.
    const intptr_t kStepSizeWhenDelayedByScavenge = 1 * MB;
    incremental_marking()->Step(kStepSizeWhenDelayedByScavenge,
                                IncrementalMarking::NO_GC_VIA_STACK_GUARD);
    if (!incremental_marking()->IsComplete() && !FLAG_gc_global) {
      if (FLAG_trace_incremental_marking) {
        PrintF("[IncrementalMarking] Delaying MarkSweep.\n");
      }
      collector = SCAVENGER;
      collector_reason = "incremental marking delaying mark-sweep";
    }
  }

  bool next_gc_likely_to_collect_more = false;

  {
    tracer()->Start(collector, gc_reason, collector_reason);
    DCHECK(AllowHeapAllocation::IsAllowed());
    DisallowHeapAllocation no_allocation_during_gc;
    GarbageCollectionPrologue();

    {
      HistogramTimerScope histogram_timer_scope(
          (collector == SCAVENGER) ? isolate_->counters()->gc_scavenger()
                                   : isolate_->counters()->gc_compactor());
      next_gc_likely_to_collect_more =
          PerformGarbageCollection(collector, gc_callback_flags);
    }

    GarbageCollectionEpilogue();
    tracer()->Stop();
  }

  // Start incremental marking for the next cycle. The heap snapshot
  // generator needs incremental marking to stay off after it aborted.
  if (!mark_compact_collector()->abort_incremental_marking() &&
      WorthActivatingIncrementalMarking()) {
    incremental_marking()->Start();
  }

  return next_gc_likely_to_collect_more;
}


int Heap::NotifyContextDisposed() {
  if (isolate()->concurrent_recompilation_enabled()) {
    // Flush the queued recompilation tasks.
    isolate()->optimizing_compiler_thread()->Flush();
  }
  flush_monomorphic_ics_ = true;
  AgeInlineCaches();
  return ++contexts_disposed_;
}


void Heap::MoveElements(FixedArray* array, int dst_index, int src_index,
                        int len) {
  if (len == 0) return;

  DCHECK(array->map() != fixed_cow_array_map());
  Object** dst_objects = array->data_start() + dst_index;
  MemMove(dst_objects, array->data_start() + src_index, len * kPointerSize);
  if (!InNewSpace(array)) {
    for (int i = 0; i < len; i++) {
      // TODO(hpayer): check store buffer for entries
      if (InNewSpace(dst_objects[i])) {
        RecordWrite(array->address(), array->OffsetOfElementAt(dst_index + i));
      }
    }
  }
  incremental_marking()->RecordWrites(array);
}


#ifdef VERIFY_HEAP
// Helper class for verifying the string table.
class StringTableVerifier : public ObjectVisitor {
 public:
  void VisitPointers(Object** start, Object** end) {
    // Visit all HeapObject pointers in [start, end).
    for (Object** p = start; p < end; p++) {
      if ((*p)->IsHeapObject()) {
        // Check that the string is actually internalized.
        CHECK((*p)->IsTheHole() || (*p)->IsUndefined() ||
              (*p)->IsInternalizedString());
      }
    }
  }
};


static void VerifyStringTable(Heap* heap) {
  StringTableVerifier verifier;
  heap->string_table()->IterateElements(&verifier);
}
#endif  // VERIFY_HEAP


static bool AbortIncrementalMarkingAndCollectGarbage(
    Heap* heap, AllocationSpace space, const char* gc_reason = NULL) {
  heap->mark_compact_collector()->SetFlags(Heap::kAbortIncrementalMarkingMask);
  bool result = heap->CollectGarbage(space, gc_reason);
  heap->mark_compact_collector()->SetFlags(Heap::kNoGCFlags);
  return result;
}


void Heap::ReserveSpace(int* sizes, Address* locations_out) {
  bool gc_performed = true;
  int counter = 0;
  static const int kThreshold = 20;
  while (gc_performed && counter++ < kThreshold) {
    gc_performed = false;
    for (int space = NEW_SPACE; space < Serializer::kNumberOfSpaces; space++) {
      if (sizes[space] == 0) continue;
      bool perform_gc = false;
      if (space == LO_SPACE) {
        perform_gc = !lo_space()->CanAllocateSize(sizes[space]);
      } else {
        AllocationResult allocation;
        if (space == NEW_SPACE) {
          allocation = new_space()->AllocateRaw(sizes[space]);
        } else {
          allocation = paged_space(space)->AllocateRaw(sizes[space]);
        }
        FreeListNode* node;
        if (allocation.To(&node)) {
          // Mark with a free list node, in case we have a GC before
          // deserializing.
          node->set_size(this, sizes[space]);
          DCHECK(space < Serializer::kNumberOfPreallocatedSpaces);
          locations_out[space] = node->address();
        } else {
          perform_gc = true;
        }
      }
      if (perform_gc) {
        if (space == NEW_SPACE) {
          Heap::CollectGarbage(NEW_SPACE,
                               "failed to reserve space in the new space");
        } else {
          AbortIncrementalMarkingAndCollectGarbage(
              this, static_cast<AllocationSpace>(space),
              "failed to reserve space in paged or large object space");
        }
        gc_performed = true;
        break;  // Abort for-loop over spaces and retry.
      }
    }
  }

  if (gc_performed) {
    // Failed to reserve the space after several attempts.
    V8::FatalProcessOutOfMemory("Heap::ReserveSpace");
  }
}


void Heap::EnsureFromSpaceIsCommitted() {
  if (new_space_.CommitFromSpaceIfNeeded()) return;

  // Committing memory to from space failed.
  // Memory is exhausted and we will die.
  V8::FatalProcessOutOfMemory("Committing semi space failed.");
}


void Heap::ClearJSFunctionResultCaches() {
  if (isolate_->bootstrapper()->IsActive()) return;

  Object* context = native_contexts_list();
  while (!context->IsUndefined()) {
    // Get the caches for this context. GC can happen when the context
    // is not fully initialized, so the caches can be undefined.
    Object* caches_or_undefined =
        Context::cast(context)->get(Context::JSFUNCTION_RESULT_CACHES_INDEX);
    if (!caches_or_undefined->IsUndefined()) {
      FixedArray* caches = FixedArray::cast(caches_or_undefined);
      // Clear the caches:
      int length = caches->length();
      for (int i = 0; i < length; i++) {
        JSFunctionResultCache::cast(caches->get(i))->Clear();
      }
    }
    // Get the next context:
    context = Context::cast(context)->get(Context::NEXT_CONTEXT_LINK);
  }
}


void Heap::ClearNormalizedMapCaches() {
  if (isolate_->bootstrapper()->IsActive() &&
      !incremental_marking()->IsMarking()) {
    return;
  }

  Object* context = native_contexts_list();
  while (!context->IsUndefined()) {
    // GC can happen when the context is not fully initialized,
    // so the cache can be undefined.
    Object* cache =
        Context::cast(context)->get(Context::NORMALIZED_MAP_CACHE_INDEX);
    if (!cache->IsUndefined()) {
      NormalizedMapCache::cast(cache)->Clear();
    }
    context = Context::cast(context)->get(Context::NEXT_CONTEXT_LINK);
  }
}


void Heap::UpdateSurvivalStatistics(int start_new_space_size) {
  if (start_new_space_size == 0) return;

  promotion_rate_ = (static_cast<double>(promoted_objects_size_) /
                     static_cast<double>(start_new_space_size) * 100);

  semi_space_copied_rate_ =
      (static_cast<double>(semi_space_copied_object_size_) /
       static_cast<double>(start_new_space_size) * 100);

  double survival_rate = promotion_rate_ + semi_space_copied_rate_;

  if (survival_rate > kYoungSurvivalRateHighThreshold) {
    high_survival_rate_period_length_++;
  } else {
    high_survival_rate_period_length_ = 0;
  }
}

bool Heap::PerformGarbageCollection(
    GarbageCollector collector, const v8::GCCallbackFlags gc_callback_flags) {
  int freed_global_handles = 0;

  if (collector != SCAVENGER) {
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
      GCTracer::Scope scope(tracer(), GCTracer::Scope::EXTERNAL);
      VMState<EXTERNAL> state(isolate_);
      HandleScope handle_scope(isolate_);
      CallGCPrologueCallbacks(gc_type, kNoGCCallbackFlags);
    }
  }

  EnsureFromSpaceIsCommitted();

  int start_new_space_size = Heap::new_space()->SizeAsInt();

  if (IsHighSurvivalRate()) {
    // We speed up the incremental marker if it is running so that it
    // does not fall behind the rate of promotion, which would cause a
    // constantly growing old space.
    incremental_marking()->NotifyOfHighPromotionRate();
  }

  if (collector == MARK_COMPACTOR) {
    // Perform mark-sweep with optional compaction.
    MarkCompact();
    sweep_generation_++;
    // Temporarily set the limit for case when PostGarbageCollectionProcessing
    // allocates and triggers GC. The real limit is set at after
    // PostGarbageCollectionProcessing.
    old_generation_allocation_limit_ =
        OldGenerationAllocationLimit(PromotedSpaceSizeOfObjects(), 0);
    old_gen_exhausted_ = false;
  } else {
    Scavenge();
  }

  UpdateSurvivalStatistics(start_new_space_size);

  isolate_->counters()->objs_since_last_young()->Set(0);

  // Callbacks that fire after this point might trigger nested GCs and
  // restart incremental marking, the assertion can't be moved down.
  DCHECK(collector == SCAVENGER || incremental_marking()->IsStopped());

  gc_post_processing_depth_++;
  {
    AllowHeapAllocation allow_allocation;
    GCTracer::Scope scope(tracer(), GCTracer::Scope::EXTERNAL);
    freed_global_handles =
        isolate_->global_handles()->PostGarbageCollectionProcessing(collector);
  }
  gc_post_processing_depth_--;

  isolate_->eternal_handles()->PostGarbageCollectionProcessing(this);

  // Update relocatables.
  Relocatable::PostGarbageCollectionProcessing(isolate_);

  if (collector == MARK_COMPACTOR) {
    // Register the amount of external allocated memory.
    amount_of_external_allocated_memory_at_last_global_gc_ =
        amount_of_external_allocated_memory_;
    old_generation_allocation_limit_ = OldGenerationAllocationLimit(
        PromotedSpaceSizeOfObjects(), freed_global_handles);
  }

  {
    GCCallbacksScope scope(this);
    if (scope.CheckReenter()) {
      AllowHeapAllocation allow_allocation;
      GCTracer::Scope scope(tracer(), GCTracer::Scope::EXTERNAL);
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
  for (int i = 0; i < gc_prologue_callbacks_.length(); ++i) {
    if (gc_type & gc_prologue_callbacks_[i].gc_type) {
      if (!gc_prologue_callbacks_[i].pass_isolate_) {
        v8::GCPrologueCallback callback =
            reinterpret_cast<v8::GCPrologueCallback>(
                gc_prologue_callbacks_[i].callback);
        callback(gc_type, flags);
      } else {
        v8::Isolate* isolate = reinterpret_cast<v8::Isolate*>(this->isolate());
        gc_prologue_callbacks_[i].callback(isolate, gc_type, flags);
      }
    }
  }
}


void Heap::CallGCEpilogueCallbacks(GCType gc_type,
                                   GCCallbackFlags gc_callback_flags) {
  for (int i = 0; i < gc_epilogue_callbacks_.length(); ++i) {
    if (gc_type & gc_epilogue_callbacks_[i].gc_type) {
      if (!gc_epilogue_callbacks_[i].pass_isolate_) {
        v8::GCPrologueCallback callback =
            reinterpret_cast<v8::GCPrologueCallback>(
                gc_epilogue_callbacks_[i].callback);
        callback(gc_type, gc_callback_flags);
      } else {
        v8::Isolate* isolate = reinterpret_cast<v8::Isolate*>(this->isolate());
        gc_epilogue_callbacks_[i].callback(isolate, gc_type, gc_callback_flags);
      }
    }
  }
}


void Heap::MarkCompact() {
  gc_state_ = MARK_COMPACT;
  LOG(isolate_, ResourceEvent("markcompact", "begin"));

  uint64_t size_of_objects_before_gc = SizeOfObjects();

  mark_compact_collector_.Prepare();

  ms_count_++;

  MarkCompactPrologue();

  mark_compact_collector_.CollectGarbage();

  LOG(isolate_, ResourceEvent("markcompact", "end"));

  gc_state_ = NOT_IN_GC;

  isolate_->counters()->objs_since_last_full()->Set(0);

  flush_monomorphic_ics_ = false;

  if (FLAG_allocation_site_pretenuring) {
    EvaluateOldSpaceLocalPretenuring(size_of_objects_before_gc);
  }
}


void Heap::MarkCompactPrologue() {
  // At any old GC clear the keyed lookup cache to enable collection of unused
  // maps.
  isolate_->keyed_lookup_cache()->Clear();
  isolate_->context_slot_cache()->Clear();
  isolate_->descriptor_lookup_cache()->Clear();
  RegExpResultsCache::Clear(string_split_cache());
  RegExpResultsCache::Clear(regexp_multiple_cache());

  isolate_->compilation_cache()->MarkCompactPrologue();

  CompletelyClearInstanceofCache();

  FlushNumberStringCache();
  if (FLAG_cleanup_code_caches_at_gc) {
    polymorphic_code_cache()->set_cache(undefined_value());
  }

  ClearNormalizedMapCaches();
}


// Helper class for copying HeapObjects
class ScavengeVisitor : public ObjectVisitor {
 public:
  explicit ScavengeVisitor(Heap* heap) : heap_(heap) {}

  void VisitPointer(Object** p) { ScavengePointer(p); }

  void VisitPointers(Object** start, Object** end) {
    // Copy all HeapObject pointers in [start, end)
    for (Object** p = start; p < end; p++) ScavengePointer(p);
  }

 private:
  void ScavengePointer(Object** p) {
    Object* object = *p;
    if (!heap_->InNewSpace(object)) return;
    Heap::ScavengeObject(reinterpret_cast<HeapObject**>(p),
                         reinterpret_cast<HeapObject*>(object));
  }

  Heap* heap_;
};


#ifdef VERIFY_HEAP
// Visitor class to verify pointers in code or data space do not point into
// new space.
class VerifyNonPointerSpacePointersVisitor : public ObjectVisitor {
 public:
  explicit VerifyNonPointerSpacePointersVisitor(Heap* heap) : heap_(heap) {}
  void VisitPointers(Object** start, Object** end) {
    for (Object** current = start; current < end; current++) {
      if ((*current)->IsHeapObject()) {
        CHECK(!heap_->InNewSpace(HeapObject::cast(*current)));
      }
    }
  }

 private:
  Heap* heap_;
};


static void VerifyNonPointerSpacePointers(Heap* heap) {
  // Verify that there are no pointers to new space in spaces where we
  // do not expect them.
  VerifyNonPointerSpacePointersVisitor v(heap);
  HeapObjectIterator code_it(heap->code_space());
  for (HeapObject* object = code_it.Next(); object != NULL;
       object = code_it.Next())
    object->Iterate(&v);

    HeapObjectIterator data_it(heap->old_data_space());
    for (HeapObject* object = data_it.Next(); object != NULL;
         object = data_it.Next())
      object->Iterate(&v);
}
#endif  // VERIFY_HEAP


void Heap::CheckNewSpaceExpansionCriteria() {
  if (new_space_.TotalCapacity() < new_space_.MaximumCapacity() &&
      survived_since_last_expansion_ > new_space_.TotalCapacity()) {
    // Grow the size of new space if there is room to grow, enough data
    // has survived scavenge since the last expansion and we are not in
    // high promotion mode.
    new_space_.Grow();
    survived_since_last_expansion_ = 0;
  }
}


static bool IsUnscavengedHeapObject(Heap* heap, Object** p) {
  return heap->InNewSpace(*p) &&
         !HeapObject::cast(*p)->map_word().IsForwardingAddress();
}


void Heap::ScavengeStoreBufferCallback(Heap* heap, MemoryChunk* page,
                                       StoreBufferEvent event) {
  heap->store_buffer_rebuilder_.Callback(page, event);
}


void StoreBufferRebuilder::Callback(MemoryChunk* page, StoreBufferEvent event) {
  if (event == kStoreBufferStartScanningPagesEvent) {
    start_of_current_page_ = NULL;
    current_page_ = NULL;
  } else if (event == kStoreBufferScanningPageEvent) {
    if (current_page_ != NULL) {
      // If this page already overflowed the store buffer during this iteration.
      if (current_page_->scan_on_scavenge()) {
        // Then we should wipe out the entries that have been added for it.
        store_buffer_->SetTop(start_of_current_page_);
      } else if (store_buffer_->Top() - start_of_current_page_ >=
                 (store_buffer_->Limit() - store_buffer_->Top()) >> 2) {
        // Did we find too many pointers in the previous page?  The heuristic is
        // that no page can take more then 1/5 the remaining slots in the store
        // buffer.
        current_page_->set_scan_on_scavenge(true);
        store_buffer_->SetTop(start_of_current_page_);
      } else {
        // In this case the page we scanned took a reasonable number of slots in
        // the store buffer.  It has now been rehabilitated and is no longer
        // marked scan_on_scavenge.
        DCHECK(!current_page_->scan_on_scavenge());
      }
    }
    start_of_current_page_ = store_buffer_->Top();
    current_page_ = page;
  } else if (event == kStoreBufferFullEvent) {
    // The current page overflowed the store buffer again.  Wipe out its entries
    // in the store buffer and mark it scan-on-scavenge again.  This may happen
    // several times while scanning.
    if (current_page_ == NULL) {
      // Store Buffer overflowed while scanning promoted objects.  These are not
      // in any particular page, though they are likely to be clustered by the
      // allocation routines.
      store_buffer_->EnsureSpace(StoreBuffer::kStoreBufferSize / 2);
    } else {
      // Store Buffer overflowed while scanning a particular old space page for
      // pointers to new space.
      DCHECK(current_page_ == page);
      DCHECK(page != NULL);
      current_page_->set_scan_on_scavenge(true);
      DCHECK(start_of_current_page_ != store_buffer_->Top());
      store_buffer_->SetTop(start_of_current_page_);
    }
  } else {
    UNREACHABLE();
  }
}


void PromotionQueue::Initialize() {
  // Assumes that a NewSpacePage exactly fits a number of promotion queue
  // entries (where each is a pair of intptr_t). This allows us to simplify
  // the test fpr when to switch pages.
  DCHECK((Page::kPageSize - MemoryChunk::kBodyOffset) % (2 * kPointerSize) ==
         0);
  limit_ = reinterpret_cast<intptr_t*>(heap_->new_space()->ToSpaceStart());
  front_ = rear_ =
      reinterpret_cast<intptr_t*>(heap_->new_space()->ToSpaceEnd());
  emergency_stack_ = NULL;
}


void PromotionQueue::RelocateQueueHead() {
  DCHECK(emergency_stack_ == NULL);

  Page* p = Page::FromAllocationTop(reinterpret_cast<Address>(rear_));
  intptr_t* head_start = rear_;
  intptr_t* head_end = Min(front_, reinterpret_cast<intptr_t*>(p->area_end()));

  int entries_count =
      static_cast<int>(head_end - head_start) / kEntrySizeInWords;

  emergency_stack_ = new List<Entry>(2 * entries_count);

  while (head_start != head_end) {
    int size = static_cast<int>(*(head_start++));
    HeapObject* obj = reinterpret_cast<HeapObject*>(*(head_start++));
    emergency_stack_->Add(Entry(obj, size));
  }
  rear_ = head_end;
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
    return NULL;
  }

 private:
  Heap* heap_;
};


void Heap::Scavenge() {
  RelocationLock relocation_lock(this);

#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) VerifyNonPointerSpacePointers(this);
#endif

  gc_state_ = SCAVENGE;

  // Implements Cheney's copying algorithm
  LOG(isolate_, ResourceEvent("scavenge", "begin"));

  // Clear descriptor cache.
  isolate_->descriptor_lookup_cache()->Clear();

  // Used for updating survived_since_last_expansion_ at function end.
  intptr_t survived_watermark = PromotedSpaceSizeOfObjects();

  SelectScavengingVisitorsTable();

  incremental_marking()->PrepareForScavenge();

  // Flip the semispaces.  After flipping, to space is empty, from space has
  // live objects.
  new_space_.Flip();
  new_space_.ResetAllocationInfo();

  // We need to sweep newly copied objects which can be either in the
  // to space or promoted to the old generation.  For to-space
  // objects, we treat the bottom of the to space as a queue.  Newly
  // copied and unswept objects lie between a 'front' mark and the
  // allocation pointer.
  //
  // Promoted objects can go into various old-generation spaces, and
  // can be allocated internally in the spaces (from the free list).
  // We treat the top of the to space as a queue of addresses of
  // promoted objects.  The addresses of newly promoted and unswept
  // objects lie between a 'front' mark and a 'rear' mark that is
  // updated as a side effect of promoting an object.
  //
  // There is guaranteed to be enough room at the top of the to space
  // for the addresses of promoted objects: every object promoted
  // frees up its size in bytes from the top of the new space, and
  // objects are at least one pointer in size.
  Address new_space_front = new_space_.ToSpaceStart();
  promotion_queue_.Initialize();

#ifdef DEBUG
  store_buffer()->Clean();
#endif

  ScavengeVisitor scavenge_visitor(this);
  // Copy roots.
  IterateRoots(&scavenge_visitor, VISIT_ALL_IN_SCAVENGE);

  // Copy objects reachable from the old generation.
  {
    StoreBufferRebuildScope scope(this, store_buffer(),
                                  &ScavengeStoreBufferCallback);
    store_buffer()->IteratePointersToNewSpace(&ScavengeObject);
  }

  // Copy objects reachable from simple cells by scavenging cell values
  // directly.
  HeapObjectIterator cell_iterator(cell_space_);
  for (HeapObject* heap_object = cell_iterator.Next(); heap_object != NULL;
       heap_object = cell_iterator.Next()) {
    if (heap_object->IsCell()) {
      Cell* cell = Cell::cast(heap_object);
      Address value_address = cell->ValueAddress();
      scavenge_visitor.VisitPointer(reinterpret_cast<Object**>(value_address));
    }
  }

  // Copy objects reachable from global property cells by scavenging global
  // property cell values directly.
  HeapObjectIterator js_global_property_cell_iterator(property_cell_space_);
  for (HeapObject* heap_object = js_global_property_cell_iterator.Next();
       heap_object != NULL;
       heap_object = js_global_property_cell_iterator.Next()) {
    if (heap_object->IsPropertyCell()) {
      PropertyCell* cell = PropertyCell::cast(heap_object);
      Address value_address = cell->ValueAddress();
      scavenge_visitor.VisitPointer(reinterpret_cast<Object**>(value_address));
      Address type_address = cell->TypeAddress();
      scavenge_visitor.VisitPointer(reinterpret_cast<Object**>(type_address));
    }
  }

  // Copy objects reachable from the encountered weak collections list.
  scavenge_visitor.VisitPointer(&encountered_weak_collections_);

  // Copy objects reachable from the code flushing candidates list.
  MarkCompactCollector* collector = mark_compact_collector();
  if (collector->is_code_flushing_enabled()) {
    collector->code_flusher()->IteratePointersToFromSpace(&scavenge_visitor);
  }

  new_space_front = DoScavenge(&scavenge_visitor, new_space_front);

  while (isolate()->global_handles()->IterateObjectGroups(
      &scavenge_visitor, &IsUnscavengedHeapObject)) {
    new_space_front = DoScavenge(&scavenge_visitor, new_space_front);
  }
  isolate()->global_handles()->RemoveObjectGroups();
  isolate()->global_handles()->RemoveImplicitRefGroups();

  isolate_->global_handles()->IdentifyNewSpaceWeakIndependentHandles(
      &IsUnscavengedHeapObject);
  isolate_->global_handles()->IterateNewSpaceWeakIndependentRoots(
      &scavenge_visitor);
  new_space_front = DoScavenge(&scavenge_visitor, new_space_front);

  UpdateNewSpaceReferencesInExternalStringTable(
      &UpdateNewSpaceReferenceInExternalStringTableEntry);

  promotion_queue_.Destroy();

  incremental_marking()->UpdateMarkingDequeAfterScavenge();

  ScavengeWeakObjectRetainer weak_object_retainer(this);
  ProcessWeakReferences(&weak_object_retainer);

  DCHECK(new_space_front == new_space_.top());

  // Set age mark.
  new_space_.set_age_mark(new_space_.top());

  new_space_.LowerInlineAllocationLimit(
      new_space_.inline_allocation_limit_step());

  // Update how much has survived scavenge.
  IncrementYoungSurvivorsCounter(static_cast<int>(
      (PromotedSpaceSizeOfObjects() - survived_watermark) + new_space_.Size()));

  LOG(isolate_, ResourceEvent("scavenge", "end"));

  gc_state_ = NOT_IN_GC;
}


String* Heap::UpdateNewSpaceReferenceInExternalStringTableEntry(Heap* heap,
                                                                Object** p) {
  MapWord first_word = HeapObject::cast(*p)->map_word();

  if (!first_word.IsForwardingAddress()) {
    // Unreachable external string can be finalized.
    heap->FinalizeExternalString(String::cast(*p));
    return NULL;
  }

  // String is still reachable.
  return String::cast(first_word.ToForwardingAddress());
}


void Heap::UpdateNewSpaceReferencesInExternalStringTable(
    ExternalStringTableUpdaterCallback updater_func) {
#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) {
    external_string_table_.Verify();
  }
#endif

  if (external_string_table_.new_space_strings_.is_empty()) return;

  Object** start = &external_string_table_.new_space_strings_[0];
  Object** end = start + external_string_table_.new_space_strings_.length();
  Object** last = start;

  for (Object** p = start; p < end; ++p) {
    DCHECK(InFromSpace(*p));
    String* target = updater_func(this, p);

    if (target == NULL) continue;

    DCHECK(target->IsExternalString());

    if (InNewSpace(target)) {
      // String is still in new space.  Update the table entry.
      *last = target;
      ++last;
    } else {
      // String got promoted.  Move it to the old string list.
      external_string_table_.AddOldString(target);
    }
  }

  DCHECK(last <= end);
  external_string_table_.ShrinkNewStrings(static_cast<int>(last - start));
}


void Heap::UpdateReferencesInExternalStringTable(
    ExternalStringTableUpdaterCallback updater_func) {
  // Update old space string references.
  if (external_string_table_.old_space_strings_.length() > 0) {
    Object** start = &external_string_table_.old_space_strings_[0];
    Object** end = start + external_string_table_.old_space_strings_.length();
    for (Object** p = start; p < end; ++p) *p = updater_func(this, p);
  }

  UpdateNewSpaceReferencesInExternalStringTable(updater_func);
}


void Heap::ProcessWeakReferences(WeakObjectRetainer* retainer) {
  ProcessArrayBuffers(retainer);
  ProcessNativeContexts(retainer);
  // TODO(mvstanton): AllocationSites only need to be processed during
  // MARK_COMPACT, as they live in old space. Verify and address.
  ProcessAllocationSites(retainer);
}


void Heap::ProcessNativeContexts(WeakObjectRetainer* retainer) {
  Object* head = VisitWeakList<Context>(this, native_contexts_list(), retainer);
  // Update the head of the list of contexts.
  set_native_contexts_list(head);
}


void Heap::ProcessArrayBuffers(WeakObjectRetainer* retainer) {
  Object* array_buffer_obj =
      VisitWeakList<JSArrayBuffer>(this, array_buffers_list(), retainer);
  set_array_buffers_list(array_buffer_obj);
}


void Heap::TearDownArrayBuffers() {
  Object* undefined = undefined_value();
  for (Object* o = array_buffers_list(); o != undefined;) {
    JSArrayBuffer* buffer = JSArrayBuffer::cast(o);
    Runtime::FreeArrayBuffer(isolate(), buffer);
    o = buffer->weak_next();
  }
  set_array_buffers_list(undefined);
}


void Heap::ProcessAllocationSites(WeakObjectRetainer* retainer) {
  Object* allocation_site_obj =
      VisitWeakList<AllocationSite>(this, allocation_sites_list(), retainer);
  set_allocation_sites_list(allocation_site_obj);
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

  class ExternalStringTableVisitorAdapter : public ObjectVisitor {
   public:
    explicit ExternalStringTableVisitorAdapter(
        v8::ExternalResourceVisitor* visitor)
        : visitor_(visitor) {}
    virtual void VisitPointers(Object** start, Object** end) {
      for (Object** p = start; p < end; p++) {
        DCHECK((*p)->IsExternalString());
        visitor_->VisitExternalString(
            Utils::ToLocal(Handle<String>(String::cast(*p))));
      }
    }

   private:
    v8::ExternalResourceVisitor* visitor_;
  } external_string_table_visitor(visitor);

  external_string_table_.Iterate(&external_string_table_visitor);
}


class NewSpaceScavenger : public StaticNewSpaceVisitor<NewSpaceScavenger> {
 public:
  static inline void VisitPointer(Heap* heap, Object** p) {
    Object* object = *p;
    if (!heap->InNewSpace(object)) return;
    Heap::ScavengeObject(reinterpret_cast<HeapObject**>(p),
                         reinterpret_cast<HeapObject*>(object));
  }
};


Address Heap::DoScavenge(ObjectVisitor* scavenge_visitor,
                         Address new_space_front) {
  do {
    SemiSpace::AssertValidRange(new_space_front, new_space_.top());
    // The addresses new_space_front and new_space_.top() define a
    // queue of unprocessed copied objects.  Process them until the
    // queue is empty.
    while (new_space_front != new_space_.top()) {
      if (!NewSpacePage::IsAtEnd(new_space_front)) {
        HeapObject* object = HeapObject::FromAddress(new_space_front);
        new_space_front +=
            NewSpaceScavenger::IterateBody(object->map(), object);
      } else {
        new_space_front =
            NewSpacePage::FromLimit(new_space_front)->next_page()->area_start();
      }
    }

    // Promote and process all the to-be-promoted objects.
    {
      StoreBufferRebuildScope scope(this, store_buffer(),
                                    &ScavengeStoreBufferCallback);
      while (!promotion_queue()->is_empty()) {
        HeapObject* target;
        int size;
        promotion_queue()->remove(&target, &size);

        // Promoted object might be already partially visited
        // during old space pointer iteration. Thus we search specificly
        // for pointers to from semispace instead of looking for pointers
        // to new space.
        DCHECK(!target->IsMap());
        IterateAndMarkPointersToFromSpace(
            target->address(), target->address() + size, &ScavengeObject);
      }
    }

    // Take another spin if there are now unswept objects in new space
    // (there are currently no more unswept promoted objects).
  } while (new_space_front != new_space_.top());

  return new_space_front;
}


STATIC_ASSERT((FixedDoubleArray::kHeaderSize & kDoubleAlignmentMask) ==
              0);  // NOLINT
STATIC_ASSERT((ConstantPoolArray::kFirstEntryOffset & kDoubleAlignmentMask) ==
              0);  // NOLINT
STATIC_ASSERT((ConstantPoolArray::kExtendedFirstOffset &
               kDoubleAlignmentMask) == 0);  // NOLINT


INLINE(static HeapObject* EnsureDoubleAligned(Heap* heap, HeapObject* object,
                                              int size));

static HeapObject* EnsureDoubleAligned(Heap* heap, HeapObject* object,
                                       int size) {
  if ((OffsetFrom(object->address()) & kDoubleAlignmentMask) != 0) {
    heap->CreateFillerObjectAt(object->address(), kPointerSize);
    return HeapObject::FromAddress(object->address() + kPointerSize);
  } else {
    heap->CreateFillerObjectAt(object->address() + size - kPointerSize,
                               kPointerSize);
    return object;
  }
}


enum LoggingAndProfiling {
  LOGGING_AND_PROFILING_ENABLED,
  LOGGING_AND_PROFILING_DISABLED
};


enum MarksHandling { TRANSFER_MARKS, IGNORE_MARKS };


template <MarksHandling marks_handling,
          LoggingAndProfiling logging_and_profiling_mode>
class ScavengingVisitor : public StaticVisitorBase {
 public:
  static void Initialize() {
    table_.Register(kVisitSeqOneByteString, &EvacuateSeqOneByteString);
    table_.Register(kVisitSeqTwoByteString, &EvacuateSeqTwoByteString);
    table_.Register(kVisitShortcutCandidate, &EvacuateShortcutCandidate);
    table_.Register(kVisitByteArray, &EvacuateByteArray);
    table_.Register(kVisitFixedArray, &EvacuateFixedArray);
    table_.Register(kVisitFixedDoubleArray, &EvacuateFixedDoubleArray);
    table_.Register(kVisitFixedTypedArray, &EvacuateFixedTypedArray);
    table_.Register(kVisitFixedFloat64Array, &EvacuateFixedFloat64Array);

    table_.Register(
        kVisitNativeContext,
        &ObjectEvacuationStrategy<POINTER_OBJECT>::template VisitSpecialized<
            Context::kSize>);

    table_.Register(
        kVisitConsString,
        &ObjectEvacuationStrategy<POINTER_OBJECT>::template VisitSpecialized<
            ConsString::kSize>);

    table_.Register(
        kVisitSlicedString,
        &ObjectEvacuationStrategy<POINTER_OBJECT>::template VisitSpecialized<
            SlicedString::kSize>);

    table_.Register(
        kVisitSymbol,
        &ObjectEvacuationStrategy<POINTER_OBJECT>::template VisitSpecialized<
            Symbol::kSize>);

    table_.Register(
        kVisitSharedFunctionInfo,
        &ObjectEvacuationStrategy<POINTER_OBJECT>::template VisitSpecialized<
            SharedFunctionInfo::kSize>);

    table_.Register(kVisitJSWeakCollection,
                    &ObjectEvacuationStrategy<POINTER_OBJECT>::Visit);

    table_.Register(kVisitJSArrayBuffer,
                    &ObjectEvacuationStrategy<POINTER_OBJECT>::Visit);

    table_.Register(kVisitJSTypedArray,
                    &ObjectEvacuationStrategy<POINTER_OBJECT>::Visit);

    table_.Register(kVisitJSDataView,
                    &ObjectEvacuationStrategy<POINTER_OBJECT>::Visit);

    table_.Register(kVisitJSRegExp,
                    &ObjectEvacuationStrategy<POINTER_OBJECT>::Visit);

    if (marks_handling == IGNORE_MARKS) {
      table_.Register(
          kVisitJSFunction,
          &ObjectEvacuationStrategy<POINTER_OBJECT>::template VisitSpecialized<
              JSFunction::kSize>);
    } else {
      table_.Register(kVisitJSFunction, &EvacuateJSFunction);
    }

    table_.RegisterSpecializations<ObjectEvacuationStrategy<DATA_OBJECT>,
                                   kVisitDataObject, kVisitDataObjectGeneric>();

    table_.RegisterSpecializations<ObjectEvacuationStrategy<POINTER_OBJECT>,
                                   kVisitJSObject, kVisitJSObjectGeneric>();

    table_.RegisterSpecializations<ObjectEvacuationStrategy<POINTER_OBJECT>,
                                   kVisitStruct, kVisitStructGeneric>();
  }

  static VisitorDispatchTable<ScavengingCallback>* GetTable() {
    return &table_;
  }

 private:
  enum ObjectContents { DATA_OBJECT, POINTER_OBJECT };

  static void RecordCopiedObject(Heap* heap, HeapObject* obj) {
    bool should_record = false;
#ifdef DEBUG
    should_record = FLAG_heap_stats;
#endif
    should_record = should_record || FLAG_log_gc;
    if (should_record) {
      if (heap->new_space()->Contains(obj)) {
        heap->new_space()->RecordAllocation(obj);
      } else {
        heap->new_space()->RecordPromotion(obj);
      }
    }
  }

  // Helper function used by CopyObject to copy a source object to an
  // allocated target object and update the forwarding pointer in the source
  // object.  Returns the target object.
  INLINE(static void MigrateObject(Heap* heap, HeapObject* source,
                                   HeapObject* target, int size)) {
    // If we migrate into to-space, then the to-space top pointer should be
    // right after the target object. Incorporate double alignment
    // over-allocation.
    DCHECK(!heap->InToSpace(target) ||
           target->address() + size == heap->new_space()->top() ||
           target->address() + size + kPointerSize == heap->new_space()->top());

    // Make sure that we do not overwrite the promotion queue which is at
    // the end of to-space.
    DCHECK(!heap->InToSpace(target) ||
           heap->promotion_queue()->IsBelowPromotionQueue(
               heap->new_space()->top()));

    // Copy the content of source to target.
    heap->CopyBlock(target->address(), source->address(), size);

    // Set the forwarding address.
    source->set_map_word(MapWord::FromForwardingAddress(target));

    if (logging_and_profiling_mode == LOGGING_AND_PROFILING_ENABLED) {
      // Update NewSpace stats if necessary.
      RecordCopiedObject(heap, target);
      heap->OnMoveEvent(target, source, size);
    }

    if (marks_handling == TRANSFER_MARKS) {
      if (Marking::TransferColor(source, target)) {
        MemoryChunk::IncrementLiveBytesFromGC(target->address(), size);
      }
    }
  }

  template <int alignment>
  static inline bool SemiSpaceCopyObject(Map* map, HeapObject** slot,
                                         HeapObject* object, int object_size) {
    Heap* heap = map->GetHeap();

    int allocation_size = object_size;
    if (alignment != kObjectAlignment) {
      DCHECK(alignment == kDoubleAlignment);
      allocation_size += kPointerSize;
    }

    DCHECK(heap->AllowedToBeMigrated(object, NEW_SPACE));
    AllocationResult allocation =
        heap->new_space()->AllocateRaw(allocation_size);

    HeapObject* target = NULL;  // Initialization to please compiler.
    if (allocation.To(&target)) {
      // Order is important here: Set the promotion limit before storing a
      // filler for double alignment or migrating the object. Otherwise we
      // may end up overwriting promotion queue entries when we migrate the
      // object.
      heap->promotion_queue()->SetNewLimit(heap->new_space()->top());

      if (alignment != kObjectAlignment) {
        target = EnsureDoubleAligned(heap, target, allocation_size);
      }

      // Order is important: slot might be inside of the target if target
      // was allocated over a dead object and slot comes from the store
      // buffer.
      *slot = target;
      MigrateObject(heap, object, target, object_size);

      heap->IncrementSemiSpaceCopiedObjectSize(object_size);
      return true;
    }
    return false;
  }


  template <ObjectContents object_contents, int alignment>
  static inline bool PromoteObject(Map* map, HeapObject** slot,
                                   HeapObject* object, int object_size) {
    Heap* heap = map->GetHeap();

    int allocation_size = object_size;
    if (alignment != kObjectAlignment) {
      DCHECK(alignment == kDoubleAlignment);
      allocation_size += kPointerSize;
    }

    AllocationResult allocation;
    if (object_contents == DATA_OBJECT) {
      DCHECK(heap->AllowedToBeMigrated(object, OLD_DATA_SPACE));
      allocation = heap->old_data_space()->AllocateRaw(allocation_size);
    } else {
      DCHECK(heap->AllowedToBeMigrated(object, OLD_POINTER_SPACE));
      allocation = heap->old_pointer_space()->AllocateRaw(allocation_size);
    }

    HeapObject* target = NULL;  // Initialization to please compiler.
    if (allocation.To(&target)) {
      if (alignment != kObjectAlignment) {
        target = EnsureDoubleAligned(heap, target, allocation_size);
      }

      // Order is important: slot might be inside of the target if target
      // was allocated over a dead object and slot comes from the store
      // buffer.
      *slot = target;
      MigrateObject(heap, object, target, object_size);

      if (object_contents == POINTER_OBJECT) {
        if (map->instance_type() == JS_FUNCTION_TYPE) {
          heap->promotion_queue()->insert(target,
                                          JSFunction::kNonWeakFieldsEndOffset);
        } else {
          heap->promotion_queue()->insert(target, object_size);
        }
      }
      heap->IncrementPromotedObjectsSize(object_size);
      return true;
    }
    return false;
  }


  template <ObjectContents object_contents, int alignment>
  static inline void EvacuateObject(Map* map, HeapObject** slot,
                                    HeapObject* object, int object_size) {
    SLOW_DCHECK(object_size <= Page::kMaxRegularHeapObjectSize);
    SLOW_DCHECK(object->Size() == object_size);
    Heap* heap = map->GetHeap();

    if (!heap->ShouldBePromoted(object->address(), object_size)) {
      // A semi-space copy may fail due to fragmentation. In that case, we
      // try to promote the object.
      if (SemiSpaceCopyObject<alignment>(map, slot, object, object_size)) {
        return;
      }
    }

    if (PromoteObject<object_contents, alignment>(map, slot, object,
                                                  object_size)) {
      return;
    }

    // If promotion failed, we try to copy the object to the other semi-space
    if (SemiSpaceCopyObject<alignment>(map, slot, object, object_size)) return;

    UNREACHABLE();
  }


  static inline void EvacuateJSFunction(Map* map, HeapObject** slot,
                                        HeapObject* object) {
    ObjectEvacuationStrategy<POINTER_OBJECT>::template VisitSpecialized<
        JSFunction::kSize>(map, slot, object);

    MapWord map_word = object->map_word();
    DCHECK(map_word.IsForwardingAddress());
    HeapObject* target = map_word.ToForwardingAddress();

    MarkBit mark_bit = Marking::MarkBitFrom(target);
    if (Marking::IsBlack(mark_bit)) {
      // This object is black and it might not be rescanned by marker.
      // We should explicitly record code entry slot for compaction because
      // promotion queue processing (IterateAndMarkPointersToFromSpace) will
      // miss it as it is not HeapObject-tagged.
      Address code_entry_slot =
          target->address() + JSFunction::kCodeEntryOffset;
      Code* code = Code::cast(Code::GetObjectFromEntryAddress(code_entry_slot));
      map->GetHeap()->mark_compact_collector()->RecordCodeEntrySlot(
          code_entry_slot, code);
    }
  }


  static inline void EvacuateFixedArray(Map* map, HeapObject** slot,
                                        HeapObject* object) {
    int object_size = FixedArray::BodyDescriptor::SizeOf(map, object);
    EvacuateObject<POINTER_OBJECT, kObjectAlignment>(map, slot, object,
                                                     object_size);
  }


  static inline void EvacuateFixedDoubleArray(Map* map, HeapObject** slot,
                                              HeapObject* object) {
    int length = reinterpret_cast<FixedDoubleArray*>(object)->length();
    int object_size = FixedDoubleArray::SizeFor(length);
    EvacuateObject<DATA_OBJECT, kDoubleAlignment>(map, slot, object,
                                                  object_size);
  }


  static inline void EvacuateFixedTypedArray(Map* map, HeapObject** slot,
                                             HeapObject* object) {
    int object_size = reinterpret_cast<FixedTypedArrayBase*>(object)->size();
    EvacuateObject<DATA_OBJECT, kObjectAlignment>(map, slot, object,
                                                  object_size);
  }


  static inline void EvacuateFixedFloat64Array(Map* map, HeapObject** slot,
                                               HeapObject* object) {
    int object_size = reinterpret_cast<FixedFloat64Array*>(object)->size();
    EvacuateObject<DATA_OBJECT, kDoubleAlignment>(map, slot, object,
                                                  object_size);
  }


  static inline void EvacuateByteArray(Map* map, HeapObject** slot,
                                       HeapObject* object) {
    int object_size = reinterpret_cast<ByteArray*>(object)->ByteArraySize();
    EvacuateObject<DATA_OBJECT, kObjectAlignment>(map, slot, object,
                                                  object_size);
  }


  static inline void EvacuateSeqOneByteString(Map* map, HeapObject** slot,
                                              HeapObject* object) {
    int object_size = SeqOneByteString::cast(object)
                          ->SeqOneByteStringSize(map->instance_type());
    EvacuateObject<DATA_OBJECT, kObjectAlignment>(map, slot, object,
                                                  object_size);
  }


  static inline void EvacuateSeqTwoByteString(Map* map, HeapObject** slot,
                                              HeapObject* object) {
    int object_size = SeqTwoByteString::cast(object)
                          ->SeqTwoByteStringSize(map->instance_type());
    EvacuateObject<DATA_OBJECT, kObjectAlignment>(map, slot, object,
                                                  object_size);
  }


  static inline void EvacuateShortcutCandidate(Map* map, HeapObject** slot,
                                               HeapObject* object) {
    DCHECK(IsShortcutCandidate(map->instance_type()));

    Heap* heap = map->GetHeap();

    if (marks_handling == IGNORE_MARKS &&
        ConsString::cast(object)->unchecked_second() == heap->empty_string()) {
      HeapObject* first =
          HeapObject::cast(ConsString::cast(object)->unchecked_first());

      *slot = first;

      if (!heap->InNewSpace(first)) {
        object->set_map_word(MapWord::FromForwardingAddress(first));
        return;
      }

      MapWord first_word = first->map_word();
      if (first_word.IsForwardingAddress()) {
        HeapObject* target = first_word.ToForwardingAddress();

        *slot = target;
        object->set_map_word(MapWord::FromForwardingAddress(target));
        return;
      }

      heap->DoScavengeObject(first->map(), slot, first);
      object->set_map_word(MapWord::FromForwardingAddress(*slot));
      return;
    }

    int object_size = ConsString::kSize;
    EvacuateObject<POINTER_OBJECT, kObjectAlignment>(map, slot, object,
                                                     object_size);
  }

  template <ObjectContents object_contents>
  class ObjectEvacuationStrategy {
   public:
    template <int object_size>
    static inline void VisitSpecialized(Map* map, HeapObject** slot,
                                        HeapObject* object) {
      EvacuateObject<object_contents, kObjectAlignment>(map, slot, object,
                                                        object_size);
    }

    static inline void Visit(Map* map, HeapObject** slot, HeapObject* object) {
      int object_size = map->instance_size();
      EvacuateObject<object_contents, kObjectAlignment>(map, slot, object,
                                                        object_size);
    }
  };

  static VisitorDispatchTable<ScavengingCallback> table_;
};


template <MarksHandling marks_handling,
          LoggingAndProfiling logging_and_profiling_mode>
VisitorDispatchTable<ScavengingCallback>
    ScavengingVisitor<marks_handling, logging_and_profiling_mode>::table_;


static void InitializeScavengingVisitorsTables() {
  ScavengingVisitor<TRANSFER_MARKS,
                    LOGGING_AND_PROFILING_DISABLED>::Initialize();
  ScavengingVisitor<IGNORE_MARKS, LOGGING_AND_PROFILING_DISABLED>::Initialize();
  ScavengingVisitor<TRANSFER_MARKS,
                    LOGGING_AND_PROFILING_ENABLED>::Initialize();
  ScavengingVisitor<IGNORE_MARKS, LOGGING_AND_PROFILING_ENABLED>::Initialize();
}


void Heap::SelectScavengingVisitorsTable() {
  bool logging_and_profiling =
      FLAG_verify_predictable || isolate()->logger()->is_logging() ||
      isolate()->cpu_profiler()->is_profiling() ||
      (isolate()->heap_profiler() != NULL &&
       isolate()->heap_profiler()->is_tracking_object_moves());

  if (!incremental_marking()->IsMarking()) {
    if (!logging_and_profiling) {
      scavenging_visitors_table_.CopyFrom(ScavengingVisitor<
          IGNORE_MARKS, LOGGING_AND_PROFILING_DISABLED>::GetTable());
    } else {
      scavenging_visitors_table_.CopyFrom(ScavengingVisitor<
          IGNORE_MARKS, LOGGING_AND_PROFILING_ENABLED>::GetTable());
    }
  } else {
    if (!logging_and_profiling) {
      scavenging_visitors_table_.CopyFrom(ScavengingVisitor<
          TRANSFER_MARKS, LOGGING_AND_PROFILING_DISABLED>::GetTable());
    } else {
      scavenging_visitors_table_.CopyFrom(ScavengingVisitor<
          TRANSFER_MARKS, LOGGING_AND_PROFILING_ENABLED>::GetTable());
    }

    if (incremental_marking()->IsCompacting()) {
      // When compacting forbid short-circuiting of cons-strings.
      // Scavenging code relies on the fact that new space object
      // can't be evacuated into evacuation candidate but
      // short-circuiting violates this assumption.
      scavenging_visitors_table_.Register(
          StaticVisitorBase::kVisitShortcutCandidate,
          scavenging_visitors_table_.GetVisitorById(
              StaticVisitorBase::kVisitConsString));
    }
  }
}


void Heap::ScavengeObjectSlow(HeapObject** p, HeapObject* object) {
  SLOW_DCHECK(object->GetIsolate()->heap()->InFromSpace(object));
  MapWord first_word = object->map_word();
  SLOW_DCHECK(!first_word.IsForwardingAddress());
  Map* map = first_word.ToMap();
  map->GetHeap()->DoScavengeObject(map, p, object);
}


AllocationResult Heap::AllocatePartialMap(InstanceType instance_type,
                                          int instance_size) {
  Object* result;
  AllocationResult allocation = AllocateRaw(Map::kSize, MAP_SPACE, MAP_SPACE);
  if (!allocation.To(&result)) return allocation;

  // Map::cast cannot be used due to uninitialized map field.
  reinterpret_cast<Map*>(result)->set_map(raw_unchecked_meta_map());
  reinterpret_cast<Map*>(result)->set_instance_type(instance_type);
  reinterpret_cast<Map*>(result)->set_instance_size(instance_size);
  reinterpret_cast<Map*>(result)->set_visitor_id(
      StaticVisitorBase::GetVisitorId(instance_type, instance_size));
  reinterpret_cast<Map*>(result)->set_inobject_properties(0);
  reinterpret_cast<Map*>(result)->set_pre_allocated_property_fields(0);
  reinterpret_cast<Map*>(result)->set_unused_property_fields(0);
  reinterpret_cast<Map*>(result)->set_bit_field(0);
  reinterpret_cast<Map*>(result)->set_bit_field2(0);
  int bit_field3 = Map::EnumLengthBits::encode(kInvalidEnumCacheSentinel) |
                   Map::OwnsDescriptors::encode(true);
  reinterpret_cast<Map*>(result)->set_bit_field3(bit_field3);
  return result;
}


AllocationResult Heap::AllocateMap(InstanceType instance_type,
                                   int instance_size,
                                   ElementsKind elements_kind) {
  HeapObject* result;
  AllocationResult allocation = AllocateRaw(Map::kSize, MAP_SPACE, MAP_SPACE);
  if (!allocation.To(&result)) return allocation;

  result->set_map_no_write_barrier(meta_map());
  Map* map = Map::cast(result);
  map->set_instance_type(instance_type);
  map->set_visitor_id(
      StaticVisitorBase::GetVisitorId(instance_type, instance_size));
  map->set_prototype(null_value(), SKIP_WRITE_BARRIER);
  map->set_constructor(null_value(), SKIP_WRITE_BARRIER);
  map->set_instance_size(instance_size);
  map->set_inobject_properties(0);
  map->set_pre_allocated_property_fields(0);
  map->set_code_cache(empty_fixed_array(), SKIP_WRITE_BARRIER);
  map->set_dependent_code(DependentCode::cast(empty_fixed_array()),
                          SKIP_WRITE_BARRIER);
  map->init_back_pointer(undefined_value());
  map->set_unused_property_fields(0);
  map->set_instance_descriptors(empty_descriptor_array());
  map->set_bit_field(0);
  map->set_bit_field2(1 << Map::kIsExtensible);
  int bit_field3 = Map::EnumLengthBits::encode(kInvalidEnumCacheSentinel) |
                   Map::OwnsDescriptors::encode(true);
  map->set_bit_field3(bit_field3);
  map->set_elements_kind(elements_kind);

  return map;
}


AllocationResult Heap::AllocateFillerObject(int size, bool double_align,
                                            AllocationSpace space) {
  HeapObject* obj;
  {
    AllocationResult allocation = AllocateRaw(size, space, space);
    if (!allocation.To(&obj)) return allocation;
  }
#ifdef DEBUG
  MemoryChunk* chunk = MemoryChunk::FromAddress(obj->address());
  DCHECK(chunk->owner()->identity() == space);
#endif
  CreateFillerObjectAt(obj->address(), size);
  return obj;
}


const Heap::StringTypeTable Heap::string_type_table[] = {
#define STRING_TYPE_ELEMENT(type, size, name, camel_name) \
  { type, size, k##camel_name##MapRootIndex }             \
  ,
    STRING_TYPE_LIST(STRING_TYPE_ELEMENT)
#undef STRING_TYPE_ELEMENT
};


const Heap::ConstantStringTable Heap::constant_string_table[] = {
#define CONSTANT_STRING_ELEMENT(name, contents) \
  { contents, k##name##RootIndex }              \
  ,
    INTERNALIZED_STRING_LIST(CONSTANT_STRING_ELEMENT)
#undef CONSTANT_STRING_ELEMENT
};


const Heap::StructTable Heap::struct_table[] = {
#define STRUCT_TABLE_ELEMENT(NAME, Name, name)        \
  { NAME##_TYPE, Name::kSize, k##Name##MapRootIndex } \
  ,
    STRUCT_LIST(STRUCT_TABLE_ELEMENT)
#undef STRUCT_TABLE_ELEMENT
};


bool Heap::CreateInitialMaps() {
  HeapObject* obj;
  {
    AllocationResult allocation = AllocatePartialMap(MAP_TYPE, Map::kSize);
    if (!allocation.To(&obj)) return false;
  }
  // Map::cast cannot be used due to uninitialized map field.
  Map* new_meta_map = reinterpret_cast<Map*>(obj);
  set_meta_map(new_meta_map);
  new_meta_map->set_map(new_meta_map);

  {  // Partial map allocation
#define ALLOCATE_PARTIAL_MAP(instance_type, size, field_name)                \
  {                                                                          \
    Map* map;                                                                \
    if (!AllocatePartialMap((instance_type), (size)).To(&map)) return false; \
    set_##field_name##_map(map);                                             \
  }

    ALLOCATE_PARTIAL_MAP(FIXED_ARRAY_TYPE, kVariableSizeSentinel, fixed_array);
    ALLOCATE_PARTIAL_MAP(ODDBALL_TYPE, Oddball::kSize, undefined);
    ALLOCATE_PARTIAL_MAP(ODDBALL_TYPE, Oddball::kSize, null);
    ALLOCATE_PARTIAL_MAP(CONSTANT_POOL_ARRAY_TYPE, kVariableSizeSentinel,
                         constant_pool_array);

#undef ALLOCATE_PARTIAL_MAP
  }

  // Allocate the empty array.
  {
    AllocationResult allocation = AllocateEmptyFixedArray();
    if (!allocation.To(&obj)) return false;
  }
  set_empty_fixed_array(FixedArray::cast(obj));

  {
    AllocationResult allocation = Allocate(null_map(), OLD_POINTER_SPACE);
    if (!allocation.To(&obj)) return false;
  }
  set_null_value(Oddball::cast(obj));
  Oddball::cast(obj)->set_kind(Oddball::kNull);

  {
    AllocationResult allocation = Allocate(undefined_map(), OLD_POINTER_SPACE);
    if (!allocation.To(&obj)) return false;
  }
  set_undefined_value(Oddball::cast(obj));
  Oddball::cast(obj)->set_kind(Oddball::kUndefined);
  DCHECK(!InNewSpace(undefined_value()));

  // Set preliminary exception sentinel value before actually initializing it.
  set_exception(null_value());

  // Allocate the empty descriptor array.
  {
    AllocationResult allocation = AllocateEmptyFixedArray();
    if (!allocation.To(&obj)) return false;
  }
  set_empty_descriptor_array(DescriptorArray::cast(obj));

  // Allocate the constant pool array.
  {
    AllocationResult allocation = AllocateEmptyConstantPoolArray();
    if (!allocation.To(&obj)) return false;
  }
  set_empty_constant_pool_array(ConstantPoolArray::cast(obj));

  // Fix the instance_descriptors for the existing maps.
  meta_map()->set_code_cache(empty_fixed_array());
  meta_map()->set_dependent_code(DependentCode::cast(empty_fixed_array()));
  meta_map()->init_back_pointer(undefined_value());
  meta_map()->set_instance_descriptors(empty_descriptor_array());

  fixed_array_map()->set_code_cache(empty_fixed_array());
  fixed_array_map()->set_dependent_code(
      DependentCode::cast(empty_fixed_array()));
  fixed_array_map()->init_back_pointer(undefined_value());
  fixed_array_map()->set_instance_descriptors(empty_descriptor_array());

  undefined_map()->set_code_cache(empty_fixed_array());
  undefined_map()->set_dependent_code(DependentCode::cast(empty_fixed_array()));
  undefined_map()->init_back_pointer(undefined_value());
  undefined_map()->set_instance_descriptors(empty_descriptor_array());

  null_map()->set_code_cache(empty_fixed_array());
  null_map()->set_dependent_code(DependentCode::cast(empty_fixed_array()));
  null_map()->init_back_pointer(undefined_value());
  null_map()->set_instance_descriptors(empty_descriptor_array());

  constant_pool_array_map()->set_code_cache(empty_fixed_array());
  constant_pool_array_map()->set_dependent_code(
      DependentCode::cast(empty_fixed_array()));
  constant_pool_array_map()->init_back_pointer(undefined_value());
  constant_pool_array_map()->set_instance_descriptors(empty_descriptor_array());

  // Fix prototype object for existing maps.
  meta_map()->set_prototype(null_value());
  meta_map()->set_constructor(null_value());

  fixed_array_map()->set_prototype(null_value());
  fixed_array_map()->set_constructor(null_value());

  undefined_map()->set_prototype(null_value());
  undefined_map()->set_constructor(null_value());

  null_map()->set_prototype(null_value());
  null_map()->set_constructor(null_value());

  constant_pool_array_map()->set_prototype(null_value());
  constant_pool_array_map()->set_constructor(null_value());

  {  // Map allocation
#define ALLOCATE_MAP(instance_type, size, field_name)               \
  {                                                                 \
    Map* map;                                                       \
    if (!AllocateMap((instance_type), size).To(&map)) return false; \
    set_##field_name##_map(map);                                    \
  }

#define ALLOCATE_VARSIZE_MAP(instance_type, field_name) \
  ALLOCATE_MAP(instance_type, kVariableSizeSentinel, field_name)

    ALLOCATE_VARSIZE_MAP(FIXED_ARRAY_TYPE, fixed_cow_array)
    DCHECK(fixed_array_map() != fixed_cow_array_map());

    ALLOCATE_VARSIZE_MAP(FIXED_ARRAY_TYPE, scope_info)
    ALLOCATE_MAP(HEAP_NUMBER_TYPE, HeapNumber::kSize, heap_number)
    ALLOCATE_MAP(MUTABLE_HEAP_NUMBER_TYPE, HeapNumber::kSize,
                 mutable_heap_number)
    ALLOCATE_MAP(SYMBOL_TYPE, Symbol::kSize, symbol)
    ALLOCATE_MAP(FOREIGN_TYPE, Foreign::kSize, foreign)

    ALLOCATE_MAP(ODDBALL_TYPE, Oddball::kSize, the_hole);
    ALLOCATE_MAP(ODDBALL_TYPE, Oddball::kSize, boolean);
    ALLOCATE_MAP(ODDBALL_TYPE, Oddball::kSize, uninitialized);
    ALLOCATE_MAP(ODDBALL_TYPE, Oddball::kSize, arguments_marker);
    ALLOCATE_MAP(ODDBALL_TYPE, Oddball::kSize, no_interceptor_result_sentinel);
    ALLOCATE_MAP(ODDBALL_TYPE, Oddball::kSize, exception);
    ALLOCATE_MAP(ODDBALL_TYPE, Oddball::kSize, termination_exception);

    for (unsigned i = 0; i < arraysize(string_type_table); i++) {
      const StringTypeTable& entry = string_type_table[i];
      {
        AllocationResult allocation = AllocateMap(entry.type, entry.size);
        if (!allocation.To(&obj)) return false;
      }
      // Mark cons string maps as unstable, because their objects can change
      // maps during GC.
      Map* map = Map::cast(obj);
      if (StringShape(entry.type).IsCons()) map->mark_unstable();
      roots_[entry.index] = map;
    }

    ALLOCATE_VARSIZE_MAP(STRING_TYPE, undetectable_string)
    undetectable_string_map()->set_is_undetectable();

    ALLOCATE_VARSIZE_MAP(ONE_BYTE_STRING_TYPE, undetectable_one_byte_string);
    undetectable_one_byte_string_map()->set_is_undetectable();

    ALLOCATE_VARSIZE_MAP(FIXED_DOUBLE_ARRAY_TYPE, fixed_double_array)
    ALLOCATE_VARSIZE_MAP(BYTE_ARRAY_TYPE, byte_array)
    ALLOCATE_VARSIZE_MAP(FREE_SPACE_TYPE, free_space)

#define ALLOCATE_EXTERNAL_ARRAY_MAP(Type, type, TYPE, ctype, size)        \
  ALLOCATE_MAP(EXTERNAL_##TYPE##_ARRAY_TYPE, ExternalArray::kAlignedSize, \
               external_##type##_array)

    TYPED_ARRAYS(ALLOCATE_EXTERNAL_ARRAY_MAP)
#undef ALLOCATE_EXTERNAL_ARRAY_MAP

#define ALLOCATE_FIXED_TYPED_ARRAY_MAP(Type, type, TYPE, ctype, size) \
  ALLOCATE_VARSIZE_MAP(FIXED_##TYPE##_ARRAY_TYPE, fixed_##type##_array)

    TYPED_ARRAYS(ALLOCATE_FIXED_TYPED_ARRAY_MAP)
#undef ALLOCATE_FIXED_TYPED_ARRAY_MAP

    ALLOCATE_VARSIZE_MAP(FIXED_ARRAY_TYPE, sloppy_arguments_elements)

    ALLOCATE_VARSIZE_MAP(CODE_TYPE, code)

    ALLOCATE_MAP(CELL_TYPE, Cell::kSize, cell)
    ALLOCATE_MAP(PROPERTY_CELL_TYPE, PropertyCell::kSize, global_property_cell)
    ALLOCATE_MAP(FILLER_TYPE, kPointerSize, one_pointer_filler)
    ALLOCATE_MAP(FILLER_TYPE, 2 * kPointerSize, two_pointer_filler)


    for (unsigned i = 0; i < arraysize(struct_table); i++) {
      const StructTable& entry = struct_table[i];
      Map* map;
      if (!AllocateMap(entry.type, entry.size).To(&map)) return false;
      roots_[entry.index] = map;
    }

    ALLOCATE_VARSIZE_MAP(FIXED_ARRAY_TYPE, hash_table)
    ALLOCATE_VARSIZE_MAP(FIXED_ARRAY_TYPE, ordered_hash_table)

    ALLOCATE_VARSIZE_MAP(FIXED_ARRAY_TYPE, function_context)
    ALLOCATE_VARSIZE_MAP(FIXED_ARRAY_TYPE, catch_context)
    ALLOCATE_VARSIZE_MAP(FIXED_ARRAY_TYPE, with_context)
    ALLOCATE_VARSIZE_MAP(FIXED_ARRAY_TYPE, block_context)
    ALLOCATE_VARSIZE_MAP(FIXED_ARRAY_TYPE, module_context)
    ALLOCATE_VARSIZE_MAP(FIXED_ARRAY_TYPE, global_context)

    ALLOCATE_VARSIZE_MAP(FIXED_ARRAY_TYPE, native_context)
    native_context_map()->set_dictionary_map(true);
    native_context_map()->set_visitor_id(
        StaticVisitorBase::kVisitNativeContext);

    ALLOCATE_MAP(SHARED_FUNCTION_INFO_TYPE, SharedFunctionInfo::kAlignedSize,
                 shared_function_info)

    ALLOCATE_MAP(JS_MESSAGE_OBJECT_TYPE, JSMessageObject::kSize, message_object)
    ALLOCATE_MAP(JS_OBJECT_TYPE, JSObject::kHeaderSize + kPointerSize, external)
    external_map()->set_is_extensible(false);
#undef ALLOCATE_VARSIZE_MAP
#undef ALLOCATE_MAP
  }

  {  // Empty arrays
    {
      ByteArray* byte_array;
      if (!AllocateByteArray(0, TENURED).To(&byte_array)) return false;
      set_empty_byte_array(byte_array);
    }

#define ALLOCATE_EMPTY_EXTERNAL_ARRAY(Type, type, TYPE, ctype, size)  \
  {                                                                   \
    ExternalArray* obj;                                               \
    if (!AllocateEmptyExternalArray(kExternal##Type##Array).To(&obj)) \
      return false;                                                   \
    set_empty_external_##type##_array(obj);                           \
  }

    TYPED_ARRAYS(ALLOCATE_EMPTY_EXTERNAL_ARRAY)
#undef ALLOCATE_EMPTY_EXTERNAL_ARRAY

#define ALLOCATE_EMPTY_FIXED_TYPED_ARRAY(Type, type, TYPE, ctype, size) \
  {                                                                     \
    FixedTypedArrayBase* obj;                                           \
    if (!AllocateEmptyFixedTypedArray(kExternal##Type##Array).To(&obj)) \
      return false;                                                     \
    set_empty_fixed_##type##_array(obj);                                \
  }

    TYPED_ARRAYS(ALLOCATE_EMPTY_FIXED_TYPED_ARRAY)
#undef ALLOCATE_EMPTY_FIXED_TYPED_ARRAY
  }
  DCHECK(!InNewSpace(empty_fixed_array()));
  return true;
}


AllocationResult Heap::AllocateHeapNumber(double value, MutableMode mode,
                                          PretenureFlag pretenure) {
  // Statically ensure that it is safe to allocate heap numbers in paged
  // spaces.
  int size = HeapNumber::kSize;
  STATIC_ASSERT(HeapNumber::kSize <= Page::kMaxRegularHeapObjectSize);

  AllocationSpace space = SelectSpace(size, OLD_DATA_SPACE, pretenure);

  HeapObject* result;
  {
    AllocationResult allocation = AllocateRaw(size, space, OLD_DATA_SPACE);
    if (!allocation.To(&result)) return allocation;
  }

  Map* map = mode == MUTABLE ? mutable_heap_number_map() : heap_number_map();
  HeapObject::cast(result)->set_map_no_write_barrier(map);
  HeapNumber::cast(result)->set_value(value);
  return result;
}


AllocationResult Heap::AllocateCell(Object* value) {
  int size = Cell::kSize;
  STATIC_ASSERT(Cell::kSize <= Page::kMaxRegularHeapObjectSize);

  HeapObject* result;
  {
    AllocationResult allocation = AllocateRaw(size, CELL_SPACE, CELL_SPACE);
    if (!allocation.To(&result)) return allocation;
  }
  result->set_map_no_write_barrier(cell_map());
  Cell::cast(result)->set_value(value);
  return result;
}


AllocationResult Heap::AllocatePropertyCell() {
  int size = PropertyCell::kSize;
  STATIC_ASSERT(PropertyCell::kSize <= Page::kMaxRegularHeapObjectSize);

  HeapObject* result;
  AllocationResult allocation =
      AllocateRaw(size, PROPERTY_CELL_SPACE, PROPERTY_CELL_SPACE);
  if (!allocation.To(&result)) return allocation;

  result->set_map_no_write_barrier(global_property_cell_map());
  PropertyCell* cell = PropertyCell::cast(result);
  cell->set_dependent_code(DependentCode::cast(empty_fixed_array()),
                           SKIP_WRITE_BARRIER);
  cell->set_value(the_hole_value());
  cell->set_type(HeapType::None());
  return result;
}


void Heap::CreateApiObjects() {
  HandleScope scope(isolate());
  Factory* factory = isolate()->factory();
  Handle<Map> new_neander_map =
      factory->NewMap(JS_OBJECT_TYPE, JSObject::kHeaderSize);

  // Don't use Smi-only elements optimizations for objects with the neander
  // map. There are too many cases where element values are set directly with a
  // bottleneck to trap the Smi-only -> fast elements transition, and there
  // appears to be no benefit for optimize this case.
  new_neander_map->set_elements_kind(TERMINAL_FAST_ELEMENTS_KIND);
  set_neander_map(*new_neander_map);

  Handle<JSObject> listeners = factory->NewNeanderObject();
  Handle<FixedArray> elements = factory->NewFixedArray(2);
  elements->set(0, Smi::FromInt(0));
  listeners->set_elements(*elements);
  set_message_listeners(*listeners);
}


void Heap::CreateJSEntryStub() {
  JSEntryStub stub(isolate(), StackFrame::ENTRY);
  set_js_entry_code(*stub.GetCode());
}


void Heap::CreateJSConstructEntryStub() {
  JSEntryStub stub(isolate(), StackFrame::ENTRY_CONSTRUCT);
  set_js_construct_entry_code(*stub.GetCode());
}


void Heap::CreateFixedStubs() {
  // Here we create roots for fixed stubs. They are needed at GC
  // for cooking and uncooking (check out frames.cc).
  // The eliminates the need for doing dictionary lookup in the
  // stub cache for these stubs.
  HandleScope scope(isolate());

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


void Heap::CreateInitialObjects() {
  HandleScope scope(isolate());
  Factory* factory = isolate()->factory();

  // The -0 value must be set before NewNumber works.
  set_minus_zero_value(*factory->NewHeapNumber(-0.0, IMMUTABLE, TENURED));
  DCHECK(std::signbit(minus_zero_value()->Number()) != 0);

  set_nan_value(
      *factory->NewHeapNumber(base::OS::nan_value(), IMMUTABLE, TENURED));
  set_infinity_value(*factory->NewHeapNumber(V8_INFINITY, IMMUTABLE, TENURED));

  // The hole has not been created yet, but we want to put something
  // predictable in the gaps in the string table, so lets make that Smi zero.
  set_the_hole_value(reinterpret_cast<Oddball*>(Smi::FromInt(0)));

  // Allocate initial string table.
  set_string_table(*StringTable::New(isolate(), kInitialStringTableSize));

  // Finish initializing oddballs after creating the string table.
  Oddball::Initialize(isolate(), factory->undefined_value(), "undefined",
                      factory->nan_value(), Oddball::kUndefined);

  // Initialize the null_value.
  Oddball::Initialize(isolate(), factory->null_value(), "null",
                      handle(Smi::FromInt(0), isolate()), Oddball::kNull);

  set_true_value(*factory->NewOddball(factory->boolean_map(), "true",
                                      handle(Smi::FromInt(1), isolate()),
                                      Oddball::kTrue));

  set_false_value(*factory->NewOddball(factory->boolean_map(), "false",
                                       handle(Smi::FromInt(0), isolate()),
                                       Oddball::kFalse));

  set_the_hole_value(*factory->NewOddball(factory->the_hole_map(), "hole",
                                          handle(Smi::FromInt(-1), isolate()),
                                          Oddball::kTheHole));

  set_uninitialized_value(*factory->NewOddball(
      factory->uninitialized_map(), "uninitialized",
      handle(Smi::FromInt(-1), isolate()), Oddball::kUninitialized));

  set_arguments_marker(*factory->NewOddball(
      factory->arguments_marker_map(), "arguments_marker",
      handle(Smi::FromInt(-4), isolate()), Oddball::kArgumentMarker));

  set_no_interceptor_result_sentinel(*factory->NewOddball(
      factory->no_interceptor_result_sentinel_map(),
      "no_interceptor_result_sentinel", handle(Smi::FromInt(-2), isolate()),
      Oddball::kOther));

  set_termination_exception(*factory->NewOddball(
      factory->termination_exception_map(), "termination_exception",
      handle(Smi::FromInt(-3), isolate()), Oddball::kOther));

  set_exception(*factory->NewOddball(factory->exception_map(), "exception",
                                     handle(Smi::FromInt(-5), isolate()),
                                     Oddball::kException));

  for (unsigned i = 0; i < arraysize(constant_string_table); i++) {
    Handle<String> str =
        factory->InternalizeUtf8String(constant_string_table[i].contents);
    roots_[constant_string_table[i].index] = *str;
  }

  // Allocate the hidden string which is used to identify the hidden properties
  // in JSObjects. The hash code has a special value so that it will not match
  // the empty string when searching for the property. It cannot be part of the
  // loop above because it needs to be allocated manually with the special
  // hash code in place. The hash code for the hidden_string is zero to ensure
  // that it will always be at the first entry in property descriptors.
  hidden_string_ = *factory->NewOneByteInternalizedString(
      OneByteVector("", 0), String::kEmptyStringHash);

  // Create the code_stubs dictionary. The initial size is set to avoid
  // expanding the dictionary during bootstrapping.
  set_code_stubs(*UnseededNumberDictionary::New(isolate(), 128));

  // Create the non_monomorphic_cache used in stub-cache.cc. The initial size
  // is set to avoid expanding the dictionary during bootstrapping.
  set_non_monomorphic_cache(*UnseededNumberDictionary::New(isolate(), 64));

  set_polymorphic_code_cache(PolymorphicCodeCache::cast(
      *factory->NewStruct(POLYMORPHIC_CODE_CACHE_TYPE)));

  set_instanceof_cache_function(Smi::FromInt(0));
  set_instanceof_cache_map(Smi::FromInt(0));
  set_instanceof_cache_answer(Smi::FromInt(0));

  CreateFixedStubs();

  // Allocate the dictionary of intrinsic function names.
  Handle<NameDictionary> intrinsic_names =
      NameDictionary::New(isolate(), Runtime::kNumFunctions, TENURED);
  Runtime::InitializeIntrinsicFunctionNames(isolate(), intrinsic_names);
  set_intrinsic_function_names(*intrinsic_names);

  set_number_string_cache(
      *factory->NewFixedArray(kInitialNumberStringCacheSize * 2, TENURED));

  // Allocate cache for single character one byte strings.
  set_single_character_string_cache(
      *factory->NewFixedArray(String::kMaxOneByteCharCode + 1, TENURED));

  // Allocate cache for string split and regexp-multiple.
  set_string_split_cache(*factory->NewFixedArray(
      RegExpResultsCache::kRegExpResultsCacheSize, TENURED));
  set_regexp_multiple_cache(*factory->NewFixedArray(
      RegExpResultsCache::kRegExpResultsCacheSize, TENURED));

  // Allocate cache for external strings pointing to native source code.
  set_natives_source_cache(
      *factory->NewFixedArray(Natives::GetBuiltinsCount()));

  set_undefined_cell(*factory->NewCell(factory->undefined_value()));

  // The symbol registry is initialized lazily.
  set_symbol_registry(undefined_value());

  // Allocate object to hold object observation state.
  set_observation_state(*factory->NewJSObjectFromMap(
      factory->NewMap(JS_OBJECT_TYPE, JSObject::kHeaderSize)));

  // Microtask queue uses the empty fixed array as a sentinel for "empty".
  // Number of queued microtasks stored in Isolate::pending_microtask_count().
  set_microtask_queue(empty_fixed_array());

  set_detailed_stack_trace_symbol(*factory->NewPrivateOwnSymbol());
  set_elements_transition_symbol(*factory->NewPrivateOwnSymbol());
  set_frozen_symbol(*factory->NewPrivateOwnSymbol());
  set_megamorphic_symbol(*factory->NewPrivateOwnSymbol());
  set_premonomorphic_symbol(*factory->NewPrivateOwnSymbol());
  set_generic_symbol(*factory->NewPrivateOwnSymbol());
  set_nonexistent_symbol(*factory->NewPrivateOwnSymbol());
  set_normal_ic_symbol(*factory->NewPrivateOwnSymbol());
  set_observed_symbol(*factory->NewPrivateOwnSymbol());
  set_stack_trace_symbol(*factory->NewPrivateOwnSymbol());
  set_uninitialized_symbol(*factory->NewPrivateOwnSymbol());
  set_home_object_symbol(*factory->NewPrivateOwnSymbol());

  Handle<SeededNumberDictionary> slow_element_dictionary =
      SeededNumberDictionary::New(isolate(), 0, TENURED);
  slow_element_dictionary->set_requires_slow_elements();
  set_empty_slow_element_dictionary(*slow_element_dictionary);

  set_materialized_objects(*factory->NewFixedArray(0, TENURED));

  // Handling of script id generation is in Factory::NewScript.
  set_last_script_id(Smi::FromInt(v8::UnboundScript::kNoScriptId));

  set_allocation_sites_scratchpad(
      *factory->NewFixedArray(kAllocationSiteScratchpadSize, TENURED));
  InitializeAllocationSitesScratchpad();

  // Initialize keyed lookup cache.
  isolate_->keyed_lookup_cache()->Clear();

  // Initialize context slot cache.
  isolate_->context_slot_cache()->Clear();

  // Initialize descriptor cache.
  isolate_->descriptor_lookup_cache()->Clear();

  // Initialize compilation cache.
  isolate_->compilation_cache()->Clear();
}


bool Heap::RootCanBeWrittenAfterInitialization(Heap::RootListIndex root_index) {
  RootListIndex writable_roots[] = {
      kStoreBufferTopRootIndex,
      kStackLimitRootIndex,
      kNumberStringCacheRootIndex,
      kInstanceofCacheFunctionRootIndex,
      kInstanceofCacheMapRootIndex,
      kInstanceofCacheAnswerRootIndex,
      kCodeStubsRootIndex,
      kNonMonomorphicCacheRootIndex,
      kPolymorphicCodeCacheRootIndex,
      kLastScriptIdRootIndex,
      kEmptyScriptRootIndex,
      kRealStackLimitRootIndex,
      kArgumentsAdaptorDeoptPCOffsetRootIndex,
      kConstructStubDeoptPCOffsetRootIndex,
      kGetterStubDeoptPCOffsetRootIndex,
      kSetterStubDeoptPCOffsetRootIndex,
      kStringTableRootIndex,
  };

  for (unsigned int i = 0; i < arraysize(writable_roots); i++) {
    if (root_index == writable_roots[i]) return true;
  }
  return false;
}


bool Heap::RootCanBeTreatedAsConstant(RootListIndex root_index) {
  return !RootCanBeWrittenAfterInitialization(root_index) &&
         !InNewSpace(roots_array_start()[root_index]);
}


Object* RegExpResultsCache::Lookup(Heap* heap, String* key_string,
                                   Object* key_pattern, ResultsCacheType type) {
  FixedArray* cache;
  if (!key_string->IsInternalizedString()) return Smi::FromInt(0);
  if (type == STRING_SPLIT_SUBSTRINGS) {
    DCHECK(key_pattern->IsString());
    if (!key_pattern->IsInternalizedString()) return Smi::FromInt(0);
    cache = heap->string_split_cache();
  } else {
    DCHECK(type == REGEXP_MULTIPLE_INDICES);
    DCHECK(key_pattern->IsFixedArray());
    cache = heap->regexp_multiple_cache();
  }

  uint32_t hash = key_string->Hash();
  uint32_t index = ((hash & (kRegExpResultsCacheSize - 1)) &
                    ~(kArrayEntriesPerCacheEntry - 1));
  if (cache->get(index + kStringOffset) == key_string &&
      cache->get(index + kPatternOffset) == key_pattern) {
    return cache->get(index + kArrayOffset);
  }
  index =
      ((index + kArrayEntriesPerCacheEntry) & (kRegExpResultsCacheSize - 1));
  if (cache->get(index + kStringOffset) == key_string &&
      cache->get(index + kPatternOffset) == key_pattern) {
    return cache->get(index + kArrayOffset);
  }
  return Smi::FromInt(0);
}


void RegExpResultsCache::Enter(Isolate* isolate, Handle<String> key_string,
                               Handle<Object> key_pattern,
                               Handle<FixedArray> value_array,
                               ResultsCacheType type) {
  Factory* factory = isolate->factory();
  Handle<FixedArray> cache;
  if (!key_string->IsInternalizedString()) return;
  if (type == STRING_SPLIT_SUBSTRINGS) {
    DCHECK(key_pattern->IsString());
    if (!key_pattern->IsInternalizedString()) return;
    cache = factory->string_split_cache();
  } else {
    DCHECK(type == REGEXP_MULTIPLE_INDICES);
    DCHECK(key_pattern->IsFixedArray());
    cache = factory->regexp_multiple_cache();
  }

  uint32_t hash = key_string->Hash();
  uint32_t index = ((hash & (kRegExpResultsCacheSize - 1)) &
                    ~(kArrayEntriesPerCacheEntry - 1));
  if (cache->get(index + kStringOffset) == Smi::FromInt(0)) {
    cache->set(index + kStringOffset, *key_string);
    cache->set(index + kPatternOffset, *key_pattern);
    cache->set(index + kArrayOffset, *value_array);
  } else {
    uint32_t index2 =
        ((index + kArrayEntriesPerCacheEntry) & (kRegExpResultsCacheSize - 1));
    if (cache->get(index2 + kStringOffset) == Smi::FromInt(0)) {
      cache->set(index2 + kStringOffset, *key_string);
      cache->set(index2 + kPatternOffset, *key_pattern);
      cache->set(index2 + kArrayOffset, *value_array);
    } else {
      cache->set(index2 + kStringOffset, Smi::FromInt(0));
      cache->set(index2 + kPatternOffset, Smi::FromInt(0));
      cache->set(index2 + kArrayOffset, Smi::FromInt(0));
      cache->set(index + kStringOffset, *key_string);
      cache->set(index + kPatternOffset, *key_pattern);
      cache->set(index + kArrayOffset, *value_array);
    }
  }
  // If the array is a reasonably short list of substrings, convert it into a
  // list of internalized strings.
  if (type == STRING_SPLIT_SUBSTRINGS && value_array->length() < 100) {
    for (int i = 0; i < value_array->length(); i++) {
      Handle<String> str(String::cast(value_array->get(i)), isolate);
      Handle<String> internalized_str = factory->InternalizeString(str);
      value_array->set(i, *internalized_str);
    }
  }
  // Convert backing store to a copy-on-write array.
  value_array->set_map_no_write_barrier(*factory->fixed_cow_array_map());
}


void RegExpResultsCache::Clear(FixedArray* cache) {
  for (int i = 0; i < kRegExpResultsCacheSize; i++) {
    cache->set(i, Smi::FromInt(0));
  }
}


int Heap::FullSizeNumberStringCacheLength() {
  // Compute the size of the number string cache based on the max newspace size.
  // The number string cache has a minimum size based on twice the initial cache
  // size to ensure that it is bigger after being made 'full size'.
  int number_string_cache_size = max_semi_space_size_ / 512;
  number_string_cache_size = Max(kInitialNumberStringCacheSize * 2,
                                 Min(0x4000, number_string_cache_size));
  // There is a string and a number per entry so the length is twice the number
  // of entries.
  return number_string_cache_size * 2;
}


void Heap::FlushNumberStringCache() {
  // Flush the number to string cache.
  int len = number_string_cache()->length();
  for (int i = 0; i < len; i++) {
    number_string_cache()->set_undefined(i);
  }
}


void Heap::FlushAllocationSitesScratchpad() {
  for (int i = 0; i < allocation_sites_scratchpad_length_; i++) {
    allocation_sites_scratchpad()->set_undefined(i);
  }
  allocation_sites_scratchpad_length_ = 0;
}


void Heap::InitializeAllocationSitesScratchpad() {
  DCHECK(allocation_sites_scratchpad()->length() ==
         kAllocationSiteScratchpadSize);
  for (int i = 0; i < kAllocationSiteScratchpadSize; i++) {
    allocation_sites_scratchpad()->set_undefined(i);
  }
}


void Heap::AddAllocationSiteToScratchpad(AllocationSite* site,
                                         ScratchpadSlotMode mode) {
  if (allocation_sites_scratchpad_length_ < kAllocationSiteScratchpadSize) {
    // We cannot use the normal write-barrier because slots need to be
    // recorded with non-incremental marking as well. We have to explicitly
    // record the slot to take evacuation candidates into account.
    allocation_sites_scratchpad()->set(allocation_sites_scratchpad_length_,
                                       site, SKIP_WRITE_BARRIER);
    Object** slot = allocation_sites_scratchpad()->RawFieldOfElementAt(
        allocation_sites_scratchpad_length_);

    if (mode == RECORD_SCRATCHPAD_SLOT) {
      // We need to allow slots buffer overflow here since the evacuation
      // candidates are not part of the global list of old space pages and
      // releasing an evacuation candidate due to a slots buffer overflow
      // results in lost pages.
      mark_compact_collector()->RecordSlot(slot, slot, *slot,
                                           SlotsBuffer::IGNORE_OVERFLOW);
    }
    allocation_sites_scratchpad_length_++;
  }
}


Map* Heap::MapForExternalArrayType(ExternalArrayType array_type) {
  return Map::cast(roots_[RootIndexForExternalArrayType(array_type)]);
}


Heap::RootListIndex Heap::RootIndexForExternalArrayType(
    ExternalArrayType array_type) {
  switch (array_type) {
#define ARRAY_TYPE_TO_ROOT_INDEX(Type, type, TYPE, ctype, size) \
  case kExternal##Type##Array:                                  \
    return kExternal##Type##ArrayMapRootIndex;

    TYPED_ARRAYS(ARRAY_TYPE_TO_ROOT_INDEX)
#undef ARRAY_TYPE_TO_ROOT_INDEX

    default:
      UNREACHABLE();
      return kUndefinedValueRootIndex;
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
      return kUndefinedValueRootIndex;
  }
}


Heap::RootListIndex Heap::RootIndexForEmptyExternalArray(
    ElementsKind elementsKind) {
  switch (elementsKind) {
#define ELEMENT_KIND_TO_ROOT_INDEX(Type, type, TYPE, ctype, size) \
  case EXTERNAL_##TYPE##_ELEMENTS:                                \
    return kEmptyExternal##Type##ArrayRootIndex;

    TYPED_ARRAYS(ELEMENT_KIND_TO_ROOT_INDEX)
#undef ELEMENT_KIND_TO_ROOT_INDEX

    default:
      UNREACHABLE();
      return kUndefinedValueRootIndex;
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
      return kUndefinedValueRootIndex;
  }
}


ExternalArray* Heap::EmptyExternalArrayForMap(Map* map) {
  return ExternalArray::cast(
      roots_[RootIndexForEmptyExternalArray(map->elements_kind())]);
}


FixedTypedArrayBase* Heap::EmptyFixedTypedArrayForMap(Map* map) {
  return FixedTypedArrayBase::cast(
      roots_[RootIndexForEmptyFixedTypedArray(map->elements_kind())]);
}


AllocationResult Heap::AllocateForeign(Address address,
                                       PretenureFlag pretenure) {
  // Statically ensure that it is safe to allocate foreigns in paged spaces.
  STATIC_ASSERT(Foreign::kSize <= Page::kMaxRegularHeapObjectSize);
  AllocationSpace space = (pretenure == TENURED) ? OLD_DATA_SPACE : NEW_SPACE;
  Foreign* result;
  AllocationResult allocation = Allocate(foreign_map(), space);
  if (!allocation.To(&result)) return allocation;
  result->set_foreign_address(address);
  return result;
}


AllocationResult Heap::AllocateByteArray(int length, PretenureFlag pretenure) {
  if (length < 0 || length > ByteArray::kMaxLength) {
    v8::internal::Heap::FatalProcessOutOfMemory("invalid array length", true);
  }
  int size = ByteArray::SizeFor(length);
  AllocationSpace space = SelectSpace(size, OLD_DATA_SPACE, pretenure);
  HeapObject* result;
  {
    AllocationResult allocation = AllocateRaw(size, space, OLD_DATA_SPACE);
    if (!allocation.To(&result)) return allocation;
  }

  result->set_map_no_write_barrier(byte_array_map());
  ByteArray::cast(result)->set_length(length);
  return result;
}


void Heap::CreateFillerObjectAt(Address addr, int size) {
  if (size == 0) return;
  HeapObject* filler = HeapObject::FromAddress(addr);
  if (size == kPointerSize) {
    filler->set_map_no_write_barrier(one_pointer_filler_map());
  } else if (size == 2 * kPointerSize) {
    filler->set_map_no_write_barrier(two_pointer_filler_map());
  } else {
    filler->set_map_no_write_barrier(free_space_map());
    FreeSpace::cast(filler)->set_size(size);
  }
}


bool Heap::CanMoveObjectStart(HeapObject* object) {
  Address address = object->address();
  bool is_in_old_pointer_space = InOldPointerSpace(address);
  bool is_in_old_data_space = InOldDataSpace(address);

  if (lo_space()->Contains(object)) return false;

  Page* page = Page::FromAddress(address);
  // We can move the object start if:
  // (1) the object is not in old pointer or old data space,
  // (2) the page of the object was already swept,
  // (3) the page was already concurrently swept. This case is an optimization
  // for concurrent sweeping. The WasSwept predicate for concurrently swept
  // pages is set after sweeping all pages.
  return (!is_in_old_pointer_space && !is_in_old_data_space) ||
         page->WasSwept() || page->SweepingCompleted();
}


void Heap::AdjustLiveBytes(Address address, int by, InvocationMode mode) {
  if (incremental_marking()->IsMarking() &&
      Marking::IsBlack(Marking::MarkBitFrom(address))) {
    if (mode == FROM_GC) {
      MemoryChunk::IncrementLiveBytesFromGC(address, by);
    } else {
      MemoryChunk::IncrementLiveBytesFromMutator(address, by);
    }
  }
}


FixedArrayBase* Heap::LeftTrimFixedArray(FixedArrayBase* object,
                                         int elements_to_trim) {
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
  Address new_start = object->address() + bytes_to_trim;

  // Technically in new space this write might be omitted (except for
  // debug mode which iterates through the heap), but to play safer
  // we still do it.
  CreateFillerObjectAt(object->address(), bytes_to_trim);

  // Initialize header of the trimmed array. Since left trimming is only
  // performed on pages which are not concurrently swept creating a filler
  // object does not require synchronization.
  DCHECK(CanMoveObjectStart(object));
  Object** former_start = HeapObject::RawField(object, 0);
  int new_start_index = elements_to_trim * (element_size / kPointerSize);
  former_start[new_start_index] = map;
  former_start[new_start_index + 1] = Smi::FromInt(len - elements_to_trim);
  FixedArrayBase* new_object =
      FixedArrayBase::cast(HeapObject::FromAddress(new_start));

  // Maintain consistency of live bytes during incremental marking
  marking()->TransferMark(object->address(), new_start);
  AdjustLiveBytes(new_start, -bytes_to_trim, Heap::FROM_MUTATOR);

  // Notify the heap profiler of change in object layout.
  OnMoveEvent(new_object, object, new_object->Size());
  return new_object;
}


// Force instantiation of templatized method.
template
void Heap::RightTrimFixedArray<Heap::FROM_GC>(FixedArrayBase*, int);
template
void Heap::RightTrimFixedArray<Heap::FROM_MUTATOR>(FixedArrayBase*, int);


template<Heap::InvocationMode mode>
void Heap::RightTrimFixedArray(FixedArrayBase* object, int elements_to_trim) {
  const int element_size = object->IsFixedArray() ? kPointerSize : kDoubleSize;
  const int bytes_to_trim = elements_to_trim * element_size;

  // For now this trick is only applied to objects in new and paged space.
  DCHECK(object->map() != fixed_cow_array_map());

  const int len = object->length();
  DCHECK(elements_to_trim < len);

  // Calculate location of new array end.
  Address new_end = object->address() + object->Size() - bytes_to_trim;

  // Technically in new space this write might be omitted (except for
  // debug mode which iterates through the heap), but to play safer
  // we still do it.
  // We do not create a filler for objects in large object space.
  // TODO(hpayer): We should shrink the large object page if the size
  // of the object changed significantly.
  if (!lo_space()->Contains(object)) {
    CreateFillerObjectAt(new_end, bytes_to_trim);
  }

  // Initialize header of the trimmed array. We are storing the new length
  // using release store after creating a filler for the left-over space to
  // avoid races with the sweeper thread.
  object->synchronized_set_length(len - elements_to_trim);

  // Maintain consistency of live bytes during incremental marking
  AdjustLiveBytes(object->address(), -bytes_to_trim, mode);

  // Notify the heap profiler of change in object layout. The array may not be
  // moved during GC, and size has to be adjusted nevertheless.
  HeapProfiler* profiler = isolate()->heap_profiler();
  if (profiler->is_tracking_allocations()) {
    profiler->UpdateObjectSizeEvent(object->address(), object->Size());
  }
}


AllocationResult Heap::AllocateExternalArray(int length,
                                             ExternalArrayType array_type,
                                             void* external_pointer,
                                             PretenureFlag pretenure) {
  int size = ExternalArray::kAlignedSize;
  AllocationSpace space = SelectSpace(size, OLD_DATA_SPACE, pretenure);
  HeapObject* result;
  {
    AllocationResult allocation = AllocateRaw(size, space, OLD_DATA_SPACE);
    if (!allocation.To(&result)) return allocation;
  }

  result->set_map_no_write_barrier(MapForExternalArrayType(array_type));
  ExternalArray::cast(result)->set_length(length);
  ExternalArray::cast(result)->set_external_pointer(external_pointer);
  return result;
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
                                               PretenureFlag pretenure) {
  int element_size;
  ElementsKind elements_kind;
  ForFixedTypedArray(array_type, &element_size, &elements_kind);
  int size = OBJECT_POINTER_ALIGN(length * element_size +
                                  FixedTypedArrayBase::kDataOffset);
#ifndef V8_HOST_ARCH_64_BIT
  if (array_type == kExternalFloat64Array) {
    size += kPointerSize;
  }
#endif
  AllocationSpace space = SelectSpace(size, OLD_DATA_SPACE, pretenure);

  HeapObject* object;
  AllocationResult allocation = AllocateRaw(size, space, OLD_DATA_SPACE);
  if (!allocation.To(&object)) return allocation;

  if (array_type == kExternalFloat64Array) {
    object = EnsureDoubleAligned(this, object, size);
  }

  object->set_map(MapForFixedTypedArray(array_type));
  FixedTypedArrayBase* elements = FixedTypedArrayBase::cast(object);
  elements->set_length(length);
  memset(elements->DataPtr(), 0, elements->DataSize());
  return elements;
}


AllocationResult Heap::AllocateCode(int object_size, bool immovable) {
  DCHECK(IsAligned(static_cast<intptr_t>(object_size), kCodeAlignment));
  AllocationResult allocation =
      AllocateRaw(object_size, CODE_SPACE, CODE_SPACE);

  HeapObject* result;
  if (!allocation.To(&result)) return allocation;

  if (immovable) {
    Address address = result->address();
    // Code objects which should stay at a fixed address are allocated either
    // in the first page of code space (objects on the first page of each space
    // are never moved) or in large object space.
    if (!code_space_->FirstPage()->Contains(address) &&
        MemoryChunk::FromAddress(address)->owner()->identity() != LO_SPACE) {
      // Discard the first code allocation, which was on a page where it could
      // be moved.
      CreateFillerObjectAt(result->address(), object_size);
      allocation = lo_space_->AllocateRaw(object_size, EXECUTABLE);
      if (!allocation.To(&result)) return allocation;
      OnAllocationEvent(result, object_size);
    }
  }

  result->set_map_no_write_barrier(code_map());
  Code* code = Code::cast(result);
  DCHECK(isolate_->code_range() == NULL || !isolate_->code_range()->valid() ||
         isolate_->code_range()->contains(code->address()));
  code->set_gc_metadata(Smi::FromInt(0));
  code->set_ic_age(global_ic_age_);
  return code;
}


AllocationResult Heap::CopyCode(Code* code) {
  AllocationResult allocation;
  HeapObject* new_constant_pool;
  if (FLAG_enable_ool_constant_pool &&
      code->constant_pool() != empty_constant_pool_array()) {
    // Copy the constant pool, since edits to the copied code may modify
    // the constant pool.
    allocation = CopyConstantPoolArray(code->constant_pool());
    if (!allocation.To(&new_constant_pool)) return allocation;
  } else {
    new_constant_pool = empty_constant_pool_array();
  }

  HeapObject* result;
  // Allocate an object the same size as the code object.
  int obj_size = code->Size();
  allocation = AllocateRaw(obj_size, CODE_SPACE, CODE_SPACE);
  if (!allocation.To(&result)) return allocation;

  // Copy code object.
  Address old_addr = code->address();
  Address new_addr = result->address();
  CopyBlock(new_addr, old_addr, obj_size);
  Code* new_code = Code::cast(result);

  // Update the constant pool.
  new_code->set_constant_pool(new_constant_pool);

  // Relocate the copy.
  DCHECK(isolate_->code_range() == NULL || !isolate_->code_range()->valid() ||
         isolate_->code_range()->contains(code->address()));
  new_code->Relocate(new_addr - old_addr);
  return new_code;
}


AllocationResult Heap::CopyCode(Code* code, Vector<byte> reloc_info) {
  // Allocate ByteArray and ConstantPoolArray before the Code object, so that we
  // do not risk leaving uninitialized Code object (and breaking the heap).
  ByteArray* reloc_info_array;
  {
    AllocationResult allocation =
        AllocateByteArray(reloc_info.length(), TENURED);
    if (!allocation.To(&reloc_info_array)) return allocation;
  }
  HeapObject* new_constant_pool;
  if (FLAG_enable_ool_constant_pool &&
      code->constant_pool() != empty_constant_pool_array()) {
    // Copy the constant pool, since edits to the copied code may modify
    // the constant pool.
    AllocationResult allocation = CopyConstantPoolArray(code->constant_pool());
    if (!allocation.To(&new_constant_pool)) return allocation;
  } else {
    new_constant_pool = empty_constant_pool_array();
  }

  int new_body_size = RoundUp(code->instruction_size(), kObjectAlignment);

  int new_obj_size = Code::SizeFor(new_body_size);

  Address old_addr = code->address();

  size_t relocation_offset =
      static_cast<size_t>(code->instruction_end() - old_addr);

  HeapObject* result;
  AllocationResult allocation =
      AllocateRaw(new_obj_size, CODE_SPACE, CODE_SPACE);
  if (!allocation.To(&result)) return allocation;

  // Copy code object.
  Address new_addr = result->address();

  // Copy header and instructions.
  CopyBytes(new_addr, old_addr, relocation_offset);

  Code* new_code = Code::cast(result);
  new_code->set_relocation_info(reloc_info_array);

  // Update constant pool.
  new_code->set_constant_pool(new_constant_pool);

  // Copy patched rinfo.
  CopyBytes(new_code->relocation_start(), reloc_info.start(),
            static_cast<size_t>(reloc_info.length()));

  // Relocate the copy.
  DCHECK(isolate_->code_range() == NULL || !isolate_->code_range()->valid() ||
         isolate_->code_range()->contains(code->address()));
  new_code->Relocate(new_addr - old_addr);

#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) code->ObjectVerify();
#endif
  return new_code;
}


void Heap::InitializeAllocationMemento(AllocationMemento* memento,
                                       AllocationSite* allocation_site) {
  memento->set_map_no_write_barrier(allocation_memento_map());
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
  // If allocation failures are disallowed, we may allocate in a different
  // space when new space is full and the object is not a large object.
  AllocationSpace retry_space =
      (space != NEW_SPACE) ? space : TargetSpaceId(map->instance_type());
  int size = map->instance_size();
  if (allocation_site != NULL) {
    size += AllocationMemento::kSize;
  }
  HeapObject* result;
  AllocationResult allocation = AllocateRaw(size, space, retry_space);
  if (!allocation.To(&result)) return allocation;
  // No need for write barrier since object is white and map is in old space.
  result->set_map_no_write_barrier(map);
  if (allocation_site != NULL) {
    AllocationMemento* alloc_memento = reinterpret_cast<AllocationMemento*>(
        reinterpret_cast<Address>(result) + map->instance_size());
    InitializeAllocationMemento(alloc_memento, allocation_site);
  }
  return result;
}


void Heap::InitializeJSObjectFromMap(JSObject* obj, FixedArray* properties,
                                     Map* map) {
  obj->set_properties(properties);
  obj->initialize_elements();
  // TODO(1240798): Initialize the object's body using valid initial values
  // according to the object's initial map.  For example, if the map's
  // instance type is JS_ARRAY_TYPE, the length field should be initialized
  // to a number (e.g. Smi::FromInt(0)) and the elements initialized to a
  // fixed array (e.g. Heap::empty_fixed_array()).  Currently, the object
  // verification code has to cope with (temporarily) invalid objects.  See
  // for example, JSArray::JSArrayVerify).
  Object* filler;
  // We cannot always fill with one_pointer_filler_map because objects
  // created from API functions expect their internal fields to be initialized
  // with undefined_value.
  // Pre-allocated fields need to be initialized with undefined_value as well
  // so that object accesses before the constructor completes (e.g. in the
  // debugger) will not cause a crash.
  if (map->constructor()->IsJSFunction() &&
      JSFunction::cast(map->constructor())
          ->IsInobjectSlackTrackingInProgress()) {
    // We might want to shrink the object later.
    DCHECK(obj->GetInternalFieldCount() == 0);
    filler = Heap::one_pointer_filler_map();
  } else {
    filler = Heap::undefined_value();
  }
  obj->InitializeBody(map, Heap::undefined_value(), filler);
}


AllocationResult Heap::AllocateJSObjectFromMap(
    Map* map, PretenureFlag pretenure, bool allocate_properties,
    AllocationSite* allocation_site) {
  // JSFunctions should be allocated using AllocateFunction to be
  // properly initialized.
  DCHECK(map->instance_type() != JS_FUNCTION_TYPE);

  // Both types of global objects should be allocated using
  // AllocateGlobalObject to be properly initialized.
  DCHECK(map->instance_type() != JS_GLOBAL_OBJECT_TYPE);
  DCHECK(map->instance_type() != JS_BUILTINS_OBJECT_TYPE);

  // Allocate the backing storage for the properties.
  FixedArray* properties;
  if (allocate_properties) {
    int prop_size = map->InitialPropertiesLength();
    DCHECK(prop_size >= 0);
    {
      AllocationResult allocation = AllocateFixedArray(prop_size, pretenure);
      if (!allocation.To(&properties)) return allocation;
    }
  } else {
    properties = empty_fixed_array();
  }

  // Allocate the JSObject.
  int size = map->instance_size();
  AllocationSpace space = SelectSpace(size, OLD_POINTER_SPACE, pretenure);
  JSObject* js_obj;
  AllocationResult allocation = Allocate(map, space, allocation_site);
  if (!allocation.To(&js_obj)) return allocation;

  // Initialize the JSObject.
  InitializeJSObjectFromMap(js_obj, properties, map);
  DCHECK(js_obj->HasFastElements() || js_obj->HasExternalArrayElements() ||
         js_obj->HasFixedTypedArrayElements());
  return js_obj;
}


AllocationResult Heap::AllocateJSObject(JSFunction* constructor,
                                        PretenureFlag pretenure,
                                        AllocationSite* allocation_site) {
  DCHECK(constructor->has_initial_map());

  // Allocate the object based on the constructors initial map.
  AllocationResult allocation = AllocateJSObjectFromMap(
      constructor->initial_map(), pretenure, true, allocation_site);
#ifdef DEBUG
  // Make sure result is NOT a global object if valid.
  HeapObject* obj;
  DCHECK(!allocation.To(&obj) || !obj->IsGlobalObject());
#endif
  return allocation;
}


AllocationResult Heap::CopyJSObject(JSObject* source, AllocationSite* site) {
  // Never used to copy functions.  If functions need to be copied we
  // have to be careful to clear the literals array.
  SLOW_DCHECK(!source->IsJSFunction());

  // Make the clone.
  Map* map = source->map();
  int object_size = map->instance_size();
  HeapObject* clone;

  DCHECK(site == NULL || AllocationSite::CanTrack(map->instance_type()));

  WriteBarrierMode wb_mode = UPDATE_WRITE_BARRIER;

  // If we're forced to always allocate, we use the general allocation
  // functions which may leave us with an object in old space.
  if (always_allocate()) {
    {
      AllocationResult allocation =
          AllocateRaw(object_size, NEW_SPACE, OLD_POINTER_SPACE);
      if (!allocation.To(&clone)) return allocation;
    }
    Address clone_address = clone->address();
    CopyBlock(clone_address, source->address(), object_size);
    // Update write barrier for all fields that lie beyond the header.
    RecordWrites(clone_address, JSObject::kHeaderSize,
                 (object_size - JSObject::kHeaderSize) / kPointerSize);
  } else {
    wb_mode = SKIP_WRITE_BARRIER;

    {
      int adjusted_object_size =
          site != NULL ? object_size + AllocationMemento::kSize : object_size;
      AllocationResult allocation =
          AllocateRaw(adjusted_object_size, NEW_SPACE, NEW_SPACE);
      if (!allocation.To(&clone)) return allocation;
    }
    SLOW_DCHECK(InNewSpace(clone));
    // Since we know the clone is allocated in new space, we can copy
    // the contents without worrying about updating the write barrier.
    CopyBlock(clone->address(), source->address(), object_size);

    if (site != NULL) {
      AllocationMemento* alloc_memento = reinterpret_cast<AllocationMemento*>(
          reinterpret_cast<Address>(clone) + object_size);
      InitializeAllocationMemento(alloc_memento, site);
    }
  }

  SLOW_DCHECK(JSObject::cast(clone)->GetElementsKind() ==
              source->GetElementsKind());
  FixedArrayBase* elements = FixedArrayBase::cast(source->elements());
  FixedArray* properties = FixedArray::cast(source->properties());
  // Update elements if necessary.
  if (elements->length() > 0) {
    FixedArrayBase* elem;
    {
      AllocationResult allocation;
      if (elements->map() == fixed_cow_array_map()) {
        allocation = FixedArray::cast(elements);
      } else if (source->HasFastDoubleElements()) {
        allocation = CopyFixedDoubleArray(FixedDoubleArray::cast(elements));
      } else {
        allocation = CopyFixedArray(FixedArray::cast(elements));
      }
      if (!allocation.To(&elem)) return allocation;
    }
    JSObject::cast(clone)->set_elements(elem, wb_mode);
  }
  // Update properties if necessary.
  if (properties->length() > 0) {
    FixedArray* prop;
    {
      AllocationResult allocation = CopyFixedArray(properties);
      if (!allocation.To(&prop)) return allocation;
    }
    JSObject::cast(clone)->set_properties(prop, wb_mode);
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
  unsigned stream_length = vector.length();
  while (stream_length != 0) {
    unsigned consumed = 0;
    uint32_t c = unibrow::Utf8::ValueOf(stream, stream_length, &consumed);
    DCHECK(c != unibrow::Utf8::kBadChar);
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
  DCHECK(stream_length == 0);
  DCHECK(len == 0);
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
  DCHECK(chars >= 0);
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
  AllocationSpace space = SelectSpace(size, OLD_DATA_SPACE, TENURED);

  // Allocate string.
  HeapObject* result;
  {
    AllocationResult allocation = AllocateRaw(size, space, OLD_DATA_SPACE);
    if (!allocation.To(&result)) return allocation;
  }

  result->set_map_no_write_barrier(map);
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
  DCHECK(size <= SeqOneByteString::kMaxSize);
  AllocationSpace space = SelectSpace(size, OLD_DATA_SPACE, pretenure);

  HeapObject* result;
  {
    AllocationResult allocation = AllocateRaw(size, space, OLD_DATA_SPACE);
    if (!allocation.To(&result)) return allocation;
  }

  // Partially initialize the object.
  result->set_map_no_write_barrier(one_byte_string_map());
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
  DCHECK(size <= SeqTwoByteString::kMaxSize);
  AllocationSpace space = SelectSpace(size, OLD_DATA_SPACE, pretenure);

  HeapObject* result;
  {
    AllocationResult allocation = AllocateRaw(size, space, OLD_DATA_SPACE);
    if (!allocation.To(&result)) return allocation;
  }

  // Partially initialize the object.
  result->set_map_no_write_barrier(string_map());
  String::cast(result)->set_length(length);
  String::cast(result)->set_hash_field(String::kEmptyHashField);
  DCHECK_EQ(size, HeapObject::cast(result)->Size());
  return result;
}


AllocationResult Heap::AllocateEmptyFixedArray() {
  int size = FixedArray::SizeFor(0);
  HeapObject* result;
  {
    AllocationResult allocation =
        AllocateRaw(size, OLD_DATA_SPACE, OLD_DATA_SPACE);
    if (!allocation.To(&result)) return allocation;
  }
  // Initialize the object.
  result->set_map_no_write_barrier(fixed_array_map());
  FixedArray::cast(result)->set_length(0);
  return result;
}


AllocationResult Heap::AllocateEmptyExternalArray(
    ExternalArrayType array_type) {
  return AllocateExternalArray(0, array_type, NULL, TENURED);
}


AllocationResult Heap::CopyAndTenureFixedCOWArray(FixedArray* src) {
  if (!InNewSpace(src)) {
    return src;
  }

  int len = src->length();
  HeapObject* obj;
  {
    AllocationResult allocation = AllocateRawFixedArray(len, TENURED);
    if (!allocation.To(&obj)) return allocation;
  }
  obj->set_map_no_write_barrier(fixed_array_map());
  FixedArray* result = FixedArray::cast(obj);
  result->set_length(len);

  // Copy the content
  DisallowHeapAllocation no_gc;
  WriteBarrierMode mode = result->GetWriteBarrierMode(no_gc);
  for (int i = 0; i < len; i++) result->set(i, src->get(i), mode);

  // TODO(mvstanton): The map is set twice because of protection against calling
  // set() on a COW FixedArray. Issue v8:3221 created to track this, and
  // we might then be able to remove this whole method.
  HeapObject::cast(obj)->set_map_no_write_barrier(fixed_cow_array_map());
  return result;
}


AllocationResult Heap::AllocateEmptyFixedTypedArray(
    ExternalArrayType array_type) {
  return AllocateFixedTypedArray(0, array_type, TENURED);
}


AllocationResult Heap::CopyFixedArrayWithMap(FixedArray* src, Map* map) {
  int len = src->length();
  HeapObject* obj;
  {
    AllocationResult allocation = AllocateRawFixedArray(len, NOT_TENURED);
    if (!allocation.To(&obj)) return allocation;
  }
  if (InNewSpace(obj)) {
    obj->set_map_no_write_barrier(map);
    CopyBlock(obj->address() + kPointerSize, src->address() + kPointerSize,
              FixedArray::SizeFor(len) - kPointerSize);
    return obj;
  }
  obj->set_map_no_write_barrier(map);
  FixedArray* result = FixedArray::cast(obj);
  result->set_length(len);

  // Copy the content
  DisallowHeapAllocation no_gc;
  WriteBarrierMode mode = result->GetWriteBarrierMode(no_gc);
  for (int i = 0; i < len; i++) result->set(i, src->get(i), mode);
  return result;
}


AllocationResult Heap::CopyFixedDoubleArrayWithMap(FixedDoubleArray* src,
                                                   Map* map) {
  int len = src->length();
  HeapObject* obj;
  {
    AllocationResult allocation = AllocateRawFixedDoubleArray(len, NOT_TENURED);
    if (!allocation.To(&obj)) return allocation;
  }
  obj->set_map_no_write_barrier(map);
  CopyBlock(obj->address() + FixedDoubleArray::kLengthOffset,
            src->address() + FixedDoubleArray::kLengthOffset,
            FixedDoubleArray::SizeFor(len) - FixedDoubleArray::kLengthOffset);
  return obj;
}


AllocationResult Heap::CopyConstantPoolArrayWithMap(ConstantPoolArray* src,
                                                    Map* map) {
  HeapObject* obj;
  if (src->is_extended_layout()) {
    ConstantPoolArray::NumberOfEntries small(src,
                                             ConstantPoolArray::SMALL_SECTION);
    ConstantPoolArray::NumberOfEntries extended(
        src, ConstantPoolArray::EXTENDED_SECTION);
    AllocationResult allocation =
        AllocateExtendedConstantPoolArray(small, extended);
    if (!allocation.To(&obj)) return allocation;
  } else {
    ConstantPoolArray::NumberOfEntries small(src,
                                             ConstantPoolArray::SMALL_SECTION);
    AllocationResult allocation = AllocateConstantPoolArray(small);
    if (!allocation.To(&obj)) return allocation;
  }
  obj->set_map_no_write_barrier(map);
  CopyBlock(obj->address() + ConstantPoolArray::kFirstEntryOffset,
            src->address() + ConstantPoolArray::kFirstEntryOffset,
            src->size() - ConstantPoolArray::kFirstEntryOffset);
  return obj;
}


AllocationResult Heap::AllocateRawFixedArray(int length,
                                             PretenureFlag pretenure) {
  if (length < 0 || length > FixedArray::kMaxLength) {
    v8::internal::Heap::FatalProcessOutOfMemory("invalid array length", true);
  }
  int size = FixedArray::SizeFor(length);
  AllocationSpace space = SelectSpace(size, OLD_POINTER_SPACE, pretenure);

  return AllocateRaw(size, space, OLD_POINTER_SPACE);
}


AllocationResult Heap::AllocateFixedArrayWithFiller(int length,
                                                    PretenureFlag pretenure,
                                                    Object* filler) {
  DCHECK(length >= 0);
  DCHECK(empty_fixed_array()->IsFixedArray());
  if (length == 0) return empty_fixed_array();

  DCHECK(!InNewSpace(filler));
  HeapObject* result;
  {
    AllocationResult allocation = AllocateRawFixedArray(length, pretenure);
    if (!allocation.To(&result)) return allocation;
  }

  result->set_map_no_write_barrier(fixed_array_map());
  FixedArray* array = FixedArray::cast(result);
  array->set_length(length);
  MemsetPointer(array->data_start(), filler, length);
  return array;
}


AllocationResult Heap::AllocateFixedArray(int length, PretenureFlag pretenure) {
  return AllocateFixedArrayWithFiller(length, pretenure, undefined_value());
}


AllocationResult Heap::AllocateUninitializedFixedArray(int length) {
  if (length == 0) return empty_fixed_array();

  HeapObject* obj;
  {
    AllocationResult allocation = AllocateRawFixedArray(length, NOT_TENURED);
    if (!allocation.To(&obj)) return allocation;
  }

  obj->set_map_no_write_barrier(fixed_array_map());
  FixedArray::cast(obj)->set_length(length);
  return obj;
}


AllocationResult Heap::AllocateUninitializedFixedDoubleArray(
    int length, PretenureFlag pretenure) {
  if (length == 0) return empty_fixed_array();

  HeapObject* elements;
  AllocationResult allocation = AllocateRawFixedDoubleArray(length, pretenure);
  if (!allocation.To(&elements)) return allocation;

  elements->set_map_no_write_barrier(fixed_double_array_map());
  FixedDoubleArray::cast(elements)->set_length(length);
  return elements;
}


AllocationResult Heap::AllocateRawFixedDoubleArray(int length,
                                                   PretenureFlag pretenure) {
  if (length < 0 || length > FixedDoubleArray::kMaxLength) {
    v8::internal::Heap::FatalProcessOutOfMemory("invalid array length", true);
  }
  int size = FixedDoubleArray::SizeFor(length);
#ifndef V8_HOST_ARCH_64_BIT
  size += kPointerSize;
#endif
  AllocationSpace space = SelectSpace(size, OLD_DATA_SPACE, pretenure);

  HeapObject* object;
  {
    AllocationResult allocation = AllocateRaw(size, space, OLD_DATA_SPACE);
    if (!allocation.To(&object)) return allocation;
  }

  return EnsureDoubleAligned(this, object, size);
}


AllocationResult Heap::AllocateConstantPoolArray(
    const ConstantPoolArray::NumberOfEntries& small) {
  CHECK(small.are_in_range(0, ConstantPoolArray::kMaxSmallEntriesPerType));
  int size = ConstantPoolArray::SizeFor(small);
#ifndef V8_HOST_ARCH_64_BIT
  size += kPointerSize;
#endif
  AllocationSpace space = SelectSpace(size, OLD_POINTER_SPACE, TENURED);

  HeapObject* object;
  {
    AllocationResult allocation = AllocateRaw(size, space, OLD_POINTER_SPACE);
    if (!allocation.To(&object)) return allocation;
  }
  object = EnsureDoubleAligned(this, object, size);
  object->set_map_no_write_barrier(constant_pool_array_map());

  ConstantPoolArray* constant_pool = ConstantPoolArray::cast(object);
  constant_pool->Init(small);
  constant_pool->ClearPtrEntries(isolate());
  return constant_pool;
}


AllocationResult Heap::AllocateExtendedConstantPoolArray(
    const ConstantPoolArray::NumberOfEntries& small,
    const ConstantPoolArray::NumberOfEntries& extended) {
  CHECK(small.are_in_range(0, ConstantPoolArray::kMaxSmallEntriesPerType));
  CHECK(extended.are_in_range(0, kMaxInt));
  int size = ConstantPoolArray::SizeForExtended(small, extended);
#ifndef V8_HOST_ARCH_64_BIT
  size += kPointerSize;
#endif
  AllocationSpace space = SelectSpace(size, OLD_POINTER_SPACE, TENURED);

  HeapObject* object;
  {
    AllocationResult allocation = AllocateRaw(size, space, OLD_POINTER_SPACE);
    if (!allocation.To(&object)) return allocation;
  }
  object = EnsureDoubleAligned(this, object, size);
  object->set_map_no_write_barrier(constant_pool_array_map());

  ConstantPoolArray* constant_pool = ConstantPoolArray::cast(object);
  constant_pool->InitExtended(small, extended);
  constant_pool->ClearPtrEntries(isolate());
  return constant_pool;
}


AllocationResult Heap::AllocateEmptyConstantPoolArray() {
  ConstantPoolArray::NumberOfEntries small(0, 0, 0, 0);
  int size = ConstantPoolArray::SizeFor(small);
  HeapObject* result;
  {
    AllocationResult allocation =
        AllocateRaw(size, OLD_DATA_SPACE, OLD_DATA_SPACE);
    if (!allocation.To(&result)) return allocation;
  }
  result->set_map_no_write_barrier(constant_pool_array_map());
  ConstantPoolArray::cast(result)->Init(small);
  return result;
}


AllocationResult Heap::AllocateSymbol() {
  // Statically ensure that it is safe to allocate symbols in paged spaces.
  STATIC_ASSERT(Symbol::kSize <= Page::kMaxRegularHeapObjectSize);

  HeapObject* result;
  AllocationResult allocation =
      AllocateRaw(Symbol::kSize, OLD_POINTER_SPACE, OLD_POINTER_SPACE);
  if (!allocation.To(&result)) return allocation;

  result->set_map_no_write_barrier(symbol_map());

  // Generate a random hash value.
  int hash;
  int attempts = 0;
  do {
    hash = isolate()->random_number_generator()->NextInt() & Name::kHashBitMask;
    attempts++;
  } while (hash == 0 && attempts < 30);
  if (hash == 0) hash = 1;  // never return 0

  Symbol::cast(result)
      ->set_hash_field(Name::kIsNotArrayIndexMask | (hash << Name::kHashShift));
  Symbol::cast(result)->set_name(undefined_value());
  Symbol::cast(result)->set_flags(Smi::FromInt(0));

  DCHECK(!Symbol::cast(result)->is_private());
  return result;
}


AllocationResult Heap::AllocateStruct(InstanceType type) {
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
      return exception();
  }
  int size = map->instance_size();
  AllocationSpace space = SelectSpace(size, OLD_POINTER_SPACE, TENURED);
  Struct* result;
  {
    AllocationResult allocation = Allocate(map, space);
    if (!allocation.To(&result)) return allocation;
  }
  result->InitializeBody(size);
  return result;
}


bool Heap::IsHeapIterable() {
  // TODO(hpayer): This function is not correct. Allocation folding in old
  // space breaks the iterability.
  return new_space_top_after_last_gc_ == new_space()->top();
}


void Heap::MakeHeapIterable() {
  DCHECK(AllowHeapAllocation::IsAllowed());
  if (!IsHeapIterable()) {
    CollectAllGarbage(kMakeHeapIterableMask, "Heap::MakeHeapIterable");
  }
  if (mark_compact_collector()->sweeping_in_progress()) {
    mark_compact_collector()->EnsureSweepingCompleted();
  }
  DCHECK(IsHeapIterable());
}


void Heap::AdvanceIdleIncrementalMarking(intptr_t step_size) {
  incremental_marking()->Step(step_size,
                              IncrementalMarking::NO_GC_VIA_STACK_GUARD, true);

  if (incremental_marking()->IsComplete()) {
    bool uncommit = false;
    if (gc_count_at_last_idle_gc_ == gc_count_) {
      // No GC since the last full GC, the mutator is probably not active.
      isolate_->compilation_cache()->Clear();
      uncommit = true;
    }
    CollectAllGarbage(kReduceMemoryFootprintMask,
                      "idle notification: finalize incremental");
    gc_idle_time_handler_.NotifyIdleMarkCompact();
    gc_count_at_last_idle_gc_ = gc_count_;
    if (uncommit) {
      new_space_.Shrink();
      UncommitFromSpace();
    }
  }
}


bool Heap::WorthActivatingIncrementalMarking() {
  return incremental_marking()->IsStopped() &&
         incremental_marking()->WorthActivating() && NextGCIsLikelyToBeFull();
}


bool Heap::IdleNotification(int idle_time_in_ms) {
  // If incremental marking is off, we do not perform idle notification.
  if (!FLAG_incremental_marking) return true;
  base::ElapsedTimer timer;
  timer.Start();
  isolate()->counters()->gc_idle_time_allotted_in_ms()->AddSample(
      idle_time_in_ms);
  HistogramTimerScope idle_notification_scope(
      isolate_->counters()->gc_idle_notification());

  GCIdleTimeHandler::HeapState heap_state;
  heap_state.contexts_disposed = contexts_disposed_;
  heap_state.size_of_objects = static_cast<size_t>(SizeOfObjects());
  heap_state.incremental_marking_stopped = incremental_marking()->IsStopped();
  // TODO(ulan): Start incremental marking only for large heaps.
  heap_state.can_start_incremental_marking =
      incremental_marking()->ShouldActivate();
  heap_state.sweeping_in_progress =
      mark_compact_collector()->sweeping_in_progress();
  heap_state.mark_compact_speed_in_bytes_per_ms =
      static_cast<size_t>(tracer()->MarkCompactSpeedInBytesPerMillisecond());
  heap_state.incremental_marking_speed_in_bytes_per_ms = static_cast<size_t>(
      tracer()->IncrementalMarkingSpeedInBytesPerMillisecond());
  heap_state.scavenge_speed_in_bytes_per_ms =
      static_cast<size_t>(tracer()->ScavengeSpeedInBytesPerMillisecond());
  heap_state.available_new_space_memory = new_space_.Available();
  heap_state.new_space_capacity = new_space_.Capacity();
  heap_state.new_space_allocation_throughput_in_bytes_per_ms =
      static_cast<size_t>(
          tracer()->NewSpaceAllocationThroughputInBytesPerMillisecond());

  GCIdleTimeAction action =
      gc_idle_time_handler_.Compute(idle_time_in_ms, heap_state);

  bool result = false;
  switch (action.type) {
    case DONE:
      result = true;
      break;
    case DO_INCREMENTAL_MARKING:
      if (incremental_marking()->IsStopped()) {
        incremental_marking()->Start();
      }
      AdvanceIdleIncrementalMarking(action.parameter);
      break;
    case DO_FULL_GC: {
      HistogramTimerScope scope(isolate_->counters()->gc_context());
      const char* message = contexts_disposed_
                                ? "idle notification: contexts disposed"
                                : "idle notification: finalize idle round";
      CollectAllGarbage(kReduceMemoryFootprintMask, message);
      gc_idle_time_handler_.NotifyIdleMarkCompact();
      break;
    }
    case DO_SCAVENGE:
      CollectGarbage(NEW_SPACE, "idle notification: scavenge");
      break;
    case DO_FINALIZE_SWEEPING:
      mark_compact_collector()->EnsureSweepingCompleted();
      break;
    case DO_NOTHING:
      break;
  }

  int actual_time_ms = static_cast<int>(timer.Elapsed().InMilliseconds());
  if (actual_time_ms <= idle_time_in_ms) {
    if (action.type != DONE && action.type != DO_NOTHING) {
      isolate()->counters()->gc_idle_time_limit_undershot()->AddSample(
          idle_time_in_ms - actual_time_ms);
    }
  } else {
    isolate()->counters()->gc_idle_time_limit_overshot()->AddSample(
        actual_time_ms - idle_time_in_ms);
  }

  if (FLAG_trace_idle_notification) {
    PrintF("Idle notification: requested idle time %d ms, actual time %d ms [",
           idle_time_in_ms, actual_time_ms);
    action.Print();
    PrintF("]\n");
  }

  contexts_disposed_ = 0;
  return result;
}


#ifdef DEBUG

void Heap::Print() {
  if (!HasBeenSetUp()) return;
  isolate()->PrintStack(stdout);
  AllSpaces spaces(this);
  for (Space* space = spaces.next(); space != NULL; space = spaces.next()) {
    space->Print();
  }
}


void Heap::ReportCodeStatistics(const char* title) {
  PrintF(">>>>>> Code Stats (%s) >>>>>>\n", title);
  PagedSpace::ResetCodeStatistics(isolate());
  // We do not look for code in new space, map space, or old space.  If code
  // somehow ends up in those spaces, we would miss it here.
  code_space_->CollectCodeStatistics();
  lo_space_->CollectCodeStatistics();
  PagedSpace::ReportCodeStatistics(isolate());
}


// This function expects that NewSpace's allocated objects histogram is
// populated (via a call to CollectStatistics or else as a side effect of a
// just-completed scavenge collection).
void Heap::ReportHeapStatistics(const char* title) {
  USE(title);
  PrintF(">>>>>> =============== %s (%d) =============== >>>>>>\n", title,
         gc_count_);
  PrintF("old_generation_allocation_limit_ %" V8_PTR_PREFIX "d\n",
         old_generation_allocation_limit_);

  PrintF("\n");
  PrintF("Number of handles : %d\n", HandleScope::NumberOfHandles(isolate_));
  isolate_->global_handles()->PrintStats();
  PrintF("\n");

  PrintF("Heap statistics : ");
  isolate_->memory_allocator()->ReportStatistics();
  PrintF("To space : ");
  new_space_.ReportStatistics();
  PrintF("Old pointer space : ");
  old_pointer_space_->ReportStatistics();
  PrintF("Old data space : ");
  old_data_space_->ReportStatistics();
  PrintF("Code space : ");
  code_space_->ReportStatistics();
  PrintF("Map space : ");
  map_space_->ReportStatistics();
  PrintF("Cell space : ");
  cell_space_->ReportStatistics();
  PrintF("PropertyCell space : ");
  property_cell_space_->ReportStatistics();
  PrintF("Large object space : ");
  lo_space_->ReportStatistics();
  PrintF(">>>>>> ========================================= >>>>>>\n");
}

#endif  // DEBUG

bool Heap::Contains(HeapObject* value) { return Contains(value->address()); }


bool Heap::Contains(Address addr) {
  if (isolate_->memory_allocator()->IsOutsideAllocatedSpace(addr)) return false;
  return HasBeenSetUp() &&
         (new_space_.ToSpaceContains(addr) ||
          old_pointer_space_->Contains(addr) ||
          old_data_space_->Contains(addr) || code_space_->Contains(addr) ||
          map_space_->Contains(addr) || cell_space_->Contains(addr) ||
          property_cell_space_->Contains(addr) ||
          lo_space_->SlowContains(addr));
}


bool Heap::InSpace(HeapObject* value, AllocationSpace space) {
  return InSpace(value->address(), space);
}


bool Heap::InSpace(Address addr, AllocationSpace space) {
  if (isolate_->memory_allocator()->IsOutsideAllocatedSpace(addr)) return false;
  if (!HasBeenSetUp()) return false;

  switch (space) {
    case NEW_SPACE:
      return new_space_.ToSpaceContains(addr);
    case OLD_POINTER_SPACE:
      return old_pointer_space_->Contains(addr);
    case OLD_DATA_SPACE:
      return old_data_space_->Contains(addr);
    case CODE_SPACE:
      return code_space_->Contains(addr);
    case MAP_SPACE:
      return map_space_->Contains(addr);
    case CELL_SPACE:
      return cell_space_->Contains(addr);
    case PROPERTY_CELL_SPACE:
      return property_cell_space_->Contains(addr);
    case LO_SPACE:
      return lo_space_->SlowContains(addr);
    case INVALID_SPACE:
      break;
  }
  UNREACHABLE();
  return false;
}


#ifdef VERIFY_HEAP
void Heap::Verify() {
  CHECK(HasBeenSetUp());
  HandleScope scope(isolate());

  store_buffer()->Verify();

  if (mark_compact_collector()->sweeping_in_progress()) {
    // We have to wait here for the sweeper threads to have an iterable heap.
    mark_compact_collector()->EnsureSweepingCompleted();
  }

  VerifyPointersVisitor visitor;
  IterateRoots(&visitor, VISIT_ONLY_STRONG);

  VerifySmisVisitor smis_visitor;
  IterateSmiRoots(&smis_visitor);

  new_space_.Verify();

  old_pointer_space_->Verify(&visitor);
  map_space_->Verify(&visitor);

  VerifyPointersVisitor no_dirty_regions_visitor;
  old_data_space_->Verify(&no_dirty_regions_visitor);
  code_space_->Verify(&no_dirty_regions_visitor);
  cell_space_->Verify(&no_dirty_regions_visitor);
  property_cell_space_->Verify(&no_dirty_regions_visitor);

  lo_space_->Verify();
}
#endif


void Heap::ZapFromSpace() {
  NewSpacePageIterator it(new_space_.FromSpaceStart(),
                          new_space_.FromSpaceEnd());
  while (it.has_next()) {
    NewSpacePage* page = it.next();
    for (Address cursor = page->area_start(), limit = page->area_end();
         cursor < limit; cursor += kPointerSize) {
      Memory::Address_at(cursor) = kFromSpaceZapValue;
    }
  }
}


void Heap::IterateAndMarkPointersToFromSpace(Address start, Address end,
                                             ObjectSlotCallback callback) {
  Address slot_address = start;

  // We are not collecting slots on new space objects during mutation
  // thus we have to scan for pointers to evacuation candidates when we
  // promote objects. But we should not record any slots in non-black
  // objects. Grey object's slots would be rescanned.
  // White object might not survive until the end of collection
  // it would be a violation of the invariant to record it's slots.
  bool record_slots = false;
  if (incremental_marking()->IsCompacting()) {
    MarkBit mark_bit = Marking::MarkBitFrom(HeapObject::FromAddress(start));
    record_slots = Marking::IsBlack(mark_bit);
  }

  while (slot_address < end) {
    Object** slot = reinterpret_cast<Object**>(slot_address);
    Object* object = *slot;
    // If the store buffer becomes overfull we mark pages as being exempt from
    // the store buffer.  These pages are scanned to find pointers that point
    // to the new space.  In that case we may hit newly promoted objects and
    // fix the pointers before the promotion queue gets to them.  Thus the 'if'.
    if (object->IsHeapObject()) {
      if (Heap::InFromSpace(object)) {
        callback(reinterpret_cast<HeapObject**>(slot),
                 HeapObject::cast(object));
        Object* new_object = *slot;
        if (InNewSpace(new_object)) {
          SLOW_DCHECK(Heap::InToSpace(new_object));
          SLOW_DCHECK(new_object->IsHeapObject());
          store_buffer_.EnterDirectlyIntoStoreBuffer(
              reinterpret_cast<Address>(slot));
        }
        SLOW_DCHECK(!MarkCompactCollector::IsOnEvacuationCandidate(new_object));
      } else if (record_slots &&
                 MarkCompactCollector::IsOnEvacuationCandidate(object)) {
        mark_compact_collector()->RecordSlot(slot, slot, object);
      }
    }
    slot_address += kPointerSize;
  }
}


#ifdef DEBUG
typedef bool (*CheckStoreBufferFilter)(Object** addr);


bool IsAMapPointerAddress(Object** addr) {
  uintptr_t a = reinterpret_cast<uintptr_t>(addr);
  int mod = a % Map::kSize;
  return mod >= Map::kPointerFieldsBeginOffset &&
         mod < Map::kPointerFieldsEndOffset;
}


bool EverythingsAPointer(Object** addr) { return true; }


static void CheckStoreBuffer(Heap* heap, Object** current, Object** limit,
                             Object**** store_buffer_position,
                             Object*** store_buffer_top,
                             CheckStoreBufferFilter filter,
                             Address special_garbage_start,
                             Address special_garbage_end) {
  Map* free_space_map = heap->free_space_map();
  for (; current < limit; current++) {
    Object* o = *current;
    Address current_address = reinterpret_cast<Address>(current);
    // Skip free space.
    if (o == free_space_map) {
      Address current_address = reinterpret_cast<Address>(current);
      FreeSpace* free_space =
          FreeSpace::cast(HeapObject::FromAddress(current_address));
      int skip = free_space->Size();
      DCHECK(current_address + skip <= reinterpret_cast<Address>(limit));
      DCHECK(skip > 0);
      current_address += skip - kPointerSize;
      current = reinterpret_cast<Object**>(current_address);
      continue;
    }
    // Skip the current linear allocation space between top and limit which is
    // unmarked with the free space map, but can contain junk.
    if (current_address == special_garbage_start &&
        special_garbage_end != special_garbage_start) {
      current_address = special_garbage_end - kPointerSize;
      current = reinterpret_cast<Object**>(current_address);
      continue;
    }
    if (!(*filter)(current)) continue;
    DCHECK(current_address < special_garbage_start ||
           current_address >= special_garbage_end);
    DCHECK(reinterpret_cast<uintptr_t>(o) != kFreeListZapValue);
    // We have to check that the pointer does not point into new space
    // without trying to cast it to a heap object since the hash field of
    // a string can contain values like 1 and 3 which are tagged null
    // pointers.
    if (!heap->InNewSpace(o)) continue;
    while (**store_buffer_position < current &&
           *store_buffer_position < store_buffer_top) {
      (*store_buffer_position)++;
    }
    if (**store_buffer_position != current ||
        *store_buffer_position == store_buffer_top) {
      Object** obj_start = current;
      while (!(*obj_start)->IsMap()) obj_start--;
      UNREACHABLE();
    }
  }
}


// Check that the store buffer contains all intergenerational pointers by
// scanning a page and ensuring that all pointers to young space are in the
// store buffer.
void Heap::OldPointerSpaceCheckStoreBuffer() {
  OldSpace* space = old_pointer_space();
  PageIterator pages(space);

  store_buffer()->SortUniq();

  while (pages.has_next()) {
    Page* page = pages.next();
    Object** current = reinterpret_cast<Object**>(page->area_start());

    Address end = page->area_end();

    Object*** store_buffer_position = store_buffer()->Start();
    Object*** store_buffer_top = store_buffer()->Top();

    Object** limit = reinterpret_cast<Object**>(end);
    CheckStoreBuffer(this, current, limit, &store_buffer_position,
                     store_buffer_top, &EverythingsAPointer, space->top(),
                     space->limit());
  }
}


void Heap::MapSpaceCheckStoreBuffer() {
  MapSpace* space = map_space();
  PageIterator pages(space);

  store_buffer()->SortUniq();

  while (pages.has_next()) {
    Page* page = pages.next();
    Object** current = reinterpret_cast<Object**>(page->area_start());

    Address end = page->area_end();

    Object*** store_buffer_position = store_buffer()->Start();
    Object*** store_buffer_top = store_buffer()->Top();

    Object** limit = reinterpret_cast<Object**>(end);
    CheckStoreBuffer(this, current, limit, &store_buffer_position,
                     store_buffer_top, &IsAMapPointerAddress, space->top(),
                     space->limit());
  }
}


void Heap::LargeObjectSpaceCheckStoreBuffer() {
  LargeObjectIterator it(lo_space());
  for (HeapObject* object = it.Next(); object != NULL; object = it.Next()) {
    // We only have code, sequential strings, or fixed arrays in large
    // object space, and only fixed arrays can possibly contain pointers to
    // the young generation.
    if (object->IsFixedArray()) {
      Object*** store_buffer_position = store_buffer()->Start();
      Object*** store_buffer_top = store_buffer()->Top();
      Object** current = reinterpret_cast<Object**>(object->address());
      Object** limit =
          reinterpret_cast<Object**>(object->address() + object->Size());
      CheckStoreBuffer(this, current, limit, &store_buffer_position,
                       store_buffer_top, &EverythingsAPointer, NULL, NULL);
    }
  }
}
#endif


void Heap::IterateRoots(ObjectVisitor* v, VisitMode mode) {
  IterateStrongRoots(v, mode);
  IterateWeakRoots(v, mode);
}


void Heap::IterateWeakRoots(ObjectVisitor* v, VisitMode mode) {
  v->VisitPointer(reinterpret_cast<Object**>(&roots_[kStringTableRootIndex]));
  v->Synchronize(VisitorSynchronization::kStringTable);
  if (mode != VISIT_ALL_IN_SCAVENGE && mode != VISIT_ALL_IN_SWEEP_NEWSPACE) {
    // Scavenge collections have special processing for this.
    external_string_table_.Iterate(v);
  }
  v->Synchronize(VisitorSynchronization::kExternalStringsTable);
}


void Heap::IterateSmiRoots(ObjectVisitor* v) {
  // Acquire execution access since we are going to read stack limit values.
  ExecutionAccess access(isolate());
  v->VisitPointers(&roots_[kSmiRootsStart], &roots_[kRootListLength]);
  v->Synchronize(VisitorSynchronization::kSmiRootList);
}


void Heap::IterateStrongRoots(ObjectVisitor* v, VisitMode mode) {
  v->VisitPointers(&roots_[0], &roots_[kStrongRootListLength]);
  v->Synchronize(VisitorSynchronization::kStrongRootList);

  v->VisitPointer(bit_cast<Object**>(&hidden_string_));
  v->Synchronize(VisitorSynchronization::kInternalizedString);

  isolate_->bootstrapper()->Iterate(v);
  v->Synchronize(VisitorSynchronization::kBootstrapper);
  isolate_->Iterate(v);
  v->Synchronize(VisitorSynchronization::kTop);
  Relocatable::Iterate(isolate_, v);
  v->Synchronize(VisitorSynchronization::kRelocatable);

  if (isolate_->deoptimizer_data() != NULL) {
    isolate_->deoptimizer_data()->Iterate(v);
  }
  v->Synchronize(VisitorSynchronization::kDebug);
  isolate_->compilation_cache()->Iterate(v);
  v->Synchronize(VisitorSynchronization::kCompilationCache);

  // Iterate over local handles in handle scopes.
  isolate_->handle_scope_implementer()->Iterate(v);
  isolate_->IterateDeferredHandles(v);
  v->Synchronize(VisitorSynchronization::kHandleScope);

  // Iterate over the builtin code objects and code stubs in the
  // heap. Note that it is not necessary to iterate over code objects
  // on scavenge collections.
  if (mode != VISIT_ALL_IN_SCAVENGE) {
    isolate_->builtins()->IterateBuiltins(v);
  }
  v->Synchronize(VisitorSynchronization::kBuiltins);

  // Iterate over global handles.
  switch (mode) {
    case VISIT_ONLY_STRONG:
      isolate_->global_handles()->IterateStrongRoots(v);
      break;
    case VISIT_ALL_IN_SCAVENGE:
      isolate_->global_handles()->IterateNewSpaceStrongAndDependentRoots(v);
      break;
    case VISIT_ALL_IN_SWEEP_NEWSPACE:
    case VISIT_ALL:
      isolate_->global_handles()->IterateAllRoots(v);
      break;
  }
  v->Synchronize(VisitorSynchronization::kGlobalHandles);

  // Iterate over eternal handles.
  if (mode == VISIT_ALL_IN_SCAVENGE) {
    isolate_->eternal_handles()->IterateNewSpaceRoots(v);
  } else {
    isolate_->eternal_handles()->IterateAllRoots(v);
  }
  v->Synchronize(VisitorSynchronization::kEternalHandles);

  // Iterate over pointers being held by inactive threads.
  isolate_->thread_manager()->Iterate(v);
  v->Synchronize(VisitorSynchronization::kThreadManager);

  // Iterate over the pointers the Serialization/Deserialization code is
  // holding.
  // During garbage collection this keeps the partial snapshot cache alive.
  // During deserialization of the startup snapshot this creates the partial
  // snapshot cache and deserializes the objects it refers to.  During
  // serialization this does nothing, since the partial snapshot cache is
  // empty.  However the next thing we do is create the partial snapshot,
  // filling up the partial snapshot cache with objects it needs as we go.
  SerializerDeserializer::Iterate(isolate_, v);
  // We don't do a v->Synchronize call here, because in debug mode that will
  // output a flag to the snapshot.  However at this point the serializer and
  // deserializer are deliberately a little unsynchronized (see above) so the
  // checking of the sync flag in the snapshot would fail.
}


// TODO(1236194): Since the heap size is configurable on the command line
// and through the API, we should gracefully handle the case that the heap
// size is not big enough to fit all the initial objects.
bool Heap::ConfigureHeap(int max_semi_space_size, int max_old_space_size,
                         int max_executable_size, size_t code_range_size) {
  if (HasBeenSetUp()) return false;

  // Overwrite default configuration.
  if (max_semi_space_size > 0) {
    max_semi_space_size_ = max_semi_space_size * MB;
  }
  if (max_old_space_size > 0) {
    max_old_generation_size_ = max_old_space_size * MB;
  }
  if (max_executable_size > 0) {
    max_executable_size_ = max_executable_size * MB;
  }

  // If max space size flags are specified overwrite the configuration.
  if (FLAG_max_semi_space_size > 0) {
    max_semi_space_size_ = FLAG_max_semi_space_size * MB;
  }
  if (FLAG_max_old_space_size > 0) {
    max_old_generation_size_ = FLAG_max_old_space_size * MB;
  }
  if (FLAG_max_executable_size > 0) {
    max_executable_size_ = FLAG_max_executable_size * MB;
  }

  if (FLAG_stress_compaction) {
    // This will cause more frequent GCs when stressing.
    max_semi_space_size_ = Page::kPageSize;
  }

  if (Snapshot::HaveASnapshotToStartFrom()) {
    // If we are using a snapshot we always reserve the default amount
    // of memory for each semispace because code in the snapshot has
    // write-barrier code that relies on the size and alignment of new
    // space.  We therefore cannot use a larger max semispace size
    // than the default reserved semispace size.
    if (max_semi_space_size_ > reserved_semispace_size_) {
      max_semi_space_size_ = reserved_semispace_size_;
      if (FLAG_trace_gc) {
        PrintPID("Max semi-space size cannot be more than %d kbytes\n",
                 reserved_semispace_size_ >> 10);
      }
    }
  } else {
    // If we are not using snapshots we reserve space for the actual
    // max semispace size.
    reserved_semispace_size_ = max_semi_space_size_;
  }

  // The max executable size must be less than or equal to the max old
  // generation size.
  if (max_executable_size_ > max_old_generation_size_) {
    max_executable_size_ = max_old_generation_size_;
  }

  // The new space size must be a power of two to support single-bit testing
  // for containment.
  max_semi_space_size_ =
      base::bits::RoundUpToPowerOfTwo32(max_semi_space_size_);
  reserved_semispace_size_ =
      base::bits::RoundUpToPowerOfTwo32(reserved_semispace_size_);

  if (FLAG_min_semi_space_size > 0) {
    int initial_semispace_size = FLAG_min_semi_space_size * MB;
    if (initial_semispace_size > max_semi_space_size_) {
      initial_semispace_size_ = max_semi_space_size_;
      if (FLAG_trace_gc) {
        PrintPID(
            "Min semi-space size cannot be more than the maximum"
            "semi-space size of %d MB\n",
            max_semi_space_size_);
      }
    } else {
      initial_semispace_size_ = initial_semispace_size;
    }
  }

  initial_semispace_size_ = Min(initial_semispace_size_, max_semi_space_size_);

  // The old generation is paged and needs at least one page for each space.
  int paged_space_count = LAST_PAGED_SPACE - FIRST_PAGED_SPACE + 1;
  max_old_generation_size_ =
      Max(static_cast<intptr_t>(paged_space_count * Page::kPageSize),
          max_old_generation_size_);

  // We rely on being able to allocate new arrays in paged spaces.
  DCHECK(Page::kMaxRegularHeapObjectSize >=
         (JSArray::kSize +
          FixedArray::SizeFor(JSObject::kInitialMaxFastElementArray) +
          AllocationMemento::kSize));

  code_range_size_ = code_range_size * MB;

  configured_ = true;
  return true;
}


bool Heap::ConfigureHeapDefault() { return ConfigureHeap(0, 0, 0, 0); }


void Heap::RecordStats(HeapStats* stats, bool take_snapshot) {
  *stats->start_marker = HeapStats::kStartMarker;
  *stats->end_marker = HeapStats::kEndMarker;
  *stats->new_space_size = new_space_.SizeAsInt();
  *stats->new_space_capacity = static_cast<int>(new_space_.Capacity());
  *stats->old_pointer_space_size = old_pointer_space_->SizeOfObjects();
  *stats->old_pointer_space_capacity = old_pointer_space_->Capacity();
  *stats->old_data_space_size = old_data_space_->SizeOfObjects();
  *stats->old_data_space_capacity = old_data_space_->Capacity();
  *stats->code_space_size = code_space_->SizeOfObjects();
  *stats->code_space_capacity = code_space_->Capacity();
  *stats->map_space_size = map_space_->SizeOfObjects();
  *stats->map_space_capacity = map_space_->Capacity();
  *stats->cell_space_size = cell_space_->SizeOfObjects();
  *stats->cell_space_capacity = cell_space_->Capacity();
  *stats->property_cell_space_size = property_cell_space_->SizeOfObjects();
  *stats->property_cell_space_capacity = property_cell_space_->Capacity();
  *stats->lo_space_size = lo_space_->Size();
  isolate_->global_handles()->RecordStats(stats);
  *stats->memory_allocator_size = isolate()->memory_allocator()->Size();
  *stats->memory_allocator_capacity =
      isolate()->memory_allocator()->Size() +
      isolate()->memory_allocator()->Available();
  *stats->os_error = base::OS::GetLastError();
  isolate()->memory_allocator()->Available();
  if (take_snapshot) {
    HeapIterator iterator(this);
    for (HeapObject* obj = iterator.next(); obj != NULL;
         obj = iterator.next()) {
      InstanceType type = obj->map()->instance_type();
      DCHECK(0 <= type && type <= LAST_TYPE);
      stats->objects_per_type[type]++;
      stats->size_per_type[type] += obj->Size();
    }
  }
}


intptr_t Heap::PromotedSpaceSizeOfObjects() {
  return old_pointer_space_->SizeOfObjects() +
         old_data_space_->SizeOfObjects() + code_space_->SizeOfObjects() +
         map_space_->SizeOfObjects() + cell_space_->SizeOfObjects() +
         property_cell_space_->SizeOfObjects() + lo_space_->SizeOfObjects();
}


int64_t Heap::PromotedExternalMemorySize() {
  if (amount_of_external_allocated_memory_ <=
      amount_of_external_allocated_memory_at_last_global_gc_)
    return 0;
  return amount_of_external_allocated_memory_ -
         amount_of_external_allocated_memory_at_last_global_gc_;
}


intptr_t Heap::OldGenerationAllocationLimit(intptr_t old_gen_size,
                                            int freed_global_handles) {
  const int kMaxHandles = 1000;
  const int kMinHandles = 100;
  double min_factor = 1.1;
  double max_factor = 4;
  // We set the old generation growing factor to 2 to grow the heap slower on
  // memory-constrained devices.
  if (max_old_generation_size_ <= kMaxOldSpaceSizeMediumMemoryDevice) {
    max_factor = 2;
  }
  // If there are many freed global handles, then the next full GC will
  // likely collect a lot of garbage. Choose the heap growing factor
  // depending on freed global handles.
  // TODO(ulan, hpayer): Take into account mutator utilization.
  double factor;
  if (freed_global_handles <= kMinHandles) {
    factor = max_factor;
  } else if (freed_global_handles >= kMaxHandles) {
    factor = min_factor;
  } else {
    // Compute factor using linear interpolation between points
    // (kMinHandles, max_factor) and (kMaxHandles, min_factor).
    factor = max_factor -
             (freed_global_handles - kMinHandles) * (max_factor - min_factor) /
                 (kMaxHandles - kMinHandles);
  }

  if (FLAG_stress_compaction ||
      mark_compact_collector()->reduce_memory_footprint_) {
    factor = min_factor;
  }

  intptr_t limit = static_cast<intptr_t>(old_gen_size * factor);
  limit = Max(limit, kMinimumOldGenerationAllocationLimit);
  limit += new_space_.Capacity();
  intptr_t halfway_to_the_max = (old_gen_size + max_old_generation_size_) / 2;
  return Min(limit, halfway_to_the_max);
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
  for (PagedSpace* space = spaces.next(); space != NULL;
       space = spaces.next()) {
    space->EmptyAllocationInfo();
  }
}


V8_DECLARE_ONCE(initialize_gc_once);

static void InitializeGCOnce() {
  InitializeScavengingVisitorsTables();
  NewSpaceScavenger::Initialize();
  MarkCompactCollector::Initialize();
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

  base::CallOnce(&initialize_gc_once, &InitializeGCOnce);

  MarkMapPointersAsEncoded(false);

  // Set up memory allocator.
  if (!isolate_->memory_allocator()->SetUp(MaxReserved(), MaxExecutableSize()))
    return false;

  // Set up new space.
  if (!new_space_.SetUp(reserved_semispace_size_, max_semi_space_size_)) {
    return false;
  }
  new_space_top_after_last_gc_ = new_space()->top();

  // Initialize old pointer space.
  old_pointer_space_ = new OldSpace(this, max_old_generation_size_,
                                    OLD_POINTER_SPACE, NOT_EXECUTABLE);
  if (old_pointer_space_ == NULL) return false;
  if (!old_pointer_space_->SetUp()) return false;

  // Initialize old data space.
  old_data_space_ = new OldSpace(this, max_old_generation_size_, OLD_DATA_SPACE,
                                 NOT_EXECUTABLE);
  if (old_data_space_ == NULL) return false;
  if (!old_data_space_->SetUp()) return false;

  if (!isolate_->code_range()->SetUp(code_range_size_)) return false;

  // Initialize the code space, set its maximum capacity to the old
  // generation size. It needs executable memory.
  code_space_ =
      new OldSpace(this, max_old_generation_size_, CODE_SPACE, EXECUTABLE);
  if (code_space_ == NULL) return false;
  if (!code_space_->SetUp()) return false;

  // Initialize map space.
  map_space_ = new MapSpace(this, max_old_generation_size_, MAP_SPACE);
  if (map_space_ == NULL) return false;
  if (!map_space_->SetUp()) return false;

  // Initialize simple cell space.
  cell_space_ = new CellSpace(this, max_old_generation_size_, CELL_SPACE);
  if (cell_space_ == NULL) return false;
  if (!cell_space_->SetUp()) return false;

  // Initialize global property cell space.
  property_cell_space_ = new PropertyCellSpace(this, max_old_generation_size_,
                                               PROPERTY_CELL_SPACE);
  if (property_cell_space_ == NULL) return false;
  if (!property_cell_space_->SetUp()) return false;

  // The large object code space may contain code or data.  We set the memory
  // to be non-executable here for safety, but this means we need to enable it
  // explicitly when allocating large code objects.
  lo_space_ = new LargeObjectSpace(this, max_old_generation_size_, LO_SPACE);
  if (lo_space_ == NULL) return false;
  if (!lo_space_->SetUp()) return false;

  // Set up the seed that is used to randomize the string hash function.
  DCHECK(hash_seed() == 0);
  if (FLAG_randomize_hashes) {
    if (FLAG_hash_seed == 0) {
      int rnd = isolate()->random_number_generator()->NextInt();
      set_hash_seed(Smi::FromInt(rnd & Name::kHashBitMask));
    } else {
      set_hash_seed(Smi::FromInt(FLAG_hash_seed));
    }
  }

  LOG(isolate_, IntPtrTEvent("heap-capacity", Capacity()));
  LOG(isolate_, IntPtrTEvent("heap-available", Available()));

  store_buffer()->SetUp();

  mark_compact_collector()->SetUp();

  return true;
}


bool Heap::CreateHeapObjects() {
  // Create initial maps.
  if (!CreateInitialMaps()) return false;
  CreateApiObjects();

  // Create initial objects
  CreateInitialObjects();
  CHECK_EQ(0, gc_count_);

  set_native_contexts_list(undefined_value());
  set_array_buffers_list(undefined_value());
  set_allocation_sites_list(undefined_value());
  weak_object_to_code_table_ = undefined_value();
  return true;
}


void Heap::SetStackLimits() {
  DCHECK(isolate_ != NULL);
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


void Heap::TearDown() {
#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) {
    Verify();
  }
#endif

  UpdateMaximumCommitted();

  if (FLAG_print_cumulative_gc_stat) {
    PrintF("\n");
    PrintF("gc_count=%d ", gc_count_);
    PrintF("mark_sweep_count=%d ", ms_count_);
    PrintF("max_gc_pause=%.1f ", get_max_gc_pause());
    PrintF("total_gc_time=%.1f ", total_gc_time_ms_);
    PrintF("min_in_mutator=%.1f ", get_min_in_mutator());
    PrintF("max_alive_after_gc=%" V8_PTR_PREFIX "d ", get_max_alive_after_gc());
    PrintF("total_marking_time=%.1f ", tracer_.cumulative_sweeping_duration());
    PrintF("total_sweeping_time=%.1f ", tracer_.cumulative_sweeping_duration());
    PrintF("\n\n");
  }

  if (FLAG_print_max_heap_committed) {
    PrintF("\n");
    PrintF("maximum_committed_by_heap=%" V8_PTR_PREFIX "d ",
           MaximumCommittedMemory());
    PrintF("maximum_committed_by_new_space=%" V8_PTR_PREFIX "d ",
           new_space_.MaximumCommittedMemory());
    PrintF("maximum_committed_by_old_pointer_space=%" V8_PTR_PREFIX "d ",
           old_data_space_->MaximumCommittedMemory());
    PrintF("maximum_committed_by_old_data_space=%" V8_PTR_PREFIX "d ",
           old_pointer_space_->MaximumCommittedMemory());
    PrintF("maximum_committed_by_old_data_space=%" V8_PTR_PREFIX "d ",
           old_pointer_space_->MaximumCommittedMemory());
    PrintF("maximum_committed_by_code_space=%" V8_PTR_PREFIX "d ",
           code_space_->MaximumCommittedMemory());
    PrintF("maximum_committed_by_map_space=%" V8_PTR_PREFIX "d ",
           map_space_->MaximumCommittedMemory());
    PrintF("maximum_committed_by_cell_space=%" V8_PTR_PREFIX "d ",
           cell_space_->MaximumCommittedMemory());
    PrintF("maximum_committed_by_property_space=%" V8_PTR_PREFIX "d ",
           property_cell_space_->MaximumCommittedMemory());
    PrintF("maximum_committed_by_lo_space=%" V8_PTR_PREFIX "d ",
           lo_space_->MaximumCommittedMemory());
    PrintF("\n\n");
  }

  if (FLAG_verify_predictable) {
    PrintAlloctionsHash();
  }

  TearDownArrayBuffers();

  isolate_->global_handles()->TearDown();

  external_string_table_.TearDown();

  mark_compact_collector()->TearDown();

  new_space_.TearDown();

  if (old_pointer_space_ != NULL) {
    old_pointer_space_->TearDown();
    delete old_pointer_space_;
    old_pointer_space_ = NULL;
  }

  if (old_data_space_ != NULL) {
    old_data_space_->TearDown();
    delete old_data_space_;
    old_data_space_ = NULL;
  }

  if (code_space_ != NULL) {
    code_space_->TearDown();
    delete code_space_;
    code_space_ = NULL;
  }

  if (map_space_ != NULL) {
    map_space_->TearDown();
    delete map_space_;
    map_space_ = NULL;
  }

  if (cell_space_ != NULL) {
    cell_space_->TearDown();
    delete cell_space_;
    cell_space_ = NULL;
  }

  if (property_cell_space_ != NULL) {
    property_cell_space_->TearDown();
    delete property_cell_space_;
    property_cell_space_ = NULL;
  }

  if (lo_space_ != NULL) {
    lo_space_->TearDown();
    delete lo_space_;
    lo_space_ = NULL;
  }

  store_buffer()->TearDown();
  incremental_marking()->TearDown();

  isolate_->memory_allocator()->TearDown();
}


void Heap::AddGCPrologueCallback(v8::Isolate::GCPrologueCallback callback,
                                 GCType gc_type, bool pass_isolate) {
  DCHECK(callback != NULL);
  GCPrologueCallbackPair pair(callback, gc_type, pass_isolate);
  DCHECK(!gc_prologue_callbacks_.Contains(pair));
  return gc_prologue_callbacks_.Add(pair);
}


void Heap::RemoveGCPrologueCallback(v8::Isolate::GCPrologueCallback callback) {
  DCHECK(callback != NULL);
  for (int i = 0; i < gc_prologue_callbacks_.length(); ++i) {
    if (gc_prologue_callbacks_[i].callback == callback) {
      gc_prologue_callbacks_.Remove(i);
      return;
    }
  }
  UNREACHABLE();
}


void Heap::AddGCEpilogueCallback(v8::Isolate::GCEpilogueCallback callback,
                                 GCType gc_type, bool pass_isolate) {
  DCHECK(callback != NULL);
  GCEpilogueCallbackPair pair(callback, gc_type, pass_isolate);
  DCHECK(!gc_epilogue_callbacks_.Contains(pair));
  return gc_epilogue_callbacks_.Add(pair);
}


void Heap::RemoveGCEpilogueCallback(v8::Isolate::GCEpilogueCallback callback) {
  DCHECK(callback != NULL);
  for (int i = 0; i < gc_epilogue_callbacks_.length(); ++i) {
    if (gc_epilogue_callbacks_[i].callback == callback) {
      gc_epilogue_callbacks_.Remove(i);
      return;
    }
  }
  UNREACHABLE();
}


// TODO(ishell): Find a better place for this.
void Heap::AddWeakObjectToCodeDependency(Handle<Object> obj,
                                         Handle<DependentCode> dep) {
  DCHECK(!InNewSpace(*obj));
  DCHECK(!InNewSpace(*dep));
  // This handle scope keeps the table handle local to this function, which
  // allows us to safely skip write barriers in table update operations.
  HandleScope scope(isolate());
  Handle<WeakHashTable> table(WeakHashTable::cast(weak_object_to_code_table_),
                              isolate());
  table = WeakHashTable::Put(table, obj, dep);

  if (ShouldZapGarbage() && weak_object_to_code_table_ != *table) {
    WeakHashTable::cast(weak_object_to_code_table_)->Zap(the_hole_value());
  }
  set_weak_object_to_code_table(*table);
  DCHECK_EQ(*dep, table->Lookup(obj));
}


DependentCode* Heap::LookupWeakObjectToCodeDependency(Handle<Object> obj) {
  Object* dep = WeakHashTable::cast(weak_object_to_code_table_)->Lookup(obj);
  if (dep->IsDependentCode()) return DependentCode::cast(dep);
  return DependentCode::cast(empty_fixed_array());
}


void Heap::EnsureWeakObjectToCodeTable() {
  if (!weak_object_to_code_table()->IsHashTable()) {
    set_weak_object_to_code_table(
        *WeakHashTable::New(isolate(), 16, USE_DEFAULT_MINIMUM_CAPACITY,
                            TENURED));
  }
}


void Heap::FatalProcessOutOfMemory(const char* location, bool take_snapshot) {
  v8::internal::V8::FatalProcessOutOfMemory(location, take_snapshot);
}

#ifdef DEBUG

class PrintHandleVisitor : public ObjectVisitor {
 public:
  void VisitPointers(Object** start, Object** end) {
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


Space* AllSpaces::next() {
  switch (counter_++) {
    case NEW_SPACE:
      return heap_->new_space();
    case OLD_POINTER_SPACE:
      return heap_->old_pointer_space();
    case OLD_DATA_SPACE:
      return heap_->old_data_space();
    case CODE_SPACE:
      return heap_->code_space();
    case MAP_SPACE:
      return heap_->map_space();
    case CELL_SPACE:
      return heap_->cell_space();
    case PROPERTY_CELL_SPACE:
      return heap_->property_cell_space();
    case LO_SPACE:
      return heap_->lo_space();
    default:
      return NULL;
  }
}


PagedSpace* PagedSpaces::next() {
  switch (counter_++) {
    case OLD_POINTER_SPACE:
      return heap_->old_pointer_space();
    case OLD_DATA_SPACE:
      return heap_->old_data_space();
    case CODE_SPACE:
      return heap_->code_space();
    case MAP_SPACE:
      return heap_->map_space();
    case CELL_SPACE:
      return heap_->cell_space();
    case PROPERTY_CELL_SPACE:
      return heap_->property_cell_space();
    default:
      return NULL;
  }
}


OldSpace* OldSpaces::next() {
  switch (counter_++) {
    case OLD_POINTER_SPACE:
      return heap_->old_pointer_space();
    case OLD_DATA_SPACE:
      return heap_->old_data_space();
    case CODE_SPACE:
      return heap_->code_space();
    default:
      return NULL;
  }
}


SpaceIterator::SpaceIterator(Heap* heap)
    : heap_(heap),
      current_space_(FIRST_SPACE),
      iterator_(NULL),
      size_func_(NULL) {}


SpaceIterator::SpaceIterator(Heap* heap, HeapObjectCallback size_func)
    : heap_(heap),
      current_space_(FIRST_SPACE),
      iterator_(NULL),
      size_func_(size_func) {}


SpaceIterator::~SpaceIterator() {
  // Delete active iterator if any.
  delete iterator_;
}


bool SpaceIterator::has_next() {
  // Iterate until no more spaces.
  return current_space_ != LAST_SPACE;
}


ObjectIterator* SpaceIterator::next() {
  if (iterator_ != NULL) {
    delete iterator_;
    iterator_ = NULL;
    // Move to the next space
    current_space_++;
    if (current_space_ > LAST_SPACE) {
      return NULL;
    }
  }

  // Return iterator for the new current space.
  return CreateIterator();
}


// Create an iterator for the space to iterate.
ObjectIterator* SpaceIterator::CreateIterator() {
  DCHECK(iterator_ == NULL);

  switch (current_space_) {
    case NEW_SPACE:
      iterator_ = new SemiSpaceIterator(heap_->new_space(), size_func_);
      break;
    case OLD_POINTER_SPACE:
      iterator_ =
          new HeapObjectIterator(heap_->old_pointer_space(), size_func_);
      break;
    case OLD_DATA_SPACE:
      iterator_ = new HeapObjectIterator(heap_->old_data_space(), size_func_);
      break;
    case CODE_SPACE:
      iterator_ = new HeapObjectIterator(heap_->code_space(), size_func_);
      break;
    case MAP_SPACE:
      iterator_ = new HeapObjectIterator(heap_->map_space(), size_func_);
      break;
    case CELL_SPACE:
      iterator_ = new HeapObjectIterator(heap_->cell_space(), size_func_);
      break;
    case PROPERTY_CELL_SPACE:
      iterator_ =
          new HeapObjectIterator(heap_->property_cell_space(), size_func_);
      break;
    case LO_SPACE:
      iterator_ = new LargeObjectIterator(heap_->lo_space(), size_func_);
      break;
  }

  // Return the newly allocated iterator;
  DCHECK(iterator_ != NULL);
  return iterator_;
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
    heap_->mark_compact_collector()->ClearMarkbits();
  }

  bool SkipObject(HeapObject* object) {
    MarkBit mark_bit = Marking::MarkBitFrom(object);
    return !mark_bit.Get();
  }

 private:
  class MarkingVisitor : public ObjectVisitor {
   public:
    MarkingVisitor() : marking_stack_(10) {}

    void VisitPointers(Object** start, Object** end) {
      for (Object** p = start; p < end; p++) {
        if (!(*p)->IsHeapObject()) continue;
        HeapObject* obj = HeapObject::cast(*p);
        MarkBit mark_bit = Marking::MarkBitFrom(obj);
        if (!mark_bit.Get()) {
          mark_bit.Set();
          marking_stack_.Add(obj);
        }
      }
    }

    void TransitiveClosure() {
      while (!marking_stack_.is_empty()) {
        HeapObject* obj = marking_stack_.RemoveLast();
        obj->Iterate(this);
      }
    }

   private:
    List<HeapObject*> marking_stack_;
  };

  void MarkReachableObjects() {
    MarkingVisitor visitor;
    heap_->IterateRoots(&visitor, VISIT_ALL);
    visitor.TransitiveClosure();
  }

  Heap* heap_;
  DisallowHeapAllocation no_allocation_;
};


HeapIterator::HeapIterator(Heap* heap)
    : make_heap_iterable_helper_(heap),
      no_heap_allocation_(),
      heap_(heap),
      filtering_(HeapIterator::kNoFiltering),
      filter_(NULL) {
  Init();
}


HeapIterator::HeapIterator(Heap* heap,
                           HeapIterator::HeapObjectsFiltering filtering)
    : make_heap_iterable_helper_(heap),
      no_heap_allocation_(),
      heap_(heap),
      filtering_(filtering),
      filter_(NULL) {
  Init();
}


HeapIterator::~HeapIterator() { Shutdown(); }


void HeapIterator::Init() {
  // Start the iteration.
  space_iterator_ = new SpaceIterator(heap_);
  switch (filtering_) {
    case kFilterUnreachable:
      filter_ = new UnreachableObjectsFilter(heap_);
      break;
    default:
      break;
  }
  object_iterator_ = space_iterator_->next();
}


void HeapIterator::Shutdown() {
#ifdef DEBUG
  // Assert that in filtering mode we have iterated through all
  // objects. Otherwise, heap will be left in an inconsistent state.
  if (filtering_ != kNoFiltering) {
    DCHECK(object_iterator_ == NULL);
  }
#endif
  // Make sure the last iterator is deallocated.
  delete space_iterator_;
  space_iterator_ = NULL;
  object_iterator_ = NULL;
  delete filter_;
  filter_ = NULL;
}


HeapObject* HeapIterator::next() {
  if (filter_ == NULL) return NextObject();

  HeapObject* obj = NextObject();
  while (obj != NULL && filter_->SkipObject(obj)) obj = NextObject();
  return obj;
}


HeapObject* HeapIterator::NextObject() {
  // No iterator means we are done.
  if (object_iterator_ == NULL) return NULL;

  if (HeapObject* obj = object_iterator_->next_object()) {
    // If the current iterator has more objects we are fine.
    return obj;
  } else {
    // Go though the spaces looking for one that has objects.
    while (space_iterator_->has_next()) {
      object_iterator_ = space_iterator_->next();
      if (HeapObject* obj = object_iterator_->next_object()) {
        return obj;
      }
    }
  }
  // Done with the last space.
  object_iterator_ = NULL;
  return NULL;
}


void HeapIterator::reset() {
  // Restart the iterator.
  Shutdown();
  Init();
}


#ifdef DEBUG

Object* const PathTracer::kAnyGlobalObject = NULL;

class PathTracer::MarkVisitor : public ObjectVisitor {
 public:
  explicit MarkVisitor(PathTracer* tracer) : tracer_(tracer) {}
  void VisitPointers(Object** start, Object** end) {
    // Scan all HeapObject pointers in [start, end)
    for (Object** p = start; !tracer_->found() && (p < end); p++) {
      if ((*p)->IsHeapObject()) tracer_->MarkRecursively(p, this);
    }
  }

 private:
  PathTracer* tracer_;
};


class PathTracer::UnmarkVisitor : public ObjectVisitor {
 public:
  explicit UnmarkVisitor(PathTracer* tracer) : tracer_(tracer) {}
  void VisitPointers(Object** start, Object** end) {
    // Scan all HeapObject pointers in [start, end)
    for (Object** p = start; p < end; p++) {
      if ((*p)->IsHeapObject()) tracer_->UnmarkRecursively(p, this);
    }
  }

 private:
  PathTracer* tracer_;
};


void PathTracer::VisitPointers(Object** start, Object** end) {
  bool done = ((what_to_find_ == FIND_FIRST) && found_target_);
  // Visit all HeapObject pointers in [start, end)
  for (Object** p = start; !done && (p < end); p++) {
    if ((*p)->IsHeapObject()) {
      TracePathFrom(p);
      done = ((what_to_find_ == FIND_FIRST) && found_target_);
    }
  }
}


void PathTracer::Reset() {
  found_target_ = false;
  object_stack_.Clear();
}


void PathTracer::TracePathFrom(Object** root) {
  DCHECK((search_target_ == kAnyGlobalObject) ||
         search_target_->IsHeapObject());
  found_target_in_trace_ = false;
  Reset();

  MarkVisitor mark_visitor(this);
  MarkRecursively(root, &mark_visitor);

  UnmarkVisitor unmark_visitor(this);
  UnmarkRecursively(root, &unmark_visitor);

  ProcessResults();
}


static bool SafeIsNativeContext(HeapObject* obj) {
  return obj->map() == obj->GetHeap()->raw_unchecked_native_context_map();
}


void PathTracer::MarkRecursively(Object** p, MarkVisitor* mark_visitor) {
  if (!(*p)->IsHeapObject()) return;

  HeapObject* obj = HeapObject::cast(*p);

  MapWord map_word = obj->map_word();
  if (!map_word.ToMap()->IsHeapObject()) return;  // visited before

  if (found_target_in_trace_) return;  // stop if target found
  object_stack_.Add(obj);
  if (((search_target_ == kAnyGlobalObject) && obj->IsJSGlobalObject()) ||
      (obj == search_target_)) {
    found_target_in_trace_ = true;
    found_target_ = true;
    return;
  }

  bool is_native_context = SafeIsNativeContext(obj);

  // not visited yet
  Map* map = Map::cast(map_word.ToMap());

  MapWord marked_map_word =
      MapWord::FromRawValue(obj->map_word().ToRawValue() + kMarkTag);
  obj->set_map_word(marked_map_word);

  // Scan the object body.
  if (is_native_context && (visit_mode_ == VISIT_ONLY_STRONG)) {
    // This is specialized to scan Context's properly.
    Object** start =
        reinterpret_cast<Object**>(obj->address() + Context::kHeaderSize);
    Object** end =
        reinterpret_cast<Object**>(obj->address() + Context::kHeaderSize +
                                   Context::FIRST_WEAK_SLOT * kPointerSize);
    mark_visitor->VisitPointers(start, end);
  } else {
    obj->IterateBody(map->instance_type(), obj->SizeFromMap(map), mark_visitor);
  }

  // Scan the map after the body because the body is a lot more interesting
  // when doing leak detection.
  MarkRecursively(reinterpret_cast<Object**>(&map), mark_visitor);

  if (!found_target_in_trace_) {  // don't pop if found the target
    object_stack_.RemoveLast();
  }
}


void PathTracer::UnmarkRecursively(Object** p, UnmarkVisitor* unmark_visitor) {
  if (!(*p)->IsHeapObject()) return;

  HeapObject* obj = HeapObject::cast(*p);

  MapWord map_word = obj->map_word();
  if (map_word.ToMap()->IsHeapObject()) return;  // unmarked already

  MapWord unmarked_map_word =
      MapWord::FromRawValue(map_word.ToRawValue() - kMarkTag);
  obj->set_map_word(unmarked_map_word);

  Map* map = Map::cast(unmarked_map_word.ToMap());

  UnmarkRecursively(reinterpret_cast<Object**>(&map), unmark_visitor);

  obj->IterateBody(map->instance_type(), obj->SizeFromMap(map), unmark_visitor);
}


void PathTracer::ProcessResults() {
  if (found_target_) {
    OFStream os(stdout);
    os << "=====================================\n"
       << "====        Path to object       ====\n"
       << "=====================================\n\n";

    DCHECK(!object_stack_.is_empty());
    for (int i = 0; i < object_stack_.length(); i++) {
      if (i > 0) os << "\n     |\n     |\n     V\n\n";
      object_stack_[i]->Print(os);
    }
    os << "=====================================\n";
  }
}


// Triggers a depth-first traversal of reachable objects from one
// given root object and finds a path to a specific heap object and
// prints it.
void Heap::TracePathToObjectFrom(Object* target, Object* root) {
  PathTracer tracer(target, PathTracer::FIND_ALL, VISIT_ALL);
  tracer.VisitPointer(&root);
}


// Triggers a depth-first traversal of reachable objects from roots
// and finds a path to a specific heap object and prints it.
void Heap::TracePathToObject(Object* target) {
  PathTracer tracer(target, PathTracer::FIND_ALL, VISIT_ALL);
  IterateRoots(&tracer, VISIT_ONLY_STRONG);
}


// Triggers a depth-first traversal of reachable objects from roots
// and finds a path to any global object and prints it. Useful for
// determining the source for leaks of global objects.
void Heap::TracePathToGlobal() {
  PathTracer tracer(PathTracer::kAnyGlobalObject, PathTracer::FIND_ALL,
                    VISIT_ALL);
  IterateRoots(&tracer, VISIT_ONLY_STRONG);
}
#endif


void Heap::UpdateCumulativeGCStatistics(double duration,
                                        double spent_in_mutator,
                                        double marking_time) {
  if (FLAG_print_cumulative_gc_stat) {
    total_gc_time_ms_ += duration;
    max_gc_pause_ = Max(max_gc_pause_, duration);
    max_alive_after_gc_ = Max(max_alive_after_gc_, SizeOfObjects());
    min_in_mutator_ = Min(min_in_mutator_, spent_in_mutator);
  } else if (FLAG_trace_gc_verbose) {
    total_gc_time_ms_ += duration;
  }

  marking_time_ += marking_time;
}


int KeyedLookupCache::Hash(Handle<Map> map, Handle<Name> name) {
  DisallowHeapAllocation no_gc;
  // Uses only lower 32 bits if pointers are larger.
  uintptr_t addr_hash =
      static_cast<uint32_t>(reinterpret_cast<uintptr_t>(*map)) >> kMapHashShift;
  return static_cast<uint32_t>((addr_hash ^ name->Hash()) & kCapacityMask);
}


int KeyedLookupCache::Lookup(Handle<Map> map, Handle<Name> name) {
  DisallowHeapAllocation no_gc;
  int index = (Hash(map, name) & kHashMask);
  for (int i = 0; i < kEntriesPerBucket; i++) {
    Key& key = keys_[index + i];
    if ((key.map == *map) && key.name->Equals(*name)) {
      return field_offsets_[index + i];
    }
  }
  return kNotFound;
}


void KeyedLookupCache::Update(Handle<Map> map, Handle<Name> name,
                              int field_offset) {
  DisallowHeapAllocation no_gc;
  if (!name->IsUniqueName()) {
    if (!StringTable::InternalizeStringIfExists(
             name->GetIsolate(), Handle<String>::cast(name)).ToHandle(&name)) {
      return;
    }
  }
  // This cache is cleared only between mark compact passes, so we expect the
  // cache to only contain old space names.
  DCHECK(!map->GetIsolate()->heap()->InNewSpace(*name));

  int index = (Hash(map, name) & kHashMask);
  // After a GC there will be free slots, so we use them in order (this may
  // help to get the most frequently used one in position 0).
  for (int i = 0; i < kEntriesPerBucket; i++) {
    Key& key = keys_[index];
    Object* free_entry_indicator = NULL;
    if (key.map == free_entry_indicator) {
      key.map = *map;
      key.name = *name;
      field_offsets_[index + i] = field_offset;
      return;
    }
  }
  // No free entry found in this bucket, so we move them all down one and
  // put the new entry at position zero.
  for (int i = kEntriesPerBucket - 1; i > 0; i--) {
    Key& key = keys_[index + i];
    Key& key2 = keys_[index + i - 1];
    key = key2;
    field_offsets_[index + i] = field_offsets_[index + i - 1];
  }

  // Write the new first entry.
  Key& key = keys_[index];
  key.map = *map;
  key.name = *name;
  field_offsets_[index] = field_offset;
}


void KeyedLookupCache::Clear() {
  for (int index = 0; index < kLength; index++) keys_[index].map = NULL;
}


void DescriptorLookupCache::Clear() {
  for (int index = 0; index < kLength; index++) keys_[index].source = NULL;
}


void ExternalStringTable::CleanUp() {
  int last = 0;
  for (int i = 0; i < new_space_strings_.length(); ++i) {
    if (new_space_strings_[i] == heap_->the_hole_value()) {
      continue;
    }
    DCHECK(new_space_strings_[i]->IsExternalString());
    if (heap_->InNewSpace(new_space_strings_[i])) {
      new_space_strings_[last++] = new_space_strings_[i];
    } else {
      old_space_strings_.Add(new_space_strings_[i]);
    }
  }
  new_space_strings_.Rewind(last);
  new_space_strings_.Trim();

  last = 0;
  for (int i = 0; i < old_space_strings_.length(); ++i) {
    if (old_space_strings_[i] == heap_->the_hole_value()) {
      continue;
    }
    DCHECK(old_space_strings_[i]->IsExternalString());
    DCHECK(!heap_->InNewSpace(old_space_strings_[i]));
    old_space_strings_[last++] = old_space_strings_[i];
  }
  old_space_strings_.Rewind(last);
  old_space_strings_.Trim();
#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) {
    Verify();
  }
#endif
}


void ExternalStringTable::TearDown() {
  for (int i = 0; i < new_space_strings_.length(); ++i) {
    heap_->FinalizeExternalString(ExternalString::cast(new_space_strings_[i]));
  }
  new_space_strings_.Free();
  for (int i = 0; i < old_space_strings_.length(); ++i) {
    heap_->FinalizeExternalString(ExternalString::cast(old_space_strings_[i]));
  }
  old_space_strings_.Free();
}


void Heap::QueueMemoryChunkForFree(MemoryChunk* chunk) {
  chunk->set_next_chunk(chunks_queued_for_free_);
  chunks_queued_for_free_ = chunk;
}


void Heap::FreeQueuedChunks() {
  if (chunks_queued_for_free_ == NULL) return;
  MemoryChunk* next;
  MemoryChunk* chunk;
  for (chunk = chunks_queued_for_free_; chunk != NULL; chunk = next) {
    next = chunk->next_chunk();
    chunk->SetFlag(MemoryChunk::ABOUT_TO_BE_FREED);

    if (chunk->owner()->identity() == LO_SPACE) {
      // StoreBuffer::Filter relies on MemoryChunk::FromAnyPointerAddress.
      // If FromAnyPointerAddress encounters a slot that belongs to a large
      // chunk queued for deletion it will fail to find the chunk because
      // it try to perform a search in the list of pages owned by of the large
      // object space and queued chunks were detached from that list.
      // To work around this we split large chunk into normal kPageSize aligned
      // pieces and initialize size, owner and flags field of every piece.
      // If FromAnyPointerAddress encounters a slot that belongs to one of
      // these smaller pieces it will treat it as a slot on a normal Page.
      Address chunk_end = chunk->address() + chunk->size();
      MemoryChunk* inner =
          MemoryChunk::FromAddress(chunk->address() + Page::kPageSize);
      MemoryChunk* inner_last = MemoryChunk::FromAddress(chunk_end - 1);
      while (inner <= inner_last) {
        // Size of a large chunk is always a multiple of
        // OS::AllocateAlignment() so there is always
        // enough space for a fake MemoryChunk header.
        Address area_end = Min(inner->address() + Page::kPageSize, chunk_end);
        // Guard against overflow.
        if (area_end < inner->address()) area_end = chunk_end;
        inner->SetArea(inner->address(), area_end);
        inner->set_size(Page::kPageSize);
        inner->set_owner(lo_space());
        inner->SetFlag(MemoryChunk::ABOUT_TO_BE_FREED);
        inner = MemoryChunk::FromAddress(inner->address() + Page::kPageSize);
      }
    }
  }
  isolate_->heap()->store_buffer()->Compact();
  isolate_->heap()->store_buffer()->Filter(MemoryChunk::ABOUT_TO_BE_FREED);
  for (chunk = chunks_queued_for_free_; chunk != NULL; chunk = next) {
    next = chunk->next_chunk();
    isolate_->memory_allocator()->Free(chunk);
  }
  chunks_queued_for_free_ = NULL;
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


void Heap::ClearObjectStats(bool clear_last_time_stats) {
  memset(object_counts_, 0, sizeof(object_counts_));
  memset(object_sizes_, 0, sizeof(object_sizes_));
  if (clear_last_time_stats) {
    memset(object_counts_last_time_, 0, sizeof(object_counts_last_time_));
    memset(object_sizes_last_time_, 0, sizeof(object_sizes_last_time_));
  }
}


static base::LazyMutex checkpoint_object_stats_mutex = LAZY_MUTEX_INITIALIZER;


void Heap::CheckpointObjectStats() {
  base::LockGuard<base::Mutex> lock_guard(
      checkpoint_object_stats_mutex.Pointer());
  Counters* counters = isolate()->counters();
#define ADJUST_LAST_TIME_OBJECT_COUNT(name)              \
  counters->count_of_##name()->Increment(                \
      static_cast<int>(object_counts_[name]));           \
  counters->count_of_##name()->Decrement(                \
      static_cast<int>(object_counts_last_time_[name])); \
  counters->size_of_##name()->Increment(                 \
      static_cast<int>(object_sizes_[name]));            \
  counters->size_of_##name()->Decrement(                 \
      static_cast<int>(object_sizes_last_time_[name]));
  INSTANCE_TYPE_LIST(ADJUST_LAST_TIME_OBJECT_COUNT)
#undef ADJUST_LAST_TIME_OBJECT_COUNT
  int index;
#define ADJUST_LAST_TIME_OBJECT_COUNT(name)               \
  index = FIRST_CODE_KIND_SUB_TYPE + Code::name;          \
  counters->count_of_CODE_TYPE_##name()->Increment(       \
      static_cast<int>(object_counts_[index]));           \
  counters->count_of_CODE_TYPE_##name()->Decrement(       \
      static_cast<int>(object_counts_last_time_[index])); \
  counters->size_of_CODE_TYPE_##name()->Increment(        \
      static_cast<int>(object_sizes_[index]));            \
  counters->size_of_CODE_TYPE_##name()->Decrement(        \
      static_cast<int>(object_sizes_last_time_[index]));
  CODE_KIND_LIST(ADJUST_LAST_TIME_OBJECT_COUNT)
#undef ADJUST_LAST_TIME_OBJECT_COUNT
#define ADJUST_LAST_TIME_OBJECT_COUNT(name)               \
  index = FIRST_FIXED_ARRAY_SUB_TYPE + name;              \
  counters->count_of_FIXED_ARRAY_##name()->Increment(     \
      static_cast<int>(object_counts_[index]));           \
  counters->count_of_FIXED_ARRAY_##name()->Decrement(     \
      static_cast<int>(object_counts_last_time_[index])); \
  counters->size_of_FIXED_ARRAY_##name()->Increment(      \
      static_cast<int>(object_sizes_[index]));            \
  counters->size_of_FIXED_ARRAY_##name()->Decrement(      \
      static_cast<int>(object_sizes_last_time_[index]));
  FIXED_ARRAY_SUB_INSTANCE_TYPE_LIST(ADJUST_LAST_TIME_OBJECT_COUNT)
#undef ADJUST_LAST_TIME_OBJECT_COUNT
#define ADJUST_LAST_TIME_OBJECT_COUNT(name)                                   \
  index =                                                                     \
      FIRST_CODE_AGE_SUB_TYPE + Code::k##name##CodeAge - Code::kFirstCodeAge; \
  counters->count_of_CODE_AGE_##name()->Increment(                            \
      static_cast<int>(object_counts_[index]));                               \
  counters->count_of_CODE_AGE_##name()->Decrement(                            \
      static_cast<int>(object_counts_last_time_[index]));                     \
  counters->size_of_CODE_AGE_##name()->Increment(                             \
      static_cast<int>(object_sizes_[index]));                                \
  counters->size_of_CODE_AGE_##name()->Decrement(                             \
      static_cast<int>(object_sizes_last_time_[index]));
  CODE_AGE_LIST_COMPLETE(ADJUST_LAST_TIME_OBJECT_COUNT)
#undef ADJUST_LAST_TIME_OBJECT_COUNT

  MemCopy(object_counts_last_time_, object_counts_, sizeof(object_counts_));
  MemCopy(object_sizes_last_time_, object_sizes_, sizeof(object_sizes_));
  ClearObjectStats();
}
}
}  // namespace v8::internal
