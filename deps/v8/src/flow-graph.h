// Copyright 2010 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_FLOW_GRAPH_H_
#define V8_FLOW_GRAPH_H_

#include "v8.h"

#include "data-flow.h"
#include "zone.h"

namespace v8 {
namespace internal {

// The nodes of a flow graph are basic blocks.  Basic blocks consist of
// instructions represented as pointers to AST nodes in the order that they
// would be visited by the code generator.  A block can have arbitrarily many
// (even zero) predecessors and up to two successors.  Blocks with multiple
// predecessors are "join nodes" and blocks with multiple successors are
// "branch nodes".  A block can be both a branch and a join node.
//
// Flow graphs are in edge split form: a branch node is never the
// predecessor of a merge node.  Empty basic blocks are inserted to maintain
// edge split form.
class BasicBlock: public ZoneObject {
 public:
  // Construct a basic block with a given predecessor.  NULL indicates no
  // predecessor or that the predecessor will be set later.
  explicit BasicBlock(BasicBlock* predecessor)
      : predecessors_(2),
        instructions_(8),
        left_successor_(NULL),
        right_successor_(NULL),
        mark_(false) {
    if (predecessor != NULL) AddPredecessor(predecessor);
  }

  bool HasPredecessor() { return !predecessors_.is_empty(); }
  bool HasSuccessor() { return left_successor_ != NULL; }

  // Add a given basic block as a predecessor of this block.  This function
  // also adds this block as a successor of the given block.
  void AddPredecessor(BasicBlock* predecessor) {
    ASSERT(predecessor != NULL);
    predecessors_.Add(predecessor);
    predecessor->AddSuccessor(this);
  }

  // Add an instruction to the end of this block.  The block must be "open"
  // by not having a successor yet.
  void AddInstruction(AstNode* instruction) {
    ASSERT(!HasSuccessor() && instruction != NULL);
    instructions_.Add(instruction);
  }

  // Perform a depth-first traversal of graph rooted at this node,
  // accumulating pre- and postorder traversal orders.  Visited nodes are
  // marked with mark.
  void BuildTraversalOrder(ZoneList<BasicBlock*>* preorder,
                           ZoneList<BasicBlock*>* postorder,
                           bool mark);
  bool GetMark() { return mark_; }

#ifdef DEBUG
  // In debug mode, blocks are numbered in reverse postorder to help with
  // printing.
  int number() { return number_; }
  void set_number(int n) { number_ = n; }

  // Print a basic block, given the number of the first instruction.
  // Returns the next number after the number of the last instruction.
  int PrintAsText(int instruction_number);
#endif

 private:
  // Add a given basic block as successor to this block.  This function does
  // not add this block as a predecessor of the given block so as to avoid
  // circularity.
  void AddSuccessor(BasicBlock* successor) {
    ASSERT(right_successor_ == NULL && successor != NULL);
    if (HasSuccessor()) {
      right_successor_ = successor;
    } else {
      left_successor_ = successor;
    }
  }

  ZoneList<BasicBlock*> predecessors_;
  ZoneList<AstNode*> instructions_;
  BasicBlock* left_successor_;
  BasicBlock* right_successor_;

  // Support for graph traversal.  Before traversal, all nodes in the graph
  // have the same mark (true or false).  Traversal marks already-visited
  // nodes with the opposite mark.  After traversal, all nodes again have
  // the same mark.  Traversal of the same graph is not reentrant.
  bool mark_;

#ifdef DEBUG
  int number_;
#endif

  DISALLOW_COPY_AND_ASSIGN(BasicBlock);
};


// A flow graph has distinguished entry and exit blocks.  The entry block is
// the only one with no predecessors and the exit block is the only one with
// no successors.
class FlowGraph: public ZoneObject {
 public:
  FlowGraph(BasicBlock* entry, BasicBlock* exit)
      : entry_(entry), exit_(exit), preorder_(8), postorder_(8) {
  }

  ZoneList<BasicBlock*>* preorder() { return &preorder_; }
  ZoneList<BasicBlock*>* postorder() { return &postorder_; }

#ifdef DEBUG
  void PrintAsText(Handle<String> name);
#endif

 private:
  BasicBlock* entry_;
  BasicBlock* exit_;
  ZoneList<BasicBlock*> preorder_;
  ZoneList<BasicBlock*> postorder_;
};


// The flow graph builder walks the AST adding reachable AST nodes to the
// flow graph as instructions.  It remembers the entry and exit nodes of the
// graph, and keeps a pointer to the current block being constructed.
class FlowGraphBuilder: public AstVisitor {
 public:
  FlowGraphBuilder() {}

  FlowGraph* Build(FunctionLiteral* lit);

 private:
  // AST node visit functions.
#define DECLARE_VISIT(type) virtual void Visit##type(type* node);
  AST_NODE_LIST(DECLARE_VISIT)
#undef DECLARE_VISIT

  BasicBlock* entry_;
  BasicBlock* exit_;
  BasicBlock* current_;

  DISALLOW_COPY_AND_ASSIGN(FlowGraphBuilder);
};


} }  // namespace v8::internal

#endif  // V8_FLOW_GRAPH_H_
