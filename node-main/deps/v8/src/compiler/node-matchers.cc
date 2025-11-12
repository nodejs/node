// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/node-matchers.h"

namespace v8 {
namespace internal {
namespace compiler {

bool NodeMatcher::IsComparison() const {
  return IrOpcode::IsComparisonOpcode(opcode());
}


BranchMatcher::BranchMatcher(Node* branch)
    : NodeMatcher(branch), if_true_(nullptr), if_false_(nullptr) {
  if (branch->opcode() != IrOpcode::kBranch) return;
  for (Node* use : branch->uses()) {
    if (use->opcode() == IrOpcode::kIfTrue) {
      DCHECK_NULL(if_true_);
      if_true_ = use;
    } else if (use->opcode() == IrOpcode::kIfFalse) {
      DCHECK_NULL(if_false_);
      if_false_ = use;
    }
  }
}


DiamondMatcher::DiamondMatcher(Node* merge)
    : NodeMatcher(merge),
      branch_(nullptr),
      if_true_(nullptr),
      if_false_(nullptr) {
  if (merge->InputCount() != 2) return;
  if (merge->opcode() != IrOpcode::kMerge) return;
  Node* input0 = merge->InputAt(0);
  if (input0->InputCount() != 1) return;
  Node* input1 = merge->InputAt(1);
  if (input1->InputCount() != 1) return;
  Node* branch = input0->InputAt(0);
  if (branch != input1->InputAt(0)) return;
  if (branch->opcode() != IrOpcode::kBranch) return;
  if (input0->opcode() == IrOpcode::kIfTrue &&
      input1->opcode() == IrOpcode::kIfFalse) {
    branch_ = branch;
    if_true_ = input0;
    if_false_ = input1;
  } else if (input0->opcode() == IrOpcode::kIfFalse &&
             input1->opcode() == IrOpcode::kIfTrue) {
    branch_ = branch;
    if_true_ = input1;
    if_false_ = input0;
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
