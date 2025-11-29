// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/checkpoint-elimination.h"

#include "src/compiler/common-operator.h"
#include "src/compiler/node-properties.h"

namespace v8 {
namespace internal {
namespace compiler {

CheckpointElimination::CheckpointElimination(Editor* editor)
    : AdvancedReducer(editor) {}

namespace {

FrameStateFunctionInfo const* GetFunctionInfo(Node* checkpoint) {
  DCHECK_EQ(IrOpcode::kCheckpoint, checkpoint->opcode());
  Node* frame_state = NodeProperties::GetFrameStateInput(checkpoint);
  return frame_state->opcode() == IrOpcode::kFrameState
             ? FrameStateInfoOf(frame_state->op()).function_info()
             : nullptr;
}

// The given checkpoint is redundant if it is effect-wise dominated by another
// checkpoint of the same origin (*) and there is no observable write in
// between. For now we consider a linear effect chain only instead of true
// effect-wise dominance.
// "Same origin" here refers to the same graph building pass and is expressed as
// the identity of the checkpoint's FrameStateFunctionInfo pointer. This
// restriction ensures that an eager deopt from an inlined function will resume
// the inlined function's bytecode (rather than, say, the call in the caller's
// bytecode), which in turn is necessary to ensure that we learn something from
// the deopt in the case where an optimized code object for the inlined function
// exists. See regress-9945-*.js and v8:9945.
bool IsRedundantCheckpoint(Node* node) {
  FrameStateFunctionInfo const* function_info = GetFunctionInfo(node);
  if (function_info == nullptr) return false;
  Node* effect = NodeProperties::GetEffectInput(node);
  while (effect->op()->HasProperty(Operator::kNoWrite) &&
         effect->op()->EffectInputCount() == 1) {
    if (effect->opcode() == IrOpcode::kCheckpoint) {
      return GetFunctionInfo(effect) == function_info;
    }
    effect = NodeProperties::GetEffectInput(effect);
  }
  return false;
}

}  // namespace

Reduction CheckpointElimination::ReduceCheckpoint(Node* node) {
  DCHECK_EQ(IrOpcode::kCheckpoint, node->opcode());
  if (IsRedundantCheckpoint(node)) {
    return Replace(NodeProperties::GetEffectInput(node));
  }
  return NoChange();
}

Reduction CheckpointElimination::Reduce(Node* node) {
  DisallowHeapAccess no_heap_access;
  switch (node->opcode()) {
    case IrOpcode::kCheckpoint:
      return ReduceCheckpoint(node);
    default:
      break;
  }
  return NoChange();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
