// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-native-context-specialization.h"

#include "src/accessors.h"
#include "src/code-factory.h"
#include "src/compilation-dependencies.h"
#include "src/compiler/access-builder.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/js-operator.h"
#include "src/compiler/linkage.h"
#include "src/compiler/node-matchers.h"
#include "src/field-index-inl.h"
#include "src/objects-inl.h"  // TODO(mstarzinger): Temporary cycle breaker!
#include "src/type-cache.h"
#include "src/type-feedback-vector.h"

namespace v8 {
namespace internal {
namespace compiler {

JSNativeContextSpecialization::JSNativeContextSpecialization(
    Editor* editor, JSGraph* jsgraph, Flags flags,
    Handle<Context> native_context, CompilationDependencies* dependencies,
    Zone* zone)
    : AdvancedReducer(editor),
      jsgraph_(jsgraph),
      flags_(flags),
      native_context_(native_context),
      dependencies_(dependencies),
      zone_(zone),
      type_cache_(TypeCache::Get()),
      access_info_factory_(dependencies, native_context, graph()->zone()) {}


Reduction JSNativeContextSpecialization::Reduce(Node* node) {
  switch (node->opcode()) {
    case IrOpcode::kJSCallFunction:
      return ReduceJSCallFunction(node);
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


Reduction JSNativeContextSpecialization::ReduceJSCallFunction(Node* node) {
  DCHECK_EQ(IrOpcode::kJSCallFunction, node->opcode());
  CallFunctionParameters const& p = CallFunctionParametersOf(node->op());
  Node* target = NodeProperties::GetValueInput(node, 0);
  Node* frame_state = NodeProperties::GetFrameStateInput(node, 1);
  Node* control = NodeProperties::GetControlInput(node);
  Node* effect = NodeProperties::GetEffectInput(node);

  // Not much we can do if deoptimization support is disabled.
  if (!(flags() & kDeoptimizationEnabled)) return NoChange();

  // Don't mess with JSCallFunction nodes that have a constant {target}.
  if (HeapObjectMatcher(target).HasValue()) return NoChange();
  if (!p.feedback().IsValid()) return NoChange();
  CallICNexus nexus(p.feedback().vector(), p.feedback().slot());
  Handle<Object> feedback(nexus.GetFeedback(), isolate());
  if (feedback->IsWeakCell()) {
    Handle<WeakCell> cell = Handle<WeakCell>::cast(feedback);
    if (cell->value()->IsJSFunction()) {
      // Avoid cross-context leaks, meaning don't embed references to functions
      // in other native contexts.
      Handle<JSFunction> function(JSFunction::cast(cell->value()), isolate());
      if (function->context()->native_context() != *native_context()) {
        return NoChange();
      }

      // Check that the {target} is still the {target_function}.
      Node* target_function = jsgraph()->HeapConstant(function);
      Node* check = graph()->NewNode(simplified()->ReferenceEqual(Type::Any()),
                                     target, target_function);
      Node* branch =
          graph()->NewNode(common()->Branch(BranchHint::kTrue), check, control);
      Node* if_false = graph()->NewNode(common()->IfFalse(), branch);
      Node* deoptimize = graph()->NewNode(common()->Deoptimize(), frame_state,
                                          effect, if_false);
      // TODO(bmeurer): This should be on the AdvancedReducer somehow.
      NodeProperties::MergeControlToEnd(graph(), common(), deoptimize);
      control = graph()->NewNode(common()->IfTrue(), branch);

      // Specialize the JSCallFunction node to the {target_function}.
      NodeProperties::ReplaceValueInput(node, target_function, 0);
      NodeProperties::ReplaceControlInput(node, control);
      return Changed(node);
    }
    // TODO(bmeurer): Also support optimizing bound functions and proxies here.
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

  // Compute property access infos for the receiver maps.
  ZoneVector<PropertyAccessInfo> access_infos(zone());
  if (!access_info_factory().ComputePropertyAccessInfos(
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

  // The list of "exiting" controls, which currently go to a single deoptimize.
  // TODO(bmeurer): Consider using an IC as fallback.
  Node* const exit_effect = effect;
  ZoneVector<Node*> exit_controls(zone());

  // Ensure that {index} matches the specified {name} (if {index} is given).
  if (index != nullptr) {
    Node* check = graph()->NewNode(simplified()->ReferenceEqual(Type::Name()),
                                   index, jsgraph()->HeapConstant(name));
    Node* branch =
        graph()->NewNode(common()->Branch(BranchHint::kTrue), check, control);
    exit_controls.push_back(graph()->NewNode(common()->IfFalse(), branch));
    control = graph()->NewNode(common()->IfTrue(), branch);
  }

  // Ensure that {receiver} is a heap object.
  Node* check = graph()->NewNode(simplified()->ObjectIsSmi(), receiver);
  Node* branch = graph()->NewNode(common()->Branch(), check, control);
  control = graph()->NewNode(common()->IfFalse(), branch);
  Node* receiverissmi_control = graph()->NewNode(common()->IfTrue(), branch);
  Node* receiverissmi_effect = effect;

  // Load the {receiver} map. The resulting effect is the dominating effect for
  // all (polymorphic) branches.
  Node* receiver_map = effect =
      graph()->NewNode(simplified()->LoadField(AccessBuilder::ForMap()),
                       receiver, effect, control);

  // Generate code for the various different property access patterns.
  Node* fallthrough_control = control;
  for (PropertyAccessInfo const& access_info : access_infos) {
    Node* this_value = value;
    Node* this_receiver = receiver;
    Node* this_effect = effect;
    Node* this_control;

    // Perform map check on {receiver}.
    Type* receiver_type = access_info.receiver_type();
    if (receiver_type->Is(Type::String())) {
      // Emit an instance type check for strings.
      Node* receiver_instance_type = this_effect = graph()->NewNode(
          simplified()->LoadField(AccessBuilder::ForMapInstanceType()),
          receiver_map, this_effect, fallthrough_control);
      Node* check =
          graph()->NewNode(machine()->Uint32LessThan(), receiver_instance_type,
                           jsgraph()->Uint32Constant(FIRST_NONSTRING_TYPE));
      Node* branch =
          graph()->NewNode(common()->Branch(), check, fallthrough_control);
      fallthrough_control = graph()->NewNode(common()->IfFalse(), branch);
      this_control = graph()->NewNode(common()->IfTrue(), branch);
    } else {
      // Emit a (sequence of) map checks for other {receiver}s.
      ZoneVector<Node*> this_controls(zone());
      ZoneVector<Node*> this_effects(zone());
      for (auto i = access_info.receiver_type()->Classes(); !i.Done();
           i.Advance()) {
        Handle<Map> map = i.Current();
        Node* check =
            graph()->NewNode(simplified()->ReferenceEqual(Type::Internal()),
                             receiver_map, jsgraph()->Constant(map));
        Node* branch =
            graph()->NewNode(common()->Branch(), check, fallthrough_control);
        fallthrough_control = graph()->NewNode(common()->IfFalse(), branch);
        this_controls.push_back(graph()->NewNode(common()->IfTrue(), branch));
        this_effects.push_back(this_effect);
      }

      // The Number case requires special treatment to also deal with Smis.
      if (receiver_type->Is(Type::Number())) {
        // Join this check with the "receiver is smi" check above, and mark the
        // "receiver is smi" check as "consumed" so that we don't deoptimize if
        // the {receiver} is actually a Smi.
        if (receiverissmi_control != nullptr) {
          this_controls.push_back(receiverissmi_control);
          this_effects.push_back(receiverissmi_effect);
          receiverissmi_control = receiverissmi_effect = nullptr;
        }
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
      AssumePrototypesStable(receiver_type, holder);
    }

    // Generate the actual property access.
    if (access_info.IsNotFound()) {
      DCHECK_EQ(AccessMode::kLoad, access_mode);
      if (is_strong(language_mode)) {
        // TODO(bmeurer/mstarzinger): Add support for lowering inside try
        // blocks rewiring the IfException edge to a runtime call/throw.
        exit_controls.push_back(this_control);
        continue;
      } else {
        this_value = jsgraph()->UndefinedConstant();
      }
    } else if (access_info.IsDataConstant()) {
      this_value = jsgraph()->Constant(access_info.constant());
      if (access_mode == AccessMode::kStore) {
        Node* check = graph()->NewNode(
            simplified()->ReferenceEqual(Type::Tagged()), value, this_value);
        Node* branch = graph()->NewNode(common()->Branch(BranchHint::kTrue),
                                        check, this_control);
        exit_controls.push_back(graph()->NewNode(common()->IfFalse(), branch));
        this_control = graph()->NewNode(common()->IfTrue(), branch);
      }
    } else {
      DCHECK(access_info.IsDataField());
      FieldIndex const field_index = access_info.field_index();
      Type* const field_type = access_info.field_type();
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
      FieldAccess field_access = {kTaggedBase, field_index.offset(), name,
                                  field_type, kMachAnyTagged};
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
          field_access.machine_type = kMachFloat64;
        }
        this_value = this_effect =
            graph()->NewNode(simplified()->LoadField(field_access),
                             this_storage, this_effect, this_control);
      } else {
        DCHECK_EQ(AccessMode::kStore, access_mode);
        if (field_type->Is(Type::UntaggedFloat64())) {
          Node* check =
              graph()->NewNode(simplified()->ObjectIsNumber(), this_value);
          Node* branch = graph()->NewNode(common()->Branch(BranchHint::kTrue),
                                          check, this_control);
          exit_controls.push_back(
              graph()->NewNode(common()->IfFalse(), branch));
          this_control = graph()->NewNode(common()->IfTrue(), branch);
          this_value = graph()->NewNode(common()->Guard(Type::Number()),
                                        this_value, this_control);

          if (!field_index.is_inobject() || field_index.is_hidden_field() ||
              !FLAG_unbox_double_fields) {
            if (access_info.HasTransitionMap()) {
              // Allocate a MutableHeapNumber for the new property.
              Callable callable =
                  CodeFactory::AllocateMutableHeapNumber(isolate());
              CallDescriptor* desc = Linkage::GetStubCallDescriptor(
                  isolate(), jsgraph()->zone(), callable.descriptor(), 0,
                  CallDescriptor::kNoFlags, Operator::kNoThrow);
              Node* this_box = this_effect = graph()->NewNode(
                  common()->Call(desc),
                  jsgraph()->HeapConstant(callable.code()),
                  jsgraph()->NoContextConstant(), this_effect, this_control);
              this_effect = graph()->NewNode(
                  simplified()->StoreField(AccessBuilder::ForHeapNumberValue()),
                  this_box, this_value, this_effect, this_control);
              this_value = this_box;

              field_access.type = Type::TaggedPointer();
            } else {
              // We just store directly to the MutableHeapNumber.
              this_storage = this_effect =
                  graph()->NewNode(simplified()->LoadField(field_access),
                                   this_storage, this_effect, this_control);
              field_access.offset = HeapNumber::kValueOffset;
              field_access.name = MaybeHandle<Name>();
              field_access.machine_type = kMachFloat64;
            }
          } else {
            // Unboxed double field, we store directly to the field.
            field_access.machine_type = kMachFloat64;
          }
        } else if (field_type->Is(Type::TaggedSigned())) {
          Node* check =
              graph()->NewNode(simplified()->ObjectIsSmi(), this_value);
          Node* branch = graph()->NewNode(common()->Branch(BranchHint::kTrue),
                                          check, this_control);
          exit_controls.push_back(
              graph()->NewNode(common()->IfFalse(), branch));
          this_control = graph()->NewNode(common()->IfTrue(), branch);
        } else if (field_type->Is(Type::TaggedPointer())) {
          Node* check =
              graph()->NewNode(simplified()->ObjectIsSmi(), this_value);
          Node* branch = graph()->NewNode(common()->Branch(BranchHint::kFalse),
                                          check, this_control);
          exit_controls.push_back(graph()->NewNode(common()->IfTrue(), branch));
          this_control = graph()->NewNode(common()->IfFalse(), branch);
          if (field_type->NumClasses() > 0) {
            // Emit a (sequence of) map checks for the value.
            ZoneVector<Node*> this_controls(zone());
            Node* this_value_map = this_effect = graph()->NewNode(
                simplified()->LoadField(AccessBuilder::ForMap()), this_value,
                this_effect, this_control);
            for (auto i = field_type->Classes(); !i.Done(); i.Advance()) {
              Handle<Map> field_map(i.Current());
              check = graph()->NewNode(
                  simplified()->ReferenceEqual(Type::Internal()),
                  this_value_map, jsgraph()->Constant(field_map));
              branch = graph()->NewNode(common()->Branch(BranchHint::kTrue),
                                        check, this_control);
              this_control = graph()->NewNode(common()->IfFalse(), branch);
              this_controls.push_back(
                  graph()->NewNode(common()->IfTrue(), branch));
            }
            exit_controls.push_back(this_control);
            int const this_control_count =
                static_cast<int>(this_controls.size());
            this_control =
                (this_control_count == 1)
                    ? this_controls.front()
                    : graph()->NewNode(common()->Merge(this_control_count),
                                       this_control_count,
                                       &this_controls.front());
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

  // Collect the fallthrough control as final "exit" control.
  if (fallthrough_control != control) {
    // Mark the last fallthrough branch as deferred.
    MarkAsDeferred(fallthrough_control);
  }
  exit_controls.push_back(fallthrough_control);

  // Also collect the "receiver is smi" control if we didn't handle the case of
  // Number primitives in the polymorphic branches above.
  if (receiverissmi_control != nullptr) {
    // Mark the "receiver is smi" case as deferred.
    MarkAsDeferred(receiverissmi_control);
    DCHECK_EQ(exit_effect, receiverissmi_effect);
    exit_controls.push_back(receiverissmi_control);
  }

  // Generate the single "exit" point, where we get if either all map/instance
  // type checks failed, or one of the assumptions inside one of the cases
  // failes (i.e. failing prototype chain check).
  // TODO(bmeurer): Consider falling back to IC here if deoptimization is
  // disabled.
  int const exit_control_count = static_cast<int>(exit_controls.size());
  Node* exit_control =
      (exit_control_count == 1)
          ? exit_controls.front()
          : graph()->NewNode(common()->Merge(exit_control_count),
                             exit_control_count, &exit_controls.front());
  Node* deoptimize = graph()->NewNode(common()->Deoptimize(), frame_state,
                                      exit_effect, exit_control);
  // TODO(bmeurer): This should be on the AdvancedReducer somehow.
  NodeProperties::MergeControlToEnd(graph(), common(), deoptimize);

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
    value = graph()->NewNode(common()->Phi(kMachAnyTagged, control_count),
                             control_count + 1, &values.front());
    effects.push_back(control);
    effect = graph()->NewNode(common()->EffectPhi(control_count),
                              control_count + 1, &effects.front());
  }
  ReplaceWithValue(node, value, effect, control);
  return Replace(value);
}


Reduction JSNativeContextSpecialization::ReduceJSLoadNamed(Node* node) {
  DCHECK_EQ(IrOpcode::kJSLoadNamed, node->opcode());
  NamedAccess const& p = NamedAccessOf(node->op());
  Node* const value = jsgraph()->Dead();

  // Extract receiver maps from the LOAD_IC using the LoadICNexus.
  MapHandleList receiver_maps;
  if (!p.feedback().IsValid()) return NoChange();
  LoadICNexus nexus(p.feedback().vector(), p.feedback().slot());
  if (nexus.ExtractMaps(&receiver_maps) == 0) return NoChange();
  DCHECK_LT(0, receiver_maps.length());

  // Try to lower the named access based on the {receiver_maps}.
  return ReduceNamedAccess(node, value, receiver_maps, p.name(),
                           AccessMode::kLoad, p.language_mode());
}


Reduction JSNativeContextSpecialization::ReduceJSStoreNamed(Node* node) {
  DCHECK_EQ(IrOpcode::kJSStoreNamed, node->opcode());
  NamedAccess const& p = NamedAccessOf(node->op());
  Node* const value = NodeProperties::GetValueInput(node, 1);

  // Extract receiver maps from the STORE_IC using the StoreICNexus.
  MapHandleList receiver_maps;
  if (!p.feedback().IsValid()) return NoChange();
  StoreICNexus nexus(p.feedback().vector(), p.feedback().slot());
  if (nexus.ExtractMaps(&receiver_maps) == 0) return NoChange();
  DCHECK_LT(0, receiver_maps.length());

  // Try to lower the named access based on the {receiver_maps}.
  return ReduceNamedAccess(node, value, receiver_maps, p.name(),
                           AccessMode::kStore, p.language_mode());
}


Reduction JSNativeContextSpecialization::ReduceElementAccess(
    Node* node, Node* index, Node* value, MapHandleList const& receiver_maps,
    AccessMode access_mode, LanguageMode language_mode) {
  DCHECK(node->opcode() == IrOpcode::kJSLoadProperty ||
         node->opcode() == IrOpcode::kJSStoreProperty);
  Node* receiver = NodeProperties::GetValueInput(node, 0);
  Node* frame_state = NodeProperties::GetFrameStateInput(node, 1);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // Not much we can do if deoptimization support is disabled.
  if (!(flags() & kDeoptimizationEnabled)) return NoChange();

  // Compute element access infos for the receiver maps.
  ZoneVector<ElementAccessInfo> access_infos(zone());
  if (!access_info_factory().ComputeElementAccessInfos(
          receiver_maps, access_mode, &access_infos)) {
    return NoChange();
  }

  // Nothing to do if we have no non-deprecated maps.
  if (access_infos.empty()) return NoChange();

  // The final states for every polymorphic branch. We join them with
  // Merge+Phi+EffectPhi at the bottom.
  ZoneVector<Node*> values(zone());
  ZoneVector<Node*> effects(zone());
  ZoneVector<Node*> controls(zone());

  // The list of "exiting" controls, which currently go to a single deoptimize.
  // TODO(bmeurer): Consider using an IC as fallback.
  Node* const exit_effect = effect;
  ZoneVector<Node*> exit_controls(zone());

  // Ensure that {receiver} is a heap object.
  Node* check = graph()->NewNode(simplified()->ObjectIsSmi(), receiver);
  Node* branch =
      graph()->NewNode(common()->Branch(BranchHint::kFalse), check, control);
  exit_controls.push_back(graph()->NewNode(common()->IfTrue(), branch));
  control = graph()->NewNode(common()->IfFalse(), branch);

  // Load the {receiver} map. The resulting effect is the dominating effect for
  // all (polymorphic) branches.
  Node* receiver_map = effect =
      graph()->NewNode(simplified()->LoadField(AccessBuilder::ForMap()),
                       receiver, effect, control);

  // Generate code for the various different element access patterns.
  Node* fallthrough_control = control;
  for (ElementAccessInfo const& access_info : access_infos) {
    Node* this_receiver = receiver;
    Node* this_value = value;
    Node* this_index = index;
    Node* this_effect = effect;
    Node* this_control;

    // Perform map check on {receiver}.
    Type* receiver_type = access_info.receiver_type();
    {
      ZoneVector<Node*> this_controls(zone());
      for (auto i = access_info.receiver_type()->Classes(); !i.Done();
           i.Advance()) {
        Handle<Map> map = i.Current();
        Node* check =
            graph()->NewNode(simplified()->ReferenceEqual(Type::Internal()),
                             receiver_map, jsgraph()->Constant(map));
        Node* branch =
            graph()->NewNode(common()->Branch(), check, fallthrough_control);
        this_controls.push_back(graph()->NewNode(common()->IfTrue(), branch));
        fallthrough_control = graph()->NewNode(common()->IfFalse(), branch);
      }
      int const this_control_count = static_cast<int>(this_controls.size());
      this_control =
          (this_control_count == 1)
              ? this_controls.front()
              : graph()->NewNode(common()->Merge(this_control_count),
                                 this_control_count, &this_controls.front());
    }

    // Certain stores need a prototype chain check because shape changes
    // could allow callbacks on elements in the prototype chain that are
    // not compatible with (monomorphic) keyed stores.
    Handle<JSObject> holder;
    if (access_info.holder().ToHandle(&holder)) {
      AssumePrototypesStable(receiver_type, holder);
    }

    // Check that the {index} is actually a Number.
    if (!NumberMatcher(this_index).HasValue()) {
      Node* check =
          graph()->NewNode(simplified()->ObjectIsNumber(), this_index);
      Node* branch = graph()->NewNode(common()->Branch(BranchHint::kTrue),
                                      check, this_control);
      exit_controls.push_back(graph()->NewNode(common()->IfFalse(), branch));
      this_control = graph()->NewNode(common()->IfTrue(), branch);
      this_index = graph()->NewNode(common()->Guard(Type::Number()), this_index,
                                    this_control);
    }

    // Convert the {index} to an unsigned32 value and check if the result is
    // equal to the original {index}.
    if (!NumberMatcher(this_index).IsInRange(0.0, kMaxUInt32)) {
      Node* this_index32 =
          graph()->NewNode(simplified()->NumberToUint32(), this_index);
      Node* check = graph()->NewNode(simplified()->NumberEqual(), this_index32,
                                     this_index);
      Node* branch = graph()->NewNode(common()->Branch(BranchHint::kTrue),
                                      check, this_control);
      exit_controls.push_back(graph()->NewNode(common()->IfFalse(), branch));
      this_control = graph()->NewNode(common()->IfTrue(), branch);
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
      check = graph()->NewNode(
          simplified()->ReferenceEqual(Type::Any()), this_elements_map,
          jsgraph()->HeapConstant(factory()->fixed_array_map()));
      branch = graph()->NewNode(common()->Branch(BranchHint::kTrue), check,
                                this_control);
      exit_controls.push_back(graph()->NewNode(common()->IfFalse(), branch));
      this_control = graph()->NewNode(common()->IfTrue(), branch);
    }

    // Load the length of the {receiver}.
    FieldAccess length_access = {
        kTaggedBase, JSArray::kLengthOffset, factory()->name_string(),
        type_cache_.kJSArrayLengthType, kMachAnyTagged};
    if (IsFastDoubleElementsKind(elements_kind)) {
      length_access.type = type_cache_.kFixedDoubleArrayLengthType;
    } else if (IsFastElementsKind(elements_kind)) {
      length_access.type = type_cache_.kFixedArrayLengthType;
    }
    Node* this_length = this_effect =
        graph()->NewNode(simplified()->LoadField(length_access), this_receiver,
                         this_effect, this_control);

    // Check that the {index} is in the valid range for the {receiver}.
    Node* check = graph()->NewNode(simplified()->NumberLessThan(), this_index,
                                   this_length);
    Node* branch = graph()->NewNode(common()->Branch(BranchHint::kTrue), check,
                                    this_control);
    exit_controls.push_back(graph()->NewNode(common()->IfFalse(), branch));
    this_control = graph()->NewNode(common()->IfTrue(), branch);

    // Compute the element access.
    Type* element_type = Type::Any();
    MachineType element_machine_type = kMachAnyTagged;
    if (IsFastDoubleElementsKind(elements_kind)) {
      element_type = type_cache_.kFloat64;
      element_machine_type = kMachFloat64;
    } else if (IsFastSmiElementsKind(elements_kind)) {
      element_type = type_cache_.kSmi;
    }
    ElementAccess element_access = {kTaggedBase, FixedArray::kHeaderSize,
                                    element_type, element_machine_type};

    // Access the actual element.
    if (access_mode == AccessMode::kLoad) {
      this_value = this_effect = graph()->NewNode(
          simplified()->LoadElement(element_access), this_elements, this_index,
          this_effect, this_control);
    } else {
      DCHECK_EQ(AccessMode::kStore, access_mode);
      if (IsFastSmiElementsKind(elements_kind)) {
        Node* check = graph()->NewNode(simplified()->ObjectIsSmi(), this_value);
        Node* branch = graph()->NewNode(common()->Branch(BranchHint::kTrue),
                                        check, this_control);
        exit_controls.push_back(graph()->NewNode(common()->IfFalse(), branch));
        this_control = graph()->NewNode(common()->IfTrue(), branch);
      } else if (IsFastDoubleElementsKind(elements_kind)) {
        Node* check =
            graph()->NewNode(simplified()->ObjectIsNumber(), this_value);
        Node* branch = graph()->NewNode(common()->Branch(BranchHint::kTrue),
                                        check, this_control);
        exit_controls.push_back(graph()->NewNode(common()->IfFalse(), branch));
        this_control = graph()->NewNode(common()->IfTrue(), branch);
        this_value = graph()->NewNode(common()->Guard(Type::Number()),
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

  // Collect the fallthrough control as final "exit" control.
  if (fallthrough_control != control) {
    // Mark the last fallthrough branch as deferred.
    MarkAsDeferred(fallthrough_control);
  }
  exit_controls.push_back(fallthrough_control);

  // Generate the single "exit" point, where we get if either all map/instance
  // type checks failed, or one of the assumptions inside one of the cases
  // failes (i.e. failing prototype chain check).
  // TODO(bmeurer): Consider falling back to IC here if deoptimization is
  // disabled.
  int const exit_control_count = static_cast<int>(exit_controls.size());
  Node* exit_control =
      (exit_control_count == 1)
          ? exit_controls.front()
          : graph()->NewNode(common()->Merge(exit_control_count),
                             exit_control_count, &exit_controls.front());
  Node* deoptimize = graph()->NewNode(common()->Deoptimize(), frame_state,
                                      exit_effect, exit_control);
  // TODO(bmeurer): This should be on the AdvancedReducer somehow.
  NodeProperties::MergeControlToEnd(graph(), common(), deoptimize);

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
    value = graph()->NewNode(common()->Phi(kMachAnyTagged, control_count),
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
    AccessMode access_mode, LanguageMode language_mode) {
  DCHECK(node->opcode() == IrOpcode::kJSLoadProperty ||
         node->opcode() == IrOpcode::kJSStoreProperty);

  // Extract receiver maps from the {nexus}.
  MapHandleList receiver_maps;
  if (nexus.ExtractMaps(&receiver_maps) == 0) return NoChange();
  DCHECK_LT(0, receiver_maps.length());

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
                             language_mode);
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
                           p.language_mode());
}


Reduction JSNativeContextSpecialization::ReduceJSStoreProperty(Node* node) {
  DCHECK_EQ(IrOpcode::kJSStoreProperty, node->opcode());
  PropertyAccess const& p = PropertyAccessOf(node->op());
  Node* const index = NodeProperties::GetValueInput(node, 1);
  Node* const value = NodeProperties::GetValueInput(node, 2);

  // Extract receiver maps from the KEYED_STORE_IC using the KeyedStoreICNexus.
  if (!p.feedback().IsValid()) return NoChange();
  KeyedStoreICNexus nexus(p.feedback().vector(), p.feedback().slot());

  // Try to lower the keyed access based on the {nexus}.
  return ReduceKeyedAccess(node, index, value, nexus, AccessMode::kStore,
                           p.language_mode());
}


void JSNativeContextSpecialization::AssumePrototypesStable(
    Type* receiver_type, Handle<JSObject> holder) {
  // Determine actual holder and perform prototype chain checks.
  for (auto i = receiver_type->Classes(); !i.Done(); i.Advance()) {
    Handle<Map> map = i.Current();
    // Perform the implicit ToObject for primitives here.
    // Implemented according to ES6 section 7.3.2 GetV (V, P).
    Handle<JSFunction> constructor;
    if (Map::GetConstructorFunction(map, native_context())
            .ToHandle(&constructor)) {
      map = handle(constructor->initial_map(), isolate());
    }
    for (PrototypeIterator j(map); !j.IsAtEnd(); j.Advance()) {
      // Check that the {prototype} still has the same map.  All prototype
      // maps are guaranteed to be stable, so it's sufficient to add a
      // stability dependency here.
      Handle<JSReceiver> const prototype =
          PrototypeIterator::GetCurrent<JSReceiver>(j);
      dependencies()->AssumeMapStable(handle(prototype->map(), isolate()));
      // Stop once we get to the holder.
      if (prototype.is_identical_to(holder)) break;
    }
  }
}


void JSNativeContextSpecialization::MarkAsDeferred(Node* if_projection) {
  Node* branch = NodeProperties::GetControlInput(if_projection);
  DCHECK_EQ(IrOpcode::kBranch, branch->opcode());
  if (if_projection->opcode() == IrOpcode::kIfTrue) {
    NodeProperties::ChangeOp(branch, common()->Branch(BranchHint::kFalse));
  } else {
    DCHECK_EQ(IrOpcode::kIfFalse, if_projection->opcode());
    NodeProperties::ChangeOp(branch, common()->Branch(BranchHint::kTrue));
  }
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
