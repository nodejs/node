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
    case IrOpcode::kCheckClosure: {
      FeedbackCellRef cell(broker(), FeedbackCellOf(node->op()));
      FeedbackVectorRef feedback_vector = cell.value().AsFeedbackVector();
      feedback_vector.Serialize();
      break;
    }
    case IrOpcode::kHeapConstant: {
      ObjectRef object(broker(), HeapConstantOf(node->op()));
      if (object.IsJSFunction()) object.AsJSFunction().Serialize();
      if (object.IsJSObject()) {
        object.AsJSObject().SerializeObjectCreateMap();
      }
      if (object.IsSourceTextModule()) {
        object.AsSourceTextModule().Serialize();
      }
      break;
    }
    case IrOpcode::kJSCreateArray: {
      CreateArrayParameters const& p = CreateArrayParametersOf(node->op());
      Handle<AllocationSite> site;
      if (p.site().ToHandle(&site)) AllocationSiteRef(broker(), site);
      break;
    }
    case IrOpcode::kJSCreateArguments: {
      Node* const frame_state = NodeProperties::GetFrameStateInput(node);
      FrameStateInfo state_info = FrameStateInfoOf(frame_state->op());
      SharedFunctionInfoRef shared(broker(),
                                   state_info.shared_info().ToHandleChecked());
      break;
    }
    case IrOpcode::kJSCreateBlockContext: {
      ScopeInfoRef(broker(), ScopeInfoOf(node->op()));
      break;
    }
    case IrOpcode::kJSCreateBoundFunction: {
      CreateBoundFunctionParameters const& p =
          CreateBoundFunctionParametersOf(node->op());
      MapRef(broker(), p.map());
      break;
    }
    case IrOpcode::kJSCreateCatchContext: {
      ScopeInfoRef(broker(), ScopeInfoOf(node->op()));
      break;
    }
    case IrOpcode::kJSCreateClosure: {
      CreateClosureParameters const& p = CreateClosureParametersOf(node->op());
      SharedFunctionInfoRef(broker(), p.shared_info());
      FeedbackCellRef(broker(), p.feedback_cell());
      HeapObjectRef(broker(), p.code());
      break;
    }
    case IrOpcode::kJSCreateEmptyLiteralArray: {
      FeedbackParameter const& p = FeedbackParameterOf(node->op());
      if (p.feedback().IsValid()) {
        broker()->ProcessFeedbackForArrayOrObjectLiteral(p.feedback());
      }
      break;
    }
    case IrOpcode::kJSCreateFunctionContext: {
      CreateFunctionContextParameters const& p =
          CreateFunctionContextParametersOf(node->op());
      ScopeInfoRef(broker(), p.scope_info());
      break;
    }
    case IrOpcode::kJSCreateLiteralArray:
    case IrOpcode::kJSCreateLiteralObject: {
      CreateLiteralParameters const& p = CreateLiteralParametersOf(node->op());
      if (p.feedback().IsValid()) {
        broker()->ProcessFeedbackForArrayOrObjectLiteral(p.feedback());
      }
      break;
    }
    case IrOpcode::kJSCreateLiteralRegExp: {
      CreateLiteralParameters const& p = CreateLiteralParametersOf(node->op());
      if (p.feedback().IsValid()) {
        broker()->ProcessFeedbackForRegExpLiteral(p.feedback());
      }
      break;
    }
    case IrOpcode::kJSGetTemplateObject: {
      GetTemplateObjectParameters const& p =
          GetTemplateObjectParametersOf(node->op());
      SharedFunctionInfoRef shared(broker(), p.shared());
      TemplateObjectDescriptionRef description(broker(), p.description());
      shared.GetTemplateObject(description, p.feedback(),
                               SerializationPolicy::kSerializeIfNeeded);
      break;
    }
    case IrOpcode::kJSCreateWithContext: {
      ScopeInfoRef(broker(), ScopeInfoOf(node->op()));
      break;
    }
    case IrOpcode::kJSLoadNamed: {
      NamedAccess const& p = NamedAccessOf(node->op());
      NameRef name(broker(), p.name());
      if (p.feedback().IsValid()) {
        broker()->ProcessFeedbackForPropertyAccess(p.feedback(),
                                                   AccessMode::kLoad, name);
      }
      break;
    }
    case IrOpcode::kJSStoreNamed: {
      NamedAccess const& p = NamedAccessOf(node->op());
      NameRef name(broker(), p.name());
      break;
    }
    case IrOpcode::kStoreField:
    case IrOpcode::kLoadField: {
      FieldAccess access = FieldAccessOf(node->op());
      Handle<Map> map_handle;
      if (access.map.ToHandle(&map_handle)) {
        MapRef(broker(), map_handle);
      }
      Handle<Name> name_handle;
      if (access.name.ToHandle(&name_handle)) {
        NameRef(broker(), name_handle);
      }
      break;
    }
    case IrOpcode::kMapGuard: {
      ZoneHandleSet<Map> const& maps = MapGuardMapsOf(node->op());
      for (Handle<Map> map : maps) {
        MapRef(broker(), map);
      }
      break;
    }
    case IrOpcode::kCheckMaps: {
      ZoneHandleSet<Map> const& maps = CheckMapsParametersOf(node->op()).maps();
      for (Handle<Map> map : maps) {
        MapRef(broker(), map);
      }
      break;
    }
    case IrOpcode::kCompareMaps: {
      ZoneHandleSet<Map> const& maps = CompareMapsParametersOf(node->op());
      for (Handle<Map> map : maps) {
        MapRef(broker(), map);
      }
      break;
    }
    case IrOpcode::kJSLoadProperty: {
      PropertyAccess const& p = PropertyAccessOf(node->op());
      AccessMode access_mode = AccessMode::kLoad;
      if (p.feedback().IsValid()) {
        broker()->ProcessFeedbackForPropertyAccess(p.feedback(), access_mode,
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
