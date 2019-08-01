// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_CONTROL_FLOW_OPTIMIZER_H_
#define V8_COMPILER_CONTROL_FLOW_OPTIMIZER_H_

#include "src/common/globals.h"
#include "src/compiler/node-marker.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forward declarations.
class CommonOperatorBuilder;
class Graph;
class MachineOperatorBuilder;
class Node;

class V8_EXPORT_PRIVATE ControlFlowOptimizer final {
 public:
  ControlFlowOptimizer(Graph* graph, CommonOperatorBuilder* common,
                       MachineOperatorBuilder* machine, Zone* zone);

  void Optimize();

 private:
  void Enqueue(Node* node);
  void VisitNode(Node* node);
  void VisitBranch(Node* node);

  bool TryBuildSwitch(Node* node);
  bool TryCloneBranch(Node* node);

  Graph* graph() const { return graph_; }
  CommonOperatorBuilder* common() const { return common_; }
  MachineOperatorBuilder* machine() const { return machine_; }
  Zone* zone() const { return zone_; }

  Graph* const graph_;
  CommonOperatorBuilder* const common_;
  MachineOperatorBuilder* const machine_;
  ZoneQueue<Node*> queue_;
  NodeMarker<bool> queued_;
  Zone* const zone_;

  DISALLOW_COPY_AND_ASSIGN(ControlFlowOptimizer);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_CONTROL_FLOW_OPTIMIZER_H_
