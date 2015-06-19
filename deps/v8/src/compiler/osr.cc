// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler.h"
#include "src/compiler/all-nodes.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/control-reducer.h"
#include "src/compiler/frame.h"
#include "src/compiler/graph.h"
#include "src/compiler/graph-visualizer.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/loop-analysis.h"
#include "src/compiler/node.h"
#include "src/compiler/node-marker.h"
#include "src/compiler/osr.h"
#include "src/scopes.h"

namespace v8 {
namespace internal {
namespace compiler {

OsrHelper::OsrHelper(CompilationInfo* info)
    : parameter_count_(info->scope()->num_parameters()),
      stack_slot_count_(info->scope()->num_stack_slots() +
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


// Peel outer loops and rewire the graph so that control reduction can
// produce a properly formed graph.
static void PeelOuterLoopsForOsr(Graph* graph, CommonOperatorBuilder* common,
                                 Zone* tmp_zone, Node* dead,
                                 LoopTree* loop_tree, LoopTree::Loop* osr_loop,
                                 Node* osr_normal_entry, Node* osr_loop_entry) {
  const int original_count = graph->NodeCount();
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
    for (size_t i = 0; i < all.live.size(); i++) {
      Node* orig = all.live[i];
      Node* copy = mapping->at(orig->id());
      if (copy != sentinel) {
        // Mapping already exists.
        continue;
      }
      if (orig->InputCount() == 0 || orig->opcode() == IrOpcode::kParameter ||
          orig->opcode() == IrOpcode::kOsrValue) {
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
        NodeProperties::SetBounds(copy, NodeProperties::GetBounds(orig));
      }
      mapping->at(orig->id()) = copy;
      TRACE(" copy #%d:%s -> #%d\n", orig->id(), orig->op()->mnemonic(),
            copy->id());
    }

    // Fix missing inputs.
    for (Node* orig : all.live) {
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
  Node* end = graph->end();
  tmp_inputs.clear();
  for (int i = -1; i < static_cast<int>(copies.size()); i++) {
    Node* input = end->InputAt(0);
    if (i >= 0) input = copies[i]->at(input->id());
    if (input->opcode() == IrOpcode::kMerge) {
      for (Node* node : input->inputs()) tmp_inputs.push_back(node);
    } else {
      tmp_inputs.push_back(input);
    }
  }
  int count = static_cast<int>(tmp_inputs.size());
  Node* merge = graph->NewNode(common->Merge(count), count, &tmp_inputs[0]);
  end->ReplaceInput(0, merge);

  if (FLAG_trace_turbo_graph) {  // Simple textual RPO.
    OFStream os(stdout);
    os << "-- Graph after OSR duplication -- " << std::endl;
    os << AsRPO(*graph);
  }
}


static void TransferOsrValueTypesFromLoopPhis(Zone* zone, Node* osr_loop_entry,
                                              Node* osr_loop) {
  // Find the index of the osr loop entry into the loop.
  int index = 0;
  for (index = 0; index < osr_loop->InputCount(); index++) {
    if (osr_loop->InputAt(index) == osr_loop_entry) break;
  }
  if (index == osr_loop->InputCount()) return;

  for (Node* osr_value : osr_loop_entry->uses()) {
    if (osr_value->opcode() != IrOpcode::kOsrValue) continue;
    bool unknown = true;
    for (Node* phi : osr_value->uses()) {
      if (phi->opcode() != IrOpcode::kPhi) continue;
      if (NodeProperties::GetControlInput(phi) != osr_loop) continue;
      if (phi->InputAt(index) != osr_value) continue;
      if (NodeProperties::IsTyped(phi)) {
        // Transfer the type from the phi to the OSR value itself.
        Bounds phi_bounds = NodeProperties::GetBounds(phi);
        if (unknown) {
          NodeProperties::SetBounds(osr_value, phi_bounds);
        } else {
          Bounds osr_bounds = NodeProperties::GetBounds(osr_value);
          NodeProperties::SetBounds(osr_value,
                                    Bounds::Both(phi_bounds, osr_bounds, zone));
        }
        unknown = false;
      }
    }
    if (unknown) NodeProperties::SetBounds(osr_value, Bounds::Unbounded(zone));
  }
}


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

  if (osr_loop_entry == nullptr) {
    // No OSR entry found, do nothing.
    CHECK(osr_normal_entry);
    return;
  }

  for (Node* use : osr_loop_entry->uses()) {
    if (use->opcode() == IrOpcode::kLoop) {
      CHECK(!osr_loop);             // should be only one OSR loop.
      osr_loop = use;               // found the OSR loop.
    }
  }

  CHECK(osr_loop);  // Should have found the OSR loop.

  // Transfer the types from loop phis to the OSR values which flow into them.
  TransferOsrValueTypesFromLoopPhis(graph->zone(), osr_loop_entry, osr_loop);

  // Analyze the graph to determine how deeply nested the OSR loop is.
  LoopTree* loop_tree = LoopFinder::BuildLoopTree(graph, tmp_zone);

  Node* dead = jsgraph->DeadControl();
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

  // Normally the control reducer removes loops whose first input is dead,
  // but we need to avoid that because the osr_loop is reachable through
  // the second input, so reduce it and its phis manually.
  osr_loop->ReplaceInput(0, dead);
  Node* node = ControlReducer::ReduceMerge(jsgraph, osr_loop);
  if (node != osr_loop) osr_loop->ReplaceUses(node);

  // Run the normal control reduction, which naturally trims away the dead
  // parts of the graph.
  ControlReducer::ReduceGraph(tmp_zone, jsgraph);
}


void OsrHelper::SetupFrame(Frame* frame) {
  // The optimized frame will subsume the unoptimized frame. Do so by reserving
  // the first spill slots.
  frame->ReserveSpillSlots(UnoptimizedFrameSlots());
  // The frame needs to be adjusted by the number of unoptimized frame slots.
  frame->SetOsrStackSlotCount(static_cast<int>(UnoptimizedFrameSlots()));
}


}  // namespace compiler
}  // namespace internal
}  // namespace v8
