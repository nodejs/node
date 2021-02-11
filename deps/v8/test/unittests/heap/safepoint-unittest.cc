// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/safepoint.h"
#include "src/base/platform/mutex.h"
#include "src/base/platform/platform.h"
#include "src/heap/heap.h"
#include "src/heap/local-heap.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

void EnsureFlagLocalHeapsEnabled() {
  // Avoid data race in concurrent thread by only setting the flag to true if
  // not already enabled.
  if (!FLAG_local_heaps) FLAG_local_heaps = true;
}

using SafepointTest = TestWithIsolate;

TEST_F(SafepointTest, ReachSafepointWithoutLocalHeaps) {
  EnsureFlagLocalHeapsEnabled();
  Heap* heap = i_isolate()->heap();
  bool run = false;
  {
    SafepointScope scope(heap);
    run = true;
  }
  CHECK(run);
}

class ParkedThread final : public v8::base::Thread {
 public:
  ParkedThread(Heap* heap, base::Mutex* mutex)
      : v8::base::Thread(base::Thread::Options("ThreadWithLocalHeap")),
        heap_(heap),
        mutex_(mutex) {}

  void Run() override {
    LocalHeap local_heap(heap_, ThreadKind::kBackground);

    if (mutex_) {
      base::MutexGuard guard(mutex_);
    }
  }

  Heap* heap_;
  base::Mutex* mutex_;
};

TEST_F(SafepointTest, StopParkedThreads) {
  EnsureFlagLocalHeapsEnabled();
  Heap* heap = i_isolate()->heap();

  int safepoints = 0;

  const int kThreads = 10;
  const int kRuns = 5;

  for (int run = 0; run < kRuns; run++) {
    base::Mutex mutex;
    std::vector<ParkedThread*> threads;

    mutex.Lock();

    for (int i = 0; i < kThreads; i++) {
      ParkedThread* thread =
          new ParkedThread(heap, i % 2 == 0 ? &mutex : nullptr);
      CHECK(thread->Start());
      threads.push_back(thread);
    }

    {
      SafepointScope scope(heap);
      safepoints++;
    }
    mutex.Unlock();

    for (ParkedThread* thread : threads) {
      thread->Join();
      delete thread;
    }
  }

  CHECK_EQ(safepoints, kRuns);
}

static const int kRuns = 10000;

class RunningThread final : public v8::base::Thread {
 public:
  RunningThread(Heap* heap, std::atomic<int>* counter)
      : v8::base::Thread(base::Thread::Options("ThreadWithLocalHeap")),
        heap_(heap),
        counter_(counter) {}

  void Run() override {
    LocalHeap local_heap(heap_, ThreadKind::kBackground);
    UnparkedScope unparked_scope(&local_heap);

    for (int i = 0; i < kRuns; i++) {
      counter_->fetch_add(1);
      if (i % 100 == 0) local_heap.Safepoint();
    }
  }

  Heap* heap_;
  std::atomic<int>* counter_;
};

TEST_F(SafepointTest, StopRunningThreads) {
  EnsureFlagLocalHeapsEnabled();
  Heap* heap = i_isolate()->heap();

  const int kThreads = 10;
  const int kRuns = 5;
  const int kSafepoints = 3;
  int safepoint_count = 0;

  for (int run = 0; run < kRuns; run++) {
    std::atomic<int> counter(0);
    std::vector<RunningThread*> threads;

    for (int i = 0; i < kThreads; i++) {
      RunningThread* thread = new RunningThread(heap, &counter);
      CHECK(thread->Start());
      threads.push_back(thread);
    }

    for (int i = 0; i < kSafepoints; i++) {
      SafepointScope scope(heap);
      safepoint_count++;
    }

    for (RunningThread* thread : threads) {
      thread->Join();
      delete thread;
    }
  }

  CHECK_EQ(safepoint_count, kRuns * kSafepoints);
}

TEST_F(SafepointTest, SkipLocalHeapOfThisThread) {
  EnsureFlagLocalHeapsEnabled();
  Heap* heap = i_isolate()->heap();
  LocalHeap local_heap(heap, ThreadKind::kMain);
  UnparkedScope unparked_scope(&local_heap);
  {
    SafepointScope scope(heap);
    local_heap.Safepoint();
  }
  {
    ParkedScope parked_scope(&local_heap);
    SafepointScope scope(heap);
    local_heap.Safepoint();
  }
  {
    SafepointScope scope(heap);
    local_heap.Safepoint();
  }
}

}  // namespace internal
}  // namespace v8
