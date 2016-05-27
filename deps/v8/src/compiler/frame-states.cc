// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/frame-states.h"

#include "src/base/functional.h"
#include "src/handles-inl.h"

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


bool operator==(FrameStateInfo const& lhs, FrameStateInfo const& rhs) {
  return lhs.type() == rhs.type() && lhs.bailout_id() == rhs.bailout_id() &&
         lhs.state_combine() == rhs.state_combine() &&
         lhs.function_info() == rhs.function_info();
}


bool operator!=(FrameStateInfo const& lhs, FrameStateInfo const& rhs) {
  return !(lhs == rhs);
}


size_t hash_value(FrameStateInfo const& info) {
  return base::hash_combine(static_cast<int>(info.type()), info.bailout_id(),
                            info.state_combine());
}


std::ostream& operator<<(std::ostream& os, FrameStateType type) {
  switch (type) {
    case FrameStateType::kJavaScriptFunction:
      os << "JS_FRAME";
      break;
    case FrameStateType::kInterpretedFunction:
      os << "INTERPRETED_FRAME";
      break;
    case FrameStateType::kArgumentsAdaptor:
      os << "ARGUMENTS_ADAPTOR";
      break;
    case FrameStateType::kTailCallerFunction:
      os << "TAIL_CALLER_FRAME";
      break;
    case FrameStateType::kConstructStub:
      os << "CONSTRUCT_STUB";
      break;
  }
  return os;
}


std::ostream& operator<<(std::ostream& os, FrameStateInfo const& info) {
  os << info.type() << ", " << info.bailout_id() << ", "
     << info.state_combine();
  Handle<SharedFunctionInfo> shared_info;
  if (info.shared_info().ToHandle(&shared_info)) {
    os << ", " << Brief(*shared_info);
  }
  return os;
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
