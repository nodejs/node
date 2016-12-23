// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PARSING_PARSER_BASE_H
#define V8_PARSING_PARSER_BASE_H

#include "src/ast/ast.h"
#include "src/ast/scopes.h"
#include "src/bailout-reason.h"
#include "src/base/hashmap.h"
#include "src/globals.h"
#include "src/messages.h"
#include "src/parsing/expression-classifier.h"
#include "src/parsing/func-name-inferrer.h"
#include "src/parsing/scanner.h"
#include "src/parsing/token.h"

namespace v8 {
namespace internal {


enum FunctionNameValidity {
  kFunctionNameIsStrictReserved,
  kSkipFunctionNameCheck,
  kFunctionNameValidityUnknown
};

enum AllowLabelledFunctionStatement {
  kAllowLabelledFunctionStatement,
  kDisallowLabelledFunctionStatement,
};

enum class ParseFunctionFlags {
  kIsNormal = 0,
  kIsGenerator = 1,
  kIsAsync = 2,
  kIsDefault = 4
};

static inline ParseFunctionFlags operator|(ParseFunctionFlags lhs,
                                           ParseFunctionFlags rhs) {
  typedef unsigned char T;
  return static_cast<ParseFunctionFlags>(static_cast<T>(lhs) |
                                         static_cast<T>(rhs));
}

static inline ParseFunctionFlags& operator|=(ParseFunctionFlags& lhs,
                                             const ParseFunctionFlags& rhs) {
  lhs = lhs | rhs;
  return lhs;
}

static inline bool operator&(ParseFunctionFlags bitfield,
                             ParseFunctionFlags mask) {
  typedef unsigned char T;
  return static_cast<T>(bitfield) & static_cast<T>(mask);
}

struct FormalParametersBase {
  explicit FormalParametersBase(DeclarationScope* scope) : scope(scope) {}
  DeclarationScope* scope;
  bool has_rest = false;
  bool is_simple = true;
  int materialized_literals_count = 0;
};


// ----------------------------------------------------------------------------
// The CHECK_OK macro is a convenient macro to enforce error
// handling for functions that may fail (by returning !*ok).
//
// CAUTION: This macro appends extra statements after a call,
// thus it must never be used where only a single statement
// is correct (e.g. an if statement branch w/o braces)!

#define CHECK_OK_CUSTOM(x, ...) ok);       \
  if (!*ok) return impl()->x(__VA_ARGS__); \
  ((void)0
#define DUMMY )  // to make indentation work
#undef DUMMY

// Used in functions where the return type is ExpressionT.
#define CHECK_OK CHECK_OK_CUSTOM(EmptyExpression)

// Common base class template shared between parser and pre-parser.
// The Impl parameter is the actual class of the parser/pre-parser,
// following the Curiously Recurring Template Pattern (CRTP).
// The structure of the parser objects is roughly the following:
//
//   // A structure template containing type definitions, needed to
//   // avoid a cyclic dependency.
//   template <typename Impl>
//   struct ParserTypes;
//
//   // The parser base object, which should just implement pure
//   // parser behavior.  The Impl parameter is the actual derived
//   // class (according to CRTP), which implements impure parser
//   // behavior.
//   template <typename Impl>
//   class ParserBase { ... };
//
//   // And then, for each parser variant (e.g., parser, preparser, etc):
//   class Parser;
//
//   template <>
//   class ParserTypes<Parser> { ... };
//
//   class Parser : public ParserBase<Parser> { ... };
//
// The parser base object implements pure parsing, according to the
// language grammar.  Different parser implementations may exhibit
// different parser-driven behavior that is not considered as pure
// parsing, e.g., early error detection and reporting, AST generation, etc.

// The ParserTypes structure encapsulates the differences in the
// types used in parsing methods.  E.g., Parser methods use Expression*
// and PreParser methods use PreParserExpression.  For any given parser
// implementation class Impl, it is expected to contain the following typedefs:
//
// template <>
// struct ParserTypes<Impl> {
//   // Synonyms for ParserBase<Impl> and Impl, respectively.
//   typedef Base;
//   typedef Impl;
//   // TODO(nikolaos): this one will probably go away, as it is
//   // not related to pure parsing.
//   typedef Variable;
//   // Return types for traversing functions.
//   typedef Identifier;
//   typedef Expression;
//   typedef FunctionLiteral;
//   typedef ObjectLiteralProperty;
//   typedef ClassLiteralProperty;
//   typedef ExpressionList;
//   typedef ObjectPropertyList;
//   typedef ClassPropertyList;
//   typedef FormalParameters;
//   typedef Statement;
//   typedef StatementList;
//   typedef Block;
//   typedef BreakableStatement;
//   typedef IterationStatement;
//   // For constructing objects returned by the traversing functions.
//   typedef Factory;
//   // For other implementation-specific tasks.
//   typedef Target;
//   typedef TargetScope;
// };

template <typename Impl>
struct ParserTypes;

template <typename Impl>
class ParserBase {
 public:
  // Shorten type names defined by ParserTypes<Impl>.
  typedef ParserTypes<Impl> Types;
  typedef typename Types::Identifier IdentifierT;
  typedef typename Types::Expression ExpressionT;
  typedef typename Types::FunctionLiteral FunctionLiteralT;
  typedef typename Types::ObjectLiteralProperty ObjectLiteralPropertyT;
  typedef typename Types::ClassLiteralProperty ClassLiteralPropertyT;
  typedef typename Types::ExpressionList ExpressionListT;
  typedef typename Types::FormalParameters FormalParametersT;
  typedef typename Types::Statement StatementT;
  typedef typename Types::StatementList StatementListT;
  typedef typename Types::Block BlockT;
  typedef typename v8::internal::ExpressionClassifier<Types>
      ExpressionClassifier;

  // All implementation-specific methods must be called through this.
  Impl* impl() { return static_cast<Impl*>(this); }
  const Impl* impl() const { return static_cast<const Impl*>(this); }

  ParserBase(Zone* zone, Scanner* scanner, uintptr_t stack_limit,
             v8::Extension* extension, AstValueFactory* ast_value_factory,
             ParserRecorder* log)
      : scope_state_(nullptr),
        function_state_(nullptr),
        extension_(extension),
        fni_(nullptr),
        ast_value_factory_(ast_value_factory),
        ast_node_factory_(ast_value_factory),
        log_(log),
        mode_(PARSE_EAGERLY),  // Lazy mode must be set explicitly.
        parsing_module_(false),
        stack_limit_(stack_limit),
        zone_(zone),
        classifier_(nullptr),
        scanner_(scanner),
        stack_overflow_(false),
        allow_lazy_(false),
        allow_natives_(false),
        allow_tailcalls_(false),
        allow_harmony_restrictive_declarations_(false),
        allow_harmony_do_expressions_(false),
        allow_harmony_for_in_(false),
        allow_harmony_function_sent_(false),
        allow_harmony_async_await_(false),
        allow_harmony_restrictive_generators_(false),
        allow_harmony_trailing_commas_(false),
        allow_harmony_class_fields_(false) {}

#define ALLOW_ACCESSORS(name)                           \
  bool allow_##name() const { return allow_##name##_; } \
  void set_allow_##name(bool allow) { allow_##name##_ = allow; }

  ALLOW_ACCESSORS(lazy);
  ALLOW_ACCESSORS(natives);
  ALLOW_ACCESSORS(tailcalls);
  ALLOW_ACCESSORS(harmony_restrictive_declarations);
  ALLOW_ACCESSORS(harmony_do_expressions);
  ALLOW_ACCESSORS(harmony_for_in);
  ALLOW_ACCESSORS(harmony_function_sent);
  ALLOW_ACCESSORS(harmony_async_await);
  ALLOW_ACCESSORS(harmony_restrictive_generators);
  ALLOW_ACCESSORS(harmony_trailing_commas);
  ALLOW_ACCESSORS(harmony_class_fields);

#undef ALLOW_ACCESSORS

  uintptr_t stack_limit() const { return stack_limit_; }

  void set_stack_limit(uintptr_t stack_limit) { stack_limit_ = stack_limit; }

  Zone* zone() const { return zone_; }

 protected:
  friend class v8::internal::ExpressionClassifier<ParserTypes<Impl>>;

  // clang-format off
  enum AllowRestrictedIdentifiers {
    kAllowRestrictedIdentifiers,
    kDontAllowRestrictedIdentifiers
  };

  enum Mode {
    PARSE_LAZILY,
    PARSE_EAGERLY
  };

  enum LazyParsingResult {
    kLazyParsingComplete,
    kLazyParsingAborted
  };

  enum VariableDeclarationContext {
    kStatementListItem,
    kStatement,
    kForStatement
  };

  enum class FunctionBodyType {
    kNormal,
    kSingleExpression
  };
  // clang-format on

  class Checkpoint;
  class ClassLiteralChecker;
  class ObjectLiteralChecker;

  // ---------------------------------------------------------------------------
  // ScopeState and its subclasses implement the parser's scope stack.
  // ScopeState keeps track of the current scope, and the outer ScopeState. The
  // parser's scope_state_ points to the top ScopeState. ScopeState's
  // constructor push on the scope stack and the destructors pop. BlockState and
  // FunctionState are used to hold additional per-block and per-function state.
  class ScopeState BASE_EMBEDDED {
   public:
    V8_INLINE Scope* scope() const { return scope_; }
    Zone* zone() const { return scope_->zone(); }

   protected:
    ScopeState(ScopeState** scope_stack, Scope* scope)
        : scope_stack_(scope_stack), outer_scope_(*scope_stack), scope_(scope) {
      *scope_stack = this;
    }
    ~ScopeState() { *scope_stack_ = outer_scope_; }

   private:
    ScopeState** const scope_stack_;
    ScopeState* const outer_scope_;
    Scope* const scope_;
  };

  class BlockState final : public ScopeState {
   public:
    BlockState(ScopeState** scope_stack, Scope* scope)
        : ScopeState(scope_stack, scope) {}

    // BlockState(ScopeState**) automatically manages Scope(BLOCK_SCOPE)
    // allocation.
    // TODO(verwaest): Move to LazyBlockState class that only allocates the
    // scope when needed.
    explicit BlockState(Zone* zone, ScopeState** scope_stack)
        : ScopeState(scope_stack, NewScope(zone, *scope_stack)) {}

    void SetNonlinear() { this->scope()->SetNonlinear(); }
    void set_start_position(int pos) { this->scope()->set_start_position(pos); }
    void set_end_position(int pos) { this->scope()->set_end_position(pos); }
    void set_is_hidden() { this->scope()->set_is_hidden(); }
    Scope* FinalizedBlockScope() const {
      return this->scope()->FinalizeBlockScope();
    }
    LanguageMode language_mode() const {
      return this->scope()->language_mode();
    }

   private:
    Scope* NewScope(Zone* zone, ScopeState* outer_state) {
      Scope* parent = outer_state->scope();
      return new (zone) Scope(zone, parent, BLOCK_SCOPE);
    }
  };

  struct DestructuringAssignment {
   public:
    DestructuringAssignment(ExpressionT expression, Scope* scope)
        : assignment(expression), scope(scope) {}

    ExpressionT assignment;
    Scope* scope;
  };

  class TailCallExpressionList {
   public:
    explicit TailCallExpressionList(Zone* zone)
        : zone_(zone), expressions_(0, zone), has_explicit_tail_calls_(false) {}

    const ZoneList<ExpressionT>& expressions() const { return expressions_; }
    const Scanner::Location& location() const { return loc_; }

    bool has_explicit_tail_calls() const { return has_explicit_tail_calls_; }

    void Swap(TailCallExpressionList& other) {
      expressions_.Swap(&other.expressions_);
      std::swap(loc_, other.loc_);
      std::swap(has_explicit_tail_calls_, other.has_explicit_tail_calls_);
    }

    void AddImplicitTailCall(ExpressionT expr) {
      expressions_.Add(expr, zone_);
    }

    void Append(const TailCallExpressionList& other) {
      if (!has_explicit_tail_calls()) {
        loc_ = other.loc_;
        has_explicit_tail_calls_ = other.has_explicit_tail_calls_;
      }
      expressions_.AddAll(other.expressions_, zone_);
    }

   private:
    Zone* zone_;
    ZoneList<ExpressionT> expressions_;
    Scanner::Location loc_;
    bool has_explicit_tail_calls_;
  };

  // Defines whether tail call expressions are allowed or not.
  enum class ReturnExprContext {
    // We are inside return statement which is allowed to contain tail call
    // expressions. Tail call expressions are allowed.
    kInsideValidReturnStatement,

    // We are inside a block in which tail call expressions are allowed but
    // not yet inside a return statement.
    kInsideValidBlock,

    // Tail call expressions are not allowed in the following blocks.
    kInsideTryBlock,
    kInsideForInOfBody,
  };

  class FunctionState final : public ScopeState {
   public:
    FunctionState(FunctionState** function_state_stack,
                  ScopeState** scope_stack, DeclarationScope* scope);
    ~FunctionState();

    DeclarationScope* scope() const {
      return ScopeState::scope()->AsDeclarationScope();
    }

    int NextMaterializedLiteralIndex() {
      return next_materialized_literal_index_++;
    }
    int materialized_literal_count() {
      return next_materialized_literal_index_;
    }

    void SkipMaterializedLiterals(int count) {
      next_materialized_literal_index_ += count;
    }

    void AddProperty() { expected_property_count_++; }
    int expected_property_count() { return expected_property_count_; }

    FunctionKind kind() const { return scope()->function_kind(); }
    FunctionState* outer() const { return outer_function_state_; }

    void set_generator_object_variable(typename Types::Variable* variable) {
      DCHECK(variable != NULL);
      DCHECK(IsResumableFunction(kind()));
      generator_object_variable_ = variable;
    }
    typename Types::Variable* generator_object_variable() const {
      return generator_object_variable_;
    }

    void set_promise_variable(typename Types::Variable* variable) {
      DCHECK(variable != NULL);
      DCHECK(IsAsyncFunction(kind()));
      promise_variable_ = variable;
    }
    typename Types::Variable* promise_variable() const {
      return promise_variable_;
    }

    const ZoneList<DestructuringAssignment>&
        destructuring_assignments_to_rewrite() const {
      return destructuring_assignments_to_rewrite_;
    }

    TailCallExpressionList& tail_call_expressions() {
      return tail_call_expressions_;
    }
    void AddImplicitTailCallExpression(ExpressionT expression) {
      if (return_expr_context() ==
          ReturnExprContext::kInsideValidReturnStatement) {
        tail_call_expressions_.AddImplicitTailCall(expression);
      }
    }

    ZoneList<typename ExpressionClassifier::Error>* GetReportedErrorList() {
      return &reported_errors_;
    }

    ReturnExprContext return_expr_context() const {
      return return_expr_context_;
    }
    void set_return_expr_context(ReturnExprContext context) {
      return_expr_context_ = context;
    }

    ZoneList<ExpressionT>* non_patterns_to_rewrite() {
      return &non_patterns_to_rewrite_;
    }

    bool next_function_is_parenthesized() const {
      return next_function_is_parenthesized_;
    }

    void set_next_function_is_parenthesized(bool parenthesized) {
      next_function_is_parenthesized_ = parenthesized;
    }

    bool this_function_is_parenthesized() const {
      return this_function_is_parenthesized_;
    }

   private:
    void AddDestructuringAssignment(DestructuringAssignment pair) {
      destructuring_assignments_to_rewrite_.Add(pair, this->zone());
    }

    void AddNonPatternForRewriting(ExpressionT expr, bool* ok) {
      non_patterns_to_rewrite_.Add(expr, this->zone());
      if (non_patterns_to_rewrite_.length() >=
          std::numeric_limits<uint16_t>::max())
        *ok = false;
    }

    // Used to assign an index to each literal that needs materialization in
    // the function.  Includes regexp literals, and boilerplate for object and
    // array literals.
    int next_materialized_literal_index_;

    // Properties count estimation.
    int expected_property_count_;

    // For generators, this variable may hold the generator object. It variable
    // is used by yield expressions and return statements. It is not necessary
    // for generator functions to have this variable set.
    Variable* generator_object_variable_;
    // For async functions, this variable holds a temporary for the Promise
    // being created as output of the async function.
    Variable* promise_variable_;

    FunctionState** function_state_stack_;
    FunctionState* outer_function_state_;

    ZoneList<DestructuringAssignment> destructuring_assignments_to_rewrite_;
    TailCallExpressionList tail_call_expressions_;
    ReturnExprContext return_expr_context_;
    ZoneList<ExpressionT> non_patterns_to_rewrite_;

    ZoneList<typename ExpressionClassifier::Error> reported_errors_;

    // If true, the next (and immediately following) function literal is
    // preceded by a parenthesis.
    bool next_function_is_parenthesized_;

    // The value of the parents' next_function_is_parenthesized_, as it applies
    // to this function. Filled in by constructor.
    bool this_function_is_parenthesized_;

    friend Impl;
    friend class Checkpoint;
  };

  // This scope sets current ReturnExprContext to given value.
  class ReturnExprScope {
   public:
    explicit ReturnExprScope(FunctionState* function_state,
                             ReturnExprContext return_expr_context)
        : function_state_(function_state),
          sav_return_expr_context_(function_state->return_expr_context()) {
      // Don't update context if we are requested to enable tail call
      // expressions but current block does not allow them.
      if (return_expr_context !=
              ReturnExprContext::kInsideValidReturnStatement ||
          sav_return_expr_context_ == ReturnExprContext::kInsideValidBlock) {
        function_state->set_return_expr_context(return_expr_context);
      }
    }
    ~ReturnExprScope() {
      function_state_->set_return_expr_context(sav_return_expr_context_);
    }

   private:
    FunctionState* function_state_;
    ReturnExprContext sav_return_expr_context_;
  };

  // Collects all return expressions at tail call position in this scope
  // to a separate list.
  class CollectExpressionsInTailPositionToListScope {
   public:
    CollectExpressionsInTailPositionToListScope(FunctionState* function_state,
                                                TailCallExpressionList* list)
        : function_state_(function_state), list_(list) {
      function_state->tail_call_expressions().Swap(*list_);
    }
    ~CollectExpressionsInTailPositionToListScope() {
      function_state_->tail_call_expressions().Swap(*list_);
    }

   private:
    FunctionState* function_state_;
    TailCallExpressionList* list_;
  };

  // Annoyingly, arrow functions first parse as comma expressions, then when we
  // see the => we have to go back and reinterpret the arguments as being formal
  // parameters.  To do so we need to reset some of the parser state back to
  // what it was before the arguments were first seen.
  class Checkpoint BASE_EMBEDDED {
   public:
    explicit Checkpoint(ParserBase* parser) {
      function_state_ = parser->function_state_;
      next_materialized_literal_index_ =
          function_state_->next_materialized_literal_index_;
      expected_property_count_ = function_state_->expected_property_count_;
    }

    void Restore(int* materialized_literal_index_delta) {
      *materialized_literal_index_delta =
          function_state_->next_materialized_literal_index_ -
          next_materialized_literal_index_;
      function_state_->next_materialized_literal_index_ =
          next_materialized_literal_index_;
      function_state_->expected_property_count_ = expected_property_count_;
    }

   private:
    FunctionState* function_state_;
    int next_materialized_literal_index_;
    int expected_property_count_;
  };

  class ParsingModeScope BASE_EMBEDDED {
   public:
    ParsingModeScope(ParserBase* parser, Mode mode)
        : parser_(parser),
          old_mode_(parser->mode()) {
      parser_->mode_ = mode;
    }
    ~ParsingModeScope() {
      parser_->mode_ = old_mode_;
    }

   private:
    ParserBase* parser_;
    Mode old_mode_;
  };

  struct DeclarationDescriptor {
    enum Kind { NORMAL, PARAMETER };
    Scope* scope;
    Scope* hoist_scope;
    VariableMode mode;
    int declaration_pos;
    int initialization_pos;
    Kind declaration_kind;
  };

  struct DeclarationParsingResult {
    struct Declaration {
      Declaration(ExpressionT pattern, int initializer_position,
                  ExpressionT initializer)
          : pattern(pattern),
            initializer_position(initializer_position),
            initializer(initializer) {}

      ExpressionT pattern;
      int initializer_position;
      ExpressionT initializer;
    };

    DeclarationParsingResult()
        : declarations(4),
          first_initializer_loc(Scanner::Location::invalid()),
          bindings_loc(Scanner::Location::invalid()) {}

    DeclarationDescriptor descriptor;
    List<Declaration> declarations;
    Scanner::Location first_initializer_loc;
    Scanner::Location bindings_loc;
  };

  struct CatchInfo {
   public:
    explicit CatchInfo(ParserBase* parser)
        : name(parser->impl()->EmptyIdentifier()),
          variable(nullptr),
          pattern(parser->impl()->EmptyExpression()),
          scope(nullptr),
          init_block(parser->impl()->NullBlock()),
          inner_block(parser->impl()->NullBlock()),
          for_promise_reject(false),
          bound_names(1, parser->zone()),
          tail_call_expressions(parser->zone()) {}
    IdentifierT name;
    Variable* variable;
    ExpressionT pattern;
    Scope* scope;
    BlockT init_block;
    BlockT inner_block;
    bool for_promise_reject;
    ZoneList<const AstRawString*> bound_names;
    TailCallExpressionList tail_call_expressions;
  };

  struct ForInfo {
   public:
    explicit ForInfo(ParserBase* parser)
        : bound_names(1, parser->zone()),
          mode(ForEachStatement::ENUMERATE),
          each_loc(),
          parsing_result() {}
    ZoneList<const AstRawString*> bound_names;
    ForEachStatement::VisitMode mode;
    Scanner::Location each_loc;
    DeclarationParsingResult parsing_result;
  };

  struct ClassInfo {
   public:
    explicit ClassInfo(ParserBase* parser)
        : proxy(nullptr),
          extends(parser->impl()->EmptyExpression()),
          properties(parser->impl()->NewClassPropertyList(4)),
          instance_field_initializers(parser->impl()->NewExpressionList(0)),
          constructor(parser->impl()->EmptyFunctionLiteral()),
          has_seen_constructor(false),
          static_initializer_var(nullptr) {}
    VariableProxy* proxy;
    ExpressionT extends;
    typename Types::ClassPropertyList properties;
    ExpressionListT instance_field_initializers;
    FunctionLiteralT constructor;
    bool has_seen_constructor;
    Variable* static_initializer_var;
  };

  DeclarationScope* NewScriptScope() const {
    return new (zone()) DeclarationScope(zone(), ast_value_factory());
  }

  DeclarationScope* NewVarblockScope() const {
    return new (zone()) DeclarationScope(zone(), scope(), BLOCK_SCOPE);
  }

  ModuleScope* NewModuleScope(DeclarationScope* parent) const {
    return new (zone()) ModuleScope(parent, ast_value_factory());
  }

  DeclarationScope* NewEvalScope(Scope* parent) const {
    return new (zone()) DeclarationScope(zone(), parent, EVAL_SCOPE);
  }

  Scope* NewScope(ScopeType scope_type) const {
    return NewScopeWithParent(scope(), scope_type);
  }

  // This constructor should only be used when absolutely necessary. Most scopes
  // should automatically use scope() as parent, and be fine with
  // NewScope(ScopeType) above.
  Scope* NewScopeWithParent(Scope* parent, ScopeType scope_type) const {
    // Must always use the specific constructors for the blacklisted scope
    // types.
    DCHECK_NE(FUNCTION_SCOPE, scope_type);
    DCHECK_NE(SCRIPT_SCOPE, scope_type);
    DCHECK_NE(MODULE_SCOPE, scope_type);
    DCHECK_NOT_NULL(parent);
    return new (zone()) Scope(zone(), parent, scope_type);
  }

  DeclarationScope* NewFunctionScope(FunctionKind kind) const {
    DCHECK(ast_value_factory());
    DeclarationScope* result =
        new (zone()) DeclarationScope(zone(), scope(), FUNCTION_SCOPE, kind);
    // TODO(verwaest): Move into the DeclarationScope constructor.
    if (!IsArrowFunction(kind)) {
      result->DeclareDefaultFunctionVariables(ast_value_factory());
    }
    return result;
  }

  V8_INLINE DeclarationScope* GetDeclarationScope() const {
    return scope()->GetDeclarationScope();
  }
  V8_INLINE DeclarationScope* GetClosureScope() const {
    return scope()->GetClosureScope();
  }

  Scanner* scanner() const { return scanner_; }
  AstValueFactory* ast_value_factory() const { return ast_value_factory_; }
  int position() const { return scanner_->location().beg_pos; }
  int peek_position() const { return scanner_->peek_location().beg_pos; }
  bool stack_overflow() const { return stack_overflow_; }
  void set_stack_overflow() { stack_overflow_ = true; }
  Mode mode() const { return mode_; }

  INLINE(Token::Value peek()) {
    if (stack_overflow_) return Token::ILLEGAL;
    return scanner()->peek();
  }

  INLINE(Token::Value PeekAhead()) {
    if (stack_overflow_) return Token::ILLEGAL;
    return scanner()->PeekAhead();
  }

  INLINE(Token::Value Next()) {
    if (stack_overflow_) return Token::ILLEGAL;
    {
      if (GetCurrentStackPosition() < stack_limit_) {
        // Any further calls to Next or peek will return the illegal token.
        // The current call must return the next token, which might already
        // have been peek'ed.
        stack_overflow_ = true;
      }
    }
    return scanner()->Next();
  }

  void Consume(Token::Value token) {
    Token::Value next = Next();
    USE(next);
    USE(token);
    DCHECK(next == token);
  }

  bool Check(Token::Value token) {
    Token::Value next = peek();
    if (next == token) {
      Consume(next);
      return true;
    }
    return false;
  }

  void Expect(Token::Value token, bool* ok) {
    Token::Value next = Next();
    if (next != token) {
      ReportUnexpectedToken(next);
      *ok = false;
    }
  }

  void ExpectSemicolon(bool* ok) {
    // Check for automatic semicolon insertion according to
    // the rules given in ECMA-262, section 7.9, page 21.
    Token::Value tok = peek();
    if (tok == Token::SEMICOLON) {
      Next();
      return;
    }
    if (scanner()->HasAnyLineTerminatorBeforeNext() ||
        tok == Token::RBRACE ||
        tok == Token::EOS) {
      return;
    }
    Expect(Token::SEMICOLON, ok);
  }

  // Dummy functions, just useful as arguments to CHECK_OK_CUSTOM.
  static void Void() {}
  template <typename T>
  static T Return(T result) {
    return result;
  }

  bool is_any_identifier(Token::Value token) {
    return token == Token::IDENTIFIER || token == Token::ENUM ||
           token == Token::AWAIT || token == Token::ASYNC ||
           token == Token::FUTURE_STRICT_RESERVED_WORD || token == Token::LET ||
           token == Token::STATIC || token == Token::YIELD;
  }
  bool peek_any_identifier() { return is_any_identifier(peek()); }

  bool CheckContextualKeyword(Vector<const char> keyword) {
    if (PeekContextualKeyword(keyword)) {
      Consume(Token::IDENTIFIER);
      return true;
    }
    return false;
  }

  bool PeekContextualKeyword(Vector<const char> keyword) {
    return peek() == Token::IDENTIFIER &&
           scanner()->is_next_contextual_keyword(keyword);
  }

  void ExpectMetaProperty(Vector<const char> property_name,
                          const char* full_name, int pos, bool* ok);

  void ExpectContextualKeyword(Vector<const char> keyword, bool* ok) {
    Expect(Token::IDENTIFIER, CHECK_OK_CUSTOM(Void));
    if (!scanner()->is_literal_contextual_keyword(keyword)) {
      ReportUnexpectedToken(scanner()->current_token());
      *ok = false;
    }
  }

  bool CheckInOrOf(ForEachStatement::VisitMode* visit_mode) {
    if (Check(Token::IN)) {
      *visit_mode = ForEachStatement::ENUMERATE;
      return true;
    } else if (CheckContextualKeyword(CStrVector("of"))) {
      *visit_mode = ForEachStatement::ITERATE;
      return true;
    }
    return false;
  }

  bool PeekInOrOf() {
    return peek() == Token::IN || PeekContextualKeyword(CStrVector("of"));
  }

  // Checks whether an octal literal was last seen between beg_pos and end_pos.
  // If so, reports an error. Only called for strict mode and template strings.
  void CheckOctalLiteral(int beg_pos, int end_pos,
                         MessageTemplate::Template message, bool* ok) {
    Scanner::Location octal = scanner()->octal_position();
    if (octal.IsValid() && beg_pos <= octal.beg_pos &&
        octal.end_pos <= end_pos) {
      impl()->ReportMessageAt(octal, message);
      scanner()->clear_octal_position();
      *ok = false;
    }
  }
  // for now, this check just collects statistics.
  void CheckDecimalLiteralWithLeadingZero(int beg_pos, int end_pos) {
    Scanner::Location token_location =
        scanner()->decimal_with_leading_zero_position();
    if (token_location.IsValid() && beg_pos <= token_location.beg_pos &&
        token_location.end_pos <= end_pos) {
      scanner()->clear_decimal_with_leading_zero_position();
      impl()->CountUsage(v8::Isolate::kDecimalWithLeadingZeroInStrictMode);
    }
  }

  inline void CheckStrictOctalLiteral(int beg_pos, int end_pos, bool* ok) {
    CheckOctalLiteral(beg_pos, end_pos, MessageTemplate::kStrictOctalLiteral,
                      ok);
  }

  inline void CheckTemplateOctalLiteral(int beg_pos, int end_pos, bool* ok) {
    CheckOctalLiteral(beg_pos, end_pos, MessageTemplate::kTemplateOctalLiteral,
                      ok);
  }

  void CheckDestructuringElement(ExpressionT element, int beg_pos, int end_pos);

  // Checking the name of a function literal. This has to be done after parsing
  // the function, since the function can declare itself strict.
  void CheckFunctionName(LanguageMode language_mode, IdentifierT function_name,
                         FunctionNameValidity function_name_validity,
                         const Scanner::Location& function_name_loc, bool* ok) {
    if (function_name_validity == kSkipFunctionNameCheck) return;
    // The function name needs to be checked in strict mode.
    if (is_sloppy(language_mode)) return;

    if (impl()->IsEvalOrArguments(function_name)) {
      impl()->ReportMessageAt(function_name_loc,
                              MessageTemplate::kStrictEvalArguments);
      *ok = false;
      return;
    }
    if (function_name_validity == kFunctionNameIsStrictReserved) {
      impl()->ReportMessageAt(function_name_loc,
                              MessageTemplate::kUnexpectedStrictReserved);
      *ok = false;
      return;
    }
  }

  // Determine precedence of given token.
  static int Precedence(Token::Value token, bool accept_IN) {
    if (token == Token::IN && !accept_IN)
      return 0;  // 0 precedence will terminate binary expression parsing
    return Token::Precedence(token);
  }

  typename Types::Factory* factory() { return &ast_node_factory_; }

  DeclarationScope* GetReceiverScope() const {
    return scope()->GetReceiverScope();
  }
  LanguageMode language_mode() { return scope()->language_mode(); }
  void RaiseLanguageMode(LanguageMode mode) {
    LanguageMode old = scope()->language_mode();
    impl()->SetLanguageMode(scope(), old > mode ? old : mode);
  }
  bool is_generator() const {
    return IsGeneratorFunction(function_state_->kind());
  }
  bool is_async_function() const {
    return IsAsyncFunction(function_state_->kind());
  }
  bool is_resumable() const {
    return IsResumableFunction(function_state_->kind());
  }

  // Report syntax errors.
  void ReportMessage(MessageTemplate::Template message) {
    Scanner::Location source_location = scanner()->location();
    impl()->ReportMessageAt(source_location, message,
                            static_cast<const char*>(nullptr), kSyntaxError);
  }

  template <typename T>
  void ReportMessage(MessageTemplate::Template message, T arg,
                     ParseErrorType error_type = kSyntaxError) {
    Scanner::Location source_location = scanner()->location();
    impl()->ReportMessageAt(source_location, message, arg, error_type);
  }

  void ReportMessageAt(Scanner::Location location,
                       MessageTemplate::Template message,
                       ParseErrorType error_type) {
    impl()->ReportMessageAt(location, message,
                            static_cast<const char*>(nullptr), error_type);
  }

  void GetUnexpectedTokenMessage(
      Token::Value token, MessageTemplate::Template* message,
      Scanner::Location* location, const char** arg,
      MessageTemplate::Template default_ = MessageTemplate::kUnexpectedToken);

  void ReportUnexpectedToken(Token::Value token);
  void ReportUnexpectedTokenAt(
      Scanner::Location location, Token::Value token,
      MessageTemplate::Template message = MessageTemplate::kUnexpectedToken);

  void ReportClassifierError(
      const typename ExpressionClassifier::Error& error) {
    impl()->ReportMessageAt(error.location, error.message, error.arg,
                            error.type);
  }

  void ValidateExpression(bool* ok) {
    if (!classifier()->is_valid_expression()) {
      ReportClassifierError(classifier()->expression_error());
      *ok = false;
    }
  }

  void ValidateFormalParameterInitializer(bool* ok) {
    if (!classifier()->is_valid_formal_parameter_initializer()) {
      ReportClassifierError(classifier()->formal_parameter_initializer_error());
      *ok = false;
    }
  }

  void ValidateBindingPattern(bool* ok) {
    if (!classifier()->is_valid_binding_pattern()) {
      ReportClassifierError(classifier()->binding_pattern_error());
      *ok = false;
    }
  }

  void ValidateAssignmentPattern(bool* ok) {
    if (!classifier()->is_valid_assignment_pattern()) {
      ReportClassifierError(classifier()->assignment_pattern_error());
      *ok = false;
    }
  }

  void ValidateFormalParameters(LanguageMode language_mode,
                                bool allow_duplicates, bool* ok) {
    if (!allow_duplicates &&
        !classifier()->is_valid_formal_parameter_list_without_duplicates()) {
      ReportClassifierError(classifier()->duplicate_formal_parameter_error());
      *ok = false;
    } else if (is_strict(language_mode) &&
               !classifier()->is_valid_strict_mode_formal_parameters()) {
      ReportClassifierError(classifier()->strict_mode_formal_parameter_error());
      *ok = false;
    }
  }

  bool IsValidArrowFormalParametersStart(Token::Value token) {
    return is_any_identifier(token) || token == Token::LPAREN;
  }

  void ValidateArrowFormalParameters(ExpressionT expr,
                                     bool parenthesized_formals, bool is_async,
                                     bool* ok) {
    if (classifier()->is_valid_binding_pattern()) {
      // A simple arrow formal parameter: IDENTIFIER => BODY.
      if (!impl()->IsIdentifier(expr)) {
        impl()->ReportMessageAt(scanner()->location(),
                                MessageTemplate::kUnexpectedToken,
                                Token::String(scanner()->current_token()));
        *ok = false;
      }
    } else if (!classifier()->is_valid_arrow_formal_parameters()) {
      // If after parsing the expr, we see an error but the expression is
      // neither a valid binding pattern nor a valid parenthesized formal
      // parameter list, show the "arrow formal parameters" error if the formals
      // started with a parenthesis, and the binding pattern error otherwise.
      const typename ExpressionClassifier::Error& error =
          parenthesized_formals ? classifier()->arrow_formal_parameters_error()
                                : classifier()->binding_pattern_error();
      ReportClassifierError(error);
      *ok = false;
    }
    if (is_async && !classifier()->is_valid_async_arrow_formal_parameters()) {
      const typename ExpressionClassifier::Error& error =
          classifier()->async_arrow_formal_parameters_error();
      ReportClassifierError(error);
      *ok = false;
    }
  }

  void ValidateLetPattern(bool* ok) {
    if (!classifier()->is_valid_let_pattern()) {
      ReportClassifierError(classifier()->let_pattern_error());
      *ok = false;
    }
  }

  void ExpressionUnexpectedToken() {
    MessageTemplate::Template message = MessageTemplate::kUnexpectedToken;
    const char* arg;
    Scanner::Location location = scanner()->peek_location();
    GetUnexpectedTokenMessage(peek(), &message, &location, &arg);
    classifier()->RecordExpressionError(location, message, arg);
  }

  void BindingPatternUnexpectedToken() {
    MessageTemplate::Template message = MessageTemplate::kUnexpectedToken;
    const char* arg;
    Scanner::Location location = scanner()->peek_location();
    GetUnexpectedTokenMessage(peek(), &message, &location, &arg);
    classifier()->RecordBindingPatternError(location, message, arg);
  }

  void ArrowFormalParametersUnexpectedToken() {
    MessageTemplate::Template message = MessageTemplate::kUnexpectedToken;
    const char* arg;
    Scanner::Location location = scanner()->peek_location();
    GetUnexpectedTokenMessage(peek(), &message, &location, &arg);
    classifier()->RecordArrowFormalParametersError(location, message, arg);
  }

  // Recursive descent functions.
  // All ParseXXX functions take as the last argument an *ok parameter
  // which is set to false if parsing failed; it is unchanged otherwise.
  // By making the 'exception handling' explicit, we are forced to check
  // for failure at the call sites. The family of CHECK_OK* macros can
  // be useful for this.

  // Parses an identifier that is valid for the current scope, in particular it
  // fails on strict mode future reserved keywords in a strict scope. If
  // allow_eval_or_arguments is kAllowEvalOrArguments, we allow "eval" or
  // "arguments" as identifier even in strict mode (this is needed in cases like
  // "var foo = eval;").
  IdentifierT ParseIdentifier(AllowRestrictedIdentifiers, bool* ok);
  IdentifierT ParseAndClassifyIdentifier(bool* ok);
  // Parses an identifier or a strict mode future reserved word, and indicate
  // whether it is strict mode future reserved. Allows passing in function_kind
  // for the case of parsing the identifier in a function expression, where the
  // relevant "function_kind" bit is of the function being parsed, not the
  // containing function.
  IdentifierT ParseIdentifierOrStrictReservedWord(FunctionKind function_kind,
                                                  bool* is_strict_reserved,
                                                  bool* ok);
  IdentifierT ParseIdentifierOrStrictReservedWord(bool* is_strict_reserved,
                                                  bool* ok) {
    return ParseIdentifierOrStrictReservedWord(function_state_->kind(),
                                               is_strict_reserved, ok);
  }

  IdentifierT ParseIdentifierName(bool* ok);

  ExpressionT ParseRegExpLiteral(bool* ok);

  ExpressionT ParsePrimaryExpression(bool* is_async, bool* ok);
  ExpressionT ParsePrimaryExpression(bool* ok) {
    bool is_async;
    return ParsePrimaryExpression(&is_async, ok);
  }

  // This method wraps the parsing of the expression inside a new expression
  // classifier and calls RewriteNonPattern if parsing is successful.
  // It should be used whenever we're parsing an expression that will be
  // used as a non-pattern (i.e., in most cases).
  V8_INLINE ExpressionT ParseExpression(bool accept_IN, bool* ok);

  // This method does not wrap the parsing of the expression inside a
  // new expression classifier; it uses the top-level classifier instead.
  // It should be used whenever we're parsing something with the "cover"
  // grammar that recognizes both patterns and non-patterns (which roughly
  // corresponds to what's inside the parentheses generated by the symbol
  // "CoverParenthesizedExpressionAndArrowParameterList" in the ES 2017
  // specification).
  ExpressionT ParseExpressionCoverGrammar(bool accept_IN, bool* ok);

  ExpressionT ParseArrayLiteral(bool* ok);

  enum class PropertyKind {
    kAccessorProperty,
    kValueProperty,
    kShorthandProperty,
    kMethodProperty,
    kClassField,
    kNotSet
  };

  bool SetPropertyKindFromToken(Token::Value token, PropertyKind* kind);
  ExpressionT ParsePropertyName(IdentifierT* name, PropertyKind* kind,
                                bool* is_generator, bool* is_get, bool* is_set,
                                bool* is_async, bool* is_computed_name,
                                bool* ok);
  ExpressionT ParseObjectLiteral(bool* ok);
  ClassLiteralPropertyT ParseClassPropertyDefinition(
      ClassLiteralChecker* checker, bool has_extends, bool* is_computed_name,
      bool* has_seen_constructor, bool* ok);
  FunctionLiteralT ParseClassFieldForInitializer(bool has_initializer,
                                                 bool* ok);
  ObjectLiteralPropertyT ParseObjectPropertyDefinition(
      ObjectLiteralChecker* checker, bool* is_computed_name, bool* ok);
  ExpressionListT ParseArguments(Scanner::Location* first_spread_pos,
                                 bool maybe_arrow, bool* ok);
  ExpressionListT ParseArguments(Scanner::Location* first_spread_pos,
                                 bool* ok) {
    return ParseArguments(first_spread_pos, false, ok);
  }

  ExpressionT ParseAssignmentExpression(bool accept_IN, bool* ok);
  ExpressionT ParseYieldExpression(bool accept_IN, bool* ok);
  ExpressionT ParseConditionalExpression(bool accept_IN, bool* ok);
  ExpressionT ParseBinaryExpression(int prec, bool accept_IN, bool* ok);
  ExpressionT ParseUnaryExpression(bool* ok);
  ExpressionT ParsePostfixExpression(bool* ok);
  ExpressionT ParseLeftHandSideExpression(bool* ok);
  ExpressionT ParseMemberWithNewPrefixesExpression(bool* is_async, bool* ok);
  ExpressionT ParseMemberExpression(bool* is_async, bool* ok);
  ExpressionT ParseMemberExpressionContinuation(ExpressionT expression,
                                                bool* is_async, bool* ok);
  ExpressionT ParseArrowFunctionLiteral(bool accept_IN,
                                        const FormalParametersT& parameters,
                                        bool* ok);
  void ParseAsyncFunctionBody(Scope* scope, StatementListT body,
                              FunctionKind kind, FunctionBodyType type,
                              bool accept_IN, int pos, bool* ok);
  ExpressionT ParseAsyncFunctionLiteral(bool* ok);
  ExpressionT ParseClassLiteral(IdentifierT name,
                                Scanner::Location class_name_location,
                                bool name_is_strict_reserved,
                                int class_token_pos, bool* ok);
  ExpressionT ParseTemplateLiteral(ExpressionT tag, int start, bool* ok);
  ExpressionT ParseSuperExpression(bool is_new, bool* ok);
  ExpressionT ParseNewTargetExpression(bool* ok);

  void ParseFormalParameter(FormalParametersT* parameters, bool* ok);
  void ParseFormalParameterList(FormalParametersT* parameters, bool* ok);
  void CheckArityRestrictions(int param_count, FunctionKind function_type,
                              bool has_rest, int formals_start_pos,
                              int formals_end_pos, bool* ok);

  BlockT ParseVariableDeclarations(VariableDeclarationContext var_context,
                                   DeclarationParsingResult* parsing_result,
                                   ZoneList<const AstRawString*>* names,
                                   bool* ok);
  StatementT ParseAsyncFunctionDeclaration(ZoneList<const AstRawString*>* names,
                                           bool default_export, bool* ok);
  StatementT ParseFunctionDeclaration(bool* ok);
  StatementT ParseHoistableDeclaration(ZoneList<const AstRawString*>* names,
                                       bool default_export, bool* ok);
  StatementT ParseHoistableDeclaration(int pos, ParseFunctionFlags flags,
                                       ZoneList<const AstRawString*>* names,
                                       bool default_export, bool* ok);
  StatementT ParseClassDeclaration(ZoneList<const AstRawString*>* names,
                                   bool default_export, bool* ok);
  StatementT ParseNativeDeclaration(bool* ok);

  // Under some circumstances, we allow preparsing to abort if the preparsed
  // function is "long and trivial", and fully parse instead. Our current
  // definition of "long and trivial" is:
  // - over kLazyParseTrialLimit statements
  // - all starting with an identifier (i.e., no if, for, while, etc.)
  static const int kLazyParseTrialLimit = 200;

  // TODO(nikolaos, marja): The first argument should not really be passed
  // by value. The method is expected to add the parsed statements to the
  // list. This works because in the case of the parser, StatementListT is
  // a pointer whereas the preparser does not really modify the body.
  V8_INLINE void ParseStatementList(StatementListT body, int end_token,
                                    bool* ok) {
    LazyParsingResult result = ParseStatementList(body, end_token, false, ok);
    USE(result);
    DCHECK_EQ(result, kLazyParsingComplete);
  }
  LazyParsingResult ParseStatementList(StatementListT body, int end_token,
                                       bool may_abort, bool* ok);
  StatementT ParseStatementListItem(bool* ok);
  StatementT ParseStatement(ZoneList<const AstRawString*>* labels,
                            AllowLabelledFunctionStatement allow_function,
                            bool* ok);
  StatementT ParseStatementAsUnlabelled(ZoneList<const AstRawString*>* labels,
                                        bool* ok);
  BlockT ParseBlock(ZoneList<const AstRawString*>* labels, bool* ok);

  // Parse a SubStatement in strict mode, or with an extra block scope in
  // sloppy mode to handle
  // ES#sec-functiondeclarations-in-ifstatement-statement-clauses
  // The legacy parameter indicates whether function declarations are
  // banned by the ES2015 specification in this location, and they are being
  // permitted here to match previous V8 behavior.
  StatementT ParseScopedStatement(ZoneList<const AstRawString*>* labels,
                                  bool legacy, bool* ok);

  StatementT ParseVariableStatement(VariableDeclarationContext var_context,
                                    ZoneList<const AstRawString*>* names,
                                    bool* ok);

  // Magical syntax support.
  ExpressionT ParseV8Intrinsic(bool* ok);

  ExpressionT ParseDoExpression(bool* ok);

  StatementT ParseDebuggerStatement(bool* ok);

  StatementT ParseExpressionOrLabelledStatement(
      ZoneList<const AstRawString*>* labels,
      AllowLabelledFunctionStatement allow_function, bool* ok);
  StatementT ParseIfStatement(ZoneList<const AstRawString*>* labels, bool* ok);
  StatementT ParseContinueStatement(bool* ok);
  StatementT ParseBreakStatement(ZoneList<const AstRawString*>* labels,
                                 bool* ok);
  StatementT ParseReturnStatement(bool* ok);
  StatementT ParseWithStatement(ZoneList<const AstRawString*>* labels,
                                bool* ok);
  StatementT ParseDoWhileStatement(ZoneList<const AstRawString*>* labels,
                                   bool* ok);
  StatementT ParseWhileStatement(ZoneList<const AstRawString*>* labels,
                                 bool* ok);
  StatementT ParseThrowStatement(bool* ok);
  StatementT ParseSwitchStatement(ZoneList<const AstRawString*>* labels,
                                  bool* ok);
  StatementT ParseTryStatement(bool* ok);
  StatementT ParseForStatement(ZoneList<const AstRawString*>* labels, bool* ok);

  bool IsNextLetKeyword();
  bool IsTrivialExpression();

  // Checks if the expression is a valid reference expression (e.g., on the
  // left-hand side of assignments). Although ruled out by ECMA as early errors,
  // we allow calls for web compatibility and rewrite them to a runtime throw.
  ExpressionT CheckAndRewriteReferenceExpression(
      ExpressionT expression, int beg_pos, int end_pos,
      MessageTemplate::Template message, bool* ok);
  ExpressionT CheckAndRewriteReferenceExpression(
      ExpressionT expression, int beg_pos, int end_pos,
      MessageTemplate::Template message, ParseErrorType type, bool* ok);

  bool IsValidReferenceExpression(ExpressionT expression);

  bool IsAssignableIdentifier(ExpressionT expression) {
    if (!impl()->IsIdentifier(expression)) return false;
    if (is_strict(language_mode()) &&
        impl()->IsEvalOrArguments(impl()->AsIdentifier(expression))) {
      return false;
    }
    return true;
  }

  bool IsValidPattern(ExpressionT expression) {
    return expression->IsObjectLiteral() || expression->IsArrayLiteral();
  }

  // Keep track of eval() calls since they disable all local variable
  // optimizations. This checks if expression is an eval call, and if yes,
  // forwards the information to scope.
  Call::PossiblyEval CheckPossibleEvalCall(ExpressionT expression,
                                           Scope* scope) {
    if (impl()->IsIdentifier(expression) &&
        impl()->IsEval(impl()->AsIdentifier(expression))) {
      scope->RecordEvalCall();
      if (is_sloppy(scope->language_mode())) {
        // For sloppy scopes we also have to record the call at function level,
        // in case it includes declarations that will be hoisted.
        scope->GetDeclarationScope()->RecordEvalCall();
      }
      return Call::IS_POSSIBLY_EVAL;
    }
    return Call::NOT_EVAL;
  }

  // Validation per ES6 object literals.
  class ObjectLiteralChecker {
   public:
    explicit ObjectLiteralChecker(ParserBase* parser)
        : parser_(parser), has_seen_proto_(false) {}

    void CheckDuplicateProto(Token::Value property);

   private:
    bool IsProto() { return this->scanner()->LiteralMatches("__proto__", 9); }

    ParserBase* parser() const { return parser_; }
    Scanner* scanner() const { return parser_->scanner(); }

    ParserBase* parser_;
    bool has_seen_proto_;
  };

  // Validation per ES6 class literals.
  class ClassLiteralChecker {
   public:
    explicit ClassLiteralChecker(ParserBase* parser)
        : parser_(parser), has_seen_constructor_(false) {}

    void CheckClassMethodName(Token::Value property, PropertyKind type,
                              bool is_generator, bool is_async, bool is_static,
                              bool* ok);

   private:
    bool IsConstructor() {
      return this->scanner()->LiteralMatches("constructor", 11);
    }
    bool IsPrototype() {
      return this->scanner()->LiteralMatches("prototype", 9);
    }

    ParserBase* parser() const { return parser_; }
    Scanner* scanner() const { return parser_->scanner(); }

    ParserBase* parser_;
    bool has_seen_constructor_;
  };

  ModuleDescriptor* module() const {
    return scope()->AsModuleScope()->module();
  }
  Scope* scope() const { return scope_state_->scope(); }

  // Stack of expression classifiers.
  // The top of the stack is always pointed to by classifier().
  V8_INLINE ExpressionClassifier* classifier() const {
    DCHECK_NOT_NULL(classifier_);
    return classifier_;
  }

  // Accumulates the classifier that is on top of the stack (inner) to
  // the one that is right below (outer) and pops the inner.
  V8_INLINE void Accumulate(unsigned productions,
                            bool merge_non_patterns = true) {
    DCHECK_NOT_NULL(classifier_);
    ExpressionClassifier* previous = classifier_->previous();
    DCHECK_NOT_NULL(previous);
    previous->Accumulate(classifier_, productions, merge_non_patterns);
    classifier_ = previous;
  }

  // Pops and discards the classifier that is on top of the stack
  // without accumulating.
  V8_INLINE void Discard() {
    DCHECK_NOT_NULL(classifier_);
    classifier_->Discard();
    classifier_ = classifier_->previous();
  }

  // Accumulate errors that can be arbitrarily deep in an expression.
  // These correspond to the ECMAScript spec's 'Contains' operation
  // on productions. This includes:
  //
  // - YieldExpression is disallowed in arrow parameters in a generator.
  // - AwaitExpression is disallowed in arrow parameters in an async function.
  // - AwaitExpression is disallowed in async arrow parameters.
  //
  V8_INLINE void AccumulateFormalParameterContainmentErrors() {
    Accumulate(ExpressionClassifier::FormalParameterInitializerProduction |
               ExpressionClassifier::AsyncArrowFormalParametersProduction);
  }

  // Parser base's protected field members.

  ScopeState* scope_state_;        // Scope stack.
  FunctionState* function_state_;  // Function state stack.
  v8::Extension* extension_;
  FuncNameInferrer* fni_;
  AstValueFactory* ast_value_factory_;  // Not owned.
  typename Types::Factory ast_node_factory_;
  ParserRecorder* log_;
  Mode mode_;
  bool parsing_module_;
  uintptr_t stack_limit_;

  // Parser base's private field members.

 private:
  Zone* zone_;
  ExpressionClassifier* classifier_;

  Scanner* scanner_;
  bool stack_overflow_;

  bool allow_lazy_;
  bool allow_natives_;
  bool allow_tailcalls_;
  bool allow_harmony_restrictive_declarations_;
  bool allow_harmony_do_expressions_;
  bool allow_harmony_for_in_;
  bool allow_harmony_function_sent_;
  bool allow_harmony_async_await_;
  bool allow_harmony_restrictive_generators_;
  bool allow_harmony_trailing_commas_;
  bool allow_harmony_class_fields_;

  friend class DiscardableZoneScope;
};

template <typename Impl>
ParserBase<Impl>::FunctionState::FunctionState(
    FunctionState** function_state_stack, ScopeState** scope_stack,
    DeclarationScope* scope)
    : ScopeState(scope_stack, scope),
      next_materialized_literal_index_(0),
      expected_property_count_(0),
      generator_object_variable_(nullptr),
      promise_variable_(nullptr),
      function_state_stack_(function_state_stack),
      outer_function_state_(*function_state_stack),
      destructuring_assignments_to_rewrite_(16, scope->zone()),
      tail_call_expressions_(scope->zone()),
      return_expr_context_(ReturnExprContext::kInsideValidBlock),
      non_patterns_to_rewrite_(0, scope->zone()),
      reported_errors_(16, scope->zone()),
      next_function_is_parenthesized_(false),
      this_function_is_parenthesized_(false) {
  *function_state_stack = this;
  if (outer_function_state_) {
    this_function_is_parenthesized_ =
        outer_function_state_->next_function_is_parenthesized_;
    outer_function_state_->next_function_is_parenthesized_ = false;
  }
}

template <typename Impl>
ParserBase<Impl>::FunctionState::~FunctionState() {
  *function_state_stack_ = outer_function_state_;
}

template <typename Impl>
void ParserBase<Impl>::GetUnexpectedTokenMessage(
    Token::Value token, MessageTemplate::Template* message,
    Scanner::Location* location, const char** arg,
    MessageTemplate::Template default_) {
  *arg = nullptr;
  switch (token) {
    case Token::EOS:
      *message = MessageTemplate::kUnexpectedEOS;
      break;
    case Token::SMI:
    case Token::NUMBER:
      *message = MessageTemplate::kUnexpectedTokenNumber;
      break;
    case Token::STRING:
      *message = MessageTemplate::kUnexpectedTokenString;
      break;
    case Token::IDENTIFIER:
      *message = MessageTemplate::kUnexpectedTokenIdentifier;
      break;
    case Token::AWAIT:
    case Token::ENUM:
      *message = MessageTemplate::kUnexpectedReserved;
      break;
    case Token::LET:
    case Token::STATIC:
    case Token::YIELD:
    case Token::FUTURE_STRICT_RESERVED_WORD:
      *message = is_strict(language_mode())
                     ? MessageTemplate::kUnexpectedStrictReserved
                     : MessageTemplate::kUnexpectedTokenIdentifier;
      break;
    case Token::TEMPLATE_SPAN:
    case Token::TEMPLATE_TAIL:
      *message = MessageTemplate::kUnexpectedTemplateString;
      break;
    case Token::ESCAPED_STRICT_RESERVED_WORD:
    case Token::ESCAPED_KEYWORD:
      *message = MessageTemplate::kInvalidEscapedReservedWord;
      break;
    case Token::ILLEGAL:
      if (scanner()->has_error()) {
        *message = scanner()->error();
        *location = scanner()->error_location();
      } else {
        *message = MessageTemplate::kInvalidOrUnexpectedToken;
      }
      break;
    case Token::REGEXP_LITERAL:
      *message = MessageTemplate::kUnexpectedTokenRegExp;
      break;
    default:
      const char* name = Token::String(token);
      DCHECK(name != NULL);
      *arg = name;
      break;
  }
}

template <typename Impl>
void ParserBase<Impl>::ReportUnexpectedToken(Token::Value token) {
  return ReportUnexpectedTokenAt(scanner_->location(), token);
}

template <typename Impl>
void ParserBase<Impl>::ReportUnexpectedTokenAt(
    Scanner::Location source_location, Token::Value token,
    MessageTemplate::Template message) {
  const char* arg;
  GetUnexpectedTokenMessage(token, &message, &source_location, &arg);
  impl()->ReportMessageAt(source_location, message, arg);
}

template <typename Impl>
typename ParserBase<Impl>::IdentifierT ParserBase<Impl>::ParseIdentifier(
    AllowRestrictedIdentifiers allow_restricted_identifiers, bool* ok) {
  ExpressionClassifier classifier(this);
  auto result = ParseAndClassifyIdentifier(CHECK_OK_CUSTOM(EmptyIdentifier));

  if (allow_restricted_identifiers == kDontAllowRestrictedIdentifiers) {
    ValidateAssignmentPattern(CHECK_OK_CUSTOM(EmptyIdentifier));
    ValidateBindingPattern(CHECK_OK_CUSTOM(EmptyIdentifier));
  }

  return result;
}

template <typename Impl>
typename ParserBase<Impl>::IdentifierT
ParserBase<Impl>::ParseAndClassifyIdentifier(bool* ok) {
  Token::Value next = Next();
  if (next == Token::IDENTIFIER || next == Token::ASYNC ||
      (next == Token::AWAIT && !parsing_module_ && !is_async_function())) {
    IdentifierT name = impl()->GetSymbol();
    // When this function is used to read a formal parameter, we don't always
    // know whether the function is going to be strict or sloppy.  Indeed for
    // arrow functions we don't always know that the identifier we are reading
    // is actually a formal parameter.  Therefore besides the errors that we
    // must detect because we know we're in strict mode, we also record any
    // error that we might make in the future once we know the language mode.
    if (impl()->IsEvalOrArguments(name)) {
      classifier()->RecordStrictModeFormalParameterError(
          scanner()->location(), MessageTemplate::kStrictEvalArguments);
      if (is_strict(language_mode())) {
        classifier()->RecordBindingPatternError(
            scanner()->location(), MessageTemplate::kStrictEvalArguments);
      }
    } else if (next == Token::AWAIT) {
      classifier()->RecordAsyncArrowFormalParametersError(
          scanner()->location(), MessageTemplate::kAwaitBindingIdentifier);
    }

    if (classifier()->duplicate_finder() != nullptr &&
        scanner()->FindSymbol(classifier()->duplicate_finder(), 1) != 0) {
      classifier()->RecordDuplicateFormalParameterError(scanner()->location());
    }
    return name;
  } else if (is_sloppy(language_mode()) &&
             (next == Token::FUTURE_STRICT_RESERVED_WORD ||
              next == Token::ESCAPED_STRICT_RESERVED_WORD ||
              next == Token::LET || next == Token::STATIC ||
              (next == Token::YIELD && !is_generator()))) {
    classifier()->RecordStrictModeFormalParameterError(
        scanner()->location(), MessageTemplate::kUnexpectedStrictReserved);
    if (next == Token::ESCAPED_STRICT_RESERVED_WORD &&
        is_strict(language_mode())) {
      ReportUnexpectedToken(next);
      *ok = false;
      return impl()->EmptyIdentifier();
    }
    if (next == Token::LET ||
        (next == Token::ESCAPED_STRICT_RESERVED_WORD &&
         scanner()->is_literal_contextual_keyword(CStrVector("let")))) {
      classifier()->RecordLetPatternError(
          scanner()->location(), MessageTemplate::kLetInLexicalBinding);
    }
    return impl()->GetSymbol();
  } else {
    ReportUnexpectedToken(next);
    *ok = false;
    return impl()->EmptyIdentifier();
  }
}

template <class Impl>
typename ParserBase<Impl>::IdentifierT
ParserBase<Impl>::ParseIdentifierOrStrictReservedWord(
    FunctionKind function_kind, bool* is_strict_reserved, bool* ok) {
  Token::Value next = Next();
  if (next == Token::IDENTIFIER || (next == Token::AWAIT && !parsing_module_ &&
                                    !IsAsyncFunction(function_kind)) ||
      next == Token::ASYNC) {
    *is_strict_reserved = false;
  } else if (next == Token::FUTURE_STRICT_RESERVED_WORD || next == Token::LET ||
             next == Token::STATIC ||
             (next == Token::YIELD && !IsGeneratorFunction(function_kind))) {
    *is_strict_reserved = true;
  } else {
    ReportUnexpectedToken(next);
    *ok = false;
    return impl()->EmptyIdentifier();
  }

  return impl()->GetSymbol();
}

template <typename Impl>
typename ParserBase<Impl>::IdentifierT ParserBase<Impl>::ParseIdentifierName(
    bool* ok) {
  Token::Value next = Next();
  if (next != Token::IDENTIFIER && next != Token::ASYNC &&
      next != Token::ENUM && next != Token::AWAIT && next != Token::LET &&
      next != Token::STATIC && next != Token::YIELD &&
      next != Token::FUTURE_STRICT_RESERVED_WORD &&
      next != Token::ESCAPED_KEYWORD &&
      next != Token::ESCAPED_STRICT_RESERVED_WORD && !Token::IsKeyword(next)) {
    ReportUnexpectedToken(next);
    *ok = false;
    return impl()->EmptyIdentifier();
  }

  return impl()->GetSymbol();
}

template <typename Impl>
typename ParserBase<Impl>::ExpressionT ParserBase<Impl>::ParseRegExpLiteral(
    bool* ok) {
  int pos = peek_position();
  if (!scanner()->ScanRegExpPattern()) {
    Next();
    ReportMessage(MessageTemplate::kUnterminatedRegExp);
    *ok = false;
    return impl()->EmptyExpression();
  }

  int literal_index = function_state_->NextMaterializedLiteralIndex();

  IdentifierT js_pattern = impl()->GetNextSymbol();
  Maybe<RegExp::Flags> flags = scanner()->ScanRegExpFlags();
  if (flags.IsNothing()) {
    Next();
    ReportMessage(MessageTemplate::kMalformedRegExpFlags);
    *ok = false;
    return impl()->EmptyExpression();
  }
  int js_flags = flags.FromJust();
  Next();
  return factory()->NewRegExpLiteral(js_pattern, js_flags, literal_index, pos);
}

template <typename Impl>
typename ParserBase<Impl>::ExpressionT ParserBase<Impl>::ParsePrimaryExpression(
    bool* is_async, bool* ok) {
  // PrimaryExpression ::
  //   'this'
  //   'null'
  //   'true'
  //   'false'
  //   Identifier
  //   Number
  //   String
  //   ArrayLiteral
  //   ObjectLiteral
  //   RegExpLiteral
  //   ClassLiteral
  //   '(' Expression ')'
  //   TemplateLiteral
  //   do Block
  //   AsyncFunctionLiteral

  int beg_pos = peek_position();
  switch (peek()) {
    case Token::THIS: {
      BindingPatternUnexpectedToken();
      Consume(Token::THIS);
      return impl()->ThisExpression(beg_pos);
    }

    case Token::NULL_LITERAL:
    case Token::TRUE_LITERAL:
    case Token::FALSE_LITERAL:
    case Token::SMI:
    case Token::NUMBER:
      BindingPatternUnexpectedToken();
      return impl()->ExpressionFromLiteral(Next(), beg_pos);

    case Token::ASYNC:
      if (allow_harmony_async_await() &&
          !scanner()->HasAnyLineTerminatorAfterNext() &&
          PeekAhead() == Token::FUNCTION) {
        Consume(Token::ASYNC);
        return ParseAsyncFunctionLiteral(CHECK_OK);
      }
      // CoverCallExpressionAndAsyncArrowHead
      *is_async = true;
    /* falls through */
    case Token::IDENTIFIER:
    case Token::LET:
    case Token::STATIC:
    case Token::YIELD:
    case Token::AWAIT:
    case Token::ESCAPED_STRICT_RESERVED_WORD:
    case Token::FUTURE_STRICT_RESERVED_WORD: {
      // Using eval or arguments in this context is OK even in strict mode.
      IdentifierT name = ParseAndClassifyIdentifier(CHECK_OK);
      return impl()->ExpressionFromIdentifier(name, beg_pos,
                                              scanner()->location().end_pos);
    }

    case Token::STRING: {
      BindingPatternUnexpectedToken();
      Consume(Token::STRING);
      return impl()->ExpressionFromString(beg_pos);
    }

    case Token::ASSIGN_DIV:
    case Token::DIV:
      classifier()->RecordBindingPatternError(
          scanner()->peek_location(), MessageTemplate::kUnexpectedTokenRegExp);
      return ParseRegExpLiteral(ok);

    case Token::LBRACK:
      return ParseArrayLiteral(ok);

    case Token::LBRACE:
      return ParseObjectLiteral(ok);

    case Token::LPAREN: {
      // Arrow function formal parameters are either a single identifier or a
      // list of BindingPattern productions enclosed in parentheses.
      // Parentheses are not valid on the LHS of a BindingPattern, so we use the
      // is_valid_binding_pattern() check to detect multiple levels of
      // parenthesization.
      bool pattern_error = !classifier()->is_valid_binding_pattern();
      classifier()->RecordPatternError(scanner()->peek_location(),
                                       MessageTemplate::kUnexpectedToken,
                                       Token::String(Token::LPAREN));
      if (pattern_error) ArrowFormalParametersUnexpectedToken();
      Consume(Token::LPAREN);
      if (Check(Token::RPAREN)) {
        // ()=>x.  The continuation that looks for the => is in
        // ParseAssignmentExpression.
        classifier()->RecordExpressionError(scanner()->location(),
                                            MessageTemplate::kUnexpectedToken,
                                            Token::String(Token::RPAREN));
        return factory()->NewEmptyParentheses(beg_pos);
      }
      // Heuristically try to detect immediately called functions before
      // seeing the call parentheses.
      function_state_->set_next_function_is_parenthesized(peek() ==
                                                          Token::FUNCTION);
      ExpressionT expr = ParseExpressionCoverGrammar(true, CHECK_OK);
      Expect(Token::RPAREN, CHECK_OK);
      return expr;
    }

    case Token::CLASS: {
      BindingPatternUnexpectedToken();
      Consume(Token::CLASS);
      int class_token_pos = position();
      IdentifierT name = impl()->EmptyIdentifier();
      bool is_strict_reserved_name = false;
      Scanner::Location class_name_location = Scanner::Location::invalid();
      if (peek_any_identifier()) {
        name = ParseIdentifierOrStrictReservedWord(&is_strict_reserved_name,
                                                   CHECK_OK);
        class_name_location = scanner()->location();
      }
      return ParseClassLiteral(name, class_name_location,
                               is_strict_reserved_name, class_token_pos, ok);
    }

    case Token::TEMPLATE_SPAN:
    case Token::TEMPLATE_TAIL:
      BindingPatternUnexpectedToken();
      return ParseTemplateLiteral(impl()->NoTemplateTag(), beg_pos, ok);

    case Token::MOD:
      if (allow_natives() || extension_ != NULL) {
        BindingPatternUnexpectedToken();
        return ParseV8Intrinsic(ok);
      }
      break;

    case Token::DO:
      if (allow_harmony_do_expressions()) {
        BindingPatternUnexpectedToken();
        return ParseDoExpression(ok);
      }
      break;

    default:
      break;
  }

  ReportUnexpectedToken(Next());
  *ok = false;
  return impl()->EmptyExpression();
}

template <typename Impl>
typename ParserBase<Impl>::ExpressionT ParserBase<Impl>::ParseExpression(
    bool accept_IN, bool* ok) {
  ExpressionClassifier classifier(this);
  ExpressionT result = ParseExpressionCoverGrammar(accept_IN, CHECK_OK);
  impl()->RewriteNonPattern(CHECK_OK);
  return result;
}

template <typename Impl>
typename ParserBase<Impl>::ExpressionT
ParserBase<Impl>::ParseExpressionCoverGrammar(bool accept_IN, bool* ok) {
  // Expression ::
  //   AssignmentExpression
  //   Expression ',' AssignmentExpression

  ExpressionT result = impl()->EmptyExpression();
  while (true) {
    int comma_pos = position();
    ExpressionClassifier binding_classifier(this);
    ExpressionT right;
    if (Check(Token::ELLIPSIS)) {
      // 'x, y, ...z' in CoverParenthesizedExpressionAndArrowParameterList only
      // as the formal parameters of'(x, y, ...z) => foo', and is not itself a
      // valid expression.
      classifier()->RecordExpressionError(scanner()->location(),
                                          MessageTemplate::kUnexpectedToken,
                                          Token::String(Token::ELLIPSIS));
      int ellipsis_pos = position();
      int pattern_pos = peek_position();
      ExpressionT pattern = ParsePrimaryExpression(CHECK_OK);
      ValidateBindingPattern(CHECK_OK);
      right = factory()->NewSpread(pattern, ellipsis_pos, pattern_pos);
    } else {
      right = ParseAssignmentExpression(accept_IN, CHECK_OK);
    }
    // No need to accumulate binding pattern-related errors, since
    // an Expression can't be a binding pattern anyway.
    impl()->Accumulate(ExpressionClassifier::AllProductions &
                       ~(ExpressionClassifier::BindingPatternProduction |
                         ExpressionClassifier::LetPatternProduction));
    if (!impl()->IsIdentifier(right)) classifier()->RecordNonSimpleParameter();
    if (impl()->IsEmptyExpression(result)) {
      // First time through the loop.
      result = right;
    } else {
      result =
          factory()->NewBinaryOperation(Token::COMMA, result, right, comma_pos);
    }

    if (!Check(Token::COMMA)) break;

    if (right->IsSpread()) {
      classifier()->RecordArrowFormalParametersError(
          scanner()->location(), MessageTemplate::kParamAfterRest);
    }

    if (allow_harmony_trailing_commas() && peek() == Token::RPAREN &&
        PeekAhead() == Token::ARROW) {
      // a trailing comma is allowed at the end of an arrow parameter list
      break;
    }
  }

  return result;
}

template <typename Impl>
typename ParserBase<Impl>::ExpressionT ParserBase<Impl>::ParseArrayLiteral(
    bool* ok) {
  // ArrayLiteral ::
  //   '[' Expression? (',' Expression?)* ']'

  int pos = peek_position();
  ExpressionListT values = impl()->NewExpressionList(4);
  int first_spread_index = -1;
  Expect(Token::LBRACK, CHECK_OK);
  while (peek() != Token::RBRACK) {
    ExpressionT elem;
    if (peek() == Token::COMMA) {
      elem = impl()->GetLiteralTheHole(peek_position());
    } else if (peek() == Token::ELLIPSIS) {
      int start_pos = peek_position();
      Consume(Token::ELLIPSIS);
      int expr_pos = peek_position();
      ExpressionT argument = ParseAssignmentExpression(true, CHECK_OK);
      elem = factory()->NewSpread(argument, start_pos, expr_pos);

      if (first_spread_index < 0) {
        first_spread_index = values->length();
      }

      if (argument->IsAssignment()) {
        classifier()->RecordPatternError(
            Scanner::Location(start_pos, scanner()->location().end_pos),
            MessageTemplate::kInvalidDestructuringTarget);
      } else {
        CheckDestructuringElement(argument, start_pos,
                                  scanner()->location().end_pos);
      }

      if (peek() == Token::COMMA) {
        classifier()->RecordPatternError(
            Scanner::Location(start_pos, scanner()->location().end_pos),
            MessageTemplate::kElementAfterRest);
      }
    } else {
      int beg_pos = peek_position();
      elem = ParseAssignmentExpression(true, CHECK_OK);
      CheckDestructuringElement(elem, beg_pos, scanner()->location().end_pos);
    }
    values->Add(elem, zone_);
    if (peek() != Token::RBRACK) {
      Expect(Token::COMMA, CHECK_OK);
    }
  }
  Expect(Token::RBRACK, CHECK_OK);

  // Update the scope information before the pre-parsing bailout.
  int literal_index = function_state_->NextMaterializedLiteralIndex();

  ExpressionT result = factory()->NewArrayLiteral(values, first_spread_index,
                                                  literal_index, pos);
  if (first_spread_index >= 0) {
    result = factory()->NewRewritableExpression(result);
    impl()->QueueNonPatternForRewriting(result, ok);
    if (!*ok) {
      // If the non-pattern rewriting mechanism is used in the future for
      // rewriting other things than spreads, this error message will have
      // to change.  Also, this error message will never appear while pre-
      // parsing (this is OK, as it is an implementation limitation).
      ReportMessage(MessageTemplate::kTooManySpreads);
      return impl()->EmptyExpression();
    }
  }
  return result;
}

template <class Impl>
bool ParserBase<Impl>::SetPropertyKindFromToken(Token::Value token,
                                                PropertyKind* kind) {
  // This returns true, setting the property kind, iff the given token is one
  // which must occur after a property name, indicating that the previous token
  // was in fact a name and not a modifier (like the "get" in "get x").
  switch (token) {
    case Token::COLON:
      *kind = PropertyKind::kValueProperty;
      return true;
    case Token::COMMA:
    case Token::RBRACE:
    case Token::ASSIGN:
      *kind = PropertyKind::kShorthandProperty;
      return true;
    case Token::LPAREN:
      *kind = PropertyKind::kMethodProperty;
      return true;
    case Token::MUL:
    case Token::SEMICOLON:
      *kind = PropertyKind::kClassField;
      return true;
    default:
      break;
  }
  return false;
}

template <class Impl>
typename ParserBase<Impl>::ExpressionT ParserBase<Impl>::ParsePropertyName(
    IdentifierT* name, PropertyKind* kind, bool* is_generator, bool* is_get,
    bool* is_set, bool* is_async, bool* is_computed_name, bool* ok) {
  DCHECK(*kind == PropertyKind::kNotSet);
  DCHECK(!*is_generator);
  DCHECK(!*is_get);
  DCHECK(!*is_set);
  DCHECK(!*is_async);
  DCHECK(!*is_computed_name);

  *is_generator = Check(Token::MUL);
  if (*is_generator) {
    *kind = PropertyKind::kMethodProperty;
  }

  Token::Value token = peek();
  int pos = peek_position();

  if (allow_harmony_async_await() && !*is_generator && token == Token::ASYNC &&
      !scanner()->HasAnyLineTerminatorAfterNext()) {
    Consume(Token::ASYNC);
    token = peek();
    if (SetPropertyKindFromToken(token, kind)) {
      *name = impl()->GetSymbol();  // TODO(bakkot) specialize on 'async'
      impl()->PushLiteralName(*name);
      return factory()->NewStringLiteral(*name, pos);
    }
    *kind = PropertyKind::kMethodProperty;
    *is_async = true;
    pos = peek_position();
  }

  if (token == Token::IDENTIFIER && !*is_generator && !*is_async) {
    // This is checking for 'get' and 'set' in particular.
    Consume(Token::IDENTIFIER);
    token = peek();
    if (SetPropertyKindFromToken(token, kind) ||
        !scanner()->IsGetOrSet(is_get, is_set)) {
      *name = impl()->GetSymbol();
      impl()->PushLiteralName(*name);
      return factory()->NewStringLiteral(*name, pos);
    }
    *kind = PropertyKind::kAccessorProperty;
    pos = peek_position();
  }

  // For non computed property names we normalize the name a bit:
  //
  //   "12" -> 12
  //   12.3 -> "12.3"
  //   12.30 -> "12.3"
  //   identifier -> "identifier"
  //
  // This is important because we use the property name as a key in a hash
  // table when we compute constant properties.
  ExpressionT expression = impl()->EmptyExpression();
  switch (token) {
    case Token::STRING:
      Consume(Token::STRING);
      *name = impl()->GetSymbol();
      break;

    case Token::SMI:
      Consume(Token::SMI);
      *name = impl()->GetNumberAsSymbol();
      break;

    case Token::NUMBER:
      Consume(Token::NUMBER);
      *name = impl()->GetNumberAsSymbol();
      break;

    case Token::LBRACK: {
      *name = impl()->EmptyIdentifier();
      *is_computed_name = true;
      Consume(Token::LBRACK);
      ExpressionClassifier computed_name_classifier(this);
      expression = ParseAssignmentExpression(true, CHECK_OK);
      impl()->RewriteNonPattern(CHECK_OK);
      impl()->AccumulateFormalParameterContainmentErrors();
      Expect(Token::RBRACK, CHECK_OK);
      break;
    }

    default:
      *name = ParseIdentifierName(CHECK_OK);
      break;
  }

  if (*kind == PropertyKind::kNotSet) {
    SetPropertyKindFromToken(peek(), kind);
  }

  if (*is_computed_name) {
    return expression;
  }

  impl()->PushLiteralName(*name);

  uint32_t index;
  return impl()->IsArrayIndex(*name, &index)
             ? factory()->NewNumberLiteral(index, pos)
             : factory()->NewStringLiteral(*name, pos);
}

template <typename Impl>
typename ParserBase<Impl>::ClassLiteralPropertyT
ParserBase<Impl>::ParseClassPropertyDefinition(ClassLiteralChecker* checker,
                                               bool has_extends,
                                               bool* is_computed_name,
                                               bool* has_seen_constructor,
                                               bool* ok) {
  DCHECK(has_seen_constructor != nullptr);
  bool is_get = false;
  bool is_set = false;
  bool is_generator = false;
  bool is_async = false;
  bool is_static = false;
  PropertyKind kind = PropertyKind::kNotSet;

  Token::Value name_token = peek();

  IdentifierT name = impl()->EmptyIdentifier();
  ExpressionT name_expression;
  if (name_token == Token::STATIC) {
    Consume(Token::STATIC);
    if (peek() == Token::LPAREN) {
      kind = PropertyKind::kMethodProperty;
      name = impl()->GetSymbol();  // TODO(bakkot) specialize on 'static'
      name_expression = factory()->NewStringLiteral(name, position());
    } else if (peek() == Token::ASSIGN || peek() == Token::SEMICOLON ||
               peek() == Token::RBRACE) {
      name = impl()->GetSymbol();  // TODO(bakkot) specialize on 'static'
      name_expression = factory()->NewStringLiteral(name, position());
    } else {
      is_static = true;
      name_expression = ParsePropertyName(
          &name, &kind, &is_generator, &is_get, &is_set, &is_async,
          is_computed_name, CHECK_OK_CUSTOM(EmptyClassLiteralProperty));
    }
  } else {
    name_expression = ParsePropertyName(
        &name, &kind, &is_generator, &is_get, &is_set, &is_async,
        is_computed_name, CHECK_OK_CUSTOM(EmptyClassLiteralProperty));
  }

  switch (kind) {
    case PropertyKind::kClassField:
    case PropertyKind::kNotSet:  // This case is a name followed by a name or
                                 // other property. Here we have to assume
                                 // that's an uninitialized field followed by a
                                 // linebreak followed by a property, with ASI
                                 // adding the semicolon. If not, there will be
                                 // a syntax error after parsing the first name
                                 // as an uninitialized field.
    case PropertyKind::kShorthandProperty:
    case PropertyKind::kValueProperty:
      if (allow_harmony_class_fields()) {
        bool has_initializer = Check(Token::ASSIGN);
        ExpressionT function_literal = ParseClassFieldForInitializer(
            has_initializer, CHECK_OK_CUSTOM(EmptyClassLiteralProperty));
        ExpectSemicolon(CHECK_OK_CUSTOM(EmptyClassLiteralProperty));
        return factory()->NewClassLiteralProperty(
            name_expression, function_literal, ClassLiteralProperty::FIELD,
            is_static, *is_computed_name);
      } else {
        ReportUnexpectedToken(Next());
        *ok = false;
        return impl()->EmptyClassLiteralProperty();
      }

    case PropertyKind::kMethodProperty: {
      DCHECK(!is_get && !is_set);

      // MethodDefinition
      //    PropertyName '(' StrictFormalParameters ')' '{' FunctionBody '}'
      //    '*' PropertyName '(' StrictFormalParameters ')' '{' FunctionBody '}'

      if (!*is_computed_name) {
        checker->CheckClassMethodName(
            name_token, PropertyKind::kMethodProperty, is_generator, is_async,
            is_static, CHECK_OK_CUSTOM(EmptyClassLiteralProperty));
      }

      FunctionKind kind = is_generator
                              ? FunctionKind::kConciseGeneratorMethod
                              : is_async ? FunctionKind::kAsyncConciseMethod
                                         : FunctionKind::kConciseMethod;

      if (!is_static && impl()->IsConstructor(name)) {
        *has_seen_constructor = true;
        kind = has_extends ? FunctionKind::kSubclassConstructor
                           : FunctionKind::kBaseConstructor;
      }

      ExpressionT value = impl()->ParseFunctionLiteral(
          name, scanner()->location(), kSkipFunctionNameCheck, kind,
          kNoSourcePosition, FunctionLiteral::kAccessorOrMethod,
          language_mode(), CHECK_OK_CUSTOM(EmptyClassLiteralProperty));

      return factory()->NewClassLiteralProperty(name_expression, value,
                                                ClassLiteralProperty::METHOD,
                                                is_static, *is_computed_name);
    }

    case PropertyKind::kAccessorProperty: {
      DCHECK((is_get || is_set) && !is_generator && !is_async);

      if (!*is_computed_name) {
        checker->CheckClassMethodName(
            name_token, PropertyKind::kAccessorProperty, false, false,
            is_static, CHECK_OK_CUSTOM(EmptyClassLiteralProperty));
        // Make sure the name expression is a string since we need a Name for
        // Runtime_DefineAccessorPropertyUnchecked and since we can determine
        // this statically we can skip the extra runtime check.
        name_expression =
            factory()->NewStringLiteral(name, name_expression->position());
      }

      FunctionKind kind = is_get ? FunctionKind::kGetterFunction
                                 : FunctionKind::kSetterFunction;

      FunctionLiteralT value = impl()->ParseFunctionLiteral(
          name, scanner()->location(), kSkipFunctionNameCheck, kind,
          kNoSourcePosition, FunctionLiteral::kAccessorOrMethod,
          language_mode(), CHECK_OK_CUSTOM(EmptyClassLiteralProperty));

      if (!*is_computed_name) {
        impl()->AddAccessorPrefixToFunctionName(is_get, value, name);
      }

      return factory()->NewClassLiteralProperty(
          name_expression, value,
          is_get ? ClassLiteralProperty::GETTER : ClassLiteralProperty::SETTER,
          is_static, *is_computed_name);
    }
  }
  UNREACHABLE();
  return impl()->EmptyClassLiteralProperty();
}

template <typename Impl>
typename ParserBase<Impl>::FunctionLiteralT
ParserBase<Impl>::ParseClassFieldForInitializer(bool has_initializer,
                                                bool* ok) {
  // Makes a concise method which evaluates and returns the initialized value
  // (or undefined if absent).
  FunctionKind kind = FunctionKind::kConciseMethod;
  DeclarationScope* initializer_scope = NewFunctionScope(kind);
  initializer_scope->set_start_position(scanner()->location().end_pos);
  FunctionState initializer_state(&function_state_, &scope_state_,
                                  initializer_scope);
  DCHECK(scope() == initializer_scope);
  scope()->SetLanguageMode(STRICT);
  ExpressionClassifier expression_classifier(this);
  ExpressionT value;
  if (has_initializer) {
    value = this->ParseAssignmentExpression(
        true, CHECK_OK_CUSTOM(EmptyFunctionLiteral));
    impl()->RewriteNonPattern(CHECK_OK_CUSTOM(EmptyFunctionLiteral));
  } else {
    value = factory()->NewUndefinedLiteral(kNoSourcePosition);
  }
  initializer_scope->set_end_position(scanner()->location().end_pos);
  typename Types::StatementList body = impl()->NewStatementList(1);
  body->Add(factory()->NewReturnStatement(value, kNoSourcePosition), zone());
  FunctionLiteralT function_literal = factory()->NewFunctionLiteral(
      impl()->EmptyIdentifierString(), initializer_scope, body,
      initializer_state.materialized_literal_count(),
      initializer_state.expected_property_count(), 0,
      FunctionLiteral::kNoDuplicateParameters,
      FunctionLiteral::kAnonymousExpression,
      FunctionLiteral::kShouldLazyCompile, initializer_scope->start_position());
  function_literal->set_is_class_field_initializer(true);
  return function_literal;
}

template <typename Impl>
typename ParserBase<Impl>::ObjectLiteralPropertyT
ParserBase<Impl>::ParseObjectPropertyDefinition(ObjectLiteralChecker* checker,
                                                bool* is_computed_name,
                                                bool* ok) {
  bool is_get = false;
  bool is_set = false;
  bool is_generator = false;
  bool is_async = false;
  PropertyKind kind = PropertyKind::kNotSet;

  IdentifierT name = impl()->EmptyIdentifier();
  Token::Value name_token = peek();
  int next_beg_pos = scanner()->peek_location().beg_pos;
  int next_end_pos = scanner()->peek_location().end_pos;

  ExpressionT name_expression = ParsePropertyName(
      &name, &kind, &is_generator, &is_get, &is_set, &is_async,
      is_computed_name, CHECK_OK_CUSTOM(EmptyObjectLiteralProperty));

  switch (kind) {
    case PropertyKind::kValueProperty: {
      DCHECK(!is_get && !is_set && !is_generator && !is_async);

      if (!*is_computed_name) {
        checker->CheckDuplicateProto(name_token);
      }
      Consume(Token::COLON);
      int beg_pos = peek_position();
      ExpressionT value = ParseAssignmentExpression(
          true, CHECK_OK_CUSTOM(EmptyObjectLiteralProperty));
      CheckDestructuringElement(value, beg_pos, scanner()->location().end_pos);

      ObjectLiteralPropertyT result = factory()->NewObjectLiteralProperty(
          name_expression, value, *is_computed_name);

      if (!*is_computed_name) {
        impl()->SetFunctionNameFromPropertyName(result, name);
      }

      return result;
    }

    case PropertyKind::kShorthandProperty: {
      // PropertyDefinition
      //    IdentifierReference
      //    CoverInitializedName
      //
      // CoverInitializedName
      //    IdentifierReference Initializer?
      DCHECK(!is_get && !is_set && !is_generator && !is_async);

      if (!Token::IsIdentifier(name_token, language_mode(),
                               this->is_generator(),
                               parsing_module_ || is_async_function())) {
        ReportUnexpectedToken(Next());
        *ok = false;
        return impl()->EmptyObjectLiteralProperty();
      }

      DCHECK(!*is_computed_name);

      if (classifier()->duplicate_finder() != nullptr &&
          scanner()->FindSymbol(classifier()->duplicate_finder(), 1) != 0) {
        classifier()->RecordDuplicateFormalParameterError(
            scanner()->location());
      }

      if (impl()->IsEvalOrArguments(name) && is_strict(language_mode())) {
        classifier()->RecordBindingPatternError(
            scanner()->location(), MessageTemplate::kStrictEvalArguments);
      }

      if (name_token == Token::LET) {
        classifier()->RecordLetPatternError(
            scanner()->location(), MessageTemplate::kLetInLexicalBinding);
      }
      if (name_token == Token::AWAIT) {
        DCHECK(!is_async_function());
        classifier()->RecordAsyncArrowFormalParametersError(
            Scanner::Location(next_beg_pos, next_end_pos),
            MessageTemplate::kAwaitBindingIdentifier);
      }
      ExpressionT lhs =
          impl()->ExpressionFromIdentifier(name, next_beg_pos, next_end_pos);
      CheckDestructuringElement(lhs, next_beg_pos, next_end_pos);

      ExpressionT value;
      if (peek() == Token::ASSIGN) {
        Consume(Token::ASSIGN);
        ExpressionClassifier rhs_classifier(this);
        ExpressionT rhs = ParseAssignmentExpression(
            true, CHECK_OK_CUSTOM(EmptyObjectLiteralProperty));
        impl()->RewriteNonPattern(CHECK_OK_CUSTOM(EmptyObjectLiteralProperty));
        impl()->AccumulateFormalParameterContainmentErrors();
        value = factory()->NewAssignment(Token::ASSIGN, lhs, rhs,
                                         kNoSourcePosition);
        classifier()->RecordExpressionError(
            Scanner::Location(next_beg_pos, scanner()->location().end_pos),
            MessageTemplate::kInvalidCoverInitializedName);

        impl()->SetFunctionNameFromIdentifierRef(rhs, lhs);
      } else {
        value = lhs;
      }

      return factory()->NewObjectLiteralProperty(
          name_expression, value, ObjectLiteralProperty::COMPUTED, false);
    }

    case PropertyKind::kMethodProperty: {
      DCHECK(!is_get && !is_set);

      // MethodDefinition
      //    PropertyName '(' StrictFormalParameters ')' '{' FunctionBody '}'
      //    '*' PropertyName '(' StrictFormalParameters ')' '{' FunctionBody '}'

      classifier()->RecordPatternError(
          Scanner::Location(next_beg_pos, scanner()->location().end_pos),
          MessageTemplate::kInvalidDestructuringTarget);

      FunctionKind kind = is_generator
                              ? FunctionKind::kConciseGeneratorMethod
                              : is_async ? FunctionKind::kAsyncConciseMethod
                                         : FunctionKind::kConciseMethod;

      ExpressionT value = impl()->ParseFunctionLiteral(
          name, scanner()->location(), kSkipFunctionNameCheck, kind,
          kNoSourcePosition, FunctionLiteral::kAccessorOrMethod,
          language_mode(), CHECK_OK_CUSTOM(EmptyObjectLiteralProperty));

      return factory()->NewObjectLiteralProperty(
          name_expression, value, ObjectLiteralProperty::COMPUTED,
          *is_computed_name);
    }

    case PropertyKind::kAccessorProperty: {
      DCHECK((is_get || is_set) && !(is_set && is_get) && !is_generator &&
             !is_async);

      classifier()->RecordPatternError(
          Scanner::Location(next_beg_pos, scanner()->location().end_pos),
          MessageTemplate::kInvalidDestructuringTarget);

      if (!*is_computed_name) {
        // Make sure the name expression is a string since we need a Name for
        // Runtime_DefineAccessorPropertyUnchecked and since we can determine
        // this statically we can skip the extra runtime check.
        name_expression =
            factory()->NewStringLiteral(name, name_expression->position());
      }

      FunctionKind kind = is_get ? FunctionKind::kGetterFunction
                                 : FunctionKind::kSetterFunction;

      FunctionLiteralT value = impl()->ParseFunctionLiteral(
          name, scanner()->location(), kSkipFunctionNameCheck, kind,
          kNoSourcePosition, FunctionLiteral::kAccessorOrMethod,
          language_mode(), CHECK_OK_CUSTOM(EmptyObjectLiteralProperty));

      if (!*is_computed_name) {
        impl()->AddAccessorPrefixToFunctionName(is_get, value, name);
      }

      return factory()->NewObjectLiteralProperty(
          name_expression, value, is_get ? ObjectLiteralProperty::GETTER
                                         : ObjectLiteralProperty::SETTER,
          *is_computed_name);
    }

    case PropertyKind::kClassField:
    case PropertyKind::kNotSet:
      ReportUnexpectedToken(Next());
      *ok = false;
      return impl()->EmptyObjectLiteralProperty();
  }
  UNREACHABLE();
  return impl()->EmptyObjectLiteralProperty();
}

template <typename Impl>
typename ParserBase<Impl>::ExpressionT ParserBase<Impl>::ParseObjectLiteral(
    bool* ok) {
  // ObjectLiteral ::
  // '{' (PropertyDefinition (',' PropertyDefinition)* ','? )? '}'

  int pos = peek_position();
  typename Types::ObjectPropertyList properties =
      impl()->NewObjectPropertyList(4);
  int number_of_boilerplate_properties = 0;
  bool has_computed_names = false;
  ObjectLiteralChecker checker(this);

  Expect(Token::LBRACE, CHECK_OK);

  while (peek() != Token::RBRACE) {
    FuncNameInferrer::State fni_state(fni_);

    bool is_computed_name = false;
    ObjectLiteralPropertyT property =
        ParseObjectPropertyDefinition(&checker, &is_computed_name, CHECK_OK);

    if (is_computed_name) {
      has_computed_names = true;
    }

    // Count CONSTANT or COMPUTED properties to maintain the enumeration order.
    if (!has_computed_names && impl()->IsBoilerplateProperty(property)) {
      number_of_boilerplate_properties++;
    }
    properties->Add(property, zone());

    if (peek() != Token::RBRACE) {
      // Need {} because of the CHECK_OK macro.
      Expect(Token::COMMA, CHECK_OK);
    }

    if (fni_ != nullptr) fni_->Infer();
  }
  Expect(Token::RBRACE, CHECK_OK);

  // Computation of literal_index must happen before pre parse bailout.
  int literal_index = function_state_->NextMaterializedLiteralIndex();

  return factory()->NewObjectLiteral(properties,
                                     literal_index,
                                     number_of_boilerplate_properties,
                                     pos);
}

template <typename Impl>
typename ParserBase<Impl>::ExpressionListT ParserBase<Impl>::ParseArguments(
    Scanner::Location* first_spread_arg_loc, bool maybe_arrow, bool* ok) {
  // Arguments ::
  //   '(' (AssignmentExpression)*[','] ')'

  Scanner::Location spread_arg = Scanner::Location::invalid();
  ExpressionListT result = impl()->NewExpressionList(4);
  Expect(Token::LPAREN, CHECK_OK_CUSTOM(NullExpressionList));
  bool done = (peek() == Token::RPAREN);
  bool was_unspread = false;
  int unspread_sequences_count = 0;
  while (!done) {
    int start_pos = peek_position();
    bool is_spread = Check(Token::ELLIPSIS);
    int expr_pos = peek_position();

    ExpressionT argument =
        ParseAssignmentExpression(true, CHECK_OK_CUSTOM(NullExpressionList));
    if (!maybe_arrow) {
      impl()->RewriteNonPattern(CHECK_OK_CUSTOM(NullExpressionList));
    }
    if (is_spread) {
      if (!spread_arg.IsValid()) {
        spread_arg.beg_pos = start_pos;
        spread_arg.end_pos = peek_position();
      }
      argument = factory()->NewSpread(argument, start_pos, expr_pos);
    }
    result->Add(argument, zone_);

    // unspread_sequences_count is the number of sequences of parameters which
    // are not prefixed with a spread '...' operator.
    if (is_spread) {
      was_unspread = false;
    } else if (!was_unspread) {
      was_unspread = true;
      unspread_sequences_count++;
    }

    if (result->length() > Code::kMaxArguments) {
      ReportMessage(MessageTemplate::kTooManyArguments);
      *ok = false;
      return impl()->NullExpressionList();
    }
    done = (peek() != Token::COMMA);
    if (!done) {
      Next();
      if (allow_harmony_trailing_commas() && peek() == Token::RPAREN) {
        // allow trailing comma
        done = true;
      }
    }
  }
  Scanner::Location location = scanner_->location();
  if (Token::RPAREN != Next()) {
    impl()->ReportMessageAt(location, MessageTemplate::kUnterminatedArgList);
    *ok = false;
    return impl()->NullExpressionList();
  }
  *first_spread_arg_loc = spread_arg;

  if (!maybe_arrow || peek() != Token::ARROW) {
    if (maybe_arrow) {
      impl()->RewriteNonPattern(CHECK_OK_CUSTOM(NullExpressionList));
    }
    if (spread_arg.IsValid()) {
      // Unspread parameter sequences are translated into array literals in the
      // parser. Ensure that the number of materialized literals matches between
      // the parser and preparser
      impl()->MaterializeUnspreadArgumentsLiterals(unspread_sequences_count);
    }
  }

  return result;
}

// Precedence = 2
template <typename Impl>
typename ParserBase<Impl>::ExpressionT
ParserBase<Impl>::ParseAssignmentExpression(bool accept_IN, bool* ok) {
  // AssignmentExpression ::
  //   ConditionalExpression
  //   ArrowFunction
  //   YieldExpression
  //   LeftHandSideExpression AssignmentOperator AssignmentExpression
  int lhs_beg_pos = peek_position();

  if (peek() == Token::YIELD && is_generator()) {
    return ParseYieldExpression(accept_IN, ok);
  }

  FuncNameInferrer::State fni_state(fni_);
  Checkpoint checkpoint(this);
  ExpressionClassifier arrow_formals_classifier(
      this, classifier()->duplicate_finder());

  Scope::Snapshot scope_snapshot(scope());

  bool is_async = allow_harmony_async_await() && peek() == Token::ASYNC &&
                  !scanner()->HasAnyLineTerminatorAfterNext() &&
                  IsValidArrowFormalParametersStart(PeekAhead());

  bool parenthesized_formals = peek() == Token::LPAREN;
  if (!is_async && !parenthesized_formals) {
    ArrowFormalParametersUnexpectedToken();
  }

  // Parse a simple, faster sub-grammar (primary expression) if it's evident
  // that we have only a trivial expression to parse.
  ExpressionT expression;
  if (IsTrivialExpression()) {
    expression = ParsePrimaryExpression(&is_async, CHECK_OK);
  } else {
    expression = ParseConditionalExpression(accept_IN, CHECK_OK);
  }

  if (is_async && impl()->IsIdentifier(expression) && peek_any_identifier() &&
      PeekAhead() == Token::ARROW) {
    // async Identifier => AsyncConciseBody
    IdentifierT name = ParseAndClassifyIdentifier(CHECK_OK);
    expression = impl()->ExpressionFromIdentifier(
        name, position(), scanner()->location().end_pos, InferName::kNo);
    if (fni_) {
      // Remove `async` keyword from inferred name stack.
      fni_->RemoveAsyncKeywordFromEnd();
    }
  }

  if (peek() == Token::ARROW) {
    Scanner::Location arrow_loc = scanner()->peek_location();
    ValidateArrowFormalParameters(expression, parenthesized_formals, is_async,
                                  CHECK_OK);
    // This reads strangely, but is correct: it checks whether any
    // sub-expression of the parameter list failed to be a valid formal
    // parameter initializer. Since YieldExpressions are banned anywhere
    // in an arrow parameter list, this is correct.
    // TODO(adamk): Rename "FormalParameterInitializerError" to refer to
    // "YieldExpression", which is its only use.
    ValidateFormalParameterInitializer(ok);

    Scanner::Location loc(lhs_beg_pos, scanner()->location().end_pos);
    DeclarationScope* scope =
        NewFunctionScope(is_async ? FunctionKind::kAsyncArrowFunction
                                  : FunctionKind::kArrowFunction);
    // Because the arrow's parameters were parsed in the outer scope, any
    // usage flags that might have been triggered there need to be copied
    // to the arrow scope.
    this->scope()->PropagateUsageFlagsToScope(scope);

    scope_snapshot.Reparent(scope);

    FormalParametersT parameters(scope);
    if (!classifier()->is_simple_parameter_list()) {
      scope->SetHasNonSimpleParameters();
      parameters.is_simple = false;
    }

    checkpoint.Restore(&parameters.materialized_literals_count);

    scope->set_start_position(lhs_beg_pos);
    Scanner::Location duplicate_loc = Scanner::Location::invalid();
    impl()->DeclareArrowFunctionFormalParameters(&parameters, expression, loc,
                                                 &duplicate_loc, CHECK_OK);
    if (duplicate_loc.IsValid()) {
      classifier()->RecordDuplicateFormalParameterError(duplicate_loc);
    }
    expression = ParseArrowFunctionLiteral(accept_IN, parameters, CHECK_OK);
    impl()->Discard();
    classifier()->RecordPatternError(arrow_loc,
                                     MessageTemplate::kUnexpectedToken,
                                     Token::String(Token::ARROW));

    if (fni_ != nullptr) fni_->Infer();

    return expression;
  }

  // "expression" was not itself an arrow function parameter list, but it might
  // form part of one.  Propagate speculative formal parameter error locations
  // (including those for binding patterns, since formal parameters can
  // themselves contain binding patterns).
  unsigned productions = ExpressionClassifier::AllProductions &
                         ~ExpressionClassifier::ArrowFormalParametersProduction;

  // Parenthesized identifiers and property references are allowed as part
  // of a larger assignment pattern, even though parenthesized patterns
  // themselves are not allowed, e.g., "[(x)] = []". Only accumulate
  // assignment pattern errors if the parsed expression is more complex.
  if (IsValidReferenceExpression(expression)) {
    productions &= ~ExpressionClassifier::AssignmentPatternProduction;
  }

  const bool is_destructuring_assignment =
      IsValidPattern(expression) && peek() == Token::ASSIGN;
  if (is_destructuring_assignment) {
    // This is definitely not an expression so don't accumulate
    // expression-related errors.
    productions &= ~(ExpressionClassifier::ExpressionProduction |
                     ExpressionClassifier::TailCallExpressionProduction);
  }

  if (!Token::IsAssignmentOp(peek())) {
    // Parsed conditional expression only (no assignment).
    // Pending non-pattern expressions must be merged.
    impl()->Accumulate(productions);
    return expression;
  } else {
    // Pending non-pattern expressions must be discarded.
    impl()->Accumulate(productions, false);
  }

  if (is_destructuring_assignment) {
    ValidateAssignmentPattern(CHECK_OK);
  } else {
    expression = CheckAndRewriteReferenceExpression(
        expression, lhs_beg_pos, scanner()->location().end_pos,
        MessageTemplate::kInvalidLhsInAssignment, CHECK_OK);
  }

  expression = impl()->MarkExpressionAsAssigned(expression);

  Token::Value op = Next();  // Get assignment operator.
  if (op != Token::ASSIGN) {
    classifier()->RecordPatternError(scanner()->location(),
                                     MessageTemplate::kUnexpectedToken,
                                     Token::String(op));
  }
  int pos = position();

  ExpressionClassifier rhs_classifier(this);

  ExpressionT right = ParseAssignmentExpression(accept_IN, CHECK_OK);
  impl()->RewriteNonPattern(CHECK_OK);
  impl()->AccumulateFormalParameterContainmentErrors();

  // TODO(1231235): We try to estimate the set of properties set by
  // constructors. We define a new property whenever there is an
  // assignment to a property of 'this'. We should probably only add
  // properties if we haven't seen them before. Otherwise we'll
  // probably overestimate the number of properties.
  if (op == Token::ASSIGN && impl()->IsThisProperty(expression)) {
    function_state_->AddProperty();
  }

  impl()->CheckAssigningFunctionLiteralToProperty(expression, right);

  if (fni_ != NULL) {
    // Check if the right hand side is a call to avoid inferring a
    // name if we're dealing with "a = function(){...}();"-like
    // expression.
    if ((op == Token::INIT || op == Token::ASSIGN) &&
        (!right->IsCall() && !right->IsCallNew())) {
      fni_->Infer();
    } else {
      fni_->RemoveLastFunction();
    }
  }

  if (op == Token::ASSIGN) {
    impl()->SetFunctionNameFromIdentifierRef(right, expression);
  }

  if (op == Token::ASSIGN_EXP) {
    DCHECK(!is_destructuring_assignment);
    return impl()->RewriteAssignExponentiation(expression, right, pos);
  }

  ExpressionT result = factory()->NewAssignment(op, expression, right, pos);

  if (is_destructuring_assignment) {
    result = factory()->NewRewritableExpression(result);
    impl()->QueueDestructuringAssignmentForRewriting(result);
  }

  return result;
}

template <typename Impl>
typename ParserBase<Impl>::ExpressionT ParserBase<Impl>::ParseYieldExpression(
    bool accept_IN, bool* ok) {
  // YieldExpression ::
  //   'yield' ([no line terminator] '*'? AssignmentExpression)?
  int pos = peek_position();
  classifier()->RecordPatternError(
      scanner()->peek_location(), MessageTemplate::kInvalidDestructuringTarget);
  classifier()->RecordFormalParameterInitializerError(
      scanner()->peek_location(), MessageTemplate::kYieldInParameter);
  Expect(Token::YIELD, CHECK_OK);
  ExpressionT generator_object =
      factory()->NewVariableProxy(function_state_->generator_object_variable());
  // The following initialization is necessary.
  ExpressionT expression = impl()->EmptyExpression();
  bool delegating = false;  // yield*
  if (!scanner()->HasAnyLineTerminatorBeforeNext()) {
    if (Check(Token::MUL)) delegating = true;
    switch (peek()) {
      case Token::EOS:
      case Token::SEMICOLON:
      case Token::RBRACE:
      case Token::RBRACK:
      case Token::RPAREN:
      case Token::COLON:
      case Token::COMMA:
        // The above set of tokens is the complete set of tokens that can appear
        // after an AssignmentExpression, and none of them can start an
        // AssignmentExpression.  This allows us to avoid looking for an RHS for
        // a regular yield, given only one look-ahead token.
        if (!delegating) break;
        // Delegating yields require an RHS; fall through.
      default:
        expression = ParseAssignmentExpression(accept_IN, CHECK_OK);
        impl()->RewriteNonPattern(CHECK_OK);
        break;
    }
  }

  if (delegating) {
    return impl()->RewriteYieldStar(generator_object, expression, pos);
  }

  expression = impl()->BuildIteratorResult(expression, false);
  // Hackily disambiguate o from o.next and o [Symbol.iterator]().
  // TODO(verwaest): Come up with a better solution.
  ExpressionT yield = factory()->NewYield(generator_object, expression, pos,
                                          Yield::kOnExceptionThrow);
  return yield;
}

// Precedence = 3
template <typename Impl>
typename ParserBase<Impl>::ExpressionT
ParserBase<Impl>::ParseConditionalExpression(bool accept_IN,
                                             bool* ok) {
  // ConditionalExpression ::
  //   LogicalOrExpression
  //   LogicalOrExpression '?' AssignmentExpression ':' AssignmentExpression

  int pos = peek_position();
  // We start using the binary expression parser for prec >= 4 only!
  ExpressionT expression = ParseBinaryExpression(4, accept_IN, CHECK_OK);
  if (peek() != Token::CONDITIONAL) return expression;
  impl()->RewriteNonPattern(CHECK_OK);
  BindingPatternUnexpectedToken();
  ArrowFormalParametersUnexpectedToken();
  Consume(Token::CONDITIONAL);
  // In parsing the first assignment expression in conditional
  // expressions we always accept the 'in' keyword; see ECMA-262,
  // section 11.12, page 58.
  ExpressionT left = ParseAssignmentExpression(true, CHECK_OK);
  impl()->RewriteNonPattern(CHECK_OK);
  Expect(Token::COLON, CHECK_OK);
  ExpressionT right = ParseAssignmentExpression(accept_IN, CHECK_OK);
  impl()->RewriteNonPattern(CHECK_OK);
  return factory()->NewConditional(expression, left, right, pos);
}


// Precedence >= 4
template <typename Impl>
typename ParserBase<Impl>::ExpressionT ParserBase<Impl>::ParseBinaryExpression(
    int prec, bool accept_IN, bool* ok) {
  DCHECK(prec >= 4);
  ExpressionT x = ParseUnaryExpression(CHECK_OK);
  for (int prec1 = Precedence(peek(), accept_IN); prec1 >= prec; prec1--) {
    // prec1 >= 4
    while (Precedence(peek(), accept_IN) == prec1) {
      impl()->RewriteNonPattern(CHECK_OK);
      BindingPatternUnexpectedToken();
      ArrowFormalParametersUnexpectedToken();
      Token::Value op = Next();
      int pos = position();

      const bool is_right_associative = op == Token::EXP;
      const int next_prec = is_right_associative ? prec1 : prec1 + 1;
      ExpressionT y = ParseBinaryExpression(next_prec, accept_IN, CHECK_OK);
      impl()->RewriteNonPattern(CHECK_OK);

      if (impl()->ShortcutNumericLiteralBinaryExpression(&x, y, op, pos)) {
        continue;
      }

      // For now we distinguish between comparisons and other binary
      // operations.  (We could combine the two and get rid of this
      // code and AST node eventually.)
      if (Token::IsCompareOp(op)) {
        // We have a comparison.
        Token::Value cmp = op;
        switch (op) {
          case Token::NE: cmp = Token::EQ; break;
          case Token::NE_STRICT: cmp = Token::EQ_STRICT; break;
          default: break;
        }
        x = factory()->NewCompareOperation(cmp, x, y, pos);
        if (cmp != op) {
          // The comparison was negated - add a NOT.
          x = factory()->NewUnaryOperation(Token::NOT, x, pos);
        }
      } else if (op == Token::EXP) {
        x = impl()->RewriteExponentiation(x, y, pos);
      } else {
        // We have a "normal" binary operation.
        x = factory()->NewBinaryOperation(op, x, y, pos);
      }
    }
  }
  return x;
}

template <typename Impl>
typename ParserBase<Impl>::ExpressionT ParserBase<Impl>::ParseUnaryExpression(
    bool* ok) {
  // UnaryExpression ::
  //   PostfixExpression
  //   'delete' UnaryExpression
  //   'void' UnaryExpression
  //   'typeof' UnaryExpression
  //   '++' UnaryExpression
  //   '--' UnaryExpression
  //   '+' UnaryExpression
  //   '-' UnaryExpression
  //   '~' UnaryExpression
  //   '!' UnaryExpression
  //   [+Await] AwaitExpression[?Yield]

  Token::Value op = peek();
  if (Token::IsUnaryOp(op)) {
    BindingPatternUnexpectedToken();
    ArrowFormalParametersUnexpectedToken();

    op = Next();
    int pos = position();
    ExpressionT expression = ParseUnaryExpression(CHECK_OK);
    impl()->RewriteNonPattern(CHECK_OK);

    if (op == Token::DELETE && is_strict(language_mode())) {
      if (impl()->IsIdentifier(expression)) {
        // "delete identifier" is a syntax error in strict mode.
        ReportMessage(MessageTemplate::kStrictDelete);
        *ok = false;
        return impl()->EmptyExpression();
      }
    }

    if (peek() == Token::EXP) {
      ReportUnexpectedToken(Next());
      *ok = false;
      return impl()->EmptyExpression();
    }

    // Allow the parser's implementation to rewrite the expression.
    return impl()->BuildUnaryExpression(expression, op, pos);
  } else if (Token::IsCountOp(op)) {
    BindingPatternUnexpectedToken();
    ArrowFormalParametersUnexpectedToken();
    op = Next();
    int beg_pos = peek_position();
    ExpressionT expression = ParseUnaryExpression(CHECK_OK);
    expression = CheckAndRewriteReferenceExpression(
        expression, beg_pos, scanner()->location().end_pos,
        MessageTemplate::kInvalidLhsInPrefixOp, CHECK_OK);
    expression = impl()->MarkExpressionAsAssigned(expression);
    impl()->RewriteNonPattern(CHECK_OK);

    return factory()->NewCountOperation(op,
                                        true /* prefix */,
                                        expression,
                                        position());

  } else if (is_async_function() && peek() == Token::AWAIT) {
    classifier()->RecordFormalParameterInitializerError(
        scanner()->peek_location(),
        MessageTemplate::kAwaitExpressionFormalParameter);

    int await_pos = peek_position();
    Consume(Token::AWAIT);

    ExpressionT value = ParseUnaryExpression(CHECK_OK);

    return impl()->RewriteAwaitExpression(value, await_pos);
  } else {
    return ParsePostfixExpression(ok);
  }
}

template <typename Impl>
typename ParserBase<Impl>::ExpressionT ParserBase<Impl>::ParsePostfixExpression(
    bool* ok) {
  // PostfixExpression ::
  //   LeftHandSideExpression ('++' | '--')?

  int lhs_beg_pos = peek_position();
  ExpressionT expression = ParseLeftHandSideExpression(CHECK_OK);
  if (!scanner()->HasAnyLineTerminatorBeforeNext() &&
      Token::IsCountOp(peek())) {
    BindingPatternUnexpectedToken();
    ArrowFormalParametersUnexpectedToken();

    expression = CheckAndRewriteReferenceExpression(
        expression, lhs_beg_pos, scanner()->location().end_pos,
        MessageTemplate::kInvalidLhsInPostfixOp, CHECK_OK);
    expression = impl()->MarkExpressionAsAssigned(expression);
    impl()->RewriteNonPattern(CHECK_OK);

    Token::Value next = Next();
    expression =
        factory()->NewCountOperation(next,
                                     false /* postfix */,
                                     expression,
                                     position());
  }
  return expression;
}

template <typename Impl>
typename ParserBase<Impl>::ExpressionT
ParserBase<Impl>::ParseLeftHandSideExpression(bool* ok) {
  // LeftHandSideExpression ::
  //   (NewExpression | MemberExpression) ...

  bool is_async = false;
  ExpressionT result =
      ParseMemberWithNewPrefixesExpression(&is_async, CHECK_OK);

  while (true) {
    switch (peek()) {
      case Token::LBRACK: {
        impl()->RewriteNonPattern(CHECK_OK);
        BindingPatternUnexpectedToken();
        ArrowFormalParametersUnexpectedToken();
        Consume(Token::LBRACK);
        int pos = position();
        ExpressionT index = ParseExpressionCoverGrammar(true, CHECK_OK);
        impl()->RewriteNonPattern(CHECK_OK);
        result = factory()->NewProperty(result, index, pos);
        Expect(Token::RBRACK, CHECK_OK);
        break;
      }

      case Token::LPAREN: {
        int pos;
        impl()->RewriteNonPattern(CHECK_OK);
        BindingPatternUnexpectedToken();
        if (scanner()->current_token() == Token::IDENTIFIER ||
            scanner()->current_token() == Token::SUPER ||
            scanner()->current_token() == Token::ASYNC) {
          // For call of an identifier we want to report position of
          // the identifier as position of the call in the stack trace.
          pos = position();
        } else {
          // For other kinds of calls we record position of the parenthesis as
          // position of the call. Note that this is extremely important for
          // expressions of the form function(){...}() for which call position
          // should not point to the closing brace otherwise it will intersect
          // with positions recorded for function literal and confuse debugger.
          pos = peek_position();
          // Also the trailing parenthesis are a hint that the function will
          // be called immediately. If we happen to have parsed a preceding
          // function literal eagerly, we can also compile it eagerly.
          if (result->IsFunctionLiteral() && mode() == PARSE_EAGERLY) {
            result->AsFunctionLiteral()->set_should_eager_compile();
          }
        }
        Scanner::Location spread_pos;
        ExpressionListT args;
        if (V8_UNLIKELY(is_async && impl()->IsIdentifier(result))) {
          ExpressionClassifier async_classifier(this);
          args = ParseArguments(&spread_pos, true, CHECK_OK);
          if (peek() == Token::ARROW) {
            if (fni_) {
              fni_->RemoveAsyncKeywordFromEnd();
            }
            ValidateBindingPattern(CHECK_OK);
            ValidateFormalParameterInitializer(CHECK_OK);
            if (!classifier()->is_valid_async_arrow_formal_parameters()) {
              ReportClassifierError(
                  classifier()->async_arrow_formal_parameters_error());
              *ok = false;
              return impl()->EmptyExpression();
            }
            if (args->length()) {
              // async ( Arguments ) => ...
              return impl()->ExpressionListToExpression(args);
            }
            // async () => ...
            return factory()->NewEmptyParentheses(pos);
          } else {
            impl()->AccumulateFormalParameterContainmentErrors();
          }
        } else {
          args = ParseArguments(&spread_pos, false, CHECK_OK);
        }

        ArrowFormalParametersUnexpectedToken();

        // Keep track of eval() calls since they disable all local variable
        // optimizations.
        // The calls that need special treatment are the
        // direct eval calls. These calls are all of the form eval(...), with
        // no explicit receiver.
        // These calls are marked as potentially direct eval calls. Whether
        // they are actually direct calls to eval is determined at run time.
        Call::PossiblyEval is_possibly_eval =
            CheckPossibleEvalCall(result, scope());

        bool is_super_call = result->IsSuperCallReference();
        if (spread_pos.IsValid()) {
          args = impl()->PrepareSpreadArguments(args);
          result = impl()->SpreadCall(result, args, pos);
        } else {
          result = factory()->NewCall(result, args, pos, is_possibly_eval);
        }

        // Explicit calls to the super constructor using super() perform an
        // implicit binding assignment to the 'this' variable.
        if (is_super_call) {
          result = impl()->RewriteSuperCall(result);
          ExpressionT this_expr = impl()->ThisExpression(pos);
          result =
              factory()->NewAssignment(Token::INIT, this_expr, result, pos);
        }

        if (fni_ != NULL) fni_->RemoveLastFunction();
        break;
      }

      case Token::PERIOD: {
        impl()->RewriteNonPattern(CHECK_OK);
        BindingPatternUnexpectedToken();
        ArrowFormalParametersUnexpectedToken();
        Consume(Token::PERIOD);
        int pos = position();
        IdentifierT name = ParseIdentifierName(CHECK_OK);
        result = factory()->NewProperty(
            result, factory()->NewStringLiteral(name, pos), pos);
        impl()->PushLiteralName(name);
        break;
      }

      case Token::TEMPLATE_SPAN:
      case Token::TEMPLATE_TAIL: {
        impl()->RewriteNonPattern(CHECK_OK);
        BindingPatternUnexpectedToken();
        ArrowFormalParametersUnexpectedToken();
        result = ParseTemplateLiteral(result, position(), CHECK_OK);
        break;
      }

      default:
        return result;
    }
  }
}

template <typename Impl>
typename ParserBase<Impl>::ExpressionT
ParserBase<Impl>::ParseMemberWithNewPrefixesExpression(bool* is_async,
                                                       bool* ok) {
  // NewExpression ::
  //   ('new')+ MemberExpression
  //
  // NewTarget ::
  //   'new' '.' 'target'

  // The grammar for new expressions is pretty warped. We can have several 'new'
  // keywords following each other, and then a MemberExpression. When we see '('
  // after the MemberExpression, it's associated with the rightmost unassociated
  // 'new' to create a NewExpression with arguments. However, a NewExpression
  // can also occur without arguments.

  // Examples of new expression:
  // new foo.bar().baz means (new (foo.bar)()).baz
  // new foo()() means (new foo())()
  // new new foo()() means (new (new foo())())
  // new new foo means new (new foo)
  // new new foo() means new (new foo())
  // new new foo().bar().baz means (new (new foo()).bar()).baz

  if (peek() == Token::NEW) {
    BindingPatternUnexpectedToken();
    ArrowFormalParametersUnexpectedToken();
    Consume(Token::NEW);
    int new_pos = position();
    ExpressionT result;
    if (peek() == Token::SUPER) {
      const bool is_new = true;
      result = ParseSuperExpression(is_new, CHECK_OK);
    } else if (peek() == Token::PERIOD) {
      return ParseNewTargetExpression(CHECK_OK);
    } else {
      result = ParseMemberWithNewPrefixesExpression(is_async, CHECK_OK);
    }
    impl()->RewriteNonPattern(CHECK_OK);
    if (peek() == Token::LPAREN) {
      // NewExpression with arguments.
      Scanner::Location spread_pos;
      ExpressionListT args = ParseArguments(&spread_pos, CHECK_OK);

      if (spread_pos.IsValid()) {
        args = impl()->PrepareSpreadArguments(args);
        result = impl()->SpreadCallNew(result, args, new_pos);
      } else {
        result = factory()->NewCallNew(result, args, new_pos);
      }
      // The expression can still continue with . or [ after the arguments.
      result = ParseMemberExpressionContinuation(result, is_async, CHECK_OK);
      return result;
    }
    // NewExpression without arguments.
    return factory()->NewCallNew(result, impl()->NewExpressionList(0), new_pos);
  }
  // No 'new' or 'super' keyword.
  return ParseMemberExpression(is_async, ok);
}

template <typename Impl>
typename ParserBase<Impl>::ExpressionT ParserBase<Impl>::ParseMemberExpression(
    bool* is_async, bool* ok) {
  // MemberExpression ::
  //   (PrimaryExpression | FunctionLiteral | ClassLiteral)
  //     ('[' Expression ']' | '.' Identifier | Arguments | TemplateLiteral)*

  // The '[' Expression ']' and '.' Identifier parts are parsed by
  // ParseMemberExpressionContinuation, and the Arguments part is parsed by the
  // caller.

  // Parse the initial primary or function expression.
  ExpressionT result;
  if (peek() == Token::FUNCTION) {
    BindingPatternUnexpectedToken();
    ArrowFormalParametersUnexpectedToken();

    Consume(Token::FUNCTION);
    int function_token_position = position();

    if (allow_harmony_function_sent() && peek() == Token::PERIOD) {
      // function.sent
      int pos = position();
      ExpectMetaProperty(CStrVector("sent"), "function.sent", pos, CHECK_OK);

      if (!is_generator()) {
        // TODO(neis): allow escaping into closures?
        impl()->ReportMessageAt(scanner()->location(),
                                MessageTemplate::kUnexpectedFunctionSent);
        *ok = false;
        return impl()->EmptyExpression();
      }

      return impl()->FunctionSentExpression(pos);
    }

    FunctionKind function_kind = Check(Token::MUL)
                                     ? FunctionKind::kGeneratorFunction
                                     : FunctionKind::kNormalFunction;
    IdentifierT name = impl()->EmptyIdentifier();
    bool is_strict_reserved_name = false;
    Scanner::Location function_name_location = Scanner::Location::invalid();
    FunctionLiteral::FunctionType function_type =
        FunctionLiteral::kAnonymousExpression;
    if (peek_any_identifier()) {
      name = ParseIdentifierOrStrictReservedWord(
          function_kind, &is_strict_reserved_name, CHECK_OK);
      function_name_location = scanner()->location();
      function_type = FunctionLiteral::kNamedExpression;
    }
    result = impl()->ParseFunctionLiteral(
        name, function_name_location,
        is_strict_reserved_name ? kFunctionNameIsStrictReserved
                                : kFunctionNameValidityUnknown,
        function_kind, function_token_position, function_type, language_mode(),
        CHECK_OK);
  } else if (peek() == Token::SUPER) {
    const bool is_new = false;
    result = ParseSuperExpression(is_new, CHECK_OK);
  } else {
    result = ParsePrimaryExpression(is_async, CHECK_OK);
  }

  result = ParseMemberExpressionContinuation(result, is_async, CHECK_OK);
  return result;
}

template <typename Impl>
typename ParserBase<Impl>::ExpressionT ParserBase<Impl>::ParseSuperExpression(
    bool is_new, bool* ok) {
  Expect(Token::SUPER, CHECK_OK);
  int pos = position();

  DeclarationScope* scope = GetReceiverScope();
  FunctionKind kind = scope->function_kind();
  if (IsConciseMethod(kind) || IsAccessorFunction(kind) ||
      IsClassConstructor(kind)) {
    if (peek() == Token::PERIOD || peek() == Token::LBRACK) {
      scope->RecordSuperPropertyUsage();
      return impl()->NewSuperPropertyReference(pos);
    }
    // new super() is never allowed.
    // super() is only allowed in derived constructor
    if (!is_new && peek() == Token::LPAREN && IsSubclassConstructor(kind)) {
      // TODO(rossberg): This might not be the correct FunctionState for the
      // method here.
      return impl()->NewSuperCallReference(pos);
    }
  }

  impl()->ReportMessageAt(scanner()->location(),
                          MessageTemplate::kUnexpectedSuper);
  *ok = false;
  return impl()->EmptyExpression();
}

template <typename Impl>
void ParserBase<Impl>::ExpectMetaProperty(Vector<const char> property_name,
                                          const char* full_name, int pos,
                                          bool* ok) {
  Consume(Token::PERIOD);
  ExpectContextualKeyword(property_name, CHECK_OK_CUSTOM(Void));
  if (scanner()->literal_contains_escapes()) {
    impl()->ReportMessageAt(
        Scanner::Location(pos, scanner()->location().end_pos),
        MessageTemplate::kInvalidEscapedMetaProperty, full_name);
    *ok = false;
  }
}

template <typename Impl>
typename ParserBase<Impl>::ExpressionT
ParserBase<Impl>::ParseNewTargetExpression(bool* ok) {
  int pos = position();
  ExpectMetaProperty(CStrVector("target"), "new.target", pos, CHECK_OK);

  if (!GetReceiverScope()->is_function_scope()) {
    impl()->ReportMessageAt(scanner()->location(),
                            MessageTemplate::kUnexpectedNewTarget);
    *ok = false;
    return impl()->EmptyExpression();
  }

  return impl()->NewTargetExpression(pos);
}

template <typename Impl>
typename ParserBase<Impl>::ExpressionT
ParserBase<Impl>::ParseMemberExpressionContinuation(ExpressionT expression,
                                                    bool* is_async, bool* ok) {
  // Parses this part of MemberExpression:
  // ('[' Expression ']' | '.' Identifier | TemplateLiteral)*
  while (true) {
    switch (peek()) {
      case Token::LBRACK: {
        *is_async = false;
        impl()->RewriteNonPattern(CHECK_OK);
        BindingPatternUnexpectedToken();
        ArrowFormalParametersUnexpectedToken();

        Consume(Token::LBRACK);
        int pos = position();
        ExpressionT index = ParseExpressionCoverGrammar(true, CHECK_OK);
        impl()->RewriteNonPattern(CHECK_OK);
        expression = factory()->NewProperty(expression, index, pos);
        impl()->PushPropertyName(index);
        Expect(Token::RBRACK, CHECK_OK);
        break;
      }
      case Token::PERIOD: {
        *is_async = false;
        impl()->RewriteNonPattern(CHECK_OK);
        BindingPatternUnexpectedToken();
        ArrowFormalParametersUnexpectedToken();

        Consume(Token::PERIOD);
        int pos = position();
        IdentifierT name = ParseIdentifierName(CHECK_OK);
        expression = factory()->NewProperty(
            expression, factory()->NewStringLiteral(name, pos), pos);
        impl()->PushLiteralName(name);
        break;
      }
      case Token::TEMPLATE_SPAN:
      case Token::TEMPLATE_TAIL: {
        *is_async = false;
        impl()->RewriteNonPattern(CHECK_OK);
        BindingPatternUnexpectedToken();
        ArrowFormalParametersUnexpectedToken();
        int pos;
        if (scanner()->current_token() == Token::IDENTIFIER) {
          pos = position();
        } else {
          pos = peek_position();
          if (expression->IsFunctionLiteral() && mode() == PARSE_EAGERLY) {
            // If the tag function looks like an IIFE, set_parenthesized() to
            // force eager compilation.
            expression->AsFunctionLiteral()->set_should_eager_compile();
          }
        }
        expression = ParseTemplateLiteral(expression, pos, CHECK_OK);
        break;
      }
      case Token::ILLEGAL: {
        ReportUnexpectedTokenAt(scanner()->peek_location(), Token::ILLEGAL);
        *ok = false;
        return impl()->EmptyExpression();
      }
      default:
        return expression;
    }
  }
  DCHECK(false);
  return impl()->EmptyExpression();
}

template <typename Impl>
void ParserBase<Impl>::ParseFormalParameter(FormalParametersT* parameters,
                                            bool* ok) {
  // FormalParameter[Yield,GeneratorParameter] :
  //   BindingElement[?Yield, ?GeneratorParameter]
  bool is_rest = parameters->has_rest;

  ExpressionT pattern = ParsePrimaryExpression(CHECK_OK_CUSTOM(Void));
  ValidateBindingPattern(CHECK_OK_CUSTOM(Void));

  if (!impl()->IsIdentifier(pattern)) {
    parameters->is_simple = false;
    ValidateFormalParameterInitializer(CHECK_OK_CUSTOM(Void));
    classifier()->RecordNonSimpleParameter();
  }

  ExpressionT initializer = impl()->EmptyExpression();
  if (!is_rest && Check(Token::ASSIGN)) {
    ExpressionClassifier init_classifier(this);
    initializer = ParseAssignmentExpression(true, CHECK_OK_CUSTOM(Void));
    impl()->RewriteNonPattern(CHECK_OK_CUSTOM(Void));
    ValidateFormalParameterInitializer(CHECK_OK_CUSTOM(Void));
    parameters->is_simple = false;
    impl()->Discard();
    classifier()->RecordNonSimpleParameter();

    impl()->SetFunctionNameFromIdentifierRef(initializer, pattern);
  }

  impl()->AddFormalParameter(parameters, pattern, initializer,
                             scanner()->location().end_pos, is_rest);
}

template <typename Impl>
void ParserBase<Impl>::ParseFormalParameterList(FormalParametersT* parameters,
                                                bool* ok) {
  // FormalParameters[Yield] :
  //   [empty]
  //   FunctionRestParameter[?Yield]
  //   FormalParameterList[?Yield]
  //   FormalParameterList[?Yield] ,
  //   FormalParameterList[?Yield] , FunctionRestParameter[?Yield]
  //
  // FormalParameterList[Yield] :
  //   FormalParameter[?Yield]
  //   FormalParameterList[?Yield] , FormalParameter[?Yield]

  DCHECK_EQ(0, parameters->Arity());

  if (peek() != Token::RPAREN) {
    while (true) {
      if (parameters->Arity() > Code::kMaxArguments) {
        ReportMessage(MessageTemplate::kTooManyParameters);
        *ok = false;
        return;
      }
      parameters->has_rest = Check(Token::ELLIPSIS);
      ParseFormalParameter(parameters, CHECK_OK_CUSTOM(Void));

      if (parameters->has_rest) {
        parameters->is_simple = false;
        classifier()->RecordNonSimpleParameter();
        if (peek() == Token::COMMA) {
          impl()->ReportMessageAt(scanner()->peek_location(),
                                  MessageTemplate::kParamAfterRest);
          *ok = false;
          return;
        }
        break;
      }
      if (!Check(Token::COMMA)) break;
      if (allow_harmony_trailing_commas() && peek() == Token::RPAREN) {
        // allow the trailing comma
        break;
      }
    }
  }

  for (int i = 0; i < parameters->Arity(); ++i) {
    auto parameter = parameters->at(i);
    impl()->DeclareFormalParameter(parameters->scope, parameter);
  }
}

template <typename Impl>
typename ParserBase<Impl>::BlockT ParserBase<Impl>::ParseVariableDeclarations(
    VariableDeclarationContext var_context,
    DeclarationParsingResult* parsing_result,
    ZoneList<const AstRawString*>* names, bool* ok) {
  // VariableDeclarations ::
  //   ('var' | 'const' | 'let') (Identifier ('=' AssignmentExpression)?)+[',']
  //
  // ES6:
  // FIXME(marja, nikolaos): Add an up-to-date comment about ES6 variable
  // declaration syntax.

  DCHECK_NOT_NULL(parsing_result);
  parsing_result->descriptor.declaration_kind = DeclarationDescriptor::NORMAL;
  parsing_result->descriptor.declaration_pos = peek_position();
  parsing_result->descriptor.initialization_pos = peek_position();

  BlockT init_block = impl()->NullBlock();
  if (var_context != kForStatement) {
    init_block = factory()->NewBlock(
        nullptr, 1, true, parsing_result->descriptor.declaration_pos);
  }

  switch (peek()) {
    case Token::VAR:
      parsing_result->descriptor.mode = VAR;
      Consume(Token::VAR);
      break;
    case Token::CONST:
      Consume(Token::CONST);
      DCHECK(var_context != kStatement);
      parsing_result->descriptor.mode = CONST;
      break;
    case Token::LET:
      Consume(Token::LET);
      DCHECK(var_context != kStatement);
      parsing_result->descriptor.mode = LET;
      break;
    default:
      UNREACHABLE();  // by current callers
      break;
  }

  parsing_result->descriptor.scope = scope();
  parsing_result->descriptor.hoist_scope = nullptr;

  // The scope of a var/const declared variable anywhere inside a function
  // is the entire function (ECMA-262, 3rd, 10.1.3, and 12.2). The scope
  // of a let declared variable is the scope of the immediately enclosing
  // block.
  int bindings_start = peek_position();
  do {
    // Parse binding pattern.
    FuncNameInferrer::State fni_state(fni_);

    ExpressionT pattern = impl()->EmptyExpression();
    int decl_pos = peek_position();
    {
      ExpressionClassifier pattern_classifier(this);
      pattern = ParsePrimaryExpression(CHECK_OK_CUSTOM(NullBlock));

      ValidateBindingPattern(CHECK_OK_CUSTOM(NullBlock));
      if (IsLexicalVariableMode(parsing_result->descriptor.mode)) {
        ValidateLetPattern(CHECK_OK_CUSTOM(NullBlock));
      }
    }

    Scanner::Location variable_loc = scanner()->location();
    bool single_name = impl()->IsIdentifier(pattern);

    if (single_name) {
      impl()->PushVariableName(impl()->AsIdentifier(pattern));
    }

    ExpressionT value = impl()->EmptyExpression();
    int initializer_position = kNoSourcePosition;
    if (Check(Token::ASSIGN)) {
      ExpressionClassifier classifier(this);
      value = ParseAssignmentExpression(var_context != kForStatement,
                                        CHECK_OK_CUSTOM(NullBlock));
      impl()->RewriteNonPattern(CHECK_OK_CUSTOM(NullBlock));
      variable_loc.end_pos = scanner()->location().end_pos;

      if (!parsing_result->first_initializer_loc.IsValid()) {
        parsing_result->first_initializer_loc = variable_loc;
      }

      // Don't infer if it is "a = function(){...}();"-like expression.
      if (single_name && fni_ != nullptr) {
        if (!value->IsCall() && !value->IsCallNew()) {
          fni_->Infer();
        } else {
          fni_->RemoveLastFunction();
        }
      }

      impl()->SetFunctionNameFromIdentifierRef(value, pattern);

      // End position of the initializer is after the assignment expression.
      initializer_position = scanner()->location().end_pos;
    } else {
      if (var_context != kForStatement || !PeekInOrOf()) {
        // ES6 'const' and binding patterns require initializers.
        if (parsing_result->descriptor.mode == CONST ||
            !impl()->IsIdentifier(pattern)) {
          impl()->ReportMessageAt(
              Scanner::Location(decl_pos, scanner()->location().end_pos),
              MessageTemplate::kDeclarationMissingInitializer,
              !impl()->IsIdentifier(pattern) ? "destructuring" : "const");
          *ok = false;
          return impl()->NullBlock();
        }
        // 'let x' initializes 'x' to undefined.
        if (parsing_result->descriptor.mode == LET) {
          value = impl()->GetLiteralUndefined(position());
        }
      }

      // End position of the initializer is after the variable.
      initializer_position = position();
    }

    typename DeclarationParsingResult::Declaration decl(
        pattern, initializer_position, value);
    if (var_context == kForStatement) {
      // Save the declaration for further handling in ParseForStatement.
      parsing_result->declarations.Add(decl);
    } else {
      // Immediately declare the variable otherwise. This avoids O(N^2)
      // behavior (where N is the number of variables in a single
      // declaration) in the PatternRewriter having to do with removing
      // and adding VariableProxies to the Scope (see bug 4699).
      impl()->DeclareAndInitializeVariables(init_block,
                                            &parsing_result->descriptor, &decl,
                                            names, CHECK_OK_CUSTOM(NullBlock));
    }
  } while (Check(Token::COMMA));

  parsing_result->bindings_loc =
      Scanner::Location(bindings_start, scanner()->location().end_pos);

  DCHECK(*ok);
  return init_block;
}

template <typename Impl>
typename ParserBase<Impl>::StatementT
ParserBase<Impl>::ParseFunctionDeclaration(bool* ok) {
  Consume(Token::FUNCTION);
  int pos = position();
  ParseFunctionFlags flags = ParseFunctionFlags::kIsNormal;
  if (Check(Token::MUL)) {
    flags |= ParseFunctionFlags::kIsGenerator;
    if (allow_harmony_restrictive_declarations()) {
      impl()->ReportMessageAt(scanner()->location(),
                              MessageTemplate::kGeneratorInLegacyContext);
      *ok = false;
      return impl()->NullStatement();
    }
  }
  return ParseHoistableDeclaration(pos, flags, nullptr, false, ok);
}

template <typename Impl>
typename ParserBase<Impl>::StatementT
ParserBase<Impl>::ParseHoistableDeclaration(
    ZoneList<const AstRawString*>* names, bool default_export, bool* ok) {
  Expect(Token::FUNCTION, CHECK_OK_CUSTOM(NullStatement));
  int pos = position();
  ParseFunctionFlags flags = ParseFunctionFlags::kIsNormal;
  if (Check(Token::MUL)) {
    flags |= ParseFunctionFlags::kIsGenerator;
  }
  return ParseHoistableDeclaration(pos, flags, names, default_export, ok);
}

template <typename Impl>
typename ParserBase<Impl>::StatementT
ParserBase<Impl>::ParseHoistableDeclaration(
    int pos, ParseFunctionFlags flags, ZoneList<const AstRawString*>* names,
    bool default_export, bool* ok) {
  // FunctionDeclaration ::
  //   'function' Identifier '(' FormalParameters ')' '{' FunctionBody '}'
  //   'function' '(' FormalParameters ')' '{' FunctionBody '}'
  // GeneratorDeclaration ::
  //   'function' '*' Identifier '(' FormalParameters ')' '{' FunctionBody '}'
  //   'function' '*' '(' FormalParameters ')' '{' FunctionBody '}'
  //
  // The anonymous forms are allowed iff [default_export] is true.
  //
  // 'function' and '*' (if present) have been consumed by the caller.

  const bool is_generator = flags & ParseFunctionFlags::kIsGenerator;
  const bool is_async = flags & ParseFunctionFlags::kIsAsync;
  DCHECK(!is_generator || !is_async);

  IdentifierT name;
  FunctionNameValidity name_validity;
  IdentifierT variable_name;
  if (default_export && peek() == Token::LPAREN) {
    impl()->GetDefaultStrings(&name, &variable_name);
    name_validity = kSkipFunctionNameCheck;
  } else {
    bool is_strict_reserved;
    name = ParseIdentifierOrStrictReservedWord(&is_strict_reserved,
                                               CHECK_OK_CUSTOM(NullStatement));
    name_validity = is_strict_reserved ? kFunctionNameIsStrictReserved
                                       : kFunctionNameValidityUnknown;
    variable_name = name;
  }

  FuncNameInferrer::State fni_state(fni_);
  impl()->PushEnclosingName(name);
  FunctionLiteralT function = impl()->ParseFunctionLiteral(
      name, scanner()->location(), name_validity,
      is_generator ? FunctionKind::kGeneratorFunction
                   : is_async ? FunctionKind::kAsyncFunction
                              : FunctionKind::kNormalFunction,
      pos, FunctionLiteral::kDeclaration, language_mode(),
      CHECK_OK_CUSTOM(NullStatement));

  return impl()->DeclareFunction(variable_name, function, pos, is_generator,
                                 is_async, names, ok);
}

template <typename Impl>
typename ParserBase<Impl>::StatementT ParserBase<Impl>::ParseClassDeclaration(
    ZoneList<const AstRawString*>* names, bool default_export, bool* ok) {
  // ClassDeclaration ::
  //   'class' Identifier ('extends' LeftHandExpression)? '{' ClassBody '}'
  //   'class' ('extends' LeftHandExpression)? '{' ClassBody '}'
  //
  // The anonymous form is allowed iff [default_export] is true.
  //
  // 'class' is expected to be consumed by the caller.
  //
  // A ClassDeclaration
  //
  //   class C { ... }
  //
  // has the same semantics as:
  //
  //   let C = class C { ... };
  //
  // so rewrite it as such.

  int class_token_pos = position();
  IdentifierT name = impl()->EmptyIdentifier();
  bool is_strict_reserved = false;
  IdentifierT variable_name = impl()->EmptyIdentifier();
  if (default_export && (peek() == Token::EXTENDS || peek() == Token::LBRACE)) {
    impl()->GetDefaultStrings(&name, &variable_name);
  } else {
    name = ParseIdentifierOrStrictReservedWord(&is_strict_reserved,
                                               CHECK_OK_CUSTOM(NullStatement));
    variable_name = name;
  }

  ExpressionClassifier no_classifier(this);
  ExpressionT value =
      ParseClassLiteral(name, scanner()->location(), is_strict_reserved,
                        class_token_pos, CHECK_OK_CUSTOM(NullStatement));
  int end_pos = position();
  return impl()->DeclareClass(variable_name, value, names, class_token_pos,
                              end_pos, ok);
}

// Language extension which is only enabled for source files loaded
// through the API's extension mechanism.  A native function
// declaration is resolved by looking up the function through a
// callback provided by the extension.
template <typename Impl>
typename ParserBase<Impl>::StatementT ParserBase<Impl>::ParseNativeDeclaration(
    bool* ok) {
  int pos = peek_position();
  Expect(Token::FUNCTION, CHECK_OK_CUSTOM(NullStatement));
  // Allow "eval" or "arguments" for backward compatibility.
  IdentifierT name = ParseIdentifier(kAllowRestrictedIdentifiers,
                                     CHECK_OK_CUSTOM(NullStatement));
  Expect(Token::LPAREN, CHECK_OK_CUSTOM(NullStatement));
  if (peek() != Token::RPAREN) {
    do {
      ParseIdentifier(kAllowRestrictedIdentifiers,
                      CHECK_OK_CUSTOM(NullStatement));
    } while (Check(Token::COMMA));
  }
  Expect(Token::RPAREN, CHECK_OK_CUSTOM(NullStatement));
  Expect(Token::SEMICOLON, CHECK_OK_CUSTOM(NullStatement));
  return impl()->DeclareNative(name, pos, ok);
}

template <typename Impl>
typename ParserBase<Impl>::StatementT
ParserBase<Impl>::ParseAsyncFunctionDeclaration(
    ZoneList<const AstRawString*>* names, bool default_export, bool* ok) {
  // AsyncFunctionDeclaration ::
  //   async [no LineTerminator here] function BindingIdentifier[Await]
  //       ( FormalParameters[Await] ) { AsyncFunctionBody }
  DCHECK_EQ(scanner()->current_token(), Token::ASYNC);
  int pos = position();
  if (scanner()->HasAnyLineTerminatorBeforeNext()) {
    *ok = false;
    impl()->ReportUnexpectedToken(scanner()->current_token());
    return impl()->NullStatement();
  }
  Expect(Token::FUNCTION, CHECK_OK_CUSTOM(NullStatement));
  ParseFunctionFlags flags = ParseFunctionFlags::kIsAsync;
  return ParseHoistableDeclaration(pos, flags, names, default_export, ok);
}

template <typename Impl>
void ParserBase<Impl>::CheckArityRestrictions(int param_count,
                                              FunctionKind function_kind,
                                              bool has_rest,
                                              int formals_start_pos,
                                              int formals_end_pos, bool* ok) {
  if (IsGetterFunction(function_kind)) {
    if (param_count != 0) {
      impl()->ReportMessageAt(
          Scanner::Location(formals_start_pos, formals_end_pos),
          MessageTemplate::kBadGetterArity);
      *ok = false;
    }
  } else if (IsSetterFunction(function_kind)) {
    if (param_count != 1) {
      impl()->ReportMessageAt(
          Scanner::Location(formals_start_pos, formals_end_pos),
          MessageTemplate::kBadSetterArity);
      *ok = false;
    }
    if (has_rest) {
      impl()->ReportMessageAt(
          Scanner::Location(formals_start_pos, formals_end_pos),
          MessageTemplate::kBadSetterRestParameter);
      *ok = false;
    }
  }
}

template <typename Impl>
bool ParserBase<Impl>::IsNextLetKeyword() {
  DCHECK(peek() == Token::LET);
  Token::Value next_next = PeekAhead();
  switch (next_next) {
    case Token::LBRACE:
    case Token::LBRACK:
    case Token::IDENTIFIER:
    case Token::STATIC:
    case Token::LET:  // `let let;` is disallowed by static semantics, but the
                      // token must be first interpreted as a keyword in order
                      // for those semantics to apply. This ensures that ASI is
                      // not honored when a LineTerminator separates the
                      // tokens.
    case Token::YIELD:
    case Token::AWAIT:
    case Token::ASYNC:
      return true;
    case Token::FUTURE_STRICT_RESERVED_WORD:
      return is_sloppy(language_mode());
    default:
      return false;
  }
}

template <typename Impl>
bool ParserBase<Impl>::IsTrivialExpression() {
  Token::Value peek_token = peek();
  if (peek_token == Token::SMI || peek_token == Token::NUMBER ||
      peek_token == Token::NULL_LITERAL || peek_token == Token::TRUE_LITERAL ||
      peek_token == Token::FALSE_LITERAL || peek_token == Token::STRING ||
      peek_token == Token::IDENTIFIER || peek_token == Token::THIS) {
    // PeekAhead() is expensive & may not always be called, so we only call it
    // after checking peek().
    Token::Value peek_ahead = PeekAhead();
    if (peek_ahead == Token::COMMA || peek_ahead == Token::RPAREN ||
        peek_ahead == Token::SEMICOLON || peek_ahead == Token::RBRACK) {
      return true;
    }
  }
  return false;
}

template <typename Impl>
typename ParserBase<Impl>::ExpressionT
ParserBase<Impl>::ParseArrowFunctionLiteral(
    bool accept_IN, const FormalParametersT& formal_parameters, bool* ok) {
  if (peek() == Token::ARROW && scanner_->HasAnyLineTerminatorBeforeNext()) {
    // ASI inserts `;` after arrow parameters if a line terminator is found.
    // `=> ...` is never a valid expression, so report as syntax error.
    // If next token is not `=>`, it's a syntax error anyways.
    ReportUnexpectedTokenAt(scanner_->peek_location(), Token::ARROW);
    *ok = false;
    return impl()->EmptyExpression();
  }

  StatementListT body = impl()->NullStatementList();
  int num_parameters = formal_parameters.scope->num_parameters();
  int materialized_literal_count = -1;
  int expected_property_count = -1;

  FunctionKind kind = formal_parameters.scope->function_kind();
  FunctionLiteral::EagerCompileHint eager_compile_hint =
      FunctionLiteral::kShouldLazyCompile;
  bool should_be_used_once_hint = false;
  {
    FunctionState function_state(&function_state_, &scope_state_,
                                 formal_parameters.scope);

    function_state.SkipMaterializedLiterals(
        formal_parameters.materialized_literals_count);

    impl()->ReindexLiterals(formal_parameters);

    Expect(Token::ARROW, CHECK_OK);

    if (peek() == Token::LBRACE) {
      // Multiple statement body
      Consume(Token::LBRACE);
      DCHECK_EQ(scope(), formal_parameters.scope);
      bool is_lazily_parsed =
          (mode() == PARSE_LAZILY &&
           formal_parameters.scope
               ->AllowsLazyParsingWithoutUnresolvedVariables());
      // TODO(marja): consider lazy-parsing inner arrow functions too. is_this
      // handling in Scope::ResolveVariable needs to change.
      if (is_lazily_parsed) {
        Scanner::BookmarkScope bookmark(scanner());
        bookmark.Set();
        LazyParsingResult result = impl()->SkipLazyFunctionBody(
            &materialized_literal_count, &expected_property_count, false, true,
            CHECK_OK);
        formal_parameters.scope->ResetAfterPreparsing(
            ast_value_factory_, result == kLazyParsingAborted);

        if (formal_parameters.materialized_literals_count > 0) {
          materialized_literal_count +=
              formal_parameters.materialized_literals_count;
        }

        if (result == kLazyParsingAborted) {
          bookmark.Apply();
          // Trigger eager (re-)parsing, just below this block.
          is_lazily_parsed = false;

          // This is probably an initialization function. Inform the compiler it
          // should also eager-compile this function, and that we expect it to
          // be used once.
          eager_compile_hint = FunctionLiteral::kShouldEagerCompile;
          should_be_used_once_hint = true;
        }
      }
      if (!is_lazily_parsed) {
        body = impl()->ParseEagerFunctionBody(
            impl()->EmptyIdentifier(), kNoSourcePosition, formal_parameters,
            kind, FunctionLiteral::kAnonymousExpression, CHECK_OK);
        materialized_literal_count =
            function_state.materialized_literal_count();
        expected_property_count = function_state.expected_property_count();
      }
    } else {
      // Single-expression body
      int pos = position();
      DCHECK(ReturnExprContext::kInsideValidBlock ==
             function_state_->return_expr_context());
      ReturnExprScope allow_tail_calls(
          function_state_, ReturnExprContext::kInsideValidReturnStatement);
      body = impl()->NewStatementList(1);
      impl()->AddParameterInitializationBlock(
          formal_parameters, body, kind == kAsyncArrowFunction, CHECK_OK);
      ExpressionClassifier classifier(this);
      if (kind == kAsyncArrowFunction) {
        ParseAsyncFunctionBody(scope(), body, kAsyncArrowFunction,
                               FunctionBodyType::kSingleExpression, accept_IN,
                               pos, CHECK_OK);
        impl()->RewriteNonPattern(CHECK_OK);
      } else {
        ExpressionT expression = ParseAssignmentExpression(accept_IN, CHECK_OK);
        impl()->RewriteNonPattern(CHECK_OK);
        body->Add(factory()->NewReturnStatement(expression, pos), zone());
        if (allow_tailcalls() && !is_sloppy(language_mode())) {
          // ES6 14.6.1 Static Semantics: IsInTailPosition
          impl()->MarkTailPosition(expression);
        }
      }
      materialized_literal_count = function_state.materialized_literal_count();
      expected_property_count = function_state.expected_property_count();
      impl()->MarkCollectedTailCallExpressions();
    }

    formal_parameters.scope->set_end_position(scanner()->location().end_pos);

    // Arrow function formal parameters are parsed as StrictFormalParameterList,
    // which is not the same as "parameters of a strict function"; it only means
    // that duplicates are not allowed.  Of course, the arrow function may
    // itself be strict as well.
    const bool allow_duplicate_parameters = false;
    ValidateFormalParameters(language_mode(), allow_duplicate_parameters,
                             CHECK_OK);

    // Validate strict mode.
    if (is_strict(language_mode())) {
      CheckStrictOctalLiteral(formal_parameters.scope->start_position(),
                              scanner()->location().end_pos, CHECK_OK);
    }
    impl()->CheckConflictingVarDeclarations(formal_parameters.scope, CHECK_OK);

    impl()->RewriteDestructuringAssignments();
  }

  FunctionLiteralT function_literal = factory()->NewFunctionLiteral(
      impl()->EmptyIdentifierString(), formal_parameters.scope, body,
      materialized_literal_count, expected_property_count, num_parameters,
      FunctionLiteral::kNoDuplicateParameters,
      FunctionLiteral::kAnonymousExpression, eager_compile_hint,
      formal_parameters.scope->start_position());

  function_literal->set_function_token_position(
      formal_parameters.scope->start_position());
  if (should_be_used_once_hint) {
    function_literal->set_should_be_used_once_hint();
  }

  impl()->AddFunctionForNameInference(function_literal);

  return function_literal;
}

template <typename Impl>
typename ParserBase<Impl>::ExpressionT ParserBase<Impl>::ParseClassLiteral(
    IdentifierT name, Scanner::Location class_name_location,
    bool name_is_strict_reserved, int class_token_pos, bool* ok) {
  // All parts of a ClassDeclaration and ClassExpression are strict code.
  if (name_is_strict_reserved) {
    impl()->ReportMessageAt(class_name_location,
                            MessageTemplate::kUnexpectedStrictReserved);
    *ok = false;
    return impl()->EmptyExpression();
  }
  if (impl()->IsEvalOrArguments(name)) {
    impl()->ReportMessageAt(class_name_location,
                            MessageTemplate::kStrictEvalArguments);
    *ok = false;
    return impl()->EmptyExpression();
  }

  BlockState block_state(zone(), &scope_state_);
  RaiseLanguageMode(STRICT);

  ClassInfo class_info(this);
  impl()->DeclareClassVariable(name, block_state.scope(), &class_info,
                               class_token_pos, CHECK_OK);

  if (Check(Token::EXTENDS)) {
    block_state.set_start_position(scanner()->location().end_pos);
    ExpressionClassifier extends_classifier(this);
    class_info.extends = ParseLeftHandSideExpression(CHECK_OK);
    impl()->RewriteNonPattern(CHECK_OK);
    impl()->AccumulateFormalParameterContainmentErrors();
  } else {
    block_state.set_start_position(scanner()->location().end_pos);
  }

  ClassLiteralChecker checker(this);

  Expect(Token::LBRACE, CHECK_OK);

  const bool has_extends = !impl()->IsEmptyExpression(class_info.extends);
  while (peek() != Token::RBRACE) {
    if (Check(Token::SEMICOLON)) continue;
    FuncNameInferrer::State fni_state(fni_);
    bool is_computed_name = false;  // Classes do not care about computed
                                    // property names here.
    ExpressionClassifier property_classifier(this);
    ClassLiteralPropertyT property = ParseClassPropertyDefinition(
        &checker, has_extends, &is_computed_name,
        &class_info.has_seen_constructor, CHECK_OK);
    impl()->RewriteNonPattern(CHECK_OK);
    impl()->AccumulateFormalParameterContainmentErrors();

    impl()->DeclareClassProperty(name, property, &class_info, CHECK_OK);
    impl()->InferFunctionName();
  }

  Expect(Token::RBRACE, CHECK_OK);
  return impl()->RewriteClassLiteral(name, &class_info, class_token_pos, ok);
}

template <typename Impl>
void ParserBase<Impl>::ParseAsyncFunctionBody(Scope* scope, StatementListT body,
                                              FunctionKind kind,
                                              FunctionBodyType body_type,
                                              bool accept_IN, int pos,
                                              bool* ok) {
  scope->ForceContextAllocation();

  impl()->PrepareAsyncFunctionBody(body, kind, pos);

  BlockT block = factory()->NewBlock(nullptr, 8, true, kNoSourcePosition);

  ExpressionT return_value = impl()->EmptyExpression();
  if (body_type == FunctionBodyType::kNormal) {
    ParseStatementList(block->statements(), Token::RBRACE,
                       CHECK_OK_CUSTOM(Void));
    return_value = factory()->NewUndefinedLiteral(kNoSourcePosition);
  } else {
    return_value = ParseAssignmentExpression(accept_IN, CHECK_OK_CUSTOM(Void));
    impl()->RewriteNonPattern(CHECK_OK_CUSTOM(Void));
  }

  impl()->RewriteAsyncFunctionBody(body, block, return_value,
                                   CHECK_OK_CUSTOM(Void));
  scope->set_end_position(scanner()->location().end_pos);
}

template <typename Impl>
typename ParserBase<Impl>::ExpressionT
ParserBase<Impl>::ParseAsyncFunctionLiteral(bool* ok) {
  // AsyncFunctionLiteral ::
  //   async [no LineTerminator here] function ( FormalParameters[Await] )
  //       { AsyncFunctionBody }
  //
  //   async [no LineTerminator here] function BindingIdentifier[Await]
  //       ( FormalParameters[Await] ) { AsyncFunctionBody }
  DCHECK_EQ(scanner()->current_token(), Token::ASYNC);
  int pos = position();
  Expect(Token::FUNCTION, CHECK_OK);
  bool is_strict_reserved = false;
  IdentifierT name = impl()->EmptyIdentifier();
  FunctionLiteral::FunctionType type = FunctionLiteral::kAnonymousExpression;

  if (peek_any_identifier()) {
    type = FunctionLiteral::kNamedExpression;
    name = ParseIdentifierOrStrictReservedWord(FunctionKind::kAsyncFunction,
                                               &is_strict_reserved, CHECK_OK);
  }
  return impl()->ParseFunctionLiteral(
      name, scanner()->location(),
      is_strict_reserved ? kFunctionNameIsStrictReserved
                         : kFunctionNameValidityUnknown,
      FunctionKind::kAsyncFunction, pos, type, language_mode(), CHECK_OK);
}

template <typename Impl>
typename ParserBase<Impl>::ExpressionT ParserBase<Impl>::ParseTemplateLiteral(
    ExpressionT tag, int start, bool* ok) {
  // A TemplateLiteral is made up of 0 or more TEMPLATE_SPAN tokens (literal
  // text followed by a substitution expression), finalized by a single
  // TEMPLATE_TAIL.
  //
  // In terms of draft language, TEMPLATE_SPAN may be either the TemplateHead or
  // TemplateMiddle productions, while TEMPLATE_TAIL is either TemplateTail, or
  // NoSubstitutionTemplate.
  //
  // When parsing a TemplateLiteral, we must have scanned either an initial
  // TEMPLATE_SPAN, or a TEMPLATE_TAIL.
  CHECK(peek() == Token::TEMPLATE_SPAN || peek() == Token::TEMPLATE_TAIL);

  // If we reach a TEMPLATE_TAIL first, we are parsing a NoSubstitutionTemplate.
  // In this case we may simply consume the token and build a template with a
  // single TEMPLATE_SPAN and no expressions.
  if (peek() == Token::TEMPLATE_TAIL) {
    Consume(Token::TEMPLATE_TAIL);
    int pos = position();
    CheckTemplateOctalLiteral(pos, peek_position(), CHECK_OK);
    typename Impl::TemplateLiteralState ts = impl()->OpenTemplateLiteral(pos);
    impl()->AddTemplateSpan(&ts, true);
    return impl()->CloseTemplateLiteral(&ts, start, tag);
  }

  Consume(Token::TEMPLATE_SPAN);
  int pos = position();
  typename Impl::TemplateLiteralState ts = impl()->OpenTemplateLiteral(pos);
  impl()->AddTemplateSpan(&ts, false);
  Token::Value next;

  // If we open with a TEMPLATE_SPAN, we must scan the subsequent expression,
  // and repeat if the following token is a TEMPLATE_SPAN as well (in this
  // case, representing a TemplateMiddle).

  do {
    CheckTemplateOctalLiteral(pos, peek_position(), CHECK_OK);
    next = peek();
    if (next == Token::EOS) {
      impl()->ReportMessageAt(Scanner::Location(start, peek_position()),
                              MessageTemplate::kUnterminatedTemplate);
      *ok = false;
      return impl()->EmptyExpression();
    } else if (next == Token::ILLEGAL) {
      impl()->ReportMessageAt(
          Scanner::Location(position() + 1, peek_position()),
          MessageTemplate::kUnexpectedToken, "ILLEGAL", kSyntaxError);
      *ok = false;
      return impl()->EmptyExpression();
    }

    int expr_pos = peek_position();
    ExpressionT expression = ParseExpressionCoverGrammar(true, CHECK_OK);
    impl()->RewriteNonPattern(CHECK_OK);
    impl()->AddTemplateExpression(&ts, expression);

    if (peek() != Token::RBRACE) {
      impl()->ReportMessageAt(Scanner::Location(expr_pos, peek_position()),
                              MessageTemplate::kUnterminatedTemplateExpr);
      *ok = false;
      return impl()->EmptyExpression();
    }

    // If we didn't die parsing that expression, our next token should be a
    // TEMPLATE_SPAN or TEMPLATE_TAIL.
    next = scanner()->ScanTemplateContinuation();
    Next();
    pos = position();

    if (next == Token::EOS) {
      impl()->ReportMessageAt(Scanner::Location(start, pos),
                              MessageTemplate::kUnterminatedTemplate);
      *ok = false;
      return impl()->EmptyExpression();
    } else if (next == Token::ILLEGAL) {
      impl()->ReportMessageAt(
          Scanner::Location(position() + 1, peek_position()),
          MessageTemplate::kUnexpectedToken, "ILLEGAL", kSyntaxError);
      *ok = false;
      return impl()->EmptyExpression();
    }

    impl()->AddTemplateSpan(&ts, next == Token::TEMPLATE_TAIL);
  } while (next == Token::TEMPLATE_SPAN);

  DCHECK_EQ(next, Token::TEMPLATE_TAIL);
  CheckTemplateOctalLiteral(pos, peek_position(), CHECK_OK);
  // Once we've reached a TEMPLATE_TAIL, we can close the TemplateLiteral.
  return impl()->CloseTemplateLiteral(&ts, start, tag);
}

template <typename Impl>
typename ParserBase<Impl>::ExpressionT
ParserBase<Impl>::CheckAndRewriteReferenceExpression(
    ExpressionT expression, int beg_pos, int end_pos,
    MessageTemplate::Template message, bool* ok) {
  return CheckAndRewriteReferenceExpression(expression, beg_pos, end_pos,
                                            message, kReferenceError, ok);
}

template <typename Impl>
typename ParserBase<Impl>::ExpressionT
ParserBase<Impl>::CheckAndRewriteReferenceExpression(
    ExpressionT expression, int beg_pos, int end_pos,
    MessageTemplate::Template message, ParseErrorType type, bool* ok) {
  if (impl()->IsIdentifier(expression) && is_strict(language_mode()) &&
      impl()->IsEvalOrArguments(impl()->AsIdentifier(expression))) {
    ReportMessageAt(Scanner::Location(beg_pos, end_pos),
                    MessageTemplate::kStrictEvalArguments, kSyntaxError);
    *ok = false;
    return impl()->EmptyExpression();
  }
  if (expression->IsValidReferenceExpression()) {
    return expression;
  }
  if (expression->IsCall()) {
    // If it is a call, make it a runtime error for legacy web compatibility.
    // Rewrite `expr' to `expr[throw ReferenceError]'.
    ExpressionT error = impl()->NewThrowReferenceError(message, beg_pos);
    return factory()->NewProperty(expression, error, beg_pos);
  }
  ReportMessageAt(Scanner::Location(beg_pos, end_pos), message, type);
  *ok = false;
  return impl()->EmptyExpression();
}

template <typename Impl>
bool ParserBase<Impl>::IsValidReferenceExpression(ExpressionT expression) {
  return IsAssignableIdentifier(expression) || expression->IsProperty();
}

template <typename Impl>
void ParserBase<Impl>::CheckDestructuringElement(ExpressionT expression,
                                                 int begin, int end) {
  if (!IsValidPattern(expression) && !expression->IsAssignment() &&
      !IsValidReferenceExpression(expression)) {
    classifier()->RecordAssignmentPatternError(
        Scanner::Location(begin, end),
        MessageTemplate::kInvalidDestructuringTarget);
  }
}

template <typename Impl>
typename ParserBase<Impl>::ExpressionT ParserBase<Impl>::ParseV8Intrinsic(
    bool* ok) {
  // CallRuntime ::
  //   '%' Identifier Arguments

  int pos = peek_position();
  Expect(Token::MOD, CHECK_OK);
  // Allow "eval" or "arguments" for backward compatibility.
  IdentifierT name = ParseIdentifier(kAllowRestrictedIdentifiers, CHECK_OK);
  Scanner::Location spread_pos;
  ExpressionClassifier classifier(this);
  ExpressionListT args = ParseArguments(&spread_pos, CHECK_OK);

  DCHECK(!spread_pos.IsValid());

  return impl()->NewV8Intrinsic(name, args, pos, ok);
}

template <typename Impl>
typename ParserBase<Impl>::ExpressionT ParserBase<Impl>::ParseDoExpression(
    bool* ok) {
  // AssignmentExpression ::
  //     do '{' StatementList '}'

  int pos = peek_position();
  Expect(Token::DO, CHECK_OK);
  BlockT block = ParseBlock(nullptr, CHECK_OK);
  return impl()->RewriteDoExpression(block, pos, ok);
}

// Redefinition of CHECK_OK for parsing statements.
#undef CHECK_OK
#define CHECK_OK CHECK_OK_CUSTOM(NullStatement)

template <typename Impl>
typename ParserBase<Impl>::LazyParsingResult
ParserBase<Impl>::ParseStatementList(StatementListT body, int end_token,
                                     bool may_abort, bool* ok) {
  // StatementList ::
  //   (StatementListItem)* <end_token>

  // Allocate a target stack to use for this set of source
  // elements. This way, all scripts and functions get their own
  // target stack thus avoiding illegal breaks and continues across
  // functions.
  typename Types::TargetScope target_scope(this);
  int count_statements = 0;

  DCHECK(!impl()->IsNullStatementList(body));
  bool directive_prologue = true;  // Parsing directive prologue.

  while (peek() != end_token) {
    if (directive_prologue && peek() != Token::STRING) {
      directive_prologue = false;
    }

    bool starts_with_identifier = peek() == Token::IDENTIFIER;
    Scanner::Location token_loc = scanner()->peek_location();
    StatementT stat =
        ParseStatementListItem(CHECK_OK_CUSTOM(Return, kLazyParsingComplete));

    if (impl()->IsNullStatement(stat) || impl()->IsEmptyStatement(stat)) {
      directive_prologue = false;  // End of directive prologue.
      continue;
    }

    if (directive_prologue) {
      // The length of the token is used to distinguish between strings literals
      // that evaluate equal to directives but contain either escape sequences
      // (e.g., "use \x73trict") or line continuations (e.g., "use \(newline)
      // strict").
      if (impl()->IsUseStrictDirective(stat) &&
          token_loc.end_pos - token_loc.beg_pos == sizeof("use strict") + 1) {
        // Directive "use strict" (ES5 14.1).
        RaiseLanguageMode(STRICT);
        if (!scope()->HasSimpleParameters()) {
          // TC39 deemed "use strict" directives to be an error when occurring
          // in the body of a function with non-simple parameter list, on
          // 29/7/2015. https://goo.gl/ueA7Ln
          impl()->ReportMessageAt(
              token_loc, MessageTemplate::kIllegalLanguageModeDirective,
              "use strict");
          *ok = false;
          return kLazyParsingComplete;
        }
        // Because declarations in strict eval code don't leak into the scope
        // of the eval call, it is likely that functions declared in strict
        // eval code will be used within the eval code, so lazy parsing is
        // probably not a win.
        if (scope()->is_eval_scope()) mode_ = PARSE_EAGERLY;
      } else if (impl()->IsUseAsmDirective(stat) &&
                 token_loc.end_pos - token_loc.beg_pos ==
                     sizeof("use asm") + 1) {
        // Directive "use asm".
        impl()->SetAsmModule();
      } else if (impl()->IsStringLiteral(stat)) {
        // Possibly an unknown directive.
        // Should not change mode, but will increment usage counters
        // as appropriate. Ditto usages below.
        RaiseLanguageMode(SLOPPY);
      } else {
        // End of the directive prologue.
        directive_prologue = false;
        RaiseLanguageMode(SLOPPY);
      }
    } else {
      RaiseLanguageMode(SLOPPY);
    }

    // If we're allowed to abort, we will do so when we see a "long and
    // trivial" function. Our current definition of "long and trivial" is:
    // - over kLazyParseTrialLimit statements
    // - all starting with an identifier (i.e., no if, for, while, etc.)
    if (may_abort) {
      if (!starts_with_identifier) {
        may_abort = false;
      } else if (++count_statements > kLazyParseTrialLimit) {
        return kLazyParsingAborted;
      }
    }

    body->Add(stat, zone());
  }
  return kLazyParsingComplete;
}

template <typename Impl>
typename ParserBase<Impl>::StatementT ParserBase<Impl>::ParseStatementListItem(
    bool* ok) {
  // ECMA 262 6th Edition
  // StatementListItem[Yield, Return] :
  //   Statement[?Yield, ?Return]
  //   Declaration[?Yield]
  //
  // Declaration[Yield] :
  //   HoistableDeclaration[?Yield]
  //   ClassDeclaration[?Yield]
  //   LexicalDeclaration[In, ?Yield]
  //
  // HoistableDeclaration[Yield, Default] :
  //   FunctionDeclaration[?Yield, ?Default]
  //   GeneratorDeclaration[?Yield, ?Default]
  //
  // LexicalDeclaration[In, Yield] :
  //   LetOrConst BindingList[?In, ?Yield] ;

  switch (peek()) {
    case Token::FUNCTION:
      return ParseHoistableDeclaration(nullptr, false, ok);
    case Token::CLASS:
      Consume(Token::CLASS);
      return ParseClassDeclaration(nullptr, false, ok);
    case Token::VAR:
    case Token::CONST:
      return ParseVariableStatement(kStatementListItem, nullptr, ok);
    case Token::LET:
      if (IsNextLetKeyword()) {
        return ParseVariableStatement(kStatementListItem, nullptr, ok);
      }
      break;
    case Token::ASYNC:
      if (allow_harmony_async_await() && PeekAhead() == Token::FUNCTION &&
          !scanner()->HasAnyLineTerminatorAfterNext()) {
        Consume(Token::ASYNC);
        return ParseAsyncFunctionDeclaration(nullptr, false, ok);
      }
    /* falls through */
    default:
      break;
  }
  return ParseStatement(nullptr, kAllowLabelledFunctionStatement, ok);
}

template <typename Impl>
typename ParserBase<Impl>::StatementT ParserBase<Impl>::ParseStatement(
    ZoneList<const AstRawString*>* labels,
    AllowLabelledFunctionStatement allow_function, bool* ok) {
  // Statement ::
  //   Block
  //   VariableStatement
  //   EmptyStatement
  //   ExpressionStatement
  //   IfStatement
  //   IterationStatement
  //   ContinueStatement
  //   BreakStatement
  //   ReturnStatement
  //   WithStatement
  //   LabelledStatement
  //   SwitchStatement
  //   ThrowStatement
  //   TryStatement
  //   DebuggerStatement

  // Note: Since labels can only be used by 'break' and 'continue'
  // statements, which themselves are only valid within blocks,
  // iterations or 'switch' statements (i.e., BreakableStatements),
  // labels can be simply ignored in all other cases; except for
  // trivial labeled break statements 'label: break label' which is
  // parsed into an empty statement.
  switch (peek()) {
    case Token::LBRACE:
      return ParseBlock(labels, ok);
    case Token::SEMICOLON:
      Next();
      return factory()->NewEmptyStatement(kNoSourcePosition);
    case Token::IF:
      return ParseIfStatement(labels, ok);
    case Token::DO:
      return ParseDoWhileStatement(labels, ok);
    case Token::WHILE:
      return ParseWhileStatement(labels, ok);
    case Token::FOR:
      return ParseForStatement(labels, ok);
    case Token::CONTINUE:
    case Token::BREAK:
    case Token::RETURN:
    case Token::THROW:
    case Token::TRY: {
      // These statements must have their labels preserved in an enclosing
      // block, as the corresponding AST nodes do not currently store their
      // labels.
      // TODO(nikolaos, marja): Consider adding the labels to the AST nodes.
      if (labels == nullptr) {
        return ParseStatementAsUnlabelled(labels, ok);
      } else {
        BlockT result =
            factory()->NewBlock(labels, 1, false, kNoSourcePosition);
        typename Types::Target target(this, result);
        StatementT statement = ParseStatementAsUnlabelled(labels, CHECK_OK);
        result->statements()->Add(statement, zone());
        return result;
      }
    }
    case Token::WITH:
      return ParseWithStatement(labels, ok);
    case Token::SWITCH:
      return ParseSwitchStatement(labels, ok);
    case Token::FUNCTION:
      // FunctionDeclaration only allowed as a StatementListItem, not in
      // an arbitrary Statement position. Exceptions such as
      // ES#sec-functiondeclarations-in-ifstatement-statement-clauses
      // are handled by calling ParseScopedStatement rather than
      // ParseStatement directly.
      impl()->ReportMessageAt(scanner()->peek_location(),
                              is_strict(language_mode())
                                  ? MessageTemplate::kStrictFunction
                                  : MessageTemplate::kSloppyFunction);
      *ok = false;
      return impl()->NullStatement();
    case Token::DEBUGGER:
      return ParseDebuggerStatement(ok);
    case Token::VAR:
      return ParseVariableStatement(kStatement, nullptr, ok);
    default:
      return ParseExpressionOrLabelledStatement(labels, allow_function, ok);
  }
}

// This method parses a subset of statements (break, continue, return, throw,
// try) which are to be grouped because they all require their labeles to be
// preserved in an enclosing block.
template <typename Impl>
typename ParserBase<Impl>::StatementT
ParserBase<Impl>::ParseStatementAsUnlabelled(
    ZoneList<const AstRawString*>* labels, bool* ok) {
  switch (peek()) {
    case Token::CONTINUE:
      return ParseContinueStatement(ok);
    case Token::BREAK:
      return ParseBreakStatement(labels, ok);
    case Token::RETURN:
      return ParseReturnStatement(ok);
    case Token::THROW:
      return ParseThrowStatement(ok);
    case Token::TRY:
      return ParseTryStatement(ok);
    default:
      UNREACHABLE();
      return impl()->NullStatement();
  }
}

template <typename Impl>
typename ParserBase<Impl>::BlockT ParserBase<Impl>::ParseBlock(
    ZoneList<const AstRawString*>* labels, bool* ok) {
  // Block ::
  //   '{' StatementList '}'

  // Construct block expecting 16 statements.
  BlockT body = factory()->NewBlock(labels, 16, false, kNoSourcePosition);

  // Parse the statements and collect escaping labels.
  Expect(Token::LBRACE, CHECK_OK_CUSTOM(NullBlock));
  {
    BlockState block_state(zone(), &scope_state_);
    block_state.set_start_position(scanner()->location().beg_pos);
    typename Types::Target target(this, body);

    while (peek() != Token::RBRACE) {
      StatementT stat = ParseStatementListItem(CHECK_OK_CUSTOM(NullBlock));
      if (!impl()->IsNullStatement(stat) && !impl()->IsEmptyStatement(stat)) {
        body->statements()->Add(stat, zone());
      }
    }

    Expect(Token::RBRACE, CHECK_OK_CUSTOM(NullBlock));
    block_state.set_end_position(scanner()->location().end_pos);
    body->set_scope(block_state.FinalizedBlockScope());
  }
  return body;
}

template <typename Impl>
typename ParserBase<Impl>::StatementT ParserBase<Impl>::ParseScopedStatement(
    ZoneList<const AstRawString*>* labels, bool legacy, bool* ok) {
  if (is_strict(language_mode()) || peek() != Token::FUNCTION ||
      (legacy && allow_harmony_restrictive_declarations())) {
    return ParseStatement(labels, kDisallowLabelledFunctionStatement, ok);
  } else {
    if (legacy) {
      impl()->CountUsage(v8::Isolate::kLegacyFunctionDeclaration);
    }
    // Make a block around the statement for a lexical binding
    // is introduced by a FunctionDeclaration.
    BlockState block_state(zone(), &scope_state_);
    block_state.set_start_position(scanner()->location().beg_pos);
    BlockT block = factory()->NewBlock(NULL, 1, false, kNoSourcePosition);
    StatementT body = ParseFunctionDeclaration(CHECK_OK);
    block->statements()->Add(body, zone());
    block_state.set_end_position(scanner()->location().end_pos);
    block->set_scope(block_state.FinalizedBlockScope());
    return block;
  }
}

template <typename Impl>
typename ParserBase<Impl>::StatementT ParserBase<Impl>::ParseVariableStatement(
    VariableDeclarationContext var_context,
    ZoneList<const AstRawString*>* names, bool* ok) {
  // VariableStatement ::
  //   VariableDeclarations ';'

  // The scope of a var declared variable anywhere inside a function
  // is the entire function (ECMA-262, 3rd, 10.1.3, and 12.2). Thus we can
  // transform a source-level var declaration into a (Function) Scope
  // declaration, and rewrite the source-level initialization into an assignment
  // statement. We use a block to collect multiple assignments.
  //
  // We mark the block as initializer block because we don't want the
  // rewriter to add a '.result' assignment to such a block (to get compliant
  // behavior for code such as print(eval('var x = 7')), and for cosmetic
  // reasons when pretty-printing. Also, unless an assignment (initialization)
  // is inside an initializer block, it is ignored.

  DeclarationParsingResult parsing_result;
  StatementT result =
      ParseVariableDeclarations(var_context, &parsing_result, names, CHECK_OK);
  ExpectSemicolon(CHECK_OK);
  return result;
}

template <typename Impl>
typename ParserBase<Impl>::StatementT ParserBase<Impl>::ParseDebuggerStatement(
    bool* ok) {
  // In ECMA-262 'debugger' is defined as a reserved keyword. In some browser
  // contexts this is used as a statement which invokes the debugger as i a
  // break point is present.
  // DebuggerStatement ::
  //   'debugger' ';'

  int pos = peek_position();
  Expect(Token::DEBUGGER, CHECK_OK);
  ExpectSemicolon(CHECK_OK);
  return factory()->NewDebuggerStatement(pos);
}

template <typename Impl>
typename ParserBase<Impl>::StatementT
ParserBase<Impl>::ParseExpressionOrLabelledStatement(
    ZoneList<const AstRawString*>* labels,
    AllowLabelledFunctionStatement allow_function, bool* ok) {
  // ExpressionStatement | LabelledStatement ::
  //   Expression ';'
  //   Identifier ':' Statement
  //
  // ExpressionStatement[Yield] :
  //   [lookahead ∉ {{, function, class, let [}] Expression[In, ?Yield] ;

  int pos = peek_position();

  switch (peek()) {
    case Token::FUNCTION:
    case Token::LBRACE:
      UNREACHABLE();  // Always handled by the callers.
    case Token::CLASS:
      ReportUnexpectedToken(Next());
      *ok = false;
      return impl()->NullStatement();
    default:
      break;
  }

  bool starts_with_identifier = peek_any_identifier();
  ExpressionT expr = ParseExpression(true, CHECK_OK);
  if (peek() == Token::COLON && starts_with_identifier &&
      impl()->IsIdentifier(expr)) {
    // The whole expression was a single identifier, and not, e.g.,
    // something starting with an identifier or a parenthesized identifier.
    labels = impl()->DeclareLabel(labels, impl()->AsIdentifierExpression(expr),
                                  CHECK_OK);
    Consume(Token::COLON);
    // ES#sec-labelled-function-declarations Labelled Function Declarations
    if (peek() == Token::FUNCTION && is_sloppy(language_mode())) {
      if (allow_function == kAllowLabelledFunctionStatement) {
        return ParseFunctionDeclaration(ok);
      } else {
        return ParseScopedStatement(labels, true, ok);
      }
    }
    return ParseStatement(labels, kDisallowLabelledFunctionStatement, ok);
  }

  // If we have an extension, we allow a native function declaration.
  // A native function declaration starts with "native function" with
  // no line-terminator between the two words.
  if (extension_ != nullptr && peek() == Token::FUNCTION &&
      !scanner()->HasAnyLineTerminatorBeforeNext() && impl()->IsNative(expr) &&
      !scanner()->literal_contains_escapes()) {
    return ParseNativeDeclaration(ok);
  }

  // Parsed expression statement, followed by semicolon.
  ExpectSemicolon(CHECK_OK);
  return factory()->NewExpressionStatement(expr, pos);
}

template <typename Impl>
typename ParserBase<Impl>::StatementT ParserBase<Impl>::ParseIfStatement(
    ZoneList<const AstRawString*>* labels, bool* ok) {
  // IfStatement ::
  //   'if' '(' Expression ')' Statement ('else' Statement)?

  int pos = peek_position();
  Expect(Token::IF, CHECK_OK);
  Expect(Token::LPAREN, CHECK_OK);
  ExpressionT condition = ParseExpression(true, CHECK_OK);
  Expect(Token::RPAREN, CHECK_OK);
  StatementT then_statement = ParseScopedStatement(labels, false, CHECK_OK);
  StatementT else_statement = impl()->NullStatement();
  if (Check(Token::ELSE)) {
    else_statement = ParseScopedStatement(labels, false, CHECK_OK);
  } else {
    else_statement = factory()->NewEmptyStatement(kNoSourcePosition);
  }
  return factory()->NewIfStatement(condition, then_statement, else_statement,
                                   pos);
}

template <typename Impl>
typename ParserBase<Impl>::StatementT ParserBase<Impl>::ParseContinueStatement(
    bool* ok) {
  // ContinueStatement ::
  //   'continue' Identifier? ';'

  int pos = peek_position();
  Expect(Token::CONTINUE, CHECK_OK);
  IdentifierT label = impl()->EmptyIdentifier();
  Token::Value tok = peek();
  if (!scanner()->HasAnyLineTerminatorBeforeNext() && tok != Token::SEMICOLON &&
      tok != Token::RBRACE && tok != Token::EOS) {
    // ECMA allows "eval" or "arguments" as labels even in strict mode.
    label = ParseIdentifier(kAllowRestrictedIdentifiers, CHECK_OK);
  }
  typename Types::IterationStatement target =
      impl()->LookupContinueTarget(label, CHECK_OK);
  if (impl()->IsNullStatement(target)) {
    // Illegal continue statement.
    MessageTemplate::Template message = MessageTemplate::kIllegalContinue;
    if (!impl()->IsEmptyIdentifier(label)) {
      message = MessageTemplate::kUnknownLabel;
    }
    ReportMessage(message, label);
    *ok = false;
    return impl()->NullStatement();
  }
  ExpectSemicolon(CHECK_OK);
  return factory()->NewContinueStatement(target, pos);
}

template <typename Impl>
typename ParserBase<Impl>::StatementT ParserBase<Impl>::ParseBreakStatement(
    ZoneList<const AstRawString*>* labels, bool* ok) {
  // BreakStatement ::
  //   'break' Identifier? ';'

  int pos = peek_position();
  Expect(Token::BREAK, CHECK_OK);
  IdentifierT label = impl()->EmptyIdentifier();
  Token::Value tok = peek();
  if (!scanner()->HasAnyLineTerminatorBeforeNext() && tok != Token::SEMICOLON &&
      tok != Token::RBRACE && tok != Token::EOS) {
    // ECMA allows "eval" or "arguments" as labels even in strict mode.
    label = ParseIdentifier(kAllowRestrictedIdentifiers, CHECK_OK);
  }
  // Parse labeled break statements that target themselves into
  // empty statements, e.g. 'l1: l2: l3: break l2;'
  if (!impl()->IsEmptyIdentifier(label) &&
      impl()->ContainsLabel(labels, label)) {
    ExpectSemicolon(CHECK_OK);
    return factory()->NewEmptyStatement(pos);
  }
  typename Types::BreakableStatement target =
      impl()->LookupBreakTarget(label, CHECK_OK);
  if (impl()->IsNullStatement(target)) {
    // Illegal break statement.
    MessageTemplate::Template message = MessageTemplate::kIllegalBreak;
    if (!impl()->IsEmptyIdentifier(label)) {
      message = MessageTemplate::kUnknownLabel;
    }
    ReportMessage(message, label);
    *ok = false;
    return impl()->NullStatement();
  }
  ExpectSemicolon(CHECK_OK);
  return factory()->NewBreakStatement(target, pos);
}

template <typename Impl>
typename ParserBase<Impl>::StatementT ParserBase<Impl>::ParseReturnStatement(
    bool* ok) {
  // ReturnStatement ::
  //   'return' [no line terminator] Expression? ';'

  // Consume the return token. It is necessary to do that before
  // reporting any errors on it, because of the way errors are
  // reported (underlining).
  Expect(Token::RETURN, CHECK_OK);
  Scanner::Location loc = scanner()->location();

  switch (GetDeclarationScope()->scope_type()) {
    case SCRIPT_SCOPE:
    case EVAL_SCOPE:
    case MODULE_SCOPE:
      impl()->ReportMessageAt(loc, MessageTemplate::kIllegalReturn);
      *ok = false;
      return impl()->NullStatement();
    default:
      break;
  }

  Token::Value tok = peek();
  ExpressionT return_value = impl()->EmptyExpression();
  if (scanner()->HasAnyLineTerminatorBeforeNext() || tok == Token::SEMICOLON ||
      tok == Token::RBRACE || tok == Token::EOS) {
    if (IsSubclassConstructor(function_state_->kind())) {
      return_value = impl()->ThisExpression(loc.beg_pos);
    } else {
      return_value = impl()->GetLiteralUndefined(position());
    }
  } else {
    if (IsSubclassConstructor(function_state_->kind())) {
      // Because of the return code rewriting that happens in case of a subclass
      // constructor we don't want to accept tail calls, therefore we don't set
      // ReturnExprScope to kInsideValidReturnStatement here.
      return_value = ParseExpression(true, CHECK_OK);
    } else {
      ReturnExprScope maybe_allow_tail_calls(
          function_state_, ReturnExprContext::kInsideValidReturnStatement);
      return_value = ParseExpression(true, CHECK_OK);

      if (allow_tailcalls() && !is_sloppy(language_mode()) && !is_resumable()) {
        // ES6 14.6.1 Static Semantics: IsInTailPosition
        function_state_->AddImplicitTailCallExpression(return_value);
      }
    }
  }
  ExpectSemicolon(CHECK_OK);
  return_value = impl()->RewriteReturn(return_value, loc.beg_pos);
  return factory()->NewReturnStatement(return_value, loc.beg_pos);
}

template <typename Impl>
typename ParserBase<Impl>::StatementT ParserBase<Impl>::ParseWithStatement(
    ZoneList<const AstRawString*>* labels, bool* ok) {
  // WithStatement ::
  //   'with' '(' Expression ')' Statement

  Expect(Token::WITH, CHECK_OK);
  int pos = position();

  if (is_strict(language_mode())) {
    ReportMessage(MessageTemplate::kStrictWith);
    *ok = false;
    return impl()->NullStatement();
  }

  Expect(Token::LPAREN, CHECK_OK);
  ExpressionT expr = ParseExpression(true, CHECK_OK);
  Expect(Token::RPAREN, CHECK_OK);

  Scope* with_scope = NewScope(WITH_SCOPE);
  StatementT body = impl()->NullStatement();
  {
    BlockState block_state(&scope_state_, with_scope);
    with_scope->set_start_position(scanner()->peek_location().beg_pos);
    body = ParseScopedStatement(labels, true, CHECK_OK);
    with_scope->set_end_position(scanner()->location().end_pos);
  }
  return factory()->NewWithStatement(with_scope, expr, body, pos);
}

template <typename Impl>
typename ParserBase<Impl>::StatementT ParserBase<Impl>::ParseDoWhileStatement(
    ZoneList<const AstRawString*>* labels, bool* ok) {
  // DoStatement ::
  //   'do' Statement 'while' '(' Expression ')' ';'

  auto loop = factory()->NewDoWhileStatement(labels, peek_position());
  typename Types::Target target(this, loop);

  Expect(Token::DO, CHECK_OK);
  StatementT body = ParseScopedStatement(nullptr, true, CHECK_OK);
  Expect(Token::WHILE, CHECK_OK);
  Expect(Token::LPAREN, CHECK_OK);

  ExpressionT cond = ParseExpression(true, CHECK_OK);
  Expect(Token::RPAREN, CHECK_OK);

  // Allow do-statements to be terminated with and without
  // semi-colons. This allows code such as 'do;while(0)return' to
  // parse, which would not be the case if we had used the
  // ExpectSemicolon() functionality here.
  Check(Token::SEMICOLON);

  loop->Initialize(cond, body);
  return loop;
}

template <typename Impl>
typename ParserBase<Impl>::StatementT ParserBase<Impl>::ParseWhileStatement(
    ZoneList<const AstRawString*>* labels, bool* ok) {
  // WhileStatement ::
  //   'while' '(' Expression ')' Statement

  auto loop = factory()->NewWhileStatement(labels, peek_position());
  typename Types::Target target(this, loop);

  Expect(Token::WHILE, CHECK_OK);
  Expect(Token::LPAREN, CHECK_OK);
  ExpressionT cond = ParseExpression(true, CHECK_OK);
  Expect(Token::RPAREN, CHECK_OK);
  StatementT body = ParseScopedStatement(nullptr, true, CHECK_OK);

  loop->Initialize(cond, body);
  return loop;
}

template <typename Impl>
typename ParserBase<Impl>::StatementT ParserBase<Impl>::ParseThrowStatement(
    bool* ok) {
  // ThrowStatement ::
  //   'throw' Expression ';'

  Expect(Token::THROW, CHECK_OK);
  int pos = position();
  if (scanner()->HasAnyLineTerminatorBeforeNext()) {
    ReportMessage(MessageTemplate::kNewlineAfterThrow);
    *ok = false;
    return impl()->NullStatement();
  }
  ExpressionT exception = ParseExpression(true, CHECK_OK);
  ExpectSemicolon(CHECK_OK);

  return impl()->NewThrowStatement(exception, pos);
}

template <typename Impl>
typename ParserBase<Impl>::StatementT ParserBase<Impl>::ParseSwitchStatement(
    ZoneList<const AstRawString*>* labels, bool* ok) {
  // SwitchStatement ::
  //   'switch' '(' Expression ')' '{' CaseClause* '}'
  // CaseClause ::
  //   'case' Expression ':' StatementList
  //   'default' ':' StatementList

  int switch_pos = peek_position();

  Expect(Token::SWITCH, CHECK_OK);
  Expect(Token::LPAREN, CHECK_OK);
  ExpressionT tag = ParseExpression(true, CHECK_OK);
  Expect(Token::RPAREN, CHECK_OK);

  auto switch_statement = factory()->NewSwitchStatement(labels, switch_pos);

  {
    BlockState cases_block_state(zone(), &scope_state_);
    cases_block_state.set_start_position(scanner()->location().beg_pos);
    cases_block_state.SetNonlinear();
    typename Types::Target target(this, switch_statement);

    bool default_seen = false;
    auto cases = impl()->NewCaseClauseList(4);
    Expect(Token::LBRACE, CHECK_OK);
    while (peek() != Token::RBRACE) {
      // An empty label indicates the default case.
      ExpressionT label = impl()->EmptyExpression();
      if (Check(Token::CASE)) {
        label = ParseExpression(true, CHECK_OK);
      } else {
        Expect(Token::DEFAULT, CHECK_OK);
        if (default_seen) {
          ReportMessage(MessageTemplate::kMultipleDefaultsInSwitch);
          *ok = false;
          return impl()->NullStatement();
        }
        default_seen = true;
      }
      Expect(Token::COLON, CHECK_OK);
      int clause_pos = position();
      StatementListT statements = impl()->NewStatementList(5);
      while (peek() != Token::CASE && peek() != Token::DEFAULT &&
             peek() != Token::RBRACE) {
        StatementT stat = ParseStatementListItem(CHECK_OK);
        statements->Add(stat, zone());
      }
      auto clause = factory()->NewCaseClause(label, statements, clause_pos);
      cases->Add(clause, zone());
    }
    Expect(Token::RBRACE, CHECK_OK);

    cases_block_state.set_end_position(scanner()->location().end_pos);
    return impl()->RewriteSwitchStatement(
        tag, switch_statement, cases, cases_block_state.FinalizedBlockScope());
  }
}

template <typename Impl>
typename ParserBase<Impl>::StatementT ParserBase<Impl>::ParseTryStatement(
    bool* ok) {
  // TryStatement ::
  //   'try' Block Catch
  //   'try' Block Finally
  //   'try' Block Catch Finally
  //
  // Catch ::
  //   'catch' '(' Identifier ')' Block
  //
  // Finally ::
  //   'finally' Block

  Expect(Token::TRY, CHECK_OK);
  int pos = position();

  BlockT try_block = impl()->NullBlock();
  {
    ReturnExprScope no_tail_calls(function_state_,
                                  ReturnExprContext::kInsideTryBlock);
    try_block = ParseBlock(nullptr, CHECK_OK);
  }

  CatchInfo catch_info(this);
  catch_info.for_promise_reject = allow_natives() && Check(Token::MOD);

  if (peek() != Token::CATCH && peek() != Token::FINALLY) {
    ReportMessage(MessageTemplate::kNoCatchOrFinally);
    *ok = false;
    return impl()->NullStatement();
  }

  BlockT catch_block = impl()->NullBlock();
  if (Check(Token::CATCH)) {
    Expect(Token::LPAREN, CHECK_OK);
    catch_info.scope = NewScope(CATCH_SCOPE);
    catch_info.scope->set_start_position(scanner()->location().beg_pos);

    {
      CollectExpressionsInTailPositionToListScope
          collect_tail_call_expressions_scope(
              function_state_, &catch_info.tail_call_expressions);
      BlockState catch_block_state(&scope_state_, catch_info.scope);

      catch_block = factory()->NewBlock(nullptr, 16, false, kNoSourcePosition);

      // Create a block scope to hold any lexical declarations created
      // as part of destructuring the catch parameter.
      {
        BlockState catch_variable_block_state(zone(), &scope_state_);
        catch_variable_block_state.set_start_position(
            scanner()->location().beg_pos);
        typename Types::Target target(this, catch_block);

        // This does not simply call ParsePrimaryExpression to avoid
        // ExpressionFromIdentifier from being called in the first
        // branch, which would introduce an unresolved symbol and mess
        // with arrow function names.
        if (peek_any_identifier()) {
          catch_info.name =
              ParseIdentifier(kDontAllowRestrictedIdentifiers, CHECK_OK);
        } else {
          ExpressionClassifier pattern_classifier(this);
          catch_info.pattern = ParsePrimaryExpression(CHECK_OK);
          ValidateBindingPattern(CHECK_OK);
        }

        Expect(Token::RPAREN, CHECK_OK);
        impl()->RewriteCatchPattern(&catch_info, CHECK_OK);
        if (!impl()->IsNullStatement(catch_info.init_block)) {
          catch_block->statements()->Add(catch_info.init_block, zone());
        }

        catch_info.inner_block = ParseBlock(nullptr, CHECK_OK);
        catch_block->statements()->Add(catch_info.inner_block, zone());
        impl()->ValidateCatchBlock(catch_info, CHECK_OK);
        catch_variable_block_state.set_end_position(
            scanner()->location().end_pos);
        catch_block->set_scope(
            catch_variable_block_state.FinalizedBlockScope());
      }
    }

    catch_info.scope->set_end_position(scanner()->location().end_pos);
  }

  BlockT finally_block = impl()->NullBlock();
  DCHECK(peek() == Token::FINALLY || !impl()->IsNullStatement(catch_block));
  if (Check(Token::FINALLY)) {
    finally_block = ParseBlock(nullptr, CHECK_OK);
  }

  return impl()->RewriteTryStatement(try_block, catch_block, finally_block,
                                     catch_info, pos);
}

template <typename Impl>
typename ParserBase<Impl>::StatementT ParserBase<Impl>::ParseForStatement(
    ZoneList<const AstRawString*>* labels, bool* ok) {
  int stmt_pos = peek_position();
  ForInfo for_info(this);
  bool bound_names_are_lexical = false;

  // Create an in-between scope for let-bound iteration variables.
  BlockState for_state(zone(), &scope_state_);
  Expect(Token::FOR, CHECK_OK);
  Expect(Token::LPAREN, CHECK_OK);
  for_state.set_start_position(scanner()->location().beg_pos);
  for_state.set_is_hidden();

  StatementT init = impl()->NullStatement();
  if (peek() != Token::SEMICOLON) {
    // An initializer is present.
    if (peek() == Token::VAR || peek() == Token::CONST ||
        (peek() == Token::LET && IsNextLetKeyword())) {
      // The initializer contains declarations.
      ParseVariableDeclarations(kForStatement, &for_info.parsing_result,
                                nullptr, CHECK_OK);
      bound_names_are_lexical =
          IsLexicalVariableMode(for_info.parsing_result.descriptor.mode);
      for_info.each_loc = scanner()->location();

      if (CheckInOrOf(&for_info.mode)) {
        // Just one declaration followed by in/of.
        if (for_info.parsing_result.declarations.length() != 1) {
          impl()->ReportMessageAt(
              for_info.parsing_result.bindings_loc,
              MessageTemplate::kForInOfLoopMultiBindings,
              ForEachStatement::VisitModeString(for_info.mode));
          *ok = false;
          return impl()->NullStatement();
        }
        if (for_info.parsing_result.first_initializer_loc.IsValid() &&
            (is_strict(language_mode()) ||
             for_info.mode == ForEachStatement::ITERATE ||
             bound_names_are_lexical ||
             !impl()->IsIdentifier(
                 for_info.parsing_result.declarations[0].pattern) ||
             allow_harmony_for_in())) {
          // Only increment the use count if we would have let this through
          // without the flag.
          if (allow_harmony_for_in()) {
            impl()->CountUsage(v8::Isolate::kForInInitializer);
          }
          impl()->ReportMessageAt(
              for_info.parsing_result.first_initializer_loc,
              MessageTemplate::kForInOfLoopInitializer,
              ForEachStatement::VisitModeString(for_info.mode));
          *ok = false;
          return impl()->NullStatement();
        }

        BlockT init_block = impl()->RewriteForVarInLegacy(for_info);

        auto loop =
            factory()->NewForEachStatement(for_info.mode, labels, stmt_pos);
        typename Types::Target target(this, loop);

        int each_keyword_pos = scanner()->location().beg_pos;

        ExpressionT enumerable = impl()->EmptyExpression();
        if (for_info.mode == ForEachStatement::ITERATE) {
          ExpressionClassifier classifier(this);
          enumerable = ParseAssignmentExpression(true, CHECK_OK);
          impl()->RewriteNonPattern(CHECK_OK);
        } else {
          enumerable = ParseExpression(true, CHECK_OK);
        }

        Expect(Token::RPAREN, CHECK_OK);

        StatementT final_loop = impl()->NullStatement();
        {
          ReturnExprScope no_tail_calls(function_state_,
                                        ReturnExprContext::kInsideForInOfBody);
          BlockState block_state(zone(), &scope_state_);
          block_state.set_start_position(scanner()->location().beg_pos);

          StatementT body = ParseScopedStatement(nullptr, true, CHECK_OK);

          BlockT body_block = impl()->NullBlock();
          ExpressionT each_variable = impl()->EmptyExpression();
          impl()->DesugarBindingInForEachStatement(&for_info, &body_block,
                                                   &each_variable, CHECK_OK);
          body_block->statements()->Add(body, zone());
          final_loop = impl()->InitializeForEachStatement(
              loop, each_variable, enumerable, body_block, each_keyword_pos);

          block_state.set_end_position(scanner()->location().end_pos);
          body_block->set_scope(block_state.FinalizedBlockScope());
        }

        init_block =
            impl()->CreateForEachStatementTDZ(init_block, for_info, ok);

        for_state.set_end_position(scanner()->location().end_pos);
        Scope* for_scope = for_state.FinalizedBlockScope();
        // Parsed for-in loop w/ variable declarations.
        if (!impl()->IsNullStatement(init_block)) {
          init_block->statements()->Add(final_loop, zone());
          init_block->set_scope(for_scope);
          return init_block;
        } else {
          DCHECK_NULL(for_scope);
          return final_loop;
        }
      } else {
        // One or more declaration not followed by in/of.
        init = impl()->BuildInitializationBlock(
            &for_info.parsing_result,
            bound_names_are_lexical ? &for_info.bound_names : nullptr,
            CHECK_OK);
      }
    } else {
      // The initializer does not contain declarations.
      int lhs_beg_pos = peek_position();
      ExpressionClassifier classifier(this);
      ExpressionT expression = ParseExpressionCoverGrammar(false, CHECK_OK);
      int lhs_end_pos = scanner()->location().end_pos;

      bool is_for_each = CheckInOrOf(&for_info.mode);
      bool is_destructuring = is_for_each && (expression->IsArrayLiteral() ||
                                              expression->IsObjectLiteral());

      if (is_destructuring) {
        ValidateAssignmentPattern(CHECK_OK);
      } else {
        impl()->RewriteNonPattern(CHECK_OK);
      }

      if (is_for_each) {
        // Initializer is reference followed by in/of.
        if (!is_destructuring) {
          expression = impl()->CheckAndRewriteReferenceExpression(
              expression, lhs_beg_pos, lhs_end_pos,
              MessageTemplate::kInvalidLhsInFor, kSyntaxError, CHECK_OK);
        }

        auto loop =
            factory()->NewForEachStatement(for_info.mode, labels, stmt_pos);
        typename Types::Target target(this, loop);

        int each_keyword_pos = scanner()->location().beg_pos;

        ExpressionT enumerable = impl()->EmptyExpression();
        if (for_info.mode == ForEachStatement::ITERATE) {
          ExpressionClassifier classifier(this);
          enumerable = ParseAssignmentExpression(true, CHECK_OK);
          impl()->RewriteNonPattern(CHECK_OK);
        } else {
          enumerable = ParseExpression(true, CHECK_OK);
        }

        Expect(Token::RPAREN, CHECK_OK);

        {
          ReturnExprScope no_tail_calls(function_state_,
                                        ReturnExprContext::kInsideForInOfBody);
          BlockState block_state(zone(), &scope_state_);
          block_state.set_start_position(scanner()->location().beg_pos);

          // For legacy compat reasons, give for loops similar treatment to
          // if statements in allowing a function declaration for a body
          StatementT body = ParseScopedStatement(nullptr, true, CHECK_OK);
          block_state.set_end_position(scanner()->location().end_pos);
          StatementT final_loop = impl()->InitializeForEachStatement(
              loop, expression, enumerable, body, each_keyword_pos);

          Scope* for_scope = for_state.FinalizedBlockScope();
          DCHECK_NULL(for_scope);
          USE(for_scope);
          Scope* block_scope = block_state.FinalizedBlockScope();
          DCHECK_NULL(block_scope);
          USE(block_scope);
          return final_loop;
        }
      } else {
        // Initializer is just an expression.
        init = factory()->NewExpressionStatement(expression, lhs_beg_pos);
      }
    }
  }

  // Standard 'for' loop, we have parsed the initializer at this point.
  auto loop = factory()->NewForStatement(labels, stmt_pos);
  typename Types::Target target(this, loop);

  Expect(Token::SEMICOLON, CHECK_OK);

  ExpressionT cond = impl()->EmptyExpression();
  StatementT next = impl()->NullStatement();
  StatementT body = impl()->NullStatement();

  // If there are let bindings, then condition and the next statement of the
  // for loop must be parsed in a new scope.
  Scope* inner_scope = scope();
  // TODO(verwaest): Allocate this through a ScopeState as well.
  if (bound_names_are_lexical && for_info.bound_names.length() > 0) {
    inner_scope = NewScopeWithParent(inner_scope, BLOCK_SCOPE);
    inner_scope->set_start_position(scanner()->location().beg_pos);
  }
  {
    BlockState block_state(&scope_state_, inner_scope);

    if (peek() != Token::SEMICOLON) {
      cond = ParseExpression(true, CHECK_OK);
    }
    Expect(Token::SEMICOLON, CHECK_OK);

    if (peek() != Token::RPAREN) {
      ExpressionT exp = ParseExpression(true, CHECK_OK);
      next = factory()->NewExpressionStatement(exp, exp->position());
    }
    Expect(Token::RPAREN, CHECK_OK);

    body = ParseScopedStatement(nullptr, true, CHECK_OK);
  }

  if (bound_names_are_lexical && for_info.bound_names.length() > 0) {
    auto result = impl()->DesugarLexicalBindingsInForStatement(
        loop, init, cond, next, body, inner_scope, for_info, CHECK_OK);
    for_state.set_end_position(scanner()->location().end_pos);
    return result;
  } else {
    for_state.set_end_position(scanner()->location().end_pos);
    Scope* for_scope = for_state.FinalizedBlockScope();
    if (for_scope != nullptr) {
      // Rewrite a for statement of the form
      //   for (const x = i; c; n) b
      //
      // into
      //
      //   {
      //     const x = i;
      //     for (; c; n) b
      //   }
      //
      // or, desugar
      //   for (; c; n) b
      // into
      //   {
      //     for (; c; n) b
      //   }
      // just in case b introduces a lexical binding some other way, e.g., if b
      // is a FunctionDeclaration.
      BlockT block = factory()->NewBlock(nullptr, 2, false, kNoSourcePosition);
      if (!impl()->IsNullStatement(init)) {
        block->statements()->Add(init, zone());
      }
      block->statements()->Add(loop, zone());
      block->set_scope(for_scope);
      loop->Initialize(init, cond, next, body);
      return block;
    } else {
      loop->Initialize(init, cond, next, body);
      return loop;
    }
  }
}

#undef CHECK_OK
#undef CHECK_OK_CUSTOM

template <typename Impl>
void ParserBase<Impl>::ObjectLiteralChecker::CheckDuplicateProto(
    Token::Value property) {
  if (property == Token::SMI || property == Token::NUMBER) return;

  if (IsProto()) {
    if (has_seen_proto_) {
      this->parser()->classifier()->RecordExpressionError(
          this->scanner()->location(), MessageTemplate::kDuplicateProto);
      return;
    }
    has_seen_proto_ = true;
  }
}

template <typename Impl>
void ParserBase<Impl>::ClassLiteralChecker::CheckClassMethodName(
    Token::Value property, PropertyKind type, bool is_generator, bool is_async,
    bool is_static, bool* ok) {
  DCHECK(type == PropertyKind::kMethodProperty ||
         type == PropertyKind::kAccessorProperty);

  if (property == Token::SMI || property == Token::NUMBER) return;

  if (is_static) {
    if (IsPrototype()) {
      this->parser()->ReportMessage(MessageTemplate::kStaticPrototype);
      *ok = false;
      return;
    }
  } else if (IsConstructor()) {
    if (is_generator || is_async || type == PropertyKind::kAccessorProperty) {
      MessageTemplate::Template msg =
          is_generator ? MessageTemplate::kConstructorIsGenerator
                       : is_async ? MessageTemplate::kConstructorIsAsync
                                  : MessageTemplate::kConstructorIsAccessor;
      this->parser()->ReportMessage(msg);
      *ok = false;
      return;
    }
    if (has_seen_constructor_) {
      this->parser()->ReportMessage(MessageTemplate::kDuplicateConstructor);
      *ok = false;
      return;
    }
    has_seen_constructor_ = true;
    return;
  }
}


}  // namespace internal
}  // namespace v8

#endif  // V8_PARSING_PARSER_BASE_H
