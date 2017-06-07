// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/tail-call-optimization.h"

#include "src/compiler/common-operator.h"
#include "src/compiler/graph.h"
#include "src/compiler/linkage.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties.h"

namespace v8 {
namespace internal {
namespace compiler {

Reduction TailCallOptimization::Reduce(Node* node) {
  if (node->opcode() != IrOpcode::kReturn) return NoChange();
  // The value which is returned must be the result of a potential tail call,
  // there must be no try/catch/finally around the Call, and there must be no
  // other effect or control between the Call and the Return nodes.
  Node* const call = NodeProperties::GetValueInput(node, 1);
  if (call->opcode() == IrOpcode::kCall &&
      CallDescriptorOf(call->op())->SupportsTailCalls() &&
      NodeProperties::GetEffectInput(node) == call &&
      NodeProperties::GetControlInput(node) == call &&
      !NodeProperties::IsExceptionalCall(call) && call->UseCount() == 3) {
    // Ensure that no additional arguments are being popped other than those in
    // the CallDescriptor, otherwise the tail call transformation is invalid.
    DCHECK_EQ(0, Int32Matcher(NodeProperties::GetValueInput(node, 0)).Value());
    // Furthermore, the Return node value, effect, and control depends
    // directly on the Call, no other uses of the Call node exist.
    //
    // The input graph looks as follows:

    // Value1 ... ValueN Effect Control
    //   ^          ^      ^       ^
    //   |          |      |       |
    //   |          +--+ +-+       |
    //   +----------+  | |  +------+
    //               \ | | /
    //             Call[Descriptor]
    //                ^ ^ ^
    //  Int32(0) <-+  | | |
    //              \ | | |
    //               Return
    //                  ^
    //                  |

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

    DCHECK_EQ(4, node->InputCount());
    node->ReplaceInput(0, NodeProperties::GetEffectInput(call));
    node->ReplaceInput(1, NodeProperties::GetControlInput(call));
    node->RemoveInput(3);
    node->RemoveInput(2);
    for (int index = 0; index < call->op()->ValueInputCount(); ++index) {
      node->InsertInput(graph()->zone(), index,
                        NodeProperties::GetValueInput(call, index));
    }
    NodeProperties::ChangeOp(node,
                             common()->TailCall(CallDescriptorOf(call->op())));
    return Changed(node);
  }
  return NoChange();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
