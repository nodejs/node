// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_GLOBALS_H_
#define V8_COMPILER_GLOBALS_H_

#include "src/common/globals.h"
#include "src/flags/flags.h"
#include "src/objects/js-objects.h"

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

// The CallFeedbackRelation provides the meaning of the call feedback for a
// TurboFan JSCall operator
// - kReceiver: The call target was Function.prototype.apply and its receiver
//   was recorded as the feedback value.
// - kTarget: The call target was recorded as the feedback value.
// - kUnrelated: The feedback is no longer related to the call. If, during
//   lowering, a JSCall (e.g. of a higher order function) is replaced by a
//   JSCall with another target, the feedback has to be kept but is now
//   unrelated.
enum class CallFeedbackRelation { kReceiver, kTarget, kUnrelated };

inline std::ostream& operator<<(std::ostream& os,
                                CallFeedbackRelation call_feedback_relation) {
  switch (call_feedback_relation) {
    case CallFeedbackRelation::kReceiver:
      return os << "CallFeedbackRelation::kReceiver";
    case CallFeedbackRelation::kTarget:
      return os << "CallFeedbackRelation::kTarget";
    case CallFeedbackRelation::kUnrelated:
      return os << "CallFeedbackRelation::kUnrelated";
  }
  UNREACHABLE();
  return os;
}

// Maximum depth and total number of elements and properties for literal
// graphs to be considered for fast deep-copying. The limit is chosen to
// match the maximum number of inobject properties, to ensure that the
// performance of using object literals is not worse than using constructor
// functions, see crbug.com/v8/6211 for details.
const int kMaxFastLiteralDepth = 3;
const int kMaxFastLiteralProperties = JSObject::kMaxInObjectProperties;

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
