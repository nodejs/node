// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_SCHEDULE_H_
#define V8_COMPILER_SCHEDULE_H_

#include <vector>

#include "src/v8.h"

#include "src/compiler/generic-algorithm.h"
#include "src/compiler/generic-graph.h"
#include "src/compiler/generic-node.h"
#include "src/compiler/generic-node-inl.h"
#include "src/compiler/node.h"
#include "src/compiler/opcodes.h"
#include "src/zone.h"

namespace v8 {
namespace internal {
namespace compiler {

class BasicBlock;
class BasicBlockInstrumentor;
class Graph;
class ConstructScheduleData;
class CodeGenerator;  // Because of a namespace bug in clang.

class BasicBlockData {
 public:
  // Possible control nodes that can end a block.
  enum Control {
    kNone,    // Control not initialized yet.
    kGoto,    // Goto a single successor block.
    kBranch,  // Branch if true to first successor, otherwise second.
    kReturn,  // Return a value from this method.
    kThrow    // Throw an exception.
  };

  int32_t rpo_number_;       // special RPO number of the block.
  BasicBlock* dominator_;    // Immediate dominator of the block.
  BasicBlock* loop_header_;  // Pointer to dominating loop header basic block,
                             // NULL if none. For loop headers, this points to
                             // enclosing loop header.
  int32_t loop_depth_;       // loop nesting, 0 is top-level
  int32_t loop_end_;         // end of the loop, if this block is a loop header.
  int32_t code_start_;       // start index of arch-specific code.
  int32_t code_end_;         // end index of arch-specific code.
  bool deferred_;            // {true} if this block is considered the slow
                             // path.
  Control control_;          // Control at the end of the block.
  Node* control_input_;      // Input value for control.
  NodeVector nodes_;         // nodes of this block in forward order.

  explicit BasicBlockData(Zone* zone)
      : rpo_number_(-1),
        dominator_(NULL),
        loop_header_(NULL),
        loop_depth_(0),
        loop_end_(-1),
        code_start_(-1),
        code_end_(-1),
        deferred_(false),
        control_(kNone),
        control_input_(NULL),
        nodes_(zone) {}

  inline bool IsLoopHeader() const { return loop_end_ >= 0; }
  inline bool LoopContains(BasicBlockData* block) const {
    // RPO numbers must be initialized.
    DCHECK(rpo_number_ >= 0);
    DCHECK(block->rpo_number_ >= 0);
    if (loop_end_ < 0) return false;  // This is not a loop.
    return block->rpo_number_ >= rpo_number_ && block->rpo_number_ < loop_end_;
  }
  int first_instruction_index() {
    DCHECK(code_start_ >= 0);
    DCHECK(code_end_ > 0);
    DCHECK(code_end_ >= code_start_);
    return code_start_;
  }
  int last_instruction_index() {
    DCHECK(code_start_ >= 0);
    DCHECK(code_end_ > 0);
    DCHECK(code_end_ >= code_start_);
    return code_end_ - 1;
  }
};

OStream& operator<<(OStream& os, const BasicBlockData::Control& c);

// A basic block contains an ordered list of nodes and ends with a control
// node. Note that if a basic block has phis, then all phis must appear as the
// first nodes in the block.
class BasicBlock FINAL : public GenericNode<BasicBlockData, BasicBlock> {
 public:
  BasicBlock(GenericGraphBase* graph, int input_count)
      : GenericNode<BasicBlockData, BasicBlock>(graph, input_count) {}

  typedef Uses Successors;
  typedef Inputs Predecessors;

  Successors successors() { return static_cast<Successors>(uses()); }
  Predecessors predecessors() { return static_cast<Predecessors>(inputs()); }

  int PredecessorCount() { return InputCount(); }
  BasicBlock* PredecessorAt(int index) { return InputAt(index); }

  int SuccessorCount() { return UseCount(); }
  BasicBlock* SuccessorAt(int index) { return UseAt(index); }

  int PredecessorIndexOf(BasicBlock* predecessor) {
    BasicBlock::Predecessors predecessors = this->predecessors();
    for (BasicBlock::Predecessors::iterator i = predecessors.begin();
         i != predecessors.end(); ++i) {
      if (*i == predecessor) return i.index();
    }
    return -1;
  }

  inline BasicBlock* loop_header() {
    return static_cast<BasicBlock*>(loop_header_);
  }
  inline BasicBlock* ContainingLoop() {
    if (IsLoopHeader()) return this;
    return static_cast<BasicBlock*>(loop_header_);
  }

  typedef NodeVector::iterator iterator;
  iterator begin() { return nodes_.begin(); }
  iterator end() { return nodes_.end(); }

  typedef NodeVector::const_iterator const_iterator;
  const_iterator begin() const { return nodes_.begin(); }
  const_iterator end() const { return nodes_.end(); }

  typedef NodeVector::reverse_iterator reverse_iterator;
  reverse_iterator rbegin() { return nodes_.rbegin(); }
  reverse_iterator rend() { return nodes_.rend(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(BasicBlock);
};

typedef GenericGraphVisit::NullNodeVisitor<BasicBlockData, BasicBlock>
    NullBasicBlockVisitor;

typedef ZoneVector<BasicBlock*> BasicBlockVector;
typedef BasicBlockVector::iterator BasicBlockVectorIter;
typedef BasicBlockVector::reverse_iterator BasicBlockVectorRIter;

// A schedule represents the result of assigning nodes to basic blocks
// and ordering them within basic blocks. Prior to computing a schedule,
// a graph has no notion of control flow ordering other than that induced
// by the graph's dependencies. A schedule is required to generate code.
class Schedule : public GenericGraph<BasicBlock> {
 public:
  explicit Schedule(Zone* zone, size_t node_count_hint = 0)
      : GenericGraph<BasicBlock>(zone),
        zone_(zone),
        all_blocks_(zone),
        nodeid_to_block_(zone),
        rpo_order_(zone) {
    SetStart(NewBasicBlock());  // entry.
    SetEnd(NewBasicBlock());    // exit.
    nodeid_to_block_.reserve(node_count_hint);
  }

  // Return the block which contains {node}, if any.
  BasicBlock* block(Node* node) const {
    if (node->id() < static_cast<NodeId>(nodeid_to_block_.size())) {
      return nodeid_to_block_[node->id()];
    }
    return NULL;
  }

  bool IsScheduled(Node* node) {
    int length = static_cast<int>(nodeid_to_block_.size());
    if (node->id() >= length) return false;
    return nodeid_to_block_[node->id()] != NULL;
  }

  BasicBlock* GetBlockById(int block_id) { return all_blocks_[block_id]; }

  int BasicBlockCount() const { return NodeCount(); }
  int RpoBlockCount() const { return static_cast<int>(rpo_order_.size()); }

  typedef ContainerPointerWrapper<BasicBlockVector> BasicBlocks;

  // Return a list of all the blocks in the schedule, in arbitrary order.
  BasicBlocks all_blocks() { return BasicBlocks(&all_blocks_); }

  // Check if nodes {a} and {b} are in the same block.
  inline bool SameBasicBlock(Node* a, Node* b) const {
    BasicBlock* block = this->block(a);
    return block != NULL && block == this->block(b);
  }

  // BasicBlock building: create a new block.
  inline BasicBlock* NewBasicBlock() {
    BasicBlock* block =
        BasicBlock::New(this, 0, static_cast<BasicBlock**>(NULL));
    all_blocks_.push_back(block);
    return block;
  }

  // BasicBlock building: records that a node will later be added to a block but
  // doesn't actually add the node to the block.
  inline void PlanNode(BasicBlock* block, Node* node) {
    if (FLAG_trace_turbo_scheduler) {
      PrintF("Planning #%d:%s for future add to B%d\n", node->id(),
             node->op()->mnemonic(), block->id());
    }
    DCHECK(this->block(node) == NULL);
    SetBlockForNode(block, node);
  }

  // BasicBlock building: add a node to the end of the block.
  inline void AddNode(BasicBlock* block, Node* node) {
    if (FLAG_trace_turbo_scheduler) {
      PrintF("Adding #%d:%s to B%d\n", node->id(), node->op()->mnemonic(),
             block->id());
    }
    DCHECK(this->block(node) == NULL || this->block(node) == block);
    block->nodes_.push_back(node);
    SetBlockForNode(block, node);
  }

  // BasicBlock building: add a goto to the end of {block}.
  void AddGoto(BasicBlock* block, BasicBlock* succ) {
    DCHECK(block->control_ == BasicBlock::kNone);
    block->control_ = BasicBlock::kGoto;
    AddSuccessor(block, succ);
  }

  // BasicBlock building: add a branch at the end of {block}.
  void AddBranch(BasicBlock* block, Node* branch, BasicBlock* tblock,
                 BasicBlock* fblock) {
    DCHECK(block->control_ == BasicBlock::kNone);
    DCHECK(branch->opcode() == IrOpcode::kBranch);
    block->control_ = BasicBlock::kBranch;
    AddSuccessor(block, tblock);
    AddSuccessor(block, fblock);
    SetControlInput(block, branch);
    if (branch->opcode() == IrOpcode::kBranch) {
      // TODO(titzer): require a Branch node here. (sloppy tests).
      SetBlockForNode(block, branch);
    }
  }

  // BasicBlock building: add a return at the end of {block}.
  void AddReturn(BasicBlock* block, Node* input) {
    DCHECK(block->control_ == BasicBlock::kNone);
    block->control_ = BasicBlock::kReturn;
    SetControlInput(block, input);
    if (block != end()) AddSuccessor(block, end());
    if (input->opcode() == IrOpcode::kReturn) {
      // TODO(titzer): require a Return node here. (sloppy tests).
      SetBlockForNode(block, input);
    }
  }

  // BasicBlock building: add a throw at the end of {block}.
  void AddThrow(BasicBlock* block, Node* input) {
    DCHECK(block->control_ == BasicBlock::kNone);
    block->control_ = BasicBlock::kThrow;
    SetControlInput(block, input);
    if (block != end()) AddSuccessor(block, end());
  }

  friend class Scheduler;
  friend class CodeGenerator;

  void AddSuccessor(BasicBlock* block, BasicBlock* succ) {
    succ->AppendInput(zone_, block);
  }

  BasicBlockVector* rpo_order() { return &rpo_order_; }

 private:
  friend class ScheduleVisualizer;
  friend class BasicBlockInstrumentor;

  void SetControlInput(BasicBlock* block, Node* node) {
    block->control_input_ = node;
    SetBlockForNode(block, node);
  }

  void SetBlockForNode(BasicBlock* block, Node* node) {
    int length = static_cast<int>(nodeid_to_block_.size());
    if (node->id() >= length) {
      nodeid_to_block_.resize(node->id() + 1);
    }
    nodeid_to_block_[node->id()] = block;
  }

  Zone* zone_;
  BasicBlockVector all_blocks_;           // All basic blocks in the schedule.
  BasicBlockVector nodeid_to_block_;      // Map from node to containing block.
  BasicBlockVector rpo_order_;            // Reverse-post-order block list.
};

OStream& operator<<(OStream& os, const Schedule& s);
}
}
}  // namespace v8::internal::compiler

#endif  // V8_COMPILER_SCHEDULE_H_
