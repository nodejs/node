// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/heap.h"

#include <algorithm>
#include <iterator>
#include <numeric>

#include "include/cppgc/allocation.h"
#include "src/heap/cppgc/globals.h"
#include "test/unittests/heap/cppgc/tests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cppgc {
namespace internal {

namespace {

class GCHeapTest : public testing::TestWithHeap {
 public:
  void ConservativeGC() {
    internal::Heap::From(GetHeap())->CollectGarbage(
        {Heap::GCConfig::StackState::kMayContainHeapPointers});
  }
  void PreciseGC() {
    internal::Heap::From(GetHeap())->CollectGarbage(
        {Heap::GCConfig::StackState::kNoHeapPointers});
  }
};

class Foo : public GarbageCollected<Foo> {
 public:
  static size_t destructor_callcount;

  Foo() { destructor_callcount = 0; }
  ~Foo() { destructor_callcount++; }
};

size_t Foo::destructor_callcount;

template <size_t Size>
class GCed : public GarbageCollected<Foo> {
 public:
  void Visit(cppgc::Visitor*) {}
  char buf[Size];
};

}  // namespace

TEST_F(GCHeapTest, PreciseGCReclaimsObjectOnStack) {
  Foo* volatile do_not_access = MakeGarbageCollected<Foo>(GetHeap());
  USE(do_not_access);
  EXPECT_EQ(0u, Foo::destructor_callcount);
  PreciseGC();
  EXPECT_EQ(1u, Foo::destructor_callcount);
  PreciseGC();
  EXPECT_EQ(1u, Foo::destructor_callcount);
}

namespace {

const void* ConservativeGCReturningObject(cppgc::Heap* heap,
                                          const void* volatile object) {
  internal::Heap::From(heap)->CollectGarbage(
      {Heap::GCConfig::StackState::kMayContainHeapPointers});
  return object;
}

}  // namespace

TEST_F(GCHeapTest, ConservativeGCRetainsObjectOnStack) {
  Foo* volatile object = MakeGarbageCollected<Foo>(GetHeap());
  EXPECT_EQ(0u, Foo::destructor_callcount);
  EXPECT_EQ(object, ConservativeGCReturningObject(GetHeap(), object));
  EXPECT_EQ(0u, Foo::destructor_callcount);
  PreciseGC();
  EXPECT_EQ(1u, Foo::destructor_callcount);
  PreciseGC();
  EXPECT_EQ(1u, Foo::destructor_callcount);
}

TEST_F(GCHeapTest, ObjectPayloadSize) {
  static constexpr size_t kNumberOfObjectsPerArena = 16;
  static constexpr size_t kObjectSizes[] = {1, 32, 64, 128,
                                            2 * kLargeObjectSizeThreshold};

  Heap::From(GetHeap())->CollectGarbage();

  for (size_t k = 0; k < kNumberOfObjectsPerArena; ++k) {
    MakeGarbageCollected<GCed<kObjectSizes[0]>>(GetHeap());
    MakeGarbageCollected<GCed<kObjectSizes[1]>>(GetHeap());
    MakeGarbageCollected<GCed<kObjectSizes[2]>>(GetHeap());
    MakeGarbageCollected<GCed<kObjectSizes[3]>>(GetHeap());
    MakeGarbageCollected<GCed<kObjectSizes[4]>>(GetHeap());
  }

  size_t aligned_object_sizes[arraysize(kObjectSizes)];
  std::transform(std::cbegin(kObjectSizes), std::cend(kObjectSizes),
                 std::begin(aligned_object_sizes), [](size_t size) {
                   return RoundUp(size, kAllocationGranularity);
                 });
  const size_t expected_size = std::accumulate(
      std::cbegin(aligned_object_sizes), std::cend(aligned_object_sizes), 0u,
      [](size_t acc, size_t size) {
        return acc + kNumberOfObjectsPerArena * size;
      });
  // TODO(chromium:1056170): Change to EXPECT_EQ when proper sweeping is
  // implemented.
  EXPECT_LE(expected_size, Heap::From(GetHeap())->ObjectPayloadSize());
}

}  // namespace internal
}  // namespace cppgc
