// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-builtin-reducer.h"

#include "src/base/bits.h"
#include "src/builtins/builtins-utils.h"
#include "src/code-factory.h"
#include "src/compilation-dependencies.h"
#include "src/compiler/access-builder.h"
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
                                   Flags flags,
                                   CompilationDependencies* dependencies,
                                   Handle<Context> native_context)
    : AdvancedReducer(editor),
      dependencies_(dependencies),
      flags_(flags),
      jsgraph_(jsgraph),
      native_context_(native_context),
      type_cache_(TypeCache::Get()) {}

namespace {

MaybeHandle<Map> GetMapWitness(Node* node) {
  ZoneHandleSet<Map> maps;
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);
  NodeProperties::InferReceiverMapsResult result =
      NodeProperties::InferReceiverMaps(receiver, effect, &maps);
  if (result == NodeProperties::kReliableReceiverMaps && maps.size() == 1) {
    return maps[0];
  }
  return MaybeHandle<Map>();
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

bool CanInlineJSArrayIteration(Handle<Map> receiver_map) {
  Isolate* const isolate = receiver_map->GetIsolate();
  // Ensure that the [[Prototype]] is actually an exotic Array
  if (!receiver_map->prototype()->IsJSArray()) return false;

  // Don't inline JSArrays with slow elements of any kind
  if (!IsFastElementsKind(receiver_map->elements_kind())) return false;

  // If the receiver map has packed elements, no need to check the prototype.
  // This requires a MapCheck where this is used.
  if (!IsFastHoleyElementsKind(receiver_map->elements_kind())) return true;

  Handle<JSArray> receiver_prototype(JSArray::cast(receiver_map->prototype()),
                                     isolate);
  // Ensure all prototypes of the {receiver} are stable.
  for (PrototypeIterator it(isolate, receiver_prototype, kStartAtReceiver);
       !it.IsAtEnd(); it.Advance()) {
    Handle<JSReceiver> current = PrototypeIterator::GetCurrent<JSReceiver>(it);
    if (!current->map()->is_stable()) return false;
  }

  // For holey Arrays, ensure that the array_protector cell is valid (must be
  // a CompilationDependency), and the JSArray prototype has not been altered.
  return receiver_map->instance_type() == JS_ARRAY_TYPE &&
         (!receiver_map->is_dictionary_map() || receiver_map->is_stable()) &&
         isolate->IsFastArrayConstructorPrototypeChainIntact() &&
         isolate->IsAnyInitialArrayPrototype(receiver_prototype);
}

}  // namespace

Reduction JSBuiltinReducer::ReduceArrayIterator(Node* node,
                                                IterationKind kind) {
  Handle<Map> receiver_map;
  if (GetMapWitness(node).ToHandle(&receiver_map)) {
    return ReduceArrayIterator(receiver_map, node, kind,
                               ArrayIteratorKind::kArray);
  }
  return NoChange();
}

Reduction JSBuiltinReducer::ReduceTypedArrayIterator(Node* node,
                                                     IterationKind kind) {
  Handle<Map> receiver_map;
  if (GetMapWitness(node).ToHandle(&receiver_map) &&
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
      effect =
          graph()->NewNode(simplified()->CheckIf(), check, effect, control);
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
          if (IsFastHoleyElementsKind(receiver_map->elements_kind())) {
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
        DCHECK_LE(receiver_map->elements_kind(), UINT8_CLAMPED_ELEMENTS);
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

  // allocate new iterator
  effect = graph()->NewNode(
      common()->BeginRegion(RegionObservability::kNotObservable), effect);
  Node* value = effect = graph()->NewNode(
      simplified()->Allocate(Type::OtherObject(), NOT_TENURED),
      jsgraph()->Constant(JSArrayIterator::kSize), effect, control);
  effect = graph()->NewNode(simplified()->StoreField(AccessBuilder::ForMap()),
                            value, jsgraph()->Constant(map), effect, control);
  effect = graph()->NewNode(
      simplified()->StoreField(AccessBuilder::ForJSObjectProperties()), value,
      jsgraph()->EmptyFixedArrayConstant(), effect, control);
  effect = graph()->NewNode(
      simplified()->StoreField(AccessBuilder::ForJSObjectElements()), value,
      jsgraph()->EmptyFixedArrayConstant(), effect, control);

  // attach the iterator to this object
  effect = graph()->NewNode(
      simplified()->StoreField(AccessBuilder::ForJSArrayIteratorObject()),
      value, receiver, effect, control);
  effect = graph()->NewNode(
      simplified()->StoreField(AccessBuilder::ForJSArrayIteratorIndex()), value,
      jsgraph()->ZeroConstant(), effect, control);
  effect = graph()->NewNode(
      simplified()->StoreField(AccessBuilder::ForJSArrayIteratorObjectMap()),
      value, object_map, effect, control);

  value = effect = graph()->NewNode(common()->FinishRegion(), value, effect);

  // replace it
  ReplaceWithValue(node, value, effect, control);
  return Replace(value);
}

Reduction JSBuiltinReducer::ReduceFastArrayIteratorNext(
    Handle<Map> iterator_map, Node* node, IterationKind kind) {
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

  ElementsKind elements_kind = JSArrayIterator::ElementsKindForInstanceType(
      iterator_map->instance_type());

  if (IsFastHoleyElementsKind(elements_kind)) {
    if (!isolate()->IsFastArrayConstructorPrototypeChainIntact()) {
      return NoChange();
    } else {
      Handle<JSObject> initial_array_prototype(
          native_context()->initial_array_prototype(), isolate());
      dependencies()->AssumePropertyCell(factory()->array_protector());
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
        etrue1 = graph()->NewNode(simplified()->CheckIf(), check_map, etrue1,
                                  if_true1);
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
        if (elements_kind == FAST_HOLEY_ELEMENTS ||
            elements_kind == FAST_HOLEY_SMI_ELEMENTS) {
          value = graph()->NewNode(simplified()->ConvertTaggedHoleToUndefined(),
                                   value);
        } else if (elements_kind == FAST_HOLEY_DOUBLE_ELEMENTS) {
          // TODO(bmeurer): avoid deopt if not all uses of value are truncated.
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

Reduction JSBuiltinReducer::ReduceTypedArrayIteratorNext(
    Handle<Map> iterator_map, Node* node, IterationKind kind) {
  Node* iterator = NodeProperties::GetValueInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* context = NodeProperties::GetContextInput(node);

  ElementsKind elements_kind = JSArrayIterator::ElementsKindForInstanceType(
      iterator_map->instance_type());

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
      efalse0 =
          graph()->NewNode(simplified()->CheckIf(), check1, efalse0, if_false0);
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
          DCHECK(kind == IterationKind::kValues);
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

Reduction JSBuiltinReducer::ReduceArrayIteratorNext(Node* node) {
  Handle<Map> receiver_map;
  if (GetMapWitness(node).ToHandle(&receiver_map)) {
    switch (receiver_map->instance_type()) {
      case JS_TYPED_ARRAY_KEY_ITERATOR_TYPE:
        return ReduceTypedArrayIteratorNext(receiver_map, node,
                                            IterationKind::kKeys);

      case JS_FAST_ARRAY_KEY_ITERATOR_TYPE:
        return ReduceFastArrayIteratorNext(receiver_map, node,
                                           IterationKind::kKeys);

      case JS_INT8_ARRAY_KEY_VALUE_ITERATOR_TYPE:
      case JS_UINT8_ARRAY_KEY_VALUE_ITERATOR_TYPE:
      case JS_INT16_ARRAY_KEY_VALUE_ITERATOR_TYPE:
      case JS_UINT16_ARRAY_KEY_VALUE_ITERATOR_TYPE:
      case JS_INT32_ARRAY_KEY_VALUE_ITERATOR_TYPE:
      case JS_UINT32_ARRAY_KEY_VALUE_ITERATOR_TYPE:
      case JS_FLOAT32_ARRAY_KEY_VALUE_ITERATOR_TYPE:
      case JS_FLOAT64_ARRAY_KEY_VALUE_ITERATOR_TYPE:
      case JS_UINT8_CLAMPED_ARRAY_KEY_VALUE_ITERATOR_TYPE:
        return ReduceTypedArrayIteratorNext(receiver_map, node,
                                            IterationKind::kEntries);

      case JS_FAST_SMI_ARRAY_KEY_VALUE_ITERATOR_TYPE:
      case JS_FAST_HOLEY_SMI_ARRAY_KEY_VALUE_ITERATOR_TYPE:
      case JS_FAST_ARRAY_KEY_VALUE_ITERATOR_TYPE:
      case JS_FAST_HOLEY_ARRAY_KEY_VALUE_ITERATOR_TYPE:
      case JS_FAST_DOUBLE_ARRAY_KEY_VALUE_ITERATOR_TYPE:
      case JS_FAST_HOLEY_DOUBLE_ARRAY_KEY_VALUE_ITERATOR_TYPE:
        return ReduceFastArrayIteratorNext(receiver_map, node,
                                           IterationKind::kEntries);

      case JS_INT8_ARRAY_VALUE_ITERATOR_TYPE:
      case JS_UINT8_ARRAY_VALUE_ITERATOR_TYPE:
      case JS_INT16_ARRAY_VALUE_ITERATOR_TYPE:
      case JS_UINT16_ARRAY_VALUE_ITERATOR_TYPE:
      case JS_INT32_ARRAY_VALUE_ITERATOR_TYPE:
      case JS_UINT32_ARRAY_VALUE_ITERATOR_TYPE:
      case JS_FLOAT32_ARRAY_VALUE_ITERATOR_TYPE:
      case JS_FLOAT64_ARRAY_VALUE_ITERATOR_TYPE:
      case JS_UINT8_CLAMPED_ARRAY_VALUE_ITERATOR_TYPE:
        return ReduceTypedArrayIteratorNext(receiver_map, node,
                                            IterationKind::kValues);

      case JS_FAST_SMI_ARRAY_VALUE_ITERATOR_TYPE:
      case JS_FAST_HOLEY_SMI_ARRAY_VALUE_ITERATOR_TYPE:
      case JS_FAST_ARRAY_VALUE_ITERATOR_TYPE:
      case JS_FAST_HOLEY_ARRAY_VALUE_ITERATOR_TYPE:
      case JS_FAST_DOUBLE_ARRAY_VALUE_ITERATOR_TYPE:
      case JS_FAST_HOLEY_DOUBLE_ARRAY_VALUE_ITERATOR_TYPE:
        return ReduceFastArrayIteratorNext(receiver_map, node,
                                           IterationKind::kValues);

      default:
        // Slow array iterators are not reduced
        return NoChange();
    }
  }
  return NoChange();
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

// ES6 section 22.1.3.17 Array.prototype.pop ( )
Reduction JSBuiltinReducer::ReduceArrayPop(Node* node) {
  Handle<Map> receiver_map;
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  // TODO(turbofan): Extend this to also handle fast holey double elements
  // once we got the hole NaN mess sorted out in TurboFan/V8.
  if (GetMapWitness(node).ToHandle(&receiver_map) &&
      CanInlineArrayResizeOperation(receiver_map) &&
      receiver_map->elements_kind() != FAST_HOLEY_DOUBLE_ELEMENTS) {
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
      // TODO(tebbi): We should trim the backing store if the capacity is too
      // big, as implemented in elements.cc:ElementsAccessorBase::SetLengthImpl.

      // Load the elements backing store from the {receiver}.
      Node* elements = efalse = graph()->NewNode(
          simplified()->LoadField(AccessBuilder::ForJSObjectElements()),
          receiver, efalse, if_false);

      // Ensure that we aren't popping from a copy-on-write backing store.
      if (IsFastSmiOrObjectElementsKind(receiver_map->elements_kind())) {
        elements = efalse =
            graph()->NewNode(simplified()->EnsureWritableFastElements(),
                             receiver, elements, efalse, if_false);
      }

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
  // We need exactly target, receiver and value parameters.
  if (node->op()->ValueInputCount() != 3) return NoChange();
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* value = NodeProperties::GetValueInput(node, 2);
  ZoneHandleSet<Map> receiver_maps;
  NodeProperties::InferReceiverMapsResult result =
      NodeProperties::InferReceiverMaps(receiver, effect, &receiver_maps);
  if (receiver_maps.size() != 1) return NoChange();
  DCHECK_NE(NodeProperties::kNoReceiverMaps, result);

  // TODO(turbofan): Relax this to deal with multiple {receiver} maps.
  Handle<Map> receiver_map = receiver_maps[0];
  if (CanInlineArrayResizeOperation(receiver_map)) {
    // Install code dependencies on the {receiver} prototype maps and the
    // global array protector cell.
    dependencies()->AssumePropertyCell(factory()->array_protector());
    dependencies()->AssumePrototypeMapsStable(receiver_map);

    // If the {receiver_maps} information is not reliable, we need
    // to check that the {receiver} still has one of these maps.
    if (result == NodeProperties::kUnreliableReceiverMaps) {
      if (receiver_map->is_stable()) {
        dependencies()->AssumeMapStable(receiver_map);
      } else {
        // TODO(turbofan): This is a potential - yet unlikely - deoptimization
        // loop, since we might not learn from this deoptimization in baseline
        // code. We need a way to learn from deoptimizations in optimized to
        // address these problems.
        effect = graph()->NewNode(
            simplified()->CheckMaps(CheckMapsFlag::kNone, receiver_maps),
            receiver, effect, control);
      }
    }

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

// ES6 section 22.1.3.22 Array.prototype.shift ( )
Reduction JSBuiltinReducer::ReduceArrayShift(Node* node) {
  Node* target = NodeProperties::GetValueInput(node, 0);
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* context = NodeProperties::GetContextInput(node);
  Node* frame_state = NodeProperties::GetFrameStateInput(node);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // TODO(turbofan): Extend this to also handle fast holey double elements
  // once we got the hole NaN mess sorted out in TurboFan/V8.
  Handle<Map> receiver_map;
  if (GetMapWitness(node).ToHandle(&receiver_map) &&
      CanInlineArrayResizeOperation(receiver_map) &&
      receiver_map->elements_kind() != FAST_HOLEY_DOUBLE_ELEMENTS) {
    // Install code dependencies on the {receiver} prototype maps and the
    // global array protector cell.
    dependencies()->AssumePropertyCell(factory()->array_protector());
    dependencies()->AssumePrototypeMapsStable(receiver_map);

    // Load length of the {receiver}.
    Node* length = effect = graph()->NewNode(
        simplified()->LoadField(
            AccessBuilder::ForJSArrayLength(receiver_map->elements_kind())),
        receiver, effect, control);

    // Return undefined if {receiver} has no elements.
    Node* check0 = graph()->NewNode(simplified()->NumberEqual(), length,
                                    jsgraph()->ZeroConstant());
    Node* branch0 =
        graph()->NewNode(common()->Branch(BranchHint::kFalse), check0, control);

    Node* if_true0 = graph()->NewNode(common()->IfTrue(), branch0);
    Node* etrue0 = effect;
    Node* vtrue0 = jsgraph()->UndefinedConstant();

    Node* if_false0 = graph()->NewNode(common()->IfFalse(), branch0);
    Node* efalse0 = effect;
    Node* vfalse0;
    {
      // Check if we should take the fast-path.
      Node* check1 =
          graph()->NewNode(simplified()->NumberLessThanOrEqual(), length,
                           jsgraph()->Constant(JSArray::kMaxCopyElements));
      Node* branch1 = graph()->NewNode(common()->Branch(BranchHint::kTrue),
                                       check1, if_false0);

      Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
      Node* etrue1 = efalse0;
      Node* vtrue1;
      {
        Node* elements = etrue1 = graph()->NewNode(
            simplified()->LoadField(AccessBuilder::ForJSObjectElements()),
            receiver, etrue1, if_true1);

        // Load the first element here, which we return below.
        vtrue1 = etrue1 = graph()->NewNode(
            simplified()->LoadElement(AccessBuilder::ForFixedArrayElement(
                receiver_map->elements_kind())),
            elements, jsgraph()->ZeroConstant(), etrue1, if_true1);

        // Ensure that we aren't shifting a copy-on-write backing store.
        if (IsFastSmiOrObjectElementsKind(receiver_map->elements_kind())) {
          elements = etrue1 =
              graph()->NewNode(simplified()->EnsureWritableFastElements(),
                               receiver, elements, etrue1, if_true1);
        }

        // Shift the remaining {elements} by one towards the start.
        Node* loop = graph()->NewNode(common()->Loop(2), if_true1, if_true1);
        Node* eloop =
            graph()->NewNode(common()->EffectPhi(2), etrue1, etrue1, loop);
        Node* index = graph()->NewNode(
            common()->Phi(MachineRepresentation::kTagged, 2),
            jsgraph()->OneConstant(),
            jsgraph()->Constant(JSArray::kMaxCopyElements - 1), loop);

        {
          Node* check2 =
              graph()->NewNode(simplified()->NumberLessThan(), index, length);
          Node* branch2 = graph()->NewNode(common()->Branch(), check2, loop);

          if_true1 = graph()->NewNode(common()->IfFalse(), branch2);
          etrue1 = eloop;

          Node* control = graph()->NewNode(common()->IfTrue(), branch2);
          Node* effect = etrue1;

          ElementAccess const access = AccessBuilder::ForFixedArrayElement(
              receiver_map->elements_kind());
          Node* value = effect =
              graph()->NewNode(simplified()->LoadElement(access), elements,
                               index, effect, control);
          effect = graph()->NewNode(
              simplified()->StoreElement(access), elements,
              graph()->NewNode(simplified()->NumberSubtract(), index,
                               jsgraph()->OneConstant()),
              value, effect, control);

          loop->ReplaceInput(1, control);
          eloop->ReplaceInput(1, effect);
          index->ReplaceInput(1,
                              graph()->NewNode(simplified()->NumberAdd(), index,
                                               jsgraph()->OneConstant()));
        }

        // Compute the new {length}.
        length = graph()->NewNode(simplified()->NumberSubtract(), length,
                                  jsgraph()->OneConstant());

        // Store the new {length} to the {receiver}.
        etrue1 = graph()->NewNode(
            simplified()->StoreField(
                AccessBuilder::ForJSArrayLength(receiver_map->elements_kind())),
            receiver, length, etrue1, if_true1);

        // Store a hole to the element we just removed from the {receiver}.
        etrue1 = graph()->NewNode(
            simplified()->StoreElement(AccessBuilder::ForFixedArrayElement(
                GetHoleyElementsKind(receiver_map->elements_kind()))),
            elements, length, jsgraph()->TheHoleConstant(), etrue1, if_true1);
      }

      Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch1);
      Node* efalse1 = efalse0;
      Node* vfalse1;
      {
        // Call the generic C++ implementation.
        const int builtin_index = Builtins::kArrayShift;
        CallDescriptor const* const desc = Linkage::GetCEntryStubCallDescriptor(
            graph()->zone(), 1, BuiltinArguments::kNumExtraArgsWithReceiver,
            Builtins::name(builtin_index), node->op()->properties(),
            CallDescriptor::kNeedsFrameState);
        Node* stub_code = jsgraph()->CEntryStubConstant(1, kDontSaveFPRegs,
                                                        kArgvOnStack, true);
        Address builtin_entry = Builtins::CppEntryOf(builtin_index);
        Node* entry = jsgraph()->ExternalConstant(
            ExternalReference(builtin_entry, isolate()));
        Node* argc =
            jsgraph()->Constant(BuiltinArguments::kNumExtraArgsWithReceiver);
        if_false1 = efalse1 = vfalse1 =
            graph()->NewNode(common()->Call(desc), stub_code, receiver, argc,
                             target, jsgraph()->UndefinedConstant(), entry,
                             argc, context, frame_state, efalse1, if_false1);
      }

      if_false0 = graph()->NewNode(common()->Merge(2), if_true1, if_false1);
      efalse0 =
          graph()->NewNode(common()->EffectPhi(2), etrue1, efalse1, if_false0);
      vfalse0 =
          graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                           vtrue1, vfalse1, if_false0);
    }

    control = graph()->NewNode(common()->Merge(2), if_true0, if_false0);
    effect = graph()->NewNode(common()->EffectPhi(2), etrue0, efalse0, control);
    Node* value =
        graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                         vtrue0, vfalse0, control);

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

namespace {

bool HasInstanceTypeWitness(Node* receiver, Node* effect,
                            InstanceType instance_type) {
  for (Node* dominator = effect;;) {
    if (dominator->opcode() == IrOpcode::kCheckMaps &&
        NodeProperties::IsSame(dominator->InputAt(0), receiver)) {
      ZoneHandleSet<Map> const& maps =
          CheckMapsParametersOf(dominator->op()).maps();
      // Check if all maps have the given {instance_type}.
      for (size_t i = 0; i < maps.size(); ++i) {
        if (maps[i]->instance_type() != instance_type) return false;
      }
      return true;
    }
    // The instance type doesn't change for JSReceiver values, so we
    // don't need to pay attention to potentially side-effecting nodes
    // here. Strings and internal structures like FixedArray and
    // FixedDoubleArray are weird here, but we don't use this function then.
    DCHECK_LE(FIRST_JS_RECEIVER_TYPE, instance_type);
    DCHECK_EQ(1, dominator->op()->EffectOutputCount());
    if (dominator->op()->EffectInputCount() != 1) {
      // Didn't find any appropriate CheckMaps node.
      return false;
    }
    dominator = NodeProperties::GetEffectInput(dominator);
  }
}

}  // namespace

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
  if (HasInstanceTypeWitness(receiver, effect, JS_DATE_TYPE)) {
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
  Handle<Map> instance_map;
  if (!prototype_type->IsHeapConstant()) return NoChange();
  Handle<HeapObject> prototype_const =
      prototype_type->AsHeapConstant()->Value();
  if (!prototype_const->IsNull(isolate()) && !prototype_const->IsJSReceiver()) {
    return NoChange();
  }
  instance_map = Map::GetObjectCreateMap(prototype_const);
  Node* properties = jsgraph()->EmptyFixedArrayConstant();
  if (instance_map->is_dictionary_map()) {
    // Allocated an empty NameDictionary as backing store for the properties.
    Handle<Map> map(isolate()->heap()->hash_table_map(), isolate());
    int capacity =
        NameDictionary::ComputeCapacity(NameDictionary::kInitialCapacity);
    DCHECK(base::bits::IsPowerOfTwo32(capacity));
    int length = NameDictionary::EntryToIndex(capacity);
    int size = NameDictionary::SizeFor(length);

    effect = graph()->NewNode(
        common()->BeginRegion(RegionObservability::kNotObservable), effect);

    Node* value = effect =
        graph()->NewNode(simplified()->Allocate(Type::Any(), NOT_TENURED),
                         jsgraph()->Constant(size), effect, control);
    effect =
        graph()->NewNode(simplified()->StoreField(AccessBuilder::ForMap()),
                         value, jsgraph()->HeapConstant(map), effect, control);

    // Initialize FixedArray fields.
    effect = graph()->NewNode(
        simplified()->StoreField(AccessBuilder::ForFixedArrayLength()), value,
        jsgraph()->SmiConstant(length), effect, control);
    // Initialize HashTable fields.
    effect =
        graph()->NewNode(simplified()->StoreField(
                             AccessBuilder::ForHashTableBaseNumberOfElements()),
                         value, jsgraph()->SmiConstant(0), effect, control);
    effect = graph()->NewNode(
        simplified()->StoreField(
            AccessBuilder::ForHashTableBaseNumberOfDeletedElement()),
        value, jsgraph()->SmiConstant(0), effect, control);
    effect = graph()->NewNode(
        simplified()->StoreField(AccessBuilder::ForHashTableBaseCapacity()),
        value, jsgraph()->SmiConstant(capacity), effect, control);
    // Initialize Dictionary fields.
    Node* undefined = jsgraph()->UndefinedConstant();
    effect = graph()->NewNode(
        simplified()->StoreField(AccessBuilder::ForDictionaryMaxNumberKey()),
        value, undefined, effect, control);
    effect = graph()->NewNode(
        simplified()->StoreField(
            AccessBuilder::ForDictionaryNextEnumerationIndex()),
        value, jsgraph()->SmiConstant(PropertyDetails::kInitialIndex), effect,
        control);
    // Initialize hte Properties fields.
    for (int index = NameDictionary::kNextEnumerationIndexIndex + 1;
         index < length; index++) {
      effect = graph()->NewNode(
          simplified()->StoreField(
              AccessBuilder::ForFixedArraySlot(index, kNoWriteBarrier)),
          value, undefined, effect, control);
    }
    properties = effect =
        graph()->NewNode(common()->FinishRegion(), value, effect);
  }

  int const instance_size = instance_map->instance_size();
  if (instance_size > kMaxRegularHeapObjectSize) return NoChange();
  dependencies()->AssumeInitialMapCantChange(instance_map);

  // Emit code to allocate the JSObject instance for the given
  // {instance_map}.
  effect = graph()->NewNode(
      common()->BeginRegion(RegionObservability::kNotObservable), effect);
  Node* value = effect =
      graph()->NewNode(simplified()->Allocate(Type::Any(), NOT_TENURED),
                       jsgraph()->Constant(instance_size), effect, control);
  effect =
      graph()->NewNode(simplified()->StoreField(AccessBuilder::ForMap()), value,
                       jsgraph()->HeapConstant(instance_map), effect, control);
  effect = graph()->NewNode(
      simplified()->StoreField(AccessBuilder::ForJSObjectProperties()), value,
      properties, effect, control);
  effect = graph()->NewNode(
      simplified()->StoreField(AccessBuilder::ForJSObjectElements()), value,
      jsgraph()->EmptyFixedArrayConstant(), effect, control);
  // Initialize Object fields.
  Node* undefined = jsgraph()->UndefinedConstant();
  for (int offset = JSObject::kHeaderSize; offset < instance_size;
       offset += kPointerSize) {
    effect = graph()->NewNode(
        simplified()->StoreField(
            AccessBuilder::ForJSObjectOffset(offset, kNoWriteBarrier)),
        value, undefined, effect, control);
  }
  value = effect = graph()->NewNode(common()->FinishRegion(), value, effect);

  // replace it
  ReplaceWithValue(node, value, effect, control);
  return Replace(value);
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

// ES6 section 21.1.3.1 String.prototype.charAt ( pos )
Reduction JSBuiltinReducer::ReduceStringCharAt(Node* node) {
  // We need at least target, receiver and index parameters.
  if (node->op()->ValueInputCount() >= 3) {
    Node* index = NodeProperties::GetValueInput(node, 2);
    Type* index_type = NodeProperties::GetType(index);
    Node* effect = NodeProperties::GetEffectInput(node);
    Node* control = NodeProperties::GetControlInput(node);

    if (index_type->Is(Type::Integral32OrMinusZeroOrNaN())) {
      if (Node* receiver = GetStringWitness(node)) {
        if (!index_type->Is(Type::Unsigned32())) {
          // Map -0 and NaN to 0 (as per ToInteger), and the values in
          // the [-2^31,-1] range to the [2^31,2^32-1] range, which will
          // be considered out-of-bounds as well, because of the maximal
          // String length limit in V8.
          STATIC_ASSERT(String::kMaxLength <= kMaxInt);
          index = graph()->NewNode(simplified()->NumberToUint32(), index);
        }

        // Determine the {receiver} length.
        Node* receiver_length = effect = graph()->NewNode(
            simplified()->LoadField(AccessBuilder::ForStringLength()), receiver,
            effect, control);

        // Check if {index} is less than {receiver} length.
        Node* check = graph()->NewNode(simplified()->NumberLessThan(), index,
                                       receiver_length);
        Node* branch = graph()->NewNode(common()->Branch(BranchHint::kTrue),
                                        check, control);

        // Return the character from the {receiver} as single character string.
        Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
        Node* vtrue = graph()->NewNode(simplified()->StringCharAt(), receiver,
                                       index, if_true);

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

    if (index_type->Is(Type::Integral32OrMinusZeroOrNaN())) {
      if (Node* receiver = GetStringWitness(node)) {
        if (!index_type->Is(Type::Unsigned32())) {
          // Map -0 and NaN to 0 (as per ToInteger), and the values in
          // the [-2^31,-1] range to the [2^31,2^32-1] range, which will
          // be considered out-of-bounds as well, because of the maximal
          // String length limit in V8.
          STATIC_ASSERT(String::kMaxLength <= kMaxInt);
          index = graph()->NewNode(simplified()->NumberToUint32(), index);
        }

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
      CallDescriptor const* const desc = Linkage::GetStubCallDescriptor(
          isolate(), graph()->zone(), callable.descriptor(), 0,
          CallDescriptor::kNeedsFrameState,
          Operator::kNoDeopt | Operator::kNoWrite);
      node->ReplaceInput(0, jsgraph()->HeapConstant(callable.code()));
      node->ReplaceInput(1, receiver);
      NodeProperties::ChangeOp(node, common()->Call(desc));
      return Changed(node);
    }
  }

  return NoChange();
}

// ES6 String.prototype.indexOf(searchString [, position])
// #sec-string.prototype.indexof
Reduction JSBuiltinReducer::ReduceStringIndexOf(Node* node) {
  // We need at least target, receiver and search_string parameters.
  if (node->op()->ValueInputCount() >= 3) {
    Node* search_string = NodeProperties::GetValueInput(node, 2);
    Type* search_string_type = NodeProperties::GetType(search_string);
    Node* position = (node->op()->ValueInputCount() >= 4)
                         ? NodeProperties::GetValueInput(node, 3)
                         : jsgraph()->ZeroConstant();
    Type* position_type = NodeProperties::GetType(position);

    if (search_string_type->Is(Type::String()) &&
        position_type->Is(Type::SignedSmall())) {
      if (Node* receiver = GetStringWitness(node)) {
        RelaxEffectsAndControls(node);
        node->ReplaceInput(0, receiver);
        node->ReplaceInput(1, search_string);
        node->ReplaceInput(2, position);
        node->TrimInputCount(3);
        NodeProperties::ChangeOp(node, simplified()->StringIndexOf());
        return Changed(node);
      }
    }
  }
  return NoChange();
}

Reduction JSBuiltinReducer::ReduceStringIterator(Node* node) {
  if (Node* receiver = GetStringWitness(node)) {
    Node* effect = NodeProperties::GetEffectInput(node);
    Node* control = NodeProperties::GetControlInput(node);

    Node* map = jsgraph()->HeapConstant(
        handle(native_context()->string_iterator_map(), isolate()));

    // allocate new iterator
    effect = graph()->NewNode(
        common()->BeginRegion(RegionObservability::kNotObservable), effect);
    Node* value = effect = graph()->NewNode(
        simplified()->Allocate(Type::OtherObject(), NOT_TENURED),
        jsgraph()->Constant(JSStringIterator::kSize), effect, control);
    effect = graph()->NewNode(simplified()->StoreField(AccessBuilder::ForMap()),
                              value, map, effect, control);
    effect = graph()->NewNode(
        simplified()->StoreField(AccessBuilder::ForJSObjectProperties()), value,
        jsgraph()->EmptyFixedArrayConstant(), effect, control);
    effect = graph()->NewNode(
        simplified()->StoreField(AccessBuilder::ForJSObjectElements()), value,
        jsgraph()->EmptyFixedArrayConstant(), effect, control);

    // attach the iterator to this string
    effect = graph()->NewNode(
        simplified()->StoreField(AccessBuilder::ForJSStringIteratorString()),
        value, receiver, effect, control);
    effect = graph()->NewNode(
        simplified()->StoreField(AccessBuilder::ForJSStringIteratorIndex()),
        value, jsgraph()->SmiConstant(0), effect, control);

    value = effect = graph()->NewNode(common()->FinishRegion(), value, effect);

    // replace it
    ReplaceWithValue(node, value, effect, control);
    return Replace(value);
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
      Node* check1 =
          graph()->NewNode(simplified()->NumberEqual(),
                           graph()->NewNode(simplified()->NumberBitwiseAnd(),
                                            lead, jsgraph()->Constant(0xFC00)),
                           jsgraph()->Constant(0xD800));
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
                               jsgraph()->Constant(0xFC00)),
              jsgraph()->Constant(0xDC00));
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
                                 jsgraph()->Constant(16)),
                trail);
#else
                graph()->NewNode(simplified()->NumberShiftLeft(), trail,
                                 jsgraph()->Constant(16)),
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
    case kArrayPop:
      return ReduceArrayPop(node);
    case kArrayPush:
      return ReduceArrayPush(node);
    case kArrayShift:
      return ReduceArrayShift(node);
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
    case kObjectCreate:
      reduction = ReduceObjectCreate(node);
      break;
    case kStringFromCharCode:
      reduction = ReduceStringFromCharCode(node);
      break;
    case kStringCharAt:
      return ReduceStringCharAt(node);
    case kStringCharCodeAt:
      return ReduceStringCharCodeAt(node);
    case kStringConcat:
      return ReduceStringConcat(node);
    case kStringIndexOf:
      return ReduceStringIndexOf(node);
    case kStringIterator:
      return ReduceStringIterator(node);
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
    case kTypedArrayEntries:
      return ReduceTypedArrayIterator(node, IterationKind::kEntries);
    case kTypedArrayKeys:
      return ReduceTypedArrayIterator(node, IterationKind::kKeys);
    case kTypedArrayValues:
      return ReduceTypedArrayIterator(node, IterationKind::kValues);
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
