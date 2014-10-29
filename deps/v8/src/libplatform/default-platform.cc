// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/libplatform/default-platform.h"

#include <algorithm>
#include <queue>

#include "src/base/logging.h"
#include "src/base/platform/platform.h"
#include "src/base/sys-info.h"
#include "src/libplatform/worker-thread.h"

namespace v8 {
namespace platform {


v8::Platform* CreateDefaultPlatform(int thread_pool_size) {
  DefaultPlatform* platform = new DefaultPlatform();
  platform->SetThreadPoolSize(thread_pool_size);
  platform->EnsureInitialized();
  return platform;
}


bool PumpMessageLoop(v8::Platform* platform, v8::Isolate* isolate) {
  return reinterpret_cast<DefaultPlatform*>(platform)->PumpMessageLoop(isolate);
}


const int DefaultPlatform::kMaxThreadPoolSize = 4;


DefaultPlatform::DefaultPlatform()
    : initialized_(false), thread_pool_size_(0) {}


DefaultPlatform::~DefaultPlatform() {
  base::LockGuard<base::Mutex> guard(&lock_);
  queue_.Terminate();
  if (initialized_) {
    for (std::vector<WorkerThread*>::iterator i = thread_pool_.begin();
         i != thread_pool_.end(); ++i) {
      delete *i;
    }
  }
  for (std::map<v8::Isolate*, std::queue<Task*> >::iterator i =
           main_thread_queue_.begin();
       i != main_thread_queue_.end(); ++i) {
    while (!i->second.empty()) {
      delete i->second.front();
      i->second.pop();
    }
  }
}


void DefaultPlatform::SetThreadPoolSize(int thread_pool_size) {
  base::LockGuard<base::Mutex> guard(&lock_);
  DCHECK(thread_pool_size >= 0);
  if (thread_pool_size < 1) {
    thread_pool_size = base::SysInfo::NumberOfProcessors();
  }
  thread_pool_size_ =
      std::max(std::min(thread_pool_size, kMaxThreadPoolSize), 1);
}


void DefaultPlatform::EnsureInitialized() {
  base::LockGuard<base::Mutex> guard(&lock_);
  if (initialized_) return;
  initialized_ = true;

  for (int i = 0; i < thread_pool_size_; ++i)
    thread_pool_.push_back(new WorkerThread(&queue_));
}


bool DefaultPlatform::PumpMessageLoop(v8::Isolate* isolate) {
  Task* task = NULL;
  {
    base::LockGuard<base::Mutex> guard(&lock_);
    std::map<v8::Isolate*, std::queue<Task*> >::iterator it =
        main_thread_queue_.find(isolate);
    if (it == main_thread_queue_.end() || it->second.empty()) {
      return false;
    }
    task = it->second.front();
    it->second.pop();
  }
  task->Run();
  delete task;
  return true;
}

void DefaultPlatform::CallOnBackgroundThread(Task *task,
                                             ExpectedRuntime expected_runtime) {
  EnsureInitialized();
  queue_.Append(task);
}


void DefaultPlatform::CallOnForegroundThread(v8::Isolate* isolate, Task* task) {
  base::LockGuard<base::Mutex> guard(&lock_);
  main_thread_queue_[isolate].push(task);
}

} }  // namespace v8::platform
