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

#ifndef V8_DATAFLOW_H_
#define V8_DATAFLOW_H_

#include "v8.h"

#include "ast.h"
#include "compiler.h"
#include "zone-inl.h"

namespace v8 {
namespace internal {

class BitVector: public ZoneObject {
 public:
  explicit BitVector(int length)
      : length_(length),
        data_length_(SizeFor(length)),
        data_(Zone::NewArray<uint32_t>(data_length_)) {
    ASSERT(length > 0);
    Clear();
  }

  BitVector(const BitVector& other)
      : length_(other.length()),
        data_length_(SizeFor(length_)),
        data_(Zone::NewArray<uint32_t>(data_length_)) {
    CopyFrom(other);
  }

  static int SizeFor(int length) {
    return 1 + ((length - 1) / 32);
  }

  BitVector& operator=(const BitVector& rhs) {
    if (this != &rhs) CopyFrom(rhs);
    return *this;
  }

  void CopyFrom(const BitVector& other) {
    ASSERT(other.length() == length());
    for (int i = 0; i < data_length_; i++) {
      data_[i] = other.data_[i];
    }
  }

  bool Contains(int i) {
    ASSERT(i >= 0 && i < length());
    uint32_t block = data_[i / 32];
    return (block & (1U << (i % 32))) != 0;
  }

  void Add(int i) {
    ASSERT(i >= 0 && i < length());
    data_[i / 32] |= (1U << (i % 32));
  }

  void Remove(int i) {
    ASSERT(i >= 0 && i < length());
    data_[i / 32] &= ~(1U << (i % 32));
  }

  void Union(const BitVector& other) {
    ASSERT(other.length() == length());
    for (int i = 0; i < data_length_; i++) {
      data_[i] |= other.data_[i];
    }
  }

  void Intersect(const BitVector& other) {
    ASSERT(other.length() == length());
    for (int i = 0; i < data_length_; i++) {
      data_[i] &= other.data_[i];
    }
  }

  void Subtract(const BitVector& other) {
    ASSERT(other.length() == length());
    for (int i = 0; i < data_length_; i++) {
      data_[i] &= ~other.data_[i];
    }
  }

  void Clear() {
    for (int i = 0; i < data_length_; i++) {
      data_[i] = 0;
    }
  }

  bool IsEmpty() const {
    for (int i = 0; i < data_length_; i++) {
      if (data_[i] != 0) return false;
    }
    return true;
  }

  bool Equals(const BitVector& other) {
    for (int i = 0; i < data_length_; i++) {
      if (data_[i] != other.data_[i]) return false;
    }
    return true;
  }

  int length() const { return length_; }

#ifdef DEBUG
  void Print();
#endif

 private:
  int length_;
  int data_length_;
  uint32_t* data_;
};


// Simple fixed-capacity list-based worklist (managed as a queue) of
// pointers to T.
template<typename T>
class WorkList BASE_EMBEDDED {
 public:
  // The worklist cannot grow bigger than size.  We keep one item empty to
  // distinguish between empty and full.
  explicit WorkList(int size)
      : capacity_(size + 1), head_(0), tail_(0), queue_(capacity_) {
    for (int i = 0; i < capacity_; i++) queue_.Add(NULL);
  }

  bool is_empty() { return head_ == tail_; }

  bool is_full() {
    // The worklist is full if head is at 0 and tail is at capacity - 1:
    //   head == 0 && tail == capacity-1 ==> tail - head == capacity - 1
    // or if tail is immediately to the left of head:
    //   tail+1 == head  ==> tail - head == -1
    int diff = tail_ - head_;
    return (diff == -1 || diff == capacity_ - 1);
  }

  void Insert(T* item) {
    ASSERT(!is_full());
    queue_[tail_++] = item;
    if (tail_ == capacity_) tail_ = 0;
  }

  T* Remove() {
    ASSERT(!is_empty());
    T* item = queue_[head_++];
    if (head_ == capacity_) head_ = 0;
    return item;
  }

 private:
  int capacity_;  // Including one empty slot.
  int head_;      // Where the first item is.
  int tail_;      // Where the next inserted item will go.
  List<T*> queue_;
};


struct ReachingDefinitionsData BASE_EMBEDDED {
 public:
  ReachingDefinitionsData() : rd_in_(NULL), kill_(NULL), gen_(NULL) {}

  void Initialize(int definition_count) {
    rd_in_ = new BitVector(definition_count);
    kill_ = new BitVector(definition_count);
    gen_ = new BitVector(definition_count);
  }

  BitVector* rd_in() { return rd_in_; }
  BitVector* kill() { return kill_; }
  BitVector* gen() { return gen_; }

 private:
  BitVector* rd_in_;
  BitVector* kill_;
  BitVector* gen_;
};


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


// This class is used to number all expressions in the AST according to
// their evaluation order (post-order left-to-right traversal).
class AstLabeler: public AstVisitor {
 public:
  AstLabeler() : next_number_(0) {}

  void Label(CompilationInfo* info);

 private:
  CompilationInfo* info() { return info_; }

  void VisitDeclarations(ZoneList<Declaration*>* decls);
  void VisitStatements(ZoneList<Statement*>* stmts);

  // AST node visit functions.
#define DECLARE_VISIT(type) virtual void Visit##type(type* node);
  AST_NODE_LIST(DECLARE_VISIT)
#undef DECLARE_VISIT

  // Traversal number for labelling AST nodes.
  int next_number_;

  CompilationInfo* info_;

  DISALLOW_COPY_AND_ASSIGN(AstLabeler);
};


// Computes the set of assigned variables and annotates variables proxies
// that are trivial sub-expressions and for-loops where the loop variable
// is guaranteed to be a smi.
class AssignedVariablesAnalyzer : public AstVisitor {
 public:
  explicit AssignedVariablesAnalyzer(FunctionLiteral* fun);

  void Analyze();

 private:
  Variable* FindSmiLoopVariable(ForStatement* stmt);

  int BitIndex(Variable* var);

  void RecordAssignedVar(Variable* var);

  void MarkIfTrivial(Expression* expr);

  // Visits an expression saving the accumulator before, clearing
  // it before visting and restoring it after visiting.
  void ProcessExpression(Expression* expr);

  // AST node visit functions.
#define DECLARE_VISIT(type) virtual void Visit##type(type* node);
  AST_NODE_LIST(DECLARE_VISIT)
#undef DECLARE_VISIT

  FunctionLiteral* fun_;

  // Accumulator for assigned variables set.
  BitVector av_;

  DISALLOW_COPY_AND_ASSIGN(AssignedVariablesAnalyzer);
};


class ReachingDefinitions BASE_EMBEDDED {
 public:
  ReachingDefinitions(ZoneList<Node*>* postorder,
                      ZoneList<Expression*>* body_definitions,
                      int variable_count)
      : postorder_(postorder),
        body_definitions_(body_definitions),
        variable_count_(variable_count) {
  }

  static int IndexFor(Variable* var, int variable_count);

  void Compute();

 private:
  // A (postorder) list of flow-graph nodes in the body.
  ZoneList<Node*>* postorder_;

  // A list of all the definitions in the body.
  ZoneList<Expression*>* body_definitions_;

  int variable_count_;

  DISALLOW_COPY_AND_ASSIGN(ReachingDefinitions);
};


class TypeAnalyzer BASE_EMBEDDED {
 public:
  TypeAnalyzer(ZoneList<Node*>* postorder,
              ZoneList<Expression*>* body_definitions,
               int variable_count,
               int param_count)
      : postorder_(postorder),
        body_definitions_(body_definitions),
        variable_count_(variable_count),
        param_count_(param_count) {}

  void Compute();

 private:
  // Get the primitity of definition number i. Definitions are numbered
  // by the flow graph builder.
  bool IsPrimitiveDef(int def_num);

  ZoneList<Node*>* postorder_;
  ZoneList<Expression*>* body_definitions_;
  int variable_count_;
  int param_count_;

  DISALLOW_COPY_AND_ASSIGN(TypeAnalyzer);
};


void MarkLiveCode(ZoneList<Node*>* nodes,
                  ZoneList<Expression*>* body_definitions,
                  int variable_count);


} }  // namespace v8::internal


#endif  // V8_DATAFLOW_H_
