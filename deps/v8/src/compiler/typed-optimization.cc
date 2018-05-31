// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/typed-optimization.h"

#include "src/compilation-dependencies.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/simplified-operator.h"
#include "src/compiler/type-cache.h"
#include "src/isolate-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

TypedOptimization::TypedOptimization(Editor* editor,
                                     CompilationDependencies* dependencies,
                                     JSGraph* jsgraph)
    : AdvancedReducer(editor),
      dependencies_(dependencies),
      jsgraph_(jsgraph),
      true_type_(Type::HeapConstant(factory()->true_value(), graph()->zone())),
      false_type_(
          Type::HeapConstant(factory()->false_value(), graph()->zone())),
      type_cache_(TypeCache::Get()) {}

TypedOptimization::~TypedOptimization() {}

Reduction TypedOptimization::Reduce(Node* node) {
  // Check if the output type is a singleton.  In that case we already know the
  // result value and can simply replace the node if it's eliminable.
  if (!NodeProperties::IsConstant(node) && NodeProperties::IsTyped(node) &&
      node->op()->HasProperty(Operator::kEliminatable)) {
    // TODO(v8:5303): We must not eliminate FinishRegion here. This special
    // case can be removed once we have separate operators for value and
    // effect regions.
    if (node->opcode() == IrOpcode::kFinishRegion) return NoChange();
    // We can only constant-fold nodes here, that are known to not cause any
    // side-effect, may it be a JavaScript observable side-effect or a possible
    // eager deoptimization exit (i.e. {node} has an operator that doesn't have
    // the Operator::kNoDeopt property).
    Type* upper = NodeProperties::GetType(node);
    if (!upper->IsNone()) {
      if (upper->IsHeapConstant()) {
        Node* replacement =
            jsgraph()->Constant(upper->AsHeapConstant()->Value());
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
      } else if (upper->Is(Type::PlainNumber()) &&
                 upper->Min() == upper->Max()) {
        Node* replacement = jsgraph()->Constant(upper->Min());
        ReplaceWithValue(node, replacement);
        return Changed(replacement);
      } else if (upper->Is(Type::Undefined())) {
        Node* replacement = jsgraph()->UndefinedConstant();
        ReplaceWithValue(node, replacement);
        return Changed(replacement);
      }
    }
  }
  switch (node->opcode()) {
    case IrOpcode::kConvertReceiver:
      return ReduceConvertReceiver(node);
    case IrOpcode::kCheckHeapObject:
      return ReduceCheckHeapObject(node);
    case IrOpcode::kCheckNotTaggedHole:
      return ReduceCheckNotTaggedHole(node);
    case IrOpcode::kCheckMaps:
      return ReduceCheckMaps(node);
    case IrOpcode::kCheckNumber:
      return ReduceCheckNumber(node);
    case IrOpcode::kCheckString:
      return ReduceCheckString(node);
    case IrOpcode::kCheckEqualsInternalizedString:
      return ReduceCheckEqualsInternalizedString(node);
    case IrOpcode::kCheckEqualsSymbol:
      return ReduceCheckEqualsSymbol(node);
    case IrOpcode::kLoadField:
      return ReduceLoadField(node);
    case IrOpcode::kNumberCeil:
    case IrOpcode::kNumberRound:
    case IrOpcode::kNumberTrunc:
      return ReduceNumberRoundop(node);
    case IrOpcode::kNumberFloor:
      return ReduceNumberFloor(node);
    case IrOpcode::kNumberToUint8Clamped:
      return ReduceNumberToUint8Clamped(node);
    case IrOpcode::kPhi:
      return ReducePhi(node);
    case IrOpcode::kReferenceEqual:
      return ReduceReferenceEqual(node);
    case IrOpcode::kStringEqual:
    case IrOpcode::kStringLessThan:
    case IrOpcode::kStringLessThanOrEqual:
      return ReduceStringComparison(node);
    case IrOpcode::kSameValue:
      return ReduceSameValue(node);
    case IrOpcode::kSelect:
      return ReduceSelect(node);
    case IrOpcode::kTypeOf:
      return ReduceTypeOf(node);
    case IrOpcode::kToBoolean:
      return ReduceToBoolean(node);
    case IrOpcode::kSpeculativeToNumber:
      return ReduceSpeculativeToNumber(node);
    default:
      break;
  }
  return NoChange();
}

namespace {

MaybeHandle<Map> GetStableMapFromObjectType(Type* object_type) {
  if (object_type->IsHeapConstant()) {
    Handle<Map> object_map(object_type->AsHeapConstant()->Value()->map());
    if (object_map->is_stable()) return object_map;
  }
  return MaybeHandle<Map>();
}

}  // namespace

Reduction TypedOptimization::ReduceConvertReceiver(Node* node) {
  Node* const value = NodeProperties::GetValueInput(node, 0);
  Type* const value_type = NodeProperties::GetType(value);
  Node* const global_proxy = NodeProperties::GetValueInput(node, 1);
  if (value_type->Is(Type::Receiver())) {
    ReplaceWithValue(node, value);
    return Replace(value);
  } else if (value_type->Is(Type::NullOrUndefined())) {
    ReplaceWithValue(node, global_proxy);
    return Replace(global_proxy);
  }
  return NoChange();
}

Reduction TypedOptimization::ReduceCheckHeapObject(Node* node) {
  Node* const input = NodeProperties::GetValueInput(node, 0);
  Type* const input_type = NodeProperties::GetType(input);
  if (!input_type->Maybe(Type::SignedSmall())) {
    ReplaceWithValue(node, input);
    return Replace(input);
  }
  return NoChange();
}

Reduction TypedOptimization::ReduceCheckNotTaggedHole(Node* node) {
  Node* const input = NodeProperties::GetValueInput(node, 0);
  Type* const input_type = NodeProperties::GetType(input);
  if (!input_type->Maybe(Type::Hole())) {
    ReplaceWithValue(node, input);
    return Replace(input);
  }
  return NoChange();
}

Reduction TypedOptimization::ReduceCheckMaps(Node* node) {
  // The CheckMaps(o, ...map...) can be eliminated if map is stable,
  // o has type Constant(object) and map == object->map, and either
  //  (1) map cannot transition further, or
  //  (2) we can add a code dependency on the stability of map
  //      (to guard the Constant type information).
  Node* const object = NodeProperties::GetValueInput(node, 0);
  Type* const object_type = NodeProperties::GetType(object);
  Node* const effect = NodeProperties::GetEffectInput(node);
  Handle<Map> object_map;
  if (GetStableMapFromObjectType(object_type).ToHandle(&object_map)) {
    for (int i = 1; i < node->op()->ValueInputCount(); ++i) {
      Node* const map = NodeProperties::GetValueInput(node, i);
      Type* const map_type = NodeProperties::GetType(map);
      if (map_type->IsHeapConstant() &&
          map_type->AsHeapConstant()->Value().is_identical_to(object_map)) {
        if (object_map->CanTransition()) {
          dependencies()->AssumeMapStable(object_map);
        }
        return Replace(effect);
      }
    }
  }
  return NoChange();
}

Reduction TypedOptimization::ReduceCheckNumber(Node* node) {
  Node* const input = NodeProperties::GetValueInput(node, 0);
  Type* const input_type = NodeProperties::GetType(input);
  if (input_type->Is(Type::Number())) {
    ReplaceWithValue(node, input);
    return Replace(input);
  }
  return NoChange();
}

Reduction TypedOptimization::ReduceCheckString(Node* node) {
  Node* const input = NodeProperties::GetValueInput(node, 0);
  Type* const input_type = NodeProperties::GetType(input);
  if (input_type->Is(Type::String())) {
    ReplaceWithValue(node, input);
    return Replace(input);
  }
  return NoChange();
}

Reduction TypedOptimization::ReduceCheckEqualsInternalizedString(Node* node) {
  Node* const exp = NodeProperties::GetValueInput(node, 0);
  Type* const exp_type = NodeProperties::GetType(exp);
  Node* const val = NodeProperties::GetValueInput(node, 1);
  Type* const val_type = NodeProperties::GetType(val);
  Node* const effect = NodeProperties::GetEffectInput(node);
  if (val_type->Is(exp_type)) return Replace(effect);
  // TODO(turbofan): Should we also try to optimize the
  // non-internalized String case for {val} here?
  return NoChange();
}

Reduction TypedOptimization::ReduceCheckEqualsSymbol(Node* node) {
  Node* const exp = NodeProperties::GetValueInput(node, 0);
  Type* const exp_type = NodeProperties::GetType(exp);
  Node* const val = NodeProperties::GetValueInput(node, 1);
  Type* const val_type = NodeProperties::GetType(val);
  Node* const effect = NodeProperties::GetEffectInput(node);
  if (val_type->Is(exp_type)) return Replace(effect);
  return NoChange();
}

Reduction TypedOptimization::ReduceLoadField(Node* node) {
  Node* const object = NodeProperties::GetValueInput(node, 0);
  Type* const object_type = NodeProperties::GetType(object);
  FieldAccess const& access = FieldAccessOf(node->op());
  if (access.base_is_tagged == kTaggedBase &&
      access.offset == HeapObject::kMapOffset) {
    // We can replace LoadField[Map](o) with map if is stable, and
    // o has type Constant(object) and map == object->map, and either
    //  (1) map cannot transition further, or
    //  (2) deoptimization is enabled and we can add a code dependency on the
    //      stability of map (to guard the Constant type information).
    Handle<Map> object_map;
    if (GetStableMapFromObjectType(object_type).ToHandle(&object_map)) {
      if (object_map->CanTransition()) {
        dependencies()->AssumeMapStable(object_map);
      }
      Node* const value = jsgraph()->HeapConstant(object_map);
      ReplaceWithValue(node, value);
      return Replace(value);
    }
  }
  return NoChange();
}

Reduction TypedOptimization::ReduceNumberFloor(Node* node) {
  Node* const input = NodeProperties::GetValueInput(node, 0);
  Type* const input_type = NodeProperties::GetType(input);
  if (input_type->Is(type_cache_.kIntegerOrMinusZeroOrNaN)) {
    return Replace(input);
  }
  if (input_type->Is(Type::PlainNumber()) &&
      (input->opcode() == IrOpcode::kNumberDivide ||
       input->opcode() == IrOpcode::kSpeculativeNumberDivide)) {
    Node* const lhs = NodeProperties::GetValueInput(input, 0);
    Type* const lhs_type = NodeProperties::GetType(lhs);
    Node* const rhs = NodeProperties::GetValueInput(input, 1);
    Type* const rhs_type = NodeProperties::GetType(rhs);
    if (lhs_type->Is(Type::Unsigned32()) && rhs_type->Is(Type::Unsigned32())) {
      // We can replace
      //
      //   NumberFloor(NumberDivide(lhs: unsigned32,
      //                            rhs: unsigned32)): plain-number
      //
      // with
      //
      //   NumberToUint32(NumberDivide(lhs, rhs))
      //
      // and just smash the type of the {lhs} on the {node},
      // as the truncated result must be in the same range as
      // {lhs} since {rhs} cannot be less than 1 (due to the
      // plain-number type constraint on the {node}).
      NodeProperties::ChangeOp(node, simplified()->NumberToUint32());
      NodeProperties::SetType(node, lhs_type);
      return Changed(node);
    }
  }
  return NoChange();
}

Reduction TypedOptimization::ReduceNumberRoundop(Node* node) {
  Node* const input = NodeProperties::GetValueInput(node, 0);
  Type* const input_type = NodeProperties::GetType(input);
  if (input_type->Is(type_cache_.kIntegerOrMinusZeroOrNaN)) {
    return Replace(input);
  }
  return NoChange();
}

Reduction TypedOptimization::ReduceNumberToUint8Clamped(Node* node) {
  Node* const input = NodeProperties::GetValueInput(node, 0);
  Type* const input_type = NodeProperties::GetType(input);
  if (input_type->Is(type_cache_.kUint8)) {
    return Replace(input);
  }
  return NoChange();
}

Reduction TypedOptimization::ReducePhi(Node* node) {
  // Try to narrow the type of the Phi {node}, which might be more precise now
  // after lowering based on types, i.e. a SpeculativeNumberAdd has a more
  // precise type than the JSAdd that was in the graph when the Typer was run.
  DCHECK_EQ(IrOpcode::kPhi, node->opcode());
  int arity = node->op()->ValueInputCount();
  Type* type = NodeProperties::GetType(node->InputAt(0));
  for (int i = 1; i < arity; ++i) {
    type = Type::Union(type, NodeProperties::GetType(node->InputAt(i)),
                       graph()->zone());
  }
  Type* const node_type = NodeProperties::GetType(node);
  if (!node_type->Is(type)) {
    type = Type::Intersect(node_type, type, graph()->zone());
    NodeProperties::SetType(node, type);
    return Changed(node);
  }
  return NoChange();
}

Reduction TypedOptimization::ReduceReferenceEqual(Node* node) {
  DCHECK_EQ(IrOpcode::kReferenceEqual, node->opcode());
  Node* const lhs = NodeProperties::GetValueInput(node, 0);
  Node* const rhs = NodeProperties::GetValueInput(node, 1);
  Type* const lhs_type = NodeProperties::GetType(lhs);
  Type* const rhs_type = NodeProperties::GetType(rhs);
  if (!lhs_type->Maybe(rhs_type)) {
    Node* replacement = jsgraph()->FalseConstant();
    // Make sure we do not widen the type.
    if (NodeProperties::GetType(replacement)
            ->Is(NodeProperties::GetType(node))) {
      return Replace(jsgraph()->FalseConstant());
    }
  }
  return NoChange();
}

const Operator* TypedOptimization::NumberComparisonFor(const Operator* op) {
  switch (op->opcode()) {
    case IrOpcode::kStringEqual:
      return simplified()->NumberEqual();
    case IrOpcode::kStringLessThan:
      return simplified()->NumberLessThan();
    case IrOpcode::kStringLessThanOrEqual:
      return simplified()->NumberLessThanOrEqual();
    default:
      break;
  }
  UNREACHABLE();
}

Reduction TypedOptimization::
    TryReduceStringComparisonOfStringFromSingleCharCodeToConstant(
        Node* comparison, Handle<String> string, bool inverted) {
  switch (comparison->opcode()) {
    case IrOpcode::kStringEqual:
      if (string->length() != 1) {
        // String.fromCharCode(x) always has length 1.
        return Replace(jsgraph()->BooleanConstant(false));
      }
      break;
    case IrOpcode::kStringLessThan:
      V8_FALLTHROUGH;
    case IrOpcode::kStringLessThanOrEqual:
      if (string->length() == 0) {
        // String.fromCharCode(x) <= "" is always false,
        // "" < String.fromCharCode(x) is always true.
        return Replace(jsgraph()->BooleanConstant(inverted));
      }
      break;
    default:
      UNREACHABLE();
  }
  return NoChange();
}

// Try to reduces a string comparison of the form
// String.fromCharCode(x) {comparison} {constant} if inverted is false,
// and {constant} {comparison} String.fromCharCode(x) if inverted is true.
Reduction
TypedOptimization::TryReduceStringComparisonOfStringFromSingleCharCode(
    Node* comparison, Node* from_char_code, Node* constant, bool inverted) {
  DCHECK_EQ(IrOpcode::kStringFromSingleCharCode, from_char_code->opcode());
  HeapObjectMatcher m(constant);
  if (!m.HasValue() || !m.Value()->IsString()) return NoChange();
  Handle<String> string = Handle<String>::cast(m.Value());

  // Check if comparison can be resolved statically.
  Reduction red = TryReduceStringComparisonOfStringFromSingleCharCodeToConstant(
      comparison, string, inverted);
  if (red.Changed()) return red;

  const Operator* comparison_op = NumberComparisonFor(comparison->op());
  Node* from_char_code_repl = NodeProperties::GetValueInput(from_char_code, 0);
  Type* from_char_code_repl_type = NodeProperties::GetType(from_char_code_repl);
  if (!from_char_code_repl_type->Is(type_cache_.kUint16)) {
    // Convert to signed int32 to satisfy type of {NumberBitwiseAnd}.
    from_char_code_repl =
        graph()->NewNode(simplified()->NumberToInt32(), from_char_code_repl);
    from_char_code_repl = graph()->NewNode(
        simplified()->NumberBitwiseAnd(), from_char_code_repl,
        jsgraph()->Constant(std::numeric_limits<uint16_t>::max()));
  }
  Node* constant_repl = jsgraph()->Constant(string->Get(0));

  Node* number_comparison = nullptr;
  if (inverted) {
    // "x..." <= String.fromCharCode(z) is true if x < z.
    if (string->length() > 1 &&
        comparison->opcode() == IrOpcode::kStringLessThanOrEqual) {
      comparison_op = simplified()->NumberLessThan();
    }
    number_comparison =
        graph()->NewNode(comparison_op, constant_repl, from_char_code_repl);
  } else {
    // String.fromCharCode(z) < "x..." is true if z <= x.
    if (string->length() > 1 &&
        comparison->opcode() == IrOpcode::kStringLessThan) {
      comparison_op = simplified()->NumberLessThanOrEqual();
    }
    number_comparison =
        graph()->NewNode(comparison_op, from_char_code_repl, constant_repl);
  }
  ReplaceWithValue(comparison, number_comparison);
  return Replace(number_comparison);
}

Reduction TypedOptimization::ReduceStringComparison(Node* node) {
  DCHECK(IrOpcode::kStringEqual == node->opcode() ||
         IrOpcode::kStringLessThan == node->opcode() ||
         IrOpcode::kStringLessThanOrEqual == node->opcode());
  Node* const lhs = NodeProperties::GetValueInput(node, 0);
  Node* const rhs = NodeProperties::GetValueInput(node, 1);
  if (lhs->opcode() == IrOpcode::kStringFromSingleCharCode) {
    if (rhs->opcode() == IrOpcode::kStringFromSingleCharCode) {
      Node* left = NodeProperties::GetValueInput(lhs, 0);
      Node* right = NodeProperties::GetValueInput(rhs, 0);
      Type* left_type = NodeProperties::GetType(left);
      Type* right_type = NodeProperties::GetType(right);
      if (!left_type->Is(type_cache_.kUint16)) {
        // Convert to signed int32 to satisfy type of {NumberBitwiseAnd}.
        left = graph()->NewNode(simplified()->NumberToInt32(), left);
        left = graph()->NewNode(
            simplified()->NumberBitwiseAnd(), left,
            jsgraph()->Constant(std::numeric_limits<uint16_t>::max()));
      }
      if (!right_type->Is(type_cache_.kUint16)) {
        // Convert to signed int32 to satisfy type of {NumberBitwiseAnd}.
        right = graph()->NewNode(simplified()->NumberToInt32(), right);
        right = graph()->NewNode(
            simplified()->NumberBitwiseAnd(), right,
            jsgraph()->Constant(std::numeric_limits<uint16_t>::max()));
      }
      Node* equal =
          graph()->NewNode(NumberComparisonFor(node->op()), left, right);
      ReplaceWithValue(node, equal);
      return Replace(equal);
    } else {
      return TryReduceStringComparisonOfStringFromSingleCharCode(node, lhs, rhs,
                                                                 false);
    }
  } else if (rhs->opcode() == IrOpcode::kStringFromSingleCharCode) {
    return TryReduceStringComparisonOfStringFromSingleCharCode(node, rhs, lhs,
                                                               true);
  }
  return NoChange();
}

Reduction TypedOptimization::ReduceSameValue(Node* node) {
  DCHECK_EQ(IrOpcode::kSameValue, node->opcode());
  Node* const lhs = NodeProperties::GetValueInput(node, 0);
  Node* const rhs = NodeProperties::GetValueInput(node, 1);
  Type* const lhs_type = NodeProperties::GetType(lhs);
  Type* const rhs_type = NodeProperties::GetType(rhs);
  if (lhs == rhs) {
    // SameValue(x,x) => #true
    return Replace(jsgraph()->TrueConstant());
  } else if (lhs_type->Is(Type::Unique()) && rhs_type->Is(Type::Unique())) {
    // SameValue(x:unique,y:unique) => ReferenceEqual(x,y)
    NodeProperties::ChangeOp(node, simplified()->ReferenceEqual());
    return Changed(node);
  } else if (lhs_type->Is(Type::String()) && rhs_type->Is(Type::String())) {
    // SameValue(x:string,y:string) => StringEqual(x,y)
    NodeProperties::ChangeOp(node, simplified()->StringEqual());
    return Changed(node);
  } else if (lhs_type->Is(Type::MinusZero())) {
    // SameValue(x:minus-zero,y) => ObjectIsMinusZero(y)
    node->RemoveInput(0);
    NodeProperties::ChangeOp(node, simplified()->ObjectIsMinusZero());
    return Changed(node);
  } else if (rhs_type->Is(Type::MinusZero())) {
    // SameValue(x,y:minus-zero) => ObjectIsMinusZero(x)
    node->RemoveInput(1);
    NodeProperties::ChangeOp(node, simplified()->ObjectIsMinusZero());
    return Changed(node);
  } else if (lhs_type->Is(Type::NaN())) {
    // SameValue(x:nan,y) => ObjectIsNaN(y)
    node->RemoveInput(0);
    NodeProperties::ChangeOp(node, simplified()->ObjectIsNaN());
    return Changed(node);
  } else if (rhs_type->Is(Type::NaN())) {
    // SameValue(x,y:nan) => ObjectIsNaN(x)
    node->RemoveInput(1);
    NodeProperties::ChangeOp(node, simplified()->ObjectIsNaN());
    return Changed(node);
  } else if (lhs_type->Is(Type::PlainNumber()) &&
             rhs_type->Is(Type::PlainNumber())) {
    // SameValue(x:plain-number,y:plain-number) => NumberEqual(x,y)
    NodeProperties::ChangeOp(node, simplified()->NumberEqual());
    return Changed(node);
  }
  return NoChange();
}

Reduction TypedOptimization::ReduceSelect(Node* node) {
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
  // Try to narrow the type of the Select {node}, which might be more precise
  // now after lowering based on types.
  Type* type = Type::Union(vtrue_type, vfalse_type, graph()->zone());
  Type* const node_type = NodeProperties::GetType(node);
  if (!node_type->Is(type)) {
    type = Type::Intersect(node_type, type, graph()->zone());
    NodeProperties::SetType(node, type);
    return Changed(node);
  }
  return NoChange();
}

Reduction TypedOptimization::ReduceSpeculativeToNumber(Node* node) {
  DCHECK_EQ(IrOpcode::kSpeculativeToNumber, node->opcode());
  Node* const input = NodeProperties::GetValueInput(node, 0);
  Type* const input_type = NodeProperties::GetType(input);
  if (input_type->Is(Type::Number())) {
    // SpeculativeToNumber(x:number) => x
    ReplaceWithValue(node, input);
    return Replace(input);
  }
  return NoChange();
}

Reduction TypedOptimization::ReduceTypeOf(Node* node) {
  Node* const input = node->InputAt(0);
  Type* const type = NodeProperties::GetType(input);
  Factory* const f = factory();
  if (type->Is(Type::Boolean())) {
    return Replace(jsgraph()->Constant(f->boolean_string()));
  } else if (type->Is(Type::Number())) {
    return Replace(jsgraph()->Constant(f->number_string()));
  } else if (type->Is(Type::String())) {
    return Replace(jsgraph()->Constant(f->string_string()));
  } else if (type->Is(Type::BigInt())) {
    return Replace(jsgraph()->Constant(f->bigint_string()));
  } else if (type->Is(Type::Symbol())) {
    return Replace(jsgraph()->Constant(f->symbol_string()));
  } else if (type->Is(Type::OtherUndetectableOrUndefined())) {
    return Replace(jsgraph()->Constant(f->undefined_string()));
  } else if (type->Is(Type::NonCallableOrNull())) {
    return Replace(jsgraph()->Constant(f->object_string()));
  } else if (type->Is(Type::Function())) {
    return Replace(jsgraph()->Constant(f->function_string()));
  } else if (type->IsHeapConstant()) {
    return Replace(jsgraph()->Constant(
        Object::TypeOf(isolate(), type->AsHeapConstant()->Value())));
  }

  return NoChange();
}

Reduction TypedOptimization::ReduceToBoolean(Node* node) {
  Node* const input = node->InputAt(0);
  Type* const input_type = NodeProperties::GetType(input);
  if (input_type->Is(Type::Boolean())) {
    // ToBoolean(x:boolean) => x
    return Replace(input);
  } else if (input_type->Is(Type::OrderedNumber())) {
    // SToBoolean(x:ordered-number) => BooleanNot(NumberEqual(x,#0))
    node->ReplaceInput(0, graph()->NewNode(simplified()->NumberEqual(), input,
                                           jsgraph()->ZeroConstant()));
    node->TrimInputCount(1);
    NodeProperties::ChangeOp(node, simplified()->BooleanNot());
    return Changed(node);
  } else if (input_type->Is(Type::Number())) {
    // ToBoolean(x:number) => NumberToBoolean(x)
    node->TrimInputCount(1);
    NodeProperties::ChangeOp(node, simplified()->NumberToBoolean());
    return Changed(node);
  } else if (input_type->Is(Type::DetectableReceiverOrNull())) {
    // ToBoolean(x:detectable receiver \/ null)
    //   => BooleanNot(ReferenceEqual(x,#null))
    node->ReplaceInput(0, graph()->NewNode(simplified()->ReferenceEqual(),
                                           input, jsgraph()->NullConstant()));
    node->TrimInputCount(1);
    NodeProperties::ChangeOp(node, simplified()->BooleanNot());
    return Changed(node);
  } else if (input_type->Is(Type::ReceiverOrNullOrUndefined())) {
    // ToBoolean(x:receiver \/ null \/ undefined)
    //   => BooleanNot(ObjectIsUndetectable(x))
    node->ReplaceInput(
        0, graph()->NewNode(simplified()->ObjectIsUndetectable(), input));
    node->TrimInputCount(1);
    NodeProperties::ChangeOp(node, simplified()->BooleanNot());
    return Changed(node);
  } else if (input_type->Is(Type::String())) {
    // ToBoolean(x:string) => BooleanNot(ReferenceEqual(x,""))
    node->ReplaceInput(0,
                       graph()->NewNode(simplified()->ReferenceEqual(), input,
                                        jsgraph()->EmptyStringConstant()));
    node->TrimInputCount(1);
    NodeProperties::ChangeOp(node, simplified()->BooleanNot());
    return Changed(node);
  }
  return NoChange();
}

Factory* TypedOptimization::factory() const { return isolate()->factory(); }

Graph* TypedOptimization::graph() const { return jsgraph()->graph(); }

Isolate* TypedOptimization::isolate() const { return jsgraph()->isolate(); }

SimplifiedOperatorBuilder* TypedOptimization::simplified() const {
  return jsgraph()->simplified();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
