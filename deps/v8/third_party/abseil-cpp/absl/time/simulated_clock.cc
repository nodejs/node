// Copyright 2026 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "absl/time/simulated_clock.h"

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "absl/base/config.h"
#include "absl/base/macros.h"
#include "absl/base/nullability.h"
#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"

namespace absl {
ABSL_NAMESPACE_BEGIN

// There are a few tricky details in the implementation of SimulatedClock.
//
// The external mutex that is passed as a param of AwaitWithDeadline() is not
// under the control of SimulatedClock; in particular, it can be destroyed any
// time after AwaitWithDeadline() returns.  This requires the wakeup call from
// AdvanceTime() to avoid grabbing the external mutex if AwaitWithDeadline()
// has returned.  This is accomplished by allowing the waiter to be cancelled
// via a bool guarded by a mutex in WakeUpInfo.
//
// Once AwaitWithDeadline() has released lock_, someone nefarious might call
// the SimulatedClock destructor, so it isn't possible for AwaitWithDeadline()
// to remove the wakeup call from waiters_ if the condition it is awaiting
// becomes true; it cancels the waiter instead.  This means that,
// theoretically, many obsolete entries could pile up in waiters_ if
// AwaitWithDeadline() keeps being called but simulated time is not advanced.
// This seems unlikely to happen in practice.
//
// The WakeUpInfo in waiters_ are always awoken via WakeUp() (and
// removed) or cancelled before AwaitWithDeadline() returns.  If a
// waiters_ value is cancelled then calling its WakeUp() method will
// short-circuit before touching the external mutex.

class SimulatedClock::WakeUpInfo {
 public:
  WakeUpInfo(absl::Mutex* mu, absl::Condition cond)
      : mu_(mu),
        cond_(cond),
        wakeup_time_passed_(false),
        cancelled_(false),
        wakeup_called_(false) {}

  void WakeUp() {
    // If we are cancelled then AwaitWithDeadline may have returned, in which
    // case we can't lock mu_.
    {
      absl::MutexLock lock(cancellation_mu_);
      if (cancelled_) return;
      wakeup_called_ = true;
    }
    absl::MutexLock lock(*mu_);
    wakeup_time_passed_ = true;
  }

  void AwaitConditionOrWakeUp() {
    mu_->Await(absl::Condition(this, &WakeUpInfo::Ready));
  }

  void CancelOrAwaitWakeUp() {
    bool wakeup_called;
    {
      absl::MutexLock lock(cancellation_mu_);
      cancelled_ = true;
      wakeup_called = wakeup_called_;
    }
    if (wakeup_called && !wakeup_time_passed_) {
      // Wait for WakeUp to complete.
      //
      // Note that this will unlock 'mu_'; this is actually necessary
      // so that WakeUp() can unblock and complete.  This does allow
      // for 'cond_' to potentially change from true to false; that is
      // OK, since WakeUp() is being called, so the deadline must be
      // past, and so this method is fulfilling its duties.  (Well, the
      // destructor might be calling WakeUp(), but if you're deleting
      // time itself while waiting for a deadline to pass, you deserve
      // what you get.)
      mu_->Await(absl::Condition(&wakeup_time_passed_));
    }
  }

 private:
  bool Ready() const { return wakeup_time_passed_ || cond_.Eval(); }

  absl::Mutex* mu_;
  absl::Condition cond_;
  bool wakeup_time_passed_;
  absl::Mutex cancellation_mu_;
  bool cancelled_ ABSL_GUARDED_BY(cancellation_mu_);
  bool wakeup_called_ ABSL_GUARDED_BY(cancellation_mu_);
};

SimulatedClock::SimulatedClock(absl::Time t) : now_(t) {}

SimulatedClock::~SimulatedClock() {
  // Wake up all existing waiters.
  WaiterList waiters;
  {
    absl::MutexLock l(lock_);
    waiters.swap(waiters_);
  }
  for (auto& iter : waiters) {
    iter.second->WakeUp();
  }
}

absl::Time SimulatedClock::TimeNow() {
  absl::ReaderMutexLock l(lock_);
  return now_;
}

void SimulatedClock::Sleep(absl::Duration d) { SleepUntil(TimeNow() + d); }

int64_t SimulatedClock::SetTime(absl::Time t) ABSL_NO_THREAD_SAFETY_ANALYSIS {
  return UpdateTime([this, t]()
                        ABSL_EXCLUSIVE_LOCKS_REQUIRED(lock_) { now_ = t; });
}

int64_t SimulatedClock::AdvanceTime(absl::Duration d)
    ABSL_NO_THREAD_SAFETY_ANALYSIS {
  return UpdateTime([this, d]()
                        ABSL_EXCLUSIVE_LOCKS_REQUIRED(lock_) { now_ += d; });
}

template <class T>
int64_t SimulatedClock::UpdateTime(const T& now_updater) {
  // Deadlock could occur if UpdateTime() were to hold lock_ while waking up
  // waiters, since waking up requires acquiring the external mutex, and
  // AwaitWithDeadline() acquires the mutexes in the opposite order.  So let's
  // first grab all the wakeup callbacks, then release lock_, then call the
  // callbacks.
  std::vector<WaiterList::mapped_type> wakeup_calls;

  lock_.lock();
  now_updater();  // reset now_
  WaiterList::iterator iter;
  while (((iter = waiters_.begin()) != waiters_.end()) &&
         (iter->first <= now_)) {
    wakeup_calls.push_back(std::move(iter->second));
    waiters_.erase(iter);
  }
  lock_.unlock();

  for (const auto& wakeup_call : wakeup_calls) {
    wakeup_call->WakeUp();
  }

  return static_cast<int64_t>(wakeup_calls.size());
}

void SimulatedClock::SleepUntil(absl::Time wakeup_time) {
  absl::Mutex mu;
  absl::MutexLock lock(mu);
  bool f = false;
  AwaitWithDeadline(&mu, absl::Condition(&f), wakeup_time);
}

bool SimulatedClock::AwaitWithDeadline(absl::Mutex* mu,
                                       const absl::Condition& cond,
                                       absl::Time deadline) {
  mu->AssertReaderHeld();

  // Evaluate cond outside our own lock to minimize contention.
  const bool ready = cond.Eval();

  lock_.lock();
  num_await_calls_++;

  // Return now if the deadline is already past, or if the condition is true.
  // This avoids creating a WakeUpInfo that won't be deleted until an
  // appropriate UpdateTime() call.
  if (deadline <= now_ || ready) {
    lock_.unlock();
    return ready;
  }

  auto wakeup_info = std::make_shared<WakeUpInfo>(mu, cond);
  waiters_.insert(std::make_pair(deadline, wakeup_info));

  lock_.unlock();
  // SimulatedClock may be destroyed any time after this, so we can't
  // acquire lock_ again.

  // Wait until either cond.Eval() becomes true, or the deadline has passed.
  wakeup_info->AwaitConditionOrWakeUp();

  // Cancel the wakeup call, or if it's already in progress, wait for it to
  // finish, since we must ensure no one touches 'mu' or 'cond' after we return.
  wakeup_info->CancelOrAwaitWakeUp();

  return cond.Eval();
}

std::optional<absl::Time> SimulatedClock::GetEarliestWakeupTime() const {
  absl::ReaderMutexLock l(lock_);
  if (waiters_.empty()) {
    return std::nullopt;
  }
  return waiters_.begin()->first;
}

ABSL_NAMESPACE_END
}  // namespace absl
