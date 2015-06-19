// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-type-feedback.h"

#include "src/property-details.h"

#include "src/accessors.h"
#include "src/ast.h"
#include "src/compiler.h"
#include "src/type-info.h"

#include "src/compiler/access-builder.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/node-aux-data.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/operator-properties.h"
#include "src/compiler/simplified-operator.h"

namespace v8 {
namespace internal {
namespace compiler {

enum LoadOrStore { LOAD, STORE };

#define EAGER_DEOPT_LOCATIONS_FOR_PROPERTY_ACCESS_ARE_CORRECT false

JSTypeFeedbackTable::JSTypeFeedbackTable(Zone* zone)
    : map_(TypeFeedbackIdMap::key_compare(),
           TypeFeedbackIdMap::allocator_type(zone)) {}


void JSTypeFeedbackTable::Record(Node* node, TypeFeedbackId id) {
  map_.insert(std::make_pair(node->id(), id));
}


Reduction JSTypeFeedbackSpecializer::Reduce(Node* node) {
  switch (node->opcode()) {
    case IrOpcode::kJSLoadProperty: {
      HeapObjectMatcher<Name> match(node->InputAt(1));
      if (match.HasValue() && match.Value().handle()->IsName()) {
        // LoadProperty(o, "constant") => LoadNamed["constant"](o).
        Unique<Name> name = match.Value();
        const VectorSlotPair& feedback =
            LoadPropertyParametersOf(node->op()).feedback();
        node->set_op(jsgraph()->javascript()->LoadNamed(name, feedback,
                                                        NOT_CONTEXTUAL, KEYED));
        node->RemoveInput(1);
        return ReduceJSLoadNamed(node);
      }
      return ReduceJSLoadProperty(node);
    }
    case IrOpcode::kJSLoadNamed:
      return ReduceJSLoadNamed(node);
    case IrOpcode::kJSStoreNamed:
      return ReduceJSStoreNamed(node);
    case IrOpcode::kJSStoreProperty: {
      HeapObjectMatcher<Name> match(node->InputAt(1));
      if (match.HasValue() && match.Value().handle()->IsName()) {
        // StoreProperty(o, "constant", v) => StoreNamed["constant"](o, v).
        Unique<Name> name = match.Value();
        LanguageMode language_mode = OpParameter<LanguageMode>(node);
        // StoreProperty has 2 frame state inputs, but StoreNamed only 1.
        DCHECK_EQ(2, OperatorProperties::GetFrameStateInputCount(node->op()));
        node->RemoveInput(NodeProperties::FirstFrameStateIndex(node) + 1);
        node->set_op(
            jsgraph()->javascript()->StoreNamed(language_mode, name, KEYED));
        node->RemoveInput(1);
        return ReduceJSStoreNamed(node);
      }
      return ReduceJSStoreProperty(node);
    }
    default:
      break;
  }
  return NoChange();
}


static void AddFieldAccessTypes(FieldAccess* access,
                                PropertyDetails property_details) {
  if (property_details.representation().IsSmi()) {
    access->type = Type::SignedSmall();
    access->machine_type = static_cast<MachineType>(kTypeInt32 | kRepTagged);
  } else if (property_details.representation().IsDouble()) {
    access->type = Type::Number();
    access->machine_type = kMachFloat64;
  }
}


static bool GetInObjectFieldAccess(LoadOrStore mode, Handle<Map> map,
                                   Handle<Name> name, FieldAccess* access) {
  access->base_is_tagged = kTaggedBase;
  access->offset = -1;
  access->name = name;
  access->type = Type::Any();
  access->machine_type = kMachAnyTagged;

  // Check for properties that have accessors but are JSObject fields.
  if (Accessors::IsJSObjectFieldAccessor(map, name, &access->offset)) {
    // TODO(turbofan): fill in types for special JSObject field accesses.
    return true;
  }

  // Check if the map is a dictionary.
  if (map->is_dictionary_map()) return false;

  // Search the descriptor array.
  DescriptorArray* descriptors = map->instance_descriptors();
  int number = descriptors->SearchWithCache(*name, *map);
  if (number == DescriptorArray::kNotFound) return false;
  PropertyDetails property_details = descriptors->GetDetails(number);

  bool is_smi = property_details.representation().IsSmi();
  bool is_double = property_details.representation().IsDouble();

  if (property_details.type() != DATA) {
    // TODO(turbofan): constant loads and stores.
    return false;
  }

  // Transfer known types from property details.
  AddFieldAccessTypes(access, property_details);

  if (mode == STORE) {
    if (property_details.IsReadOnly()) {
      // TODO(turbofan): deopt, ignore or throw on readonly stores.
      return false;
    }
    if (is_smi || is_double) {
      // TODO(turbofan): check type and deopt for SMI/double stores.
      return false;
    }
  }

  int index = map->instance_descriptors()->GetFieldIndex(number);
  FieldIndex field_index = FieldIndex::ForPropertyIndex(*map, index, is_double);

  if (field_index.is_inobject()) {
    access->offset = field_index.offset();
    return true;
  }

  // TODO(turbofan): handle out of object properties.
  return false;
}


static bool IsGlobalObject(Node* node) {
  return NodeProperties::IsTyped(node) &&
         NodeProperties::GetBounds(node).upper->Is(Type::GlobalObject());
}


Reduction JSTypeFeedbackSpecializer::ReduceJSLoadNamed(Node* node) {
  DCHECK(node->opcode() == IrOpcode::kJSLoadNamed);
  Node* receiver = node->InputAt(0);
  if (IsGlobalObject(receiver)) {
    return ReduceJSLoadNamedForGlobalVariable(node);
  }

  if (!FLAG_turbo_deoptimization) return NoChange();
  // TODO(titzer): deopt locations are wrong for property accesses
  if (!EAGER_DEOPT_LOCATIONS_FOR_PROPERTY_ACCESS_ARE_CORRECT) return NoChange();

  // TODO(turbofan): handle vector-based type feedback.
  TypeFeedbackId id = js_type_feedback_->find(node);
  if (id.IsNone() || oracle()->LoadInlineCacheState(id) == UNINITIALIZED) {
    return NoChange();
  }

  const LoadNamedParameters& p = LoadNamedParametersOf(node->op());
  SmallMapList maps;
  Handle<Name> name = p.name().handle();
  Node* effect = NodeProperties::GetEffectInput(node);
  GatherReceiverTypes(receiver, effect, id, name, &maps);

  if (maps.length() != 1) return NoChange();  // TODO(turbofan): polymorphism

  Handle<Map> map = maps.first();
  FieldAccess field_access;
  if (!GetInObjectFieldAccess(LOAD, map, name, &field_access)) {
    return NoChange();
  }

  Node* control = NodeProperties::GetControlInput(node);
  Node* check_success;
  Node* check_failed;
  BuildMapCheck(receiver, map, true, effect, control, &check_success,
                &check_failed);

  // Build the actual load.
  Node* load = graph()->NewNode(simplified()->LoadField(field_access), receiver,
                                effect, check_success);

  // TODO(turbofan): handle slow case instead of deoptimizing.
  // TODO(titzer): frame state should be from before the load.
  Node* frame_state = NodeProperties::GetFrameStateInput(node, 0);
  Node* deopt = graph()->NewNode(common()->Deoptimize(), frame_state, effect,
                                 check_failed);
  NodeProperties::MergeControlToEnd(graph(), common(), deopt);
  NodeProperties::ReplaceWithValue(node, load, load, check_success);
  return Replace(load);
}


Reduction JSTypeFeedbackSpecializer::ReduceJSLoadNamedForGlobalVariable(
    Node* node) {
  Handle<String> name =
      Handle<String>::cast(LoadNamedParametersOf(node->op()).name().handle());
  // Try to optimize loads from the global object.
  Handle<Object> constant_value =
      jsgraph()->isolate()->factory()->GlobalConstantFor(name);
  if (!constant_value.is_null()) {
    // Always optimize global constants.
    Node* constant = jsgraph()->Constant(constant_value);
    NodeProperties::ReplaceWithValue(node, constant);
    return Replace(constant);
  }

  if (global_object_.is_null()) {
    // Nothing else can be done if we don't have a global object.
    return NoChange();
  }

  if (FLAG_turbo_deoptimization) {
    // Handle lookups in the script context.
    {
      Handle<ScriptContextTable> script_contexts(
          global_object_->native_context()->script_context_table());
      ScriptContextTable::LookupResult lookup;
      if (ScriptContextTable::Lookup(script_contexts, name, &lookup)) {
        // TODO(turbofan): introduce a LoadContext here.
        return NoChange();
      }
    }

    // Constant promotion or cell access requires lazy deoptimization support.
    LookupIterator it(global_object_, name, LookupIterator::OWN);

    if (it.state() == LookupIterator::DATA) {
      Handle<PropertyCell> cell = it.GetPropertyCell();
      dependencies_->AssumePropertyCell(cell);

      if (it.property_details().cell_type() == PropertyCellType::kConstant) {
        // Constant promote the global's current value.
        Handle<Object> constant_value(cell->value(), jsgraph()->isolate());
        if (constant_value->IsConsString()) {
          constant_value =
              String::Flatten(Handle<String>::cast(constant_value));
        }
        Node* constant = jsgraph()->Constant(constant_value);
        NodeProperties::ReplaceWithValue(node, constant);
        return Replace(constant);
      } else {
        // Load directly from the property cell.
        FieldAccess access = AccessBuilder::ForPropertyCellValue();
        Node* control = NodeProperties::GetControlInput(node);
        Node* load_field = graph()->NewNode(
            simplified()->LoadField(access), jsgraph()->Constant(cell),
            NodeProperties::GetEffectInput(node), control);
        NodeProperties::ReplaceWithValue(node, load_field, load_field, control);
        return Replace(load_field);
      }
    }
  } else {
    // TODO(turbofan): non-configurable properties on the global object
    // should be loadable through a cell without deoptimization support.
  }

  return NoChange();
}


Reduction JSTypeFeedbackSpecializer::ReduceJSLoadProperty(Node* node) {
  return NoChange();
}


Reduction JSTypeFeedbackSpecializer::ReduceJSStoreNamed(Node* node) {
  DCHECK(node->opcode() == IrOpcode::kJSStoreNamed);
  // TODO(titzer): deopt locations are wrong for property accesses
  if (!EAGER_DEOPT_LOCATIONS_FOR_PROPERTY_ACCESS_ARE_CORRECT) return NoChange();

  TypeFeedbackId id = js_type_feedback_->find(node);
  if (id.IsNone() || oracle()->StoreIsUninitialized(id)) return NoChange();

  const StoreNamedParameters& p = StoreNamedParametersOf(node->op());
  SmallMapList maps;
  Handle<Name> name = p.name().handle();
  Node* receiver = node->InputAt(0);
  Node* effect = NodeProperties::GetEffectInput(node);
  GatherReceiverTypes(receiver, effect, id, name, &maps);

  if (maps.length() != 1) return NoChange();  // TODO(turbofan): polymorphism

  Handle<Map> map = maps.first();
  FieldAccess field_access;
  if (!GetInObjectFieldAccess(STORE, map, name, &field_access)) {
    return NoChange();
  }

  Node* control = NodeProperties::GetControlInput(node);
  Node* check_success;
  Node* check_failed;
  BuildMapCheck(receiver, map, true, effect, control, &check_success,
                &check_failed);

  // Build the actual load.
  Node* value = node->InputAt(1);
  Node* store = graph()->NewNode(simplified()->StoreField(field_access),
                                 receiver, value, effect, check_success);

  // TODO(turbofan): handle slow case instead of deoptimizing.
  // TODO(titzer): frame state should be from before the store.
  Node* frame_state = NodeProperties::GetFrameStateInput(node, 0);
  Node* deopt = graph()->NewNode(common()->Deoptimize(), frame_state, effect,
                                 check_failed);
  NodeProperties::MergeControlToEnd(graph(), common(), deopt);
  NodeProperties::ReplaceWithValue(node, store, store, check_success);
  return Replace(store);
}


Reduction JSTypeFeedbackSpecializer::ReduceJSStoreProperty(Node* node) {
  return NoChange();
}


void JSTypeFeedbackSpecializer::BuildMapCheck(Node* receiver, Handle<Map> map,
                                              bool smi_check, Node* effect,
                                              Node* control, Node** success,
                                              Node** fail) {
  Node* if_smi = nullptr;
  if (smi_check) {
    Node* branch_smi = graph()->NewNode(
        common()->Branch(BranchHint::kFalse),
        graph()->NewNode(simplified()->ObjectIsSmi(), receiver), control);
    if_smi = graph()->NewNode(common()->IfTrue(), branch_smi);
    control = graph()->NewNode(common()->IfFalse(), branch_smi);
  }

  FieldAccess map_access = AccessBuilder::ForMap();
  Node* receiver_map = graph()->NewNode(simplified()->LoadField(map_access),
                                        receiver, effect, control);
  Node* map_const = jsgraph_->Constant(map);
  Node* cmp = graph()->NewNode(simplified()->ReferenceEqual(Type::Internal()),
                               receiver_map, map_const);
  Node* branch =
      graph()->NewNode(common()->Branch(BranchHint::kTrue), cmp, control);
  *success = graph()->NewNode(common()->IfTrue(), branch);
  *fail = graph()->NewNode(common()->IfFalse(), branch);

  if (if_smi) {
    *fail = graph()->NewNode(common()->Merge(2), *fail, if_smi);
  }
}


void JSTypeFeedbackSpecializer::GatherReceiverTypes(Node* receiver,
                                                    Node* effect,
                                                    TypeFeedbackId id,
                                                    Handle<Name> name,
                                                    SmallMapList* maps) {
  // TODO(turbofan): filter maps by initial receiver map if known
  // TODO(turbofan): filter maps by native context (if specializing)
  // TODO(turbofan): filter maps by effect chain
  oracle()->PropertyReceiverTypes(id, name, maps);
}


}  // namespace compiler
}  // namespace internal
}  // namespace v8
