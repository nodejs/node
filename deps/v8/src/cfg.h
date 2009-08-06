// Copyright 2009 the V8 project authors. All rights reserved.
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

#ifndef V8_CFG_H_
#define V8_CFG_H_

#include "ast.h"

namespace v8 {
namespace internal {

class ExitNode;

// A convenient class to keep 'global' values when building a CFG.  Since
// CFG construction can be invoked recursively, CFG globals are stacked.
class CfgGlobals BASE_EMBEDDED {
 public:
  explicit CfgGlobals(FunctionLiteral* fun);

  ~CfgGlobals() { top_ = previous_; }

  static CfgGlobals* current() {
    ASSERT(top_ != NULL);
    return top_;
  }

  FunctionLiteral* fun() { return global_fun_; }

  ExitNode* exit() { return global_exit_; }

#ifdef DEBUG
  int next_number() { return node_counter_++; }
#endif

 private:
  static CfgGlobals* top_;

  // Function literal currently compiling.
  FunctionLiteral* global_fun_;

  // Shared global exit node for all returns from the same function.
  ExitNode* global_exit_;

#ifdef DEBUG
  // Used to number nodes when printing.
  int node_counter_;
#endif

  CfgGlobals* previous_;
};


// Values appear in instructions.  They represent trivial source
// expressions: ones with no side effects and that do not require code to be
// generated.
class Value : public ZoneObject {
 public:
  virtual ~Value() {}

  virtual void ToRegister(MacroAssembler* masm, Register reg) = 0;

#ifdef DEBUG
  virtual void Print() = 0;
#endif
};


// A compile-time constant that appeared as a literal in the source AST.
class Constant : public Value {
 public:
  explicit Constant(Handle<Object> handle) : handle_(handle) {}

  virtual ~Constant() {}

  void ToRegister(MacroAssembler* masm, Register reg);

#ifdef DEBUG
  void Print();
#endif

 private:
  Handle<Object> handle_;
};


// Locations are values that can be stored into ('lvalues').
class Location : public Value {
 public:
  virtual ~Location() {}

  virtual void ToRegister(MacroAssembler* masm, Register reg) = 0;

#ifdef DEBUG
  virtual void Print() = 0;
#endif
};


// SlotLocations represent parameters and stack-allocated (i.e.,
// non-context) local variables.
class SlotLocation : public Location {
 public:
  SlotLocation(Slot::Type type, int index) : type_(type), index_(index) {}

  void ToRegister(MacroAssembler* masm, Register reg);

#ifdef DEBUG
  void Print();
#endif

 private:
  Slot::Type type_;
  int index_;
};


// Instructions are computations.  The represent non-trivial source
// expressions: typically ones that have side effects and require code to
// be generated.
class Instruction : public ZoneObject {
 public:
  virtual ~Instruction() {}

  virtual void Compile(MacroAssembler* masm) = 0;

#ifdef DEBUG
  virtual void Print() = 0;
#endif
};


// Return a value.
class ReturnInstr : public Instruction {
 public:
  explicit ReturnInstr(Value* value) : value_(value) {}

  virtual ~ReturnInstr() {}

  void Compile(MacroAssembler* masm);

#ifdef DEBUG
  void Print();
#endif

 private:
  Value* value_;
};


// Nodes make up control-flow graphs.  They contain single-entry,
// single-exit blocks of instructions and administrative nodes making up the
// graph structure.
class CfgNode : public ZoneObject {
 public:
  CfgNode() : is_marked_(false) {
#ifdef DEBUG
    number_ = -1;
#endif
  }

  virtual ~CfgNode() {}

  bool is_marked() { return is_marked_; }

  virtual bool is_block() { return false; }

  virtual void Unmark() = 0;

  virtual void Compile(MacroAssembler* masm) = 0;

#ifdef DEBUG
  int number() {
    if (number_ == -1) number_ = CfgGlobals::current()->next_number();
    return number_;
  }

  virtual void Print() = 0;
#endif

 protected:
  bool is_marked_;

#ifdef DEBUG
  int number_;
#endif
};


// A block is a single-entry, single-exit block of instructions.
class InstructionBlock : public CfgNode {
 public:
  InstructionBlock() : successor_(NULL), instructions_(4) {}

  virtual ~InstructionBlock() {}

  static InstructionBlock* cast(CfgNode* node) {
    ASSERT(node->is_block());
    return reinterpret_cast<InstructionBlock*>(node);
  }

  void set_successor(CfgNode* succ) {
    ASSERT(successor_ == NULL);
    successor_ = succ;
  }

  bool is_block() { return true; }

  void Unmark();

  void Compile(MacroAssembler* masm);

  void Append(Instruction* instr) { instructions_.Add(instr); }

#ifdef DEBUG
  void Print();
#endif

 private:
  CfgNode* successor_;
  ZoneList<Instruction*> instructions_;
};


// The CFG for a function has a distinguished entry node.  It has no
// predecessors and a single successor.  The successor is the block
// containing the function's first instruction.
class EntryNode : public CfgNode {
 public:
  explicit EntryNode(InstructionBlock* succ) : successor_(succ) {}

  virtual ~EntryNode() {}

  void Unmark();

  void Compile(MacroAssembler* masm);

#ifdef DEBUG
  void Print();
#endif

 private:
  InstructionBlock* successor_;
};


// The CFG for a function has a distinguished exit node.  It has no
// successor and arbitrarily many predecessors.  The predecessors are all
// the blocks returning from the function.
class ExitNode : public CfgNode {
 public:
  ExitNode() {}

  virtual ~ExitNode() {}

  void Unmark();

  void Compile(MacroAssembler* masm);

#ifdef DEBUG
  void Print();
#endif
};


// A CFG consists of a linked structure of nodes.  It has a single entry
// node and optionally an exit node.  There is a distinguished global exit
// node that is used as the successor of all blocks that return from the
// function.
//
// Fragments of control-flow graphs, produced when traversing the statements
// and expressions in the source AST, are represented by the same class.
// They have instruction blocks as both their entry and exit (if there is
// one).  Instructions can always be prepended or appended to fragments, and
// fragments can always be concatenated.
//
// A singleton CFG fragment (i.e., with only one node) has the same node as
// both entry and exit (if the exit is available).
class Cfg : public ZoneObject {
 public:
  // Create a singleton CFG fragment.
  explicit Cfg(InstructionBlock* block) : entry_(block), exit_(block) {}

  // Build the CFG for a function.
  static Cfg* Build();

  // The entry and exit nodes.
  CfgNode* entry() { return entry_; }
  CfgNode* exit() { return exit_; }

  // True if the CFG has no nodes.
  bool is_empty() { return entry_ == NULL; }

  // True if the CFG has an available exit node (i.e., it can be appended or
  // concatenated to).
  bool has_exit() { return exit_ != NULL; }

  // Add an entry node to a CFG fragment.  It is no longer a fragment
  // (instructions cannot be prepended).
  void PrependEntryNode();

  // Append an instruction to the end of a CFG fragment.  Assumes it has an
  // available exit.
  void Append(Instruction* instr);

  // Appends a return instruction to the end of a CFG fragment.  It no
  // longer has an available exit node.
  void AppendReturnInstruction(Value* value);

  Handle<Code> Compile(Handle<Script> script);

#ifdef DEBUG
  // Support for printing.
  void Print();
#endif

 private:
  // Entry and exit nodes.
  CfgNode* entry_;
  CfgNode* exit_;
};


// An Expression Builder traverses a trivial expression and returns a value.
class ExpressionBuilder : public AstVisitor {
 public:
  ExpressionBuilder() : value_(new Constant(Handle<Object>::null())) {}

  Value* value() { return value_; }

  // AST node visitors.
#define DECLARE_VISIT(type) void Visit##type(type* node);
  AST_NODE_LIST(DECLARE_VISIT)
#undef DECLARE_VISIT

 private:
  Value* value_;
};


// A StatementBuilder traverses a statement and returns a CFG.
class StatementBuilder : public AstVisitor {
 public:
  StatementBuilder() : cfg_(new Cfg(new InstructionBlock())) {}

  Cfg* cfg() { return cfg_; }

  void VisitStatements(ZoneList<Statement*>* stmts);

  // AST node visitors.
#define DECLARE_VISIT(type) void Visit##type(type* node);
  AST_NODE_LIST(DECLARE_VISIT)
#undef DECLARE_VISIT

 private:
  Cfg* cfg_;
};


} }  // namespace v8::internal

#endif  // V8_CFG_H_
