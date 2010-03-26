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

// Flow-graph nodes.
class Node: public ZoneObject {
 public:
  Node() : number_(-1), mark_(false) {}

  virtual ~Node() {}

  virtual bool IsExitNode() { return false; }
  virtual bool IsBlockNode() { return false; }
  virtual bool IsBranchNode() { return false; }
  virtual bool IsJoinNode() { return false; }

  virtual void AddPredecessor(Node* predecessor) = 0;
  virtual void AddSuccessor(Node* successor) = 0;

  bool IsMarkedWith(bool mark) { return mark_ == mark; }
  void MarkWith(bool mark) { mark_ = mark; }

  // Perform a depth first search and record preorder and postorder
  // traversal orders.
  virtual void Traverse(bool mark,
                        ZoneList<Node*>* preorder,
                        ZoneList<Node*>* postorder) = 0;

  int number() { return number_; }
  void set_number(int number) { number_ = number; }

  // Functions used by data-flow analyses.
  virtual void InitializeReachingDefinitions(int definition_count,
                                             List<BitVector*>* variables,
                                             WorkList<Node>* worklist,
                                             bool mark);
  virtual void ComputeRDOut(BitVector* result) = 0;
  virtual void UpdateRDIn(WorkList<Node>* worklist, bool mark) = 0;
  virtual void PropagateReachingDefinitions(List<BitVector*>* variables);

  // Functions used by dead-code elimination.
  virtual void MarkCriticalInstructions(
      List<AstNode*>* stack,
      ZoneList<Expression*>* body_definitions,
      int variable_count);

#ifdef DEBUG
  void AssignNodeNumber();
  void PrintReachingDefinitions();
  virtual void PrintText() = 0;
#endif

 protected:
  ReachingDefinitionsData rd_;

 private:
  int number_;
  bool mark_;

  DISALLOW_COPY_AND_ASSIGN(Node);
};


// An exit node has a arbitrarily many predecessors and no successors.
class ExitNode: public Node {
 public:
  ExitNode() : predecessors_(4) {}

  virtual bool IsExitNode() { return true; }

  virtual void AddPredecessor(Node* predecessor) {
    ASSERT(predecessor != NULL);
    predecessors_.Add(predecessor);
  }

  virtual void AddSuccessor(Node* successor) { UNREACHABLE(); }

  virtual void Traverse(bool mark,
                        ZoneList<Node*>* preorder,
                        ZoneList<Node*>* postorder);

  virtual void ComputeRDOut(BitVector* result);
  virtual void UpdateRDIn(WorkList<Node>* worklist, bool mark);

#ifdef DEBUG
  virtual void PrintText();
#endif

 private:
  ZoneList<Node*> predecessors_;

  DISALLOW_COPY_AND_ASSIGN(ExitNode);
};


// Block nodes have a single successor and predecessor and a list of
// instructions.
class BlockNode: public Node {
 public:
  BlockNode() : predecessor_(NULL), successor_(NULL), instructions_(4) {}

  static BlockNode* cast(Node* node) {
    ASSERT(node->IsBlockNode());
    return reinterpret_cast<BlockNode*>(node);
  }

  virtual bool IsBlockNode() { return true; }

  bool is_empty() { return instructions_.is_empty(); }

  ZoneList<AstNode*>* instructions() { return &instructions_; }

  virtual void AddPredecessor(Node* predecessor) {
    ASSERT(predecessor_ == NULL && predecessor != NULL);
    predecessor_ = predecessor;
  }

  virtual void AddSuccessor(Node* successor) {
    ASSERT(successor_ == NULL && successor != NULL);
    successor_ = successor;
  }

  void AddInstruction(AstNode* instruction) {
    instructions_.Add(instruction);
  }

  virtual void Traverse(bool mark,
                        ZoneList<Node*>* preorder,
                        ZoneList<Node*>* postorder);

  virtual void InitializeReachingDefinitions(int definition_count,
                                             List<BitVector*>* variables,
                                             WorkList<Node>* worklist,
                                             bool mark);
  virtual void ComputeRDOut(BitVector* result);
  virtual void UpdateRDIn(WorkList<Node>* worklist, bool mark);
  virtual void PropagateReachingDefinitions(List<BitVector*>* variables);

  virtual void MarkCriticalInstructions(
      List<AstNode*>* stack,
      ZoneList<Expression*>* body_definitions,
      int variable_count);

#ifdef DEBUG
  virtual void PrintText();
#endif

 private:
  Node* predecessor_;
  Node* successor_;
  ZoneList<AstNode*> instructions_;

  DISALLOW_COPY_AND_ASSIGN(BlockNode);
};


// Branch nodes have a single predecessor and a pair of successors.
class BranchNode: public Node {
 public:
  BranchNode() : predecessor_(NULL), successor0_(NULL), successor1_(NULL) {}

  virtual bool IsBranchNode() { return true; }

  virtual void AddPredecessor(Node* predecessor) {
    ASSERT(predecessor_ == NULL && predecessor != NULL);
    predecessor_ = predecessor;
  }

  virtual void AddSuccessor(Node* successor) {
    ASSERT(successor1_ == NULL && successor != NULL);
    if (successor0_ == NULL) {
      successor0_ = successor;
    } else {
      successor1_ = successor;
    }
  }

  virtual void Traverse(bool mark,
                        ZoneList<Node*>* preorder,
                        ZoneList<Node*>* postorder);

  virtual void ComputeRDOut(BitVector* result);
  virtual void UpdateRDIn(WorkList<Node>* worklist, bool mark);

#ifdef DEBUG
  virtual void PrintText();
#endif

 private:
  Node* predecessor_;
  Node* successor0_;
  Node* successor1_;

  DISALLOW_COPY_AND_ASSIGN(BranchNode);
};


// Join nodes have arbitrarily many predecessors and a single successor.
class JoinNode: public Node {
 public:
  JoinNode() : predecessors_(2), successor_(NULL) {}

  static JoinNode* cast(Node* node) {
    ASSERT(node->IsJoinNode());
    return reinterpret_cast<JoinNode*>(node);
  }

  virtual bool IsJoinNode() { return true; }

  virtual void AddPredecessor(Node* predecessor) {
    ASSERT(predecessor != NULL);
    predecessors_.Add(predecessor);
  }

  virtual void AddSuccessor(Node* successor) {
    ASSERT(successor_ == NULL && successor != NULL);
    successor_ = successor;
  }

  virtual void Traverse(bool mark,
                        ZoneList<Node*>* preorder,
                        ZoneList<Node*>* postorder);

  virtual void ComputeRDOut(BitVector* result);
  virtual void UpdateRDIn(WorkList<Node>* worklist, bool mark);

#ifdef DEBUG
  virtual void PrintText();
#endif

 private:
  ZoneList<Node*> predecessors_;
  Node* successor_;

  DISALLOW_COPY_AND_ASSIGN(JoinNode);
};


// Flow graphs have a single entry and single exit.  The empty flowgraph is
// represented by both entry and exit being NULL.
class FlowGraph BASE_EMBEDDED {
 public:
  static FlowGraph Empty() {
    FlowGraph graph;
    graph.entry_ = new BlockNode();
    graph.exit_ = graph.entry_;
    return graph;
  }

  bool is_empty() const {
    return entry_ == exit_ && BlockNode::cast(entry_)->is_empty();
  }
  Node* entry() const { return entry_; }
  Node* exit() const { return exit_; }

  // Add a single instruction to the end of this flowgraph.
  void AppendInstruction(AstNode* instruction);

  // Add a single node to the end of this flow graph.
  void AppendNode(Node* node);

  // Add a flow graph fragment to the end of this one.
  void AppendGraph(FlowGraph* graph);

  // Concatenate an if-then-else flow-graph to this one.  Control is split
  // and merged, so the graph remains single-entry, single-exit.
  void Split(BranchNode* branch,
             FlowGraph* left,
             FlowGraph* right,
             JoinNode* merge);

  // Concatenate a forward loop (e.g., while or for loop) flow-graph to this
  // one.  Control is split by the condition and merged back from the back
  // edge at end of the body to the beginning of the condition.  The single
  // (free) exit of the result graph is the right (false) arm of the branch
  // node.
  void Loop(JoinNode* merge,
            FlowGraph* condition,
            BranchNode* branch,
            FlowGraph* body);

#ifdef DEBUG
  void PrintText(FunctionLiteral* fun, ZoneList<Node*>* postorder);
#endif

 private:
  FlowGraph() : entry_(NULL), exit_(NULL) {}

  Node* entry_;
  Node* exit_;
};


// Construct a flow graph from a function literal.  Build pre- and postorder
// traversal orders as a byproduct.
class FlowGraphBuilder: public AstVisitor {
 public:
  explicit FlowGraphBuilder(int variable_count)
      : graph_(FlowGraph::Empty()),
        global_exit_(NULL),
        preorder_(4),
        postorder_(4),
        variable_count_(variable_count),
        body_definitions_(4) {
  }

  void Build(FunctionLiteral* lit);

  FlowGraph* graph() { return &graph_; }
  ZoneList<Node*>* preorder() { return &preorder_; }
  ZoneList<Node*>* postorder() { return &postorder_; }
  ZoneList<Expression*>* body_definitions() { return &body_definitions_; }

 private:
  ExitNode* global_exit() { return global_exit_; }

  // Helpers to allow tranforming the ast during flow graph construction.
  void VisitStatements(ZoneList<Statement*>* stmts);
  Statement* ProcessStatement(Statement* stmt);

  // AST node visit functions.
#define DECLARE_VISIT(type) virtual void Visit##type(type* node);
  AST_NODE_LIST(DECLARE_VISIT)
#undef DECLARE_VISIT

  FlowGraph graph_;
  ExitNode* global_exit_;
  ZoneList<Node*> preorder_;
  ZoneList<Node*> postorder_;

  // The flow graph builder collects a list of explicit definitions
  // (assignments and count operations) to stack-allocated variables to use
  // for reaching definitions analysis.  It does not count the implicit
  // definition at function entry.  AST node numbers in the AST are used to
  // refer into this list.
  int variable_count_;
  ZoneList<Expression*> body_definitions_;

  DISALLOW_COPY_AND_ASSIGN(FlowGraphBuilder);
};


} }  // namespace v8::internal

#endif  // V8_FLOW_GRAPH_H_
