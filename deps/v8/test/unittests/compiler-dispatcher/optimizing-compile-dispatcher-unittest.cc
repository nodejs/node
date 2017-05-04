// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler-dispatcher/optimizing-compile-dispatcher.h"

#include "src/base/atomic-utils.h"
#include "src/base/platform/semaphore.h"
#include "src/compilation-info.h"
#include "src/compiler.h"
#include "src/handles.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/parsing/parse-info.h"
#include "test/unittests/test-helpers.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

typedef TestWithContext OptimizingCompileDispatcherTest;

namespace {

class BlockingCompilationJob : public CompilationJob {
 public:
  BlockingCompilationJob(Isolate* isolate, Handle<JSFunction> function)
      : CompilationJob(isolate, &info_, "BlockingCompilationJob",
                       State::kReadyToExecute),
        parse_info_(handle(function->shared())),
        info_(parse_info_.zone(), &parse_info_, function->GetIsolate(),
              function),
        blocking_(false),
        semaphore_(0) {}
  ~BlockingCompilationJob() override = default;

  bool IsBlocking() const { return blocking_.Value(); }
  void Signal() { semaphore_.Signal(); }

  // CompilationJob implementation.
  Status PrepareJobImpl() override {
    UNREACHABLE();
    return FAILED;
  }

  Status ExecuteJobImpl() override {
    blocking_.SetValue(true);
    semaphore_.Wait();
    blocking_.SetValue(false);
    return SUCCEEDED;
  }

  Status FinalizeJobImpl() override { return SUCCEEDED; }

 private:
  ParseInfo parse_info_;
  CompilationInfo info_;
  base::AtomicValue<bool> blocking_;
  base::Semaphore semaphore_;

  DISALLOW_COPY_AND_ASSIGN(BlockingCompilationJob);
};

}  // namespace

TEST_F(OptimizingCompileDispatcherTest, Construct) {
  OptimizingCompileDispatcher dispatcher(i_isolate());
  ASSERT_TRUE(OptimizingCompileDispatcher::Enabled());
  ASSERT_TRUE(dispatcher.IsQueueAvailable());
}

TEST_F(OptimizingCompileDispatcherTest, NonBlockingFlush) {
  Handle<JSFunction> fun = Handle<JSFunction>::cast(test::RunJS(
      isolate(), "function f() { function g() {}; return g;}; f();"));
  BlockingCompilationJob* job = new BlockingCompilationJob(i_isolate(), fun);

  OptimizingCompileDispatcher dispatcher(i_isolate());
  ASSERT_TRUE(OptimizingCompileDispatcher::Enabled());
  ASSERT_TRUE(dispatcher.IsQueueAvailable());
  dispatcher.QueueForOptimization(job);

  // Busy-wait for the job to run on a background thread.
  while (!job->IsBlocking()) {
  }

  // Should not block.
  dispatcher.Flush(OptimizingCompileDispatcher::BlockingBehavior::kDontBlock);

  // Unblock the job & finish.
  job->Signal();
  dispatcher.Stop();
}

}  // namespace internal
}  // namespace v8
