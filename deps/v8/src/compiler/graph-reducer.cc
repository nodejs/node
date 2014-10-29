// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/graph-reducer.h"

#include <functional>

#include "src/compiler/graph-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

GraphReducer::GraphReducer(Graph* graph)
    : graph_(graph), reducers_(graph->zone()) {}


static bool NodeIdIsLessThan(const Node* node, NodeId id) {
  return node->id() < id;
}


void GraphReducer::ReduceNode(Node* node) {
  static const unsigned kMaxAttempts = 16;
  bool reduce = true;
  for (unsigned attempts = 0; attempts <= kMaxAttempts; ++attempts) {
    if (!reduce) return;
    reduce = false;  // Assume we don't need to rerun any reducers.
    int before = graph_->NodeCount();
    for (ZoneVector<Reducer*>::iterator i = reducers_.begin();
         i != reducers_.end(); ++i) {
      Reduction reduction = (*i)->Reduce(node);
      Node* replacement = reduction.replacement();
      if (replacement == NULL) {
        // No change from this reducer.
      } else if (replacement == node) {
        // {replacement == node} represents an in-place reduction.
        // Rerun all the reducers for this node, as now there may be more
        // opportunities for reduction.
        reduce = true;
        break;
      } else {
        if (node == graph_->start()) graph_->SetStart(replacement);
        if (node == graph_->end()) graph_->SetEnd(replacement);
        // If {node} was replaced by an old node, unlink {node} and assume that
        // {replacement} was already reduced and finish.
        if (replacement->id() < before) {
          node->ReplaceUses(replacement);
          node->Kill();
          return;
        }
        // Otherwise, {node} was replaced by a new node. Replace all old uses of
        // {node} with {replacement}. New nodes created by this reduction can
        // use {node}.
        node->ReplaceUsesIf(
            std::bind2nd(std::ptr_fun(&NodeIdIsLessThan), before), replacement);
        // Unlink {node} if it's no longer used.
        if (node->uses().empty()) {
          node->Kill();
        }
        // Rerun all the reductions on the {replacement}.
        node = replacement;
        reduce = true;
        break;
      }
    }
  }
}


// A helper class to reuse the node traversal algorithm.
struct GraphReducerVisitor FINAL : public NullNodeVisitor {
  explicit GraphReducerVisitor(GraphReducer* reducer) : reducer_(reducer) {}
  GenericGraphVisit::Control Post(Node* node) {
    reducer_->ReduceNode(node);
    return GenericGraphVisit::CONTINUE;
  }
  GraphReducer* reducer_;
};


void GraphReducer::ReduceGraph() {
  GraphReducerVisitor visitor(this);
  // Perform a post-order reduction of all nodes starting from the end.
  graph()->VisitNodeInputsFromEnd(&visitor);
}


// TODO(titzer): partial graph reductions.

}  // namespace compiler
}  // namespace internal
}  // namespace v8
