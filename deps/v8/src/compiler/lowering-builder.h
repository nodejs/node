// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_LOWERING_BUILDER_H_
#define V8_COMPILER_LOWERING_BUILDER_H_

#include "src/v8.h"

#include "src/compiler/graph.h"


namespace v8 {
namespace internal {
namespace compiler {

// TODO(dcarney): rename this class.
class LoweringBuilder {
 public:
  explicit LoweringBuilder(Graph* graph, SourcePositionTable* source_positions);
  virtual ~LoweringBuilder() {}

  void LowerAllNodes();
  virtual void Lower(Node* node) = 0;  // Exposed for testing.

  Graph* graph() const { return graph_; }

 private:
  class NodeVisitor;
  Graph* graph_;
  SourcePositionTable* source_positions_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_LOWERING_BUILDER_H_
