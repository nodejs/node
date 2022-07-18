// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/graph.h"

#include <iomanip>

namespace v8::internal::compiler::turboshaft {

void Graph::GenerateDominatorTree() {
  for (Block* block : bound_blocks_) {
    if (block->index() == StartBlock().index()) {
      // Start block has no dominators. We create a jmp_ edge to itself, so that
      // the SetDominator algorithm does not need a special case for when the
      // start block is reached.
      block->jmp_ = block;
      block->nxt_ = nullptr;
      block->len_ = 0;
      block->jmp_len_ = 0;
      continue;
    }
    if (block->kind_ == Block::Kind::kBranchTarget) {
      // kBranchTarget blocks always have a single predecessor, which dominates
      // them.
      DCHECK_EQ(block->PredecessorCount(), 1);
      block->SetDominator(block->LastPredecessor());
    } else if (block->kind_ == Block::Kind::kLoopHeader) {
      // kLoopHeader blocks have 2 predecessors, but their dominator is
      // always their first predecessor (the 2nd one is the loop's backedge).
      DCHECK_EQ(block->PredecessorCount(), 2);
      block->SetDominator(block->LastPredecessor()->NeighboringPredecessor());
    } else {
      // kMerge has (more or less) an arbitrary number of predecessors. We need
      // to find the lowest common ancestor (LCA) of all of the predecessors.
      DCHECK_EQ(block->kind_, Block::Kind::kMerge);
      Block* dominator = block->LastPredecessor();
      for (Block* pred = dominator->NeighboringPredecessor(); pred != nullptr;
           pred = pred->NeighboringPredecessor()) {
        dominator = dominator->GetCommonDominator(pred);
      }
      block->SetDominator(dominator);
    }
  }
}

template <class Derived>
void RandomAccessStackDominatorNode<Derived>::SetDominator(Derived* dominator) {
  DCHECK_NOT_NULL(dominator);
  // Determining the jmp pointer
  Derived* t = dominator->jmp_;
  if (dominator->len_ - t->len_ == t->len_ - t->jmp_len_) {
    t = t->jmp_;
  } else {
    t = dominator;
  }
  // Initializing fields
  nxt_ = dominator;
  jmp_ = t;
  len_ = dominator->len_ + 1;
  jmp_len_ = jmp_->len_;
  dominator->AddChild(static_cast<Derived*>(this));
}

template <class Derived>
Derived* RandomAccessStackDominatorNode<Derived>::GetCommonDominator(
    RandomAccessStackDominatorNode<Derived>* b) {
  RandomAccessStackDominatorNode* a = this;
  if (b->len_ > a->len_) {
    // Swapping |a| and |b| so that |a| always has a greater length.
    std::swap(a, b);
  }
  DCHECK_GE(a->len_, 0);
  DCHECK_GE(b->len_, 0);

  // Going up the dominators of |a| in order to reach the level of |b|.
  while (a->len_ != b->len_) {
    DCHECK_GE(a->len_, 0);
    if (a->jmp_len_ >= b->len_) {
      a = a->jmp_;
    } else {
      a = a->nxt_;
    }
  }

  // Going up the dominators of |a| and |b| simultaneously until |a| == |b|
  while (a != b) {
    DCHECK_EQ(a->len_, b->len_);
    DCHECK_GE(a->len_, 0);
    if (a->jmp_ == b->jmp_) {
      // We found a common dominator, but we actually want to find the smallest
      // one, so we go down in the current subtree.
      a = a->nxt_;
      b = b->nxt_;
    } else {
      a = a->jmp_;
      b = b->jmp_;
    }
  }

  return static_cast<Derived*>(a);
}

// PrintDominatorTree prints the dominator tree in a format that looks like:
//
//    0
//    ╠ 1
//    ╠ 2
//    ╚ 3
//      ╠ 4
//      ║ ╠ 5
//      ║ ╚ 6
//      ╚ 7
//        ╠ 8
//        ╚ 16
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
    PrintF("%d\n", index().id());
    tree_symbols.push_back("");
  } else {
    // This node is not the root of the tree; we start by printing the
    // connectors of the previous levels.
    for (const char* s : tree_symbols) PrintF("%s", s);
    // Then, we print the node id, preceeded by a ╠ or ╚ connector.
    const char* tree_connector_symbol = has_next ? "╠" : "╚";
    PrintF("%s %d\n", tree_connector_symbol, index().id());
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
  os << "\n" << block.kind() << " " << block.index();
  if (block.IsDeferred()) os << " (deferred)";
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
    os << PrintAsBlockHeader{block} << "\n";
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
