// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_PHI_REDUCER_H_
#define V8_COMPILER_PHI_REDUCER_H_

#include "src/compiler/generic-node-inl.h"
#include "src/compiler/graph-reducer.h"

namespace v8 {
namespace internal {
namespace compiler {

// Replaces redundant phis if all the inputs are the same or the phi itself.
class PhiReducer FINAL : public Reducer {
 public:
  virtual Reduction Reduce(Node* node) OVERRIDE {
    if (node->opcode() != IrOpcode::kPhi &&
        node->opcode() != IrOpcode::kEffectPhi)
      return NoChange();

    int n = node->op()->InputCount();
    if (n == 1) return Replace(node->InputAt(0));

    Node* replacement = NULL;
    Node::Inputs inputs = node->inputs();
    for (InputIter it = inputs.begin(); n > 0; --n, ++it) {
      Node* input = *it;
      if (input != node && input != replacement) {
        if (replacement != NULL) return NoChange();
        replacement = input;
      }
    }
    DCHECK_NE(node, replacement);
    return Replace(replacement);
  }
};
}
}
}  // namespace v8::internal::compiler

#endif  // V8_COMPILER_PHI_REDUCER_H_
