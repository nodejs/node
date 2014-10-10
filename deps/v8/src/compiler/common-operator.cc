// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/common-operator.h"

#include "src/assembler.h"
#include "src/base/lazy-instance.h"
#include "src/compiler/linkage.h"
#include "src/unique.h"
#include "src/zone.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {

// TODO(turbofan): Use size_t instead of int here.
class ControlOperator : public Operator1<int> {
 public:
  ControlOperator(IrOpcode::Value opcode, Properties properties, int inputs,
                  int outputs, int controls, const char* mnemonic)
      : Operator1<int>(opcode, properties, inputs, outputs, mnemonic,
                       controls) {}

  virtual OStream& PrintParameter(OStream& os) const FINAL { return os; }
};

}  // namespace


// Specialization for static parameters of type {ExternalReference}.
template <>
struct StaticParameterTraits<ExternalReference> {
  static OStream& PrintTo(OStream& os, ExternalReference reference) {
    os << reference.address();
    // TODO(bmeurer): Move to operator<<(os, ExternalReference)
    const Runtime::Function* function =
        Runtime::FunctionForEntry(reference.address());
    if (function) {
      os << " <" << function->name << ".entry>";
    }
    return os;
  }
  static int HashCode(ExternalReference reference) {
    return bit_cast<int>(static_cast<uint32_t>(
        reinterpret_cast<uintptr_t>(reference.address())));
  }
  static bool Equals(ExternalReference lhs, ExternalReference rhs) {
    return lhs == rhs;
  }
};


#define SHARED_OP_LIST(V)               \
  V(Dead, Operator::kFoldable, 0, 0)    \
  V(End, Operator::kFoldable, 0, 1)     \
  V(Branch, Operator::kFoldable, 1, 1)  \
  V(IfTrue, Operator::kFoldable, 0, 1)  \
  V(IfFalse, Operator::kFoldable, 0, 1) \
  V(Throw, Operator::kFoldable, 1, 1)   \
  V(Return, Operator::kNoProperties, 1, 1)


struct CommonOperatorBuilderImpl FINAL {
#define SHARED(Name, properties, value_input_count, control_input_count)       \
  struct Name##Operator FINAL : public ControlOperator {                       \
    Name##Operator()                                                           \
        : ControlOperator(IrOpcode::k##Name, properties, value_input_count, 0, \
                          control_input_count, #Name) {}                       \
  };                                                                           \
  Name##Operator k##Name##Operator;
  SHARED_OP_LIST(SHARED)
#undef SHARED

  struct ControlEffectOperator FINAL : public SimpleOperator {
    ControlEffectOperator()
        : SimpleOperator(IrOpcode::kControlEffect, Operator::kPure, 0, 0,
                         "ControlEffect") {}
  };
  ControlEffectOperator kControlEffectOperator;
};


static base::LazyInstance<CommonOperatorBuilderImpl>::type kImpl =
    LAZY_INSTANCE_INITIALIZER;


CommonOperatorBuilder::CommonOperatorBuilder(Zone* zone)
    : impl_(kImpl.Get()), zone_(zone) {}


#define SHARED(Name, properties, value_input_count, control_input_count) \
  const Operator* CommonOperatorBuilder::Name() {                        \
    return &impl_.k##Name##Operator;                                     \
  }
SHARED_OP_LIST(SHARED)
#undef SHARED


const Operator* CommonOperatorBuilder::Start(int num_formal_parameters) {
  // Outputs are formal parameters, plus context, receiver, and JSFunction.
  const int value_output_count = num_formal_parameters + 3;
  return new (zone()) ControlOperator(IrOpcode::kStart, Operator::kFoldable, 0,
                                      value_output_count, 0, "Start");
}


const Operator* CommonOperatorBuilder::Merge(int controls) {
  return new (zone()) ControlOperator(IrOpcode::kMerge, Operator::kFoldable, 0,
                                      0, controls, "Merge");
}


const Operator* CommonOperatorBuilder::Loop(int controls) {
  return new (zone()) ControlOperator(IrOpcode::kLoop, Operator::kFoldable, 0,
                                      0, controls, "Loop");
}


const Operator* CommonOperatorBuilder::Parameter(int index) {
  return new (zone()) Operator1<int>(IrOpcode::kParameter, Operator::kPure, 1,
                                     1, "Parameter", index);
}


const Operator* CommonOperatorBuilder::Int32Constant(int32_t value) {
  return new (zone()) Operator1<int32_t>(
      IrOpcode::kInt32Constant, Operator::kPure, 0, 1, "Int32Constant", value);
}


const Operator* CommonOperatorBuilder::Int64Constant(int64_t value) {
  return new (zone()) Operator1<int64_t>(
      IrOpcode::kInt64Constant, Operator::kPure, 0, 1, "Int64Constant", value);
}


const Operator* CommonOperatorBuilder::Float32Constant(volatile float value) {
  return new (zone())
      Operator1<float>(IrOpcode::kFloat32Constant, Operator::kPure, 0, 1,
                       "Float32Constant", value);
}


const Operator* CommonOperatorBuilder::Float64Constant(volatile double value) {
  return new (zone())
      Operator1<double>(IrOpcode::kFloat64Constant, Operator::kPure, 0, 1,
                        "Float64Constant", value);
}


const Operator* CommonOperatorBuilder::ExternalConstant(
    const ExternalReference& value) {
  return new (zone())
      Operator1<ExternalReference>(IrOpcode::kExternalConstant, Operator::kPure,
                                   0, 1, "ExternalConstant", value);
}


const Operator* CommonOperatorBuilder::NumberConstant(volatile double value) {
  return new (zone())
      Operator1<double>(IrOpcode::kNumberConstant, Operator::kPure, 0, 1,
                        "NumberConstant", value);
}


const Operator* CommonOperatorBuilder::HeapConstant(
    const Unique<Object>& value) {
  return new (zone()) Operator1<Unique<Object> >(
      IrOpcode::kHeapConstant, Operator::kPure, 0, 1, "HeapConstant", value);
}


const Operator* CommonOperatorBuilder::Phi(MachineType type, int arguments) {
  DCHECK(arguments > 0);  // Disallow empty phis.
  return new (zone()) Operator1<MachineType>(IrOpcode::kPhi, Operator::kPure,
                                             arguments, 1, "Phi", type);
}


const Operator* CommonOperatorBuilder::EffectPhi(int arguments) {
  DCHECK(arguments > 0);  // Disallow empty phis.
  return new (zone()) Operator1<int>(IrOpcode::kEffectPhi, Operator::kPure, 0,
                                     0, "EffectPhi", arguments);
}


const Operator* CommonOperatorBuilder::ControlEffect() {
  return &impl_.kControlEffectOperator;
}


const Operator* CommonOperatorBuilder::ValueEffect(int arguments) {
  DCHECK(arguments > 0);  // Disallow empty value effects.
  return new (zone()) SimpleOperator(IrOpcode::kValueEffect, Operator::kPure,
                                     arguments, 0, "ValueEffect");
}


const Operator* CommonOperatorBuilder::Finish(int arguments) {
  DCHECK(arguments > 0);  // Disallow empty finishes.
  return new (zone()) Operator1<int>(IrOpcode::kFinish, Operator::kPure, 1, 1,
                                     "Finish", arguments);
}


const Operator* CommonOperatorBuilder::StateValues(int arguments) {
  return new (zone()) Operator1<int>(IrOpcode::kStateValues, Operator::kPure,
                                     arguments, 1, "StateValues", arguments);
}


const Operator* CommonOperatorBuilder::FrameState(
    FrameStateType type, BailoutId bailout_id,
    OutputFrameStateCombine state_combine, MaybeHandle<JSFunction> jsfunction) {
  return new (zone()) Operator1<FrameStateCallInfo>(
      IrOpcode::kFrameState, Operator::kPure, 4, 1, "FrameState",
      FrameStateCallInfo(type, bailout_id, state_combine, jsfunction));
}


const Operator* CommonOperatorBuilder::Call(const CallDescriptor* descriptor) {
  class CallOperator FINAL : public Operator1<const CallDescriptor*> {
   public:
    // TODO(titzer): Operator still uses int, whereas CallDescriptor uses
    // size_t.
    CallOperator(const CallDescriptor* descriptor, const char* mnemonic)
        : Operator1<const CallDescriptor*>(
              IrOpcode::kCall, descriptor->properties(),
              static_cast<int>(descriptor->InputCount() +
                               descriptor->FrameStateCount()),
              static_cast<int>(descriptor->ReturnCount()), mnemonic,
              descriptor) {}

    virtual OStream& PrintParameter(OStream& os) const OVERRIDE {
      return os << "[" << *parameter() << "]";
    }
  };
  return new (zone()) CallOperator(descriptor, "Call");
}


const Operator* CommonOperatorBuilder::Projection(size_t index) {
  return new (zone()) Operator1<size_t>(IrOpcode::kProjection, Operator::kPure,
                                        1, 1, "Projection", index);
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
