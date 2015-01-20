// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/access-builder.h"
#include "src/compiler/graph-inl.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/js-typed-lowering.h"
#include "src/compiler/node-aux-data-inl.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties-inl.h"
#include "src/types.h"

namespace v8 {
namespace internal {
namespace compiler {

// TODO(turbofan): js-typed-lowering improvements possible
// - immediately put in type bounds for all new nodes
// - relax effects from generic but not-side-effecting operations


// Relax the effects of {node} by immediately replacing effect uses of {node}
// with the effect input to {node}.
// TODO(turbofan): replace the effect input to {node} with {graph->start()}.
// TODO(titzer): move into a GraphEditor?
static void RelaxEffects(Node* node) {
  NodeProperties::ReplaceWithValue(node, node, NULL);
}


JSTypedLowering::JSTypedLowering(JSGraph* jsgraph, Zone* zone)
    : jsgraph_(jsgraph), simplified_(graph()->zone()), conversions_(zone) {
  Handle<Object> zero = factory()->NewNumber(0.0);
  Handle<Object> one = factory()->NewNumber(1.0);
  zero_range_ = Type::Range(zero, zero, graph()->zone());
  one_range_ = Type::Range(one, one, graph()->zone());
  Handle<Object> thirtyone = factory()->NewNumber(31.0);
  zero_thirtyone_range_ = Type::Range(zero, thirtyone, graph()->zone());
  // TODO(jarin): Can we have a correctification of the stupid type system?
  // These stupid work-arounds are just stupid!
  shifted_int32_ranges_[0] = Type::Signed32();
  if (SmiValuesAre31Bits()) {
    shifted_int32_ranges_[1] = Type::SignedSmall();
    for (size_t k = 2; k < arraysize(shifted_int32_ranges_); ++k) {
      Handle<Object> min = factory()->NewNumber(kMinInt / (1 << k));
      Handle<Object> max = factory()->NewNumber(kMaxInt / (1 << k));
      shifted_int32_ranges_[k] = Type::Range(min, max, graph()->zone());
    }
  } else {
    for (size_t k = 1; k < arraysize(shifted_int32_ranges_); ++k) {
      Handle<Object> min = factory()->NewNumber(kMinInt / (1 << k));
      Handle<Object> max = factory()->NewNumber(kMaxInt / (1 << k));
      shifted_int32_ranges_[k] = Type::Range(min, max, graph()->zone());
    }
  }
}


Reduction JSTypedLowering::ReplaceEagerly(Node* old, Node* node) {
  NodeProperties::ReplaceWithValue(old, node, node);
  return Changed(node);
}


// A helper class to simplify the process of reducing a single binop node with a
// JSOperator. This class manages the rewriting of context, control, and effect
// dependencies during lowering of a binop and contains numerous helper
// functions for matching the types of inputs to an operation.
class JSBinopReduction FINAL {
 public:
  JSBinopReduction(JSTypedLowering* lowering, Node* node)
      : lowering_(lowering),
        node_(node),
        left_type_(NodeProperties::GetBounds(node->InputAt(0)).upper),
        right_type_(NodeProperties::GetBounds(node->InputAt(1)).upper) {}

  void ConvertInputsToNumber() {
    node_->ReplaceInput(0, ConvertToNumber(left()));
    node_->ReplaceInput(1, ConvertToNumber(right()));
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
    node_->ReplaceInput(0, ConvertToUI32(left(), left_signedness));
    Node* rnum = ConvertToUI32(right(), kUnsigned);
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
    std::swap(left_type_, right_type_);
  }

  // Remove all effect and control inputs and outputs to this node and change
  // to the pure operator {op}, possibly inserting a boolean inversion.
  Reduction ChangeToPureOperator(const Operator* op, bool invert = false,
                                 Type* type = Type::Any()) {
    DCHECK_EQ(0, op->EffectInputCount());
    DCHECK_EQ(false, OperatorProperties::HasContextInput(op));
    DCHECK_EQ(0, op->ControlInputCount());
    DCHECK_EQ(2, op->ValueInputCount());

    // Remove the effects from the node, if any, and update its effect usages.
    if (node_->op()->EffectInputCount() > 0) {
      RelaxEffects(node_);
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

  bool OneInputIs(Type* t) { return left_type_->Is(t) || right_type_->Is(t); }

  bool BothInputsAre(Type* t) {
    return left_type_->Is(t) && right_type_->Is(t);
  }

  bool OneInputCannotBe(Type* t) {
    return !left_type_->Maybe(t) || !right_type_->Maybe(t);
  }

  bool NeitherInputCanBe(Type* t) {
    return !left_type_->Maybe(t) && !right_type_->Maybe(t);
  }

  Node* effect() { return NodeProperties::GetEffectInput(node_); }
  Node* control() { return NodeProperties::GetControlInput(node_); }
  Node* context() { return NodeProperties::GetContextInput(node_); }
  Node* left() { return NodeProperties::GetValueInput(node_, 0); }
  Node* right() { return NodeProperties::GetValueInput(node_, 1); }
  Type* left_type() { return left_type_; }
  Type* right_type() { return right_type_; }

  SimplifiedOperatorBuilder* simplified() { return lowering_->simplified(); }
  Graph* graph() const { return lowering_->graph(); }
  JSGraph* jsgraph() { return lowering_->jsgraph(); }
  JSOperatorBuilder* javascript() { return lowering_->javascript(); }
  MachineOperatorBuilder* machine() { return lowering_->machine(); }
  Zone* zone() const { return graph()->zone(); }

 private:
  JSTypedLowering* lowering_;  // The containing lowering instance.
  Node* node_;                 // The original node.
  Type* left_type_;            // Cache of the left input's type.
  Type* right_type_;           // Cache of the right input's type.

  Node* ConvertToString(Node* node) {
    // Avoid introducing too many eager ToString() operations.
    Reduction reduced = lowering_->ReduceJSToStringInput(node);
    if (reduced.Changed()) return reduced.replacement();
    Node* n = graph()->NewNode(javascript()->ToString(), node, context(),
                               effect(), control());
    update_effect(n);
    return n;
  }

  Node* ConvertToNumber(Node* node) {
    if (NodeProperties::GetBounds(node).upper->Is(Type::PlainPrimitive())) {
      return lowering_->ConvertToNumber(node);
    }
    Node* n = graph()->NewNode(javascript()->ToNumber(), node, context(),
                               effect(), control());
    update_effect(n);
    return n;
  }

  Node* ConvertToUI32(Node* node, Signedness signedness) {
    // Avoid introducing too many eager NumberToXXnt32() operations.
    node = ConvertToNumber(node);
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
  if (r.BothInputsAre(Type::Primitive()) &&
      r.NeitherInputCanBe(Type::StringOrReceiver())) {
    // JSAdd(x:-string, y:-string) => NumberAdd(ToNumber(x), ToNumber(y))
    r.ConvertInputsToNumber();
    return r.ChangeToPureOperator(simplified()->NumberAdd(), Type::Number());
  }
#if 0
  // TODO(turbofan): General ToNumber disabled for now because:
  //   a) The inserted ToNumber operation screws up observability of valueOf.
  //   b) Deoptimization at ToNumber doesn't have corresponding bailout id.
  Type* maybe_string = Type::Union(Type::String(), Type::Receiver(), zone());
  if (r.NeitherInputCanBe(maybe_string)) {
    ...
  }
#endif
#if 0
  // TODO(turbofan): Lowering of StringAdd is disabled for now because:
  //   a) The inserted ToString operation screws up valueOf vs. toString order.
  //   b) Deoptimization at ToString doesn't have corresponding bailout id.
  //   c) Our current StringAddStub is actually non-pure and requires context.
  if (r.OneInputIs(Type::String())) {
    // JSAdd(x:string, y:string) => StringAdd(x, y)
    // JSAdd(x:string, y) => StringAdd(x, ToString(y))
    // JSAdd(x, y:string) => StringAdd(ToString(x), y)
    r.ConvertInputsToString();
    return r.ChangeToPureOperator(simplified()->StringAdd());
  }
#endif
  return NoChange();
}


Reduction JSTypedLowering::ReduceJSBitwiseOr(Node* node) {
  JSBinopReduction r(this, node);
  if (r.BothInputsAre(Type::Primitive()) || r.OneInputIs(zero_range_)) {
    // TODO(jarin): Propagate frame state input from non-primitive input node to
    // JSToNumber node.
    // TODO(titzer): some Smi bitwise operations don't really require going
    // all the way to int32, which can save tagging/untagging for some
    // operations
    // on some platforms.
    // TODO(turbofan): make this heuristic configurable for code size.
    r.ConvertInputsToUI32(kSigned, kSigned);
    return r.ChangeToPureOperator(machine()->Word32Or(), Type::Integral32());
  }
  return NoChange();
}


Reduction JSTypedLowering::ReduceJSMultiply(Node* node) {
  JSBinopReduction r(this, node);
  if (r.BothInputsAre(Type::Primitive()) || r.OneInputIs(one_range_)) {
    // TODO(jarin): Propagate frame state input from non-primitive input node to
    // JSToNumber node.
    r.ConvertInputsToNumber();
    return r.ChangeToPureOperator(simplified()->NumberMultiply(),
                                  Type::Number());
  }
  // TODO(turbofan): relax/remove the effects of this operator in other cases.
  return NoChange();
}


Reduction JSTypedLowering::ReduceNumberBinop(Node* node,
                                             const Operator* numberOp) {
  JSBinopReduction r(this, node);
  if (r.BothInputsAre(Type::Primitive())) {
    r.ConvertInputsToNumber();
    return r.ChangeToPureOperator(numberOp, Type::Number());
  }
#if 0
  // TODO(turbofan): General ToNumber disabled for now because:
  //   a) The inserted ToNumber operation screws up observability of valueOf.
  //   b) Deoptimization at ToNumber doesn't have corresponding bailout id.
  if (r.OneInputIs(Type::Primitive())) {
    // If at least one input is a primitive, then insert appropriate conversions
    // to number and reduce this operator to the given numeric one.
    // TODO(turbofan): make this heuristic configurable for code size.
    r.ConvertInputsToNumber();
    return r.ChangeToPureOperator(numberOp);
  }
#endif
  // TODO(turbofan): relax/remove the effects of this operator in other cases.
  return NoChange();
}


Reduction JSTypedLowering::ReduceInt32Binop(Node* node, const Operator* intOp) {
  JSBinopReduction r(this, node);
  if (r.BothInputsAre(Type::Primitive())) {
    // TODO(titzer): some Smi bitwise operations don't really require going
    // all the way to int32, which can save tagging/untagging for some
    // operations
    // on some platforms.
    // TODO(turbofan): make this heuristic configurable for code size.
    r.ConvertInputsToUI32(kSigned, kSigned);
    return r.ChangeToPureOperator(intOp, Type::Integral32());
  }
  return NoChange();
}


Reduction JSTypedLowering::ReduceUI32Shift(Node* node,
                                           Signedness left_signedness,
                                           const Operator* shift_op) {
  JSBinopReduction r(this, node);
  if (r.BothInputsAre(Type::Primitive())) {
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
  if (r.BothInputsAre(Type::Primitive()) &&
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
      r.ConvertInputsToNumber();
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
  Node* input = node->InputAt(0);
  Type* input_type = NodeProperties::GetBounds(input).upper;
  if (input_type->Is(Type::Boolean())) {
    // JSUnaryNot(x:boolean,context) => BooleanNot(x)
    node->set_op(simplified()->BooleanNot());
    node->TrimInputCount(1);
    return Changed(node);
  }
  // JSUnaryNot(x,context) => BooleanNot(AnyToBoolean(x))
  node->set_op(simplified()->BooleanNot());
  node->ReplaceInput(0, graph()->NewNode(simplified()->AnyToBoolean(), input));
  node->TrimInputCount(1);
  return Changed(node);
}


Reduction JSTypedLowering::ReduceJSToBoolean(Node* node) {
  Node* input = node->InputAt(0);
  Type* input_type = NodeProperties::GetBounds(input).upper;
  if (input_type->Is(Type::Boolean())) {
    // JSToBoolean(x:boolean,context) => x
    return Replace(input);
  }
  // JSToBoolean(x,context) => AnyToBoolean(x)
  node->set_op(simplified()->AnyToBoolean());
  node->TrimInputCount(1);
  return Changed(node);
}


Reduction JSTypedLowering::ReduceJSToNumberInput(Node* input) {
  if (input->opcode() == IrOpcode::kJSToNumber) {
    // Recursively try to reduce the input first.
    Reduction result = ReduceJSToNumber(input);
    if (result.Changed()) return result;
    return Changed(input);  // JSToNumber(JSToNumber(x)) => JSToNumber(x)
  }
  // Check if we have a cached conversion.
  Node* conversion = FindConversion<IrOpcode::kJSToNumber>(input);
  if (conversion) return Replace(conversion);
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
    if (input->opcode() == IrOpcode::kPhi) {
      // JSToNumber(phi(x1,...,xn,control):plain-primitive,context)
      //   => phi(JSToNumber(x1,no-context),
      //          ...,
      //          JSToNumber(xn,no-context),control)
      int const input_count = input->InputCount() - 1;
      Node* const control = input->InputAt(input_count);
      DCHECK_LE(0, input_count);
      DCHECK(NodeProperties::IsControl(control));
      DCHECK(NodeProperties::GetBounds(node).upper->Is(Type::Number()));
      DCHECK(!NodeProperties::GetBounds(input).upper->Is(Type::Number()));
      RelaxEffects(node);
      node->set_op(common()->Phi(kMachAnyTagged, input_count));
      for (int i = 0; i < input_count; ++i) {
        // We must be very careful not to introduce cycles when pushing
        // operations into phis. It is safe for {value}, since it appears
        // as input to the phi that we are replacing, but it's not safe
        // to simply reuse the context of the {node}. However, ToNumber()
        // does not require a context anyways, so it's safe to discard it
        // here and pass the dummy context.
        Node* const value = ConvertToNumber(input->InputAt(i));
        if (i < node->InputCount()) {
          node->ReplaceInput(i, value);
        } else {
          node->AppendInput(graph()->zone(), value);
        }
      }
      if (input_count < node->InputCount()) {
        node->ReplaceInput(input_count, control);
      } else {
        node->AppendInput(graph()->zone(), control);
      }
      node->TrimInputCount(input_count + 1);
      return Changed(node);
    }
    if (input->opcode() == IrOpcode::kSelect) {
      // JSToNumber(select(c,x1,x2):plain-primitive,context)
      //   => select(c,JSToNumber(x1,no-context),JSToNumber(x2,no-context))
      int const input_count = input->InputCount();
      BranchHint const input_hint = SelectParametersOf(input->op()).hint();
      DCHECK_EQ(3, input_count);
      DCHECK(NodeProperties::GetBounds(node).upper->Is(Type::Number()));
      DCHECK(!NodeProperties::GetBounds(input).upper->Is(Type::Number()));
      RelaxEffects(node);
      node->set_op(common()->Select(kMachAnyTagged, input_hint));
      node->ReplaceInput(0, input->InputAt(0));
      for (int i = 1; i < input_count; ++i) {
        // We must be very careful not to introduce cycles when pushing
        // operations into selects. It is safe for {value}, since it appears
        // as input to the select that we are replacing, but it's not safe
        // to simply reuse the context of the {node}. However, ToNumber()
        // does not require a context anyways, so it's safe to discard it
        // here and pass the dummy context.
        Node* const value = ConvertToNumber(input->InputAt(i));
        node->ReplaceInput(i, value);
      }
      node->TrimInputCount(input_count);
      return Changed(node);
    }
    // Remember this conversion.
    InsertConversion(node);
    if (node->InputAt(1) != jsgraph()->NoContextConstant() ||
        node->InputAt(2) != graph()->start() ||
        node->InputAt(3) != graph()->start()) {
      // JSToNumber(x:plain-primitive,context,effect,control)
      //   => JSToNumber(x,no-context,start,start)
      RelaxEffects(node);
      node->ReplaceInput(1, jsgraph()->NoContextConstant());
      node->ReplaceInput(2, graph()->start());
      node->ReplaceInput(3, graph()->start());
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


Reduction JSTypedLowering::ReduceJSLoadProperty(Node* node) {
  Node* key = NodeProperties::GetValueInput(node, 1);
  Node* base = NodeProperties::GetValueInput(node, 0);
  Type* key_type = NodeProperties::GetBounds(key).upper;
  // TODO(mstarzinger): This lowering is not correct if:
  //   a) The typed array or it's buffer is neutered.
  HeapObjectMatcher<Object> mbase(base);
  if (mbase.HasValue() && mbase.Value().handle()->IsJSTypedArray()) {
    Handle<JSTypedArray> const array =
        Handle<JSTypedArray>::cast(mbase.Value().handle());
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
      if (key_type->Min() >= 0 && key_type->Max() < array->length()->Number()) {
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
  return NoChange();
}


Reduction JSTypedLowering::ReduceJSStoreProperty(Node* node) {
  Node* key = NodeProperties::GetValueInput(node, 1);
  Node* base = NodeProperties::GetValueInput(node, 0);
  Node* value = NodeProperties::GetValueInput(node, 2);
  Type* key_type = NodeProperties::GetBounds(key).upper;
  Type* value_type = NodeProperties::GetBounds(value).upper;
  // TODO(mstarzinger): This lowering is not correct if:
  //   a) The typed array or its buffer is neutered.
  HeapObjectMatcher<Object> mbase(base);
  if (mbase.HasValue() && mbase.Value().handle()->IsJSTypedArray()) {
    Handle<JSTypedArray> const array =
        Handle<JSTypedArray>::cast(mbase.Value().handle());
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
          value = effect = graph()->NewNode(javascript()->ToNumber(), value,
                                            context, effect, control);
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
      if (key_type->Min() >= 0 && key_type->Max() < array->length()->Number()) {
        node->set_op(simplified()->StoreElement(
            AccessBuilder::ForTypedArrayElement(array->type(), true)));
        node->ReplaceInput(0, buffer);
        DCHECK_EQ(key, node->InputAt(1));
        node->ReplaceInput(2, value);
        node->ReplaceInput(3, effect);
        node->ReplaceInput(4, control);
        node->TrimInputCount(5);
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
      DCHECK_EQ(control, node->InputAt(5));
      DCHECK_EQ(6, node->InputCount());
      return Changed(node);
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


Reduction JSTypedLowering::Reduce(Node* node) {
  // Check if the output type is a singleton.  In that case we already know the
  // result value and can simply replace the node if it's eliminable.
  if (NodeProperties::IsTyped(node) &&
      !IrOpcode::IsLeafOpcode(node->opcode()) &&
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
      return ReduceJSBitwiseOr(node);
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
      return ReduceJSMultiply(node);
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
    case IrOpcode::kJSLoadProperty:
      return ReduceJSLoadProperty(node);
    case IrOpcode::kJSStoreProperty:
      return ReduceJSStoreProperty(node);
    case IrOpcode::kJSLoadContext:
      return ReduceJSLoadContext(node);
    case IrOpcode::kJSStoreContext:
      return ReduceJSStoreContext(node);
    default:
      break;
  }
  return NoChange();
}


Node* JSTypedLowering::ConvertToNumber(Node* input) {
  DCHECK(NodeProperties::GetBounds(input).upper->Is(Type::PlainPrimitive()));
  // Avoid inserting too many eager ToNumber() operations.
  Reduction const reduction = ReduceJSToNumberInput(input);
  if (reduction.Changed()) return reduction.replacement();
  Node* const conversion = graph()->NewNode(javascript()->ToNumber(), input,
                                            jsgraph()->NoContextConstant(),
                                            graph()->start(), graph()->start());
  InsertConversion(conversion);
  return conversion;
}


template <IrOpcode::Value kOpcode>
Node* JSTypedLowering::FindConversion(Node* input) {
  size_t const input_id = input->id();
  if (input_id < conversions_.size()) {
    Node* const conversion = conversions_[input_id];
    if (conversion && conversion->opcode() == kOpcode) {
      return conversion;
    }
  }
  return nullptr;
}


void JSTypedLowering::InsertConversion(Node* conversion) {
  DCHECK(conversion->opcode() == IrOpcode::kJSToNumber);
  size_t const input_id = conversion->InputAt(0)->id();
  if (input_id >= conversions_.size()) {
    conversions_.resize(2 * input_id + 1);
  }
  conversions_[input_id] = conversion;
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
