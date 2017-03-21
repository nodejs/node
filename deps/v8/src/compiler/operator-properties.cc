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
bool OperatorProperties::HasFrameStateInput(const Operator* op) {
  switch (op->opcode()) {
    case IrOpcode::kCheckpoint:
    case IrOpcode::kFrameState:
      return true;
    case IrOpcode::kJSCallRuntime: {
      const CallRuntimeParameters& p = CallRuntimeParametersOf(op);
      return Linkage::NeedsFrameStateInput(p.id());
    }

    // Strict equality cannot lazily deoptimize.
    case IrOpcode::kJSStrictEqual:
    case IrOpcode::kJSStrictNotEqual:
      return false;

    // Binary operations
    case IrOpcode::kJSAdd:
    case IrOpcode::kJSSubtract:
    case IrOpcode::kJSMultiply:
    case IrOpcode::kJSDivide:
    case IrOpcode::kJSModulus:

    // Bitwise operations
    case IrOpcode::kJSBitwiseOr:
    case IrOpcode::kJSBitwiseXor:
    case IrOpcode::kJSBitwiseAnd:

    // Shift operations
    case IrOpcode::kJSShiftLeft:
    case IrOpcode::kJSShiftRight:
    case IrOpcode::kJSShiftRightLogical:

    // Compare operations
    case IrOpcode::kJSEqual:
    case IrOpcode::kJSNotEqual:
    case IrOpcode::kJSGreaterThan:
    case IrOpcode::kJSGreaterThanOrEqual:
    case IrOpcode::kJSLessThan:
    case IrOpcode::kJSLessThanOrEqual:
    case IrOpcode::kJSHasProperty:
    case IrOpcode::kJSInstanceOf:
    case IrOpcode::kJSOrdinaryHasInstance:

    // Object operations
    case IrOpcode::kJSCreate:
    case IrOpcode::kJSCreateArguments:
    case IrOpcode::kJSCreateArray:
    case IrOpcode::kJSCreateLiteralArray:
    case IrOpcode::kJSCreateLiteralObject:
    case IrOpcode::kJSCreateLiteralRegExp:

    // Property access operations
    case IrOpcode::kJSLoadNamed:
    case IrOpcode::kJSStoreNamed:
    case IrOpcode::kJSLoadProperty:
    case IrOpcode::kJSStoreProperty:
    case IrOpcode::kJSLoadGlobal:
    case IrOpcode::kJSStoreGlobal:
    case IrOpcode::kJSStoreDataPropertyInLiteral:
    case IrOpcode::kJSDeleteProperty:

    // Context operations
    case IrOpcode::kJSCreateScriptContext:

    // Conversions
    case IrOpcode::kJSToInteger:
    case IrOpcode::kJSToLength:
    case IrOpcode::kJSToName:
    case IrOpcode::kJSToNumber:
    case IrOpcode::kJSToObject:
    case IrOpcode::kJSToString:

    // Call operations
    case IrOpcode::kJSCallConstruct:
    case IrOpcode::kJSCallConstructWithSpread:
    case IrOpcode::kJSCallFunction:

    // Misc operations
    case IrOpcode::kJSConvertReceiver:
    case IrOpcode::kJSForInNext:
    case IrOpcode::kJSForInPrepare:
    case IrOpcode::kJSStackCheck:
    case IrOpcode::kJSGetSuperConstructor:
      return true;

    default:
      return false;
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
