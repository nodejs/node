// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/graph.h"

#include <algorithm>
#include <iomanip>

#include "src/base/logging.h"

namespace v8::internal::compiler::turboshaft {

// PrintDominatorTree prints the dominator tree in a format that looks like:
//
//    0
//    ╠ 1
//    ╠ 2
//    ╠ 3
//    ║ ╠ 4
//    ║ ║ ╠ 5
//    ║ ║ ╚ 6
//    ║ ╚ 7
//    ║   ╠ 8
//    ║   ╚ 16
//    ╚ 17
//
// Where the numbers are the IDs of the Blocks.
// Doing so is mostly straight forward, with the subtelty that we need to know
// where to put "║" symbols (eg, in from of "╠ 5" above). The logic to do this
// is basically: "if the current node is not the last of its siblings, then,
// when going down to print its content, we add a "║" in front of each of its
// children; otherwise (current node is the last of its siblings), we add a
// blank space " " in front of its children". We maintain this information
// using a stack (implemented with a std::vector).
void Block::PrintDominatorTree(std::vector<const char*> tree_symbols,
                               bool has_next) const {
  // Printing the current node.
  if (tree_symbols.empty()) {
    // This node is the root of the tree.
    PrintF("B%d\n", index().id());
    tree_symbols.push_back("");
  } else {
    // This node is not the root of the tree; we start by printing the
    // connectors of the previous levels.
    for (const char* s : tree_symbols) PrintF("%s", s);
    // Then, we print the node id, preceeded by a ╠ or ╚ connector.
    const char* tree_connector_symbol = has_next ? "╠" : "╚";
    PrintF("%s B%d\n", tree_connector_symbol, index().id());
    // And we add to the stack a connector to continue this path (if needed)
    // while printing the current node's children.
    const char* tree_cont_symbol = has_next ? "║ " : "  ";
    tree_symbols.push_back(tree_cont_symbol);
  }
  // Recursively printing the children of this node.
  base::SmallVector<Block*, 8> children = Children();
  for (Block* child : children) {
    child->PrintDominatorTree(tree_symbols, child != children.back());
  }
  // Removing from the stack the "║" or " " corresponding to this node.
  tree_symbols.pop_back();
}

std::ostream& operator<<(std::ostream& os, PrintAsBlockHeader block_header) {
  const Block& block = block_header.block;
  os << block.kind() << " " << block_header.block_id;
  if (!block.Predecessors().empty()) {
    os << " <- ";
    bool first = true;
    for (const Block* pred : block.Predecessors()) {
      if (!first) os << ", ";
      os << pred->index();
      first = false;
    }
  }
  return os;
}

std::ostream& operator<<(std::ostream& os, const Graph& graph) {
  for (const Block& block : graph.blocks()) {
    os << "\n" << PrintAsBlockHeader{block} << "\n";
    for (const Operation& op : graph.operations(block)) {
      os << std::setw(5) << graph.Index(op).id() << ": " << op << "\n";
    }
  }
  return os;
}

std::ostream& operator<<(std::ostream& os, const Block::Kind& kind) {
  switch (kind) {
    case Block::Kind::kLoopHeader:
      os << "LOOP";
      break;
    case Block::Kind::kMerge:
      os << "MERGE";
      break;
    case Block::Kind::kBranchTarget:
      os << "BLOCK";
      break;
  }
  return os;
}

}  // namespace v8::internal::compiler::turboshaft
