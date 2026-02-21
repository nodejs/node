// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/debug/stack_trace.h"

#include <iomanip>
#include <ostream>

#include "src/base/platform/platform.h"

namespace v8 {
namespace base {
namespace debug {

bool EnableInProcessStackDumping() {
  // The system crashlogger captures and prints backtraces which are then
  // symbolized by a host-side script that runs addr2line. Because symbols are
  // not available on device, there's not much use in implementing in-process
  // capture.
  return false;
}

void DisableSignalStackDump() {}

StackTrace::StackTrace() {}

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
