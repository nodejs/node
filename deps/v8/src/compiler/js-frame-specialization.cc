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
  // JSFrameSpecialization should never run on interpreted frames, since the
  // code below assumes standard stack frame layouts.
  DCHECK(!frame()->is_interpreted());
  DCHECK_EQ(IrOpcode::kOsrValue, node->opcode());
  Handle<Object> value;
  int index = OsrValueIndexOf(node->op());
  int const parameters_count = frame()->ComputeParametersCount() + 1;
  if (index == Linkage::kOsrContextSpillSlotIndex) {
    value = handle(frame()->context(), isolate());
  } else if (index >= parameters_count) {
    value = handle(frame()->GetExpression(index - parameters_count), isolate());
  } else {
    // The OsrValue index 0 is the receiver.
    value =
        handle(index ? frame()->GetParameter(index - 1) : frame()->receiver(),
               isolate());
  }
  return Replace(jsgraph()->Constant(value));
}

Reduction JSFrameSpecialization::ReduceParameter(Node* node) {
  DCHECK_EQ(IrOpcode::kParameter, node->opcode());
  Handle<Object> value;
  int const index = ParameterIndexOf(node->op());
  int const parameters_count = frame()->ComputeParametersCount() + 1;
  if (index == Linkage::kJSCallClosureParamIndex) {
    // The Parameter index references the closure.
    value = handle(frame()->function(), isolate());
  } else if (index == Linkage::GetJSCallArgCountParamIndex(parameters_count)) {
    // The Parameter index references the parameter count.
    value = handle(Smi::FromInt(parameters_count - 1), isolate());
  } else if (index == Linkage::GetJSCallContextParamIndex(parameters_count)) {
    // The Parameter index references the context.
    value = handle(frame()->context(), isolate());
  } else {
    // The Parameter index 0 is the receiver.
    value =
        handle(index ? frame()->GetParameter(index - 1) : frame()->receiver(),
               isolate());
  }
  return Replace(jsgraph()->Constant(value));
}


Isolate* JSFrameSpecialization::isolate() const { return jsgraph()->isolate(); }

}  // namespace compiler
}  // namespace internal
}  // namespace v8
