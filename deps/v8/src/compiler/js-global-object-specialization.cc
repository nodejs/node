// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-global-object-specialization.h"

#include "src/compilation-dependencies.h"
#include "src/compiler/access-builder.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/js-operator.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/simplified-operator.h"
#include "src/compiler/type-cache.h"
#include "src/lookup.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

struct JSGlobalObjectSpecialization::ScriptContextTableLookupResult {
  Handle<Context> context;
  bool immutable;
  int index;
};

JSGlobalObjectSpecialization::JSGlobalObjectSpecialization(
    Editor* editor, JSGraph* jsgraph, Handle<JSGlobalObject> global_object,
    CompilationDependencies* dependencies)
    : AdvancedReducer(editor),
      jsgraph_(jsgraph),
      global_object_(global_object),
      dependencies_(dependencies),
      type_cache_(TypeCache::Get()) {}

Reduction JSGlobalObjectSpecialization::Reduce(Node* node) {
  switch (node->opcode()) {
    case IrOpcode::kJSLoadGlobal:
      return ReduceJSLoadGlobal(node);
    case IrOpcode::kJSStoreGlobal:
      return ReduceJSStoreGlobal(node);
    default:
      break;
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

Reduction JSGlobalObjectSpecialization::ReduceJSLoadGlobal(Node* node) {
  DCHECK_EQ(IrOpcode::kJSLoadGlobal, node->opcode());
  Handle<Name> name = LoadGlobalParametersOf(node->op()).name();
  Node* effect = NodeProperties::GetEffectInput(node);
  Node* control = NodeProperties::GetControlInput(node);

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

  // Lookup on the global object instead.  We only deal with own data
  // properties of the global object here (represented as PropertyCell).
  LookupIterator it(global_object(), name, LookupIterator::OWN);
  if (it.state() != LookupIterator::DATA) return NoChange();
  if (!it.GetHolder<JSObject>()->IsJSGlobalObject()) return NoChange();
  Handle<PropertyCell> property_cell = it.GetPropertyCell();
  PropertyDetails property_details = property_cell->property_details();
  Handle<Object> property_cell_value(property_cell->value(), isolate());

  // Load from non-configurable, read-only data property on the global
  // object can be constant-folded, even without deoptimization support.
  if (!property_details.IsConfigurable() && property_details.IsReadOnly()) {
    Node* value = jsgraph()->Constant(property_cell_value);
    ReplaceWithValue(node, value);
    return Replace(value);
  }

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
    Node* value = jsgraph()->Constant(property_cell_value);
    ReplaceWithValue(node, value);
    return Replace(value);
  }

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
          Handle<HeapObject>::cast(property_cell_value)->map(), isolate());
      property_cell_value_type = Type::For(property_cell_value_map);
      representation = MachineRepresentation::kTaggedPointer;

      // We can only use the property cell value map for map check elimination
      // if it's stable, i.e. the HeapObject wasn't mutated without the cell
      // state being updated.
      if (property_cell_value_map->is_stable()) {
        dependencies()->AssumeMapStable(property_cell_value_map);
        map = property_cell_value_map;
      }
    }
  }
  Node* value = effect = graph()->NewNode(
      simplified()->LoadField(ForPropertyCellValue(
          representation, property_cell_value_type, map, name)),
      jsgraph()->HeapConstant(property_cell), effect, control);
  ReplaceWithValue(node, value, effect, control);
  return Replace(value);
}


Reduction JSGlobalObjectSpecialization::ReduceJSStoreGlobal(Node* node) {
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

  // Lookup on the global object instead.  We only deal with own data
  // properties of the global object here (represented as PropertyCell).
  LookupIterator it(global_object(), name, LookupIterator::OWN);
  if (it.state() != LookupIterator::DATA) return NoChange();
  if (!it.GetHolder<JSObject>()->IsJSGlobalObject()) return NoChange();
  Handle<PropertyCell> property_cell = it.GetPropertyCell();
  PropertyDetails property_details = property_cell->property_details();
  Handle<Object> property_cell_value(property_cell->value(), isolate());

  // Don't even bother trying to lower stores to read-only data properties.
  if (property_details.IsReadOnly()) return NoChange();
  switch (property_details.cell_type()) {
    case PropertyCellType::kUndefined: {
      return NoChange();
    }
    case PropertyCellType::kConstant: {
      // Record a code dependency on the cell, and just deoptimize if the new
      // value doesn't match the previous value stored inside the cell.
      dependencies()->AssumePropertyCell(property_cell);
      Node* check = graph()->NewNode(simplified()->ReferenceEqual(), value,
                                     jsgraph()->Constant(property_cell_value));
      effect =
          graph()->NewNode(simplified()->CheckIf(), check, effect, control);
      break;
    }
    case PropertyCellType::kConstantType: {
      // Record a code dependency on the cell, and just deoptimize if the new
      // values' type doesn't match the type of the previous value in the cell.
      dependencies()->AssumePropertyCell(property_cell);
      Type* property_cell_value_type;
      MachineRepresentation representation = MachineRepresentation::kTagged;
      if (property_cell_value->IsHeapObject()) {
        // We cannot do anything if the {property_cell_value}s map is no
        // longer stable.
        Handle<Map> property_cell_value_map(
            Handle<HeapObject>::cast(property_cell_value)->map(), isolate());
        if (!property_cell_value_map->is_stable()) return NoChange();
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
        value = effect =
            graph()->NewNode(simplified()->CheckSmi(), value, effect, control);
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
  ReplaceWithValue(node, value, effect, control);
  return Replace(value);
}

bool JSGlobalObjectSpecialization::LookupInScriptContextTable(
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

Graph* JSGlobalObjectSpecialization::graph() const {
  return jsgraph()->graph();
}

Isolate* JSGlobalObjectSpecialization::isolate() const {
  return jsgraph()->isolate();
}

CommonOperatorBuilder* JSGlobalObjectSpecialization::common() const {
  return jsgraph()->common();
}

JSOperatorBuilder* JSGlobalObjectSpecialization::javascript() const {
  return jsgraph()->javascript();
}

SimplifiedOperatorBuilder* JSGlobalObjectSpecialization::simplified() const {
  return jsgraph()->simplified();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
