// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LIBPLATFORM_WORKER_THREAD_H_
#define V8_LIBPLATFORM_WORKER_THREAD_H_

#include <queue>

#include "include/libplatform/libplatform-export.h"
#include "src/base/compiler-specific.h"
#include "src/base/macros.h"
#include "src/base/platform/platform.h"

namespace v8 {

namespace platform {

class TaskQueue;

class V8_PLATFORM_EXPORT WorkerThread : public NON_EXPORTED_BASE(base::Thread) {
 public:
  explicit WorkerThread(TaskQueue* queue);
  ~WorkerThread() override;

  // Thread implementation.
  void Run() override;

 private:
  TaskQueue* queue_;

  DISALLOW_COPY_AND_ASSIGN(WorkerThread);
};

}  // namespace platform
}  // namespace v8


#endif  // V8_LIBPLATFORM_WORKER_THREAD_H_
