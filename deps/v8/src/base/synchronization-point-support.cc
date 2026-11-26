// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/synchronization-point-support.h"

#include <inttypes.h>

#include <atomic>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include "src/base/platform/mutex.h"
#include "src/base/platform/platform.h"
#include "src/base/platform/time.h"
#include "src/base/thread-id.h"

namespace v8 {
namespace base {

namespace {
template <typename Callback>
void InvokeWithWrapper(SynchronizationPointSupport::WaitWrapper wrapper,
                       Callback& callback) {
  if (wrapper) {
    wrapper([](void* data) { (*static_cast<Callback*>(data))(); }, &callback);
  } else {
    callback();
  }
}
}  // namespace

SynchronizationPointSupport* SynchronizationPointSupport::instance_ = nullptr;

void SynchronizationPointSupport::Initialize() {
  if (!instance_) {
    instance_ = new SynchronizationPointSupport();
  }
}

void SynchronizationPointSupport::RequestBlockAt(
    std::string synchronization_point, TimeDelta timeout) {
  MutexGuard lock(&mutex_);
  std::unique_ptr<SyncPointState>& state =
      states_[std::move(synchronization_point)];
  if (!state) state = std::make_unique<SyncPointState>();
  if (!state->block_requested) {
    state->block_requested = true;
    ++active_requests_;
    any_requested_.store(true, std::memory_order_release);
  }
  state->block_requester_thread = ThreadId::Current();
  state->block_timeout = timeout;
}

void SynchronizationPointSupport::ClearBlockRequestLocked(
    SyncPointState* state) {
  DCHECK(state->block_requested);
  state->block_requested = false;
  DCHECK_GT(active_requests_, 0);
  if (--active_requests_ == 0) {
    any_requested_.store(false, std::memory_order_release);
  }
  state->cv.NotifyAll();
}

bool SynchronizationPointSupport::Resume(
    std::string_view synchronization_point) {
  MutexGuard lock(&mutex_);
  auto it = states_.find(synchronization_point);
  if (it != states_.end()) {
    SyncPointState* state = it->second.get();
    if (state->block_requested) {
      ClearBlockRequestLocked(state);
      return true;
    }
  }
  OS::PrintError("Synchronization point not armed\n");
  return false;
}

bool SynchronizationPointSupport::WaitUntilBlocked(
    std::string_view synchronization_point, TimeDelta timeout) {
  // Safe: map elements are never removed, so the pointer is stable.
  SyncPointState* state;
  {
    MutexGuard lock(&mutex_);
    auto it = states_.find(synchronization_point);
    if (it == states_.end()) {
      OS::PrintError("Synchronization point not armed\n");
      return false;
    }
    state = it->second.get();
  }

  bool success = false;
  auto wait_loop = [this, state, timeout, &success]() {
    MutexGuard lock(&mutex_);
    const TimeTicks start = TimeTicks::Now();
    while (state->blocked_threads == 0) {
      if (!state->block_requested) {
        OS::PrintError("Synchronization point not armed\n");
        return;
      }
      const TimeDelta remaining = start + timeout - TimeTicks::Now();
      if (remaining <= TimeDelta()) {
        OS::PrintError("Synchronization point wait timed out\n");
        return;
      }
      std::ignore = state->cv.WaitFor(&mutex_, remaining);
    }
    success = true;
  };
  InvokeWithWrapper(wait_wrapper_.load(std::memory_order_acquire), wait_loop);
  return success;
}

void SynchronizationPointSupport::BlockIfRequestedSlow(
    std::string_view synchronization_point, AllowSafepoint allow_safepoint) {
  // Safe: map elements are never removed, so the pointer is stable.
  SyncPointState* state = nullptr;
  {
    MutexGuard lock(&mutex_);
    auto it = states_.find(synchronization_point);
    if (it == states_.end()) return;
    state = it->second.get();
    if (!state->block_requested) return;
    if (state->block_requester_thread == ThreadId::Current()) {
      OS::PrintError(
          "Warning: ignoring self-deadlock at synchronization point '%.*s'\n",
          static_cast<int>(synchronization_point.length()),
          synchronization_point.data());
      return;
    }
  }

  bool timed_out = false;
  TimeDelta timeout;
  auto wait_loop = [this, state, &timed_out, &timeout]() {
    MutexGuard lock(&mutex_);
    if (!state->block_requested ||
        state->block_requester_thread == ThreadId::Current()) {
      return;
    }
    const TimeTicks start = TimeTicks::Now();
    ++state->blocked_threads;
    state->cv.NotifyAll();
    while (state->block_requested) {
      timeout = state->block_timeout;
      TimeDelta remaining = start + timeout - TimeTicks::Now();
      if (remaining <= TimeDelta()) break;
      std::ignore = state->cv.WaitFor(&mutex_, remaining);
    }
    timed_out = state->block_requested;
    if (timed_out) {
      ClearBlockRequestLocked(state);
    }
    --state->blocked_threads;
  };
  if (allow_safepoint.value()) {
    InvokeWithWrapper(wait_wrapper_.load(std::memory_order_acquire), wait_loop);
  } else {
    wait_loop();
  }

  if (timed_out) {
    OS::PrintError(
        "Warning: Synchronization point '%.*s' timed out after %" PRId64
        " ms. Resuming automatically.\n",
        static_cast<int>(synchronization_point.length()),
        synchronization_point.data(), timeout.InMilliseconds());
  }
}

}  // namespace base
}  // namespace v8
