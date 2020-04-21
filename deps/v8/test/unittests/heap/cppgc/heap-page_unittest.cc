// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/heap-page.h"
#include "include/cppgc/allocation.h"
#include "include/cppgc/internal/accessors.h"
#include "test/unittests/heap/cppgc/tests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cppgc {
namespace internal {

namespace {

class PageTest : public testing::TestWithHeap {};

class GCed : public GarbageCollected<GCed> {};

}  // namespace

TEST_F(PageTest, PageLayout) {
  auto* np = NormalPage::Create(Heap::From(GetHeap()));
  NormalPage::Destroy(np);
}

TEST_F(PageTest, GetHeapForAllocatedObject) {
  GCed* gced = MakeGarbageCollected<GCed>(GetHeap());
  EXPECT_EQ(GetHeap(), GetHeapFromPayload(gced));
}

}  // namespace internal
}  // namespace cppgc
