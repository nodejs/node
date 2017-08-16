// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/loop-peeling.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/graph.h"
#include "src/compiler/node-marker.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/node.h"
#include "src/zone/zone.h"

// Loop peeling is an optimization that copies the body of a loop, creating
// a new copy of the body called the "peeled iteration" that represents the
// first iteration. Beginning with a loop as follows:

//             E
//             |                 A
//             |                 |                     (backedges)
//             | +---------------|---------------------------------+
//             | | +-------------|-------------------------------+ |
//             | | |             | +--------+                    | |
//             | | |             | | +----+ |                    | |
//             | | |             | | |    | |                    | |
//           ( Loop )<-------- ( phiA )   | |                    | |
//              |                 |       | |                    | |
//      ((======P=================U=======|=|=====))             | |
//      ((                                | |     ))             | |
//      ((        X <---------------------+ |     ))             | |
//      ((                                  |     ))             | |
//      ((     body                         |     ))             | |
//      ((                                  |     ))             | |
//      ((        Y <-----------------------+     ))             | |
//      ((                                        ))             | |
//      ((===K====L====M==========================))             | |
//           |    |    |                                         | |
//           |    |    +-----------------------------------------+ |
//           |    +------------------------------------------------+
//           |
//          exit

// The body of the loop is duplicated so that all nodes considered "inside"
// the loop (e.g. {P, U, X, Y, K, L, M}) have a corresponding copies in the
// peeled iteration (e.g. {P', U', X', Y', K', L', M'}). What were considered
// backedges of the loop correspond to edges from the peeled iteration to
// the main loop body, with multiple backedges requiring a merge.

// Similarly, any exits from the loop body need to be merged with "exits"
// from the peeled iteration, resulting in the graph as follows:

//             E
//             |                 A
//             |                 |
//      ((=====P'================U'===============))
//      ((                                        ))
//      ((        X'<-------------+               ))
//      ((                        |               ))
//      ((   peeled iteration     |               ))
//      ((                        |               ))
//      ((        Y'<-----------+ |               ))
//      ((                      | |               ))
//      ((===K'===L'====M'======|=|===============))
//           |    |     |       | |
//  +--------+    +-+ +-+       | |
//  |               | |         | |
//  |              Merge <------phi
//  |                |           |
//  |          +-----+           |
//  |          |                 |                     (backedges)
//  |          | +---------------|---------------------------------+
//  |          | | +-------------|-------------------------------+ |
//  |          | | |             | +--------+                    | |
//  |          | | |             | | +----+ |                    | |
//  |          | | |             | | |    | |                    | |
//  |        ( Loop )<-------- ( phiA )   | |                    | |
//  |           |                 |       | |                    | |
//  |   ((======P=================U=======|=|=====))             | |
//  |   ((                                | |     ))             | |
//  |   ((        X <---------------------+ |     ))             | |
//  |   ((                                  |     ))             | |
//  |   ((     body                         |     ))             | |
//  |   ((                                  |     ))             | |
//  |   ((        Y <-----------------------+     ))             | |
//  |   ((                                        ))             | |
//  |   ((===K====L====M==========================))             | |
//  |        |    |    |                                         | |
//  |        |    |    +-----------------------------------------+ |
//  |        |    +------------------------------------------------+
//  |        |
//  |        |
//  +----+ +-+
//       | |
//      Merge
//        |
//      exit

// Note that the boxes ((===)) above are not explicitly represented in the
// graph, but are instead computed by the {LoopFinder}.

namespace v8 {
namespace internal {
namespace compiler {

struct Peeling {
  // Maps a node to its index in the {pairs} vector.
  NodeMarker<size_t> node_map;
  // The vector which contains the mapped nodes.
  NodeVector* pairs;

  Peeling(Graph* graph, Zone* tmp_zone, size_t max, NodeVector* p)
      : node_map(graph, static_cast<uint32_t>(max)), pairs(p) {}

  Node* map(Node* node) {
    if (node_map.Get(node) == 0) return node;
    return pairs->at(node_map.Get(node));
  }

  void Insert(Node* original, Node* copy) {
    node_map.Set(original, 1 + pairs->size());
    pairs->push_back(original);
    pairs->push_back(copy);
  }

  void CopyNodes(Graph* graph, Zone* tmp_zone, Node* dead, NodeRange nodes) {
    NodeVector inputs(tmp_zone);
    // Copy all the nodes first.
    for (Node* node : nodes) {
      inputs.clear();
      for (Node* input : node->inputs()) {
        inputs.push_back(map(input));
      }
      Node* copy = graph->NewNode(node->op(), node->InputCount(), &inputs[0]);
      if (NodeProperties::IsTyped(node)) {
        NodeProperties::SetType(copy, NodeProperties::GetType(node));
      }
      Insert(node, copy);
    }

    // Fix remaining inputs of the copies.
    for (Node* original : nodes) {
      Node* copy = pairs->at(node_map.Get(original));
      for (int i = 0; i < copy->InputCount(); i++) {
        copy->ReplaceInput(i, map(original->InputAt(i)));
      }
    }
  }

  bool Marked(Node* node) { return node_map.Get(node) > 0; }
};


class PeeledIterationImpl : public PeeledIteration {
 public:
  NodeVector node_pairs_;
  explicit PeeledIterationImpl(Zone* zone) : node_pairs_(zone) {}
};


Node* PeeledIteration::map(Node* node) {
  // TODO(turbofan): we use a simple linear search, since the peeled iteration
  // is really only used in testing.
  PeeledIterationImpl* impl = static_cast<PeeledIterationImpl*>(this);
  for (size_t i = 0; i < impl->node_pairs_.size(); i += 2) {
    if (impl->node_pairs_[i] == node) return impl->node_pairs_[i + 1];
  }
  return node;
}

bool LoopPeeler::CanPeel(LoopTree* loop_tree, LoopTree::Loop* loop) {
  // Look for returns and if projections that are outside the loop but whose
  // control input is inside the loop.
  Node* loop_node = loop_tree->GetLoopControl(loop);
  for (Node* node : loop_tree->LoopNodes(loop)) {
    for (Node* use : node->uses()) {
      if (!loop_tree->Contains(loop, use)) {
        bool unmarked_exit;
        switch (node->opcode()) {
          case IrOpcode::kLoopExit:
            unmarked_exit = (node->InputAt(1) != loop_node);
            break;
          case IrOpcode::kLoopExitValue:
          case IrOpcode::kLoopExitEffect:
            unmarked_exit = (node->InputAt(1)->InputAt(1) != loop_node);
            break;
          default:
            unmarked_exit = (use->opcode() != IrOpcode::kTerminate);
        }
        if (unmarked_exit) {
          if (FLAG_trace_turbo_loop) {
            Node* loop_node = loop_tree->GetLoopControl(loop);
            PrintF(
                "Cannot peel loop %i. Loop exit without explicit mark: Node %i "
                "(%s) is inside "
                "loop, but its use %i (%s) is outside.\n",
                loop_node->id(), node->id(), node->op()->mnemonic(), use->id(),
                use->op()->mnemonic());
          }
          return false;
        }
      }
    }
  }
  return true;
}


PeeledIteration* LoopPeeler::Peel(Graph* graph, CommonOperatorBuilder* common,
                                  LoopTree* loop_tree, LoopTree::Loop* loop,
                                  Zone* tmp_zone) {
  if (!CanPeel(loop_tree, loop)) return nullptr;

  //============================================================================
  // Construct the peeled iteration.
  //============================================================================
  PeeledIterationImpl* iter = new (tmp_zone) PeeledIterationImpl(tmp_zone);
  size_t estimated_peeled_size = 5 + (loop->TotalSize()) * 2;
  Peeling peeling(graph, tmp_zone, estimated_peeled_size, &iter->node_pairs_);

  Node* dead = graph->NewNode(common->Dead());

  // Map the loop header nodes to their entry values.
  for (Node* node : loop_tree->HeaderNodes(loop)) {
    peeling.Insert(node, node->InputAt(kAssumedLoopEntryIndex));
  }

  // Copy all the nodes of loop body for the peeled iteration.
  peeling.CopyNodes(graph, tmp_zone, dead, loop_tree->BodyNodes(loop));

  //============================================================================
  // Replace the entry to the loop with the output of the peeled iteration.
  //============================================================================
  Node* loop_node = loop_tree->GetLoopControl(loop);
  Node* new_entry;
  int backedges = loop_node->InputCount() - 1;
  if (backedges > 1) {
    // Multiple backedges from original loop, therefore multiple output edges
    // from the peeled iteration.
    NodeVector inputs(tmp_zone);
    for (int i = 1; i < loop_node->InputCount(); i++) {
      inputs.push_back(peeling.map(loop_node->InputAt(i)));
    }
    Node* merge =
        graph->NewNode(common->Merge(backedges), backedges, &inputs[0]);

    // Merge values from the multiple output edges of the peeled iteration.
    for (Node* node : loop_tree->HeaderNodes(loop)) {
      if (node->opcode() == IrOpcode::kLoop) continue;  // already done.
      inputs.clear();
      for (int i = 0; i < backedges; i++) {
        inputs.push_back(peeling.map(node->InputAt(1 + i)));
      }
      for (Node* input : inputs) {
        if (input != inputs[0]) {  // Non-redundant phi.
          inputs.push_back(merge);
          const Operator* op = common->ResizeMergeOrPhi(node->op(), backedges);
          Node* phi = graph->NewNode(op, backedges + 1, &inputs[0]);
          node->ReplaceInput(0, phi);
          break;
        }
      }
    }
    new_entry = merge;
  } else {
    // Only one backedge, simply replace the input to loop with output of
    // peeling.
    for (Node* node : loop_tree->HeaderNodes(loop)) {
      node->ReplaceInput(0, peeling.map(node->InputAt(1)));
    }
    new_entry = peeling.map(loop_node->InputAt(1));
  }
  loop_node->ReplaceInput(0, new_entry);

  //============================================================================
  // Change the exit and exit markers to merge/phi/effect-phi.
  //============================================================================
  for (Node* exit : loop_tree->ExitNodes(loop)) {
    switch (exit->opcode()) {
      case IrOpcode::kLoopExit:
        // Change the loop exit node to a merge node.
        exit->ReplaceInput(1, peeling.map(exit->InputAt(0)));
        NodeProperties::ChangeOp(exit, common->Merge(2));
        break;
      case IrOpcode::kLoopExitValue:
        // Change exit marker to phi.
        exit->InsertInput(graph->zone(), 1, peeling.map(exit->InputAt(0)));
        NodeProperties::ChangeOp(
            exit, common->Phi(MachineRepresentation::kTagged, 2));
        break;
      case IrOpcode::kLoopExitEffect:
        // Change effect exit marker to effect phi.
        exit->InsertInput(graph->zone(), 1, peeling.map(exit->InputAt(0)));
        NodeProperties::ChangeOp(exit, common->EffectPhi(2));
        break;
      default:
        break;
    }
  }
  return iter;
}

namespace {

void PeelInnerLoops(Graph* graph, CommonOperatorBuilder* common,
                    LoopTree* loop_tree, LoopTree::Loop* loop,
                    Zone* temp_zone) {
  // If the loop has nested loops, peel inside those.
  if (!loop->children().empty()) {
    for (LoopTree::Loop* inner_loop : loop->children()) {
      PeelInnerLoops(graph, common, loop_tree, inner_loop, temp_zone);
    }
    return;
  }
  // Only peel small-enough loops.
  if (loop->TotalSize() > LoopPeeler::kMaxPeeledNodes) return;
  if (FLAG_trace_turbo_loop) {
    PrintF("Peeling loop with header: ");
    for (Node* node : loop_tree->HeaderNodes(loop)) {
      PrintF("%i ", node->id());
    }
    PrintF("\n");
  }

  LoopPeeler::Peel(graph, common, loop_tree, loop, temp_zone);
}

void EliminateLoopExit(Node* node) {
  DCHECK_EQ(IrOpcode::kLoopExit, node->opcode());
  // The exit markers take the loop exit as input. We iterate over uses
  // and remove all the markers from the graph.
  for (Edge edge : node->use_edges()) {
    if (NodeProperties::IsControlEdge(edge)) {
      Node* marker = edge.from();
      if (marker->opcode() == IrOpcode::kLoopExitValue) {
        NodeProperties::ReplaceUses(marker, marker->InputAt(0));
        marker->Kill();
      } else if (marker->opcode() == IrOpcode::kLoopExitEffect) {
        NodeProperties::ReplaceUses(marker, nullptr,
                                    NodeProperties::GetEffectInput(marker));
        marker->Kill();
      }
    }
  }
  NodeProperties::ReplaceUses(node, nullptr, nullptr,
                              NodeProperties::GetControlInput(node, 0));
  node->Kill();
}

}  // namespace

// static
void LoopPeeler::PeelInnerLoopsOfTree(Graph* graph,
                                      CommonOperatorBuilder* common,
                                      LoopTree* loop_tree, Zone* temp_zone) {
  for (LoopTree::Loop* loop : loop_tree->outer_loops()) {
    PeelInnerLoops(graph, common, loop_tree, loop, temp_zone);
  }

  EliminateLoopExits(graph, temp_zone);
}

// static
void LoopPeeler::EliminateLoopExits(Graph* graph, Zone* temp_zone) {
  ZoneQueue<Node*> queue(temp_zone);
  ZoneVector<bool> visited(graph->NodeCount(), false, temp_zone);
  queue.push(graph->end());
  while (!queue.empty()) {
    Node* node = queue.front();
    queue.pop();

    if (node->opcode() == IrOpcode::kLoopExit) {
      Node* control = NodeProperties::GetControlInput(node);
      EliminateLoopExit(node);
      if (!visited[control->id()]) {
        visited[control->id()] = true;
        queue.push(control);
      }
    } else {
      for (int i = 0; i < node->op()->ControlInputCount(); i++) {
        Node* control = NodeProperties::GetControlInput(node, i);
        if (!visited[control->id()]) {
          visited[control->id()] = true;
          queue.push(control);
        }
      }
    }
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
