// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_GRAPH_VERIFIER_H_
#define V8_MAGLEV_MAGLEV_GRAPH_VERIFIER_H_

#include "src/maglev/maglev-graph-labeller.h"
#include "src/maglev/maglev-ir.h"

namespace v8 {
namespace internal {
namespace maglev {

std::ostream& operator<<(std::ostream& os, const ValueRepresentation& repr) {
  switch (repr) {
    case ValueRepresentation::kTagged:
      os << "TaggedValue";
      break;
    case ValueRepresentation::kUntagged:
      os << "UntaggedValue";
      break;
  }
  return os;
}

class Graph;

// TODO(victorgomes): Currently it only verifies the inputs for all ValueNodes
// are expected to be tagged/untagged. Add more verification later.
class MaglevGraphVerifier {
 public:
  void PreProcessGraph(MaglevCompilationUnit* compilation_unit, Graph* graph) {
    if (compilation_unit->has_graph_labeller()) {
      graph_labeller_ = compilation_unit->graph_labeller();
    }
  }

  void PostProcessGraph(MaglevCompilationUnit*, Graph* graph) {}
  void PreProcessBasicBlock(MaglevCompilationUnit*, BasicBlock* block) {}

  void CheckValueInputIs(NodeBase* node, int i, ValueRepresentation repr) {
    ValueNode* input = node->input(i).node();
    if (input->value_representation() != repr) {
      std::ostringstream str;
      str << "Type representation error: node ";
      if (graph_labeller_) {
        str << "#" << graph_labeller_->NodeId(node) << " : ";
      }
      str << node->opcode() << " (input @" << i << " = " << input->opcode()
          << ") type " << input->value_representation() << " is not " << repr;
      FATAL("%s", str.str().c_str());
    }
  }

  void Process(NodeBase* node, const ProcessingState& state) {
    switch (node->opcode()) {
      case Opcode::kConstant:
      case Opcode::kSmiConstant:
      case Opcode::kInt32Constant:
      case Opcode::kRootConstant:
      case Opcode::kInitialValue:
      case Opcode::kRegisterInput:
      case Opcode::kGapMove:
      case Opcode::kDeopt:
      case Opcode::kJump:
      case Opcode::kJumpLoop:
        // No input.
        DCHECK_EQ(node->input_count(), 0);
        break;
      case Opcode::kGenericNegate:
      case Opcode::kGenericIncrement:
      case Opcode::kGenericDecrement:
      case Opcode::kCheckedSmiUntag:
      case Opcode::kLoadField:
      case Opcode::kLoadGlobal:
      // TODO(victorgomes): Can we check that the input is actually a map?
      case Opcode::kCheckMaps:
      // TODO(victorgomes): Can we check that the input is Boolean?
      case Opcode::kBranchIfTrue:
      case Opcode::kBranchIfToBooleanTrue:
      case Opcode::kReturn:
        // Generic tagged unary operations.
        DCHECK_EQ(node->input_count(), 1);
        CheckValueInputIs(node, 0, ValueRepresentation::kTagged);
        break;
      case Opcode::kCheckedSmiTag:
        // Untagged unary operations.
        CheckValueInputIs(node, 0, ValueRepresentation::kUntagged);
        break;
      case Opcode::kGenericAdd:
      case Opcode::kGenericSubtract:
      case Opcode::kGenericMultiply:
      case Opcode::kGenericDivide:
      case Opcode::kGenericModulus:
      case Opcode::kGenericExponentiate:
      case Opcode::kGenericBitwiseAnd:
      case Opcode::kGenericBitwiseOr:
      case Opcode::kGenericBitwiseXor:
      case Opcode::kGenericShiftLeft:
      case Opcode::kGenericShiftRight:
      case Opcode::kGenericShiftRightLogical:
      case Opcode::kGenericBitwiseNot:
      // TODO(victorgomes): Can we use the fact that these nodes return a
      // Boolean?
      case Opcode::kGenericEqual:
      case Opcode::kGenericStrictEqual:
      case Opcode::kGenericLessThan:
      case Opcode::kGenericLessThanOrEqual:
      case Opcode::kGenericGreaterThan:
      case Opcode::kGenericGreaterThanOrEqual:
      // TODO(victorgomes): Can we check that first input is an Object?
      case Opcode::kStoreField:
      case Opcode::kLoadNamedGeneric:
        // Generic tagged binary operations.
        DCHECK_EQ(node->input_count(), 2);
        CheckValueInputIs(node, 0, ValueRepresentation::kTagged);
        CheckValueInputIs(node, 1, ValueRepresentation::kTagged);
        break;
      case Opcode::kInt32AddWithOverflow:
        // Untagged binary operations.
        CheckValueInputIs(node, 0, ValueRepresentation::kUntagged);
        CheckValueInputIs(node, 1, ValueRepresentation::kUntagged);
        break;
      case Opcode::kCall:
      case Opcode::kPhi:
        // All inputs should be tagged.
        for (int i = 0; i < node->input_count(); i++) {
          CheckValueInputIs(node, i, ValueRepresentation::kTagged);
        }
        break;
    }
  }

 private:
  MaglevGraphLabeller* graph_labeller_ = nullptr;
};

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_GRAPH_VERIFIER_H_
