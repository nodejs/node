// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_CONTROL_EQUIVALENCE_H_
#define V8_COMPILER_CONTROL_EQUIVALENCE_H_

#include "src/v8.h"

#include "src/compiler/graph.h"
#include "src/compiler/node.h"
#include "src/compiler/node-properties.h"
#include "src/zone-containers.h"

namespace v8 {
namespace internal {
namespace compiler {

// Determines control dependence equivalence classes for control nodes. Any two
// nodes having the same set of control dependences land in one class. These
// classes can in turn be used to:
//  - Build a program structure tree (PST) for controls in the graph.
//  - Determine single-entry single-exit (SESE) regions within the graph.
//
// Note that this implementation actually uses cycle equivalence to establish
// class numbers. Any two nodes are cycle equivalent if they occur in the same
// set of cycles. It can be shown that control dependence equivalence reduces
// to undirected cycle equivalence for strongly connected control flow graphs.
//
// The algorithm is based on the paper, "The program structure tree: computing
// control regions in linear time" by Johnson, Pearson & Pingali (PLDI94) which
// also contains proofs for the aforementioned equivalence. References to line
// numbers in the algorithm from figure 4 have been added [line:x].
class ControlEquivalence : public ZoneObject {
 public:
  ControlEquivalence(Zone* zone, Graph* graph)
      : zone_(zone),
        graph_(graph),
        dfs_number_(0),
        class_number_(1),
        node_data_(graph->NodeCount(), EmptyData(), zone) {}

  // Run the main algorithm starting from the {exit} control node. This causes
  // the following iterations over control edges of the graph:
  //  1) A breadth-first backwards traversal to determine the set of nodes that
  //     participate in the next step. Takes O(E) time and O(N) space.
  //  2) An undirected depth-first backwards traversal that determines class
  //     numbers for all participating nodes. Takes O(E) time and O(N) space.
  void Run(Node* exit) {
    if (GetClass(exit) != kInvalidClass) return;
    DetermineParticipation(exit);
    RunUndirectedDFS(exit);
  }

  // Retrieves a previously computed class number.
  size_t ClassOf(Node* node) {
    DCHECK(GetClass(node) != kInvalidClass);
    return GetClass(node);
  }

 private:
  static const size_t kInvalidClass = static_cast<size_t>(-1);
  typedef enum { kInputDirection, kUseDirection } DFSDirection;

  struct Bracket {
    DFSDirection direction;  // Direction in which this bracket was added.
    size_t recent_class;     // Cached class when bracket was topmost.
    size_t recent_size;      // Cached set-size when bracket was topmost.
    Node* from;              // Node that this bracket originates from.
    Node* to;                // Node that this bracket points to.
  };

  // The set of brackets for each node during the DFS walk.
  typedef ZoneLinkedList<Bracket> BracketList;

  struct DFSStackEntry {
    DFSDirection direction;            // Direction currently used in DFS walk.
    Node::InputEdges::iterator input;  // Iterator used for "input" direction.
    Node::UseEdges::iterator use;      // Iterator used for "use" direction.
    Node* parent_node;                 // Parent node of entry during DFS walk.
    Node* node;                        // Node that this stack entry belongs to.
  };

  // The stack is used during the undirected DFS walk.
  typedef ZoneStack<DFSStackEntry> DFSStack;

  struct NodeData {
    size_t class_number;  // Equivalence class number assigned to node.
    size_t dfs_number;    // Pre-order DFS number assigned to node.
    bool visited;         // Indicates node has already been visited.
    bool on_stack;        // Indicates node is on DFS stack during walk.
    bool participates;    // Indicates node participates in DFS walk.
    BracketList blist;    // List of brackets per node.
  };

  // The per-node data computed during the DFS walk.
  typedef ZoneVector<NodeData> Data;

  // Called at pre-visit during DFS walk.
  void VisitPre(Node* node) {
    Trace("CEQ: Pre-visit of #%d:%s\n", node->id(), node->op()->mnemonic());

    // Dispense a new pre-order number.
    SetNumber(node, NewDFSNumber());
    Trace("  Assigned DFS number is %d\n", GetNumber(node));
  }

  // Called at mid-visit during DFS walk.
  void VisitMid(Node* node, DFSDirection direction) {
    Trace("CEQ: Mid-visit of #%d:%s\n", node->id(), node->op()->mnemonic());
    BracketList& blist = GetBracketList(node);

    // Remove brackets pointing to this node [line:19].
    BracketListDelete(blist, node, direction);

    // Potentially introduce artificial dependency from start to end.
    if (blist.empty()) {
      DCHECK_EQ(kInputDirection, direction);
      VisitBackedge(node, graph_->end(), kInputDirection);
    }

    // Potentially start a new equivalence class [line:37].
    BracketListTrace(blist);
    Bracket* recent = &blist.back();
    if (recent->recent_size != blist.size()) {
      recent->recent_size = blist.size();
      recent->recent_class = NewClassNumber();
    }

    // Assign equivalence class to node.
    SetClass(node, recent->recent_class);
    Trace("  Assigned class number is %d\n", GetClass(node));
  }

  // Called at post-visit during DFS walk.
  void VisitPost(Node* node, Node* parent_node, DFSDirection direction) {
    Trace("CEQ: Post-visit of #%d:%s\n", node->id(), node->op()->mnemonic());
    BracketList& blist = GetBracketList(node);

    // Remove brackets pointing to this node [line:19].
    BracketListDelete(blist, node, direction);

    // Propagate bracket list up the DFS tree [line:13].
    if (parent_node != NULL) {
      BracketList& parent_blist = GetBracketList(parent_node);
      parent_blist.splice(parent_blist.end(), blist);
    }
  }

  // Called when hitting a back edge in the DFS walk.
  void VisitBackedge(Node* from, Node* to, DFSDirection direction) {
    Trace("CEQ: Backedge from #%d:%s to #%d:%s\n", from->id(),
          from->op()->mnemonic(), to->id(), to->op()->mnemonic());

    // Push backedge onto the bracket list [line:25].
    Bracket bracket = {direction, kInvalidClass, 0, from, to};
    GetBracketList(from).push_back(bracket);
  }

  // Performs and undirected DFS walk of the graph. Conceptually all nodes are
  // expanded, splitting "input" and "use" out into separate nodes. During the
  // traversal, edges towards the representative nodes are preferred.
  //
  //   \ /        - Pre-visit: When N1 is visited in direction D the preferred
  //    x   N1      edge towards N is taken next, calling VisitPre(N).
  //    |         - Mid-visit: After all edges out of N2 in direction D have
  //    |   N       been visited, we switch the direction and start considering
  //    |           edges out of N1 now, and we call VisitMid(N).
  //    x   N2    - Post-visit: After all edges out of N1 in direction opposite
  //   / \          to D have been visited, we pop N and call VisitPost(N).
  //
  // This will yield a true spanning tree (without cross or forward edges) and
  // also discover proper back edges in both directions.
  void RunUndirectedDFS(Node* exit) {
    ZoneStack<DFSStackEntry> stack(zone_);
    DFSPush(stack, exit, NULL, kInputDirection);
    VisitPre(exit);

    while (!stack.empty()) {  // Undirected depth-first backwards traversal.
      DFSStackEntry& entry = stack.top();
      Node* node = entry.node;

      if (entry.direction == kInputDirection) {
        if (entry.input != node->input_edges().end()) {
          Edge edge = *entry.input;
          Node* input = edge.to();
          ++(entry.input);
          if (NodeProperties::IsControlEdge(edge) &&
              NodeProperties::IsControl(input)) {
            // Visit next control input.
            if (!GetData(input)->participates) continue;
            if (GetData(input)->visited) continue;
            if (GetData(input)->on_stack) {
              // Found backedge if input is on stack.
              if (input != entry.parent_node) {
                VisitBackedge(node, input, kInputDirection);
              }
            } else {
              // Push input onto stack.
              DFSPush(stack, input, node, kInputDirection);
              VisitPre(input);
            }
          }
          continue;
        }
        if (entry.use != node->use_edges().end()) {
          // Switch direction to uses.
          entry.direction = kUseDirection;
          VisitMid(node, kInputDirection);
          continue;
        }
      }

      if (entry.direction == kUseDirection) {
        if (entry.use != node->use_edges().end()) {
          Edge edge = *entry.use;
          Node* use = edge.from();
          ++(entry.use);
          if (NodeProperties::IsControlEdge(edge) &&
              NodeProperties::IsControl(use)) {
            // Visit next control use.
            if (!GetData(use)->participates) continue;
            if (GetData(use)->visited) continue;
            if (GetData(use)->on_stack) {
              // Found backedge if use is on stack.
              if (use != entry.parent_node) {
                VisitBackedge(node, use, kUseDirection);
              }
            } else {
              // Push use onto stack.
              DFSPush(stack, use, node, kUseDirection);
              VisitPre(use);
            }
          }
          continue;
        }
        if (entry.input != node->input_edges().end()) {
          // Switch direction to inputs.
          entry.direction = kInputDirection;
          VisitMid(node, kUseDirection);
          continue;
        }
      }

      // Pop node from stack when done with all inputs and uses.
      DCHECK(entry.input == node->input_edges().end());
      DCHECK(entry.use == node->use_edges().end());
      DFSPop(stack, node);
      VisitPost(node, entry.parent_node, entry.direction);
    }
  }

  void DetermineParticipationEnqueue(ZoneQueue<Node*>& queue, Node* node) {
    if (!GetData(node)->participates) {
      GetData(node)->participates = true;
      queue.push(node);
    }
  }

  void DetermineParticipation(Node* exit) {
    ZoneQueue<Node*> queue(zone_);
    DetermineParticipationEnqueue(queue, exit);
    while (!queue.empty()) {  // Breadth-first backwards traversal.
      Node* node = queue.front();
      queue.pop();
      int max = NodeProperties::PastControlIndex(node);
      for (int i = NodeProperties::FirstControlIndex(node); i < max; i++) {
        DetermineParticipationEnqueue(queue, node->InputAt(i));
      }
    }
  }

 private:
  NodeData* GetData(Node* node) { return &node_data_[node->id()]; }
  int NewClassNumber() { return class_number_++; }
  int NewDFSNumber() { return dfs_number_++; }

  // Template used to initialize per-node data.
  NodeData EmptyData() {
    return {kInvalidClass, 0, false, false, false, BracketList(zone_)};
  }

  // Accessors for the DFS number stored within the per-node data.
  size_t GetNumber(Node* node) { return GetData(node)->dfs_number; }
  void SetNumber(Node* node, size_t number) {
    GetData(node)->dfs_number = number;
  }

  // Accessors for the equivalence class stored within the per-node data.
  size_t GetClass(Node* node) { return GetData(node)->class_number; }
  void SetClass(Node* node, size_t number) {
    GetData(node)->class_number = number;
  }

  // Accessors for the bracket list stored within the per-node data.
  BracketList& GetBracketList(Node* node) { return GetData(node)->blist; }
  void SetBracketList(Node* node, BracketList& list) {
    GetData(node)->blist = list;
  }

  // Mutates the DFS stack by pushing an entry.
  void DFSPush(DFSStack& stack, Node* node, Node* from, DFSDirection dir) {
    DCHECK(GetData(node)->participates);
    DCHECK(!GetData(node)->visited);
    GetData(node)->on_stack = true;
    Node::InputEdges::iterator input = node->input_edges().begin();
    Node::UseEdges::iterator use = node->use_edges().begin();
    stack.push({dir, input, use, from, node});
  }

  // Mutates the DFS stack by popping an entry.
  void DFSPop(DFSStack& stack, Node* node) {
    DCHECK_EQ(stack.top().node, node);
    GetData(node)->on_stack = false;
    GetData(node)->visited = true;
    stack.pop();
  }

  // TODO(mstarzinger): Optimize this to avoid linear search.
  void BracketListDelete(BracketList& blist, Node* to, DFSDirection direction) {
    for (BracketList::iterator i = blist.begin(); i != blist.end(); /*nop*/) {
      if (i->to == to && i->direction != direction) {
        Trace("  BList erased: {%d->%d}\n", i->from->id(), i->to->id());
        i = blist.erase(i);
      } else {
        ++i;
      }
    }
  }

  void BracketListTrace(BracketList& blist) {
    if (FLAG_trace_turbo_scheduler) {
      Trace("  BList: ");
      for (Bracket bracket : blist) {
        Trace("{%d->%d} ", bracket.from->id(), bracket.to->id());
      }
      Trace("\n");
    }
  }

  void Trace(const char* msg, ...) {
    if (FLAG_trace_turbo_scheduler) {
      va_list arguments;
      va_start(arguments, msg);
      base::OS::VPrint(msg, arguments);
      va_end(arguments);
    }
  }

  Zone* zone_;
  Graph* graph_;
  int dfs_number_;    // Generates new DFS pre-order numbers on demand.
  int class_number_;  // Generates new equivalence class numbers on demand.
  Data node_data_;    // Per-node data stored as a side-table.
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_CONTROL_EQUIVALENCE_H_
