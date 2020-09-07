// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <include/cppgc/allocation.h>
#include <include/cppgc/garbage-collected.h>
#include <include/cppgc/heap.h>
#include <include/cppgc/member.h>
#include <include/cppgc/platform.h>
#include <include/cppgc/visitor.h>
#include <include/libplatform/libplatform.h>
#include <include/v8.h>

#include <iostream>
#include <memory>
#include <string>

/**
 * This sample program shows how to set up a stand-alone cppgc heap as an
 * embedder of V8. Most importantly, this example shows how to reuse V8's
 * platform for cppgc.
 */

/**
 * Platform used by cppgc. Can just redirect to v8::Platform for most calls.
 * Exception: GetForegroundTaskRunner(), see below.
 *
 * This example uses V8's default platform implementation to drive the cppgc
 * platform.
 */
class Platform final : public cppgc::Platform {
 public:
  Platform() : v8_platform_(v8::platform::NewDefaultPlatform()) {}

  cppgc::PageAllocator* GetPageAllocator() final {
    return v8_platform_->GetPageAllocator();
  }

  double MonotonicallyIncreasingTime() final {
    return v8_platform_->MonotonicallyIncreasingTime();
  }

  std::shared_ptr<cppgc::TaskRunner> GetForegroundTaskRunner() final {
    // V8's default platform creates a new task runner when passed the
    // v8::Isolate pointer the first time. For non-default platforms this will
    // require getting the appropriate task runner.
    return v8_platform_->GetForegroundTaskRunner(nullptr);
  }

  std::unique_ptr<cppgc::JobHandle> PostJob(
      cppgc::TaskPriority priority,
      std::unique_ptr<cppgc::JobTask> job_task) final {
    return v8_platform_->PostJob(priority, std::move(job_task));
  }

 private:
  std::unique_ptr<v8::Platform> v8_platform_;
};

/**
 * Simple string rope to illustrate allocation and garbage collection below. The
 * rope keeps the next parts alive via regular managed reference.
 */
class Rope final : public cppgc::GarbageCollected<Rope> {
 public:
  explicit Rope(std::string part, Rope* next = nullptr)
      : part_(part), next_(next) {}

  void Trace(cppgc::Visitor* visitor) const { visitor->Trace(next_); }

 private:
  std::string part_;
  cppgc::Member<Rope> next_;

  friend std::ostream& operator<<(std::ostream& os, const Rope& rope);
};

std::ostream& operator<<(std::ostream& os, const Rope& rope) {
  os << rope.part_;
  if (rope.next_) {
    os << *rope.next_;
  }
  return os;
}

int main(int argc, char* argv[]) {
  // Create a platform that is used by cppgc::Heap for execution and backend
  // allocation.
  auto cppgc_platform = std::make_shared<Platform>();
  // Initialize the process. This must happen before any cppgc::Heap::Create()
  // calls.
  cppgc::InitializeProcess(cppgc_platform->GetPageAllocator());
  // Create a managed heap.
  std::unique_ptr<cppgc::Heap> heap = cppgc::Heap::Create(cppgc_platform);
  // Allocate a string rope on the managed heap.
  auto* greeting = cppgc::MakeGarbageCollected<Rope>(
      heap->GetAllocationHandle(), "Hello ",
      cppgc::MakeGarbageCollected<Rope>(heap->GetAllocationHandle(), "World!"));
  // Manually trigger garbage collection. The object greeting is held alive
  // through conservative stack scanning.
  heap->ForceGarbageCollectionSlow("V8 embedders example", "Testing");
  std::cout << *greeting << std::endl;
  // Gracefully shutdown the process.
  cppgc::ShutdownProcess();
  return 0;
}
