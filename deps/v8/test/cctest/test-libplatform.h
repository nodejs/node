// Copyright 2013 the V8 project authors. All rights reserved.
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

#ifndef TEST_LIBPLATFORM_H_
#define TEST_LIBPLATFORM_H_

#include "src/v8.h"

#include "test/cctest/cctest.h"

using namespace v8::internal;
using namespace v8::platform;

class TaskCounter {
 public:
  TaskCounter() : counter_(0) {}
  ~TaskCounter() { CHECK_EQ(0, counter_); }

  int GetCount() const {
    v8::base::LockGuard<v8::base::Mutex> guard(&lock_);
    return counter_;
  }

  void Inc() {
    v8::base::LockGuard<v8::base::Mutex> guard(&lock_);
    ++counter_;
  }

  void Dec() {
    v8::base::LockGuard<v8::base::Mutex> guard(&lock_);
    --counter_;
  }

 private:
  mutable v8::base::Mutex lock_;
  int counter_;

  DISALLOW_COPY_AND_ASSIGN(TaskCounter);
};


class TestTask : public v8::Task {
 public:
  TestTask(TaskCounter* task_counter, bool expected_to_run)
      : task_counter_(task_counter),
        expected_to_run_(expected_to_run),
        executed_(false) {
    task_counter_->Inc();
  }

  explicit TestTask(TaskCounter* task_counter)
      : task_counter_(task_counter), expected_to_run_(false), executed_(false) {
    task_counter_->Inc();
  }

  virtual ~TestTask() {
    CHECK_EQ(expected_to_run_, executed_);
    task_counter_->Dec();
  }

  // v8::Task implementation.
  virtual void Run() V8_OVERRIDE { executed_ = true; }

 private:
  TaskCounter* task_counter_;
  bool expected_to_run_;
  bool executed_;

  DISALLOW_COPY_AND_ASSIGN(TestTask);
};


class TestWorkerThread : public v8::base::Thread {
 public:
  explicit TestWorkerThread(v8::Task* task)
      : Thread(Options("libplatform TestWorkerThread")),
        semaphore_(0),
        task_(task) {}
  virtual ~TestWorkerThread() {}

  void Signal() { semaphore_.Signal(); }

  // Thread implementation.
  virtual void Run() V8_OVERRIDE {
    semaphore_.Wait();
    if (task_) {
      task_->Run();
      delete task_;
    }
  }

 private:
  v8::base::Semaphore semaphore_;
  v8::Task* task_;

  DISALLOW_COPY_AND_ASSIGN(TestWorkerThread);
};

#endif  // TEST_LIBPLATFORM_H_
