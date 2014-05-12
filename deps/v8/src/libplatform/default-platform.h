// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LIBPLATFORM_DEFAULT_PLATFORM_H_
#define V8_LIBPLATFORM_DEFAULT_PLATFORM_H_

#include <vector>

#include "../../include/v8-platform.h"
#include "../base/macros.h"
#include "../platform/mutex.h"
#include "task-queue.h"

namespace v8 {
namespace internal {

class TaskQueue;
class Thread;
class WorkerThread;

class DefaultPlatform : public Platform {
 public:
  DefaultPlatform();
  virtual ~DefaultPlatform();

  void SetThreadPoolSize(int thread_pool_size);

  void EnsureInitialized();

  // v8::Platform implementation.
  virtual void CallOnBackgroundThread(
      Task *task, ExpectedRuntime expected_runtime) V8_OVERRIDE;
  virtual void CallOnForegroundThread(v8::Isolate *isolate,
                                      Task *task) V8_OVERRIDE;

 private:
  static const int kMaxThreadPoolSize;

  Mutex lock_;
  bool initialized_;
  int thread_pool_size_;
  std::vector<WorkerThread*> thread_pool_;
  TaskQueue queue_;

  DISALLOW_COPY_AND_ASSIGN(DefaultPlatform);
};


} }  // namespace v8::internal


#endif  // V8_LIBPLATFORM_DEFAULT_PLATFORM_H_
