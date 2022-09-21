// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_GRAPH_VERIFIER_H_
#define V8_MAGLEV_MAGLEV_GRAPH_VERIFIER_H_

#include "src/maglev/maglev-compilation-info.h"
#include "src/maglev/maglev-graph-labeller.h"
#include "src/maglev/maglev-ir.h"

namespace v8 {
namespace internal {
namespace maglev {

std::ostream& operator<<(std::ostream& os, const ValueRepresentation& repr) {
  switch (repr) {
    case ValueRepresentation::kTagged:
      os << "Tagged";
      break;
    case ValueRepresentation::kInt32:
      os << "Int32";
      break;
    case ValueRepresentation::kFloat64:
      os << "Float64";
      break;
  }
  return os;
}

namespace {
ValueRepresentation ToValueRepresentation(MachineType type) {
  switch (type.representation()) {
    case MachineRepresentation::kTagged:
    case MachineRepresentation::kTaggedSigned:
    case MachineRepresentation::kTaggedPointer:
      return ValueRepresentation::kTagged;
    case MachineRepresentation::kFloat64:
      return ValueRepresentation::kFloat64;
    default:
      return ValueRepresentation::kInt32;
  }
}
}  // namespace

class Graph;

// TODO(victorgomes): Currently it only verifies the inputs for all ValueNodes
// are expected to be tagged/untagged. Add more verification later.
class MaglevGraphVerifier {
 public:
  explicit MaglevGraphVerifier(MaglevCompilationInfo* compilation_info) {
    if (compilation_info->has_graph_labeller()) {
      graph_labeller_ = compilation_info->graph_labeller();
    }
  }

  void PreProcessGraph(Graph* graph) {}
  void PostProcessGraph(Graph* graph) {}
  void PreProcessBasicBlock(BasicBlock* block) {}

  void CheckValueInputIs(NodeBase* node, int i, ValueRepresentation expected) {
    ValueNode* input = node->input(i).node();
    ValueRepresentation got = input->properties().value_representation();
    if (got != expected) {
      std::ostringstream str;
      str << "Type representation error: node ";
      if (graph_labeller_) {
        str << "#" << graph_labeller_->NodeId(node) << " : ";
      }
      str << node->opcode() << " (input @" << i << " = " << input->opcode()
          << ") type " << got << " is not " << expected;
      FATAL("%s", str.str().c_str());
    }
  }

  void Process(NodeBase* node, const ProcessingState& state) {
    switch (node->opcode()) {
      case Opcode::kAbort:
      case Opcode::kConstant:
      case Opcode::kConstantGapMove:
      case Opcode::kCreateEmptyArrayLiteral:
      case Opcode::kCreateEmptyObjectLiteral:
      case Opcode::kCreateArrayLiteral:
      case Opcode::kCreateShallowArrayLiteral:
      case Opcode::kCreateObjectLiteral:
      case Opcode::kCreateShallowObjectLiteral:
      case Opcode::kCreateRegExpLiteral:
      case Opcode::kDeopt:
      case Opcode::kFloat64Constant:
      case Opcode::kGapMove:
      case Opcode::kGetSecondReturnedValue:
      case Opcode::kInitialValue:
      case Opcode::kInt32Constant:
      case Opcode::kJump:
      case Opcode::kJumpFromInlined:
      case Opcode::kJumpLoop:
      case Opcode::kJumpLoopPrologue:
      case Opcode::kJumpToInlined:
      case Opcode::kRegisterInput:
      case Opcode::kRootConstant:
      case Opcode::kSmiConstant:
      case Opcode::kIncreaseInterruptBudget:
      case Opcode::kReduceInterruptBudget:
        // No input.
        DCHECK_EQ(node->input_count(), 0);
        break;
      case Opcode::kCheckedSmiUntag:
      case Opcode::kGenericBitwiseNot:
      case Opcode::kGenericDecrement:
      case Opcode::kGenericIncrement:
      case Opcode::kGenericNegate:
      case Opcode::kLoadDoubleField:
      case Opcode::kLoadGlobal:
      case Opcode::kLoadTaggedField:
      // TODO(victorgomes): Can we check that the input is actually a receiver?
      case Opcode::kCheckHeapObject:
      case Opcode::kCheckMaps:
      case Opcode::kCheckMapsWithMigration:
      case Opcode::kCheckSmi:
      case Opcode::kCheckNumber:
      case Opcode::kCheckString:
      case Opcode::kCheckSymbol:
      case Opcode::kCheckedInternalizedString:
      // TODO(victorgomes): Can we check that the input is Boolean?
      case Opcode::kBranchIfToBooleanTrue:
      case Opcode::kBranchIfRootConstant:
      case Opcode::kBranchIfUndefinedOrNull:
      case Opcode::kBranchIfJSReceiver:
      case Opcode::kCheckedFloat64Unbox:
      case Opcode::kCreateFunctionContext:
      case Opcode::kCreateClosure:
      case Opcode::kFastCreateClosure:
      case Opcode::kGeneratorRestoreRegister:
      case Opcode::kGetTemplateObject:
      case Opcode::kLogicalNot:
      case Opcode::kSetPendingMessage:
      case Opcode::kToBooleanLogicalNot:
      case Opcode::kTestUndetectable:
      case Opcode::kTestTypeOf:
      case Opcode::kThrowReferenceErrorIfHole:
      case Opcode::kThrowSuperNotCalledIfHole:
      case Opcode::kThrowSuperAlreadyCalledIfNotHole:
      case Opcode::kReturn:
        DCHECK_EQ(node->input_count(), 1);
        CheckValueInputIs(node, 0, ValueRepresentation::kTagged);
        break;
      case Opcode::kSwitch:
      case Opcode::kCheckedSmiTag:
      case Opcode::kChangeInt32ToFloat64:
        DCHECK_EQ(node->input_count(), 1);
        CheckValueInputIs(node, 0, ValueRepresentation::kInt32);
        break;
      case Opcode::kFloat64Box:
        DCHECK_EQ(node->input_count(), 1);
        CheckValueInputIs(node, 0, ValueRepresentation::kFloat64);
        break;
      case Opcode::kForInPrepare:
      case Opcode::kGenericAdd:
      case Opcode::kGenericBitwiseAnd:
      case Opcode::kGenericBitwiseOr:
      case Opcode::kGenericBitwiseXor:
      case Opcode::kGenericDivide:
      case Opcode::kGenericExponentiate:
      case Opcode::kGenericModulus:
      case Opcode::kGenericMultiply:
      case Opcode::kGenericShiftLeft:
      case Opcode::kGenericShiftRight:
      case Opcode::kGenericShiftRightLogical:
      case Opcode::kGenericSubtract:
      // TODO(victorgomes): Can we use the fact that these nodes return a
      // Boolean?
      case Opcode::kGenericEqual:
      case Opcode::kGenericGreaterThan:
      case Opcode::kGenericGreaterThanOrEqual:
      case Opcode::kGenericLessThan:
      case Opcode::kGenericLessThanOrEqual:
      case Opcode::kGenericStrictEqual:
      case Opcode::kCheckJSArrayBounds:
      case Opcode::kCheckJSObjectElementsBounds:
      case Opcode::kLoadTaggedElement:
      case Opcode::kLoadDoubleElement:
      case Opcode::kGetIterator:
      case Opcode::kTaggedEqual:
      case Opcode::kTaggedNotEqual:
      case Opcode::kStoreGlobal:
      // TODO(victorgomes): Can we check that first input is an Object?
      case Opcode::kStoreTaggedFieldNoWriteBarrier:
      // TODO(victorgomes): Can we check that second input is a Smi?
      case Opcode::kStoreTaggedFieldWithWriteBarrier:
      case Opcode::kLoadNamedGeneric:
      case Opcode::kThrowIfNotSuperConstructor:
      case Opcode::kToName:
      case Opcode::kToNumberOrNumeric:
      case Opcode::kToObject:
      case Opcode::kToString:
        DCHECK_EQ(node->input_count(), 2);
        CheckValueInputIs(node, 0, ValueRepresentation::kTagged);
        CheckValueInputIs(node, 1, ValueRepresentation::kTagged);
        break;
      case Opcode::kDeleteProperty:
      case Opcode::kLoadNamedFromSuperGeneric:
      case Opcode::kSetNamedGeneric:
      case Opcode::kDefineNamedOwnGeneric:
      case Opcode::kGetKeyedGeneric:
      case Opcode::kTestInstanceOf:
        DCHECK_EQ(node->input_count(), 3);
        CheckValueInputIs(node, 0, ValueRepresentation::kTagged);
        CheckValueInputIs(node, 1, ValueRepresentation::kTagged);
        CheckValueInputIs(node, 2, ValueRepresentation::kTagged);
        break;
      case Opcode::kSetKeyedGeneric:
      case Opcode::kDefineKeyedOwnGeneric:
      case Opcode::kStoreInArrayLiteralGeneric:
        DCHECK_EQ(node->input_count(), 4);
        CheckValueInputIs(node, 0, ValueRepresentation::kTagged);
        CheckValueInputIs(node, 1, ValueRepresentation::kTagged);
        CheckValueInputIs(node, 2, ValueRepresentation::kTagged);
        CheckValueInputIs(node, 3, ValueRepresentation::kTagged);
        break;
      case Opcode::kAssertInt32:
      case Opcode::kInt32AddWithOverflow:
      case Opcode::kInt32SubtractWithOverflow:
      case Opcode::kInt32MultiplyWithOverflow:
      case Opcode::kInt32DivideWithOverflow:
      // case Opcode::kInt32ExponentiateWithOverflow:
      // case Opcode::kInt32ModulusWithOverflow:
      case Opcode::kInt32BitwiseAnd:
      case Opcode::kInt32BitwiseOr:
      case Opcode::kInt32BitwiseXor:
      case Opcode::kInt32ShiftLeft:
      case Opcode::kInt32ShiftRight:
      case Opcode::kInt32ShiftRightLogical:
      case Opcode::kInt32Equal:
      case Opcode::kInt32StrictEqual:
      case Opcode::kInt32LessThan:
      case Opcode::kInt32LessThanOrEqual:
      case Opcode::kInt32GreaterThan:
      case Opcode::kInt32GreaterThanOrEqual:
      case Opcode::kBranchIfInt32Compare:
        DCHECK_EQ(node->input_count(), 2);
        CheckValueInputIs(node, 0, ValueRepresentation::kInt32);
        CheckValueInputIs(node, 1, ValueRepresentation::kInt32);
        break;
      case Opcode::kBranchIfReferenceCompare:
        DCHECK_EQ(node->input_count(), 2);
        CheckValueInputIs(node, 0, ValueRepresentation::kTagged);
        CheckValueInputIs(node, 1, ValueRepresentation::kTagged);
        break;
      case Opcode::kFloat64Add:
      case Opcode::kFloat64Subtract:
      case Opcode::kFloat64Multiply:
      case Opcode::kFloat64Divide:
      case Opcode::kFloat64Equal:
      case Opcode::kFloat64StrictEqual:
      case Opcode::kFloat64LessThan:
      case Opcode::kFloat64LessThanOrEqual:
      case Opcode::kFloat64GreaterThan:
      case Opcode::kFloat64GreaterThanOrEqual:
      case Opcode::kBranchIfFloat64Compare:
        DCHECK_EQ(node->input_count(), 2);
        CheckValueInputIs(node, 0, ValueRepresentation::kFloat64);
        CheckValueInputIs(node, 1, ValueRepresentation::kFloat64);
        break;
      case Opcode::kCall:
      case Opcode::kCallRuntime:
      case Opcode::kCallWithSpread:
      case Opcode::kConstruct:
      case Opcode::kConstructWithSpread:
      case Opcode::kGeneratorStore:
      case Opcode::kForInNext:
      case Opcode::kPhi:
        // All inputs should be tagged.
        for (int i = 0; i < node->input_count(); i++) {
          CheckValueInputIs(node, i, ValueRepresentation::kTagged);
        }
        break;
      case Opcode::kCallBuiltin: {
        CallBuiltin* call_builtin = node->Cast<CallBuiltin>();
        auto descriptor =
            Builtins::CallInterfaceDescriptorFor(call_builtin->builtin());
        int count = call_builtin->input_count();
        // Verify context.
        if (descriptor.HasContextParameter()) {
          CheckValueInputIs(call_builtin, count - 1,
                            ValueRepresentation::kTagged);
          count--;
        }

// {all_input_count} includes the feedback slot and vector.
#ifdef DEBUG
        int all_input_count = count + (call_builtin->has_feedback() ? 2 : 0);
        if (descriptor.AllowVarArgs()) {
          DCHECK_GE(all_input_count, descriptor.GetParameterCount());
        } else {
          DCHECK_EQ(all_input_count, descriptor.GetParameterCount());
        }
#endif
        int i = 0;
        // Check the rest of inputs.
        for (; i < count; ++i) {
          MachineType type = i < descriptor.GetParameterCount()
                                 ? descriptor.GetParameterType(i)
                                 : MachineType::AnyTagged();
          CheckValueInputIs(call_builtin, i, ToValueRepresentation(type));
        }
        break;
      }
    }
  }

 private:
  MaglevGraphLabeller* graph_labeller_ = nullptr;
};

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_GRAPH_VERIFIER_H_
