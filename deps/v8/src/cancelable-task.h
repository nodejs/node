// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CANCELABLE_TASK_H_
#define V8_CANCELABLE_TASK_H_

#include <unordered_map>

#include "include/v8-platform.h"
#include "src/base/atomic-utils.h"
#include "src/base/macros.h"
#include "src/base/platform/condition-variable.h"
#include "src/globals.h"

namespace v8 {
namespace internal {

class Cancelable;
class Isolate;


// Keeps track of cancelable tasks. It is possible to register and remove tasks
// from any fore- and background task/thread.
class V8_EXPORT_PRIVATE CancelableTaskManager {
 public:
  using Id = uint64_t;

  CancelableTaskManager();

  // Registers a new cancelable {task}. Returns the unique {id} of the task that
  // can be used to try to abort a task by calling {Abort}.
  // Must not be called after CancelAndWait.
  Id Register(Cancelable* task);

  // Try to abort running a task identified by {id}. The possible outcomes are:
  // (1) The task is already finished running or was canceled before and
  //     thus has been removed from the manager.
  // (2) The task is currently running and cannot be canceled anymore.
  // (3) The task is not yet running (or finished) so it is canceled and
  //     removed.
  //
  enum TryAbortResult { kTaskRemoved, kTaskRunning, kTaskAborted };
  TryAbortResult TryAbort(Id id);

  // Cancels all remaining registered tasks and waits for tasks that are
  // already running. This disallows subsequent Register calls.
  void CancelAndWait();

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

 private:
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
  explicit Cancelable(CancelableTaskManager* parent);
  virtual ~Cancelable();

  // Never invoke after handing over the task to the platform! The reason is
  // that {Cancelable} is used in combination with {v8::Task} and handed to
  // a platform. This step transfers ownership to the platform, which destroys
  // the task after running it. Since the exact time is not known, we cannot
  // access the object after handing it to a platform.
  CancelableTaskManager::Id id() { return id_; }

 protected:
  bool TryRun() { return status_.TrySetValue(kWaiting, kRunning); }
  bool IsRunning() { return status_.Value() == kRunning; }
  intptr_t CancelAttempts() { return cancel_counter_; }

 private:
  // Identifies the state a cancelable task is in:
  // |kWaiting|: The task is scheduled and waiting to be executed. {TryRun} will
  //   succeed.
  // |kCanceled|: The task has been canceled. {TryRun} will fail.
  // |kRunning|: The task is currently running and cannot be canceled anymore.
  enum Status {
    kWaiting,
    kCanceled,
    kRunning,
  };

  // Use {CancelableTaskManager} to abort a task that has not yet been
  // executed.
  bool Cancel() {
    if (status_.TrySetValue(kWaiting, kCanceled)) {
      return true;
    }
    cancel_counter_++;
    return false;
  }

  CancelableTaskManager* parent_;
  base::AtomicValue<Status> status_;
  CancelableTaskManager::Id id_;

  // The counter is incremented for failing tries to cancel a task. This can be
  // used by the task itself as an indication how often external entities tried
  // to abort it.
  std::atomic<intptr_t> cancel_counter_;

  friend class CancelableTaskManager;

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

// TODO(clemensh): Use std::function and move implementation to cc file.
template <typename Func>
class CancelableLambdaTask final : public CancelableTask {
 public:
  CancelableLambdaTask(Isolate* isolate, Func func)
      : CancelableTask(isolate), func_(std::move(func)) {}
  CancelableLambdaTask(CancelableTaskManager* manager, Func func)
      : CancelableTask(manager), func_(std::move(func)) {}
  void RunInternal() final { func_(); }

 private:
  Func func_;
};

template <typename Func>
std::unique_ptr<CancelableTask> MakeCancelableLambdaTask(Isolate* isolate,
                                                         Func func) {
  return std::unique_ptr<CancelableTask>(
      new CancelableLambdaTask<Func>(isolate, std::move(func)));
}
template <typename Func>
std::unique_ptr<CancelableTask> MakeCancelableLambdaTask(
    CancelableTaskManager* manager, Func func) {
  return std::unique_ptr<CancelableTask>(
      new CancelableLambdaTask<Func>(manager, std::move(func)));
}

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

template <typename Func>
class CancelableIdleLambdaTask final : public CancelableIdleTask {
 public:
  CancelableIdleLambdaTask(Isolate* isolate, Func func)
      : CancelableIdleTask(isolate), func_(std::move(func)) {}
  CancelableIdleLambdaTask(CancelableTaskManager* manager, Func func)
      : CancelableIdleTask(manager), func_(std::move(func)) {}
  void RunInternal(double deadline_in_seconds) final {
    func_(deadline_in_seconds);
  }

 private:
  Func func_;
};

template <typename Func>
std::unique_ptr<CancelableIdleTask> MakeCancelableIdleLambdaTask(
    Isolate* isolate, Func func) {
  return std::unique_ptr<CancelableIdleTask>(
      new CancelableIdleLambdaTask<Func>(isolate, std::move(func)));
}
template <typename Func>
std::unique_ptr<CancelableIdleTask> MakeCancelableIdleLambdaTask(
    CancelableTaskManager* manager, Func func) {
  return std::unique_ptr<CancelableIdleTask>(
      new CancelableIdleLambdaTask<Func>(manager, std::move(func)));
}

}  // namespace internal
}  // namespace v8

#endif  // V8_CANCELABLE_TASK_H_
