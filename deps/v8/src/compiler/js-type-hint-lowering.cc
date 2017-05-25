// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-type-hint-lowering.h"

#include "src/compiler/js-graph.h"
#include "src/compiler/operator-properties.h"
#include "src/compiler/simplified-operator.h"
#include "src/feedback-vector.h"
#include "src/type-hints.h"

namespace v8 {
namespace internal {
namespace compiler {

class JSSpeculativeBinopBuilder final {
 public:
  JSSpeculativeBinopBuilder(JSTypeHintLowering* lowering, const Operator* op,
                            Node* left, Node* right, Node* effect,
                            Node* control, FeedbackSlot slot)
      : lowering_(lowering),
        op_(op),
        left_(left),
        right_(right),
        effect_(effect),
        control_(control),
        slot_(slot) {}

  BinaryOperationHint GetBinaryOperationHint() {
    DCHECK_EQ(FeedbackSlotKind::kBinaryOp, feedback_vector()->GetKind(slot_));
    BinaryOpICNexus nexus(feedback_vector(), slot_);
    return nexus.GetBinaryOperationFeedback();
  }

  bool GetBinaryNumberOperationHint(NumberOperationHint* hint) {
    switch (GetBinaryOperationHint()) {
      case BinaryOperationHint::kSignedSmall:
        *hint = NumberOperationHint::kSignedSmall;
        return true;
      case BinaryOperationHint::kSigned32:
        *hint = NumberOperationHint::kSigned32;
        return true;
      case BinaryOperationHint::kNumberOrOddball:
        *hint = NumberOperationHint::kNumberOrOddball;
        return true;
      case BinaryOperationHint::kAny:
      case BinaryOperationHint::kNone:
      case BinaryOperationHint::kString:
        break;
    }
    return false;
  }

  const Operator* SpeculativeNumberOp(NumberOperationHint hint) {
    switch (op_->opcode()) {
      case IrOpcode::kJSAdd:
        return simplified()->SpeculativeNumberAdd(hint);
      case IrOpcode::kJSSubtract:
        return simplified()->SpeculativeNumberSubtract(hint);
      case IrOpcode::kJSMultiply:
        return simplified()->SpeculativeNumberMultiply(hint);
      case IrOpcode::kJSDivide:
        return simplified()->SpeculativeNumberDivide(hint);
      case IrOpcode::kJSModulus:
        return simplified()->SpeculativeNumberModulus(hint);
      case IrOpcode::kJSBitwiseAnd:
        return simplified()->SpeculativeNumberBitwiseAnd(hint);
      case IrOpcode::kJSBitwiseOr:
        return simplified()->SpeculativeNumberBitwiseOr(hint);
      case IrOpcode::kJSBitwiseXor:
        return simplified()->SpeculativeNumberBitwiseXor(hint);
      case IrOpcode::kJSShiftLeft:
        return simplified()->SpeculativeNumberShiftLeft(hint);
      case IrOpcode::kJSShiftRight:
        return simplified()->SpeculativeNumberShiftRight(hint);
      case IrOpcode::kJSShiftRightLogical:
        return simplified()->SpeculativeNumberShiftRightLogical(hint);
      default:
        break;
    }
    UNREACHABLE();
    return nullptr;
  }

  Node* BuildSpeculativeOperator(const Operator* op) {
    DCHECK_EQ(2, op->ValueInputCount());
    DCHECK_EQ(1, op->EffectInputCount());
    DCHECK_EQ(1, op->ControlInputCount());
    DCHECK_EQ(false, OperatorProperties::HasFrameStateInput(op));
    DCHECK_EQ(false, OperatorProperties::HasContextInput(op));
    DCHECK_EQ(1, op->EffectOutputCount());
    DCHECK_EQ(0, op->ControlOutputCount());
    return graph()->NewNode(op, left_, right_, effect_, control_);
  }

  JSGraph* jsgraph() const { return lowering_->jsgraph(); }
  Graph* graph() const { return jsgraph()->graph(); }
  JSOperatorBuilder* javascript() { return jsgraph()->javascript(); }
  SimplifiedOperatorBuilder* simplified() { return jsgraph()->simplified(); }
  CommonOperatorBuilder* common() { return jsgraph()->common(); }
  const Handle<FeedbackVector>& feedback_vector() const {
    return lowering_->feedback_vector();
  }

 private:
  JSTypeHintLowering* lowering_;
  const Operator* op_;
  Node* left_;
  Node* right_;
  Node* effect_;
  Node* control_;
  FeedbackSlot slot_;
};

JSTypeHintLowering::JSTypeHintLowering(JSGraph* jsgraph,
                                       Handle<FeedbackVector> feedback_vector)
    : jsgraph_(jsgraph), feedback_vector_(feedback_vector) {}

Reduction JSTypeHintLowering::ReduceBinaryOperation(const Operator* op,
                                                    Node* left, Node* right,
                                                    Node* effect, Node* control,
                                                    FeedbackSlot slot) {
  switch (op->opcode()) {
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
    case IrOpcode::kJSModulus: {
      JSSpeculativeBinopBuilder b(this, op, left, right, effect, control, slot);
      NumberOperationHint hint;
      if (b.GetBinaryNumberOperationHint(&hint)) {
        Node* node = b.BuildSpeculativeOperator(b.SpeculativeNumberOp(hint));
        return Reduction(node);
      }
      break;
    }
    default:
      UNREACHABLE();
      break;
  }
  return Reduction();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
