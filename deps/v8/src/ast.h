// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_AST_H_
#define V8_AST_H_

#include "src/v8.h"

#include "src/assembler.h"
#include "src/ast-value-factory.h"
#include "src/bailout-reason.h"
#include "src/factory.h"
#include "src/isolate.h"
#include "src/jsregexp.h"
#include "src/list-inl.h"
#include "src/modules.h"
#include "src/runtime/runtime.h"
#include "src/small-pointer-list.h"
#include "src/smart-pointers.h"
#include "src/token.h"
#include "src/types.h"
#include "src/utils.h"
#include "src/variables.h"

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
  V(ModuleDeclaration)           \
  V(ImportDeclaration)           \
  V(ExportDeclaration)

#define MODULE_NODE_LIST(V)                     \
  V(ModuleLiteral)                              \
  V(ModulePath)                                 \
  V(ModuleUrl)

#define STATEMENT_NODE_LIST(V)                  \
  V(Block)                                      \
  V(ModuleStatement)                            \
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
  V(ForOfStatement)                             \
  V(TryCatchStatement)                          \
  V(TryFinallyStatement)                        \
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
  V(ThisFunction)               \
  V(SuperReference)             \
  V(CaseClause)

#define AST_NODE_LIST(V)                        \
  DECLARATION_NODE_LIST(V)                      \
  MODULE_NODE_LIST(V)                           \
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

class RegExpAlternative;
class RegExpAssertion;
class RegExpAtom;
class RegExpBackReference;
class RegExpCapture;
class RegExpCharacterClass;
class RegExpCompiler;
class RegExpDisjunction;
class RegExpEmpty;
class RegExpLookahead;
class RegExpQuantifier;
class RegExpText;

#define DEF_FORWARD_DECLARATION(type) class type;
AST_NODE_LIST(DEF_FORWARD_DECLARATION)
#undef DEF_FORWARD_DECLARATION


// Typedef only introduced to avoid unreadable code.
// Please do appreciate the required space in "> >".
typedef ZoneList<Handle<String> > ZoneStringList;
typedef ZoneList<Handle<Object> > ZoneObjectList;


#define DECLARE_NODE_TYPE(type)                                          \
  void Accept(AstVisitor* v) OVERRIDE;                                   \
  AstNode::NodeType node_type() const FINAL { return AstNode::k##type; } \
  friend class AstNodeFactory;


enum AstPropertiesFlag {
  kDontSelfOptimize,
  kDontSoftInline,
  kDontCache
};


class FeedbackVectorRequirements {
 public:
  FeedbackVectorRequirements(int slots, int ic_slots)
      : slots_(slots), ic_slots_(ic_slots) {}

  int slots() const { return slots_; }
  int ic_slots() const { return ic_slots_; }

 private:
  int slots_;
  int ic_slots_;
};


class VariableICSlotPair FINAL {
 public:
  VariableICSlotPair(Variable* variable, FeedbackVectorICSlot slot)
      : variable_(variable), slot_(slot) {}
  VariableICSlotPair()
      : variable_(NULL), slot_(FeedbackVectorICSlot::Invalid()) {}

  Variable* variable() const { return variable_; }
  FeedbackVectorICSlot slot() const { return slot_; }

 private:
  Variable* variable_;
  FeedbackVectorICSlot slot_;
};


typedef List<VariableICSlotPair> ICSlotCache;


class AstProperties FINAL BASE_EMBEDDED {
 public:
  class Flags : public EnumSet<AstPropertiesFlag, int> {};

  explicit AstProperties(Zone* zone) : node_count_(0), spec_(zone) {}

  Flags* flags() { return &flags_; }
  int node_count() { return node_count_; }
  void add_node_count(int count) { node_count_ += count; }

  int slots() const { return spec_.slots(); }
  void increase_slots(int count) { spec_.increase_slots(count); }

  int ic_slots() const { return spec_.ic_slots(); }
  void increase_ic_slots(int count) { spec_.increase_ic_slots(count); }
  void SetKind(int ic_slot, Code::Kind kind) { spec_.SetKind(ic_slot, kind); }
  const ZoneFeedbackVectorSpec* get_spec() const { return &spec_; }

 private:
  Flags flags_;
  int node_count_;
  ZoneFeedbackVectorSpec spec_;
};


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
  virtual FeedbackVectorRequirements ComputeFeedbackRequirements(
      Isolate* isolate, const ICSlotCache* cache) {
    return FeedbackVectorRequirements(0, 0);
  }
  virtual void SetFirstFeedbackSlot(FeedbackVectorSlot slot) { UNREACHABLE(); }
  virtual void SetFirstFeedbackICSlot(FeedbackVectorICSlot slot,
                                      ICSlotCache* cache) {
    UNREACHABLE();
  }
  // Each ICSlot stores a kind of IC which the participating node should know.
  virtual Code::Kind FeedbackICSlotKind(int index) {
    UNREACHABLE();
    return Code::NUMBER_OF_KINDS;
  }

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
};


class SmallMapList FINAL {
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

  // Expression type bounds
  Bounds bounds() const { return bounds_; }
  void set_bounds(Bounds bounds) { bounds_ = bounds; }

  // Whether the expression is parenthesized
  bool is_parenthesized() const {
    return IsParenthesizedField::decode(bit_field_);
  }
  bool is_multi_parenthesized() const {
    return IsMultiParenthesizedField::decode(bit_field_);
  }
  void increase_parenthesization_level() {
    bit_field_ =
        IsMultiParenthesizedField::update(bit_field_, is_parenthesized());
    bit_field_ = IsParenthesizedField::update(bit_field_, true);
  }

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
  byte to_boolean_types() const {
    return ToBooleanTypesField::decode(bit_field_);
  }

  void set_base_id(int id) { base_id_ = id; }
  static int num_ids() { return parent_num_ids() + 2; }
  BailoutId id() const { return BailoutId(local_id(0)); }
  TypeFeedbackId test_id() const { return TypeFeedbackId(local_id(1)); }

 protected:
  Expression(Zone* zone, int pos)
      : AstNode(pos),
        base_id_(BailoutId::None().ToInt()),
        bounds_(Bounds::Unbounded(zone)),
        bit_field_(0) {}
  static int parent_num_ids() { return 0; }
  void set_to_boolean_types(byte types) {
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
  class ToBooleanTypesField : public BitField16<byte, 0, 8> {};
  class IsParenthesizedField : public BitField16<bool, 8, 1> {};
  class IsMultiParenthesizedField : public BitField16<bool, 9, 1> {};
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
  BreakableStatement* AsBreakableStatement() FINAL { return this; }

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


class Block FINAL : public BreakableStatement {
 public:
  DECLARE_NODE_TYPE(Block)

  void AddStatement(Statement* statement, Zone* zone) {
    statements_.Add(statement, zone);
  }

  ZoneList<Statement*>* statements() { return &statements_; }
  bool is_initializer_block() const { return is_initializer_block_; }

  static int num_ids() { return parent_num_ids() + 1; }
  BailoutId DeclsId() const { return BailoutId(local_id(0)); }

  bool IsJump() const OVERRIDE {
    return !statements_.is_empty() && statements_.last()->IsJump()
        && labels() == NULL;  // Good enough as an approximation...
  }

  Scope* scope() const { return scope_; }
  void set_scope(Scope* scope) { scope_ = scope; }

 protected:
  Block(Zone* zone, ZoneList<const AstRawString*>* labels, int capacity,
        bool is_initializer_block, int pos)
      : BreakableStatement(zone, labels, TARGET_FOR_NAMED_ONLY, pos),
        statements_(capacity, zone),
        is_initializer_block_(is_initializer_block),
        scope_(NULL) {}
  static int parent_num_ids() { return BreakableStatement::num_ids(); }

 private:
  int local_id(int n) const { return base_id() + parent_num_ids() + n; }

  ZoneList<Statement*> statements_;
  bool is_initializer_block_;
  Scope* scope_;
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


class VariableDeclaration FINAL : public Declaration {
 public:
  DECLARE_NODE_TYPE(VariableDeclaration)

  InitializationFlag initialization() const OVERRIDE {
    return mode() == VAR ? kCreatedInitialized : kNeedsInitialization;
  }

 protected:
  VariableDeclaration(Zone* zone,
                      VariableProxy* proxy,
                      VariableMode mode,
                      Scope* scope,
                      int pos)
      : Declaration(zone, proxy, mode, scope, pos) {
  }
};


class FunctionDeclaration FINAL : public Declaration {
 public:
  DECLARE_NODE_TYPE(FunctionDeclaration)

  FunctionLiteral* fun() const { return fun_; }
  InitializationFlag initialization() const OVERRIDE {
    return kCreatedInitialized;
  }
  bool IsInlineable() const OVERRIDE;

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


class ModuleDeclaration FINAL : public Declaration {
 public:
  DECLARE_NODE_TYPE(ModuleDeclaration)

  Module* module() const { return module_; }
  InitializationFlag initialization() const OVERRIDE {
    return kCreatedInitialized;
  }

 protected:
  ModuleDeclaration(Zone* zone, VariableProxy* proxy, Module* module,
                    Scope* scope, int pos)
      : Declaration(zone, proxy, CONST, scope, pos), module_(module) {}

 private:
  Module* module_;
};


class ImportDeclaration FINAL : public Declaration {
 public:
  DECLARE_NODE_TYPE(ImportDeclaration)

  const AstRawString* import_name() const { return import_name_; }
  const AstRawString* module_specifier() const { return module_specifier_; }
  void set_module_specifier(const AstRawString* module_specifier) {
    DCHECK(module_specifier_ == NULL);
    module_specifier_ = module_specifier;
  }
  InitializationFlag initialization() const OVERRIDE {
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


class ExportDeclaration FINAL : public Declaration {
 public:
  DECLARE_NODE_TYPE(ExportDeclaration)

  InitializationFlag initialization() const OVERRIDE {
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


class ModuleLiteral FINAL : public Module {
 public:
  DECLARE_NODE_TYPE(ModuleLiteral)

 protected:
  ModuleLiteral(Zone* zone, Block* body, ModuleDescriptor* descriptor, int pos)
      : Module(zone, descriptor, pos, body) {}
};


class ModulePath FINAL : public Module {
 public:
  DECLARE_NODE_TYPE(ModulePath)

  Module* module() const { return module_; }
  Handle<String> name() const { return name_->string(); }

 protected:
  ModulePath(Zone* zone, Module* module, const AstRawString* name, int pos)
      : Module(zone, pos), module_(module), name_(name) {}

 private:
  Module* module_;
  const AstRawString* name_;
};


class ModuleUrl FINAL : public Module {
 public:
  DECLARE_NODE_TYPE(ModuleUrl)

  Handle<String> url() const { return url_; }

 protected:
  ModuleUrl(Zone* zone, Handle<String> url, int pos)
      : Module(zone, pos), url_(url) {
  }

 private:
  Handle<String> url_;
};


class ModuleStatement FINAL : public Statement {
 public:
  DECLARE_NODE_TYPE(ModuleStatement)

  Block* body() const { return body_; }

 protected:
  ModuleStatement(Zone* zone, Block* body, int pos)
      : Statement(zone, pos), body_(body) {}

 private:
  Block* body_;
};


class IterationStatement : public BreakableStatement {
 public:
  // Type testing & conversion.
  IterationStatement* AsIterationStatement() FINAL { return this; }

  Statement* body() const { return body_; }

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


class DoWhileStatement FINAL : public IterationStatement {
 public:
  DECLARE_NODE_TYPE(DoWhileStatement)

  void Initialize(Expression* cond, Statement* body) {
    IterationStatement::Initialize(body);
    cond_ = cond;
  }

  Expression* cond() const { return cond_; }

  static int num_ids() { return parent_num_ids() + 2; }
  BailoutId ContinueId() const OVERRIDE { return BailoutId(local_id(0)); }
  BailoutId StackCheckId() const OVERRIDE { return BackEdgeId(); }
  BailoutId BackEdgeId() const { return BailoutId(local_id(1)); }

 protected:
  DoWhileStatement(Zone* zone, ZoneList<const AstRawString*>* labels, int pos)
      : IterationStatement(zone, labels, pos), cond_(NULL) {}
  static int parent_num_ids() { return IterationStatement::num_ids(); }

 private:
  int local_id(int n) const { return base_id() + parent_num_ids() + n; }

  Expression* cond_;
};


class WhileStatement FINAL : public IterationStatement {
 public:
  DECLARE_NODE_TYPE(WhileStatement)

  void Initialize(Expression* cond, Statement* body) {
    IterationStatement::Initialize(body);
    cond_ = cond;
  }

  Expression* cond() const { return cond_; }

  static int num_ids() { return parent_num_ids() + 1; }
  BailoutId ContinueId() const OVERRIDE { return EntryId(); }
  BailoutId StackCheckId() const OVERRIDE { return BodyId(); }
  BailoutId BodyId() const { return BailoutId(local_id(0)); }

 protected:
  WhileStatement(Zone* zone, ZoneList<const AstRawString*>* labels, int pos)
      : IterationStatement(zone, labels, pos), cond_(NULL) {}
  static int parent_num_ids() { return IterationStatement::num_ids(); }

 private:
  int local_id(int n) const { return base_id() + parent_num_ids() + n; }

  Expression* cond_;
};


class ForStatement FINAL : public IterationStatement {
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

  static int num_ids() { return parent_num_ids() + 2; }
  BailoutId ContinueId() const OVERRIDE { return BailoutId(local_id(0)); }
  BailoutId StackCheckId() const OVERRIDE { return BodyId(); }
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

 protected:
  ForEachStatement(Zone* zone, ZoneList<const AstRawString*>* labels, int pos)
      : IterationStatement(zone, labels, pos), each_(NULL), subject_(NULL) {}

 private:
  Expression* each_;
  Expression* subject_;
};


class ForInStatement FINAL : public ForEachStatement {
 public:
  DECLARE_NODE_TYPE(ForInStatement)

  Expression* enumerable() const {
    return subject();
  }

  // Type feedback information.
  virtual FeedbackVectorRequirements ComputeFeedbackRequirements(
      Isolate* isolate, const ICSlotCache* cache) OVERRIDE {
    return FeedbackVectorRequirements(1, 0);
  }
  void SetFirstFeedbackSlot(FeedbackVectorSlot slot) OVERRIDE {
    for_in_feedback_slot_ = slot;
  }

  FeedbackVectorSlot ForInFeedbackSlot() {
    DCHECK(!for_in_feedback_slot_.IsInvalid());
    return for_in_feedback_slot_;
  }

  enum ForInType { FAST_FOR_IN, SLOW_FOR_IN };
  ForInType for_in_type() const { return for_in_type_; }
  void set_for_in_type(ForInType type) { for_in_type_ = type; }

  static int num_ids() { return parent_num_ids() + 5; }
  BailoutId BodyId() const { return BailoutId(local_id(0)); }
  BailoutId PrepareId() const { return BailoutId(local_id(1)); }
  BailoutId EnumId() const { return BailoutId(local_id(2)); }
  BailoutId ToObjectId() const { return BailoutId(local_id(3)); }
  BailoutId AssignmentId() const { return BailoutId(local_id(4)); }
  BailoutId ContinueId() const OVERRIDE { return EntryId(); }
  BailoutId StackCheckId() const OVERRIDE { return BodyId(); }

 protected:
  ForInStatement(Zone* zone, ZoneList<const AstRawString*>* labels, int pos)
      : ForEachStatement(zone, labels, pos),
        for_in_type_(SLOW_FOR_IN),
        for_in_feedback_slot_(FeedbackVectorSlot::Invalid()) {}
  static int parent_num_ids() { return ForEachStatement::num_ids(); }

 private:
  int local_id(int n) const { return base_id() + parent_num_ids() + n; }

  ForInType for_in_type_;
  FeedbackVectorSlot for_in_feedback_slot_;
};


class ForOfStatement FINAL : public ForEachStatement {
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

  BailoutId ContinueId() const OVERRIDE { return EntryId(); }
  BailoutId StackCheckId() const OVERRIDE { return BackEdgeId(); }

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


class ExpressionStatement FINAL : public Statement {
 public:
  DECLARE_NODE_TYPE(ExpressionStatement)

  void set_expression(Expression* e) { expression_ = e; }
  Expression* expression() const { return expression_; }
  bool IsJump() const OVERRIDE { return expression_->IsThrow(); }

 protected:
  ExpressionStatement(Zone* zone, Expression* expression, int pos)
      : Statement(zone, pos), expression_(expression) { }

 private:
  Expression* expression_;
};


class JumpStatement : public Statement {
 public:
  bool IsJump() const FINAL { return true; }

 protected:
  explicit JumpStatement(Zone* zone, int pos) : Statement(zone, pos) {}
};


class ContinueStatement FINAL : public JumpStatement {
 public:
  DECLARE_NODE_TYPE(ContinueStatement)

  IterationStatement* target() const { return target_; }

 protected:
  explicit ContinueStatement(Zone* zone, IterationStatement* target, int pos)
      : JumpStatement(zone, pos), target_(target) { }

 private:
  IterationStatement* target_;
};


class BreakStatement FINAL : public JumpStatement {
 public:
  DECLARE_NODE_TYPE(BreakStatement)

  BreakableStatement* target() const { return target_; }

 protected:
  explicit BreakStatement(Zone* zone, BreakableStatement* target, int pos)
      : JumpStatement(zone, pos), target_(target) { }

 private:
  BreakableStatement* target_;
};


class ReturnStatement FINAL : public JumpStatement {
 public:
  DECLARE_NODE_TYPE(ReturnStatement)

  Expression* expression() const { return expression_; }

 protected:
  explicit ReturnStatement(Zone* zone, Expression* expression, int pos)
      : JumpStatement(zone, pos), expression_(expression) { }

 private:
  Expression* expression_;
};


class WithStatement FINAL : public Statement {
 public:
  DECLARE_NODE_TYPE(WithStatement)

  Scope* scope() { return scope_; }
  Expression* expression() const { return expression_; }
  Statement* statement() const { return statement_; }

  void set_base_id(int id) { base_id_ = id; }
  static int num_ids() { return parent_num_ids() + 1; }
  BailoutId EntryId() const { return BailoutId(local_id(0)); }

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


class CaseClause FINAL : public Expression {
 public:
  DECLARE_NODE_TYPE(CaseClause)

  bool is_default() const { return label_ == NULL; }
  Expression* label() const {
    CHECK(!is_default());
    return label_;
  }
  Label* body_target() { return &body_target_; }
  ZoneList<Statement*>* statements() const { return statements_; }

  static int num_ids() { return parent_num_ids() + 2; }
  BailoutId EntryId() const { return BailoutId(local_id(0)); }
  TypeFeedbackId CompareId() { return TypeFeedbackId(local_id(1)); }

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


class SwitchStatement FINAL : public BreakableStatement {
 public:
  DECLARE_NODE_TYPE(SwitchStatement)

  void Initialize(Expression* tag, ZoneList<CaseClause*>* cases) {
    tag_ = tag;
    cases_ = cases;
  }

  Expression* tag() const { return tag_; }
  ZoneList<CaseClause*>* cases() const { return cases_; }

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
class IfStatement FINAL : public Statement {
 public:
  DECLARE_NODE_TYPE(IfStatement)

  bool HasThenStatement() const { return !then_statement()->IsEmpty(); }
  bool HasElseStatement() const { return !else_statement()->IsEmpty(); }

  Expression* condition() const { return condition_; }
  Statement* then_statement() const { return then_statement_; }
  Statement* else_statement() const { return else_statement_; }

  bool IsJump() const OVERRIDE {
    return HasThenStatement() && then_statement()->IsJump()
        && HasElseStatement() && else_statement()->IsJump();
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
  int index() const { return index_; }
  Block* try_block() const { return try_block_; }

 protected:
  TryStatement(Zone* zone, int index, Block* try_block, int pos)
      : Statement(zone, pos), index_(index), try_block_(try_block) {}

 private:
  // Unique (per-function) index of this handler.  This is not an AST ID.
  int index_;

  Block* try_block_;
};


class TryCatchStatement FINAL : public TryStatement {
 public:
  DECLARE_NODE_TYPE(TryCatchStatement)

  Scope* scope() { return scope_; }
  Variable* variable() { return variable_; }
  Block* catch_block() const { return catch_block_; }

 protected:
  TryCatchStatement(Zone* zone,
                    int index,
                    Block* try_block,
                    Scope* scope,
                    Variable* variable,
                    Block* catch_block,
                    int pos)
      : TryStatement(zone, index, try_block, pos),
        scope_(scope),
        variable_(variable),
        catch_block_(catch_block) {
  }

 private:
  Scope* scope_;
  Variable* variable_;
  Block* catch_block_;
};


class TryFinallyStatement FINAL : public TryStatement {
 public:
  DECLARE_NODE_TYPE(TryFinallyStatement)

  Block* finally_block() const { return finally_block_; }

 protected:
  TryFinallyStatement(
      Zone* zone, int index, Block* try_block, Block* finally_block, int pos)
      : TryStatement(zone, index, try_block, pos),
        finally_block_(finally_block) { }

 private:
  Block* finally_block_;
};


class DebuggerStatement FINAL : public Statement {
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


class EmptyStatement FINAL : public Statement {
 public:
  DECLARE_NODE_TYPE(EmptyStatement)

 protected:
  explicit EmptyStatement(Zone* zone, int pos): Statement(zone, pos) {}
};


class Literal FINAL : public Expression {
 public:
  DECLARE_NODE_TYPE(Literal)

  bool IsPropertyName() const OVERRIDE { return value_->IsPropertyName(); }

  Handle<String> AsPropertyName() {
    DCHECK(IsPropertyName());
    return Handle<String>::cast(value());
  }

  const AstRawString* AsRawPropertyName() {
    DCHECK(IsPropertyName());
    return value_->AsString();
  }

  bool ToBooleanIsTrue() const OVERRIDE { return value()->BooleanValue(); }
  bool ToBooleanIsFalse() const OVERRIDE { return !value()->BooleanValue(); }

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


// Base class for literals that needs space in the corresponding JSFunction.
class MaterializedLiteral : public Expression {
 public:
  virtual MaterializedLiteral* AsMaterializedLiteral() { return this; }

  int literal_index() { return literal_index_; }

  int depth() const {
    // only callable after initialization.
    DCHECK(depth_ >= 1);
    return depth_;
  }

 protected:
  MaterializedLiteral(Zone* zone, int literal_index, int pos)
      : Expression(zone, pos),
        literal_index_(literal_index),
        is_simple_(false),
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
  int depth_;
};


// Property is used for passing information
// about an object literal's properties from the parser
// to the code generator.
class ObjectLiteralProperty FINAL : public ZoneObject {
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

  // Type feedback information.
  bool IsMonomorphic() { return !receiver_type_.is_null(); }
  Handle<Map> GetReceiverType() { return receiver_type_; }

  bool IsCompileTimeValue();

  void set_emit_store(bool emit_store);
  bool emit_store();

  bool is_static() const { return is_static_; }
  bool is_computed_name() const { return is_computed_name_; }

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
  Kind kind_;
  bool emit_store_;
  bool is_static_;
  bool is_computed_name_;
  Handle<Map> receiver_type_;
};


// An object literal has a boilerplate object that is used
// for minimizing the work when constructing it at runtime.
class ObjectLiteral FINAL : public MaterializedLiteral {
 public:
  typedef ObjectLiteralProperty Property;

  DECLARE_NODE_TYPE(ObjectLiteral)

  Handle<FixedArray> constant_properties() const {
    return constant_properties_;
  }
  ZoneList<Property*>* properties() const { return properties_; }
  bool fast_elements() const { return fast_elements_; }
  bool may_store_doubles() const { return may_store_doubles_; }
  bool has_function() const { return has_function_; }

  // Decide if a property should be in the object boilerplate.
  static bool IsBoilerplateProperty(Property* property);

  // Populate the constant properties fixed array.
  void BuildConstantProperties(Isolate* isolate);

  // Mark all computed expressions that are bound to a key that
  // is shadowed by a later occurrence of the same key. For the
  // marked expressions, no store code is emitted.
  void CalculateEmitStore(Zone* zone);

  // Assemble bitfield of flags for the CreateObjectLiteral helper.
  int ComputeFlags() const {
    int flags = fast_elements() ? kFastElements : kNoFlags;
    flags |= has_function() ? kHasFunction : kNoFlags;
    return flags;
  }

  enum Flags {
    kNoFlags = 0,
    kFastElements = 1,
    kHasFunction = 1 << 1
  };

  struct Accessors: public ZoneObject {
    Accessors() : getter(NULL), setter(NULL) {}
    Expression* getter;
    Expression* setter;
  };

  BailoutId CreateLiteralId() const { return BailoutId(local_id(0)); }

  // Return an AST id for a property that is used in simulate instructions.
  BailoutId GetIdForProperty(int i) { return BailoutId(local_id(i + 1)); }

  // Unlike other AST nodes, this number of bailout IDs allocated for an
  // ObjectLiteral can vary, so num_ids() is not a static method.
  int num_ids() const { return parent_num_ids() + 1 + properties()->length(); }

 protected:
  ObjectLiteral(Zone* zone, ZoneList<Property*>* properties, int literal_index,
                int boilerplate_properties, bool has_function, int pos)
      : MaterializedLiteral(zone, literal_index, pos),
        properties_(properties),
        boilerplate_properties_(boilerplate_properties),
        fast_elements_(false),
        may_store_doubles_(false),
        has_function_(has_function) {}
  static int parent_num_ids() { return MaterializedLiteral::num_ids(); }

 private:
  int local_id(int n) const { return base_id() + parent_num_ids() + n; }
  Handle<FixedArray> constant_properties_;
  ZoneList<Property*>* properties_;
  int boilerplate_properties_;
  bool fast_elements_;
  bool may_store_doubles_;
  bool has_function_;
};


// Node for capturing a regexp literal.
class RegExpLiteral FINAL : public MaterializedLiteral {
 public:
  DECLARE_NODE_TYPE(RegExpLiteral)

  Handle<String> pattern() const { return pattern_->string(); }
  Handle<String> flags() const { return flags_->string(); }

 protected:
  RegExpLiteral(Zone* zone, const AstRawString* pattern,
                const AstRawString* flags, int literal_index, int pos)
      : MaterializedLiteral(zone, literal_index, pos),
        pattern_(pattern),
        flags_(flags) {
    set_depth(1);
  }

 private:
  const AstRawString* pattern_;
  const AstRawString* flags_;
};


// An array literal has a literals object that is used
// for minimizing the work when constructing it at runtime.
class ArrayLiteral FINAL : public MaterializedLiteral {
 public:
  DECLARE_NODE_TYPE(ArrayLiteral)

  Handle<FixedArray> constant_elements() const { return constant_elements_; }
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
  int ComputeFlags() const {
    int flags = depth() == 1 ? kShallowElements : kNoFlags;
    flags |= ArrayLiteral::kDisableMementos;
    return flags;
  }

  enum Flags {
    kNoFlags = 0,
    kShallowElements = 1,
    kDisableMementos = 1 << 1
  };

 protected:
  ArrayLiteral(Zone* zone, ZoneList<Expression*>* values, int literal_index,
               int pos)
      : MaterializedLiteral(zone, literal_index, pos), values_(values) {}
  static int parent_num_ids() { return MaterializedLiteral::num_ids(); }

 private:
  int local_id(int n) const { return base_id() + parent_num_ids() + n; }

  Handle<FixedArray> constant_elements_;
  ZoneList<Expression*>* values_;
};


class VariableProxy FINAL : public Expression {
 public:
  DECLARE_NODE_TYPE(VariableProxy)

  bool IsValidReferenceExpression() const OVERRIDE { return !is_this(); }

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

  int end_position() const { return end_position_; }

  // Bind this proxy to the variable var.
  void BindTo(Variable* var);

  bool UsesVariableFeedbackSlot() const {
    return FLAG_vector_ics && (var()->IsUnallocated() || var()->IsLookupSlot());
  }

  virtual FeedbackVectorRequirements ComputeFeedbackRequirements(
      Isolate* isolate, const ICSlotCache* cache) OVERRIDE;

  void SetFirstFeedbackICSlot(FeedbackVectorICSlot slot,
                              ICSlotCache* cache) OVERRIDE;
  Code::Kind FeedbackICSlotKind(int index) OVERRIDE { return Code::LOAD_IC; }
  FeedbackVectorICSlot VariableFeedbackSlot() {
    DCHECK(!UsesVariableFeedbackSlot() || !variable_feedback_slot_.IsInvalid());
    return variable_feedback_slot_;
  }

 protected:
  VariableProxy(Zone* zone, Variable* var, int start_position,
                int end_position);

  VariableProxy(Zone* zone, const AstRawString* name,
                Variable::Kind variable_kind, int start_position,
                int end_position);

  class IsThisField : public BitField8<bool, 0, 1> {};
  class IsAssignedField : public BitField8<bool, 1, 1> {};
  class IsResolvedField : public BitField8<bool, 2, 1> {};

  // Start with 16-bit (or smaller) field, which should get packed together
  // with Expression's trailing 16-bit field.
  uint8_t bit_field_;
  FeedbackVectorICSlot variable_feedback_slot_;
  union {
    const AstRawString* raw_name_;  // if !is_resolved_
    Variable* var_;                 // if is_resolved_
  };
  // Position is stored in the AstNode superclass, but VariableProxy needs to
  // know its end position too (for error messages). It cannot be inferred from
  // the variable name length because it can contain escapes.
  int end_position_;
};


class Property FINAL : public Expression {
 public:
  DECLARE_NODE_TYPE(Property)

  bool IsValidReferenceExpression() const OVERRIDE { return true; }

  Expression* obj() const { return obj_; }
  Expression* key() const { return key_; }

  static int num_ids() { return parent_num_ids() + 2; }
  BailoutId LoadId() const { return BailoutId(local_id(0)); }
  TypeFeedbackId PropertyFeedbackId() { return TypeFeedbackId(local_id(1)); }

  bool IsStringAccess() const {
    return IsStringAccessField::decode(bit_field_);
  }

  // Type feedback information.
  bool IsMonomorphic() OVERRIDE { return receiver_types_.length() == 1; }
  SmallMapList* GetReceiverTypes() OVERRIDE { return &receiver_types_; }
  KeyedAccessStoreMode GetStoreMode() const OVERRIDE { return STANDARD_STORE; }
  IcCheckType GetKeyType() const OVERRIDE {
    return KeyTypeField::decode(bit_field_);
  }
  bool IsUninitialized() const {
    return !is_for_call() && HasNoTypeInformation();
  }
  bool HasNoTypeInformation() const {
    return IsUninitializedField::decode(bit_field_);
  }
  void set_is_uninitialized(bool b) {
    bit_field_ = IsUninitializedField::update(bit_field_, b);
  }
  void set_is_string_access(bool b) {
    bit_field_ = IsStringAccessField::update(bit_field_, b);
  }
  void set_key_type(IcCheckType key_type) {
    bit_field_ = KeyTypeField::update(bit_field_, key_type);
  }
  void mark_for_call() {
    bit_field_ = IsForCallField::update(bit_field_, true);
  }
  bool is_for_call() const { return IsForCallField::decode(bit_field_); }

  bool IsSuperAccess() {
    return obj()->IsSuperReference();
  }

  virtual FeedbackVectorRequirements ComputeFeedbackRequirements(
      Isolate* isolate, const ICSlotCache* cache) OVERRIDE {
    return FeedbackVectorRequirements(0, FLAG_vector_ics ? 1 : 0);
  }
  void SetFirstFeedbackICSlot(FeedbackVectorICSlot slot,
                              ICSlotCache* cache) OVERRIDE {
    property_feedback_slot_ = slot;
  }
  Code::Kind FeedbackICSlotKind(int index) OVERRIDE {
    return key()->IsPropertyName() ? Code::LOAD_IC : Code::KEYED_LOAD_IC;
  }

  FeedbackVectorICSlot PropertyFeedbackSlot() const {
    DCHECK(!FLAG_vector_ics || !property_feedback_slot_.IsInvalid());
    return property_feedback_slot_;
  }

 protected:
  Property(Zone* zone, Expression* obj, Expression* key, int pos)
      : Expression(zone, pos),
        bit_field_(IsForCallField::encode(false) |
                   IsUninitializedField::encode(false) |
                   IsStringAccessField::encode(false)),
        property_feedback_slot_(FeedbackVectorICSlot::Invalid()),
        obj_(obj),
        key_(key) {}
  static int parent_num_ids() { return Expression::num_ids(); }

 private:
  int local_id(int n) const { return base_id() + parent_num_ids() + n; }

  class IsForCallField : public BitField8<bool, 0, 1> {};
  class IsUninitializedField : public BitField8<bool, 1, 1> {};
  class IsStringAccessField : public BitField8<bool, 2, 1> {};
  class KeyTypeField : public BitField8<IcCheckType, 3, 1> {};
  uint8_t bit_field_;
  FeedbackVectorICSlot property_feedback_slot_;
  Expression* obj_;
  Expression* key_;
  SmallMapList receiver_types_;
};


class Call FINAL : public Expression {
 public:
  DECLARE_NODE_TYPE(Call)

  Expression* expression() const { return expression_; }
  ZoneList<Expression*>* arguments() const { return arguments_; }

  // Type feedback information.
  virtual FeedbackVectorRequirements ComputeFeedbackRequirements(
      Isolate* isolate, const ICSlotCache* cache) OVERRIDE;
  void SetFirstFeedbackICSlot(FeedbackVectorICSlot slot,
                              ICSlotCache* cache) OVERRIDE {
    ic_slot_or_slot_ = slot.ToInt();
  }
  void SetFirstFeedbackSlot(FeedbackVectorSlot slot) OVERRIDE {
    ic_slot_or_slot_ = slot.ToInt();
  }
  Code::Kind FeedbackICSlotKind(int index) OVERRIDE { return Code::CALL_IC; }

  FeedbackVectorSlot CallFeedbackSlot() const {
    DCHECK(ic_slot_or_slot_ != FeedbackVectorSlot::Invalid().ToInt());
    return FeedbackVectorSlot(ic_slot_or_slot_);
  }

  FeedbackVectorICSlot CallFeedbackICSlot() const {
    DCHECK(ic_slot_or_slot_ != FeedbackVectorICSlot::Invalid().ToInt());
    return FeedbackVectorICSlot(ic_slot_or_slot_);
  }

  SmallMapList* GetReceiverTypes() OVERRIDE {
    if (expression()->IsProperty()) {
      return expression()->AsProperty()->GetReceiverTypes();
    }
    return NULL;
  }

  bool IsMonomorphic() OVERRIDE {
    if (expression()->IsProperty()) {
      return expression()->AsProperty()->IsMonomorphic();
    }
    return !target_.is_null();
  }

  bool global_call() const {
    VariableProxy* proxy = expression_->AsVariableProxy();
    return proxy != NULL && proxy->var()->IsUnallocated();
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

  static int num_ids() { return parent_num_ids() + 2; }
  BailoutId ReturnId() const { return BailoutId(local_id(0)); }
  BailoutId EvalOrLookupId() const { return BailoutId(local_id(1)); }

  bool is_uninitialized() const {
    return IsUninitializedField::decode(bit_field_);
  }
  void set_is_uninitialized(bool b) {
    bit_field_ = IsUninitializedField::update(bit_field_, b);
  }

  enum CallType {
    POSSIBLY_EVAL_CALL,
    GLOBAL_CALL,
    LOOKUP_SLOT_CALL,
    PROPERTY_CALL,
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
        ic_slot_or_slot_(FeedbackVectorICSlot::Invalid().ToInt()),
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

  // We store this as an integer because we don't know if we have a slot or
  // an ic slot until scoping time.
  int ic_slot_or_slot_;
  Expression* expression_;
  ZoneList<Expression*>* arguments_;
  Handle<JSFunction> target_;
  Handle<AllocationSite> allocation_site_;
  class IsUninitializedField : public BitField8<bool, 0, 1> {};
  uint8_t bit_field_;
};


class CallNew FINAL : public Expression {
 public:
  DECLARE_NODE_TYPE(CallNew)

  Expression* expression() const { return expression_; }
  ZoneList<Expression*>* arguments() const { return arguments_; }

  // Type feedback information.
  virtual FeedbackVectorRequirements ComputeFeedbackRequirements(
      Isolate* isolate, const ICSlotCache* cache) OVERRIDE {
    return FeedbackVectorRequirements(FLAG_pretenuring_call_new ? 2 : 1, 0);
  }
  void SetFirstFeedbackSlot(FeedbackVectorSlot slot) OVERRIDE {
    callnew_feedback_slot_ = slot;
  }

  FeedbackVectorSlot CallNewFeedbackSlot() {
    DCHECK(!callnew_feedback_slot_.IsInvalid());
    return callnew_feedback_slot_;
  }
  FeedbackVectorSlot AllocationSiteFeedbackSlot() {
    DCHECK(FLAG_pretenuring_call_new);
    return CallNewFeedbackSlot().next();
  }

  bool IsMonomorphic() OVERRIDE { return is_monomorphic_; }
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
        is_monomorphic_(false),
        callnew_feedback_slot_(FeedbackVectorSlot::Invalid()) {}

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
class CallRuntime FINAL : public Expression {
 public:
  DECLARE_NODE_TYPE(CallRuntime)

  Handle<String> name() const { return raw_name_->string(); }
  const AstRawString* raw_name() const { return raw_name_; }
  const Runtime::Function* function() const { return function_; }
  ZoneList<Expression*>* arguments() const { return arguments_; }
  bool is_jsruntime() const { return function_ == NULL; }

  // Type feedback information.
  bool HasCallRuntimeFeedbackSlot() const {
    return FLAG_vector_ics && is_jsruntime();
  }
  virtual FeedbackVectorRequirements ComputeFeedbackRequirements(
      Isolate* isolate, const ICSlotCache* cache) OVERRIDE {
    return FeedbackVectorRequirements(0, HasCallRuntimeFeedbackSlot() ? 1 : 0);
  }
  void SetFirstFeedbackICSlot(FeedbackVectorICSlot slot,
                              ICSlotCache* cache) OVERRIDE {
    callruntime_feedback_slot_ = slot;
  }
  Code::Kind FeedbackICSlotKind(int index) OVERRIDE { return Code::LOAD_IC; }

  FeedbackVectorICSlot CallRuntimeFeedbackSlot() {
    DCHECK(!HasCallRuntimeFeedbackSlot() ||
           !callruntime_feedback_slot_.IsInvalid());
    return callruntime_feedback_slot_;
  }

  static int num_ids() { return parent_num_ids() + 1; }
  TypeFeedbackId CallRuntimeFeedbackId() const {
    return TypeFeedbackId(local_id(0));
  }

 protected:
  CallRuntime(Zone* zone, const AstRawString* name,
              const Runtime::Function* function,
              ZoneList<Expression*>* arguments, int pos)
      : Expression(zone, pos),
        raw_name_(name),
        function_(function),
        arguments_(arguments),
        callruntime_feedback_slot_(FeedbackVectorICSlot::Invalid()) {}
  static int parent_num_ids() { return Expression::num_ids(); }

 private:
  int local_id(int n) const { return base_id() + parent_num_ids() + n; }

  const AstRawString* raw_name_;
  const Runtime::Function* function_;
  ZoneList<Expression*>* arguments_;
  FeedbackVectorICSlot callruntime_feedback_slot_;
};


class UnaryOperation FINAL : public Expression {
 public:
  DECLARE_NODE_TYPE(UnaryOperation)

  Token::Value op() const { return op_; }
  Expression* expression() const { return expression_; }

  // For unary not (Token::NOT), the AST ids where true and false will
  // actually be materialized, respectively.
  static int num_ids() { return parent_num_ids() + 2; }
  BailoutId MaterializeTrueId() const { return BailoutId(local_id(0)); }
  BailoutId MaterializeFalseId() const { return BailoutId(local_id(1)); }

  virtual void RecordToBooleanTypeFeedback(
      TypeFeedbackOracle* oracle) OVERRIDE;

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


class BinaryOperation FINAL : public Expression {
 public:
  DECLARE_NODE_TYPE(BinaryOperation)

  Token::Value op() const { return static_cast<Token::Value>(op_); }
  Expression* left() const { return left_; }
  Expression* right() const { return right_; }
  Handle<AllocationSite> allocation_site() const { return allocation_site_; }
  void set_allocation_site(Handle<AllocationSite> allocation_site) {
    allocation_site_ = allocation_site;
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

  virtual void RecordToBooleanTypeFeedback(
      TypeFeedbackOracle* oracle) OVERRIDE;

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


class CountOperation FINAL : public Expression {
 public:
  DECLARE_NODE_TYPE(CountOperation)

  bool is_prefix() const { return IsPrefixField::decode(bit_field_); }
  bool is_postfix() const { return !is_prefix(); }

  Token::Value op() const { return TokenField::decode(bit_field_); }
  Token::Value binary_op() {
    return (op() == Token::INC) ? Token::ADD : Token::SUB;
  }

  Expression* expression() const { return expression_; }

  bool IsMonomorphic() OVERRIDE { return receiver_types_.length() == 1; }
  SmallMapList* GetReceiverTypes() OVERRIDE { return &receiver_types_; }
  IcCheckType GetKeyType() const OVERRIDE {
    return KeyTypeField::decode(bit_field_);
  }
  KeyedAccessStoreMode GetStoreMode() const OVERRIDE {
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

 protected:
  CountOperation(Zone* zone, Token::Value op, bool is_prefix, Expression* expr,
                 int pos)
      : Expression(zone, pos),
        bit_field_(IsPrefixField::encode(is_prefix) |
                   KeyTypeField::encode(ELEMENT) |
                   StoreModeField::encode(STANDARD_STORE) |
                   TokenField::encode(op)),
        type_(NULL),
        expression_(expr) {}
  static int parent_num_ids() { return Expression::num_ids(); }

 private:
  int local_id(int n) const { return base_id() + parent_num_ids() + n; }

  class IsPrefixField : public BitField16<bool, 0, 1> {};
  class KeyTypeField : public BitField16<IcCheckType, 1, 1> {};
  class StoreModeField : public BitField16<KeyedAccessStoreMode, 2, 4> {};
  class TokenField : public BitField16<Token::Value, 6, 8> {};

  // Starts with 16-bit field, which should get packed together with
  // Expression's trailing 16-bit field.
  uint16_t bit_field_;
  Type* type_;
  Expression* expression_;
  SmallMapList receiver_types_;
};


class CompareOperation FINAL : public Expression {
 public:
  DECLARE_NODE_TYPE(CompareOperation)

  Token::Value op() const { return op_; }
  Expression* left() const { return left_; }
  Expression* right() const { return right_; }

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


class Conditional FINAL : public Expression {
 public:
  DECLARE_NODE_TYPE(Conditional)

  Expression* condition() const { return condition_; }
  Expression* then_expression() const { return then_expression_; }
  Expression* else_expression() const { return else_expression_; }

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


class Assignment FINAL : public Expression {
 public:
  DECLARE_NODE_TYPE(Assignment)

  Assignment* AsSimpleAssignment() { return !is_compound() ? this : NULL; }

  Token::Value binary_op() const;

  Token::Value op() const { return TokenField::decode(bit_field_); }
  Expression* target() const { return target_; }
  Expression* value() const { return value_; }
  BinaryOperation* binary_operation() const { return binary_operation_; }

  // This check relies on the definition order of token in token.h.
  bool is_compound() const { return op() > Token::ASSIGN; }

  static int num_ids() { return parent_num_ids() + 2; }
  BailoutId AssignmentId() const { return BailoutId(local_id(0)); }

  // Type feedback information.
  TypeFeedbackId AssignmentFeedbackId() { return TypeFeedbackId(local_id(1)); }
  bool IsMonomorphic() OVERRIDE { return receiver_types_.length() == 1; }
  bool IsUninitialized() const {
    return IsUninitializedField::decode(bit_field_);
  }
  bool HasNoTypeInformation() {
    return IsUninitializedField::decode(bit_field_);
  }
  SmallMapList* GetReceiverTypes() OVERRIDE { return &receiver_types_; }
  IcCheckType GetKeyType() const OVERRIDE {
    return KeyTypeField::decode(bit_field_);
  }
  KeyedAccessStoreMode GetStoreMode() const OVERRIDE {
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

 protected:
  Assignment(Zone* zone, Token::Value op, Expression* target, Expression* value,
             int pos);
  static int parent_num_ids() { return Expression::num_ids(); }

 private:
  int local_id(int n) const { return base_id() + parent_num_ids() + n; }

  class IsUninitializedField : public BitField16<bool, 0, 1> {};
  class KeyTypeField : public BitField16<IcCheckType, 1, 1> {};
  class StoreModeField : public BitField16<KeyedAccessStoreMode, 2, 4> {};
  class TokenField : public BitField16<Token::Value, 6, 8> {};

  // Starts with 16-bit field, which should get packed together with
  // Expression's trailing 16-bit field.
  uint16_t bit_field_;
  Expression* target_;
  Expression* value_;
  BinaryOperation* binary_operation_;
  SmallMapList receiver_types_;
};


class Yield FINAL : public Expression {
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

  // Delegating yield surrounds the "yield" in a "try/catch".  This index
  // locates the catch handler in the handler table, and is equivalent to
  // TryCatchStatement::index().
  int index() const {
    DCHECK_EQ(kDelegating, yield_kind());
    return index_;
  }
  void set_index(int index) {
    DCHECK_EQ(kDelegating, yield_kind());
    index_ = index;
  }

  // Type feedback information.
  bool HasFeedbackSlots() const {
    return FLAG_vector_ics && (yield_kind() == kDelegating);
  }
  virtual FeedbackVectorRequirements ComputeFeedbackRequirements(
      Isolate* isolate, const ICSlotCache* cache) OVERRIDE {
    return FeedbackVectorRequirements(0, HasFeedbackSlots() ? 3 : 0);
  }
  void SetFirstFeedbackICSlot(FeedbackVectorICSlot slot,
                              ICSlotCache* cache) OVERRIDE {
    yield_first_feedback_slot_ = slot;
  }
  Code::Kind FeedbackICSlotKind(int index) OVERRIDE {
    return index == 0 ? Code::KEYED_LOAD_IC : Code::LOAD_IC;
  }

  FeedbackVectorICSlot KeyedLoadFeedbackSlot() {
    DCHECK(!HasFeedbackSlots() || !yield_first_feedback_slot_.IsInvalid());
    return yield_first_feedback_slot_;
  }

  FeedbackVectorICSlot DoneFeedbackSlot() {
    return KeyedLoadFeedbackSlot().next();
  }

  FeedbackVectorICSlot ValueFeedbackSlot() { return DoneFeedbackSlot().next(); }

 protected:
  Yield(Zone* zone, Expression* generator_object, Expression* expression,
        Kind yield_kind, int pos)
      : Expression(zone, pos),
        generator_object_(generator_object),
        expression_(expression),
        yield_kind_(yield_kind),
        index_(-1),
        yield_first_feedback_slot_(FeedbackVectorICSlot::Invalid()) {}

 private:
  Expression* generator_object_;
  Expression* expression_;
  Kind yield_kind_;
  int index_;
  FeedbackVectorICSlot yield_first_feedback_slot_;
};


class Throw FINAL : public Expression {
 public:
  DECLARE_NODE_TYPE(Throw)

  Expression* exception() const { return exception_; }

 protected:
  Throw(Zone* zone, Expression* exception, int pos)
      : Expression(zone, pos), exception_(exception) {}

 private:
  Expression* exception_;
};


class FunctionLiteral FINAL : public Expression {
 public:
  enum FunctionType {
    ANONYMOUS_EXPRESSION,
    NAMED_EXPRESSION,
    DECLARATION
  };

  enum ParameterFlag {
    kNoDuplicateParameters = 0,
    kHasDuplicateParameters = 1
  };

  enum IsFunctionFlag {
    kGlobalOrEval,
    kIsFunction
  };

  enum IsParenthesizedFlag {
    kIsParenthesized,
    kNotParenthesized
  };

  enum ArityRestriction {
    NORMAL_ARITY,
    GETTER_ARITY,
    SETTER_ARITY
  };

  DECLARE_NODE_TYPE(FunctionLiteral)

  Handle<String> name() const { return raw_name_->string(); }
  const AstRawString* raw_name() const { return raw_name_; }
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
  bool uses_super_property() const;

  static bool NeedsHomeObject(Expression* literal) {
    return literal != NULL && literal->IsFunctionLiteral() &&
           literal->AsFunctionLiteral()->uses_super_property();
  }

  int materialized_literal_count() { return materialized_literal_count_; }
  int expected_property_count() { return expected_property_count_; }
  int handler_count() { return handler_count_; }
  int parameter_count() { return parameter_count_; }

  bool AllowsLazyCompilation();
  bool AllowsLazyCompilationWithoutContext();

  void InitializeSharedInfo(Handle<Code> code);

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

  // shared_info may be null if it's not cached in full code.
  Handle<SharedFunctionInfo> shared_info() { return shared_info_; }

  bool pretenure() { return Pretenure::decode(bitfield_); }
  void set_pretenure() { bitfield_ |= Pretenure::encode(true); }

  bool has_duplicate_parameters() {
    return HasDuplicateParameters::decode(bitfield_);
  }

  bool is_function() { return IsFunction::decode(bitfield_) == kIsFunction; }

  // This is used as a heuristic on when to eagerly compile a function
  // literal. We consider the following constructs as hints that the
  // function will be called immediately:
  // - (function() { ... })();
  // - var x = function() { ... }();
  bool is_parenthesized() {
    return IsParenthesized::decode(bitfield_) == kIsParenthesized;
  }
  void set_parenthesized() {
    bitfield_ = IsParenthesized::update(bitfield_, kIsParenthesized);
  }

  FunctionKind kind() { return FunctionKindBits::decode(bitfield_); }

  int ast_node_count() { return ast_properties_.node_count(); }
  AstProperties::Flags* flags() { return ast_properties_.flags(); }
  void set_ast_properties(AstProperties* ast_properties) {
    ast_properties_ = *ast_properties;
  }
  const ZoneFeedbackVectorSpec* feedback_vector_spec() const {
    return ast_properties_.get_spec();
  }
  bool dont_optimize() { return dont_optimize_reason_ != kNoReason; }
  BailoutReason dont_optimize_reason() { return dont_optimize_reason_; }
  void set_dont_optimize_reason(BailoutReason reason) {
    dont_optimize_reason_ = reason;
  }

 protected:
  FunctionLiteral(Zone* zone, const AstRawString* name,
                  AstValueFactory* ast_value_factory, Scope* scope,
                  ZoneList<Statement*>* body, int materialized_literal_count,
                  int expected_property_count, int handler_count,
                  int parameter_count, FunctionType function_type,
                  ParameterFlag has_duplicate_parameters,
                  IsFunctionFlag is_function,
                  IsParenthesizedFlag is_parenthesized, FunctionKind kind,
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
        handler_count_(handler_count),
        parameter_count_(parameter_count),
        function_token_position_(RelocInfo::kNoPosition) {
    bitfield_ = IsExpression::encode(function_type != DECLARATION) |
                IsAnonymous::encode(function_type == ANONYMOUS_EXPRESSION) |
                Pretenure::encode(false) |
                HasDuplicateParameters::encode(has_duplicate_parameters) |
                IsFunction::encode(is_function) |
                IsParenthesized::encode(is_parenthesized) |
                FunctionKindBits::encode(kind);
    DCHECK(IsValidFunctionKind(kind));
  }

 private:
  const AstRawString* raw_name_;
  Handle<String> name_;
  Handle<SharedFunctionInfo> shared_info_;
  Scope* scope_;
  ZoneList<Statement*>* body_;
  const AstString* raw_inferred_name_;
  Handle<String> inferred_name_;
  AstProperties ast_properties_;
  BailoutReason dont_optimize_reason_;

  int materialized_literal_count_;
  int expected_property_count_;
  int handler_count_;
  int parameter_count_;
  int function_token_position_;

  unsigned bitfield_;
  class IsExpression : public BitField<bool, 0, 1> {};
  class IsAnonymous : public BitField<bool, 1, 1> {};
  class Pretenure : public BitField<bool, 2, 1> {};
  class HasDuplicateParameters : public BitField<ParameterFlag, 3, 1> {};
  class IsFunction : public BitField<IsFunctionFlag, 4, 1> {};
  class IsParenthesized : public BitField<IsParenthesizedFlag, 5, 1> {};
  class FunctionKindBits : public BitField<FunctionKind, 6, 8> {};
};


class ClassLiteral FINAL : public Expression {
 public:
  typedef ObjectLiteralProperty Property;

  DECLARE_NODE_TYPE(ClassLiteral)

  Handle<String> name() const { return raw_name_->string(); }
  const AstRawString* raw_name() const { return raw_name_; }
  Scope* scope() const { return scope_; }
  VariableProxy* class_variable_proxy() const { return class_variable_proxy_; }
  Expression* extends() const { return extends_; }
  FunctionLiteral* constructor() const { return constructor_; }
  ZoneList<Property*>* properties() const { return properties_; }
  int start_position() const { return position(); }
  int end_position() const { return end_position_; }

  BailoutId EntryId() const { return BailoutId(local_id(0)); }
  BailoutId DeclsId() const { return BailoutId(local_id(1)); }
  BailoutId ExitId() { return BailoutId(local_id(2)); }

  // Return an AST id for a property that is used in simulate instructions.
  BailoutId GetIdForProperty(int i) { return BailoutId(local_id(i + 3)); }

  // Unlike other AST nodes, this number of bailout IDs allocated for an
  // ClassLiteral can vary, so num_ids() is not a static method.
  int num_ids() const { return parent_num_ids() + 3 + properties()->length(); }

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
};


class NativeFunctionLiteral FINAL : public Expression {
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


class ThisFunction FINAL : public Expression {
 public:
  DECLARE_NODE_TYPE(ThisFunction)

 protected:
  ThisFunction(Zone* zone, int pos) : Expression(zone, pos) {}
};


class SuperReference FINAL : public Expression {
 public:
  DECLARE_NODE_TYPE(SuperReference)

  VariableProxy* this_var() const { return this_var_; }

  static int num_ids() { return parent_num_ids() + 1; }
  TypeFeedbackId HomeObjectFeedbackId() { return TypeFeedbackId(local_id(0)); }

  // Type feedback information.
  virtual FeedbackVectorRequirements ComputeFeedbackRequirements(
      Isolate* isolate, const ICSlotCache* cache) OVERRIDE {
    return FeedbackVectorRequirements(0, FLAG_vector_ics ? 1 : 0);
  }
  void SetFirstFeedbackICSlot(FeedbackVectorICSlot slot,
                              ICSlotCache* cache) OVERRIDE {
    homeobject_feedback_slot_ = slot;
  }
  Code::Kind FeedbackICSlotKind(int index) OVERRIDE { return Code::LOAD_IC; }

  FeedbackVectorICSlot HomeObjectFeedbackSlot() {
    DCHECK(!FLAG_vector_ics || !homeobject_feedback_slot_.IsInvalid());
    return homeobject_feedback_slot_;
  }

 protected:
  SuperReference(Zone* zone, VariableProxy* this_var, int pos)
      : Expression(zone, pos),
        this_var_(this_var),
        homeobject_feedback_slot_(FeedbackVectorICSlot::Invalid()) {
    DCHECK(this_var->is_this());
  }
  static int parent_num_ids() { return Expression::num_ids(); }

 private:
  int local_id(int n) const { return base_id() + parent_num_ids() + n; }

  VariableProxy* this_var_;
  FeedbackVectorICSlot homeobject_feedback_slot_;
};


#undef DECLARE_NODE_TYPE


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


class RegExpTree : public ZoneObject {
 public:
  static const int kInfinity = kMaxInt;
  virtual ~RegExpTree() {}
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
  virtual void AppendToText(RegExpText* text, Zone* zone);
  std::ostream& Print(std::ostream& os, Zone* zone);  // NOLINT
#define MAKE_ASTYPE(Name)                                                  \
  virtual RegExp##Name* As##Name();                                        \
  virtual bool Is##Name();
  FOR_EACH_REG_EXP_TREE_TYPE(MAKE_ASTYPE)
#undef MAKE_ASTYPE
};


class RegExpDisjunction FINAL : public RegExpTree {
 public:
  explicit RegExpDisjunction(ZoneList<RegExpTree*>* alternatives);
  void* Accept(RegExpVisitor* visitor, void* data) OVERRIDE;
  virtual RegExpNode* ToNode(RegExpCompiler* compiler,
                             RegExpNode* on_success) OVERRIDE;
  RegExpDisjunction* AsDisjunction() OVERRIDE;
  Interval CaptureRegisters() OVERRIDE;
  bool IsDisjunction() OVERRIDE;
  bool IsAnchoredAtStart() OVERRIDE;
  bool IsAnchoredAtEnd() OVERRIDE;
  int min_match() OVERRIDE { return min_match_; }
  int max_match() OVERRIDE { return max_match_; }
  ZoneList<RegExpTree*>* alternatives() { return alternatives_; }
 private:
  ZoneList<RegExpTree*>* alternatives_;
  int min_match_;
  int max_match_;
};


class RegExpAlternative FINAL : public RegExpTree {
 public:
  explicit RegExpAlternative(ZoneList<RegExpTree*>* nodes);
  void* Accept(RegExpVisitor* visitor, void* data) OVERRIDE;
  virtual RegExpNode* ToNode(RegExpCompiler* compiler,
                             RegExpNode* on_success) OVERRIDE;
  RegExpAlternative* AsAlternative() OVERRIDE;
  Interval CaptureRegisters() OVERRIDE;
  bool IsAlternative() OVERRIDE;
  bool IsAnchoredAtStart() OVERRIDE;
  bool IsAnchoredAtEnd() OVERRIDE;
  int min_match() OVERRIDE { return min_match_; }
  int max_match() OVERRIDE { return max_match_; }
  ZoneList<RegExpTree*>* nodes() { return nodes_; }
 private:
  ZoneList<RegExpTree*>* nodes_;
  int min_match_;
  int max_match_;
};


class RegExpAssertion FINAL : public RegExpTree {
 public:
  enum AssertionType {
    START_OF_LINE,
    START_OF_INPUT,
    END_OF_LINE,
    END_OF_INPUT,
    BOUNDARY,
    NON_BOUNDARY
  };
  explicit RegExpAssertion(AssertionType type) : assertion_type_(type) { }
  void* Accept(RegExpVisitor* visitor, void* data) OVERRIDE;
  virtual RegExpNode* ToNode(RegExpCompiler* compiler,
                             RegExpNode* on_success) OVERRIDE;
  RegExpAssertion* AsAssertion() OVERRIDE;
  bool IsAssertion() OVERRIDE;
  bool IsAnchoredAtStart() OVERRIDE;
  bool IsAnchoredAtEnd() OVERRIDE;
  int min_match() OVERRIDE { return 0; }
  int max_match() OVERRIDE { return 0; }
  AssertionType assertion_type() { return assertion_type_; }
 private:
  AssertionType assertion_type_;
};


class CharacterSet FINAL BASE_EMBEDDED {
 public:
  explicit CharacterSet(uc16 standard_set_type)
      : ranges_(NULL),
        standard_set_type_(standard_set_type) {}
  explicit CharacterSet(ZoneList<CharacterRange>* ranges)
      : ranges_(ranges),
        standard_set_type_(0) {}
  ZoneList<CharacterRange>* ranges(Zone* zone);
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


class RegExpCharacterClass FINAL : public RegExpTree {
 public:
  RegExpCharacterClass(ZoneList<CharacterRange>* ranges, bool is_negated)
      : set_(ranges),
        is_negated_(is_negated) { }
  explicit RegExpCharacterClass(uc16 type)
      : set_(type),
        is_negated_(false) { }
  void* Accept(RegExpVisitor* visitor, void* data) OVERRIDE;
  virtual RegExpNode* ToNode(RegExpCompiler* compiler,
                             RegExpNode* on_success) OVERRIDE;
  RegExpCharacterClass* AsCharacterClass() OVERRIDE;
  bool IsCharacterClass() OVERRIDE;
  bool IsTextElement() OVERRIDE { return true; }
  int min_match() OVERRIDE { return 1; }
  int max_match() OVERRIDE { return 1; }
  void AppendToText(RegExpText* text, Zone* zone) OVERRIDE;
  CharacterSet character_set() { return set_; }
  // TODO(lrn): Remove need for complex version if is_standard that
  // recognizes a mangled standard set and just do { return set_.is_special(); }
  bool is_standard(Zone* zone);
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
  ZoneList<CharacterRange>* ranges(Zone* zone) { return set_.ranges(zone); }
  bool is_negated() { return is_negated_; }

 private:
  CharacterSet set_;
  bool is_negated_;
};


class RegExpAtom FINAL : public RegExpTree {
 public:
  explicit RegExpAtom(Vector<const uc16> data) : data_(data) { }
  void* Accept(RegExpVisitor* visitor, void* data) OVERRIDE;
  virtual RegExpNode* ToNode(RegExpCompiler* compiler,
                             RegExpNode* on_success) OVERRIDE;
  RegExpAtom* AsAtom() OVERRIDE;
  bool IsAtom() OVERRIDE;
  bool IsTextElement() OVERRIDE { return true; }
  int min_match() OVERRIDE { return data_.length(); }
  int max_match() OVERRIDE { return data_.length(); }
  void AppendToText(RegExpText* text, Zone* zone) OVERRIDE;
  Vector<const uc16> data() { return data_; }
  int length() { return data_.length(); }
 private:
  Vector<const uc16> data_;
};


class RegExpText FINAL : public RegExpTree {
 public:
  explicit RegExpText(Zone* zone) : elements_(2, zone), length_(0) {}
  void* Accept(RegExpVisitor* visitor, void* data) OVERRIDE;
  virtual RegExpNode* ToNode(RegExpCompiler* compiler,
                             RegExpNode* on_success) OVERRIDE;
  RegExpText* AsText() OVERRIDE;
  bool IsText() OVERRIDE;
  bool IsTextElement() OVERRIDE { return true; }
  int min_match() OVERRIDE { return length_; }
  int max_match() OVERRIDE { return length_; }
  void AppendToText(RegExpText* text, Zone* zone) OVERRIDE;
  void AddElement(TextElement elm, Zone* zone)  {
    elements_.Add(elm, zone);
    length_ += elm.length();
  }
  ZoneList<TextElement>* elements() { return &elements_; }
 private:
  ZoneList<TextElement> elements_;
  int length_;
};


class RegExpQuantifier FINAL : public RegExpTree {
 public:
  enum QuantifierType { GREEDY, NON_GREEDY, POSSESSIVE };
  RegExpQuantifier(int min, int max, QuantifierType type, RegExpTree* body)
      : body_(body),
        min_(min),
        max_(max),
        min_match_(min * body->min_match()),
        quantifier_type_(type) {
    if (max > 0 && body->max_match() > kInfinity / max) {
      max_match_ = kInfinity;
    } else {
      max_match_ = max * body->max_match();
    }
  }
  void* Accept(RegExpVisitor* visitor, void* data) OVERRIDE;
  virtual RegExpNode* ToNode(RegExpCompiler* compiler,
                             RegExpNode* on_success) OVERRIDE;
  static RegExpNode* ToNode(int min,
                            int max,
                            bool is_greedy,
                            RegExpTree* body,
                            RegExpCompiler* compiler,
                            RegExpNode* on_success,
                            bool not_at_start = false);
  RegExpQuantifier* AsQuantifier() OVERRIDE;
  Interval CaptureRegisters() OVERRIDE;
  bool IsQuantifier() OVERRIDE;
  int min_match() OVERRIDE { return min_match_; }
  int max_match() OVERRIDE { return max_match_; }
  int min() { return min_; }
  int max() { return max_; }
  bool is_possessive() { return quantifier_type_ == POSSESSIVE; }
  bool is_non_greedy() { return quantifier_type_ == NON_GREEDY; }
  bool is_greedy() { return quantifier_type_ == GREEDY; }
  RegExpTree* body() { return body_; }

 private:
  RegExpTree* body_;
  int min_;
  int max_;
  int min_match_;
  int max_match_;
  QuantifierType quantifier_type_;
};


class RegExpCapture FINAL : public RegExpTree {
 public:
  explicit RegExpCapture(RegExpTree* body, int index)
      : body_(body), index_(index) { }
  void* Accept(RegExpVisitor* visitor, void* data) OVERRIDE;
  virtual RegExpNode* ToNode(RegExpCompiler* compiler,
                             RegExpNode* on_success) OVERRIDE;
  static RegExpNode* ToNode(RegExpTree* body,
                            int index,
                            RegExpCompiler* compiler,
                            RegExpNode* on_success);
  RegExpCapture* AsCapture() OVERRIDE;
  bool IsAnchoredAtStart() OVERRIDE;
  bool IsAnchoredAtEnd() OVERRIDE;
  Interval CaptureRegisters() OVERRIDE;
  bool IsCapture() OVERRIDE;
  int min_match() OVERRIDE { return body_->min_match(); }
  int max_match() OVERRIDE { return body_->max_match(); }
  RegExpTree* body() { return body_; }
  int index() { return index_; }
  static int StartRegister(int index) { return index * 2; }
  static int EndRegister(int index) { return index * 2 + 1; }

 private:
  RegExpTree* body_;
  int index_;
};


class RegExpLookahead FINAL : public RegExpTree {
 public:
  RegExpLookahead(RegExpTree* body,
                  bool is_positive,
                  int capture_count,
                  int capture_from)
      : body_(body),
        is_positive_(is_positive),
        capture_count_(capture_count),
        capture_from_(capture_from) { }

  void* Accept(RegExpVisitor* visitor, void* data) OVERRIDE;
  virtual RegExpNode* ToNode(RegExpCompiler* compiler,
                             RegExpNode* on_success) OVERRIDE;
  RegExpLookahead* AsLookahead() OVERRIDE;
  Interval CaptureRegisters() OVERRIDE;
  bool IsLookahead() OVERRIDE;
  bool IsAnchoredAtStart() OVERRIDE;
  int min_match() OVERRIDE { return 0; }
  int max_match() OVERRIDE { return 0; }
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


class RegExpBackReference FINAL : public RegExpTree {
 public:
  explicit RegExpBackReference(RegExpCapture* capture)
      : capture_(capture) { }
  void* Accept(RegExpVisitor* visitor, void* data) OVERRIDE;
  virtual RegExpNode* ToNode(RegExpCompiler* compiler,
                             RegExpNode* on_success) OVERRIDE;
  RegExpBackReference* AsBackReference() OVERRIDE;
  bool IsBackReference() OVERRIDE;
  int min_match() OVERRIDE { return 0; }
  int max_match() OVERRIDE { return capture_->max_match(); }
  int index() { return capture_->index(); }
  RegExpCapture* capture() { return capture_; }
 private:
  RegExpCapture* capture_;
};


class RegExpEmpty FINAL : public RegExpTree {
 public:
  RegExpEmpty() { }
  void* Accept(RegExpVisitor* visitor, void* data) OVERRIDE;
  virtual RegExpNode* ToNode(RegExpCompiler* compiler,
                             RegExpNode* on_success) OVERRIDE;
  RegExpEmpty* AsEmpty() OVERRIDE;
  bool IsEmpty() OVERRIDE;
  int min_match() OVERRIDE { return 0; }
  int max_match() OVERRIDE { return 0; }
};


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
  void Visit(AstNode* node) FINAL {                         \
    if (!CheckStackOverflow()) node->Accept(this);          \
  }                                                         \
                                                            \
  void SetStackOverflow() { stack_overflow_ = true; }       \
  void ClearStackOverflow() { stack_overflow_ = false; }    \
  bool HasStackOverflow() const { return stack_overflow_; } \
                                                            \
  bool CheckStackOverflow() {                               \
    if (stack_overflow_) return true;                       \
    StackLimitCheck check(isolate_);                        \
    if (!check.HasOverflowed()) return false;               \
    stack_overflow_ = true;                                 \
    return true;                                            \
  }                                                         \
                                                            \
 private:                                                   \
  void InitializeAstVisitor(Isolate* isolate, Zone* zone) { \
    isolate_ = isolate;                                     \
    zone_ = zone;                                           \
    stack_overflow_ = false;                                \
  }                                                         \
  Zone* zone() { return zone_; }                            \
  Isolate* isolate() { return isolate_; }                   \
                                                            \
  Isolate* isolate_;                                        \
  Zone* zone_;                                              \
  bool stack_overflow_


// ----------------------------------------------------------------------------
// AstNode factory

class AstNodeFactory FINAL BASE_EMBEDDED {
 public:
  explicit AstNodeFactory(AstValueFactory* ast_value_factory)
      : zone_(ast_value_factory->zone()),
        ast_value_factory_(ast_value_factory) {}

  VariableDeclaration* NewVariableDeclaration(VariableProxy* proxy,
                                              VariableMode mode,
                                              Scope* scope,
                                              int pos) {
    return new (zone_) VariableDeclaration(zone_, proxy, mode, scope, pos);
  }

  FunctionDeclaration* NewFunctionDeclaration(VariableProxy* proxy,
                                              VariableMode mode,
                                              FunctionLiteral* fun,
                                              Scope* scope,
                                              int pos) {
    return new (zone_) FunctionDeclaration(zone_, proxy, mode, fun, scope, pos);
  }

  ModuleDeclaration* NewModuleDeclaration(VariableProxy* proxy,
                                          Module* module,
                                          Scope* scope,
                                          int pos) {
    return new (zone_) ModuleDeclaration(zone_, proxy, module, scope, pos);
  }

  ImportDeclaration* NewImportDeclaration(VariableProxy* proxy,
                                          const AstRawString* import_name,
                                          const AstRawString* module_specifier,
                                          Scope* scope, int pos) {
    return new (zone_) ImportDeclaration(zone_, proxy, import_name,
                                         module_specifier, scope, pos);
  }

  ExportDeclaration* NewExportDeclaration(VariableProxy* proxy,
                                          Scope* scope,
                                          int pos) {
    return new (zone_) ExportDeclaration(zone_, proxy, scope, pos);
  }

  ModuleLiteral* NewModuleLiteral(Block* body, ModuleDescriptor* descriptor,
                                  int pos) {
    return new (zone_) ModuleLiteral(zone_, body, descriptor, pos);
  }

  ModulePath* NewModulePath(Module* origin, const AstRawString* name, int pos) {
    return new (zone_) ModulePath(zone_, origin, name, pos);
  }

  ModuleUrl* NewModuleUrl(Handle<String> url, int pos) {
    return new (zone_) ModuleUrl(zone_, url, pos);
  }

  Block* NewBlock(ZoneList<const AstRawString*>* labels,
                  int capacity,
                  bool is_initializer_block,
                  int pos) {
    return new (zone_)
        Block(zone_, labels, capacity, is_initializer_block, pos);
  }

#define STATEMENT_WITH_LABELS(NodeType)                                     \
  NodeType* New##NodeType(ZoneList<const AstRawString*>* labels, int pos) { \
    return new (zone_) NodeType(zone_, labels, pos);                        \
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
        return new (zone_) ForInStatement(zone_, labels, pos);
      }
      case ForEachStatement::ITERATE: {
        return new (zone_) ForOfStatement(zone_, labels, pos);
      }
    }
    UNREACHABLE();
    return NULL;
  }

  ModuleStatement* NewModuleStatement(Block* body, int pos) {
    return new (zone_) ModuleStatement(zone_, body, pos);
  }

  ExpressionStatement* NewExpressionStatement(Expression* expression, int pos) {
    return new (zone_) ExpressionStatement(zone_, expression, pos);
  }

  ContinueStatement* NewContinueStatement(IterationStatement* target, int pos) {
    return new (zone_) ContinueStatement(zone_, target, pos);
  }

  BreakStatement* NewBreakStatement(BreakableStatement* target, int pos) {
    return new (zone_) BreakStatement(zone_, target, pos);
  }

  ReturnStatement* NewReturnStatement(Expression* expression, int pos) {
    return new (zone_) ReturnStatement(zone_, expression, pos);
  }

  WithStatement* NewWithStatement(Scope* scope,
                                  Expression* expression,
                                  Statement* statement,
                                  int pos) {
    return new (zone_) WithStatement(zone_, scope, expression, statement, pos);
  }

  IfStatement* NewIfStatement(Expression* condition,
                              Statement* then_statement,
                              Statement* else_statement,
                              int pos) {
    return new (zone_)
        IfStatement(zone_, condition, then_statement, else_statement, pos);
  }

  TryCatchStatement* NewTryCatchStatement(int index,
                                          Block* try_block,
                                          Scope* scope,
                                          Variable* variable,
                                          Block* catch_block,
                                          int pos) {
    return new (zone_) TryCatchStatement(zone_, index, try_block, scope,
                                         variable, catch_block, pos);
  }

  TryFinallyStatement* NewTryFinallyStatement(int index,
                                              Block* try_block,
                                              Block* finally_block,
                                              int pos) {
    return new (zone_)
        TryFinallyStatement(zone_, index, try_block, finally_block, pos);
  }

  DebuggerStatement* NewDebuggerStatement(int pos) {
    return new (zone_) DebuggerStatement(zone_, pos);
  }

  EmptyStatement* NewEmptyStatement(int pos) {
    return new(zone_) EmptyStatement(zone_, pos);
  }

  CaseClause* NewCaseClause(
      Expression* label, ZoneList<Statement*>* statements, int pos) {
    return new (zone_) CaseClause(zone_, label, statements, pos);
  }

  Literal* NewStringLiteral(const AstRawString* string, int pos) {
    return new (zone_)
        Literal(zone_, ast_value_factory_->NewString(string), pos);
  }

  // A JavaScript symbol (ECMA-262 edition 6).
  Literal* NewSymbolLiteral(const char* name, int pos) {
    return new (zone_) Literal(zone_, ast_value_factory_->NewSymbol(name), pos);
  }

  Literal* NewNumberLiteral(double number, int pos) {
    return new (zone_)
        Literal(zone_, ast_value_factory_->NewNumber(number), pos);
  }

  Literal* NewSmiLiteral(int number, int pos) {
    return new (zone_) Literal(zone_, ast_value_factory_->NewSmi(number), pos);
  }

  Literal* NewBooleanLiteral(bool b, int pos) {
    return new (zone_) Literal(zone_, ast_value_factory_->NewBoolean(b), pos);
  }

  Literal* NewNullLiteral(int pos) {
    return new (zone_) Literal(zone_, ast_value_factory_->NewNull(), pos);
  }

  Literal* NewUndefinedLiteral(int pos) {
    return new (zone_) Literal(zone_, ast_value_factory_->NewUndefined(), pos);
  }

  Literal* NewTheHoleLiteral(int pos) {
    return new (zone_) Literal(zone_, ast_value_factory_->NewTheHole(), pos);
  }

  ObjectLiteral* NewObjectLiteral(
      ZoneList<ObjectLiteral::Property*>* properties,
      int literal_index,
      int boilerplate_properties,
      bool has_function,
      int pos) {
    return new (zone_) ObjectLiteral(zone_, properties, literal_index,
                                     boilerplate_properties, has_function, pos);
  }

  ObjectLiteral::Property* NewObjectLiteralProperty(
      Expression* key, Expression* value, ObjectLiteralProperty::Kind kind,
      bool is_static, bool is_computed_name) {
    return new (zone_)
        ObjectLiteral::Property(key, value, kind, is_static, is_computed_name);
  }

  ObjectLiteral::Property* NewObjectLiteralProperty(Expression* key,
                                                    Expression* value,
                                                    bool is_static,
                                                    bool is_computed_name) {
    return new (zone_) ObjectLiteral::Property(ast_value_factory_, key, value,
                                               is_static, is_computed_name);
  }

  RegExpLiteral* NewRegExpLiteral(const AstRawString* pattern,
                                  const AstRawString* flags,
                                  int literal_index,
                                  int pos) {
    return new (zone_) RegExpLiteral(zone_, pattern, flags, literal_index, pos);
  }

  ArrayLiteral* NewArrayLiteral(ZoneList<Expression*>* values,
                                int literal_index,
                                int pos) {
    return new (zone_) ArrayLiteral(zone_, values, literal_index, pos);
  }

  VariableProxy* NewVariableProxy(Variable* var,
                                  int start_position = RelocInfo::kNoPosition,
                                  int end_position = RelocInfo::kNoPosition) {
    return new (zone_) VariableProxy(zone_, var, start_position, end_position);
  }

  VariableProxy* NewVariableProxy(const AstRawString* name,
                                  Variable::Kind variable_kind,
                                  int start_position = RelocInfo::kNoPosition,
                                  int end_position = RelocInfo::kNoPosition) {
    return new (zone_)
        VariableProxy(zone_, name, variable_kind, start_position, end_position);
  }

  Property* NewProperty(Expression* obj, Expression* key, int pos) {
    return new (zone_) Property(zone_, obj, key, pos);
  }

  Call* NewCall(Expression* expression,
                ZoneList<Expression*>* arguments,
                int pos) {
    return new (zone_) Call(zone_, expression, arguments, pos);
  }

  CallNew* NewCallNew(Expression* expression,
                      ZoneList<Expression*>* arguments,
                      int pos) {
    return new (zone_) CallNew(zone_, expression, arguments, pos);
  }

  CallRuntime* NewCallRuntime(const AstRawString* name,
                              const Runtime::Function* function,
                              ZoneList<Expression*>* arguments,
                              int pos) {
    return new (zone_) CallRuntime(zone_, name, function, arguments, pos);
  }

  UnaryOperation* NewUnaryOperation(Token::Value op,
                                    Expression* expression,
                                    int pos) {
    return new (zone_) UnaryOperation(zone_, op, expression, pos);
  }

  BinaryOperation* NewBinaryOperation(Token::Value op,
                                      Expression* left,
                                      Expression* right,
                                      int pos) {
    return new (zone_) BinaryOperation(zone_, op, left, right, pos);
  }

  CountOperation* NewCountOperation(Token::Value op,
                                    bool is_prefix,
                                    Expression* expr,
                                    int pos) {
    return new (zone_) CountOperation(zone_, op, is_prefix, expr, pos);
  }

  CompareOperation* NewCompareOperation(Token::Value op,
                                        Expression* left,
                                        Expression* right,
                                        int pos) {
    return new (zone_) CompareOperation(zone_, op, left, right, pos);
  }

  Conditional* NewConditional(Expression* condition,
                              Expression* then_expression,
                              Expression* else_expression,
                              int position) {
    return new (zone_) Conditional(zone_, condition, then_expression,
                                   else_expression, position);
  }

  Assignment* NewAssignment(Token::Value op,
                            Expression* target,
                            Expression* value,
                            int pos) {
    DCHECK(Token::IsAssignmentOp(op));
    Assignment* assign = new (zone_) Assignment(zone_, op, target, value, pos);
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
    return new (zone_)
        Yield(zone_, generator_object, expression, yield_kind, pos);
  }

  Throw* NewThrow(Expression* exception, int pos) {
    return new (zone_) Throw(zone_, exception, pos);
  }

  FunctionLiteral* NewFunctionLiteral(
      const AstRawString* name, AstValueFactory* ast_value_factory,
      Scope* scope, ZoneList<Statement*>* body, int materialized_literal_count,
      int expected_property_count, int handler_count, int parameter_count,
      FunctionLiteral::ParameterFlag has_duplicate_parameters,
      FunctionLiteral::FunctionType function_type,
      FunctionLiteral::IsFunctionFlag is_function,
      FunctionLiteral::IsParenthesizedFlag is_parenthesized, FunctionKind kind,
      int position) {
    return new (zone_) FunctionLiteral(
        zone_, name, ast_value_factory, scope, body, materialized_literal_count,
        expected_property_count, handler_count, parameter_count, function_type,
        has_duplicate_parameters, is_function, is_parenthesized, kind,
        position);
  }

  ClassLiteral* NewClassLiteral(const AstRawString* name, Scope* scope,
                                VariableProxy* proxy, Expression* extends,
                                FunctionLiteral* constructor,
                                ZoneList<ObjectLiteral::Property*>* properties,
                                int start_position, int end_position) {
    return new (zone_)
        ClassLiteral(zone_, name, scope, proxy, extends, constructor,
                     properties, start_position, end_position);
  }

  NativeFunctionLiteral* NewNativeFunctionLiteral(const AstRawString* name,
                                                  v8::Extension* extension,
                                                  int pos) {
    return new (zone_) NativeFunctionLiteral(zone_, name, extension, pos);
  }

  ThisFunction* NewThisFunction(int pos) {
    return new (zone_) ThisFunction(zone_, pos);
  }

  SuperReference* NewSuperReference(VariableProxy* this_var, int pos) {
    return new (zone_) SuperReference(zone_, this_var, pos);
  }

 private:
  Zone* zone_;
  AstValueFactory* ast_value_factory_;
};


} }  // namespace v8::internal

#endif  // V8_AST_H_
