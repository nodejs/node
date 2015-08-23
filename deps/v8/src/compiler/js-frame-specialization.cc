// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-frame-specialization.h"

#include "src/compiler/js-graph.h"
#include "src/compiler/linkage.h"
#include "src/frames-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

Reduction JSFrameSpecialization::Reduce(Node* node) {
  switch (node->opcode()) {
    case IrOpcode::kOsrValue:
      return ReduceOsrValue(node);
    case IrOpcode::kParameter:
      return ReduceParameter(node);
    default:
      break;
  }
  return NoChange();
}


Reduction JSFrameSpecialization::ReduceOsrValue(Node* node) {
  DCHECK_EQ(IrOpcode::kOsrValue, node->opcode());
  DisallowHeapAllocation no_gc;
  Object* object;
  int const index = OpParameter<int>(node);
  int const parameters_count = frame()->ComputeParametersCount() + 1;
  if (index == Linkage::kOsrContextSpillSlotIndex) {
    object = frame()->context();
  } else if (index >= parameters_count) {
    object = frame()->GetExpression(index - parameters_count);
  } else {
    // The OsrValue index 0 is the receiver.
    object = index ? frame()->GetParameter(index - 1) : frame()->receiver();
  }
  return Replace(jsgraph()->Constant(handle(object, isolate())));
}


Reduction JSFrameSpecialization::ReduceParameter(Node* node) {
  DCHECK_EQ(IrOpcode::kParameter, node->opcode());
  DisallowHeapAllocation no_gc;
  Object* object;
  int const index = ParameterIndexOf(node->op());
  int const parameters_count = frame()->ComputeParametersCount() + 1;
  if (index == Linkage::kJSFunctionCallClosureParamIndex) {
    object = frame()->function();
  } else if (index == parameters_count) {
    // The Parameter index (arity + 1) is the context.
    object = frame()->context();
  } else {
    // The Parameter index 0 is the receiver.
    object = index ? frame()->GetParameter(index - 1) : frame()->receiver();
  }
  return Replace(jsgraph()->Constant(handle(object, isolate())));
}


Isolate* JSFrameSpecialization::isolate() const { return jsgraph()->isolate(); }

}  // namespace compiler
}  // namespace internal
}  // namespace v8
