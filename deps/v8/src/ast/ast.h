// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_AST_AST_H_
#define V8_AST_AST_H_

#include "src/assembler.h"
#include "src/ast/ast-value-factory.h"
#include "src/ast/modules.h"
#include "src/ast/variables.h"
#include "src/bailout-reason.h"
#include "src/base/flags.h"
#include "src/base/smart-pointers.h"
#include "src/factory.h"
#include "src/isolate.h"
#include "src/list.h"
#include "src/parsing/token.h"
#include "src/runtime/runtime.h"
#include "src/small-pointer-list.h"
#include "src/types.h"
#include "src/utils.h"

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
  V(FunctionDeclaration)         \
  V(ImportDeclaration)           \
  V(ExportDeclaration)

#define STATEMENT_NODE_LIST(V)    \
  V(Block)                        \
  V(ExpressionStatement)          \
  V(EmptyStatement)               \
  V(SloppyBlockFunctionStatement) \
  V(IfStatement)                  \
  V(ContinueStatement)            \
  V(BreakStatement)               \
  V(ReturnStatement)              \
  V(WithStatement)                \
  V(SwitchStatement)              \
  V(DoWhileStatement)             \
  V(WhileStatement)               \
  V(ForStatement)                 \
  V(ForInStatement)               \
  V(ForOfStatement)               \
  V(TryCatchStatement)            \
  V(TryFinallyStatement)          \
  V(DebuggerStatement)

#define EXPRESSION_NODE_LIST(V) \
  V(FunctionLiteral)            \
  V(ClassLiteral)               \
  V(NativeFunctionLiteral)      \
  V(Conditional)                \
  V(VariableProxy)              \
  V(Literal)                    \
  V(RegExpLiteral)              \
  V(ObjectLiteral)              \
  V(ArrayLiteral)               \
  V(Assignment)                 \
  V(Yield)                      \
  V(Throw)                      \
  V(Property)                   \
  V(Call)                       \
  V(CallNew)                    \
  V(CallRuntime)                \
  V(UnaryOperation)             \
  V(CountOperation)             \
  V(BinaryOperation)            \
  V(CompareOperation)           \
  V(Spread)                     \
  V(ThisFunction)               \
  V(SuperPropertyReference)     \
  V(SuperCallReference)         \
  V(CaseClause)                 \
  V(EmptyParentheses)           \
  V(DoExpression)               \
  V(RewritableAssignmentExpression)

#define AST_NODE_LIST(V)                        \
  DECLARATION_NODE_LIST(V)                      \
  STATEMENT_NODE_LIST(V)                        \
  EXPRESSION_NODE_LIST(V)

// Forward declarations
class AstNodeFactory;
class AstVisitor;
class Declaration;
class Module;
class BreakableStatement;
class Expression;
class IterationStatement;
class MaterializedLiteral;
class Statement;
class TypeFeedbackOracle;

#define DEF_FORWARD_DECLARATION(type) class type;
AST_NODE_LIST(DEF_FORWARD_DECLARATION)
#undef DEF_FORWARD_DECLARATION


// Typedef only introduced to avoid unreadable code.
typedef ZoneList<Handle<String>> ZoneStringList;
typedef ZoneList<Handle<Object>> ZoneObjectList;


#define DECLARE_NODE_TYPE(type)                                          \
  void Accept(AstVisitor* v) override;                                   \
  AstNode::NodeType node_type() const final { return AstNode::k##type; } \
  friend class AstNodeFactory;


class FeedbackVectorSlotCache {
 public:
  explicit FeedbackVectorSlotCache(Zone* zone)
      : zone_(zone),
        hash_map_(HashMap::PointersMatch, ZoneHashMap::kDefaultHashMapCapacity,
                  ZoneAllocationPolicy(zone)) {}

  void Put(Variable* variable, FeedbackVectorSlot slot) {
    ZoneHashMap::Entry* entry = hash_map_.LookupOrInsert(
        variable, ComputePointerHash(variable), ZoneAllocationPolicy(zone_));
    entry->value = reinterpret_cast<void*>(slot.ToInt());
  }

  ZoneHashMap::Entry* Get(Variable* variable) const {
    return hash_map_.Lookup(variable, ComputePointerHash(variable));
  }

 private:
  Zone* zone_;
  ZoneHashMap hash_map_;
};


class AstProperties final BASE_EMBEDDED {
 public:
  enum Flag {
    kNoFlags = 0,
    kDontSelfOptimize = 1 << 0,
    kDontCrankshaft = 1 << 1
  };

  typedef base::Flags<Flag> Flags;

  explicit AstProperties(Zone* zone) : node_count_(0), spec_(zone) {}

  Flags& flags() { return flags_; }
  Flags flags() const { return flags_; }
  int node_count() { return node_count_; }
  void add_node_count(int count) { node_count_ += count; }

  const FeedbackVectorSpec* get_spec() const { return &spec_; }
  FeedbackVectorSpec* get_spec() { return &spec_; }

 private:
  Flags flags_;
  int node_count_;
  FeedbackVectorSpec spec_;
};

DEFINE_OPERATORS_FOR_FLAGS(AstProperties::Flags)


class AstNode: public ZoneObject {
 public:
#define DECLARE_TYPE_ENUM(type) k##type,
  enum NodeType {
    AST_NODE_LIST(DECLARE_TYPE_ENUM)
    kInvalid = -1
  };
#undef DECLARE_TYPE_ENUM

  void* operator new(size_t size, Zone* zone) { return zone->New(size); }

  explicit AstNode(int position): position_(position) {}
  virtual ~AstNode() {}

  virtual void Accept(AstVisitor* v) = 0;
  virtual NodeType node_type() const = 0;
  int position() const { return position_; }

  // Type testing & conversion functions overridden by concrete subclasses.
#define DECLARE_NODE_FUNCTIONS(type) \
  bool Is##type() const { return node_type() == AstNode::k##type; } \
  type* As##type() { \
    return Is##type() ? reinterpret_cast<type*>(this) : NULL; \
  } \
  const type* As##type() const { \
    return Is##type() ? reinterpret_cast<const type*>(this) : NULL; \
  }
  AST_NODE_LIST(DECLARE_NODE_FUNCTIONS)
#undef DECLARE_NODE_FUNCTIONS

  virtual BreakableStatement* AsBreakableStatement() { return NULL; }
  virtual IterationStatement* AsIterationStatement() { return NULL; }
  virtual MaterializedLiteral* AsMaterializedLiteral() { return NULL; }

  // The interface for feedback slots, with default no-op implementations for
  // node types which don't actually have this. Note that this is conceptually
  // not really nice, but multiple inheritance would introduce yet another
  // vtable entry per node, something we don't want for space reasons.
  virtual void AssignFeedbackVectorSlots(Isolate* isolate,
                                         FeedbackVectorSpec* spec,
                                         FeedbackVectorSlotCache* cache) {}

 private:
  // Hidden to prevent accidental usage. It would have to load the
  // current zone from the TLS.
  void* operator new(size_t size);

  friend class CaseClause;  // Generates AST IDs.

  int position_;
};


class Statement : public AstNode {
 public:
  explicit Statement(Zone* zone, int position) : AstNode(position) {}

  bool IsEmpty() { return AsEmptyStatement() != NULL; }
  virtual bool IsJump() const { return false; }
  virtual void MarkTail() {}
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

  void AddMapIfMissing(Handle<Map> map, Zone* zone) {
    if (!Map::TryUpdate(map).ToHandle(&map)) return;
    for (int i = 0; i < length(); ++i) {
      if (at(i).is_identical_to(map)) return;
    }
    Add(map, zone);
  }

  void FilterForPossibleTransitions(Map* root_map) {
    for (int i = list_.length() - 1; i >= 0; i--) {
      if (at(i)->FindRootMap() != root_map) {
        list_.RemoveElement(list_.at(i));
      }
    }
  }

  void Add(Handle<Map> handle, Zone* zone) {
    list_.Add(handle.location(), zone);
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

  // Mark this expression as being in tail position.
  virtual void MarkTail() {}

  // True iff the expression is a valid reference expression.
  virtual bool IsValidReferenceExpression() const { return false; }

  // Helpers for ToBoolean conversion.
  virtual bool ToBooleanIsTrue() const { return false; }
  virtual bool ToBooleanIsFalse() const { return false; }

  // Symbols that cannot be parsed as array indices are considered property
  // names.  We do not treat symbols that can be array indexes as property
  // names because [] for string objects is handled only by keyed ICs.
  virtual bool IsPropertyName() const { return false; }

  // True iff the expression is a literal represented as a smi.
  bool IsSmiLiteral() const;

  // True iff the expression is a string literal.
  bool IsStringLiteral() const;

  // True iff the expression is the null literal.
  bool IsNullLiteral() const;

  // True if we can prove that the expression is the undefined literal.
  bool IsUndefinedLiteral(Isolate* isolate) const;

  // True iff the expression is a valid target for an assignment.
  bool IsValidReferenceExpressionOrThis() const;

  // Expression type bounds
  Bounds bounds() const { return bounds_; }
  void set_bounds(Bounds bounds) { bounds_ = bounds; }

  // Type feedback information for assignments and properties.
  virtual bool IsMonomorphic() {
    UNREACHABLE();
    return false;
  }
  virtual SmallMapList* GetReceiverTypes() {
    UNREACHABLE();
    return NULL;
  }
  virtual KeyedAccessStoreMode GetStoreMode() const {
    UNREACHABLE();
    return STANDARD_STORE;
  }
  virtual IcCheckType GetKeyType() const {
    UNREACHABLE();
    return ELEMENT;
  }

  // TODO(rossberg): this should move to its own AST node eventually.
  virtual void RecordToBooleanTypeFeedback(TypeFeedbackOracle* oracle);
  uint16_t to_boolean_types() const {
    return ToBooleanTypesField::decode(bit_field_);
  }

  void set_base_id(int id) { base_id_ = id; }
  static int num_ids() { return parent_num_ids() + 2; }
  BailoutId id() const { return BailoutId(local_id(0)); }
  TypeFeedbackId test_id() const { return TypeFeedbackId(local_id(1)); }

  // Parenthesized expressions in the form `( Expression )`.
  void set_is_parenthesized() {
    bit_field_ = ParenthesizedField::update(bit_field_, true);
  }
  bool is_parenthesized() const {
    return ParenthesizedField::decode(bit_field_);
  }

 protected:
  Expression(Zone* zone, int pos)
      : AstNode(pos),
        base_id_(BailoutId::None().ToInt()),
        bounds_(Bounds::Unbounded()),
        bit_field_(0) {}
  static int parent_num_ids() { return 0; }
  void set_to_boolean_types(uint16_t types) {
    bit_field_ = ToBooleanTypesField::update(bit_field_, types);
  }

  int base_id() const {
    DCHECK(!BailoutId(base_id_).IsNone());
    return base_id_;
  }

 private:
  int local_id(int n) const { return base_id() + parent_num_ids() + n; }

  int base_id_;
  Bounds bounds_;
  class ToBooleanTypesField : public BitField16<uint16_t, 0, 9> {};
  class ParenthesizedField
      : public BitField16<bool, ToBooleanTypesField::kNext, 1> {};
  uint16_t bit_field_;
  // Ends with 16-bit field; deriving classes in turn begin with
  // 16-bit fields for optimum packing efficiency.
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

  // Type testing & conversion.
  BreakableStatement* AsBreakableStatement() final { return this; }

  // Code generation
  Label* break_target() { return &break_target_; }

  // Testers.
  bool is_target_for_anonymous() const {
    return breakable_type_ == TARGET_FOR_ANONYMOUS;
  }

  void set_base_id(int id) { base_id_ = id; }
  static int num_ids() { return parent_num_ids() + 2; }
  BailoutId EntryId() const { return BailoutId(local_id(0)); }
  BailoutId ExitId() const { return BailoutId(local_id(1)); }

 protected:
  BreakableStatement(Zone* zone, ZoneList<const AstRawString*>* labels,
                     BreakableType breakable_type, int position)
      : Statement(zone, position),
        labels_(labels),
        breakable_type_(breakable_type),
        base_id_(BailoutId::None().ToInt()) {
    DCHECK(labels == NULL || labels->length() > 0);
  }
  static int parent_num_ids() { return 0; }

  int base_id() const {
    DCHECK(!BailoutId(base_id_).IsNone());
    return base_id_;
  }

 private:
  int local_id(int n) const { return base_id() + parent_num_ids() + n; }

  ZoneList<const AstRawString*>* labels_;
  BreakableType breakable_type_;
  Label break_target_;
  int base_id_;
};


class Block final : public BreakableStatement {
 public:
  DECLARE_NODE_TYPE(Block)

  ZoneList<Statement*>* statements() { return &statements_; }
  bool ignore_completion_value() const { return ignore_completion_value_; }

  static int num_ids() { return parent_num_ids() + 1; }
  BailoutId DeclsId() const { return BailoutId(local_id(0)); }

  bool IsJump() const override {
    return !statements_.is_empty() && statements_.last()->IsJump()
        && labels() == NULL;  // Good enough as an approximation...
  }

  void MarkTail() override {
    if (!statements_.is_empty()) statements_.last()->MarkTail();
  }

  Scope* scope() const { return scope_; }
  void set_scope(Scope* scope) { scope_ = scope; }

 protected:
  Block(Zone* zone, ZoneList<const AstRawString*>* labels, int capacity,
        bool ignore_completion_value, int pos)
      : BreakableStatement(zone, labels, TARGET_FOR_NAMED_ONLY, pos),
        statements_(capacity, zone),
        ignore_completion_value_(ignore_completion_value),
        scope_(NULL) {}
  static int parent_num_ids() { return BreakableStatement::num_ids(); }

 private:
  int local_id(int n) const { return base_id() + parent_num_ids() + n; }

  ZoneList<Statement*> statements_;
  bool ignore_completion_value_;
  Scope* scope_;
};


class DoExpression final : public Expression {
 public:
  DECLARE_NODE_TYPE(DoExpression)

  Block* block() { return block_; }
  void set_block(Block* b) { block_ = b; }
  VariableProxy* result() { return result_; }
  void set_result(VariableProxy* v) { result_ = v; }

  void MarkTail() override { block_->MarkTail(); }

 protected:
  DoExpression(Zone* zone, Block* block, VariableProxy* result, int pos)
      : Expression(zone, pos), block_(block), result_(result) {
    DCHECK_NOT_NULL(block_);
    DCHECK_NOT_NULL(result_);
  }
  static int parent_num_ids() { return Expression::num_ids(); }

 private:
  int local_id(int n) const { return base_id() + parent_num_ids() + n; }

  Block* block_;
  VariableProxy* result_;
};


class Declaration : public AstNode {
 public:
  VariableProxy* proxy() const { return proxy_; }
  VariableMode mode() const { return mode_; }
  Scope* scope() const { return scope_; }
  virtual InitializationFlag initialization() const = 0;
  virtual bool IsInlineable() const;

 protected:
  Declaration(Zone* zone, VariableProxy* proxy, VariableMode mode, Scope* scope,
              int pos)
      : AstNode(pos), mode_(mode), proxy_(proxy), scope_(scope) {
    DCHECK(IsDeclaredVariableMode(mode));
  }

 private:
  VariableMode mode_;
  VariableProxy* proxy_;

  // Nested scope from which the declaration originated.
  Scope* scope_;
};


class VariableDeclaration final : public Declaration {
 public:
  DECLARE_NODE_TYPE(VariableDeclaration)

  InitializationFlag initialization() const override {
    return mode() == VAR ? kCreatedInitialized : kNeedsInitialization;
  }

  bool is_class_declaration() const { return is_class_declaration_; }

  // VariableDeclarations can be grouped into consecutive declaration
  // groups. Each VariableDeclaration is associated with the start position of
  // the group it belongs to. The positions are used for strong mode scope
  // checks for classes and functions.
  int declaration_group_start() const { return declaration_group_start_; }

 protected:
  VariableDeclaration(Zone* zone, VariableProxy* proxy, VariableMode mode,
                      Scope* scope, int pos, bool is_class_declaration = false,
                      int declaration_group_start = -1)
      : Declaration(zone, proxy, mode, scope, pos),
        is_class_declaration_(is_class_declaration),
        declaration_group_start_(declaration_group_start) {}

  bool is_class_declaration_;
  int declaration_group_start_;
};


class FunctionDeclaration final : public Declaration {
 public:
  DECLARE_NODE_TYPE(FunctionDeclaration)

  FunctionLiteral* fun() const { return fun_; }
  void set_fun(FunctionLiteral* f) { fun_ = f; }
  InitializationFlag initialization() const override {
    return kCreatedInitialized;
  }
  bool IsInlineable() const override;

 protected:
  FunctionDeclaration(Zone* zone,
                      VariableProxy* proxy,
                      VariableMode mode,
                      FunctionLiteral* fun,
                      Scope* scope,
                      int pos)
      : Declaration(zone, proxy, mode, scope, pos),
        fun_(fun) {
    DCHECK(mode == VAR || mode == LET || mode == CONST);
    DCHECK(fun != NULL);
  }

 private:
  FunctionLiteral* fun_;
};


class ImportDeclaration final : public Declaration {
 public:
  DECLARE_NODE_TYPE(ImportDeclaration)

  const AstRawString* import_name() const { return import_name_; }
  const AstRawString* module_specifier() const { return module_specifier_; }
  void set_module_specifier(const AstRawString* module_specifier) {
    DCHECK(module_specifier_ == NULL);
    module_specifier_ = module_specifier;
  }
  InitializationFlag initialization() const override {
    return kNeedsInitialization;
  }

 protected:
  ImportDeclaration(Zone* zone, VariableProxy* proxy,
                    const AstRawString* import_name,
                    const AstRawString* module_specifier, Scope* scope, int pos)
      : Declaration(zone, proxy, IMPORT, scope, pos),
        import_name_(import_name),
        module_specifier_(module_specifier) {}

 private:
  const AstRawString* import_name_;
  const AstRawString* module_specifier_;
};


class ExportDeclaration final : public Declaration {
 public:
  DECLARE_NODE_TYPE(ExportDeclaration)

  InitializationFlag initialization() const override {
    return kCreatedInitialized;
  }

 protected:
  ExportDeclaration(Zone* zone, VariableProxy* proxy, Scope* scope, int pos)
      : Declaration(zone, proxy, LET, scope, pos) {}
};


class Module : public AstNode {
 public:
  ModuleDescriptor* descriptor() const { return descriptor_; }
  Block* body() const { return body_; }

 protected:
  Module(Zone* zone, int pos)
      : AstNode(pos), descriptor_(ModuleDescriptor::New(zone)), body_(NULL) {}
  Module(Zone* zone, ModuleDescriptor* descriptor, int pos, Block* body = NULL)
      : AstNode(pos), descriptor_(descriptor), body_(body) {}

 private:
  ModuleDescriptor* descriptor_;
  Block* body_;
};


class IterationStatement : public BreakableStatement {
 public:
  // Type testing & conversion.
  IterationStatement* AsIterationStatement() final { return this; }

  Statement* body() const { return body_; }
  void set_body(Statement* s) { body_ = s; }

  static int num_ids() { return parent_num_ids() + 1; }
  BailoutId OsrEntryId() const { return BailoutId(local_id(0)); }
  virtual BailoutId ContinueId() const = 0;
  virtual BailoutId StackCheckId() const = 0;

  // Code generation
  Label* continue_target()  { return &continue_target_; }

 protected:
  IterationStatement(Zone* zone, ZoneList<const AstRawString*>* labels, int pos)
      : BreakableStatement(zone, labels, TARGET_FOR_ANONYMOUS, pos),
        body_(NULL) {}
  static int parent_num_ids() { return BreakableStatement::num_ids(); }
  void Initialize(Statement* body) { body_ = body; }

 private:
  int local_id(int n) const { return base_id() + parent_num_ids() + n; }

  Statement* body_;
  Label continue_target_;
};


class DoWhileStatement final : public IterationStatement {
 public:
  DECLARE_NODE_TYPE(DoWhileStatement)

  void Initialize(Expression* cond, Statement* body) {
    IterationStatement::Initialize(body);
    cond_ = cond;
  }

  Expression* cond() const { return cond_; }
  void set_cond(Expression* e) { cond_ = e; }

  static int num_ids() { return parent_num_ids() + 2; }
  BailoutId ContinueId() const override { return BailoutId(local_id(0)); }
  BailoutId StackCheckId() const override { return BackEdgeId(); }
  BailoutId BackEdgeId() const { return BailoutId(local_id(1)); }

 protected:
  DoWhileStatement(Zone* zone, ZoneList<const AstRawString*>* labels, int pos)
      : IterationStatement(zone, labels, pos), cond_(NULL) {}
  static int parent_num_ids() { return IterationStatement::num_ids(); }

 private:
  int local_id(int n) const { return base_id() + parent_num_ids() + n; }

  Expression* cond_;
};


class WhileStatement final : public IterationStatement {
 public:
  DECLARE_NODE_TYPE(WhileStatement)

  void Initialize(Expression* cond, Statement* body) {
    IterationStatement::Initialize(body);
    cond_ = cond;
  }

  Expression* cond() const { return cond_; }
  void set_cond(Expression* e) { cond_ = e; }

  static int num_ids() { return parent_num_ids() + 1; }
  BailoutId ContinueId() const override { return EntryId(); }
  BailoutId StackCheckId() const override { return BodyId(); }
  BailoutId BodyId() const { return BailoutId(local_id(0)); }

 protected:
  WhileStatement(Zone* zone, ZoneList<const AstRawString*>* labels, int pos)
      : IterationStatement(zone, labels, pos), cond_(NULL) {}
  static int parent_num_ids() { return IterationStatement::num_ids(); }

 private:
  int local_id(int n) const { return base_id() + parent_num_ids() + n; }

  Expression* cond_;
};


class ForStatement final : public IterationStatement {
 public:
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

  void set_init(Statement* s) { init_ = s; }
  void set_cond(Expression* e) { cond_ = e; }
  void set_next(Statement* s) { next_ = s; }

  static int num_ids() { return parent_num_ids() + 2; }
  BailoutId ContinueId() const override { return BailoutId(local_id(0)); }
  BailoutId StackCheckId() const override { return BodyId(); }
  BailoutId BodyId() const { return BailoutId(local_id(1)); }

 protected:
  ForStatement(Zone* zone, ZoneList<const AstRawString*>* labels, int pos)
      : IterationStatement(zone, labels, pos),
        init_(NULL),
        cond_(NULL),
        next_(NULL) {}
  static int parent_num_ids() { return IterationStatement::num_ids(); }

 private:
  int local_id(int n) const { return base_id() + parent_num_ids() + n; }

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

  void Initialize(Expression* each, Expression* subject, Statement* body) {
    IterationStatement::Initialize(body);
    each_ = each;
    subject_ = subject;
  }

  Expression* each() const { return each_; }
  Expression* subject() const { return subject_; }

  void set_each(Expression* e) { each_ = e; }
  void set_subject(Expression* e) { subject_ = e; }

  void AssignFeedbackVectorSlots(Isolate* isolate, FeedbackVectorSpec* spec,
                                 FeedbackVectorSlotCache* cache) override;
  FeedbackVectorSlot EachFeedbackSlot() const { return each_slot_; }

 protected:
  ForEachStatement(Zone* zone, ZoneList<const AstRawString*>* labels, int pos)
      : IterationStatement(zone, labels, pos), each_(NULL), subject_(NULL) {}

 private:
  Expression* each_;
  Expression* subject_;
  FeedbackVectorSlot each_slot_;
};


class ForInStatement final : public ForEachStatement {
 public:
  DECLARE_NODE_TYPE(ForInStatement)

  Expression* enumerable() const {
    return subject();
  }

  // Type feedback information.
  void AssignFeedbackVectorSlots(Isolate* isolate, FeedbackVectorSpec* spec,
                                 FeedbackVectorSlotCache* cache) override {
    ForEachStatement::AssignFeedbackVectorSlots(isolate, spec, cache);
    for_in_feedback_slot_ = spec->AddGeneralSlot();
  }

  FeedbackVectorSlot ForInFeedbackSlot() {
    DCHECK(!for_in_feedback_slot_.IsInvalid());
    return for_in_feedback_slot_;
  }

  enum ForInType { FAST_FOR_IN, SLOW_FOR_IN };
  ForInType for_in_type() const { return for_in_type_; }
  void set_for_in_type(ForInType type) { for_in_type_ = type; }

  static int num_ids() { return parent_num_ids() + 6; }
  BailoutId BodyId() const { return BailoutId(local_id(0)); }
  BailoutId PrepareId() const { return BailoutId(local_id(1)); }
  BailoutId EnumId() const { return BailoutId(local_id(2)); }
  BailoutId ToObjectId() const { return BailoutId(local_id(3)); }
  BailoutId FilterId() const { return BailoutId(local_id(4)); }
  BailoutId AssignmentId() const { return BailoutId(local_id(5)); }
  BailoutId ContinueId() const override { return EntryId(); }
  BailoutId StackCheckId() const override { return BodyId(); }

 protected:
  ForInStatement(Zone* zone, ZoneList<const AstRawString*>* labels, int pos)
      : ForEachStatement(zone, labels, pos), for_in_type_(SLOW_FOR_IN) {}
  static int parent_num_ids() { return ForEachStatement::num_ids(); }

 private:
  int local_id(int n) const { return base_id() + parent_num_ids() + n; }

  ForInType for_in_type_;
  FeedbackVectorSlot for_in_feedback_slot_;
};


class ForOfStatement final : public ForEachStatement {
 public:
  DECLARE_NODE_TYPE(ForOfStatement)

  void Initialize(Expression* each,
                  Expression* subject,
                  Statement* body,
                  Expression* assign_iterator,
                  Expression* next_result,
                  Expression* result_done,
                  Expression* assign_each) {
    ForEachStatement::Initialize(each, subject, body);
    assign_iterator_ = assign_iterator;
    next_result_ = next_result;
    result_done_ = result_done;
    assign_each_ = assign_each;
  }

  Expression* iterable() const {
    return subject();
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

  BailoutId ContinueId() const override { return EntryId(); }
  BailoutId StackCheckId() const override { return BackEdgeId(); }

  static int num_ids() { return parent_num_ids() + 1; }
  BailoutId BackEdgeId() const { return BailoutId(local_id(0)); }

 protected:
  ForOfStatement(Zone* zone, ZoneList<const AstRawString*>* labels, int pos)
      : ForEachStatement(zone, labels, pos),
        assign_iterator_(NULL),
        next_result_(NULL),
        result_done_(NULL),
        assign_each_(NULL) {}
  static int parent_num_ids() { return ForEachStatement::num_ids(); }

 private:
  int local_id(int n) const { return base_id() + parent_num_ids() + n; }

  Expression* assign_iterator_;
  Expression* next_result_;
  Expression* result_done_;
  Expression* assign_each_;
};


class ExpressionStatement final : public Statement {
 public:
  DECLARE_NODE_TYPE(ExpressionStatement)

  void set_expression(Expression* e) { expression_ = e; }
  Expression* expression() const { return expression_; }
  bool IsJump() const override { return expression_->IsThrow(); }
  void MarkTail() override { expression_->MarkTail(); }

 protected:
  ExpressionStatement(Zone* zone, Expression* expression, int pos)
      : Statement(zone, pos), expression_(expression) { }

 private:
  Expression* expression_;
};


class JumpStatement : public Statement {
 public:
  bool IsJump() const final { return true; }

 protected:
  explicit JumpStatement(Zone* zone, int pos) : Statement(zone, pos) {}
};


class ContinueStatement final : public JumpStatement {
 public:
  DECLARE_NODE_TYPE(ContinueStatement)

  IterationStatement* target() const { return target_; }

 protected:
  explicit ContinueStatement(Zone* zone, IterationStatement* target, int pos)
      : JumpStatement(zone, pos), target_(target) { }

 private:
  IterationStatement* target_;
};


class BreakStatement final : public JumpStatement {
 public:
  DECLARE_NODE_TYPE(BreakStatement)

  BreakableStatement* target() const { return target_; }

 protected:
  explicit BreakStatement(Zone* zone, BreakableStatement* target, int pos)
      : JumpStatement(zone, pos), target_(target) { }

 private:
  BreakableStatement* target_;
};


class ReturnStatement final : public JumpStatement {
 public:
  DECLARE_NODE_TYPE(ReturnStatement)

  Expression* expression() const { return expression_; }

  void set_expression(Expression* e) { expression_ = e; }

 protected:
  explicit ReturnStatement(Zone* zone, Expression* expression, int pos)
      : JumpStatement(zone, pos), expression_(expression) { }

 private:
  Expression* expression_;
};


class WithStatement final : public Statement {
 public:
  DECLARE_NODE_TYPE(WithStatement)

  Scope* scope() { return scope_; }
  Expression* expression() const { return expression_; }
  void set_expression(Expression* e) { expression_ = e; }
  Statement* statement() const { return statement_; }
  void set_statement(Statement* s) { statement_ = s; }

  void set_base_id(int id) { base_id_ = id; }
  static int num_ids() { return parent_num_ids() + 2; }
  BailoutId ToObjectId() const { return BailoutId(local_id(0)); }
  BailoutId EntryId() const { return BailoutId(local_id(1)); }

  void MarkTail() override { statement_->MarkTail(); }

 protected:
  WithStatement(Zone* zone, Scope* scope, Expression* expression,
                Statement* statement, int pos)
      : Statement(zone, pos),
        scope_(scope),
        expression_(expression),
        statement_(statement),
        base_id_(BailoutId::None().ToInt()) {}
  static int parent_num_ids() { return 0; }

  int base_id() const {
    DCHECK(!BailoutId(base_id_).IsNone());
    return base_id_;
  }

 private:
  int local_id(int n) const { return base_id() + parent_num_ids() + n; }

  Scope* scope_;
  Expression* expression_;
  Statement* statement_;
  int base_id_;
};


class CaseClause final : public Expression {
 public:
  DECLARE_NODE_TYPE(CaseClause)

  bool is_default() const { return label_ == NULL; }
  Expression* label() const {
    CHECK(!is_default());
    return label_;
  }
  void set_label(Expression* e) { label_ = e; }
  Label* body_target() { return &body_target_; }
  ZoneList<Statement*>* statements() const { return statements_; }

  static int num_ids() { return parent_num_ids() + 2; }
  BailoutId EntryId() const { return BailoutId(local_id(0)); }
  TypeFeedbackId CompareId() { return TypeFeedbackId(local_id(1)); }

  void MarkTail() override {
    if (!statements_->is_empty()) statements_->last()->MarkTail();
  }

  Type* compare_type() { return compare_type_; }
  void set_compare_type(Type* type) { compare_type_ = type; }

 protected:
  static int parent_num_ids() { return Expression::num_ids(); }

 private:
  CaseClause(Zone* zone, Expression* label, ZoneList<Statement*>* statements,
             int pos);
  int local_id(int n) const { return base_id() + parent_num_ids() + n; }

  Expression* label_;
  Label body_target_;
  ZoneList<Statement*>* statements_;
  Type* compare_type_;
};


class SwitchStatement final : public BreakableStatement {
 public:
  DECLARE_NODE_TYPE(SwitchStatement)

  void Initialize(Expression* tag, ZoneList<CaseClause*>* cases) {
    tag_ = tag;
    cases_ = cases;
  }

  Expression* tag() const { return tag_; }
  ZoneList<CaseClause*>* cases() const { return cases_; }

  void set_tag(Expression* t) { tag_ = t; }

  void MarkTail() override {
    if (!cases_->is_empty()) cases_->last()->MarkTail();
  }

 protected:
  SwitchStatement(Zone* zone, ZoneList<const AstRawString*>* labels, int pos)
      : BreakableStatement(zone, labels, TARGET_FOR_ANONYMOUS, pos),
        tag_(NULL),
        cases_(NULL) {}

 private:
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
  DECLARE_NODE_TYPE(IfStatement)

  bool HasThenStatement() const { return !then_statement()->IsEmpty(); }
  bool HasElseStatement() const { return !else_statement()->IsEmpty(); }

  Expression* condition() const { return condition_; }
  Statement* then_statement() const { return then_statement_; }
  Statement* else_statement() const { return else_statement_; }

  void set_condition(Expression* e) { condition_ = e; }
  void set_then_statement(Statement* s) { then_statement_ = s; }
  void set_else_statement(Statement* s) { else_statement_ = s; }

  bool IsJump() const override {
    return HasThenStatement() && then_statement()->IsJump()
        && HasElseStatement() && else_statement()->IsJump();
  }

  void MarkTail() override {
    then_statement_->MarkTail();
    else_statement_->MarkTail();
  }

  void set_base_id(int id) { base_id_ = id; }
  static int num_ids() { return parent_num_ids() + 3; }
  BailoutId IfId() const { return BailoutId(local_id(0)); }
  BailoutId ThenId() const { return BailoutId(local_id(1)); }
  BailoutId ElseId() const { return BailoutId(local_id(2)); }

 protected:
  IfStatement(Zone* zone, Expression* condition, Statement* then_statement,
              Statement* else_statement, int pos)
      : Statement(zone, pos),
        condition_(condition),
        then_statement_(then_statement),
        else_statement_(else_statement),
        base_id_(BailoutId::None().ToInt()) {}
  static int parent_num_ids() { return 0; }

  int base_id() const {
    DCHECK(!BailoutId(base_id_).IsNone());
    return base_id_;
  }

 private:
  int local_id(int n) const { return base_id() + parent_num_ids() + n; }

  Expression* condition_;
  Statement* then_statement_;
  Statement* else_statement_;
  int base_id_;
};


class TryStatement : public Statement {
 public:
  Block* try_block() const { return try_block_; }
  void set_try_block(Block* b) { try_block_ = b; }

  void set_base_id(int id) { base_id_ = id; }
  static int num_ids() { return parent_num_ids() + 1; }
  BailoutId HandlerId() const { return BailoutId(local_id(0)); }

 protected:
  TryStatement(Zone* zone, Block* try_block, int pos)
      : Statement(zone, pos),
        try_block_(try_block),
        base_id_(BailoutId::None().ToInt()) {}
  static int parent_num_ids() { return 0; }

  int base_id() const {
    DCHECK(!BailoutId(base_id_).IsNone());
    return base_id_;
  }

 private:
  int local_id(int n) const { return base_id() + parent_num_ids() + n; }

  Block* try_block_;
  int base_id_;
};


class TryCatchStatement final : public TryStatement {
 public:
  DECLARE_NODE_TYPE(TryCatchStatement)

  Scope* scope() { return scope_; }
  Variable* variable() { return variable_; }
  Block* catch_block() const { return catch_block_; }
  void set_catch_block(Block* b) { catch_block_ = b; }

  void MarkTail() override { catch_block_->MarkTail(); }

 protected:
  TryCatchStatement(Zone* zone, Block* try_block, Scope* scope,
                    Variable* variable, Block* catch_block, int pos)
      : TryStatement(zone, try_block, pos),
        scope_(scope),
        variable_(variable),
        catch_block_(catch_block) {}

 private:
  Scope* scope_;
  Variable* variable_;
  Block* catch_block_;
};


class TryFinallyStatement final : public TryStatement {
 public:
  DECLARE_NODE_TYPE(TryFinallyStatement)

  Block* finally_block() const { return finally_block_; }
  void set_finally_block(Block* b) { finally_block_ = b; }

  void MarkTail() override { finally_block_->MarkTail(); }

 protected:
  TryFinallyStatement(Zone* zone, Block* try_block, Block* finally_block,
                      int pos)
      : TryStatement(zone, try_block, pos), finally_block_(finally_block) {}

 private:
  Block* finally_block_;
};


class DebuggerStatement final : public Statement {
 public:
  DECLARE_NODE_TYPE(DebuggerStatement)

  void set_base_id(int id) { base_id_ = id; }
  static int num_ids() { return parent_num_ids() + 1; }
  BailoutId DebugBreakId() const { return BailoutId(local_id(0)); }

 protected:
  explicit DebuggerStatement(Zone* zone, int pos)
      : Statement(zone, pos), base_id_(BailoutId::None().ToInt()) {}
  static int parent_num_ids() { return 0; }

  int base_id() const {
    DCHECK(!BailoutId(base_id_).IsNone());
    return base_id_;
  }

 private:
  int local_id(int n) const { return base_id() + parent_num_ids() + n; }

  int base_id_;
};


class EmptyStatement final : public Statement {
 public:
  DECLARE_NODE_TYPE(EmptyStatement)

 protected:
  explicit EmptyStatement(Zone* zone, int pos): Statement(zone, pos) {}
};


// Delegates to another statement, which may be overwritten.
// This was introduced to implement ES2015 Annex B3.3 for conditionally making
// sloppy-mode block-scoped functions have a var binding, which is changed
// from one statement to another during parsing.
class SloppyBlockFunctionStatement final : public Statement {
 public:
  DECLARE_NODE_TYPE(SloppyBlockFunctionStatement)

  Statement* statement() const { return statement_; }
  void set_statement(Statement* statement) { statement_ = statement; }
  Scope* scope() const { return scope_; }

 private:
  SloppyBlockFunctionStatement(Zone* zone, Statement* statement, Scope* scope)
      : Statement(zone, RelocInfo::kNoPosition),
        statement_(statement),
        scope_(scope) {}

  Statement* statement_;
  Scope* const scope_;
};


class Literal final : public Expression {
 public:
  DECLARE_NODE_TYPE(Literal)

  bool IsPropertyName() const override { return value_->IsPropertyName(); }

  Handle<String> AsPropertyName() {
    DCHECK(IsPropertyName());
    return Handle<String>::cast(value());
  }

  const AstRawString* AsRawPropertyName() {
    DCHECK(IsPropertyName());
    return value_->AsString();
  }

  bool ToBooleanIsTrue() const override { return value()->BooleanValue(); }
  bool ToBooleanIsFalse() const override { return !value()->BooleanValue(); }

  Handle<Object> value() const { return value_->value(); }
  const AstValue* raw_value() const { return value_; }

  // Support for using Literal as a HashMap key. NOTE: Currently, this works
  // only for string and number literals!
  uint32_t Hash();
  static bool Match(void* literal1, void* literal2);

  static int num_ids() { return parent_num_ids() + 1; }
  TypeFeedbackId LiteralFeedbackId() const {
    return TypeFeedbackId(local_id(0));
  }

 protected:
  Literal(Zone* zone, const AstValue* value, int position)
      : Expression(zone, position), value_(value) {}
  static int parent_num_ids() { return Expression::num_ids(); }

 private:
  int local_id(int n) const { return base_id() + parent_num_ids() + n; }

  const AstValue* value_;
};


class AstLiteralReindexer;

// Base class for literals that needs space in the corresponding JSFunction.
class MaterializedLiteral : public Expression {
 public:
  MaterializedLiteral* AsMaterializedLiteral() final { return this; }

  int literal_index() { return literal_index_; }

  int depth() const {
    // only callable after initialization.
    DCHECK(depth_ >= 1);
    return depth_;
  }

  bool is_strong() const { return is_strong_; }

 protected:
  MaterializedLiteral(Zone* zone, int literal_index, bool is_strong, int pos)
      : Expression(zone, pos),
        literal_index_(literal_index),
        is_simple_(false),
        is_strong_(is_strong),
        depth_(0) {}

  // A materialized literal is simple if the values consist of only
  // constants and simple object and array literals.
  bool is_simple() const { return is_simple_; }
  void set_is_simple(bool is_simple) { is_simple_ = is_simple; }
  friend class CompileTimeValue;

  void set_depth(int depth) {
    DCHECK(depth >= 1);
    depth_ = depth;
  }

  // Populate the constant properties/elements fixed array.
  void BuildConstants(Isolate* isolate);
  friend class ArrayLiteral;
  friend class ObjectLiteral;

  // If the expression is a literal, return the literal value;
  // if the expression is a materialized literal and is simple return a
  // compile time value as encoded by CompileTimeValue::GetValue().
  // Otherwise, return undefined literal as the placeholder
  // in the object literal boilerplate.
  Handle<Object> GetBoilerplateValue(Expression* expression, Isolate* isolate);

 private:
  int literal_index_;
  bool is_simple_;
  bool is_strong_;
  int depth_;

  friend class AstLiteralReindexer;
};


// Property is used for passing information
// about an object literal's properties from the parser
// to the code generator.
class ObjectLiteralProperty final : public ZoneObject {
 public:
  enum Kind {
    CONSTANT,              // Property with constant value (compile time).
    COMPUTED,              // Property with computed value (execution time).
    MATERIALIZED_LITERAL,  // Property value is a materialized literal.
    GETTER, SETTER,        // Property is an accessor function.
    PROTOTYPE              // Property is __proto__.
  };

  Expression* key() { return key_; }
  Expression* value() { return value_; }
  Kind kind() { return kind_; }

  void set_key(Expression* e) { key_ = e; }
  void set_value(Expression* e) { value_ = e; }

  // Type feedback information.
  bool IsMonomorphic() { return !receiver_type_.is_null(); }
  Handle<Map> GetReceiverType() { return receiver_type_; }

  bool IsCompileTimeValue();

  void set_emit_store(bool emit_store);
  bool emit_store();

  bool is_static() const { return is_static_; }
  bool is_computed_name() const { return is_computed_name_; }

  FeedbackVectorSlot GetSlot(int offset = 0) const {
    DCHECK_LT(offset, static_cast<int>(arraysize(slots_)));
    return slots_[offset];
  }
  void SetSlot(FeedbackVectorSlot slot, int offset = 0) {
    DCHECK_LT(offset, static_cast<int>(arraysize(slots_)));
    slots_[offset] = slot;
  }

  void set_receiver_type(Handle<Map> map) { receiver_type_ = map; }

 protected:
  friend class AstNodeFactory;

  ObjectLiteralProperty(Expression* key, Expression* value, Kind kind,
                        bool is_static, bool is_computed_name);
  ObjectLiteralProperty(AstValueFactory* ast_value_factory, Expression* key,
                        Expression* value, bool is_static,
                        bool is_computed_name);

 private:
  Expression* key_;
  Expression* value_;
  FeedbackVectorSlot slots_[2];
  Kind kind_;
  bool emit_store_;
  bool is_static_;
  bool is_computed_name_;
  Handle<Map> receiver_type_;
};


// An object literal has a boilerplate object that is used
// for minimizing the work when constructing it at runtime.
class ObjectLiteral final : public MaterializedLiteral {
 public:
  typedef ObjectLiteralProperty Property;

  DECLARE_NODE_TYPE(ObjectLiteral)

  Handle<FixedArray> constant_properties() const {
    return constant_properties_;
  }
  int properties_count() const { return constant_properties_->length() / 2; }
  ZoneList<Property*>* properties() const { return properties_; }
  bool fast_elements() const { return fast_elements_; }
  bool may_store_doubles() const { return may_store_doubles_; }
  bool has_function() const { return has_function_; }
  bool has_elements() const { return has_elements_; }

  // Decide if a property should be in the object boilerplate.
  static bool IsBoilerplateProperty(Property* property);

  // Populate the constant properties fixed array.
  void BuildConstantProperties(Isolate* isolate);

  // Mark all computed expressions that are bound to a key that
  // is shadowed by a later occurrence of the same key. For the
  // marked expressions, no store code is emitted.
  void CalculateEmitStore(Zone* zone);

  // Assemble bitfield of flags for the CreateObjectLiteral helper.
  int ComputeFlags(bool disable_mementos = false) const {
    int flags = fast_elements() ? kFastElements : kNoFlags;
    flags |= has_function() ? kHasFunction : kNoFlags;
    if (depth() == 1 && !has_elements() && !may_store_doubles()) {
      flags |= kShallowProperties;
    }
    if (disable_mementos) {
      flags |= kDisableMementos;
    }
    if (is_strong()) {
      flags |= kIsStrong;
    }
    return flags;
  }

  enum Flags {
    kNoFlags = 0,
    kFastElements = 1,
    kHasFunction = 1 << 1,
    kShallowProperties = 1 << 2,
    kDisableMementos = 1 << 3,
    kIsStrong = 1 << 4
  };

  struct Accessors: public ZoneObject {
    Accessors() : getter(NULL), setter(NULL) {}
    ObjectLiteralProperty* getter;
    ObjectLiteralProperty* setter;
  };

  BailoutId CreateLiteralId() const { return BailoutId(local_id(0)); }

  // Return an AST id for a property that is used in simulate instructions.
  BailoutId GetIdForPropertyName(int i) {
    return BailoutId(local_id(2 * i + 1));
  }
  BailoutId GetIdForPropertySet(int i) {
    return BailoutId(local_id(2 * i + 2));
  }

  // Unlike other AST nodes, this number of bailout IDs allocated for an
  // ObjectLiteral can vary, so num_ids() is not a static method.
  int num_ids() const {
    return parent_num_ids() + 1 + 2 * properties()->length();
  }

  // Object literals need one feedback slot for each non-trivial value, as well
  // as some slots for home objects.
  void AssignFeedbackVectorSlots(Isolate* isolate, FeedbackVectorSpec* spec,
                                 FeedbackVectorSlotCache* cache) override;

 protected:
  ObjectLiteral(Zone* zone, ZoneList<Property*>* properties, int literal_index,
                int boilerplate_properties, bool has_function, bool is_strong,
                int pos)
      : MaterializedLiteral(zone, literal_index, is_strong, pos),
        properties_(properties),
        boilerplate_properties_(boilerplate_properties),
        fast_elements_(false),
        has_elements_(false),
        may_store_doubles_(false),
        has_function_(has_function) {}
  static int parent_num_ids() { return MaterializedLiteral::num_ids(); }

 private:
  int local_id(int n) const { return base_id() + parent_num_ids() + n; }
  Handle<FixedArray> constant_properties_;
  ZoneList<Property*>* properties_;
  int boilerplate_properties_;
  bool fast_elements_;
  bool has_elements_;
  bool may_store_doubles_;
  bool has_function_;
  FeedbackVectorSlot slot_;
};


// A map from property names to getter/setter pairs allocated in the zone.
class AccessorTable : public TemplateHashMap<Literal, ObjectLiteral::Accessors,
                                             ZoneAllocationPolicy> {
 public:
  explicit AccessorTable(Zone* zone)
      : TemplateHashMap<Literal, ObjectLiteral::Accessors,
                        ZoneAllocationPolicy>(Literal::Match,
                                              ZoneAllocationPolicy(zone)),
        zone_(zone) {}

  Iterator lookup(Literal* literal) {
    Iterator it = find(literal, true, ZoneAllocationPolicy(zone_));
    if (it->second == NULL) it->second = new (zone_) ObjectLiteral::Accessors();
    return it;
  }

 private:
  Zone* zone_;
};


// Node for capturing a regexp literal.
class RegExpLiteral final : public MaterializedLiteral {
 public:
  DECLARE_NODE_TYPE(RegExpLiteral)

  Handle<String> pattern() const { return pattern_->string(); }
  int flags() const { return flags_; }

 protected:
  RegExpLiteral(Zone* zone, const AstRawString* pattern, int flags,
                int literal_index, bool is_strong, int pos)
      : MaterializedLiteral(zone, literal_index, is_strong, pos),
        pattern_(pattern),
        flags_(flags) {
    set_depth(1);
  }

 private:
  const AstRawString* const pattern_;
  int const flags_;
};


// An array literal has a literals object that is used
// for minimizing the work when constructing it at runtime.
class ArrayLiteral final : public MaterializedLiteral {
 public:
  DECLARE_NODE_TYPE(ArrayLiteral)

  Handle<FixedArray> constant_elements() const { return constant_elements_; }
  ElementsKind constant_elements_kind() const {
    DCHECK_EQ(2, constant_elements_->length());
    return static_cast<ElementsKind>(
        Smi::cast(constant_elements_->get(0))->value());
  }

  ZoneList<Expression*>* values() const { return values_; }

  BailoutId CreateLiteralId() const { return BailoutId(local_id(0)); }

  // Return an AST id for an element that is used in simulate instructions.
  BailoutId GetIdForElement(int i) { return BailoutId(local_id(i + 1)); }

  // Unlike other AST nodes, this number of bailout IDs allocated for an
  // ArrayLiteral can vary, so num_ids() is not a static method.
  int num_ids() const { return parent_num_ids() + 1 + values()->length(); }

  // Populate the constant elements fixed array.
  void BuildConstantElements(Isolate* isolate);

  // Assemble bitfield of flags for the CreateArrayLiteral helper.
  int ComputeFlags(bool disable_mementos = false) const {
    int flags = depth() == 1 ? kShallowElements : kNoFlags;
    if (disable_mementos) {
      flags |= kDisableMementos;
    }
    if (is_strong()) {
      flags |= kIsStrong;
    }
    return flags;
  }

  enum Flags {
    kNoFlags = 0,
    kShallowElements = 1,
    kDisableMementos = 1 << 1,
    kIsStrong = 1 << 2
  };

  void AssignFeedbackVectorSlots(Isolate* isolate, FeedbackVectorSpec* spec,
                                 FeedbackVectorSlotCache* cache) override;
  FeedbackVectorSlot LiteralFeedbackSlot() const { return literal_slot_; }

 protected:
  ArrayLiteral(Zone* zone, ZoneList<Expression*>* values,
               int first_spread_index, int literal_index, bool is_strong,
               int pos)
      : MaterializedLiteral(zone, literal_index, is_strong, pos),
        values_(values),
        first_spread_index_(first_spread_index) {}
  static int parent_num_ids() { return MaterializedLiteral::num_ids(); }

 private:
  int local_id(int n) const { return base_id() + parent_num_ids() + n; }

  Handle<FixedArray> constant_elements_;
  ZoneList<Expression*>* values_;
  int first_spread_index_;
  FeedbackVectorSlot literal_slot_;
};


class VariableProxy final : public Expression {
 public:
  DECLARE_NODE_TYPE(VariableProxy)

  bool IsValidReferenceExpression() const override {
    return !is_this() && !is_new_target();
  }

  bool IsArguments() const { return is_resolved() && var()->is_arguments(); }

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
  }

  bool is_resolved() const { return IsResolvedField::decode(bit_field_); }
  void set_is_resolved() {
    bit_field_ = IsResolvedField::update(bit_field_, true);
  }

  bool is_new_target() const { return IsNewTargetField::decode(bit_field_); }
  void set_is_new_target() {
    bit_field_ = IsNewTargetField::update(bit_field_, true);
  }

  int end_position() const { return end_position_; }

  // Bind this proxy to the variable var.
  void BindTo(Variable* var);

  bool UsesVariableFeedbackSlot() const {
    return var()->IsUnallocated() || var()->IsLookupSlot();
  }

  void AssignFeedbackVectorSlots(Isolate* isolate, FeedbackVectorSpec* spec,
                                 FeedbackVectorSlotCache* cache) override;

  FeedbackVectorSlot VariableFeedbackSlot() { return variable_feedback_slot_; }

  static int num_ids() { return parent_num_ids() + 1; }
  BailoutId BeforeId() const { return BailoutId(local_id(0)); }

 protected:
  VariableProxy(Zone* zone, Variable* var, int start_position,
                int end_position);

  VariableProxy(Zone* zone, const AstRawString* name,
                Variable::Kind variable_kind, int start_position,
                int end_position);
  static int parent_num_ids() { return Expression::num_ids(); }
  int local_id(int n) const { return base_id() + parent_num_ids() + n; }

  class IsThisField : public BitField8<bool, 0, 1> {};
  class IsAssignedField : public BitField8<bool, 1, 1> {};
  class IsResolvedField : public BitField8<bool, 2, 1> {};
  class IsNewTargetField : public BitField8<bool, 3, 1> {};

  // Start with 16-bit (or smaller) field, which should get packed together
  // with Expression's trailing 16-bit field.
  uint8_t bit_field_;
  FeedbackVectorSlot variable_feedback_slot_;
  union {
    const AstRawString* raw_name_;  // if !is_resolved_
    Variable* var_;                 // if is_resolved_
  };
  // Position is stored in the AstNode superclass, but VariableProxy needs to
  // know its end position too (for error messages). It cannot be inferred from
  // the variable name length because it can contain escapes.
  int end_position_;
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
  DECLARE_NODE_TYPE(Property)

  bool IsValidReferenceExpression() const override { return true; }

  Expression* obj() const { return obj_; }
  Expression* key() const { return key_; }

  void set_obj(Expression* e) { obj_ = e; }
  void set_key(Expression* e) { key_ = e; }

  static int num_ids() { return parent_num_ids() + 1; }
  BailoutId LoadId() const { return BailoutId(local_id(0)); }

  bool IsStringAccess() const {
    return IsStringAccessField::decode(bit_field_);
  }

  // Type feedback information.
  bool IsMonomorphic() override { return receiver_types_.length() == 1; }
  SmallMapList* GetReceiverTypes() override { return &receiver_types_; }
  KeyedAccessStoreMode GetStoreMode() const override { return STANDARD_STORE; }
  IcCheckType GetKeyType() const override {
    return KeyTypeField::decode(bit_field_);
  }
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

  void AssignFeedbackVectorSlots(Isolate* isolate, FeedbackVectorSpec* spec,
                                 FeedbackVectorSlotCache* cache) override {
    FeedbackVectorSlotKind kind = key()->IsPropertyName()
                                      ? FeedbackVectorSlotKind::LOAD_IC
                                      : FeedbackVectorSlotKind::KEYED_LOAD_IC;
    property_feedback_slot_ = spec->AddSlot(kind);
  }

  FeedbackVectorSlot PropertyFeedbackSlot() const {
    return property_feedback_slot_;
  }

  static LhsKind GetAssignType(Property* property) {
    if (property == NULL) return VARIABLE;
    bool super_access = property->IsSuperAccess();
    return (property->key()->IsPropertyName())
               ? (super_access ? NAMED_SUPER_PROPERTY : NAMED_PROPERTY)
               : (super_access ? KEYED_SUPER_PROPERTY : KEYED_PROPERTY);
  }

 protected:
  Property(Zone* zone, Expression* obj, Expression* key, int pos)
      : Expression(zone, pos),
        bit_field_(IsForCallField::encode(false) |
                   IsStringAccessField::encode(false) |
                   InlineCacheStateField::encode(UNINITIALIZED)),
        obj_(obj),
        key_(key) {}
  static int parent_num_ids() { return Expression::num_ids(); }

 private:
  int local_id(int n) const { return base_id() + parent_num_ids() + n; }

  class IsForCallField : public BitField8<bool, 0, 1> {};
  class IsStringAccessField : public BitField8<bool, 1, 1> {};
  class KeyTypeField : public BitField8<IcCheckType, 2, 1> {};
  class InlineCacheStateField : public BitField8<InlineCacheState, 3, 4> {};
  uint8_t bit_field_;
  FeedbackVectorSlot property_feedback_slot_;
  Expression* obj_;
  Expression* key_;
  SmallMapList receiver_types_;
};


class Call final : public Expression {
 public:
  DECLARE_NODE_TYPE(Call)

  Expression* expression() const { return expression_; }
  ZoneList<Expression*>* arguments() const { return arguments_; }

  void set_expression(Expression* e) { expression_ = e; }

  // Type feedback information.
  void AssignFeedbackVectorSlots(Isolate* isolate, FeedbackVectorSpec* spec,
                                 FeedbackVectorSlotCache* cache) override;

  FeedbackVectorSlot CallFeedbackSlot() const { return stub_slot_; }

  FeedbackVectorSlot CallFeedbackICSlot() const { return ic_slot_; }

  SmallMapList* GetReceiverTypes() override {
    if (expression()->IsProperty()) {
      return expression()->AsProperty()->GetReceiverTypes();
    }
    return NULL;
  }

  bool IsMonomorphic() override {
    if (expression()->IsProperty()) {
      return expression()->AsProperty()->IsMonomorphic();
    }
    return !target_.is_null();
  }

  bool global_call() const {
    VariableProxy* proxy = expression_->AsVariableProxy();
    return proxy != NULL && proxy->var()->IsUnallocatedOrGlobalSlot();
  }

  bool known_global_function() const {
    return global_call() && !target_.is_null();
  }

  Handle<JSFunction> target() { return target_; }

  Handle<AllocationSite> allocation_site() { return allocation_site_; }

  void SetKnownGlobalTarget(Handle<JSFunction> target) {
    target_ = target;
    set_is_uninitialized(false);
  }
  void set_target(Handle<JSFunction> target) { target_ = target; }
  void set_allocation_site(Handle<AllocationSite> site) {
    allocation_site_ = site;
  }

  static int num_ids() { return parent_num_ids() + 4; }
  BailoutId ReturnId() const { return BailoutId(local_id(0)); }
  BailoutId EvalId() const { return BailoutId(local_id(1)); }
  BailoutId LookupId() const { return BailoutId(local_id(2)); }
  BailoutId CallId() const { return BailoutId(local_id(3)); }

  bool is_uninitialized() const {
    return IsUninitializedField::decode(bit_field_);
  }
  void set_is_uninitialized(bool b) {
    bit_field_ = IsUninitializedField::update(bit_field_, b);
  }

  bool is_tail() const { return IsTailField::decode(bit_field_); }
  void MarkTail() override {
    bit_field_ = IsTailField::update(bit_field_, true);
  }

  enum CallType {
    POSSIBLY_EVAL_CALL,
    GLOBAL_CALL,
    LOOKUP_SLOT_CALL,
    NAMED_PROPERTY_CALL,
    KEYED_PROPERTY_CALL,
    NAMED_SUPER_PROPERTY_CALL,
    KEYED_SUPER_PROPERTY_CALL,
    SUPER_CALL,
    OTHER_CALL
  };

  // Helpers to determine how to handle the call.
  CallType GetCallType(Isolate* isolate) const;
  bool IsUsingCallFeedbackSlot(Isolate* isolate) const;
  bool IsUsingCallFeedbackICSlot(Isolate* isolate) const;

#ifdef DEBUG
  // Used to assert that the FullCodeGenerator records the return site.
  bool return_is_recorded_;
#endif

 protected:
  Call(Zone* zone, Expression* expression, ZoneList<Expression*>* arguments,
       int pos)
      : Expression(zone, pos),
        expression_(expression),
        arguments_(arguments),
        bit_field_(IsUninitializedField::encode(false)) {
    if (expression->IsProperty()) {
      expression->AsProperty()->mark_for_call();
    }
  }
  static int parent_num_ids() { return Expression::num_ids(); }

 private:
  int local_id(int n) const { return base_id() + parent_num_ids() + n; }

  FeedbackVectorSlot ic_slot_;
  FeedbackVectorSlot stub_slot_;
  Expression* expression_;
  ZoneList<Expression*>* arguments_;
  Handle<JSFunction> target_;
  Handle<AllocationSite> allocation_site_;
  class IsUninitializedField : public BitField8<bool, 0, 1> {};
  class IsTailField : public BitField8<bool, 1, 1> {};
  uint8_t bit_field_;
};


class CallNew final : public Expression {
 public:
  DECLARE_NODE_TYPE(CallNew)

  Expression* expression() const { return expression_; }
  ZoneList<Expression*>* arguments() const { return arguments_; }

  void set_expression(Expression* e) { expression_ = e; }

  // Type feedback information.
  void AssignFeedbackVectorSlots(Isolate* isolate, FeedbackVectorSpec* spec,
                                 FeedbackVectorSlotCache* cache) override {
    callnew_feedback_slot_ = spec->AddGeneralSlot();
  }

  FeedbackVectorSlot CallNewFeedbackSlot() {
    DCHECK(!callnew_feedback_slot_.IsInvalid());
    return callnew_feedback_slot_;
  }

  bool IsMonomorphic() override { return is_monomorphic_; }
  Handle<JSFunction> target() const { return target_; }
  Handle<AllocationSite> allocation_site() const {
    return allocation_site_;
  }

  static int num_ids() { return parent_num_ids() + 1; }
  static int feedback_slots() { return 1; }
  BailoutId ReturnId() const { return BailoutId(local_id(0)); }

  void set_allocation_site(Handle<AllocationSite> site) {
    allocation_site_ = site;
  }
  void set_is_monomorphic(bool monomorphic) { is_monomorphic_ = monomorphic; }
  void set_target(Handle<JSFunction> target) { target_ = target; }
  void SetKnownGlobalTarget(Handle<JSFunction> target) {
    target_ = target;
    is_monomorphic_ = true;
  }

 protected:
  CallNew(Zone* zone, Expression* expression, ZoneList<Expression*>* arguments,
          int pos)
      : Expression(zone, pos),
        expression_(expression),
        arguments_(arguments),
        is_monomorphic_(false) {}

  static int parent_num_ids() { return Expression::num_ids(); }

 private:
  int local_id(int n) const { return base_id() + parent_num_ids() + n; }

  Expression* expression_;
  ZoneList<Expression*>* arguments_;
  bool is_monomorphic_;
  Handle<JSFunction> target_;
  Handle<AllocationSite> allocation_site_;
  FeedbackVectorSlot callnew_feedback_slot_;
};


// The CallRuntime class does not represent any official JavaScript
// language construct. Instead it is used to call a C or JS function
// with a set of arguments. This is used from the builtins that are
// implemented in JavaScript (see "v8natives.js").
class CallRuntime final : public Expression {
 public:
  DECLARE_NODE_TYPE(CallRuntime)

  ZoneList<Expression*>* arguments() const { return arguments_; }
  bool is_jsruntime() const { return function_ == NULL; }

  int context_index() const {
    DCHECK(is_jsruntime());
    return context_index_;
  }
  const Runtime::Function* function() const {
    DCHECK(!is_jsruntime());
    return function_;
  }

  static int num_ids() { return parent_num_ids() + 1; }
  BailoutId CallId() { return BailoutId(local_id(0)); }

  const char* debug_name() {
    return is_jsruntime() ? "(context function)" : function_->name;
  }

 protected:
  CallRuntime(Zone* zone, const Runtime::Function* function,
              ZoneList<Expression*>* arguments, int pos)
      : Expression(zone, pos), function_(function), arguments_(arguments) {}

  CallRuntime(Zone* zone, int context_index, ZoneList<Expression*>* arguments,
              int pos)
      : Expression(zone, pos),
        function_(NULL),
        context_index_(context_index),
        arguments_(arguments) {}

  static int parent_num_ids() { return Expression::num_ids(); }

 private:
  int local_id(int n) const { return base_id() + parent_num_ids() + n; }

  const Runtime::Function* function_;
  int context_index_;
  ZoneList<Expression*>* arguments_;
};


class UnaryOperation final : public Expression {
 public:
  DECLARE_NODE_TYPE(UnaryOperation)

  Token::Value op() const { return op_; }
  Expression* expression() const { return expression_; }
  void set_expression(Expression* e) { expression_ = e; }

  // For unary not (Token::NOT), the AST ids where true and false will
  // actually be materialized, respectively.
  static int num_ids() { return parent_num_ids() + 2; }
  BailoutId MaterializeTrueId() const { return BailoutId(local_id(0)); }
  BailoutId MaterializeFalseId() const { return BailoutId(local_id(1)); }

  void RecordToBooleanTypeFeedback(TypeFeedbackOracle* oracle) override;

 protected:
  UnaryOperation(Zone* zone, Token::Value op, Expression* expression, int pos)
      : Expression(zone, pos), op_(op), expression_(expression) {
    DCHECK(Token::IsUnaryOp(op));
  }
  static int parent_num_ids() { return Expression::num_ids(); }

 private:
  int local_id(int n) const { return base_id() + parent_num_ids() + n; }

  Token::Value op_;
  Expression* expression_;
};


class BinaryOperation final : public Expression {
 public:
  DECLARE_NODE_TYPE(BinaryOperation)

  Token::Value op() const { return static_cast<Token::Value>(op_); }
  Expression* left() const { return left_; }
  void set_left(Expression* e) { left_ = e; }
  Expression* right() const { return right_; }
  void set_right(Expression* e) { right_ = e; }
  Handle<AllocationSite> allocation_site() const { return allocation_site_; }
  void set_allocation_site(Handle<AllocationSite> allocation_site) {
    allocation_site_ = allocation_site;
  }

  void MarkTail() override {
    switch (op()) {
      case Token::COMMA:
      case Token::AND:
      case Token::OR:
        right_->MarkTail();
      default:
        break;
    }
  }

  // The short-circuit logical operations need an AST ID for their
  // right-hand subexpression.
  static int num_ids() { return parent_num_ids() + 2; }
  BailoutId RightId() const { return BailoutId(local_id(0)); }

  TypeFeedbackId BinaryOperationFeedbackId() const {
    return TypeFeedbackId(local_id(1));
  }
  Maybe<int> fixed_right_arg() const {
    return has_fixed_right_arg_ ? Just(fixed_right_arg_value_) : Nothing<int>();
  }
  void set_fixed_right_arg(Maybe<int> arg) {
    has_fixed_right_arg_ = arg.IsJust();
    if (arg.IsJust()) fixed_right_arg_value_ = arg.FromJust();
  }

  void RecordToBooleanTypeFeedback(TypeFeedbackOracle* oracle) override;

 protected:
  BinaryOperation(Zone* zone, Token::Value op, Expression* left,
                  Expression* right, int pos)
      : Expression(zone, pos),
        op_(static_cast<byte>(op)),
        has_fixed_right_arg_(false),
        fixed_right_arg_value_(0),
        left_(left),
        right_(right) {
    DCHECK(Token::IsBinaryOp(op));
  }
  static int parent_num_ids() { return Expression::num_ids(); }

 private:
  int local_id(int n) const { return base_id() + parent_num_ids() + n; }

  const byte op_;  // actually Token::Value
  // TODO(rossberg): the fixed arg should probably be represented as a Constant
  // type for the RHS. Currenty it's actually a Maybe<int>
  bool has_fixed_right_arg_;
  int fixed_right_arg_value_;
  Expression* left_;
  Expression* right_;
  Handle<AllocationSite> allocation_site_;
};


class CountOperation final : public Expression {
 public:
  DECLARE_NODE_TYPE(CountOperation)

  bool is_prefix() const { return IsPrefixField::decode(bit_field_); }
  bool is_postfix() const { return !is_prefix(); }

  Token::Value op() const { return TokenField::decode(bit_field_); }
  Token::Value binary_op() {
    return (op() == Token::INC) ? Token::ADD : Token::SUB;
  }

  Expression* expression() const { return expression_; }
  void set_expression(Expression* e) { expression_ = e; }

  bool IsMonomorphic() override { return receiver_types_.length() == 1; }
  SmallMapList* GetReceiverTypes() override { return &receiver_types_; }
  IcCheckType GetKeyType() const override {
    return KeyTypeField::decode(bit_field_);
  }
  KeyedAccessStoreMode GetStoreMode() const override {
    return StoreModeField::decode(bit_field_);
  }
  Type* type() const { return type_; }
  void set_key_type(IcCheckType type) {
    bit_field_ = KeyTypeField::update(bit_field_, type);
  }
  void set_store_mode(KeyedAccessStoreMode mode) {
    bit_field_ = StoreModeField::update(bit_field_, mode);
  }
  void set_type(Type* type) { type_ = type; }

  static int num_ids() { return parent_num_ids() + 4; }
  BailoutId AssignmentId() const { return BailoutId(local_id(0)); }
  BailoutId ToNumberId() const { return BailoutId(local_id(1)); }
  TypeFeedbackId CountBinOpFeedbackId() const {
    return TypeFeedbackId(local_id(2));
  }
  TypeFeedbackId CountStoreFeedbackId() const {
    return TypeFeedbackId(local_id(3));
  }

  void AssignFeedbackVectorSlots(Isolate* isolate, FeedbackVectorSpec* spec,
                                 FeedbackVectorSlotCache* cache) override;
  FeedbackVectorSlot CountSlot() const { return slot_; }

 protected:
  CountOperation(Zone* zone, Token::Value op, bool is_prefix, Expression* expr,
                 int pos)
      : Expression(zone, pos),
        bit_field_(
            IsPrefixField::encode(is_prefix) | KeyTypeField::encode(ELEMENT) |
            StoreModeField::encode(STANDARD_STORE) | TokenField::encode(op)),
        type_(NULL),
        expression_(expr) {}
  static int parent_num_ids() { return Expression::num_ids(); }

 private:
  int local_id(int n) const { return base_id() + parent_num_ids() + n; }

  class IsPrefixField : public BitField16<bool, 0, 1> {};
  class KeyTypeField : public BitField16<IcCheckType, 1, 1> {};
  class StoreModeField : public BitField16<KeyedAccessStoreMode, 2, 3> {};
  class TokenField : public BitField16<Token::Value, 5, 8> {};

  // Starts with 16-bit field, which should get packed together with
  // Expression's trailing 16-bit field.
  uint16_t bit_field_;
  Type* type_;
  Expression* expression_;
  SmallMapList receiver_types_;
  FeedbackVectorSlot slot_;
};


class CompareOperation final : public Expression {
 public:
  DECLARE_NODE_TYPE(CompareOperation)

  Token::Value op() const { return op_; }
  Expression* left() const { return left_; }
  Expression* right() const { return right_; }

  void set_left(Expression* e) { left_ = e; }
  void set_right(Expression* e) { right_ = e; }

  // Type feedback information.
  static int num_ids() { return parent_num_ids() + 1; }
  TypeFeedbackId CompareOperationFeedbackId() const {
    return TypeFeedbackId(local_id(0));
  }
  Type* combined_type() const { return combined_type_; }
  void set_combined_type(Type* type) { combined_type_ = type; }

  // Match special cases.
  bool IsLiteralCompareTypeof(Expression** expr, Handle<String>* check);
  bool IsLiteralCompareUndefined(Expression** expr, Isolate* isolate);
  bool IsLiteralCompareNull(Expression** expr);

 protected:
  CompareOperation(Zone* zone, Token::Value op, Expression* left,
                   Expression* right, int pos)
      : Expression(zone, pos),
        op_(op),
        left_(left),
        right_(right),
        combined_type_(Type::None(zone)) {
    DCHECK(Token::IsCompareOp(op));
  }
  static int parent_num_ids() { return Expression::num_ids(); }

 private:
  int local_id(int n) const { return base_id() + parent_num_ids() + n; }

  Token::Value op_;
  Expression* left_;
  Expression* right_;

  Type* combined_type_;
};


class Spread final : public Expression {
 public:
  DECLARE_NODE_TYPE(Spread)

  Expression* expression() const { return expression_; }
  void set_expression(Expression* e) { expression_ = e; }

  static int num_ids() { return parent_num_ids(); }

 protected:
  Spread(Zone* zone, Expression* expression, int pos)
      : Expression(zone, pos), expression_(expression) {}
  static int parent_num_ids() { return Expression::num_ids(); }

 private:
  int local_id(int n) const { return base_id() + parent_num_ids() + n; }

  Expression* expression_;
};


class Conditional final : public Expression {
 public:
  DECLARE_NODE_TYPE(Conditional)

  Expression* condition() const { return condition_; }
  Expression* then_expression() const { return then_expression_; }
  Expression* else_expression() const { return else_expression_; }

  void set_condition(Expression* e) { condition_ = e; }
  void set_then_expression(Expression* e) { then_expression_ = e; }
  void set_else_expression(Expression* e) { else_expression_ = e; }

  void MarkTail() override {
    then_expression_->MarkTail();
    else_expression_->MarkTail();
  }

  static int num_ids() { return parent_num_ids() + 2; }
  BailoutId ThenId() const { return BailoutId(local_id(0)); }
  BailoutId ElseId() const { return BailoutId(local_id(1)); }

 protected:
  Conditional(Zone* zone, Expression* condition, Expression* then_expression,
              Expression* else_expression, int position)
      : Expression(zone, position),
        condition_(condition),
        then_expression_(then_expression),
        else_expression_(else_expression) {}
  static int parent_num_ids() { return Expression::num_ids(); }

 private:
  int local_id(int n) const { return base_id() + parent_num_ids() + n; }

  Expression* condition_;
  Expression* then_expression_;
  Expression* else_expression_;
};


class Assignment final : public Expression {
 public:
  DECLARE_NODE_TYPE(Assignment)

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

  static int num_ids() { return parent_num_ids() + 2; }
  BailoutId AssignmentId() const { return BailoutId(local_id(0)); }

  // Type feedback information.
  TypeFeedbackId AssignmentFeedbackId() { return TypeFeedbackId(local_id(1)); }
  bool IsMonomorphic() override { return receiver_types_.length() == 1; }
  bool IsUninitialized() const {
    return IsUninitializedField::decode(bit_field_);
  }
  bool HasNoTypeInformation() {
    return IsUninitializedField::decode(bit_field_);
  }
  SmallMapList* GetReceiverTypes() override { return &receiver_types_; }
  IcCheckType GetKeyType() const override {
    return KeyTypeField::decode(bit_field_);
  }
  KeyedAccessStoreMode GetStoreMode() const override {
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

  void AssignFeedbackVectorSlots(Isolate* isolate, FeedbackVectorSpec* spec,
                                 FeedbackVectorSlotCache* cache) override;
  FeedbackVectorSlot AssignmentSlot() const { return slot_; }

 protected:
  Assignment(Zone* zone, Token::Value op, Expression* target, Expression* value,
             int pos);
  static int parent_num_ids() { return Expression::num_ids(); }

 private:
  int local_id(int n) const { return base_id() + parent_num_ids() + n; }

  class IsUninitializedField : public BitField16<bool, 0, 1> {};
  class KeyTypeField
      : public BitField16<IcCheckType, IsUninitializedField::kNext, 1> {};
  class StoreModeField
      : public BitField16<KeyedAccessStoreMode, KeyTypeField::kNext, 3> {};
  class TokenField : public BitField16<Token::Value, StoreModeField::kNext, 8> {
  };

  // Starts with 16-bit field, which should get packed together with
  // Expression's trailing 16-bit field.
  uint16_t bit_field_;
  Expression* target_;
  Expression* value_;
  BinaryOperation* binary_operation_;
  SmallMapList receiver_types_;
  FeedbackVectorSlot slot_;
};


class RewritableAssignmentExpression : public Expression {
 public:
  DECLARE_NODE_TYPE(RewritableAssignmentExpression)

  Expression* expression() { return expr_; }
  bool is_rewritten() const { return is_rewritten_; }

  void set_expression(Expression* e) { expr_ = e; }

  void Rewrite(Expression* new_expression) {
    DCHECK(!is_rewritten());
    DCHECK_NOT_NULL(new_expression);
    expr_ = new_expression;
    is_rewritten_ = true;
  }

  static int num_ids() { return parent_num_ids(); }

 protected:
  RewritableAssignmentExpression(Zone* zone, Expression* expression)
      : Expression(zone, expression->position()),
        is_rewritten_(false),
        expr_(expression) {}

 private:
  int local_id(int n) const { return base_id() + parent_num_ids() + n; }

  bool is_rewritten_;
  Expression* expr_;
};


class Yield final : public Expression {
 public:
  DECLARE_NODE_TYPE(Yield)

  enum Kind {
    kInitial,  // The initial yield that returns the unboxed generator object.
    kSuspend,  // A normal yield: { value: EXPRESSION, done: false }
    kDelegating,  // A yield*.
    kFinal        // A return: { value: EXPRESSION, done: true }
  };

  Expression* generator_object() const { return generator_object_; }
  Expression* expression() const { return expression_; }
  Kind yield_kind() const { return yield_kind_; }

  void set_generator_object(Expression* e) { generator_object_ = e; }
  void set_expression(Expression* e) { expression_ = e; }

  // Type feedback information.
  bool HasFeedbackSlots() const { return yield_kind() == kDelegating; }
  void AssignFeedbackVectorSlots(Isolate* isolate, FeedbackVectorSpec* spec,
                                 FeedbackVectorSlotCache* cache) override {
    if (HasFeedbackSlots()) {
      yield_first_feedback_slot_ = spec->AddKeyedLoadICSlot();
      keyed_load_feedback_slot_ = spec->AddLoadICSlot();
      done_feedback_slot_ = spec->AddLoadICSlot();
    }
  }

  FeedbackVectorSlot KeyedLoadFeedbackSlot() {
    DCHECK(!HasFeedbackSlots() || !yield_first_feedback_slot_.IsInvalid());
    return yield_first_feedback_slot_;
  }

  FeedbackVectorSlot DoneFeedbackSlot() { return keyed_load_feedback_slot_; }

  FeedbackVectorSlot ValueFeedbackSlot() { return done_feedback_slot_; }

 protected:
  Yield(Zone* zone, Expression* generator_object, Expression* expression,
        Kind yield_kind, int pos)
      : Expression(zone, pos),
        generator_object_(generator_object),
        expression_(expression),
        yield_kind_(yield_kind) {}

 private:
  Expression* generator_object_;
  Expression* expression_;
  Kind yield_kind_;
  FeedbackVectorSlot yield_first_feedback_slot_;
  FeedbackVectorSlot keyed_load_feedback_slot_;
  FeedbackVectorSlot done_feedback_slot_;
};


class Throw final : public Expression {
 public:
  DECLARE_NODE_TYPE(Throw)

  Expression* exception() const { return exception_; }
  void set_exception(Expression* e) { exception_ = e; }

 protected:
  Throw(Zone* zone, Expression* exception, int pos)
      : Expression(zone, pos), exception_(exception) {}

 private:
  Expression* exception_;
};


class FunctionLiteral final : public Expression {
 public:
  enum FunctionType {
    kAnonymousExpression,
    kNamedExpression,
    kDeclaration,
    kGlobalOrEval
  };

  enum ParameterFlag { kNoDuplicateParameters, kHasDuplicateParameters };

  enum EagerCompileHint { kShouldEagerCompile, kShouldLazyCompile };

  enum ArityRestriction { kNormalArity, kGetterArity, kSetterArity };

  DECLARE_NODE_TYPE(FunctionLiteral)

  Handle<String> name() const { return raw_name_->string(); }
  const AstString* raw_name() const { return raw_name_; }
  void set_raw_name(const AstString* name) { raw_name_ = name; }
  Scope* scope() const { return scope_; }
  ZoneList<Statement*>* body() const { return body_; }
  void set_function_token_position(int pos) { function_token_position_ = pos; }
  int function_token_position() const { return function_token_position_; }
  int start_position() const;
  int end_position() const;
  int SourceSize() const { return end_position() - start_position(); }
  bool is_expression() const { return IsExpression::decode(bitfield_); }
  bool is_anonymous() const { return IsAnonymous::decode(bitfield_); }
  LanguageMode language_mode() const;

  static bool NeedsHomeObject(Expression* expr);

  int materialized_literal_count() { return materialized_literal_count_; }
  int expected_property_count() { return expected_property_count_; }
  int parameter_count() { return parameter_count_; }

  bool AllowsLazyCompilation();
  bool AllowsLazyCompilationWithoutContext();

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
    return Handle<String>();
  }

  // Only one of {set_inferred_name, set_raw_inferred_name} should be called.
  void set_inferred_name(Handle<String> inferred_name) {
    DCHECK(!inferred_name.is_null());
    inferred_name_ = inferred_name;
    DCHECK(raw_inferred_name_== NULL || raw_inferred_name_->IsEmpty());
    raw_inferred_name_ = NULL;
  }

  void set_raw_inferred_name(const AstString* raw_inferred_name) {
    DCHECK(raw_inferred_name != NULL);
    raw_inferred_name_ = raw_inferred_name;
    DCHECK(inferred_name_.is_null());
    inferred_name_ = Handle<String>();
  }

  bool pretenure() const { return Pretenure::decode(bitfield_); }
  void set_pretenure() { bitfield_ = Pretenure::update(bitfield_, true); }

  bool has_duplicate_parameters() const {
    return HasDuplicateParameters::decode(bitfield_);
  }

  bool is_function() const { return IsFunction::decode(bitfield_); }

  // This is used as a heuristic on when to eagerly compile a function
  // literal. We consider the following constructs as hints that the
  // function will be called immediately:
  // - (function() { ... })();
  // - var x = function() { ... }();
  bool should_eager_compile() const {
    return ShouldEagerCompile::decode(bitfield_);
  }
  void set_should_eager_compile() {
    bitfield_ = ShouldEagerCompile::update(bitfield_, true);
  }

  // A hint that we expect this function to be called (exactly) once,
  // i.e. we suspect it's an initialization function.
  bool should_be_used_once_hint() const {
    return ShouldBeUsedOnceHint::decode(bitfield_);
  }
  void set_should_be_used_once_hint() {
    bitfield_ = ShouldBeUsedOnceHint::update(bitfield_, true);
  }

  FunctionKind kind() const { return FunctionKindBits::decode(bitfield_); }

  int ast_node_count() { return ast_properties_.node_count(); }
  AstProperties::Flags flags() const { return ast_properties_.flags(); }
  void set_ast_properties(AstProperties* ast_properties) {
    ast_properties_ = *ast_properties;
  }
  const FeedbackVectorSpec* feedback_vector_spec() const {
    return ast_properties_.get_spec();
  }
  bool dont_optimize() { return dont_optimize_reason_ != kNoReason; }
  BailoutReason dont_optimize_reason() { return dont_optimize_reason_; }
  void set_dont_optimize_reason(BailoutReason reason) {
    dont_optimize_reason_ = reason;
  }

 protected:
  FunctionLiteral(Zone* zone, const AstString* name,
                  AstValueFactory* ast_value_factory, Scope* scope,
                  ZoneList<Statement*>* body, int materialized_literal_count,
                  int expected_property_count, int parameter_count,
                  FunctionType function_type,
                  ParameterFlag has_duplicate_parameters,
                  EagerCompileHint eager_compile_hint, FunctionKind kind,
                  int position)
      : Expression(zone, position),
        raw_name_(name),
        scope_(scope),
        body_(body),
        raw_inferred_name_(ast_value_factory->empty_string()),
        ast_properties_(zone),
        dont_optimize_reason_(kNoReason),
        materialized_literal_count_(materialized_literal_count),
        expected_property_count_(expected_property_count),
        parameter_count_(parameter_count),
        function_token_position_(RelocInfo::kNoPosition) {
    bitfield_ =
        IsExpression::encode(function_type != kDeclaration) |
        IsAnonymous::encode(function_type == kAnonymousExpression) |
        Pretenure::encode(false) |
        HasDuplicateParameters::encode(has_duplicate_parameters ==
                                       kHasDuplicateParameters) |
        IsFunction::encode(function_type != kGlobalOrEval) |
        ShouldEagerCompile::encode(eager_compile_hint == kShouldEagerCompile) |
        FunctionKindBits::encode(kind) | ShouldBeUsedOnceHint::encode(false);
    DCHECK(IsValidFunctionKind(kind));
  }

 private:
  class IsExpression : public BitField16<bool, 0, 1> {};
  class IsAnonymous : public BitField16<bool, 1, 1> {};
  class Pretenure : public BitField16<bool, 2, 1> {};
  class HasDuplicateParameters : public BitField16<bool, 3, 1> {};
  class IsFunction : public BitField16<bool, 4, 1> {};
  class ShouldEagerCompile : public BitField16<bool, 5, 1> {};
  class FunctionKindBits : public BitField16<FunctionKind, 6, 8> {};
  class ShouldBeUsedOnceHint : public BitField16<bool, 15, 1> {};

  // Start with 16-bit field, which should get packed together
  // with Expression's trailing 16-bit field.
  uint16_t bitfield_;

  const AstString* raw_name_;
  Scope* scope_;
  ZoneList<Statement*>* body_;
  const AstString* raw_inferred_name_;
  Handle<String> inferred_name_;
  AstProperties ast_properties_;
  BailoutReason dont_optimize_reason_;

  int materialized_literal_count_;
  int expected_property_count_;
  int parameter_count_;
  int function_token_position_;
};


class ClassLiteral final : public Expression {
 public:
  typedef ObjectLiteralProperty Property;

  DECLARE_NODE_TYPE(ClassLiteral)

  Handle<String> name() const { return raw_name_->string(); }
  const AstRawString* raw_name() const { return raw_name_; }
  void set_raw_name(const AstRawString* name) {
    DCHECK_NULL(raw_name_);
    raw_name_ = name;
  }

  Scope* scope() const { return scope_; }
  VariableProxy* class_variable_proxy() const { return class_variable_proxy_; }
  Expression* extends() const { return extends_; }
  void set_extends(Expression* e) { extends_ = e; }
  FunctionLiteral* constructor() const { return constructor_; }
  void set_constructor(FunctionLiteral* f) { constructor_ = f; }
  ZoneList<Property*>* properties() const { return properties_; }
  int start_position() const { return position(); }
  int end_position() const { return end_position_; }

  BailoutId EntryId() const { return BailoutId(local_id(0)); }
  BailoutId DeclsId() const { return BailoutId(local_id(1)); }
  BailoutId ExitId() { return BailoutId(local_id(2)); }
  BailoutId CreateLiteralId() const { return BailoutId(local_id(3)); }

  // Return an AST id for a property that is used in simulate instructions.
  BailoutId GetIdForProperty(int i) { return BailoutId(local_id(i + 4)); }

  // Unlike other AST nodes, this number of bailout IDs allocated for an
  // ClassLiteral can vary, so num_ids() is not a static method.
  int num_ids() const { return parent_num_ids() + 4 + properties()->length(); }

  // Object literals need one feedback slot for each non-trivial value, as well
  // as some slots for home objects.
  void AssignFeedbackVectorSlots(Isolate* isolate, FeedbackVectorSpec* spec,
                                 FeedbackVectorSlotCache* cache) override;

  bool NeedsProxySlot() const {
    return class_variable_proxy() != nullptr &&
           class_variable_proxy()->var()->IsUnallocated();
  }

  FeedbackVectorSlot ProxySlot() const { return slot_; }

 protected:
  ClassLiteral(Zone* zone, const AstRawString* name, Scope* scope,
               VariableProxy* class_variable_proxy, Expression* extends,
               FunctionLiteral* constructor, ZoneList<Property*>* properties,
               int start_position, int end_position)
      : Expression(zone, start_position),
        raw_name_(name),
        scope_(scope),
        class_variable_proxy_(class_variable_proxy),
        extends_(extends),
        constructor_(constructor),
        properties_(properties),
        end_position_(end_position) {}

  static int parent_num_ids() { return Expression::num_ids(); }

 private:
  int local_id(int n) const { return base_id() + parent_num_ids() + n; }

  const AstRawString* raw_name_;
  Scope* scope_;
  VariableProxy* class_variable_proxy_;
  Expression* extends_;
  FunctionLiteral* constructor_;
  ZoneList<Property*>* properties_;
  int end_position_;
  FeedbackVectorSlot slot_;
};


class NativeFunctionLiteral final : public Expression {
 public:
  DECLARE_NODE_TYPE(NativeFunctionLiteral)

  Handle<String> name() const { return name_->string(); }
  v8::Extension* extension() const { return extension_; }

 protected:
  NativeFunctionLiteral(Zone* zone, const AstRawString* name,
                        v8::Extension* extension, int pos)
      : Expression(zone, pos), name_(name), extension_(extension) {}

 private:
  const AstRawString* name_;
  v8::Extension* extension_;
};


class ThisFunction final : public Expression {
 public:
  DECLARE_NODE_TYPE(ThisFunction)

 protected:
  ThisFunction(Zone* zone, int pos) : Expression(zone, pos) {}
};


class SuperPropertyReference final : public Expression {
 public:
  DECLARE_NODE_TYPE(SuperPropertyReference)

  VariableProxy* this_var() const { return this_var_; }
  void set_this_var(VariableProxy* v) { this_var_ = v; }
  Expression* home_object() const { return home_object_; }
  void set_home_object(Expression* e) { home_object_ = e; }

 protected:
  SuperPropertyReference(Zone* zone, VariableProxy* this_var,
                         Expression* home_object, int pos)
      : Expression(zone, pos), this_var_(this_var), home_object_(home_object) {
    DCHECK(this_var->is_this());
    DCHECK(home_object->IsProperty());
  }

 private:
  VariableProxy* this_var_;
  Expression* home_object_;
};


class SuperCallReference final : public Expression {
 public:
  DECLARE_NODE_TYPE(SuperCallReference)

  VariableProxy* this_var() const { return this_var_; }
  void set_this_var(VariableProxy* v) { this_var_ = v; }
  VariableProxy* new_target_var() const { return new_target_var_; }
  void set_new_target_var(VariableProxy* v) { new_target_var_ = v; }
  VariableProxy* this_function_var() const { return this_function_var_; }
  void set_this_function_var(VariableProxy* v) { this_function_var_ = v; }

 protected:
  SuperCallReference(Zone* zone, VariableProxy* this_var,
                     VariableProxy* new_target_var,
                     VariableProxy* this_function_var, int pos)
      : Expression(zone, pos),
        this_var_(this_var),
        new_target_var_(new_target_var),
        this_function_var_(this_function_var) {
    DCHECK(this_var->is_this());
    DCHECK(new_target_var->raw_name()->IsOneByteEqualTo(".new.target"));
    DCHECK(this_function_var->raw_name()->IsOneByteEqualTo(".this_function"));
  }

 private:
  VariableProxy* this_var_;
  VariableProxy* new_target_var_;
  VariableProxy* this_function_var_;
};


// This class is produced when parsing the () in arrow functions without any
// arguments and is not actually a valid expression.
class EmptyParentheses final : public Expression {
 public:
  DECLARE_NODE_TYPE(EmptyParentheses)

 private:
  EmptyParentheses(Zone* zone, int pos) : Expression(zone, pos) {}
};


#undef DECLARE_NODE_TYPE


// ----------------------------------------------------------------------------
// Basic visitor
// - leaf node visitors are abstract.

class AstVisitor BASE_EMBEDDED {
 public:
  AstVisitor() {}
  virtual ~AstVisitor() {}

  // Stack overflow check and dynamic dispatch.
  virtual void Visit(AstNode* node) = 0;

  // Iteration left-to-right.
  virtual void VisitDeclarations(ZoneList<Declaration*>* declarations);
  virtual void VisitStatements(ZoneList<Statement*>* statements);
  virtual void VisitExpressions(ZoneList<Expression*>* expressions);

  // Individual AST nodes.
#define DEF_VISIT(type)                         \
  virtual void Visit##type(type* node) = 0;
  AST_NODE_LIST(DEF_VISIT)
#undef DEF_VISIT
};

#define DEFINE_AST_VISITOR_SUBCLASS_MEMBERS()               \
 public:                                                    \
  void Visit(AstNode* node) final {                         \
    if (!CheckStackOverflow()) node->Accept(this);          \
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
  explicit AstNodeFactory(AstValueFactory* ast_value_factory)
      : local_zone_(ast_value_factory->zone()),
        parser_zone_(ast_value_factory->zone()),
        ast_value_factory_(ast_value_factory) {}

  AstValueFactory* ast_value_factory() const { return ast_value_factory_; }

  VariableDeclaration* NewVariableDeclaration(
      VariableProxy* proxy, VariableMode mode, Scope* scope, int pos,
      bool is_class_declaration = false, int declaration_group_start = -1) {
    return new (parser_zone_)
        VariableDeclaration(parser_zone_, proxy, mode, scope, pos,
                            is_class_declaration, declaration_group_start);
  }

  FunctionDeclaration* NewFunctionDeclaration(VariableProxy* proxy,
                                              VariableMode mode,
                                              FunctionLiteral* fun,
                                              Scope* scope,
                                              int pos) {
    return new (parser_zone_)
        FunctionDeclaration(parser_zone_, proxy, mode, fun, scope, pos);
  }

  ImportDeclaration* NewImportDeclaration(VariableProxy* proxy,
                                          const AstRawString* import_name,
                                          const AstRawString* module_specifier,
                                          Scope* scope, int pos) {
    return new (parser_zone_) ImportDeclaration(
        parser_zone_, proxy, import_name, module_specifier, scope, pos);
  }

  ExportDeclaration* NewExportDeclaration(VariableProxy* proxy,
                                          Scope* scope,
                                          int pos) {
    return new (parser_zone_)
        ExportDeclaration(parser_zone_, proxy, scope, pos);
  }

  Block* NewBlock(ZoneList<const AstRawString*>* labels, int capacity,
                  bool ignore_completion_value, int pos) {
    return new (local_zone_)
        Block(local_zone_, labels, capacity, ignore_completion_value, pos);
  }

#define STATEMENT_WITH_LABELS(NodeType)                                     \
  NodeType* New##NodeType(ZoneList<const AstRawString*>* labels, int pos) { \
    return new (local_zone_) NodeType(local_zone_, labels, pos);            \
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
        return new (local_zone_) ForInStatement(local_zone_, labels, pos);
      }
      case ForEachStatement::ITERATE: {
        return new (local_zone_) ForOfStatement(local_zone_, labels, pos);
      }
    }
    UNREACHABLE();
    return NULL;
  }

  ExpressionStatement* NewExpressionStatement(Expression* expression, int pos) {
    return new (local_zone_) ExpressionStatement(local_zone_, expression, pos);
  }

  ContinueStatement* NewContinueStatement(IterationStatement* target, int pos) {
    return new (local_zone_) ContinueStatement(local_zone_, target, pos);
  }

  BreakStatement* NewBreakStatement(BreakableStatement* target, int pos) {
    return new (local_zone_) BreakStatement(local_zone_, target, pos);
  }

  ReturnStatement* NewReturnStatement(Expression* expression, int pos) {
    return new (local_zone_) ReturnStatement(local_zone_, expression, pos);
  }

  WithStatement* NewWithStatement(Scope* scope,
                                  Expression* expression,
                                  Statement* statement,
                                  int pos) {
    return new (local_zone_)
        WithStatement(local_zone_, scope, expression, statement, pos);
  }

  IfStatement* NewIfStatement(Expression* condition,
                              Statement* then_statement,
                              Statement* else_statement,
                              int pos) {
    return new (local_zone_) IfStatement(local_zone_, condition, then_statement,
                                         else_statement, pos);
  }

  TryCatchStatement* NewTryCatchStatement(Block* try_block, Scope* scope,
                                          Variable* variable,
                                          Block* catch_block, int pos) {
    return new (local_zone_) TryCatchStatement(local_zone_, try_block, scope,
                                               variable, catch_block, pos);
  }

  TryFinallyStatement* NewTryFinallyStatement(Block* try_block,
                                              Block* finally_block, int pos) {
    return new (local_zone_)
        TryFinallyStatement(local_zone_, try_block, finally_block, pos);
  }

  DebuggerStatement* NewDebuggerStatement(int pos) {
    return new (local_zone_) DebuggerStatement(local_zone_, pos);
  }

  EmptyStatement* NewEmptyStatement(int pos) {
    return new (local_zone_) EmptyStatement(local_zone_, pos);
  }

  SloppyBlockFunctionStatement* NewSloppyBlockFunctionStatement(
      Statement* statement, Scope* scope) {
    return new (parser_zone_)
        SloppyBlockFunctionStatement(parser_zone_, statement, scope);
  }

  CaseClause* NewCaseClause(
      Expression* label, ZoneList<Statement*>* statements, int pos) {
    return new (local_zone_) CaseClause(local_zone_, label, statements, pos);
  }

  Literal* NewStringLiteral(const AstRawString* string, int pos) {
    return new (local_zone_)
        Literal(local_zone_, ast_value_factory_->NewString(string), pos);
  }

  // A JavaScript symbol (ECMA-262 edition 6).
  Literal* NewSymbolLiteral(const char* name, int pos) {
    return new (local_zone_)
        Literal(local_zone_, ast_value_factory_->NewSymbol(name), pos);
  }

  Literal* NewNumberLiteral(double number, int pos, bool with_dot = false) {
    return new (local_zone_) Literal(
        local_zone_, ast_value_factory_->NewNumber(number, with_dot), pos);
  }

  Literal* NewSmiLiteral(int number, int pos) {
    return new (local_zone_)
        Literal(local_zone_, ast_value_factory_->NewSmi(number), pos);
  }

  Literal* NewBooleanLiteral(bool b, int pos) {
    return new (local_zone_)
        Literal(local_zone_, ast_value_factory_->NewBoolean(b), pos);
  }

  Literal* NewNullLiteral(int pos) {
    return new (local_zone_)
        Literal(local_zone_, ast_value_factory_->NewNull(), pos);
  }

  Literal* NewUndefinedLiteral(int pos) {
    return new (local_zone_)
        Literal(local_zone_, ast_value_factory_->NewUndefined(), pos);
  }

  Literal* NewTheHoleLiteral(int pos) {
    return new (local_zone_)
        Literal(local_zone_, ast_value_factory_->NewTheHole(), pos);
  }

  ObjectLiteral* NewObjectLiteral(
      ZoneList<ObjectLiteral::Property*>* properties,
      int literal_index,
      int boilerplate_properties,
      bool has_function,
      bool is_strong,
      int pos) {
    return new (local_zone_)
        ObjectLiteral(local_zone_, properties, literal_index,
                      boilerplate_properties, has_function, is_strong, pos);
  }

  ObjectLiteral::Property* NewObjectLiteralProperty(
      Expression* key, Expression* value, ObjectLiteralProperty::Kind kind,
      bool is_static, bool is_computed_name) {
    return new (local_zone_)
        ObjectLiteral::Property(key, value, kind, is_static, is_computed_name);
  }

  ObjectLiteral::Property* NewObjectLiteralProperty(Expression* key,
                                                    Expression* value,
                                                    bool is_static,
                                                    bool is_computed_name) {
    return new (local_zone_) ObjectLiteral::Property(
        ast_value_factory_, key, value, is_static, is_computed_name);
  }

  RegExpLiteral* NewRegExpLiteral(const AstRawString* pattern, int flags,
                                  int literal_index, bool is_strong, int pos) {
    return new (local_zone_) RegExpLiteral(local_zone_, pattern, flags,
                                           literal_index, is_strong, pos);
  }

  ArrayLiteral* NewArrayLiteral(ZoneList<Expression*>* values,
                                int literal_index,
                                bool is_strong,
                                int pos) {
    return new (local_zone_)
        ArrayLiteral(local_zone_, values, -1, literal_index, is_strong, pos);
  }

  ArrayLiteral* NewArrayLiteral(ZoneList<Expression*>* values,
                                int first_spread_index, int literal_index,
                                bool is_strong, int pos) {
    return new (local_zone_) ArrayLiteral(
        local_zone_, values, first_spread_index, literal_index, is_strong, pos);
  }

  VariableProxy* NewVariableProxy(Variable* var,
                                  int start_position = RelocInfo::kNoPosition,
                                  int end_position = RelocInfo::kNoPosition) {
    return new (parser_zone_)
        VariableProxy(parser_zone_, var, start_position, end_position);
  }

  VariableProxy* NewVariableProxy(const AstRawString* name,
                                  Variable::Kind variable_kind,
                                  int start_position = RelocInfo::kNoPosition,
                                  int end_position = RelocInfo::kNoPosition) {
    DCHECK_NOT_NULL(name);
    return new (parser_zone_) VariableProxy(parser_zone_, name, variable_kind,
                                            start_position, end_position);
  }

  Property* NewProperty(Expression* obj, Expression* key, int pos) {
    return new (local_zone_) Property(local_zone_, obj, key, pos);
  }

  Call* NewCall(Expression* expression,
                ZoneList<Expression*>* arguments,
                int pos) {
    return new (local_zone_) Call(local_zone_, expression, arguments, pos);
  }

  CallNew* NewCallNew(Expression* expression,
                      ZoneList<Expression*>* arguments,
                      int pos) {
    return new (local_zone_) CallNew(local_zone_, expression, arguments, pos);
  }

  CallRuntime* NewCallRuntime(Runtime::FunctionId id,
                              ZoneList<Expression*>* arguments, int pos) {
    return new (local_zone_)
        CallRuntime(local_zone_, Runtime::FunctionForId(id), arguments, pos);
  }

  CallRuntime* NewCallRuntime(const Runtime::Function* function,
                              ZoneList<Expression*>* arguments, int pos) {
    return new (local_zone_) CallRuntime(local_zone_, function, arguments, pos);
  }

  CallRuntime* NewCallRuntime(int context_index,
                              ZoneList<Expression*>* arguments, int pos) {
    return new (local_zone_)
        CallRuntime(local_zone_, context_index, arguments, pos);
  }

  UnaryOperation* NewUnaryOperation(Token::Value op,
                                    Expression* expression,
                                    int pos) {
    return new (local_zone_) UnaryOperation(local_zone_, op, expression, pos);
  }

  BinaryOperation* NewBinaryOperation(Token::Value op,
                                      Expression* left,
                                      Expression* right,
                                      int pos) {
    return new (local_zone_) BinaryOperation(local_zone_, op, left, right, pos);
  }

  CountOperation* NewCountOperation(Token::Value op,
                                    bool is_prefix,
                                    Expression* expr,
                                    int pos) {
    return new (local_zone_)
        CountOperation(local_zone_, op, is_prefix, expr, pos);
  }

  CompareOperation* NewCompareOperation(Token::Value op,
                                        Expression* left,
                                        Expression* right,
                                        int pos) {
    return new (local_zone_)
        CompareOperation(local_zone_, op, left, right, pos);
  }

  Spread* NewSpread(Expression* expression, int pos) {
    return new (local_zone_) Spread(local_zone_, expression, pos);
  }

  Conditional* NewConditional(Expression* condition,
                              Expression* then_expression,
                              Expression* else_expression,
                              int position) {
    return new (local_zone_) Conditional(
        local_zone_, condition, then_expression, else_expression, position);
  }

  RewritableAssignmentExpression* NewRewritableAssignmentExpression(
      Expression* expression) {
    DCHECK_NOT_NULL(expression);
    DCHECK(expression->IsAssignment());
    return new (local_zone_)
        RewritableAssignmentExpression(local_zone_, expression);
  }

  Assignment* NewAssignment(Token::Value op,
                            Expression* target,
                            Expression* value,
                            int pos) {
    DCHECK(Token::IsAssignmentOp(op));
    Assignment* assign =
        new (local_zone_) Assignment(local_zone_, op, target, value, pos);
    if (assign->is_compound()) {
      DCHECK(Token::IsAssignmentOp(op));
      assign->binary_operation_ =
          NewBinaryOperation(assign->binary_op(), target, value, pos + 1);
    }
    return assign;
  }

  Yield* NewYield(Expression *generator_object,
                  Expression* expression,
                  Yield::Kind yield_kind,
                  int pos) {
    if (!expression) expression = NewUndefinedLiteral(pos);
    return new (local_zone_)
        Yield(local_zone_, generator_object, expression, yield_kind, pos);
  }

  Throw* NewThrow(Expression* exception, int pos) {
    return new (local_zone_) Throw(local_zone_, exception, pos);
  }

  FunctionLiteral* NewFunctionLiteral(
      const AstRawString* name, Scope* scope, ZoneList<Statement*>* body,
      int materialized_literal_count, int expected_property_count,
      int parameter_count,
      FunctionLiteral::ParameterFlag has_duplicate_parameters,
      FunctionLiteral::FunctionType function_type,
      FunctionLiteral::EagerCompileHint eager_compile_hint, FunctionKind kind,
      int position) {
    return new (parser_zone_) FunctionLiteral(
        parser_zone_, name, ast_value_factory_, scope, body,
        materialized_literal_count, expected_property_count, parameter_count,
        function_type, has_duplicate_parameters, eager_compile_hint, kind,
        position);
  }

  ClassLiteral* NewClassLiteral(const AstRawString* name, Scope* scope,
                                VariableProxy* proxy, Expression* extends,
                                FunctionLiteral* constructor,
                                ZoneList<ObjectLiteral::Property*>* properties,
                                int start_position, int end_position) {
    return new (parser_zone_)
        ClassLiteral(parser_zone_, name, scope, proxy, extends, constructor,
                     properties, start_position, end_position);
  }

  NativeFunctionLiteral* NewNativeFunctionLiteral(const AstRawString* name,
                                                  v8::Extension* extension,
                                                  int pos) {
    return new (parser_zone_)
        NativeFunctionLiteral(parser_zone_, name, extension, pos);
  }

  DoExpression* NewDoExpression(Block* block, Variable* result_var, int pos) {
    VariableProxy* result = NewVariableProxy(result_var, pos);
    return new (parser_zone_) DoExpression(parser_zone_, block, result, pos);
  }

  ThisFunction* NewThisFunction(int pos) {
    return new (local_zone_) ThisFunction(local_zone_, pos);
  }

  SuperPropertyReference* NewSuperPropertyReference(VariableProxy* this_var,
                                                    Expression* home_object,
                                                    int pos) {
    return new (parser_zone_)
        SuperPropertyReference(parser_zone_, this_var, home_object, pos);
  }

  SuperCallReference* NewSuperCallReference(VariableProxy* this_var,
                                            VariableProxy* new_target_var,
                                            VariableProxy* this_function_var,
                                            int pos) {
    return new (parser_zone_) SuperCallReference(
        parser_zone_, this_var, new_target_var, this_function_var, pos);
  }

  EmptyParentheses* NewEmptyParentheses(int pos) {
    return new (local_zone_) EmptyParentheses(local_zone_, pos);
  }

  Zone* zone() const { return local_zone_; }

  // Handles use of temporary zones when parsing inner function bodies.
  class BodyScope {
   public:
    BodyScope(AstNodeFactory* factory, Zone* temp_zone, bool use_temp_zone)
        : factory_(factory), prev_zone_(factory->local_zone_) {
      if (use_temp_zone) {
        factory->local_zone_ = temp_zone;
      }
    }

    ~BodyScope() { factory_->local_zone_ = prev_zone_; }

   private:
    AstNodeFactory* factory_;
    Zone* prev_zone_;
  };

 private:
  // This zone may be deallocated upon returning from parsing a function body
  // which we can guarantee is not going to be compiled or have its AST
  // inspected.
  // See ParseFunctionLiteral in parser.cc for preconditions.
  Zone* local_zone_;
  // ZoneObjects which need to persist until scope analysis must be allocated in
  // the parser-level zone.
  Zone* parser_zone_;
  AstValueFactory* ast_value_factory_;
};


}  // namespace internal
}  // namespace v8

#endif  // V8_AST_AST_H_
