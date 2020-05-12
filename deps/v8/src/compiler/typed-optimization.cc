// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/typed-optimization.h"

#include "src/base/optional.h"
#include "src/compiler/compilation-dependencies.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/js-heap-broker.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/simplified-operator.h"
#include "src/compiler/type-cache.h"
#include "src/execution/isolate-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

TypedOptimization::TypedOptimization(Editor* editor,
                                     CompilationDependencies* dependencies,
                                     JSGraph* jsgraph, JSHeapBroker* broker)
    : AdvancedReducer(editor),
      dependencies_(dependencies),
      jsgraph_(jsgraph),
      broker_(broker),
      true_type_(
          Type::Constant(broker, factory()->true_value(), graph()->zone())),
      false_type_(
          Type::Constant(broker, factory()->false_value(), graph()->zone())),
      type_cache_(TypeCache::Get()) {}

TypedOptimization::~TypedOptimization() = default;

Reduction TypedOptimization::Reduce(Node* node) {
  DisallowHeapAccess no_heap_access;
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
    case IrOpcode::kNumberSilenceNaN:
      return ReduceNumberSilenceNaN(node);
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
    case IrOpcode::kStringLength:
      return ReduceStringLength(node);
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
    case IrOpcode::kSpeculativeNumberAdd:
      return ReduceSpeculativeNumberAdd(node);
    case IrOpcode::kSpeculativeNumberSubtract:
    case IrOpcode::kSpeculativeNumberMultiply:
    case IrOpcode::kSpeculativeNumberDivide:
    case IrOpcode::kSpeculativeNumberModulus:
      return ReduceSpeculativeNumberBinop(node);
    case IrOpcode::kSpeculativeNumberEqual:
    case IrOpcode::kSpeculativeNumberLessThan:
    case IrOpcode::kSpeculativeNumberLessThanOrEqual:
      return ReduceSpeculativeNumberComparison(node);
    default:
      break;
  }
  return NoChange();
}

namespace {

base::Optional<MapRef> GetStableMapFromObjectType(JSHeapBroker* broker,
                                                  Type object_type) {
  if (object_type.IsHeapConstant()) {
    HeapObjectRef object = object_type.AsHeapConstant()->Ref();
    MapRef object_map = object.map();
    if (object_map.is_stable()) return object_map;
  }
  return {};
}

Node* ResolveSameValueRenames(Node* node) {
  while (true) {
    switch (node->opcode()) {
      case IrOpcode::kCheckHeapObject:
      case IrOpcode::kCheckNumber:
      case IrOpcode::kCheckSmi:
      case IrOpcode::kFinishRegion:
      case IrOpcode::kTypeGuard:
        if (node->IsDead()) {
          return node;
        } else {
          node = node->InputAt(0);
          continue;
        }
      default:
        return node;
    }
  }
}

}  // namespace

Reduction TypedOptimization::ReduceConvertReceiver(Node* node) {
  Node* const value = NodeProperties::GetValueInput(node, 0);
  Type const value_type = NodeProperties::GetType(value);
  Node* const global_proxy = NodeProperties::GetValueInput(node, 1);
  if (value_type.Is(Type::Receiver())) {
    ReplaceWithValue(node, value);
    return Replace(value);
  } else if (value_type.Is(Type::NullOrUndefined())) {
    ReplaceWithValue(node, global_proxy);
    return Replace(global_proxy);
  }
  return NoChange();
}

Reduction TypedOptimization::ReduceCheckHeapObject(Node* node) {
  Node* const input = NodeProperties::GetValueInput(node, 0);
  Type const input_type = NodeProperties::GetType(input);
  if (!input_type.Maybe(Type::SignedSmall())) {
    ReplaceWithValue(node, input);
    return Replace(input);
  }
  return NoChange();
}

Reduction TypedOptimization::ReduceCheckNotTaggedHole(Node* node) {
  Node* const input = NodeProperties::GetValueInput(node, 0);
  Type const input_type = NodeProperties::GetType(input);
  if (!input_type.Maybe(Type::Hole())) {
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
  Type const object_type = NodeProperties::GetType(object);
  Node* const effect = NodeProperties::GetEffectInput(node);
  base::Optional<MapRef> object_map =
      GetStableMapFromObjectType(broker(), object_type);
  if (object_map.has_value()) {
    for (int i = 1; i < node->op()->ValueInputCount(); ++i) {
      Node* const map = NodeProperties::GetValueInput(node, i);
      Type const map_type = NodeProperties::GetType(map);
      if (map_type.IsHeapConstant() &&
          map_type.AsHeapConstant()->Ref().equals(*object_map)) {
        if (object_map->CanTransition()) {
          dependencies()->DependOnStableMap(*object_map);
        }
        return Replace(effect);
      }
    }
  }
  return NoChange();
}

Reduction TypedOptimization::ReduceCheckNumber(Node* node) {
  Node* const input = NodeProperties::GetValueInput(node, 0);
  Type const input_type = NodeProperties::GetType(input);
  if (input_type.Is(Type::Number())) {
    ReplaceWithValue(node, input);
    return Replace(input);
  }
  return NoChange();
}

Reduction TypedOptimization::ReduceCheckString(Node* node) {
  Node* const input = NodeProperties::GetValueInput(node, 0);
  Type const input_type = NodeProperties::GetType(input);
  if (input_type.Is(Type::String())) {
    ReplaceWithValue(node, input);
    return Replace(input);
  }
  return NoChange();
}

Reduction TypedOptimization::ReduceCheckEqualsInternalizedString(Node* node) {
  Node* const exp = NodeProperties::GetValueInput(node, 0);
  Type const exp_type = NodeProperties::GetType(exp);
  Node* const val = NodeProperties::GetValueInput(node, 1);
  Type const val_type = NodeProperties::GetType(val);
  Node* const effect = NodeProperties::GetEffectInput(node);
  if (val_type.Is(exp_type)) return Replace(effect);
  // TODO(turbofan): Should we also try to optimize the
  // non-internalized String case for {val} here?
  return NoChange();
}

Reduction TypedOptimization::ReduceCheckEqualsSymbol(Node* node) {
  Node* const exp = NodeProperties::GetValueInput(node, 0);
  Type const exp_type = NodeProperties::GetType(exp);
  Node* const val = NodeProperties::GetValueInput(node, 1);
  Type const val_type = NodeProperties::GetType(val);
  Node* const effect = NodeProperties::GetEffectInput(node);
  if (val_type.Is(exp_type)) return Replace(effect);
  return NoChange();
}

Reduction TypedOptimization::ReduceLoadField(Node* node) {
  Node* const object = NodeProperties::GetValueInput(node, 0);
  Type const object_type = NodeProperties::GetType(object);
  FieldAccess const& access = FieldAccessOf(node->op());
  if (access.base_is_tagged == kTaggedBase &&
      access.offset == HeapObject::kMapOffset) {
    // We can replace LoadField[Map](o) with map if is stable, and
    // o has type Constant(object) and map == object->map, and either
    //  (1) map cannot transition further, or
    //  (2) deoptimization is enabled and we can add a code dependency on the
    //      stability of map (to guard the Constant type information).
    base::Optional<MapRef> object_map =
        GetStableMapFromObjectType(broker(), object_type);
    if (object_map.has_value()) {
      dependencies()->DependOnStableMap(*object_map);
      Node* const value = jsgraph()->Constant(*object_map);
      ReplaceWithValue(node, value);
      return Replace(value);
    }
  }
  return NoChange();
}

Reduction TypedOptimization::ReduceNumberFloor(Node* node) {
  Node* const input = NodeProperties::GetValueInput(node, 0);
  Type const input_type = NodeProperties::GetType(input);
  if (input_type.Is(type_cache_->kIntegerOrMinusZeroOrNaN)) {
    return Replace(input);
  }
  if (input_type.Is(Type::PlainNumber()) &&
      (input->opcode() == IrOpcode::kNumberDivide ||
       input->opcode() == IrOpcode::kSpeculativeNumberDivide)) {
    Node* const lhs = NodeProperties::GetValueInput(input, 0);
    Type const lhs_type = NodeProperties::GetType(lhs);
    Node* const rhs = NodeProperties::GetValueInput(input, 1);
    Type const rhs_type = NodeProperties::GetType(rhs);
    if (lhs_type.Is(Type::Unsigned32()) && rhs_type.Is(Type::Unsigned32())) {
      // We can replace
      //
      //   NumberFloor(NumberDivide(lhs: unsigned32,
      //                            rhs: unsigned32)): plain-number
      //
      // with
      //
      //   NumberToUint32(NumberDivide(lhs, rhs))
      //
      // and just smash the type [0...lhs.Max] on the {node},
      // as the truncated result must be loewr than {lhs}'s maximum
      // value (note that {rhs} cannot be less than 1 due to the
      // plain-number type constraint on the {node}).
      NodeProperties::ChangeOp(node, simplified()->NumberToUint32());
      NodeProperties::SetType(node,
                              Type::Range(0, lhs_type.Max(), graph()->zone()));
      return Changed(node);
    }
  }
  return NoChange();
}

Reduction TypedOptimization::ReduceNumberRoundop(Node* node) {
  Node* const input = NodeProperties::GetValueInput(node, 0);
  Type const input_type = NodeProperties::GetType(input);
  if (input_type.Is(type_cache_->kIntegerOrMinusZeroOrNaN)) {
    return Replace(input);
  }
  return NoChange();
}

Reduction TypedOptimization::ReduceNumberSilenceNaN(Node* node) {
  Node* const input = NodeProperties::GetValueInput(node, 0);
  Type const input_type = NodeProperties::GetType(input);
  if (input_type.Is(Type::OrderedNumber())) {
    return Replace(input);
  }
  return NoChange();
}

Reduction TypedOptimization::ReduceNumberToUint8Clamped(Node* node) {
  Node* const input = NodeProperties::GetValueInput(node, 0);
  Type const input_type = NodeProperties::GetType(input);
  if (input_type.Is(type_cache_->kUint8)) {
    return Replace(input);
  }
  return NoChange();
}

Reduction TypedOptimization::ReducePhi(Node* node) {
  // Try to narrow the type of the Phi {node}, which might be more precise now
  // after lowering based on types, i.e. a SpeculativeNumberAdd has a more
  // precise type than the JSAdd that was in the graph when the Typer was run.
  DCHECK_EQ(IrOpcode::kPhi, node->opcode());
  // Prevent new types from being propagated through loop-related Phis for now.
  // This is to avoid slow convergence of type narrowing when we learn very
  // precise information about loop variables.
  if (NodeProperties::GetControlInput(node, 0)->opcode() == IrOpcode::kLoop) {
    return NoChange();
  }
  int arity = node->op()->ValueInputCount();
  Type type = NodeProperties::GetType(node->InputAt(0));
  for (int i = 1; i < arity; ++i) {
    type = Type::Union(type, NodeProperties::GetType(node->InputAt(i)),
                       graph()->zone());
  }
  Type const node_type = NodeProperties::GetType(node);
  if (!node_type.Is(type)) {
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
  Type const lhs_type = NodeProperties::GetType(lhs);
  Type const rhs_type = NodeProperties::GetType(rhs);
  if (!lhs_type.Maybe(rhs_type)) {
    Node* replacement = jsgraph()->FalseConstant();
    // Make sure we do not widen the type.
    if (NodeProperties::GetType(replacement)
            .Is(NodeProperties::GetType(node))) {
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
        Node* comparison, const StringRef& string, bool inverted) {
  switch (comparison->opcode()) {
    case IrOpcode::kStringEqual:
      if (string.length() != 1) {
        // String.fromCharCode(x) always has length 1.
        return Replace(jsgraph()->BooleanConstant(false));
      }
      break;
    case IrOpcode::kStringLessThan:
      V8_FALLTHROUGH;
    case IrOpcode::kStringLessThanOrEqual:
      if (string.length() == 0) {
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
    Node* comparison, Node* from_char_code, Type constant_type, bool inverted) {
  DCHECK_EQ(IrOpcode::kStringFromSingleCharCode, from_char_code->opcode());

  if (!constant_type.IsHeapConstant()) return NoChange();
  ObjectRef constant = constant_type.AsHeapConstant()->Ref();

  if (!constant.IsString()) return NoChange();
  StringRef string = constant.AsString();

  // Check if comparison can be resolved statically.
  Reduction red = TryReduceStringComparisonOfStringFromSingleCharCodeToConstant(
      comparison, string, inverted);
  if (red.Changed()) return red;

  const Operator* comparison_op = NumberComparisonFor(comparison->op());
  Node* from_char_code_repl = NodeProperties::GetValueInput(from_char_code, 0);
  Type from_char_code_repl_type = NodeProperties::GetType(from_char_code_repl);
  if (!from_char_code_repl_type.Is(type_cache_->kUint16)) {
    // Convert to signed int32 to satisfy type of {NumberBitwiseAnd}.
    from_char_code_repl =
        graph()->NewNode(simplified()->NumberToInt32(), from_char_code_repl);
    from_char_code_repl = graph()->NewNode(
        simplified()->NumberBitwiseAnd(), from_char_code_repl,
        jsgraph()->Constant(std::numeric_limits<uint16_t>::max()));
  }
  Node* constant_repl = jsgraph()->Constant(string.GetFirstChar());

  Node* number_comparison = nullptr;
  if (inverted) {
    // "x..." <= String.fromCharCode(z) is true if x < z.
    if (string.length() > 1 &&
        comparison->opcode() == IrOpcode::kStringLessThanOrEqual) {
      comparison_op = simplified()->NumberLessThan();
    }
    number_comparison =
        graph()->NewNode(comparison_op, constant_repl, from_char_code_repl);
  } else {
    // String.fromCharCode(z) < "x..." is true if z <= x.
    if (string.length() > 1 &&
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
  Type lhs_type = NodeProperties::GetType(lhs);
  Type rhs_type = NodeProperties::GetType(rhs);
  if (lhs->opcode() == IrOpcode::kStringFromSingleCharCode) {
    if (rhs->opcode() == IrOpcode::kStringFromSingleCharCode) {
      Node* left = NodeProperties::GetValueInput(lhs, 0);
      Node* right = NodeProperties::GetValueInput(rhs, 0);
      Type left_type = NodeProperties::GetType(left);
      Type right_type = NodeProperties::GetType(right);
      if (!left_type.Is(type_cache_->kUint16)) {
        // Convert to signed int32 to satisfy type of {NumberBitwiseAnd}.
        left = graph()->NewNode(simplified()->NumberToInt32(), left);
        left = graph()->NewNode(
            simplified()->NumberBitwiseAnd(), left,
            jsgraph()->Constant(std::numeric_limits<uint16_t>::max()));
      }
      if (!right_type.Is(type_cache_->kUint16)) {
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
      return TryReduceStringComparisonOfStringFromSingleCharCode(
          node, lhs, rhs_type, false);
    }
  } else if (rhs->opcode() == IrOpcode::kStringFromSingleCharCode) {
    return TryReduceStringComparisonOfStringFromSingleCharCode(node, rhs,
                                                               lhs_type, true);
  }
  return NoChange();
}

Reduction TypedOptimization::ReduceStringLength(Node* node) {
  DCHECK_EQ(IrOpcode::kStringLength, node->opcode());
  Node* const input = NodeProperties::GetValueInput(node, 0);
  switch (input->opcode()) {
    case IrOpcode::kHeapConstant: {
      // Constant-fold the String::length of the {input}.
      HeapObjectMatcher m(input);
      if (m.Ref(broker()).IsString()) {
        uint32_t const length = m.Ref(broker()).AsString().length();
        Node* value = jsgraph()->Constant(length);
        return Replace(value);
      }
      break;
    }
    case IrOpcode::kStringConcat: {
      // The first value input to the {input} is the resulting length.
      return Replace(input->InputAt(0));
    }
    default:
      break;
  }
  return NoChange();
}

Reduction TypedOptimization::ReduceSameValue(Node* node) {
  DCHECK_EQ(IrOpcode::kSameValue, node->opcode());
  Node* const lhs = NodeProperties::GetValueInput(node, 0);
  Node* const rhs = NodeProperties::GetValueInput(node, 1);
  Type const lhs_type = NodeProperties::GetType(lhs);
  Type const rhs_type = NodeProperties::GetType(rhs);
  if (ResolveSameValueRenames(lhs) == ResolveSameValueRenames(rhs)) {
    if (NodeProperties::GetType(node).IsNone()) {
      return NoChange();
    }
    // SameValue(x,x) => #true
    return Replace(jsgraph()->TrueConstant());
  } else if (lhs_type.Is(Type::Unique()) && rhs_type.Is(Type::Unique())) {
    // SameValue(x:unique,y:unique) => ReferenceEqual(x,y)
    NodeProperties::ChangeOp(node, simplified()->ReferenceEqual());
    return Changed(node);
  } else if (lhs_type.Is(Type::String()) && rhs_type.Is(Type::String())) {
    // SameValue(x:string,y:string) => StringEqual(x,y)
    NodeProperties::ChangeOp(node, simplified()->StringEqual());
    return Changed(node);
  } else if (lhs_type.Is(Type::MinusZero())) {
    // SameValue(x:minus-zero,y) => ObjectIsMinusZero(y)
    node->RemoveInput(0);
    NodeProperties::ChangeOp(node, simplified()->ObjectIsMinusZero());
    return Changed(node);
  } else if (rhs_type.Is(Type::MinusZero())) {
    // SameValue(x,y:minus-zero) => ObjectIsMinusZero(x)
    node->RemoveInput(1);
    NodeProperties::ChangeOp(node, simplified()->ObjectIsMinusZero());
    return Changed(node);
  } else if (lhs_type.Is(Type::NaN())) {
    // SameValue(x:nan,y) => ObjectIsNaN(y)
    node->RemoveInput(0);
    NodeProperties::ChangeOp(node, simplified()->ObjectIsNaN());
    return Changed(node);
  } else if (rhs_type.Is(Type::NaN())) {
    // SameValue(x,y:nan) => ObjectIsNaN(x)
    node->RemoveInput(1);
    NodeProperties::ChangeOp(node, simplified()->ObjectIsNaN());
    return Changed(node);
  } else if (lhs_type.Is(Type::PlainNumber()) &&
             rhs_type.Is(Type::PlainNumber())) {
    // SameValue(x:plain-number,y:plain-number) => NumberEqual(x,y)
    NodeProperties::ChangeOp(node, simplified()->NumberEqual());
    return Changed(node);
  }
  return NoChange();
}

Reduction TypedOptimization::ReduceSelect(Node* node) {
  DCHECK_EQ(IrOpcode::kSelect, node->opcode());
  Node* const condition = NodeProperties::GetValueInput(node, 0);
  Type const condition_type = NodeProperties::GetType(condition);
  Node* const vtrue = NodeProperties::GetValueInput(node, 1);
  Type const vtrue_type = NodeProperties::GetType(vtrue);
  Node* const vfalse = NodeProperties::GetValueInput(node, 2);
  Type const vfalse_type = NodeProperties::GetType(vfalse);
  if (condition_type.Is(true_type_)) {
    // Select(condition:true, vtrue, vfalse) => vtrue
    return Replace(vtrue);
  }
  if (condition_type.Is(false_type_)) {
    // Select(condition:false, vtrue, vfalse) => vfalse
    return Replace(vfalse);
  }
  if (vtrue_type.Is(true_type_) && vfalse_type.Is(false_type_)) {
    // Select(condition, vtrue:true, vfalse:false) => condition
    return Replace(condition);
  }
  if (vtrue_type.Is(false_type_) && vfalse_type.Is(true_type_)) {
    // Select(condition, vtrue:false, vfalse:true) => BooleanNot(condition)
    node->TrimInputCount(1);
    NodeProperties::ChangeOp(node, simplified()->BooleanNot());
    return Changed(node);
  }
  // Try to narrow the type of the Select {node}, which might be more precise
  // now after lowering based on types.
  Type type = Type::Union(vtrue_type, vfalse_type, graph()->zone());
  Type const node_type = NodeProperties::GetType(node);
  if (!node_type.Is(type)) {
    type = Type::Intersect(node_type, type, graph()->zone());
    NodeProperties::SetType(node, type);
    return Changed(node);
  }
  return NoChange();
}

Reduction TypedOptimization::ReduceSpeculativeToNumber(Node* node) {
  DCHECK_EQ(IrOpcode::kSpeculativeToNumber, node->opcode());
  Node* const input = NodeProperties::GetValueInput(node, 0);
  Type const input_type = NodeProperties::GetType(input);
  if (input_type.Is(Type::Number())) {
    // SpeculativeToNumber(x:number) => x
    ReplaceWithValue(node, input);
    return Replace(input);
  }
  return NoChange();
}

Reduction TypedOptimization::ReduceTypeOf(Node* node) {
  Node* const input = node->InputAt(0);
  Type const type = NodeProperties::GetType(input);
  Factory* const f = factory();
  if (type.Is(Type::Boolean())) {
    return Replace(
        jsgraph()->Constant(ObjectRef(broker(), f->boolean_string())));
  } else if (type.Is(Type::Number())) {
    return Replace(
        jsgraph()->Constant(ObjectRef(broker(), f->number_string())));
  } else if (type.Is(Type::String())) {
    return Replace(
        jsgraph()->Constant(ObjectRef(broker(), f->string_string())));
  } else if (type.Is(Type::BigInt())) {
    return Replace(
        jsgraph()->Constant(ObjectRef(broker(), f->bigint_string())));
  } else if (type.Is(Type::Symbol())) {
    return Replace(
        jsgraph()->Constant(ObjectRef(broker(), f->symbol_string())));
  } else if (type.Is(Type::OtherUndetectableOrUndefined())) {
    return Replace(
        jsgraph()->Constant(ObjectRef(broker(), f->undefined_string())));
  } else if (type.Is(Type::NonCallableOrNull())) {
    return Replace(
        jsgraph()->Constant(ObjectRef(broker(), f->object_string())));
  } else if (type.Is(Type::Function())) {
    return Replace(
        jsgraph()->Constant(ObjectRef(broker(), f->function_string())));
  }
  return NoChange();
}

Reduction TypedOptimization::ReduceToBoolean(Node* node) {
  Node* const input = node->InputAt(0);
  Type const input_type = NodeProperties::GetType(input);
  if (input_type.Is(Type::Boolean())) {
    // ToBoolean(x:boolean) => x
    return Replace(input);
  } else if (input_type.Is(Type::OrderedNumber())) {
    // SToBoolean(x:ordered-number) => BooleanNot(NumberEqual(x,#0))
    node->ReplaceInput(0, graph()->NewNode(simplified()->NumberEqual(), input,
                                           jsgraph()->ZeroConstant()));
    node->TrimInputCount(1);
    NodeProperties::ChangeOp(node, simplified()->BooleanNot());
    return Changed(node);
  } else if (input_type.Is(Type::Number())) {
    // ToBoolean(x:number) => NumberToBoolean(x)
    node->TrimInputCount(1);
    NodeProperties::ChangeOp(node, simplified()->NumberToBoolean());
    return Changed(node);
  } else if (input_type.Is(Type::DetectableReceiverOrNull())) {
    // ToBoolean(x:detectable receiver \/ null)
    //   => BooleanNot(ReferenceEqual(x,#null))
    node->ReplaceInput(0, graph()->NewNode(simplified()->ReferenceEqual(),
                                           input, jsgraph()->NullConstant()));
    node->TrimInputCount(1);
    NodeProperties::ChangeOp(node, simplified()->BooleanNot());
    return Changed(node);
  } else if (input_type.Is(Type::ReceiverOrNullOrUndefined())) {
    // ToBoolean(x:receiver \/ null \/ undefined)
    //   => BooleanNot(ObjectIsUndetectable(x))
    node->ReplaceInput(
        0, graph()->NewNode(simplified()->ObjectIsUndetectable(), input));
    node->TrimInputCount(1);
    NodeProperties::ChangeOp(node, simplified()->BooleanNot());
    return Changed(node);
  } else if (input_type.Is(Type::String())) {
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

namespace {
bool BothAre(Type t1, Type t2, Type t3) { return t1.Is(t3) && t2.Is(t3); }

bool NeitherCanBe(Type t1, Type t2, Type t3) {
  return !t1.Maybe(t3) && !t2.Maybe(t3);
}

const Operator* NumberOpFromSpeculativeNumberOp(
    SimplifiedOperatorBuilder* simplified, const Operator* op) {
  switch (op->opcode()) {
    case IrOpcode::kSpeculativeNumberEqual:
      return simplified->NumberEqual();
    case IrOpcode::kSpeculativeNumberLessThan:
      return simplified->NumberLessThan();
    case IrOpcode::kSpeculativeNumberLessThanOrEqual:
      return simplified->NumberLessThanOrEqual();
    case IrOpcode::kSpeculativeNumberAdd:
      // Handled by ReduceSpeculativeNumberAdd.
      UNREACHABLE();
    case IrOpcode::kSpeculativeNumberSubtract:
      return simplified->NumberSubtract();
    case IrOpcode::kSpeculativeNumberMultiply:
      return simplified->NumberMultiply();
    case IrOpcode::kSpeculativeNumberDivide:
      return simplified->NumberDivide();
    case IrOpcode::kSpeculativeNumberModulus:
      return simplified->NumberModulus();
    default:
      break;
  }
  UNREACHABLE();
}

}  // namespace

Reduction TypedOptimization::ReduceSpeculativeNumberAdd(Node* node) {
  Node* const lhs = NodeProperties::GetValueInput(node, 0);
  Node* const rhs = NodeProperties::GetValueInput(node, 1);
  Type const lhs_type = NodeProperties::GetType(lhs);
  Type const rhs_type = NodeProperties::GetType(rhs);
  NumberOperationHint hint = NumberOperationHintOf(node->op());
  if ((hint == NumberOperationHint::kNumber ||
       hint == NumberOperationHint::kNumberOrOddball) &&
      BothAre(lhs_type, rhs_type, Type::PlainPrimitive()) &&
      NeitherCanBe(lhs_type, rhs_type, Type::StringOrReceiver())) {
    // SpeculativeNumberAdd(x:-string, y:-string) =>
    //     NumberAdd(ToNumber(x), ToNumber(y))
    Node* const toNum_lhs = ConvertPlainPrimitiveToNumber(lhs);
    Node* const toNum_rhs = ConvertPlainPrimitiveToNumber(rhs);
    Node* const value =
        graph()->NewNode(simplified()->NumberAdd(), toNum_lhs, toNum_rhs);
    ReplaceWithValue(node, value);
    return Replace(value);
  }
  return NoChange();
}

Reduction TypedOptimization::ReduceJSToNumberInput(Node* input) {
  // Try constant-folding of JSToNumber with constant inputs.
  Type input_type = NodeProperties::GetType(input);

  if (input_type.Is(Type::String())) {
    HeapObjectMatcher m(input);
    if (m.HasValue() && m.Ref(broker()).IsString()) {
      StringRef input_value = m.Ref(broker()).AsString();
      double number;
      ASSIGN_RETURN_NO_CHANGE_IF_DATA_MISSING(number, input_value.ToNumber());
      return Replace(jsgraph()->Constant(number));
    }
  }
  if (input_type.IsHeapConstant()) {
    HeapObjectRef input_value = input_type.AsHeapConstant()->Ref();
    double value;
    if (input_value.OddballToNumber().To(&value)) {
      return Replace(jsgraph()->Constant(value));
    }
  }
  if (input_type.Is(Type::Number())) {
    // JSToNumber(x:number) => x
    return Changed(input);
  }
  if (input_type.Is(Type::Undefined())) {
    // JSToNumber(undefined) => #NaN
    return Replace(jsgraph()->NaNConstant());
  }
  if (input_type.Is(Type::Null())) {
    // JSToNumber(null) => #0
    return Replace(jsgraph()->ZeroConstant());
  }
  return NoChange();
}

Node* TypedOptimization::ConvertPlainPrimitiveToNumber(Node* node) {
  DCHECK(NodeProperties::GetType(node).Is(Type::PlainPrimitive()));
  // Avoid inserting too many eager ToNumber() operations.
  Reduction const reduction = ReduceJSToNumberInput(node);
  if (reduction.Changed()) return reduction.replacement();
  if (NodeProperties::GetType(node).Is(Type::Number())) {
    return node;
  }
  return graph()->NewNode(simplified()->PlainPrimitiveToNumber(), node);
}

Reduction TypedOptimization::ReduceSpeculativeNumberBinop(Node* node) {
  Node* const lhs = NodeProperties::GetValueInput(node, 0);
  Node* const rhs = NodeProperties::GetValueInput(node, 1);
  Type const lhs_type = NodeProperties::GetType(lhs);
  Type const rhs_type = NodeProperties::GetType(rhs);
  NumberOperationHint hint = NumberOperationHintOf(node->op());
  if ((hint == NumberOperationHint::kNumber ||
       hint == NumberOperationHint::kNumberOrOddball) &&
      BothAre(lhs_type, rhs_type, Type::NumberOrUndefinedOrNullOrBoolean())) {
    // We intentionally do this only in the Number and NumberOrOddball hint case
    // because simplified lowering of these speculative ops may do some clever
    // reductions in the other cases.
    Node* const toNum_lhs = ConvertPlainPrimitiveToNumber(lhs);
    Node* const toNum_rhs = ConvertPlainPrimitiveToNumber(rhs);
    Node* const value = graph()->NewNode(
        NumberOpFromSpeculativeNumberOp(simplified(), node->op()), toNum_lhs,
        toNum_rhs);
    ReplaceWithValue(node, value);
    return Replace(value);
  }
  return NoChange();
}

Reduction TypedOptimization::ReduceSpeculativeNumberComparison(Node* node) {
  Node* const lhs = NodeProperties::GetValueInput(node, 0);
  Node* const rhs = NodeProperties::GetValueInput(node, 1);
  Type const lhs_type = NodeProperties::GetType(lhs);
  Type const rhs_type = NodeProperties::GetType(rhs);
  if (BothAre(lhs_type, rhs_type, Type::Signed32()) ||
      BothAre(lhs_type, rhs_type, Type::Unsigned32())) {
    Node* const value = graph()->NewNode(
        NumberOpFromSpeculativeNumberOp(simplified(), node->op()), lhs, rhs);
    ReplaceWithValue(node, value);
    return Replace(value);
  }
  return NoChange();
}

Factory* TypedOptimization::factory() const {
  return jsgraph()->isolate()->factory();
}

Graph* TypedOptimization::graph() const { return jsgraph()->graph(); }

SimplifiedOperatorBuilder* TypedOptimization::simplified() const {
  return jsgraph()->simplified();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
