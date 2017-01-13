// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Slightly adapted for inclusion in V8.
// Copyright 2016 the V8 project authors. All rights reserved.

#include "src/base/debug/stack_trace.h"

#include <signal.h>
#include <stddef.h>
#include <string.h>
#include <unwind.h>

#include <src/base/platform/platform.h>

#include <iomanip>
#include <ostream>

namespace {

struct StackCrawlState {
  StackCrawlState(uintptr_t* frames, size_t max_depth)
      : frames(frames),
        frame_count(0),
        max_depth(max_depth),
        have_skipped_self(false) {}

  uintptr_t* frames;
  size_t frame_count;
  size_t max_depth;
  bool have_skipped_self;
};

_Unwind_Reason_Code TraceStackFrame(_Unwind_Context* context, void* arg) {
  StackCrawlState* state = static_cast<StackCrawlState*>(arg);
  uintptr_t ip = _Unwind_GetIP(context);

  // The first stack frame is this function itself.  Skip it.
  if (ip != 0 && !state->have_skipped_self) {
    state->have_skipped_self = true;
    return _URC_NO_REASON;
  }

  state->frames[state->frame_count++] = ip;
  if (state->frame_count >= state->max_depth)
    return _URC_END_OF_STACK;
  return _URC_NO_REASON;
}

}  // namespace

namespace v8 {
namespace base {
namespace debug {

bool EnableInProcessStackDumping() {
  // When running in an application, our code typically expects SIGPIPE
  // to be ignored.  Therefore, when testing that same code, it should run
  // with SIGPIPE ignored as well.
  // TODO(phajdan.jr): De-duplicate this SIGPIPE code.
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_handler = SIG_IGN;
  sigemptyset(&action.sa_mask);
  return (sigaction(SIGPIPE, &action, NULL) == 0);
}

void DisableSignalStackDump() {
}

StackTrace::StackTrace() {
  StackCrawlState state(reinterpret_cast<uintptr_t*>(trace_), kMaxTraces);
  _Unwind_Backtrace(&TraceStackFrame, &state);
  count_ = state.frame_count;
}

void StackTrace::Print() const {
  std::string backtrace = ToString();
  OS::Print("%s\n", backtrace.c_str());
}

void StackTrace::OutputToStream(std::ostream* os) const {
  for (size_t i = 0; i < count_; ++i) {
    *os << "#" << std::setw(2) << i << trace_[i] << "\n";
  }
}

}  // namespace debug
}  // namespace base
}  // namespace v8
