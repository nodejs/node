// Copyright 2008 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "src/execution/thread-id.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

using ThreadsTest = TestWithIsolate;

// {ThreadId} must be trivially copyable to be stored in {std::atomic}.
ASSERT_TRIVIALLY_COPYABLE(i::ThreadId);
using AtomicThreadId = std::atomic<i::ThreadId>;

class ThreadIdValidationThread : public base::Thread {
 public:
  ThreadIdValidationThread(base::Thread* thread_to_start, AtomicThreadId* refs,
                           unsigned int thread_no, base::Semaphore* semaphore)
      : Thread(Options("ThreadRefValidationThread")),
        refs_(refs),
        thread_no_(thread_no),
        thread_to_start_(thread_to_start),
        semaphore_(semaphore) {}

  void Run() override {
    i::ThreadId thread_id = i::ThreadId::Current();
    CHECK(thread_id.IsValid());
    for (int i = 0; i < thread_no_; i++) {
      CHECK_NE(refs_[i].load(std::memory_order_relaxed), thread_id);
    }
    refs_[thread_no_].store(thread_id, std::memory_order_relaxed);
    if (thread_to_start_ != nullptr) {
      CHECK(thread_to_start_->Start());
    }
    semaphore_->Signal();
  }

 private:
  AtomicThreadId* const refs_;
  const int thread_no_;
  base::Thread* const thread_to_start_;
  base::Semaphore* const semaphore_;
};

TEST_F(ThreadsTest, ThreadIdValidation) {
  constexpr int kNThreads = 100;
  std::unique_ptr<ThreadIdValidationThread> threads[kNThreads];
  AtomicThreadId refs[kNThreads];
  base::Semaphore semaphore(0);
  for (int i = kNThreads - 1; i >= 0; i--) {
    ThreadIdValidationThread* prev =
        i == kNThreads - 1 ? nullptr : threads[i + 1].get();
    threads[i] =
        std::make_unique<ThreadIdValidationThread>(prev, refs, i, &semaphore);
  }
  CHECK(threads[0]->Start());
  for (int i = 0; i < kNThreads; i++) {
    semaphore.Wait();
  }
}

}  // namespace internal
}  // namespace v8
