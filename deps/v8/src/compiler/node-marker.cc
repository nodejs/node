// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/node-marker.h"

#include "src/compiler/graph.h"
#include "src/compiler/node.h"

namespace v8 {
namespace internal {
namespace compiler {

NodeMarkerBase::NodeMarkerBase(Graph* graph, uint32_t num_states)
    : mark_min_(graph->mark_max_), mark_max_(graph->mark_max_ += num_states) {
  DCHECK_NE(0u, num_states);        // user error!
  DCHECK_LT(mark_min_, mark_max_);  // check for wraparound.
}


Mark NodeMarkerBase::Get(Node* node) {
  Mark mark = node->mark();
  if (mark < mark_min_) {
    mark = mark_min_;
    node->set_mark(mark_min_);
  }
  DCHECK_LT(mark, mark_max_);
  return mark - mark_min_;
}


void NodeMarkerBase::Set(Node* node, Mark mark) {
  DCHECK_LT(mark, mark_max_ - mark_min_);
  DCHECK_LT(node->mark(), mark_max_);
  node->set_mark(mark + mark_min_);
}


void NodeMarkerBase::Reset(Graph* graph) {
  uint32_t const num_states = mark_max_ - mark_min_;
  mark_min_ = graph->mark_max_;
  mark_max_ = graph->mark_max_ += num_states;
  DCHECK_LT(mark_min_, mark_max_);  // check for wraparound.
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
