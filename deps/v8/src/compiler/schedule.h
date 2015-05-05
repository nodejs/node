// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_SCHEDULE_H_
#define V8_COMPILER_SCHEDULE_H_

#include <iosfwd>

#include "src/zone-containers.h"

namespace v8 {
namespace internal {
namespace compiler {

// Forward declarations.
class BasicBlock;
class BasicBlockInstrumentor;
class Node;


typedef ZoneVector<BasicBlock*> BasicBlockVector;
typedef ZoneVector<Node*> NodeVector;


// A basic block contains an ordered list of nodes and ends with a control
// node. Note that if a basic block has phis, then all phis must appear as the
// first nodes in the block.
class BasicBlock FINAL : public ZoneObject {
 public:
  // Possible control nodes that can end a block.
  enum Control {
    kNone,        // Control not initialized yet.
    kGoto,        // Goto a single successor block.
    kCall,        // Call with continuation as first successor, exception
                  // second.
    kBranch,      // Branch if true to first successor, otherwise second.
    kSwitch,      // Table dispatch to one of the successor blocks.
    kDeoptimize,  // Return a value from this method.
    kReturn,      // Return a value from this method.
    kThrow        // Throw an exception.
  };

  class Id {
   public:
    int ToInt() const { return static_cast<int>(index_); }
    size_t ToSize() const { return index_; }
    static Id FromSize(size_t index) { return Id(index); }
    static Id FromInt(int index) { return Id(static_cast<size_t>(index)); }

   private:
    explicit Id(size_t index) : index_(index) {}
    size_t index_;
  };

  BasicBlock(Zone* zone, Id id);

  Id id() const { return id_; }

  // Predecessors.
  BasicBlockVector& predecessors() { return predecessors_; }
  const BasicBlockVector& predecessors() const { return predecessors_; }
  size_t PredecessorCount() const { return predecessors_.size(); }
  BasicBlock* PredecessorAt(size_t index) { return predecessors_[index]; }
  void ClearPredecessors() { predecessors_.clear(); }
  void AddPredecessor(BasicBlock* predecessor);

  // Successors.
  BasicBlockVector& successors() { return successors_; }
  const BasicBlockVector& successors() const { return successors_; }
  size_t SuccessorCount() const { return successors_.size(); }
  BasicBlock* SuccessorAt(size_t index) { return successors_[index]; }
  void ClearSuccessors() { successors_.clear(); }
  void AddSuccessor(BasicBlock* successor);

  // Nodes in the basic block.
  typedef Node* value_type;
  bool empty() const { return nodes_.empty(); }
  size_t size() const { return nodes_.size(); }
  Node* NodeAt(size_t index) { return nodes_[index]; }
  size_t NodeCount() const { return nodes_.size(); }

  value_type& front() { return nodes_.front(); }
  value_type const& front() const { return nodes_.front(); }

  typedef NodeVector::iterator iterator;
  iterator begin() { return nodes_.begin(); }
  iterator end() { return nodes_.end(); }

  typedef NodeVector::const_iterator const_iterator;
  const_iterator begin() const { return nodes_.begin(); }
  const_iterator end() const { return nodes_.end(); }

  typedef NodeVector::reverse_iterator reverse_iterator;
  reverse_iterator rbegin() { return nodes_.rbegin(); }
  reverse_iterator rend() { return nodes_.rend(); }

  void AddNode(Node* node);
  template <class InputIterator>
  void InsertNodes(iterator insertion_point, InputIterator insertion_start,
                   InputIterator insertion_end) {
    nodes_.insert(insertion_point, insertion_start, insertion_end);
  }

  // Accessors.
  Control control() const { return control_; }
  void set_control(Control control);

  Node* control_input() const { return control_input_; }
  void set_control_input(Node* control_input);

  bool deferred() const { return deferred_; }
  void set_deferred(bool deferred) { deferred_ = deferred; }

  int32_t dominator_depth() const { return dominator_depth_; }
  void set_dominator_depth(int32_t depth) { dominator_depth_ = depth; }

  BasicBlock* dominator() const { return dominator_; }
  void set_dominator(BasicBlock* dominator) { dominator_ = dominator; }

  BasicBlock* rpo_next() const { return rpo_next_; }
  void set_rpo_next(BasicBlock* rpo_next) { rpo_next_ = rpo_next; }

  BasicBlock* loop_header() const { return loop_header_; }
  void set_loop_header(BasicBlock* loop_header);

  BasicBlock* loop_end() const { return loop_end_; }
  void set_loop_end(BasicBlock* loop_end);

  int32_t loop_depth() const { return loop_depth_; }
  void set_loop_depth(int32_t loop_depth);

  int32_t loop_number() const { return loop_number_; }
  void set_loop_number(int32_t loop_number) { loop_number_ = loop_number; }

  int32_t rpo_number() const { return rpo_number_; }
  void set_rpo_number(int32_t rpo_number);

  // Loop membership helpers.
  inline bool IsLoopHeader() const { return loop_end_ != NULL; }
  bool LoopContains(BasicBlock* block) const;

  // Computes the immediate common dominator of {b1} and {b2}. The worst time
  // complexity is O(N) where N is the height of the dominator tree.
  static BasicBlock* GetCommonDominator(BasicBlock* b1, BasicBlock* b2);

 private:
  int32_t loop_number_;      // loop number of the block.
  int32_t rpo_number_;       // special RPO number of the block.
  bool deferred_;            // true if the block contains deferred code.
  int32_t dominator_depth_;  // Depth within the dominator tree.
  BasicBlock* dominator_;    // Immediate dominator of the block.
  BasicBlock* rpo_next_;     // Link to next block in special RPO order.
  BasicBlock* loop_header_;  // Pointer to dominating loop header basic block,
                             // NULL if none. For loop headers, this points to
                             // enclosing loop header.
  BasicBlock* loop_end_;     // end of the loop, if this block is a loop header.
  int32_t loop_depth_;       // loop nesting, 0 is top-level

  Control control_;          // Control at the end of the block.
  Node* control_input_;      // Input value for control.
  NodeVector nodes_;         // nodes of this block in forward order.

  BasicBlockVector successors_;
  BasicBlockVector predecessors_;
  Id id_;

  DISALLOW_COPY_AND_ASSIGN(BasicBlock);
};

std::ostream& operator<<(std::ostream&, const BasicBlock::Control&);
std::ostream& operator<<(std::ostream&, const BasicBlock::Id&);


// A schedule represents the result of assigning nodes to basic blocks
// and ordering them within basic blocks. Prior to computing a schedule,
// a graph has no notion of control flow ordering other than that induced
// by the graph's dependencies. A schedule is required to generate code.
class Schedule FINAL : public ZoneObject {
 public:
  explicit Schedule(Zone* zone, size_t node_count_hint = 0);

  // Return the block which contains {node}, if any.
  BasicBlock* block(Node* node) const;

  bool IsScheduled(Node* node);
  BasicBlock* GetBlockById(BasicBlock::Id block_id);

  size_t BasicBlockCount() const { return all_blocks_.size(); }
  size_t RpoBlockCount() const { return rpo_order_.size(); }

  // Check if nodes {a} and {b} are in the same block.
  bool SameBasicBlock(Node* a, Node* b) const;

  // BasicBlock building: create a new block.
  BasicBlock* NewBasicBlock();

  // BasicBlock building: records that a node will later be added to a block but
  // doesn't actually add the node to the block.
  void PlanNode(BasicBlock* block, Node* node);

  // BasicBlock building: add a node to the end of the block.
  void AddNode(BasicBlock* block, Node* node);

  // BasicBlock building: add a goto to the end of {block}.
  void AddGoto(BasicBlock* block, BasicBlock* succ);

  // BasicBlock building: add a call at the end of {block}.
  void AddCall(BasicBlock* block, Node* call, BasicBlock* success_block,
               BasicBlock* exception_block);

  // BasicBlock building: add a branch at the end of {block}.
  void AddBranch(BasicBlock* block, Node* branch, BasicBlock* tblock,
                 BasicBlock* fblock);

  // BasicBlock building: add a switch at the end of {block}.
  void AddSwitch(BasicBlock* block, Node* sw, BasicBlock** succ_blocks,
                 size_t succ_count);

  // BasicBlock building: add a deoptimize at the end of {block}.
  void AddDeoptimize(BasicBlock* block, Node* input);

  // BasicBlock building: add a return at the end of {block}.
  void AddReturn(BasicBlock* block, Node* input);

  // BasicBlock building: add a throw at the end of {block}.
  void AddThrow(BasicBlock* block, Node* input);

  // BasicBlock mutation: insert a branch into the end of {block}.
  void InsertBranch(BasicBlock* block, BasicBlock* end, Node* branch,
                    BasicBlock* tblock, BasicBlock* fblock);

  // BasicBlock mutation: insert a switch into the end of {block}.
  void InsertSwitch(BasicBlock* block, BasicBlock* end, Node* sw,
                    BasicBlock** succ_blocks, size_t succ_count);

  // Exposed publicly for testing only.
  void AddSuccessorForTesting(BasicBlock* block, BasicBlock* succ) {
    return AddSuccessor(block, succ);
  }

  BasicBlockVector* rpo_order() { return &rpo_order_; }
  const BasicBlockVector* rpo_order() const { return &rpo_order_; }

  BasicBlock* start() { return start_; }
  BasicBlock* end() { return end_; }

  Zone* zone() const { return zone_; }

 private:
  friend class Scheduler;
  friend class BasicBlockInstrumentor;

  void AddSuccessor(BasicBlock* block, BasicBlock* succ);
  void MoveSuccessors(BasicBlock* from, BasicBlock* to);

  void SetControlInput(BasicBlock* block, Node* node);
  void SetBlockForNode(BasicBlock* block, Node* node);

  Zone* zone_;
  BasicBlockVector all_blocks_;           // All basic blocks in the schedule.
  BasicBlockVector nodeid_to_block_;      // Map from node to containing block.
  BasicBlockVector rpo_order_;            // Reverse-post-order block list.
  BasicBlock* start_;
  BasicBlock* end_;

  DISALLOW_COPY_AND_ASSIGN(Schedule);
};

std::ostream& operator<<(std::ostream&, const Schedule&);

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_SCHEDULE_H_
