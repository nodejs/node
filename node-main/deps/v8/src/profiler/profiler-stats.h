// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PROFILER_PROFILER_STATS_H_
#define V8_PROFILER_PROFILER_STATS_H_

#include <atomic>

namespace v8 {
namespace internal {

// Stats are used to diagnose the reasons for dropped or unnattributed frames.
class ProfilerStats {
 public:
  enum Reason {
    // Reasons we fail to record a TickSample.
    kTickBufferFull,
    kIsolateNotLocked,
    // These all generate a TickSample.
    kSimulatorFillRegistersFailed,
    kNoFrameRegion,
    kInCallOrApply,
    kNoSymbolizedFrames,
    kNullPC,

    kNumberOfReasons,
  };

  static ProfilerStats* Instance() {
    static ProfilerStats stats;
    return &stats;
  }

  void AddReason(Reason reason);
  void Clear();
  void Print() const;

 private:
  ProfilerStats() = default;
  static const char* ReasonToString(Reason reason);

  std::atomic_int counts_[Reason::kNumberOfReasons] = {};
};

}  // namespace internal
}  // namespace v8

#endif  // V8_PROFILER_PROFILER_STATS_H_
