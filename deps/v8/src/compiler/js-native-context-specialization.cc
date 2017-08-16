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
#include "src/feedback-vector.h"
#include "src/field-index-inl.h"
#include "src/isolate-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {

bool HasNumberMaps(MapHandles const& maps) {
  for (auto map : maps) {
    if (map->instance_type() == HEAP_NUMBER_TYPE) return true;
  }
  return false;
}

bool HasOnlyJSArrayMaps(MapHandles const& maps) {
  for (auto map : maps) {
    if (!map->IsJSArrayMap()) return false;
  }
  return true;
}

bool HasOnlyNumberMaps(MapHandles const& maps) {
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

struct JSNativeContextSpecialization::ScriptContextTableLookupResult {
  Handle<Context> context;
  bool immutable;
  int index;
};

JSNativeContextSpecialization::JSNativeContextSpecialization(
    Editor* editor, JSGraph* jsgraph, Flags flags,
    Handle<Context> native_context, CompilationDependencies* dependencies,
    Zone* zone)
    : AdvancedReducer(editor),
      jsgraph_(jsgraph),
      flags_(flags),
      global_object_(native_context->global_object()),
      global_proxy_(JSGlobalProxy::cast(native_context->global_proxy())),
      native_context_(native_context),
      dependencies_(dependencies),
      zone_(zone),
      type_cache_(TypeCache::Get()) {}

Reduction JSNativeContextSpecialization::Reduce(Node* node) {
  switch (node->opcode()) {
    case IrOpcode::kJSAdd:
      return ReduceJSAdd(node);
    case IrOpcode::kJSGetSuperConstructor:
      return ReduceJSGetSuperConstructor(node);
    case IrOpcode::kJSInstanceOf:
      return ReduceJSInstanceOf(node);
    case IrOpcode::kJSOrdinaryHasInstance:
      return ReduceJSOrdinaryHasInstance(node);
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
    case IrOpcode::kJSLoadProperty:
      return ReduceJSLoadProperty(node);
    case IrOpcode::kJSStoreProperty:
      return ReduceJSStoreProperty(node);
    case IrOpcode::kJSStoreNamedOwn:
      return ReduceJSStoreNamedOwn(node);
    case IrOpcode::kJSStoreDataPropertyInLiteral:
      return ReduceJSStoreDataPropertyInLiteral(node);
    default:
      break;
  }
  return NoChange();
}

Reduction JSNativeContextSpecialization::ReduceJSAdd(Node* node) {
  // TODO(turbofan): This has to run together with the inlining and
  // native context specialization to be able to leverage the string
  // constant-folding for optimizing property access, but we should
  // nevertheless find a better home for this at some point.
  DCHECK_EQ(IrOpcode::kJSAdd, node->opcode());

  // Constant-fold string concatenation.
  HeapObjectBinopMatcher m(node);
  if (m.left().HasValue() && m.left().Value()->IsString() &&
      m.right().HasValue() && m.right().Value()->IsString()) {
    Handle<String> left = Handle<String>::cast(m.left().Value());
    Handle<String> right = Handle<String>::cast(m.right().Value());
    if (left->length() + right->length() <= String::kMaxLength) {
      Handle<String> result =
          factory()->NewConsString(left, right).ToHandleChecked();
      Node* value = jsgraph()->HeapConstant(result);
      ReplaceWithValue(node, value);
      return Replace(value);
    }
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
  Handle<JSFunction> function = Handle<JSFunction>::cast(m.Value());
  Handle<Map> function_map(function->map(), isolate());
  Handle<Object> function_prototype(function_map->prototype(), isolate());

  // We can constant-fold the super constructor access if the
  // {function}s map is stable, i.e. we can use a code dependency
  // to guard against [[Prototype]] changes of {function}.
  if (function_map->is_stable()) {
    Node* value = jsgraph()->Constant(function_prototype);
    dependencies()->AssumeMapStable(function_map);
    if (function_prototype->IsConstructor()) {
      ReplaceWithValue(node, value);
      return Replace(value);
    }
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

  // Check if the right hand side is a known {receiver}.
  HeapObjectMatcher m(constructor);
  if (!m.HasValue() || !m.Value()->IsJSObject()) return NoChange();
  Handle<JSObject> receiver = Handle<JSObject>::cast(m.Value());
  Handle<Map> receiver_map(receiver->map(), isolate());

  // Compute property access info for @@hasInstance on {receiver}.
  PropertyAccessInfo access_info;
  AccessInfoFactory access_info_factory(dependencies(), native_context(),
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
        AssumePrototypesStable(access_info.receiver_maps(), holder);
      }

      // Monomorphic property access.
      effect = BuildCheckMaps(constructor, effect, control,
                              access_info.receiver_maps());

      // Lower to OrdinaryHasInstance(C, O).
      NodeProperties::ReplaceValueInput(node, constructor, 0);
      NodeProperties::ReplaceValueInput(node, object, 1);
      NodeProperties::ReplaceEffectInput(node, effect);
      NodeProperties::ChangeOp(node, javascript()->OrdinaryHasInstance());
      Reduction const reduction = ReduceJSOrdinaryHasInstance(node);
      return reduction.Changed() ? reduction : Changed(node);
    }
  } else if (access_info.IsDataConstant() ||
             access_info.IsDataConstantField()) {
    // Determine actual holder and perform prototype chain checks.
    Handle<JSObject> holder;
    if (access_info.holder().ToHandle(&holder)) {
      AssumePrototypesStable(access_info.receiver_maps(), holder);
    } else {
      holder = receiver;
    }

    Handle<Object> constant;
    if (access_info.IsDataConstant()) {
      DCHECK(!FLAG_track_constant_fields);
      constant = access_info.constant();
    } else {
      DCHECK(FLAG_track_constant_fields);
      DCHECK(access_info.IsDataConstantField());
      // The value must be callable therefore tagged.
      DCHECK(CanBeTaggedPointer(access_info.field_representation()));
      FieldIndex field_index = access_info.field_index();
      constant = JSObject::FastPropertyAt(holder, Representation::Tagged(),
                                          field_index);
    }
    DCHECK(constant->IsCallable());

    // Monomorphic property access.
    effect = BuildCheckMaps(constructor, effect, control,
                            access_info.receiver_maps());

    // Call the @@hasInstance handler.
    Node* target = jsgraph()->Constant(constant);
    node->InsertInput(graph()->zone(), 0, target);
    node->ReplaceInput(1, constructor);
    node->ReplaceInput(2, object);
    node->ReplaceInput(5, effect);
    NodeProperties::ChangeOp(
        node, javascript()->Call(3, CallFrequency(), VectorSlotPair(),
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

Reduction JSNativeContextSpecialization::ReduceJSOrdinaryHasInstance(
    Node* node) {
  DCHECK_EQ(IrOpcode::kJSOrdinaryHasInstance, node->opcode());
  Node* constructor = NodeProperties::GetValueInput(node, 0);
  Node* object = NodeProperties::GetValueInput(node, 1);
  Node* context = NodeProperties::GetContextInput(node);
  Node* frame_state = NodeProperties::GetFrameStateInput(node);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // Check if the {constructor} is known at compile time.
  HeapObjectMatcher m(constructor);
  if (!m.HasValue()) return NoChange();

  // Check if the {constructor} is a JSBoundFunction.
  if (m.Value()->IsJSBoundFunction()) {
    // OrdinaryHasInstance on bound functions turns into a recursive
    // invocation of the instanceof operator again.
    // ES6 section 7.3.19 OrdinaryHasInstance (C, O) step 2.
    Handle<JSBoundFunction> function = Handle<JSBoundFunction>::cast(m.Value());
    Handle<JSReceiver> bound_target_function(function->bound_target_function());
    NodeProperties::ReplaceValueInput(node, object, 0);
    NodeProperties::ReplaceValueInput(
        node, jsgraph()->HeapConstant(bound_target_function), 1);
    NodeProperties::ChangeOp(node, javascript()->InstanceOf());
    Reduction const reduction = ReduceJSInstanceOf(node);
    return reduction.Changed() ? reduction : Changed(node);
  }

  // Check if the {constructor} is a JSFunction.
  if (m.Value()->IsJSFunction()) {
    // Check if the {function} is a constructor and has an instance "prototype".
    Handle<JSFunction> function = Handle<JSFunction>::cast(m.Value());
    if (function->IsConstructor() && function->has_instance_prototype() &&
        function->prototype()->IsJSReceiver()) {
      // Ensure that the {function} has a valid initial map, so we can
      // depend on that for the prototype constant-folding below.
      JSFunction::EnsureHasInitialMap(function);

      // Install a code dependency on the {function}s initial map.
      Handle<Map> initial_map(function->initial_map(), isolate());
      dependencies()->AssumeInitialMapCantChange(initial_map);
      Handle<JSReceiver> function_prototype =
          handle(JSReceiver::cast(initial_map->prototype()), isolate());

      // Check if we can constant-fold the prototype chain walk
      // for the given {object} and the {function_prototype}.
      InferHasInPrototypeChainResult result =
          InferHasInPrototypeChain(object, effect, function_prototype);
      if (result != kMayBeInPrototypeChain) {
        Node* value = jsgraph()->BooleanConstant(result == kIsInPrototypeChain);
        ReplaceWithValue(node, value, effect, control);
        return Replace(value);
      }

      Node* prototype = jsgraph()->Constant(function_prototype);

      Node* check0 = graph()->NewNode(simplified()->ObjectIsSmi(), object);
      Node* branch0 = graph()->NewNode(common()->Branch(BranchHint::kFalse),
                                       check0, control);

      Node* if_true0 = graph()->NewNode(common()->IfTrue(), branch0);
      Node* etrue0 = effect;
      Node* vtrue0 = jsgraph()->FalseConstant();

      control = graph()->NewNode(common()->IfFalse(), branch0);

      // Loop through the {object}s prototype chain looking for the {prototype}.
      Node* loop = control =
          graph()->NewNode(common()->Loop(2), control, control);
      Node* eloop = effect =
          graph()->NewNode(common()->EffectPhi(2), effect, effect, loop);
      Node* vloop = object =
          graph()->NewNode(common()->Phi(MachineRepresentation::kTagged, 2),
                           object, object, loop);

      // Load the {object} map and instance type.
      Node* object_map = effect =
          graph()->NewNode(simplified()->LoadField(AccessBuilder::ForMap()),
                           object, effect, control);
      Node* object_instance_type = effect = graph()->NewNode(
          simplified()->LoadField(AccessBuilder::ForMapInstanceType()),
          object_map, effect, control);

      // Check if the {object} is a special receiver, because for special
      // receivers, i.e. proxies or API objects that need access checks,
      // we have to use the %HasInPrototypeChain runtime function instead.
      Node* check1 = graph()->NewNode(
          simplified()->NumberLessThanOrEqual(), object_instance_type,
          jsgraph()->Constant(LAST_SPECIAL_RECEIVER_TYPE));
      Node* branch1 = graph()->NewNode(common()->Branch(BranchHint::kFalse),
                                       check1, control);

      control = graph()->NewNode(common()->IfFalse(), branch1);

      Node* if_true1 = graph()->NewNode(common()->IfTrue(), branch1);
      Node* etrue1 = effect;
      Node* vtrue1;

      // Check if the {object} is not a receiver at all.
      Node* check10 =
          graph()->NewNode(simplified()->NumberLessThan(), object_instance_type,
                           jsgraph()->Constant(FIRST_JS_RECEIVER_TYPE));
      Node* branch10 = graph()->NewNode(common()->Branch(BranchHint::kTrue),
                                        check10, if_true1);

      // A primitive value cannot match the {prototype} we're looking for.
      if_true1 = graph()->NewNode(common()->IfTrue(), branch10);
      vtrue1 = jsgraph()->FalseConstant();

      Node* if_false1 = graph()->NewNode(common()->IfFalse(), branch10);
      Node* efalse1 = etrue1;
      Node* vfalse1;
      {
        // Slow path, need to call the %HasInPrototypeChain runtime function.
        vfalse1 = efalse1 = if_false1 = graph()->NewNode(
            javascript()->CallRuntime(Runtime::kHasInPrototypeChain), object,
            prototype, context, frame_state, efalse1, if_false1);

        // Replace any potential {IfException} uses of {node} to catch
        // exceptions from this %HasInPrototypeChain runtime call instead.
        Node* on_exception = nullptr;
        if (NodeProperties::IsExceptionalCall(node, &on_exception)) {
          NodeProperties::ReplaceControlInput(on_exception, vfalse1);
          NodeProperties::ReplaceEffectInput(on_exception, efalse1);
          if_false1 = graph()->NewNode(common()->IfSuccess(), vfalse1);
          Revisit(on_exception);
        }
      }

      // Load the {object} prototype.
      Node* object_prototype = effect = graph()->NewNode(
          simplified()->LoadField(AccessBuilder::ForMapPrototype()), object_map,
          effect, control);

      // Check if we reached the end of {object}s prototype chain.
      Node* check2 =
          graph()->NewNode(simplified()->ReferenceEqual(), object_prototype,
                           jsgraph()->NullConstant());
      Node* branch2 = graph()->NewNode(common()->Branch(), check2, control);

      Node* if_true2 = graph()->NewNode(common()->IfTrue(), branch2);
      Node* etrue2 = effect;
      Node* vtrue2 = jsgraph()->FalseConstant();

      control = graph()->NewNode(common()->IfFalse(), branch2);

      // Check if we reached the {prototype}.
      Node* check3 = graph()->NewNode(simplified()->ReferenceEqual(),
                                      object_prototype, prototype);
      Node* branch3 = graph()->NewNode(common()->Branch(), check3, control);

      Node* if_true3 = graph()->NewNode(common()->IfTrue(), branch3);
      Node* etrue3 = effect;
      Node* vtrue3 = jsgraph()->TrueConstant();

      control = graph()->NewNode(common()->IfFalse(), branch3);

      // Close the loop.
      vloop->ReplaceInput(1, object_prototype);
      eloop->ReplaceInput(1, effect);
      loop->ReplaceInput(1, control);

      control = graph()->NewNode(common()->Merge(5), if_true0, if_true1,
                                 if_true2, if_true3, if_false1);
      effect = graph()->NewNode(common()->EffectPhi(5), etrue0, etrue1, etrue2,
                                etrue3, efalse1, control);

      // Morph the {node} into an appropriate Phi.
      ReplaceWithValue(node, node, effect, control);
      node->ReplaceInput(0, vtrue0);
      node->ReplaceInput(1, vtrue1);
      node->ReplaceInput(2, vtrue2);
      node->ReplaceInput(3, vtrue3);
      node->ReplaceInput(4, vfalse1);
      node->ReplaceInput(5, control);
      node->TrimInputCount(6);
      NodeProperties::ChangeOp(
          node, common()->Phi(MachineRepresentation::kTagged, 5));
      return Changed(node);
    }
  }

  return NoChange();
}

Reduction JSNativeContextSpecialization::ReduceJSLoadContext(Node* node) {
  DCHECK_EQ(IrOpcode::kJSLoadContext, node->opcode());
  ContextAccess const& access = ContextAccessOf(node->op());
  // Specialize JSLoadContext(NATIVE_CONTEXT_INDEX) to the known native
  // context (if any), so we can constant-fold those fields, which is
  // safe, since the NATIVE_CONTEXT_INDEX slot is always immutable.
  if (access.index() == Context::NATIVE_CONTEXT_INDEX) {
    Node* value = jsgraph()->HeapConstant(native_context());
    ReplaceWithValue(node, value);
    return Replace(value);
  }
  return NoChange();
}

namespace {

FieldAccess ForPropertyCellValue(MachineRepresentation representation,
                                 Type* type, MaybeHandle<Map> map,
                                 Handle<Name> name) {
  WriteBarrierKind kind = kFullWriteBarrier;
  if (representation == MachineRepresentation::kTaggedSigned) {
    kind = kNoWriteBarrier;
  } else if (representation == MachineRepresentation::kTaggedPointer) {
    kind = kPointerWriteBarrier;
  }
  MachineType r = MachineType::TypeForRepresentation(representation);
  FieldAccess access = {
      kTaggedBase, PropertyCell::kValueOffset, name, map, type, r, kind};
  return access;
}

}  // namespace

Reduction JSNativeContextSpecialization::ReduceGlobalAccess(
    Node* node, Node* receiver, Node* value, Handle<Name> name,
    AccessMode access_mode, Node* index) {
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // Lookup on the global object. We only deal with own data properties
  // of the global object here (represented as PropertyCell).
  LookupIterator it(global_object(), name, LookupIterator::OWN);
  it.TryLookupCachedProperty();
  if (it.state() != LookupIterator::DATA) return NoChange();
  if (!it.GetHolder<JSObject>()->IsJSGlobalObject()) return NoChange();
  Handle<PropertyCell> property_cell = it.GetPropertyCell();
  PropertyDetails property_details = property_cell->property_details();
  Handle<Object> property_cell_value(property_cell->value(), isolate());
  PropertyCellType property_cell_type = property_details.cell_type();

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
      if (property_cell_value->IsHeapObject() &&
          !Handle<HeapObject>::cast(property_cell_value)->map()->is_stable()) {
        return NoChange();
      }
    }
  }

  // Ensure that {index} matches the specified {name} (if {index} is given).
  if (index != nullptr) {
    Node* check = graph()->NewNode(simplified()->ReferenceEqual(), index,
                                   jsgraph()->HeapConstant(name));
    effect = graph()->NewNode(simplified()->CheckIf(), check, effect, control);
  }

  // Check if we have a {receiver} to validate. If so, we need to check that
  // the {receiver} is actually the JSGlobalProxy for the native context that
  // we are specializing to.
  if (receiver != nullptr) {
    Node* check = graph()->NewNode(simplified()->ReferenceEqual(), receiver,
                                   jsgraph()->HeapConstant(global_proxy()));
    effect = graph()->NewNode(simplified()->CheckIf(), check, effect, control);
  }

  if (access_mode == AccessMode::kLoad) {
    // Load from non-configurable, read-only data property on the global
    // object can be constant-folded, even without deoptimization support.
    if (!property_details.IsConfigurable() && property_details.IsReadOnly()) {
      value = jsgraph()->Constant(property_cell_value);
    } else {
      // Record a code dependency on the cell if we can benefit from the
      // additional feedback, or the global property is configurable (i.e.
      // can be deleted or reconfigured to an accessor property).
      if (property_details.cell_type() != PropertyCellType::kMutable ||
          property_details.IsConfigurable()) {
        dependencies()->AssumePropertyCell(property_cell);
      }

      // Load from constant/undefined global property can be constant-folded.
      if (property_details.cell_type() == PropertyCellType::kConstant ||
          property_details.cell_type() == PropertyCellType::kUndefined) {
        value = jsgraph()->Constant(property_cell_value);
      } else {
        // Load from constant type cell can benefit from type feedback.
        MaybeHandle<Map> map;
        Type* property_cell_value_type = Type::NonInternal();
        MachineRepresentation representation = MachineRepresentation::kTagged;
        if (property_details.cell_type() == PropertyCellType::kConstantType) {
          // Compute proper type based on the current value in the cell.
          if (property_cell_value->IsSmi()) {
            property_cell_value_type = Type::SignedSmall();
            representation = MachineRepresentation::kTaggedSigned;
          } else if (property_cell_value->IsNumber()) {
            property_cell_value_type = Type::Number();
            representation = MachineRepresentation::kTaggedPointer;
          } else {
            Handle<Map> property_cell_value_map(
                Handle<HeapObject>::cast(property_cell_value)->map(),
                isolate());
            property_cell_value_type = Type::For(property_cell_value_map);
            representation = MachineRepresentation::kTaggedPointer;

            // We can only use the property cell value map for map check
            // elimination if it's stable, i.e. the HeapObject wasn't
            // mutated without the cell state being updated.
            if (property_cell_value_map->is_stable()) {
              dependencies()->AssumeMapStable(property_cell_value_map);
              map = property_cell_value_map;
            }
          }
        }
        value = effect = graph()->NewNode(
            simplified()->LoadField(ForPropertyCellValue(
                representation, property_cell_value_type, map, name)),
            jsgraph()->HeapConstant(property_cell), effect, control);
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
        dependencies()->AssumePropertyCell(property_cell);
        Node* check =
            graph()->NewNode(simplified()->ReferenceEqual(), value,
                             jsgraph()->Constant(property_cell_value));
        effect =
            graph()->NewNode(simplified()->CheckIf(), check, effect, control);
        break;
      }
      case PropertyCellType::kConstantType: {
        // Record a code dependency on the cell, and just deoptimize if the new
        // values' type doesn't match the type of the previous value in the
        // cell.
        dependencies()->AssumePropertyCell(property_cell);
        Type* property_cell_value_type;
        MachineRepresentation representation = MachineRepresentation::kTagged;
        if (property_cell_value->IsHeapObject()) {
          // We cannot do anything if the {property_cell_value}s map is no
          // longer stable.
          Handle<Map> property_cell_value_map(
              Handle<HeapObject>::cast(property_cell_value)->map(), isolate());
          DCHECK(property_cell_value_map->is_stable());
          dependencies()->AssumeMapStable(property_cell_value_map);

          // Check that the {value} is a HeapObject.
          value = effect = graph()->NewNode(simplified()->CheckHeapObject(),
                                            value, effect, control);

          // Check {value} map agains the {property_cell} map.
          effect =
              graph()->NewNode(simplified()->CheckMaps(
                                   CheckMapsFlag::kNone,
                                   ZoneHandleSet<Map>(property_cell_value_map)),
                               value, effect, control);
          property_cell_value_type = Type::OtherInternal();
          representation = MachineRepresentation::kTaggedPointer;
        } else {
          // Check that the {value} is a Smi.
          value = effect = graph()->NewNode(simplified()->CheckSmi(), value,
                                            effect, control);
          property_cell_value_type = Type::SignedSmall();
          representation = MachineRepresentation::kTaggedSigned;
        }
        effect = graph()->NewNode(simplified()->StoreField(ForPropertyCellValue(
                                      representation, property_cell_value_type,
                                      MaybeHandle<Map>(), name)),
                                  jsgraph()->HeapConstant(property_cell), value,
                                  effect, control);
        break;
      }
      case PropertyCellType::kMutable: {
        // Record a code dependency on the cell, and just deoptimize if the
        // property ever becomes read-only.
        dependencies()->AssumePropertyCell(property_cell);
        effect = graph()->NewNode(
            simplified()->StoreField(ForPropertyCellValue(
                MachineRepresentation::kTagged, Type::NonInternal(),
                MaybeHandle<Map>(), name)),
            jsgraph()->HeapConstant(property_cell), value, effect, control);
        break;
      }
    }
  }

  ReplaceWithValue(node, value, effect, control);
  return Replace(value);
}

Reduction JSNativeContextSpecialization::ReduceJSLoadGlobal(Node* node) {
  DCHECK_EQ(IrOpcode::kJSLoadGlobal, node->opcode());
  Handle<Name> name = LoadGlobalParametersOf(node->op()).name();
  Node* effect = NodeProperties::GetEffectInput(node);

  // Try to lookup the name on the script context table first (lexical scoping).
  ScriptContextTableLookupResult result;
  if (LookupInScriptContextTable(name, &result)) {
    if (result.context->is_the_hole(isolate(), result.index)) return NoChange();
    Node* context = jsgraph()->HeapConstant(result.context);
    Node* value = effect = graph()->NewNode(
        javascript()->LoadContext(0, result.index, result.immutable), context,
        effect);
    ReplaceWithValue(node, value, effect);
    return Replace(value);
  }

  // Lookup the {name} on the global object instead.
  return ReduceGlobalAccess(node, nullptr, nullptr, name, AccessMode::kLoad);
}

Reduction JSNativeContextSpecialization::ReduceJSStoreGlobal(Node* node) {
  DCHECK_EQ(IrOpcode::kJSStoreGlobal, node->opcode());
  Handle<Name> name = StoreGlobalParametersOf(node->op()).name();
  Node* value = NodeProperties::GetValueInput(node, 0);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // Try to lookup the name on the script context table first (lexical scoping).
  ScriptContextTableLookupResult result;
  if (LookupInScriptContextTable(name, &result)) {
    if (result.context->is_the_hole(isolate(), result.index)) return NoChange();
    if (result.immutable) return NoChange();
    Node* context = jsgraph()->HeapConstant(result.context);
    effect = graph()->NewNode(javascript()->StoreContext(0, result.index),
                              value, context, effect, control);
    ReplaceWithValue(node, value, effect, control);
    return Replace(value);
  }

  // Lookup the {name} on the global object instead.
  return ReduceGlobalAccess(node, nullptr, value, name, AccessMode::kStore);
}

Reduction JSNativeContextSpecialization::ReduceNamedAccess(
    Node* node, Node* value, MapHandles const& receiver_maps, Handle<Name> name,
    AccessMode access_mode, LanguageMode language_mode, Node* index) {
  DCHECK(node->opcode() == IrOpcode::kJSLoadNamed ||
         node->opcode() == IrOpcode::kJSStoreNamed ||
         node->opcode() == IrOpcode::kJSLoadProperty ||
         node->opcode() == IrOpcode::kJSStoreProperty ||
         node->opcode() == IrOpcode::kJSStoreNamedOwn);
  Node* receiver = NodeProperties::GetValueInput(node, 0);
  Node* context = NodeProperties::GetContextInput(node);
  Node* frame_state = NodeProperties::GetFrameStateInput(node);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // Check if we have an access o.x or o.x=v where o is the current
  // native contexts' global proxy, and turn that into a direct access
  // to the current native contexts' global object instead.
  if (receiver_maps.size() == 1) {
    Handle<Map> receiver_map = receiver_maps.front();
    if (receiver_map->IsJSGlobalProxyMap()) {
      Object* maybe_constructor = receiver_map->GetConstructor();
      // Detached global proxies have |null| as their constructor.
      if (maybe_constructor->IsJSFunction() &&
          JSFunction::cast(maybe_constructor)->native_context() ==
              *native_context()) {
        return ReduceGlobalAccess(node, receiver, value, name, access_mode,
                                  index);
      }
    }
  }

  // Compute property access infos for the receiver maps.
  AccessInfoFactory access_info_factory(dependencies(), native_context(),
                                        graph()->zone());
  ZoneVector<PropertyAccessInfo> access_infos(zone());
  if (!access_info_factory.ComputePropertyAccessInfos(
          receiver_maps, name, access_mode, &access_infos)) {
    return NoChange();
  }

  // TODO(turbofan): Add support for inlining into try blocks.
  bool is_exceptional = NodeProperties::IsExceptionalCall(node);
  for (const auto& access_info : access_infos) {
    if (access_info.IsAccessorConstant()) {
      // Accessor in try-blocks are not supported yet.
      if (is_exceptional || !(flags() & kAccessorInliningEnabled)) {
        return NoChange();
      }
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
      receiver = BuildCheckHeapObject(receiver, &effect, control);
      effect = BuildCheckMaps(receiver, effect, control,
                              access_info.receiver_maps());
    }

    // Generate the actual property access.
    ValueEffectControl continuation = BuildPropertyAccess(
        receiver, value, context, frame_state, effect, control, name,
        access_info, access_mode, language_mode);
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
      receiver = BuildCheckHeapObject(receiver, &effect, control);
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
      Node* this_control = fallthrough_control;

      // Perform map check on {receiver}.
      MapHandles const& receiver_maps = access_info.receiver_maps();
      {
        // Emit a (sequence of) map checks for other {receiver}s.
        ZoneVector<Node*> this_controls(zone());
        ZoneVector<Node*> this_effects(zone());
        if (j == access_infos.size() - 1) {
          // Last map check on the fallthrough control path, do a
          // conditional eager deoptimization exit here.
          this_effect = BuildCheckMaps(receiver, this_effect, this_control,
                                       receiver_maps);
          this_effects.push_back(this_effect);
          this_controls.push_back(fallthrough_control);
          fallthrough_control = nullptr;
        } else {
          for (auto map : receiver_maps) {
            Node* check =
                graph()->NewNode(simplified()->ReferenceEqual(), receiver_map,
                                 jsgraph()->Constant(map));
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
        }
      }

      // Generate the actual property access.
      ValueEffectControl continuation = BuildPropertyAccess(
          this_receiver, this_value, context, frame_state, this_effect,
          this_control, name, access_info, access_mode, language_mode);
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
         node->opcode() == IrOpcode::kJSStoreNamed ||
         node->opcode() == IrOpcode::kJSStoreNamedOwn);
  Node* const receiver = NodeProperties::GetValueInput(node, 0);
  Node* const effect = NodeProperties::GetEffectInput(node);

  // Check if we are accessing the current native contexts' global proxy.
  HeapObjectMatcher m(receiver);
  if (m.HasValue() && m.Value().is_identical_to(global_proxy())) {
    // Optimize accesses to the current native contexts' global proxy.
    return ReduceGlobalAccess(node, nullptr, value, name, access_mode);
  }

  // Check if the {nexus} reports type feedback for the IC.
  if (nexus.IsUninitialized()) {
    if (flags() & kBailoutOnUninitialized) {
      return ReduceSoftDeoptimize(
          node,
          DeoptimizeReason::kInsufficientTypeFeedbackForGenericNamedAccess);
    }
    return NoChange();
  }

  // Extract receiver maps from the IC using the {nexus}.
  MapHandles receiver_maps;
  if (!ExtractReceiverMaps(receiver, effect, nexus, &receiver_maps)) {
    return NoChange();
  } else if (receiver_maps.empty()) {
    if (flags() & kBailoutOnUninitialized) {
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
    if (m.Value()->IsJSFunction() &&
        p.name().is_identical_to(factory()->prototype_string())) {
      // Optimize "prototype" property of functions.
      Handle<JSFunction> function = Handle<JSFunction>::cast(m.Value());
      if (function->IsConstructor()) {
        // We need to add a code dependency on the initial map of the
        // {function} in order to be notified about changes to the
        // "prototype" of {function}.
        JSFunction::EnsureHasInitialMap(function);
        Handle<Map> initial_map(function->initial_map(), isolate());
        dependencies()->AssumeInitialMapCantChange(initial_map);
        Handle<Object> prototype(function->prototype(), isolate());
        Node* value = jsgraph()->Constant(prototype);
        ReplaceWithValue(node, value);
        return Replace(value);
      }
    } else if (m.Value()->IsString() &&
               p.name().is_identical_to(factory()->length_string())) {
      // Constant-fold "length" property on constant strings.
      Handle<String> string = Handle<String>::cast(m.Value());
      Node* value = jsgraph()->Constant(string->length());
      ReplaceWithValue(node, value);
      return Replace(value);
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

Reduction JSNativeContextSpecialization::ReduceJSStoreNamedOwn(Node* node) {
  DCHECK_EQ(IrOpcode::kJSStoreNamedOwn, node->opcode());
  StoreNamedOwnParameters const& p = StoreNamedOwnParametersOf(node->op());
  Node* const value = NodeProperties::GetValueInput(node, 1);

  // Extract receiver maps from the IC using the StoreOwnICNexus.
  if (!p.feedback().IsValid()) return NoChange();
  StoreOwnICNexus nexus(p.feedback().vector(), p.feedback().slot());

  // Try to lower the creation of a named property based on the {receiver_maps}.
  return ReduceNamedAccessFromNexus(node, value, nexus, p.name(),
                                    AccessMode::kStoreInLiteral, STRICT);
}

Reduction JSNativeContextSpecialization::ReduceElementAccess(
    Node* node, Node* index, Node* value, MapHandles const& receiver_maps,
    AccessMode access_mode, LanguageMode language_mode,
    KeyedAccessStoreMode store_mode) {
  DCHECK(node->opcode() == IrOpcode::kJSLoadProperty ||
         node->opcode() == IrOpcode::kJSStoreProperty);
  Node* receiver = NodeProperties::GetValueInput(node, 0);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);
  Node* frame_state = NodeProperties::FindFrameStateBefore(node);

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

    // Return the character from the {receiver} as single character string.
    value = graph()->NewNode(simplified()->StringCharAt(), receiver, index,
                             control);
  } else {
    // Retrieve the native context from the given {node}.
    // Compute element access infos for the receiver maps.
    AccessInfoFactory access_info_factory(dependencies(), native_context(),
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
    receiver = BuildCheckHeapObject(receiver, &effect, control);

    // Check for the monomorphic case.
    if (access_infos.size() == 1) {
      ElementAccessInfo access_info = access_infos.front();

      // Perform possible elements kind transitions.
      for (auto transition : access_info.transitions()) {
        Handle<Map> const transition_source = transition.first;
        Handle<Map> const transition_target = transition.second;
        effect = graph()->NewNode(
            simplified()->TransitionElementsKind(ElementsTransition(
                IsSimpleMapChangeTransition(transition_source->elements_kind(),
                                            transition_target->elements_kind())
                    ? ElementsTransition::kFastTransition
                    : ElementsTransition::kSlowTransition,
                transition_source, transition_target)),
            receiver, effect, control);
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
      ValueEffectControl continuation =
          BuildElementAccess(receiver, index, value, effect, control,
                             access_info, access_mode, store_mode);
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
                  ElementsTransition(IsSimpleMapChangeTransition(
                                         transition_source->elements_kind(),
                                         transition_target->elements_kind())
                                         ? ElementsTransition::kFastTransition
                                         : ElementsTransition::kSlowTransition,
                                     transition_source, transition_target)),
              receiver, this_effect, this_control);
        }

        // Load the {receiver} map.
        Node* receiver_map = this_effect =
            graph()->NewNode(simplified()->LoadField(AccessBuilder::ForMap()),
                             receiver, this_effect, this_control);

        // Perform map check(s) on {receiver}.
        MapHandles const& receiver_maps = access_info.receiver_maps();
        if (j == access_infos.size() - 1) {
          // Last map check on the fallthrough control path, do a
          // conditional eager deoptimization exit here.
          this_effect = BuildCheckMaps(receiver, this_effect, this_control,
                                       receiver_maps);
          fallthrough_control = nullptr;
        } else {
          ZoneVector<Node*> this_controls(zone());
          ZoneVector<Node*> this_effects(zone());
          for (Handle<Map> map : receiver_maps) {
            Node* check =
                graph()->NewNode(simplified()->ReferenceEqual(), receiver_map,
                                 jsgraph()->Constant(map));
            Node* branch = graph()->NewNode(common()->Branch(), check,
                                            fallthrough_control);
            this_controls.push_back(
                graph()->NewNode(common()->IfTrue(), branch));
            this_effects.push_back(this_effect);
            fallthrough_control = graph()->NewNode(common()->IfFalse(), branch);
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

        // Access the actual element.
        ValueEffectControl continuation = BuildElementAccess(
            this_receiver, this_index, this_value, this_effect, this_control,
            access_info, access_mode, store_mode);
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
  Node* receiver = NodeProperties::GetValueInput(node, 0);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // Optimize access for constant {receiver}.
  HeapObjectMatcher mreceiver(receiver);
  if (mreceiver.HasValue() && mreceiver.Value()->IsString()) {
    Handle<String> string = Handle<String>::cast(mreceiver.Value());

    // Strings are immutable in JavaScript.
    if (access_mode == AccessMode::kStore) return NoChange();

    // Properly deal with constant {index}.
    NumberMatcher mindex(index);
    if (mindex.IsInteger() && mindex.IsInRange(0.0, string->length() - 1)) {
      // Constant-fold the {index} access to {string}.
      Node* value = jsgraph()->HeapConstant(
          factory()->LookupSingleCharacterStringFromCode(
              string->Get(static_cast<int>(mindex.Value()))));
      ReplaceWithValue(node, value, effect, control);
      return Replace(value);
    }

    // We can only assume that the {index} is a valid array index if the IC
    // is in element access mode and not MEGAMORPHIC, otherwise there's no
    // guard for the bounds check below.
    if (nexus.ic_state() != MEGAMORPHIC && nexus.GetKeyType() == ELEMENT) {
      // Ensure that {index} is less than {receiver} length.
      Node* length = jsgraph()->Constant(string->length());
      index = effect = graph()->NewNode(simplified()->CheckBounds(), index,
                                        length, effect, control);

      // Return the character from the {receiver} as single character string.
      value = graph()->NewNode(simplified()->StringCharAt(), receiver, index,
                               control);
      ReplaceWithValue(node, value, effect, control);
      return Replace(value);
    }
  }

  // Check if the {nexus} reports type feedback for the IC.
  if (nexus.IsUninitialized()) {
    if (flags() & kBailoutOnUninitialized) {
      return ReduceSoftDeoptimize(
          node,
          DeoptimizeReason::kInsufficientTypeFeedbackForGenericKeyedAccess);
    }
    return NoChange();
  }

  // Extract receiver maps from the {nexus}.
  MapHandles receiver_maps;
  if (!ExtractReceiverMaps(receiver, effect, nexus, &receiver_maps)) {
    return NoChange();
  } else if (receiver_maps.empty()) {
    if (flags() & kBailoutOnUninitialized) {
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
  } else if (nexus.ic_state() == MEGAMORPHIC) {
    // The KeyedLoad/StoreIC uses the MEGAMORPHIC state to guard the assumption
    // that a numeric {index} is within the valid bounds for {receiver}, i.e.
    // it transitions to MEGAMORPHIC once it sees an out-of-bounds access. Thus
    // we cannot continue here if the IC state is MEGAMORPHIC.
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
    Node* control, Handle<Name> name, PropertyAccessInfo const& access_info,
    AccessMode access_mode, LanguageMode language_mode) {
  // Determine actual holder and perform prototype chain checks.
  Handle<JSObject> holder;
  if (access_info.holder().ToHandle(&holder)) {
    DCHECK_NE(AccessMode::kStoreInLiteral, access_mode);
    AssumePrototypesStable(access_info.receiver_maps(), holder);
  }

  // Generate the actual property access.
  if (access_info.IsNotFound()) {
    DCHECK_EQ(AccessMode::kLoad, access_mode);
    value = jsgraph()->UndefinedConstant();
  } else if (access_info.IsDataConstant()) {
    DCHECK(!FLAG_track_constant_fields);
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
            graph()->NewNode(common()->StateValues(1, SparseInputMask::Dense()),
                             receiver),
            jsgraph()->EmptyStateValues(), jsgraph()->EmptyStateValues(),
            context, target, frame_state);

        // Introduce the call to the getter function.
        if (access_info.constant()->IsJSFunction()) {
          value = effect = control = graph()->NewNode(
              javascript()->Call(2, CallFrequency(), VectorSlotPair(),
                                 ConvertReceiverMode::kNotNullOrUndefined),
              target, receiver, context, frame_state0, effect, control);
        } else {
          DCHECK(access_info.constant()->IsFunctionTemplateInfo());
          Handle<FunctionTemplateInfo> function_template_info(
              Handle<FunctionTemplateInfo>::cast(access_info.constant()));
          DCHECK(!function_template_info->call_code()->IsUndefined(isolate()));
          ValueEffectControl value_effect_control = InlineApiCall(
              receiver, context, target, frame_state0, nullptr, effect, control,
              shared_info, function_template_info);
          value = value_effect_control.value();
          effect = value_effect_control.effect();
          control = value_effect_control.control();
        }
        break;
      }
      case AccessMode::kStoreInLiteral:
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
            graph()->NewNode(common()->StateValues(2, SparseInputMask::Dense()),
                             receiver, value),
            jsgraph()->EmptyStateValues(), jsgraph()->EmptyStateValues(),
            context, target, frame_state);

        // Introduce the call to the setter function.
        if (access_info.constant()->IsJSFunction()) {
          effect = control = graph()->NewNode(
              javascript()->Call(3, CallFrequency(), VectorSlotPair(),
                                 ConvertReceiverMode::kNotNullOrUndefined),
              target, receiver, value, context, frame_state0, effect, control);
        } else {
          DCHECK(access_info.constant()->IsFunctionTemplateInfo());
          Handle<FunctionTemplateInfo> function_template_info(
              Handle<FunctionTemplateInfo>::cast(access_info.constant()));
          DCHECK(!function_template_info->call_code()->IsUndefined(isolate()));
          ValueEffectControl value_effect_control = InlineApiCall(
              receiver, context, target, frame_state0, value, effect, control,
              shared_info, function_template_info);
          value = value_effect_control.value();
          effect = value_effect_control.effect();
          control = value_effect_control.control();
        }
        break;
      }
    }
  } else {
    DCHECK(access_info.IsDataField() || access_info.IsDataConstantField());
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
        // TODO(ishell): Use something simpler like
        //
        // Handle<Object> value =
        //     JSObject::FastPropertyAt(Handle<JSObject>::cast(m.Value()),
        //                              Representation::Tagged(), field_index);
        //
        // here, once we have the immutable bit in the access_info.

        // TODO(turbofan): Given that we already have the field_index here, we
        // might be smarter in the future and not rely on the LookupIterator,
        // but for now let's just do what Crankshaft does.
        LookupIterator it(m.Value(), name,
                          LookupIterator::OWN_SKIP_INTERCEPTOR);
        if (it.state() == LookupIterator::DATA) {
          bool is_reaonly_non_configurable =
              it.IsReadOnly() && !it.IsConfigurable();
          if (is_reaonly_non_configurable ||
              (FLAG_track_constant_fields &&
               access_info.IsDataConstantField())) {
            Node* value = jsgraph()->Constant(JSReceiver::GetDataProperty(&it));
            if (!is_reaonly_non_configurable) {
              // It's necessary to add dependency on the map that introduced
              // the field.
              DCHECK(access_info.IsDataConstantField());
              DCHECK(!it.is_dictionary_holder());
              Handle<Map> field_owner_map = it.GetFieldOwnerMap();
              dependencies()->AssumeFieldOwner(field_owner_map);
            }
            return ValueEffectControl(value, effect, control);
          }
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
        MaybeHandle<Map>(),
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
                                              MaybeHandle<Map>(),
                                              Type::OtherInternal(),
                                              MachineType::TaggedPointer(),
                                              kPointerWriteBarrier};
          storage = effect =
              graph()->NewNode(simplified()->LoadField(storage_access), storage,
                               effect, control);
          field_access.offset = HeapNumber::kValueOffset;
          field_access.name = MaybeHandle<Name>();
        }
      } else if (field_representation ==
                 MachineRepresentation::kTaggedPointer) {
        // Remember the map of the field value, if its map is stable. This is
        // used by the LoadElimination to eliminate map checks on the result.
        Handle<Map> field_map;
        if (access_info.field_map().ToHandle(&field_map)) {
          if (field_map->is_stable()) {
            dependencies()->AssumeMapStable(field_map);
            field_access.map = field_map;
          }
        }
      }
      value = effect = graph()->NewNode(simplified()->LoadField(field_access),
                                        storage, effect, control);
    } else {
      bool store_to_constant_field = FLAG_track_constant_fields &&
                                     (access_mode == AccessMode::kStore) &&
                                     access_info.IsDataConstantField();

      DCHECK(access_mode == AccessMode::kStore ||
             access_mode == AccessMode::kStoreInLiteral);
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
                  simplified()->Allocate(Type::OtherInternal(), NOT_TENURED),
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
                                                  MaybeHandle<Map>(),
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
          if (store_to_constant_field) {
            DCHECK(!access_info.HasTransitionMap());
            // If the field is constant check that the value we are going
            // to store matches current value.
            Node* current_value = effect =
                graph()->NewNode(simplified()->LoadField(field_access), storage,
                                 effect, control);

            Node* check = graph()->NewNode(simplified()->NumberEqual(),
                                           current_value, value);
            effect = graph()->NewNode(simplified()->CheckIf(), check, effect,
                                      control);
            return ValueEffectControl(value, effect, control);
          }
          break;
        }
        case MachineRepresentation::kTaggedSigned:
        case MachineRepresentation::kTaggedPointer:
        case MachineRepresentation::kTagged:
          if (store_to_constant_field) {
            DCHECK(!access_info.HasTransitionMap());
            // If the field is constant check that the value we are going
            // to store matches current value.
            Node* current_value = effect =
                graph()->NewNode(simplified()->LoadField(field_access), storage,
                                 effect, control);

            Node* check = graph()->NewNode(simplified()->ReferenceEqual(),
                                           current_value, value);
            effect = graph()->NewNode(simplified()->CheckIf(), check, effect,
                                      control);
            return ValueEffectControl(value, effect, control);
          }

          if (field_representation == MachineRepresentation::kTaggedSigned) {
            value = effect = graph()->NewNode(simplified()->CheckSmi(), value,
                                              effect, control);
            field_access.write_barrier_kind = kNoWriteBarrier;

          } else if (field_representation ==
                     MachineRepresentation::kTaggedPointer) {
            // Ensure that {value} is a HeapObject.
            value = BuildCheckHeapObject(value, &effect, control);
            Handle<Map> field_map;
            if (access_info.field_map().ToHandle(&field_map)) {
              // Emit a map check for the value.
              effect = graph()->NewNode(
                  simplified()->CheckMaps(CheckMapsFlag::kNone,
                                          ZoneHandleSet<Map>(field_map)),
                  value, effect, control);
            }
            field_access.write_barrier_kind = kPointerWriteBarrier;

          } else {
            DCHECK_EQ(MachineRepresentation::kTagged, field_representation);
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
        case MachineRepresentation::kSimd1x4:
        case MachineRepresentation::kSimd1x8:
        case MachineRepresentation::kSimd1x16:
          UNREACHABLE();
          break;
      }
      // Check if we need to perform a transitioning store.
      Handle<Map> transition_map;
      if (access_info.transition_map().ToHandle(&transition_map)) {
        // Check if we need to grow the properties backing store
        // with this transitioning store.
        Handle<Map> original_map(Map::cast(transition_map->GetBackPointer()),
                                 isolate());
        if (original_map->unused_property_fields() == 0) {
          DCHECK(!field_index.is_inobject());

          // Reallocate the properties {storage}.
          storage = effect = BuildExtendPropertiesBackingStore(
              original_map, storage, effect, control);

          // Perform the actual store.
          effect = graph()->NewNode(simplified()->StoreField(field_access),
                                    storage, value, effect, control);

          // Atomically switch to the new properties below.
          field_access = AccessBuilder::ForJSObjectProperties();
          value = storage;
          storage = receiver;
        }
        effect = graph()->NewNode(
            common()->BeginRegion(RegionObservability::kObservable), effect);
        effect = graph()->NewNode(
            simplified()->StoreField(AccessBuilder::ForMap()), receiver,
            jsgraph()->Constant(transition_map), effect, control);
        effect = graph()->NewNode(simplified()->StoreField(field_access),
                                  storage, value, effect, control);
        effect = graph()->NewNode(common()->FinishRegion(),
                                  jsgraph()->UndefinedConstant(), effect);
      } else {
        // Regular non-transitioning field store.
        effect = graph()->NewNode(simplified()->StoreField(field_access),
                                  storage, value, effect, control);
      }
    }
  }

  return ValueEffectControl(value, effect, control);
}

Reduction JSNativeContextSpecialization::ReduceJSStoreDataPropertyInLiteral(
    Node* node) {
  DCHECK_EQ(IrOpcode::kJSStoreDataPropertyInLiteral, node->opcode());

  FeedbackParameter const& p = FeedbackParameterOf(node->op());

  if (!p.feedback().IsValid()) return NoChange();

  StoreDataPropertyInLiteralICNexus nexus(p.feedback().vector(),
                                          p.feedback().slot());
  if (nexus.IsUninitialized()) {
    return NoChange();
  }

  if (nexus.ic_state() == MEGAMORPHIC) {
    return NoChange();
  }

  DCHECK_EQ(MONOMORPHIC, nexus.ic_state());

  Map* map = nexus.FindFirstMap();
  if (map == nullptr) {
    // Maps are weakly held in the type feedback vector, we may not have one.
    return NoChange();
  }

  Handle<Map> receiver_map(map, isolate());
  Handle<Name> cached_name =
      handle(Name::cast(nexus.GetFeedbackExtra()), isolate());

  PropertyAccessInfo access_info;
  AccessInfoFactory access_info_factory(dependencies(), native_context(),
                                        graph()->zone());
  if (!access_info_factory.ComputePropertyAccessInfo(
          receiver_map, cached_name, AccessMode::kStoreInLiteral,
          &access_info)) {
    return NoChange();
  }

  Node* receiver = NodeProperties::GetValueInput(node, 0);
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

  // Monomorphic property access.
  receiver = BuildCheckHeapObject(receiver, &effect, control);

  effect =
      BuildCheckMaps(receiver, effect, control, access_info.receiver_maps());

  // Ensure that {name} matches the cached name.
  Node* name = NodeProperties::GetValueInput(node, 1);
  Node* check = graph()->NewNode(simplified()->ReferenceEqual(), name,
                                 jsgraph()->HeapConstant(cached_name));
  effect = graph()->NewNode(simplified()->CheckIf(), check, effect, control);

  Node* value = NodeProperties::GetValueInput(node, 2);
  Node* context = NodeProperties::GetContextInput(node);
  Node* frame_state_lazy = NodeProperties::GetFrameStateInput(node);

  // Generate the actual property access.
  ValueEffectControl continuation = BuildPropertyAccess(
      receiver, value, context, frame_state_lazy, effect, control, cached_name,
      access_info, AccessMode::kStoreInLiteral, LanguageMode::SLOPPY);
  value = continuation.value();
  effect = continuation.effect();
  control = continuation.control();

  ReplaceWithValue(node, value, effect, control);
  return Replace(value);
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
    ElementAccessInfo const& access_info, AccessMode access_mode,
    KeyedAccessStoreMode store_mode) {
  DCHECK_NE(AccessMode::kStoreInLiteral, access_mode);

  // TODO(bmeurer): We currently specialize based on elements kind. We should
  // also be able to properly support strings and other JSObjects here.
  ElementsKind elements_kind = access_info.elements_kind();
  MapHandles const& receiver_maps = access_info.receiver_maps();

  if (IsFixedTypedArrayElementsKind(elements_kind)) {
    Node* buffer;
    Node* length;
    Node* base_pointer;
    Node* external_pointer;

    // Check if we can constant-fold information about the {receiver} (i.e.
    // for asm.js-like code patterns).
    HeapObjectMatcher m(receiver);
    if (m.HasValue() && m.Value()->IsJSTypedArray()) {
      Handle<JSTypedArray> typed_array = Handle<JSTypedArray>::cast(m.Value());

      // Determine the {receiver}s (known) length.
      length = jsgraph()->Constant(typed_array->length_value());

      // Check if the {receiver}s buffer was neutered.
      buffer = jsgraph()->HeapConstant(typed_array->GetBuffer());

      // Load the (known) base and external pointer for the {receiver}. The
      // {external_pointer} might be invalid if the {buffer} was neutered, so
      // we need to make sure that any access is properly guarded.
      base_pointer = jsgraph()->ZeroConstant();
      external_pointer = jsgraph()->PointerConstant(
          FixedTypedArrayBase::cast(typed_array->elements())
              ->external_pointer());
    } else {
      // Load the {receiver}s length.
      length = effect = graph()->NewNode(
          simplified()->LoadField(AccessBuilder::ForJSTypedArrayLength()),
          receiver, effect, control);

      // Load the buffer for the {receiver}.
      buffer = effect = graph()->NewNode(
          simplified()->LoadField(AccessBuilder::ForJSArrayBufferViewBuffer()),
          receiver, effect, control);

      // Load the elements for the {receiver}.
      Node* elements = effect = graph()->NewNode(
          simplified()->LoadField(AccessBuilder::ForJSObjectElements()),
          receiver, effect, control);

      // Load the base and external pointer for the {receiver}s {elements}.
      base_pointer = effect = graph()->NewNode(
          simplified()->LoadField(
              AccessBuilder::ForFixedTypedArrayBaseBasePointer()),
          elements, effect, control);
      external_pointer = effect = graph()->NewNode(
          simplified()->LoadField(
              AccessBuilder::ForFixedTypedArrayBaseExternalPointer()),
          elements, effect, control);
    }

    // See if we can skip the neutering check.
    if (isolate()->IsArrayBufferNeuteringIntact()) {
      // Add a code dependency so we are deoptimized in case an ArrayBuffer
      // gets neutered.
      dependencies()->AssumePropertyCell(
          factory()->array_buffer_neutering_protector());
    } else {
      // Default to zero if the {receiver}s buffer was neutered.
      Node* check = effect = graph()->NewNode(
          simplified()->ArrayBufferWasNeutered(), buffer, effect, control);
      length = graph()->NewNode(
          common()->Select(MachineRepresentation::kTagged, BranchHint::kFalse),
          check, jsgraph()->ZeroConstant(), length);
    }

    if (store_mode == STORE_NO_TRANSITION_IGNORE_OUT_OF_BOUNDS) {
      // Check that the {index} is a valid array index, we do the actual
      // bounds check below and just skip the store below if it's out of
      // bounds for the {receiver}.
      index = effect = graph()->NewNode(simplified()->CheckBounds(), index,
                                        jsgraph()->Constant(Smi::kMaxValue),
                                        effect, control);
    } else {
      // Check that the {index} is in the valid range for the {receiver}.
      index = effect = graph()->NewNode(simplified()->CheckBounds(), index,
                                        length, effect, control);
    }

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
      case AccessMode::kStoreInLiteral:
        UNREACHABLE();
        break;
      case AccessMode::kStore: {
        // Ensure that the {value} is actually a Number.
        value = effect = graph()->NewNode(simplified()->CheckNumber(), value,
                                          effect, control);

        // Introduce the appropriate truncation for {value}. Currently we
        // only need to do this for ClamedUint8Array {receiver}s, as the
        // other truncations are implicit in the StoreTypedElement, but we
        // might want to change that at some point.
        if (external_array_type == kExternalUint8ClampedArray) {
          value = graph()->NewNode(simplified()->NumberToUint8Clamped(), value);
        }

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
          effect = graph()->NewNode(
              simplified()->StoreTypedElement(external_array_type), buffer,
              base_pointer, external_pointer, index, value, effect, control);
        }
        break;
      }
    }
  } else {
    // Load the elements for the {receiver}.
    Node* elements = effect = graph()->NewNode(
        simplified()->LoadField(AccessBuilder::ForJSObjectElements()), receiver,
        effect, control);

    // Don't try to store to a copy-on-write backing store.
    if (access_mode == AccessMode::kStore &&
        IsFastSmiOrObjectElementsKind(elements_kind) &&
        store_mode != STORE_NO_TRANSITION_HANDLE_COW) {
      effect = graph()->NewNode(
          simplified()->CheckMaps(
              CheckMapsFlag::kNone,
              ZoneHandleSet<Map>(factory()->fixed_array_map())),
          elements, effect, control);
    }

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
      element_type = Type::SignedSmall();
      element_machine_type = MachineType::TaggedSigned();
    }
    ElementAccess element_access = {kTaggedBase, FixedArray::kHeaderSize,
                                    element_type, element_machine_type,
                                    kFullWriteBarrier};

    // Access the actual element.
    if (access_mode == AccessMode::kLoad) {
      // Compute the real element access type, which includes the hole in case
      // of holey backing stores.
      if (IsHoleyElementsKind(elements_kind)) {
        element_access.type =
            Type::Union(element_type, Type::Hole(), graph()->zone());
      }
      if (elements_kind == FAST_HOLEY_ELEMENTS ||
          elements_kind == FAST_HOLEY_SMI_ELEMENTS) {
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
        if (CanTreatHoleAsUndefined(receiver_maps)) {
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
        if (CanTreatHoleAsUndefined(receiver_maps)) {
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

JSNativeContextSpecialization::ValueEffectControl
JSNativeContextSpecialization::InlineApiCall(
    Node* receiver, Node* context, Node* target, Node* frame_state, Node* value,
    Node* effect, Node* control, Handle<SharedFunctionInfo> shared_info,
    Handle<FunctionTemplateInfo> function_template_info) {
  Handle<CallHandlerInfo> call_handler_info = handle(
      CallHandlerInfo::cast(function_template_info->call_code()), isolate());
  Handle<Object> call_data_object(call_handler_info->data(), isolate());

  // Only setters have a value.
  int const argc = value == nullptr ? 0 : 1;
  // The stub always expects the receiver as the first param on the stack.
  CallApiCallbackStub stub(
      isolate(), argc,
      true /* FunctionTemplateInfo doesn't have an associated context. */);
  CallInterfaceDescriptor call_interface_descriptor =
      stub.GetCallInterfaceDescriptor();
  CallDescriptor* call_descriptor = Linkage::GetStubCallDescriptor(
      isolate(), graph()->zone(), call_interface_descriptor,
      call_interface_descriptor.GetStackParameterCount() + argc +
          1 /* implicit receiver */,
      CallDescriptor::kNeedsFrameState, Operator::kNoProperties,
      MachineType::AnyTagged(), 1);

  Node* data = jsgraph()->Constant(call_data_object);
  ApiFunction function(v8::ToCData<Address>(call_handler_info->callback()));
  Node* function_reference =
      graph()->NewNode(common()->ExternalConstant(ExternalReference(
          &function, ExternalReference::DIRECT_API_CALL, isolate())));
  Node* code = jsgraph()->HeapConstant(stub.GetCode());

  // Add CallApiCallbackStub's register argument as well.
  Node* inputs[11] = {
      code, target, data, receiver /* holder */, function_reference, receiver};
  int index = 6 + argc;
  inputs[index++] = context;
  inputs[index++] = frame_state;
  inputs[index++] = effect;
  inputs[index++] = control;
  // This needs to stay here because of the edge case described in
  // http://crbug.com/675648.
  if (value != nullptr) {
    inputs[6] = value;
  }

  Node* control0;
  Node* effect0;
  Node* value0 = effect0 = control0 =
      graph()->NewNode(common()->Call(call_descriptor), index, inputs);
  return ValueEffectControl(value0, effect0, control0);
}

Node* JSNativeContextSpecialization::BuildCheckHeapObject(Node* receiver,
                                                          Node** effect,
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
      return receiver;
    }
    default: {
      return *effect = graph()->NewNode(simplified()->CheckHeapObject(),
                                        receiver, *effect, control);
    }
  }
}

Node* JSNativeContextSpecialization::BuildCheckMaps(
    Node* receiver, Node* effect, Node* control,
    MapHandles const& receiver_maps) {
  HeapObjectMatcher m(receiver);
  if (m.HasValue()) {
    Handle<Map> receiver_map(m.Value()->map(), isolate());
    if (receiver_map->is_stable()) {
      for (Handle<Map> map : receiver_maps) {
        if (map.is_identical_to(receiver_map)) {
          dependencies()->AssumeMapStable(receiver_map);
          return effect;
        }
      }
    }
  }
  ZoneHandleSet<Map> maps;
  CheckMapsFlags flags = CheckMapsFlag::kNone;
  for (Handle<Map> map : receiver_maps) {
    maps.insert(map, graph()->zone());
    if (map->is_migration_target()) {
      flags |= CheckMapsFlag::kTryMigrateInstance;
    }
  }
  return graph()->NewNode(simplified()->CheckMaps(flags, maps), receiver,
                          effect, control);
}

Node* JSNativeContextSpecialization::BuildExtendPropertiesBackingStore(
    Handle<Map> map, Node* properties, Node* effect, Node* control) {
  // TODO(bmeurer/jkummerow): Property deletions can undo map transitions
  // while keeping the backing store around, meaning that even though the
  // map might believe that objects have no unused property fields, there
  // might actually be some. It would be nice to not create a new backing
  // store in that case (i.e. when properties->length() >= new_length).
  // However, introducing branches and Phi nodes here would make it more
  // difficult for escape analysis to get rid of the backing stores used
  // for intermediate states of chains of property additions. That makes
  // it unclear what the best approach is here.
  DCHECK_EQ(0, map->unused_property_fields());
  // Compute the length of the old {properties} and the new properties.
  int length = map->NextFreePropertyIndex() - map->GetInObjectProperties();
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
  // Allocate and initialize the new properties.
  effect = graph()->NewNode(
      common()->BeginRegion(RegionObservability::kNotObservable), effect);
  Node* new_properties = effect = graph()->NewNode(
      simplified()->Allocate(Type::OtherInternal(), NOT_TENURED),
      jsgraph()->Constant(FixedArray::SizeFor(new_length)), effect, control);
  effect = graph()->NewNode(simplified()->StoreField(AccessBuilder::ForMap()),
                            new_properties, jsgraph()->FixedArrayMapConstant(),
                            effect, control);
  effect = graph()->NewNode(
      simplified()->StoreField(AccessBuilder::ForFixedArrayLength()),
      new_properties, jsgraph()->Constant(new_length), effect, control);
  for (int i = 0; i < new_length; ++i) {
    effect = graph()->NewNode(
        simplified()->StoreField(AccessBuilder::ForFixedArraySlot(i)),
        new_properties, values[i], effect, control);
  }
  return graph()->NewNode(common()->FinishRegion(), new_properties, effect);
}

void JSNativeContextSpecialization::AssumePrototypesStable(
    MapHandles const& receiver_maps, Handle<JSObject> holder) {
  // Determine actual holder and perform prototype chain checks.
  for (auto map : receiver_maps) {
    // Perform the implicit ToObject for primitives here.
    // Implemented according to ES6 section 7.3.2 GetV (V, P).
    Handle<JSFunction> constructor;
    if (Map::GetConstructorFunction(map, native_context())
            .ToHandle(&constructor)) {
      map = handle(constructor->initial_map(), isolate());
    }
    dependencies()->AssumePrototypeMapsStable(map, holder);
  }
}

bool JSNativeContextSpecialization::CanTreatHoleAsUndefined(
    MapHandles const& receiver_maps) {
  // Check if the array prototype chain is intact.
  if (!isolate()->IsFastArrayConstructorPrototypeChainIntact()) return false;

  // Make sure both the initial Array and Object prototypes are stable.
  Handle<JSObject> initial_array_prototype(
      native_context()->initial_array_prototype(), isolate());
  Handle<JSObject> initial_object_prototype(
      native_context()->initial_object_prototype(), isolate());
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

JSNativeContextSpecialization::InferHasInPrototypeChainResult
JSNativeContextSpecialization::InferHasInPrototypeChain(
    Node* receiver, Node* effect, Handle<JSReceiver> prototype) {
  ZoneHandleSet<Map> receiver_maps;
  NodeProperties::InferReceiverMapsResult result =
      NodeProperties::InferReceiverMaps(receiver, effect, &receiver_maps);
  if (result == NodeProperties::kNoReceiverMaps) return kMayBeInPrototypeChain;

  // Check if either all or none of the {receiver_maps} have the given
  // {prototype} in their prototype chain.
  bool all = true;
  bool none = true;
  for (size_t i = 0; i < receiver_maps.size(); ++i) {
    Handle<Map> receiver_map = receiver_maps[i];
    if (receiver_map->instance_type() <= LAST_SPECIAL_RECEIVER_TYPE) {
      return kMayBeInPrototypeChain;
    }
    if (result == NodeProperties::kUnreliableReceiverMaps) {
      // In case of an unreliable {result} we need to ensure that all
      // {receiver_maps} are stable, because otherwise we cannot trust
      // the {receiver_maps} information, since arbitrary side-effects
      // may have happened.
      if (!receiver_map->is_stable()) {
        return kMayBeInPrototypeChain;
      }
    }
    for (PrototypeIterator j(receiver_map);; j.Advance()) {
      if (j.IsAtEnd()) {
        all = false;
        break;
      }
      Handle<JSReceiver> const current =
          PrototypeIterator::GetCurrent<JSReceiver>(j);
      if (current.is_identical_to(prototype)) {
        none = false;
        break;
      }
      if (!current->map()->is_stable() ||
          current->map()->instance_type() <= LAST_SPECIAL_RECEIVER_TYPE) {
        return kMayBeInPrototypeChain;
      }
    }
  }
  DCHECK_IMPLIES(all, !none);
  DCHECK_IMPLIES(none, !all);

  if (all) return kIsInPrototypeChain;
  if (none) return kIsNotInPrototypeChain;
  return kMayBeInPrototypeChain;
}

bool JSNativeContextSpecialization::ExtractReceiverMaps(
    Node* receiver, Node* effect, FeedbackNexus const& nexus,
    MapHandles* receiver_maps) {
  DCHECK_EQ(0, receiver_maps->size());
  // See if we can infer a concrete type for the {receiver}.
  if (InferReceiverMaps(receiver, effect, receiver_maps)) {
    // We can assume that the {receiver} still has the infered {receiver_maps}.
    return true;
  }
  // Try to extract some maps from the {nexus}.
  if (nexus.ExtractMaps(receiver_maps) != 0) {
    // Try to filter impossible candidates based on infered root map.
    Handle<Map> receiver_map;
    if (InferReceiverRootMap(receiver).ToHandle(&receiver_map)) {
      receiver_maps->erase(
          std::remove_if(receiver_maps->begin(), receiver_maps->end(),
                         [receiver_map](const Handle<Map>& map) {
                           return map->FindRootMap() != *receiver_map;
                         }),
          receiver_maps->end());
    }
    return true;
  }
  return false;
}

bool JSNativeContextSpecialization::InferReceiverMaps(
    Node* receiver, Node* effect, MapHandles* receiver_maps) {
  ZoneHandleSet<Map> maps;
  NodeProperties::InferReceiverMapsResult result =
      NodeProperties::InferReceiverMaps(receiver, effect, &maps);
  if (result == NodeProperties::kReliableReceiverMaps) {
    for (size_t i = 0; i < maps.size(); ++i) {
      receiver_maps->push_back(maps[i]);
    }
    return true;
  } else if (result == NodeProperties::kUnreliableReceiverMaps) {
    // For untrusted receiver maps, we can still use the information
    // if the maps are stable.
    for (size_t i = 0; i < maps.size(); ++i) {
      if (!maps[i]->is_stable()) return false;
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

bool JSNativeContextSpecialization::LookupInScriptContextTable(
    Handle<Name> name, ScriptContextTableLookupResult* result) {
  if (!name->IsString()) return false;
  Handle<ScriptContextTable> script_context_table(
      global_object()->native_context()->script_context_table(), isolate());
  ScriptContextTable::LookupResult lookup_result;
  if (!ScriptContextTable::Lookup(script_context_table,
                                  Handle<String>::cast(name), &lookup_result)) {
    return false;
  }
  Handle<Context> script_context = ScriptContextTable::GetContext(
      script_context_table, lookup_result.context_index);
  result->context = script_context;
  result->immutable = lookup_result.mode == CONST;
  result->index = lookup_result.slot_index;
  return true;
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
