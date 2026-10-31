// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/maglev/maglev-basic-block.h"

#include <algorithm>

#include "src/maglev/maglev-ir-inl.h"

namespace v8 {
namespace internal {
namespace maglev {

ZoneVector<Node*> BasicBlock::Split(Node* node, Zone* zone) {
  size_t split = 0;
  for (; split < nodes_.size(); split++) {
    if (nodes_[split] == node) break;
  }
  DCHECK_NE(split, nodes_.size());
  ZoneVector<Node*> result(nodes_.size() - split, zone);
  for (size_t i = 0; i < result.size(); i++) {
    result[i] = nodes_[i + split];
  }
  nodes_.resize(split);
  return result;
}

void BasicBlock::TruncateAt(Node* node) {
  auto it = std::find(nodes_.begin(), nodes_.end(), node);
  DCHECK_NE(it, nodes_.end());
  for (auto cur = it; cur != nodes_.end(); ++cur) {
    if (*cur != nullptr) (*cur)->set_owner(nullptr);
  }
  nodes_.resize(it - nodes_.begin());
}

void BasicBlock::OverwriteControlWithAbort(AbortReason reason) {
  ControlNode* control = reset_control_node();
  RemovePredecessorFollowing(control);
  control->OverwriteWith<Abort>(reason);
  set_deferred(true);
  set_control_node(control);
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8
