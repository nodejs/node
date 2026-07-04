// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMMON_SYNCHRONIZATION_POINT_SUPPORT_H_
#define V8_COMMON_SYNCHRONIZATION_POINT_SUPPORT_H_

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>

#include "include/v8config.h"
#include "src/base/macros.h"
#include "src/base/platform/condition-variable.h"
#include "src/base/platform/mutex.h"
#include "src/base/platform/time.h"
#include "src/execution/thread-id.h"

namespace v8 {
namespace internal {

// Singleton that backs up the synchronization point functionality, which allows
// blocking the V8 execution at potentially interesting places in tests/fuzzers
// to simulate specific timings.
//
// The C++ code should use the `SYNCHRONIZATION_POINT` macro in order to let the
// execution there be controlled via the %BlockAt/%Resume/%WaitUntilBlocked
// native APIs.
class V8_EXPORT_PRIVATE SynchronizationPointSupport {
 public:
  static void Initialize();
  static V8_INLINE SynchronizationPointSupport* Get() { return instance_; }

  // Requests a block at the given synchronization point. When a thread reaches
  // it, it will block for the specified timeout (or less, if it is resumed).
  void RequestBlockAt(std::string synchronization_point,
                      base::TimeDelta timeout);

  // Resumes a thread currently blocked at the given synchronization point.
  // Returns false if a block wasn't requested.
  bool Resume(std::string_view synchronization_point);

  // Waits until the given synchronization point is reached by some thread.
  // Returns false if a block wasn't requested or on timeout (in which case
  // `timed_out` is set to true as well). Note: this does not request a block;
  // it must be requested first.
  bool WaitUntilBlocked(std::string_view synchronization_point,
                        base::TimeDelta timeout, bool& timed_out);

  // Called when the synchronization point is reached; blocks if requested.
  V8_INLINE void BlockIfRequested(std::string_view synchronization_point) {
    if (!any_requested_.load(std::memory_order_relaxed)) [[likely]] {
      return;
    }
    BlockIfRequestedSlow(synchronization_point);
  }

 private:
  struct SyncPointState {
    base::ConditionVariable cv;
    // Set to true to signal that any thread that reaches this point should
    // block.
    bool block_requested = false;
    // Number of threads that have reached the point and are currently blocked.
    int blocked_threads = 0;
    // The identity of the thread that set the `block_requested` flag.
    ThreadId block_requester_thread = ThreadId::Invalid();
    // How long a thread should remain blocked, unless resumed.
    base::TimeDelta block_timeout;
  };

  V8_NOINLINE V8_PRESERVE_MOST void BlockIfRequestedSlow(
      std::string_view synchronization_point);

  static SynchronizationPointSupport* instance_;

  std::atomic<bool> any_requested_{false};
  base::Mutex mutex_;
  std::map<std::string, std::unique_ptr<SyncPointState>, std::less<>> states_;
};

// A synchronization point that allows a thread reaching it to be predictably
// blocked and resumed by JS testing intrinsics (%BlockAt and %Resume).
#define SYNCHRONIZATION_POINT(sync_point_name)                        \
  v8::internal::SynchronizationPointSupport::Get()->BlockIfRequested( \
      sync_point_name)

// Similar to `SYNCHRONIZATION_POINT`, but is intended for being used on hot
// paths. Unlike the former, it compiles to nop unless the
// `v8_enable_test_only_sync_points` GN flag is true.
#ifdef V8_ENABLE_TEST_ONLY_SYNC_POINTS
#define SYNCHRONIZATION_POINT_TEST_ONLY(sync_point_name) \
  SYNCHRONIZATION_POINT(sync_point_name)
#else
#define SYNCHRONIZATION_POINT_TEST_ONLY(sync_point_name) ((void)0)
#endif

}  // namespace internal
}  // namespace v8

#endif  // V8_COMMON_SYNCHRONIZATION_POINT_SUPPORT_H_
