// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/graph.h"
#include "src/compiler/loop-analysis.h"
#include "src/compiler/node.h"
#include "src/compiler/node-properties-inl.h"
#include "src/zone.h"

namespace v8 {
namespace internal {
namespace compiler {

typedef uint32_t LoopMarks;


// TODO(titzer): don't assume entry edges have a particular index.
// TODO(titzer): use a BitMatrix to generalize this algorithm.
static const size_t kMaxLoops = 31;
static const int kAssumedLoopEntryIndex = 0;  // assume loops are entered here.
static const LoopMarks kVisited = 1;          // loop #0 is reserved.


// Temporary information for each node during marking.
struct NodeInfo {
  Node* node;
  NodeInfo* next;       // link in chaining loop members
  LoopMarks forward;    // accumulated marks in the forward direction
  LoopMarks backward;   // accumulated marks in the backward direction
  LoopMarks loop_mark;  // loop mark for header nodes; encodes loop_num

  bool MarkBackward(LoopMarks bw) {
    LoopMarks prev = backward;
    LoopMarks next = backward | bw;
    backward = next;
    return prev != next;
  }

  bool MarkForward(LoopMarks fw) {
    LoopMarks prev = forward;
    LoopMarks next = forward | fw;
    forward = next;
    return prev != next;
  }

  bool IsInLoop(size_t loop_num) {
    DCHECK(loop_num > 0 && loop_num <= 31);
    return forward & backward & (1 << loop_num);
  }

  bool IsLoopHeader() { return loop_mark != 0; }
  bool IsInAnyLoop() { return (forward & backward) > kVisited; }

  bool IsInHeaderForLoop(size_t loop_num) {
    DCHECK(loop_num > 0);
    return loop_mark == (kVisited | (1 << loop_num));
  }
};


// Temporary loop info needed during traversal and building the loop tree.
struct LoopInfo {
  Node* header;
  NodeInfo* header_list;
  NodeInfo* body_list;
  LoopTree::Loop* loop;
};


static const NodeInfo kEmptyNodeInfo = {nullptr, nullptr, 0, 0, 0};


// Encapsulation of the loop finding algorithm.
// -----------------------------------------------------------------------------
// Conceptually, the contents of a loop are those nodes that are "between" the
// loop header and the backedges of the loop. Graphs in the soup of nodes can
// form improper cycles, so standard loop finding algorithms that work on CFGs
// aren't sufficient. However, in valid TurboFan graphs, all cycles involve
// either a {Loop} node or a phi. The {Loop} node itself and its accompanying
// phis are treated together as a set referred to here as the loop header.
// This loop finding algorithm works by traversing the graph in two directions,
// first from nodes to their inputs, starting at {end}, then in the reverse
// direction, from nodes to their uses, starting at loop headers.
// 1 bit per loop per node per direction are required during the marking phase.
// To handle nested loops correctly, the algorithm must filter some reachability
// marks on edges into/out-of the loop header nodes.
class LoopFinderImpl {
 public:
  LoopFinderImpl(Graph* graph, LoopTree* loop_tree, Zone* zone)
      : end_(graph->end()),
        queue_(zone),
        queued_(graph, 2),
        info_(graph->NodeCount(), kEmptyNodeInfo, zone),
        loops_(zone),
        loop_tree_(loop_tree),
        loops_found_(0) {}

  void Run() {
    PropagateBackward();
    PropagateForward();
    FinishLoopTree();
  }

  void Print() {
    // Print out the results.
    for (NodeInfo& ni : info_) {
      if (ni.node == nullptr) continue;
      for (size_t i = 1; i <= loops_.size(); i++) {
        if (ni.IsInLoop(i)) {
          PrintF("X");
        } else if (ni.forward & (1 << i)) {
          PrintF("/");
        } else if (ni.backward & (1 << i)) {
          PrintF("\\");
        } else {
          PrintF(" ");
        }
      }
      PrintF(" #%d:%s\n", ni.node->id(), ni.node->op()->mnemonic());
    }

    int i = 0;
    for (LoopInfo& li : loops_) {
      PrintF("Loop %d headed at #%d\n", i, li.header->id());
      i++;
    }

    for (LoopTree::Loop* loop : loop_tree_->outer_loops_) {
      PrintLoop(loop);
    }
  }

 private:
  Node* end_;
  NodeDeque queue_;
  NodeMarker<bool> queued_;
  ZoneVector<NodeInfo> info_;
  ZoneVector<LoopInfo> loops_;
  LoopTree* loop_tree_;
  size_t loops_found_;

  // Propagate marks backward from loop headers.
  void PropagateBackward() {
    PropagateBackward(end_, kVisited);

    while (!queue_.empty()) {
      Node* node = queue_.front();
      queue_.pop_front();
      queued_.Set(node, false);

      // Setup loop headers first.
      if (node->opcode() == IrOpcode::kLoop) {
        // found the loop node first.
        CreateLoopInfo(node);
      } else if (node->opcode() == IrOpcode::kPhi ||
                 node->opcode() == IrOpcode::kEffectPhi) {
        // found a phi first.
        Node* merge = node->InputAt(node->InputCount() - 1);
        if (merge->opcode() == IrOpcode::kLoop) CreateLoopInfo(merge);
      }

      // Propagate reachability marks backwards from this node.
      NodeInfo& ni = info(node);
      if (ni.IsLoopHeader()) {
        // Handle edges from loop header nodes specially.
        for (int i = 0; i < node->InputCount(); i++) {
          if (i == kAssumedLoopEntryIndex) {
            // Don't propagate the loop mark backwards on the entry edge.
            PropagateBackward(node->InputAt(0),
                              kVisited | (ni.backward & ~ni.loop_mark));
          } else {
            // Only propagate the loop mark on backedges.
            PropagateBackward(node->InputAt(i), ni.loop_mark);
          }
        }
      } else {
        // Propagate all loop marks backwards for a normal node.
        for (Node* const input : node->inputs()) {
          PropagateBackward(input, ni.backward);
        }
      }
    }
  }

  // Make a new loop header for the given node.
  void CreateLoopInfo(Node* node) {
    NodeInfo& ni = info(node);
    if (ni.IsLoopHeader()) return;  // loop already set up.

    loops_found_++;
    size_t loop_num = loops_.size() + 1;
    CHECK(loops_found_ <= kMaxLoops);  // TODO(titzer): don't crash.
    // Create a new loop.
    loops_.push_back({node, nullptr, nullptr, nullptr});
    loop_tree_->NewLoop();
    LoopMarks loop_mark = kVisited | (1 << loop_num);
    ni.node = node;
    ni.loop_mark = loop_mark;

    // Setup loop mark for phis attached to loop header.
    for (Node* use : node->uses()) {
      if (use->opcode() == IrOpcode::kPhi ||
          use->opcode() == IrOpcode::kEffectPhi) {
        info(use).loop_mark = loop_mark;
      }
    }
  }

  // Propagate marks forward from loops.
  void PropagateForward() {
    for (LoopInfo& li : loops_) {
      queued_.Set(li.header, true);
      queue_.push_back(li.header);
      NodeInfo& ni = info(li.header);
      ni.forward = ni.loop_mark;
    }
    // Propagate forward on paths that were backward reachable from backedges.
    while (!queue_.empty()) {
      Node* node = queue_.front();
      queue_.pop_front();
      queued_.Set(node, false);
      NodeInfo& ni = info(node);
      for (Edge edge : node->use_edges()) {
        Node* use = edge.from();
        NodeInfo& ui = info(use);
        if (IsBackedge(use, ui, edge)) continue;  // skip backedges.
        LoopMarks both = ni.forward & ui.backward;
        if (ui.MarkForward(both) && !queued_.Get(use)) {
          queued_.Set(use, true);
          queue_.push_back(use);
        }
      }
    }
  }

  bool IsBackedge(Node* use, NodeInfo& ui, Edge& edge) {
    // TODO(titzer): checking for backedges here is ugly.
    if (!ui.IsLoopHeader()) return false;
    if (edge.index() == kAssumedLoopEntryIndex) return false;
    if (use->opcode() == IrOpcode::kPhi ||
        use->opcode() == IrOpcode::kEffectPhi) {
      return !NodeProperties::IsControlEdge(edge);
    }
    return true;
  }

  NodeInfo& info(Node* node) {
    NodeInfo& i = info_[node->id()];
    if (i.node == nullptr) i.node = node;
    return i;
  }

  void PropagateBackward(Node* node, LoopMarks marks) {
    if (info(node).MarkBackward(marks) && !queued_.Get(node)) {
      queue_.push_back(node);
      queued_.Set(node, true);
    }
  }

  void FinishLoopTree() {
    // Degenerate cases.
    if (loops_.size() == 0) return;
    if (loops_.size() == 1) return FinishSingleLoop();

    for (size_t i = 1; i <= loops_.size(); i++) ConnectLoopTree(i);

    size_t count = 0;
    // Place the node into the innermost nested loop of which it is a member.
    for (NodeInfo& ni : info_) {
      if (ni.node == nullptr || !ni.IsInAnyLoop()) continue;

      LoopInfo* innermost = nullptr;
      size_t index = 0;
      for (size_t i = 1; i <= loops_.size(); i++) {
        if (ni.IsInLoop(i)) {
          LoopInfo* loop = &loops_[i - 1];
          if (innermost == nullptr ||
              loop->loop->depth_ > innermost->loop->depth_) {
            innermost = loop;
            index = i;
          }
        }
      }
      if (ni.IsInHeaderForLoop(index)) {
        ni.next = innermost->header_list;
        innermost->header_list = &ni;
      } else {
        ni.next = innermost->body_list;
        innermost->body_list = &ni;
      }
      count++;
    }

    // Serialize the node lists for loops into the loop tree.
    loop_tree_->loop_nodes_.reserve(count);
    for (LoopTree::Loop* loop : loop_tree_->outer_loops_) {
      SerializeLoop(loop);
    }
  }

  // Handle the simpler case of a single loop (no checks for nesting necessary).
  void FinishSingleLoop() {
    DCHECK(loops_.size() == 1);
    DCHECK(loop_tree_->all_loops_.size() == 1);

    // Place nodes into the loop header and body.
    LoopInfo* li = &loops_[0];
    li->loop = &loop_tree_->all_loops_[0];
    loop_tree_->SetParent(nullptr, li->loop);
    size_t count = 0;
    for (NodeInfo& ni : info_) {
      if (ni.node == nullptr || !ni.IsInAnyLoop()) continue;
      DCHECK(ni.IsInLoop(1));
      if (ni.IsInHeaderForLoop(1)) {
        ni.next = li->header_list;
        li->header_list = &ni;
      } else {
        ni.next = li->body_list;
        li->body_list = &ni;
      }
      count++;
    }

    // Serialize the node lists for the loop into the loop tree.
    loop_tree_->loop_nodes_.reserve(count);
    SerializeLoop(li->loop);
  }

  // Recursively serialize the list of header nodes and body nodes
  // so that nested loops occupy nested intervals.
  void SerializeLoop(LoopTree::Loop* loop) {
    size_t loop_num = loop_tree_->LoopNum(loop);
    LoopInfo& li = loops_[loop_num - 1];

    // Serialize the header.
    loop->header_start_ = static_cast<int>(loop_tree_->loop_nodes_.size());
    for (NodeInfo* ni = li.header_list; ni != nullptr; ni = ni->next) {
      loop_tree_->loop_nodes_.push_back(ni->node);
      // TODO(titzer): lift loop count restriction.
      loop_tree_->node_to_loop_num_[ni->node->id()] =
          static_cast<uint8_t>(loop_num);
    }

    // Serialize the body.
    loop->body_start_ = static_cast<int>(loop_tree_->loop_nodes_.size());
    for (NodeInfo* ni = li.body_list; ni != nullptr; ni = ni->next) {
      loop_tree_->loop_nodes_.push_back(ni->node);
      // TODO(titzer): lift loop count restriction.
      loop_tree_->node_to_loop_num_[ni->node->id()] =
          static_cast<uint8_t>(loop_num);
    }

    // Serialize nested loops.
    for (LoopTree::Loop* child : loop->children_) SerializeLoop(child);

    loop->body_end_ = static_cast<int>(loop_tree_->loop_nodes_.size());
  }

  // Connect the LoopTree loops to their parents recursively.
  LoopTree::Loop* ConnectLoopTree(size_t loop_num) {
    LoopInfo& li = loops_[loop_num - 1];
    if (li.loop != nullptr) return li.loop;

    NodeInfo& ni = info(li.header);
    LoopTree::Loop* parent = nullptr;
    for (size_t i = 1; i <= loops_.size(); i++) {
      if (i == loop_num) continue;
      if (ni.IsInLoop(i)) {
        // recursively create potential parent loops first.
        LoopTree::Loop* upper = ConnectLoopTree(i);
        if (parent == nullptr || upper->depth_ > parent->depth_) {
          parent = upper;
        }
      }
    }
    li.loop = &loop_tree_->all_loops_[loop_num - 1];
    loop_tree_->SetParent(parent, li.loop);
    return li.loop;
  }

  void PrintLoop(LoopTree::Loop* loop) {
    for (int i = 0; i < loop->depth_; i++) PrintF("  ");
    PrintF("Loop depth = %d ", loop->depth_);
    int i = loop->header_start_;
    while (i < loop->body_start_) {
      PrintF(" H#%d", loop_tree_->loop_nodes_[i++]->id());
    }
    while (i < loop->body_end_) {
      PrintF(" B#%d", loop_tree_->loop_nodes_[i++]->id());
    }
    PrintF("\n");
    for (LoopTree::Loop* child : loop->children_) PrintLoop(child);
  }
};


LoopTree* LoopFinder::BuildLoopTree(Graph* graph, Zone* zone) {
  LoopTree* loop_tree =
      new (graph->zone()) LoopTree(graph->NodeCount(), graph->zone());
  LoopFinderImpl finder(graph, loop_tree, zone);
  finder.Run();
  if (FLAG_trace_turbo_graph) {
    finder.Print();
  }
  return loop_tree;
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
