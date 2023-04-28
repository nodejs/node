// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_LINEAR_SCHEDULER_H_
#define V8_COMPILER_LINEAR_SCHEDULER_H_

#include "src/base/flags.h"
#include "src/common/globals.h"
#include "src/compiler/node.h"
#include "src/compiler/opcodes.h"
#include "src/compiler/zone-stats.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {
namespace compiler {

// A simple, linear-time scheduler to check whether two nodes are in a same
// basic block without actually building basic block.
class V8_EXPORT_PRIVATE LinearScheduler {
 public:
  explicit LinearScheduler(Zone* zone, Graph* graph);
  bool SameBasicBlock(Node* node0, Node* node1);
  // Get a node's early schedule position. It is the earliest block (represented
  // by a control node) where a node could be scheduled.
  Node* GetEarlySchedulePosition(Node* node);

 private:
  // Compute the level of each control node. The level is defined by the
  // shortest control path from the start node.
  void ComputeControlLevel();

  struct NodeState {
    Node* node;
    Node* early_schedule_position;
    int input_index;
  };

  int GetControlLevel(Node* control) const {
    auto it = control_level_.find(control);
    DCHECK(it != control_level_.end());
    return it->second;
  }

  void SetControlLevel(Node* control, int level) {
    DCHECK(control_level_.find(control) == control_level_.end());
    control_level_[control] = level;
  }

  void SetEarlySchedulePosition(Node* node, Node* early_schedule_position) {
    early_schedule_position_[node] = early_schedule_position;
  }

  Graph* graph_;
  // A map from a control node to the control level of the corresponding basic
  // block.
  ZoneMap<Node*, int> control_level_;
  // A map from a non-control node to its early schedule position.
  ZoneMap<Node*, Node*> early_schedule_position_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_LINEAR_SCHEDULER_H_
