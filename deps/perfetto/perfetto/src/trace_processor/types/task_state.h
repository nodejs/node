/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SRC_TRACE_PROCESSOR_TYPES_TASK_STATE_H_
#define SRC_TRACE_PROCESSOR_TYPES_TASK_STATE_H_

#include <stddef.h>
#include <array>

#include "perfetto/base/logging.h"
#include "perfetto/ext/base/string_view.h"
#include "perfetto/ext/base/string_writer.h"

namespace perfetto {
namespace trace_processor {
namespace ftrace_utils {

// A strongly typed representation of the TaskState enum given in sched_switch
// events.
class TaskState {
 public:
  using TaskStateStr = std::array<char, 4>;

  // The ordering and values of these fields comes from the kernel in the file
  // https://android.googlesource.com/kernel/msm.git/+/android-msm-wahoo-4.4-pie-qpr1/include/linux/sched.h#212
  enum Atom : uint16_t {
    kRunnable = 0,
    kInterruptibleSleep = 1,
    kUninterruptibleSleep = 2,
    kStopped = 4,
    kTraced = 8,
    kExitDead = 16,
    kExitZombie = 32,
    kTaskDead = 64,
    kWakeKill = 128,
    kWaking = 256,
    kParked = 512,
    kNoLoad = 1024,

    // kMaxState is used by sched switch to show a task was kernel preempted.
    // The difficult thing is that in newer kernels a task_new state is added
    // and kMaxState increases to 4096. Without the kernel version and the
    // mapping of this to the correct version of this enum we cannot be
    // accurate. Since task_new is rare we ignore it for now.
    kTaskNewOrMaxState = 2048,
    kMaxState = 4096,
    kValid = 0x8000,
  };

  TaskState() = default;
  explicit TaskState(uint16_t raw_state);
  explicit TaskState(const char* state_str);

  // Returns if this TaskState has a valid representation.
  bool is_valid() const { return state_ & kValid; }

  // Returns the string representation of this (valid) TaskState. This array
  // is null terminated. |seperator| specifies if a separator should be printed
  // between the atoms (default: \0 meaning no separator).
  // Note: This function CHECKs that |is_valid()| is true.
  TaskStateStr ToString(char separator = '\0') const;

  // Returns the raw state this class was created from.
  uint16_t raw_state() const {
    PERFETTO_DCHECK(is_valid());
    return state_ & ~kValid;
  }

  // Returns if this TaskState is runnable.
  bool is_runnable() const {
    return ((state_ & (kMaxState - 1)) == 0) ||
           ((state_ & (kTaskNewOrMaxState - 1)) == 0);
  }

  // Returns whether kernel preemption caused the exit state.
  bool is_kernel_preempt() const {
    return state_ & kTaskNewOrMaxState || state_ & kMaxState;
  }

 private:
  uint16_t state_ = 0;
};

}  // namespace ftrace_utils
}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_TYPES_TASK_STATE_H_
