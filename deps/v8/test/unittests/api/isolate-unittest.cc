// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "include/libplatform/libplatform.h"
#include "include/v8-platform.h"
#include "include/v8.h"
#include "src/base/macros.h"
#include "src/base/platform/semaphore.h"
#include "src/execution.h"
#include "src/isolate.h"
#include "src/v8.h"
#include "test/unittests/test-utils.h"

namespace v8 {

typedef TestWithIsolate IsolateTest;

namespace {

class MemoryPressureTask : public v8::Task {
 public:
  MemoryPressureTask(Isolate* isolate, base::Semaphore* semaphore)
      : isolate_(isolate), semaphore_(semaphore) {}
  ~MemoryPressureTask() override = default;

  // v8::Task implementation.
  void Run() override {
    isolate_->MemoryPressureNotification(MemoryPressureLevel::kCritical);
    semaphore_->Signal();
  }

 private:
  Isolate* isolate_;
  base::Semaphore* semaphore_;

  DISALLOW_COPY_AND_ASSIGN(MemoryPressureTask);
};

}  // namespace

// Check that triggering a memory pressure notification on the isolate thread
// doesn't request a GC interrupt.
TEST_F(IsolateTest, MemoryPressureNotificationForeground) {
  internal::Isolate* i_isolate =
      reinterpret_cast<internal::Isolate*>(isolate());

  ASSERT_FALSE(i_isolate->stack_guard()->CheckGC());
  isolate()->MemoryPressureNotification(MemoryPressureLevel::kCritical);
  ASSERT_FALSE(i_isolate->stack_guard()->CheckGC());
}

// Check that triggering a memory pressure notification on an background thread
// requests a GC interrupt.
TEST_F(IsolateTest, MemoryPressureNotificationBackground) {
  internal::Isolate* i_isolate =
      reinterpret_cast<internal::Isolate*>(isolate());

  base::Semaphore semaphore(0);

  internal::V8::GetCurrentPlatform()->CallOnBackgroundThread(
      new MemoryPressureTask(isolate(), &semaphore),
      v8::Platform::kShortRunningTask);

  semaphore.Wait();

  ASSERT_TRUE(i_isolate->stack_guard()->CheckGC());
  v8::platform::PumpMessageLoop(internal::V8::GetCurrentPlatform(), isolate());
}

}  // namespace v8
