// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_FRAME_STATES_H_
#define V8_COMPILER_FRAME_STATES_H_

#include "src/handles.h"
#include "src/utils.h"

namespace v8 {
namespace internal {

// Forward declarations.
class SharedFunctionInfo;

namespace compiler {

// Flag that describes how to combine the current environment with
// the output of a node to obtain a framestate for lazy bailout.
class OutputFrameStateCombine {
 public:
  enum Kind {
    kPushOutput,  // Push the output on the expression stack.
    kPokeAt       // Poke at the given environment location,
                  // counting from the top of the stack.
  };

  static OutputFrameStateCombine Ignore() {
    return OutputFrameStateCombine(kPushOutput, 0);
  }
  static OutputFrameStateCombine Push(size_t count = 1) {
    return OutputFrameStateCombine(kPushOutput, count);
  }
  static OutputFrameStateCombine PokeAt(size_t index) {
    return OutputFrameStateCombine(kPokeAt, index);
  }

  Kind kind() const { return kind_; }
  size_t GetPushCount() const {
    DCHECK_EQ(kPushOutput, kind());
    return parameter_;
  }
  size_t GetOffsetToPokeAt() const {
    DCHECK_EQ(kPokeAt, kind());
    return parameter_;
  }

  bool IsOutputIgnored() const {
    return kind_ == kPushOutput && parameter_ == 0;
  }

  size_t ConsumedOutputCount() const {
    return kind_ == kPushOutput ? GetPushCount() : 1;
  }

  bool operator==(OutputFrameStateCombine const& other) const {
    return kind_ == other.kind_ && parameter_ == other.parameter_;
  }
  bool operator!=(OutputFrameStateCombine const& other) const {
    return !(*this == other);
  }

  friend size_t hash_value(OutputFrameStateCombine const&);
  friend std::ostream& operator<<(std::ostream&,
                                  OutputFrameStateCombine const&);

 private:
  OutputFrameStateCombine(Kind kind, size_t parameter)
      : kind_(kind), parameter_(parameter) {}

  Kind const kind_;
  size_t const parameter_;
};


// The type of stack frame that a FrameState node represents.
enum class FrameStateType {
  kJavaScriptFunction,   // Represents an unoptimized JavaScriptFrame.
  kInterpretedFunction,  // Represents an InterpretedFrame.
  kArgumentsAdaptor,     // Represents an ArgumentsAdaptorFrame.
  kTailCallerFunction,   // Represents a frame removed by tail call elimination.
  kConstructStub         // Represents a ConstructStubFrame.
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
    return type == FrameStateType::kJavaScriptFunction ||
           type == FrameStateType::kInterpretedFunction;
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
    return info_ == nullptr ? FrameStateType::kJavaScriptFunction
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

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_FRAME_STATES_H_
