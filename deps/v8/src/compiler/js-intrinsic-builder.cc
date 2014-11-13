// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/access-builder.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/diamond.h"
#include "src/compiler/generic-node-inl.h"
#include "src/compiler/js-intrinsic-builder.h"
#include "src/compiler/js-operator.h"
#include "src/compiler/simplified-operator.h"


namespace v8 {
namespace internal {
namespace compiler {

ResultAndEffect JSIntrinsicBuilder::BuildGraphFor(Runtime::FunctionId id,
                                                  const NodeVector& arguments) {
  switch (id) {
    case Runtime::kInlineIsSmi:
      return BuildGraphFor_IsSmi(arguments);
    case Runtime::kInlineIsNonNegativeSmi:
      return BuildGraphFor_IsNonNegativeSmi(arguments);
    case Runtime::kInlineIsArray:
      return BuildMapCheck(arguments[0], arguments[2], JS_ARRAY_TYPE);
    case Runtime::kInlineIsRegExp:
      return BuildMapCheck(arguments[0], arguments[2], JS_REGEXP_TYPE);
    case Runtime::kInlineIsFunction:
      return BuildMapCheck(arguments[0], arguments[2], JS_FUNCTION_TYPE);
    case Runtime::kInlineValueOf:
      return BuildGraphFor_ValueOf(arguments);
    default:
      break;
  }
  return ResultAndEffect();
}

ResultAndEffect JSIntrinsicBuilder::BuildGraphFor_IsSmi(
    const NodeVector& arguments) {
  Node* object = arguments[0];
  SimplifiedOperatorBuilder simplified(jsgraph_->zone());
  Node* condition = graph()->NewNode(simplified.ObjectIsSmi(), object);

  return ResultAndEffect(condition, arguments[2]);
}


ResultAndEffect JSIntrinsicBuilder::BuildGraphFor_IsNonNegativeSmi(
    const NodeVector& arguments) {
  Node* object = arguments[0];
  SimplifiedOperatorBuilder simplified(jsgraph_->zone());
  Node* condition =
      graph()->NewNode(simplified.ObjectIsNonNegativeSmi(), object);

  return ResultAndEffect(condition, arguments[2]);
}


/*
 * if (_isSmi(object)) {
 *   return false
 * } else {
 *   return %_GetMapInstanceType(object) == map_type
 * }
 */
ResultAndEffect JSIntrinsicBuilder::BuildMapCheck(Node* object, Node* effect,
                                                  InstanceType map_type) {
  SimplifiedOperatorBuilder simplified(jsgraph_->zone());

  Node* is_smi = graph()->NewNode(simplified.ObjectIsSmi(), object);
  Diamond d(graph(), common(), is_smi);

  Node* map = graph()->NewNode(simplified.LoadField(AccessBuilder::ForMap()),
                               object, effect, d.if_false);

  Node* instance_type = graph()->NewNode(
      simplified.LoadField(AccessBuilder::ForMapInstanceType()), map, map,
      d.if_false);

  Node* has_map_type =
      graph()->NewNode(jsgraph_->machine()->Word32Equal(), instance_type,
                       jsgraph_->Int32Constant(map_type));

  Node* phi = d.Phi(static_cast<MachineType>(kTypeBool | kRepTagged),
                    jsgraph_->FalseConstant(), has_map_type);

  Node* ephi = d.EffectPhi(effect, instance_type);

  return ResultAndEffect(phi, ephi);
}


/*
 * if (%_isSmi(object)) {
 *   return object;
 * } else if (%_GetMapInstanceType(object) == JS_VALUE_TYPE) {
 *   return %_LoadValueField(object);
 * } else {
 *   return object;
 * }
 */
ResultAndEffect JSIntrinsicBuilder::BuildGraphFor_ValueOf(
    const NodeVector& arguments) {
  Node* object = arguments[0];
  Node* effect = arguments[2];
  SimplifiedOperatorBuilder simplified(jsgraph_->zone());

  Node* is_smi = graph()->NewNode(simplified.ObjectIsSmi(), object);

  Diamond if_is_smi(graph(), common(), is_smi);

  Node* map = graph()->NewNode(simplified.LoadField(AccessBuilder::ForMap()),
                               object, effect, if_is_smi.if_false);

  Node* instance_type = graph()->NewNode(
      simplified.LoadField(AccessBuilder::ForMapInstanceType()), map, map,
      if_is_smi.if_false);

  Node* is_value =
      graph()->NewNode(jsgraph_->machine()->Word32Equal(), instance_type,
                       jsgraph_->Constant(JS_VALUE_TYPE));

  Diamond if_is_value(graph(), common(), is_value);
  if_is_value.Nest(if_is_smi, false);

  Node* value =
      graph()->NewNode(simplified.LoadField(AccessBuilder::ForValue()), object,
                       instance_type, if_is_value.if_true);

  Node* phi_is_value = if_is_value.Phi(kTypeAny, value, object);

  Node* phi = if_is_smi.Phi(kTypeAny, object, phi_is_value);

  Node* ephi = if_is_smi.EffectPhi(effect, instance_type);

  return ResultAndEffect(phi, ephi);
}
}
}
}  // namespace v8::internal::compiler
