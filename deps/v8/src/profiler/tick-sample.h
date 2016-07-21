// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PROFILER_TICK_SAMPLE_H_
#define V8_PROFILER_TICK_SAMPLE_H_

#include "include/v8.h"

#include "src/base/platform/time.h"
#include "src/frames.h"
#include "src/globals.h"

namespace v8 {
namespace internal {

class Isolate;

// ----------------------------------------------------------------------------
// Sampler
//
// A sampler periodically samples the state of the VM and optionally
// (if used for profiling) the program counter and stack pointer for
// the thread that created it.

// TickSample captures the information collected for each sample.
struct TickSample {
  // Internal profiling (with --prof + tools/$OS-tick-processor) wants to
  // include the runtime function we're calling. Externally exposed tick
  // samples don't care.
  enum RecordCEntryFrame { kIncludeCEntryFrame, kSkipCEntryFrame };

  TickSample()
      : state(OTHER),
        pc(NULL),
        external_callback_entry(NULL),
        frames_count(0),
        has_external_callback(false),
        update_stats(true),
        top_frame_type(StackFrame::NONE) {}
  void Init(Isolate* isolate, const v8::RegisterState& state,
            RecordCEntryFrame record_c_entry_frame, bool update_stats);
  static void GetStackSample(Isolate* isolate, const v8::RegisterState& state,
                             RecordCEntryFrame record_c_entry_frame,
                             void** frames, size_t frames_limit,
                             v8::SampleInfo* sample_info);
  StateTag state;  // The state of the VM.
  Address pc;      // Instruction pointer.
  union {
    Address tos;   // Top stack value (*sp).
    Address external_callback_entry;
  };
  static const unsigned kMaxFramesCountLog2 = 8;
  static const unsigned kMaxFramesCount = (1 << kMaxFramesCountLog2) - 1;
  Address stack[kMaxFramesCount];  // Call stack.
  base::TimeTicks timestamp;
  unsigned frames_count : kMaxFramesCountLog2;  // Number of captured frames.
  bool has_external_callback : 1;
  bool update_stats : 1;  // Whether the sample should update aggregated stats.
  StackFrame::Type top_frame_type : 5;
};


#if defined(USE_SIMULATOR)
class SimulatorHelper {
 public:
  // Returns true if register values were successfully retrieved
  // from the simulator, otherwise returns false.
  static bool FillRegisters(Isolate* isolate, v8::RegisterState* state);
};
#endif  // USE_SIMULATOR

}  // namespace internal
}  // namespace v8

#endif  // V8_PROFILER_TICK_SAMPLE_H_
