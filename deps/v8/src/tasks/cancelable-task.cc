// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/tasks/cancelable-task.h"

#include "src/base/platform/platform.h"
#include "src/execution/isolate.h"

namespace v8 {
namespace internal {

Cancelable::~Cancelable() {
  // The following check is needed to avoid calling an already terminated
  // manager object. This happens when the manager cancels all pending tasks
  // in {CancelAndWait} only before destroying the manager object.
  Status previous;
  if (TryRun(&previous) || previous == kRunning) {
    parent_->RemoveFinishedTask(id_);
  }
}

CancelableTaskManager::CancelableTaskManager()
    : task_id_counter_(kInvalidTaskId), canceled_(false) {}

CancelableTaskManager::~CancelableTaskManager() {
  // It is required that {CancelAndWait} is called before the manager object is
  // destroyed. This guarantees that all tasks managed by this
  // {CancelableTaskManager} are either canceled or finished their execution
  // when the {CancelableTaskManager} dies.
  CHECK(canceled_);
}

CancelableTaskManager::Id CancelableTaskManager::Register(Cancelable* task) {
  base::MutexGuard guard(&mutex_);
  if (canceled_) {
    // The CancelableTaskManager has already been canceled. Therefore we mark
    // the new task immediately as canceled so that it does not get executed.
    task->Cancel();
    return kInvalidTaskId;
  }
  CancelableTaskManager::Id id = ++task_id_counter_;
  // Id overflows are not supported.
  CHECK_NE(kInvalidTaskId, id);
  CHECK(!canceled_);
  cancelable_tasks_[id] = task;
  return id;
}

void CancelableTaskManager::RemoveFinishedTask(CancelableTaskManager::Id id) {
  CHECK_NE(kInvalidTaskId, id);
  base::MutexGuard guard(&mutex_);
  size_t removed = cancelable_tasks_.erase(id);
  USE(removed);
  DCHECK_NE(0u, removed);
  cancelable_tasks_barrier_.NotifyOne();
}

TryAbortResult CancelableTaskManager::TryAbort(CancelableTaskManager::Id id) {
  CHECK_NE(kInvalidTaskId, id);
  base::MutexGuard guard(&mutex_);
  auto entry = cancelable_tasks_.find(id);
  if (entry != cancelable_tasks_.end()) {
    Cancelable* value = entry->second;
    if (value->Cancel()) {
      // Cannot call RemoveFinishedTask here because of recursive locking.
      cancelable_tasks_.erase(entry);
      cancelable_tasks_barrier_.NotifyOne();
      return TryAbortResult::kTaskAborted;
    } else {
      return TryAbortResult::kTaskRunning;
    }
  }
  return TryAbortResult::kTaskRemoved;
}

void CancelableTaskManager::CancelAndWait() {
  // Clean up all cancelable fore- and background tasks. Tasks are canceled on
  // the way if possible, i.e., if they have not started yet.  After each round
  // of canceling we wait for the background tasks that have already been
  // started.
  base::MutexGuard guard(&mutex_);
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

TryAbortResult CancelableTaskManager::TryAbortAll() {
  // Clean up all cancelable fore- and background tasks. Tasks are canceled on
  // the way if possible, i.e., if they have not started yet.
  base::MutexGuard guard(&mutex_);

  if (cancelable_tasks_.empty()) return TryAbortResult::kTaskRemoved;

  for (auto it = cancelable_tasks_.begin(); it != cancelable_tasks_.end();) {
    if (it->second->Cancel()) {
      it = cancelable_tasks_.erase(it);
    } else {
      ++it;
    }
  }

  return cancelable_tasks_.empty() ? TryAbortResult::kTaskAborted
                                   : TryAbortResult::kTaskRunning;
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
