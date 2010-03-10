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

  int length() const { return length_; }

 private:
  int length_;
  int data_length_;
  uint32_t* data_;
};


// Forward declarations of Node types.
class Node;
class BranchNode;
class JoinNode;

// Flow graphs have a single entry and single exit.  The empty flowgraph is
// represented by both entry and exit being NULL.
class FlowGraph BASE_EMBEDDED {
 public:
  FlowGraph() : entry_(NULL), exit_(NULL) {}

  static FlowGraph Empty() { return FlowGraph(); }

  bool is_empty() const { return entry_ == NULL; }
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
  void PrintText(ZoneList<Node*>* postorder);
#endif

 private:
  Node* entry_;
  Node* exit_;
};


// Flow-graph nodes.
class Node: public ZoneObject {
 public:
  Node() : number_(-1), mark_(false) {}

  virtual ~Node() {}

  virtual bool IsBlockNode() { return false; }
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

#ifdef DEBUG
  virtual void AssignNumbers();
  virtual void PrintText() = 0;
#endif

 private:
  int number_;
  bool mark_;

  DISALLOW_COPY_AND_ASSIGN(Node);
};


// An entry node has no predecessors and a single successor.
class EntryNode: public Node {
 public:
  EntryNode() : successor_(NULL) {}

  void AddPredecessor(Node* predecessor) { UNREACHABLE(); }

  void AddSuccessor(Node* successor) {
    ASSERT(successor_ == NULL && successor != NULL);
    successor_ = successor;
  }

  void Traverse(bool mark,
                ZoneList<Node*>* preorder,
                ZoneList<Node*>* postorder);

#ifdef DEBUG
  void PrintText();
#endif

 private:
  Node* successor_;

  DISALLOW_COPY_AND_ASSIGN(EntryNode);
};


// An exit node has a arbitrarily many predecessors and no successors.
class ExitNode: public Node {
 public:
  ExitNode() : predecessors_(4) {}

  void AddPredecessor(Node* predecessor) {
    ASSERT(predecessor != NULL);
    predecessors_.Add(predecessor);
  }

  void AddSuccessor(Node* successor) { /* Do nothing. */ }

  void Traverse(bool mark,
                ZoneList<Node*>* preorder,
                ZoneList<Node*>* postorder);

#ifdef DEBUG
  void PrintText();
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

  bool IsBlockNode() { return true; }

  void AddPredecessor(Node* predecessor) {
    ASSERT(predecessor_ == NULL && predecessor != NULL);
    predecessor_ = predecessor;
  }

  void AddSuccessor(Node* successor) {
    ASSERT(successor_ == NULL && successor != NULL);
    successor_ = successor;
  }

  void AddInstruction(AstNode* instruction) {
    instructions_.Add(instruction);
  }

  void Traverse(bool mark,
                ZoneList<Node*>* preorder,
                ZoneList<Node*>* postorder);

#ifdef DEBUG
  void AssignNumbers();
  void PrintText();
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

  void AddPredecessor(Node* predecessor) {
    ASSERT(predecessor_ == NULL && predecessor != NULL);
    predecessor_ = predecessor;
  }

  void AddSuccessor(Node* successor) {
    ASSERT(successor1_ == NULL && successor != NULL);
    if (successor0_ == NULL) {
      successor0_ = successor;
    } else {
      successor1_ = successor;
    }
  }

  void Traverse(bool mark,
                ZoneList<Node*>* preorder,
                ZoneList<Node*>* postorder);

#ifdef DEBUG
  void PrintText();
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

  bool IsJoinNode() { return true; }

  void AddPredecessor(Node* predecessor) {
    ASSERT(predecessor != NULL);
    predecessors_.Add(predecessor);
  }

  void AddSuccessor(Node* successor) {
    ASSERT(successor_ == NULL && successor != NULL);
    successor_ = successor;
  }

  void Traverse(bool mark,
                ZoneList<Node*>* preorder,
                ZoneList<Node*>* postorder);

#ifdef DEBUG
  void PrintText();
#endif

 private:
  ZoneList<Node*> predecessors_;
  Node* successor_;

  DISALLOW_COPY_AND_ASSIGN(JoinNode);
};


// Construct a flow graph from a function literal.  Build pre- and postorder
// traversal orders as a byproduct.
class FlowGraphBuilder: public AstVisitor {
 public:
  FlowGraphBuilder()
      : global_exit_(NULL),
        preorder_(4),
        postorder_(4),
        definitions_(4) {
  }

  void Build(FunctionLiteral* lit);

  FlowGraph* graph() { return &graph_; }

  ZoneList<Node*>* postorder() { return &postorder_; }

 private:
  ExitNode* global_exit() { return global_exit_; }

  // AST node visit functions.
#define DECLARE_VISIT(type) virtual void Visit##type(type* node);
  AST_NODE_LIST(DECLARE_VISIT)
#undef DECLARE_VISIT

  FlowGraph graph_;
  ExitNode* global_exit_;
  ZoneList<Node*> preorder_;
  ZoneList<Node*> postorder_;

  // The flow graph builder collects a list of definitions (assignments and
  // count operations) to stack-allocated variables to use for reaching
  // definitions analysis.
  ZoneList<AstNode*> definitions_;

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


class VarUseMap : public HashMap {
 public:
  VarUseMap() : HashMap(VarMatch) {}

  ZoneList<Expression*>* Lookup(Variable* var);

 private:
  static bool VarMatch(void* key1, void* key2) { return key1 == key2; }
};


class DefinitionInfo : public ZoneObject {
 public:
  explicit DefinitionInfo() : last_use_(NULL) {}

  Expression* last_use() { return last_use_; }
  void set_last_use(Expression* expr) { last_use_ = expr; }

 private:
  Expression* last_use_;
  Register location_;
};


class LivenessAnalyzer : public AstVisitor {
 public:
  LivenessAnalyzer() {}

  void Analyze(FunctionLiteral* fun);

 private:
  void VisitStatements(ZoneList<Statement*>* stmts);

  void RecordUse(Variable* var, Expression* expr);
  void RecordDef(Variable* var, Expression* expr);


  // AST node visit functions.
#define DECLARE_VISIT(type) virtual void Visit##type(type* node);
  AST_NODE_LIST(DECLARE_VISIT)
#undef DECLARE_VISIT

  // Map for tracking the live variables.
  VarUseMap live_vars_;

  DISALLOW_COPY_AND_ASSIGN(LivenessAnalyzer);
};


} }  // namespace v8::internal


#endif  // V8_DATAFLOW_H_
