// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "include/cppgc/allocation.h"
#include "include/cppgc/heap.h"
#include "src/heap/cppgc/heap-base.h"
#include "src/heap/cppgc/process-heap.h"
#include "test/unittests/heap/cppgc/tests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cppgc {
namespace internal {

class HeapRegistryTest : public testing::TestWithPlatform {};

TEST_F(HeapRegistryTest, Empty) {
  EXPECT_EQ(0u, HeapRegistry::GetRegisteredHeapsForTesting().size());
}

namespace {

bool Contains(const HeapRegistry::Storage& storage, const cppgc::Heap* needle) {
  return storage.end() !=
         std::find(storage.begin(), storage.end(),
                   &cppgc::internal::Heap::From(needle)->AsBase());
}

}  // namespace

TEST_F(HeapRegistryTest, RegisterUnregisterHeaps) {
  const auto& storage = HeapRegistry::GetRegisteredHeapsForTesting();
  EXPECT_EQ(0u, storage.size());
  {
    const auto heap1 = Heap::Create(platform_);
    EXPECT_TRUE(Contains(storage, heap1.get()));
    EXPECT_EQ(1u, storage.size());
    {
      const auto heap2 = Heap::Create(platform_);
      EXPECT_TRUE(Contains(storage, heap1.get()));
      EXPECT_TRUE(Contains(storage, heap2.get()));
      EXPECT_EQ(2u, storage.size());
    }
    EXPECT_TRUE(Contains(storage, heap1.get()));
    EXPECT_EQ(1u, storage.size());
  }
  EXPECT_EQ(0u, storage.size());
}

TEST_F(HeapRegistryTest, DoesNotFindNullptr) {
  const auto heap = Heap::Create(platform_);
  EXPECT_EQ(nullptr, HeapRegistry::TryFromManagedPointer(nullptr));
}

TEST_F(HeapRegistryTest, DoesNotFindStackAddress) {
  const auto heap = Heap::Create(platform_);
  EXPECT_EQ(nullptr, HeapRegistry::TryFromManagedPointer(&heap));
}

TEST_F(HeapRegistryTest, DoesNotFindOffHeap) {
  const auto heap = Heap::Create(platform_);
  auto dummy = std::make_unique<char>();
  EXPECT_EQ(nullptr, HeapRegistry::TryFromManagedPointer(dummy.get()));
}

namespace {

class GCed final : public GarbageCollected<GCed> {
 public:
  void Trace(Visitor*) const {}
};

}  // namespace

TEST_F(HeapRegistryTest, FindsRightHeapForOnHeapAddress) {
  const auto heap1 = Heap::Create(platform_);
  const auto heap2 = Heap::Create(platform_);
  auto* o = MakeGarbageCollected<GCed>(heap1->GetAllocationHandle());
  EXPECT_EQ(&cppgc::internal::Heap::From(heap1.get())->AsBase(),
            HeapRegistry::TryFromManagedPointer(o));
  EXPECT_NE(&cppgc::internal::Heap::From(heap2.get())->AsBase(),
            HeapRegistry::TryFromManagedPointer(o));
}

}  // namespace internal
}  // namespace cppgc
