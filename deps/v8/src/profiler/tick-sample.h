// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PROFILER_TICK_SAMPLE_H_
#define V8_PROFILER_TICK_SAMPLE_H_

#include "include/v8-profiler.h"
#include "src/base/platform/time.h"
#include "src/globals.h"

namespace v8 {
namespace internal {

class Isolate;

struct TickSample : public v8::TickSample {
  void Init(Isolate* isolate, const v8::RegisterState& state,
            RecordCEntryFrame record_c_entry_frame, bool update_stats,
            bool use_simulator_reg_state = true);
  base::TimeTicks timestamp;

  void print() const;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_PROFILER_TICK_SAMPLE_H_
