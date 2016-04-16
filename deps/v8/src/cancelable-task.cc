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
  CHECK(id_ != 0);
}


Cancelable::~Cancelable() {
  // The following check is needed to avoid calling an already terminated
  // manager object. This happens when the manager cancels all pending tasks
  // in {CancelAndWait} only before destroying the manager object.
  if (TryRun() || IsRunning()) {
    parent_->RemoveFinishedTask(id_);
  }
}


static bool ComparePointers(void* ptr1, void* ptr2) { return ptr1 == ptr2; }


CancelableTaskManager::CancelableTaskManager()
    : task_id_counter_(0), cancelable_tasks_(ComparePointers) {}


uint32_t CancelableTaskManager::Register(Cancelable* task) {
  base::LockGuard<base::Mutex> guard(&mutex_);
  uint32_t id = ++task_id_counter_;
  // The loop below is just used when task_id_counter_ overflows.
  while ((id == 0) || (cancelable_tasks_.Lookup(reinterpret_cast<void*>(id),
                                                id) != nullptr)) {
    ++id;
  }
  HashMap::Entry* entry =
      cancelable_tasks_.LookupOrInsert(reinterpret_cast<void*>(id), id);
  entry->value = task;
  return id;
}


void CancelableTaskManager::RemoveFinishedTask(uint32_t id) {
  base::LockGuard<base::Mutex> guard(&mutex_);
  void* removed = cancelable_tasks_.Remove(reinterpret_cast<void*>(id), id);
  USE(removed);
  DCHECK(removed != nullptr);
  cancelable_tasks_barrier_.NotifyOne();
}


bool CancelableTaskManager::TryAbort(uint32_t id) {
  base::LockGuard<base::Mutex> guard(&mutex_);
  HashMap::Entry* entry =
      cancelable_tasks_.Lookup(reinterpret_cast<void*>(id), id);
  if (entry != nullptr) {
    Cancelable* value = reinterpret_cast<Cancelable*>(entry->value);
    if (value->Cancel()) {
      // Cannot call RemoveFinishedTask here because of recursive locking.
      void* removed = cancelable_tasks_.Remove(reinterpret_cast<void*>(id), id);
      USE(removed);
      DCHECK(removed != nullptr);
      cancelable_tasks_barrier_.NotifyOne();
      return true;
    }
  }
  return false;
}


void CancelableTaskManager::CancelAndWait() {
  // Clean up all cancelable fore- and background tasks. Tasks are canceled on
  // the way if possible, i.e., if they have not started yet.  After each round
  // of canceling we wait for the background tasks that have already been
  // started.
  base::LockGuard<base::Mutex> guard(&mutex_);

  // HashMap does not support removing while iterating, hence keep a set of
  // entries that are to be removed.
  std::set<uint32_t> to_remove;

  // Cancelable tasks could potentially register new tasks, requiring a loop
  // here.
  while (cancelable_tasks_.occupancy() > 0) {
    for (HashMap::Entry* p = cancelable_tasks_.Start(); p != nullptr;
         p = cancelable_tasks_.Next(p)) {
      if (reinterpret_cast<Cancelable*>(p->value)->Cancel()) {
        to_remove.insert(reinterpret_cast<Cancelable*>(p->value)->id());
      }
    }
    // Remove tasks that were successfully canceled.
    for (auto id : to_remove) {
      cancelable_tasks_.Remove(reinterpret_cast<void*>(id), id);
    }
    to_remove.clear();

    // Finally, wait for already running background tasks.
    if (cancelable_tasks_.occupancy() > 0) {
      cancelable_tasks_barrier_.Wait(&mutex_);
    }
  }
}


CancelableTask::CancelableTask(Isolate* isolate)
    : Cancelable(isolate->cancelable_task_manager()), isolate_(isolate) {}


CancelableIdleTask::CancelableIdleTask(Isolate* isolate)
    : Cancelable(isolate->cancelable_task_manager()), isolate_(isolate) {}

}  // namespace internal
}  // namespace v8
