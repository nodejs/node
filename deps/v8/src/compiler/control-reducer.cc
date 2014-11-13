// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/common-operator.h"
#include "src/compiler/control-reducer.h"
#include "src/compiler/graph.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-properties-inl.h"
#include "src/zone-containers.h"

namespace v8 {
namespace internal {
namespace compiler {

enum VisitState { kUnvisited = 0, kOnStack = 1, kRevisit = 2, kVisited = 3 };
enum Reachability { kFromStart = 8 };

#define TRACE(x) \
  if (FLAG_trace_turbo_reduction) PrintF x

class ControlReducerImpl {
 public:
  ControlReducerImpl(Zone* zone, JSGraph* jsgraph,
                     CommonOperatorBuilder* common)
      : zone_(zone),
        jsgraph_(jsgraph),
        common_(common),
        state_(jsgraph->graph()->NodeCount(), kUnvisited, zone_),
        stack_(zone_),
        revisit_(zone_),
        dead_(NULL) {}

  Zone* zone_;
  JSGraph* jsgraph_;
  CommonOperatorBuilder* common_;
  ZoneVector<VisitState> state_;
  ZoneDeque<Node*> stack_;
  ZoneDeque<Node*> revisit_;
  Node* dead_;

  void Reduce() {
    Push(graph()->end());
    do {
      // Process the node on the top of the stack, potentially pushing more
      // or popping the node off the stack.
      ReduceTop();
      // If the stack becomes empty, revisit any nodes in the revisit queue.
      // If no nodes in the revisit queue, try removing dead loops.
      // If no dead loops, then finish.
    } while (!stack_.empty() || TryRevisit() || RepairAndRemoveLoops());
  }

  bool TryRevisit() {
    while (!revisit_.empty()) {
      Node* n = revisit_.back();
      revisit_.pop_back();
      if (state_[n->id()] == kRevisit) {  // state can change while in queue.
        Push(n);
        return true;
      }
    }
    return false;
  }

  // Repair the graph after the possible creation of non-terminating or dead
  // loops. Removing dead loops can produce more opportunities for reduction.
  bool RepairAndRemoveLoops() {
    // TODO(turbofan): we can skip this if the graph has no loops, but
    // we have to be careful about proper loop detection during reduction.

    // Gather all nodes backwards-reachable from end (through inputs).
    state_.assign(graph()->NodeCount(), kUnvisited);
    NodeVector nodes(zone_);
    AddNodesReachableFromEnd(nodes);

    // Walk forward through control nodes, looking for back edges to nodes
    // that are not connected to end. Those are non-terminating loops (NTLs).
    Node* start = graph()->start();
    ZoneVector<byte> fw_reachability(graph()->NodeCount(), 0, zone_);
    fw_reachability[start->id()] = kFromStart | kOnStack;

    // We use a stack of (Node, UseIter) pairs to avoid O(n^2) traversal.
    typedef std::pair<Node*, UseIter> FwIter;
    ZoneDeque<FwIter> fw_stack(zone_);
    fw_stack.push_back(FwIter(start, start->uses().begin()));

    while (!fw_stack.empty()) {
      Node* node = fw_stack.back().first;
      TRACE(("ControlFw: #%d:%s\n", node->id(), node->op()->mnemonic()));
      bool pop = true;
      while (fw_stack.back().second != node->uses().end()) {
        Node* succ = *(fw_stack.back().second);
        byte reach = fw_reachability[succ->id()];
        if ((reach & kOnStack) != 0 && state_[succ->id()] != kVisited) {
          // {succ} is on stack and not reachable from end.
          ConnectNTL(nodes, succ);
          fw_reachability.resize(graph()->NodeCount(), 0);
          // The use list of {succ} might have changed.
          fw_stack[fw_stack.size() - 1] = FwIter(succ, succ->uses().begin());
          pop = false;  // restart traversing successors of this node.
          break;
        }
        if ((reach & kFromStart) == 0 &&
            IrOpcode::IsControlOpcode(succ->opcode())) {
          // {succ} is a control node and not yet reached from start.
          fw_reachability[succ->id()] |= kFromStart | kOnStack;
          fw_stack.push_back(FwIter(succ, succ->uses().begin()));
          pop = false;  // "recurse" into successor control node.
          break;
        }
        ++fw_stack.back().second;
      }
      if (pop) {
        fw_reachability[node->id()] &= ~kOnStack;
        fw_stack.pop_back();
      }
    }

    // Trim references from dead nodes to live nodes first.
    jsgraph_->GetCachedNodes(&nodes);
    TrimNodes(nodes);

    // Any control nodes not reachable from start are dead, even loops.
    for (size_t i = 0; i < nodes.size(); i++) {
      Node* node = nodes[i];
      byte reach = fw_reachability[node->id()];
      if ((reach & kFromStart) == 0 &&
          IrOpcode::IsControlOpcode(node->opcode())) {
        ReplaceNode(node, dead());  // uses will be added to revisit queue.
      }
    }
    return TryRevisit();  // try to push a node onto the stack.
  }

  // Connect {loop}, the header of a non-terminating loop, to the end node.
  void ConnectNTL(NodeVector& nodes, Node* loop) {
    TRACE(("ConnectNTL: #%d:%s\n", loop->id(), loop->op()->mnemonic()));

    if (loop->opcode() != IrOpcode::kTerminate) {
      // Insert a {Terminate} node if the loop has effects.
      ZoneDeque<Node*> effects(zone_);
      for (Node* const use : loop->uses()) {
        if (use->opcode() == IrOpcode::kEffectPhi) effects.push_back(use);
      }
      int count = static_cast<int>(effects.size());
      if (count > 0) {
        Node** inputs = zone_->NewArray<Node*>(1 + count);
        for (int i = 0; i < count; i++) inputs[i] = effects[i];
        inputs[count] = loop;
        loop = graph()->NewNode(common_->Terminate(count), 1 + count, inputs);
        TRACE(("AddTerminate: #%d:%s[%d]\n", loop->id(), loop->op()->mnemonic(),
               count));
      }
    }

    Node* to_add = loop;
    Node* end = graph()->end();
    CHECK_EQ(IrOpcode::kEnd, end->opcode());
    Node* merge = end->InputAt(0);
    if (merge == NULL || merge->opcode() == IrOpcode::kDead) {
      // The end node died; just connect end to {loop}.
      end->ReplaceInput(0, loop);
    } else if (merge->opcode() != IrOpcode::kMerge) {
      // Introduce a final merge node for {end->InputAt(0)} and {loop}.
      merge = graph()->NewNode(common_->Merge(2), merge, loop);
      end->ReplaceInput(0, merge);
      to_add = merge;
    } else {
      // Append a new input to the final merge at the end.
      merge->AppendInput(graph()->zone(), loop);
      merge->set_op(common_->Merge(merge->InputCount()));
    }
    nodes.push_back(to_add);
    state_.resize(graph()->NodeCount(), kUnvisited);
    state_[to_add->id()] = kVisited;
    AddBackwardsReachableNodes(nodes, nodes.size() - 1);
  }

  void AddNodesReachableFromEnd(NodeVector& nodes) {
    Node* end = graph()->end();
    state_[end->id()] = kVisited;
    if (!end->IsDead()) {
      nodes.push_back(end);
      AddBackwardsReachableNodes(nodes, nodes.size() - 1);
    }
  }

  void AddBackwardsReachableNodes(NodeVector& nodes, size_t cursor) {
    while (cursor < nodes.size()) {
      Node* node = nodes[cursor++];
      for (Node* const input : node->inputs()) {
        if (state_[input->id()] != kVisited) {
          state_[input->id()] = kVisited;
          nodes.push_back(input);
        }
      }
    }
  }

  void Trim() {
    // Gather all nodes backwards-reachable from end through inputs.
    state_.assign(graph()->NodeCount(), kUnvisited);
    NodeVector nodes(zone_);
    AddNodesReachableFromEnd(nodes);

    // Process cached nodes in the JSGraph too.
    jsgraph_->GetCachedNodes(&nodes);
    TrimNodes(nodes);
  }

  void TrimNodes(NodeVector& nodes) {
    // Remove dead->live edges.
    for (size_t j = 0; j < nodes.size(); j++) {
      Node* node = nodes[j];
      for (UseIter i = node->uses().begin(); i != node->uses().end();) {
        size_t id = static_cast<size_t>((*i)->id());
        if (state_[id] != kVisited) {
          TRACE(("DeadLink: #%d:%s(%d) -> #%d:%s\n", (*i)->id(),
                 (*i)->op()->mnemonic(), i.index(), node->id(),
                 node->op()->mnemonic()));
          i.UpdateToAndIncrement(NULL);
        } else {
          ++i;
        }
      }
    }
#if DEBUG
    // Verify that no inputs to live nodes are NULL.
    for (size_t j = 0; j < nodes.size(); j++) {
      Node* node = nodes[j];
      for (Node* const input : node->inputs()) {
        CHECK_NE(NULL, input);
      }
      for (Node* const use : node->uses()) {
        size_t id = static_cast<size_t>(use->id());
        CHECK_EQ(kVisited, state_[id]);
      }
    }
#endif
  }

  // Reduce the node on the top of the stack.
  // If an input {i} is not yet visited or needs to be revisited, push {i} onto
  // the stack and return. Otherwise, all inputs are visited, so apply
  // reductions for {node} and pop it off the stack.
  void ReduceTop() {
    size_t height = stack_.size();
    Node* node = stack_.back();

    if (node->IsDead()) return Pop();  // Node was killed while on stack.

    TRACE(("ControlReduce: #%d:%s\n", node->id(), node->op()->mnemonic()));

    // Recurse on an input if necessary.
    for (Node* const input : node->inputs()) {
      if (Recurse(input)) return;
    }

    // All inputs should be visited or on stack. Apply reductions to node.
    Node* replacement = ReduceNode(node);
    if (replacement != node) ReplaceNode(node, replacement);

    // After reducing the node, pop it off the stack.
    CHECK_EQ(static_cast<int>(height), static_cast<int>(stack_.size()));
    Pop();

    // If there was a replacement, reduce it after popping {node}.
    if (replacement != node) Recurse(replacement);
  }

  // Push a node onto the stack if its state is {kUnvisited} or {kRevisit}.
  bool Recurse(Node* node) {
    size_t id = static_cast<size_t>(node->id());
    if (id < state_.size()) {
      if (state_[id] != kRevisit && state_[id] != kUnvisited) return false;
    } else {
      state_.resize((3 * id) / 2, kUnvisited);
    }
    Push(node);
    return true;
  }

  void Push(Node* node) {
    state_[node->id()] = kOnStack;
    stack_.push_back(node);
  }

  void Pop() {
    int pos = static_cast<int>(stack_.size()) - 1;
    DCHECK_GE(pos, 0);
    DCHECK_EQ(kOnStack, state_[stack_[pos]->id()]);
    state_[stack_[pos]->id()] = kVisited;
    stack_.pop_back();
  }

  // Queue a node to be revisited if it has been visited once already.
  void Revisit(Node* node) {
    size_t id = static_cast<size_t>(node->id());
    if (id < state_.size() && state_[id] == kVisited) {
      TRACE(("  Revisit #%d:%s\n", node->id(), node->op()->mnemonic()));
      state_[id] = kRevisit;
      revisit_.push_back(node);
    }
  }

  Node* dead() {
    if (dead_ == NULL) dead_ = graph()->NewNode(common_->Dead());
    return dead_;
  }

  //===========================================================================
  // Reducer implementation: perform reductions on a node.
  //===========================================================================
  Node* ReduceNode(Node* node) {
    if (node->op()->ControlInputCount() == 1) {
      // If a node has only one control input and it is dead, replace with dead.
      Node* control = NodeProperties::GetControlInput(node);
      if (control->opcode() == IrOpcode::kDead) {
        TRACE(("ControlDead: #%d:%s\n", node->id(), node->op()->mnemonic()));
        return control;
      }
    }

    // Reduce branches, phis, and merges.
    switch (node->opcode()) {
      case IrOpcode::kBranch:
        return ReduceBranch(node);
      case IrOpcode::kLoop:
      case IrOpcode::kMerge:
        return ReduceMerge(node);
      case IrOpcode::kSelect:
        return ReduceSelect(node);
      case IrOpcode::kPhi:
      case IrOpcode::kEffectPhi:
        return ReducePhi(node);
      default:
        return node;
    }
  }

  // Reduce redundant selects.
  Node* ReduceSelect(Node* const node) {
    Node* const cond = node->InputAt(0);
    Node* const tvalue = node->InputAt(1);
    Node* const fvalue = node->InputAt(2);
    if (tvalue == fvalue) return tvalue;
    switch (cond->opcode()) {
      case IrOpcode::kInt32Constant:
        return Int32Matcher(cond).Is(0) ? fvalue : tvalue;
      case IrOpcode::kInt64Constant:
        return Int64Matcher(cond).Is(0) ? fvalue : tvalue;
      case IrOpcode::kNumberConstant:
        return NumberMatcher(cond).Is(0) ? fvalue : tvalue;
      case IrOpcode::kHeapConstant: {
        Handle<Object> object =
            HeapObjectMatcher<Object>(cond).Value().handle();
        if (object->IsTrue()) return tvalue;
        if (object->IsFalse()) return fvalue;
        break;
      }
      default:
        break;
    }
    return node;
  }

  // Reduce redundant phis.
  Node* ReducePhi(Node* node) {
    int n = node->InputCount();
    if (n <= 1) return dead();            // No non-control inputs.
    if (n == 2) return node->InputAt(0);  // Only one non-control input.

    Node* replacement = NULL;
    Node::Inputs inputs = node->inputs();
    for (InputIter it = inputs.begin(); n > 1; --n, ++it) {
      Node* input = *it;
      if (input->opcode() == IrOpcode::kDead) continue;  // ignore dead inputs.
      if (input != node && input != replacement) {       // non-redundant input.
        if (replacement != NULL) return node;
        replacement = input;
      }
    }
    return replacement == NULL ? dead() : replacement;
  }

  // Reduce merges by trimming away dead inputs from the merge and phis.
  Node* ReduceMerge(Node* node) {
    // Count the number of live inputs.
    int live = 0;
    int index = 0;
    int live_index = 0;
    for (Node* const input : node->inputs()) {
      if (input->opcode() != IrOpcode::kDead) {
        live++;
        live_index = index;
      }
      index++;
    }

    if (live > 1 && live == node->InputCount()) return node;  // nothing to do.

    TRACE(("ReduceMerge: #%d:%s (%d live)\n", node->id(),
           node->op()->mnemonic(), live));

    if (live == 0) return dead();  // no remaining inputs.

    // Gather phis and effect phis to be edited.
    ZoneVector<Node*> phis(zone_);
    for (Node* const use : node->uses()) {
      if (use->opcode() == IrOpcode::kPhi ||
          use->opcode() == IrOpcode::kEffectPhi) {
        phis.push_back(use);
      }
    }

    if (live == 1) {
      // All phis are redundant. Replace them with their live input.
      for (Node* const phi : phis) ReplaceNode(phi, phi->InputAt(live_index));
      // The merge itself is redundant.
      return node->InputAt(live_index);
    }

    // Edit phis in place, removing dead inputs and revisiting them.
    for (Node* const phi : phis) {
      TRACE(("  PhiInMerge: #%d:%s (%d live)\n", phi->id(),
             phi->op()->mnemonic(), live));
      RemoveDeadInputs(node, phi);
      Revisit(phi);
    }
    // Edit the merge in place, removing dead inputs.
    RemoveDeadInputs(node, node);
    return node;
  }

  // Reduce branches if they have constant inputs.
  Node* ReduceBranch(Node* node) {
    Node* cond = node->InputAt(0);
    bool is_true;
    switch (cond->opcode()) {
      case IrOpcode::kInt32Constant:
        is_true = !Int32Matcher(cond).Is(0);
        break;
      case IrOpcode::kInt64Constant:
        is_true = !Int64Matcher(cond).Is(0);
        break;
      case IrOpcode::kNumberConstant:
        is_true = !NumberMatcher(cond).Is(0);
        break;
      case IrOpcode::kHeapConstant: {
        Handle<Object> object =
            HeapObjectMatcher<Object>(cond).Value().handle();
        if (object->IsTrue())
          is_true = true;
        else if (object->IsFalse())
          is_true = false;
        else
          return node;  // TODO(turbofan): fold branches on strings, objects.
        break;
      }
      default:
        return node;
    }

    TRACE(("BranchReduce: #%d:%s = %s\n", node->id(), node->op()->mnemonic(),
           is_true ? "true" : "false"));

    // Replace IfTrue and IfFalse projections from this branch.
    Node* control = NodeProperties::GetControlInput(node);
    for (UseIter i = node->uses().begin(); i != node->uses().end();) {
      Node* to = *i;
      if (to->opcode() == IrOpcode::kIfTrue) {
        TRACE(("  IfTrue: #%d:%s\n", to->id(), to->op()->mnemonic()));
        i.UpdateToAndIncrement(NULL);
        ReplaceNode(to, is_true ? control : dead());
      } else if (to->opcode() == IrOpcode::kIfFalse) {
        TRACE(("  IfFalse: #%d:%s\n", to->id(), to->op()->mnemonic()));
        i.UpdateToAndIncrement(NULL);
        ReplaceNode(to, is_true ? dead() : control);
      } else {
        ++i;
      }
    }
    return control;
  }

  // Remove inputs to {node} corresponding to the dead inputs to {merge}
  // and compact the remaining inputs, updating the operator.
  void RemoveDeadInputs(Node* merge, Node* node) {
    int pos = 0;
    for (int i = 0; i < node->InputCount(); i++) {
      // skip dead inputs.
      if (i < merge->InputCount() &&
          merge->InputAt(i)->opcode() == IrOpcode::kDead)
        continue;
      // compact live inputs.
      if (pos != i) node->ReplaceInput(pos, node->InputAt(i));
      pos++;
    }
    node->TrimInputCount(pos);
    if (node->opcode() == IrOpcode::kPhi) {
      node->set_op(common_->Phi(OpParameter<MachineType>(node->op()), pos - 1));
    } else if (node->opcode() == IrOpcode::kEffectPhi) {
      node->set_op(common_->EffectPhi(pos - 1));
    } else if (node->opcode() == IrOpcode::kMerge) {
      node->set_op(common_->Merge(pos));
    } else if (node->opcode() == IrOpcode::kLoop) {
      node->set_op(common_->Loop(pos));
    } else {
      UNREACHABLE();
    }
  }

  // Replace uses of {node} with {replacement} and revisit the uses.
  void ReplaceNode(Node* node, Node* replacement) {
    if (node == replacement) return;
    TRACE(("  Replace: #%d:%s with #%d:%s\n", node->id(),
           node->op()->mnemonic(), replacement->id(),
           replacement->op()->mnemonic()));
    for (Node* const use : node->uses()) {
      // Don't revisit this node if it refers to itself.
      if (use != node) Revisit(use);
    }
    node->ReplaceUses(replacement);
    node->Kill();
  }

  Graph* graph() { return jsgraph_->graph(); }
};


void ControlReducer::ReduceGraph(Zone* zone, JSGraph* jsgraph,
                                 CommonOperatorBuilder* common) {
  ControlReducerImpl impl(zone, jsgraph, common);
  impl.Reduce();
  impl.Trim();
}


void ControlReducer::TrimGraph(Zone* zone, JSGraph* jsgraph) {
  ControlReducerImpl impl(zone, jsgraph, NULL);
  impl.Trim();
}


Node* ControlReducer::ReducePhiForTesting(JSGraph* jsgraph,
                                          CommonOperatorBuilder* common,
                                          Node* node) {
  Zone zone(jsgraph->graph()->zone()->isolate());
  ControlReducerImpl impl(&zone, jsgraph, common);
  return impl.ReducePhi(node);
}


Node* ControlReducer::ReduceMergeForTesting(JSGraph* jsgraph,
                                            CommonOperatorBuilder* common,
                                            Node* node) {
  Zone zone(jsgraph->graph()->zone()->isolate());
  ControlReducerImpl impl(&zone, jsgraph, common);
  return impl.ReduceMerge(node);
}


Node* ControlReducer::ReduceBranchForTesting(JSGraph* jsgraph,
                                             CommonOperatorBuilder* common,
                                             Node* node) {
  Zone zone(jsgraph->graph()->zone()->isolate());
  ControlReducerImpl impl(&zone, jsgraph, common);
  return impl.ReduceBranch(node);
}
}
}
}  // namespace v8::internal::compiler
