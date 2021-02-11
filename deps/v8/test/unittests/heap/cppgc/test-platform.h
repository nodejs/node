// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UNITTESTS_HEAP_CPPGC_TEST_PLATFORM_H_
#define V8_UNITTESTS_HEAP_CPPGC_TEST_PLATFORM_H_

#include "include/cppgc/default-platform.h"
#include "src/base/compiler-specific.h"

namespace cppgc {
namespace internal {
namespace testing {

class TestPlatform : public DefaultPlatform {
 public:
  class DisableBackgroundTasksScope {
   public:
    explicit DisableBackgroundTasksScope(TestPlatform*);
    ~DisableBackgroundTasksScope() V8_NOEXCEPT;

   private:
    TestPlatform* platform_;
  };

  TestPlatform();

  std::unique_ptr<cppgc::JobHandle> PostJob(
      cppgc::TaskPriority priority,
      std::unique_ptr<cppgc::JobTask> job_task) final;

  void RunAllForegroundTasks();

 private:
  bool AreBackgroundTasksDisabled() const {
    return disabled_background_tasks_ > 0;
  }

  size_t disabled_background_tasks_ = 0;
};

}  // namespace testing
}  // namespace internal
}  // namespace cppgc

#endif  // V8_UNITTESTS_HEAP_CPPGC_TEST_PLATFORM_H_
