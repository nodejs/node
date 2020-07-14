// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/sweeper.h"

#include <algorithm>

#include "include/cppgc/allocation.h"
#include "include/cppgc/persistent.h"
#include "src/heap/cppgc/globals.h"
#include "src/heap/cppgc/heap-object-header-inl.h"
#include "src/heap/cppgc/heap-object-header.h"
#include "src/heap/cppgc/heap-page.h"
#include "src/heap/cppgc/heap.h"
#include "src/heap/cppgc/page-memory-inl.h"
#include "src/heap/cppgc/page-memory.h"
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
    Sweeper& sweeper = Heap::From(GetHeap())->sweeper();
    sweeper.Start(Sweeper::Config::kAtomic);
    sweeper.Finish();
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

  MakeGarbageCollected<Type>(GetHeap());

  EXPECT_EQ(0u, g_destructor_callcount);

  Sweep();

  EXPECT_EQ(1u, g_destructor_callcount);
}

TEST_F(SweeperTest, DontSweepMarkedNormalObject) {
  constexpr size_t kObjectSize = 8;
  using Type = GCed<kObjectSize>;

  auto* object = MakeGarbageCollected<Type>(GetHeap());
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

  auto* object = MakeGarbageCollected<Type>(GetHeap());
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

  auto* object = MakeGarbageCollected<Type>(GetHeap());
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
    MakeGarbageCollected<Type>(GetHeap());
  }

  EXPECT_EQ(0u, g_destructor_callcount);

  Sweep();

  EXPECT_EQ(kNumberOfObjects, g_destructor_callcount);
}

TEST_F(SweeperTest, SweepObjectsOnAllArenas) {
  MakeGarbageCollected<GCed<1>>(GetHeap());
  MakeGarbageCollected<GCed<32>>(GetHeap());
  MakeGarbageCollected<GCed<64>>(GetHeap());
  MakeGarbageCollected<GCed<128>>(GetHeap());
  MakeGarbageCollected<GCed<2 * kLargeObjectSizeThreshold>>(GetHeap());

  EXPECT_EQ(0u, g_destructor_callcount);

  Sweep();

  EXPECT_EQ(5u, g_destructor_callcount);
}

TEST_F(SweeperTest, SweepMultiplePagesInSingleSpace) {
  MakeGarbageCollected<GCed<2 * kLargeObjectSizeThreshold>>(GetHeap());
  MakeGarbageCollected<GCed<2 * kLargeObjectSizeThreshold>>(GetHeap());
  MakeGarbageCollected<GCed<2 * kLargeObjectSizeThreshold>>(GetHeap());

  EXPECT_EQ(0u, g_destructor_callcount);

  Sweep();

  EXPECT_EQ(3u, g_destructor_callcount);
}

TEST_F(SweeperTest, CoalesceFreeListEntries) {
  constexpr size_t kObjectSize = 32;
  using Type = GCed<kObjectSize>;

  auto* object1 = MakeGarbageCollected<Type>(GetHeap());
  auto* object2 = MakeGarbageCollected<Type>(GetHeap());
  auto* object3 = MakeGarbageCollected<Type>(GetHeap());
  auto* object4 = MakeGarbageCollected<Type>(GetHeap());

  MarkObject(object1);
  MarkObject(object4);

  Address object2_start =
      reinterpret_cast<Address>(&HeapObjectHeader::FromPayload(object2));
  Address object3_end =
      reinterpret_cast<Address>(&HeapObjectHeader::FromPayload(object3)) +
      HeapObjectHeader::FromPayload(object3).GetSize();

  const BasePage* page = BasePage::FromPayload(object2);
  const FreeList& freelist = NormalPageSpace::From(page->space())->free_list();

  const FreeList::Block coalesced_block = {object2_start,
                                           object3_end - object2_start};

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
    heap_->CollectGarbage(internal::Heap::GCConfig::Default());
  }

 private:
  Heap* heap_;
};

}  // namespace

TEST_F(SweeperTest, SweepDoesNotTriggerRecursiveGC) {
  auto* internal_heap = internal::Heap::From(GetHeap());
  size_t saved_epoch = internal_heap->epoch();
  MakeGarbageCollected<GCInDestructor>(GetHeap(), internal_heap);
  PreciseGC();
  EXPECT_EQ(saved_epoch + 1, internal_heap->epoch());
}

}  // namespace internal
}  // namespace cppgc
