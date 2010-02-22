// Copyright 2006-2008 the V8 project authors. All rights reserved.
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

#include "execution.h"
#include "factory.h"
#include "jsregexp.h"
#include "jump-target.h"
#include "runtime.h"
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
  V(WithEnterStatement)                         \
  V(WithExitStatement)                          \
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
  V(FunctionBoilerplateLiteral)                 \
  V(Conditional)                                \
  V(Slot)                                       \
  V(VariableProxy)                              \
  V(Literal)                                    \
  V(RegExpLiteral)                              \
  V(ObjectLiteral)                              \
  V(ArrayLiteral)                               \
  V(CatchExtensionObject)                       \
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
  V(ThisFunction)

#define AST_NODE_LIST(V)                        \
  V(Declaration)                                \
  STATEMENT_NODE_LIST(V)                        \
  EXPRESSION_NODE_LIST(V)

// Forward declarations
class TargetCollector;
class MaterializedLiteral;
class DefinitionInfo;

#define DEF_FORWARD_DECLARATION(type) class type;
AST_NODE_LIST(DEF_FORWARD_DECLARATION)
#undef DEF_FORWARD_DECLARATION


// Typedef only introduced to avoid unreadable code.
// Please do appreciate the required space in "> >".
typedef ZoneList<Handle<String> > ZoneStringList;
typedef ZoneList<Handle<Object> > ZoneObjectList;


class AstNode: public ZoneObject {
 public:
  virtual ~AstNode() { }
  virtual void Accept(AstVisitor* v) = 0;

  // Type testing & conversion.
  virtual Statement* AsStatement() { return NULL; }
  virtual ExpressionStatement* AsExpressionStatement() { return NULL; }
  virtual EmptyStatement* AsEmptyStatement() { return NULL; }
  virtual Expression* AsExpression() { return NULL; }
  virtual Literal* AsLiteral() { return NULL; }
  virtual Slot* AsSlot() { return NULL; }
  virtual VariableProxy* AsVariableProxy() { return NULL; }
  virtual Property* AsProperty() { return NULL; }
  virtual Call* AsCall() { return NULL; }
  virtual TargetCollector* AsTargetCollector() { return NULL; }
  virtual BreakableStatement* AsBreakableStatement() { return NULL; }
  virtual IterationStatement* AsIterationStatement() { return NULL; }
  virtual UnaryOperation* AsUnaryOperation() { return NULL; }
  virtual BinaryOperation* AsBinaryOperation() { return NULL; }
  virtual Assignment* AsAssignment() { return NULL; }
  virtual FunctionLiteral* AsFunctionLiteral() { return NULL; }
  virtual MaterializedLiteral* AsMaterializedLiteral() { return NULL; }
  virtual ObjectLiteral* AsObjectLiteral() { return NULL; }
  virtual ArrayLiteral* AsArrayLiteral() { return NULL; }
  virtual CompareOperation* AsCompareOperation() { return NULL; }
};


class Statement: public AstNode {
 public:
  Statement() : statement_pos_(RelocInfo::kNoPosition) {}

  virtual Statement* AsStatement()  { return this; }
  virtual ReturnStatement* AsReturnStatement() { return NULL; }

  bool IsEmpty() { return AsEmptyStatement() != NULL; }

  void set_statement_pos(int statement_pos) { statement_pos_ = statement_pos; }
  int statement_pos() const { return statement_pos_; }

 private:
  int statement_pos_;
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
    kTest,
    // Evaluated for control flow and side effects.  Value is also
    // needed if true.
    kValueTest,
    // Evaluated for control flow and side effects.  Value is also
    // needed if false.
    kTestValue
  };

  static const int kNoLabel = -1;

  Expression() : num_(kNoLabel), def_(NULL), defined_vars_(NULL) {}

  virtual Expression* AsExpression()  { return this; }

  virtual bool IsValidLeftHandSide() { return false; }

  // Symbols that cannot be parsed as array indices are considered property
  // names.  We do not treat symbols that can be array indexes as property
  // names because [] for string objects is handled only by keyed ICs.
  virtual bool IsPropertyName() { return false; }

  // True if the expression does not have (evaluated) subexpressions.
  // Function literals are leaves because their subexpressions are not
  // evaluated.
  virtual bool IsLeaf() { return false; }

  // Mark the expression as being compiled as an expression
  // statement. This is used to transform postfix increments to
  // (faster) prefix increments.
  virtual void MarkAsStatement() { /* do nothing */ }

  // Static type information for this expression.
  StaticType* type() { return &type_; }

  int num() { return num_; }

  // AST node numbering ordered by evaluation order.
  void set_num(int n) { num_ = n; }

  // Data flow information.
  DefinitionInfo* var_def() { return def_; }
  void set_var_def(DefinitionInfo* def) { def_ = def; }

  ZoneList<DefinitionInfo*>* defined_vars() { return defined_vars_; }
  void set_defined_vars(ZoneList<DefinitionInfo*>* defined_vars) {
    defined_vars_ = defined_vars;
  }

 private:
  StaticType type_;
  int num_;
  DefinitionInfo* def_;
  ZoneList<DefinitionInfo*>* defined_vars_;
};


/**
 * A sentinel used during pre parsing that represents some expression
 * that is a valid left hand side without having to actually build
 * the expression.
 */
class ValidLeftHandSideSentinel: public Expression {
 public:
  virtual bool IsValidLeftHandSide() { return true; }
  virtual void Accept(AstVisitor* v) { UNREACHABLE(); }
  static ValidLeftHandSideSentinel* instance() { return &instance_; }
 private:
  static ValidLeftHandSideSentinel instance_;
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
  BreakTarget* break_target() { return &break_target_; }

  // Testers.
  bool is_target_for_anonymous() const { return type_ == TARGET_FOR_ANONYMOUS; }

 protected:
  BreakableStatement(ZoneStringList* labels, Type type)
      : labels_(labels), type_(type) {
    ASSERT(labels == NULL || labels->length() > 0);
  }

 private:
  ZoneStringList* labels_;
  Type type_;
  BreakTarget break_target_;
};


class Block: public BreakableStatement {
 public:
  Block(ZoneStringList* labels, int capacity, bool is_initializer_block)
      : BreakableStatement(labels, TARGET_FOR_NAMED_ONLY),
        statements_(capacity),
        is_initializer_block_(is_initializer_block) { }

  virtual void Accept(AstVisitor* v);

  void AddStatement(Statement* statement) { statements_.Add(statement); }

  ZoneList<Statement*>* statements() { return &statements_; }
  bool is_initializer_block() const  { return is_initializer_block_; }

 private:
  ZoneList<Statement*> statements_;
  bool is_initializer_block_;
};


class Declaration: public AstNode {
 public:
  Declaration(VariableProxy* proxy, Variable::Mode mode, FunctionLiteral* fun)
      : proxy_(proxy),
        mode_(mode),
        fun_(fun) {
    ASSERT(mode == Variable::VAR || mode == Variable::CONST);
    // At the moment there are no "const functions"'s in JavaScript...
    ASSERT(fun == NULL || mode == Variable::VAR);
  }

  virtual void Accept(AstVisitor* v);

  VariableProxy* proxy() const  { return proxy_; }
  Variable::Mode mode() const  { return mode_; }
  FunctionLiteral* fun() const  { return fun_; }  // may be NULL

 private:
  VariableProxy* proxy_;
  Variable::Mode mode_;
  FunctionLiteral* fun_;
};


class IterationStatement: public BreakableStatement {
 public:
  // Type testing & conversion.
  virtual IterationStatement* AsIterationStatement() { return this; }

  Statement* body() const { return body_; }

  // Code generation
  BreakTarget* continue_target()  { return &continue_target_; }

 protected:
  explicit IterationStatement(ZoneStringList* labels)
      : BreakableStatement(labels, TARGET_FOR_ANONYMOUS), body_(NULL) { }

  void Initialize(Statement* body) {
    body_ = body;
  }

 private:
  Statement* body_;
  BreakTarget continue_target_;
};


class DoWhileStatement: public IterationStatement {
 public:
  explicit DoWhileStatement(ZoneStringList* labels)
      : IterationStatement(labels), cond_(NULL), condition_position_(-1) {
  }

  void Initialize(Expression* cond, Statement* body) {
    IterationStatement::Initialize(body);
    cond_ = cond;
  }

  virtual void Accept(AstVisitor* v);

  Expression* cond() const { return cond_; }

  // Position where condition expression starts. We need it to make
  // the loop's condition a breakable location.
  int condition_position() { return condition_position_; }
  void set_condition_position(int pos) { condition_position_ = pos; }

 private:
  Expression* cond_;
  int condition_position_;
};


class WhileStatement: public IterationStatement {
 public:
  explicit WhileStatement(ZoneStringList* labels)
      : IterationStatement(labels),
        cond_(NULL),
        may_have_function_literal_(true) {
  }

  void Initialize(Expression* cond, Statement* body) {
    IterationStatement::Initialize(body);
    cond_ = cond;
  }

  virtual void Accept(AstVisitor* v);

  Expression* cond() const { return cond_; }
  bool may_have_function_literal() const {
    return may_have_function_literal_;
  }

 private:
  Expression* cond_;
  // True if there is a function literal subexpression in the condition.
  bool may_have_function_literal_;

  friend class AstOptimizer;
};


class ForStatement: public IterationStatement {
 public:
  explicit ForStatement(ZoneStringList* labels)
      : IterationStatement(labels),
        init_(NULL),
        cond_(NULL),
        next_(NULL),
        may_have_function_literal_(true) {
  }

  void Initialize(Statement* init,
                  Expression* cond,
                  Statement* next,
                  Statement* body) {
    IterationStatement::Initialize(body);
    init_ = init;
    cond_ = cond;
    next_ = next;
  }

  virtual void Accept(AstVisitor* v);

  Statement* init() const  { return init_; }
  Expression* cond() const  { return cond_; }
  Statement* next() const  { return next_; }
  bool may_have_function_literal() const {
    return may_have_function_literal_;
  }

 private:
  Statement* init_;
  Expression* cond_;
  Statement* next_;
  // True if there is a function literal subexpression in the condition.
  bool may_have_function_literal_;

  friend class AstOptimizer;
};


class ForInStatement: public IterationStatement {
 public:
  explicit ForInStatement(ZoneStringList* labels)
      : IterationStatement(labels), each_(NULL), enumerable_(NULL) { }

  void Initialize(Expression* each, Expression* enumerable, Statement* body) {
    IterationStatement::Initialize(body);
    each_ = each;
    enumerable_ = enumerable;
  }

  virtual void Accept(AstVisitor* v);

  Expression* each() const { return each_; }
  Expression* enumerable() const { return enumerable_; }

 private:
  Expression* each_;
  Expression* enumerable_;
};


class ExpressionStatement: public Statement {
 public:
  explicit ExpressionStatement(Expression* expression)
      : expression_(expression) { }

  virtual void Accept(AstVisitor* v);

  // Type testing & conversion.
  virtual ExpressionStatement* AsExpressionStatement() { return this; }

  void set_expression(Expression* e) { expression_ = e; }
  Expression* expression() { return expression_; }

 private:
  Expression* expression_;
};


class ContinueStatement: public Statement {
 public:
  explicit ContinueStatement(IterationStatement* target)
      : target_(target) { }

  virtual void Accept(AstVisitor* v);

  IterationStatement* target() const  { return target_; }

 private:
  IterationStatement* target_;
};


class BreakStatement: public Statement {
 public:
  explicit BreakStatement(BreakableStatement* target)
      : target_(target) { }

  virtual void Accept(AstVisitor* v);

  BreakableStatement* target() const  { return target_; }

 private:
  BreakableStatement* target_;
};


class ReturnStatement: public Statement {
 public:
  explicit ReturnStatement(Expression* expression)
      : expression_(expression) { }

  virtual void Accept(AstVisitor* v);

  // Type testing & conversion.
  virtual ReturnStatement* AsReturnStatement() { return this; }

  Expression* expression() { return expression_; }

 private:
  Expression* expression_;
};


class WithEnterStatement: public Statement {
 public:
  explicit WithEnterStatement(Expression* expression, bool is_catch_block)
      : expression_(expression), is_catch_block_(is_catch_block) { }

  virtual void Accept(AstVisitor* v);

  Expression* expression() const  { return expression_; }

  bool is_catch_block() const { return is_catch_block_; }

 private:
  Expression* expression_;
  bool is_catch_block_;
};


class WithExitStatement: public Statement {
 public:
  WithExitStatement() { }

  virtual void Accept(AstVisitor* v);
};


class CaseClause: public ZoneObject {
 public:
  CaseClause(Expression* label, ZoneList<Statement*>* statements)
      : label_(label), statements_(statements) { }

  bool is_default() const  { return label_ == NULL; }
  Expression* label() const  {
    CHECK(!is_default());
    return label_;
  }
  JumpTarget* body_target() { return &body_target_; }
  ZoneList<Statement*>* statements() const  { return statements_; }

 private:
  Expression* label_;
  JumpTarget body_target_;
  ZoneList<Statement*>* statements_;
};


class SwitchStatement: public BreakableStatement {
 public:
  explicit SwitchStatement(ZoneStringList* labels)
      : BreakableStatement(labels, TARGET_FOR_ANONYMOUS),
        tag_(NULL), cases_(NULL) { }

  void Initialize(Expression* tag, ZoneList<CaseClause*>* cases) {
    tag_ = tag;
    cases_ = cases;
  }

  virtual void Accept(AstVisitor* v);

  Expression* tag() const  { return tag_; }
  ZoneList<CaseClause*>* cases() const  { return cases_; }

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
  IfStatement(Expression* condition,
              Statement* then_statement,
              Statement* else_statement)
      : condition_(condition),
        then_statement_(then_statement),
        else_statement_(else_statement) { }

  virtual void Accept(AstVisitor* v);

  bool HasThenStatement() const { return !then_statement()->IsEmpty(); }
  bool HasElseStatement() const { return !else_statement()->IsEmpty(); }

  Expression* condition() const { return condition_; }
  Statement* then_statement() const { return then_statement_; }
  Statement* else_statement() const { return else_statement_; }

 private:
  Expression* condition_;
  Statement* then_statement_;
  Statement* else_statement_;
};


// NOTE: TargetCollectors are represented as nodes to fit in the target
// stack in the compiler; this should probably be reworked.
class TargetCollector: public AstNode {
 public:
  explicit TargetCollector(ZoneList<BreakTarget*>* targets)
      : targets_(targets) {
  }

  // Adds a jump target to the collector. The collector stores a pointer not
  // a copy of the target to make binding work, so make sure not to pass in
  // references to something on the stack.
  void AddTarget(BreakTarget* target);

  // Virtual behaviour. TargetCollectors are never part of the AST.
  virtual void Accept(AstVisitor* v) { UNREACHABLE(); }
  virtual TargetCollector* AsTargetCollector() { return this; }

  ZoneList<BreakTarget*>* targets() { return targets_; }

 private:
  ZoneList<BreakTarget*>* targets_;
};


class TryStatement: public Statement {
 public:
  explicit TryStatement(Block* try_block)
      : try_block_(try_block), escaping_targets_(NULL) { }

  void set_escaping_targets(ZoneList<BreakTarget*>* targets) {
    escaping_targets_ = targets;
  }

  Block* try_block() const { return try_block_; }
  ZoneList<BreakTarget*>* escaping_targets() const { return escaping_targets_; }

 private:
  Block* try_block_;
  ZoneList<BreakTarget*>* escaping_targets_;
};


class TryCatchStatement: public TryStatement {
 public:
  TryCatchStatement(Block* try_block,
                    VariableProxy* catch_var,
                    Block* catch_block)
      : TryStatement(try_block),
        catch_var_(catch_var),
        catch_block_(catch_block) {
  }

  virtual void Accept(AstVisitor* v);

  VariableProxy* catch_var() const  { return catch_var_; }
  Block* catch_block() const  { return catch_block_; }

 private:
  VariableProxy* catch_var_;
  Block* catch_block_;
};


class TryFinallyStatement: public TryStatement {
 public:
  TryFinallyStatement(Block* try_block, Block* finally_block)
      : TryStatement(try_block),
        finally_block_(finally_block) { }

  virtual void Accept(AstVisitor* v);

  Block* finally_block() const { return finally_block_; }

 private:
  Block* finally_block_;
};


class DebuggerStatement: public Statement {
 public:
  virtual void Accept(AstVisitor* v);
};


class EmptyStatement: public Statement {
 public:
  virtual void Accept(AstVisitor* v);

  // Type testing & conversion.
  virtual EmptyStatement* AsEmptyStatement() { return this; }
};


class Literal: public Expression {
 public:
  explicit Literal(Handle<Object> handle) : handle_(handle) { }

  virtual void Accept(AstVisitor* v);

  // Type testing & conversion.
  virtual Literal* AsLiteral() { return this; }

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

  virtual bool IsLeaf() { return true; }

  // Identity testers.
  bool IsNull() const { return handle_.is_identical_to(Factory::null_value()); }
  bool IsTrue() const { return handle_.is_identical_to(Factory::true_value()); }
  bool IsFalse() const {
    return handle_.is_identical_to(Factory::false_value());
  }

  Handle<Object> handle() const { return handle_; }

 private:
  Handle<Object> handle_;
};


// Base class for literals that needs space in the corresponding JSFunction.
class MaterializedLiteral: public Expression {
 public:
  explicit MaterializedLiteral(int literal_index, bool is_simple, int depth)
      : literal_index_(literal_index), is_simple_(is_simple), depth_(depth) {}

  virtual MaterializedLiteral* AsMaterializedLiteral() { return this; }

  int literal_index() { return literal_index_; }

  // A materialized literal is simple if the values consist of only
  // constants and simple object and array literals.
  bool is_simple() const { return is_simple_; }

  int depth() const { return depth_; }

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

   private:
    Literal* key_;
    Expression* value_;
    Kind kind_;
  };

  ObjectLiteral(Handle<FixedArray> constant_properties,
                ZoneList<Property*>* properties,
                int literal_index,
                bool is_simple,
                int depth)
      : MaterializedLiteral(literal_index, is_simple, depth),
        constant_properties_(constant_properties),
        properties_(properties) {}

  virtual ObjectLiteral* AsObjectLiteral() { return this; }
  virtual void Accept(AstVisitor* v);

  virtual bool IsLeaf() { return properties()->is_empty(); }

  Handle<FixedArray> constant_properties() const {
    return constant_properties_;
  }
  ZoneList<Property*>* properties() const { return properties_; }

 private:
  Handle<FixedArray> constant_properties_;
  ZoneList<Property*>* properties_;
};


// Node for capturing a regexp literal.
class RegExpLiteral: public MaterializedLiteral {
 public:
  RegExpLiteral(Handle<String> pattern,
                Handle<String> flags,
                int literal_index)
      : MaterializedLiteral(literal_index, false, 1),
        pattern_(pattern),
        flags_(flags) {}

  virtual void Accept(AstVisitor* v);

  virtual bool IsLeaf() { return true; }

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
  ArrayLiteral(Handle<FixedArray> constant_elements,
               ZoneList<Expression*>* values,
               int literal_index,
               bool is_simple,
               int depth)
      : MaterializedLiteral(literal_index, is_simple, depth),
        constant_elements_(constant_elements),
        values_(values) {}

  virtual void Accept(AstVisitor* v);
  virtual ArrayLiteral* AsArrayLiteral() { return this; }

  virtual bool IsLeaf() { return values()->is_empty(); }

  Handle<FixedArray> constant_elements() const { return constant_elements_; }
  ZoneList<Expression*>* values() const { return values_; }

 private:
  Handle<FixedArray> constant_elements_;
  ZoneList<Expression*>* values_;
};


// Node for constructing a context extension object for a catch block.
// The catch context extension object has one property, the catch
// variable, which should be DontDelete.
class CatchExtensionObject: public Expression {
 public:
  CatchExtensionObject(Literal* key, VariableProxy* value)
      : key_(key), value_(value) {
  }

  virtual void Accept(AstVisitor* v);

  Literal* key() const { return key_; }
  VariableProxy* value() const { return value_; }

 private:
  Literal* key_;
  VariableProxy* value_;
};


class VariableProxy: public Expression {
 public:
  virtual void Accept(AstVisitor* v);

  // Type testing & conversion
  virtual Property* AsProperty() {
    return var_ == NULL ? NULL : var_->AsProperty();
  }
  virtual VariableProxy* AsVariableProxy()  { return this; }

  Variable* AsVariable() {
    return this == NULL || var_ == NULL ? NULL : var_->AsVariable();
  }

  virtual bool IsValidLeftHandSide() {
    return var_ == NULL ? true : var_->IsValidLeftHandSide();
  }

  virtual bool IsLeaf() {
    ASSERT(var_ != NULL);  // Variable must be resolved.
    return var()->is_global() || var()->rewrite()->IsLeaf();
  }

  bool IsVariable(Handle<String> n) {
    return !is_this() && name().is_identical_to(n);
  }

  bool IsArguments() {
    Variable* variable = AsVariable();
    return (variable == NULL) ? false : variable->is_arguments();
  }

  Handle<String> name() const  { return name_; }
  Variable* var() const  { return var_; }
  UseCount* var_uses()  { return &var_uses_; }
  UseCount* obj_uses()  { return &obj_uses_; }
  bool is_this() const  { return is_this_; }
  bool inside_with() const  { return inside_with_; }

  // Bind this proxy to the variable var.
  void BindTo(Variable* var);

 protected:
  Handle<String> name_;
  Variable* var_;  // resolved variable, or NULL
  bool is_this_;
  bool inside_with_;

  // VariableProxy usage info.
  UseCount var_uses_;  // uses of the variable value
  UseCount obj_uses_;  // uses of the object the variable points to

  VariableProxy(Handle<String> name, bool is_this, bool inside_with);
  explicit VariableProxy(bool is_this);

  friend class Scope;
};


class VariableProxySentinel: public VariableProxy {
 public:
  virtual bool IsValidLeftHandSide() { return !is_this(); }
  static VariableProxySentinel* this_proxy() { return &this_proxy_; }
  static VariableProxySentinel* identifier_proxy() {
    return &identifier_proxy_;
  }

 private:
  explicit VariableProxySentinel(bool is_this) : VariableProxy(is_this) { }
  static VariableProxySentinel this_proxy_;
  static VariableProxySentinel identifier_proxy_;
};


class Slot: public Expression {
 public:
  enum Type {
    // A slot in the parameter section on the stack. index() is
    // the parameter index, counting left-to-right, starting at 0.
    PARAMETER,

    // A slot in the local section on the stack. index() is
    // the variable index in the stack frame, starting at 0.
    LOCAL,

    // An indexed slot in a heap context. index() is the
    // variable index in the context object on the heap,
    // starting at 0. var()->scope() is the corresponding
    // scope.
    CONTEXT,

    // A named slot in a heap context. var()->name() is the
    // variable name in the context object on the heap,
    // with lookup starting at the current context. index()
    // is invalid.
    LOOKUP
  };

  Slot(Variable* var, Type type, int index)
      : var_(var), type_(type), index_(index) {
    ASSERT(var != NULL);
  }

  virtual void Accept(AstVisitor* v);

  // Type testing & conversion
  virtual Slot* AsSlot() { return this; }

  virtual bool IsLeaf() { return true; }

  // Accessors
  Variable* var() const { return var_; }
  Type type() const { return type_; }
  int index() const { return index_; }
  bool is_arguments() const { return var_->is_arguments(); }

 private:
  Variable* var_;
  Type type_;
  int index_;
};


class Property: public Expression {
 public:
  // Synthetic properties are property lookups introduced by the system,
  // to objects that aren't visible to the user. Function calls to synthetic
  // properties should use the global object as receiver, not the base object
  // of the resolved Reference.
  enum Type { NORMAL, SYNTHETIC };
  Property(Expression* obj, Expression* key, int pos, Type type = NORMAL)
      : obj_(obj), key_(key), pos_(pos), type_(type) { }

  virtual void Accept(AstVisitor* v);

  // Type testing & conversion
  virtual Property* AsProperty() { return this; }

  virtual bool IsValidLeftHandSide() { return true; }

  Expression* obj() const { return obj_; }
  Expression* key() const { return key_; }
  int position() const { return pos_; }
  bool is_synthetic() const { return type_ == SYNTHETIC; }

  // Returns a property singleton property access on 'this'.  Used
  // during preparsing.
  static Property* this_property() { return &this_property_; }

 private:
  Expression* obj_;
  Expression* key_;
  int pos_;
  Type type_;

  // Dummy property used during preparsing.
  static Property this_property_;
};


class Call: public Expression {
 public:
  Call(Expression* expression, ZoneList<Expression*>* arguments, int pos)
      : expression_(expression), arguments_(arguments), pos_(pos) { }

  virtual void Accept(AstVisitor* v);

  // Type testing and conversion.
  virtual Call* AsCall() { return this; }

  Expression* expression() const { return expression_; }
  ZoneList<Expression*>* arguments() const { return arguments_; }
  int position() { return pos_; }

  static Call* sentinel() { return &sentinel_; }

 private:
  Expression* expression_;
  ZoneList<Expression*>* arguments_;
  int pos_;

  static Call sentinel_;
};


class CallNew: public Expression {
 public:
  CallNew(Expression* expression, ZoneList<Expression*>* arguments, int pos)
      : expression_(expression), arguments_(arguments), pos_(pos) { }

  virtual void Accept(AstVisitor* v);

  Expression* expression() const { return expression_; }
  ZoneList<Expression*>* arguments() const { return arguments_; }
  int position() { return pos_; }

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
  CallRuntime(Handle<String> name,
              Runtime::Function* function,
              ZoneList<Expression*>* arguments)
      : name_(name), function_(function), arguments_(arguments) { }

  virtual void Accept(AstVisitor* v);

  Handle<String> name() const { return name_; }
  Runtime::Function* function() const { return function_; }
  ZoneList<Expression*>* arguments() const { return arguments_; }
  bool is_jsruntime() const { return function_ == NULL; }

 private:
  Handle<String> name_;
  Runtime::Function* function_;
  ZoneList<Expression*>* arguments_;
};


class UnaryOperation: public Expression {
 public:
  UnaryOperation(Token::Value op, Expression* expression)
      : op_(op), expression_(expression) {
    ASSERT(Token::IsUnaryOp(op));
  }

  virtual void Accept(AstVisitor* v);

  // Type testing & conversion
  virtual UnaryOperation* AsUnaryOperation() { return this; }

  Token::Value op() const { return op_; }
  Expression* expression() const { return expression_; }

 private:
  Token::Value op_;
  Expression* expression_;
};


class BinaryOperation: public Expression {
 public:
  BinaryOperation(Token::Value op, Expression* left, Expression* right)
      : op_(op), left_(left), right_(right) {
    ASSERT(Token::IsBinaryOp(op));
  }

  virtual void Accept(AstVisitor* v);

  // Type testing & conversion
  virtual BinaryOperation* AsBinaryOperation() { return this; }

  // True iff the result can be safely overwritten (to avoid allocation).
  // False for operations that can return one of their operands.
  bool ResultOverwriteAllowed() {
    switch (op_) {
      case Token::COMMA:
      case Token::OR:
      case Token::AND:
        return false;
      case Token::BIT_OR:
      case Token::BIT_XOR:
      case Token::BIT_AND:
      case Token::SHL:
      case Token::SAR:
      case Token::SHR:
      case Token::ADD:
      case Token::SUB:
      case Token::MUL:
      case Token::DIV:
      case Token::MOD:
        return true;
      default:
        UNREACHABLE();
    }
    return false;
  }

  Token::Value op() const { return op_; }
  Expression* left() const { return left_; }
  Expression* right() const { return right_; }

 private:
  Token::Value op_;
  Expression* left_;
  Expression* right_;
};


class CountOperation: public Expression {
 public:
  CountOperation(bool is_prefix, Token::Value op, Expression* expression)
      : is_prefix_(is_prefix), op_(op), expression_(expression) {
    ASSERT(Token::IsCountOp(op));
  }

  virtual void Accept(AstVisitor* v);

  bool is_prefix() const { return is_prefix_; }
  bool is_postfix() const { return !is_prefix_; }
  Token::Value op() const { return op_; }
  Token::Value binary_op() {
    return op_ == Token::INC ? Token::ADD : Token::SUB;
  }
  Expression* expression() const { return expression_; }

  virtual void MarkAsStatement() { is_prefix_ = true; }

 private:
  bool is_prefix_;
  Token::Value op_;
  Expression* expression_;
};


class CompareOperation: public Expression {
 public:
  CompareOperation(Token::Value op, Expression* left, Expression* right)
      : op_(op), left_(left), right_(right), is_for_loop_condition_(false) {
    ASSERT(Token::IsCompareOp(op));
  }

  virtual void Accept(AstVisitor* v);

  Token::Value op() const { return op_; }
  Expression* left() const { return left_; }
  Expression* right() const { return right_; }

  // Accessors for flag whether this compare operation is hanging of a for loop.
  bool is_for_loop_condition() const { return is_for_loop_condition_; }
  void set_is_for_loop_condition() { is_for_loop_condition_ = true; }

  // Type testing & conversion
  virtual CompareOperation* AsCompareOperation() { return this; }

 private:
  Token::Value op_;
  Expression* left_;
  Expression* right_;
  bool is_for_loop_condition_;
};


class Conditional: public Expression {
 public:
  Conditional(Expression* condition,
              Expression* then_expression,
              Expression* else_expression)
      : condition_(condition),
        then_expression_(then_expression),
        else_expression_(else_expression) { }

  virtual void Accept(AstVisitor* v);

  Expression* condition() const { return condition_; }
  Expression* then_expression() const { return then_expression_; }
  Expression* else_expression() const { return else_expression_; }

 private:
  Expression* condition_;
  Expression* then_expression_;
  Expression* else_expression_;
};


class Assignment: public Expression {
 public:
  Assignment(Token::Value op, Expression* target, Expression* value, int pos)
      : op_(op), target_(target), value_(value), pos_(pos),
        block_start_(false), block_end_(false) {
    ASSERT(Token::IsAssignmentOp(op));
  }

  virtual void Accept(AstVisitor* v);
  virtual Assignment* AsAssignment() { return this; }

  Token::Value binary_op() const;

  Token::Value op() const { return op_; }
  Expression* target() const { return target_; }
  Expression* value() const { return value_; }
  int position() { return pos_; }
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

 private:
  Token::Value op_;
  Expression* target_;
  Expression* value_;
  int pos_;
  bool block_start_;
  bool block_end_;
};


class Throw: public Expression {
 public:
  Throw(Expression* exception, int pos)
      : exception_(exception), pos_(pos) {}

  virtual void Accept(AstVisitor* v);
  Expression* exception() const { return exception_; }
  int position() const { return pos_; }

 private:
  Expression* exception_;
  int pos_;
};


class FunctionLiteral: public Expression {
 public:
  FunctionLiteral(Handle<String> name,
                  Scope* scope,
                  ZoneList<Statement*>* body,
                  int materialized_literal_count,
                  int expected_property_count,
                  bool has_only_simple_this_property_assignments,
                  Handle<FixedArray> this_property_assignments,
                  int num_parameters,
                  int start_position,
                  int end_position,
                  bool is_expression)
      : name_(name),
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
        is_expression_(is_expression),
        function_token_position_(RelocInfo::kNoPosition),
        inferred_name_(Heap::empty_string()),
        try_full_codegen_(false) {
#ifdef DEBUG
    already_compiled_ = false;
#endif
  }

  virtual void Accept(AstVisitor* v);

  // Type testing & conversion
  virtual FunctionLiteral* AsFunctionLiteral()  { return this; }

  virtual bool IsLeaf() { return true; }

  Handle<String> name() const  { return name_; }
  Scope* scope() const  { return scope_; }
  ZoneList<Statement*>* body() const  { return body_; }
  void set_function_token_position(int pos) { function_token_position_ = pos; }
  int function_token_position() const { return function_token_position_; }
  int start_position() const { return start_position_; }
  int end_position() const { return end_position_; }
  bool is_expression() const { return is_expression_; }

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

  Handle<String> inferred_name() const  { return inferred_name_; }
  void set_inferred_name(Handle<String> inferred_name) {
    inferred_name_ = inferred_name;
  }

  bool try_full_codegen() { return try_full_codegen_; }
  void set_try_full_codegen(bool flag) { try_full_codegen_ = flag; }

#ifdef DEBUG
  void mark_as_compiled() {
    ASSERT(!already_compiled_);
    already_compiled_ = true;
  }
#endif

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
  bool is_expression_;
  int function_token_position_;
  Handle<String> inferred_name_;
  bool try_full_codegen_;
#ifdef DEBUG
  bool already_compiled_;
#endif
};


class FunctionBoilerplateLiteral: public Expression {
 public:
  explicit FunctionBoilerplateLiteral(Handle<JSFunction> boilerplate)
      : boilerplate_(boilerplate) {
    ASSERT(boilerplate->IsBoilerplate());
  }

  Handle<JSFunction> boilerplate() const { return boilerplate_; }

  virtual bool IsLeaf() { return true; }

  virtual void Accept(AstVisitor* v);

 private:
  Handle<JSFunction> boilerplate_;
};


class ThisFunction: public Expression {
 public:
  virtual void Accept(AstVisitor* v);
  virtual bool IsLeaf() { return true; }
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
  virtual bool IsAnchored() { return false; }
  virtual int min_match() = 0;
  virtual int max_match() = 0;
  // Returns the interval of registers used for captures within this
  // expression.
  virtual Interval CaptureRegisters() { return Interval::Empty(); }
  virtual void AppendToText(RegExpText* text);
  SmartPointer<const char> ToString();
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
  virtual bool IsAnchored();
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
  virtual bool IsAnchored();
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
  virtual bool IsAnchored();
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
  };
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
  virtual bool IsAnchored();
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
  virtual bool IsAnchored();
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
  AstVisitor() : stack_overflow_(false) { }
  virtual ~AstVisitor() { }

  // Dispatch
  void Visit(AstNode* node) { node->Accept(this); }

  // Iteration
  virtual void VisitDeclarations(ZoneList<Declaration*>* declarations);
  virtual void VisitStatements(ZoneList<Statement*>* statements);
  virtual void VisitExpressions(ZoneList<Expression*>* expressions);

  // Stack overflow tracking support.
  bool HasStackOverflow() const { return stack_overflow_; }
  bool CheckStackOverflow() {
    if (stack_overflow_) return true;
    StackLimitCheck check;
    if (!check.HasOverflowed()) return false;
    return (stack_overflow_ = true);
  }

  // If a stack-overflow exception is encountered when visiting a
  // node, calling SetStackOverflow will make sure that the visitor
  // bails out without visiting more nodes.
  void SetStackOverflow() { stack_overflow_ = true; }


  // Individual nodes
#define DEF_VISIT(type)                         \
  virtual void Visit##type(type* node) = 0;
  AST_NODE_LIST(DEF_VISIT)
#undef DEF_VISIT

 private:
  bool stack_overflow_;
};


} }  // namespace v8::internal

#endif  // V8_AST_H_
