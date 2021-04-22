// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/sweeper.h"

#include <algorithm>

#include "include/cppgc/allocation.h"
#include "include/cppgc/persistent.h"
#include "src/heap/cppgc/globals.h"
#include "src/heap/cppgc/heap-object-header.h"
#include "src/heap/cppgc/heap-page.h"
#include "src/heap/cppgc/heap-visitor.h"
#include "src/heap/cppgc/heap.h"
#include "src/heap/cppgc/page-memory.h"
#include "src/heap/cppgc/stats-collector.h"
#include "test/unittests/heap/cppgc/tests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cppgc {
namespace internal {

namespace {

size_t g_destructor_callcount;

template <size_t Size>
class GCed : public GarbageCollected<GCed<Size>> {
 public:
  virtual ~GCed() { ++g_destructor_callcount; }

  virtual void Trace(cppgc::Visitor*) const {}

 private:
  char array[Size];
};

class SweeperTest : public testing::TestWithHeap {
 public:
  SweeperTest() { g_destructor_callcount = 0; }

  void Sweep() {
    Heap* heap = Heap::From(GetHeap());
    ResetLinearAllocationBuffers();
    Sweeper& sweeper = heap->sweeper();
    // Pretend do finish marking as StatsCollector verifies that Notify*
    // methods are called in the right order.
    heap->stats_collector()->NotifyMarkingStarted(
        GarbageCollector::Config::CollectionType::kMajor,
        GarbageCollector::Config::IsForcedGC::kNotForced);
    heap->stats_collector()->NotifyMarkingCompleted(0);
    const Sweeper::SweepingConfig sweeping_config{
        Sweeper::SweepingConfig::SweepingType::kAtomic,
        Sweeper::SweepingConfig::CompactableSpaceHandling::kSweep};
    sweeper.Start(sweeping_config);
    sweeper.FinishIfRunning();
  }

  void MarkObject(void* payload) {
    HeapObjectHeader& header = HeapObjectHeader::FromPayload(payload);
    header.TryMarkAtomic();
  }

  PageBackend* GetBackend() { return Heap::From(GetHeap())->page_backend(); }
};

}  // namespace

TEST_F(SweeperTest, SweepUnmarkedNormalObject) {
  constexpr size_t kObjectSize = 8;
  using Type = GCed<kObjectSize>;

  MakeGarbageCollected<Type>(GetAllocationHandle());

  EXPECT_EQ(0u, g_destructor_callcount);

  Sweep();

  EXPECT_EQ(1u, g_destructor_callcount);
}

TEST_F(SweeperTest, DontSweepMarkedNormalObject) {
  constexpr size_t kObjectSize = 8;
  using Type = GCed<kObjectSize>;

  auto* object = MakeGarbageCollected<Type>(GetAllocationHandle());
  MarkObject(object);
  BasePage* page = BasePage::FromPayload(object);
  BaseSpace* space = page->space();

  EXPECT_EQ(0u, g_destructor_callcount);

  Sweep();

  EXPECT_EQ(0u, g_destructor_callcount);
  // Check that page is returned back to the space.
  EXPECT_NE(space->end(), std::find(space->begin(), space->end(), page));
  EXPECT_NE(nullptr, GetBackend()->Lookup(reinterpret_cast<Address>(object)));
}

TEST_F(SweeperTest, SweepUnmarkedLargeObject) {
  constexpr size_t kObjectSize = kLargeObjectSizeThreshold * 2;
  using Type = GCed<kObjectSize>;

  auto* object = MakeGarbageCollected<Type>(GetAllocationHandle());
  BasePage* page = BasePage::FromPayload(object);
  BaseSpace* space = page->space();

  EXPECT_EQ(0u, g_destructor_callcount);

  Sweep();

  EXPECT_EQ(1u, g_destructor_callcount);
  // Check that page is gone.
  EXPECT_EQ(space->end(), std::find(space->begin(), space->end(), page));
  EXPECT_EQ(nullptr, GetBackend()->Lookup(reinterpret_cast<Address>(object)));
}

TEST_F(SweeperTest, DontSweepMarkedLargeObject) {
  constexpr size_t kObjectSize = kLargeObjectSizeThreshold * 2;
  using Type = GCed<kObjectSize>;

  auto* object = MakeGarbageCollected<Type>(GetAllocationHandle());
  MarkObject(object);
  BasePage* page = BasePage::FromPayload(object);
  BaseSpace* space = page->space();

  EXPECT_EQ(0u, g_destructor_callcount);

  Sweep();

  EXPECT_EQ(0u, g_destructor_callcount);
  // Check that page is returned back to the space.
  EXPECT_NE(space->end(), std::find(space->begin(), space->end(), page));
  EXPECT_NE(nullptr, GetBackend()->Lookup(reinterpret_cast<Address>(object)));
}

TEST_F(SweeperTest, SweepMultipleObjectsOnPage) {
  constexpr size_t kObjectSize = 8;
  using Type = GCed<kObjectSize>;
  const size_t kNumberOfObjects =
      NormalPage::PayloadSize() / (sizeof(Type) + sizeof(HeapObjectHeader));

  for (size_t i = 0; i < kNumberOfObjects; ++i) {
    MakeGarbageCollected<Type>(GetAllocationHandle());
  }

  EXPECT_EQ(0u, g_destructor_callcount);

  Sweep();

  EXPECT_EQ(kNumberOfObjects, g_destructor_callcount);
}

TEST_F(SweeperTest, SweepObjectsOnAllArenas) {
  MakeGarbageCollected<GCed<1>>(GetAllocationHandle());
  MakeGarbageCollected<GCed<32>>(GetAllocationHandle());
  MakeGarbageCollected<GCed<64>>(GetAllocationHandle());
  MakeGarbageCollected<GCed<128>>(GetAllocationHandle());
  MakeGarbageCollected<GCed<2 * kLargeObjectSizeThreshold>>(
      GetAllocationHandle());

  EXPECT_EQ(0u, g_destructor_callcount);

  Sweep();

  EXPECT_EQ(5u, g_destructor_callcount);
}

TEST_F(SweeperTest, SweepMultiplePagesInSingleSpace) {
  MakeGarbageCollected<GCed<2 * kLargeObjectSizeThreshold>>(
      GetAllocationHandle());
  MakeGarbageCollected<GCed<2 * kLargeObjectSizeThreshold>>(
      GetAllocationHandle());
  MakeGarbageCollected<GCed<2 * kLargeObjectSizeThreshold>>(
      GetAllocationHandle());

  EXPECT_EQ(0u, g_destructor_callcount);

  Sweep();

  EXPECT_EQ(3u, g_destructor_callcount);
}

TEST_F(SweeperTest, CoalesceFreeListEntries) {
  constexpr size_t kObjectSize = 32;
  using Type = GCed<kObjectSize>;

  auto* object1 = MakeGarbageCollected<Type>(GetAllocationHandle());
  auto* object2 = MakeGarbageCollected<Type>(GetAllocationHandle());
  auto* object3 = MakeGarbageCollected<Type>(GetAllocationHandle());
  auto* object4 = MakeGarbageCollected<Type>(GetAllocationHandle());

  MarkObject(object1);
  MarkObject(object4);

  Address object2_start =
      reinterpret_cast<Address>(&HeapObjectHeader::FromPayload(object2));
  Address object3_end =
      reinterpret_cast<Address>(&HeapObjectHeader::FromPayload(object3)) +
      HeapObjectHeader::FromPayload(object3).GetSize();

  const BasePage* page = BasePage::FromPayload(object2);
  const FreeList& freelist = NormalPageSpace::From(page->space())->free_list();

  const FreeList::Block coalesced_block = {
      object2_start, static_cast<size_t>(object3_end - object2_start)};

  EXPECT_EQ(0u, g_destructor_callcount);
  EXPECT_FALSE(freelist.Contains(coalesced_block));

  Sweep();

  EXPECT_EQ(2u, g_destructor_callcount);
  EXPECT_TRUE(freelist.Contains(coalesced_block));
}

namespace {

class GCInDestructor final : public GarbageCollected<GCInDestructor> {
 public:
  explicit GCInDestructor(Heap* heap) : heap_(heap) {}
  ~GCInDestructor() {
    // Instead of directly calling GC, allocations should be supported here as
    // well.
    heap_->CollectGarbage(
        internal::GarbageCollector::Config::ConservativeAtomicConfig());
  }
  void Trace(Visitor*) const {}

 private:
  Heap* heap_;
};

}  // namespace

TEST_F(SweeperTest, SweepDoesNotTriggerRecursiveGC) {
  auto* internal_heap = internal::Heap::From(GetHeap());
  size_t saved_epoch = internal_heap->epoch();
  MakeGarbageCollected<GCInDestructor>(GetAllocationHandle(), internal_heap);
  PreciseGC();
  EXPECT_EQ(saved_epoch + 1, internal_heap->epoch());
}

TEST_F(SweeperTest, UnmarkObjects) {
  auto* normal_object = MakeGarbageCollected<GCed<32>>(GetAllocationHandle());
  auto* large_object =
      MakeGarbageCollected<GCed<kLargeObjectSizeThreshold * 2>>(
          GetAllocationHandle());

  auto& normal_object_header = HeapObjectHeader::FromPayload(normal_object);
  auto& large_object_header = HeapObjectHeader::FromPayload(large_object);

  normal_object_header.TryMarkAtomic();
  large_object_header.TryMarkAtomic();

  EXPECT_TRUE(normal_object_header.IsMarked());
  EXPECT_TRUE(large_object_header.IsMarked());

  Sweep();

#if !defined(CPPGC_YOUNG_GENERATION)
  EXPECT_FALSE(normal_object_header.IsMarked());
  EXPECT_FALSE(large_object_header.IsMarked());
#else
  EXPECT_TRUE(normal_object_header.IsMarked());
  EXPECT_TRUE(large_object_header.IsMarked());
#endif
}

TEST_F(SweeperTest, LazySweepingDuringAllocation) {
  using GCedObject = GCed<256>;
  static const size_t kObjectsPerPage =
      NormalPage::PayloadSize() /
      (sizeof(GCedObject) + sizeof(HeapObjectHeader));
  // This test expects each page contain at least 2 objects.
  DCHECK_LT(2u, kObjectsPerPage);
  PreciseGC();
  std::vector<Persistent<GCedObject>> first_page;
  first_page.push_back(MakeGarbageCollected<GCedObject>(GetAllocationHandle()));
  GCedObject* expected_address_on_first_page =
      MakeGarbageCollected<GCedObject>(GetAllocationHandle());
  for (size_t i = 2; i < kObjectsPerPage; ++i) {
    first_page.push_back(
        MakeGarbageCollected<GCedObject>(GetAllocationHandle()));
  }
  std::vector<Persistent<GCedObject>> second_page;
  second_page.push_back(
      MakeGarbageCollected<GCedObject>(GetAllocationHandle()));
  GCedObject* expected_address_on_second_page =
      MakeGarbageCollected<GCedObject>(GetAllocationHandle());
  for (size_t i = 2; i < kObjectsPerPage; ++i) {
    second_page.push_back(
        MakeGarbageCollected<GCedObject>(GetAllocationHandle()));
  }
  testing::TestPlatform::DisableBackgroundTasksScope no_concurrent_sweep_scope(
      GetPlatformHandle().get());
  g_destructor_callcount = 0;
  static constexpr Heap::Config config = {
      Heap::Config::CollectionType::kMajor,
      Heap::Config::StackState::kNoHeapPointers,
      Heap::Config::MarkingType::kAtomic,
      Heap::Config::SweepingType::kIncrementalAndConcurrent};
  Heap::From(GetHeap())->CollectGarbage(config);
  // Incremetal sweeping is active and the space should have two pages with
  // no room for an additional GCedObject. Allocating a new GCedObject should
  // trigger sweeping. All objects other than the 2nd object on each page are
  // marked. Lazy sweeping on allocation should reclaim the object on one of
  // the pages and reuse its memory. The object on the other page should remain
  // un-reclaimed. To confirm: the newly object will be allcoated at one of the
  // expected addresses and the GCedObject destructor is only called once.
  GCedObject* new_object1 =
      MakeGarbageCollected<GCedObject>(GetAllocationHandle());
  EXPECT_EQ(1u, g_destructor_callcount);
  EXPECT_TRUE((new_object1 == expected_address_on_first_page) ||
              (new_object1 == expected_address_on_second_page));
  // Allocating again should reclaim the other unmarked object and reuse its
  // memory. The destructor will be called again and the new object will be
  // allocated in one of the expected addresses but not the same one as before.
  GCedObject* new_object2 =
      MakeGarbageCollected<GCedObject>(GetAllocationHandle());
  EXPECT_EQ(2u, g_destructor_callcount);
  EXPECT_TRUE((new_object2 == expected_address_on_first_page) ||
              (new_object2 == expected_address_on_second_page));
  EXPECT_NE(new_object1, new_object2);
}

}  // namespace internal
}  // namespace cppgc
