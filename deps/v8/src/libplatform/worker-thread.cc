// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/libplatform/worker-thread.h"

#include "include/v8-platform.h"
#include "src/libplatform/task-queue.h"

namespace v8 {
namespace platform {

WorkerThread::WorkerThread(TaskQueue* queue)
    : Thread(Options("V8 WorkerThread")), queue_(queue) {
  Start();
}


WorkerThread::~WorkerThread() {
  Join();
}


void WorkerThread::Run() {
  while (std::unique_ptr<Task> task = queue_->GetNext()) {
    task->Run();
  }
}

}  // namespace platform
}  // namespace v8
