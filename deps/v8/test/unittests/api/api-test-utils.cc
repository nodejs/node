// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/unittests/api/api-test-utils.h"

#include <cstdio>

#include "include/v8-locker.h"
#include "src/base/platform/platform.h"
#include "src/flags/flags.h"
#include "src/heap/heap.h"

namespace v8 {

namespace {
constexpr bool kLogThreading = false;
}  // namespace

std::vector<const RegisterThreadedTest*> RegisterThreadedTest::tests_;

v8::Isolate* ApiTestFuzzer::isolate_ = nullptr;
unsigned ApiTestFuzzer::linear_congruential_generator;
std::vector<std::unique_ptr<ApiTestFuzzer>> ApiTestFuzzer::fuzzers_;
bool ApiTestFuzzer::fuzzing_ = false;
v8::base::Semaphore ApiTestFuzzer::all_tests_done_(0);
int ApiTestFuzzer::tests_being_run_;
int ApiTestFuzzer::active_tests_;
int ApiTestFuzzer::current_fuzzer_;

// We are in a callback and want to switch to another thread (if we
// are currently running the thread fuzzing test).
void ApiTestFuzzer::Fuzz() {
  // Emulate context switch which might cause side effects as well.
  // This is mostly to ensure that the callbacks in the tests do not cause
  // side effects when they don't intercept the operation.
  i::Isolate::Current()->IncrementJavascriptExecutionCounter();

  if (!fuzzing_) return;
  fuzzers_[current_fuzzer_]->ContextSwitch();
}

// Let the next thread go.  Since it is also waiting on the V8 lock it may
// not start immediately.
bool ApiTestFuzzer::NextThread() {
  int next_fuzzer = GetNextFuzzer();
  if (next_fuzzer == current_fuzzer_) {
    if (kLogThreading) {
      int current_number = fuzzers_[current_fuzzer_]->test_number_;
      printf("Stay with %s #%d\n",
             RegisterThreadedTest::nth(current_number)->name(), current_number);
    }
    return false;
  }
  if (kLogThreading) {
    int current_number =
        current_fuzzer_ >= 0 ? fuzzers_[current_fuzzer_]->test_number_ : -1;
    int next_number = fuzzers_[next_fuzzer]->test_number_;
    printf("Switch from %s #%d to %s #%d\n",
           current_number >= 0
               ? RegisterThreadedTest::nth(current_number)->name()
               : "<none>",
           current_number, RegisterThreadedTest::nth(next_number)->name(),
           next_number);
  }
  current_fuzzer_ = next_fuzzer;
  fuzzers_[current_fuzzer_]->gate_.Signal();
  return true;
}

void ApiTestFuzzer::Run() {
  // Wait until it is our turn.
  gate_.Wait();
  {
    // Get the V8 lock.
    v8::Locker locker(isolate_);
    // Start running the test, which will enter the isolate and exit it when it
    // finishes.
    CallTest();
  }
  // This test finished.
  active_ = false;
  active_tests_--;
  // If it was the last then signal that fact.
  if (active_tests_ == 0) {
    all_tests_done_.Signal();
  } else {
    // Otherwise select a new test and start that.
    NextThread();
  }
}

void ApiTestFuzzer::SetUp(PartOfTest part) {
  isolate_ = v8::Isolate::GetCurrent();
  CHECK_NOT_NULL(isolate_);
  linear_congruential_generator = i::v8_flags.testing_prng_seed;
  fuzzing_ = true;
  int count = RegisterThreadedTest::count();
  int start = count * part / (LAST_PART + 1);
  int end = (count * (part + 1) / (LAST_PART + 1)) - 1;
  active_tests_ = tests_being_run_ = end - start + 1;
  fuzzers_.clear();
  for (int i = 0; i < tests_being_run_; i++) {
    fuzzers_.push_back(
        std::unique_ptr<ApiTestFuzzer>(new ApiTestFuzzer(i + start)));
  }
  for (const auto& fuzzer : fuzzers_) {
    CHECK(fuzzer->Start());
  }
}

void ApiTestFuzzer::RunAllTests() {
  // This method is called on the main thread, which has entered the isolate
  // at this point. We need to exit the isolate, so that the fuzzer threads can
  // enter it in turn, while running their tests.
  isolate_->Exit();
  // Set off the first test.
  current_fuzzer_ = -1;
  NextThread();
  // Wait till they are all done.
  all_tests_done_.Wait();
  // We enter the isolate again, to prepare for teardown.
  isolate_->Enter();
}

int ApiTestFuzzer::GetNextFuzzer() {
  int next;
  do {
    next = (linear_congruential_generator >> 16) % tests_being_run_;
    linear_congruential_generator *= 1664525u;
    linear_congruential_generator += 1013904223u;
  } while (!fuzzers_[next]->active_);
  return next;
}

void ApiTestFuzzer::ContextSwitch() {
  // If the new thread is the same as the current thread there is nothing to do.
  if (!NextThread()) return;
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate_);
  // Mark the stack of this background thread for conservative stack scanning.
  i_isolate->heap()->stack().SetMarkerForBackgroundThreadAndCallback(
      v8::base::ThreadId::Current().ToInteger(), [this, i_isolate]() {
        // Exit the isolate from this thread.
        i_isolate->Exit();
        {
          // Now the new thread can start.
          v8::Unlocker unlocker(isolate_);
          // Wait till someone starts us again.
          gate_.Wait();
        }
        // Enter the isolate from this thread again.
        i_isolate->Enter();
        // And we're off.
      });
}

void ApiTestFuzzer::TearDown() {
  fuzzing_ = false;
  for (const auto& fuzzer : fuzzers_) {
    if (fuzzer) fuzzer->Join();
  }
}

void ApiTestFuzzer::CallTest() {
  v8::Isolate::Scope scope(isolate_);
  if (kLogThreading) {
    printf("Start test %s #%d\n",
           RegisterThreadedTest::nth(test_number_)->name(), test_number_);
  }
  (RegisterThreadedTest::nth(test_number_)->callback())();
  if (kLogThreading) {
    printf("End test %s #%d\n", RegisterThreadedTest::nth(test_number_)->name(),
           test_number_);
  }
}

}  // namespace v8
