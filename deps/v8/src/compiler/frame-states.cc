// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/functional.h"
#include "src/compiler/frame-states.h"

namespace v8 {
namespace internal {
namespace compiler {

size_t hash_value(OutputFrameStateCombine const& sc) {
  return base::hash_combine(sc.kind_, sc.parameter_);
}


std::ostream& operator<<(std::ostream& os, OutputFrameStateCombine const& sc) {
  switch (sc.kind_) {
    case OutputFrameStateCombine::kPushOutput:
      if (sc.parameter_ == 0) return os << "Ignore";
      return os << "Push(" << sc.parameter_ << ")";
    case OutputFrameStateCombine::kPokeAt:
      return os << "PokeAt(" << sc.parameter_ << ")";
  }
  UNREACHABLE();
  return os;
}


bool operator==(FrameStateCallInfo const& lhs, FrameStateCallInfo const& rhs) {
  return lhs.type() == rhs.type() && lhs.bailout_id() == rhs.bailout_id() &&
         lhs.state_combine() == rhs.state_combine();
}


bool operator!=(FrameStateCallInfo const& lhs, FrameStateCallInfo const& rhs) {
  return !(lhs == rhs);
}


size_t hash_value(FrameStateCallInfo const& info) {
  return base::hash_combine(info.type(), info.bailout_id(),
                            info.state_combine());
}


std::ostream& operator<<(std::ostream& os, FrameStateCallInfo const& info) {
  return os << info.type() << ", " << info.bailout_id() << ", "
            << info.state_combine();
}
}
}
}
