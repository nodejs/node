// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/libplatform/default-job.h"

namespace v8 {
namespace platform {

DefaultJobState::~DefaultJobState() { DCHECK_EQ(0U, active_workers_); }

void DefaultJobState::NotifyConcurrencyIncrease() {
  if (is_canceled_.load(std::memory_order_relaxed)) return;

  size_t num_tasks_to_post = 0;
  {
    base::MutexGuard guard(&mutex_);
    const size_t max_concurrency = CappedMaxConcurrency();
    // Consider |pending_tasks_| to avoid posting too many tasks.
    if (max_concurrency > (active_workers_ + pending_tasks_)) {
      num_tasks_to_post = max_concurrency - active_workers_ - pending_tasks_;
      pending_tasks_ += num_tasks_to_post;
    }
  }
  // Post additional worker tasks to reach |max_concurrency|.
  for (size_t i = 0; i < num_tasks_to_post; ++i) {
    CallOnWorkerThread(std::make_unique<DefaultJobWorker>(shared_from_this(),
                                                          job_task_.get()));
  }
}

void DefaultJobState::Join() {
  bool can_run = false;
  {
    base::MutexGuard guard(&mutex_);
    priority_ = TaskPriority::kUserBlocking;
    // Reserve a worker for the joining thread. GetMaxConcurrency() is ignored
    // here, but WaitForParticipationOpportunityLockRequired() waits for
    // workers to return if necessary so we don't exceed GetMaxConcurrency().
    num_worker_threads_ = platform_->NumberOfWorkerThreads() + 1;
    ++active_workers_;
    can_run = WaitForParticipationOpportunityLockRequired();
  }
  while (can_run) {
    job_task_->Run(this);
    base::MutexGuard guard(&mutex_);
    can_run = WaitForParticipationOpportunityLockRequired();
  }
}

void DefaultJobState::CancelAndWait() {
  {
    base::MutexGuard guard(&mutex_);
    is_canceled_.store(true, std::memory_order_relaxed);
    while (active_workers_ > 0) {
      worker_released_condition_.Wait(&mutex_);
    }
  }
}

bool DefaultJobState::CanRunFirstTask() {
  base::MutexGuard guard(&mutex_);
  --pending_tasks_;
  if (is_canceled_.load(std::memory_order_relaxed)) return false;
  if (active_workers_ >=
      std::min(job_task_->GetMaxConcurrency(), num_worker_threads_)) {
    return false;
  }
  // Acquire current worker.
  ++active_workers_;
  return true;
}

bool DefaultJobState::DidRunTask() {
  size_t num_tasks_to_post = 0;
  {
    base::MutexGuard guard(&mutex_);
    const size_t max_concurrency = CappedMaxConcurrency();
    if (is_canceled_.load(std::memory_order_relaxed) ||
        active_workers_ > max_concurrency) {
      // Release current worker and notify.
      --active_workers_;
      worker_released_condition_.NotifyOne();
      return false;
    }
    // Consider |pending_tasks_| to avoid posting too many tasks.
    if (max_concurrency > active_workers_ + pending_tasks_) {
      num_tasks_to_post = max_concurrency - active_workers_ - pending_tasks_;
      pending_tasks_ += num_tasks_to_post;
    }
  }
  // Post additional worker tasks to reach |max_concurrency| in the case that
  // max concurrency increased. This is not strictly necessary, since
  // NotifyConcurrencyIncrease() should eventually be invoked. However, some
  // users of PostJob() batch work and tend to call NotifyConcurrencyIncrease()
  // late. Posting here allows us to spawn new workers sooner.
  for (size_t i = 0; i < num_tasks_to_post; ++i) {
    CallOnWorkerThread(std::make_unique<DefaultJobWorker>(shared_from_this(),
                                                          job_task_.get()));
  }
  return true;
}

bool DefaultJobState::WaitForParticipationOpportunityLockRequired() {
  size_t max_concurrency = CappedMaxConcurrency();
  while (active_workers_ > max_concurrency && active_workers_ > 1) {
    worker_released_condition_.Wait(&mutex_);
    max_concurrency = CappedMaxConcurrency();
  }
  if (active_workers_ <= max_concurrency) return true;
  DCHECK_EQ(1U, active_workers_);
  DCHECK_EQ(0U, max_concurrency);
  active_workers_ = 0;
  is_canceled_.store(true, std::memory_order_relaxed);
  return false;
}

size_t DefaultJobState::CappedMaxConcurrency() const {
  return std::min(job_task_->GetMaxConcurrency(), num_worker_threads_);
}

void DefaultJobState::CallOnWorkerThread(std::unique_ptr<Task> task) {
  switch (priority_) {
    case TaskPriority::kBestEffort:
      return platform_->CallLowPriorityTaskOnWorkerThread(std::move(task));
    case TaskPriority::kUserVisible:
      return platform_->CallOnWorkerThread(std::move(task));
    case TaskPriority::kUserBlocking:
      return platform_->CallBlockingTaskOnWorkerThread(std::move(task));
  }
}

DefaultJobHandle::DefaultJobHandle(std::shared_ptr<DefaultJobState> state)
    : state_(std::move(state)) {
  state_->NotifyConcurrencyIncrease();
}

DefaultJobHandle::~DefaultJobHandle() { DCHECK_EQ(nullptr, state_); }

void DefaultJobHandle::Join() {
  state_->Join();
  state_ = nullptr;
}
void DefaultJobHandle::Cancel() {
  state_->CancelAndWait();
  state_ = nullptr;
}

}  // namespace platform
}  // namespace v8
