// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-heap-copy-reducer.h"

#include "src/compiler/common-operator.h"
#include "src/compiler/js-heap-broker.h"
#include "src/compiler/js-operator.h"
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
      ObjectRef(broker(), HeapConstantOf(node->op()));
      break;
    }
    case IrOpcode::kJSCreateArray: {
      CreateArrayParameters const& p = CreateArrayParametersOf(node->op());
      Handle<AllocationSite> site;
      if (p.site().ToHandle(&site)) AllocationSiteRef(broker(), site);
      break;
    }
    case IrOpcode::kJSCreateCatchContext: {
      ScopeInfoRef(broker(), ScopeInfoOf(node->op()));
      break;
    }
    case IrOpcode::kJSCreateClosure: {
      CreateClosureParameters const& p = CreateClosureParametersOf(node->op());
      SharedFunctionInfoRef(broker(), p.shared_info());
      HeapObjectRef(broker(), p.feedback_cell());
      HeapObjectRef(broker(), p.code());
      break;
    }
    case IrOpcode::kJSCreateEmptyLiteralArray: {
      // TODO(neis, jarin) Force serialization of the entire feedback vector
      // rather than just the one element.
      FeedbackParameter const& p = FeedbackParameterOf(node->op());
      FeedbackVectorRef(broker(), p.feedback().vector());
      Handle<Object> feedback(
          p.feedback().vector()->Get(p.feedback().slot())->ToObject(),
          broker()->isolate());
      ObjectRef(broker(), feedback);
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
      ObjectRef(broker(), p.feedback().vector());
      break;
    }
    case IrOpcode::kJSLoadNamed:
    case IrOpcode::kJSStoreNamed: {
      NamedAccess const& p = NamedAccessOf(node->op());
      NameRef(broker(), p.name());
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
