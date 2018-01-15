// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/control-equivalence.h"
#include "src/compiler/node-properties.h"

#define TRACE(...)                                 \
  do {                                             \
    if (FLAG_trace_turbo_ceq) PrintF(__VA_ARGS__); \
  } while (false)

namespace v8 {
namespace internal {
namespace compiler {

void ControlEquivalence::Run(Node* exit) {
  if (!Participates(exit) || GetClass(exit) == kInvalidClass) {
    DetermineParticipation(exit);
    RunUndirectedDFS(exit);
  }
}


// static
STATIC_CONST_MEMBER_DEFINITION const size_t ControlEquivalence::kInvalidClass;


void ControlEquivalence::VisitPre(Node* node) {
  TRACE("CEQ: Pre-visit of #%d:%s\n", node->id(), node->op()->mnemonic());
}


void ControlEquivalence::VisitMid(Node* node, DFSDirection direction) {
  TRACE("CEQ: Mid-visit of #%d:%s\n", node->id(), node->op()->mnemonic());
  BracketList& blist = GetBracketList(node);

  // Remove brackets pointing to this node [line:19].
  BracketListDelete(blist, node, direction);

  // Potentially introduce artificial dependency from start to end.
  if (blist.empty()) {
    DCHECK_EQ(kInputDirection, direction);
    VisitBackedge(node, graph_->end(), kInputDirection);
  }

  // Potentially start a new equivalence class [line:37].
  BracketListTRACE(blist);
  Bracket* recent = &blist.back();
  if (recent->recent_size != blist.size()) {
    recent->recent_size = blist.size();
    recent->recent_class = NewClassNumber();
  }

  // Assign equivalence class to node.
  SetClass(node, recent->recent_class);
  TRACE("  Assigned class number is %zu\n", GetClass(node));
}


void ControlEquivalence::VisitPost(Node* node, Node* parent_node,
                                   DFSDirection direction) {
  TRACE("CEQ: Post-visit of #%d:%s\n", node->id(), node->op()->mnemonic());
  BracketList& blist = GetBracketList(node);

  // Remove brackets pointing to this node [line:19].
  BracketListDelete(blist, node, direction);

  // Propagate bracket list up the DFS tree [line:13].
  if (parent_node != nullptr) {
    BracketList& parent_blist = GetBracketList(parent_node);
    parent_blist.splice(parent_blist.end(), blist);
  }
}


void ControlEquivalence::VisitBackedge(Node* from, Node* to,
                                       DFSDirection direction) {
  TRACE("CEQ: Backedge from #%d:%s to #%d:%s\n", from->id(),
        from->op()->mnemonic(), to->id(), to->op()->mnemonic());

  // Push backedge onto the bracket list [line:25].
  Bracket bracket = {direction, kInvalidClass, 0, from, to};
  GetBracketList(from).push_back(bracket);
}


void ControlEquivalence::RunUndirectedDFS(Node* exit) {
  ZoneStack<DFSStackEntry> stack(zone_);
  DFSPush(stack, exit, nullptr, kInputDirection);
  VisitPre(exit);

  while (!stack.empty()) {  // Undirected depth-first backwards traversal.
    DFSStackEntry& entry = stack.top();
    Node* node = entry.node;

    if (entry.direction == kInputDirection) {
      if (entry.input != node->input_edges().end()) {
        Edge edge = *entry.input;
        Node* input = edge.to();
        ++(entry.input);
        if (NodeProperties::IsControlEdge(edge)) {
          // Visit next control input.
          if (!Participates(input)) continue;
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
        if (NodeProperties::IsControlEdge(edge)) {
          // Visit next control use.
          if (!Participates(use)) continue;
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

void ControlEquivalence::DetermineParticipationEnqueue(ZoneQueue<Node*>& queue,
                                                       Node* node) {
  if (!Participates(node)) {
    AllocateData(node);
    queue.push(node);
  }
}


void ControlEquivalence::DetermineParticipation(Node* exit) {
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


void ControlEquivalence::DFSPush(DFSStack& stack, Node* node, Node* from,
                                 DFSDirection dir) {
  DCHECK(Participates(node));
  DCHECK(!GetData(node)->visited);
  GetData(node)->on_stack = true;
  Node::InputEdges::iterator input = node->input_edges().begin();
  Node::UseEdges::iterator use = node->use_edges().begin();
  stack.push({dir, input, use, from, node});
}


void ControlEquivalence::DFSPop(DFSStack& stack, Node* node) {
  DCHECK_EQ(stack.top().node, node);
  GetData(node)->on_stack = false;
  GetData(node)->visited = true;
  stack.pop();
}


void ControlEquivalence::BracketListDelete(BracketList& blist, Node* to,
                                           DFSDirection direction) {
  // TODO(mstarzinger): Optimize this to avoid linear search.
  for (BracketList::iterator i = blist.begin(); i != blist.end(); /*nop*/) {
    if (i->to == to && i->direction != direction) {
      TRACE("  BList erased: {%d->%d}\n", i->from->id(), i->to->id());
      i = blist.erase(i);
    } else {
      ++i;
    }
  }
}


void ControlEquivalence::BracketListTRACE(BracketList& blist) {
  if (FLAG_trace_turbo_ceq) {
    TRACE("  BList: ");
    for (Bracket bracket : blist) {
      TRACE("{%d->%d} ", bracket.from->id(), bracket.to->id());
    }
    TRACE("\n");
  }
}

#undef TRACE

}  // namespace compiler
}  // namespace internal
}  // namespace v8
