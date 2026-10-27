// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "include/cppgc/heap.h"
#include "src/heap/base/stack.h"
#include "src/heap/cppgc-internal/heap.h"
#include "test/unittests/heap/cppgc/tests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cppgc {
namespace internal {

namespace {

class StackStartMarkerTest : public testing::TestWithPlatform {};

}  // namespace

TEST_F(StackStartMarkerTest, StackStartMarkerIsUsed) {
  StackStartMarker marker;
  Heap::HeapOptions options;
  options.stack_start_marker = marker;

  auto heap = Heap::Create(GetPlatformHandle(), std::move(options));
  Heap* internal_heap = Heap::From(heap.get());

  EXPECT_EQ(marker.stack_start(), internal_heap->stack()->stack_start());
  EXPECT_LT(internal_heap->stack()->stack_start(),
            v8::base::Stack::GetStackStart());
}

TEST_F(StackStartMarkerTest, DefaultWithoutStackStartMarker) {
  Heap::HeapOptions options;
  EXPECT_FALSE(options.stack_start_marker.has_value());

  auto heap = Heap::Create(GetPlatformHandle(), std::move(options));
  Heap* internal_heap = Heap::From(heap.get());

  // Without a marker, it should use the default stack start.
  EXPECT_EQ(v8::base::Stack::GetStackStart(),
            internal_heap->stack()->stack_start());
}

TEST_F(StackStartMarkerTest, StackStartScopeIsUsed) {
  const void* original_stack_start = v8::base::Stack::GetStackStart();
  EXPECT_EQ(original_stack_start, v8::base::Stack::GetEffectiveStackStart());
  {
    StackStartScope scope;
    EXPECT_GT(original_stack_start, v8::base::Stack::GetEffectiveStackStart());
    EXPECT_EQ(original_stack_start, v8::base::Stack::GetStackStart());

    auto heap = Heap::Create(GetPlatformHandle());
    Heap* internal_heap = Heap::From(heap.get());

    EXPECT_EQ(v8::base::Stack::GetEffectiveStackStart(),
              internal_heap->stack()->stack_start());
    EXPECT_TRUE(internal_heap->ShouldCheckForTLSOnStack());
  }
  EXPECT_EQ(original_stack_start, v8::base::Stack::GetEffectiveStackStart());
}

V8_NOINLINE void TestNestedStackStartScope(const void* outer_stack_start) {
  StackStartScope inner_scope;
  EXPECT_GT(outer_stack_start, v8::base::Stack::GetEffectiveStackStart());
}

TEST_F(StackStartMarkerTest, NestedStackStartScopes) {
  const void* original_stack_start = v8::base::Stack::GetEffectiveStackStart();
  {
    StackStartScope outer_scope;
    const void* outer_stack_start = v8::base::Stack::GetEffectiveStackStart();
    EXPECT_LT(outer_stack_start, original_stack_start);
    TestNestedStackStartScope(outer_stack_start);
    EXPECT_EQ(outer_stack_start, v8::base::Stack::GetEffectiveStackStart());
  }
  EXPECT_EQ(original_stack_start, v8::base::Stack::GetEffectiveStackStart());
}

}  // namespace internal
}  // namespace cppgc
