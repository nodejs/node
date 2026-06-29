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
    : public std::enable_shared_from_this<DefaultJobState> {
 public:
  class JobDelegate : public v8::JobDelegate {
   public:
    explicit JobDelegate(DefaultJobState* outer, bool is_joining_thread = false)
        : outer_(outer), is_joining_thread_(is_joining_thread) {}
    ~JobDelegate();

    void NotifyConcurrencyIncrease() override {
      outer_->NotifyConcurrencyIncrease();
    }
    bool ShouldYield() override {
      // After {ShouldYield} returned true, the job is expected to return and
      // not call {ShouldYield} again. This resembles a similar DCHECK in the
      // gin platform.
      DCHECK(!was_told_to_yield_);
      // Thread-safe but may return an outdated result.
      was_told_to_yield_ |=
          outer_->is_canceled_.load(std::memory_order_relaxed);
      return was_told_to_yield_;
    }
    uint8_t GetTaskId() override;
    bool IsJoiningThread() const override { return is_joining_thread_; }

   private:
    static constexpr uint8_t kInvalidTaskId =
        std::numeric_limits<uint8_t>::max();

    DefaultJobState* outer_;
    uint8_t task_id_ = kInvalidTaskId;
    bool is_joining_thread_;
    bool was_told_to_yield_ = false;
  };

  DefaultJobState(Platform* platform, std::unique_ptr<JobTask> job_task,
                  TaskPriority priority, size_t num_worker_threads);
  virtual ~DefaultJobState();

  void NotifyConcurrencyIncrease();
  uint8_t AcquireTaskId();
  void ReleaseTaskId(uint8_t task_id);

  void Join();
  void CancelAndWait();
  void CancelAndDetach();
  bool IsActive();

  // Must be called before running |job_task_| for the first time. If it returns
  // true, then the worker thread must contribute and must call DidRunTask(), or
  // false if it should return.
  bool CanRunFirstTask();
  // Must be called after running |job_task_|. Returns true if the worker thread
  // must contribute again, or false if it should return.
  bool DidRunTask();

  void UpdatePriority(TaskPriority);

 private:
  // Returns GetMaxConcurrency() capped by the number of threads used by this
  // job.
  size_t CappedMaxConcurrency(size_t worker_count) const;

  void CallOnWorkerThread(TaskPriority priority, std::unique_ptr<Task> task);

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

  std::atomic<uint32_t> assigned_task_ids_{0};
};

class V8_PLATFORM_EXPORT DefaultJobHandle : public JobHandle {
 public:
  explicit DefaultJobHandle(std::shared_ptr<DefaultJobState> state);
  ~DefaultJobHandle() override;

  DefaultJobHandle(const DefaultJobHandle&) = delete;
  DefaultJobHandle& operator=(const DefaultJobHandle&) = delete;

  void NotifyConcurrencyIncrease() override {
    state_->NotifyConcurrencyIncrease();
  }

  void Join() override;
  void Cancel() override;
  void CancelAndDetach() override;
  bool IsActive() override;
  bool IsValid() override { return state_ != nullptr; }

  bool UpdatePriorityEnabled() const override { return true; }

  void UpdatePriority(TaskPriority) override;

 private:
  std::shared_ptr<DefaultJobState> state_;
};

class DefaultJobWorker : public Task {
 public:
  DefaultJobWorker(std::weak_ptr<DefaultJobState> state, JobTask* job_task)
      : state_(std::move(state)), job_task_(job_task) {}
  ~DefaultJobWorker() override = default;

  DefaultJobWorker(const DefaultJobWorker&) = delete;
  DefaultJobWorker& operator=(const DefaultJobWorker&) = delete;

  void Run() override {
    auto shared_state = state_.lock();
    if (!shared_state) return;
    if (!shared_state->CanRunFirstTask()) return;
    do {
      // Scope of |delegate| must not outlive DidRunTask() so that associated
      // state is freed before the worker becomes inactive.
      DefaultJobState::JobDelegate delegate(shared_state.get());
      job_task_->Run(&delegate);
    } while (shared_state->DidRunTask());
  }

 private:
  friend class DefaultJob;

  std::weak_ptr<DefaultJobState> state_;
  JobTask* job_task_;
};

}  // namespace platform
}  // namespace v8

#endif  // V8_LIBPLATFORM_DEFAULT_JOB_H_
