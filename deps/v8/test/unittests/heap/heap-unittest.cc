// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/heap.h"

#include <cmath>
#include <iostream>
#include <limits>

#include "include/v8-isolate.h"
#include "include/v8-object.h"
#include "src/handles/handles-inl.h"
#include "src/heap/gc-tracer.h"
#include "src/heap/memory-chunk.h"
#include "src/heap/remembered-set.h"
#include "src/heap/safepoint.h"
#include "src/heap/spaces-inl.h"
#include "src/objects/objects-inl.h"
#include "test/unittests/heap/heap-utils.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

using HeapTest = TestWithHeapInternalsAndContext;

TEST(Heap, YoungGenerationSizeFromOldGenerationSize) {
  const size_t pm = i::Heap::kPointerMultiplier;
  const size_t hlm = i::Heap::kHeapLimitMultiplier;
  ASSERT_EQ(3 * 512u * pm * KB,
            i::Heap::YoungGenerationSizeFromOldGenerationSize(128u * hlm * MB));
  ASSERT_EQ(3 * 2048u * pm * KB,
            i::Heap::YoungGenerationSizeFromOldGenerationSize(256u * hlm * MB));
  ASSERT_EQ(3 * 4096u * pm * KB,
            i::Heap::YoungGenerationSizeFromOldGenerationSize(512u * hlm * MB));
  ASSERT_EQ(
      3 * 8192u * pm * KB,
      i::Heap::YoungGenerationSizeFromOldGenerationSize(1024u * hlm * MB));
}

TEST(Heap, GenerationSizesFromHeapSize) {
  const size_t pm = i::Heap::kPointerMultiplier;
  const size_t hlm = i::Heap::kHeapLimitMultiplier;
  size_t old, young;

  i::Heap::GenerationSizesFromHeapSize(1 * KB, &young, &old);
  ASSERT_EQ(0u, old);
  ASSERT_EQ(0u, young);

  i::Heap::GenerationSizesFromHeapSize(1 * KB + 3 * 512u * pm * KB, &young,
                                       &old);
  ASSERT_EQ(1u * KB, old);
  ASSERT_EQ(3 * 512u * pm * KB, young);

  i::Heap::GenerationSizesFromHeapSize(128 * hlm * MB + 3 * 512 * pm * KB,
                                       &young, &old);
  ASSERT_EQ(128u * hlm * MB, old);
  ASSERT_EQ(3 * 512u * pm * KB, young);

  i::Heap::GenerationSizesFromHeapSize(256u * hlm * MB + 3 * 2048 * pm * KB,
                                       &young, &old);
  ASSERT_EQ(256u * hlm * MB, old);
  ASSERT_EQ(3 * 2048u * pm * KB, young);

  i::Heap::GenerationSizesFromHeapSize(512u * hlm * MB + 3 * 4096 * pm * KB,
                                       &young, &old);
  ASSERT_EQ(512u * hlm * MB, old);
  ASSERT_EQ(3 * 4096u * pm * KB, young);

  i::Heap::GenerationSizesFromHeapSize(1024u * hlm * MB + 3 * 8192 * pm * KB,
                                       &young, &old);
  ASSERT_EQ(1024u * hlm * MB, old);
  ASSERT_EQ(3 * 8192u * pm * KB, young);
}

TEST(Heap, HeapSizeFromPhysicalMemory) {
  const size_t pm = i::Heap::kPointerMultiplier;
  const size_t hlm = i::Heap::kHeapLimitMultiplier;

  // The expected value is old_generation_size + 3 * semi_space_size.
  ASSERT_EQ(128 * hlm * MB + 3 * 512 * pm * KB,
            i::Heap::HeapSizeFromPhysicalMemory(0u));
  ASSERT_EQ(128 * hlm * MB + 3 * 512 * pm * KB,
            i::Heap::HeapSizeFromPhysicalMemory(512u * MB));
  ASSERT_EQ(256 * hlm * MB + 3 * 2048 * pm * KB,
            i::Heap::HeapSizeFromPhysicalMemory(1024u * MB));
  ASSERT_EQ(512 * hlm * MB + 3 * 4096 * pm * KB,
            i::Heap::HeapSizeFromPhysicalMemory(2048u * MB));
  ASSERT_EQ(
      1024 * hlm * MB + 3 * 8192 * pm * KB,
      i::Heap::HeapSizeFromPhysicalMemory(static_cast<uint64_t>(4096u) * MB));
  ASSERT_EQ(
      1024 * hlm * MB + 3 * 8192 * pm * KB,
      i::Heap::HeapSizeFromPhysicalMemory(static_cast<uint64_t>(8192u) * MB));
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
  EXPECT_EQ(kExternalAllocationSoftLimit, heap->external_memory_limit());
}

TEST_F(HeapTest, ExternalLimitStaysAboveDefaultForExplicitHandling) {
  v8_isolate()->AdjustAmountOfExternalAllocatedMemory(+10 * MB);
  v8_isolate()->AdjustAmountOfExternalAllocatedMemory(-10 * MB);
  Heap* heap = i_isolate()->heap();
  EXPECT_GE(heap->external_memory_limit(), kExternalAllocationSoftLimit);
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
  EXPECT_TRUE(IsAligned(code_cage_base, size_t{4} * GB));

#ifdef V8_COMPRESS_POINTERS_IN_ISOLATE_CAGE
  Address isolate_root = i_isolate()->isolate_root();
  EXPECT_EQ(cage_base, isolate_root);
#endif

  // Check that all memory chunks belong this region.
  base::AddressRegion heap_reservation(cage_base, size_t{4} * GB);
  base::AddressRegion code_reservation(code_cage_base, size_t{4} * GB);

  SafepointScope scope(i_isolate()->heap());
  OldGenerationMemoryChunkIterator iter(i_isolate()->heap());
  for (;;) {
    MemoryChunk* chunk = iter.next();
    if (chunk == nullptr) break;

    Address address = chunk->address();
    size_t size = chunk->area_end() - address;
    AllocationSpace owner_id = chunk->owner_identity();
    if (V8_EXTERNAL_CODE_SPACE_BOOL &&
        (owner_id == CODE_SPACE || owner_id == CODE_LO_SPACE)) {
      EXPECT_TRUE(code_reservation.contains(address, size));
    } else {
      EXPECT_TRUE(heap_reservation.contains(address, size));
    }
  }
}
#endif  // V8_COMPRESS_POINTERS

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
  CollectAllGarbage();
  CollectAllGarbage();
  new_space->Shrink();

  // Explicitly growing should double the space capacity.
  size_t old_capacity, new_capacity;
  old_capacity = new_space->TotalCapacity();
  GrowNewSpace();
  new_capacity = new_space->TotalCapacity();
  CHECK_EQ(2 * old_capacity, new_capacity);

  old_capacity = new_space->TotalCapacity();
  {
    v8::HandleScope temporary_scope(reinterpret_cast<v8::Isolate*>(isolate()));
    SimulateFullSpace(new_space);
  }
  new_capacity = new_space->TotalCapacity();
  CHECK_EQ(old_capacity, new_capacity);

  // Explicitly shrinking should not affect space capacity.
  old_capacity = new_space->TotalCapacity();
  new_space->Shrink();
  new_capacity = new_space->TotalCapacity();
  CHECK_EQ(old_capacity, new_capacity);

  // Let the scavenger empty the new space.
  CollectGarbage(NEW_SPACE);
  CHECK_LE(new_space->Size(), old_capacity);

  // Explicitly shrinking should halve the space capacity.
  old_capacity = new_space->TotalCapacity();
  new_space->Shrink();
  new_capacity = new_space->TotalCapacity();
  if (v8_flags.minor_mc) {
    // Shrinking may not be able to remove any pages if all contain live
    // objects.
    CHECK_GE(old_capacity, new_capacity);
  } else {
    CHECK_EQ(old_capacity, 2 * new_capacity);
  }

  // Consecutive shrinking should not affect space capacity.
  old_capacity = new_space->TotalCapacity();
  new_space->Shrink();
  new_space->Shrink();
  new_space->Shrink();
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
  size_t old_capacity, new_capacity;
  old_capacity = new_space->TotalCapacity();
  GrowNewSpace();
  new_capacity = new_space->TotalCapacity();
  CHECK_EQ(2 * old_capacity, new_capacity);
  {
    v8::HandleScope temporary_scope(iso);
    SimulateFullSpace(new_space);
  }
  CollectAllAvailableGarbage();
  new_capacity = new_space->TotalCapacity();
  CHECK_EQ(old_capacity, new_capacity);
}

// Test that HAllocateObject will always return an object in new-space.
TEST_F(HeapTest, OptimizedAllocationAlwaysInNewSpace) {
  if (v8_flags.single_generation) return;
  v8_flags.allow_natives_syntax = true;
  v8_flags.stress_concurrent_allocation = false;  // For SimulateFullSpace.
  if (!isolate()->use_optimizer() || v8_flags.always_turbofan) return;
  if (v8_flags.gc_global || v8_flags.stress_compaction ||
      v8_flags.stress_incremental_marking)
    return;
  v8::Isolate* iso = reinterpret_cast<v8::Isolate*>(isolate());
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

  i::Handle<JSReceiver> o =
      v8::Utils::OpenHandle(*v8::Local<v8::Object>::Cast(res));

  CHECK(Heap::InYoungGeneration(*o));
}

namespace {
template <RememberedSetType direction>
static size_t GetRememberedSetSize(HeapObject obj) {
  size_t count = 0;
  auto chunk = MemoryChunk::FromHeapObject(obj);
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
  if (v8_flags.single_generation || v8_flags.stress_incremental_marking) return;
  v8_flags.stress_concurrent_allocation = false;  // For SealCurrentObjects.
  Factory* factory = isolate()->factory();
  Heap* heap = isolate()->heap();
  SealCurrentObjects();
  HandleScope scope(isolate());

  // Create a young object and age it one generation inside the new space.
  Handle<FixedArray> arr = factory->NewFixedArray(1);
  std::vector<Handle<FixedArray>> handles;
  if (v8_flags.minor_mc) {
    NewSpace* new_space = heap->new_space();
    CHECK(!new_space->IsAtMaximumCapacity());
    // Fill current pages to force MinorMC to promote them.
    SimulateFullSpace(new_space, &handles);
    SafepointScope scope(heap);
    // New empty pages should remain in new space.
    new_space->Grow();
  } else {
    CollectGarbage(i::NEW_SPACE);
  }
  CHECK(Heap::InYoungGeneration(*arr));

  // Add into 'arr' a reference to an object one generation younger.
  {
    HandleScope scope_inner(isolate());
    Handle<Object> number = factory->NewHeapNumber(42);
    arr->set(0, *number);
  }

  // Promote 'arr' into old, its element is still in new, the old to new
  // refs are inserted into the remembered sets during GC.
  CollectGarbage(i::NEW_SPACE);

  CHECK(heap->InOldSpace(*arr));
  CHECK(heap->InYoungGeneration(arr->get(0)));
  CHECK_EQ(1, GetRememberedSetSize<OLD_TO_NEW>(*arr));
}

TEST_F(HeapTest, Regress978156) {
  if (!v8_flags.incremental_marking) return;
  if (v8_flags.single_generation) return;
  ManualGCScope manual_gc_scope(isolate());

  HandleScope handle_scope(isolate());
  Heap* heap = isolate()->heap();

  // 1. Ensure that the new space is empty.
  GcAndSweep(OLD_SPACE);
  // 2. Fill the new space with FixedArrays.
  std::vector<Handle<FixedArray>> arrays;
  SimulateFullSpace(heap->new_space(), &arrays);
  // 3. Trim the last array by one word thus creating a one-word filler.
  Handle<FixedArray> last = arrays.back();
  CHECK_GT(last->length(), 0);
  heap->RightTrimFixedArray(*last, 1);
  // 4. Get the last filler on the page.
  HeapObject filler = HeapObject::FromAddress(
      MemoryChunk::FromHeapObject(*last)->area_end() - kTaggedSize);
  HeapObject::FromAddress(last->address() + last->Size());
  CHECK(filler.IsFiller());
  // 5. Start incremental marking.
  i::IncrementalMarking* marking = heap->incremental_marking();
  if (marking->IsStopped()) {
    SafepointScope scope(heap);
    heap->tracer()->StartCycle(
        GarbageCollector::MARK_COMPACTOR, GarbageCollectionReason::kTesting,
        "collector cctest", GCTracer::MarkingType::kIncremental);
    marking->Start(GarbageCollector::MARK_COMPACTOR,
                   i::GarbageCollectionReason::kTesting);
  }
  MarkingState* marking_state = marking->marking_state();
  // 6. Mark the filler black to access its two markbits. This triggers
  // an out-of-bounds access of the marking bitmap in a bad case.
  marking_state->WhiteToGrey(filler);
  marking_state->GreyToBlack(filler);
}

}  // namespace internal
}  // namespace v8
