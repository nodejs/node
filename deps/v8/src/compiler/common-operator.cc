// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/common-operator.h"

#include "src/assembler.h"
#include "src/base/lazy-instance.h"
#include "src/compiler/linkage.h"
#include "src/compiler/opcodes.h"
#include "src/compiler/operator.h"
#include "src/unique.h"
#include "src/zone.h"

namespace v8 {
namespace internal {
namespace compiler {

std::ostream& operator<<(std::ostream& os, BranchHint hint) {
  switch (hint) {
    case BranchHint::kNone:
      return os << "None";
    case BranchHint::kTrue:
      return os << "True";
    case BranchHint::kFalse:
      return os << "False";
  }
  UNREACHABLE();
  return os;
}


BranchHint BranchHintOf(const Operator* const op) {
  DCHECK_EQ(IrOpcode::kBranch, op->opcode());
  return OpParameter<BranchHint>(op);
}


bool operator==(SelectParameters const& lhs, SelectParameters const& rhs) {
  return lhs.type() == rhs.type() && lhs.hint() == rhs.hint();
}


bool operator!=(SelectParameters const& lhs, SelectParameters const& rhs) {
  return !(lhs == rhs);
}


size_t hash_value(SelectParameters const& p) {
  return base::hash_combine(p.type(), p.hint());
}


std::ostream& operator<<(std::ostream& os, SelectParameters const& p) {
  return os << p.type() << "|" << p.hint();
}


SelectParameters const& SelectParametersOf(const Operator* const op) {
  DCHECK_EQ(IrOpcode::kSelect, op->opcode());
  return OpParameter<SelectParameters>(op);
}


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


#define CACHED_OP_LIST(V)                     \
  V(Dead, Operator::kFoldable, 0, 0, 0, 1)    \
  V(End, Operator::kFoldable, 0, 0, 1, 0)     \
  V(IfTrue, Operator::kFoldable, 0, 0, 1, 1)  \
  V(IfFalse, Operator::kFoldable, 0, 0, 1, 1) \
  V(Throw, Operator::kFoldable, 1, 1, 1, 1)   \
  V(Return, Operator::kNoProperties, 1, 1, 1, 1)


#define CACHED_LOOP_LIST(V) \
  V(1)                      \
  V(2)


#define CACHED_MERGE_LIST(V) \
  V(1)                       \
  V(2)                       \
  V(3)                       \
  V(4)                       \
  V(5)                       \
  V(6)                       \
  V(7)                       \
  V(8)


#define CACHED_PARAMETER_LIST(V) \
  V(0)                           \
  V(1)                           \
  V(2)                           \
  V(3)                           \
  V(4)                           \
  V(5)                           \
  V(6)


struct CommonOperatorGlobalCache FINAL {
#define CACHED(Name, properties, value_input_count, effect_input_count,     \
               control_input_count, control_output_count)                   \
  struct Name##Operator FINAL : public Operator {                           \
    Name##Operator()                                                        \
        : Operator(IrOpcode::k##Name, properties, #Name, value_input_count, \
                   effect_input_count, control_input_count, 0, 0,           \
                   control_output_count) {}                                 \
  };                                                                        \
  Name##Operator k##Name##Operator;
  CACHED_OP_LIST(CACHED)
#undef CACHED

  template <BranchHint kBranchHint>
  struct BranchOperator FINAL : public Operator1<BranchHint> {
    BranchOperator()
        : Operator1<BranchHint>(                       // --
              IrOpcode::kBranch, Operator::kFoldable,  // opcode
              "Branch",                                // name
              1, 0, 1, 0, 0, 2,                        // counts
              kBranchHint) {}                          // parameter
  };
  BranchOperator<BranchHint::kNone> kBranchNoneOperator;
  BranchOperator<BranchHint::kTrue> kBranchTrueOperator;
  BranchOperator<BranchHint::kFalse> kBranchFalseOperator;

  template <size_t kInputCount>
  struct LoopOperator FINAL : public Operator {
    LoopOperator()
        : Operator(                                  // --
              IrOpcode::kLoop, Operator::kFoldable,  // opcode
              "Loop",                                // name
              0, 0, kInputCount, 0, 0, 1) {}         // counts
  };
#define CACHED_LOOP(input_count) \
  LoopOperator<input_count> kLoop##input_count##Operator;
  CACHED_LOOP_LIST(CACHED_LOOP)
#undef CACHED_LOOP

  template <size_t kInputCount>
  struct MergeOperator FINAL : public Operator {
    MergeOperator()
        : Operator(                                   // --
              IrOpcode::kMerge, Operator::kFoldable,  // opcode
              "Merge",                                // name
              0, 0, kInputCount, 0, 0, 1) {}          // counts
  };
#define CACHED_MERGE(input_count) \
  MergeOperator<input_count> kMerge##input_count##Operator;
  CACHED_MERGE_LIST(CACHED_MERGE)
#undef CACHED_MERGE

  template <int kIndex>
  struct ParameterOperator FINAL : public Operator1<int> {
    ParameterOperator()
        : Operator1<int>(                             // --
              IrOpcode::kParameter, Operator::kPure,  // opcode
              "Parameter",                            // name
              1, 0, 0, 1, 0, 0,                       // counts,
              kIndex) {}                              // parameter
  };
#define CACHED_PARAMETER(index) \
  ParameterOperator<index> kParameter##index##Operator;
  CACHED_PARAMETER_LIST(CACHED_PARAMETER)
#undef CACHED_PARAMETER
};


static base::LazyInstance<CommonOperatorGlobalCache>::type kCache =
    LAZY_INSTANCE_INITIALIZER;


CommonOperatorBuilder::CommonOperatorBuilder(Zone* zone)
    : cache_(kCache.Get()), zone_(zone) {}


#define CACHED(Name, properties, value_input_count, effect_input_count, \
               control_input_count, control_output_count)               \
  const Operator* CommonOperatorBuilder::Name() {                       \
    return &cache_.k##Name##Operator;                                   \
  }
CACHED_OP_LIST(CACHED)
#undef CACHED


const Operator* CommonOperatorBuilder::Branch(BranchHint hint) {
  switch (hint) {
    case BranchHint::kNone:
      return &cache_.kBranchNoneOperator;
    case BranchHint::kTrue:
      return &cache_.kBranchTrueOperator;
    case BranchHint::kFalse:
      return &cache_.kBranchFalseOperator;
  }
  UNREACHABLE();
  return nullptr;
}


const Operator* CommonOperatorBuilder::Start(int num_formal_parameters) {
  // Outputs are formal parameters, plus context, receiver, and JSFunction.
  const int value_output_count = num_formal_parameters + 3;
  return new (zone()) Operator(               // --
      IrOpcode::kStart, Operator::kFoldable,  // opcode
      "Start",                                // name
      0, 0, 0, value_output_count, 1, 1);     // counts
}


const Operator* CommonOperatorBuilder::Loop(int control_input_count) {
  switch (control_input_count) {
#define CACHED_LOOP(input_count) \
  case input_count:              \
    return &cache_.kLoop##input_count##Operator;
    CACHED_LOOP_LIST(CACHED_LOOP)
#undef CACHED_LOOP
    default:
      break;
  }
  // Uncached.
  return new (zone()) Operator(              // --
      IrOpcode::kLoop, Operator::kFoldable,  // opcode
      "Loop",                                // name
      0, 0, control_input_count, 0, 0, 1);   // counts
}


const Operator* CommonOperatorBuilder::Merge(int control_input_count) {
  switch (control_input_count) {
#define CACHED_MERGE(input_count) \
  case input_count:               \
    return &cache_.kMerge##input_count##Operator;
    CACHED_MERGE_LIST(CACHED_MERGE)
#undef CACHED_MERGE
    default:
      break;
  }
  // Uncached.
  return new (zone()) Operator(               // --
      IrOpcode::kMerge, Operator::kFoldable,  // opcode
      "Merge",                                // name
      0, 0, control_input_count, 0, 0, 1);    // counts
}


const Operator* CommonOperatorBuilder::Terminate(int effects) {
  return new (zone()) Operator(               // --
      IrOpcode::kTerminate, Operator::kPure,  // opcode
      "Terminate",                            // name
      0, effects, 1, 0, 0, 1);                // counts
}


const Operator* CommonOperatorBuilder::Parameter(int index) {
  switch (index) {
#define CACHED_PARAMETER(index) \
  case index:                   \
    return &cache_.kParameter##index##Operator;
    CACHED_PARAMETER_LIST(CACHED_PARAMETER)
#undef CACHED_PARAMETER
    default:
      break;
  }
  // Uncached.
  return new (zone()) Operator1<int>(         // --
      IrOpcode::kParameter, Operator::kPure,  // opcode
      "Parameter",                            // name
      1, 0, 0, 1, 0, 0,                       // counts
      index);                                 // parameter
}


const Operator* CommonOperatorBuilder::Int32Constant(int32_t value) {
  return new (zone()) Operator1<int32_t>(         // --
      IrOpcode::kInt32Constant, Operator::kPure,  // opcode
      "Int32Constant",                            // name
      0, 0, 0, 1, 0, 0,                           // counts
      value);                                     // parameter
}


const Operator* CommonOperatorBuilder::Int64Constant(int64_t value) {
  return new (zone()) Operator1<int64_t>(         // --
      IrOpcode::kInt64Constant, Operator::kPure,  // opcode
      "Int64Constant",                            // name
      0, 0, 0, 1, 0, 0,                           // counts
      value);                                     // parameter
}


const Operator* CommonOperatorBuilder::Float32Constant(volatile float value) {
  return new (zone())
      Operator1<float, base::bit_equal_to<float>, base::bit_hash<float>>(  // --
          IrOpcode::kFloat32Constant, Operator::kPure,  // opcode
          "Float32Constant",                            // name
          0, 0, 0, 1, 0, 0,                             // counts
          value);                                       // parameter
}


const Operator* CommonOperatorBuilder::Float64Constant(volatile double value) {
  return new (zone()) Operator1<double, base::bit_equal_to<double>,
                                base::bit_hash<double>>(  // --
      IrOpcode::kFloat64Constant, Operator::kPure,        // opcode
      "Float64Constant",                                  // name
      0, 0, 0, 1, 0, 0,                                   // counts
      value);                                             // parameter
}


const Operator* CommonOperatorBuilder::ExternalConstant(
    const ExternalReference& value) {
  return new (zone()) Operator1<ExternalReference>(  // --
      IrOpcode::kExternalConstant, Operator::kPure,  // opcode
      "ExternalConstant",                            // name
      0, 0, 0, 1, 0, 0,                              // counts
      value);                                        // parameter
}


const Operator* CommonOperatorBuilder::NumberConstant(volatile double value) {
  return new (zone()) Operator1<double, base::bit_equal_to<double>,
                                base::bit_hash<double>>(  // --
      IrOpcode::kNumberConstant, Operator::kPure,         // opcode
      "NumberConstant",                                   // name
      0, 0, 0, 1, 0, 0,                                   // counts
      value);                                             // parameter
}


const Operator* CommonOperatorBuilder::HeapConstant(
    const Unique<HeapObject>& value) {
  return new (zone()) Operator1<Unique<HeapObject>>(  // --
      IrOpcode::kHeapConstant, Operator::kPure,       // opcode
      "HeapConstant",                                 // name
      0, 0, 0, 1, 0, 0,                               // counts
      value);                                         // parameter
}


const Operator* CommonOperatorBuilder::Select(MachineType type,
                                              BranchHint hint) {
  return new (zone()) Operator1<SelectParameters>(  // --
      IrOpcode::kSelect, Operator::kPure,           // opcode
      "Select",                                     // name
      3, 0, 0, 1, 0, 0,                             // counts
      SelectParameters(type, hint));                // parameter
}


const Operator* CommonOperatorBuilder::Phi(MachineType type, int arguments) {
  DCHECK(arguments > 0);                       // Disallow empty phis.
  return new (zone()) Operator1<MachineType>(  // --
      IrOpcode::kPhi, Operator::kPure,         // opcode
      "Phi",                                   // name
      arguments, 0, 1, 1, 0, 0,                // counts
      type);                                   // parameter
}


const Operator* CommonOperatorBuilder::EffectPhi(int arguments) {
  DCHECK(arguments > 0);                      // Disallow empty phis.
  return new (zone()) Operator(               // --
      IrOpcode::kEffectPhi, Operator::kPure,  // opcode
      "EffectPhi",                            // name
      0, arguments, 1, 0, 1, 0);              // counts
}


const Operator* CommonOperatorBuilder::ValueEffect(int arguments) {
  DCHECK(arguments > 0);                        // Disallow empty value effects.
  return new (zone()) Operator(                 // --
      IrOpcode::kValueEffect, Operator::kPure,  // opcode
      "ValueEffect",                            // name
      arguments, 0, 0, 0, 1, 0);                // counts
}


const Operator* CommonOperatorBuilder::Finish(int arguments) {
  DCHECK(arguments > 0);                   // Disallow empty finishes.
  return new (zone()) Operator(            // --
      IrOpcode::kFinish, Operator::kPure,  // opcode
      "Finish",                            // name
      1, arguments, 0, 1, 0, 0);           // counts
}


const Operator* CommonOperatorBuilder::StateValues(int arguments) {
  return new (zone()) Operator(                 // --
      IrOpcode::kStateValues, Operator::kPure,  // opcode
      "StateValues",                            // name
      arguments, 0, 0, 1, 0, 0);                // counts
}


const Operator* CommonOperatorBuilder::FrameState(
    FrameStateType type, BailoutId bailout_id,
    OutputFrameStateCombine state_combine, MaybeHandle<JSFunction> jsfunction) {
  return new (zone()) Operator1<FrameStateCallInfo>(  // --
      IrOpcode::kFrameState, Operator::kPure,         // opcode
      "FrameState",                                   // name
      4, 0, 0, 1, 0, 0,                               // counts
      FrameStateCallInfo(type, bailout_id, state_combine, jsfunction));
}


const Operator* CommonOperatorBuilder::Call(const CallDescriptor* descriptor) {
  class CallOperator FINAL : public Operator1<const CallDescriptor*> {
   public:
    CallOperator(const CallDescriptor* descriptor, const char* mnemonic)
        : Operator1<const CallDescriptor*>(
              IrOpcode::kCall, descriptor->properties(), mnemonic,
              descriptor->InputCount() + descriptor->FrameStateCount(),
              Operator::ZeroIfPure(descriptor->properties()),
              Operator::ZeroIfPure(descriptor->properties()),
              descriptor->ReturnCount(),
              Operator::ZeroIfPure(descriptor->properties()), 0, descriptor) {}

    void PrintParameter(std::ostream& os) const OVERRIDE {
      os << "[" << *parameter() << "]";
    }
  };
  return new (zone()) CallOperator(descriptor, "Call");
}


const Operator* CommonOperatorBuilder::Projection(size_t index) {
  return new (zone()) Operator1<size_t>(       // --
      IrOpcode::kProjection, Operator::kPure,  // opcode
      "Projection",                            // name
      1, 0, 0, 1, 0, 0,                        // counts
      index);                                  // parameter
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
