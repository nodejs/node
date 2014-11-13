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

BasicBlock::BasicBlock(Zone* zone, Id id)
    : ao_number_(-1),
      rpo_number_(-1),
      deferred_(false),
      dominator_depth_(-1),
      dominator_(NULL),
      loop_header_(NULL),
      loop_end_(NULL),
      loop_depth_(0),
      control_(kNone),
      control_input_(NULL),
      nodes_(zone),
      successors_(zone),
      predecessors_(zone),
      id_(id) {}


bool BasicBlock::LoopContains(BasicBlock* block) const {
  // RPO numbers must be initialized.
  DCHECK(rpo_number_ >= 0);
  DCHECK(block->rpo_number_ >= 0);
  if (loop_end_ == NULL) return false;  // This is not a loop.
  return block->rpo_number_ >= rpo_number_ &&
         block->rpo_number_ < loop_end_->rpo_number_;
}


void BasicBlock::AddSuccessor(BasicBlock* successor) {
  successors_.push_back(successor);
}


void BasicBlock::AddPredecessor(BasicBlock* predecessor) {
  predecessors_.push_back(predecessor);
}


void BasicBlock::AddNode(Node* node) { nodes_.push_back(node); }


void BasicBlock::set_control(Control control) {
  control_ = control;
}


void BasicBlock::set_control_input(Node* control_input) {
  control_input_ = control_input;
}


void BasicBlock::set_dominator_depth(int32_t dominator_depth) {
  dominator_depth_ = dominator_depth;
}


void BasicBlock::set_dominator(BasicBlock* dominator) {
  dominator_ = dominator;
}


void BasicBlock::set_loop_depth(int32_t loop_depth) {
  loop_depth_ = loop_depth;
}


void BasicBlock::set_rpo_number(int32_t rpo_number) {
  rpo_number_ = rpo_number;
}


void BasicBlock::set_loop_end(BasicBlock* loop_end) { loop_end_ = loop_end; }


void BasicBlock::set_loop_header(BasicBlock* loop_header) {
  loop_header_ = loop_header;
}


std::ostream& operator<<(std::ostream& os, const BasicBlock::Control& c) {
  switch (c) {
    case BasicBlock::kNone:
      return os << "none";
    case BasicBlock::kGoto:
      return os << "goto";
    case BasicBlock::kBranch:
      return os << "branch";
    case BasicBlock::kReturn:
      return os << "return";
    case BasicBlock::kThrow:
      return os << "throw";
  }
  UNREACHABLE();
  return os;
}


std::ostream& operator<<(std::ostream& os, const BasicBlock::Id& id) {
  return os << id.ToSize();
}


std::ostream& operator<<(std::ostream& os, const BasicBlock::RpoNumber& rpo) {
  return os << rpo.ToSize();
}


Schedule::Schedule(Zone* zone, size_t node_count_hint)
    : zone_(zone),
      all_blocks_(zone),
      nodeid_to_block_(zone),
      rpo_order_(zone),
      start_(NewBasicBlock()),
      end_(NewBasicBlock()) {
  nodeid_to_block_.reserve(node_count_hint);
}


BasicBlock* Schedule::block(Node* node) const {
  if (node->id() < static_cast<NodeId>(nodeid_to_block_.size())) {
    return nodeid_to_block_[node->id()];
  }
  return NULL;
}


bool Schedule::IsScheduled(Node* node) {
  int length = static_cast<int>(nodeid_to_block_.size());
  if (node->id() >= length) return false;
  return nodeid_to_block_[node->id()] != NULL;
}


BasicBlock* Schedule::GetBlockById(BasicBlock::Id block_id) {
  DCHECK(block_id.ToSize() < all_blocks_.size());
  return all_blocks_[block_id.ToSize()];
}


bool Schedule::SameBasicBlock(Node* a, Node* b) const {
  BasicBlock* block = this->block(a);
  return block != NULL && block == this->block(b);
}


BasicBlock* Schedule::NewBasicBlock() {
  BasicBlock* block = new (zone_)
      BasicBlock(zone_, BasicBlock::Id::FromSize(all_blocks_.size()));
  all_blocks_.push_back(block);
  return block;
}


void Schedule::PlanNode(BasicBlock* block, Node* node) {
  if (FLAG_trace_turbo_scheduler) {
    OFStream os(stdout);
    os << "Planning #" << node->id() << ":" << node->op()->mnemonic()
       << " for future add to B" << block->id() << "\n";
  }
  DCHECK(this->block(node) == NULL);
  SetBlockForNode(block, node);
}


void Schedule::AddNode(BasicBlock* block, Node* node) {
  if (FLAG_trace_turbo_scheduler) {
    OFStream os(stdout);
    os << "Adding #" << node->id() << ":" << node->op()->mnemonic() << " to B"
       << block->id() << "\n";
  }
  DCHECK(this->block(node) == NULL || this->block(node) == block);
  block->AddNode(node);
  SetBlockForNode(block, node);
}


void Schedule::AddGoto(BasicBlock* block, BasicBlock* succ) {
  DCHECK(block->control() == BasicBlock::kNone);
  block->set_control(BasicBlock::kGoto);
  AddSuccessor(block, succ);
}


void Schedule::AddBranch(BasicBlock* block, Node* branch, BasicBlock* tblock,
                         BasicBlock* fblock) {
  DCHECK(block->control() == BasicBlock::kNone);
  DCHECK(branch->opcode() == IrOpcode::kBranch);
  block->set_control(BasicBlock::kBranch);
  AddSuccessor(block, tblock);
  AddSuccessor(block, fblock);
  SetControlInput(block, branch);
}


void Schedule::AddReturn(BasicBlock* block, Node* input) {
  DCHECK(block->control() == BasicBlock::kNone);
  block->set_control(BasicBlock::kReturn);
  SetControlInput(block, input);
  if (block != end()) AddSuccessor(block, end());
}


void Schedule::AddThrow(BasicBlock* block, Node* input) {
  DCHECK(block->control() == BasicBlock::kNone);
  block->set_control(BasicBlock::kThrow);
  SetControlInput(block, input);
  if (block != end()) AddSuccessor(block, end());
}


void Schedule::InsertBranch(BasicBlock* block, BasicBlock* end, Node* branch,
                            BasicBlock* tblock, BasicBlock* fblock) {
  DCHECK(block->control() != BasicBlock::kNone);
  DCHECK(end->control() == BasicBlock::kNone);
  end->set_control(block->control());
  block->set_control(BasicBlock::kBranch);
  MoveSuccessors(block, end);
  AddSuccessor(block, tblock);
  AddSuccessor(block, fblock);
  if (block->control_input() != NULL) {
    SetControlInput(end, block->control_input());
  }
  SetControlInput(block, branch);
}


void Schedule::AddSuccessor(BasicBlock* block, BasicBlock* succ) {
  block->AddSuccessor(succ);
  succ->AddPredecessor(block);
}


void Schedule::MoveSuccessors(BasicBlock* from, BasicBlock* to) {
  for (BasicBlock::Predecessors::iterator i = from->successors_begin();
       i != from->successors_end(); ++i) {
    BasicBlock* succ = *i;
    to->AddSuccessor(succ);
    for (BasicBlock::Predecessors::iterator j = succ->predecessors_begin();
         j != succ->predecessors_end(); ++j) {
      if (*j == from) *j = to;
    }
  }
  from->ClearSuccessors();
}


void Schedule::SetControlInput(BasicBlock* block, Node* node) {
  block->set_control_input(node);
  SetBlockForNode(block, node);
}


void Schedule::SetBlockForNode(BasicBlock* block, Node* node) {
  int length = static_cast<int>(nodeid_to_block_.size());
  if (node->id() >= length) {
    nodeid_to_block_.resize(node->id() + 1);
  }
  nodeid_to_block_[node->id()] = block;
}


std::ostream& operator<<(std::ostream& os, const Schedule& s) {
  // TODO(svenpanne) Const-correct the RPO stuff/iterators.
  BasicBlockVector* rpo = const_cast<Schedule*>(&s)->rpo_order();
  for (BasicBlockVectorIter i = rpo->begin(); i != rpo->end(); ++i) {
    BasicBlock* block = *i;
    os << "--- BLOCK B" << block->id();
    if (block->deferred()) os << " (deferred)";
    if (block->PredecessorCount() != 0) os << " <- ";
    bool comma = false;
    for (BasicBlock::Predecessors::iterator j = block->predecessors_begin();
         j != block->predecessors_end(); ++j) {
      if (comma) os << ", ";
      comma = true;
      os << "B" << (*j)->id();
    }
    os << " ---\n";
    for (BasicBlock::const_iterator j = block->begin(); j != block->end();
         ++j) {
      Node* node = *j;
      os << "  " << *node;
      if (NodeProperties::IsTyped(node)) {
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
    BasicBlock::Control control = block->control();
    if (control != BasicBlock::kNone) {
      os << "  ";
      if (block->control_input() != NULL) {
        os << *block->control_input();
      } else {
        os << "Goto";
      }
      os << " -> ";
      comma = false;
      for (BasicBlock::Successors::iterator j = block->successors_begin();
           j != block->successors_end(); ++j) {
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
