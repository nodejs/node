// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_GRAPH_LABELLER_H_
#define V8_MAGLEV_MAGLEV_GRAPH_LABELLER_H_

#include <map>

#include "src/maglev/maglev-graph.h"
#include "src/maglev/maglev-ir.h"

namespace v8 {
namespace internal {
namespace maglev {

class MaglevGraphLabeller {
 public:
  void RegisterNode(const Node* node) {
    if (node_ids_.emplace(node, next_node_id_).second) {
      next_node_id_++;
    }
  }
  void RegisterBasicBlock(const BasicBlock* block) {
    block_ids_[block] = next_block_id_++;
    if (node_ids_.emplace(block->control_node(), next_node_id_).second) {
      next_node_id_++;
    }
  }

  int BlockId(const BasicBlock* block) { return block_ids_[block]; }
  int NodeId(const NodeBase* node) { return node_ids_[node]; }

  int max_node_id() const { return next_node_id_ - 1; }

  int max_node_id_width() const { return std::ceil(std::log10(max_node_id())); }

  void PrintNodeLabel(std::ostream& os, const Node* node) {
    auto node_id_it = node_ids_.find(node);

    if (node_id_it == node_ids_.end()) {
      os << "<invalid node " << node << ">";
      return;
    }

    os << "n" << node_id_it->second;
  }

  void PrintInput(std::ostream& os, const Input& input) {
    PrintNodeLabel(os, input.node());
    os << ":" << input.operand();
  }

 private:
  std::map<const BasicBlock*, int> block_ids_;
  std::map<const NodeBase*, int> node_ids_;
  int next_block_id_ = 1;
  int next_node_id_ = 1;
};

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_GRAPH_LABELLER_H_
