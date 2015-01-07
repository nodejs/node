// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_SCHEDULER_H_
#define V8_COMPILER_SCHEDULER_H_

#include "src/v8.h"

#include "src/compiler/opcodes.h"
#include "src/compiler/schedule.h"
#include "src/compiler/zone-pool.h"
#include "src/zone-containers.h"

namespace v8 {
namespace internal {
namespace compiler {

class CFGBuilder;
class ControlEquivalence;
class SpecialRPONumberer;

// Computes a schedule from a graph, placing nodes into basic blocks and
// ordering the basic blocks in the special RPO order.
class Scheduler {
 public:
  // The complete scheduling algorithm. Creates a new schedule and places all
  // nodes from the graph into it.
  static Schedule* ComputeSchedule(Zone* zone, Graph* graph);

  // Compute the RPO of blocks in an existing schedule.
  static BasicBlockVector* ComputeSpecialRPO(Zone* zone, Schedule* schedule);

 private:
  // Placement of a node changes during scheduling. The placement state
  // transitions over time while the scheduler is choosing a position:
  //
  //                   +---------------------+-----+----> kFixed
  //                  /                     /     /
  //    kUnknown ----+------> kCoupled ----+     /
  //                  \                         /
  //                   +----> kSchedulable ----+--------> kScheduled
  //
  // 1) GetPlacement(): kUnknown -> kCoupled|kSchedulable|kFixed
  // 2) UpdatePlacement(): kCoupled|kSchedulable -> kFixed|kScheduled
  enum Placement { kUnknown, kSchedulable, kFixed, kCoupled, kScheduled };

  // Per-node data tracked during scheduling.
  struct SchedulerData {
    BasicBlock* minimum_block_;  // Minimum legal RPO placement.
    int unscheduled_count_;      // Number of unscheduled uses of this node.
    Placement placement_;        // Whether the node is fixed, schedulable,
                                 // coupled to another node, or not yet known.
  };

  Zone* zone_;
  Graph* graph_;
  Schedule* schedule_;
  NodeVectorVector scheduled_nodes_;     // Per-block list of nodes in reverse.
  NodeVector schedule_root_nodes_;       // Fixed root nodes seed the worklist.
  ZoneQueue<Node*> schedule_queue_;      // Worklist of schedulable nodes.
  ZoneVector<SchedulerData> node_data_;  // Per-node data for all nodes.
  CFGBuilder* control_flow_builder_;     // Builds basic blocks for controls.
  SpecialRPONumberer* special_rpo_;      // Special RPO numbering of blocks.
  ControlEquivalence* equivalence_;      // Control dependence equivalence.

  Scheduler(Zone* zone, Graph* graph, Schedule* schedule);

  inline SchedulerData DefaultSchedulerData();
  inline SchedulerData* GetData(Node* node);

  Placement GetPlacement(Node* node);
  void UpdatePlacement(Node* node, Placement placement);

  inline bool IsCoupledControlEdge(Node* node, int index);
  void IncrementUnscheduledUseCount(Node* node, int index, Node* from);
  void DecrementUnscheduledUseCount(Node* node, int index, Node* from);

  BasicBlock* GetCommonDominator(BasicBlock* b1, BasicBlock* b2);
  void PropagateImmediateDominators(BasicBlock* block);

  // Phase 1: Build control-flow graph.
  friend class CFGBuilder;
  void BuildCFG();

  // Phase 2: Compute special RPO and dominator tree.
  friend class SpecialRPONumberer;
  void ComputeSpecialRPONumbering();
  void GenerateImmediateDominatorTree();

  // Phase 3: Prepare use counts for nodes.
  friend class PrepareUsesVisitor;
  void PrepareUses();

  // Phase 4: Schedule nodes early.
  friend class ScheduleEarlyNodeVisitor;
  void ScheduleEarly();

  // Phase 5: Schedule nodes late.
  friend class ScheduleLateNodeVisitor;
  void ScheduleLate();

  // Phase 6: Seal the final schedule.
  void SealFinalSchedule();

  void FuseFloatingControl(BasicBlock* block, Node* node);
  void MovePlannedNodes(BasicBlock* from, BasicBlock* to);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_SCHEDULER_H_
