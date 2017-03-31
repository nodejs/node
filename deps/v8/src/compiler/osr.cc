// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/osr.h"
#include "src/ast/scopes.h"
#include "src/compilation-info.h"
#include "src/compiler/all-nodes.h"
#include "src/compiler/common-operator-reducer.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/dead-code-elimination.h"
#include "src/compiler/frame.h"
#include "src/compiler/graph-reducer.h"
#include "src/compiler/graph-trimmer.h"
#include "src/compiler/graph-visualizer.h"
#include "src/compiler/graph.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/loop-analysis.h"
#include "src/compiler/node-marker.h"
#include "src/compiler/node.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

OsrHelper::OsrHelper(CompilationInfo* info)
    : parameter_count_(
          info->is_optimizing_from_bytecode()
              ? info->shared_info()->bytecode_array()->parameter_count()
              : info->scope()->num_parameters()),
      stack_slot_count_(
          info->is_optimizing_from_bytecode()
              ? info->shared_info()->bytecode_array()->register_count() +
                    InterpreterFrameConstants::kExtraSlotCount
              : info->scope()->num_stack_slots() +
                    info->osr_expr_stack_height()) {}

#ifdef DEBUG
#define TRACE_COND (FLAG_trace_turbo_graph && FLAG_trace_osr)
#else
#define TRACE_COND false
#endif

#define TRACE(...)                       \
  do {                                   \
    if (TRACE_COND) PrintF(__VA_ARGS__); \
  } while (false)

namespace {

// Peel outer loops and rewire the graph so that control reduction can
// produce a properly formed graph.
void PeelOuterLoopsForOsr(Graph* graph, CommonOperatorBuilder* common,
                          Zone* tmp_zone, Node* dead, LoopTree* loop_tree,
                          LoopTree::Loop* osr_loop, Node* osr_normal_entry,
                          Node* osr_loop_entry) {
  const size_t original_count = graph->NodeCount();
  AllNodes all(tmp_zone, graph);
  NodeVector tmp_inputs(tmp_zone);
  Node* sentinel = graph->NewNode(dead->op());

  // Make a copy of the graph for each outer loop.
  ZoneVector<NodeVector*> copies(tmp_zone);
  for (LoopTree::Loop* loop = osr_loop->parent(); loop; loop = loop->parent()) {
    void* stuff = tmp_zone->New(sizeof(NodeVector));
    NodeVector* mapping =
        new (stuff) NodeVector(original_count, sentinel, tmp_zone);
    copies.push_back(mapping);
    TRACE("OsrDuplication #%zu, depth %zu, header #%d:%s\n", copies.size(),
          loop->depth(), loop_tree->HeaderNode(loop)->id(),
          loop_tree->HeaderNode(loop)->op()->mnemonic());

    // Prepare the mapping for OSR values and the OSR loop entry.
    mapping->at(osr_normal_entry->id()) = dead;
    mapping->at(osr_loop_entry->id()) = dead;

    // The outer loops are dead in this copy.
    for (LoopTree::Loop* outer = loop->parent(); outer;
         outer = outer->parent()) {
      for (Node* node : loop_tree->HeaderNodes(outer)) {
        mapping->at(node->id()) = dead;
        TRACE(" ---- #%d:%s -> dead (header)\n", node->id(),
              node->op()->mnemonic());
      }
    }

    // Copy all nodes.
    for (size_t i = 0; i < all.reachable.size(); i++) {
      Node* orig = all.reachable[i];
      Node* copy = mapping->at(orig->id());
      if (copy != sentinel) {
        // Mapping already exists.
        continue;
      }
      if (orig->InputCount() == 0 || orig->opcode() == IrOpcode::kParameter ||
          orig->opcode() == IrOpcode::kOsrValue ||
          orig->opcode() == IrOpcode::kOsrGuard) {
        // No need to copy leaf nodes or parameters.
        mapping->at(orig->id()) = orig;
        continue;
      }

      // Copy the node.
      tmp_inputs.clear();
      for (Node* input : orig->inputs()) {
        tmp_inputs.push_back(mapping->at(input->id()));
      }
      copy = graph->NewNode(orig->op(), orig->InputCount(), &tmp_inputs[0]);
      if (NodeProperties::IsTyped(orig)) {
        NodeProperties::SetType(copy, NodeProperties::GetType(orig));
      }
      mapping->at(orig->id()) = copy;
      TRACE(" copy #%d:%s -> #%d\n", orig->id(), orig->op()->mnemonic(),
            copy->id());
    }

    // Fix missing inputs.
    for (Node* orig : all.reachable) {
      Node* copy = mapping->at(orig->id());
      for (int j = 0; j < copy->InputCount(); j++) {
        if (copy->InputAt(j) == sentinel) {
          copy->ReplaceInput(j, mapping->at(orig->InputAt(j)->id()));
        }
      }
    }

    // Construct the entry into this loop from previous copies.

    // Gather the live loop header nodes, {loop_header} first.
    Node* loop_header = loop_tree->HeaderNode(loop);
    NodeVector header_nodes(tmp_zone);
    header_nodes.reserve(loop->HeaderSize());
    header_nodes.push_back(loop_header);  // put the loop header first.
    for (Node* node : loop_tree->HeaderNodes(loop)) {
      if (node != loop_header && all.IsLive(node)) {
        header_nodes.push_back(node);
      }
    }

    // Gather backedges from the previous copies of the inner loops of {loop}.
    NodeVectorVector backedges(tmp_zone);
    TRACE("Gathering backedges...\n");
    for (int i = 1; i < loop_header->InputCount(); i++) {
      if (TRACE_COND) {
        Node* control = loop_header->InputAt(i);
        size_t incoming_depth = 0;
        for (int j = 0; j < control->op()->ControlInputCount(); j++) {
          Node* k = NodeProperties::GetControlInput(control, j);
          incoming_depth =
              std::max(incoming_depth, loop_tree->ContainingLoop(k)->depth());
        }

        TRACE(" edge @%d #%d:%s, incoming depth %zu\n", i, control->id(),
              control->op()->mnemonic(), incoming_depth);
      }

      for (int pos = static_cast<int>(copies.size()) - 1; pos >= 0; pos--) {
        backedges.push_back(NodeVector(tmp_zone));
        backedges.back().reserve(header_nodes.size());

        NodeVector* previous_map = pos > 0 ? copies[pos - 1] : nullptr;

        for (Node* node : header_nodes) {
          Node* input = node->InputAt(i);
          if (previous_map) input = previous_map->at(input->id());
          backedges.back().push_back(input);
          TRACE("   node #%d:%s(@%d) = #%d:%s\n", node->id(),
                node->op()->mnemonic(), i, input->id(),
                input->op()->mnemonic());
        }
      }
    }

    int backedge_count = static_cast<int>(backedges.size());
    if (backedge_count == 1) {
      // Simple case of single backedge, therefore a single entry.
      int index = 0;
      for (Node* node : header_nodes) {
        Node* copy = mapping->at(node->id());
        Node* input = backedges[0][index];
        copy->ReplaceInput(0, input);
        TRACE(" header #%d:%s(0) => #%d:%s\n", copy->id(),
              copy->op()->mnemonic(), input->id(), input->op()->mnemonic());
        index++;
      }
    } else {
      // Complex case of multiple backedges from previous copies requires
      // merging the backedges to create the entry into the loop header.
      Node* merge = nullptr;
      int index = 0;
      for (Node* node : header_nodes) {
        // Gather edge inputs into {tmp_inputs}.
        tmp_inputs.clear();
        for (int edge = 0; edge < backedge_count; edge++) {
          tmp_inputs.push_back(backedges[edge][index]);
        }
        Node* copy = mapping->at(node->id());
        Node* input;
        if (node == loop_header) {
          // Create the merge for the entry into the loop header.
          input = merge = graph->NewNode(common->Merge(backedge_count),
                                         backedge_count, &tmp_inputs[0]);
          copy->ReplaceInput(0, merge);
        } else {
          // Create a phi that merges values at entry into the loop header.
          DCHECK_NOT_NULL(merge);
          DCHECK(IrOpcode::IsPhiOpcode(node->opcode()));
          tmp_inputs.push_back(merge);
          Node* phi = input = graph->NewNode(
              common->ResizeMergeOrPhi(node->op(), backedge_count),
              backedge_count + 1, &tmp_inputs[0]);
          copy->ReplaceInput(0, phi);
        }

        // Print the merge.
        if (TRACE_COND) {
          TRACE(" header #%d:%s(0) => #%d:%s(", copy->id(),
                copy->op()->mnemonic(), input->id(), input->op()->mnemonic());
          for (size_t i = 0; i < tmp_inputs.size(); i++) {
            if (i > 0) TRACE(", ");
            Node* input = tmp_inputs[i];
            TRACE("#%d:%s", input->id(), input->op()->mnemonic());
          }
          TRACE(")\n");
        }

        index++;
      }
    }
  }

  // Kill the outer loops in the original graph.
  TRACE("Killing outer loop headers...\n");
  for (LoopTree::Loop* outer = osr_loop->parent(); outer;
       outer = outer->parent()) {
    Node* loop_header = loop_tree->HeaderNode(outer);
    loop_header->ReplaceUses(dead);
    TRACE(" ---- #%d:%s\n", loop_header->id(), loop_header->op()->mnemonic());
  }

  // Merge the ends of the graph copies.
  Node* const end = graph->end();
  int const input_count = end->InputCount();
  for (int i = 0; i < input_count; ++i) {
    NodeId const id = end->InputAt(i)->id();
    for (NodeVector* const copy : copies) {
      end->AppendInput(graph->zone(), copy->at(id));
      NodeProperties::ChangeOp(end, common->End(end->InputCount()));
    }
  }

  if (FLAG_trace_turbo_graph) {  // Simple textual RPO.
    OFStream os(stdout);
    os << "-- Graph after OSR duplication -- " << std::endl;
    os << AsRPO(*graph);
  }
}

void SetTypeForOsrValue(Node* osr_value, Node* loop,
                        CommonOperatorBuilder* common) {
  Node* osr_guard = nullptr;
  for (Node* use : osr_value->uses()) {
    if (use->opcode() == IrOpcode::kOsrGuard) {
      DCHECK_EQ(use->InputAt(0), osr_value);
      osr_guard = use;
      break;
    }
  }

  NodeProperties::ChangeOp(osr_guard, common->OsrGuard(OsrGuardType::kAny));
}

}  // namespace

void OsrHelper::Deconstruct(JSGraph* jsgraph, CommonOperatorBuilder* common,
                            Zone* tmp_zone) {
  Graph* graph = jsgraph->graph();
  Node* osr_normal_entry = nullptr;
  Node* osr_loop_entry = nullptr;
  Node* osr_loop = nullptr;

  for (Node* node : graph->start()->uses()) {
    if (node->opcode() == IrOpcode::kOsrLoopEntry) {
      osr_loop_entry = node;  // found the OSR loop entry
    } else if (node->opcode() == IrOpcode::kOsrNormalEntry) {
      osr_normal_entry = node;
    }
  }

  CHECK_NOT_NULL(osr_normal_entry);  // Should have found the OSR normal entry.
  CHECK_NOT_NULL(osr_loop_entry);    // Should have found the OSR loop entry.

  for (Node* use : osr_loop_entry->uses()) {
    if (use->opcode() == IrOpcode::kLoop) {
      CHECK(!osr_loop);             // should be only one OSR loop.
      osr_loop = use;               // found the OSR loop.
    }
  }

  CHECK(osr_loop);  // Should have found the OSR loop.

  for (Node* use : osr_loop_entry->uses()) {
    if (use->opcode() == IrOpcode::kOsrValue) {
      SetTypeForOsrValue(use, osr_loop, common);
    }
  }

  // Analyze the graph to determine how deeply nested the OSR loop is.
  LoopTree* loop_tree = LoopFinder::BuildLoopTree(graph, tmp_zone);

  Node* dead = jsgraph->Dead();
  LoopTree::Loop* loop = loop_tree->ContainingLoop(osr_loop);
  if (loop->depth() > 0) {
    PeelOuterLoopsForOsr(graph, common, tmp_zone, dead, loop_tree, loop,
                         osr_normal_entry, osr_loop_entry);
  }

  // Replace the normal entry with {Dead} and the loop entry with {Start}
  // and run the control reducer to clean up the graph.
  osr_normal_entry->ReplaceUses(dead);
  osr_normal_entry->Kill();
  osr_loop_entry->ReplaceUses(graph->start());
  osr_loop_entry->Kill();

  // Remove the first input to the {osr_loop}.
  int const live_input_count = osr_loop->InputCount() - 1;
  CHECK_NE(0, live_input_count);
  for (Node* const use : osr_loop->uses()) {
    if (NodeProperties::IsPhi(use)) {
      use->RemoveInput(0);
      NodeProperties::ChangeOp(
          use, common->ResizeMergeOrPhi(use->op(), live_input_count));
    }
  }
  osr_loop->RemoveInput(0);
  NodeProperties::ChangeOp(
      osr_loop, common->ResizeMergeOrPhi(osr_loop->op(), live_input_count));

  // Run control reduction and graph trimming.
  // TODO(bmeurer): The OSR deconstruction could be a regular reducer and play
  // nice together with the rest, instead of having this custom stuff here.
  GraphReducer graph_reducer(tmp_zone, graph);
  DeadCodeElimination dce(&graph_reducer, graph, common);
  CommonOperatorReducer cor(&graph_reducer, graph, common, jsgraph->machine());
  graph_reducer.AddReducer(&dce);
  graph_reducer.AddReducer(&cor);
  graph_reducer.ReduceGraph();
  GraphTrimmer trimmer(tmp_zone, graph);
  NodeVector roots(tmp_zone);
  jsgraph->GetCachedNodes(&roots);
  trimmer.TrimGraph(roots.begin(), roots.end());
}


void OsrHelper::SetupFrame(Frame* frame) {
  // The optimized frame will subsume the unoptimized frame. Do so by reserving
  // the first spill slots.
  frame->ReserveSpillSlots(UnoptimizedFrameSlots());
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
