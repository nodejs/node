// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_AST_AST_H_
#define V8_AST_AST_H_

#include "src/ast/ast-value-factory.h"
#include "src/ast/modules.h"
#include "src/ast/variables.h"
#include "src/bailout-reason.h"
#include "src/factory.h"
#include "src/globals.h"
#include "src/isolate.h"
#include "src/label.h"
#include "src/list.h"
#include "src/objects/literal-objects.h"
#include "src/parsing/token.h"
#include "src/runtime/runtime.h"
#include "src/small-pointer-list.h"

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
  V(DebuggerStatement)

#define LITERAL_NODE_LIST(V) \
  V(RegExpLiteral)           \
  V(ObjectLiteral)           \
  V(ArrayLiteral)

#define PROPERTY_NODE_LIST(V) \
  V(Assignment)               \
  V(CountOperation)           \
  V(Property)

#define CALL_NODE_LIST(V) \
  V(Call)                 \
  V(CallNew)

#define EXPRESSION_NODE_LIST(V) \
  LITERAL_NODE_LIST(V)          \
  PROPERTY_NODE_LIST(V)         \
  CALL_NODE_LIST(V)             \
  V(FunctionLiteral)            \
  V(ClassLiteral)               \
  V(NativeFunctionLiteral)      \
  V(Conditional)                \
  V(VariableProxy)              \
  V(Literal)                    \
  V(Yield)                      \
  V(YieldStar)                  \
  V(Await)                      \
  V(Throw)                      \
  V(CallRuntime)                \
  V(UnaryOperation)             \
  V(BinaryOperation)            \
  V(CompareOperation)           \
  V(Spread)                     \
  V(ThisFunction)               \
  V(SuperPropertyReference)     \
  V(SuperCallReference)         \
  V(CaseClause)                 \
  V(EmptyParentheses)           \
  V(GetIterator)                \
  V(DoExpression)               \
  V(RewritableExpression)       \
  V(ImportCallExpression)

#define AST_NODE_LIST(V)                        \
  DECLARATION_NODE_LIST(V)                      \
  STATEMENT_NODE_LIST(V)                        \
  EXPRESSION_NODE_LIST(V)

// Forward declarations
class AstNodeFactory;
class Declaration;
class Module;
class BreakableStatement;
class Expression;
class IterationStatement;
class MaterializedLiteral;
class ProducedPreParsedScopeData;
class Statement;

#define DEF_FORWARD_DECLARATION(type) class type;
AST_NODE_LIST(DEF_FORWARD_DECLARATION)
#undef DEF_FORWARD_DECLARATION

class FeedbackSlotCache {
 public:
  typedef std::pair<TypeofMode, Variable*> Key;

  explicit FeedbackSlotCache(Zone* zone) : map_(zone) {}

  void Put(TypeofMode typeof_mode, Variable* variable, FeedbackSlot slot) {
    Key key = std::make_pair(typeof_mode, variable);
    auto entry = std::make_pair(key, slot);
    map_.insert(entry);
  }

  FeedbackSlot Get(TypeofMode typeof_mode, Variable* variable) const {
    Key key = std::make_pair(typeof_mode, variable);
    auto iter = map_.find(key);
    if (iter != map_.end()) {
      return iter->second;
    }
    return FeedbackSlot();
  }

 private:
  ZoneMap<Key, FeedbackSlot> map_;
};


class AstProperties final BASE_EMBEDDED {
 public:
  explicit AstProperties(Zone* zone) : node_count_(0), spec_(zone) {}

  int node_count() { return node_count_; }
  void add_node_count(int count) { node_count_ += count; }

  const FeedbackVectorSpec* get_spec() const { return &spec_; }
  FeedbackVectorSpec* get_spec() { return &spec_; }

 private:
  int node_count_;
  FeedbackVectorSpec spec_;
};


class AstNode: public ZoneObject {
 public:
#define DECLARE_TYPE_ENUM(type) k##type,
  enum NodeType : uint8_t { AST_NODE_LIST(DECLARE_TYPE_ENUM) };
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
#undef DECLARE_NODE_FUNCTIONS

  BreakableStatement* AsBreakableStatement();
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
 public:
  bool IsEmpty() { return AsEmptyStatement() != NULL; }
  bool IsJump() const;

 protected:
  Statement(int position, NodeType type) : AstNode(position, type) {}

  static const uint8_t kNextBitFieldIndex = AstNode::kNextBitFieldIndex;
};


class SmallMapList final {
 public:
  SmallMapList() {}
  SmallMapList(int capacity, Zone* zone) : list_(capacity, zone) {}

  void Reserve(int capacity, Zone* zone) { list_.Reserve(capacity, zone); }
  void Clear() { list_.Clear(); }
  void Sort() { list_.Sort(); }

  bool is_empty() const { return list_.is_empty(); }
  int length() const { return list_.length(); }

  void Add(Handle<Map> handle, Zone* zone) {
    list_.Add(handle.location(), zone);
  }

  Handle<Map> at(int i) const;

  Handle<Map> first() const { return at(0); }
  Handle<Map> last() const { return at(length() - 1); }

 private:
  // The list stores pointers to Map*, that is Map**, so it's GC safe.
  SmallPointerList<Map*> list_;

  DISALLOW_COPY_AND_ASSIGN(SmallMapList);
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
  bool IsNumberLiteral() const;

  // True iff the expression is a string literal.
  bool IsStringLiteral() const;

  // True iff the expression is the null literal.
  bool IsNullLiteral() const;

  // True if we can prove that the expression is the undefined literal. Note
  // that this also checks for loads of the global "undefined" variable.
  bool IsUndefinedLiteral() const;

  // True iff the expression is a valid target for an assignment.
  bool IsValidReferenceExpressionOrThis() const;

  SmallMapList* GetReceiverTypes();
  KeyedAccessStoreMode GetStoreMode() const;
  IcCheckType GetKeyType() const;
  bool IsMonomorphic() const;

 protected:
  Expression(int pos, NodeType type) : AstNode(pos, type) {}

  static const uint8_t kNextBitFieldIndex = AstNode::kNextBitFieldIndex;
};


class BreakableStatement : public Statement {
 public:
  enum BreakableType {
    TARGET_FOR_ANONYMOUS,
    TARGET_FOR_NAMED_ONLY
  };

  // The labels associated with this statement. May be NULL;
  // if it is != NULL, guaranteed to contain at least one entry.
  ZoneList<const AstRawString*>* labels() const { return labels_; }

  // Testers.
  bool is_target_for_anonymous() const {
    return BreakableTypeField::decode(bit_field_) == TARGET_FOR_ANONYMOUS;
  }

 private:
  BreakableType breakableType() const {
    return BreakableTypeField::decode(bit_field_);
  }

  ZoneList<const AstRawString*>* labels_;

  class BreakableTypeField
      : public BitField<BreakableType, Statement::kNextBitFieldIndex, 1> {};

 protected:
  BreakableStatement(ZoneList<const AstRawString*>* labels,
                     BreakableType breakable_type, int position, NodeType type)
      : Statement(position, type),
        labels_(labels) {
    DCHECK(labels == NULL || labels->length() > 0);
    bit_field_ |= BreakableTypeField::encode(breakable_type);
  }

  static const uint8_t kNextBitFieldIndex = BreakableTypeField::kNext;
};


class Block final : public BreakableStatement {
 public:
  ZoneList<Statement*>* statements() { return &statements_; }
  bool ignore_completion_value() const {
    return IgnoreCompletionField::decode(bit_field_);
  }

  bool IsJump() const {
    return !statements_.is_empty() && statements_.last()->IsJump()
        && labels() == NULL;  // Good enough as an approximation...
  }

  Scope* scope() const { return scope_; }
  void set_scope(Scope* scope) { scope_ = scope; }

 private:
  friend class AstNodeFactory;

  Block(Zone* zone, ZoneList<const AstRawString*>* labels, int capacity,
        bool ignore_completion_value, int pos)
      : BreakableStatement(labels, TARGET_FOR_NAMED_ONLY, pos, kBlock),
        statements_(capacity, zone),
        scope_(NULL) {
    bit_field_ |= IgnoreCompletionField::encode(ignore_completion_value);
  }

  ZoneList<Statement*> statements_;
  Scope* scope_;

  class IgnoreCompletionField
      : public BitField<bool, BreakableStatement::kNextBitFieldIndex, 1> {};
};


class DoExpression final : public Expression {
 public:
  Block* block() { return block_; }
  void set_block(Block* b) { block_ = b; }
  VariableProxy* result() { return result_; }
  void set_result(VariableProxy* v) { result_ = v; }

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
  typedef ThreadedList<Declaration> List;

  VariableProxy* proxy() const { return proxy_; }
  Scope* scope() const { return scope_; }

 protected:
  Declaration(VariableProxy* proxy, Scope* scope, int pos, NodeType type)
      : AstNode(pos, type), proxy_(proxy), scope_(scope), next_(nullptr) {}

 private:
  VariableProxy* proxy_;
  // Nested scope from which the declaration originated.
  Scope* scope_;
  // Declarations list threaded through the declarations.
  Declaration** next() { return &next_; }
  Declaration* next_;
  friend List;
};


class VariableDeclaration final : public Declaration {
 private:
  friend class AstNodeFactory;

  VariableDeclaration(VariableProxy* proxy, Scope* scope, int pos)
      : Declaration(proxy, scope, pos, kVariableDeclaration) {}
};


class FunctionDeclaration final : public Declaration {
 public:
  FunctionLiteral* fun() const { return fun_; }
  void set_fun(FunctionLiteral* f) { fun_ = f; }

 private:
  friend class AstNodeFactory;

  FunctionDeclaration(VariableProxy* proxy, FunctionLiteral* fun, Scope* scope,
                      int pos)
      : Declaration(proxy, scope, pos, kFunctionDeclaration), fun_(fun) {
    DCHECK(fun != NULL);
  }

  FunctionLiteral* fun_;
};


class IterationStatement : public BreakableStatement {
 public:
  Statement* body() const { return body_; }
  void set_body(Statement* s) { body_ = s; }

  int suspend_count() const { return suspend_count_; }
  int first_suspend_id() const { return first_suspend_id_; }
  void set_suspend_count(int suspend_count) { suspend_count_ = suspend_count; }
  void set_first_suspend_id(int first_suspend_id) {
    first_suspend_id_ = first_suspend_id;
  }

  void set_osr_id(int id) { osr_id_ = BailoutId(id); }
  BailoutId OsrEntryId() const {
    DCHECK(!osr_id_.IsNone());
    return osr_id_;
  }

 protected:
  IterationStatement(ZoneList<const AstRawString*>* labels, int pos,
                     NodeType type)
      : BreakableStatement(labels, TARGET_FOR_ANONYMOUS, pos, type),
        osr_id_(BailoutId::None()),
        body_(NULL),
        suspend_count_(0),
        first_suspend_id_(0) {}
  void Initialize(Statement* body) { body_ = body; }

  static const uint8_t kNextBitFieldIndex =
      BreakableStatement::kNextBitFieldIndex;

 private:
  BailoutId osr_id_;
  Statement* body_;
  int suspend_count_;
  int first_suspend_id_;
};


class DoWhileStatement final : public IterationStatement {
 public:
  void Initialize(Expression* cond, Statement* body) {
    IterationStatement::Initialize(body);
    cond_ = cond;
  }

  Expression* cond() const { return cond_; }
  void set_cond(Expression* e) { cond_ = e; }

 private:
  friend class AstNodeFactory;

  DoWhileStatement(ZoneList<const AstRawString*>* labels, int pos)
      : IterationStatement(labels, pos, kDoWhileStatement), cond_(NULL) {}

  Expression* cond_;
};


class WhileStatement final : public IterationStatement {
 public:
  void Initialize(Expression* cond, Statement* body) {
    IterationStatement::Initialize(body);
    cond_ = cond;
  }

  Expression* cond() const { return cond_; }
  void set_cond(Expression* e) { cond_ = e; }

 private:
  friend class AstNodeFactory;

  WhileStatement(ZoneList<const AstRawString*>* labels, int pos)
      : IterationStatement(labels, pos, kWhileStatement), cond_(NULL) {}

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

  void set_init(Statement* s) { init_ = s; }
  void set_cond(Expression* e) { cond_ = e; }
  void set_next(Statement* s) { next_ = s; }

 private:
  friend class AstNodeFactory;

  ForStatement(ZoneList<const AstRawString*>* labels, int pos)
      : IterationStatement(labels, pos, kForStatement),
        init_(NULL),
        cond_(NULL),
        next_(NULL) {}

  Statement* init_;
  Expression* cond_;
  Statement* next_;
};


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

 protected:
  ForEachStatement(ZoneList<const AstRawString*>* labels, int pos,
                   NodeType type)
      : IterationStatement(labels, pos, type) {}
};


class ForInStatement final : public ForEachStatement {
 public:
  void Initialize(Expression* each, Expression* subject, Statement* body) {
    ForEachStatement::Initialize(body);
    each_ = each;
    subject_ = subject;
  }

  Expression* enumerable() const {
    return subject();
  }

  Expression* each() const { return each_; }
  Expression* subject() const { return subject_; }

  void set_each(Expression* e) { each_ = e; }
  void set_subject(Expression* e) { subject_ = e; }

  // Type feedback information.
  void AssignFeedbackSlots(FeedbackVectorSpec* spec, LanguageMode language_mode,
                           FeedbackSlotCache* cache);
  FeedbackSlot EachFeedbackSlot() const { return each_slot_; }
  FeedbackSlot ForInFeedbackSlot() {
    DCHECK(!for_in_feedback_slot_.IsInvalid());
    return for_in_feedback_slot_;
  }

  enum ForInType { FAST_FOR_IN, SLOW_FOR_IN };
  ForInType for_in_type() const { return ForInTypeField::decode(bit_field_); }
  void set_for_in_type(ForInType type) {
    bit_field_ = ForInTypeField::update(bit_field_, type);
  }

 private:
  friend class AstNodeFactory;

  ForInStatement(ZoneList<const AstRawString*>* labels, int pos)
      : ForEachStatement(labels, pos, kForInStatement),
        each_(nullptr),
        subject_(nullptr) {
    bit_field_ = ForInTypeField::update(bit_field_, SLOW_FOR_IN);
  }

  Expression* each_;
  Expression* subject_;
  FeedbackSlot each_slot_;
  FeedbackSlot for_in_feedback_slot_;

  class ForInTypeField
      : public BitField<ForInType, ForEachStatement::kNextBitFieldIndex, 1> {};
};


class ForOfStatement final : public ForEachStatement {
 public:
  void Initialize(Statement* body, Variable* iterator,
                  Expression* assign_iterator, Expression* next_result,
                  Expression* result_done, Expression* assign_each) {
    ForEachStatement::Initialize(body);
    iterator_ = iterator;
    assign_iterator_ = assign_iterator;
    next_result_ = next_result;
    result_done_ = result_done;
    assign_each_ = assign_each;
  }

  Variable* iterator() const {
    return iterator_;
  }

  // iterator = subject[Symbol.iterator]()
  Expression* assign_iterator() const {
    return assign_iterator_;
  }

  // result = iterator.next()  // with type check
  Expression* next_result() const {
    return next_result_;
  }

  // result.done
  Expression* result_done() const {
    return result_done_;
  }

  // each = result.value
  Expression* assign_each() const {
    return assign_each_;
  }

  void set_assign_iterator(Expression* e) { assign_iterator_ = e; }
  void set_next_result(Expression* e) { next_result_ = e; }
  void set_result_done(Expression* e) { result_done_ = e; }
  void set_assign_each(Expression* e) { assign_each_ = e; }

 private:
  friend class AstNodeFactory;

  ForOfStatement(ZoneList<const AstRawString*>* labels, int pos)
      : ForEachStatement(labels, pos, kForOfStatement),
        iterator_(NULL),
        assign_iterator_(NULL),
        next_result_(NULL),
        result_done_(NULL),
        assign_each_(NULL) {}

  Variable* iterator_;
  Expression* assign_iterator_;
  Expression* next_result_;
  Expression* result_done_;
  Expression* assign_each_;
};


class ExpressionStatement final : public Statement {
 public:
  void set_expression(Expression* e) { expression_ = e; }
  Expression* expression() const { return expression_; }
  bool IsJump() const { return expression_->IsThrow(); }

 private:
  friend class AstNodeFactory;

  ExpressionStatement(Expression* expression, int pos)
      : Statement(pos, kExpressionStatement), expression_(expression) {}

  Expression* expression_;
};


class JumpStatement : public Statement {
 public:
  bool IsJump() const { return true; }

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

  void set_expression(Expression* e) { expression_ = e; }
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
  void set_expression(Expression* e) { expression_ = e; }
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


class CaseClause final : public Expression {
 public:
  bool is_default() const { return label_ == NULL; }
  Expression* label() const {
    CHECK(!is_default());
    return label_;
  }
  void set_label(Expression* e) { label_ = e; }
  Label* body_target() { return &body_target_; }
  ZoneList<Statement*>* statements() const { return statements_; }

  void AssignFeedbackSlots(FeedbackVectorSpec* spec, LanguageMode language_mode,
                           FeedbackSlotCache* cache);

  FeedbackSlot CompareOperationFeedbackSlot() { return feedback_slot_; }

 private:
  friend class AstNodeFactory;

  CaseClause(Expression* label, ZoneList<Statement*>* statements, int pos);

  FeedbackSlot feedback_slot_;
  Expression* label_;
  Label body_target_;
  ZoneList<Statement*>* statements_;
};


class SwitchStatement final : public BreakableStatement {
 public:
  void Initialize(Expression* tag, ZoneList<CaseClause*>* cases) {
    tag_ = tag;
    cases_ = cases;
  }

  Expression* tag() const { return tag_; }
  ZoneList<CaseClause*>* cases() const { return cases_; }

  void set_tag(Expression* t) { tag_ = t; }

 private:
  friend class AstNodeFactory;

  SwitchStatement(ZoneList<const AstRawString*>* labels, int pos)
      : BreakableStatement(labels, TARGET_FOR_ANONYMOUS, pos, kSwitchStatement),
        tag_(NULL),
        cases_(NULL) {}

  Expression* tag_;
  ZoneList<CaseClause*>* cases_;
};


// If-statements always have non-null references to their then- and
// else-parts. When parsing if-statements with no explicit else-part,
// the parser implicitly creates an empty statement. Use the
// HasThenStatement() and HasElseStatement() functions to check if a
// given if-statement has a then- or an else-part containing code.
class IfStatement final : public Statement {
 public:
  bool HasThenStatement() const { return !then_statement()->IsEmpty(); }
  bool HasElseStatement() const { return !else_statement()->IsEmpty(); }

  Expression* condition() const { return condition_; }
  Statement* then_statement() const { return then_statement_; }
  Statement* else_statement() const { return else_statement_; }

  void set_condition(Expression* e) { condition_ = e; }
  void set_then_statement(Statement* s) { then_statement_ = s; }
  void set_else_statement(Statement* s) { else_statement_ = s; }

  bool IsJump() const {
    return HasThenStatement() && then_statement()->IsJump()
        && HasElseStatement() && else_statement()->IsJump();
  }

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
  explicit EmptyStatement(int pos) : Statement(pos, kEmptyStatement) {}
};


// Delegates to another statement, which may be overwritten.
// This was introduced to implement ES2015 Annex B3.3 for conditionally making
// sloppy-mode block-scoped functions have a var binding, which is changed
// from one statement to another during parsing.
class SloppyBlockFunctionStatement final : public Statement {
 public:
  Statement* statement() const { return statement_; }
  void set_statement(Statement* statement) { statement_ = statement; }

 private:
  friend class AstNodeFactory;

  explicit SloppyBlockFunctionStatement(Statement* statement)
      : Statement(kNoSourcePosition, kSloppyBlockFunctionStatement),
        statement_(statement) {}

  Statement* statement_;
};


class Literal final : public Expression {
 public:
  // Returns true if literal represents a property name (i.e. cannot be parsed
  // as array indices).
  bool IsPropertyName() const { return value_->IsPropertyName(); }

  Handle<String> AsPropertyName() {
    DCHECK(IsPropertyName());
    return Handle<String>::cast(value());
  }

  const AstRawString* AsRawPropertyName() {
    DCHECK(IsPropertyName());
    return value_->AsString();
  }

  Smi* AsSmiLiteral() {
    DCHECK(IsSmiLiteral());
    return raw_value()->AsSmi();
  }

  bool ToBooleanIsTrue() const { return raw_value()->BooleanValue(); }
  bool ToBooleanIsFalse() const { return !raw_value()->BooleanValue(); }

  Handle<Object> value() const { return value_->value(); }
  const AstValue* raw_value() const { return value_; }

  // Support for using Literal as a HashMap key. NOTE: Currently, this works
  // only for string and number literals!
  uint32_t Hash();
  static bool Match(void* literal1, void* literal2);

 private:
  friend class AstNodeFactory;

  Literal(const AstValue* value, int position)
      : Expression(position, kLiteral), value_(value) {}

  const AstValue* value_;
};

// Base class for literals that need space in the type feedback vector.
class MaterializedLiteral : public Expression {
 public:
  void AssignFeedbackSlots(FeedbackVectorSpec* spec, LanguageMode language_mode,
                           FeedbackSlotCache* cache) {
    literal_slot_ = spec->AddLiteralSlot();
  }

  FeedbackSlot literal_slot() const { return literal_slot_; }

  // A Materializedliteral is simple if the values consist of only
  // constants and simple object and array literals.
  bool IsSimple() const;

 private:
  FeedbackSlot literal_slot_;

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
  // if the expression is a materialized literal and is simple return a
  // compile time value as encoded by CompileTimeValue::GetValue().
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
  Expression* key() const { return key_; }
  Expression* value() const { return value_; }
  void set_key(Expression* e) { key_ = e; }
  void set_value(Expression* e) { value_ = e; }

  bool is_computed_name() const { return is_computed_name_; }

  FeedbackSlot GetSlot(int offset = 0) const {
    DCHECK_LT(offset, static_cast<int>(arraysize(slots_)));
    return slots_[offset];
  }

  FeedbackSlot GetStoreDataPropertySlot() const;

  void SetSlot(FeedbackSlot slot, int offset = 0) {
    DCHECK_LT(offset, static_cast<int>(arraysize(slots_)));
    slots_[offset] = slot;
  }

  void SetStoreDataPropertySlot(FeedbackSlot slot);

  bool NeedsSetFunctionName() const;

 protected:
  LiteralProperty(Expression* key, Expression* value, bool is_computed_name)
      : key_(key), value_(value), is_computed_name_(is_computed_name) {}

  Expression* key_;
  Expression* value_;
  FeedbackSlot slots_[2];
  bool is_computed_name_;
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

  // Type feedback information.
  bool IsMonomorphic() const { return !receiver_type_.is_null(); }
  Handle<Map> GetReceiverType() const { return receiver_type_; }

  bool IsCompileTimeValue() const;

  void set_emit_store(bool emit_store);
  bool emit_store() const;

  void set_receiver_type(Handle<Map> map) { receiver_type_ = map; }

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
  Handle<Map> receiver_type_;
};


// An object literal has a boilerplate object that is used
// for minimizing the work when constructing it at runtime.
class ObjectLiteral final : public AggregateLiteral {
 public:
  typedef ObjectLiteralProperty Property;

  Handle<BoilerplateDescription> constant_properties() const {
    DCHECK(!constant_properties_.is_null());
    return constant_properties_;
  }
  int properties_count() const { return boilerplate_properties_; }
  ZoneList<Property*>* properties() const { return properties_; }
  bool has_elements() const { return HasElementsField::decode(bit_field_); }
  bool has_rest_property() const {
    return HasRestPropertyField::decode(bit_field_);
  }
  bool fast_elements() const { return FastElementsField::decode(bit_field_); }
  bool has_null_prototype() const {
    return HasNullPrototypeField::decode(bit_field_);
  }

  // Populate the depth field and flags, returns the depth.
  int InitDepthAndFlags();

  // Get the constant properties fixed array, populating it if necessary.
  Handle<BoilerplateDescription> GetOrBuildConstantProperties(
      Isolate* isolate) {
    if (constant_properties_.is_null()) {
      BuildConstantProperties(isolate);
    }
    return constant_properties();
  }

  // Populate the constant properties fixed array.
  void BuildConstantProperties(Isolate* isolate);

  // Mark all computed expressions that are bound to a key that
  // is shadowed by a later occurrence of the same key. For the
  // marked expressions, no store code is emitted.
  void CalculateEmitStore(Zone* zone);

  // Determines whether the {FastCloneShallowObject} builtin can be used.
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
    Accessors() : getter(NULL), setter(NULL) {}
    ObjectLiteralProperty* getter;
    ObjectLiteralProperty* setter;
  };

  // Object literals need one feedback slot for each non-trivial value, as well
  // as some slots for home objects.
  void AssignFeedbackSlots(FeedbackVectorSpec* spec, LanguageMode language_mode,
                           FeedbackSlotCache* cache);

 private:
  friend class AstNodeFactory;

  ObjectLiteral(ZoneList<Property*>* properties,
                uint32_t boilerplate_properties, int pos,
                bool has_rest_property)
      : AggregateLiteral(pos, kObjectLiteral),
        boilerplate_properties_(boilerplate_properties),
        properties_(properties) {
    bit_field_ |= HasElementsField::encode(false) |
                  HasRestPropertyField::encode(has_rest_property) |
                  FastElementsField::encode(false) |
                  HasNullPrototypeField::encode(false);
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
  Handle<BoilerplateDescription> constant_properties_;
  ZoneList<Property*>* properties_;

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
    if (it->second == NULL) it->second = new (zone_) ObjectLiteral::Accessors();
    return it;
  }

 private:
  Zone* zone_;
};


// An array literal has a literals object that is used
// for minimizing the work when constructing it at runtime.
class ArrayLiteral final : public AggregateLiteral {
 public:
  Handle<ConstantElementsPair> constant_elements() const {
    return constant_elements_;
  }

  ZoneList<Expression*>* values() const { return values_; }

  // Populate the depth field and flags, returns the depth.
  int InitDepthAndFlags();

  // Get the constant elements fixed array, populating it if necessary.
  Handle<ConstantElementsPair> GetOrBuildConstantElements(Isolate* isolate) {
    if (constant_elements_.is_null()) {
      BuildConstantElements(isolate);
    }
    return constant_elements();
  }

  // Populate the constant elements fixed array.
  void BuildConstantElements(Isolate* isolate);

  // Determines whether the {FastCloneShallowArray} builtin can be used.
  bool IsFastCloningSupported() const;

  // Assemble bitfield of flags for the CreateArrayLiteral helper.
  int ComputeFlags(bool disable_mementos = false) const {
    return AggregateLiteral::ComputeFlags(disable_mementos);
  }

  // Provide a mechanism for iterating through values to rewrite spreads.
  ZoneList<Expression*>::iterator FirstSpread() const {
    return (first_spread_index_ >= 0) ? values_->begin() + first_spread_index_
                                      : values_->end();
  }
  ZoneList<Expression*>::iterator EndValue() const { return values_->end(); }

  // Rewind an array literal omitting everything from the first spread on.
  void RewindSpreads();

  void AssignFeedbackSlots(FeedbackVectorSpec* spec, LanguageMode language_mode,
                           FeedbackSlotCache* cache);
  FeedbackSlot LiteralFeedbackSlot() const { return literal_slot_; }

 private:
  friend class AstNodeFactory;

  ArrayLiteral(ZoneList<Expression*>* values, int first_spread_index, int pos)
      : AggregateLiteral(pos, kArrayLiteral),
        first_spread_index_(first_spread_index),
        values_(values) {}

  int first_spread_index_;
  FeedbackSlot literal_slot_;
  Handle<ConstantElementsPair> constant_elements_;
  ZoneList<Expression*>* values_;
};


class VariableProxy final : public Expression {
 public:
  bool IsValidReferenceExpression() const {
    return !is_this() && !is_new_target();
  }

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

  bool is_this() const { return IsThisField::decode(bit_field_); }

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

  // Bind this proxy to the variable var.
  void BindTo(Variable* var);

  bool UsesVariableFeedbackSlot() const {
    return var()->IsUnallocated() || var()->IsLookupSlot();
  }

  void AssignFeedbackSlots(FeedbackVectorSpec* spec, TypeofMode typeof_mode,
                           FeedbackSlotCache* cache);

  FeedbackSlot VariableFeedbackSlot() { return variable_feedback_slot_; }

  void set_next_unresolved(VariableProxy* next) { next_unresolved_ = next; }
  VariableProxy* next_unresolved() { return next_unresolved_; }

 private:
  friend class AstNodeFactory;

  VariableProxy(Variable* var, int start_position);

  VariableProxy(const AstRawString* name, VariableKind variable_kind,
                int start_position)
      : Expression(start_position, kVariableProxy),
        raw_name_(name),
        next_unresolved_(nullptr) {
    bit_field_ |= IsThisField::encode(variable_kind == THIS_VARIABLE) |
                  IsAssignedField::encode(false) |
                  IsResolvedField::encode(false) |
                  HoleCheckModeField::encode(HoleCheckMode::kElided);
  }

  explicit VariableProxy(const VariableProxy* copy_from);

  class IsThisField : public BitField<bool, Expression::kNextBitFieldIndex, 1> {
  };
  class IsAssignedField : public BitField<bool, IsThisField::kNext, 1> {};
  class IsResolvedField : public BitField<bool, IsAssignedField::kNext, 1> {};
  class IsNewTargetField : public BitField<bool, IsResolvedField::kNext, 1> {};
  class HoleCheckModeField
      : public BitField<HoleCheckMode, IsNewTargetField::kNext, 1> {};

  FeedbackSlot variable_feedback_slot_;
  union {
    const AstRawString* raw_name_;  // if !is_resolved_
    Variable* var_;                 // if is_resolved_
  };
  VariableProxy* next_unresolved_;
};


// Left-hand side can only be a property, a global or a (parameter or local)
// slot.
enum LhsKind {
  VARIABLE,
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

  void set_obj(Expression* e) { obj_ = e; }
  void set_key(Expression* e) { key_ = e; }

  bool IsStringAccess() const {
    return IsStringAccessField::decode(bit_field_);
  }

  // Type feedback information.
  bool IsMonomorphic() const { return receiver_types_.length() == 1; }
  SmallMapList* GetReceiverTypes() { return &receiver_types_; }
  KeyedAccessStoreMode GetStoreMode() const { return STANDARD_STORE; }
  IcCheckType GetKeyType() const { return KeyTypeField::decode(bit_field_); }
  bool IsUninitialized() const {
    return !is_for_call() && HasNoTypeInformation();
  }
  bool HasNoTypeInformation() const {
    return GetInlineCacheState() == UNINITIALIZED;
  }
  InlineCacheState GetInlineCacheState() const {
    return InlineCacheStateField::decode(bit_field_);
  }
  void set_is_string_access(bool b) {
    bit_field_ = IsStringAccessField::update(bit_field_, b);
  }
  void set_key_type(IcCheckType key_type) {
    bit_field_ = KeyTypeField::update(bit_field_, key_type);
  }
  void set_inline_cache_state(InlineCacheState state) {
    bit_field_ = InlineCacheStateField::update(bit_field_, state);
  }
  void mark_for_call() {
    bit_field_ = IsForCallField::update(bit_field_, true);
  }
  bool is_for_call() const { return IsForCallField::decode(bit_field_); }

  bool IsSuperAccess() { return obj()->IsSuperPropertyReference(); }

  void AssignFeedbackSlots(FeedbackVectorSpec* spec, LanguageMode language_mode,
                           FeedbackSlotCache* cache) {
    if (key()->IsPropertyName()) {
      property_feedback_slot_ = spec->AddLoadICSlot();
    } else {
      property_feedback_slot_ = spec->AddKeyedLoadICSlot();
    }
  }

  FeedbackSlot PropertyFeedbackSlot() const { return property_feedback_slot_; }

  // Returns the properties assign type.
  static LhsKind GetAssignType(Property* property) {
    if (property == NULL) return VARIABLE;
    bool super_access = property->IsSuperAccess();
    return (property->key()->IsPropertyName())
               ? (super_access ? NAMED_SUPER_PROPERTY : NAMED_PROPERTY)
               : (super_access ? KEYED_SUPER_PROPERTY : KEYED_PROPERTY);
  }

 private:
  friend class AstNodeFactory;

  Property(Expression* obj, Expression* key, int pos)
      : Expression(pos, kProperty), obj_(obj), key_(key) {
    bit_field_ |= IsForCallField::encode(false) |
                  IsStringAccessField::encode(false) |
                  InlineCacheStateField::encode(UNINITIALIZED);
  }

  class IsForCallField
      : public BitField<bool, Expression::kNextBitFieldIndex, 1> {};
  class IsStringAccessField : public BitField<bool, IsForCallField::kNext, 1> {
  };
  class KeyTypeField
      : public BitField<IcCheckType, IsStringAccessField::kNext, 1> {};
  class InlineCacheStateField
      : public BitField<InlineCacheState, KeyTypeField::kNext, 4> {};

  FeedbackSlot property_feedback_slot_;
  Expression* obj_;
  Expression* key_;
  SmallMapList receiver_types_;
};


class Call final : public Expression {
 public:
  Expression* expression() const { return expression_; }
  ZoneList<Expression*>* arguments() const { return arguments_; }

  void set_expression(Expression* e) { expression_ = e; }

  // Type feedback information.
  void AssignFeedbackSlots(FeedbackVectorSpec* spec, LanguageMode language_mode,
                           FeedbackSlotCache* cache);

  FeedbackSlot CallFeedbackICSlot() const { return ic_slot_; }

  SmallMapList* GetReceiverTypes() {
    if (expression()->IsProperty()) {
      return expression()->AsProperty()->GetReceiverTypes();
    }
    return nullptr;
  }

  bool IsMonomorphic() const {
    if (expression()->IsProperty()) {
      return expression()->AsProperty()->IsMonomorphic();
    }
    return !target_.is_null();
  }

  Handle<JSFunction> target() { return target_; }

  void SetKnownGlobalTarget(Handle<JSFunction> target) {
    target_ = target;
    set_is_uninitialized(false);
  }
  void set_target(Handle<JSFunction> target) { target_ = target; }

  bool is_uninitialized() const {
    return IsUninitializedField::decode(bit_field_);
  }
  void set_is_uninitialized(bool b) {
    bit_field_ = IsUninitializedField::update(bit_field_, b);
  }

  bool is_possibly_eval() const {
    return IsPossiblyEvalField::decode(bit_field_);
  }

  bool only_last_arg_is_spread() {
    return !arguments_->is_empty() && arguments_->last()->IsSpread();
  }

  enum CallType {
    GLOBAL_CALL,
    WITH_CALL,
    NAMED_PROPERTY_CALL,
    KEYED_PROPERTY_CALL,
    NAMED_SUPER_PROPERTY_CALL,
    KEYED_SUPER_PROPERTY_CALL,
    SUPER_CALL,
    OTHER_CALL
  };

  enum PossiblyEval {
    IS_POSSIBLY_EVAL,
    NOT_EVAL,
  };

  // Helpers to determine how to handle the call.
  CallType GetCallType() const;

 private:
  friend class AstNodeFactory;

  Call(Expression* expression, ZoneList<Expression*>* arguments, int pos,
       PossiblyEval possibly_eval)
      : Expression(pos, kCall),
        expression_(expression),
        arguments_(arguments) {
    bit_field_ |=
        IsUninitializedField::encode(false) |
        IsPossiblyEvalField::encode(possibly_eval == IS_POSSIBLY_EVAL);

    if (expression->IsProperty()) {
      expression->AsProperty()->mark_for_call();
    }
  }

  class IsUninitializedField
      : public BitField<bool, Expression::kNextBitFieldIndex, 1> {};
  class IsPossiblyEvalField
      : public BitField<bool, IsUninitializedField::kNext, 1> {};

  FeedbackSlot ic_slot_;
  Expression* expression_;
  ZoneList<Expression*>* arguments_;
  Handle<JSFunction> target_;
};


class CallNew final : public Expression {
 public:
  Expression* expression() const { return expression_; }
  ZoneList<Expression*>* arguments() const { return arguments_; }

  void set_expression(Expression* e) { expression_ = e; }

  // Type feedback information.
  void AssignFeedbackSlots(FeedbackVectorSpec* spec, LanguageMode language_mode,
                           FeedbackSlotCache* cache) {
    // CallNew stores feedback in the exact same way as Call. We can
    // piggyback on the type feedback infrastructure for calls.
    callnew_feedback_slot_ = spec->AddCallICSlot();
  }

  FeedbackSlot CallNewFeedbackSlot() {
    DCHECK(!callnew_feedback_slot_.IsInvalid());
    return callnew_feedback_slot_;
  }

  bool IsMonomorphic() const { return IsMonomorphicField::decode(bit_field_); }
  Handle<JSFunction> target() const { return target_; }

  void set_is_monomorphic(bool monomorphic) {
    bit_field_ = IsMonomorphicField::update(bit_field_, monomorphic);
  }
  void set_target(Handle<JSFunction> target) { target_ = target; }
  void SetKnownGlobalTarget(Handle<JSFunction> target) {
    target_ = target;
    set_is_monomorphic(true);
  }

  bool only_last_arg_is_spread() {
    return !arguments_->is_empty() && arguments_->last()->IsSpread();
  }

 private:
  friend class AstNodeFactory;

  CallNew(Expression* expression, ZoneList<Expression*>* arguments, int pos)
      : Expression(pos, kCallNew),
        expression_(expression),
        arguments_(arguments) {
    bit_field_ |= IsMonomorphicField::encode(false);
  }

  FeedbackSlot callnew_feedback_slot_;
  Expression* expression_;
  ZoneList<Expression*>* arguments_;
  Handle<JSFunction> target_;

  class IsMonomorphicField
      : public BitField<bool, Expression::kNextBitFieldIndex, 1> {};
};


// The CallRuntime class does not represent any official JavaScript
// language construct. Instead it is used to call a C or JS function
// with a set of arguments. This is used from the builtins that are
// implemented in JavaScript (see "v8natives.js").
class CallRuntime final : public Expression {
 public:
  ZoneList<Expression*>* arguments() const { return arguments_; }
  bool is_jsruntime() const { return function_ == NULL; }

  int context_index() const {
    DCHECK(is_jsruntime());
    return context_index_;
  }
  void set_context_index(int index) {
    DCHECK(is_jsruntime());
    context_index_ = index;
  }
  const Runtime::Function* function() const {
    DCHECK(!is_jsruntime());
    return function_;
  }

  const char* debug_name();

 private:
  friend class AstNodeFactory;

  CallRuntime(const Runtime::Function* function,
              ZoneList<Expression*>* arguments, int pos)
      : Expression(pos, kCallRuntime),
        function_(function),
        arguments_(arguments) {}
  CallRuntime(int context_index, ZoneList<Expression*>* arguments, int pos)
      : Expression(pos, kCallRuntime),
        context_index_(context_index),
        function_(NULL),
        arguments_(arguments) {}

  int context_index_;
  const Runtime::Function* function_;
  ZoneList<Expression*>* arguments_;
};


class UnaryOperation final : public Expression {
 public:
  Token::Value op() const { return OperatorField::decode(bit_field_); }
  Expression* expression() const { return expression_; }
  void set_expression(Expression* e) { expression_ = e; }

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
  void set_left(Expression* e) { left_ = e; }
  Expression* right() const { return right_; }
  void set_right(Expression* e) { right_ = e; }

  void AssignFeedbackSlots(FeedbackVectorSpec* spec, LanguageMode language_mode,
                           FeedbackSlotCache* cache);

  FeedbackSlot BinaryOperationFeedbackSlot() const { return feedback_slot_; }

  // Returns true if one side is a Smi literal, returning the other side's
  // sub-expression in |subexpr| and the literal Smi in |literal|.
  bool IsSmiLiteralOperation(Expression** subexpr, Smi** literal);

 private:
  friend class AstNodeFactory;

  BinaryOperation(Token::Value op, Expression* left, Expression* right, int pos)
      : Expression(pos, kBinaryOperation), left_(left), right_(right) {
    bit_field_ |= OperatorField::encode(op);
    DCHECK(Token::IsBinaryOp(op));
  }

  FeedbackSlot feedback_slot_;
  Expression* left_;
  Expression* right_;

  class OperatorField
      : public BitField<Token::Value, Expression::kNextBitFieldIndex, 7> {};
};


class CountOperation final : public Expression {
 public:
  bool is_prefix() const { return IsPrefixField::decode(bit_field_); }
  bool is_postfix() const { return !is_prefix(); }

  Token::Value op() const { return TokenField::decode(bit_field_); }
  Token::Value binary_op() {
    return (op() == Token::INC) ? Token::ADD : Token::SUB;
  }

  Expression* expression() const { return expression_; }
  void set_expression(Expression* e) { expression_ = e; }

  bool IsMonomorphic() const { return receiver_types_.length() == 1; }
  SmallMapList* GetReceiverTypes() { return &receiver_types_; }
  IcCheckType GetKeyType() const { return KeyTypeField::decode(bit_field_); }
  KeyedAccessStoreMode GetStoreMode() const {
    return StoreModeField::decode(bit_field_);
  }
  void set_key_type(IcCheckType type) {
    bit_field_ = KeyTypeField::update(bit_field_, type);
  }
  void set_store_mode(KeyedAccessStoreMode mode) {
    bit_field_ = StoreModeField::update(bit_field_, mode);
  }

  // Feedback slot for binary operation is only used by ignition.
  FeedbackSlot CountBinaryOpFeedbackSlot() const {
    return binary_operation_slot_;
  }

  void AssignFeedbackSlots(FeedbackVectorSpec* spec, LanguageMode language_mode,
                           FeedbackSlotCache* cache);
  FeedbackSlot CountSlot() const { return slot_; }

 private:
  friend class AstNodeFactory;

  CountOperation(Token::Value op, bool is_prefix, Expression* expr, int pos)
      : Expression(pos, kCountOperation), expression_(expr) {
    bit_field_ |=
        IsPrefixField::encode(is_prefix) | KeyTypeField::encode(ELEMENT) |
        StoreModeField::encode(STANDARD_STORE) | TokenField::encode(op);
  }

  class IsPrefixField
      : public BitField<bool, Expression::kNextBitFieldIndex, 1> {};
  class KeyTypeField : public BitField<IcCheckType, IsPrefixField::kNext, 1> {};
  class StoreModeField
      : public BitField<KeyedAccessStoreMode, KeyTypeField::kNext, 3> {};
  class TokenField : public BitField<Token::Value, StoreModeField::kNext, 7> {};

  FeedbackSlot slot_;
  FeedbackSlot binary_operation_slot_;
  Expression* expression_;
  SmallMapList receiver_types_;
};


class CompareOperation final : public Expression {
 public:
  Token::Value op() const { return OperatorField::decode(bit_field_); }
  Expression* left() const { return left_; }
  Expression* right() const { return right_; }

  void set_left(Expression* e) { left_ = e; }
  void set_right(Expression* e) { right_ = e; }

  void AssignFeedbackSlots(FeedbackVectorSpec* spec, LanguageMode language_mode,
                           FeedbackSlotCache* cache);

  FeedbackSlot CompareOperationFeedbackSlot() const { return feedback_slot_; }

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

  FeedbackSlot feedback_slot_;
  Expression* left_;
  Expression* right_;

  class OperatorField
      : public BitField<Token::Value, Expression::kNextBitFieldIndex, 7> {};
};


class Spread final : public Expression {
 public:
  Expression* expression() const { return expression_; }
  void set_expression(Expression* e) { expression_ = e; }

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


class Conditional final : public Expression {
 public:
  Expression* condition() const { return condition_; }
  Expression* then_expression() const { return then_expression_; }
  Expression* else_expression() const { return else_expression_; }

  void set_condition(Expression* e) { condition_ = e; }
  void set_then_expression(Expression* e) { then_expression_ = e; }
  void set_else_expression(Expression* e) { else_expression_ = e; }

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


class Assignment final : public Expression {
 public:
  Assignment* AsSimpleAssignment() { return !is_compound() ? this : NULL; }

  Token::Value binary_op() const;

  Token::Value op() const { return TokenField::decode(bit_field_); }
  Expression* target() const { return target_; }
  Expression* value() const { return value_; }

  void set_target(Expression* e) { target_ = e; }
  void set_value(Expression* e) { value_ = e; }

  BinaryOperation* binary_operation() const { return binary_operation_; }

  // This check relies on the definition order of token in token.h.
  bool is_compound() const { return op() > Token::ASSIGN; }

  // Type feedback information.
  bool IsUninitialized() const {
    return IsUninitializedField::decode(bit_field_);
  }
  bool HasNoTypeInformation() {
    return IsUninitializedField::decode(bit_field_);
  }
  bool IsMonomorphic() const { return receiver_types_.length() == 1; }
  SmallMapList* GetReceiverTypes() { return &receiver_types_; }
  IcCheckType GetKeyType() const { return KeyTypeField::decode(bit_field_); }
  KeyedAccessStoreMode GetStoreMode() const {
    return StoreModeField::decode(bit_field_);
  }
  void set_is_uninitialized(bool b) {
    bit_field_ = IsUninitializedField::update(bit_field_, b);
  }
  void set_key_type(IcCheckType key_type) {
    bit_field_ = KeyTypeField::update(bit_field_, key_type);
  }
  void set_store_mode(KeyedAccessStoreMode mode) {
    bit_field_ = StoreModeField::update(bit_field_, mode);
  }

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

  void AssignFeedbackSlots(FeedbackVectorSpec* spec, LanguageMode language_mode,
                           FeedbackSlotCache* cache);
  FeedbackSlot AssignmentSlot() const { return slot_; }

 private:
  friend class AstNodeFactory;

  Assignment(Token::Value op, Expression* target, Expression* value, int pos);

  class IsUninitializedField
      : public BitField<bool, Expression::kNextBitFieldIndex, 1> {};
  class KeyTypeField
      : public BitField<IcCheckType, IsUninitializedField::kNext, 1> {};
  class StoreModeField
      : public BitField<KeyedAccessStoreMode, KeyTypeField::kNext, 3> {};
  class TokenField : public BitField<Token::Value, StoreModeField::kNext, 7> {};
  class LookupHoistingModeField : public BitField<bool, TokenField::kNext, 1> {
  };

  FeedbackSlot slot_;
  Expression* target_;
  Expression* value_;
  BinaryOperation* binary_operation_;
  SmallMapList receiver_types_;
};


// The RewritableExpression class is a wrapper for AST nodes that wait
// for some potential rewriting.  However, even if such nodes are indeed
// rewritten, the RewritableExpression wrapper nodes will survive in the
// final AST and should be just ignored, i.e., they should be treated as
// equivalent to the wrapped nodes.  For this reason and to simplify later
// phases, RewritableExpressions are considered as exceptions of AST nodes
// in the following sense:
//
// 1. IsRewritableExpression and AsRewritableExpression behave as usual.
// 2. All other Is* and As* methods are practically delegated to the
//    wrapped node, i.e. IsArrayLiteral() will return true iff the
//    wrapped node is an array literal.
//
// Furthermore, an invariant that should be respected is that the wrapped
// node is not a RewritableExpression.
class RewritableExpression final : public Expression {
 public:
  Expression* expression() const { return expr_; }
  bool is_rewritten() const { return IsRewrittenField::decode(bit_field_); }

  void Rewrite(Expression* new_expression) {
    DCHECK(!is_rewritten());
    DCHECK_NOT_NULL(new_expression);
    DCHECK(!new_expression->IsRewritableExpression());
    expr_ = new_expression;
    bit_field_ = IsRewrittenField::update(bit_field_, true);
  }

 private:
  friend class AstNodeFactory;

  explicit RewritableExpression(Expression* expression)
      : Expression(expression->position(), kRewritableExpression),
        expr_(expression) {
    bit_field_ |= IsRewrittenField::encode(false);
    DCHECK(!expression->IsRewritableExpression());
  }

  Expression* expr_;

  class IsRewrittenField
      : public BitField<bool, Expression::kNextBitFieldIndex, 1> {};
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
  enum OnAbruptResume { kOnExceptionThrow, kOnExceptionRethrow, kNoControl };

  Expression* expression() const { return expression_; }
  OnAbruptResume on_abrupt_resume() const {
    return OnAbruptResumeField::decode(bit_field_);
  }
  bool rethrow_on_exception() const {
    return on_abrupt_resume() == kOnExceptionRethrow;
  }

  int suspend_id() const { return suspend_id_; }

  void set_expression(Expression* e) { expression_ = e; }
  void set_suspend_id(int id) { suspend_id_ = id; }

  inline bool IsInitialYield() const { return suspend_id_ == 0 && IsYield(); }

 private:
  friend class AstNodeFactory;
  friend class Yield;
  friend class YieldStar;
  friend class Await;

  Suspend(NodeType node_type, Expression* expression, int pos,
          OnAbruptResume on_abrupt_resume)
      : Expression(pos, node_type), suspend_id_(-1), expression_(expression) {
    bit_field_ |= OnAbruptResumeField::encode(on_abrupt_resume);
  }

  int suspend_id_;
  Expression* expression_;

  class OnAbruptResumeField
      : public BitField<OnAbruptResume, Expression::kNextBitFieldIndex, 2> {};
};

class Yield final : public Suspend {
 private:
  friend class AstNodeFactory;
  Yield(Expression* expression, int pos, OnAbruptResume on_abrupt_resume)
      : Suspend(kYield, expression, pos, on_abrupt_resume) {}
};

class YieldStar final : public Suspend {
 public:
  void AssignFeedbackSlots(FeedbackVectorSpec* spec, LanguageMode language_mode,
                           FeedbackSlotCache* cache) {
    load_iterable_iterator_slot_ = spec->AddLoadICSlot();
    load_iterator_return_slot_ = spec->AddLoadICSlot();
    load_iterator_next_slot_ = spec->AddLoadICSlot();
    load_iterator_throw_slot_ = spec->AddLoadICSlot();
    load_output_done_slot_ = spec->AddLoadICSlot();
    load_output_value_slot_ = spec->AddLoadICSlot();
    call_iterable_iterator_slot_ = spec->AddCallICSlot();
    call_iterator_return_slot1_ = spec->AddCallICSlot();
    call_iterator_return_slot2_ = spec->AddCallICSlot();
    call_iterator_next_slot_ = spec->AddCallICSlot();
    call_iterator_throw_slot_ = spec->AddCallICSlot();
  }

  FeedbackSlot load_iterable_iterator_slot() const {
    return load_iterable_iterator_slot_;
  }
  FeedbackSlot load_iterator_return_slot() const {
    return load_iterator_return_slot_;
  }
  FeedbackSlot load_iterator_next_slot() const {
    return load_iterator_next_slot_;
  }
  FeedbackSlot load_iterator_throw_slot() const {
    return load_iterator_throw_slot_;
  }
  FeedbackSlot load_output_done_slot() const { return load_output_done_slot_; }
  FeedbackSlot load_output_value_slot() const {
    return load_output_value_slot_;
  }
  FeedbackSlot call_iterable_iterator_slot() const {
    return call_iterable_iterator_slot_;
  }
  FeedbackSlot call_iterator_return_slot1() const {
    return call_iterator_return_slot1_;
  }
  FeedbackSlot call_iterator_return_slot2() const {
    return call_iterator_return_slot2_;
  }
  FeedbackSlot call_iterator_next_slot() const {
    return call_iterator_next_slot_;
  }
  FeedbackSlot call_iterator_throw_slot() const {
    return call_iterator_throw_slot_;
  }

 private:
  friend class AstNodeFactory;

  YieldStar(Expression* expression, int pos)
      : Suspend(kYieldStar, expression, pos,
                Suspend::OnAbruptResume::kNoControl) {}

  FeedbackSlot load_iterable_iterator_slot_;
  FeedbackSlot load_iterator_return_slot_;
  FeedbackSlot load_iterator_next_slot_;
  FeedbackSlot load_iterator_throw_slot_;
  FeedbackSlot load_output_done_slot_;
  FeedbackSlot load_output_value_slot_;
  FeedbackSlot call_iterable_iterator_slot_;
  FeedbackSlot call_iterator_return_slot1_;
  FeedbackSlot call_iterator_return_slot2_;
  FeedbackSlot call_iterator_next_slot_;
  FeedbackSlot call_iterator_throw_slot_;
};

class Await final : public Suspend {
 private:
  friend class AstNodeFactory;

  Await(Expression* expression, int pos)
      : Suspend(kAwait, expression, pos, Suspend::kOnExceptionRethrow) {}
};

class Throw final : public Expression {
 public:
  Expression* exception() const { return exception_; }
  void set_exception(Expression* e) { exception_ = e; }

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
    kAccessorOrMethod
  };

  enum IdType { kIdTypeInvalid = -1, kIdTypeTopLevel = 0 };

  enum ParameterFlag { kNoDuplicateParameters, kHasDuplicateParameters };

  enum EagerCompileHint { kShouldEagerCompile, kShouldLazyCompile };

  // Empty handle means that the function does not have a shared name (i.e.
  // the name will be set dynamically after creation of the function closure).
  MaybeHandle<String> name() const {
    return raw_name_ ? raw_name_->string() : MaybeHandle<String>();
  }
  Handle<String> name(Isolate* isolate) const {
    return raw_name_ ? raw_name_->string() : isolate->factory()->empty_string();
  }
  bool has_shared_name() const { return raw_name_ != nullptr; }
  const AstConsString* raw_name() const { return raw_name_; }
  void set_raw_name(const AstConsString* name) { raw_name_ = name; }
  DeclarationScope* scope() const { return scope_; }
  ZoneList<Statement*>* body() const { return body_; }
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
  LanguageMode language_mode() const;

  void AssignFeedbackSlots(FeedbackVectorSpec* spec, LanguageMode language_mode,
                           FeedbackSlotCache* cache) {
    literal_feedback_slot_ = spec->AddCreateClosureSlot();
  }

  FeedbackSlot LiteralFeedbackSlot() const { return literal_feedback_slot_; }

  static bool NeedsHomeObject(Expression* expr);

  int expected_property_count() {
    // Not valid for lazy functions.
    DCHECK_NOT_NULL(body_);
    return expected_property_count_;
  }
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

  Handle<String> debug_name() const {
    if (raw_name_ != NULL && !raw_name_->IsEmpty()) {
      return raw_name_->string();
    }
    return inferred_name();
  }

  Handle<String> inferred_name() const {
    if (!inferred_name_.is_null()) {
      DCHECK(raw_inferred_name_ == NULL);
      return inferred_name_;
    }
    if (raw_inferred_name_ != NULL) {
      return raw_inferred_name_->string();
    }
    UNREACHABLE();
  }

  // Only one of {set_inferred_name, set_raw_inferred_name} should be called.
  void set_inferred_name(Handle<String> inferred_name) {
    DCHECK(!inferred_name.is_null());
    inferred_name_ = inferred_name;
    DCHECK(raw_inferred_name_== NULL || raw_inferred_name_->IsEmpty());
    raw_inferred_name_ = NULL;
  }

  void set_raw_inferred_name(const AstConsString* raw_inferred_name) {
    DCHECK(raw_inferred_name != NULL);
    raw_inferred_name_ = raw_inferred_name;
    DCHECK(inferred_name_.is_null());
    inferred_name_ = Handle<String>();
  }

  bool pretenure() const { return Pretenure::decode(bit_field_); }
  void set_pretenure() { bit_field_ = Pretenure::update(bit_field_, true); }

  bool has_duplicate_parameters() const {
    // Not valid for lazy functions.
    DCHECK_NOT_NULL(body_);
    return HasDuplicateParameters::decode(bit_field_);
  }

  // This is used as a heuristic on when to eagerly compile a function
  // literal. We consider the following constructs as hints that the
  // function will be called immediately:
  // - (function() { ... })();
  // - var x = function() { ... }();
  bool ShouldEagerCompile() const;
  void SetShouldEagerCompile();

  // A hint that we expect this function to be called (exactly) once,
  // i.e. we suspect it's an initialization function.
  bool should_be_used_once_hint() const {
    return ShouldNotBeUsedOnceHintField::decode(bit_field_);
  }
  void set_should_be_used_once_hint() {
    bit_field_ = ShouldNotBeUsedOnceHintField::update(bit_field_, true);
  }

  FunctionType function_type() const {
    return FunctionTypeBits::decode(bit_field_);
  }
  FunctionKind kind() const;

  void set_ast_properties(AstProperties* ast_properties) {
    ast_properties_ = *ast_properties;
  }
  int ast_node_count() { return ast_properties_.node_count(); }
  const FeedbackVectorSpec* feedback_vector_spec() const {
    return ast_properties_.get_spec();
  }

  bool must_use_ignition() { return MustUseIgnitionField::decode(bit_field_); }
  void set_must_use_ignition() {
    bit_field_ = MustUseIgnitionField::update(bit_field_, true);
  }

  bool dont_self_optimize() { return DontSelfOptimize::decode(bit_field_); }
  void set_dont_self_optimize() {
    bit_field_ = DontSelfOptimize::update(bit_field_, true);
  }

  bool dont_optimize() { return dont_optimize_reason() != kNoReason; }
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
    return std::max(start_position(), end_position() - (has_braces_ ? 1 : 0));
  }

  int function_literal_id() const { return function_literal_id_; }
  void set_function_literal_id(int function_literal_id) {
    function_literal_id_ = function_literal_id;
  }

  ProducedPreParsedScopeData* produced_preparsed_scope_data() const {
    return produced_preparsed_scope_data_;
  }

  void ReplaceBodyAndScope(FunctionLiteral* other);

 private:
  friend class AstNodeFactory;

  FunctionLiteral(
      Zone* zone, const AstRawString* name, AstValueFactory* ast_value_factory,
      DeclarationScope* scope, ZoneList<Statement*>* body,
      int expected_property_count, int parameter_count, int function_length,
      FunctionType function_type, ParameterFlag has_duplicate_parameters,
      EagerCompileHint eager_compile_hint, int position, bool has_braces,
      int function_literal_id,
      ProducedPreParsedScopeData* produced_preparsed_scope_data = nullptr)
      : Expression(position, kFunctionLiteral),
        expected_property_count_(expected_property_count),
        parameter_count_(parameter_count),
        function_length_(function_length),
        function_token_position_(kNoSourcePosition),
        suspend_count_(0),
        has_braces_(has_braces),
        raw_name_(name ? ast_value_factory->NewConsString(name) : nullptr),
        scope_(scope),
        body_(body),
        raw_inferred_name_(ast_value_factory->empty_cons_string()),
        ast_properties_(zone),
        function_literal_id_(function_literal_id),
        produced_preparsed_scope_data_(produced_preparsed_scope_data) {
    bit_field_ |= FunctionTypeBits::encode(function_type) |
                  Pretenure::encode(false) |
                  HasDuplicateParameters::encode(has_duplicate_parameters ==
                                                 kHasDuplicateParameters) |
                  ShouldNotBeUsedOnceHintField::encode(false) |
                  DontOptimizeReasonField::encode(kNoReason);
    if (eager_compile_hint == kShouldEagerCompile) SetShouldEagerCompile();
    DCHECK_EQ(body == nullptr, expected_property_count < 0);
  }

  class FunctionTypeBits
      : public BitField<FunctionType, Expression::kNextBitFieldIndex, 2> {};
  class Pretenure : public BitField<bool, FunctionTypeBits::kNext, 1> {};
  class HasDuplicateParameters : public BitField<bool, Pretenure::kNext, 1> {};
  class ShouldNotBeUsedOnceHintField
      : public BitField<bool, HasDuplicateParameters::kNext, 1> {};
  class MustUseIgnitionField
      : public BitField<bool, ShouldNotBeUsedOnceHintField::kNext, 1> {};
  // TODO(6409): Remove when Full-Codegen dies.
  class DontSelfOptimize
      : public BitField<bool, MustUseIgnitionField::kNext, 1> {};
  class DontOptimizeReasonField
      : public BitField<BailoutReason, DontSelfOptimize::kNext, 8> {};

  int expected_property_count_;
  int parameter_count_;
  int function_length_;
  int function_token_position_;
  int suspend_count_;
  bool has_braces_;

  const AstConsString* raw_name_;
  DeclarationScope* scope_;
  ZoneList<Statement*>* body_;
  const AstConsString* raw_inferred_name_;
  Handle<String> inferred_name_;
  AstProperties ast_properties_;
  int function_literal_id_;
  FeedbackSlot literal_feedback_slot_;
  ProducedPreParsedScopeData* produced_preparsed_scope_data_;
};

// Property is used for passing information
// about a class literal's properties from the parser to the code generator.
class ClassLiteralProperty final : public LiteralProperty {
 public:
  enum Kind : uint8_t { METHOD, GETTER, SETTER, FIELD };

  Kind kind() const { return kind_; }

  bool is_static() const { return is_static_; }

 private:
  friend class AstNodeFactory;

  ClassLiteralProperty(Expression* key, Expression* value, Kind kind,
                       bool is_static, bool is_computed_name);

  Kind kind_;
  bool is_static_;
};

class ClassLiteral final : public Expression {
 public:
  typedef ClassLiteralProperty Property;

  Scope* scope() const { return scope_; }
  VariableProxy* class_variable_proxy() const { return class_variable_proxy_; }
  Expression* extends() const { return extends_; }
  void set_extends(Expression* e) { extends_ = e; }
  FunctionLiteral* constructor() const { return constructor_; }
  void set_constructor(FunctionLiteral* f) { constructor_ = f; }
  ZoneList<Property*>* properties() const { return properties_; }
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

  // Object literals need one feedback slot for each non-trivial value, as well
  // as some slots for home objects.
  void AssignFeedbackSlots(FeedbackVectorSpec* spec, LanguageMode language_mode,
                           FeedbackSlotCache* cache);

  bool NeedsProxySlot() const {
    return class_variable_proxy() != nullptr &&
           class_variable_proxy()->var()->IsUnallocated();
  }

  FeedbackSlot HomeObjectSlot() const { return home_object_slot_; }
  FeedbackSlot ProxySlot() const { return proxy_slot_; }

 private:
  friend class AstNodeFactory;

  ClassLiteral(Scope* scope, VariableProxy* class_variable_proxy,
               Expression* extends, FunctionLiteral* constructor,
               ZoneList<Property*>* properties, int start_position,
               int end_position, bool has_name_static_property,
               bool has_static_computed_names, bool is_anonymous)
      : Expression(start_position, kClassLiteral),
        end_position_(end_position),
        scope_(scope),
        class_variable_proxy_(class_variable_proxy),
        extends_(extends),
        constructor_(constructor),
        properties_(properties) {
    bit_field_ |= HasNameStaticProperty::encode(has_name_static_property) |
                  HasStaticComputedNames::encode(has_static_computed_names) |
                  IsAnonymousExpression::encode(is_anonymous);
  }

  int end_position_;
  FeedbackSlot home_object_slot_;
  FeedbackSlot proxy_slot_;
  Scope* scope_;
  VariableProxy* class_variable_proxy_;
  Expression* extends_;
  FunctionLiteral* constructor_;
  ZoneList<Property*>* properties_;

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
  v8::Extension* extension() const { return extension_; }
  FeedbackSlot LiteralFeedbackSlot() const { return literal_feedback_slot_; }

  void AssignFeedbackSlots(FeedbackVectorSpec* spec, LanguageMode language_mode,
                           FeedbackSlotCache* cache) {
    // TODO(mvstanton): The FeedbackSlotCache can be adapted
    // to always return the same slot for this case.
    literal_feedback_slot_ = spec->AddCreateClosureSlot();
  }

 private:
  friend class AstNodeFactory;

  NativeFunctionLiteral(const AstRawString* name, v8::Extension* extension,
                        int pos)
      : Expression(pos, kNativeFunctionLiteral),
        name_(name),
        extension_(extension) {}

  FeedbackSlot literal_feedback_slot_;
  const AstRawString* name_;
  v8::Extension* extension_;
};


class ThisFunction final : public Expression {
 private:
  friend class AstNodeFactory;
  explicit ThisFunction(int pos) : Expression(pos, kThisFunction) {}
};


class SuperPropertyReference final : public Expression {
 public:
  VariableProxy* this_var() const { return this_var_; }
  void set_this_var(VariableProxy* v) { this_var_ = v; }
  Expression* home_object() const { return home_object_; }
  void set_home_object(Expression* e) { home_object_ = e; }

 private:
  friend class AstNodeFactory;

  SuperPropertyReference(VariableProxy* this_var, Expression* home_object,
                         int pos)
      : Expression(pos, kSuperPropertyReference),
        this_var_(this_var),
        home_object_(home_object) {
    DCHECK(this_var->is_this());
    DCHECK(home_object->IsProperty());
  }

  VariableProxy* this_var_;
  Expression* home_object_;
};


class SuperCallReference final : public Expression {
 public:
  VariableProxy* this_var() const { return this_var_; }
  void set_this_var(VariableProxy* v) { this_var_ = v; }
  VariableProxy* new_target_var() const { return new_target_var_; }
  void set_new_target_var(VariableProxy* v) { new_target_var_ = v; }
  VariableProxy* this_function_var() const { return this_function_var_; }
  void set_this_function_var(VariableProxy* v) { this_function_var_ = v; }

 private:
  friend class AstNodeFactory;

  SuperCallReference(VariableProxy* this_var, VariableProxy* new_target_var,
                     VariableProxy* this_function_var, int pos)
      : Expression(pos, kSuperCallReference),
        this_var_(this_var),
        new_target_var_(new_target_var),
        this_function_var_(this_function_var) {
    DCHECK(this_var->is_this());
    DCHECK(new_target_var->raw_name()->IsOneByteEqualTo(".new.target"));
    DCHECK(this_function_var->raw_name()->IsOneByteEqualTo(".this_function"));
  }

  VariableProxy* this_var_;
  VariableProxy* new_target_var_;
  VariableProxy* this_function_var_;
};

// This AST Node is used to represent a dynamic import call --
// import(argument).
class ImportCallExpression final : public Expression {
 public:
  Expression* argument() const { return argument_; }
  void set_argument(Expression* argument) { argument_ = argument; }

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

  explicit EmptyParentheses(int pos) : Expression(pos, kEmptyParentheses) {}
};

// Represents the spec operation `GetIterator()`
// (defined at https://tc39.github.io/ecma262/#sec-getiterator). Ignition
// desugars this into a LoadIC / JSLoadNamed, CallIC, and a type-check to
// validate return value of the Symbol.iterator() call.
enum class IteratorType { kNormal, kAsync };
class GetIterator final : public Expression {
 public:
  IteratorType hint() const { return hint_; }

  Expression* iterable() const { return iterable_; }
  void set_iterable(Expression* iterable) { iterable_ = iterable; }

  void AssignFeedbackSlots(FeedbackVectorSpec* spec, LanguageMode language_mode,
                           FeedbackSlotCache* cache) {
    iterator_property_feedback_slot_ = spec->AddLoadICSlot();
    iterator_call_feedback_slot_ = spec->AddCallICSlot();
    if (hint() == IteratorType::kAsync) {
      async_iterator_property_feedback_slot_ = spec->AddLoadICSlot();
      async_iterator_call_feedback_slot_ = spec->AddCallICSlot();
    }
  }

  FeedbackSlot IteratorPropertyFeedbackSlot() const {
    return iterator_property_feedback_slot_;
  }

  FeedbackSlot IteratorCallFeedbackSlot() const {
    return iterator_call_feedback_slot_;
  }

  FeedbackSlot AsyncIteratorPropertyFeedbackSlot() const {
    return async_iterator_property_feedback_slot_;
  }

  FeedbackSlot AsyncIteratorCallFeedbackSlot() const {
    return async_iterator_call_feedback_slot_;
  }

  Expression* iterable_for_call_printer() const {
    return destructured_iterable_ != nullptr ? destructured_iterable_
                                             : iterable_;
  }

 private:
  friend class AstNodeFactory;

  GetIterator(Expression* iterable, Expression* destructured_iterable,
              IteratorType hint, int pos)
      : Expression(pos, kGetIterator),
        hint_(hint),
        iterable_(iterable),
        destructured_iterable_(destructured_iterable) {}

  GetIterator(Expression* iterable, IteratorType hint, int pos)
      : Expression(pos, kGetIterator),
        hint_(hint),
        iterable_(iterable),
        destructured_iterable_(nullptr) {}

  IteratorType hint_;
  Expression* iterable_;

  // iterable_ is the variable proxy, while destructured_iterable_ points to
  // the raw value stored in the variable proxy. This is only used for
  // pretty printing error messages.
  Expression* destructured_iterable_;

  FeedbackSlot iterator_property_feedback_slot_;
  FeedbackSlot iterator_call_feedback_slot_;
  FeedbackSlot async_iterator_property_feedback_slot_;
  FeedbackSlot async_iterator_call_feedback_slot_;
};

// ----------------------------------------------------------------------------
// Basic visitor
// Sub-class should parametrize AstVisitor with itself, e.g.:
//   class SpecificVisitor : public AstVisitor<SpecificVisitor> { ... }

template <class Subclass>
class AstVisitor BASE_EMBEDDED {
 public:
  void Visit(AstNode* node) { impl()->Visit(node); }

  void VisitDeclarations(Declaration::List* declarations) {
    for (Declaration* decl : *declarations) Visit(decl);
  }

  void VisitStatements(ZoneList<Statement*>* statements) {
    for (int i = 0; i < statements->length(); i++) {
      Statement* stmt = statements->at(i);
      Visit(stmt);
      if (stmt->IsJump()) break;
    }
  }

  void VisitExpressions(ZoneList<Expression*>* expressions) {
    for (int i = 0; i < expressions->length(); i++) {
      // The variable statement visiting code may pass NULL expressions
      // to this code. Maybe this should be handled by introducing an
      // undefined expression or literal?  Revisit this code if this
      // changes
      Expression* expression = expressions->at(i);
      if (expression != NULL) Visit(expression);
    }
  }

 protected:
  Subclass* impl() { return static_cast<Subclass*>(this); }
};

#define GENERATE_VISIT_CASE(NodeType)                                   \
  case AstNode::k##NodeType:                                            \
    return this->impl()->Visit##NodeType(static_cast<NodeType*>(node));

#define GENERATE_AST_VISITOR_SWITCH()  \
  switch (node->node_type()) {         \
    AST_NODE_LIST(GENERATE_VISIT_CASE) \
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

#define DEFINE_AST_REWRITER_SUBCLASS_MEMBERS()        \
 public:                                              \
  AstNode* Rewrite(AstNode* node) {                   \
    DCHECK_NULL(replacement_);                        \
    DCHECK_NOT_NULL(node);                            \
    Visit(node);                                      \
    if (HasStackOverflow()) return node;              \
    if (replacement_ == nullptr) return node;         \
    AstNode* result = replacement_;                   \
    replacement_ = nullptr;                           \
    return result;                                    \
  }                                                   \
                                                      \
 private:                                             \
  void InitializeAstRewriter(Isolate* isolate) {      \
    InitializeAstVisitor(isolate);                    \
    replacement_ = nullptr;                           \
  }                                                   \
                                                      \
  void InitializeAstRewriter(uintptr_t stack_limit) { \
    InitializeAstVisitor(stack_limit);                \
    replacement_ = nullptr;                           \
  }                                                   \
                                                      \
  DEFINE_AST_VISITOR_SUBCLASS_MEMBERS();              \
                                                      \
 protected:                                           \
  AstNode* replacement_
// Generic macro for rewriting things; `GET` is the expression to be
// rewritten; `SET` is a command that should do the rewriting, i.e.
// something sensible with the variable called `replacement`.
#define AST_REWRITE(Type, GET, SET)                            \
  do {                                                         \
    DCHECK(!HasStackOverflow());                               \
    DCHECK_NULL(replacement_);                                 \
    Visit(GET);                                                \
    if (HasStackOverflow()) return;                            \
    if (replacement_ == nullptr) break;                        \
    Type* replacement = reinterpret_cast<Type*>(replacement_); \
    do {                                                       \
      SET;                                                     \
    } while (false);                                           \
    replacement_ = nullptr;                                    \
  } while (false)

// Macro for rewriting object properties; it assumes that `object` has
// `property` with a public getter and setter.
#define AST_REWRITE_PROPERTY(Type, object, property)                        \
  do {                                                                      \
    auto _obj = (object);                                                   \
    AST_REWRITE(Type, _obj->property(), _obj->set_##property(replacement)); \
  } while (false)

// Macro for rewriting list elements; it assumes that `list` has methods
// `at` and `Set`.
#define AST_REWRITE_LIST_ELEMENT(Type, list, index)                        \
  do {                                                                     \
    auto _list = (list);                                                   \
    auto _index = (index);                                                 \
    AST_REWRITE(Type, _list->at(_index), _list->Set(_index, replacement)); \
  } while (false)


// ----------------------------------------------------------------------------
// AstNode factory

class AstNodeFactory final BASE_EMBEDDED {
 public:
  AstNodeFactory(AstValueFactory* ast_value_factory, Zone* zone)
      : zone_(zone), ast_value_factory_(ast_value_factory) {}

  AstValueFactory* ast_value_factory() const { return ast_value_factory_; }
  void set_ast_value_factory(AstValueFactory* ast_value_factory) {
    ast_value_factory_ = ast_value_factory;
  }

  VariableDeclaration* NewVariableDeclaration(VariableProxy* proxy,
                                              Scope* scope, int pos) {
    return new (zone_) VariableDeclaration(proxy, scope, pos);
  }

  FunctionDeclaration* NewFunctionDeclaration(VariableProxy* proxy,
                                              FunctionLiteral* fun,
                                              Scope* scope, int pos) {
    return new (zone_) FunctionDeclaration(proxy, fun, scope, pos);
  }

  Block* NewBlock(ZoneList<const AstRawString*>* labels, int capacity,
                  bool ignore_completion_value, int pos) {
    return new (zone_)
        Block(zone_, labels, capacity, ignore_completion_value, pos);
  }

#define STATEMENT_WITH_LABELS(NodeType)                                     \
  NodeType* New##NodeType(ZoneList<const AstRawString*>* labels, int pos) { \
    return new (zone_) NodeType(labels, pos);                               \
  }
  STATEMENT_WITH_LABELS(DoWhileStatement)
  STATEMENT_WITH_LABELS(WhileStatement)
  STATEMENT_WITH_LABELS(ForStatement)
  STATEMENT_WITH_LABELS(SwitchStatement)
#undef STATEMENT_WITH_LABELS

  ForEachStatement* NewForEachStatement(ForEachStatement::VisitMode visit_mode,
                                        ZoneList<const AstRawString*>* labels,
                                        int pos) {
    switch (visit_mode) {
      case ForEachStatement::ENUMERATE: {
        return new (zone_) ForInStatement(labels, pos);
      }
      case ForEachStatement::ITERATE: {
        return new (zone_) ForOfStatement(labels, pos);
      }
    }
    UNREACHABLE();
  }

  ForOfStatement* NewForOfStatement(ZoneList<const AstRawString*>* labels,
                                    int pos) {
    return new (zone_) ForOfStatement(labels, pos);
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

  EmptyStatement* NewEmptyStatement(int pos) {
    return new (zone_) EmptyStatement(pos);
  }

  SloppyBlockFunctionStatement* NewSloppyBlockFunctionStatement() {
    return new (zone_)
        SloppyBlockFunctionStatement(NewEmptyStatement(kNoSourcePosition));
  }

  CaseClause* NewCaseClause(Expression* label, ZoneList<Statement*>* statements,
                            int pos) {
    return new (zone_) CaseClause(label, statements, pos);
  }

  Literal* NewStringLiteral(const AstRawString* string, int pos) {
    return new (zone_) Literal(ast_value_factory_->NewString(string), pos);
  }

  // A JavaScript symbol (ECMA-262 edition 6).
  Literal* NewSymbolLiteral(AstSymbol symbol, int pos) {
    return new (zone_) Literal(ast_value_factory_->NewSymbol(symbol), pos);
  }

  Literal* NewNumberLiteral(double number, int pos) {
    return new (zone_) Literal(ast_value_factory_->NewNumber(number), pos);
  }

  Literal* NewSmiLiteral(uint32_t number, int pos) {
    return new (zone_) Literal(ast_value_factory_->NewSmi(number), pos);
  }

  Literal* NewBooleanLiteral(bool b, int pos) {
    return new (zone_) Literal(ast_value_factory_->NewBoolean(b), pos);
  }

  Literal* NewNullLiteral(int pos) {
    return new (zone_) Literal(ast_value_factory_->NewNull(), pos);
  }

  Literal* NewUndefinedLiteral(int pos) {
    return new (zone_) Literal(ast_value_factory_->NewUndefined(), pos);
  }

  Literal* NewTheHoleLiteral(int pos) {
    return new (zone_) Literal(ast_value_factory_->NewTheHole(), pos);
  }

  ObjectLiteral* NewObjectLiteral(
      ZoneList<ObjectLiteral::Property*>* properties,
      uint32_t boilerplate_properties, int pos, bool has_rest_property) {
    return new (zone_) ObjectLiteral(properties, boilerplate_properties, pos,
                                     has_rest_property);
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

  ArrayLiteral* NewArrayLiteral(ZoneList<Expression*>* values,
                                int pos) {
    return new (zone_) ArrayLiteral(values, -1, pos);
  }

  ArrayLiteral* NewArrayLiteral(ZoneList<Expression*>* values,
                                int first_spread_index, int pos) {
    return new (zone_) ArrayLiteral(values, first_spread_index, pos);
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

  Call* NewCall(Expression* expression, ZoneList<Expression*>* arguments,
                int pos, Call::PossiblyEval possibly_eval = Call::NOT_EVAL) {
    return new (zone_) Call(expression, arguments, pos, possibly_eval);
  }

  CallNew* NewCallNew(Expression* expression,
                      ZoneList<Expression*>* arguments,
                      int pos) {
    return new (zone_) CallNew(expression, arguments, pos);
  }

  CallRuntime* NewCallRuntime(Runtime::FunctionId id,
                              ZoneList<Expression*>* arguments, int pos) {
    return new (zone_) CallRuntime(Runtime::FunctionForId(id), arguments, pos);
  }

  CallRuntime* NewCallRuntime(const Runtime::Function* function,
                              ZoneList<Expression*>* arguments, int pos) {
    return new (zone_) CallRuntime(function, arguments, pos);
  }

  CallRuntime* NewCallRuntime(int context_index,
                              ZoneList<Expression*>* arguments, int pos) {
    return new (zone_) CallRuntime(context_index, arguments, pos);
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

  Conditional* NewConditional(Expression* condition,
                              Expression* then_expression,
                              Expression* else_expression,
                              int position) {
    return new (zone_)
        Conditional(condition, then_expression, else_expression, position);
  }

  RewritableExpression* NewRewritableExpression(Expression* expression) {
    DCHECK_NOT_NULL(expression);
    return new (zone_) RewritableExpression(expression);
  }

  Assignment* NewAssignment(Token::Value op,
                            Expression* target,
                            Expression* value,
                            int pos) {
    DCHECK(Token::IsAssignmentOp(op));

    if (op != Token::INIT && target->IsVariableProxy()) {
      target->AsVariableProxy()->set_is_assigned();
    }

    Assignment* assign = new (zone_) Assignment(op, target, value, pos);
    if (assign->is_compound()) {
      assign->binary_operation_ =
          NewBinaryOperation(assign->binary_op(), target, value, pos + 1);
    }
    return assign;
  }

  Suspend* NewYield(Expression* expression, int pos,
                    Suspend::OnAbruptResume on_abrupt_resume) {
    if (!expression) expression = NewUndefinedLiteral(pos);
    return new (zone_) Yield(expression, pos, on_abrupt_resume);
  }

  YieldStar* NewYieldStar(Expression* expression, int pos) {
    DCHECK_NOT_NULL(expression);
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
      ZoneList<Statement*>* body, int expected_property_count,
      int parameter_count, int function_length,
      FunctionLiteral::ParameterFlag has_duplicate_parameters,
      FunctionLiteral::FunctionType function_type,
      FunctionLiteral::EagerCompileHint eager_compile_hint, int position,
      bool has_braces, int function_literal_id,
      ProducedPreParsedScopeData* produced_preparsed_scope_data = nullptr) {
    return new (zone_) FunctionLiteral(
        zone_, name, ast_value_factory_, scope, body, expected_property_count,
        parameter_count, function_length, function_type,
        has_duplicate_parameters, eager_compile_hint, position, has_braces,
        function_literal_id, produced_preparsed_scope_data);
  }

  // Creates a FunctionLiteral representing a top-level script, the
  // result of an eval (top-level or otherwise), or the result of calling
  // the Function constructor.
  FunctionLiteral* NewScriptOrEvalFunctionLiteral(DeclarationScope* scope,
                                                  ZoneList<Statement*>* body,
                                                  int expected_property_count,
                                                  int parameter_count) {
    return new (zone_) FunctionLiteral(
        zone_, ast_value_factory_->empty_string(), ast_value_factory_, scope,
        body, expected_property_count, parameter_count, parameter_count,
        FunctionLiteral::kAnonymousExpression,
        FunctionLiteral::kNoDuplicateParameters,
        FunctionLiteral::kShouldLazyCompile, 0, true,
        FunctionLiteral::kIdTypeTopLevel);
  }

  ClassLiteral::Property* NewClassLiteralProperty(
      Expression* key, Expression* value, ClassLiteralProperty::Kind kind,
      bool is_static, bool is_computed_name) {
    return new (zone_)
        ClassLiteral::Property(key, value, kind, is_static, is_computed_name);
  }

  ClassLiteral* NewClassLiteral(Scope* scope, VariableProxy* proxy,
                                Expression* extends,
                                FunctionLiteral* constructor,
                                ZoneList<ClassLiteral::Property*>* properties,
                                int start_position, int end_position,
                                bool has_name_static_property,
                                bool has_static_computed_names,
                                bool is_anonymous) {
    return new (zone_)
        ClassLiteral(scope, proxy, extends, constructor, properties,
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

  ThisFunction* NewThisFunction(int pos) {
    return new (zone_) ThisFunction(pos);
  }

  SuperPropertyReference* NewSuperPropertyReference(VariableProxy* this_var,
                                                    Expression* home_object,
                                                    int pos) {
    return new (zone_) SuperPropertyReference(this_var, home_object, pos);
  }

  SuperCallReference* NewSuperCallReference(VariableProxy* this_var,
                                            VariableProxy* new_target_var,
                                            VariableProxy* this_function_var,
                                            int pos) {
    return new (zone_)
        SuperCallReference(this_var, new_target_var, this_function_var, pos);
  }

  EmptyParentheses* NewEmptyParentheses(int pos) {
    return new (zone_) EmptyParentheses(pos);
  }

  GetIterator* NewGetIterator(Expression* iterable,
                              Expression* destructured_iterable,
                              IteratorType hint, int pos) {
    return new (zone_) GetIterator(iterable, destructured_iterable, hint, pos);
  }

  GetIterator* NewGetIterator(Expression* iterable, IteratorType hint,
                              int pos) {
    return new (zone_) GetIterator(iterable, hint, pos);
  }

  ImportCallExpression* NewImportCallExpression(Expression* args, int pos) {
    return new (zone_) ImportCallExpression(args, pos);
  }

  Zone* zone() const { return zone_; }
  void set_zone(Zone* zone) { zone_ = zone; }

 private:
  // This zone may be deallocated upon returning from parsing a function body
  // which we can guarantee is not going to be compiled or have its AST
  // inspected.
  // See ParseFunctionLiteral in parser.cc for preconditions.
  Zone* zone_;
  AstValueFactory* ast_value_factory_;
};


// Type testing & conversion functions overridden by concrete subclasses.
// Inline functions for AstNode.

#define DECLARE_NODE_FUNCTIONS(type)                                          \
  bool AstNode::Is##type() const {                                            \
    NodeType mine = node_type();                                              \
    if (mine == AstNode::kRewritableExpression &&                             \
        AstNode::k##type != AstNode::kRewritableExpression)                   \
      mine = reinterpret_cast<const RewritableExpression*>(this)              \
                 ->expression()                                               \
                 ->node_type();                                               \
    return mine == AstNode::k##type;                                          \
  }                                                                           \
  type* AstNode::As##type() {                                                 \
    NodeType mine = node_type();                                              \
    AstNode* result = this;                                                   \
    if (mine == AstNode::kRewritableExpression &&                             \
        AstNode::k##type != AstNode::kRewritableExpression) {                 \
      result =                                                                \
          reinterpret_cast<const RewritableExpression*>(this)->expression();  \
      mine = result->node_type();                                             \
    }                                                                         \
    return mine == AstNode::k##type ? reinterpret_cast<type*>(result) : NULL; \
  }                                                                           \
  const type* AstNode::As##type() const {                                     \
    NodeType mine = node_type();                                              \
    const AstNode* result = this;                                             \
    if (mine == AstNode::kRewritableExpression &&                             \
        AstNode::k##type != AstNode::kRewritableExpression) {                 \
      result =                                                                \
          reinterpret_cast<const RewritableExpression*>(this)->expression();  \
      mine = result->node_type();                                             \
    }                                                                         \
    return mine == AstNode::k##type ? reinterpret_cast<const type*>(result)   \
                                    : NULL;                                   \
  }
AST_NODE_LIST(DECLARE_NODE_FUNCTIONS)
#undef DECLARE_NODE_FUNCTIONS

}  // namespace internal
}  // namespace v8

#endif  // V8_AST_AST_H_
