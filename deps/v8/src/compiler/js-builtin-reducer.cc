// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-builtin-reducer.h"

#include "src/base/bits.h"
#include "src/builtins/builtins-utils.h"
#include "src/code-factory.h"
#include "src/compilation-dependencies.h"
#include "src/compiler/access-builder.h"
#include "src/compiler/allocation-builder.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/linkage.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/simplified-operator.h"
#include "src/compiler/type-cache.h"
#include "src/compiler/types.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

// Helper class to access JSCall nodes that are potential candidates
// for reduction when they have a BuiltinFunctionId associated with them.
class JSCallReduction {
 public:
  explicit JSCallReduction(Node* node) : node_(node) {}

  // Determines whether the node is a JSCall operation that targets a
  // constant callee being a well-known builtin with a BuiltinFunctionId.
  bool HasBuiltinFunctionId() {
    if (node_->opcode() != IrOpcode::kJSCall) return false;
    HeapObjectMatcher m(NodeProperties::GetValueInput(node_, 0));
    if (!m.HasValue() || !m.Value()->IsJSFunction()) return false;
    Handle<JSFunction> function = Handle<JSFunction>::cast(m.Value());
    return function->shared()->HasBuiltinFunctionId();
  }

  bool BuiltinCanBeInlined() {
    DCHECK_EQ(IrOpcode::kJSCall, node_->opcode());
    HeapObjectMatcher m(NodeProperties::GetValueInput(node_, 0));
    Handle<JSFunction> function = Handle<JSFunction>::cast(m.Value());
    // Do not inline if the builtin may have break points.
    return !function->shared()->HasBreakInfo();
  }

  // Retrieves the BuiltinFunctionId as described above.
  BuiltinFunctionId GetBuiltinFunctionId() {
    DCHECK_EQ(IrOpcode::kJSCall, node_->opcode());
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
    DCHECK_EQ(IrOpcode::kJSCall, node_->opcode());
    // Skip first (i.e. callee) and second (i.e. receiver) operand.
    return node_->op()->ValueInputCount() - 2;
  }

  Node* GetJSCallInput(int index) {
    DCHECK_EQ(IrOpcode::kJSCall, node_->opcode());
    DCHECK_LT(index, GetJSCallArity());
    // Skip first (i.e. callee) and second (i.e. receiver) operand.
    return NodeProperties::GetValueInput(node_, index + 2);
  }

 private:
  Node* node_;
};

JSBuiltinReducer::JSBuiltinReducer(Editor* editor, JSGraph* jsgraph,
                                   CompilationDependencies* dependencies,
                                   Handle<Context> native_context)
    : AdvancedReducer(editor),
      dependencies_(dependencies),
      jsgraph_(jsgraph),
      native_context_(native_context),
      type_cache_(TypeCache::Get()) {}

namespace {

Maybe<InstanceType> GetInstanceTypeWitness(Node* node) {
  ZoneHandleSet<Map> maps;
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);
  NodeProperties::InferReceiverMapsResult result =
      NodeProperties::InferReceiverMaps(receiver, effect, &maps);

  if (result == NodeProperties::kNoReceiverMaps || maps.size() == 0) {
    return Nothing<InstanceType>();
  }

  InstanceType first_type = maps[0]->instance_type();
  for (const Handle<Map>& map : maps) {
    if (map->instance_type() != first_type) return Nothing<InstanceType>();
  }
  return Just(first_type);
}

bool CanInlineJSArrayIteration(Handle<Map> receiver_map) {
  Isolate* const isolate = receiver_map->GetIsolate();
  // Ensure that the [[Prototype]] is actually an exotic Array
  if (!receiver_map->prototype()->IsJSArray()) return false;

  // Don't inline JSArrays with slow elements of any kind
  if (!IsFastElementsKind(receiver_map->elements_kind())) return false;

  // If the receiver map has packed elements, no need to check the prototype.
  // This requires a MapCheck where this is used.
  if (!IsHoleyElementsKind(receiver_map->elements_kind())) return true;

  Handle<JSArray> receiver_prototype(JSArray::cast(receiver_map->prototype()),
                                     isolate);
  // Ensure all prototypes of the {receiver} are stable.
  for (PrototypeIterator it(isolate, receiver_prototype, kStartAtReceiver);
       !it.IsAtEnd(); it.Advance()) {
    Handle<JSReceiver> current = PrototypeIterator::GetCurrent<JSReceiver>(it);
    if (!current->map()->is_stable()) return false;
  }

  // For holey Arrays, ensure that the no_elements_protector cell is valid (must
  // be a CompilationDependency), and the JSArray prototype has not been
  // altered.
  return receiver_map->instance_type() == JS_ARRAY_TYPE &&
         (!receiver_map->is_dictionary_map() || receiver_map->is_stable()) &&
         isolate->IsNoElementsProtectorIntact() &&
         isolate->IsAnyInitialArrayPrototype(receiver_prototype);
}

}  // namespace

Reduction JSBuiltinReducer::ReduceArrayIterator(Node* node,
                                                IterationKind kind) {
  Handle<Map> receiver_map;
  if (NodeProperties::GetMapWitness(node).ToHandle(&receiver_map)) {
    return ReduceArrayIterator(receiver_map, node, kind,
                               ArrayIteratorKind::kArray);
  }
  return NoChange();
}

Reduction JSBuiltinReducer::ReduceTypedArrayIterator(Node* node,
                                                     IterationKind kind) {
  Handle<Map> receiver_map;
  if (NodeProperties::GetMapWitness(node).ToHandle(&receiver_map) &&
      receiver_map->instance_type() == JS_TYPED_ARRAY_TYPE) {
    return ReduceArrayIterator(receiver_map, node, kind,
                               ArrayIteratorKind::kTypedArray);
  }
  return NoChange();
}

Reduction JSBuiltinReducer::ReduceArrayIterator(Handle<Map> receiver_map,
                                                Node* node, IterationKind kind,
                                                ArrayIteratorKind iter_kind) {
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  if (iter_kind == ArrayIteratorKind::kTypedArray) {
    // See if we can skip the neutering check.
    if (isolate()->IsArrayBufferNeuteringIntact()) {
      // Add a code dependency so we are deoptimized in case an ArrayBuffer
      // gets neutered.
      dependencies()->AssumePropertyCell(
          factory()->array_buffer_neutering_protector());
    } else {
      // For JSTypedArray iterator methods, deopt if the buffer is neutered.
      // This is potentially a deopt loop, but should be extremely unlikely.
      DCHECK_EQ(JS_TYPED_ARRAY_TYPE, receiver_map->instance_type());
      Node* buffer = effect = graph()->NewNode(
          simplified()->LoadField(AccessBuilder::ForJSArrayBufferViewBuffer()),
          receiver, effect, control);

      // Deoptimize if the {buffer} has been neutered.
      Node* check = effect = graph()->NewNode(
          simplified()->ArrayBufferWasNeutered(), buffer, effect, control);
      check = graph()->NewNode(simplified()->BooleanNot(), check);
      effect = graph()->NewNode(
          simplified()->CheckIf(DeoptimizeReason::kArrayBufferWasNeutered),
          check, effect, control);
    }
  }

  int map_index = -1;
  Node* object_map = jsgraph()->UndefinedConstant();
  switch (receiver_map->instance_type()) {
    case JS_ARRAY_TYPE:
      if (kind == IterationKind::kKeys) {
        map_index = Context::FAST_ARRAY_KEY_ITERATOR_MAP_INDEX;
      } else {
        map_index = kind == IterationKind::kValues
                        ? Context::FAST_SMI_ARRAY_VALUE_ITERATOR_MAP_INDEX
                        : Context::FAST_SMI_ARRAY_KEY_VALUE_ITERATOR_MAP_INDEX;

        if (CanInlineJSArrayIteration(receiver_map)) {
          // Use `generic` elements for holey arrays if there may be elements
          // on the prototype chain.
          map_index += static_cast<int>(receiver_map->elements_kind());
          object_map = jsgraph()->Constant(receiver_map);
          if (IsHoleyElementsKind(receiver_map->elements_kind())) {
            Handle<JSObject> initial_array_prototype(
                native_context()->initial_array_prototype(), isolate());
            dependencies()->AssumePrototypeMapsStable(receiver_map,
                                                      initial_array_prototype);
          }
        } else {
          map_index += (Context::GENERIC_ARRAY_VALUE_ITERATOR_MAP_INDEX -
                        Context::FAST_SMI_ARRAY_VALUE_ITERATOR_MAP_INDEX);
        }
      }
      break;
    case JS_TYPED_ARRAY_TYPE:
      if (kind == IterationKind::kKeys) {
        map_index = Context::TYPED_ARRAY_KEY_ITERATOR_MAP_INDEX;
      } else {
        DCHECK_GE(receiver_map->elements_kind(), UINT8_ELEMENTS);
        DCHECK_LE(receiver_map->elements_kind(), BIGINT64_ELEMENTS);
        map_index = (kind == IterationKind::kValues
                         ? Context::UINT8_ARRAY_VALUE_ITERATOR_MAP_INDEX
                         : Context::UINT8_ARRAY_KEY_VALUE_ITERATOR_MAP_INDEX) +
                    (receiver_map->elements_kind() - UINT8_ELEMENTS);
      }
      break;
    default:
      if (kind == IterationKind::kKeys) {
        map_index = Context::GENERIC_ARRAY_KEY_ITERATOR_MAP_INDEX;
      } else if (kind == IterationKind::kValues) {
        map_index = Context::GENERIC_ARRAY_VALUE_ITERATOR_MAP_INDEX;
      } else {
        map_index = Context::GENERIC_ARRAY_KEY_VALUE_ITERATOR_MAP_INDEX;
      }
      break;
  }

  DCHECK_GE(map_index, Context::TYPED_ARRAY_KEY_ITERATOR_MAP_INDEX);
  DCHECK_LE(map_index, Context::GENERIC_ARRAY_VALUE_ITERATOR_MAP_INDEX);

  Handle<Map> map(Map::cast(native_context()->get(map_index)), isolate());

  // Allocate new iterator and attach the iterator to this object.
  AllocationBuilder a(jsgraph(), effect, control);
  a.Allocate(JSArrayIterator::kSize, NOT_TENURED, Type::OtherObject());
  a.Store(AccessBuilder::ForMap(), map);
  a.Store(AccessBuilder::ForJSObjectPropertiesOrHash(),
          jsgraph()->EmptyFixedArrayConstant());
  a.Store(AccessBuilder::ForJSObjectElements(),
          jsgraph()->EmptyFixedArrayConstant());
  a.Store(AccessBuilder::ForJSArrayIteratorObject(), receiver);
  a.Store(AccessBuilder::ForJSArrayIteratorIndex(), jsgraph()->ZeroConstant());
  a.Store(AccessBuilder::ForJSArrayIteratorObjectMap(), object_map);
  Node* value = effect = a.Finish();

  // Replace it.
  ReplaceWithValue(node, value, effect, control);
  return Replace(value);
}

Reduction JSBuiltinReducer::ReduceFastArrayIteratorNext(InstanceType type,
                                                        Node* node,
                                                        IterationKind kind) {
  Node* iterator = NodeProperties::GetValueInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* context = NodeProperties::GetContextInput(node);

  if (kind != IterationKind::kKeys &&
      !isolate()->IsFastArrayIterationIntact()) {
    // Avoid deopt loops for non-key iteration if the
    // fast_array_iteration_protector cell has been invalidated.
    return NoChange();
  }

  ElementsKind elements_kind =
      JSArrayIterator::ElementsKindForInstanceType(type);

  if (IsHoleyElementsKind(elements_kind)) {
    if (!isolate()->IsNoElementsProtectorIntact()) {
      return NoChange();
    } else {
      Handle<JSObject> initial_array_prototype(
          native_context()->initial_array_prototype(), isolate());
      dependencies()->AssumePropertyCell(factory()->no_elements_protector());
    }
  }

  Node* array = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForJSArrayIteratorObject()),
      iterator, effect, control);
  Node* check0 = graph()->NewNode(simplified()->ReferenceEqual(), array,
                                  jsgraph()->UndefinedConstant());
  Node* branch0 =
      graph()->NewNode(common()->Branch(BranchHint::kFalse), check0, control);

  Node* vdone_false0;
  Node* vfalse0;
  Node* efalse0 = effect;
  Node* if_false0 = graph()->NewNode(common()->IfFalse(), branch0);
  {
    // iterator.[[IteratedObject]] !== undefined, continue iterating.
    Node* index = efalse0 = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForJSArrayIteratorIndex(
            JS_ARRAY_TYPE, elements_kind)),
        iterator, efalse0, if_false0);

    Node* length = efalse0 = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForJSArrayLength(elements_kind)),
        array, efalse0, if_false0);
    Node* check1 =
        graph()->NewNode(simplified()->NumberLessThan(), index, length);
    Node* branch1 = graph()->NewNode(common()->Branch(BranchHint::kTrue),
                                     check1, if_false0);

    Node* vdone_true1;
    Node* vtrue1;
    Node* etrue1 = efalse0;
    Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
    {
      // iterator.[[NextIndex]] < array.length, continue iterating
      vdone_true1 = jsgraph()->FalseConstant();
      if (kind == IterationKind::kKeys) {
        vtrue1 = index;
      } else {
        // For value/entry iteration, first step is a mapcheck to ensure
        // inlining is still valid.
        Node* array_map = etrue1 =
            graph()->NewNode(simplified()->LoadField(AccessBuilder::ForMap()),
                             array, etrue1, if_true1);
        Node* orig_map = etrue1 =
            graph()->NewNode(simplified()->LoadField(
                                 AccessBuilder::ForJSArrayIteratorObjectMap()),
                             iterator, etrue1, if_true1);
        Node* check_map = graph()->NewNode(simplified()->ReferenceEqual(),
                                           array_map, orig_map);
        etrue1 =
            graph()->NewNode(simplified()->CheckIf(DeoptimizeReason::kWrongMap),
                             check_map, etrue1, if_true1);
      }

      if (kind != IterationKind::kKeys) {
        Node* elements = etrue1 = graph()->NewNode(
            simplified()->LoadField(AccessBuilder::ForJSObjectElements()),
            array, etrue1, if_true1);
        Node* value = etrue1 = graph()->NewNode(
            simplified()->LoadElement(
                AccessBuilder::ForFixedArrayElement(elements_kind)),
            elements, index, etrue1, if_true1);

        // Convert hole to undefined if needed.
        if (elements_kind == HOLEY_ELEMENTS ||
            elements_kind == HOLEY_SMI_ELEMENTS) {
          value = graph()->NewNode(simplified()->ConvertTaggedHoleToUndefined(),
                                   value);
        } else if (elements_kind == HOLEY_DOUBLE_ELEMENTS) {
          // TODO(6587): avoid deopt if not all uses of value are truncated.
          CheckFloat64HoleMode mode = CheckFloat64HoleMode::kAllowReturnHole;
          value = etrue1 = graph()->NewNode(
              simplified()->CheckFloat64Hole(mode), value, etrue1, if_true1);
        }

        if (kind == IterationKind::kEntries) {
          // Allocate elements for key/value pair
          vtrue1 = etrue1 =
              graph()->NewNode(javascript()->CreateKeyValueArray(), index,
                               value, context, etrue1);
        } else {
          DCHECK_EQ(kind, IterationKind::kValues);
          vtrue1 = value;
        }
      }

      Node* next_index = graph()->NewNode(simplified()->NumberAdd(), index,
                                          jsgraph()->OneConstant());
      next_index = graph()->NewNode(simplified()->NumberToUint32(), next_index);

      etrue1 = graph()->NewNode(
          simplified()->StoreField(AccessBuilder::ForJSArrayIteratorIndex(
              JS_ARRAY_TYPE, elements_kind)),
          iterator, next_index, etrue1, if_true1);
    }

    Node* vdone_false1;
    Node* vfalse1;
    Node* efalse1 = efalse0;
    Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
    {
      // iterator.[[NextIndex]] >= array.length, stop iterating.
      vdone_false1 = jsgraph()->TrueConstant();
      vfalse1 = jsgraph()->UndefinedConstant();
      efalse1 = graph()->NewNode(
          simplified()->StoreField(AccessBuilder::ForJSArrayIteratorObject()),
          iterator, vfalse1, efalse1, if_false1);
    }

    if_false0 = graph()->NewNode(common()->Merge(2), if_true1, if_false1);
    efalse0 =
        graph()->NewNode(common()->EffectPhi(2), etrue1, efalse1, if_false0);
    vfalse0 = graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                               vtrue1, vfalse1, if_false0);
    vdone_false0 =
        graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                         vdone_true1, vdone_false1, if_false0);
  }

  Node* vdone_true0;
  Node* vtrue0;
  Node* etrue0 = effect;
  Node* if_true0 = graph()->NewNode(common()->IfTrue(), branch0);
  {
    // iterator.[[IteratedObject]] === undefined, the iterator is done.
    vdone_true0 = jsgraph()->TrueConstant();
    vtrue0 = jsgraph()->UndefinedConstant();
  }

  control = graph()->NewNode(common()->Merge(2), if_false0, if_true0);
  effect = graph()->NewNode(common()->EffectPhi(2), efalse0, etrue0, control);
  Node* value =
      graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                       vfalse0, vtrue0, control);
  Node* done =
      graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                       vdone_false0, vdone_true0, control);

  // Create IteratorResult object.
  value = effect = graph()->NewNode(javascript()->CreateIterResultObject(),
                                    value, done, context, effect);
  ReplaceWithValue(node, value, effect, control);
  return Replace(value);
}

Reduction JSBuiltinReducer::ReduceTypedArrayIteratorNext(InstanceType type,
                                                         Node* node,
                                                         IterationKind kind) {
  Node* iterator = NodeProperties::GetValueInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* context = NodeProperties::GetContextInput(node);

  ElementsKind elements_kind =
      JSArrayIterator::ElementsKindForInstanceType(type);

  Node* array = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForJSArrayIteratorObject()),
      iterator, effect, control);
  Node* check0 = graph()->NewNode(simplified()->ReferenceEqual(), array,
                                  jsgraph()->UndefinedConstant());
  Node* branch0 =
      graph()->NewNode(common()->Branch(BranchHint::kFalse), check0, control);

  Node* vdone_false0;
  Node* vfalse0;
  Node* efalse0 = effect;
  Node* if_false0 = graph()->NewNode(common()->IfFalse(), branch0);
  {
    // iterator.[[IteratedObject]] !== undefined, continue iterating.
    Node* index = efalse0 = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForJSArrayIteratorIndex(
            JS_TYPED_ARRAY_TYPE, elements_kind)),
        iterator, efalse0, if_false0);

    // typedarray.[[ViewedArrayBuffer]]
    Node* buffer = efalse0 = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForJSArrayBufferViewBuffer()),
        array, efalse0, if_false0);

    // See if we can skip the neutering check.
    if (isolate()->IsArrayBufferNeuteringIntact()) {
      // Add a code dependency so we are deoptimized in case an ArrayBuffer
      // gets neutered.
      dependencies()->AssumePropertyCell(
          factory()->array_buffer_neutering_protector());
    } else {
      // Deoptimize if the array buffer was neutered.
      Node* check1 = efalse0 = graph()->NewNode(
          simplified()->ArrayBufferWasNeutered(), buffer, efalse0, if_false0);
      check1 = graph()->NewNode(simplified()->BooleanNot(), check1);
      efalse0 = graph()->NewNode(
          simplified()->CheckIf(DeoptimizeReason::kArrayBufferWasNeutered),
          check1, efalse0, if_false0);
    }

    Node* length = efalse0 = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForJSTypedArrayLength()), array,
        efalse0, if_false0);

    Node* check2 =
        graph()->NewNode(simplified()->NumberLessThan(), index, length);
    Node* branch2 = graph()->NewNode(common()->Branch(BranchHint::kTrue),
                                     check2, if_false0);

    Node* vdone_true2;
    Node* vtrue2;
    Node* etrue2 = efalse0;
    Node* if_true2 = graph()->NewNode(common()->IfTrue(), branch2);
    {
      // iterator.[[NextIndex]] < array.length, continue iterating
      vdone_true2 = jsgraph()->FalseConstant();
      if (kind == IterationKind::kKeys) {
        vtrue2 = index;
      }

      Node* next_index = graph()->NewNode(simplified()->NumberAdd(), index,
                                          jsgraph()->OneConstant());
      next_index = graph()->NewNode(simplified()->NumberToUint32(), next_index);

      etrue2 = graph()->NewNode(
          simplified()->StoreField(AccessBuilder::ForJSArrayIteratorIndex(
              JS_TYPED_ARRAY_TYPE, elements_kind)),
          iterator, next_index, etrue2, if_true2);

      if (kind != IterationKind::kKeys) {
        Node* elements = etrue2 = graph()->NewNode(
            simplified()->LoadField(AccessBuilder::ForJSObjectElements()),
            array, etrue2, if_true2);
        Node* base_ptr = etrue2 = graph()->NewNode(
            simplified()->LoadField(
                AccessBuilder::ForFixedTypedArrayBaseBasePointer()),
            elements, etrue2, if_true2);
        Node* external_ptr = etrue2 = graph()->NewNode(
            simplified()->LoadField(
                AccessBuilder::ForFixedTypedArrayBaseExternalPointer()),
            elements, etrue2, if_true2);

        ExternalArrayType array_type = kExternalInt8Array;
        switch (elements_kind) {
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size) \
  case TYPE##_ELEMENTS:                                 \
    array_type = kExternal##Type##Array;                \
    break;
          TYPED_ARRAYS(TYPED_ARRAY_CASE)
          default:
            UNREACHABLE();
#undef TYPED_ARRAY_CASE
        }

        Node* value = etrue2 =
            graph()->NewNode(simplified()->LoadTypedElement(array_type), buffer,
                             base_ptr, external_ptr, index, etrue2, if_true2);

        if (kind == IterationKind::kEntries) {
          // Allocate elements for key/value pair
          vtrue2 = etrue2 =
              graph()->NewNode(javascript()->CreateKeyValueArray(), index,
                               value, context, etrue2);
        } else {
          DCHECK_EQ(IterationKind::kValues, kind);
          vtrue2 = value;
        }
      }
    }

    Node* vdone_false2;
    Node* vfalse2;
    Node* efalse2 = efalse0;
    Node* if_false2 = graph()->NewNode(common()->IfFalse(), branch2);
    {
      // iterator.[[NextIndex]] >= array.length, stop iterating.
      vdone_false2 = jsgraph()->TrueConstant();
      vfalse2 = jsgraph()->UndefinedConstant();
      efalse2 = graph()->NewNode(
          simplified()->StoreField(AccessBuilder::ForJSArrayIteratorObject()),
          iterator, vfalse2, efalse2, if_false2);
    }

    if_false0 = graph()->NewNode(common()->Merge(2), if_true2, if_false2);
    efalse0 =
        graph()->NewNode(common()->EffectPhi(2), etrue2, efalse2, if_false0);
    vfalse0 = graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                               vtrue2, vfalse2, if_false0);
    vdone_false0 =
        graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                         vdone_true2, vdone_false2, if_false0);
  }

  Node* vdone_true0;
  Node* vtrue0;
  Node* etrue0 = effect;
  Node* if_true0 = graph()->NewNode(common()->IfTrue(), branch0);
  {
    // iterator.[[IteratedObject]] === undefined, the iterator is done.
    vdone_true0 = jsgraph()->TrueConstant();
    vtrue0 = jsgraph()->UndefinedConstant();
  }

  control = graph()->NewNode(common()->Merge(2), if_false0, if_true0);
  effect = graph()->NewNode(common()->EffectPhi(2), efalse0, etrue0, control);
  Node* value =
      graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                       vfalse0, vtrue0, control);
  Node* done =
      graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                       vdone_false0, vdone_true0, control);

  // Create IteratorResult object.
  value = effect = graph()->NewNode(javascript()->CreateIterResultObject(),
                                    value, done, context, effect);
  ReplaceWithValue(node, value, effect, control);
  return Replace(value);
}

// ES #sec-get-%typedarray%.prototype-@@tostringtag
Reduction JSBuiltinReducer::ReduceTypedArrayToStringTag(Node* node) {
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  NodeVector values(graph()->zone());
  NodeVector effects(graph()->zone());
  NodeVector controls(graph()->zone());

  Node* check = graph()->NewNode(simplified()->ObjectIsSmi(), receiver);
  control =
      graph()->NewNode(common()->Branch(BranchHint::kFalse), check, control);

  values.push_back(jsgraph()->UndefinedConstant());
  effects.push_back(effect);
  controls.push_back(graph()->NewNode(common()->IfTrue(), control));

  control = graph()->NewNode(common()->IfFalse(), control);
  Node* receiver_map = effect =
      graph()->NewNode(simplified()->LoadField(AccessBuilder::ForMap()),
                       receiver, effect, control);
  Node* receiver_bit_field2 = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForMapBitField2()), receiver_map,
      effect, control);
  Node* receiver_elements_kind = graph()->NewNode(
      simplified()->NumberShiftRightLogical(),
      graph()->NewNode(simplified()->NumberBitwiseAnd(), receiver_bit_field2,
                       jsgraph()->Constant(Map::ElementsKindBits::kMask)),
      jsgraph()->Constant(Map::ElementsKindBits::kShift));

  // Offset the elements kind by FIRST_FIXED_TYPED_ARRAY_ELEMENTS_KIND,
  // so that the branch cascade below is turned into a simple table
  // switch by the ControlFlowOptimizer later.
  receiver_elements_kind = graph()->NewNode(
      simplified()->NumberSubtract(), receiver_elements_kind,
      jsgraph()->Constant(FIRST_FIXED_TYPED_ARRAY_ELEMENTS_KIND));

#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size)                \
  do {                                                                 \
    Node* check = graph()->NewNode(                                    \
        simplified()->NumberEqual(), receiver_elements_kind,           \
        jsgraph()->Constant(TYPE##_ELEMENTS -                          \
                            FIRST_FIXED_TYPED_ARRAY_ELEMENTS_KIND));   \
    control = graph()->NewNode(common()->Branch(), check, control);    \
    values.push_back(jsgraph()->HeapConstant(                          \
        factory()->InternalizeUtf8String(#Type "Array")));             \
    effects.push_back(effect);                                         \
    controls.push_back(graph()->NewNode(common()->IfTrue(), control)); \
    control = graph()->NewNode(common()->IfFalse(), control);          \
  } while (false);
  TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE

  values.push_back(jsgraph()->UndefinedConstant());
  effects.push_back(effect);
  controls.push_back(control);

  int const count = static_cast<int>(controls.size());
  control = graph()->NewNode(common()->Merge(count), count, &controls.front());
  effects.push_back(control);
  effect =
      graph()->NewNode(common()->EffectPhi(count), count + 1, &effects.front());
  values.push_back(control);
  Node* value =
      graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, count),
                       count + 1, &values.front());
  ReplaceWithValue(node, value, effect, control);
  return Replace(value);
}

Reduction JSBuiltinReducer::ReduceArrayIteratorNext(Node* node) {
  Maybe<InstanceType> maybe_type = GetInstanceTypeWitness(node);
  if (!maybe_type.IsJust()) return NoChange();
  InstanceType type = maybe_type.FromJust();
  switch (type) {
    case JS_TYPED_ARRAY_KEY_ITERATOR_TYPE:
      return ReduceTypedArrayIteratorNext(type, node, IterationKind::kKeys);

    case JS_FAST_ARRAY_KEY_ITERATOR_TYPE:
      return ReduceFastArrayIteratorNext(type, node, IterationKind::kKeys);

    case JS_INT8_ARRAY_KEY_VALUE_ITERATOR_TYPE:
    case JS_UINT8_ARRAY_KEY_VALUE_ITERATOR_TYPE:
    case JS_INT16_ARRAY_KEY_VALUE_ITERATOR_TYPE:
    case JS_UINT16_ARRAY_KEY_VALUE_ITERATOR_TYPE:
    case JS_INT32_ARRAY_KEY_VALUE_ITERATOR_TYPE:
    case JS_UINT32_ARRAY_KEY_VALUE_ITERATOR_TYPE:
    case JS_FLOAT32_ARRAY_KEY_VALUE_ITERATOR_TYPE:
    case JS_FLOAT64_ARRAY_KEY_VALUE_ITERATOR_TYPE:
    case JS_UINT8_CLAMPED_ARRAY_KEY_VALUE_ITERATOR_TYPE:
      return ReduceTypedArrayIteratorNext(type, node, IterationKind::kEntries);

    case JS_FAST_SMI_ARRAY_KEY_VALUE_ITERATOR_TYPE:
    case JS_FAST_HOLEY_SMI_ARRAY_KEY_VALUE_ITERATOR_TYPE:
    case JS_FAST_ARRAY_KEY_VALUE_ITERATOR_TYPE:
    case JS_FAST_HOLEY_ARRAY_KEY_VALUE_ITERATOR_TYPE:
    case JS_FAST_DOUBLE_ARRAY_KEY_VALUE_ITERATOR_TYPE:
    case JS_FAST_HOLEY_DOUBLE_ARRAY_KEY_VALUE_ITERATOR_TYPE:
      return ReduceFastArrayIteratorNext(type, node, IterationKind::kEntries);

    case JS_INT8_ARRAY_VALUE_ITERATOR_TYPE:
    case JS_UINT8_ARRAY_VALUE_ITERATOR_TYPE:
    case JS_INT16_ARRAY_VALUE_ITERATOR_TYPE:
    case JS_UINT16_ARRAY_VALUE_ITERATOR_TYPE:
    case JS_INT32_ARRAY_VALUE_ITERATOR_TYPE:
    case JS_UINT32_ARRAY_VALUE_ITERATOR_TYPE:
    case JS_FLOAT32_ARRAY_VALUE_ITERATOR_TYPE:
    case JS_FLOAT64_ARRAY_VALUE_ITERATOR_TYPE:
    case JS_UINT8_CLAMPED_ARRAY_VALUE_ITERATOR_TYPE:
      return ReduceTypedArrayIteratorNext(type, node, IterationKind::kValues);

    case JS_FAST_SMI_ARRAY_VALUE_ITERATOR_TYPE:
    case JS_FAST_HOLEY_SMI_ARRAY_VALUE_ITERATOR_TYPE:
    case JS_FAST_ARRAY_VALUE_ITERATOR_TYPE:
    case JS_FAST_HOLEY_ARRAY_VALUE_ITERATOR_TYPE:
    case JS_FAST_DOUBLE_ARRAY_VALUE_ITERATOR_TYPE:
    case JS_FAST_HOLEY_DOUBLE_ARRAY_VALUE_ITERATOR_TYPE:
      return ReduceFastArrayIteratorNext(type, node, IterationKind::kValues);

    default:
      // Slow array iterators are not reduced
      return NoChange();
  }
}

// ES6 section 22.1.2.2 Array.isArray ( arg )
Reduction JSBuiltinReducer::ReduceArrayIsArray(Node* node) {
  // We certainly know that undefined is not an array.
  if (node->op()->ValueInputCount() < 3) {
    Node* value = jsgraph()->FalseConstant();
    ReplaceWithValue(node, value);
    return Replace(value);
  }
  Node* value = NodeProperties::GetValueInput(node, 2);
  Type* value_type = NodeProperties::GetType(value);
  Node* context = NodeProperties::GetContextInput(node);
  Node* frame_state = NodeProperties::GetFrameStateInput(node);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // Constant-fold based on {value} type.
  if (value_type->Is(Type::Array())) {
    Node* value = jsgraph()->TrueConstant();
    ReplaceWithValue(node, value);
    return Replace(value);
  } else if (!value_type->Maybe(Type::ArrayOrProxy())) {
    Node* value = jsgraph()->FalseConstant();
    ReplaceWithValue(node, value);
    return Replace(value);
  }

  int count = 0;
  Node* values[5];
  Node* effects[5];
  Node* controls[4];

  // Check if the {value} is a Smi.
  Node* check = graph()->NewNode(simplified()->ObjectIsSmi(), value);
  control =
      graph()->NewNode(common()->Branch(BranchHint::kFalse), check, control);

  // The {value} is a Smi.
  controls[count] = graph()->NewNode(common()->IfTrue(), control);
  effects[count] = effect;
  values[count] = jsgraph()->FalseConstant();
  count++;

  control = graph()->NewNode(common()->IfFalse(), control);

  // Load the {value}s instance type.
  Node* value_map = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForMap()), value, effect, control);
  Node* value_instance_type = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForMapInstanceType()), value_map,
      effect, control);

  // Check if the {value} is a JSArray.
  check = graph()->NewNode(simplified()->NumberEqual(), value_instance_type,
                           jsgraph()->Constant(JS_ARRAY_TYPE));
  control = graph()->NewNode(common()->Branch(), check, control);

  // The {value} is a JSArray.
  controls[count] = graph()->NewNode(common()->IfTrue(), control);
  effects[count] = effect;
  values[count] = jsgraph()->TrueConstant();
  count++;

  control = graph()->NewNode(common()->IfFalse(), control);

  // Check if the {value} is a JSProxy.
  check = graph()->NewNode(simplified()->NumberEqual(), value_instance_type,
                           jsgraph()->Constant(JS_PROXY_TYPE));
  control =
      graph()->NewNode(common()->Branch(BranchHint::kFalse), check, control);

  // The {value} is neither a JSArray nor a JSProxy.
  controls[count] = graph()->NewNode(common()->IfFalse(), control);
  effects[count] = effect;
  values[count] = jsgraph()->FalseConstant();
  count++;

  control = graph()->NewNode(common()->IfTrue(), control);

  // Let the %ArrayIsArray runtime function deal with the JSProxy {value}.
  value = effect = control =
      graph()->NewNode(javascript()->CallRuntime(Runtime::kArrayIsArray), value,
                       context, frame_state, effect, control);
  NodeProperties::SetType(value, Type::Boolean());

  // Update potential {IfException} uses of {node} to point to the above
  // %ArrayIsArray runtime call node instead.
  Node* on_exception = nullptr;
  if (NodeProperties::IsExceptionalCall(node, &on_exception)) {
    NodeProperties::ReplaceControlInput(on_exception, control);
    NodeProperties::ReplaceEffectInput(on_exception, effect);
    control = graph()->NewNode(common()->IfSuccess(), control);
    Revisit(on_exception);
  }

  // The {value} is neither a JSArray nor a JSProxy.
  controls[count] = control;
  effects[count] = effect;
  values[count] = value;
  count++;

  control = graph()->NewNode(common()->Merge(count), count, controls);
  effects[count] = control;
  values[count] = control;
  effect = graph()->NewNode(common()->EffectPhi(count), count + 1, effects);
  value = graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, count),
                           count + 1, values);
  ReplaceWithValue(node, value, effect, control);
  return Replace(value);
}

Reduction JSBuiltinReducer::ReduceCollectionIterator(
    Node* node, InstanceType collection_instance_type,
    int collection_iterator_map_index) {
  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  if (NodeProperties::HasInstanceTypeWitness(receiver, effect,
                                             collection_instance_type)) {
    // Figure out the proper collection iterator map.
    Handle<Map> collection_iterator_map(
        Map::cast(native_context()->get(collection_iterator_map_index)),
        isolate());

    // Load the OrderedHashTable from the {receiver}.
    Node* table = effect = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForJSCollectionTable()),
        receiver, effect, control);

    // Create the JSCollectionIterator result.
    AllocationBuilder a(jsgraph(), effect, control);
    a.Allocate(JSCollectionIterator::kSize, NOT_TENURED, Type::OtherObject());
    a.Store(AccessBuilder::ForMap(), collection_iterator_map);
    a.Store(AccessBuilder::ForJSObjectPropertiesOrHash(),
            jsgraph()->EmptyFixedArrayConstant());
    a.Store(AccessBuilder::ForJSObjectElements(),
            jsgraph()->EmptyFixedArrayConstant());
    a.Store(AccessBuilder::ForJSCollectionIteratorTable(), table);
    a.Store(AccessBuilder::ForJSCollectionIteratorIndex(),
            jsgraph()->ZeroConstant());
    Node* value = effect = a.Finish();
    ReplaceWithValue(node, value, effect, control);
    return Replace(value);
  }
  return NoChange();
}

Reduction JSBuiltinReducer::ReduceCollectionSize(
    Node* node, InstanceType collection_instance_type) {
  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  if (NodeProperties::HasInstanceTypeWitness(receiver, effect,
                                             collection_instance_type)) {
    Node* table = effect = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForJSCollectionTable()),
        receiver, effect, control);
    Node* value = effect = graph()->NewNode(
        simplified()->LoadField(
            AccessBuilder::ForOrderedHashTableBaseNumberOfElements()),
        table, effect, control);
    ReplaceWithValue(node, value, effect, control);
    return Replace(value);
  }
  return NoChange();
}

Reduction JSBuiltinReducer::ReduceCollectionIteratorNext(
    Node* node, int entry_size, Handle<HeapObject> empty_collection,
    InstanceType collection_iterator_instance_type_first,
    InstanceType collection_iterator_instance_type_last) {
  DCHECK_EQ(IrOpcode::kJSCall, node->opcode());
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* context = NodeProperties::GetContextInput(node);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // A word of warning to begin with: This whole method might look a bit
  // strange at times, but that's mostly because it was carefully handcrafted
  // to allow for full escape analysis and scalar replacement of both the
  // collection iterator object and the iterator results, including the
  // key-value arrays in case of Set/Map entry iteration.
  //
  // TODO(turbofan): Currently the escape analysis (and the store-load
  // forwarding) is unable to eliminate the allocations for the key-value
  // arrays in case of Set/Map entry iteration, and we should investigate
  // how to update the escape analysis / arrange the graph in a way that
  // this becomes possible.

  // Infer the {receiver} instance type.
  InstanceType receiver_instance_type;
  ZoneHandleSet<Map> receiver_maps;
  NodeProperties::InferReceiverMapsResult result =
      NodeProperties::InferReceiverMaps(receiver, effect, &receiver_maps);
  if (result == NodeProperties::kNoReceiverMaps) return NoChange();
  DCHECK_NE(0, receiver_maps.size());
  receiver_instance_type = receiver_maps[0]->instance_type();
  for (size_t i = 1; i < receiver_maps.size(); ++i) {
    if (receiver_maps[i]->instance_type() != receiver_instance_type) {
      return NoChange();
    }
  }
  if (receiver_instance_type < collection_iterator_instance_type_first ||
      receiver_instance_type > collection_iterator_instance_type_last) {
    return NoChange();
  }

  // Transition the JSCollectionIterator {receiver} if necessary
  // (i.e. there were certain mutations while we're iterating).
  {
    Node* done_loop;
    Node* done_eloop;
    Node* loop = control =
        graph()->NewNode(common()->Loop(2), control, control);
    Node* eloop = effect =
        graph()->NewNode(common()->EffectPhi(2), effect, effect, loop);
    Node* terminate = graph()->NewNode(common()->Terminate(), eloop, loop);
    NodeProperties::MergeControlToEnd(graph(), common(), terminate);

    // Check if reached the final table of the {receiver}.
    Node* table = effect = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForJSCollectionIteratorTable()),
        receiver, effect, control);
    Node* next_table = effect =
        graph()->NewNode(simplified()->LoadField(
                             AccessBuilder::ForOrderedHashTableBaseNextTable()),
                         table, effect, control);
    Node* check = graph()->NewNode(simplified()->ObjectIsSmi(), next_table);
    control =
        graph()->NewNode(common()->Branch(BranchHint::kTrue), check, control);

    // Abort the {loop} when we reach the final table.
    done_loop = graph()->NewNode(common()->IfTrue(), control);
    done_eloop = effect;

    // Migrate to the {next_table} otherwise.
    control = graph()->NewNode(common()->IfFalse(), control);

    // Self-heal the {receiver}s index.
    Node* index = effect = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForJSCollectionIteratorIndex()),
        receiver, effect, control);
    Callable const callable =
        Builtins::CallableFor(isolate(), Builtins::kOrderedHashTableHealIndex);
    auto call_descriptor = Linkage::GetStubCallDescriptor(
        isolate(), graph()->zone(), callable.descriptor(), 0,
        CallDescriptor::kNoFlags, Operator::kEliminatable);
    index = effect =
        graph()->NewNode(common()->Call(call_descriptor),
                         jsgraph()->HeapConstant(callable.code()), table, index,
                         jsgraph()->NoContextConstant(), effect);
    NodeProperties::SetType(index, type_cache_.kFixedArrayLengthType);

    // Update the {index} and {table} on the {receiver}.
    effect = graph()->NewNode(
        simplified()->StoreField(AccessBuilder::ForJSCollectionIteratorIndex()),
        receiver, index, effect, control);
    effect = graph()->NewNode(
        simplified()->StoreField(AccessBuilder::ForJSCollectionIteratorTable()),
        receiver, next_table, effect, control);

    // Tie the knot.
    loop->ReplaceInput(1, control);
    eloop->ReplaceInput(1, effect);

    control = done_loop;
    effect = done_eloop;
  }

  // Get current index and table from the JSCollectionIterator {receiver}.
  Node* index = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForJSCollectionIteratorIndex()),
      receiver, effect, control);
  Node* table = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForJSCollectionIteratorTable()),
      receiver, effect, control);

  // Create the {JSIteratorResult} first to ensure that we always have
  // a dominating Allocate node for the allocation folding phase.
  Node* iterator_result = effect = graph()->NewNode(
      javascript()->CreateIterResultObject(), jsgraph()->UndefinedConstant(),
      jsgraph()->TrueConstant(), context, effect);

  // Look for the next non-holey key, starting from {index} in the {table}.
  Node* controls[2];
  Node* effects[3];
  {
    // Compute the currently used capacity.
    Node* number_of_buckets = effect = graph()->NewNode(
        simplified()->LoadField(
            AccessBuilder::ForOrderedHashTableBaseNumberOfBuckets()),
        table, effect, control);
    Node* number_of_elements = effect = graph()->NewNode(
        simplified()->LoadField(
            AccessBuilder::ForOrderedHashTableBaseNumberOfElements()),
        table, effect, control);
    Node* number_of_deleted_elements = effect = graph()->NewNode(
        simplified()->LoadField(
            AccessBuilder::ForOrderedHashTableBaseNumberOfDeletedElements()),
        table, effect, control);
    Node* used_capacity =
        graph()->NewNode(simplified()->NumberAdd(), number_of_elements,
                         number_of_deleted_elements);

    // Skip holes and update the {index}.
    Node* loop = graph()->NewNode(common()->Loop(2), control, control);
    Node* eloop =
        graph()->NewNode(common()->EffectPhi(2), effect, effect, loop);
    Node* terminate = graph()->NewNode(common()->Terminate(), eloop, loop);
    NodeProperties::MergeControlToEnd(graph(), common(), terminate);
    Node* iloop = graph()->NewNode(
        common()->Phi(MachineRepresentation::kTagged, 2), index, index, loop);
    NodeProperties::SetType(iloop, type_cache_.kFixedArrayLengthType);
    {
      Node* check0 = graph()->NewNode(simplified()->NumberLessThan(), iloop,
                                      used_capacity);
      Node* branch0 =
          graph()->NewNode(common()->Branch(BranchHint::kTrue), check0, loop);

      Node* if_false0 = graph()->NewNode(common()->IfFalse(), branch0);
      Node* efalse0 = eloop;
      {
        // Mark the {receiver} as exhausted.
        efalse0 = graph()->NewNode(
            simplified()->StoreField(
                AccessBuilder::ForJSCollectionIteratorTable()),
            receiver, jsgraph()->HeapConstant(empty_collection), efalse0,
            if_false0);

        controls[0] = if_false0;
        effects[0] = efalse0;
      }

      Node* if_true0 = graph()->NewNode(common()->IfTrue(), branch0);
      Node* etrue0 = eloop;
      {
        // Load the key of the entry.
        Node* entry_start_position = graph()->NewNode(
            simplified()->NumberAdd(),
            graph()->NewNode(
                simplified()->NumberAdd(),
                graph()->NewNode(simplified()->NumberMultiply(), iloop,
                                 jsgraph()->Constant(entry_size)),
                number_of_buckets),
            jsgraph()->Constant(OrderedHashTableBase::kHashTableStartIndex));
        Node* entry_key = etrue0 = graph()->NewNode(
            simplified()->LoadElement(AccessBuilder::ForFixedArrayElement()),
            table, entry_start_position, etrue0, if_true0);

        // Advance the index.
        Node* index = graph()->NewNode(simplified()->NumberAdd(), iloop,
                                       jsgraph()->OneConstant());

        Node* check1 =
            graph()->NewNode(simplified()->ReferenceEqual(), entry_key,
                             jsgraph()->TheHoleConstant());
        Node* branch1 = graph()->NewNode(common()->Branch(BranchHint::kFalse),
                                         check1, if_true0);

        {
          // Abort loop with resulting value.
          Node* control = graph()->NewNode(common()->IfFalse(), branch1);
          Node* effect = etrue0;
          Node* value = effect =
              graph()->NewNode(common()->TypeGuard(Type::NonInternal()),
                               entry_key, effect, control);
          Node* done = jsgraph()->FalseConstant();

          // Advance the index on the {receiver}.
          effect = graph()->NewNode(
              simplified()->StoreField(
                  AccessBuilder::ForJSCollectionIteratorIndex()),
              receiver, index, effect, control);

          // The actual {value} depends on the {receiver} iteration type.
          switch (receiver_instance_type) {
            case JS_MAP_KEY_ITERATOR_TYPE:
            case JS_SET_VALUE_ITERATOR_TYPE:
              break;

            case JS_SET_KEY_VALUE_ITERATOR_TYPE:
              value = effect =
                  graph()->NewNode(javascript()->CreateKeyValueArray(), value,
                                   value, context, effect);
              break;

            case JS_MAP_VALUE_ITERATOR_TYPE:
              value = effect = graph()->NewNode(
                  simplified()->LoadElement(
                      AccessBuilder::ForFixedArrayElement()),
                  table,
                  graph()->NewNode(
                      simplified()->NumberAdd(), entry_start_position,
                      jsgraph()->Constant(OrderedHashMap::kValueOffset)),
                  effect, control);
              break;

            case JS_MAP_KEY_VALUE_ITERATOR_TYPE:
              value = effect = graph()->NewNode(
                  simplified()->LoadElement(
                      AccessBuilder::ForFixedArrayElement()),
                  table,
                  graph()->NewNode(
                      simplified()->NumberAdd(), entry_start_position,
                      jsgraph()->Constant(OrderedHashMap::kValueOffset)),
                  effect, control);
              value = effect =
                  graph()->NewNode(javascript()->CreateKeyValueArray(),
                                   entry_key, value, context, effect);
              break;

            default:
              UNREACHABLE();
              break;
          }

          // Store final {value} and {done} into the {iterator_result}.
          effect =
              graph()->NewNode(simplified()->StoreField(
                                   AccessBuilder::ForJSIteratorResultValue()),
                               iterator_result, value, effect, control);
          effect =
              graph()->NewNode(simplified()->StoreField(
                                   AccessBuilder::ForJSIteratorResultDone()),
                               iterator_result, done, effect, control);

          controls[1] = control;
          effects[1] = effect;
        }

        // Continue with next loop index.
        loop->ReplaceInput(1, graph()->NewNode(common()->IfTrue(), branch1));
        eloop->ReplaceInput(1, etrue0);
        iloop->ReplaceInput(1, index);
      }
    }

    control = effects[2] = graph()->NewNode(common()->Merge(2), 2, controls);
    effect = graph()->NewNode(common()->EffectPhi(2), 3, effects);
  }

  // Yield the final {iterator_result}.
  ReplaceWithValue(node, iterator_result, effect, control);
  return Replace(iterator_result);
}

// ES6 section 20.3.3.1 Date.now ( )
Reduction JSBuiltinReducer::ReduceDateNow(Node* node) {
  NodeProperties::RemoveValueInputs(node);
  NodeProperties::ChangeOp(
      node, javascript()->CallRuntime(Runtime::kDateCurrentTime));
  return Changed(node);
}

// ES6 section 20.3.4.10 Date.prototype.getTime ( )
Reduction JSBuiltinReducer::ReduceDateGetTime(Node* node) {
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  if (NodeProperties::HasInstanceTypeWitness(receiver, effect, JS_DATE_TYPE)) {
    Node* value = effect = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForJSDateValue()), receiver,
        effect, control);
    ReplaceWithValue(node, value, effect, control);
    return Replace(value);
  }
  return NoChange();
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

Reduction JSBuiltinReducer::ReduceMapGet(Node* node) {
  // We only optimize if we have target, receiver and key parameters.
  if (node->op()->ValueInputCount() != 3) return NoChange();
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* key = NodeProperties::GetValueInput(node, 2);

  if (!NodeProperties::HasInstanceTypeWitness(receiver, effect, JS_MAP_TYPE))
    return NoChange();

  Node* table = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForJSCollectionTable()), receiver,
      effect, control);

  Node* entry = effect = graph()->NewNode(
      simplified()->FindOrderedHashMapEntry(), table, key, effect, control);

  Node* check = graph()->NewNode(simplified()->NumberEqual(), entry,
                                 jsgraph()->MinusOneConstant());

  Node* branch = graph()->NewNode(common()->Branch(), check, control);

  // Key not found.
  Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
  Node* etrue = effect;
  Node* vtrue = jsgraph()->UndefinedConstant();

  // Key found.
  Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
  Node* efalse = effect;
  Node* vfalse = efalse = graph()->NewNode(
      simplified()->LoadElement(AccessBuilder::ForOrderedHashMapEntryValue()),
      table, entry, efalse, if_false);

  control = graph()->NewNode(common()->Merge(2), if_true, if_false);
  Node* value = graph()->NewNode(
      common()->Phi(MachineRepresentation::kTagged, 2), vtrue, vfalse, control);
  effect = graph()->NewNode(common()->EffectPhi(2), etrue, efalse, control);

  ReplaceWithValue(node, value, effect, control);
  return Replace(value);
}

Reduction JSBuiltinReducer::ReduceMapHas(Node* node) {
  // We only optimize if we have target, receiver and key parameters.
  if (node->op()->ValueInputCount() != 3) return NoChange();
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* key = NodeProperties::GetValueInput(node, 2);

  if (!NodeProperties::HasInstanceTypeWitness(receiver, effect, JS_MAP_TYPE))
    return NoChange();

  Node* table = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForJSCollectionTable()), receiver,
      effect, control);

  Node* index = effect = graph()->NewNode(
      simplified()->FindOrderedHashMapEntry(), table, key, effect, control);

  Node* value = graph()->NewNode(simplified()->NumberEqual(), index,
                                 jsgraph()->MinusOneConstant());
  value = graph()->NewNode(simplified()->BooleanNot(), value);

  ReplaceWithValue(node, value, effect, control);
  return Replace(value);
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
  if (r.InputsMatchZero()) {
    // Number.isNaN() -> #false
    Node* value = jsgraph()->FalseConstant();
    return Replace(value);
  }
  // Number.isNaN(a:number) -> ObjectIsNaN(a)
  Node* input = r.GetJSCallInput(0);
  Node* value = graph()->NewNode(simplified()->ObjectIsNaN(), input);
  return Replace(value);
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
    // Number.parseInt(a:safe-integer) -> a
    // Number.parseInt(a:safe-integer,b:#0\/undefined) -> a
    // Number.parseInt(a:safe-integer,b:#10\/undefined) -> a
    Node* value = r.GetJSCallInput(0);
    return Replace(value);
  }
  return NoChange();
}

// ES6 section #sec-object.create Object.create(proto, properties)
Reduction JSBuiltinReducer::ReduceObjectCreate(Node* node) {
  // We need exactly target, receiver and value parameters.
  int arg_count = node->op()->ValueInputCount();
  if (arg_count != 3) return NoChange();
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* prototype = NodeProperties::GetValueInput(node, 2);
  Type* prototype_type = NodeProperties::GetType(prototype);
  if (!prototype_type->IsHeapConstant()) return NoChange();
  Handle<HeapObject> prototype_const =
      prototype_type->AsHeapConstant()->Value();
  Handle<Map> instance_map;
  MaybeHandle<Map> maybe_instance_map =
      Map::TryGetObjectCreateMap(prototype_const);
  if (!maybe_instance_map.ToHandle(&instance_map)) return NoChange();
  Node* properties = jsgraph()->EmptyFixedArrayConstant();
  if (instance_map->is_dictionary_map()) {
    // Allocated an empty NameDictionary as backing store for the properties.
    Handle<Map> map(isolate()->heap()->name_dictionary_map(), isolate());
    int capacity =
        NameDictionary::ComputeCapacity(NameDictionary::kInitialCapacity);
    DCHECK(base::bits::IsPowerOfTwo(capacity));
    int length = NameDictionary::EntryToIndex(capacity);
    int size = NameDictionary::SizeFor(length);

    AllocationBuilder a(jsgraph(), effect, control);
    a.Allocate(size, NOT_TENURED, Type::Any());
    a.Store(AccessBuilder::ForMap(), map);
    // Initialize FixedArray fields.
    a.Store(AccessBuilder::ForFixedArrayLength(),
            jsgraph()->SmiConstant(length));
    // Initialize HashTable fields.
    a.Store(AccessBuilder::ForHashTableBaseNumberOfElements(),
            jsgraph()->SmiConstant(0));
    a.Store(AccessBuilder::ForHashTableBaseNumberOfDeletedElement(),
            jsgraph()->SmiConstant(0));
    a.Store(AccessBuilder::ForHashTableBaseCapacity(),
            jsgraph()->SmiConstant(capacity));
    // Initialize Dictionary fields.
    a.Store(AccessBuilder::ForDictionaryNextEnumerationIndex(),
            jsgraph()->SmiConstant(PropertyDetails::kInitialIndex));
    a.Store(AccessBuilder::ForDictionaryObjectHashIndex(),
            jsgraph()->SmiConstant(PropertyArray::kNoHashSentinel));
    // Initialize the Properties fields.
    Node* undefined = jsgraph()->UndefinedConstant();
    STATIC_ASSERT(NameDictionary::kElementsStartIndex ==
                  NameDictionary::kObjectHashIndex + 1);
    for (int index = NameDictionary::kElementsStartIndex; index < length;
         index++) {
      a.Store(AccessBuilder::ForFixedArraySlot(index, kNoWriteBarrier),
              undefined);
    }
    properties = effect = a.Finish();
  }

  int const instance_size = instance_map->instance_size();
  if (instance_size > kMaxRegularHeapObjectSize) return NoChange();
  dependencies()->AssumeInitialMapCantChange(instance_map);

  // Emit code to allocate the JSObject instance for the given
  // {instance_map}.
  AllocationBuilder a(jsgraph(), effect, control);
  a.Allocate(instance_size, NOT_TENURED, Type::Any());
  a.Store(AccessBuilder::ForMap(), instance_map);
  a.Store(AccessBuilder::ForJSObjectPropertiesOrHash(), properties);
  a.Store(AccessBuilder::ForJSObjectElements(),
          jsgraph()->EmptyFixedArrayConstant());
  // Initialize Object fields.
  Node* undefined = jsgraph()->UndefinedConstant();
  for (int offset = JSObject::kHeaderSize; offset < instance_size;
       offset += kPointerSize) {
    a.Store(AccessBuilder::ForJSObjectOffset(offset, kNoWriteBarrier),
            undefined);
  }
  Node* value = effect = a.Finish();

  // replace it
  ReplaceWithValue(node, value, effect, control);
  return Replace(value);
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
    if ((dominator->opcode() == IrOpcode::kCheckString ||
         dominator->opcode() == IrOpcode::kCheckInternalizedString ||
         dominator->opcode() == IrOpcode::kCheckSeqString) &&
        NodeProperties::IsSame(dominator->InputAt(0), receiver)) {
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

// ES6 String.prototype.concat(...args)
// #sec-string.prototype.concat
Reduction JSBuiltinReducer::ReduceStringConcat(Node* node) {
  if (Node* receiver = GetStringWitness(node)) {
    JSCallReduction r(node);
    if (r.InputsMatchOne(Type::PlainPrimitive())) {
      // String.prototype.concat(lhs:string, rhs:plain-primitive)
      //   -> Call[StringAddStub](lhs, rhs)
      StringAddFlags flags = r.InputsMatchOne(Type::String())
                                 ? STRING_ADD_CHECK_NONE
                                 : STRING_ADD_CONVERT_RIGHT;
      // TODO(turbofan): Massage the FrameState of the {node} here once we
      // have an artificial builtin frame type, so that it looks like the
      // exception from StringAdd overflow came from String.prototype.concat
      // builtin instead of the calling function.
      Callable const callable =
          CodeFactory::StringAdd(isolate(), flags, NOT_TENURED);
      auto call_descriptor = Linkage::GetStubCallDescriptor(
          isolate(), graph()->zone(), callable.descriptor(), 0,
          CallDescriptor::kNeedsFrameState,
          Operator::kNoDeopt | Operator::kNoWrite);
      node->ReplaceInput(0, jsgraph()->HeapConstant(callable.code()));
      node->ReplaceInput(1, receiver);
      NodeProperties::ChangeOp(node, common()->Call(call_descriptor));
      return Changed(node);
    }
  }

  return NoChange();
}

// ES section #sec-string.prototype.slice
Reduction JSBuiltinReducer::ReduceStringSlice(Node* node) {
  if (Node* receiver = GetStringWitness(node)) {
    Node* start = node->op()->ValueInputCount() >= 3
                      ? NodeProperties::GetValueInput(node, 2)
                      : jsgraph()->UndefinedConstant();
    Type* start_type = NodeProperties::GetType(start);
    Node* end = node->op()->ValueInputCount() >= 4
                    ? NodeProperties::GetValueInput(node, 3)
                    : jsgraph()->UndefinedConstant();
    Type* end_type = NodeProperties::GetType(end);
    Node* effect = NodeProperties::GetEffectInput(node);
    Node* control = NodeProperties::GetControlInput(node);

    if (start_type->Is(type_cache_.kSingletonMinusOne) &&
        end_type->Is(Type::Undefined())) {
      Node* receiver_length =
          graph()->NewNode(simplified()->StringLength(), receiver);

      Node* check =
          graph()->NewNode(simplified()->NumberEqual(), receiver_length,
                           jsgraph()->ZeroConstant());
      Node* branch = graph()->NewNode(common()->Branch(BranchHint::kFalse),
                                      check, control);

      Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
      Node* vtrue = jsgraph()->EmptyStringConstant();

      Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
      Node* vfalse;
      Node* efalse;
      {
        // We need to convince TurboFan that {receiver_length}-1 is a valid
        // Unsigned32 value, so we just apply NumberToUint32 to the result
        // of the subtraction, which is a no-op and merely acts as a marker.
        Node* index =
            graph()->NewNode(simplified()->NumberSubtract(), receiver_length,
                             jsgraph()->OneConstant());
        index = graph()->NewNode(simplified()->NumberToUint32(), index);
        vfalse = efalse = graph()->NewNode(simplified()->StringCharAt(),
                                           receiver, index, effect, if_false);
      }

      control = graph()->NewNode(common()->Merge(2), if_true, if_false);
      Node* value =
          graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                           vtrue, vfalse, control);
      effect =
          graph()->NewNode(common()->EffectPhi(2), effect, efalse, control);
      ReplaceWithValue(node, value, effect, control);
      return Replace(value);
    }
  }
  return NoChange();
}

Reduction JSBuiltinReducer::ReduceArrayBufferIsView(Node* node) {
  Node* value = node->op()->ValueInputCount() >= 3
                    ? NodeProperties::GetValueInput(node, 2)
                    : jsgraph()->UndefinedConstant();
  RelaxEffectsAndControls(node);
  node->ReplaceInput(0, value);
  node->TrimInputCount(1);
  NodeProperties::ChangeOp(node, simplified()->ObjectIsArrayBufferView());
  return Changed(node);
}

Reduction JSBuiltinReducer::ReduceArrayBufferViewAccessor(
    Node* node, InstanceType instance_type, FieldAccess const& access) {
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  if (NodeProperties::HasInstanceTypeWitness(receiver, effect, instance_type)) {
    // Load the {receiver}s field.
    Node* value = effect = graph()->NewNode(simplified()->LoadField(access),
                                            receiver, effect, control);

    // See if we can skip the neutering check.
    if (isolate()->IsArrayBufferNeuteringIntact()) {
      // Add a code dependency so we are deoptimized in case an ArrayBuffer
      // gets neutered.
      dependencies()->AssumePropertyCell(
          factory()->array_buffer_neutering_protector());
    } else {
      // Check if the {receiver}s buffer was neutered.
      Node* receiver_buffer = effect = graph()->NewNode(
          simplified()->LoadField(AccessBuilder::ForJSArrayBufferViewBuffer()),
          receiver, effect, control);
      Node* check = effect =
          graph()->NewNode(simplified()->ArrayBufferWasNeutered(),
                           receiver_buffer, effect, control);

      // Default to zero if the {receiver}s buffer was neutered.
      value = graph()->NewNode(
          common()->Select(MachineRepresentation::kTagged, BranchHint::kFalse),
          check, jsgraph()->ZeroConstant(), value);
    }

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
  if (!r.BuiltinCanBeInlined()) return NoChange();
  switch (r.GetBuiltinFunctionId()) {
    case kArrayEntries:
      return ReduceArrayIterator(node, IterationKind::kEntries);
    case kArrayKeys:
      return ReduceArrayIterator(node, IterationKind::kKeys);
    case kArrayValues:
      return ReduceArrayIterator(node, IterationKind::kValues);
    case kArrayIteratorNext:
      return ReduceArrayIteratorNext(node);
    case kArrayIsArray:
      return ReduceArrayIsArray(node);
    case kDateNow:
      return ReduceDateNow(node);
    case kDateGetTime:
      return ReduceDateGetTime(node);
    case kGlobalIsFinite:
      reduction = ReduceGlobalIsFinite(node);
      break;
    case kGlobalIsNaN:
      reduction = ReduceGlobalIsNaN(node);
      break;
    case kMapEntries:
      return ReduceCollectionIterator(
          node, JS_MAP_TYPE, Context::MAP_KEY_VALUE_ITERATOR_MAP_INDEX);
    case kMapGet:
      reduction = ReduceMapGet(node);
      break;
    case kMapHas:
      reduction = ReduceMapHas(node);
      break;
    case kMapKeys:
      return ReduceCollectionIterator(node, JS_MAP_TYPE,
                                      Context::MAP_KEY_ITERATOR_MAP_INDEX);
    case kMapSize:
      return ReduceCollectionSize(node, JS_MAP_TYPE);
    case kMapValues:
      return ReduceCollectionIterator(node, JS_MAP_TYPE,
                                      Context::MAP_VALUE_ITERATOR_MAP_INDEX);
    case kMapIteratorNext:
      return ReduceCollectionIteratorNext(
          node, OrderedHashMap::kEntrySize, factory()->empty_ordered_hash_map(),
          FIRST_MAP_ITERATOR_TYPE, LAST_MAP_ITERATOR_TYPE);
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
    case kObjectCreate:
      reduction = ReduceObjectCreate(node);
      break;
    case kSetEntries:
      return ReduceCollectionIterator(
          node, JS_SET_TYPE, Context::SET_KEY_VALUE_ITERATOR_MAP_INDEX);
    case kSetSize:
      return ReduceCollectionSize(node, JS_SET_TYPE);
    case kSetValues:
      return ReduceCollectionIterator(node, JS_SET_TYPE,
                                      Context::SET_VALUE_ITERATOR_MAP_INDEX);
    case kSetIteratorNext:
      return ReduceCollectionIteratorNext(
          node, OrderedHashSet::kEntrySize, factory()->empty_ordered_hash_set(),
          FIRST_SET_ITERATOR_TYPE, LAST_SET_ITERATOR_TYPE);
    case kStringConcat:
      return ReduceStringConcat(node);
    case kStringSlice:
      return ReduceStringSlice(node);
    case kArrayBufferIsView:
      return ReduceArrayBufferIsView(node);
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
    case kTypedArrayEntries:
      return ReduceTypedArrayIterator(node, IterationKind::kEntries);
    case kTypedArrayKeys:
      return ReduceTypedArrayIterator(node, IterationKind::kKeys);
    case kTypedArrayValues:
      return ReduceTypedArrayIterator(node, IterationKind::kValues);
    case kTypedArrayToStringTag:
      return ReduceTypedArrayToStringTag(node);
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
