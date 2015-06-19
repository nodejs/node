// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/operator-properties.h"

#include "src/compiler/js-operator.h"
#include "src/compiler/linkage.h"
#include "src/compiler/opcodes.h"

namespace v8 {
namespace internal {
namespace compiler {

// static
bool OperatorProperties::HasContextInput(const Operator* op) {
  IrOpcode::Value opcode = static_cast<IrOpcode::Value>(op->opcode());
  return IrOpcode::IsJsOpcode(opcode);
}


// static
int OperatorProperties::GetFrameStateInputCount(const Operator* op) {
  switch (op->opcode()) {
    case IrOpcode::kFrameState:
      return 1;
    case IrOpcode::kJSCallRuntime: {
      const CallRuntimeParameters& p = CallRuntimeParametersOf(op);
      return Linkage::NeedsFrameState(p.id());
    }

    // Strict equality cannot lazily deoptimize.
    case IrOpcode::kJSStrictEqual:
    case IrOpcode::kJSStrictNotEqual:
      return 0;

    // Calls
    case IrOpcode::kJSCallFunction:
    case IrOpcode::kJSCallConstruct:

    // Compare operations
    case IrOpcode::kJSEqual:
    case IrOpcode::kJSGreaterThan:
    case IrOpcode::kJSGreaterThanOrEqual:
    case IrOpcode::kJSHasProperty:
    case IrOpcode::kJSInstanceOf:
    case IrOpcode::kJSLessThan:
    case IrOpcode::kJSLessThanOrEqual:
    case IrOpcode::kJSNotEqual:

    // Object operations
    case IrOpcode::kJSCreateLiteralArray:
    case IrOpcode::kJSCreateLiteralObject:

    // Context operations
    case IrOpcode::kJSCreateScriptContext:
    case IrOpcode::kJSCreateWithContext:

    // Conversions
    case IrOpcode::kJSToObject:
    case IrOpcode::kJSToNumber:
    case IrOpcode::kJSToName:

    // Misc operations
    case IrOpcode::kJSStackCheck:

    // Properties
    case IrOpcode::kJSLoadNamed:
    case IrOpcode::kJSStoreNamed:
    case IrOpcode::kJSLoadProperty:
    case IrOpcode::kJSDeleteProperty:
      return 1;

    // StoreProperty provides a second frame state just before
    // the operation. This is used to lazy-deoptimize a to-number
    // conversion for typed arrays.
    case IrOpcode::kJSStoreProperty:
      return 2;

    // Binary operators that can deopt in the middle the operation (e.g.,
    // as a result of lazy deopt in ToNumber conversion) need a second frame
    // state so that we can resume before the operation.
    case IrOpcode::kJSMultiply:
    case IrOpcode::kJSAdd:
    case IrOpcode::kJSBitwiseAnd:
    case IrOpcode::kJSBitwiseOr:
    case IrOpcode::kJSBitwiseXor:
    case IrOpcode::kJSDivide:
    case IrOpcode::kJSModulus:
    case IrOpcode::kJSShiftLeft:
    case IrOpcode::kJSShiftRight:
    case IrOpcode::kJSShiftRightLogical:
    case IrOpcode::kJSSubtract:
      return 2;

    default:
      return 0;
  }
}


// static
int OperatorProperties::GetTotalInputCount(const Operator* op) {
  return op->ValueInputCount() + GetContextInputCount(op) +
         GetFrameStateInputCount(op) + op->EffectInputCount() +
         op->ControlInputCount();
}


// static
bool OperatorProperties::IsBasicBlockBegin(const Operator* op) {
  Operator::Opcode const opcode = op->opcode();
  return opcode == IrOpcode::kStart || opcode == IrOpcode::kEnd ||
         opcode == IrOpcode::kDead || opcode == IrOpcode::kLoop ||
         opcode == IrOpcode::kMerge || opcode == IrOpcode::kIfTrue ||
         opcode == IrOpcode::kIfFalse || opcode == IrOpcode::kIfSuccess ||
         opcode == IrOpcode::kIfException || opcode == IrOpcode::kIfValue ||
         opcode == IrOpcode::kIfDefault;
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
