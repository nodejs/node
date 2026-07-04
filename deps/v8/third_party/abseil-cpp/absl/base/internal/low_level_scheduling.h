// Copyright 2017 The Abseil Authors.
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
//
// Core interfaces and definitions used by by low-level interfaces such as
// SpinLock.

#ifndef ABSL_BASE_INTERNAL_LOW_LEVEL_SCHEDULING_H_
#define ABSL_BASE_INTERNAL_LOW_LEVEL_SCHEDULING_H_

#include <atomic>

#include "absl/base/internal/raw_logging.h"
#include "absl/base/internal/scheduling_mode.h"
#include "absl/base/internal/thread_identity.h"
#include "absl/base/macros.h"

// The following two declarations exist so SchedulingGuard may friend them with
// the appropriate language linkage.  These callbacks allow libc internals, such
// as function level statics, to schedule cooperatively when locking.
extern "C" bool __google_disable_rescheduling(void);
extern "C" void __google_enable_rescheduling(bool disable_result);

namespace absl {
ABSL_NAMESPACE_BEGIN
class CondVar;
class Mutex;

namespace synchronization_internal {
int MutexDelay(int32_t c, int mode);
}  // namespace synchronization_internal

namespace base_internal {

class SchedulingHelper;  // To allow use of SchedulingGuard.
class SpinLock;          // To allow use of SchedulingGuard.

// SchedulingGuard
// Provides guard semantics that may be used to disable cooperative rescheduling
// of the calling thread within specific program blocks.  This is used to
// protect resources (e.g. low-level SpinLocks or Domain code) that cooperative
// scheduling depends on.
//
// Domain implementations capable of rescheduling in reaction to involuntary
// kernel thread actions (e.g blocking due to a pagefault or syscall) must
// guarantee that an annotated thread is not allowed to (cooperatively)
// reschedule until the annotated region is complete.
//
// It is an error to attempt to use a cooperatively scheduled resource (e.g.
// Mutex) within a rescheduling-disabled region.
//
// All methods are async-signal safe.
class SchedulingGuard {
 public:
  // Returns true iff the calling thread may be cooperatively rescheduled.
  static bool ReschedulingIsAllowed();
  SchedulingGuard(const SchedulingGuard&) = delete;
  SchedulingGuard& operator=(const SchedulingGuard&) = delete;

  // Disable cooperative rescheduling of the calling thread.  It may still
  // initiate scheduling operations (e.g. wake-ups), however, it may not itself
  // reschedule.  Nestable.  The returned result is opaque, clients should not
  // attempt to interpret it.
  // REQUIRES: Result must be passed to a pairing EnableScheduling().
  static bool DisableRescheduling();

  // Marks the end of a rescheduling disabled region, previously started by
  // DisableRescheduling().
  // REQUIRES: Pairs with innermost call (and result) of DisableRescheduling().
  static void EnableRescheduling(bool disable_result);

  // A scoped helper for {Disable, Enable}Rescheduling().
  // REQUIRES: destructor must run in same thread as constructor.
  struct ScopedDisable {
    ScopedDisable() { disabled = SchedulingGuard::DisableRescheduling(); }
    ~ScopedDisable() { SchedulingGuard::EnableRescheduling(disabled); }

    bool disabled;
  };

  // A scoped helper to enable rescheduling temporarily.
  // REQUIRES: destructor must run in same thread as constructor.
  class ScopedEnable {
   public:
    ScopedEnable();
    ~ScopedEnable();

   private:
    int scheduling_disabled_depth_;
  };
};

//------------------------------------------------------------------------------
// End of public interfaces.
//------------------------------------------------------------------------------

inline bool SchedulingGuard::ReschedulingIsAllowed() {
  ThreadIdentity* identity = CurrentThreadIdentityIfPresent();
  if (identity != nullptr) {
    ThreadIdentity::SchedulerState* state = &identity->scheduler_state;
    // For a thread to be eligible for re-scheduling it must have a bound
    // schedulable (otherwise it's not cooperative) and not be within a
    // SchedulerGuard region.
    return state->bound_schedulable.load(std::memory_order_relaxed) !=
               nullptr &&
           state->scheduling_disabled_depth.load(std::memory_order_relaxed) ==
               0;
  } else {
    // Cooperative threads always have a ThreadIdentity.
    return false;
  }
}

// We don't use [[nodiscard]] here as some clients (e.g.
// FinishPotentiallyBlockingRegion()) cannot yet properly consume it.
inline bool SchedulingGuard::DisableRescheduling() {
  ThreadIdentity* identity;
  identity = CurrentThreadIdentityIfPresent();
  if (identity != nullptr) {
    // The depth is accessed concurrently from other threads, so it must be
    // atomic, but it's only mutated from this thread, so we don't need an
    // atomic increment.
    int old_val = identity->scheduler_state.scheduling_disabled_depth.load(
        std::memory_order_relaxed);
    identity->scheduler_state.scheduling_disabled_depth.store(
        old_val + 1, std::memory_order_relaxed);
    return true;
  } else {
    return false;
  }
}

inline void SchedulingGuard::EnableRescheduling(bool disable_result) {
  if (!disable_result) {
    // There was no installed thread identity at the time that scheduling was
    // disabled, so we have nothing to do.  This is an implementation detail
    // that may change in the future, clients may not depend on it.
    // EnableRescheduling() must always be called.
    return;
  }

  ThreadIdentity* identity;
  // A thread identity exists, see above
  identity = CurrentThreadIdentityIfPresent();
  // The depth is accessed concurrently from other threads, so it must be
  // atomic, but it's only mutated from this thread, so we don't need an atomic
  // decrement.
  int old_val = identity->scheduler_state.scheduling_disabled_depth.load(
      std::memory_order_relaxed);
  identity->scheduler_state.scheduling_disabled_depth.store(
      old_val - 1, std::memory_order_relaxed);
}

inline SchedulingGuard::ScopedEnable::ScopedEnable() {
  ThreadIdentity* identity;
  identity = CurrentThreadIdentityIfPresent();
  if (identity != nullptr) {
    scheduling_disabled_depth_ =
        identity->scheduler_state.scheduling_disabled_depth.load(
            std::memory_order_relaxed);
    if (scheduling_disabled_depth_ != 0) {
      // The store below does not need to be compare_exchange because
      // the value is never modified concurrently (only accessed).
      identity->scheduler_state.scheduling_disabled_depth.store(
          0, std::memory_order_relaxed);
    }
  } else {
    scheduling_disabled_depth_ = 0;
  }
}

inline SchedulingGuard::ScopedEnable::~ScopedEnable() {
  if (scheduling_disabled_depth_ == 0) {
    return;
  }
  ThreadIdentity* identity = CurrentThreadIdentityIfPresent();
  // itentity is guaranteed to exist, see the constructor above.
  identity->scheduler_state.scheduling_disabled_depth.store(
      scheduling_disabled_depth_, std::memory_order_relaxed);
}

}  // namespace base_internal
ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_BASE_INTERNAL_LOW_LEVEL_SCHEDULING_H_
