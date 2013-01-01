// Copyright 2012 the V8 project authors. All rights reserved.
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

#include "v8.h"

#include "assembler.h"
#include "factory.h"
#include "isolate.h"
#include "jsregexp.h"
#include "list-inl.h"
#include "runtime.h"
#include "small-pointer-list.h"
#include "smart-pointers.h"
#include "token.h"
#include "utils.h"
#include "variables.h"
#include "interface.h"
#include "zone-inl.h"

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

#define DECLARATION_NODE_LIST(V)                \
  V(VariableDeclaration)                        \
  V(FunctionDeclaration)                        \
  V(ModuleDeclaration)                          \
  V(ImportDeclaration)                          \
  V(ExportDeclaration)                          \

#define MODULE_NODE_LIST(V)                     \
  V(ModuleLiteral)                              \
  V(ModuleVariable)                             \
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
  V(ThisFunction)

#define AST_NODE_LIST(V)                        \
  DECLARATION_NODE_LIST(V)                      \
  MODULE_NODE_LIST(V)                           \
  STATEMENT_NODE_LIST(V)                        \
  EXPRESSION_NODE_LIST(V)

// Forward declarations
class AstConstructionVisitor;
template<class> class AstNodeFactory;
class AstVisitor;
class Declaration;
class Module;
class BreakableStatement;
class Expression;
class IterationStatement;
class MaterializedLiteral;
class Statement;
class TargetCollector;
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


#define DECLARE_NODE_TYPE(type)                                         \
  virtual void Accept(AstVisitor* v);                                   \
  virtual AstNode::Type node_type() const { return AstNode::k##type; }  \
  template<class> friend class AstNodeFactory;


enum AstPropertiesFlag {
  kDontInline,
  kDontOptimize,
  kDontSelfOptimize,
  kDontSoftInline,
  kDontCache
};


class AstProperties BASE_EMBEDDED {
 public:
  class Flags : public EnumSet<AstPropertiesFlag, int> {};

  AstProperties() : node_count_(0) { }

  Flags* flags() { return &flags_; }
  int node_count() { return node_count_; }
  void add_node_count(int count) { node_count_ += count; }

 private:
  Flags flags_;
  int node_count_;
};


class AstNode: public ZoneObject {
 public:
#define DECLARE_TYPE_ENUM(type) k##type,
  enum Type {
    AST_NODE_LIST(DECLARE_TYPE_ENUM)
    kInvalid = -1
  };
#undef DECLARE_TYPE_ENUM

  void* operator new(size_t size, Zone* zone) {
    return zone->New(static_cast<int>(size));
  }

  AstNode() { }

  virtual ~AstNode() { }

  virtual void Accept(AstVisitor* v) = 0;
  virtual Type node_type() const = 0;

  // Type testing & conversion functions overridden by concrete subclasses.
#define DECLARE_NODE_FUNCTIONS(type)                  \
  bool Is##type() { return node_type() == AstNode::k##type; }          \
  type* As##type() { return Is##type() ? reinterpret_cast<type*>(this) : NULL; }
  AST_NODE_LIST(DECLARE_NODE_FUNCTIONS)
#undef DECLARE_NODE_FUNCTIONS

  virtual TargetCollector* AsTargetCollector() { return NULL; }
  virtual BreakableStatement* AsBreakableStatement() { return NULL; }
  virtual IterationStatement* AsIterationStatement() { return NULL; }
  virtual MaterializedLiteral* AsMaterializedLiteral() { return NULL; }

 protected:
  static int GetNextId(Isolate* isolate) {
    return ReserveIdRange(isolate, 1);
  }

  static int ReserveIdRange(Isolate* isolate, int n) {
    int tmp = isolate->ast_node_id();
    isolate->set_ast_node_id(tmp + n);
    return tmp;
  }

  // Some nodes re-use bailout IDs for type feedback.
  static TypeFeedbackId reuse(BailoutId id) {
    return TypeFeedbackId(id.ToInt());
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

  bool IsEmpty() { return AsEmptyStatement() != NULL; }

  void set_statement_pos(int statement_pos) { statement_pos_ = statement_pos; }
  int statement_pos() const { return statement_pos_; }

 private:
  int statement_pos_;
};


class SmallMapList {
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

  virtual int position() const {
    UNREACHABLE();
    return 0;
  }

  virtual bool IsValidLeftHandSide() { return false; }

  // Helpers for ToBoolean conversion.
  virtual bool ToBooleanIsTrue() { return false; }
  virtual bool ToBooleanIsFalse() { return false; }

  // Symbols that cannot be parsed as array indices are considered property
  // names.  We do not treat symbols that can be array indexes as property
  // names because [] for string objects is handled only by keyed ICs.
  virtual bool IsPropertyName() { return false; }

  // True iff the result can be safely overwritten (to avoid allocation).
  // False for operations that can return one of their operands.
  virtual bool ResultOverwriteAllowed() { return false; }

  // True iff the expression is a literal represented as a smi.
  bool IsSmiLiteral();

  // True iff the expression is a string literal.
  bool IsStringLiteral();

  // True iff the expression is the null literal.
  bool IsNullLiteral();

  // Type feedback information for assignments and properties.
  virtual bool IsMonomorphic() {
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

  BailoutId id() const { return id_; }
  TypeFeedbackId test_id() const { return test_id_; }

 protected:
  explicit Expression(Isolate* isolate)
      : id_(GetNextId(isolate)),
        test_id_(GetNextId(isolate)) {}

 private:
  const BailoutId id_;
  const TypeFeedbackId test_id_;
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

  BailoutId EntryId() const { return entry_id_; }
  BailoutId ExitId() const { return exit_id_; }

 protected:
  BreakableStatement(Isolate* isolate, ZoneStringList* labels, Type type)
      : labels_(labels),
        type_(type),
        entry_id_(GetNextId(isolate)),
        exit_id_(GetNextId(isolate)) {
    ASSERT(labels == NULL || labels->length() > 0);
  }


 private:
  ZoneStringList* labels_;
  Type type_;
  Label break_target_;
  const BailoutId entry_id_;
  const BailoutId exit_id_;
};


class Block: public BreakableStatement {
 public:
  DECLARE_NODE_TYPE(Block)

  void AddStatement(Statement* statement, Zone* zone) {
    statements_.Add(statement, zone);
  }

  ZoneList<Statement*>* statements() { return &statements_; }
  bool is_initializer_block() const { return is_initializer_block_; }

  Scope* scope() const { return scope_; }
  void set_scope(Scope* scope) { scope_ = scope; }

 protected:
  Block(Isolate* isolate,
        ZoneStringList* labels,
        int capacity,
        bool is_initializer_block,
        Zone* zone)
      : BreakableStatement(isolate, labels, TARGET_FOR_NAMED_ONLY),
        statements_(capacity, zone),
        is_initializer_block_(is_initializer_block),
        scope_(NULL) {
  }

 private:
  ZoneList<Statement*> statements_;
  bool is_initializer_block_;
  Scope* scope_;
};


class Declaration: public AstNode {
 public:
  VariableProxy* proxy() const { return proxy_; }
  VariableMode mode() const { return mode_; }
  Scope* scope() const { return scope_; }
  virtual InitializationFlag initialization() const = 0;
  virtual bool IsInlineable() const;

 protected:
  Declaration(VariableProxy* proxy,
              VariableMode mode,
              Scope* scope)
      : proxy_(proxy),
        mode_(mode),
        scope_(scope) {
    ASSERT(IsDeclaredVariableMode(mode));
  }

 private:
  VariableProxy* proxy_;
  VariableMode mode_;

  // Nested scope from which the declaration originated.
  Scope* scope_;
};


class VariableDeclaration: public Declaration {
 public:
  DECLARE_NODE_TYPE(VariableDeclaration)

  virtual InitializationFlag initialization() const {
    return mode() == VAR ? kCreatedInitialized : kNeedsInitialization;
  }

 protected:
  VariableDeclaration(VariableProxy* proxy,
                      VariableMode mode,
                      Scope* scope)
      : Declaration(proxy, mode, scope) {
  }
};


class FunctionDeclaration: public Declaration {
 public:
  DECLARE_NODE_TYPE(FunctionDeclaration)

  FunctionLiteral* fun() const { return fun_; }
  virtual InitializationFlag initialization() const {
    return kCreatedInitialized;
  }
  virtual bool IsInlineable() const;

 protected:
  FunctionDeclaration(VariableProxy* proxy,
                      VariableMode mode,
                      FunctionLiteral* fun,
                      Scope* scope)
      : Declaration(proxy, mode, scope),
        fun_(fun) {
    // At the moment there are no "const functions" in JavaScript...
    ASSERT(mode == VAR || mode == LET);
    ASSERT(fun != NULL);
  }

 private:
  FunctionLiteral* fun_;
};


class ModuleDeclaration: public Declaration {
 public:
  DECLARE_NODE_TYPE(ModuleDeclaration)

  Module* module() const { return module_; }
  virtual InitializationFlag initialization() const {
    return kCreatedInitialized;
  }

 protected:
  ModuleDeclaration(VariableProxy* proxy,
                    Module* module,
                    Scope* scope)
      : Declaration(proxy, MODULE, scope),
        module_(module) {
  }

 private:
  Module* module_;
};


class ImportDeclaration: public Declaration {
 public:
  DECLARE_NODE_TYPE(ImportDeclaration)

  Module* module() const { return module_; }
  virtual InitializationFlag initialization() const {
    return kCreatedInitialized;
  }

 protected:
  ImportDeclaration(VariableProxy* proxy,
                    Module* module,
                    Scope* scope)
      : Declaration(proxy, LET, scope),
        module_(module) {
  }

 private:
  Module* module_;
};


class ExportDeclaration: public Declaration {
 public:
  DECLARE_NODE_TYPE(ExportDeclaration)

  virtual InitializationFlag initialization() const {
    return kCreatedInitialized;
  }

 protected:
  ExportDeclaration(VariableProxy* proxy, Scope* scope)
      : Declaration(proxy, LET, scope) {}
};


class Module: public AstNode {
 public:
  Interface* interface() const { return interface_; }
  Block* body() const { return body_; }

 protected:
  explicit Module(Zone* zone)
      : interface_(Interface::NewModule(zone)),
        body_(NULL) {}
  explicit Module(Interface* interface, Block* body = NULL)
      : interface_(interface),
        body_(body) {}

 private:
  Interface* interface_;
  Block* body_;
};


class ModuleLiteral: public Module {
 public:
  DECLARE_NODE_TYPE(ModuleLiteral)

 protected:
  ModuleLiteral(Block* body, Interface* interface) : Module(interface, body) {}
};


class ModuleVariable: public Module {
 public:
  DECLARE_NODE_TYPE(ModuleVariable)

  VariableProxy* proxy() const { return proxy_; }

 protected:
  inline explicit ModuleVariable(VariableProxy* proxy);

 private:
  VariableProxy* proxy_;
};


class ModulePath: public Module {
 public:
  DECLARE_NODE_TYPE(ModulePath)

  Module* module() const { return module_; }
  Handle<String> name() const { return name_; }

 protected:
  ModulePath(Module* module, Handle<String> name, Zone* zone)
      : Module(zone),
        module_(module),
        name_(name) {
  }

 private:
  Module* module_;
  Handle<String> name_;
};


class ModuleUrl: public Module {
 public:
  DECLARE_NODE_TYPE(ModuleUrl)

  Handle<String> url() const { return url_; }

 protected:
  ModuleUrl(Handle<String> url, Zone* zone)
      : Module(zone), url_(url) {
  }

 private:
  Handle<String> url_;
};


class ModuleStatement: public Statement {
 public:
  DECLARE_NODE_TYPE(ModuleStatement)

  VariableProxy* proxy() const { return proxy_; }
  Block* body() const { return body_; }

 protected:
  ModuleStatement(VariableProxy* proxy, Block* body)
      : proxy_(proxy),
        body_(body) {
  }

 private:
  VariableProxy* proxy_;
  Block* body_;
};


class IterationStatement: public BreakableStatement {
 public:
  // Type testing & conversion.
  virtual IterationStatement* AsIterationStatement() { return this; }

  Statement* body() const { return body_; }

  BailoutId OsrEntryId() const { return osr_entry_id_; }
  virtual BailoutId ContinueId() const = 0;
  virtual BailoutId StackCheckId() const = 0;

  // Code generation
  Label* continue_target()  { return &continue_target_; }

 protected:
  IterationStatement(Isolate* isolate, ZoneStringList* labels)
      : BreakableStatement(isolate, labels, TARGET_FOR_ANONYMOUS),
        body_(NULL),
        osr_entry_id_(GetNextId(isolate)) {
  }

  void Initialize(Statement* body) {
    body_ = body;
  }

 private:
  Statement* body_;
  Label continue_target_;
  const BailoutId osr_entry_id_;
};


class DoWhileStatement: public IterationStatement {
 public:
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

  virtual BailoutId ContinueId() const { return continue_id_; }
  virtual BailoutId StackCheckId() const { return back_edge_id_; }
  BailoutId BackEdgeId() const { return back_edge_id_; }

 protected:
  DoWhileStatement(Isolate* isolate, ZoneStringList* labels)
      : IterationStatement(isolate, labels),
        cond_(NULL),
        condition_position_(-1),
        continue_id_(GetNextId(isolate)),
        back_edge_id_(GetNextId(isolate)) {
  }

 private:
  Expression* cond_;
  int condition_position_;
  const BailoutId continue_id_;
  const BailoutId back_edge_id_;
};


class WhileStatement: public IterationStatement {
 public:
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

  virtual BailoutId ContinueId() const { return EntryId(); }
  virtual BailoutId StackCheckId() const { return body_id_; }
  BailoutId BodyId() const { return body_id_; }

 protected:
  WhileStatement(Isolate* isolate, ZoneStringList* labels)
      : IterationStatement(isolate, labels),
        cond_(NULL),
        may_have_function_literal_(true),
        body_id_(GetNextId(isolate)) {
  }

 private:
  Expression* cond_;
  // True if there is a function literal subexpression in the condition.
  bool may_have_function_literal_;
  const BailoutId body_id_;
};


class ForStatement: public IterationStatement {
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

  bool may_have_function_literal() const {
    return may_have_function_literal_;
  }
  void set_may_have_function_literal(bool value) {
    may_have_function_literal_ = value;
  }

  virtual BailoutId ContinueId() const { return continue_id_; }
  virtual BailoutId StackCheckId() const { return body_id_; }
  BailoutId BodyId() const { return body_id_; }

  bool is_fast_smi_loop() { return loop_variable_ != NULL; }
  Variable* loop_variable() { return loop_variable_; }
  void set_loop_variable(Variable* var) { loop_variable_ = var; }

 protected:
  ForStatement(Isolate* isolate, ZoneStringList* labels)
      : IterationStatement(isolate, labels),
        init_(NULL),
        cond_(NULL),
        next_(NULL),
        may_have_function_literal_(true),
        loop_variable_(NULL),
        continue_id_(GetNextId(isolate)),
        body_id_(GetNextId(isolate)) {
  }

 private:
  Statement* init_;
  Expression* cond_;
  Statement* next_;
  // True if there is a function literal subexpression in the condition.
  bool may_have_function_literal_;
  Variable* loop_variable_;
  const BailoutId continue_id_;
  const BailoutId body_id_;
};


class ForInStatement: public IterationStatement {
 public:
  DECLARE_NODE_TYPE(ForInStatement)

  void Initialize(Expression* each, Expression* enumerable, Statement* body) {
    IterationStatement::Initialize(body);
    each_ = each;
    enumerable_ = enumerable;
  }

  Expression* each() const { return each_; }
  Expression* enumerable() const { return enumerable_; }

  virtual BailoutId ContinueId() const { return EntryId(); }
  virtual BailoutId StackCheckId() const { return body_id_; }
  BailoutId BodyId() const { return body_id_; }
  BailoutId PrepareId() const { return prepare_id_; }

  TypeFeedbackId ForInFeedbackId() const { return reuse(PrepareId()); }

 protected:
  ForInStatement(Isolate* isolate, ZoneStringList* labels)
      : IterationStatement(isolate, labels),
        each_(NULL),
        enumerable_(NULL),
        body_id_(GetNextId(isolate)),
        prepare_id_(GetNextId(isolate)) {
  }

 private:
  Expression* each_;
  Expression* enumerable_;
  const BailoutId body_id_;
  const BailoutId prepare_id_;
};


class ExpressionStatement: public Statement {
 public:
  DECLARE_NODE_TYPE(ExpressionStatement)

  void set_expression(Expression* e) { expression_ = e; }
  Expression* expression() const { return expression_; }

 protected:
  explicit ExpressionStatement(Expression* expression)
      : expression_(expression) { }

 private:
  Expression* expression_;
};


class ContinueStatement: public Statement {
 public:
  DECLARE_NODE_TYPE(ContinueStatement)

  IterationStatement* target() const { return target_; }

 protected:
  explicit ContinueStatement(IterationStatement* target)
      : target_(target) { }

 private:
  IterationStatement* target_;
};


class BreakStatement: public Statement {
 public:
  DECLARE_NODE_TYPE(BreakStatement)

  BreakableStatement* target() const { return target_; }

 protected:
  explicit BreakStatement(BreakableStatement* target)
      : target_(target) { }

 private:
  BreakableStatement* target_;
};


class ReturnStatement: public Statement {
 public:
  DECLARE_NODE_TYPE(ReturnStatement)

  Expression* expression() const { return expression_; }

 protected:
  explicit ReturnStatement(Expression* expression)
      : expression_(expression) { }

 private:
  Expression* expression_;
};


class WithStatement: public Statement {
 public:
  DECLARE_NODE_TYPE(WithStatement)

  Expression* expression() const { return expression_; }
  Statement* statement() const { return statement_; }

 protected:
  WithStatement(Expression* expression, Statement* statement)
      : expression_(expression),
        statement_(statement) { }

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

  BailoutId EntryId() const { return entry_id_; }

  // Type feedback information.
  TypeFeedbackId CompareId() { return compare_id_; }
  void RecordTypeFeedback(TypeFeedbackOracle* oracle);
  bool IsSmiCompare() { return compare_type_ == SMI_ONLY; }
  bool IsSymbolCompare() { return compare_type_ == SYMBOL_ONLY; }
  bool IsStringCompare() { return compare_type_ == STRING_ONLY; }
  bool IsObjectCompare() { return compare_type_ == OBJECT_ONLY; }

 private:
  Expression* label_;
  Label body_target_;
  ZoneList<Statement*>* statements_;
  int position_;
  enum CompareTypeFeedback {
    NONE,
    SMI_ONLY,
    SYMBOL_ONLY,
    STRING_ONLY,
    OBJECT_ONLY
  };
  CompareTypeFeedback compare_type_;
  const TypeFeedbackId compare_id_;
  const BailoutId entry_id_;
};


class SwitchStatement: public BreakableStatement {
 public:
  DECLARE_NODE_TYPE(SwitchStatement)

  void Initialize(Expression* tag, ZoneList<CaseClause*>* cases) {
    tag_ = tag;
    cases_ = cases;
  }

  Expression* tag() const { return tag_; }
  ZoneList<CaseClause*>* cases() const { return cases_; }

 protected:
  SwitchStatement(Isolate* isolate, ZoneStringList* labels)
      : BreakableStatement(isolate, labels, TARGET_FOR_ANONYMOUS),
        tag_(NULL),
        cases_(NULL) { }

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
  DECLARE_NODE_TYPE(IfStatement)

  bool HasThenStatement() const { return !then_statement()->IsEmpty(); }
  bool HasElseStatement() const { return !else_statement()->IsEmpty(); }

  Expression* condition() const { return condition_; }
  Statement* then_statement() const { return then_statement_; }
  Statement* else_statement() const { return else_statement_; }

  BailoutId IfId() const { return if_id_; }
  BailoutId ThenId() const { return then_id_; }
  BailoutId ElseId() const { return else_id_; }

 protected:
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

 private:
  Expression* condition_;
  Statement* then_statement_;
  Statement* else_statement_;
  const BailoutId if_id_;
  const BailoutId then_id_;
  const BailoutId else_id_;
};


// NOTE: TargetCollectors are represented as nodes to fit in the target
// stack in the compiler; this should probably be reworked.
class TargetCollector: public AstNode {
 public:
  explicit TargetCollector(Zone* zone) : targets_(0, zone) { }

  // Adds a jump target to the collector. The collector stores a pointer not
  // a copy of the target to make binding work, so make sure not to pass in
  // references to something on the stack.
  void AddTarget(Label* target, Zone* zone);

  // Virtual behaviour. TargetCollectors are never part of the AST.
  virtual void Accept(AstVisitor* v) { UNREACHABLE(); }
  virtual Type node_type() const { return kInvalid; }
  virtual TargetCollector* AsTargetCollector() { return this; }

  ZoneList<Label*>* targets() { return &targets_; }

 private:
  ZoneList<Label*> targets_;
};


class TryStatement: public Statement {
 public:
  void set_escaping_targets(ZoneList<Label*>* targets) {
    escaping_targets_ = targets;
  }

  int index() const { return index_; }
  Block* try_block() const { return try_block_; }
  ZoneList<Label*>* escaping_targets() const { return escaping_targets_; }

 protected:
  TryStatement(int index, Block* try_block)
      : index_(index),
        try_block_(try_block),
        escaping_targets_(NULL) { }

 private:
  // Unique (per-function) index of this handler.  This is not an AST ID.
  int index_;

  Block* try_block_;
  ZoneList<Label*>* escaping_targets_;
};


class TryCatchStatement: public TryStatement {
 public:
  DECLARE_NODE_TYPE(TryCatchStatement)

  Scope* scope() { return scope_; }
  Variable* variable() { return variable_; }
  Block* catch_block() const { return catch_block_; }

 protected:
  TryCatchStatement(int index,
                    Block* try_block,
                    Scope* scope,
                    Variable* variable,
                    Block* catch_block)
      : TryStatement(index, try_block),
        scope_(scope),
        variable_(variable),
        catch_block_(catch_block) {
  }

 private:
  Scope* scope_;
  Variable* variable_;
  Block* catch_block_;
};


class TryFinallyStatement: public TryStatement {
 public:
  DECLARE_NODE_TYPE(TryFinallyStatement)

  Block* finally_block() const { return finally_block_; }

 protected:
  TryFinallyStatement(int index, Block* try_block, Block* finally_block)
      : TryStatement(index, try_block),
        finally_block_(finally_block) { }

 private:
  Block* finally_block_;
};


class DebuggerStatement: public Statement {
 public:
  DECLARE_NODE_TYPE(DebuggerStatement)

 protected:
  DebuggerStatement() {}
};


class EmptyStatement: public Statement {
 public:
  DECLARE_NODE_TYPE(EmptyStatement)

 protected:
  EmptyStatement() {}
};


class Literal: public Expression {
 public:
  DECLARE_NODE_TYPE(Literal)

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

  // Support for using Literal as a HashMap key. NOTE: Currently, this works
  // only for string and number literals!
  uint32_t Hash() { return ToString()->Hash(); }

  static bool Match(void* literal1, void* literal2) {
    Handle<String> s1 = static_cast<Literal*>(literal1)->ToString();
    Handle<String> s2 = static_cast<Literal*>(literal2)->ToString();
    return s1->Equals(*s2);
  }

  TypeFeedbackId LiteralFeedbackId() const { return reuse(id()); }

 protected:
  Literal(Isolate* isolate, Handle<Object> handle)
      : Expression(isolate),
        handle_(handle) { }

 private:
  Handle<String> ToString();

  Handle<Object> handle_;
};


// Base class for literals that needs space in the corresponding JSFunction.
class MaterializedLiteral: public Expression {
 public:
  virtual MaterializedLiteral* AsMaterializedLiteral() { return this; }

  int literal_index() { return literal_index_; }

  // A materialized literal is simple if the values consist of only
  // constants and simple object and array literals.
  bool is_simple() const { return is_simple_; }

  int depth() const { return depth_; }

 protected:
  MaterializedLiteral(Isolate* isolate,
                      int literal_index,
                      bool is_simple,
                      int depth)
      : Expression(isolate),
        literal_index_(literal_index),
        is_simple_(is_simple),
        depth_(depth) {}

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

    Property(Literal* key, Expression* value, Isolate* isolate);

    Literal* key() { return key_; }
    Expression* value() { return value_; }
    Kind kind() { return kind_; }

    // Type feedback information.
    void RecordTypeFeedback(TypeFeedbackOracle* oracle);
    bool IsMonomorphic() { return !receiver_type_.is_null(); }
    Handle<Map> GetReceiverType() { return receiver_type_; }

    bool IsCompileTimeValue();

    void set_emit_store(bool emit_store);
    bool emit_store();

   protected:
    template<class> friend class AstNodeFactory;

    Property(bool is_getter, FunctionLiteral* value);
    void set_key(Literal* key) { key_ = key; }

   private:
    Literal* key_;
    Expression* value_;
    Kind kind_;
    bool emit_store_;
    Handle<Map> receiver_type_;
  };

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
  void CalculateEmitStore(Zone* zone);

  enum Flags {
    kNoFlags = 0,
    kFastElements = 1,
    kHasFunction = 1 << 1
  };

  struct Accessors: public ZoneObject {
    Accessors() : getter(NULL), setter(NULL) { }
    Expression* getter;
    Expression* setter;
  };

 protected:
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

 private:
  Handle<FixedArray> constant_properties_;
  ZoneList<Property*>* properties_;
  bool fast_elements_;
  bool has_function_;
};


// Node for capturing a regexp literal.
class RegExpLiteral: public MaterializedLiteral {
 public:
  DECLARE_NODE_TYPE(RegExpLiteral)

  Handle<String> pattern() const { return pattern_; }
  Handle<String> flags() const { return flags_; }

 protected:
  RegExpLiteral(Isolate* isolate,
                Handle<String> pattern,
                Handle<String> flags,
                int literal_index)
      : MaterializedLiteral(isolate, literal_index, false, 1),
        pattern_(pattern),
        flags_(flags) {}

 private:
  Handle<String> pattern_;
  Handle<String> flags_;
};

// An array literal has a literals object that is used
// for minimizing the work when constructing it at runtime.
class ArrayLiteral: public MaterializedLiteral {
 public:
  DECLARE_NODE_TYPE(ArrayLiteral)

  Handle<FixedArray> constant_elements() const { return constant_elements_; }
  ZoneList<Expression*>* values() const { return values_; }

  // Return an AST id for an element that is used in simulate instructions.
  BailoutId GetIdForElement(int i) {
    return BailoutId(first_element_id_.ToInt() + i);
  }

 protected:
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

 private:
  Handle<FixedArray> constant_elements_;
  ZoneList<Expression*>* values_;
  const BailoutId first_element_id_;
};


class VariableProxy: public Expression {
 public:
  DECLARE_NODE_TYPE(VariableProxy)

  virtual bool IsValidLeftHandSide() {
    return var_ == NULL ? true : var_->IsValidLeftHandSide();
  }

  bool IsVariable(Handle<String> n) {
    return !is_this() && name().is_identical_to(n);
  }

  bool IsArguments() { return var_ != NULL && var_->is_arguments(); }

  bool IsLValue() {
    return is_lvalue_;
  }

  Handle<String> name() const { return name_; }
  Variable* var() const { return var_; }
  bool is_this() const { return is_this_; }
  int position() const { return position_; }
  Interface* interface() const { return interface_; }


  void MarkAsTrivial() { is_trivial_ = true; }
  void MarkAsLValue() { is_lvalue_ = true; }

  // Bind this proxy to the variable var. Interfaces must match.
  void BindTo(Variable* var);

 protected:
  VariableProxy(Isolate* isolate, Variable* var);

  VariableProxy(Isolate* isolate,
                Handle<String> name,
                bool is_this,
                Interface* interface,
                int position);

  Handle<String> name_;
  Variable* var_;  // resolved variable, or NULL
  bool is_this_;
  bool is_trivial_;
  // True if this variable proxy is being used in an assignment
  // or with a increment/decrement operator.
  bool is_lvalue_;
  int position_;
  Interface* interface_;
};


class Property: public Expression {
 public:
  DECLARE_NODE_TYPE(Property)

  virtual bool IsValidLeftHandSide() { return true; }

  Expression* obj() const { return obj_; }
  Expression* key() const { return key_; }
  virtual int position() const { return pos_; }

  BailoutId LoadId() const { return load_id_; }

  bool IsStringLength() const { return is_string_length_; }
  bool IsStringAccess() const { return is_string_access_; }
  bool IsFunctionPrototype() const { return is_function_prototype_; }

  // Type feedback information.
  void RecordTypeFeedback(TypeFeedbackOracle* oracle, Zone* zone);
  virtual bool IsMonomorphic() { return is_monomorphic_; }
  virtual SmallMapList* GetReceiverTypes() { return &receiver_types_; }
  bool IsArrayLength() { return is_array_length_; }
  bool IsUninitialized() { return is_uninitialized_; }
  TypeFeedbackId PropertyFeedbackId() { return reuse(id()); }

 protected:
  Property(Isolate* isolate,
           Expression* obj,
           Expression* key,
           int pos)
      : Expression(isolate),
        obj_(obj),
        key_(key),
        pos_(pos),
        load_id_(GetNextId(isolate)),
        is_monomorphic_(false),
        is_uninitialized_(false),
        is_array_length_(false),
        is_string_length_(false),
        is_string_access_(false),
        is_function_prototype_(false) { }

 private:
  Expression* obj_;
  Expression* key_;
  int pos_;
  const BailoutId load_id_;

  SmallMapList receiver_types_;
  bool is_monomorphic_ : 1;
  bool is_uninitialized_ : 1;
  bool is_array_length_ : 1;
  bool is_string_length_ : 1;
  bool is_string_access_ : 1;
  bool is_function_prototype_ : 1;
};


class Call: public Expression {
 public:
  DECLARE_NODE_TYPE(Call)

  Expression* expression() const { return expression_; }
  ZoneList<Expression*>* arguments() const { return arguments_; }
  virtual int position() const { return pos_; }

  // Type feedback information.
  TypeFeedbackId CallFeedbackId() const { return reuse(id()); }
  void RecordTypeFeedback(TypeFeedbackOracle* oracle, CallKind call_kind);
  virtual SmallMapList* GetReceiverTypes() { return &receiver_types_; }
  virtual bool IsMonomorphic() { return is_monomorphic_; }
  CheckType check_type() const { return check_type_; }
  Handle<JSFunction> target() { return target_; }

  // A cache for the holder, set as a side effect of computing the target of the
  // call. Note that it contains the null handle when the receiver is the same
  // as the holder!
  Handle<JSObject> holder() { return holder_; }

  Handle<JSGlobalPropertyCell> cell() { return cell_; }

  bool ComputeTarget(Handle<Map> type, Handle<String> name);
  bool ComputeGlobalTarget(Handle<GlobalObject> global, LookupResult* lookup);

  BailoutId ReturnId() const { return return_id_; }

#ifdef DEBUG
  // Used to assert that the FullCodeGenerator records the return site.
  bool return_is_recorded_;
#endif

 protected:
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
        return_id_(GetNextId(isolate)) { }

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

  const BailoutId return_id_;
};


class CallNew: public Expression {
 public:
  DECLARE_NODE_TYPE(CallNew)

  Expression* expression() const { return expression_; }
  ZoneList<Expression*>* arguments() const { return arguments_; }
  virtual int position() const { return pos_; }

  // Type feedback information.
  TypeFeedbackId CallNewFeedbackId() const { return reuse(id()); }
  void RecordTypeFeedback(TypeFeedbackOracle* oracle);
  virtual bool IsMonomorphic() { return is_monomorphic_; }
  Handle<JSFunction> target() { return target_; }

  BailoutId ReturnId() const { return return_id_; }

 protected:
  CallNew(Isolate* isolate,
          Expression* expression,
          ZoneList<Expression*>* arguments,
          int pos)
      : Expression(isolate),
        expression_(expression),
        arguments_(arguments),
        pos_(pos),
        is_monomorphic_(false),
        return_id_(GetNextId(isolate)) { }

 private:
  Expression* expression_;
  ZoneList<Expression*>* arguments_;
  int pos_;

  bool is_monomorphic_;
  Handle<JSFunction> target_;

  const BailoutId return_id_;
};


// The CallRuntime class does not represent any official JavaScript
// language construct. Instead it is used to call a C or JS function
// with a set of arguments. This is used from the builtins that are
// implemented in JavaScript (see "v8natives.js").
class CallRuntime: public Expression {
 public:
  DECLARE_NODE_TYPE(CallRuntime)

  Handle<String> name() const { return name_; }
  const Runtime::Function* function() const { return function_; }
  ZoneList<Expression*>* arguments() const { return arguments_; }
  bool is_jsruntime() const { return function_ == NULL; }

  TypeFeedbackId CallRuntimeFeedbackId() const { return reuse(id()); }

 protected:
  CallRuntime(Isolate* isolate,
              Handle<String> name,
              const Runtime::Function* function,
              ZoneList<Expression*>* arguments)
      : Expression(isolate),
        name_(name),
        function_(function),
        arguments_(arguments) { }

 private:
  Handle<String> name_;
  const Runtime::Function* function_;
  ZoneList<Expression*>* arguments_;
};


class UnaryOperation: public Expression {
 public:
  DECLARE_NODE_TYPE(UnaryOperation)

  virtual bool ResultOverwriteAllowed();

  Token::Value op() const { return op_; }
  Expression* expression() const { return expression_; }
  virtual int position() const { return pos_; }

  BailoutId MaterializeTrueId() { return materialize_true_id_; }
  BailoutId MaterializeFalseId() { return materialize_false_id_; }

  TypeFeedbackId UnaryOperationFeedbackId() const { return reuse(id()); }

 protected:
  UnaryOperation(Isolate* isolate,
                 Token::Value op,
                 Expression* expression,
                 int pos)
      : Expression(isolate),
        op_(op),
        expression_(expression),
        pos_(pos),
        materialize_true_id_(GetNextId(isolate)),
        materialize_false_id_(GetNextId(isolate)) {
    ASSERT(Token::IsUnaryOp(op));
  }

 private:
  Token::Value op_;
  Expression* expression_;
  int pos_;

  // For unary not (Token::NOT), the AST ids where true and false will
  // actually be materialized, respectively.
  const BailoutId materialize_true_id_;
  const BailoutId materialize_false_id_;
};


class BinaryOperation: public Expression {
 public:
  DECLARE_NODE_TYPE(BinaryOperation)

  virtual bool ResultOverwriteAllowed();

  Token::Value op() const { return op_; }
  Expression* left() const { return left_; }
  Expression* right() const { return right_; }
  virtual int position() const { return pos_; }

  BailoutId RightId() const { return right_id_; }

  TypeFeedbackId BinaryOperationFeedbackId() const { return reuse(id()); }

 protected:
  BinaryOperation(Isolate* isolate,
                  Token::Value op,
                  Expression* left,
                  Expression* right,
                  int pos)
      : Expression(isolate),
        op_(op),
        left_(left),
        right_(right),
        pos_(pos),
        right_id_(GetNextId(isolate)) {
    ASSERT(Token::IsBinaryOp(op));
  }

 private:
  Token::Value op_;
  Expression* left_;
  Expression* right_;
  int pos_;
  // The short-circuit logical operations need an AST ID for their
  // right-hand subexpression.
  const BailoutId right_id_;
};


class CountOperation: public Expression {
 public:
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

  void RecordTypeFeedback(TypeFeedbackOracle* oracle, Zone* znoe);
  virtual bool IsMonomorphic() { return is_monomorphic_; }
  virtual SmallMapList* GetReceiverTypes() { return &receiver_types_; }

  BailoutId AssignmentId() const { return assignment_id_; }

  TypeFeedbackId CountBinOpFeedbackId() const { return count_id_; }
  TypeFeedbackId CountStoreFeedbackId() const { return reuse(id()); }

 protected:
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

 private:
  Token::Value op_;
  bool is_prefix_;
  bool is_monomorphic_;
  Expression* expression_;
  int pos_;
  const BailoutId assignment_id_;
  const TypeFeedbackId count_id_;
  SmallMapList receiver_types_;
};


class CompareOperation: public Expression {
 public:
  DECLARE_NODE_TYPE(CompareOperation)

  Token::Value op() const { return op_; }
  Expression* left() const { return left_; }
  Expression* right() const { return right_; }
  virtual int position() const { return pos_; }

  // Type feedback information.
  TypeFeedbackId CompareOperationFeedbackId() const { return reuse(id()); }

  // Match special cases.
  bool IsLiteralCompareTypeof(Expression** expr, Handle<String>* check);
  bool IsLiteralCompareUndefined(Expression** expr);
  bool IsLiteralCompareNull(Expression** expr);

 protected:
  CompareOperation(Isolate* isolate,
                   Token::Value op,
                   Expression* left,
                   Expression* right,
                   int pos)
      : Expression(isolate),
        op_(op),
        left_(left),
        right_(right),
        pos_(pos) {
    ASSERT(Token::IsCompareOp(op));
  }

 private:
  Token::Value op_;
  Expression* left_;
  Expression* right_;
  int pos_;
};


class Conditional: public Expression {
 public:
  DECLARE_NODE_TYPE(Conditional)

  Expression* condition() const { return condition_; }
  Expression* then_expression() const { return then_expression_; }
  Expression* else_expression() const { return else_expression_; }

  int then_expression_position() const { return then_expression_position_; }
  int else_expression_position() const { return else_expression_position_; }

  BailoutId ThenId() const { return then_id_; }
  BailoutId ElseId() const { return else_id_; }

 protected:
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
        else_id_(GetNextId(isolate)) { }

 private:
  Expression* condition_;
  Expression* then_expression_;
  Expression* else_expression_;
  int then_expression_position_;
  int else_expression_position_;
  const BailoutId then_id_;
  const BailoutId else_id_;
};


class Assignment: public Expression {
 public:
  DECLARE_NODE_TYPE(Assignment)

  Assignment* AsSimpleAssignment() { return !is_compound() ? this : NULL; }

  Token::Value binary_op() const;

  Token::Value op() const { return op_; }
  Expression* target() const { return target_; }
  Expression* value() const { return value_; }
  virtual int position() const { return pos_; }
  BinaryOperation* binary_operation() const { return binary_operation_; }

  // This check relies on the definition order of token in token.h.
  bool is_compound() const { return op() > Token::ASSIGN; }

  BailoutId AssignmentId() const { return assignment_id_; }

  // Type feedback information.
  TypeFeedbackId AssignmentFeedbackId() { return reuse(id()); }
  void RecordTypeFeedback(TypeFeedbackOracle* oracle, Zone* zone);
  virtual bool IsMonomorphic() { return is_monomorphic_; }
  virtual SmallMapList* GetReceiverTypes() { return &receiver_types_; }

 protected:
  Assignment(Isolate* isolate,
             Token::Value op,
             Expression* target,
             Expression* value,
             int pos);

  template<class Visitor>
  void Init(Isolate* isolate, AstNodeFactory<Visitor>* factory) {
    ASSERT(Token::IsAssignmentOp(op_));
    if (is_compound()) {
      binary_operation_ =
          factory->NewBinaryOperation(binary_op(), target_, value_, pos_ + 1);
    }
  }

 private:
  Token::Value op_;
  Expression* target_;
  Expression* value_;
  int pos_;
  BinaryOperation* binary_operation_;
  const BailoutId assignment_id_;

  bool is_monomorphic_;
  SmallMapList receiver_types_;
};


class Throw: public Expression {
 public:
  DECLARE_NODE_TYPE(Throw)

  Expression* exception() const { return exception_; }
  virtual int position() const { return pos_; }

 protected:
  Throw(Isolate* isolate, Expression* exception, int pos)
      : Expression(isolate), exception_(exception), pos_(pos) {}

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

  DECLARE_NODE_TYPE(FunctionLiteral)

  Handle<String> name() const { return name_; }
  Scope* scope() const { return scope_; }
  ZoneList<Statement*>* body() const { return body_; }
  void set_function_token_position(int pos) { function_token_position_ = pos; }
  int function_token_position() const { return function_token_position_; }
  int start_position() const;
  int end_position() const;
  int SourceSize() const { return end_position() - start_position(); }
  bool is_expression() const { return IsExpression::decode(bitfield_); }
  bool is_anonymous() const { return IsAnonymous::decode(bitfield_); }
  bool is_classic_mode() const { return language_mode() == CLASSIC_MODE; }
  LanguageMode language_mode() const;

  int materialized_literal_count() { return materialized_literal_count_; }
  int expected_property_count() { return expected_property_count_; }
  int handler_count() { return handler_count_; }
  bool has_only_simple_this_property_assignments() {
    return HasOnlySimpleThisPropertyAssignments::decode(bitfield_);
  }
  Handle<FixedArray> this_property_assignments() {
      return this_property_assignments_;
  }
  int parameter_count() { return parameter_count_; }

  bool AllowsLazyCompilation();
  bool AllowsLazyCompilationWithoutContext();

  Handle<String> debug_name() const {
    if (name_->length() > 0) return name_;
    return inferred_name();
  }

  Handle<String> inferred_name() const { return inferred_name_; }
  void set_inferred_name(Handle<String> inferred_name) {
    inferred_name_ = inferred_name;
  }

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

  int ast_node_count() { return ast_properties_.node_count(); }
  AstProperties::Flags* flags() { return ast_properties_.flags(); }
  void set_ast_properties(AstProperties* ast_properties) {
    ast_properties_ = *ast_properties;
  }

 protected:
  FunctionLiteral(Isolate* isolate,
                  Handle<String> name,
                  Scope* scope,
                  ZoneList<Statement*>* body,
                  int materialized_literal_count,
                  int expected_property_count,
                  int handler_count,
                  bool has_only_simple_this_property_assignments,
                  Handle<FixedArray> this_property_assignments,
                  int parameter_count,
                  Type type,
                  ParameterFlag has_duplicate_parameters,
                  IsFunctionFlag is_function,
                  IsParenthesizedFlag is_parenthesized)
      : Expression(isolate),
        name_(name),
        scope_(scope),
        body_(body),
        this_property_assignments_(this_property_assignments),
        inferred_name_(isolate->factory()->empty_string()),
        materialized_literal_count_(materialized_literal_count),
        expected_property_count_(expected_property_count),
        handler_count_(handler_count),
        parameter_count_(parameter_count),
        function_token_position_(RelocInfo::kNoPosition) {
    bitfield_ =
        HasOnlySimpleThisPropertyAssignments::encode(
            has_only_simple_this_property_assignments) |
        IsExpression::encode(type != DECLARATION) |
        IsAnonymous::encode(type == ANONYMOUS_EXPRESSION) |
        Pretenure::encode(false) |
        HasDuplicateParameters::encode(has_duplicate_parameters) |
        IsFunction::encode(is_function) |
        IsParenthesized::encode(is_parenthesized);
  }

 private:
  Handle<String> name_;
  Scope* scope_;
  ZoneList<Statement*>* body_;
  Handle<FixedArray> this_property_assignments_;
  Handle<String> inferred_name_;
  AstProperties ast_properties_;

  int materialized_literal_count_;
  int expected_property_count_;
  int handler_count_;
  int parameter_count_;
  int function_token_position_;

  unsigned bitfield_;
  class HasOnlySimpleThisPropertyAssignments: public BitField<bool, 0, 1> {};
  class IsExpression: public BitField<bool, 1, 1> {};
  class IsAnonymous: public BitField<bool, 2, 1> {};
  class Pretenure: public BitField<bool, 3, 1> {};
  class HasDuplicateParameters: public BitField<ParameterFlag, 4, 1> {};
  class IsFunction: public BitField<IsFunctionFlag, 5, 1> {};
  class IsParenthesized: public BitField<IsParenthesizedFlag, 6, 1> {};
};


class SharedFunctionInfoLiteral: public Expression {
 public:
  DECLARE_NODE_TYPE(SharedFunctionInfoLiteral)

  Handle<SharedFunctionInfo> shared_function_info() const {
    return shared_function_info_;
  }

 protected:
  SharedFunctionInfoLiteral(
      Isolate* isolate,
      Handle<SharedFunctionInfo> shared_function_info)
      : Expression(isolate),
        shared_function_info_(shared_function_info) { }

 private:
  Handle<SharedFunctionInfo> shared_function_info_;
};


class ThisFunction: public Expression {
 public:
  DECLARE_NODE_TYPE(ThisFunction)

 protected:
  explicit ThisFunction(Isolate* isolate): Expression(isolate) {}
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
  virtual void AppendToText(RegExpText* text, Zone* zone);
  SmartArrayPointer<const char> ToString(Zone* zone);
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
  virtual void AppendToText(RegExpText* text, Zone* zone);
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
  virtual void AppendToText(RegExpText* text, Zone* zone);
  Vector<const uc16> data() { return data_; }
  int length() { return data_.length(); }
 private:
  Vector<const uc16> data_;
};


class RegExpText: public RegExpTree {
 public:
  explicit RegExpText(Zone* zone) : elements_(2, zone), length_(0) {}
  virtual void* Accept(RegExpVisitor* visitor, void* data);
  virtual RegExpNode* ToNode(RegExpCompiler* compiler,
                             RegExpNode* on_success);
  virtual RegExpText* AsText();
  virtual bool IsText();
  virtual bool IsTextElement() { return true; }
  virtual int min_match() { return length_; }
  virtual int max_match() { return length_; }
  virtual void AppendToText(RegExpText* text, Zone* zone);
  void AddElement(TextElement elm, Zone* zone)  {
    elements_.Add(elm, zone);
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
  static RegExpEmpty* GetInstance() {
    static RegExpEmpty* instance = ::new RegExpEmpty();
    return instance;
  }
};


// ----------------------------------------------------------------------------
// Out-of-line inline constructors (to side-step cyclic dependencies).

inline ModuleVariable::ModuleVariable(VariableProxy* proxy)
    : Module(proxy->interface()),
      proxy_(proxy) {
}


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


// ----------------------------------------------------------------------------
// Construction time visitor.

class AstConstructionVisitor BASE_EMBEDDED {
 public:
  AstConstructionVisitor() { }

  AstProperties* ast_properties() { return &properties_; }

 private:
  template<class> friend class AstNodeFactory;

  // Node visitors.
#define DEF_VISIT(type) \
  void Visit##type(type* node);
  AST_NODE_LIST(DEF_VISIT)
#undef DEF_VISIT

  void increase_node_count() { properties_.add_node_count(1); }
  void add_flag(AstPropertiesFlag flag) { properties_.flags()->Add(flag); }

  AstProperties properties_;
};


class AstNullVisitor BASE_EMBEDDED {
 public:
  // Node visitors.
#define DEF_VISIT(type) \
  void Visit##type(type* node) {}
  AST_NODE_LIST(DEF_VISIT)
#undef DEF_VISIT
};



// ----------------------------------------------------------------------------
// AstNode factory

template<class Visitor>
class AstNodeFactory BASE_EMBEDDED {
 public:
  AstNodeFactory(Isolate* isolate, Zone* zone)
      : isolate_(isolate),
        zone_(zone) { }

  Visitor* visitor() { return &visitor_; }

#define VISIT_AND_RETURN(NodeType, node) \
  visitor_.Visit##NodeType((node)); \
  return node;

  VariableDeclaration* NewVariableDeclaration(VariableProxy* proxy,
                                              VariableMode mode,
                                              Scope* scope) {
    VariableDeclaration* decl =
        new(zone_) VariableDeclaration(proxy, mode, scope);
    VISIT_AND_RETURN(VariableDeclaration, decl)
  }

  FunctionDeclaration* NewFunctionDeclaration(VariableProxy* proxy,
                                              VariableMode mode,
                                              FunctionLiteral* fun,
                                              Scope* scope) {
    FunctionDeclaration* decl =
        new(zone_) FunctionDeclaration(proxy, mode, fun, scope);
    VISIT_AND_RETURN(FunctionDeclaration, decl)
  }

  ModuleDeclaration* NewModuleDeclaration(VariableProxy* proxy,
                                          Module* module,
                                          Scope* scope) {
    ModuleDeclaration* decl =
        new(zone_) ModuleDeclaration(proxy, module, scope);
    VISIT_AND_RETURN(ModuleDeclaration, decl)
  }

  ImportDeclaration* NewImportDeclaration(VariableProxy* proxy,
                                          Module* module,
                                          Scope* scope) {
    ImportDeclaration* decl =
        new(zone_) ImportDeclaration(proxy, module, scope);
    VISIT_AND_RETURN(ImportDeclaration, decl)
  }

  ExportDeclaration* NewExportDeclaration(VariableProxy* proxy,
                                          Scope* scope) {
    ExportDeclaration* decl =
        new(zone_) ExportDeclaration(proxy, scope);
    VISIT_AND_RETURN(ExportDeclaration, decl)
  }

  ModuleLiteral* NewModuleLiteral(Block* body, Interface* interface) {
    ModuleLiteral* module = new(zone_) ModuleLiteral(body, interface);
    VISIT_AND_RETURN(ModuleLiteral, module)
  }

  ModuleVariable* NewModuleVariable(VariableProxy* proxy) {
    ModuleVariable* module = new(zone_) ModuleVariable(proxy);
    VISIT_AND_RETURN(ModuleVariable, module)
  }

  ModulePath* NewModulePath(Module* origin, Handle<String> name) {
    ModulePath* module = new(zone_) ModulePath(origin, name, zone_);
    VISIT_AND_RETURN(ModulePath, module)
  }

  ModuleUrl* NewModuleUrl(Handle<String> url) {
    ModuleUrl* module = new(zone_) ModuleUrl(url, zone_);
    VISIT_AND_RETURN(ModuleUrl, module)
  }

  Block* NewBlock(ZoneStringList* labels,
                  int capacity,
                  bool is_initializer_block) {
    Block* block = new(zone_) Block(
        isolate_, labels, capacity, is_initializer_block, zone_);
    VISIT_AND_RETURN(Block, block)
  }

#define STATEMENT_WITH_LABELS(NodeType) \
  NodeType* New##NodeType(ZoneStringList* labels) { \
    NodeType* stmt = new(zone_) NodeType(isolate_, labels); \
    VISIT_AND_RETURN(NodeType, stmt); \
  }
  STATEMENT_WITH_LABELS(DoWhileStatement)
  STATEMENT_WITH_LABELS(WhileStatement)
  STATEMENT_WITH_LABELS(ForStatement)
  STATEMENT_WITH_LABELS(ForInStatement)
  STATEMENT_WITH_LABELS(SwitchStatement)
#undef STATEMENT_WITH_LABELS

  ModuleStatement* NewModuleStatement(VariableProxy* proxy, Block* body) {
    ModuleStatement* stmt = new(zone_) ModuleStatement(proxy, body);
    VISIT_AND_RETURN(ModuleStatement, stmt)
  }

  ExpressionStatement* NewExpressionStatement(Expression* expression) {
    ExpressionStatement* stmt = new(zone_) ExpressionStatement(expression);
    VISIT_AND_RETURN(ExpressionStatement, stmt)
  }

  ContinueStatement* NewContinueStatement(IterationStatement* target) {
    ContinueStatement* stmt = new(zone_) ContinueStatement(target);
    VISIT_AND_RETURN(ContinueStatement, stmt)
  }

  BreakStatement* NewBreakStatement(BreakableStatement* target) {
    BreakStatement* stmt = new(zone_) BreakStatement(target);
    VISIT_AND_RETURN(BreakStatement, stmt)
  }

  ReturnStatement* NewReturnStatement(Expression* expression) {
    ReturnStatement* stmt = new(zone_) ReturnStatement(expression);
    VISIT_AND_RETURN(ReturnStatement, stmt)
  }

  WithStatement* NewWithStatement(Expression* expression,
                                  Statement* statement) {
    WithStatement* stmt = new(zone_) WithStatement(expression, statement);
    VISIT_AND_RETURN(WithStatement, stmt)
  }

  IfStatement* NewIfStatement(Expression* condition,
                              Statement* then_statement,
                              Statement* else_statement) {
    IfStatement* stmt = new(zone_) IfStatement(
        isolate_, condition, then_statement, else_statement);
    VISIT_AND_RETURN(IfStatement, stmt)
  }

  TryCatchStatement* NewTryCatchStatement(int index,
                                          Block* try_block,
                                          Scope* scope,
                                          Variable* variable,
                                          Block* catch_block) {
    TryCatchStatement* stmt = new(zone_) TryCatchStatement(
        index, try_block, scope, variable, catch_block);
    VISIT_AND_RETURN(TryCatchStatement, stmt)
  }

  TryFinallyStatement* NewTryFinallyStatement(int index,
                                              Block* try_block,
                                              Block* finally_block) {
    TryFinallyStatement* stmt =
        new(zone_) TryFinallyStatement(index, try_block, finally_block);
    VISIT_AND_RETURN(TryFinallyStatement, stmt)
  }

  DebuggerStatement* NewDebuggerStatement() {
    DebuggerStatement* stmt = new(zone_) DebuggerStatement();
    VISIT_AND_RETURN(DebuggerStatement, stmt)
  }

  EmptyStatement* NewEmptyStatement() {
    return new(zone_) EmptyStatement();
  }

  Literal* NewLiteral(Handle<Object> handle) {
    Literal* lit = new(zone_) Literal(isolate_, handle);
    VISIT_AND_RETURN(Literal, lit)
  }

  Literal* NewNumberLiteral(double number) {
    return NewLiteral(isolate_->factory()->NewNumber(number, TENURED));
  }

  ObjectLiteral* NewObjectLiteral(
      Handle<FixedArray> constant_properties,
      ZoneList<ObjectLiteral::Property*>* properties,
      int literal_index,
      bool is_simple,
      bool fast_elements,
      int depth,
      bool has_function) {
    ObjectLiteral* lit = new(zone_) ObjectLiteral(
        isolate_, constant_properties, properties, literal_index,
        is_simple, fast_elements, depth, has_function);
    VISIT_AND_RETURN(ObjectLiteral, lit)
  }

  ObjectLiteral::Property* NewObjectLiteralProperty(bool is_getter,
                                                    FunctionLiteral* value) {
    ObjectLiteral::Property* prop =
        new(zone_) ObjectLiteral::Property(is_getter, value);
    prop->set_key(NewLiteral(value->name()));
    return prop;  // Not an AST node, will not be visited.
  }

  RegExpLiteral* NewRegExpLiteral(Handle<String> pattern,
                                  Handle<String> flags,
                                  int literal_index) {
    RegExpLiteral* lit =
        new(zone_) RegExpLiteral(isolate_, pattern, flags, literal_index);
    VISIT_AND_RETURN(RegExpLiteral, lit);
  }

  ArrayLiteral* NewArrayLiteral(Handle<FixedArray> constant_elements,
                                ZoneList<Expression*>* values,
                                int literal_index,
                                bool is_simple,
                                int depth) {
    ArrayLiteral* lit = new(zone_) ArrayLiteral(
        isolate_, constant_elements, values, literal_index, is_simple, depth);
    VISIT_AND_RETURN(ArrayLiteral, lit)
  }

  VariableProxy* NewVariableProxy(Variable* var) {
    VariableProxy* proxy = new(zone_) VariableProxy(isolate_, var);
    VISIT_AND_RETURN(VariableProxy, proxy)
  }

  VariableProxy* NewVariableProxy(Handle<String> name,
                                  bool is_this,
                                  Interface* interface = Interface::NewValue(),
                                  int position = RelocInfo::kNoPosition) {
    VariableProxy* proxy =
        new(zone_) VariableProxy(isolate_, name, is_this, interface, position);
    VISIT_AND_RETURN(VariableProxy, proxy)
  }

  Property* NewProperty(Expression* obj, Expression* key, int pos) {
    Property* prop = new(zone_) Property(isolate_, obj, key, pos);
    VISIT_AND_RETURN(Property, prop)
  }

  Call* NewCall(Expression* expression,
                ZoneList<Expression*>* arguments,
                int pos) {
    Call* call = new(zone_) Call(isolate_, expression, arguments, pos);
    VISIT_AND_RETURN(Call, call)
  }

  CallNew* NewCallNew(Expression* expression,
                      ZoneList<Expression*>* arguments,
                      int pos) {
    CallNew* call = new(zone_) CallNew(isolate_, expression, arguments, pos);
    VISIT_AND_RETURN(CallNew, call)
  }

  CallRuntime* NewCallRuntime(Handle<String> name,
                              const Runtime::Function* function,
                              ZoneList<Expression*>* arguments) {
    CallRuntime* call =
        new(zone_) CallRuntime(isolate_, name, function, arguments);
    VISIT_AND_RETURN(CallRuntime, call)
  }

  UnaryOperation* NewUnaryOperation(Token::Value op,
                                    Expression* expression,
                                    int pos) {
    UnaryOperation* node =
        new(zone_) UnaryOperation(isolate_, op, expression, pos);
    VISIT_AND_RETURN(UnaryOperation, node)
  }

  BinaryOperation* NewBinaryOperation(Token::Value op,
                                      Expression* left,
                                      Expression* right,
                                      int pos) {
    BinaryOperation* node =
        new(zone_) BinaryOperation(isolate_, op, left, right, pos);
    VISIT_AND_RETURN(BinaryOperation, node)
  }

  CountOperation* NewCountOperation(Token::Value op,
                                    bool is_prefix,
                                    Expression* expr,
                                    int pos) {
    CountOperation* node =
        new(zone_) CountOperation(isolate_, op, is_prefix, expr, pos);
    VISIT_AND_RETURN(CountOperation, node)
  }

  CompareOperation* NewCompareOperation(Token::Value op,
                                        Expression* left,
                                        Expression* right,
                                        int pos) {
    CompareOperation* node =
        new(zone_) CompareOperation(isolate_, op, left, right, pos);
    VISIT_AND_RETURN(CompareOperation, node)
  }

  Conditional* NewConditional(Expression* condition,
                              Expression* then_expression,
                              Expression* else_expression,
                              int then_expression_position,
                              int else_expression_position) {
    Conditional* cond = new(zone_) Conditional(
        isolate_, condition, then_expression, else_expression,
        then_expression_position, else_expression_position);
    VISIT_AND_RETURN(Conditional, cond)
  }

  Assignment* NewAssignment(Token::Value op,
                            Expression* target,
                            Expression* value,
                            int pos) {
    Assignment* assign =
        new(zone_) Assignment(isolate_, op, target, value, pos);
    assign->Init(isolate_, this);
    VISIT_AND_RETURN(Assignment, assign)
  }

  Throw* NewThrow(Expression* exception, int pos) {
    Throw* t = new(zone_) Throw(isolate_, exception, pos);
    VISIT_AND_RETURN(Throw, t)
  }

  FunctionLiteral* NewFunctionLiteral(
      Handle<String> name,
      Scope* scope,
      ZoneList<Statement*>* body,
      int materialized_literal_count,
      int expected_property_count,
      int handler_count,
      bool has_only_simple_this_property_assignments,
      Handle<FixedArray> this_property_assignments,
      int parameter_count,
      FunctionLiteral::ParameterFlag has_duplicate_parameters,
      FunctionLiteral::Type type,
      FunctionLiteral::IsFunctionFlag is_function,
      FunctionLiteral::IsParenthesizedFlag is_parenthesized) {
    FunctionLiteral* lit = new(zone_) FunctionLiteral(
        isolate_, name, scope, body,
        materialized_literal_count, expected_property_count, handler_count,
        has_only_simple_this_property_assignments, this_property_assignments,
        parameter_count, type, has_duplicate_parameters, is_function,
        is_parenthesized);
    // Top-level literal doesn't count for the AST's properties.
    if (is_function == FunctionLiteral::kIsFunction) {
      visitor_.VisitFunctionLiteral(lit);
    }
    return lit;
  }

  SharedFunctionInfoLiteral* NewSharedFunctionInfoLiteral(
      Handle<SharedFunctionInfo> shared_function_info) {
    SharedFunctionInfoLiteral* lit =
        new(zone_) SharedFunctionInfoLiteral(isolate_, shared_function_info);
    VISIT_AND_RETURN(SharedFunctionInfoLiteral, lit)
  }

  ThisFunction* NewThisFunction() {
    ThisFunction* fun = new(zone_) ThisFunction(isolate_);
    VISIT_AND_RETURN(ThisFunction, fun)
  }

#undef VISIT_AND_RETURN

 private:
  Isolate* isolate_;
  Zone* zone_;
  Visitor visitor_;
};


} }  // namespace v8::internal

#endif  // V8_AST_H_
