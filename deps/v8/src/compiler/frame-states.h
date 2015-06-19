// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_FRAME_STATES_H_
#define V8_COMPILER_FRAME_STATES_H_

#include "src/handles-inl.h"  // TODO(everyone): Fix our inl.h crap
#include "src/objects-inl.h"  // TODO(everyone): Fix our inl.h crap
#include "src/utils.h"

namespace v8 {
namespace internal {
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
enum FrameStateType {
  JS_FRAME,          // Represents an unoptimized JavaScriptFrame.
  ARGUMENTS_ADAPTOR  // Represents an ArgumentsAdaptorFrame.
};


class FrameStateCallInfo final {
 public:
  FrameStateCallInfo(
      FrameStateType type, BailoutId bailout_id,
      OutputFrameStateCombine state_combine,
      MaybeHandle<JSFunction> jsfunction = MaybeHandle<JSFunction>())
      : type_(type),
        bailout_id_(bailout_id),
        frame_state_combine_(state_combine),
        jsfunction_(jsfunction) {}

  FrameStateType type() const { return type_; }
  BailoutId bailout_id() const { return bailout_id_; }
  OutputFrameStateCombine state_combine() const { return frame_state_combine_; }
  MaybeHandle<JSFunction> jsfunction() const { return jsfunction_; }

 private:
  FrameStateType type_;
  BailoutId bailout_id_;
  OutputFrameStateCombine frame_state_combine_;
  MaybeHandle<JSFunction> jsfunction_;
};

bool operator==(FrameStateCallInfo const&, FrameStateCallInfo const&);
bool operator!=(FrameStateCallInfo const&, FrameStateCallInfo const&);

size_t hash_value(FrameStateCallInfo const&);

std::ostream& operator<<(std::ostream&, FrameStateCallInfo const&);


}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_FRAME_STATES_H_
