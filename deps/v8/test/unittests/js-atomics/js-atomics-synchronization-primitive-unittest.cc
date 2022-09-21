// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/platform/platform.h"
#include "src/base/platform/time.h"
#include "src/heap/parked-scope.h"
#include "src/objects/js-atomics-synchronization-inl.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

#if V8_CAN_CREATE_SHARED_HEAP_BOOL

namespace v8 {
namespace internal {

using JSAtomicsMutexTest = TestJSSharedMemoryWithNativeContext;
using JSAtomicsConditionTest = TestJSSharedMemoryWithNativeContext;

namespace {

class IsolateWithContextWrapper final {
 public:
  IsolateWithContextWrapper()
      : isolate_wrapper_(kNoCounters),
        isolate_scope_(isolate_wrapper_.isolate()),
        handle_scope_(isolate_wrapper_.isolate()),
        context_(v8::Context::New(isolate_wrapper_.isolate())),
        context_scope_(context_) {}

  v8::Isolate* v8_isolate() const { return isolate_wrapper_.isolate(); }
  Isolate* isolate() const { return reinterpret_cast<Isolate*>(v8_isolate()); }

 private:
  IsolateWrapper isolate_wrapper_;
  v8::Isolate::Scope isolate_scope_;
  v8::HandleScope handle_scope_;
  v8::Local<v8::Context> context_;
  v8::Context::Scope context_scope_;
};

class LockingThread final : public ParkingThread {
 public:
  LockingThread(Handle<JSAtomicsMutex> mutex, ParkingSemaphore* sema_ready,
                ParkingSemaphore* sema_execute_start,
                ParkingSemaphore* sema_execute_complete)
      : ParkingThread(Options("LockingThread")),
        mutex_(mutex),
        sema_ready_(sema_ready),
        sema_execute_start_(sema_execute_start),
        sema_execute_complete_(sema_execute_complete) {}

  void Run() override {
    IsolateWithContextWrapper isolate_wrapper;
    Isolate* isolate = isolate_wrapper.isolate();

    sema_ready_->Signal();
    sema_execute_start_->ParkedWait(isolate->main_thread_local_isolate());

    HandleScope scope(isolate);
    JSAtomicsMutex::Lock(isolate, mutex_);
    EXPECT_TRUE(mutex_->IsHeld());
    EXPECT_TRUE(mutex_->IsCurrentThreadOwner());
    base::OS::Sleep(base::TimeDelta::FromMilliseconds(1));
    mutex_->Unlock(isolate);

    sema_execute_complete_->Signal();
  }

 private:
  Handle<JSAtomicsMutex> mutex_;
  ParkingSemaphore* sema_ready_;
  ParkingSemaphore* sema_execute_start_;
  ParkingSemaphore* sema_execute_complete_;
};

}  // namespace

TEST_F(JSAtomicsMutexTest, Contention) {
  constexpr int kThreads = 32;

  Isolate* i_main_isolate = i_isolate();
  Handle<JSAtomicsMutex> contended_mutex =
      i_main_isolate->factory()->NewJSAtomicsMutex();
  ParkingSemaphore sema_ready(0);
  ParkingSemaphore sema_execute_start(0);
  ParkingSemaphore sema_execute_complete(0);
  std::vector<std::unique_ptr<LockingThread>> threads;
  for (int i = 0; i < kThreads; i++) {
    auto thread = std::make_unique<LockingThread>(contended_mutex, &sema_ready,
                                                  &sema_execute_start,
                                                  &sema_execute_complete);
    CHECK(thread->Start());
    threads.push_back(std::move(thread));
  }

  LocalIsolate* local_isolate = i_main_isolate->main_thread_local_isolate();
  for (int i = 0; i < kThreads; i++) {
    sema_ready.ParkedWait(local_isolate);
  }
  for (int i = 0; i < kThreads; i++) sema_execute_start.Signal();
  for (int i = 0; i < kThreads; i++) {
    sema_execute_complete.ParkedWait(local_isolate);
  }

  ParkedScope parked(local_isolate);
  for (auto& thread : threads) {
    thread->ParkedJoin(parked);
  }

  EXPECT_FALSE(contended_mutex->IsHeld());
}

namespace {
class WaitOnConditionThread final : public ParkingThread {
 public:
  WaitOnConditionThread(Handle<JSAtomicsMutex> mutex,
                        Handle<JSAtomicsCondition> condition,
                        uint32_t* waiting_threads_count,
                        ParkingSemaphore* sema_ready,
                        ParkingSemaphore* sema_execute_complete)
      : ParkingThread(Options("WaitOnConditionThread")),
        mutex_(mutex),
        condition_(condition),
        waiting_threads_count_(waiting_threads_count),
        sema_ready_(sema_ready),
        sema_execute_complete_(sema_execute_complete) {}

  void Run() override {
    IsolateWithContextWrapper isolate_wrapper;
    Isolate* isolate = isolate_wrapper.isolate();

    sema_ready_->Signal();

    HandleScope scope(isolate);
    JSAtomicsMutex::Lock(isolate, mutex_);
    while (keep_waiting) {
      (*waiting_threads_count_)++;
      EXPECT_TRUE(JSAtomicsCondition::WaitFor(isolate, condition_, mutex_,
                                              base::nullopt));
      (*waiting_threads_count_)--;
    }
    mutex_->Unlock(isolate);

    sema_execute_complete_->Signal();
  }

  bool keep_waiting = true;

 private:
  Handle<JSAtomicsMutex> mutex_;
  Handle<JSAtomicsCondition> condition_;
  uint32_t* waiting_threads_count_;
  ParkingSemaphore* sema_ready_;
  ParkingSemaphore* sema_execute_complete_;
};
}  // namespace

TEST_F(JSAtomicsConditionTest, NotifyAll) {
  constexpr uint32_t kThreads = 32;

  Isolate* i_main_isolate = i_isolate();
  Handle<JSAtomicsMutex> mutex = i_main_isolate->factory()->NewJSAtomicsMutex();
  Handle<JSAtomicsCondition> condition =
      i_main_isolate->factory()->NewJSAtomicsCondition();

  uint32_t waiting_threads_count = 0;
  ParkingSemaphore sema_ready(0);
  ParkingSemaphore sema_execute_complete(0);
  std::vector<std::unique_ptr<WaitOnConditionThread>> threads;
  for (uint32_t i = 0; i < kThreads; i++) {
    auto thread = std::make_unique<WaitOnConditionThread>(
        mutex, condition, &waiting_threads_count, &sema_ready,
        &sema_execute_complete);
    CHECK(thread->Start());
    threads.push_back(std::move(thread));
  }

  LocalIsolate* local_isolate = i_main_isolate->main_thread_local_isolate();
  for (uint32_t i = 0; i < kThreads; i++) {
    sema_ready.ParkedWait(local_isolate);
  }

  // Wait until all threads are waiting on the condition.
  for (;;) {
    JSAtomicsMutex::LockGuard lock_guard(i_main_isolate, mutex);
    uint32_t count = waiting_threads_count;
    if (count == kThreads) break;
  }

  // Wake all the threads up.
  for (uint32_t i = 0; i < kThreads; i++) {
    threads[i]->keep_waiting = false;
  }
  EXPECT_EQ(kThreads,
            condition->Notify(i_main_isolate, JSAtomicsCondition::kAllWaiters));

  for (uint32_t i = 0; i < kThreads; i++) {
    sema_execute_complete.ParkedWait(local_isolate);
  }

  ParkedScope parked(local_isolate);
  for (auto& thread : threads) {
    thread->ParkedJoin(parked);
  }

  EXPECT_EQ(0U, waiting_threads_count);
  EXPECT_FALSE(mutex->IsHeld());
}

}  // namespace internal
}  // namespace v8

#endif  // V8_CAN_CREATE_SHARED_HEAP
