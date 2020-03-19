// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/heap.h"

#include <cinttypes>
#include <iomanip>
#include <unordered_map>
#include <unordered_set>

#include "src/api/api-inl.h"
#include "src/base/bits.h"
#include "src/base/flags.h"
#include "src/base/once.h"
#include "src/base/utils/random-number-generator.h"
#include "src/builtins/accessors.h"
#include "src/codegen/assembler-inl.h"
#include "src/codegen/compilation-cache.h"
#include "src/common/globals.h"
#include "src/debug/debug.h"
#include "src/deoptimizer/deoptimizer.h"
#include "src/execution/microtask-queue.h"
#include "src/execution/runtime-profiler.h"
#include "src/execution/v8threads.h"
#include "src/execution/vm-state-inl.h"
#include "src/handles/global-handles.h"
#include "src/heap/array-buffer-collector.h"
#include "src/heap/array-buffer-sweeper.h"
#include "src/heap/array-buffer-tracker-inl.h"
#include "src/heap/barrier.h"
#include "src/heap/code-stats.h"
#include "src/heap/combined-heap.h"
#include "src/heap/concurrent-marking.h"
#include "src/heap/embedder-tracing.h"
#include "src/heap/finalization-registry-cleanup-task.h"
#include "src/heap/gc-idle-time-handler.h"
#include "src/heap/gc-tracer.h"
#include "src/heap/heap-controller.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/heap/incremental-marking-inl.h"
#include "src/heap/incremental-marking.h"
#include "src/heap/local-heap.h"
#include "src/heap/mark-compact-inl.h"
#include "src/heap/mark-compact.h"
#include "src/heap/memory-measurement.h"
#include "src/heap/memory-reducer.h"
#include "src/heap/object-stats.h"
#include "src/heap/objects-visiting-inl.h"
#include "src/heap/objects-visiting.h"
#include "src/heap/read-only-heap.h"
#include "src/heap/remembered-set.h"
#include "src/heap/scavenge-job.h"
#include "src/heap/scavenger-inl.h"
#include "src/heap/stress-marking-observer.h"
#include "src/heap/stress-scavenge-observer.h"
#include "src/heap/sweeper.h"
#include "src/init/bootstrapper.h"
#include "src/init/v8.h"
#include "src/interpreter/interpreter.h"
#include "src/logging/log.h"
#include "src/numbers/conversions.h"
#include "src/objects/data-handler.h"
#include "src/objects/feedback-vector.h"
#include "src/objects/free-space-inl.h"
#include "src/objects/hash-table-inl.h"
#include "src/objects/maybe-object.h"
#include "src/objects/shared-function-info.h"
#include "src/objects/slots-atomic-inl.h"
#include "src/objects/slots-inl.h"
#include "src/regexp/regexp.h"
#include "src/snapshot/embedded/embedded-data.h"
#include "src/snapshot/serializer-common.h"
#include "src/snapshot/snapshot.h"
#include "src/strings/string-stream.h"
#include "src/strings/unicode-decoder.h"
#include "src/strings/unicode-inl.h"
#include "src/tracing/trace-event.h"
#include "src/utils/utils-inl.h"
#include "src/utils/utils.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#ifdef V8_ENABLE_THIRD_PARTY_HEAP
Isolate* Heap::GetIsolateFromWritableObject(HeapObject object) {
  return reinterpret_cast<Isolate*>(
      third_party_heap::Heap::GetIsolate(object.address()));
}
#endif

// These are outside the Heap class so they can be forward-declared
// in heap-write-barrier-inl.h.
bool Heap_PageFlagsAreConsistent(HeapObject object) {
  return Heap::PageFlagsAreConsistent(object);
}

void Heap_GenerationalBarrierSlow(HeapObject object, Address slot,
                                  HeapObject value) {
  Heap::GenerationalBarrierSlow(object, slot, value);
}

void Heap_MarkingBarrierSlow(HeapObject object, Address slot,
                             HeapObject value) {
  Heap::MarkingBarrierSlow(object, slot, value);
}

void Heap_WriteBarrierForCodeSlow(Code host) {
  Heap::WriteBarrierForCodeSlow(host);
}

void Heap_MarkingBarrierForArrayBufferExtensionSlow(
    HeapObject object, ArrayBufferExtension* extension) {
  Heap::MarkingBarrierForArrayBufferExtensionSlow(object, extension);
}

void Heap_GenerationalBarrierForCodeSlow(Code host, RelocInfo* rinfo,
                                         HeapObject object) {
  Heap::GenerationalBarrierForCodeSlow(host, rinfo, object);
}

void Heap_MarkingBarrierForCodeSlow(Code host, RelocInfo* rinfo,
                                    HeapObject object) {
  Heap::MarkingBarrierForCodeSlow(host, rinfo, object);
}

void Heap_MarkingBarrierForDescriptorArraySlow(Heap* heap, HeapObject host,
                                               HeapObject descriptor_array,
                                               int number_of_own_descriptors) {
  Heap::MarkingBarrierForDescriptorArraySlow(heap, host, descriptor_array,
                                             number_of_own_descriptors);
}

void Heap_GenerationalEphemeronKeyBarrierSlow(Heap* heap,
                                              EphemeronHashTable table,
                                              Address slot) {
  heap->RecordEphemeronKeyWrite(table, slot);
}

void Heap::SetArgumentsAdaptorDeoptPCOffset(int pc_offset) {
  DCHECK_EQ(Smi::zero(), arguments_adaptor_deopt_pc_offset());
  set_arguments_adaptor_deopt_pc_offset(Smi::FromInt(pc_offset));
}

void Heap::SetConstructStubCreateDeoptPCOffset(int pc_offset) {
  DCHECK_EQ(Smi::zero(), construct_stub_create_deopt_pc_offset());
  set_construct_stub_create_deopt_pc_offset(Smi::FromInt(pc_offset));
}

void Heap::SetConstructStubInvokeDeoptPCOffset(int pc_offset) {
  DCHECK_EQ(Smi::zero(), construct_stub_invoke_deopt_pc_offset());
  set_construct_stub_invoke_deopt_pc_offset(Smi::FromInt(pc_offset));
}

void Heap::SetInterpreterEntryReturnPCOffset(int pc_offset) {
  DCHECK_EQ(Smi::zero(), interpreter_entry_return_pc_offset());
  set_interpreter_entry_return_pc_offset(Smi::FromInt(pc_offset));
}

void Heap::SetSerializedObjects(FixedArray objects) {
  DCHECK(isolate()->serializer_enabled());
  set_serialized_objects(objects);
}

void Heap::SetSerializedGlobalProxySizes(FixedArray sizes) {
  DCHECK(isolate()->serializer_enabled());
  set_serialized_global_proxy_sizes(sizes);
}

bool Heap::GCCallbackTuple::operator==(
    const Heap::GCCallbackTuple& other) const {
  return other.callback == callback && other.data == data;
}

Heap::GCCallbackTuple& Heap::GCCallbackTuple::operator=(
    const Heap::GCCallbackTuple& other) V8_NOEXCEPT = default;

struct Heap::StrongRootsList {
  FullObjectSlot start;
  FullObjectSlot end;
  StrongRootsList* next;
};

class ScavengeTaskObserver : public AllocationObserver {
 public:
  ScavengeTaskObserver(Heap* heap, intptr_t step_size)
      : AllocationObserver(step_size), heap_(heap) {}

  void Step(int bytes_allocated, Address, size_t) override {
    heap_->ScheduleScavengeTaskIfNeeded();
  }

 private:
  Heap* heap_;
};

Heap::Heap()
    : isolate_(isolate()),
      memory_pressure_level_(MemoryPressureLevel::kNone),
      global_pretenuring_feedback_(kInitialFeedbackCapacity),
      local_heaps_head_(nullptr),
      external_string_table_(this) {
  // Ensure old_generation_size_ is a multiple of kPageSize.
  DCHECK_EQ(0, max_old_generation_size_ & (Page::kPageSize - 1));

  set_native_contexts_list(Smi::zero());
  set_allocation_sites_list(Smi::zero());
  set_dirty_js_finalization_registries_list(Smi::zero());
  set_dirty_js_finalization_registries_list_tail(Smi::zero());
  // Put a dummy entry in the remembered pages so we can find the list the
  // minidump even if there are no real unmapped pages.
  RememberUnmappedPage(kNullAddress, false);
}

Heap::~Heap() = default;

size_t Heap::MaxReserved() {
  const size_t kMaxNewLargeObjectSpaceSize = max_semi_space_size_;
  return static_cast<size_t>(2 * max_semi_space_size_ +
                             kMaxNewLargeObjectSpaceSize +
                             max_old_generation_size_);
}

size_t Heap::YoungGenerationSizeFromOldGenerationSize(size_t old_generation) {
  // Compute the semi space size and cap it.
  size_t ratio = old_generation <= kOldGenerationLowMemory
                     ? kOldGenerationToSemiSpaceRatioLowMemory
                     : kOldGenerationToSemiSpaceRatio;
  size_t semi_space = old_generation / ratio;
  semi_space = Min<size_t>(semi_space, kMaxSemiSpaceSize);
  semi_space = Max<size_t>(semi_space, kMinSemiSpaceSize);
  semi_space = RoundUp(semi_space, Page::kPageSize);
  return YoungGenerationSizeFromSemiSpaceSize(semi_space);
}

size_t Heap::HeapSizeFromPhysicalMemory(uint64_t physical_memory) {
  // Compute the old generation size and cap it.
  uint64_t old_generation = physical_memory /
                            kPhysicalMemoryToOldGenerationRatio *
                            kHeapLimitMultiplier;
  old_generation =
      Min<uint64_t>(old_generation, MaxOldGenerationSize(physical_memory));
  old_generation = Max<uint64_t>(old_generation, V8HeapTrait::kMinSize);
  old_generation = RoundUp(old_generation, Page::kPageSize);

  size_t young_generation = YoungGenerationSizeFromOldGenerationSize(
      static_cast<size_t>(old_generation));
  return static_cast<size_t>(old_generation) + young_generation;
}

void Heap::GenerationSizesFromHeapSize(size_t heap_size,
                                       size_t* young_generation_size,
                                       size_t* old_generation_size) {
  // Initialize values for the case when the given heap size is too small.
  *young_generation_size = 0;
  *old_generation_size = 0;
  // Binary search for the largest old generation size that fits to the given
  // heap limit considering the correspondingly sized young generation.
  size_t lower = 0, upper = heap_size;
  while (lower + 1 < upper) {
    size_t old_generation = lower + (upper - lower) / 2;
    size_t young_generation =
        YoungGenerationSizeFromOldGenerationSize(old_generation);
    if (old_generation + young_generation <= heap_size) {
      // This size configuration fits into the given heap limit.
      *young_generation_size = young_generation;
      *old_generation_size = old_generation;
      lower = old_generation;
    } else {
      upper = old_generation;
    }
  }
}

size_t Heap::MinYoungGenerationSize() {
  return YoungGenerationSizeFromSemiSpaceSize(kMinSemiSpaceSize);
}

size_t Heap::MinOldGenerationSize() {
  size_t paged_space_count =
      LAST_GROWABLE_PAGED_SPACE - FIRST_GROWABLE_PAGED_SPACE + 1;
  return paged_space_count * Page::kPageSize;
}

size_t Heap::AllocatorLimitOnMaxOldGenerationSize() {
#ifdef V8_COMPRESS_POINTERS
  // Isolate and the young generation are also allocated on the heap.
  return kPtrComprHeapReservationSize -
         YoungGenerationSizeFromSemiSpaceSize(kMaxSemiSpaceSize) -
         RoundUp(sizeof(Isolate), size_t{1} << kPageSizeBits);
#endif
  return std::numeric_limits<size_t>::max();
}

size_t Heap::MaxOldGenerationSize(uint64_t physical_memory) {
  size_t max_size = V8HeapTrait::kMaxSize;
  // Finch experiment: Increase the heap size from 2GB to 4GB for 64-bit
  // systems with physical memory bigger than 16GB. The physical memory
  // is rounded up to GB.
  constexpr bool x64_bit = Heap::kHeapLimitMultiplier >= 2;
  if (FLAG_huge_max_old_generation_size && x64_bit &&
      (physical_memory + 512 * MB) / GB >= 16) {
    DCHECK_EQ(max_size / GB, 2);
    max_size *= 2;
  }
  return Min(max_size, AllocatorLimitOnMaxOldGenerationSize());
}

size_t Heap::YoungGenerationSizeFromSemiSpaceSize(size_t semi_space_size) {
  return semi_space_size * (2 + kNewLargeObjectSpaceToSemiSpaceRatio);
}

size_t Heap::SemiSpaceSizeFromYoungGenerationSize(
    size_t young_generation_size) {
  return young_generation_size / (2 + kNewLargeObjectSpaceToSemiSpaceRatio);
}

size_t Heap::Capacity() {
  if (!HasBeenSetUp()) return 0;

  return new_space_->Capacity() + OldGenerationCapacity();
}

size_t Heap::OldGenerationCapacity() {
  if (!HasBeenSetUp()) return 0;
  PagedSpaceIterator spaces(this);
  size_t total = 0;
  for (PagedSpace* space = spaces.Next(); space != nullptr;
       space = spaces.Next()) {
    total += space->Capacity();
  }
  return total + lo_space_->SizeOfObjects() + code_lo_space_->SizeOfObjects();
}

size_t Heap::CommittedOldGenerationMemory() {
  if (!HasBeenSetUp()) return 0;

  PagedSpaceIterator spaces(this);
  size_t total = 0;
  for (PagedSpace* space = spaces.Next(); space != nullptr;
       space = spaces.Next()) {
    total += space->CommittedMemory();
  }
  return total + lo_space_->Size() + code_lo_space_->Size();
}

size_t Heap::CommittedMemoryOfUnmapper() {
  if (!HasBeenSetUp()) return 0;

  return memory_allocator()->unmapper()->CommittedBufferedMemory();
}

size_t Heap::CommittedMemory() {
  if (!HasBeenSetUp()) return 0;

  return new_space_->CommittedMemory() + new_lo_space_->Size() +
         CommittedOldGenerationMemory();
}


size_t Heap::CommittedPhysicalMemory() {
  if (!HasBeenSetUp()) return 0;

  size_t total = 0;
  for (SpaceIterator it(this); it.HasNext();) {
    total += it.Next()->CommittedPhysicalMemory();
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

  for (SpaceIterator it(this); it.HasNext();) {
    total += it.Next()->Available();
  }

  total += memory_allocator()->Available();
  return total;
}

bool Heap::CanExpandOldGeneration(size_t size) {
  if (force_oom_) return false;
  if (OldGenerationCapacity() + size > max_old_generation_size_) return false;
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
  if (space != NEW_SPACE && space != NEW_LO_SPACE) {
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
  if (!CanExpandOldGeneration(new_space_->TotalCapacity() +
                              new_lo_space()->Size())) {
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
               "Memory allocator,       used: %6zu KB,"
               " available: %6zu KB\n",
               memory_allocator()->Size() / KB,
               memory_allocator()->Available() / KB);
  PrintIsolate(isolate_,
               "Read-only space,        used: %6zu KB"
               ", available: %6zu KB"
               ", committed: %6zu KB\n",
               read_only_space_->Size() / KB,
               read_only_space_->Available() / KB,
               read_only_space_->CommittedMemory() / KB);
  PrintIsolate(isolate_,
               "New space,              used: %6zu KB"
               ", available: %6zu KB"
               ", committed: %6zu KB\n",
               new_space_->Size() / KB, new_space_->Available() / KB,
               new_space_->CommittedMemory() / KB);
  PrintIsolate(isolate_,
               "New large object space, used: %6zu KB"
               ", available: %6zu KB"
               ", committed: %6zu KB\n",
               new_lo_space_->SizeOfObjects() / KB,
               new_lo_space_->Available() / KB,
               new_lo_space_->CommittedMemory() / KB);
  PrintIsolate(isolate_,
               "Old space,              used: %6zu KB"
               ", available: %6zu KB"
               ", committed: %6zu KB\n",
               old_space_->SizeOfObjects() / KB, old_space_->Available() / KB,
               old_space_->CommittedMemory() / KB);
  PrintIsolate(isolate_,
               "Code space,             used: %6zu KB"
               ", available: %6zu KB"
               ", committed: %6zu KB\n",
               code_space_->SizeOfObjects() / KB, code_space_->Available() / KB,
               code_space_->CommittedMemory() / KB);
  PrintIsolate(isolate_,
               "Map space,              used: %6zu KB"
               ", available: %6zu KB"
               ", committed: %6zu KB\n",
               map_space_->SizeOfObjects() / KB, map_space_->Available() / KB,
               map_space_->CommittedMemory() / KB);
  PrintIsolate(isolate_,
               "Large object space,     used: %6zu KB"
               ", available: %6zu KB"
               ", committed: %6zu KB\n",
               lo_space_->SizeOfObjects() / KB, lo_space_->Available() / KB,
               lo_space_->CommittedMemory() / KB);
  PrintIsolate(isolate_,
               "Code large object space,     used: %6zu KB"
               ", available: %6zu KB"
               ", committed: %6zu KB\n",
               code_lo_space_->SizeOfObjects() / KB,
               code_lo_space_->Available() / KB,
               code_lo_space_->CommittedMemory() / KB);
  ReadOnlySpace* const ro_space = read_only_space_;
  PrintIsolate(isolate_,
               "All spaces,             used: %6zu KB"
               ", available: %6zu KB"
               ", committed: %6zu KB\n",
               (this->SizeOfObjects() + ro_space->SizeOfObjects()) / KB,
               (this->Available() + ro_space->Available()) / KB,
               (this->CommittedMemory() + ro_space->CommittedMemory()) / KB);
  PrintIsolate(isolate_,
               "Unmapper buffering %zu chunks of committed: %6zu KB\n",
               memory_allocator()->unmapper()->NumberOfCommittedChunks(),
               CommittedMemoryOfUnmapper() / KB);
  PrintIsolate(isolate_, "External memory reported: %6" PRId64 " KB\n",
               isolate()->isolate_data()->external_memory_ / KB);
  PrintIsolate(isolate_, "Backing store memory: %6zu KB\n",
               backing_store_bytes_ / KB);
  PrintIsolate(isolate_, "External memory global %zu KB\n",
               external_memory_callback_() / KB);
  PrintIsolate(isolate_, "Total time spent in GC  : %.1f ms\n",
               total_gc_time_ms_);
}

void Heap::PrintFreeListsStats() {
  DCHECK(FLAG_trace_gc_freelists);

  if (FLAG_trace_gc_freelists_verbose) {
    PrintIsolate(isolate_,
                 "Freelists statistics per Page: "
                 "[category: length || total free bytes]\n");
  }

  std::vector<int> categories_lengths(
      old_space()->free_list()->number_of_categories(), 0);
  std::vector<size_t> categories_sums(
      old_space()->free_list()->number_of_categories(), 0);
  unsigned int pageCnt = 0;

  // This loops computes freelists lengths and sum.
  // If FLAG_trace_gc_freelists_verbose is enabled, it also prints
  // the stats of each FreeListCategory of each Page.
  for (Page* page : *old_space()) {
    std::ostringstream out_str;

    if (FLAG_trace_gc_freelists_verbose) {
      out_str << "Page " << std::setw(4) << pageCnt;
    }

    for (int cat = kFirstCategory;
         cat <= old_space()->free_list()->last_category(); cat++) {
      FreeListCategory* free_list =
          page->free_list_category(static_cast<FreeListCategoryType>(cat));
      int length = free_list->FreeListLength();
      size_t sum = free_list->SumFreeList();

      if (FLAG_trace_gc_freelists_verbose) {
        out_str << "[" << cat << ": " << std::setw(4) << length << " || "
                << std::setw(6) << sum << " ]"
                << (cat == old_space()->free_list()->last_category() ? "\n"
                                                                     : ", ");
      }
      categories_lengths[cat] += length;
      categories_sums[cat] += sum;
    }

    if (FLAG_trace_gc_freelists_verbose) {
      PrintIsolate(isolate_, "%s", out_str.str().c_str());
    }

    pageCnt++;
  }

  // Print statistics about old_space (pages, free/wasted/used memory...).
  PrintIsolate(
      isolate_,
      "%d pages. Free space: %.1f MB (waste: %.2f). "
      "Usage: %.1f/%.1f (MB) -> %.2f%%.\n",
      pageCnt, static_cast<double>(old_space_->Available()) / MB,
      static_cast<double>(old_space_->Waste()) / MB,
      static_cast<double>(old_space_->Size()) / MB,
      static_cast<double>(old_space_->Capacity()) / MB,
      static_cast<double>(old_space_->Size()) / old_space_->Capacity() * 100);

  // Print global statistics of each FreeListCategory (length & sum).
  PrintIsolate(isolate_,
               "FreeLists global statistics: "
               "[category: length || total free KB]\n");
  std::ostringstream out_str;
  for (int cat = kFirstCategory;
       cat <= old_space()->free_list()->last_category(); cat++) {
    out_str << "[" << cat << ": " << categories_lengths[cat] << " || "
            << std::fixed << std::setprecision(2)
            << static_cast<double>(categories_sums[cat]) / KB << " KB]"
            << (cat == old_space()->free_list()->last_category() ? "\n" : ", ");
  }
  PrintIsolate(isolate_, "%s", out_str.str().c_str());
}

void Heap::DumpJSONHeapStatistics(std::stringstream& stream) {
  HeapStatistics stats;
  reinterpret_cast<v8::Isolate*>(isolate())->GetHeapStatistics(&stats);

// clang-format off
#define DICT(s) "{" << s << "}"
#define LIST(s) "[" << s << "]"
#define ESCAPE(s) "\"" << s << "\""
#define MEMBER(s) ESCAPE(s) << ":"

  auto SpaceStatistics = [this](int space_index) {
    HeapSpaceStatistics space_stats;
    reinterpret_cast<v8::Isolate*>(isolate())->GetHeapSpaceStatistics(
        &space_stats, space_index);
    std::stringstream stream;
    stream << DICT(
      MEMBER("name")
        << ESCAPE(GetSpaceName(static_cast<AllocationSpace>(space_index)))
        << ","
      MEMBER("size") << space_stats.space_size() << ","
      MEMBER("used_size") << space_stats.space_used_size() << ","
      MEMBER("available_size") << space_stats.space_available_size() << ","
      MEMBER("physical_size") << space_stats.physical_space_size());
    return stream.str();
  };

  stream << DICT(
    MEMBER("isolate") << ESCAPE(reinterpret_cast<void*>(isolate())) << ","
    MEMBER("id") << gc_count() << ","
    MEMBER("time_ms") << isolate()->time_millis_since_init() << ","
    MEMBER("total_heap_size") << stats.total_heap_size() << ","
    MEMBER("total_heap_size_executable")
      << stats.total_heap_size_executable() << ","
    MEMBER("total_physical_size") << stats.total_physical_size() << ","
    MEMBER("total_available_size") << stats.total_available_size() << ","
    MEMBER("used_heap_size") << stats.used_heap_size() << ","
    MEMBER("heap_size_limit") << stats.heap_size_limit() << ","
    MEMBER("malloced_memory") << stats.malloced_memory() << ","
    MEMBER("external_memory") << stats.external_memory() << ","
    MEMBER("peak_malloced_memory") << stats.peak_malloced_memory() << ","
    MEMBER("spaces") << LIST(
      SpaceStatistics(RO_SPACE)      << "," <<
      SpaceStatistics(NEW_SPACE)     << "," <<
      SpaceStatistics(OLD_SPACE)     << "," <<
      SpaceStatistics(CODE_SPACE)    << "," <<
      SpaceStatistics(MAP_SPACE)     << "," <<
      SpaceStatistics(LO_SPACE)      << "," <<
      SpaceStatistics(CODE_LO_SPACE) << "," <<
      SpaceStatistics(NEW_LO_SPACE)));

#undef DICT
#undef LIST
#undef ESCAPE
#undef MEMBER
  // clang-format on
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
  if (allocation_trackers_.empty() && FLAG_inline_new) {
    DisableInlineAllocation();
  }
  allocation_trackers_.push_back(tracker);
}

void Heap::RemoveHeapObjectAllocationTracker(
    HeapObjectAllocationTracker* tracker) {
  allocation_trackers_.erase(std::remove(allocation_trackers_.begin(),
                                         allocation_trackers_.end(), tracker),
                             allocation_trackers_.end());
  if (allocation_trackers_.empty() && FLAG_inline_new) {
    EnableInlineAllocation();
  }
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

bool Heap::IsRetainingPathTarget(HeapObject object,
                                 RetainingPathOption* option) {
  WeakArrayList targets = retaining_path_targets();
  int length = targets.length();
  MaybeObject object_to_check = HeapObjectReference::Weak(object);
  for (int i = 0; i < length; i++) {
    MaybeObject target = targets.Get(i);
    DCHECK(target->IsWeakOrCleared());
    if (target == object_to_check) {
      DCHECK(retaining_path_target_option_.count(i));
      *option = retaining_path_target_option_[i];
      return true;
    }
  }
  return false;
}

void Heap::PrintRetainingPath(HeapObject target, RetainingPathOption option) {
  PrintF("\n\n\n");
  PrintF("#################################################\n");
  PrintF("Retaining path for %p:\n", reinterpret_cast<void*>(target.ptr()));
  HeapObject object = target;
  std::vector<std::pair<HeapObject, bool>> retaining_path;
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
    HeapObject object = node.first;
    bool ephemeron = node.second;
    PrintF("\n");
    PrintF("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");
    PrintF("Distance from root %d%s: ", distance,
           ephemeron ? " (ephemeron)" : "");
    object.ShortPrint();
    PrintF("\n");
#ifdef OBJECT_PRINT
    object.Print();
    PrintF("\n");
#endif
    --distance;
  }
  PrintF("\n");
  PrintF("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");
  PrintF("Root: %s\n", RootVisitor::RootName(root));
  PrintF("-------------------------------------------------\n");
}

void Heap::AddRetainer(HeapObject retainer, HeapObject object) {
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

void Heap::AddEphemeronRetainer(HeapObject retainer, HeapObject object) {
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

void Heap::AddRetainingRoot(Root root, HeapObject object) {
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
  memory_allocator()->unmapper()->PrepareForGC();
}

size_t Heap::SizeOfObjects() {
  size_t total = 0;

  for (SpaceIterator it(this); it.HasNext();) {
    total += it.Next()->SizeOfObjects();
  }
  return total;
}

size_t Heap::TotalGlobalHandlesSize() {
  return isolate_->global_handles()->TotalSize();
}

size_t Heap::UsedGlobalHandlesSize() {
  return isolate_->global_handles()->UsedSize();
}

// static
const char* Heap::GetSpaceName(AllocationSpace space) {
  switch (space) {
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
    case CODE_LO_SPACE:
      return "code_large_object_space";
    case RO_SPACE:
      return "read_only_space";
  }
  UNREACHABLE();
}

void Heap::MergeAllocationSitePretenuringFeedback(
    const PretenuringFeedbackMap& local_pretenuring_feedback) {
  AllocationSite site;
  for (auto& site_and_count : local_pretenuring_feedback) {
    site = site_and_count.first;
    MapWord map_word = site_and_count.first.map_word();
    if (map_word.IsForwardingAddress()) {
      site = AllocationSite::cast(map_word.ToForwardingAddress());
    }

    // We have not validated the allocation site yet, since we have not
    // dereferenced the site during collecting information.
    // This is an inlined check of AllocationMemento::IsValid.
    if (!site.IsAllocationSite() || site.IsZombie()) continue;

    const int value = static_cast<int>(site_and_count.second);
    DCHECK_LT(0, value);
    if (site.IncrementMementoFoundCount(value)) {
      // For sites in the global map the count is accessed through the site.
      global_pretenuring_feedback_.insert(std::make_pair(site, 0));
    }
  }
}

void Heap::AddAllocationObserversToAllSpaces(
    AllocationObserver* observer, AllocationObserver* new_space_observer) {
  DCHECK(observer && new_space_observer);

  for (SpaceIterator it(this); it.HasNext();) {
    Space* space = it.Next();
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

  for (SpaceIterator it(this); it.HasNext();) {
    Space* space = it.Next();
    if (space == new_space()) {
      space->RemoveAllocationObserver(new_space_observer);
    } else {
      space->RemoveAllocationObserver(observer);
    }
  }
}

namespace {
inline bool MakePretenureDecision(
    AllocationSite site, AllocationSite::PretenureDecision current_decision,
    double ratio, bool maximum_size_scavenge) {
  // Here we just allow state transitions from undecided or maybe tenure
  // to don't tenure, maybe tenure, or tenure.
  if ((current_decision == AllocationSite::kUndecided ||
       current_decision == AllocationSite::kMaybeTenure)) {
    if (ratio >= AllocationSite::kPretenureRatio) {
      // We just transition into tenure state when the semi-space was at
      // maximum capacity.
      if (maximum_size_scavenge) {
        site.set_deopt_dependent_code(true);
        site.set_pretenure_decision(AllocationSite::kTenure);
        // Currently we just need to deopt when we make a state transition to
        // tenure.
        return true;
      }
      site.set_pretenure_decision(AllocationSite::kMaybeTenure);
    } else {
      site.set_pretenure_decision(AllocationSite::kDontTenure);
    }
  }
  return false;
}

inline bool DigestPretenuringFeedback(Isolate* isolate, AllocationSite site,
                                      bool maximum_size_scavenge) {
  bool deopt = false;
  int create_count = site.memento_create_count();
  int found_count = site.memento_found_count();
  bool minimum_mementos_created =
      create_count >= AllocationSite::kPretenureMinimumCreated;
  double ratio = minimum_mementos_created || FLAG_trace_pretenuring_statistics
                     ? static_cast<double>(found_count) / create_count
                     : 0.0;
  AllocationSite::PretenureDecision current_decision =
      site.pretenure_decision();

  if (minimum_mementos_created) {
    deopt = MakePretenureDecision(site, current_decision, ratio,
                                  maximum_size_scavenge);
  }

  if (FLAG_trace_pretenuring_statistics) {
    PrintIsolate(isolate,
                 "pretenuring: AllocationSite(%p): (created, found, ratio) "
                 "(%d, %d, %f) %s => %s\n",
                 reinterpret_cast<void*>(site.ptr()), create_count, found_count,
                 ratio, site.PretenureDecisionName(current_decision),
                 site.PretenureDecisionName(site.pretenure_decision()));
  }

  // Clear feedback calculation fields until the next gc.
  site.set_memento_found_count(0);
  site.set_memento_create_count(0);
  return deopt;
}
}  // namespace

void Heap::RemoveAllocationSitePretenuringFeedback(AllocationSite site) {
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

    AllocationSite site;

    // Step 1: Digest feedback for recorded allocation sites.
    bool maximum_size_scavenge = MaximumSizeScavenge();
    for (auto& site_and_count : global_pretenuring_feedback_) {
      allocation_sites++;
      site = site_and_count.first;
      // Count is always access through the site.
      DCHECK_EQ(0, site_and_count.second);
      int found_count = site.memento_found_count();
      // An entry in the storage does not imply that the count is > 0 because
      // allocation sites might have been reset due to too many objects dying
      // in old space.
      if (found_count > 0) {
        DCHECK(site.IsAllocationSite());
        active_allocation_sites++;
        allocation_mementos_found += found_count;
        if (DigestPretenuringFeedback(isolate_, site, maximum_size_scavenge)) {
          trigger_deoptimization = true;
        }
        if (site.GetAllocationType() == AllocationType::kOld) {
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
          [&allocation_sites, &trigger_deoptimization](AllocationSite site) {
            DCHECK(site.IsAllocationSite());
            allocation_sites++;
            if (site.IsMaybeTenure()) {
              site.set_deopt_dependent_code(true);
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

void Heap::InvalidateCodeDeoptimizationData(Code code) {
  CodePageMemoryModificationScope modification_scope(code);
  code.set_deoptimization_data(ReadOnlyRoots(this).empty_fixed_array());
}

void Heap::DeoptMarkedAllocationSites() {
  // TODO(hpayer): If iterating over the allocation sites list becomes a
  // performance issue, use a cache data structure in heap instead.

  ForeachAllocationSite(allocation_sites_list(), [](AllocationSite site) {
    if (site.deopt_dependent_code()) {
      site.dependent_code().MarkCodeForDeoptimization(
          DependentCode::kAllocationSiteTenuringChangedGroup);
      site.set_deopt_dependent_code(false);
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
  // Old-to-new slot sets must be empty after each collection.
  for (SpaceIterator it(this); it.HasNext();) {
    Space* space = it.Next();

    for (MemoryChunk* chunk = space->first_page(); chunk != space->last_page();
         chunk = chunk->list_node().next())
      DCHECK_NULL(chunk->invalidated_slots<OLD_TO_NEW>());
  }

  if (FLAG_print_global_handles) isolate_->global_handles()->Print();
  if (FLAG_print_handles) PrintHandles();
  if (FLAG_gc_verbose) Print();
  if (FLAG_code_stats) ReportCodeStatistics("After GC");
  if (FLAG_check_handle_count) CheckHandleCount();
#endif

  UpdateMaximumCommitted();

  isolate_->counters()->alive_after_last_gc()->Set(
      static_cast<int>(SizeOfObjects()));

  isolate_->counters()->string_table_capacity()->Set(string_table().Capacity());
  isolate_->counters()->number_of_symbols()->Set(
      string_table().NumberOfElements());

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

  if (FLAG_harmony_weak_refs &&
      isolate()->host_cleanup_finalization_group_callback()) {
    HandleScope handle_scope(isolate());
    Handle<JSFinalizationRegistry> finalization_registry;
    while (
        DequeueDirtyJSFinalizationRegistry().ToHandle(&finalization_registry)) {
      isolate()->RunHostCleanupFinalizationGroupCallback(finalization_registry);
    }
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

void Heap::ScheduleScavengeTaskIfNeeded() {
  DCHECK_NOT_NULL(scavenge_job_);
  scavenge_job_->ScheduleTaskIfNeeded(this);
}

TimedHistogram* Heap::GCTypePriorityTimer(GarbageCollector collector) {
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

TimedHistogram* Heap::GCTypeTimer(GarbageCollector collector) {
  if (IsYoungGenerationCollector(collector)) {
    return isolate_->counters()->gc_scavenger();
  }
  if (incremental_marking()->IsStopped()) {
    return isolate_->counters()->gc_compactor();
  }
  if (ShouldReduceMemory()) {
    return isolate_->counters()->gc_finalize_reduce_memory();
  }
  if (incremental_marking()->IsMarking() &&
      incremental_marking()->marking_worklists()->IsPerContextMode()) {
    return isolate_->counters()->gc_finalize_measure_memory();
  }
  return isolate_->counters()->gc_finalize();
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

intptr_t CompareWords(int size, HeapObject a, HeapObject b) {
  int slots = size / kTaggedSize;
  DCHECK_EQ(a.Size(), size);
  DCHECK_EQ(b.Size(), size);
  Tagged_t* slot_a = reinterpret_cast<Tagged_t*>(a.address());
  Tagged_t* slot_b = reinterpret_cast<Tagged_t*>(b.address());
  for (int i = 0; i < slots; i++) {
    if (*slot_a != *slot_b) {
      return *slot_a - *slot_b;
    }
    slot_a++;
    slot_b++;
  }
  return 0;
}

void ReportDuplicates(int size, std::vector<HeapObject>* objects) {
  if (objects->size() == 0) return;

  sort(objects->begin(), objects->end(), [size](HeapObject a, HeapObject b) {
    intptr_t c = CompareWords(size, a, b);
    if (c != 0) return c < 0;
    return a < b;
  });

  std::vector<std::pair<int, HeapObject>> duplicates;
  HeapObject current = (*objects)[0];
  int count = 1;
  for (size_t i = 1; i < objects->size(); i++) {
    if (CompareWords(size, current, (*objects)[i]) == 0) {
      count++;
    } else {
      if (count > 1) {
        duplicates.push_back(std::make_pair(count - 1, current));
      }
      count = 1;
      current = (*objects)[i];
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
    it->second.Print();
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
  new_lo_space_->SetCapacity(new_space_->Capacity() *
                             kNewLargeObjectSpaceToSemiSpaceRatio);
  UncommitFromSpace();
  EagerlyFreeExternalMemory();

  if (FLAG_trace_duplicate_threshold_kb) {
    std::map<int, std::vector<HeapObject>> objects_by_size;
    PagedSpaceIterator spaces(this);
    for (PagedSpace* space = spaces.Next(); space != nullptr;
         space = spaces.Next()) {
      PagedSpaceObjectIterator it(this, space);
      for (HeapObject obj = it.Next(); !obj.is_null(); obj = it.Next()) {
        objects_by_size[obj.Size()].push_back(obj);
      }
    }
    {
      LargeObjectSpaceObjectIterator it(lo_space());
      for (HeapObject obj = it.Next(); !obj.is_null(); obj = it.Next()) {
        objects_by_size[obj.Size()].push_back(obj);
      }
    }
    for (auto it = objects_by_size.rbegin(); it != objects_by_size.rend();
         ++it) {
      ReportDuplicates(it->first, &it->second);
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
  if (isolate()->isolate_data()->external_memory_ >
      (isolate()->isolate_data()->external_memory_low_since_mark_compact_ +
       external_memory_hard_limit())) {
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
    const double ms_step = Min(
        kMaxStepSize,
        Max(kMinStepSize,
            static_cast<double>(isolate()->isolate_data()->external_memory_) /
                isolate()->isolate_data()->external_memory_limit_ *
                kMinStepSize));
    const double deadline = MonotonicallyIncreasingTimeInMs() + ms_step;
    // Extend the gc callback flags with external memory flags.
    current_gc_callback_flags_ = static_cast<GCCallbackFlags>(
        current_gc_callback_flags_ | kGCCallbackFlagsForExternalMemory);
    incremental_marking()->AdvanceWithDeadline(
        deadline, IncrementalMarking::GC_VIA_STACK_GUARD, StepOrigin::kV8);
  }
}

void Heap::EnsureFillerObjectAtTop() {
  // There may be an allocation memento behind objects in new space. Upon
  // evacuation of a non-full new space (or if we are on the last page) there
  // may be uninitialized memory behind top. We fill the remainder of the page
  // with a filler.
  Address to_top = new_space_->top();
  Page* page = Page::FromAddress(to_top - kTaggedSize);
  if (page->Contains(to_top)) {
    int remaining_in_page = static_cast<int>(page->area_end() - to_top);
    CreateFillerObjectAt(to_top, remaining_in_page, ClearRecordedSlots::kNo);
  }
}

Heap::DevToolsTraceEventScope::DevToolsTraceEventScope(Heap* heap,
                                                       const char* event_name,
                                                       const char* event_type)
    : heap_(heap), event_name_(event_name) {
  TRACE_EVENT_BEGIN2("devtools.timeline,v8", event_name_, "usedHeapSizeBefore",
                     heap_->SizeOfObjects(), "type", event_type);
}

Heap::DevToolsTraceEventScope::~DevToolsTraceEventScope() {
  TRACE_EVENT_END1("devtools.timeline,v8", event_name_, "usedHeapSizeAfter",
                   heap_->SizeOfObjects());
}

bool Heap::CollectGarbage(AllocationSpace space,
                          GarbageCollectionReason gc_reason,
                          const v8::GCCallbackFlags gc_callback_flags) {
  const char* collector_reason = nullptr;
  GarbageCollector collector = SelectGarbageCollector(space, &collector_reason);
  is_current_gc_forced_ = gc_callback_flags & v8::kGCCallbackFlagForced;

  DevToolsTraceEventScope devtools_trace_event_scope(
      this, IsYoungGenerationCollector(collector) ? "MinorGC" : "MajorGC",
      GarbageCollectionReasonToString(gc_reason));

  if (!CanExpandOldGeneration(new_space()->Capacity() +
                              new_lo_space()->Size())) {
    InvokeNearHeapLimitCallback();
  }

  // Filter on-stack reference below this method.
  isolate()
      ->global_handles()
      ->CleanupOnStackReferencesBelowCurrentStackPosition();

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
      TimedHistogram* gc_type_timer = GCTypeTimer(collector);
      TimedHistogramScope histogram_timer_scope(gc_type_timer, isolate_);
      TRACE_EVENT0("v8", gc_type_timer->name());

      TimedHistogram* gc_type_priority_timer = GCTypePriorityTimer(collector);
      OptionalTimedHistogramScopeMode mode =
          isolate_->IsMemorySavingsModeActive()
              ? OptionalTimedHistogramScopeMode::DONT_TAKE_TIME
              : OptionalTimedHistogramScopeMode::TAKE_TIME;
      OptionalTimedHistogramScope histogram_timer_priority_scope(
          gc_type_priority_timer, isolate_, mode);

      next_gc_likely_to_collect_more =
          PerformGarbageCollection(collector, gc_callback_flags);
      if (collector == MARK_COMPACTOR || collector == SCAVENGER) {
        tracer()->RecordGCPhasesHistograms(gc_type_timer);
      }
    }

    // Clear is_current_gc_forced now that the current GC is complete. Do this
    // before GarbageCollectionEpilogue() since that could trigger another
    // unforced GC.
    is_current_gc_forced_ = false;

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
          (detached_contexts().length() > 0);
      event.committed_memory = committed_memory_after;
      if (deserialization_complete_) {
        memory_reducer_->NotifyMarkCompact(event);
      }
      if (initial_max_old_generation_size_ < max_old_generation_size_ &&
          used_memory_after < initial_max_old_generation_size_threshold_) {
        max_old_generation_size_ = initial_max_old_generation_size_;
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
    old_generation_allocation_limit_ = initial_old_generation_size_;
    MemoryReducer::Event event;
    event.type = MemoryReducer::kPossibleGarbage;
    event.time_ms = MonotonicallyIncreasingTimeInMs();
    memory_reducer_->NotifyPossibleGarbage(event);
  }
  isolate()->AbortConcurrentOptimization(BlockingBehavior::kDontBlock);
  if (!isolate()->context().is_null()) {
    RemoveDirtyFinalizationRegistriesOnContext(isolate()->raw_native_context());
  }

  number_of_disposed_maps_ = retained_maps().length();
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
      StartIncrementalMarking(
          gc_flags,
          OldGenerationSpaceAvailable() <= new_space_->Capacity()
              ? GarbageCollectionReason::kAllocationLimit
              : GarbageCollectionReason::kGlobalAllocationLimit,
          gc_callback_flags);
    }
  }
}

void Heap::StartIdleIncrementalMarking(
    GarbageCollectionReason gc_reason,
    const GCCallbackFlags gc_callback_flags) {
  StartIncrementalMarking(kReduceMemoryFootprintMask, gc_reason,
                          gc_callback_flags);
}

void Heap::MoveRange(HeapObject dst_object, const ObjectSlot dst_slot,
                     const ObjectSlot src_slot, int len,
                     WriteBarrierMode mode) {
  DCHECK_NE(len, 0);
  DCHECK_NE(dst_object.map(), ReadOnlyRoots(this).fixed_cow_array_map());
  const ObjectSlot dst_end(dst_slot + len);
  // Ensure no range overflow.
  DCHECK(dst_slot < dst_end);
  DCHECK(src_slot < src_slot + len);

  if (FLAG_concurrent_marking && incremental_marking()->IsMarking()) {
    if (dst_slot < src_slot) {
      // Copy tagged values forward using relaxed load/stores that do not
      // involve value decompression.
      const AtomicSlot atomic_dst_end(dst_end);
      AtomicSlot dst(dst_slot);
      AtomicSlot src(src_slot);
      while (dst < atomic_dst_end) {
        *dst = *src;
        ++dst;
        ++src;
      }
    } else {
      // Copy tagged values backwards using relaxed load/stores that do not
      // involve value decompression.
      const AtomicSlot atomic_dst_begin(dst_slot);
      AtomicSlot dst(dst_slot + len - 1);
      AtomicSlot src(src_slot + len - 1);
      while (dst >= atomic_dst_begin) {
        *dst = *src;
        --dst;
        --src;
      }
    }
  } else {
    MemMove(dst_slot.ToVoidPtr(), src_slot.ToVoidPtr(), len * kTaggedSize);
  }
  if (mode == SKIP_WRITE_BARRIER) return;
  WriteBarrierForRange(dst_object, dst_slot, dst_end);
}

// Instantiate Heap::CopyRange() for ObjectSlot and MaybeObjectSlot.
template void Heap::CopyRange<ObjectSlot>(HeapObject dst_object,
                                          ObjectSlot dst_slot,
                                          ObjectSlot src_slot, int len,
                                          WriteBarrierMode mode);
template void Heap::CopyRange<MaybeObjectSlot>(HeapObject dst_object,
                                               MaybeObjectSlot dst_slot,
                                               MaybeObjectSlot src_slot,
                                               int len, WriteBarrierMode mode);

template <typename TSlot>
void Heap::CopyRange(HeapObject dst_object, const TSlot dst_slot,
                     const TSlot src_slot, int len, WriteBarrierMode mode) {
  DCHECK_NE(len, 0);

  DCHECK_NE(dst_object.map(), ReadOnlyRoots(this).fixed_cow_array_map());
  const TSlot dst_end(dst_slot + len);
  // Ensure ranges do not overlap.
  DCHECK(dst_end <= src_slot || (src_slot + len) <= dst_slot);

  if (FLAG_concurrent_marking && incremental_marking()->IsMarking()) {
    // Copy tagged values using relaxed load/stores that do not involve value
    // decompression.
    const AtomicSlot atomic_dst_end(dst_end);
    AtomicSlot dst(dst_slot);
    AtomicSlot src(src_slot);
    while (dst < atomic_dst_end) {
      *dst = *src;
      ++dst;
      ++src;
    }
  } else {
    MemCopy(dst_slot.ToVoidPtr(), src_slot.ToVoidPtr(), len * kTaggedSize);
  }
  if (mode == SKIP_WRITE_BARRIER) return;
  WriteBarrierForRange(dst_object, dst_slot, dst_end);
}

#ifdef VERIFY_HEAP
// Helper class for verifying the string table.
class StringTableVerifier : public ObjectVisitor {
 public:
  explicit StringTableVerifier(Isolate* isolate) : isolate_(isolate) {}

  void VisitPointers(HeapObject host, ObjectSlot start,
                     ObjectSlot end) override {
    // Visit all HeapObject pointers in [start, end).
    for (ObjectSlot p = start; p < end; ++p) {
      DCHECK(!HasWeakHeapObjectTag(*p));
      if ((*p).IsHeapObject()) {
        HeapObject object = HeapObject::cast(*p);
        // Check that the string is actually internalized.
        CHECK(object.IsTheHole(isolate_) || object.IsUndefined(isolate_) ||
              object.IsInternalizedString());
      }
    }
  }
  void VisitPointers(HeapObject host, MaybeObjectSlot start,
                     MaybeObjectSlot end) override {
    UNREACHABLE();
  }

  void VisitCodeTarget(Code host, RelocInfo* rinfo) override { UNREACHABLE(); }

  void VisitEmbeddedPointer(Code host, RelocInfo* rinfo) override {
    UNREACHABLE();
  }

 private:
  Isolate* isolate_;
};

static void VerifyStringTable(Isolate* isolate) {
  StringTableVerifier verifier(isolate);
  isolate->heap()->string_table().IterateElements(&verifier);
}
#endif  // VERIFY_HEAP

bool Heap::ReserveSpace(Reservation* reservations, std::vector<Address>* maps) {
  bool gc_performed = true;
  int counter = 0;
  static const int kThreshold = 20;
  while (gc_performed && counter++ < kThreshold) {
    gc_performed = false;
    for (int space = FIRST_SPACE;
         space < static_cast<int>(SnapshotSpace::kNumberOfHeapSpaces);
         space++) {
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
          AllocationResult allocation;
#if V8_ENABLE_THIRD_PARTY_HEAP_BOOL
          allocation = AllocateRaw(Map::kSize, AllocationType::kMap,
                                   AllocationOrigin::kRuntime, kWordAligned);
#else
          allocation = map_space()->AllocateRawUnaligned(Map::kSize);
#endif
          HeapObject free_space;
          if (allocation.To(&free_space)) {
            // Mark with a free list node, in case we have a GC before
            // deserializing.
            Address free_space_address = free_space.address();
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
                    MemoryChunkLayout::AllocatableMemoryInMemoryChunk(
                        static_cast<AllocationSpace>(space)));
#if V8_ENABLE_THIRD_PARTY_HEAP_BOOL
          AllocationType type = (space == CODE_SPACE)
                                    ? AllocationType::kCode
                                    : (space == RO_SPACE)
                                          ? AllocationType::kReadOnly
                                          : AllocationType::kYoung;
          AllocationAlignment align =
              (space == CODE_SPACE) ? kCodeAligned : kWordAligned;
          allocation =
              AllocateRaw(size, type, AllocationOrigin::kRuntime, align);
#else
          if (space == NEW_SPACE) {
            allocation = new_space()->AllocateRawUnaligned(size);
          } else {
            // The deserializer will update the skip list.
            allocation = paged_space(space)->AllocateRawUnaligned(size);
          }
#endif
          HeapObject free_space;
          if (allocation.To(&free_space)) {
            // Mark with a free list node, in case we have a GC before
            // deserializing.
            Address free_space_address = free_space.address();
            CreateFillerObjectAt(free_space_address, size,
                                 ClearRecordedSlots::kNo);
            DCHECK(IsPreAllocatedSpace(static_cast<SnapshotSpace>(space)));
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
  DisallowJavascriptExecution no_js(isolate());

  if (V8_ENABLE_THIRD_PARTY_HEAP_BOOL) {
    return tp_heap_->CollectGarbage();
  }

  size_t freed_global_handles = 0;

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
    // Temporary override any embedder stack state as callbacks may create their
    // own state on the stack and recursively trigger GC.
    EmbedderStackStateScope embedder_scope(
        local_embedder_heap_tracer(),
        EmbedderHeapTracer::EmbedderStackState::kUnknown);
    if (scope.CheckReenter()) {
      AllowHeapAllocation allow_allocation;
      AllowJavascriptExecution allow_js(isolate());
      TRACE_GC(tracer(), GCTracer::Scope::HEAP_EXTERNAL_PROLOGUE);
      VMState<EXTERNAL> state(isolate_);
      HandleScope handle_scope(isolate_);
      CallGCPrologueCallbacks(gc_type, kNoGCCallbackFlags);
    }
  }

  EnsureFromSpaceIsCommitted();

  size_t start_young_generation_size =
      Heap::new_space()->Size() + new_lo_space()->SizeOfObjects();

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
           CanExpandOldGeneration(new_space()->Size() +
                                  new_lo_space()->Size()))) {
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

  UpdateSurvivalStatistics(static_cast<int>(start_young_generation_size));
  ConfigureInitialOldGenerationSize();

  if (collector != MARK_COMPACTOR) {
    // Objects that died in the new space might have been accounted
    // as bytes marked ahead of schedule by the incremental marker.
    incremental_marking()->UpdateMarkedBytesAfterScavenge(
        start_young_generation_size - SurvivedYoungObjectSize());
  }

  if (!fast_promotion_mode_ || collector == MARK_COMPACTOR) {
    ComputeFastPromotionMode();
  }

  isolate_->counters()->objs_since_last_young()->Set(0);

  {
    TRACE_GC(tracer(), GCTracer::Scope::HEAP_EXTERNAL_WEAK_GLOBAL_HANDLES);
    // First round weak callbacks are not supposed to allocate and trigger
    // nested GCs.
    freed_global_handles =
        isolate_->global_handles()->InvokeFirstPassWeakCallbacks();
  }

  if (collector == MARK_COMPACTOR) {
    TRACE_GC(tracer(), GCTracer::Scope::HEAP_EMBEDDER_TRACING_EPILOGUE);
    // TraceEpilogue may trigger operations that invalidate global handles. It
    // has to be called *after* all other operations that potentially touch and
    // reset global handles. It is also still part of the main garbage
    // collection pause and thus needs to be called *before* any operation that
    // can potentially trigger recursive garbage
    local_embedder_heap_tracer()->TraceEpilogue();
  }

  {
    TRACE_GC(tracer(), GCTracer::Scope::HEAP_EXTERNAL_WEAK_GLOBAL_HANDLES);
    gc_post_processing_depth_++;
    {
      AllowHeapAllocation allow_allocation;
      AllowJavascriptExecution allow_js(isolate());
      freed_global_handles +=
          isolate_->global_handles()->PostGarbageCollectionProcessing(
              collector, gc_callback_flags);
    }
    gc_post_processing_depth_--;
  }

  isolate_->eternal_handles()->PostGarbageCollectionProcessing();

  // Update relocatables.
  Relocatable::PostGarbageCollectionProcessing(isolate_);

  RecomputeLimits(collector);

  {
    GCCallbacksScope scope(this);
    if (scope.CheckReenter()) {
      AllowHeapAllocation allow_allocation;
      AllowJavascriptExecution allow_js(isolate());
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

void Heap::RecomputeLimits(GarbageCollector collector) {
  if (!((collector == MARK_COMPACTOR) ||
        (HasLowYoungGenerationAllocationRate() &&
         old_generation_size_configured_))) {
    return;
  }

  double v8_gc_speed =
      tracer()->CombinedMarkCompactSpeedInBytesPerMillisecond();
  double v8_mutator_speed =
      tracer()->CurrentOldGenerationAllocationThroughputInBytesPerMillisecond();
  double v8_growing_factor = MemoryController<V8HeapTrait>::GrowingFactor(
      this, max_old_generation_size_, v8_gc_speed, v8_mutator_speed);
  double global_growing_factor = 0;
  if (UseGlobalMemoryScheduling()) {
    DCHECK_NOT_NULL(local_embedder_heap_tracer());
    double embedder_gc_speed = tracer()->EmbedderSpeedInBytesPerMillisecond();
    double embedder_speed =
        tracer()->CurrentEmbedderAllocationThroughputInBytesPerMillisecond();
    double embedder_growing_factor =
        (embedder_gc_speed > 0 && embedder_speed > 0)
            ? MemoryController<GlobalMemoryTrait>::GrowingFactor(
                  this, max_global_memory_size_, embedder_gc_speed,
                  embedder_speed)
            : 0;
    global_growing_factor = Max(v8_growing_factor, embedder_growing_factor);
  }

  size_t old_gen_size = OldGenerationSizeOfObjects();
  size_t new_space_capacity = new_space()->Capacity();
  HeapGrowingMode mode = CurrentHeapGrowingMode();

  if (collector == MARK_COMPACTOR) {
    // Register the amount of external allocated memory.
    isolate()->isolate_data()->external_memory_low_since_mark_compact_ =
        isolate()->isolate_data()->external_memory_;
    isolate()->isolate_data()->external_memory_limit_ =
        isolate()->isolate_data()->external_memory_ +
        kExternalAllocationSoftLimit;

    old_generation_allocation_limit_ =
        MemoryController<V8HeapTrait>::CalculateAllocationLimit(
            this, old_gen_size, min_old_generation_size_,
            max_old_generation_size_, new_space_capacity, v8_growing_factor,
            mode);
    if (UseGlobalMemoryScheduling()) {
      DCHECK_GT(global_growing_factor, 0);
      global_allocation_limit_ =
          MemoryController<GlobalMemoryTrait>::CalculateAllocationLimit(
              this, GlobalSizeOfObjects(), min_global_memory_size_,
              max_global_memory_size_, new_space_capacity,
              global_growing_factor, mode);
    }
    CheckIneffectiveMarkCompact(
        old_gen_size, tracer()->AverageMarkCompactMutatorUtilization());
  } else if (HasLowYoungGenerationAllocationRate() &&
             old_generation_size_configured_) {
    size_t new_old_generation_limit =
        MemoryController<V8HeapTrait>::CalculateAllocationLimit(
            this, old_gen_size, min_old_generation_size_,
            max_old_generation_size_, new_space_capacity, v8_growing_factor,
            mode);
    if (new_old_generation_limit < old_generation_allocation_limit_) {
      old_generation_allocation_limit_ = new_old_generation_limit;
    }
    if (UseGlobalMemoryScheduling()) {
      DCHECK_GT(global_growing_factor, 0);
      size_t new_global_limit =
          MemoryController<GlobalMemoryTrait>::CalculateAllocationLimit(
              this, GlobalSizeOfObjects(), min_global_memory_size_,
              max_global_memory_size_, new_space_capacity,
              global_growing_factor, mode);
      if (new_global_limit < global_allocation_limit_) {
        global_allocation_limit_ = new_global_limit;
      }
    }
  }
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
  AlwaysAllocateScope always_allocate(this);
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
  new_lo_space()->SetCapacity(new_space()->Capacity());
}

void Heap::EvacuateYoungGeneration() {
  TRACE_GC(tracer(), GCTracer::Scope::SCAVENGER_FAST_PROMOTE);
  base::MutexGuard guard(relocation_mutex());
  ConcurrentMarking::PauseScope pause_scope(concurrent_marking());
  if (!FLAG_concurrent_marking) {
    DCHECK(fast_promotion_mode_);
    DCHECK(
        CanExpandOldGeneration(new_space()->Size() + new_lo_space()->Size()));
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

  for (auto it = new_lo_space()->begin(); it != new_lo_space()->end();) {
    LargePage* page = *it;
    // Increment has to happen after we save the page, because it is going to
    // be removed below.
    it++;
    lo_space()->PromoteNewLargeObject(page);
  }

  // Fix up special trackers.
  external_string_table_.PromoteYoung();
  // GlobalHandles are updated in PostGarbageCollectonProcessing

  size_t promoted = new_space()->Size() + new_lo_space()->Size();
  IncrementYoungSurvivorsCounter(promoted);
  IncrementPromotedObjectsSize(promoted);
  IncrementSemiSpaceCopiedObjectSize(0);

  LOG(isolate_, ResourceEvent("scavenge", "end"));
  SetGCState(NOT_IN_GC);
}

void Heap::Scavenge() {
  TRACE_GC(tracer(), GCTracer::Scope::SCAVENGER_SCAVENGE);
  base::MutexGuard guard(relocation_mutex());
  ConcurrentMarking::PauseScope pause_scope(concurrent_marking());
  // There are soft limits in the allocation code, designed to trigger a mark
  // sweep collection by failing allocations. There is no sense in trying to
  // trigger one during scavenge: scavenges allocation should always succeed.
  AlwaysAllocateScope scope(this);

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
  new_lo_space()->ResetPendingObject();

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
    PrintIsolate(isolate(), "Fast promotion mode: %s survival rate: %zu%%\n",
                 fast_promotion_mode_ ? "true" : "false",
                 survived_in_new_space);
  }
}

void Heap::UnprotectAndRegisterMemoryChunk(MemoryChunk* chunk) {
  if (unprotected_memory_chunks_registry_enabled_) {
    base::MutexGuard guard(&unprotected_memory_chunks_mutex_);
    if (unprotected_memory_chunks_.insert(chunk).second) {
      chunk->SetReadAndWritable();
    }
  }
}

void Heap::UnprotectAndRegisterMemoryChunk(HeapObject object) {
  UnprotectAndRegisterMemoryChunk(MemoryChunk::FromHeapObject(object));
}

void Heap::UnregisterUnprotectedMemoryChunk(MemoryChunk* chunk) {
  unprotected_memory_chunks_.erase(chunk);
}

void Heap::ProtectUnprotectedMemoryChunks() {
  DCHECK(unprotected_memory_chunks_registry_enabled_);
  for (auto chunk = unprotected_memory_chunks_.begin();
       chunk != unprotected_memory_chunks_.end(); chunk++) {
    CHECK(memory_allocator()->IsMemoryChunkExecutable(*chunk));
    (*chunk)->SetDefaultCodePermissions();
  }
  unprotected_memory_chunks_.clear();
}

bool Heap::ExternalStringTable::Contains(String string) {
  for (size_t i = 0; i < young_strings_.size(); ++i) {
    if (young_strings_[i] == string) return true;
  }
  for (size_t i = 0; i < old_strings_.size(); ++i) {
    if (old_strings_[i] == string) return true;
  }
  return false;
}

void Heap::UpdateExternalString(String string, size_t old_payload,
                                size_t new_payload) {
  DCHECK(string.IsExternalString());
  Page* page = Page::FromHeapObject(string);

  if (old_payload > new_payload) {
    page->DecrementExternalBackingStoreBytes(
        ExternalBackingStoreType::kExternalString, old_payload - new_payload);
  } else {
    page->IncrementExternalBackingStoreBytes(
        ExternalBackingStoreType::kExternalString, new_payload - old_payload);
  }
}

String Heap::UpdateYoungReferenceInExternalStringTableEntry(Heap* heap,
                                                            FullObjectSlot p) {
  HeapObject obj = HeapObject::cast(*p);
  MapWord first_word = obj.map_word();

  String new_string;

  if (InFromPage(obj)) {
    if (!first_word.IsForwardingAddress()) {
      // Unreachable external string can be finalized.
      String string = String::cast(obj);
      if (!string.IsExternalString()) {
        // Original external string has been internalized.
        DCHECK(string.IsThinString());
        return String();
      }
      heap->FinalizeExternalString(string);
      return String();
    }
    new_string = String::cast(first_word.ToForwardingAddress());
  } else {
    new_string = String::cast(obj);
  }

  // String is still reachable.
  if (new_string.IsThinString()) {
    // Filtering Thin strings out of the external string table.
    return String();
  } else if (new_string.IsExternalString()) {
    MemoryChunk::MoveExternalBackingStoreBytes(
        ExternalBackingStoreType::kExternalString,
        Page::FromAddress((*p).ptr()), Page::FromHeapObject(new_string),
        ExternalString::cast(new_string).ExternalPayloadSize());
    return new_string;
  }

  // Internalization can replace external strings with non-external strings.
  return new_string.IsExternalString() ? new_string : String();
}

void Heap::ExternalStringTable::VerifyYoung() {
#ifdef DEBUG
  std::set<String> visited_map;
  std::map<MemoryChunk*, size_t> size_map;
  ExternalBackingStoreType type = ExternalBackingStoreType::kExternalString;
  for (size_t i = 0; i < young_strings_.size(); ++i) {
    String obj = String::cast(young_strings_[i]);
    MemoryChunk* mc = MemoryChunk::FromHeapObject(obj);
    DCHECK(mc->InYoungGeneration());
    DCHECK(heap_->InYoungGeneration(obj));
    DCHECK(!obj.IsTheHole(heap_->isolate()));
    DCHECK(obj.IsExternalString());
    // Note: we can have repeated elements in the table.
    DCHECK_EQ(0, visited_map.count(obj));
    visited_map.insert(obj);
    size_map[mc] += ExternalString::cast(obj).ExternalPayloadSize();
  }
  for (std::map<MemoryChunk*, size_t>::iterator it = size_map.begin();
       it != size_map.end(); it++)
    DCHECK_EQ(it->first->ExternalBackingStoreBytes(type), it->second);
#endif
}

void Heap::ExternalStringTable::Verify() {
#ifdef DEBUG
  std::set<String> visited_map;
  std::map<MemoryChunk*, size_t> size_map;
  ExternalBackingStoreType type = ExternalBackingStoreType::kExternalString;
  VerifyYoung();
  for (size_t i = 0; i < old_strings_.size(); ++i) {
    String obj = String::cast(old_strings_[i]);
    MemoryChunk* mc = MemoryChunk::FromHeapObject(obj);
    DCHECK(!mc->InYoungGeneration());
    DCHECK(!heap_->InYoungGeneration(obj));
    DCHECK(!obj.IsTheHole(heap_->isolate()));
    DCHECK(obj.IsExternalString());
    // Note: we can have repeated elements in the table.
    DCHECK_EQ(0, visited_map.count(obj));
    visited_map.insert(obj);
    size_map[mc] += ExternalString::cast(obj).ExternalPayloadSize();
  }
  for (std::map<MemoryChunk*, size_t>::iterator it = size_map.begin();
       it != size_map.end(); it++)
    DCHECK_EQ(it->first->ExternalBackingStoreBytes(type), it->second);
#endif
}

void Heap::ExternalStringTable::UpdateYoungReferences(
    Heap::ExternalStringTableUpdaterCallback updater_func) {
  if (young_strings_.empty()) return;

  FullObjectSlot start(young_strings_.data());
  FullObjectSlot end(young_strings_.data() + young_strings_.size());
  FullObjectSlot last = start;

  for (FullObjectSlot p = start; p < end; ++p) {
    String target = updater_func(heap_, p);

    if (target.is_null()) continue;

    DCHECK(target.IsExternalString());

    if (InYoungGeneration(target)) {
      // String is still in new space. Update the table entry.
      last.store(target);
      ++last;
    } else {
      // String got promoted. Move it to the old string list.
      old_strings_.push_back(target);
    }
  }

  DCHECK(last <= end);
  young_strings_.resize(last - start);
#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) {
    VerifyYoung();
  }
#endif
}

void Heap::ExternalStringTable::PromoteYoung() {
  old_strings_.reserve(old_strings_.size() + young_strings_.size());
  std::move(std::begin(young_strings_), std::end(young_strings_),
            std::back_inserter(old_strings_));
  young_strings_.clear();
}

void Heap::ExternalStringTable::IterateYoung(RootVisitor* v) {
  if (!young_strings_.empty()) {
    v->VisitRootPointers(
        Root::kExternalStringsTable, nullptr,
        FullObjectSlot(young_strings_.data()),
        FullObjectSlot(young_strings_.data() + young_strings_.size()));
  }
}

void Heap::ExternalStringTable::IterateAll(RootVisitor* v) {
  IterateYoung(v);
  if (!old_strings_.empty()) {
    v->VisitRootPointers(
        Root::kExternalStringsTable, nullptr,
        FullObjectSlot(old_strings_.data()),
        FullObjectSlot(old_strings_.data() + old_strings_.size()));
  }
}

void Heap::UpdateYoungReferencesInExternalStringTable(
    ExternalStringTableUpdaterCallback updater_func) {
  external_string_table_.UpdateYoungReferences(updater_func);
}

void Heap::ExternalStringTable::UpdateReferences(
    Heap::ExternalStringTableUpdaterCallback updater_func) {
  if (old_strings_.size() > 0) {
    FullObjectSlot start(old_strings_.data());
    FullObjectSlot end(old_strings_.data() + old_strings_.size());
    for (FullObjectSlot p = start; p < end; ++p)
      p.store(updater_func(heap_, p));
  }

  UpdateYoungReferences(updater_func);
}

void Heap::UpdateReferencesInExternalStringTable(
    ExternalStringTableUpdaterCallback updater_func) {
  external_string_table_.UpdateReferences(updater_func);
}


void Heap::ProcessAllWeakReferences(WeakObjectRetainer* retainer) {
  ProcessNativeContexts(retainer);
  ProcessAllocationSites(retainer);
  ProcessDirtyJSFinalizationRegistries(retainer);
}


void Heap::ProcessYoungWeakReferences(WeakObjectRetainer* retainer) {
  ProcessNativeContexts(retainer);
}


void Heap::ProcessNativeContexts(WeakObjectRetainer* retainer) {
  Object head = VisitWeakList<Context>(this, native_contexts_list(), retainer);
  // Update the head of the list of contexts.
  set_native_contexts_list(head);
}


void Heap::ProcessAllocationSites(WeakObjectRetainer* retainer) {
  Object allocation_site_obj =
      VisitWeakList<AllocationSite>(this, allocation_sites_list(), retainer);
  set_allocation_sites_list(allocation_site_obj);
}

void Heap::ProcessDirtyJSFinalizationRegistries(WeakObjectRetainer* retainer) {
  Object head = VisitWeakList<JSFinalizationRegistry>(
      this, dirty_js_finalization_registries_list(), retainer);
  set_dirty_js_finalization_registries_list(head);
  // If the list is empty, set the tail to undefined. Otherwise the tail is set
  // by WeakListVisitor<JSFinalizationRegistry>::VisitLiveObject.
  if (head.IsUndefined(isolate())) {
    set_dirty_js_finalization_registries_list_tail(head);
  }
}

void Heap::ProcessWeakListRoots(WeakObjectRetainer* retainer) {
  set_native_contexts_list(retainer->RetainAs(native_contexts_list()));
  set_allocation_sites_list(retainer->RetainAs(allocation_sites_list()));
  set_dirty_js_finalization_registries_list(
      retainer->RetainAs(dirty_js_finalization_registries_list()));
  set_dirty_js_finalization_registries_list_tail(
      retainer->RetainAs(dirty_js_finalization_registries_list_tail()));
}

void Heap::ForeachAllocationSite(
    Object list, const std::function<void(AllocationSite)>& visitor) {
  DisallowHeapAllocation disallow_heap_allocation;
  Object current = list;
  while (current.IsAllocationSite()) {
    AllocationSite site = AllocationSite::cast(current);
    visitor(site);
    Object current_nested = site.nested_site();
    while (current_nested.IsAllocationSite()) {
      AllocationSite nested_site = AllocationSite::cast(current_nested);
      visitor(nested_site);
      current_nested = nested_site.nested_site();
    }
    current = site.weak_next();
  }
}

void Heap::ResetAllAllocationSitesDependentCode(AllocationType allocation) {
  DisallowHeapAllocation no_allocation_scope;
  bool marked = false;

  ForeachAllocationSite(allocation_sites_list(),
                        [&marked, allocation, this](AllocationSite site) {
                          if (site.GetAllocationType() == allocation) {
                            site.ResetPretenureDecision();
                            site.set_deopt_dependent_code(true);
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
    ResetAllAllocationSitesDependentCode(AllocationType::kOld);
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
                           FullObjectSlot start, FullObjectSlot end) override {
      for (FullObjectSlot p = start; p < end; ++p) {
        DCHECK((*p).IsExternalString());
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

STATIC_ASSERT(IsAligned(FixedDoubleArray::kHeaderSize, kDoubleAlignment));

#ifdef V8_COMPRESS_POINTERS
// TODO(ishell, v8:8875): When pointer compression is enabled the kHeaderSize
// is only kTaggedSize aligned but we can keep using unaligned access since
// both x64 and arm64 architectures (where pointer compression supported)
// allow unaligned access to doubles.
STATIC_ASSERT(IsAligned(ByteArray::kHeaderSize, kTaggedSize));
#else
STATIC_ASSERT(IsAligned(ByteArray::kHeaderSize, kDoubleAlignment));
#endif

#ifdef V8_HOST_ARCH_32_BIT
// NOLINTNEXTLINE(runtime/references) (false positive)
STATIC_ASSERT((HeapNumber::kValueOffset & kDoubleAlignmentMask) == kTaggedSize);
#endif


int Heap::GetMaximumFillToAlign(AllocationAlignment alignment) {
  switch (alignment) {
    case kWordAligned:
      return 0;
    case kDoubleAligned:
    case kDoubleUnaligned:
      return kDoubleSize - kTaggedSize;
    default:
      UNREACHABLE();
  }
  return 0;
}


int Heap::GetFillToAlign(Address address, AllocationAlignment alignment) {
  if (alignment == kDoubleAligned && (address & kDoubleAlignmentMask) != 0)
    return kTaggedSize;
  if (alignment == kDoubleUnaligned && (address & kDoubleAlignmentMask) == 0)
    return kDoubleSize - kTaggedSize;  // No fill if double is always aligned.
  return 0;
}

size_t Heap::GetCodeRangeReservedAreaSize() {
  return kReservedCodeRangePages * MemoryAllocator::GetCommitPageSize();
}

HeapObject Heap::PrecedeWithFiller(HeapObject object, int filler_size) {
  CreateFillerObjectAt(object.address(), filler_size, ClearRecordedSlots::kNo);
  return HeapObject::FromAddress(object.address() + filler_size);
}

HeapObject Heap::AlignWithFiller(HeapObject object, int object_size,
                                 int allocation_size,
                                 AllocationAlignment alignment) {
  int filler_size = allocation_size - object_size;
  DCHECK_LT(0, filler_size);
  int pre_filler = GetFillToAlign(object.address(), alignment);
  if (pre_filler) {
    object = PrecedeWithFiller(object, pre_filler);
    filler_size -= pre_filler;
  }
  if (filler_size) {
    CreateFillerObjectAt(object.address() + object_size, filler_size,
                         ClearRecordedSlots::kNo);
  }
  return object;
}

void* Heap::AllocateExternalBackingStore(
    const std::function<void*(size_t)>& allocate, size_t byte_length) {
  if (!always_allocate()) {
    size_t new_space_backing_store_bytes =
        new_space()->ExternalBackingStoreBytes();
    if (new_space_backing_store_bytes >= 2 * kMaxSemiSpaceSize &&
        new_space_backing_store_bytes >= byte_length) {
      // Performing a young generation GC amortizes over the allocated backing
      // store bytes and may free enough external bytes for this allocation.
      CollectGarbage(NEW_SPACE,
                     GarbageCollectionReason::kExternalMemoryPressure);
    }
  }
  // TODO(ulan): Perform GCs proactively based on the byte_length and
  // the current external backing store counters.
  void* result = allocate(byte_length);
  if (result) return result;
  if (!always_allocate()) {
    for (int i = 0; i < 2; i++) {
      CollectGarbage(OLD_SPACE,
                     GarbageCollectionReason::kExternalMemoryPressure);
      result = allocate(byte_length);
      if (result) return result;
    }
    isolate()->counters()->gc_last_resort_from_handles()->Increment();
    CollectAllAvailableGarbage(
        GarbageCollectionReason::kExternalMemoryPressure);
  }
  return allocate(byte_length);
}

void Heap::RegisterBackingStore(JSArrayBuffer buffer,
                                std::shared_ptr<BackingStore> backing_store) {
  ArrayBufferTracker::RegisterNew(this, buffer, std::move(backing_store));
}

std::shared_ptr<BackingStore> Heap::UnregisterBackingStore(
    JSArrayBuffer buffer) {
  return ArrayBufferTracker::Unregister(this, buffer);
}

std::shared_ptr<BackingStore> Heap::LookupBackingStore(JSArrayBuffer buffer) {
  return ArrayBufferTracker::Lookup(this, buffer);
}

void Heap::ConfigureInitialOldGenerationSize() {
  if (!old_generation_size_configured_ && tracer()->SurvivalEventsRecorded()) {
    const size_t minimum_growing_step =
        MemoryController<V8HeapTrait>::MinimumAllocationLimitGrowingStep(
            CurrentHeapGrowingMode());
    const size_t new_old_generation_allocation_limit =
        Max(OldGenerationSizeOfObjects() + minimum_growing_step,
            static_cast<size_t>(
                static_cast<double>(old_generation_allocation_limit_) *
                (tracer()->AverageSurvivalRatio() / 100)));
    if (new_old_generation_allocation_limit <
        old_generation_allocation_limit_) {
      old_generation_allocation_limit_ = new_old_generation_allocation_limit;
    } else {
      old_generation_size_configured_ = true;
    }
    if (UseGlobalMemoryScheduling()) {
      const size_t new_global_memory_limit = Max(
          GlobalSizeOfObjects() + minimum_growing_step,
          static_cast<size_t>(static_cast<double>(global_allocation_limit_) *
                              (tracer()->AverageSurvivalRatio() / 100)));
      if (new_global_memory_limit < global_allocation_limit_) {
        global_allocation_limit_ = new_global_memory_limit;
      }
    }
  }
}

void Heap::FlushNumberStringCache() {
  // Flush the number to string cache.
  int len = number_string_cache().length();
  for (int i = 0; i < len; i++) {
    number_string_cache().set_undefined(i);
  }
}

HeapObject Heap::CreateFillerObjectAt(Address addr, int size,
                                      ClearRecordedSlots clear_slots_mode,
                                      ClearFreedMemoryMode clear_memory_mode) {
  if (size == 0) return HeapObject();
  HeapObject filler = HeapObject::FromAddress(addr);
  bool clear_memory =
      (clear_memory_mode == ClearFreedMemoryMode::kClearFreedMemory ||
       clear_slots_mode == ClearRecordedSlots::kYes);
  if (size == kTaggedSize) {
    filler.set_map_after_allocation(
        Map::unchecked_cast(isolate()->root(RootIndex::kOnePointerFillerMap)),
        SKIP_WRITE_BARRIER);
  } else if (size == 2 * kTaggedSize) {
    filler.set_map_after_allocation(
        Map::unchecked_cast(isolate()->root(RootIndex::kTwoPointerFillerMap)),
        SKIP_WRITE_BARRIER);
    if (clear_memory) {
      AtomicSlot slot(ObjectSlot(addr) + 1);
      *slot = static_cast<Tagged_t>(kClearedFreeMemoryValue);
    }
  } else {
    DCHECK_GT(size, 2 * kTaggedSize);
    filler.set_map_after_allocation(
        Map::unchecked_cast(isolate()->root(RootIndex::kFreeSpaceMap)),
        SKIP_WRITE_BARRIER);
    FreeSpace::cast(filler).relaxed_write_size(size);
    if (clear_memory) {
      MemsetTagged(ObjectSlot(addr) + 2, Object(kClearedFreeMemoryValue),
                   (size / kTaggedSize) - 2);
    }
  }
  if (clear_slots_mode == ClearRecordedSlots::kYes &&
      !V8_ENABLE_THIRD_PARTY_HEAP_BOOL) {
    ClearRecordedSlotRange(addr, addr + size);
  }

  // At this point, we may be deserializing the heap from a snapshot, and
  // none of the maps have been created yet and are nullptr.
  DCHECK((filler.map_slot().contains_value(kNullAddress) &&
          !deserialization_complete_) ||
         filler.map().IsMap());
  return filler;
}

bool Heap::CanMoveObjectStart(HeapObject object) {
  if (!FLAG_move_object_start) return false;

  // Sampling heap profiler may have a reference to the object.
  if (isolate()->heap_profiler()->is_sampling_allocations()) return false;

  if (IsLargeObject(object)) return false;

  // We can move the object start if the page was already swept.
  return Page::FromHeapObject(object)->SweepingDone();
}

// static
bool Heap::InOffThreadSpace(HeapObject heap_object) {
#ifdef V8_ENABLE_THIRD_PARTY_HEAP
  return false;  // currently unsupported
#else
  Space* owner = MemoryChunk::FromHeapObject(heap_object)->owner();
  if (owner->identity() == OLD_SPACE) {
    // TODO(leszeks): Should we exclude compaction spaces here?
    return static_cast<PagedSpace*>(owner)->is_off_thread_space();
  }
  if (owner->identity() == LO_SPACE) {
    return static_cast<LargeObjectSpace*>(owner)->is_off_thread();
  }
  return false;
#endif
}

bool Heap::IsImmovable(HeapObject object) {
  if (V8_ENABLE_THIRD_PARTY_HEAP_BOOL) {
    // TODO(steveblackburn): For now all objects are immovable.
    // Will need to revisit once moving is supported.
    return true;
  }

  MemoryChunk* chunk = MemoryChunk::FromHeapObject(object);
  return chunk->NeverEvacuate() || IsLargeObject(object);
}

bool Heap::IsLargeObject(HeapObject object) {
  return MemoryChunk::FromHeapObject(object)->IsLargePage();
}

#ifdef ENABLE_SLOW_DCHECKS
namespace {

class LeftTrimmerVerifierRootVisitor : public RootVisitor {
 public:
  explicit LeftTrimmerVerifierRootVisitor(FixedArrayBase to_check)
      : to_check_(to_check) {}

  void VisitRootPointers(Root root, const char* description,
                         FullObjectSlot start, FullObjectSlot end) override {
    for (FullObjectSlot p = start; p < end; ++p) {
      DCHECK_NE(*p, to_check_);
    }
  }

 private:
  FixedArrayBase to_check_;

  DISALLOW_COPY_AND_ASSIGN(LeftTrimmerVerifierRootVisitor);
};
}  // namespace
#endif  // ENABLE_SLOW_DCHECKS

namespace {
bool MayContainRecordedSlots(HeapObject object) {
  // New space object do not have recorded slots.
  if (MemoryChunk::FromHeapObject(object)->InYoungGeneration()) return false;
  // Whitelist objects that definitely do not have pointers.
  if (object.IsByteArray() || object.IsFixedDoubleArray()) return false;
  // Conservatively return true for other objects.
  return true;
}
}  // namespace

void Heap::OnMoveEvent(HeapObject target, HeapObject source,
                       int size_in_bytes) {
  HeapProfiler* heap_profiler = isolate_->heap_profiler();
  if (heap_profiler->is_tracking_object_moves()) {
    heap_profiler->ObjectMoveEvent(source.address(), target.address(),
                                   size_in_bytes);
  }
  for (auto& tracker : allocation_trackers_) {
    tracker->MoveEvent(source.address(), target.address(), size_in_bytes);
  }
  if (target.IsSharedFunctionInfo()) {
    LOG_CODE_EVENT(isolate_, SharedFunctionInfoMoveEvent(source.address(),
                                                         target.address()));
  } else if (target.IsNativeContext()) {
    PROFILE(isolate_,
            NativeContextMoveEvent(source.address(), target.address()));
  }

  if (FLAG_verify_predictable) {
    ++allocations_count_;
    // Advance synthetic time by making a time request.
    MonotonicallyIncreasingTimeInMs();

    UpdateAllocationsHash(source);
    UpdateAllocationsHash(target);
    UpdateAllocationsHash(size_in_bytes);

    if (allocations_count_ % FLAG_dump_allocations_digest_at_alloc == 0) {
      PrintAllocationsHash();
    }
  } else if (FLAG_fuzzer_gc_analysis) {
    ++allocations_count_;
  }
}

FixedArrayBase Heap::LeftTrimFixedArray(FixedArrayBase object,
                                        int elements_to_trim) {
  if (elements_to_trim == 0) {
    // This simplifies reasoning in the rest of the function.
    return object;
  }
  CHECK(!object.is_null());
  DCHECK(CanMoveObjectStart(object));
  // Add custom visitor to concurrent marker if new left-trimmable type
  // is added.
  DCHECK(object.IsFixedArray() || object.IsFixedDoubleArray());
  const int element_size = object.IsFixedArray() ? kTaggedSize : kDoubleSize;
  const int bytes_to_trim = elements_to_trim * element_size;
  Map map = object.map();

  // For now this trick is only applied to fixed arrays which may be in new
  // space or old space. In a large object space the object's start must
  // coincide with chunk and thus the trick is just not applicable.
  DCHECK(!IsLargeObject(object));
  DCHECK(object.map() != ReadOnlyRoots(this).fixed_cow_array_map());

  STATIC_ASSERT(FixedArrayBase::kMapOffset == 0);
  STATIC_ASSERT(FixedArrayBase::kLengthOffset == kTaggedSize);
  STATIC_ASSERT(FixedArrayBase::kHeaderSize == 2 * kTaggedSize);

  const int len = object.length();
  DCHECK(elements_to_trim <= len);

  // Calculate location of new array start.
  Address old_start = object.address();
  Address new_start = old_start + bytes_to_trim;

  if (incremental_marking()->IsMarking()) {
    incremental_marking()->NotifyLeftTrimming(
        object, HeapObject::FromAddress(new_start));
  }

#ifdef DEBUG
  if (MayContainRecordedSlots(object)) {
    MemoryChunk* chunk = MemoryChunk::FromHeapObject(object);
    DCHECK(!chunk->RegisteredObjectWithInvalidatedSlots<OLD_TO_OLD>(object));
    DCHECK(!chunk->RegisteredObjectWithInvalidatedSlots<OLD_TO_NEW>(object));
  }
#endif

  // Technically in new space this write might be omitted (except for
  // debug mode which iterates through the heap), but to play safer
  // we still do it.
  CreateFillerObjectAt(old_start, bytes_to_trim,
                       MayContainRecordedSlots(object)
                           ? ClearRecordedSlots::kYes
                           : ClearRecordedSlots::kNo);

  // Initialize header of the trimmed array. Since left trimming is only
  // performed on pages which are not concurrently swept creating a filler
  // object does not require synchronization.
  RELAXED_WRITE_FIELD(object, bytes_to_trim, map);
  RELAXED_WRITE_FIELD(object, bytes_to_trim + kTaggedSize,
                      Smi::FromInt(len - elements_to_trim));

  FixedArrayBase new_object =
      FixedArrayBase::cast(HeapObject::FromAddress(new_start));

  // Notify the heap profiler of change in object layout.
  OnMoveEvent(new_object, object, new_object.Size());

#ifdef ENABLE_SLOW_DCHECKS
  if (FLAG_enable_slow_asserts) {
    // Make sure the stack or other roots (e.g., Handles) don't contain pointers
    // to the original FixedArray (which is now the filler object).
    LeftTrimmerVerifierRootVisitor root_visitor(object);
    ReadOnlyRoots(this).Iterate(&root_visitor);
    IterateRoots(&root_visitor, VISIT_ALL);
  }
#endif  // ENABLE_SLOW_DCHECKS

  return new_object;
}

void Heap::RightTrimFixedArray(FixedArrayBase object, int elements_to_trim) {
  const int len = object.length();
  DCHECK_LE(elements_to_trim, len);
  DCHECK_GE(elements_to_trim, 0);

  int bytes_to_trim;
  if (object.IsByteArray()) {
    int new_size = ByteArray::SizeFor(len - elements_to_trim);
    bytes_to_trim = ByteArray::SizeFor(len) - new_size;
    DCHECK_GE(bytes_to_trim, 0);
  } else if (object.IsFixedArray()) {
    CHECK_NE(elements_to_trim, len);
    bytes_to_trim = elements_to_trim * kTaggedSize;
  } else {
    DCHECK(object.IsFixedDoubleArray());
    CHECK_NE(elements_to_trim, len);
    bytes_to_trim = elements_to_trim * kDoubleSize;
  }

  CreateFillerForArray<FixedArrayBase>(object, elements_to_trim, bytes_to_trim);
}

void Heap::RightTrimWeakFixedArray(WeakFixedArray object,
                                   int elements_to_trim) {
  // This function is safe to use only at the end of the mark compact
  // collection: When marking, we record the weak slots, and shrinking
  // invalidates them.
  DCHECK_EQ(gc_state(), MARK_COMPACT);
  CreateFillerForArray<WeakFixedArray>(object, elements_to_trim,
                                       elements_to_trim * kTaggedSize);
}

template <typename T>
void Heap::CreateFillerForArray(T object, int elements_to_trim,
                                int bytes_to_trim) {
  DCHECK(object.IsFixedArrayBase() || object.IsByteArray() ||
         object.IsWeakFixedArray());

  // For now this trick is only applied to objects in new and paged space.
  DCHECK(object.map() != ReadOnlyRoots(this).fixed_cow_array_map());

  if (bytes_to_trim == 0) {
    DCHECK_EQ(elements_to_trim, 0);
    // No need to create filler and update live bytes counters.
    return;
  }

  // Calculate location of new array end.
  int old_size = object.Size();
  Address old_end = object.address() + old_size;
  Address new_end = old_end - bytes_to_trim;

#ifdef DEBUG
  if (MayContainRecordedSlots(object)) {
    MemoryChunk* chunk = MemoryChunk::FromHeapObject(object);
    DCHECK(!chunk->RegisteredObjectWithInvalidatedSlots<OLD_TO_NEW>(object));
    DCHECK(!chunk->RegisteredObjectWithInvalidatedSlots<OLD_TO_OLD>(object));
  }
#endif

  bool clear_slots = MayContainRecordedSlots(object);

  // Technically in new space this write might be omitted (except for
  // debug mode which iterates through the heap), but to play safer
  // we still do it.
  // We do not create a filler for objects in a large object space.
  if (!IsLargeObject(object)) {
    HeapObject filler = CreateFillerObjectAt(
        new_end, bytes_to_trim,
        clear_slots ? ClearRecordedSlots::kYes : ClearRecordedSlots::kNo);
    DCHECK(!filler.is_null());
    // Clear the mark bits of the black area that belongs now to the filler.
    // This is an optimization. The sweeper will release black fillers anyway.
    if (incremental_marking()->black_allocation() &&
        incremental_marking()->marking_state()->IsBlackOrGrey(filler)) {
      Page* page = Page::FromAddress(new_end);
      incremental_marking()->marking_state()->bitmap(page)->ClearRange(
          page->AddressToMarkbitIndex(new_end),
          page->AddressToMarkbitIndex(new_end + bytes_to_trim));
    }
  } else if (clear_slots) {
    // Large objects are not swept, so it is not necessary to clear the
    // recorded slot.
    MemsetTagged(ObjectSlot(new_end), Object(kClearedFreeMemoryValue),
                 (old_end - new_end) / kTaggedSize);
  }

  // Initialize header of the trimmed array. We are storing the new length
  // using release store after creating a filler for the left-over space to
  // avoid races with the sweeper thread.
  object.synchronized_set_length(object.length() - elements_to_trim);

  // Notify the heap object allocation tracker of change in object layout. The
  // array may not be moved during GC, and size has to be adjusted nevertheless.
  for (auto& tracker : allocation_trackers_) {
    tracker->UpdateObjectSizeEvent(object.address(), object.Size());
  }
}

void Heap::MakeHeapIterable() {
  mark_compact_collector()->EnsureSweepingCompleted();
}

namespace {

double ComputeMutatorUtilizationImpl(double mutator_speed, double gc_speed) {
  constexpr double kMinMutatorUtilization = 0.0;
  constexpr double kConservativeGcSpeedInBytesPerMillisecond = 200000;
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

}  // namespace

double Heap::ComputeMutatorUtilization(const char* tag, double mutator_speed,
                                       double gc_speed) {
  double result = ComputeMutatorUtilizationImpl(mutator_speed, gc_speed);
  if (FLAG_trace_mutator_utilization) {
    isolate()->PrintWithTimestamp(
        "%s mutator utilization = %.3f ("
        "mutator_speed=%.f, gc_speed=%.f)\n",
        tag, result, mutator_speed, gc_speed);
  }
  return result;
}

bool Heap::HasLowYoungGenerationAllocationRate() {
  double mu = ComputeMutatorUtilization(
      "Young generation",
      tracer()->NewSpaceAllocationThroughputInBytesPerMillisecond(),
      tracer()->ScavengeSpeedInBytesPerMillisecond(kForSurvivedObjects));
  constexpr double kHighMutatorUtilization = 0.993;
  return mu > kHighMutatorUtilization;
}

bool Heap::HasLowOldGenerationAllocationRate() {
  double mu = ComputeMutatorUtilization(
      "Old generation",
      tracer()->OldGenerationAllocationThroughputInBytesPerMillisecond(),
      tracer()->CombinedMarkCompactSpeedInBytesPerMillisecond());
  const double kHighMutatorUtilization = 0.993;
  return mu > kHighMutatorUtilization;
}

bool Heap::HasLowEmbedderAllocationRate() {
  if (!UseGlobalMemoryScheduling()) return true;

  DCHECK_NOT_NULL(local_embedder_heap_tracer());
  double mu = ComputeMutatorUtilization(
      "Embedder",
      tracer()->CurrentEmbedderAllocationThroughputInBytesPerMillisecond(),
      tracer()->EmbedderSpeedInBytesPerMillisecond());
  const double kHighMutatorUtilization = 0.993;
  return mu > kHighMutatorUtilization;
}

bool Heap::HasLowAllocationRate() {
  return HasLowYoungGenerationAllocationRate() &&
         HasLowOldGenerationAllocationRate() && HasLowEmbedderAllocationRate();
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
    new_lo_space_->SetCapacity(new_space_->Capacity());
    UncommitFromSpace();
  }
}

void Heap::FinalizeIncrementalMarkingIfComplete(
    GarbageCollectionReason gc_reason) {
  if (incremental_marking()->IsMarking() &&
      (incremental_marking()->IsReadyToOverApproximateWeakClosure() ||
       (!incremental_marking()->finalize_marking_completed() &&
        mark_compact_collector()->marking_worklists()->IsEmpty() &&
        local_embedder_heap_tracer()->ShouldFinalizeIncrementalMarking()))) {
    FinalizeIncrementalMarkingIncrementally(gc_reason);
  } else if (incremental_marking()->IsComplete() ||
             (incremental_marking()->IsMarking() &&
              mark_compact_collector()->marking_worklists()->IsEmpty() &&
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

  DevToolsTraceEventScope devtools_trace_event_scope(
      this, "MajorGC", "incremental finalization step");

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
    Reservation* reservations, const std::vector<HeapObject>& large_objects,
    const std::vector<Address>& maps) {
  // TODO(ulan): pause black allocation during deserialization to avoid
  // iterating all these objects in one go.

  if (!incremental_marking()->black_allocation()) return;

  // Iterate black objects in old space, code space, map space, and large
  // object space for side effects.
  IncrementalMarking::MarkingState* marking_state =
      incremental_marking()->marking_state();
  for (int i = OLD_SPACE;
       i < static_cast<int>(SnapshotSpace::kNumberOfHeapSpaces); i++) {
    const Heap::Reservation& res = reservations[i];
    for (auto& chunk : res) {
      Address addr = chunk.start;
      while (addr < chunk.end) {
        HeapObject obj = HeapObject::FromAddress(addr);
        // Objects can have any color because incremental marking can
        // start in the middle of Heap::ReserveSpace().
        if (marking_state->IsBlack(obj)) {
          incremental_marking()->ProcessBlackAllocatedObject(obj);
        }
        addr += obj.Size();
      }
    }
  }

  // Large object space doesn't use reservations, so it needs custom handling.
  for (HeapObject object : large_objects) {
    incremental_marking()->ProcessBlackAllocatedObject(object);
  }

  // Map space doesn't use reservations, so it needs custom handling.
  for (Address addr : maps) {
    incremental_marking()->ProcessBlackAllocatedObject(
        HeapObject::FromAddress(addr));
  }
}

void Heap::NotifyObjectLayoutChange(
    HeapObject object, const DisallowHeapAllocation&,
    InvalidateRecordedSlots invalidate_recorded_slots) {
  if (incremental_marking()->IsMarking()) {
    incremental_marking()->MarkBlackAndVisitObjectDueToLayoutChange(object);
    if (incremental_marking()->IsCompacting() &&
        invalidate_recorded_slots == InvalidateRecordedSlots::kYes &&
        MayContainRecordedSlots(object)) {
      MemoryChunk::FromHeapObject(object)
          ->RegisterObjectWithInvalidatedSlots<OLD_TO_OLD>(object);
    }
  }
  if (invalidate_recorded_slots == InvalidateRecordedSlots::kYes &&
      MayContainRecordedSlots(object)) {
    MemoryChunk::FromHeapObject(object)
        ->RegisterObjectWithInvalidatedSlots<OLD_TO_NEW>(object);
  }
#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) {
    DCHECK(pending_layout_change_object_.is_null());
    pending_layout_change_object_ = object;
  }
#endif
}

#ifdef VERIFY_HEAP
// Helper class for collecting slot addresses.
class SlotCollectingVisitor final : public ObjectVisitor {
 public:
  void VisitPointers(HeapObject host, ObjectSlot start,
                     ObjectSlot end) override {
    VisitPointers(host, MaybeObjectSlot(start), MaybeObjectSlot(end));
  }
  void VisitPointers(HeapObject host, MaybeObjectSlot start,
                     MaybeObjectSlot end) final {
    for (MaybeObjectSlot p = start; p < end; ++p) {
      slots_.push_back(p);
    }
  }

  void VisitCodeTarget(Code host, RelocInfo* rinfo) final { UNREACHABLE(); }

  void VisitEmbeddedPointer(Code host, RelocInfo* rinfo) override {
    UNREACHABLE();
  }

  int number_of_slots() { return static_cast<int>(slots_.size()); }

  MaybeObjectSlot slot(int i) { return slots_[i]; }

 private:
  std::vector<MaybeObjectSlot> slots_;
};

void Heap::VerifyObjectLayoutChange(HeapObject object, Map new_map) {
  if (!FLAG_verify_heap) return;

  // Check that Heap::NotifyObjectLayout was called for object transitions
  // that are not safe for concurrent marking.
  // If you see this check triggering for a freshly allocated object,
  // use object->set_map_after_allocation() to initialize its map.
  if (pending_layout_change_object_.is_null()) {
    if (object.IsJSObject()) {
      DCHECK(!object.map().TransitionRequiresSynchronizationWithGC(new_map));
    } else {
      // Check that the set of slots before and after the transition match.
      SlotCollectingVisitor old_visitor;
      object.IterateFast(&old_visitor);
      MapWord old_map_word = object.map_word();
      // Temporarily set the new map to iterate new slots.
      object.set_map_word(MapWord::FromMap(new_map));
      SlotCollectingVisitor new_visitor;
      object.IterateFast(&new_visitor);
      // Restore the old map.
      object.set_map_word(old_map_word);
      DCHECK_EQ(new_visitor.number_of_slots(), old_visitor.number_of_slots());
      for (int i = 0; i < new_visitor.number_of_slots(); i++) {
        DCHECK(new_visitor.slot(i) == old_visitor.slot(i));
      }
    }
  } else {
    DCHECK_EQ(pending_layout_change_object_, object);
    pending_layout_change_object_ = HeapObject();
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
  switch (action) {
    case GCIdleTimeAction::kDone:
      result = true;
      break;
    case GCIdleTimeAction::kIncrementalStep: {
      incremental_marking()->AdvanceWithDeadline(
          deadline_in_ms, IncrementalMarking::NO_GC_VIA_STACK_GUARD,
          StepOrigin::kTask);
      FinalizeIncrementalMarkingIfComplete(
          GarbageCollectionReason::kFinalizeMarkingViaTask);
      result = incremental_marking()->IsStopped();
      break;
    }
    case GCIdleTimeAction::kFullGC: {
      DCHECK_LT(0, contexts_disposed_);
      HistogramTimerScope scope(isolate_->counters()->gc_context());
      TRACE_EVENT0("v8", "V8.GCContext");
      CollectAllGarbage(kNoGCFlags, GarbageCollectionReason::kContextDisposal);
      break;
    }
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

  if (FLAG_trace_idle_notification) {
    isolate_->PrintWithTimestamp(
        "Idle notification: requested idle time %.2f ms, used idle time %.2f "
        "ms, deadline usage %.2f ms [",
        idle_time_in_ms, idle_time_in_ms - deadline_difference,
        deadline_difference);
    switch (action) {
      case GCIdleTimeAction::kDone:
        PrintF("done");
        break;
      case GCIdleTimeAction::kIncrementalStep:
        PrintF("incremental step");
        break;
      case GCIdleTimeAction::kFullGC:
        PrintF("full GC");
        break;
    }
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
                             OldGenerationAllocationCounter(),
                             EmbedderAllocationCounter());

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
  int64_t potential_garbage = (CommittedMemory() - SizeOfObjects()) +
                              isolate()->isolate_data()->external_memory_;
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
      taskrunner->PostTask(std::make_unique<MemoryPressureInterruptTask>(this));
    }
  }
}

void Heap::EagerlyFreeExternalMemory() {
  for (Page* page : *old_space()) {
    if (!page->SweepingDone()) {
      base::MutexGuard guard(page->mutex());
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

void Heap::AppendArrayBufferExtension(JSArrayBuffer object,
                                      ArrayBufferExtension* extension) {
  array_buffer_sweeper_->Append(object, extension);
}

void Heap::AddLocalHeap(LocalHeap* local_heap) {
  base::MutexGuard guard(&local_heaps_mutex_);
  if (local_heaps_head_) local_heaps_head_->prev_ = local_heap;
  local_heap->prev_ = nullptr;
  local_heap->next_ = local_heaps_head_;
  local_heaps_head_ = local_heap;
}

void Heap::RemoveLocalHeap(LocalHeap* local_heap) {
  base::MutexGuard guard(&local_heaps_mutex_);
  if (local_heap->next_) local_heap->next_->prev_ = local_heap->prev_;
  if (local_heap->prev_)
    local_heap->prev_->next_ = local_heap->next_;
  else
    local_heaps_head_ = local_heap->next_;
}

bool Heap::ContainsLocalHeap(LocalHeap* local_heap) {
  base::MutexGuard guard(&local_heaps_mutex_);
  LocalHeap* current = local_heaps_head_;

  while (current) {
    if (current == local_heap) return true;
    current = current->next_;
  }

  return false;
}

bool Heap::ContainsAnyLocalHeap() {
  base::MutexGuard guard(&local_heaps_mutex_);
  return local_heaps_head_ != nullptr;
}

void Heap::AutomaticallyRestoreInitialHeapLimit(double threshold_percent) {
  initial_max_old_generation_size_threshold_ =
      initial_max_old_generation_size_ * threshold_percent;
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
      max_old_generation_size_ =
          Min(heap_limit, AllocatorLimitOnMaxOldGenerationSize());
      return true;
    }
  }
  return false;
}

bool Heap::MeasureMemory(std::unique_ptr<v8::MeasureMemoryDelegate> delegate,
                         v8::MeasureMemoryExecution execution) {
  HandleScope handle_scope(isolate());
  std::vector<Handle<NativeContext>> contexts = FindAllNativeContexts();
  std::vector<Handle<NativeContext>> to_measure;
  for (auto& current : contexts) {
    if (delegate->ShouldMeasure(
            v8::Utils::ToLocal(Handle<Context>::cast(current)))) {
      to_measure.push_back(current);
    }
  }
  return memory_measurement_->EnqueueRequest(std::move(delegate), execution,
                                             to_measure);
}

std::unique_ptr<v8::MeasureMemoryDelegate> Heap::MeasureMemoryDelegate(
    Handle<NativeContext> context, Handle<JSPromise> promise,
    v8::MeasureMemoryMode mode) {
  return i::MemoryMeasurement::DefaultDelegate(isolate_, context, promise,
                                               mode);
}

void Heap::CollectCodeStatistics() {
  TRACE_EVENT0("v8", "Heap::CollectCodeStatistics");
  CodeStatistics::ResetCodeAndMetadataStatistics(isolate());
  // We do not look for code in new space, or map space.  If code
  // somehow ends up in those spaces, we would miss it here.
  CodeStatistics::CollectCodeStatistics(code_space_, isolate());
  CodeStatistics::CollectCodeStatistics(old_space_, isolate());
  CodeStatistics::CollectCodeStatistics(code_lo_space_, isolate());
}

#ifdef DEBUG

void Heap::Print() {
  if (!HasBeenSetUp()) return;
  isolate()->PrintStack(stdout);

  for (SpaceIterator it(this); it.HasNext();) {
    it.Next()->Print();
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
    case GarbageCollectionReason::kTask:
      return "task";
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
    case GarbageCollectionReason::kGlobalAllocationLimit:
      return "global allocation limit";
    case GarbageCollectionReason::kMeasureMemory:
      return "measure memory";
    case GarbageCollectionReason::kUnknown:
      return "unknown";
  }
  UNREACHABLE();
}

bool Heap::Contains(HeapObject value) {
  if (V8_ENABLE_THIRD_PARTY_HEAP_BOOL) {
    return true;
  }
  if (ReadOnlyHeap::Contains(value)) {
    return false;
  }
  if (memory_allocator()->IsOutsideAllocatedSpace(value.address())) {
    return false;
  }
  return HasBeenSetUp() &&
         (new_space_->ToSpaceContains(value) || old_space_->Contains(value) ||
          code_space_->Contains(value) || map_space_->Contains(value) ||
          lo_space_->Contains(value) || code_lo_space_->Contains(value) ||
          new_lo_space_->Contains(value));
}

bool Heap::InSpace(HeapObject value, AllocationSpace space) {
  if (memory_allocator()->IsOutsideAllocatedSpace(value.address())) {
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
    case CODE_LO_SPACE:
      return code_lo_space_->Contains(value);
    case NEW_LO_SPACE:
      return new_lo_space_->Contains(value);
    case RO_SPACE:
      return ReadOnlyHeap::Contains(value);
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
    case CODE_LO_SPACE:
      return code_lo_space_->ContainsSlow(addr);
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
    case CODE_LO_SPACE:
    case RO_SPACE:
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
  void VerifyPointers(HeapObject host, MaybeObjectSlot start,
                      MaybeObjectSlot end) override {
    if (!host.is_null()) {
      CHECK(ReadOnlyHeap::Contains(host.map()));
    }
    VerifyPointersVisitor::VerifyPointers(host, start, end);

    for (MaybeObjectSlot current = start; current < end; ++current) {
      HeapObject heap_object;
      if ((*current)->GetHeapObject(&heap_object)) {
        CHECK(ReadOnlyHeap::Contains(heap_object));
      }
    }
  }
};

void Heap::Verify() {
  CHECK(HasBeenSetUp());
  HandleScope scope(isolate());

  // We have to wait here for the sweeper threads to have an iterable heap.
  mark_compact_collector()->EnsureSweepingCompleted();
  array_buffer_sweeper()->EnsureFinished();

  VerifyPointersVisitor visitor(this);
  IterateRoots(&visitor, VISIT_ONLY_STRONG);

  if (!isolate()->context().is_null() &&
      !isolate()->normalized_map_cache()->IsUndefined(isolate())) {
    NormalizedMapCache::cast(*isolate()->normalized_map_cache())
        .NormalizedMapCacheVerify(isolate());
  }

  VerifySmisVisitor smis_visitor;
  IterateSmiRoots(&smis_visitor);

  new_space_->Verify(isolate());

  old_space_->Verify(isolate(), &visitor);
  map_space_->Verify(isolate(), &visitor);

  VerifyPointersVisitor no_dirty_regions_visitor(this);
  code_space_->Verify(isolate(), &no_dirty_regions_visitor);

  lo_space_->Verify(isolate());
  code_lo_space_->Verify(isolate());
  new_lo_space_->Verify(isolate());
}

void Heap::VerifyReadOnlyHeap() {
  CHECK(!read_only_space_->writable());
  VerifyReadOnlyPointersVisitor read_only_visitor(this);
  read_only_space_->Verify(isolate(), &read_only_visitor);
}

class SlotVerifyingVisitor : public ObjectVisitor {
 public:
  SlotVerifyingVisitor(std::set<Address>* untyped,
                       std::set<std::pair<SlotType, Address> >* typed)
      : untyped_(untyped), typed_(typed) {}

  virtual bool ShouldHaveBeenRecorded(HeapObject host, MaybeObject target) = 0;

  void VisitPointers(HeapObject host, ObjectSlot start,
                     ObjectSlot end) override {
#ifdef DEBUG
    for (ObjectSlot slot = start; slot < end; ++slot) {
      DCHECK(!HasWeakHeapObjectTag(*slot));
    }
#endif  // DEBUG
    VisitPointers(host, MaybeObjectSlot(start), MaybeObjectSlot(end));
  }

  void VisitPointers(HeapObject host, MaybeObjectSlot start,
                     MaybeObjectSlot end) final {
    for (MaybeObjectSlot slot = start; slot < end; ++slot) {
      if (ShouldHaveBeenRecorded(host, *slot)) {
        CHECK_GT(untyped_->count(slot.address()), 0);
      }
    }
  }

  void VisitCodeTarget(Code host, RelocInfo* rinfo) override {
    Object target = Code::GetCodeFromTargetAddress(rinfo->target_address());
    if (ShouldHaveBeenRecorded(host, MaybeObject::FromObject(target))) {
      CHECK(
          InTypedSet(CODE_TARGET_SLOT, rinfo->pc()) ||
          (rinfo->IsInConstantPool() &&
           InTypedSet(CODE_ENTRY_SLOT, rinfo->constant_pool_entry_address())));
    }
  }

  void VisitEmbeddedPointer(Code host, RelocInfo* rinfo) override {
    Object target = rinfo->target_object();
    if (ShouldHaveBeenRecorded(host, MaybeObject::FromObject(target))) {
      CHECK(InTypedSet(FULL_EMBEDDED_OBJECT_SLOT, rinfo->pc()) ||
            InTypedSet(COMPRESSED_EMBEDDED_OBJECT_SLOT, rinfo->pc()) ||
            (rinfo->IsInConstantPool() &&
             InTypedSet(OBJECT_SLOT, rinfo->constant_pool_entry_address())));
    }
  }

 protected:
  bool InUntypedSet(ObjectSlot slot) {
    return untyped_->count(slot.address()) > 0;
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
                               std::set<std::pair<SlotType, Address>>* typed,
                               EphemeronRememberedSet* ephemeron_remembered_set)
      : SlotVerifyingVisitor(untyped, typed),
        ephemeron_remembered_set_(ephemeron_remembered_set) {}

  bool ShouldHaveBeenRecorded(HeapObject host, MaybeObject target) override {
    DCHECK_IMPLIES(target->IsStrongOrWeak() && Heap::InYoungGeneration(target),
                   Heap::InToPage(target));
    return target->IsStrongOrWeak() && Heap::InYoungGeneration(target) &&
           !Heap::InYoungGeneration(host);
  }

  void VisitEphemeron(HeapObject host, int index, ObjectSlot key,
                      ObjectSlot target) override {
    VisitPointer(host, target);
    if (FLAG_minor_mc) {
      VisitPointer(host, target);
    } else {
      // Keys are handled separately and should never appear in this set.
      CHECK(!InUntypedSet(key));
      Object k = *key;
      if (!ObjectInYoungGeneration(host) && ObjectInYoungGeneration(k)) {
        EphemeronHashTable table = EphemeronHashTable::cast(host);
        auto it = ephemeron_remembered_set_->find(table);
        CHECK(it != ephemeron_remembered_set_->end());
        int slot_index =
            EphemeronHashTable::SlotToIndex(table.address(), key.address());
        InternalIndex entry = EphemeronHashTable::IndexToEntry(slot_index);
        CHECK(it->second.find(entry.as_int()) != it->second.end());
      }
    }
  }

 private:
  EphemeronRememberedSet* ephemeron_remembered_set_;
};

template <RememberedSetType direction>
void CollectSlots(MemoryChunk* chunk, Address start, Address end,
                  std::set<Address>* untyped,
                  std::set<std::pair<SlotType, Address> >* typed) {
  RememberedSet<direction>::Iterate(
      chunk,
      [start, end, untyped](MaybeObjectSlot slot) {
        if (start <= slot.address() && slot.address() < end) {
          untyped->insert(slot.address());
        }
        return KEEP_SLOT;
      },
      SlotSet::FREE_EMPTY_BUCKETS);
  if (direction == OLD_TO_NEW) {
    CHECK(chunk->SweepingDone());
    RememberedSetSweeping::Iterate(
        chunk,
        [start, end, untyped](MaybeObjectSlot slot) {
          if (start <= slot.address() && slot.address() < end) {
            untyped->insert(slot.address());
          }
          return KEEP_SLOT;
        },
        SlotSet::FREE_EMPTY_BUCKETS);
  }
  RememberedSet<direction>::IterateTyped(
      chunk, [=](SlotType type, Address slot) {
        if (start <= slot && slot < end) {
          typed->insert(std::make_pair(type, slot));
        }
        return KEEP_SLOT;
      });
}

void Heap::VerifyRememberedSetFor(HeapObject object) {
  MemoryChunk* chunk = MemoryChunk::FromHeapObject(object);
  DCHECK_IMPLIES(chunk->mutex() == nullptr, ReadOnlyHeap::Contains(object));
  // In RO_SPACE chunk->mutex() may be nullptr, so just ignore it.
  base::LockGuard<base::Mutex, base::NullBehavior::kIgnoreIfNull> lock_guard(
      chunk->mutex());
  Address start = object.address();
  Address end = start + object.Size();
  std::set<Address> old_to_new;
  std::set<std::pair<SlotType, Address> > typed_old_to_new;
  if (!InYoungGeneration(object)) {
    CollectSlots<OLD_TO_NEW>(chunk, start, end, &old_to_new, &typed_old_to_new);
    OldToNewSlotVerifyingVisitor visitor(&old_to_new, &typed_old_to_new,
                                         &this->ephemeron_remembered_set_);
    object.IterateBody(&visitor);
  }
  // TODO(ulan): Add old to old slot set verification once all weak objects
  // have their own instance types and slots are recorded for all weal fields.
}
#endif

#ifdef DEBUG
void Heap::VerifyCountersAfterSweeping() {
  PagedSpaceIterator spaces(this);
  for (PagedSpace* space = spaces.Next(); space != nullptr;
       space = spaces.Next()) {
    space->VerifyCountersAfterSweeping(this);
  }
}

void Heap::VerifyCountersBeforeConcurrentSweeping() {
  PagedSpaceIterator spaces(this);
  for (PagedSpace* space = spaces.Next(); space != nullptr;
       space = spaces.Next()) {
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
  DCHECK(IsAligned(start_address, kIntSize));
  for (int i = 0; i < size_in_bytes / kIntSize; i++) {
    Memory<int>(start_address + i * kIntSize) = kCodeZapValue;
  }
#endif
}

// TODO(ishell): move builtin accessors out from Heap.
Code Heap::builtin(int index) {
  DCHECK(Builtins::IsBuiltinId(index));
  return Code::cast(Object(isolate()->builtins_table()[index]));
}

Address Heap::builtin_address(int index) {
  DCHECK(Builtins::IsBuiltinId(index) || index == Builtins::builtin_count);
  return reinterpret_cast<Address>(&isolate()->builtins_table()[index]);
}

void Heap::set_builtin(int index, Code builtin) {
  DCHECK(Builtins::IsBuiltinId(index));
  DCHECK(Internals::HasHeapObjectTag(builtin.ptr()));
  // The given builtin may be completely uninitialized thus we cannot check its
  // type here.
  isolate()->builtins_table()[index] = builtin.ptr();
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
                      FullObjectSlot(&roots_table()[RootIndex::kStringTable]));
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
  v->VisitRootPointers(Root::kSmiRootList, nullptr,
                       roots_table().smi_roots_begin(),
                       roots_table().smi_roots_end());
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
                        FullObjectSlot p) override {
    FixHandle(p);
  }

  void VisitRootPointers(Root root, const char* description,
                         FullObjectSlot start, FullObjectSlot end) override {
    for (FullObjectSlot p = start; p < end; ++p) FixHandle(p);
  }

 private:
  inline void FixHandle(FullObjectSlot p) {
    if (!(*p).IsHeapObject()) return;
    HeapObject current = HeapObject::cast(*p);
    const MapWord map_word = current.map_word();
    if (!map_word.IsForwardingAddress() && current.IsFreeSpaceOrFiller()) {
#ifdef DEBUG
      // We need to find a FixedArrayBase map after walking the fillers.
      while (current.IsFreeSpaceOrFiller()) {
        Address next = current.ptr();
        if (current.map() == ReadOnlyRoots(heap_).one_pointer_filler_map()) {
          next += kTaggedSize;
        } else if (current.map() ==
                   ReadOnlyRoots(heap_).two_pointer_filler_map()) {
          next += 2 * kTaggedSize;
        } else {
          next += current.Size();
        }
        current = HeapObject::cast(Object(next));
      }
      DCHECK(current.IsFixedArrayBase());
#endif  // DEBUG
      p.store(Smi::zero());
    }
  }

  Heap* heap_;
};

void Heap::IterateStrongRoots(RootVisitor* v, VisitMode mode) {
  const bool isMinorGC = mode == VISIT_ALL_IN_SCAVENGE ||
                         mode == VISIT_ALL_IN_MINOR_MC_MARK ||
                         mode == VISIT_ALL_IN_MINOR_MC_UPDATE;
  v->VisitRootPointers(Root::kStrongRootList, nullptr,
                       roots_table().strong_roots_begin(),
                       roots_table().strong_roots_end());
  v->Synchronize(VisitorSynchronization::kStrongRootList);

  isolate_->bootstrapper()->Iterate(v);
  v->Synchronize(VisitorSynchronization::kBootstrapper);
  if (mode != VISIT_ONLY_STRONG_IGNORE_STACK) {
    isolate_->Iterate(v);
    isolate_->global_handles()->IterateStrongStackRoots(v);
    v->Synchronize(VisitorSynchronization::kTop);
  }
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
  isolate_->IterateDeferredHandles(&left_trim_visitor);
  isolate_->IterateDeferredHandles(v);
  v->Synchronize(VisitorSynchronization::kHandleScope);

  // Iterate over the builtin code objects in the heap. Note that it is not
  // necessary to iterate over code objects on scavenge collections.
  if (!isMinorGC) {
    IterateBuiltins(v);
    v->Synchronize(VisitorSynchronization::kBuiltins);
  }

  // Iterate over global handles.
  switch (mode) {
    case VISIT_FOR_SERIALIZATION:
      // Global handles are not iterated by the serializer. Values referenced by
      // global handles need to be added manually.
      break;
    case VISIT_ONLY_STRONG:
    case VISIT_ONLY_STRONG_IGNORE_STACK:
      isolate_->global_handles()->IterateStrongRoots(v);
      break;
    case VISIT_ALL_IN_SCAVENGE:
    case VISIT_ALL_IN_MINOR_MC_MARK:
      isolate_->global_handles()->IterateYoungStrongAndDependentRoots(v);
      break;
    case VISIT_ALL_IN_MINOR_MC_UPDATE:
      isolate_->global_handles()->IterateAllYoungRoots(v);
      break;
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
      isolate_->eternal_handles()->IterateYoungRoots(v);
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

  // Iterate over pending Microtasks stored in MicrotaskQueues.
  MicrotaskQueue* default_microtask_queue = isolate_->default_microtask_queue();
  if (default_microtask_queue) {
    MicrotaskQueue* microtask_queue = default_microtask_queue;
    do {
      microtask_queue->IterateMicrotasks(v);
      microtask_queue = microtask_queue->next();
    } while (microtask_queue != default_microtask_queue);
  }

  // Iterate over the partial snapshot cache unless serializing or
  // deserializing.
  if (mode != VISIT_FOR_SERIALIZATION) {
    SerializerDeserializer::Iterate(isolate_, v);
    v->Synchronize(VisitorSynchronization::kPartialSnapshotCache);
  }
}

void Heap::IterateWeakGlobalHandles(RootVisitor* v) {
  isolate_->global_handles()->IterateWeakRoots(v);
}

void Heap::IterateBuiltins(RootVisitor* v) {
  for (int i = 0; i < Builtins::builtin_count; i++) {
    v->VisitRootPointer(Root::kBuiltins, Builtins::name(i),
                        FullObjectSlot(builtin_address(i)));
  }

  // The entry table doesn't need to be updated since all builtins are embedded.
  STATIC_ASSERT(Builtins::AllBuiltinsAreIsolateIndependent());
}

namespace {
size_t GlobalMemorySizeFromV8Size(size_t v8_size) {
  const size_t kGlobalMemoryToV8Ratio = 2;
  return Min(static_cast<uint64_t>(std::numeric_limits<size_t>::max()),
             static_cast<uint64_t>(v8_size) * kGlobalMemoryToV8Ratio);
}
}  // anonymous namespace

void Heap::ConfigureHeap(const v8::ResourceConstraints& constraints) {
  // Initialize max_semi_space_size_.
  {
    max_semi_space_size_ = 8 * (kSystemPointerSize / 4) * MB;
    if (constraints.max_young_generation_size_in_bytes() > 0) {
      max_semi_space_size_ = SemiSpaceSizeFromYoungGenerationSize(
          constraints.max_young_generation_size_in_bytes());
    }
    if (FLAG_max_semi_space_size > 0) {
      max_semi_space_size_ = static_cast<size_t>(FLAG_max_semi_space_size) * MB;
    } else if (FLAG_max_heap_size > 0) {
      size_t max_heap_size = static_cast<size_t>(FLAG_max_heap_size) * MB;
      size_t young_generation_size, old_generation_size;
      if (FLAG_max_old_space_size > 0) {
        old_generation_size = static_cast<size_t>(FLAG_max_old_space_size) * MB;
        young_generation_size = max_heap_size > old_generation_size
                                    ? max_heap_size - old_generation_size
                                    : 0;
      } else {
        GenerationSizesFromHeapSize(max_heap_size, &young_generation_size,
                                    &old_generation_size);
      }
      max_semi_space_size_ =
          SemiSpaceSizeFromYoungGenerationSize(young_generation_size);
    }
    if (FLAG_stress_compaction) {
      // This will cause more frequent GCs when stressing.
      max_semi_space_size_ = MB;
    }
    // TODO(dinfuehr): Rounding to a power of 2 is not longer needed. Remove it.
    max_semi_space_size_ =
        static_cast<size_t>(base::bits::RoundUpToPowerOfTwo64(
            static_cast<uint64_t>(max_semi_space_size_)));
    max_semi_space_size_ = Max(max_semi_space_size_, kMinSemiSpaceSize);
    max_semi_space_size_ = RoundDown<Page::kPageSize>(max_semi_space_size_);
  }

  // Initialize max_old_generation_size_ and max_global_memory_.
  {
    max_old_generation_size_ = 700ul * (kSystemPointerSize / 4) * MB;
    if (constraints.max_old_generation_size_in_bytes() > 0) {
      max_old_generation_size_ = constraints.max_old_generation_size_in_bytes();
    }
    if (FLAG_max_old_space_size > 0) {
      max_old_generation_size_ =
          static_cast<size_t>(FLAG_max_old_space_size) * MB;
    } else if (FLAG_max_heap_size > 0) {
      size_t max_heap_size = static_cast<size_t>(FLAG_max_heap_size) * MB;
      size_t young_generation_size =
          YoungGenerationSizeFromSemiSpaceSize(max_semi_space_size_);
      max_old_generation_size_ = max_heap_size > young_generation_size
                                     ? max_heap_size - young_generation_size
                                     : 0;
    }
    max_old_generation_size_ =
        Max(max_old_generation_size_, MinOldGenerationSize());
    max_old_generation_size_ =
        Min(max_old_generation_size_, AllocatorLimitOnMaxOldGenerationSize());
    max_old_generation_size_ =
        RoundDown<Page::kPageSize>(max_old_generation_size_);

    max_global_memory_size_ =
        GlobalMemorySizeFromV8Size(max_old_generation_size_);
  }

  CHECK_IMPLIES(FLAG_max_heap_size > 0,
                FLAG_max_semi_space_size == 0 || FLAG_max_old_space_size == 0);

  // Initialize initial_semispace_size_.
  {
    initial_semispace_size_ = kMinSemiSpaceSize;
    if (max_semi_space_size_ == kMaxSemiSpaceSize) {
      // Start with at least 1*MB semi-space on machines with a lot of memory.
      initial_semispace_size_ =
          Max(initial_semispace_size_, static_cast<size_t>(1 * MB));
    }
    if (constraints.initial_young_generation_size_in_bytes() > 0) {
      initial_semispace_size_ = SemiSpaceSizeFromYoungGenerationSize(
          constraints.initial_young_generation_size_in_bytes());
    }
    if (FLAG_initial_heap_size > 0) {
      size_t young_generation, old_generation;
      Heap::GenerationSizesFromHeapSize(
          static_cast<size_t>(FLAG_initial_heap_size) * MB, &young_generation,
          &old_generation);
      initial_semispace_size_ =
          SemiSpaceSizeFromYoungGenerationSize(young_generation);
    }
    if (FLAG_min_semi_space_size > 0) {
      initial_semispace_size_ =
          static_cast<size_t>(FLAG_min_semi_space_size) * MB;
    }
    initial_semispace_size_ =
        Min(initial_semispace_size_, max_semi_space_size_);
    initial_semispace_size_ =
        RoundDown<Page::kPageSize>(initial_semispace_size_);
  }

  // Initialize initial_old_space_size_.
  {
    initial_old_generation_size_ = kMaxInitialOldGenerationSize;
    if (constraints.initial_old_generation_size_in_bytes() > 0) {
      initial_old_generation_size_ =
          constraints.initial_old_generation_size_in_bytes();
      old_generation_size_configured_ = true;
    }
    if (FLAG_initial_heap_size > 0) {
      size_t initial_heap_size =
          static_cast<size_t>(FLAG_initial_heap_size) * MB;
      size_t young_generation_size =
          YoungGenerationSizeFromSemiSpaceSize(initial_semispace_size_);
      initial_old_generation_size_ =
          initial_heap_size > young_generation_size
              ? initial_heap_size - young_generation_size
              : 0;
      old_generation_size_configured_ = true;
    }
    if (FLAG_initial_old_space_size > 0) {
      initial_old_generation_size_ =
          static_cast<size_t>(FLAG_initial_old_space_size) * MB;
      old_generation_size_configured_ = true;
    }
    initial_old_generation_size_ =
        Min(initial_old_generation_size_, max_old_generation_size_ / 2);
    initial_old_generation_size_ =
        RoundDown<Page::kPageSize>(initial_old_generation_size_);
  }

  if (old_generation_size_configured_) {
    // If the embedder pre-configures the initial old generation size,
    // then allow V8 to skip full GCs below that threshold.
    min_old_generation_size_ = initial_old_generation_size_;
    min_global_memory_size_ =
        GlobalMemorySizeFromV8Size(min_old_generation_size_);
  }

  if (FLAG_semi_space_growth_factor < 2) {
    FLAG_semi_space_growth_factor = 2;
  }

  old_generation_allocation_limit_ = initial_old_generation_size_;
  global_allocation_limit_ =
      GlobalMemorySizeFromV8Size(old_generation_allocation_limit_);
  initial_max_old_generation_size_ = max_old_generation_size_;

  // We rely on being able to allocate new arrays in paged spaces.
  DCHECK(kMaxRegularHeapObjectSize >=
         (JSArray::kHeaderSize +
          FixedArray::SizeFor(JSArray::kInitialMaxFastElementArray) +
          AllocationMemento::kSize));

  code_range_size_ = constraints.code_range_size_in_bytes();

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

void Heap::ConfigureHeapDefault() {
  v8::ResourceConstraints constraints;
  ConfigureHeap(constraints);
}

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
  *stats->code_lo_space_size = code_lo_space_->Size();
  isolate_->global_handles()->RecordStats(stats);
  *stats->memory_allocator_size = memory_allocator()->Size();
  *stats->memory_allocator_capacity =
      memory_allocator()->Size() + memory_allocator()->Available();
  *stats->os_error = base::OS::GetLastError();
  *stats->malloced_memory = isolate_->allocator()->GetCurrentMemoryUsage();
  *stats->malloced_peak_memory = isolate_->allocator()->GetMaxMemoryUsage();
  if (take_snapshot) {
    HeapObjectIterator iterator(this);
    for (HeapObject obj = iterator.Next(); !obj.is_null();
         obj = iterator.Next()) {
      InstanceType type = obj.map().instance_type();
      DCHECK(0 <= type && type <= LAST_TYPE);
      stats->objects_per_type[type]++;
      stats->size_per_type[type] += obj.Size();
    }
  }
  if (stats->last_few_messages != nullptr)
    GetFromRingBuffer(stats->last_few_messages);
}

size_t Heap::OldGenerationSizeOfObjects() {
  PagedSpaceIterator spaces(this);
  size_t total = 0;
  for (PagedSpace* space = spaces.Next(); space != nullptr;
       space = spaces.Next()) {
    total += space->SizeOfObjects();
  }
  return total + lo_space_->SizeOfObjects();
}

size_t Heap::GlobalSizeOfObjects() {
  const size_t on_heap_size = OldGenerationSizeOfObjects();
  const size_t embedder_size = local_embedder_heap_tracer()
                                   ? local_embedder_heap_tracer()->used_size()
                                   : 0;
  return on_heap_size + embedder_size;
}

uint64_t Heap::PromotedExternalMemorySize() {
  IsolateData* isolate_data = isolate()->isolate_data();
  if (isolate_data->external_memory_ <=
      isolate_data->external_memory_low_since_mark_compact_) {
    return 0;
  }
  return static_cast<uint64_t>(
      isolate_data->external_memory_ -
      isolate_data->external_memory_low_since_mark_compact_);
}

bool Heap::AllocationLimitOvershotByLargeMargin() {
  // This guards against too eager finalization in small heaps.
  // The number is chosen based on v8.browsing_mobile on Nexus 7v2.
  constexpr size_t kMarginForSmallHeaps = 32u * MB;

  const size_t v8_overshoot =
      old_generation_allocation_limit_ <
              OldGenerationObjectsAndPromotedExternalMemorySize()
          ? OldGenerationObjectsAndPromotedExternalMemorySize() -
                old_generation_allocation_limit_
          : 0;
  const size_t global_overshoot =
      global_allocation_limit_ < GlobalSizeOfObjects()
          ? GlobalSizeOfObjects() - global_allocation_limit_
          : 0;

  // Bail out if the V8 and global sizes are still below their respective
  // limits.
  if (v8_overshoot == 0 && global_overshoot == 0) {
    return false;
  }

  // Overshoot margin is 50% of allocation limit or half-way to the max heap
  // with special handling of small heaps.
  const size_t v8_margin =
      Min(Max(old_generation_allocation_limit_ / 2, kMarginForSmallHeaps),
          (max_old_generation_size_ - old_generation_allocation_limit_) / 2);
  const size_t global_margin =
      Min(Max(global_allocation_limit_ / 2, kMarginForSmallHeaps),
          (max_global_memory_size_ - global_allocation_limit_) / 2);

  return v8_overshoot >= v8_margin || global_overshoot >= global_margin;
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

size_t Heap::GlobalMemoryAvailable() {
  return UseGlobalMemoryScheduling()
             ? GlobalSizeOfObjects() < global_allocation_limit_
                   ? global_allocation_limit_ - GlobalSizeOfObjects()
                   : 0
             : new_space_->Capacity() + 1;
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
  if (incremental_marking()->IsBelowActivationThresholds()) {
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
        (isolate()->isolate_data()->external_memory_ -
         isolate()->isolate_data()->external_memory_low_since_mark_compact_);
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
  const size_t global_memory_available = GlobalMemoryAvailable();

  if (old_generation_space_available > new_space_->Capacity() &&
      (global_memory_available > new_space_->Capacity())) {
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
  if (global_memory_available == 0) {
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
  PagedSpaceIterator spaces(this);
  CodeSpaceMemoryModificationScope modification_scope(this);
  for (PagedSpace* space = spaces.Next(); space != nullptr;
       space = spaces.Next()) {
    space->FreeLinearAllocationArea();
  }
}

HeapObject Heap::EnsureImmovableCode(HeapObject heap_object, int object_size) {
  // Code objects which should stay at a fixed address are allocated either
  // in the first page of code space, in large object space, or (during
  // snapshot creation) the containing page is marked as immovable.
  DCHECK(!heap_object.is_null());
#ifndef V8_ENABLE_THIRD_PARTY_HEAP
  DCHECK(code_space_->Contains(heap_object));
#endif
  DCHECK_GE(object_size, 0);
  if (!Heap::IsImmovable(heap_object)) {
    if (isolate()->serializer_enabled() ||
        code_space_->first_page()->Contains(heap_object.address())) {
      MemoryChunk::FromHeapObject(heap_object)->MarkNeverEvacuate();
    } else {
      // Discard the first code allocation, which was on a page where it could
      // be moved.
      CreateFillerObjectAt(heap_object.address(), object_size,
                           ClearRecordedSlots::kNo);
      heap_object = AllocateRawCodeInLargeObjectSpace(object_size);
      UnprotectAndRegisterMemoryChunk(heap_object);
      ZapCodeObject(heap_object.address(), object_size);
      OnAllocationEvent(heap_object, object_size);
    }
  }
  return heap_object;
}

HeapObject Heap::AllocateRawWithLightRetrySlowPath(
    int size, AllocationType allocation, AllocationOrigin origin,
    AllocationAlignment alignment) {
  HeapObject result;
  AllocationResult alloc = AllocateRaw(size, allocation, origin, alignment);
  if (alloc.To(&result)) {
    DCHECK(result != ReadOnlyRoots(this).exception());
    return result;
  }
  // Two GCs before panicking. In newspace will almost always succeed.
  for (int i = 0; i < 2; i++) {
    CollectGarbage(alloc.RetrySpace(),
                   GarbageCollectionReason::kAllocationFailure);
    alloc = AllocateRaw(size, allocation, origin, alignment);
    if (alloc.To(&result)) {
      DCHECK(result != ReadOnlyRoots(this).exception());
      return result;
    }
  }
  return HeapObject();
}

HeapObject Heap::AllocateRawWithRetryOrFailSlowPath(
    int size, AllocationType allocation, AllocationOrigin origin,
    AllocationAlignment alignment) {
  AllocationResult alloc;
  HeapObject result =
      AllocateRawWithLightRetrySlowPath(size, allocation, origin, alignment);
  if (!result.is_null()) return result;

  isolate()->counters()->gc_last_resort_from_handles()->Increment();
  CollectAllAvailableGarbage(GarbageCollectionReason::kLastResort);
  {
    AlwaysAllocateScope scope(this);
    alloc = AllocateRaw(size, allocation, origin, alignment);
  }
  if (alloc.To(&result)) {
    DCHECK(result != ReadOnlyRoots(this).exception());
    return result;
  }
  // TODO(1181417): Fix this.
  FatalProcessOutOfMemory("CALL_AND_RETRY_LAST");
  return HeapObject();
}

// TODO(jkummerow): Refactor this. AllocateRaw should take an "immovability"
// parameter and just do what's necessary.
HeapObject Heap::AllocateRawCodeInLargeObjectSpace(int size) {
  AllocationResult alloc = code_lo_space()->AllocateRaw(size);
  HeapObject result;
  if (alloc.To(&result)) {
    DCHECK(result != ReadOnlyRoots(this).exception());
    return result;
  }
  // Two GCs before panicking.
  for (int i = 0; i < 2; i++) {
    CollectGarbage(alloc.RetrySpace(),
                   GarbageCollectionReason::kAllocationFailure);
    alloc = code_lo_space()->AllocateRaw(size);
    if (alloc.To(&result)) {
      DCHECK(result != ReadOnlyRoots(this).exception());
      return result;
    }
  }
  isolate()->counters()->gc_last_resort_from_handles()->Increment();
  CollectAllAvailableGarbage(GarbageCollectionReason::kLastResort);
  {
    AlwaysAllocateScope scope(this);
    alloc = code_lo_space()->AllocateRaw(size);
  }
  if (alloc.To(&result)) {
    DCHECK(result != ReadOnlyRoots(this).exception());
    return result;
  }
  // TODO(1181417): Fix this.
  FatalProcessOutOfMemory("CALL_AND_RETRY_LAST");
  return HeapObject();
}

void Heap::SetUp() {
#ifdef V8_ENABLE_ALLOCATION_TIMEOUT
  allocation_timeout_ = NextAllocationTimeout();
#endif

#ifdef V8_ENABLE_THIRD_PARTY_HEAP
  tp_heap_ = third_party_heap::Heap::New(isolate());
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
  memory_allocator_.reset(
      new MemoryAllocator(isolate_, MaxReserved(), code_range_size_));

  mark_compact_collector_.reset(new MarkCompactCollector(this));

  scavenger_collector_.reset(new ScavengerCollector(this));

  incremental_marking_.reset(
      new IncrementalMarking(this, mark_compact_collector_->weak_objects()));

  if (FLAG_concurrent_marking || FLAG_parallel_marking) {
    concurrent_marking_.reset(new ConcurrentMarking(
        this, mark_compact_collector_->marking_worklists_holder(),
        mark_compact_collector_->weak_objects()));
  } else {
    concurrent_marking_.reset(new ConcurrentMarking(this, nullptr, nullptr));
  }

  for (int i = FIRST_SPACE; i <= LAST_SPACE; i++) {
    space_[i] = nullptr;
  }
}

void Heap::SetUpFromReadOnlyHeap(ReadOnlyHeap* ro_heap) {
  DCHECK_NOT_NULL(ro_heap);
  DCHECK_IMPLIES(read_only_space_ != nullptr,
                 read_only_space_ == ro_heap->read_only_space());
  space_[RO_SPACE] = read_only_space_ = ro_heap->read_only_space();
}

void Heap::SetUpSpaces() {
  // Ensure SetUpFromReadOnlySpace has been ran.
  DCHECK_NOT_NULL(read_only_space_);
  space_[NEW_SPACE] = new_space_ =
      new NewSpace(this, memory_allocator_->data_page_allocator(),
                   initial_semispace_size_, max_semi_space_size_);
  space_[OLD_SPACE] = old_space_ = new OldSpace(this);
  space_[CODE_SPACE] = code_space_ = new CodeSpace(this);
  space_[MAP_SPACE] = map_space_ = new MapSpace(this);
  space_[LO_SPACE] = lo_space_ = new OldLargeObjectSpace(this);
  space_[NEW_LO_SPACE] = new_lo_space_ =
      new NewLargeObjectSpace(this, new_space_->Capacity());
  space_[CODE_LO_SPACE] = code_lo_space_ = new CodeLargeObjectSpace(this);

  for (int i = 0; i < static_cast<int>(v8::Isolate::kUseCounterFeatureCount);
       i++) {
    deferred_counters_[i] = 0;
  }

  tracer_.reset(new GCTracer(this));
#ifdef ENABLE_MINOR_MC
  minor_mark_compact_collector_ = new MinorMarkCompactCollector(this);
#else
  minor_mark_compact_collector_ = nullptr;
#endif  // ENABLE_MINOR_MC
  array_buffer_collector_.reset(new ArrayBufferCollector(this));
  array_buffer_sweeper_.reset(new ArrayBufferSweeper(this));
  gc_idle_time_handler_.reset(new GCIdleTimeHandler());
  memory_measurement_.reset(new MemoryMeasurement(isolate()));
  memory_reducer_.reset(new MemoryReducer(this));
  if (V8_UNLIKELY(TracingFlags::is_gc_stats_enabled())) {
    live_object_stats_.reset(new ObjectStats(this));
    dead_object_stats_.reset(new ObjectStats(this));
  }
  local_embedder_heap_tracer_.reset(new LocalEmbedderHeapTracer(isolate()));

  LOG(isolate_, IntPtrTEvent("heap-capacity", Capacity()));
  LOG(isolate_, IntPtrTEvent("heap-available", Available()));

  mark_compact_collector()->SetUp();
#ifdef ENABLE_MINOR_MC
  if (minor_mark_compact_collector() != nullptr) {
    minor_mark_compact_collector()->SetUp();
  }
#endif  // ENABLE_MINOR_MC

  scavenge_job_.reset(new ScavengeJob());
  scavenge_task_observer_.reset(new ScavengeTaskObserver(
      this, ScavengeJob::YoungGenerationTaskTriggerSize(this)));
  new_space()->AddAllocationObserver(scavenge_task_observer_.get());

  SetGetExternallyAllocatedMemoryInBytesCallback(
      DefaultGetExternallyAllocatedMemoryInBytesCallback);

  if (FLAG_stress_marking > 0) {
    stress_marking_percentage_ = NextStressMarkingLimit();
    stress_marking_observer_ = new StressMarkingObserver(this);
    AddAllocationObserversToAllSpaces(stress_marking_observer_,
                                      stress_marking_observer_);
  }
  if (FLAG_stress_scavenge > 0) {
    stress_scavenge_observer_ = new StressScavengeObserver(this);
    new_space()->AddAllocationObserver(stress_scavenge_observer_);
  }

  write_protect_code_memory_ = FLAG_write_protect_code_memory;
}

void Heap::InitializeHashSeed() {
  DCHECK(!deserialization_complete_);
  uint64_t new_hash_seed;
  if (FLAG_hash_seed == 0) {
    int64_t rnd = isolate()->random_number_generator()->NextInt64();
    new_hash_seed = static_cast<uint64_t>(rnd);
  } else {
    new_hash_seed = static_cast<uint64_t>(FLAG_hash_seed);
  }
  ReadOnlyRoots(this).hash_seed().copy_in(
      0, reinterpret_cast<byte*>(&new_hash_seed), kInt64Size);
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
  PagedSpaceIterator spaces(this);
  for (PagedSpace* s = spaces.Next(); s != nullptr; s = spaces.Next()) {
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

void Heap::NotifyBootstrapComplete() {
  // This function is invoked for each native context creation. We are
  // interested only in the first native context.
  if (old_generation_capacity_after_bootstrap_ == 0) {
    old_generation_capacity_after_bootstrap_ = OldGenerationCapacity();
  }
}

void Heap::NotifyOldGenerationExpansion() {
  const size_t kMemoryReducerActivationThreshold = 1 * MB;
  if (old_generation_capacity_after_bootstrap_ && ms_count_ == 0 &&
      OldGenerationCapacity() >= old_generation_capacity_after_bootstrap_ +
                                     kMemoryReducerActivationThreshold &&
      FLAG_memory_reducer_for_small_heaps) {
    MemoryReducer::Event event;
    event.type = MemoryReducer::kPossibleGarbage;
    event.time_ms = MonotonicallyIncreasingTimeInMs();
    memory_reducer()->NotifyPossibleGarbage(event);
  }
}

void Heap::NotifyOffThreadSpaceMerged() {
  // TODO(leszeks): Ideally we would do this check during off-thread page
  // allocation too, to proactively do GC. We should also probably do this check
  // before merging rather than after.
  if (!CanExpandOldGeneration(0)) {
    // TODO(leszeks): We should try to invoke the near-heap limit callback and
    // do a last-resort GC first.
    FatalProcessOutOfMemory("Failed to merge off-thread pages into heap.");
  }
  StartIncrementalMarkingIfAllocationLimitIsReached(
      GCFlagsForIncrementalMarking(), kGCCallbackScheduleIdleGarbageCollection);
  NotifyOldGenerationExpansion();
}

void Heap::SetEmbedderHeapTracer(EmbedderHeapTracer* tracer) {
  DCHECK_EQ(gc_state_, HeapState::NOT_IN_GC);
  local_embedder_heap_tracer()->SetRemoteTracer(tracer);
}

EmbedderHeapTracer* Heap::GetEmbedderHeapTracer() const {
  return local_embedder_heap_tracer()->remote_tracer();
}

EmbedderHeapTracer::TraceFlags Heap::flags_for_embedder_tracer() const {
  if (ShouldReduceMemory())
    return EmbedderHeapTracer::TraceFlags::kReduceMemory;
  return EmbedderHeapTracer::TraceFlags::kNoFlags;
}

void Heap::RegisterExternallyReferencedObject(Address* location) {
  GlobalHandles::MarkTraced(location);
  Object object(*location);
  if (!object.IsHeapObject()) {
    // The embedder is not aware of whether numbers are materialized as heap
    // objects are just passed around as Smis.
    return;
  }
  HeapObject heap_object = HeapObject::cast(object);
  DCHECK(IsValidHeapObject(this, heap_object));
  if (FLAG_incremental_marking_wrappers && incremental_marking()->IsMarking()) {
    incremental_marking()->WhiteToGreyAndPush(heap_object);
  } else {
    DCHECK(mark_compact_collector()->in_use());
    mark_compact_collector()->MarkExternallyReferencedObject(heap_object);
  }
}

void Heap::StartTearDown() {
  SetGCState(TEAR_DOWN);
#ifdef VERIFY_HEAP
  // {StartTearDown} is called fairly early during Isolate teardown, so it's
  // a good time to run heap verification (if requested), before starting to
  // tear down parts of the Isolate.
  if (FLAG_verify_heap) {
    Verify();
  }
#endif
}

void Heap::TearDown() {
  DCHECK_EQ(gc_state_, TEAR_DOWN);

  // It's too late for Heap::Verify() here, as parts of the Isolate are
  // already gone by the time this is called.

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

  new_space()->RemoveAllocationObserver(scavenge_task_observer_.get());
  scavenge_task_observer_.reset();
  scavenge_job_.reset();

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

  if (mark_compact_collector_) {
    mark_compact_collector_->TearDown();
    mark_compact_collector_.reset();
  }

#ifdef ENABLE_MINOR_MC
  if (minor_mark_compact_collector_ != nullptr) {
    minor_mark_compact_collector_->TearDown();
    delete minor_mark_compact_collector_;
    minor_mark_compact_collector_ = nullptr;
  }
#endif  // ENABLE_MINOR_MC

  scavenger_collector_.reset();
  array_buffer_collector_.reset();
  array_buffer_sweeper_.reset();
  incremental_marking_.reset();
  concurrent_marking_.reset();

  gc_idle_time_handler_.reset();

  memory_measurement_.reset();

  if (memory_reducer_ != nullptr) {
    memory_reducer_->TearDown();
    memory_reducer_.reset();
  }

  live_object_stats_.reset();
  dead_object_stats_.reset();

  local_embedder_heap_tracer_.reset();

  external_string_table_.TearDown();

  // Tear down all ArrayBuffers before tearing down the heap since  their
  // byte_length may be a HeapNumber which is required for freeing the backing
  // store.
  ArrayBufferTracker::TearDown(this);

  tracer_.reset();

  isolate()->read_only_heap()->OnHeapTearDown();
  space_[RO_SPACE] = read_only_space_ = nullptr;
  for (int i = FIRST_MUTABLE_SPACE; i <= LAST_MUTABLE_SPACE; i++) {
    delete space_[i];
    space_[i] = nullptr;
  }

  memory_allocator()->TearDown();

  StrongRootsList* next = nullptr;
  for (StrongRootsList* list = strong_roots_list_; list; list = next) {
    next = list->next;
    delete list;
  }
  strong_roots_list_ = nullptr;

  memory_allocator_.reset();
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
                                           AllocationType allocation) {
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
      new_length, allocation);
  // Allocation might have caused GC and turned some of the elements into
  // cleared weak heap objects. Count the number of live references again and
  // fill in the new array.
  int copy_to = 0;
  for (int i = 0; i < array->length(); i++) {
    MaybeObject element = array->Get(i);
    if (element->IsCleared()) continue;
    new_array->Set(copy_to++, element);
  }
  new_array->set_length(copy_to);
  return new_array;
}

}  // anonymous namespace

void Heap::CompactWeakArrayLists(AllocationType allocation) {
  // Find known PrototypeUsers and compact them.
  std::vector<Handle<PrototypeInfo>> prototype_infos;
  {
    HeapObjectIterator iterator(this);
    for (HeapObject o = iterator.Next(); !o.is_null(); o = iterator.Next()) {
      if (o.IsPrototypeInfo()) {
        PrototypeInfo prototype_info = PrototypeInfo::cast(o);
        if (prototype_info.prototype_users().IsWeakArrayList()) {
          prototype_infos.emplace_back(handle(prototype_info, isolate()));
        }
      }
    }
  }
  for (auto& prototype_info : prototype_infos) {
    Handle<WeakArrayList> array(
        WeakArrayList::cast(prototype_info->prototype_users()), isolate());
    DCHECK_IMPLIES(allocation == AllocationType::kOld,
                   InOldSpace(*array) ||
                       *array == ReadOnlyRoots(this).empty_weak_array_list());
    WeakArrayList new_array = PrototypeUsers::Compact(
        array, this, JSObject::PrototypeRegistryCompactionCallback, allocation);
    prototype_info->set_prototype_users(new_array);
  }

  // Find known WeakArrayLists and compact them.
  Handle<WeakArrayList> scripts(script_list(), isolate());
  DCHECK_IMPLIES(
      !V8_ENABLE_THIRD_PARTY_HEAP_BOOL && allocation == AllocationType::kOld,
      InOldSpace(*scripts));
  scripts = CompactWeakArrayList(this, scripts, allocation);
  set_script_list(*scripts);
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

void Heap::CompactRetainedMaps(WeakArrayList retained_maps) {
  DCHECK_EQ(retained_maps, this->retained_maps());
  int length = retained_maps.length();
  int new_length = 0;
  int new_number_of_disposed_maps = 0;
  // This loop compacts the array by removing cleared weak cells.
  for (int i = 0; i < length; i += 2) {
    MaybeObject maybe_object = retained_maps.Get(i);
    if (maybe_object->IsCleared()) {
      continue;
    }

    DCHECK(maybe_object->IsWeak());

    MaybeObject age = retained_maps.Get(i + 1);
    DCHECK(age->IsSmi());
    if (i != new_length) {
      retained_maps.Set(new_length, maybe_object);
      retained_maps.Set(new_length + 1, age);
    }
    if (i < number_of_disposed_maps_) {
      new_number_of_disposed_maps += 2;
    }
    new_length += 2;
  }
  number_of_disposed_maps_ = new_number_of_disposed_maps;
  HeapObject undefined = ReadOnlyRoots(this).undefined_value();
  for (int i = new_length; i < length; i++) {
    retained_maps.Set(i, HeapObjectReference::Strong(undefined));
  }
  if (new_length != length) retained_maps.set_length(new_length);
}

void Heap::FatalProcessOutOfMemory(const char* location) {
  v8::internal::V8::FatalProcessOutOfMemory(isolate(), location, true);
}

#ifdef DEBUG

class PrintHandleVisitor : public RootVisitor {
 public:
  void VisitRootPointers(Root root, const char* description,
                         FullObjectSlot start, FullObjectSlot end) override {
    for (FullObjectSlot p = start; p < end; ++p)
      PrintF("  handle %p to %p\n", p.ToVoidPtr(),
             reinterpret_cast<void*>((*p).ptr()));
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
  void VisitRootPointers(Root root, const char* description,
                         FullObjectSlot start, FullObjectSlot end) override {
    handle_count_ += end - start;
  }

 private:
  ptrdiff_t handle_count_;
};


void Heap::CheckHandleCount() {
  CheckHandleCountVisitor v;
  isolate_->handle_scope_implementer()->Iterate(&v);
}

void Heap::ClearRecordedSlot(HeapObject object, ObjectSlot slot) {
#ifndef V8_DISABLE_WRITE_BARRIERS
  DCHECK(!IsLargeObject(object));
  Page* page = Page::FromAddress(slot.address());
  if (!page->InYoungGeneration()) {
    DCHECK_EQ(page->owner_identity(), OLD_SPACE);

    if (!page->SweepingDone()) {
      RememberedSet<OLD_TO_NEW>::Remove(page, slot.address());
    }
  }
#endif
}

// static
int Heap::InsertIntoRememberedSetFromCode(MemoryChunk* chunk, Address slot) {
  RememberedSet<OLD_TO_NEW>::Insert<AccessMode::NON_ATOMIC>(chunk, slot);
  return 0;
}

#ifdef DEBUG
void Heap::VerifyClearedSlot(HeapObject object, ObjectSlot slot) {
#ifndef V8_DISABLE_WRITE_BARRIERS
  DCHECK(!IsLargeObject(object));
  if (InYoungGeneration(object)) return;
  Page* page = Page::FromAddress(slot.address());
  DCHECK_EQ(page->owner_identity(), OLD_SPACE);
  // Slots are filtered with invalidated slots.
  CHECK_IMPLIES(RememberedSet<OLD_TO_NEW>::Contains(page, slot.address()),
                page->RegisteredObjectWithInvalidatedSlots<OLD_TO_NEW>(object));
  CHECK_IMPLIES(RememberedSet<OLD_TO_OLD>::Contains(page, slot.address()),
                page->RegisteredObjectWithInvalidatedSlots<OLD_TO_OLD>(object));
#endif
}
#endif

void Heap::ClearRecordedSlotRange(Address start, Address end) {
#ifndef V8_DISABLE_WRITE_BARRIERS
  Page* page = Page::FromAddress(start);
  DCHECK(!page->IsLargePage());
  if (!page->InYoungGeneration()) {
    DCHECK_EQ(page->owner_identity(), OLD_SPACE);

    if (!page->SweepingDone()) {
      RememberedSet<OLD_TO_NEW>::RemoveRange(page, start, end,
                                             SlotSet::KEEP_EMPTY_BUCKETS);
    }
  }
#endif
}

PagedSpace* PagedSpaceIterator::Next() {
  switch (counter_++) {
    case RO_SPACE:
    case NEW_SPACE:
      UNREACHABLE();
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
    : heap_(heap), current_space_(FIRST_MUTABLE_SPACE - 1) {}

SpaceIterator::~SpaceIterator() = default;

bool SpaceIterator::HasNext() {
  // Iterate until no more spaces.
  return current_space_ != LAST_SPACE;
}

Space* SpaceIterator::Next() {
  DCHECK(HasNext());
  return heap_->space(++current_space_);
}

class HeapObjectsFilter {
 public:
  virtual ~HeapObjectsFilter() = default;
  virtual bool SkipObject(HeapObject object) = 0;
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

  bool SkipObject(HeapObject object) override {
    if (object.IsFreeSpaceOrFiller()) return true;
    MemoryChunk* chunk = MemoryChunk::FromHeapObject(object);
    if (reachable_.count(chunk) == 0) return true;
    return reachable_[chunk]->count(object) == 0;
  }

 private:
  bool MarkAsReachable(HeapObject object) {
    MemoryChunk* chunk = MemoryChunk::FromHeapObject(object);
    if (reachable_.count(chunk) == 0) {
      reachable_[chunk] = new std::unordered_set<HeapObject, Object::Hasher>();
    }
    if (reachable_[chunk]->count(object)) return false;
    reachable_[chunk]->insert(object);
    return true;
  }

  class MarkingVisitor : public ObjectVisitor, public RootVisitor {
   public:
    explicit MarkingVisitor(UnreachableObjectsFilter* filter)
        : filter_(filter) {}

    void VisitPointers(HeapObject host, ObjectSlot start,
                       ObjectSlot end) override {
      MarkPointers(MaybeObjectSlot(start), MaybeObjectSlot(end));
    }

    void VisitPointers(HeapObject host, MaybeObjectSlot start,
                       MaybeObjectSlot end) final {
      MarkPointers(start, end);
    }

    void VisitCodeTarget(Code host, RelocInfo* rinfo) final {
      Code target = Code::GetCodeFromTargetAddress(rinfo->target_address());
      MarkHeapObject(target);
    }
    void VisitEmbeddedPointer(Code host, RelocInfo* rinfo) final {
      MarkHeapObject(rinfo->target_object());
    }

    void VisitRootPointers(Root root, const char* description,
                           FullObjectSlot start, FullObjectSlot end) override {
      MarkPointersImpl(start, end);
    }

    void TransitiveClosure() {
      while (!marking_stack_.empty()) {
        HeapObject obj = marking_stack_.back();
        marking_stack_.pop_back();
        obj.Iterate(this);
      }
    }

   private:
    void MarkPointers(MaybeObjectSlot start, MaybeObjectSlot end) {
      MarkPointersImpl(start, end);
    }

    template <typename TSlot>
    V8_INLINE void MarkPointersImpl(TSlot start, TSlot end) {
      // Treat weak references as strong.
      for (TSlot p = start; p < end; ++p) {
        typename TSlot::TObject object = *p;
        HeapObject heap_object;
        if (object.GetHeapObject(&heap_object)) {
          MarkHeapObject(heap_object);
        }
      }
    }

    V8_INLINE void MarkHeapObject(HeapObject heap_object) {
      if (filter_->MarkAsReachable(heap_object)) {
        marking_stack_.push_back(heap_object);
      }
    }

    UnreachableObjectsFilter* filter_;
    std::vector<HeapObject> marking_stack_;
  };

  friend class MarkingVisitor;

  void MarkReachableObjects() {
    MarkingVisitor visitor(this);
    heap_->IterateRoots(&visitor, VISIT_ALL);
    visitor.TransitiveClosure();
  }

  Heap* heap_;
  DisallowHeapAllocation no_allocation_;
  std::unordered_map<MemoryChunk*,
                     std::unordered_set<HeapObject, Object::Hasher>*>
      reachable_;
};

HeapObjectIterator::HeapObjectIterator(
    Heap* heap, HeapObjectIterator::HeapObjectsFiltering filtering)
    : heap_(heap),
      filtering_(filtering),
      filter_(nullptr),
      space_iterator_(nullptr),
      object_iterator_(nullptr) {
  heap_->MakeHeapIterable();
  // Start the iteration.
  space_iterator_ = new SpaceIterator(heap_);
  switch (filtering_) {
    case kFilterUnreachable:
      filter_ = new UnreachableObjectsFilter(heap_);
      break;
    default:
      break;
  }
  object_iterator_ = space_iterator_->Next()->GetObjectIterator(heap_);
}

HeapObjectIterator::~HeapObjectIterator() {
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

HeapObject HeapObjectIterator::Next() {
  if (filter_ == nullptr) return NextObject();

  HeapObject obj = NextObject();
  while (!obj.is_null() && (filter_->SkipObject(obj))) obj = NextObject();
  return obj;
}

HeapObject HeapObjectIterator::NextObject() {
  // No iterator means we are done.
  if (object_iterator_.get() == nullptr) return HeapObject();

  HeapObject obj = object_iterator_.get()->Next();
  if (!obj.is_null()) {
    // If the current iterator has more objects we are fine.
    return obj;
  } else {
    // Go though the spaces looking for one that has objects.
    while (space_iterator_->HasNext()) {
      object_iterator_ = space_iterator_->Next()->GetObjectIterator(heap_);
      obj = object_iterator_.get()->Next();
      if (!obj.is_null()) {
        return obj;
      }
    }
  }
  // Done with the last space.
  object_iterator_.reset(nullptr);
  return HeapObject();
}

void Heap::UpdateTotalGCTime(double duration) {
  if (FLAG_trace_gc_verbose) {
    total_gc_time_ms_ += duration;
  }
}

void Heap::ExternalStringTable::CleanUpYoung() {
  int last = 0;
  Isolate* isolate = heap_->isolate();
  for (size_t i = 0; i < young_strings_.size(); ++i) {
    Object o = young_strings_[i];
    if (o.IsTheHole(isolate)) {
      continue;
    }
    // The real external string is already in one of these vectors and was or
    // will be processed. Re-processing it will add a duplicate to the vector.
    if (o.IsThinString()) continue;
    DCHECK(o.IsExternalString());
    if (InYoungGeneration(o)) {
      young_strings_[last++] = o;
    } else {
      old_strings_.push_back(o);
    }
  }
  young_strings_.resize(last);
}

void Heap::ExternalStringTable::CleanUpAll() {
  CleanUpYoung();
  int last = 0;
  Isolate* isolate = heap_->isolate();
  for (size_t i = 0; i < old_strings_.size(); ++i) {
    Object o = old_strings_[i];
    if (o.IsTheHole(isolate)) {
      continue;
    }
    // The real external string is already in one of these vectors and was or
    // will be processed. Re-processing it will add a duplicate to the vector.
    if (o.IsThinString()) continue;
    DCHECK(o.IsExternalString());
    DCHECK(!InYoungGeneration(o));
    old_strings_[last++] = o;
  }
  old_strings_.resize(last);
#ifdef VERIFY_HEAP
  if (FLAG_verify_heap) {
    Verify();
  }
#endif
}

void Heap::ExternalStringTable::TearDown() {
  for (size_t i = 0; i < young_strings_.size(); ++i) {
    Object o = young_strings_[i];
    // Dont finalize thin strings.
    if (o.IsThinString()) continue;
    heap_->FinalizeExternalString(ExternalString::cast(o));
  }
  young_strings_.clear();
  for (size_t i = 0; i < old_strings_.size(); ++i) {
    Object o = old_strings_[i];
    // Dont finalize thin strings.
    if (o.IsThinString()) continue;
    heap_->FinalizeExternalString(ExternalString::cast(o));
  }
  old_strings_.clear();
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

size_t Heap::YoungArrayBufferBytes() {
  DCHECK(V8_ARRAY_BUFFER_EXTENSION_BOOL);
  return array_buffer_sweeper()->YoungBytes();
}

size_t Heap::OldArrayBufferBytes() {
  DCHECK(V8_ARRAY_BUFFER_EXTENSION_BOOL);
  return array_buffer_sweeper()->OldBytes();
}

void Heap::RegisterStrongRoots(FullObjectSlot start, FullObjectSlot end) {
  StrongRootsList* list = new StrongRootsList();
  list->next = strong_roots_list_;
  list->start = start;
  list->end = end;
  strong_roots_list_ = list;
}

void Heap::UnregisterStrongRoots(FullObjectSlot start) {
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

void Heap::SetBuiltinsConstantsTable(FixedArray cache) {
  set_builtins_constants_table(cache);
}

void Heap::SetInterpreterEntryTrampolineForProfiling(Code code) {
  DCHECK_EQ(Builtins::kInterpreterEntryTrampoline, code.builtin_index());
  set_interpreter_entry_trampoline_for_profiling(code);
}

void Heap::PostFinalizationRegistryCleanupTaskIfNeeded() {
  DCHECK(!isolate()->host_cleanup_finalization_group_callback());
  // Only one cleanup task is posted at a time.
  if (!HasDirtyJSFinalizationRegistries() ||
      is_finalization_registry_cleanup_task_posted_) {
    return;
  }
  auto taskrunner = V8::GetCurrentPlatform()->GetForegroundTaskRunner(
      reinterpret_cast<v8::Isolate*>(isolate()));
  auto task = std::make_unique<FinalizationRegistryCleanupTask>(this);
  taskrunner->PostNonNestableTask(std::move(task));
  is_finalization_registry_cleanup_task_posted_ = true;
}

void Heap::EnqueueDirtyJSFinalizationRegistry(
    JSFinalizationRegistry finalization_registry,
    std::function<void(HeapObject object, ObjectSlot slot, Object target)>
        gc_notify_updated_slot) {
  // Add a FinalizationRegistry to the tail of the dirty list.
  DCHECK(!HasDirtyJSFinalizationRegistries() ||
         dirty_js_finalization_registries_list().IsJSFinalizationRegistry());
  DCHECK(finalization_registry.next_dirty().IsUndefined(isolate()));
  DCHECK(!finalization_registry.scheduled_for_cleanup());
  finalization_registry.set_scheduled_for_cleanup(true);
  if (dirty_js_finalization_registries_list_tail().IsUndefined(isolate())) {
    DCHECK(dirty_js_finalization_registries_list().IsUndefined(isolate()));
    set_dirty_js_finalization_registries_list(finalization_registry);
    // dirty_js_finalization_registries_list_ is rescanned by
    // ProcessWeakListRoots.
  } else {
    JSFinalizationRegistry tail = JSFinalizationRegistry::cast(
        dirty_js_finalization_registries_list_tail());
    tail.set_next_dirty(finalization_registry);
    gc_notify_updated_slot(tail,
                           finalization_registry.RawField(
                               JSFinalizationRegistry::kNextDirtyOffset),
                           finalization_registry);
  }
  set_dirty_js_finalization_registries_list_tail(finalization_registry);
  // dirty_js_finalization_registries_list_tail_ is rescanned by
  // ProcessWeakListRoots.
}

MaybeHandle<JSFinalizationRegistry> Heap::DequeueDirtyJSFinalizationRegistry() {
  // Take a FinalizationRegistry from the head of the dirty list for fairness.
  if (HasDirtyJSFinalizationRegistries()) {
    Handle<JSFinalizationRegistry> head(
        JSFinalizationRegistry::cast(dirty_js_finalization_registries_list()),
        isolate());
    set_dirty_js_finalization_registries_list(head->next_dirty());
    head->set_next_dirty(ReadOnlyRoots(this).undefined_value());
    if (*head == dirty_js_finalization_registries_list_tail()) {
      set_dirty_js_finalization_registries_list_tail(
          ReadOnlyRoots(this).undefined_value());
    }
    return head;
  }
  return {};
}

void Heap::RemoveDirtyFinalizationRegistriesOnContext(NativeContext context) {
  if (!FLAG_harmony_weak_refs) return;
  if (isolate()->host_cleanup_finalization_group_callback()) return;

  DisallowHeapAllocation no_gc;

  Isolate* isolate = this->isolate();
  Object prev = ReadOnlyRoots(isolate).undefined_value();
  Object current = dirty_js_finalization_registries_list();
  while (!current.IsUndefined(isolate)) {
    JSFinalizationRegistry finalization_registry =
        JSFinalizationRegistry::cast(current);
    if (finalization_registry.native_context() == context) {
      if (prev.IsUndefined(isolate)) {
        set_dirty_js_finalization_registries_list(
            finalization_registry.next_dirty());
      } else {
        JSFinalizationRegistry::cast(prev).set_next_dirty(
            finalization_registry.next_dirty());
      }
      finalization_registry.set_scheduled_for_cleanup(false);
      current = finalization_registry.next_dirty();
      finalization_registry.set_next_dirty(
          ReadOnlyRoots(isolate).undefined_value());
    } else {
      prev = current;
      current = finalization_registry.next_dirty();
    }
  }
  set_dirty_js_finalization_registries_list_tail(prev);
}

void Heap::KeepDuringJob(Handle<JSReceiver> target) {
  DCHECK(FLAG_harmony_weak_refs);
  DCHECK(weak_refs_keep_during_job().IsUndefined() ||
         weak_refs_keep_during_job().IsOrderedHashSet());
  Handle<OrderedHashSet> table;
  if (weak_refs_keep_during_job().IsUndefined(isolate())) {
    table = isolate()->factory()->NewOrderedHashSet();
  } else {
    table =
        handle(OrderedHashSet::cast(weak_refs_keep_during_job()), isolate());
  }
  table = OrderedHashSet::Add(isolate(), table, target).ToHandleChecked();
  set_weak_refs_keep_during_job(*table);
}

void Heap::ClearKeptObjects() {
  set_weak_refs_keep_during_job(ReadOnlyRoots(isolate()).undefined_value());
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
  Object context = native_contexts_list();
  while (!context.IsUndefined(isolate())) {
    ++result;
    Context native_context = Context::cast(context);
    context = native_context.next_context_link();
  }
  return result;
}

std::vector<Handle<NativeContext>> Heap::FindAllNativeContexts() {
  std::vector<Handle<NativeContext>> result;
  Object context = native_contexts_list();
  while (!context.IsUndefined(isolate())) {
    NativeContext native_context = NativeContext::cast(context);
    result.push_back(handle(native_context, isolate()));
    context = native_context.next_context_link();
  }
  return result;
}

size_t Heap::NumberOfDetachedContexts() {
  // The detached_contexts() array has two entries per detached context.
  return detached_contexts().length() / 2;
}

void VerifyPointersVisitor::VisitPointers(HeapObject host, ObjectSlot start,
                                          ObjectSlot end) {
  VerifyPointers(host, MaybeObjectSlot(start), MaybeObjectSlot(end));
}

void VerifyPointersVisitor::VisitPointers(HeapObject host,
                                          MaybeObjectSlot start,
                                          MaybeObjectSlot end) {
  VerifyPointers(host, start, end);
}

void VerifyPointersVisitor::VisitRootPointers(Root root,
                                              const char* description,
                                              FullObjectSlot start,
                                              FullObjectSlot end) {
  VerifyPointersImpl(start, end);
}

void VerifyPointersVisitor::VerifyHeapObjectImpl(HeapObject heap_object) {
  CHECK(IsValidHeapObject(heap_, heap_object));
  CHECK(heap_object.map().IsMap());
}

template <typename TSlot>
void VerifyPointersVisitor::VerifyPointersImpl(TSlot start, TSlot end) {
  for (TSlot slot = start; slot < end; ++slot) {
    typename TSlot::TObject object = *slot;
    HeapObject heap_object;
    if (object.GetHeapObject(&heap_object)) {
      VerifyHeapObjectImpl(heap_object);
    } else {
      CHECK(object.IsSmi() || object.IsCleared());
    }
  }
}

void VerifyPointersVisitor::VerifyPointers(HeapObject host,
                                           MaybeObjectSlot start,
                                           MaybeObjectSlot end) {
  // If this DCHECK fires then you probably added a pointer field
  // to one of objects in DATA_ONLY_VISITOR_ID_LIST. You can fix
  // this by moving that object to POINTER_VISITOR_ID_LIST.
  DCHECK_EQ(ObjectFields::kMaybePointers,
            Map::ObjectFieldsFrom(host.map().visitor_id()));
  VerifyPointersImpl(start, end);
}

void VerifyPointersVisitor::VisitCodeTarget(Code host, RelocInfo* rinfo) {
  Code target = Code::GetCodeFromTargetAddress(rinfo->target_address());
  VerifyHeapObjectImpl(target);
}

void VerifyPointersVisitor::VisitEmbeddedPointer(Code host, RelocInfo* rinfo) {
  VerifyHeapObjectImpl(rinfo->target_object());
}

void VerifySmisVisitor::VisitRootPointers(Root root, const char* description,
                                          FullObjectSlot start,
                                          FullObjectSlot end) {
  for (FullObjectSlot current = start; current < end; ++current) {
    CHECK((*current).IsSmi());
  }
}

bool Heap::AllowedToBeMigrated(Map map, HeapObject obj, AllocationSpace dst) {
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
  if (map == ReadOnlyRoots(this).one_pointer_filler_map()) return false;
  InstanceType type = map.instance_type();
  MemoryChunk* chunk = MemoryChunk::FromHeapObject(obj);
  AllocationSpace src = chunk->owner_identity();
  switch (src) {
    case NEW_SPACE:
      return dst == NEW_SPACE || dst == OLD_SPACE;
    case OLD_SPACE:
      return dst == OLD_SPACE;
    case CODE_SPACE:
      return dst == CODE_SPACE && type == CODE_TYPE;
    case MAP_SPACE:
    case LO_SPACE:
    case CODE_LO_SPACE:
    case NEW_LO_SPACE:
    case RO_SPACE:
      return false;
  }
  UNREACHABLE();
}

size_t Heap::EmbedderAllocationCounter() const {
  return local_embedder_heap_tracer()
             ? local_embedder_heap_tracer()->allocated_size()
             : 0;
}

void Heap::CreateObjectStats() {
  if (V8_LIKELY(!TracingFlags::is_gc_stats_enabled())) return;
  if (!live_object_stats_) {
    live_object_stats_.reset(new ObjectStats(this));
  }
  if (!dead_object_stats_) {
    dead_object_stats_.reset(new ObjectStats(this));
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

Map Heap::GcSafeMapOfCodeSpaceObject(HeapObject object) {
  MapWord map_word = object.map_word();
  return map_word.IsForwardingAddress() ? map_word.ToForwardingAddress().map()
                                        : map_word.ToMap();
}

Code Heap::GcSafeCastToCode(HeapObject object, Address inner_pointer) {
  Code code = Code::unchecked_cast(object);
  DCHECK(!code.is_null());
  DCHECK(GcSafeCodeContains(code, inner_pointer));
  return code;
}

bool Heap::GcSafeCodeContains(Code code, Address addr) {
  Map map = GcSafeMapOfCodeSpaceObject(code);
  DCHECK(map == ReadOnlyRoots(this).code_map());
  if (InstructionStream::TryLookupCode(isolate(), addr) == code) return true;
  Address start = code.address();
  Address end = code.address() + code.SizeFromMap(map);
  return start <= addr && addr < end;
}

Code Heap::GcSafeFindCodeForInnerPointer(Address inner_pointer) {
  Code code = InstructionStream::TryLookupCode(isolate(), inner_pointer);
  if (!code.is_null()) return code;

  if (V8_ENABLE_THIRD_PARTY_HEAP_BOOL) {
    Address start = tp_heap_->GetObjectFromInnerPointer(inner_pointer);
    return GcSafeCastToCode(HeapObject::FromAddress(start), inner_pointer);
  }

  // Check if the inner pointer points into a large object chunk.
  LargePage* large_page = code_lo_space()->FindPage(inner_pointer);
  if (large_page != nullptr) {
    return GcSafeCastToCode(large_page->GetObject(), inner_pointer);
  }

  if (V8_LIKELY(code_space()->Contains(inner_pointer))) {
    // Iterate through the page until we reach the end or find an object
    // starting after the inner pointer.
    Page* page = Page::FromAddress(inner_pointer);

    Address start =
        page->GetCodeObjectRegistry()->GetCodeObjectStartFromInnerAddress(
            inner_pointer);
    return GcSafeCastToCode(HeapObject::FromAddress(start), inner_pointer);
  }

  // It can only fall through to here during debugging, where for instance "jco"
  // was called on an address within a RO_SPACE builtin. It cannot reach here
  // during stack iteration as RO_SPACE memory is not executable so cannot
  // appear on the stack as an instruction address.
  DCHECK(ReadOnlyHeap::Contains(
      HeapObject::FromAddress(inner_pointer & ~kHeapObjectTagMask)));

  // TODO(delphick): Possibly optimize this as it iterates over all pages in
  // RO_SPACE instead of just the one containing the address.
  ReadOnlyHeapObjectIterator iterator(isolate()->read_only_heap());
  for (HeapObject object = iterator.Next(); !object.is_null();
       object = iterator.Next()) {
    if (!object.IsCode()) continue;
    Code code = Code::cast(object);
    if (inner_pointer >= code.address() &&
        inner_pointer < code.address() + code.Size()) {
      return code;
    }
  }
  UNREACHABLE();
}

void Heap::WriteBarrierForCodeSlow(Code code) {
  for (RelocIterator it(code, RelocInfo::EmbeddedObjectModeMask()); !it.done();
       it.next()) {
    GenerationalBarrierForCode(code, it.rinfo(), it.rinfo()->target_object());
    MarkingBarrierForCode(code, it.rinfo(), it.rinfo()->target_object());
  }
}

void Heap::MarkingBarrierForArrayBufferExtensionSlow(
    HeapObject object, ArrayBufferExtension* extension) {
  if (V8_CONCURRENT_MARKING_BOOL || GetIsolateFromWritableObject(object)
                                        ->heap()
                                        ->incremental_marking()
                                        ->marking_state()
                                        ->IsBlack(object))
    extension->Mark();
}

void Heap::GenerationalBarrierSlow(HeapObject object, Address slot,
                                   HeapObject value) {
  MemoryChunk* chunk = MemoryChunk::FromHeapObject(object);
  RememberedSet<OLD_TO_NEW>::Insert<AccessMode::NON_ATOMIC>(chunk, slot);
}

void Heap::RecordEphemeronKeyWrite(EphemeronHashTable table, Address slot) {
  DCHECK(ObjectInYoungGeneration(HeapObjectSlot(slot).ToHeapObject()));
  int slot_index = EphemeronHashTable::SlotToIndex(table.address(), slot);
  InternalIndex entry = EphemeronHashTable::IndexToEntry(slot_index);
  auto it =
      ephemeron_remembered_set_.insert({table, std::unordered_set<int>()});
  it.first->second.insert(entry.as_int());
}

void Heap::EphemeronKeyWriteBarrierFromCode(Address raw_object,
                                            Address key_slot_address,
                                            Isolate* isolate) {
  EphemeronHashTable table = EphemeronHashTable::cast(Object(raw_object));
  MaybeObjectSlot key_slot(key_slot_address);
  MaybeObject maybe_key = *key_slot;
  HeapObject key;
  if (!maybe_key.GetHeapObject(&key)) return;
  if (!ObjectInYoungGeneration(table) && ObjectInYoungGeneration(key)) {
    isolate->heap()->RecordEphemeronKeyWrite(table, key_slot_address);
  }
  isolate->heap()->incremental_marking()->RecordWrite(table, key_slot,
                                                      maybe_key);
}

enum RangeWriteBarrierMode {
  kDoGenerational = 1 << 0,
  kDoMarking = 1 << 1,
  kDoEvacuationSlotRecording = 1 << 2,
};

template <int kModeMask, typename TSlot>
void Heap::WriteBarrierForRangeImpl(MemoryChunk* source_page, HeapObject object,
                                    TSlot start_slot, TSlot end_slot) {
  // At least one of generational or marking write barrier should be requested.
  STATIC_ASSERT(kModeMask & (kDoGenerational | kDoMarking));
  // kDoEvacuationSlotRecording implies kDoMarking.
  STATIC_ASSERT(!(kModeMask & kDoEvacuationSlotRecording) ||
                (kModeMask & kDoMarking));

  IncrementalMarking* incremental_marking = this->incremental_marking();
  MarkCompactCollector* collector = this->mark_compact_collector();

  for (TSlot slot = start_slot; slot < end_slot; ++slot) {
    typename TSlot::TObject value = *slot;
    HeapObject value_heap_object;
    if (!value.GetHeapObject(&value_heap_object)) continue;

    if ((kModeMask & kDoGenerational) &&
        Heap::InYoungGeneration(value_heap_object)) {
      RememberedSet<OLD_TO_NEW>::Insert<AccessMode::NON_ATOMIC>(source_page,
                                                                slot.address());
    }

    if ((kModeMask & kDoMarking) &&
        incremental_marking->BaseRecordWrite(object, value_heap_object)) {
      if (kModeMask & kDoEvacuationSlotRecording) {
        collector->RecordSlot(source_page, HeapObjectSlot(slot),
                              value_heap_object);
      }
    }
  }
}

// Instantiate Heap::WriteBarrierForRange() for ObjectSlot and MaybeObjectSlot.
template void Heap::WriteBarrierForRange<ObjectSlot>(HeapObject object,
                                                     ObjectSlot start_slot,
                                                     ObjectSlot end_slot);
template void Heap::WriteBarrierForRange<MaybeObjectSlot>(
    HeapObject object, MaybeObjectSlot start_slot, MaybeObjectSlot end_slot);

template <typename TSlot>
void Heap::WriteBarrierForRange(HeapObject object, TSlot start_slot,
                                TSlot end_slot) {
  MemoryChunk* source_page = MemoryChunk::FromHeapObject(object);
  base::Flags<RangeWriteBarrierMode> mode;

  if (!source_page->InYoungGeneration()) {
    mode |= kDoGenerational;
  }

  if (incremental_marking()->IsMarking()) {
    mode |= kDoMarking;
    if (!source_page->ShouldSkipEvacuationSlotRecording<AccessMode::ATOMIC>()) {
      mode |= kDoEvacuationSlotRecording;
    }
  }

  switch (mode) {
    // Nothing to be done.
    case 0:
      return;

    // Generational only.
    case kDoGenerational:
      return WriteBarrierForRangeImpl<kDoGenerational>(source_page, object,
                                                       start_slot, end_slot);
    // Marking, no evacuation slot recording.
    case kDoMarking:
      return WriteBarrierForRangeImpl<kDoMarking>(source_page, object,
                                                  start_slot, end_slot);
    // Marking with evacuation slot recording.
    case kDoMarking | kDoEvacuationSlotRecording:
      return WriteBarrierForRangeImpl<kDoMarking | kDoEvacuationSlotRecording>(
          source_page, object, start_slot, end_slot);

    // Generational and marking, no evacuation slot recording.
    case kDoGenerational | kDoMarking:
      return WriteBarrierForRangeImpl<kDoGenerational | kDoMarking>(
          source_page, object, start_slot, end_slot);

    // Generational and marking with evacuation slot recording.
    case kDoGenerational | kDoMarking | kDoEvacuationSlotRecording:
      return WriteBarrierForRangeImpl<kDoGenerational | kDoMarking |
                                      kDoEvacuationSlotRecording>(
          source_page, object, start_slot, end_slot);

    default:
      UNREACHABLE();
  }
}

void Heap::GenerationalBarrierForCodeSlow(Code host, RelocInfo* rinfo,
                                          HeapObject object) {
  DCHECK(InYoungGeneration(object));
  Page* source_page = Page::FromHeapObject(host);
  RelocInfo::Mode rmode = rinfo->rmode();
  Address addr = rinfo->pc();
  SlotType slot_type = SlotTypeForRelocInfoMode(rmode);
  if (rinfo->IsInConstantPool()) {
    addr = rinfo->constant_pool_entry_address();
    if (RelocInfo::IsCodeTargetMode(rmode)) {
      slot_type = CODE_ENTRY_SLOT;
    } else {
      // Constant pools don't currently support compressed objects, as
      // their values are all pointer sized (though this could change
      // therefore we have a DCHECK).
      DCHECK(RelocInfo::IsFullEmbeddedObject(rmode));
      slot_type = OBJECT_SLOT;
    }
  }
  uintptr_t offset = addr - source_page->address();
  DCHECK_LT(offset, static_cast<uintptr_t>(TypedSlotSet::kMaxOffset));
  RememberedSet<OLD_TO_NEW>::InsertTyped(source_page, slot_type,
                                         static_cast<uint32_t>(offset));
}

void Heap::MarkingBarrierSlow(HeapObject object, Address slot,
                              HeapObject value) {
  Heap* heap = Heap::FromWritableHeapObject(object);
  heap->incremental_marking()->RecordWriteSlow(object, HeapObjectSlot(slot),
                                               value);
}

void Heap::MarkingBarrierForCodeSlow(Code host, RelocInfo* rinfo,
                                     HeapObject object) {
  Heap* heap = Heap::FromWritableHeapObject(host);
  DCHECK(heap->incremental_marking()->IsMarking());
  heap->incremental_marking()->RecordWriteIntoCode(host, rinfo, object);
}

void Heap::MarkingBarrierForDescriptorArraySlow(Heap* heap, HeapObject host,
                                                HeapObject raw_descriptor_array,
                                                int number_of_own_descriptors) {
  DCHECK(heap->incremental_marking()->IsMarking());
  DescriptorArray descriptor_array =
      DescriptorArray::cast(raw_descriptor_array);
  int16_t raw_marked = descriptor_array.raw_number_of_marked_descriptors();
  if (NumberOfMarkedDescriptors::decode(heap->mark_compact_collector()->epoch(),
                                        raw_marked) <
      number_of_own_descriptors) {
    heap->mark_compact_collector()->MarkDescriptorArrayFromWriteBarrier(
        host, descriptor_array, number_of_own_descriptors);
  }
}

bool Heap::PageFlagsAreConsistent(HeapObject object) {
  if (V8_ENABLE_THIRD_PARTY_HEAP_BOOL) {
    return true;
  }
  MemoryChunk* chunk = MemoryChunk::FromHeapObject(object);
  heap_internals::MemoryChunk* slim_chunk =
      heap_internals::MemoryChunk::FromHeapObject(object);

  // Slim chunk flags consistency.
  CHECK_EQ(chunk->InYoungGeneration(), slim_chunk->InYoungGeneration());
  CHECK_EQ(chunk->IsFlagSet(MemoryChunk::INCREMENTAL_MARKING),
           slim_chunk->IsMarking());

  AllocationSpace identity = chunk->owner_identity();

  // Generation consistency.
  CHECK_EQ(identity == NEW_SPACE || identity == NEW_LO_SPACE,
           slim_chunk->InYoungGeneration());
  // Read-only consistency.
  CHECK_EQ(chunk->InReadOnlySpace(), slim_chunk->InReadOnlySpace());

  // Marking consistency.
  if (chunk->IsWritable()) {
    // RO_SPACE can be shared between heaps, so we can't use RO_SPACE objects to
    // find a heap. The exception is when the ReadOnlySpace is writeable, during
    // bootstrapping, so explicitly allow this case.
    Heap* heap = Heap::FromWritableHeapObject(object);
    CHECK_EQ(slim_chunk->IsMarking(), heap->incremental_marking()->IsMarking());
  } else {
    // Non-writable RO_SPACE must never have marking flag set.
    CHECK(!slim_chunk->IsMarking());
  }
  return true;
}

void Heap::SetEmbedderStackStateForNextFinalizaton(
    EmbedderHeapTracer::EmbedderStackState stack_state) {
  local_embedder_heap_tracer()->SetEmbedderStackStateForNextFinalization(
      stack_state);
}

#ifdef DEBUG
void Heap::IncrementObjectCounters() {
  isolate_->counters()->objs_since_last_full()->Increment();
  isolate_->counters()->objs_since_last_young()->Increment();
}
#endif  // DEBUG

}  // namespace internal
}  // namespace v8
