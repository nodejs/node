// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/code-factory.h"
#include "src/compilation-dependencies.h"
#include "src/compiler/access-builder.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/js-typed-lowering.h"
#include "src/compiler/linkage.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/operator-properties.h"
#include "src/type-cache.h"
#include "src/types.h"

namespace v8 {
namespace internal {
namespace compiler {

// A helper class to simplify the process of reducing a single binop node with a
// JSOperator. This class manages the rewriting of context, control, and effect
// dependencies during lowering of a binop and contains numerous helper
// functions for matching the types of inputs to an operation.
class JSBinopReduction final {
 public:
  JSBinopReduction(JSTypedLowering* lowering, Node* node)
      : lowering_(lowering), node_(node) {}

  void ConvertInputsToNumber(Node* frame_state) {
    // To convert the inputs to numbers, we have to provide frame states
    // for lazy bailouts in the ToNumber conversions.
    // We use a little hack here: we take the frame state before the binary
    // operation and use it to construct the frame states for the conversion
    // so that after the deoptimization, the binary operation IC gets
    // already converted values from full code. This way we are sure that we
    // will not re-do any of the side effects.

    Node* left_input = nullptr;
    Node* right_input = nullptr;
    bool left_is_primitive = left_type()->Is(Type::PlainPrimitive());
    bool right_is_primitive = right_type()->Is(Type::PlainPrimitive());
    bool handles_exception = NodeProperties::IsExceptionalCall(node_);

    if (!left_is_primitive && !right_is_primitive && handles_exception) {
      ConvertBothInputsToNumber(&left_input, &right_input, frame_state);
    } else {
      left_input = left_is_primitive
                       ? ConvertPlainPrimitiveToNumber(left())
                       : ConvertSingleInputToNumber(
                             left(), CreateFrameStateForLeftInput(frame_state));
      right_input = right_is_primitive
                        ? ConvertPlainPrimitiveToNumber(right())
                        : ConvertSingleInputToNumber(
                              right(), CreateFrameStateForRightInput(
                                           frame_state, left_input));
    }

    node_->ReplaceInput(0, left_input);
    node_->ReplaceInput(1, right_input);
  }

  void ConvertInputsToUI32(Signedness left_signedness,
                           Signedness right_signedness) {
    node_->ReplaceInput(0, ConvertToUI32(left(), left_signedness));
    node_->ReplaceInput(1, ConvertToUI32(right(), right_signedness));
  }

  void SwapInputs() {
    Node* l = left();
    Node* r = right();
    node_->ReplaceInput(0, r);
    node_->ReplaceInput(1, l);
  }

  // Remove all effect and control inputs and outputs to this node and change
  // to the pure operator {op}, possibly inserting a boolean inversion.
  Reduction ChangeToPureOperator(const Operator* op, bool invert = false,
                                 Type* type = Type::Any()) {
    DCHECK_EQ(0, op->EffectInputCount());
    DCHECK_EQ(false, OperatorProperties::HasContextInput(op));
    DCHECK_EQ(0, op->ControlInputCount());
    DCHECK_EQ(2, op->ValueInputCount());

    // Remove the effects from the node, and update its effect/control usages.
    if (node_->op()->EffectInputCount() > 0) {
      lowering_->RelaxEffectsAndControls(node_);
    }
    // Remove the inputs corresponding to context, effect, and control.
    NodeProperties::RemoveNonValueInputs(node_);
    // Finally, update the operator to the new one.
    NodeProperties::ChangeOp(node_, op);

    // TODO(jarin): Replace the explicit typing hack with a call to some method
    // that encapsulates changing the operator and re-typing.
    Type* node_type = NodeProperties::GetType(node_);
    NodeProperties::SetType(node_, Type::Intersect(node_type, type, zone()));

    if (invert) {
      // Insert an boolean not to invert the value.
      Node* value = graph()->NewNode(simplified()->BooleanNot(), node_);
      node_->ReplaceUses(value);
      // Note: ReplaceUses() smashes all uses, so smash it back here.
      value->ReplaceInput(0, node_);
      return lowering_->Replace(value);
    }
    return lowering_->Changed(node_);
  }

  Reduction ChangeToStringComparisonOperator(const Operator* op,
                                             bool invert = false) {
    if (node_->op()->ControlInputCount() > 0) {
      lowering_->RelaxControls(node_);
    }
    // String comparison operators need effect and control inputs, so copy them
    // over.
    Node* effect = NodeProperties::GetEffectInput(node_);
    Node* control = NodeProperties::GetControlInput(node_);
    node_->ReplaceInput(2, effect);
    node_->ReplaceInput(3, control);

    node_->TrimInputCount(4);
    NodeProperties::ChangeOp(node_, op);

    if (invert) {
      // Insert a boolean-not to invert the value.
      Node* value = graph()->NewNode(simplified()->BooleanNot(), node_);
      node_->ReplaceUses(value);
      // Note: ReplaceUses() smashes all uses, so smash it back here.
      value->ReplaceInput(0, node_);
      return lowering_->Replace(value);
    }
    return lowering_->Changed(node_);
  }

  Reduction ChangeToPureOperator(const Operator* op, Type* type) {
    return ChangeToPureOperator(op, false, type);
  }

  bool LeftInputIs(Type* t) { return left_type()->Is(t); }

  bool RightInputIs(Type* t) { return right_type()->Is(t); }

  bool OneInputIs(Type* t) { return LeftInputIs(t) || RightInputIs(t); }

  bool BothInputsAre(Type* t) { return LeftInputIs(t) && RightInputIs(t); }

  bool OneInputCannotBe(Type* t) {
    return !left_type()->Maybe(t) || !right_type()->Maybe(t);
  }

  bool NeitherInputCanBe(Type* t) {
    return !left_type()->Maybe(t) && !right_type()->Maybe(t);
  }

  Node* effect() { return NodeProperties::GetEffectInput(node_); }
  Node* control() { return NodeProperties::GetControlInput(node_); }
  Node* context() { return NodeProperties::GetContextInput(node_); }
  Node* left() { return NodeProperties::GetValueInput(node_, 0); }
  Node* right() { return NodeProperties::GetValueInput(node_, 1); }
  Type* left_type() { return NodeProperties::GetType(node_->InputAt(0)); }
  Type* right_type() { return NodeProperties::GetType(node_->InputAt(1)); }

  SimplifiedOperatorBuilder* simplified() { return lowering_->simplified(); }
  Graph* graph() const { return lowering_->graph(); }
  JSGraph* jsgraph() { return lowering_->jsgraph(); }
  JSOperatorBuilder* javascript() { return lowering_->javascript(); }
  MachineOperatorBuilder* machine() { return lowering_->machine(); }
  CommonOperatorBuilder* common() { return jsgraph()->common(); }
  Zone* zone() const { return graph()->zone(); }

 private:
  JSTypedLowering* lowering_;  // The containing lowering instance.
  Node* node_;                 // The original node.

  Node* CreateFrameStateForLeftInput(Node* frame_state) {
    FrameStateInfo state_info = OpParameter<FrameStateInfo>(frame_state);

    if (state_info.bailout_id() == BailoutId::None()) {
      // Dummy frame state => just leave it as is.
      return frame_state;
    }

    // If the frame state is already the right one, just return it.
    if (state_info.state_combine().kind() == OutputFrameStateCombine::kPokeAt &&
        state_info.state_combine().GetOffsetToPokeAt() == 1) {
      return frame_state;
    }

    // Here, we smash the result of the conversion into the slot just below
    // the stack top. This is the slot that full code uses to store the
    // left operand.
    const Operator* op = jsgraph()->common()->FrameState(
        state_info.bailout_id(), OutputFrameStateCombine::PokeAt(1),
        state_info.function_info());

    return graph()->NewNode(op,
                            frame_state->InputAt(kFrameStateParametersInput),
                            frame_state->InputAt(kFrameStateLocalsInput),
                            frame_state->InputAt(kFrameStateStackInput),
                            frame_state->InputAt(kFrameStateContextInput),
                            frame_state->InputAt(kFrameStateFunctionInput),
                            frame_state->InputAt(kFrameStateOuterStateInput));
  }

  Node* CreateFrameStateForRightInput(Node* frame_state, Node* converted_left) {
    FrameStateInfo state_info = OpParameter<FrameStateInfo>(frame_state);

    if (state_info.bailout_id() == BailoutId::None()) {
      // Dummy frame state => just leave it as is.
      return frame_state;
    }

    // Create a frame state that stores the result of the operation to the
    // top of the stack (i.e., the slot used for the right operand).
    const Operator* op = jsgraph()->common()->FrameState(
        state_info.bailout_id(), OutputFrameStateCombine::PokeAt(0),
        state_info.function_info());

    // Change the left operand {converted_left} on the expression stack.
    Node* stack = frame_state->InputAt(2);
    DCHECK_EQ(stack->opcode(), IrOpcode::kStateValues);
    DCHECK_GE(stack->InputCount(), 2);

    // TODO(jarin) Allocate in a local zone or a reusable buffer.
    NodeVector new_values(stack->InputCount(), zone());
    for (int i = 0; i < stack->InputCount(); i++) {
      if (i == stack->InputCount() - 2) {
        new_values[i] = converted_left;
      } else {
        new_values[i] = stack->InputAt(i);
      }
    }
    Node* new_stack =
        graph()->NewNode(stack->op(), stack->InputCount(), &new_values.front());

    return graph()->NewNode(
        op, frame_state->InputAt(kFrameStateParametersInput),
        frame_state->InputAt(kFrameStateLocalsInput), new_stack,
        frame_state->InputAt(kFrameStateContextInput),
        frame_state->InputAt(kFrameStateFunctionInput),
        frame_state->InputAt(kFrameStateOuterStateInput));
  }

  Node* ConvertPlainPrimitiveToNumber(Node* node) {
    DCHECK(NodeProperties::GetType(node)->Is(Type::PlainPrimitive()));
    // Avoid inserting too many eager ToNumber() operations.
    Reduction const reduction = lowering_->ReduceJSToNumberInput(node);
    if (reduction.Changed()) return reduction.replacement();
    // TODO(jarin) Use PlainPrimitiveToNumber once we have it.
    return graph()->NewNode(
        javascript()->ToNumber(), node, jsgraph()->NoContextConstant(),
        jsgraph()->EmptyFrameState(), graph()->start(), graph()->start());
  }

  Node* ConvertSingleInputToNumber(Node* node, Node* frame_state) {
    DCHECK(!NodeProperties::GetType(node)->Is(Type::PlainPrimitive()));
    Node* const n = graph()->NewNode(javascript()->ToNumber(), node, context(),
                                     frame_state, effect(), control());
    NodeProperties::ReplaceUses(node_, node_, node_, n, n);
    update_effect(n);
    return n;
  }

  void ConvertBothInputsToNumber(Node** left_result, Node** right_result,
                                 Node* frame_state) {
    Node* projections[2];

    // Find {IfSuccess} and {IfException} continuations of the operation.
    NodeProperties::CollectControlProjections(node_, projections, 2);
    IfExceptionHint hint = OpParameter<IfExceptionHint>(projections[1]);
    Node* if_exception = projections[1];
    Node* if_success = projections[0];

    // Insert two ToNumber() operations that both potentially throw.
    Node* left_state = CreateFrameStateForLeftInput(frame_state);
    Node* left_conv =
        graph()->NewNode(javascript()->ToNumber(), left(), context(),
                         left_state, effect(), control());
    Node* left_success = graph()->NewNode(common()->IfSuccess(), left_conv);
    Node* right_state = CreateFrameStateForRightInput(frame_state, left_conv);
    Node* right_conv =
        graph()->NewNode(javascript()->ToNumber(), right(), context(),
                         right_state, left_conv, left_success);
    Node* left_exception =
        graph()->NewNode(common()->IfException(hint), left_conv, left_conv);
    Node* right_exception =
        graph()->NewNode(common()->IfException(hint), right_conv, right_conv);
    NodeProperties::ReplaceControlInput(if_success, right_conv);
    update_effect(right_conv);

    // Wire conversions to existing {IfException} continuation.
    Node* exception_merge = if_exception;
    Node* exception_value =
        graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                         left_exception, right_exception, exception_merge);
    Node* exception_effect =
        graph()->NewNode(common()->EffectPhi(2), left_exception,
                         right_exception, exception_merge);
    for (Edge edge : exception_merge->use_edges()) {
      if (NodeProperties::IsEffectEdge(edge)) edge.UpdateTo(exception_effect);
      if (NodeProperties::IsValueEdge(edge)) edge.UpdateTo(exception_value);
    }
    NodeProperties::RemoveType(exception_merge);
    exception_merge->ReplaceInput(0, left_exception);
    exception_merge->ReplaceInput(1, right_exception);
    NodeProperties::ChangeOp(exception_merge, common()->Merge(2));

    *left_result = left_conv;
    *right_result = right_conv;
  }

  Node* ConvertToUI32(Node* node, Signedness signedness) {
    // Avoid introducing too many eager NumberToXXnt32() operations.
    Type* type = NodeProperties::GetType(node);
    if (signedness == kSigned) {
      if (!type->Is(Type::Signed32())) {
        node = graph()->NewNode(simplified()->NumberToInt32(), node);
      }
    } else {
      DCHECK_EQ(kUnsigned, signedness);
      if (!type->Is(Type::Unsigned32())) {
        node = graph()->NewNode(simplified()->NumberToUint32(), node);
      }
    }
    return node;
  }

  void update_effect(Node* effect) {
    NodeProperties::ReplaceEffectInput(node_, effect);
  }
};


// TODO(turbofan): js-typed-lowering improvements possible
// - immediately put in type bounds for all new nodes
// - relax effects from generic but not-side-effecting operations


JSTypedLowering::JSTypedLowering(Editor* editor,
                                 CompilationDependencies* dependencies,
                                 Flags flags, JSGraph* jsgraph, Zone* zone)
    : AdvancedReducer(editor),
      dependencies_(dependencies),
      flags_(flags),
      jsgraph_(jsgraph),
      true_type_(Type::Constant(factory()->true_value(), graph()->zone())),
      false_type_(Type::Constant(factory()->false_value(), graph()->zone())),
      the_hole_type_(
          Type::Constant(factory()->the_hole_value(), graph()->zone())),
      type_cache_(TypeCache::Get()) {
  for (size_t k = 0; k < arraysize(shifted_int32_ranges_); ++k) {
    double min = kMinInt / (1 << k);
    double max = kMaxInt / (1 << k);
    shifted_int32_ranges_[k] = Type::Range(min, max, graph()->zone());
  }
}


Reduction JSTypedLowering::ReduceJSAdd(Node* node) {
  if (flags() & kDisableBinaryOpReduction) return NoChange();

  JSBinopReduction r(this, node);
  if (r.BothInputsAre(Type::Number())) {
    // JSAdd(x:number, y:number) => NumberAdd(x, y)
    return r.ChangeToPureOperator(simplified()->NumberAdd(), Type::Number());
  }
  if (r.NeitherInputCanBe(Type::StringOrReceiver())) {
    // JSAdd(x:-string, y:-string) => NumberAdd(ToNumber(x), ToNumber(y))
    Node* frame_state = NodeProperties::GetFrameStateInput(node, 1);
    r.ConvertInputsToNumber(frame_state);
    return r.ChangeToPureOperator(simplified()->NumberAdd(), Type::Number());
  }
  if (r.BothInputsAre(Type::String())) {
    // JSAdd(x:string, y:string) => CallStub[StringAdd](x, y)
    Callable const callable =
        CodeFactory::StringAdd(isolate(), STRING_ADD_CHECK_NONE, NOT_TENURED);
    CallDescriptor const* const desc = Linkage::GetStubCallDescriptor(
        isolate(), graph()->zone(), callable.descriptor(), 0,
        CallDescriptor::kNeedsFrameState, node->op()->properties());
    DCHECK_EQ(2, OperatorProperties::GetFrameStateInputCount(node->op()));
    node->RemoveInput(NodeProperties::FirstFrameStateIndex(node) + 1);
    node->InsertInput(graph()->zone(), 0,
                      jsgraph()->HeapConstant(callable.code()));
    NodeProperties::ChangeOp(node, common()->Call(desc));
    return Changed(node);
  }
  return NoChange();
}


Reduction JSTypedLowering::ReduceJSModulus(Node* node) {
  if (flags() & kDisableBinaryOpReduction) return NoChange();

  JSBinopReduction r(this, node);
  if (r.BothInputsAre(Type::Number())) {
    // JSModulus(x:number, x:number) => NumberModulus(x, y)
    return r.ChangeToPureOperator(simplified()->NumberModulus(),
                                  Type::Number());
  }
  return NoChange();
}


Reduction JSTypedLowering::ReduceNumberBinop(Node* node,
                                             const Operator* numberOp) {
  if (flags() & kDisableBinaryOpReduction) return NoChange();

  JSBinopReduction r(this, node);
  if (numberOp == simplified()->NumberModulus()) {
    if (r.BothInputsAre(Type::Number())) {
      return r.ChangeToPureOperator(numberOp, Type::Number());
    }
    return NoChange();
  }
  Node* frame_state = NodeProperties::GetFrameStateInput(node, 1);
  r.ConvertInputsToNumber(frame_state);
  return r.ChangeToPureOperator(numberOp, Type::Number());
}


Reduction JSTypedLowering::ReduceInt32Binop(Node* node, const Operator* intOp) {
  if (flags() & kDisableBinaryOpReduction) return NoChange();

  JSBinopReduction r(this, node);
  Node* frame_state = NodeProperties::GetFrameStateInput(node, 1);
  r.ConvertInputsToNumber(frame_state);
  r.ConvertInputsToUI32(kSigned, kSigned);
  return r.ChangeToPureOperator(intOp, Type::Integral32());
}


Reduction JSTypedLowering::ReduceUI32Shift(Node* node,
                                           Signedness left_signedness,
                                           const Operator* shift_op) {
  if (flags() & kDisableBinaryOpReduction) return NoChange();

  JSBinopReduction r(this, node);
  Node* frame_state = NodeProperties::GetFrameStateInput(node, 1);
  r.ConvertInputsToNumber(frame_state);
  r.ConvertInputsToUI32(left_signedness, kUnsigned);
  return r.ChangeToPureOperator(shift_op);
}


Reduction JSTypedLowering::ReduceJSComparison(Node* node) {
  if (flags() & kDisableBinaryOpReduction) return NoChange();

  JSBinopReduction r(this, node);
  if (r.BothInputsAre(Type::String())) {
    // If both inputs are definitely strings, perform a string comparison.
    const Operator* stringOp;
    switch (node->opcode()) {
      case IrOpcode::kJSLessThan:
        stringOp = simplified()->StringLessThan();
        break;
      case IrOpcode::kJSGreaterThan:
        stringOp = simplified()->StringLessThan();
        r.SwapInputs();  // a > b => b < a
        break;
      case IrOpcode::kJSLessThanOrEqual:
        stringOp = simplified()->StringLessThanOrEqual();
        break;
      case IrOpcode::kJSGreaterThanOrEqual:
        stringOp = simplified()->StringLessThanOrEqual();
        r.SwapInputs();  // a >= b => b <= a
        break;
      default:
        return NoChange();
    }
    r.ChangeToStringComparisonOperator(stringOp);
    return Changed(node);
  }
  if (r.OneInputCannotBe(Type::StringOrReceiver())) {
    const Operator* less_than;
    const Operator* less_than_or_equal;
    if (r.BothInputsAre(Type::Unsigned32())) {
      less_than = machine()->Uint32LessThan();
      less_than_or_equal = machine()->Uint32LessThanOrEqual();
    } else if (r.BothInputsAre(Type::Signed32())) {
      less_than = machine()->Int32LessThan();
      less_than_or_equal = machine()->Int32LessThanOrEqual();
    } else {
      // TODO(turbofan): mixed signed/unsigned int32 comparisons.
      Node* frame_state = NodeProperties::GetFrameStateInput(node, 1);
      r.ConvertInputsToNumber(frame_state);
      less_than = simplified()->NumberLessThan();
      less_than_or_equal = simplified()->NumberLessThanOrEqual();
    }
    const Operator* comparison;
    switch (node->opcode()) {
      case IrOpcode::kJSLessThan:
        comparison = less_than;
        break;
      case IrOpcode::kJSGreaterThan:
        comparison = less_than;
        r.SwapInputs();  // a > b => b < a
        break;
      case IrOpcode::kJSLessThanOrEqual:
        comparison = less_than_or_equal;
        break;
      case IrOpcode::kJSGreaterThanOrEqual:
        comparison = less_than_or_equal;
        r.SwapInputs();  // a >= b => b <= a
        break;
      default:
        return NoChange();
    }
    return r.ChangeToPureOperator(comparison);
  }
  // TODO(turbofan): relax/remove effects of this operator in other cases.
  return NoChange();  // Keep a generic comparison.
}


Reduction JSTypedLowering::ReduceJSEqual(Node* node, bool invert) {
  if (flags() & kDisableBinaryOpReduction) return NoChange();

  JSBinopReduction r(this, node);

  if (r.BothInputsAre(Type::Number())) {
    return r.ChangeToPureOperator(simplified()->NumberEqual(), invert);
  }
  if (r.BothInputsAre(Type::String())) {
    return r.ChangeToStringComparisonOperator(simplified()->StringEqual(),
                                              invert);
  }
  if (r.BothInputsAre(Type::Boolean())) {
    return r.ChangeToPureOperator(simplified()->ReferenceEqual(Type::Boolean()),
                                  invert);
  }
  if (r.BothInputsAre(Type::Receiver())) {
    return r.ChangeToPureOperator(
        simplified()->ReferenceEqual(Type::Receiver()), invert);
  }
  if (r.OneInputIs(Type::NullOrUndefined())) {
    Callable const callable = CodeFactory::CompareNilIC(isolate(), kNullValue);
    CallDescriptor const* const desc = Linkage::GetStubCallDescriptor(
        isolate(), graph()->zone(), callable.descriptor(), 0,
        CallDescriptor::kNeedsFrameState, node->op()->properties());
    node->RemoveInput(r.LeftInputIs(Type::NullOrUndefined()) ? 0 : 1);
    node->InsertInput(graph()->zone(), 0,
                      jsgraph()->HeapConstant(callable.code()));
    NodeProperties::ChangeOp(node, common()->Call(desc));
    if (invert) {
      // Insert an boolean not to invert the value.
      Node* value = graph()->NewNode(simplified()->BooleanNot(), node);
      node->ReplaceUses(value);
      // Note: ReplaceUses() smashes all uses, so smash it back here.
      value->ReplaceInput(0, node);
      return Replace(value);
    }
    return Changed(node);
  }
  return NoChange();
}


Reduction JSTypedLowering::ReduceJSStrictEqual(Node* node, bool invert) {
  if (flags() & kDisableBinaryOpReduction) return NoChange();

  JSBinopReduction r(this, node);
  if (r.left() == r.right()) {
    // x === x is always true if x != NaN
    if (!r.left_type()->Maybe(Type::NaN())) {
      Node* replacement = jsgraph()->BooleanConstant(!invert);
      ReplaceWithValue(node, replacement);
      return Replace(replacement);
    }
  }
  if (r.OneInputCannotBe(Type::NumberOrString())) {
    // For values with canonical representation (i.e. not string nor number) an
    // empty type intersection means the values cannot be strictly equal.
    if (!r.left_type()->Maybe(r.right_type())) {
      Node* replacement = jsgraph()->BooleanConstant(invert);
      ReplaceWithValue(node, replacement);
      return Replace(replacement);
    }
  }
  if (r.OneInputIs(the_hole_type_)) {
    return r.ChangeToPureOperator(simplified()->ReferenceEqual(the_hole_type_),
                                  invert);
  }
  if (r.OneInputIs(Type::Undefined())) {
    return r.ChangeToPureOperator(
        simplified()->ReferenceEqual(Type::Undefined()), invert);
  }
  if (r.OneInputIs(Type::Null())) {
    return r.ChangeToPureOperator(simplified()->ReferenceEqual(Type::Null()),
                                  invert);
  }
  if (r.OneInputIs(Type::Boolean())) {
    return r.ChangeToPureOperator(simplified()->ReferenceEqual(Type::Boolean()),
                                  invert);
  }
  if (r.OneInputIs(Type::Object())) {
    return r.ChangeToPureOperator(simplified()->ReferenceEqual(Type::Object()),
                                  invert);
  }
  if (r.OneInputIs(Type::Receiver())) {
    return r.ChangeToPureOperator(
        simplified()->ReferenceEqual(Type::Receiver()), invert);
  }
  if (r.BothInputsAre(Type::Unique())) {
    return r.ChangeToPureOperator(simplified()->ReferenceEqual(Type::Unique()),
                                  invert);
  }
  if (r.BothInputsAre(Type::String())) {
    return r.ChangeToStringComparisonOperator(simplified()->StringEqual(),
                                              invert);
  }
  if (r.BothInputsAre(Type::Number())) {
    return r.ChangeToPureOperator(simplified()->NumberEqual(), invert);
  }
  // TODO(turbofan): js-typed-lowering of StrictEqual(mixed types)
  return NoChange();
}


Reduction JSTypedLowering::ReduceJSToBoolean(Node* node) {
  Node* const input = node->InputAt(0);
  Type* const input_type = NodeProperties::GetType(input);
  Node* const effect = NodeProperties::GetEffectInput(node);
  if (input_type->Is(Type::Boolean())) {
    // JSToBoolean(x:boolean) => x
    ReplaceWithValue(node, input, effect);
    return Replace(input);
  } else if (input_type->Is(Type::OrderedNumber())) {
    // JSToBoolean(x:ordered-number) => BooleanNot(NumberEqual(x,#0))
    RelaxEffectsAndControls(node);
    node->ReplaceInput(0, graph()->NewNode(simplified()->NumberEqual(), input,
                                           jsgraph()->ZeroConstant()));
    node->TrimInputCount(1);
    NodeProperties::ChangeOp(node, simplified()->BooleanNot());
    return Changed(node);
  } else if (input_type->Is(Type::String())) {
    // JSToBoolean(x:string) => NumberLessThan(#0,x.length)
    FieldAccess const access = AccessBuilder::ForStringLength();
    Node* length = graph()->NewNode(simplified()->LoadField(access), input,
                                    effect, graph()->start());
    ReplaceWithValue(node, node, length);
    node->ReplaceInput(0, jsgraph()->ZeroConstant());
    node->ReplaceInput(1, length);
    node->TrimInputCount(2);
    NodeProperties::ChangeOp(node, simplified()->NumberLessThan());
    return Changed(node);
  }
  return NoChange();
}


Reduction JSTypedLowering::ReduceJSToNumberInput(Node* input) {
  if (input->opcode() == IrOpcode::kJSToNumber) {
    // Recursively try to reduce the input first.
    Reduction result = ReduceJSToNumber(input);
    if (result.Changed()) return result;
    return Changed(input);  // JSToNumber(JSToNumber(x)) => JSToNumber(x)
  }
  // Check for ToNumber truncation of signaling NaN to undefined mapping.
  if (input->opcode() == IrOpcode::kSelect) {
    Node* check = NodeProperties::GetValueInput(input, 0);
    Node* vtrue = NodeProperties::GetValueInput(input, 1);
    Type* vtrue_type = NodeProperties::GetType(vtrue);
    Node* vfalse = NodeProperties::GetValueInput(input, 2);
    Type* vfalse_type = NodeProperties::GetType(vfalse);
    if (vtrue_type->Is(Type::Undefined()) && vfalse_type->Is(Type::Number())) {
      if (check->opcode() == IrOpcode::kNumberIsHoleNaN &&
          check->InputAt(0) == vfalse) {
        // JSToNumber(Select(NumberIsHoleNaN(x), y:undefined, x:number)) => x
        return Replace(vfalse);
      }
    }
  }
  // Try constant-folding of JSToNumber with constant inputs.
  Type* input_type = NodeProperties::GetType(input);
  if (input_type->IsConstant()) {
    Handle<Object> input_value = input_type->AsConstant()->Value();
    if (input_value->IsString()) {
      return Replace(jsgraph()->Constant(
          String::ToNumber(Handle<String>::cast(input_value))));
    } else if (input_value->IsOddball()) {
      return Replace(jsgraph()->Constant(
          Oddball::ToNumber(Handle<Oddball>::cast(input_value))));
    }
  }
  if (input_type->Is(Type::Number())) {
    // JSToNumber(x:number) => x
    return Changed(input);
  }
  if (input_type->Is(Type::Undefined())) {
    // JSToNumber(undefined) => #NaN
    return Replace(jsgraph()->NaNConstant());
  }
  if (input_type->Is(Type::Null())) {
    // JSToNumber(null) => #0
    return Replace(jsgraph()->ZeroConstant());
  }
  if (input_type->Is(Type::Boolean())) {
    // JSToNumber(x:boolean) => BooleanToNumber(x)
    return Replace(graph()->NewNode(simplified()->BooleanToNumber(), input));
  }
  // TODO(turbofan): js-typed-lowering of ToNumber(x:string)
  return NoChange();
}


Reduction JSTypedLowering::ReduceJSToNumber(Node* node) {
  // Try to reduce the input first.
  Node* const input = node->InputAt(0);
  Reduction reduction = ReduceJSToNumberInput(input);
  if (reduction.Changed()) {
    ReplaceWithValue(node, reduction.replacement());
    return reduction;
  }
  Type* const input_type = NodeProperties::GetType(input);
  if (input_type->Is(Type::PlainPrimitive())) {
    if (NodeProperties::GetContextInput(node) !=
            jsgraph()->NoContextConstant() ||
        NodeProperties::GetEffectInput(node) != graph()->start() ||
        NodeProperties::GetControlInput(node) != graph()->start()) {
      // JSToNumber(x:plain-primitive,context,effect,control)
      //   => JSToNumber(x,no-context,start,start)
      RelaxEffectsAndControls(node);
      NodeProperties::ReplaceContextInput(node, jsgraph()->NoContextConstant());
      NodeProperties::ReplaceControlInput(node, graph()->start());
      NodeProperties::ReplaceEffectInput(node, graph()->start());
      DCHECK_EQ(1, OperatorProperties::GetFrameStateInputCount(node->op()));
      NodeProperties::ReplaceFrameStateInput(node, 0,
                                             jsgraph()->EmptyFrameState());
      return Changed(node);
    }
  }
  return NoChange();
}


Reduction JSTypedLowering::ReduceJSToStringInput(Node* input) {
  if (input->opcode() == IrOpcode::kJSToString) {
    // Recursively try to reduce the input first.
    Reduction result = ReduceJSToString(input);
    if (result.Changed()) return result;
    return Changed(input);  // JSToString(JSToString(x)) => JSToString(x)
  }
  Type* input_type = NodeProperties::GetType(input);
  if (input_type->Is(Type::String())) {
    return Changed(input);  // JSToString(x:string) => x
  }
  if (input_type->Is(Type::Boolean())) {
    return Replace(graph()->NewNode(
        common()->Select(MachineRepresentation::kTagged), input,
        jsgraph()->HeapConstant(factory()->true_string()),
        jsgraph()->HeapConstant(factory()->false_string())));
  }
  if (input_type->Is(Type::Undefined())) {
    return Replace(jsgraph()->HeapConstant(factory()->undefined_string()));
  }
  if (input_type->Is(Type::Null())) {
    return Replace(jsgraph()->HeapConstant(factory()->null_string()));
  }
  // TODO(turbofan): js-typed-lowering of ToString(x:number)
  return NoChange();
}


Reduction JSTypedLowering::ReduceJSToString(Node* node) {
  // Try to reduce the input first.
  Node* const input = node->InputAt(0);
  Reduction reduction = ReduceJSToStringInput(input);
  if (reduction.Changed()) {
    ReplaceWithValue(node, reduction.replacement());
    return reduction;
  }
  return NoChange();
}


Reduction JSTypedLowering::ReduceJSToObject(Node* node) {
  DCHECK_EQ(IrOpcode::kJSToObject, node->opcode());
  Node* receiver = NodeProperties::GetValueInput(node, 0);
  Type* receiver_type = NodeProperties::GetType(receiver);
  Node* context = NodeProperties::GetContextInput(node);
  Node* frame_state = NodeProperties::GetFrameStateInput(node, 0);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  if (!receiver_type->Is(Type::Receiver())) {
    // TODO(bmeurer/mstarzinger): Add support for lowering inside try blocks.
    if (receiver_type->Maybe(Type::NullOrUndefined()) &&
        NodeProperties::IsExceptionalCall(node)) {
      // ToObject throws for null or undefined inputs.
      return NoChange();
    }

    // Check whether {receiver} is a Smi.
    Node* check0 = graph()->NewNode(simplified()->ObjectIsSmi(), receiver);
    Node* branch0 =
        graph()->NewNode(common()->Branch(BranchHint::kFalse), check0, control);
    Node* if_true0 = graph()->NewNode(common()->IfTrue(), branch0);
    Node* etrue0 = effect;

    Node* if_false0 = graph()->NewNode(common()->IfFalse(), branch0);
    Node* efalse0 = effect;

    // Determine the instance type of {receiver}.
    Node* receiver_map = efalse0 =
        graph()->NewNode(simplified()->LoadField(AccessBuilder::ForMap()),
                         receiver, efalse0, if_false0);
    Node* receiver_instance_type = efalse0 = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForMapInstanceType()),
        receiver_map, efalse0, if_false0);

    // Check whether {receiver} is a spec object.
    STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
    Node* check1 =
        graph()->NewNode(machine()->Uint32LessThanOrEqual(),
                         jsgraph()->Uint32Constant(FIRST_JS_RECEIVER_TYPE),
                         receiver_instance_type);
    Node* branch1 = graph()->NewNode(common()->Branch(BranchHint::kTrue),
                                     check1, if_false0);
    Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
    Node* etrue1 = efalse0;

    Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
    Node* efalse1 = efalse0;

    // Convert {receiver} using the ToObjectStub.
    Node* if_convert =
        graph()->NewNode(common()->Merge(2), if_true0, if_false1);
    Node* econvert =
        graph()->NewNode(common()->EffectPhi(2), etrue0, efalse1, if_convert);
    Node* rconvert;
    {
      Callable callable = CodeFactory::ToObject(isolate());
      CallDescriptor const* const desc = Linkage::GetStubCallDescriptor(
          isolate(), graph()->zone(), callable.descriptor(), 0,
          CallDescriptor::kNeedsFrameState, node->op()->properties());
      rconvert = econvert = graph()->NewNode(
          common()->Call(desc), jsgraph()->HeapConstant(callable.code()),
          receiver, context, frame_state, econvert, if_convert);
    }

    // The {receiver} is already a spec object.
    Node* if_done = if_true1;
    Node* edone = etrue1;
    Node* rdone = receiver;

    control = graph()->NewNode(common()->Merge(2), if_convert, if_done);
    effect = graph()->NewNode(common()->EffectPhi(2), econvert, edone, control);
    receiver =
        graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                         rconvert, rdone, control);
  }
  ReplaceWithValue(node, receiver, effect, control);
  return Changed(receiver);
}


Reduction JSTypedLowering::ReduceJSLoadNamed(Node* node) {
  DCHECK_EQ(IrOpcode::kJSLoadNamed, node->opcode());
  Node* receiver = NodeProperties::GetValueInput(node, 0);
  Type* receiver_type = NodeProperties::GetType(receiver);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Handle<Name> name = NamedAccessOf(node->op()).name();
  // Optimize "length" property of strings.
  if (name.is_identical_to(factory()->length_string()) &&
      receiver_type->Is(Type::String())) {
    Node* value = effect = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForStringLength()), receiver,
        effect, control);
    ReplaceWithValue(node, value, effect);
    return Replace(value);
  }
  // Optimize "prototype" property of functions.
  if (name.is_identical_to(factory()->prototype_string()) &&
      receiver_type->IsConstant() &&
      receiver_type->AsConstant()->Value()->IsJSFunction()) {
    // TODO(turbofan): This lowering might not kick in if we ever lower
    // the C++ accessor for "prototype" in an earlier optimization pass.
    Handle<JSFunction> function =
        Handle<JSFunction>::cast(receiver_type->AsConstant()->Value());
    if (function->has_initial_map()) {
      // We need to add a code dependency on the initial map of the {function}
      // in order to be notified about changes to the "prototype" of {function},
      // so it doesn't make sense to continue unless deoptimization is enabled.
      if (!(flags() & kDeoptimizationEnabled)) return NoChange();
      Handle<Map> initial_map(function->initial_map(), isolate());
      dependencies()->AssumeInitialMapCantChange(initial_map);
      Node* value =
          jsgraph()->Constant(handle(initial_map->prototype(), isolate()));
      ReplaceWithValue(node, value);
      return Replace(value);
    }
  }
  return NoChange();
}


Reduction JSTypedLowering::ReduceJSLoadProperty(Node* node) {
  Node* key = NodeProperties::GetValueInput(node, 1);
  Node* base = NodeProperties::GetValueInput(node, 0);
  Type* key_type = NodeProperties::GetType(key);
  HeapObjectMatcher mbase(base);
  if (mbase.HasValue() && mbase.Value()->IsJSTypedArray()) {
    Handle<JSTypedArray> const array =
        Handle<JSTypedArray>::cast(mbase.Value());
    if (!array->GetBuffer()->was_neutered()) {
      array->GetBuffer()->set_is_neuterable(false);
      BufferAccess const access(array->type());
      size_t const k =
          ElementSizeLog2Of(access.machine_type().representation());
      double const byte_length = array->byte_length()->Number();
      CHECK_LT(k, arraysize(shifted_int32_ranges_));
      if (key_type->Is(shifted_int32_ranges_[k]) && byte_length <= kMaxInt) {
        // JSLoadProperty(typed-array, int32)
        Handle<FixedTypedArrayBase> elements =
            Handle<FixedTypedArrayBase>::cast(handle(array->elements()));
        Node* buffer = jsgraph()->PointerConstant(elements->external_pointer());
        Node* length = jsgraph()->Constant(byte_length);
        Node* effect = NodeProperties::GetEffectInput(node);
        Node* control = NodeProperties::GetControlInput(node);
        // Check if we can avoid the bounds check.
        if (key_type->Min() >= 0 && key_type->Max() < array->length_value()) {
          Node* load = graph()->NewNode(
              simplified()->LoadElement(
                  AccessBuilder::ForTypedArrayElement(array->type(), true)),
              buffer, key, effect, control);
          ReplaceWithValue(node, load, load);
          return Replace(load);
        }
        // Compute byte offset.
        Node* offset = Word32Shl(key, static_cast<int>(k));
        Node* load = graph()->NewNode(simplified()->LoadBuffer(access), buffer,
                                      offset, length, effect, control);
        ReplaceWithValue(node, load, load);
        return Replace(load);
      }
    }
  }
  return NoChange();
}


Reduction JSTypedLowering::ReduceJSStoreProperty(Node* node) {
  Node* key = NodeProperties::GetValueInput(node, 1);
  Node* base = NodeProperties::GetValueInput(node, 0);
  Node* value = NodeProperties::GetValueInput(node, 2);
  Type* key_type = NodeProperties::GetType(key);
  Type* value_type = NodeProperties::GetType(value);
  HeapObjectMatcher mbase(base);
  if (mbase.HasValue() && mbase.Value()->IsJSTypedArray()) {
    Handle<JSTypedArray> const array =
        Handle<JSTypedArray>::cast(mbase.Value());
    if (!array->GetBuffer()->was_neutered()) {
      array->GetBuffer()->set_is_neuterable(false);
      BufferAccess const access(array->type());
      size_t const k =
          ElementSizeLog2Of(access.machine_type().representation());
      double const byte_length = array->byte_length()->Number();
      CHECK_LT(k, arraysize(shifted_int32_ranges_));
      if (access.external_array_type() != kExternalUint8ClampedArray &&
          key_type->Is(shifted_int32_ranges_[k]) && byte_length <= kMaxInt) {
        // JSLoadProperty(typed-array, int32)
        Handle<FixedTypedArrayBase> elements =
            Handle<FixedTypedArrayBase>::cast(handle(array->elements()));
        Node* buffer = jsgraph()->PointerConstant(elements->external_pointer());
        Node* length = jsgraph()->Constant(byte_length);
        Node* context = NodeProperties::GetContextInput(node);
        Node* effect = NodeProperties::GetEffectInput(node);
        Node* control = NodeProperties::GetControlInput(node);
        // Convert to a number first.
        if (!value_type->Is(Type::Number())) {
          Reduction number_reduction = ReduceJSToNumberInput(value);
          if (number_reduction.Changed()) {
            value = number_reduction.replacement();
          } else {
            Node* frame_state_for_to_number =
                NodeProperties::GetFrameStateInput(node, 1);
            value = effect =
                graph()->NewNode(javascript()->ToNumber(), value, context,
                                 frame_state_for_to_number, effect, control);
          }
        }
        // Check if we can avoid the bounds check.
        if (key_type->Min() >= 0 && key_type->Max() < array->length_value()) {
          RelaxControls(node);
          node->ReplaceInput(0, buffer);
          DCHECK_EQ(key, node->InputAt(1));
          node->ReplaceInput(2, value);
          node->ReplaceInput(3, effect);
          node->ReplaceInput(4, control);
          node->TrimInputCount(5);
          NodeProperties::ChangeOp(
              node,
              simplified()->StoreElement(
                  AccessBuilder::ForTypedArrayElement(array->type(), true)));
          return Changed(node);
        }
        // Compute byte offset.
        Node* offset = Word32Shl(key, static_cast<int>(k));
        // Turn into a StoreBuffer operation.
        RelaxControls(node);
        node->ReplaceInput(0, buffer);
        node->ReplaceInput(1, offset);
        node->ReplaceInput(2, length);
        node->ReplaceInput(3, value);
        node->ReplaceInput(4, effect);
        node->ReplaceInput(5, control);
        node->TrimInputCount(6);
        NodeProperties::ChangeOp(node, simplified()->StoreBuffer(access));
        return Changed(node);
      }
    }
  }
  return NoChange();
}


Reduction JSTypedLowering::ReduceJSInstanceOf(Node* node) {
  DCHECK_EQ(IrOpcode::kJSInstanceOf, node->opcode());
  Node* const context = NodeProperties::GetContextInput(node);
  Node* const frame_state = NodeProperties::GetFrameStateInput(node, 0);

  // If deoptimization is disabled, we cannot optimize.
  if (!(flags() & kDeoptimizationEnabled) ||
      (flags() & kDisableBinaryOpReduction)) {
    return NoChange();
  }

  // If we are in a try block, don't optimize since the runtime call
  // in the proxy case can throw.
  if (NodeProperties::IsExceptionalCall(node)) return NoChange();

  JSBinopReduction r(this, node);
  Node* effect = r.effect();
  Node* control = r.control();

  if (!r.right_type()->IsConstant() ||
      !r.right_type()->AsConstant()->Value()->IsJSFunction()) {
    return NoChange();
  }

  Handle<JSFunction> function =
      Handle<JSFunction>::cast(r.right_type()->AsConstant()->Value());
  Handle<SharedFunctionInfo> shared(function->shared(), isolate());

  if (!function->IsConstructor() ||
      function->map()->has_non_instance_prototype()) {
    return NoChange();
  }

  JSFunction::EnsureHasInitialMap(function);
  DCHECK(function->has_initial_map());
  Handle<Map> initial_map(function->initial_map(), isolate());
  this->dependencies()->AssumeInitialMapCantChange(initial_map);
  Node* prototype =
      jsgraph()->Constant(handle(initial_map->prototype(), isolate()));

  Node* if_is_smi = nullptr;
  Node* e_is_smi = nullptr;
  // If the left hand side is an object, no smi check is needed.
  if (r.left_type()->Maybe(Type::TaggedSigned())) {
    Node* is_smi = graph()->NewNode(simplified()->ObjectIsSmi(), r.left());
    Node* branch_is_smi =
        graph()->NewNode(common()->Branch(BranchHint::kFalse), is_smi, control);
    if_is_smi = graph()->NewNode(common()->IfTrue(), branch_is_smi);
    e_is_smi = effect;
    control = graph()->NewNode(common()->IfFalse(), branch_is_smi);
  }

  Node* object_map = effect =
      graph()->NewNode(simplified()->LoadField(AccessBuilder::ForMap()),
                       r.left(), effect, control);

  // Loop through the {object}s prototype chain looking for the {prototype}.
  Node* loop = control = graph()->NewNode(common()->Loop(2), control, control);

  Node* loop_effect = effect =
      graph()->NewNode(common()->EffectPhi(2), effect, effect, loop);

  Node* loop_object_map =
      graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                       object_map, r.left(), loop);

  // Check if the lhs needs access checks.
  Node* map_bit_field = effect =
      graph()->NewNode(simplified()->LoadField(AccessBuilder::ForMapBitField()),
                       loop_object_map, loop_effect, control);
  int is_access_check_needed_bit = 1 << Map::kIsAccessCheckNeeded;
  Node* is_access_check_needed_num =
      graph()->NewNode(simplified()->NumberBitwiseAnd(), map_bit_field,
                       jsgraph()->Uint32Constant(is_access_check_needed_bit));
  Node* is_access_check_needed =
      graph()->NewNode(machine()->Word32Equal(), is_access_check_needed_num,
                       jsgraph()->Uint32Constant(is_access_check_needed_bit));

  Node* branch_is_access_check_needed = graph()->NewNode(
      common()->Branch(BranchHint::kFalse), is_access_check_needed, control);
  Node* if_is_access_check_needed =
      graph()->NewNode(common()->IfTrue(), branch_is_access_check_needed);
  Node* e_is_access_check_needed = effect;

  control =
      graph()->NewNode(common()->IfFalse(), branch_is_access_check_needed);

  // Check if the lhs is a proxy.
  Node* map_instance_type = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForMapInstanceType()),
      loop_object_map, loop_effect, control);
  Node* is_proxy = graph()->NewNode(machine()->Word32Equal(), map_instance_type,
                                    jsgraph()->Uint32Constant(JS_PROXY_TYPE));
  Node* branch_is_proxy =
      graph()->NewNode(common()->Branch(BranchHint::kFalse), is_proxy, control);
  Node* if_is_proxy = graph()->NewNode(common()->IfTrue(), branch_is_proxy);
  Node* e_is_proxy = effect;


  Node* runtime_has_in_proto_chain = control = graph()->NewNode(
      common()->Merge(2), if_is_access_check_needed, if_is_proxy);
  effect = graph()->NewNode(common()->EffectPhi(2), e_is_access_check_needed,
                            e_is_proxy, control);

  // If we need an access check or the object is a Proxy, make a runtime call
  // to finish the lowering.
  Node* bool_result_runtime_has_in_proto_chain_case = graph()->NewNode(
      javascript()->CallRuntime(Runtime::kHasInPrototypeChain), r.left(),
      prototype, context, frame_state, effect, control);

  control = graph()->NewNode(common()->IfFalse(), branch_is_proxy);

  Node* object_prototype = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForMapPrototype()),
      loop_object_map, loop_effect, control);

  // Check if object prototype is equal to function prototype.
  Node* eq_proto =
      graph()->NewNode(simplified()->ReferenceEqual(r.right_type()),
                       object_prototype, prototype);
  Node* branch_eq_proto =
      graph()->NewNode(common()->Branch(BranchHint::kFalse), eq_proto, control);
  Node* if_eq_proto = graph()->NewNode(common()->IfTrue(), branch_eq_proto);
  Node* e_eq_proto = effect;

  control = graph()->NewNode(common()->IfFalse(), branch_eq_proto);

  // If not, check if object prototype is the null prototype.
  Node* null_proto =
      graph()->NewNode(simplified()->ReferenceEqual(r.right_type()),
                       object_prototype, jsgraph()->NullConstant());
  Node* branch_null_proto = graph()->NewNode(
      common()->Branch(BranchHint::kFalse), null_proto, control);
  Node* if_null_proto = graph()->NewNode(common()->IfTrue(), branch_null_proto);
  Node* e_null_proto = effect;

  control = graph()->NewNode(common()->IfFalse(), branch_null_proto);
  Node* load_object_map = effect =
      graph()->NewNode(simplified()->LoadField(AccessBuilder::ForMap()),
                       object_prototype, effect, control);
  // Close the loop.
  loop_effect->ReplaceInput(1, effect);
  loop_object_map->ReplaceInput(1, load_object_map);
  loop->ReplaceInput(1, control);

  control = graph()->NewNode(common()->Merge(3), runtime_has_in_proto_chain,
                             if_eq_proto, if_null_proto);
  effect = graph()->NewNode(common()->EffectPhi(3),
                            bool_result_runtime_has_in_proto_chain_case,
                            e_eq_proto, e_null_proto, control);

  Node* result = graph()->NewNode(
      common()->Phi(MachineRepresentation::kTagged, 3),
      bool_result_runtime_has_in_proto_chain_case, jsgraph()->TrueConstant(),
      jsgraph()->FalseConstant(), control);

  if (if_is_smi != nullptr) {
    DCHECK_NOT_NULL(e_is_smi);
    control = graph()->NewNode(common()->Merge(2), if_is_smi, control);
    effect =
        graph()->NewNode(common()->EffectPhi(2), e_is_smi, effect, control);
    result = graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                              jsgraph()->FalseConstant(), result, control);
  }

  ReplaceWithValue(node, result, effect, control);
  return Changed(result);
}


Reduction JSTypedLowering::ReduceJSLoadContext(Node* node) {
  DCHECK_EQ(IrOpcode::kJSLoadContext, node->opcode());
  ContextAccess const& access = ContextAccessOf(node->op());
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = graph()->start();
  for (size_t i = 0; i < access.depth(); ++i) {
    Node* previous = effect = graph()->NewNode(
        simplified()->LoadField(
            AccessBuilder::ForContextSlot(Context::PREVIOUS_INDEX)),
        NodeProperties::GetValueInput(node, 0), effect, control);
    node->ReplaceInput(0, previous);
  }
  node->ReplaceInput(1, effect);
  node->ReplaceInput(2, control);
  NodeProperties::ChangeOp(
      node,
      simplified()->LoadField(AccessBuilder::ForContextSlot(access.index())));
  return Changed(node);
}


Reduction JSTypedLowering::ReduceJSStoreContext(Node* node) {
  DCHECK_EQ(IrOpcode::kJSStoreContext, node->opcode());
  ContextAccess const& access = ContextAccessOf(node->op());
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = graph()->start();
  for (size_t i = 0; i < access.depth(); ++i) {
    Node* previous = effect = graph()->NewNode(
        simplified()->LoadField(
            AccessBuilder::ForContextSlot(Context::PREVIOUS_INDEX)),
        NodeProperties::GetValueInput(node, 0), effect, control);
    node->ReplaceInput(0, previous);
  }
  node->RemoveInput(2);
  node->ReplaceInput(2, effect);
  NodeProperties::ChangeOp(
      node,
      simplified()->StoreField(AccessBuilder::ForContextSlot(access.index())));
  return Changed(node);
}


Reduction JSTypedLowering::ReduceJSConvertReceiver(Node* node) {
  DCHECK_EQ(IrOpcode::kJSConvertReceiver, node->opcode());
  ConvertReceiverMode mode = ConvertReceiverModeOf(node->op());
  Node* receiver = NodeProperties::GetValueInput(node, 0);
  Type* receiver_type = NodeProperties::GetType(receiver);
  Node* context = NodeProperties::GetContextInput(node);
  Type* context_type = NodeProperties::GetType(context);
  Node* frame_state = NodeProperties::GetFrameStateInput(node, 0);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  if (!receiver_type->Is(Type::Receiver())) {
    if (receiver_type->Is(Type::NullOrUndefined()) ||
        mode == ConvertReceiverMode::kNullOrUndefined) {
      if (context_type->IsConstant()) {
        Handle<JSObject> global_proxy(
            Handle<Context>::cast(context_type->AsConstant()->Value())
                ->global_proxy(),
            isolate());
        receiver = jsgraph()->Constant(global_proxy);
      } else {
        Node* native_context = effect = graph()->NewNode(
            javascript()->LoadContext(0, Context::NATIVE_CONTEXT_INDEX, true),
            context, context, effect);
        receiver = effect = graph()->NewNode(
            javascript()->LoadContext(0, Context::GLOBAL_PROXY_INDEX, true),
            native_context, native_context, effect);
      }
    } else if (!receiver_type->Maybe(Type::NullOrUndefined()) ||
               mode == ConvertReceiverMode::kNotNullOrUndefined) {
      receiver = effect =
          graph()->NewNode(javascript()->ToObject(), receiver, context,
                           frame_state, effect, control);
    } else {
      // Check {receiver} for undefined.
      Node* check0 =
          graph()->NewNode(simplified()->ReferenceEqual(receiver_type),
                           receiver, jsgraph()->UndefinedConstant());
      Node* branch0 = graph()->NewNode(common()->Branch(BranchHint::kFalse),
                                       check0, control);
      Node* if_true0 = graph()->NewNode(common()->IfTrue(), branch0);
      Node* if_false0 = graph()->NewNode(common()->IfFalse(), branch0);

      // Check {receiver} for null.
      Node* check1 =
          graph()->NewNode(simplified()->ReferenceEqual(receiver_type),
                           receiver, jsgraph()->NullConstant());
      Node* branch1 = graph()->NewNode(common()->Branch(BranchHint::kFalse),
                                       check1, if_false0);
      Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
      Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);

      // Convert {receiver} using ToObject.
      Node* if_convert = if_false1;
      Node* econvert = effect;
      Node* rconvert;
      {
        rconvert = econvert =
            graph()->NewNode(javascript()->ToObject(), receiver, context,
                             frame_state, econvert, if_convert);
      }

      // Replace {receiver} with global proxy of {context}.
      Node* if_global =
          graph()->NewNode(common()->Merge(2), if_true0, if_true1);
      Node* eglobal = effect;
      Node* rglobal;
      {
        if (context_type->IsConstant()) {
          Handle<JSObject> global_proxy(
              Handle<Context>::cast(context_type->AsConstant()->Value())
                  ->global_proxy(),
              isolate());
          rglobal = jsgraph()->Constant(global_proxy);
        } else {
          Node* native_context = eglobal = graph()->NewNode(
              javascript()->LoadContext(0, Context::NATIVE_CONTEXT_INDEX, true),
              context, context, eglobal);
          rglobal = eglobal = graph()->NewNode(
              javascript()->LoadContext(0, Context::GLOBAL_PROXY_INDEX, true),
              native_context, native_context, eglobal);
        }
      }

      control = graph()->NewNode(common()->Merge(2), if_convert, if_global);
      effect =
          graph()->NewNode(common()->EffectPhi(2), econvert, eglobal, control);
      receiver =
          graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                           rconvert, rglobal, control);
    }
  }
  ReplaceWithValue(node, receiver, effect, control);
  return Changed(receiver);
}


Reduction JSTypedLowering::ReduceJSCallConstruct(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCallConstruct, node->opcode());
  CallConstructParameters const& p = CallConstructParametersOf(node->op());
  DCHECK_LE(2u, p.arity());
  int const arity = static_cast<int>(p.arity() - 2);
  Node* target = NodeProperties::GetValueInput(node, 0);
  Type* target_type = NodeProperties::GetType(target);
  Node* new_target = NodeProperties::GetValueInput(node, arity + 1);

  // Check if {target} is a known JSFunction.
  if (target_type->IsConstant() &&
      target_type->AsConstant()->Value()->IsJSFunction()) {
    Handle<JSFunction> function =
        Handle<JSFunction>::cast(target_type->AsConstant()->Value());
    Handle<SharedFunctionInfo> shared(function->shared(), isolate());

    // Remove the eager bailout frame state.
    NodeProperties::RemoveFrameStateInput(node, 1);

    // Patch {node} to an indirect call via the {function}s construct stub.
    Callable callable(handle(shared->construct_stub(), isolate()),
                      ConstructStubDescriptor(isolate()));
    node->RemoveInput(arity + 1);
    node->InsertInput(graph()->zone(), 0,
                      jsgraph()->HeapConstant(callable.code()));
    node->InsertInput(graph()->zone(), 2, new_target);
    node->InsertInput(graph()->zone(), 3, jsgraph()->Int32Constant(arity));
    node->InsertInput(graph()->zone(), 4, jsgraph()->UndefinedConstant());
    node->InsertInput(graph()->zone(), 5, jsgraph()->UndefinedConstant());
    NodeProperties::ChangeOp(
        node, common()->Call(Linkage::GetStubCallDescriptor(
                  isolate(), graph()->zone(), callable.descriptor(), 1 + arity,
                  CallDescriptor::kNeedsFrameState)));
    return Changed(node);
  }

  // Check if {target} is a JSFunction.
  if (target_type->Is(Type::Function())) {
    // Remove the eager bailout frame state.
    NodeProperties::RemoveFrameStateInput(node, 1);

    // Patch {node} to an indirect call via the ConstructFunction builtin.
    Callable callable = CodeFactory::ConstructFunction(isolate());
    node->RemoveInput(arity + 1);
    node->InsertInput(graph()->zone(), 0,
                      jsgraph()->HeapConstant(callable.code()));
    node->InsertInput(graph()->zone(), 2, new_target);
    node->InsertInput(graph()->zone(), 3, jsgraph()->Int32Constant(arity));
    node->InsertInput(graph()->zone(), 4, jsgraph()->UndefinedConstant());
    NodeProperties::ChangeOp(
        node, common()->Call(Linkage::GetStubCallDescriptor(
                  isolate(), graph()->zone(), callable.descriptor(), 1 + arity,
                  CallDescriptor::kNeedsFrameState)));
    return Changed(node);
  }

  return NoChange();
}


Reduction JSTypedLowering::ReduceJSCallFunction(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCallFunction, node->opcode());
  CallFunctionParameters const& p = CallFunctionParametersOf(node->op());
  int const arity = static_cast<int>(p.arity() - 2);
  ConvertReceiverMode convert_mode = p.convert_mode();
  Node* target = NodeProperties::GetValueInput(node, 0);
  Type* target_type = NodeProperties::GetType(target);
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Type* receiver_type = NodeProperties::GetType(receiver);
  Node* frame_state = NodeProperties::GetFrameStateInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // Try to infer receiver {convert_mode} from {receiver} type.
  if (receiver_type->Is(Type::NullOrUndefined())) {
    convert_mode = ConvertReceiverMode::kNullOrUndefined;
  } else if (!receiver_type->Maybe(Type::NullOrUndefined())) {
    convert_mode = ConvertReceiverMode::kNotNullOrUndefined;
  }

  // Check if {target} is a known JSFunction.
  if (target_type->IsConstant() &&
      target_type->AsConstant()->Value()->IsJSFunction()) {
    Handle<JSFunction> function =
        Handle<JSFunction>::cast(target_type->AsConstant()->Value());
    Handle<SharedFunctionInfo> shared(function->shared(), isolate());

    // Class constructors are callable, but [[Call]] will raise an exception.
    // See ES6 section 9.2.1 [[Call]] ( thisArgument, argumentsList ).
    if (IsClassConstructor(shared->kind())) return NoChange();

    // Load the context from the {target}.
    Node* context = effect = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForJSFunctionContext()), target,
        effect, control);
    NodeProperties::ReplaceContextInput(node, context);

    // Check if we need to convert the {receiver}.
    if (is_sloppy(shared->language_mode()) && !shared->native() &&
        !receiver_type->Is(Type::Receiver())) {
      receiver = effect =
          graph()->NewNode(javascript()->ConvertReceiver(convert_mode),
                           receiver, context, frame_state, effect, control);
      NodeProperties::ReplaceValueInput(node, receiver, 1);
    }

    // Update the effect dependency for the {node}.
    NodeProperties::ReplaceEffectInput(node, effect);

    // Remove the eager bailout frame state.
    NodeProperties::RemoveFrameStateInput(node, 1);

    // Compute flags for the call.
    CallDescriptor::Flags flags = CallDescriptor::kNeedsFrameState;
    if (p.tail_call_mode() == TailCallMode::kAllow) {
      flags |= CallDescriptor::kSupportsTailCalls;
    }

    Node* new_target = jsgraph()->UndefinedConstant();
    Node* argument_count = jsgraph()->Int32Constant(arity);
    if (shared->internal_formal_parameter_count() == arity ||
        shared->internal_formal_parameter_count() ==
            SharedFunctionInfo::kDontAdaptArgumentsSentinel) {
      // Patch {node} to a direct call.
      node->InsertInput(graph()->zone(), arity + 2, new_target);
      node->InsertInput(graph()->zone(), arity + 3, argument_count);
      NodeProperties::ChangeOp(node,
                               common()->Call(Linkage::GetJSCallDescriptor(
                                   graph()->zone(), false, 1 + arity, flags)));
    } else {
      // Patch {node} to an indirect call via the ArgumentsAdaptorTrampoline.
      Callable callable = CodeFactory::ArgumentAdaptor(isolate());
      node->InsertInput(graph()->zone(), 0,
                        jsgraph()->HeapConstant(callable.code()));
      node->InsertInput(graph()->zone(), 2, new_target);
      node->InsertInput(graph()->zone(), 3, argument_count);
      node->InsertInput(
          graph()->zone(), 4,
          jsgraph()->Int32Constant(shared->internal_formal_parameter_count()));
      NodeProperties::ChangeOp(
          node, common()->Call(Linkage::GetStubCallDescriptor(
                    isolate(), graph()->zone(), callable.descriptor(),
                    1 + arity, flags)));
    }
    return Changed(node);
  }

  // Check if {target} is a JSFunction.
  if (target_type->Is(Type::Function())) {
    // Remove the eager bailout frame state.
    NodeProperties::RemoveFrameStateInput(node, 1);

    // Compute flags for the call.
    CallDescriptor::Flags flags = CallDescriptor::kNeedsFrameState;
    if (p.tail_call_mode() == TailCallMode::kAllow) {
      flags |= CallDescriptor::kSupportsTailCalls;
    }

    // Patch {node} to an indirect call via the CallFunction builtin.
    Callable callable = CodeFactory::CallFunction(isolate(), convert_mode);
    node->InsertInput(graph()->zone(), 0,
                      jsgraph()->HeapConstant(callable.code()));
    node->InsertInput(graph()->zone(), 2, jsgraph()->Int32Constant(arity));
    NodeProperties::ChangeOp(
        node, common()->Call(Linkage::GetStubCallDescriptor(
                  isolate(), graph()->zone(), callable.descriptor(), 1 + arity,
                  flags)));
    return Changed(node);
  }

  // Maybe we did at least learn something about the {receiver}.
  if (p.convert_mode() != convert_mode) {
    NodeProperties::ChangeOp(
        node, javascript()->CallFunction(p.arity(), p.feedback(), convert_mode,
                                         p.tail_call_mode()));
    return Changed(node);
  }

  return NoChange();
}


Reduction JSTypedLowering::ReduceJSForInDone(Node* node) {
  DCHECK_EQ(IrOpcode::kJSForInDone, node->opcode());
  node->TrimInputCount(2);
  NodeProperties::ChangeOp(node, machine()->Word32Equal());
  return Changed(node);
}


Reduction JSTypedLowering::ReduceJSForInNext(Node* node) {
  DCHECK_EQ(IrOpcode::kJSForInNext, node->opcode());
  Node* receiver = NodeProperties::GetValueInput(node, 0);
  Node* cache_array = NodeProperties::GetValueInput(node, 1);
  Node* cache_type = NodeProperties::GetValueInput(node, 2);
  Node* index = NodeProperties::GetValueInput(node, 3);
  Node* context = NodeProperties::GetContextInput(node);
  Node* frame_state = NodeProperties::GetFrameStateInput(node, 0);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // Load the next {key} from the {cache_array}.
  Node* key = effect = graph()->NewNode(
      simplified()->LoadElement(AccessBuilder::ForFixedArrayElement()),
      cache_array, index, effect, control);

  // Load the map of the {receiver}.
  Node* receiver_map = effect =
      graph()->NewNode(simplified()->LoadField(AccessBuilder::ForMap()),
                       receiver, effect, control);

  // Check if the expected map still matches that of the {receiver}.
  Node* check0 = graph()->NewNode(simplified()->ReferenceEqual(Type::Any()),
                                  receiver_map, cache_type);
  Node* branch0 =
      graph()->NewNode(common()->Branch(BranchHint::kTrue), check0, control);

  Node* if_true0 = graph()->NewNode(common()->IfTrue(), branch0);
  Node* etrue0;
  Node* vtrue0;
  {
    // Don't need filtering since expected map still matches that of the
    // {receiver}.
    etrue0 = effect;
    vtrue0 = key;
  }

  Node* if_false0 = graph()->NewNode(common()->IfFalse(), branch0);
  Node* efalse0;
  Node* vfalse0;
  {
    // Filter the {key} to check if it's still a valid property of the
    // {receiver} (does the ToName conversion implicitly).
    vfalse0 = efalse0 = graph()->NewNode(
        javascript()->CallRuntime(Runtime::kForInFilter), receiver, key,
        context, frame_state, effect, if_false0);
    if_false0 = graph()->NewNode(common()->IfSuccess(), vfalse0);
  }

  control = graph()->NewNode(common()->Merge(2), if_true0, if_false0);
  effect = graph()->NewNode(common()->EffectPhi(2), etrue0, efalse0, control);
  ReplaceWithValue(node, node, effect, control);
  node->ReplaceInput(0, vtrue0);
  node->ReplaceInput(1, vfalse0);
  node->ReplaceInput(2, control);
  node->TrimInputCount(3);
  NodeProperties::ChangeOp(node,
                           common()->Phi(MachineRepresentation::kTagged, 2));
  return Changed(node);
}


Reduction JSTypedLowering::ReduceJSForInStep(Node* node) {
  DCHECK_EQ(IrOpcode::kJSForInStep, node->opcode());
  node->ReplaceInput(1, jsgraph()->Int32Constant(1));
  NodeProperties::ChangeOp(node, machine()->Int32Add());
  return Changed(node);
}


Reduction JSTypedLowering::ReduceSelect(Node* node) {
  DCHECK_EQ(IrOpcode::kSelect, node->opcode());
  Node* const condition = NodeProperties::GetValueInput(node, 0);
  Type* const condition_type = NodeProperties::GetType(condition);
  Node* const vtrue = NodeProperties::GetValueInput(node, 1);
  Type* const vtrue_type = NodeProperties::GetType(vtrue);
  Node* const vfalse = NodeProperties::GetValueInput(node, 2);
  Type* const vfalse_type = NodeProperties::GetType(vfalse);
  if (condition_type->Is(true_type_)) {
    // Select(condition:true, vtrue, vfalse) => vtrue
    return Replace(vtrue);
  }
  if (condition_type->Is(false_type_)) {
    // Select(condition:false, vtrue, vfalse) => vfalse
    return Replace(vfalse);
  }
  if (vtrue_type->Is(true_type_) && vfalse_type->Is(false_type_)) {
    // Select(condition, vtrue:true, vfalse:false) => condition
    return Replace(condition);
  }
  if (vtrue_type->Is(false_type_) && vfalse_type->Is(true_type_)) {
    // Select(condition, vtrue:false, vfalse:true) => BooleanNot(condition)
    node->TrimInputCount(1);
    NodeProperties::ChangeOp(node, simplified()->BooleanNot());
    return Changed(node);
  }
  return NoChange();
}


Reduction JSTypedLowering::Reduce(Node* node) {
  // Check if the output type is a singleton.  In that case we already know the
  // result value and can simply replace the node if it's eliminable.
  if (!NodeProperties::IsConstant(node) && NodeProperties::IsTyped(node) &&
      node->op()->HasProperty(Operator::kEliminatable)) {
    Type* upper = NodeProperties::GetType(node);
    if (upper->IsConstant()) {
      Node* replacement = jsgraph()->Constant(upper->AsConstant()->Value());
      ReplaceWithValue(node, replacement);
      return Changed(replacement);
    } else if (upper->Is(Type::MinusZero())) {
      Node* replacement = jsgraph()->Constant(factory()->minus_zero_value());
      ReplaceWithValue(node, replacement);
      return Changed(replacement);
    } else if (upper->Is(Type::NaN())) {
      Node* replacement = jsgraph()->NaNConstant();
      ReplaceWithValue(node, replacement);
      return Changed(replacement);
    } else if (upper->Is(Type::Null())) {
      Node* replacement = jsgraph()->NullConstant();
      ReplaceWithValue(node, replacement);
      return Changed(replacement);
    } else if (upper->Is(Type::PlainNumber()) && upper->Min() == upper->Max()) {
      Node* replacement = jsgraph()->Constant(upper->Min());
      ReplaceWithValue(node, replacement);
      return Changed(replacement);
    } else if (upper->Is(Type::Undefined())) {
      Node* replacement = jsgraph()->UndefinedConstant();
      ReplaceWithValue(node, replacement);
      return Changed(replacement);
    }
  }
  switch (node->opcode()) {
    case IrOpcode::kJSEqual:
      return ReduceJSEqual(node, false);
    case IrOpcode::kJSNotEqual:
      return ReduceJSEqual(node, true);
    case IrOpcode::kJSStrictEqual:
      return ReduceJSStrictEqual(node, false);
    case IrOpcode::kJSStrictNotEqual:
      return ReduceJSStrictEqual(node, true);
    case IrOpcode::kJSLessThan:         // fall through
    case IrOpcode::kJSGreaterThan:      // fall through
    case IrOpcode::kJSLessThanOrEqual:  // fall through
    case IrOpcode::kJSGreaterThanOrEqual:
      return ReduceJSComparison(node);
    case IrOpcode::kJSBitwiseOr:
      return ReduceInt32Binop(node, simplified()->NumberBitwiseOr());
    case IrOpcode::kJSBitwiseXor:
      return ReduceInt32Binop(node, simplified()->NumberBitwiseXor());
    case IrOpcode::kJSBitwiseAnd:
      return ReduceInt32Binop(node, simplified()->NumberBitwiseAnd());
    case IrOpcode::kJSShiftLeft:
      return ReduceUI32Shift(node, kSigned, simplified()->NumberShiftLeft());
    case IrOpcode::kJSShiftRight:
      return ReduceUI32Shift(node, kSigned, simplified()->NumberShiftRight());
    case IrOpcode::kJSShiftRightLogical:
      return ReduceUI32Shift(node, kUnsigned,
                             simplified()->NumberShiftRightLogical());
    case IrOpcode::kJSAdd:
      return ReduceJSAdd(node);
    case IrOpcode::kJSSubtract:
      return ReduceNumberBinop(node, simplified()->NumberSubtract());
    case IrOpcode::kJSMultiply:
      return ReduceNumberBinop(node, simplified()->NumberMultiply());
    case IrOpcode::kJSDivide:
      return ReduceNumberBinop(node, simplified()->NumberDivide());
    case IrOpcode::kJSModulus:
      return ReduceJSModulus(node);
    case IrOpcode::kJSToBoolean:
      return ReduceJSToBoolean(node);
    case IrOpcode::kJSToNumber:
      return ReduceJSToNumber(node);
    case IrOpcode::kJSToString:
      return ReduceJSToString(node);
    case IrOpcode::kJSToObject:
      return ReduceJSToObject(node);
    case IrOpcode::kJSLoadNamed:
      return ReduceJSLoadNamed(node);
    case IrOpcode::kJSLoadProperty:
      return ReduceJSLoadProperty(node);
    case IrOpcode::kJSStoreProperty:
      return ReduceJSStoreProperty(node);
    case IrOpcode::kJSInstanceOf:
      return ReduceJSInstanceOf(node);
    case IrOpcode::kJSLoadContext:
      return ReduceJSLoadContext(node);
    case IrOpcode::kJSStoreContext:
      return ReduceJSStoreContext(node);
    case IrOpcode::kJSConvertReceiver:
      return ReduceJSConvertReceiver(node);
    case IrOpcode::kJSCallConstruct:
      return ReduceJSCallConstruct(node);
    case IrOpcode::kJSCallFunction:
      return ReduceJSCallFunction(node);
    case IrOpcode::kJSForInDone:
      return ReduceJSForInDone(node);
    case IrOpcode::kJSForInNext:
      return ReduceJSForInNext(node);
    case IrOpcode::kJSForInStep:
      return ReduceJSForInStep(node);
    case IrOpcode::kSelect:
      return ReduceSelect(node);
    default:
      break;
  }
  return NoChange();
}


Node* JSTypedLowering::Word32Shl(Node* const lhs, int32_t const rhs) {
  if (rhs == 0) return lhs;
  return graph()->NewNode(machine()->Word32Shl(), lhs,
                          jsgraph()->Int32Constant(rhs));
}


Factory* JSTypedLowering::factory() const { return jsgraph()->factory(); }


Graph* JSTypedLowering::graph() const { return jsgraph()->graph(); }


Isolate* JSTypedLowering::isolate() const { return jsgraph()->isolate(); }


JSOperatorBuilder* JSTypedLowering::javascript() const {
  return jsgraph()->javascript();
}


CommonOperatorBuilder* JSTypedLowering::common() const {
  return jsgraph()->common();
}


SimplifiedOperatorBuilder* JSTypedLowering::simplified() const {
  return jsgraph()->simplified();
}


MachineOperatorBuilder* JSTypedLowering::machine() const {
  return jsgraph()->machine();
}


CompilationDependencies* JSTypedLowering::dependencies() const {
  return dependencies_;
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
