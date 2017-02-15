// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-native-context-specialization.h"

#include "src/accessors.h"
#include "src/code-factory.h"
#include "src/compilation-dependencies.h"
#include "src/compiler/access-builder.h"
#include "src/compiler/access-info.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/js-operator.h"
#include "src/compiler/linkage.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/type-cache.h"
#include "src/field-index-inl.h"
#include "src/isolate-inl.h"
#include "src/type-feedback-vector.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {

bool HasNumberMaps(MapList const& maps) {
  for (auto map : maps) {
    if (map->instance_type() == HEAP_NUMBER_TYPE) return true;
  }
  return false;
}

bool HasOnlyJSArrayMaps(MapList const& maps) {
  for (auto map : maps) {
    if (!map->IsJSArrayMap()) return false;
  }
  return true;
}

bool HasOnlyNumberMaps(MapList const& maps) {
  for (auto map : maps) {
    if (map->instance_type() != HEAP_NUMBER_TYPE) return false;
  }
  return true;
}

template <typename T>
bool HasOnlyStringMaps(T const& maps) {
  for (auto map : maps) {
    if (!map->IsStringMap()) return false;
  }
  return true;
}

}  // namespace

JSNativeContextSpecialization::JSNativeContextSpecialization(
    Editor* editor, JSGraph* jsgraph, Flags flags,
    MaybeHandle<Context> native_context, CompilationDependencies* dependencies,
    Zone* zone)
    : AdvancedReducer(editor),
      jsgraph_(jsgraph),
      flags_(flags),
      native_context_(native_context),
      dependencies_(dependencies),
      zone_(zone),
      type_cache_(TypeCache::Get()) {}


Reduction JSNativeContextSpecialization::Reduce(Node* node) {
  switch (node->opcode()) {
    case IrOpcode::kJSInstanceOf:
      return ReduceJSInstanceOf(node);
    case IrOpcode::kJSLoadContext:
      return ReduceJSLoadContext(node);
    case IrOpcode::kJSLoadNamed:
      return ReduceJSLoadNamed(node);
    case IrOpcode::kJSStoreNamed:
      return ReduceJSStoreNamed(node);
    case IrOpcode::kJSLoadProperty:
      return ReduceJSLoadProperty(node);
    case IrOpcode::kJSStoreProperty:
      return ReduceJSStoreProperty(node);
    default:
      break;
  }
  return NoChange();
}

Reduction JSNativeContextSpecialization::ReduceJSInstanceOf(Node* node) {
  DCHECK_EQ(IrOpcode::kJSInstanceOf, node->opcode());
  Node* object = NodeProperties::GetValueInput(node, 0);
  Node* constructor = NodeProperties::GetValueInput(node, 1);
  Node* context = NodeProperties::GetContextInput(node);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // Retrieve the native context from the given {node}.
  Handle<Context> native_context;
  if (!GetNativeContext(node).ToHandle(&native_context)) return NoChange();

  // If deoptimization is disabled, we cannot optimize.
  if (!(flags() & kDeoptimizationEnabled)) return NoChange();

  // Check if the right hand side is a known {receiver}.
  HeapObjectMatcher m(constructor);
  if (!m.HasValue() || !m.Value()->IsJSObject()) return NoChange();
  Handle<JSObject> receiver = Handle<JSObject>::cast(m.Value());
  Handle<Map> receiver_map(receiver->map(), isolate());

  // Compute property access info for @@hasInstance on {receiver}.
  PropertyAccessInfo access_info;
  AccessInfoFactory access_info_factory(dependencies(), native_context,
                                        graph()->zone());
  if (!access_info_factory.ComputePropertyAccessInfo(
          receiver_map, factory()->has_instance_symbol(), AccessMode::kLoad,
          &access_info)) {
    return NoChange();
  }

  if (access_info.IsNotFound()) {
    // If there's no @@hasInstance handler, the OrdinaryHasInstance operation
    // takes over, but that requires the {receiver} to be callable.
    if (receiver->IsCallable()) {
      // Determine actual holder and perform prototype chain checks.
      Handle<JSObject> holder;
      if (access_info.holder().ToHandle(&holder)) {
        AssumePrototypesStable(access_info.receiver_maps(), native_context,
                               holder);
      }

      // Monomorphic property access.
      effect =
          BuildCheckMaps(constructor, effect, control, MapList{receiver_map});

      // Lower to OrdinaryHasInstance(C, O).
      NodeProperties::ReplaceValueInput(node, constructor, 0);
      NodeProperties::ReplaceValueInput(node, object, 1);
      NodeProperties::ReplaceEffectInput(node, effect);
      NodeProperties::ChangeOp(node, javascript()->OrdinaryHasInstance());
      return Changed(node);
    }
  } else if (access_info.IsDataConstant()) {
    DCHECK(access_info.constant()->IsCallable());

    // Determine actual holder and perform prototype chain checks.
    Handle<JSObject> holder;
    if (access_info.holder().ToHandle(&holder)) {
      AssumePrototypesStable(access_info.receiver_maps(), native_context,
                             holder);
    }

    // Monomorphic property access.
    effect =
        BuildCheckMaps(constructor, effect, control, MapList{receiver_map});

    // Call the @@hasInstance handler.
    Node* target = jsgraph()->Constant(access_info.constant());
    node->InsertInput(graph()->zone(), 0, target);
    node->ReplaceInput(1, constructor);
    node->ReplaceInput(2, object);
    node->ReplaceInput(5, effect);
    NodeProperties::ChangeOp(
        node,
        javascript()->CallFunction(3, 0.0f, VectorSlotPair(),
                                   ConvertReceiverMode::kNotNullOrUndefined));

    // Rewire the value uses of {node} to ToBoolean conversion of the result.
    Node* value = graph()->NewNode(javascript()->ToBoolean(ToBooleanHint::kAny),
                                   node, context);
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

Reduction JSNativeContextSpecialization::ReduceJSLoadContext(Node* node) {
  DCHECK_EQ(IrOpcode::kJSLoadContext, node->opcode());
  ContextAccess const& access = ContextAccessOf(node->op());
  Handle<Context> native_context;
  // Specialize JSLoadContext(NATIVE_CONTEXT_INDEX) to the known native
  // context (if any), so we can constant-fold those fields, which is
  // safe, since the NATIVE_CONTEXT_INDEX slot is always immutable.
  if (access.index() == Context::NATIVE_CONTEXT_INDEX &&
      GetNativeContext(node).ToHandle(&native_context)) {
    Node* value = jsgraph()->HeapConstant(native_context);
    ReplaceWithValue(node, value);
    return Replace(value);
  }
  return NoChange();
}

Reduction JSNativeContextSpecialization::ReduceNamedAccess(
    Node* node, Node* value, MapHandleList const& receiver_maps,
    Handle<Name> name, AccessMode access_mode, LanguageMode language_mode,
    Node* index) {
  DCHECK(node->opcode() == IrOpcode::kJSLoadNamed ||
         node->opcode() == IrOpcode::kJSStoreNamed ||
         node->opcode() == IrOpcode::kJSLoadProperty ||
         node->opcode() == IrOpcode::kJSStoreProperty);
  Node* receiver = NodeProperties::GetValueInput(node, 0);
  Node* context = NodeProperties::GetContextInput(node);
  Node* frame_state_eager = NodeProperties::FindFrameStateBefore(node);
  Node* frame_state_lazy = NodeProperties::GetFrameStateInput(node);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // Not much we can do if deoptimization support is disabled.
  if (!(flags() & kDeoptimizationEnabled)) return NoChange();

  // Retrieve the native context from the given {node}.
  Handle<Context> native_context;
  if (!GetNativeContext(node).ToHandle(&native_context)) return NoChange();

  // Compute property access infos for the receiver maps.
  AccessInfoFactory access_info_factory(dependencies(), native_context,
                                        graph()->zone());
  ZoneVector<PropertyAccessInfo> access_infos(zone());
  if (!access_info_factory.ComputePropertyAccessInfos(
          receiver_maps, name, access_mode, &access_infos)) {
    return NoChange();
  }

  // TODO(turbofan): Add support for inlining into try blocks.
  if (NodeProperties::IsExceptionalCall(node) ||
      !(flags() & kAccessorInliningEnabled)) {
    for (auto access_info : access_infos) {
      if (access_info.IsAccessorConstant()) return NoChange();
    }
  }

  // Nothing to do if we have no non-deprecated maps.
  if (access_infos.empty()) {
    return ReduceSoftDeoptimize(
        node, DeoptimizeReason::kInsufficientTypeFeedbackForGenericNamedAccess);
  }

  // Ensure that {index} matches the specified {name} (if {index} is given).
  if (index != nullptr) {
    Node* check = graph()->NewNode(simplified()->ReferenceEqual(), index,
                                   jsgraph()->HeapConstant(name));
    effect = graph()->NewNode(simplified()->CheckIf(), check, effect, control);
  }

  // Check for the monomorphic cases.
  if (access_infos.size() == 1) {
    PropertyAccessInfo access_info = access_infos.front();
    if (HasOnlyStringMaps(access_info.receiver_maps())) {
      // Monormorphic string access (ignoring the fact that there are multiple
      // String maps).
      receiver = effect = graph()->NewNode(simplified()->CheckString(),
                                           receiver, effect, control);
    } else if (HasOnlyNumberMaps(access_info.receiver_maps())) {
      // Monomorphic number access (we also deal with Smis here).
      receiver = effect = graph()->NewNode(simplified()->CheckNumber(),
                                           receiver, effect, control);
    } else {
      // Monomorphic property access.
      effect = BuildCheckHeapObject(receiver, effect, control);
      effect = BuildCheckMaps(receiver, effect, control,
                              access_info.receiver_maps());
    }

    // Generate the actual property access.
    ValueEffectControl continuation = BuildPropertyAccess(
        receiver, value, context, frame_state_lazy, effect, control, name,
        native_context, access_info, access_mode);
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
      if (HasNumberMaps(access_info.receiver_maps())) {
        receiverissmi_possible = true;
        break;
      }
    }

    // Ensure that {receiver} is a heap object.
    Node* receiverissmi_control = nullptr;
    Node* receiverissmi_effect = effect;
    if (receiverissmi_possible) {
      Node* check = graph()->NewNode(simplified()->ObjectIsSmi(), receiver);
      Node* branch = graph()->NewNode(common()->Branch(), check, control);
      control = graph()->NewNode(common()->IfFalse(), branch);
      receiverissmi_control = graph()->NewNode(common()->IfTrue(), branch);
      receiverissmi_effect = effect;
    } else {
      effect = BuildCheckHeapObject(receiver, effect, control);
    }

    // Load the {receiver} map. The resulting effect is the dominating effect
    // for all (polymorphic) branches.
    Node* receiver_map = effect =
        graph()->NewNode(simplified()->LoadField(AccessBuilder::ForMap()),
                         receiver, effect, control);

    // Generate code for the various different property access patterns.
    Node* fallthrough_control = control;
    for (size_t j = 0; j < access_infos.size(); ++j) {
      PropertyAccessInfo const& access_info = access_infos[j];
      Node* this_value = value;
      Node* this_receiver = receiver;
      Node* this_effect = effect;
      Node* this_control;

      // Perform map check on {receiver}.
      MapList const& receiver_maps = access_info.receiver_maps();
      {
        // Emit a (sequence of) map checks for other {receiver}s.
        ZoneVector<Node*> this_controls(zone());
        ZoneVector<Node*> this_effects(zone());
        size_t num_classes = receiver_maps.size();
        for (auto map : receiver_maps) {
          DCHECK_LT(0u, num_classes);
          Node* check =
              graph()->NewNode(simplified()->ReferenceEqual(), receiver_map,
                               jsgraph()->Constant(map));
          if (--num_classes == 0 && j == access_infos.size() - 1) {
            check = graph()->NewNode(simplified()->CheckIf(), check,
                                     this_effect, fallthrough_control);
            this_controls.push_back(fallthrough_control);
            this_effects.push_back(check);
            fallthrough_control = nullptr;
          } else {
            Node* branch = graph()->NewNode(common()->Branch(), check,
                                            fallthrough_control);
            fallthrough_control = graph()->NewNode(common()->IfFalse(), branch);
            this_controls.push_back(
                graph()->NewNode(common()->IfTrue(), branch));
            this_effects.push_back(this_effect);
          }
        }

        // The Number case requires special treatment to also deal with Smis.
        if (HasNumberMaps(receiver_maps)) {
          // Join this check with the "receiver is smi" check above.
          DCHECK_NOT_NULL(receiverissmi_effect);
          DCHECK_NOT_NULL(receiverissmi_control);
          this_effects.push_back(receiverissmi_effect);
          this_controls.push_back(receiverissmi_control);
          receiverissmi_effect = receiverissmi_control = nullptr;
        }

        // Create single chokepoint for the control.
        int const this_control_count = static_cast<int>(this_controls.size());
        if (this_control_count == 1) {
          this_control = this_controls.front();
          this_effect = this_effects.front();
        } else {
          this_control =
              graph()->NewNode(common()->Merge(this_control_count),
                               this_control_count, &this_controls.front());
          this_effects.push_back(this_control);
          this_effect =
              graph()->NewNode(common()->EffectPhi(this_control_count),
                               this_control_count + 1, &this_effects.front());

          // TODO(turbofan): The effect/control linearization will not find a
          // FrameState after the EffectPhi that is generated above.
          this_effect =
              graph()->NewNode(common()->Checkpoint(), frame_state_eager,
                               this_effect, this_control);
        }
      }

      // Generate the actual property access.
      ValueEffectControl continuation = BuildPropertyAccess(
          this_receiver, this_value, context, frame_state_lazy, this_effect,
          this_control, name, native_context, access_info, access_mode);
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

Reduction JSNativeContextSpecialization::ReduceNamedAccessFromNexus(
    Node* node, Node* value, FeedbackNexus const& nexus, Handle<Name> name,
    AccessMode access_mode, LanguageMode language_mode) {
  DCHECK(node->opcode() == IrOpcode::kJSLoadNamed ||
         node->opcode() == IrOpcode::kJSStoreNamed);
  Node* const receiver = NodeProperties::GetValueInput(node, 0);
  Node* const effect = NodeProperties::GetEffectInput(node);

  // Check if the {nexus} reports type feedback for the IC.
  if (nexus.IsUninitialized()) {
    if ((flags() & kDeoptimizationEnabled) &&
        (flags() & kBailoutOnUninitialized)) {
      return ReduceSoftDeoptimize(
          node,
          DeoptimizeReason::kInsufficientTypeFeedbackForGenericNamedAccess);
    }
    return NoChange();
  }

  // Extract receiver maps from the IC using the {nexus}.
  MapHandleList receiver_maps;
  if (!ExtractReceiverMaps(receiver, effect, nexus, &receiver_maps)) {
    return NoChange();
  } else if (receiver_maps.length() == 0) {
    if ((flags() & kDeoptimizationEnabled) &&
        (flags() & kBailoutOnUninitialized)) {
      return ReduceSoftDeoptimize(
          node,
          DeoptimizeReason::kInsufficientTypeFeedbackForGenericNamedAccess);
    }
    return NoChange();
  }

  // Try to lower the named access based on the {receiver_maps}.
  return ReduceNamedAccess(node, value, receiver_maps, name, access_mode,
                           language_mode);
}


Reduction JSNativeContextSpecialization::ReduceJSLoadNamed(Node* node) {
  DCHECK_EQ(IrOpcode::kJSLoadNamed, node->opcode());
  NamedAccess const& p = NamedAccessOf(node->op());
  Node* const receiver = NodeProperties::GetValueInput(node, 0);
  Node* const value = jsgraph()->Dead();

  // Check if we have a constant receiver.
  HeapObjectMatcher m(receiver);
  if (m.HasValue()) {
    // Optimize "prototype" property of functions.
    if (m.Value()->IsJSFunction() &&
        p.name().is_identical_to(factory()->prototype_string())) {
      Handle<JSFunction> function = Handle<JSFunction>::cast(m.Value());
      if (function->has_initial_map()) {
        // We need to add a code dependency on the initial map of the
        // {function} in order to be notified about changes to the
        // "prototype" of {function}, so it doesn't make sense to
        // continue unless deoptimization is enabled.
        if (flags() & kDeoptimizationEnabled) {
          Handle<Map> initial_map(function->initial_map(), isolate());
          dependencies()->AssumeInitialMapCantChange(initial_map);
          Handle<Object> prototype(initial_map->prototype(), isolate());
          Node* value = jsgraph()->Constant(prototype);
          ReplaceWithValue(node, value);
          return Replace(value);
        }
      }
    }
  }

  // Extract receiver maps from the LOAD_IC using the LoadICNexus.
  if (!p.feedback().IsValid()) return NoChange();
  LoadICNexus nexus(p.feedback().vector(), p.feedback().slot());

  // Try to lower the named access based on the {receiver_maps}.
  return ReduceNamedAccessFromNexus(node, value, nexus, p.name(),
                                    AccessMode::kLoad, p.language_mode());
}


Reduction JSNativeContextSpecialization::ReduceJSStoreNamed(Node* node) {
  DCHECK_EQ(IrOpcode::kJSStoreNamed, node->opcode());
  NamedAccess const& p = NamedAccessOf(node->op());
  Node* const value = NodeProperties::GetValueInput(node, 1);

  // Extract receiver maps from the STORE_IC using the StoreICNexus.
  if (!p.feedback().IsValid()) return NoChange();
  StoreICNexus nexus(p.feedback().vector(), p.feedback().slot());

  // Try to lower the named access based on the {receiver_maps}.
  return ReduceNamedAccessFromNexus(node, value, nexus, p.name(),
                                    AccessMode::kStore, p.language_mode());
}


Reduction JSNativeContextSpecialization::ReduceElementAccess(
    Node* node, Node* index, Node* value, MapHandleList const& receiver_maps,
    AccessMode access_mode, LanguageMode language_mode,
    KeyedAccessStoreMode store_mode) {
  DCHECK(node->opcode() == IrOpcode::kJSLoadProperty ||
         node->opcode() == IrOpcode::kJSStoreProperty);
  Node* receiver = NodeProperties::GetValueInput(node, 0);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* frame_state = NodeProperties::FindFrameStateBefore(node);

  // Not much we can do if deoptimization support is disabled.
  if (!(flags() & kDeoptimizationEnabled)) return NoChange();

  // Check for keyed access to strings.
  if (HasOnlyStringMaps(receiver_maps)) {
    // Strings are immutable in JavaScript.
    if (access_mode == AccessMode::kStore) return NoChange();

    // Ensure that the {receiver} is actually a String.
    receiver = effect = graph()->NewNode(simplified()->CheckString(), receiver,
                                         effect, control);

    // Determine the {receiver} length.
    Node* length = effect = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForStringLength()), receiver,
        effect, control);

    // Ensure that {index} is less than {receiver} length.
    index = effect = graph()->NewNode(simplified()->CheckBounds(), index,
                                      length, effect, control);

    // Load the character from the {receiver}.
    value = graph()->NewNode(simplified()->StringCharCodeAt(), receiver, index,
                             control);

    // Return it as a single character string.
    value = graph()->NewNode(simplified()->StringFromCharCode(), value);
  } else {
    // Retrieve the native context from the given {node}.
    Handle<Context> native_context;
    if (!GetNativeContext(node).ToHandle(&native_context)) return NoChange();

    // Compute element access infos for the receiver maps.
    AccessInfoFactory access_info_factory(dependencies(), native_context,
                                          graph()->zone());
    ZoneVector<ElementAccessInfo> access_infos(zone());
    if (!access_info_factory.ComputeElementAccessInfos(
            receiver_maps, access_mode, &access_infos)) {
      return NoChange();
    }

    // Nothing to do if we have no non-deprecated maps.
    if (access_infos.empty()) {
      return ReduceSoftDeoptimize(
          node,
          DeoptimizeReason::kInsufficientTypeFeedbackForGenericKeyedAccess);
    }

    // For holey stores or growing stores, we need to check that the prototype
    // chain contains no setters for elements, and we need to guard those checks
    // via code dependencies on the relevant prototype maps.
    if (access_mode == AccessMode::kStore) {
      // TODO(turbofan): We could have a fast path here, that checks for the
      // common case of Array or Object prototype only and therefore avoids
      // the zone allocation of this vector.
      ZoneVector<Handle<Map>> prototype_maps(zone());
      for (ElementAccessInfo const& access_info : access_infos) {
        for (Handle<Map> receiver_map : access_info.receiver_maps()) {
          // If the {receiver_map} has a prototype and it's elements backing
          // store is either holey, or we have a potentially growing store,
          // then we need to check that all prototypes have stable maps with
          // fast elements (and we need to guard against changes to that below).
          if (IsHoleyElementsKind(receiver_map->elements_kind()) ||
              IsGrowStoreMode(store_mode)) {
            // Make sure all prototypes are stable and have fast elements.
            for (Handle<Map> map = receiver_map;;) {
              Handle<Object> map_prototype(map->prototype(), isolate());
              if (map_prototype->IsNull(isolate())) break;
              if (!map_prototype->IsJSObject()) return NoChange();
              map = handle(Handle<JSObject>::cast(map_prototype)->map(),
                           isolate());
              if (!map->is_stable()) return NoChange();
              if (!IsFastElementsKind(map->elements_kind())) return NoChange();
              prototype_maps.push_back(map);
            }
          }
        }
      }

      // Install dependencies on the relevant prototype maps.
      for (Handle<Map> prototype_map : prototype_maps) {
        dependencies()->AssumeMapStable(prototype_map);
      }
    }

    // Ensure that {receiver} is a heap object.
    effect = BuildCheckHeapObject(receiver, effect, control);

    // Check for the monomorphic case.
    if (access_infos.size() == 1) {
      ElementAccessInfo access_info = access_infos.front();

      // Perform possible elements kind transitions.
      for (auto transition : access_info.transitions()) {
        Handle<Map> const transition_source = transition.first;
        Handle<Map> const transition_target = transition.second;
        effect = graph()->NewNode(
            simplified()->TransitionElementsKind(
                IsSimpleMapChangeTransition(transition_source->elements_kind(),
                                            transition_target->elements_kind())
                    ? ElementsTransition::kFastTransition
                    : ElementsTransition::kSlowTransition),
            receiver, jsgraph()->HeapConstant(transition_source),
            jsgraph()->HeapConstant(transition_target), effect, control);
      }

      // TODO(turbofan): The effect/control linearization will not find a
      // FrameState after the StoreField or Call that is generated for the
      // elements kind transition above. This is because those operators
      // don't have the kNoWrite flag on it, even though they are not
      // observable by JavaScript.
      effect = graph()->NewNode(common()->Checkpoint(), frame_state, effect,
                                control);

      // Perform map check on the {receiver}.
      effect = BuildCheckMaps(receiver, effect, control,
                              access_info.receiver_maps());

      // Access the actual element.
      ValueEffectControl continuation = BuildElementAccess(
          receiver, index, value, effect, control, native_context, access_info,
          access_mode, store_mode);
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
        for (auto transition : access_info.transitions()) {
          Handle<Map> const transition_source = transition.first;
          Handle<Map> const transition_target = transition.second;
          this_effect = graph()->NewNode(
              simplified()->TransitionElementsKind(
                  IsSimpleMapChangeTransition(
                      transition_source->elements_kind(),
                      transition_target->elements_kind())
                      ? ElementsTransition::kFastTransition
                      : ElementsTransition::kSlowTransition),
              receiver, jsgraph()->HeapConstant(transition_source),
              jsgraph()->HeapConstant(transition_target), this_effect,
              this_control);
        }

        // Load the {receiver} map.
        Node* receiver_map = this_effect =
            graph()->NewNode(simplified()->LoadField(AccessBuilder::ForMap()),
                             receiver, this_effect, this_control);

        // Perform map check(s) on {receiver}.
        MapList const& receiver_maps = access_info.receiver_maps();
        {
          ZoneVector<Node*> this_controls(zone());
          ZoneVector<Node*> this_effects(zone());
          size_t num_classes = receiver_maps.size();
          for (Handle<Map> map : receiver_maps) {
            DCHECK_LT(0u, num_classes);
            Node* check =
                graph()->NewNode(simplified()->ReferenceEqual(), receiver_map,
                                 jsgraph()->Constant(map));
            if (--num_classes == 0 && j == access_infos.size() - 1) {
              // Last map check on the fallthrough control path, do a
              // conditional eager deoptimization exit here.
              // TODO(turbofan): This is ugly as hell! We should probably
              // introduce macro-ish operators for property access that
              // encapsulate this whole mess.
              check = graph()->NewNode(simplified()->CheckIf(), check,
                                       this_effect, this_control);
              this_controls.push_back(this_control);
              this_effects.push_back(check);
              fallthrough_control = nullptr;
            } else {
              Node* branch = graph()->NewNode(common()->Branch(), check,
                                              fallthrough_control);
              this_controls.push_back(
                  graph()->NewNode(common()->IfTrue(), branch));
              this_effects.push_back(this_effect);
              fallthrough_control =
                  graph()->NewNode(common()->IfFalse(), branch);
            }
          }

          // Create single chokepoint for the control.
          int const this_control_count = static_cast<int>(this_controls.size());
          if (this_control_count == 1) {
            this_control = this_controls.front();
            this_effect = this_effects.front();
          } else {
            this_control =
                graph()->NewNode(common()->Merge(this_control_count),
                                 this_control_count, &this_controls.front());
            this_effects.push_back(this_control);
            this_effect =
                graph()->NewNode(common()->EffectPhi(this_control_count),
                                 this_control_count + 1, &this_effects.front());

            // TODO(turbofan): The effect/control linearization will not find a
            // FrameState after the EffectPhi that is generated above.
            this_effect = graph()->NewNode(common()->Checkpoint(), frame_state,
                                           this_effect, this_control);
          }
        }

        // Access the actual element.
        ValueEffectControl continuation = BuildElementAccess(
            this_receiver, this_index, this_value, this_effect, this_control,
            native_context, access_info, access_mode, store_mode);
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
        control = graph()->NewNode(common()->Merge(control_count),
                                   control_count, &controls.front());
        values.push_back(control);
        value = graph()->NewNode(
            common()->Phi(MachineRepresentation::kTagged, control_count),
            control_count + 1, &values.front());
        effects.push_back(control);
        effect = graph()->NewNode(common()->EffectPhi(control_count),
                                  control_count + 1, &effects.front());
      }
    }
  }

  ReplaceWithValue(node, value, effect, control);
  return Replace(value);
}

template <typename KeyedICNexus>
Reduction JSNativeContextSpecialization::ReduceKeyedAccess(
    Node* node, Node* index, Node* value, KeyedICNexus const& nexus,
    AccessMode access_mode, LanguageMode language_mode,
    KeyedAccessStoreMode store_mode) {
  DCHECK(node->opcode() == IrOpcode::kJSLoadProperty ||
         node->opcode() == IrOpcode::kJSStoreProperty);
  Node* const receiver = NodeProperties::GetValueInput(node, 0);
  Node* const effect = NodeProperties::GetEffectInput(node);

  // Check if the {nexus} reports type feedback for the IC.
  if (nexus.IsUninitialized()) {
    if ((flags() & kDeoptimizationEnabled) &&
        (flags() & kBailoutOnUninitialized)) {
      return ReduceSoftDeoptimize(
          node,
          DeoptimizeReason::kInsufficientTypeFeedbackForGenericKeyedAccess);
    }
    return NoChange();
  }

  // Extract receiver maps from the {nexus}.
  MapHandleList receiver_maps;
  if (!ExtractReceiverMaps(receiver, effect, nexus, &receiver_maps)) {
    return NoChange();
  } else if (receiver_maps.length() == 0) {
    if ((flags() & kDeoptimizationEnabled) &&
        (flags() & kBailoutOnUninitialized)) {
      return ReduceSoftDeoptimize(
          node,
          DeoptimizeReason::kInsufficientTypeFeedbackForGenericKeyedAccess);
    }
    return NoChange();
  }

  // Optimize access for constant {index}.
  HeapObjectMatcher mindex(index);
  if (mindex.HasValue() && mindex.Value()->IsPrimitive()) {
    // Keyed access requires a ToPropertyKey on the {index} first before
    // looking up the property on the object (see ES6 section 12.3.2.1).
    // We can only do this for non-observable ToPropertyKey invocations,
    // so we limit the constant indices to primitives at this point.
    Handle<Name> name;
    if (Object::ToName(isolate(), mindex.Value()).ToHandle(&name)) {
      uint32_t array_index;
      if (name->AsArrayIndex(&array_index)) {
        // Use the constant array index.
        index = jsgraph()->Constant(static_cast<double>(array_index));
      } else {
        name = factory()->InternalizeName(name);
        return ReduceNamedAccess(node, value, receiver_maps, name, access_mode,
                                 language_mode);
      }
    }
  }

  // Check if we have feedback for a named access.
  if (Name* name = nexus.FindFirstName()) {
    return ReduceNamedAccess(node, value, receiver_maps,
                             handle(name, isolate()), access_mode,
                             language_mode, index);
  } else if (nexus.GetKeyType() != ELEMENT) {
    // The KeyedLoad/StoreIC has seen non-element accesses, so we cannot assume
    // that the {index} is a valid array index, thus we just let the IC continue
    // to deal with this load/store.
    return NoChange();
  }

  // Try to lower the element access based on the {receiver_maps}.
  return ReduceElementAccess(node, index, value, receiver_maps, access_mode,
                             language_mode, store_mode);
}

Reduction JSNativeContextSpecialization::ReduceSoftDeoptimize(
    Node* node, DeoptimizeReason reason) {
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* frame_state = NodeProperties::FindFrameStateBefore(node);
  Node* deoptimize =
      graph()->NewNode(common()->Deoptimize(DeoptimizeKind::kSoft, reason),
                       frame_state, effect, control);
  // TODO(bmeurer): This should be on the AdvancedReducer somehow.
  NodeProperties::MergeControlToEnd(graph(), common(), deoptimize);
  Revisit(graph()->end());
  node->TrimInputCount(0);
  NodeProperties::ChangeOp(node, common()->Dead());
  return Changed(node);
}


Reduction JSNativeContextSpecialization::ReduceJSLoadProperty(Node* node) {
  DCHECK_EQ(IrOpcode::kJSLoadProperty, node->opcode());
  PropertyAccess const& p = PropertyAccessOf(node->op());
  Node* const index = NodeProperties::GetValueInput(node, 1);
  Node* const value = jsgraph()->Dead();

  // Extract receiver maps from the KEYED_LOAD_IC using the KeyedLoadICNexus.
  if (!p.feedback().IsValid()) return NoChange();
  KeyedLoadICNexus nexus(p.feedback().vector(), p.feedback().slot());

  // Try to lower the keyed access based on the {nexus}.
  return ReduceKeyedAccess(node, index, value, nexus, AccessMode::kLoad,
                           p.language_mode(), STANDARD_STORE);
}


Reduction JSNativeContextSpecialization::ReduceJSStoreProperty(Node* node) {
  DCHECK_EQ(IrOpcode::kJSStoreProperty, node->opcode());
  PropertyAccess const& p = PropertyAccessOf(node->op());
  Node* const index = NodeProperties::GetValueInput(node, 1);
  Node* const value = NodeProperties::GetValueInput(node, 2);

  // Extract receiver maps from the KEYED_STORE_IC using the KeyedStoreICNexus.
  if (!p.feedback().IsValid()) return NoChange();
  KeyedStoreICNexus nexus(p.feedback().vector(), p.feedback().slot());

  // Extract the keyed access store mode from the KEYED_STORE_IC.
  KeyedAccessStoreMode store_mode = nexus.GetKeyedAccessStoreMode();

  // Try to lower the keyed access based on the {nexus}.
  return ReduceKeyedAccess(node, index, value, nexus, AccessMode::kStore,
                           p.language_mode(), store_mode);
}

JSNativeContextSpecialization::ValueEffectControl
JSNativeContextSpecialization::BuildPropertyAccess(
    Node* receiver, Node* value, Node* context, Node* frame_state, Node* effect,
    Node* control, Handle<Name> name, Handle<Context> native_context,
    PropertyAccessInfo const& access_info, AccessMode access_mode) {
  // Determine actual holder and perform prototype chain checks.
  Handle<JSObject> holder;
  if (access_info.holder().ToHandle(&holder)) {
    AssumePrototypesStable(access_info.receiver_maps(), native_context, holder);
  }

  // Generate the actual property access.
  if (access_info.IsNotFound()) {
    DCHECK_EQ(AccessMode::kLoad, access_mode);
    value = jsgraph()->UndefinedConstant();
  } else if (access_info.IsDataConstant()) {
    Node* constant_value = jsgraph()->Constant(access_info.constant());
    if (access_mode == AccessMode::kStore) {
      Node* check = graph()->NewNode(simplified()->ReferenceEqual(), value,
                                     constant_value);
      effect =
          graph()->NewNode(simplified()->CheckIf(), check, effect, control);
    }
    value = constant_value;
  } else if (access_info.IsAccessorConstant()) {
    // TODO(bmeurer): Properly rewire the IfException edge here if there's any.
    Node* target = jsgraph()->Constant(access_info.constant());
    FrameStateInfo const& frame_info = OpParameter<FrameStateInfo>(frame_state);
    Handle<SharedFunctionInfo> shared_info =
        frame_info.shared_info().ToHandleChecked();
    switch (access_mode) {
      case AccessMode::kLoad: {
        // We need a FrameState for the getter stub to restore the correct
        // context before returning to fullcodegen.
        FrameStateFunctionInfo const* frame_info0 =
            common()->CreateFrameStateFunctionInfo(FrameStateType::kGetterStub,
                                                   1, 0, shared_info);
        Node* frame_state0 = graph()->NewNode(
            common()->FrameState(BailoutId::None(),
                                 OutputFrameStateCombine::Ignore(),
                                 frame_info0),
            graph()->NewNode(common()->StateValues(1), receiver),
            jsgraph()->EmptyStateValues(), jsgraph()->EmptyStateValues(),
            context, target, frame_state);

        // Introduce the call to the getter function.
        value = effect = graph()->NewNode(
            javascript()->CallFunction(
                2, 0.0f, VectorSlotPair(),
                ConvertReceiverMode::kNotNullOrUndefined),
            target, receiver, context, frame_state0, effect, control);
        control = graph()->NewNode(common()->IfSuccess(), value);
        break;
      }
      case AccessMode::kStore: {
        // We need a FrameState for the setter stub to restore the correct
        // context and return the appropriate value to fullcodegen.
        FrameStateFunctionInfo const* frame_info0 =
            common()->CreateFrameStateFunctionInfo(FrameStateType::kSetterStub,
                                                   2, 0, shared_info);
        Node* frame_state0 = graph()->NewNode(
            common()->FrameState(BailoutId::None(),
                                 OutputFrameStateCombine::Ignore(),
                                 frame_info0),
            graph()->NewNode(common()->StateValues(2), receiver, value),
            jsgraph()->EmptyStateValues(), jsgraph()->EmptyStateValues(),
            context, target, frame_state);

        // Introduce the call to the setter function.
        effect = graph()->NewNode(javascript()->CallFunction(
                                      3, 0.0f, VectorSlotPair(),
                                      ConvertReceiverMode::kNotNullOrUndefined),
                                  target, receiver, value, context,
                                  frame_state0, effect, control);
        control = graph()->NewNode(common()->IfSuccess(), effect);
        break;
      }
    }
  } else {
    DCHECK(access_info.IsDataField());
    FieldIndex const field_index = access_info.field_index();
    Type* const field_type = access_info.field_type();
    MachineRepresentation const field_representation =
        access_info.field_representation();
    if (access_mode == AccessMode::kLoad) {
      if (access_info.holder().ToHandle(&holder)) {
        receiver = jsgraph()->Constant(holder);
      }
      // Optimize immutable property loads.
      HeapObjectMatcher m(receiver);
      if (m.HasValue() && m.Value()->IsJSObject()) {
        // TODO(turbofan): Given that we already have the field_index here, we
        // might be smarter in the future and not rely on the LookupIterator,
        // but for now let's just do what Crankshaft does.
        LookupIterator it(m.Value(), name,
                          LookupIterator::OWN_SKIP_INTERCEPTOR);
        if (it.IsFound() && it.IsReadOnly() && !it.IsConfigurable()) {
          Node* value = jsgraph()->Constant(JSReceiver::GetDataProperty(&it));
          return ValueEffectControl(value, effect, control);
        }
      }
    }
    Node* storage = receiver;
    if (!field_index.is_inobject()) {
      storage = effect = graph()->NewNode(
          simplified()->LoadField(AccessBuilder::ForJSObjectProperties()),
          storage, effect, control);
    }
    FieldAccess field_access = {
        kTaggedBase,
        field_index.offset(),
        name,
        field_type,
        MachineType::TypeForRepresentation(field_representation),
        kFullWriteBarrier};
    if (access_mode == AccessMode::kLoad) {
      if (field_representation == MachineRepresentation::kFloat64) {
        if (!field_index.is_inobject() || field_index.is_hidden_field() ||
            !FLAG_unbox_double_fields) {
          FieldAccess const storage_access = {kTaggedBase,
                                              field_index.offset(),
                                              name,
                                              Type::OtherInternal(),
                                              MachineType::TaggedPointer(),
                                              kPointerWriteBarrier};
          storage = effect =
              graph()->NewNode(simplified()->LoadField(storage_access), storage,
                               effect, control);
          field_access.offset = HeapNumber::kValueOffset;
          field_access.name = MaybeHandle<Name>();
        }
      }
      // TODO(turbofan): Track the field_map (if any) on the {field_access} and
      // use it in LoadElimination to eliminate map checks.
      value = effect = graph()->NewNode(simplified()->LoadField(field_access),
                                        storage, effect, control);
    } else {
      DCHECK_EQ(AccessMode::kStore, access_mode);
      switch (field_representation) {
        case MachineRepresentation::kFloat64: {
          value = effect = graph()->NewNode(simplified()->CheckNumber(), value,
                                            effect, control);
          if (!field_index.is_inobject() || field_index.is_hidden_field() ||
              !FLAG_unbox_double_fields) {
            if (access_info.HasTransitionMap()) {
              // Allocate a MutableHeapNumber for the new property.
              effect = graph()->NewNode(
                  common()->BeginRegion(RegionObservability::kNotObservable),
                  effect);
              Node* box = effect = graph()->NewNode(
                  simplified()->Allocate(NOT_TENURED),
                  jsgraph()->Constant(HeapNumber::kSize), effect, control);
              effect = graph()->NewNode(
                  simplified()->StoreField(AccessBuilder::ForMap()), box,
                  jsgraph()->HeapConstant(factory()->mutable_heap_number_map()),
                  effect, control);
              effect = graph()->NewNode(
                  simplified()->StoreField(AccessBuilder::ForHeapNumberValue()),
                  box, value, effect, control);
              value = effect =
                  graph()->NewNode(common()->FinishRegion(), box, effect);

              field_access.type = Type::Any();
              field_access.machine_type = MachineType::TaggedPointer();
              field_access.write_barrier_kind = kPointerWriteBarrier;
            } else {
              // We just store directly to the MutableHeapNumber.
              FieldAccess const storage_access = {kTaggedBase,
                                                  field_index.offset(),
                                                  name,
                                                  Type::OtherInternal(),
                                                  MachineType::TaggedPointer(),
                                                  kPointerWriteBarrier};
              storage = effect =
                  graph()->NewNode(simplified()->LoadField(storage_access),
                                   storage, effect, control);
              field_access.offset = HeapNumber::kValueOffset;
              field_access.name = MaybeHandle<Name>();
              field_access.machine_type = MachineType::Float64();
            }
          }
          break;
        }
        case MachineRepresentation::kTaggedSigned: {
          value = effect = graph()->NewNode(simplified()->CheckSmi(), value,
                                            effect, control);
          field_access.write_barrier_kind = kNoWriteBarrier;
          break;
        }
        case MachineRepresentation::kTaggedPointer: {
          // Ensure that {value} is a HeapObject.
          value = effect = graph()->NewNode(simplified()->CheckHeapObject(),
                                            value, effect, control);
          Handle<Map> field_map;
          if (access_info.field_map().ToHandle(&field_map)) {
            // Emit a map check for the value.
            effect = graph()->NewNode(simplified()->CheckMaps(1), value,
                                      jsgraph()->HeapConstant(field_map),
                                      effect, control);
          }
          field_access.write_barrier_kind = kPointerWriteBarrier;
          break;
        }
        case MachineRepresentation::kTagged:
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
      Handle<Map> transition_map;
      if (access_info.transition_map().ToHandle(&transition_map)) {
        effect = graph()->NewNode(
            common()->BeginRegion(RegionObservability::kObservable), effect);
        effect = graph()->NewNode(
            simplified()->StoreField(AccessBuilder::ForMap()), receiver,
            jsgraph()->Constant(transition_map), effect, control);
      }
      effect = graph()->NewNode(simplified()->StoreField(field_access), storage,
                                value, effect, control);
      if (access_info.HasTransitionMap()) {
        effect = graph()->NewNode(common()->FinishRegion(),
                                  jsgraph()->UndefinedConstant(), effect);
      }
    }
  }

  return ValueEffectControl(value, effect, control);
}

namespace {

ExternalArrayType GetArrayTypeFromElementsKind(ElementsKind kind) {
  switch (kind) {
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size) \
  case TYPE##_ELEMENTS:                                 \
    return kExternal##Type##Array;
    TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
    default:
      break;
  }
  UNREACHABLE();
  return kExternalInt8Array;
}

}  // namespace

JSNativeContextSpecialization::ValueEffectControl
JSNativeContextSpecialization::BuildElementAccess(
    Node* receiver, Node* index, Node* value, Node* effect, Node* control,
    Handle<Context> native_context, ElementAccessInfo const& access_info,
    AccessMode access_mode, KeyedAccessStoreMode store_mode) {
  // TODO(bmeurer): We currently specialize based on elements kind. We should
  // also be able to properly support strings and other JSObjects here.
  ElementsKind elements_kind = access_info.elements_kind();
  MapList const& receiver_maps = access_info.receiver_maps();

  // Load the elements for the {receiver}.
  Node* elements = effect = graph()->NewNode(
      simplified()->LoadField(AccessBuilder::ForJSObjectElements()), receiver,
      effect, control);

  // Don't try to store to a copy-on-write backing store.
  if (access_mode == AccessMode::kStore &&
      IsFastSmiOrObjectElementsKind(elements_kind) &&
      store_mode != STORE_NO_TRANSITION_HANDLE_COW) {
    effect =
        graph()->NewNode(simplified()->CheckMaps(1), elements,
                         jsgraph()->FixedArrayMapConstant(), effect, control);
  }

  if (IsFixedTypedArrayElementsKind(elements_kind)) {
    // Load the {receiver}s length.
    Node* length = effect = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForJSTypedArrayLength()),
        receiver, effect, control);

    // Check if the {receiver}s buffer was neutered.
    Node* buffer = effect = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForJSArrayBufferViewBuffer()),
        receiver, effect, control);
    Node* check = effect = graph()->NewNode(
        simplified()->ArrayBufferWasNeutered(), buffer, effect, control);

    // Default to zero if the {receiver}s buffer was neutered.
    length = graph()->NewNode(
        common()->Select(MachineRepresentation::kTagged, BranchHint::kFalse),
        check, jsgraph()->ZeroConstant(), length);

    if (store_mode == STORE_NO_TRANSITION_IGNORE_OUT_OF_BOUNDS) {
      // Check that the {index} is a valid array index, we do the actual
      // bounds check below and just skip the store below if it's out of
      // bounds for the {receiver}.
      index = effect = graph()->NewNode(simplified()->CheckBounds(), index,
                                        jsgraph()->Constant(Smi::kMaxValue),
                                        effect, control);
    } else {
      // Check that the {index} is in the valid range for the {receiver}.
      DCHECK_EQ(STANDARD_STORE, store_mode);
      index = effect = graph()->NewNode(simplified()->CheckBounds(), index,
                                        length, effect, control);
    }

    // Load the base and external pointer for the {receiver}.
    Node* base_pointer = effect = graph()->NewNode(
        simplified()->LoadField(
            AccessBuilder::ForFixedTypedArrayBaseBasePointer()),
        elements, effect, control);
    Node* external_pointer = effect = graph()->NewNode(
        simplified()->LoadField(
            AccessBuilder::ForFixedTypedArrayBaseExternalPointer()),
        elements, effect, control);

    // Access the actual element.
    ExternalArrayType external_array_type =
        GetArrayTypeFromElementsKind(elements_kind);
    switch (access_mode) {
      case AccessMode::kLoad: {
        value = effect = graph()->NewNode(
            simplified()->LoadTypedElement(external_array_type), buffer,
            base_pointer, external_pointer, index, effect, control);
        break;
      }
      case AccessMode::kStore: {
        // Ensure that the {value} is actually a Number.
        value = effect = graph()->NewNode(simplified()->CheckNumber(), value,
                                          effect, control);

        // Check if we can skip the out-of-bounds store.
        if (store_mode == STORE_NO_TRANSITION_IGNORE_OUT_OF_BOUNDS) {
          Node* check =
              graph()->NewNode(simplified()->NumberLessThan(), index, length);
          Node* branch = graph()->NewNode(common()->Branch(BranchHint::kTrue),
                                          check, control);

          Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
          Node* etrue = effect;
          {
            // Perform the actual store.
            etrue = graph()->NewNode(
                simplified()->StoreTypedElement(external_array_type), buffer,
                base_pointer, external_pointer, index, value, etrue, if_true);
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
          DCHECK_EQ(STANDARD_STORE, store_mode);
          effect = graph()->NewNode(
              simplified()->StoreTypedElement(external_array_type), buffer,
              base_pointer, external_pointer, index, value, effect, control);
        }
        break;
      }
    }
  } else {
    // Check if the {receiver} is a JSArray.
    bool receiver_is_jsarray = HasOnlyJSArrayMaps(receiver_maps);

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
      DCHECK_EQ(AccessMode::kStore, access_mode);

      // Check that the {index} is a valid array index; the actual checking
      // happens below right before the element store.
      index = effect = graph()->NewNode(simplified()->CheckBounds(), index,
                                        jsgraph()->Constant(Smi::kMaxValue),
                                        effect, control);
    } else {
      // Check that the {index} is in the valid range for the {receiver}.
      index = effect = graph()->NewNode(simplified()->CheckBounds(), index,
                                        length, effect, control);
    }

    // Compute the element access.
    Type* element_type = Type::NonInternal();
    MachineType element_machine_type = MachineType::AnyTagged();
    if (IsFastDoubleElementsKind(elements_kind)) {
      element_type = Type::Number();
      element_machine_type = MachineType::Float64();
    } else if (IsFastSmiElementsKind(elements_kind)) {
      element_type = type_cache_.kSmi;
      element_machine_type = MachineType::TaggedSigned();
    }
    ElementAccess element_access = {kTaggedBase, FixedArray::kHeaderSize,
                                    element_type, element_machine_type,
                                    kFullWriteBarrier};

    // Access the actual element.
    if (access_mode == AccessMode::kLoad) {
      // Compute the real element access type, which includes the hole in case
      // of holey backing stores.
      if (elements_kind == FAST_HOLEY_ELEMENTS ||
          elements_kind == FAST_HOLEY_SMI_ELEMENTS) {
        element_access.type =
            Type::Union(element_type, Type::Hole(), graph()->zone());
        element_access.machine_type = MachineType::AnyTagged();
      }
      // Perform the actual backing store access.
      value = effect =
          graph()->NewNode(simplified()->LoadElement(element_access), elements,
                           index, effect, control);
      // Handle loading from holey backing stores correctly, by either mapping
      // the hole to undefined if possible, or deoptimizing otherwise.
      if (elements_kind == FAST_HOLEY_ELEMENTS ||
          elements_kind == FAST_HOLEY_SMI_ELEMENTS) {
        // Check if we are allowed to turn the hole into undefined.
        if (CanTreatHoleAsUndefined(receiver_maps, native_context)) {
          // Turn the hole into undefined.
          value = graph()->NewNode(simplified()->ConvertTaggedHoleToUndefined(),
                                   value);
        } else {
          // Bailout if we see the hole.
          value = effect = graph()->NewNode(simplified()->CheckTaggedHole(),
                                            value, effect, control);
        }
      } else if (elements_kind == FAST_HOLEY_DOUBLE_ELEMENTS) {
        // Perform the hole check on the result.
        CheckFloat64HoleMode mode = CheckFloat64HoleMode::kNeverReturnHole;
        // Check if we are allowed to return the hole directly.
        if (CanTreatHoleAsUndefined(receiver_maps, native_context)) {
          // Return the signaling NaN hole directly if all uses are truncating.
          mode = CheckFloat64HoleMode::kAllowReturnHole;
        }
        value = effect = graph()->NewNode(simplified()->CheckFloat64Hole(mode),
                                          value, effect, control);
      }
    } else {
      DCHECK_EQ(AccessMode::kStore, access_mode);
      if (IsFastSmiElementsKind(elements_kind)) {
        value = effect =
            graph()->NewNode(simplified()->CheckSmi(), value, effect, control);
      } else if (IsFastDoubleElementsKind(elements_kind)) {
        value = effect = graph()->NewNode(simplified()->CheckNumber(), value,
                                          effect, control);
        // Make sure we do not store signalling NaNs into double arrays.
        value = graph()->NewNode(simplified()->NumberSilenceNaN(), value);
      }

      // Ensure that copy-on-write backing store is writable.
      if (IsFastSmiOrObjectElementsKind(elements_kind) &&
          store_mode == STORE_NO_TRANSITION_HANDLE_COW) {
        elements = effect =
            graph()->NewNode(simplified()->EnsureWritableFastElements(),
                             receiver, elements, effect, control);
      } else if (IsGrowStoreMode(store_mode)) {
        // Grow {elements} backing store if necessary. Also updates the
        // "length" property for JSArray {receiver}s, hence there must
        // not be any other check after this operation, as the write
        // to the "length" property is observable.
        GrowFastElementsFlags flags = GrowFastElementsFlag::kNone;
        if (receiver_is_jsarray) {
          flags |= GrowFastElementsFlag::kArrayObject;
        }
        if (IsHoleyElementsKind(elements_kind)) {
          flags |= GrowFastElementsFlag::kHoleyElements;
        }
        if (IsFastDoubleElementsKind(elements_kind)) {
          flags |= GrowFastElementsFlag::kDoubleElements;
        }
        elements = effect = graph()->NewNode(
            simplified()->MaybeGrowFastElements(flags), receiver, elements,
            index, length, effect, control);
      }

      // Perform the actual element access.
      effect = graph()->NewNode(simplified()->StoreElement(element_access),
                                elements, index, value, effect, control);
    }
  }

  return ValueEffectControl(value, effect, control);
}

Node* JSNativeContextSpecialization::BuildCheckMaps(
    Node* receiver, Node* effect, Node* control,
    std::vector<Handle<Map>> const& maps) {
  HeapObjectMatcher m(receiver);
  if (m.HasValue()) {
    Handle<Map> receiver_map(m.Value()->map(), isolate());
    if (receiver_map->is_stable()) {
      for (Handle<Map> map : maps) {
        if (map.is_identical_to(receiver_map)) {
          dependencies()->AssumeMapStable(receiver_map);
          return effect;
        }
      }
    }
  }
  int const map_input_count = static_cast<int>(maps.size());
  int const input_count = 1 + map_input_count + 1 + 1;
  Node** inputs = zone()->NewArray<Node*>(input_count);
  inputs[0] = receiver;
  for (int i = 0; i < map_input_count; ++i) {
    inputs[1 + i] = jsgraph()->HeapConstant(maps[i]);
  }
  inputs[input_count - 2] = effect;
  inputs[input_count - 1] = control;
  return graph()->NewNode(simplified()->CheckMaps(map_input_count), input_count,
                          inputs);
}

Node* JSNativeContextSpecialization::BuildCheckHeapObject(Node* receiver,
                                                          Node* effect,
                                                          Node* control) {
  switch (receiver->opcode()) {
    case IrOpcode::kHeapConstant:
    case IrOpcode::kJSCreate:
    case IrOpcode::kJSCreateArguments:
    case IrOpcode::kJSCreateArray:
    case IrOpcode::kJSCreateClosure:
    case IrOpcode::kJSCreateIterResultObject:
    case IrOpcode::kJSCreateLiteralArray:
    case IrOpcode::kJSCreateLiteralObject:
    case IrOpcode::kJSCreateLiteralRegExp:
    case IrOpcode::kJSConvertReceiver:
    case IrOpcode::kJSToName:
    case IrOpcode::kJSToString:
    case IrOpcode::kJSToObject:
    case IrOpcode::kJSTypeOf: {
      return effect;
    }
    default: {
      return graph()->NewNode(simplified()->CheckHeapObject(), receiver, effect,
                              control);
    }
  }
}

void JSNativeContextSpecialization::AssumePrototypesStable(
    std::vector<Handle<Map>> const& receiver_maps,
    Handle<Context> native_context, Handle<JSObject> holder) {
  // Determine actual holder and perform prototype chain checks.
  for (auto map : receiver_maps) {
    // Perform the implicit ToObject for primitives here.
    // Implemented according to ES6 section 7.3.2 GetV (V, P).
    Handle<JSFunction> constructor;
    if (Map::GetConstructorFunction(map, native_context)
            .ToHandle(&constructor)) {
      map = handle(constructor->initial_map(), isolate());
    }
    dependencies()->AssumePrototypeMapsStable(map, holder);
  }
}

bool JSNativeContextSpecialization::CanTreatHoleAsUndefined(
    std::vector<Handle<Map>> const& receiver_maps,
    Handle<Context> native_context) {
  // Check if the array prototype chain is intact.
  if (!isolate()->IsFastArrayConstructorPrototypeChainIntact()) return false;

  // Make sure both the initial Array and Object prototypes are stable.
  Handle<JSObject> initial_array_prototype(
      native_context->initial_array_prototype(), isolate());
  Handle<JSObject> initial_object_prototype(
      native_context->initial_object_prototype(), isolate());
  if (!initial_array_prototype->map()->is_stable() ||
      !initial_object_prototype->map()->is_stable()) {
    return false;
  }

  // Check if all {receiver_maps} either have the initial Array.prototype
  // or the initial Object.prototype as their prototype, as those are
  // guarded by the array protector cell.
  for (Handle<Map> map : receiver_maps) {
    if (map->prototype() != *initial_array_prototype &&
        map->prototype() != *initial_object_prototype) {
      return false;
    }
  }

  // Install code dependencies on the prototype maps.
  for (Handle<Map> map : receiver_maps) {
    dependencies()->AssumePrototypeMapsStable(map, initial_object_prototype);
  }

  // Install code dependency on the array protector cell.
  dependencies()->AssumePropertyCell(factory()->array_protector());
  return true;
}

bool JSNativeContextSpecialization::ExtractReceiverMaps(
    Node* receiver, Node* effect, FeedbackNexus const& nexus,
    MapHandleList* receiver_maps) {
  DCHECK_EQ(0, receiver_maps->length());
  // See if we can infer a concrete type for the {receiver}.
  Handle<Map> receiver_map;
  if (InferReceiverMap(receiver, effect).ToHandle(&receiver_map)) {
    // We can assume that the {receiver} still has the infered {receiver_map}.
    receiver_maps->Add(receiver_map);
    return true;
  }
  // Try to extract some maps from the {nexus}.
  if (nexus.ExtractMaps(receiver_maps) != 0) {
    // Try to filter impossible candidates based on infered root map.
    if (InferReceiverRootMap(receiver).ToHandle(&receiver_map)) {
      for (int i = receiver_maps->length(); --i >= 0;) {
        if (receiver_maps->at(i)->FindRootMap() != *receiver_map) {
          receiver_maps->Remove(i);
        }
      }
    }
    return true;
  }
  return false;
}

MaybeHandle<Map> JSNativeContextSpecialization::InferReceiverMap(Node* receiver,
                                                                 Node* effect) {
  HeapObjectMatcher m(receiver);
  if (m.HasValue()) {
    Handle<Map> receiver_map(m.Value()->map(), isolate());
    if (receiver_map->is_stable()) return receiver_map;
  } else if (m.IsJSCreate()) {
    HeapObjectMatcher mtarget(m.InputAt(0));
    HeapObjectMatcher mnewtarget(m.InputAt(1));
    if (mtarget.HasValue() && mnewtarget.HasValue()) {
      Handle<JSFunction> constructor =
          Handle<JSFunction>::cast(mtarget.Value());
      if (constructor->has_initial_map()) {
        Handle<Map> initial_map(constructor->initial_map(), isolate());
        if (initial_map->constructor_or_backpointer() == *mnewtarget.Value()) {
          // Walk up the {effect} chain to see if the {receiver} is the
          // dominating effect and there's no other observable write in
          // between.
          while (true) {
            if (receiver == effect) return initial_map;
            if (!effect->op()->HasProperty(Operator::kNoWrite) ||
                effect->op()->EffectInputCount() != 1) {
              break;
            }
            effect = NodeProperties::GetEffectInput(effect);
          }
        }
      }
    }
  }
  // TODO(turbofan): Go hunting for CheckMaps(receiver) in the effect chain?
  return MaybeHandle<Map>();
}

MaybeHandle<Map> JSNativeContextSpecialization::InferReceiverRootMap(
    Node* receiver) {
  HeapObjectMatcher m(receiver);
  if (m.HasValue()) {
    return handle(m.Value()->map()->FindRootMap(), isolate());
  } else if (m.IsJSCreate()) {
    HeapObjectMatcher mtarget(m.InputAt(0));
    HeapObjectMatcher mnewtarget(m.InputAt(1));
    if (mtarget.HasValue() && mnewtarget.HasValue()) {
      Handle<JSFunction> constructor =
          Handle<JSFunction>::cast(mtarget.Value());
      if (constructor->has_initial_map()) {
        Handle<Map> initial_map(constructor->initial_map(), isolate());
        if (initial_map->constructor_or_backpointer() == *mnewtarget.Value()) {
          DCHECK_EQ(*initial_map, initial_map->FindRootMap());
          return initial_map;
        }
      }
    }
  }
  return MaybeHandle<Map>();
}

MaybeHandle<Context> JSNativeContextSpecialization::GetNativeContext(
    Node* node) {
  Node* const context = NodeProperties::GetContextInput(node);
  return NodeProperties::GetSpecializationNativeContext(context,
                                                        native_context());
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


MachineOperatorBuilder* JSNativeContextSpecialization::machine() const {
  return jsgraph()->machine();
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

}  // namespace compiler
}  // namespace internal
}  // namespace v8
