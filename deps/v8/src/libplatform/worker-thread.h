// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LIBPLATFORM_WORKER_THREAD_H_
#define V8_LIBPLATFORM_WORKER_THREAD_H_

#include <queue>

#include "src/base/macros.h"
#include "src/base/platform/platform.h"

namespace v8 {

namespace platform {

class TaskQueue;

class WorkerThread : public base::Thread {
 public:
  explicit WorkerThread(TaskQueue* queue);
  virtual ~WorkerThread();

  // Thread implementation.
  virtual void Run() OVERRIDE;

 private:
  friend class QuitTask;

  TaskQueue* queue_;

  DISALLOW_COPY_AND_ASSIGN(WorkerThread);
};

} }  // namespace v8::platform


#endif  // V8_LIBPLATFORM_WORKER_THREAD_H_
