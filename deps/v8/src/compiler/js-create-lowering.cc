// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-create-lowering.h"

#include "src/allocation-site-scopes.h"
#include "src/code-factory.h"
#include "src/compilation-dependencies.h"
#include "src/compiler/access-builder.h"
#include "src/compiler/allocation-builder.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/js-operator.h"
#include "src/compiler/linkage.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/node.h"
#include "src/compiler/operator-properties.h"
#include "src/compiler/simplified-operator.h"
#include "src/compiler/state-values-utils.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {

// Retrieves the frame state holding actual argument values.
Node* GetArgumentsFrameState(Node* frame_state) {
  Node* const outer_state = NodeProperties::GetFrameStateInput(frame_state);
  FrameStateInfo outer_state_info = FrameStateInfoOf(outer_state->op());
  return outer_state_info.type() == FrameStateType::kArgumentsAdaptor
             ? outer_state
             : frame_state;
}

// Checks whether allocation using the given target and new.target can be
// inlined.
bool IsAllocationInlineable(Handle<JSFunction> target,
                            Handle<JSFunction> new_target) {
  return new_target->has_initial_map() &&
         !new_target->initial_map()->is_dictionary_map() &&
         new_target->initial_map()->constructor_or_backpointer() == *target;
}

// When initializing arrays, we'll unfold the loop if the number of
// elements is known to be of this type.
const int kElementLoopUnrollLimit = 16;

// Limits up to which context allocations are inlined.
const int kFunctionContextAllocationLimit = 16;
const int kBlockContextAllocationLimit = 16;

// Determines whether the given array or object literal boilerplate satisfies
// all limits to be considered for fast deep-copying and computes the total
// size of all objects that are part of the graph.
bool IsFastLiteral(Handle<JSObject> boilerplate, int max_depth,
                   int* max_properties) {
  DCHECK_GE(max_depth, 0);
  DCHECK_GE(*max_properties, 0);

  // Make sure the boilerplate map is not deprecated.
  if (!JSObject::TryMigrateInstance(boilerplate)) return false;

  // Check for too deep nesting.
  if (max_depth == 0) return false;

  // Check the elements.
  Isolate* const isolate = boilerplate->GetIsolate();
  Handle<FixedArrayBase> elements(boilerplate->elements(), isolate);
  if (elements->length() > 0 &&
      elements->map() != isolate->heap()->fixed_cow_array_map()) {
    if (boilerplate->HasSmiOrObjectElements()) {
      Handle<FixedArray> fast_elements = Handle<FixedArray>::cast(elements);
      int length = elements->length();
      for (int i = 0; i < length; i++) {
        if ((*max_properties)-- == 0) return false;
        Handle<Object> value(fast_elements->get(i), isolate);
        if (value->IsJSObject()) {
          Handle<JSObject> value_object = Handle<JSObject>::cast(value);
          if (!IsFastLiteral(value_object, max_depth - 1, max_properties)) {
            return false;
          }
        }
      }
    } else if (boilerplate->HasDoubleElements()) {
      if (elements->Size() > kMaxRegularHeapObjectSize) return false;
    } else {
      return false;
    }
  }

  // TODO(turbofan): Do we want to support out-of-object properties?
  if (!(boilerplate->HasFastProperties() &&
        boilerplate->property_array()->length() == 0)) {
    return false;
  }

  // Check the in-object properties.
  Handle<DescriptorArray> descriptors(
      boilerplate->map()->instance_descriptors(), isolate);
  int limit = boilerplate->map()->NumberOfOwnDescriptors();
  for (int i = 0; i < limit; i++) {
    PropertyDetails details = descriptors->GetDetails(i);
    if (details.location() != kField) continue;
    DCHECK_EQ(kData, details.kind());
    if ((*max_properties)-- == 0) return false;
    FieldIndex field_index = FieldIndex::ForDescriptor(boilerplate->map(), i);
    if (boilerplate->IsUnboxedDoubleField(field_index)) continue;
    Handle<Object> value(boilerplate->RawFastPropertyAt(field_index), isolate);
    if (value->IsJSObject()) {
      Handle<JSObject> value_object = Handle<JSObject>::cast(value);
      if (!IsFastLiteral(value_object, max_depth - 1, max_properties)) {
        return false;
      }
    }
  }
  return true;
}

// Maximum depth and total number of elements and properties for literal
// graphs to be considered for fast deep-copying. The limit is chosen to
// match the maximum number of inobject properties, to ensure that the
// performance of using object literals is not worse than using constructor
// functions, see crbug.com/v8/6211 for details.
const int kMaxFastLiteralDepth = 3;
const int kMaxFastLiteralProperties = JSObject::kMaxInObjectProperties;

}  // namespace

Reduction JSCreateLowering::Reduce(Node* node) {
  switch (node->opcode()) {
    case IrOpcode::kJSCreate:
      return ReduceJSCreate(node);
    case IrOpcode::kJSCreateArguments:
      return ReduceJSCreateArguments(node);
    case IrOpcode::kJSCreateArray:
      return ReduceJSCreateArray(node);
    case IrOpcode::kJSCreateArrayIterator:
      return ReduceJSCreateArrayIterator(node);
    case IrOpcode::kJSCreateBoundFunction:
      return ReduceJSCreateBoundFunction(node);
    case IrOpcode::kJSCreateClosure:
      return ReduceJSCreateClosure(node);
    case IrOpcode::kJSCreateCollectionIterator:
      return ReduceJSCreateCollectionIterator(node);
    case IrOpcode::kJSCreateIterResultObject:
      return ReduceJSCreateIterResultObject(node);
    case IrOpcode::kJSCreateStringIterator:
      return ReduceJSCreateStringIterator(node);
    case IrOpcode::kJSCreateKeyValueArray:
      return ReduceJSCreateKeyValueArray(node);
    case IrOpcode::kJSCreatePromise:
      return ReduceJSCreatePromise(node);
    case IrOpcode::kJSCreateLiteralArray:
    case IrOpcode::kJSCreateLiteralObject:
      return ReduceJSCreateLiteralArrayOrObject(node);
    case IrOpcode::kJSCreateLiteralRegExp:
      return ReduceJSCreateLiteralRegExp(node);
    case IrOpcode::kJSCreateEmptyLiteralArray:
      return ReduceJSCreateEmptyLiteralArray(node);
    case IrOpcode::kJSCreateEmptyLiteralObject:
      return ReduceJSCreateEmptyLiteralObject(node);
    case IrOpcode::kJSCreateFunctionContext:
      return ReduceJSCreateFunctionContext(node);
    case IrOpcode::kJSCreateWithContext:
      return ReduceJSCreateWithContext(node);
    case IrOpcode::kJSCreateCatchContext:
      return ReduceJSCreateCatchContext(node);
    case IrOpcode::kJSCreateBlockContext:
      return ReduceJSCreateBlockContext(node);
    case IrOpcode::kJSCreateGeneratorObject:
      return ReduceJSCreateGeneratorObject(node);
    default:
      break;
  }
  return NoChange();
}

Reduction JSCreateLowering::ReduceJSCreate(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCreate, node->opcode());
  Node* const target = NodeProperties::GetValueInput(node, 0);
  Type* const target_type = NodeProperties::GetType(target);
  Node* const new_target = NodeProperties::GetValueInput(node, 1);
  Type* const new_target_type = NodeProperties::GetType(new_target);
  Node* const effect = NodeProperties::GetEffectInput(node);
  Node* const control = NodeProperties::GetControlInput(node);
  // Extract constructor and original constructor function.
  if (target_type->IsHeapConstant() && new_target_type->IsHeapConstant() &&
      target_type->AsHeapConstant()->Value()->IsJSFunction() &&
      new_target_type->AsHeapConstant()->Value()->IsJSFunction()) {
    Handle<JSFunction> constructor =
        Handle<JSFunction>::cast(target_type->AsHeapConstant()->Value());
    if (!constructor->IsConstructor()) return NoChange();
    Handle<JSFunction> original_constructor =
        Handle<JSFunction>::cast(new_target_type->AsHeapConstant()->Value());
    if (!original_constructor->IsConstructor()) return NoChange();

    // Check if we can inline the allocation.
    if (IsAllocationInlineable(constructor, original_constructor)) {
      // Force completion of inobject slack tracking before
      // generating code to finalize the instance size.
      original_constructor->CompleteInobjectSlackTrackingIfActive();
      Handle<Map> initial_map(original_constructor->initial_map(), isolate());
      int const instance_size = initial_map->instance_size();

      // Add a dependency on the {initial_map} to make sure that this code is
      // deoptimized whenever the {initial_map} changes.
      dependencies()->AssumeInitialMapCantChange(initial_map);

      // Emit code to allocate the JSObject instance for the
      // {original_constructor}.
      AllocationBuilder a(jsgraph(), effect, control);
      a.Allocate(instance_size);
      a.Store(AccessBuilder::ForMap(), initial_map);
      a.Store(AccessBuilder::ForJSObjectPropertiesOrHash(),
              jsgraph()->EmptyFixedArrayConstant());
      a.Store(AccessBuilder::ForJSObjectElements(),
              jsgraph()->EmptyFixedArrayConstant());
      for (int i = 0; i < initial_map->GetInObjectProperties(); ++i) {
        a.Store(AccessBuilder::ForJSObjectInObjectProperty(initial_map, i),
                jsgraph()->UndefinedConstant());
      }
      RelaxControls(node);
      a.FinishAndChange(node);
      return Changed(node);
    }
  }
  return NoChange();
}

Reduction JSCreateLowering::ReduceJSCreateArguments(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCreateArguments, node->opcode());
  CreateArgumentsType type = CreateArgumentsTypeOf(node->op());
  Node* const frame_state = NodeProperties::GetFrameStateInput(node);
  Node* const outer_state = frame_state->InputAt(kFrameStateOuterStateInput);
  Node* const control = graph()->start();
  FrameStateInfo state_info = FrameStateInfoOf(frame_state->op());
  Handle<SharedFunctionInfo> shared =
      state_info.shared_info().ToHandleChecked();

  // Use the ArgumentsAccessStub for materializing both mapped and unmapped
  // arguments object, but only for non-inlined (i.e. outermost) frames.
  if (outer_state->opcode() != IrOpcode::kFrameState) {
    switch (type) {
      case CreateArgumentsType::kMappedArguments: {
        // TODO(mstarzinger): Duplicate parameters are not handled yet.
        if (shared->has_duplicate_parameters()) return NoChange();
        Node* const callee = NodeProperties::GetValueInput(node, 0);
        Node* const context = NodeProperties::GetContextInput(node);
        Node* effect = NodeProperties::GetEffectInput(node);
        Node* const arguments_frame =
            graph()->NewNode(simplified()->ArgumentsFrame());
        Node* const arguments_length = graph()->NewNode(
            simplified()->ArgumentsLength(
                shared->internal_formal_parameter_count(), false),
            arguments_frame);
        // Allocate the elements backing store.
        bool has_aliased_arguments = false;
        Node* const elements = effect = AllocateAliasedArguments(
            effect, control, context, arguments_frame, arguments_length, shared,
            &has_aliased_arguments);
        // Load the arguments object map.
        Node* const arguments_map = jsgraph()->HeapConstant(
            handle(has_aliased_arguments
                       ? native_context()->fast_aliased_arguments_map()
                       : native_context()->sloppy_arguments_map(),
                   isolate()));
        // Actually allocate and initialize the arguments object.
        AllocationBuilder a(jsgraph(), effect, control);
        Node* properties = jsgraph()->EmptyFixedArrayConstant();
        STATIC_ASSERT(JSSloppyArgumentsObject::kSize == 5 * kPointerSize);
        a.Allocate(JSSloppyArgumentsObject::kSize);
        a.Store(AccessBuilder::ForMap(), arguments_map);
        a.Store(AccessBuilder::ForJSObjectPropertiesOrHash(), properties);
        a.Store(AccessBuilder::ForJSObjectElements(), elements);
        a.Store(AccessBuilder::ForArgumentsLength(), arguments_length);
        a.Store(AccessBuilder::ForArgumentsCallee(), callee);
        RelaxControls(node);
        a.FinishAndChange(node);
        return Changed(node);
      }
      case CreateArgumentsType::kUnmappedArguments: {
        Node* effect = NodeProperties::GetEffectInput(node);
        Node* const arguments_frame =
            graph()->NewNode(simplified()->ArgumentsFrame());
        Node* const arguments_length = graph()->NewNode(
            simplified()->ArgumentsLength(
                shared->internal_formal_parameter_count(), false),
            arguments_frame);
        // Allocate the elements backing store.
        Node* const elements = effect =
            graph()->NewNode(simplified()->NewArgumentsElements(0),
                             arguments_frame, arguments_length, effect);
        // Load the arguments object map.
        Node* const arguments_map = jsgraph()->HeapConstant(
            handle(native_context()->strict_arguments_map(), isolate()));
        // Actually allocate and initialize the arguments object.
        AllocationBuilder a(jsgraph(), effect, control);
        Node* properties = jsgraph()->EmptyFixedArrayConstant();
        STATIC_ASSERT(JSStrictArgumentsObject::kSize == 4 * kPointerSize);
        a.Allocate(JSStrictArgumentsObject::kSize);
        a.Store(AccessBuilder::ForMap(), arguments_map);
        a.Store(AccessBuilder::ForJSObjectPropertiesOrHash(), properties);
        a.Store(AccessBuilder::ForJSObjectElements(), elements);
        a.Store(AccessBuilder::ForArgumentsLength(), arguments_length);
        RelaxControls(node);
        a.FinishAndChange(node);
        return Changed(node);
      }
      case CreateArgumentsType::kRestParameter: {
        Node* effect = NodeProperties::GetEffectInput(node);
        Node* const arguments_frame =
            graph()->NewNode(simplified()->ArgumentsFrame());
        Node* const rest_length = graph()->NewNode(
            simplified()->ArgumentsLength(
                shared->internal_formal_parameter_count(), true),
            arguments_frame);
        // Allocate the elements backing store. Since NewArgumentsElements
        // copies from the end of the arguments adapter frame, this is a suffix
        // of the actual arguments.
        Node* const elements = effect =
            graph()->NewNode(simplified()->NewArgumentsElements(0),
                             arguments_frame, rest_length, effect);
        // Load the JSArray object map.
        Node* const jsarray_map = jsgraph()->HeapConstant(handle(
            native_context()->js_array_fast_elements_map_index(), isolate()));
        // Actually allocate and initialize the jsarray.
        AllocationBuilder a(jsgraph(), effect, control);
        Node* properties = jsgraph()->EmptyFixedArrayConstant();
        STATIC_ASSERT(JSArray::kSize == 4 * kPointerSize);
        a.Allocate(JSArray::kSize);
        a.Store(AccessBuilder::ForMap(), jsarray_map);
        a.Store(AccessBuilder::ForJSObjectPropertiesOrHash(), properties);
        a.Store(AccessBuilder::ForJSObjectElements(), elements);
        a.Store(AccessBuilder::ForJSArrayLength(PACKED_ELEMENTS), rest_length);
        RelaxControls(node);
        a.FinishAndChange(node);
        return Changed(node);
      }
    }
    UNREACHABLE();
  } else if (outer_state->opcode() == IrOpcode::kFrameState) {
    // Use inline allocation for all mapped arguments objects within inlined
    // (i.e. non-outermost) frames, independent of the object size.
    if (type == CreateArgumentsType::kMappedArguments) {
      Node* const callee = NodeProperties::GetValueInput(node, 0);
      Node* const context = NodeProperties::GetContextInput(node);
      Node* effect = NodeProperties::GetEffectInput(node);
      // TODO(mstarzinger): Duplicate parameters are not handled yet.
      if (shared->has_duplicate_parameters()) return NoChange();
      // Choose the correct frame state and frame state info depending on
      // whether there conceptually is an arguments adaptor frame in the call
      // chain.
      Node* const args_state = GetArgumentsFrameState(frame_state);
      if (args_state->InputAt(kFrameStateParametersInput)->opcode() ==
          IrOpcode::kDeadValue) {
        // This protects against an incompletely propagated DeadValue node.
        // If the FrameState has a DeadValue input, then this node will be
        // pruned anyway.
        return NoChange();
      }
      FrameStateInfo args_state_info = FrameStateInfoOf(args_state->op());
      // Prepare element backing store to be used by arguments object.
      bool has_aliased_arguments = false;
      Node* const elements = AllocateAliasedArguments(
          effect, control, args_state, context, shared, &has_aliased_arguments);
      effect = elements->op()->EffectOutputCount() > 0 ? elements : effect;
      // Load the arguments object map.
      Node* const arguments_map = jsgraph()->HeapConstant(handle(
          has_aliased_arguments ? native_context()->fast_aliased_arguments_map()
                                : native_context()->sloppy_arguments_map(),
          isolate()));
      // Actually allocate and initialize the arguments object.
      AllocationBuilder a(jsgraph(), effect, control);
      Node* properties = jsgraph()->EmptyFixedArrayConstant();
      int length = args_state_info.parameter_count() - 1;  // Minus receiver.
      STATIC_ASSERT(JSSloppyArgumentsObject::kSize == 5 * kPointerSize);
      a.Allocate(JSSloppyArgumentsObject::kSize);
      a.Store(AccessBuilder::ForMap(), arguments_map);
      a.Store(AccessBuilder::ForJSObjectPropertiesOrHash(), properties);
      a.Store(AccessBuilder::ForJSObjectElements(), elements);
      a.Store(AccessBuilder::ForArgumentsLength(), jsgraph()->Constant(length));
      a.Store(AccessBuilder::ForArgumentsCallee(), callee);
      RelaxControls(node);
      a.FinishAndChange(node);
      return Changed(node);
    } else if (type == CreateArgumentsType::kUnmappedArguments) {
      // Use inline allocation for all unmapped arguments objects within inlined
      // (i.e. non-outermost) frames, independent of the object size.
      Node* effect = NodeProperties::GetEffectInput(node);
      // Choose the correct frame state and frame state info depending on
      // whether there conceptually is an arguments adaptor frame in the call
      // chain.
      Node* const args_state = GetArgumentsFrameState(frame_state);
      if (args_state->InputAt(kFrameStateParametersInput)->opcode() ==
          IrOpcode::kDeadValue) {
        // This protects against an incompletely propagated DeadValue node.
        // If the FrameState has a DeadValue input, then this node will be
        // pruned anyway.
        return NoChange();
      }
      FrameStateInfo args_state_info = FrameStateInfoOf(args_state->op());
      // Prepare element backing store to be used by arguments object.
      Node* const elements = AllocateArguments(effect, control, args_state);
      effect = elements->op()->EffectOutputCount() > 0 ? elements : effect;
      // Load the arguments object map.
      Node* const arguments_map = jsgraph()->HeapConstant(
          handle(native_context()->strict_arguments_map(), isolate()));
      // Actually allocate and initialize the arguments object.
      AllocationBuilder a(jsgraph(), effect, control);
      Node* properties = jsgraph()->EmptyFixedArrayConstant();
      int length = args_state_info.parameter_count() - 1;  // Minus receiver.
      STATIC_ASSERT(JSStrictArgumentsObject::kSize == 4 * kPointerSize);
      a.Allocate(JSStrictArgumentsObject::kSize);
      a.Store(AccessBuilder::ForMap(), arguments_map);
      a.Store(AccessBuilder::ForJSObjectPropertiesOrHash(), properties);
      a.Store(AccessBuilder::ForJSObjectElements(), elements);
      a.Store(AccessBuilder::ForArgumentsLength(), jsgraph()->Constant(length));
      RelaxControls(node);
      a.FinishAndChange(node);
      return Changed(node);
    } else if (type == CreateArgumentsType::kRestParameter) {
      int start_index = shared->internal_formal_parameter_count();
      // Use inline allocation for all unmapped arguments objects within inlined
      // (i.e. non-outermost) frames, independent of the object size.
      Node* effect = NodeProperties::GetEffectInput(node);
      // Choose the correct frame state and frame state info depending on
      // whether there conceptually is an arguments adaptor frame in the call
      // chain.
      Node* const args_state = GetArgumentsFrameState(frame_state);
      if (args_state->InputAt(kFrameStateParametersInput)->opcode() ==
          IrOpcode::kDeadValue) {
        // This protects against an incompletely propagated DeadValue node.
        // If the FrameState has a DeadValue input, then this node will be
        // pruned anyway.
        return NoChange();
      }
      FrameStateInfo args_state_info = FrameStateInfoOf(args_state->op());
      // Prepare element backing store to be used by the rest array.
      Node* const elements =
          AllocateRestArguments(effect, control, args_state, start_index);
      effect = elements->op()->EffectOutputCount() > 0 ? elements : effect;
      // Load the JSArray object map.
      Node* const jsarray_map = jsgraph()->HeapConstant(handle(
          native_context()->js_array_fast_elements_map_index(), isolate()));
      // Actually allocate and initialize the jsarray.
      AllocationBuilder a(jsgraph(), effect, control);
      Node* properties = jsgraph()->EmptyFixedArrayConstant();

      // -1 to minus receiver
      int argument_count = args_state_info.parameter_count() - 1;
      int length = std::max(0, argument_count - start_index);
      STATIC_ASSERT(JSArray::kSize == 4 * kPointerSize);
      a.Allocate(JSArray::kSize);
      a.Store(AccessBuilder::ForMap(), jsarray_map);
      a.Store(AccessBuilder::ForJSObjectPropertiesOrHash(), properties);
      a.Store(AccessBuilder::ForJSObjectElements(), elements);
      a.Store(AccessBuilder::ForJSArrayLength(PACKED_ELEMENTS),
              jsgraph()->Constant(length));
      RelaxControls(node);
      a.FinishAndChange(node);
      return Changed(node);
    }
  }

  return NoChange();
}

Reduction JSCreateLowering::ReduceJSCreateGeneratorObject(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCreateGeneratorObject, node->opcode());
  Node* const closure = NodeProperties::GetValueInput(node, 0);
  Node* const receiver = NodeProperties::GetValueInput(node, 1);
  Node* const context = NodeProperties::GetContextInput(node);
  Type* const closure_type = NodeProperties::GetType(closure);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* const control = NodeProperties::GetControlInput(node);
  if (closure_type->IsHeapConstant()) {
    DCHECK(closure_type->AsHeapConstant()->Value()->IsJSFunction());
    Handle<JSFunction> js_function =
        Handle<JSFunction>::cast(closure_type->AsHeapConstant()->Value());
    JSFunction::EnsureHasInitialMap(js_function);

    // Force completion of inobject slack tracking before
    // generating code to finalize the instance size.
    js_function->CompleteInobjectSlackTrackingIfActive();
    Handle<Map> initial_map(js_function->initial_map(), isolate());
    DCHECK(initial_map->instance_type() == JS_GENERATOR_OBJECT_TYPE ||
           initial_map->instance_type() == JS_ASYNC_GENERATOR_OBJECT_TYPE);

    // Add a dependency on the {initial_map} to make sure that this code is
    // deoptimized whenever the {initial_map} changes.
    dependencies()->AssumeInitialMapCantChange(initial_map);

    // Allocate a register file.
    DCHECK(js_function->shared()->HasBytecodeArray());
    int size = js_function->shared()->GetBytecodeArray()->register_count();
    AllocationBuilder ab(jsgraph(), effect, control);
    ab.AllocateArray(size, factory()->fixed_array_map());
    for (int i = 0; i < size; ++i) {
      ab.Store(AccessBuilder::ForFixedArraySlot(i),
               jsgraph()->UndefinedConstant());
    }
    Node* register_file = effect = ab.Finish();

    // Emit code to allocate the JS[Async]GeneratorObject instance.
    AllocationBuilder a(jsgraph(), effect, control);
    a.Allocate(initial_map->instance_size());
    Node* empty_fixed_array = jsgraph()->EmptyFixedArrayConstant();
    Node* undefined = jsgraph()->UndefinedConstant();
    a.Store(AccessBuilder::ForMap(), initial_map);
    a.Store(AccessBuilder::ForJSObjectPropertiesOrHash(), empty_fixed_array);
    a.Store(AccessBuilder::ForJSObjectElements(), empty_fixed_array);
    a.Store(AccessBuilder::ForJSGeneratorObjectContext(), context);
    a.Store(AccessBuilder::ForJSGeneratorObjectFunction(), closure);
    a.Store(AccessBuilder::ForJSGeneratorObjectReceiver(), receiver);
    a.Store(AccessBuilder::ForJSGeneratorObjectInputOrDebugPos(), undefined);
    a.Store(AccessBuilder::ForJSGeneratorObjectResumeMode(),
            jsgraph()->Constant(JSGeneratorObject::kNext));
    a.Store(AccessBuilder::ForJSGeneratorObjectContinuation(),
            jsgraph()->Constant(JSGeneratorObject::kGeneratorExecuting));
    a.Store(AccessBuilder::ForJSGeneratorObjectRegisterFile(), register_file);

    if (initial_map->instance_type() == JS_ASYNC_GENERATOR_OBJECT_TYPE) {
      a.Store(AccessBuilder::ForJSAsyncGeneratorObjectQueue(), undefined);
      a.Store(AccessBuilder::ForJSAsyncGeneratorObjectIsAwaiting(),
              jsgraph()->ZeroConstant());
    }

    // Handle in-object properties, too.
    for (int i = 0; i < initial_map->GetInObjectProperties(); ++i) {
      a.Store(AccessBuilder::ForJSObjectInObjectProperty(initial_map, i),
              undefined);
    }
    a.FinishAndChange(node);
    return Changed(node);
  }
  return NoChange();
}

// Constructs an array with a variable {length} when no upper bound
// is known for the capacity.
Reduction JSCreateLowering::ReduceNewArray(Node* node, Node* length,
                                           Handle<Map> initial_map,
                                           PretenureFlag pretenure) {
  DCHECK_EQ(IrOpcode::kJSCreateArray, node->opcode());
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // Constructing an Array via new Array(N) where N is an unsigned
  // integer, always creates a holey backing store.
  if (!IsHoleyElementsKind(initial_map->elements_kind())) {
    initial_map = Map::AsElementsKind(
        initial_map, GetHoleyElementsKind(initial_map->elements_kind()));
  }

  // Check that the {limit} is an unsigned integer in the valid range.
  // This has to be kept in sync with src/runtime/runtime-array.cc,
  // where this limit is protected.
  length = effect = graph()->NewNode(
      simplified()->CheckBounds(VectorSlotPair()), length,
      jsgraph()->Constant(JSArray::kInitialMaxFastElementArray), effect,
      control);

  // Construct elements and properties for the resulting JSArray.
  Node* elements = effect =
      graph()->NewNode(IsDoubleElementsKind(initial_map->elements_kind())
                           ? simplified()->NewDoubleElements(pretenure)
                           : simplified()->NewSmiOrObjectElements(pretenure),
                       length, effect, control);
  Node* properties = jsgraph()->EmptyFixedArrayConstant();

  // Perform the allocation of the actual JSArray object.
  AllocationBuilder a(jsgraph(), effect, control);
  a.Allocate(initial_map->instance_size(), pretenure);
  a.Store(AccessBuilder::ForMap(), initial_map);
  a.Store(AccessBuilder::ForJSObjectPropertiesOrHash(), properties);
  a.Store(AccessBuilder::ForJSObjectElements(), elements);
  a.Store(AccessBuilder::ForJSArrayLength(initial_map->elements_kind()),
          length);
  for (int i = 0; i < initial_map->GetInObjectProperties(); ++i) {
    a.Store(AccessBuilder::ForJSObjectInObjectProperty(initial_map, i),
            jsgraph()->UndefinedConstant());
  }
  RelaxControls(node);
  a.FinishAndChange(node);
  return Changed(node);
}

// Constructs an array with a variable {length} when an actual
// upper bound is known for the {capacity}.
Reduction JSCreateLowering::ReduceNewArray(Node* node, Node* length,
                                           int capacity,
                                           Handle<Map> initial_map,
                                           PretenureFlag pretenure) {
  DCHECK(node->opcode() == IrOpcode::kJSCreateArray ||
         node->opcode() == IrOpcode::kJSCreateEmptyLiteralArray);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // Determine the appropriate elements kind.
  ElementsKind elements_kind = initial_map->elements_kind();
  if (NodeProperties::GetType(length)->Max() > 0.0) {
    elements_kind = GetHoleyElementsKind(elements_kind);
    initial_map = Map::AsElementsKind(initial_map, elements_kind);
  }
  DCHECK(IsFastElementsKind(elements_kind));

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
  a.Allocate(initial_map->instance_size(), pretenure);
  a.Store(AccessBuilder::ForMap(), initial_map);
  a.Store(AccessBuilder::ForJSObjectPropertiesOrHash(), properties);
  a.Store(AccessBuilder::ForJSObjectElements(), elements);
  a.Store(AccessBuilder::ForJSArrayLength(elements_kind), length);
  for (int i = 0; i < initial_map->GetInObjectProperties(); ++i) {
    a.Store(AccessBuilder::ForJSObjectInObjectProperty(initial_map, i),
            jsgraph()->UndefinedConstant());
  }
  RelaxControls(node);
  a.FinishAndChange(node);
  return Changed(node);
}

Reduction JSCreateLowering::ReduceNewArray(Node* node,
                                           std::vector<Node*> values,
                                           Handle<Map> initial_map,
                                           PretenureFlag pretenure) {
  DCHECK_EQ(IrOpcode::kJSCreateArray, node->opcode());
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // Determine the appropriate elements kind.
  ElementsKind elements_kind = initial_map->elements_kind();
  DCHECK(IsFastElementsKind(elements_kind));

  // Check {values} based on the {elements_kind}. These checks are guarded
  // by the {elements_kind} feedback on the {site}, so it's safe to just
  // deoptimize in this case.
  if (IsSmiElementsKind(elements_kind)) {
    for (auto& value : values) {
      if (!NodeProperties::GetType(value)->Is(Type::SignedSmall())) {
        value = effect = graph()->NewNode(
            simplified()->CheckSmi(VectorSlotPair()), value, effect, control);
      }
    }
  } else if (IsDoubleElementsKind(elements_kind)) {
    for (auto& value : values) {
      if (!NodeProperties::GetType(value)->Is(Type::Number())) {
        value = effect =
            graph()->NewNode(simplified()->CheckNumber(VectorSlotPair()), value,
                             effect, control);
      }
      // Make sure we do not store signaling NaNs into double arrays.
      value = graph()->NewNode(simplified()->NumberSilenceNaN(), value);
    }
  }

  // Setup elements, properties and length.
  Node* elements = effect =
      AllocateElements(effect, control, elements_kind, values, pretenure);
  Node* properties = jsgraph()->EmptyFixedArrayConstant();
  Node* length = jsgraph()->Constant(static_cast<int>(values.size()));

  // Perform the allocation of the actual JSArray object.
  AllocationBuilder a(jsgraph(), effect, control);
  a.Allocate(initial_map->instance_size(), pretenure);
  a.Store(AccessBuilder::ForMap(), initial_map);
  a.Store(AccessBuilder::ForJSObjectPropertiesOrHash(), properties);
  a.Store(AccessBuilder::ForJSObjectElements(), elements);
  a.Store(AccessBuilder::ForJSArrayLength(elements_kind), length);
  for (int i = 0; i < initial_map->GetInObjectProperties(); ++i) {
    a.Store(AccessBuilder::ForJSObjectInObjectProperty(initial_map, i),
            jsgraph()->UndefinedConstant());
  }
  RelaxControls(node);
  a.FinishAndChange(node);
  return Changed(node);
}

Reduction JSCreateLowering::ReduceNewArrayToStubCall(
    Node* node, Handle<AllocationSite> site) {
  CreateArrayParameters const& p = CreateArrayParametersOf(node->op());
  int const arity = static_cast<int>(p.arity());
  Node* target = NodeProperties::GetValueInput(node, 0);
  Node* new_target = NodeProperties::GetValueInput(node, 1);
  Type* new_target_type = NodeProperties::GetType(new_target);
  Node* type_info = site.is_null() ? jsgraph()->UndefinedConstant()
                                   : jsgraph()->HeapConstant(site);

  ElementsKind elements_kind =
      site.is_null() ? GetInitialFastElementsKind() : site->GetElementsKind();
  AllocationSiteOverrideMode override_mode =
      (site.is_null() || AllocationSite::ShouldTrack(elements_kind))
          ? DISABLE_ALLOCATION_SITES
          : DONT_OVERRIDE;

  // The Array constructor can only trigger an observable side-effect
  // if the new.target may be a proxy.
  Operator::Properties const properties =
      (new_target != target || new_target_type->Maybe(Type::Proxy()))
          ? Operator::kNoDeopt
          : Operator::kNoDeopt | Operator::kNoWrite;

  if (arity == 0) {
    ArrayNoArgumentConstructorStub stub(isolate(), elements_kind,
                                        override_mode);
    auto call_descriptor = Linkage::GetStubCallDescriptor(
        isolate(), graph()->zone(), stub.GetCallInterfaceDescriptor(),
        arity + 1, CallDescriptor::kNeedsFrameState, properties);
    node->ReplaceInput(0, jsgraph()->HeapConstant(stub.GetCode()));
    node->InsertInput(graph()->zone(), 2, type_info);
    node->InsertInput(graph()->zone(), 3, jsgraph()->Constant(arity));
    node->InsertInput(graph()->zone(), 4, jsgraph()->UndefinedConstant());
    NodeProperties::ChangeOp(node, common()->Call(call_descriptor));
  } else if (arity == 1) {
    // Require elements kind to "go holey".
    ArraySingleArgumentConstructorStub stub(
        isolate(), GetHoleyElementsKind(elements_kind), override_mode);
    auto call_descriptor = Linkage::GetStubCallDescriptor(
        isolate(), graph()->zone(), stub.GetCallInterfaceDescriptor(),
        arity + 1, CallDescriptor::kNeedsFrameState, properties);
    node->ReplaceInput(0, jsgraph()->HeapConstant(stub.GetCode()));
    node->InsertInput(graph()->zone(), 2, type_info);
    node->InsertInput(graph()->zone(), 3, jsgraph()->Constant(arity));
    node->InsertInput(graph()->zone(), 4, jsgraph()->UndefinedConstant());
    NodeProperties::ChangeOp(node, common()->Call(call_descriptor));
  } else {
    DCHECK_GT(arity, 1);
    ArrayNArgumentsConstructorStub stub(isolate());
    auto call_descriptor = Linkage::GetStubCallDescriptor(
        isolate(), graph()->zone(), stub.GetCallInterfaceDescriptor(),
        arity + 1, CallDescriptor::kNeedsFrameState);
    node->ReplaceInput(0, jsgraph()->HeapConstant(stub.GetCode()));
    node->InsertInput(graph()->zone(), 2, type_info);
    node->InsertInput(graph()->zone(), 3, jsgraph()->Constant(arity));
    node->InsertInput(graph()->zone(), 4, jsgraph()->UndefinedConstant());
    NodeProperties::ChangeOp(node, common()->Call(call_descriptor));
  }
  return Changed(node);
}

Reduction JSCreateLowering::ReduceJSCreateArray(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCreateArray, node->opcode());
  CreateArrayParameters const& p = CreateArrayParametersOf(node->op());
  int const arity = static_cast<int>(p.arity());
  Handle<AllocationSite> const site = p.site();
  PretenureFlag pretenure = NOT_TENURED;
  Handle<JSFunction> constructor(native_context()->array_function(), isolate());
  Node* target = NodeProperties::GetValueInput(node, 0);
  Node* new_target = NodeProperties::GetValueInput(node, 1);
  Type* new_target_type = (target == new_target)
                              ? Type::HeapConstant(constructor, zone())
                              : NodeProperties::GetType(new_target);

  // Extract original constructor function.
  if (new_target_type->IsHeapConstant() &&
      new_target_type->AsHeapConstant()->Value()->IsJSFunction()) {
    Handle<JSFunction> original_constructor =
        Handle<JSFunction>::cast(new_target_type->AsHeapConstant()->Value());
    DCHECK(constructor->IsConstructor());
    DCHECK(original_constructor->IsConstructor());

    // Check if we can inline the allocation.
    if (IsAllocationInlineable(constructor, original_constructor)) {
      // Force completion of inobject slack tracking before
      // generating code to finalize the instance size.
      original_constructor->CompleteInobjectSlackTrackingIfActive();
      Handle<Map> initial_map(original_constructor->initial_map(), isolate());

      // Add a dependency on the {initial_map} to make sure that this code is
      // deoptimized whenever the {initial_map} changes.
      dependencies()->AssumeInitialMapCantChange(initial_map);

      // Tells whether we are protected by either the {site} or a
      // protector cell to do certain speculative optimizations.
      bool can_inline_call = false;

      // Check if we have a feedback {site} on the {node}.
      if (!site.is_null()) {
        ElementsKind elements_kind = site->GetElementsKind();
        if (initial_map->elements_kind() != elements_kind) {
          initial_map = Map::AsElementsKind(initial_map, elements_kind);
        }
        can_inline_call = site->CanInlineCall();
        pretenure = site->GetPretenureMode();

        dependencies()->AssumeTransitionStable(site);
        dependencies()->AssumeTenuringDecision(site);
      } else {
        can_inline_call = isolate()->IsArrayConstructorIntact();
      }

      if (arity == 0) {
        Node* length = jsgraph()->ZeroConstant();
        int capacity = JSArray::kPreallocatedArrayElements;
        return ReduceNewArray(node, length, capacity, initial_map, pretenure);
      } else if (arity == 1) {
        Node* length = NodeProperties::GetValueInput(node, 2);
        Type* length_type = NodeProperties::GetType(length);
        if (!length_type->Maybe(Type::Number())) {
          // Handle the single argument case, where we know that the value
          // cannot be a valid Array length.
          ElementsKind elements_kind = initial_map->elements_kind();
          elements_kind = GetMoreGeneralElementsKind(
              elements_kind, IsHoleyElementsKind(elements_kind)
                                 ? HOLEY_ELEMENTS
                                 : PACKED_ELEMENTS);
          initial_map = Map::AsElementsKind(initial_map, elements_kind);
          return ReduceNewArray(node, std::vector<Node*>{length}, initial_map,
                                pretenure);
        }
        if (length_type->Is(Type::SignedSmall()) && length_type->Min() >= 0 &&
            length_type->Max() <= kElementLoopUnrollLimit &&
            length_type->Min() == length_type->Max()) {
          int capacity = static_cast<int>(length_type->Max());
          return ReduceNewArray(node, length, capacity, initial_map, pretenure);
        }
        if (length_type->Maybe(Type::UnsignedSmall()) && can_inline_call) {
          return ReduceNewArray(node, length, initial_map, pretenure);
        }
      } else if (arity <= JSArray::kInitialMaxFastElementArray) {
        // Gather the values to store into the newly created array.
        bool values_all_smis = true, values_all_numbers = true,
             values_any_nonnumber = false;
        std::vector<Node*> values;
        values.reserve(p.arity());
        for (int i = 0; i < arity; ++i) {
          Node* value = NodeProperties::GetValueInput(node, 2 + i);
          Type* value_type = NodeProperties::GetType(value);
          if (!value_type->Is(Type::SignedSmall())) {
            values_all_smis = false;
          }
          if (!value_type->Is(Type::Number())) {
            values_all_numbers = false;
          }
          if (!value_type->Maybe(Type::Number())) {
            values_any_nonnumber = true;
          }
          values.push_back(value);
        }

        // Try to figure out the ideal elements kind statically.
        ElementsKind elements_kind = initial_map->elements_kind();
        if (values_all_smis) {
          // Smis can be stored with any elements kind.
        } else if (values_all_numbers) {
          elements_kind = GetMoreGeneralElementsKind(
              elements_kind, IsHoleyElementsKind(elements_kind)
                                 ? HOLEY_DOUBLE_ELEMENTS
                                 : PACKED_DOUBLE_ELEMENTS);
        } else if (values_any_nonnumber) {
          elements_kind = GetMoreGeneralElementsKind(
              elements_kind, IsHoleyElementsKind(elements_kind)
                                 ? HOLEY_ELEMENTS
                                 : PACKED_ELEMENTS);
        } else if (!can_inline_call) {
          // We have some crazy combination of types for the {values} where
          // there's no clear decision on the elements kind statically. And
          // we don't have a protection against deoptimization loops for the
          // checks that are introduced in the call to ReduceNewArray, so
          // we cannot inline this invocation of the Array constructor here.
          return NoChange();
        }
        initial_map = Map::AsElementsKind(initial_map, elements_kind);

        return ReduceNewArray(node, values, initial_map, pretenure);
      }
    }
  }

  // TODO(bmeurer): Optimize the subclassing case.
  if (target != new_target) return NoChange();

  return ReduceNewArrayToStubCall(node, site);
}

Reduction JSCreateLowering::ReduceJSCreateArrayIterator(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCreateArrayIterator, node->opcode());
  CreateArrayIteratorParameters const& p =
      CreateArrayIteratorParametersOf(node->op());
  Node* iterated_object = NodeProperties::GetValueInput(node, 0);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // Create the JSArrayIterator result.
  AllocationBuilder a(jsgraph(), effect, control);
  a.Allocate(JSArrayIterator::kSize, NOT_TENURED, Type::OtherObject());
  a.Store(AccessBuilder::ForMap(),
          handle(native_context()->initial_array_iterator_map(), isolate()));
  a.Store(AccessBuilder::ForJSObjectPropertiesOrHash(),
          jsgraph()->EmptyFixedArrayConstant());
  a.Store(AccessBuilder::ForJSObjectElements(),
          jsgraph()->EmptyFixedArrayConstant());
  a.Store(AccessBuilder::ForJSArrayIteratorIteratedObject(), iterated_object);
  a.Store(AccessBuilder::ForJSArrayIteratorNextIndex(),
          jsgraph()->ZeroConstant());
  a.Store(AccessBuilder::ForJSArrayIteratorKind(),
          jsgraph()->Constant(static_cast<int>(p.kind())));
  RelaxControls(node);
  a.FinishAndChange(node);
  return Changed(node);
}

namespace {

Context::Field ContextFieldForCollectionIterationKind(
    CollectionKind collection_kind, IterationKind iteration_kind) {
  switch (collection_kind) {
    case CollectionKind::kSet:
      switch (iteration_kind) {
        case IterationKind::kKeys:
          UNREACHABLE();
        case IterationKind::kValues:
          return Context::SET_VALUE_ITERATOR_MAP_INDEX;
        case IterationKind::kEntries:
          return Context::SET_KEY_VALUE_ITERATOR_MAP_INDEX;
      }
      break;
    case CollectionKind::kMap:
      switch (iteration_kind) {
        case IterationKind::kKeys:
          return Context::MAP_KEY_ITERATOR_MAP_INDEX;
        case IterationKind::kValues:
          return Context::MAP_VALUE_ITERATOR_MAP_INDEX;
        case IterationKind::kEntries:
          return Context::MAP_KEY_VALUE_ITERATOR_MAP_INDEX;
      }
      break;
  }
  UNREACHABLE();
}

}  // namespace

Reduction JSCreateLowering::ReduceJSCreateCollectionIterator(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCreateCollectionIterator, node->opcode());
  CreateCollectionIteratorParameters const& p =
      CreateCollectionIteratorParametersOf(node->op());
  Node* iterated_object = NodeProperties::GetValueInput(node, 0);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // Load the OrderedHashTable from the {receiver}.
  Node* table = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForJSCollectionTable()),
      iterated_object, effect, control);

  // Create the JSArrayIterator result.
  AllocationBuilder a(jsgraph(), effect, control);
  a.Allocate(JSCollectionIterator::kSize, NOT_TENURED, Type::OtherObject());
  a.Store(AccessBuilder::ForMap(),
          handle(native_context()->get(ContextFieldForCollectionIterationKind(
                     p.collection_kind(), p.iteration_kind())),
                 isolate()));
  a.Store(AccessBuilder::ForJSObjectPropertiesOrHash(),
          jsgraph()->EmptyFixedArrayConstant());
  a.Store(AccessBuilder::ForJSObjectElements(),
          jsgraph()->EmptyFixedArrayConstant());
  a.Store(AccessBuilder::ForJSCollectionIteratorTable(), table);
  a.Store(AccessBuilder::ForJSCollectionIteratorIndex(),
          jsgraph()->ZeroConstant());
  RelaxControls(node);
  a.FinishAndChange(node);
  return Changed(node);
}

Reduction JSCreateLowering::ReduceJSCreateBoundFunction(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCreateBoundFunction, node->opcode());
  CreateBoundFunctionParameters const& p =
      CreateBoundFunctionParametersOf(node->op());
  int const arity = static_cast<int>(p.arity());
  Handle<Map> const map = p.map();
  Node* bound_target_function = NodeProperties::GetValueInput(node, 0);
  Node* bound_this = NodeProperties::GetValueInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // Create the [[BoundArguments]] for the result.
  Node* bound_arguments = jsgraph()->EmptyFixedArrayConstant();
  if (arity > 0) {
    AllocationBuilder a(jsgraph(), effect, control);
    a.AllocateArray(arity, factory()->fixed_array_map());
    for (int i = 0; i < arity; ++i) {
      a.Store(AccessBuilder::ForFixedArraySlot(i),
              NodeProperties::GetValueInput(node, 2 + i));
    }
    bound_arguments = effect = a.Finish();
  }

  // Create the JSBoundFunction result.
  AllocationBuilder a(jsgraph(), effect, control);
  a.Allocate(JSBoundFunction::kSize, NOT_TENURED, Type::BoundFunction());
  a.Store(AccessBuilder::ForMap(), map);
  a.Store(AccessBuilder::ForJSObjectPropertiesOrHash(),
          jsgraph()->EmptyFixedArrayConstant());
  a.Store(AccessBuilder::ForJSObjectElements(),
          jsgraph()->EmptyFixedArrayConstant());
  a.Store(AccessBuilder::ForJSBoundFunctionBoundTargetFunction(),
          bound_target_function);
  a.Store(AccessBuilder::ForJSBoundFunctionBoundThis(), bound_this);
  a.Store(AccessBuilder::ForJSBoundFunctionBoundArguments(), bound_arguments);
  RelaxControls(node);
  a.FinishAndChange(node);
  return Changed(node);
}

Reduction JSCreateLowering::ReduceJSCreateClosure(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCreateClosure, node->opcode());
  CreateClosureParameters const& p = CreateClosureParametersOf(node->op());
  Handle<SharedFunctionInfo> shared = p.shared_info();
  Handle<FeedbackCell> feedback_cell = p.feedback_cell();
  Handle<Code> code = p.code();
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* context = NodeProperties::GetContextInput(node);

  // Use inline allocation of closures only for instantiation sites that have
  // seen more than one instantiation, this simplifies the generated code and
  // also serves as a heuristic of which allocation sites benefit from it.
  if (feedback_cell->map() != isolate()->heap()->many_closures_cell_map()) {
    // The generic path can only create closures for user functions.
    DCHECK_EQ(isolate()->builtins()->builtin(Builtins::kCompileLazy), *code);
    return NoChange();
  }

  Handle<Map> function_map(
      Map::cast(native_context()->get(shared->function_map_index())));
  DCHECK(!function_map->IsInobjectSlackTrackingInProgress());
  DCHECK(!function_map->is_dictionary_map());

  // TODO(turbofan): We should use the pretenure flag from {p} here,
  // but currently the heuristic in the parser works against us, as
  // it marks closures like
  //
  //   args[l] = function(...) { ... }
  //
  // for old-space allocation, which doesn't always make sense. For
  // example in case of the bluebird-parallel benchmark, where this
  // is a core part of the *promisify* logic (see crbug.com/810132).
  PretenureFlag pretenure = NOT_TENURED;

  // Emit code to allocate the JSFunction instance.
  STATIC_ASSERT(JSFunction::kSizeWithoutPrototype == 7 * kPointerSize);
  AllocationBuilder a(jsgraph(), effect, control);
  a.Allocate(function_map->instance_size(), pretenure, Type::Function());
  a.Store(AccessBuilder::ForMap(), function_map);
  a.Store(AccessBuilder::ForJSObjectPropertiesOrHash(),
          jsgraph()->EmptyFixedArrayConstant());
  a.Store(AccessBuilder::ForJSObjectElements(),
          jsgraph()->EmptyFixedArrayConstant());
  a.Store(AccessBuilder::ForJSFunctionSharedFunctionInfo(), shared);
  a.Store(AccessBuilder::ForJSFunctionContext(), context);
  a.Store(AccessBuilder::ForJSFunctionFeedbackCell(), feedback_cell);
  a.Store(AccessBuilder::ForJSFunctionCode(), code);
  STATIC_ASSERT(JSFunction::kSizeWithoutPrototype == 7 * kPointerSize);
  if (function_map->has_prototype_slot()) {
    a.Store(AccessBuilder::ForJSFunctionPrototypeOrInitialMap(),
            jsgraph()->TheHoleConstant());
    STATIC_ASSERT(JSFunction::kSizeWithPrototype == 8 * kPointerSize);
  }
  for (int i = 0; i < function_map->GetInObjectProperties(); i++) {
    a.Store(AccessBuilder::ForJSObjectInObjectProperty(function_map, i),
            jsgraph()->UndefinedConstant());
  }
  RelaxControls(node);
  a.FinishAndChange(node);
  return Changed(node);
}

Reduction JSCreateLowering::ReduceJSCreateIterResultObject(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCreateIterResultObject, node->opcode());
  Node* value = NodeProperties::GetValueInput(node, 0);
  Node* done = NodeProperties::GetValueInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);

  Node* iterator_result_map = jsgraph()->HeapConstant(
      handle(native_context()->iterator_result_map(), isolate()));

  // Emit code to allocate the JSIteratorResult instance.
  AllocationBuilder a(jsgraph(), effect, graph()->start());
  a.Allocate(JSIteratorResult::kSize);
  a.Store(AccessBuilder::ForMap(), iterator_result_map);
  a.Store(AccessBuilder::ForJSObjectPropertiesOrHash(),
          jsgraph()->EmptyFixedArrayConstant());
  a.Store(AccessBuilder::ForJSObjectElements(),
          jsgraph()->EmptyFixedArrayConstant());
  a.Store(AccessBuilder::ForJSIteratorResultValue(), value);
  a.Store(AccessBuilder::ForJSIteratorResultDone(), done);
  STATIC_ASSERT(JSIteratorResult::kSize == 5 * kPointerSize);
  a.FinishAndChange(node);
  return Changed(node);
}

Reduction JSCreateLowering::ReduceJSCreateStringIterator(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCreateStringIterator, node->opcode());
  Node* string = NodeProperties::GetValueInput(node, 0);
  Node* effect = NodeProperties::GetEffectInput(node);

  Node* map = jsgraph()->HeapConstant(
      handle(native_context()->string_iterator_map(), isolate()));
  // Allocate new iterator and attach the iterator to this string.
  AllocationBuilder a(jsgraph(), effect, graph()->start());
  a.Allocate(JSStringIterator::kSize, NOT_TENURED, Type::OtherObject());
  a.Store(AccessBuilder::ForMap(), map);
  a.Store(AccessBuilder::ForJSObjectPropertiesOrHash(),
          jsgraph()->EmptyFixedArrayConstant());
  a.Store(AccessBuilder::ForJSObjectElements(),
          jsgraph()->EmptyFixedArrayConstant());
  a.Store(AccessBuilder::ForJSStringIteratorString(), string);
  a.Store(AccessBuilder::ForJSStringIteratorIndex(), jsgraph()->SmiConstant(0));
  STATIC_ASSERT(JSIteratorResult::kSize == 5 * kPointerSize);
  a.FinishAndChange(node);
  return Changed(node);
}

Reduction JSCreateLowering::ReduceJSCreateKeyValueArray(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCreateKeyValueArray, node->opcode());
  Node* key = NodeProperties::GetValueInput(node, 0);
  Node* value = NodeProperties::GetValueInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);

  Node* array_map = jsgraph()->HeapConstant(
      handle(native_context()->js_array_fast_elements_map_index()));
  Node* properties = jsgraph()->EmptyFixedArrayConstant();
  Node* length = jsgraph()->Constant(2);

  AllocationBuilder aa(jsgraph(), effect, graph()->start());
  aa.AllocateArray(2, factory()->fixed_array_map());
  aa.Store(AccessBuilder::ForFixedArrayElement(PACKED_ELEMENTS),
           jsgraph()->ZeroConstant(), key);
  aa.Store(AccessBuilder::ForFixedArrayElement(PACKED_ELEMENTS),
           jsgraph()->OneConstant(), value);
  Node* elements = aa.Finish();

  AllocationBuilder a(jsgraph(), elements, graph()->start());
  a.Allocate(JSArray::kSize);
  a.Store(AccessBuilder::ForMap(), array_map);
  a.Store(AccessBuilder::ForJSObjectPropertiesOrHash(), properties);
  a.Store(AccessBuilder::ForJSObjectElements(), elements);
  a.Store(AccessBuilder::ForJSArrayLength(PACKED_ELEMENTS), length);
  STATIC_ASSERT(JSArray::kSize == 4 * kPointerSize);
  a.FinishAndChange(node);
  return Changed(node);
}

Reduction JSCreateLowering::ReduceJSCreatePromise(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCreatePromise, node->opcode());
  Node* effect = NodeProperties::GetEffectInput(node);

  Handle<Map> promise_map(native_context()->promise_function()->initial_map());

  AllocationBuilder a(jsgraph(), effect, graph()->start());
  a.Allocate(promise_map->instance_size());
  a.Store(AccessBuilder::ForMap(), promise_map);
  a.Store(AccessBuilder::ForJSObjectPropertiesOrHash(),
          jsgraph()->EmptyFixedArrayConstant());
  a.Store(AccessBuilder::ForJSObjectElements(),
          jsgraph()->EmptyFixedArrayConstant());
  a.Store(AccessBuilder::ForJSObjectOffset(JSPromise::kReactionsOrResultOffset),
          jsgraph()->ZeroConstant());
  STATIC_ASSERT(v8::Promise::kPending == 0);
  a.Store(AccessBuilder::ForJSObjectOffset(JSPromise::kFlagsOffset),
          jsgraph()->ZeroConstant());
  STATIC_ASSERT(JSPromise::kSize == 5 * kPointerSize);
  for (int i = 0; i < v8::Promise::kEmbedderFieldCount; ++i) {
    a.Store(
        AccessBuilder::ForJSObjectOffset(JSPromise::kSize + i * kPointerSize),
        jsgraph()->ZeroConstant());
  }
  a.FinishAndChange(node);
  return Changed(node);
}

Reduction JSCreateLowering::ReduceJSCreateLiteralArrayOrObject(Node* node) {
  DCHECK(node->opcode() == IrOpcode::kJSCreateLiteralArray ||
         node->opcode() == IrOpcode::kJSCreateLiteralObject);
  CreateLiteralParameters const& p = CreateLiteralParametersOf(node->op());
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  Handle<Object> feedback(p.feedback().vector()->Get(p.feedback().slot()),
                          isolate());
  if (feedback->IsAllocationSite()) {
    Handle<AllocationSite> site = Handle<AllocationSite>::cast(feedback);
    Handle<JSObject> boilerplate(site->boilerplate(), isolate());
    int max_properties = kMaxFastLiteralProperties;
    if (IsFastLiteral(boilerplate, kMaxFastLiteralDepth, &max_properties)) {
      AllocationSiteUsageContext site_context(isolate(), site, false);
      site_context.EnterNewScope();
      Node* value = effect =
          AllocateFastLiteral(effect, control, boilerplate, &site_context);
      site_context.ExitScope(site, boilerplate);
      ReplaceWithValue(node, value, effect, control);
      return Replace(value);
    }
  }
  return NoChange();
}

Reduction JSCreateLowering::ReduceJSCreateEmptyLiteralArray(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCreateEmptyLiteralArray, node->opcode());
  FeedbackParameter const& p = FeedbackParameterOf(node->op());
  Handle<Object> feedback(p.feedback().vector()->Get(p.feedback().slot()),
                          isolate());
  if (feedback->IsAllocationSite()) {
    Handle<AllocationSite> site = Handle<AllocationSite>::cast(feedback);
    DCHECK(!site->PointsToLiteral());
    Handle<Map> const initial_map(
        native_context()->GetInitialJSArrayMap(site->GetElementsKind()),
        isolate());
    PretenureFlag const pretenure = site->GetPretenureMode();
    dependencies()->AssumeTransitionStable(site);
    dependencies()->AssumeTenuringDecision(site);
    Node* length = jsgraph()->ZeroConstant();
    return ReduceNewArray(node, length, 0, initial_map, pretenure);
  }
  return NoChange();
}

Reduction JSCreateLowering::ReduceJSCreateEmptyLiteralObject(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCreateEmptyLiteralObject, node->opcode());
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // Retrieve the initial map for the object.
  Handle<Map> map = factory()->ObjectLiteralMapFromCache(native_context(), 0);
  DCHECK(!map->is_dictionary_map());
  DCHECK(!map->IsInobjectSlackTrackingInProgress());
  Node* js_object_map = jsgraph()->HeapConstant(map);

  // Setup elements and properties.
  Node* elements = jsgraph()->EmptyFixedArrayConstant();
  Node* properties = jsgraph()->EmptyFixedArrayConstant();

  // Perform the allocation of the actual JSArray object.
  AllocationBuilder a(jsgraph(), effect, control);
  a.Allocate(map->instance_size());
  a.Store(AccessBuilder::ForMap(), js_object_map);
  a.Store(AccessBuilder::ForJSObjectPropertiesOrHash(), properties);
  a.Store(AccessBuilder::ForJSObjectElements(), elements);
  for (int i = 0; i < map->GetInObjectProperties(); i++) {
    a.Store(AccessBuilder::ForJSObjectInObjectProperty(map, i),
            jsgraph()->UndefinedConstant());
  }

  RelaxControls(node);
  a.FinishAndChange(node);
  return Changed(node);
}

Reduction JSCreateLowering::ReduceJSCreateLiteralRegExp(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCreateLiteralRegExp, node->opcode());
  CreateLiteralParameters const& p = CreateLiteralParametersOf(node->op());
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  Handle<Object> feedback(p.feedback().vector()->Get(p.feedback().slot()),
                          isolate());
  if (feedback->IsJSRegExp()) {
    Handle<JSRegExp> boilerplate = Handle<JSRegExp>::cast(feedback);
    Node* value = effect = AllocateLiteralRegExp(effect, control, boilerplate);
    ReplaceWithValue(node, value, effect, control);
    return Replace(value);
  }
  return NoChange();
}

Reduction JSCreateLowering::ReduceJSCreateFunctionContext(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCreateFunctionContext, node->opcode());
  const CreateFunctionContextParameters& parameters =
      CreateFunctionContextParametersOf(node->op());
  int slot_count = parameters.slot_count();
  ScopeType scope_type = parameters.scope_type();
  Node* const closure = NodeProperties::GetValueInput(node, 0);

  // Use inline allocation for function contexts up to a size limit.
  if (slot_count < kFunctionContextAllocationLimit) {
    // JSCreateFunctionContext[slot_count < limit]](fun)
    Node* effect = NodeProperties::GetEffectInput(node);
    Node* control = NodeProperties::GetControlInput(node);
    Node* context = NodeProperties::GetContextInput(node);
    Node* extension = jsgraph()->TheHoleConstant();
    AllocationBuilder a(jsgraph(), effect, control);
    STATIC_ASSERT(Context::MIN_CONTEXT_SLOTS == 4);  // Ensure fully covered.
    int context_length = slot_count + Context::MIN_CONTEXT_SLOTS;
    Handle<Map> map;
    switch (scope_type) {
      case EVAL_SCOPE:
        map = factory()->eval_context_map();
        break;
      case FUNCTION_SCOPE:
        map = factory()->function_context_map();
        break;
      default:
        UNREACHABLE();
    }
    a.AllocateContext(context_length, map);
    a.Store(AccessBuilder::ForContextSlot(Context::CLOSURE_INDEX), closure);
    a.Store(AccessBuilder::ForContextSlot(Context::PREVIOUS_INDEX), context);
    a.Store(AccessBuilder::ForContextSlot(Context::EXTENSION_INDEX), extension);
    a.Store(AccessBuilder::ForContextSlot(Context::NATIVE_CONTEXT_INDEX),
            jsgraph()->HeapConstant(native_context()));
    for (int i = Context::MIN_CONTEXT_SLOTS; i < context_length; ++i) {
      a.Store(AccessBuilder::ForContextSlot(i), jsgraph()->UndefinedConstant());
    }
    RelaxControls(node);
    a.FinishAndChange(node);
    return Changed(node);
  }

  return NoChange();
}

Reduction JSCreateLowering::ReduceJSCreateWithContext(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCreateWithContext, node->opcode());
  Handle<ScopeInfo> scope_info = ScopeInfoOf(node->op());
  Node* object = NodeProperties::GetValueInput(node, 0);
  Node* closure = NodeProperties::GetValueInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* context = NodeProperties::GetContextInput(node);

  AllocationBuilder aa(jsgraph(), effect, control);
  aa.Allocate(ContextExtension::kSize);
  aa.Store(AccessBuilder::ForMap(), factory()->context_extension_map());
  aa.Store(AccessBuilder::ForContextExtensionScopeInfo(), scope_info);
  aa.Store(AccessBuilder::ForContextExtensionExtension(), object);
  Node* extension = aa.Finish();

  AllocationBuilder a(jsgraph(), extension, control);
  STATIC_ASSERT(Context::MIN_CONTEXT_SLOTS == 4);  // Ensure fully covered.
  a.AllocateContext(Context::MIN_CONTEXT_SLOTS, factory()->with_context_map());
  a.Store(AccessBuilder::ForContextSlot(Context::CLOSURE_INDEX), closure);
  a.Store(AccessBuilder::ForContextSlot(Context::PREVIOUS_INDEX), context);
  a.Store(AccessBuilder::ForContextSlot(Context::EXTENSION_INDEX), extension);
  a.Store(AccessBuilder::ForContextSlot(Context::NATIVE_CONTEXT_INDEX),
          jsgraph()->HeapConstant(native_context()));
  RelaxControls(node);
  a.FinishAndChange(node);
  return Changed(node);
}

Reduction JSCreateLowering::ReduceJSCreateCatchContext(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCreateCatchContext, node->opcode());
  const CreateCatchContextParameters& parameters =
      CreateCatchContextParametersOf(node->op());
  Node* exception = NodeProperties::GetValueInput(node, 0);
  Node* closure = NodeProperties::GetValueInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* context = NodeProperties::GetContextInput(node);

  AllocationBuilder aa(jsgraph(), effect, control);
  aa.Allocate(ContextExtension::kSize);
  aa.Store(AccessBuilder::ForMap(), factory()->context_extension_map());
  aa.Store(AccessBuilder::ForContextExtensionScopeInfo(),
           parameters.scope_info());
  aa.Store(AccessBuilder::ForContextExtensionExtension(),
           parameters.catch_name());
  Node* extension = aa.Finish();

  AllocationBuilder a(jsgraph(), extension, control);
  STATIC_ASSERT(Context::MIN_CONTEXT_SLOTS == 4);  // Ensure fully covered.
  a.AllocateContext(Context::MIN_CONTEXT_SLOTS + 1,
                    factory()->catch_context_map());
  a.Store(AccessBuilder::ForContextSlot(Context::CLOSURE_INDEX), closure);
  a.Store(AccessBuilder::ForContextSlot(Context::PREVIOUS_INDEX), context);
  a.Store(AccessBuilder::ForContextSlot(Context::EXTENSION_INDEX), extension);
  a.Store(AccessBuilder::ForContextSlot(Context::NATIVE_CONTEXT_INDEX),
          jsgraph()->HeapConstant(native_context()));
  a.Store(AccessBuilder::ForContextSlot(Context::THROWN_OBJECT_INDEX),
          exception);
  RelaxControls(node);
  a.FinishAndChange(node);
  return Changed(node);
}

Reduction JSCreateLowering::ReduceJSCreateBlockContext(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCreateBlockContext, node->opcode());
  Handle<ScopeInfo> scope_info = ScopeInfoOf(node->op());
  int const context_length = scope_info->ContextLength();
  Node* const closure = NodeProperties::GetValueInput(node, 0);

  // Use inline allocation for block contexts up to a size limit.
  if (context_length < kBlockContextAllocationLimit) {
    // JSCreateBlockContext[scope[length < limit]](fun)
    Node* effect = NodeProperties::GetEffectInput(node);
    Node* control = NodeProperties::GetControlInput(node);
    Node* context = NodeProperties::GetContextInput(node);
    Node* extension = jsgraph()->Constant(scope_info);

    AllocationBuilder a(jsgraph(), effect, control);
    STATIC_ASSERT(Context::MIN_CONTEXT_SLOTS == 4);  // Ensure fully covered.
    a.AllocateContext(context_length, factory()->block_context_map());
    a.Store(AccessBuilder::ForContextSlot(Context::CLOSURE_INDEX), closure);
    a.Store(AccessBuilder::ForContextSlot(Context::PREVIOUS_INDEX), context);
    a.Store(AccessBuilder::ForContextSlot(Context::EXTENSION_INDEX), extension);
    a.Store(AccessBuilder::ForContextSlot(Context::NATIVE_CONTEXT_INDEX),
            jsgraph()->HeapConstant(native_context()));
    for (int i = Context::MIN_CONTEXT_SLOTS; i < context_length; ++i) {
      a.Store(AccessBuilder::ForContextSlot(i), jsgraph()->UndefinedConstant());
    }
    RelaxControls(node);
    a.FinishAndChange(node);
    return Changed(node);
  }

  return NoChange();
}

// Helper that allocates a FixedArray holding argument values recorded in the
// given {frame_state}. Serves as backing store for JSCreateArguments nodes.
Node* JSCreateLowering::AllocateArguments(Node* effect, Node* control,
                                          Node* frame_state) {
  FrameStateInfo state_info = FrameStateInfoOf(frame_state->op());
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
    DCHECK_NOT_NULL((*parameters_it).node);
    a.Store(AccessBuilder::ForFixedArraySlot(i), (*parameters_it).node);
  }
  return a.Finish();
}

// Helper that allocates a FixedArray holding argument values recorded in the
// given {frame_state}. Serves as backing store for JSCreateArguments nodes.
Node* JSCreateLowering::AllocateRestArguments(Node* effect, Node* control,
                                              Node* frame_state,
                                              int start_index) {
  FrameStateInfo state_info = FrameStateInfoOf(frame_state->op());
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
    DCHECK_NOT_NULL((*parameters_it).node);
    a.Store(AccessBuilder::ForFixedArraySlot(i), (*parameters_it).node);
  }
  return a.Finish();
}

// Helper that allocates a FixedArray serving as a parameter map for values
// recorded in the given {frame_state}. Some elements map to slots within the
// given {context}. Serves as backing store for JSCreateArguments nodes.
Node* JSCreateLowering::AllocateAliasedArguments(
    Node* effect, Node* control, Node* frame_state, Node* context,
    Handle<SharedFunctionInfo> shared, bool* has_aliased_arguments) {
  FrameStateInfo state_info = FrameStateInfoOf(frame_state->op());
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
  auto parameters_it = ++parameters_access.begin();

  // The unmapped argument values recorded in the frame state are stored yet
  // another indirection away and then linked into the parameter map below,
  // whereas mapped argument values are replaced with a hole instead.
  AllocationBuilder aa(jsgraph(), effect, control);
  aa.AllocateArray(argument_count, factory()->fixed_array_map());
  for (int i = 0; i < mapped_count; ++i, ++parameters_it) {
    aa.Store(AccessBuilder::ForFixedArraySlot(i), jsgraph()->TheHoleConstant());
  }
  for (int i = mapped_count; i < argument_count; ++i, ++parameters_it) {
    DCHECK_NOT_NULL((*parameters_it).node);
    aa.Store(AccessBuilder::ForFixedArraySlot(i), (*parameters_it).node);
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

// Helper that allocates a FixedArray serving as a parameter map for values
// unknown at compile-time, the true {arguments_length} and {arguments_frame}
// values can only be determined dynamically at run-time and are provided.
// Serves as backing store for JSCreateArguments nodes.
Node* JSCreateLowering::AllocateAliasedArguments(
    Node* effect, Node* control, Node* context, Node* arguments_frame,
    Node* arguments_length, Handle<SharedFunctionInfo> shared,
    bool* has_aliased_arguments) {
  // If there is no aliasing, the arguments object elements are not
  // special in any way, we can just return an unmapped backing store.
  int parameter_count = shared->internal_formal_parameter_count();
  if (parameter_count == 0) {
    return graph()->NewNode(simplified()->NewArgumentsElements(0),
                            arguments_frame, arguments_length, effect);
  }

  // From here on we are going to allocate a mapped (aka. aliased) elements
  // backing store. We do not statically know how many arguments exist, but
  // dynamically selecting the hole for some of the "mapped" elements allows
  // using a static shape for the parameter map.
  int mapped_count = parameter_count;
  *has_aliased_arguments = true;

  // The unmapped argument values are stored yet another indirection away and
  // then linked into the parameter map below, whereas mapped argument values
  // (i.e. the first {mapped_count} elements) are replaced with a hole instead.
  Node* arguments =
      graph()->NewNode(simplified()->NewArgumentsElements(mapped_count),
                       arguments_frame, arguments_length, effect);

  // Actually allocate the backing store.
  AllocationBuilder a(jsgraph(), arguments, control);
  a.AllocateArray(mapped_count + 2, factory()->sloppy_arguments_elements_map());
  a.Store(AccessBuilder::ForFixedArraySlot(0), context);
  a.Store(AccessBuilder::ForFixedArraySlot(1), arguments);
  for (int i = 0; i < mapped_count; ++i) {
    int idx = Context::MIN_CONTEXT_SLOTS + parameter_count - 1 - i;
    Node* value = graph()->NewNode(
        common()->Select(MachineRepresentation::kTagged),
        graph()->NewNode(simplified()->NumberLessThan(), jsgraph()->Constant(i),
                         arguments_length),
        jsgraph()->Constant(idx), jsgraph()->TheHoleConstant());
    a.Store(AccessBuilder::ForFixedArraySlot(i + 2), value);
  }
  return a.Finish();
}

Node* JSCreateLowering::AllocateElements(Node* effect, Node* control,
                                         ElementsKind elements_kind,
                                         int capacity,
                                         PretenureFlag pretenure) {
  DCHECK_LE(1, capacity);
  DCHECK_LE(capacity, JSArray::kInitialMaxFastElementArray);

  Handle<Map> elements_map = IsDoubleElementsKind(elements_kind)
                                 ? factory()->fixed_double_array_map()
                                 : factory()->fixed_array_map();
  ElementAccess access = IsDoubleElementsKind(elements_kind)
                             ? AccessBuilder::ForFixedDoubleArrayElement()
                             : AccessBuilder::ForFixedArrayElement();
  Node* value = jsgraph()->TheHoleConstant();

  // Actually allocate the backing store.
  AllocationBuilder a(jsgraph(), effect, control);
  a.AllocateArray(capacity, elements_map, pretenure);
  for (int i = 0; i < capacity; ++i) {
    Node* index = jsgraph()->Constant(i);
    a.Store(access, index, value);
  }
  return a.Finish();
}

Node* JSCreateLowering::AllocateElements(Node* effect, Node* control,
                                         ElementsKind elements_kind,
                                         std::vector<Node*> const& values,
                                         PretenureFlag pretenure) {
  int const capacity = static_cast<int>(values.size());
  DCHECK_LE(1, capacity);
  DCHECK_LE(capacity, JSArray::kInitialMaxFastElementArray);

  Handle<Map> elements_map = IsDoubleElementsKind(elements_kind)
                                 ? factory()->fixed_double_array_map()
                                 : factory()->fixed_array_map();
  ElementAccess access = IsDoubleElementsKind(elements_kind)
                             ? AccessBuilder::ForFixedDoubleArrayElement()
                             : AccessBuilder::ForFixedArrayElement();

  // Actually allocate the backing store.
  AllocationBuilder a(jsgraph(), effect, control);
  a.AllocateArray(capacity, elements_map, pretenure);
  for (int i = 0; i < capacity; ++i) {
    Node* index = jsgraph()->Constant(i);
    a.Store(access, index, values[i]);
  }
  return a.Finish();
}

Node* JSCreateLowering::AllocateFastLiteral(
    Node* effect, Node* control, Handle<JSObject> boilerplate,
    AllocationSiteUsageContext* site_context) {
  Handle<AllocationSite> current_site(*site_context->current(), isolate());
  dependencies()->AssumeTransitionStable(current_site);

  PretenureFlag pretenure = NOT_TENURED;
  if (FLAG_allocation_site_pretenuring) {
    Handle<AllocationSite> top_site(*site_context->top(), isolate());
    pretenure = top_site->GetPretenureMode();
    if (current_site.is_identical_to(top_site)) {
      // We install a dependency for pretenuring only on the outermost literal.
      dependencies()->AssumeTenuringDecision(top_site);
    }
  }

  // Setup the properties backing store.
  Node* properties = jsgraph()->EmptyFixedArrayConstant();

  // Compute the in-object properties to store first (might have effects).
  Handle<Map> boilerplate_map(boilerplate->map(), isolate());
  ZoneVector<std::pair<FieldAccess, Node*>> inobject_fields(zone());
  inobject_fields.reserve(boilerplate_map->GetInObjectProperties());
  int const boilerplate_nof = boilerplate_map->NumberOfOwnDescriptors();
  for (int i = 0; i < boilerplate_nof; ++i) {
    PropertyDetails const property_details =
        boilerplate_map->instance_descriptors()->GetDetails(i);
    if (property_details.location() != kField) continue;
    DCHECK_EQ(kData, property_details.kind());
    Handle<Name> property_name(
        boilerplate_map->instance_descriptors()->GetKey(i), isolate());
    FieldIndex index = FieldIndex::ForDescriptor(*boilerplate_map, i);
    FieldAccess access = {kTaggedBase,      index.offset(),
                          property_name,    MaybeHandle<Map>(),
                          Type::Any(),      MachineType::AnyTagged(),
                          kFullWriteBarrier};
    Node* value;
    if (boilerplate->IsUnboxedDoubleField(index)) {
      access.machine_type = MachineType::Float64();
      access.type = Type::Number();
      value = jsgraph()->Constant(boilerplate->RawFastDoublePropertyAt(index));
    } else {
      Handle<Object> boilerplate_value(boilerplate->RawFastPropertyAt(index),
                                       isolate());
      if (boilerplate_value->IsJSObject()) {
        Handle<JSObject> boilerplate_object =
            Handle<JSObject>::cast(boilerplate_value);
        Handle<AllocationSite> current_site = site_context->EnterNewScope();
        value = effect = AllocateFastLiteral(effect, control,
                                             boilerplate_object, site_context);
        site_context->ExitScope(current_site, boilerplate_object);
      } else if (property_details.representation().IsDouble()) {
        double number = Handle<HeapNumber>::cast(boilerplate_value)->value();
        // Allocate a mutable HeapNumber box and store the value into it.
        AllocationBuilder builder(jsgraph(), effect, control);
        builder.Allocate(HeapNumber::kSize, pretenure);
        builder.Store(AccessBuilder::ForMap(),
                      factory()->mutable_heap_number_map());
        builder.Store(AccessBuilder::ForHeapNumberValue(),
                      jsgraph()->Constant(number));
        value = effect = builder.Finish();
      } else if (property_details.representation().IsSmi()) {
        // Ensure that value is stored as smi.
        value = boilerplate_value->IsUninitialized(isolate())
                    ? jsgraph()->ZeroConstant()
                    : jsgraph()->Constant(boilerplate_value);
      } else {
        value = jsgraph()->Constant(boilerplate_value);
      }
    }
    inobject_fields.push_back(std::make_pair(access, value));
  }

  // Fill slack at the end of the boilerplate object with filler maps.
  int const boilerplate_length = boilerplate_map->GetInObjectProperties();
  for (int index = static_cast<int>(inobject_fields.size());
       index < boilerplate_length; ++index) {
    FieldAccess access =
        AccessBuilder::ForJSObjectInObjectProperty(boilerplate_map, index);
    Node* value = jsgraph()->HeapConstant(factory()->one_pointer_filler_map());
    inobject_fields.push_back(std::make_pair(access, value));
  }

  // Setup the elements backing store.
  Node* elements = AllocateFastLiteralElements(effect, control, boilerplate,
                                               pretenure, site_context);
  if (elements->op()->EffectOutputCount() > 0) effect = elements;

  // Actually allocate and initialize the object.
  AllocationBuilder builder(jsgraph(), effect, control);
  builder.Allocate(boilerplate_map->instance_size(), pretenure,
                   Type::For(boilerplate_map));
  builder.Store(AccessBuilder::ForMap(), boilerplate_map);
  builder.Store(AccessBuilder::ForJSObjectPropertiesOrHash(), properties);
  builder.Store(AccessBuilder::ForJSObjectElements(), elements);
  if (boilerplate_map->IsJSArrayMap()) {
    Handle<JSArray> boilerplate_array = Handle<JSArray>::cast(boilerplate);
    builder.Store(
        AccessBuilder::ForJSArrayLength(boilerplate_array->GetElementsKind()),
        handle(boilerplate_array->length(), isolate()));
  }
  for (auto const& inobject_field : inobject_fields) {
    builder.Store(inobject_field.first, inobject_field.second);
  }
  return builder.Finish();
}

Node* JSCreateLowering::AllocateFastLiteralElements(
    Node* effect, Node* control, Handle<JSObject> boilerplate,
    PretenureFlag pretenure, AllocationSiteUsageContext* site_context) {
  Handle<FixedArrayBase> boilerplate_elements(boilerplate->elements(),
                                              isolate());

  // Empty or copy-on-write elements just store a constant.
  if (boilerplate_elements->length() == 0 ||
      boilerplate_elements->map() == isolate()->heap()->fixed_cow_array_map()) {
    if (pretenure == TENURED &&
        isolate()->heap()->InNewSpace(*boilerplate_elements)) {
      // If we would like to pretenure a fixed cow array, we must ensure that
      // the array is already in old space, otherwise we'll create too many
      // old-to-new-space pointers (overflowing the store buffer).
      boilerplate_elements = Handle<FixedArrayBase>(
          isolate()->factory()->CopyAndTenureFixedCOWArray(
              Handle<FixedArray>::cast(boilerplate_elements)));
      boilerplate->set_elements(*boilerplate_elements);
    }
    return jsgraph()->HeapConstant(boilerplate_elements);
  }

  // Compute the elements to store first (might have effects).
  int const elements_length = boilerplate_elements->length();
  Handle<Map> elements_map(boilerplate_elements->map(), isolate());
  ZoneVector<Node*> elements_values(elements_length, zone());
  if (elements_map->instance_type() == FIXED_DOUBLE_ARRAY_TYPE) {
    Handle<FixedDoubleArray> elements =
        Handle<FixedDoubleArray>::cast(boilerplate_elements);
    for (int i = 0; i < elements_length; ++i) {
      if (elements->is_the_hole(i)) {
        elements_values[i] = jsgraph()->TheHoleConstant();
      } else {
        elements_values[i] = jsgraph()->Constant(elements->get_scalar(i));
      }
    }
  } else {
    Handle<FixedArray> elements =
        Handle<FixedArray>::cast(boilerplate_elements);
    for (int i = 0; i < elements_length; ++i) {
      if (elements->is_the_hole(isolate(), i)) {
        elements_values[i] = jsgraph()->TheHoleConstant();
      } else {
        Handle<Object> element_value(elements->get(i), isolate());
        if (element_value->IsJSObject()) {
          Handle<JSObject> boilerplate_object =
              Handle<JSObject>::cast(element_value);
          Handle<AllocationSite> current_site = site_context->EnterNewScope();
          elements_values[i] = effect = AllocateFastLiteral(
              effect, control, boilerplate_object, site_context);
          site_context->ExitScope(current_site, boilerplate_object);
        } else {
          elements_values[i] = jsgraph()->Constant(element_value);
        }
      }
    }
  }

  // Allocate the backing store array and store the elements.
  AllocationBuilder builder(jsgraph(), effect, control);
  builder.AllocateArray(elements_length, elements_map, pretenure);
  ElementAccess const access =
      (elements_map->instance_type() == FIXED_DOUBLE_ARRAY_TYPE)
          ? AccessBuilder::ForFixedDoubleArrayElement()
          : AccessBuilder::ForFixedArrayElement();
  for (int i = 0; i < elements_length; ++i) {
    builder.Store(access, jsgraph()->Constant(i), elements_values[i]);
  }
  return builder.Finish();
}

Node* JSCreateLowering::AllocateLiteralRegExp(Node* effect, Node* control,
                                              Handle<JSRegExp> boilerplate) {
  Handle<Map> boilerplate_map(boilerplate->map(), isolate());

  // Sanity check that JSRegExp object layout hasn't changed.
  STATIC_ASSERT(JSRegExp::kDataOffset == JSObject::kHeaderSize);
  STATIC_ASSERT(JSRegExp::kSourceOffset ==
                JSRegExp::kDataOffset + kPointerSize);
  STATIC_ASSERT(JSRegExp::kFlagsOffset ==
                JSRegExp::kSourceOffset + kPointerSize);
  STATIC_ASSERT(JSRegExp::kSize == JSRegExp::kFlagsOffset + kPointerSize);
  STATIC_ASSERT(JSRegExp::kLastIndexOffset == JSRegExp::kSize);
  STATIC_ASSERT(JSRegExp::kInObjectFieldCount == 1);  // LastIndex.

  const PretenureFlag pretenure = NOT_TENURED;
  const int size =
      JSRegExp::kSize + JSRegExp::kInObjectFieldCount * kPointerSize;

  AllocationBuilder builder(jsgraph(), effect, control);
  builder.Allocate(size, pretenure, Type::For(boilerplate_map));
  builder.Store(AccessBuilder::ForMap(), boilerplate_map);
  builder.Store(AccessBuilder::ForJSObjectPropertiesOrHash(),
                handle(boilerplate->raw_properties_or_hash(), isolate()));
  builder.Store(AccessBuilder::ForJSObjectElements(),
                handle(boilerplate->elements(), isolate()));

  builder.Store(AccessBuilder::ForJSRegExpData(),
                handle(boilerplate->data(), isolate()));
  builder.Store(AccessBuilder::ForJSRegExpSource(),
                handle(boilerplate->source(), isolate()));
  builder.Store(AccessBuilder::ForJSRegExpFlags(),
                handle(boilerplate->flags(), isolate()));
  builder.Store(AccessBuilder::ForJSRegExpLastIndex(),
                handle(boilerplate->last_index(), isolate()));

  return builder.Finish();
}

Factory* JSCreateLowering::factory() const { return isolate()->factory(); }

Graph* JSCreateLowering::graph() const { return jsgraph()->graph(); }

Isolate* JSCreateLowering::isolate() const { return jsgraph()->isolate(); }

CommonOperatorBuilder* JSCreateLowering::common() const {
  return jsgraph()->common();
}

SimplifiedOperatorBuilder* JSCreateLowering::simplified() const {
  return jsgraph()->simplified();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
