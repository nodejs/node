// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_CONTROL_FLOW_OPTIMIZER_H_
#define V8_COMPILER_CONTROL_FLOW_OPTIMIZER_H_

#include "src/compiler/node-marker.h"
#include "src/zone-containers.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forward declarations.
class CommonOperatorBuilder;
class Graph;
class JSGraph;
class MachineOperatorBuilder;
class Node;


class ControlFlowOptimizer final {
 public:
  ControlFlowOptimizer(JSGraph* jsgraph, Zone* zone);

  void Optimize();

 private:
  void Enqueue(Node* node);
  void VisitNode(Node* node);
  void VisitBranch(Node* node);

  bool TryBuildSwitch(Node* node);
  bool TryCloneBranch(Node* node);

  CommonOperatorBuilder* common() const;
  Graph* graph() const;
  JSGraph* jsgraph() const { return jsgraph_; }
  MachineOperatorBuilder* machine() const;
  Zone* zone() const { return zone_; }

  JSGraph* const jsgraph_;
  ZoneQueue<Node*> queue_;
  NodeMarker<bool> queued_;
  Zone* const zone_;

  DISALLOW_COPY_AND_ASSIGN(ControlFlowOptimizer);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_CONTROL_FLOW_OPTIMIZER_H_
