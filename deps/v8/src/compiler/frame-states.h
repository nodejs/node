// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_FRAME_STATES_H_
#define V8_COMPILER_FRAME_STATES_H_

#include "src/builtins/builtins.h"
#include "src/handles.h"
#include "src/objects/shared-function-info.h"
#include "src/utils.h"

namespace v8 {
namespace internal {

namespace compiler {

class JSGraph;
class Node;
class SharedFunctionInfoRef;

// Flag that describes how to combine the current environment with
// the output of a node to obtain a framestate for lazy bailout.
class OutputFrameStateCombine {
 public:
  static const size_t kInvalidIndex = SIZE_MAX;

  static OutputFrameStateCombine Ignore() {
    return OutputFrameStateCombine(kInvalidIndex);
  }
  static OutputFrameStateCombine PokeAt(size_t index) {
    return OutputFrameStateCombine(index);
  }

  size_t GetOffsetToPokeAt() const {
    DCHECK_NE(parameter_, kInvalidIndex);
    return parameter_;
  }

  bool IsOutputIgnored() const { return parameter_ == kInvalidIndex; }

  size_t ConsumedOutputCount() const { return IsOutputIgnored() ? 0 : 1; }

  bool operator==(OutputFrameStateCombine const& other) const {
    return parameter_ == other.parameter_;
  }
  bool operator!=(OutputFrameStateCombine const& other) const {
    return !(*this == other);
  }

  friend size_t hash_value(OutputFrameStateCombine const&);
  friend std::ostream& operator<<(std::ostream&,
                                  OutputFrameStateCombine const&);

 private:
  explicit OutputFrameStateCombine(size_t parameter) : parameter_(parameter) {}

  size_t const parameter_;
};


// The type of stack frame that a FrameState node represents.
enum class FrameStateType {
  kInterpretedFunction,            // Represents an InterpretedFrame.
  kArgumentsAdaptor,               // Represents an ArgumentsAdaptorFrame.
  kConstructStub,                  // Represents a ConstructStubFrame.
  kBuiltinContinuation,            // Represents a continuation to a stub.
  kJavaScriptBuiltinContinuation,  // Represents a continuation to a JavaScipt
                                   // builtin.
  kJavaScriptBuiltinContinuationWithCatch  // Represents a continuation to a
                                           // JavaScipt builtin with a catch
                                           // handler.
};

class FrameStateFunctionInfo {
 public:
  FrameStateFunctionInfo(FrameStateType type, int parameter_count,
                         int local_count,
                         Handle<SharedFunctionInfo> shared_info)
      : type_(type),
        parameter_count_(parameter_count),
        local_count_(local_count),
        shared_info_(shared_info) {}

  int local_count() const { return local_count_; }
  int parameter_count() const { return parameter_count_; }
  Handle<SharedFunctionInfo> shared_info() const { return shared_info_; }
  FrameStateType type() const { return type_; }

  static bool IsJSFunctionType(FrameStateType type) {
    return type == FrameStateType::kInterpretedFunction ||
           type == FrameStateType::kJavaScriptBuiltinContinuation ||
           type == FrameStateType::kJavaScriptBuiltinContinuationWithCatch;
  }

 private:
  FrameStateType const type_;
  int const parameter_count_;
  int const local_count_;
  Handle<SharedFunctionInfo> const shared_info_;
};


class FrameStateInfo final {
 public:
  FrameStateInfo(BailoutId bailout_id, OutputFrameStateCombine state_combine,
                 const FrameStateFunctionInfo* info)
      : bailout_id_(bailout_id),
        frame_state_combine_(state_combine),
        info_(info) {}

  FrameStateType type() const {
    return info_ == nullptr ? FrameStateType::kInterpretedFunction
                            : info_->type();
  }
  BailoutId bailout_id() const { return bailout_id_; }
  OutputFrameStateCombine state_combine() const { return frame_state_combine_; }
  MaybeHandle<SharedFunctionInfo> shared_info() const {
    return info_ == nullptr ? MaybeHandle<SharedFunctionInfo>()
                            : info_->shared_info();
  }
  int parameter_count() const {
    return info_ == nullptr ? 0 : info_->parameter_count();
  }
  int local_count() const {
    return info_ == nullptr ? 0 : info_->local_count();
  }
  const FrameStateFunctionInfo* function_info() const { return info_; }

 private:
  BailoutId const bailout_id_;
  OutputFrameStateCombine const frame_state_combine_;
  const FrameStateFunctionInfo* const info_;
};

bool operator==(FrameStateInfo const&, FrameStateInfo const&);
bool operator!=(FrameStateInfo const&, FrameStateInfo const&);

size_t hash_value(FrameStateInfo const&);

std::ostream& operator<<(std::ostream&, FrameStateInfo const&);

static const int kFrameStateParametersInput = 0;
static const int kFrameStateLocalsInput = 1;
static const int kFrameStateStackInput = 2;
static const int kFrameStateContextInput = 3;
static const int kFrameStateFunctionInput = 4;
static const int kFrameStateOuterStateInput = 5;
static const int kFrameStateInputCount = kFrameStateOuterStateInput + 1;

enum class ContinuationFrameStateMode { EAGER, LAZY, LAZY_WITH_CATCH };

Node* CreateStubBuiltinContinuationFrameState(
    JSGraph* graph, Builtins::Name name, Node* context, Node* const* parameters,
    int parameter_count, Node* outer_frame_state,
    ContinuationFrameStateMode mode);

Node* CreateJavaScriptBuiltinContinuationFrameState(
    JSGraph* graph, const SharedFunctionInfoRef& shared, Builtins::Name name,
    Node* target, Node* context, Node* const* stack_parameters,
    int stack_parameter_count, Node* outer_frame_state,
    ContinuationFrameStateMode mode);

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_FRAME_STATES_H_
