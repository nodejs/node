// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-builtin-reducer.h"

#include "src/compilation-dependencies.h"
#include "src/compiler/access-builder.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/simplified-operator.h"
#include "src/compiler/type-cache.h"
#include "src/compiler/types.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {
namespace compiler {


// Helper class to access JSCallFunction nodes that are potential candidates
// for reduction when they have a BuiltinFunctionId associated with them.
class JSCallReduction {
 public:
  explicit JSCallReduction(Node* node) : node_(node) {}

  // Determines whether the node is a JSCallFunction operation that targets a
  // constant callee being a well-known builtin with a BuiltinFunctionId.
  bool HasBuiltinFunctionId() {
    if (node_->opcode() != IrOpcode::kJSCallFunction) return false;
    HeapObjectMatcher m(NodeProperties::GetValueInput(node_, 0));
    if (!m.HasValue() || !m.Value()->IsJSFunction()) return false;
    Handle<JSFunction> function = Handle<JSFunction>::cast(m.Value());
    return function->shared()->HasBuiltinFunctionId();
  }

  // Retrieves the BuiltinFunctionId as described above.
  BuiltinFunctionId GetBuiltinFunctionId() {
    DCHECK_EQ(IrOpcode::kJSCallFunction, node_->opcode());
    HeapObjectMatcher m(NodeProperties::GetValueInput(node_, 0));
    Handle<JSFunction> function = Handle<JSFunction>::cast(m.Value());
    return function->shared()->builtin_function_id();
  }

  bool ReceiverMatches(Type* type) {
    return NodeProperties::GetType(receiver())->Is(type);
  }

  // Determines whether the call takes zero inputs.
  bool InputsMatchZero() { return GetJSCallArity() == 0; }

  // Determines whether the call takes one input of the given type.
  bool InputsMatchOne(Type* t1) {
    return GetJSCallArity() == 1 &&
           NodeProperties::GetType(GetJSCallInput(0))->Is(t1);
  }

  // Determines whether the call takes two inputs of the given types.
  bool InputsMatchTwo(Type* t1, Type* t2) {
    return GetJSCallArity() == 2 &&
           NodeProperties::GetType(GetJSCallInput(0))->Is(t1) &&
           NodeProperties::GetType(GetJSCallInput(1))->Is(t2);
  }

  // Determines whether the call takes inputs all of the given type.
  bool InputsMatchAll(Type* t) {
    for (int i = 0; i < GetJSCallArity(); i++) {
      if (!NodeProperties::GetType(GetJSCallInput(i))->Is(t)) {
        return false;
      }
    }
    return true;
  }

  Node* receiver() { return NodeProperties::GetValueInput(node_, 1); }
  Node* left() { return GetJSCallInput(0); }
  Node* right() { return GetJSCallInput(1); }

  int GetJSCallArity() {
    DCHECK_EQ(IrOpcode::kJSCallFunction, node_->opcode());
    // Skip first (i.e. callee) and second (i.e. receiver) operand.
    return node_->op()->ValueInputCount() - 2;
  }

  Node* GetJSCallInput(int index) {
    DCHECK_EQ(IrOpcode::kJSCallFunction, node_->opcode());
    DCHECK_LT(index, GetJSCallArity());
    // Skip first (i.e. callee) and second (i.e. receiver) operand.
    return NodeProperties::GetValueInput(node_, index + 2);
  }

 private:
  Node* node_;
};

JSBuiltinReducer::JSBuiltinReducer(Editor* editor, JSGraph* jsgraph,
                                   Flags flags,
                                   CompilationDependencies* dependencies)
    : AdvancedReducer(editor),
      dependencies_(dependencies),
      flags_(flags),
      jsgraph_(jsgraph),
      type_cache_(TypeCache::Get()) {}

namespace {

MaybeHandle<Map> GetMapWitness(Node* node) {
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);
  // Check if the {node} is dominated by a CheckMaps with a single map
  // for the {receiver}, and if so use that map for the lowering below.
  for (Node* dominator = effect;;) {
    if (dominator->opcode() == IrOpcode::kCheckMaps &&
        dominator->InputAt(0) == receiver) {
      if (dominator->op()->ValueInputCount() == 2) {
        HeapObjectMatcher m(dominator->InputAt(1));
        if (m.HasValue()) return Handle<Map>::cast(m.Value());
      }
      return MaybeHandle<Map>();
    }
    if (dominator->op()->EffectInputCount() != 1) {
      // Didn't find any appropriate CheckMaps node.
      return MaybeHandle<Map>();
    }
    dominator = NodeProperties::GetEffectInput(dominator);
  }
}

// TODO(turbofan): This was copied from Crankshaft, might be too restrictive.
bool IsReadOnlyLengthDescriptor(Handle<Map> jsarray_map) {
  DCHECK(!jsarray_map->is_dictionary_map());
  Isolate* isolate = jsarray_map->GetIsolate();
  Handle<Name> length_string = isolate->factory()->length_string();
  DescriptorArray* descriptors = jsarray_map->instance_descriptors();
  int number =
      descriptors->SearchWithCache(isolate, *length_string, *jsarray_map);
  DCHECK_NE(DescriptorArray::kNotFound, number);
  return descriptors->GetDetails(number).IsReadOnly();
}

// TODO(turbofan): This was copied from Crankshaft, might be too restrictive.
bool CanInlineArrayResizeOperation(Handle<Map> receiver_map) {
  Isolate* const isolate = receiver_map->GetIsolate();
  if (!receiver_map->prototype()->IsJSArray()) return false;
  Handle<JSArray> receiver_prototype(JSArray::cast(receiver_map->prototype()),
                                     isolate);
  // Ensure that all prototypes of the {receiver} are stable.
  for (PrototypeIterator it(isolate, receiver_prototype, kStartAtReceiver);
       !it.IsAtEnd(); it.Advance()) {
    Handle<JSReceiver> current = PrototypeIterator::GetCurrent<JSReceiver>(it);
    if (!current->map()->is_stable()) return false;
  }
  return receiver_map->instance_type() == JS_ARRAY_TYPE &&
         IsFastElementsKind(receiver_map->elements_kind()) &&
         !receiver_map->is_dictionary_map() && receiver_map->is_extensible() &&
         (!receiver_map->is_prototype_map() || receiver_map->is_stable()) &&
         isolate->IsFastArrayConstructorPrototypeChainIntact() &&
         isolate->IsAnyInitialArrayPrototype(receiver_prototype) &&
         !IsReadOnlyLengthDescriptor(receiver_map);
}

}  // namespace

// ES6 section 22.1.3.17 Array.prototype.pop ( )
Reduction JSBuiltinReducer::ReduceArrayPop(Node* node) {
  Handle<Map> receiver_map;
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  // TODO(turbofan): Extend this to also handle fast (holey) double elements
  // once we got the hole NaN mess sorted out in TurboFan/V8.
  if (GetMapWitness(node).ToHandle(&receiver_map) &&
      CanInlineArrayResizeOperation(receiver_map) &&
      IsFastSmiOrObjectElementsKind(receiver_map->elements_kind())) {
    // Install code dependencies on the {receiver} prototype maps and the
    // global array protector cell.
    dependencies()->AssumePropertyCell(factory()->array_protector());
    dependencies()->AssumePrototypeMapsStable(receiver_map);

    // Load the "length" property of the {receiver}.
    Node* length = effect = graph()->NewNode(
        simplified()->LoadField(
            AccessBuilder::ForJSArrayLength(receiver_map->elements_kind())),
        receiver, effect, control);

    // Check if the {receiver} has any elements.
    Node* check = graph()->NewNode(simplified()->NumberEqual(), length,
                                   jsgraph()->ZeroConstant());
    Node* branch =
        graph()->NewNode(common()->Branch(BranchHint::kFalse), check, control);

    Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
    Node* etrue = effect;
    Node* vtrue = jsgraph()->UndefinedConstant();

    Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
    Node* efalse = effect;
    Node* vfalse;
    {
      // Load the elements backing store from the {receiver}.
      Node* elements = efalse = graph()->NewNode(
          simplified()->LoadField(AccessBuilder::ForJSObjectElements()),
          receiver, efalse, if_false);

      // Ensure that we aren't popping from a copy-on-write backing store.
      elements = efalse =
          graph()->NewNode(simplified()->EnsureWritableFastElements(), receiver,
                           elements, efalse, if_false);

      // Compute the new {length}.
      length = graph()->NewNode(simplified()->NumberSubtract(), length,
                                jsgraph()->OneConstant());

      // Store the new {length} to the {receiver}.
      efalse = graph()->NewNode(
          simplified()->StoreField(
              AccessBuilder::ForJSArrayLength(receiver_map->elements_kind())),
          receiver, length, efalse, if_false);

      // Load the last entry from the {elements}.
      vfalse = efalse = graph()->NewNode(
          simplified()->LoadElement(AccessBuilder::ForFixedArrayElement(
              receiver_map->elements_kind())),
          elements, length, efalse, if_false);

      // Store a hole to the element we just removed from the {receiver}.
      efalse = graph()->NewNode(
          simplified()->StoreElement(AccessBuilder::ForFixedArrayElement(
              GetHoleyElementsKind(receiver_map->elements_kind()))),
          elements, length, jsgraph()->TheHoleConstant(), efalse, if_false);
    }

    control = graph()->NewNode(common()->Merge(2), if_true, if_false);
    effect = graph()->NewNode(common()->EffectPhi(2), etrue, efalse, control);
    Node* value =
        graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                         vtrue, vfalse, control);

    // Convert the hole to undefined. Do this last, so that we can optimize
    // conversion operator via some smart strength reduction in many cases.
    if (IsFastHoleyElementsKind(receiver_map->elements_kind())) {
      value =
          graph()->NewNode(simplified()->ConvertTaggedHoleToUndefined(), value);
    }

    ReplaceWithValue(node, value, effect, control);
    return Replace(value);
  }
  return NoChange();
}

// ES6 section 22.1.3.18 Array.prototype.push ( )
Reduction JSBuiltinReducer::ReduceArrayPush(Node* node) {
  Handle<Map> receiver_map;
  // We need exactly target, receiver and value parameters.
  if (node->op()->ValueInputCount() != 3) return NoChange();
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* value = NodeProperties::GetValueInput(node, 2);
  if (GetMapWitness(node).ToHandle(&receiver_map) &&
      CanInlineArrayResizeOperation(receiver_map)) {
    // Install code dependencies on the {receiver} prototype maps and the
    // global array protector cell.
    dependencies()->AssumePropertyCell(factory()->array_protector());
    dependencies()->AssumePrototypeMapsStable(receiver_map);

    // TODO(turbofan): Perform type checks on the {value}. We are not guaranteed
    // to learn from these checks in case they fail, as the witness (i.e. the
    // map check from the LoadIC for a.push) might not be executed in baseline
    // code (after we stored the value in the builtin and thereby changed the
    // elements kind of a) before be decide to optimize this function again. We
    // currently don't have a proper way to deal with this; the proper solution
    // here is to learn on deopt, i.e. disable Array.prototype.push inlining
    // for this function.
    if (IsFastSmiElementsKind(receiver_map->elements_kind())) {
      value = effect =
          graph()->NewNode(simplified()->CheckSmi(), value, effect, control);
    } else if (IsFastDoubleElementsKind(receiver_map->elements_kind())) {
      value = effect =
          graph()->NewNode(simplified()->CheckNumber(), value, effect, control);
      // Make sure we do not store signaling NaNs into double arrays.
      value = graph()->NewNode(simplified()->NumberSilenceNaN(), value);
    }

    // Load the "length" property of the {receiver}.
    Node* length = effect = graph()->NewNode(
        simplified()->LoadField(
            AccessBuilder::ForJSArrayLength(receiver_map->elements_kind())),
        receiver, effect, control);

    // Load the elements backing store of the {receiver}.
    Node* elements = effect = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForJSObjectElements()), receiver,
        effect, control);

    // TODO(turbofan): Check if we need to grow the {elements} backing store.
    // This will deopt if we cannot grow the array further, and we currently
    // don't necessarily learn from it. See the comment on the value type check
    // above.
    GrowFastElementsFlags flags = GrowFastElementsFlag::kArrayObject;
    if (IsFastDoubleElementsKind(receiver_map->elements_kind())) {
      flags |= GrowFastElementsFlag::kDoubleElements;
    }
    elements = effect =
        graph()->NewNode(simplified()->MaybeGrowFastElements(flags), receiver,
                         elements, length, length, effect, control);

    // Append the value to the {elements}.
    effect = graph()->NewNode(
        simplified()->StoreElement(
            AccessBuilder::ForFixedArrayElement(receiver_map->elements_kind())),
        elements, length, value, effect, control);

    // Return the new length of the {receiver}.
    value = graph()->NewNode(simplified()->NumberAdd(), length,
                             jsgraph()->OneConstant());

    ReplaceWithValue(node, value, effect, control);
    return Replace(value);
  }
  return NoChange();
}

namespace {

bool HasInstanceTypeWitness(Node* receiver, Node* effect,
                            InstanceType instance_type) {
  for (Node* dominator = effect;;) {
    if (dominator->opcode() == IrOpcode::kCheckMaps &&
        dominator->InputAt(0) == receiver) {
      // Check if all maps have the given {instance_type}.
      for (int i = 1; i < dominator->op()->ValueInputCount(); ++i) {
        Node* const map = NodeProperties::GetValueInput(dominator, i);
        Type* const map_type = NodeProperties::GetType(map);
        if (!map_type->IsConstant()) return false;
        Handle<Map> const map_value =
            Handle<Map>::cast(map_type->AsConstant()->Value());
        if (map_value->instance_type() != instance_type) return false;
      }
      return true;
    }
    switch (dominator->opcode()) {
      case IrOpcode::kStoreField: {
        FieldAccess const& access = FieldAccessOf(dominator->op());
        if (access.base_is_tagged == kTaggedBase &&
            access.offset == HeapObject::kMapOffset) {
          return false;
        }
        break;
      }
      case IrOpcode::kStoreElement:
      case IrOpcode::kStoreTypedElement:
        break;
      default: {
        DCHECK_EQ(1, dominator->op()->EffectOutputCount());
        if (dominator->op()->EffectInputCount() != 1 ||
            !dominator->op()->HasProperty(Operator::kNoWrite)) {
          // Didn't find any appropriate CheckMaps node.
          return false;
        }
        break;
      }
    }
    dominator = NodeProperties::GetEffectInput(dominator);
  }
}

}  // namespace

// ES6 section 20.3.4.10 Date.prototype.getTime ( )
Reduction JSBuiltinReducer::ReduceDateGetTime(Node* node) {
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  if (HasInstanceTypeWitness(receiver, effect, JS_DATE_TYPE)) {
    Node* value = effect = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForJSDateValue()), receiver,
        effect, control);
    ReplaceWithValue(node, value, effect, control);
    return Replace(value);
  }
  return NoChange();
}

// ES6 section 19.2.3.6 Function.prototype [ @@hasInstance ] ( V )
Reduction JSBuiltinReducer::ReduceFunctionHasInstance(Node* node) {
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* object = (node->op()->ValueInputCount() >= 3)
                     ? NodeProperties::GetValueInput(node, 2)
                     : jsgraph()->UndefinedConstant();
  Node* context = NodeProperties::GetContextInput(node);
  Node* frame_state = NodeProperties::GetFrameStateInput(node);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // TODO(turbofan): If JSOrdinaryToInstance raises an exception, the
  // stack trace doesn't contain the @@hasInstance call; we have the
  // corresponding bug in the baseline case. Some massaging of the frame
  // state would be necessary here.

  // Morph this {node} into a JSOrdinaryHasInstance node.
  node->ReplaceInput(0, receiver);
  node->ReplaceInput(1, object);
  node->ReplaceInput(2, context);
  node->ReplaceInput(3, frame_state);
  node->ReplaceInput(4, effect);
  node->ReplaceInput(5, control);
  node->TrimInputCount(6);
  NodeProperties::ChangeOp(node, javascript()->OrdinaryHasInstance());
  return Changed(node);
}

// ES6 section 18.2.2 isFinite ( number )
Reduction JSBuiltinReducer::ReduceGlobalIsFinite(Node* node) {
  JSCallReduction r(node);
  if (r.InputsMatchOne(Type::PlainPrimitive())) {
    // isFinite(a:plain-primitive) -> NumberEqual(a', a')
    // where a' = NumberSubtract(ToNumber(a), ToNumber(a))
    Node* input = ToNumber(r.GetJSCallInput(0));
    Node* diff = graph()->NewNode(simplified()->NumberSubtract(), input, input);
    Node* value = graph()->NewNode(simplified()->NumberEqual(), diff, diff);
    return Replace(value);
  }
  return NoChange();
}

// ES6 section 18.2.3 isNaN ( number )
Reduction JSBuiltinReducer::ReduceGlobalIsNaN(Node* node) {
  JSCallReduction r(node);
  if (r.InputsMatchOne(Type::PlainPrimitive())) {
    // isNaN(a:plain-primitive) -> BooleanNot(NumberEqual(a', a'))
    // where a' = ToNumber(a)
    Node* input = ToNumber(r.GetJSCallInput(0));
    Node* check = graph()->NewNode(simplified()->NumberEqual(), input, input);
    Node* value = graph()->NewNode(simplified()->BooleanNot(), check);
    return Replace(value);
  }
  return NoChange();
}

// ES6 section 20.2.2.1 Math.abs ( x )
Reduction JSBuiltinReducer::ReduceMathAbs(Node* node) {
  JSCallReduction r(node);
  if (r.InputsMatchOne(Type::PlainPrimitive())) {
    // Math.abs(a:plain-primitive) -> NumberAbs(ToNumber(a))
    Node* input = ToNumber(r.GetJSCallInput(0));
    Node* value = graph()->NewNode(simplified()->NumberAbs(), input);
    return Replace(value);
  }
  return NoChange();
}

// ES6 section 20.2.2.2 Math.acos ( x )
Reduction JSBuiltinReducer::ReduceMathAcos(Node* node) {
  JSCallReduction r(node);
  if (r.InputsMatchOne(Type::PlainPrimitive())) {
    // Math.acos(a:plain-primitive) -> NumberAcos(ToNumber(a))
    Node* input = ToNumber(r.GetJSCallInput(0));
    Node* value = graph()->NewNode(simplified()->NumberAcos(), input);
    return Replace(value);
  }
  return NoChange();
}

// ES6 section 20.2.2.3 Math.acosh ( x )
Reduction JSBuiltinReducer::ReduceMathAcosh(Node* node) {
  JSCallReduction r(node);
  if (r.InputsMatchOne(Type::PlainPrimitive())) {
    // Math.acosh(a:plain-primitive) -> NumberAcosh(ToNumber(a))
    Node* input = ToNumber(r.GetJSCallInput(0));
    Node* value = graph()->NewNode(simplified()->NumberAcosh(), input);
    return Replace(value);
  }
  return NoChange();
}

// ES6 section 20.2.2.4 Math.asin ( x )
Reduction JSBuiltinReducer::ReduceMathAsin(Node* node) {
  JSCallReduction r(node);
  if (r.InputsMatchOne(Type::PlainPrimitive())) {
    // Math.asin(a:plain-primitive) -> NumberAsin(ToNumber(a))
    Node* input = ToNumber(r.GetJSCallInput(0));
    Node* value = graph()->NewNode(simplified()->NumberAsin(), input);
    return Replace(value);
  }
  return NoChange();
}

// ES6 section 20.2.2.5 Math.asinh ( x )
Reduction JSBuiltinReducer::ReduceMathAsinh(Node* node) {
  JSCallReduction r(node);
  if (r.InputsMatchOne(Type::PlainPrimitive())) {
    // Math.asinh(a:plain-primitive) -> NumberAsinh(ToNumber(a))
    Node* input = ToNumber(r.GetJSCallInput(0));
    Node* value = graph()->NewNode(simplified()->NumberAsinh(), input);
    return Replace(value);
  }
  return NoChange();
}

// ES6 section 20.2.2.6 Math.atan ( x )
Reduction JSBuiltinReducer::ReduceMathAtan(Node* node) {
  JSCallReduction r(node);
  if (r.InputsMatchOne(Type::PlainPrimitive())) {
    // Math.atan(a:plain-primitive) -> NumberAtan(ToNumber(a))
    Node* input = ToNumber(r.GetJSCallInput(0));
    Node* value = graph()->NewNode(simplified()->NumberAtan(), input);
    return Replace(value);
  }
  return NoChange();
}

// ES6 section 20.2.2.7 Math.atanh ( x )
Reduction JSBuiltinReducer::ReduceMathAtanh(Node* node) {
  JSCallReduction r(node);
  if (r.InputsMatchOne(Type::PlainPrimitive())) {
    // Math.atanh(a:plain-primitive) -> NumberAtanh(ToNumber(a))
    Node* input = ToNumber(r.GetJSCallInput(0));
    Node* value = graph()->NewNode(simplified()->NumberAtanh(), input);
    return Replace(value);
  }
  return NoChange();
}

// ES6 section 20.2.2.8 Math.atan2 ( y, x )
Reduction JSBuiltinReducer::ReduceMathAtan2(Node* node) {
  JSCallReduction r(node);
  if (r.InputsMatchTwo(Type::PlainPrimitive(), Type::PlainPrimitive())) {
    // Math.atan2(a:plain-primitive,
    //            b:plain-primitive) -> NumberAtan2(ToNumber(a),
    //                                              ToNumber(b))
    Node* left = ToNumber(r.left());
    Node* right = ToNumber(r.right());
    Node* value = graph()->NewNode(simplified()->NumberAtan2(), left, right);
    return Replace(value);
  }
  return NoChange();
}

// ES6 section 20.2.2.10 Math.ceil ( x )
Reduction JSBuiltinReducer::ReduceMathCeil(Node* node) {
  JSCallReduction r(node);
  if (r.InputsMatchOne(Type::PlainPrimitive())) {
    // Math.ceil(a:plain-primitive) -> NumberCeil(ToNumber(a))
    Node* input = ToNumber(r.GetJSCallInput(0));
    Node* value = graph()->NewNode(simplified()->NumberCeil(), input);
    return Replace(value);
  }
  return NoChange();
}

// ES6 section 20.2.2.11 Math.clz32 ( x )
Reduction JSBuiltinReducer::ReduceMathClz32(Node* node) {
  JSCallReduction r(node);
  if (r.InputsMatchOne(Type::PlainPrimitive())) {
    // Math.clz32(a:plain-primitive) -> NumberClz32(ToUint32(a))
    Node* input = ToUint32(r.GetJSCallInput(0));
    Node* value = graph()->NewNode(simplified()->NumberClz32(), input);
    return Replace(value);
  }
  return NoChange();
}

// ES6 section 20.2.2.12 Math.cos ( x )
Reduction JSBuiltinReducer::ReduceMathCos(Node* node) {
  JSCallReduction r(node);
  if (r.InputsMatchOne(Type::PlainPrimitive())) {
    // Math.cos(a:plain-primitive) -> NumberCos(ToNumber(a))
    Node* input = ToNumber(r.GetJSCallInput(0));
    Node* value = graph()->NewNode(simplified()->NumberCos(), input);
    return Replace(value);
  }
  return NoChange();
}

// ES6 section 20.2.2.13 Math.cosh ( x )
Reduction JSBuiltinReducer::ReduceMathCosh(Node* node) {
  JSCallReduction r(node);
  if (r.InputsMatchOne(Type::PlainPrimitive())) {
    // Math.cosh(a:plain-primitive) -> NumberCosh(ToNumber(a))
    Node* input = ToNumber(r.GetJSCallInput(0));
    Node* value = graph()->NewNode(simplified()->NumberCosh(), input);
    return Replace(value);
  }
  return NoChange();
}

// ES6 section 20.2.2.14 Math.exp ( x )
Reduction JSBuiltinReducer::ReduceMathExp(Node* node) {
  JSCallReduction r(node);
  if (r.InputsMatchOne(Type::PlainPrimitive())) {
    // Math.exp(a:plain-primitive) -> NumberExp(ToNumber(a))
    Node* input = ToNumber(r.GetJSCallInput(0));
    Node* value = graph()->NewNode(simplified()->NumberExp(), input);
    return Replace(value);
  }
  return NoChange();
}

// ES6 section 20.2.2.15 Math.expm1 ( x )
Reduction JSBuiltinReducer::ReduceMathExpm1(Node* node) {
  JSCallReduction r(node);
  if (r.InputsMatchOne(Type::Number())) {
    // Math.expm1(a:number) -> NumberExpm1(a)
    Node* value = graph()->NewNode(simplified()->NumberExpm1(), r.left());
    return Replace(value);
  }
  return NoChange();
}

// ES6 section 20.2.2.16 Math.floor ( x )
Reduction JSBuiltinReducer::ReduceMathFloor(Node* node) {
  JSCallReduction r(node);
  if (r.InputsMatchOne(Type::PlainPrimitive())) {
    // Math.floor(a:plain-primitive) -> NumberFloor(ToNumber(a))
    Node* input = ToNumber(r.GetJSCallInput(0));
    Node* value = graph()->NewNode(simplified()->NumberFloor(), input);
    return Replace(value);
  }
  return NoChange();
}

// ES6 section 20.2.2.17 Math.fround ( x )
Reduction JSBuiltinReducer::ReduceMathFround(Node* node) {
  JSCallReduction r(node);
  if (r.InputsMatchOne(Type::PlainPrimitive())) {
    // Math.fround(a:plain-primitive) -> NumberFround(ToNumber(a))
    Node* input = ToNumber(r.GetJSCallInput(0));
    Node* value = graph()->NewNode(simplified()->NumberFround(), input);
    return Replace(value);
  }
  return NoChange();
}

// ES6 section 20.2.2.19 Math.imul ( x, y )
Reduction JSBuiltinReducer::ReduceMathImul(Node* node) {
  JSCallReduction r(node);
  if (r.InputsMatchTwo(Type::PlainPrimitive(), Type::PlainPrimitive())) {
    // Math.imul(a:plain-primitive,
    //           b:plain-primitive) -> NumberImul(ToUint32(a),
    //                                            ToUint32(b))
    Node* left = ToUint32(r.left());
    Node* right = ToUint32(r.right());
    Node* value = graph()->NewNode(simplified()->NumberImul(), left, right);
    return Replace(value);
  }
  return NoChange();
}

// ES6 section 20.2.2.20 Math.log ( x )
Reduction JSBuiltinReducer::ReduceMathLog(Node* node) {
  JSCallReduction r(node);
  if (r.InputsMatchOne(Type::PlainPrimitive())) {
    // Math.log(a:plain-primitive) -> NumberLog(ToNumber(a))
    Node* input = ToNumber(r.GetJSCallInput(0));
    Node* value = graph()->NewNode(simplified()->NumberLog(), input);
    return Replace(value);
  }
  return NoChange();
}

// ES6 section 20.2.2.21 Math.log1p ( x )
Reduction JSBuiltinReducer::ReduceMathLog1p(Node* node) {
  JSCallReduction r(node);
  if (r.InputsMatchOne(Type::PlainPrimitive())) {
    // Math.log1p(a:plain-primitive) -> NumberLog1p(ToNumber(a))
    Node* input = ToNumber(r.GetJSCallInput(0));
    Node* value = graph()->NewNode(simplified()->NumberLog1p(), input);
    return Replace(value);
  }
  return NoChange();
}

// ES6 section 20.2.2.22 Math.log10 ( x )
Reduction JSBuiltinReducer::ReduceMathLog10(Node* node) {
  JSCallReduction r(node);
  if (r.InputsMatchOne(Type::Number())) {
    // Math.log10(a:number) -> NumberLog10(a)
    Node* value = graph()->NewNode(simplified()->NumberLog10(), r.left());
    return Replace(value);
  }
  return NoChange();
}

// ES6 section 20.2.2.23 Math.log2 ( x )
Reduction JSBuiltinReducer::ReduceMathLog2(Node* node) {
  JSCallReduction r(node);
  if (r.InputsMatchOne(Type::Number())) {
    // Math.log2(a:number) -> NumberLog(a)
    Node* value = graph()->NewNode(simplified()->NumberLog2(), r.left());
    return Replace(value);
  }
  return NoChange();
}

// ES6 section 20.2.2.24 Math.max ( value1, value2, ...values )
Reduction JSBuiltinReducer::ReduceMathMax(Node* node) {
  JSCallReduction r(node);
  if (r.InputsMatchZero()) {
    // Math.max() -> -Infinity
    return Replace(jsgraph()->Constant(-V8_INFINITY));
  }
  if (r.InputsMatchAll(Type::PlainPrimitive())) {
    // Math.max(a:plain-primitive, b:plain-primitive, ...)
    Node* value = ToNumber(r.GetJSCallInput(0));
    for (int i = 1; i < r.GetJSCallArity(); i++) {
      Node* input = ToNumber(r.GetJSCallInput(i));
      value = graph()->NewNode(simplified()->NumberMax(), value, input);
    }
    return Replace(value);
  }
  return NoChange();
}

// ES6 section 20.2.2.25 Math.min ( value1, value2, ...values )
Reduction JSBuiltinReducer::ReduceMathMin(Node* node) {
  JSCallReduction r(node);
  if (r.InputsMatchZero()) {
    // Math.min() -> Infinity
    return Replace(jsgraph()->Constant(V8_INFINITY));
  }
  if (r.InputsMatchAll(Type::PlainPrimitive())) {
    // Math.min(a:plain-primitive, b:plain-primitive, ...)
    Node* value = ToNumber(r.GetJSCallInput(0));
    for (int i = 1; i < r.GetJSCallArity(); i++) {
      Node* input = ToNumber(r.GetJSCallInput(i));
      value = graph()->NewNode(simplified()->NumberMin(), value, input);
    }
    return Replace(value);
  }
  return NoChange();
}

// ES6 section 20.2.2.26 Math.pow ( x, y )
Reduction JSBuiltinReducer::ReduceMathPow(Node* node) {
  JSCallReduction r(node);
  if (r.InputsMatchTwo(Type::PlainPrimitive(), Type::PlainPrimitive())) {
    // Math.pow(a:plain-primitive,
    //          b:plain-primitive) -> NumberPow(ToNumber(a), ToNumber(b))
    Node* left = ToNumber(r.left());
    Node* right = ToNumber(r.right());
    Node* value = graph()->NewNode(simplified()->NumberPow(), left, right);
    return Replace(value);
  }
  return NoChange();
}

// ES6 section 20.2.2.28 Math.round ( x )
Reduction JSBuiltinReducer::ReduceMathRound(Node* node) {
  JSCallReduction r(node);
  if (r.InputsMatchOne(Type::PlainPrimitive())) {
    // Math.round(a:plain-primitive) -> NumberRound(ToNumber(a))
    Node* input = ToNumber(r.GetJSCallInput(0));
    Node* value = graph()->NewNode(simplified()->NumberRound(), input);
    return Replace(value);
  }
  return NoChange();
}

// ES6 section 20.2.2.9 Math.cbrt ( x )
Reduction JSBuiltinReducer::ReduceMathCbrt(Node* node) {
  JSCallReduction r(node);
  if (r.InputsMatchOne(Type::Number())) {
    // Math.cbrt(a:number) -> NumberCbrt(a)
    Node* value = graph()->NewNode(simplified()->NumberCbrt(), r.left());
    return Replace(value);
  }
  return NoChange();
}

// ES6 section 20.2.2.29 Math.sign ( x )
Reduction JSBuiltinReducer::ReduceMathSign(Node* node) {
  JSCallReduction r(node);
  if (r.InputsMatchOne(Type::PlainPrimitive())) {
    // Math.sign(a:plain-primitive) -> NumberSign(ToNumber(a))
    Node* input = ToNumber(r.GetJSCallInput(0));
    Node* value = graph()->NewNode(simplified()->NumberSign(), input);
    return Replace(value);
  }
  return NoChange();
}

// ES6 section 20.2.2.30 Math.sin ( x )
Reduction JSBuiltinReducer::ReduceMathSin(Node* node) {
  JSCallReduction r(node);
  if (r.InputsMatchOne(Type::PlainPrimitive())) {
    // Math.sin(a:plain-primitive) -> NumberSin(ToNumber(a))
    Node* input = ToNumber(r.GetJSCallInput(0));
    Node* value = graph()->NewNode(simplified()->NumberSin(), input);
    return Replace(value);
  }
  return NoChange();
}

// ES6 section 20.2.2.31 Math.sinh ( x )
Reduction JSBuiltinReducer::ReduceMathSinh(Node* node) {
  JSCallReduction r(node);
  if (r.InputsMatchOne(Type::PlainPrimitive())) {
    // Math.sinh(a:plain-primitive) -> NumberSinh(ToNumber(a))
    Node* input = ToNumber(r.GetJSCallInput(0));
    Node* value = graph()->NewNode(simplified()->NumberSinh(), input);
    return Replace(value);
  }
  return NoChange();
}

// ES6 section 20.2.2.32 Math.sqrt ( x )
Reduction JSBuiltinReducer::ReduceMathSqrt(Node* node) {
  JSCallReduction r(node);
  if (r.InputsMatchOne(Type::PlainPrimitive())) {
    // Math.sqrt(a:plain-primitive) -> NumberSqrt(ToNumber(a))
    Node* input = ToNumber(r.GetJSCallInput(0));
    Node* value = graph()->NewNode(simplified()->NumberSqrt(), input);
    return Replace(value);
  }
  return NoChange();
}

// ES6 section 20.2.2.33 Math.tan ( x )
Reduction JSBuiltinReducer::ReduceMathTan(Node* node) {
  JSCallReduction r(node);
  if (r.InputsMatchOne(Type::PlainPrimitive())) {
    // Math.tan(a:plain-primitive) -> NumberTan(ToNumber(a))
    Node* input = ToNumber(r.GetJSCallInput(0));
    Node* value = graph()->NewNode(simplified()->NumberTan(), input);
    return Replace(value);
  }
  return NoChange();
}

// ES6 section 20.2.2.34 Math.tanh ( x )
Reduction JSBuiltinReducer::ReduceMathTanh(Node* node) {
  JSCallReduction r(node);
  if (r.InputsMatchOne(Type::PlainPrimitive())) {
    // Math.tanh(a:plain-primitive) -> NumberTanh(ToNumber(a))
    Node* input = ToNumber(r.GetJSCallInput(0));
    Node* value = graph()->NewNode(simplified()->NumberTanh(), input);
    return Replace(value);
  }
  return NoChange();
}

// ES6 section 20.2.2.35 Math.trunc ( x )
Reduction JSBuiltinReducer::ReduceMathTrunc(Node* node) {
  JSCallReduction r(node);
  if (r.InputsMatchOne(Type::PlainPrimitive())) {
    // Math.trunc(a:plain-primitive) -> NumberTrunc(ToNumber(a))
    Node* input = ToNumber(r.GetJSCallInput(0));
    Node* value = graph()->NewNode(simplified()->NumberTrunc(), input);
    return Replace(value);
  }
  return NoChange();
}

// ES6 section 20.1.2.2 Number.isFinite ( number )
Reduction JSBuiltinReducer::ReduceNumberIsFinite(Node* node) {
  JSCallReduction r(node);
  if (r.InputsMatchOne(Type::Number())) {
    // Number.isFinite(a:number) -> NumberEqual(a', a')
    // where a' = NumberSubtract(a, a)
    Node* input = r.GetJSCallInput(0);
    Node* diff = graph()->NewNode(simplified()->NumberSubtract(), input, input);
    Node* value = graph()->NewNode(simplified()->NumberEqual(), diff, diff);
    return Replace(value);
  }
  return NoChange();
}

// ES6 section 20.1.2.3 Number.isInteger ( number )
Reduction JSBuiltinReducer::ReduceNumberIsInteger(Node* node) {
  JSCallReduction r(node);
  if (r.InputsMatchOne(Type::Number())) {
    // Number.isInteger(x:number) -> NumberEqual(NumberSubtract(x, x'), #0)
    // where x' = NumberTrunc(x)
    Node* input = r.GetJSCallInput(0);
    Node* trunc = graph()->NewNode(simplified()->NumberTrunc(), input);
    Node* diff = graph()->NewNode(simplified()->NumberSubtract(), input, trunc);
    Node* value = graph()->NewNode(simplified()->NumberEqual(), diff,
                                   jsgraph()->ZeroConstant());
    return Replace(value);
  }
  return NoChange();
}

// ES6 section 20.1.2.4 Number.isNaN ( number )
Reduction JSBuiltinReducer::ReduceNumberIsNaN(Node* node) {
  JSCallReduction r(node);
  if (r.InputsMatchOne(Type::Number())) {
    // Number.isNaN(a:number) -> BooleanNot(NumberEqual(a, a))
    Node* input = r.GetJSCallInput(0);
    Node* check = graph()->NewNode(simplified()->NumberEqual(), input, input);
    Node* value = graph()->NewNode(simplified()->BooleanNot(), check);
    return Replace(value);
  }
  return NoChange();
}

// ES6 section 20.1.2.5 Number.isSafeInteger ( number )
Reduction JSBuiltinReducer::ReduceNumberIsSafeInteger(Node* node) {
  JSCallReduction r(node);
  if (r.InputsMatchOne(type_cache_.kSafeInteger)) {
    // Number.isInteger(x:safe-integer) -> #true
    Node* value = jsgraph()->TrueConstant();
    return Replace(value);
  }
  return NoChange();
}

// ES6 section 20.1.2.13 Number.parseInt ( string, radix )
Reduction JSBuiltinReducer::ReduceNumberParseInt(Node* node) {
  JSCallReduction r(node);
  if (r.InputsMatchOne(type_cache_.kSafeInteger) ||
      r.InputsMatchTwo(type_cache_.kSafeInteger,
                       type_cache_.kZeroOrUndefined) ||
      r.InputsMatchTwo(type_cache_.kSafeInteger, type_cache_.kTenOrUndefined)) {
    // Number.parseInt(a:safe-integer) -> NumberToInt32(a)
    // Number.parseInt(a:safe-integer,b:#0\/undefined) -> NumberToInt32(a)
    // Number.parseInt(a:safe-integer,b:#10\/undefined) -> NumberToInt32(a)
    Node* input = r.GetJSCallInput(0);
    Node* value = graph()->NewNode(simplified()->NumberToInt32(), input);
    return Replace(value);
  }
  return NoChange();
}

// ES6 section 21.1.2.1 String.fromCharCode ( ...codeUnits )
Reduction JSBuiltinReducer::ReduceStringFromCharCode(Node* node) {
  JSCallReduction r(node);
  if (r.InputsMatchOne(Type::PlainPrimitive())) {
    // String.fromCharCode(a:plain-primitive) -> StringFromCharCode(a)
    Node* input = ToNumber(r.GetJSCallInput(0));
    Node* value = graph()->NewNode(simplified()->StringFromCharCode(), input);
    return Replace(value);
  }
  return NoChange();
}

namespace {

Node* GetStringWitness(Node* node) {
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Type* receiver_type = NodeProperties::GetType(receiver);
  Node* effect = NodeProperties::GetEffectInput(node);
  if (receiver_type->Is(Type::String())) return receiver;
  // Check if the {node} is dominated by a CheckString renaming for
  // it's {receiver}, and if so use that renaming as {receiver} for
  // the lowering below.
  for (Node* dominator = effect;;) {
    if (dominator->opcode() == IrOpcode::kCheckString &&
        dominator->InputAt(0) == receiver) {
      return dominator;
    }
    if (dominator->op()->EffectInputCount() != 1) {
      // Didn't find any appropriate CheckString node.
      return nullptr;
    }
    dominator = NodeProperties::GetEffectInput(dominator);
  }
}

}  // namespace

// ES6 section 21.1.3.1 String.prototype.charAt ( pos )
Reduction JSBuiltinReducer::ReduceStringCharAt(Node* node) {
  // We need at least target, receiver and index parameters.
  if (node->op()->ValueInputCount() >= 3) {
    Node* index = NodeProperties::GetValueInput(node, 2);
    Type* index_type = NodeProperties::GetType(index);
    Node* effect = NodeProperties::GetEffectInput(node);
    Node* control = NodeProperties::GetControlInput(node);

    if (index_type->Is(Type::Unsigned32())) {
      if (Node* receiver = GetStringWitness(node)) {
        // Determine the {receiver} length.
        Node* receiver_length = effect = graph()->NewNode(
            simplified()->LoadField(AccessBuilder::ForStringLength()), receiver,
            effect, control);

        // Check if {index} is less than {receiver} length.
        Node* check = graph()->NewNode(simplified()->NumberLessThan(), index,
                                       receiver_length);
        Node* branch = graph()->NewNode(common()->Branch(BranchHint::kTrue),
                                        check, control);

        Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
        Node* vtrue;
        {
          // Load the character from the {receiver}.
          vtrue = graph()->NewNode(simplified()->StringCharCodeAt(), receiver,
                                   index, if_true);

          // Return it as single character string.
          vtrue = graph()->NewNode(simplified()->StringFromCharCode(), vtrue);
        }

        // Return the empty string otherwise.
        Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
        Node* vfalse = jsgraph()->EmptyStringConstant();

        control = graph()->NewNode(common()->Merge(2), if_true, if_false);
        Node* value =
            graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                             vtrue, vfalse, control);

        ReplaceWithValue(node, value, effect, control);
        return Replace(value);
      }
    }
  }

  return NoChange();
}

// ES6 section 21.1.3.2 String.prototype.charCodeAt ( pos )
Reduction JSBuiltinReducer::ReduceStringCharCodeAt(Node* node) {
  // We need at least target, receiver and index parameters.
  if (node->op()->ValueInputCount() >= 3) {
    Node* index = NodeProperties::GetValueInput(node, 2);
    Type* index_type = NodeProperties::GetType(index);
    Node* effect = NodeProperties::GetEffectInput(node);
    Node* control = NodeProperties::GetControlInput(node);

    if (index_type->Is(Type::Unsigned32())) {
      if (Node* receiver = GetStringWitness(node)) {
        // Determine the {receiver} length.
        Node* receiver_length = effect = graph()->NewNode(
            simplified()->LoadField(AccessBuilder::ForStringLength()), receiver,
            effect, control);

        // Check if {index} is less than {receiver} length.
        Node* check = graph()->NewNode(simplified()->NumberLessThan(), index,
                                       receiver_length);
        Node* branch = graph()->NewNode(common()->Branch(BranchHint::kTrue),
                                        check, control);

        // Load the character from the {receiver}.
        Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
        Node* vtrue = graph()->NewNode(simplified()->StringCharCodeAt(),
                                       receiver, index, if_true);

        // Return NaN otherwise.
        Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
        Node* vfalse = jsgraph()->NaNConstant();

        control = graph()->NewNode(common()->Merge(2), if_true, if_false);
        Node* value =
            graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                             vtrue, vfalse, control);

        ReplaceWithValue(node, value, effect, control);
        return Replace(value);
      }
    }
  }

  return NoChange();
}

Reduction JSBuiltinReducer::ReduceStringIteratorNext(Node* node) {
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* context = NodeProperties::GetContextInput(node);
  if (HasInstanceTypeWitness(receiver, effect, JS_STRING_ITERATOR_TYPE)) {
    Node* string = effect = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForJSStringIteratorString()),
        receiver, effect, control);
    Node* index = effect = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForJSStringIteratorIndex()),
        receiver, effect, control);
    Node* length = effect = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForStringLength()), string,
        effect, control);

    // branch0: if (index < length)
    Node* check0 =
        graph()->NewNode(simplified()->NumberLessThan(), index, length);
    Node* branch0 =
        graph()->NewNode(common()->Branch(BranchHint::kTrue), check0, control);

    Node* etrue0 = effect;
    Node* if_true0 = graph()->NewNode(common()->IfTrue(), branch0);
    Node* done_true;
    Node* vtrue0;
    {
      done_true = jsgraph()->FalseConstant();
      Node* lead = graph()->NewNode(simplified()->StringCharCodeAt(), string,
                                    index, if_true0);

      // branch1: if ((lead & 0xFC00) === 0xD800)
      Node* check1 = graph()->NewNode(
          simplified()->NumberEqual(),
          graph()->NewNode(simplified()->NumberBitwiseAnd(), lead,
                           jsgraph()->Int32Constant(0xFC00)),
          jsgraph()->Int32Constant(0xD800));
      Node* branch1 = graph()->NewNode(common()->Branch(BranchHint::kFalse),
                                       check1, if_true0);
      Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
      Node* vtrue1;
      {
        Node* next_index = graph()->NewNode(simplified()->NumberAdd(), index,
                                            jsgraph()->OneConstant());
        // branch2: if ((index + 1) < length)
        Node* check2 = graph()->NewNode(simplified()->NumberLessThan(),
                                        next_index, length);
        Node* branch2 = graph()->NewNode(common()->Branch(BranchHint::kTrue),
                                         check2, if_true1);
        Node* if_true2 = graph()->NewNode(common()->IfTrue(), branch2);
        Node* vtrue2;
        {
          Node* trail = graph()->NewNode(simplified()->StringCharCodeAt(),
                                         string, next_index, if_true2);
          // branch3: if ((trail & 0xFC00) === 0xDC00)
          Node* check3 = graph()->NewNode(
              simplified()->NumberEqual(),
              graph()->NewNode(simplified()->NumberBitwiseAnd(), trail,
                               jsgraph()->Int32Constant(0xFC00)),
              jsgraph()->Int32Constant(0xDC00));
          Node* branch3 = graph()->NewNode(common()->Branch(BranchHint::kTrue),
                                           check3, if_true2);
          Node* if_true3 = graph()->NewNode(common()->IfTrue(), branch3);
          Node* vtrue3;
          {
            vtrue3 = graph()->NewNode(
                simplified()->NumberBitwiseOr(),
// Need to swap the order for big-endian platforms
#if V8_TARGET_BIG_ENDIAN
                graph()->NewNode(simplified()->NumberShiftLeft(), lead,
                                 jsgraph()->Int32Constant(16)),
                trail);
#else
                graph()->NewNode(simplified()->NumberShiftLeft(), trail,
                                 jsgraph()->Int32Constant(16)),
                lead);
#endif
          }

          Node* if_false3 = graph()->NewNode(common()->IfFalse(), branch3);
          Node* vfalse3 = lead;
          if_true2 = graph()->NewNode(common()->Merge(2), if_true3, if_false3);
          vtrue2 =
              graph()->NewNode(common()->Phi(MachineRepresentation::kWord32, 2),
                               vtrue3, vfalse3, if_true2);
        }

        Node* if_false2 = graph()->NewNode(common()->IfFalse(), branch2);
        Node* vfalse2 = lead;
        if_true1 = graph()->NewNode(common()->Merge(2), if_true2, if_false2);
        vtrue1 =
            graph()->NewNode(common()->Phi(MachineRepresentation::kWord32, 2),
                             vtrue2, vfalse2, if_true1);
      }

      Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
      Node* vfalse1 = lead;
      if_true0 = graph()->NewNode(common()->Merge(2), if_true1, if_false1);
      vtrue0 =
          graph()->NewNode(common()->Phi(MachineRepresentation::kWord32, 2),
                           vtrue1, vfalse1, if_true0);
      vtrue0 = graph()->NewNode(
          simplified()->StringFromCodePoint(UnicodeEncoding::UTF16), vtrue0);

      // Update iterator.[[NextIndex]]
      Node* char_length = etrue0 = graph()->NewNode(
          simplified()->LoadField(AccessBuilder::ForStringLength()), vtrue0,
          etrue0, if_true0);
      index = graph()->NewNode(simplified()->NumberAdd(), index, char_length);
      etrue0 = graph()->NewNode(
          simplified()->StoreField(AccessBuilder::ForJSStringIteratorIndex()),
          receiver, index, etrue0, if_true0);
    }

    Node* if_false0 = graph()->NewNode(common()->IfFalse(), branch0);
    Node* done_false;
    Node* vfalse0;
    {
      vfalse0 = jsgraph()->UndefinedConstant();
      done_false = jsgraph()->TrueConstant();
    }

    control = graph()->NewNode(common()->Merge(2), if_true0, if_false0);
    effect = graph()->NewNode(common()->EffectPhi(2), etrue0, effect, control);
    Node* value =
        graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                         vtrue0, vfalse0, control);
    Node* done =
        graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                         done_true, done_false, control);

    value = effect = graph()->NewNode(javascript()->CreateIterResultObject(),
                                      value, done, context, effect);

    ReplaceWithValue(node, value, effect, control);
    return Replace(value);
  }
  return NoChange();
}

Reduction JSBuiltinReducer::ReduceArrayBufferViewAccessor(
    Node* node, InstanceType instance_type, FieldAccess const& access) {
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  if (HasInstanceTypeWitness(receiver, effect, instance_type)) {
    // Load the {receiver}s field.
    Node* receiver_value = effect = graph()->NewNode(
        simplified()->LoadField(access), receiver, effect, control);

    // Check if the {receiver}s buffer was neutered.
    Node* receiver_buffer = effect = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForJSArrayBufferViewBuffer()),
        receiver, effect, control);
    Node* check = effect =
        graph()->NewNode(simplified()->ArrayBufferWasNeutered(),
                         receiver_buffer, effect, control);

    // Default to zero if the {receiver}s buffer was neutered.
    Node* value = graph()->NewNode(
        common()->Select(MachineRepresentation::kTagged, BranchHint::kFalse),
        check, jsgraph()->ZeroConstant(), receiver_value);

    ReplaceWithValue(node, value, effect, control);
    return Replace(value);
  }
  return NoChange();
}

Reduction JSBuiltinReducer::Reduce(Node* node) {
  Reduction reduction = NoChange();
  JSCallReduction r(node);

  // Dispatch according to the BuiltinFunctionId if present.
  if (!r.HasBuiltinFunctionId()) return NoChange();
  switch (r.GetBuiltinFunctionId()) {
    case kArrayPop:
      return ReduceArrayPop(node);
    case kArrayPush:
      return ReduceArrayPush(node);
    case kDateGetTime:
      return ReduceDateGetTime(node);
    case kFunctionHasInstance:
      return ReduceFunctionHasInstance(node);
      break;
    case kGlobalIsFinite:
      reduction = ReduceGlobalIsFinite(node);
      break;
    case kGlobalIsNaN:
      reduction = ReduceGlobalIsNaN(node);
      break;
    case kMathAbs:
      reduction = ReduceMathAbs(node);
      break;
    case kMathAcos:
      reduction = ReduceMathAcos(node);
      break;
    case kMathAcosh:
      reduction = ReduceMathAcosh(node);
      break;
    case kMathAsin:
      reduction = ReduceMathAsin(node);
      break;
    case kMathAsinh:
      reduction = ReduceMathAsinh(node);
      break;
    case kMathAtan:
      reduction = ReduceMathAtan(node);
      break;
    case kMathAtanh:
      reduction = ReduceMathAtanh(node);
      break;
    case kMathAtan2:
      reduction = ReduceMathAtan2(node);
      break;
    case kMathCbrt:
      reduction = ReduceMathCbrt(node);
      break;
    case kMathCeil:
      reduction = ReduceMathCeil(node);
      break;
    case kMathClz32:
      reduction = ReduceMathClz32(node);
      break;
    case kMathCos:
      reduction = ReduceMathCos(node);
      break;
    case kMathCosh:
      reduction = ReduceMathCosh(node);
      break;
    case kMathExp:
      reduction = ReduceMathExp(node);
      break;
    case kMathExpm1:
      reduction = ReduceMathExpm1(node);
      break;
    case kMathFloor:
      reduction = ReduceMathFloor(node);
      break;
    case kMathFround:
      reduction = ReduceMathFround(node);
      break;
    case kMathImul:
      reduction = ReduceMathImul(node);
      break;
    case kMathLog:
      reduction = ReduceMathLog(node);
      break;
    case kMathLog1p:
      reduction = ReduceMathLog1p(node);
      break;
    case kMathLog10:
      reduction = ReduceMathLog10(node);
      break;
    case kMathLog2:
      reduction = ReduceMathLog2(node);
      break;
    case kMathMax:
      reduction = ReduceMathMax(node);
      break;
    case kMathMin:
      reduction = ReduceMathMin(node);
      break;
    case kMathPow:
      reduction = ReduceMathPow(node);
      break;
    case kMathRound:
      reduction = ReduceMathRound(node);
      break;
    case kMathSign:
      reduction = ReduceMathSign(node);
      break;
    case kMathSin:
      reduction = ReduceMathSin(node);
      break;
    case kMathSinh:
      reduction = ReduceMathSinh(node);
      break;
    case kMathSqrt:
      reduction = ReduceMathSqrt(node);
      break;
    case kMathTan:
      reduction = ReduceMathTan(node);
      break;
    case kMathTanh:
      reduction = ReduceMathTanh(node);
      break;
    case kMathTrunc:
      reduction = ReduceMathTrunc(node);
      break;
    case kNumberIsFinite:
      reduction = ReduceNumberIsFinite(node);
      break;
    case kNumberIsInteger:
      reduction = ReduceNumberIsInteger(node);
      break;
    case kNumberIsNaN:
      reduction = ReduceNumberIsNaN(node);
      break;
    case kNumberIsSafeInteger:
      reduction = ReduceNumberIsSafeInteger(node);
      break;
    case kNumberParseInt:
      reduction = ReduceNumberParseInt(node);
      break;
    case kStringFromCharCode:
      reduction = ReduceStringFromCharCode(node);
      break;
    case kStringCharAt:
      return ReduceStringCharAt(node);
    case kStringCharCodeAt:
      return ReduceStringCharCodeAt(node);
    case kStringIteratorNext:
      return ReduceStringIteratorNext(node);
    case kDataViewByteLength:
      return ReduceArrayBufferViewAccessor(
          node, JS_DATA_VIEW_TYPE,
          AccessBuilder::ForJSArrayBufferViewByteLength());
    case kDataViewByteOffset:
      return ReduceArrayBufferViewAccessor(
          node, JS_DATA_VIEW_TYPE,
          AccessBuilder::ForJSArrayBufferViewByteOffset());
    case kTypedArrayByteLength:
      return ReduceArrayBufferViewAccessor(
          node, JS_TYPED_ARRAY_TYPE,
          AccessBuilder::ForJSArrayBufferViewByteLength());
    case kTypedArrayByteOffset:
      return ReduceArrayBufferViewAccessor(
          node, JS_TYPED_ARRAY_TYPE,
          AccessBuilder::ForJSArrayBufferViewByteOffset());
    case kTypedArrayLength:
      return ReduceArrayBufferViewAccessor(
          node, JS_TYPED_ARRAY_TYPE, AccessBuilder::ForJSTypedArrayLength());
    default:
      break;
  }

  // Replace builtin call assuming replacement nodes are pure values that don't
  // produce an effect. Replaces {node} with {reduction} and relaxes effects.
  if (reduction.Changed()) ReplaceWithValue(node, reduction.replacement());

  return reduction;
}

Node* JSBuiltinReducer::ToNumber(Node* input) {
  Type* input_type = NodeProperties::GetType(input);
  if (input_type->Is(Type::Number())) return input;
  return graph()->NewNode(simplified()->PlainPrimitiveToNumber(), input);
}

Node* JSBuiltinReducer::ToUint32(Node* input) {
  input = ToNumber(input);
  Type* input_type = NodeProperties::GetType(input);
  if (input_type->Is(Type::Unsigned32())) return input;
  return graph()->NewNode(simplified()->NumberToUint32(), input);
}

Graph* JSBuiltinReducer::graph() const { return jsgraph()->graph(); }

Factory* JSBuiltinReducer::factory() const { return isolate()->factory(); }

Isolate* JSBuiltinReducer::isolate() const { return jsgraph()->isolate(); }


CommonOperatorBuilder* JSBuiltinReducer::common() const {
  return jsgraph()->common();
}


SimplifiedOperatorBuilder* JSBuiltinReducer::simplified() const {
  return jsgraph()->simplified();
}

JSOperatorBuilder* JSBuiltinReducer::javascript() const {
  return jsgraph()->javascript();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
