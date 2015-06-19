// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <functional>
#include <limits>

#include "src/compiler/graph.h"
#include "src/compiler/graph-reducer.h"
#include "src/compiler/node.h"

namespace v8 {
namespace internal {
namespace compiler {

bool Reducer::Finish() { return true; }


enum class GraphReducer::State : uint8_t {
  kUnvisited,
  kRevisit,
  kOnStack,
  kVisited
};


GraphReducer::GraphReducer(Graph* graph, Zone* zone)
    : graph_(graph),
      state_(graph, 4),
      reducers_(zone),
      revisit_(zone),
      stack_(zone) {}


GraphReducer::~GraphReducer() {}


void GraphReducer::AddReducer(Reducer* reducer) {
  reducers_.push_back(reducer);
}


void GraphReducer::ReduceNode(Node* node) {
  DCHECK(stack_.empty());
  DCHECK(revisit_.empty());
  Push(node);
  for (;;) {
    if (!stack_.empty()) {
      // Process the node on the top of the stack, potentially pushing more or
      // popping the node off the stack.
      ReduceTop();
    } else if (!revisit_.empty()) {
      // If the stack becomes empty, revisit any nodes in the revisit queue.
      Node* const node = revisit_.top();
      revisit_.pop();
      if (state_.Get(node) == State::kRevisit) {
        // state can change while in queue.
        Push(node);
      }
    } else {
      break;
    }
  }
  DCHECK(revisit_.empty());
  DCHECK(stack_.empty());
}


void GraphReducer::ReduceGraph() {
  for (;;) {
    ReduceNode(graph()->end());
    // TODO(turbofan): Remove this once the dead node trimming is in the
    // GraphReducer.
    bool done = true;
    for (Reducer* const reducer : reducers_) {
      if (!reducer->Finish()) {
        done = false;
        break;
      }
    }
    if (done) break;
    // Reset all marks on the graph in preparation to re-reduce the graph.
    state_.Reset(graph());
  }
}


Reduction GraphReducer::Reduce(Node* const node) {
  auto skip = reducers_.end();
  for (auto i = reducers_.begin(); i != reducers_.end();) {
    if (i != skip) {
      Reduction reduction = (*i)->Reduce(node);
      if (!reduction.Changed()) {
        // No change from this reducer.
      } else if (reduction.replacement() == node) {
        // {replacement} == {node} represents an in-place reduction. Rerun
        // all the other reducers for this node, as now there may be more
        // opportunities for reduction.
        skip = i;
        i = reducers_.begin();
        continue;
      } else {
        // {node} was replaced by another node.
        return reduction;
      }
    }
    ++i;
  }
  if (skip == reducers_.end()) {
    // No change from any reducer.
    return Reducer::NoChange();
  }
  // At least one reducer did some in-place reduction.
  return Reducer::Changed(node);
}


void GraphReducer::ReduceTop() {
  NodeState& entry = stack_.top();
  Node* node = entry.node;
  DCHECK(state_.Get(node) == State::kOnStack);

  if (node->IsDead()) return Pop();  // Node was killed while on stack.

  // Recurse on an input if necessary.
  int start = entry.input_index < node->InputCount() ? entry.input_index : 0;
  for (int i = start; i < node->InputCount(); i++) {
    Node* input = node->InputAt(i);
    entry.input_index = i + 1;
    if (input != node && Recurse(input)) return;
  }
  for (int i = 0; i < start; i++) {
    Node* input = node->InputAt(i);
    entry.input_index = i + 1;
    if (input != node && Recurse(input)) return;
  }

  // Remember the max node id before reduction.
  NodeId const max_id = graph()->NodeCount() - 1;

  // All inputs should be visited or on stack. Apply reductions to node.
  Reduction reduction = Reduce(node);

  // If there was no reduction, pop {node} and continue.
  if (!reduction.Changed()) return Pop();

  // Check if the reduction is an in-place update of the {node}.
  Node* const replacement = reduction.replacement();
  if (replacement == node) {
    // In-place update of {node}, may need to recurse on an input.
    for (int i = 0; i < node->InputCount(); ++i) {
      Node* input = node->InputAt(i);
      entry.input_index = i + 1;
      if (input != node && Recurse(input)) return;
    }
  }

  // After reducing the node, pop it off the stack.
  Pop();

  // Check if we have a new replacement.
  if (replacement != node) {
    Replace(node, replacement, max_id);
  } else {
    // Revisit all uses of the node.
    for (Node* const user : node->uses()) {
      // Don't revisit this node if it refers to itself.
      if (user != node) Revisit(user);
    }
  }
}


void GraphReducer::Replace(Node* node, Node* replacement) {
  Replace(node, replacement, std::numeric_limits<NodeId>::max());
}


void GraphReducer::Replace(Node* node, Node* replacement, NodeId max_id) {
  if (node == graph()->start()) graph()->SetStart(replacement);
  if (node == graph()->end()) graph()->SetEnd(replacement);
  if (replacement->id() <= max_id) {
    // {replacement} is an old node, so unlink {node} and assume that
    // {replacement} was already reduced and finish.
    for (Edge edge : node->use_edges()) {
      Node* const user = edge.from();
      edge.UpdateTo(replacement);
      // Don't revisit this node if it refers to itself.
      if (user != node) Revisit(user);
    }
    node->Kill();
  } else {
    // Replace all old uses of {node} with {replacement}, but allow new nodes
    // created by this reduction to use {node}.
    for (Edge edge : node->use_edges()) {
      Node* const user = edge.from();
      if (user->id() <= max_id) {
        edge.UpdateTo(replacement);
        // Don't revisit this node if it refers to itself.
        if (user != node) Revisit(user);
      }
    }
    // Unlink {node} if it's no longer used.
    if (node->uses().empty()) node->Kill();

    // If there was a replacement, reduce it after popping {node}.
    Recurse(replacement);
  }
}


void GraphReducer::Pop() {
  Node* node = stack_.top().node;
  state_.Set(node, State::kVisited);
  stack_.pop();
}


void GraphReducer::Push(Node* const node) {
  DCHECK(state_.Get(node) != State::kOnStack);
  state_.Set(node, State::kOnStack);
  stack_.push({node, 0});
}


bool GraphReducer::Recurse(Node* node) {
  if (state_.Get(node) > State::kRevisit) return false;
  Push(node);
  return true;
}


void GraphReducer::Revisit(Node* node) {
  if (state_.Get(node) == State::kVisited) {
    state_.Set(node, State::kRevisit);
    revisit_.push(node);
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
