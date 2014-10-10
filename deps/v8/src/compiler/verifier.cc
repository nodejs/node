// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/verifier.h"

#include <deque>
#include <queue>

#include "src/compiler/generic-algorithm.h"
#include "src/compiler/generic-node-inl.h"
#include "src/compiler/generic-node.h"
#include "src/compiler/graph-inl.h"
#include "src/compiler/graph.h"
#include "src/compiler/node.h"
#include "src/compiler/node-properties-inl.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/opcodes.h"
#include "src/compiler/operator.h"
#include "src/compiler/schedule.h"
#include "src/data-flow.h"

namespace v8 {
namespace internal {
namespace compiler {


static bool IsDefUseChainLinkPresent(Node* def, Node* use) {
  Node::Uses uses = def->uses();
  for (Node::Uses::iterator it = uses.begin(); it != uses.end(); ++it) {
    if (*it == use) return true;
  }
  return false;
}


static bool IsUseDefChainLinkPresent(Node* def, Node* use) {
  Node::Inputs inputs = use->inputs();
  for (Node::Inputs::iterator it = inputs.begin(); it != inputs.end(); ++it) {
    if (*it == def) return true;
  }
  return false;
}


class Verifier::Visitor : public NullNodeVisitor {
 public:
  explicit Visitor(Zone* zone)
      : reached_from_start(NodeSet::key_compare(),
                           NodeSet::allocator_type(zone)),
        reached_from_end(NodeSet::key_compare(),
                         NodeSet::allocator_type(zone)) {}

  // Fulfills the PreNodeCallback interface.
  GenericGraphVisit::Control Pre(Node* node);

  bool from_start;
  NodeSet reached_from_start;
  NodeSet reached_from_end;
};


GenericGraphVisit::Control Verifier::Visitor::Pre(Node* node) {
  int value_count = OperatorProperties::GetValueInputCount(node->op());
  int context_count = OperatorProperties::GetContextInputCount(node->op());
  int frame_state_count =
      OperatorProperties::GetFrameStateInputCount(node->op());
  int effect_count = OperatorProperties::GetEffectInputCount(node->op());
  int control_count = OperatorProperties::GetControlInputCount(node->op());

  // Verify number of inputs matches up.
  int input_count = value_count + context_count + frame_state_count +
                    effect_count + control_count;
  CHECK_EQ(input_count, node->InputCount());

  // Verify that frame state has been inserted for the nodes that need it.
  if (OperatorProperties::HasFrameStateInput(node->op())) {
    Node* frame_state = NodeProperties::GetFrameStateInput(node);
    CHECK(frame_state->opcode() == IrOpcode::kFrameState ||
          // kFrameState uses undefined as a sentinel.
          (node->opcode() == IrOpcode::kFrameState &&
           frame_state->opcode() == IrOpcode::kHeapConstant));
    CHECK(IsDefUseChainLinkPresent(frame_state, node));
    CHECK(IsUseDefChainLinkPresent(frame_state, node));
  }

  // Verify all value inputs actually produce a value.
  for (int i = 0; i < value_count; ++i) {
    Node* value = NodeProperties::GetValueInput(node, i);
    CHECK(OperatorProperties::HasValueOutput(value->op()));
    CHECK(IsDefUseChainLinkPresent(value, node));
    CHECK(IsUseDefChainLinkPresent(value, node));
  }

  // Verify all context inputs are value nodes.
  for (int i = 0; i < context_count; ++i) {
    Node* context = NodeProperties::GetContextInput(node);
    CHECK(OperatorProperties::HasValueOutput(context->op()));
    CHECK(IsDefUseChainLinkPresent(context, node));
    CHECK(IsUseDefChainLinkPresent(context, node));
  }

  // Verify all effect inputs actually have an effect.
  for (int i = 0; i < effect_count; ++i) {
    Node* effect = NodeProperties::GetEffectInput(node);
    CHECK(OperatorProperties::HasEffectOutput(effect->op()));
    CHECK(IsDefUseChainLinkPresent(effect, node));
    CHECK(IsUseDefChainLinkPresent(effect, node));
  }

  // Verify all control inputs are control nodes.
  for (int i = 0; i < control_count; ++i) {
    Node* control = NodeProperties::GetControlInput(node, i);
    CHECK(OperatorProperties::HasControlOutput(control->op()));
    CHECK(IsDefUseChainLinkPresent(control, node));
    CHECK(IsUseDefChainLinkPresent(control, node));
  }

  // Verify all successors are projections if multiple value outputs exist.
  if (OperatorProperties::GetValueOutputCount(node->op()) > 1) {
    Node::Uses uses = node->uses();
    for (Node::Uses::iterator it = uses.begin(); it != uses.end(); ++it) {
      CHECK(!NodeProperties::IsValueEdge(it.edge()) ||
            (*it)->opcode() == IrOpcode::kProjection ||
            (*it)->opcode() == IrOpcode::kParameter);
    }
  }

  switch (node->opcode()) {
    case IrOpcode::kStart:
      // Start has no inputs.
      CHECK_EQ(0, input_count);
      break;
    case IrOpcode::kEnd:
      // End has no outputs.
      CHECK(!OperatorProperties::HasValueOutput(node->op()));
      CHECK(!OperatorProperties::HasEffectOutput(node->op()));
      CHECK(!OperatorProperties::HasControlOutput(node->op()));
      break;
    case IrOpcode::kDead:
      // Dead is never connected to the graph.
      UNREACHABLE();
    case IrOpcode::kBranch: {
      // Branch uses are IfTrue and IfFalse.
      Node::Uses uses = node->uses();
      bool got_true = false, got_false = false;
      for (Node::Uses::iterator it = uses.begin(); it != uses.end(); ++it) {
        CHECK(((*it)->opcode() == IrOpcode::kIfTrue && !got_true) ||
              ((*it)->opcode() == IrOpcode::kIfFalse && !got_false));
        if ((*it)->opcode() == IrOpcode::kIfTrue) got_true = true;
        if ((*it)->opcode() == IrOpcode::kIfFalse) got_false = true;
      }
      // TODO(rossberg): Currently fails for various tests.
      // CHECK(got_true && got_false);
      break;
    }
    case IrOpcode::kIfTrue:
    case IrOpcode::kIfFalse:
      CHECK_EQ(IrOpcode::kBranch,
               NodeProperties::GetControlInput(node, 0)->opcode());
      break;
    case IrOpcode::kLoop:
    case IrOpcode::kMerge:
      break;
    case IrOpcode::kReturn:
      // TODO(rossberg): check successor is End
      break;
    case IrOpcode::kThrow:
      // TODO(rossberg): what are the constraints on these?
      break;
    case IrOpcode::kParameter: {
      // Parameters have the start node as inputs.
      CHECK_EQ(1, input_count);
      CHECK_EQ(IrOpcode::kStart,
               NodeProperties::GetValueInput(node, 0)->opcode());
      // Parameter has an input that produces enough values.
      int index = OpParameter<int>(node);
      Node* input = NodeProperties::GetValueInput(node, 0);
      // Currently, parameter indices start at -1 instead of 0.
      CHECK_GT(OperatorProperties::GetValueOutputCount(input->op()), index + 1);
      break;
    }
    case IrOpcode::kInt32Constant:
    case IrOpcode::kInt64Constant:
    case IrOpcode::kFloat64Constant:
    case IrOpcode::kExternalConstant:
    case IrOpcode::kNumberConstant:
    case IrOpcode::kHeapConstant:
      // Constants have no inputs.
      CHECK_EQ(0, input_count);
      break;
    case IrOpcode::kPhi: {
      // Phi input count matches parent control node.
      CHECK_EQ(1, control_count);
      Node* control = NodeProperties::GetControlInput(node, 0);
      CHECK_EQ(value_count,
               OperatorProperties::GetControlInputCount(control->op()));
      break;
    }
    case IrOpcode::kEffectPhi: {
      // EffectPhi input count matches parent control node.
      CHECK_EQ(1, control_count);
      Node* control = NodeProperties::GetControlInput(node, 0);
      CHECK_EQ(effect_count,
               OperatorProperties::GetControlInputCount(control->op()));
      break;
    }
    case IrOpcode::kFrameState:
      // TODO(jarin): what are the constraints on these?
      break;
    case IrOpcode::kCall:
      // TODO(rossberg): what are the constraints on these?
      break;
    case IrOpcode::kProjection: {
      // Projection has an input that produces enough values.
      size_t index = OpParameter<size_t>(node);
      Node* input = NodeProperties::GetValueInput(node, 0);
      CHECK_GT(OperatorProperties::GetValueOutputCount(input->op()),
               static_cast<int>(index));
      break;
    }
    default:
      // TODO(rossberg): Check other node kinds.
      break;
  }

  if (from_start) {
    reached_from_start.insert(node);
  } else {
    reached_from_end.insert(node);
  }

  return GenericGraphVisit::CONTINUE;
}


void Verifier::Run(Graph* graph) {
  Visitor visitor(graph->zone());

  CHECK_NE(NULL, graph->start());
  visitor.from_start = true;
  graph->VisitNodeUsesFromStart(&visitor);
  CHECK_NE(NULL, graph->end());
  visitor.from_start = false;
  graph->VisitNodeInputsFromEnd(&visitor);

  // All control nodes reachable from end are reachable from start.
  for (NodeSet::iterator it = visitor.reached_from_end.begin();
       it != visitor.reached_from_end.end(); ++it) {
    CHECK(!NodeProperties::IsControl(*it) ||
          visitor.reached_from_start.count(*it));
  }
}


static bool HasDominatingDef(Schedule* schedule, Node* node,
                             BasicBlock* container, BasicBlock* use_block,
                             int use_pos) {
  BasicBlock* block = use_block;
  while (true) {
    while (use_pos >= 0) {
      if (block->nodes_[use_pos] == node) return true;
      use_pos--;
    }
    block = block->dominator_;
    if (block == NULL) break;
    use_pos = static_cast<int>(block->nodes_.size()) - 1;
    if (node == block->control_input_) return true;
  }
  return false;
}


static void CheckInputsDominate(Schedule* schedule, BasicBlock* block,
                                Node* node, int use_pos) {
  for (int j = OperatorProperties::GetValueInputCount(node->op()) - 1; j >= 0;
       j--) {
    BasicBlock* use_block = block;
    if (node->opcode() == IrOpcode::kPhi) {
      use_block = use_block->PredecessorAt(j);
      use_pos = static_cast<int>(use_block->nodes_.size()) - 1;
    }
    Node* input = node->InputAt(j);
    if (!HasDominatingDef(schedule, node->InputAt(j), block, use_block,
                          use_pos)) {
      V8_Fatal(__FILE__, __LINE__,
               "Node #%d:%s in B%d is not dominated by input@%d #%d:%s",
               node->id(), node->op()->mnemonic(), block->id(), j, input->id(),
               input->op()->mnemonic());
    }
  }
}


void ScheduleVerifier::Run(Schedule* schedule) {
  const int count = schedule->BasicBlockCount();
  Zone tmp_zone(schedule->zone()->isolate());
  Zone* zone = &tmp_zone;
  BasicBlock* start = schedule->start();
  BasicBlockVector* rpo_order = schedule->rpo_order();

  // Verify the RPO order contains only blocks from this schedule.
  CHECK_GE(count, static_cast<int>(rpo_order->size()));
  for (BasicBlockVector::iterator b = rpo_order->begin(); b != rpo_order->end();
       ++b) {
    CHECK_EQ((*b), schedule->GetBlockById((*b)->id()));
  }

  // Verify RPO numbers of blocks.
  CHECK_EQ(start, rpo_order->at(0));  // Start should be first.
  for (size_t b = 0; b < rpo_order->size(); b++) {
    BasicBlock* block = rpo_order->at(b);
    CHECK_EQ(static_cast<int>(b), block->rpo_number_);
    BasicBlock* dom = block->dominator_;
    if (b == 0) {
      // All blocks except start should have a dominator.
      CHECK_EQ(NULL, dom);
    } else {
      // Check that the immediate dominator appears somewhere before the block.
      CHECK_NE(NULL, dom);
      CHECK_LT(dom->rpo_number_, block->rpo_number_);
    }
  }

  // Verify that all blocks reachable from start are in the RPO.
  BoolVector marked(count, false, zone);
  {
    ZoneQueue<BasicBlock*> queue(zone);
    queue.push(start);
    marked[start->id()] = true;
    while (!queue.empty()) {
      BasicBlock* block = queue.front();
      queue.pop();
      for (int s = 0; s < block->SuccessorCount(); s++) {
        BasicBlock* succ = block->SuccessorAt(s);
        if (!marked[succ->id()]) {
          marked[succ->id()] = true;
          queue.push(succ);
        }
      }
    }
  }
  // Verify marked blocks are in the RPO.
  for (int i = 0; i < count; i++) {
    BasicBlock* block = schedule->GetBlockById(i);
    if (marked[i]) {
      CHECK_GE(block->rpo_number_, 0);
      CHECK_EQ(block, rpo_order->at(block->rpo_number_));
    }
  }
  // Verify RPO blocks are marked.
  for (size_t b = 0; b < rpo_order->size(); b++) {
    CHECK(marked[rpo_order->at(b)->id()]);
  }

  {
    // Verify the dominance relation.
    ZoneList<BitVector*> dominators(count, zone);
    dominators.Initialize(count, zone);
    dominators.AddBlock(NULL, count, zone);

    // Compute a set of all the nodes that dominate a given node by using
    // a forward fixpoint. O(n^2).
    ZoneQueue<BasicBlock*> queue(zone);
    queue.push(start);
    dominators[start->id()] = new (zone) BitVector(count, zone);
    while (!queue.empty()) {
      BasicBlock* block = queue.front();
      queue.pop();
      BitVector* block_doms = dominators[block->id()];
      BasicBlock* idom = block->dominator_;
      if (idom != NULL && !block_doms->Contains(idom->id())) {
        V8_Fatal(__FILE__, __LINE__, "Block B%d is not dominated by B%d",
                 block->id(), idom->id());
      }
      for (int s = 0; s < block->SuccessorCount(); s++) {
        BasicBlock* succ = block->SuccessorAt(s);
        BitVector* succ_doms = dominators[succ->id()];

        if (succ_doms == NULL) {
          // First time visiting the node. S.doms = B U B.doms
          succ_doms = new (zone) BitVector(count, zone);
          succ_doms->CopyFrom(*block_doms);
          succ_doms->Add(block->id());
          dominators[succ->id()] = succ_doms;
          queue.push(succ);
        } else {
          // Nth time visiting the successor. S.doms = S.doms ^ (B U B.doms)
          bool had = succ_doms->Contains(block->id());
          if (had) succ_doms->Remove(block->id());
          if (succ_doms->IntersectIsChanged(*block_doms)) queue.push(succ);
          if (had) succ_doms->Add(block->id());
        }
      }
    }

    // Verify the immediateness of dominators.
    for (BasicBlockVector::iterator b = rpo_order->begin();
         b != rpo_order->end(); ++b) {
      BasicBlock* block = *b;
      BasicBlock* idom = block->dominator_;
      if (idom == NULL) continue;
      BitVector* block_doms = dominators[block->id()];

      for (BitVector::Iterator it(block_doms); !it.Done(); it.Advance()) {
        BasicBlock* dom = schedule->GetBlockById(it.Current());
        if (dom != idom && !dominators[idom->id()]->Contains(dom->id())) {
          V8_Fatal(__FILE__, __LINE__,
                   "Block B%d is not immediately dominated by B%d", block->id(),
                   idom->id());
        }
      }
    }
  }

  // Verify phis are placed in the block of their control input.
  for (BasicBlockVector::iterator b = rpo_order->begin(); b != rpo_order->end();
       ++b) {
    for (BasicBlock::const_iterator i = (*b)->begin(); i != (*b)->end(); ++i) {
      Node* phi = *i;
      if (phi->opcode() != IrOpcode::kPhi) continue;
      // TODO(titzer): Nasty special case. Phis from RawMachineAssembler
      // schedules don't have control inputs.
      if (phi->InputCount() >
          OperatorProperties::GetValueInputCount(phi->op())) {
        Node* control = NodeProperties::GetControlInput(phi);
        CHECK(control->opcode() == IrOpcode::kMerge ||
              control->opcode() == IrOpcode::kLoop);
        CHECK_EQ((*b), schedule->block(control));
      }
    }
  }

  // Verify that all uses are dominated by their definitions.
  for (BasicBlockVector::iterator b = rpo_order->begin(); b != rpo_order->end();
       ++b) {
    BasicBlock* block = *b;

    // Check inputs to control for this block.
    Node* control = block->control_input_;
    if (control != NULL) {
      CHECK_EQ(block, schedule->block(control));
      CheckInputsDominate(schedule, block, control,
                          static_cast<int>(block->nodes_.size()) - 1);
    }
    // Check inputs for all nodes in the block.
    for (size_t i = 0; i < block->nodes_.size(); i++) {
      Node* node = block->nodes_[i];
      CheckInputsDominate(schedule, block, node, static_cast<int>(i) - 1);
    }
  }
}
}
}
}  // namespace v8::internal::compiler
