// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/heap.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "include/v8-callbacks.h"
#include "include/v8-function.h"
#include "include/v8-initialization.h"
#include "include/v8-isolate.h"
#include "include/v8-object.h"
#include "src/base/bounded-page-allocator.h"
#include "src/codegen/assembler-inl.h"
#include "src/codegen/compilation-cache.h"
#include "src/codegen/compiler.h"
#include "src/codegen/script-details.h"
#include "src/common/globals.h"
#include "src/flags/flags.h"
#include "src/handles/handles-inl.h"
#include "src/heap/factory.h"
#include "src/heap/gc-tracer-inl.h"
#include "src/heap/gc-tracer.h"
#include "src/heap/heap-controller.h"
#include "src/heap/heap-layout-inl.h"
#include "src/heap/heap-layout.h"
#include "src/heap/main-allocator-inl.h"
#include "src/heap/marking-state-inl.h"
#include "src/heap/minor-mark-sweep.h"
#include "src/heap/mutable-page.h"
#include "src/heap/remembered-set.h"
#include "src/heap/safepoint.h"
#include "src/heap/spaces-inl.h"
#include "src/heap/trusted-range.h"
#include "src/objects/bytecode-array-inl.h"
#include "src/objects/feedback-vector-inl.h"
#include "src/objects/fixed-array.h"
#include "src/objects/free-space-inl.h"
#include "src/objects/instruction-stream-inl.h"
#include "src/objects/internal-index.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/js-collection-inl.h"
#include "src/objects/script-inl.h"
#include "src/objects/shared-function-info-inl.h"
#include "src/objects/transitions-inl.h"
#include "src/regexp/regexp.h"
#include "src/sandbox/external-pointer-table.h"
#include "test/common/noop-bytecode-verifier.h"
#include "test/unittests/heap/heap-utils.h"
#include "test/unittests/test-utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

using HeapTest = TestWithHeapInternalsAndContext;

TEST(Heap, GenerationSizesFromHeapSize) {
  if (v8_flags.minor_ms) return;

  struct GenerationLimit {
    size_t heap_size;
    size_t expected_young_size;
    size_t expected_old_size;
  };

  static constexpr uint64_t kMB = static_cast<uint64_t>(MB);
  static constexpr uint64_t kGB = static_cast<uint64_t>(GB);

  // Here we just need to pick a large enough value.
  static constexpr uint64_t kPhysicalMemory = 16 * kGB;

  std::vector<GenerationLimit> limits = {
      {16 * kMB, 6 * kMB, 10 * kMB},    {32 * kMB, 6 * kMB, 26 * kMB},
      {64 * kMB, 6 * kMB, 58 * kMB},    {128 * kMB, 12 * kMB, 116 * kMB},
      {256 * kMB, 24 * kMB, 232 * kMB}, {512 * kMB, 48 * kMB, 464 * kMB},
      {1 * kGB, 96 * kMB, 928 * kMB},   {2 * kGB, 96 * kMB, 1952 * kMB},
      {3 * kGB, 96 * kMB, 2976 * kMB},
#if !defined(V8_TARGET_ARCH_32_BIT)
      {4 * kGB, 96 * kMB, 4000 * kMB},  {8 * kGB, 96 * kMB, 8096 * kMB},
#endif
  };

  for (const GenerationLimit& limit : limits) {
    size_t actual_young, actual_old;
    i::Heap::GenerationSizesFromHeapSize(kPhysicalMemory, limit.heap_size,
                                         &actual_young, &actual_old);
    if (limit.expected_old_size != actual_old ||
        limit.expected_young_size != actual_young) {
      printf(
          "FAIL for %.1fMB: old (actual=%.1fMB expected %.1fMB); young "
          "(actual=%.1fMB expected %.1fMB)",
          static_cast<double>(limit.heap_size) / MB,
          static_cast<double>(actual_old) / MB,
          static_cast<double>(limit.expected_old_size) / MB,
          static_cast<double>(actual_young) / MB,
          static_cast<double>(limit.expected_young_size) / MB);
    }
    EXPECT_EQ(limit.expected_old_size, actual_old);
    EXPECT_EQ(limit.expected_young_size, actual_young);
  }
}

TEST(Heap, LimitsComputationBoundariesClamp) {
  using Boundaries = HeapLimitBounds;
  Boundaries boundaries;
  boundaries.minimum_old_generation_allocation_limit = 100u;
  boundaries.maximum_old_generation_allocation_limit = 200u;
  boundaries.minimum_global_allocation_limit = 300u;
  boundaries.maximum_global_allocation_limit = 600u;

  EXPECT_EQ(100u, boundaries.bounded_old_generation_allocation_limit(50u));
  EXPECT_EQ(150u, boundaries.bounded_old_generation_allocation_limit(150u));
  EXPECT_EQ(200u, boundaries.bounded_old_generation_allocation_limit(250u));

  EXPECT_EQ(300u, boundaries.bounded_global_allocation_limit(100u));
  EXPECT_EQ(450u, boundaries.bounded_global_allocation_limit(450u));
  EXPECT_EQ(600u, boundaries.bounded_global_allocation_limit(900u));
}

TEST(Heap, LimitsComputationBoundariesAtLeastAndAtMost) {
  HeapLimitBounds boundaries;
  boundaries.maximum_old_generation_allocation_limit = 200u;
  boundaries.maximum_global_allocation_limit = 400u;

  boundaries.AtLeast(120u, 150u);
  EXPECT_EQ(120u, boundaries.minimum_old_generation_allocation_limit);
  EXPECT_EQ(150u, boundaries.minimum_global_allocation_limit);

  boundaries.AtLeast(0u, 0u);
  EXPECT_EQ(120u, boundaries.minimum_old_generation_allocation_limit);
  EXPECT_EQ(150u, boundaries.minimum_global_allocation_limit);

  const size_t kSizeMax = std::numeric_limits<size_t>::max();
  boundaries.AtMost(kSizeMax, kSizeMax);
  EXPECT_EQ(200u, boundaries.maximum_old_generation_allocation_limit);
  EXPECT_EQ(400u, boundaries.maximum_global_allocation_limit);

  boundaries.AtMost(180u, 300u);
  EXPECT_EQ(180u, boundaries.maximum_old_generation_allocation_limit);
  EXPECT_EQ(300u, boundaries.maximum_global_allocation_limit);

  boundaries.AtMost(100u, 100u);
  EXPECT_EQ(120u, boundaries.maximum_old_generation_allocation_limit);
  EXPECT_EQ(150u, boundaries.maximum_global_allocation_limit);
}

TEST_F(HeapTest, LimitsComputationBoundariesConstruction) {
  Heap* heap = i_isolate()->heap();

  const size_t kSizeMax = std::numeric_limits<size_t>::max();
  HeapLimitBounds no_boundaries;
  EXPECT_EQ(0u, no_boundaries.minimum_old_generation_allocation_limit);
  EXPECT_EQ(0u, no_boundaries.minimum_global_allocation_limit);
  EXPECT_EQ(kSizeMax, no_boundaries.maximum_old_generation_allocation_limit);
  EXPECT_EQ(kSizeMax, no_boundaries.maximum_global_allocation_limit);

  HeapLimitBounds at_least = heap->limits()->AtLeastCurrentLimits();
  EXPECT_EQ(heap->OldGenerationAllocationLimitForTesting(),
            at_least.minimum_old_generation_allocation_limit);
  EXPECT_EQ(heap->GlobalAllocationLimitForTesting(),
            at_least.minimum_global_allocation_limit);
  EXPECT_EQ(kSizeMax, at_least.maximum_old_generation_allocation_limit);
  EXPECT_EQ(kSizeMax, at_least.maximum_global_allocation_limit);

  HeapLimitBounds at_most = heap->limits()->AtMostCurrentLimits();
  EXPECT_EQ(heap->OldGenerationAllocationLimitForTesting(),
            at_most.maximum_old_generation_allocation_limit);
  EXPECT_EQ(heap->GlobalAllocationLimitForTesting(),
            at_most.maximum_global_allocation_limit);
  EXPECT_EQ(0u, at_most.minimum_old_generation_allocation_limit);
  EXPECT_EQ(0u, at_most.minimum_global_allocation_limit);
}

namespace {
std::pair<size_t, size_t> HeapLimitsForPhysicalMemory(
    uint64_t physical_memory) {
  std::unique_ptr<v8::ArrayBuffer::Allocator> array_buffer_allocator(
      v8::ArrayBuffer::Allocator::NewDefaultAllocator());
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = array_buffer_allocator.get();
  create_params.constraints.ConfigureDefaults(physical_memory, 0);
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);
  size_t max_old_generation_size = i_isolate->heap()->MaxOldGenerationSize();
  size_t young_generation_size = i_isolate->heap()->MaxSemiSpaceSize();
  isolate->Dispose();
  return std::make_pair(max_old_generation_size, young_generation_size);
}
}  // anonymous namespace

TEST_F(HeapTest, ExpectedGenerationLimitsForPhysicalMemory) {
  if (v8_flags.max_semi_space_size != 0) return;

  struct OldLimit {
    uint64_t physical_memory;
    // Max old generation allocation limit for 32-bit.
    uint64_t arch_32bit;
    uint64_t arch_64bit_android;
    // Max old generation allocation limit for 64-bit.
    uint64_t arch_64bit;
  };

  struct YoungLimit {
    uint64_t physical_memory;
    uint64_t scavenger;
    uint64_t scavenger_android;
    uint64_t minor_ms;
  };

  static constexpr uint64_t kMB = static_cast<uint64_t>(MB);
  static constexpr uint64_t kGB = static_cast<uint64_t>(GB);

  // Expected young generation limits.
  std::vector<YoungLimit> young_limits = {
      {512 * kMB, 4 * kMB, 2 * kMB, 32 * kMB},
      {1 * kGB, 8 * kMB, 2 * kMB, 64 * kMB},
      {1536 * kMB, 16 * kMB, 4 * kMB, 72 * kMB},
      {2 * kGB, 16 * kMB, 4 * kMB, 72 * kMB},
      {3 * kGB, 32 * kMB, 8 * kMB, 72 * kMB},
      {4 * kGB, 32 * kMB, 8 * kMB, 72 * kMB},
      {6 * kGB, 32 * kMB, 8 * kMB, 72 * kMB},
      {8 * kGB - 1, 32 * kMB, 8 * kMB, 72 * kMB},
      {8 * kGB, 32 * kMB, 32 * kMB, 72 * kMB},
      {15 * kGB - 1, 32 * kMB, 32 * kMB, 72 * kMB},
      {15 * kGB, 32 * kMB, 32 * kMB, 72 * kMB},
      {16 * kGB, 32 * kMB, 32 * kMB, 72 * kMB},
      {32 * kGB, 32 * kMB, 32 * kMB, 72 * kMB},
  };

  // Expected old generation limits.
  std::vector<OldLimit> old_limits = {
      {512 * kMB, 256 * kMB, 256 * kMB, 256 * kMB},
      {1 * kGB, 256 * kMB, 256 * kMB, 512 * kMB},
      {1536 * kMB, 384 * kMB, 384 * kMB, 768 * kMB},
      {2 * kGB, 512 * kMB, 512 * kMB, 1 * kGB},
      {3 * kGB, 768 * kMB, 768 * kMB, 1536 * kMB},
      {4 * kGB, kGB, kGB, 2 * kGB},
      {6 * kGB, kGB, 1 * kGB + 512 * kMB, 3 * kGB},
      {8 * kGB - 1, kGB, 2 * kGB, 4 * kGB},
      {8 * kGB, kGB, 2 * kGB, 4 * kGB},
      {15 * kGB - 1, kGB, 3 * kGB + 768 * kMB, 4 * kGB},
      {15 * kGB, kGB, 3 * kGB + 768 * kMB, 4 * kGB},
      {16 * kGB, kGB, 4 * kGB, 4 * kGB},
      {32 * kGB, kGB, 4 * kGB, 4 * kGB},
  };

  EXPECT_EQ(young_limits.size(), old_limits.size());
  size_t last = 0;

  for (size_t i = 0; i < young_limits.size(); i++) {
    // Make sure that list is sorted by physical memory size.
    EXPECT_LT(last, young_limits[i].physical_memory);
    last = young_limits[i].physical_memory;

    // Make sure that same physical memory is tested for both old & young.
    EXPECT_EQ(young_limits[i].physical_memory, old_limits[i].physical_memory);
  }

  // There are no devices with < 1GB of RAM. We only test 512MB so we can show
  // that limits remain the same.
  EXPECT_EQ(512 * kMB, young_limits[0].physical_memory);
  EXPECT_EQ(1 * kGB, young_limits[1].physical_memory);

  for (size_t i = 0; i < old_limits.size(); i++) {
    const YoungLimit& young_limit = young_limits[i];
    const OldLimit& old_limit = old_limits[i];
    uint64_t physical_memory = old_limit.physical_memory;

#if defined(V8_TARGET_ARCH_32_BIT)
    const uint64_t expected_old = old_limit.arch_32bit;
#elif V8_OS_ANDROID
    const uint64_t expected_old = old_limit.arch_64bit_android;
#else
    const uint64_t expected_old = old_limit.arch_64bit;
#endif

#if V8_OS_ANDROID
    // Android enforces 8MB limit on semi-space size unless high-end android
    // mode is enabled.
    const uint64_t expected_young = v8_flags.minor_ms
                                        ? young_limit.minor_ms
                                        : young_limit.scavenger_android;
#else
    const uint64_t expected_young =
        v8_flags.minor_ms ? young_limit.minor_ms : young_limit.scavenger;
#endif

    auto [actual_old, actual_young] =
        HeapLimitsForPhysicalMemory(physical_memory);

    if (actual_old != expected_old || actual_young != expected_young) {
      printf("Error for physical memory size %dMB\n",
             static_cast<int>(physical_memory / kMB));
    }

    EXPECT_EQ(actual_old, expected_old);
    EXPECT_EQ(actual_young, expected_young);
  }
}

TEST_F(HeapTest, ASLR) {
#if V8_TARGET_ARCH_X64
#if V8_OS_DARWIN
  Heap* heap = i_isolate()->heap();
  std::set<void*> hints;
  for (int i = 0; i < 1000; i++) {
    hints.insert(heap->GetRandomMmapAddr());
  }
  if (hints.size() == 1) {
    EXPECT_TRUE((*hints.begin()) == nullptr);
    EXPECT_TRUE(i::GetRandomMmapAddr() == nullptr);
  } else {
    // It is unlikely that 1000 random samples will collide to less then 500
    // values.
    EXPECT_GT(hints.size(), 500u);
    const uintptr_t kRegionMask = 0xFFFFFFFFu;
    void* first = *hints.begin();
    for (void* hint : hints) {
      uintptr_t diff = reinterpret_cast<uintptr_t>(first) ^
                       reinterpret_cast<uintptr_t>(hint);
      EXPECT_LE(diff, kRegionMask);
    }
  }
#endif  // V8_OS_DARWIN
#endif  // V8_TARGET_ARCH_X64
}

TEST_F(HeapTest, ExternalLimitDefault) {
  Heap* heap = i_isolate()->heap();
  EXPECT_EQ(kExternalAllocationSoftLimit, heap->external_memory_soft_limit());
}

TEST_F(HeapTest, ExternalLimitStaysAboveDefaultForExplicitHandling) {
  v8::ExternalMemoryAccounter accounter;
  accounter.Increase(v8_isolate(), 10 * MB);
  accounter.Decrease(v8_isolate(), 10 * MB);
  Heap* heap = i_isolate()->heap();
  EXPECT_GE(heap->external_memory_soft_limit(), kExternalAllocationSoftLimit);
}

#ifdef V8_COMPRESS_POINTERS
TEST_F(HeapTest, HeapLayout) {
  // Produce some garbage.
  RunJS(
      "let ar = [];"
      "for (let i = 0; i < 100; i++) {"
      "  ar.push(Array(i));"
      "}"
      "ar.push(Array(32 * 1024 * 1024));");

  Address cage_base = i_isolate()->cage_base();
  EXPECT_TRUE(IsAligned(cage_base, size_t{4} * GB));

  Address code_cage_base = i_isolate()->code_cage_base();
  if (V8_EXTERNAL_CODE_SPACE_BOOL) {
    EXPECT_TRUE(IsAligned(code_cage_base, kMinExpectedOSPageSize));
  } else {
    EXPECT_TRUE(IsAligned(code_cage_base, size_t{4} * GB));
  }

#if V8_ENABLE_SANDBOX
  Address trusted_space_base =
      i_isolate()->isolate_group()->GetTrustedPtrComprCageBase();
  EXPECT_TRUE(IsAligned(trusted_space_base, size_t{4} * GB));
  base::AddressRegion trusted_reservation(trusted_space_base, size_t{4} * GB);
#endif

  // Check that all memory chunks belong this region.
  base::AddressRegion heap_reservation(cage_base, size_t{4} * GB);
  base::AddressRegion code_reservation(code_cage_base, size_t{4} * GB);

  SafepointScope scope(i_isolate(), kGlobalSafepointForSharedSpaceIsolate);
  OldGenerationMemoryChunkIterator iter(i_isolate()->heap());
  while (MutablePage* chunk = iter.next()) {
    Address address = chunk->ChunkAddress();
    size_t size = chunk->area_end() - address;
    AllocationSpace owner_id = chunk->owner_identity();
    if (V8_EXTERNAL_CODE_SPACE_BOOL && IsAnyCodeSpace(owner_id)) {
      EXPECT_TRUE(code_reservation.contains(address, size));
#if V8_ENABLE_SANDBOX
    } else if (IsAnyTrustedSpace(owner_id)) {
      EXPECT_TRUE(trusted_reservation.contains(address, size));
#endif
    } else {
      EXPECT_TRUE(heap_reservation.contains(address, size));
    }
  }
}
#endif  // V8_COMPRESS_POINTERS

namespace {
void ShrinkNewSpace(NewSpace* new_space) {
  if (!v8_flags.minor_ms) {
    new_space->heap()->ReduceNewSpaceSizeForTesting();
    return;
  }
  // MinorMS shrinks the space as part of sweeping. Here we fake a GC cycle, in
  // which we just shrink without marking or sweeping.
  PagedNewSpace* paged_new_space = PagedNewSpace::From(new_space);
  Heap* heap = paged_new_space->heap();
  heap->EnsureSweepingCompleted(
      Heap::SweepingForcedFinalizationMode::kUnifiedHeap,
      CompleteSweepingReason::kTesting);
  GCTracer* tracer = heap->tracer();
  tracer->StartObservablePause(base::TimeTicks::Now());
  tracer->StartCycle(GarbageCollector::MARK_COMPACTOR,
                     GarbageCollectionReason::kTesting, "heap unittest",
                     GCTracer::MarkingType::kAtomic);
  tracer->StartAtomicPause();
  paged_new_space->StartShrinking(paged_new_space->MinimumCapacity());
  for (auto it = paged_new_space->begin();
       it != paged_new_space->end() &&
       (paged_new_space->ShouldReleaseEmptyPage());) {
    NormalPage* page = *it++;
    if (page->allocated_bytes() == 0) {
      paged_new_space->paged_space()->RemovePageFromSpace(page);
      heap->memory_allocator()->Free(MemoryAllocator::FreeMode::kImmediately,
                                     page);
    } else {
      // The number of live bytes should be zero, because at this point we're
      // after a GC.
      DCHECK_EQ(0, page->live_bytes());
      // We set it to the number of allocated bytes, because FinishShrinking
      // below expects that all pages have been swept and those that remain
      // contain live bytes.
      page->SetLiveBytes(page->allocated_bytes());
    }
  }
  paged_new_space->FinishShrinking();
  for (NormalPage* page : *paged_new_space) {
    // We reset the number of live bytes to zero, as is expected after a GC.
    page->SetLiveBytes(0);
  }
  tracer->StopAtomicPause();
  tracer->StopObservablePause(GarbageCollector::MARK_COMPACTOR,
                              base::TimeTicks::Now());
  if (heap->cpp_heap()) {
    cppgc::internal::StatsCollector* stats_collector =
        CppHeap::From(heap->cpp_heap())->stats_collector();
    stats_collector->NotifyMarkingStarted(
        cppgc::internal::CollectionType::kMajor,
        cppgc::Heap::MarkingType::kAtomic,
        cppgc::internal::MarkingConfig::IsForcedGC::kNotForced);
    stats_collector->NotifyMarkingCompleted(0);
    stats_collector->NotifySweepingCompleted(
        cppgc::Heap::SweepingType::kAtomic);
  }
  tracer->NotifyFullSweepingCompletedAndStopCycleIfFinished();
}
}  // namespace

TEST_F(HeapTest, GrowAndShrinkNewSpace) {
  if (v8_flags.single_generation) return;
  {
    ManualGCScope manual_gc_scope(i_isolate());
    // Avoid shrinking new space in GC epilogue. This can happen if allocation
    // throughput samples have been taken while executing the benchmark.
    v8_flags.predictable = true;
    v8_flags.stress_concurrent_allocation = false;  // For SimulateFullSpace.
  }
  NewSpace* new_space = heap()->new_space();

  if (heap()->MaxSemiSpaceSize() == heap()->InitialSemiSpaceSize()) {
    return;
  }

  // Make sure we're in a consistent state to start out.
  InvokeMajorGC();
  InvokeMajorGC();
  ShrinkNewSpace(new_space);

  // Explicitly growing should double the space capacity.
  size_t old_capacity, new_capacity;
  old_capacity = new_space->TotalCapacity();
  GrowNewSpaceToMaximumCapacity();
  new_capacity = new_space->TotalCapacity();

  old_capacity = new_space->TotalCapacity();
  {
    v8::HandleScope temporary_scope(reinterpret_cast<v8::Isolate*>(isolate()));
    SimulateFullSpace(new_space);
  }
  new_capacity = new_space->TotalCapacity();
  CHECK_EQ(old_capacity, new_capacity);

  // Explicitly shrinking should not affect space capacity.
  old_capacity = new_space->TotalCapacity();
  ShrinkNewSpace(new_space);
  new_capacity = new_space->TotalCapacity();
  CHECK_EQ(old_capacity, new_capacity);

  // Let the scavenger empty the new space.
  EmptyNewSpaceUsingGC();
  CHECK_LE(new_space->Size(), old_capacity);

  // Explicitly shrinking should halve the space capacity.
  old_capacity = new_space->TotalCapacity();
  ShrinkNewSpace(new_space);
  new_capacity = new_space->TotalCapacity();
  if (v8_flags.minor_ms) {
    // Shrinking may not be able to remove any pages if all contain live
    // objects.
    CHECK_GE(old_capacity, new_capacity);
  } else {
    CHECK_EQ(new_capacity, new_space->MinimumCapacity());
  }

  // Consecutive shrinking should not affect space capacity.
  old_capacity = new_space->TotalCapacity();
  ShrinkNewSpace(new_space);
  ShrinkNewSpace(new_space);
  ShrinkNewSpace(new_space);
  new_capacity = new_space->TotalCapacity();
  CHECK_EQ(old_capacity, new_capacity);
}

TEST_F(HeapTest, CollectingAllAvailableGarbageShrinksNewSpace) {
  if (v8_flags.single_generation) return;
  v8_flags.stress_concurrent_allocation = false;  // For SimulateFullSpace.
  if (heap()->MaxSemiSpaceSize() == heap()->InitialSemiSpaceSize()) {
    return;
  }

  v8::Isolate* iso = reinterpret_cast<v8::Isolate*>(isolate());
  v8::HandleScope scope(iso);
  NewSpace* new_space = heap()->new_space();
  GrowNewSpaceToMaximumCapacity();
  {
    v8::HandleScope temporary_scope(iso);
    SimulateFullSpace(new_space);
  }
  InvokeMemoryReducingMajorGCs();
  CHECK_EQ(new_space->TotalCapacity(), new_space->MinimumCapacity());
}

// Test that HAllocateObject will always return an object in new-space.
TEST_F(HeapTest, OptimizedAllocationAlwaysInNewSpace) {
  if (v8_flags.single_generation) return;
  v8_flags.allow_natives_syntax = true;
  v8_flags.stress_concurrent_allocation = false;  // For SimulateFullSpace.
  if (!isolate()->use_optimizer()) return;
  if (v8_flags.gc_global || v8_flags.stress_compaction ||
      v8_flags.stress_incremental_marking) {
    return;
  }
  v8::Isolate* iso = reinterpret_cast<v8::Isolate*>(isolate());
  ManualGCScope manual_gc_scope(isolate());
  v8::HandleScope scope(iso);
  v8::Local<v8::Context> ctx = iso->GetCurrentContext();
  SimulateFullSpace(heap()->new_space());
  AlwaysAllocateScopeForTesting always_allocate(heap());
  v8::Local<v8::Value> res = WithIsolateScopeMixin::RunJS(
      "function c(x) {"
      "  this.x = x;"
      "  for (var i = 0; i < 32; i++) {"
      "    this['x' + i] = x;"
      "  }"
      "}"
      "function f(x) { return new c(x); };"
      "%PrepareFunctionForOptimization(f);"
      "f(1); f(2); f(3);"
      "%OptimizeFunctionOnNextCall(f);"
      "f(4);");

  CHECK_EQ(4, res.As<v8::Object>()
                  ->GetRealNamedProperty(ctx, NewString("x"))
                  .ToLocalChecked()
                  ->Int32Value(ctx)
                  .FromJust());

  i::DirectHandle<JSReceiver> o =
      v8::Utils::OpenDirectHandle(*v8::Local<v8::Object>::Cast(res));

  CHECK(HeapLayout::InYoungGeneration(*o));
}

namespace {
template <RememberedSetType direction>
static size_t GetRememberedSetSize(Isolate* isolate, Tagged<HeapObject> obj) {
  size_t count = 0;
  auto chunk = MutablePage::FromHeapObject(isolate, obj);
  RememberedSet<direction>::Iterate(
      chunk,
      [&count](MaybeObjectSlot slot) {
        count++;
        return KEEP_SLOT;
      },
      SlotSet::KEEP_EMPTY_BUCKETS);
  return count;
}
}  // namespace

TEST_F(HeapTest, RememberedSet_InsertOnPromotingObjectToOld) {
  if (v8_flags.single_generation || v8_flags.stress_incremental_marking ||
      v8_flags.scavenger_chaos_mode) {
    return;
  }
  v8_flags.stress_concurrent_allocation = false;  // For SealCurrentObjects.
  v8_flags.scavenger_precise_object_pinning = false;
  ManualGCScope manual_gc_scope(isolate());
  Factory* factory = isolate()->factory();
  Heap* heap = isolate()->heap();
  SealCurrentObjects();
  HandleScope handle_scope(isolate());

  // Create a young object and age it one generation inside the new space.
  IndirectHandle<FixedArray> arr = factory->NewFixedArray(1);
  std::vector<Handle<FixedArray>> handles;
  if (v8_flags.minor_ms) {
    NewSpace* new_space = heap->new_space();
    CHECK_NE(new_space->TotalCapacity(), new_space->MaximumCapacity());
    // Fill current pages to force MinorMS to promote them.
    SimulateFullSpace(new_space, &handles);
    SafepointScope scope(isolate(), kGlobalSafepointForSharedSpaceIsolate);
    // New empty pages should remain in new space.
    GrowNewSpaceToMaximumCapacity();
  }
  InvokeMinorGC();
  CHECK(HeapLayout::InYoungGeneration(*arr));

  // Add into 'arr' a reference to an object one generation younger.
  {
    HandleScope scope_inner(isolate());
    DirectHandle<Object> number = factory->NewHeapNumber(42);
    arr->set(0, *number);
  }

  // Promote 'arr' into old, its element is still in new, the old to new
  // refs are inserted into the remembered sets during GC.
  {
    // CSS prevents promoting objects to old space.
    DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap);
    InvokeMinorGC();
  }
  heap->EnsureSweepingCompleted(Heap::SweepingForcedFinalizationMode::kV8Only,
                                CompleteSweepingReason::kTesting);

  CHECK(heap->InOldSpace(*arr));
  CHECK(HeapLayout::InYoungGeneration(arr->get(0)));
  if (v8_flags.minor_ms) {
    CHECK_EQ(1, GetRememberedSetSize<OLD_TO_NEW_BACKGROUND>(isolate(), *arr));
  } else {
    CHECK_EQ(1, GetRememberedSetSize<OLD_TO_NEW>(isolate(), *arr));
  }
}

TEST_F(HeapTest, Regress978156) {
  if (!v8_flags.incremental_marking) return;
  if (v8_flags.single_generation) return;
  ManualGCScope manual_gc_scope(isolate());

  HandleScope handle_scope(isolate());
  Heap* heap = isolate()->heap();

  // 1. Ensure that the new space is empty.
  EmptyNewSpaceUsingGC();
  // 2. Fill the new space with FixedArrays.
  std::vector<Handle<FixedArray>> arrays;
  SimulateFullSpace(heap->new_space(), &arrays);
  // 3. Trim the last array by one word thus creating a one-word filler.
  DirectHandle<FixedArray> last = arrays.back();
  const uint32_t last_len = last->length().value();
  CHECK_GT(last_len, 0);
  heap->RightTrimArray(*last, last_len - 1, last_len);
  // 4. Get the last filler on the page.
  Tagged<HeapObject> filler = HeapObject::FromAddress(
      MutablePage::FromHeapObject(isolate(), *last)->area_end() - kTaggedSize);
  HeapObject::FromAddress(last->address() + last->Size());
  CHECK(IsFiller(filler));
  // 5. Start incremental marking.
  i::IncrementalMarking* marking = heap->incremental_marking();
  if (marking->IsStopped()) {
    SafepointScope scope(isolate(), kGlobalSafepointForSharedSpaceIsolate);
    heap->tracer()->StartCycle(
        GarbageCollector::MARK_COMPACTOR, GarbageCollectionReason::kTesting,
        "collector cctest", GCTracer::MarkingType::kIncremental);
    marking->Start(GarbageCollector::MARK_COMPACTOR,
                   i::GarbageCollectionReason::kTesting, "testing");
  }
  // 6. Mark the filler black to access its two markbits. This triggers
  // an out-of-bounds access of the marking bitmap in a bad case.
  heap->marking_state()->TryMarkAndAccountLiveBytes(filler);
}

TEST_F(HeapTest, SemiSpaceNewSpaceGrowsDuringFullGCIncrementalMarking) {
  if (!v8_flags.incremental_marking) return;
  if (v8_flags.single_generation) return;
  if (v8_flags.minor_ms) return;
  ManualGCScope manual_gc_scope(isolate());

  HandleScope handle_scope(isolate());
  Heap* heap = isolate()->heap();

  // 1. Record gc_count and epoch.
  auto gc_count = heap->gc_count();
  auto last_epoch = heap->tracer()->CurrentEpoch();
  // 2. Fill the new space with FixedArrays.
  std::vector<Handle<FixedArray>> arrays;
  SimulateFullSpace(heap->new_space(), &arrays);
  CHECK_EQ(0, heap->new_space()->Available());
  AllocationResult failed_allocation = heap->allocator()->AllocateRaw(
      2 * kTaggedSize, AllocationType::kYoung, AllocationOrigin::kRuntime);
  EXPECT_TRUE(failed_allocation.IsFailure());
  // 3. Start incremental marking.
  i::IncrementalMarking* marking = heap->incremental_marking();
  CHECK(marking->IsStopped());
  {
    SafepointScope scope(isolate(), kGlobalSafepointForSharedSpaceIsolate);
    heap->tracer()->StartCycle(GarbageCollector::MARK_COMPACTOR,
                               GarbageCollectionReason::kTesting, "tesing",
                               GCTracer::MarkingType::kIncremental);
    marking->Start(GarbageCollector::MARK_COMPACTOR,
                   i::GarbageCollectionReason::kTesting, "testing");
  }
  // 4. Allocate in new space.
  AllocationResult allocation = heap->allocator()->AllocateRaw(
      2 * kTaggedSize, AllocationType::kYoung, AllocationOrigin::kRuntime);
  EXPECT_FALSE(allocation.IsFailure());
  // 5. Allocation should succeed without triggering a GC.
  EXPECT_EQ(gc_count, heap->gc_count());
  EXPECT_EQ(last_epoch + 1, heap->tracer()->CurrentEpoch());
}

#ifdef V8_ENABLE_ALLOCATION_TIMEOUT
namespace {
struct RandomGCIntervalTestSetter {
  RandomGCIntervalTestSetter() {
    static constexpr int kInterval = 87;
    v8_flags.random_gc_interval = kInterval;
  }
  ~RandomGCIntervalTestSetter() { v8_flags.random_gc_interval = 0; }
};

struct HeapTestWithRandomGCInterval : RandomGCIntervalTestSetter, HeapTest {};
}  // namespace

TEST_F(HeapTestWithRandomGCInterval, AllocationTimeout) {
  if (v8_flags.stress_incremental_marking) return;
  if (v8_flags.stress_concurrent_allocation) return;

  auto* allocator = heap()->allocator();

  // Invoke major GC to cause the timeout to be updated.
  InvokeMajorGC();
  const int initial_allocation_timeout =
      allocator->get_allocation_timeout_for_testing().value_or(0);
  ASSERT_GT(initial_allocation_timeout, 0);

  for (int i = 0; i < initial_allocation_timeout - 1; ++i) {
    AllocationResult allocation = allocator->AllocateRaw(
        2 * kTaggedSize, AllocationType::kYoung, AllocationOrigin::kRuntime);
    EXPECT_FALSE(allocation.IsFailure());
  }

  // The last allocation must fail.
  AllocationResult allocation = allocator->AllocateRaw(
      2 * kTaggedSize, AllocationType::kYoung, AllocationOrigin::kRuntime);
  EXPECT_TRUE(allocation.IsFailure());
}
#endif  // V8_ENABLE_ALLOCATION_TIMEOUT

namespace {
struct CompactionDisabler {
  CompactionDisabler() : was_enabled_(v8_flags.compact) {
    v8_flags.compact = false;
  }
  ~CompactionDisabler() {
    if (was_enabled_) {
      v8_flags.compact = true;
    }
  }
  const bool was_enabled_;
};
}  // namespace

TEST_F(HeapTest, BlackAllocatedPages) {
  if (!v8_flags.black_allocated_pages) return;
  if (!v8_flags.incremental_marking) return;

  // Disable compaction to test that the FreeListCategories of black allocated
  // pages are not reset.
  CompactionDisabler disable_compaction;

  Isolate* iso = isolate();
  ManualGCScope manual_gc_scope(iso);

  auto in_free_list = [](NormalPage* page, Address address) {
    bool found = false;
    page->ForAllFreeListCategories(
        [address, &found](FreeListCategory* category) {
          category->IterateNodesForTesting(
              [address, &found](Tagged<FreeSpace> node) {
                if (!found) found = node.address() == address;
              });
        });
    return found;
  };

  Heap* heap = iso->heap();
  SimulateFullSpace(heap->old_space());

  // Allocate an object on a new page.
  HandleScope scope(iso);
  DirectHandle<FixedArray> arr =
      iso->factory()->NewFixedArray(1, AllocationType::kOld);
  Address next = arr->address() + arr->Size();

  // Assert that the next address is in the lab.
  const Address lab_top = heap->allocator()->old_space_allocator()->top();
  ASSERT_EQ(lab_top, next);

  auto* page = NormalPage::FromAddress(next);
  const size_t wasted_before_incremental_marking_start = page->wasted_memory();

  heap->StartIncrementalMarking(
      GCFlag::kNoFlags, GarbageCollectionReason::kTesting,
      GCCallbackFlags::kNoGCCallbackFlags, GarbageCollector::MARK_COMPACTOR);

  // Expect the free-space object is created.
  auto freed = HeapObject::FromAddress(next);
  EXPECT_TRUE(IsFreeSpaceOrFiller(freed));

  // The free-space object must be accounted as wasted.
  EXPECT_EQ(wasted_before_incremental_marking_start + freed->Size(),
            page->wasted_memory());

  // Check that the free-space object is not in freelist.
  EXPECT_FALSE(in_free_list(page, next));

  // The page allocated before incremental marking is not black.
  EXPECT_FALSE(page->is_black_allocated());

  // Allocate a new object on a BLACK_ALLOCATED page.
  arr = iso->factory()->NewFixedArray(1, AllocationType::kOld);
  next = arr->address() + arr->Size();

  // Expect the page to be black.
  page = NormalPage::FromHeapObject(*arr);
  EXPECT_TRUE(page->is_black_allocated());

  // Invoke GC.
  InvokeMajorGC();

  // The page is not black now.
  EXPECT_FALSE(page->is_black_allocated());
  // After the GC the next free-space object must be in freelist.
  EXPECT_TRUE(in_free_list(page, next));
}

TEST_F(HeapTest, ContainsSlow) {
  Isolate* iso = isolate();
  ManualGCScope manual_gc_scope(iso);

  Heap* heap = iso->heap();
  SimulateFullSpace(heap->old_space());

  // Allocate an object on a new page.
  HandleScope scope(iso);
  DirectHandle<FixedArray> arr =
      iso->factory()->NewFixedArray(1, AllocationType::kOld);
  CHECK(heap->old_space()->ContainsSlow(arr->address()));
  CHECK(heap->old_space()->ContainsSlow(
      MemoryChunk::FromAddress(arr->address())->address()));
  CHECK(!heap->old_space()->ContainsSlow(0));

  DirectHandle<FixedArray> large_arr = iso->factory()->NewFixedArray(
      kMaxRegularHeapObjectSize + 1, AllocationType::kOld);
  CHECK(heap->lo_space()->ContainsSlow(large_arr->address()));
  CHECK(heap->lo_space()->ContainsSlow(
      MemoryChunk::FromAddress(large_arr->address())->address()));
  CHECK(!heap->lo_space()->ContainsSlow(0));
}

TEST_F(HeapTest, ReadOnlySpaceContainsSlowRejectsAddressPastAreaEnd) {
  // After ShrinkToHighWaterMark, memory past area_end is decommitted,
  // so reporting it as "contained" simply because it's within the chunk
  // can be incorrect and lead to crashes when callers dereference it.
  Heap* heap = isolate()->heap();
  ReadOnlySpace* ro_space = heap->read_only_space();
  const auto& pages = ro_space->pages();
  CHECK(!pages.empty());

  for (ReadOnlyPage* page : pages) {
    Address area_start = page->area_start();
    Address area_end = page->area_end();
    Address chunk_end = page->ChunkAddress() + kRegularPageSize;

    // Addresses within the committed area must be reported as contained.
    CHECK(ro_space->ContainsSlow(area_start));
    CHECK(ro_space->ContainsSlow(area_end - 1));

    // Addresses past area_end but still within the original chunk range must
    // NOT be reported as contained — that memory may be decommitted.
    if (area_end < chunk_end) {
      CHECK(!ro_space->ContainsSlow(area_end));
      CHECK(!ro_space->ContainsSlow(chunk_end - 1));
    }
  }
}

TEST_F(
    HeapTest,
    ConservativePinningScavengerDoesntMoveObjectReachableFromStackNoPromotion) {
  if (v8_flags.single_generation) return;
  if (v8_flags.minor_ms) return;
  if (v8_flags.scavenger_chaos_mode) return;
  v8_flags.scavenger_conservative_object_pinning = true;
  v8_flags.scavenger_precise_object_pinning = false;
  v8_flags.scavenger_promote_quarantined_pages = false;
  ManualGCScope manual_gc_scope(isolate());

  IndirectHandle<HeapObject> number =
      isolate()->factory()->NewHeapNumber<AllocationType::kYoung>(42);

  // The conservative stack visitor will find this on the stack, so `number`
  // will not move during GCs with stack.
  Address number_address = number->address();

  CHECK(HeapLayout::InYoungGeneration(*number));

  for (int i = 0; i < 10; i++) {
    InvokeMinorGC();
    CHECK(HeapLayout::InYoungGeneration(*number));
    CHECK_EQ(number_address, number->address());
  }

  // `number` is already in the intermediate generation. A stackless GC should
  // now evacuate the object to the old generation.
  {
    DisableConservativeStackScanningScopeForTesting no_stack_scanning(
        isolate()->heap());
    InvokeMinorGC();
  }
  CHECK(!HeapLayout::InYoungGeneration(*number));
  CHECK_NE(number_address, number->address());
}

TEST_F(HeapTest,
       ConservativePinningScavengerDoesntMoveObjectReachableFromStack) {
  if (v8_flags.single_generation) return;
  if (v8_flags.minor_ms) return;
  v8_flags.scavenger_conservative_object_pinning = true;
  v8_flags.scavenger_precise_object_pinning = false;
  v8_flags.scavenger_promote_quarantined_pages = true;
  ManualGCScope manual_gc_scope(isolate());

  IndirectHandle<HeapObject> number =
      isolate()->factory()->NewHeapNumber<AllocationType::kYoung>(42);

  // The conservative stack visitor will find this on the stack, so `number`
  // will not move during a GC with stack.
  Address number_address = number->address();

  CHECK(HeapLayout::InYoungGeneration(*number));

  InvokeMinorGC();
  CHECK(HeapLayout::InYoungGeneration(*number));
  CHECK_EQ(number_address, number->address());

  // `number` is already in the intermediate generation. Another GC should
  // now promote the page to the old generation, again not moving the object.
  InvokeMinorGC();
  CHECK(!HeapLayout::InYoungGeneration(*number));
  CHECK_EQ(number_address, number->address());
}

TEST_F(HeapTest, ConservativePinningScavengerObjectWithSelfReference) {
  if (v8_flags.single_generation) return;
  if (v8_flags.minor_ms) return;
  v8_flags.scavenger_conservative_object_pinning = true;
  ManualGCScope manual_gc_scope(isolate());

  static constexpr int kArraySize = 10;
  DirectHandle<FixedArray> array =
      isolate()->factory()->NewFixedArray(kArraySize, AllocationType::kYoung);
  CHECK(HeapLayout::InYoungGeneration(*array));

  for (int i = 0; i < kArraySize; i++) {
    array->set(i, *array);
  }

  InvokeMinorGC();
}

TEST_F(HeapTest,
       PrecisePinningScavengerDoesntMoveObjectReachableFromHandlesNoPromotion) {
  if (v8_flags.single_generation) return;
  if (v8_flags.minor_ms) return;
  v8_flags.scavenger_precise_object_pinning = true;
  v8_flags.scavenger_conservative_object_pinning = false;
  v8_flags.scavenger_promote_quarantined_pages = false;
  ManualGCScope manual_gc_scope(isolate());

  v8::HandleScope handle_scope(reinterpret_cast<v8::Isolate*>(isolate()));
  IndirectHandle<HeapObject> number =
      isolate()->factory()->NewHeapNumber<AllocationType::kYoung>(42);

  Address number_address = number->address();

  CHECK(HeapLayout::InYoungGeneration(*number));

  for (int i = 0; i < 10; i++) {
    InvokeMinorGC();
    CHECK(HeapLayout::InYoungGeneration(*number));
    CHECK_EQ(number_address, number->address());
  }
}

TEST_F(HeapTest, PrecisePinningScavengerDoesntMoveObjectReachableFromHandles) {
  if (v8_flags.single_generation) return;
  if (v8_flags.minor_ms) return;
  v8_flags.scavenger_precise_object_pinning = true;
  v8_flags.scavenger_conservative_object_pinning = false;
  v8_flags.scavenger_promote_quarantined_pages = true;
  ManualGCScope manual_gc_scope(isolate());

  v8::HandleScope handle_scope(reinterpret_cast<v8::Isolate*>(isolate()));
  IndirectHandle<HeapObject> number =
      isolate()->factory()->NewHeapNumber<AllocationType::kYoung>(42);

  Address number_address = number->address();

  CHECK(HeapLayout::InYoungGeneration(*number));

  InvokeMinorGC();
  CHECK(HeapLayout::InYoungGeneration(*number));
  CHECK_EQ(number_address, number->address());

  // `number` is already in the intermediate generation. Another GC should
  // now move it to old gen.
  InvokeMinorGC();
  CHECK(!HeapLayout::InYoungGeneration(*number));
  CHECK_EQ(number_address, number->address());
}

TEST_F(HeapTest,
       PrecisePinningFullGCDoesntMoveYoungObjectReachableFromHandles) {
  if (v8_flags.single_generation) return;
  v8_flags.precise_object_pinning = true;
  ManualGCScope manual_gc_scope(isolate());

  v8::HandleScope handle_scope(reinterpret_cast<v8::Isolate*>(isolate()));
  IndirectHandle<HeapObject> number =
      isolate()->factory()->NewHeapNumber<AllocationType::kYoung>(42);

  Address number_address = number->address();

  CHECK(HeapLayout::InYoungGeneration(*number));
  InvokeMajorGC();
  CHECK(!HeapLayout::InYoungGeneration(*number));

  CHECK_EQ(number_address, number->address());
}

TEST_F(HeapTest, PrecisePinningFullGCDoesntMoveOldObjectReachableFromHandles) {
  v8_flags.precise_object_pinning = true;
  v8_flags.manual_evacuation_candidates_selection = true;
  ManualGCScope manual_gc_scope(isolate());

  SimulateFullSpace(isolate()->heap()->old_space());

  v8::HandleScope handle_scope(reinterpret_cast<v8::Isolate*>(isolate()));
  IndirectHandle<HeapObject> number =
      isolate()->factory()->NewHeapNumber<AllocationType::kOld>(42);

  CHECK(!HeapLayout::InYoungGeneration(*number));

  Address number_address = number->address();

  i::MutablePage::FromHeapObject(isolate(), *number)
      ->set_forced_evacuation_candidate_for_testing(true);

  InvokeMajorGC();

  CHECK_EQ(number_address, number->address());
}

TEST_F(
    HeapTest,
    ConservativePinningScopeScavengeDoesntMoveObjectReachableFromStackNoPromotion) {  // NOLINT(whitespace/line_length)
  if (v8_flags.single_generation) return;
  if (v8_flags.minor_ms) return;
  v8_flags.scavenger_conservative_object_pinning = false;
  v8_flags.scavenger_precise_object_pinning = false;
  v8_flags.scavenger_promote_quarantined_pages = false;
  ManualGCScope manual_gc_scope(isolate());

  ConservativePinningScope conservative_pinning_scope(isolate()->heap());

  IndirectHandle<HeapObject> number =
      isolate()->factory()->NewHeapNumber<AllocationType::kYoung>(42);

  // The conservative stack visitor will find this on the stack, so `number`
  // will not move during GCs with stack.
  Address number_address = number->address();

  CHECK(HeapLayout::InYoungGeneration(*number));

  for (int i = 0; i < 10; i++) {
    InvokeMinorGC();
    CHECK(HeapLayout::InYoungGeneration(*number));
    CHECK_EQ(number_address, number->address());
  }
}

TEST_F(HeapTest,
       ConservativePinningScopeScavengeDoesntMoveObjectReachableFromStack) {
  if (v8_flags.single_generation) return;
  if (v8_flags.minor_ms) return;
  v8_flags.scavenger_conservative_object_pinning = false;
  v8_flags.scavenger_precise_object_pinning = false;
  v8_flags.scavenger_promote_quarantined_pages = true;
  ManualGCScope manual_gc_scope(isolate());

  ConservativePinningScope conservative_pinning_scope(isolate()->heap());

  IndirectHandle<HeapObject> number =
      isolate()->factory()->NewHeapNumber<AllocationType::kYoung>(42);

  // The conservative stack visitor will find this on the stack, so `number`
  // will not move during a GC with stack.
  Address number_address = number->address();

  CHECK(HeapLayout::InYoungGeneration(*number));

  InvokeMinorGC();
  CHECK(HeapLayout::InYoungGeneration(*number));
  CHECK_EQ(number_address, number->address());

  // `number` is already in the intermediate generation. Another GC should
  // now promote the page to the old generation, again not moving the object.
  InvokeMinorGC();
  CHECK(!HeapLayout::InYoungGeneration(*number));
  CHECK_EQ(number_address, number->address());
}

TEST_F(HeapTest,
       ConservativePinningScopeMarkCompactDoesntMoveObjectReachableFromStack) {
  v8_flags.manual_evacuation_candidates_selection = true;
  v8_flags.compact_with_stack = true;
  ManualGCScope manual_gc_scope(isolate());

  SimulateFullSpace(isolate()->heap()->old_space());

  ConservativePinningScope conservative_pinning_scope(isolate()->heap());

  IndirectHandle<HeapObject> number =
      isolate()->factory()->NewHeapNumber<AllocationType::kOld>(42);

  // The conservative stack visitor will find this on the stack, so `number`
  // will not move during a GC with stack.
  Address number_address = number->address();

  for (int i = 0; i < 10; i++) {
    i::MutablePage::FromHeapObject(isolate(), *number)
        ->set_forced_evacuation_candidate_for_testing(true);
    InvokeMajorGC();
    CHECK_EQ(number_address, number->address());
  }
}

TEST_F(HeapTest, ScavengerDoesntStrongifyWeakGlobals) {
  if (v8_flags.single_generation) return;
  if (v8_flags.minor_ms) return;
  v8_flags.scavenger_conservative_object_pinning = false;
  v8_flags.scavenger_precise_object_pinning = false;
  ManualGCScope manual_gc_scope(isolate());
  Factory* factory = isolate()->factory();
  v8::Isolate* iso = reinterpret_cast<v8::Isolate*>(isolate());

  // Make sure weak Globals are not strongified by Scavenger. If this ever
  // changes the CHECK in this block should fail.
  v8::Global<v8::String> global;
  {
    v8::HandleScope handle_scope(iso);
    IndirectHandle<String> str =
        factory->NewStringFromStaticChars("should be reclaimed");
    global.Reset(iso, Utils::ToLocal(str));
    global.SetWeak();
  }
  CHECK(!global.IsEmpty());
  InvokeMinorGC();
  CHECK(global.IsEmpty());
}

namespace {
template <typename GCCallback>
void ConservativePinningScopeScavengeRetainsObjectReachableFromStack(
    HeapTest* test, AllocationType allocation_type, GCCallback gc_callback) {
  v8_flags.scavenger_conservative_object_pinning = false;
  v8_flags.scavenger_precise_object_pinning = false;
  v8_flags.scavenger_promote_quarantined_pages = false;
  ManualGCScope manual_gc_scope(test->isolate());
  Factory* factory = test->isolate()->factory();
  v8::Isolate* iso = reinterpret_cast<v8::Isolate*>(test->isolate());

  ConservativePinningScope conservative_pinning_scope(test->isolate()->heap());

  // The conservative stack visitor will find this on the stack, so `object`
  // will not move during GCs with stack.
  Tagged<String> object;
  // Use a `Global` to check whether `object` is actually retained. There should
  // be no other references to it.
  v8::Global<v8::String> global;
  {
    v8::HandleScope handle_scope(iso);
    IndirectHandle<String> str =
        factory->NewStringFromStaticChars("test", allocation_type);
    // Set a weak Global reference to `str` to check that it isn't reclaimed.
    // Scavenger should not strongify weak Globals.
    global.Reset(iso, Utils::ToLocal(str));
    global.SetWeak();
    object = *str;
  }

  CHECK(!global.IsEmpty());
  CHECK(global.IsWeak());
  gc_callback(object);
  CHECK(global.IsWeak());
  CHECK(!global.IsEmpty());
  // Make sure `object` isn't optimized away.
  CHECK_EQ(*Utils::OpenDirectHandle(*global.Get(iso)), object);
}
}  // namespace

TEST_F(HeapTest,
       ConservativePinningScopeScavengeRetainsObjectReachableFromStack) {
  if (v8_flags.single_generation) return;
  if (v8_flags.minor_ms) return;
  ConservativePinningScopeScavengeRetainsObjectReachableFromStack(
      this, AllocationType::kYoung, [this](Tagged<String> object) {
        CHECK(HeapLayout::InYoungGeneration(object));
        InvokeMinorGC();
        CHECK(HeapLayout::InYoungGeneration(object));
      });
}

TEST_F(HeapTest,
       ConservativePinningScopeMarkCompactRetainsObjectReachableFromStack) {
  ConservativePinningScopeScavengeRetainsObjectReachableFromStack(
      this, AllocationType::kOld,
      [this](Tagged<String> object) { InvokeMajorGC(); });
}

TEST_F(HeapTest, ReportStatsAsCrashKeys) {
  CrashKeyStore crash_key_store(i_isolate());

  HeapStats stats;
  auto next_value = [value = size_t{1}]() mutable { return value++; };
  stats.ro_space_size = ByteSize(static_cast<size_t>(KB));
  stats.ro_space_capacity = ByteSize(static_cast<size_t>(MB));
  stats.new_space_size = ByteSize(static_cast<size_t>(GB));
  stats.new_space_capacity = ByteSize(next_value());
  stats.old_space_size = ByteSize(next_value());
  stats.old_space_capacity = ByteSize(next_value());
  stats.code_space_size = ByteSize(next_value());
  stats.code_space_capacity = ByteSize(next_value());
  stats.map_space_size = ByteSize(next_value());
  stats.map_space_capacity = ByteSize(next_value());
  stats.lo_space_size = ByteSize(next_value());
  stats.code_lo_space_size = ByteSize(next_value());
  stats.global_handle_count = next_value();
  stats.weak_global_handle_count = next_value();
  stats.pending_global_handle_count = next_value();
  stats.near_death_global_handle_count = next_value();
  stats.free_global_handle_count = next_value();
  stats.memory_allocator_size = ByteSize(next_value());
  stats.memory_allocator_capacity = ByteSize(next_value());
  stats.malloced_memory = ByteSize(next_value());
  stats.malloced_peak_memory = ByteSize(next_value());
  stats.isolate_count = next_value();
  stats.native_context_count = next_value();
  stats.is_main_isolate = true;
  stats.last_os_error = next_value();

  stats.main_cage.start = HexAddress(0x1000);
  stats.main_cage.size = ByteSize(next_value());
  stats.main_cage.free_size = ByteSize(next_value());
  stats.main_cage.largest_free_region = ByteSize(next_value());
  stats.main_cage.last_allocation_status =
      base::BoundedPageAllocator::AllocationStatus::kSuccess;

  stats.trusted_cage.start = HexAddress(0x2000);
  stats.trusted_cage.size = ByteSize(next_value());
  stats.trusted_cage.free_size = ByteSize(next_value());
  stats.trusted_cage.largest_free_region = ByteSize(next_value());
  stats.trusted_cage.last_allocation_status =
      base::BoundedPageAllocator::AllocationStatus::kFailedToCommit;

  stats.code_cage.start = HexAddress(0x3000);
  stats.code_cage.size = ByteSize(next_value());
  stats.code_cage.free_size = ByteSize(next_value());
  stats.code_cage.largest_free_region = ByteSize(next_value());
  stats.code_cage.last_allocation_status =
      base::BoundedPageAllocator::AllocationStatus::kRanOutOfReservation;

  constexpr char kMessages[] = "Last GC: minor; reason: testing";
  std::strncpy(stats.last_few_messages, kMessages,
               sizeof(stats.last_few_messages) - 1);
  stats.last_few_messages[sizeof(stats.last_few_messages) - 1] = '\0';

  heap()->ReportStatsAsCrashKeys(stats);
  auto remaining_keys = crash_key_store.KeyNames();

  auto formatted_size = [](ByteSize size) {
    const size_t bytes = size.value();

    if (bytes >= MB) {
      return absl::StrFormat("%.2fMB", static_cast<double>(bytes) / MB);
    } else if (bytes >= KB) {
      return absl::StrFormat("%.2fKB", static_cast<double>(bytes) / KB);
    } else {
      return absl::StrFormat("%zuB", bytes);
    }

  };

  const std::vector<std::pair<std::string, ByteSize>>
      expected_byte_size_fields = {
          {"v8-oom-ro-space-size", stats.ro_space_size},
          {"v8-oom-ro-space-capacity", stats.ro_space_capacity},
          {"v8-oom-new-space-size", stats.new_space_size},
          {"v8-oom-new-space-capacity", stats.new_space_capacity},
          {"v8-oom-old-space-size", stats.old_space_size},
          {"v8-oom-old-space-capacity", stats.old_space_capacity},
          {"v8-oom-code-space-size", stats.code_space_size},
          {"v8-oom-code-space-capacity", stats.code_space_capacity},
          {"v8-oom-map-space-size", stats.map_space_size},
          {"v8-oom-map-space-capacity", stats.map_space_capacity},
          {"v8-oom-lo-space-size", stats.lo_space_size},
          {"v8-oom-code-lo-space-size", stats.code_lo_space_size},
          {"v8-oom-memory-allocator-size", stats.memory_allocator_size},
          {"v8-oom-memory-allocator-capacity", stats.memory_allocator_capacity},
          {"v8-oom-malloced-memory", stats.malloced_memory},
          {"v8-oom-malloced-peak-memory", stats.malloced_peak_memory},
      };

  for (const auto& [key, value] : expected_byte_size_fields) {
    EXPECT_TRUE(crash_key_store.HasKey(key)) << key;
    EXPECT_EQ(formatted_size(value), crash_key_store.ValueForKey(key)) << key;
    remaining_keys.erase(key);
  }

  EXPECT_EQ("1.00KB", crash_key_store.ValueForKey("v8-oom-ro-space-size"));
  EXPECT_EQ("1.00MB", crash_key_store.ValueForKey("v8-oom-ro-space-capacity"));
  EXPECT_EQ("1024.00MB", crash_key_store.ValueForKey("v8-oom-new-space-size"));

  const std::vector<std::pair<std::string, size_t>> expected_size_fields = {
      {"v8-oom-global-handle-count", stats.global_handle_count},
      {"v8-oom-weak-global-handle-count", stats.weak_global_handle_count},
      {"v8-oom-pending-global-handle-count", stats.pending_global_handle_count},
      {"v8-oom-near-death-global-handle-count",
       stats.near_death_global_handle_count},
      {"v8-oom-free-global-handle-count", stats.free_global_handle_count},
      {"v8-oom-isolate-count", stats.isolate_count},
      {"v8-oom-native-context-count", stats.native_context_count},
      {"v8-oom-last-os-error", stats.last_os_error},
  };

  for (const auto& [key, value] : expected_size_fields) {
    EXPECT_TRUE(crash_key_store.HasKey(key)) << key;
    EXPECT_EQ(std::to_string(value), crash_key_store.ValueForKey(key)) << key;
    remaining_keys.erase(key);
  }

  const std::vector<std::pair<std::string, ByteSize>> expected_cage_fields = {
      {"v8-oom-main-cage-size", stats.main_cage.size},
      {"v8-oom-main-cage-free-size", stats.main_cage.free_size},
      {"v8-oom-main-cage-largest-free-region",
       stats.main_cage.largest_free_region},
      {"v8-oom-trusted-cage-size", stats.trusted_cage.size},
      {"v8-oom-trusted-cage-free-size", stats.trusted_cage.free_size},
      {"v8-oom-trusted-cage-largest-free-region",
       stats.trusted_cage.largest_free_region},
      {"v8-oom-code-cage-size", stats.code_cage.size},
      {"v8-oom-code-cage-free-size", stats.code_cage.free_size},
      {"v8-oom-code-cage-largest-free-region",
       stats.code_cage.largest_free_region},
  };

  for (const auto& [key, value] : expected_cage_fields) {
    EXPECT_TRUE(crash_key_store.HasKey(key)) << key;
    EXPECT_EQ(formatted_size(value), crash_key_store.ValueForKey(key)) << key;
    remaining_keys.erase(key);
  }

  std::vector<std::pair<std::string, std::string>> expected_string_fields = {
      {"v8-oom-main-cage-start", "0x1000"},
      {"v8-oom-trusted-cage-start", "0x2000"},
      {"v8-oom-code-cage-start", "0x3000"},
      {"v8-oom-is-main-isolate", "true"},
      {"v8-oom-last-few-messages", kMessages},
      {"v8-oom-main-cage-last-alloc-status",
       base::ToString(stats.main_cage.last_allocation_status)},
      {"v8-oom-trusted-cage-last-alloc-status",
       base::ToString(stats.trusted_cage.last_allocation_status)},
      {"v8-oom-code-cage-last-alloc-status",
       base::ToString(stats.code_cage.last_allocation_status)},
  };

  for (const auto& [key, expected] : expected_string_fields) {
    EXPECT_TRUE(crash_key_store.HasKey(key)) << key;
    EXPECT_EQ(expected, crash_key_store.ValueForKey(key)) << key;
    remaining_keys.erase(key);
  }

  EXPECT_THAT(remaining_keys, ::testing::IsEmpty());
}

namespace {
CrashKeyStore* g_crash_key_store_for_oom = nullptr;

void FatalMemoryErrorCallbackForTest(const char* location,
                                     const OOMDetails& details) {
  CHECK_NOT_NULL(g_crash_key_store_for_oom);
  // Update this number when adding/removing crash keys.
  CHECK_EQ(g_crash_key_store_for_oom->size(), 42u);
  CHECK(g_crash_key_store_for_oom->HasKey("v8-oom-stack"));
  base::OS::PrintError("Reached end of test.\n");
  std::abort();
}
}  // anonymous namespace

TEST_F(HeapTest, CheckCrashKeysAreReportedInOOM) {
  CrashKeyStore crash_key_store(i_isolate());
  g_crash_key_store_for_oom = &crash_key_store;
  v8_isolate()->SetOOMErrorHandler(FatalMemoryErrorCallbackForTest);
  EXPECT_DEATH_IF_SUPPORTED(
      { heap()->FatalProcessOutOfMemory("CheckCrashKeysAreReportedInOOM"); },
      "Reached end of test.");
  g_crash_key_store_for_oom = nullptr;
}

TEST_F(HeapTest, FindCodeForInnerPointer) {
#define __ assm.

  Assembler assm(i_isolate()->allocator(), AssemblerOptions{});

  __ nop();  // supported on all architectures

  CodeDesc desc;
  assm.GetCode(i_isolate(), &desc);
  DirectHandle<InstructionStream> code(
      Factory::CodeBuilder(i_isolate(), desc, CodeKind::FOR_TESTING)
          .Build()
          ->instruction_stream(),
      i_isolate());
  EXPECT_TRUE(IsInstructionStream(*code));

  Tagged<HeapObject> obj = Cast<HeapObject>(*code);
  Address obj_addr = obj.address();

  for (int i = 0; i < obj->Size(); i += kTaggedSize) {
    Tagged<Code> lookup_result = heap()->FindCodeForInnerPointer(obj_addr + i);
    EXPECT_EQ(*code, lookup_result->instruction_stream());
  }

  DirectHandle<InstructionStream> copy(
      Factory::CodeBuilder(i_isolate(), desc, CodeKind::FOR_TESTING)
          .Build()
          ->instruction_stream(),
      i_isolate());
  Tagged<HeapObject> obj_copy = Cast<HeapObject>(*copy);
  Tagged<Code> not_right = heap()->FindCodeForInnerPointer(
      obj_copy.address() + obj_copy->Size() / 2);
  EXPECT_NE(not_right->instruction_stream(), *code);
  EXPECT_EQ(not_right->instruction_stream(), *copy);
#undef __
}

TEST_F(HeapTest, BytecodeArray) {
  if (!v8_flags.compact) return;
  if (v8_flags.precise_object_pinning) return;
  static const uint8_t kRawBytes[] = {0xC3, 0x7E, 0xA5, 0x5A};
  static const int kRawBytesSize = sizeof(kRawBytes);
  static const int32_t kFrameSize = 32;
  static const uint16_t kParameterCount = 2;
  static const uint16_t kMaxArguments = 0;

  v8_flags.manual_evacuation_candidates_selection = true;
  ManualGCScope manual_gc_scope(isolate());
  Factory* factory = i_isolate()->factory();
  HandleScope scope(i_isolate());

  SimulateFullSpace(heap()->trusted_space());

  IndirectHandle<TrustedFixedArray> constant_pool =
      factory->NewTrustedFixedArray(5);
  for (int i = 0; i < 5; i++) {
    IndirectHandle<Object> number = factory->NewHeapNumber(i);
    constant_pool->set(i, *number);
  }

  IndirectHandle<TrustedByteArray> handler_table =
      factory->NewTrustedByteArray(3);

  // Allocate and initialize BytecodeArray
  IndirectHandle<BytecodeArray> array = factory->NewBytecodeArray(
      kRawBytesSize, kRawBytes, kFrameSize, kParameterCount, kMaxArguments,
      constant_pool, handler_table);

  EXPECT_TRUE(IsBytecodeArray(*array));

#ifdef V8_ENABLE_SANDBOX
  // BytecodeArrays are not exposed to the sandbox ("published") immediately
  // after allocation. Instead, this only happens once the bytecode has been
  // verified. This way, we ensure that we only execute safe bytecode.
  EXPECT_FALSE(array->IsPublished(i_isolate()));
  NoOpBytecodeVerifier::Verify(i_isolate(), array);
  EXPECT_TRUE(array->IsPublished(i_isolate()));
#endif  // V8_ENABLE_SANDBOX

  EXPECT_EQ(array->length(), (int)sizeof(kRawBytes));
  EXPECT_EQ(array->frame_size(), kFrameSize);
  EXPECT_EQ(array->parameter_count(), kParameterCount);
  EXPECT_EQ(array->constant_pool(), *constant_pool);
  EXPECT_EQ(array->handler_table(), *handler_table);
  EXPECT_LE(array->address(), array->GetFirstBytecodeAddress());
  EXPECT_GE(array->address() + array->BytecodeArraySize(),
            array->GetFirstBytecodeAddress() + array->length());
  for (int i = 0; i < kRawBytesSize; i++) {
    EXPECT_EQ(Memory<uint8_t>(array->GetFirstBytecodeAddress() + i),
              kRawBytes[i]);
    EXPECT_EQ(array->get(i), kRawBytes[i]);
  }

  Tagged<TrustedFixedArray> old_constant_pool_address = *constant_pool;

  // Perform a full garbage collection and force the constant pool to be on an
  // evacuation candidate.
  i::MutablePage::FromHeapObject(isolate(), *constant_pool)
      ->set_forced_evacuation_candidate_for_testing(true);
  {
    // We need to invoke GC without stack, otherwise no compaction is performed.
    DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap());
    InvokeMajorGC();
  }

  // BytecodeArray should survive.
  EXPECT_EQ(array->length(), kRawBytesSize);
  EXPECT_EQ(array->frame_size(), kFrameSize);
  for (int i = 0; i < kRawBytesSize; i++) {
    EXPECT_EQ(array->get(i), kRawBytes[i]);
    EXPECT_EQ(Memory<uint8_t>(array->GetFirstBytecodeAddress() + i),
              kRawBytes[i]);
  }

  // Constant pool should have been migrated.
  EXPECT_EQ(array->constant_pool().ptr(), constant_pool->ptr());
  EXPECT_NE(array->constant_pool().ptr(), old_constant_pool_address.ptr());
}

TEST_F(HeapTest, BytecodeFlushing) {
#if !defined(V8_LITE_MODE) && defined(V8_ENABLE_TURBOFAN)
  v8_flags.turbofan = false;
  i::v8_flags.optimize_for_size = false;
#endif  // !defined(V8_LITE_MODE) && defined(V8_ENABLE_TURBOFAN)
#ifdef V8_ENABLE_SPARKPLUG
  v8_flags.always_sparkplug = false;
#endif  // V8_ENABLE_SPARKPLUG
  i::v8_flags.flush_bytecode = true;
  i::v8_flags.allow_natives_syntax = true;

  ManualGCScope manual_gc_scope(i_isolate());
  v8::HandleScope scope(v8_isolate());
  const char* source =
      "function foo() {"
      "  var x = 42;"
      "  var y = 42;"
      "  var z = x + y;"
      "};"
      "foo()";
  DirectHandle<String> foo_name = factory()->InternalizeUtf8String("foo");

  // This compile will add the code to the compilation cache.
  {
    v8::HandleScope new_scope(v8_isolate());
    RunJS(source);
  }

  // Check function is compiled.
  DirectHandle<Object> func_value =
      Object::GetProperty(i_isolate(), i_isolate()->global_object(), foo_name)
          .ToHandleChecked();
  EXPECT_TRUE(IsJSFunction(*func_value));
  DirectHandle<JSFunction> function = Cast<JSFunction>(func_value);
  EXPECT_TRUE(function->shared()->is_compiled());

  // The code will survive at least two GCs.
  {
    DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap());
    InvokeMajorGC();
    InvokeMajorGC();
  }
  EXPECT_TRUE(function->shared()->is_compiled());

  i::SharedFunctionInfo::EnsureOldForTesting(function->shared());
  {
    DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap());
    InvokeMajorGC();
  }

  // foo should no longer be compiled
  EXPECT_FALSE(function->shared()->is_compiled());
  EXPECT_FALSE(function->is_compiled(i_isolate()));
  // Call foo to get it recompiled.
  RunJS("foo()");
  EXPECT_TRUE(function->shared()->is_compiled());
  EXPECT_TRUE(function->is_compiled(i_isolate()));
}

namespace {
void RunMultiReferencedBytecodeFlushingTest(HeapTest* test,
                                            bool sparkplug_compile) {
#if !defined(V8_LITE_MODE) && defined(V8_ENABLE_TURBOFAN)
  v8_flags.turbofan = false;
  i::v8_flags.optimize_for_size = false;
#endif  // !defined(V8_LITE_MODE) && defined(V8_ENABLE_TURBOFAN)
#ifdef V8_ENABLE_SPARKPLUG
  v8_flags.always_sparkplug = false;
  v8_flags.flush_baseline_code = true;
#else
  if (sparkplug_compile) return;
#endif  // V8_ENABLE_SPARKPLUG
  i::v8_flags.flush_bytecode = true;
  i::v8_flags.allow_natives_syntax = true;

  ManualGCScope manual_gc_scope(test->i_isolate());
  v8::HandleScope scope(test->v8_isolate());
  const char* source =
      "function foo() {"
      "  var x = 42;"
      "  var y = 42;"
      "  var z = x + y;"
      "};"
      "foo()";
  DirectHandle<String> foo_name = test->factory()->InternalizeUtf8String("foo");

  // This compile will add the code to the compilation cache.
  {
    v8::HandleScope new_scope(test->v8_isolate());
    test->RunJS(source);
  }

  // Check function is compiled.
  DirectHandle<Object> func_value =
      Object::GetProperty(test->i_isolate(), test->i_isolate()->global_object(),
                          foo_name)
          .ToHandleChecked();
  EXPECT_TRUE(IsJSFunction(*func_value));
  DirectHandle<JSFunction> function = Cast<JSFunction>(func_value);
  DirectHandle<SharedFunctionInfo> shared(function->shared(),
                                          test->i_isolate());
  EXPECT_TRUE(shared->is_compiled());

  // Make a copy of the SharedFunctionInfo which points to the same bytecode.
  Handle<SharedFunctionInfo> copy =
      test->factory()->CloneSharedFunctionInfo(shared);

  if (sparkplug_compile) {
    v8::HandleScope baseline_compilation_scope(test->v8_isolate());
    IsCompiledScope is_compiled_scope =
        copy->is_compiled_scope(test->i_isolate());
    Compiler::CompileSharedWithBaseline(
        test->i_isolate(), copy, Compiler::CLEAR_EXCEPTION, &is_compiled_scope);
  }

  i::SharedFunctionInfo::EnsureOldForTesting(*shared);
  {
    // We need to invoke GC without stack, otherwise some objects may not be
    // reclaimed because of conservative stack scanning.
    DisableConservativeStackScanningScopeForTesting no_stack_scanning(
        test->heap());
    test->InvokeMajorGC();
  }

  // shared SFI is marked old but BytecodeArray is kept alive by copy.
  EXPECT_TRUE(shared->is_compiled());
  EXPECT_TRUE(copy->is_compiled());
  EXPECT_TRUE(function->is_compiled(test->i_isolate()));

  // The feedback metadata for both SharedFunctionInfo instances should have
  // been reset.
  EXPECT_TRUE(shared->HasFeedbackMetadata());
  EXPECT_TRUE(copy->HasFeedbackMetadata());
}
}  // namespace

TEST_F(HeapTest, MultiReferencedBytecodeFlushing) {
  RunMultiReferencedBytecodeFlushingTest(this, /*sparkplug_compile=*/false);
}

TEST_F(HeapTest, MultiReferencedBytecodeFlushingWithSparkplug) {
  RunMultiReferencedBytecodeFlushingTest(this, /*sparkplug_compile=*/true);
}

TEST_F(HeapTest, JSInterceptorMap) {
  DirectHandle<InterceptorInfo> named_interceptor =
      factory()->NewInterceptorInfo(InterceptorKind::kNamed);
  DirectHandle<InterceptorInfo> indexed_interceptor =
      factory()->NewInterceptorInfo(InterceptorKind::kIndexed);

  DirectHandle<Map> last_map;
  {
    HandleScope sc1(isolate());

    const size_t N = 100;
    DirectHandleVector<JSObject> objects(isolate());
    objects.reserve(N);
    DirectHandle<JSInterceptorMap> map;

    for (size_t i = 0; i < N; i++) {
      if (i % 10 == 0) {
        // Create a fresh map every now and then.
        const int inobject_properties = 2;
        map = Cast<JSInterceptorMap>(factory()->NewExtendedMapWithMetaMap(
            isolate()->meta_map(), ExtendedMapKind::kJSInterceptorMap,
            JS_OBJECT_TYPE,
            JSObject::kHeaderSize + inobject_properties * kTaggedSize,
            TERMINAL_FAST_ELEMENTS_KIND, inobject_properties));
        map->init_flags_and_clear_extended_padding();
        map->set_named_interceptor(*named_interceptor);
        map->set_indexed_interceptor(*indexed_interceptor);
        map->set_fast_case_validity_cell(
            ReadOnlyRoots(isolate()).invalid_prototype_validity_cell());
      }

      Handle<JSObject> obj = factory()->NewJSObjectFromMap(map);
      Object::SetProperty(isolate(), obj, factory()->a_string(), obj).Check();
      Object::SetProperty(isolate(), obj, factory()->b_string(), obj).Check();
      Object::SetProperty(isolate(), obj, factory()->c_string(), obj).Check();
      Object::SetProperty(isolate(), obj, factory()->d_string(), obj).Check();

#ifdef VERIFY_HEAP
      obj->HeapObjectVerify(isolate());
      obj->map()->HeapObjectVerify(isolate());
#endif  // VERIFY_HEAP
      objects.emplace_back(obj);
      if (i == N / 2) {
        InvokeMajorGC();
        InvokeMajorGC();
      }
    }

    InvokeMajorGC();
    InvokeMajorGC();
    last_map = sc1.CloseAndEscape(map);
  }
  InvokeMajorGC();
  InvokeMajorGC();

  EXPECT_TRUE(IsJSObjectMap(*last_map));
  EXPECT_TRUE(last_map->is_extended_map());

  EXPECT_EQ(UncheckedCast<ExtendedMap>(last_map)->map_kind(),
            ExtendedMapKind::kJSInterceptorMap);
  DirectHandle<JSInterceptorMap> map = Cast<JSInterceptorMap>(last_map);
  EXPECT_EQ(map->named_interceptor(), *named_interceptor);
  EXPECT_EQ(map->indexed_interceptor(), *indexed_interceptor);
}

namespace {
void RunCompilationCacheCachingBehaviorTest(HeapTest* test,
                                            bool retain_script) {
  // If we do not have the compilation cache turned off, this test is invalid.
  if (!v8_flags.compilation_cache) {
    return;
  }
  if (!v8_flags.flush_bytecode ||
      (v8_flags.always_sparkplug && !v8_flags.flush_baseline_code)) {
    return;
  }
  Isolate* isolate = test->i_isolate();
  Factory* factory = test->factory();
  CompilationCache* compilation_cache = isolate->compilation_cache();
  LanguageMode language_mode = LanguageMode::kSloppy;

  v8::HandleScope outer_scope(test->v8_isolate());
  const char* raw_source = retain_script ? "function foo() {"
                                           "  var x = 42;"
                                           "  var y = 42;"
                                           "  var z = x + y;"
                                           "};"
                                           "foo();"
                                         : "(function foo() {"
                                           "  var x = 42;"
                                           "  var y = 42;"
                                           "  var z = x + y;"
                                           "})();";
  Handle<String> source =
      Cast<String>(factory->InternalizeUtf8String(raw_source));

  {
    v8::HandleScope scope(test->v8_isolate());
    test->RunJS(raw_source);
  }

  // The script should be in the cache now.
  {
    v8::HandleScope scope(test->v8_isolate());
    ScriptDetails script_details(Handle<Object>(),
                                 v8::ScriptOriginOptions(true, false));
    auto lookup_result =
        compilation_cache->LookupScript(source, script_details, language_mode);
    EXPECT_FALSE(lookup_result.toplevel_sfi().is_null());
  }

  // Check that the code cache entry survives at least one GC.
  {
    // In this test, we need to invoke GC without stack, otherwise some objects
    // may not be reclaimed because of conservative stack scanning.
    DisableConservativeStackScanningScopeForTesting no_stack_scanning(
        test->heap());
    test->InvokeMajorGC();
  }
  {
    v8::HandleScope scope(test->v8_isolate());
    ScriptDetails script_details(Handle<Object>(),
                                 v8::ScriptOriginOptions(true, false));
    auto lookup_result =
        compilation_cache->LookupScript(source, script_details, language_mode);
    EXPECT_FALSE(lookup_result.toplevel_sfi().is_null());

    // Progress code age until it's old and ready for GC.
    DirectHandle<SharedFunctionInfo> shared =
        lookup_result.toplevel_sfi().ToHandleChecked();
    EXPECT_TRUE(shared->HasBytecodeArray());
    SharedFunctionInfo::EnsureOldForTesting(*shared);
  }

  {
    DisableConservativeStackScanningScopeForTesting no_stack_scanning(
        test->heap());
    // The first GC flushes the BytecodeArray from the SFI.
    test->InvokeMajorGC();
    // The second GC removes the SFI from the compilation cache.
    test->InvokeMajorGC();
  }

  {
    v8::HandleScope scope(test->v8_isolate());
    // Ensure code aging cleared the entry from the cache.
    ScriptDetails script_details(Handle<Object>(),
                                 v8::ScriptOriginOptions(true, false));
    auto lookup_result =
        compilation_cache->LookupScript(source, script_details, language_mode);
    EXPECT_TRUE(lookup_result.toplevel_sfi().is_null());
    EXPECT_EQ(retain_script, !lookup_result.script().is_null());
  }
}
}  // namespace

TEST_F(HeapTest, CompilationCacheCachingBehaviorDiscardScript) {
  RunCompilationCacheCachingBehaviorTest(this, false);
}

TEST_F(HeapTest, CompilationCacheCachingBehaviorRetainScript) {
  RunCompilationCacheCachingBehaviorTest(this, true);
}

#if !defined(V8_LITE_MODE) && defined(V8_ENABLE_TURBOFAN)
TEST_F(HeapTest, OptimizeAfterBytecodeFlushingCandidate) {
  if (v8_flags.single_generation) return;
  v8_flags.turbofan = true;
#ifdef V8_ENABLE_SPARKPLUG
  v8_flags.always_sparkplug = false;
#endif  // V8_ENABLE_SPARKPLUG
  i::v8_flags.optimize_for_size = false;
  i::v8_flags.incremental_marking = true;
  i::v8_flags.flush_bytecode = true;
  i::v8_flags.allow_natives_syntax = true;
  ManualGCScope manual_gc_scope(i_isolate());

  v8::HandleScope outer_scope(v8_isolate());
  const char* source =
      "function foo() {"
      "  var x = 42;"
      "  var y = 42;"
      "  var z = x + y;"
      "};"
      "foo()";
  DirectHandle<String> foo_name = factory()->InternalizeUtf8String("foo");

  // This compile will add the code to the compilation cache.
  {
    v8::HandleScope scope(v8_isolate());
    RunJS(source);
  }

  // Check function is compiled.
  DirectHandle<Object> func_value =
      Object::GetProperty(i_isolate(), i_isolate()->global_object(), foo_name)
          .ToHandleChecked();
  EXPECT_TRUE(IsJSFunction(*func_value));
  DirectHandle<JSFunction> function = Cast<JSFunction>(func_value);
  EXPECT_TRUE(function->shared()->is_compiled());

  // The code will survive at least two GCs.
  {
    // In this test, we need to invoke GC without stack, otherwise some objects
    // may not be reclaimed because of conservative stack scanning.
    DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap());
    InvokeMajorGC();
    InvokeMajorGC();
  }
  EXPECT_TRUE(function->shared()->is_compiled());

  i::SharedFunctionInfo::EnsureOldForTesting(function->shared());
  {
    DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap());
    InvokeMajorGC();
  }

  EXPECT_FALSE(function->shared()->is_compiled());
  EXPECT_FALSE(function->is_compiled(i_isolate()));

  // This compile will compile the function again.
  {
    v8::HandleScope scope(v8_isolate());
    RunJS("foo();");
  }

  SharedFunctionInfo::EnsureOldForTesting(function->shared());
  SimulateIncrementalMarking();

  // Force optimization while incremental marking is active and while
  // the function is enqueued as a candidate.
  {
    v8::HandleScope scope(v8_isolate());
    RunJS(
        "%PrepareFunctionForOptimization(foo); foo();"
        "%OptimizeFunctionOnNextCall(foo); foo();");
  }

  // Simulate one final GC and make sure the candidate wasn't flushed.
  {
    DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap());
    InvokeMajorGC();
  }
  EXPECT_TRUE(function->shared()->is_compiled());
  EXPECT_TRUE(function->is_compiled(i_isolate()));
}
#endif  // !defined(V8_LITE_MODE) && defined(V8_ENABLE_TURBOFAN)

TEST_F(HeapTest, MultiReferencedBytecodeFlushingBothOld) {
#if !defined(V8_LITE_MODE) && defined(V8_ENABLE_TURBOFAN)
  v8_flags.turbofan = false;
  i::v8_flags.optimize_for_size = false;
#endif  // !defined(V8_LITE_MODE) && defined(V8_ENABLE_TURBOFAN)
#ifdef V8_ENABLE_SPARKPLUG
  v8_flags.always_sparkplug = false;
  v8_flags.flush_baseline_code = true;
#endif  // V8_ENABLE_SPARKPLUG
  i::v8_flags.flush_bytecode = true;
  i::v8_flags.allow_natives_syntax = true;

  ManualGCScope manual_gc_scope(i_isolate());
  v8::HandleScope scope(v8_isolate());
  const char* source =
      "function foo() {"
      "  var x = 42;"
      "  var y = 42;"
      "  var z = x + y;"
      "};"
      "foo()";
  DirectHandle<String> foo_name = factory()->InternalizeUtf8String("foo");

  // This compile will add the code to the compilation cache.
  {
    v8::HandleScope new_scope(v8_isolate());
    RunJS(source);
  }

  // Check function is compiled.
  DirectHandle<Object> func_value =
      Object::GetProperty(i_isolate(), i_isolate()->global_object(), foo_name)
          .ToHandleChecked();
  EXPECT_TRUE(IsJSFunction(*func_value));
  DirectHandle<JSFunction> function = Cast<JSFunction>(func_value);
  DirectHandle<SharedFunctionInfo> shared(function->shared(), i_isolate());
  EXPECT_TRUE(shared->is_compiled());

  // Make a copy of the SharedFunctionInfo which points to the same bytecode.
  Handle<SharedFunctionInfo> copy = factory()->CloneSharedFunctionInfo(shared);

  // Verify that both SFIs share the same BytecodeArray.
  EXPECT_EQ(shared->GetBytecodeArray(i_isolate()),
            copy->GetBytecodeArray(i_isolate()));

  i::SharedFunctionInfo::EnsureOldForTesting(*shared);
  i::SharedFunctionInfo::EnsureOldForTesting(*copy);

  {
    // We need to invoke GC without stack, otherwise some objects may not be
    // reclaimed because of conservative stack scanning.
    DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap());
    InvokeMajorGC();
  }

  // Both should be decompiled (flushed) because both were old.
  EXPECT_FALSE(shared->is_compiled());
  EXPECT_FALSE(copy->is_compiled());
}

TEST_F(HeapTest, Regress10843) {
  v8_flags.max_semi_space_size = 2;
  v8_flags.min_semi_space_size = 2;
  v8_flags.max_old_space_size = 8;
  v8_flags.compact_on_every_full_gc = true;
  v8::Isolate::CreateParams create_params;
  std::unique_ptr<v8::ArrayBuffer::Allocator> allocator(
      v8::ArrayBuffer::Allocator::NewDefaultAllocator());
  create_params.array_buffer_allocator = allocator.get();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);
  Factory* factory = i_isolate->factory();
  Heap* heap = i_isolate->heap();
  bool callback_was_invoked = false;

  heap->AddNearHeapLimitCallback(
      [](void* data, size_t current_heap_limit,
         size_t initial_heap_limit) -> size_t {
        *reinterpret_cast<bool*>(data) = true;
        return current_heap_limit * 2;
      },
      &callback_was_invoked);

  {
    v8::Isolate::Scope isolate_scope(isolate);
    PtrComprCageAccessScope ptr_compr_cage_access_scope(i_isolate);
    HandleScope scope(i_isolate);
    std::vector<Handle<FixedArray>> arrays;
    for (int i = 0; i < 140; i++) {
      arrays.push_back(factory->NewFixedArray(10000));
    }
    i::InvokeMajorGC(i_isolate);
    i::InvokeMajorGC(i_isolate);
    for (int i = 0; i < 40; i++) {
      arrays.push_back(factory->NewFixedArray(10000));
    }
    i::InvokeMajorGC(i_isolate);
    for (int i = 0; i < 100; i++) {
      arrays.push_back(factory->NewFixedArray(10000));
    }
    i::InvokeMajorGC(i_isolate);
    EXPECT_TRUE(callback_was_invoked);
  }
  isolate->Dispose();
}

TEST_F(HeapTest, Regress10560) {
  i::v8_flags.flush_bytecode = true;
  i::v8_flags.allow_natives_syntax = true;
  // Disable flags that allocate a feedback vector eagerly.
#if !defined(V8_LITE_MODE) && defined(V8_ENABLE_TURBOFAN)
  i::v8_flags.turbofan = false;
#endif  // !defined(V8_LITE_MODE) && defined(V8_ENABLE_TURBOFAN)
#ifdef V8_ENABLE_SPARKPLUG
  v8_flags.always_sparkplug = false;
#endif  // V8_ENABLE_SPARKPLUG
  i::v8_flags.lazy_feedback_allocation = true;

  ManualGCScope manual_gc_scope(i_isolate());
  v8::HandleScope scope(v8_isolate());
  const char* source =
      "function foo() {"
      "  var x = 42;"
      "  var y = 42;"
      "  var z = x + y;"
      "};"
      "foo()";
  DirectHandle<String> foo_name = factory()->InternalizeUtf8String("foo");
  RunJS(source);

  // Check function is compiled.
  DirectHandle<Object> func_value =
      Object::GetProperty(i_isolate(), i_isolate()->global_object(), foo_name)
          .ToHandleChecked();
  EXPECT_TRUE(IsJSFunction(*func_value));
  DirectHandle<JSFunction> function = Cast<JSFunction>(func_value);
  EXPECT_TRUE(function->shared()->is_compiled());
  EXPECT_FALSE(function->has_feedback_vector());

  // Pre-age bytecode so it will be flushed on next run.
  EXPECT_TRUE(function->shared()->HasBytecodeArray());
  SharedFunctionInfo::EnsureOldForTesting(function->shared());

  SimulateFullSpace(heap()->old_space());

  // Just check bytecode isn't flushed still
  EXPECT_TRUE(function->shared()->is_compiled());

  heap()->set_force_gc_on_next_allocation(true);

  // Allocate feedback vector.
  IsCompiledScope is_compiled_scope(
      function->shared()->is_compiled_scope(i_isolate()));
  JSFunction::EnsureFeedbackVector(i_isolate(), function, &is_compiled_scope);

  EXPECT_TRUE(function->has_feedback_vector());
  EXPECT_TRUE(function->shared()->is_compiled());
  EXPECT_TRUE(function->is_compiled(i_isolate()));
}

using LeakNativeContextViaMapKeyedTest = TestWithHeapInternals;

TEST_F(LeakNativeContextViaMapKeyedTest, LeakNativeContextViaMapKeyed) {
  v8_flags.allow_natives_syntax = true;
  v8::Isolate* isolate = v8_isolate();
  Heap* heap_instance = heap();
  v8::HandleScope outer_scope(isolate);
  v8::Persistent<v8::Context> ctx1p;
  v8::Persistent<v8::Context> ctx2p;
  {
    v8::HandleScope scope(isolate);
    ctx1p.Reset(isolate, v8::Context::New(isolate));
    ctx2p.Reset(isolate, v8::Context::New(isolate));
    v8::Local<v8::Context>::New(isolate, ctx1p)->Enter();
  }

  {
    // In this test, we need to invoke GC without stack, otherwise some objects
    // may not be reclaimed because of conservative stack scanning.
    DisableConservativeStackScanningScopeForTesting no_stack_scanning(
        heap_instance);
    InvokeMemoryReducingMajorGCs();
  }
  EXPECT_EQ(2, NumberOfGlobalObjects());

  {
    v8::HandleScope inner_scope(isolate);
    v8::Local<v8::Context> ctx1 = v8::Local<v8::Context>::New(isolate, ctx1p);
    v8::Local<v8::Context> ctx2 = v8::Local<v8::Context>::New(isolate, ctx2p);
    RunJS(ctx1, "var v = [42, 43];");
    v8::Local<v8::Value> v =
        ctx1->Global()
            ->Get(ctx1, v8::String::NewFromUtf8Literal(isolate, "v"))
            .ToLocalChecked();
    ctx2->Enter();
    EXPECT_TRUE(ctx2->Global()
                    ->Set(ctx2, v8::String::NewFromUtf8Literal(isolate, "o"), v)
                    .FromJust());
    Handle<Object> res = RunJS(ctx2,
                               "function f() { return o[0]; }"
                               "%PrepareFunctionForOptimization(f);"
                               "for (var i = 0; i < 10; ++i) f();"
                               "%OptimizeFunctionOnNextCall(f);"
                               "f();");
    EXPECT_EQ(42, Object::NumberValue(*res));
    EXPECT_TRUE(ctx2->Global()
                    ->Set(ctx2, v8::String::NewFromUtf8Literal(isolate, "o"),
                          v8::Int32::New(isolate, 0))
                    .FromJust());
    ctx2->Exit();
    v8::Local<v8::Context>::New(isolate, ctx1)->Exit();
    ctx1p.Reset();
    isolate->ContextDisposedNotification(
        v8::ContextDependants::kSomeDependants);
  }
  {
    DisableConservativeStackScanningScopeForTesting no_stack_scanning(
        heap_instance);
    InvokeMemoryReducingMajorGCs();
  }
  EXPECT_EQ(1, NumberOfGlobalObjects());
  ctx2p.Reset();
  {
    DisableConservativeStackScanningScopeForTesting no_stack_scanning(
        heap_instance);
    InvokeMemoryReducingMajorGCs();
  }
  EXPECT_EQ(0, NumberOfGlobalObjects());
}

namespace {
size_t near_heap_limit_invocation_count = 0;
size_t InvokeGCNearHeapLimitCallback(void* data, size_t current_heap_limit,
                                     size_t initial_heap_limit) {
  near_heap_limit_invocation_count++;
  if (near_heap_limit_invocation_count > 1) {
    // We are already in a GC triggered in this callback, raise the limit
    // to avoid an OOM.
    return current_heap_limit * 5;
  }

  DCHECK_EQ(near_heap_limit_invocation_count, 1);
  // Operations that may cause GC (e.g. taking heap snapshots) in the
  // near heap limit callback should not hit the AllowGarbageCollection
  // assertion.
  static_cast<v8::Isolate*>(data)->GetHeapProfiler()->TakeHeapSnapshot();
  return current_heap_limit * 5;
}
}  // namespace

TEST_F(HeapTest, Regress12777) {
  v8::Isolate::CreateParams create_params;
  create_params.constraints.set_max_old_generation_size_in_bytes(10 * i::MB);
  std::unique_ptr<v8::ArrayBuffer::Allocator> allocator(
      v8::ArrayBuffer::Allocator::NewDefaultAllocator());
  create_params.array_buffer_allocator = allocator.get();
  v8::Isolate* isolate = v8::Isolate::New(create_params);

  near_heap_limit_invocation_count = 0;
  isolate->AddNearHeapLimitCallback(InvokeGCNearHeapLimitCallback, isolate);

  {
    v8::Isolate::Scope isolate_scope(isolate);

    Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);
    // Allocate data to trigger the NearHeapLimitCallback.
    HandleScope scope(i_isolate);
    int length = 2 * i::MB / i::kTaggedSize;
    std::vector<Handle<FixedArray>> arrays;
    for (int i = 0; i < 5; i++) {
      arrays.push_back(i_isolate->factory()->NewFixedArray(length));
    }
    i::InvokeMajorGC(i_isolate);
    for (int i = 0; i < 5; i++) {
      arrays.push_back(i_isolate->factory()->NewFixedArray(length));
    }
    i::InvokeMajorGC(i_isolate);
    for (int i = 0; i < 5; i++) {
      arrays.push_back(i_isolate->factory()->NewFixedArray(length));
    }
    i::InvokeMajorGC(i_isolate);
    EXPECT_GT(near_heap_limit_invocation_count, 0u);
  }
  isolate->Dispose();
}

TEST_F(HeapTest, LeakNativeContextViaMap) {
  v8_flags.allow_natives_syntax = true;
  v8::Isolate* isolate = v8_isolate();
  Heap* heap_instance = heap();
  v8::HandleScope outer_scope(isolate);
  v8::Persistent<v8::Context> ctx1p;
  v8::Persistent<v8::Context> ctx2p;
  {
    v8::HandleScope scope(isolate);
    ctx1p.Reset(isolate, v8::Context::New(isolate));
    ctx2p.Reset(isolate, v8::Context::New(isolate));
    v8::Local<v8::Context>::New(isolate, ctx1p)->Enter();
  }

  {
    // In this test, we need to invoke GC without stack, otherwise some objects
    // may not be reclaimed because of conservative stack scanning.
    DisableConservativeStackScanningScopeForTesting no_stack_scanning(
        heap_instance);
    InvokeMemoryReducingMajorGCs();
  }
  // HeapTest creates and enters a default context (context 0) which is
  // unaffected by the test, so there are 3 global objects initially (default
  // context + ctx1 + ctx2).
  EXPECT_EQ(3, NumberOfGlobalObjects());

  {
    v8::HandleScope inner_scope(isolate);
    v8::Local<v8::Context> ctx1 = v8::Local<v8::Context>::New(isolate, ctx1p);
    v8::Local<v8::Context> ctx2 = v8::Local<v8::Context>::New(isolate, ctx2p);
    RunJS(ctx1, "var v = {x: 42};");
    v8::Local<v8::Value> v =
        ctx1->Global()
            ->Get(ctx1, v8::String::NewFromUtf8Literal(isolate, "v"))
            .ToLocalChecked();
    ctx2->Enter();
    EXPECT_TRUE(ctx2->Global()
                    ->Set(ctx2, v8::String::NewFromUtf8Literal(isolate, "o"), v)
                    .FromJust());
    Handle<Object> res = RunJS(ctx2,
                               "function f() { return o.x; }"
                               "%PrepareFunctionForOptimization(f);"
                               "for (var i = 0; i < 10; ++i) f();"
                               "%OptimizeFunctionOnNextCall(f);"
                               "f();");
    EXPECT_EQ(42, Object::NumberValue(*res));
    EXPECT_TRUE(ctx2->Global()
                    ->Set(ctx2, v8::String::NewFromUtf8Literal(isolate, "o"),
                          v8::Int32::New(isolate, 0))
                    .FromJust());
    ctx2->Exit();
    v8::Local<v8::Context>::New(isolate, ctx1)->Exit();
    ctx1p.Reset();
    isolate->ContextDisposedNotification(
        v8::ContextDependants::kSomeDependants);
  }
  {
    DisableConservativeStackScanningScopeForTesting no_stack_scanning(
        heap_instance);
    InvokeMemoryReducingMajorGCs();
  }
  EXPECT_EQ(2, NumberOfGlobalObjects());
  ctx2p.Reset();
  {
    DisableConservativeStackScanningScopeForTesting no_stack_scanning(
        heap_instance);
    InvokeMemoryReducingMajorGCs();
  }
  EXPECT_EQ(1, NumberOfGlobalObjects());
}

TEST_F(HeapTest, LeakNativeContextViaMapProto) {
  v8_flags.allow_natives_syntax = true;
  v8::Isolate* isolate = v8_isolate();
  Heap* heap_instance = heap();
  v8::HandleScope outer_scope(isolate);
  v8::Persistent<v8::Context> ctx1p;
  v8::Persistent<v8::Context> ctx2p;
  {
    v8::HandleScope scope(isolate);
    ctx1p.Reset(isolate, v8::Context::New(isolate));
    ctx2p.Reset(isolate, v8::Context::New(isolate));
    v8::Local<v8::Context>::New(isolate, ctx1p)->Enter();
  }

  {
    // In this test, we need to invoke GC without stack, otherwise some objects
    // may not be reclaimed because of conservative stack scanning.
    DisableConservativeStackScanningScopeForTesting no_stack_scanning(
        heap_instance);
    InvokeMemoryReducingMajorGCs();
  }
  EXPECT_EQ(3, NumberOfGlobalObjects());

  {
    v8::HandleScope inner_scope(isolate);
    v8::Local<v8::Context> ctx1 = v8::Local<v8::Context>::New(isolate, ctx1p);
    v8::Local<v8::Context> ctx2 = v8::Local<v8::Context>::New(isolate, ctx2p);
    RunJS(ctx1, "var v = { y: 42};");
    v8::Local<v8::Value> v =
        ctx1->Global()
            ->Get(ctx1, v8::String::NewFromUtf8Literal(isolate, "v"))
            .ToLocalChecked();
    ctx2->Enter();
    EXPECT_TRUE(ctx2->Global()
                    ->Set(ctx2, v8::String::NewFromUtf8Literal(isolate, "o"), v)
                    .FromJust());
    Handle<Object> res = RunJS(ctx2,
                               "function f() {"
                               "  var p = {x: 42};"
                               "  p.__proto__ = o;"
                               "  return p.x;"
                               "}"
                               "%PrepareFunctionForOptimization(f);"
                               "for (var i = 0; i < 10; ++i) f();"
                               "%OptimizeFunctionOnNextCall(f);"
                               "f();");
    EXPECT_EQ(42, Object::NumberValue(*res));
    EXPECT_TRUE(ctx2->Global()
                    ->Set(ctx2, v8::String::NewFromUtf8Literal(isolate, "o"),
                          v8::Int32::New(isolate, 0))
                    .FromJust());
    ctx2->Exit();
    v8::Local<v8::Context>::New(isolate, ctx1)->Exit();
    ctx1p.Reset();
    isolate->ContextDisposedNotification(
        v8::ContextDependants::kSomeDependants);
  }
  {
    DisableConservativeStackScanningScopeForTesting no_stack_scanning(
        heap_instance);
    InvokeMemoryReducingMajorGCs();
  }
  EXPECT_EQ(2, NumberOfGlobalObjects());
  ctx2p.Reset();
  {
    DisableConservativeStackScanningScopeForTesting no_stack_scanning(
        heap_instance);
    InvokeMemoryReducingMajorGCs();
  }
  EXPECT_EQ(1, NumberOfGlobalObjects());
}

TEST_F(HeapTest, OptimizedPretenuringNestedDoubleLiterals) {
  v8_flags.allow_natives_syntax = true;
  v8_flags.expose_gc = true;
  if (!i_isolate()->use_optimizer()) return;
  if (v8_flags.gc_global || v8_flags.stress_compaction ||
      v8_flags.stress_incremental_marking || v8_flags.single_generation ||
      v8_flags.stress_concurrent_allocation || v8_flags.scavenger_chaos_mode) {
    return;
  }
  v8::HandleScope scope(v8_isolate());
  const char* extension_names[] = {"v8/gc"};
  v8::ExtensionConfiguration extensions(1, extension_names);
  v8::Local<v8::Context> ctx = v8::Context::New(v8_isolate(), &extensions);
  v8::Context::Scope context_scope(ctx);
  ManualGCScope manual_gc_scope(i_isolate());
  GrowNewSpaceToMaximumCapacity();

  static const int kPretenureCreationCount =
      PretenuringHandler::GetMinMementoCountForTesting() + 1;

  auto source = base::OwnedVector<char>::NewForOverwrite(1024);
  base::SNPrintF(source.as_vector(),
                 "var number_elements = %d;"
                 "var elements = new Array(number_elements);"
                 "function f() {"
                 "  for (var i = 0; i < number_elements; i++) {"
                 "    elements[i] = [[1.1, 1.2, 1.3],[2.1, 2.2, 2.3]];"
                 "  }"
                 "  return elements[number_elements - 1];"
                 "};"
                 "%%PrepareFunctionForOptimization(f);"
                 "f(); gc({type: 'minor'});"
                 "f(); f();"
                 "%%OptimizeFunctionOnNextCall(f);"
                 "f();",
                 kPretenureCreationCount);

  Handle<JSObject> o = Cast<JSObject>(RunJS(ctx, source.begin()));

  DirectHandle<JSObject> double_array_handle_1 = Cast<JSObject>(
      JSReceiver::GetElement(i_isolate(), o, 0).ToHandleChecked());
  DirectHandle<JSObject> double_array_handle_2 = Cast<JSObject>(
      JSReceiver::GetElement(i_isolate(), o, 1).ToHandleChecked());

  EXPECT_TRUE(heap()->InOldSpace(*o));
  EXPECT_TRUE(heap()->InOldSpace(*double_array_handle_1));
  EXPECT_TRUE(heap()->InOldSpace(double_array_handle_1->elements()));
  EXPECT_TRUE(heap()->InOldSpace(*double_array_handle_2));
  EXPECT_TRUE(heap()->InOldSpace(double_array_handle_2->elements()));
}
TEST_F(HeapTest, OptimizedPretenuringNestedInObjectProperties) {
  v8_flags.allow_natives_syntax = true;
  v8_flags.expose_gc = true;
  if (!i_isolate()->use_optimizer()) return;
  if (v8_flags.gc_global || v8_flags.stress_compaction ||
      v8_flags.stress_incremental_marking || v8_flags.single_generation ||
      v8_flags.stress_concurrent_allocation || v8_flags.scavenger_chaos_mode) {
    return;
  }
  v8::HandleScope scope(v8_isolate());
  const char* extension_names[] = {"v8/gc"};
  v8::ExtensionConfiguration extensions(1, extension_names);
  v8::Local<v8::Context> ctx = v8::Context::New(v8_isolate(), &extensions);
  v8::Context::Scope context_scope(ctx);
  ManualGCScope manual_gc_scope(i_isolate());
  GrowNewSpaceToMaximumCapacity();

  static const int kPretenureCreationCount =
      PretenuringHandler::GetMinMementoCountForTesting() + 1;

  auto source = base::OwnedVector<char>::NewForOverwrite(1024);
  base::SNPrintF(
      source.as_vector(),
      "let number_elements = %d;"
      "let elements = new Array(number_elements);"
      "function f() {"
      "  for (let i = 0; i < number_elements; i++) {"
      "     let l =  {a: {b: {c: {d: {e: 2.2}, e: 3.3}, g: {h: 1.1}}}}; "
      "    elements[i] = l.a.b.c.d;"
      "  }"
      "  return elements[number_elements-1];"
      "};"
      "%%PrepareFunctionForOptimization(f);"
      "f(); gc({type: 'minor'}); gc({type: 'minor'});"
      "f(); f();"
      "%%OptimizeFunctionOnNextCall(f);"
      "f();",
      kPretenureCreationCount);

  Handle<JSObject> o = Cast<JSObject>(RunJS(ctx, source.begin()));

  EXPECT_TRUE(HeapLayout::InYoungGeneration(*o));
}
TEST_F(HeapTest, OptimizedPretenuringNestedObjectLiterals) {
  v8_flags.allow_natives_syntax = true;
  v8_flags.expose_gc = true;
  if (!i_isolate()->use_optimizer()) return;
  if (v8_flags.gc_global || v8_flags.stress_compaction ||
      v8_flags.stress_incremental_marking || v8_flags.single_generation ||
      v8_flags.stress_concurrent_allocation || v8_flags.scavenger_chaos_mode) {
    return;
  }
  v8::HandleScope scope(v8_isolate());
  const char* extension_names[] = {"v8/gc"};
  v8::ExtensionConfiguration extensions(1, extension_names);
  v8::Local<v8::Context> ctx = v8::Context::New(v8_isolate(), &extensions);
  v8::Context::Scope context_scope(ctx);
  ManualGCScope manual_gc_scope(i_isolate());
  GrowNewSpaceToMaximumCapacity();

  static const int kPretenureCreationCount =
      PretenuringHandler::GetMinMementoCountForTesting() + 1;

  auto source = base::OwnedVector<char>::NewForOverwrite(1024);
  base::SNPrintF(source.as_vector(),
                 "var number_elements = %d;"
                 "var elements = new Array(number_elements);"
                 "function f() {"
                 "  for (var i = 0; i < number_elements; i++) {"
                 "    elements[i] = [[{}, {}, {}],[{}, {}, {}]];"
                 "  }"
                 "  return elements[number_elements - 1];"
                 "};"
                 "%%PrepareFunctionForOptimization(f);"
                 "f(); gc({type: 'minor'});"
                 "f(); f();"
                 "%%OptimizeFunctionOnNextCall(f);"
                 "f();",
                 kPretenureCreationCount);

  Handle<JSObject> o = Cast<JSObject>(RunJS(ctx, source.begin()));

  DirectHandle<JSObject> int_array_handle_1 = Cast<JSObject>(
      JSReceiver::GetElement(i_isolate(), o, 0).ToHandleChecked());
  DirectHandle<JSObject> int_array_handle_2 = Cast<JSObject>(
      JSReceiver::GetElement(i_isolate(), o, 1).ToHandleChecked());

  EXPECT_TRUE(heap()->InOldSpace(*o));
  EXPECT_TRUE(heap()->InOldSpace(*int_array_handle_1));
  EXPECT_TRUE(heap()->InOldSpace(int_array_handle_1->elements()));
  EXPECT_TRUE(heap()->InOldSpace(*int_array_handle_2));
  EXPECT_TRUE(heap()->InOldSpace(int_array_handle_2->elements()));
}
TEST_F(HeapTest, OptimizedPretenuringMixedInObjectProperties) {
  v8_flags.allow_natives_syntax = true;
  v8_flags.expose_gc = true;
  if (!i_isolate()->use_optimizer()) return;
  if (v8_flags.gc_global || v8_flags.stress_compaction ||
      v8_flags.stress_incremental_marking || v8_flags.single_generation ||
      v8_flags.stress_concurrent_allocation || v8_flags.scavenger_chaos_mode) {
    return;
  }
  v8::HandleScope scope(v8_isolate());
  const char* extension_names[] = {"v8/gc"};
  v8::ExtensionConfiguration extensions(1, extension_names);
  v8::Local<v8::Context> ctx = v8::Context::New(v8_isolate(), &extensions);
  v8::Context::Scope context_scope(ctx);
  ManualGCScope manual_gc_scope(i_isolate());
  GrowNewSpaceToMaximumCapacity();

  static const int kPretenureCreationCount =
      PretenuringHandler::GetMinMementoCountForTesting() + 1;

  auto source = base::OwnedVector<char>::NewForOverwrite(1024);
  base::SNPrintF(source.as_vector(),
                 "var number_elements = %d;"
                 "var elements = new Array(number_elements);"
                 "function f() {"
                 "  for (var i = 0; i < number_elements; i++) {"
                 "    elements[i] = {a: {c: 2.2, d: {}}, b: 1.1};"
                 "  }"
                 "  return elements[number_elements - 1];"
                 "};"
                 "%%PrepareFunctionForOptimization(f);"
                 "f(); gc({type: 'minor'});"
                 "f(); f();"
                 "%%OptimizeFunctionOnNextCall(f);"
                 "f();",
                 kPretenureCreationCount);

  Handle<JSObject> o = Cast<JSObject>(RunJS(ctx, source.begin()));

  EXPECT_TRUE(heap()->InOldSpace(*o));
  FieldIndex idx1 = FieldIndex::ForPropertyIndex(o->map(), 0);
  FieldIndex idx2 = FieldIndex::ForPropertyIndex(o->map(), 1);
  EXPECT_TRUE(heap()->InOldSpace(o->RawFastPropertyAt(idx1)));
  EXPECT_TRUE(heap()->InOldSpace(o->RawFastPropertyAt(idx2)));

  Tagged<JSObject> inner_object = Cast<JSObject>(o->RawFastPropertyAt(idx1));
  EXPECT_TRUE(heap()->InOldSpace(inner_object));
  EXPECT_TRUE(heap()->InOldSpace(inner_object->RawFastPropertyAt(idx1)));
  EXPECT_TRUE(heap()->InOldSpace(inner_object->RawFastPropertyAt(idx2)));
}

TEST_F(HeapTest, OptimizedPretenuringDoubleArrayLiterals) {
  v8_flags.allow_natives_syntax = true;
  v8_flags.expose_gc = true;
  if (!i_isolate()->use_optimizer()) return;
  if (v8_flags.gc_global || v8_flags.stress_compaction ||
      v8_flags.stress_incremental_marking || v8_flags.single_generation ||
      v8_flags.stress_concurrent_allocation || v8_flags.scavenger_chaos_mode) {
    return;
  }
  v8::HandleScope scope(v8_isolate());
  const char* extension_names[] = {"v8/gc"};
  v8::ExtensionConfiguration extensions(1, extension_names);
  v8::Local<v8::Context> ctx = v8::Context::New(v8_isolate(), &extensions);
  v8::Context::Scope context_scope(ctx);
  ManualGCScope manual_gc_scope(i_isolate());
  GrowNewSpaceToMaximumCapacity();

  static const int kPretenureCreationCount =
      PretenuringHandler::GetMinMementoCountForTesting() + 1;

  auto source = base::OwnedVector<char>::NewForOverwrite(1024);
  base::SNPrintF(source.as_vector(),
                 "var number_elements = %d;"
                 "var elements = new Array(number_elements);"
                 "function f() {"
                 "  for (var i = 0; i < number_elements; i++) {"
                 "    elements[i] = [1.1, 2.2, 3.3];"
                 "  }"
                 "  return elements[number_elements - 1];"
                 "};"
                 "%%PrepareFunctionForOptimization(f);"
                 "f(); gc({type: 'minor'});"
                 "f(); f();"
                 "%%OptimizeFunctionOnNextCall(f);"
                 "f();",
                 kPretenureCreationCount);

  Handle<JSObject> o = Cast<JSObject>(RunJS(ctx, source.begin()));

  EXPECT_TRUE(heap()->InOldSpace(o->elements()));
  EXPECT_TRUE(heap()->InOldSpace(*o));
}

TEST_F(HeapTest, OptimizedPretenuringNestedMixedArrayLiterals) {
  v8_flags.allow_natives_syntax = true;
  v8_flags.expose_gc = true;
  if (!i_isolate()->use_optimizer()) return;
  if (v8_flags.gc_global || v8_flags.stress_compaction ||
      v8_flags.stress_incremental_marking || v8_flags.single_generation ||
      v8_flags.stress_concurrent_allocation || v8_flags.scavenger_chaos_mode) {
    return;
  }
  v8::HandleScope scope(v8_isolate());
  const char* extension_names[] = {"v8/gc"};
  v8::ExtensionConfiguration extensions(1, extension_names);
  v8::Local<v8::Context> ctx = v8::Context::New(v8_isolate(), &extensions);
  v8::Context::Scope context_scope(ctx);
  ManualGCScope manual_gc_scope(i_isolate());
  GrowNewSpaceToMaximumCapacity();

  static const int kPretenureCreationCount =
      PretenuringHandler::GetMinMementoCountForTesting() + 1;

  auto source = base::OwnedVector<char>::NewForOverwrite(1024);
  base::SNPrintF(source.as_vector(),
                 "var number_elements = %d;"
                 "var elements = new Array(number_elements);"
                 "function f() {"
                 "  for (var i = 0; i < number_elements; i++) {"
                 "    elements[i] = [[{}, {}, {}], [1.1, 2.2, 3.3]];"
                 "  }"
                 "  return elements[number_elements - 1];"
                 "};"
                 "%%PrepareFunctionForOptimization(f);"
                 "f(); gc({type: 'minor'});"
                 "f(); f();"
                 "%%OptimizeFunctionOnNextCall(f);"
                 "f();",
                 kPretenureCreationCount);

  Handle<JSObject> o = Cast<JSObject>(RunJS(ctx, source.begin()));

  v8::Local<v8::Value> int_array = v8::Utils::ToLocal(o)
                                       ->ToObject(ctx)
                                       .ToLocalChecked()
                                       ->Get(ctx, NewString("0"))
                                       .ToLocalChecked();
  i::DirectHandle<JSObject> int_array_handle = i::Cast<JSObject>(
      v8::Utils::OpenDirectHandle(*v8::Local<v8::Object>::Cast(int_array)));
  v8::Local<v8::Value> double_array = v8::Utils::ToLocal(o)
                                          ->ToObject(ctx)
                                          .ToLocalChecked()
                                          ->Get(ctx, NewString("1"))
                                          .ToLocalChecked();
  i::DirectHandle<JSObject> double_array_handle = i::Cast<JSObject>(
      v8::Utils::OpenDirectHandle(*v8::Local<v8::Object>::Cast(double_array)));

  EXPECT_TRUE(heap()->InOldSpace(*o));
  EXPECT_TRUE(heap()->InOldSpace(*int_array_handle));
  EXPECT_TRUE(heap()->InOldSpace(int_array_handle->elements()));
  EXPECT_TRUE(heap()->InOldSpace(*double_array_handle));
  EXPECT_TRUE(heap()->InOldSpace(double_array_handle->elements()));
}

TEST_F(HeapTest, IncrementalMarkingPreservesMonomorphicConstructor) {
  if (!v8_flags.incremental_marking) return;
  v8_flags.allow_natives_syntax = true;
  v8::HandleScope scope(v8_isolate());
  v8::Local<v8::Context> ctx = v8_isolate()->GetCurrentContext();
  // Prepare function f that contains a monomorphic IC for object
  // originating from the same native context.
  RunJS(ctx,
        "function fun() { this.x = 1; };"
        "function f(o) { return new o(); }"
        "%EnsureFeedbackVectorForFunction(f);"
        "f(fun); f(fun);");
  DirectHandle<JSFunction> f = Cast<JSFunction>(
      v8::Utils::OpenDirectHandle(*v8::Local<v8::Function>::Cast(
          ctx->Global()
              ->Get(ctx, v8::String::NewFromUtf8Literal(v8_isolate(), "f"))
              .ToLocalChecked())));

  DirectHandle<FeedbackVector> vector(f->feedback_vector(), i_isolate());
  EXPECT_TRUE(vector->Get(FeedbackSlot(0)).IsWeakOrCleared());

  SimulateIncrementalMarking();
  InvokeMajorGC();

  EXPECT_TRUE(vector->Get(FeedbackSlot(0)).IsWeakOrCleared());
}
namespace {

template <typename T>
DirectHandle<SharedFunctionInfo> GetSharedFunctionInfo(
    v8::Local<T> function_or_script, Isolate* isolate) {
  DirectHandle<JSFunction> i_function =
      Cast<JSFunction>(v8::Utils::OpenDirectHandle(*function_or_script));
  return direct_handle(i_function->shared(), isolate);
}

template <typename T>
void AgeBytecode(v8::Local<T> function_or_script, Isolate* isolate) {
  DirectHandle<SharedFunctionInfo> shared =
      GetSharedFunctionInfo(function_or_script, isolate);
  EXPECT_TRUE(shared->HasBytecodeArray());
  SharedFunctionInfo::EnsureOldForTesting(*shared);
}

void RunCompilationCacheRegenerationTest(HeapTest* test, bool retain_root_sfi,
                                         bool flush_root_sfi,
                                         bool flush_eager_sfi) {
  // If the compilation cache is turned off, this test is invalid.
  if (!v8_flags.compilation_cache) {
    return;
  }

  // Skip test if code flushing was disabled.
  if (!v8_flags.flush_bytecode ||
      (v8_flags.always_sparkplug && !v8_flags.flush_baseline_code)) {
    return;
  }

  Isolate* isolate = test->i_isolate();
  Heap* heap = test->heap();
  v8::Isolate* v8_isolate = test->v8_isolate();

  const char* source =
      "({"
      "  lazyFunction: function () {"
      "    var x = 42;"
      "    var y = 42;"
      "    var z = x + y;"
      "  },"
      "  eagerFunction: (function () {"
      "    var x = 43;"
      "    var y = 43;"
      "    var z = x + y;"
      "  })"
      "})";

  v8::Global<v8::Script> outer_function;
  v8::Global<v8::Function> lazy_function;
  v8::Global<v8::Function> eager_function;

  {
    v8::HandleScope scope(v8_isolate);
    v8::Local<v8::Context> context = test->context();
    v8::Local<v8::String> source_str =
        v8::String::NewFromUtf8(v8_isolate, source).ToLocalChecked();
    v8::Local<v8::Script> script =
        v8::Script::Compile(context, source_str).ToLocalChecked();
    outer_function.Reset(v8_isolate, script);

    // Even though the script has not executed, it should already be parsed.
    DirectHandle<SharedFunctionInfo> script_sfi =
        GetSharedFunctionInfo(script, isolate);
    EXPECT_TRUE(script_sfi->is_compiled());

    v8::Local<v8::Value> result = script->Run(context).ToLocalChecked();

    // Now that the script has run, we can get references to the inner
    // functions, and verify that the eager parsing heuristics are behaving as
    // expected.
    v8::Local<v8::Object> result_obj =
        result->ToObject(context).ToLocalChecked();
    v8::Local<v8::Value> lazy_function_value =
        result_obj
            ->GetRealNamedProperty(
                context, v8::String::NewFromUtf8(v8_isolate, "lazyFunction")
                             .ToLocalChecked())
            .ToLocalChecked();
    EXPECT_TRUE(lazy_function_value->IsFunction());
    EXPECT_FALSE(
        GetSharedFunctionInfo(lazy_function_value, isolate)->is_compiled());
    lazy_function.Reset(v8_isolate, lazy_function_value.As<v8::Function>());
    v8::Local<v8::Value> eager_function_value =
        result_obj
            ->GetRealNamedProperty(
                context, v8::String::NewFromUtf8(v8_isolate, "eagerFunction")
                             .ToLocalChecked())
            .ToLocalChecked();
    EXPECT_TRUE(eager_function_value->IsFunction());
    eager_function.Reset(v8_isolate, eager_function_value.As<v8::Function>());
    EXPECT_TRUE(
        GetSharedFunctionInfo(eager_function_value, isolate)->is_compiled());
  }

  {
    v8::HandleScope scope(v8_isolate);

    // Progress code age until it's old and ready for GC.
    if (flush_root_sfi) {
      v8::Local<v8::Script> outer_function_value =
          outer_function.Get(v8_isolate);
      AgeBytecode(outer_function_value, isolate);
    }
    if (flush_eager_sfi) {
      v8::Local<v8::Function> eager_function_value =
          eager_function.Get(v8_isolate);
      AgeBytecode(eager_function_value, isolate);
    }
    if (!retain_root_sfi) {
      outer_function.Reset();
    }
  }

  {
    // In these tests, we need to invoke GC without stack, otherwise some
    // objects may not be reclaimed because of conservative stack scanning.
    DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap);

    if (v8_flags.stress_incremental_marking) {
      // This GC finishes incremental marking if it is already running. If
      // incremental marking was already running we would not flush the code
      // right away.
      test->InvokeMajorGC();
    }

    // The first GC performs code flushing.
    test->InvokeMajorGC();
    // The second GC clears the entry from the compilation cache.
    test->InvokeMajorGC();
  }

  // The root SharedFunctionInfo can be retained either by a Global in this
  // function or by the compilation cache.
  bool root_sfi_should_still_exist = retain_root_sfi || !flush_root_sfi;

  {
    v8::HandleScope scope(v8_isolate);

    // The lazy function should still not be compiled.
    DirectHandle<SharedFunctionInfo> lazy_sfi =
        GetSharedFunctionInfo(lazy_function.Get(v8_isolate), isolate);
    EXPECT_FALSE(lazy_sfi->is_compiled());

    // The eager function may have had its bytecode flushed.
    DirectHandle<SharedFunctionInfo> eager_sfi =
        GetSharedFunctionInfo(eager_function.Get(v8_isolate), isolate);
    EXPECT_EQ(!flush_eager_sfi, eager_sfi->is_compiled());

    // Check whether the root SharedFunctionInfo is still reachable from the
    // Script.
    DirectHandle<Script> script(Cast<Script>(lazy_sfi->script()), isolate);
    bool root_sfi_still_exists = false;
    Tagged<MaybeObject> maybe_root_sfi =
        script->infos()->get(kFunctionLiteralIdTopLevel);
    if (Tagged<HeapObject> sfi_or_undefined;
        maybe_root_sfi.GetHeapObject(&sfi_or_undefined)) {
      root_sfi_still_exists = !IsUndefined(sfi_or_undefined);
    }
    EXPECT_EQ(root_sfi_should_still_exist, root_sfi_still_exists);
  }

  {
    // Run the script again and check that no SharedFunctionInfos were
    // duplicated, and that the expected ones were compiled.
    v8::HandleScope scope(v8_isolate);
    v8::Local<v8::Context> context = test->context();
    v8::Local<v8::String> source_str =
        v8::String::NewFromUtf8(v8_isolate, source).ToLocalChecked();
    v8::Local<v8::Script> script =
        v8::Script::Compile(context, source_str).ToLocalChecked();

    // The script should be compiled by now.
    DirectHandle<SharedFunctionInfo> script_sfi =
        GetSharedFunctionInfo(script, isolate);
    EXPECT_TRUE(script_sfi->is_compiled());

    // This compilation should not have created a new root SharedFunctionInfo if
    // one already existed.
    if (retain_root_sfi) {
      DirectHandle<SharedFunctionInfo> old_script_sfi =
          GetSharedFunctionInfo(outer_function.Get(v8_isolate), isolate);
      EXPECT_EQ(*old_script_sfi, *script_sfi);
    }

    DirectHandle<SharedFunctionInfo> old_lazy_sfi =
        GetSharedFunctionInfo(lazy_function.Get(v8_isolate), isolate);
    EXPECT_FALSE(old_lazy_sfi->is_compiled());

    // The only way for the eager function to be uncompiled at this point is if
    // it was flushed but the root function was not.
    DirectHandle<SharedFunctionInfo> old_eager_sfi =
        GetSharedFunctionInfo(eager_function.Get(v8_isolate), isolate);
    EXPECT_EQ(!(flush_eager_sfi && !flush_root_sfi),
              old_eager_sfi->is_compiled());

    v8::Local<v8::Value> result = script->Run(context).ToLocalChecked();

    // Check that both functions reused the existing SharedFunctionInfos.
    v8::Local<v8::Object> result_obj =
        result->ToObject(context).ToLocalChecked();
    v8::Local<v8::Value> lazy_function_value =
        result_obj
            ->GetRealNamedProperty(
                context, v8::String::NewFromUtf8(v8_isolate, "lazyFunction")
                             .ToLocalChecked())
            .ToLocalChecked();
    EXPECT_TRUE(lazy_function_value->IsFunction());
    DirectHandle<SharedFunctionInfo> lazy_sfi =
        GetSharedFunctionInfo(lazy_function_value, isolate);
    EXPECT_EQ(*old_lazy_sfi, *lazy_sfi);
    v8::Local<v8::Value> eager_function_value =
        result_obj
            ->GetRealNamedProperty(
                context, v8::String::NewFromUtf8(v8_isolate, "eagerFunction")
                             .ToLocalChecked())
            .ToLocalChecked();
    EXPECT_TRUE(eager_function_value->IsFunction());
    DirectHandle<SharedFunctionInfo> eager_sfi =
        GetSharedFunctionInfo(eager_function_value, isolate);
    EXPECT_EQ(*old_eager_sfi, *eager_sfi);
  }
}

}  // namespace

TEST_F(HeapTest, CompilationCacheRegeneration0) {
  RunCompilationCacheRegenerationTest(this, false, false, false);
}

TEST_F(HeapTest, CompilationCacheRegeneration1) {
  RunCompilationCacheRegenerationTest(this, false, false, true);
}

TEST_F(HeapTest, CompilationCacheRegeneration2) {
  RunCompilationCacheRegenerationTest(this, false, true, false);
}

TEST_F(HeapTest, CompilationCacheRegeneration3) {
  RunCompilationCacheRegenerationTest(this, false, true, true);
}

TEST_F(HeapTest, CompilationCacheRegeneration4) {
  RunCompilationCacheRegenerationTest(this, true, false, false);
}

TEST_F(HeapTest, CompilationCacheRegeneration5) {
  RunCompilationCacheRegenerationTest(this, true, false, true);
}

TEST_F(HeapTest, CompilationCacheRegeneration6) {
  RunCompilationCacheRegenerationTest(this, true, true, false);
}

TEST_F(HeapTest, CompilationCacheRegeneration7) {
  RunCompilationCacheRegenerationTest(this, true, true, true);
}

void CheckWeakness(HeapTest* test, const char* source) {
  // ManualGCScope is necessary to finalize any active incremental marking and
  // disable concurrent/stress marking. Otherwise, under flags like
  // --stress-incremental-marking, the isolate (initialized before this test
  // runs) may already have active marking retaining the object, preventing the
  // weak reference from being cleared.
  ManualGCScope manual_gc_scope(test->isolate());
  v8_flags.stress_compaction = false;
  v8_flags.stress_incremental_marking = false;
  v8_flags.allow_natives_syntax = true;

  v8::Global<v8::Object> garbage;
  {
    v8::HandleScope scope(test->v8_isolate());
    HandleScope i_scope(test->i_isolate());
    v8::Local<v8::Object> obj =
        v8::Utils::ToLocal(test->RunJS<JSObject>(source));
    garbage.Reset(test->v8_isolate(), obj);
  }
  garbage.SetWeak();
  {
    DisableConservativeStackScanningScopeForTesting no_stack_scanning(
        test->heap());
    test->InvokeMajorGC();
  }
  EXPECT_TRUE(garbage.IsEmpty());
}

TEST_F(HeapTest, WeakFunctionInConstructor) {
  // ManualGCScope is necessary to finalize any active incremental marking and
  // disable concurrent/stress marking so that the weak reference can be
  // cleared.
  ManualGCScope manual_gc_scope(isolate());
  v8_flags.stress_compaction = false;
  v8_flags.stress_incremental_marking = false;
  v8_flags.allow_natives_syntax = true;

  RunJS(
      "function createObj(obj) {"
      "  return new obj();"
      "}");
  DirectHandle<JSFunction> createObj = RunJS<JSFunction>("createObj");

  v8::Global<v8::Object> garbage;
  {
    v8::HandleScope scope(v8_isolate());
    HandleScope i_scope(i_isolate());
    const char* source =
        " (function() {"
        "   function hat() { this.x = 5; }"
        "   %EnsureFeedbackVectorForFunction(hat);"
        "   %EnsureFeedbackVectorForFunction(createObj);"
        "   createObj(hat);"
        "   createObj(hat);"
        "   return hat;"
        " })();";
    v8::Local<v8::Object> obj = v8::Utils::ToLocal(RunJS<JSObject>(source));
    garbage.Reset(v8_isolate(), obj);
  }
  garbage.SetWeak();
  {
    // In this test, we need to invoke GC without stack, otherwise some objects
    // may not be reclaimed because of conservative stack scanning.
    DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap());
    InvokeMajorGC();
  }
  EXPECT_TRUE(garbage.IsEmpty());

  // We've determined the constructor in createObj has had its weak cell
  // cleared. Now, verify that one additional call with a new function
  // allows monomorphicity.
  DirectHandle<FeedbackVector> feedback_vector(createObj->feedback_vector(),
                                               isolate());
  for (int i = 0; i < 20; i++) {
    Tagged<MaybeObject> slot_value = feedback_vector->Get(FeedbackSlot(0));
    EXPECT_TRUE(slot_value.IsWeakOrCleared());
    if (slot_value.IsCleared()) break;
    DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap());
    InvokeMajorGC();
  }

  Tagged<MaybeObject> slot_value = feedback_vector->Get(FeedbackSlot(0));
  EXPECT_TRUE(slot_value.IsCleared());
  RunJS(
      "function coat() { this.x = 6; }"
      "createObj(coat);");
  slot_value = feedback_vector->Get(FeedbackSlot(0));
  EXPECT_TRUE(slot_value.IsWeak());
}

TEST_F(HeapTest, WeakMapInMonomorphicLoadIC) {
  CheckWeakness(this,
                "function loadIC(obj) {"
                "  return obj.name;"
                "}"
                "%EnsureFeedbackVectorForFunction(loadIC);"
                " (function() {"
                "   var proto = {'name' : 'weak'};"
                "   var obj = Object.create(proto);"
                "   loadIC(obj);"
                "   loadIC(obj);"
                "   loadIC(obj);"
                "   return proto;"
                " })();");
}

TEST_F(HeapTest, WeakMapInPolymorphicLoadIC) {
  CheckWeakness(this,
                "function loadIC(obj) {"
                "  return obj.name;"
                "}"
                "%EnsureFeedbackVectorForFunction(loadIC);"
                " (function() {"
                "   var proto = {'name' : 'weak'};"
                "   var obj = Object.create(proto);"
                "   loadIC(obj);"
                "   loadIC(obj);"
                "   loadIC(obj);"
                "   var poly = Object.create(proto);"
                "   poly.x = true;"
                "   loadIC(poly);"
                "   return proto;"
                " })();");
}

TEST_F(HeapTest, WeakMapInMonomorphicKeyedLoadIC) {
  CheckWeakness(this,
                "function keyedLoadIC(obj, field) {"
                "  return obj[field];"
                "}"
                "%EnsureFeedbackVectorForFunction(keyedLoadIC);"
                " (function() {"
                "   var proto = {'name' : 'weak'};"
                "   var obj = Object.create(proto);"
                "   keyedLoadIC(obj, 'name');"
                "   keyedLoadIC(obj, 'name');"
                "   keyedLoadIC(obj, 'name');"
                "   return proto;"
                " })();");
}

TEST_F(HeapTest, WeakMapInPolymorphicKeyedLoadIC) {
  CheckWeakness(this,
                "function keyedLoadIC(obj, field) {"
                "  return obj[field];"
                "}"
                "%EnsureFeedbackVectorForFunction(keyedLoadIC);"
                " (function() {"
                "   var proto = {'name' : 'weak'};"
                "   var obj = Object.create(proto);"
                "   keyedLoadIC(obj, 'name');"
                "   keyedLoadIC(obj, 'name');"
                "   keyedLoadIC(obj, 'name');"
                "   var poly = Object.create(proto);"
                "   poly.x = true;"
                "   keyedLoadIC(poly, 'name');"
                "   return proto;"
                " })();");
}

TEST_F(HeapTest, WeakMapInMonomorphicStoreIC) {
  CheckWeakness(this,
                "function storeIC(obj, value) {"
                "  obj.name = value;"
                "}"
                "%EnsureFeedbackVectorForFunction(storeIC);"
                " (function() {"
                "   var proto = {'name' : 'weak'};"
                "   var obj = Object.create(proto);"
                "   storeIC(obj, 'x');"
                "   storeIC(obj, 'x');"
                "   storeIC(obj, 'x');"
                "   return proto;"
                " })();");
}

TEST_F(HeapTest, WeakMapInPolymorphicStoreIC) {
  CheckWeakness(this,
                "function storeIC(obj, value) {"
                "  obj.name = value;"
                "}"
                "%EnsureFeedbackVectorForFunction(storeIC);"
                " (function() {"
                "   var proto = {'name' : 'weak'};"
                "   var obj = Object.create(proto);"
                "   storeIC(obj, 'x');"
                "   storeIC(obj, 'x');"
                "   storeIC(obj, 'x');"
                "   var poly = Object.create(proto);"
                "   poly.x = true;"
                "   storeIC(poly, 'x');"
                "   return proto;"
                " })();");
}

TEST_F(HeapTest, WeakMapInMonomorphicKeyedStoreIC) {
  CheckWeakness(this,
                "function keyedStoreIC(obj, field, value) {"
                "  obj[field] = value;"
                "}"
                "%EnsureFeedbackVectorForFunction(keyedStoreIC);"
                " (function() {"
                "   var proto = {'name' : 'weak'};"
                "   var obj = Object.create(proto);"
                "   keyedStoreIC(obj, 'x');"
                "   keyedStoreIC(obj, 'x');"
                "   keyedStoreIC(obj, 'x');"
                "   return proto;"
                " })();");
}

TEST_F(HeapTest, WeakMapInPolymorphicKeyedStoreIC) {
  CheckWeakness(this,
                "function keyedStoreIC(obj, field, value) {"
                "  obj[field] = value;"
                "}"
                "%EnsureFeedbackVectorForFunction(keyedStoreIC);"
                " (function() {"
                "   var proto = {'name' : 'weak'};"
                "   var obj = Object.create(proto);"
                "   keyedStoreIC(obj, 'x');"
                "   keyedStoreIC(obj, 'x');"
                "   keyedStoreIC(obj, 'x');"
                "   var poly = Object.create(proto);"
                "   poly.x = true;"
                "   keyedStoreIC(poly, 'x');"
                "   return proto;"
                " })();");
}

TEST_F(HeapTest, WeakMapInMonomorphicCompareNilIC) {
  v8_flags.allow_natives_syntax = true;
  CheckWeakness(this,
                "function compareNilIC(obj) {"
                "  return obj == null;"
                "}"
                "%EnsureFeedbackVectorForFunction(compareNilIC);"
                " (function() {"
                "   var proto = {'name' : 'weak'};"
                "   var obj = Object.create(proto);"
                "   compareNilIC(obj);"
                "   compareNilIC(obj);"
                "   compareNilIC(obj);"
                "   return proto;"
                " })();");
}

TEST_F(HeapTest, TestSizeOfRegExpCode) {
  if (!v8_flags.regexp_optimization) return;
  v8_flags.stress_concurrent_allocation = false;

  v8::HandleScope scope(v8_isolate());

  EXPECT_EQ(static_cast<int>(RegExp::kMaxOptimizedPatternLength), 20 * KB);

  // Compile a regexp that is much larger if we are using regexp optimizations.
  RunJS(
      "var reg_exp_source = '(?:a|bc|def|ghij|klmno|pqrstu)';"
      "var half_size_reg_exp;"
      "while (reg_exp_source.length < 20 * 1024) {"
      "  half_size_reg_exp = reg_exp_source;"
      "  reg_exp_source = reg_exp_source + reg_exp_source;"
      "}"
      // Flatten string.
      "reg_exp_source.match(/f/);");

  {
    // In this test, we need to invoke GC without stack, otherwise some objects
    // may not be reclaimed because of conservative stack scanning.
    DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap());
    // Get initial heap size after several full GCs, which will stabilize
    // the heap size and return with sweeping finished completely.
    InvokeMemoryReducingMajorGCs();
    if (heap()->sweeping_in_progress()) {
      heap()->EnsureSweepingCompleted(
          Heap::SweepingForcedFinalizationMode::kV8Only,
          CompleteSweepingReason::kTesting);
    }
  }
  int initial_size = static_cast<int>(heap()->SizeOfObjects());

  RunJS("'foo'.match(reg_exp_source);");
  {
    DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap());
    InvokeMemoryReducingMajorGCs();
  }
  int size_with_regexp = static_cast<int>(heap()->SizeOfObjects());

  RunJS("'foo'.match(half_size_reg_exp);");
  {
    DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap());
    InvokeMemoryReducingMajorGCs();
  }
  int size_with_optimized_regexp = static_cast<int>(heap()->SizeOfObjects());

  int size_of_regexp_code = size_with_regexp - initial_size;

  // On some platforms the debug-code flag causes huge amounts of regexp code
  // to be emitted, breaking this test.
  if (!v8_flags.debug_code) {
    EXPECT_LE(size_of_regexp_code, 1 * MB);
  }

  // Small regexp is half the size, but compiles to more than twice the code
  // due to the optimization steps.
  EXPECT_GE(size_with_optimized_regexp,
            size_with_regexp + size_of_regexp_code * 2);
}

TEST_F(HeapTest, TestSizeOfObjects) {
  v8_flags.stress_concurrent_allocation = false;

  // Disable LAB, such that calculations with SizeOfObjects() and object size
  // are correct.
  heap()->DisableInlineAllocation();

  // Get initial heap size after several full GCs, which will stabilize
  // the heap size and return with sweeping finished completely.
  {
    // In this test, we need to invoke GC without stack, otherwise some objects
    // may not be reclaimed because of conservative stack scanning.
    DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap());
    InvokeMemoryReducingMajorGCs();
    if (heap()->sweeping_in_progress()) {
      heap()->EnsureSweepingCompleted(
          Heap::SweepingForcedFinalizationMode::kV8Only,
          CompleteSweepingReason::kTesting);
    }
  }
  int initial_size = static_cast<int>(heap()->SizeOfObjects());

  {
    HandleScope scope(i_isolate());
    // Allocate objects on several different old-space pages so that
    // concurrent sweeper threads will be busy sweeping the old space on
    // subsequent GC runs.
    AlwaysAllocateScopeForTesting always_allocate(heap());
    int filler_size = static_cast<int>(FixedArray::SizeFor(8192));
    for (int i = 1; i <= 100; i++) {
      i_isolate()->factory()->NewFixedArray(8192, AllocationType::kOld);
      EXPECT_EQ(initial_size + i * filler_size,
                static_cast<int>(heap()->SizeOfObjects()));
    }
  }

  // The heap size should go back to initial size after a full GC, even
  // though sweeping didn't finish yet.
  {
    DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap());
    InvokeMajorGC();
  }
  // Normally sweeping would not be complete here, but no guarantees.
  EXPECT_EQ(initial_size, static_cast<int>(heap()->SizeOfObjects()));
  // Waiting for sweeper threads should not change heap size.
  if (heap()->sweeping_in_progress()) {
    DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap());
    heap()->EnsureSweepingCompleted(
        Heap::SweepingForcedFinalizationMode::kV8Only,
        CompleteSweepingReason::kTesting);
  }
  EXPECT_EQ(initial_size, static_cast<int>(heap()->SizeOfObjects()));
}

TEST_F(HeapTest, TestAlignmentCalculations) {
  // Maximum fill amounts are consistent.
  int maximum_double_misalignment = kDoubleSize - kTaggedSize;
  int max_word_fill = MainAllocator::GetMaximumFillToAlign(kTaggedAligned);
  EXPECT_EQ(0, max_word_fill);
  int max_double_fill = MainAllocator::GetMaximumFillToAlign(kDoubleAligned);
  EXPECT_EQ(maximum_double_misalignment, max_double_fill);
  int max_double_unaligned_fill =
      MainAllocator::GetMaximumFillToAlign(kDoubleUnaligned);
  EXPECT_EQ(maximum_double_misalignment, max_double_unaligned_fill);

  Address base = kNullAddress;
  int fill = 0;

  // Word alignment never requires fill.
  fill = MainAllocator::GetFillToAlign(base, kTaggedAligned);
  EXPECT_EQ(0, fill);
  fill = MainAllocator::GetFillToAlign(base + kTaggedSize, kTaggedAligned);
  EXPECT_EQ(0, fill);

  // No fill is required when address is double aligned.
  fill = MainAllocator::GetFillToAlign(base, kDoubleAligned);
  EXPECT_EQ(0, fill);
  // Fill is required if address is not double aligned.
  fill = MainAllocator::GetFillToAlign(base + kTaggedSize, kDoubleAligned);
  EXPECT_EQ(maximum_double_misalignment, fill);
  // kDoubleUnaligned has the opposite fill amounts.
  fill = MainAllocator::GetFillToAlign(base, kDoubleUnaligned);
  EXPECT_EQ(maximum_double_misalignment, fill);
  fill = MainAllocator::GetFillToAlign(base + kTaggedSize, kDoubleUnaligned);
  EXPECT_EQ(0, fill);
}

TEST_F(HeapTest, TestAlignedAllocation) {
  if (v8_flags.single_generation) return;
  // Double misalignment is 4 on 32-bit platforms or when pointer compression
  // is enabled, 0 on 64-bit ones when pointer compression is disabled.
  const intptr_t double_misalignment = kDoubleSize - kTaggedSize;
  Address start;
  Tagged<HeapObject> obj;
  Tagged<HeapObject> filler;
  if (double_misalignment) {
    MainAllocator* allocator = heap()->allocator()->new_space_allocator();

    // Make one allocation to force allocating an allocation area. Using
    // kDoubleSize to not change space alignment
    AllocationResult dummy =
        allocator->AllocateRaw(SafeHeapObjectSize(kDoubleSize), kDoubleAligned,
                               AllocationOrigin::kRuntime, AllocationHint());
    ASSERT_FALSE(dummy.IsFailure());
    heap()->CreateFillerObjectAt(dummy.ToObjectChecked().address(),
                                 kDoubleSize);

    // Allocate a pointer sized object that must be double aligned at an
    // aligned address.
    start = allocator->AlignTopForTesting(kDoubleAligned, 0);
    obj = AllocateAligned(heap(), allocator, kTaggedSize, kDoubleAligned);
    EXPECT_TRUE(IsAligned(obj.address(), kDoubleAlignment));
    // There is no filler.
    EXPECT_EQ(start, obj.address());

    // Allocate a second pointer sized object that must be double aligned at an
    // unaligned address.
    start = allocator->AlignTopForTesting(kDoubleAligned, kTaggedSize);
    obj = AllocateAligned(heap(), allocator, kTaggedSize, kDoubleAligned);
    EXPECT_TRUE(IsAligned(obj.address(), kDoubleAlignment));
    // There is a filler object before the object.
    filler = HeapObject::FromAddress(start);
    EXPECT_NE(obj, filler);
    EXPECT_TRUE(IsFreeSpaceOrFiller(filler));
    EXPECT_EQ(filler->Size(), kTaggedSize);
    EXPECT_EQ(start + double_misalignment, obj.address());

    // Similarly for kDoubleUnaligned.
    start = allocator->AlignTopForTesting(kDoubleUnaligned, 0);
    obj = AllocateAligned(heap(), allocator, kTaggedSize, kDoubleUnaligned);
    EXPECT_TRUE(IsAligned(obj.address() + kTaggedSize, kDoubleAlignment));
    EXPECT_EQ(start, obj.address());

    start = allocator->AlignTopForTesting(kDoubleUnaligned, kTaggedSize);
    obj = AllocateAligned(heap(), allocator, kTaggedSize, kDoubleUnaligned);
    EXPECT_TRUE(IsAligned(obj.address() + kTaggedSize, kDoubleAlignment));
    // There is a filler object before the object.
    filler = HeapObject::FromAddress(start);
    EXPECT_NE(obj, filler);
    EXPECT_TRUE(IsFreeSpaceOrFiller(filler));
    EXPECT_EQ(filler->Size(), kTaggedSize);
    EXPECT_EQ(start + kTaggedSize, obj.address());
  }
}

// Test the case where allocation must be done from the free list, so filler
// may precede or follow the object.
TEST_F(HeapTest, TestAlignedOverAllocation) {
  if (v8_flags.stress_concurrent_allocation) return;
  ManualGCScope manual_gc_scope(i_isolate());
  // Test checks for fillers before and behind objects and requires a fresh
  // page and empty free list.
  AbandonCurrentlyFreeMemory(heap()->old_space());
  // Allocate a dummy object to properly set up the linear allocation info.
  AllocationResult dummy =
      heap()->allocator()->old_space_allocator()->AllocateRaw(
          SafeHeapObjectSize(kTaggedSize), kTaggedAligned,
          AllocationOrigin::kRuntime, AllocationHint());
  ASSERT_FALSE(dummy.IsFailure());
  heap()->CreateFillerObjectAt(dummy.ToObjectChecked().address(), kTaggedSize);

  // Double misalignment is 4 on 32-bit platforms or when pointer compression
  // is enabled, 0 on 64-bit ones when pointer compression is disabled.
  const intptr_t double_misalignment = kDoubleSize - kTaggedSize;
  Address start;
  Tagged<HeapObject> obj;
  Tagged<HeapObject> filler;
  if (double_misalignment) {
    start = AlignOldSpace(heap(), kDoubleAligned, 0);
    obj = AllocateAligned(heap(), heap()->allocator()->old_space_allocator(),
                          kTaggedSize, kDoubleAligned);
    // The object is aligned.
    EXPECT_TRUE(IsAligned(obj.address(), kDoubleAlignment));
    // Try the opposite alignment case.
    start = AlignOldSpace(heap(), kDoubleAligned, kTaggedSize);
    obj = AllocateAligned(heap(), heap()->allocator()->old_space_allocator(),
                          kTaggedSize, kDoubleAligned);
    EXPECT_TRUE(IsAligned(obj.address(), kDoubleAlignment));
    filler = HeapObject::FromAddress(start);
    EXPECT_NE(obj, filler);
    EXPECT_TRUE(IsFreeSpaceOrFiller(filler));
    EXPECT_EQ(kTaggedSize, filler->Size());

    // Similarly for kDoubleUnaligned.
    start = AlignOldSpace(heap(), kDoubleUnaligned, 0);
    obj = AllocateAligned(heap(), heap()->allocator()->old_space_allocator(),
                          kTaggedSize, kDoubleUnaligned);
    // The object is aligned.
    EXPECT_TRUE(IsAligned(obj.address() + kTaggedSize, kDoubleAlignment));
    // Try the opposite alignment case.
    start = AlignOldSpace(heap(), kDoubleUnaligned, kTaggedSize);
    obj = AllocateAligned(heap(), heap()->allocator()->old_space_allocator(),
                          kTaggedSize, kDoubleUnaligned);
    EXPECT_TRUE(IsAligned(obj.address() + kTaggedSize, kDoubleAlignment));
    filler = HeapObject::FromAddress(start);
    EXPECT_NE(obj, filler);
    EXPECT_TRUE(IsFreeSpaceOrFiller(filler));
    EXPECT_EQ(kTaggedSize, filler->Size());
  }
}

#ifdef DEBUG
TEST_F(HeapTest, TransitionArrayShrinksDuringAllocToZero) {
  v8_flags.stress_compaction = false;
  v8_flags.stress_incremental_marking = false;
  v8_flags.allow_natives_syntax = true;

  static const int transitions_count = 10;
  RunJS("function F() { }");
  {
    AlwaysAllocateScopeForTesting always_allocate(heap());
    for (int i = 0; i < transitions_count; i++) {
      base::EmbeddedVector<char, 64> buffer;
      base::SNPrintF(buffer, "var o = new F; o.prop%d = %d;", i, i);
      RunJS(buffer.begin());
    }
  }
  RunJS("var root = new F;");
  DirectHandle<JSObject> root = RunJS<JSObject>("root");

  // Count number of live transitions before marking.
  int transitions_before =
      TransitionsAccessor(i_isolate(), root->map()).NumberOfTransitions();
  EXPECT_EQ(transitions_count, transitions_before);

  // Get rid of o
  RunJS(
      "o = new F;"
      "root = new F");
  root = RunJS<JSObject>("root");

  DirectHandle<String> prop_name = factory()->InternalizeUtf8String("funny");
  DirectHandle<Smi> twenty_three(Smi::FromInt(23), i_isolate());
  HeapAllocator::SetAllocationGcInterval(2);
  v8_flags.gc_global = true;
  v8_flags.retain_maps_for_n_gc = 0;
  heap()->set_allocation_timeout(2);
  Object::SetProperty(i_isolate(), root, prop_name, twenty_three).Check();
  {
    // We need to invoke GC without stack, otherwise some objects may not be
    // reclaimed because of conservative stack scanning.
    DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap());
    InvokeMinorGC();
  }

  // Count number of live transitions after marking. Note that one transition
  // is left, because 'o' still holds an instance of one transition target.
  int transitions_after =
      TransitionsAccessor(i_isolate(), Cast<Map>(root->map()->GetBackPointer()))
          .NumberOfTransitions();
  EXPECT_EQ(1, transitions_after);
}

TEST_F(HeapTest, TransitionArrayShrinksDuringAllocToOne) {
  v8_flags.stress_compaction = false;
  v8_flags.stress_incremental_marking = false;
  v8_flags.allow_natives_syntax = true;

  static const int transitions_count = 10;
  RunJS("function F() {}");
  {
    AlwaysAllocateScopeForTesting always_allocate(heap());
    for (int i = 0; i < transitions_count; i++) {
      base::EmbeddedVector<char, 64> buffer;
      base::SNPrintF(buffer, "var o = new F; o.prop%d = %d;", i, i);
      RunJS(buffer.begin());
    }
  }
  RunJS("var root = new F;");
  DirectHandle<JSObject> root = RunJS<JSObject>("root");

  // Count number of live transitions before marking.
  int transitions_before =
      TransitionsAccessor(i_isolate(), root->map()).NumberOfTransitions();
  EXPECT_EQ(transitions_count, transitions_before);

  root = RunJS<JSObject>("root");
  DirectHandle<String> prop_name = factory()->InternalizeUtf8String("funny");
  DirectHandle<Smi> twenty_three(Smi::FromInt(23), i_isolate());
  HeapAllocator::SetAllocationGcInterval(2);
  v8_flags.gc_global = true;
  v8_flags.retain_maps_for_n_gc = 0;
  heap()->set_allocation_timeout(2);
  Object::SetProperty(i_isolate(), root, prop_name, twenty_three).Check();
  {
    // We need to invoke GC without stack, otherwise some objects may not be
    // reclaimed because of conservative stack scanning.
    DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap());
    InvokeMinorGC();
  }

  // Count number of live transitions after marking. Note that one transition
  // is left, because 'o' still holds an instance of one transition target.
  int transitions_after =
      TransitionsAccessor(i_isolate(), Cast<Map>(root->map()->GetBackPointer()))
          .NumberOfTransitions();
  EXPECT_EQ(2, transitions_after);
}

TEST_F(HeapTest, TransitionArrayShrinksDuringAllocToOnePropertyFound) {
  v8_flags.stress_compaction = false;
  v8_flags.stress_incremental_marking = false;
  v8_flags.allow_natives_syntax = true;

  static const int transitions_count = 10;
  RunJS("function F() {}");
  {
    AlwaysAllocateScopeForTesting always_allocate(heap());
    for (int i = 0; i < transitions_count; i++) {
      base::EmbeddedVector<char, 64> buffer;
      base::SNPrintF(buffer, "var o = new F; o.prop%d = %d;", i, i);
      RunJS(buffer.begin());
    }
  }
  RunJS("var root = new F;");
  DirectHandle<JSObject> root = RunJS<JSObject>("root");

  // Count number of live transitions before marking.
  int transitions_before =
      TransitionsAccessor(i_isolate(), root->map()).NumberOfTransitions();
  EXPECT_EQ(transitions_count, transitions_before);

  root = RunJS<JSObject>("root");
  DirectHandle<String> prop_name = factory()->InternalizeUtf8String("prop9");
  DirectHandle<Smi> twenty_three(Smi::FromInt(23), i_isolate());
  HeapAllocator::SetAllocationGcInterval(0);
  v8_flags.gc_global = true;
  v8_flags.retain_maps_for_n_gc = 0;
  heap()->set_allocation_timeout(0);
  Object::SetProperty(i_isolate(), root, prop_name, twenty_three).Check();
  InvokeMajorGC();

  // Count number of live transitions after marking. Note that one transition
  // is left, because 'o' still holds an instance of one transition target.
  int transitions_after =
      TransitionsAccessor(i_isolate(), Cast<Map>(root->map()->GetBackPointer()))
          .NumberOfTransitions();
  EXPECT_EQ(1, transitions_after);
}
#endif  // DEBUG

TEST_F(HeapTest, ReleaseOverReservedPages) {
  if (!v8_flags.compact) return;
  v8_flags.trace_gc = true;
  // The optimizer can allocate stuff, messing up the test.
#if !defined(V8_LITE_MODE) && defined(V8_ENABLE_TURBOFAN)
  v8_flags.turbofan = false;
#endif  // !defined(V8_LITE_MODE) && defined(V8_ENABLE_TURBOFAN)
  // - Parallel compaction increases fragmentation, depending on how existing
  //   memory is distributed. Since this is non-deterministic because of
  //   concurrent sweeping, we disable it for this test.
  // - Concurrent sweeping adds non determinism, depending on when memory is
  //   available for further reuse.
  // - Fast evacuation of pages may result in a different page count in old
  //   space.
  ManualGCScope manual_gc_scope(i_isolate());
  v8_flags.page_promotion = false;
  v8_flags.parallel_compaction = false;
  // If there's snapshot available, we don't know whether 20 small arrays will
  // fit on the initial pages.
  if (!i_isolate()->snapshot_available()) return;
  Factory* factory = i_isolate()->factory();
  Heap* heap = this->heap();

  // Ensure that the young generation is empty.
  {
    // In this test, we need to invoke GC without stack, otherwise some objects
    // may not be reclaimed because of conservative stack scanning.
    DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap);
    EmptyNewSpaceUsingGC();
  }
  static const int number_of_test_pages = 20;

  // Prepare many pages with low live-bytes count.
  PagedSpace* old_space = heap->old_space();
  const int initial_page_count = old_space->CountTotalPages();
  const int overall_page_count = number_of_test_pages + initial_page_count;

  Global<v8::FixedArray> fixed_arrays[number_of_test_pages];
  {
    v8::HandleScope scope(v8_isolate());

    for (int i = 0; i < number_of_test_pages; i++) {
      AlwaysAllocateScopeForTesting always_allocate(heap);
      {
        DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap);
        SimulateFullSpace(old_space);
      }
      Handle<FixedArray> fixed_array =
          factory->NewFixedArray(1, AllocationType::kOld);
      fixed_arrays[i].Reset(v8_isolate(),
                            v8::Utils::FixedArrayToLocal(fixed_array));
    }
  }

  EXPECT_EQ(overall_page_count, old_space->CountTotalPages());

  DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap);

  // Triggering one GC will cause a lot of garbage to be discovered but
  // even spread across all allocated pages.
  InvokeMajorGC();
  EXPECT_GE(overall_page_count, old_space->CountTotalPages());

  // Triggering subsequent GCs should cause at least half of the pages
  // to be released to the OS after at most two cycles.
  InvokeMajorGC();
  EXPECT_GE(overall_page_count, old_space->CountTotalPages());
  InvokeMajorGC();
  EXPECT_GE(number_of_test_pages,
            (old_space->CountTotalPages() - initial_page_count) * 2);

  // Triggering a last-resort GC should cause all pages to be released to the
  // OS so that other processes can seize the memory.
  const int page_count_before_memory_reducing_gcs =
      old_space->CountTotalPages();
  InvokeMemoryReducingMajorGCs();
  // With precise object pinning, some pages may be pinned and thus not
  // evacuated. It is therefore not guaranteed that the page count can return
  // to the initial count.
  EXPECT_GE(v8_flags.precise_object_pinning
                ? page_count_before_memory_reducing_gcs
                : initial_page_count,
            old_space->CountTotalPages());
}

TEST_F(HeapTest, RememberedSet_InsertOnWriteBarrier) {
  if (v8_flags.single_generation) return;
  v8_flags.stress_concurrent_allocation = false;  // For SealCurrentObjects.
  ManualGCScope manual_gc_scope(isolate());
  SealCurrentObjects();
  HandleScope scope(isolate());

  // Allocate an object in old space.
  DirectHandle<FixedArray> arr =
      factory()->NewFixedArray(3, AllocationType::kOld);

  // Add into 'arr' references to young objects.
  {
    HandleScope scope_inner(isolate());
    DirectHandle<Object> number = factory()->NewHeapNumber(42);
    arr->set(0, *number);
    arr->set(1, *number);
    arr->set(2, *number);
    DirectHandle<Object> number_other = factory()->NewHeapNumber(24);
    arr->set(2, *number_other);
  }
  // Remembered sets track *slots* pages with cross-generational pointers, so
  // must have recorded three of them each exactly once.
  EXPECT_EQ(size_t{3}, GetRememberedSetSize<OLD_TO_NEW>(isolate(), *arr));
}

TEST_F(HeapTest, RememberedSet_InsertInLargePage) {
  if (v8_flags.single_generation) return;
  v8_flags.stress_concurrent_allocation = false;  // For SealCurrentObjects.
  ManualGCScope manual_gc_scope(isolate());
  Factory* factory = isolate()->factory();
  Heap* heap = isolate()->heap();
  SealCurrentObjects();
  HandleScope scope(isolate());

  // Allocate an object in Large space.
  const int count = std::max(FixedArray::kMaxRegularLength + 1, 128 * KB);
  DirectHandle<FixedArray> arr =
      factory->NewFixedArray(count, AllocationType::kOld);
  EXPECT_TRUE(heap->lo_space()->Contains(*arr));
  EXPECT_EQ(size_t{0}, GetRememberedSetSize<OLD_TO_NEW>(isolate(), *arr));

  // Create OLD_TO_NEW references from the large object so that the
  // corresponding slots end up in different SlotSets.
  {
    HandleScope short_lived(isolate());
    DirectHandle<Object> number = factory->NewHeapNumber(42);
    arr->set(0, *number);
    arr->set(count - 1, *number);
  }
  EXPECT_EQ(size_t{2}, GetRememberedSetSize<OLD_TO_NEW>(isolate(), *arr));
}

TEST_F(HeapTest, RememberedSet_RemoveStaleOnScavenge) {
  if (v8_flags.single_generation || v8_flags.stress_incremental_marking) return;
  v8_flags.stress_concurrent_allocation = false;  // For SealCurrentObjects.
  ManualGCScope manual_gc_scope(isolate());
  Factory* factory = isolate()->factory();
  Heap* heap = isolate()->heap();
  SealCurrentObjects();
  HandleScope scope(isolate());

  // Allocate an object in old space and add into it references to young.
  DirectHandle<FixedArray> arr =
      factory->NewFixedArray(3, AllocationType::kOld);
  {
    HandleScope scope_inner(isolate());
    DirectHandle<Object> number = factory->NewHeapNumber(42);
    arr->set(0, *number);  // will be trimmed away
    arr->set(1, *number);  // will be replaced with #undefined
    arr->set(2, *number);  // will be promoted into old
  }
  EXPECT_EQ(size_t{3}, GetRememberedSetSize<OLD_TO_NEW>(isolate(), *arr));

  arr->set(1, ReadOnlyRoots(heap).undefined_value());
  DirectHandle<FixedArrayBase> tail(heap->LeftTrimFixedArray(*arr, 1),
                                    isolate());

  // The first slot should be removed from the remembered set since the length
  // is now in that place and represented as a uint32_t.
  EXPECT_EQ(size_t{2}, GetRememberedSetSize<OLD_TO_NEW>(isolate(), *tail));

  // Run GC to promote the remaining young object and fixup the stale entries in
  // the remembered set.
  EmptyNewSpaceUsingGC();
  EXPECT_EQ(size_t{0}, GetRememberedSetSize<OLD_TO_NEW>(isolate(), *tail));
}

// The OLD_TO_OLD remembered set is created temporary by GC and is cleared at
// the end of the pass. There is no way to observe it so the test only checks
// that compaction has happened and otherwise relies on code's self-validation.
TEST_F(HeapTest, RememberedSet_OldToOld) {
  if (v8_flags.stress_incremental_marking) return;
  v8_flags.stress_concurrent_allocation = false;  // For SealCurrentObjects.
  ManualGCScope manual_gc_scope(isolate());
  ManualEvacuationCandidatesSelectionScope
      manual_evacuation_candidate_selection_scope(manual_gc_scope);
  SealCurrentObjects();

  v8::Global<v8::FixedArray> arr_global;
  Tagged<FixedArray> prev_location;
  {
    HandleScope scope(isolate());

    DirectHandle<FixedArray> arr =
        factory()->NewFixedArray(10, AllocationType::kOld);
    {
      HandleScope short_lived(isolate());
      factory()->NewFixedArray(100, AllocationType::kOld);
    }
    DirectHandle<Object> ref =
        factory()->NewFixedArray(100, AllocationType::kOld);
    arr->set(0, *ref);

    // To force compaction of the old space, fill it with garbage and start a
    // new page (so that the page with 'arr' becomes subject to compaction).
    {
      HandleScope short_lived(isolate());
      SimulateFullSpace(heap()->old_space());
      factory()->NewFixedArray(100, AllocationType::kOld);
    }

    ForceEvacuationCandidate(NormalPage::FromHeapObject(*arr));
    prev_location = *arr;
    arr_global.Reset(v8_isolate(), v8::Utils::FixedArrayToLocal(arr));
  }
  {
    // This GC pass will evacuate the page with 'arr'/'ref' so it will have to
    // create OLD_TO_OLD remembered set to track the reference.
    // We need to invoke GC without stack, otherwise no compaction is performed.
    DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap());
    InvokeMajorGC();
  }
  {
    v8::HandleScope scope(v8_isolate());
    DirectHandle<FixedArray> arr =
        v8::Utils::OpenDirectHandle(*arr_global.Get(v8_isolate()));
    EXPECT_NE(prev_location.ptr(), arr->ptr());
  }
}

TEST_F(HeapTest, RememberedSetRemoveRange) {
  if (v8_flags.single_generation) return;
  HandleScope scope(isolate());

  DirectHandle<FixedArray> array = factory()->NewFixedArray(
      NormalPage::kPageSize / kTaggedSize, AllocationType::kOld);
  MutablePage* chunk = MutablePage::FromHeapObject(isolate(), *array);
  EXPECT_EQ(chunk->owner_identity(), LO_SPACE);
  Address start = array->address();
  // Maps slot to boolean indicator of whether the slot should be in the set.
  std::map<Address, bool> slots;
  slots[start + 0] = true;
  slots[start + kTaggedSize] = true;
  slots[start + NormalPage::kPageSize - kTaggedSize] = true;
  slots[start + NormalPage::kPageSize] = true;
  slots[start + NormalPage::kPageSize + kTaggedSize] = true;
  slots[chunk->area_end() - kTaggedSize] = true;

  for (auto x : slots) {
    RememberedSet<OLD_TO_NEW>::Insert<AccessMode::ATOMIC>(
        chunk, chunk->Offset(x.first));
  }

  RememberedSet<OLD_TO_NEW>::Iterate(
      chunk,
      [&slots](MaybeObjectSlot slot) {
        EXPECT_TRUE(slots[slot.address()]);
        return KEEP_SLOT;
      },
      SlotSet::FREE_EMPTY_BUCKETS);

  RememberedSet<OLD_TO_NEW>::RemoveRange(chunk, start, start + kTaggedSize,
                                         SlotSet::FREE_EMPTY_BUCKETS);
  slots[start] = false;
  RememberedSet<OLD_TO_NEW>::Iterate(
      chunk,
      [&slots](MaybeObjectSlot slot) {
        EXPECT_TRUE(slots[slot.address()]);
        return KEEP_SLOT;
      },
      SlotSet::FREE_EMPTY_BUCKETS);

  RememberedSet<OLD_TO_NEW>::RemoveRange(chunk, start + kTaggedSize,
                                         start + NormalPage::kPageSize,
                                         SlotSet::FREE_EMPTY_BUCKETS);
  slots[start + kTaggedSize] = false;
  slots[start + NormalPage::kPageSize - kTaggedSize] = false;
  RememberedSet<OLD_TO_NEW>::Iterate(
      chunk,
      [&slots](MaybeObjectSlot slot) {
        EXPECT_TRUE(slots[slot.address()]);
        return KEEP_SLOT;
      },
      SlotSet::FREE_EMPTY_BUCKETS);

  RememberedSet<OLD_TO_NEW>::RemoveRange(
      chunk, start, start + NormalPage::kPageSize + kTaggedSize,
      SlotSet::FREE_EMPTY_BUCKETS);
  slots[start + NormalPage::kPageSize] = false;
  RememberedSet<OLD_TO_NEW>::Iterate(
      chunk,
      [&slots](MaybeObjectSlot slot) {
        EXPECT_TRUE(slots[slot.address()]);
        return KEEP_SLOT;
      },
      SlotSet::FREE_EMPTY_BUCKETS);

  RememberedSet<OLD_TO_NEW>::RemoveRange(chunk, chunk->area_end() - kTaggedSize,
                                         chunk->area_end(),
                                         SlotSet::FREE_EMPTY_BUCKETS);
  slots[chunk->area_end() - kTaggedSize] = false;
  RememberedSet<OLD_TO_NEW>::Iterate(
      chunk,
      [&slots](MaybeObjectSlot slot) {
        EXPECT_TRUE(slots[slot.address()]);
        return KEEP_SLOT;
      },
      SlotSet::FREE_EMPTY_BUCKETS);
}

TEST_F(HeapTest, Regress670675) {
  if (!v8_flags.incremental_marking) return;
  ManualGCScope manual_gc_scope(isolate());
  HandleScope scope(isolate());
  Heap* heap = this->heap();
  Isolate* isolate = this->isolate();
  InvokeMajorGC();

  heap->EnsureSweepingCompleted(
      Heap::SweepingForcedFinalizationMode::kUnifiedHeap,
      CompleteSweepingReason::kTesting);
  heap->tracer()->StopFullCycleIfFinished();
  i::IncrementalMarking* marking = heap->incremental_marking();
  if (marking->IsStopped()) {
    SafepointScope safepoint_scope(isolate,
                                   kGlobalSafepointForSharedSpaceIsolate);
    heap->tracer()->StartCycle(
        GarbageCollector::MARK_COMPACTOR, GarbageCollectionReason::kTesting,
        "collector unittest", GCTracer::MarkingType::kIncremental);
    marking->Start(GarbageCollector::MARK_COMPACTOR,
                   i::GarbageCollectionReason::kTesting, "testing");
  }
  size_t array_length = 128 * KB;
  size_t n = OldGenerationSpaceAvailable() / array_length;
  for (size_t i = 0; i < n + 60; i++) {
    {
      HandleScope inner_scope(isolate);
      isolate->factory()->NewFixedArray(static_cast<int>(array_length),
                                        AllocationType::kOld);
    }
    if (marking->IsStopped()) break;
    marking->AdvanceForTesting(v8::base::TimeDelta::FromMillisecondsD(0.1));
  }
  EXPECT_TRUE(marking->IsStopped());
}

TEST_F(HeapTest, RegressMissingWriteBarrierInAllocate) {
  if (!v8_flags.incremental_marking) return;
  ManualGCScope manual_gc_scope(isolate());
  v8::HandleScope scope(v8_isolate());
  Isolate* iso = isolate();
  InvokeMajorGC();
  SimulateIncrementalMarking(false);
  DirectHandle<Map> map;
  {
    AlwaysAllocateScopeForTesting always_allocate(heap());
    map = factory()->NewContextfulMapForCurrentContext(JS_OBJECT_TYPE,
                                                       JSObject::kHeaderSize);
  }
  EXPECT_TRUE(heap()->incremental_marking()->black_allocation());
  DirectHandle<JSObject> object;
  {
    AlwaysAllocateScopeForTesting always_allocate(heap());
    object = direct_handle(
        Cast<JSObject>(factory()->NewForTest(map, AllocationType::kOld)), iso);
  }
  // Initialize backing stores to ensure object is valid.
  ReadOnlyRoots roots(iso);
  object->set_raw_properties_or_hash(roots.empty_property_array(),
                                     SKIP_WRITE_BARRIER);
  object->set_elements(roots.empty_fixed_array(), SKIP_WRITE_BARRIER);

  // The object is black. If Factory::New sets the map without write-barrier,
  // then the map is white and will be freed prematurely.
  SimulateIncrementalMarking(true);
  InvokeMajorGC();
  if (heap()->sweeping_in_progress()) {
    heap()->EnsureSweepingCompleted(
        Heap::SweepingForcedFinalizationMode::kV8Only,
        CompleteSweepingReason::kTesting);
  }
  EXPECT_TRUE(IsMap(object->map()));
}

TEST_F(HeapTest, MarkCompactEpochCounter) {
  if (!v8_flags.incremental_marking) return;
  ManualGCScope manual_gc_scope(isolate());

  unsigned epoch0 = heap()->mark_compact_collector()->epoch();
  InvokeMajorGC();
  unsigned epoch1 = heap()->mark_compact_collector()->epoch();
  EXPECT_EQ(epoch0 + 1, epoch1);
  SimulateIncrementalMarking();
  InvokeMajorGC();
  unsigned epoch2 = heap()->mark_compact_collector()->epoch();
  EXPECT_EQ(epoch1 + 1, epoch2);
  InvokeMinorGC();
  unsigned epoch3 = heap()->mark_compact_collector()->epoch();
  EXPECT_EQ(epoch2, epoch3);
}

TEST_F(TestWithPlatform, ReinitializeStringHashSeed) {
  // Enable rehashing and create an isolate and context.
  v8_flags.rehash_snapshot = true;
  std::unique_ptr<v8::ArrayBuffer::Allocator> allocator(
      v8::ArrayBuffer::Allocator::NewDefaultAllocator());
  for (int i = 1; i < 3; i++) {
    v8_flags.hash_seed = 1337 * i;
    v8::Isolate::CreateParams create_params;
    create_params.array_buffer_allocator = allocator.get();
    v8::Isolate* isolate = v8::Isolate::New(create_params);
    {
      v8::Isolate::Scope isolate_scope(isolate);
      EXPECT_EQ(static_cast<uint64_t>(1337 * i),
                HashSeed(reinterpret_cast<Isolate*>(isolate)).seed());
      v8::HandleScope handle_scope(isolate);
      v8::Local<v8::Context> context = v8::Context::New(isolate);
      EXPECT_FALSE(context.IsEmpty());
      v8::Context::Scope context_scope(context);
    }
    isolate->Dispose();
  }
}

TEST_F(HeapTest, Regress169928) {
  v8_flags.allow_natives_syntax = true;
#if !defined(V8_LITE_MODE) && defined(V8_ENABLE_TURBOFAN)
  v8_flags.turbofan = false;
#endif  // !defined(V8_LITE_MODE) && defined(V8_ENABLE_TURBOFAN)

  // Some flags turn Scavenge collections into Mark-sweep collections
  // and hence are incompatible with this test case.
  if (v8_flags.gc_global || v8_flags.stress_compaction ||
      v8_flags.stress_incremental_marking || v8_flags.single_generation ||
      v8_flags.minor_ms) {
    return;
  }

  // Prepare the environment
  RunJS(
      "function fastliteralcase(literal, value) {"
      "    literal[0] = value;"
      "    return literal;"
      "}"
      "function get_standard_literal() {"
      "    var literal = [1, 2, 3];"
      "    return literal;"
      "}"
      "obj = fastliteralcase(get_standard_literal(), 1);"
      "obj = fastliteralcase(get_standard_literal(), 1.5);"
      "obj = fastliteralcase(get_standard_literal(), 2);");

  // Prepare the heap: pre-allocate strings before page filling to avoid
  // disturbing heap layout.
  v8::Local<v8::String> mote_code_string =
      NewString("fastliteralcase(mote, 2.5);");
  v8::Local<v8::String> array_name = NewString("mote");
  EXPECT_TRUE(context()
                  ->Global()
                  ->Set(context(), array_name, v8::Int32::New(v8_isolate(), 0))
                  .FromJust());

  // First make sure we flip spaces
  InvokeMinorGC();

  // Allocate the object.
  DirectHandle<FixedArray> array_data =
      factory()->NewFixedArray(2, AllocationType::kYoung);
  array_data->set(0, Smi::FromInt(1));
  array_data->set(1, Smi::FromInt(2));

  FillCurrentPageButNBytes(
      SemiSpaceNewSpace::From(heap()->new_space()),
      JSArray::kHeaderSize + sizeof(AllocationMemento) + kTaggedSize);

  DirectHandle<JSArray> array =
      factory()->NewJSArrayWithElements(array_data, PACKED_SMI_ELEMENTS);

  EXPECT_EQ(Smi::FromInt(2), array->length());
  EXPECT_TRUE(array->HasSmiOrObjectElements());

  // We need filler the size of AllocationMemento object, plus an extra
  // fill pointer value.
  Tagged<HeapObject> obj;
  AllocationResult allocation =
      heap()->allocator()->new_space_allocator()->AllocateRaw(
          SafeHeapObjectSize(sizeof(AllocationMemento) + kTaggedSize),
          kTaggedAligned, AllocationOrigin::kRuntime, AllocationHint());
  EXPECT_TRUE(allocation.To(&obj));
  Address addr_obj = obj.address();
  heap()->CreateFillerObjectAt(addr_obj,
                               sizeof(AllocationMemento) + kTaggedSize);

  // Give the array a name, making sure not to allocate strings.
  v8::Local<v8::Object> array_obj = v8::Utils::ToLocal(array);
  EXPECT_TRUE(
      context()->Global()->Set(context(), array_name, array_obj).FromJust());

  // This should crash with a protection violation if running a build with the
  // bug.
  AlwaysAllocateScopeForTesting aa_scope(heap());
  v8::Script::Compile(context(), mote_code_string)
      .ToLocalChecked()
      ->Run(context())
      .ToLocalChecked();
}

TEST_F(HeapTest, LargeObjectSlotRecording) {
  if (!v8_flags.incremental_marking) return;
  if (!v8_flags.compact) return;
  v8_flags.manual_evacuation_candidates_selection = true;
  ManualGCScope manual_gc_scope(isolate());

  const int size = std::max(1000000, kMaxRegularHeapObjectSize + KB);
  const int kStep = size / 10;

  v8::Global<v8::FixedArray> lit_global;
  v8::Global<v8::FixedArray> lo_global;
  Tagged<FixedArray> old_location;
  {
    HandleScope scope(isolate());

    // Create an object on an evacuation candidate.
    SimulateFullSpace(heap()->old_space());
    IndirectHandle<FixedArray> lit =
        factory()->NewFixedArray(4, AllocationType::kOld);
    MutablePage::FromHeapObject(isolate(), *lit)
        ->set_forced_evacuation_candidate_for_testing(true);
    old_location = *lit;

    // Allocate a large object.
    EXPECT_LT(kMaxRegularHeapObjectSize, size);
    IndirectHandle<FixedArray> lo =
        factory()->NewFixedArray(size, AllocationType::kOld);
    EXPECT_TRUE(heap()->lo_space()->Contains(*lo));

    // Start incremental marking to activate write barrier.
    SimulateIncrementalMarking(false);

    // Create references from the large object to the object on the evacuation
    // candidate.
    for (int i = 0; i < size; i += kStep) {
      lo->set(i, *lit);
      EXPECT_EQ(lo->get(i), old_location);
    }

    SimulateIncrementalMarking(true);
    lit_global.Reset(v8_isolate(), Utils::FixedArrayToLocal(lit));
    lo_global.Reset(v8_isolate(), Utils::FixedArrayToLocal(lo));
  }

  // Move the evacuation candidate object.
  {
    // We need to invoke GC without stack, otherwise no compaction is performed.
    DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap());
    InvokeMajorGC();
  }

  {
    v8::HandleScope scope(v8_isolate());
    IndirectHandle<FixedArray> lit =
        Utils::OpenHandle(*lit_global.Get(v8_isolate()));
    IndirectHandle<FixedArray> lo =
        Utils::OpenHandle(*lo_global.Get(v8_isolate()));
    // Verify that the pointers in the large object got updated.
    for (int i = 0; i < size; i += kStep) {
      EXPECT_EQ(lo->get(i).ptr(), lit->ptr());
      EXPECT_NE(lo->get(i).ptr(), old_location.ptr());
    }
  }
}

TEST_F(HeapTest, PersistentHandles) {
  Isolate* isolate = i_isolate();
  Heap* heap = isolate->heap();
  v8::HandleScope scope(v8_isolate());
  HandleScopeData* data = isolate->handle_scope_data();
  IndirectHandle<Object> init(ReadOnlyRoots(heap).empty_string(), isolate);
  while (data->next < data->limit) {
    IndirectHandle<Object> obj(ReadOnlyRoots(heap).empty_string(), isolate);
  }
  // An entire block of handles has been filled.
  // Next handle would require a new block.
  EXPECT_EQ(data->next, data->limit);

  PersistentHandlesScope persistent(isolate);
  class DummyVisitor : public RootVisitor {
   public:
    void VisitRootPointers(Root root, const char* description,
                           FullObjectSlot start, FullObjectSlot end) override {}
  } visitor;
  isolate->handle_scope_implementer()->Iterate(&visitor);
  persistent.Detach();
}

void TestFillersFromPersistentHandles(Isolate* isolate, bool promote) {
  // We assume that the fillers can only arise when left-trimming arrays.
  ManualGCScope manual_gc_scope(isolate);
  Heap* heap = isolate->heap();
  v8::HandleScope scope(reinterpret_cast<v8::Isolate*>(isolate));

  const size_t n = 10;
  DirectHandle<FixedArray> array = isolate->factory()->NewFixedArray(n);

  if (promote) {
    // Age the array so it's ready for promotion on next GC.
    InvokeMinorGC(isolate);
  }
  EXPECT_TRUE(HeapLayout::InYoungGeneration(*array));

  PersistentHandlesScope persistent_scope(isolate);

  // Trim the array three times to different sizes so all kinds of fillers are
  // created and tracked by the persistent handles.
  DirectHandle<FixedArrayBase> filler_1(*array, isolate);
  DirectHandle<FixedArrayBase> filler_2(heap->LeftTrimFixedArray(*filler_1, 1),
                                        isolate);
  DirectHandle<FixedArrayBase> filler_3(heap->LeftTrimFixedArray(*filler_2, 2),
                                        isolate);
  DirectHandle<FixedArrayBase> tail(heap->LeftTrimFixedArray(*filler_3, 3),
                                    isolate);

  std::unique_ptr<PersistentHandles> persistent_handles(
      persistent_scope.Detach());

  // GC should retain the trimmed array but drop all of the three fillers.
  InvokeMinorGC(isolate);
  if (!v8_flags.single_generation) {
    if (promote) {
      EXPECT_TRUE(heap->InOldSpace(*tail));
    } else {
      EXPECT_TRUE(HeapLayout::InYoungGeneration(*tail));
    }
  }
  EXPECT_EQ(n - 6, static_cast<size_t>(tail->length().value()));
  EXPECT_FALSE(IsHeapObject(*filler_1));
  EXPECT_FALSE(IsHeapObject(*filler_2));
  EXPECT_FALSE(IsHeapObject(*filler_3));
}

TEST_F(HeapTest, DoNotEvacuateFillersFromPersistentHandles) {
  if (v8_flags.single_generation || v8_flags.move_object_start) return;
  TestFillersFromPersistentHandles(i_isolate(), false /*promote*/);
}

TEST_F(HeapTest, DoNotPromoteFillersFromPersistentHandles) {
  if (v8_flags.single_generation || v8_flags.move_object_start) return;
  TestFillersFromPersistentHandles(i_isolate(), true /*promote*/);
}

TEST_F(HeapTest, IncrementalMarkingStepMakesBigProgressWithLargeObjects) {
  if (!v8_flags.incremental_marking) return;
  ManualGCScope manual_gc_scope(i_isolate());
  RunJS(
      "function f(n) {"
      "    var a = new Array(n);"
      "    for (var i = 0; i < n; i += 100) a[i] = i;"
      "};"
      "f(10 * 1024 * 1024);");
  IncrementalMarking* marking = heap()->incremental_marking();
  if (marking->IsStopped()) {
    heap()->StartIncrementalMarking(GCFlag::kNoFlags,
                                    GarbageCollectionReason::kTesting);
  }
  SimulateIncrementalMarking();
  EXPECT_TRUE(marking->IsMajorMarkingComplete());
}

TEST_F(HeapTest, DisableInlineAllocation) {
  v8_flags.allow_natives_syntax = true;
  RunJS(
      "function test() {"
      "  var x = [];"
      "  for (var i = 0; i < 10; i++) {"
      "    x[i] = [ {}, [1,2,3], [1,x,3] ];"
      "  }"
      "}"
      "function run() {"
      "  %PrepareFunctionForOptimization(test);"
      "  %OptimizeFunctionOnNextCall(test);"
      "  test();"
      "  %DeoptimizeFunction(test);"
      "}");

  // Warm-up with inline allocation enabled.
  RunJS("test(); test(); run();");

  // Run test with inline allocation disabled.
  heap()->DisableInlineAllocation();
  RunJS("run()");

  // Run test with inline allocation re-enabled.
  heap()->EnableInlineAllocation();
  RunJS("run()");
}

TEST_F(HeapTest, EnsureAllocationSiteDependentCodesProcessed) {
  if (!V8_ALLOCATION_SITE_TRACKING_BOOL) {
    return;
  }
  v8_flags.allow_natives_syntax = true;
  Isolate* isolate = i_isolate();
  Heap* heap = this->heap();
  GlobalHandles* global_handles = isolate->global_handles();

  if (!isolate->use_optimizer()) return;

  auto allocation_sites_count = [](Heap* heap) {
    int count = 0;
    for (Tagged<Object> site = heap->allocation_sites_list();
         IsAllocationSite(site);) {
      Tagged<AllocationSiteWithWeakNext> cur =
          Cast<AllocationSiteWithWeakNext>(site);
      EXPECT_TRUE(cur->HasWeakNext());
      site = cur->weak_next();
      count++;
    }
    return count;
  };

  // The allocation site at the head of the list is ours.
  IndirectHandle<AllocationSite> site;
  {
    v8::HandleScope scope(v8_isolate());
    v8::Local<v8::Context> context = v8::Context::New(v8_isolate());
    v8::Context::Scope context_scope(context);

    int count = allocation_sites_count(heap);
    RunJS(context,
          "var bar = function() { return (new Array()); };"
          "%PrepareFunctionForOptimization(bar);"
          "var a = bar();"
          "bar();"
          "bar();");

    // One allocation site should have been created.
    int new_count = allocation_sites_count(heap);
    EXPECT_EQ(new_count, count + 1);
    site = Cast<AllocationSite>(global_handles->Create(
        Cast<AllocationSite>(heap->allocation_sites_list())));

    RunJS(context, "%OptimizeFunctionOnNextCall(bar); bar();");

    IndirectHandle<JSFunction> bar_handle =
        Cast<JSFunction>(v8::Utils::OpenIndirectHandle(
            *v8::Local<v8::Function>::Cast(context->Global()
                                               ->Get(context, NewString("bar"))
                                               .ToLocalChecked())));

    // Expect a dependent code object for transitioning and pretenuring.
    Tagged<DependentCode> dependency = site->dependent_code();
    EXPECT_NE(dependency,
              DependentCode::empty_dependent_code(ReadOnlyRoots(isolate)));
    EXPECT_EQ(dependency->length().value(),
              static_cast<uint32_t>(DependentCode::kSlotsPerEntry));
    Tagged<MaybeObject> code =
        dependency->Get(0 + DependentCode::kCodeSlotOffset);
    EXPECT_TRUE(code.IsWeak());
    EXPECT_EQ(bar_handle->code(isolate),
              Cast<CodeWrapper>(code.GetHeapObjectAssumeWeak())->code(isolate));
    Tagged<Smi> groups =
        dependency->Get(0 + DependentCode::kGroupsSlotOffset).ToSmi();
    EXPECT_EQ(static_cast<DependentCode::DependencyGroups>(groups.value()),
              DependentCode::kAllocationSiteTransitionChangedGroup |
                  DependentCode::kAllocationSiteTenuringChangedGroup);
  }

  // Now make sure that a gc should get rid of the function, even though we
  // still have the allocation site alive.
  for (int i = 0; i < 4; i++) {
    // We need to invoke GC without stack, otherwise some objects may not be
    // reclaimed because of conservative stack scanning.
    DisableConservativeStackScanningScopeForTesting no_stack_scanning(heap);
    InvokeMajorGC();
  }

  // The site still exists because of our global handle, but the code is no
  // longer referred to by dependent_code().
  EXPECT_TRUE(site->dependent_code()->Get(0).IsCleared());
}

}  // namespace internal
}  // namespace v8
