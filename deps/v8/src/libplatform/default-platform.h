// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LIBPLATFORM_DEFAULT_PLATFORM_H_
#define V8_LIBPLATFORM_DEFAULT_PLATFORM_H_

#include <map>
#include <queue>
#include <vector>

#include "include/v8-platform.h"
#include "src/base/macros.h"
#include "src/base/platform/mutex.h"
#include "src/libplatform/task-queue.h"

namespace v8 {
namespace platform {

class TaskQueue;
class Thread;
class WorkerThread;

class DefaultPlatform : public Platform {
 public:
  DefaultPlatform();
  virtual ~DefaultPlatform();

  void SetThreadPoolSize(int thread_pool_size);

  void EnsureInitialized();

  bool PumpMessageLoop(v8::Isolate* isolate);

  // v8::Platform implementation.
  virtual void CallOnBackgroundThread(
      Task* task, ExpectedRuntime expected_runtime) override;
  virtual void CallOnForegroundThread(v8::Isolate* isolate,
                                      Task* task) override;
  double MonotonicallyIncreasingTime() override;

 private:
  static const int kMaxThreadPoolSize;

  base::Mutex lock_;
  bool initialized_;
  int thread_pool_size_;
  std::vector<WorkerThread*> thread_pool_;
  TaskQueue queue_;
  std::map<v8::Isolate*, std::queue<Task*> > main_thread_queue_;

  DISALLOW_COPY_AND_ASSIGN(DefaultPlatform);
};


} }  // namespace v8::platform


#endif  // V8_LIBPLATFORM_DEFAULT_PLATFORM_H_
