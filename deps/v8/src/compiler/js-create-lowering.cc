// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-create-lowering.h"

#include "src/allocation-site-scopes.h"
#include "src/code-factory.h"
#include "src/compilation-dependencies.h"
#include "src/compiler/access-builder.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/js-operator.h"
#include "src/compiler/linkage.h"
#include "src/compiler/node.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/operator-properties.h"
#include "src/compiler/simplified-operator.h"
#include "src/compiler/state-values-utils.h"

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
    effect_ = graph()->NewNode(
        common()->BeginRegion(RegionObservability::kNotObservable), effect_);
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

// Retrieves the frame state holding actual argument values.
Node* GetArgumentsFrameState(Node* frame_state) {
  Node* const outer_state = NodeProperties::GetFrameStateInput(frame_state);
  FrameStateInfo outer_state_info = OpParameter<FrameStateInfo>(outer_state);
  return outer_state_info.type() == FrameStateType::kArgumentsAdaptor
             ? outer_state
             : frame_state;
}

// Checks whether allocation using the given target and new.target can be
// inlined.
bool IsAllocationInlineable(Handle<JSFunction> target,
                            Handle<JSFunction> new_target) {
  return new_target->has_initial_map() &&
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
    if (boilerplate->HasFastSmiOrObjectElements()) {
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
    } else if (!boilerplate->HasFastDoubleElements()) {
      return false;
    }
  }

  // TODO(turbofan): Do we want to support out-of-object properties?
  Handle<FixedArray> properties(boilerplate->properties(), isolate);
  if (properties->length() > 0) return false;

  // Check the in-object properties.
  Handle<DescriptorArray> descriptors(
      boilerplate->map()->instance_descriptors(), isolate);
  int limit = boilerplate->map()->NumberOfOwnDescriptors();
  for (int i = 0; i < limit; i++) {
    PropertyDetails details = descriptors->GetDetails(i);
    if (details.type() != DATA) continue;
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
// graphs to be considered for fast deep-copying.
const int kMaxFastLiteralDepth = 3;
const int kMaxFastLiteralProperties = 8;

}  // namespace

Reduction JSCreateLowering::Reduce(Node* node) {
  switch (node->opcode()) {
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
    case IrOpcode::kJSCreateLiteralObject:
      return ReduceJSCreateLiteral(node);
    case IrOpcode::kJSCreateFunctionContext:
      return ReduceJSCreateFunctionContext(node);
    case IrOpcode::kJSCreateWithContext:
      return ReduceJSCreateWithContext(node);
    case IrOpcode::kJSCreateCatchContext:
      return ReduceJSCreateCatchContext(node);
    case IrOpcode::kJSCreateBlockContext:
      return ReduceJSCreateBlockContext(node);
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
  // Extract constructor and original constructor function.
  if (target_type->IsConstant() &&
      new_target_type->IsConstant() &&
      new_target_type->AsConstant()->Value()->IsJSFunction()) {
    Handle<JSFunction> constructor =
        Handle<JSFunction>::cast(target_type->AsConstant()->Value());
    Handle<JSFunction> original_constructor =
        Handle<JSFunction>::cast(new_target_type->AsConstant()->Value());
    DCHECK(constructor->IsConstructor());
    DCHECK(original_constructor->IsConstructor());

    // Check if we can inline the allocation.
    if (IsAllocationInlineable(constructor, original_constructor)) {
      // Force completion of inobject slack tracking before
      // generating code to finalize the instance size.
      original_constructor->CompleteInobjectSlackTrackingIfActive();

      // Compute instance size from initial map of {original_constructor}.
      Handle<Map> initial_map(original_constructor->initial_map(), isolate());
      int const instance_size = initial_map->instance_size();

      // Add a dependency on the {initial_map} to make sure that this code is
      // deoptimized whenever the {initial_map} of the {original_constructor}
      // changes.
      dependencies()->AssumeInitialMapCantChange(initial_map);

      // Emit code to allocate the JSObject instance for the
      // {original_constructor}.
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

Reduction JSCreateLowering::ReduceJSCreateArguments(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCreateArguments, node->opcode());
  CreateArgumentsType type = CreateArgumentsTypeOf(node->op());
  Node* const frame_state = NodeProperties::GetFrameStateInput(node);
  Node* const outer_state = frame_state->InputAt(kFrameStateOuterStateInput);
  Node* const control = graph()->start();
  FrameStateInfo state_info = OpParameter<FrameStateInfo>(frame_state);

  // Use the ArgumentsAccessStub for materializing both mapped and unmapped
  // arguments object, but only for non-inlined (i.e. outermost) frames.
  if (outer_state->opcode() != IrOpcode::kFrameState) {
    switch (type) {
      case CreateArgumentsType::kMappedArguments: {
        // TODO(mstarzinger): Duplicate parameters are not handled yet.
        Handle<SharedFunctionInfo> shared_info;
        if (!state_info.shared_info().ToHandle(&shared_info) ||
            shared_info->has_duplicate_parameters()) {
          return NoChange();
        }
        Callable callable = CodeFactory::FastNewSloppyArguments(isolate());
        Operator::Properties properties = node->op()->properties();
        CallDescriptor* desc = Linkage::GetStubCallDescriptor(
            isolate(), graph()->zone(), callable.descriptor(), 0,
            CallDescriptor::kNoFlags, properties);
        const Operator* new_op = common()->Call(desc);
        Node* stub_code = jsgraph()->HeapConstant(callable.code());
        node->InsertInput(graph()->zone(), 0, stub_code);
        node->RemoveInput(3);  // Remove the frame state.
        NodeProperties::ChangeOp(node, new_op);
        return Changed(node);
      }
      case CreateArgumentsType::kUnmappedArguments: {
        Callable callable = CodeFactory::FastNewStrictArguments(isolate());
        Operator::Properties properties = node->op()->properties();
        CallDescriptor* desc = Linkage::GetStubCallDescriptor(
            isolate(), graph()->zone(), callable.descriptor(), 0,
            CallDescriptor::kNeedsFrameState, properties);
        const Operator* new_op = common()->Call(desc);
        Node* stub_code = jsgraph()->HeapConstant(callable.code());
        node->InsertInput(graph()->zone(), 0, stub_code);
        NodeProperties::ChangeOp(node, new_op);
        return Changed(node);
      }
      case CreateArgumentsType::kRestParameter: {
        Callable callable = CodeFactory::FastNewRestParameter(isolate());
        Operator::Properties properties = node->op()->properties();
        CallDescriptor* desc = Linkage::GetStubCallDescriptor(
            isolate(), graph()->zone(), callable.descriptor(), 0,
            CallDescriptor::kNeedsFrameState, properties);
        const Operator* new_op = common()->Call(desc);
        Node* stub_code = jsgraph()->HeapConstant(callable.code());
        node->InsertInput(graph()->zone(), 0, stub_code);
        NodeProperties::ChangeOp(node, new_op);
        return Changed(node);
      }
    }
    UNREACHABLE();
  } else if (outer_state->opcode() == IrOpcode::kFrameState) {
    // Use inline allocation for all mapped arguments objects within inlined
    // (i.e. non-outermost) frames, independent of the object size.
    if (type == CreateArgumentsType::kMappedArguments) {
      Handle<SharedFunctionInfo> shared;
      if (!state_info.shared_info().ToHandle(&shared)) return NoChange();
      Node* const callee = NodeProperties::GetValueInput(node, 0);
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
      STATIC_ASSERT(JSSloppyArgumentsObject::kSize == 5 * kPointerSize);
      a.Allocate(JSSloppyArgumentsObject::kSize);
      a.Store(AccessBuilder::ForMap(), load_arguments_map);
      a.Store(AccessBuilder::ForJSObjectProperties(), properties);
      a.Store(AccessBuilder::ForJSObjectElements(), elements);
      a.Store(AccessBuilder::ForArgumentsLength(), jsgraph()->Constant(length));
      a.Store(AccessBuilder::ForArgumentsCallee(), callee);
      RelaxControls(node);
      a.FinishAndChange(node);
      return Changed(node);
    } else if (type == CreateArgumentsType::kUnmappedArguments) {
      // Use inline allocation for all unmapped arguments objects within inlined
      // (i.e. non-outermost) frames, independent of the object size.
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
      STATIC_ASSERT(JSStrictArgumentsObject::kSize == 4 * kPointerSize);
      a.Allocate(JSStrictArgumentsObject::kSize);
      a.Store(AccessBuilder::ForMap(), load_arguments_map);
      a.Store(AccessBuilder::ForJSObjectProperties(), properties);
      a.Store(AccessBuilder::ForJSObjectElements(), elements);
      a.Store(AccessBuilder::ForArgumentsLength(), jsgraph()->Constant(length));
      RelaxControls(node);
      a.FinishAndChange(node);
      return Changed(node);
    } else if (type == CreateArgumentsType::kRestParameter) {
      Handle<SharedFunctionInfo> shared;
      if (!state_info.shared_info().ToHandle(&shared)) return NoChange();
      int start_index = shared->internal_formal_parameter_count();
      // Use inline allocation for all unmapped arguments objects within inlined
      // (i.e. non-outermost) frames, independent of the object size.
      Node* const context = NodeProperties::GetContextInput(node);
      Node* effect = NodeProperties::GetEffectInput(node);
      // Choose the correct frame state and frame state info depending on
      // whether there conceptually is an arguments adaptor frame in the call
      // chain.
      Node* const args_state = GetArgumentsFrameState(frame_state);
      FrameStateInfo args_state_info = OpParameter<FrameStateInfo>(args_state);
      // Prepare element backing store to be used by the rest array.
      Node* const elements =
          AllocateRestArguments(effect, control, args_state, start_index);
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
      int length = std::max(0, argument_count - start_index);
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

Reduction JSCreateLowering::ReduceNewArray(Node* node, Node* length,
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
  if (NodeProperties::GetType(length)->Max() > 0) {
    elements_kind = GetHoleyElementsKind(elements_kind);
  }
  dependencies()->AssumeTenuringDecision(site);
  dependencies()->AssumeTransitionStable(site);

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

Reduction JSCreateLowering::ReduceNewArrayToStubCall(
    Node* node, Handle<AllocationSite> site) {
  CreateArrayParameters const& p = CreateArrayParametersOf(node->op());
  int const arity = static_cast<int>(p.arity());

  ElementsKind elements_kind = site->GetElementsKind();
  AllocationSiteOverrideMode override_mode =
      (AllocationSite::GetMode(elements_kind) == TRACK_ALLOCATION_SITE)
          ? DISABLE_ALLOCATION_SITES
          : DONT_OVERRIDE;

  if (arity == 0) {
    ArrayNoArgumentConstructorStub stub(isolate(), elements_kind,
                                        override_mode);
    CallDescriptor* desc = Linkage::GetStubCallDescriptor(
        isolate(), graph()->zone(), stub.GetCallInterfaceDescriptor(), 1,
        CallDescriptor::kNeedsFrameState);
    node->ReplaceInput(0, jsgraph()->HeapConstant(stub.GetCode()));
    node->InsertInput(graph()->zone(), 2, jsgraph()->HeapConstant(site));
    node->InsertInput(graph()->zone(), 3, jsgraph()->Int32Constant(0));
    node->InsertInput(graph()->zone(), 4, jsgraph()->UndefinedConstant());
    NodeProperties::ChangeOp(node, common()->Call(desc));
    return Changed(node);
  } else if (arity == 1) {
    AllocationSiteOverrideMode override_mode =
        (AllocationSite::GetMode(elements_kind) == TRACK_ALLOCATION_SITE)
            ? DISABLE_ALLOCATION_SITES
            : DONT_OVERRIDE;

    if (IsHoleyElementsKind(elements_kind)) {
      ArraySingleArgumentConstructorStub stub(isolate(), elements_kind,
                                              override_mode);
      CallDescriptor* desc = Linkage::GetStubCallDescriptor(
          isolate(), graph()->zone(), stub.GetCallInterfaceDescriptor(), 2,
          CallDescriptor::kNeedsFrameState);
      node->ReplaceInput(0, jsgraph()->HeapConstant(stub.GetCode()));
      node->InsertInput(graph()->zone(), 2, jsgraph()->HeapConstant(site));
      node->InsertInput(graph()->zone(), 3, jsgraph()->Int32Constant(1));
      node->InsertInput(graph()->zone(), 4, jsgraph()->UndefinedConstant());
      NodeProperties::ChangeOp(node, common()->Call(desc));
      return Changed(node);
    }

    Node* effect = NodeProperties::GetEffectInput(node);
    Node* control = NodeProperties::GetControlInput(node);
    Node* length = NodeProperties::GetValueInput(node, 2);
    Node* equal = graph()->NewNode(simplified()->ReferenceEqual(), length,
                                   jsgraph()->ZeroConstant());

    Node* branch =
        graph()->NewNode(common()->Branch(BranchHint::kFalse), equal, control);
    Node* call_holey;
    Node* call_packed;
    Node* if_success_packed;
    Node* if_success_holey;
    Node* context = NodeProperties::GetContextInput(node);
    Node* frame_state = NodeProperties::GetFrameStateInput(node);
    Node* if_equal = graph()->NewNode(common()->IfTrue(), branch);
    {
      ArraySingleArgumentConstructorStub stub(isolate(), elements_kind,
                                              override_mode);
      CallDescriptor* desc = Linkage::GetStubCallDescriptor(
          isolate(), graph()->zone(), stub.GetCallInterfaceDescriptor(), 2,
          CallDescriptor::kNeedsFrameState);

      Node* inputs[] = {jsgraph()->HeapConstant(stub.GetCode()),
                        node->InputAt(1),
                        jsgraph()->HeapConstant(site),
                        jsgraph()->Int32Constant(1),
                        jsgraph()->UndefinedConstant(),
                        length,
                        context,
                        frame_state,
                        effect,
                        if_equal};

      call_holey =
          graph()->NewNode(common()->Call(desc), arraysize(inputs), inputs);
      if_success_holey = graph()->NewNode(common()->IfSuccess(), call_holey);
    }
    Node* if_not_equal = graph()->NewNode(common()->IfFalse(), branch);
    {
      // Require elements kind to "go holey."
      ArraySingleArgumentConstructorStub stub(
          isolate(), GetHoleyElementsKind(elements_kind), override_mode);
      CallDescriptor* desc = Linkage::GetStubCallDescriptor(
          isolate(), graph()->zone(), stub.GetCallInterfaceDescriptor(), 2,
          CallDescriptor::kNeedsFrameState);

      Node* inputs[] = {jsgraph()->HeapConstant(stub.GetCode()),
                        node->InputAt(1),
                        jsgraph()->HeapConstant(site),
                        jsgraph()->Int32Constant(1),
                        jsgraph()->UndefinedConstant(),
                        length,
                        context,
                        frame_state,
                        effect,
                        if_not_equal};

      call_packed =
          graph()->NewNode(common()->Call(desc), arraysize(inputs), inputs);
      if_success_packed = graph()->NewNode(common()->IfSuccess(), call_packed);
    }
    Node* merge = graph()->NewNode(common()->Merge(2), if_success_holey,
                                   if_success_packed);
    Node* effect_phi = graph()->NewNode(common()->EffectPhi(2), call_holey,
                                        call_packed, merge);
    Node* phi =
        graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                         call_holey, call_packed, merge);

    ReplaceWithValue(node, phi, effect_phi, merge);
    return Changed(node);
  }

  DCHECK(arity > 1);
  ArrayNArgumentsConstructorStub stub(isolate());
  CallDescriptor* desc = Linkage::GetStubCallDescriptor(
      isolate(), graph()->zone(), stub.GetCallInterfaceDescriptor(), arity + 1,
      CallDescriptor::kNeedsFrameState);
  node->ReplaceInput(0, jsgraph()->HeapConstant(stub.GetCode()));
  node->InsertInput(graph()->zone(), 2, jsgraph()->HeapConstant(site));
  node->InsertInput(graph()->zone(), 3, jsgraph()->Int32Constant(arity));
  node->InsertInput(graph()->zone(), 4, jsgraph()->UndefinedConstant());
  NodeProperties::ChangeOp(node, common()->Call(desc));
  return Changed(node);
}

Reduction JSCreateLowering::ReduceJSCreateArray(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCreateArray, node->opcode());
  CreateArrayParameters const& p = CreateArrayParametersOf(node->op());
  Node* target = NodeProperties::GetValueInput(node, 0);
  Node* new_target = NodeProperties::GetValueInput(node, 1);

  // TODO(mstarzinger): Array constructor can throw. Hook up exceptional edges.
  if (NodeProperties::IsExceptionalCall(node)) return NoChange();

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
      if (length_type->Is(Type::SignedSmall()) && length_type->Min() >= 0 &&
          length_type->Max() <= kElementLoopUnrollLimit &&
          length_type->Min() == length_type->Max()) {
        int capacity = static_cast<int>(length_type->Max());
        return ReduceNewArray(node, length, capacity, site);
      }
    }
  }

  return ReduceNewArrayToStubCall(node, site);
}

Reduction JSCreateLowering::ReduceJSCreateClosure(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCreateClosure, node->opcode());
  CreateClosureParameters const& p = CreateClosureParametersOf(node->op());
  Handle<SharedFunctionInfo> shared = p.shared_info();

  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* context = NodeProperties::GetContextInput(node);
  Node* native_context = effect = graph()->NewNode(
      javascript()->LoadContext(0, Context::NATIVE_CONTEXT_INDEX, true),
      context, context, effect);
  int function_map_index =
      Context::FunctionMapIndex(shared->language_mode(), shared->kind());
  Node* function_map = effect =
      graph()->NewNode(javascript()->LoadContext(0, function_map_index, true),
                       native_context, native_context, effect);
  // Note that it is only safe to embed the raw entry point of the compile
  // lazy stub into the code, because that stub is immortal and immovable.
  Node* compile_entry = jsgraph()->IntPtrConstant(reinterpret_cast<intptr_t>(
      jsgraph()->isolate()->builtins()->CompileLazy()->entry()));
  Node* empty_fixed_array = jsgraph()->EmptyFixedArrayConstant();
  Node* empty_literals_array = jsgraph()->EmptyLiteralsArrayConstant();
  Node* the_hole = jsgraph()->TheHoleConstant();
  Node* undefined = jsgraph()->UndefinedConstant();
  AllocationBuilder a(jsgraph(), effect, control);
  STATIC_ASSERT(JSFunction::kSize == 9 * kPointerSize);
  a.Allocate(JSFunction::kSize, p.pretenure());
  a.Store(AccessBuilder::ForMap(), function_map);
  a.Store(AccessBuilder::ForJSObjectProperties(), empty_fixed_array);
  a.Store(AccessBuilder::ForJSObjectElements(), empty_fixed_array);
  a.Store(AccessBuilder::ForJSFunctionLiterals(), empty_literals_array);
  a.Store(AccessBuilder::ForJSFunctionPrototypeOrInitialMap(), the_hole);
  a.Store(AccessBuilder::ForJSFunctionSharedFunctionInfo(), shared);
  a.Store(AccessBuilder::ForJSFunctionContext(), context);
  a.Store(AccessBuilder::ForJSFunctionCodeEntry(), compile_entry);
  a.Store(AccessBuilder::ForJSFunctionNextFunctionLink(), undefined);
  RelaxControls(node);
  a.FinishAndChange(node);
  return Changed(node);
}

Reduction JSCreateLowering::ReduceJSCreateIterResultObject(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCreateIterResultObject, node->opcode());
  Node* value = NodeProperties::GetValueInput(node, 0);
  Node* done = NodeProperties::GetValueInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);

  Node* iterator_result_map;
  Handle<Context> native_context;
  if (GetSpecializationNativeContext(node).ToHandle(&native_context)) {
    // Specialize to the constant JSIteratorResult map to enable map check
    // elimination to eliminate subsequent checks in case of inlining.
    iterator_result_map = jsgraph()->HeapConstant(
        handle(native_context->iterator_result_map(), isolate()));
  } else {
    // Load the JSIteratorResult map for the {context}.
    Node* context = NodeProperties::GetContextInput(node);
    Node* native_context = effect = graph()->NewNode(
        javascript()->LoadContext(0, Context::NATIVE_CONTEXT_INDEX, true),
        context, context, effect);
    iterator_result_map = effect = graph()->NewNode(
        javascript()->LoadContext(0, Context::ITERATOR_RESULT_MAP_INDEX, true),
        native_context, native_context, effect);
  }

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

Reduction JSCreateLowering::ReduceJSCreateLiteral(Node* node) {
  DCHECK(node->opcode() == IrOpcode::kJSCreateLiteralArray ||
         node->opcode() == IrOpcode::kJSCreateLiteralObject);
  CreateLiteralParameters const& p = CreateLiteralParametersOf(node->op());
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  Handle<LiteralsArray> literals_array;
  if (GetSpecializationLiterals(node).ToHandle(&literals_array)) {
    Handle<Object> literal(literals_array->literal(p.index()), isolate());
    if (literal->IsAllocationSite()) {
      Handle<AllocationSite> site = Handle<AllocationSite>::cast(literal);
      Handle<JSObject> boilerplate(JSObject::cast(site->transition_info()),
                                   isolate());
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
  }

  return NoChange();
}

Reduction JSCreateLowering::ReduceJSCreateFunctionContext(Node* node) {
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

  return NoChange();
}

Reduction JSCreateLowering::ReduceJSCreateWithContext(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCreateWithContext, node->opcode());
  Handle<ScopeInfo> scope_info = OpParameter<Handle<ScopeInfo>>(node);
  Node* object = NodeProperties::GetValueInput(node, 0);
  Node* closure = NodeProperties::GetValueInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* context = NodeProperties::GetContextInput(node);
  Node* native_context = effect = graph()->NewNode(
      javascript()->LoadContext(0, Context::NATIVE_CONTEXT_INDEX, true),
      context, context, effect);

  AllocationBuilder aa(jsgraph(), effect, control);
  aa.Allocate(ContextExtension::kSize);
  aa.Store(AccessBuilder::ForMap(), factory()->context_extension_map());
  aa.Store(AccessBuilder::ForContextExtensionScopeInfo(), scope_info);
  aa.Store(AccessBuilder::ForContextExtensionExtension(), object);
  Node* extension = aa.Finish();

  AllocationBuilder a(jsgraph(), extension, control);
  STATIC_ASSERT(Context::MIN_CONTEXT_SLOTS == 4);  // Ensure fully covered.
  a.AllocateArray(Context::MIN_CONTEXT_SLOTS, factory()->with_context_map());
  a.Store(AccessBuilder::ForContextSlot(Context::CLOSURE_INDEX), closure);
  a.Store(AccessBuilder::ForContextSlot(Context::PREVIOUS_INDEX), context);
  a.Store(AccessBuilder::ForContextSlot(Context::EXTENSION_INDEX), extension);
  a.Store(AccessBuilder::ForContextSlot(Context::NATIVE_CONTEXT_INDEX),
          native_context);
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
  Node* native_context = effect = graph()->NewNode(
      javascript()->LoadContext(0, Context::NATIVE_CONTEXT_INDEX, true),
      context, context, effect);

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
  a.AllocateArray(Context::MIN_CONTEXT_SLOTS + 1,
                  factory()->catch_context_map());
  a.Store(AccessBuilder::ForContextSlot(Context::CLOSURE_INDEX), closure);
  a.Store(AccessBuilder::ForContextSlot(Context::PREVIOUS_INDEX), context);
  a.Store(AccessBuilder::ForContextSlot(Context::EXTENSION_INDEX), extension);
  a.Store(AccessBuilder::ForContextSlot(Context::NATIVE_CONTEXT_INDEX),
          native_context);
  a.Store(AccessBuilder::ForContextSlot(Context::THROWN_OBJECT_INDEX),
          exception);
  RelaxControls(node);
  a.FinishAndChange(node);
  return Changed(node);
}

Reduction JSCreateLowering::ReduceJSCreateBlockContext(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCreateBlockContext, node->opcode());
  Handle<ScopeInfo> scope_info = OpParameter<Handle<ScopeInfo>>(node);
  int const context_length = scope_info->ContextLength();
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

// Helper that allocates a FixedArray holding argument values recorded in the
// given {frame_state}. Serves as backing store for JSCreateArguments nodes.
Node* JSCreateLowering::AllocateArguments(Node* effect, Node* control,
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
Node* JSCreateLowering::AllocateRestArguments(Node* effect, Node* control,
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
Node* JSCreateLowering::AllocateAliasedArguments(
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

Node* JSCreateLowering::AllocateElements(Node* effect, Node* control,
                                         ElementsKind elements_kind,
                                         int capacity,
                                         PretenureFlag pretenure) {
  DCHECK_LE(1, capacity);
  DCHECK_LE(capacity, JSArray::kInitialMaxFastElementArray);

  Handle<Map> elements_map = IsFastDoubleElementsKind(elements_kind)
                                 ? factory()->fixed_double_array_map()
                                 : factory()->fixed_array_map();
  ElementAccess access = IsFastDoubleElementsKind(elements_kind)
                             ? AccessBuilder::ForFixedDoubleArrayElement()
                             : AccessBuilder::ForFixedArrayElement();
  Node* value;
  if (IsFastDoubleElementsKind(elements_kind)) {
    // Load the hole NaN pattern from the canonical location.
    value = effect = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForExternalDoubleValue()),
        jsgraph()->ExternalConstant(
            ExternalReference::address_of_the_hole_nan()),
        effect, control);
  } else {
    value = jsgraph()->TheHoleConstant();
  }

  // Actually allocate the backing store.
  AllocationBuilder a(jsgraph(), effect, control);
  a.AllocateArray(capacity, elements_map, pretenure);
  for (int i = 0; i < capacity; ++i) {
    Node* index = jsgraph()->Constant(i);
    a.Store(access, index, value);
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

  // Setup the elements backing store.
  Node* elements = AllocateFastLiteralElements(effect, control, boilerplate,
                                               pretenure, site_context);
  if (elements->op()->EffectOutputCount() > 0) effect = elements;

  // Compute the in-object properties to store first (might have effects).
  Handle<Map> boilerplate_map(boilerplate->map(), isolate());
  ZoneVector<std::pair<FieldAccess, Node*>> inobject_fields(zone());
  inobject_fields.reserve(boilerplate_map->GetInObjectProperties());
  int const boilerplate_nof = boilerplate_map->NumberOfOwnDescriptors();
  for (int i = 0; i < boilerplate_nof; ++i) {
    PropertyDetails const property_details =
        boilerplate_map->instance_descriptors()->GetDetails(i);
    if (property_details.type() != DATA) continue;
    Handle<Name> property_name(
        boilerplate_map->instance_descriptors()->GetKey(i), isolate());
    FieldIndex index = FieldIndex::ForDescriptor(*boilerplate_map, i);
    FieldAccess access = {
        kTaggedBase, index.offset(),           property_name,
        Type::Any(), MachineType::AnyTagged(), kFullWriteBarrier};
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
        // Allocate a mutable HeapNumber box and store the value into it.
        effect = graph()->NewNode(
            common()->BeginRegion(RegionObservability::kNotObservable), effect);
        value = effect = graph()->NewNode(
            simplified()->Allocate(pretenure),
            jsgraph()->Constant(HeapNumber::kSize), effect, control);
        effect = graph()->NewNode(
            simplified()->StoreField(AccessBuilder::ForMap()), value,
            jsgraph()->HeapConstant(factory()->mutable_heap_number_map()),
            effect, control);
        effect = graph()->NewNode(
            simplified()->StoreField(AccessBuilder::ForHeapNumberValue()),
            value, jsgraph()->Constant(
                       Handle<HeapNumber>::cast(boilerplate_value)->value()),
            effect, control);
        value = effect =
            graph()->NewNode(common()->FinishRegion(), value, effect);
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

  // Actually allocate and initialize the object.
  AllocationBuilder builder(jsgraph(), effect, control);
  builder.Allocate(boilerplate_map->instance_size(), pretenure);
  builder.Store(AccessBuilder::ForMap(), boilerplate_map);
  builder.Store(AccessBuilder::ForJSObjectProperties(), properties);
  builder.Store(AccessBuilder::ForJSObjectElements(), elements);
  if (boilerplate_map->IsJSArrayMap()) {
    Handle<JSArray> boilerplate_array = Handle<JSArray>::cast(boilerplate);
    builder.Store(
        AccessBuilder::ForJSArrayLength(boilerplate_array->GetElementsKind()),
        handle(boilerplate_array->length(), isolate()));
  }
  for (auto const inobject_field : inobject_fields) {
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
    Node* the_hole_value = nullptr;
    for (int i = 0; i < elements_length; ++i) {
      if (elements->is_the_hole(i)) {
        if (the_hole_value == nullptr) {
          // Load the hole NaN pattern from the canonical location.
          the_hole_value = effect = graph()->NewNode(
              simplified()->LoadField(AccessBuilder::ForExternalDoubleValue()),
              jsgraph()->ExternalConstant(
                  ExternalReference::address_of_the_hole_nan()),
              effect, control);
        }
        elements_values[i] = the_hole_value;
      } else {
        elements_values[i] = jsgraph()->Constant(elements->get_scalar(i));
      }
    }
  } else {
    Handle<FixedArray> elements =
        Handle<FixedArray>::cast(boilerplate_elements);
    for (int i = 0; i < elements_length; ++i) {
      if (elements->is_the_hole(i)) {
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

MaybeHandle<LiteralsArray> JSCreateLowering::GetSpecializationLiterals(
    Node* node) {
  Node* const closure = NodeProperties::GetValueInput(node, 0);
  switch (closure->opcode()) {
    case IrOpcode::kHeapConstant: {
      Handle<HeapObject> object = OpParameter<Handle<HeapObject>>(closure);
      return handle(Handle<JSFunction>::cast(object)->literals());
    }
    case IrOpcode::kParameter: {
      int const index = ParameterIndexOf(closure->op());
      // The closure is always the last parameter to a JavaScript function, and
      // {Parameter} indices start at -1, so value outputs of {Start} look like
      // this: closure, receiver, param0, ..., paramN, context.
      if (index == -1) {
        return literals_array_;
      }
      break;
    }
    default:
      break;
  }
  return MaybeHandle<LiteralsArray>();
}

MaybeHandle<Context> JSCreateLowering::GetSpecializationNativeContext(
    Node* node) {
  Node* const context = NodeProperties::GetContextInput(node);
  return NodeProperties::GetSpecializationNativeContext(context,
                                                        native_context_);
}

Factory* JSCreateLowering::factory() const { return isolate()->factory(); }

Graph* JSCreateLowering::graph() const { return jsgraph()->graph(); }

Isolate* JSCreateLowering::isolate() const { return jsgraph()->isolate(); }

JSOperatorBuilder* JSCreateLowering::javascript() const {
  return jsgraph()->javascript();
}

CommonOperatorBuilder* JSCreateLowering::common() const {
  return jsgraph()->common();
}

SimplifiedOperatorBuilder* JSCreateLowering::simplified() const {
  return jsgraph()->simplified();
}

MachineOperatorBuilder* JSCreateLowering::machine() const {
  return jsgraph()->machine();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
