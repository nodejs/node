// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/node.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/node-properties-inl.h"
#include "src/compiler/schedule.h"
#include "src/ostreams.h"

namespace v8 {
namespace internal {
namespace compiler {

OStream& operator<<(OStream& os, const BasicBlockData::Control& c) {
  switch (c) {
    case BasicBlockData::kNone:
      return os << "none";
    case BasicBlockData::kGoto:
      return os << "goto";
    case BasicBlockData::kBranch:
      return os << "branch";
    case BasicBlockData::kReturn:
      return os << "return";
    case BasicBlockData::kThrow:
      return os << "throw";
    case BasicBlockData::kCall:
      return os << "call";
    case BasicBlockData::kDeoptimize:
      return os << "deoptimize";
  }
  UNREACHABLE();
  return os;
}


OStream& operator<<(OStream& os, const Schedule& s) {
  // TODO(svenpanne) Const-correct the RPO stuff/iterators.
  BasicBlockVector* rpo = const_cast<Schedule*>(&s)->rpo_order();
  for (BasicBlockVectorIter i = rpo->begin(); i != rpo->end(); ++i) {
    BasicBlock* block = *i;
    os << "--- BLOCK B" << block->id();
    if (block->PredecessorCount() != 0) os << " <- ";
    BasicBlock::Predecessors predecessors = block->predecessors();
    bool comma = false;
    for (BasicBlock::Predecessors::iterator j = predecessors.begin();
         j != predecessors.end(); ++j) {
      if (comma) os << ", ";
      comma = true;
      os << "B" << (*j)->id();
    }
    os << " ---\n";
    for (BasicBlock::const_iterator j = block->begin(); j != block->end();
         ++j) {
      Node* node = *j;
      os << "  " << *node;
      if (!NodeProperties::IsControl(node)) {
        Bounds bounds = NodeProperties::GetBounds(node);
        os << " : ";
        bounds.lower->PrintTo(os);
        if (!bounds.upper->Is(bounds.lower)) {
          os << "..";
          bounds.upper->PrintTo(os);
        }
      }
      os << "\n";
    }
    BasicBlock::Control control = block->control_;
    if (control != BasicBlock::kNone) {
      os << "  ";
      if (block->control_input_ != NULL) {
        os << *block->control_input_;
      } else {
        os << "Goto";
      }
      os << " -> ";
      BasicBlock::Successors successors = block->successors();
      comma = false;
      for (BasicBlock::Successors::iterator j = successors.begin();
           j != successors.end(); ++j) {
        if (comma) os << ", ";
        comma = true;
        os << "B" << (*j)->id();
      }
      os << "\n";
    }
  }
  return os;
}
}  // namespace compiler
}  // namespace internal
}  // namespace v8
