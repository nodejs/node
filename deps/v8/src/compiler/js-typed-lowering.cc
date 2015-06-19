// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/code-factory.h"
#include "src/compiler/access-builder.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/js-typed-lowering.h"
#include "src/compiler/linkage.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/operator-properties.h"
#include "src/types.h"

namespace v8 {
namespace internal {
namespace compiler {

// TODO(turbofan): js-typed-lowering improvements possible
// - immediately put in type bounds for all new nodes
// - relax effects from generic but not-side-effecting operations


// Relax the effects of {node} by immediately replacing effect and control uses
// of {node} with the effect and control input to {node}.
// TODO(turbofan): replace the effect input to {node} with {graph->start()}.
// TODO(titzer): move into a GraphEditor?
static void RelaxEffectsAndControls(Node* node) {
  NodeProperties::ReplaceWithValue(node, node, NULL);
}


// Relax the control uses of {node} by immediately replacing them with the
// control input to {node}.
// TODO(titzer): move into a GraphEditor?
static void RelaxControls(Node* node) {
  NodeProperties::ReplaceWithValue(node, node, node);
}


JSTypedLowering::JSTypedLowering(JSGraph* jsgraph, Zone* zone)
    : jsgraph_(jsgraph), simplified_(graph()->zone()) {
  zero_range_ = Type::Range(0.0, 0.0, graph()->zone());
  one_range_ = Type::Range(1.0, 1.0, graph()->zone());
  zero_thirtyone_range_ = Type::Range(0.0, 31.0, graph()->zone());
  for (size_t k = 0; k < arraysize(shifted_int32_ranges_); ++k) {
    double min = kMinInt / (1 << k);
    double max = kMaxInt / (1 << k);
    shifted_int32_ranges_[k] = Type::Range(min, max, graph()->zone());
  }
}


Reduction JSTypedLowering::ReplaceEagerly(Node* old, Node* node) {
  NodeProperties::ReplaceWithValue(old, node, node);
  return Changed(node);
}


// A helper class to construct inline allocations on the simplified operator
// level. This keeps track of the effect chain for initial stores on a newly
// allocated object and also provides helpers for commonly allocated objects.
class AllocationBuilder final {
 public:
  AllocationBuilder(JSGraph* jsgraph, SimplifiedOperatorBuilder* simplified,
                    Node* effect, Node* control)
      : jsgraph_(jsgraph),
        simplified_(simplified),
        allocation_(nullptr),
        effect_(effect),
        control_(control) {}

  // Primitive allocation of static size.
  void Allocate(int size) {
    allocation_ = graph()->NewNode(
        simplified()->Allocate(), jsgraph()->Constant(size), effect_, control_);
    effect_ = allocation_;
  }

  // Primitive store into a field.
  void Store(const FieldAccess& access, Node* value) {
    effect_ = graph()->NewNode(simplified()->StoreField(access), allocation_,
                               value, effect_, control_);
  }

  // Compound allocation of a FixedArray.
  void AllocateArray(int length, Handle<Map> map) {
    Allocate(FixedArray::SizeFor(length));
    Store(AccessBuilder::ForMap(), map);
    Store(AccessBuilder::ForFixedArrayLength(), jsgraph()->Constant(length));
  }

  // Compound store of a constant into a field.
  void Store(const FieldAccess& access, Handle<Object> value) {
    Store(access, jsgraph()->Constant(value));
  }

  Node* allocation() const { return allocation_; }
  Node* effect() const { return effect_; }

 protected:
  JSGraph* jsgraph() { return jsgraph_; }
  Graph* graph() { return jsgraph_->graph(); }
  SimplifiedOperatorBuilder* simplified() { return simplified_; }

 private:
  JSGraph* const jsgraph_;
  SimplifiedOperatorBuilder* simplified_;
  Node* allocation_;
  Node* effect_;
  Node* control_;
};


// A helper class to simplify the process of reducing a single binop node with a
// JSOperator. This class manages the rewriting of context, control, and effect
// dependencies during lowering of a binop and contains numerous helper
// functions for matching the types of inputs to an operation.
class JSBinopReduction final {
 public:
  JSBinopReduction(JSTypedLowering* lowering, Node* node)
      : lowering_(lowering), node_(node) {}

  void ConvertPrimitiveInputsToNumber() {
    node_->ReplaceInput(0, ConvertPrimitiveToNumber(left()));
    node_->ReplaceInput(1, ConvertPrimitiveToNumber(right()));
  }

  void ConvertInputsToNumber(Node* frame_state) {
    // To convert the inputs to numbers, we have to provide frame states
    // for lazy bailouts in the ToNumber conversions.
    // We use a little hack here: we take the frame state before the binary
    // operation and use it to construct the frame states for the conversion
    // so that after the deoptimization, the binary operation IC gets
    // already converted values from full code. This way we are sure that we
    // will not re-do any of the side effects.

    Node* left_input =
        left_type()->Is(Type::PlainPrimitive())
            ? ConvertPrimitiveToNumber(left())
            : ConvertToNumber(left(),
                              CreateFrameStateForLeftInput(frame_state));

    Node* right_input =
        right_type()->Is(Type::PlainPrimitive())
            ? ConvertPrimitiveToNumber(right())
            : ConvertToNumber(right(), CreateFrameStateForRightInput(
                                           frame_state, left_input));

    node_->ReplaceInput(0, left_input);
    node_->ReplaceInput(1, right_input);
  }

  void ConvertInputsToUI32(Signedness left_signedness,
                           Signedness right_signedness) {
    node_->ReplaceInput(0, ConvertToUI32(left(), left_signedness));
    node_->ReplaceInput(1, ConvertToUI32(right(), right_signedness));
  }

  void ConvertInputsToString() {
    node_->ReplaceInput(0, ConvertToString(left()));
    node_->ReplaceInput(1, ConvertToString(right()));
  }

  // Convert inputs for bitwise shift operation (ES5 spec 11.7).
  void ConvertInputsForShift(Signedness left_signedness) {
    node_->ReplaceInput(
        0, ConvertToUI32(ConvertPrimitiveToNumber(left()), left_signedness));
    Node* rnum = ConvertToUI32(ConvertPrimitiveToNumber(right()), kUnsigned);
    Type* rnum_type = NodeProperties::GetBounds(rnum).upper;
    if (!rnum_type->Is(lowering_->zero_thirtyone_range_)) {
      rnum = graph()->NewNode(machine()->Word32And(), rnum,
                              jsgraph()->Int32Constant(0x1F));
    }
    node_->ReplaceInput(1, rnum);
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
      RelaxEffectsAndControls(node_);
    }
    // Remove the inputs corresponding to context, effect, and control.
    NodeProperties::RemoveNonValueInputs(node_);
    // Finally, update the operator to the new one.
    node_->set_op(op);

    // TODO(jarin): Replace the explicit typing hack with a call to some method
    // that encapsulates changing the operator and re-typing.
    Bounds const bounds = NodeProperties::GetBounds(node_);
    NodeProperties::SetBounds(node_, Bounds::NarrowUpper(bounds, type, zone()));

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

  Reduction ChangeToPureOperator(const Operator* op, Type* type) {
    return ChangeToPureOperator(op, false, type);
  }

  bool IsStrong() { return is_strong(OpParameter<LanguageMode>(node_)); }

  bool OneInputIs(Type* t) { return left_type()->Is(t) || right_type()->Is(t); }

  bool BothInputsAre(Type* t) {
    return left_type()->Is(t) && right_type()->Is(t);
  }

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
  Type* left_type() {
    return NodeProperties::GetBounds(node_->InputAt(0)).upper;
  }
  Type* right_type() {
    return NodeProperties::GetBounds(node_->InputAt(1)).upper;
  }

  SimplifiedOperatorBuilder* simplified() { return lowering_->simplified(); }
  Graph* graph() const { return lowering_->graph(); }
  JSGraph* jsgraph() { return lowering_->jsgraph(); }
  JSOperatorBuilder* javascript() { return lowering_->javascript(); }
  MachineOperatorBuilder* machine() { return lowering_->machine(); }
  Zone* zone() const { return graph()->zone(); }

 private:
  JSTypedLowering* lowering_;  // The containing lowering instance.
  Node* node_;                 // The original node.

  Node* ConvertToString(Node* node) {
    // Avoid introducing too many eager ToString() operations.
    Reduction reduced = lowering_->ReduceJSToStringInput(node);
    if (reduced.Changed()) return reduced.replacement();
    Node* n = graph()->NewNode(javascript()->ToString(), node, context(),
                               effect(), control());
    update_effect(n);
    return n;
  }

  Node* CreateFrameStateForLeftInput(Node* frame_state) {
    FrameStateCallInfo state_info =
        OpParameter<FrameStateCallInfo>(frame_state);

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
        state_info.type(), state_info.bailout_id(),
        OutputFrameStateCombine::PokeAt(1));

    return graph()->NewNode(op, frame_state->InputAt(0),
                            frame_state->InputAt(1), frame_state->InputAt(2),
                            frame_state->InputAt(3), frame_state->InputAt(4));
  }

  Node* CreateFrameStateForRightInput(Node* frame_state, Node* converted_left) {
    FrameStateCallInfo state_info =
        OpParameter<FrameStateCallInfo>(frame_state);

    if (state_info.bailout_id() == BailoutId::None()) {
      // Dummy frame state => just leave it as is.
      return frame_state;
    }

    // Create a frame state that stores the result of the operation to the
    // top of the stack (i.e., the slot used for the right operand).
    const Operator* op = jsgraph()->common()->FrameState(
        state_info.type(), state_info.bailout_id(),
        OutputFrameStateCombine::PokeAt(0));

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

    return graph()->NewNode(op, frame_state->InputAt(0),
                            frame_state->InputAt(1), new_stack,
                            frame_state->InputAt(3), frame_state->InputAt(4));
  }

  Node* ConvertPrimitiveToNumber(Node* node) {
    return lowering_->ConvertPrimitiveToNumber(node);
  }

  Node* ConvertToNumber(Node* node, Node* frame_state) {
    if (NodeProperties::GetBounds(node).upper->Is(Type::PlainPrimitive())) {
      return ConvertPrimitiveToNumber(node);
    } else {
      Node* const n =
          graph()->NewNode(javascript()->ToNumber(), node, context(),
                           frame_state, effect(), control());
      update_effect(n);
      return n;
    }
  }

  Node* ConvertToUI32(Node* node, Signedness signedness) {
    // Avoid introducing too many eager NumberToXXnt32() operations.
    Type* type = NodeProperties::GetBounds(node).upper;
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


Reduction JSTypedLowering::ReduceJSAdd(Node* node) {
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
#if 0
  // TODO(turbofan): Lowering of StringAdd is disabled for now because:
  //   a) The inserted ToString operation screws up valueOf vs. toString order.
  //   b) Deoptimization at ToString doesn't have corresponding bailout id.
  //   c) Our current StringAddStub is actually non-pure and requires context.
  if ((r.OneInputIs(Type::String()) && !r.IsStrong()) ||
      r.BothInputsAre(Type::String())) {
    // JSAdd(x:string, y:string) => StringAdd(x, y)
    // JSAdd(x:string, y) => StringAdd(x, ToString(y))
    // JSAdd(x, y:string) => StringAdd(ToString(x), y)
    r.ConvertInputsToString();
    return r.ChangeToPureOperator(simplified()->StringAdd());
  }
#endif
  return NoChange();
}


Reduction JSTypedLowering::ReduceNumberBinop(Node* node,
                                             const Operator* numberOp) {
  JSBinopReduction r(this, node);
  if (r.IsStrong()) {
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
  JSBinopReduction r(this, node);
  Type* reduce_type = r.IsStrong() ? Type::Number() : Type::Primitive();
  if (r.BothInputsAre(reduce_type)) {
    r.ConvertInputsForShift(left_signedness);
    return r.ChangeToPureOperator(shift_op, Type::Integral32());
  }
  return NoChange();
}


Reduction JSTypedLowering::ReduceJSComparison(Node* node) {
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
    return r.ChangeToPureOperator(stringOp);
  }
#if 0
  // TODO(turbofan): General ToNumber disabled for now because:
  //   a) The inserted ToNumber operation screws up observability of valueOf.
  //   b) Deoptimization at ToNumber doesn't have corresponding bailout id.
  Type* maybe_string = Type::Union(Type::String(), Type::Receiver(), zone());
  if (r.OneInputCannotBe(maybe_string)) {
    // If one input cannot be a string, then emit a number comparison.
    ...
  }
#endif
  if (r.BothInputsAre(Type::PlainPrimitive()) &&
      r.OneInputCannotBe(Type::StringOrReceiver())) {
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
      r.ConvertPrimitiveInputsToNumber();
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
  JSBinopReduction r(this, node);

  if (r.BothInputsAre(Type::Number())) {
    return r.ChangeToPureOperator(simplified()->NumberEqual(), invert);
  }
  if (r.BothInputsAre(Type::String())) {
    return r.ChangeToPureOperator(simplified()->StringEqual(), invert);
  }
  if (r.BothInputsAre(Type::Receiver())) {
    return r.ChangeToPureOperator(
        simplified()->ReferenceEqual(Type::Receiver()), invert);
  }
  // TODO(turbofan): js-typed-lowering of Equal(undefined)
  // TODO(turbofan): js-typed-lowering of Equal(null)
  // TODO(turbofan): js-typed-lowering of Equal(boolean)
  return NoChange();
}


Reduction JSTypedLowering::ReduceJSStrictEqual(Node* node, bool invert) {
  JSBinopReduction r(this, node);
  if (r.left() == r.right()) {
    // x === x is always true if x != NaN
    if (!r.left_type()->Maybe(Type::NaN())) {
      return ReplaceEagerly(node, jsgraph()->BooleanConstant(!invert));
    }
  }
  if (r.OneInputCannotBe(Type::NumberOrString())) {
    // For values with canonical representation (i.e. not string nor number) an
    // empty type intersection means the values cannot be strictly equal.
    if (!r.left_type()->Maybe(r.right_type())) {
      return ReplaceEagerly(node, jsgraph()->BooleanConstant(invert));
    }
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
  if (r.BothInputsAre(Type::String())) {
    return r.ChangeToPureOperator(simplified()->StringEqual(), invert);
  }
  if (r.BothInputsAre(Type::Number())) {
    return r.ChangeToPureOperator(simplified()->NumberEqual(), invert);
  }
  // TODO(turbofan): js-typed-lowering of StrictEqual(mixed types)
  return NoChange();
}


Reduction JSTypedLowering::ReduceJSUnaryNot(Node* node) {
  Node* const input = node->InputAt(0);
  Type* const input_type = NodeProperties::GetBounds(input).upper;
  if (input_type->Is(Type::Boolean())) {
    // JSUnaryNot(x:boolean) => BooleanNot(x)
    node->set_op(simplified()->BooleanNot());
    node->TrimInputCount(1);
    return Changed(node);
  } else if (input_type->Is(Type::OrderedNumber())) {
    // JSUnaryNot(x:number) => NumberEqual(x,#0)
    node->set_op(simplified()->NumberEqual());
    node->ReplaceInput(1, jsgraph()->ZeroConstant());
    DCHECK_EQ(2, node->InputCount());
    return Changed(node);
  } else if (input_type->Is(Type::String())) {
    // JSUnaryNot(x:string) => NumberEqual(x.length,#0)
    FieldAccess const access = AccessBuilder::ForStringLength(graph()->zone());
    // It is safe for the load to be effect-free (i.e. not linked into effect
    // chain) because we assume String::length to be immutable.
    Node* length = graph()->NewNode(simplified()->LoadField(access), input,
                                    graph()->start(), graph()->start());
    node->set_op(simplified()->NumberEqual());
    node->ReplaceInput(0, length);
    node->ReplaceInput(1, jsgraph()->ZeroConstant());
    NodeProperties::ReplaceWithValue(node, node, length);
    DCHECK_EQ(2, node->InputCount());
    return Changed(node);
  }
  return NoChange();
}


Reduction JSTypedLowering::ReduceJSToBoolean(Node* node) {
  Node* const input = node->InputAt(0);
  Type* const input_type = NodeProperties::GetBounds(input).upper;
  if (input_type->Is(Type::Boolean())) {
    // JSToBoolean(x:boolean) => x
    return Replace(input);
  } else if (input_type->Is(Type::OrderedNumber())) {
    // JSToBoolean(x:ordered-number) => BooleanNot(NumberEqual(x,#0))
    node->set_op(simplified()->BooleanNot());
    node->ReplaceInput(0, graph()->NewNode(simplified()->NumberEqual(), input,
                                           jsgraph()->ZeroConstant()));
    node->TrimInputCount(1);
    return Changed(node);
  } else if (input_type->Is(Type::String())) {
    // JSToBoolean(x:string) => NumberLessThan(#0,x.length)
    FieldAccess const access = AccessBuilder::ForStringLength(graph()->zone());
    // It is safe for the load to be effect-free (i.e. not linked into effect
    // chain) because we assume String::length to be immutable.
    Node* length = graph()->NewNode(simplified()->LoadField(access), input,
                                    graph()->start(), graph()->start());
    node->set_op(simplified()->NumberLessThan());
    node->ReplaceInput(0, jsgraph()->ZeroConstant());
    node->ReplaceInput(1, length);
    DCHECK_EQ(2, node->InputCount());
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
  // Check if we have a cached conversion.
  Type* input_type = NodeProperties::GetBounds(input).upper;
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
    NodeProperties::ReplaceWithValue(node, reduction.replacement());
    return reduction;
  }
  Type* const input_type = NodeProperties::GetBounds(input).upper;
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
  Type* input_type = NodeProperties::GetBounds(input).upper;
  if (input_type->Is(Type::String())) {
    return Changed(input);  // JSToString(x:string) => x
  }
  if (input_type->Is(Type::Undefined())) {
    return Replace(jsgraph()->HeapConstant(factory()->undefined_string()));
  }
  if (input_type->Is(Type::Null())) {
    return Replace(jsgraph()->HeapConstant(factory()->null_string()));
  }
  // TODO(turbofan): js-typed-lowering of ToString(x:boolean)
  // TODO(turbofan): js-typed-lowering of ToString(x:number)
  return NoChange();
}


Reduction JSTypedLowering::ReduceJSToString(Node* node) {
  // Try to reduce the input first.
  Node* const input = node->InputAt(0);
  Reduction reduction = ReduceJSToStringInput(input);
  if (reduction.Changed()) {
    NodeProperties::ReplaceWithValue(node, reduction.replacement());
    return reduction;
  }
  return NoChange();
}


Reduction JSTypedLowering::ReduceJSLoadNamed(Node* node) {
  Node* object = NodeProperties::GetValueInput(node, 0);
  Type* object_type = NodeProperties::GetBounds(object).upper;
  if (object_type->Is(Type::GlobalObject())) {
    // Optimize global constants like "undefined", "Infinity", and "NaN".
    Handle<Name> name = LoadNamedParametersOf(node->op()).name().handle();
    Handle<Object> constant_value = factory()->GlobalConstantFor(name);
    if (!constant_value.is_null()) {
      Node* constant = jsgraph()->Constant(constant_value);
      NodeProperties::ReplaceWithValue(node, constant);
      return Replace(constant);
    }
  }
  return NoChange();
}


Reduction JSTypedLowering::ReduceJSLoadProperty(Node* node) {
  Node* key = NodeProperties::GetValueInput(node, 1);
  Node* base = NodeProperties::GetValueInput(node, 0);
  Type* key_type = NodeProperties::GetBounds(key).upper;
  HeapObjectMatcher<Object> mbase(base);
  if (mbase.HasValue() && mbase.Value().handle()->IsJSTypedArray()) {
    Handle<JSTypedArray> const array =
        Handle<JSTypedArray>::cast(mbase.Value().handle());
    if (!array->GetBuffer()->was_neutered()) {
      array->GetBuffer()->set_is_neuterable(false);
      BufferAccess const access(array->type());
      size_t const k = ElementSizeLog2Of(access.machine_type());
      double const byte_length = array->byte_length()->Number();
      CHECK_LT(k, arraysize(shifted_int32_ranges_));
      if (IsExternalArrayElementsKind(array->map()->elements_kind()) &&
          key_type->Is(shifted_int32_ranges_[k]) && byte_length <= kMaxInt) {
        // JSLoadProperty(typed-array, int32)
        Handle<ExternalArray> elements =
            Handle<ExternalArray>::cast(handle(array->elements()));
        Node* buffer = jsgraph()->PointerConstant(elements->external_pointer());
        Node* length = jsgraph()->Constant(byte_length);
        Node* effect = NodeProperties::GetEffectInput(node);
        Node* control = NodeProperties::GetControlInput(node);
        // Check if we can avoid the bounds check.
        if (key_type->Min() >= 0 &&
            key_type->Max() < array->length()->Number()) {
          Node* load = graph()->NewNode(
              simplified()->LoadElement(
                  AccessBuilder::ForTypedArrayElement(array->type(), true)),
              buffer, key, effect, control);
          return ReplaceEagerly(node, load);
        }
        // Compute byte offset.
        Node* offset = Word32Shl(key, static_cast<int>(k));
        Node* load = graph()->NewNode(simplified()->LoadBuffer(access), buffer,
                                      offset, length, effect, control);
        return ReplaceEagerly(node, load);
      }
    }
  }
  return NoChange();
}


Reduction JSTypedLowering::ReduceJSStoreProperty(Node* node) {
  Node* key = NodeProperties::GetValueInput(node, 1);
  Node* base = NodeProperties::GetValueInput(node, 0);
  Node* value = NodeProperties::GetValueInput(node, 2);
  Type* key_type = NodeProperties::GetBounds(key).upper;
  Type* value_type = NodeProperties::GetBounds(value).upper;
  HeapObjectMatcher<Object> mbase(base);
  if (mbase.HasValue() && mbase.Value().handle()->IsJSTypedArray()) {
    Handle<JSTypedArray> const array =
        Handle<JSTypedArray>::cast(mbase.Value().handle());
    if (!array->GetBuffer()->was_neutered()) {
      array->GetBuffer()->set_is_neuterable(false);
      BufferAccess const access(array->type());
      size_t const k = ElementSizeLog2Of(access.machine_type());
      double const byte_length = array->byte_length()->Number();
      CHECK_LT(k, arraysize(shifted_int32_ranges_));
      if (IsExternalArrayElementsKind(array->map()->elements_kind()) &&
          access.external_array_type() != kExternalUint8ClampedArray &&
          key_type->Is(shifted_int32_ranges_[k]) && byte_length <= kMaxInt) {
        // JSLoadProperty(typed-array, int32)
        Handle<ExternalArray> elements =
            Handle<ExternalArray>::cast(handle(array->elements()));
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
        // For integer-typed arrays, convert to the integer type.
        if (TypeOf(access.machine_type()) == kTypeInt32 &&
            !value_type->Is(Type::Signed32())) {
          value = graph()->NewNode(simplified()->NumberToInt32(), value);
        } else if (TypeOf(access.machine_type()) == kTypeUint32 &&
                   !value_type->Is(Type::Unsigned32())) {
          value = graph()->NewNode(simplified()->NumberToUint32(), value);
        }
        // Check if we can avoid the bounds check.
        if (key_type->Min() >= 0 &&
            key_type->Max() < array->length()->Number()) {
          node->set_op(simplified()->StoreElement(
              AccessBuilder::ForTypedArrayElement(array->type(), true)));
          node->ReplaceInput(0, buffer);
          DCHECK_EQ(key, node->InputAt(1));
          node->ReplaceInput(2, value);
          node->ReplaceInput(3, effect);
          node->ReplaceInput(4, control);
          node->TrimInputCount(5);
          RelaxControls(node);
          return Changed(node);
        }
        // Compute byte offset.
        Node* offset = Word32Shl(key, static_cast<int>(k));
        // Turn into a StoreBuffer operation.
        node->set_op(simplified()->StoreBuffer(access));
        node->ReplaceInput(0, buffer);
        node->ReplaceInput(1, offset);
        node->ReplaceInput(2, length);
        node->ReplaceInput(3, value);
        node->ReplaceInput(4, effect);
        node->ReplaceInput(5, control);
        node->TrimInputCount(6);
        RelaxControls(node);
        return Changed(node);
      }
    }
  }
  return NoChange();
}


Reduction JSTypedLowering::ReduceJSLoadContext(Node* node) {
  DCHECK_EQ(IrOpcode::kJSLoadContext, node->opcode());
  ContextAccess const& access = ContextAccessOf(node->op());
  Node* const effect = NodeProperties::GetEffectInput(node);
  Node* const control = graph()->start();
  for (size_t i = 0; i < access.depth(); ++i) {
    node->ReplaceInput(
        0, graph()->NewNode(
               simplified()->LoadField(
                   AccessBuilder::ForContextSlot(Context::PREVIOUS_INDEX)),
               NodeProperties::GetValueInput(node, 0), effect, control));
  }
  node->set_op(
      simplified()->LoadField(AccessBuilder::ForContextSlot(access.index())));
  node->ReplaceInput(1, effect);
  node->ReplaceInput(2, control);
  DCHECK_EQ(3, node->InputCount());
  return Changed(node);
}


Reduction JSTypedLowering::ReduceJSStoreContext(Node* node) {
  DCHECK_EQ(IrOpcode::kJSStoreContext, node->opcode());
  ContextAccess const& access = ContextAccessOf(node->op());
  Node* const effect = NodeProperties::GetEffectInput(node);
  Node* const control = graph()->start();
  for (size_t i = 0; i < access.depth(); ++i) {
    node->ReplaceInput(
        0, graph()->NewNode(
               simplified()->LoadField(
                   AccessBuilder::ForContextSlot(Context::PREVIOUS_INDEX)),
               NodeProperties::GetValueInput(node, 0), effect, control));
  }
  node->set_op(
      simplified()->StoreField(AccessBuilder::ForContextSlot(access.index())));
  node->RemoveInput(2);
  DCHECK_EQ(4, node->InputCount());
  return Changed(node);
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
    node->ReplaceInput(0, jsgraph()->HeapConstant(shared));
    node->InsertInput(graph()->zone(), 0, stub_code);
    node->set_op(new_op);
    return Changed(node);
  }

  return NoChange();
}


Reduction JSTypedLowering::ReduceJSCreateLiteralArray(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCreateLiteralArray, node->opcode());
  HeapObjectMatcher<FixedArray> mconst(NodeProperties::GetValueInput(node, 2));
  int length = mconst.Value().handle()->length();
  int flags = OpParameter<int>(node->op());

  // Use the FastCloneShallowArrayStub only for shallow boilerplates up to the
  // initial length limit for arrays with "fast" elements kind.
  if ((flags & ArrayLiteral::kShallowElements) != 0 &&
      length < JSObject::kInitialMaxFastElementArray) {
    Isolate* isolate = jsgraph()->isolate();
    Callable callable = CodeFactory::FastCloneShallowArray(isolate);
    CallDescriptor* desc = Linkage::GetStubCallDescriptor(
        isolate, graph()->zone(), callable.descriptor(), 0,
        (OperatorProperties::GetFrameStateInputCount(node->op()) != 0)
            ? CallDescriptor::kNeedsFrameState
            : CallDescriptor::kNoFlags);
    const Operator* new_op = common()->Call(desc);
    Node* stub_code = jsgraph()->HeapConstant(callable.code());
    node->InsertInput(graph()->zone(), 0, stub_code);
    node->set_op(new_op);
    return Changed(node);
  }

  return NoChange();
}


Reduction JSTypedLowering::ReduceJSCreateLiteralObject(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCreateLiteralObject, node->opcode());
  HeapObjectMatcher<FixedArray> mconst(NodeProperties::GetValueInput(node, 2));
  // Constants are pairs, see ObjectLiteral::properties_count().
  int length = mconst.Value().handle()->length() / 2;
  int flags = OpParameter<int>(node->op());

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
    node->InsertInput(graph()->zone(), 3, jsgraph()->Constant(flags));
    node->InsertInput(graph()->zone(), 0, stub_code);
    node->set_op(new_op);
    return Changed(node);
  }

  return NoChange();
}


Reduction JSTypedLowering::ReduceJSCreateWithContext(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCreateWithContext, node->opcode());
  Node* const input = NodeProperties::GetValueInput(node, 0);
  Type* input_type = NodeProperties::GetBounds(input).upper;
  if (FLAG_turbo_allocate && input_type->Is(Type::Receiver())) {
    // JSCreateWithContext(o:receiver, f)
    Node* const effect = NodeProperties::GetEffectInput(node);
    Node* const control = NodeProperties::GetControlInput(node);
    Node* const closure = NodeProperties::GetValueInput(node, 1);
    Node* const context = NodeProperties::GetContextInput(node);
    Node* const load = graph()->NewNode(
        simplified()->LoadField(
            AccessBuilder::ForContextSlot(Context::GLOBAL_OBJECT_INDEX)),
        context, effect, control);
    AllocationBuilder a(jsgraph(), simplified(), effect, control);
    STATIC_ASSERT(Context::MIN_CONTEXT_SLOTS == 4);  // Ensure fully covered.
    a.AllocateArray(Context::MIN_CONTEXT_SLOTS, factory()->with_context_map());
    a.Store(AccessBuilder::ForContextSlot(Context::CLOSURE_INDEX), closure);
    a.Store(AccessBuilder::ForContextSlot(Context::PREVIOUS_INDEX), context);
    a.Store(AccessBuilder::ForContextSlot(Context::EXTENSION_INDEX), input);
    a.Store(AccessBuilder::ForContextSlot(Context::GLOBAL_OBJECT_INDEX), load);
    // TODO(mstarzinger): We could mutate {node} into the allocation instead.
    NodeProperties::SetBounds(a.allocation(), NodeProperties::GetBounds(node));
    NodeProperties::ReplaceWithValue(node, node, a.effect());
    node->ReplaceInput(0, a.allocation());
    node->ReplaceInput(1, a.effect());
    node->set_op(common()->Finish(1));
    node->TrimInputCount(2);
    return Changed(node);
  }
  return NoChange();
}


Reduction JSTypedLowering::ReduceJSCreateBlockContext(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCreateBlockContext, node->opcode());
  Node* const input = NodeProperties::GetValueInput(node, 0);
  HeapObjectMatcher<ScopeInfo> minput(input);
  DCHECK(minput.HasValue());  // TODO(mstarzinger): Make ScopeInfo static.
  int context_length = minput.Value().handle()->ContextLength();
  if (FLAG_turbo_allocate && context_length < kBlockContextAllocationLimit) {
    // JSCreateBlockContext(s:scope[length < limit], f)
    Node* const effect = NodeProperties::GetEffectInput(node);
    Node* const control = NodeProperties::GetControlInput(node);
    Node* const closure = NodeProperties::GetValueInput(node, 1);
    Node* const context = NodeProperties::GetContextInput(node);
    Node* const load = graph()->NewNode(
        simplified()->LoadField(
            AccessBuilder::ForContextSlot(Context::GLOBAL_OBJECT_INDEX)),
        context, effect, control);
    AllocationBuilder a(jsgraph(), simplified(), effect, control);
    STATIC_ASSERT(Context::MIN_CONTEXT_SLOTS == 4);  // Ensure fully covered.
    a.AllocateArray(context_length, factory()->block_context_map());
    a.Store(AccessBuilder::ForContextSlot(Context::CLOSURE_INDEX), closure);
    a.Store(AccessBuilder::ForContextSlot(Context::PREVIOUS_INDEX), context);
    a.Store(AccessBuilder::ForContextSlot(Context::EXTENSION_INDEX), input);
    a.Store(AccessBuilder::ForContextSlot(Context::GLOBAL_OBJECT_INDEX), load);
    for (int i = Context::MIN_CONTEXT_SLOTS; i < context_length; ++i) {
      a.Store(AccessBuilder::ForContextSlot(i), jsgraph()->TheHoleConstant());
    }
    // TODO(mstarzinger): We could mutate {node} into the allocation instead.
    NodeProperties::SetBounds(a.allocation(), NodeProperties::GetBounds(node));
    NodeProperties::ReplaceWithValue(node, node, a.effect());
    node->ReplaceInput(0, a.allocation());
    node->ReplaceInput(1, a.effect());
    node->set_op(common()->Finish(1));
    node->TrimInputCount(2);
    return Changed(node);
  }
  return NoChange();
}


Reduction JSTypedLowering::Reduce(Node* node) {
  // Check if the output type is a singleton.  In that case we already know the
  // result value and can simply replace the node if it's eliminable.
  if (!NodeProperties::IsConstant(node) && NodeProperties::IsTyped(node) &&
      node->op()->HasProperty(Operator::kEliminatable)) {
    Type* upper = NodeProperties::GetBounds(node).upper;
    if (upper->IsConstant()) {
      Node* replacement = jsgraph()->Constant(upper->AsConstant()->Value());
      NodeProperties::ReplaceWithValue(node, replacement);
      return Changed(replacement);
    } else if (upper->Is(Type::MinusZero())) {
      Node* replacement = jsgraph()->Constant(factory()->minus_zero_value());
      NodeProperties::ReplaceWithValue(node, replacement);
      return Changed(replacement);
    } else if (upper->Is(Type::NaN())) {
      Node* replacement = jsgraph()->NaNConstant();
      NodeProperties::ReplaceWithValue(node, replacement);
      return Changed(replacement);
    } else if (upper->Is(Type::Null())) {
      Node* replacement = jsgraph()->NullConstant();
      NodeProperties::ReplaceWithValue(node, replacement);
      return Changed(replacement);
    } else if (upper->Is(Type::PlainNumber()) && upper->Min() == upper->Max()) {
      Node* replacement = jsgraph()->Constant(upper->Min());
      NodeProperties::ReplaceWithValue(node, replacement);
      return Changed(replacement);
    } else if (upper->Is(Type::Undefined())) {
      Node* replacement = jsgraph()->UndefinedConstant();
      NodeProperties::ReplaceWithValue(node, replacement);
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
      return ReduceInt32Binop(node, machine()->Word32Or());
    case IrOpcode::kJSBitwiseXor:
      return ReduceInt32Binop(node, machine()->Word32Xor());
    case IrOpcode::kJSBitwiseAnd:
      return ReduceInt32Binop(node, machine()->Word32And());
    case IrOpcode::kJSShiftLeft:
      return ReduceUI32Shift(node, kSigned, machine()->Word32Shl());
    case IrOpcode::kJSShiftRight:
      return ReduceUI32Shift(node, kSigned, machine()->Word32Sar());
    case IrOpcode::kJSShiftRightLogical:
      return ReduceUI32Shift(node, kUnsigned, machine()->Word32Shr());
    case IrOpcode::kJSAdd:
      return ReduceJSAdd(node);
    case IrOpcode::kJSSubtract:
      return ReduceNumberBinop(node, simplified()->NumberSubtract());
    case IrOpcode::kJSMultiply:
      return ReduceNumberBinop(node, simplified()->NumberMultiply());
    case IrOpcode::kJSDivide:
      return ReduceNumberBinop(node, simplified()->NumberDivide());
    case IrOpcode::kJSModulus:
      return ReduceNumberBinop(node, simplified()->NumberModulus());
    case IrOpcode::kJSUnaryNot:
      return ReduceJSUnaryNot(node);
    case IrOpcode::kJSToBoolean:
      return ReduceJSToBoolean(node);
    case IrOpcode::kJSToNumber:
      return ReduceJSToNumber(node);
    case IrOpcode::kJSToString:
      return ReduceJSToString(node);
    case IrOpcode::kJSLoadNamed:
      return ReduceJSLoadNamed(node);
    case IrOpcode::kJSLoadProperty:
      return ReduceJSLoadProperty(node);
    case IrOpcode::kJSStoreProperty:
      return ReduceJSStoreProperty(node);
    case IrOpcode::kJSLoadContext:
      return ReduceJSLoadContext(node);
    case IrOpcode::kJSStoreContext:
      return ReduceJSStoreContext(node);
    case IrOpcode::kJSCreateClosure:
      return ReduceJSCreateClosure(node);
    case IrOpcode::kJSCreateLiteralArray:
      return ReduceJSCreateLiteralArray(node);
    case IrOpcode::kJSCreateLiteralObject:
      return ReduceJSCreateLiteralObject(node);
    case IrOpcode::kJSCreateWithContext:
      return ReduceJSCreateWithContext(node);
    case IrOpcode::kJSCreateBlockContext:
      return ReduceJSCreateBlockContext(node);
    default:
      break;
  }
  return NoChange();
}


Node* JSTypedLowering::ConvertPrimitiveToNumber(Node* input) {
  DCHECK(NodeProperties::GetBounds(input).upper->Is(Type::PlainPrimitive()));
  // Avoid inserting too many eager ToNumber() operations.
  Reduction const reduction = ReduceJSToNumberInput(input);
  if (reduction.Changed()) return reduction.replacement();
  // TODO(jarin) Use PlainPrimitiveToNumber once we have it.
  return graph()->NewNode(
      javascript()->ToNumber(), input, jsgraph()->NoContextConstant(),
      jsgraph()->EmptyFrameState(), graph()->start(), graph()->start());
}


Node* JSTypedLowering::Word32Shl(Node* const lhs, int32_t const rhs) {
  if (rhs == 0) return lhs;
  return graph()->NewNode(machine()->Word32Shl(), lhs,
                          jsgraph()->Int32Constant(rhs));
}


Factory* JSTypedLowering::factory() const { return jsgraph()->factory(); }


Graph* JSTypedLowering::graph() const { return jsgraph()->graph(); }


JSOperatorBuilder* JSTypedLowering::javascript() const {
  return jsgraph()->javascript();
}


CommonOperatorBuilder* JSTypedLowering::common() const {
  return jsgraph()->common();
}


MachineOperatorBuilder* JSTypedLowering::machine() const {
  return jsgraph()->machine();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
