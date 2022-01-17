// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-heap-copy-reducer.h"

#include "src/compiler/common-operator.h"
#include "src/compiler/js-heap-broker.h"
#include "src/compiler/js-operator.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/simplified-operator.h"
#include "src/heap/factory-inl.h"
#include "src/objects/map.h"
#include "src/objects/scope-info.h"
#include "src/objects/template-objects.h"

namespace v8 {
namespace internal {
namespace compiler {

// In the functions below, we call the ObjectRef (or subclass) constructor in
// order to trigger serialization if not yet done.

JSHeapCopyReducer::JSHeapCopyReducer(JSHeapBroker* broker) : broker_(broker) {}

JSHeapBroker* JSHeapCopyReducer::broker() { return broker_; }

Reduction JSHeapCopyReducer::Reduce(Node* node) {
  switch (node->opcode()) {
    case IrOpcode::kHeapConstant: {
      ObjectRef object = MakeRef(broker(), HeapConstantOf(node->op()));
      if (object.IsJSObject()) {
        object.AsJSObject().SerializeObjectCreateMap(
            NotConcurrentInliningTag{broker()});
      }
      break;
    }
    case IrOpcode::kJSCreateArray: {
      CreateArrayParametersOf(node->op()).site(broker());
      break;
    }
    case IrOpcode::kJSCreateArguments: {
      Node* const frame_state = NodeProperties::GetFrameStateInput(node);
      FrameStateInfo state_info = FrameStateInfoOf(frame_state->op());
      MakeRef(broker(), state_info.shared_info().ToHandleChecked());
      break;
    }
    case IrOpcode::kJSCreateBlockContext: {
      USE(ScopeInfoOf(broker(), node->op()));
      break;
    }
    case IrOpcode::kJSCreateBoundFunction: {
      CreateBoundFunctionParameters const& p =
          CreateBoundFunctionParametersOf(node->op());
      p.map(broker());
      break;
    }
    case IrOpcode::kJSCreateCatchContext: {
      USE(ScopeInfoOf(broker(), node->op()));
      break;
    }
    case IrOpcode::kJSCreateClosure: {
      CreateClosureParameters const& p = CreateClosureParametersOf(node->op());
      p.shared_info(broker());
      p.code(broker());
      break;
    }
    case IrOpcode::kJSCreateEmptyLiteralArray: {
      FeedbackParameter const& p = FeedbackParameterOf(node->op());
      if (p.feedback().IsValid()) {
        broker()->GetFeedbackForArrayOrObjectLiteral(p.feedback());
      }
      break;
    }
    /* Unary ops. */
    case IrOpcode::kJSBitwiseNot:
    case IrOpcode::kJSDecrement:
    case IrOpcode::kJSIncrement:
    case IrOpcode::kJSNegate: {
      FeedbackParameter const& p = FeedbackParameterOf(node->op());
      if (p.feedback().IsValid()) {
        // Unary ops are treated as binary ops with respect to feedback.
        broker()->GetFeedbackForBinaryOperation(p.feedback());
      }
      break;
    }
    /* Binary ops. */
    case IrOpcode::kJSAdd:
    case IrOpcode::kJSSubtract:
    case IrOpcode::kJSMultiply:
    case IrOpcode::kJSDivide:
    case IrOpcode::kJSModulus:
    case IrOpcode::kJSExponentiate:
    case IrOpcode::kJSBitwiseOr:
    case IrOpcode::kJSBitwiseXor:
    case IrOpcode::kJSBitwiseAnd:
    case IrOpcode::kJSShiftLeft:
    case IrOpcode::kJSShiftRight:
    case IrOpcode::kJSShiftRightLogical: {
      FeedbackParameter const& p = FeedbackParameterOf(node->op());
      if (p.feedback().IsValid()) {
        broker()->GetFeedbackForBinaryOperation(p.feedback());
      }
      break;
    }
    /* Compare ops. */
    case IrOpcode::kJSEqual:
    case IrOpcode::kJSGreaterThan:
    case IrOpcode::kJSGreaterThanOrEqual:
    case IrOpcode::kJSLessThan:
    case IrOpcode::kJSLessThanOrEqual:
    case IrOpcode::kJSStrictEqual: {
      FeedbackParameter const& p = FeedbackParameterOf(node->op());
      if (p.feedback().IsValid()) {
        broker()->GetFeedbackForCompareOperation(p.feedback());
      }
      break;
    }
    case IrOpcode::kJSCreateFunctionContext: {
      CreateFunctionContextParameters const& p =
          CreateFunctionContextParametersOf(node->op());
      p.scope_info(broker());
      break;
    }
    case IrOpcode::kJSCreateLiteralArray:
    case IrOpcode::kJSCreateLiteralObject: {
      CreateLiteralParameters const& p = CreateLiteralParametersOf(node->op());
      if (p.feedback().IsValid()) {
        broker()->GetFeedbackForArrayOrObjectLiteral(p.feedback());
      }
      break;
    }
    case IrOpcode::kJSCreateLiteralRegExp: {
      CreateLiteralParameters const& p = CreateLiteralParametersOf(node->op());
      if (p.feedback().IsValid()) {
        broker()->GetFeedbackForRegExpLiteral(p.feedback());
      }
      break;
    }
    case IrOpcode::kJSGetTemplateObject: {
      GetTemplateObjectParameters const& p =
          GetTemplateObjectParametersOf(node->op());
      p.shared(broker());
      p.description(broker());
      broker()->GetFeedbackForTemplateObject(p.feedback());
      break;
    }
    case IrOpcode::kJSCreateWithContext: {
      USE(ScopeInfoOf(broker(), node->op()));
      break;
    }
    case IrOpcode::kJSLoadNamed: {
      NamedAccess const& p = NamedAccessOf(node->op());
      NameRef name = p.name(broker());
      if (p.feedback().IsValid()) {
        broker()->GetFeedbackForPropertyAccess(p.feedback(), AccessMode::kLoad,
                                               name);
      }
      break;
    }
    case IrOpcode::kJSLoadNamedFromSuper: {
      NamedAccess const& p = NamedAccessOf(node->op());
      NameRef name = p.name(broker());
      if (p.feedback().IsValid()) {
        broker()->GetFeedbackForPropertyAccess(p.feedback(), AccessMode::kLoad,
                                               name);
      }
      break;
    }
    case IrOpcode::kJSStoreNamed: {
      NamedAccess const& p = NamedAccessOf(node->op());
      p.name(broker());
      break;
    }
    case IrOpcode::kStoreField:
    case IrOpcode::kLoadField: {
      FieldAccess access = FieldAccessOf(node->op());
      Handle<Map> map_handle;
      if (access.map.ToHandle(&map_handle)) {
        MakeRef(broker(), map_handle);
      }
      Handle<Name> name_handle;
      if (access.name.ToHandle(&name_handle)) {
        MakeRef(broker(), name_handle);
      }
      break;
    }
    case IrOpcode::kMapGuard: {
      ZoneHandleSet<Map> const& maps = MapGuardMapsOf(node->op());
      for (Handle<Map> map : maps) {
        MakeRef(broker(), map);
      }
      break;
    }
    case IrOpcode::kCheckMaps: {
      ZoneHandleSet<Map> const& maps = CheckMapsParametersOf(node->op()).maps();
      for (Handle<Map> map : maps) {
        MakeRef(broker(), map);
      }
      break;
    }
    case IrOpcode::kCompareMaps: {
      ZoneHandleSet<Map> const& maps = CompareMapsParametersOf(node->op());
      for (Handle<Map> map : maps) {
        MakeRef(broker(), map);
      }
      break;
    }
    case IrOpcode::kJSLoadProperty: {
      PropertyAccess const& p = PropertyAccessOf(node->op());
      AccessMode access_mode = AccessMode::kLoad;
      if (p.feedback().IsValid()) {
        broker()->GetFeedbackForPropertyAccess(p.feedback(), access_mode,
                                               base::nullopt);
      }
      break;
    }
    default:
      break;
  }
  return NoChange();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
