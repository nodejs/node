// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/frame-states.h"
#include "src/compiler/js-context-relaxation.h"
#include "src/compiler/js-operator.h"
#include "src/compiler/node.h"
#include "src/compiler/node-properties.h"

namespace v8 {
namespace internal {
namespace compiler {

Reduction JSContextRelaxation::Reduce(Node* node) {
  switch (node->opcode()) {
    case IrOpcode::kJSCallFunction:
    case IrOpcode::kJSToNumber: {
      Node* frame_state = NodeProperties::GetFrameStateInput(node, 0);
      Node* outer_frame = frame_state;
      Node* original_context = NodeProperties::GetContextInput(node);
      Node* candidate_new_context = original_context;
      do {
        FrameStateInfo frame_state_info(
            OpParameter<FrameStateInfo>(outer_frame->op()));
        const FrameStateFunctionInfo* function_info =
            frame_state_info.function_info();
        if (function_info == nullptr ||
            (function_info->context_calling_mode() ==
             CALL_CHANGES_NATIVE_CONTEXT)) {
          break;
        }
        candidate_new_context = outer_frame->InputAt(kFrameStateContextInput);
        outer_frame = outer_frame->InputAt(kFrameStateOuterStateInput);
      } while (outer_frame->opcode() == IrOpcode::kFrameState);

      while (true) {
        switch (candidate_new_context->opcode()) {
          case IrOpcode::kParameter:
          case IrOpcode::kJSCreateModuleContext:
          case IrOpcode::kJSCreateScriptContext:
            if (candidate_new_context != original_context) {
              NodeProperties::ReplaceContextInput(node, candidate_new_context);
              return Changed(node);
            } else {
              return NoChange();
            }
          case IrOpcode::kJSCreateCatchContext:
          case IrOpcode::kJSCreateWithContext:
          case IrOpcode::kJSCreateBlockContext:
            candidate_new_context =
                NodeProperties::GetContextInput(candidate_new_context);
            break;
          default:
            return NoChange();
        }
      }
    }
    default:
      break;
  }
  return NoChange();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
