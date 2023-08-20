// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_BRANCH_CONDITION_DUPLICATOR_H_
#define V8_COMPILER_BRANCH_CONDITION_DUPLICATOR_H_

#include "src/base/macros.h"
#include "src/compiler/node-marker.h"
#include "src/compiler/node.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forward declare.
class Graph;

// BranchConditionDuplicator makes sure that the condition nodes of branches are
// used only once. When it finds a branch node whose condition has multiples
// uses, this condition is duplicated.
//
// Doing this enables the InstructionSelector to generate more efficient code
// for branches. For instance, consider this code:
//
//     if (a + b == 0) { /* some code */ }
//     if (a + b == 0) { /* more code */ }
//
// Then the generated code will be something like (using registers "ra" for "a"
// and "rb" for "b", and "rt" a temporary register):
//
//     add ra, rb  ; a + b
//     cmp ra, 0   ; (a + b) == 0
//     sete rt     ; rt = (a + b) == 0
//     cmp rt, 0   ; rt == 0
//     jz
//     ...
//     cmp rt, 0   ; rt == 0
//     jz
//
// As you can see, TurboFan materialized the == bit into a temporary register.
// However, since the "add" instruction sets the ZF flag (on x64), it can be
// used to determine wether the jump should be taken or not. The code we'd like
// to generate instead if thus:
//
//     add ra, rb
//     jnz
//     ...
//     add ra, rb
//     jnz
//
// However, this requires to generate twice the instruction "add ra, rb". Due to
// how virtual registers are assigned in TurboFan (there is a map from node ID
// to virtual registers), both "add" instructions will use the same virtual
// register as output, which will break SSA.
//
// In order to overcome this issue, BranchConditionDuplicator duplicates branch
// conditions that are used more than once, so that they can be generated right
// before each branch without worrying about breaking SSA.

class V8_EXPORT_PRIVATE BranchConditionDuplicator final {
 public:
  BranchConditionDuplicator(Zone* zone, Graph* graph);
  ~BranchConditionDuplicator() = default;

  void Reduce();

  Node* DuplicateNode(Node* node);
  void DuplicateConditionIfNeeded(Node* node);
  void Enqueue(Node* node);
  void VisitNode(Node* node);
  void ProcessGraph();

 private:
  Graph* const graph_;
  ZoneQueue<Node*> to_visit_;
  NodeMarker<bool> seen_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_BRANCH_CONDITION_DUPLICATOR_H_
