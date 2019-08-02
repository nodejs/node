// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-native-context-specialization.h"

#include "src/api/api-inl.h"
#include "src/builtins/accessors.h"
#include "src/codegen/code-factory.h"
#include "src/codegen/string-constants.h"
#include "src/compiler/access-builder.h"
#include "src/compiler/access-info.h"
#include "src/compiler/allocation-builder.h"
#include "src/compiler/compilation-dependencies.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/js-operator.h"
#include "src/compiler/linkage.h"
#include "src/compiler/map-inference.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/property-access-builder.h"
#include "src/compiler/type-cache.h"
#include "src/compiler/vector-slot-pair.h"
#include "src/execution/isolate-inl.h"
#include "src/numbers/dtoa.h"
#include "src/objects/feedback-vector.h"
#include "src/objects/field-index-inl.h"
#include "src/objects/heap-number.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/templates.h"

namespace v8 {
namespace internal {
namespace compiler {

// This is needed for gc_mole which will compile this file without the full set
// of GN defined macros.
#ifndef V8_TYPED_ARRAY_MAX_SIZE_IN_HEAP
#define V8_TYPED_ARRAY_MAX_SIZE_IN_HEAP 64
#endif

namespace {

bool HasNumberMaps(JSHeapBroker* broker, ZoneVector<Handle<Map>> const& maps) {
  for (auto map : maps) {
    MapRef map_ref(broker, map);
    if (map_ref.IsHeapNumberMap()) return true;
  }
  return false;
}

bool HasOnlyJSArrayMaps(JSHeapBroker* broker,
                        ZoneVector<Handle<Map>> const& maps) {
  for (auto map : maps) {
    MapRef map_ref(broker, map);
    if (!map_ref.IsJSArrayMap()) return false;
  }
  return true;
}

void TryUpdateThenDropDeprecated(Isolate* isolate, MapHandles* maps) {
  for (auto it = maps->begin(); it != maps->end();) {
    if (Map::TryUpdate(isolate, *it).ToHandle(&*it)) {
      DCHECK(!(*it)->is_deprecated());
      ++it;
    } else {
      it = maps->erase(it);
    }
  }
}

}  // namespace

JSNativeContextSpecialization::JSNativeContextSpecialization(
    Editor* editor, JSGraph* jsgraph, JSHeapBroker* broker, Flags flags,
    Handle<Context> native_context, CompilationDependencies* dependencies,
    Zone* zone, Zone* shared_zone)
    : AdvancedReducer(editor),
      jsgraph_(jsgraph),
      broker_(broker),
      flags_(flags),
      global_object_(native_context->global_object(), jsgraph->isolate()),
      global_proxy_(native_context->global_proxy(), jsgraph->isolate()),
      dependencies_(dependencies),
      zone_(zone),
      shared_zone_(shared_zone),
      type_cache_(TypeCache::Get()) {}

Reduction JSNativeContextSpecialization::Reduce(Node* node) {
  switch (node->opcode()) {
    case IrOpcode::kJSAdd:
      return ReduceJSAdd(node);
    case IrOpcode::kJSAsyncFunctionEnter:
      return ReduceJSAsyncFunctionEnter(node);
    case IrOpcode::kJSAsyncFunctionReject:
      return ReduceJSAsyncFunctionReject(node);
    case IrOpcode::kJSAsyncFunctionResolve:
      return ReduceJSAsyncFunctionResolve(node);
    case IrOpcode::kJSGetSuperConstructor:
      return ReduceJSGetSuperConstructor(node);
    case IrOpcode::kJSInstanceOf:
      return ReduceJSInstanceOf(node);
    case IrOpcode::kJSHasInPrototypeChain:
      return ReduceJSHasInPrototypeChain(node);
    case IrOpcode::kJSOrdinaryHasInstance:
      return ReduceJSOrdinaryHasInstance(node);
    case IrOpcode::kJSPromiseResolve:
      return ReduceJSPromiseResolve(node);
    case IrOpcode::kJSResolvePromise:
      return ReduceJSResolvePromise(node);
    case IrOpcode::kJSLoadContext:
      return ReduceJSLoadContext(node);
    case IrOpcode::kJSLoadGlobal:
      return ReduceJSLoadGlobal(node);
    case IrOpcode::kJSStoreGlobal:
      return ReduceJSStoreGlobal(node);
    case IrOpcode::kJSLoadNamed:
      return ReduceJSLoadNamed(node);
    case IrOpcode::kJSStoreNamed:
      return ReduceJSStoreNamed(node);
    case IrOpcode::kJSHasProperty:
      return ReduceJSHasProperty(node);
    case IrOpcode::kJSLoadProperty:
      return ReduceJSLoadProperty(node);
    case IrOpcode::kJSStoreProperty:
      return ReduceJSStoreProperty(node);
    case IrOpcode::kJSStoreNamedOwn:
      return ReduceJSStoreNamedOwn(node);
    case IrOpcode::kJSStoreDataPropertyInLiteral:
      return ReduceJSStoreDataPropertyInLiteral(node);
    case IrOpcode::kJSStoreInArrayLiteral:
      return ReduceJSStoreInArrayLiteral(node);
    case IrOpcode::kJSToObject:
      return ReduceJSToObject(node);
    case IrOpcode::kJSToString:
      return ReduceJSToString(node);
    default:
      break;
  }
  return NoChange();
}

// static
base::Optional<size_t> JSNativeContextSpecialization::GetMaxStringLength(
    JSHeapBroker* broker, Node* node) {
  if (node->opcode() == IrOpcode::kDelayedStringConstant) {
    return StringConstantBaseOf(node->op())->GetMaxStringConstantLength();
  }

  HeapObjectMatcher matcher(node);
  if (matcher.HasValue() && matcher.Ref(broker).IsString()) {
    StringRef input = matcher.Ref(broker).AsString();
    return input.length();
  }

  NumberMatcher number_matcher(node);
  if (number_matcher.HasValue()) {
    return kBase10MaximalLength + 1;
  }

  // We don't support objects with possibly monkey-patched prototype.toString
  // as it might have side-effects, so we shouldn't attempt lowering them.
  return base::nullopt;
}

Reduction JSNativeContextSpecialization::ReduceJSToString(Node* node) {
  DCHECK_EQ(IrOpcode::kJSToString, node->opcode());
  Node* const input = node->InputAt(0);
  Reduction reduction;

  HeapObjectMatcher matcher(input);
  if (matcher.HasValue() && matcher.Ref(broker()).IsString()) {
    reduction = Changed(input);  // JSToString(x:string) => x
    ReplaceWithValue(node, reduction.replacement());
    return reduction;
  }

  // TODO(turbofan): This optimization is weaker than what we used to have
  // in js-typed-lowering for OrderedNumbers. We don't have types here though,
  // so alternative approach should be designed if this causes performance
  // regressions and the stronger optimization should be re-implemented.
  NumberMatcher number_matcher(input);
  if (number_matcher.HasValue()) {
    const StringConstantBase* base =
        new (shared_zone()) NumberToStringConstant(number_matcher.Value());
    reduction =
        Replace(graph()->NewNode(common()->DelayedStringConstant(base)));
    ReplaceWithValue(node, reduction.replacement());
    return reduction;
  }

  return NoChange();
}

const StringConstantBase*
JSNativeContextSpecialization::CreateDelayedStringConstant(Node* node) {
  if (node->opcode() == IrOpcode::kDelayedStringConstant) {
    return StringConstantBaseOf(node->op());
  } else {
    NumberMatcher number_matcher(node);
    if (number_matcher.HasValue()) {
      return new (shared_zone()) NumberToStringConstant(number_matcher.Value());
    } else {
      HeapObjectMatcher matcher(node);
      if (matcher.HasValue() && matcher.Ref(broker()).IsString()) {
        StringRef s = matcher.Ref(broker()).AsString();
        return new (shared_zone())
            StringLiteral(s.object(), static_cast<size_t>(s.length()));
      } else {
        UNREACHABLE();
      }
    }
  }
}

namespace {
bool IsStringConstant(JSHeapBroker* broker, Node* node) {
  if (node->opcode() == IrOpcode::kDelayedStringConstant) {
    return true;
  }

  HeapObjectMatcher matcher(node);
  return matcher.HasValue() && matcher.Ref(broker).IsString();
}
}  // namespace

Reduction JSNativeContextSpecialization::ReduceJSAsyncFunctionEnter(
    Node* node) {
  DCHECK_EQ(IrOpcode::kJSAsyncFunctionEnter, node->opcode());
  Node* closure = NodeProperties::GetValueInput(node, 0);
  Node* receiver = NodeProperties::GetValueInput(node, 1);
  Node* context = NodeProperties::GetContextInput(node);
  Node* frame_state = NodeProperties::GetFrameStateInput(node);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  if (!dependencies()->DependOnPromiseHookProtector()) return NoChange();

  // Create the promise for the async function.
  Node* promise = effect =
      graph()->NewNode(javascript()->CreatePromise(), context, effect);

  // Create the JSAsyncFunctionObject based on the SharedFunctionInfo
  // extracted from the top-most frame in {frame_state}.
  Handle<SharedFunctionInfo> shared =
      FrameStateInfoOf(frame_state->op()).shared_info().ToHandleChecked();
  DCHECK(shared->is_compiled());
  int register_count = shared->internal_formal_parameter_count() +
                       shared->GetBytecodeArray().register_count();
  Node* value = effect =
      graph()->NewNode(javascript()->CreateAsyncFunctionObject(register_count),
                       closure, receiver, promise, context, effect, control);
  ReplaceWithValue(node, value, effect, control);
  return Replace(value);
}

Reduction JSNativeContextSpecialization::ReduceJSAsyncFunctionReject(
    Node* node) {
  DCHECK_EQ(IrOpcode::kJSAsyncFunctionReject, node->opcode());
  Node* async_function_object = NodeProperties::GetValueInput(node, 0);
  Node* reason = NodeProperties::GetValueInput(node, 1);
  Node* context = NodeProperties::GetContextInput(node);
  Node* frame_state = NodeProperties::GetFrameStateInput(node);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  if (!dependencies()->DependOnPromiseHookProtector()) return NoChange();

  // Load the promise from the {async_function_object}.
  Node* promise = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForJSAsyncFunctionObjectPromise()),
      async_function_object, effect, control);

  // Create a nested frame state inside the current method's most-recent
  // {frame_state} that will ensure that lazy deoptimizations at this
  // point will still return the {promise} instead of the result of the
  // JSRejectPromise operation (which yields undefined).
  Node* parameters[] = {promise};
  frame_state = CreateStubBuiltinContinuationFrameState(
      jsgraph(), Builtins::kAsyncFunctionLazyDeoptContinuation, context,
      parameters, arraysize(parameters), frame_state,
      ContinuationFrameStateMode::LAZY);

  // Disable the additional debug event for the rejection since a
  // debug event already happend for the exception that got us here.
  Node* debug_event = jsgraph()->FalseConstant();
  effect = graph()->NewNode(javascript()->RejectPromise(), promise, reason,
                            debug_event, context, frame_state, effect, control);
  ReplaceWithValue(node, promise, effect, control);
  return Replace(promise);
}

Reduction JSNativeContextSpecialization::ReduceJSAsyncFunctionResolve(
    Node* node) {
  DCHECK_EQ(IrOpcode::kJSAsyncFunctionResolve, node->opcode());
  Node* async_function_object = NodeProperties::GetValueInput(node, 0);
  Node* value = NodeProperties::GetValueInput(node, 1);
  Node* context = NodeProperties::GetContextInput(node);
  Node* frame_state = NodeProperties::GetFrameStateInput(node);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  if (!dependencies()->DependOnPromiseHookProtector()) return NoChange();

  // Load the promise from the {async_function_object}.
  Node* promise = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForJSAsyncFunctionObjectPromise()),
      async_function_object, effect, control);

  // Create a nested frame state inside the current method's most-recent
  // {frame_state} that will ensure that lazy deoptimizations at this
  // point will still return the {promise} instead of the result of the
  // JSResolvePromise operation (which yields undefined).
  Node* parameters[] = {promise};
  frame_state = CreateStubBuiltinContinuationFrameState(
      jsgraph(), Builtins::kAsyncFunctionLazyDeoptContinuation, context,
      parameters, arraysize(parameters), frame_state,
      ContinuationFrameStateMode::LAZY);

  effect = graph()->NewNode(javascript()->ResolvePromise(), promise, value,
                            context, frame_state, effect, control);
  ReplaceWithValue(node, promise, effect, control);
  return Replace(promise);
}

Reduction JSNativeContextSpecialization::ReduceJSAdd(Node* node) {
  // TODO(turbofan): This has to run together with the inlining and
  // native context specialization to be able to leverage the string
  // constant-folding for optimizing property access, but we should
  // nevertheless find a better home for this at some point.
  DCHECK_EQ(IrOpcode::kJSAdd, node->opcode());

  Node* const lhs = node->InputAt(0);
  Node* const rhs = node->InputAt(1);

  base::Optional<size_t> lhs_len = GetMaxStringLength(broker(), lhs);
  base::Optional<size_t> rhs_len = GetMaxStringLength(broker(), rhs);
  if (!lhs_len || !rhs_len) {
    return NoChange();
  }

  // Fold into DelayedStringConstant if at least one of the parameters is a
  // string constant and the addition won't throw due to too long result.
  if (*lhs_len + *rhs_len <= String::kMaxLength &&
      (IsStringConstant(broker(), lhs) || IsStringConstant(broker(), rhs))) {
    const StringConstantBase* left = CreateDelayedStringConstant(lhs);
    const StringConstantBase* right = CreateDelayedStringConstant(rhs);
    const StringConstantBase* cons =
        new (shared_zone()) StringCons(left, right);

    Node* reduced = graph()->NewNode(common()->DelayedStringConstant(cons));
    ReplaceWithValue(node, reduced);
    return Replace(reduced);
  }

  return NoChange();
}

Reduction JSNativeContextSpecialization::ReduceJSGetSuperConstructor(
    Node* node) {
  DCHECK_EQ(IrOpcode::kJSGetSuperConstructor, node->opcode());
  Node* constructor = NodeProperties::GetValueInput(node, 0);

  // Check if the input is a known JSFunction.
  HeapObjectMatcher m(constructor);
  if (!m.HasValue()) return NoChange();
  JSFunctionRef function = m.Ref(broker()).AsJSFunction();
  MapRef function_map = function.map();
  if (!FLAG_concurrent_inlining) {
    function_map.SerializePrototype();
  } else if (!function_map.serialized_prototype()) {
    TRACE_BROKER_MISSING(broker(), "data for map " << function_map);
    return NoChange();
  }
  ObjectRef function_prototype = function_map.prototype();

  // We can constant-fold the super constructor access if the
  // {function}s map is stable, i.e. we can use a code dependency
  // to guard against [[Prototype]] changes of {function}.
  if (function_map.is_stable() && function_prototype.IsHeapObject() &&
      function_prototype.AsHeapObject().map().is_constructor()) {
    dependencies()->DependOnStableMap(function_map);
    Node* value = jsgraph()->Constant(function_prototype);
    ReplaceWithValue(node, value);
    return Replace(value);
  }

  return NoChange();
}

Reduction JSNativeContextSpecialization::ReduceJSInstanceOf(Node* node) {
  DCHECK_EQ(IrOpcode::kJSInstanceOf, node->opcode());
  FeedbackParameter const& p = FeedbackParameterOf(node->op());
  Node* object = NodeProperties::GetValueInput(node, 0);
  Node* constructor = NodeProperties::GetValueInput(node, 1);
  Node* context = NodeProperties::GetContextInput(node);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* frame_state = NodeProperties::GetFrameStateInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // Check if the right hand side is a known {receiver}, or
  // we have feedback from the InstanceOfIC.
  Handle<JSObject> receiver;
  HeapObjectMatcher m(constructor);
  if (m.HasValue() && m.Value()->IsJSObject()) {
    receiver = Handle<JSObject>::cast(m.Value());
  } else if (p.feedback().IsValid()) {
    FeedbackNexus nexus(p.feedback().vector(), p.feedback().slot());
    if (!nexus.GetConstructorFeedback().ToHandle(&receiver)) return NoChange();
  } else {
    return NoChange();
  }
  Handle<Map> receiver_map(receiver->map(), isolate());

  // Compute property access info for @@hasInstance on the constructor.
  AccessInfoFactory access_info_factory(broker(), dependencies(),
                                        graph()->zone());
  PropertyAccessInfo access_info =
      access_info_factory.ComputePropertyAccessInfo(
          receiver_map, factory()->has_instance_symbol(), AccessMode::kLoad);
  if (access_info.IsInvalid()) return NoChange();
  access_info.RecordDependencies(dependencies());

  PropertyAccessBuilder access_builder(jsgraph(), broker(), dependencies());

  if (access_info.IsNotFound()) {
    // If there's no @@hasInstance handler, the OrdinaryHasInstance operation
    // takes over, but that requires the constructor to be callable.
    if (!receiver_map->is_callable()) return NoChange();

    dependencies()->DependOnStablePrototypeChains(access_info.receiver_maps(),
                                                  kStartAtPrototype);

    // Monomorphic property access.
    access_builder.BuildCheckMaps(constructor, &effect, control,
                                  access_info.receiver_maps());

    // Lower to OrdinaryHasInstance(C, O).
    NodeProperties::ReplaceValueInput(node, constructor, 0);
    NodeProperties::ReplaceValueInput(node, object, 1);
    NodeProperties::ReplaceEffectInput(node, effect);
    NodeProperties::ChangeOp(node, javascript()->OrdinaryHasInstance());
    Reduction const reduction = ReduceJSOrdinaryHasInstance(node);
    return reduction.Changed() ? reduction : Changed(node);
  }

  if (access_info.IsDataConstant()) {
    // Determine actual holder.
    Handle<JSObject> holder;
    bool found_on_proto = access_info.holder().ToHandle(&holder);
    if (!found_on_proto) holder = receiver;

    FieldIndex field_index = access_info.field_index();
    Handle<Object> constant = JSObject::FastPropertyAt(
        holder, access_info.field_representation(), field_index);
    if (!constant->IsCallable()) {
      return NoChange();
    }

    if (found_on_proto) {
      dependencies()->DependOnStablePrototypeChains(
          access_info.receiver_maps(), kStartAtPrototype,
          JSObjectRef(broker(), holder));
    }

    DCHECK(constant->IsCallable());

    // Check that {constructor} is actually {receiver}.
    constructor =
        access_builder.BuildCheckValue(constructor, &effect, control, receiver);

    // Monomorphic property access.
    access_builder.BuildCheckMaps(constructor, &effect, control,
                                  access_info.receiver_maps());

    // Create a nested frame state inside the current method's most-recent frame
    // state that will ensure that deopts that happen after this point will not
    // fallback to the last Checkpoint--which would completely re-execute the
    // instanceof logic--but rather create an activation of a version of the
    // ToBoolean stub that finishes the remaining work of instanceof and returns
    // to the caller without duplicating side-effects upon a lazy deopt.
    Node* continuation_frame_state = CreateStubBuiltinContinuationFrameState(
        jsgraph(), Builtins::kToBooleanLazyDeoptContinuation, context, nullptr,
        0, frame_state, ContinuationFrameStateMode::LAZY);

    // Call the @@hasInstance handler.
    Node* target = jsgraph()->Constant(constant);
    node->InsertInput(graph()->zone(), 0, target);
    node->ReplaceInput(1, constructor);
    node->ReplaceInput(2, object);
    node->ReplaceInput(4, continuation_frame_state);
    node->ReplaceInput(5, effect);
    NodeProperties::ChangeOp(
        node, javascript()->Call(3, CallFrequency(), VectorSlotPair(),
                                 ConvertReceiverMode::kNotNullOrUndefined));

    // Rewire the value uses of {node} to ToBoolean conversion of the result.
    Node* value = graph()->NewNode(simplified()->ToBoolean(), node);
    for (Edge edge : node->use_edges()) {
      if (NodeProperties::IsValueEdge(edge) && edge.from() != value) {
        edge.UpdateTo(value);
        Revisit(edge.from());
      }
    }
    return Changed(node);
  }

  return NoChange();
}

JSNativeContextSpecialization::InferHasInPrototypeChainResult
JSNativeContextSpecialization::InferHasInPrototypeChain(
    Node* receiver, Node* effect, Handle<HeapObject> prototype) {
  ZoneHandleSet<Map> receiver_maps;
  NodeProperties::InferReceiverMapsResult result =
      NodeProperties::InferReceiverMaps(broker(), receiver, effect,
                                        &receiver_maps);
  if (result == NodeProperties::kNoReceiverMaps) return kMayBeInPrototypeChain;

  // Try to determine either that all of the {receiver_maps} have the given
  // {prototype} in their chain, or that none do. If we can't tell, return
  // kMayBeInPrototypeChain.
  bool all = true;
  bool none = true;
  for (size_t i = 0; i < receiver_maps.size(); ++i) {
    Handle<Map> receiver_map = receiver_maps[i];
    if (receiver_map->instance_type() <= LAST_SPECIAL_RECEIVER_TYPE) {
      return kMayBeInPrototypeChain;
    }
    if (result == NodeProperties::kUnreliableReceiverMaps &&
        !receiver_map->is_stable()) {
      return kMayBeInPrototypeChain;
    }
    for (PrototypeIterator it(isolate(), receiver_map);; it.Advance()) {
      if (it.IsAtEnd()) {
        all = false;
        break;
      }
      Handle<HeapObject> current =
          PrototypeIterator::GetCurrent<HeapObject>(it);
      if (current.is_identical_to(prototype)) {
        none = false;
        break;
      }
      if (!current->map().is_stable() ||
          current->map().instance_type() <= LAST_SPECIAL_RECEIVER_TYPE) {
        return kMayBeInPrototypeChain;
      }
    }
  }
  DCHECK_IMPLIES(all, !none);
  if (!all && !none) return kMayBeInPrototypeChain;

  {
    base::Optional<JSObjectRef> last_prototype;
    if (all) {
      // We don't need to protect the full chain if we found the prototype, we
      // can stop at {prototype}.  In fact we could stop at the one before
      // {prototype} but since we're dealing with multiple receiver maps this
      // might be a different object each time, so it's much simpler to include
      // {prototype}. That does, however, mean that we must check {prototype}'s
      // map stability.
      if (!prototype->map().is_stable()) return kMayBeInPrototypeChain;
      last_prototype.emplace(broker(), Handle<JSObject>::cast(prototype));
    }
    WhereToStart start = result == NodeProperties::kUnreliableReceiverMaps
                             ? kStartAtReceiver
                             : kStartAtPrototype;
    dependencies()->DependOnStablePrototypeChains(receiver_maps, start,
                                                  last_prototype);
  }

  DCHECK_EQ(all, !none);
  return all ? kIsInPrototypeChain : kIsNotInPrototypeChain;
}

Reduction JSNativeContextSpecialization::ReduceJSHasInPrototypeChain(
    Node* node) {
  DCHECK_EQ(IrOpcode::kJSHasInPrototypeChain, node->opcode());
  Node* value = NodeProperties::GetValueInput(node, 0);
  Node* prototype = NodeProperties::GetValueInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);

  // Check if we can constant-fold the prototype chain walk
  // for the given {value} and the {prototype}.
  HeapObjectMatcher m(prototype);
  if (m.HasValue()) {
    InferHasInPrototypeChainResult result =
        InferHasInPrototypeChain(value, effect, m.Value());
    if (result != kMayBeInPrototypeChain) {
      Node* value = jsgraph()->BooleanConstant(result == kIsInPrototypeChain);
      ReplaceWithValue(node, value);
      return Replace(value);
    }
  }

  return NoChange();
}

Reduction JSNativeContextSpecialization::ReduceJSOrdinaryHasInstance(
    Node* node) {
  DCHECK_EQ(IrOpcode::kJSOrdinaryHasInstance, node->opcode());
  Node* constructor = NodeProperties::GetValueInput(node, 0);
  Node* object = NodeProperties::GetValueInput(node, 1);

  // Check if the {constructor} is known at compile time.
  HeapObjectMatcher m(constructor);
  if (!m.HasValue()) return NoChange();

  // Check if the {constructor} is a JSBoundFunction.
  if (m.Value()->IsJSBoundFunction()) {
    // OrdinaryHasInstance on bound functions turns into a recursive
    // invocation of the instanceof operator again.
    // ES6 section 7.3.19 OrdinaryHasInstance (C, O) step 2.
    Handle<JSBoundFunction> function = Handle<JSBoundFunction>::cast(m.Value());
    Handle<JSReceiver> bound_target_function(function->bound_target_function(),
                                             isolate());
    NodeProperties::ReplaceValueInput(node, object, 0);
    NodeProperties::ReplaceValueInput(
        node, jsgraph()->HeapConstant(bound_target_function), 1);
    NodeProperties::ChangeOp(node, javascript()->InstanceOf(VectorSlotPair()));
    Reduction const reduction = ReduceJSInstanceOf(node);
    return reduction.Changed() ? reduction : Changed(node);
  }

  // Optimize if we currently know the "prototype" property.
  if (m.Value()->IsJSFunction()) {
    JSFunctionRef function = m.Ref(broker()).AsJSFunction();
    // TODO(neis): This is a temporary hack needed because the copy reducer
    // runs only after this pass.
    function.Serialize();
    // TODO(neis): Remove the has_prototype_slot condition once the broker is
    // always enabled.
    if (!function.map().has_prototype_slot() || !function.has_prototype() ||
        function.PrototypeRequiresRuntimeLookup()) {
      return NoChange();
    }
    ObjectRef prototype = dependencies()->DependOnPrototypeProperty(function);
    Node* prototype_constant = jsgraph()->Constant(prototype);

    // Lower the {node} to JSHasInPrototypeChain.
    NodeProperties::ReplaceValueInput(node, object, 0);
    NodeProperties::ReplaceValueInput(node, prototype_constant, 1);
    NodeProperties::ChangeOp(node, javascript()->HasInPrototypeChain());
    Reduction const reduction = ReduceJSHasInPrototypeChain(node);
    return reduction.Changed() ? reduction : Changed(node);
  }

  return NoChange();
}

// ES section #sec-promise-resolve
Reduction JSNativeContextSpecialization::ReduceJSPromiseResolve(Node* node) {
  DCHECK_EQ(IrOpcode::kJSPromiseResolve, node->opcode());
  Node* constructor = NodeProperties::GetValueInput(node, 0);
  Node* value = NodeProperties::GetValueInput(node, 1);
  Node* context = NodeProperties::GetContextInput(node);
  Node* frame_state = NodeProperties::GetFrameStateInput(node);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // Check if the {constructor} is the %Promise% function.
  HeapObjectMatcher m(constructor);
  if (!m.HasValue() ||
      !m.Ref(broker()).equals(broker()->native_context().promise_function())) {
    return NoChange();
  }

  // Only optimize if {value} cannot be a JSPromise.
  MapInference inference(broker(), value, effect);
  if (!inference.HaveMaps() ||
      inference.AnyOfInstanceTypesAre(JS_PROMISE_TYPE)) {
    return NoChange();
  }

  if (!dependencies()->DependOnPromiseHookProtector()) return NoChange();

  // Create a %Promise% instance and resolve it with {value}.
  Node* promise = effect =
      graph()->NewNode(javascript()->CreatePromise(), context, effect);
  effect = graph()->NewNode(javascript()->ResolvePromise(), promise, value,
                            context, frame_state, effect, control);
  ReplaceWithValue(node, promise, effect, control);
  return Replace(promise);
}

// ES section #sec-promise-resolve-functions
Reduction JSNativeContextSpecialization::ReduceJSResolvePromise(Node* node) {
  DCHECK_EQ(IrOpcode::kJSResolvePromise, node->opcode());
  Node* promise = NodeProperties::GetValueInput(node, 0);
  Node* resolution = NodeProperties::GetValueInput(node, 1);
  Node* context = NodeProperties::GetContextInput(node);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // Check if we know something about the {resolution}.
  MapInference inference(broker(), resolution, effect);
  if (!inference.HaveMaps()) return NoChange();
  MapHandles const& resolution_maps = inference.GetMaps();

  // Compute property access info for "then" on {resolution}.
  ZoneVector<PropertyAccessInfo> access_infos(graph()->zone());
  AccessInfoFactory access_info_factory(broker(), dependencies(),
                                        graph()->zone());
  access_info_factory.ComputePropertyAccessInfos(
      resolution_maps, factory()->then_string(), AccessMode::kLoad,
      &access_infos);
  PropertyAccessInfo access_info =
      access_info_factory.FinalizePropertyAccessInfosAsOne(access_infos,
                                                           AccessMode::kLoad);
  if (access_info.IsInvalid()) return inference.NoChange();

  // Only optimize when {resolution} definitely doesn't have a "then" property.
  if (!access_info.IsNotFound()) return inference.NoChange();

  if (!inference.RelyOnMapsViaStability(dependencies())) {
    return inference.NoChange();
  }

  dependencies()->DependOnStablePrototypeChains(access_info.receiver_maps(),
                                                kStartAtPrototype);

  // Simply fulfill the {promise} with the {resolution}.
  Node* value = effect =
      graph()->NewNode(javascript()->FulfillPromise(), promise, resolution,
                       context, effect, control);
  ReplaceWithValue(node, value, effect, control);
  return Replace(value);
}

Reduction JSNativeContextSpecialization::ReduceJSLoadContext(Node* node) {
  DCHECK_EQ(IrOpcode::kJSLoadContext, node->opcode());
  ContextAccess const& access = ContextAccessOf(node->op());
  // Specialize JSLoadContext(NATIVE_CONTEXT_INDEX) to the known native
  // context (if any), so we can constant-fold those fields, which is
  // safe, since the NATIVE_CONTEXT_INDEX slot is always immutable.
  if (access.index() == Context::NATIVE_CONTEXT_INDEX) {
    Node* value = jsgraph()->Constant(native_context());
    ReplaceWithValue(node, value);
    return Replace(value);
  }
  return NoChange();
}

namespace {

FieldAccess ForPropertyCellValue(MachineRepresentation representation,
                                 Type type, MaybeHandle<Map> map,
                                 NameRef const& name) {
  WriteBarrierKind kind = kFullWriteBarrier;
  if (representation == MachineRepresentation::kTaggedSigned ||
      representation == MachineRepresentation::kCompressedSigned) {
    kind = kNoWriteBarrier;
  } else if (representation == MachineRepresentation::kTaggedPointer ||
             representation == MachineRepresentation::kCompressedPointer) {
    kind = kPointerWriteBarrier;
  }
  MachineType r = MachineType::TypeForRepresentation(representation);
  FieldAccess access = {
      kTaggedBase, PropertyCell::kValueOffset, name.object(), map, type, r,
      kind};
  return access;
}

}  // namespace

Reduction JSNativeContextSpecialization::ReduceGlobalAccess(
    Node* node, Node* receiver, Node* value, NameRef const& name,
    AccessMode access_mode, Node* key) {
  base::Optional<PropertyCellRef> cell =
      native_context().global_proxy_object().GetPropertyCell(name);
  return cell.has_value() ? ReduceGlobalAccess(node, receiver, value, name,
                                               access_mode, key, *cell)
                          : NoChange();
}

Reduction JSNativeContextSpecialization::ReduceGlobalAccess(
    Node* node, Node* receiver, Node* value, NameRef const& name,
    AccessMode access_mode, Node* key, PropertyCellRef const& property_cell) {
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  ObjectRef property_cell_value = property_cell.value();
  if (property_cell_value.IsHeapObject() &&
      property_cell_value.AsHeapObject().map().oddball_type() ==
          OddballType::kHole) {
    // The property cell is no longer valid.
    return NoChange();
  }

  PropertyDetails property_details = property_cell.property_details();
  PropertyCellType property_cell_type = property_details.cell_type();
  DCHECK_EQ(kData, property_details.kind());

  // We have additional constraints for stores.
  if (access_mode == AccessMode::kStore) {
    if (property_details.IsReadOnly()) {
      // Don't even bother trying to lower stores to read-only data properties.
      return NoChange();
    } else if (property_cell_type == PropertyCellType::kUndefined) {
      // There's no fast-path for dealing with undefined property cells.
      return NoChange();
    } else if (property_cell_type == PropertyCellType::kConstantType) {
      // There's also no fast-path to store to a global cell which pretended
      // to be stable, but is no longer stable now.
      if (property_cell_value.IsHeapObject() &&
          !property_cell_value.AsHeapObject().map().is_stable()) {
        return NoChange();
      }
    }
  } else if (access_mode == AccessMode::kHas) {
    // has checks cannot follow the fast-path used by loads when these
    // conditions hold.
    if ((property_details.IsConfigurable() || !property_details.IsReadOnly()) &&
        property_details.cell_type() != PropertyCellType::kConstant &&
        property_details.cell_type() != PropertyCellType::kUndefined)
      return NoChange();
  }

  // Ensure that {key} matches the specified {name} (if {key} is given).
  if (key != nullptr) {
    effect = BuildCheckEqualsName(name, key, effect, control);
  }

  // Check if we have a {receiver} to validate. If so, we need to check that
  // the {receiver} is actually the JSGlobalProxy for the native context that
  // we are specializing to.
  if (receiver != nullptr) {
    Node* check = graph()->NewNode(simplified()->ReferenceEqual(), receiver,
                                   jsgraph()->HeapConstant(global_proxy()));
    effect = graph()->NewNode(
        simplified()->CheckIf(DeoptimizeReason::kReceiverNotAGlobalProxy),
        check, effect, control);
  }

  if (access_mode == AccessMode::kLoad || access_mode == AccessMode::kHas) {
    // Load from non-configurable, read-only data property on the global
    // object can be constant-folded, even without deoptimization support.
    if (!property_details.IsConfigurable() && property_details.IsReadOnly()) {
      value = access_mode == AccessMode::kHas
                  ? jsgraph()->TrueConstant()
                  : jsgraph()->Constant(property_cell_value);
    } else {
      // Record a code dependency on the cell if we can benefit from the
      // additional feedback, or the global property is configurable (i.e.
      // can be deleted or reconfigured to an accessor property).
      if (property_details.cell_type() != PropertyCellType::kMutable ||
          property_details.IsConfigurable()) {
        dependencies()->DependOnGlobalProperty(property_cell);
      }

      // Load from constant/undefined global property can be constant-folded.
      if (property_details.cell_type() == PropertyCellType::kConstant ||
          property_details.cell_type() == PropertyCellType::kUndefined) {
        value = access_mode == AccessMode::kHas
                    ? jsgraph()->TrueConstant()
                    : jsgraph()->Constant(property_cell_value);
        DCHECK(!property_cell_value.IsHeapObject() ||
               property_cell_value.AsHeapObject().map().oddball_type() !=
                   OddballType::kHole);
      } else {
        DCHECK_NE(AccessMode::kHas, access_mode);

        // Load from constant type cell can benefit from type feedback.
        MaybeHandle<Map> map;
        Type property_cell_value_type = Type::NonInternal();
        MachineRepresentation representation =
            MachineType::RepCompressedTagged();
        if (property_details.cell_type() == PropertyCellType::kConstantType) {
          // Compute proper type based on the current value in the cell.
          if (property_cell_value.IsSmi()) {
            property_cell_value_type = Type::SignedSmall();
            representation = MachineType::RepCompressedTaggedSigned();
          } else if (property_cell_value.IsHeapNumber()) {
            property_cell_value_type = Type::Number();
            representation = MachineType::RepCompressedTaggedPointer();
          } else {
            MapRef property_cell_value_map =
                property_cell_value.AsHeapObject().map();
            property_cell_value_type = Type::For(property_cell_value_map);
            representation = MachineType::RepCompressedTaggedPointer();

            // We can only use the property cell value map for map check
            // elimination if it's stable, i.e. the HeapObject wasn't
            // mutated without the cell state being updated.
            if (property_cell_value_map.is_stable()) {
              dependencies()->DependOnStableMap(property_cell_value_map);
              map = property_cell_value_map.object();
            }
          }
        }
        value = effect = graph()->NewNode(
            simplified()->LoadField(ForPropertyCellValue(
                representation, property_cell_value_type, map, name)),
            jsgraph()->Constant(property_cell), effect, control);
      }
    }
  } else {
    DCHECK_EQ(AccessMode::kStore, access_mode);
    DCHECK(!property_details.IsReadOnly());
    switch (property_details.cell_type()) {
      case PropertyCellType::kUndefined: {
        UNREACHABLE();
        break;
      }
      case PropertyCellType::kConstant: {
        // Record a code dependency on the cell, and just deoptimize if the new
        // value doesn't match the previous value stored inside the cell.
        dependencies()->DependOnGlobalProperty(property_cell);
        Node* check =
            graph()->NewNode(simplified()->ReferenceEqual(), value,
                             jsgraph()->Constant(property_cell_value));
        effect = graph()->NewNode(
            simplified()->CheckIf(DeoptimizeReason::kValueMismatch), check,
            effect, control);
        break;
      }
      case PropertyCellType::kConstantType: {
        // Record a code dependency on the cell, and just deoptimize if the new
        // values' type doesn't match the type of the previous value in the
        // cell.
        dependencies()->DependOnGlobalProperty(property_cell);
        Type property_cell_value_type;
        MachineRepresentation representation =
            MachineType::RepCompressedTagged();
        if (property_cell_value.IsHeapObject()) {
          // We cannot do anything if the {property_cell_value}s map is no
          // longer stable.
          MapRef property_cell_value_map =
              property_cell_value.AsHeapObject().map();
          dependencies()->DependOnStableMap(property_cell_value_map);

          // Check that the {value} is a HeapObject.
          value = effect = graph()->NewNode(simplified()->CheckHeapObject(),
                                            value, effect, control);

          // Check {value} map against the {property_cell} map.
          effect = graph()->NewNode(
              simplified()->CheckMaps(
                  CheckMapsFlag::kNone,
                  ZoneHandleSet<Map>(property_cell_value_map.object())),
              value, effect, control);
          property_cell_value_type = Type::OtherInternal();
          representation = MachineType::RepCompressedTaggedPointer();
        } else {
          // Check that the {value} is a Smi.
          value = effect = graph()->NewNode(
              simplified()->CheckSmi(VectorSlotPair()), value, effect, control);
          property_cell_value_type = Type::SignedSmall();
          representation = MachineType::RepCompressedTaggedSigned();
        }
        effect = graph()->NewNode(simplified()->StoreField(ForPropertyCellValue(
                                      representation, property_cell_value_type,
                                      MaybeHandle<Map>(), name)),
                                  jsgraph()->Constant(property_cell), value,
                                  effect, control);
        break;
      }
      case PropertyCellType::kMutable: {
        // Record a code dependency on the cell, and just deoptimize if the
        // property ever becomes read-only.
        dependencies()->DependOnGlobalProperty(property_cell);
        effect = graph()->NewNode(
            simplified()->StoreField(ForPropertyCellValue(
                MachineType::RepCompressedTagged(), Type::NonInternal(),
                MaybeHandle<Map>(), name)),
            jsgraph()->Constant(property_cell), value, effect, control);
        break;
      }
    }
  }

  ReplaceWithValue(node, value, effect, control);
  return Replace(value);
}

Reduction JSNativeContextSpecialization::ReduceJSLoadGlobal(Node* node) {
  DCHECK_EQ(IrOpcode::kJSLoadGlobal, node->opcode());
  DisallowHeapAccessIf no_heap_acess(FLAG_concurrent_inlining);

  LoadGlobalParameters const& p = LoadGlobalParametersOf(node->op());
  if (!p.feedback().IsValid()) return NoChange();
  FeedbackSource source(p.feedback());

  // TODO(neis): Make consistent with other feedback processing code.
  GlobalAccessFeedback const* processed =
      FLAG_concurrent_inlining
          ? broker()->GetGlobalAccessFeedback(source)
          : broker()->ProcessFeedbackForGlobalAccess(source);
  if (processed == nullptr) return NoChange();

  if (processed->IsScriptContextSlot()) {
    Node* effect = NodeProperties::GetEffectInput(node);
    Node* script_context = jsgraph()->Constant(processed->script_context());
    Node* value = effect =
        graph()->NewNode(javascript()->LoadContext(0, processed->slot_index(),
                                                   processed->immutable()),
                         script_context, effect);
    ReplaceWithValue(node, value, effect);
    return Replace(value);
  }

  CHECK(processed->IsPropertyCell());
  return ReduceGlobalAccess(node, nullptr, nullptr, NameRef(broker(), p.name()),
                            AccessMode::kLoad, nullptr,
                            processed->property_cell());
}

Reduction JSNativeContextSpecialization::ReduceJSStoreGlobal(Node* node) {
  DCHECK_EQ(IrOpcode::kJSStoreGlobal, node->opcode());
  DisallowHeapAccessIf no_heap_acess(FLAG_concurrent_inlining);

  Node* value = NodeProperties::GetValueInput(node, 0);

  StoreGlobalParameters const& p = StoreGlobalParametersOf(node->op());
  if (!p.feedback().IsValid()) return NoChange();
  FeedbackSource source(p.feedback());

  GlobalAccessFeedback const* processed =
      FLAG_concurrent_inlining
          ? broker()->GetGlobalAccessFeedback(source)
          : broker()->ProcessFeedbackForGlobalAccess(source);
  if (processed == nullptr) return NoChange();

  if (processed->IsScriptContextSlot()) {
    if (processed->immutable()) return NoChange();
    Node* effect = NodeProperties::GetEffectInput(node);
    Node* control = NodeProperties::GetControlInput(node);
    Node* script_context = jsgraph()->Constant(processed->script_context());
    effect =
        graph()->NewNode(javascript()->StoreContext(0, processed->slot_index()),
                         value, script_context, effect, control);
    ReplaceWithValue(node, value, effect, control);
    return Replace(value);
  }

  if (processed->IsPropertyCell()) {
    return ReduceGlobalAccess(node, nullptr, value, NameRef(broker(), p.name()),
                              AccessMode::kStore, nullptr,
                              processed->property_cell());
  }

  UNREACHABLE();
}

Reduction JSNativeContextSpecialization::ReduceNamedAccess(
    Node* node, Node* value, NamedAccessFeedback const& feedback,
    AccessMode access_mode, Node* key) {
  DCHECK(node->opcode() == IrOpcode::kJSLoadNamed ||
         node->opcode() == IrOpcode::kJSStoreNamed ||
         node->opcode() == IrOpcode::kJSLoadProperty ||
         node->opcode() == IrOpcode::kJSStoreProperty ||
         node->opcode() == IrOpcode::kJSStoreNamedOwn ||
         node->opcode() == IrOpcode::kJSHasProperty);
  Node* receiver = NodeProperties::GetValueInput(node, 0);
  Node* context = NodeProperties::GetContextInput(node);
  Node* frame_state = NodeProperties::GetFrameStateInput(node);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  ZoneVector<PropertyAccessInfo> access_infos(zone());
  AccessInfoFactory access_info_factory(broker(), dependencies(), zone());
  if (!access_info_factory.FinalizePropertyAccessInfos(
          feedback.access_infos(), access_mode, &access_infos)) {
    return NoChange();
  }

  // Check if we have an access o.x or o.x=v where o is the current
  // native contexts' global proxy, and turn that into a direct access
  // to the current native context's global object instead.
  if (access_infos.size() == 1 && access_infos[0].receiver_maps().size() == 1) {
    MapRef receiver_map(broker(), access_infos[0].receiver_maps()[0]);
    if (receiver_map.IsMapOfCurrentGlobalProxy()) {
      return ReduceGlobalAccess(node, receiver, value, feedback.name(),
                                access_mode, key);
    }
  }

  // Ensure that {key} matches the specified name (if {key} is given).
  if (key != nullptr) {
    effect = BuildCheckEqualsName(feedback.name(), key, effect, control);
  }

  // Collect call nodes to rewire exception edges.
  ZoneVector<Node*> if_exception_nodes(zone());
  ZoneVector<Node*>* if_exceptions = nullptr;
  Node* if_exception = nullptr;
  if (NodeProperties::IsExceptionalCall(node, &if_exception)) {
    if_exceptions = &if_exception_nodes;
  }

  PropertyAccessBuilder access_builder(jsgraph(), broker(), dependencies());

  // Check for the monomorphic cases.
  if (access_infos.size() == 1) {
    PropertyAccessInfo access_info = access_infos.front();
    // Try to build string check or number check if possible.
    // Otherwise build a map check.
    if (!access_builder.TryBuildStringCheck(broker(),
                                            access_info.receiver_maps(),
                                            &receiver, &effect, control) &&
        !access_builder.TryBuildNumberCheck(broker(),
                                            access_info.receiver_maps(),
                                            &receiver, &effect, control)) {
      if (HasNumberMaps(broker(), access_info.receiver_maps())) {
        // We need to also let Smi {receiver}s through in this case, so
        // we construct a diamond, guarded by the Sminess of the {receiver}
        // and if {receiver} is not a Smi just emit a sequence of map checks.
        Node* check = graph()->NewNode(simplified()->ObjectIsSmi(), receiver);
        Node* branch = graph()->NewNode(common()->Branch(), check, control);

        Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
        Node* etrue = effect;

        Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
        Node* efalse = effect;
        {
          access_builder.BuildCheckMaps(receiver, &efalse, if_false,
                                        access_info.receiver_maps());
        }

        control = graph()->NewNode(common()->Merge(2), if_true, if_false);
        effect =
            graph()->NewNode(common()->EffectPhi(2), etrue, efalse, control);
      } else {
        access_builder.BuildCheckMaps(receiver, &effect, control,
                                      access_info.receiver_maps());
      }
    }

    // Generate the actual property access.
    ValueEffectControl continuation = BuildPropertyAccess(
        receiver, value, context, frame_state, effect, control, feedback.name(),
        if_exceptions, access_info, access_mode);
    value = continuation.value();
    effect = continuation.effect();
    control = continuation.control();
  } else {
    // The final states for every polymorphic branch. We join them with
    // Merge+Phi+EffectPhi at the bottom.
    ZoneVector<Node*> values(zone());
    ZoneVector<Node*> effects(zone());
    ZoneVector<Node*> controls(zone());

    // Check if {receiver} may be a number.
    bool receiverissmi_possible = false;
    for (PropertyAccessInfo const& access_info : access_infos) {
      if (HasNumberMaps(broker(), access_info.receiver_maps())) {
        receiverissmi_possible = true;
        break;
      }
    }

    // Handle the case that {receiver} may be a number.
    Node* receiverissmi_control = nullptr;
    Node* receiverissmi_effect = effect;
    if (receiverissmi_possible) {
      Node* check = graph()->NewNode(simplified()->ObjectIsSmi(), receiver);
      Node* branch = graph()->NewNode(common()->Branch(), check, control);
      control = graph()->NewNode(common()->IfFalse(), branch);
      receiverissmi_control = graph()->NewNode(common()->IfTrue(), branch);
      receiverissmi_effect = effect;
    }

    // Generate code for the various different property access patterns.
    Node* fallthrough_control = control;
    for (size_t j = 0; j < access_infos.size(); ++j) {
      PropertyAccessInfo const& access_info = access_infos[j];
      Node* this_value = value;
      Node* this_receiver = receiver;
      Node* this_effect = effect;
      Node* this_control = fallthrough_control;

      // Perform map check on {receiver}.
      ZoneVector<Handle<Map>> const& receiver_maps =
          access_info.receiver_maps();
      {
        // Whether to insert a dedicated MapGuard node into the
        // effect to be able to learn from the control flow.
        bool insert_map_guard = true;

        // Check maps for the {receiver}s.
        if (j == access_infos.size() - 1) {
          // Last map check on the fallthrough control path, do a
          // conditional eager deoptimization exit here.
          access_builder.BuildCheckMaps(receiver, &this_effect, this_control,
                                        receiver_maps);
          fallthrough_control = nullptr;

          // Don't insert a MapGuard in this case, as the CheckMaps
          // node already gives you all the information you need
          // along the effect chain.
          insert_map_guard = false;
        } else {
          // Explicitly branch on the {receiver_maps}.
          ZoneHandleSet<Map> maps;
          for (Handle<Map> map : receiver_maps) {
            maps.insert(map, graph()->zone());
          }
          Node* check = this_effect =
              graph()->NewNode(simplified()->CompareMaps(maps), receiver,
                               this_effect, this_control);
          Node* branch =
              graph()->NewNode(common()->Branch(), check, this_control);
          fallthrough_control = graph()->NewNode(common()->IfFalse(), branch);
          this_control = graph()->NewNode(common()->IfTrue(), branch);
        }

        // The Number case requires special treatment to also deal with Smis.
        if (HasNumberMaps(broker(), receiver_maps)) {
          // Join this check with the "receiver is smi" check above.
          DCHECK_NOT_NULL(receiverissmi_effect);
          DCHECK_NOT_NULL(receiverissmi_control);
          this_control = graph()->NewNode(common()->Merge(2), this_control,
                                          receiverissmi_control);
          this_effect = graph()->NewNode(common()->EffectPhi(2), this_effect,
                                         receiverissmi_effect, this_control);
          receiverissmi_effect = receiverissmi_control = nullptr;

          // The {receiver} can also be a Smi in this case, so
          // a MapGuard doesn't make sense for this at all.
          insert_map_guard = false;
        }

        // Introduce a MapGuard to learn from this on the effect chain.
        if (insert_map_guard) {
          ZoneHandleSet<Map> maps;
          for (auto receiver_map : receiver_maps) {
            maps.insert(receiver_map, graph()->zone());
          }
          this_effect = graph()->NewNode(simplified()->MapGuard(maps), receiver,
                                         this_effect, this_control);
        }

        // If all {receiver_maps} are Strings we also need to rename the
        // {receiver} here to make sure that TurboFan knows that along this
        // path the {this_receiver} is a String. This is because we want
        // strict checking of types, for example for StringLength operators.
        if (HasOnlyStringMaps(broker(), receiver_maps)) {
          this_receiver = this_effect =
              graph()->NewNode(common()->TypeGuard(Type::String()), receiver,
                               this_effect, this_control);
        }
      }

      // Generate the actual property access.
      ValueEffectControl continuation =
          BuildPropertyAccess(this_receiver, this_value, context, frame_state,
                              this_effect, this_control, feedback.name(),
                              if_exceptions, access_info, access_mode);
      values.push_back(continuation.value());
      effects.push_back(continuation.effect());
      controls.push_back(continuation.control());
    }

    DCHECK_NULL(fallthrough_control);

    // Generate the final merge point for all (polymorphic) branches.
    int const control_count = static_cast<int>(controls.size());
    if (control_count == 0) {
      value = effect = control = jsgraph()->Dead();
    } else if (control_count == 1) {
      value = values.front();
      effect = effects.front();
      control = controls.front();
    } else {
      control = graph()->NewNode(common()->Merge(control_count), control_count,
                                 &controls.front());
      values.push_back(control);
      value = graph()->NewNode(
          common()->Phi(MachineRepresentation::kTagged, control_count),
          control_count + 1, &values.front());
      effects.push_back(control);
      effect = graph()->NewNode(common()->EffectPhi(control_count),
                                control_count + 1, &effects.front());
    }
  }

  // Properly rewire IfException edges if {node} is inside a try-block.
  if (!if_exception_nodes.empty()) {
    DCHECK_NOT_NULL(if_exception);
    DCHECK_EQ(if_exceptions, &if_exception_nodes);
    int const if_exception_count = static_cast<int>(if_exceptions->size());
    Node* merge = graph()->NewNode(common()->Merge(if_exception_count),
                                   if_exception_count, &if_exceptions->front());
    if_exceptions->push_back(merge);
    Node* ephi =
        graph()->NewNode(common()->EffectPhi(if_exception_count),
                         if_exception_count + 1, &if_exceptions->front());
    Node* phi = graph()->NewNode(
        common()->Phi(MachineRepresentation::kTagged, if_exception_count),
        if_exception_count + 1, &if_exceptions->front());
    ReplaceWithValue(if_exception, phi, ephi, merge);
  }

  ReplaceWithValue(node, value, effect, control);
  return Replace(value);
}

Reduction JSNativeContextSpecialization::ReduceNamedAccessFromNexus(
    Node* node, Node* value, FeedbackNexus const& nexus, NameRef const& name,
    AccessMode access_mode) {
  DCHECK(node->opcode() == IrOpcode::kJSLoadNamed ||
         node->opcode() == IrOpcode::kJSStoreNamed ||
         node->opcode() == IrOpcode::kJSStoreNamedOwn);
  Node* const receiver = NodeProperties::GetValueInput(node, 0);

  // Optimize accesses to the current native context's global proxy.
  HeapObjectMatcher m(receiver);
  if (m.HasValue() &&
      m.Ref(broker()).equals(native_context().global_proxy_object())) {
    return ReduceGlobalAccess(node, nullptr, value, name, access_mode);
  }

  return ReducePropertyAccessUsingProcessedFeedback(node, nullptr, name, value,
                                                    nexus, access_mode);
}

Reduction JSNativeContextSpecialization::ReduceJSLoadNamed(Node* node) {
  DCHECK_EQ(IrOpcode::kJSLoadNamed, node->opcode());
  NamedAccess const& p = NamedAccessOf(node->op());
  Node* const receiver = NodeProperties::GetValueInput(node, 0);
  NameRef name(broker(), p.name());

  // Check if we have a constant receiver.
  HeapObjectMatcher m(receiver);
  if (m.HasValue()) {
    ObjectRef object = m.Ref(broker());
    if (object.IsJSFunction() &&
        name.equals(ObjectRef(broker(), factory()->prototype_string()))) {
      // Optimize "prototype" property of functions.
      JSFunctionRef function = object.AsJSFunction();
      if (!FLAG_concurrent_inlining) {
        function.Serialize();
      } else if (!function.serialized()) {
        TRACE_BROKER_MISSING(broker(), "data for function " << function);
        return NoChange();
      }
      // TODO(neis): Remove the has_prototype_slot condition once the broker is
      // always enabled.
      if (!function.map().has_prototype_slot() || !function.has_prototype() ||
          function.PrototypeRequiresRuntimeLookup()) {
        return NoChange();
      }
      ObjectRef prototype = dependencies()->DependOnPrototypeProperty(function);
      Node* value = jsgraph()->Constant(prototype);
      ReplaceWithValue(node, value);
      return Replace(value);
    } else if (object.IsString() &&
               name.equals(ObjectRef(broker(), factory()->length_string()))) {
      // Constant-fold "length" property on constant strings.
      Node* value = jsgraph()->Constant(object.AsString().length());
      ReplaceWithValue(node, value);
      return Replace(value);
    }
  }

  // Extract receiver maps from the load IC using the FeedbackNexus.
  if (!p.feedback().IsValid()) return NoChange();
  FeedbackNexus nexus(p.feedback().vector(), p.feedback().slot());

  // Try to lower the named access based on the {receiver_maps}.
  return ReduceNamedAccessFromNexus(node, jsgraph()->Dead(), nexus, name,
                                    AccessMode::kLoad);
}

Reduction JSNativeContextSpecialization::ReduceJSStoreNamed(Node* node) {
  DCHECK_EQ(IrOpcode::kJSStoreNamed, node->opcode());
  NamedAccess const& p = NamedAccessOf(node->op());
  Node* const value = NodeProperties::GetValueInput(node, 1);

  // Extract receiver maps from the store IC using the FeedbackNexus.
  if (!p.feedback().IsValid()) return NoChange();
  FeedbackNexus nexus(p.feedback().vector(), p.feedback().slot());

  // Try to lower the named access based on the {receiver_maps}.
  return ReduceNamedAccessFromNexus(
      node, value, nexus, NameRef(broker(), p.name()), AccessMode::kStore);
}

Reduction JSNativeContextSpecialization::ReduceJSStoreNamedOwn(Node* node) {
  DCHECK_EQ(IrOpcode::kJSStoreNamedOwn, node->opcode());
  StoreNamedOwnParameters const& p = StoreNamedOwnParametersOf(node->op());
  Node* const value = NodeProperties::GetValueInput(node, 1);

  // Extract receiver maps from the IC using the FeedbackNexus.
  if (!p.feedback().IsValid()) return NoChange();
  FeedbackNexus nexus(p.feedback().vector(), p.feedback().slot());

  // Try to lower the creation of a named property based on the {receiver_maps}.
  return ReduceNamedAccessFromNexus(node, value, nexus,
                                    NameRef(broker(), p.name()),
                                    AccessMode::kStoreInLiteral);
}

Reduction JSNativeContextSpecialization::ReduceElementAccessOnString(
    Node* node, Node* index, Node* value, AccessMode access_mode,
    KeyedAccessLoadMode load_mode) {
  Node* receiver = NodeProperties::GetValueInput(node, 0);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // Strings are immutable in JavaScript.
  if (access_mode == AccessMode::kStore) return NoChange();

  // `in` cannot be used on strings.
  if (access_mode == AccessMode::kHas) return NoChange();

  // Ensure that the {receiver} is actually a String.
  receiver = effect = graph()->NewNode(
      simplified()->CheckString(VectorSlotPair()), receiver, effect, control);

  // Determine the {receiver} length.
  Node* length = graph()->NewNode(simplified()->StringLength(), receiver);

  // Load the single character string from {receiver} or yield undefined
  // if the {index} is out of bounds (depending on the {load_mode}).
  value = BuildIndexedStringLoad(receiver, index, length, &effect, &control,
                                 load_mode);

  ReplaceWithValue(node, value, effect, control);
  return Replace(value);
}

namespace {
base::Optional<JSTypedArrayRef> GetTypedArrayConstant(JSHeapBroker* broker,
                                                      Node* receiver) {
  HeapObjectMatcher m(receiver);
  if (!m.HasValue()) return base::nullopt;
  ObjectRef object = m.Ref(broker);
  if (!object.IsJSTypedArray()) return base::nullopt;
  JSTypedArrayRef typed_array = object.AsJSTypedArray();
  if (typed_array.is_on_heap()) return base::nullopt;
  return typed_array;
}
}  // namespace

Reduction JSNativeContextSpecialization::ReduceElementAccess(
    Node* node, Node* index, Node* value,
    ElementAccessFeedback const& processed, AccessMode access_mode,
    KeyedAccessLoadMode load_mode, KeyedAccessStoreMode store_mode) {
  DisallowHeapAccessIf no_heap_access(FLAG_concurrent_inlining);

  DCHECK(node->opcode() == IrOpcode::kJSLoadProperty ||
         node->opcode() == IrOpcode::kJSStoreProperty ||
         node->opcode() == IrOpcode::kJSStoreInArrayLiteral ||
         node->opcode() == IrOpcode::kJSHasProperty);
  Node* receiver = NodeProperties::GetValueInput(node, 0);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* frame_state =
      NodeProperties::FindFrameStateBefore(node, jsgraph()->Dead());

  if (HasOnlyStringMaps(broker(), processed.receiver_maps)) {
    DCHECK(processed.transitions.empty());
    return ReduceElementAccessOnString(node, index, value, access_mode,
                                       load_mode);
  }

  // Compute element access infos for the receiver maps.
  AccessInfoFactory access_info_factory(broker(), dependencies(),
                                        graph()->zone());
  ZoneVector<ElementAccessInfo> access_infos(zone());
  if (!access_info_factory.ComputeElementAccessInfos(processed, access_mode,
                                                     &access_infos)) {
    return NoChange();
  } else if (access_infos.empty()) {
    return ReduceSoftDeoptimize(
        node, DeoptimizeReason::kInsufficientTypeFeedbackForGenericKeyedAccess);
  }

  // For holey stores or growing stores, we need to check that the prototype
  // chain contains no setters for elements, and we need to guard those checks
  // via code dependencies on the relevant prototype maps.
  if (access_mode == AccessMode::kStore) {
    // TODO(turbofan): We could have a fast path here, that checks for the
    // common case of Array or Object prototype only and therefore avoids
    // the zone allocation of this vector.
    ZoneVector<MapRef> prototype_maps(zone());
    for (ElementAccessInfo const& access_info : access_infos) {
      for (Handle<Map> map : access_info.receiver_maps()) {
        MapRef receiver_map(broker(), map);
        // If the {receiver_map} has a prototype and its elements backing
        // store is either holey, or we have a potentially growing store,
        // then we need to check that all prototypes have stable maps with
        // fast elements (and we need to guard against changes to that below).
        if ((IsHoleyOrDictionaryElementsKind(receiver_map.elements_kind()) ||
             IsGrowStoreMode(store_mode)) &&
            !receiver_map.HasOnlyStablePrototypesWithFastElements(
                &prototype_maps)) {
          return NoChange();
        }
      }
    }
    for (MapRef const& prototype_map : prototype_maps) {
      dependencies()->DependOnStableMap(prototype_map);
    }
  } else if (access_mode == AccessMode::kHas) {
    // If we have any fast arrays, we need to check and depend on
    // NoElementsProtector.
    for (ElementAccessInfo const& access_info : access_infos) {
      if (IsFastElementsKind(access_info.elements_kind())) {
        if (!isolate()->IsNoElementsProtectorIntact()) return NoChange();
        dependencies()->DependOnProtector(
            PropertyCellRef(broker(), factory()->no_elements_protector()));
        break;
      }
    }
  }

  // Check if we have the necessary data for building element accesses.
  for (ElementAccessInfo const& access_info : access_infos) {
    if (!IsTypedArrayElementsKind(access_info.elements_kind())) continue;
    base::Optional<JSTypedArrayRef> typed_array =
        GetTypedArrayConstant(broker(), receiver);
    if (typed_array.has_value()) {
      if (!FLAG_concurrent_inlining) {
        typed_array->Serialize();
      } else if (!typed_array->serialized()) {
        TRACE_BROKER_MISSING(broker(), "data for typed array " << *typed_array);
        return NoChange();
      }
    }
  }

  // Check for the monomorphic case.
  PropertyAccessBuilder access_builder(jsgraph(), broker(), dependencies());
  if (access_infos.size() == 1) {
    ElementAccessInfo access_info = access_infos.front();

    // Perform possible elements kind transitions.
    MapRef transition_target(broker(), access_info.receiver_maps().front());
    for (auto source : access_info.transition_sources()) {
      DCHECK_EQ(access_info.receiver_maps().size(), 1);
      MapRef transition_source(broker(), source);
      effect = graph()->NewNode(
          simplified()->TransitionElementsKind(ElementsTransition(
              IsSimpleMapChangeTransition(transition_source.elements_kind(),
                                          transition_target.elements_kind())
                  ? ElementsTransition::kFastTransition
                  : ElementsTransition::kSlowTransition,
              transition_source.object(), transition_target.object())),
          receiver, effect, control);
    }

    // TODO(turbofan): The effect/control linearization will not find a
    // FrameState after the StoreField or Call that is generated for the
    // elements kind transition above. This is because those operators
    // don't have the kNoWrite flag on it, even though they are not
    // observable by JavaScript.
    effect =
        graph()->NewNode(common()->Checkpoint(), frame_state, effect, control);

    // Perform map check on the {receiver}.
    access_builder.BuildCheckMaps(receiver, &effect, control,
                                  access_info.receiver_maps());

    // Access the actual element.
    ValueEffectControl continuation =
        BuildElementAccess(receiver, index, value, effect, control, access_info,
                           access_mode, load_mode, store_mode);
    value = continuation.value();
    effect = continuation.effect();
    control = continuation.control();
  } else {
    // The final states for every polymorphic branch. We join them with
    // Merge+Phi+EffectPhi at the bottom.
    ZoneVector<Node*> values(zone());
    ZoneVector<Node*> effects(zone());
    ZoneVector<Node*> controls(zone());

    // Generate code for the various different element access patterns.
    Node* fallthrough_control = control;
    for (size_t j = 0; j < access_infos.size(); ++j) {
      ElementAccessInfo const& access_info = access_infos[j];
      Node* this_receiver = receiver;
      Node* this_value = value;
      Node* this_index = index;
      Node* this_effect = effect;
      Node* this_control = fallthrough_control;

      // Perform possible elements kind transitions.
      MapRef transition_target(broker(), access_info.receiver_maps().front());
      for (auto source : access_info.transition_sources()) {
        MapRef transition_source(broker(), source);
        DCHECK_EQ(access_info.receiver_maps().size(), 1);
        this_effect = graph()->NewNode(
            simplified()->TransitionElementsKind(ElementsTransition(
                IsSimpleMapChangeTransition(transition_source.elements_kind(),
                                            transition_target.elements_kind())
                    ? ElementsTransition::kFastTransition
                    : ElementsTransition::kSlowTransition,
                transition_source.object(), transition_target.object())),
            receiver, effect, control);
      }

      // Perform map check(s) on {receiver}.
      ZoneVector<Handle<Map>> const& receiver_maps =
          access_info.receiver_maps();
      if (j == access_infos.size() - 1) {
        // Last map check on the fallthrough control path, do a
        // conditional eager deoptimization exit here.
        access_builder.BuildCheckMaps(receiver, &this_effect, this_control,
                                      receiver_maps);
        fallthrough_control = nullptr;
      } else {
        // Explicitly branch on the {receiver_maps}.
        ZoneHandleSet<Map> maps;
        for (Handle<Map> map : receiver_maps) {
          maps.insert(map, graph()->zone());
        }
        Node* check = this_effect =
            graph()->NewNode(simplified()->CompareMaps(maps), receiver,
                             this_effect, fallthrough_control);
        Node* branch =
            graph()->NewNode(common()->Branch(), check, fallthrough_control);
        fallthrough_control = graph()->NewNode(common()->IfFalse(), branch);
        this_control = graph()->NewNode(common()->IfTrue(), branch);

        // Introduce a MapGuard to learn from this on the effect chain.
        this_effect = graph()->NewNode(simplified()->MapGuard(maps), receiver,
                                       this_effect, this_control);
      }

      // Access the actual element.
      ValueEffectControl continuation = BuildElementAccess(
          this_receiver, this_index, this_value, this_effect, this_control,
          access_info, access_mode, load_mode, store_mode);
      values.push_back(continuation.value());
      effects.push_back(continuation.effect());
      controls.push_back(continuation.control());
    }

    DCHECK_NULL(fallthrough_control);

    // Generate the final merge point for all (polymorphic) branches.
    int const control_count = static_cast<int>(controls.size());
    if (control_count == 0) {
      value = effect = control = jsgraph()->Dead();
    } else if (control_count == 1) {
      value = values.front();
      effect = effects.front();
      control = controls.front();
    } else {
      control = graph()->NewNode(common()->Merge(control_count), control_count,
                                 &controls.front());
      values.push_back(control);
      value = graph()->NewNode(
          common()->Phi(MachineRepresentation::kTagged, control_count),
          control_count + 1, &values.front());
      effects.push_back(control);
      effect = graph()->NewNode(common()->EffectPhi(control_count),
                                control_count + 1, &effects.front());
    }
  }

  ReplaceWithValue(node, value, effect, control);
  return Replace(value);
}

Reduction JSNativeContextSpecialization::ReduceKeyedLoadFromHeapConstant(
    Node* node, Node* key, FeedbackNexus const& nexus, AccessMode access_mode,
    KeyedAccessLoadMode load_mode) {
  DCHECK(node->opcode() == IrOpcode::kJSLoadProperty ||
         node->opcode() == IrOpcode::kJSHasProperty);
  Node* receiver = NodeProperties::GetValueInput(node, 0);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  HeapObjectMatcher mreceiver(receiver);
  HeapObjectRef receiver_ref = mreceiver.Ref(broker());
  if (receiver_ref.map().oddball_type() == OddballType::kHole ||
      receiver_ref.map().oddball_type() == OddballType::kNull ||
      receiver_ref.map().oddball_type() == OddballType::kUndefined ||
      // The 'in' operator throws a TypeError on primitive values.
      (receiver_ref.IsString() && access_mode == AccessMode::kHas)) {
    return NoChange();
  }

  // Check whether we're accessing a known element on the {receiver} and can
  // constant-fold the load.
  NumberMatcher mkey(key);
  if (mkey.IsInteger() && mkey.IsInRange(0.0, kMaxUInt32 - 1.0)) {
    uint32_t index = static_cast<uint32_t>(mkey.Value());
    base::Optional<ObjectRef> element =
        receiver_ref.GetOwnConstantElement(index);
    if (!element.has_value() && receiver_ref.IsJSArray()) {
      // We didn't find a constant element, but if the receiver is a cow-array
      // we can exploit the fact that any future write to the element will
      // replace the whole elements storage.
      element = receiver_ref.AsJSArray().GetOwnCowElement(index);
      if (element.has_value()) {
        Node* elements = effect = graph()->NewNode(
            simplified()->LoadField(AccessBuilder::ForJSObjectElements()),
            receiver, effect, control);
        FixedArrayRef array_elements =
            receiver_ref.AsJSArray().elements().AsFixedArray();
        Node* check = graph()->NewNode(simplified()->ReferenceEqual(), elements,
                                       jsgraph()->Constant(array_elements));
        effect = graph()->NewNode(
            simplified()->CheckIf(DeoptimizeReason::kCowArrayElementsChanged),
            check, effect, control);
      }
    }
    if (element.has_value()) {
      Node* value = access_mode == AccessMode::kHas
                        ? jsgraph()->TrueConstant()
                        : jsgraph()->Constant(*element);
      ReplaceWithValue(node, value, effect, control);
      return Replace(value);
    }
  }

  // For constant Strings we can eagerly strength-reduce the keyed
  // accesses using the known length, which doesn't change.
  if (receiver_ref.IsString()) {
    DCHECK_NE(access_mode, AccessMode::kHas);
    // We can only assume that the {index} is a valid array index if the
    // IC is in element access mode and not MEGAMORPHIC, otherwise there's
    // no guard for the bounds check below.
    if (nexus.ic_state() != MEGAMORPHIC && nexus.GetKeyType() == ELEMENT) {
      // Ensure that {key} is less than {receiver} length.
      Node* length = jsgraph()->Constant(receiver_ref.AsString().length());

      // Load the single character string from {receiver} or yield
      // undefined if the {key} is out of bounds (depending on the
      // {load_mode}).
      Node* value = BuildIndexedStringLoad(receiver, key, length, &effect,
                                           &control, load_mode);
      ReplaceWithValue(node, value, effect, control);
      return Replace(value);
    }
  }

  return NoChange();
}

Reduction JSNativeContextSpecialization::ReduceKeyedAccess(
    Node* node, Node* key, Node* value, FeedbackNexus const& nexus,
    AccessMode access_mode, KeyedAccessLoadMode load_mode,
    KeyedAccessStoreMode store_mode) {
  DCHECK(node->opcode() == IrOpcode::kJSLoadProperty ||
         node->opcode() == IrOpcode::kJSStoreProperty ||
         node->opcode() == IrOpcode::kJSStoreInArrayLiteral ||
         node->opcode() == IrOpcode::kJSHasProperty);

  Node* receiver = NodeProperties::GetValueInput(node, 0);

  if ((access_mode == AccessMode::kLoad || access_mode == AccessMode::kHas) &&
      receiver->opcode() == IrOpcode::kHeapConstant) {
    Reduction reduction = ReduceKeyedLoadFromHeapConstant(
        node, key, nexus, access_mode, load_mode);
    if (reduction.Changed()) return reduction;
  }

  return ReducePropertyAccessUsingProcessedFeedback(node, key, base::nullopt,
                                                    value, nexus, access_mode,
                                                    load_mode, store_mode);
}

Reduction
JSNativeContextSpecialization::ReducePropertyAccessUsingProcessedFeedback(
    Node* node, Node* key, base::Optional<NameRef> static_name, Node* value,
    FeedbackNexus const& nexus, AccessMode access_mode,
    KeyedAccessLoadMode load_mode, KeyedAccessStoreMode store_mode) {
  DCHECK_EQ(key == nullptr, static_name.has_value());
  DCHECK(node->opcode() == IrOpcode::kJSLoadProperty ||
         node->opcode() == IrOpcode::kJSStoreProperty ||
         node->opcode() == IrOpcode::kJSStoreInArrayLiteral ||
         node->opcode() == IrOpcode::kJSHasProperty ||
         node->opcode() == IrOpcode::kJSLoadNamed ||
         node->opcode() == IrOpcode::kJSStoreNamed ||
         node->opcode() == IrOpcode::kJSStoreNamedOwn);

  Node* receiver = NodeProperties::GetValueInput(node, 0);
  Node* effect = NodeProperties::GetEffectInput(node);

  ProcessedFeedback const* processed = nullptr;
  if (FLAG_concurrent_inlining) {
    processed = broker()->GetFeedback(FeedbackSource(nexus));
    // TODO(neis): Infer maps from the graph and consolidate with feedback/hints
    // and filter impossible candidates based on inferred root map.
  } else {
    // TODO(neis): Try to unify this with the similar code in the serializer.
    if (nexus.ic_state() == UNINITIALIZED) {
      processed = new (zone()) InsufficientFeedback();
    } else {
      MapHandles receiver_maps;
      if (!ExtractReceiverMaps(receiver, effect, nexus, &receiver_maps)) {
        processed = new (zone()) InsufficientFeedback();
      } else if (!receiver_maps.empty()) {
        base::Optional<NameRef> name = static_name.has_value()
                                           ? static_name
                                           : broker()->GetNameFeedback(nexus);
        if (name.has_value()) {
          ZoneVector<PropertyAccessInfo> access_infos(zone());
          AccessInfoFactory access_info_factory(broker(), dependencies(),
                                                zone());
          access_info_factory.ComputePropertyAccessInfos(
              receiver_maps, name->object(), access_mode, &access_infos);
          processed = new (zone()) NamedAccessFeedback(*name, access_infos);
        } else if (nexus.GetKeyType() == ELEMENT &&
                   MEGAMORPHIC != nexus.ic_state()) {
          processed =
              broker()->ProcessFeedbackMapsForElementAccess(receiver_maps);
        }
      }
    }
  }

  if (processed == nullptr) return NoChange();
  switch (processed->kind()) {
    case ProcessedFeedback::kInsufficient:
      return ReduceSoftDeoptimize(
          node,
          DeoptimizeReason::kInsufficientTypeFeedbackForGenericNamedAccess);
    case ProcessedFeedback::kNamedAccess:
      return ReduceNamedAccess(node, value, *processed->AsNamedAccess(),
                               access_mode, key);
    case ProcessedFeedback::kElementAccess:
      return ReduceElementAccess(node, key, value,
                                 *processed->AsElementAccess(), access_mode,
                                 load_mode, store_mode);
    case ProcessedFeedback::kGlobalAccess:
      UNREACHABLE();
  }
}

Reduction JSNativeContextSpecialization::ReduceSoftDeoptimize(
    Node* node, DeoptimizeReason reason) {
  if (!(flags() & kBailoutOnUninitialized)) return NoChange();

  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* frame_state =
      NodeProperties::FindFrameStateBefore(node, jsgraph()->Dead());
  Node* deoptimize = graph()->NewNode(
      common()->Deoptimize(DeoptimizeKind::kSoft, reason, VectorSlotPair()),
      frame_state, effect, control);
  // TODO(bmeurer): This should be on the AdvancedReducer somehow.
  NodeProperties::MergeControlToEnd(graph(), common(), deoptimize);
  Revisit(graph()->end());
  node->TrimInputCount(0);
  NodeProperties::ChangeOp(node, common()->Dead());
  return Changed(node);
}

Reduction JSNativeContextSpecialization::ReduceJSHasProperty(Node* node) {
  DCHECK_EQ(IrOpcode::kJSHasProperty, node->opcode());
  PropertyAccess const& p = PropertyAccessOf(node->op());
  Node* key = NodeProperties::GetValueInput(node, 1);
  Node* value = jsgraph()->Dead();

  // Extract receiver maps from the has property IC using the FeedbackNexus.
  if (!p.feedback().IsValid()) return NoChange();
  FeedbackNexus nexus(p.feedback().vector(), p.feedback().slot());

  // Extract the keyed access load mode from the keyed load IC.
  KeyedAccessLoadMode load_mode = nexus.GetKeyedAccessLoadMode();

  // Try to lower the keyed access based on the {nexus}.
  return ReduceKeyedAccess(node, key, value, nexus, AccessMode::kHas, load_mode,
                           STANDARD_STORE);
}

Reduction JSNativeContextSpecialization::ReduceJSLoadPropertyWithEnumeratedKey(
    Node* node) {
  // We can optimize a property load if it's being used inside a for..in:
  //   for (name in receiver) {
  //     value = receiver[name];
  //     ...
  //   }
  //
  // If the for..in is in fast-mode, we know that the {receiver} has {name}
  // as own property, otherwise the enumeration wouldn't include it. The graph
  // constructed by the BytecodeGraphBuilder in this case looks like this:

  // receiver
  //  ^    ^
  //  |    |
  //  |    +-+
  //  |      |
  //  |   JSToObject
  //  |      ^
  //  |      |
  //  |      |
  //  |  JSForInNext
  //  |      ^
  //  |      |
  //  +----+ |
  //       | |
  //       | |
  //   JSLoadProperty

  // If the for..in has only seen maps with enum cache consisting of keys
  // and indices so far, we can turn the {JSLoadProperty} into a map check
  // on the {receiver} and then just load the field value dynamically via
  // the {LoadFieldByIndex} operator. The map check is only necessary when
  // TurboFan cannot prove that there is no observable side effect between
  // the {JSForInNext} and the {JSLoadProperty} node.
  //
  // Also note that it's safe to look through the {JSToObject}, since the
  // [[Get]] operation does an implicit ToObject anyway, and these operations
  // are not observable.

  DCHECK_EQ(IrOpcode::kJSLoadProperty, node->opcode());
  Node* receiver = NodeProperties::GetValueInput(node, 0);
  Node* name = NodeProperties::GetValueInput(node, 1);
  DCHECK_EQ(IrOpcode::kJSForInNext, name->opcode());
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  if (ForInModeOf(name->op()) != ForInMode::kUseEnumCacheKeysAndIndices) {
    return NoChange();
  }

  Node* object = NodeProperties::GetValueInput(name, 0);
  Node* enumerator = NodeProperties::GetValueInput(name, 2);
  Node* key = NodeProperties::GetValueInput(name, 3);
  if (object->opcode() == IrOpcode::kJSToObject) {
    object = NodeProperties::GetValueInput(object, 0);
  }
  if (object != receiver) return NoChange();

  // No need to repeat the map check if we can prove that there's no
  // observable side effect between {effect} and {name].
  if (!NodeProperties::NoObservableSideEffectBetween(effect, name)) {
    // Check that the {receiver} map is still valid.
    Node* receiver_map = effect =
        graph()->NewNode(simplified()->LoadField(AccessBuilder::ForMap()),
                         receiver, effect, control);
    Node* check = graph()->NewNode(simplified()->ReferenceEqual(), receiver_map,
                                   enumerator);
    effect =
        graph()->NewNode(simplified()->CheckIf(DeoptimizeReason::kWrongMap),
                         check, effect, control);
  }

  // Load the enum cache indices from the {cache_type}.
  Node* descriptor_array = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForMapDescriptors()), enumerator,
      effect, control);
  Node* enum_cache = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForDescriptorArrayEnumCache()),
      descriptor_array, effect, control);
  Node* enum_indices = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForEnumCacheIndices()), enum_cache,
      effect, control);

  // Ensure that the {enum_indices} are valid.
  Node* check = graph()->NewNode(
      simplified()->BooleanNot(),
      graph()->NewNode(simplified()->ReferenceEqual(), enum_indices,
                       jsgraph()->EmptyFixedArrayConstant()));
  effect = graph()->NewNode(
      simplified()->CheckIf(DeoptimizeReason::kWrongEnumIndices), check, effect,
      control);

  // Determine the key from the {enum_indices}.
  key = effect = graph()->NewNode(
      simplified()->LoadElement(
          AccessBuilder::ForFixedArrayElement(PACKED_SMI_ELEMENTS)),
      enum_indices, key, effect, control);

  // Load the actual field value.
  Node* value = effect = graph()->NewNode(simplified()->LoadFieldByIndex(),
                                          receiver, key, effect, control);
  ReplaceWithValue(node, value, effect, control);
  return Replace(value);
}

Reduction JSNativeContextSpecialization::ReduceJSLoadProperty(Node* node) {
  DCHECK_EQ(IrOpcode::kJSLoadProperty, node->opcode());
  PropertyAccess const& p = PropertyAccessOf(node->op());
  Node* name = NodeProperties::GetValueInput(node, 1);

  if (name->opcode() == IrOpcode::kJSForInNext) {
    Reduction reduction = ReduceJSLoadPropertyWithEnumeratedKey(node);
    if (reduction.Changed()) return reduction;
  }

  // Extract receiver maps from the keyed load IC using the FeedbackNexus.
  if (!p.feedback().IsValid()) return NoChange();
  FeedbackNexus nexus(p.feedback().vector(), p.feedback().slot());

  // Extract the keyed access load mode from the keyed load IC.
  KeyedAccessLoadMode load_mode = nexus.GetKeyedAccessLoadMode();

  // Try to lower the keyed access based on the {nexus}.
  Node* value = jsgraph()->Dead();
  return ReduceKeyedAccess(node, name, value, nexus, AccessMode::kLoad,
                           load_mode, STANDARD_STORE);
}

Reduction JSNativeContextSpecialization::ReduceJSStoreProperty(Node* node) {
  DCHECK_EQ(IrOpcode::kJSStoreProperty, node->opcode());
  PropertyAccess const& p = PropertyAccessOf(node->op());
  Node* const key = NodeProperties::GetValueInput(node, 1);
  Node* const value = NodeProperties::GetValueInput(node, 2);

  // Extract receiver maps from the keyed store IC using the FeedbackNexus.
  if (!p.feedback().IsValid()) return NoChange();
  FeedbackNexus nexus(p.feedback().vector(), p.feedback().slot());

  // Extract the keyed access store mode from the keyed store IC.
  KeyedAccessStoreMode store_mode = nexus.GetKeyedAccessStoreMode();

  // Try to lower the keyed access based on the {nexus}.
  return ReduceKeyedAccess(node, key, value, nexus, AccessMode::kStore,
                           STANDARD_LOAD, store_mode);
}

Node* JSNativeContextSpecialization::InlinePropertyGetterCall(
    Node* receiver, Node* context, Node* frame_state, Node** effect,
    Node** control, ZoneVector<Node*>* if_exceptions,
    PropertyAccessInfo const& access_info) {
  Node* target = jsgraph()->Constant(access_info.constant());
  FrameStateInfo const& frame_info = FrameStateInfoOf(frame_state->op());
  // Introduce the call to the getter function.
  Node* value;
  ObjectRef constant(broker(), access_info.constant());
  if (constant.IsJSFunction()) {
    value = *effect = *control = graph()->NewNode(
        jsgraph()->javascript()->Call(2, CallFrequency(), VectorSlotPair(),
                                      ConvertReceiverMode::kNotNullOrUndefined),
        target, receiver, context, frame_state, *effect, *control);
  } else {
    auto function_template_info = constant.AsFunctionTemplateInfo();
    function_template_info.Serialize();
    Node* holder =
        access_info.holder().is_null()
            ? receiver
            : jsgraph()->Constant(access_info.holder().ToHandleChecked());
    SharedFunctionInfoRef shared_info(
        broker(), frame_info.shared_info().ToHandleChecked());
    value = InlineApiCall(receiver, holder, frame_state, nullptr, effect,
                          control, shared_info, function_template_info);
  }
  // Remember to rewire the IfException edge if this is inside a try-block.
  if (if_exceptions != nullptr) {
    // Create the appropriate IfException/IfSuccess projections.
    Node* const if_exception =
        graph()->NewNode(common()->IfException(), *control, *effect);
    Node* const if_success = graph()->NewNode(common()->IfSuccess(), *control);
    if_exceptions->push_back(if_exception);
    *control = if_success;
  }
  return value;
}

void JSNativeContextSpecialization::InlinePropertySetterCall(
    Node* receiver, Node* value, Node* context, Node* frame_state,
    Node** effect, Node** control, ZoneVector<Node*>* if_exceptions,
    PropertyAccessInfo const& access_info) {
  Node* target = jsgraph()->Constant(access_info.constant());
  FrameStateInfo const& frame_info = FrameStateInfoOf(frame_state->op());
  // Introduce the call to the setter function.
  ObjectRef constant(broker(), access_info.constant());
  if (constant.IsJSFunction()) {
    *effect = *control = graph()->NewNode(
        jsgraph()->javascript()->Call(3, CallFrequency(), VectorSlotPair(),
                                      ConvertReceiverMode::kNotNullOrUndefined),
        target, receiver, value, context, frame_state, *effect, *control);
  } else {
    auto function_template_info = constant.AsFunctionTemplateInfo();
    function_template_info.Serialize();
    Node* holder =
        access_info.holder().is_null()
            ? receiver
            : jsgraph()->Constant(access_info.holder().ToHandleChecked());
    SharedFunctionInfoRef shared_info(
        broker(), frame_info.shared_info().ToHandleChecked());
    InlineApiCall(receiver, holder, frame_state, value, effect, control,
                  shared_info, function_template_info);
  }
  // Remember to rewire the IfException edge if this is inside a try-block.
  if (if_exceptions != nullptr) {
    // Create the appropriate IfException/IfSuccess projections.
    Node* const if_exception =
        graph()->NewNode(common()->IfException(), *control, *effect);
    Node* const if_success = graph()->NewNode(common()->IfSuccess(), *control);
    if_exceptions->push_back(if_exception);
    *control = if_success;
  }
}

Node* JSNativeContextSpecialization::InlineApiCall(
    Node* receiver, Node* holder, Node* frame_state, Node* value, Node** effect,
    Node** control, SharedFunctionInfoRef const& shared_info,
    FunctionTemplateInfoRef const& function_template_info) {
  auto call_handler_info =
      function_template_info.call_code().AsCallHandlerInfo();

  // Only setters have a value.
  int const argc = value == nullptr ? 0 : 1;
  // The stub always expects the receiver as the first param on the stack.
  Callable call_api_callback = CodeFactory::CallApiCallback(isolate());
  CallInterfaceDescriptor call_interface_descriptor =
      call_api_callback.descriptor();
  auto call_descriptor = Linkage::GetStubCallDescriptor(
      graph()->zone(), call_interface_descriptor,
      call_interface_descriptor.GetStackParameterCount() + argc +
          1 /* implicit receiver */,
      CallDescriptor::kNeedsFrameState);

  Node* data = jsgraph()->Constant(call_handler_info.data());
  ApiFunction function(call_handler_info.callback());
  Node* function_reference =
      graph()->NewNode(common()->ExternalConstant(ExternalReference::Create(
          &function, ExternalReference::DIRECT_API_CALL)));
  Node* code = jsgraph()->HeapConstant(call_api_callback.code());

  // Add CallApiCallbackStub's register argument as well.
  Node* context = jsgraph()->Constant(native_context());
  Node* inputs[11] = {
      code,    function_reference, jsgraph()->Constant(argc), data, holder,
      receiver};
  int index = 6 + argc;
  inputs[index++] = context;
  inputs[index++] = frame_state;
  inputs[index++] = *effect;
  inputs[index++] = *control;
  // This needs to stay here because of the edge case described in
  // http://crbug.com/675648.
  if (value != nullptr) {
    inputs[6] = value;
  }

  return *effect = *control =
             graph()->NewNode(common()->Call(call_descriptor), index, inputs);
}

JSNativeContextSpecialization::ValueEffectControl
JSNativeContextSpecialization::BuildPropertyLoad(
    Node* receiver, Node* context, Node* frame_state, Node* effect,
    Node* control, NameRef const& name, ZoneVector<Node*>* if_exceptions,
    PropertyAccessInfo const& access_info) {
  // Determine actual holder and perform prototype chain checks.
  Handle<JSObject> holder;
  if (access_info.holder().ToHandle(&holder)) {
    dependencies()->DependOnStablePrototypeChains(
        access_info.receiver_maps(), kStartAtPrototype,
        JSObjectRef(broker(), holder));
  }

  // Generate the actual property access.
  Node* value;
  if (access_info.IsNotFound()) {
    value = jsgraph()->UndefinedConstant();
  } else if (access_info.IsAccessorConstant()) {
    value = InlinePropertyGetterCall(receiver, context, frame_state, &effect,
                                     &control, if_exceptions, access_info);
  } else if (access_info.IsModuleExport()) {
    Node* cell = jsgraph()->Constant(access_info.export_cell());
    value = effect =
        graph()->NewNode(simplified()->LoadField(AccessBuilder::ForCellValue()),
                         cell, effect, control);
  } else if (access_info.IsStringLength()) {
    value = graph()->NewNode(simplified()->StringLength(), receiver);
  } else {
    DCHECK(access_info.IsDataField() || access_info.IsDataConstant());
    PropertyAccessBuilder access_builder(jsgraph(), broker(), dependencies());
    value = access_builder.BuildLoadDataField(name, access_info, receiver,
                                              &effect, &control);
  }

  return ValueEffectControl(value, effect, control);
}

JSNativeContextSpecialization::ValueEffectControl
JSNativeContextSpecialization::BuildPropertyTest(
    Node* effect, Node* control, PropertyAccessInfo const& access_info) {
  // Determine actual holder and perform prototype chain checks.
  Handle<JSObject> holder;
  if (access_info.holder().ToHandle(&holder)) {
    dependencies()->DependOnStablePrototypeChains(
        access_info.receiver_maps(), kStartAtPrototype,
        JSObjectRef(broker(), holder));
  }

  Node* value = access_info.IsNotFound() ? jsgraph()->FalseConstant()
                                         : jsgraph()->TrueConstant();
  return ValueEffectControl(value, effect, control);
}

JSNativeContextSpecialization::ValueEffectControl
JSNativeContextSpecialization::BuildPropertyAccess(
    Node* receiver, Node* value, Node* context, Node* frame_state, Node* effect,
    Node* control, NameRef const& name, ZoneVector<Node*>* if_exceptions,
    PropertyAccessInfo const& access_info, AccessMode access_mode) {
  switch (access_mode) {
    case AccessMode::kLoad:
      return BuildPropertyLoad(receiver, context, frame_state, effect, control,
                               name, if_exceptions, access_info);
    case AccessMode::kStore:
    case AccessMode::kStoreInLiteral:
      return BuildPropertyStore(receiver, value, context, frame_state, effect,
                                control, name, if_exceptions, access_info,
                                access_mode);
    case AccessMode::kHas:
      return BuildPropertyTest(effect, control, access_info);
  }
  UNREACHABLE();
}

JSNativeContextSpecialization::ValueEffectControl
JSNativeContextSpecialization::BuildPropertyStore(
    Node* receiver, Node* value, Node* context, Node* frame_state, Node* effect,
    Node* control, NameRef const& name, ZoneVector<Node*>* if_exceptions,
    PropertyAccessInfo const& access_info, AccessMode access_mode) {
  // Determine actual holder and perform prototype chain checks.
  Handle<JSObject> holder;
  PropertyAccessBuilder access_builder(jsgraph(), broker(), dependencies());
  if (access_info.holder().ToHandle(&holder)) {
    DCHECK_NE(AccessMode::kStoreInLiteral, access_mode);
    dependencies()->DependOnStablePrototypeChains(
        access_info.receiver_maps(), kStartAtPrototype,
        JSObjectRef(broker(), holder));
  }

  DCHECK(!access_info.IsNotFound());

  // Generate the actual property access.
  if (access_info.IsAccessorConstant()) {
    InlinePropertySetterCall(receiver, value, context, frame_state, &effect,
                             &control, if_exceptions, access_info);
  } else {
    DCHECK(access_info.IsDataField() || access_info.IsDataConstant());
    DCHECK(access_mode == AccessMode::kStore ||
           access_mode == AccessMode::kStoreInLiteral);
    FieldIndex const field_index = access_info.field_index();
    Type const field_type = access_info.field_type();
    MachineRepresentation const field_representation =
        PropertyAccessBuilder::ConvertRepresentation(
            access_info.field_representation());
    Node* storage = receiver;
    if (!field_index.is_inobject()) {
      storage = effect = graph()->NewNode(
          simplified()->LoadField(AccessBuilder::ForJSObjectPropertiesOrHash()),
          storage, effect, control);
    }
    PropertyConstness constness = access_info.IsDataConstant()
                                      ? PropertyConstness::kConst
                                      : PropertyConstness::kMutable;
    bool store_to_existing_constant_field = access_info.IsDataConstant() &&
                                            access_mode == AccessMode::kStore &&
                                            !access_info.HasTransitionMap();
    FieldAccess field_access = {
        kTaggedBase,
        field_index.offset(),
        name.object(),
        MaybeHandle<Map>(),
        field_type,
        MachineType::TypeForRepresentation(field_representation),
        kFullWriteBarrier,
        LoadSensitivity::kUnsafe,
        constness};

    switch (field_representation) {
      case MachineRepresentation::kFloat64: {
        value = effect =
            graph()->NewNode(simplified()->CheckNumber(VectorSlotPair()), value,
                             effect, control);
        if (!field_index.is_inobject() || !FLAG_unbox_double_fields) {
          if (access_info.HasTransitionMap()) {
            // Allocate a MutableHeapNumber for the new property.
            AllocationBuilder a(jsgraph(), effect, control);
            a.Allocate(HeapNumber::kSize, AllocationType::kYoung,
                       Type::OtherInternal());
            a.Store(AccessBuilder::ForMap(),
                    factory()->mutable_heap_number_map());
            FieldAccess value_field_access =
                AccessBuilder::ForHeapNumberValue();
            value_field_access.constness = field_access.constness;
            a.Store(value_field_access, value);
            value = effect = a.Finish();

            field_access.type = Type::Any();
            field_access.machine_type =
                MachineType::TypeCompressedTaggedPointer();
            field_access.write_barrier_kind = kPointerWriteBarrier;
          } else {
            // We just store directly to the MutableHeapNumber.
            FieldAccess const storage_access = {
                kTaggedBase,
                field_index.offset(),
                name.object(),
                MaybeHandle<Map>(),
                Type::OtherInternal(),
                MachineType::TypeCompressedTaggedPointer(),
                kPointerWriteBarrier,
                LoadSensitivity::kUnsafe,
                constness};
            storage = effect =
                graph()->NewNode(simplified()->LoadField(storage_access),
                                 storage, effect, control);
            field_access.offset = HeapNumber::kValueOffset;
            field_access.name = MaybeHandle<Name>();
            field_access.machine_type = MachineType::Float64();
          }
        }
        if (store_to_existing_constant_field) {
          DCHECK(!access_info.HasTransitionMap());
          // If the field is constant check that the value we are going
          // to store matches current value.
          Node* current_value = effect = graph()->NewNode(
              simplified()->LoadField(field_access), storage, effect, control);

          Node* check =
              graph()->NewNode(simplified()->SameValue(), current_value, value);
          effect = graph()->NewNode(
              simplified()->CheckIf(DeoptimizeReason::kWrongValue), check,
              effect, control);
          return ValueEffectControl(value, effect, control);
        }
        break;
      }
      case MachineRepresentation::kTaggedSigned:
      case MachineRepresentation::kTaggedPointer:
      case MachineRepresentation::kTagged:
      case MachineRepresentation::kCompressedSigned:
      case MachineRepresentation::kCompressedPointer:
      case MachineRepresentation::kCompressed:
        if (store_to_existing_constant_field) {
          DCHECK(!access_info.HasTransitionMap());
          // If the field is constant check that the value we are going
          // to store matches current value.
          Node* current_value = effect = graph()->NewNode(
              simplified()->LoadField(field_access), storage, effect, control);

          Node* check = graph()->NewNode(simplified()->SameValueNumbersOnly(),
                                         current_value, value);
          effect = graph()->NewNode(
              simplified()->CheckIf(DeoptimizeReason::kWrongValue), check,
              effect, control);
          return ValueEffectControl(value, effect, control);
        }

        if (field_representation == MachineRepresentation::kTaggedSigned ||
            field_representation == MachineRepresentation::kCompressedSigned) {
          value = effect = graph()->NewNode(
              simplified()->CheckSmi(VectorSlotPair()), value, effect, control);
          field_access.write_barrier_kind = kNoWriteBarrier;

        } else if (field_representation ==
                       MachineRepresentation::kTaggedPointer ||
                   field_representation ==
                       MachineRepresentation::kCompressedPointer) {
          Handle<Map> field_map;
          if (access_info.field_map().ToHandle(&field_map)) {
            // Emit a map check for the value.
            effect = graph()->NewNode(
                simplified()->CheckMaps(CheckMapsFlag::kNone,
                                        ZoneHandleSet<Map>(field_map)),
                value, effect, control);
          } else {
            // Ensure that {value} is a HeapObject.
            value = effect = graph()->NewNode(simplified()->CheckHeapObject(),
                                              value, effect, control);
          }
          field_access.write_barrier_kind = kPointerWriteBarrier;

        } else {
          DCHECK(field_representation == MachineRepresentation::kTagged ||
                 field_representation == MachineRepresentation::kCompressed);
        }
        break;
      case MachineRepresentation::kNone:
      case MachineRepresentation::kBit:
      case MachineRepresentation::kWord8:
      case MachineRepresentation::kWord16:
      case MachineRepresentation::kWord32:
      case MachineRepresentation::kWord64:
      case MachineRepresentation::kFloat32:
      case MachineRepresentation::kSimd128:
        UNREACHABLE();
        break;
    }
    // Check if we need to perform a transitioning store.
    Handle<Map> transition_map;
    if (access_info.transition_map().ToHandle(&transition_map)) {
      // Check if we need to grow the properties backing store
      // with this transitioning store.
      MapRef transition_map_ref(broker(), transition_map);
      transition_map_ref.SerializeBackPointer();
      MapRef original_map = transition_map_ref.GetBackPointer().AsMap();
      if (original_map.UnusedPropertyFields() == 0) {
        DCHECK(!field_index.is_inobject());

        // Reallocate the properties {storage}.
        storage = effect = BuildExtendPropertiesBackingStore(
            original_map, storage, effect, control);

        // Perform the actual store.
        effect = graph()->NewNode(simplified()->StoreField(field_access),
                                  storage, value, effect, control);

        // Atomically switch to the new properties below.
        field_access = AccessBuilder::ForJSObjectPropertiesOrHash();
        value = storage;
        storage = receiver;
      }
      effect = graph()->NewNode(
          common()->BeginRegion(RegionObservability::kObservable), effect);
      effect = graph()->NewNode(
          simplified()->StoreField(AccessBuilder::ForMap()), receiver,
          jsgraph()->Constant(transition_map), effect, control);
      effect = graph()->NewNode(simplified()->StoreField(field_access), storage,
                                value, effect, control);
      effect = graph()->NewNode(common()->FinishRegion(),
                                jsgraph()->UndefinedConstant(), effect);
    } else {
      // Regular non-transitioning field store.
      effect = graph()->NewNode(simplified()->StoreField(field_access), storage,
                                value, effect, control);
    }
  }

  return ValueEffectControl(value, effect, control);
}

Reduction JSNativeContextSpecialization::ReduceJSStoreDataPropertyInLiteral(
    Node* node) {
  DCHECK_EQ(IrOpcode::kJSStoreDataPropertyInLiteral, node->opcode());

  FeedbackParameter const& p = FeedbackParameterOf(node->op());

  if (!p.feedback().IsValid()) return NoChange();

  FeedbackNexus nexus(p.feedback().vector(), p.feedback().slot());
  if (nexus.IsUninitialized()) {
    return NoChange();
  }

  if (nexus.ic_state() == MEGAMORPHIC) {
    return NoChange();
  }

  DCHECK_EQ(MONOMORPHIC, nexus.ic_state());

  Map map = nexus.GetFirstMap();
  if (map.is_null()) {
    // Maps are weakly held in the type feedback vector, we may not have one.
    return NoChange();
  }

  Handle<Map> receiver_map(map, isolate());
  if (!Map::TryUpdate(isolate(), receiver_map).ToHandle(&receiver_map))
    return NoChange();

  NameRef cached_name(
      broker(),
      handle(Name::cast(nexus.GetFeedbackExtra()->GetHeapObjectAssumeStrong()),
             isolate()));

  AccessInfoFactory access_info_factory(broker(), dependencies(),
                                        graph()->zone());
  PropertyAccessInfo access_info =
      access_info_factory.ComputePropertyAccessInfo(
          receiver_map, cached_name.object(), AccessMode::kStoreInLiteral);
  if (access_info.IsInvalid()) return NoChange();
  access_info.RecordDependencies(dependencies());

  Node* receiver = NodeProperties::GetValueInput(node, 0);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // Monomorphic property access.
  PropertyAccessBuilder access_builder(jsgraph(), broker(), dependencies());
  access_builder.BuildCheckMaps(receiver, &effect, control,
                                access_info.receiver_maps());

  // Ensure that {name} matches the cached name.
  Node* name = NodeProperties::GetValueInput(node, 1);
  Node* check = graph()->NewNode(simplified()->ReferenceEqual(), name,
                                 jsgraph()->Constant(cached_name));
  effect = graph()->NewNode(simplified()->CheckIf(DeoptimizeReason::kWrongName),
                            check, effect, control);

  Node* value = NodeProperties::GetValueInput(node, 2);
  Node* context = NodeProperties::GetContextInput(node);
  Node* frame_state_lazy = NodeProperties::GetFrameStateInput(node);

  // Generate the actual property access.
  ValueEffectControl continuation = BuildPropertyAccess(
      receiver, value, context, frame_state_lazy, effect, control, cached_name,
      nullptr, access_info, AccessMode::kStoreInLiteral);
  value = continuation.value();
  effect = continuation.effect();
  control = continuation.control();

  ReplaceWithValue(node, value, effect, control);
  return Replace(value);
}

Reduction JSNativeContextSpecialization::ReduceJSStoreInArrayLiteral(
    Node* node) {
  DCHECK_EQ(IrOpcode::kJSStoreInArrayLiteral, node->opcode());
  FeedbackParameter const& p = FeedbackParameterOf(node->op());
  Node* const index = NodeProperties::GetValueInput(node, 1);
  Node* const value = NodeProperties::GetValueInput(node, 2);

  // Extract receiver maps from the keyed store IC using the FeedbackNexus.
  if (!p.feedback().IsValid()) return NoChange();
  FeedbackNexus nexus(p.feedback().vector(), p.feedback().slot());

  // Extract the keyed access store mode from the keyed store IC.
  KeyedAccessStoreMode store_mode = nexus.GetKeyedAccessStoreMode();

  return ReduceKeyedAccess(node, index, value, nexus,
                           AccessMode::kStoreInLiteral, STANDARD_LOAD,
                           store_mode);
}

Reduction JSNativeContextSpecialization::ReduceJSToObject(Node* node) {
  DCHECK_EQ(IrOpcode::kJSToObject, node->opcode());
  Node* receiver = NodeProperties::GetValueInput(node, 0);
  Node* effect = NodeProperties::GetEffectInput(node);

  MapInference inference(broker(), receiver, effect);
  if (!inference.HaveMaps() || !inference.AllOfInstanceTypesAreJSReceiver()) {
    return NoChange();
  }

  ReplaceWithValue(node, receiver, effect);
  return Replace(receiver);
}

namespace {

ExternalArrayType GetArrayTypeFromElementsKind(ElementsKind kind) {
  switch (kind) {
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype) \
  case TYPE##_ELEMENTS:                           \
    return kExternal##Type##Array;
    TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
    default:
      break;
  }
  UNREACHABLE();
}

}  // namespace

JSNativeContextSpecialization::ValueEffectControl
JSNativeContextSpecialization::BuildElementAccess(
    Node* receiver, Node* index, Node* value, Node* effect, Node* control,
    ElementAccessInfo const& access_info, AccessMode access_mode,
    KeyedAccessLoadMode load_mode, KeyedAccessStoreMode store_mode) {
  // TODO(bmeurer): We currently specialize based on elements kind. We should
  // also be able to properly support strings and other JSObjects here.
  ElementsKind elements_kind = access_info.elements_kind();
  ZoneVector<Handle<Map>> const& receiver_maps = access_info.receiver_maps();

  if (IsTypedArrayElementsKind(elements_kind)) {
    Node* buffer_or_receiver = receiver;
    Node* length;
    Node* base_pointer;
    Node* external_pointer;

    // Check if we can constant-fold information about the {receiver} (e.g.
    // for asm.js-like code patterns).
    base::Optional<JSTypedArrayRef> typed_array =
        GetTypedArrayConstant(broker(), receiver);
    if (typed_array.has_value()) {
      length = jsgraph()->Constant(static_cast<double>(typed_array->length()));

      // Load the (known) base and external pointer for the {receiver}. The
      // {external_pointer} might be invalid if the {buffer} was detached, so
      // we need to make sure that any access is properly guarded.
      base_pointer = jsgraph()->ZeroConstant();
      external_pointer =
          jsgraph()->PointerConstant(typed_array->external_pointer());
    } else {
      // Load the {receiver}s length.
      length = effect = graph()->NewNode(
          simplified()->LoadField(AccessBuilder::ForJSTypedArrayLength()),
          receiver, effect, control);

      // Load the base pointer for the {receiver}. This will always be Smi
      // zero unless we allow on-heap TypedArrays, which is only the case
      // for Chrome. Node and Electron both set this limit to 0. Setting
      // the base to Smi zero here allows the EffectControlLinearizer to
      // optimize away the tricky part of the access later.
      if (V8_TYPED_ARRAY_MAX_SIZE_IN_HEAP == 0) {
        base_pointer = jsgraph()->ZeroConstant();
      } else {
        base_pointer = effect =
            graph()->NewNode(simplified()->LoadField(
                                 AccessBuilder::ForJSTypedArrayBasePointer()),
                             receiver, effect, control);
      }

      // Load the external pointer for the {receiver}.
      external_pointer = effect =
          graph()->NewNode(simplified()->LoadField(
                               AccessBuilder::ForJSTypedArrayExternalPointer()),
                           receiver, effect, control);
    }

    // See if we can skip the detaching check.
    if (!dependencies()->DependOnArrayBufferDetachingProtector()) {
      // Load the buffer for the {receiver}.
      Node* buffer =
          typed_array.has_value()
              ? jsgraph()->Constant(typed_array->buffer())
              : (effect = graph()->NewNode(
                     simplified()->LoadField(
                         AccessBuilder::ForJSArrayBufferViewBuffer()),
                     receiver, effect, control));

      // Deopt if the {buffer} was detached.
      // Note: A detached buffer leads to megamorphic feedback.
      Node* buffer_bit_field = effect = graph()->NewNode(
          simplified()->LoadField(AccessBuilder::ForJSArrayBufferBitField()),
          buffer, effect, control);
      Node* check = graph()->NewNode(
          simplified()->NumberEqual(),
          graph()->NewNode(
              simplified()->NumberBitwiseAnd(), buffer_bit_field,
              jsgraph()->Constant(JSArrayBuffer::WasDetachedBit::kMask)),
          jsgraph()->ZeroConstant());
      effect = graph()->NewNode(
          simplified()->CheckIf(DeoptimizeReason::kArrayBufferWasDetached),
          check, effect, control);

      // Retain the {buffer} instead of {receiver} to reduce live ranges.
      buffer_or_receiver = buffer;
    }

    if (load_mode == LOAD_IGNORE_OUT_OF_BOUNDS ||
        store_mode == STORE_IGNORE_OUT_OF_BOUNDS) {
      // Only check that the {index} is in SignedSmall range. We do the actual
      // bounds check below and just skip the property access if it's out of
      // bounds for the {receiver}.
      index = effect = graph()->NewNode(
          simplified()->CheckSmi(VectorSlotPair()), index, effect, control);

      // Cast the {index} to Unsigned32 range, so that the bounds checks
      // below are performed on unsigned values, which means that all the
      // Negative32 values are treated as out-of-bounds.
      index = graph()->NewNode(simplified()->NumberToUint32(), index);
    } else {
      // Check that the {index} is in the valid range for the {receiver}.
      index = effect =
          graph()->NewNode(simplified()->CheckBounds(VectorSlotPair()), index,
                           length, effect, control);
    }

    // Access the actual element.
    ExternalArrayType external_array_type =
        GetArrayTypeFromElementsKind(elements_kind);
    switch (access_mode) {
      case AccessMode::kLoad: {
        // Check if we can return undefined for out-of-bounds loads.
        if (load_mode == LOAD_IGNORE_OUT_OF_BOUNDS) {
          Node* check =
              graph()->NewNode(simplified()->NumberLessThan(), index, length);
          Node* branch = graph()->NewNode(
              common()->Branch(BranchHint::kTrue,
                               IsSafetyCheck::kCriticalSafetyCheck),
              check, control);

          Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
          Node* etrue = effect;
          Node* vtrue;
          {
            // Perform the actual load
            vtrue = etrue = graph()->NewNode(
                simplified()->LoadTypedElement(external_array_type),
                buffer_or_receiver, base_pointer, external_pointer, index,
                etrue, if_true);
          }

          Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
          Node* efalse = effect;
          Node* vfalse;
          {
            // Materialize undefined for out-of-bounds loads.
            vfalse = jsgraph()->UndefinedConstant();
          }

          control = graph()->NewNode(common()->Merge(2), if_true, if_false);
          effect =
              graph()->NewNode(common()->EffectPhi(2), etrue, efalse, control);
          value =
              graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                               vtrue, vfalse, control);
        } else {
          // Perform the actual load.
          value = effect = graph()->NewNode(
              simplified()->LoadTypedElement(external_array_type),
              buffer_or_receiver, base_pointer, external_pointer, index, effect,
              control);
        }
        break;
      }
      case AccessMode::kStoreInLiteral:
        UNREACHABLE();
        break;
      case AccessMode::kStore: {
        // Ensure that the {value} is actually a Number or an Oddball,
        // and truncate it to a Number appropriately.
        value = effect = graph()->NewNode(
            simplified()->SpeculativeToNumber(
                NumberOperationHint::kNumberOrOddball, VectorSlotPair()),
            value, effect, control);

        // Introduce the appropriate truncation for {value}. Currently we
        // only need to do this for ClamedUint8Array {receiver}s, as the
        // other truncations are implicit in the StoreTypedElement, but we
        // might want to change that at some point.
        if (external_array_type == kExternalUint8ClampedArray) {
          value = graph()->NewNode(simplified()->NumberToUint8Clamped(), value);
        }

        // Check if we can skip the out-of-bounds store.
        if (store_mode == STORE_IGNORE_OUT_OF_BOUNDS) {
          Node* check =
              graph()->NewNode(simplified()->NumberLessThan(), index, length);
          Node* branch = graph()->NewNode(common()->Branch(BranchHint::kTrue),
                                          check, control);

          Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
          Node* etrue = effect;
          {
            // Perform the actual store.
            etrue = graph()->NewNode(
                simplified()->StoreTypedElement(external_array_type),
                buffer_or_receiver, base_pointer, external_pointer, index,
                value, etrue, if_true);
          }

          Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
          Node* efalse = effect;
          {
            // Just ignore the out-of-bounds write.
          }

          control = graph()->NewNode(common()->Merge(2), if_true, if_false);
          effect =
              graph()->NewNode(common()->EffectPhi(2), etrue, efalse, control);
        } else {
          // Perform the actual store
          effect = graph()->NewNode(
              simplified()->StoreTypedElement(external_array_type),
              buffer_or_receiver, base_pointer, external_pointer, index, value,
              effect, control);
        }
        break;
      }
      case AccessMode::kHas:
        // For has property on a typed array, all we need is a bounds check.
        value = effect =
            graph()->NewNode(simplified()->SpeculativeNumberLessThan(
                                 NumberOperationHint::kSignedSmall),
                             index, length, effect, control);
        break;
    }
  } else {
    // Load the elements for the {receiver}.
    Node* elements = effect = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForJSObjectElements()), receiver,
        effect, control);

    // Don't try to store to a copy-on-write backing store (unless supported by
    // the store mode).
    if (access_mode == AccessMode::kStore &&
        IsSmiOrObjectElementsKind(elements_kind) &&
        !IsCOWHandlingStoreMode(store_mode)) {
      effect = graph()->NewNode(
          simplified()->CheckMaps(
              CheckMapsFlag::kNone,
              ZoneHandleSet<Map>(factory()->fixed_array_map())),
          elements, effect, control);
    }

    // Check if the {receiver} is a JSArray.
    bool receiver_is_jsarray = HasOnlyJSArrayMaps(broker(), receiver_maps);

    // Load the length of the {receiver}.
    Node* length = effect =
        receiver_is_jsarray
            ? graph()->NewNode(
                  simplified()->LoadField(
                      AccessBuilder::ForJSArrayLength(elements_kind)),
                  receiver, effect, control)
            : graph()->NewNode(
                  simplified()->LoadField(AccessBuilder::ForFixedArrayLength()),
                  elements, effect, control);

    // Check if we might need to grow the {elements} backing store.
    if (IsGrowStoreMode(store_mode)) {
      // For growing stores we validate the {index} below.
      DCHECK(access_mode == AccessMode::kStore ||
             access_mode == AccessMode::kStoreInLiteral);
    } else if (load_mode == LOAD_IGNORE_OUT_OF_BOUNDS &&
               CanTreatHoleAsUndefined(receiver_maps)) {
      // Check that the {index} is a valid array index, we do the actual
      // bounds check below and just skip the store below if it's out of
      // bounds for the {receiver}.
      index = effect = graph()->NewNode(
          simplified()->CheckBounds(VectorSlotPair()), index,
          jsgraph()->Constant(Smi::kMaxValue), effect, control);
    } else {
      // Check that the {index} is in the valid range for the {receiver}.
      index = effect =
          graph()->NewNode(simplified()->CheckBounds(VectorSlotPair()), index,
                           length, effect, control);
    }

    // Compute the element access.
    Type element_type = Type::NonInternal();
    MachineType element_machine_type = MachineType::TypeCompressedTagged();
    if (IsDoubleElementsKind(elements_kind)) {
      element_type = Type::Number();
      element_machine_type = MachineType::Float64();
    } else if (IsSmiElementsKind(elements_kind)) {
      element_type = Type::SignedSmall();
      element_machine_type = MachineType::TypeCompressedTaggedSigned();
    }
    ElementAccess element_access = {
        kTaggedBase,       FixedArray::kHeaderSize,
        element_type,      element_machine_type,
        kFullWriteBarrier, LoadSensitivity::kCritical};

    // Access the actual element.
    if (access_mode == AccessMode::kLoad) {
      // Compute the real element access type, which includes the hole in case
      // of holey backing stores.
      if (IsHoleyElementsKind(elements_kind)) {
        element_access.type =
            Type::Union(element_type, Type::Hole(), graph()->zone());
      }
      if (elements_kind == HOLEY_ELEMENTS ||
          elements_kind == HOLEY_SMI_ELEMENTS) {
        element_access.machine_type = MachineType::TypeCompressedTagged();
      }

      // Check if we can return undefined for out-of-bounds loads.
      if (load_mode == LOAD_IGNORE_OUT_OF_BOUNDS &&
          CanTreatHoleAsUndefined(receiver_maps)) {
        Node* check =
            graph()->NewNode(simplified()->NumberLessThan(), index, length);
        Node* branch = graph()->NewNode(
            common()->Branch(BranchHint::kTrue,
                             IsSafetyCheck::kCriticalSafetyCheck),
            check, control);

        Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
        Node* etrue = effect;
        Node* vtrue;
        {
          // Perform the actual load
          vtrue = etrue =
              graph()->NewNode(simplified()->LoadElement(element_access),
                               elements, index, etrue, if_true);

          // Handle loading from holey backing stores correctly, by either
          // mapping the hole to undefined if possible, or deoptimizing
          // otherwise.
          if (elements_kind == HOLEY_ELEMENTS ||
              elements_kind == HOLEY_SMI_ELEMENTS) {
            // Turn the hole into undefined.
            vtrue = graph()->NewNode(
                simplified()->ConvertTaggedHoleToUndefined(), vtrue);
          } else if (elements_kind == HOLEY_DOUBLE_ELEMENTS) {
            // Return the signaling NaN hole directly if all uses are
            // truncating.
            vtrue = etrue = graph()->NewNode(
                simplified()->CheckFloat64Hole(
                    CheckFloat64HoleMode::kAllowReturnHole, VectorSlotPair()),
                vtrue, etrue, if_true);
          }
        }

        Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
        Node* efalse = effect;
        Node* vfalse;
        {
          // Materialize undefined for out-of-bounds loads.
          vfalse = jsgraph()->UndefinedConstant();
        }

        control = graph()->NewNode(common()->Merge(2), if_true, if_false);
        effect =
            graph()->NewNode(common()->EffectPhi(2), etrue, efalse, control);
        value =
            graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                             vtrue, vfalse, control);
      } else {
        // Perform the actual load.
        value = effect =
            graph()->NewNode(simplified()->LoadElement(element_access),
                             elements, index, effect, control);

        // Handle loading from holey backing stores correctly, by either mapping
        // the hole to undefined if possible, or deoptimizing otherwise.
        if (elements_kind == HOLEY_ELEMENTS ||
            elements_kind == HOLEY_SMI_ELEMENTS) {
          // Check if we are allowed to turn the hole into undefined.
          if (CanTreatHoleAsUndefined(receiver_maps)) {
            // Turn the hole into undefined.
            value = graph()->NewNode(
                simplified()->ConvertTaggedHoleToUndefined(), value);
          } else {
            // Bailout if we see the hole.
            value = effect = graph()->NewNode(
                simplified()->CheckNotTaggedHole(), value, effect, control);
          }
        } else if (elements_kind == HOLEY_DOUBLE_ELEMENTS) {
          // Perform the hole check on the result.
          CheckFloat64HoleMode mode = CheckFloat64HoleMode::kNeverReturnHole;
          // Check if we are allowed to return the hole directly.
          if (CanTreatHoleAsUndefined(receiver_maps)) {
            // Return the signaling NaN hole directly if all uses are
            // truncating.
            mode = CheckFloat64HoleMode::kAllowReturnHole;
          }
          value = effect = graph()->NewNode(
              simplified()->CheckFloat64Hole(mode, VectorSlotPair()), value,
              effect, control);
        }
      }
    } else if (access_mode == AccessMode::kHas) {
      // For packed arrays with NoElementsProctector valid, a bound check
      // is equivalent to HasProperty.
      value = effect = graph()->NewNode(simplified()->SpeculativeNumberLessThan(
                                            NumberOperationHint::kSignedSmall),
                                        index, length, effect, control);
      if (IsHoleyElementsKind(elements_kind)) {
        // If the index is in bounds, do a load and hole check.

        Node* branch = graph()->NewNode(common()->Branch(), value, control);

        Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
        Node* efalse = effect;
        Node* vfalse = jsgraph()->FalseConstant();

        element_access.type =
            Type::Union(element_type, Type::Hole(), graph()->zone());

        if (elements_kind == HOLEY_ELEMENTS ||
            elements_kind == HOLEY_SMI_ELEMENTS) {
          element_access.machine_type = MachineType::TypeCompressedTagged();
        }

        Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
        Node* etrue = effect;

        Node* checked = etrue =
            graph()->NewNode(simplified()->CheckBounds(VectorSlotPair()), index,
                             length, etrue, if_true);

        Node* element = etrue =
            graph()->NewNode(simplified()->LoadElement(element_access),
                             elements, checked, etrue, if_true);

        Node* vtrue;
        if (CanTreatHoleAsUndefined(receiver_maps)) {
          if (elements_kind == HOLEY_ELEMENTS ||
              elements_kind == HOLEY_SMI_ELEMENTS) {
            // Check if we are allowed to turn the hole into undefined.
            // Turn the hole into undefined.
            vtrue = graph()->NewNode(simplified()->ReferenceEqual(), element,
                                     jsgraph()->TheHoleConstant());
          } else {
            vtrue =
                graph()->NewNode(simplified()->NumberIsFloat64Hole(), element);
          }

          // has == !IsHole
          vtrue = graph()->NewNode(simplified()->BooleanNot(), vtrue);
        } else {
          if (elements_kind == HOLEY_ELEMENTS ||
              elements_kind == HOLEY_SMI_ELEMENTS) {
            // Bailout if we see the hole.
            etrue = graph()->NewNode(simplified()->CheckNotTaggedHole(),
                                     element, etrue, if_true);
          } else {
            etrue = graph()->NewNode(
                simplified()->CheckFloat64Hole(
                    CheckFloat64HoleMode::kNeverReturnHole, VectorSlotPair()),
                element, etrue, if_true);
          }

          vtrue = jsgraph()->TrueConstant();
        }

        control = graph()->NewNode(common()->Merge(2), if_true, if_false);
        effect =
            graph()->NewNode(common()->EffectPhi(2), etrue, efalse, control);
        value =
            graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                             vtrue, vfalse, control);
      }
    } else {
      DCHECK(access_mode == AccessMode::kStore ||
             access_mode == AccessMode::kStoreInLiteral);
      if (IsSmiElementsKind(elements_kind)) {
        value = effect = graph()->NewNode(
            simplified()->CheckSmi(VectorSlotPair()), value, effect, control);
      } else if (IsDoubleElementsKind(elements_kind)) {
        value = effect =
            graph()->NewNode(simplified()->CheckNumber(VectorSlotPair()), value,
                             effect, control);
        // Make sure we do not store signalling NaNs into double arrays.
        value = graph()->NewNode(simplified()->NumberSilenceNaN(), value);
      }

      // Ensure that copy-on-write backing store is writable.
      if (IsSmiOrObjectElementsKind(elements_kind) &&
          store_mode == STORE_HANDLE_COW) {
        elements = effect =
            graph()->NewNode(simplified()->EnsureWritableFastElements(),
                             receiver, elements, effect, control);
      } else if (IsGrowStoreMode(store_mode)) {
        // Determine the length of the {elements} backing store.
        Node* elements_length = effect = graph()->NewNode(
            simplified()->LoadField(AccessBuilder::ForFixedArrayLength()),
            elements, effect, control);

        // Validate the {index} depending on holeyness:
        //
        // For HOLEY_*_ELEMENTS the {index} must not exceed the {elements}
        // backing store capacity plus the maximum allowed gap, as otherwise
        // the (potential) backing store growth would normalize and thus
        // the elements kind of the {receiver} would change to slow mode.
        //
        // For PACKED_*_ELEMENTS the {index} must be within the range
        // [0,length+1[ to be valid. In case {index} equals {length},
        // the {receiver} will be extended, but kept packed.
        Node* limit =
            IsHoleyElementsKind(elements_kind)
                ? graph()->NewNode(simplified()->NumberAdd(), elements_length,
                                   jsgraph()->Constant(JSObject::kMaxGap))
                : graph()->NewNode(simplified()->NumberAdd(), length,
                                   jsgraph()->OneConstant());
        index = effect =
            graph()->NewNode(simplified()->CheckBounds(VectorSlotPair()), index,
                             limit, effect, control);

        // Grow {elements} backing store if necessary.
        GrowFastElementsMode mode =
            IsDoubleElementsKind(elements_kind)
                ? GrowFastElementsMode::kDoubleElements
                : GrowFastElementsMode::kSmiOrObjectElements;
        elements = effect = graph()->NewNode(
            simplified()->MaybeGrowFastElements(mode, VectorSlotPair()),
            receiver, elements, index, elements_length, effect, control);

        // If we didn't grow {elements}, it might still be COW, in which case we
        // copy it now.
        if (IsSmiOrObjectElementsKind(elements_kind) &&
            store_mode == STORE_AND_GROW_HANDLE_COW) {
          elements = effect =
              graph()->NewNode(simplified()->EnsureWritableFastElements(),
                               receiver, elements, effect, control);
        }

        // Also update the "length" property if {receiver} is a JSArray.
        if (receiver_is_jsarray) {
          Node* check =
              graph()->NewNode(simplified()->NumberLessThan(), index, length);
          Node* branch = graph()->NewNode(common()->Branch(), check, control);

          Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
          Node* etrue = effect;
          {
            // We don't need to do anything, the {index} is within
            // the valid bounds for the JSArray {receiver}.
          }

          Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
          Node* efalse = effect;
          {
            // Update the JSArray::length field. Since this is observable,
            // there must be no other check after this.
            Node* new_length = graph()->NewNode(
                simplified()->NumberAdd(), index, jsgraph()->OneConstant());
            efalse = graph()->NewNode(
                simplified()->StoreField(
                    AccessBuilder::ForJSArrayLength(elements_kind)),
                receiver, new_length, efalse, if_false);
          }

          control = graph()->NewNode(common()->Merge(2), if_true, if_false);
          effect =
              graph()->NewNode(common()->EffectPhi(2), etrue, efalse, control);
        }
      }

      // Perform the actual element access.
      effect = graph()->NewNode(simplified()->StoreElement(element_access),
                                elements, index, value, effect, control);
    }
  }

  return ValueEffectControl(value, effect, control);
}

Node* JSNativeContextSpecialization::BuildIndexedStringLoad(
    Node* receiver, Node* index, Node* length, Node** effect, Node** control,
    KeyedAccessLoadMode load_mode) {
  if (load_mode == LOAD_IGNORE_OUT_OF_BOUNDS &&
      dependencies()->DependOnNoElementsProtector()) {
    // Ensure that the {index} is a valid String length.
    index = *effect = graph()->NewNode(
        simplified()->CheckBounds(VectorSlotPair()), index,
        jsgraph()->Constant(String::kMaxLength), *effect, *control);

    // Load the single character string from {receiver} or yield
    // undefined if the {index} is not within the valid bounds.
    Node* check =
        graph()->NewNode(simplified()->NumberLessThan(), index, length);
    Node* branch =
        graph()->NewNode(common()->Branch(BranchHint::kTrue,
                                          IsSafetyCheck::kCriticalSafetyCheck),
                         check, *control);

    Node* masked_index = graph()->NewNode(simplified()->PoisonIndex(), index);

    Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
    Node* etrue;
    Node* vtrue = etrue =
        graph()->NewNode(simplified()->StringCharCodeAt(), receiver,
                         masked_index, *effect, if_true);
    vtrue = graph()->NewNode(simplified()->StringFromSingleCharCode(), vtrue);

    Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
    Node* vfalse = jsgraph()->UndefinedConstant();

    *control = graph()->NewNode(common()->Merge(2), if_true, if_false);
    *effect =
        graph()->NewNode(common()->EffectPhi(2), etrue, *effect, *control);
    return graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                            vtrue, vfalse, *control);
  } else {
    // Ensure that {index} is less than {receiver} length.
    index = *effect =
        graph()->NewNode(simplified()->CheckBounds(VectorSlotPair()), index,
                         length, *effect, *control);

    Node* masked_index = graph()->NewNode(simplified()->PoisonIndex(), index);

    // Return the character from the {receiver} as single character string.
    Node* value = *effect =
        graph()->NewNode(simplified()->StringCharCodeAt(), receiver,
                         masked_index, *effect, *control);
    value = graph()->NewNode(simplified()->StringFromSingleCharCode(), value);
    return value;
  }
}

Node* JSNativeContextSpecialization::BuildExtendPropertiesBackingStore(
    const MapRef& map, Node* properties, Node* effect, Node* control) {
  // TODO(bmeurer/jkummerow): Property deletions can undo map transitions
  // while keeping the backing store around, meaning that even though the
  // map might believe that objects have no unused property fields, there
  // might actually be some. It would be nice to not create a new backing
  // store in that case (i.e. when properties->length() >= new_length).
  // However, introducing branches and Phi nodes here would make it more
  // difficult for escape analysis to get rid of the backing stores used
  // for intermediate states of chains of property additions. That makes
  // it unclear what the best approach is here.
  DCHECK_EQ(0, map.UnusedPropertyFields());
  // Compute the length of the old {properties} and the new properties.
  int length = map.NextFreePropertyIndex() - map.GetInObjectProperties();
  int new_length = length + JSObject::kFieldsAdded;
  // Collect the field values from the {properties}.
  ZoneVector<Node*> values(zone());
  values.reserve(new_length);
  for (int i = 0; i < length; ++i) {
    Node* value = effect = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForFixedArraySlot(i)),
        properties, effect, control);
    values.push_back(value);
  }
  // Initialize the new fields to undefined.
  for (int i = 0; i < JSObject::kFieldsAdded; ++i) {
    values.push_back(jsgraph()->UndefinedConstant());
  }

  // Compute new length and hash.
  Node* hash;
  if (length == 0) {
    hash = graph()->NewNode(
        common()->Select(MachineRepresentation::kTaggedSigned),
        graph()->NewNode(simplified()->ObjectIsSmi(), properties), properties,
        jsgraph()->SmiConstant(PropertyArray::kNoHashSentinel));
    hash = effect = graph()->NewNode(common()->TypeGuard(Type::SignedSmall()),
                                     hash, effect, control);
    hash =
        graph()->NewNode(simplified()->NumberShiftLeft(), hash,
                         jsgraph()->Constant(PropertyArray::HashField::kShift));
  } else {
    hash = effect = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForPropertyArrayLengthAndHash()),
        properties, effect, control);
    hash =
        graph()->NewNode(simplified()->NumberBitwiseAnd(), hash,
                         jsgraph()->Constant(PropertyArray::HashField::kMask));
  }
  Node* new_length_and_hash = graph()->NewNode(
      simplified()->NumberBitwiseOr(), jsgraph()->Constant(new_length), hash);
  // TDOO(jarin): Fix the typer to infer tighter bound for NumberBitwiseOr.
  new_length_and_hash = effect =
      graph()->NewNode(common()->TypeGuard(Type::SignedSmall()),
                       new_length_and_hash, effect, control);

  // Allocate and initialize the new properties.
  AllocationBuilder a(jsgraph(), effect, control);
  a.Allocate(PropertyArray::SizeFor(new_length), AllocationType::kYoung,
             Type::OtherInternal());
  a.Store(AccessBuilder::ForMap(), jsgraph()->PropertyArrayMapConstant());
  a.Store(AccessBuilder::ForPropertyArrayLengthAndHash(), new_length_and_hash);
  for (int i = 0; i < new_length; ++i) {
    a.Store(AccessBuilder::ForFixedArraySlot(i), values[i]);
  }
  return a.Finish();
}

Node* JSNativeContextSpecialization::BuildCheckEqualsName(NameRef const& name,
                                                          Node* value,
                                                          Node* effect,
                                                          Node* control) {
  DCHECK(name.IsUniqueName());
  Operator const* const op =
      name.IsSymbol() ? simplified()->CheckEqualsSymbol()
                      : simplified()->CheckEqualsInternalizedString();
  return graph()->NewNode(op, jsgraph()->Constant(name), value, effect,
                          control);
}

bool JSNativeContextSpecialization::CanTreatHoleAsUndefined(
    ZoneVector<Handle<Map>> const& receiver_maps) {
  // Check if all {receiver_maps} have one of the initial Array.prototype
  // or Object.prototype objects as their prototype (in any of the current
  // native contexts, as the global Array protector works isolate-wide).
  for (Handle<Map> map : receiver_maps) {
    MapRef receiver_map(broker(), map);
    if (!FLAG_concurrent_inlining) receiver_map.SerializePrototype();
    ObjectRef receiver_prototype = receiver_map.prototype();
    if (!receiver_prototype.IsJSObject() ||
        !broker()->IsArrayOrObjectPrototype(receiver_prototype.AsJSObject())) {
      return false;
    }
  }

  // Check if the array prototype chain is intact.
  return dependencies()->DependOnNoElementsProtector();
}

// Returns false iff we have insufficient feedback (uninitialized or obsolete).
bool JSNativeContextSpecialization::ExtractReceiverMaps(
    Node* receiver, Node* effect, FeedbackNexus const& nexus,
    MapHandles* receiver_maps) {
  DCHECK(receiver_maps->empty());
  if (nexus.IsUninitialized()) return false;

  // See if we can infer a concrete type for the {receiver}. Solely relying on
  // the inference is not safe for keyed stores, because we would potentially
  // miss out on transitions that need to be performed.
  {
    FeedbackSlotKind kind = nexus.kind();
    bool use_inference =
        !IsKeyedStoreICKind(kind) && !IsStoreInArrayLiteralICKind(kind);
    if (use_inference && InferReceiverMaps(receiver, effect, receiver_maps)) {
      TryUpdateThenDropDeprecated(isolate(), receiver_maps);
      return true;
    }
  }

  if (nexus.ExtractMaps(receiver_maps) == 0) return true;

  // Try to filter impossible candidates based on inferred root map.
  Handle<Map> root_map;
  if (InferReceiverRootMap(receiver).ToHandle(&root_map)) {
    DCHECK(!root_map->is_abandoned_prototype_map());
    Isolate* isolate = this->isolate();
    receiver_maps->erase(
        std::remove_if(receiver_maps->begin(), receiver_maps->end(),
                       [root_map, isolate](Handle<Map> map) {
                         return map->is_abandoned_prototype_map() ||
                                map->FindRootMap(isolate) != *root_map;
                       }),
        receiver_maps->end());
  }
  TryUpdateThenDropDeprecated(isolate(), receiver_maps);
  return !receiver_maps->empty();
}

bool JSNativeContextSpecialization::InferReceiverMaps(
    Node* receiver, Node* effect, MapHandles* receiver_maps) {
  ZoneHandleSet<Map> maps;
  NodeProperties::InferReceiverMapsResult result =
      NodeProperties::InferReceiverMaps(broker(), receiver, effect, &maps);
  if (result == NodeProperties::kReliableReceiverMaps) {
    for (size_t i = 0; i < maps.size(); ++i) {
      receiver_maps->push_back(maps[i]);
    }
    return true;
  } else if (result == NodeProperties::kUnreliableReceiverMaps) {
    // For untrusted receiver maps, we can still use the information
    // if the maps are stable.
    for (size_t i = 0; i < maps.size(); ++i) {
      MapRef map(broker(), maps[i]);
      if (!map.is_stable()) return false;
    }
    for (size_t i = 0; i < maps.size(); ++i) {
      receiver_maps->push_back(maps[i]);
    }
    return true;
  }
  return false;
}

MaybeHandle<Map> JSNativeContextSpecialization::InferReceiverRootMap(
    Node* receiver) {
  HeapObjectMatcher m(receiver);
  if (m.HasValue()) {
    return handle(m.Value()->map().FindRootMap(isolate()), isolate());
  } else if (m.IsJSCreate()) {
    base::Optional<MapRef> initial_map =
        NodeProperties::GetJSCreateMap(broker(), receiver);
    if (initial_map.has_value()) {
      DCHECK_EQ(*initial_map->object(),
                initial_map->object()->FindRootMap(isolate()));
      return initial_map->object();
    }
  }
  return MaybeHandle<Map>();
}

Graph* JSNativeContextSpecialization::graph() const {
  return jsgraph()->graph();
}

Isolate* JSNativeContextSpecialization::isolate() const {
  return jsgraph()->isolate();
}

Factory* JSNativeContextSpecialization::factory() const {
  return isolate()->factory();
}

CommonOperatorBuilder* JSNativeContextSpecialization::common() const {
  return jsgraph()->common();
}

JSOperatorBuilder* JSNativeContextSpecialization::javascript() const {
  return jsgraph()->javascript();
}

SimplifiedOperatorBuilder* JSNativeContextSpecialization::simplified() const {
  return jsgraph()->simplified();
}

#undef V8_TYPED_ARRAY_MAX_SIZE_IN_HEAP

}  // namespace compiler
}  // namespace internal
}  // namespace v8
