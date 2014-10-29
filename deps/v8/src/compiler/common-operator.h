// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_COMMON_OPERATOR_H_
#define V8_COMPILER_COMMON_OPERATOR_H_

#include "src/compiler/machine-type.h"
#include "src/unique.h"

namespace v8 {
namespace internal {

// Forward declarations.
class ExternalReference;
class OStream;


namespace compiler {

// Forward declarations.
class CallDescriptor;
struct CommonOperatorBuilderImpl;
class Operator;


// Flag that describes how to combine the current environment with
// the output of a node to obtain a framestate for lazy bailout.
enum OutputFrameStateCombine {
  kPushOutput,   // Push the output on the expression stack.
  kIgnoreOutput  // Use the frame state as-is.
};


// The type of stack frame that a FrameState node represents.
enum FrameStateType {
  JS_FRAME,          // Represents an unoptimized JavaScriptFrame.
  ARGUMENTS_ADAPTOR  // Represents an ArgumentsAdaptorFrame.
};


class FrameStateCallInfo FINAL {
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


// Interface for building common operators that can be used at any level of IR,
// including JavaScript, mid-level, and low-level.
class CommonOperatorBuilder FINAL {
 public:
  explicit CommonOperatorBuilder(Zone* zone);

  const Operator* Dead();
  const Operator* End();
  const Operator* Branch();
  const Operator* IfTrue();
  const Operator* IfFalse();
  const Operator* Throw();
  const Operator* Return();

  const Operator* Start(int num_formal_parameters);
  const Operator* Merge(int controls);
  const Operator* Loop(int controls);
  const Operator* Parameter(int index);

  const Operator* Int32Constant(int32_t);
  const Operator* Int64Constant(int64_t);
  const Operator* Float32Constant(volatile float);
  const Operator* Float64Constant(volatile double);
  const Operator* ExternalConstant(const ExternalReference&);
  const Operator* NumberConstant(volatile double);
  const Operator* HeapConstant(const Unique<Object>&);

  const Operator* Phi(MachineType type, int arguments);
  const Operator* EffectPhi(int arguments);
  const Operator* ControlEffect();
  const Operator* ValueEffect(int arguments);
  const Operator* Finish(int arguments);
  const Operator* StateValues(int arguments);
  const Operator* FrameState(
      FrameStateType type, BailoutId bailout_id,
      OutputFrameStateCombine state_combine,
      MaybeHandle<JSFunction> jsfunction = MaybeHandle<JSFunction>());
  const Operator* Call(const CallDescriptor* descriptor);
  const Operator* Projection(size_t index);

 private:
  Zone* zone() const { return zone_; }

  const CommonOperatorBuilderImpl& impl_;
  Zone* const zone_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_COMMON_OPERATOR_H_
