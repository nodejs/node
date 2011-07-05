// Copyright 2010 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "v8.h"

#include "accessors.h"
#include "api.h"
#include "bootstrapper.h"
#include "codegen-inl.h"
#include "compilation-cache.h"
#include "debug.h"
#include "heap-profiler.h"
#include "global-handles.h"
#include "liveobjectlist-inl.h"
#include "mark-compact.h"
#include "natives.h"
#include "objects-visiting.h"
#include "runtime-profiler.h"
#include "scanner-base.h"
#include "scopeinfo.h"
#include "snapshot.h"
#include "v8threads.h"
#include "vm-state-inl.h"
#if V8_TARGET_ARCH_ARM && !V8_INTERPRETED_REGEXP
#include "regexp-macro-assembler.h"
#include "arm/regexp-macro-assembler-arm.h"
#endif


namespace v8 {
namespace internal {


String* Heap::hidden_symbol_;
Object* Heap::roots_[Heap::kRootListLength];
Object* Heap::global_contexts_list_;


NewSpace Heap::new_space_;
OldSpace* Heap::old_pointer_space_ = NULL;
OldSpace* Heap::old_data_space_ = NULL;
OldSpace* Heap::code_space_ = NULL;
MapSpace* Heap::map_space_ = NULL;
CellSpace* Heap::cell_space_ = NULL;
LargeObjectSpace* Heap::lo_space_ = NULL;

static const intptr_t kMinimumPromotionLimit = 2 * MB;
static const intptr_t kMinimumAllocationLimit = 8 * MB;

intptr_t Heap::old_gen_promotion_limit_ = kMinimumPromotionLimit;
intptr_t Heap::old_gen_allocation_limit_ = kMinimumAllocationLimit;

int Heap::old_gen_exhausted_ = false;

int Heap::amount_of_external_allocated_memory_ = 0;
int Heap::amount_of_external_allocated_memory_at_last_global_gc_ = 0;

// semispace_size_ should be a power of 2 and old_generation_size_ should be
// a multiple of Page::kPageSize.
#if defined(ANDROID)
static const int default_max_semispace_size_  = 2*MB;
intptr_t Heap::max_old_generation_size_ = 192*MB;
int Heap::initial_semispace_size_ = 128*KB;
intptr_t Heap::code_range_size_ = 0;
intptr_t Heap::max_executable_size_ = max_old_generation_size_;
#elif defined(V8_TARGET_ARCH_X64)
static const int default_max_semispace_size_  = 16*MB;
intptr_t Heap::max_old_generation_size_ = 1*GB;
int Heap::initial_semispace_size_ = 1*MB;
intptr_t Heap::code_range_size_ = 512*MB;
intptr_t Heap::max_executable_size_ = 256*MB;
#else
static const int default_max_semispace_size_  = 8*MB;
intptr_t Heap::max_old_generation_size_ = 512*MB;
int Heap::initial_semispace_size_ = 512*KB;
intptr_t Heap::code_range_size_ = 0;
intptr_t Heap::max_executable_size_ = 128*MB;
#endif

// Allow build-time customization of the max semispace size. Building
// V8 with snapshots and a non-default max semispace size is much
// easier if you can define it as part of the build environment.
#if defined(V8_MAX_SEMISPACE_SIZE)
int Heap::max_semispace_size_ = V8_MAX_SEMISPACE_SIZE;
#else
int Heap::max_semispace_size_ = default_max_semispace_size_;
#endif

// The snapshot semispace size will be the default semispace size if
// snapshotting is used and will be the requested semispace size as
// set up by ConfigureHeap otherwise.
int Heap::reserved_semispace_size_ = Heap::max_semispace_size_;

List<Heap::GCPrologueCallbackPair> Heap::gc_prologue_callbacks_;
List<Heap::GCEpilogueCallbackPair> Heap::gc_epilogue_callbacks_;

GCCallback Heap::global_gc_prologue_callback_ = NULL;
GCCallback Heap::global_gc_epilogue_callback_ = NULL;
HeapObjectCallback Heap::gc_safe_size_of_old_object_ = NULL;

// Variables set based on semispace_size_ and old_generation_size_ in
// ConfigureHeap.

// Will be 4 * reserved_semispace_size_ to ensure that young
// generation can be aligned to its size.
int Heap::survived_since_last_expansion_ = 0;
intptr_t Heap::external_allocation_limit_ = 0;

Heap::HeapState Heap::gc_state_ = NOT_IN_GC;

int Heap::mc_count_ = 0;
int Heap::ms_count_ = 0;
unsigned int Heap::gc_count_ = 0;

GCTracer* Heap::tracer_ = NULL;

int Heap::unflattened_strings_length_ = 0;

int Heap::always_allocate_scope_depth_ = 0;
int Heap::linear_allocation_scope_depth_ = 0;
int Heap::contexts_disposed_ = 0;

int Heap::young_survivors_after_last_gc_ = 0;
int Heap::high_survival_rate_period_length_ = 0;
double Heap::survival_rate_ = 0;
Heap::SurvivalRateTrend Heap::previous_survival_rate_trend_ = Heap::STABLE;
Heap::SurvivalRateTrend Heap::survival_rate_trend_ = Heap::STABLE;

#ifdef DEBUG
bool Heap::allocation_allowed_ = true;

int Heap::allocation_timeout_ = 0;
bool Heap::disallow_allocation_failure_ = false;
#endif  // DEBUG

intptr_t GCTracer::alive_after_last_gc_ = 0;
double GCTracer::last_gc_end_timestamp_ = 0.0;
int GCTracer::max_gc_pause_ = 0;
intptr_t GCTracer::max_alive_after_gc_ = 0;
int GCTracer::min_in_mutator_ = kMaxInt;

intptr_t Heap::Capacity() {
  if (!HasBeenSetup()) return 0;

  return new_space_.Capacity() +
      old_pointer_space_->Capacity() +
      old_data_space_->Capacity() +
      code_space_->Capacity() +
      map_space_->Capacity() +
      cell_space_->Capacity();
}


intptr_t Heap::CommittedMemory() {
  if (!HasBeenSetup()) return 0;

  return new_space_.CommittedMemory() +
      old_pointer_space_->CommittedMemory() +
      old_data_space_->CommittedMemory() +
      code_space_->CommittedMemory() +
      map_space_->CommittedMemory() +
      cell_space_->CommittedMemory() +
      lo_space_->Size();
}

intptr_t Heap::CommittedMemoryExecutable() {
  if (!HasBeenSetup()) return 0;

  return MemoryAllocator::SizeExecutable();
}


intptr_t Heap::Available() {
  if (!HasBeenSetup()) return 0;

  return new_space_.Available() +
      old_pointer_space_->Available() +
      old_data_space_->Available() +
      code_space_->Available() +
      map_space_->Available() +
      cell_space_->Available();
}


bool Heap::HasBeenSetup() {
  return old_pointer_space_ != NULL &&
         old_data_space_ != NULL &&
         code_space_ != NULL &&
         map_space_ != NULL &&
         cell_space_ != NULL &&
         lo_space_ != NULL;
}


int Heap::GcSafeSizeOfOldObject(HeapObject* object) {
  ASSERT(!Heap::InNewSpace(object));  // Code only works for old objects.
  ASSERT(!MarkCompactCollector::are_map_pointers_encoded());
  MapWord map_word = object->map_word();
  map_word.ClearMark();
  map_word.ClearOverflow();
  return object->SizeFromMap(map_word.ToMap());
}


int Heap::GcSafeSizeOfOldObjectWithEncodedMap(HeapObject* object) {
  ASSERT(!Heap::InNewSpace(object));  // Code only works for old objects.
  ASSERT(MarkCompactCollector::are_map_pointers_encoded());
  uint32_t marker = Memory::uint32_at(object->address());
  if (marker == MarkCompactCollector::kSingleFreeEncoding) {
    return kIntSize;
  } else if (marker == MarkCompactCollector::kMultiFreeEncoding) {
    return Memory::int_at(object->address() + kIntSize);
  } else {
    MapWord map_word = object->map_word();
    Address map_address = map_word.DecodeMapAddress(Heap::map_space());
    Map* map = reinterpret_cast<Map*>(HeapObject::FromAddress(map_address));
    return object->SizeFromMap(map);
  }
}


GarbageCollector Heap::SelectGarbageCollector(AllocationSpace space) {
  // Is global GC requested?
  if (space != NEW_SPACE || FLAG_gc_global) {
    Counters::gc_compactor_caused_by_request.Increment();
    return MARK_COMPACTOR;
  }

  // Is enough data promoted to justify a global GC?
  if (OldGenerationPromotionLimitReached()) {
    Counters::gc_compactor_caused_by_promoted_data.Increment();
    return MARK_COMPACTOR;
  }

  // Have allocation in OLD and LO failed?
  if (old_gen_exhausted_) {
    Counters::gc_compactor_caused_by_oldspace_exhaustion.Increment();
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
  if (MemoryAllocator::MaxAvailable() <= new_space_.Size()) {
    Counters::gc_compactor_caused_by_oldspace_exhaustion.Increment();
    return MARK_COMPACTOR;
  }

  // Default
  return SCAVENGER;
}


// TODO(1238405): Combine the infrastructure for --heap-stats and
// --log-gc to avoid the complicated preprocessor and flag testing.
#if defined(DEBUG) || defined(ENABLE_LOGGING_AND_PROFILING)
void Heap::ReportStatisticsBeforeGC() {
  // Heap::ReportHeapStatistics will also log NewSpace statistics when
  // compiled with ENABLE_LOGGING_AND_PROFILING and --log-gc is set.  The
  // following logic is used to avoid double logging.
#if defined(DEBUG) && defined(ENABLE_LOGGING_AND_PROFILING)
  if (FLAG_heap_stats || FLAG_log_gc) new_space_.CollectStatistics();
  if (FLAG_heap_stats) {
    ReportHeapStatistics("Before GC");
  } else if (FLAG_log_gc) {
    new_space_.ReportStatistics();
  }
  if (FLAG_heap_stats || FLAG_log_gc) new_space_.ClearHistograms();
#elif defined(DEBUG)
  if (FLAG_heap_stats) {
    new_space_.CollectStatistics();
    ReportHeapStatistics("Before GC");
    new_space_.ClearHistograms();
  }
#elif defined(ENABLE_LOGGING_AND_PROFILING)
  if (FLAG_log_gc) {
    new_space_.CollectStatistics();
    new_space_.ReportStatistics();
    new_space_.ClearHistograms();
  }
#endif
}


#if defined(ENABLE_LOGGING_AND_PROFILING)
void Heap::PrintShortHeapStatistics() {
  if (!FLAG_trace_gc_verbose) return;
  PrintF("Memory allocator,   used: %8" V8_PTR_PREFIX "d"
             ", available: %8" V8_PTR_PREFIX "d\n",
         MemoryAllocator::Size(),
         MemoryAllocator::Available());
  PrintF("New space,          used: %8" V8_PTR_PREFIX "d"
             ", available: %8" V8_PTR_PREFIX "d\n",
         Heap::new_space_.Size(),
         new_space_.Available());
  PrintF("Old pointers,       used: %8" V8_PTR_PREFIX "d"
             ", available: %8" V8_PTR_PREFIX "d"
             ", waste: %8" V8_PTR_PREFIX "d\n",
         old_pointer_space_->Size(),
         old_pointer_space_->Available(),
         old_pointer_space_->Waste());
  PrintF("Old data space,     used: %8" V8_PTR_PREFIX "d"
             ", available: %8" V8_PTR_PREFIX "d"
             ", waste: %8" V8_PTR_PREFIX "d\n",
         old_data_space_->Size(),
         old_data_space_->Available(),
         old_data_space_->Waste());
  PrintF("Code space,         used: %8" V8_PTR_PREFIX "d"
             ", available: %8" V8_PTR_PREFIX "d"
             ", waste: %8" V8_PTR_PREFIX "d\n",
         code_space_->Size(),
         code_space_->Available(),
         code_space_->Waste());
  PrintF("Map space,          used: %8" V8_PTR_PREFIX "d"
             ", available: %8" V8_PTR_PREFIX "d"
             ", waste: %8" V8_PTR_PREFIX "d\n",
         map_space_->Size(),
         map_space_->Available(),
         map_space_->Waste());
  PrintF("Cell space,         used: %8" V8_PTR_PREFIX "d"
             ", available: %8" V8_PTR_PREFIX "d"
             ", waste: %8" V8_PTR_PREFIX "d\n",
         cell_space_->Size(),
         cell_space_->Available(),
         cell_space_->Waste());
  PrintF("Large object space, used: %8" V8_PTR_PREFIX "d"
             ", available: %8" V8_PTR_PREFIX "d\n",
         lo_space_->Size(),
         lo_space_->Available());
}
#endif


// TODO(1238405): Combine the infrastructure for --heap-stats and
// --log-gc to avoid the complicated preprocessor and flag testing.
void Heap::ReportStatisticsAfterGC() {
  // Similar to the before GC, we use some complicated logic to ensure that
  // NewSpace statistics are logged exactly once when --log-gc is turned on.
#if defined(DEBUG) && defined(ENABLE_LOGGING_AND_PROFILING)
  if (FLAG_heap_stats) {
    new_space_.CollectStatistics();
    ReportHeapStatistics("After GC");
  } else if (FLAG_log_gc) {
    new_space_.ReportStatistics();
  }
#elif defined(DEBUG)
  if (FLAG_heap_stats) ReportHeapStatistics("After GC");
#elif defined(ENABLE_LOGGING_AND_PROFILING)
  if (FLAG_log_gc) new_space_.ReportStatistics();
#endif
}
#endif  // defined(DEBUG) || defined(ENABLE_LOGGING_AND_PROFILING)


void Heap::GarbageCollectionPrologue() {
  TranscendentalCache::Clear();
  ClearJSFunctionResultCaches();
  gc_count_++;
  unflattened_strings_length_ = 0;
#ifdef DEBUG
  ASSERT(allocation_allowed_ && gc_state_ == NOT_IN_GC);
  allow_allocation(false);

  if (FLAG_verify_heap) {
    Verify();
  }

  if (FLAG_gc_verbose) Print();
#endif

#if defined(DEBUG) || defined(ENABLE_LOGGING_AND_PROFILING)
  ReportStatisticsBeforeGC();
#endif

  LiveObjectList::GCPrologue();
}

intptr_t Heap::SizeOfObjects() {
  intptr_t total = 0;
  AllSpaces spaces;
  for (Space* space = spaces.next(); space != NULL; space = spaces.next()) {
    total += space->SizeOfObjects();
  }
  return total;
}

void Heap::GarbageCollectionEpilogue() {
  LiveObjectList::GCEpilogue();
#ifdef DEBUG
  allow_allocation(true);
  ZapFromSpace();

  if (FLAG_verify_heap) {
    Verify();
  }

  if (FLAG_print_global_handles) GlobalHandles::Print();
  if (FLAG_print_handles) PrintHandles();
  if (FLAG_gc_verbose) Print();
  if (FLAG_code_stats) ReportCodeStatistics("After GC");
#endif

  Counters::alive_after_last_gc.Set(static_cast<int>(SizeOfObjects()));

  Counters::symbol_table_capacity.Set(symbol_table()->Capacity());
  Counters::number_of_symbols.Set(symbol_table()->NumberOfElements());
#if defined(DEBUG) || defined(ENABLE_LOGGING_AND_PROFILING)
  ReportStatisticsAfterGC();
#endif
#ifdef ENABLE_DEBUGGER_SUPPORT
  Debug::AfterGarbageCollection();
#endif
}


void Heap::CollectAllGarbage(bool force_compaction) {
  // Since we are ignoring the return value, the exact choice of space does
  // not matter, so long as we do not specify NEW_SPACE, which would not
  // cause a full GC.
  MarkCompactCollector::SetForceCompaction(force_compaction);
  CollectGarbage(OLD_POINTER_SPACE);
  MarkCompactCollector::SetForceCompaction(false);
}


void Heap::CollectAllAvailableGarbage() {
  // Since we are ignoring the return value, the exact choice of space does
  // not matter, so long as we do not specify NEW_SPACE, which would not
  // cause a full GC.
  MarkCompactCollector::SetForceCompaction(true);

  // Major GC would invoke weak handle callbacks on weakly reachable
  // handles, but won't collect weakly reachable objects until next
  // major GC.  Therefore if we collect aggressively and weak handle callback
  // has been invoked, we rerun major GC to release objects which become
  // garbage.
  // Note: as weak callbacks can execute arbitrary code, we cannot
  // hope that eventually there will be no weak callbacks invocations.
  // Therefore stop recollecting after several attempts.
  const int kMaxNumberOfAttempts = 7;
  for (int attempt = 0; attempt < kMaxNumberOfAttempts; attempt++) {
    if (!CollectGarbage(OLD_POINTER_SPACE, MARK_COMPACTOR)) {
      break;
    }
  }
  MarkCompactCollector::SetForceCompaction(false);
}


bool Heap::CollectGarbage(AllocationSpace space, GarbageCollector collector) {
  // The VM is in the GC state until exiting this function.
  VMState state(GC);

#ifdef DEBUG
  // Reset the allocation timeout to the GC interval, but make sure to
  // allow at least a few allocations after a collection. The reason
  // for this is that we have a lot of allocation sequences and we
  // assume that a garbage collection will allow the subsequent
  // allocation attempts to go through.
  allocation_timeout_ = Max(6, FLAG_gc_interval);
#endif

  bool next_gc_likely_to_collect_more = false;

  { GCTracer tracer;
    GarbageCollectionPrologue();
    // The GC count was incremented in the prologue.  Tell the tracer about
    // it.
    tracer.set_gc_count(gc_count_);

    // Tell the tracer which collector we've selected.
    tracer.set_collector(collector);

    HistogramTimer* rate = (collector == SCAVENGER)
        ? &Counters::gc_scavenger
        : &Counters::gc_compactor;
    rate->Start();
    next_gc_likely_to_collect_more =
        PerformGarbageCollection(collector, &tracer);
    rate->Stop();

    GarbageCollectionEpilogue();
  }


#ifdef ENABLE_LOGGING_AND_PROFILING
  if (FLAG_log_gc) HeapProfiler::WriteSample();
#endif

  return next_gc_likely_to_collect_more;
}


void Heap::PerformScavenge() {
  GCTracer tracer;
  PerformGarbageCollection(SCAVENGER, &tracer);
}


#ifdef DEBUG
// Helper class for verifying the symbol table.
class SymbolTableVerifier : public ObjectVisitor {
 public:
  SymbolTableVerifier() { }
  void VisitPointers(Object** start, Object** end) {
    // Visit all HeapObject pointers in [start, end).
    for (Object** p = start; p < end; p++) {
      if ((*p)->IsHeapObject()) {
        // Check that the symbol is actually a symbol.
        ASSERT((*p)->IsNull() || (*p)->IsUndefined() || (*p)->IsSymbol());
      }
    }
  }
};
#endif  // DEBUG


static void VerifySymbolTable() {
#ifdef DEBUG
  SymbolTableVerifier verifier;
  Heap::symbol_table()->IterateElements(&verifier);
#endif  // DEBUG
}


void Heap::ReserveSpace(
    int new_space_size,
    int pointer_space_size,
    int data_space_size,
    int code_space_size,
    int map_space_size,
    int cell_space_size,
    int large_object_size) {
  NewSpace* new_space = Heap::new_space();
  PagedSpace* old_pointer_space = Heap::old_pointer_space();
  PagedSpace* old_data_space = Heap::old_data_space();
  PagedSpace* code_space = Heap::code_space();
  PagedSpace* map_space = Heap::map_space();
  PagedSpace* cell_space = Heap::cell_space();
  LargeObjectSpace* lo_space = Heap::lo_space();
  bool gc_performed = true;
  while (gc_performed) {
    gc_performed = false;
    if (!new_space->ReserveSpace(new_space_size)) {
      Heap::CollectGarbage(NEW_SPACE);
      gc_performed = true;
    }
    if (!old_pointer_space->ReserveSpace(pointer_space_size)) {
      Heap::CollectGarbage(OLD_POINTER_SPACE);
      gc_performed = true;
    }
    if (!(old_data_space->ReserveSpace(data_space_size))) {
      Heap::CollectGarbage(OLD_DATA_SPACE);
      gc_performed = true;
    }
    if (!(code_space->ReserveSpace(code_space_size))) {
      Heap::CollectGarbage(CODE_SPACE);
      gc_performed = true;
    }
    if (!(map_space->ReserveSpace(map_space_size))) {
      Heap::CollectGarbage(MAP_SPACE);
      gc_performed = true;
    }
    if (!(cell_space->ReserveSpace(cell_space_size))) {
      Heap::CollectGarbage(CELL_SPACE);
      gc_performed = true;
    }
    // We add a slack-factor of 2 in order to have space for a series of
    // large-object allocations that are only just larger than the page size.
    large_object_size *= 2;
    // The ReserveSpace method on the large object space checks how much
    // we can expand the old generation.  This includes expansion caused by
    // allocation in the other spaces.
    large_object_size += cell_space_size + map_space_size + code_space_size +
        data_space_size + pointer_space_size;
    if (!(lo_space->ReserveSpace(large_object_size))) {
      Heap::CollectGarbage(LO_SPACE);
      gc_performed = true;
    }
  }
}


void Heap::EnsureFromSpaceIsCommitted() {
  if (new_space_.CommitFromSpaceIfNeeded()) return;

  // Committing memory to from space failed.
  // Try shrinking and try again.
  PagedSpaces spaces;
  for (PagedSpace* space = spaces.next();
       space != NULL;
       space = spaces.next()) {
    space->RelinkPageListInChunkOrder(true);
  }

  Shrink();
  if (new_space_.CommitFromSpaceIfNeeded()) return;

  // Committing memory to from space failed again.
  // Memory is exhausted and we will die.
  V8::FatalProcessOutOfMemory("Committing semi space failed.");
}


void Heap::ClearJSFunctionResultCaches() {
  if (Bootstrapper::IsActive()) return;

  Object* context = global_contexts_list_;
  while (!context->IsUndefined()) {
    // Get the caches for this context:
    FixedArray* caches =
      Context::cast(context)->jsfunction_result_caches();
    // Clear the caches:
    int length = caches->length();
    for (int i = 0; i < length; i++) {
      JSFunctionResultCache::cast(caches->get(i))->Clear();
    }
    // Get the next context:
    context = Context::cast(context)->get(Context::NEXT_CONTEXT_LINK);
  }
}


void Heap::ClearNormalizedMapCaches() {
  if (Bootstrapper::IsActive()) return;

  Object* context = global_contexts_list_;
  while (!context->IsUndefined()) {
    Context::cast(context)->normalized_map_cache()->Clear();
    context = Context::cast(context)->get(Context::NEXT_CONTEXT_LINK);
  }
}


#ifdef DEBUG

enum PageWatermarkValidity {
  ALL_VALID,
  ALL_INVALID
};

static void VerifyPageWatermarkValidity(PagedSpace* space,
                                        PageWatermarkValidity validity) {
  PageIterator it(space, PageIterator::PAGES_IN_USE);
  bool expected_value = (validity == ALL_VALID);
  while (it.has_next()) {
    Page* page = it.next();
    ASSERT(page->IsWatermarkValid() == expected_value);
  }
}
#endif

void Heap::UpdateSurvivalRateTrend(int start_new_space_size) {
  double survival_rate =
      (static_cast<double>(young_survivors_after_last_gc_) * 100) /
      start_new_space_size;

  if (survival_rate > kYoungSurvivalRateThreshold) {
    high_survival_rate_period_length_++;
  } else {
    high_survival_rate_period_length_ = 0;
  }

  double survival_rate_diff = survival_rate_ - survival_rate;

  if (survival_rate_diff > kYoungSurvivalRateAllowedDeviation) {
    set_survival_rate_trend(DECREASING);
  } else if (survival_rate_diff < -kYoungSurvivalRateAllowedDeviation) {
    set_survival_rate_trend(INCREASING);
  } else {
    set_survival_rate_trend(STABLE);
  }

  survival_rate_ = survival_rate;
}

bool Heap::PerformGarbageCollection(GarbageCollector collector,
                                    GCTracer* tracer) {
  bool next_gc_likely_to_collect_more = false;

  if (collector != SCAVENGER) {
    PROFILE(CodeMovingGCEvent());
  }

  VerifySymbolTable();
  if (collector == MARK_COMPACTOR && global_gc_prologue_callback_) {
    ASSERT(!allocation_allowed_);
    GCTracer::Scope scope(tracer, GCTracer::Scope::EXTERNAL);
    global_gc_prologue_callback_();
  }

  GCType gc_type =
      collector == MARK_COMPACTOR ? kGCTypeMarkSweepCompact : kGCTypeScavenge;

  for (int i = 0; i < gc_prologue_callbacks_.length(); ++i) {
    if (gc_type & gc_prologue_callbacks_[i].gc_type) {
      gc_prologue_callbacks_[i].callback(gc_type, kNoGCCallbackFlags);
    }
  }

  EnsureFromSpaceIsCommitted();

  int start_new_space_size = Heap::new_space()->SizeAsInt();

  if (collector == MARK_COMPACTOR) {
    // Perform mark-sweep with optional compaction.
    MarkCompact(tracer);

    bool high_survival_rate_during_scavenges = IsHighSurvivalRate() &&
        IsStableOrIncreasingSurvivalTrend();

    UpdateSurvivalRateTrend(start_new_space_size);

    intptr_t old_gen_size = PromotedSpaceSize();
    old_gen_promotion_limit_ =
        old_gen_size + Max(kMinimumPromotionLimit, old_gen_size / 3);
    old_gen_allocation_limit_ =
        old_gen_size + Max(kMinimumAllocationLimit, old_gen_size / 2);

    if (high_survival_rate_during_scavenges &&
        IsStableOrIncreasingSurvivalTrend()) {
      // Stable high survival rates of young objects both during partial and
      // full collection indicate that mutator is either building or modifying
      // a structure with a long lifetime.
      // In this case we aggressively raise old generation memory limits to
      // postpone subsequent mark-sweep collection and thus trade memory
      // space for the mutation speed.
      old_gen_promotion_limit_ *= 2;
      old_gen_allocation_limit_ *= 2;
    }

    old_gen_exhausted_ = false;
  } else {
    tracer_ = tracer;
    Scavenge();
    tracer_ = NULL;

    UpdateSurvivalRateTrend(start_new_space_size);
  }

  Counters::objs_since_last_young.Set(0);

  if (collector == MARK_COMPACTOR) {
    DisableAssertNoAllocation allow_allocation;
    GCTracer::Scope scope(tracer, GCTracer::Scope::EXTERNAL);
    next_gc_likely_to_collect_more =
        GlobalHandles::PostGarbageCollectionProcessing();
  }

  // Update relocatables.
  Relocatable::PostGarbageCollectionProcessing();

  if (collector == MARK_COMPACTOR) {
    // Register the amount of external allocated memory.
    amount_of_external_allocated_memory_at_last_global_gc_ =
        amount_of_external_allocated_memory_;
  }

  GCCallbackFlags callback_flags = tracer->is_compacting()
      ? kGCCallbackFlagCompacted
      : kNoGCCallbackFlags;
  for (int i = 0; i < gc_epilogue_callbacks_.length(); ++i) {
    if (gc_type & gc_epilogue_callbacks_[i].gc_type) {
      gc_epilogue_callbacks_[i].callback(gc_type, callback_flags);
    }
  }

  if (collector == MARK_COMPACTOR && global_gc_epilogue_callback_) {
    ASSERT(!allocation_allowed_);
    GCTracer::Scope scope(tracer, GCTracer::Scope::EXTERNAL);
    global_gc_epilogue_callback_();
  }
  VerifySymbolTable();

  return next_gc_likely_to_collect_more;
}


void Heap::MarkCompact(GCTracer* tracer) {
  gc_state_ = MARK_COMPACT;
  LOG(ResourceEvent("markcompact", "begin"));

  MarkCompactCollector::Prepare(tracer);

  bool is_compacting = MarkCompactCollector::IsCompacting();

  if (is_compacting) {
    mc_count_++;
  } else {
    ms_count_++;
  }
  tracer->set_full_gc_count(mc_count_ + ms_count_);

  MarkCompactPrologue(is_compacting);

  MarkCompactCollector::CollectGarbage();

  LOG(ResourceEvent("markcompact", "end"));

  gc_state_ = NOT_IN_GC;

  Shrink();

  Counters::objs_since_last_full.Set(0);

  contexts_disposed_ = 0;
}


void Heap::MarkCompactPrologue(bool is_compacting) {
  // At any old GC clear the keyed lookup cache to enable collection of unused
  // maps.
  KeyedLookupCache::Clear();
  ContextSlotCache::Clear();
  DescriptorLookupCache::Clear();

  CompilationCache::MarkCompactPrologue();

  CompletelyClearInstanceofCache();

  if (is_compacting) FlushNumberStringCache();

  ClearNormalizedMapCaches();
}


Object* Heap::FindCodeObject(Address a) {
  Object* obj = NULL;  // Initialization to please compiler.
  { MaybeObject* maybe_obj = code_space_->FindObject(a);
    if (!maybe_obj->ToObject(&obj)) {
      obj = lo_space_->FindObject(a)->ToObjectUnchecked();
    }
  }
  return obj;
}


// Helper class for copying HeapObjects
class ScavengeVisitor: public ObjectVisitor {
 public:

  void VisitPointer(Object** p) { ScavengePointer(p); }

  void VisitPointers(Object** start, Object** end) {
    // Copy all HeapObject pointers in [start, end)
    for (Object** p = start; p < end; p++) ScavengePointer(p);
  }

 private:
  void ScavengePointer(Object** p) {
    Object* object = *p;
    if (!Heap::InNewSpace(object)) return;
    Heap::ScavengeObject(reinterpret_cast<HeapObject**>(p),
                         reinterpret_cast<HeapObject*>(object));
  }
};


// A queue of objects promoted during scavenge. Each object is accompanied
// by it's size to avoid dereferencing a map pointer for scanning.
class PromotionQueue {
 public:
  void Initialize(Address start_address) {
    front_ = rear_ = reinterpret_cast<intptr_t*>(start_address);
  }

  bool is_empty() { return front_ <= rear_; }

  void insert(HeapObject* target, int size) {
    *(--rear_) = reinterpret_cast<intptr_t>(target);
    *(--rear_) = size;
    // Assert no overflow into live objects.
    ASSERT(reinterpret_cast<Address>(rear_) >= Heap::new_space()->top());
  }

  void remove(HeapObject** target, int* size) {
    *target = reinterpret_cast<HeapObject*>(*(--front_));
    *size = static_cast<int>(*(--front_));
    // Assert no underflow.
    ASSERT(front_ >= rear_);
  }

 private:
  // The front of the queue is higher in memory than the rear.
  intptr_t* front_;
  intptr_t* rear_;
};


// Shared state read by the scavenge collector and set by ScavengeObject.
static PromotionQueue promotion_queue;


#ifdef DEBUG
// Visitor class to verify pointers in code or data space do not point into
// new space.
class VerifyNonPointerSpacePointersVisitor: public ObjectVisitor {
 public:
  void VisitPointers(Object** start, Object**end) {
    for (Object** current = start; current < end; current++) {
      if ((*current)->IsHeapObject()) {
        ASSERT(!Heap::InNewSpace(HeapObject::cast(*current)));
      }
    }
  }
};


static void VerifyNonPointerSpacePointers() {
  // Verify that there are no pointers to new space in spaces where we
  // do not expect them.
  VerifyNonPointerSpacePointersVisitor v;
  HeapObjectIterator code_it(Heap::code_space());
  for (HeapObject* object = code_it.next();
       object != NULL; object = code_it.next())
    object->Iterate(&v);

  HeapObjectIterator data_it(Heap::old_data_space());
  for (HeapObject* object = data_it.next();
       object != NULL; object = data_it.next())
    object->Iterate(&v);
}
#endif


void Heap::CheckNewSpaceExpansionCriteria() {
  if (new_space_.Capacity() < new_space_.MaximumCapacity() &&
      survived_since_last_expansion_ > new_space_.Capacity()) {
    // Grow the size of new space if there is room to grow and enough
    // data has survived scavenge since the last expansion.
    new_space_.Grow();
    survived_since_last_expansion_ = 0;
  }
}


void Heap::Scavenge() {
#ifdef DEBUG
  if (FLAG_enable_slow_asserts) VerifyNonPointerSpacePointers();
#endif

  gc_state_ = SCAVENGE;

  Page::FlipMeaningOfInvalidatedWatermarkFlag();
#ifdef DEBUG
  VerifyPageWatermarkValidity(old_pointer_space_, ALL_VALID);
  VerifyPageWatermarkValidity(map_space_, ALL_VALID);
#endif

  // We do not update an allocation watermark of the top page during linear
  // allocation to avoid overhead. So to maintain the watermark invariant
  // we have to manually cache the watermark and mark the top page as having an
  // invalid watermark. This guarantees that dirty regions iteration will use a
  // correct watermark even if a linear allocation happens.
  old_pointer_space_->FlushTopPageWatermark();
  map_space_->FlushTopPageWatermark();

  // Implements Cheney's copying algorithm
  LOG(ResourceEvent("scavenge", "begin"));

  // Clear descriptor cache.
  DescriptorLookupCache::Clear();

  // Used for updating survived_since_last_expansion_ at function end.
  intptr_t survived_watermark = PromotedSpaceSize();

  CheckNewSpaceExpansionCriteria();

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
  Address new_space_front = new_space_.ToSpaceLow();
  promotion_queue.Initialize(new_space_.ToSpaceHigh());

  ScavengeVisitor scavenge_visitor;
  // Copy roots.
  IterateRoots(&scavenge_visitor, VISIT_ALL_IN_SCAVENGE);

  // Copy objects reachable from the old generation.  By definition,
  // there are no intergenerational pointers in code or data spaces.
  IterateDirtyRegions(old_pointer_space_,
                      &IteratePointersInDirtyRegion,
                      &ScavengePointer,
                      WATERMARK_CAN_BE_INVALID);

  IterateDirtyRegions(map_space_,
                      &IteratePointersInDirtyMapsRegion,
                      &ScavengePointer,
                      WATERMARK_CAN_BE_INVALID);

  lo_space_->IterateDirtyRegions(&ScavengePointer);

  // Copy objects reachable from cells by scavenging cell values directly.
  HeapObjectIterator cell_iterator(cell_space_);
  for (HeapObject* cell = cell_iterator.next();
       cell != NULL; cell = cell_iterator.next()) {
    if (cell->IsJSGlobalPropertyCell()) {
      Address value_address =
          reinterpret_cast<Address>(cell) +
          (JSGlobalPropertyCell::kValueOffset - kHeapObjectTag);
      scavenge_visitor.VisitPointer(reinterpret_cast<Object**>(value_address));
    }
  }

  // Scavenge object reachable from the global contexts list directly.
  scavenge_visitor.VisitPointer(BitCast<Object**>(&global_contexts_list_));

  new_space_front = DoScavenge(&scavenge_visitor, new_space_front);

  UpdateNewSpaceReferencesInExternalStringTable(
      &UpdateNewSpaceReferenceInExternalStringTableEntry);

  LiveObjectList::UpdateReferencesForScavengeGC();
  RuntimeProfiler::UpdateSamplesAfterScavenge();

  ASSERT(new_space_front == new_space_.top());

  // Set age mark.
  new_space_.set_age_mark(new_space_.top());

  // Update how much has survived scavenge.
  IncrementYoungSurvivorsCounter(static_cast<int>(
      (PromotedSpaceSize() - survived_watermark) + new_space_.Size()));

  LOG(ResourceEvent("scavenge", "end"));

  gc_state_ = NOT_IN_GC;
}


String* Heap::UpdateNewSpaceReferenceInExternalStringTableEntry(Object** p) {
  MapWord first_word = HeapObject::cast(*p)->map_word();

  if (!first_word.IsForwardingAddress()) {
    // Unreachable external string can be finalized.
    FinalizeExternalString(String::cast(*p));
    return NULL;
  }

  // String is still reachable.
  return String::cast(first_word.ToForwardingAddress());
}


void Heap::UpdateNewSpaceReferencesInExternalStringTable(
    ExternalStringTableUpdaterCallback updater_func) {
  ExternalStringTable::Verify();

  if (ExternalStringTable::new_space_strings_.is_empty()) return;

  Object** start = &ExternalStringTable::new_space_strings_[0];
  Object** end = start + ExternalStringTable::new_space_strings_.length();
  Object** last = start;

  for (Object** p = start; p < end; ++p) {
    ASSERT(Heap::InFromSpace(*p));
    String* target = updater_func(p);

    if (target == NULL) continue;

    ASSERT(target->IsExternalString());

    if (Heap::InNewSpace(target)) {
      // String is still in new space.  Update the table entry.
      *last = target;
      ++last;
    } else {
      // String got promoted.  Move it to the old string list.
      ExternalStringTable::AddOldString(target);
    }
  }

  ASSERT(last <= end);
  ExternalStringTable::ShrinkNewStrings(static_cast<int>(last - start));
}


static Object* ProcessFunctionWeakReferences(Object* function,
                                             WeakObjectRetainer* retainer) {
  Object* head = Heap::undefined_value();
  JSFunction* tail = NULL;
  Object* candidate = function;
  while (!candidate->IsUndefined()) {
    // Check whether to keep the candidate in the list.
    JSFunction* candidate_function = reinterpret_cast<JSFunction*>(candidate);
    Object* retain = retainer->RetainAs(candidate);
    if (retain != NULL) {
      if (head->IsUndefined()) {
        // First element in the list.
        head = candidate_function;
      } else {
        // Subsequent elements in the list.
        ASSERT(tail != NULL);
        tail->set_next_function_link(candidate_function);
      }
      // Retained function is new tail.
      tail = candidate_function;
    }
    // Move to next element in the list.
    candidate = candidate_function->next_function_link();
  }

  // Terminate the list if there is one or more elements.
  if (tail != NULL) {
    tail->set_next_function_link(Heap::undefined_value());
  }

  return head;
}


void Heap::ProcessWeakReferences(WeakObjectRetainer* retainer) {
  Object* head = undefined_value();
  Context* tail = NULL;
  Object* candidate = global_contexts_list_;
  while (!candidate->IsUndefined()) {
    // Check whether to keep the candidate in the list.
    Context* candidate_context = reinterpret_cast<Context*>(candidate);
    Object* retain = retainer->RetainAs(candidate);
    if (retain != NULL) {
      if (head->IsUndefined()) {
        // First element in the list.
        head = candidate_context;
      } else {
        // Subsequent elements in the list.
        ASSERT(tail != NULL);
        tail->set_unchecked(Context::NEXT_CONTEXT_LINK,
                            candidate_context,
                            UPDATE_WRITE_BARRIER);
      }
      // Retained context is new tail.
      tail = candidate_context;

      // Process the weak list of optimized functions for the context.
      Object* function_list_head =
          ProcessFunctionWeakReferences(
              candidate_context->get(Context::OPTIMIZED_FUNCTIONS_LIST),
              retainer);
      candidate_context->set_unchecked(Context::OPTIMIZED_FUNCTIONS_LIST,
                                       function_list_head,
                                       UPDATE_WRITE_BARRIER);
    }
    // Move to next element in the list.
    candidate = candidate_context->get(Context::NEXT_CONTEXT_LINK);
  }

  // Terminate the list if there is one or more elements.
  if (tail != NULL) {
    tail->set_unchecked(Context::NEXT_CONTEXT_LINK,
                        Heap::undefined_value(),
                        UPDATE_WRITE_BARRIER);
  }

  // Update the head of the list of contexts.
  Heap::global_contexts_list_ = head;
}


class NewSpaceScavenger : public StaticNewSpaceVisitor<NewSpaceScavenger> {
 public:
  static inline void VisitPointer(Object** p) {
    Object* object = *p;
    if (!Heap::InNewSpace(object)) return;
    Heap::ScavengeObject(reinterpret_cast<HeapObject**>(p),
                         reinterpret_cast<HeapObject*>(object));
  }
};


Address Heap::DoScavenge(ObjectVisitor* scavenge_visitor,
                         Address new_space_front) {
  do {
    ASSERT(new_space_front <= new_space_.top());

    // The addresses new_space_front and new_space_.top() define a
    // queue of unprocessed copied objects.  Process them until the
    // queue is empty.
    while (new_space_front < new_space_.top()) {
      HeapObject* object = HeapObject::FromAddress(new_space_front);
      new_space_front += NewSpaceScavenger::IterateBody(object->map(), object);
    }

    // Promote and process all the to-be-promoted objects.
    while (!promotion_queue.is_empty()) {
      HeapObject* target;
      int size;
      promotion_queue.remove(&target, &size);

      // Promoted object might be already partially visited
      // during dirty regions iteration. Thus we search specificly
      // for pointers to from semispace instead of looking for pointers
      // to new space.
      ASSERT(!target->IsMap());
      IterateAndMarkPointersToFromSpace(target->address(),
                                        target->address() + size,
                                        &ScavengePointer);
    }

    // Take another spin if there are now unswept objects in new space
    // (there are currently no more unswept promoted objects).
  } while (new_space_front < new_space_.top());

  return new_space_front;
}


class ScavengingVisitor : public StaticVisitorBase {
 public:
  static void Initialize() {
    table_.Register(kVisitSeqAsciiString, &EvacuateSeqAsciiString);
    table_.Register(kVisitSeqTwoByteString, &EvacuateSeqTwoByteString);
    table_.Register(kVisitShortcutCandidate, &EvacuateShortcutCandidate);
    table_.Register(kVisitByteArray, &EvacuateByteArray);
    table_.Register(kVisitFixedArray, &EvacuateFixedArray);
    table_.Register(kVisitGlobalContext,
                    &ObjectEvacuationStrategy<POINTER_OBJECT>::
                        VisitSpecialized<Context::kSize>);

    typedef ObjectEvacuationStrategy<POINTER_OBJECT> PointerObject;

    table_.Register(kVisitConsString,
                    &ObjectEvacuationStrategy<POINTER_OBJECT>::
                        VisitSpecialized<ConsString::kSize>);

    table_.Register(kVisitSharedFunctionInfo,
                    &ObjectEvacuationStrategy<POINTER_OBJECT>::
                        VisitSpecialized<SharedFunctionInfo::kSize>);

    table_.Register(kVisitJSFunction,
                    &ObjectEvacuationStrategy<POINTER_OBJECT>::
                    VisitSpecialized<JSFunction::kSize>);

    table_.RegisterSpecializations<ObjectEvacuationStrategy<DATA_OBJECT>,
                                   kVisitDataObject,
                                   kVisitDataObjectGeneric>();

    table_.RegisterSpecializations<ObjectEvacuationStrategy<POINTER_OBJECT>,
                                   kVisitJSObject,
                                   kVisitJSObjectGeneric>();

    table_.RegisterSpecializations<ObjectEvacuationStrategy<POINTER_OBJECT>,
                                   kVisitStruct,
                                   kVisitStructGeneric>();
  }


  static inline void Scavenge(Map* map, HeapObject** slot, HeapObject* obj) {
    table_.GetVisitor(map)(map, slot, obj);
  }


 private:
  enum ObjectContents  { DATA_OBJECT, POINTER_OBJECT };
  enum SizeRestriction { SMALL, UNKNOWN_SIZE };

#if defined(DEBUG) || defined(ENABLE_LOGGING_AND_PROFILING)
  static void RecordCopiedObject(HeapObject* obj) {
    bool should_record = false;
#ifdef DEBUG
    should_record = FLAG_heap_stats;
#endif
#ifdef ENABLE_LOGGING_AND_PROFILING
    should_record = should_record || FLAG_log_gc;
#endif
    if (should_record) {
      if (Heap::new_space()->Contains(obj)) {
        Heap::new_space()->RecordAllocation(obj);
      } else {
        Heap::new_space()->RecordPromotion(obj);
      }
    }
  }
#endif  // defined(DEBUG) || defined(ENABLE_LOGGING_AND_PROFILING)

  // Helper function used by CopyObject to copy a source object to an
  // allocated target object and update the forwarding pointer in the source
  // object.  Returns the target object.
  INLINE(static HeapObject* MigrateObject(HeapObject* source,
                                          HeapObject* target,
                                          int size)) {
    // Copy the content of source to target.
    Heap::CopyBlock(target->address(), source->address(), size);

    // Set the forwarding address.
    source->set_map_word(MapWord::FromForwardingAddress(target));

#if defined(DEBUG) || defined(ENABLE_LOGGING_AND_PROFILING)
    // Update NewSpace stats if necessary.
    RecordCopiedObject(target);
#endif
    HEAP_PROFILE(ObjectMoveEvent(source->address(), target->address()));
#if defined(ENABLE_LOGGING_AND_PROFILING)
    if (Logger::is_logging() || CpuProfiler::is_profiling()) {
      if (target->IsSharedFunctionInfo()) {
        PROFILE(SFIMoveEvent(source->address(), target->address()));
      }
    }
#endif
    return target;
  }


  template<ObjectContents object_contents, SizeRestriction size_restriction>
  static inline void EvacuateObject(Map* map,
                                    HeapObject** slot,
                                    HeapObject* object,
                                    int object_size) {
    ASSERT((size_restriction != SMALL) ||
           (object_size <= Page::kMaxHeapObjectSize));
    ASSERT(object->Size() == object_size);

    if (Heap::ShouldBePromoted(object->address(), object_size)) {
      MaybeObject* maybe_result;

      if ((size_restriction != SMALL) &&
          (object_size > Page::kMaxHeapObjectSize)) {
        maybe_result = Heap::lo_space()->AllocateRawFixedArray(object_size);
      } else {
        if (object_contents == DATA_OBJECT) {
          maybe_result = Heap::old_data_space()->AllocateRaw(object_size);
        } else {
          maybe_result = Heap::old_pointer_space()->AllocateRaw(object_size);
        }
      }

      Object* result = NULL;  // Initialization to please compiler.
      if (maybe_result->ToObject(&result)) {
        HeapObject* target = HeapObject::cast(result);
        *slot = MigrateObject(object, target, object_size);

        if (object_contents == POINTER_OBJECT) {
          promotion_queue.insert(target, object_size);
        }

        Heap::tracer()->increment_promoted_objects_size(object_size);
        return;
      }
    }
    Object* result =
        Heap::new_space()->AllocateRaw(object_size)->ToObjectUnchecked();
    *slot = MigrateObject(object, HeapObject::cast(result), object_size);
    return;
  }


  static inline void EvacuateFixedArray(Map* map,
                                        HeapObject** slot,
                                        HeapObject* object) {
    int object_size = FixedArray::BodyDescriptor::SizeOf(map, object);
    EvacuateObject<POINTER_OBJECT, UNKNOWN_SIZE>(map,
                                                 slot,
                                                 object,
                                                 object_size);
  }


  static inline void EvacuateByteArray(Map* map,
                                       HeapObject** slot,
                                       HeapObject* object) {
    int object_size = reinterpret_cast<ByteArray*>(object)->ByteArraySize();
    EvacuateObject<DATA_OBJECT, UNKNOWN_SIZE>(map, slot, object, object_size);
  }


  static inline void EvacuateSeqAsciiString(Map* map,
                                            HeapObject** slot,
                                            HeapObject* object) {
    int object_size = SeqAsciiString::cast(object)->
        SeqAsciiStringSize(map->instance_type());
    EvacuateObject<DATA_OBJECT, UNKNOWN_SIZE>(map, slot, object, object_size);
  }


  static inline void EvacuateSeqTwoByteString(Map* map,
                                              HeapObject** slot,
                                              HeapObject* object) {
    int object_size = SeqTwoByteString::cast(object)->
        SeqTwoByteStringSize(map->instance_type());
    EvacuateObject<DATA_OBJECT, UNKNOWN_SIZE>(map, slot, object, object_size);
  }


  static inline bool IsShortcutCandidate(int type) {
    return ((type & kShortcutTypeMask) == kShortcutTypeTag);
  }

  static inline void EvacuateShortcutCandidate(Map* map,
                                               HeapObject** slot,
                                               HeapObject* object) {
    ASSERT(IsShortcutCandidate(map->instance_type()));

    if (ConsString::cast(object)->unchecked_second() == Heap::empty_string()) {
      HeapObject* first =
          HeapObject::cast(ConsString::cast(object)->unchecked_first());

      *slot = first;

      if (!Heap::InNewSpace(first)) {
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

      Scavenge(first->map(), slot, first);
      object->set_map_word(MapWord::FromForwardingAddress(*slot));
      return;
    }

    int object_size = ConsString::kSize;
    EvacuateObject<POINTER_OBJECT, SMALL>(map, slot, object, object_size);
  }

  template<ObjectContents object_contents>
  class ObjectEvacuationStrategy {
   public:
    template<int object_size>
    static inline void VisitSpecialized(Map* map,
                                        HeapObject** slot,
                                        HeapObject* object) {
      EvacuateObject<object_contents, SMALL>(map, slot, object, object_size);
    }

    static inline void Visit(Map* map,
                             HeapObject** slot,
                             HeapObject* object) {
      int object_size = map->instance_size();
      EvacuateObject<object_contents, SMALL>(map, slot, object, object_size);
    }
  };

  typedef void (*Callback)(Map* map, HeapObject** slot, HeapObject* object);

  static VisitorDispatchTable<Callback> table_;
};


VisitorDispatchTable<ScavengingVisitor::Callback> ScavengingVisitor::table_;


void Heap::ScavengeObjectSlow(HeapObject** p, HeapObject* object) {
  ASSERT(InFromSpace(object));
  MapWord first_word = object->map_word();
  ASSERT(!first_word.IsForwardingAddress());
  Map* map = first_word.ToMap();
  ScavengingVisitor::Scavenge(map, p, object);
}


void Heap::ScavengePointer(HeapObject** p) {
  ScavengeObject(p, *p);
}


MaybeObject* Heap::AllocatePartialMap(InstanceType instance_type,
                                      int instance_size) {
  Object* result;
  { MaybeObject* maybe_result = AllocateRawMap();
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }

  // Map::cast cannot be used due to uninitialized map field.
  reinterpret_cast<Map*>(result)->set_map(raw_unchecked_meta_map());
  reinterpret_cast<Map*>(result)->set_instance_type(instance_type);
  reinterpret_cast<Map*>(result)->set_instance_size(instance_size);
  reinterpret_cast<Map*>(result)->
      set_visitor_id(
          StaticVisitorBase::GetVisitorId(instance_type, instance_size));
  reinterpret_cast<Map*>(result)->set_inobject_properties(0);
  reinterpret_cast<Map*>(result)->set_pre_allocated_property_fields(0);
  reinterpret_cast<Map*>(result)->set_unused_property_fields(0);
  reinterpret_cast<Map*>(result)->set_bit_field(0);
  reinterpret_cast<Map*>(result)->set_bit_field2(0);
  return result;
}


MaybeObject* Heap::AllocateMap(InstanceType instance_type, int instance_size) {
  Object* result;
  { MaybeObject* maybe_result = AllocateRawMap();
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }

  Map* map = reinterpret_cast<Map*>(result);
  map->set_map(meta_map());
  map->set_instance_type(instance_type);
  map->set_visitor_id(
      StaticVisitorBase::GetVisitorId(instance_type, instance_size));
  map->set_prototype(null_value());
  map->set_constructor(null_value());
  map->set_instance_size(instance_size);
  map->set_inobject_properties(0);
  map->set_pre_allocated_property_fields(0);
  map->set_instance_descriptors(empty_descriptor_array());
  map->set_code_cache(empty_fixed_array());
  map->set_unused_property_fields(0);
  map->set_bit_field(0);
  map->set_bit_field2((1 << Map::kIsExtensible) | (1 << Map::kHasFastElements));

  // If the map object is aligned fill the padding area with Smi 0 objects.
  if (Map::kPadStart < Map::kSize) {
    memset(reinterpret_cast<byte*>(map) + Map::kPadStart - kHeapObjectTag,
           0,
           Map::kSize - Map::kPadStart);
  }
  return map;
}


MaybeObject* Heap::AllocateCodeCache() {
  Object* result;
  { MaybeObject* maybe_result = AllocateStruct(CODE_CACHE_TYPE);
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }
  CodeCache* code_cache = CodeCache::cast(result);
  code_cache->set_default_cache(empty_fixed_array());
  code_cache->set_normal_type_cache(undefined_value());
  return code_cache;
}


const Heap::StringTypeTable Heap::string_type_table[] = {
#define STRING_TYPE_ELEMENT(type, size, name, camel_name)                      \
  {type, size, k##camel_name##MapRootIndex},
  STRING_TYPE_LIST(STRING_TYPE_ELEMENT)
#undef STRING_TYPE_ELEMENT
};


const Heap::ConstantSymbolTable Heap::constant_symbol_table[] = {
#define CONSTANT_SYMBOL_ELEMENT(name, contents)                                \
  {contents, k##name##RootIndex},
  SYMBOL_LIST(CONSTANT_SYMBOL_ELEMENT)
#undef CONSTANT_SYMBOL_ELEMENT
};


const Heap::StructTable Heap::struct_table[] = {
#define STRUCT_TABLE_ELEMENT(NAME, Name, name)                                 \
  { NAME##_TYPE, Name::kSize, k##Name##MapRootIndex },
  STRUCT_LIST(STRUCT_TABLE_ELEMENT)
#undef STRUCT_TABLE_ELEMENT
};


bool Heap::CreateInitialMaps() {
  Object* obj;
  { MaybeObject* maybe_obj = AllocatePartialMap(MAP_TYPE, Map::kSize);
    if (!maybe_obj->ToObject(&obj)) return false;
  }
  // Map::cast cannot be used due to uninitialized map field.
  Map* new_meta_map = reinterpret_cast<Map*>(obj);
  set_meta_map(new_meta_map);
  new_meta_map->set_map(new_meta_map);

  { MaybeObject* maybe_obj =
        AllocatePartialMap(FIXED_ARRAY_TYPE, kVariableSizeSentinel);
    if (!maybe_obj->ToObject(&obj)) return false;
  }
  set_fixed_array_map(Map::cast(obj));

  { MaybeObject* maybe_obj = AllocatePartialMap(ODDBALL_TYPE, Oddball::kSize);
    if (!maybe_obj->ToObject(&obj)) return false;
  }
  set_oddball_map(Map::cast(obj));

  // Allocate the empty array.
  { MaybeObject* maybe_obj = AllocateEmptyFixedArray();
    if (!maybe_obj->ToObject(&obj)) return false;
  }
  set_empty_fixed_array(FixedArray::cast(obj));

  { MaybeObject* maybe_obj = Allocate(oddball_map(), OLD_DATA_SPACE);
    if (!maybe_obj->ToObject(&obj)) return false;
  }
  set_null_value(obj);

  // Allocate the empty descriptor array.
  { MaybeObject* maybe_obj = AllocateEmptyFixedArray();
    if (!maybe_obj->ToObject(&obj)) return false;
  }
  set_empty_descriptor_array(DescriptorArray::cast(obj));

  // Fix the instance_descriptors for the existing maps.
  meta_map()->set_instance_descriptors(empty_descriptor_array());
  meta_map()->set_code_cache(empty_fixed_array());

  fixed_array_map()->set_instance_descriptors(empty_descriptor_array());
  fixed_array_map()->set_code_cache(empty_fixed_array());

  oddball_map()->set_instance_descriptors(empty_descriptor_array());
  oddball_map()->set_code_cache(empty_fixed_array());

  // Fix prototype object for existing maps.
  meta_map()->set_prototype(null_value());
  meta_map()->set_constructor(null_value());

  fixed_array_map()->set_prototype(null_value());
  fixed_array_map()->set_constructor(null_value());

  oddball_map()->set_prototype(null_value());
  oddball_map()->set_constructor(null_value());

  { MaybeObject* maybe_obj =
        AllocateMap(FIXED_ARRAY_TYPE, kVariableSizeSentinel);
    if (!maybe_obj->ToObject(&obj)) return false;
  }
  set_fixed_cow_array_map(Map::cast(obj));
  ASSERT(fixed_array_map() != fixed_cow_array_map());

  { MaybeObject* maybe_obj = AllocateMap(HEAP_NUMBER_TYPE, HeapNumber::kSize);
    if (!maybe_obj->ToObject(&obj)) return false;
  }
  set_heap_number_map(Map::cast(obj));

  { MaybeObject* maybe_obj = AllocateMap(PROXY_TYPE, Proxy::kSize);
    if (!maybe_obj->ToObject(&obj)) return false;
  }
  set_proxy_map(Map::cast(obj));

  for (unsigned i = 0; i < ARRAY_SIZE(string_type_table); i++) {
    const StringTypeTable& entry = string_type_table[i];
    { MaybeObject* maybe_obj = AllocateMap(entry.type, entry.size);
      if (!maybe_obj->ToObject(&obj)) return false;
    }
    roots_[entry.index] = Map::cast(obj);
  }

  { MaybeObject* maybe_obj = AllocateMap(STRING_TYPE, kVariableSizeSentinel);
    if (!maybe_obj->ToObject(&obj)) return false;
  }
  set_undetectable_string_map(Map::cast(obj));
  Map::cast(obj)->set_is_undetectable();

  { MaybeObject* maybe_obj =
        AllocateMap(ASCII_STRING_TYPE, kVariableSizeSentinel);
    if (!maybe_obj->ToObject(&obj)) return false;
  }
  set_undetectable_ascii_string_map(Map::cast(obj));
  Map::cast(obj)->set_is_undetectable();

  { MaybeObject* maybe_obj =
        AllocateMap(BYTE_ARRAY_TYPE, kVariableSizeSentinel);
    if (!maybe_obj->ToObject(&obj)) return false;
  }
  set_byte_array_map(Map::cast(obj));

  { MaybeObject* maybe_obj = AllocateByteArray(0, TENURED);
    if (!maybe_obj->ToObject(&obj)) return false;
  }
  set_empty_byte_array(ByteArray::cast(obj));

  { MaybeObject* maybe_obj =
        AllocateMap(PIXEL_ARRAY_TYPE, PixelArray::kAlignedSize);
    if (!maybe_obj->ToObject(&obj)) return false;
  }
  set_pixel_array_map(Map::cast(obj));

  { MaybeObject* maybe_obj = AllocateMap(EXTERNAL_BYTE_ARRAY_TYPE,
                                         ExternalArray::kAlignedSize);
    if (!maybe_obj->ToObject(&obj)) return false;
  }
  set_external_byte_array_map(Map::cast(obj));

  { MaybeObject* maybe_obj = AllocateMap(EXTERNAL_UNSIGNED_BYTE_ARRAY_TYPE,
                                         ExternalArray::kAlignedSize);
    if (!maybe_obj->ToObject(&obj)) return false;
  }
  set_external_unsigned_byte_array_map(Map::cast(obj));

  { MaybeObject* maybe_obj = AllocateMap(EXTERNAL_SHORT_ARRAY_TYPE,
                                         ExternalArray::kAlignedSize);
    if (!maybe_obj->ToObject(&obj)) return false;
  }
  set_external_short_array_map(Map::cast(obj));

  { MaybeObject* maybe_obj = AllocateMap(EXTERNAL_UNSIGNED_SHORT_ARRAY_TYPE,
                                         ExternalArray::kAlignedSize);
    if (!maybe_obj->ToObject(&obj)) return false;
  }
  set_external_unsigned_short_array_map(Map::cast(obj));

  { MaybeObject* maybe_obj = AllocateMap(EXTERNAL_INT_ARRAY_TYPE,
                                         ExternalArray::kAlignedSize);
    if (!maybe_obj->ToObject(&obj)) return false;
  }
  set_external_int_array_map(Map::cast(obj));

  { MaybeObject* maybe_obj = AllocateMap(EXTERNAL_UNSIGNED_INT_ARRAY_TYPE,
                                         ExternalArray::kAlignedSize);
    if (!maybe_obj->ToObject(&obj)) return false;
  }
  set_external_unsigned_int_array_map(Map::cast(obj));

  { MaybeObject* maybe_obj = AllocateMap(EXTERNAL_FLOAT_ARRAY_TYPE,
                                         ExternalArray::kAlignedSize);
    if (!maybe_obj->ToObject(&obj)) return false;
  }
  set_external_float_array_map(Map::cast(obj));

  { MaybeObject* maybe_obj = AllocateMap(CODE_TYPE, kVariableSizeSentinel);
    if (!maybe_obj->ToObject(&obj)) return false;
  }
  set_code_map(Map::cast(obj));

  { MaybeObject* maybe_obj = AllocateMap(JS_GLOBAL_PROPERTY_CELL_TYPE,
                                         JSGlobalPropertyCell::kSize);
    if (!maybe_obj->ToObject(&obj)) return false;
  }
  set_global_property_cell_map(Map::cast(obj));

  { MaybeObject* maybe_obj = AllocateMap(FILLER_TYPE, kPointerSize);
    if (!maybe_obj->ToObject(&obj)) return false;
  }
  set_one_pointer_filler_map(Map::cast(obj));

  { MaybeObject* maybe_obj = AllocateMap(FILLER_TYPE, 2 * kPointerSize);
    if (!maybe_obj->ToObject(&obj)) return false;
  }
  set_two_pointer_filler_map(Map::cast(obj));

  for (unsigned i = 0; i < ARRAY_SIZE(struct_table); i++) {
    const StructTable& entry = struct_table[i];
    { MaybeObject* maybe_obj = AllocateMap(entry.type, entry.size);
      if (!maybe_obj->ToObject(&obj)) return false;
    }
    roots_[entry.index] = Map::cast(obj);
  }

  { MaybeObject* maybe_obj =
        AllocateMap(FIXED_ARRAY_TYPE, kVariableSizeSentinel);
    if (!maybe_obj->ToObject(&obj)) return false;
  }
  set_hash_table_map(Map::cast(obj));

  { MaybeObject* maybe_obj =
        AllocateMap(FIXED_ARRAY_TYPE, kVariableSizeSentinel);
    if (!maybe_obj->ToObject(&obj)) return false;
  }
  set_context_map(Map::cast(obj));

  { MaybeObject* maybe_obj =
        AllocateMap(FIXED_ARRAY_TYPE, kVariableSizeSentinel);
    if (!maybe_obj->ToObject(&obj)) return false;
  }
  set_catch_context_map(Map::cast(obj));

  { MaybeObject* maybe_obj =
        AllocateMap(FIXED_ARRAY_TYPE, kVariableSizeSentinel);
    if (!maybe_obj->ToObject(&obj)) return false;
  }
  Map* global_context_map = Map::cast(obj);
  global_context_map->set_visitor_id(StaticVisitorBase::kVisitGlobalContext);
  set_global_context_map(global_context_map);

  { MaybeObject* maybe_obj = AllocateMap(SHARED_FUNCTION_INFO_TYPE,
                                         SharedFunctionInfo::kAlignedSize);
    if (!maybe_obj->ToObject(&obj)) return false;
  }
  set_shared_function_info_map(Map::cast(obj));

  { MaybeObject* maybe_obj = AllocateMap(JS_MESSAGE_OBJECT_TYPE,
                                         JSMessageObject::kSize);
    if (!maybe_obj->ToObject(&obj)) return false;
  }
  set_message_object_map(Map::cast(obj));

  ASSERT(!Heap::InNewSpace(Heap::empty_fixed_array()));
  return true;
}


MaybeObject* Heap::AllocateHeapNumber(double value, PretenureFlag pretenure) {
  // Statically ensure that it is safe to allocate heap numbers in paged
  // spaces.
  STATIC_ASSERT(HeapNumber::kSize <= Page::kMaxHeapObjectSize);
  AllocationSpace space = (pretenure == TENURED) ? OLD_DATA_SPACE : NEW_SPACE;

  Object* result;
  { MaybeObject* maybe_result =
        AllocateRaw(HeapNumber::kSize, space, OLD_DATA_SPACE);
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }

  HeapObject::cast(result)->set_map(heap_number_map());
  HeapNumber::cast(result)->set_value(value);
  return result;
}


MaybeObject* Heap::AllocateHeapNumber(double value) {
  // Use general version, if we're forced to always allocate.
  if (always_allocate()) return AllocateHeapNumber(value, TENURED);

  // This version of AllocateHeapNumber is optimized for
  // allocation in new space.
  STATIC_ASSERT(HeapNumber::kSize <= Page::kMaxHeapObjectSize);
  ASSERT(allocation_allowed_ && gc_state_ == NOT_IN_GC);
  Object* result;
  { MaybeObject* maybe_result = new_space_.AllocateRaw(HeapNumber::kSize);
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }
  HeapObject::cast(result)->set_map(heap_number_map());
  HeapNumber::cast(result)->set_value(value);
  return result;
}


MaybeObject* Heap::AllocateJSGlobalPropertyCell(Object* value) {
  Object* result;
  { MaybeObject* maybe_result = AllocateRawCell();
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }
  HeapObject::cast(result)->set_map(global_property_cell_map());
  JSGlobalPropertyCell::cast(result)->set_value(value);
  return result;
}


MaybeObject* Heap::CreateOddball(const char* to_string,
                                 Object* to_number) {
  Object* result;
  { MaybeObject* maybe_result = Allocate(oddball_map(), OLD_DATA_SPACE);
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }
  return Oddball::cast(result)->Initialize(to_string, to_number);
}


bool Heap::CreateApiObjects() {
  Object* obj;

  { MaybeObject* maybe_obj = AllocateMap(JS_OBJECT_TYPE, JSObject::kHeaderSize);
    if (!maybe_obj->ToObject(&obj)) return false;
  }
  set_neander_map(Map::cast(obj));

  { MaybeObject* maybe_obj = Heap::AllocateJSObjectFromMap(neander_map());
    if (!maybe_obj->ToObject(&obj)) return false;
  }
  Object* elements;
  { MaybeObject* maybe_elements = AllocateFixedArray(2);
    if (!maybe_elements->ToObject(&elements)) return false;
  }
  FixedArray::cast(elements)->set(0, Smi::FromInt(0));
  JSObject::cast(obj)->set_elements(FixedArray::cast(elements));
  set_message_listeners(JSObject::cast(obj));

  return true;
}


void Heap::CreateCEntryStub() {
  CEntryStub stub(1);
  set_c_entry_code(*stub.GetCode());
}


#if V8_TARGET_ARCH_ARM && !V8_INTERPRETED_REGEXP
void Heap::CreateRegExpCEntryStub() {
  RegExpCEntryStub stub;
  set_re_c_entry_code(*stub.GetCode());
}
#endif


void Heap::CreateJSEntryStub() {
  JSEntryStub stub;
  set_js_entry_code(*stub.GetCode());
}


void Heap::CreateJSConstructEntryStub() {
  JSConstructEntryStub stub;
  set_js_construct_entry_code(*stub.GetCode());
}


#if V8_TARGET_ARCH_ARM
void Heap::CreateDirectCEntryStub() {
  DirectCEntryStub stub;
  set_direct_c_entry_code(*stub.GetCode());
}
#endif


void Heap::CreateFixedStubs() {
  // Here we create roots for fixed stubs. They are needed at GC
  // for cooking and uncooking (check out frames.cc).
  // The eliminates the need for doing dictionary lookup in the
  // stub cache for these stubs.
  HandleScope scope;
  // gcc-4.4 has problem generating correct code of following snippet:
  // {  CEntryStub stub;
  //    c_entry_code_ = *stub.GetCode();
  // }
  // {  DebuggerStatementStub stub;
  //    debugger_statement_code_ = *stub.GetCode();
  // }
  // To workaround the problem, make separate functions without inlining.
  Heap::CreateCEntryStub();
  Heap::CreateJSEntryStub();
  Heap::CreateJSConstructEntryStub();
#if V8_TARGET_ARCH_ARM && !V8_INTERPRETED_REGEXP
  Heap::CreateRegExpCEntryStub();
#endif
#if V8_TARGET_ARCH_ARM
  Heap::CreateDirectCEntryStub();
#endif
}


bool Heap::CreateInitialObjects() {
  Object* obj;

  // The -0 value must be set before NumberFromDouble works.
  { MaybeObject* maybe_obj = AllocateHeapNumber(-0.0, TENURED);
    if (!maybe_obj->ToObject(&obj)) return false;
  }
  set_minus_zero_value(obj);
  ASSERT(signbit(minus_zero_value()->Number()) != 0);

  { MaybeObject* maybe_obj = AllocateHeapNumber(OS::nan_value(), TENURED);
    if (!maybe_obj->ToObject(&obj)) return false;
  }
  set_nan_value(obj);

  { MaybeObject* maybe_obj = Allocate(oddball_map(), OLD_DATA_SPACE);
    if (!maybe_obj->ToObject(&obj)) return false;
  }
  set_undefined_value(obj);
  ASSERT(!InNewSpace(undefined_value()));

  // Allocate initial symbol table.
  { MaybeObject* maybe_obj = SymbolTable::Allocate(kInitialSymbolTableSize);
    if (!maybe_obj->ToObject(&obj)) return false;
  }
  // Don't use set_symbol_table() due to asserts.
  roots_[kSymbolTableRootIndex] = obj;

  // Assign the print strings for oddballs after creating symboltable.
  Object* symbol;
  { MaybeObject* maybe_symbol = LookupAsciiSymbol("undefined");
    if (!maybe_symbol->ToObject(&symbol)) return false;
  }
  Oddball::cast(undefined_value())->set_to_string(String::cast(symbol));
  Oddball::cast(undefined_value())->set_to_number(nan_value());

  // Allocate the null_value
  { MaybeObject* maybe_obj =
        Oddball::cast(null_value())->Initialize("null", Smi::FromInt(0));
    if (!maybe_obj->ToObject(&obj)) return false;
  }

  { MaybeObject* maybe_obj = CreateOddball("true", Smi::FromInt(1));
    if (!maybe_obj->ToObject(&obj)) return false;
  }
  set_true_value(obj);

  { MaybeObject* maybe_obj = CreateOddball("false", Smi::FromInt(0));
    if (!maybe_obj->ToObject(&obj)) return false;
  }
  set_false_value(obj);

  { MaybeObject* maybe_obj = CreateOddball("hole", Smi::FromInt(-1));
    if (!maybe_obj->ToObject(&obj)) return false;
  }
  set_the_hole_value(obj);

  { MaybeObject* maybe_obj = CreateOddball("arguments_marker",
                                           Smi::FromInt(-4));
    if (!maybe_obj->ToObject(&obj)) return false;
  }
  set_arguments_marker(obj);

  { MaybeObject* maybe_obj =
        CreateOddball("no_interceptor_result_sentinel", Smi::FromInt(-2));
    if (!maybe_obj->ToObject(&obj)) return false;
  }
  set_no_interceptor_result_sentinel(obj);

  { MaybeObject* maybe_obj =
        CreateOddball("termination_exception", Smi::FromInt(-3));
    if (!maybe_obj->ToObject(&obj)) return false;
  }
  set_termination_exception(obj);

  // Allocate the empty string.
  { MaybeObject* maybe_obj = AllocateRawAsciiString(0, TENURED);
    if (!maybe_obj->ToObject(&obj)) return false;
  }
  set_empty_string(String::cast(obj));

  for (unsigned i = 0; i < ARRAY_SIZE(constant_symbol_table); i++) {
    { MaybeObject* maybe_obj =
          LookupAsciiSymbol(constant_symbol_table[i].contents);
      if (!maybe_obj->ToObject(&obj)) return false;
    }
    roots_[constant_symbol_table[i].index] = String::cast(obj);
  }

  // Allocate the hidden symbol which is used to identify the hidden properties
  // in JSObjects. The hash code has a special value so that it will not match
  // the empty string when searching for the property. It cannot be part of the
  // loop above because it needs to be allocated manually with the special
  // hash code in place. The hash code for the hidden_symbol is zero to ensure
  // that it will always be at the first entry in property descriptors.
  { MaybeObject* maybe_obj =
        AllocateSymbol(CStrVector(""), 0, String::kZeroHash);
    if (!maybe_obj->ToObject(&obj)) return false;
  }
  hidden_symbol_ = String::cast(obj);

  // Allocate the proxy for __proto__.
  { MaybeObject* maybe_obj =
        AllocateProxy((Address) &Accessors::ObjectPrototype);
    if (!maybe_obj->ToObject(&obj)) return false;
  }
  set_prototype_accessors(Proxy::cast(obj));

  // Allocate the code_stubs dictionary. The initial size is set to avoid
  // expanding the dictionary during bootstrapping.
  { MaybeObject* maybe_obj = NumberDictionary::Allocate(128);
    if (!maybe_obj->ToObject(&obj)) return false;
  }
  set_code_stubs(NumberDictionary::cast(obj));

  // Allocate the non_monomorphic_cache used in stub-cache.cc. The initial size
  // is set to avoid expanding the dictionary during bootstrapping.
  { MaybeObject* maybe_obj = NumberDictionary::Allocate(64);
    if (!maybe_obj->ToObject(&obj)) return false;
  }
  set_non_monomorphic_cache(NumberDictionary::cast(obj));

  set_instanceof_cache_function(Smi::FromInt(0));
  set_instanceof_cache_map(Smi::FromInt(0));
  set_instanceof_cache_answer(Smi::FromInt(0));

  CreateFixedStubs();

  // Allocate the dictionary of intrinsic function names.
  { MaybeObject* maybe_obj = StringDictionary::Allocate(Runtime::kNumFunctions);
    if (!maybe_obj->ToObject(&obj)) return false;
  }
  { MaybeObject* maybe_obj = Runtime::InitializeIntrinsicFunctionNames(obj);
    if (!maybe_obj->ToObject(&obj)) return false;
  }
  set_intrinsic_function_names(StringDictionary::cast(obj));

  if (InitializeNumberStringCache()->IsFailure()) return false;

  // Allocate cache for single character ASCII strings.
  { MaybeObject* maybe_obj =
        AllocateFixedArray(String::kMaxAsciiCharCode + 1, TENURED);
    if (!maybe_obj->ToObject(&obj)) return false;
  }
  set_single_character_string_cache(FixedArray::cast(obj));

  // Allocate cache for external strings pointing to native source code.
  { MaybeObject* maybe_obj = AllocateFixedArray(Natives::GetBuiltinsCount());
    if (!maybe_obj->ToObject(&obj)) return false;
  }
  set_natives_source_cache(FixedArray::cast(obj));

  // Handling of script id generation is in Factory::NewScript.
  set_last_script_id(undefined_value());

  // Initialize keyed lookup cache.
  KeyedLookupCache::Clear();

  // Initialize context slot cache.
  ContextSlotCache::Clear();

  // Initialize descriptor cache.
  DescriptorLookupCache::Clear();

  // Initialize compilation cache.
  CompilationCache::Clear();

  return true;
}


MaybeObject* Heap::InitializeNumberStringCache() {
  // Compute the size of the number string cache based on the max heap size.
  // max_semispace_size_ == 512 KB => number_string_cache_size = 32.
  // max_semispace_size_ ==   8 MB => number_string_cache_size = 16KB.
  int number_string_cache_size = max_semispace_size_ / 512;
  number_string_cache_size = Max(32, Min(16*KB, number_string_cache_size));
  Object* obj;
  MaybeObject* maybe_obj =
      AllocateFixedArray(number_string_cache_size * 2, TENURED);
  if (maybe_obj->ToObject(&obj)) set_number_string_cache(FixedArray::cast(obj));
  return maybe_obj;
}


void Heap::FlushNumberStringCache() {
  // Flush the number to string cache.
  int len = number_string_cache()->length();
  for (int i = 0; i < len; i++) {
    number_string_cache()->set_undefined(i);
  }
}


static inline int double_get_hash(double d) {
  DoubleRepresentation rep(d);
  return static_cast<int>(rep.bits) ^ static_cast<int>(rep.bits >> 32);
}


static inline int smi_get_hash(Smi* smi) {
  return smi->value();
}


Object* Heap::GetNumberStringCache(Object* number) {
  int hash;
  int mask = (number_string_cache()->length() >> 1) - 1;
  if (number->IsSmi()) {
    hash = smi_get_hash(Smi::cast(number)) & mask;
  } else {
    hash = double_get_hash(number->Number()) & mask;
  }
  Object* key = number_string_cache()->get(hash * 2);
  if (key == number) {
    return String::cast(number_string_cache()->get(hash * 2 + 1));
  } else if (key->IsHeapNumber() &&
             number->IsHeapNumber() &&
             key->Number() == number->Number()) {
    return String::cast(number_string_cache()->get(hash * 2 + 1));
  }
  return undefined_value();
}


void Heap::SetNumberStringCache(Object* number, String* string) {
  int hash;
  int mask = (number_string_cache()->length() >> 1) - 1;
  if (number->IsSmi()) {
    hash = smi_get_hash(Smi::cast(number)) & mask;
    number_string_cache()->set(hash * 2, Smi::cast(number));
  } else {
    hash = double_get_hash(number->Number()) & mask;
    number_string_cache()->set(hash * 2, number);
  }
  number_string_cache()->set(hash * 2 + 1, string);
}


MaybeObject* Heap::NumberToString(Object* number,
                                  bool check_number_string_cache) {
  Counters::number_to_string_runtime.Increment();
  if (check_number_string_cache) {
    Object* cached = GetNumberStringCache(number);
    if (cached != undefined_value()) {
      return cached;
    }
  }

  char arr[100];
  Vector<char> buffer(arr, ARRAY_SIZE(arr));
  const char* str;
  if (number->IsSmi()) {
    int num = Smi::cast(number)->value();
    str = IntToCString(num, buffer);
  } else {
    double num = HeapNumber::cast(number)->value();
    str = DoubleToCString(num, buffer);
  }

  Object* js_string;
  MaybeObject* maybe_js_string = AllocateStringFromAscii(CStrVector(str));
  if (maybe_js_string->ToObject(&js_string)) {
    SetNumberStringCache(number, String::cast(js_string));
  }
  return maybe_js_string;
}


Map* Heap::MapForExternalArrayType(ExternalArrayType array_type) {
  return Map::cast(roots_[RootIndexForExternalArrayType(array_type)]);
}


Heap::RootListIndex Heap::RootIndexForExternalArrayType(
    ExternalArrayType array_type) {
  switch (array_type) {
    case kExternalByteArray:
      return kExternalByteArrayMapRootIndex;
    case kExternalUnsignedByteArray:
      return kExternalUnsignedByteArrayMapRootIndex;
    case kExternalShortArray:
      return kExternalShortArrayMapRootIndex;
    case kExternalUnsignedShortArray:
      return kExternalUnsignedShortArrayMapRootIndex;
    case kExternalIntArray:
      return kExternalIntArrayMapRootIndex;
    case kExternalUnsignedIntArray:
      return kExternalUnsignedIntArrayMapRootIndex;
    case kExternalFloatArray:
      return kExternalFloatArrayMapRootIndex;
    default:
      UNREACHABLE();
      return kUndefinedValueRootIndex;
  }
}


MaybeObject* Heap::NumberFromDouble(double value, PretenureFlag pretenure) {
  // We need to distinguish the minus zero value and this cannot be
  // done after conversion to int. Doing this by comparing bit
  // patterns is faster than using fpclassify() et al.
  static const DoubleRepresentation minus_zero(-0.0);

  DoubleRepresentation rep(value);
  if (rep.bits == minus_zero.bits) {
    return AllocateHeapNumber(-0.0, pretenure);
  }

  int int_value = FastD2I(value);
  if (value == int_value && Smi::IsValid(int_value)) {
    return Smi::FromInt(int_value);
  }

  // Materialize the value in the heap.
  return AllocateHeapNumber(value, pretenure);
}


MaybeObject* Heap::AllocateProxy(Address proxy, PretenureFlag pretenure) {
  // Statically ensure that it is safe to allocate proxies in paged spaces.
  STATIC_ASSERT(Proxy::kSize <= Page::kMaxHeapObjectSize);
  AllocationSpace space = (pretenure == TENURED) ? OLD_DATA_SPACE : NEW_SPACE;
  Object* result;
  { MaybeObject* maybe_result = Allocate(proxy_map(), space);
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }

  Proxy::cast(result)->set_proxy(proxy);
  return result;
}


MaybeObject* Heap::AllocateSharedFunctionInfo(Object* name) {
  Object* result;
  { MaybeObject* maybe_result =
        Allocate(shared_function_info_map(), OLD_POINTER_SPACE);
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }

  SharedFunctionInfo* share = SharedFunctionInfo::cast(result);
  share->set_name(name);
  Code* illegal = Builtins::builtin(Builtins::Illegal);
  share->set_code(illegal);
  share->set_scope_info(SerializedScopeInfo::Empty());
  Code* construct_stub = Builtins::builtin(Builtins::JSConstructStubGeneric);
  share->set_construct_stub(construct_stub);
  share->set_expected_nof_properties(0);
  share->set_length(0);
  share->set_formal_parameter_count(0);
  share->set_instance_class_name(Object_symbol());
  share->set_function_data(undefined_value());
  share->set_script(undefined_value());
  share->set_start_position_and_type(0);
  share->set_debug_info(undefined_value());
  share->set_inferred_name(empty_string());
  share->set_compiler_hints(0);
  share->set_deopt_counter(Smi::FromInt(FLAG_deopt_every_n_times));
  share->set_initial_map(undefined_value());
  share->set_this_property_assignments_count(0);
  share->set_this_property_assignments(undefined_value());
  share->set_opt_count(0);
  share->set_num_literals(0);
  share->set_end_position(0);
  share->set_function_token_position(0);
  return result;
}


MaybeObject* Heap::AllocateJSMessageObject(String* type,
                                           JSArray* arguments,
                                           int start_position,
                                           int end_position,
                                           Object* script,
                                           Object* stack_trace,
                                           Object* stack_frames) {
  Object* result;
  { MaybeObject* maybe_result = Allocate(message_object_map(), NEW_SPACE);
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }
  JSMessageObject* message = JSMessageObject::cast(result);
  message->set_properties(Heap::empty_fixed_array());
  message->set_elements(Heap::empty_fixed_array());
  message->set_type(type);
  message->set_arguments(arguments);
  message->set_start_position(start_position);
  message->set_end_position(end_position);
  message->set_script(script);
  message->set_stack_trace(stack_trace);
  message->set_stack_frames(stack_frames);
  return result;
}



// Returns true for a character in a range.  Both limits are inclusive.
static inline bool Between(uint32_t character, uint32_t from, uint32_t to) {
  // This makes uses of the the unsigned wraparound.
  return character - from <= to - from;
}


MUST_USE_RESULT static inline MaybeObject* MakeOrFindTwoCharacterString(
    uint32_t c1,
    uint32_t c2) {
  String* symbol;
  // Numeric strings have a different hash algorithm not known by
  // LookupTwoCharsSymbolIfExists, so we skip this step for such strings.
  if ((!Between(c1, '0', '9') || !Between(c2, '0', '9')) &&
      Heap::symbol_table()->LookupTwoCharsSymbolIfExists(c1, c2, &symbol)) {
    return symbol;
  // Now we know the length is 2, we might as well make use of that fact
  // when building the new string.
  } else if ((c1 | c2) <= String::kMaxAsciiCharCodeU) {  // We can do this
    ASSERT(IsPowerOf2(String::kMaxAsciiCharCodeU + 1));  // because of this.
    Object* result;
    { MaybeObject* maybe_result = Heap::AllocateRawAsciiString(2);
      if (!maybe_result->ToObject(&result)) return maybe_result;
    }
    char* dest = SeqAsciiString::cast(result)->GetChars();
    dest[0] = c1;
    dest[1] = c2;
    return result;
  } else {
    Object* result;
    { MaybeObject* maybe_result = Heap::AllocateRawTwoByteString(2);
      if (!maybe_result->ToObject(&result)) return maybe_result;
    }
    uc16* dest = SeqTwoByteString::cast(result)->GetChars();
    dest[0] = c1;
    dest[1] = c2;
    return result;
  }
}


MaybeObject* Heap::AllocateConsString(String* first, String* second) {
  int first_length = first->length();
  if (first_length == 0) {
    return second;
  }

  int second_length = second->length();
  if (second_length == 0) {
    return first;
  }

  int length = first_length + second_length;

  // Optimization for 2-byte strings often used as keys in a decompression
  // dictionary.  Check whether we already have the string in the symbol
  // table to prevent creation of many unneccesary strings.
  if (length == 2) {
    unsigned c1 = first->Get(0);
    unsigned c2 = second->Get(0);
    return MakeOrFindTwoCharacterString(c1, c2);
  }

  bool first_is_ascii = first->IsAsciiRepresentation();
  bool second_is_ascii = second->IsAsciiRepresentation();
  bool is_ascii = first_is_ascii && second_is_ascii;

  // Make sure that an out of memory exception is thrown if the length
  // of the new cons string is too large.
  if (length > String::kMaxLength || length < 0) {
    Top::context()->mark_out_of_memory();
    return Failure::OutOfMemoryException();
  }

  bool is_ascii_data_in_two_byte_string = false;
  if (!is_ascii) {
    // At least one of the strings uses two-byte representation so we
    // can't use the fast case code for short ascii strings below, but
    // we can try to save memory if all chars actually fit in ascii.
    is_ascii_data_in_two_byte_string =
        first->HasOnlyAsciiChars() && second->HasOnlyAsciiChars();
    if (is_ascii_data_in_two_byte_string) {
      Counters::string_add_runtime_ext_to_ascii.Increment();
    }
  }

  // If the resulting string is small make a flat string.
  if (length < String::kMinNonFlatLength) {
    ASSERT(first->IsFlat());
    ASSERT(second->IsFlat());
    if (is_ascii) {
      Object* result;
      { MaybeObject* maybe_result = AllocateRawAsciiString(length);
        if (!maybe_result->ToObject(&result)) return maybe_result;
      }
      // Copy the characters into the new object.
      char* dest = SeqAsciiString::cast(result)->GetChars();
      // Copy first part.
      const char* src;
      if (first->IsExternalString()) {
        src = ExternalAsciiString::cast(first)->resource()->data();
      } else {
        src = SeqAsciiString::cast(first)->GetChars();
      }
      for (int i = 0; i < first_length; i++) *dest++ = src[i];
      // Copy second part.
      if (second->IsExternalString()) {
        src = ExternalAsciiString::cast(second)->resource()->data();
      } else {
        src = SeqAsciiString::cast(second)->GetChars();
      }
      for (int i = 0; i < second_length; i++) *dest++ = src[i];
      return result;
    } else {
      if (is_ascii_data_in_two_byte_string) {
        Object* result;
        { MaybeObject* maybe_result = AllocateRawAsciiString(length);
          if (!maybe_result->ToObject(&result)) return maybe_result;
        }
        // Copy the characters into the new object.
        char* dest = SeqAsciiString::cast(result)->GetChars();
        String::WriteToFlat(first, dest, 0, first_length);
        String::WriteToFlat(second, dest + first_length, 0, second_length);
        return result;
      }

      Object* result;
      { MaybeObject* maybe_result = AllocateRawTwoByteString(length);
        if (!maybe_result->ToObject(&result)) return maybe_result;
      }
      // Copy the characters into the new object.
      uc16* dest = SeqTwoByteString::cast(result)->GetChars();
      String::WriteToFlat(first, dest, 0, first_length);
      String::WriteToFlat(second, dest + first_length, 0, second_length);
      return result;
    }
  }

  Map* map = (is_ascii || is_ascii_data_in_two_byte_string) ?
      cons_ascii_string_map() : cons_string_map();

  Object* result;
  { MaybeObject* maybe_result = Allocate(map, NEW_SPACE);
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }

  AssertNoAllocation no_gc;
  ConsString* cons_string = ConsString::cast(result);
  WriteBarrierMode mode = cons_string->GetWriteBarrierMode(no_gc);
  cons_string->set_length(length);
  cons_string->set_hash_field(String::kEmptyHashField);
  cons_string->set_first(first, mode);
  cons_string->set_second(second, mode);
  return result;
}


MaybeObject* Heap::AllocateSubString(String* buffer,
                                int start,
                                int end,
                                PretenureFlag pretenure) {
  int length = end - start;

  if (length == 1) {
    return Heap::LookupSingleCharacterStringFromCode(
        buffer->Get(start));
  } else if (length == 2) {
    // Optimization for 2-byte strings often used as keys in a decompression
    // dictionary.  Check whether we already have the string in the symbol
    // table to prevent creation of many unneccesary strings.
    unsigned c1 = buffer->Get(start);
    unsigned c2 = buffer->Get(start + 1);
    return MakeOrFindTwoCharacterString(c1, c2);
  }

  // Make an attempt to flatten the buffer to reduce access time.
  buffer = buffer->TryFlattenGetString();

  Object* result;
  { MaybeObject* maybe_result = buffer->IsAsciiRepresentation()
                   ? AllocateRawAsciiString(length, pretenure )
                   : AllocateRawTwoByteString(length, pretenure);
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }
  String* string_result = String::cast(result);
  // Copy the characters into the new object.
  if (buffer->IsAsciiRepresentation()) {
    ASSERT(string_result->IsAsciiRepresentation());
    char* dest = SeqAsciiString::cast(string_result)->GetChars();
    String::WriteToFlat(buffer, dest, start, end);
  } else {
    ASSERT(string_result->IsTwoByteRepresentation());
    uc16* dest = SeqTwoByteString::cast(string_result)->GetChars();
    String::WriteToFlat(buffer, dest, start, end);
  }

  return result;
}


MaybeObject* Heap::AllocateExternalStringFromAscii(
    ExternalAsciiString::Resource* resource) {
  size_t length = resource->length();
  if (length > static_cast<size_t>(String::kMaxLength)) {
    Top::context()->mark_out_of_memory();
    return Failure::OutOfMemoryException();
  }

  Map* map = external_ascii_string_map();
  Object* result;
  { MaybeObject* maybe_result = Allocate(map, NEW_SPACE);
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }

  ExternalAsciiString* external_string = ExternalAsciiString::cast(result);
  external_string->set_length(static_cast<int>(length));
  external_string->set_hash_field(String::kEmptyHashField);
  external_string->set_resource(resource);

  return result;
}


MaybeObject* Heap::AllocateExternalStringFromTwoByte(
    ExternalTwoByteString::Resource* resource) {
  size_t length = resource->length();
  if (length > static_cast<size_t>(String::kMaxLength)) {
    Top::context()->mark_out_of_memory();
    return Failure::OutOfMemoryException();
  }

  // For small strings we check whether the resource contains only
  // ASCII characters.  If yes, we use a different string map.
  static const size_t kAsciiCheckLengthLimit = 32;
  bool is_ascii = length <= kAsciiCheckLengthLimit &&
      String::IsAscii(resource->data(), static_cast<int>(length));
  Map* map = is_ascii ?
      Heap::external_string_with_ascii_data_map() : Heap::external_string_map();
  Object* result;
  { MaybeObject* maybe_result = Allocate(map, NEW_SPACE);
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }

  ExternalTwoByteString* external_string = ExternalTwoByteString::cast(result);
  external_string->set_length(static_cast<int>(length));
  external_string->set_hash_field(String::kEmptyHashField);
  external_string->set_resource(resource);

  return result;
}


MaybeObject* Heap::LookupSingleCharacterStringFromCode(uint16_t code) {
  if (code <= String::kMaxAsciiCharCode) {
    Object* value = Heap::single_character_string_cache()->get(code);
    if (value != Heap::undefined_value()) return value;

    char buffer[1];
    buffer[0] = static_cast<char>(code);
    Object* result;
    MaybeObject* maybe_result = LookupSymbol(Vector<const char>(buffer, 1));

    if (!maybe_result->ToObject(&result)) return maybe_result;
    Heap::single_character_string_cache()->set(code, result);
    return result;
  }

  Object* result;
  { MaybeObject* maybe_result = Heap::AllocateRawTwoByteString(1);
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }
  String* answer = String::cast(result);
  answer->Set(0, code);
  return answer;
}


MaybeObject* Heap::AllocateByteArray(int length, PretenureFlag pretenure) {
  if (length < 0 || length > ByteArray::kMaxLength) {
    return Failure::OutOfMemoryException();
  }
  if (pretenure == NOT_TENURED) {
    return AllocateByteArray(length);
  }
  int size = ByteArray::SizeFor(length);
  Object* result;
  { MaybeObject* maybe_result = (size <= MaxObjectSizeInPagedSpace())
                   ? old_data_space_->AllocateRaw(size)
                   : lo_space_->AllocateRaw(size);
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }

  reinterpret_cast<ByteArray*>(result)->set_map(byte_array_map());
  reinterpret_cast<ByteArray*>(result)->set_length(length);
  return result;
}


MaybeObject* Heap::AllocateByteArray(int length) {
  if (length < 0 || length > ByteArray::kMaxLength) {
    return Failure::OutOfMemoryException();
  }
  int size = ByteArray::SizeFor(length);
  AllocationSpace space =
      (size > MaxObjectSizeInPagedSpace()) ? LO_SPACE : NEW_SPACE;
  Object* result;
  { MaybeObject* maybe_result = AllocateRaw(size, space, OLD_DATA_SPACE);
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }

  reinterpret_cast<ByteArray*>(result)->set_map(byte_array_map());
  reinterpret_cast<ByteArray*>(result)->set_length(length);
  return result;
}


void Heap::CreateFillerObjectAt(Address addr, int size) {
  if (size == 0) return;
  HeapObject* filler = HeapObject::FromAddress(addr);
  if (size == kPointerSize) {
    filler->set_map(one_pointer_filler_map());
  } else if (size == 2 * kPointerSize) {
    filler->set_map(two_pointer_filler_map());
  } else {
    filler->set_map(byte_array_map());
    ByteArray::cast(filler)->set_length(ByteArray::LengthFor(size));
  }
}


MaybeObject* Heap::AllocatePixelArray(int length,
                                 uint8_t* external_pointer,
                                 PretenureFlag pretenure) {
  AllocationSpace space = (pretenure == TENURED) ? OLD_DATA_SPACE : NEW_SPACE;
  Object* result;
  { MaybeObject* maybe_result =
        AllocateRaw(PixelArray::kAlignedSize, space, OLD_DATA_SPACE);
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }

  reinterpret_cast<PixelArray*>(result)->set_map(pixel_array_map());
  reinterpret_cast<PixelArray*>(result)->set_length(length);
  reinterpret_cast<PixelArray*>(result)->set_external_pointer(external_pointer);

  return result;
}


MaybeObject* Heap::AllocateExternalArray(int length,
                                         ExternalArrayType array_type,
                                         void* external_pointer,
                                         PretenureFlag pretenure) {
  AllocationSpace space = (pretenure == TENURED) ? OLD_DATA_SPACE : NEW_SPACE;
  Object* result;
  { MaybeObject* maybe_result = AllocateRaw(ExternalArray::kAlignedSize,
                                            space,
                                            OLD_DATA_SPACE);
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }

  reinterpret_cast<ExternalArray*>(result)->set_map(
      MapForExternalArrayType(array_type));
  reinterpret_cast<ExternalArray*>(result)->set_length(length);
  reinterpret_cast<ExternalArray*>(result)->set_external_pointer(
      external_pointer);

  return result;
}


MaybeObject* Heap::CreateCode(const CodeDesc& desc,
                              Code::Flags flags,
                              Handle<Object> self_reference) {
  // Allocate ByteArray before the Code object, so that we do not risk
  // leaving uninitialized Code object (and breaking the heap).
  Object* reloc_info;
  { MaybeObject* maybe_reloc_info = AllocateByteArray(desc.reloc_size, TENURED);
    if (!maybe_reloc_info->ToObject(&reloc_info)) return maybe_reloc_info;
  }

  // Compute size
  int body_size = RoundUp(desc.instr_size, kObjectAlignment);
  int obj_size = Code::SizeFor(body_size);
  ASSERT(IsAligned(static_cast<intptr_t>(obj_size), kCodeAlignment));
  MaybeObject* maybe_result;
  if (obj_size > MaxObjectSizeInPagedSpace()) {
    maybe_result = lo_space_->AllocateRawCode(obj_size);
  } else {
    maybe_result = code_space_->AllocateRaw(obj_size);
  }

  Object* result;
  if (!maybe_result->ToObject(&result)) return maybe_result;

  // Initialize the object
  HeapObject::cast(result)->set_map(code_map());
  Code* code = Code::cast(result);
  ASSERT(!CodeRange::exists() || CodeRange::contains(code->address()));
  code->set_instruction_size(desc.instr_size);
  code->set_relocation_info(ByteArray::cast(reloc_info));
  code->set_flags(flags);
  if (code->is_call_stub() || code->is_keyed_call_stub()) {
    code->set_check_type(RECEIVER_MAP_CHECK);
  }
  code->set_deoptimization_data(empty_fixed_array());
  // Allow self references to created code object by patching the handle to
  // point to the newly allocated Code object.
  if (!self_reference.is_null()) {
    *(self_reference.location()) = code;
  }
  // Migrate generated code.
  // The generated code can contain Object** values (typically from handles)
  // that are dereferenced during the copy to point directly to the actual heap
  // objects. These pointers can include references to the code object itself,
  // through the self_reference parameter.
  code->CopyFrom(desc);

#ifdef DEBUG
  code->Verify();
#endif
  return code;
}


MaybeObject* Heap::CopyCode(Code* code) {
  // Allocate an object the same size as the code object.
  int obj_size = code->Size();
  MaybeObject* maybe_result;
  if (obj_size > MaxObjectSizeInPagedSpace()) {
    maybe_result = lo_space_->AllocateRawCode(obj_size);
  } else {
    maybe_result = code_space_->AllocateRaw(obj_size);
  }

  Object* result;
  if (!maybe_result->ToObject(&result)) return maybe_result;

  // Copy code object.
  Address old_addr = code->address();
  Address new_addr = reinterpret_cast<HeapObject*>(result)->address();
  CopyBlock(new_addr, old_addr, obj_size);
  // Relocate the copy.
  Code* new_code = Code::cast(result);
  ASSERT(!CodeRange::exists() || CodeRange::contains(code->address()));
  new_code->Relocate(new_addr - old_addr);
  return new_code;
}


MaybeObject* Heap::CopyCode(Code* code, Vector<byte> reloc_info) {
  // Allocate ByteArray before the Code object, so that we do not risk
  // leaving uninitialized Code object (and breaking the heap).
  Object* reloc_info_array;
  { MaybeObject* maybe_reloc_info_array =
        AllocateByteArray(reloc_info.length(), TENURED);
    if (!maybe_reloc_info_array->ToObject(&reloc_info_array)) {
      return maybe_reloc_info_array;
    }
  }

  int new_body_size = RoundUp(code->instruction_size(), kObjectAlignment);

  int new_obj_size = Code::SizeFor(new_body_size);

  Address old_addr = code->address();

  size_t relocation_offset =
      static_cast<size_t>(code->instruction_end() - old_addr);

  MaybeObject* maybe_result;
  if (new_obj_size > MaxObjectSizeInPagedSpace()) {
    maybe_result = lo_space_->AllocateRawCode(new_obj_size);
  } else {
    maybe_result = code_space_->AllocateRaw(new_obj_size);
  }

  Object* result;
  if (!maybe_result->ToObject(&result)) return maybe_result;

  // Copy code object.
  Address new_addr = reinterpret_cast<HeapObject*>(result)->address();

  // Copy header and instructions.
  memcpy(new_addr, old_addr, relocation_offset);

  Code* new_code = Code::cast(result);
  new_code->set_relocation_info(ByteArray::cast(reloc_info_array));

  // Copy patched rinfo.
  memcpy(new_code->relocation_start(), reloc_info.start(), reloc_info.length());

  // Relocate the copy.
  ASSERT(!CodeRange::exists() || CodeRange::contains(code->address()));
  new_code->Relocate(new_addr - old_addr);

#ifdef DEBUG
  code->Verify();
#endif
  return new_code;
}


MaybeObject* Heap::Allocate(Map* map, AllocationSpace space) {
  ASSERT(gc_state_ == NOT_IN_GC);
  ASSERT(map->instance_type() != MAP_TYPE);
  // If allocation failures are disallowed, we may allocate in a different
  // space when new space is full and the object is not a large object.
  AllocationSpace retry_space =
      (space != NEW_SPACE) ? space : TargetSpaceId(map->instance_type());
  Object* result;
  { MaybeObject* maybe_result =
        AllocateRaw(map->instance_size(), space, retry_space);
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }
  HeapObject::cast(result)->set_map(map);
#ifdef ENABLE_LOGGING_AND_PROFILING
  ProducerHeapProfile::RecordJSObjectAllocation(result);
#endif
  return result;
}


MaybeObject* Heap::InitializeFunction(JSFunction* function,
                                      SharedFunctionInfo* shared,
                                      Object* prototype) {
  ASSERT(!prototype->IsMap());
  function->initialize_properties();
  function->initialize_elements();
  function->set_shared(shared);
  function->set_code(shared->code());
  function->set_prototype_or_initial_map(prototype);
  function->set_context(undefined_value());
  function->set_literals(empty_fixed_array());
  function->set_next_function_link(undefined_value());
  return function;
}


MaybeObject* Heap::AllocateFunctionPrototype(JSFunction* function) {
  // Allocate the prototype.  Make sure to use the object function
  // from the function's context, since the function can be from a
  // different context.
  JSFunction* object_function =
      function->context()->global_context()->object_function();
  Object* prototype;
  { MaybeObject* maybe_prototype = AllocateJSObject(object_function);
    if (!maybe_prototype->ToObject(&prototype)) return maybe_prototype;
  }
  // When creating the prototype for the function we must set its
  // constructor to the function.
  Object* result;
  { MaybeObject* maybe_result =
        JSObject::cast(prototype)->SetLocalPropertyIgnoreAttributes(
            constructor_symbol(), function, DONT_ENUM);
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }
  return prototype;
}


MaybeObject* Heap::AllocateFunction(Map* function_map,
                                    SharedFunctionInfo* shared,
                                    Object* prototype,
                                    PretenureFlag pretenure) {
  AllocationSpace space =
      (pretenure == TENURED) ? OLD_POINTER_SPACE : NEW_SPACE;
  Object* result;
  { MaybeObject* maybe_result = Allocate(function_map, space);
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }
  return InitializeFunction(JSFunction::cast(result), shared, prototype);
}


MaybeObject* Heap::AllocateArgumentsObject(Object* callee, int length) {
  // To get fast allocation and map sharing for arguments objects we
  // allocate them based on an arguments boilerplate.

  // This calls Copy directly rather than using Heap::AllocateRaw so we
  // duplicate the check here.
  ASSERT(allocation_allowed_ && gc_state_ == NOT_IN_GC);

  JSObject* boilerplate =
      Top::context()->global_context()->arguments_boilerplate();

  // Check that the size of the boilerplate matches our
  // expectations. The ArgumentsAccessStub::GenerateNewObject relies
  // on the size being a known constant.
  ASSERT(kArgumentsObjectSize == boilerplate->map()->instance_size());

  // Do the allocation.
  Object* result;
  { MaybeObject* maybe_result =
        AllocateRaw(kArgumentsObjectSize, NEW_SPACE, OLD_POINTER_SPACE);
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }

  // Copy the content. The arguments boilerplate doesn't have any
  // fields that point to new space so it's safe to skip the write
  // barrier here.
  CopyBlock(HeapObject::cast(result)->address(),
            boilerplate->address(),
            kArgumentsObjectSize);

  // Set the two properties.
  JSObject::cast(result)->InObjectPropertyAtPut(arguments_callee_index,
                                                callee);
  JSObject::cast(result)->InObjectPropertyAtPut(arguments_length_index,
                                                Smi::FromInt(length),
                                                SKIP_WRITE_BARRIER);

  // Check the state of the object
  ASSERT(JSObject::cast(result)->HasFastProperties());
  ASSERT(JSObject::cast(result)->HasFastElements());

  return result;
}


static bool HasDuplicates(DescriptorArray* descriptors) {
  int count = descriptors->number_of_descriptors();
  if (count > 1) {
    String* prev_key = descriptors->GetKey(0);
    for (int i = 1; i != count; i++) {
      String* current_key = descriptors->GetKey(i);
      if (prev_key == current_key) return true;
      prev_key = current_key;
    }
  }
  return false;
}


MaybeObject* Heap::AllocateInitialMap(JSFunction* fun) {
  ASSERT(!fun->has_initial_map());

  // First create a new map with the size and number of in-object properties
  // suggested by the function.
  int instance_size = fun->shared()->CalculateInstanceSize();
  int in_object_properties = fun->shared()->CalculateInObjectProperties();
  Object* map_obj;
  { MaybeObject* maybe_map_obj =
        Heap::AllocateMap(JS_OBJECT_TYPE, instance_size);
    if (!maybe_map_obj->ToObject(&map_obj)) return maybe_map_obj;
  }

  // Fetch or allocate prototype.
  Object* prototype;
  if (fun->has_instance_prototype()) {
    prototype = fun->instance_prototype();
  } else {
    { MaybeObject* maybe_prototype = AllocateFunctionPrototype(fun);
      if (!maybe_prototype->ToObject(&prototype)) return maybe_prototype;
    }
  }
  Map* map = Map::cast(map_obj);
  map->set_inobject_properties(in_object_properties);
  map->set_unused_property_fields(in_object_properties);
  map->set_prototype(prototype);
  ASSERT(map->has_fast_elements());

  // If the function has only simple this property assignments add
  // field descriptors for these to the initial map as the object
  // cannot be constructed without having these properties.  Guard by
  // the inline_new flag so we only change the map if we generate a
  // specialized construct stub.
  ASSERT(in_object_properties <= Map::kMaxPreAllocatedPropertyFields);
  if (fun->shared()->CanGenerateInlineConstructor(prototype)) {
    int count = fun->shared()->this_property_assignments_count();
    if (count > in_object_properties) {
      // Inline constructor can only handle inobject properties.
      fun->shared()->ForbidInlineConstructor();
    } else {
      Object* descriptors_obj;
      { MaybeObject* maybe_descriptors_obj = DescriptorArray::Allocate(count);
        if (!maybe_descriptors_obj->ToObject(&descriptors_obj)) {
          return maybe_descriptors_obj;
        }
      }
      DescriptorArray* descriptors = DescriptorArray::cast(descriptors_obj);
      for (int i = 0; i < count; i++) {
        String* name = fun->shared()->GetThisPropertyAssignmentName(i);
        ASSERT(name->IsSymbol());
        FieldDescriptor field(name, i, NONE);
        field.SetEnumerationIndex(i);
        descriptors->Set(i, &field);
      }
      descriptors->SetNextEnumerationIndex(count);
      descriptors->SortUnchecked();

      // The descriptors may contain duplicates because the compiler does not
      // guarantee the uniqueness of property names (it would have required
      // quadratic time). Once the descriptors are sorted we can check for
      // duplicates in linear time.
      if (HasDuplicates(descriptors)) {
        fun->shared()->ForbidInlineConstructor();
      } else {
        map->set_instance_descriptors(descriptors);
        map->set_pre_allocated_property_fields(count);
        map->set_unused_property_fields(in_object_properties - count);
      }
    }
  }

  fun->shared()->StartInobjectSlackTracking(map);

  return map;
}


void Heap::InitializeJSObjectFromMap(JSObject* obj,
                                     FixedArray* properties,
                                     Map* map) {
  obj->set_properties(properties);
  obj->initialize_elements();
  // TODO(1240798): Initialize the object's body using valid initial values
  // according to the object's initial map.  For example, if the map's
  // instance type is JS_ARRAY_TYPE, the length field should be initialized
  // to a number (eg, Smi::FromInt(0)) and the elements initialized to a
  // fixed array (eg, Heap::empty_fixed_array()).  Currently, the object
  // verification code has to cope with (temporarily) invalid objects.  See
  // for example, JSArray::JSArrayVerify).
  Object* filler;
  // We cannot always fill with one_pointer_filler_map because objects
  // created from API functions expect their internal fields to be initialized
  // with undefined_value.
  if (map->constructor()->IsJSFunction() &&
      JSFunction::cast(map->constructor())->shared()->
          IsInobjectSlackTrackingInProgress()) {
    // We might want to shrink the object later.
    ASSERT(obj->GetInternalFieldCount() == 0);
    filler = Heap::one_pointer_filler_map();
  } else {
    filler = Heap::undefined_value();
  }
  obj->InitializeBody(map->instance_size(), filler);
}


MaybeObject* Heap::AllocateJSObjectFromMap(Map* map, PretenureFlag pretenure) {
  // JSFunctions should be allocated using AllocateFunction to be
  // properly initialized.
  ASSERT(map->instance_type() != JS_FUNCTION_TYPE);

  // Both types of global objects should be allocated using
  // AllocateGlobalObject to be properly initialized.
  ASSERT(map->instance_type() != JS_GLOBAL_OBJECT_TYPE);
  ASSERT(map->instance_type() != JS_BUILTINS_OBJECT_TYPE);

  // Allocate the backing storage for the properties.
  int prop_size =
      map->pre_allocated_property_fields() +
      map->unused_property_fields() -
      map->inobject_properties();
  ASSERT(prop_size >= 0);
  Object* properties;
  { MaybeObject* maybe_properties = AllocateFixedArray(prop_size, pretenure);
    if (!maybe_properties->ToObject(&properties)) return maybe_properties;
  }

  // Allocate the JSObject.
  AllocationSpace space =
      (pretenure == TENURED) ? OLD_POINTER_SPACE : NEW_SPACE;
  if (map->instance_size() > MaxObjectSizeInPagedSpace()) space = LO_SPACE;
  Object* obj;
  { MaybeObject* maybe_obj = Allocate(map, space);
    if (!maybe_obj->ToObject(&obj)) return maybe_obj;
  }

  // Initialize the JSObject.
  InitializeJSObjectFromMap(JSObject::cast(obj),
                            FixedArray::cast(properties),
                            map);
  ASSERT(JSObject::cast(obj)->HasFastElements());
  return obj;
}


MaybeObject* Heap::AllocateJSObject(JSFunction* constructor,
                                    PretenureFlag pretenure) {
  // Allocate the initial map if absent.
  if (!constructor->has_initial_map()) {
    Object* initial_map;
    { MaybeObject* maybe_initial_map = AllocateInitialMap(constructor);
      if (!maybe_initial_map->ToObject(&initial_map)) return maybe_initial_map;
    }
    constructor->set_initial_map(Map::cast(initial_map));
    Map::cast(initial_map)->set_constructor(constructor);
  }
  // Allocate the object based on the constructors initial map.
  MaybeObject* result =
      AllocateJSObjectFromMap(constructor->initial_map(), pretenure);
#ifdef DEBUG
  // Make sure result is NOT a global object if valid.
  Object* non_failure;
  ASSERT(!result->ToObject(&non_failure) || !non_failure->IsGlobalObject());
#endif
  return result;
}


MaybeObject* Heap::AllocateGlobalObject(JSFunction* constructor) {
  ASSERT(constructor->has_initial_map());
  Map* map = constructor->initial_map();

  // Make sure no field properties are described in the initial map.
  // This guarantees us that normalizing the properties does not
  // require us to change property values to JSGlobalPropertyCells.
  ASSERT(map->NextFreePropertyIndex() == 0);

  // Make sure we don't have a ton of pre-allocated slots in the
  // global objects. They will be unused once we normalize the object.
  ASSERT(map->unused_property_fields() == 0);
  ASSERT(map->inobject_properties() == 0);

  // Initial size of the backing store to avoid resize of the storage during
  // bootstrapping. The size differs between the JS global object ad the
  // builtins object.
  int initial_size = map->instance_type() == JS_GLOBAL_OBJECT_TYPE ? 64 : 512;

  // Allocate a dictionary object for backing storage.
  Object* obj;
  { MaybeObject* maybe_obj =
        StringDictionary::Allocate(
            map->NumberOfDescribedProperties() * 2 + initial_size);
    if (!maybe_obj->ToObject(&obj)) return maybe_obj;
  }
  StringDictionary* dictionary = StringDictionary::cast(obj);

  // The global object might be created from an object template with accessors.
  // Fill these accessors into the dictionary.
  DescriptorArray* descs = map->instance_descriptors();
  for (int i = 0; i < descs->number_of_descriptors(); i++) {
    PropertyDetails details = descs->GetDetails(i);
    ASSERT(details.type() == CALLBACKS);  // Only accessors are expected.
    PropertyDetails d =
        PropertyDetails(details.attributes(), CALLBACKS, details.index());
    Object* value = descs->GetCallbacksObject(i);
    { MaybeObject* maybe_value = Heap::AllocateJSGlobalPropertyCell(value);
      if (!maybe_value->ToObject(&value)) return maybe_value;
    }

    Object* result;
    { MaybeObject* maybe_result = dictionary->Add(descs->GetKey(i), value, d);
      if (!maybe_result->ToObject(&result)) return maybe_result;
    }
    dictionary = StringDictionary::cast(result);
  }

  // Allocate the global object and initialize it with the backing store.
  { MaybeObject* maybe_obj = Allocate(map, OLD_POINTER_SPACE);
    if (!maybe_obj->ToObject(&obj)) return maybe_obj;
  }
  JSObject* global = JSObject::cast(obj);
  InitializeJSObjectFromMap(global, dictionary, map);

  // Create a new map for the global object.
  { MaybeObject* maybe_obj = map->CopyDropDescriptors();
    if (!maybe_obj->ToObject(&obj)) return maybe_obj;
  }
  Map* new_map = Map::cast(obj);

  // Setup the global object as a normalized object.
  global->set_map(new_map);
  global->map()->set_instance_descriptors(Heap::empty_descriptor_array());
  global->set_properties(dictionary);

  // Make sure result is a global object with properties in dictionary.
  ASSERT(global->IsGlobalObject());
  ASSERT(!global->HasFastProperties());
  return global;
}


MaybeObject* Heap::CopyJSObject(JSObject* source) {
  // Never used to copy functions.  If functions need to be copied we
  // have to be careful to clear the literals array.
  ASSERT(!source->IsJSFunction());

  // Make the clone.
  Map* map = source->map();
  int object_size = map->instance_size();
  Object* clone;

  // If we're forced to always allocate, we use the general allocation
  // functions which may leave us with an object in old space.
  if (always_allocate()) {
    { MaybeObject* maybe_clone =
          AllocateRaw(object_size, NEW_SPACE, OLD_POINTER_SPACE);
      if (!maybe_clone->ToObject(&clone)) return maybe_clone;
    }
    Address clone_address = HeapObject::cast(clone)->address();
    CopyBlock(clone_address,
              source->address(),
              object_size);
    // Update write barrier for all fields that lie beyond the header.
    RecordWrites(clone_address,
                 JSObject::kHeaderSize,
                 (object_size - JSObject::kHeaderSize) / kPointerSize);
  } else {
    { MaybeObject* maybe_clone = new_space_.AllocateRaw(object_size);
      if (!maybe_clone->ToObject(&clone)) return maybe_clone;
    }
    ASSERT(Heap::InNewSpace(clone));
    // Since we know the clone is allocated in new space, we can copy
    // the contents without worrying about updating the write barrier.
    CopyBlock(HeapObject::cast(clone)->address(),
              source->address(),
              object_size);
  }

  FixedArray* elements = FixedArray::cast(source->elements());
  FixedArray* properties = FixedArray::cast(source->properties());
  // Update elements if necessary.
  if (elements->length() > 0) {
    Object* elem;
    { MaybeObject* maybe_elem =
          (elements->map() == fixed_cow_array_map()) ?
          elements : CopyFixedArray(elements);
      if (!maybe_elem->ToObject(&elem)) return maybe_elem;
    }
    JSObject::cast(clone)->set_elements(FixedArray::cast(elem));
  }
  // Update properties if necessary.
  if (properties->length() > 0) {
    Object* prop;
    { MaybeObject* maybe_prop = CopyFixedArray(properties);
      if (!maybe_prop->ToObject(&prop)) return maybe_prop;
    }
    JSObject::cast(clone)->set_properties(FixedArray::cast(prop));
  }
  // Return the new clone.
#ifdef ENABLE_LOGGING_AND_PROFILING
  ProducerHeapProfile::RecordJSObjectAllocation(clone);
#endif
  return clone;
}


MaybeObject* Heap::ReinitializeJSGlobalProxy(JSFunction* constructor,
                                             JSGlobalProxy* object) {
  ASSERT(constructor->has_initial_map());
  Map* map = constructor->initial_map();

  // Check that the already allocated object has the same size and type as
  // objects allocated using the constructor.
  ASSERT(map->instance_size() == object->map()->instance_size());
  ASSERT(map->instance_type() == object->map()->instance_type());

  // Allocate the backing storage for the properties.
  int prop_size = map->unused_property_fields() - map->inobject_properties();
  Object* properties;
  { MaybeObject* maybe_properties = AllocateFixedArray(prop_size, TENURED);
    if (!maybe_properties->ToObject(&properties)) return maybe_properties;
  }

  // Reset the map for the object.
  object->set_map(constructor->initial_map());

  // Reinitialize the object from the constructor map.
  InitializeJSObjectFromMap(object, FixedArray::cast(properties), map);
  return object;
}


MaybeObject* Heap::AllocateStringFromAscii(Vector<const char> string,
                                           PretenureFlag pretenure) {
  Object* result;
  { MaybeObject* maybe_result =
        AllocateRawAsciiString(string.length(), pretenure);
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }

  // Copy the characters into the new object.
  SeqAsciiString* string_result = SeqAsciiString::cast(result);
  for (int i = 0; i < string.length(); i++) {
    string_result->SeqAsciiStringSet(i, string[i]);
  }
  return result;
}


MaybeObject* Heap::AllocateStringFromUtf8Slow(Vector<const char> string,
                                              PretenureFlag pretenure) {
  // V8 only supports characters in the Basic Multilingual Plane.
  const uc32 kMaxSupportedChar = 0xFFFF;
  // Count the number of characters in the UTF-8 string and check if
  // it is an ASCII string.
  Access<ScannerConstants::Utf8Decoder>
      decoder(ScannerConstants::utf8_decoder());
  decoder->Reset(string.start(), string.length());
  int chars = 0;
  while (decoder->has_more()) {
    decoder->GetNext();
    chars++;
  }

  Object* result;
  { MaybeObject* maybe_result = AllocateRawTwoByteString(chars, pretenure);
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }

  // Convert and copy the characters into the new object.
  String* string_result = String::cast(result);
  decoder->Reset(string.start(), string.length());
  for (int i = 0; i < chars; i++) {
    uc32 r = decoder->GetNext();
    if (r > kMaxSupportedChar) { r = unibrow::Utf8::kBadChar; }
    string_result->Set(i, r);
  }
  return result;
}


MaybeObject* Heap::AllocateStringFromTwoByte(Vector<const uc16> string,
                                             PretenureFlag pretenure) {
  // Check if the string is an ASCII string.
  MaybeObject* maybe_result;
  if (String::IsAscii(string.start(), string.length())) {
    maybe_result = AllocateRawAsciiString(string.length(), pretenure);
  } else {  // It's not an ASCII string.
    maybe_result = AllocateRawTwoByteString(string.length(), pretenure);
  }
  Object* result;
  if (!maybe_result->ToObject(&result)) return maybe_result;

  // Copy the characters into the new object, which may be either ASCII or
  // UTF-16.
  String* string_result = String::cast(result);
  for (int i = 0; i < string.length(); i++) {
    string_result->Set(i, string[i]);
  }
  return result;
}


Map* Heap::SymbolMapForString(String* string) {
  // If the string is in new space it cannot be used as a symbol.
  if (InNewSpace(string)) return NULL;

  // Find the corresponding symbol map for strings.
  Map* map = string->map();
  if (map == ascii_string_map()) return ascii_symbol_map();
  if (map == string_map()) return symbol_map();
  if (map == cons_string_map()) return cons_symbol_map();
  if (map == cons_ascii_string_map()) return cons_ascii_symbol_map();
  if (map == external_string_map()) return external_symbol_map();
  if (map == external_ascii_string_map()) return external_ascii_symbol_map();
  if (map == external_string_with_ascii_data_map()) {
    return external_symbol_with_ascii_data_map();
  }

  // No match found.
  return NULL;
}


MaybeObject* Heap::AllocateInternalSymbol(unibrow::CharacterStream* buffer,
                                          int chars,
                                          uint32_t hash_field) {
  ASSERT(chars >= 0);
  // Ensure the chars matches the number of characters in the buffer.
  ASSERT(static_cast<unsigned>(chars) == buffer->Length());
  // Determine whether the string is ascii.
  bool is_ascii = true;
  while (buffer->has_more()) {
    if (buffer->GetNext() > unibrow::Utf8::kMaxOneByteChar) {
      is_ascii = false;
      break;
    }
  }
  buffer->Rewind();

  // Compute map and object size.
  int size;
  Map* map;

  if (is_ascii) {
    if (chars > SeqAsciiString::kMaxLength) {
      return Failure::OutOfMemoryException();
    }
    map = ascii_symbol_map();
    size = SeqAsciiString::SizeFor(chars);
  } else {
    if (chars > SeqTwoByteString::kMaxLength) {
      return Failure::OutOfMemoryException();
    }
    map = symbol_map();
    size = SeqTwoByteString::SizeFor(chars);
  }

  // Allocate string.
  Object* result;
  { MaybeObject* maybe_result = (size > MaxObjectSizeInPagedSpace())
                   ? lo_space_->AllocateRaw(size)
                   : old_data_space_->AllocateRaw(size);
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }

  reinterpret_cast<HeapObject*>(result)->set_map(map);
  // Set length and hash fields of the allocated string.
  String* answer = String::cast(result);
  answer->set_length(chars);
  answer->set_hash_field(hash_field);

  ASSERT_EQ(size, answer->Size());

  // Fill in the characters.
  for (int i = 0; i < chars; i++) {
    answer->Set(i, buffer->GetNext());
  }
  return answer;
}


MaybeObject* Heap::AllocateRawAsciiString(int length, PretenureFlag pretenure) {
  if (length < 0 || length > SeqAsciiString::kMaxLength) {
    return Failure::OutOfMemoryException();
  }

  int size = SeqAsciiString::SizeFor(length);
  ASSERT(size <= SeqAsciiString::kMaxSize);

  AllocationSpace space = (pretenure == TENURED) ? OLD_DATA_SPACE : NEW_SPACE;
  AllocationSpace retry_space = OLD_DATA_SPACE;

  if (space == NEW_SPACE) {
    if (size > kMaxObjectSizeInNewSpace) {
      // Allocate in large object space, retry space will be ignored.
      space = LO_SPACE;
    } else if (size > MaxObjectSizeInPagedSpace()) {
      // Allocate in new space, retry in large object space.
      retry_space = LO_SPACE;
    }
  } else if (space == OLD_DATA_SPACE && size > MaxObjectSizeInPagedSpace()) {
    space = LO_SPACE;
  }
  Object* result;
  { MaybeObject* maybe_result = AllocateRaw(size, space, retry_space);
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }

  // Partially initialize the object.
  HeapObject::cast(result)->set_map(ascii_string_map());
  String::cast(result)->set_length(length);
  String::cast(result)->set_hash_field(String::kEmptyHashField);
  ASSERT_EQ(size, HeapObject::cast(result)->Size());
  return result;
}


MaybeObject* Heap::AllocateRawTwoByteString(int length,
                                            PretenureFlag pretenure) {
  if (length < 0 || length > SeqTwoByteString::kMaxLength) {
    return Failure::OutOfMemoryException();
  }
  int size = SeqTwoByteString::SizeFor(length);
  ASSERT(size <= SeqTwoByteString::kMaxSize);
  AllocationSpace space = (pretenure == TENURED) ? OLD_DATA_SPACE : NEW_SPACE;
  AllocationSpace retry_space = OLD_DATA_SPACE;

  if (space == NEW_SPACE) {
    if (size > kMaxObjectSizeInNewSpace) {
      // Allocate in large object space, retry space will be ignored.
      space = LO_SPACE;
    } else if (size > MaxObjectSizeInPagedSpace()) {
      // Allocate in new space, retry in large object space.
      retry_space = LO_SPACE;
    }
  } else if (space == OLD_DATA_SPACE && size > MaxObjectSizeInPagedSpace()) {
    space = LO_SPACE;
  }
  Object* result;
  { MaybeObject* maybe_result = AllocateRaw(size, space, retry_space);
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }

  // Partially initialize the object.
  HeapObject::cast(result)->set_map(string_map());
  String::cast(result)->set_length(length);
  String::cast(result)->set_hash_field(String::kEmptyHashField);
  ASSERT_EQ(size, HeapObject::cast(result)->Size());
  return result;
}


MaybeObject* Heap::AllocateEmptyFixedArray() {
  int size = FixedArray::SizeFor(0);
  Object* result;
  { MaybeObject* maybe_result =
        AllocateRaw(size, OLD_DATA_SPACE, OLD_DATA_SPACE);
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }
  // Initialize the object.
  reinterpret_cast<FixedArray*>(result)->set_map(fixed_array_map());
  reinterpret_cast<FixedArray*>(result)->set_length(0);
  return result;
}


MaybeObject* Heap::AllocateRawFixedArray(int length) {
  if (length < 0 || length > FixedArray::kMaxLength) {
    return Failure::OutOfMemoryException();
  }
  ASSERT(length > 0);
  // Use the general function if we're forced to always allocate.
  if (always_allocate()) return AllocateFixedArray(length, TENURED);
  // Allocate the raw data for a fixed array.
  int size = FixedArray::SizeFor(length);
  return size <= kMaxObjectSizeInNewSpace
      ? new_space_.AllocateRaw(size)
      : lo_space_->AllocateRawFixedArray(size);
}


MaybeObject* Heap::CopyFixedArrayWithMap(FixedArray* src, Map* map) {
  int len = src->length();
  Object* obj;
  { MaybeObject* maybe_obj = AllocateRawFixedArray(len);
    if (!maybe_obj->ToObject(&obj)) return maybe_obj;
  }
  if (Heap::InNewSpace(obj)) {
    HeapObject* dst = HeapObject::cast(obj);
    dst->set_map(map);
    CopyBlock(dst->address() + kPointerSize,
              src->address() + kPointerSize,
              FixedArray::SizeFor(len) - kPointerSize);
    return obj;
  }
  HeapObject::cast(obj)->set_map(map);
  FixedArray* result = FixedArray::cast(obj);
  result->set_length(len);

  // Copy the content
  AssertNoAllocation no_gc;
  WriteBarrierMode mode = result->GetWriteBarrierMode(no_gc);
  for (int i = 0; i < len; i++) result->set(i, src->get(i), mode);
  return result;
}


MaybeObject* Heap::AllocateFixedArray(int length) {
  ASSERT(length >= 0);
  if (length == 0) return empty_fixed_array();
  Object* result;
  { MaybeObject* maybe_result = AllocateRawFixedArray(length);
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }
  // Initialize header.
  FixedArray* array = reinterpret_cast<FixedArray*>(result);
  array->set_map(fixed_array_map());
  array->set_length(length);
  // Initialize body.
  ASSERT(!Heap::InNewSpace(undefined_value()));
  MemsetPointer(array->data_start(), undefined_value(), length);
  return result;
}


MaybeObject* Heap::AllocateRawFixedArray(int length, PretenureFlag pretenure) {
  if (length < 0 || length > FixedArray::kMaxLength) {
    return Failure::OutOfMemoryException();
  }

  AllocationSpace space =
      (pretenure == TENURED) ? OLD_POINTER_SPACE : NEW_SPACE;
  int size = FixedArray::SizeFor(length);
  if (space == NEW_SPACE && size > kMaxObjectSizeInNewSpace) {
    // Too big for new space.
    space = LO_SPACE;
  } else if (space == OLD_POINTER_SPACE &&
             size > MaxObjectSizeInPagedSpace()) {
    // Too big for old pointer space.
    space = LO_SPACE;
  }

  AllocationSpace retry_space =
      (size <= MaxObjectSizeInPagedSpace()) ? OLD_POINTER_SPACE : LO_SPACE;

  return AllocateRaw(size, space, retry_space);
}


MUST_USE_RESULT static MaybeObject* AllocateFixedArrayWithFiller(
    int length,
    PretenureFlag pretenure,
    Object* filler) {
  ASSERT(length >= 0);
  ASSERT(Heap::empty_fixed_array()->IsFixedArray());
  if (length == 0) return Heap::empty_fixed_array();

  ASSERT(!Heap::InNewSpace(filler));
  Object* result;
  { MaybeObject* maybe_result = Heap::AllocateRawFixedArray(length, pretenure);
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }

  HeapObject::cast(result)->set_map(Heap::fixed_array_map());
  FixedArray* array = FixedArray::cast(result);
  array->set_length(length);
  MemsetPointer(array->data_start(), filler, length);
  return array;
}


MaybeObject* Heap::AllocateFixedArray(int length, PretenureFlag pretenure) {
  return AllocateFixedArrayWithFiller(length, pretenure, undefined_value());
}


MaybeObject* Heap::AllocateFixedArrayWithHoles(int length,
                                               PretenureFlag pretenure) {
  return AllocateFixedArrayWithFiller(length, pretenure, the_hole_value());
}


MaybeObject* Heap::AllocateUninitializedFixedArray(int length) {
  if (length == 0) return empty_fixed_array();

  Object* obj;
  { MaybeObject* maybe_obj = AllocateRawFixedArray(length);
    if (!maybe_obj->ToObject(&obj)) return maybe_obj;
  }

  reinterpret_cast<FixedArray*>(obj)->set_map(fixed_array_map());
  FixedArray::cast(obj)->set_length(length);
  return obj;
}


MaybeObject* Heap::AllocateHashTable(int length, PretenureFlag pretenure) {
  Object* result;
  { MaybeObject* maybe_result = Heap::AllocateFixedArray(length, pretenure);
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }
  reinterpret_cast<HeapObject*>(result)->set_map(hash_table_map());
  ASSERT(result->IsHashTable());
  return result;
}


MaybeObject* Heap::AllocateGlobalContext() {
  Object* result;
  { MaybeObject* maybe_result =
        Heap::AllocateFixedArray(Context::GLOBAL_CONTEXT_SLOTS);
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }
  Context* context = reinterpret_cast<Context*>(result);
  context->set_map(global_context_map());
  ASSERT(context->IsGlobalContext());
  ASSERT(result->IsContext());
  return result;
}


MaybeObject* Heap::AllocateFunctionContext(int length, JSFunction* function) {
  ASSERT(length >= Context::MIN_CONTEXT_SLOTS);
  Object* result;
  { MaybeObject* maybe_result = Heap::AllocateFixedArray(length);
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }
  Context* context = reinterpret_cast<Context*>(result);
  context->set_map(context_map());
  context->set_closure(function);
  context->set_fcontext(context);
  context->set_previous(NULL);
  context->set_extension(NULL);
  context->set_global(function->context()->global());
  ASSERT(!context->IsGlobalContext());
  ASSERT(context->is_function_context());
  ASSERT(result->IsContext());
  return result;
}


MaybeObject* Heap::AllocateWithContext(Context* previous,
                                       JSObject* extension,
                                       bool is_catch_context) {
  Object* result;
  { MaybeObject* maybe_result =
        Heap::AllocateFixedArray(Context::MIN_CONTEXT_SLOTS);
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }
  Context* context = reinterpret_cast<Context*>(result);
  context->set_map(is_catch_context ? catch_context_map() : context_map());
  context->set_closure(previous->closure());
  context->set_fcontext(previous->fcontext());
  context->set_previous(previous);
  context->set_extension(extension);
  context->set_global(previous->global());
  ASSERT(!context->IsGlobalContext());
  ASSERT(!context->is_function_context());
  ASSERT(result->IsContext());
  return result;
}


MaybeObject* Heap::AllocateStruct(InstanceType type) {
  Map* map;
  switch (type) {
#define MAKE_CASE(NAME, Name, name) case NAME##_TYPE: map = name##_map(); break;
STRUCT_LIST(MAKE_CASE)
#undef MAKE_CASE
    default:
      UNREACHABLE();
      return Failure::InternalError();
  }
  int size = map->instance_size();
  AllocationSpace space =
      (size > MaxObjectSizeInPagedSpace()) ? LO_SPACE : OLD_POINTER_SPACE;
  Object* result;
  { MaybeObject* maybe_result = Heap::Allocate(map, space);
    if (!maybe_result->ToObject(&result)) return maybe_result;
  }
  Struct::cast(result)->InitializeBody(size);
  return result;
}


bool Heap::IdleNotification() {
  static const int kIdlesBeforeScavenge = 4;
  static const int kIdlesBeforeMarkSweep = 7;
  static const int kIdlesBeforeMarkCompact = 8;
  static const int kMaxIdleCount = kIdlesBeforeMarkCompact + 1;
  static const unsigned int kGCsBetweenCleanup = 4;
  static int number_idle_notifications = 0;
  static unsigned int last_gc_count = gc_count_;

  bool uncommit = true;
  bool finished = false;

  // Reset the number of idle notifications received when a number of
  // GCs have taken place. This allows another round of cleanup based
  // on idle notifications if enough work has been carried out to
  // provoke a number of garbage collections.
  if (gc_count_ - last_gc_count < kGCsBetweenCleanup) {
    number_idle_notifications =
        Min(number_idle_notifications + 1, kMaxIdleCount);
  } else {
    number_idle_notifications = 0;
    last_gc_count = gc_count_;
  }

  if (number_idle_notifications == kIdlesBeforeScavenge) {
    if (contexts_disposed_ > 0) {
      HistogramTimerScope scope(&Counters::gc_context);
      CollectAllGarbage(false);
    } else {
      CollectGarbage(NEW_SPACE);
    }
    new_space_.Shrink();
    last_gc_count = gc_count_;
  } else if (number_idle_notifications == kIdlesBeforeMarkSweep) {
    // Before doing the mark-sweep collections we clear the
    // compilation cache to avoid hanging on to source code and
    // generated code for cached functions.
    CompilationCache::Clear();

    CollectAllGarbage(false);
    new_space_.Shrink();
    last_gc_count = gc_count_;

  } else if (number_idle_notifications == kIdlesBeforeMarkCompact) {
    CollectAllGarbage(true);
    new_space_.Shrink();
    last_gc_count = gc_count_;
    finished = true;

  } else if (contexts_disposed_ > 0) {
    if (FLAG_expose_gc) {
      contexts_disposed_ = 0;
    } else {
      HistogramTimerScope scope(&Counters::gc_context);
      CollectAllGarbage(false);
      last_gc_count = gc_count_;
    }
    // If this is the first idle notification, we reset the
    // notification count to avoid letting idle notifications for
    // context disposal garbage collections start a potentially too
    // aggressive idle GC cycle.
    if (number_idle_notifications <= 1) {
      number_idle_notifications = 0;
      uncommit = false;
    }
  } else if (number_idle_notifications > kIdlesBeforeMarkCompact) {
    // If we have received more than kIdlesBeforeMarkCompact idle
    // notifications we do not perform any cleanup because we don't
    // expect to gain much by doing so.
    finished = true;
  }

  // Make sure that we have no pending context disposals and
  // conditionally uncommit from space.
  ASSERT(contexts_disposed_ == 0);
  if (uncommit) Heap::UncommitFromSpace();
  return finished;
}


#ifdef DEBUG

void Heap::Print() {
  if (!HasBeenSetup()) return;
  Top::PrintStack();
  AllSpaces spaces;
  for (Space* space = spaces.next(); space != NULL; space = spaces.next())
    space->Print();
}


void Heap::ReportCodeStatistics(const char* title) {
  PrintF(">>>>>> Code Stats (%s) >>>>>>\n", title);
  PagedSpace::ResetCodeStatistics();
  // We do not look for code in new space, map space, or old space.  If code
  // somehow ends up in those spaces, we would miss it here.
  code_space_->CollectCodeStatistics();
  lo_space_->CollectCodeStatistics();
  PagedSpace::ReportCodeStatistics();
}


// This function expects that NewSpace's allocated objects histogram is
// populated (via a call to CollectStatistics or else as a side effect of a
// just-completed scavenge collection).
void Heap::ReportHeapStatistics(const char* title) {
  USE(title);
  PrintF(">>>>>> =============== %s (%d) =============== >>>>>>\n",
         title, gc_count_);
  PrintF("mark-compact GC : %d\n", mc_count_);
  PrintF("old_gen_promotion_limit_ %" V8_PTR_PREFIX "d\n",
         old_gen_promotion_limit_);
  PrintF("old_gen_allocation_limit_ %" V8_PTR_PREFIX "d\n",
         old_gen_allocation_limit_);

  PrintF("\n");
  PrintF("Number of handles : %d\n", HandleScope::NumberOfHandles());
  GlobalHandles::PrintStats();
  PrintF("\n");

  PrintF("Heap statistics : ");
  MemoryAllocator::ReportStatistics();
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
  PrintF("Large object space : ");
  lo_space_->ReportStatistics();
  PrintF(">>>>>> ========================================= >>>>>>\n");
}

#endif  // DEBUG

bool Heap::Contains(HeapObject* value) {
  return Contains(value->address());
}


bool Heap::Contains(Address addr) {
  if (OS::IsOutsideAllocatedSpace(addr)) return false;
  return HasBeenSetup() &&
    (new_space_.ToSpaceContains(addr) ||
     old_pointer_space_->Contains(addr) ||
     old_data_space_->Contains(addr) ||
     code_space_->Contains(addr) ||
     map_space_->Contains(addr) ||
     cell_space_->Contains(addr) ||
     lo_space_->SlowContains(addr));
}


bool Heap::InSpace(HeapObject* value, AllocationSpace space) {
  return InSpace(value->address(), space);
}


bool Heap::InSpace(Address addr, AllocationSpace space) {
  if (OS::IsOutsideAllocatedSpace(addr)) return false;
  if (!HasBeenSetup()) return false;

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
    case LO_SPACE:
      return lo_space_->SlowContains(addr);
  }

  return false;
}


#ifdef DEBUG
static void DummyScavengePointer(HeapObject** p) {
}


static void VerifyPointersUnderWatermark(
    PagedSpace* space,
    DirtyRegionCallback visit_dirty_region) {
  PageIterator it(space, PageIterator::PAGES_IN_USE);

  while (it.has_next()) {
    Page* page = it.next();
    Address start = page->ObjectAreaStart();
    Address end = page->AllocationWatermark();

    Heap::IterateDirtyRegions(Page::kAllRegionsDirtyMarks,
                              start,
                              end,
                              visit_dirty_region,
                              &DummyScavengePointer);
  }
}


static void VerifyPointersUnderWatermark(LargeObjectSpace* space) {
  LargeObjectIterator it(space);
  for (HeapObject* object = it.next(); object != NULL; object = it.next()) {
    if (object->IsFixedArray()) {
      Address slot_address = object->address();
      Address end = object->address() + object->Size();

      while (slot_address < end) {
        HeapObject** slot = reinterpret_cast<HeapObject**>(slot_address);
        // When we are not in GC the Heap::InNewSpace() predicate
        // checks that pointers which satisfy predicate point into
        // the active semispace.
        Heap::InNewSpace(*slot);
        slot_address += kPointerSize;
      }
    }
  }
}


void Heap::Verify() {
  ASSERT(HasBeenSetup());

  VerifyPointersVisitor visitor;
  IterateRoots(&visitor, VISIT_ONLY_STRONG);

  new_space_.Verify();

  VerifyPointersAndDirtyRegionsVisitor dirty_regions_visitor;
  old_pointer_space_->Verify(&dirty_regions_visitor);
  map_space_->Verify(&dirty_regions_visitor);

  VerifyPointersUnderWatermark(old_pointer_space_,
                               &IteratePointersInDirtyRegion);
  VerifyPointersUnderWatermark(map_space_,
                               &IteratePointersInDirtyMapsRegion);
  VerifyPointersUnderWatermark(lo_space_);

  VerifyPageWatermarkValidity(old_pointer_space_, ALL_INVALID);
  VerifyPageWatermarkValidity(map_space_, ALL_INVALID);

  VerifyPointersVisitor no_dirty_regions_visitor;
  old_data_space_->Verify(&no_dirty_regions_visitor);
  code_space_->Verify(&no_dirty_regions_visitor);
  cell_space_->Verify(&no_dirty_regions_visitor);

  lo_space_->Verify();
}
#endif  // DEBUG


MaybeObject* Heap::LookupSymbol(Vector<const char> string) {
  Object* symbol = NULL;
  Object* new_table;
  { MaybeObject* maybe_new_table =
        symbol_table()->LookupSymbol(string, &symbol);
    if (!maybe_new_table->ToObject(&new_table)) return maybe_new_table;
  }
  // Can't use set_symbol_table because SymbolTable::cast knows that
  // SymbolTable is a singleton and checks for identity.
  roots_[kSymbolTableRootIndex] = new_table;
  ASSERT(symbol != NULL);
  return symbol;
}


MaybeObject* Heap::LookupAsciiSymbol(Vector<const char> string) {
  Object* symbol = NULL;
  Object* new_table;
  { MaybeObject* maybe_new_table =
        symbol_table()->LookupAsciiSymbol(string, &symbol);
    if (!maybe_new_table->ToObject(&new_table)) return maybe_new_table;
  }
  // Can't use set_symbol_table because SymbolTable::cast knows that
  // SymbolTable is a singleton and checks for identity.
  roots_[kSymbolTableRootIndex] = new_table;
  ASSERT(symbol != NULL);
  return symbol;
}


MaybeObject* Heap::LookupTwoByteSymbol(Vector<const uc16> string) {
  Object* symbol = NULL;
  Object* new_table;
  { MaybeObject* maybe_new_table =
        symbol_table()->LookupTwoByteSymbol(string, &symbol);
    if (!maybe_new_table->ToObject(&new_table)) return maybe_new_table;
  }
  // Can't use set_symbol_table because SymbolTable::cast knows that
  // SymbolTable is a singleton and checks for identity.
  roots_[kSymbolTableRootIndex] = new_table;
  ASSERT(symbol != NULL);
  return symbol;
}


MaybeObject* Heap::LookupSymbol(String* string) {
  if (string->IsSymbol()) return string;
  Object* symbol = NULL;
  Object* new_table;
  { MaybeObject* maybe_new_table =
        symbol_table()->LookupString(string, &symbol);
    if (!maybe_new_table->ToObject(&new_table)) return maybe_new_table;
  }
  // Can't use set_symbol_table because SymbolTable::cast knows that
  // SymbolTable is a singleton and checks for identity.
  roots_[kSymbolTableRootIndex] = new_table;
  ASSERT(symbol != NULL);
  return symbol;
}


bool Heap::LookupSymbolIfExists(String* string, String** symbol) {
  if (string->IsSymbol()) {
    *symbol = string;
    return true;
  }
  return symbol_table()->LookupSymbolIfExists(string, symbol);
}


#ifdef DEBUG
void Heap::ZapFromSpace() {
  ASSERT(reinterpret_cast<Object*>(kFromSpaceZapValue)->IsFailure());
  for (Address a = new_space_.FromSpaceLow();
       a < new_space_.FromSpaceHigh();
       a += kPointerSize) {
    Memory::Address_at(a) = kFromSpaceZapValue;
  }
}
#endif  // DEBUG


bool Heap::IteratePointersInDirtyRegion(Address start,
                                        Address end,
                                        ObjectSlotCallback copy_object_func) {
  Address slot_address = start;
  bool pointers_to_new_space_found = false;

  while (slot_address < end) {
    Object** slot = reinterpret_cast<Object**>(slot_address);
    if (Heap::InNewSpace(*slot)) {
      ASSERT((*slot)->IsHeapObject());
      copy_object_func(reinterpret_cast<HeapObject**>(slot));
      if (Heap::InNewSpace(*slot)) {
        ASSERT((*slot)->IsHeapObject());
        pointers_to_new_space_found = true;
      }
    }
    slot_address += kPointerSize;
  }
  return pointers_to_new_space_found;
}


// Compute start address of the first map following given addr.
static inline Address MapStartAlign(Address addr) {
  Address page = Page::FromAddress(addr)->ObjectAreaStart();
  return page + (((addr - page) + (Map::kSize - 1)) / Map::kSize * Map::kSize);
}


// Compute end address of the first map preceding given addr.
static inline Address MapEndAlign(Address addr) {
  Address page = Page::FromAllocationTop(addr)->ObjectAreaStart();
  return page + ((addr - page) / Map::kSize * Map::kSize);
}


static bool IteratePointersInDirtyMaps(Address start,
                                       Address end,
                                       ObjectSlotCallback copy_object_func) {
  ASSERT(MapStartAlign(start) == start);
  ASSERT(MapEndAlign(end) == end);

  Address map_address = start;
  bool pointers_to_new_space_found = false;

  while (map_address < end) {
    ASSERT(!Heap::InNewSpace(Memory::Object_at(map_address)));
    ASSERT(Memory::Object_at(map_address)->IsMap());

    Address pointer_fields_start = map_address + Map::kPointerFieldsBeginOffset;
    Address pointer_fields_end = map_address + Map::kPointerFieldsEndOffset;

    if (Heap::IteratePointersInDirtyRegion(pointer_fields_start,
                                           pointer_fields_end,
                                           copy_object_func)) {
      pointers_to_new_space_found = true;
    }

    map_address += Map::kSize;
  }

  return pointers_to_new_space_found;
}


bool Heap::IteratePointersInDirtyMapsRegion(
    Address start,
    Address end,
    ObjectSlotCallback copy_object_func) {
  Address map_aligned_start = MapStartAlign(start);
  Address map_aligned_end   = MapEndAlign(end);

  bool contains_pointers_to_new_space = false;

  if (map_aligned_start != start) {
    Address prev_map = map_aligned_start - Map::kSize;
    ASSERT(Memory::Object_at(prev_map)->IsMap());

    Address pointer_fields_start =
        Max(start, prev_map + Map::kPointerFieldsBeginOffset);

    Address pointer_fields_end =
        Min(prev_map + Map::kPointerFieldsEndOffset, end);

    contains_pointers_to_new_space =
      IteratePointersInDirtyRegion(pointer_fields_start,
                                   pointer_fields_end,
                                   copy_object_func)
        || contains_pointers_to_new_space;
  }

  contains_pointers_to_new_space =
    IteratePointersInDirtyMaps(map_aligned_start,
                               map_aligned_end,
                               copy_object_func)
      || contains_pointers_to_new_space;

  if (map_aligned_end != end) {
    ASSERT(Memory::Object_at(map_aligned_end)->IsMap());

    Address pointer_fields_start =
        map_aligned_end + Map::kPointerFieldsBeginOffset;

    Address pointer_fields_end =
        Min(end, map_aligned_end + Map::kPointerFieldsEndOffset);

    contains_pointers_to_new_space =
      IteratePointersInDirtyRegion(pointer_fields_start,
                                   pointer_fields_end,
                                   copy_object_func)
        || contains_pointers_to_new_space;
  }

  return contains_pointers_to_new_space;
}


void Heap::IterateAndMarkPointersToFromSpace(Address start,
                                             Address end,
                                             ObjectSlotCallback callback) {
  Address slot_address = start;
  Page* page = Page::FromAddress(start);

  uint32_t marks = page->GetRegionMarks();

  while (slot_address < end) {
    Object** slot = reinterpret_cast<Object**>(slot_address);
    if (Heap::InFromSpace(*slot)) {
      ASSERT((*slot)->IsHeapObject());
      callback(reinterpret_cast<HeapObject**>(slot));
      if (Heap::InNewSpace(*slot)) {
        ASSERT((*slot)->IsHeapObject());
        marks |= page->GetRegionMaskForAddress(slot_address);
      }
    }
    slot_address += kPointerSize;
  }

  page->SetRegionMarks(marks);
}


uint32_t Heap::IterateDirtyRegions(
    uint32_t marks,
    Address area_start,
    Address area_end,
    DirtyRegionCallback visit_dirty_region,
    ObjectSlotCallback copy_object_func) {
  uint32_t newmarks = 0;
  uint32_t mask = 1;

  if (area_start >= area_end) {
    return newmarks;
  }

  Address region_start = area_start;

  // area_start does not necessarily coincide with start of the first region.
  // Thus to calculate the beginning of the next region we have to align
  // area_start by Page::kRegionSize.
  Address second_region =
      reinterpret_cast<Address>(
          reinterpret_cast<intptr_t>(area_start + Page::kRegionSize) &
          ~Page::kRegionAlignmentMask);

  // Next region might be beyond area_end.
  Address region_end = Min(second_region, area_end);

  if (marks & mask) {
    if (visit_dirty_region(region_start, region_end, copy_object_func)) {
      newmarks |= mask;
    }
  }
  mask <<= 1;

  // Iterate subsequent regions which fully lay inside [area_start, area_end[.
  region_start = region_end;
  region_end = region_start + Page::kRegionSize;

  while (region_end <= area_end) {
    if (marks & mask) {
      if (visit_dirty_region(region_start, region_end, copy_object_func)) {
        newmarks |= mask;
      }
    }

    region_start = region_end;
    region_end = region_start + Page::kRegionSize;

    mask <<= 1;
  }

  if (region_start != area_end) {
    // A small piece of area left uniterated because area_end does not coincide
    // with region end. Check whether region covering last part of area is
    // dirty.
    if (marks & mask) {
      if (visit_dirty_region(region_start, area_end, copy_object_func)) {
        newmarks |= mask;
      }
    }
  }

  return newmarks;
}



void Heap::IterateDirtyRegions(
    PagedSpace* space,
    DirtyRegionCallback visit_dirty_region,
    ObjectSlotCallback copy_object_func,
    ExpectedPageWatermarkState expected_page_watermark_state) {

  PageIterator it(space, PageIterator::PAGES_IN_USE);

  while (it.has_next()) {
    Page* page = it.next();
    uint32_t marks = page->GetRegionMarks();

    if (marks != Page::kAllRegionsCleanMarks) {
      Address start = page->ObjectAreaStart();

      // Do not try to visit pointers beyond page allocation watermark.
      // Page can contain garbage pointers there.
      Address end;

      if ((expected_page_watermark_state == WATERMARK_SHOULD_BE_VALID) ||
          page->IsWatermarkValid()) {
        end = page->AllocationWatermark();
      } else {
        end = page->CachedAllocationWatermark();
      }

      ASSERT(space == old_pointer_space_ ||
             (space == map_space_ &&
              ((page->ObjectAreaStart() - end) % Map::kSize == 0)));

      page->SetRegionMarks(IterateDirtyRegions(marks,
                                               start,
                                               end,
                                               visit_dirty_region,
                                               copy_object_func));
    }

    // Mark page watermark as invalid to maintain watermark validity invariant.
    // See Page::FlipMeaningOfInvalidatedWatermarkFlag() for details.
    page->InvalidateWatermark(true);
  }
}


void Heap::IterateRoots(ObjectVisitor* v, VisitMode mode) {
  IterateStrongRoots(v, mode);
  IterateWeakRoots(v, mode);
}


void Heap::IterateWeakRoots(ObjectVisitor* v, VisitMode mode) {
  v->VisitPointer(reinterpret_cast<Object**>(&roots_[kSymbolTableRootIndex]));
  v->Synchronize("symbol_table");
  if (mode != VISIT_ALL_IN_SCAVENGE) {
    // Scavenge collections have special processing for this.
    ExternalStringTable::Iterate(v);
  }
  v->Synchronize("external_string_table");
}


void Heap::IterateStrongRoots(ObjectVisitor* v, VisitMode mode) {
  v->VisitPointers(&roots_[0], &roots_[kStrongRootListLength]);
  v->Synchronize("strong_root_list");

  v->VisitPointer(BitCast<Object**>(&hidden_symbol_));
  v->Synchronize("symbol");

  Bootstrapper::Iterate(v);
  v->Synchronize("bootstrapper");
  Top::Iterate(v);
  v->Synchronize("top");
  Relocatable::Iterate(v);
  v->Synchronize("relocatable");

#ifdef ENABLE_DEBUGGER_SUPPORT
  Debug::Iterate(v);
#endif
  v->Synchronize("debug");
  CompilationCache::Iterate(v);
  v->Synchronize("compilationcache");

  // Iterate over local handles in handle scopes.
  HandleScopeImplementer::Iterate(v);
  v->Synchronize("handlescope");

  // Iterate over the builtin code objects and code stubs in the
  // heap. Note that it is not necessary to iterate over code objects
  // on scavenge collections.
  if (mode != VISIT_ALL_IN_SCAVENGE) {
    Builtins::IterateBuiltins(v);
  }
  v->Synchronize("builtins");

  // Iterate over global handles.
  if (mode == VISIT_ONLY_STRONG) {
    GlobalHandles::IterateStrongRoots(v);
  } else {
    GlobalHandles::IterateAllRoots(v);
  }
  v->Synchronize("globalhandles");

  // Iterate over pointers being held by inactive threads.
  ThreadManager::Iterate(v);
  v->Synchronize("threadmanager");

  // Iterate over the pointers the Serialization/Deserialization code is
  // holding.
  // During garbage collection this keeps the partial snapshot cache alive.
  // During deserialization of the startup snapshot this creates the partial
  // snapshot cache and deserializes the objects it refers to.  During
  // serialization this does nothing, since the partial snapshot cache is
  // empty.  However the next thing we do is create the partial snapshot,
  // filling up the partial snapshot cache with objects it needs as we go.
  SerializerDeserializer::Iterate(v);
  // We don't do a v->Synchronize call here, because in debug mode that will
  // output a flag to the snapshot.  However at this point the serializer and
  // deserializer are deliberately a little unsynchronized (see above) so the
  // checking of the sync flag in the snapshot would fail.
}


// Flag is set when the heap has been configured.  The heap can be repeatedly
// configured through the API until it is setup.
static bool heap_configured = false;

// TODO(1236194): Since the heap size is configurable on the command line
// and through the API, we should gracefully handle the case that the heap
// size is not big enough to fit all the initial objects.
bool Heap::ConfigureHeap(int max_semispace_size,
                         int max_old_gen_size,
                         int max_executable_size) {
  if (HasBeenSetup()) return false;

  if (max_semispace_size > 0) max_semispace_size_ = max_semispace_size;

  if (Snapshot::IsEnabled()) {
    // If we are using a snapshot we always reserve the default amount
    // of memory for each semispace because code in the snapshot has
    // write-barrier code that relies on the size and alignment of new
    // space.  We therefore cannot use a larger max semispace size
    // than the default reserved semispace size.
    if (max_semispace_size_ > reserved_semispace_size_) {
      max_semispace_size_ = reserved_semispace_size_;
    }
  } else {
    // If we are not using snapshots we reserve space for the actual
    // max semispace size.
    reserved_semispace_size_ = max_semispace_size_;
  }

  if (max_old_gen_size > 0) max_old_generation_size_ = max_old_gen_size;
  if (max_executable_size > 0) {
    max_executable_size_ = RoundUp(max_executable_size, Page::kPageSize);
  }

  // The max executable size must be less than or equal to the max old
  // generation size.
  if (max_executable_size_ > max_old_generation_size_) {
    max_executable_size_ = max_old_generation_size_;
  }

  // The new space size must be a power of two to support single-bit testing
  // for containment.
  max_semispace_size_ = RoundUpToPowerOf2(max_semispace_size_);
  reserved_semispace_size_ = RoundUpToPowerOf2(reserved_semispace_size_);
  initial_semispace_size_ = Min(initial_semispace_size_, max_semispace_size_);
  external_allocation_limit_ = 10 * max_semispace_size_;

  // The old generation is paged.
  max_old_generation_size_ = RoundUp(max_old_generation_size_, Page::kPageSize);

  heap_configured = true;
  return true;
}


bool Heap::ConfigureHeapDefault() {
  return ConfigureHeap(FLAG_max_new_space_size / 2 * KB,
                       FLAG_max_old_space_size * MB,
                       FLAG_max_executable_size * MB);
}


void Heap::RecordStats(HeapStats* stats, bool take_snapshot) {
  *stats->start_marker = HeapStats::kStartMarker;
  *stats->end_marker = HeapStats::kEndMarker;
  *stats->new_space_size = new_space_.SizeAsInt();
  *stats->new_space_capacity = static_cast<int>(new_space_.Capacity());
  *stats->old_pointer_space_size = old_pointer_space_->Size();
  *stats->old_pointer_space_capacity = old_pointer_space_->Capacity();
  *stats->old_data_space_size = old_data_space_->Size();
  *stats->old_data_space_capacity = old_data_space_->Capacity();
  *stats->code_space_size = code_space_->Size();
  *stats->code_space_capacity = code_space_->Capacity();
  *stats->map_space_size = map_space_->Size();
  *stats->map_space_capacity = map_space_->Capacity();
  *stats->cell_space_size = cell_space_->Size();
  *stats->cell_space_capacity = cell_space_->Capacity();
  *stats->lo_space_size = lo_space_->Size();
  GlobalHandles::RecordStats(stats);
  *stats->memory_allocator_size = MemoryAllocator::Size();
  *stats->memory_allocator_capacity =
      MemoryAllocator::Size() + MemoryAllocator::Available();
  *stats->os_error = OS::GetLastError();
  if (take_snapshot) {
    HeapIterator iterator(HeapIterator::kFilterFreeListNodes);
    for (HeapObject* obj = iterator.next();
         obj != NULL;
         obj = iterator.next()) {
      InstanceType type = obj->map()->instance_type();
      ASSERT(0 <= type && type <= LAST_TYPE);
      stats->objects_per_type[type]++;
      stats->size_per_type[type] += obj->Size();
    }
  }
}


intptr_t Heap::PromotedSpaceSize() {
  return old_pointer_space_->Size()
      + old_data_space_->Size()
      + code_space_->Size()
      + map_space_->Size()
      + cell_space_->Size()
      + lo_space_->Size();
}


int Heap::PromotedExternalMemorySize() {
  if (amount_of_external_allocated_memory_
      <= amount_of_external_allocated_memory_at_last_global_gc_) return 0;
  return amount_of_external_allocated_memory_
      - amount_of_external_allocated_memory_at_last_global_gc_;
}


bool Heap::Setup(bool create_heap_objects) {
  // Initialize heap spaces and initial maps and objects. Whenever something
  // goes wrong, just return false. The caller should check the results and
  // call Heap::TearDown() to release allocated memory.
  //
  // If the heap is not yet configured (eg, through the API), configure it.
  // Configuration is based on the flags new-space-size (really the semispace
  // size) and old-space-size if set or the initial values of semispace_size_
  // and old_generation_size_ otherwise.
  if (!heap_configured) {
    if (!ConfigureHeapDefault()) return false;
  }

  ScavengingVisitor::Initialize();
  NewSpaceScavenger::Initialize();
  MarkCompactCollector::Initialize();

  MarkMapPointersAsEncoded(false);

  // Setup memory allocator and reserve a chunk of memory for new
  // space.  The chunk is double the size of the requested reserved
  // new space size to ensure that we can find a pair of semispaces that
  // are contiguous and aligned to their size.
  if (!MemoryAllocator::Setup(MaxReserved(), MaxExecutableSize())) return false;
  void* chunk =
      MemoryAllocator::ReserveInitialChunk(4 * reserved_semispace_size_);
  if (chunk == NULL) return false;

  // Align the pair of semispaces to their size, which must be a power
  // of 2.
  Address new_space_start =
      RoundUp(reinterpret_cast<byte*>(chunk), 2 * reserved_semispace_size_);
  if (!new_space_.Setup(new_space_start, 2 * reserved_semispace_size_)) {
    return false;
  }

  // Initialize old pointer space.
  old_pointer_space_ =
      new OldSpace(max_old_generation_size_, OLD_POINTER_SPACE, NOT_EXECUTABLE);
  if (old_pointer_space_ == NULL) return false;
  if (!old_pointer_space_->Setup(NULL, 0)) return false;

  // Initialize old data space.
  old_data_space_ =
      new OldSpace(max_old_generation_size_, OLD_DATA_SPACE, NOT_EXECUTABLE);
  if (old_data_space_ == NULL) return false;
  if (!old_data_space_->Setup(NULL, 0)) return false;

  // Initialize the code space, set its maximum capacity to the old
  // generation size. It needs executable memory.
  // On 64-bit platform(s), we put all code objects in a 2 GB range of
  // virtual address space, so that they can call each other with near calls.
  if (code_range_size_ > 0) {
    if (!CodeRange::Setup(code_range_size_)) {
      return false;
    }
  }

  code_space_ =
      new OldSpace(max_old_generation_size_, CODE_SPACE, EXECUTABLE);
  if (code_space_ == NULL) return false;
  if (!code_space_->Setup(NULL, 0)) return false;

  // Initialize map space.
  map_space_ = new MapSpace(FLAG_use_big_map_space
      ? max_old_generation_size_
      : MapSpace::kMaxMapPageIndex * Page::kPageSize,
      FLAG_max_map_space_pages,
      MAP_SPACE);
  if (map_space_ == NULL) return false;
  if (!map_space_->Setup(NULL, 0)) return false;

  // Initialize global property cell space.
  cell_space_ = new CellSpace(max_old_generation_size_, CELL_SPACE);
  if (cell_space_ == NULL) return false;
  if (!cell_space_->Setup(NULL, 0)) return false;

  // The large object code space may contain code or data.  We set the memory
  // to be non-executable here for safety, but this means we need to enable it
  // explicitly when allocating large code objects.
  lo_space_ = new LargeObjectSpace(LO_SPACE);
  if (lo_space_ == NULL) return false;
  if (!lo_space_->Setup()) return false;

  if (create_heap_objects) {
    // Create initial maps.
    if (!CreateInitialMaps()) return false;
    if (!CreateApiObjects()) return false;

    // Create initial objects
    if (!CreateInitialObjects()) return false;

    global_contexts_list_ = undefined_value();
  }

  LOG(IntPtrTEvent("heap-capacity", Capacity()));
  LOG(IntPtrTEvent("heap-available", Available()));

#ifdef ENABLE_LOGGING_AND_PROFILING
  // This should be called only after initial objects have been created.
  ProducerHeapProfile::Setup();
#endif

  return true;
}


void Heap::SetStackLimits() {
  // On 64 bit machines, pointers are generally out of range of Smis.  We write
  // something that looks like an out of range Smi to the GC.

  // Set up the special root array entries containing the stack limits.
  // These are actually addresses, but the tag makes the GC ignore it.
  roots_[kStackLimitRootIndex] =
      reinterpret_cast<Object*>(
          (StackGuard::jslimit() & ~kSmiTagMask) | kSmiTag);
  roots_[kRealStackLimitRootIndex] =
      reinterpret_cast<Object*>(
          (StackGuard::real_jslimit() & ~kSmiTagMask) | kSmiTag);
}


void Heap::TearDown() {
  if (FLAG_print_cumulative_gc_stat) {
    PrintF("\n\n");
    PrintF("gc_count=%d ", gc_count_);
    PrintF("mark_sweep_count=%d ", ms_count_);
    PrintF("mark_compact_count=%d ", mc_count_);
    PrintF("max_gc_pause=%d ", GCTracer::get_max_gc_pause());
    PrintF("min_in_mutator=%d ", GCTracer::get_min_in_mutator());
    PrintF("max_alive_after_gc=%" V8_PTR_PREFIX "d ",
           GCTracer::get_max_alive_after_gc());
    PrintF("\n\n");
  }

  GlobalHandles::TearDown();

  ExternalStringTable::TearDown();

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

  if (lo_space_ != NULL) {
    lo_space_->TearDown();
    delete lo_space_;
    lo_space_ = NULL;
  }

  MemoryAllocator::TearDown();
}


void Heap::Shrink() {
  // Try to shrink all paged spaces.
  PagedSpaces spaces;
  for (PagedSpace* space = spaces.next(); space != NULL; space = spaces.next())
    space->Shrink();
}


#ifdef ENABLE_HEAP_PROTECTION

void Heap::Protect() {
  if (HasBeenSetup()) {
    AllSpaces spaces;
    for (Space* space = spaces.next(); space != NULL; space = spaces.next())
      space->Protect();
  }
}


void Heap::Unprotect() {
  if (HasBeenSetup()) {
    AllSpaces spaces;
    for (Space* space = spaces.next(); space != NULL; space = spaces.next())
      space->Unprotect();
  }
}

#endif


void Heap::AddGCPrologueCallback(GCPrologueCallback callback, GCType gc_type) {
  ASSERT(callback != NULL);
  GCPrologueCallbackPair pair(callback, gc_type);
  ASSERT(!gc_prologue_callbacks_.Contains(pair));
  return gc_prologue_callbacks_.Add(pair);
}


void Heap::RemoveGCPrologueCallback(GCPrologueCallback callback) {
  ASSERT(callback != NULL);
  for (int i = 0; i < gc_prologue_callbacks_.length(); ++i) {
    if (gc_prologue_callbacks_[i].callback == callback) {
      gc_prologue_callbacks_.Remove(i);
      return;
    }
  }
  UNREACHABLE();
}


void Heap::AddGCEpilogueCallback(GCEpilogueCallback callback, GCType gc_type) {
  ASSERT(callback != NULL);
  GCEpilogueCallbackPair pair(callback, gc_type);
  ASSERT(!gc_epilogue_callbacks_.Contains(pair));
  return gc_epilogue_callbacks_.Add(pair);
}


void Heap::RemoveGCEpilogueCallback(GCEpilogueCallback callback) {
  ASSERT(callback != NULL);
  for (int i = 0; i < gc_epilogue_callbacks_.length(); ++i) {
    if (gc_epilogue_callbacks_[i].callback == callback) {
      gc_epilogue_callbacks_.Remove(i);
      return;
    }
  }
  UNREACHABLE();
}


#ifdef DEBUG

class PrintHandleVisitor: public ObjectVisitor {
 public:
  void VisitPointers(Object** start, Object** end) {
    for (Object** p = start; p < end; p++)
      PrintF("  handle %p to %p\n",
             reinterpret_cast<void*>(p),
             reinterpret_cast<void*>(*p));
  }
};

void Heap::PrintHandles() {
  PrintF("Handles:\n");
  PrintHandleVisitor v;
  HandleScopeImplementer::Iterate(&v);
}

#endif


Space* AllSpaces::next() {
  switch (counter_++) {
    case NEW_SPACE:
      return Heap::new_space();
    case OLD_POINTER_SPACE:
      return Heap::old_pointer_space();
    case OLD_DATA_SPACE:
      return Heap::old_data_space();
    case CODE_SPACE:
      return Heap::code_space();
    case MAP_SPACE:
      return Heap::map_space();
    case CELL_SPACE:
      return Heap::cell_space();
    case LO_SPACE:
      return Heap::lo_space();
    default:
      return NULL;
  }
}


PagedSpace* PagedSpaces::next() {
  switch (counter_++) {
    case OLD_POINTER_SPACE:
      return Heap::old_pointer_space();
    case OLD_DATA_SPACE:
      return Heap::old_data_space();
    case CODE_SPACE:
      return Heap::code_space();
    case MAP_SPACE:
      return Heap::map_space();
    case CELL_SPACE:
      return Heap::cell_space();
    default:
      return NULL;
  }
}



OldSpace* OldSpaces::next() {
  switch (counter_++) {
    case OLD_POINTER_SPACE:
      return Heap::old_pointer_space();
    case OLD_DATA_SPACE:
      return Heap::old_data_space();
    case CODE_SPACE:
      return Heap::code_space();
    default:
      return NULL;
  }
}


SpaceIterator::SpaceIterator()
    : current_space_(FIRST_SPACE),
      iterator_(NULL),
      size_func_(NULL) {
}


SpaceIterator::SpaceIterator(HeapObjectCallback size_func)
    : current_space_(FIRST_SPACE),
      iterator_(NULL),
      size_func_(size_func) {
}


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
  ASSERT(iterator_ == NULL);

  switch (current_space_) {
    case NEW_SPACE:
      iterator_ = new SemiSpaceIterator(Heap::new_space(), size_func_);
      break;
    case OLD_POINTER_SPACE:
      iterator_ = new HeapObjectIterator(Heap::old_pointer_space(), size_func_);
      break;
    case OLD_DATA_SPACE:
      iterator_ = new HeapObjectIterator(Heap::old_data_space(), size_func_);
      break;
    case CODE_SPACE:
      iterator_ = new HeapObjectIterator(Heap::code_space(), size_func_);
      break;
    case MAP_SPACE:
      iterator_ = new HeapObjectIterator(Heap::map_space(), size_func_);
      break;
    case CELL_SPACE:
      iterator_ = new HeapObjectIterator(Heap::cell_space(), size_func_);
      break;
    case LO_SPACE:
      iterator_ = new LargeObjectIterator(Heap::lo_space(), size_func_);
      break;
  }

  // Return the newly allocated iterator;
  ASSERT(iterator_ != NULL);
  return iterator_;
}


class HeapObjectsFilter {
 public:
  virtual ~HeapObjectsFilter() {}
  virtual bool SkipObject(HeapObject* object) = 0;
};


class FreeListNodesFilter : public HeapObjectsFilter {
 public:
  FreeListNodesFilter() {
    MarkFreeListNodes();
  }

  bool SkipObject(HeapObject* object) {
    if (object->IsMarked()) {
      object->ClearMark();
      return true;
    } else {
      return false;
    }
  }

 private:
  void MarkFreeListNodes() {
    Heap::old_pointer_space()->MarkFreeListNodes();
    Heap::old_data_space()->MarkFreeListNodes();
    MarkCodeSpaceFreeListNodes();
    Heap::map_space()->MarkFreeListNodes();
    Heap::cell_space()->MarkFreeListNodes();
  }

  void MarkCodeSpaceFreeListNodes() {
    // For code space, using FreeListNode::IsFreeListNode is OK.
    HeapObjectIterator iter(Heap::code_space());
    for (HeapObject* obj = iter.next_object();
         obj != NULL;
         obj = iter.next_object()) {
      if (FreeListNode::IsFreeListNode(obj)) obj->SetMark();
    }
  }

  AssertNoAllocation no_alloc;
};


class UnreachableObjectsFilter : public HeapObjectsFilter {
 public:
  UnreachableObjectsFilter() {
    MarkUnreachableObjects();
  }

  bool SkipObject(HeapObject* object) {
    if (object->IsMarked()) {
      object->ClearMark();
      return true;
    } else {
      return false;
    }
  }

 private:
  class UnmarkingVisitor : public ObjectVisitor {
   public:
    UnmarkingVisitor() : list_(10) {}

    void VisitPointers(Object** start, Object** end) {
      for (Object** p = start; p < end; p++) {
        if (!(*p)->IsHeapObject()) continue;
        HeapObject* obj = HeapObject::cast(*p);
        if (obj->IsMarked()) {
          obj->ClearMark();
          list_.Add(obj);
        }
      }
    }

    bool can_process() { return !list_.is_empty(); }

    void ProcessNext() {
      HeapObject* obj = list_.RemoveLast();
      obj->Iterate(this);
    }

   private:
    List<HeapObject*> list_;
  };

  void MarkUnreachableObjects() {
    HeapIterator iterator;
    for (HeapObject* obj = iterator.next();
         obj != NULL;
         obj = iterator.next()) {
      obj->SetMark();
    }
    UnmarkingVisitor visitor;
    Heap::IterateRoots(&visitor, VISIT_ALL);
    while (visitor.can_process())
      visitor.ProcessNext();
  }

  AssertNoAllocation no_alloc;
};


HeapIterator::HeapIterator()
    : filtering_(HeapIterator::kNoFiltering),
      filter_(NULL) {
  Init();
}


HeapIterator::HeapIterator(HeapIterator::HeapObjectsFiltering filtering)
    : filtering_(filtering),
      filter_(NULL) {
  Init();
}


HeapIterator::~HeapIterator() {
  Shutdown();
}


void HeapIterator::Init() {
  // Start the iteration.
  space_iterator_ = filtering_ == kNoFiltering ? new SpaceIterator :
      new SpaceIterator(MarkCompactCollector::SizeOfMarkedObject);
  switch (filtering_) {
    case kFilterFreeListNodes:
      filter_ = new FreeListNodesFilter;
      break;
    case kFilterUnreachable:
      filter_ = new UnreachableObjectsFilter;
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
    ASSERT(object_iterator_ == NULL);
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


#if defined(DEBUG) || defined(LIVE_OBJECT_LIST)

Object* const PathTracer::kAnyGlobalObject = reinterpret_cast<Object*>(NULL);

class PathTracer::MarkVisitor: public ObjectVisitor {
 public:
  explicit MarkVisitor(PathTracer* tracer) : tracer_(tracer) {}
  void VisitPointers(Object** start, Object** end) {
    // Scan all HeapObject pointers in [start, end)
    for (Object** p = start; !tracer_->found() && (p < end); p++) {
      if ((*p)->IsHeapObject())
        tracer_->MarkRecursively(p, this);
    }
  }

 private:
  PathTracer* tracer_;
};


class PathTracer::UnmarkVisitor: public ObjectVisitor {
 public:
  explicit UnmarkVisitor(PathTracer* tracer) : tracer_(tracer) {}
  void VisitPointers(Object** start, Object** end) {
    // Scan all HeapObject pointers in [start, end)
    for (Object** p = start; p < end; p++) {
      if ((*p)->IsHeapObject())
        tracer_->UnmarkRecursively(p, this);
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
  ASSERT((search_target_ == kAnyGlobalObject) ||
         search_target_->IsHeapObject());
  found_target_in_trace_ = false;
  object_stack_.Clear();

  MarkVisitor mark_visitor(this);
  MarkRecursively(root, &mark_visitor);

  UnmarkVisitor unmark_visitor(this);
  UnmarkRecursively(root, &unmark_visitor);

  ProcessResults();
}


void PathTracer::MarkRecursively(Object** p, MarkVisitor* mark_visitor) {
  if (!(*p)->IsHeapObject()) return;

  HeapObject* obj = HeapObject::cast(*p);

  Object* map = obj->map();

  if (!map->IsHeapObject()) return;  // visited before

  if (found_target_in_trace_) return;  // stop if target found
  object_stack_.Add(obj);
  if (((search_target_ == kAnyGlobalObject) && obj->IsJSGlobalObject()) ||
      (obj == search_target_)) {
    found_target_in_trace_ = true;
    found_target_ = true;
    return;
  }

  bool is_global_context = obj->IsGlobalContext();

  // not visited yet
  Map* map_p = reinterpret_cast<Map*>(HeapObject::cast(map));

  Address map_addr = map_p->address();

  obj->set_map(reinterpret_cast<Map*>(map_addr + kMarkTag));

  // Scan the object body.
  if (is_global_context && (visit_mode_ == VISIT_ONLY_STRONG)) {
    // This is specialized to scan Context's properly.
    Object** start = reinterpret_cast<Object**>(obj->address() +
                                                Context::kHeaderSize);
    Object** end = reinterpret_cast<Object**>(obj->address() +
        Context::kHeaderSize + Context::FIRST_WEAK_SLOT * kPointerSize);
    mark_visitor->VisitPointers(start, end);
  } else {
    obj->IterateBody(map_p->instance_type(),
                     obj->SizeFromMap(map_p),
                     mark_visitor);
  }

  // Scan the map after the body because the body is a lot more interesting
  // when doing leak detection.
  MarkRecursively(&map, mark_visitor);

  if (!found_target_in_trace_)  // don't pop if found the target
    object_stack_.RemoveLast();
}


void PathTracer::UnmarkRecursively(Object** p, UnmarkVisitor* unmark_visitor) {
  if (!(*p)->IsHeapObject()) return;

  HeapObject* obj = HeapObject::cast(*p);

  Object* map = obj->map();

  if (map->IsHeapObject()) return;  // unmarked already

  Address map_addr = reinterpret_cast<Address>(map);

  map_addr -= kMarkTag;

  ASSERT_TAG_ALIGNED(map_addr);

  HeapObject* map_p = HeapObject::FromAddress(map_addr);

  obj->set_map(reinterpret_cast<Map*>(map_p));

  UnmarkRecursively(reinterpret_cast<Object**>(&map_p), unmark_visitor);

  obj->IterateBody(Map::cast(map_p)->instance_type(),
                   obj->SizeFromMap(Map::cast(map_p)),
                   unmark_visitor);
}


void PathTracer::ProcessResults() {
  if (found_target_) {
    PrintF("=====================================\n");
    PrintF("====        Path to object       ====\n");
    PrintF("=====================================\n\n");

    ASSERT(!object_stack_.is_empty());
    for (int i = 0; i < object_stack_.length(); i++) {
      if (i > 0) PrintF("\n     |\n     |\n     V\n\n");
      Object* obj = object_stack_[i];
#ifdef OBJECT_PRINT
      obj->Print();
#else
      obj->ShortPrint();
#endif
    }
    PrintF("=====================================\n");
  }
}
#endif  // DEBUG || LIVE_OBJECT_LIST


#ifdef DEBUG
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
  PathTracer tracer(PathTracer::kAnyGlobalObject,
                    PathTracer::FIND_ALL,
                    VISIT_ALL);
  IterateRoots(&tracer, VISIT_ONLY_STRONG);
}
#endif


static intptr_t CountTotalHolesSize() {
  intptr_t holes_size = 0;
  OldSpaces spaces;
  for (OldSpace* space = spaces.next();
       space != NULL;
       space = spaces.next()) {
    holes_size += space->Waste() + space->AvailableFree();
  }
  return holes_size;
}


GCTracer::GCTracer()
    : start_time_(0.0),
      start_size_(0),
      gc_count_(0),
      full_gc_count_(0),
      is_compacting_(false),
      marked_count_(0),
      allocated_since_last_gc_(0),
      spent_in_mutator_(0),
      promoted_objects_size_(0) {
  // These two fields reflect the state of the previous full collection.
  // Set them before they are changed by the collector.
  previous_has_compacted_ = MarkCompactCollector::HasCompacted();
  previous_marked_count_ = MarkCompactCollector::previous_marked_count();
  if (!FLAG_trace_gc && !FLAG_print_cumulative_gc_stat) return;
  start_time_ = OS::TimeCurrentMillis();
  start_size_ = Heap::SizeOfObjects();

  for (int i = 0; i < Scope::kNumberOfScopes; i++) {
    scopes_[i] = 0;
  }

  in_free_list_or_wasted_before_gc_ = CountTotalHolesSize();

  allocated_since_last_gc_ = Heap::SizeOfObjects() - alive_after_last_gc_;

  if (last_gc_end_timestamp_ > 0) {
    spent_in_mutator_ = Max(start_time_ - last_gc_end_timestamp_, 0.0);
  }
}


GCTracer::~GCTracer() {
  // Printf ONE line iff flag is set.
  if (!FLAG_trace_gc && !FLAG_print_cumulative_gc_stat) return;

  bool first_gc = (last_gc_end_timestamp_ == 0);

  alive_after_last_gc_ = Heap::SizeOfObjects();
  last_gc_end_timestamp_ = OS::TimeCurrentMillis();

  int time = static_cast<int>(last_gc_end_timestamp_ - start_time_);

  // Update cumulative GC statistics if required.
  if (FLAG_print_cumulative_gc_stat) {
    max_gc_pause_ = Max(max_gc_pause_, time);
    max_alive_after_gc_ = Max(max_alive_after_gc_, alive_after_last_gc_);
    if (!first_gc) {
      min_in_mutator_ = Min(min_in_mutator_,
                            static_cast<int>(spent_in_mutator_));
    }
  }

  if (!FLAG_trace_gc_nvp) {
    int external_time = static_cast<int>(scopes_[Scope::EXTERNAL]);

    PrintF("%s %.1f -> %.1f MB, ",
           CollectorString(),
           static_cast<double>(start_size_) / MB,
           SizeOfHeapObjects());

    if (external_time > 0) PrintF("%d / ", external_time);
    PrintF("%d ms.\n", time);
  } else {
    PrintF("pause=%d ", time);
    PrintF("mutator=%d ",
           static_cast<int>(spent_in_mutator_));

    PrintF("gc=");
    switch (collector_) {
      case SCAVENGER:
        PrintF("s");
        break;
      case MARK_COMPACTOR:
        PrintF(MarkCompactCollector::HasCompacted() ? "mc" : "ms");
        break;
      default:
        UNREACHABLE();
    }
    PrintF(" ");

    PrintF("external=%d ", static_cast<int>(scopes_[Scope::EXTERNAL]));
    PrintF("mark=%d ", static_cast<int>(scopes_[Scope::MC_MARK]));
    PrintF("sweep=%d ", static_cast<int>(scopes_[Scope::MC_SWEEP]));
    PrintF("sweepns=%d ", static_cast<int>(scopes_[Scope::MC_SWEEP_NEWSPACE]));
    PrintF("compact=%d ", static_cast<int>(scopes_[Scope::MC_COMPACT]));

    PrintF("total_size_before=%" V8_PTR_PREFIX "d ", start_size_);
    PrintF("total_size_after=%" V8_PTR_PREFIX "d ", Heap::SizeOfObjects());
    PrintF("holes_size_before=%" V8_PTR_PREFIX "d ",
           in_free_list_or_wasted_before_gc_);
    PrintF("holes_size_after=%" V8_PTR_PREFIX "d ", CountTotalHolesSize());

    PrintF("allocated=%" V8_PTR_PREFIX "d ", allocated_since_last_gc_);
    PrintF("promoted=%" V8_PTR_PREFIX "d ", promoted_objects_size_);

    PrintF("\n");
  }

#if defined(ENABLE_LOGGING_AND_PROFILING)
  Heap::PrintShortHeapStatistics();
#endif
}


const char* GCTracer::CollectorString() {
  switch (collector_) {
    case SCAVENGER:
      return "Scavenge";
    case MARK_COMPACTOR:
      return MarkCompactCollector::HasCompacted() ? "Mark-compact"
                                                  : "Mark-sweep";
  }
  return "Unknown GC";
}


int KeyedLookupCache::Hash(Map* map, String* name) {
  // Uses only lower 32 bits if pointers are larger.
  uintptr_t addr_hash =
      static_cast<uint32_t>(reinterpret_cast<uintptr_t>(map)) >> kMapHashShift;
  return static_cast<uint32_t>((addr_hash ^ name->Hash()) & kCapacityMask);
}


int KeyedLookupCache::Lookup(Map* map, String* name) {
  int index = Hash(map, name);
  Key& key = keys_[index];
  if ((key.map == map) && key.name->Equals(name)) {
    return field_offsets_[index];
  }
  return -1;
}


void KeyedLookupCache::Update(Map* map, String* name, int field_offset) {
  String* symbol;
  if (Heap::LookupSymbolIfExists(name, &symbol)) {
    int index = Hash(map, symbol);
    Key& key = keys_[index];
    key.map = map;
    key.name = symbol;
    field_offsets_[index] = field_offset;
  }
}


void KeyedLookupCache::Clear() {
  for (int index = 0; index < kLength; index++) keys_[index].map = NULL;
}


KeyedLookupCache::Key KeyedLookupCache::keys_[KeyedLookupCache::kLength];


int KeyedLookupCache::field_offsets_[KeyedLookupCache::kLength];


void DescriptorLookupCache::Clear() {
  for (int index = 0; index < kLength; index++) keys_[index].array = NULL;
}


DescriptorLookupCache::Key
DescriptorLookupCache::keys_[DescriptorLookupCache::kLength];

int DescriptorLookupCache::results_[DescriptorLookupCache::kLength];


#ifdef DEBUG
void Heap::GarbageCollectionGreedyCheck() {
  ASSERT(FLAG_gc_greedy);
  if (Bootstrapper::IsActive()) return;
  if (disallow_allocation_failure()) return;
  CollectGarbage(NEW_SPACE);
}
#endif


TranscendentalCache::TranscendentalCache(TranscendentalCache::Type t)
  : type_(t) {
  uint32_t in0 = 0xffffffffu;  // Bit-pattern for a NaN that isn't
  uint32_t in1 = 0xffffffffu;  // generated by the FPU.
  for (int i = 0; i < kCacheSize; i++) {
    elements_[i].in[0] = in0;
    elements_[i].in[1] = in1;
    elements_[i].output = NULL;
  }
}


TranscendentalCache* TranscendentalCache::caches_[kNumberOfCaches];


void TranscendentalCache::Clear() {
  for (int i = 0; i < kNumberOfCaches; i++) {
    if (caches_[i] != NULL) {
      delete caches_[i];
      caches_[i] = NULL;
    }
  }
}


void ExternalStringTable::CleanUp() {
  int last = 0;
  for (int i = 0; i < new_space_strings_.length(); ++i) {
    if (new_space_strings_[i] == Heap::raw_unchecked_null_value()) continue;
    if (Heap::InNewSpace(new_space_strings_[i])) {
      new_space_strings_[last++] = new_space_strings_[i];
    } else {
      old_space_strings_.Add(new_space_strings_[i]);
    }
  }
  new_space_strings_.Rewind(last);
  last = 0;
  for (int i = 0; i < old_space_strings_.length(); ++i) {
    if (old_space_strings_[i] == Heap::raw_unchecked_null_value()) continue;
    ASSERT(!Heap::InNewSpace(old_space_strings_[i]));
    old_space_strings_[last++] = old_space_strings_[i];
  }
  old_space_strings_.Rewind(last);
  Verify();
}


void ExternalStringTable::TearDown() {
  new_space_strings_.Free();
  old_space_strings_.Free();
}


List<Object*> ExternalStringTable::new_space_strings_;
List<Object*> ExternalStringTable::old_space_strings_;

} }  // namespace v8::internal
