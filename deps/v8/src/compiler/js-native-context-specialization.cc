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
#include "src/field-index-inl.h"
#include "src/isolate-inl.h"
#include "src/objects-inl.h"  // TODO(mstarzinger): Temporary cycle breaker!
#include "src/type-cache.h"
#include "src/type-feedback-vector.h"

namespace v8 {
namespace internal {
namespace compiler {

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
  Node* frame_state = NodeProperties::GetFrameStateInput(node, 1);
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

  // Nothing to do if we have no non-deprecated maps.
  if (access_infos.empty()) return NoChange();

  // The final states for every polymorphic branch. We join them with
  // Merge++Phi+EffectPhi at the bottom.
  ZoneVector<Node*> values(zone());
  ZoneVector<Node*> effects(zone());
  ZoneVector<Node*> controls(zone());

  // Ensure that {index} matches the specified {name} (if {index} is given).
  if (index != nullptr) {
    Node* check = graph()->NewNode(simplified()->ReferenceEqual(Type::Name()),
                                   index, jsgraph()->HeapConstant(name));
    control = graph()->NewNode(common()->DeoptimizeUnless(), check, frame_state,
                               effect, control);
  }

  // Check if {receiver} may be a number.
  bool receiverissmi_possible = false;
  for (PropertyAccessInfo const& access_info : access_infos) {
    if (access_info.receiver_type()->Is(Type::Number())) {
      receiverissmi_possible = true;
      break;
    }
  }

  // Ensure that {receiver} is a heap object.
  Node* check = graph()->NewNode(simplified()->ObjectIsSmi(), receiver);
  Node* receiverissmi_control = nullptr;
  Node* receiverissmi_effect = effect;
  if (receiverissmi_possible) {
    Node* branch = graph()->NewNode(common()->Branch(), check, control);
    control = graph()->NewNode(common()->IfFalse(), branch);
    receiverissmi_control = graph()->NewNode(common()->IfTrue(), branch);
    receiverissmi_effect = effect;
  } else {
    control = graph()->NewNode(common()->DeoptimizeIf(), check, frame_state,
                               effect, control);
  }

  // Load the {receiver} map. The resulting effect is the dominating effect for
  // all (polymorphic) branches.
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
    Type* receiver_type = access_info.receiver_type();
    if (receiver_type->Is(Type::String())) {
      Node* check = graph()->NewNode(simplified()->ObjectIsString(), receiver);
      if (j == access_infos.size() - 1) {
        this_control =
            graph()->NewNode(common()->DeoptimizeUnless(), check, frame_state,
                             this_effect, fallthrough_control);
        fallthrough_control = nullptr;
      } else {
        Node* branch =
            graph()->NewNode(common()->Branch(), check, fallthrough_control);
        fallthrough_control = graph()->NewNode(common()->IfFalse(), branch);
        this_control = graph()->NewNode(common()->IfTrue(), branch);
      }
    } else {
      // Emit a (sequence of) map checks for other {receiver}s.
      ZoneVector<Node*> this_controls(zone());
      ZoneVector<Node*> this_effects(zone());
      int num_classes = access_info.receiver_type()->NumClasses();
      for (auto i = access_info.receiver_type()->Classes(); !i.Done();
           i.Advance()) {
        DCHECK_LT(0, num_classes);
        Handle<Map> map = i.Current();
        Node* check =
            graph()->NewNode(simplified()->ReferenceEqual(Type::Internal()),
                             receiver_map, jsgraph()->Constant(map));
        if (--num_classes == 0 && j == access_infos.size() - 1) {
          this_controls.push_back(
              graph()->NewNode(common()->DeoptimizeUnless(), check, frame_state,
                               this_effect, fallthrough_control));
          this_effects.push_back(this_effect);
          fallthrough_control = nullptr;
        } else {
          Node* branch =
              graph()->NewNode(common()->Branch(), check, fallthrough_control);
          fallthrough_control = graph()->NewNode(common()->IfFalse(), branch);
          this_controls.push_back(graph()->NewNode(common()->IfTrue(), branch));
          this_effects.push_back(this_effect);
        }
      }

      // The Number case requires special treatment to also deal with Smis.
      if (receiver_type->Is(Type::Number())) {
        // Join this check with the "receiver is smi" check above.
        DCHECK_NOT_NULL(receiverissmi_effect);
        DCHECK_NOT_NULL(receiverissmi_control);
        this_effects.push_back(receiverissmi_effect);
        this_controls.push_back(receiverissmi_control);
        receiverissmi_effect = receiverissmi_control = nullptr;
      }

      // Create dominating Merge+EffectPhi for this {receiver} type.
      int const this_control_count = static_cast<int>(this_controls.size());
      this_control =
          (this_control_count == 1)
              ? this_controls.front()
              : graph()->NewNode(common()->Merge(this_control_count),
                                 this_control_count, &this_controls.front());
      this_effects.push_back(this_control);
      int const this_effect_count = static_cast<int>(this_effects.size());
      this_effect =
          (this_control_count == 1)
              ? this_effects.front()
              : graph()->NewNode(common()->EffectPhi(this_control_count),
                                 this_effect_count, &this_effects.front());
    }

    // Determine actual holder and perform prototype chain checks.
    Handle<JSObject> holder;
    if (access_info.holder().ToHandle(&holder)) {
      AssumePrototypesStable(receiver_type, native_context, holder);
    }

    // Generate the actual property access.
    if (access_info.IsNotFound()) {
      DCHECK_EQ(AccessMode::kLoad, access_mode);
      this_value = jsgraph()->UndefinedConstant();
    } else if (access_info.IsDataConstant()) {
      this_value = jsgraph()->Constant(access_info.constant());
      if (access_mode == AccessMode::kStore) {
        Node* check = graph()->NewNode(
            simplified()->ReferenceEqual(Type::Tagged()), value, this_value);
        this_control = graph()->NewNode(common()->DeoptimizeUnless(), check,
                                        frame_state, this_effect, this_control);
      }
    } else {
      DCHECK(access_info.IsDataField());
      FieldIndex const field_index = access_info.field_index();
      FieldCheck const field_check = access_info.field_check();
      Type* const field_type = access_info.field_type();
      switch (field_check) {
        case FieldCheck::kNone:
          break;
        case FieldCheck::kJSArrayBufferViewBufferNotNeutered: {
          Node* this_buffer = this_effect =
              graph()->NewNode(simplified()->LoadField(
                                   AccessBuilder::ForJSArrayBufferViewBuffer()),
                               this_receiver, this_effect, this_control);
          Node* this_buffer_bit_field = this_effect =
              graph()->NewNode(simplified()->LoadField(
                                   AccessBuilder::ForJSArrayBufferBitField()),
                               this_buffer, this_effect, this_control);
          Node* check = graph()->NewNode(
              machine()->Word32Equal(),
              graph()->NewNode(machine()->Word32And(), this_buffer_bit_field,
                               jsgraph()->Int32Constant(
                                   1 << JSArrayBuffer::WasNeutered::kShift)),
              jsgraph()->Int32Constant(0));
          this_control =
              graph()->NewNode(common()->DeoptimizeUnless(), check, frame_state,
                               this_effect, this_control);
          break;
        }
      }
      if (access_mode == AccessMode::kLoad &&
          access_info.holder().ToHandle(&holder)) {
        this_receiver = jsgraph()->Constant(holder);
      }
      Node* this_storage = this_receiver;
      if (!field_index.is_inobject()) {
        this_storage = this_effect = graph()->NewNode(
            simplified()->LoadField(AccessBuilder::ForJSObjectProperties()),
            this_storage, this_effect, this_control);
      }
      FieldAccess field_access = {
          kTaggedBase, field_index.offset(),     name,
          field_type,  MachineType::AnyTagged(), kFullWriteBarrier};
      if (access_mode == AccessMode::kLoad) {
        if (field_type->Is(Type::UntaggedFloat64())) {
          if (!field_index.is_inobject() || field_index.is_hidden_field() ||
              !FLAG_unbox_double_fields) {
            this_storage = this_effect =
                graph()->NewNode(simplified()->LoadField(field_access),
                                 this_storage, this_effect, this_control);
            field_access.offset = HeapNumber::kValueOffset;
            field_access.name = MaybeHandle<Name>();
          }
          field_access.machine_type = MachineType::Float64();
        }
        this_value = this_effect =
            graph()->NewNode(simplified()->LoadField(field_access),
                             this_storage, this_effect, this_control);
      } else {
        DCHECK_EQ(AccessMode::kStore, access_mode);
        if (field_type->Is(Type::UntaggedFloat64())) {
          Node* check =
              graph()->NewNode(simplified()->ObjectIsNumber(), this_value);
          this_control =
              graph()->NewNode(common()->DeoptimizeUnless(), check, frame_state,
                               this_effect, this_control);
          this_value = graph()->NewNode(simplified()->TypeGuard(Type::Number()),
                                        this_value, this_control);

          if (!field_index.is_inobject() || field_index.is_hidden_field() ||
              !FLAG_unbox_double_fields) {
            if (access_info.HasTransitionMap()) {
              // Allocate a MutableHeapNumber for the new property.
              this_effect =
                  graph()->NewNode(common()->BeginRegion(), this_effect);
              Node* this_box = this_effect =
                  graph()->NewNode(simplified()->Allocate(NOT_TENURED),
                                   jsgraph()->Constant(HeapNumber::kSize),
                                   this_effect, this_control);
              this_effect = graph()->NewNode(
                  simplified()->StoreField(AccessBuilder::ForMap()), this_box,
                  jsgraph()->HeapConstant(factory()->mutable_heap_number_map()),
                  this_effect, this_control);
              this_effect = graph()->NewNode(
                  simplified()->StoreField(AccessBuilder::ForHeapNumberValue()),
                  this_box, this_value, this_effect, this_control);
              this_value = this_effect = graph()->NewNode(
                  common()->FinishRegion(), this_box, this_effect);

              field_access.type = Type::TaggedPointer();
            } else {
              // We just store directly to the MutableHeapNumber.
              this_storage = this_effect =
                  graph()->NewNode(simplified()->LoadField(field_access),
                                   this_storage, this_effect, this_control);
              field_access.offset = HeapNumber::kValueOffset;
              field_access.name = MaybeHandle<Name>();
              field_access.machine_type = MachineType::Float64();
            }
          } else {
            // Unboxed double field, we store directly to the field.
            field_access.machine_type = MachineType::Float64();
          }
        } else if (field_type->Is(Type::TaggedSigned())) {
          Node* check =
              graph()->NewNode(simplified()->ObjectIsSmi(), this_value);
          this_control =
              graph()->NewNode(common()->DeoptimizeUnless(), check, frame_state,
                               this_effect, this_control);
          this_value =
              graph()->NewNode(simplified()->TypeGuard(type_cache_.kSmi),
                               this_value, this_control);
        } else if (field_type->Is(Type::TaggedPointer())) {
          Node* check =
              graph()->NewNode(simplified()->ObjectIsSmi(), this_value);
          this_control =
              graph()->NewNode(common()->DeoptimizeIf(), check, frame_state,
                               this_effect, this_control);
          if (field_type->NumClasses() == 1) {
            // Emit a map check for the value.
            Node* this_value_map = this_effect = graph()->NewNode(
                simplified()->LoadField(AccessBuilder::ForMap()), this_value,
                this_effect, this_control);
            Node* check = graph()->NewNode(
                simplified()->ReferenceEqual(Type::Internal()), this_value_map,
                jsgraph()->Constant(field_type->Classes().Current()));
            this_control =
                graph()->NewNode(common()->DeoptimizeUnless(), check,
                                 frame_state, this_effect, this_control);
          } else {
            DCHECK_EQ(0, field_type->NumClasses());
          }
        } else {
          DCHECK(field_type->Is(Type::Tagged()));
        }
        Handle<Map> transition_map;
        if (access_info.transition_map().ToHandle(&transition_map)) {
          this_effect = graph()->NewNode(common()->BeginRegion(), this_effect);
          this_effect = graph()->NewNode(
              simplified()->StoreField(AccessBuilder::ForMap()), this_receiver,
              jsgraph()->Constant(transition_map), this_effect, this_control);
        }
        this_effect = graph()->NewNode(simplified()->StoreField(field_access),
                                       this_storage, this_value, this_effect,
                                       this_control);
        if (access_info.HasTransitionMap()) {
          this_effect =
              graph()->NewNode(common()->FinishRegion(),
                               jsgraph()->UndefinedConstant(), this_effect);
        }
      }
    }

    // Remember the final state for this property access.
    values.push_back(this_value);
    effects.push_back(this_effect);
    controls.push_back(this_control);
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
  ReplaceWithValue(node, value, effect, control);
  return Replace(value);
}


Reduction JSNativeContextSpecialization::ReduceNamedAccess(
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
      return ReduceSoftDeoptimize(node);
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
      return ReduceSoftDeoptimize(node);
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
  return ReduceNamedAccess(node, value, nexus, p.name(), AccessMode::kLoad,
                           p.language_mode());
}


Reduction JSNativeContextSpecialization::ReduceJSStoreNamed(Node* node) {
  DCHECK_EQ(IrOpcode::kJSStoreNamed, node->opcode());
  NamedAccess const& p = NamedAccessOf(node->op());
  Node* const value = NodeProperties::GetValueInput(node, 1);

  // Extract receiver maps from the STORE_IC using the StoreICNexus.
  if (!p.feedback().IsValid()) return NoChange();
  StoreICNexus nexus(p.feedback().vector(), p.feedback().slot());

  // Try to lower the named access based on the {receiver_maps}.
  return ReduceNamedAccess(node, value, nexus, p.name(), AccessMode::kStore,
                           p.language_mode());
}


Reduction JSNativeContextSpecialization::ReduceElementAccess(
    Node* node, Node* index, Node* value, MapHandleList const& receiver_maps,
    AccessMode access_mode, LanguageMode language_mode,
    KeyedAccessStoreMode store_mode) {
  DCHECK(node->opcode() == IrOpcode::kJSLoadProperty ||
         node->opcode() == IrOpcode::kJSStoreProperty);
  Node* receiver = NodeProperties::GetValueInput(node, 0);
  Node* context = NodeProperties::GetContextInput(node);
  Node* frame_state = NodeProperties::GetFrameStateInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // Not much we can do if deoptimization support is disabled.
  if (!(flags() & kDeoptimizationEnabled)) return NoChange();

  // TODO(bmeurer): Add support for non-standard stores.
  if (store_mode != STANDARD_STORE) return NoChange();

  // Retrieve the native context from the given {node}.
  Handle<Context> native_context;
  if (!GetNativeContext(node).ToHandle(&native_context)) return NoChange();

  // Compute element access infos for the receiver maps.
  AccessInfoFactory access_info_factory(dependencies(), native_context,
                                        graph()->zone());
  ZoneVector<ElementAccessInfo> access_infos(zone());
  if (!access_info_factory.ComputeElementAccessInfos(receiver_maps, access_mode,
                                                     &access_infos)) {
    return NoChange();
  }

  // Nothing to do if we have no non-deprecated maps.
  if (access_infos.empty()) return NoChange();

  // The final states for every polymorphic branch. We join them with
  // Merge+Phi+EffectPhi at the bottom.
  ZoneVector<Node*> values(zone());
  ZoneVector<Node*> effects(zone());
  ZoneVector<Node*> controls(zone());

  // Ensure that {receiver} is a heap object.
  Node* check = graph()->NewNode(simplified()->ObjectIsSmi(), receiver);
  control = graph()->NewNode(common()->DeoptimizeIf(), check, frame_state,
                             effect, control);

  // Load the {receiver} map. The resulting effect is the dominating effect for
  // all (polymorphic) branches.
  Node* receiver_map = effect =
      graph()->NewNode(simplified()->LoadField(AccessBuilder::ForMap()),
                       receiver, effect, control);

  // Generate code for the various different element access patterns.
  Node* fallthrough_control = control;
  for (size_t j = 0; j < access_infos.size(); ++j) {
    ElementAccessInfo const& access_info = access_infos[j];
    Node* this_receiver = receiver;
    Node* this_value = value;
    Node* this_index = index;
    Node* this_effect;
    Node* this_control;

    // Perform map check on {receiver}.
    Type* receiver_type = access_info.receiver_type();
    bool receiver_is_jsarray = true;
    {
      ZoneVector<Node*> this_controls(zone());
      ZoneVector<Node*> this_effects(zone());
      size_t num_transitions = access_info.transitions().size();
      int num_classes = access_info.receiver_type()->NumClasses();
      for (auto i = access_info.receiver_type()->Classes(); !i.Done();
           i.Advance()) {
        DCHECK_LT(0, num_classes);
        Handle<Map> map = i.Current();
        Node* check =
            graph()->NewNode(simplified()->ReferenceEqual(Type::Any()),
                             receiver_map, jsgraph()->Constant(map));
        if (--num_classes == 0 && num_transitions == 0 &&
            j == access_infos.size() - 1) {
          // Last map check on the fallthrough control path, do a conditional
          // eager deoptimization exit here.
          // TODO(turbofan): This is ugly as hell! We should probably introduce
          // macro-ish operators for property access that encapsulate this whole
          // mess.
          this_controls.push_back(graph()->NewNode(common()->DeoptimizeUnless(),
                                                   check, frame_state, effect,
                                                   fallthrough_control));
          fallthrough_control = nullptr;
        } else {
          Node* branch =
              graph()->NewNode(common()->Branch(), check, fallthrough_control);
          this_controls.push_back(graph()->NewNode(common()->IfTrue(), branch));
          fallthrough_control = graph()->NewNode(common()->IfFalse(), branch);
        }
        this_effects.push_back(effect);
        if (!map->IsJSArrayMap()) receiver_is_jsarray = false;
      }

      // Generate possible elements kind transitions.
      for (auto transition : access_info.transitions()) {
        DCHECK_LT(0u, num_transitions);
        Handle<Map> transition_source = transition.first;
        Handle<Map> transition_target = transition.second;
        Node* transition_control;
        Node* transition_effect = effect;

        // Check if {receiver} has the specified {transition_source} map.
        Node* check = graph()->NewNode(
            simplified()->ReferenceEqual(Type::Any()), receiver_map,
            jsgraph()->HeapConstant(transition_source));
        if (--num_transitions == 0 && j == access_infos.size() - 1) {
          transition_control =
              graph()->NewNode(common()->DeoptimizeUnless(), check, frame_state,
                               transition_effect, fallthrough_control);
          fallthrough_control = nullptr;
        } else {
          Node* branch =
              graph()->NewNode(common()->Branch(), check, fallthrough_control);
          fallthrough_control = graph()->NewNode(common()->IfFalse(), branch);
          transition_control = graph()->NewNode(common()->IfTrue(), branch);
        }

        // Migrate {receiver} from {transition_source} to {transition_target}.
        if (IsSimpleMapChangeTransition(transition_source->elements_kind(),
                                        transition_target->elements_kind())) {
          // In-place migration, just store the {transition_target} map.
          transition_effect = graph()->NewNode(
              simplified()->StoreField(AccessBuilder::ForMap()), receiver,
              jsgraph()->HeapConstant(transition_target), transition_effect,
              transition_control);
        } else {
          // Instance migration, let the stub deal with the {receiver}.
          TransitionElementsKindStub stub(isolate(),
                                          transition_source->elements_kind(),
                                          transition_target->elements_kind(),
                                          transition_source->IsJSArrayMap());
          CallDescriptor const* const desc = Linkage::GetStubCallDescriptor(
              isolate(), graph()->zone(), stub.GetCallInterfaceDescriptor(), 0,
              CallDescriptor::kNeedsFrameState, node->op()->properties());
          transition_effect = graph()->NewNode(
              common()->Call(desc), jsgraph()->HeapConstant(stub.GetCode()),
              receiver, jsgraph()->HeapConstant(transition_target), context,
              frame_state, transition_effect, transition_control);
        }
        this_controls.push_back(transition_control);
        this_effects.push_back(transition_effect);
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
      }
    }

    // Certain stores need a prototype chain check because shape changes
    // could allow callbacks on elements in the prototype chain that are
    // not compatible with (monomorphic) keyed stores.
    Handle<JSObject> holder;
    if (access_info.holder().ToHandle(&holder)) {
      AssumePrototypesStable(receiver_type, native_context, holder);
    }

    // Check that the {index} is actually a Number.
    if (!NumberMatcher(this_index).HasValue()) {
      Node* check =
          graph()->NewNode(simplified()->ObjectIsNumber(), this_index);
      this_control = graph()->NewNode(common()->DeoptimizeUnless(), check,
                                      frame_state, this_effect, this_control);
      this_index = graph()->NewNode(simplified()->TypeGuard(Type::Number()),
                                    this_index, this_control);
    }

    // Convert the {index} to an unsigned32 value and check if the result is
    // equal to the original {index}.
    if (!NumberMatcher(this_index).IsInRange(0.0, kMaxUInt32)) {
      Node* this_index32 =
          graph()->NewNode(simplified()->NumberToUint32(), this_index);
      Node* check = graph()->NewNode(simplified()->NumberEqual(), this_index32,
                                     this_index);
      this_control = graph()->NewNode(common()->DeoptimizeUnless(), check,
                                      frame_state, this_effect, this_control);
      this_index = this_index32;
    }

    // TODO(bmeurer): We currently specialize based on elements kind. We should
    // also be able to properly support strings and other JSObjects here.
    ElementsKind elements_kind = access_info.elements_kind();

    // Load the elements for the {receiver}.
    Node* this_elements = this_effect = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForJSObjectElements()),
        this_receiver, this_effect, this_control);

    // Don't try to store to a copy-on-write backing store.
    if (access_mode == AccessMode::kStore &&
        IsFastSmiOrObjectElementsKind(elements_kind)) {
      Node* this_elements_map = this_effect =
          graph()->NewNode(simplified()->LoadField(AccessBuilder::ForMap()),
                           this_elements, this_effect, this_control);
      Node* check = graph()->NewNode(
          simplified()->ReferenceEqual(Type::Any()), this_elements_map,
          jsgraph()->HeapConstant(factory()->fixed_array_map()));
      this_control = graph()->NewNode(common()->DeoptimizeUnless(), check,
                                      frame_state, this_effect, this_control);
    }

    // Load the length of the {receiver}.
    Node* this_length = this_effect =
        receiver_is_jsarray
            ? graph()->NewNode(
                  simplified()->LoadField(
                      AccessBuilder::ForJSArrayLength(elements_kind)),
                  this_receiver, this_effect, this_control)
            : graph()->NewNode(
                  simplified()->LoadField(AccessBuilder::ForFixedArrayLength()),
                  this_elements, this_effect, this_control);

    // Check that the {index} is in the valid range for the {receiver}.
    Node* check = graph()->NewNode(simplified()->NumberLessThan(), this_index,
                                   this_length);
    this_control = graph()->NewNode(common()->DeoptimizeUnless(), check,
                                    frame_state, this_effect, this_control);

    // Compute the element access.
    Type* element_type = Type::Any();
    MachineType element_machine_type = MachineType::AnyTagged();
    if (IsFastDoubleElementsKind(elements_kind)) {
      element_type = Type::Number();
      element_machine_type = MachineType::Float64();
    } else if (IsFastSmiElementsKind(elements_kind)) {
      element_type = type_cache_.kSmi;
    }
    ElementAccess element_access = {kTaggedBase, FixedArray::kHeaderSize,
                                    element_type, element_machine_type,
                                    kFullWriteBarrier};

    // Access the actual element.
    // TODO(bmeurer): Refactor this into separate methods or even a separate
    // class that deals with the elements access.
    if (access_mode == AccessMode::kLoad) {
      // Compute the real element access type, which includes the hole in case
      // of holey backing stores.
      if (elements_kind == FAST_HOLEY_ELEMENTS ||
          elements_kind == FAST_HOLEY_SMI_ELEMENTS) {
        element_access.type = Type::Union(
            element_type,
            Type::Constant(factory()->the_hole_value(), graph()->zone()),
            graph()->zone());
      }
      // Perform the actual backing store access.
      this_value = this_effect = graph()->NewNode(
          simplified()->LoadElement(element_access), this_elements, this_index,
          this_effect, this_control);
      // Handle loading from holey backing stores correctly, by either mapping
      // the hole to undefined if possible, or deoptimizing otherwise.
      if (elements_kind == FAST_HOLEY_ELEMENTS ||
          elements_kind == FAST_HOLEY_SMI_ELEMENTS) {
        // Perform the hole check on the result.
        Node* check =
            graph()->NewNode(simplified()->ReferenceEqual(element_access.type),
                             this_value, jsgraph()->TheHoleConstant());
        // Check if we are allowed to turn the hole into undefined.
        Type* initial_holey_array_type = Type::Class(
            handle(isolate()->get_initial_js_array_map(elements_kind)),
            graph()->zone());
        if (receiver_type->NowIs(initial_holey_array_type) &&
            isolate()->IsFastArrayConstructorPrototypeChainIntact()) {
          Node* branch = graph()->NewNode(common()->Branch(BranchHint::kFalse),
                                          check, this_control);
          Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
          Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
          // Add a code dependency on the array protector cell.
          AssumePrototypesStable(receiver_type, native_context,
                                 isolate()->initial_object_prototype());
          dependencies()->AssumePropertyCell(factory()->array_protector());
          // Turn the hole into undefined.
          this_control =
              graph()->NewNode(common()->Merge(2), if_true, if_false);
          this_value = graph()->NewNode(
              common()->Phi(MachineRepresentation::kTagged, 2),
              jsgraph()->UndefinedConstant(), this_value, this_control);
          element_type =
              Type::Union(element_type, Type::Undefined(), graph()->zone());
        } else {
          // Deoptimize in case of the hole.
          this_control =
              graph()->NewNode(common()->DeoptimizeIf(), check, frame_state,
                               this_effect, this_control);
        }
        // Rename the result to represent the actual type (not polluted by the
        // hole).
        this_value = graph()->NewNode(simplified()->TypeGuard(element_type),
                                      this_value, this_control);
      } else if (elements_kind == FAST_HOLEY_DOUBLE_ELEMENTS) {
        // Perform the hole check on the result.
        Node* check =
            graph()->NewNode(simplified()->NumberIsHoleNaN(), this_value);
        // Check if we are allowed to return the hole directly.
        Type* initial_holey_array_type = Type::Class(
            handle(isolate()->get_initial_js_array_map(elements_kind)),
            graph()->zone());
        if (receiver_type->NowIs(initial_holey_array_type) &&
            isolate()->IsFastArrayConstructorPrototypeChainIntact()) {
          // Add a code dependency on the array protector cell.
          AssumePrototypesStable(receiver_type, native_context,
                                 isolate()->initial_object_prototype());
          dependencies()->AssumePropertyCell(factory()->array_protector());
          // Turn the hole into undefined.
          this_value = graph()->NewNode(
              common()->Select(MachineRepresentation::kTagged,
                               BranchHint::kFalse),
              check, jsgraph()->UndefinedConstant(), this_value);
        } else {
          // Deoptimize in case of the hole.
          this_control =
              graph()->NewNode(common()->DeoptimizeIf(), check, frame_state,
                               this_effect, this_control);
        }
      }
    } else {
      DCHECK_EQ(AccessMode::kStore, access_mode);
      if (IsFastSmiElementsKind(elements_kind)) {
        Node* check = graph()->NewNode(simplified()->ObjectIsSmi(), this_value);
        this_control = graph()->NewNode(common()->DeoptimizeUnless(), check,
                                        frame_state, this_effect, this_control);
        this_value = graph()->NewNode(simplified()->TypeGuard(type_cache_.kSmi),
                                      this_value, this_control);
      } else if (IsFastDoubleElementsKind(elements_kind)) {
        Node* check =
            graph()->NewNode(simplified()->ObjectIsNumber(), this_value);
        this_control = graph()->NewNode(common()->DeoptimizeUnless(), check,
                                        frame_state, this_effect, this_control);
        this_value = graph()->NewNode(simplified()->TypeGuard(Type::Number()),
                                      this_value, this_control);
      }
      this_effect = graph()->NewNode(simplified()->StoreElement(element_access),
                                     this_elements, this_index, this_value,
                                     this_effect, this_control);
    }

    // Remember the final state for this element access.
    values.push_back(this_value);
    effects.push_back(this_effect);
    controls.push_back(this_control);
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
  ReplaceWithValue(node, value, effect, control);
  return Replace(value);
}


Reduction JSNativeContextSpecialization::ReduceKeyedAccess(
    Node* node, Node* index, Node* value, FeedbackNexus const& nexus,
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
      return ReduceSoftDeoptimize(node);
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
      return ReduceSoftDeoptimize(node);
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
  }

  // Try to lower the element access based on the {receiver_maps}.
  return ReduceElementAccess(node, index, value, receiver_maps, access_mode,
                             language_mode, store_mode);
}


Reduction JSNativeContextSpecialization::ReduceSoftDeoptimize(Node* node) {
  Node* frame_state = NodeProperties::GetFrameStateInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* deoptimize =
      graph()->NewNode(common()->Deoptimize(DeoptimizeKind::kSoft), frame_state,
                       effect, control);
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


void JSNativeContextSpecialization::AssumePrototypesStable(
    Type* receiver_type, Handle<Context> native_context,
    Handle<JSObject> holder) {
  // Determine actual holder and perform prototype chain checks.
  for (auto i = receiver_type->Classes(); !i.Done(); i.Advance()) {
    Handle<Map> map = i.Current();
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
  NodeMatcher m(receiver);
  if (m.IsJSCreate()) {
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
