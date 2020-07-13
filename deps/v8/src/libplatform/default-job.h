// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LIBPLATFORM_DEFAULT_JOB_H_
#define V8_LIBPLATFORM_DEFAULT_JOB_H_

#include <atomic>
#include <memory>

#include "include/libplatform/libplatform-export.h"
#include "include/v8-platform.h"
#include "src/base/platform/condition-variable.h"
#include "src/base/platform/mutex.h"

namespace v8 {
namespace platform {

class V8_PLATFORM_EXPORT DefaultJobState
    : NON_EXPORTED_BASE(public JobDelegate),
      public std::enable_shared_from_this<DefaultJobState> {
 public:
  DefaultJobState(Platform* platform, std::unique_ptr<JobTask> job_task,
                  TaskPriority priority, size_t num_worker_threads)
      : platform_(platform),
        job_task_(std::move(job_task)),
        priority_(priority),
        num_worker_threads_(num_worker_threads) {}
  virtual ~DefaultJobState();

  void NotifyConcurrencyIncrease() override;
  bool ShouldYield() override {
    // Thread-safe but may return an outdated result.
    return is_canceled_.load(std::memory_order_relaxed);
  }

  void Join();
  void CancelAndWait();

  // Must be called before running |job_task_| for the first time. If it returns
  // true, then the worker thread must contribute and must call DidRunTask(), or
  // false if it should return.
  bool CanRunFirstTask();
  // Must be called after running |job_task_|. Returns true if the worker thread
  // must contribute again, or false if it should return.
  bool DidRunTask();

 private:
  // Called from the joining thread. Waits for the worker count to be below or
  // equal to max concurrency (will happen when a worker calls
  // DidRunTask()). Returns true if the joining thread should run a task, or
  // false if joining was completed and all other workers returned because
  // there's no work remaining.
  bool WaitForParticipationOpportunityLockRequired();

  // Returns GetMaxConcurrency() capped by the number of threads used by this
  // job.
  size_t CappedMaxConcurrency() const;

  void CallOnWorkerThread(std::unique_ptr<Task> task);

  Platform* const platform_;
  std::unique_ptr<JobTask> job_task_;

  // All members below are protected by |mutex_|.
  base::Mutex mutex_;
  TaskPriority priority_;
  // Number of workers running this job.
  size_t active_workers_ = 0;
  // Number of posted tasks that aren't running this job yet.
  size_t pending_tasks_ = 0;
  // Indicates if the job is canceled.
  std::atomic_bool is_canceled_{false};
  // Number of worker threads available to schedule the worker task.
  size_t num_worker_threads_;
  // Signaled when a worker returns.
  base::ConditionVariable worker_released_condition_;
};

class V8_PLATFORM_EXPORT DefaultJobHandle : public JobHandle {
 public:
  explicit DefaultJobHandle(std::shared_ptr<DefaultJobState> state);
  ~DefaultJobHandle() override;

  void NotifyConcurrencyIncrease() override {
    state_->NotifyConcurrencyIncrease();
  }

  void Join() override;
  void Cancel() override;
  bool IsRunning() override { return state_ != nullptr; }

 private:
  std::shared_ptr<DefaultJobState> state_;

  DISALLOW_COPY_AND_ASSIGN(DefaultJobHandle);
};

class DefaultJobWorker : public Task {
 public:
  DefaultJobWorker(std::weak_ptr<DefaultJobState> state, JobTask* job_task)
      : state_(std::move(state)), job_task_(job_task) {}
  ~DefaultJobWorker() override = default;

  void Run() override {
    auto shared_state = state_.lock();
    if (!shared_state) return;
    if (!shared_state->CanRunFirstTask()) return;
    do {
      job_task_->Run(shared_state.get());
    } while (shared_state->DidRunTask());
  }

 private:
  friend class DefaultJob;

  std::weak_ptr<DefaultJobState> state_;
  JobTask* job_task_;

  DISALLOW_COPY_AND_ASSIGN(DefaultJobWorker);
};

}  // namespace platform
}  // namespace v8

#endif  // V8_LIBPLATFORM_DEFAULT_JOB_H_
