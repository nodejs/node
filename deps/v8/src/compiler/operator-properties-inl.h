// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_OPERATOR_PROPERTIES_INL_H_
#define V8_COMPILER_OPERATOR_PROPERTIES_INL_H_

#include "src/compiler/common-operator.h"
#include "src/compiler/js-operator.h"
#include "src/compiler/opcodes.h"
#include "src/compiler/operator-properties.h"

namespace v8 {
namespace internal {
namespace compiler {

inline bool OperatorProperties::HasValueInput(const Operator* op) {
  return OperatorProperties::GetValueInputCount(op) > 0;
}

inline bool OperatorProperties::HasContextInput(const Operator* op) {
  IrOpcode::Value opcode = static_cast<IrOpcode::Value>(op->opcode());
  return IrOpcode::IsJsOpcode(opcode);
}

inline bool OperatorProperties::HasEffectInput(const Operator* op) {
  return OperatorProperties::GetEffectInputCount(op) > 0;
}

inline bool OperatorProperties::HasControlInput(const Operator* op) {
  return OperatorProperties::GetControlInputCount(op) > 0;
}

inline bool OperatorProperties::HasFrameStateInput(const Operator* op) {
  if (!FLAG_turbo_deoptimization) {
    return false;
  }

  switch (op->opcode()) {
    case IrOpcode::kFrameState:
      return true;
    case IrOpcode::kJSCallRuntime: {
      Runtime::FunctionId function = OpParameter<Runtime::FunctionId>(op);
      return Linkage::NeedsFrameState(function);
    }

    // Strict equality cannot lazily deoptimize.
    case IrOpcode::kJSStrictEqual:
    case IrOpcode::kJSStrictNotEqual:
      return false;

    // Calls
    case IrOpcode::kJSCallFunction:
    case IrOpcode::kJSCallConstruct:

    // Compare operations
    case IrOpcode::kJSEqual:
    case IrOpcode::kJSNotEqual:
    case IrOpcode::kJSLessThan:
    case IrOpcode::kJSGreaterThan:
    case IrOpcode::kJSLessThanOrEqual:
    case IrOpcode::kJSGreaterThanOrEqual:

    // Binary operations
    case IrOpcode::kJSBitwiseOr:
    case IrOpcode::kJSBitwiseXor:
    case IrOpcode::kJSBitwiseAnd:
    case IrOpcode::kJSShiftLeft:
    case IrOpcode::kJSShiftRight:
    case IrOpcode::kJSShiftRightLogical:
    case IrOpcode::kJSAdd:
    case IrOpcode::kJSSubtract:
    case IrOpcode::kJSMultiply:
    case IrOpcode::kJSDivide:
    case IrOpcode::kJSModulus:
    case IrOpcode::kJSLoadProperty:
    case IrOpcode::kJSStoreProperty:
    case IrOpcode::kJSLoadNamed:
    case IrOpcode::kJSStoreNamed:
      return true;

    default:
      return false;
  }
}

inline int OperatorProperties::GetValueInputCount(const Operator* op) {
  return op->InputCount();
}

inline int OperatorProperties::GetContextInputCount(const Operator* op) {
  return OperatorProperties::HasContextInput(op) ? 1 : 0;
}

inline int OperatorProperties::GetFrameStateInputCount(const Operator* op) {
  return OperatorProperties::HasFrameStateInput(op) ? 1 : 0;
}

inline int OperatorProperties::GetEffectInputCount(const Operator* op) {
  if (op->opcode() == IrOpcode::kEffectPhi ||
      op->opcode() == IrOpcode::kFinish) {
    return OpParameter<int>(op);
  }
  if (op->HasProperty(Operator::kNoRead) && op->HasProperty(Operator::kNoWrite))
    return 0;  // no effects.
  return 1;
}

inline int OperatorProperties::GetControlInputCount(const Operator* op) {
  switch (op->opcode()) {
    case IrOpcode::kPhi:
    case IrOpcode::kEffectPhi:
    case IrOpcode::kControlEffect:
      return 1;
#define OPCODE_CASE(x) case IrOpcode::k##x:
      CONTROL_OP_LIST(OPCODE_CASE)
#undef OPCODE_CASE
      // Control operators are Operator1<int>.
      return OpParameter<int>(op);
    default:
      // Operators that have write effects must have a control
      // dependency. Effect dependencies only ensure the correct order of
      // write/read operations without consideration of control flow. Without an
      // explicit control dependency writes can be float in the schedule too
      // early along a path that shouldn't generate a side-effect.
      return op->HasProperty(Operator::kNoWrite) ? 0 : 1;
  }
  return 0;
}

inline int OperatorProperties::GetTotalInputCount(const Operator* op) {
  return GetValueInputCount(op) + GetContextInputCount(op) +
         GetFrameStateInputCount(op) + GetEffectInputCount(op) +
         GetControlInputCount(op);
}

// -----------------------------------------------------------------------------
// Output properties.

inline bool OperatorProperties::HasValueOutput(const Operator* op) {
  return GetValueOutputCount(op) > 0;
}

inline bool OperatorProperties::HasEffectOutput(const Operator* op) {
  return op->opcode() == IrOpcode::kStart ||
         op->opcode() == IrOpcode::kControlEffect ||
         op->opcode() == IrOpcode::kValueEffect ||
         (op->opcode() != IrOpcode::kFinish && GetEffectInputCount(op) > 0);
}

inline bool OperatorProperties::HasControlOutput(const Operator* op) {
  IrOpcode::Value opcode = static_cast<IrOpcode::Value>(op->opcode());
  return (opcode != IrOpcode::kEnd && IrOpcode::IsControlOpcode(opcode));
}


inline int OperatorProperties::GetValueOutputCount(const Operator* op) {
  return op->OutputCount();
}

inline int OperatorProperties::GetEffectOutputCount(const Operator* op) {
  return HasEffectOutput(op) ? 1 : 0;
}

inline int OperatorProperties::GetControlOutputCount(const Operator* node) {
  return node->opcode() == IrOpcode::kBranch ? 2 : HasControlOutput(node) ? 1
                                                                          : 0;
}


inline bool OperatorProperties::IsBasicBlockBegin(const Operator* op) {
  uint8_t opcode = op->opcode();
  return opcode == IrOpcode::kStart || opcode == IrOpcode::kEnd ||
         opcode == IrOpcode::kDead || opcode == IrOpcode::kLoop ||
         opcode == IrOpcode::kMerge || opcode == IrOpcode::kIfTrue ||
         opcode == IrOpcode::kIfFalse;
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_OPERATOR_PROPERTIES_INL_H_
