// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_AST_AST_H_
#define V8_AST_AST_H_

#include <memory>

#include "src/ast/ast-value-factory.h"
#include "src/ast/modules.h"
#include "src/ast/variables.h"
#include "src/bailout-reason.h"
#include "src/base/threaded-list.h"
#include "src/globals.h"
#include "src/heap/factory.h"
#include "src/isolate.h"
#include "src/label.h"
#include "src/objects/literal-objects.h"
#include "src/objects/smi.h"
#include "src/parsing/token.h"
#include "src/runtime/runtime.h"

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

#define DECLARATION_NODE_LIST(V) \
  V(VariableDeclaration)         \
  V(FunctionDeclaration)

#define ITERATION_NODE_LIST(V) \
  V(DoWhileStatement)          \
  V(WhileStatement)            \
  V(ForStatement)              \
  V(ForInStatement)            \
  V(ForOfStatement)

#define BREAKABLE_NODE_LIST(V) \
  V(Block)                     \
  V(SwitchStatement)

#define STATEMENT_NODE_LIST(V)    \
  ITERATION_NODE_LIST(V)          \
  BREAKABLE_NODE_LIST(V)          \
  V(ExpressionStatement)          \
  V(EmptyStatement)               \
  V(SloppyBlockFunctionStatement) \
  V(IfStatement)                  \
  V(ContinueStatement)            \
  V(BreakStatement)               \
  V(ReturnStatement)              \
  V(WithStatement)                \
  V(TryCatchStatement)            \
  V(TryFinallyStatement)          \
  V(DebuggerStatement)            \
  V(InitializeClassMembersStatement)

#define LITERAL_NODE_LIST(V) \
  V(RegExpLiteral)           \
  V(ObjectLiteral)           \
  V(ArrayLiteral)

#define EXPRESSION_NODE_LIST(V) \
  LITERAL_NODE_LIST(V)          \
  V(Assignment)                 \
  V(Await)                      \
  V(BinaryOperation)            \
  V(NaryOperation)              \
  V(Call)                       \
  V(CallNew)                    \
  V(CallRuntime)                \
  V(ClassLiteral)               \
  V(CompareOperation)           \
  V(CompoundAssignment)         \
  V(Conditional)                \
  V(CountOperation)             \
  V(DoExpression)               \
  V(EmptyParentheses)           \
  V(FunctionLiteral)            \
  V(GetTemplateObject)          \
  V(ImportCallExpression)       \
  V(Literal)                    \
  V(NativeFunctionLiteral)      \
  V(Property)                   \
  V(ResolvedProperty)           \
  V(Spread)                     \
  V(StoreInArrayLiteral)        \
  V(SuperCallReference)         \
  V(SuperPropertyReference)     \
  V(TemplateLiteral)            \
  V(ThisExpression)             \
  V(Throw)                      \
  V(UnaryOperation)             \
  V(VariableProxy)              \
  V(Yield)                      \
  V(YieldStar)

#define FAILURE_NODE_LIST(V) V(FailureExpression)

#define AST_NODE_LIST(V)                        \
  DECLARATION_NODE_LIST(V)                      \
  STATEMENT_NODE_LIST(V)                        \
  EXPRESSION_NODE_LIST(V)

// Forward declarations
class AstNode;
class AstNodeFactory;
class Declaration;
class BreakableStatement;
class Expression;
class IterationStatement;
class MaterializedLiteral;
class NestedVariableDeclaration;
class ProducedPreparseData;
class Statement;

#define DEF_FORWARD_DECLARATION(type) class type;
AST_NODE_LIST(DEF_FORWARD_DECLARATION)
FAILURE_NODE_LIST(DEF_FORWARD_DECLARATION)
#undef DEF_FORWARD_DECLARATION

class AstNode: public ZoneObject {
 public:
#define DECLARE_TYPE_ENUM(type) k##type,
  enum NodeType : uint8_t {
    AST_NODE_LIST(DECLARE_TYPE_ENUM) /* , */
    FAILURE_NODE_LIST(DECLARE_TYPE_ENUM)
  };
#undef DECLARE_TYPE_ENUM

  void* operator new(size_t size, Zone* zone) { return zone->New(size); }

  NodeType node_type() const { return NodeTypeField::decode(bit_field_); }
  int position() const { return position_; }

#ifdef DEBUG
  void Print();
  void Print(Isolate* isolate);
#endif  // DEBUG

  // Type testing & conversion functions overridden by concrete subclasses.
#define DECLARE_NODE_FUNCTIONS(type) \
  V8_INLINE bool Is##type() const;   \
  V8_INLINE type* As##type();        \
  V8_INLINE const type* As##type() const;
  AST_NODE_LIST(DECLARE_NODE_FUNCTIONS)
  FAILURE_NODE_LIST(DECLARE_NODE_FUNCTIONS)
#undef DECLARE_NODE_FUNCTIONS

  IterationStatement* AsIterationStatement();
  MaterializedLiteral* AsMaterializedLiteral();

 private:
  // Hidden to prevent accidental usage. It would have to load the
  // current zone from the TLS.
  void* operator new(size_t size);

  int position_;
  class NodeTypeField : public BitField<NodeType, 0, 6> {};

 protected:
  uint32_t bit_field_;
  static const uint8_t kNextBitFieldIndex = NodeTypeField::kNext;

  AstNode(int position, NodeType type)
      : position_(position), bit_field_(NodeTypeField::encode(type)) {}
};


class Statement : public AstNode {
 protected:
  Statement(int position, NodeType type) : AstNode(position, type) {}

  static const uint8_t kNextBitFieldIndex = AstNode::kNextBitFieldIndex;
};


class Expression : public AstNode {
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

  // True iff the expression is a valid reference expression.
  bool IsValidReferenceExpression() const;

  // Helpers for ToBoolean conversion.
  bool ToBooleanIsTrue() const;
  bool ToBooleanIsFalse() const;

  // Symbols that cannot be parsed as array indices are considered property
  // names.  We do not treat symbols that can be array indexes as property
  // names because [] for string objects is handled only by keyed ICs.
  bool IsPropertyName() const;

  // True iff the expression is a class or function expression without
  // a syntactic name.
  bool IsAnonymousFunctionDefinition() const;

  // True iff the expression is a concise method definition.
  bool IsConciseMethodDefinition() const;

  // True iff the expression is an accessor function definition.
  bool IsAccessorFunctionDefinition() const;

  // True iff the expression is a literal represented as a smi.
  bool IsSmiLiteral() const;

  // True iff the expression is a literal represented as a number.
  V8_EXPORT_PRIVATE bool IsNumberLiteral() const;

  // True iff the expression is a string literal.
  bool IsStringLiteral() const;

  // True iff the expression is the null literal.
  bool IsNullLiteral() const;

  // True iff the expression is the hole literal.
  bool IsTheHoleLiteral() const;

  // True if we can prove that the expression is the undefined literal. Note
  // that this also checks for loads of the global "undefined" variable.
  bool IsUndefinedLiteral() const;

  bool IsCompileTimeValue();

  bool IsPattern() {
    STATIC_ASSERT(kObjectLiteral + 1 == kArrayLiteral);
    return IsInRange(node_type(), kObjectLiteral, kArrayLiteral);
  }

  bool is_parenthesized() const {
    return IsParenthesizedField::decode(bit_field_);
  }

  void mark_parenthesized() {
    bit_field_ = IsParenthesizedField::update(bit_field_, true);
  }

  void clear_parenthesized() {
    bit_field_ = IsParenthesizedField::update(bit_field_, false);
  }

 private:
  class IsParenthesizedField
      : public BitField<bool, AstNode::kNextBitFieldIndex, 1> {};

 protected:
  Expression(int pos, NodeType type) : AstNode(pos, type) {
    DCHECK(!is_parenthesized());
  }

  static const uint8_t kNextBitFieldIndex = IsParenthesizedField::kNext;
};

class FailureExpression : public Expression {
 private:
  friend class AstNodeFactory;
  FailureExpression() : Expression(kNoSourcePosition, kFailureExpression) {}
};

// V8's notion of BreakableStatement does not correspond to the notion of
// BreakableStatement in ECMAScript. In V8, the idea is that a
// BreakableStatement is a statement that can be the target of a break
// statement.  The BreakableStatement AST node carries a list of labels, any of
// which can be used as an argument to the break statement in order to target
// it.
//
// Since we don't want to attach a list of labels to all kinds of statements, we
// only declare switchs, loops, and blocks as BreakableStatements.  This means
// that we implement breaks targeting other statement forms as breaks targeting
// a substatement thereof. For instance, in "foo: if (b) { f(); break foo; }" we
// pretend that foo is the label of the inner block. That's okay because one
// can't observe the difference.
//
// This optimization makes it harder to detect invalid continue labels, see the
// need for own_labels in IterationStatement.
//
class BreakableStatement : public Statement {
 public:
  enum BreakableType {
    TARGET_FOR_ANONYMOUS,
    TARGET_FOR_NAMED_ONLY
  };

  // A list of all labels declared on the path up to the previous
  // BreakableStatement (if any).
  //
  // Example: "l1: for (;;) l2: l3: { l4: if (b) l5: { s } }"
  // labels() of the ForStatement will be l1.
  // labels() of the Block { l4: ... } will be l2, l3.
  // labels() of the Block { s } will be l4, l5.
  ZonePtrList<const AstRawString>* labels() const;

  // Testers.
  bool is_target_for_anonymous() const {
    return BreakableTypeField::decode(bit_field_) == TARGET_FOR_ANONYMOUS;
  }

 private:
  class BreakableTypeField
      : public BitField<BreakableType, Statement::kNextBitFieldIndex, 1> {};

 protected:
  BreakableStatement(BreakableType breakable_type, int position, NodeType type)
      : Statement(position, type) {
    bit_field_ |= BreakableTypeField::encode(breakable_type);
  }

  static const uint8_t kNextBitFieldIndex = BreakableTypeField::kNext;
};

class Block : public BreakableStatement {
 public:
  ZonePtrList<Statement>* statements() { return &statements_; }
  bool ignore_completion_value() const {
    return IgnoreCompletionField::decode(bit_field_);
  }

  inline ZonePtrList<const AstRawString>* labels() const;

  Scope* scope() const { return scope_; }
  void set_scope(Scope* scope) { scope_ = scope; }

  void InitializeStatements(const ScopedPtrList<Statement>& statements,
                            Zone* zone) {
    DCHECK_EQ(0, statements_.length());
    statements.CopyTo(&statements_, zone);
  }

 private:
  friend class AstNodeFactory;

  ZonePtrList<Statement> statements_;
  Scope* scope_;

  class IgnoreCompletionField
      : public BitField<bool, BreakableStatement::kNextBitFieldIndex, 1> {};
  class IsLabeledField
      : public BitField<bool, IgnoreCompletionField::kNext, 1> {};

 protected:
  Block(Zone* zone, ZonePtrList<const AstRawString>* labels, int capacity,
        bool ignore_completion_value)
      : BreakableStatement(TARGET_FOR_NAMED_ONLY, kNoSourcePosition, kBlock),
        statements_(capacity, zone),
        scope_(nullptr) {
    bit_field_ |= IgnoreCompletionField::encode(ignore_completion_value) |
                  IsLabeledField::encode(labels != nullptr);
  }

  Block(ZonePtrList<const AstRawString>* labels, bool ignore_completion_value)
      : Block(nullptr, labels, 0, ignore_completion_value) {}
};

class LabeledBlock final : public Block {
 private:
  friend class AstNodeFactory;
  friend class Block;

  LabeledBlock(Zone* zone, ZonePtrList<const AstRawString>* labels,
               int capacity, bool ignore_completion_value)
      : Block(zone, labels, capacity, ignore_completion_value),
        labels_(labels) {
    DCHECK_NOT_NULL(labels);
    DCHECK_GT(labels->length(), 0);
  }

  LabeledBlock(ZonePtrList<const AstRawString>* labels,
               bool ignore_completion_value)
      : LabeledBlock(nullptr, labels, 0, ignore_completion_value) {}

  ZonePtrList<const AstRawString>* labels_;
};

inline ZonePtrList<const AstRawString>* Block::labels() const {
  if (IsLabeledField::decode(bit_field_)) {
    return static_cast<const LabeledBlock*>(this)->labels_;
  }
  return nullptr;
}

class DoExpression final : public Expression {
 public:
  Block* block() { return block_; }
  VariableProxy* result() { return result_; }

 private:
  friend class AstNodeFactory;

  DoExpression(Block* block, VariableProxy* result, int pos)
      : Expression(pos, kDoExpression), block_(block), result_(result) {
    DCHECK_NOT_NULL(block_);
    DCHECK_NOT_NULL(result_);
  }

  Block* block_;
  VariableProxy* result_;
};


class Declaration : public AstNode {
 public:
  typedef base::ThreadedList<Declaration> List;

  Variable* var() const { return var_; }
  void set_var(Variable* var) { var_ = var; }

 protected:
  Declaration(int pos, NodeType type) : AstNode(pos, type), next_(nullptr) {}

 private:
  Variable* var_;
  // Declarations list threaded through the declarations.
  Declaration** next() { return &next_; }
  Declaration* next_;
  friend List;
  friend base::ThreadedListTraits<Declaration>;
};

class VariableDeclaration : public Declaration {
 public:
  inline NestedVariableDeclaration* AsNested();

 private:
  friend class AstNodeFactory;

  class IsNestedField
      : public BitField<bool, Declaration::kNextBitFieldIndex, 1> {};

 protected:
  explicit VariableDeclaration(int pos, bool is_nested = false)
      : Declaration(pos, kVariableDeclaration) {
    bit_field_ = IsNestedField::update(bit_field_, is_nested);
  }

  static const uint8_t kNextBitFieldIndex = IsNestedField::kNext;
};

// For var declarations that appear in a block scope.
// Only distinguished from VariableDeclaration during Scope analysis,
// so it doesn't get its own NodeType.
class NestedVariableDeclaration final : public VariableDeclaration {
 public:
  Scope* scope() const { return scope_; }

 private:
  friend class AstNodeFactory;

  NestedVariableDeclaration(Scope* scope, int pos)
      : VariableDeclaration(pos, true), scope_(scope) {}

  // Nested scope from which the declaration originated.
  Scope* scope_;
};

inline NestedVariableDeclaration* VariableDeclaration::AsNested() {
  return IsNestedField::decode(bit_field_)
             ? static_cast<NestedVariableDeclaration*>(this)
             : nullptr;
}

class FunctionDeclaration final : public Declaration {
 public:
  FunctionLiteral* fun() const { return fun_; }

 private:
  friend class AstNodeFactory;

  FunctionDeclaration(FunctionLiteral* fun, int pos)
      : Declaration(pos, kFunctionDeclaration), fun_(fun) {}

  FunctionLiteral* fun_;
};


class IterationStatement : public BreakableStatement {
 public:
  Statement* body() const { return body_; }
  void set_body(Statement* s) { body_ = s; }

  ZonePtrList<const AstRawString>* labels() const { return labels_; }

  // A list of all labels that the iteration statement is directly prefixed
  // with, i.e.  all the labels that a continue statement in the body can use to
  // continue this iteration statement. This is always a subset of {labels}.
  //
  // Example: "l1: { l2: if (b) l3: l4: for (;;) s }"
  // labels() of the Block will be l1.
  // labels() of the ForStatement will be l2, l3, l4.
  // own_labels() of the ForStatement will be l3, l4.
  ZonePtrList<const AstRawString>* own_labels() const { return own_labels_; }

 protected:
  IterationStatement(ZonePtrList<const AstRawString>* labels,
                     ZonePtrList<const AstRawString>* own_labels, int pos,
                     NodeType type)
      : BreakableStatement(TARGET_FOR_ANONYMOUS, pos, type),
        labels_(labels),
        own_labels_(own_labels),
        body_(nullptr) {}
  void Initialize(Statement* body) { body_ = body; }

  static const uint8_t kNextBitFieldIndex =
      BreakableStatement::kNextBitFieldIndex;

 private:
  ZonePtrList<const AstRawString>* labels_;
  ZonePtrList<const AstRawString>* own_labels_;
  Statement* body_;
};


class DoWhileStatement final : public IterationStatement {
 public:
  void Initialize(Expression* cond, Statement* body) {
    IterationStatement::Initialize(body);
    cond_ = cond;
  }

  Expression* cond() const { return cond_; }

 private:
  friend class AstNodeFactory;

  DoWhileStatement(ZonePtrList<const AstRawString>* labels,
                   ZonePtrList<const AstRawString>* own_labels, int pos)
      : IterationStatement(labels, own_labels, pos, kDoWhileStatement),
        cond_(nullptr) {}

  Expression* cond_;
};


class WhileStatement final : public IterationStatement {
 public:
  void Initialize(Expression* cond, Statement* body) {
    IterationStatement::Initialize(body);
    cond_ = cond;
  }

  Expression* cond() const { return cond_; }

 private:
  friend class AstNodeFactory;

  WhileStatement(ZonePtrList<const AstRawString>* labels,
                 ZonePtrList<const AstRawString>* own_labels, int pos)
      : IterationStatement(labels, own_labels, pos, kWhileStatement),
        cond_(nullptr) {}

  Expression* cond_;
};


class ForStatement final : public IterationStatement {
 public:
  void Initialize(Statement* init, Expression* cond, Statement* next,
                  Statement* body) {
    IterationStatement::Initialize(body);
    init_ = init;
    cond_ = cond;
    next_ = next;
  }

  Statement* init() const { return init_; }
  Expression* cond() const { return cond_; }
  Statement* next() const { return next_; }

 private:
  friend class AstNodeFactory;

  ForStatement(ZonePtrList<const AstRawString>* labels,
               ZonePtrList<const AstRawString>* own_labels, int pos)
      : IterationStatement(labels, own_labels, pos, kForStatement),
        init_(nullptr),
        cond_(nullptr),
        next_(nullptr) {}

  Statement* init_;
  Expression* cond_;
  Statement* next_;
};

// Shared class for for-in and for-of statements.
class ForEachStatement : public IterationStatement {
 public:
  enum VisitMode {
    ENUMERATE,   // for (each in subject) body;
    ITERATE      // for (each of subject) body;
  };

  using IterationStatement::Initialize;

  static const char* VisitModeString(VisitMode mode) {
    return mode == ITERATE ? "for-of" : "for-in";
  }

  void Initialize(Expression* each, Expression* subject, Statement* body) {
    IterationStatement::Initialize(body);
    each_ = each;
    subject_ = subject;
  }

  Expression* each() const { return each_; }
  Expression* subject() const { return subject_; }

 protected:
  friend class AstNodeFactory;

  ForEachStatement(ZonePtrList<const AstRawString>* labels,
                   ZonePtrList<const AstRawString>* own_labels, int pos,
                   NodeType type)
      : IterationStatement(labels, own_labels, pos, type),
        each_(nullptr),
        subject_(nullptr) {}

  Expression* each_;
  Expression* subject_;
};

class ForInStatement final : public ForEachStatement {
 private:
  friend class AstNodeFactory;

  ForInStatement(ZonePtrList<const AstRawString>* labels,
                 ZonePtrList<const AstRawString>* own_labels, int pos)
      : ForEachStatement(labels, own_labels, pos, kForInStatement) {}
};

enum class IteratorType { kNormal, kAsync };
class ForOfStatement final : public ForEachStatement {
 public:
  IteratorType type() const { return type_; }

 private:
  friend class AstNodeFactory;

  ForOfStatement(ZonePtrList<const AstRawString>* labels,
                 ZonePtrList<const AstRawString>* own_labels, int pos,
                 IteratorType type)
      : ForEachStatement(labels, own_labels, pos, kForOfStatement),
        type_(type) {}

  IteratorType type_;
};

class ExpressionStatement final : public Statement {
 public:
  void set_expression(Expression* e) { expression_ = e; }
  Expression* expression() const { return expression_; }

 private:
  friend class AstNodeFactory;

  ExpressionStatement(Expression* expression, int pos)
      : Statement(pos, kExpressionStatement), expression_(expression) {}

  Expression* expression_;
};


class JumpStatement : public Statement {
 protected:
  JumpStatement(int pos, NodeType type) : Statement(pos, type) {}
};


class ContinueStatement final : public JumpStatement {
 public:
  IterationStatement* target() const { return target_; }

 private:
  friend class AstNodeFactory;

  ContinueStatement(IterationStatement* target, int pos)
      : JumpStatement(pos, kContinueStatement), target_(target) {}

  IterationStatement* target_;
};


class BreakStatement final : public JumpStatement {
 public:
  BreakableStatement* target() const { return target_; }

 private:
  friend class AstNodeFactory;

  BreakStatement(BreakableStatement* target, int pos)
      : JumpStatement(pos, kBreakStatement), target_(target) {}

  BreakableStatement* target_;
};


class ReturnStatement final : public JumpStatement {
 public:
  enum Type { kNormal, kAsyncReturn };
  Expression* expression() const { return expression_; }

  Type type() const { return TypeField::decode(bit_field_); }
  bool is_async_return() const { return type() == kAsyncReturn; }

  int end_position() const { return end_position_; }

 private:
  friend class AstNodeFactory;

  ReturnStatement(Expression* expression, Type type, int pos, int end_position)
      : JumpStatement(pos, kReturnStatement),
        expression_(expression),
        end_position_(end_position) {
    bit_field_ |= TypeField::encode(type);
  }

  Expression* expression_;
  int end_position_;

  class TypeField
      : public BitField<Type, JumpStatement::kNextBitFieldIndex, 1> {};
};


class WithStatement final : public Statement {
 public:
  Scope* scope() { return scope_; }
  Expression* expression() const { return expression_; }
  Statement* statement() const { return statement_; }
  void set_statement(Statement* s) { statement_ = s; }

 private:
  friend class AstNodeFactory;

  WithStatement(Scope* scope, Expression* expression, Statement* statement,
                int pos)
      : Statement(pos, kWithStatement),
        scope_(scope),
        expression_(expression),
        statement_(statement) {}

  Scope* scope_;
  Expression* expression_;
  Statement* statement_;
};

class CaseClause final : public ZoneObject {
 public:
  bool is_default() const { return label_ == nullptr; }
  Expression* label() const {
    DCHECK(!is_default());
    return label_;
  }
  ZonePtrList<Statement>* statements() { return &statements_; }

 private:
  friend class AstNodeFactory;

  CaseClause(Zone* zone, Expression* label,
             const ScopedPtrList<Statement>& statements);

  Expression* label_;
  ZonePtrList<Statement> statements_;
};


class SwitchStatement final : public BreakableStatement {
 public:
  ZonePtrList<const AstRawString>* labels() const { return labels_; }

  Expression* tag() const { return tag_; }
  void set_tag(Expression* t) { tag_ = t; }

  ZonePtrList<CaseClause>* cases() { return &cases_; }

 private:
  friend class AstNodeFactory;

  SwitchStatement(Zone* zone, ZonePtrList<const AstRawString>* labels,
                  Expression* tag, int pos)
      : BreakableStatement(TARGET_FOR_ANONYMOUS, pos, kSwitchStatement),
        labels_(labels),
        tag_(tag),
        cases_(4, zone) {}

  ZonePtrList<const AstRawString>* labels_;
  Expression* tag_;
  ZonePtrList<CaseClause> cases_;
};


// If-statements always have non-null references to their then- and
// else-parts. When parsing if-statements with no explicit else-part,
// the parser implicitly creates an empty statement. Use the
// HasThenStatement() and HasElseStatement() functions to check if a
// given if-statement has a then- or an else-part containing code.
class IfStatement final : public Statement {
 public:
  bool HasThenStatement() const { return !then_statement_->IsEmptyStatement(); }
  bool HasElseStatement() const { return !else_statement_->IsEmptyStatement(); }

  Expression* condition() const { return condition_; }
  Statement* then_statement() const { return then_statement_; }
  Statement* else_statement() const { return else_statement_; }

  void set_then_statement(Statement* s) { then_statement_ = s; }
  void set_else_statement(Statement* s) { else_statement_ = s; }

 private:
  friend class AstNodeFactory;

  IfStatement(Expression* condition, Statement* then_statement,
              Statement* else_statement, int pos)
      : Statement(pos, kIfStatement),
        condition_(condition),
        then_statement_(then_statement),
        else_statement_(else_statement) {}

  Expression* condition_;
  Statement* then_statement_;
  Statement* else_statement_;
};


class TryStatement : public Statement {
 public:
  Block* try_block() const { return try_block_; }
  void set_try_block(Block* b) { try_block_ = b; }

 protected:
  TryStatement(Block* try_block, int pos, NodeType type)
      : Statement(pos, type), try_block_(try_block) {}

 private:
  Block* try_block_;
};


class TryCatchStatement final : public TryStatement {
 public:
  Scope* scope() { return scope_; }
  Block* catch_block() const { return catch_block_; }
  void set_catch_block(Block* b) { catch_block_ = b; }

  // Prediction of whether exceptions thrown into the handler for this try block
  // will be caught.
  //
  // BytecodeGenerator tracks the state of catch prediction, which can change
  // with each TryCatchStatement encountered. The tracked catch prediction is
  // later compiled into the code's handler table. The runtime uses this
  // information to implement a feature that notifies the debugger when an
  // uncaught exception is thrown, _before_ the exception propagates to the top.
  //
  // If this try/catch statement is meant to rethrow (HandlerTable::UNCAUGHT),
  // the catch prediction value is set to the same value as the surrounding
  // catch prediction.
  //
  // Since it's generally undecidable whether an exception will be caught, our
  // prediction is only an approximation.
  // ---------------------------------------------------------------------------
  inline HandlerTable::CatchPrediction GetCatchPrediction(
      HandlerTable::CatchPrediction outer_catch_prediction) const {
    if (catch_prediction_ == HandlerTable::UNCAUGHT) {
      return outer_catch_prediction;
    }
    return catch_prediction_;
  }

  // Indicates whether or not code should be generated to clear the pending
  // exception. The pending exception is cleared for cases where the exception
  // is not guaranteed to be rethrown, indicated by the value
  // HandlerTable::UNCAUGHT. If both the current and surrounding catch handler's
  // are predicted uncaught, the exception is not cleared.
  //
  // If this handler is not going to simply rethrow the exception, this method
  // indicates that the isolate's pending exception message should be cleared
  // before executing the catch_block.
  // In the normal use case, this flag is always on because the message object
  // is not needed anymore when entering the catch block and should not be
  // kept alive.
  // The use case where the flag is off is when the catch block is guaranteed
  // to rethrow the caught exception (using %ReThrow), which reuses the
  // pending message instead of generating a new one.
  // (When the catch block doesn't rethrow but is guaranteed to perform an
  // ordinary throw, not clearing the old message is safe but not very
  // useful.)
  inline bool ShouldClearPendingException(
      HandlerTable::CatchPrediction outer_catch_prediction) const {
    return catch_prediction_ != HandlerTable::UNCAUGHT ||
           outer_catch_prediction != HandlerTable::UNCAUGHT;
  }

 private:
  friend class AstNodeFactory;

  TryCatchStatement(Block* try_block, Scope* scope, Block* catch_block,
                    HandlerTable::CatchPrediction catch_prediction, int pos)
      : TryStatement(try_block, pos, kTryCatchStatement),
        scope_(scope),
        catch_block_(catch_block),
        catch_prediction_(catch_prediction) {}

  Scope* scope_;
  Block* catch_block_;
  HandlerTable::CatchPrediction catch_prediction_;
};


class TryFinallyStatement final : public TryStatement {
 public:
  Block* finally_block() const { return finally_block_; }
  void set_finally_block(Block* b) { finally_block_ = b; }

 private:
  friend class AstNodeFactory;

  TryFinallyStatement(Block* try_block, Block* finally_block, int pos)
      : TryStatement(try_block, pos, kTryFinallyStatement),
        finally_block_(finally_block) {}

  Block* finally_block_;
};


class DebuggerStatement final : public Statement {
 private:
  friend class AstNodeFactory;

  explicit DebuggerStatement(int pos) : Statement(pos, kDebuggerStatement) {}
};


class EmptyStatement final : public Statement {
 private:
  friend class AstNodeFactory;
  EmptyStatement() : Statement(kNoSourcePosition, kEmptyStatement) {}
};


// Delegates to another statement, which may be overwritten.
// This was introduced to implement ES2015 Annex B3.3 for conditionally making
// sloppy-mode block-scoped functions have a var binding, which is changed
// from one statement to another during parsing.
class SloppyBlockFunctionStatement final : public Statement {
 public:
  Statement* statement() const { return statement_; }
  void set_statement(Statement* statement) { statement_ = statement; }
  Scope* scope() const { return var_->scope(); }
  Variable* var() const { return var_; }
  Token::Value init() const { return TokenField::decode(bit_field_); }
  const AstRawString* name() const { return var_->raw_name(); }
  SloppyBlockFunctionStatement** next() { return &next_; }

 private:
  friend class AstNodeFactory;

  class TokenField
      : public BitField<Token::Value, Statement::kNextBitFieldIndex, 8> {};

  SloppyBlockFunctionStatement(int pos, Variable* var, Token::Value init,
                               Statement* statement)
      : Statement(pos, kSloppyBlockFunctionStatement),
        var_(var),
        statement_(statement),
        next_(nullptr) {
    bit_field_ = TokenField::update(bit_field_, init);
  }

  Variable* var_;
  Statement* statement_;
  SloppyBlockFunctionStatement* next_;
};


class Literal final : public Expression {
 public:
  enum Type {
    kSmi,
    kHeapNumber,
    kBigInt,
    kString,
    kSymbol,
    kBoolean,
    kUndefined,
    kNull,
    kTheHole,
  };

  Type type() const { return TypeField::decode(bit_field_); }

  // Returns true if literal represents a property name (i.e. cannot be parsed
  // as array indices).
  bool IsPropertyName() const;

  // Returns true if literal represents an array index.
  // Note, that in general the following statement is not true:
  //   key->IsPropertyName() != key->AsArrayIndex(...)
  // but for non-computed LiteralProperty properties the following is true:
  //   property->key()->IsPropertyName() != property->key()->AsArrayIndex(...)
  bool AsArrayIndex(uint32_t* index) const;

  const AstRawString* AsRawPropertyName() {
    DCHECK(IsPropertyName());
    return string_;
  }

  Smi AsSmiLiteral() const {
    DCHECK_EQ(kSmi, type());
    return Smi::FromInt(smi_);
  }

  // Returns true if literal represents a Number.
  bool IsNumber() const { return type() == kHeapNumber || type() == kSmi; }
  double AsNumber() const {
    DCHECK(IsNumber());
    switch (type()) {
      case kSmi:
        return smi_;
      case kHeapNumber:
        return number_;
      default:
        UNREACHABLE();
    }
  }

  AstBigInt AsBigInt() const {
    DCHECK_EQ(type(), kBigInt);
    return bigint_;
  }

  bool IsString() const { return type() == kString; }
  const AstRawString* AsRawString() {
    DCHECK_EQ(type(), kString);
    return string_;
  }

  AstSymbol AsSymbol() {
    DCHECK_EQ(type(), kSymbol);
    return symbol_;
  }

  V8_EXPORT_PRIVATE bool ToBooleanIsTrue() const;
  bool ToBooleanIsFalse() const { return !ToBooleanIsTrue(); }

  bool ToUint32(uint32_t* value) const;

  // Returns an appropriate Object representing this Literal, allocating
  // a heap object if needed.
  Handle<Object> BuildValue(Isolate* isolate) const;

  // Support for using Literal as a HashMap key. NOTE: Currently, this works
  // only for string and number literals!
  uint32_t Hash();
  static bool Match(void* literal1, void* literal2);

 private:
  friend class AstNodeFactory;

  class TypeField : public BitField<Type, Expression::kNextBitFieldIndex, 4> {};

  Literal(int smi, int position) : Expression(position, kLiteral), smi_(smi) {
    bit_field_ = TypeField::update(bit_field_, kSmi);
  }

  Literal(double number, int position)
      : Expression(position, kLiteral), number_(number) {
    bit_field_ = TypeField::update(bit_field_, kHeapNumber);
  }

  Literal(AstBigInt bigint, int position)
      : Expression(position, kLiteral), bigint_(bigint) {
    bit_field_ = TypeField::update(bit_field_, kBigInt);
  }

  Literal(const AstRawString* string, int position)
      : Expression(position, kLiteral), string_(string) {
    bit_field_ = TypeField::update(bit_field_, kString);
  }

  Literal(AstSymbol symbol, int position)
      : Expression(position, kLiteral), symbol_(symbol) {
    bit_field_ = TypeField::update(bit_field_, kSymbol);
  }

  Literal(bool boolean, int position)
      : Expression(position, kLiteral), boolean_(boolean) {
    bit_field_ = TypeField::update(bit_field_, kBoolean);
  }

  Literal(Type type, int position) : Expression(position, kLiteral) {
    DCHECK(type == kNull || type == kUndefined || type == kTheHole);
    bit_field_ = TypeField::update(bit_field_, type);
  }

  union {
    const AstRawString* string_;
    int smi_;
    double number_;
    AstSymbol symbol_;
    AstBigInt bigint_;
    bool boolean_;
  };
};

// Base class for literals that need space in the type feedback vector.
class MaterializedLiteral : public Expression {
 public:
  // A Materializedliteral is simple if the values consist of only
  // constants and simple object and array literals.
  bool IsSimple() const;

 protected:
  MaterializedLiteral(int pos, NodeType type) : Expression(pos, type) {}

  friend class CompileTimeValue;
  friend class ArrayLiteral;
  friend class ObjectLiteral;

  // Populate the depth field and any flags the literal has, returns the depth.
  int InitDepthAndFlags();

  bool NeedsInitialAllocationSite();

  // Populate the constant properties/elements fixed array.
  void BuildConstants(Isolate* isolate);

  // If the expression is a literal, return the literal value;
  // if the expression is a materialized literal and is_simple
  // then return an Array or Object Boilerplate Description
  // Otherwise, return undefined literal as the placeholder
  // in the object literal boilerplate.
  Handle<Object> GetBoilerplateValue(Expression* expression, Isolate* isolate);
};

// Node for capturing a regexp literal.
class RegExpLiteral final : public MaterializedLiteral {
 public:
  Handle<String> pattern() const { return pattern_->string(); }
  const AstRawString* raw_pattern() const { return pattern_; }
  int flags() const { return flags_; }

 private:
  friend class AstNodeFactory;

  RegExpLiteral(const AstRawString* pattern, int flags, int pos)
      : MaterializedLiteral(pos, kRegExpLiteral),
        flags_(flags),
        pattern_(pattern) {}

  int const flags_;
  const AstRawString* const pattern_;
};

// Base class for Array and Object literals, providing common code for handling
// nested subliterals.
class AggregateLiteral : public MaterializedLiteral {
 public:
  enum Flags {
    kNoFlags = 0,
    kIsShallow = 1,
    kDisableMementos = 1 << 1,
    kNeedsInitialAllocationSite = 1 << 2,
  };

  bool is_initialized() const { return 0 < depth_; }
  int depth() const {
    DCHECK(is_initialized());
    return depth_;
  }

  bool is_shallow() const { return depth() == 1; }
  bool needs_initial_allocation_site() const {
    return NeedsInitialAllocationSiteField::decode(bit_field_);
  }

  int ComputeFlags(bool disable_mementos = false) const {
    int flags = kNoFlags;
    if (is_shallow()) flags |= kIsShallow;
    if (disable_mementos) flags |= kDisableMementos;
    if (needs_initial_allocation_site()) flags |= kNeedsInitialAllocationSite;
    return flags;
  }

  // An AggregateLiteral is simple if the values consist of only
  // constants and simple object and array literals.
  bool is_simple() const { return IsSimpleField::decode(bit_field_); }

 private:
  int depth_ : 31;
  class NeedsInitialAllocationSiteField
      : public BitField<bool, MaterializedLiteral::kNextBitFieldIndex, 1> {};
  class IsSimpleField
      : public BitField<bool, NeedsInitialAllocationSiteField::kNext, 1> {};

 protected:
  friend class AstNodeFactory;
  AggregateLiteral(int pos, NodeType type)
      : MaterializedLiteral(pos, type), depth_(0) {
    bit_field_ |= NeedsInitialAllocationSiteField::encode(false) |
                  IsSimpleField::encode(false);
  }

  void set_is_simple(bool is_simple) {
    bit_field_ = IsSimpleField::update(bit_field_, is_simple);
  }

  void set_depth(int depth) {
    DCHECK(!is_initialized());
    depth_ = depth;
  }

  void set_needs_initial_allocation_site(bool required) {
    bit_field_ = NeedsInitialAllocationSiteField::update(bit_field_, required);
  }

  static const uint8_t kNextBitFieldIndex = IsSimpleField::kNext;
};

// Common supertype for ObjectLiteralProperty and ClassLiteralProperty
class LiteralProperty : public ZoneObject {
 public:
  Expression* key() const { return key_and_is_computed_name_.GetPointer(); }
  Expression* value() const { return value_; }

  bool is_computed_name() const {
    return key_and_is_computed_name_.GetPayload();
  }
  bool NeedsSetFunctionName() const;

 protected:
  LiteralProperty(Expression* key, Expression* value, bool is_computed_name)
      : key_and_is_computed_name_(key, is_computed_name), value_(value) {}

  PointerWithPayload<Expression, bool, 1> key_and_is_computed_name_;
  Expression* value_;
};

// Property is used for passing information
// about an object literal's properties from the parser
// to the code generator.
class ObjectLiteralProperty final : public LiteralProperty {
 public:
  enum Kind : uint8_t {
    CONSTANT,              // Property with constant value (compile time).
    COMPUTED,              // Property with computed value (execution time).
    MATERIALIZED_LITERAL,  // Property value is a materialized literal.
    GETTER,
    SETTER,     // Property is an accessor function.
    PROTOTYPE,  // Property is __proto__.
    SPREAD
  };

  Kind kind() const { return kind_; }

  bool IsCompileTimeValue() const;

  void set_emit_store(bool emit_store);
  bool emit_store() const;

  bool IsNullPrototype() const {
    return IsPrototype() && value()->IsNullLiteral();
  }
  bool IsPrototype() const { return kind() == PROTOTYPE; }

 private:
  friend class AstNodeFactory;

  ObjectLiteralProperty(Expression* key, Expression* value, Kind kind,
                        bool is_computed_name);
  ObjectLiteralProperty(AstValueFactory* ast_value_factory, Expression* key,
                        Expression* value, bool is_computed_name);

  Kind kind_;
  bool emit_store_;
};


// An object literal has a boilerplate object that is used
// for minimizing the work when constructing it at runtime.
class ObjectLiteral final : public AggregateLiteral {
 public:
  typedef ObjectLiteralProperty Property;

  Handle<ObjectBoilerplateDescription> boilerplate_description() const {
    DCHECK(!boilerplate_description_.is_null());
    return boilerplate_description_;
  }
  int properties_count() const { return boilerplate_properties_; }
  const ZonePtrList<Property>* properties() const { return &properties_; }
  bool has_elements() const { return HasElementsField::decode(bit_field_); }
  bool has_rest_property() const {
    return HasRestPropertyField::decode(bit_field_);
  }
  bool fast_elements() const { return FastElementsField::decode(bit_field_); }
  bool has_null_prototype() const {
    return HasNullPrototypeField::decode(bit_field_);
  }

  bool is_empty() const {
    DCHECK(is_initialized());
    return !has_elements() && properties_count() == 0 &&
           properties()->length() == 0;
  }

  bool IsEmptyObjectLiteral() const {
    return is_empty() && !has_null_prototype();
  }

  // Populate the depth field and flags, returns the depth.
  int InitDepthAndFlags();

  // Get the boilerplate description, populating it if necessary.
  Handle<ObjectBoilerplateDescription> GetOrBuildBoilerplateDescription(
      Isolate* isolate) {
    if (boilerplate_description_.is_null()) {
      BuildBoilerplateDescription(isolate);
    }
    return boilerplate_description();
  }

  // Populate the boilerplate description.
  void BuildBoilerplateDescription(Isolate* isolate);

  // Mark all computed expressions that are bound to a key that
  // is shadowed by a later occurrence of the same key. For the
  // marked expressions, no store code is emitted.
  void CalculateEmitStore(Zone* zone);

  // Determines whether the {CreateShallowObjectLiteratal} builtin can be used.
  bool IsFastCloningSupported() const;

  // Assemble bitfield of flags for the CreateObjectLiteral helper.
  int ComputeFlags(bool disable_mementos = false) const {
    int flags = AggregateLiteral::ComputeFlags(disable_mementos);
    if (fast_elements()) flags |= kFastElements;
    if (has_null_prototype()) flags |= kHasNullPrototype;
    return flags;
  }

  int EncodeLiteralType() {
    int flags = kNoFlags;
    if (fast_elements()) flags |= kFastElements;
    if (has_null_prototype()) flags |= kHasNullPrototype;
    return flags;
  }

  enum Flags {
    kFastElements = 1 << 3,
    kHasNullPrototype = 1 << 4,
  };
  STATIC_ASSERT(
      static_cast<int>(AggregateLiteral::kNeedsInitialAllocationSite) <
      static_cast<int>(kFastElements));

  struct Accessors: public ZoneObject {
    Accessors() : getter(nullptr), setter(nullptr) {}
    ObjectLiteralProperty* getter;
    ObjectLiteralProperty* setter;
  };

 private:
  friend class AstNodeFactory;

  ObjectLiteral(Zone* zone, const ScopedPtrList<Property>& properties,
                uint32_t boilerplate_properties, int pos,
                bool has_rest_property)
      : AggregateLiteral(pos, kObjectLiteral),
        boilerplate_properties_(boilerplate_properties),
        properties_(0, nullptr) {
    bit_field_ |= HasElementsField::encode(false) |
                  HasRestPropertyField::encode(has_rest_property) |
                  FastElementsField::encode(false) |
                  HasNullPrototypeField::encode(false);
    properties.CopyTo(&properties_, zone);
  }

  void InitFlagsForPendingNullPrototype(int i);

  void set_has_elements(bool has_elements) {
    bit_field_ = HasElementsField::update(bit_field_, has_elements);
  }
  void set_fast_elements(bool fast_elements) {
    bit_field_ = FastElementsField::update(bit_field_, fast_elements);
  }
  void set_has_null_protoype(bool has_null_prototype) {
    bit_field_ = HasNullPrototypeField::update(bit_field_, has_null_prototype);
  }

  uint32_t boilerplate_properties_;
  Handle<ObjectBoilerplateDescription> boilerplate_description_;
  ZoneList<Property*> properties_;

  class HasElementsField
      : public BitField<bool, AggregateLiteral::kNextBitFieldIndex, 1> {};
  class HasRestPropertyField
      : public BitField<bool, HasElementsField::kNext, 1> {};
  class FastElementsField
      : public BitField<bool, HasRestPropertyField::kNext, 1> {};
  class HasNullPrototypeField
      : public BitField<bool, FastElementsField::kNext, 1> {};
};


// A map from property names to getter/setter pairs allocated in the zone.
class AccessorTable
    : public base::TemplateHashMap<Literal, ObjectLiteral::Accessors,
                                   bool (*)(void*, void*),
                                   ZoneAllocationPolicy> {
 public:
  explicit AccessorTable(Zone* zone)
      : base::TemplateHashMap<Literal, ObjectLiteral::Accessors,
                              bool (*)(void*, void*), ZoneAllocationPolicy>(
            Literal::Match, ZoneAllocationPolicy(zone)),
        zone_(zone) {}

  Iterator lookup(Literal* literal) {
    Iterator it = find(literal, true, ZoneAllocationPolicy(zone_));
    if (it->second == nullptr) {
      it->second = new (zone_) ObjectLiteral::Accessors();
    }
    return it;
  }

 private:
  Zone* zone_;
};


// An array literal has a literals object that is used
// for minimizing the work when constructing it at runtime.
class ArrayLiteral final : public AggregateLiteral {
 public:
  Handle<ArrayBoilerplateDescription> boilerplate_description() const {
    return boilerplate_description_;
  }

  const ZonePtrList<Expression>* values() const { return &values_; }

  int first_spread_index() const { return first_spread_index_; }

  // Populate the depth field and flags, returns the depth.
  int InitDepthAndFlags();

  // Get the boilerplate description, populating it if necessary.
  Handle<ArrayBoilerplateDescription> GetOrBuildBoilerplateDescription(
      Isolate* isolate) {
    if (boilerplate_description_.is_null()) {
      BuildBoilerplateDescription(isolate);
    }
    return boilerplate_description();
  }

  // Populate the boilerplate description.
  void BuildBoilerplateDescription(Isolate* isolate);

  // Determines whether the {CreateShallowArrayLiteral} builtin can be used.
  bool IsFastCloningSupported() const;

  // Assemble bitfield of flags for the CreateArrayLiteral helper.
  int ComputeFlags(bool disable_mementos = false) const {
    return AggregateLiteral::ComputeFlags(disable_mementos);
  }

 private:
  friend class AstNodeFactory;

  ArrayLiteral(Zone* zone, const ScopedPtrList<Expression>& values,
               int first_spread_index, int pos)
      : AggregateLiteral(pos, kArrayLiteral),
        first_spread_index_(first_spread_index),
        values_(0, nullptr) {
    values.CopyTo(&values_, zone);
  }

  int first_spread_index_;
  Handle<ArrayBoilerplateDescription> boilerplate_description_;
  ZonePtrList<Expression> values_;
};

enum class HoleCheckMode { kRequired, kElided };

class ThisExpression final : public Expression {
 private:
  friend class AstNodeFactory;
  ThisExpression() : Expression(kNoSourcePosition, kThisExpression) {}
};

class VariableProxy final : public Expression {
 public:
  bool IsValidReferenceExpression() const { return !is_new_target(); }

  Handle<String> name() const { return raw_name()->string(); }
  const AstRawString* raw_name() const {
    return is_resolved() ? var_->raw_name() : raw_name_;
  }

  Variable* var() const {
    DCHECK(is_resolved());
    return var_;
  }
  void set_var(Variable* v) {
    DCHECK(!is_resolved());
    DCHECK_NOT_NULL(v);
    var_ = v;
  }

  Scanner::Location location() {
    return Scanner::Location(position(), position() + raw_name()->length());
  }

  bool is_assigned() const { return IsAssignedField::decode(bit_field_); }
  void set_is_assigned() {
    bit_field_ = IsAssignedField::update(bit_field_, true);
    if (is_resolved()) {
      var()->set_maybe_assigned();
    }
  }

  bool is_resolved() const { return IsResolvedField::decode(bit_field_); }
  void set_is_resolved() {
    bit_field_ = IsResolvedField::update(bit_field_, true);
  }

  bool is_new_target() const { return IsNewTargetField::decode(bit_field_); }
  void set_is_new_target() {
    bit_field_ = IsNewTargetField::update(bit_field_, true);
  }

  HoleCheckMode hole_check_mode() const {
    HoleCheckMode mode = HoleCheckModeField::decode(bit_field_);
    DCHECK_IMPLIES(mode == HoleCheckMode::kRequired,
                   var()->binding_needs_init() ||
                       var()->local_if_not_shadowed()->binding_needs_init());
    return mode;
  }
  void set_needs_hole_check() {
    bit_field_ =
        HoleCheckModeField::update(bit_field_, HoleCheckMode::kRequired);
  }

  bool IsPrivateName() const {
    return raw_name()->length() > 0 && raw_name()->FirstCharacter() == '#';
  }

  // Bind this proxy to the variable var.
  void BindTo(Variable* var);

  V8_INLINE VariableProxy* next_unresolved() { return next_unresolved_; }
  V8_INLINE bool is_removed_from_unresolved() const {
    return IsRemovedFromUnresolvedField::decode(bit_field_);
  }

  void mark_removed_from_unresolved() {
    bit_field_ = IsRemovedFromUnresolvedField::update(bit_field_, true);
  }

  // Provides filtered access to the unresolved variable proxy threaded list.
  struct UnresolvedNext {
    static VariableProxy** filter(VariableProxy** t) {
      VariableProxy** n = t;
      // Skip over possibly removed values.
      while (*n != nullptr && (*n)->is_removed_from_unresolved()) {
        n = (*n)->next();
      }
      return n;
    }

    static VariableProxy** start(VariableProxy** head) { return filter(head); }

    static VariableProxy** next(VariableProxy* t) { return filter(t->next()); }
  };

 private:
  friend class AstNodeFactory;

  VariableProxy(Variable* var, int start_position);

  VariableProxy(const AstRawString* name, VariableKind variable_kind,
                int start_position)
      : Expression(start_position, kVariableProxy),
        raw_name_(name),
        next_unresolved_(nullptr) {
    DCHECK_NE(THIS_VARIABLE, variable_kind);
    bit_field_ |= IsAssignedField::encode(false) |
                  IsResolvedField::encode(false) |
                  IsRemovedFromUnresolvedField::encode(false) |
                  HoleCheckModeField::encode(HoleCheckMode::kElided);
  }

  explicit VariableProxy(const VariableProxy* copy_from);

  class IsAssignedField
      : public BitField<bool, Expression::kNextBitFieldIndex, 1> {};
  class IsResolvedField : public BitField<bool, IsAssignedField::kNext, 1> {};
  class IsRemovedFromUnresolvedField
      : public BitField<bool, IsResolvedField::kNext, 1> {};
  class IsNewTargetField
      : public BitField<bool, IsRemovedFromUnresolvedField::kNext, 1> {};
  class HoleCheckModeField
      : public BitField<HoleCheckMode, IsNewTargetField::kNext, 1> {};

  union {
    const AstRawString* raw_name_;  // if !is_resolved_
    Variable* var_;                 // if is_resolved_
  };

  V8_INLINE VariableProxy** next() { return &next_unresolved_; }
  VariableProxy* next_unresolved_;

  friend base::ThreadedListTraits<VariableProxy>;
};

// Assignments to a property will use one of several types of property access.
// Otherwise, the assignment is to a non-property (a global, a local slot, a
// parameter slot, or a destructuring pattern).
enum AssignType {
  NON_PROPERTY,
  NAMED_PROPERTY,
  KEYED_PROPERTY,
  NAMED_SUPER_PROPERTY,
  KEYED_SUPER_PROPERTY
};

class Property final : public Expression {
 public:
  bool IsValidReferenceExpression() const { return true; }

  Expression* obj() const { return obj_; }
  Expression* key() const { return key_; }

  bool IsSuperAccess() { return obj()->IsSuperPropertyReference(); }

  // Returns the properties assign type.
  static AssignType GetAssignType(Property* property) {
    if (property == nullptr) return NON_PROPERTY;
    bool super_access = property->IsSuperAccess();
    return (property->key()->IsPropertyName())
               ? (super_access ? NAMED_SUPER_PROPERTY : NAMED_PROPERTY)
               : (super_access ? KEYED_SUPER_PROPERTY : KEYED_PROPERTY);
  }

 private:
  friend class AstNodeFactory;

  Property(Expression* obj, Expression* key, int pos)
      : Expression(pos, kProperty), obj_(obj), key_(key) {
  }

  Expression* obj_;
  Expression* key_;
};

// ResolvedProperty pairs a receiver field with a value field. It allows Call
// to support arbitrary receivers while still taking advantage of TypeFeedback.
class ResolvedProperty final : public Expression {
 public:
  VariableProxy* object() const { return object_; }
  VariableProxy* property() const { return property_; }

  void set_object(VariableProxy* e) { object_ = e; }
  void set_property(VariableProxy* e) { property_ = e; }

 private:
  friend class AstNodeFactory;

  ResolvedProperty(VariableProxy* obj, VariableProxy* property, int pos)
      : Expression(pos, kResolvedProperty), object_(obj), property_(property) {}

  VariableProxy* object_;
  VariableProxy* property_;
};

class Call final : public Expression {
 public:
  Expression* expression() const { return expression_; }
  const ZonePtrList<Expression>* arguments() const { return &arguments_; }

  bool is_possibly_eval() const {
    return IsPossiblyEvalField::decode(bit_field_);
  }

  bool is_tagged_template() const {
    return IsTaggedTemplateField::decode(bit_field_);
  }

  bool only_last_arg_is_spread() {
    return !arguments_.is_empty() && arguments_.last()->IsSpread();
  }

  enum CallType {
    GLOBAL_CALL,
    WITH_CALL,
    NAMED_PROPERTY_CALL,
    KEYED_PROPERTY_CALL,
    NAMED_SUPER_PROPERTY_CALL,
    KEYED_SUPER_PROPERTY_CALL,
    SUPER_CALL,
    RESOLVED_PROPERTY_CALL,
    OTHER_CALL
  };

  enum PossiblyEval {
    IS_POSSIBLY_EVAL,
    NOT_EVAL,
  };

  // Helpers to determine how to handle the call.
  CallType GetCallType() const;

  enum class TaggedTemplateTag { kTrue };

 private:
  friend class AstNodeFactory;

  Call(Zone* zone, Expression* expression,
       const ScopedPtrList<Expression>& arguments, int pos,
       PossiblyEval possibly_eval)
      : Expression(pos, kCall),
        expression_(expression),
        arguments_(0, nullptr) {
    bit_field_ |=
        IsPossiblyEvalField::encode(possibly_eval == IS_POSSIBLY_EVAL) |
        IsTaggedTemplateField::encode(false);
    arguments.CopyTo(&arguments_, zone);
  }

  Call(Zone* zone, Expression* expression,
       const ScopedPtrList<Expression>& arguments, int pos,
       TaggedTemplateTag tag)
      : Expression(pos, kCall),
        expression_(expression),
        arguments_(0, nullptr) {
    bit_field_ |= IsPossiblyEvalField::encode(false) |
                  IsTaggedTemplateField::encode(true);
    arguments.CopyTo(&arguments_, zone);
  }

  class IsPossiblyEvalField
      : public BitField<bool, Expression::kNextBitFieldIndex, 1> {};
  class IsTaggedTemplateField
      : public BitField<bool, IsPossiblyEvalField::kNext, 1> {};

  Expression* expression_;
  ZonePtrList<Expression> arguments_;
};


class CallNew final : public Expression {
 public:
  Expression* expression() const { return expression_; }
  const ZonePtrList<Expression>* arguments() const { return &arguments_; }

  bool only_last_arg_is_spread() {
    return !arguments_.is_empty() && arguments_.last()->IsSpread();
  }

 private:
  friend class AstNodeFactory;

  CallNew(Zone* zone, Expression* expression,
          const ScopedPtrList<Expression>& arguments, int pos)
      : Expression(pos, kCallNew),
        expression_(expression),
        arguments_(0, nullptr) {
    arguments.CopyTo(&arguments_, zone);
  }

  Expression* expression_;
  ZonePtrList<Expression> arguments_;
};

// The CallRuntime class does not represent any official JavaScript
// language construct. Instead it is used to call a C or JS function
// with a set of arguments. This is used from the builtins that are
// implemented in JavaScript.
class CallRuntime final : public Expression {
 public:
  const ZonePtrList<Expression>* arguments() const { return &arguments_; }
  bool is_jsruntime() const { return function_ == nullptr; }

  int context_index() const {
    DCHECK(is_jsruntime());
    return context_index_;
  }
  const Runtime::Function* function() const {
    DCHECK(!is_jsruntime());
    return function_;
  }

  const char* debug_name();

 private:
  friend class AstNodeFactory;

  CallRuntime(Zone* zone, const Runtime::Function* function,
              const ScopedPtrList<Expression>& arguments, int pos)
      : Expression(pos, kCallRuntime),
        function_(function),
        arguments_(0, nullptr) {
    arguments.CopyTo(&arguments_, zone);
  }
  CallRuntime(Zone* zone, int context_index,
              const ScopedPtrList<Expression>& arguments, int pos)
      : Expression(pos, kCallRuntime),
        context_index_(context_index),
        function_(nullptr),
        arguments_(0, nullptr) {
    arguments.CopyTo(&arguments_, zone);
  }

  int context_index_;
  const Runtime::Function* function_;
  ZonePtrList<Expression> arguments_;
};


class UnaryOperation final : public Expression {
 public:
  Token::Value op() const { return OperatorField::decode(bit_field_); }
  Expression* expression() const { return expression_; }

 private:
  friend class AstNodeFactory;

  UnaryOperation(Token::Value op, Expression* expression, int pos)
      : Expression(pos, kUnaryOperation), expression_(expression) {
    bit_field_ |= OperatorField::encode(op);
    DCHECK(Token::IsUnaryOp(op));
  }

  Expression* expression_;

  class OperatorField
      : public BitField<Token::Value, Expression::kNextBitFieldIndex, 7> {};
};


class BinaryOperation final : public Expression {
 public:
  Token::Value op() const { return OperatorField::decode(bit_field_); }
  Expression* left() const { return left_; }
  Expression* right() const { return right_; }

  // Returns true if one side is a Smi literal, returning the other side's
  // sub-expression in |subexpr| and the literal Smi in |literal|.
  bool IsSmiLiteralOperation(Expression** subexpr, Smi* literal);

 private:
  friend class AstNodeFactory;

  BinaryOperation(Token::Value op, Expression* left, Expression* right, int pos)
      : Expression(pos, kBinaryOperation), left_(left), right_(right) {
    bit_field_ |= OperatorField::encode(op);
    DCHECK(Token::IsBinaryOp(op));
  }

  Expression* left_;
  Expression* right_;

  class OperatorField
      : public BitField<Token::Value, Expression::kNextBitFieldIndex, 7> {};
};

class NaryOperation final : public Expression {
 public:
  Token::Value op() const { return OperatorField::decode(bit_field_); }
  Expression* first() const { return first_; }
  Expression* subsequent(size_t index) const {
    return subsequent_[index].expression;
  }

  size_t subsequent_length() const { return subsequent_.size(); }
  int subsequent_op_position(size_t index) const {
    return subsequent_[index].op_position;
  }

  void AddSubsequent(Expression* expr, int pos) {
    subsequent_.emplace_back(expr, pos);
  }

 private:
  friend class AstNodeFactory;

  NaryOperation(Zone* zone, Token::Value op, Expression* first,
                size_t initial_subsequent_size)
      : Expression(first->position(), kNaryOperation),
        first_(first),
        subsequent_(zone) {
    bit_field_ |= OperatorField::encode(op);
    DCHECK(Token::IsBinaryOp(op));
    DCHECK_NE(op, Token::EXP);
    subsequent_.reserve(initial_subsequent_size);
  }

  // Nary operations store the first (lhs) child expression inline, and the
  // child expressions (rhs of each op) are stored out-of-line, along with
  // their operation's position. Note that the Nary operation expression's
  // position has no meaning.
  //
  // So an nary add:
  //
  //    expr + expr + expr + ...
  //
  // is stored as:
  //
  //    (expr) [(+ expr), (+ expr), ...]
  //    '-.--' '-----------.-----------'
  //    first    subsequent entry list

  Expression* first_;

  struct NaryOperationEntry {
    Expression* expression;
    int op_position;
    NaryOperationEntry(Expression* e, int pos)
        : expression(e), op_position(pos) {}
  };
  ZoneVector<NaryOperationEntry> subsequent_;

  class OperatorField
      : public BitField<Token::Value, Expression::kNextBitFieldIndex, 7> {};
};

class CountOperation final : public Expression {
 public:
  bool is_prefix() const { return IsPrefixField::decode(bit_field_); }
  bool is_postfix() const { return !is_prefix(); }

  Token::Value op() const { return TokenField::decode(bit_field_); }

  Expression* expression() const { return expression_; }

 private:
  friend class AstNodeFactory;

  CountOperation(Token::Value op, bool is_prefix, Expression* expr, int pos)
      : Expression(pos, kCountOperation), expression_(expr) {
    bit_field_ |= IsPrefixField::encode(is_prefix) | TokenField::encode(op);
  }

  class IsPrefixField
      : public BitField<bool, Expression::kNextBitFieldIndex, 1> {};
  class TokenField : public BitField<Token::Value, IsPrefixField::kNext, 7> {};

  Expression* expression_;
};


class CompareOperation final : public Expression {
 public:
  Token::Value op() const { return OperatorField::decode(bit_field_); }
  Expression* left() const { return left_; }
  Expression* right() const { return right_; }

  // Match special cases.
  bool IsLiteralCompareTypeof(Expression** expr, Literal** literal);
  bool IsLiteralCompareUndefined(Expression** expr);
  bool IsLiteralCompareNull(Expression** expr);

 private:
  friend class AstNodeFactory;

  CompareOperation(Token::Value op, Expression* left, Expression* right,
                   int pos)
      : Expression(pos, kCompareOperation), left_(left), right_(right) {
    bit_field_ |= OperatorField::encode(op);
    DCHECK(Token::IsCompareOp(op));
  }

  Expression* left_;
  Expression* right_;

  class OperatorField
      : public BitField<Token::Value, Expression::kNextBitFieldIndex, 7> {};
};


class Spread final : public Expression {
 public:
  Expression* expression() const { return expression_; }

  int expression_position() const { return expr_pos_; }

 private:
  friend class AstNodeFactory;

  Spread(Expression* expression, int pos, int expr_pos)
      : Expression(pos, kSpread),
        expr_pos_(expr_pos),
        expression_(expression) {}

  int expr_pos_;
  Expression* expression_;
};

// The StoreInArrayLiteral node corresponds to the StaInArrayLiteral bytecode.
// It is used in the rewriting of destructuring assignments that contain an
// array rest pattern.
class StoreInArrayLiteral final : public Expression {
 public:
  Expression* array() const { return array_; }
  Expression* index() const { return index_; }
  Expression* value() const { return value_; }

 private:
  friend class AstNodeFactory;

  StoreInArrayLiteral(Expression* array, Expression* index, Expression* value,
                      int position)
      : Expression(position, kStoreInArrayLiteral),
        array_(array),
        index_(index),
        value_(value) {}

  Expression* array_;
  Expression* index_;
  Expression* value_;
};

class Conditional final : public Expression {
 public:
  Expression* condition() const { return condition_; }
  Expression* then_expression() const { return then_expression_; }
  Expression* else_expression() const { return else_expression_; }

 private:
  friend class AstNodeFactory;

  Conditional(Expression* condition, Expression* then_expression,
              Expression* else_expression, int position)
      : Expression(position, kConditional),
        condition_(condition),
        then_expression_(then_expression),
        else_expression_(else_expression) {}

  Expression* condition_;
  Expression* then_expression_;
  Expression* else_expression_;
};

class Assignment : public Expression {
 public:
  Token::Value op() const { return TokenField::decode(bit_field_); }
  Expression* target() const { return target_; }
  Expression* value() const { return value_; }

  // The assignment was generated as part of block-scoped sloppy-mode
  // function hoisting, see
  // ES#sec-block-level-function-declarations-web-legacy-compatibility-semantics
  LookupHoistingMode lookup_hoisting_mode() const {
    return static_cast<LookupHoistingMode>(
        LookupHoistingModeField::decode(bit_field_));
  }
  void set_lookup_hoisting_mode(LookupHoistingMode mode) {
    bit_field_ =
        LookupHoistingModeField::update(bit_field_, static_cast<bool>(mode));
  }

 protected:
  Assignment(NodeType type, Token::Value op, Expression* target,
             Expression* value, int pos);

 private:
  friend class AstNodeFactory;

  class TokenField
      : public BitField<Token::Value, Expression::kNextBitFieldIndex, 7> {};
  class LookupHoistingModeField : public BitField<bool, TokenField::kNext, 1> {
  };

  Expression* target_;
  Expression* value_;
};

class CompoundAssignment final : public Assignment {
 public:
  BinaryOperation* binary_operation() const { return binary_operation_; }

 private:
  friend class AstNodeFactory;

  CompoundAssignment(Token::Value op, Expression* target, Expression* value,
                     int pos, BinaryOperation* binary_operation)
      : Assignment(kCompoundAssignment, op, target, value, pos),
        binary_operation_(binary_operation) {}

  BinaryOperation* binary_operation_;
};

// There are several types of Suspend node:
//
// Yield
// YieldStar
// Await
//
// Our Yield is different from the JS yield in that it "returns" its argument as
// is, without wrapping it in an iterator result object.  Such wrapping, if
// desired, must be done beforehand (see the parser).
class Suspend : public Expression {
 public:
  // With {kNoControl}, the {Suspend} behaves like yield, except that it never
  // throws and never causes the current generator to return. This is used to
  // desugar yield*.
  // TODO(caitp): remove once yield* desugaring for async generators is handled
  // in BytecodeGenerator.
  enum OnAbruptResume { kOnExceptionThrow, kNoControl };

  Expression* expression() const { return expression_; }
  OnAbruptResume on_abrupt_resume() const {
    return OnAbruptResumeField::decode(bit_field_);
  }

 private:
  friend class AstNodeFactory;
  friend class Yield;
  friend class YieldStar;
  friend class Await;

  Suspend(NodeType node_type, Expression* expression, int pos,
          OnAbruptResume on_abrupt_resume)
      : Expression(pos, node_type), expression_(expression) {
    bit_field_ |= OnAbruptResumeField::encode(on_abrupt_resume);
  }

  Expression* expression_;

  class OnAbruptResumeField
      : public BitField<OnAbruptResume, Expression::kNextBitFieldIndex, 1> {};
};

class Yield final : public Suspend {
 private:
  friend class AstNodeFactory;
  Yield(Expression* expression, int pos, OnAbruptResume on_abrupt_resume)
      : Suspend(kYield, expression, pos, on_abrupt_resume) {}
};

class YieldStar final : public Suspend {
 private:
  friend class AstNodeFactory;
  YieldStar(Expression* expression, int pos)
      : Suspend(kYieldStar, expression, pos,
                Suspend::OnAbruptResume::kNoControl) {}
};

class Await final : public Suspend {
 private:
  friend class AstNodeFactory;

  Await(Expression* expression, int pos)
      : Suspend(kAwait, expression, pos, Suspend::kOnExceptionThrow) {}
};

class Throw final : public Expression {
 public:
  Expression* exception() const { return exception_; }

 private:
  friend class AstNodeFactory;

  Throw(Expression* exception, int pos)
      : Expression(pos, kThrow), exception_(exception) {}

  Expression* exception_;
};


class FunctionLiteral final : public Expression {
 public:
  enum FunctionType {
    kAnonymousExpression,
    kNamedExpression,
    kDeclaration,
    kAccessorOrMethod,
    kWrapped,
  };

  enum ParameterFlag : uint8_t {
    kNoDuplicateParameters,
    kHasDuplicateParameters
  };
  enum EagerCompileHint : uint8_t { kShouldEagerCompile, kShouldLazyCompile };

  // Empty handle means that the function does not have a shared name (i.e.
  // the name will be set dynamically after creation of the function closure).
  MaybeHandle<String> name() const {
    return raw_name_ ? raw_name_->string() : MaybeHandle<String>();
  }
  Handle<String> name(Isolate* isolate) const;
  bool has_shared_name() const { return raw_name_ != nullptr; }
  const AstConsString* raw_name() const { return raw_name_; }
  void set_raw_name(const AstConsString* name) { raw_name_ = name; }
  DeclarationScope* scope() const { return scope_; }
  ZonePtrList<Statement>* body() { return &body_; }
  void set_function_token_position(int pos) { function_token_position_ = pos; }
  int function_token_position() const { return function_token_position_; }
  int start_position() const;
  int end_position() const;
  bool is_declaration() const { return function_type() == kDeclaration; }
  bool is_named_expression() const {
    return function_type() == kNamedExpression;
  }
  bool is_anonymous_expression() const {
    return function_type() == kAnonymousExpression;
  }

  void mark_as_oneshot_iife() {
    bit_field_ = OneshotIIFEBit::update(bit_field_, true);
  }
  bool is_oneshot_iife() const { return OneshotIIFEBit::decode(bit_field_); }
  bool is_toplevel() const {
    return function_literal_id() == kFunctionLiteralIdTopLevel;
  }
  bool is_wrapped() const { return function_type() == kWrapped; }
  V8_EXPORT_PRIVATE LanguageMode language_mode() const;

  static bool NeedsHomeObject(Expression* expr);

  void add_expected_properties(int number_properties) {
    expected_property_count_ += number_properties;
  }
  int expected_property_count() { return expected_property_count_; }
  int parameter_count() { return parameter_count_; }
  int function_length() { return function_length_; }

  bool AllowsLazyCompilation();

  bool CanSuspend() {
    if (suspend_count() > 0) {
      DCHECK(IsResumableFunction(kind()));
      return true;
    }
    return false;
  }

  // We can safely skip the arguments adaptor frame setup even
  // in case of arguments mismatches for strict mode functions,
  // as long as there's
  //
  //   1. no use of the arguments object (either explicitly or
  //      potentially implicitly via a direct eval() call), and
  //   2. rest parameters aren't being used in the function.
  //
  // See http://bit.ly/v8-faster-calls-with-arguments-mismatch
  // for the details here (https://crbug.com/v8/8895).
  bool SafeToSkipArgumentsAdaptor() const;

  // Returns either name or inferred name as a cstring.
  std::unique_ptr<char[]> GetDebugName() const;

  Handle<String> inferred_name() const {
    if (!inferred_name_.is_null()) {
      DCHECK_NULL(raw_inferred_name_);
      return inferred_name_;
    }
    if (raw_inferred_name_ != nullptr) {
      return raw_inferred_name_->string();
    }
    UNREACHABLE();
  }
  const AstConsString* raw_inferred_name() { return raw_inferred_name_; }

  // Only one of {set_inferred_name, set_raw_inferred_name} should be called.
  void set_inferred_name(Handle<String> inferred_name);
  void set_raw_inferred_name(const AstConsString* raw_inferred_name);

  bool pretenure() const { return Pretenure::decode(bit_field_); }
  void set_pretenure() { bit_field_ = Pretenure::update(bit_field_, true); }

  bool has_duplicate_parameters() const {
    // Not valid for lazy functions.
    DCHECK(ShouldEagerCompile());
    return HasDuplicateParameters::decode(bit_field_);
  }

  // This is used as a heuristic on when to eagerly compile a function
  // literal. We consider the following constructs as hints that the
  // function will be called immediately:
  // - (function() { ... })();
  // - var x = function() { ... }();
  V8_EXPORT_PRIVATE bool ShouldEagerCompile() const;
  V8_EXPORT_PRIVATE void SetShouldEagerCompile();

  FunctionType function_type() const {
    return FunctionTypeBits::decode(bit_field_);
  }
  FunctionKind kind() const;

  bool dont_optimize() {
    return dont_optimize_reason() != BailoutReason::kNoReason;
  }
  BailoutReason dont_optimize_reason() {
    return DontOptimizeReasonField::decode(bit_field_);
  }
  void set_dont_optimize_reason(BailoutReason reason) {
    bit_field_ = DontOptimizeReasonField::update(bit_field_, reason);
  }

  bool IsAnonymousFunctionDefinition() const {
    return is_anonymous_expression();
  }

  int suspend_count() { return suspend_count_; }
  void set_suspend_count(int suspend_count) { suspend_count_ = suspend_count; }

  int return_position() {
    return std::max(
        start_position(),
        end_position() - (HasBracesField::decode(bit_field_) ? 1 : 0));
  }

  int function_literal_id() const { return function_literal_id_; }
  void set_function_literal_id(int function_literal_id) {
    function_literal_id_ = function_literal_id;
  }

  void set_requires_instance_members_initializer(bool value) {
    bit_field_ = RequiresInstanceMembersInitializer::update(bit_field_, value);
  }
  bool requires_instance_members_initializer() const {
    return RequiresInstanceMembersInitializer::decode(bit_field_);
  }

  ProducedPreparseData* produced_preparse_data() const {
    return produced_preparse_data_;
  }

 private:
  friend class AstNodeFactory;

  FunctionLiteral(Zone* zone, const AstRawString* name,
                  AstValueFactory* ast_value_factory, DeclarationScope* scope,
                  const ScopedPtrList<Statement>& body,
                  int expected_property_count, int parameter_count,
                  int function_length, FunctionType function_type,
                  ParameterFlag has_duplicate_parameters,
                  EagerCompileHint eager_compile_hint, int position,
                  bool has_braces, int function_literal_id,
                  ProducedPreparseData* produced_preparse_data = nullptr)
      : Expression(position, kFunctionLiteral),
        expected_property_count_(expected_property_count),
        parameter_count_(parameter_count),
        function_length_(function_length),
        function_token_position_(kNoSourcePosition),
        suspend_count_(0),
        function_literal_id_(function_literal_id),
        raw_name_(name ? ast_value_factory->NewConsString(name) : nullptr),
        scope_(scope),
        body_(0, nullptr),
        raw_inferred_name_(ast_value_factory->empty_cons_string()),
        produced_preparse_data_(produced_preparse_data) {
    bit_field_ |=
        FunctionTypeBits::encode(function_type) | Pretenure::encode(false) |
        HasDuplicateParameters::encode(has_duplicate_parameters ==
                                       kHasDuplicateParameters) |
        DontOptimizeReasonField::encode(BailoutReason::kNoReason) |
        RequiresInstanceMembersInitializer::encode(false) |
        HasBracesField::encode(has_braces) | OneshotIIFEBit::encode(false);
    if (eager_compile_hint == kShouldEagerCompile) SetShouldEagerCompile();
    body.CopyTo(&body_, zone);
  }

  class FunctionTypeBits
      : public BitField<FunctionType, Expression::kNextBitFieldIndex, 3> {};
  class Pretenure : public BitField<bool, FunctionTypeBits::kNext, 1> {};
  class HasDuplicateParameters : public BitField<bool, Pretenure::kNext, 1> {};
  class DontOptimizeReasonField
      : public BitField<BailoutReason, HasDuplicateParameters::kNext, 8> {};
  class RequiresInstanceMembersInitializer
      : public BitField<bool, DontOptimizeReasonField::kNext, 1> {};
  class HasBracesField
      : public BitField<bool, RequiresInstanceMembersInitializer::kNext, 1> {};
  class OneshotIIFEBit : public BitField<bool, HasBracesField::kNext, 1> {};

  // expected_property_count_ is the sum of instance fields and properties.
  // It can vary depending on whether a function is lazily or eagerly parsed.
  int expected_property_count_;
  int parameter_count_;
  int function_length_;
  int function_token_position_;
  int suspend_count_;
  int function_literal_id_;

  const AstConsString* raw_name_;
  DeclarationScope* scope_;
  ZonePtrList<Statement> body_;
  const AstConsString* raw_inferred_name_;
  Handle<String> inferred_name_;
  ProducedPreparseData* produced_preparse_data_;
};

// Property is used for passing information
// about a class literal's properties from the parser to the code generator.
class ClassLiteralProperty final : public LiteralProperty {
 public:
  enum Kind : uint8_t { METHOD, GETTER, SETTER, FIELD };

  Kind kind() const { return kind_; }

  bool is_static() const { return is_static_; }

  bool is_private() const { return is_private_; }

  void set_computed_name_var(Variable* var) {
    DCHECK_EQ(FIELD, kind());
    DCHECK(!is_private());
    private_or_computed_name_var_ = var;
  }

  Variable* computed_name_var() const {
    DCHECK_EQ(FIELD, kind());
    DCHECK(!is_private());
    return private_or_computed_name_var_;
  }

  void set_private_name_var(Variable* var) {
    DCHECK_EQ(FIELD, kind());
    DCHECK(is_private());
    private_or_computed_name_var_ = var;
  }
  Variable* private_name_var() const {
    DCHECK_EQ(FIELD, kind());
    DCHECK(is_private());
    return private_or_computed_name_var_;
  }

 private:
  friend class AstNodeFactory;

  ClassLiteralProperty(Expression* key, Expression* value, Kind kind,
                       bool is_static, bool is_computed_name, bool is_private);

  Kind kind_;
  bool is_static_;
  bool is_private_;
  Variable* private_or_computed_name_var_;
};

class InitializeClassMembersStatement final : public Statement {
 public:
  typedef ClassLiteralProperty Property;

  ZonePtrList<Property>* fields() const { return fields_; }

 private:
  friend class AstNodeFactory;

  InitializeClassMembersStatement(ZonePtrList<Property>* fields, int pos)
      : Statement(pos, kInitializeClassMembersStatement), fields_(fields) {}

  ZonePtrList<Property>* fields_;
};

class ClassLiteral final : public Expression {
 public:
  typedef ClassLiteralProperty Property;

  Scope* scope() const { return scope_; }
  Variable* class_variable() const { return class_variable_; }
  Expression* extends() const { return extends_; }
  FunctionLiteral* constructor() const { return constructor_; }
  ZonePtrList<Property>* properties() const { return properties_; }
  int start_position() const { return position(); }
  int end_position() const { return end_position_; }
  bool has_name_static_property() const {
    return HasNameStaticProperty::decode(bit_field_);
  }
  bool has_static_computed_names() const {
    return HasStaticComputedNames::decode(bit_field_);
  }

  bool is_anonymous_expression() const {
    return IsAnonymousExpression::decode(bit_field_);
  }
  bool IsAnonymousFunctionDefinition() const {
    return is_anonymous_expression();
  }

  FunctionLiteral* static_fields_initializer() const {
    return static_fields_initializer_;
  }

  FunctionLiteral* instance_members_initializer_function() const {
    return instance_members_initializer_function_;
  }

 private:
  friend class AstNodeFactory;

  ClassLiteral(Scope* scope, Variable* class_variable, Expression* extends,
               FunctionLiteral* constructor, ZonePtrList<Property>* properties,
               FunctionLiteral* static_fields_initializer,
               FunctionLiteral* instance_members_initializer_function,
               int start_position, int end_position,
               bool has_name_static_property, bool has_static_computed_names,
               bool is_anonymous)
      : Expression(start_position, kClassLiteral),
        end_position_(end_position),
        scope_(scope),
        class_variable_(class_variable),
        extends_(extends),
        constructor_(constructor),
        properties_(properties),
        static_fields_initializer_(static_fields_initializer),
        instance_members_initializer_function_(
            instance_members_initializer_function) {
    bit_field_ |= HasNameStaticProperty::encode(has_name_static_property) |
                  HasStaticComputedNames::encode(has_static_computed_names) |
                  IsAnonymousExpression::encode(is_anonymous);
  }

  int end_position_;
  Scope* scope_;
  Variable* class_variable_;
  Expression* extends_;
  FunctionLiteral* constructor_;
  ZonePtrList<Property>* properties_;
  FunctionLiteral* static_fields_initializer_;
  FunctionLiteral* instance_members_initializer_function_;
  class HasNameStaticProperty
      : public BitField<bool, Expression::kNextBitFieldIndex, 1> {};
  class HasStaticComputedNames
      : public BitField<bool, HasNameStaticProperty::kNext, 1> {};
  class IsAnonymousExpression
      : public BitField<bool, HasStaticComputedNames::kNext, 1> {};
};


class NativeFunctionLiteral final : public Expression {
 public:
  Handle<String> name() const { return name_->string(); }
  const AstRawString* raw_name() const { return name_; }
  v8::Extension* extension() const { return extension_; }

 private:
  friend class AstNodeFactory;

  NativeFunctionLiteral(const AstRawString* name, v8::Extension* extension,
                        int pos)
      : Expression(pos, kNativeFunctionLiteral),
        name_(name),
        extension_(extension) {}

  const AstRawString* name_;
  v8::Extension* extension_;
};


class SuperPropertyReference final : public Expression {
 public:
  Expression* home_object() const { return home_object_; }

 private:
  friend class AstNodeFactory;

  // We take in ThisExpression* only as a proof that it was accessed.
  SuperPropertyReference(Expression* home_object, int pos)
      : Expression(pos, kSuperPropertyReference), home_object_(home_object) {
    DCHECK(home_object->IsProperty());
  }

  Expression* home_object_;
};


class SuperCallReference final : public Expression {
 public:
  VariableProxy* new_target_var() const { return new_target_var_; }
  VariableProxy* this_function_var() const { return this_function_var_; }

 private:
  friend class AstNodeFactory;

  // We take in ThisExpression* only as a proof that it was accessed.
  SuperCallReference(VariableProxy* new_target_var,
                     VariableProxy* this_function_var, int pos)
      : Expression(pos, kSuperCallReference),
        new_target_var_(new_target_var),
        this_function_var_(this_function_var) {
    DCHECK(new_target_var->raw_name()->IsOneByteEqualTo(".new.target"));
    DCHECK(this_function_var->raw_name()->IsOneByteEqualTo(".this_function"));
  }

  VariableProxy* new_target_var_;
  VariableProxy* this_function_var_;
};

// This AST Node is used to represent a dynamic import call --
// import(argument).
class ImportCallExpression final : public Expression {
 public:
  Expression* argument() const { return argument_; }

 private:
  friend class AstNodeFactory;

  ImportCallExpression(Expression* argument, int pos)
      : Expression(pos, kImportCallExpression), argument_(argument) {}

  Expression* argument_;
};

// This class is produced when parsing the () in arrow functions without any
// arguments and is not actually a valid expression.
class EmptyParentheses final : public Expression {
 private:
  friend class AstNodeFactory;

  explicit EmptyParentheses(int pos) : Expression(pos, kEmptyParentheses) {
    mark_parenthesized();
  }
};

// Represents the spec operation `GetTemplateObject(templateLiteral)`
// (defined at https://tc39.github.io/ecma262/#sec-gettemplateobject).
class GetTemplateObject final : public Expression {
 public:
  const ZonePtrList<const AstRawString>* cooked_strings() const {
    return cooked_strings_;
  }
  const ZonePtrList<const AstRawString>* raw_strings() const {
    return raw_strings_;
  }

  Handle<TemplateObjectDescription> GetOrBuildDescription(Isolate* isolate);

 private:
  friend class AstNodeFactory;

  GetTemplateObject(const ZonePtrList<const AstRawString>* cooked_strings,
                    const ZonePtrList<const AstRawString>* raw_strings, int pos)
      : Expression(pos, kGetTemplateObject),
        cooked_strings_(cooked_strings),
        raw_strings_(raw_strings) {}

  const ZonePtrList<const AstRawString>* cooked_strings_;
  const ZonePtrList<const AstRawString>* raw_strings_;
};

class TemplateLiteral final : public Expression {
 public:
  const ZonePtrList<const AstRawString>* string_parts() const {
    return string_parts_;
  }
  const ZonePtrList<Expression>* substitutions() const {
    return substitutions_;
  }

 private:
  friend class AstNodeFactory;
  TemplateLiteral(const ZonePtrList<const AstRawString>* parts,
                  const ZonePtrList<Expression>* substitutions, int pos)
      : Expression(pos, kTemplateLiteral),
        string_parts_(parts),
        substitutions_(substitutions) {}

  const ZonePtrList<const AstRawString>* string_parts_;
  const ZonePtrList<Expression>* substitutions_;
};

// ----------------------------------------------------------------------------
// Basic visitor
// Sub-class should parametrize AstVisitor with itself, e.g.:
//   class SpecificVisitor : public AstVisitor<SpecificVisitor> { ... }

template <class Subclass>
class AstVisitor {
 public:
  void Visit(AstNode* node) { impl()->Visit(node); }

  void VisitDeclarations(Declaration::List* declarations) {
    for (Declaration* decl : *declarations) Visit(decl);
  }

  void VisitStatements(const ZonePtrList<Statement>* statements) {
    for (int i = 0; i < statements->length(); i++) {
      Statement* stmt = statements->at(i);
      Visit(stmt);
    }
  }

  void VisitExpressions(const ZonePtrList<Expression>* expressions) {
    for (int i = 0; i < expressions->length(); i++) {
      // The variable statement visiting code may pass null expressions
      // to this code. Maybe this should be handled by introducing an
      // undefined expression or literal? Revisit this code if this
      // changes.
      Expression* expression = expressions->at(i);
      if (expression != nullptr) Visit(expression);
    }
  }

 protected:
  Subclass* impl() { return static_cast<Subclass*>(this); }
};

#define GENERATE_VISIT_CASE(NodeType)                                   \
  case AstNode::k##NodeType:                                            \
    return this->impl()->Visit##NodeType(static_cast<NodeType*>(node));

#define GENERATE_FAILURE_CASE(NodeType) \
  case AstNode::k##NodeType:            \
    UNREACHABLE();

#define GENERATE_AST_VISITOR_SWITCH()        \
  switch (node->node_type()) {               \
    AST_NODE_LIST(GENERATE_VISIT_CASE)       \
    FAILURE_NODE_LIST(GENERATE_FAILURE_CASE) \
  }

#define DEFINE_AST_VISITOR_SUBCLASS_MEMBERS()               \
 public:                                                    \
  void VisitNoStackOverflowCheck(AstNode* node) {           \
    GENERATE_AST_VISITOR_SWITCH()                           \
  }                                                         \
                                                            \
  void Visit(AstNode* node) {                               \
    if (CheckStackOverflow()) return;                       \
    VisitNoStackOverflowCheck(node);                        \
  }                                                         \
                                                            \
  void SetStackOverflow() { stack_overflow_ = true; }       \
  void ClearStackOverflow() { stack_overflow_ = false; }    \
  bool HasStackOverflow() const { return stack_overflow_; } \
                                                            \
  bool CheckStackOverflow() {                               \
    if (stack_overflow_) return true;                       \
    if (GetCurrentStackPosition() < stack_limit_) {         \
      stack_overflow_ = true;                               \
      return true;                                          \
    }                                                       \
    return false;                                           \
  }                                                         \
                                                            \
 private:                                                   \
  void InitializeAstVisitor(Isolate* isolate) {             \
    stack_limit_ = isolate->stack_guard()->real_climit();   \
    stack_overflow_ = false;                                \
  }                                                         \
                                                            \
  void InitializeAstVisitor(uintptr_t stack_limit) {        \
    stack_limit_ = stack_limit;                             \
    stack_overflow_ = false;                                \
  }                                                         \
                                                            \
  uintptr_t stack_limit_;                                   \
  bool stack_overflow_

#define DEFINE_AST_VISITOR_MEMBERS_WITHOUT_STACKOVERFLOW()    \
 public:                                                      \
  void Visit(AstNode* node) { GENERATE_AST_VISITOR_SWITCH() } \
                                                              \
 private:

// ----------------------------------------------------------------------------
// AstNode factory

class AstNodeFactory final {
 public:
  AstNodeFactory(AstValueFactory* ast_value_factory, Zone* zone)
      : zone_(zone),
        ast_value_factory_(ast_value_factory),
        empty_statement_(new (zone) class EmptyStatement()),
        this_expression_(new (zone) class ThisExpression()),
        failure_expression_(new (zone) class FailureExpression()) {}

  AstNodeFactory* ast_node_factory() { return this; }
  AstValueFactory* ast_value_factory() const { return ast_value_factory_; }

  VariableDeclaration* NewVariableDeclaration(int pos) {
    return new (zone_) VariableDeclaration(pos);
  }

  NestedVariableDeclaration* NewNestedVariableDeclaration(Scope* scope,
                                                          int pos) {
    return new (zone_) NestedVariableDeclaration(scope, pos);
  }

  FunctionDeclaration* NewFunctionDeclaration(FunctionLiteral* fun, int pos) {
    return new (zone_) FunctionDeclaration(fun, pos);
  }

  Block* NewBlock(int capacity, bool ignore_completion_value) {
    return new (zone_) Block(zone_, nullptr, capacity, ignore_completion_value);
  }

  Block* NewBlock(bool ignore_completion_value,
                  ZonePtrList<const AstRawString>* labels) {
    return labels != nullptr
               ? new (zone_) LabeledBlock(labels, ignore_completion_value)
               : new (zone_) Block(labels, ignore_completion_value);
  }

  Block* NewBlock(bool ignore_completion_value,
                  const ScopedPtrList<Statement>& statements) {
    Block* result = NewBlock(ignore_completion_value, nullptr);
    result->InitializeStatements(statements, zone_);
    return result;
  }

#define STATEMENT_WITH_LABELS(NodeType)                                \
  NodeType* New##NodeType(ZonePtrList<const AstRawString>* labels,     \
                          ZonePtrList<const AstRawString>* own_labels, \
                          int pos) {                                   \
    return new (zone_) NodeType(labels, own_labels, pos);              \
  }
  STATEMENT_WITH_LABELS(DoWhileStatement)
  STATEMENT_WITH_LABELS(WhileStatement)
  STATEMENT_WITH_LABELS(ForStatement)
#undef STATEMENT_WITH_LABELS

  SwitchStatement* NewSwitchStatement(ZonePtrList<const AstRawString>* labels,
                                      Expression* tag, int pos) {
    return new (zone_) SwitchStatement(zone_, labels, tag, pos);
  }

  ForEachStatement* NewForEachStatement(
      ForEachStatement::VisitMode visit_mode,
      ZonePtrList<const AstRawString>* labels,
      ZonePtrList<const AstRawString>* own_labels, int pos) {
    switch (visit_mode) {
      case ForEachStatement::ENUMERATE: {
        return new (zone_) ForInStatement(labels, own_labels, pos);
      }
      case ForEachStatement::ITERATE: {
        return new (zone_)
            ForOfStatement(labels, own_labels, pos, IteratorType::kNormal);
      }
    }
    UNREACHABLE();
  }

  ForOfStatement* NewForOfStatement(ZonePtrList<const AstRawString>* labels,
                                    ZonePtrList<const AstRawString>* own_labels,
                                    int pos, IteratorType type) {
    return new (zone_) ForOfStatement(labels, own_labels, pos, type);
  }

  ExpressionStatement* NewExpressionStatement(Expression* expression, int pos) {
    return new (zone_) ExpressionStatement(expression, pos);
  }

  ContinueStatement* NewContinueStatement(IterationStatement* target, int pos) {
    return new (zone_) ContinueStatement(target, pos);
  }

  BreakStatement* NewBreakStatement(BreakableStatement* target, int pos) {
    return new (zone_) BreakStatement(target, pos);
  }

  ReturnStatement* NewReturnStatement(Expression* expression, int pos,
                                      int end_position = kNoSourcePosition) {
    return new (zone_) ReturnStatement(expression, ReturnStatement::kNormal,
                                       pos, end_position);
  }

  ReturnStatement* NewAsyncReturnStatement(
      Expression* expression, int pos, int end_position = kNoSourcePosition) {
    return new (zone_) ReturnStatement(
        expression, ReturnStatement::kAsyncReturn, pos, end_position);
  }

  WithStatement* NewWithStatement(Scope* scope,
                                  Expression* expression,
                                  Statement* statement,
                                  int pos) {
    return new (zone_) WithStatement(scope, expression, statement, pos);
  }

  IfStatement* NewIfStatement(Expression* condition, Statement* then_statement,
                              Statement* else_statement, int pos) {
    return new (zone_)
        IfStatement(condition, then_statement, else_statement, pos);
  }

  TryCatchStatement* NewTryCatchStatement(Block* try_block, Scope* scope,
                                          Block* catch_block, int pos) {
    return new (zone_) TryCatchStatement(try_block, scope, catch_block,
                                         HandlerTable::CAUGHT, pos);
  }

  TryCatchStatement* NewTryCatchStatementForReThrow(Block* try_block,
                                                    Scope* scope,
                                                    Block* catch_block,
                                                    int pos) {
    return new (zone_) TryCatchStatement(try_block, scope, catch_block,
                                         HandlerTable::UNCAUGHT, pos);
  }

  TryCatchStatement* NewTryCatchStatementForDesugaring(Block* try_block,
                                                       Scope* scope,
                                                       Block* catch_block,
                                                       int pos) {
    return new (zone_) TryCatchStatement(try_block, scope, catch_block,
                                         HandlerTable::DESUGARING, pos);
  }

  TryCatchStatement* NewTryCatchStatementForAsyncAwait(Block* try_block,
                                                       Scope* scope,
                                                       Block* catch_block,
                                                       int pos) {
    return new (zone_) TryCatchStatement(try_block, scope, catch_block,
                                         HandlerTable::ASYNC_AWAIT, pos);
  }

  TryFinallyStatement* NewTryFinallyStatement(Block* try_block,
                                              Block* finally_block, int pos) {
    return new (zone_) TryFinallyStatement(try_block, finally_block, pos);
  }

  DebuggerStatement* NewDebuggerStatement(int pos) {
    return new (zone_) DebuggerStatement(pos);
  }

  class EmptyStatement* EmptyStatement() {
    return empty_statement_;
  }

  class ThisExpression* ThisExpression() {
    return this_expression_;
  }

  class FailureExpression* FailureExpression() {
    return failure_expression_;
  }

  SloppyBlockFunctionStatement* NewSloppyBlockFunctionStatement(
      int pos, Variable* var, Token::Value init) {
    return new (zone_)
        SloppyBlockFunctionStatement(pos, var, init, EmptyStatement());
  }

  CaseClause* NewCaseClause(Expression* label,
                            const ScopedPtrList<Statement>& statements) {
    return new (zone_) CaseClause(zone_, label, statements);
  }

  Literal* NewStringLiteral(const AstRawString* string, int pos) {
    DCHECK_NOT_NULL(string);
    return new (zone_) Literal(string, pos);
  }

  // A JavaScript symbol (ECMA-262 edition 6).
  Literal* NewSymbolLiteral(AstSymbol symbol, int pos) {
    return new (zone_) Literal(symbol, pos);
  }

  Literal* NewNumberLiteral(double number, int pos);

  Literal* NewSmiLiteral(int number, int pos) {
    return new (zone_) Literal(number, pos);
  }

  Literal* NewBigIntLiteral(AstBigInt bigint, int pos) {
    return new (zone_) Literal(bigint, pos);
  }

  Literal* NewBooleanLiteral(bool b, int pos) {
    return new (zone_) Literal(b, pos);
  }

  Literal* NewNullLiteral(int pos) {
    return new (zone_) Literal(Literal::kNull, pos);
  }

  Literal* NewUndefinedLiteral(int pos) {
    return new (zone_) Literal(Literal::kUndefined, pos);
  }

  Literal* NewTheHoleLiteral() {
    return new (zone_) Literal(Literal::kTheHole, kNoSourcePosition);
  }

  ObjectLiteral* NewObjectLiteral(
      const ScopedPtrList<ObjectLiteral::Property>& properties,
      uint32_t boilerplate_properties, int pos, bool has_rest_property) {
    return new (zone_) ObjectLiteral(zone_, properties, boilerplate_properties,
                                     pos, has_rest_property);
  }

  ObjectLiteral::Property* NewObjectLiteralProperty(
      Expression* key, Expression* value, ObjectLiteralProperty::Kind kind,
      bool is_computed_name) {
    return new (zone_)
        ObjectLiteral::Property(key, value, kind, is_computed_name);
  }

  ObjectLiteral::Property* NewObjectLiteralProperty(Expression* key,
                                                    Expression* value,
                                                    bool is_computed_name) {
    return new (zone_) ObjectLiteral::Property(ast_value_factory_, key, value,
                                               is_computed_name);
  }

  RegExpLiteral* NewRegExpLiteral(const AstRawString* pattern, int flags,
                                  int pos) {
    return new (zone_) RegExpLiteral(pattern, flags, pos);
  }

  ArrayLiteral* NewArrayLiteral(const ScopedPtrList<Expression>& values,
                                int pos) {
    return new (zone_) ArrayLiteral(zone_, values, -1, pos);
  }

  ArrayLiteral* NewArrayLiteral(const ScopedPtrList<Expression>& values,
                                int first_spread_index, int pos) {
    return new (zone_) ArrayLiteral(zone_, values, first_spread_index, pos);
  }

  VariableProxy* NewVariableProxy(Variable* var,
                                  int start_position = kNoSourcePosition) {
    return new (zone_) VariableProxy(var, start_position);
  }

  VariableProxy* NewVariableProxy(const AstRawString* name,
                                  VariableKind variable_kind,
                                  int start_position = kNoSourcePosition) {
    DCHECK_NOT_NULL(name);
    return new (zone_) VariableProxy(name, variable_kind, start_position);
  }

  // Recreates the VariableProxy in this Zone.
  VariableProxy* CopyVariableProxy(VariableProxy* proxy) {
    return new (zone_) VariableProxy(proxy);
  }

  Variable* CopyVariable(Variable* variable) {
    return new (zone_) Variable(variable);
  }

  Property* NewProperty(Expression* obj, Expression* key, int pos) {
    return new (zone_) Property(obj, key, pos);
  }

  ResolvedProperty* NewResolvedProperty(VariableProxy* obj,
                                        VariableProxy* property,
                                        int pos = kNoSourcePosition) {
    return new (zone_) ResolvedProperty(obj, property, pos);
  }

  Call* NewCall(Expression* expression,
                const ScopedPtrList<Expression>& arguments, int pos,
                Call::PossiblyEval possibly_eval = Call::NOT_EVAL) {
    return new (zone_) Call(zone_, expression, arguments, pos, possibly_eval);
  }

  Call* NewTaggedTemplate(Expression* expression,
                          const ScopedPtrList<Expression>& arguments, int pos) {
    return new (zone_)
        Call(zone_, expression, arguments, pos, Call::TaggedTemplateTag::kTrue);
  }

  CallNew* NewCallNew(Expression* expression,
                      const ScopedPtrList<Expression>& arguments, int pos) {
    return new (zone_) CallNew(zone_, expression, arguments, pos);
  }

  CallRuntime* NewCallRuntime(Runtime::FunctionId id,
                              const ScopedPtrList<Expression>& arguments,
                              int pos) {
    return new (zone_)
        CallRuntime(zone_, Runtime::FunctionForId(id), arguments, pos);
  }

  CallRuntime* NewCallRuntime(const Runtime::Function* function,
                              const ScopedPtrList<Expression>& arguments,
                              int pos) {
    return new (zone_) CallRuntime(zone_, function, arguments, pos);
  }

  CallRuntime* NewCallRuntime(int context_index,
                              const ScopedPtrList<Expression>& arguments,
                              int pos) {
    return new (zone_) CallRuntime(zone_, context_index, arguments, pos);
  }

  UnaryOperation* NewUnaryOperation(Token::Value op,
                                    Expression* expression,
                                    int pos) {
    return new (zone_) UnaryOperation(op, expression, pos);
  }

  BinaryOperation* NewBinaryOperation(Token::Value op,
                                      Expression* left,
                                      Expression* right,
                                      int pos) {
    return new (zone_) BinaryOperation(op, left, right, pos);
  }

  NaryOperation* NewNaryOperation(Token::Value op, Expression* first,
                                  size_t initial_subsequent_size) {
    return new (zone_) NaryOperation(zone_, op, first, initial_subsequent_size);
  }

  CountOperation* NewCountOperation(Token::Value op,
                                    bool is_prefix,
                                    Expression* expr,
                                    int pos) {
    return new (zone_) CountOperation(op, is_prefix, expr, pos);
  }

  CompareOperation* NewCompareOperation(Token::Value op,
                                        Expression* left,
                                        Expression* right,
                                        int pos) {
    return new (zone_) CompareOperation(op, left, right, pos);
  }

  Spread* NewSpread(Expression* expression, int pos, int expr_pos) {
    return new (zone_) Spread(expression, pos, expr_pos);
  }

  StoreInArrayLiteral* NewStoreInArrayLiteral(Expression* array,
                                              Expression* index,
                                              Expression* value, int pos) {
    return new (zone_) StoreInArrayLiteral(array, index, value, pos);
  }

  Conditional* NewConditional(Expression* condition,
                              Expression* then_expression,
                              Expression* else_expression,
                              int position) {
    return new (zone_)
        Conditional(condition, then_expression, else_expression, position);
  }

  Assignment* NewAssignment(Token::Value op,
                            Expression* target,
                            Expression* value,
                            int pos) {
    DCHECK(Token::IsAssignmentOp(op));
    DCHECK_NOT_NULL(target);
    DCHECK_NOT_NULL(value);

    if (op != Token::INIT && target->IsVariableProxy()) {
      target->AsVariableProxy()->set_is_assigned();
    }

    if (op == Token::ASSIGN || op == Token::INIT) {
      return new (zone_)
          Assignment(AstNode::kAssignment, op, target, value, pos);
    } else {
      return new (zone_) CompoundAssignment(
          op, target, value, pos,
          NewBinaryOperation(Token::BinaryOpForAssignment(op), target, value,
                             pos + 1));
    }
  }

  Suspend* NewYield(Expression* expression, int pos,
                    Suspend::OnAbruptResume on_abrupt_resume) {
    if (!expression) expression = NewUndefinedLiteral(pos);
    return new (zone_) Yield(expression, pos, on_abrupt_resume);
  }

  YieldStar* NewYieldStar(Expression* expression, int pos) {
    return new (zone_) YieldStar(expression, pos);
  }

  Await* NewAwait(Expression* expression, int pos) {
    if (!expression) expression = NewUndefinedLiteral(pos);
    return new (zone_) Await(expression, pos);
  }

  Throw* NewThrow(Expression* exception, int pos) {
    return new (zone_) Throw(exception, pos);
  }

  FunctionLiteral* NewFunctionLiteral(
      const AstRawString* name, DeclarationScope* scope,
      const ScopedPtrList<Statement>& body, int expected_property_count,
      int parameter_count, int function_length,
      FunctionLiteral::ParameterFlag has_duplicate_parameters,
      FunctionLiteral::FunctionType function_type,
      FunctionLiteral::EagerCompileHint eager_compile_hint, int position,
      bool has_braces, int function_literal_id,
      ProducedPreparseData* produced_preparse_data = nullptr) {
    return new (zone_) FunctionLiteral(
        zone_, name, ast_value_factory_, scope, body, expected_property_count,
        parameter_count, function_length, function_type,
        has_duplicate_parameters, eager_compile_hint, position, has_braces,
        function_literal_id, produced_preparse_data);
  }

  // Creates a FunctionLiteral representing a top-level script, the
  // result of an eval (top-level or otherwise), or the result of calling
  // the Function constructor.
  FunctionLiteral* NewScriptOrEvalFunctionLiteral(
      DeclarationScope* scope, const ScopedPtrList<Statement>& body,
      int expected_property_count, int parameter_count) {
    return new (zone_) FunctionLiteral(
        zone_, ast_value_factory_->empty_string(), ast_value_factory_, scope,
        body, expected_property_count, parameter_count, parameter_count,
        FunctionLiteral::kAnonymousExpression,
        FunctionLiteral::kNoDuplicateParameters,
        FunctionLiteral::kShouldLazyCompile, 0, /* has_braces */ false,
        kFunctionLiteralIdTopLevel);
  }

  ClassLiteral::Property* NewClassLiteralProperty(
      Expression* key, Expression* value, ClassLiteralProperty::Kind kind,
      bool is_static, bool is_computed_name, bool is_private) {
    return new (zone_) ClassLiteral::Property(key, value, kind, is_static,
                                              is_computed_name, is_private);
  }

  ClassLiteral* NewClassLiteral(
      Scope* scope, Variable* variable, Expression* extends,
      FunctionLiteral* constructor,
      ZonePtrList<ClassLiteral::Property>* properties,
      FunctionLiteral* static_fields_initializer,
      FunctionLiteral* instance_members_initializer_function,
      int start_position, int end_position, bool has_name_static_property,
      bool has_static_computed_names, bool is_anonymous) {
    return new (zone_) ClassLiteral(
        scope, variable, extends, constructor, properties,
        static_fields_initializer, instance_members_initializer_function,
        start_position, end_position, has_name_static_property,
        has_static_computed_names, is_anonymous);
  }

  NativeFunctionLiteral* NewNativeFunctionLiteral(const AstRawString* name,
                                                  v8::Extension* extension,
                                                  int pos) {
    return new (zone_) NativeFunctionLiteral(name, extension, pos);
  }

  DoExpression* NewDoExpression(Block* block, Variable* result_var, int pos) {
    VariableProxy* result = NewVariableProxy(result_var, pos);
    return new (zone_) DoExpression(block, result, pos);
  }

  SuperPropertyReference* NewSuperPropertyReference(Expression* home_object,
                                                    int pos) {
    return new (zone_) SuperPropertyReference(home_object, pos);
  }

  SuperCallReference* NewSuperCallReference(VariableProxy* new_target_var,
                                            VariableProxy* this_function_var,
                                            int pos) {
    return new (zone_)
        SuperCallReference(new_target_var, this_function_var, pos);
  }

  EmptyParentheses* NewEmptyParentheses(int pos) {
    return new (zone_) EmptyParentheses(pos);
  }

  GetTemplateObject* NewGetTemplateObject(
      const ZonePtrList<const AstRawString>* cooked_strings,
      const ZonePtrList<const AstRawString>* raw_strings, int pos) {
    return new (zone_) GetTemplateObject(cooked_strings, raw_strings, pos);
  }

  TemplateLiteral* NewTemplateLiteral(
      const ZonePtrList<const AstRawString>* string_parts,
      const ZonePtrList<Expression>* substitutions, int pos) {
    return new (zone_) TemplateLiteral(string_parts, substitutions, pos);
  }

  ImportCallExpression* NewImportCallExpression(Expression* args, int pos) {
    return new (zone_) ImportCallExpression(args, pos);
  }

  InitializeClassMembersStatement* NewInitializeClassMembersStatement(
      ZonePtrList<ClassLiteral::Property>* args, int pos) {
    return new (zone_) InitializeClassMembersStatement(args, pos);
  }

  Zone* zone() const { return zone_; }

 private:
  // This zone may be deallocated upon returning from parsing a function body
  // which we can guarantee is not going to be compiled or have its AST
  // inspected.
  // See ParseFunctionLiteral in parser.cc for preconditions.
  Zone* zone_;
  AstValueFactory* ast_value_factory_;
  class EmptyStatement* empty_statement_;
  class ThisExpression* this_expression_;
  class FailureExpression* failure_expression_;
};


// Type testing & conversion functions overridden by concrete subclasses.
// Inline functions for AstNode.

#define DECLARE_NODE_FUNCTIONS(type)                                         \
  bool AstNode::Is##type() const { return node_type() == AstNode::k##type; } \
  type* AstNode::As##type() {                                                \
    return node_type() == AstNode::k##type ? reinterpret_cast<type*>(this)   \
                                           : nullptr;                        \
  }                                                                          \
  const type* AstNode::As##type() const {                                    \
    return node_type() == AstNode::k##type                                   \
               ? reinterpret_cast<const type*>(this)                         \
               : nullptr;                                                    \
  }
AST_NODE_LIST(DECLARE_NODE_FUNCTIONS)
FAILURE_NODE_LIST(DECLARE_NODE_FUNCTIONS)
#undef DECLARE_NODE_FUNCTIONS

}  // namespace internal
}  // namespace v8

#endif  // V8_AST_AST_H_
