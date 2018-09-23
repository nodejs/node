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
#include "src/compiler/frame-states.h"
#include "src/compiler/node-aux-data.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/operator-properties.h"
#include "src/compiler/simplified-operator.h"

namespace v8 {
namespace internal {
namespace compiler {

enum LoadOrStore { LOAD, STORE };

// TODO(turbofan): fix deoptimization problems
#define ENABLE_FAST_PROPERTY_LOADS false
#define ENABLE_FAST_PROPERTY_STORES false

JSTypeFeedbackTable::JSTypeFeedbackTable(Zone* zone)
    : type_feedback_id_map_(TypeFeedbackIdMap::key_compare(),
                            TypeFeedbackIdMap::allocator_type(zone)),
      feedback_vector_ic_slot_map_(TypeFeedbackIdMap::key_compare(),
                                   TypeFeedbackIdMap::allocator_type(zone)) {}


void JSTypeFeedbackTable::Record(Node* node, TypeFeedbackId id) {
  type_feedback_id_map_.insert(std::make_pair(node->id(), id));
}


void JSTypeFeedbackTable::Record(Node* node, FeedbackVectorICSlot slot) {
  feedback_vector_ic_slot_map_.insert(std::make_pair(node->id(), slot));
}


Reduction JSTypeFeedbackSpecializer::Reduce(Node* node) {
  switch (node->opcode()) {
    case IrOpcode::kJSLoadProperty:
      return ReduceJSLoadProperty(node);
    case IrOpcode::kJSLoadNamed:
      return ReduceJSLoadNamed(node);
    case IrOpcode::kJSLoadGlobal:
      return ReduceJSLoadGlobal(node);
    case IrOpcode::kJSStoreNamed:
      return ReduceJSStoreNamed(node);
    case IrOpcode::kJSStoreProperty:
      return ReduceJSStoreProperty(node);
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
    if (is_double && !map->IsUnboxedDoubleField(field_index)) {
      // TODO(turbofan): support for out-of-line (MutableHeapNumber) loads.
      return false;
    }
    access->offset = field_index.offset();
    return true;
  }

  // TODO(turbofan): handle out of object properties.
  return false;
}


Reduction JSTypeFeedbackSpecializer::ReduceJSLoadNamed(Node* node) {
  DCHECK(node->opcode() == IrOpcode::kJSLoadNamed);
  if (mode() != kDeoptimizationEnabled) return NoChange();
  Node* frame_state_before = GetFrameStateBefore(node);
  if (frame_state_before == nullptr) return NoChange();

  const LoadNamedParameters& p = LoadNamedParametersOf(node->op());
  Handle<Name> name = p.name().handle();
  SmallMapList maps;

  FeedbackVectorICSlot slot = js_type_feedback_->FindFeedbackVectorICSlot(node);
  if (slot.IsInvalid() ||
      oracle()->LoadInlineCacheState(slot) == UNINITIALIZED) {
    // No type feedback ids or the load is uninitialized.
    return NoChange();
  }
  oracle()->PropertyReceiverTypes(slot, name, &maps);

  Node* receiver = node->InputAt(0);
  Node* effect = NodeProperties::GetEffectInput(node);

  if (maps.length() != 1) return NoChange();  // TODO(turbofan): polymorphism
  if (!ENABLE_FAST_PROPERTY_LOADS) return NoChange();

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
  Node* deopt = graph()->NewNode(common()->Deoptimize(), frame_state_before,
                                 effect, check_failed);
  NodeProperties::MergeControlToEnd(graph(), common(), deopt);
  ReplaceWithValue(node, load, load, check_success);
  return Replace(load);
}


Reduction JSTypeFeedbackSpecializer::ReduceJSLoadGlobal(Node* node) {
  DCHECK(node->opcode() == IrOpcode::kJSLoadGlobal);
  Handle<String> name =
      Handle<String>::cast(LoadGlobalParametersOf(node->op()).name().handle());
  // Try to optimize loads from the global object.
  Handle<Object> constant_value =
      jsgraph()->isolate()->factory()->GlobalConstantFor(name);
  if (!constant_value.is_null()) {
    // Always optimize global constants.
    Node* constant = jsgraph()->Constant(constant_value);
    ReplaceWithValue(node, constant);
    return Replace(constant);
  }

  if (global_object_.is_null()) {
    // Nothing else can be done if we don't have a global object.
    return NoChange();
  }

  if (mode() == kDeoptimizationEnabled) {
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
        ReplaceWithValue(node, constant);
        return Replace(constant);
      } else {
        // Load directly from the property cell.
        FieldAccess access = AccessBuilder::ForPropertyCellValue();
        Node* control = NodeProperties::GetControlInput(node);
        Node* load_field = graph()->NewNode(
            simplified()->LoadField(access), jsgraph()->Constant(cell),
            NodeProperties::GetEffectInput(node), control);
        ReplaceWithValue(node, load_field, load_field, control);
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
  Node* frame_state_before = GetFrameStateBefore(node);
  if (frame_state_before == nullptr) return NoChange();

  const StoreNamedParameters& p = StoreNamedParametersOf(node->op());
  Handle<Name> name = p.name().handle();
  SmallMapList maps;
  TypeFeedbackId id = js_type_feedback_->FindTypeFeedbackId(node);
  if (id.IsNone() || oracle()->StoreIsUninitialized(id) == UNINITIALIZED) {
    // No type feedback ids or the store is uninitialized.
    // TODO(titzer): no feedback from vector ICs from stores.
    return NoChange();
  } else {
    oracle()->AssignmentReceiverTypes(id, name, &maps);
  }

  Node* receiver = node->InputAt(0);
  Node* effect = NodeProperties::GetEffectInput(node);

  if (maps.length() != 1) return NoChange();  // TODO(turbofan): polymorphism

  if (!ENABLE_FAST_PROPERTY_STORES) return NoChange();

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
  Node* deopt = graph()->NewNode(common()->Deoptimize(), frame_state_before,
                                 effect, check_failed);
  NodeProperties::MergeControlToEnd(graph(), common(), deopt);
  ReplaceWithValue(node, store, store, check_success);
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


// Get the frame state before an operation if it exists and has a valid
// bailout id.
Node* JSTypeFeedbackSpecializer::GetFrameStateBefore(Node* node) {
  int count = OperatorProperties::GetFrameStateInputCount(node->op());
  DCHECK_LE(count, 2);
  if (count == 2) {
    Node* frame_state = NodeProperties::GetFrameStateInput(node, 1);
    if (frame_state->opcode() == IrOpcode::kFrameState) {
      BailoutId id = OpParameter<FrameStateInfo>(node).bailout_id();
      if (id != BailoutId::None()) return frame_state;
    }
  }
  return nullptr;
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
