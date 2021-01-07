// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_GLOBALS_H_
#define V8_COMPILER_GLOBALS_H_

#include "src/common/globals.h"
#include "src/flags/flags.h"

namespace v8 {
namespace internal {
namespace compiler {

// The nci flag is currently used to experiment with feedback collection in
// optimized code produced by generic lowering.
// Considerations:
// - Should we increment the call count? https://crbug.com/v8/10524
// - Is feedback already megamorphic in all these cases?
//
// TODO(jgruber): Remove once we've made a decision whether to collect feedback
// unconditionally.
inline bool CollectFeedbackInGenericLowering() {
  return FLAG_turbo_collect_feedback_in_generic_lowering;
}

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

// Support for floating point parameters in calls to C.
// It's currently enabled only for the platforms listed below. We don't plan
// to add support for IA32, because it has a totally different approach
// (using FP stack). As support is added to more platforms, please make sure
// to list them here in order to enable tests of this functionality.
#if defined(V8_TARGET_ARCH_X64)
#define V8_ENABLE_FP_PARAMS_IN_C_LINKAGE
#endif

#endif  // V8_COMPILER_GLOBALS_H_
