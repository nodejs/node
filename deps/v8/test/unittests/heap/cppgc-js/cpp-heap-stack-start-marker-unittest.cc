// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "include/libplatform/libplatform.h"
#include "include/v8-cppgc.h"
#include "src/execution/isolate.h"
#include "src/heap/cppgc-js/cpp-heap.h"
#include "src/heap/heap.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

namespace {

class CppHeapStackStartMarkerTest : public TestWithPlatform {};

}  // namespace

TEST_F(CppHeapStackStartMarkerTest, StackStartMarkerIsUsed) {
  cppgc::StackStartMarker marker;
  CppHeapCreateParams params({});
  params.stack_start_marker = marker;

  auto cpp_heap = CppHeap::Create(platform(), params);
  CppHeap* internal_heap = CppHeap::From(cpp_heap.get());

  EXPECT_EQ(marker.stack_start(), internal_heap->stack()->stack_start());
  EXPECT_EQ(marker.stack_start(),
            internal_heap->stack_start_marker()->stack_start());
  EXPECT_LT(internal_heap->stack()->stack_start(),
            v8::base::Stack::GetStackStart());
}

TEST_F(CppHeapStackStartMarkerTest, DefaultWithoutStackStartMarker) {
  CppHeapCreateParams params({});
  EXPECT_FALSE(params.stack_start_marker.has_value());

  auto cpp_heap = CppHeap::Create(platform(), params);
  CppHeap* internal_heap = CppHeap::From(cpp_heap.get());

  // Without a marker, it should use the default stack start.
  EXPECT_EQ(v8::base::Stack::GetStackStart(),
            internal_heap->stack()->stack_start());
  EXPECT_FALSE(internal_heap->stack_start_marker().has_value());
}

TEST_F(CppHeapStackStartMarkerTest, IsolateUsesStackStartMarker) {
  cppgc::StackStartMarker marker;
  CppHeapCreateParams params({});
  params.stack_start_marker = marker;

  auto cpp_heap = CppHeap::Create(platform(), params);
  const void* expected_stack_start = marker.stack_start();

  IsolateWrapper::set_cpp_heap_for_next_isolate(std::move(cpp_heap));
  IsolateWrapper isolate_wrapper(kNoCounters);

  i::Isolate* i_isolate = isolate_wrapper.i_isolate();
  EXPECT_EQ(expected_stack_start, i_isolate->heap()->stack().stack_start());
}

}  // namespace internal
}  // namespace v8
