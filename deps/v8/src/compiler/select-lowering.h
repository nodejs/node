// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_SELECT_LOWERING_H_
#define V8_COMPILER_SELECT_LOWERING_H_

#include <map>

#include "src/compiler/graph-reducer.h"
#include "src/zone-allocator.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forward declarations.
class CommonOperatorBuilder;
class Graph;


// Lowers Select nodes to diamonds.
class SelectLowering FINAL : public Reducer {
 public:
  SelectLowering(Graph* graph, CommonOperatorBuilder* common);
  ~SelectLowering();

  Reduction Reduce(Node* node) OVERRIDE;

 private:
  typedef std::map<Node*, Node*, std::less<Node*>,
                   zone_allocator<std::pair<Node* const, Node*>>> Merges;

  CommonOperatorBuilder* common() const { return common_; }
  Graph* graph() const { return graph_; }

  CommonOperatorBuilder* common_;
  Graph* graph_;
  Merges merges_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_SELECT_LOWERING_H_
