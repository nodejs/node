// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/common-operator-reducer.h"

#include "src/compiler/common-operator.h"
#include "src/compiler/node.h"

namespace v8 {
namespace internal {
namespace compiler {

Reduction CommonOperatorReducer::Reduce(Node* node) {
  switch (node->opcode()) {
    case IrOpcode::kEffectPhi:
    case IrOpcode::kPhi: {
      int const input_count = node->InputCount();
      if (input_count > 1) {
        Node* const replacement = node->InputAt(0);
        for (int i = 1; i < input_count - 1; ++i) {
          if (node->InputAt(i) != replacement) return NoChange();
        }
        return Replace(replacement);
      }
      break;
    }
    case IrOpcode::kSelect: {
      if (node->InputAt(1) == node->InputAt(2)) {
        return Replace(node->InputAt(1));
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
