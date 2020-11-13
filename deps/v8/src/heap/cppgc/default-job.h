// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_DEFAULT_JOB_H_
#define V8_HEAP_CPPGC_DEFAULT_JOB_H_

#include <atomic>
#include <map>
#include <memory>
#include <unordered_set>
#include <vector>

#include "include/cppgc/platform.h"
#include "src/base/logging.h"
#include "src/base/platform/mutex.h"

namespace cppgc {
namespace internal {

template <typename Job>
class DefaultJobFactory {
 public:
  static std::shared_ptr<Job> Create(std::unique_ptr<cppgc::JobTask> job_task) {
    std::shared_ptr<Job> job =
        std::make_shared<Job>(typename Job::Key(), std::move(job_task));
    job->NotifyConcurrencyIncrease();
    return job;
  }
};

template <typename Thread>
class DefaultJobImpl {
 public:
  class JobDelegate;
  class JobHandle;

  class Key {
   private:
    Key() {}

    template <typename Job>
    friend class DefaultJobFactory;
  };

  DefaultJobImpl(Key, std::unique_ptr<cppgc::JobTask> job_task)
      : job_task_(std::move(job_task)) {}

  ~DefaultJobImpl() {
    Cancel();
    DCHECK_EQ(0, active_threads_.load(std::memory_order_relaxed));
  }

  void NotifyConcurrencyIncrease();

  void Join() {
    for (std::shared_ptr<Thread>& thread : job_threads_) thread->Join();
    job_threads_.clear();
    can_run_.store(false, std::memory_order_relaxed);
  }

  void Cancel() {
    can_run_.store(false, std::memory_order_relaxed);
    Join();
  }

  bool IsCompleted() const { return !IsRunning(); }
  bool IsRunning() const {
    uint8_t active_threads = active_threads_.load(std::memory_order_relaxed);
    return (active_threads + job_task_->GetMaxConcurrency(active_threads)) > 0;
  }

  bool CanRun() const { return can_run_.load(std::memory_order_relaxed); }

  void RunJobTask() {
    DCHECK_NOT_NULL(job_task_);
    NotifyJobThreadStart();
    JobDelegate delegate(this);
    job_task_->Run(&delegate);
    NotifyJobThreadEnd();
  }

 protected:
  virtual std::shared_ptr<Thread> CreateThread(DefaultJobImpl*) = 0;

  void NotifyJobThreadStart() {
    active_threads_.fetch_add(1, std::memory_order_relaxed);
  }
  void NotifyJobThreadEnd() {
    active_threads_.fetch_sub(1, std::memory_order_relaxed);
  }

  void GuaranteeAvailableIds(uint8_t max_threads) {
    if (max_threads <= highest_thread_count_) return;
    v8::base::MutexGuard guard(&ids_lock_);
    while (highest_thread_count_ < max_threads) {
      available_ids_.push_back(++highest_thread_count_);
    }
  }

  std::unique_ptr<cppgc::JobTask> job_task_;
  std::vector<std::shared_ptr<Thread>> job_threads_;
  std::atomic_bool can_run_{true};
  std::atomic<uint8_t> active_threads_{0};

  // Task id management.
  v8::base::Mutex ids_lock_;
  std::vector<uint8_t> available_ids_;
  uint8_t highest_thread_count_ = -1;
};

template <typename Thread>
class DefaultJobImpl<Thread>::JobDelegate final : public cppgc::JobDelegate {
 public:
  explicit JobDelegate(DefaultJobImpl* job) : job_(job) {}
  ~JobDelegate() { ReleaseTaskId(); }
  bool ShouldYield() override { return !job_->CanRun(); }
  void NotifyConcurrencyIncrease() override {
    job_->NotifyConcurrencyIncrease();
  }
  uint8_t GetTaskId() override {
    AcquireTaskId();
    return job_thread_id_;
  }

 private:
  void AcquireTaskId() {
    if (job_thread_id_ != kInvalidTaskId) return;
    v8::base::MutexGuard guard(&job_->ids_lock_);
    job_thread_id_ = job_->available_ids_.back();
    DCHECK_NE(kInvalidTaskId, job_thread_id_);
    job_->available_ids_.pop_back();
  }
  void ReleaseTaskId() {
    if (job_thread_id_ == kInvalidTaskId) return;
    v8::base::MutexGuard guard(&job_->ids_lock_);
    job_->available_ids_.push_back(job_thread_id_);
  }

  DefaultJobImpl* const job_;
  static constexpr uint8_t kInvalidTaskId = std::numeric_limits<uint8_t>::max();
  uint8_t job_thread_id_ = kInvalidTaskId;
};

template <typename Thread>
void DefaultJobImpl<Thread>::NotifyConcurrencyIncrease() {
  DCHECK(CanRun());
  static const size_t kMaxThreads = Thread::GetMaxSupportedConcurrency();
  uint8_t current_active_threads =
      active_threads_.load(std::memory_order_relaxed);
  size_t max_threads = std::min(
      kMaxThreads, job_task_->GetMaxConcurrency(current_active_threads));
  if (current_active_threads >= max_threads) return;
  DCHECK_LT(max_threads, std::numeric_limits<uint8_t>::max());
  GuaranteeAvailableIds(max_threads);
  for (uint8_t new_threads = max_threads - current_active_threads;
       new_threads > 0; --new_threads) {
    std::shared_ptr<Thread> thread = CreateThread(this);
    job_threads_.push_back(thread);
  }
}

template <typename Thread>
class DefaultJobImpl<Thread>::JobHandle final : public cppgc::JobHandle {
 public:
  explicit JobHandle(std::shared_ptr<DefaultJobImpl> job)
      : job_(std::move(job)) {
    DCHECK_NOT_NULL(job_);
  }

  void NotifyConcurrencyIncrease() override {
    job_->NotifyConcurrencyIncrease();
  }
  void Join() override { job_->Join(); }
  void Cancel() override { job_->Cancel(); }
  bool IsCompleted() override { return job_->IsCompleted(); }
  bool IsRunning() override { return job_->IsRunning(); }

 private:
  std::shared_ptr<DefaultJobImpl> job_;
};

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_DEFAULT_JOB_H_
