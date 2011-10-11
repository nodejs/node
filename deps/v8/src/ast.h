// Copyright 2011 the V8 project authors. All rights reserved.
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

#ifndef V8_AST_H_
#define V8_AST_H_

#include "allocation.h"
#include "execution.h"
#include "factory.h"
#include "jsregexp.h"
#include "runtime.h"
#include "small-pointer-list.h"
#include "token.h"
#include "variables.h"

namespace v8 {
namespace internal {

// The abstract syntax tree is an intermediate, light-weight
// representation of the parsed JavaScript code suitable for
// compilation to native code.

// Nodes are allocated in a separate zone, which allows faster
// allocation and constant-time deallocation of the entire syntax
// tree.


// ----------------------------------------------------------------------------
// Nodes of the abstract syntax tree. Only concrete classes are
// enumerated here.

#define STATEMENT_NODE_LIST(V)                  \
  V(Block)                                      \
  V(ExpressionStatement)                        \
  V(EmptyStatement)                             \
  V(IfStatement)                                \
  V(ContinueStatement)                          \
  V(BreakStatement)                             \
  V(ReturnStatement)                            \
  V(WithStatement)                              \
  V(SwitchStatement)                            \
  V(DoWhileStatement)                           \
  V(WhileStatement)                             \
  V(ForStatement)                               \
  V(ForInStatement)                             \
  V(TryCatchStatement)                          \
  V(TryFinallyStatement)                        \
  V(DebuggerStatement)

#define EXPRESSION_NODE_LIST(V)                 \
  V(FunctionLiteral)                            \
  V(SharedFunctionInfoLiteral)                  \
  V(Conditional)                                \
  V(VariableProxy)                              \
  V(Literal)                                    \
  V(RegExpLiteral)                              \
  V(ObjectLiteral)                              \
  V(ArrayLiteral)                               \
  V(Assignment)                                 \
  V(Throw)                                      \
  V(Property)                                   \
  V(Call)                                       \
  V(CallNew)                                    \
  V(CallRuntime)                                \
  V(UnaryOperation)                             \
  V(CountOperation)                             \
  V(BinaryOperation)                            \
  V(CompareOperation)                           \
  V(CompareToNull)                              \
  V(ThisFunction)

#define AST_NODE_LIST(V)                        \
  V(Declaration)                                \
  STATEMENT_NODE_LIST(V)                        \
  EXPRESSION_NODE_LIST(V)

// Forward declarations
class BitVector;
class DefinitionInfo;
class MaterializedLiteral;
class TargetCollector;
class TypeFeedbackOracle;

#define DEF_FORWARD_DECLARATION(type) class type;
AST_NODE_LIST(DEF_FORWARD_DECLARATION)
#undef DEF_FORWARD_DECLARATION


// Typedef only introduced to avoid unreadable code.
// Please do appreciate the required space in "> >".
typedef ZoneList<Handle<String> > ZoneStringList;
typedef ZoneList<Handle<Object> > ZoneObjectList;


#define DECLARE_NODE_TYPE(type)                                         \
  virtual void Accept(AstVisitor* v);                                   \
  virtual AstNode::Type node_type() const { return AstNode::k##type; }  \
  virtual type* As##type() { return this; }


class AstNode: public ZoneObject {
 public:
#define DECLARE_TYPE_ENUM(type) k##type,
  enum Type {
    AST_NODE_LIST(DECLARE_TYPE_ENUM)
    kInvalid = -1
  };
#undef DECLARE_TYPE_ENUM

  static const int kNoNumber = -1;
  static const int kFunctionEntryId = 2;  // Using 0 could disguise errors.
  // This AST id identifies the point after the declarations have been
  // visited. We need it to capture the environment effects of declarations
  // that emit code (function declarations).
  static const int kDeclarationsId = 3;

  // Override ZoneObject's new to count allocated AST nodes.
  void* operator new(size_t size, Zone* zone) {
    Isolate* isolate = zone->isolate();
    isolate->set_ast_node_count(isolate->ast_node_count() + 1);
    return zone->New(static_cast<int>(size));
  }

  AstNode() {}

  virtual ~AstNode() { }

  virtual void Accept(AstVisitor* v) = 0;
  virtual Type node_type() const { return kInvalid; }

  // Type testing & conversion functions overridden by concrete subclasses.
#define DECLARE_NODE_FUNCTIONS(type)                  \
  virtual type* As##type() { return NULL; }
  AST_NODE_LIST(DECLARE_NODE_FUNCTIONS)
#undef DECLARE_NODE_FUNCTIONS

  virtual Statement* AsStatement() { return NULL; }
  virtual Expression* AsExpression() { return NULL; }
  virtual TargetCollector* AsTargetCollector() { return NULL; }
  virtual BreakableStatement* AsBreakableStatement() { return NULL; }
  virtual IterationStatement* AsIterationStatement() { return NULL; }
  virtual MaterializedLiteral* AsMaterializedLiteral() { return NULL; }

  // True if the node is simple enough for us to inline calls containing it.
  virtual bool IsInlineable() const = 0;

  static int Count() { return Isolate::Current()->ast_node_count(); }
  static void ResetIds() { Isolate::Current()->set_ast_node_id(0); }

 protected:
  static unsigned GetNextId(Isolate* isolate) {
    return ReserveIdRange(isolate, 1);
  }

  static unsigned ReserveIdRange(Isolate* isolate, int n) {
    unsigned tmp = isolate->ast_node_id();
    isolate->set_ast_node_id(tmp + n);
    return tmp;
  }

 private:
  // Hidden to prevent accidental usage. It would have to load the
  // current zone from the TLS.
  void* operator new(size_t size);

  friend class CaseClause;  // Generates AST IDs.
};


class Statement: public AstNode {
 public:
  Statement() : statement_pos_(RelocInfo::kNoPosition) {}

  virtual Statement* AsStatement()  { return this; }

  virtual Assignment* StatementAsSimpleAssignment() { return NULL; }
  virtual CountOperation* StatementAsCountOperation() { return NULL; }

  bool IsEmpty() { return AsEmptyStatement() != NULL; }

  void set_statement_pos(int statement_pos) { statement_pos_ = statement_pos; }
  int statement_pos() const { return statement_pos_; }

 private:
  int statement_pos_;
};


class SmallMapList {
 public:
  SmallMapList() {}
  explicit SmallMapList(int capacity) : list_(capacity) {}

  void Reserve(int capacity) { list_.Reserve(capacity); }
  void Clear() { list_.Clear(); }

  bool is_empty() const { return list_.is_empty(); }
  int length() const { return list_.length(); }

  void Add(Handle<Map> handle) {
    list_.Add(handle.location());
  }

  Handle<Map> at(int i) const {
    return Handle<Map>(list_.at(i));
  }

  Handle<Map> first() const { return at(0); }
  Handle<Map> last() const { return at(length() - 1); }

 private:
  // The list stores pointers to Map*, that is Map**, so it's GC safe.
  SmallPointerList<Map*> list_;

  DISALLOW_COPY_AND_ASSIGN(SmallMapList);
};


class Expression: public AstNode {
 public:
  enum Context {
    // Not assigned a context yet, or else will not be visited during
    // code generation.
    kUninitialized,
    // Evaluated for its side effects.
    kEffect,
    // Evaluated for its value (and side effects).
    kValue,
    // Evaluated for control flow (and side effects).
    kTest
  };

  explicit Expression(Isolate* isolate)
      : id_(GetNextId(isolate)),
        test_id_(GetNextId(isolate)) {}

  virtual int position() const {
    UNREACHABLE();
    return 0;
  }

  virtual Expression* AsExpression()  { return this; }

  virtual bool IsTrivial() { return false; }
  virtual bool IsValidLeftHandSide() { return false; }

  // Helpers for ToBoolean conversion.
  virtual bool ToBooleanIsTrue() { return false; }
  virtual bool ToBooleanIsFalse() { return false; }

  // Symbols that cannot be parsed as array indices are considered property
  // names.  We do not treat symbols that can be array indexes as property
  // names because [] for string objects is handled only by keyed ICs.
  virtual bool IsPropertyName() { return false; }

  // Mark the expression as being compiled as an expression
  // statement. This is used to transform postfix increments to
  // (faster) prefix increments.
  virtual void MarkAsStatement() { /* do nothing */ }

  // True iff the result can be safely overwritten (to avoid allocation).
  // False for operations that can return one of their operands.
  virtual bool ResultOverwriteAllowed() { return false; }

  // True iff the expression is a literal represented as a smi.
  virtual bool IsSmiLiteral() { return false; }

  // Type feedback information for assignments and properties.
  virtual bool IsMonomorphic() {
    UNREACHABLE();
    return false;
  }
  virtual bool IsArrayLength() {
    UNREACHABLE();
    return false;
  }
  virtual SmallMapList* GetReceiverTypes() {
    UNREACHABLE();
    return NULL;
  }
  Handle<Map> GetMonomorphicReceiverType() {
    ASSERT(IsMonomorphic());
    SmallMapList* types = GetReceiverTypes();
    ASSERT(types != NULL && types->length() == 1);
    return types->at(0);
  }

  unsigned id() const { return id_; }
  unsigned test_id() const { return test_id_; }

 private:
  unsigned id_;
  unsigned test_id_;
};


class BreakableStatement: public Statement {
 public:
  enum Type {
    TARGET_FOR_ANONYMOUS,
    TARGET_FOR_NAMED_ONLY
  };

  // The labels associated with this statement. May be NULL;
  // if it is != NULL, guaranteed to contain at least one entry.
  ZoneStringList* labels() const { return labels_; }

  // Type testing & conversion.
  virtual BreakableStatement* AsBreakableStatement() { return this; }

  // Code generation
  Label* break_target() { return &break_target_; }

  // Testers.
  bool is_target_for_anonymous() const { return type_ == TARGET_FOR_ANONYMOUS; }

  // Bailout support.
  int EntryId() const { return entry_id_; }
  int ExitId() const { return exit_id_; }

 protected:
  BreakableStatement(Isolate* isolate, ZoneStringList* labels, Type type);

 private:
  ZoneStringList* labels_;
  Type type_;
  Label break_target_;
  int entry_id_;
  int exit_id_;
};


class Block: public BreakableStatement {
 public:
  inline Block(Isolate* isolate,
               ZoneStringList* labels,
               int capacity,
               bool is_initializer_block);

  DECLARE_NODE_TYPE(Block)

  virtual Assignment* StatementAsSimpleAssignment() {
    if (statements_.length() != 1) return NULL;
    return statements_[0]->StatementAsSimpleAssignment();
  }

  virtual CountOperation* StatementAsCountOperation() {
    if (statements_.length() != 1) return NULL;
    return statements_[0]->StatementAsCountOperation();
  }

  virtual bool IsInlineable() const;

  void AddStatement(Statement* statement) { statements_.Add(statement); }

  ZoneList<Statement*>* statements() { return &statements_; }
  bool is_initializer_block() const { return is_initializer_block_; }

  Scope* block_scope() const { return block_scope_; }
  void set_block_scope(Scope* block_scope) { block_scope_ = block_scope; }

 private:
  ZoneList<Statement*> statements_;
  bool is_initializer_block_;
  Scope* block_scope_;
};


class Declaration: public AstNode {
 public:
  Declaration(VariableProxy* proxy,
              Variable::Mode mode,
              FunctionLiteral* fun,
              Scope* scope)
      : proxy_(proxy),
        mode_(mode),
        fun_(fun),
        scope_(scope) {
    ASSERT(mode == Variable::VAR ||
           mode == Variable::CONST ||
           mode == Variable::LET);
    // At the moment there are no "const functions"'s in JavaScript...
    ASSERT(fun == NULL || mode == Variable::VAR || mode == Variable::LET);
  }

  DECLARE_NODE_TYPE(Declaration)

  VariableProxy* proxy() const { return proxy_; }
  Variable::Mode mode() const { return mode_; }
  FunctionLiteral* fun() const { return fun_; }  // may be NULL
  virtual bool IsInlineable() const;
  Scope* scope() const { return scope_; }

 private:
  VariableProxy* proxy_;
  Variable::Mode mode_;
  FunctionLiteral* fun_;

  // Nested scope from which the declaration originated.
  Scope* scope_;
};


class IterationStatement: public BreakableStatement {
 public:
  // Type testing & conversion.
  virtual IterationStatement* AsIterationStatement() { return this; }

  Statement* body() const { return body_; }

  // Bailout support.
  int OsrEntryId() const { return osr_entry_id_; }
  virtual int ContinueId() const = 0;
  virtual int StackCheckId() const = 0;

  // Code generation
  Label* continue_target()  { return &continue_target_; }

 protected:
  inline IterationStatement(Isolate* isolate, ZoneStringList* labels);

  void Initialize(Statement* body) {
    body_ = body;
  }

 private:
  Statement* body_;
  Label continue_target_;
  int osr_entry_id_;
};


class DoWhileStatement: public IterationStatement {
 public:
  inline DoWhileStatement(Isolate* isolate, ZoneStringList* labels);

  DECLARE_NODE_TYPE(DoWhileStatement)

  void Initialize(Expression* cond, Statement* body) {
    IterationStatement::Initialize(body);
    cond_ = cond;
  }

  Expression* cond() const { return cond_; }

  // Position where condition expression starts. We need it to make
  // the loop's condition a breakable location.
  int condition_position() { return condition_position_; }
  void set_condition_position(int pos) { condition_position_ = pos; }

  // Bailout support.
  virtual int ContinueId() const { return continue_id_; }
  virtual int StackCheckId() const { return back_edge_id_; }
  int BackEdgeId() const { return back_edge_id_; }

  virtual bool IsInlineable() const;

 private:
  Expression* cond_;
  int condition_position_;
  int continue_id_;
  int back_edge_id_;
};


class WhileStatement: public IterationStatement {
 public:
  inline WhileStatement(Isolate* isolate, ZoneStringList* labels);

  DECLARE_NODE_TYPE(WhileStatement)

  void Initialize(Expression* cond, Statement* body) {
    IterationStatement::Initialize(body);
    cond_ = cond;
  }

  Expression* cond() const { return cond_; }
  bool may_have_function_literal() const {
    return may_have_function_literal_;
  }
  void set_may_have_function_literal(bool value) {
    may_have_function_literal_ = value;
  }
  virtual bool IsInlineable() const;

  // Bailout support.
  virtual int ContinueId() const { return EntryId(); }
  virtual int StackCheckId() const { return body_id_; }
  int BodyId() const { return body_id_; }

 private:
  Expression* cond_;
  // True if there is a function literal subexpression in the condition.
  bool may_have_function_literal_;
  int body_id_;
};


class ForStatement: public IterationStatement {
 public:
  inline ForStatement(Isolate* isolate, ZoneStringList* labels);

  DECLARE_NODE_TYPE(ForStatement)

  void Initialize(Statement* init,
                  Expression* cond,
                  Statement* next,
                  Statement* body) {
    IterationStatement::Initialize(body);
    init_ = init;
    cond_ = cond;
    next_ = next;
  }

  Statement* init() const { return init_; }
  Expression* cond() const { return cond_; }
  Statement* next() const { return next_; }

  bool may_have_function_literal() const {
    return may_have_function_literal_;
  }
  void set_may_have_function_literal(bool value) {
    may_have_function_literal_ = value;
  }

  // Bailout support.
  virtual int ContinueId() const { return continue_id_; }
  virtual int StackCheckId() const { return body_id_; }
  int BodyId() const { return body_id_; }

  bool is_fast_smi_loop() { return loop_variable_ != NULL; }
  Variable* loop_variable() { return loop_variable_; }
  void set_loop_variable(Variable* var) { loop_variable_ = var; }
  virtual bool IsInlineable() const;

 private:
  Statement* init_;
  Expression* cond_;
  Statement* next_;
  // True if there is a function literal subexpression in the condition.
  bool may_have_function_literal_;
  Variable* loop_variable_;
  int continue_id_;
  int body_id_;
};


class ForInStatement: public IterationStatement {
 public:
  inline ForInStatement(Isolate* isolate, ZoneStringList* labels);

  DECLARE_NODE_TYPE(ForInStatement)

  void Initialize(Expression* each, Expression* enumerable, Statement* body) {
    IterationStatement::Initialize(body);
    each_ = each;
    enumerable_ = enumerable;
  }

  Expression* each() const { return each_; }
  Expression* enumerable() const { return enumerable_; }
  virtual bool IsInlineable() const;

  // Bailout support.
  int AssignmentId() const { return assignment_id_; }
  virtual int ContinueId() const { return EntryId(); }
  virtual int StackCheckId() const { return EntryId(); }

 private:
  Expression* each_;
  Expression* enumerable_;
  int assignment_id_;
};


class ExpressionStatement: public Statement {
 public:
  explicit ExpressionStatement(Expression* expression)
      : expression_(expression) { }

  DECLARE_NODE_TYPE(ExpressionStatement)

  virtual bool IsInlineable() const;

  virtual Assignment* StatementAsSimpleAssignment();
  virtual CountOperation* StatementAsCountOperation();

  void set_expression(Expression* e) { expression_ = e; }
  Expression* expression() const { return expression_; }

 private:
  Expression* expression_;
};


class ContinueStatement: public Statement {
 public:
  explicit ContinueStatement(IterationStatement* target)
      : target_(target) { }

  DECLARE_NODE_TYPE(ContinueStatement)

  IterationStatement* target() const { return target_; }
  virtual bool IsInlineable() const;

 private:
  IterationStatement* target_;
};


class BreakStatement: public Statement {
 public:
  explicit BreakStatement(BreakableStatement* target)
      : target_(target) { }

  DECLARE_NODE_TYPE(BreakStatement)

  BreakableStatement* target() const { return target_; }
  virtual bool IsInlineable() const;

 private:
  BreakableStatement* target_;
};


class ReturnStatement: public Statement {
 public:
  explicit ReturnStatement(Expression* expression)
      : expression_(expression) { }

  DECLARE_NODE_TYPE(ReturnStatement)

  Expression* expression() const { return expression_; }
  virtual bool IsInlineable() const;

 private:
  Expression* expression_;
};


class WithStatement: public Statement {
 public:
  WithStatement(Expression* expression, Statement* statement)
      : expression_(expression), statement_(statement) { }

  DECLARE_NODE_TYPE(WithStatement)

  Expression* expression() const { return expression_; }
  Statement* statement() const { return statement_; }

  virtual bool IsInlineable() const;

 private:
  Expression* expression_;
  Statement* statement_;
};


class CaseClause: public ZoneObject {
 public:
  CaseClause(Isolate* isolate,
             Expression* label,
             ZoneList<Statement*>* statements,
             int pos);

  bool is_default() const { return label_ == NULL; }
  Expression* label() const {
    CHECK(!is_default());
    return label_;
  }
  Label* body_target() { return &body_target_; }
  ZoneList<Statement*>* statements() const { return statements_; }

  int position() const { return position_; }
  void set_position(int pos) { position_ = pos; }

  int EntryId() { return entry_id_; }
  int CompareId() { return compare_id_; }

  // Type feedback information.
  void RecordTypeFeedback(TypeFeedbackOracle* oracle);
  bool IsSmiCompare() { return compare_type_ == SMI_ONLY; }
  bool IsObjectCompare() { return compare_type_ == OBJECT_ONLY; }

 private:
  Expression* label_;
  Label body_target_;
  ZoneList<Statement*>* statements_;
  int position_;
  enum CompareTypeFeedback { NONE, SMI_ONLY, OBJECT_ONLY };
  CompareTypeFeedback compare_type_;
  int compare_id_;
  int entry_id_;
};


class SwitchStatement: public BreakableStatement {
 public:
  inline SwitchStatement(Isolate* isolate, ZoneStringList* labels);

  DECLARE_NODE_TYPE(SwitchStatement)

  void Initialize(Expression* tag, ZoneList<CaseClause*>* cases) {
    tag_ = tag;
    cases_ = cases;
  }

  Expression* tag() const { return tag_; }
  ZoneList<CaseClause*>* cases() const { return cases_; }
  virtual bool IsInlineable() const;

 private:
  Expression* tag_;
  ZoneList<CaseClause*>* cases_;
};


// If-statements always have non-null references to their then- and
// else-parts. When parsing if-statements with no explicit else-part,
// the parser implicitly creates an empty statement. Use the
// HasThenStatement() and HasElseStatement() functions to check if a
// given if-statement has a then- or an else-part containing code.
class IfStatement: public Statement {
 public:
  IfStatement(Isolate* isolate,
              Expression* condition,
              Statement* then_statement,
              Statement* else_statement)
      : condition_(condition),
        then_statement_(then_statement),
        else_statement_(else_statement),
        if_id_(GetNextId(isolate)),
        then_id_(GetNextId(isolate)),
        else_id_(GetNextId(isolate)) {
  }

  DECLARE_NODE_TYPE(IfStatement)

  virtual bool IsInlineable() const;

  bool HasThenStatement() const { return !then_statement()->IsEmpty(); }
  bool HasElseStatement() const { return !else_statement()->IsEmpty(); }

  Expression* condition() const { return condition_; }
  Statement* then_statement() const { return then_statement_; }
  Statement* else_statement() const { return else_statement_; }

  int IfId() const { return if_id_; }
  int ThenId() const { return then_id_; }
  int ElseId() const { return else_id_; }

 private:
  Expression* condition_;
  Statement* then_statement_;
  Statement* else_statement_;
  int if_id_;
  int then_id_;
  int else_id_;
};


// NOTE: TargetCollectors are represented as nodes to fit in the target
// stack in the compiler; this should probably be reworked.
class TargetCollector: public AstNode {
 public:
  TargetCollector(): targets_(0) { }

  // Adds a jump target to the collector. The collector stores a pointer not
  // a copy of the target to make binding work, so make sure not to pass in
  // references to something on the stack.
  void AddTarget(Label* target);

  // Virtual behaviour. TargetCollectors are never part of the AST.
  virtual void Accept(AstVisitor* v) { UNREACHABLE(); }
  virtual TargetCollector* AsTargetCollector() { return this; }

  ZoneList<Label*>* targets() { return &targets_; }
  virtual bool IsInlineable() const;

 private:
  ZoneList<Label*> targets_;
};


class TryStatement: public Statement {
 public:
  explicit TryStatement(Block* try_block)
      : try_block_(try_block), escaping_targets_(NULL) { }

  void set_escaping_targets(ZoneList<Label*>* targets) {
    escaping_targets_ = targets;
  }

  Block* try_block() const { return try_block_; }
  ZoneList<Label*>* escaping_targets() const { return escaping_targets_; }
  virtual bool IsInlineable() const;

 private:
  Block* try_block_;
  ZoneList<Label*>* escaping_targets_;
};


class TryCatchStatement: public TryStatement {
 public:
  TryCatchStatement(Block* try_block,
                    Scope* scope,
                    Variable* variable,
                    Block* catch_block)
      : TryStatement(try_block),
        scope_(scope),
        variable_(variable),
        catch_block_(catch_block) {
  }

  DECLARE_NODE_TYPE(TryCatchStatement)

  Scope* scope() { return scope_; }
  Variable* variable() { return variable_; }
  Block* catch_block() const { return catch_block_; }
  virtual bool IsInlineable() const;

 private:
  Scope* scope_;
  Variable* variable_;
  Block* catch_block_;
};


class TryFinallyStatement: public TryStatement {
 public:
  TryFinallyStatement(Block* try_block, Block* finally_block)
      : TryStatement(try_block),
        finally_block_(finally_block) { }

  DECLARE_NODE_TYPE(TryFinallyStatement)

  Block* finally_block() const { return finally_block_; }
  virtual bool IsInlineable() const;

 private:
  Block* finally_block_;
};


class DebuggerStatement: public Statement {
 public:
  DECLARE_NODE_TYPE(DebuggerStatement)
  virtual bool IsInlineable() const;
};


class EmptyStatement: public Statement {
 public:
  DECLARE_NODE_TYPE(EmptyStatement)

  virtual bool IsInlineable() const;
};


class Literal: public Expression {
 public:
  Literal(Isolate* isolate, Handle<Object> handle)
      : Expression(isolate), handle_(handle) { }

  DECLARE_NODE_TYPE(Literal)

  virtual bool IsTrivial() { return true; }
  virtual bool IsSmiLiteral() { return handle_->IsSmi(); }

  // Check if this literal is identical to the other literal.
  bool IsIdenticalTo(const Literal* other) const {
    return handle_.is_identical_to(other->handle_);
  }

  virtual bool IsPropertyName() {
    if (handle_->IsSymbol()) {
      uint32_t ignored;
      return !String::cast(*handle_)->AsArrayIndex(&ignored);
    }
    return false;
  }

  Handle<String> AsPropertyName() {
    ASSERT(IsPropertyName());
    return Handle<String>::cast(handle_);
  }

  virtual bool ToBooleanIsTrue() { return handle_->ToBoolean()->IsTrue(); }
  virtual bool ToBooleanIsFalse() { return handle_->ToBoolean()->IsFalse(); }

  // Identity testers.
  bool IsNull() const {
    ASSERT(!handle_.is_null());
    return handle_->IsNull();
  }
  bool IsTrue() const {
    ASSERT(!handle_.is_null());
    return handle_->IsTrue();
  }
  bool IsFalse() const {
    ASSERT(!handle_.is_null());
    return handle_->IsFalse();
  }

  Handle<Object> handle() const { return handle_; }
  virtual bool IsInlineable() const;

 private:
  Handle<Object> handle_;
};


// Base class for literals that needs space in the corresponding JSFunction.
class MaterializedLiteral: public Expression {
 public:
  MaterializedLiteral(Isolate* isolate,
                      int literal_index,
                      bool is_simple,
                      int depth)
      : Expression(isolate),
        literal_index_(literal_index),
        is_simple_(is_simple),
        depth_(depth) {}

  virtual MaterializedLiteral* AsMaterializedLiteral() { return this; }

  int literal_index() { return literal_index_; }

  // A materialized literal is simple if the values consist of only
  // constants and simple object and array literals.
  bool is_simple() const { return is_simple_; }

  int depth() const { return depth_; }
  virtual bool IsInlineable() const;

 private:
  int literal_index_;
  bool is_simple_;
  int depth_;
};


// An object literal has a boilerplate object that is used
// for minimizing the work when constructing it at runtime.
class ObjectLiteral: public MaterializedLiteral {
 public:
  // Property is used for passing information
  // about an object literal's properties from the parser
  // to the code generator.
  class Property: public ZoneObject {
   public:
    enum Kind {
      CONSTANT,              // Property with constant value (compile time).
      COMPUTED,              // Property with computed value (execution time).
      MATERIALIZED_LITERAL,  // Property value is a materialized literal.
      GETTER, SETTER,        // Property is an accessor function.
      PROTOTYPE              // Property is __proto__.
    };

    Property(Literal* key, Expression* value);
    Property(bool is_getter, FunctionLiteral* value);

    Literal* key() { return key_; }
    Expression* value() { return value_; }
    Kind kind() { return kind_; }

    bool IsCompileTimeValue();

    void set_emit_store(bool emit_store);
    bool emit_store();

   private:
    Literal* key_;
    Expression* value_;
    Kind kind_;
    bool emit_store_;
  };

  ObjectLiteral(Isolate* isolate,
                Handle<FixedArray> constant_properties,
                ZoneList<Property*>* properties,
                int literal_index,
                bool is_simple,
                bool fast_elements,
                int depth,
                bool has_function)
      : MaterializedLiteral(isolate, literal_index, is_simple, depth),
        constant_properties_(constant_properties),
        properties_(properties),
        fast_elements_(fast_elements),
        has_function_(has_function) {}

  DECLARE_NODE_TYPE(ObjectLiteral)

  Handle<FixedArray> constant_properties() const {
    return constant_properties_;
  }
  ZoneList<Property*>* properties() const { return properties_; }

  bool fast_elements() const { return fast_elements_; }

  bool has_function() { return has_function_; }

  // Mark all computed expressions that are bound to a key that
  // is shadowed by a later occurrence of the same key. For the
  // marked expressions, no store code is emitted.
  void CalculateEmitStore();

  enum Flags {
    kNoFlags = 0,
    kFastElements = 1,
    kHasFunction = 1 << 1
  };

 private:
  Handle<FixedArray> constant_properties_;
  ZoneList<Property*>* properties_;
  bool fast_elements_;
  bool has_function_;
};


// Node for capturing a regexp literal.
class RegExpLiteral: public MaterializedLiteral {
 public:
  RegExpLiteral(Isolate* isolate,
                Handle<String> pattern,
                Handle<String> flags,
                int literal_index)
      : MaterializedLiteral(isolate, literal_index, false, 1),
        pattern_(pattern),
        flags_(flags) {}

  DECLARE_NODE_TYPE(RegExpLiteral)

  Handle<String> pattern() const { return pattern_; }
  Handle<String> flags() const { return flags_; }

 private:
  Handle<String> pattern_;
  Handle<String> flags_;
};

// An array literal has a literals object that is used
// for minimizing the work when constructing it at runtime.
class ArrayLiteral: public MaterializedLiteral {
 public:
  ArrayLiteral(Isolate* isolate,
               Handle<FixedArray> constant_elements,
               ZoneList<Expression*>* values,
               int literal_index,
               bool is_simple,
               int depth)
      : MaterializedLiteral(isolate, literal_index, is_simple, depth),
        constant_elements_(constant_elements),
        values_(values),
        first_element_id_(ReserveIdRange(isolate, values->length())) {}

  DECLARE_NODE_TYPE(ArrayLiteral)

  Handle<FixedArray> constant_elements() const { return constant_elements_; }
  ZoneList<Expression*>* values() const { return values_; }

  // Return an AST id for an element that is used in simulate instructions.
  int GetIdForElement(int i) { return first_element_id_ + i; }

 private:
  Handle<FixedArray> constant_elements_;
  ZoneList<Expression*>* values_;
  int first_element_id_;
};


class VariableProxy: public Expression {
 public:
  VariableProxy(Isolate* isolate, Variable* var);

  DECLARE_NODE_TYPE(VariableProxy)

  virtual bool IsValidLeftHandSide() {
    return var_ == NULL ? true : var_->IsValidLeftHandSide();
  }

  virtual bool IsTrivial() {
    // Reading from a mutable variable is a side effect, but the
    // variable for 'this' is immutable.
    return is_this_ || is_trivial_;
  }

  virtual bool IsInlineable() const;

  bool IsVariable(Handle<String> n) {
    return !is_this() && name().is_identical_to(n);
  }

  bool IsArguments() { return var_ != NULL && var_->is_arguments(); }

  Handle<String> name() const { return name_; }
  Variable* var() const { return var_; }
  bool is_this() const { return is_this_; }
  bool inside_with() const { return inside_with_; }
  int position() const { return position_; }

  void MarkAsTrivial() { is_trivial_ = true; }

  // Bind this proxy to the variable var.
  void BindTo(Variable* var);

 protected:
  Handle<String> name_;
  Variable* var_;  // resolved variable, or NULL
  bool is_this_;
  bool inside_with_;
  bool is_trivial_;
  int position_;

  VariableProxy(Isolate* isolate,
                Handle<String> name,
                bool is_this,
                bool inside_with,
                int position = RelocInfo::kNoPosition);

  friend class Scope;
};


class Property: public Expression {
 public:
  Property(Isolate* isolate,
           Expression* obj,
           Expression* key,
           int pos)
      : Expression(isolate),
        obj_(obj),
        key_(key),
        pos_(pos),
        is_monomorphic_(false),
        is_array_length_(false),
        is_string_length_(false),
        is_string_access_(false),
        is_function_prototype_(false) { }

  DECLARE_NODE_TYPE(Property)

  virtual bool IsValidLeftHandSide() { return true; }
  virtual bool IsInlineable() const;

  Expression* obj() const { return obj_; }
  Expression* key() const { return key_; }
  virtual int position() const { return pos_; }

  bool IsStringLength() const { return is_string_length_; }
  bool IsStringAccess() const { return is_string_access_; }
  bool IsFunctionPrototype() const { return is_function_prototype_; }

  // Type feedback information.
  void RecordTypeFeedback(TypeFeedbackOracle* oracle);
  virtual bool IsMonomorphic() { return is_monomorphic_; }
  virtual SmallMapList* GetReceiverTypes() { return &receiver_types_; }
  virtual bool IsArrayLength() { return is_array_length_; }

 private:
  Expression* obj_;
  Expression* key_;
  int pos_;

  SmallMapList receiver_types_;
  bool is_monomorphic_ : 1;
  bool is_array_length_ : 1;
  bool is_string_length_ : 1;
  bool is_string_access_ : 1;
  bool is_function_prototype_ : 1;
};


class Call: public Expression {
 public:
  Call(Isolate* isolate,
       Expression* expression,
       ZoneList<Expression*>* arguments,
       int pos)
      : Expression(isolate),
        expression_(expression),
        arguments_(arguments),
        pos_(pos),
        is_monomorphic_(false),
        check_type_(RECEIVER_MAP_CHECK),
        return_id_(GetNextId(isolate)) {
  }

  DECLARE_NODE_TYPE(Call)

  virtual bool IsInlineable() const;

  Expression* expression() const { return expression_; }
  ZoneList<Expression*>* arguments() const { return arguments_; }
  virtual int position() const { return pos_; }

  void RecordTypeFeedback(TypeFeedbackOracle* oracle,
                          CallKind call_kind);
  virtual SmallMapList* GetReceiverTypes() { return &receiver_types_; }
  virtual bool IsMonomorphic() { return is_monomorphic_; }
  CheckType check_type() const { return check_type_; }
  Handle<JSFunction> target() { return target_; }
  Handle<JSObject> holder() { return holder_; }
  Handle<JSGlobalPropertyCell> cell() { return cell_; }

  bool ComputeTarget(Handle<Map> type, Handle<String> name);
  bool ComputeGlobalTarget(Handle<GlobalObject> global, LookupResult* lookup);

  // Bailout support.
  int ReturnId() const { return return_id_; }

#ifdef DEBUG
  // Used to assert that the FullCodeGenerator records the return site.
  bool return_is_recorded_;
#endif

 private:
  Expression* expression_;
  ZoneList<Expression*>* arguments_;
  int pos_;

  bool is_monomorphic_;
  CheckType check_type_;
  SmallMapList receiver_types_;
  Handle<JSFunction> target_;
  Handle<JSObject> holder_;
  Handle<JSGlobalPropertyCell> cell_;

  int return_id_;
};


class CallNew: public Expression {
 public:
  CallNew(Isolate* isolate,
          Expression* expression,
          ZoneList<Expression*>* arguments,
          int pos)
      : Expression(isolate),
        expression_(expression),
        arguments_(arguments),
        pos_(pos) { }

  DECLARE_NODE_TYPE(CallNew)

  virtual bool IsInlineable() const;

  Expression* expression() const { return expression_; }
  ZoneList<Expression*>* arguments() const { return arguments_; }
  virtual int position() const { return pos_; }

 private:
  Expression* expression_;
  ZoneList<Expression*>* arguments_;
  int pos_;
};


// The CallRuntime class does not represent any official JavaScript
// language construct. Instead it is used to call a C or JS function
// with a set of arguments. This is used from the builtins that are
// implemented in JavaScript (see "v8natives.js").
class CallRuntime: public Expression {
 public:
  CallRuntime(Isolate* isolate,
              Handle<String> name,
              const Runtime::Function* function,
              ZoneList<Expression*>* arguments)
      : Expression(isolate),
        name_(name),
        function_(function),
        arguments_(arguments) { }

  DECLARE_NODE_TYPE(CallRuntime)

  virtual bool IsInlineable() const;

  Handle<String> name() const { return name_; }
  const Runtime::Function* function() const { return function_; }
  ZoneList<Expression*>* arguments() const { return arguments_; }
  bool is_jsruntime() const { return function_ == NULL; }

 private:
  Handle<String> name_;
  const Runtime::Function* function_;
  ZoneList<Expression*>* arguments_;
};


class UnaryOperation: public Expression {
 public:
  UnaryOperation(Isolate* isolate,
                 Token::Value op,
                 Expression* expression,
                 int pos)
      : Expression(isolate), op_(op), expression_(expression), pos_(pos) {
    ASSERT(Token::IsUnaryOp(op));
  }

  DECLARE_NODE_TYPE(UnaryOperation)

  virtual bool IsInlineable() const;

  virtual bool ResultOverwriteAllowed();

  Token::Value op() const { return op_; }
  Expression* expression() const { return expression_; }
  virtual int position() const { return pos_; }

 private:
  Token::Value op_;
  Expression* expression_;
  int pos_;
};


class BinaryOperation: public Expression {
 public:
  BinaryOperation(Isolate* isolate,
                  Token::Value op,
                  Expression* left,
                  Expression* right,
                  int pos)
      : Expression(isolate), op_(op), left_(left), right_(right), pos_(pos) {
    ASSERT(Token::IsBinaryOp(op));
    right_id_ = (op == Token::AND || op == Token::OR)
        ? static_cast<int>(GetNextId(isolate))
        : AstNode::kNoNumber;
  }

  DECLARE_NODE_TYPE(BinaryOperation)

  virtual bool IsInlineable() const;

  virtual bool ResultOverwriteAllowed();

  Token::Value op() const { return op_; }
  Expression* left() const { return left_; }
  Expression* right() const { return right_; }
  virtual int position() const { return pos_; }

  // Bailout support.
  int RightId() const { return right_id_; }

 private:
  Token::Value op_;
  Expression* left_;
  Expression* right_;
  int pos_;
  // The short-circuit logical operations have an AST ID for their
  // right-hand subexpression.
  int right_id_;
};


class CountOperation: public Expression {
 public:
  CountOperation(Isolate* isolate,
                 Token::Value op,
                 bool is_prefix,
                 Expression* expr,
                 int pos)
      : Expression(isolate),
        op_(op),
        is_prefix_(is_prefix),
        expression_(expr),
        pos_(pos),
        assignment_id_(GetNextId(isolate)),
        count_id_(GetNextId(isolate)) {}

  DECLARE_NODE_TYPE(CountOperation)

  bool is_prefix() const { return is_prefix_; }
  bool is_postfix() const { return !is_prefix_; }

  Token::Value op() const { return op_; }
  Token::Value binary_op() {
    return (op() == Token::INC) ? Token::ADD : Token::SUB;
  }

  Expression* expression() const { return expression_; }
  virtual int position() const { return pos_; }

  virtual void MarkAsStatement() { is_prefix_ = true; }

  virtual bool IsInlineable() const;

  void RecordTypeFeedback(TypeFeedbackOracle* oracle);
  virtual bool IsMonomorphic() { return is_monomorphic_; }
  virtual SmallMapList* GetReceiverTypes() { return &receiver_types_; }

  // Bailout support.
  int AssignmentId() const { return assignment_id_; }
  int CountId() const { return count_id_; }

 private:
  Token::Value op_;
  bool is_prefix_;
  bool is_monomorphic_;
  Expression* expression_;
  int pos_;
  int assignment_id_;
  int count_id_;
  SmallMapList receiver_types_;
};


class CompareOperation: public Expression {
 public:
  CompareOperation(Isolate* isolate,
                   Token::Value op,
                   Expression* left,
                   Expression* right,
                   int pos)
      : Expression(isolate),
        op_(op),
        left_(left),
        right_(right),
        pos_(pos),
        compare_type_(NONE) {
    ASSERT(Token::IsCompareOp(op));
  }

  DECLARE_NODE_TYPE(CompareOperation)

  Token::Value op() const { return op_; }
  Expression* left() const { return left_; }
  Expression* right() const { return right_; }
  virtual int position() const { return pos_; }

  virtual bool IsInlineable() const;

  // Type feedback information.
  void RecordTypeFeedback(TypeFeedbackOracle* oracle);
  bool IsSmiCompare() { return compare_type_ == SMI_ONLY; }
  bool IsObjectCompare() { return compare_type_ == OBJECT_ONLY; }

  // Match special cases.
  bool IsLiteralCompareTypeof(Expression** expr, Handle<String>* check);
  bool IsLiteralCompareUndefined(Expression** expr);

 private:
  Token::Value op_;
  Expression* left_;
  Expression* right_;
  int pos_;

  enum CompareTypeFeedback { NONE, SMI_ONLY, OBJECT_ONLY };
  CompareTypeFeedback compare_type_;
};


class CompareToNull: public Expression {
 public:
  CompareToNull(Isolate* isolate, bool is_strict, Expression* expression)
      : Expression(isolate), is_strict_(is_strict), expression_(expression) { }

  DECLARE_NODE_TYPE(CompareToNull)

  virtual bool IsInlineable() const;

  bool is_strict() const { return is_strict_; }
  Token::Value op() const { return is_strict_ ? Token::EQ_STRICT : Token::EQ; }
  Expression* expression() const { return expression_; }

 private:
  bool is_strict_;
  Expression* expression_;
};


class Conditional: public Expression {
 public:
  Conditional(Isolate* isolate,
              Expression* condition,
              Expression* then_expression,
              Expression* else_expression,
              int then_expression_position,
              int else_expression_position)
      : Expression(isolate),
        condition_(condition),
        then_expression_(then_expression),
        else_expression_(else_expression),
        then_expression_position_(then_expression_position),
        else_expression_position_(else_expression_position),
        then_id_(GetNextId(isolate)),
        else_id_(GetNextId(isolate)) {
  }

  DECLARE_NODE_TYPE(Conditional)

  virtual bool IsInlineable() const;

  Expression* condition() const { return condition_; }
  Expression* then_expression() const { return then_expression_; }
  Expression* else_expression() const { return else_expression_; }

  int then_expression_position() const { return then_expression_position_; }
  int else_expression_position() const { return else_expression_position_; }

  int ThenId() const { return then_id_; }
  int ElseId() const { return else_id_; }

 private:
  Expression* condition_;
  Expression* then_expression_;
  Expression* else_expression_;
  int then_expression_position_;
  int else_expression_position_;
  int then_id_;
  int else_id_;
};


class Assignment: public Expression {
 public:
  Assignment(Isolate* isolate,
             Token::Value op,
             Expression* target,
             Expression* value,
             int pos);

  DECLARE_NODE_TYPE(Assignment)

  virtual bool IsInlineable() const;

  Assignment* AsSimpleAssignment() { return !is_compound() ? this : NULL; }

  Token::Value binary_op() const;

  Token::Value op() const { return op_; }
  Expression* target() const { return target_; }
  Expression* value() const { return value_; }
  virtual int position() const { return pos_; }
  BinaryOperation* binary_operation() const { return binary_operation_; }

  // This check relies on the definition order of token in token.h.
  bool is_compound() const { return op() > Token::ASSIGN; }

  // An initialization block is a series of statments of the form
  // x.y.z.a = ...; x.y.z.b = ...; etc. The parser marks the beginning and
  // ending of these blocks to allow for optimizations of initialization
  // blocks.
  bool starts_initialization_block() { return block_start_; }
  bool ends_initialization_block() { return block_end_; }
  void mark_block_start() { block_start_ = true; }
  void mark_block_end() { block_end_ = true; }

  // Type feedback information.
  void RecordTypeFeedback(TypeFeedbackOracle* oracle);
  virtual bool IsMonomorphic() { return is_monomorphic_; }
  virtual SmallMapList* GetReceiverTypes() { return &receiver_types_; }

  // Bailout support.
  int CompoundLoadId() const { return compound_load_id_; }
  int AssignmentId() const { return assignment_id_; }

 private:
  Token::Value op_;
  Expression* target_;
  Expression* value_;
  int pos_;
  BinaryOperation* binary_operation_;
  int compound_load_id_;
  int assignment_id_;

  bool block_start_;
  bool block_end_;

  bool is_monomorphic_;
  SmallMapList receiver_types_;
};


class Throw: public Expression {
 public:
  Throw(Isolate* isolate, Expression* exception, int pos)
      : Expression(isolate), exception_(exception), pos_(pos) {}

  DECLARE_NODE_TYPE(Throw)

  Expression* exception() const { return exception_; }
  virtual int position() const { return pos_; }
  virtual bool IsInlineable() const;

 private:
  Expression* exception_;
  int pos_;
};


class FunctionLiteral: public Expression {
 public:
  enum Type {
    ANONYMOUS_EXPRESSION,
    NAMED_EXPRESSION,
    DECLARATION
  };

  FunctionLiteral(Isolate* isolate,
                  Handle<String> name,
                  Scope* scope,
                  ZoneList<Statement*>* body,
                  int materialized_literal_count,
                  int expected_property_count,
                  bool has_only_simple_this_property_assignments,
                  Handle<FixedArray> this_property_assignments,
                  int num_parameters,
                  int start_position,
                  int end_position,
                  Type type,
                  bool has_duplicate_parameters)
      : Expression(isolate),
        name_(name),
        scope_(scope),
        body_(body),
        materialized_literal_count_(materialized_literal_count),
        expected_property_count_(expected_property_count),
        has_only_simple_this_property_assignments_(
            has_only_simple_this_property_assignments),
        this_property_assignments_(this_property_assignments),
        num_parameters_(num_parameters),
        start_position_(start_position),
        end_position_(end_position),
        function_token_position_(RelocInfo::kNoPosition),
        inferred_name_(HEAP->empty_string()),
        is_expression_(type != DECLARATION),
        is_anonymous_(type == ANONYMOUS_EXPRESSION),
        pretenure_(false),
        has_duplicate_parameters_(has_duplicate_parameters) {
  }

  DECLARE_NODE_TYPE(FunctionLiteral)

  Handle<String> name() const { return name_; }
  Scope* scope() const { return scope_; }
  ZoneList<Statement*>* body() const { return body_; }
  void set_function_token_position(int pos) { function_token_position_ = pos; }
  int function_token_position() const { return function_token_position_; }
  int start_position() const { return start_position_; }
  int end_position() const { return end_position_; }
  bool is_expression() const { return is_expression_; }
  bool is_anonymous() const { return is_anonymous_; }
  bool strict_mode() const;

  int materialized_literal_count() { return materialized_literal_count_; }
  int expected_property_count() { return expected_property_count_; }
  bool has_only_simple_this_property_assignments() {
      return has_only_simple_this_property_assignments_;
  }
  Handle<FixedArray> this_property_assignments() {
      return this_property_assignments_;
  }
  int num_parameters() { return num_parameters_; }

  bool AllowsLazyCompilation();

  Handle<String> debug_name() const {
    if (name_->length() > 0) return name_;
    return inferred_name();
  }

  Handle<String> inferred_name() const { return inferred_name_; }
  void set_inferred_name(Handle<String> inferred_name) {
    inferred_name_ = inferred_name;
  }

  bool pretenure() { return pretenure_; }
  void set_pretenure(bool value) { pretenure_ = value; }
  virtual bool IsInlineable() const;

  bool has_duplicate_parameters() { return has_duplicate_parameters_; }

 private:
  Handle<String> name_;
  Scope* scope_;
  ZoneList<Statement*>* body_;
  int materialized_literal_count_;
  int expected_property_count_;
  bool has_only_simple_this_property_assignments_;
  Handle<FixedArray> this_property_assignments_;
  int num_parameters_;
  int start_position_;
  int end_position_;
  int function_token_position_;
  Handle<String> inferred_name_;
  bool is_expression_;
  bool is_anonymous_;
  bool pretenure_;
  bool has_duplicate_parameters_;
};


class SharedFunctionInfoLiteral: public Expression {
 public:
  SharedFunctionInfoLiteral(
      Isolate* isolate,
      Handle<SharedFunctionInfo> shared_function_info)
      : Expression(isolate), shared_function_info_(shared_function_info) { }

  DECLARE_NODE_TYPE(SharedFunctionInfoLiteral)

  Handle<SharedFunctionInfo> shared_function_info() const {
    return shared_function_info_;
  }
  virtual bool IsInlineable() const;

 private:
  Handle<SharedFunctionInfo> shared_function_info_;
};


class ThisFunction: public Expression {
 public:
  explicit ThisFunction(Isolate* isolate) : Expression(isolate) {}
  DECLARE_NODE_TYPE(ThisFunction)
  virtual bool IsInlineable() const;
};


// ----------------------------------------------------------------------------
// Regular expressions


class RegExpVisitor BASE_EMBEDDED {
 public:
  virtual ~RegExpVisitor() { }
#define MAKE_CASE(Name)                                              \
  virtual void* Visit##Name(RegExp##Name*, void* data) = 0;
  FOR_EACH_REG_EXP_TREE_TYPE(MAKE_CASE)
#undef MAKE_CASE
};


class RegExpTree: public ZoneObject {
 public:
  static const int kInfinity = kMaxInt;
  virtual ~RegExpTree() { }
  virtual void* Accept(RegExpVisitor* visitor, void* data) = 0;
  virtual RegExpNode* ToNode(RegExpCompiler* compiler,
                             RegExpNode* on_success) = 0;
  virtual bool IsTextElement() { return false; }
  virtual bool IsAnchoredAtStart() { return false; }
  virtual bool IsAnchoredAtEnd() { return false; }
  virtual int min_match() = 0;
  virtual int max_match() = 0;
  // Returns the interval of registers used for captures within this
  // expression.
  virtual Interval CaptureRegisters() { return Interval::Empty(); }
  virtual void AppendToText(RegExpText* text);
  SmartArrayPointer<const char> ToString();
#define MAKE_ASTYPE(Name)                                                  \
  virtual RegExp##Name* As##Name();                                        \
  virtual bool Is##Name();
  FOR_EACH_REG_EXP_TREE_TYPE(MAKE_ASTYPE)
#undef MAKE_ASTYPE
};


class RegExpDisjunction: public RegExpTree {
 public:
  explicit RegExpDisjunction(ZoneList<RegExpTree*>* alternatives);
  virtual void* Accept(RegExpVisitor* visitor, void* data);
  virtual RegExpNode* ToNode(RegExpCompiler* compiler,
                             RegExpNode* on_success);
  virtual RegExpDisjunction* AsDisjunction();
  virtual Interval CaptureRegisters();
  virtual bool IsDisjunction();
  virtual bool IsAnchoredAtStart();
  virtual bool IsAnchoredAtEnd();
  virtual int min_match() { return min_match_; }
  virtual int max_match() { return max_match_; }
  ZoneList<RegExpTree*>* alternatives() { return alternatives_; }
 private:
  ZoneList<RegExpTree*>* alternatives_;
  int min_match_;
  int max_match_;
};


class RegExpAlternative: public RegExpTree {
 public:
  explicit RegExpAlternative(ZoneList<RegExpTree*>* nodes);
  virtual void* Accept(RegExpVisitor* visitor, void* data);
  virtual RegExpNode* ToNode(RegExpCompiler* compiler,
                             RegExpNode* on_success);
  virtual RegExpAlternative* AsAlternative();
  virtual Interval CaptureRegisters();
  virtual bool IsAlternative();
  virtual bool IsAnchoredAtStart();
  virtual bool IsAnchoredAtEnd();
  virtual int min_match() { return min_match_; }
  virtual int max_match() { return max_match_; }
  ZoneList<RegExpTree*>* nodes() { return nodes_; }
 private:
  ZoneList<RegExpTree*>* nodes_;
  int min_match_;
  int max_match_;
};


class RegExpAssertion: public RegExpTree {
 public:
  enum Type {
    START_OF_LINE,
    START_OF_INPUT,
    END_OF_LINE,
    END_OF_INPUT,
    BOUNDARY,
    NON_BOUNDARY
  };
  explicit RegExpAssertion(Type type) : type_(type) { }
  virtual void* Accept(RegExpVisitor* visitor, void* data);
  virtual RegExpNode* ToNode(RegExpCompiler* compiler,
                             RegExpNode* on_success);
  virtual RegExpAssertion* AsAssertion();
  virtual bool IsAssertion();
  virtual bool IsAnchoredAtStart();
  virtual bool IsAnchoredAtEnd();
  virtual int min_match() { return 0; }
  virtual int max_match() { return 0; }
  Type type() { return type_; }
 private:
  Type type_;
};


class CharacterSet BASE_EMBEDDED {
 public:
  explicit CharacterSet(uc16 standard_set_type)
      : ranges_(NULL),
        standard_set_type_(standard_set_type) {}
  explicit CharacterSet(ZoneList<CharacterRange>* ranges)
      : ranges_(ranges),
        standard_set_type_(0) {}
  ZoneList<CharacterRange>* ranges();
  uc16 standard_set_type() { return standard_set_type_; }
  void set_standard_set_type(uc16 special_set_type) {
    standard_set_type_ = special_set_type;
  }
  bool is_standard() { return standard_set_type_ != 0; }
  void Canonicalize();
 private:
  ZoneList<CharacterRange>* ranges_;
  // If non-zero, the value represents a standard set (e.g., all whitespace
  // characters) without having to expand the ranges.
  uc16 standard_set_type_;
};


class RegExpCharacterClass: public RegExpTree {
 public:
  RegExpCharacterClass(ZoneList<CharacterRange>* ranges, bool is_negated)
      : set_(ranges),
        is_negated_(is_negated) { }
  explicit RegExpCharacterClass(uc16 type)
      : set_(type),
        is_negated_(false) { }
  virtual void* Accept(RegExpVisitor* visitor, void* data);
  virtual RegExpNode* ToNode(RegExpCompiler* compiler,
                             RegExpNode* on_success);
  virtual RegExpCharacterClass* AsCharacterClass();
  virtual bool IsCharacterClass();
  virtual bool IsTextElement() { return true; }
  virtual int min_match() { return 1; }
  virtual int max_match() { return 1; }
  virtual void AppendToText(RegExpText* text);
  CharacterSet character_set() { return set_; }
  // TODO(lrn): Remove need for complex version if is_standard that
  // recognizes a mangled standard set and just do { return set_.is_special(); }
  bool is_standard();
  // Returns a value representing the standard character set if is_standard()
  // returns true.
  // Currently used values are:
  // s : unicode whitespace
  // S : unicode non-whitespace
  // w : ASCII word character (digit, letter, underscore)
  // W : non-ASCII word character
  // d : ASCII digit
  // D : non-ASCII digit
  // . : non-unicode non-newline
  // * : All characters
  uc16 standard_type() { return set_.standard_set_type(); }
  ZoneList<CharacterRange>* ranges() { return set_.ranges(); }
  bool is_negated() { return is_negated_; }

 private:
  CharacterSet set_;
  bool is_negated_;
};


class RegExpAtom: public RegExpTree {
 public:
  explicit RegExpAtom(Vector<const uc16> data) : data_(data) { }
  virtual void* Accept(RegExpVisitor* visitor, void* data);
  virtual RegExpNode* ToNode(RegExpCompiler* compiler,
                             RegExpNode* on_success);
  virtual RegExpAtom* AsAtom();
  virtual bool IsAtom();
  virtual bool IsTextElement() { return true; }
  virtual int min_match() { return data_.length(); }
  virtual int max_match() { return data_.length(); }
  virtual void AppendToText(RegExpText* text);
  Vector<const uc16> data() { return data_; }
  int length() { return data_.length(); }
 private:
  Vector<const uc16> data_;
};


class RegExpText: public RegExpTree {
 public:
  RegExpText() : elements_(2), length_(0) {}
  virtual void* Accept(RegExpVisitor* visitor, void* data);
  virtual RegExpNode* ToNode(RegExpCompiler* compiler,
                             RegExpNode* on_success);
  virtual RegExpText* AsText();
  virtual bool IsText();
  virtual bool IsTextElement() { return true; }
  virtual int min_match() { return length_; }
  virtual int max_match() { return length_; }
  virtual void AppendToText(RegExpText* text);
  void AddElement(TextElement elm)  {
    elements_.Add(elm);
    length_ += elm.length();
  }
  ZoneList<TextElement>* elements() { return &elements_; }
 private:
  ZoneList<TextElement> elements_;
  int length_;
};


class RegExpQuantifier: public RegExpTree {
 public:
  enum Type { GREEDY, NON_GREEDY, POSSESSIVE };
  RegExpQuantifier(int min, int max, Type type, RegExpTree* body)
      : body_(body),
        min_(min),
        max_(max),
        min_match_(min * body->min_match()),
        type_(type) {
    if (max > 0 && body->max_match() > kInfinity / max) {
      max_match_ = kInfinity;
    } else {
      max_match_ = max * body->max_match();
    }
  }
  virtual void* Accept(RegExpVisitor* visitor, void* data);
  virtual RegExpNode* ToNode(RegExpCompiler* compiler,
                             RegExpNode* on_success);
  static RegExpNode* ToNode(int min,
                            int max,
                            bool is_greedy,
                            RegExpTree* body,
                            RegExpCompiler* compiler,
                            RegExpNode* on_success,
                            bool not_at_start = false);
  virtual RegExpQuantifier* AsQuantifier();
  virtual Interval CaptureRegisters();
  virtual bool IsQuantifier();
  virtual int min_match() { return min_match_; }
  virtual int max_match() { return max_match_; }
  int min() { return min_; }
  int max() { return max_; }
  bool is_possessive() { return type_ == POSSESSIVE; }
  bool is_non_greedy() { return type_ == NON_GREEDY; }
  bool is_greedy() { return type_ == GREEDY; }
  RegExpTree* body() { return body_; }

 private:
  RegExpTree* body_;
  int min_;
  int max_;
  int min_match_;
  int max_match_;
  Type type_;
};


class RegExpCapture: public RegExpTree {
 public:
  explicit RegExpCapture(RegExpTree* body, int index)
      : body_(body), index_(index) { }
  virtual void* Accept(RegExpVisitor* visitor, void* data);
  virtual RegExpNode* ToNode(RegExpCompiler* compiler,
                             RegExpNode* on_success);
  static RegExpNode* ToNode(RegExpTree* body,
                            int index,
                            RegExpCompiler* compiler,
                            RegExpNode* on_success);
  virtual RegExpCapture* AsCapture();
  virtual bool IsAnchoredAtStart();
  virtual bool IsAnchoredAtEnd();
  virtual Interval CaptureRegisters();
  virtual bool IsCapture();
  virtual int min_match() { return body_->min_match(); }
  virtual int max_match() { return body_->max_match(); }
  RegExpTree* body() { return body_; }
  int index() { return index_; }
  static int StartRegister(int index) { return index * 2; }
  static int EndRegister(int index) { return index * 2 + 1; }

 private:
  RegExpTree* body_;
  int index_;
};


class RegExpLookahead: public RegExpTree {
 public:
  RegExpLookahead(RegExpTree* body,
                  bool is_positive,
                  int capture_count,
                  int capture_from)
      : body_(body),
        is_positive_(is_positive),
        capture_count_(capture_count),
        capture_from_(capture_from) { }

  virtual void* Accept(RegExpVisitor* visitor, void* data);
  virtual RegExpNode* ToNode(RegExpCompiler* compiler,
                             RegExpNode* on_success);
  virtual RegExpLookahead* AsLookahead();
  virtual Interval CaptureRegisters();
  virtual bool IsLookahead();
  virtual bool IsAnchoredAtStart();
  virtual int min_match() { return 0; }
  virtual int max_match() { return 0; }
  RegExpTree* body() { return body_; }
  bool is_positive() { return is_positive_; }
  int capture_count() { return capture_count_; }
  int capture_from() { return capture_from_; }

 private:
  RegExpTree* body_;
  bool is_positive_;
  int capture_count_;
  int capture_from_;
};


class RegExpBackReference: public RegExpTree {
 public:
  explicit RegExpBackReference(RegExpCapture* capture)
      : capture_(capture) { }
  virtual void* Accept(RegExpVisitor* visitor, void* data);
  virtual RegExpNode* ToNode(RegExpCompiler* compiler,
                             RegExpNode* on_success);
  virtual RegExpBackReference* AsBackReference();
  virtual bool IsBackReference();
  virtual int min_match() { return 0; }
  virtual int max_match() { return capture_->max_match(); }
  int index() { return capture_->index(); }
  RegExpCapture* capture() { return capture_; }
 private:
  RegExpCapture* capture_;
};


class RegExpEmpty: public RegExpTree {
 public:
  RegExpEmpty() { }
  virtual void* Accept(RegExpVisitor* visitor, void* data);
  virtual RegExpNode* ToNode(RegExpCompiler* compiler,
                             RegExpNode* on_success);
  virtual RegExpEmpty* AsEmpty();
  virtual bool IsEmpty();
  virtual int min_match() { return 0; }
  virtual int max_match() { return 0; }
  static RegExpEmpty* GetInstance() { return &kInstance; }
 private:
  static RegExpEmpty kInstance;
};


// ----------------------------------------------------------------------------
// Basic visitor
// - leaf node visitors are abstract.

class AstVisitor BASE_EMBEDDED {
 public:
  AstVisitor() : isolate_(Isolate::Current()), stack_overflow_(false) { }
  virtual ~AstVisitor() { }

  // Stack overflow check and dynamic dispatch.
  void Visit(AstNode* node) { if (!CheckStackOverflow()) node->Accept(this); }

  // Iteration left-to-right.
  virtual void VisitDeclarations(ZoneList<Declaration*>* declarations);
  virtual void VisitStatements(ZoneList<Statement*>* statements);
  virtual void VisitExpressions(ZoneList<Expression*>* expressions);

  // Stack overflow tracking support.
  bool HasStackOverflow() const { return stack_overflow_; }
  bool CheckStackOverflow();

  // If a stack-overflow exception is encountered when visiting a
  // node, calling SetStackOverflow will make sure that the visitor
  // bails out without visiting more nodes.
  void SetStackOverflow() { stack_overflow_ = true; }
  void ClearStackOverflow() { stack_overflow_ = false; }

  // Individual AST nodes.
#define DEF_VISIT(type)                         \
  virtual void Visit##type(type* node) = 0;
  AST_NODE_LIST(DEF_VISIT)
#undef DEF_VISIT

 protected:
  Isolate* isolate() { return isolate_; }

 private:
  Isolate* isolate_;
  bool stack_overflow_;
};


} }  // namespace v8::internal

#endif  // V8_AST_H_
