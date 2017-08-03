// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/cancelable-task.h"

#include "src/base/platform/platform.h"
#include "src/isolate.h"

namespace v8 {
namespace internal {


Cancelable::Cancelable(CancelableTaskManager* parent)
    : parent_(parent), status_(kWaiting), id_(0), cancel_counter_(0) {
  id_ = parent->Register(this);
}


Cancelable::~Cancelable() {
  // The following check is needed to avoid calling an already terminated
  // manager object. This happens when the manager cancels all pending tasks
  // in {CancelAndWait} only before destroying the manager object.
  if (TryRun() || IsRunning()) {
    parent_->RemoveFinishedTask(id_);
  }
}

CancelableTaskManager::CancelableTaskManager()
    : task_id_counter_(0), canceled_(false) {}

CancelableTaskManager::Id CancelableTaskManager::Register(Cancelable* task) {
  base::LockGuard<base::Mutex> guard(&mutex_);
  CancelableTaskManager::Id id = ++task_id_counter_;
  // Id overflows are not supported.
  CHECK_NE(0, id);
  CHECK(!canceled_);
  cancelable_tasks_[id] = task;
  return id;
}

void CancelableTaskManager::RemoveFinishedTask(CancelableTaskManager::Id id) {
  base::LockGuard<base::Mutex> guard(&mutex_);
  size_t removed = cancelable_tasks_.erase(id);
  USE(removed);
  DCHECK_NE(0u, removed);
  cancelable_tasks_barrier_.NotifyOne();
}

CancelableTaskManager::TryAbortResult CancelableTaskManager::TryAbort(
    CancelableTaskManager::Id id) {
  base::LockGuard<base::Mutex> guard(&mutex_);
  auto entry = cancelable_tasks_.find(id);
  if (entry != cancelable_tasks_.end()) {
    Cancelable* value = entry->second;
    if (value->Cancel()) {
      // Cannot call RemoveFinishedTask here because of recursive locking.
      cancelable_tasks_.erase(entry);
      cancelable_tasks_barrier_.NotifyOne();
      return kTaskAborted;
    } else {
      return kTaskRunning;
    }
  }
  return kTaskRemoved;
}


void CancelableTaskManager::CancelAndWait() {
  // Clean up all cancelable fore- and background tasks. Tasks are canceled on
  // the way if possible, i.e., if they have not started yet.  After each round
  // of canceling we wait for the background tasks that have already been
  // started.
  base::LockGuard<base::Mutex> guard(&mutex_);
  canceled_ = true;

  // Cancelable tasks could be running or could potentially register new
  // tasks, requiring a loop here.
  while (!cancelable_tasks_.empty()) {
    for (auto it = cancelable_tasks_.begin(); it != cancelable_tasks_.end();) {
      auto current = it;
      // We need to get to the next element before erasing the current.
      ++it;
      if (current->second->Cancel()) {
        cancelable_tasks_.erase(current);
      }
    }
    // Wait for already running background tasks.
    if (!cancelable_tasks_.empty()) {
      cancelable_tasks_barrier_.Wait(&mutex_);
    }
  }
}

CancelableTaskManager::TryAbortResult CancelableTaskManager::TryAbortAll() {
  // Clean up all cancelable fore- and background tasks. Tasks are canceled on
  // the way if possible, i.e., if they have not started yet.
  base::LockGuard<base::Mutex> guard(&mutex_);

  if (cancelable_tasks_.empty()) return kTaskRemoved;

  for (auto it = cancelable_tasks_.begin(); it != cancelable_tasks_.end();) {
    if (it->second->Cancel()) {
      it = cancelable_tasks_.erase(it);
    } else {
      ++it;
    }
  }

  return cancelable_tasks_.empty() ? kTaskAborted : kTaskRunning;
}

CancelableTask::CancelableTask(Isolate* isolate)
    : CancelableTask(isolate->cancelable_task_manager()) {}

CancelableTask::CancelableTask(CancelableTaskManager* manager)
    : Cancelable(manager) {}

CancelableIdleTask::CancelableIdleTask(Isolate* isolate)
    : CancelableIdleTask(isolate->cancelable_task_manager()) {}

CancelableIdleTask::CancelableIdleTask(CancelableTaskManager* manager)
    : Cancelable(manager) {}

}  // namespace internal
}  // namespace v8
