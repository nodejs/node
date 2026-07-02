// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/common/synchronization-point-support.h"

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
#include "src/execution/thread-id.h"
#include "src/heap/local-heap-inl.h"

namespace v8 {
namespace internal {

SynchronizationPointSupport* SynchronizationPointSupport::instance_ = nullptr;

void SynchronizationPointSupport::Initialize() {
  CHECK_NULL(instance_);
  instance_ = new SynchronizationPointSupport();
}

void SynchronizationPointSupport::RequestBlockAt(
    std::string synchronization_point, base::TimeDelta timeout) {
  any_requested_ = true;
  base::MutexGuard lock(&mutex_);
  std::unique_ptr<SyncPointState>& state =
      states_[std::move(synchronization_point)];
  if (!state) state = std::make_unique<SyncPointState>();
  state->block_requested = true;
  state->block_requester_thread = ThreadId::Current();
  state->block_timeout = timeout;
}

bool SynchronizationPointSupport::Resume(
    std::string_view synchronization_point) {
  base::MutexGuard lock(&mutex_);
  auto it = states_.find(synchronization_point);
  if (it == states_.end()) return false;
  SyncPointState* state = it->second.get();
  if (!state->block_requested) return false;
  state->block_requested = false;
  state->cv.NotifyAll();
  return true;
}

bool SynchronizationPointSupport::WaitUntilBlocked(
    std::string_view synchronization_point, base::TimeDelta timeout,
    bool& timed_out) {
  bool success = false;
  auto wait_loop = [this, synchronization_point, timeout, &timed_out,
                    &success]() {
    base::MutexGuard lock(&mutex_);
    auto it = states_.find(synchronization_point);
    if (it == states_.end()) return;
    SyncPointState* state = it->second.get();
    if (!state->block_requested && state->blocked_threads == 0) return;

    const base::TimeTicks start = base::TimeTicks::Now();
    while (state->blocked_threads == 0) {
      const base::TimeDelta remaining =
          start + timeout - base::TimeTicks::Now();
      if (remaining <= base::TimeDelta()) {
        timed_out = true;
        return;
      }
      std::ignore = state->cv.WaitFor(&mutex_, remaining);
    }
    success = true;
  };

  LocalHeap* local_heap = LocalHeap::Current();
  if (local_heap && local_heap->IsRunning()) {
    local_heap->ExecuteWhileParked(wait_loop);
  } else {
    wait_loop();
  }
  return success;
}

void SynchronizationPointSupport::BlockIfRequestedSlow(
    std::string_view synchronization_point) {
  // Safe: map elements are never removed, so the pointer is stable.
  SyncPointState* state = nullptr;
  {
    base::MutexGuard lock(&mutex_);
    auto it = states_.find(synchronization_point);
    if (it == states_.end()) return;
    state = it->second.get();
    if (!state->block_requested) return;
    if (state->block_requester_thread == ThreadId::Current()) {
      base::OS::PrintError(
          "Warning: ignoring self-deadlock at synchronization point '%.*s'\n",
          static_cast<int>(synchronization_point.length()),
          synchronization_point.data());
      return;
    }
  }

  base::TimeTicks start = base::TimeTicks::Now();
  base::MutexGuard lock(&mutex_);
  ++state->blocked_threads;
  state->cv.NotifyAll();
  while (state->block_requested) {
    base::TimeDelta remaining =
        start + state->block_timeout - base::TimeTicks::Now();
    if (remaining <= base::TimeDelta()) break;
    std::ignore = state->cv.WaitFor(&mutex_, remaining);
  }
  --state->blocked_threads;

  if (state->block_requested) {
    base::OS::PrintError(
        "Warning: Synchronization point '%.*s' timed out after %" PRId64
        " ms. Resuming automatically.\n",
        static_cast<int>(synchronization_point.length()),
        synchronization_point.data(), state->block_timeout.InMilliseconds());
  }
}

}  // namespace internal
}  // namespace v8
