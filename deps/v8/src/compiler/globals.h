// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_GLOBALS_H_
#define V8_COMPILER_GLOBALS_H_

#include "src/common/globals.h"

namespace v8 {
namespace internal {
namespace compiler {

enum class StackCheckKind {
  kJSFunctionEntry = 0,
  kJSIterationBody,
  kCodeStubAssembler,
  kWasm,
};

inline std::ostream& operator<<(std::ostream& os, StackCheckKind kind) {
  switch (kind) {
    case StackCheckKind::kJSFunctionEntry:
      return os << "JSFunctionEntry";
    case StackCheckKind::kJSIterationBody:
      return os << "JSIterationBody";
    case StackCheckKind::kCodeStubAssembler:
      return os << "CodeStubAssembler";
    case StackCheckKind::kWasm:
      return os << "Wasm";
  }
  UNREACHABLE();
}

inline size_t hash_value(StackCheckKind kind) {
  return static_cast<size_t>(kind);
}

// The CallFeedbackRelation states whether the target feedback stored with a
// JSCall is related to the call. If, during lowering, a JSCall (e.g. of a
// higher order function) is replaced by a JSCall with another target, the
// feedback has to be kept but is now unrelated.
enum class CallFeedbackRelation { kRelated, kUnrelated };

inline std::ostream& operator<<(std::ostream& os,
                                CallFeedbackRelation call_feedback_relation) {
  switch (call_feedback_relation) {
    case CallFeedbackRelation::kRelated:
      return os << "CallFeedbackRelation::kRelated";
    case CallFeedbackRelation::kUnrelated:
      return os << "CallFeedbackRelation::kUnrelated";
  }
  UNREACHABLE();
  return os;
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_GLOBALS_H_
