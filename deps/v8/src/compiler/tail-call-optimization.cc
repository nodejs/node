// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/tail-call-optimization.h"

#include "src/compiler/common-operator.h"
#include "src/compiler/graph.h"
#include "src/compiler/linkage.h"
#include "src/compiler/node-properties.h"

namespace v8 {
namespace internal {
namespace compiler {

Reduction TailCallOptimization::Reduce(Node* node) {
  if (node->opcode() != IrOpcode::kReturn) return NoChange();
  // The value which is returned must be the result of a potential tail call,
  // there must be no try/catch/finally around the Call, and there must be no
  // other effect between the Call and the Return nodes.
  Node* const call = NodeProperties::GetValueInput(node, 0);
  if (call->opcode() == IrOpcode::kCall &&
      OpParameter<CallDescriptor const*>(call)->SupportsTailCalls() &&
      NodeProperties::GetEffectInput(node) == call &&
      !NodeProperties::IsExceptionalCall(call)) {
    Node* const control = NodeProperties::GetControlInput(node);
    if (control->opcode() == IrOpcode::kIfSuccess &&
        call->OwnedBy(node, control) && control->OwnedBy(node)) {
      // Furthermore, control has to flow via an IfSuccess from the Call, so
      // the Return node value and effect depends directly on the Call node,
      // and indirectly control depends on the Call via an IfSuccess.

      // Value1 ... ValueN Effect Control
      //   ^          ^      ^       ^
      //   |          |      |       |
      //   |          +--+ +-+       |
      //   +----------+  | |  +------+
      //               \ | | /
      //             Call[Descriptor]
      //                ^ ^ ^
      //                | | |
      //              +-+ | |
      //              |   | |
      //              | +-+ |
      //              | | IfSuccess
      //              | |  ^
      //              | |  |
      //              Return
      //                ^
      //                |

      // The resulting graph looks like this:

      // Value1 ... ValueN Effect Control
      //   ^          ^      ^       ^
      //   |          |      |       |
      //   |          +--+ +-+       |
      //   +----------+  | |  +------+
      //               \ | | /
      //           TailCall[Descriptor]
      //                 ^
      //                 |

      DCHECK_EQ(call, NodeProperties::GetControlInput(control, 0));
      DCHECK_EQ(3, node->InputCount());
      node->ReplaceInput(0, NodeProperties::GetEffectInput(call));
      node->ReplaceInput(1, NodeProperties::GetControlInput(call));
      node->RemoveInput(2);
      for (int index = 0; index < call->op()->ValueInputCount(); ++index) {
        node->InsertInput(graph()->zone(), index,
                          NodeProperties::GetValueInput(call, index));
      }
      NodeProperties::ChangeOp(
          node, common()->TailCall(OpParameter<CallDescriptor const*>(call)));
      return Changed(node);
    }
  }
  return NoChange();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
