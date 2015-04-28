// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_DIAMOND_H_
#define V8_COMPILER_DIAMOND_H_

#include "src/compiler/common-operator.h"
#include "src/compiler/graph.h"
#include "src/compiler/node.h"

namespace v8 {
namespace internal {
namespace compiler {

// A helper to make it easier to build diamond-shaped control patterns.
struct Diamond {
  Graph* graph;
  CommonOperatorBuilder* common;
  Node* branch;
  Node* if_true;
  Node* if_false;
  Node* merge;

  Diamond(Graph* g, CommonOperatorBuilder* b, Node* cond,
          BranchHint hint = BranchHint::kNone) {
    graph = g;
    common = b;
    branch = graph->NewNode(common->Branch(hint), cond, graph->start());
    if_true = graph->NewNode(common->IfTrue(), branch);
    if_false = graph->NewNode(common->IfFalse(), branch);
    merge = graph->NewNode(common->Merge(2), if_true, if_false);
  }

  // Place {this} after {that} in control flow order.
  void Chain(Diamond& that) { branch->ReplaceInput(1, that.merge); }

  // Place {this} after {that} in control flow order.
  void Chain(Node* that) { branch->ReplaceInput(1, that); }

  // Nest {this} into either the if_true or if_false branch of {that}.
  void Nest(Diamond& that, bool if_true) {
    if (if_true) {
      branch->ReplaceInput(1, that.if_true);
      that.merge->ReplaceInput(0, merge);
    } else {
      branch->ReplaceInput(1, that.if_false);
      that.merge->ReplaceInput(1, merge);
    }
  }

  Node* Phi(MachineType machine_type, Node* tv, Node* fv) {
    return graph->NewNode(common->Phi(machine_type, 2), tv, fv, merge);
  }

  Node* EffectPhi(Node* tv, Node* fv) {
    return graph->NewNode(common->EffectPhi(2), tv, fv, merge);
  }

  void OverwriteWithPhi(Node* node, MachineType machine_type, Node* tv,
                        Node* fv) {
    DCHECK(node->InputCount() >= 3);
    node->set_op(common->Phi(machine_type, 2));
    node->ReplaceInput(0, tv);
    node->ReplaceInput(1, fv);
    node->ReplaceInput(2, merge);
    node->TrimInputCount(3);
  }

  void OverwriteWithEffectPhi(Node* node, Node* te, Node* fe) {
    DCHECK(node->InputCount() >= 3);
    node->set_op(common->EffectPhi(2));
    node->ReplaceInput(0, te);
    node->ReplaceInput(1, fe);
    node->ReplaceInput(2, merge);
    node->TrimInputCount(3);
  }
};
}
}
}  // namespace v8::internal::compiler

#endif  // V8_COMPILER_DIAMOND_H_
