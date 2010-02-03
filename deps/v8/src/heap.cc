// Copyright 2009 the V8 project authors. All rights reserved.
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
#include "mark-compact.h"
#include "natives.h"
#include "scanner.h"
#include "scopeinfo.h"
#include "snapshot.h"
#include "v8threads.h"
#if V8_TARGET_ARCH_ARM && V8_NATIVE_REGEXP
#include "regexp-macro-assembler.h"
#include "arm/regexp-macro-assembler-arm.h"
#endif

namespace v8 {
namespace internal {


String* Heap::hidden_symbol_;
Object* Heap::roots_[Heap::kRootListLength];


NewSpace Heap::new_space_;
OldSpace* Heap::old_pointer_space_ = NULL;
OldSpace* Heap::old_data_space_ = NULL;
OldSpace* Heap::code_space_ = NULL;
MapSpace* Heap::map_space_ = NULL;
CellSpace* Heap::cell_space_ = NULL;
LargeObjectSpace* Heap::lo_space_ = NULL;

static const int kMinimumPromotionLimit = 2*MB;
static const int kMinimumAllocationLimit = 8*MB;

int Heap::old_gen_promotion_limit_ = kMinimumPromotionLimit;
int Heap::old_gen_allocation_limit_ = kMinimumAllocationLimit;

int Heap::old_gen_exhausted_ = false;

int Heap::amount_of_external_allocated_memory_ = 0;
int Heap::amount_of_external_allocated_memory_at_last_global_gc_ = 0;

// semispace_size_ should be a power of 2 and old_generation_size_ should be
// a multiple of Page::kPageSize.
#if defined(ANDROID)
int Heap::max_semispace_size_  = 2*MB;
int Heap::max_old_generation_size_ = 192*MB;
int Heap::initial_semispace_size_ = 128*KB;
size_t Heap::code_range_size_ = 0;
#elif defined(V8_TARGET_ARCH_X64)
int Heap::max_semispace_size_  = 16*MB;
int Heap::max_old_generation_size_ = 1*GB;
int Heap::initial_semispace_size_ = 1*MB;
size_t Heap::code_range_size_ = 512*MB;
#else
int Heap::max_semispace_size_  = 8*MB;
int Heap::max_old_generation_size_ = 512*MB;
int Heap::initial_semispace_size_ = 512*KB;
size_t Heap::code_range_size_ = 0;
#endif

// The snapshot semispace size will be the default semispace size if
// snapshotting is used and will be the requested semispace size as
// set up by ConfigureHeap otherwise.
int Heap::reserved_semispace_size_ = Heap::max_semispace_size_;

GCCallback Heap::global_gc_prologue_callback_ = NULL;
GCCallback Heap::global_gc_epilogue_callback_ = NULL;

// Variables set based on semispace_size_ and old_generation_size_ in
// ConfigureHeap.

// Will be 4 * reserved_semispace_size_ to ensure that young
// generation can be aligned to its size.
int Heap::survived_since_last_expansion_ = 0;
int Heap::external_allocation_limit_ = 0;

Heap::HeapState Heap::gc_state_ = NOT_IN_GC;

int Heap::mc_count_ = 0;
int Heap::gc_count_ = 0;

int Heap::always_allocate_scope_depth_ = 0;
int Heap::linear_allocation_scope_depth_ = 0;
bool Heap::context_disposed_pending_ = false;

#ifdef DEBUG
bool Heap::allocation_allowed_ = true;

int Heap::allocation_timeout_ = 0;
bool Heap::disallow_allocation_failure_ = false;
#endif  // DEBUG


int Heap::Capacity() {
  if (!HasBeenSetup()) return 0;

  return new_space_.Capacity() +
      old_pointer_space_->Capacity() +
      old_data_space_->Capacity() +
      code_space_->Capacity() +
      map_space_->Capacity() +
      cell_space_->Capacity();
}


int Heap::CommittedMemory() {
  if (!HasBeenSetup()) return 0;

  return new_space_.CommittedMemory() +
      old_pointer_space_->CommittedMemory() +
      old_data_space_->CommittedMemory() +
      code_space_->CommittedMemory() +
      map_space_->CommittedMemory() +
      cell_space_->CommittedMemory() +
      lo_space_->Size();
}


int Heap::Available() {
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
  PrintF("Memory allocator,   used: %8d, available: %8d\n",
         MemoryAllocator::Size(),
         MemoryAllocator::Available());
  PrintF("New space,          used: %8d, available: %8d\n",
         Heap::new_space_.Size(),
         new_space_.Available());
  PrintF("Old pointers,       used: %8d, available: %8d, waste: %8d\n",
         old_pointer_space_->Size(),
         old_pointer_space_->Available(),
         old_pointer_space_->Waste());
  PrintF("Old data space,     used: %8d, available: %8d, waste: %8d\n",
         old_data_space_->Size(),
         old_data_space_->Available(),
         old_data_space_->Waste());
  PrintF("Code space,         used: %8d, available: %8d, waste: %8d\n",
         code_space_->Size(),
         code_space_->Available(),
         code_space_->Waste());
  PrintF("Map space,          used: %8d, available: %8d, waste: %8d\n",
         map_space_->Size(),
         map_space_->Available(),
         map_space_->Waste());
  PrintF("Cell space,         used: %8d, available: %8d, waste: %8d\n",
         cell_space_->Size(),
         cell_space_->Available(),
         cell_space_->Waste());
  PrintF("Large object space, used: %8d, avaialble: %8d\n",
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
  gc_count_++;
#ifdef DEBUG
  ASSERT(allocation_allowed_ && gc_state_ == NOT_IN_GC);
  allow_allocation(false);

  if (FLAG_verify_heap) {
    Verify();
  }

  if (FLAG_gc_verbose) Print();

  if (FLAG_print_rset) {
    // Not all spaces have remembered set bits that we care about.
    old_pointer_space_->PrintRSet();
    map_space_->PrintRSet();
    lo_space_->PrintRSet();
  }
#endif

#if defined(DEBUG) || defined(ENABLE_LOGGING_AND_PROFILING)
  ReportStatisticsBeforeGC();
#endif
}

int Heap::SizeOfObjects() {
  int total = 0;
  AllSpaces spaces;
  for (Space* space = spaces.next(); space != NULL; space = spaces.next()) {
    total += space->Size();
  }
  return total;
}

void Heap::GarbageCollectionEpilogue() {
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

  Counters::alive_after_last_gc.Set(SizeOfObjects());

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
  CollectGarbage(0, OLD_POINTER_SPACE);
  MarkCompactCollector::SetForceCompaction(false);
}


void Heap::CollectAllGarbageIfContextDisposed() {
  // If the garbage collector interface is exposed through the global
  // gc() function, we avoid being clever about forcing GCs when
  // contexts are disposed and leave it to the embedder to make
  // informed decisions about when to force a collection.
  if (!FLAG_expose_gc && context_disposed_pending_) {
    HistogramTimerScope scope(&Counters::gc_context);
    CollectAllGarbage(false);
  }
  context_disposed_pending_ = false;
}


void Heap::NotifyContextDisposed() {
  context_disposed_pending_ = true;
}


bool Heap::CollectGarbage(int requested_size, AllocationSpace space) {
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

  { GCTracer tracer;
    GarbageCollectionPrologue();
    // The GC count was incremented in the prologue.  Tell the tracer about
    // it.
    tracer.set_gc_count(gc_count_);

    GarbageCollector collector = SelectGarbageCollector(space);
    // Tell the tracer which collector we've selected.
    tracer.set_collector(collector);

    HistogramTimer* rate = (collector == SCAVENGER)
        ? &Counters::gc_scavenger
        : &Counters::gc_compactor;
    rate->Start();
    PerformGarbageCollection(space, collector, &tracer);
    rate->Stop();

    GarbageCollectionEpilogue();
  }


#ifdef ENABLE_LOGGING_AND_PROFILING
  if (FLAG_log_gc) HeapProfiler::WriteSample();
#endif

  switch (space) {
    case NEW_SPACE:
      return new_space_.Available() >= requested_size;
    case OLD_POINTER_SPACE:
      return old_pointer_space_->Available() >= requested_size;
    case OLD_DATA_SPACE:
      return old_data_space_->Available() >= requested_size;
    case CODE_SPACE:
      return code_space_->Available() >= requested_size;
    case MAP_SPACE:
      return map_space_->Available() >= requested_size;
    case CELL_SPACE:
      return cell_space_->Available() >= requested_size;
    case LO_SPACE:
      return lo_space_->Available() >= requested_size;
  }
  return false;
}


void Heap::PerformScavenge() {
  GCTracer tracer;
  PerformGarbageCollection(NEW_SPACE, SCAVENGER, &tracer);
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
      Heap::CollectGarbage(new_space_size, NEW_SPACE);
      gc_performed = true;
    }
    if (!old_pointer_space->ReserveSpace(pointer_space_size)) {
      Heap::CollectGarbage(pointer_space_size, OLD_POINTER_SPACE);
      gc_performed = true;
    }
    if (!(old_data_space->ReserveSpace(data_space_size))) {
      Heap::CollectGarbage(data_space_size, OLD_DATA_SPACE);
      gc_performed = true;
    }
    if (!(code_space->ReserveSpace(code_space_size))) {
      Heap::CollectGarbage(code_space_size, CODE_SPACE);
      gc_performed = true;
    }
    if (!(map_space->ReserveSpace(map_space_size))) {
      Heap::CollectGarbage(map_space_size, MAP_SPACE);
      gc_performed = true;
    }
    if (!(cell_space->ReserveSpace(cell_space_size))) {
      Heap::CollectGarbage(cell_space_size, CELL_SPACE);
      gc_performed = true;
    }
    // We add a slack-factor of 2 in order to have space for the remembered
    // set and a series of large-object allocations that are only just larger
    // than the page size.
    large_object_size *= 2;
    // The ReserveSpace method on the large object space checks how much
    // we can expand the old generation.  This includes expansion caused by
    // allocation in the other spaces.
    large_object_size += cell_space_size + map_space_size + code_space_size +
        data_space_size + pointer_space_size;
    if (!(lo_space->ReserveSpace(large_object_size))) {
      Heap::CollectGarbage(large_object_size, LO_SPACE);
      gc_performed = true;
    }
  }
}


void Heap::EnsureFromSpaceIsCommitted() {
  if (new_space_.CommitFromSpaceIfNeeded()) return;

  // Committing memory to from space failed.
  // Try shrinking and try again.
  Shrink();
  if (new_space_.CommitFromSpaceIfNeeded()) return;

  // Committing memory to from space failed again.
  // Memory is exhausted and we will die.
  V8::FatalProcessOutOfMemory("Committing semi space failed.");
}


void Heap::PerformGarbageCollection(AllocationSpace space,
                                    GarbageCollector collector,
                                    GCTracer* tracer) {
  VerifySymbolTable();
  if (collector == MARK_COMPACTOR && global_gc_prologue_callback_) {
    ASSERT(!allocation_allowed_);
    global_gc_prologue_callback_();
  }
  EnsureFromSpaceIsCommitted();
  if (collector == MARK_COMPACTOR) {
    MarkCompact(tracer);

    int old_gen_size = PromotedSpaceSize();
    old_gen_promotion_limit_ =
        old_gen_size + Max(kMinimumPromotionLimit, old_gen_size / 3);
    old_gen_allocation_limit_ =
        old_gen_size + Max(kMinimumAllocationLimit, old_gen_size / 2);
    old_gen_exhausted_ = false;
  }
  Scavenge();

  Counters::objs_since_last_young.Set(0);

  if (collector == MARK_COMPACTOR) {
    DisableAssertNoAllocation allow_allocation;
    GlobalHandles::PostGarbageCollectionProcessing();
  }

  // Update relocatables.
  Relocatable::PostGarbageCollectionProcessing();

  if (collector == MARK_COMPACTOR) {
    // Register the amount of external allocated memory.
    amount_of_external_allocated_memory_at_last_global_gc_ =
        amount_of_external_allocated_memory_;
  }

  if (collector == MARK_COMPACTOR && global_gc_epilogue_callback_) {
    ASSERT(!allocation_allowed_);
    global_gc_epilogue_callback_();
  }
  VerifySymbolTable();
}


void Heap::MarkCompact(GCTracer* tracer) {
  gc_state_ = MARK_COMPACT;
  mc_count_++;
  tracer->set_full_gc_count(mc_count_);
  LOG(ResourceEvent("markcompact", "begin"));

  MarkCompactCollector::Prepare(tracer);

  bool is_compacting = MarkCompactCollector::IsCompacting();

  MarkCompactPrologue(is_compacting);

  MarkCompactCollector::CollectGarbage();

  MarkCompactEpilogue(is_compacting);

  LOG(ResourceEvent("markcompact", "end"));

  gc_state_ = NOT_IN_GC;

  Shrink();

  Counters::objs_since_last_full.Set(0);
  context_disposed_pending_ = false;
}


void Heap::MarkCompactPrologue(bool is_compacting) {
  // At any old GC clear the keyed lookup cache to enable collection of unused
  // maps.
  KeyedLookupCache::Clear();
  ContextSlotCache::Clear();
  DescriptorLookupCache::Clear();

  CompilationCache::MarkCompactPrologue();

  Top::MarkCompactPrologue(is_compacting);
  ThreadManager::MarkCompactPrologue(is_compacting);

  if (is_compacting) FlushNumberStringCache();
}


void Heap::MarkCompactEpilogue(bool is_compacting) {
  Top::MarkCompactEpilogue(is_compacting);
  ThreadManager::MarkCompactEpilogue(is_compacting);
}


Object* Heap::FindCodeObject(Address a) {
  Object* obj = code_space_->FindObject(a);
  if (obj->IsFailure()) {
    obj = lo_space_->FindObject(a);
  }
  ASSERT(!obj->IsFailure());
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


// A queue of pointers and maps of to-be-promoted objects during a
// scavenge collection.
class PromotionQueue {
 public:
  void Initialize(Address start_address) {
    front_ = rear_ = reinterpret_cast<HeapObject**>(start_address);
  }

  bool is_empty() { return front_ <= rear_; }

  void insert(HeapObject* object, Map* map) {
    *(--rear_) = object;
    *(--rear_) = map;
    // Assert no overflow into live objects.
    ASSERT(reinterpret_cast<Address>(rear_) >= Heap::new_space()->top());
  }

  void remove(HeapObject** object, Map** map) {
    *object = *(--front_);
    *map = Map::cast(*(--front_));
    // Assert no underflow.
    ASSERT(front_ >= rear_);
  }

 private:
  // The front of the queue is higher in memory than the rear.
  HeapObject** front_;
  HeapObject** rear_;
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


void Heap::Scavenge() {
#ifdef DEBUG
  if (FLAG_enable_slow_asserts) VerifyNonPointerSpacePointers();
#endif

  gc_state_ = SCAVENGE;

  // Implements Cheney's copying algorithm
  LOG(ResourceEvent("scavenge", "begin"));

  // Clear descriptor cache.
  DescriptorLookupCache::Clear();

  // Used for updating survived_since_last_expansion_ at function end.
  int survived_watermark = PromotedSpaceSize();

  if (new_space_.Capacity() < new_space_.MaximumCapacity() &&
      survived_since_last_expansion_ > new_space_.Capacity()) {
    // Grow the size of new space if there is room to grow and enough
    // data has survived scavenge since the last expansion.
    new_space_.Grow();
    survived_since_last_expansion_ = 0;
  }

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
  IterateRSet(old_pointer_space_, &ScavengePointer);
  IterateRSet(map_space_, &ScavengePointer);
  lo_space_->IterateRSet(&ScavengePointer);

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

  new_space_front = DoScavenge(&scavenge_visitor, new_space_front);

  ScavengeExternalStringTable();
  ASSERT(new_space_front == new_space_.top());

  // Set age mark.
  new_space_.set_age_mark(new_space_.top());

  // Update how much has survived scavenge.
  survived_since_last_expansion_ +=
      (PromotedSpaceSize() - survived_watermark) + new_space_.Size();

  LOG(ResourceEvent("scavenge", "end"));

  gc_state_ = NOT_IN_GC;
}


void Heap::ScavengeExternalStringTable() {
  ExternalStringTable::Verify();

  if (ExternalStringTable::new_space_strings_.is_empty()) return;

  Object** start = &ExternalStringTable::new_space_strings_[0];
  Object** end = start + ExternalStringTable::new_space_strings_.length();
  Object** last = start;

  for (Object** p = start; p < end; ++p) {
    ASSERT(Heap::InFromSpace(*p));
    MapWord first_word = HeapObject::cast(*p)->map_word();

    if (!first_word.IsForwardingAddress()) {
      // Unreachable external string can be finalized.
      FinalizeExternalString(String::cast(*p));
      continue;
    }

    // String is still reachable.
    String* target = String::cast(first_word.ToForwardingAddress());
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


Address Heap::DoScavenge(ObjectVisitor* scavenge_visitor,
                         Address new_space_front) {
  do {
    ASSERT(new_space_front <= new_space_.top());

    // The addresses new_space_front and new_space_.top() define a
    // queue of unprocessed copied objects.  Process them until the
    // queue is empty.
    while (new_space_front < new_space_.top()) {
      HeapObject* object = HeapObject::FromAddress(new_space_front);
      object->Iterate(scavenge_visitor);
      new_space_front += object->Size();
    }

    // Promote and process all the to-be-promoted objects.
    while (!promotion_queue.is_empty()) {
      HeapObject* source;
      Map* map;
      promotion_queue.remove(&source, &map);
      // Copy the from-space object to its new location (given by the
      // forwarding address) and fix its map.
      HeapObject* target = source->map_word().ToForwardingAddress();
      CopyBlock(reinterpret_cast<Object**>(target->address()),
                reinterpret_cast<Object**>(source->address()),
                source->SizeFromMap(map));
      target->set_map(map);

#if defined(DEBUG) || defined(ENABLE_LOGGING_AND_PROFILING)
      // Update NewSpace stats if necessary.
      RecordCopiedObject(target);
#endif
      // Visit the newly copied object for pointers to new space.
      target->Iterate(scavenge_visitor);
      UpdateRSet(target);
    }

    // Take another spin if there are now unswept objects in new space
    // (there are currently no more unswept promoted objects).
  } while (new_space_front < new_space_.top());

  return new_space_front;
}


void Heap::ClearRSetRange(Address start, int size_in_bytes) {
  uint32_t start_bit;
  Address start_word_address =
      Page::ComputeRSetBitPosition(start, 0, &start_bit);
  uint32_t end_bit;
  Address end_word_address =
      Page::ComputeRSetBitPosition(start + size_in_bytes - kIntSize,
                                   0,
                                   &end_bit);

  // We want to clear the bits in the starting word starting with the
  // first bit, and in the ending word up to and including the last
  // bit.  Build a pair of bitmasks to do that.
  uint32_t start_bitmask = start_bit - 1;
  uint32_t end_bitmask = ~((end_bit << 1) - 1);

  // If the start address and end address are the same, we mask that
  // word once, otherwise mask the starting and ending word
  // separately and all the ones in between.
  if (start_word_address == end_word_address) {
    Memory::uint32_at(start_word_address) &= (start_bitmask | end_bitmask);
  } else {
    Memory::uint32_at(start_word_address) &= start_bitmask;
    Memory::uint32_at(end_word_address) &= end_bitmask;
    start_word_address += kIntSize;
    memset(start_word_address, 0, end_word_address - start_word_address);
  }
}


class UpdateRSetVisitor: public ObjectVisitor {
 public:

  void VisitPointer(Object** p) {
    UpdateRSet(p);
  }

  void VisitPointers(Object** start, Object** end) {
    // Update a store into slots [start, end), used (a) to update remembered
    // set when promoting a young object to old space or (b) to rebuild
    // remembered sets after a mark-compact collection.
    for (Object** p = start; p < end; p++) UpdateRSet(p);
  }
 private:

  void UpdateRSet(Object** p) {
    // The remembered set should not be set.  It should be clear for objects
    // newly copied to old space, and it is cleared before rebuilding in the
    // mark-compact collector.
    ASSERT(!Page::IsRSetSet(reinterpret_cast<Address>(p), 0));
    if (Heap::InNewSpace(*p)) {
      Page::SetRSet(reinterpret_cast<Address>(p), 0);
    }
  }
};


int Heap::UpdateRSet(HeapObject* obj) {
  ASSERT(!InNewSpace(obj));
  // Special handling of fixed arrays to iterate the body based on the start
  // address and offset.  Just iterating the pointers as in UpdateRSetVisitor
  // will not work because Page::SetRSet needs to have the start of the
  // object for large object pages.
  if (obj->IsFixedArray()) {
    FixedArray* array = FixedArray::cast(obj);
    int length = array->length();
    for (int i = 0; i < length; i++) {
      int offset = FixedArray::kHeaderSize + i * kPointerSize;
      ASSERT(!Page::IsRSetSet(obj->address(), offset));
      if (Heap::InNewSpace(array->get(i))) {
        Page::SetRSet(obj->address(), offset);
      }
    }
  } else if (!obj->IsCode()) {
    // Skip code object, we know it does not contain inter-generational
    // pointers.
    UpdateRSetVisitor v;
    obj->Iterate(&v);
  }
  return obj->Size();
}


void Heap::RebuildRSets() {
  // By definition, we do not care about remembered set bits in code,
  // data, or cell spaces.
  map_space_->ClearRSet();
  RebuildRSets(map_space_);

  old_pointer_space_->ClearRSet();
  RebuildRSets(old_pointer_space_);

  Heap::lo_space_->ClearRSet();
  RebuildRSets(lo_space_);
}


void Heap::RebuildRSets(PagedSpace* space) {
  HeapObjectIterator it(space);
  for (HeapObject* obj = it.next(); obj != NULL; obj = it.next())
    Heap::UpdateRSet(obj);
}


void Heap::RebuildRSets(LargeObjectSpace* space) {
  LargeObjectIterator it(space);
  for (HeapObject* obj = it.next(); obj != NULL; obj = it.next())
    Heap::UpdateRSet(obj);
}


#if defined(DEBUG) || defined(ENABLE_LOGGING_AND_PROFILING)
void Heap::RecordCopiedObject(HeapObject* obj) {
  bool should_record = false;
#ifdef DEBUG
  should_record = FLAG_heap_stats;
#endif
#ifdef ENABLE_LOGGING_AND_PROFILING
  should_record = should_record || FLAG_log_gc;
#endif
  if (should_record) {
    if (new_space_.Contains(obj)) {
      new_space_.RecordAllocation(obj);
    } else {
      new_space_.RecordPromotion(obj);
    }
  }
}
#endif  // defined(DEBUG) || defined(ENABLE_LOGGING_AND_PROFILING)



HeapObject* Heap::MigrateObject(HeapObject* source,
                                HeapObject* target,
                                int size) {
  // Copy the content of source to target.
  CopyBlock(reinterpret_cast<Object**>(target->address()),
            reinterpret_cast<Object**>(source->address()),
            size);

  // Set the forwarding address.
  source->set_map_word(MapWord::FromForwardingAddress(target));

#if defined(DEBUG) || defined(ENABLE_LOGGING_AND_PROFILING)
  // Update NewSpace stats if necessary.
  RecordCopiedObject(target);
#endif

  return target;
}


static inline bool IsShortcutCandidate(HeapObject* object, Map* map) {
  STATIC_ASSERT(kNotStringTag != 0 && kSymbolTag != 0);
  ASSERT(object->map() == map);
  InstanceType type = map->instance_type();
  if ((type & kShortcutTypeMask) != kShortcutTypeTag) return false;
  ASSERT(object->IsString() && !object->IsSymbol());
  return ConsString::cast(object)->unchecked_second() == Heap::empty_string();
}


void Heap::ScavengeObjectSlow(HeapObject** p, HeapObject* object) {
  ASSERT(InFromSpace(object));
  MapWord first_word = object->map_word();
  ASSERT(!first_word.IsForwardingAddress());

  // Optimization: Bypass flattened ConsString objects.
  if (IsShortcutCandidate(object, first_word.ToMap())) {
    object = HeapObject::cast(ConsString::cast(object)->unchecked_first());
    *p = object;
    // After patching *p we have to repeat the checks that object is in the
    // active semispace of the young generation and not already copied.
    if (!InNewSpace(object)) return;
    first_word = object->map_word();
    if (first_word.IsForwardingAddress()) {
      *p = first_word.ToForwardingAddress();
      return;
    }
  }

  int object_size = object->SizeFromMap(first_word.ToMap());
  // We rely on live objects in new space to be at least two pointers,
  // so we can store the from-space address and map pointer of promoted
  // objects in the to space.
  ASSERT(object_size >= 2 * kPointerSize);

  // If the object should be promoted, we try to copy it to old space.
  if (ShouldBePromoted(object->address(), object_size)) {
    Object* result;
    if (object_size > MaxObjectSizeInPagedSpace()) {
      result = lo_space_->AllocateRawFixedArray(object_size);
      if (!result->IsFailure()) {
        // Save the from-space object pointer and its map pointer at the
        // top of the to space to be swept and copied later.  Write the
        // forwarding address over the map word of the from-space
        // object.
        HeapObject* target = HeapObject::cast(result);
        promotion_queue.insert(object, first_word.ToMap());
        object->set_map_word(MapWord::FromForwardingAddress(target));

        // Give the space allocated for the result a proper map by
        // treating it as a free list node (not linked into the free
        // list).
        FreeListNode* node = FreeListNode::FromAddress(target->address());
        node->set_size(object_size);

        *p = target;
        return;
      }
    } else {
      OldSpace* target_space = Heap::TargetSpace(object);
      ASSERT(target_space == Heap::old_pointer_space_ ||
             target_space == Heap::old_data_space_);
      result = target_space->AllocateRaw(object_size);
      if (!result->IsFailure()) {
        HeapObject* target = HeapObject::cast(result);
        if (target_space == Heap::old_pointer_space_) {
          // Save the from-space object pointer and its map pointer at the
          // top of the to space to be swept and copied later.  Write the
          // forwarding address over the map word of the from-space
          // object.
          promotion_queue.insert(object, first_word.ToMap());
          object->set_map_word(MapWord::FromForwardingAddress(target));

          // Give the space allocated for the result a proper map by
          // treating it as a free list node (not linked into the free
          // list).
          FreeListNode* node = FreeListNode::FromAddress(target->address());
          node->set_size(object_size);

          *p = target;
        } else {
          // Objects promoted to the data space can be copied immediately
          // and not revisited---we will never sweep that space for
          // pointers and the copied objects do not contain pointers to
          // new space objects.
          *p = MigrateObject(object, target, object_size);
#ifdef DEBUG
          VerifyNonPointerSpacePointersVisitor v;
          (*p)->Iterate(&v);
#endif
        }
        return;
      }
    }
  }
  // The object should remain in new space or the old space allocation failed.
  Object* result = new_space_.AllocateRaw(object_size);
  // Failed allocation at this point is utterly unexpected.
  ASSERT(!result->IsFailure());
  *p = MigrateObject(object, HeapObject::cast(result), object_size);
}


void Heap::ScavengePointer(HeapObject** p) {
  ScavengeObject(p, *p);
}


Object* Heap::AllocatePartialMap(InstanceType instance_type,
                                 int instance_size) {
  Object* result = AllocateRawMap();
  if (result->IsFailure()) return result;

  // Map::cast cannot be used due to uninitialized map field.
  reinterpret_cast<Map*>(result)->set_map(raw_unchecked_meta_map());
  reinterpret_cast<Map*>(result)->set_instance_type(instance_type);
  reinterpret_cast<Map*>(result)->set_instance_size(instance_size);
  reinterpret_cast<Map*>(result)->set_inobject_properties(0);
  reinterpret_cast<Map*>(result)->set_pre_allocated_property_fields(0);
  reinterpret_cast<Map*>(result)->set_unused_property_fields(0);
  reinterpret_cast<Map*>(result)->set_bit_field(0);
  reinterpret_cast<Map*>(result)->set_bit_field2(0);
  return result;
}


Object* Heap::AllocateMap(InstanceType instance_type, int instance_size) {
  Object* result = AllocateRawMap();
  if (result->IsFailure()) return result;

  Map* map = reinterpret_cast<Map*>(result);
  map->set_map(meta_map());
  map->set_instance_type(instance_type);
  map->set_prototype(null_value());
  map->set_constructor(null_value());
  map->set_instance_size(instance_size);
  map->set_inobject_properties(0);
  map->set_pre_allocated_property_fields(0);
  map->set_instance_descriptors(empty_descriptor_array());
  map->set_code_cache(empty_fixed_array());
  map->set_unused_property_fields(0);
  map->set_bit_field(0);
  map->set_bit_field2(1 << Map::kIsExtensible);

  // If the map object is aligned fill the padding area with Smi 0 objects.
  if (Map::kPadStart < Map::kSize) {
    memset(reinterpret_cast<byte*>(map) + Map::kPadStart - kHeapObjectTag,
           0,
           Map::kSize - Map::kPadStart);
  }
  return map;
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
  Object* obj = AllocatePartialMap(MAP_TYPE, Map::kSize);
  if (obj->IsFailure()) return false;
  // Map::cast cannot be used due to uninitialized map field.
  Map* new_meta_map = reinterpret_cast<Map*>(obj);
  set_meta_map(new_meta_map);
  new_meta_map->set_map(new_meta_map);

  obj = AllocatePartialMap(FIXED_ARRAY_TYPE, FixedArray::kHeaderSize);
  if (obj->IsFailure()) return false;
  set_fixed_array_map(Map::cast(obj));

  obj = AllocatePartialMap(ODDBALL_TYPE, Oddball::kSize);
  if (obj->IsFailure()) return false;
  set_oddball_map(Map::cast(obj));

  // Allocate the empty array
  obj = AllocateEmptyFixedArray();
  if (obj->IsFailure()) return false;
  set_empty_fixed_array(FixedArray::cast(obj));

  obj = Allocate(oddball_map(), OLD_DATA_SPACE);
  if (obj->IsFailure()) return false;
  set_null_value(obj);

  // Allocate the empty descriptor array.
  obj = AllocateEmptyFixedArray();
  if (obj->IsFailure()) return false;
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

  obj = AllocateMap(HEAP_NUMBER_TYPE, HeapNumber::kSize);
  if (obj->IsFailure()) return false;
  set_heap_number_map(Map::cast(obj));

  obj = AllocateMap(PROXY_TYPE, Proxy::kSize);
  if (obj->IsFailure()) return false;
  set_proxy_map(Map::cast(obj));

  for (unsigned i = 0; i < ARRAY_SIZE(string_type_table); i++) {
    const StringTypeTable& entry = string_type_table[i];
    obj = AllocateMap(entry.type, entry.size);
    if (obj->IsFailure()) return false;
    roots_[entry.index] = Map::cast(obj);
  }

  obj = AllocateMap(STRING_TYPE, SeqTwoByteString::kAlignedSize);
  if (obj->IsFailure()) return false;
  set_undetectable_string_map(Map::cast(obj));
  Map::cast(obj)->set_is_undetectable();

  obj = AllocateMap(ASCII_STRING_TYPE, SeqAsciiString::kAlignedSize);
  if (obj->IsFailure()) return false;
  set_undetectable_ascii_string_map(Map::cast(obj));
  Map::cast(obj)->set_is_undetectable();

  obj = AllocateMap(BYTE_ARRAY_TYPE, ByteArray::kAlignedSize);
  if (obj->IsFailure()) return false;
  set_byte_array_map(Map::cast(obj));

  obj = AllocateMap(PIXEL_ARRAY_TYPE, PixelArray::kAlignedSize);
  if (obj->IsFailure()) return false;
  set_pixel_array_map(Map::cast(obj));

  obj = AllocateMap(EXTERNAL_BYTE_ARRAY_TYPE,
                    ExternalArray::kAlignedSize);
  if (obj->IsFailure()) return false;
  set_external_byte_array_map(Map::cast(obj));

  obj = AllocateMap(EXTERNAL_UNSIGNED_BYTE_ARRAY_TYPE,
                    ExternalArray::kAlignedSize);
  if (obj->IsFailure()) return false;
  set_external_unsigned_byte_array_map(Map::cast(obj));

  obj = AllocateMap(EXTERNAL_SHORT_ARRAY_TYPE,
                    ExternalArray::kAlignedSize);
  if (obj->IsFailure()) return false;
  set_external_short_array_map(Map::cast(obj));

  obj = AllocateMap(EXTERNAL_UNSIGNED_SHORT_ARRAY_TYPE,
                    ExternalArray::kAlignedSize);
  if (obj->IsFailure()) return false;
  set_external_unsigned_short_array_map(Map::cast(obj));

  obj = AllocateMap(EXTERNAL_INT_ARRAY_TYPE,
                    ExternalArray::kAlignedSize);
  if (obj->IsFailure()) return false;
  set_external_int_array_map(Map::cast(obj));

  obj = AllocateMap(EXTERNAL_UNSIGNED_INT_ARRAY_TYPE,
                    ExternalArray::kAlignedSize);
  if (obj->IsFailure()) return false;
  set_external_unsigned_int_array_map(Map::cast(obj));

  obj = AllocateMap(EXTERNAL_FLOAT_ARRAY_TYPE,
                    ExternalArray::kAlignedSize);
  if (obj->IsFailure()) return false;
  set_external_float_array_map(Map::cast(obj));

  obj = AllocateMap(CODE_TYPE, Code::kHeaderSize);
  if (obj->IsFailure()) return false;
  set_code_map(Map::cast(obj));

  obj = AllocateMap(JS_GLOBAL_PROPERTY_CELL_TYPE,
                    JSGlobalPropertyCell::kSize);
  if (obj->IsFailure()) return false;
  set_global_property_cell_map(Map::cast(obj));

  obj = AllocateMap(FILLER_TYPE, kPointerSize);
  if (obj->IsFailure()) return false;
  set_one_pointer_filler_map(Map::cast(obj));

  obj = AllocateMap(FILLER_TYPE, 2 * kPointerSize);
  if (obj->IsFailure()) return false;
  set_two_pointer_filler_map(Map::cast(obj));

  for (unsigned i = 0; i < ARRAY_SIZE(struct_table); i++) {
    const StructTable& entry = struct_table[i];
    obj = AllocateMap(entry.type, entry.size);
    if (obj->IsFailure()) return false;
    roots_[entry.index] = Map::cast(obj);
  }

  obj = AllocateMap(FIXED_ARRAY_TYPE, HeapObject::kHeaderSize);
  if (obj->IsFailure()) return false;
  set_hash_table_map(Map::cast(obj));

  obj = AllocateMap(FIXED_ARRAY_TYPE, HeapObject::kHeaderSize);
  if (obj->IsFailure()) return false;
  set_context_map(Map::cast(obj));

  obj = AllocateMap(FIXED_ARRAY_TYPE, HeapObject::kHeaderSize);
  if (obj->IsFailure()) return false;
  set_catch_context_map(Map::cast(obj));

  obj = AllocateMap(FIXED_ARRAY_TYPE, HeapObject::kHeaderSize);
  if (obj->IsFailure()) return false;
  set_global_context_map(Map::cast(obj));

  obj = AllocateMap(JS_FUNCTION_TYPE, JSFunction::kSize);
  if (obj->IsFailure()) return false;
  set_boilerplate_function_map(Map::cast(obj));

  obj = AllocateMap(SHARED_FUNCTION_INFO_TYPE, SharedFunctionInfo::kSize);
  if (obj->IsFailure()) return false;
  set_shared_function_info_map(Map::cast(obj));

  ASSERT(!Heap::InNewSpace(Heap::empty_fixed_array()));
  return true;
}


Object* Heap::AllocateHeapNumber(double value, PretenureFlag pretenure) {
  // Statically ensure that it is safe to allocate heap numbers in paged
  // spaces.
  STATIC_ASSERT(HeapNumber::kSize <= Page::kMaxHeapObjectSize);
  AllocationSpace space = (pretenure == TENURED) ? OLD_DATA_SPACE : NEW_SPACE;

  Object* result = AllocateRaw(HeapNumber::kSize, space, OLD_DATA_SPACE);
  if (result->IsFailure()) return result;

  HeapObject::cast(result)->set_map(heap_number_map());
  HeapNumber::cast(result)->set_value(value);
  return result;
}


Object* Heap::AllocateHeapNumber(double value) {
  // Use general version, if we're forced to always allocate.
  if (always_allocate()) return AllocateHeapNumber(value, TENURED);

  // This version of AllocateHeapNumber is optimized for
  // allocation in new space.
  STATIC_ASSERT(HeapNumber::kSize <= Page::kMaxHeapObjectSize);
  ASSERT(allocation_allowed_ && gc_state_ == NOT_IN_GC);
  Object* result = new_space_.AllocateRaw(HeapNumber::kSize);
  if (result->IsFailure()) return result;
  HeapObject::cast(result)->set_map(heap_number_map());
  HeapNumber::cast(result)->set_value(value);
  return result;
}


Object* Heap::AllocateJSGlobalPropertyCell(Object* value) {
  Object* result = AllocateRawCell();
  if (result->IsFailure()) return result;
  HeapObject::cast(result)->set_map(global_property_cell_map());
  JSGlobalPropertyCell::cast(result)->set_value(value);
  return result;
}


Object* Heap::CreateOddball(Map* map,
                            const char* to_string,
                            Object* to_number) {
  Object* result = Allocate(map, OLD_DATA_SPACE);
  if (result->IsFailure()) return result;
  return Oddball::cast(result)->Initialize(to_string, to_number);
}


bool Heap::CreateApiObjects() {
  Object* obj;

  obj = AllocateMap(JS_OBJECT_TYPE, JSObject::kHeaderSize);
  if (obj->IsFailure()) return false;
  set_neander_map(Map::cast(obj));

  obj = Heap::AllocateJSObjectFromMap(neander_map());
  if (obj->IsFailure()) return false;
  Object* elements = AllocateFixedArray(2);
  if (elements->IsFailure()) return false;
  FixedArray::cast(elements)->set(0, Smi::FromInt(0));
  JSObject::cast(obj)->set_elements(FixedArray::cast(elements));
  set_message_listeners(JSObject::cast(obj));

  return true;
}


void Heap::CreateCEntryStub() {
  CEntryStub stub(1);
  set_c_entry_code(*stub.GetCode());
}


#if V8_TARGET_ARCH_ARM && V8_NATIVE_REGEXP
void Heap::CreateRegExpCEntryStub() {
  RegExpCEntryStub stub;
  set_re_c_entry_code(*stub.GetCode());
}
#endif


void Heap::CreateCEntryDebugBreakStub() {
  DebuggerStatementStub stub;
  set_debugger_statement_code(*stub.GetCode());
}


void Heap::CreateJSEntryStub() {
  JSEntryStub stub;
  set_js_entry_code(*stub.GetCode());
}


void Heap::CreateJSConstructEntryStub() {
  JSConstructEntryStub stub;
  set_js_construct_entry_code(*stub.GetCode());
}


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
  Heap::CreateCEntryDebugBreakStub();
  Heap::CreateJSEntryStub();
  Heap::CreateJSConstructEntryStub();
#if V8_TARGET_ARCH_ARM && V8_NATIVE_REGEXP
  Heap::CreateRegExpCEntryStub();
#endif
}


bool Heap::CreateInitialObjects() {
  Object* obj;

  // The -0 value must be set before NumberFromDouble works.
  obj = AllocateHeapNumber(-0.0, TENURED);
  if (obj->IsFailure()) return false;
  set_minus_zero_value(obj);
  ASSERT(signbit(minus_zero_value()->Number()) != 0);

  obj = AllocateHeapNumber(OS::nan_value(), TENURED);
  if (obj->IsFailure()) return false;
  set_nan_value(obj);

  obj = Allocate(oddball_map(), OLD_DATA_SPACE);
  if (obj->IsFailure()) return false;
  set_undefined_value(obj);
  ASSERT(!InNewSpace(undefined_value()));

  // Allocate initial symbol table.
  obj = SymbolTable::Allocate(kInitialSymbolTableSize);
  if (obj->IsFailure()) return false;
  // Don't use set_symbol_table() due to asserts.
  roots_[kSymbolTableRootIndex] = obj;

  // Assign the print strings for oddballs after creating symboltable.
  Object* symbol = LookupAsciiSymbol("undefined");
  if (symbol->IsFailure()) return false;
  Oddball::cast(undefined_value())->set_to_string(String::cast(symbol));
  Oddball::cast(undefined_value())->set_to_number(nan_value());

  // Assign the print strings for oddballs after creating symboltable.
  symbol = LookupAsciiSymbol("null");
  if (symbol->IsFailure()) return false;
  Oddball::cast(null_value())->set_to_string(String::cast(symbol));
  Oddball::cast(null_value())->set_to_number(Smi::FromInt(0));

  // Allocate the null_value
  obj = Oddball::cast(null_value())->Initialize("null", Smi::FromInt(0));
  if (obj->IsFailure()) return false;

  obj = CreateOddball(oddball_map(), "true", Smi::FromInt(1));
  if (obj->IsFailure()) return false;
  set_true_value(obj);

  obj = CreateOddball(oddball_map(), "false", Smi::FromInt(0));
  if (obj->IsFailure()) return false;
  set_false_value(obj);

  obj = CreateOddball(oddball_map(), "hole", Smi::FromInt(-1));
  if (obj->IsFailure()) return false;
  set_the_hole_value(obj);

  obj = CreateOddball(
      oddball_map(), "no_interceptor_result_sentinel", Smi::FromInt(-2));
  if (obj->IsFailure()) return false;
  set_no_interceptor_result_sentinel(obj);

  obj = CreateOddball(oddball_map(), "termination_exception", Smi::FromInt(-3));
  if (obj->IsFailure()) return false;
  set_termination_exception(obj);

  // Allocate the empty string.
  obj = AllocateRawAsciiString(0, TENURED);
  if (obj->IsFailure()) return false;
  set_empty_string(String::cast(obj));

  for (unsigned i = 0; i < ARRAY_SIZE(constant_symbol_table); i++) {
    obj = LookupAsciiSymbol(constant_symbol_table[i].contents);
    if (obj->IsFailure()) return false;
    roots_[constant_symbol_table[i].index] = String::cast(obj);
  }

  // Allocate the hidden symbol which is used to identify the hidden properties
  // in JSObjects. The hash code has a special value so that it will not match
  // the empty string when searching for the property. It cannot be part of the
  // loop above because it needs to be allocated manually with the special
  // hash code in place. The hash code for the hidden_symbol is zero to ensure
  // that it will always be at the first entry in property descriptors.
  obj = AllocateSymbol(CStrVector(""), 0, String::kHashComputedMask);
  if (obj->IsFailure()) return false;
  hidden_symbol_ = String::cast(obj);

  // Allocate the proxy for __proto__.
  obj = AllocateProxy((Address) &Accessors::ObjectPrototype);
  if (obj->IsFailure()) return false;
  set_prototype_accessors(Proxy::cast(obj));

  // Allocate the code_stubs dictionary. The initial size is set to avoid
  // expanding the dictionary during bootstrapping.
  obj = NumberDictionary::Allocate(128);
  if (obj->IsFailure()) return false;
  set_code_stubs(NumberDictionary::cast(obj));

  // Allocate the non_monomorphic_cache used in stub-cache.cc. The initial size
  // is set to avoid expanding the dictionary during bootstrapping.
  obj = NumberDictionary::Allocate(64);
  if (obj->IsFailure()) return false;
  set_non_monomorphic_cache(NumberDictionary::cast(obj));

  CreateFixedStubs();

  if (InitializeNumberStringCache()->IsFailure()) return false;

  // Allocate cache for single character strings.
  obj = AllocateFixedArray(String::kMaxAsciiCharCode+1);
  if (obj->IsFailure()) return false;
  set_single_character_string_cache(FixedArray::cast(obj));

  // Allocate cache for external strings pointing to native source code.
  obj = AllocateFixedArray(Natives::GetBuiltinsCount());
  if (obj->IsFailure()) return false;
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


Object* Heap::InitializeNumberStringCache() {
  // Compute the size of the number string cache based on the max heap size.
  // max_semispace_size_ == 512 KB => number_string_cache_size = 32.
  // max_semispace_size_ ==   8 MB => number_string_cache_size = 16KB.
  int number_string_cache_size = max_semispace_size_ / 512;
  number_string_cache_size = Max(32, Min(16*KB, number_string_cache_size));
  Object* obj = AllocateFixedArray(number_string_cache_size * 2);
  if (!obj->IsFailure()) set_number_string_cache(FixedArray::cast(obj));
  return obj;
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


Object* Heap::SmiOrNumberFromDouble(double value,
                                    bool new_object,
                                    PretenureFlag pretenure) {
  // We need to distinguish the minus zero value and this cannot be
  // done after conversion to int. Doing this by comparing bit
  // patterns is faster than using fpclassify() et al.
  static const DoubleRepresentation plus_zero(0.0);
  static const DoubleRepresentation minus_zero(-0.0);
  static const DoubleRepresentation nan(OS::nan_value());
  ASSERT(minus_zero_value() != NULL);
  ASSERT(sizeof(plus_zero.value) == sizeof(plus_zero.bits));

  DoubleRepresentation rep(value);
  if (rep.bits == plus_zero.bits) return Smi::FromInt(0);  // not uncommon
  if (rep.bits == minus_zero.bits) {
    return new_object ? AllocateHeapNumber(-0.0, pretenure)
                      : minus_zero_value();
  }
  if (rep.bits == nan.bits) {
    return new_object
        ? AllocateHeapNumber(OS::nan_value(), pretenure)
        : nan_value();
  }

  // Try to represent the value as a tagged small integer.
  int int_value = FastD2I(value);
  if (value == FastI2D(int_value) && Smi::IsValid(int_value)) {
    return Smi::FromInt(int_value);
  }

  // Materialize the value in the heap.
  return AllocateHeapNumber(value, pretenure);
}


Object* Heap::NumberToString(Object* number) {
  Object* cached = GetNumberStringCache(number);
  if (cached != undefined_value()) {
    return cached;
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
  Object* result = AllocateStringFromAscii(CStrVector(str));

  if (!result->IsFailure()) {
    SetNumberStringCache(number, String::cast(result));
  }
  return result;
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


Object* Heap::NewNumberFromDouble(double value, PretenureFlag pretenure) {
  return SmiOrNumberFromDouble(value,
                               true /* number object must be new */,
                               pretenure);
}


Object* Heap::NumberFromDouble(double value, PretenureFlag pretenure) {
  return SmiOrNumberFromDouble(value,
                               false /* use preallocated NaN, -0.0 */,
                               pretenure);
}


Object* Heap::AllocateProxy(Address proxy, PretenureFlag pretenure) {
  // Statically ensure that it is safe to allocate proxies in paged spaces.
  STATIC_ASSERT(Proxy::kSize <= Page::kMaxHeapObjectSize);
  AllocationSpace space = (pretenure == TENURED) ? OLD_DATA_SPACE : NEW_SPACE;
  Object* result = Allocate(proxy_map(), space);
  if (result->IsFailure()) return result;

  Proxy::cast(result)->set_proxy(proxy);
  return result;
}


Object* Heap::AllocateSharedFunctionInfo(Object* name) {
  Object* result = Allocate(shared_function_info_map(), OLD_POINTER_SPACE);
  if (result->IsFailure()) return result;

  SharedFunctionInfo* share = SharedFunctionInfo::cast(result);
  share->set_name(name);
  Code* illegal = Builtins::builtin(Builtins::Illegal);
  share->set_code(illegal);
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
  share->set_this_property_assignments_count(0);
  share->set_this_property_assignments(undefined_value());
  return result;
}


// Returns true for a character in a range.  Both limits are inclusive.
static inline bool Between(uint32_t character, uint32_t from, uint32_t to) {
  // This makes uses of the the unsigned wraparound.
  return character - from <= to - from;
}


static inline Object* MakeOrFindTwoCharacterString(uint32_t c1, uint32_t c2) {
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
    Object* result = Heap::AllocateRawAsciiString(2);
    if (result->IsFailure()) return result;
    char* dest = SeqAsciiString::cast(result)->GetChars();
    dest[0] = c1;
    dest[1] = c2;
    return result;
  } else {
    Object* result = Heap::AllocateRawTwoByteString(2);
    if (result->IsFailure()) return result;
    uc16* dest = SeqTwoByteString::cast(result)->GetChars();
    dest[0] = c1;
    dest[1] = c2;
    return result;
  }
}


Object* Heap::AllocateConsString(String* first, String* second) {
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

  bool is_ascii = first->IsAsciiRepresentation()
      && second->IsAsciiRepresentation();

  // Make sure that an out of memory exception is thrown if the length
  // of the new cons string is too large.
  if (length > String::kMaxLength || length < 0) {
    Top::context()->mark_out_of_memory();
    return Failure::OutOfMemoryException();
  }

  // If the resulting string is small make a flat string.
  if (length < String::kMinNonFlatLength) {
    ASSERT(first->IsFlat());
    ASSERT(second->IsFlat());
    if (is_ascii) {
      Object* result = AllocateRawAsciiString(length);
      if (result->IsFailure()) return result;
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
      Object* result = AllocateRawTwoByteString(length);
      if (result->IsFailure()) return result;
      // Copy the characters into the new object.
      uc16* dest = SeqTwoByteString::cast(result)->GetChars();
      String::WriteToFlat(first, dest, 0, first_length);
      String::WriteToFlat(second, dest + first_length, 0, second_length);
      return result;
    }
  }

  Map* map = is_ascii ? cons_ascii_string_map() : cons_string_map();

  Object* result = Allocate(map, NEW_SPACE);
  if (result->IsFailure()) return result;

  AssertNoAllocation no_gc;
  ConsString* cons_string = ConsString::cast(result);
  WriteBarrierMode mode = cons_string->GetWriteBarrierMode(no_gc);
  cons_string->set_length(length);
  cons_string->set_hash_field(String::kEmptyHashField);
  cons_string->set_first(first, mode);
  cons_string->set_second(second, mode);
  return result;
}


Object* Heap::AllocateSubString(String* buffer,
                                int start,
                                int end) {
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
  if (!buffer->IsFlat()) {
    buffer->TryFlatten();
  }

  Object* result = buffer->IsAsciiRepresentation()
      ? AllocateRawAsciiString(length)
      : AllocateRawTwoByteString(length);
  if (result->IsFailure()) return result;
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


Object* Heap::AllocateExternalStringFromAscii(
    ExternalAsciiString::Resource* resource) {
  size_t length = resource->length();
  if (length > static_cast<size_t>(String::kMaxLength)) {
    Top::context()->mark_out_of_memory();
    return Failure::OutOfMemoryException();
  }

  Map* map = external_ascii_string_map();
  Object* result = Allocate(map, NEW_SPACE);
  if (result->IsFailure()) return result;

  ExternalAsciiString* external_string = ExternalAsciiString::cast(result);
  external_string->set_length(static_cast<int>(length));
  external_string->set_hash_field(String::kEmptyHashField);
  external_string->set_resource(resource);

  return result;
}


Object* Heap::AllocateExternalStringFromTwoByte(
    ExternalTwoByteString::Resource* resource) {
  size_t length = resource->length();
  if (length > static_cast<size_t>(String::kMaxLength)) {
    Top::context()->mark_out_of_memory();
    return Failure::OutOfMemoryException();
  }

  Map* map = Heap::external_string_map();
  Object* result = Allocate(map, NEW_SPACE);
  if (result->IsFailure()) return result;

  ExternalTwoByteString* external_string = ExternalTwoByteString::cast(result);
  external_string->set_length(static_cast<int>(length));
  external_string->set_hash_field(String::kEmptyHashField);
  external_string->set_resource(resource);

  return result;
}


Object* Heap::LookupSingleCharacterStringFromCode(uint16_t code) {
  if (code <= String::kMaxAsciiCharCode) {
    Object* value = Heap::single_character_string_cache()->get(code);
    if (value != Heap::undefined_value()) return value;

    char buffer[1];
    buffer[0] = static_cast<char>(code);
    Object* result = LookupSymbol(Vector<const char>(buffer, 1));

    if (result->IsFailure()) return result;
    Heap::single_character_string_cache()->set(code, result);
    return result;
  }

  Object* result = Heap::AllocateRawTwoByteString(1);
  if (result->IsFailure()) return result;
  String* answer = String::cast(result);
  answer->Set(0, code);
  return answer;
}


Object* Heap::AllocateByteArray(int length, PretenureFlag pretenure) {
  if (length < 0 || length > ByteArray::kMaxLength) {
    return Failure::OutOfMemoryException();
  }
  if (pretenure == NOT_TENURED) {
    return AllocateByteArray(length);
  }
  int size = ByteArray::SizeFor(length);
  Object* result = (size <= MaxObjectSizeInPagedSpace())
      ? old_data_space_->AllocateRaw(size)
      : lo_space_->AllocateRaw(size);
  if (result->IsFailure()) return result;

  reinterpret_cast<Array*>(result)->set_map(byte_array_map());
  reinterpret_cast<Array*>(result)->set_length(length);
  return result;
}


Object* Heap::AllocateByteArray(int length) {
  if (length < 0 || length > ByteArray::kMaxLength) {
    return Failure::OutOfMemoryException();
  }
  int size = ByteArray::SizeFor(length);
  AllocationSpace space =
      (size > MaxObjectSizeInPagedSpace()) ? LO_SPACE : NEW_SPACE;
  Object* result = AllocateRaw(size, space, OLD_DATA_SPACE);
  if (result->IsFailure()) return result;

  reinterpret_cast<Array*>(result)->set_map(byte_array_map());
  reinterpret_cast<Array*>(result)->set_length(length);
  return result;
}


void Heap::CreateFillerObjectAt(Address addr, int size) {
  if (size == 0) return;
  HeapObject* filler = HeapObject::FromAddress(addr);
  if (size == kPointerSize) {
    filler->set_map(Heap::one_pointer_filler_map());
  } else {
    filler->set_map(Heap::byte_array_map());
    ByteArray::cast(filler)->set_length(ByteArray::LengthFor(size));
  }
}


Object* Heap::AllocatePixelArray(int length,
                                 uint8_t* external_pointer,
                                 PretenureFlag pretenure) {
  AllocationSpace space = (pretenure == TENURED) ? OLD_DATA_SPACE : NEW_SPACE;
  Object* result = AllocateRaw(PixelArray::kAlignedSize, space, OLD_DATA_SPACE);
  if (result->IsFailure()) return result;

  reinterpret_cast<PixelArray*>(result)->set_map(pixel_array_map());
  reinterpret_cast<PixelArray*>(result)->set_length(length);
  reinterpret_cast<PixelArray*>(result)->set_external_pointer(external_pointer);

  return result;
}


Object* Heap::AllocateExternalArray(int length,
                                    ExternalArrayType array_type,
                                    void* external_pointer,
                                    PretenureFlag pretenure) {
  AllocationSpace space = (pretenure == TENURED) ? OLD_DATA_SPACE : NEW_SPACE;
  Object* result = AllocateRaw(ExternalArray::kAlignedSize,
                               space,
                               OLD_DATA_SPACE);
  if (result->IsFailure()) return result;

  reinterpret_cast<ExternalArray*>(result)->set_map(
      MapForExternalArrayType(array_type));
  reinterpret_cast<ExternalArray*>(result)->set_length(length);
  reinterpret_cast<ExternalArray*>(result)->set_external_pointer(
      external_pointer);

  return result;
}


Object* Heap::CreateCode(const CodeDesc& desc,
                         ZoneScopeInfo* sinfo,
                         Code::Flags flags,
                         Handle<Object> self_reference) {
  // Compute size
  int body_size = RoundUp(desc.instr_size + desc.reloc_size, kObjectAlignment);
  int sinfo_size = 0;
  if (sinfo != NULL) sinfo_size = sinfo->Serialize(NULL);
  int obj_size = Code::SizeFor(body_size, sinfo_size);
  ASSERT(IsAligned(obj_size, Code::kCodeAlignment));
  Object* result;
  if (obj_size > MaxObjectSizeInPagedSpace()) {
    result = lo_space_->AllocateRawCode(obj_size);
  } else {
    result = code_space_->AllocateRaw(obj_size);
  }

  if (result->IsFailure()) return result;

  // Initialize the object
  HeapObject::cast(result)->set_map(code_map());
  Code* code = Code::cast(result);
  ASSERT(!CodeRange::exists() || CodeRange::contains(code->address()));
  code->set_instruction_size(desc.instr_size);
  code->set_relocation_size(desc.reloc_size);
  code->set_sinfo_size(sinfo_size);
  code->set_flags(flags);
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
  if (sinfo != NULL) sinfo->Serialize(code);  // write scope info

#ifdef DEBUG
  code->Verify();
#endif
  return code;
}


Object* Heap::CopyCode(Code* code) {
  // Allocate an object the same size as the code object.
  int obj_size = code->Size();
  Object* result;
  if (obj_size > MaxObjectSizeInPagedSpace()) {
    result = lo_space_->AllocateRawCode(obj_size);
  } else {
    result = code_space_->AllocateRaw(obj_size);
  }

  if (result->IsFailure()) return result;

  // Copy code object.
  Address old_addr = code->address();
  Address new_addr = reinterpret_cast<HeapObject*>(result)->address();
  CopyBlock(reinterpret_cast<Object**>(new_addr),
            reinterpret_cast<Object**>(old_addr),
            obj_size);
  // Relocate the copy.
  Code* new_code = Code::cast(result);
  ASSERT(!CodeRange::exists() || CodeRange::contains(code->address()));
  new_code->Relocate(new_addr - old_addr);
  return new_code;
}


Object* Heap::Allocate(Map* map, AllocationSpace space) {
  ASSERT(gc_state_ == NOT_IN_GC);
  ASSERT(map->instance_type() != MAP_TYPE);
  // If allocation failures are disallowed, we may allocate in a different
  // space when new space is full and the object is not a large object.
  AllocationSpace retry_space =
      (space != NEW_SPACE) ? space : TargetSpaceId(map->instance_type());
  Object* result =
      AllocateRaw(map->instance_size(), space, retry_space);
  if (result->IsFailure()) return result;
  HeapObject::cast(result)->set_map(map);
#ifdef ENABLE_LOGGING_AND_PROFILING
  ProducerHeapProfile::RecordJSObjectAllocation(result);
#endif
  return result;
}


Object* Heap::InitializeFunction(JSFunction* function,
                                 SharedFunctionInfo* shared,
                                 Object* prototype) {
  ASSERT(!prototype->IsMap());
  function->initialize_properties();
  function->initialize_elements();
  function->set_shared(shared);
  function->set_prototype_or_initial_map(prototype);
  function->set_context(undefined_value());
  function->set_literals(empty_fixed_array());
  return function;
}


Object* Heap::AllocateFunctionPrototype(JSFunction* function) {
  // Allocate the prototype.  Make sure to use the object function
  // from the function's context, since the function can be from a
  // different context.
  JSFunction* object_function =
      function->context()->global_context()->object_function();
  Object* prototype = AllocateJSObject(object_function);
  if (prototype->IsFailure()) return prototype;
  // When creating the prototype for the function we must set its
  // constructor to the function.
  Object* result =
      JSObject::cast(prototype)->SetProperty(constructor_symbol(),
                                             function,
                                             DONT_ENUM);
  if (result->IsFailure()) return result;
  return prototype;
}


Object* Heap::AllocateFunction(Map* function_map,
                               SharedFunctionInfo* shared,
                               Object* prototype,
                               PretenureFlag pretenure) {
  AllocationSpace space =
      (pretenure == TENURED) ? OLD_POINTER_SPACE : NEW_SPACE;
  Object* result = Allocate(function_map, space);
  if (result->IsFailure()) return result;
  return InitializeFunction(JSFunction::cast(result), shared, prototype);
}


Object* Heap::AllocateArgumentsObject(Object* callee, int length) {
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
  Object* result =
      AllocateRaw(kArgumentsObjectSize, NEW_SPACE, OLD_POINTER_SPACE);
  if (result->IsFailure()) return result;

  // Copy the content. The arguments boilerplate doesn't have any
  // fields that point to new space so it's safe to skip the write
  // barrier here.
  CopyBlock(reinterpret_cast<Object**>(HeapObject::cast(result)->address()),
            reinterpret_cast<Object**>(boilerplate->address()),
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


Object* Heap::AllocateInitialMap(JSFunction* fun) {
  ASSERT(!fun->has_initial_map());

  // First create a new map with the size and number of in-object properties
  // suggested by the function.
  int instance_size = fun->shared()->CalculateInstanceSize();
  int in_object_properties = fun->shared()->CalculateInObjectProperties();
  Object* map_obj = Heap::AllocateMap(JS_OBJECT_TYPE, instance_size);
  if (map_obj->IsFailure()) return map_obj;

  // Fetch or allocate prototype.
  Object* prototype;
  if (fun->has_instance_prototype()) {
    prototype = fun->instance_prototype();
  } else {
    prototype = AllocateFunctionPrototype(fun);
    if (prototype->IsFailure()) return prototype;
  }
  Map* map = Map::cast(map_obj);
  map->set_inobject_properties(in_object_properties);
  map->set_unused_property_fields(in_object_properties);
  map->set_prototype(prototype);

  // If the function has only simple this property assignments add field
  // descriptors for these to the initial map as the object cannot be
  // constructed without having these properties.
  ASSERT(in_object_properties <= Map::kMaxPreAllocatedPropertyFields);
  if (fun->shared()->has_only_simple_this_property_assignments() &&
      fun->shared()->this_property_assignments_count() > 0) {
    int count = fun->shared()->this_property_assignments_count();
    if (count > in_object_properties) {
      count = in_object_properties;
    }
    Object* descriptors_obj = DescriptorArray::Allocate(count);
    if (descriptors_obj->IsFailure()) return descriptors_obj;
    DescriptorArray* descriptors = DescriptorArray::cast(descriptors_obj);
    for (int i = 0; i < count; i++) {
      String* name = fun->shared()->GetThisPropertyAssignmentName(i);
      ASSERT(name->IsSymbol());
      FieldDescriptor field(name, i, NONE);
      field.SetEnumerationIndex(i);
      descriptors->Set(i, &field);
    }
    descriptors->SetNextEnumerationIndex(count);
    descriptors->Sort();
    map->set_instance_descriptors(descriptors);
    map->set_pre_allocated_property_fields(count);
    map->set_unused_property_fields(in_object_properties - count);
  }
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
  obj->InitializeBody(map->instance_size());
}


Object* Heap::AllocateJSObjectFromMap(Map* map, PretenureFlag pretenure) {
  // JSFunctions should be allocated using AllocateFunction to be
  // properly initialized.
  ASSERT(map->instance_type() != JS_FUNCTION_TYPE);

  // Both types of globla objects should be allocated using
  // AllocateGloblaObject to be properly initialized.
  ASSERT(map->instance_type() != JS_GLOBAL_OBJECT_TYPE);
  ASSERT(map->instance_type() != JS_BUILTINS_OBJECT_TYPE);

  // Allocate the backing storage for the properties.
  int prop_size =
      map->pre_allocated_property_fields() +
      map->unused_property_fields() -
      map->inobject_properties();
  ASSERT(prop_size >= 0);
  Object* properties = AllocateFixedArray(prop_size, pretenure);
  if (properties->IsFailure()) return properties;

  // Allocate the JSObject.
  AllocationSpace space =
      (pretenure == TENURED) ? OLD_POINTER_SPACE : NEW_SPACE;
  if (map->instance_size() > MaxObjectSizeInPagedSpace()) space = LO_SPACE;
  Object* obj = Allocate(map, space);
  if (obj->IsFailure()) return obj;

  // Initialize the JSObject.
  InitializeJSObjectFromMap(JSObject::cast(obj),
                            FixedArray::cast(properties),
                            map);
  return obj;
}


Object* Heap::AllocateJSObject(JSFunction* constructor,
                               PretenureFlag pretenure) {
  // Allocate the initial map if absent.
  if (!constructor->has_initial_map()) {
    Object* initial_map = AllocateInitialMap(constructor);
    if (initial_map->IsFailure()) return initial_map;
    constructor->set_initial_map(Map::cast(initial_map));
    Map::cast(initial_map)->set_constructor(constructor);
  }
  // Allocate the object based on the constructors initial map.
  Object* result =
      AllocateJSObjectFromMap(constructor->initial_map(), pretenure);
  // Make sure result is NOT a global object if valid.
  ASSERT(result->IsFailure() || !result->IsGlobalObject());
  return result;
}


Object* Heap::AllocateGlobalObject(JSFunction* constructor) {
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
  Object* obj =
      StringDictionary::Allocate(
          map->NumberOfDescribedProperties() * 2 + initial_size);
  if (obj->IsFailure()) return obj;
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
    value = Heap::AllocateJSGlobalPropertyCell(value);
    if (value->IsFailure()) return value;

    Object* result = dictionary->Add(descs->GetKey(i), value, d);
    if (result->IsFailure()) return result;
    dictionary = StringDictionary::cast(result);
  }

  // Allocate the global object and initialize it with the backing store.
  obj = Allocate(map, OLD_POINTER_SPACE);
  if (obj->IsFailure()) return obj;
  JSObject* global = JSObject::cast(obj);
  InitializeJSObjectFromMap(global, dictionary, map);

  // Create a new map for the global object.
  obj = map->CopyDropDescriptors();
  if (obj->IsFailure()) return obj;
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


Object* Heap::CopyJSObject(JSObject* source) {
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
    clone = AllocateRaw(object_size, NEW_SPACE, OLD_POINTER_SPACE);
    if (clone->IsFailure()) return clone;
    Address clone_address = HeapObject::cast(clone)->address();
    CopyBlock(reinterpret_cast<Object**>(clone_address),
              reinterpret_cast<Object**>(source->address()),
              object_size);
    // Update write barrier for all fields that lie beyond the header.
    for (int offset = JSObject::kHeaderSize;
         offset < object_size;
         offset += kPointerSize) {
      RecordWrite(clone_address, offset);
    }
  } else {
    clone = new_space_.AllocateRaw(object_size);
    if (clone->IsFailure()) return clone;
    ASSERT(Heap::InNewSpace(clone));
    // Since we know the clone is allocated in new space, we can copy
    // the contents without worrying about updating the write barrier.
    CopyBlock(reinterpret_cast<Object**>(HeapObject::cast(clone)->address()),
              reinterpret_cast<Object**>(source->address()),
              object_size);
  }

  FixedArray* elements = FixedArray::cast(source->elements());
  FixedArray* properties = FixedArray::cast(source->properties());
  // Update elements if necessary.
  if (elements->length()> 0) {
    Object* elem = CopyFixedArray(elements);
    if (elem->IsFailure()) return elem;
    JSObject::cast(clone)->set_elements(FixedArray::cast(elem));
  }
  // Update properties if necessary.
  if (properties->length() > 0) {
    Object* prop = CopyFixedArray(properties);
    if (prop->IsFailure()) return prop;
    JSObject::cast(clone)->set_properties(FixedArray::cast(prop));
  }
  // Return the new clone.
#ifdef ENABLE_LOGGING_AND_PROFILING
  ProducerHeapProfile::RecordJSObjectAllocation(clone);
#endif
  return clone;
}


Object* Heap::ReinitializeJSGlobalProxy(JSFunction* constructor,
                                        JSGlobalProxy* object) {
  // Allocate initial map if absent.
  if (!constructor->has_initial_map()) {
    Object* initial_map = AllocateInitialMap(constructor);
    if (initial_map->IsFailure()) return initial_map;
    constructor->set_initial_map(Map::cast(initial_map));
    Map::cast(initial_map)->set_constructor(constructor);
  }

  Map* map = constructor->initial_map();

  // Check that the already allocated object has the same size as
  // objects allocated using the constructor.
  ASSERT(map->instance_size() == object->map()->instance_size());

  // Allocate the backing storage for the properties.
  int prop_size = map->unused_property_fields() - map->inobject_properties();
  Object* properties = AllocateFixedArray(prop_size, TENURED);
  if (properties->IsFailure()) return properties;

  // Reset the map for the object.
  object->set_map(constructor->initial_map());

  // Reinitialize the object from the constructor map.
  InitializeJSObjectFromMap(object, FixedArray::cast(properties), map);
  return object;
}


Object* Heap::AllocateStringFromAscii(Vector<const char> string,
                                      PretenureFlag pretenure) {
  Object* result = AllocateRawAsciiString(string.length(), pretenure);
  if (result->IsFailure()) return result;

  // Copy the characters into the new object.
  SeqAsciiString* string_result = SeqAsciiString::cast(result);
  for (int i = 0; i < string.length(); i++) {
    string_result->SeqAsciiStringSet(i, string[i]);
  }
  return result;
}


Object* Heap::AllocateStringFromUtf8(Vector<const char> string,
                                     PretenureFlag pretenure) {
  // Count the number of characters in the UTF-8 string and check if
  // it is an ASCII string.
  Access<Scanner::Utf8Decoder> decoder(Scanner::utf8_decoder());
  decoder->Reset(string.start(), string.length());
  int chars = 0;
  bool is_ascii = true;
  while (decoder->has_more()) {
    uc32 r = decoder->GetNext();
    if (r > String::kMaxAsciiCharCode) is_ascii = false;
    chars++;
  }

  // If the string is ascii, we do not need to convert the characters
  // since UTF8 is backwards compatible with ascii.
  if (is_ascii) return AllocateStringFromAscii(string, pretenure);

  Object* result = AllocateRawTwoByteString(chars, pretenure);
  if (result->IsFailure()) return result;

  // Convert and copy the characters into the new object.
  String* string_result = String::cast(result);
  decoder->Reset(string.start(), string.length());
  for (int i = 0; i < chars; i++) {
    uc32 r = decoder->GetNext();
    string_result->Set(i, r);
  }
  return result;
}


Object* Heap::AllocateStringFromTwoByte(Vector<const uc16> string,
                                        PretenureFlag pretenure) {
  // Check if the string is an ASCII string.
  int i = 0;
  while (i < string.length() && string[i] <= String::kMaxAsciiCharCode) i++;

  Object* result;
  if (i == string.length()) {  // It's an ASCII string.
    result = AllocateRawAsciiString(string.length(), pretenure);
  } else {  // It's not an ASCII string.
    result = AllocateRawTwoByteString(string.length(), pretenure);
  }
  if (result->IsFailure()) return result;

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

  // No match found.
  return NULL;
}


Object* Heap::AllocateInternalSymbol(unibrow::CharacterStream* buffer,
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
  Object* result = (size > MaxObjectSizeInPagedSpace())
      ? lo_space_->AllocateRaw(size)
      : old_data_space_->AllocateRaw(size);
  if (result->IsFailure()) return result;

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


Object* Heap::AllocateRawAsciiString(int length, PretenureFlag pretenure) {
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
  Object* result = AllocateRaw(size, space, retry_space);
  if (result->IsFailure()) return result;

  // Partially initialize the object.
  HeapObject::cast(result)->set_map(ascii_string_map());
  String::cast(result)->set_length(length);
  String::cast(result)->set_hash_field(String::kEmptyHashField);
  ASSERT_EQ(size, HeapObject::cast(result)->Size());
  return result;
}


Object* Heap::AllocateRawTwoByteString(int length, PretenureFlag pretenure) {
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
  Object* result = AllocateRaw(size, space, retry_space);
  if (result->IsFailure()) return result;

  // Partially initialize the object.
  HeapObject::cast(result)->set_map(string_map());
  String::cast(result)->set_length(length);
  String::cast(result)->set_hash_field(String::kEmptyHashField);
  ASSERT_EQ(size, HeapObject::cast(result)->Size());
  return result;
}


Object* Heap::AllocateEmptyFixedArray() {
  int size = FixedArray::SizeFor(0);
  Object* result = AllocateRaw(size, OLD_DATA_SPACE, OLD_DATA_SPACE);
  if (result->IsFailure()) return result;
  // Initialize the object.
  reinterpret_cast<Array*>(result)->set_map(fixed_array_map());
  reinterpret_cast<Array*>(result)->set_length(0);
  return result;
}


Object* Heap::AllocateRawFixedArray(int length) {
  if (length < 0 || length > FixedArray::kMaxLength) {
    return Failure::OutOfMemoryException();
  }
  // Use the general function if we're forced to always allocate.
  if (always_allocate()) return AllocateFixedArray(length, TENURED);
  // Allocate the raw data for a fixed array.
  int size = FixedArray::SizeFor(length);
  return size <= kMaxObjectSizeInNewSpace
      ? new_space_.AllocateRaw(size)
      : lo_space_->AllocateRawFixedArray(size);
}


Object* Heap::CopyFixedArray(FixedArray* src) {
  int len = src->length();
  Object* obj = AllocateRawFixedArray(len);
  if (obj->IsFailure()) return obj;
  if (Heap::InNewSpace(obj)) {
    HeapObject* dst = HeapObject::cast(obj);
    CopyBlock(reinterpret_cast<Object**>(dst->address()),
              reinterpret_cast<Object**>(src->address()),
              FixedArray::SizeFor(len));
    return obj;
  }
  HeapObject::cast(obj)->set_map(src->map());
  FixedArray* result = FixedArray::cast(obj);
  result->set_length(len);

  // Copy the content
  AssertNoAllocation no_gc;
  WriteBarrierMode mode = result->GetWriteBarrierMode(no_gc);
  for (int i = 0; i < len; i++) result->set(i, src->get(i), mode);
  return result;
}


Object* Heap::AllocateFixedArray(int length) {
  ASSERT(length >= 0);
  if (length == 0) return empty_fixed_array();
  Object* result = AllocateRawFixedArray(length);
  if (!result->IsFailure()) {
    // Initialize header.
    reinterpret_cast<Array*>(result)->set_map(fixed_array_map());
    FixedArray* array = FixedArray::cast(result);
    array->set_length(length);
    Object* value = undefined_value();
    // Initialize body.
    for (int index = 0; index < length; index++) {
      ASSERT(!Heap::InNewSpace(value));  // value = undefined
      array->set(index, value, SKIP_WRITE_BARRIER);
    }
  }
  return result;
}


Object* Heap::AllocateFixedArray(int length, PretenureFlag pretenure) {
  ASSERT(length >= 0);
  ASSERT(empty_fixed_array()->IsFixedArray());
  if (length < 0 || length > FixedArray::kMaxLength) {
    return Failure::OutOfMemoryException();
  }
  if (length == 0) return empty_fixed_array();

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

  // Specialize allocation for the space.
  Object* result = Failure::OutOfMemoryException();
  if (space == NEW_SPACE) {
    // We cannot use Heap::AllocateRaw() because it will not properly
    // allocate extra remembered set bits if always_allocate() is true and
    // new space allocation fails.
    result = new_space_.AllocateRaw(size);
    if (result->IsFailure() && always_allocate()) {
      if (size <= MaxObjectSizeInPagedSpace()) {
        result = old_pointer_space_->AllocateRaw(size);
      } else {
        result = lo_space_->AllocateRawFixedArray(size);
      }
    }
  } else if (space == OLD_POINTER_SPACE) {
    result = old_pointer_space_->AllocateRaw(size);
  } else {
    ASSERT(space == LO_SPACE);
    result = lo_space_->AllocateRawFixedArray(size);
  }
  if (result->IsFailure()) return result;

  // Initialize the object.
  reinterpret_cast<Array*>(result)->set_map(fixed_array_map());
  FixedArray* array = FixedArray::cast(result);
  array->set_length(length);
  Object* value = undefined_value();
  for (int index = 0; index < length; index++) {
    ASSERT(!Heap::InNewSpace(value));  // value = undefined
    array->set(index, value, SKIP_WRITE_BARRIER);
  }
  return array;
}


Object* Heap::AllocateFixedArrayWithHoles(int length) {
  if (length == 0) return empty_fixed_array();
  Object* result = AllocateRawFixedArray(length);
  if (!result->IsFailure()) {
    // Initialize header.
    reinterpret_cast<Array*>(result)->set_map(fixed_array_map());
    FixedArray* array = FixedArray::cast(result);
    array->set_length(length);
    // Initialize body.
    Object* value = the_hole_value();
    for (int index = 0; index < length; index++)  {
      ASSERT(!Heap::InNewSpace(value));  // value = the hole
      array->set(index, value, SKIP_WRITE_BARRIER);
    }
  }
  return result;
}


Object* Heap::AllocateHashTable(int length) {
  Object* result = Heap::AllocateFixedArray(length);
  if (result->IsFailure()) return result;
  reinterpret_cast<Array*>(result)->set_map(hash_table_map());
  ASSERT(result->IsHashTable());
  return result;
}


Object* Heap::AllocateGlobalContext() {
  Object* result = Heap::AllocateFixedArray(Context::GLOBAL_CONTEXT_SLOTS);
  if (result->IsFailure()) return result;
  Context* context = reinterpret_cast<Context*>(result);
  context->set_map(global_context_map());
  ASSERT(context->IsGlobalContext());
  ASSERT(result->IsContext());
  return result;
}


Object* Heap::AllocateFunctionContext(int length, JSFunction* function) {
  ASSERT(length >= Context::MIN_CONTEXT_SLOTS);
  Object* result = Heap::AllocateFixedArray(length);
  if (result->IsFailure()) return result;
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


Object* Heap::AllocateWithContext(Context* previous,
                                  JSObject* extension,
                                  bool is_catch_context) {
  Object* result = Heap::AllocateFixedArray(Context::MIN_CONTEXT_SLOTS);
  if (result->IsFailure()) return result;
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


Object* Heap::AllocateStruct(InstanceType type) {
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
  Object* result = Heap::Allocate(map, space);
  if (result->IsFailure()) return result;
  Struct::cast(result)->InitializeBody(size);
  return result;
}


bool Heap::IdleNotification() {
  static const int kIdlesBeforeScavenge = 4;
  static const int kIdlesBeforeMarkSweep = 7;
  static const int kIdlesBeforeMarkCompact = 8;
  static int number_idle_notifications = 0;
  static int last_gc_count = gc_count_;

  bool finished = false;

  if (last_gc_count == gc_count_) {
    number_idle_notifications++;
  } else {
    number_idle_notifications = 0;
    last_gc_count = gc_count_;
  }

  if (number_idle_notifications == kIdlesBeforeScavenge) {
    CollectGarbage(0, NEW_SPACE);
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
    number_idle_notifications = 0;
    finished = true;
  }

  // Uncommit unused memory in new space.
  Heap::UncommitFromSpace();
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
  PrintF("old_gen_promotion_limit_ %d\n", old_gen_promotion_limit_);
  PrintF("old_gen_allocation_limit_ %d\n", old_gen_allocation_limit_);

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
void Heap::Verify() {
  ASSERT(HasBeenSetup());

  VerifyPointersVisitor visitor;
  IterateRoots(&visitor, VISIT_ONLY_STRONG);

  new_space_.Verify();

  VerifyPointersAndRSetVisitor rset_visitor;
  old_pointer_space_->Verify(&rset_visitor);
  map_space_->Verify(&rset_visitor);

  VerifyPointersVisitor no_rset_visitor;
  old_data_space_->Verify(&no_rset_visitor);
  code_space_->Verify(&no_rset_visitor);
  cell_space_->Verify(&no_rset_visitor);

  lo_space_->Verify();
}
#endif  // DEBUG


Object* Heap::LookupSymbol(Vector<const char> string) {
  Object* symbol = NULL;
  Object* new_table = symbol_table()->LookupSymbol(string, &symbol);
  if (new_table->IsFailure()) return new_table;
  // Can't use set_symbol_table because SymbolTable::cast knows that
  // SymbolTable is a singleton and checks for identity.
  roots_[kSymbolTableRootIndex] = new_table;
  ASSERT(symbol != NULL);
  return symbol;
}


Object* Heap::LookupSymbol(String* string) {
  if (string->IsSymbol()) return string;
  Object* symbol = NULL;
  Object* new_table = symbol_table()->LookupString(string, &symbol);
  if (new_table->IsFailure()) return new_table;
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
  ASSERT(reinterpret_cast<Object*>(kFromSpaceZapValue)->IsHeapObject());
  for (Address a = new_space_.FromSpaceLow();
       a < new_space_.FromSpaceHigh();
       a += kPointerSize) {
    Memory::Address_at(a) = kFromSpaceZapValue;
  }
}
#endif  // DEBUG


int Heap::IterateRSetRange(Address object_start,
                           Address object_end,
                           Address rset_start,
                           ObjectSlotCallback copy_object_func) {
  Address object_address = object_start;
  Address rset_address = rset_start;
  int set_bits_count = 0;

  // Loop over all the pointers in [object_start, object_end).
  while (object_address < object_end) {
    uint32_t rset_word = Memory::uint32_at(rset_address);
    if (rset_word != 0) {
      uint32_t result_rset = rset_word;
      for (uint32_t bitmask = 1; bitmask != 0; bitmask = bitmask << 1) {
        // Do not dereference pointers at or past object_end.
        if ((rset_word & bitmask) != 0 && object_address < object_end) {
          Object** object_p = reinterpret_cast<Object**>(object_address);
          if (Heap::InNewSpace(*object_p)) {
            copy_object_func(reinterpret_cast<HeapObject**>(object_p));
          }
          // If this pointer does not need to be remembered anymore, clear
          // the remembered set bit.
          if (!Heap::InNewSpace(*object_p)) result_rset &= ~bitmask;
          set_bits_count++;
        }
        object_address += kPointerSize;
      }
      // Update the remembered set if it has changed.
      if (result_rset != rset_word) {
        Memory::uint32_at(rset_address) = result_rset;
      }
    } else {
      // No bits in the word were set.  This is the common case.
      object_address += kPointerSize * kBitsPerInt;
    }
    rset_address += kIntSize;
  }
  return set_bits_count;
}


void Heap::IterateRSet(PagedSpace* space, ObjectSlotCallback copy_object_func) {
  ASSERT(Page::is_rset_in_use());
  ASSERT(space == old_pointer_space_ || space == map_space_);

  static void* paged_rset_histogram = StatsTable::CreateHistogram(
      "V8.RSetPaged",
      0,
      Page::kObjectAreaSize / kPointerSize,
      30);

  PageIterator it(space, PageIterator::PAGES_IN_USE);
  while (it.has_next()) {
    Page* page = it.next();
    int count = IterateRSetRange(page->ObjectAreaStart(), page->AllocationTop(),
                                 page->RSetStart(), copy_object_func);
    if (paged_rset_histogram != NULL) {
      StatsTable::AddHistogramSample(paged_rset_histogram, count);
    }
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

  v->VisitPointer(bit_cast<Object**, String**>(&hidden_symbol_));
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
bool Heap::ConfigureHeap(int max_semispace_size, int max_old_gen_size) {
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
  return ConfigureHeap(FLAG_max_new_space_size / 2, FLAG_max_old_space_size);
}


void Heap::RecordStats(HeapStats* stats) {
  *stats->start_marker = 0xDECADE00;
  *stats->end_marker = 0xDECADE01;
  *stats->new_space_size = new_space_.Size();
  *stats->new_space_capacity = new_space_.Capacity();
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
}


int Heap::PromotedSpaceSize() {
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

  // Setup memory allocator and reserve a chunk of memory for new
  // space.  The chunk is double the size of the requested reserved
  // new space size to ensure that we can find a pair of semispaces that
  // are contiguous and aligned to their size.
  if (!MemoryAllocator::Setup(MaxReserved())) return false;
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
  }

  LOG(IntEvent("heap-capacity", Capacity()));
  LOG(IntEvent("heap-available", Available()));

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


#ifdef DEBUG

class PrintHandleVisitor: public ObjectVisitor {
 public:
  void VisitPointers(Object** start, Object** end) {
    for (Object** p = start; p < end; p++)
      PrintF("  handle %p to %p\n", p, *p);
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


SpaceIterator::SpaceIterator() : current_space_(FIRST_SPACE), iterator_(NULL) {
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
      iterator_ = new SemiSpaceIterator(Heap::new_space());
      break;
    case OLD_POINTER_SPACE:
      iterator_ = new HeapObjectIterator(Heap::old_pointer_space());
      break;
    case OLD_DATA_SPACE:
      iterator_ = new HeapObjectIterator(Heap::old_data_space());
      break;
    case CODE_SPACE:
      iterator_ = new HeapObjectIterator(Heap::code_space());
      break;
    case MAP_SPACE:
      iterator_ = new HeapObjectIterator(Heap::map_space());
      break;
    case CELL_SPACE:
      iterator_ = new HeapObjectIterator(Heap::cell_space());
      break;
    case LO_SPACE:
      iterator_ = new LargeObjectIterator(Heap::lo_space());
      break;
  }

  // Return the newly allocated iterator;
  ASSERT(iterator_ != NULL);
  return iterator_;
}


HeapIterator::HeapIterator() {
  Init();
}


HeapIterator::~HeapIterator() {
  Shutdown();
}


void HeapIterator::Init() {
  // Start the iteration.
  space_iterator_ = new SpaceIterator();
  object_iterator_ = space_iterator_->next();
}


void HeapIterator::Shutdown() {
  // Make sure the last iterator is deallocated.
  delete space_iterator_;
  space_iterator_ = NULL;
  object_iterator_ = NULL;
}


HeapObject* HeapIterator::next() {
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

static bool search_for_any_global;
static Object* search_target;
static bool found_target;
static List<Object*> object_stack(20);


// Tags 0, 1, and 3 are used. Use 2 for marking visited HeapObject.
static const int kMarkTag = 2;

static void MarkObjectRecursively(Object** p);
class MarkObjectVisitor : public ObjectVisitor {
 public:
  void VisitPointers(Object** start, Object** end) {
    // Copy all HeapObject pointers in [start, end)
    for (Object** p = start; p < end; p++) {
      if ((*p)->IsHeapObject())
        MarkObjectRecursively(p);
    }
  }
};

static MarkObjectVisitor mark_visitor;

static void MarkObjectRecursively(Object** p) {
  if (!(*p)->IsHeapObject()) return;

  HeapObject* obj = HeapObject::cast(*p);

  Object* map = obj->map();

  if (!map->IsHeapObject()) return;  // visited before

  if (found_target) return;  // stop if target found
  object_stack.Add(obj);
  if ((search_for_any_global && obj->IsJSGlobalObject()) ||
      (!search_for_any_global && (obj == search_target))) {
    found_target = true;
    return;
  }

  // not visited yet
  Map* map_p = reinterpret_cast<Map*>(HeapObject::cast(map));

  Address map_addr = map_p->address();

  obj->set_map(reinterpret_cast<Map*>(map_addr + kMarkTag));

  MarkObjectRecursively(&map);

  obj->IterateBody(map_p->instance_type(), obj->SizeFromMap(map_p),
                   &mark_visitor);

  if (!found_target)  // don't pop if found the target
    object_stack.RemoveLast();
}


static void UnmarkObjectRecursively(Object** p);
class UnmarkObjectVisitor : public ObjectVisitor {
 public:
  void VisitPointers(Object** start, Object** end) {
    // Copy all HeapObject pointers in [start, end)
    for (Object** p = start; p < end; p++) {
      if ((*p)->IsHeapObject())
        UnmarkObjectRecursively(p);
    }
  }
};

static UnmarkObjectVisitor unmark_visitor;

static void UnmarkObjectRecursively(Object** p) {
  if (!(*p)->IsHeapObject()) return;

  HeapObject* obj = HeapObject::cast(*p);

  Object* map = obj->map();

  if (map->IsHeapObject()) return;  // unmarked already

  Address map_addr = reinterpret_cast<Address>(map);

  map_addr -= kMarkTag;

  ASSERT_TAG_ALIGNED(map_addr);

  HeapObject* map_p = HeapObject::FromAddress(map_addr);

  obj->set_map(reinterpret_cast<Map*>(map_p));

  UnmarkObjectRecursively(reinterpret_cast<Object**>(&map_p));

  obj->IterateBody(Map::cast(map_p)->instance_type(),
                   obj->SizeFromMap(Map::cast(map_p)),
                   &unmark_visitor);
}


static void MarkRootObjectRecursively(Object** root) {
  if (search_for_any_global) {
    ASSERT(search_target == NULL);
  } else {
    ASSERT(search_target->IsHeapObject());
  }
  found_target = false;
  object_stack.Clear();

  MarkObjectRecursively(root);
  UnmarkObjectRecursively(root);

  if (found_target) {
    PrintF("=====================================\n");
    PrintF("====        Path to object       ====\n");
    PrintF("=====================================\n\n");

    ASSERT(!object_stack.is_empty());
    for (int i = 0; i < object_stack.length(); i++) {
      if (i > 0) PrintF("\n     |\n     |\n     V\n\n");
      Object* obj = object_stack[i];
      obj->Print();
    }
    PrintF("=====================================\n");
  }
}


// Helper class for visiting HeapObjects recursively.
class MarkRootVisitor: public ObjectVisitor {
 public:
  void VisitPointers(Object** start, Object** end) {
    // Visit all HeapObject pointers in [start, end)
    for (Object** p = start; p < end; p++) {
      if ((*p)->IsHeapObject())
        MarkRootObjectRecursively(p);
    }
  }
};


// Triggers a depth-first traversal of reachable objects from roots
// and finds a path to a specific heap object and prints it.
void Heap::TracePathToObject(Object* target) {
  search_target = target;
  search_for_any_global = false;

  MarkRootVisitor root_visitor;
  IterateRoots(&root_visitor, VISIT_ONLY_STRONG);
}


// Triggers a depth-first traversal of reachable objects from roots
// and finds a path to any global object and prints it. Useful for
// determining the source for leaks of global objects.
void Heap::TracePathToGlobal() {
  search_target = NULL;
  search_for_any_global = true;

  MarkRootVisitor root_visitor;
  IterateRoots(&root_visitor, VISIT_ONLY_STRONG);
}
#endif


GCTracer::GCTracer()
    : start_time_(0.0),
      start_size_(0.0),
      gc_count_(0),
      full_gc_count_(0),
      is_compacting_(false),
      marked_count_(0) {
  // These two fields reflect the state of the previous full collection.
  // Set them before they are changed by the collector.
  previous_has_compacted_ = MarkCompactCollector::HasCompacted();
  previous_marked_count_ = MarkCompactCollector::previous_marked_count();
  if (!FLAG_trace_gc) return;
  start_time_ = OS::TimeCurrentMillis();
  start_size_ = SizeOfHeapObjects();
}


GCTracer::~GCTracer() {
  if (!FLAG_trace_gc) return;
  // Printf ONE line iff flag is set.
  PrintF("%s %.1f -> %.1f MB, %d ms.\n",
         CollectorString(),
         start_size_, SizeOfHeapObjects(),
         static_cast<int>(OS::TimeCurrentMillis() - start_time_));

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
  return (addr_hash ^ name->Hash()) & kCapacityMask;
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
bool Heap::GarbageCollectionGreedyCheck() {
  ASSERT(FLAG_gc_greedy);
  if (Bootstrapper::IsActive()) return true;
  if (disallow_allocation_failure()) return true;
  return CollectGarbage(0, NEW_SPACE);
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
