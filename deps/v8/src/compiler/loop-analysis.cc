// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/loop-analysis.h"

#include "src/compiler/graph.h"
#include "src/compiler/node.h"
#include "src/compiler/node-marker.h"
#include "src/compiler/node-properties.h"
#include "src/zone.h"

namespace v8 {
namespace internal {
namespace compiler {

#define OFFSET(x) ((x)&0x1f)
#define BIT(x) (1u << OFFSET(x))
#define INDEX(x) ((x) >> 5)

// Temporary information for each node during marking.
struct NodeInfo {
  Node* node;
  NodeInfo* next;       // link in chaining loop members
};


// Temporary loop info needed during traversal and building the loop tree.
struct LoopInfo {
  Node* header;
  NodeInfo* header_list;
  NodeInfo* body_list;
  LoopTree::Loop* loop;
};


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
      : zone_(zone),
        end_(graph->end()),
        queue_(zone),
        queued_(graph, 2),
        info_(graph->NodeCount(), {nullptr, nullptr}, zone),
        loops_(zone),
        loop_num_(graph->NodeCount(), -1, zone),
        loop_tree_(loop_tree),
        loops_found_(0),
        width_(0),
        backward_(nullptr),
        forward_(nullptr) {}

  void Run() {
    PropagateBackward();
    PropagateForward();
    FinishLoopTree();
  }

  void Print() {
    // Print out the results.
    for (NodeInfo& ni : info_) {
      if (ni.node == nullptr) continue;
      for (int i = 1; i <= loops_found_; i++) {
        int index = ni.node->id() * width_ + INDEX(i);
        bool marked_forward = forward_[index] & BIT(i);
        bool marked_backward = backward_[index] & BIT(i);
        if (marked_forward && marked_backward) {
          PrintF("X");
        } else if (marked_forward) {
          PrintF("/");
        } else if (marked_backward) {
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
  Zone* zone_;
  Node* end_;
  NodeDeque queue_;
  NodeMarker<bool> queued_;
  ZoneVector<NodeInfo> info_;
  ZoneVector<LoopInfo> loops_;
  ZoneVector<int> loop_num_;
  LoopTree* loop_tree_;
  int loops_found_;
  int width_;
  uint32_t* backward_;
  uint32_t* forward_;

  int num_nodes() {
    return static_cast<int>(loop_tree_->node_to_loop_num_.size());
  }

  // Tb = Tb | (Fb - loop_filter)
  bool PropagateBackwardMarks(Node* from, Node* to, int loop_filter) {
    if (from == to) return false;
    uint32_t* fp = &backward_[from->id() * width_];
    uint32_t* tp = &backward_[to->id() * width_];
    bool change = false;
    for (int i = 0; i < width_; i++) {
      uint32_t mask = i == INDEX(loop_filter) ? ~BIT(loop_filter) : 0xFFFFFFFF;
      uint32_t prev = tp[i];
      uint32_t next = prev | (fp[i] & mask);
      tp[i] = next;
      if (!change && (prev != next)) change = true;
    }
    return change;
  }

  // Tb = Tb | B
  bool SetBackwardMark(Node* to, int loop_num) {
    uint32_t* tp = &backward_[to->id() * width_ + INDEX(loop_num)];
    uint32_t prev = tp[0];
    uint32_t next = prev | BIT(loop_num);
    tp[0] = next;
    return next != prev;
  }

  // Tf = Tf | B
  bool SetForwardMark(Node* to, int loop_num) {
    uint32_t* tp = &forward_[to->id() * width_ + INDEX(loop_num)];
    uint32_t prev = tp[0];
    uint32_t next = prev | BIT(loop_num);
    tp[0] = next;
    return next != prev;
  }

  // Tf = Tf | (Ff & Tb)
  bool PropagateForwardMarks(Node* from, Node* to) {
    if (from == to) return false;
    bool change = false;
    int findex = from->id() * width_;
    int tindex = to->id() * width_;
    for (int i = 0; i < width_; i++) {
      uint32_t marks = backward_[tindex + i] & forward_[findex + i];
      uint32_t prev = forward_[tindex + i];
      uint32_t next = prev | marks;
      forward_[tindex + i] = next;
      if (!change && (prev != next)) change = true;
    }
    return change;
  }

  bool IsInLoop(Node* node, int loop_num) {
    int offset = node->id() * width_ + INDEX(loop_num);
    return backward_[offset] & forward_[offset] & BIT(loop_num);
  }

  // Propagate marks backward from loop headers.
  void PropagateBackward() {
    ResizeBackwardMarks();
    SetBackwardMark(end_, 0);
    Queue(end_);

    while (!queue_.empty()) {
      Node* node = queue_.front();
      info(node);
      queue_.pop_front();
      queued_.Set(node, false);

      int loop_num = -1;
      // Setup loop headers first.
      if (node->opcode() == IrOpcode::kLoop) {
        // found the loop node first.
        loop_num = CreateLoopInfo(node);
      } else if (NodeProperties::IsPhi(node)) {
        // found a phi first.
        Node* merge = node->InputAt(node->InputCount() - 1);
        if (merge->opcode() == IrOpcode::kLoop) {
          loop_num = CreateLoopInfo(merge);
        }
      }

      // Propagate marks backwards from this node.
      for (int i = 0; i < node->InputCount(); i++) {
        Node* input = node->InputAt(i);
        if (loop_num > 0 && i != kAssumedLoopEntryIndex) {
          // Only propagate the loop mark on backedges.
          if (SetBackwardMark(input, loop_num)) Queue(input);
        } else {
          // Entry or normal edge. Propagate all marks except loop_num.
          if (PropagateBackwardMarks(node, input, loop_num)) Queue(input);
        }
      }
    }
  }

  // Make a new loop if necessary for the given node.
  int CreateLoopInfo(Node* node) {
    int loop_num = LoopNum(node);
    if (loop_num > 0) return loop_num;

    loop_num = ++loops_found_;
    if (INDEX(loop_num) >= width_) ResizeBackwardMarks();

    // Create a new loop.
    loops_.push_back({node, nullptr, nullptr, nullptr});
    loop_tree_->NewLoop();
    SetBackwardMark(node, loop_num);
    loop_tree_->node_to_loop_num_[node->id()] = loop_num;

    // Setup loop mark for phis attached to loop header.
    for (Node* use : node->uses()) {
      if (NodeProperties::IsPhi(use)) {
        info(use);  // create the NodeInfo
        SetBackwardMark(use, loop_num);
        loop_tree_->node_to_loop_num_[use->id()] = loop_num;
      }
    }

    return loop_num;
  }

  void ResizeBackwardMarks() {
    int new_width = width_ + 1;
    int max = num_nodes();
    uint32_t* new_backward = zone_->NewArray<uint32_t>(new_width * max);
    memset(new_backward, 0, new_width * max * sizeof(uint32_t));
    if (width_ > 0) {  // copy old matrix data.
      for (int i = 0; i < max; i++) {
        uint32_t* np = &new_backward[i * new_width];
        uint32_t* op = &backward_[i * width_];
        for (int j = 0; j < width_; j++) np[j] = op[j];
      }
    }
    width_ = new_width;
    backward_ = new_backward;
  }

  void ResizeForwardMarks() {
    int max = num_nodes();
    forward_ = zone_->NewArray<uint32_t>(width_ * max);
    memset(forward_, 0, width_ * max * sizeof(uint32_t));
  }

  // Propagate marks forward from loops.
  void PropagateForward() {
    ResizeForwardMarks();
    for (LoopInfo& li : loops_) {
      SetForwardMark(li.header, LoopNum(li.header));
      Queue(li.header);
    }
    // Propagate forward on paths that were backward reachable from backedges.
    while (!queue_.empty()) {
      Node* node = queue_.front();
      queue_.pop_front();
      queued_.Set(node, false);
      for (Edge edge : node->use_edges()) {
        Node* use = edge.from();
        if (!IsBackedge(use, edge)) {
          if (PropagateForwardMarks(node, use)) Queue(use);
        }
      }
    }
  }

  bool IsBackedge(Node* use, Edge& edge) {
    if (LoopNum(use) <= 0) return false;
    if (edge.index() == kAssumedLoopEntryIndex) return false;
    if (NodeProperties::IsPhi(use)) {
      return !NodeProperties::IsControlEdge(edge);
    }
    return true;
  }

  int LoopNum(Node* node) { return loop_tree_->node_to_loop_num_[node->id()]; }

  NodeInfo& info(Node* node) {
    NodeInfo& i = info_[node->id()];
    if (i.node == nullptr) i.node = node;
    return i;
  }

  void Queue(Node* node) {
    if (!queued_.Get(node)) {
      queue_.push_back(node);
      queued_.Set(node, true);
    }
  }

  void FinishLoopTree() {
    DCHECK(loops_found_ == static_cast<int>(loops_.size()));
    DCHECK(loops_found_ == static_cast<int>(loop_tree_->all_loops_.size()));

    // Degenerate cases.
    if (loops_found_ == 0) return;
    if (loops_found_ == 1) return FinishSingleLoop();

    for (int i = 1; i <= loops_found_; i++) ConnectLoopTree(i);

    size_t count = 0;
    // Place the node into the innermost nested loop of which it is a member.
    for (NodeInfo& ni : info_) {
      if (ni.node == nullptr) continue;

      LoopInfo* innermost = nullptr;
      int innermost_index = 0;
      int pos = ni.node->id() * width_;
      // Search the marks word by word.
      for (int i = 0; i < width_; i++) {
        uint32_t marks = backward_[pos + i] & forward_[pos + i];
        for (int j = 0; j < 32; j++) {
          if (marks & (1u << j)) {
            int loop_num = i * 32 + j;
            if (loop_num == 0) continue;
            LoopInfo* loop = &loops_[loop_num - 1];
            if (innermost == nullptr ||
                loop->loop->depth_ > innermost->loop->depth_) {
              innermost = loop;
              innermost_index = loop_num;
            }
          }
        }
      }
      if (innermost == nullptr) continue;
      if (LoopNum(ni.node) == innermost_index) {
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
    // Place nodes into the loop header and body.
    LoopInfo* li = &loops_[0];
    li->loop = &loop_tree_->all_loops_[0];
    loop_tree_->SetParent(nullptr, li->loop);
    size_t count = 0;
    for (NodeInfo& ni : info_) {
      if (ni.node == nullptr || !IsInLoop(ni.node, 1)) continue;
      if (LoopNum(ni.node) == 1) {
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
    int loop_num = loop_tree_->LoopNum(loop);
    LoopInfo& li = loops_[loop_num - 1];

    // Serialize the header.
    loop->header_start_ = static_cast<int>(loop_tree_->loop_nodes_.size());
    for (NodeInfo* ni = li.header_list; ni != nullptr; ni = ni->next) {
      loop_tree_->loop_nodes_.push_back(ni->node);
      loop_tree_->node_to_loop_num_[ni->node->id()] = loop_num;
    }

    // Serialize the body.
    loop->body_start_ = static_cast<int>(loop_tree_->loop_nodes_.size());
    for (NodeInfo* ni = li.body_list; ni != nullptr; ni = ni->next) {
      loop_tree_->loop_nodes_.push_back(ni->node);
      loop_tree_->node_to_loop_num_[ni->node->id()] = loop_num;
    }

    // Serialize nested loops.
    for (LoopTree::Loop* child : loop->children_) SerializeLoop(child);

    loop->body_end_ = static_cast<int>(loop_tree_->loop_nodes_.size());
  }

  // Connect the LoopTree loops to their parents recursively.
  LoopTree::Loop* ConnectLoopTree(int loop_num) {
    LoopInfo& li = loops_[loop_num - 1];
    if (li.loop != nullptr) return li.loop;

    NodeInfo& ni = info(li.header);
    LoopTree::Loop* parent = nullptr;
    for (int i = 1; i <= loops_found_; i++) {
      if (i == loop_num) continue;
      if (IsInLoop(ni.node, i)) {
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


Node* LoopTree::HeaderNode(Loop* loop) {
  Node* first = *HeaderNodes(loop).begin();
  if (first->opcode() == IrOpcode::kLoop) return first;
  DCHECK(IrOpcode::IsPhiOpcode(first->opcode()));
  Node* header = NodeProperties::GetControlInput(first);
  DCHECK_EQ(IrOpcode::kLoop, header->opcode());
  return header;
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
