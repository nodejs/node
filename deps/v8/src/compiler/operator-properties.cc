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
      return Linkage::FrameStateInputCount(p.id());
    }

    // Strict equality cannot lazily deoptimize.
    case IrOpcode::kJSStrictEqual:
    case IrOpcode::kJSStrictNotEqual:
      return 0;

    // We record the frame state immediately before and immediately after every
    // construct/function call.
    case IrOpcode::kJSCallConstruct:
    case IrOpcode::kJSCallFunction:
      return 2;

    // Compare operations
    case IrOpcode::kJSEqual:
    case IrOpcode::kJSNotEqual:
    case IrOpcode::kJSHasProperty:
    case IrOpcode::kJSInstanceOf:

    // Object operations
    case IrOpcode::kJSCreate:
    case IrOpcode::kJSCreateArguments:
    case IrOpcode::kJSCreateArray:
    case IrOpcode::kJSCreateLiteralArray:
    case IrOpcode::kJSCreateLiteralObject:
    case IrOpcode::kJSCreateLiteralRegExp:

    // Context operations
    case IrOpcode::kJSCreateScriptContext:

    // Conversions
    case IrOpcode::kJSToName:
    case IrOpcode::kJSToNumber:
    case IrOpcode::kJSToObject:
    case IrOpcode::kJSToString:

    // Misc operations
    case IrOpcode::kJSConvertReceiver:
    case IrOpcode::kJSForInNext:
    case IrOpcode::kJSForInPrepare:
    case IrOpcode::kJSStackCheck:
    case IrOpcode::kJSDeleteProperty:
      return 1;

    // We record the frame state immediately before and immediately after
    // every property or global variable access.
    case IrOpcode::kJSLoadNamed:
    case IrOpcode::kJSStoreNamed:
    case IrOpcode::kJSLoadProperty:
    case IrOpcode::kJSStoreProperty:
    case IrOpcode::kJSLoadGlobal:
    case IrOpcode::kJSStoreGlobal:
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

    // Compare operators that can deopt in the middle the operation (e.g.,
    // as a result of lazy deopt in ToNumber conversion) need a second frame
    // state so that we can resume before the operation.
    case IrOpcode::kJSGreaterThan:
    case IrOpcode::kJSGreaterThanOrEqual:
    case IrOpcode::kJSLessThan:
    case IrOpcode::kJSLessThanOrEqual:
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
