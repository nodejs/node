// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/backend/bitcast-elider.h"

#include "src/compiler/graph.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {

bool IsBitcast(Node* node) {
  // We can only elide kBitcastTaggedToWordForTagAndSmiBits and
  // kBitcastWordToTaggedSigned because others might affect GC / safepoint
  // tables.
  return node->opcode() == IrOpcode::kBitcastTaggedToWordForTagAndSmiBits ||
         node->opcode() == IrOpcode::kBitcastWordToTaggedSigned;
}

}  // namespace

void BitcastElider::Enqueue(Node* node) {
  if (seen_.Get(node)) return;
  seen_.Set(node, true);
  to_visit_.push(node);
}

void BitcastElider::VisitNode(Node* node) {
  for (int i = 0; i < node->InputCount(); i++) {
    Node* input = node->InputAt(i);
    bool should_replace_input = false;
    while (IsBitcast(input)) {
      input = input->InputAt(0);
      should_replace_input = true;
    }
    if (should_replace_input) {
      node->ReplaceInput(i, input);
    }
    Enqueue(input);
  }
}

void BitcastElider::ProcessGraph() {
  Enqueue(graph_->end());
  while (!to_visit_.empty()) {
    Node* node = to_visit_.front();
    to_visit_.pop();
    VisitNode(node);
  }
}

BitcastElider::BitcastElider(Zone* zone, Graph* graph)
    : graph_(graph), to_visit_(zone), seen_(graph, 2) {}

void BitcastElider::Reduce() { ProcessGraph(); }

}  // namespace compiler
}  // namespace internal
}  // namespace v8
