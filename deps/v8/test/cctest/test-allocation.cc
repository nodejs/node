// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <stdlib.h>
#include <string.h>

#if V8_OS_POSIX
#include <setjmp.h>
#include <signal.h>
#include <unistd.h>  // NOLINT
#endif

#include "src/v8.h"

#include "test/cctest/cctest.h"

using v8::internal::AccountingAllocator;

using v8::IdleTask;
using v8::Isolate;
using v8::Task;

#include "src/allocation.h"
#include "src/zone/accounting-allocator.h"

// ASAN isn't configured to return nullptr, so skip all of these tests.
#if !defined(V8_USE_ADDRESS_SANITIZER) && !defined(MEMORY_SANITIZER) && \
    !defined(THREAD_SANITIZER)

namespace {

// Implementation of v8::Platform that can register OOM callbacks.
class AllocationPlatform : public TestPlatform {
 public:
  AllocationPlatform() {
    current_platform = this;
    // Now that it's completely constructed, make this the current platform.
    i::V8::SetPlatformForTesting(this);
  }
  virtual ~AllocationPlatform() = default;

  void OnCriticalMemoryPressure() override { oom_callback_called = true; }

  bool OnCriticalMemoryPressure(size_t length) override {
    oom_callback_called = true;
    return true;
  }

  static AllocationPlatform* current_platform;
  bool oom_callback_called = false;
};

AllocationPlatform* AllocationPlatform::current_platform = nullptr;

bool DidCallOnCriticalMemoryPressure() {
  return AllocationPlatform::current_platform &&
         AllocationPlatform::current_platform->oom_callback_called;
}

// No OS should be able to malloc/new this number of bytes. Generate enough
// random values in the address space to get a very large fraction of it. Using
// even larger values is that overflow from rounding or padding can cause the
// allocations to succeed somehow.
size_t GetHugeMemoryAmount() {
  static size_t huge_memory = 0;
  if (!huge_memory) {
    for (int i = 0; i < 100; i++) {
      huge_memory |= bit_cast<size_t>(v8::internal::GetRandomMmapAddr());
    }
    // Make it larger than the available address space.
    huge_memory *= 2;
    CHECK_NE(0, huge_memory);
  }
  return huge_memory;
}

void OnMallocedOperatorNewOOM(const char* location, const char* message) {
  // exit(0) if the OOM callback was called and location matches expectation.
  if (DidCallOnCriticalMemoryPressure())
    exit(strcmp(location, "Malloced operator new"));
  exit(1);
}

void OnNewArrayOOM(const char* location, const char* message) {
  // exit(0) if the OOM callback was called and location matches expectation.
  if (DidCallOnCriticalMemoryPressure()) exit(strcmp(location, "NewArray"));
  exit(1);
}

void OnAlignedAllocOOM(const char* location, const char* message) {
  // exit(0) if the OOM callback was called and location matches expectation.
  if (DidCallOnCriticalMemoryPressure()) exit(strcmp(location, "AlignedAlloc"));
  exit(1);
}

}  // namespace

TEST(AccountingAllocatorOOM) {
  AllocationPlatform platform;
  v8::internal::AccountingAllocator allocator;
  CHECK(!platform.oom_callback_called);
  v8::internal::Segment* result = allocator.GetSegment(GetHugeMemoryAmount());
  // On a few systems, allocation somehow succeeds.
  CHECK_EQ(result == nullptr, platform.oom_callback_called);
}

TEST(MallocedOperatorNewOOM) {
  AllocationPlatform platform;
  CHECK(!platform.oom_callback_called);
  CcTest::isolate()->SetFatalErrorHandler(OnMallocedOperatorNewOOM);
  // On failure, this won't return, since a Malloced::New failure is fatal.
  // In that case, behavior is checked in OnMallocedOperatorNewOOM before exit.
  void* result = v8::internal::Malloced::New(GetHugeMemoryAmount());
  // On a few systems, allocation somehow succeeds.
  CHECK_EQ(result == nullptr, platform.oom_callback_called);
}

TEST(NewArrayOOM) {
  AllocationPlatform platform;
  CHECK(!platform.oom_callback_called);
  CcTest::isolate()->SetFatalErrorHandler(OnNewArrayOOM);
  // On failure, this won't return, since a NewArray failure is fatal.
  // In that case, behavior is checked in OnNewArrayOOM before exit.
  int8_t* result = v8::internal::NewArray<int8_t>(GetHugeMemoryAmount());
  // On a few systems, allocation somehow succeeds.
  CHECK_EQ(result == nullptr, platform.oom_callback_called);
}

TEST(AlignedAllocOOM) {
  AllocationPlatform platform;
  CHECK(!platform.oom_callback_called);
  CcTest::isolate()->SetFatalErrorHandler(OnAlignedAllocOOM);
  // On failure, this won't return, since an AlignedAlloc failure is fatal.
  // In that case, behavior is checked in OnAlignedAllocOOM before exit.
  void* result = v8::internal::AlignedAlloc(GetHugeMemoryAmount(),
                                            v8::internal::AllocatePageSize());
  // On a few systems, allocation somehow succeeds.
  CHECK_EQ(result == nullptr, platform.oom_callback_called);
}

TEST(AllocVirtualMemoryOOM) {
  AllocationPlatform platform;
  CHECK(!platform.oom_callback_called);
  v8::internal::VirtualMemory result;
  bool success =
      v8::internal::AllocVirtualMemory(GetHugeMemoryAmount(), nullptr, &result);
  // On a few systems, allocation somehow succeeds.
  CHECK_IMPLIES(success, result.IsReserved());
  CHECK_IMPLIES(!success, !result.IsReserved() && platform.oom_callback_called);
}

TEST(AlignedAllocVirtualMemoryOOM) {
  AllocationPlatform platform;
  CHECK(!platform.oom_callback_called);
  v8::internal::VirtualMemory result;
  bool success = v8::internal::AlignedAllocVirtualMemory(
      GetHugeMemoryAmount(), v8::internal::AllocatePageSize(), nullptr,
      &result);
  // On a few systems, allocation somehow succeeds.
  CHECK_IMPLIES(success, result.IsReserved());
  CHECK_IMPLIES(!success, !result.IsReserved() && platform.oom_callback_called);
}

#endif  // !defined(V8_USE_ADDRESS_SANITIZER) && !defined(MEMORY_SANITIZER) &&
        // !defined(THREAD_SANITIZER)
