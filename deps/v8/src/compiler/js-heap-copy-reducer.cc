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
      if (!FLAG_concurrent_inlining) {
        ObjectRef object(broker(), HeapConstantOf(node->op()));
        if (object.IsJSFunction()) object.AsJSFunction().Serialize();
        if (object.IsJSObject()) {
          object.AsJSObject().SerializeObjectCreateMap();
        }
        if (object.IsSourceTextModule()) {
          object.AsSourceTextModule().Serialize();
        }
      }
      break;
    }
    case IrOpcode::kJSCreateArray: {
      if (!FLAG_concurrent_inlining) {
        CreateArrayParameters const& p = CreateArrayParametersOf(node->op());
        Handle<AllocationSite> site;
        if (p.site().ToHandle(&site)) AllocationSiteRef(broker(), site);
      }
      break;
    }
    case IrOpcode::kJSCreateArguments: {
      if (!FLAG_concurrent_inlining) {
        Node* const frame_state = NodeProperties::GetFrameStateInput(node);
        FrameStateInfo state_info = FrameStateInfoOf(frame_state->op());
        SharedFunctionInfoRef shared(
            broker(), state_info.shared_info().ToHandleChecked());
      }
      break;
    }
    case IrOpcode::kJSCreateBlockContext: {
      if (!FLAG_concurrent_inlining) {
        ScopeInfoRef(broker(), ScopeInfoOf(node->op()));
      }
      break;
    }
    case IrOpcode::kJSCreateBoundFunction: {
      if (!FLAG_concurrent_inlining) {
        CreateBoundFunctionParameters const& p =
            CreateBoundFunctionParametersOf(node->op());
        MapRef(broker(), p.map());
      }
      break;
    }
    case IrOpcode::kJSCreateCatchContext: {
      if (!FLAG_concurrent_inlining) {
        ScopeInfoRef(broker(), ScopeInfoOf(node->op()));
      }
      break;
    }
    case IrOpcode::kJSCreateClosure: {
      if (!FLAG_concurrent_inlining) {
        CreateClosureParameters const& p =
            CreateClosureParametersOf(node->op());
        SharedFunctionInfoRef(broker(), p.shared_info());
        FeedbackCellRef(broker(), p.feedback_cell());
        HeapObjectRef(broker(), p.code());
      }
      break;
    }
    case IrOpcode::kJSCreateEmptyLiteralArray: {
      if (!FLAG_concurrent_inlining) {
        FeedbackParameter const& p = FeedbackParameterOf(node->op());
        FeedbackVectorRef(broker(), p.feedback().vector).Serialize();
      }
      break;
    }
    case IrOpcode::kJSCreateFunctionContext: {
      if (!FLAG_concurrent_inlining) {
        CreateFunctionContextParameters const& p =
            CreateFunctionContextParametersOf(node->op());
        ScopeInfoRef(broker(), p.scope_info());
      }
      break;
    }
    case IrOpcode::kJSCreateLiteralArray:
    case IrOpcode::kJSCreateLiteralObject: {
      if (!FLAG_concurrent_inlining) {
        CreateLiteralParameters const& p =
            CreateLiteralParametersOf(node->op());
        FeedbackVectorRef(broker(), p.feedback().vector).Serialize();
      }
      break;
    }
    case IrOpcode::kJSCreateLiteralRegExp: {
      if (!FLAG_concurrent_inlining) {
        CreateLiteralParameters const& p =
            CreateLiteralParametersOf(node->op());
        FeedbackVectorRef(broker(), p.feedback().vector).Serialize();
      }
      break;
    }
    case IrOpcode::kJSCreateWithContext: {
      if (!FLAG_concurrent_inlining) {
        ScopeInfoRef(broker(), ScopeInfoOf(node->op()));
      }
      break;
    }
    case IrOpcode::kJSLoadNamed: {
      if (!FLAG_concurrent_inlining) {
        NamedAccess const& p = NamedAccessOf(node->op());
        NameRef name(broker(), p.name());
        if (p.feedback().IsValid()) {
          broker()->ProcessFeedbackForPropertyAccess(p.feedback(),
                                                     AccessMode::kLoad, name);
        }
      }
      break;
    }
    case IrOpcode::kJSStoreNamed: {
      if (!FLAG_concurrent_inlining) {
        NamedAccess const& p = NamedAccessOf(node->op());
        NameRef name(broker(), p.name());
      }
      break;
    }
    case IrOpcode::kStoreField:
    case IrOpcode::kLoadField: {
      if (!FLAG_concurrent_inlining) {
        FieldAccess access = FieldAccessOf(node->op());
        Handle<Map> map_handle;
        if (access.map.ToHandle(&map_handle)) {
          MapRef(broker(), map_handle);
        }
        Handle<Name> name_handle;
        if (access.name.ToHandle(&name_handle)) {
          NameRef(broker(), name_handle);
        }
      }
      break;
    }
    case IrOpcode::kMapGuard: {
      if (!FLAG_concurrent_inlining) {
        ZoneHandleSet<Map> const& maps = MapGuardMapsOf(node->op());
        for (Handle<Map> map : maps) {
          MapRef(broker(), map);
        }
      }
      break;
    }
    case IrOpcode::kCheckMaps: {
      if (!FLAG_concurrent_inlining) {
        ZoneHandleSet<Map> const& maps =
            CheckMapsParametersOf(node->op()).maps();
        for (Handle<Map> map : maps) {
          MapRef(broker(), map);
        }
      }
      break;
    }
    case IrOpcode::kCompareMaps: {
      if (!FLAG_concurrent_inlining) {
        ZoneHandleSet<Map> const& maps = CompareMapsParametersOf(node->op());
        for (Handle<Map> map : maps) {
          MapRef(broker(), map);
        }
      }
      break;
    }
    case IrOpcode::kJSLoadProperty: {
      if (!FLAG_concurrent_inlining) {
        PropertyAccess const& p = PropertyAccessOf(node->op());
        AccessMode access_mode = AccessMode::kLoad;
        if (p.feedback().IsValid()) {
          broker()->ProcessFeedbackForPropertyAccess(p.feedback(), access_mode,
                                                     base::nullopt);
        }
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
