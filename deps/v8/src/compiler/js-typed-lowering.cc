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
#include "src/compiler/state-values-utils.h"
#include "src/type-cache.h"
#include "src/types.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {

// A helper class to construct inline allocations on the simplified operator
// level. This keeps track of the effect chain for initial stores on a newly
// allocated object and also provides helpers for commonly allocated objects.
class AllocationBuilder final {
 public:
  AllocationBuilder(JSGraph* jsgraph, Node* effect, Node* control)
      : jsgraph_(jsgraph),
        allocation_(nullptr),
        effect_(effect),
        control_(control) {}

  // Primitive allocation of static size.
  void Allocate(int size, PretenureFlag pretenure = NOT_TENURED) {
    effect_ = graph()->NewNode(common()->BeginRegion(), effect_);
    allocation_ =
        graph()->NewNode(simplified()->Allocate(pretenure),
                         jsgraph()->Constant(size), effect_, control_);
    effect_ = allocation_;
  }

  // Primitive store into a field.
  void Store(const FieldAccess& access, Node* value) {
    effect_ = graph()->NewNode(simplified()->StoreField(access), allocation_,
                               value, effect_, control_);
  }

  // Primitive store into an element.
  void Store(ElementAccess const& access, Node* index, Node* value) {
    effect_ = graph()->NewNode(simplified()->StoreElement(access), allocation_,
                               index, value, effect_, control_);
  }

  // Compound allocation of a FixedArray.
  void AllocateArray(int length, Handle<Map> map,
                     PretenureFlag pretenure = NOT_TENURED) {
    DCHECK(map->instance_type() == FIXED_ARRAY_TYPE ||
           map->instance_type() == FIXED_DOUBLE_ARRAY_TYPE);
    int size = (map->instance_type() == FIXED_ARRAY_TYPE)
                   ? FixedArray::SizeFor(length)
                   : FixedDoubleArray::SizeFor(length);
    Allocate(size, pretenure);
    Store(AccessBuilder::ForMap(), map);
    Store(AccessBuilder::ForFixedArrayLength(), jsgraph()->Constant(length));
  }

  // Compound store of a constant into a field.
  void Store(const FieldAccess& access, Handle<Object> value) {
    Store(access, jsgraph()->Constant(value));
  }

  void FinishAndChange(Node* node) {
    NodeProperties::SetType(allocation_, NodeProperties::GetType(node));
    node->ReplaceInput(0, allocation_);
    node->ReplaceInput(1, effect_);
    node->TrimInputCount(2);
    NodeProperties::ChangeOp(node, common()->FinishRegion());
  }

  Node* Finish() {
    return graph()->NewNode(common()->FinishRegion(), allocation_, effect_);
  }

 protected:
  JSGraph* jsgraph() { return jsgraph_; }
  Graph* graph() { return jsgraph_->graph(); }
  CommonOperatorBuilder* common() { return jsgraph_->common(); }
  SimplifiedOperatorBuilder* simplified() { return jsgraph_->simplified(); }

 private:
  JSGraph* const jsgraph_;
  Node* allocation_;
  Node* effect_;
  Node* control_;
};

}  // namespace


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

  // TODO(turbofan): Strong mode should be killed soonish!
  bool IsStrong() const {
    if (node_->opcode() == IrOpcode::kJSLessThan ||
        node_->opcode() == IrOpcode::kJSLessThanOrEqual ||
        node_->opcode() == IrOpcode::kJSGreaterThan ||
        node_->opcode() == IrOpcode::kJSGreaterThanOrEqual) {
      return is_strong(OpParameter<LanguageMode>(node_));
    }
    return is_strong(BinaryOperationParametersOf(node_->op()).language_mode());
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
  if (r.NeitherInputCanBe(Type::StringOrReceiver()) && !r.IsStrong()) {
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
  if (r.IsStrong() || numberOp == simplified()->NumberModulus()) {
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
  if (r.IsStrong()) {
    if (r.BothInputsAre(Type::Number())) {
      r.ConvertInputsToUI32(kSigned, kSigned);
      return r.ChangeToPureOperator(intOp, Type::Integral32());
    }
    return NoChange();
  }
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
  if (r.IsStrong()) {
    if (r.BothInputsAre(Type::Number())) {
      r.ConvertInputsToUI32(left_signedness, kUnsigned);
      return r.ChangeToPureOperator(shift_op);
    }
    return NoChange();
  }
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
      if (r.IsStrong() && !r.BothInputsAre(Type::Number())) {
        return NoChange();
      }
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
  // Check if we have a cached conversion.
  Type* input_type = NodeProperties::GetType(input);
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
      javascript()->CallRuntime(Runtime::kHasInPrototypeChain, 2), r.left(),
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


namespace {

// Maximum instance size for which allocations will be inlined.
const int kMaxInlineInstanceSize = 64 * kPointerSize;


// Checks whether allocation using the given constructor can be inlined.
bool IsAllocationInlineable(Handle<JSFunction> constructor) {
  // TODO(bmeurer): Further relax restrictions on inlining, i.e.
  // instance type and maybe instance size (inobject properties
  // are limited anyways by the runtime).
  return constructor->has_initial_map() &&
         constructor->initial_map()->instance_type() == JS_OBJECT_TYPE &&
         constructor->initial_map()->instance_size() < kMaxInlineInstanceSize;
}

}  // namespace


Reduction JSTypedLowering::ReduceJSCreate(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCreate, node->opcode());
  Node* const target = NodeProperties::GetValueInput(node, 0);
  Type* const target_type = NodeProperties::GetType(target);
  Node* const new_target = NodeProperties::GetValueInput(node, 1);
  Node* const effect = NodeProperties::GetEffectInput(node);
  // TODO(turbofan): Add support for NewTarget passed to JSCreate.
  if (target != new_target) return NoChange();
  // Extract constructor function.
  if (target_type->IsConstant() &&
      target_type->AsConstant()->Value()->IsJSFunction()) {
    Handle<JSFunction> constructor =
        Handle<JSFunction>::cast(target_type->AsConstant()->Value());
    DCHECK(constructor->IsConstructor());
    // Force completion of inobject slack tracking before
    // generating code to finalize the instance size.
    constructor->CompleteInobjectSlackTrackingIfActive();

    // TODO(bmeurer): We fall back to the runtime in case we cannot inline
    // the allocation here, which is sort of expensive. We should think about
    // a soft fallback to some NewObjectCodeStub.
    if (IsAllocationInlineable(constructor)) {
      // Compute instance size from initial map of {constructor}.
      Handle<Map> initial_map(constructor->initial_map(), isolate());
      int const instance_size = initial_map->instance_size();

      // Add a dependency on the {initial_map} to make sure that this code is
      // deoptimized whenever the {initial_map} of the {constructor} changes.
      dependencies()->AssumeInitialMapCantChange(initial_map);

      // Emit code to allocate the JSObject instance for the {constructor}.
      AllocationBuilder a(jsgraph(), effect, graph()->start());
      a.Allocate(instance_size);
      a.Store(AccessBuilder::ForMap(), initial_map);
      a.Store(AccessBuilder::ForJSObjectProperties(),
              jsgraph()->EmptyFixedArrayConstant());
      a.Store(AccessBuilder::ForJSObjectElements(),
              jsgraph()->EmptyFixedArrayConstant());
      for (int i = 0; i < initial_map->GetInObjectProperties(); ++i) {
        a.Store(AccessBuilder::ForJSObjectInObjectProperty(initial_map, i),
                jsgraph()->UndefinedConstant());
      }
      a.FinishAndChange(node);
      return Changed(node);
    }
  }
  return NoChange();
}


namespace {

// Retrieves the frame state holding actual argument values.
Node* GetArgumentsFrameState(Node* frame_state) {
  Node* const outer_state = frame_state->InputAt(kFrameStateOuterStateInput);
  FrameStateInfo outer_state_info = OpParameter<FrameStateInfo>(outer_state);
  return outer_state_info.type() == FrameStateType::kArgumentsAdaptor
             ? outer_state
             : frame_state;
}

}  // namespace


Reduction JSTypedLowering::ReduceJSCreateArguments(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCreateArguments, node->opcode());
  CreateArgumentsParameters const& p = CreateArgumentsParametersOf(node->op());
  Node* const frame_state = NodeProperties::GetFrameStateInput(node, 0);
  Node* const outer_state = frame_state->InputAt(kFrameStateOuterStateInput);
  FrameStateInfo state_info = OpParameter<FrameStateInfo>(frame_state);

  // Use the ArgumentsAccessStub for materializing both mapped and unmapped
  // arguments object, but only for non-inlined (i.e. outermost) frames.
  if (outer_state->opcode() != IrOpcode::kFrameState) {
    Isolate* isolate = jsgraph()->isolate();
    int parameter_count = state_info.parameter_count() - 1;
    int parameter_offset = parameter_count * kPointerSize;
    int offset = StandardFrameConstants::kCallerSPOffset + parameter_offset;
    Node* parameter_pointer = graph()->NewNode(
        machine()->IntAdd(), graph()->NewNode(machine()->LoadFramePointer()),
        jsgraph()->IntPtrConstant(offset));

    if (p.type() != CreateArgumentsParameters::kRestArray) {
      Handle<SharedFunctionInfo> shared;
      if (!state_info.shared_info().ToHandle(&shared)) return NoChange();
      bool unmapped = p.type() == CreateArgumentsParameters::kUnmappedArguments;
      Callable callable = CodeFactory::ArgumentsAccess(
          isolate, unmapped, shared->has_duplicate_parameters());
      CallDescriptor* desc = Linkage::GetStubCallDescriptor(
          isolate, graph()->zone(), callable.descriptor(), 0,
          CallDescriptor::kNeedsFrameState);
      const Operator* new_op = common()->Call(desc);
      Node* stub_code = jsgraph()->HeapConstant(callable.code());
      node->InsertInput(graph()->zone(), 0, stub_code);
      node->InsertInput(graph()->zone(), 2,
                        jsgraph()->Constant(parameter_count));
      node->InsertInput(graph()->zone(), 3, parameter_pointer);
      NodeProperties::ChangeOp(node, new_op);
      return Changed(node);
    } else {
      Callable callable = CodeFactory::RestArgumentsAccess(isolate);
      CallDescriptor* desc = Linkage::GetStubCallDescriptor(
          isolate, graph()->zone(), callable.descriptor(), 0,
          CallDescriptor::kNeedsFrameState);
      const Operator* new_op = common()->Call(desc);
      Node* stub_code = jsgraph()->HeapConstant(callable.code());
      node->InsertInput(graph()->zone(), 0, stub_code);
      node->ReplaceInput(1, jsgraph()->Constant(parameter_count));
      node->InsertInput(graph()->zone(), 2, parameter_pointer);
      node->InsertInput(graph()->zone(), 3,
                        jsgraph()->Constant(p.start_index()));
      NodeProperties::ChangeOp(node, new_op);
      return Changed(node);
    }
  } else if (outer_state->opcode() == IrOpcode::kFrameState) {
    // Use inline allocation for all mapped arguments objects within inlined
    // (i.e. non-outermost) frames, independent of the object size.
    if (p.type() == CreateArgumentsParameters::kMappedArguments) {
      Handle<SharedFunctionInfo> shared;
      if (!state_info.shared_info().ToHandle(&shared)) return NoChange();
      Node* const callee = NodeProperties::GetValueInput(node, 0);
      Node* const control = NodeProperties::GetControlInput(node);
      Node* const context = NodeProperties::GetContextInput(node);
      Node* effect = NodeProperties::GetEffectInput(node);
      // TODO(mstarzinger): Duplicate parameters are not handled yet.
      if (shared->has_duplicate_parameters()) return NoChange();
      // Choose the correct frame state and frame state info depending on
      // whether there conceptually is an arguments adaptor frame in the call
      // chain.
      Node* const args_state = GetArgumentsFrameState(frame_state);
      FrameStateInfo args_state_info = OpParameter<FrameStateInfo>(args_state);
      // Prepare element backing store to be used by arguments object.
      bool has_aliased_arguments = false;
      Node* const elements = AllocateAliasedArguments(
          effect, control, args_state, context, shared, &has_aliased_arguments);
      effect = elements->op()->EffectOutputCount() > 0 ? elements : effect;
      // Load the arguments object map from the current native context.
      Node* const load_native_context = effect = graph()->NewNode(
          javascript()->LoadContext(0, Context::NATIVE_CONTEXT_INDEX, true),
          context, context, effect);
      Node* const load_arguments_map = effect = graph()->NewNode(
          simplified()->LoadField(AccessBuilder::ForContextSlot(
              has_aliased_arguments ? Context::FAST_ALIASED_ARGUMENTS_MAP_INDEX
                                    : Context::SLOPPY_ARGUMENTS_MAP_INDEX)),
          load_native_context, effect, control);
      // Actually allocate and initialize the arguments object.
      AllocationBuilder a(jsgraph(), effect, control);
      Node* properties = jsgraph()->EmptyFixedArrayConstant();
      int length = args_state_info.parameter_count() - 1;  // Minus receiver.
      STATIC_ASSERT(Heap::kSloppyArgumentsObjectSize == 5 * kPointerSize);
      a.Allocate(Heap::kSloppyArgumentsObjectSize);
      a.Store(AccessBuilder::ForMap(), load_arguments_map);
      a.Store(AccessBuilder::ForJSObjectProperties(), properties);
      a.Store(AccessBuilder::ForJSObjectElements(), elements);
      a.Store(AccessBuilder::ForArgumentsLength(), jsgraph()->Constant(length));
      a.Store(AccessBuilder::ForArgumentsCallee(), callee);
      RelaxControls(node);
      a.FinishAndChange(node);
      return Changed(node);
    } else if (p.type() == CreateArgumentsParameters::kUnmappedArguments) {
      // Use inline allocation for all unmapped arguments objects within inlined
      // (i.e. non-outermost) frames, independent of the object size.
      Node* const control = NodeProperties::GetControlInput(node);
      Node* const context = NodeProperties::GetContextInput(node);
      Node* effect = NodeProperties::GetEffectInput(node);
      // Choose the correct frame state and frame state info depending on
      // whether there conceptually is an arguments adaptor frame in the call
      // chain.
      Node* const args_state = GetArgumentsFrameState(frame_state);
      FrameStateInfo args_state_info = OpParameter<FrameStateInfo>(args_state);
      // Prepare element backing store to be used by arguments object.
      Node* const elements = AllocateArguments(effect, control, args_state);
      effect = elements->op()->EffectOutputCount() > 0 ? elements : effect;
      // Load the arguments object map from the current native context.
      Node* const load_native_context = effect = graph()->NewNode(
          javascript()->LoadContext(0, Context::NATIVE_CONTEXT_INDEX, true),
          context, context, effect);
      Node* const load_arguments_map = effect = graph()->NewNode(
          simplified()->LoadField(AccessBuilder::ForContextSlot(
              Context::STRICT_ARGUMENTS_MAP_INDEX)),
          load_native_context, effect, control);
      // Actually allocate and initialize the arguments object.
      AllocationBuilder a(jsgraph(), effect, control);
      Node* properties = jsgraph()->EmptyFixedArrayConstant();
      int length = args_state_info.parameter_count() - 1;  // Minus receiver.
      STATIC_ASSERT(Heap::kStrictArgumentsObjectSize == 4 * kPointerSize);
      a.Allocate(Heap::kStrictArgumentsObjectSize);
      a.Store(AccessBuilder::ForMap(), load_arguments_map);
      a.Store(AccessBuilder::ForJSObjectProperties(), properties);
      a.Store(AccessBuilder::ForJSObjectElements(), elements);
      a.Store(AccessBuilder::ForArgumentsLength(), jsgraph()->Constant(length));
      RelaxControls(node);
      a.FinishAndChange(node);
      return Changed(node);
    } else if (p.type() == CreateArgumentsParameters::kRestArray) {
      // Use inline allocation for all unmapped arguments objects within inlined
      // (i.e. non-outermost) frames, independent of the object size.
      Node* const control = NodeProperties::GetControlInput(node);
      Node* const context = NodeProperties::GetContextInput(node);
      Node* effect = NodeProperties::GetEffectInput(node);
      // Choose the correct frame state and frame state info depending on
      // whether there conceptually is an arguments adaptor frame in the call
      // chain.
      Node* const args_state = GetArgumentsFrameState(frame_state);
      FrameStateInfo args_state_info = OpParameter<FrameStateInfo>(args_state);
      // Prepare element backing store to be used by the rest array.
      Node* const elements =
          AllocateRestArguments(effect, control, args_state, p.start_index());
      effect = elements->op()->EffectOutputCount() > 0 ? elements : effect;
      // Load the JSArray object map from the current native context.
      Node* const load_native_context = effect = graph()->NewNode(
          javascript()->LoadContext(0, Context::NATIVE_CONTEXT_INDEX, true),
          context, context, effect);
      Node* const load_jsarray_map = effect = graph()->NewNode(
          simplified()->LoadField(AccessBuilder::ForContextSlot(
              Context::JS_ARRAY_FAST_ELEMENTS_MAP_INDEX)),
          load_native_context, effect, control);
      // Actually allocate and initialize the jsarray.
      AllocationBuilder a(jsgraph(), effect, control);
      Node* properties = jsgraph()->EmptyFixedArrayConstant();

      // -1 to minus receiver
      int argument_count = args_state_info.parameter_count() - 1;
      int length = std::max(0, argument_count - p.start_index());
      STATIC_ASSERT(JSArray::kSize == 4 * kPointerSize);
      a.Allocate(JSArray::kSize);
      a.Store(AccessBuilder::ForMap(), load_jsarray_map);
      a.Store(AccessBuilder::ForJSObjectProperties(), properties);
      a.Store(AccessBuilder::ForJSObjectElements(), elements);
      a.Store(AccessBuilder::ForJSArrayLength(FAST_ELEMENTS),
              jsgraph()->Constant(length));
      RelaxControls(node);
      a.FinishAndChange(node);
      return Changed(node);
    }
  }

  return NoChange();
}


Reduction JSTypedLowering::ReduceNewArray(Node* node, Node* length,
                                          int capacity,
                                          Handle<AllocationSite> site) {
  DCHECK_EQ(IrOpcode::kJSCreateArray, node->opcode());
  Node* context = NodeProperties::GetContextInput(node);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // Extract transition and tenuring feedback from the {site} and add
  // appropriate code dependencies on the {site} if deoptimization is
  // enabled.
  PretenureFlag pretenure = site->GetPretenureMode();
  ElementsKind elements_kind = site->GetElementsKind();
  DCHECK(IsFastElementsKind(elements_kind));
  if (flags() & kDeoptimizationEnabled) {
    dependencies()->AssumeTenuringDecision(site);
    dependencies()->AssumeTransitionStable(site);
  }

  // Retrieve the initial map for the array from the appropriate native context.
  Node* native_context = effect = graph()->NewNode(
      javascript()->LoadContext(0, Context::NATIVE_CONTEXT_INDEX, true),
      context, context, effect);
  Node* js_array_map = effect = graph()->NewNode(
      javascript()->LoadContext(0, Context::ArrayMapIndex(elements_kind), true),
      native_context, native_context, effect);

  // Setup elements and properties.
  Node* elements;
  if (capacity == 0) {
    elements = jsgraph()->EmptyFixedArrayConstant();
  } else {
    elements = effect =
        AllocateElements(effect, control, elements_kind, capacity, pretenure);
  }
  Node* properties = jsgraph()->EmptyFixedArrayConstant();

  // Perform the allocation of the actual JSArray object.
  AllocationBuilder a(jsgraph(), effect, control);
  a.Allocate(JSArray::kSize, pretenure);
  a.Store(AccessBuilder::ForMap(), js_array_map);
  a.Store(AccessBuilder::ForJSObjectProperties(), properties);
  a.Store(AccessBuilder::ForJSObjectElements(), elements);
  a.Store(AccessBuilder::ForJSArrayLength(elements_kind), length);
  RelaxControls(node);
  a.FinishAndChange(node);
  return Changed(node);
}


Reduction JSTypedLowering::ReduceJSCreateArray(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCreateArray, node->opcode());
  CreateArrayParameters const& p = CreateArrayParametersOf(node->op());
  Node* target = NodeProperties::GetValueInput(node, 0);
  Node* new_target = NodeProperties::GetValueInput(node, 1);

  // TODO(bmeurer): Optimize the subclassing case.
  if (target != new_target) return NoChange();

  // Check if we have a feedback {site} on the {node}.
  Handle<AllocationSite> site = p.site();
  if (p.site().is_null()) return NoChange();

  // Attempt to inline calls to the Array constructor for the relevant cases
  // where either no arguments are provided, or exactly one unsigned number
  // argument is given.
  if (site->CanInlineCall()) {
    if (p.arity() == 0) {
      Node* length = jsgraph()->ZeroConstant();
      int capacity = JSArray::kPreallocatedArrayElements;
      return ReduceNewArray(node, length, capacity, site);
    } else if (p.arity() == 1) {
      Node* length = NodeProperties::GetValueInput(node, 2);
      Type* length_type = NodeProperties::GetType(length);
      if (length_type->Is(type_cache_.kElementLoopUnrollType)) {
        int capacity = static_cast<int>(length_type->Max());
        return ReduceNewArray(node, length, capacity, site);
      }
    }
  }

  // Reduce {node} to the appropriate ArrayConstructorStub backend.
  // Note that these stubs "behave" like JSFunctions, which means they
  // expect a receiver on the stack, which they remove. We just push
  // undefined for the receiver.
  ElementsKind elements_kind = site->GetElementsKind();
  AllocationSiteOverrideMode override_mode =
      (AllocationSite::GetMode(elements_kind) == TRACK_ALLOCATION_SITE)
          ? DISABLE_ALLOCATION_SITES
          : DONT_OVERRIDE;
  if (p.arity() == 0) {
    ArrayNoArgumentConstructorStub stub(isolate(), elements_kind,
                                        override_mode);
    CallDescriptor* desc = Linkage::GetStubCallDescriptor(
        isolate(), graph()->zone(), stub.GetCallInterfaceDescriptor(), 1,
        CallDescriptor::kNeedsFrameState);
    node->ReplaceInput(0, jsgraph()->HeapConstant(stub.GetCode()));
    node->InsertInput(graph()->zone(), 2, jsgraph()->HeapConstant(site));
    node->InsertInput(graph()->zone(), 3, jsgraph()->UndefinedConstant());
    NodeProperties::ChangeOp(node, common()->Call(desc));
    return Changed(node);
  } else if (p.arity() == 1) {
    // TODO(bmeurer): Optimize for the 0 length non-holey case?
    ArraySingleArgumentConstructorStub stub(
        isolate(), GetHoleyElementsKind(elements_kind), override_mode);
    CallDescriptor* desc = Linkage::GetStubCallDescriptor(
        isolate(), graph()->zone(), stub.GetCallInterfaceDescriptor(), 2,
        CallDescriptor::kNeedsFrameState);
    node->ReplaceInput(0, jsgraph()->HeapConstant(stub.GetCode()));
    node->InsertInput(graph()->zone(), 2, jsgraph()->HeapConstant(site));
    node->InsertInput(graph()->zone(), 3, jsgraph()->Int32Constant(1));
    node->InsertInput(graph()->zone(), 4, jsgraph()->UndefinedConstant());
    NodeProperties::ChangeOp(node, common()->Call(desc));
    return Changed(node);
  } else {
    int const arity = static_cast<int>(p.arity());
    ArrayNArgumentsConstructorStub stub(isolate(), elements_kind,
                                        override_mode);
    CallDescriptor* desc = Linkage::GetStubCallDescriptor(
        isolate(), graph()->zone(), stub.GetCallInterfaceDescriptor(),
        arity + 1, CallDescriptor::kNeedsFrameState);
    node->ReplaceInput(0, jsgraph()->HeapConstant(stub.GetCode()));
    node->InsertInput(graph()->zone(), 2, jsgraph()->HeapConstant(site));
    node->InsertInput(graph()->zone(), 3, jsgraph()->Int32Constant(arity));
    node->InsertInput(graph()->zone(), 4, jsgraph()->UndefinedConstant());
    NodeProperties::ChangeOp(node, common()->Call(desc));
    return Changed(node);
  }
}


Reduction JSTypedLowering::ReduceJSCreateClosure(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCreateClosure, node->opcode());
  CreateClosureParameters const& p = CreateClosureParametersOf(node->op());
  Handle<SharedFunctionInfo> shared = p.shared_info();

  // Use the FastNewClosureStub that allocates in new space only for nested
  // functions that don't need literals cloning.
  if (p.pretenure() == NOT_TENURED && shared->num_literals() == 0) {
    Isolate* isolate = jsgraph()->isolate();
    Callable callable = CodeFactory::FastNewClosure(
        isolate, shared->language_mode(), shared->kind());
    CallDescriptor* desc = Linkage::GetStubCallDescriptor(
        isolate, graph()->zone(), callable.descriptor(), 0,
        CallDescriptor::kNoFlags);
    const Operator* new_op = common()->Call(desc);
    Node* stub_code = jsgraph()->HeapConstant(callable.code());
    node->InsertInput(graph()->zone(), 0, stub_code);
    node->InsertInput(graph()->zone(), 1, jsgraph()->HeapConstant(shared));
    NodeProperties::ChangeOp(node, new_op);
    return Changed(node);
  }

  return NoChange();
}


Reduction JSTypedLowering::ReduceJSCreateIterResultObject(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCreateIterResultObject, node->opcode());
  Node* value = NodeProperties::GetValueInput(node, 0);
  Node* done = NodeProperties::GetValueInput(node, 1);
  Node* context = NodeProperties::GetContextInput(node);
  Node* effect = NodeProperties::GetEffectInput(node);

  // Load the JSIteratorResult map for the {context}.
  Node* native_context = effect = graph()->NewNode(
      javascript()->LoadContext(0, Context::NATIVE_CONTEXT_INDEX, true),
      context, context, effect);
  Node* iterator_result_map = effect = graph()->NewNode(
      javascript()->LoadContext(0, Context::ITERATOR_RESULT_MAP_INDEX, true),
      native_context, native_context, effect);

  // Emit code to allocate the JSIteratorResult instance.
  AllocationBuilder a(jsgraph(), effect, graph()->start());
  a.Allocate(JSIteratorResult::kSize);
  a.Store(AccessBuilder::ForMap(), iterator_result_map);
  a.Store(AccessBuilder::ForJSObjectProperties(),
          jsgraph()->EmptyFixedArrayConstant());
  a.Store(AccessBuilder::ForJSObjectElements(),
          jsgraph()->EmptyFixedArrayConstant());
  a.Store(AccessBuilder::ForJSIteratorResultValue(), value);
  a.Store(AccessBuilder::ForJSIteratorResultDone(), done);
  STATIC_ASSERT(JSIteratorResult::kSize == 5 * kPointerSize);
  a.FinishAndChange(node);
  return Changed(node);
}


Reduction JSTypedLowering::ReduceJSCreateLiteralArray(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCreateLiteralArray, node->opcode());
  CreateLiteralParameters const& p = CreateLiteralParametersOf(node->op());
  Handle<FixedArray> const constants = Handle<FixedArray>::cast(p.constant());
  int const length = constants->length();
  int const flags = p.flags();

  // Use the FastCloneShallowArrayStub only for shallow boilerplates up to the
  // initial length limit for arrays with "fast" elements kind.
  // TODO(rossberg): Teach strong mode to FastCloneShallowArrayStub.
  if ((flags & ArrayLiteral::kShallowElements) != 0 &&
      (flags & ArrayLiteral::kIsStrong) == 0 &&
      length < JSArray::kInitialMaxFastElementArray) {
    Isolate* isolate = jsgraph()->isolate();
    Callable callable = CodeFactory::FastCloneShallowArray(isolate);
    CallDescriptor* desc = Linkage::GetStubCallDescriptor(
        isolate, graph()->zone(), callable.descriptor(), 0,
        (OperatorProperties::GetFrameStateInputCount(node->op()) != 0)
            ? CallDescriptor::kNeedsFrameState
            : CallDescriptor::kNoFlags);
    const Operator* new_op = common()->Call(desc);
    Node* stub_code = jsgraph()->HeapConstant(callable.code());
    Node* literal_index = jsgraph()->SmiConstant(p.index());
    Node* constant_elements = jsgraph()->HeapConstant(constants);
    node->InsertInput(graph()->zone(), 0, stub_code);
    node->InsertInput(graph()->zone(), 2, literal_index);
    node->InsertInput(graph()->zone(), 3, constant_elements);
    NodeProperties::ChangeOp(node, new_op);
    return Changed(node);
  }

  return NoChange();
}


Reduction JSTypedLowering::ReduceJSCreateLiteralObject(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCreateLiteralObject, node->opcode());
  CreateLiteralParameters const& p = CreateLiteralParametersOf(node->op());
  Handle<FixedArray> const constants = Handle<FixedArray>::cast(p.constant());
  // Constants are pairs, see ObjectLiteral::properties_count().
  int const length = constants->length() / 2;
  int const flags = p.flags();

  // Use the FastCloneShallowObjectStub only for shallow boilerplates without
  // elements up to the number of properties that the stubs can handle.
  if ((flags & ObjectLiteral::kShallowProperties) != 0 &&
      length <= FastCloneShallowObjectStub::kMaximumClonedProperties) {
    Isolate* isolate = jsgraph()->isolate();
    Callable callable = CodeFactory::FastCloneShallowObject(isolate, length);
    CallDescriptor* desc = Linkage::GetStubCallDescriptor(
        isolate, graph()->zone(), callable.descriptor(), 0,
        (OperatorProperties::GetFrameStateInputCount(node->op()) != 0)
            ? CallDescriptor::kNeedsFrameState
            : CallDescriptor::kNoFlags);
    const Operator* new_op = common()->Call(desc);
    Node* stub_code = jsgraph()->HeapConstant(callable.code());
    Node* literal_index = jsgraph()->SmiConstant(p.index());
    Node* literal_flags = jsgraph()->SmiConstant(flags);
    Node* constant_elements = jsgraph()->HeapConstant(constants);
    node->InsertInput(graph()->zone(), 0, stub_code);
    node->InsertInput(graph()->zone(), 2, literal_index);
    node->InsertInput(graph()->zone(), 3, constant_elements);
    node->InsertInput(graph()->zone(), 4, literal_flags);
    NodeProperties::ChangeOp(node, new_op);
    return Changed(node);
  }

  return NoChange();
}


Reduction JSTypedLowering::ReduceJSCreateFunctionContext(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCreateFunctionContext, node->opcode());
  int slot_count = OpParameter<int>(node->op());
  Node* const closure = NodeProperties::GetValueInput(node, 0);

  // Use inline allocation for function contexts up to a size limit.
  if (slot_count < kFunctionContextAllocationLimit) {
    // JSCreateFunctionContext[slot_count < limit]](fun)
    Node* effect = NodeProperties::GetEffectInput(node);
    Node* control = NodeProperties::GetControlInput(node);
    Node* context = NodeProperties::GetContextInput(node);
    Node* extension = jsgraph()->TheHoleConstant();
    Node* native_context = effect = graph()->NewNode(
        javascript()->LoadContext(0, Context::NATIVE_CONTEXT_INDEX, true),
        context, context, effect);
    AllocationBuilder a(jsgraph(), effect, control);
    STATIC_ASSERT(Context::MIN_CONTEXT_SLOTS == 4);  // Ensure fully covered.
    int context_length = slot_count + Context::MIN_CONTEXT_SLOTS;
    a.AllocateArray(context_length, factory()->function_context_map());
    a.Store(AccessBuilder::ForContextSlot(Context::CLOSURE_INDEX), closure);
    a.Store(AccessBuilder::ForContextSlot(Context::PREVIOUS_INDEX), context);
    a.Store(AccessBuilder::ForContextSlot(Context::EXTENSION_INDEX), extension);
    a.Store(AccessBuilder::ForContextSlot(Context::NATIVE_CONTEXT_INDEX),
            native_context);
    for (int i = Context::MIN_CONTEXT_SLOTS; i < context_length; ++i) {
      a.Store(AccessBuilder::ForContextSlot(i), jsgraph()->UndefinedConstant());
    }
    RelaxControls(node);
    a.FinishAndChange(node);
    return Changed(node);
  }

  // Use the FastNewContextStub only for function contexts up maximum size.
  if (slot_count <= FastNewContextStub::kMaximumSlots) {
    Isolate* isolate = jsgraph()->isolate();
    Callable callable = CodeFactory::FastNewContext(isolate, slot_count);
    CallDescriptor* desc = Linkage::GetStubCallDescriptor(
        isolate, graph()->zone(), callable.descriptor(), 0,
        CallDescriptor::kNoFlags);
    const Operator* new_op = common()->Call(desc);
    Node* stub_code = jsgraph()->HeapConstant(callable.code());
    node->InsertInput(graph()->zone(), 0, stub_code);
    NodeProperties::ChangeOp(node, new_op);
    return Changed(node);
  }

  return NoChange();
}


Reduction JSTypedLowering::ReduceJSCreateWithContext(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCreateWithContext, node->opcode());
  Node* object = NodeProperties::GetValueInput(node, 0);
  Node* closure = NodeProperties::GetValueInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* context = NodeProperties::GetContextInput(node);
  Node* native_context = effect = graph()->NewNode(
      javascript()->LoadContext(0, Context::NATIVE_CONTEXT_INDEX, true),
      context, context, effect);
  AllocationBuilder a(jsgraph(), effect, control);
  STATIC_ASSERT(Context::MIN_CONTEXT_SLOTS == 4);  // Ensure fully covered.
  a.AllocateArray(Context::MIN_CONTEXT_SLOTS, factory()->with_context_map());
  a.Store(AccessBuilder::ForContextSlot(Context::CLOSURE_INDEX), closure);
  a.Store(AccessBuilder::ForContextSlot(Context::PREVIOUS_INDEX), context);
  a.Store(AccessBuilder::ForContextSlot(Context::EXTENSION_INDEX), object);
  a.Store(AccessBuilder::ForContextSlot(Context::NATIVE_CONTEXT_INDEX),
          native_context);
  RelaxControls(node);
  a.FinishAndChange(node);
  return Changed(node);
}


Reduction JSTypedLowering::ReduceJSCreateCatchContext(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCreateCatchContext, node->opcode());
  Handle<String> name = OpParameter<Handle<String>>(node);
  Node* exception = NodeProperties::GetValueInput(node, 0);
  Node* closure = NodeProperties::GetValueInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* context = NodeProperties::GetContextInput(node);
  Node* native_context = effect = graph()->NewNode(
      javascript()->LoadContext(0, Context::NATIVE_CONTEXT_INDEX, true),
      context, context, effect);
  AllocationBuilder a(jsgraph(), effect, control);
  STATIC_ASSERT(Context::MIN_CONTEXT_SLOTS == 4);  // Ensure fully covered.
  a.AllocateArray(Context::MIN_CONTEXT_SLOTS + 1,
                  factory()->catch_context_map());
  a.Store(AccessBuilder::ForContextSlot(Context::CLOSURE_INDEX), closure);
  a.Store(AccessBuilder::ForContextSlot(Context::PREVIOUS_INDEX), context);
  a.Store(AccessBuilder::ForContextSlot(Context::EXTENSION_INDEX), name);
  a.Store(AccessBuilder::ForContextSlot(Context::NATIVE_CONTEXT_INDEX),
          native_context);
  a.Store(AccessBuilder::ForContextSlot(Context::THROWN_OBJECT_INDEX),
          exception);
  RelaxControls(node);
  a.FinishAndChange(node);
  return Changed(node);
}


Reduction JSTypedLowering::ReduceJSCreateBlockContext(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCreateBlockContext, node->opcode());
  Handle<ScopeInfo> scope_info = OpParameter<Handle<ScopeInfo>>(node);
  int context_length = scope_info->ContextLength();
  Node* const closure = NodeProperties::GetValueInput(node, 0);

  // Use inline allocation for block contexts up to a size limit.
  if (context_length < kBlockContextAllocationLimit) {
    // JSCreateBlockContext[scope[length < limit]](fun)
    Node* effect = NodeProperties::GetEffectInput(node);
    Node* control = NodeProperties::GetControlInput(node);
    Node* context = NodeProperties::GetContextInput(node);
    Node* extension = jsgraph()->Constant(scope_info);
    Node* native_context = effect = graph()->NewNode(
        javascript()->LoadContext(0, Context::NATIVE_CONTEXT_INDEX, true),
        context, context, effect);
    AllocationBuilder a(jsgraph(), effect, control);
    STATIC_ASSERT(Context::MIN_CONTEXT_SLOTS == 4);  // Ensure fully covered.
    a.AllocateArray(context_length, factory()->block_context_map());
    a.Store(AccessBuilder::ForContextSlot(Context::CLOSURE_INDEX), closure);
    a.Store(AccessBuilder::ForContextSlot(Context::PREVIOUS_INDEX), context);
    a.Store(AccessBuilder::ForContextSlot(Context::EXTENSION_INDEX), extension);
    a.Store(AccessBuilder::ForContextSlot(Context::NATIVE_CONTEXT_INDEX),
            native_context);
    for (int i = Context::MIN_CONTEXT_SLOTS; i < context_length; ++i) {
      a.Store(AccessBuilder::ForContextSlot(i), jsgraph()->UndefinedConstant());
    }
    RelaxControls(node);
    a.FinishAndChange(node);
    return Changed(node);
  }

  return NoChange();
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
        node,
        javascript()->CallFunction(p.arity(), p.language_mode(), p.feedback(),
                                   convert_mode, p.tail_call_mode()));
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


Reduction JSTypedLowering::ReduceJSForInPrepare(Node* node) {
  DCHECK_EQ(IrOpcode::kJSForInPrepare, node->opcode());
  Node* receiver = NodeProperties::GetValueInput(node, 0);
  Node* context = NodeProperties::GetContextInput(node);
  Node* frame_state = NodeProperties::GetFrameStateInput(node, 0);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // Get the set of properties to enumerate.
  Node* cache_type = effect = graph()->NewNode(
      javascript()->CallRuntime(Runtime::kGetPropertyNamesFast, 1), receiver,
      context, frame_state, effect, control);
  control = graph()->NewNode(common()->IfSuccess(), cache_type);

  Node* receiver_map = effect =
      graph()->NewNode(simplified()->LoadField(AccessBuilder::ForMap()),
                       receiver, effect, control);
  Node* cache_type_map = effect =
      graph()->NewNode(simplified()->LoadField(AccessBuilder::ForMap()),
                       cache_type, effect, control);
  Node* meta_map = jsgraph()->HeapConstant(factory()->meta_map());

  // If we got a map from the GetPropertyNamesFast runtime call, we can do a
  // fast modification check. Otherwise, we got a fixed array, and we have to
  // perform a slow check on every iteration.
  Node* check0 = graph()->NewNode(simplified()->ReferenceEqual(Type::Any()),
                                  cache_type_map, meta_map);
  Node* branch0 =
      graph()->NewNode(common()->Branch(BranchHint::kTrue), check0, control);

  Node* if_true0 = graph()->NewNode(common()->IfTrue(), branch0);
  Node* cache_array_true0;
  Node* cache_length_true0;
  Node* cache_type_true0;
  Node* etrue0;
  {
    // Enum cache case.
    Node* cache_type_enum_length = etrue0 = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForMapBitField3()), cache_type,
        effect, if_true0);
    cache_length_true0 = graph()->NewNode(
        simplified()->NumberBitwiseAnd(), cache_type_enum_length,
        jsgraph()->Int32Constant(Map::EnumLengthBits::kMask));

    Node* check1 =
        graph()->NewNode(machine()->Word32Equal(), cache_length_true0,
                         jsgraph()->Int32Constant(0));
    Node* branch1 =
        graph()->NewNode(common()->Branch(BranchHint::kTrue), check1, if_true0);

    Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
    Node* cache_array_true1;
    Node* etrue1;
    {
      // No properties to enumerate.
      cache_array_true1 =
          jsgraph()->HeapConstant(factory()->empty_fixed_array());
      etrue1 = etrue0;
    }

    Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
    Node* cache_array_false1;
    Node* efalse1;
    {
      // Load the enumeration cache from the instance descriptors of {receiver}.
      Node* receiver_map_descriptors = efalse1 = graph()->NewNode(
          simplified()->LoadField(AccessBuilder::ForMapDescriptors()),
          receiver_map, etrue0, if_false1);
      Node* object_map_enum_cache = efalse1 = graph()->NewNode(
          simplified()->LoadField(AccessBuilder::ForDescriptorArrayEnumCache()),
          receiver_map_descriptors, efalse1, if_false1);
      cache_array_false1 = efalse1 = graph()->NewNode(
          simplified()->LoadField(
              AccessBuilder::ForDescriptorArrayEnumCacheBridgeCache()),
          object_map_enum_cache, efalse1, if_false1);
    }

    if_true0 = graph()->NewNode(common()->Merge(2), if_true1, if_false1);
    etrue0 =
        graph()->NewNode(common()->EffectPhi(2), etrue1, efalse1, if_true0);
    cache_array_true0 =
        graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                         cache_array_true1, cache_array_false1, if_true0);

    cache_type_true0 = cache_type;
  }

  Node* if_false0 = graph()->NewNode(common()->IfFalse(), branch0);
  Node* cache_array_false0;
  Node* cache_length_false0;
  Node* cache_type_false0;
  Node* efalse0;
  {
    // FixedArray case.
    cache_type_false0 = jsgraph()->OneConstant();  // Smi means slow check
    cache_array_false0 = cache_type;
    cache_length_false0 = efalse0 = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForFixedArrayLength()),
        cache_array_false0, effect, if_false0);
  }

  control = graph()->NewNode(common()->Merge(2), if_true0, if_false0);
  effect = graph()->NewNode(common()->EffectPhi(2), etrue0, efalse0, control);
  Node* cache_array =
      graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                       cache_array_true0, cache_array_false0, control);
  Node* cache_length =
      graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                       cache_length_true0, cache_length_false0, control);
  cache_type =
      graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                       cache_type_true0, cache_type_false0, control);

  for (auto edge : node->use_edges()) {
    Node* const use = edge.from();
    if (NodeProperties::IsEffectEdge(edge)) {
      edge.UpdateTo(effect);
      Revisit(use);
    } else {
      if (NodeProperties::IsControlEdge(edge)) {
        if (use->opcode() == IrOpcode::kIfSuccess) {
          Replace(use, control);
        } else if (use->opcode() == IrOpcode::kIfException) {
          edge.UpdateTo(cache_type_true0);
          continue;
        } else {
          UNREACHABLE();
        }
      } else {
        DCHECK(NodeProperties::IsValueEdge(edge));
        DCHECK_EQ(IrOpcode::kProjection, use->opcode());
        switch (ProjectionIndexOf(use->op())) {
          case 0:
            Replace(use, cache_type);
            break;
          case 1:
            Replace(use, cache_array);
            break;
          case 2:
            Replace(use, cache_length);
            break;
          default:
            UNREACHABLE();
            break;
        }
      }
      use->Kill();
    }
  }
  return NoChange();  // All uses were replaced already above.
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
    // Check if the {cache_type} is zero, which indicates proxy.
    Node* check1 = graph()->NewNode(simplified()->ReferenceEqual(Type::Any()),
                                    cache_type, jsgraph()->ZeroConstant());
    Node* branch1 = graph()->NewNode(common()->Branch(BranchHint::kFalse),
                                     check1, if_false0);

    Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
    Node* etrue1;
    Node* vtrue1;
    {
      // Don't do filtering for proxies.
      etrue1 = effect;
      vtrue1 = key;
    }

    Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
    Node* efalse1;
    Node* vfalse1;
    {
      // Filter the {key} to check if it's still a valid property of the
      // {receiver} (does the ToName conversion implicitly).
      vfalse1 = efalse1 = graph()->NewNode(
          javascript()->CallRuntime(Runtime::kForInFilter, 2), receiver, key,
          context, frame_state, effect, if_false1);
      if_false1 = graph()->NewNode(common()->IfSuccess(), vfalse1);
    }

    if_false0 = graph()->NewNode(common()->Merge(2), if_true1, if_false1);
    efalse0 =
        graph()->NewNode(common()->EffectPhi(2), etrue1, efalse1, if_false0);
    vfalse0 = graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                               vtrue1, vfalse1, if_false0);
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
    case IrOpcode::kJSCreate:
      return ReduceJSCreate(node);
    case IrOpcode::kJSCreateArguments:
      return ReduceJSCreateArguments(node);
    case IrOpcode::kJSCreateArray:
      return ReduceJSCreateArray(node);
    case IrOpcode::kJSCreateClosure:
      return ReduceJSCreateClosure(node);
    case IrOpcode::kJSCreateIterResultObject:
      return ReduceJSCreateIterResultObject(node);
    case IrOpcode::kJSCreateLiteralArray:
      return ReduceJSCreateLiteralArray(node);
    case IrOpcode::kJSCreateLiteralObject:
      return ReduceJSCreateLiteralObject(node);
    case IrOpcode::kJSCreateFunctionContext:
      return ReduceJSCreateFunctionContext(node);
    case IrOpcode::kJSCreateWithContext:
      return ReduceJSCreateWithContext(node);
    case IrOpcode::kJSCreateCatchContext:
      return ReduceJSCreateCatchContext(node);
    case IrOpcode::kJSCreateBlockContext:
      return ReduceJSCreateBlockContext(node);
    case IrOpcode::kJSCallConstruct:
      return ReduceJSCallConstruct(node);
    case IrOpcode::kJSCallFunction:
      return ReduceJSCallFunction(node);
    case IrOpcode::kJSForInDone:
      return ReduceJSForInDone(node);
    case IrOpcode::kJSForInNext:
      return ReduceJSForInNext(node);
    case IrOpcode::kJSForInPrepare:
      return ReduceJSForInPrepare(node);
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


// Helper that allocates a FixedArray holding argument values recorded in the
// given {frame_state}. Serves as backing store for JSCreateArguments nodes.
Node* JSTypedLowering::AllocateArguments(Node* effect, Node* control,
                                         Node* frame_state) {
  FrameStateInfo state_info = OpParameter<FrameStateInfo>(frame_state);
  int argument_count = state_info.parameter_count() - 1;  // Minus receiver.
  if (argument_count == 0) return jsgraph()->EmptyFixedArrayConstant();

  // Prepare an iterator over argument values recorded in the frame state.
  Node* const parameters = frame_state->InputAt(kFrameStateParametersInput);
  StateValuesAccess parameters_access(parameters);
  auto parameters_it = ++parameters_access.begin();

  // Actually allocate the backing store.
  AllocationBuilder a(jsgraph(), effect, control);
  a.AllocateArray(argument_count, factory()->fixed_array_map());
  for (int i = 0; i < argument_count; ++i, ++parameters_it) {
    a.Store(AccessBuilder::ForFixedArraySlot(i), (*parameters_it).node);
  }
  return a.Finish();
}


// Helper that allocates a FixedArray holding argument values recorded in the
// given {frame_state}. Serves as backing store for JSCreateArguments nodes.
Node* JSTypedLowering::AllocateRestArguments(Node* effect, Node* control,
                                             Node* frame_state,
                                             int start_index) {
  FrameStateInfo state_info = OpParameter<FrameStateInfo>(frame_state);
  int argument_count = state_info.parameter_count() - 1;  // Minus receiver.
  int num_elements = std::max(0, argument_count - start_index);
  if (num_elements == 0) return jsgraph()->EmptyFixedArrayConstant();

  // Prepare an iterator over argument values recorded in the frame state.
  Node* const parameters = frame_state->InputAt(kFrameStateParametersInput);
  StateValuesAccess parameters_access(parameters);
  auto parameters_it = ++parameters_access.begin();

  // Skip unused arguments.
  for (int i = 0; i < start_index; i++) {
    ++parameters_it;
  }

  // Actually allocate the backing store.
  AllocationBuilder a(jsgraph(), effect, control);
  a.AllocateArray(num_elements, factory()->fixed_array_map());
  for (int i = 0; i < num_elements; ++i, ++parameters_it) {
    a.Store(AccessBuilder::ForFixedArraySlot(i), (*parameters_it).node);
  }
  return a.Finish();
}


// Helper that allocates a FixedArray serving as a parameter map for values
// recorded in the given {frame_state}. Some elements map to slots within the
// given {context}. Serves as backing store for JSCreateArguments nodes.
Node* JSTypedLowering::AllocateAliasedArguments(
    Node* effect, Node* control, Node* frame_state, Node* context,
    Handle<SharedFunctionInfo> shared, bool* has_aliased_arguments) {
  FrameStateInfo state_info = OpParameter<FrameStateInfo>(frame_state);
  int argument_count = state_info.parameter_count() - 1;  // Minus receiver.
  if (argument_count == 0) return jsgraph()->EmptyFixedArrayConstant();

  // If there is no aliasing, the arguments object elements are not special in
  // any way, we can just return an unmapped backing store instead.
  int parameter_count = shared->internal_formal_parameter_count();
  if (parameter_count == 0) {
    return AllocateArguments(effect, control, frame_state);
  }

  // Calculate number of argument values being aliased/mapped.
  int mapped_count = Min(argument_count, parameter_count);
  *has_aliased_arguments = true;

  // Prepare an iterator over argument values recorded in the frame state.
  Node* const parameters = frame_state->InputAt(kFrameStateParametersInput);
  StateValuesAccess parameters_access(parameters);
  auto paratemers_it = ++parameters_access.begin();

  // The unmapped argument values recorded in the frame state are stored yet
  // another indirection away and then linked into the parameter map below,
  // whereas mapped argument values are replaced with a hole instead.
  AllocationBuilder aa(jsgraph(), effect, control);
  aa.AllocateArray(argument_count, factory()->fixed_array_map());
  for (int i = 0; i < mapped_count; ++i, ++paratemers_it) {
    aa.Store(AccessBuilder::ForFixedArraySlot(i), jsgraph()->TheHoleConstant());
  }
  for (int i = mapped_count; i < argument_count; ++i, ++paratemers_it) {
    aa.Store(AccessBuilder::ForFixedArraySlot(i), (*paratemers_it).node);
  }
  Node* arguments = aa.Finish();

  // Actually allocate the backing store.
  AllocationBuilder a(jsgraph(), arguments, control);
  a.AllocateArray(mapped_count + 2, factory()->sloppy_arguments_elements_map());
  a.Store(AccessBuilder::ForFixedArraySlot(0), context);
  a.Store(AccessBuilder::ForFixedArraySlot(1), arguments);
  for (int i = 0; i < mapped_count; ++i) {
    int idx = Context::MIN_CONTEXT_SLOTS + parameter_count - 1 - i;
    a.Store(AccessBuilder::ForFixedArraySlot(i + 2), jsgraph()->Constant(idx));
  }
  return a.Finish();
}


Node* JSTypedLowering::AllocateElements(Node* effect, Node* control,
                                        ElementsKind elements_kind,
                                        int capacity, PretenureFlag pretenure) {
  DCHECK_LE(1, capacity);
  DCHECK_LE(capacity, JSArray::kInitialMaxFastElementArray);

  Handle<Map> elements_map = IsFastDoubleElementsKind(elements_kind)
                                 ? factory()->fixed_double_array_map()
                                 : factory()->fixed_array_map();
  ElementAccess access = IsFastDoubleElementsKind(elements_kind)
                             ? AccessBuilder::ForFixedDoubleArrayElement()
                             : AccessBuilder::ForFixedArrayElement();
  Node* value =
      IsFastDoubleElementsKind(elements_kind)
          ? jsgraph()->Float64Constant(bit_cast<double>(kHoleNanInt64))
          : jsgraph()->TheHoleConstant();

  // Actually allocate the backing store.
  AllocationBuilder a(jsgraph(), effect, control);
  a.AllocateArray(capacity, elements_map, pretenure);
  for (int i = 0; i < capacity; ++i) {
    Node* index = jsgraph()->Constant(i);
    a.Store(access, index, value);
  }
  return a.Finish();
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
