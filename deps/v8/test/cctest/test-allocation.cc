// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <stdlib.h>
#include <string.h>

#if V8_OS_POSIX
#include <setjmp.h>
#include <signal.h>
#include <unistd.h>
#endif

#include "src/init/v8.h"

#include "test/cctest/cctest.h"

using v8::internal::AccountingAllocator;

using v8::IdleTask;
using v8::Isolate;
using v8::Task;

#include "src/base/platform/platform.h"
#include "src/utils/allocation.h"
#include "src/zone/accounting-allocator.h"

// ASAN isn't configured to return nullptr, so skip all of these tests.
#if !defined(V8_USE_ADDRESS_SANITIZER) && !defined(MEMORY_SANITIZER) && \
    !defined(THREAD_SANITIZER)

namespace {

// Implementation of v8::Platform that can register OOM callbacks.
class AllocationPlatform : public TestPlatform {
 public:
  AllocationPlatform() { current_platform = this; }

  void OnCriticalMemoryPressure() override { oom_callback_called = true; }

  static AllocationPlatform* current_platform;
  bool oom_callback_called = false;
};

AllocationPlatform* AllocationPlatform::current_platform = nullptr;

}  // namespace

// We use |AllocateAtLeast| in the accounting allocator, so we check only that
// we have _at least_ the expected amount of memory allocated.
TEST_WITH_PLATFORM(AccountingAllocatorCurrentAndMax, AllocationPlatform) {
  v8::internal::AccountingAllocator allocator;
  static constexpr size_t kAllocationSizes[] = {51, 231, 27};
  std::vector<v8::internal::Segment*> segments;
  CHECK_EQ(0, allocator.GetCurrentMemoryUsage());
  CHECK_EQ(0, allocator.GetMaxMemoryUsage());
  size_t expected_current = 0;
  size_t expected_max = 0;
  for (size_t size : kAllocationSizes) {
    segments.push_back(allocator.AllocateSegment(size));
    CHECK_NOT_NULL(segments.back());
    CHECK_LE(size, segments.back()->total_size());
    expected_current += segments.back()->total_size();
    if (expected_current > expected_max) expected_max = expected_current;
    CHECK_EQ(expected_current, allocator.GetCurrentMemoryUsage());
    CHECK_EQ(expected_max, allocator.GetMaxMemoryUsage());
  }
  for (auto* segment : segments) {
    expected_current -= segment->total_size();
    allocator.ReturnSegment(segment);
    CHECK_EQ(expected_current, allocator.GetCurrentMemoryUsage());
  }
  CHECK_EQ(expected_max, allocator.GetMaxMemoryUsage());
  CHECK_EQ(0, allocator.GetCurrentMemoryUsage());
  CHECK(!platform.oom_callback_called);
}

#endif  // !defined(V8_USE_ADDRESS_SANITIZER) && !defined(MEMORY_SANITIZER) &&
        // !defined(THREAD_SANITIZER)
