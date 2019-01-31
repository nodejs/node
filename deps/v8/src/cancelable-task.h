// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CANCELABLE_TASK_H_
#define V8_CANCELABLE_TASK_H_

#include <atomic>
#include <unordered_map>

#include "include/v8-platform.h"
#include "src/base/macros.h"
#include "src/base/platform/condition-variable.h"
#include "src/globals.h"

namespace v8 {
namespace internal {

class Cancelable;
class Isolate;

// The possible outcomes of trying to abort a job are:
// (1) The task is already finished running or was canceled before and
//     thus has been removed from the manager.
// (2) The task is currently running and cannot be canceled anymore.
// (3) The task is not yet running (or finished) so it is canceled and
//     removed.
enum class TryAbortResult { kTaskRemoved, kTaskRunning, kTaskAborted };

// Keeps track of cancelable tasks. It is possible to register and remove tasks
// from any fore- and background task/thread.
class V8_EXPORT_PRIVATE CancelableTaskManager {
 public:
  using Id = uint64_t;

  CancelableTaskManager();

  ~CancelableTaskManager();

  // Registers a new cancelable {task}. Returns the unique {id} of the task that
  // can be used to try to abort a task by calling {Abort}.
  // If {Register} is called after {CancelAndWait}, then the task will be
  // aborted immediately.
  // {Register} should only be called by the thread which owns the
  // {CancelableTaskManager}, or by a task which is managed by the
  // {CancelableTaskManager}.
  Id Register(Cancelable* task);

  // Try to abort running a task identified by {id}.
  TryAbortResult TryAbort(Id id);

  // Tries to cancel all remaining registered tasks. The return value indicates
  // whether
  //
  // 1) No tasks were registered (kTaskRemoved), or
  //
  // 2) There is at least one remaining task that couldn't be cancelled
  // (kTaskRunning), or
  //
  // 3) All registered tasks were cancelled (kTaskAborted).
  TryAbortResult TryAbortAll();

  // Cancels all remaining registered tasks and waits for tasks that are
  // already running. This disallows subsequent Register calls.
  void CancelAndWait();

  // Returns true of the task manager has been cancelled.
  bool canceled() const { return canceled_; }

 private:
  static constexpr Id kInvalidTaskId = 0;

  // Only called by {Cancelable} destructor. The task is done with executing,
  // but needs to be removed.
  void RemoveFinishedTask(Id id);

  // To mitigate the ABA problem, the api refers to tasks through an id.
  Id task_id_counter_;

  // A set of cancelable tasks that are currently registered.
  std::unordered_map<Id, Cancelable*> cancelable_tasks_;

  // Mutex and condition variable enabling concurrent register and removing, as
  // well as waiting for background tasks on {CancelAndWait}.
  base::ConditionVariable cancelable_tasks_barrier_;
  base::Mutex mutex_;

  bool canceled_;

  friend class Cancelable;

  DISALLOW_COPY_AND_ASSIGN(CancelableTaskManager);
};

class V8_EXPORT_PRIVATE Cancelable {
 public:
  explicit Cancelable(CancelableTaskManager* parent)
      : parent_(parent), id_(parent->Register(this)) {}

  virtual ~Cancelable();

  // Never invoke after handing over the task to the platform! The reason is
  // that {Cancelable} is used in combination with {v8::Task} and handed to
  // a platform. This step transfers ownership to the platform, which destroys
  // the task after running it. Since the exact time is not known, we cannot
  // access the object after handing it to a platform.
  CancelableTaskManager::Id id() { return id_; }

 protected:
  // Identifies the state a cancelable task is in:
  // |kWaiting|: The task is scheduled and waiting to be executed. {TryRun} will
  //   succeed.
  // |kCanceled|: The task has been canceled. {TryRun} will fail.
  // |kRunning|: The task is currently running and cannot be canceled anymore.
  enum Status { kWaiting, kCanceled, kRunning };

  bool TryRun(Status* previous = nullptr) {
    return CompareExchangeStatus(kWaiting, kRunning, previous);
  }

 private:
  friend class CancelableTaskManager;

  // Use {CancelableTaskManager} to abort a task that has not yet been
  // executed.
  bool Cancel() { return CompareExchangeStatus(kWaiting, kCanceled); }

  bool CompareExchangeStatus(Status expected, Status desired,
                             Status* previous = nullptr) {
    // {compare_exchange_strong} updates {expected}.
    bool success = status_.compare_exchange_strong(expected, desired,
                                                   std::memory_order_acq_rel,
                                                   std::memory_order_acquire);
    if (previous) *previous = expected;
    return success;
  }

  CancelableTaskManager* const parent_;
  std::atomic<Status> status_{kWaiting};
  const CancelableTaskManager::Id id_;

  DISALLOW_COPY_AND_ASSIGN(Cancelable);
};

// Multiple inheritance can be used because Task is a pure interface.
class V8_EXPORT_PRIVATE CancelableTask : public Cancelable,
                                         NON_EXPORTED_BASE(public Task) {
 public:
  explicit CancelableTask(Isolate* isolate);
  explicit CancelableTask(CancelableTaskManager* manager);

  // Task overrides.
  void Run() final {
    if (TryRun()) {
      RunInternal();
    }
  }

  virtual void RunInternal() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(CancelableTask);
};

// Multiple inheritance can be used because IdleTask is a pure interface.
class CancelableIdleTask : public Cancelable, public IdleTask {
 public:
  explicit CancelableIdleTask(Isolate* isolate);
  explicit CancelableIdleTask(CancelableTaskManager* manager);

  // IdleTask overrides.
  void Run(double deadline_in_seconds) final {
    if (TryRun()) {
      RunInternal(deadline_in_seconds);
    }
  }

  virtual void RunInternal(double deadline_in_seconds) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(CancelableIdleTask);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_CANCELABLE_TASK_H_
