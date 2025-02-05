// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/const-tracking-let-helpers.h"

#include "src/compiler/access-builder.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/node.h"
#include "src/compiler/simplified-operator.h"
#include "src/objects/property-cell.h"

namespace v8::internal::compiler {

int ConstTrackingLetSideDataIndexForAccess(size_t access_index) {
  return static_cast<int>(access_index) - Context::MIN_CONTEXT_EXTENDED_SLOTS;
}

void GenerateCheckConstTrackingLetSideData(Node* context, Node** effect,
                                           Node** control, int side_data_index,
                                           JSGraph* jsgraph) {
  // Load side table.
  Node* side_data = *effect = jsgraph->graph()->NewNode(
      jsgraph->simplified()->LoadField(AccessBuilder::ForContextSlot(
          Context::CONTEXT_SIDE_TABLE_PROPERTY_INDEX)),
      context, *effect, *control);
  Node* side_data_value = *effect = jsgraph->graph()->NewNode(
      jsgraph->simplified()->LoadField(
          AccessBuilder::ForFixedArraySlot(side_data_index)),
      side_data, *effect, *control);

  // TODO(v8:13567): If the value is the same as the value we already have, we
  // don't need to deopt.

  // Check if data is a smi
  Node* is_data_smi = jsgraph->graph()->NewNode(
      jsgraph->simplified()->ObjectIsSmi(), side_data_value);
  Node* branch = jsgraph->graph()->NewNode(jsgraph->common()->Branch(),
                                           is_data_smi, *control);

  Node* if_true =
      jsgraph->graph()->NewNode(jsgraph->common()->IfTrue(), branch);
  Node* etrue = *effect;

  Node* property_from_cell;
  Node* if_false =
      jsgraph->graph()->NewNode(jsgraph->common()->IfFalse(), branch);
  Node* efalse = *effect;
  {
    // Check if is the undefined value, deopt if it is.
    Node* check_is_not_undefined = jsgraph->graph()->NewNode(
        jsgraph->simplified()->BooleanNot(),
        jsgraph->graph()->NewNode(jsgraph->simplified()->ReferenceEqual(),
                                  side_data_value,
                                  jsgraph->UndefinedConstant()));
    efalse = jsgraph->graph()->NewNode(
        jsgraph->simplified()->CheckIf(DeoptimizeReason::kConstTrackingLet),
        check_is_not_undefined, efalse, if_false);
    // It must then been a ContextSidePropertyCell.
    // Load property from it.
    property_from_cell = efalse =
        jsgraph->graph()->NewNode(jsgraph->simplified()->LoadField(
                                      AccessBuilder::ForContextSideProperty()),
                                  side_data_value, efalse, if_false);
  }

  // Merge.
  *control =
      jsgraph->graph()->NewNode(jsgraph->common()->Merge(2), if_true, if_false);
  *effect = jsgraph->graph()->NewNode(jsgraph->common()->EffectPhi(2), etrue,
                                      efalse, *control);
  Node* property = jsgraph->graph()->NewNode(
      jsgraph->common()->Phi(MachineRepresentation::kTagged, 2),
      side_data_value, property_from_cell, *control);

  // Check that property is not const, deopt otherwise.
  Node* check = jsgraph->graph()->NewNode(
      jsgraph->simplified()->BooleanNot(),
      jsgraph->graph()->NewNode(
          jsgraph->simplified()->ReferenceEqual(), property,
          jsgraph->SmiConstant(ContextSidePropertyCell::kConst)));
  *effect = jsgraph->graph()->NewNode(
      jsgraph->simplified()->CheckIf(DeoptimizeReason::kConstTrackingLet),
      check, *effect, *control);
}

bool IsConstTrackingLetVariableSurelyNotConstant(
    OptionalContextRef maybe_context, size_t depth, int side_data_index,
    JSHeapBroker* broker) {
  if (maybe_context.has_value() && depth == 0) {
    ContextRef context = maybe_context.value();
    OptionalObjectRef side_data =
        context.get(broker, Context::CONTEXT_SIDE_TABLE_PROPERTY_INDEX);
    if (side_data.has_value()) {
      OptionalObjectRef side_data_value =
          side_data->AsFixedArray().TryGet(broker, side_data_index);
      if (side_data_value.has_value()) {
        auto value = side_data_value.value();
        if (value.IsSmi() && value.AsSmi() != ContextSidePropertyCell::kConst) {
          // The value is not a constant any more.
          return true;
        }
      }
    }
  }
  // Either the value is not a constant, or we don't know.
  return false;
}

}  // namespace v8::internal::compiler
