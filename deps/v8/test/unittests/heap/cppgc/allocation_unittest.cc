// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cppgc/allocation.h"

#include <memory>

#include "src/heap/cppgc/heap.h"
#include "test/unittests/heap/cppgc/tests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cppgc {

TEST(GCBasicHeapTest, CreateAndDestroyHeap) {
  std::unique_ptr<Heap> heap{Heap::Create()};
}

namespace {

class Foo : public GarbageCollected<Foo> {
 public:
  static size_t destructor_callcount;

  Foo() { destructor_callcount = 0; }
  ~Foo() { destructor_callcount++; }
};

size_t Foo::destructor_callcount;

class GCAllocationTest : public testing::TestWithHeap {};

}  // namespace

TEST_F(GCAllocationTest, MakeGarbageCollectedAndReclaim) {
  MakeGarbageCollected<Foo>(GetHeap());
  EXPECT_EQ(0u, Foo::destructor_callcount);
  internal::Heap::From(GetHeap())->CollectGarbage();
  EXPECT_EQ(1u, Foo::destructor_callcount);
}

}  // namespace cppgc
