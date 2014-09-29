// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/graph-inl.h"
#include "src/compiler/lowering-builder.h"
#include "src/compiler/node-aux-data-inl.h"
#include "src/compiler/node-properties-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

class LoweringBuilder::NodeVisitor : public NullNodeVisitor {
 public:
  explicit NodeVisitor(LoweringBuilder* lowering) : lowering_(lowering) {}

  GenericGraphVisit::Control Post(Node* node) {
    if (lowering_->source_positions_ != NULL) {
      SourcePositionTable::Scope pos(lowering_->source_positions_, node);
      lowering_->Lower(node);
    } else {
      lowering_->Lower(node);
    }
    return GenericGraphVisit::CONTINUE;
  }

 private:
  LoweringBuilder* lowering_;
};


LoweringBuilder::LoweringBuilder(Graph* graph,
                                 SourcePositionTable* source_positions)
    : graph_(graph), source_positions_(source_positions) {}


void LoweringBuilder::LowerAllNodes() {
  NodeVisitor visitor(this);
  graph()->VisitNodeInputsFromEnd(&visitor);
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
