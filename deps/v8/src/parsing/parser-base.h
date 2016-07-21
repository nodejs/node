// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PARSING_PARSER_BASE_H
#define V8_PARSING_PARSER_BASE_H

#include "src/ast/scopes.h"
#include "src/bailout-reason.h"
#include "src/hashmap.h"
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

enum class FunctionBody { Normal, SingleExpression };

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

enum class MethodKind {
  Normal = 0,
  Static = 1 << 0,
  Generator = 1 << 1,
  StaticGenerator = Static | Generator,
  Async = 1 << 2,
  StaticAsync = Static | Async,

  /* Any non-ordinary method kinds */
  SpecialMask = Generator | Async
};

inline bool IsValidMethodKind(MethodKind kind) {
  return kind == MethodKind::Normal || kind == MethodKind::Static ||
         kind == MethodKind::Generator || kind == MethodKind::StaticGenerator ||
         kind == MethodKind::Async || kind == MethodKind::StaticAsync;
}

static inline MethodKind operator|(MethodKind lhs, MethodKind rhs) {
  typedef unsigned char T;
  return static_cast<MethodKind>(static_cast<T>(lhs) | static_cast<T>(rhs));
}

static inline MethodKind& operator|=(MethodKind& lhs, const MethodKind& rhs) {
  lhs = lhs | rhs;
  DCHECK(IsValidMethodKind(lhs));
  return lhs;
}

static inline bool operator&(MethodKind bitfield, MethodKind mask) {
  typedef unsigned char T;
  return static_cast<T>(bitfield) & static_cast<T>(mask);
}

inline bool IsNormalMethod(MethodKind kind) {
  return kind == MethodKind::Normal;
}

inline bool IsSpecialMethod(MethodKind kind) {
  return kind & MethodKind::SpecialMask;
}

inline bool IsStaticMethod(MethodKind kind) {
  return kind & MethodKind::Static;
}

inline bool IsGeneratorMethod(MethodKind kind) {
  return kind & MethodKind::Generator;
}

inline bool IsAsyncMethod(MethodKind kind) { return kind & MethodKind::Async; }

struct FormalParametersBase {
  explicit FormalParametersBase(Scope* scope) : scope(scope) {}
  Scope* scope;
  bool has_rest = false;
  bool is_simple = true;
  int materialized_literals_count = 0;
};


// Common base class shared between parser and pre-parser. Traits encapsulate
// the differences between Parser and PreParser:

// - Return types: For example, Parser functions return Expression* and
// PreParser functions return PreParserExpression.

// - Creating parse tree nodes: Parser generates an AST during the recursive
// descent. PreParser doesn't create a tree. Instead, it passes around minimal
// data objects (PreParserExpression, PreParserIdentifier etc.) which contain
// just enough data for the upper layer functions. PreParserFactory is
// responsible for creating these dummy objects. It provides a similar kind of
// interface as AstNodeFactory, so ParserBase doesn't need to care which one is
// used.

// - Miscellaneous other tasks interleaved with the recursive descent. For
// example, Parser keeps track of which function literals should be marked as
// pretenured, and PreParser doesn't care.

// The traits are expected to contain the following typedefs:
// struct Traits {
//   // In particular...
//   struct Type {
//     // Used by FunctionState and BlockState.
//     typedef Scope;
//     typedef GeneratorVariable;
//     // Return types for traversing functions.
//     typedef Identifier;
//     typedef Expression;
//     typedef FunctionLiteral;
//     typedef ClassLiteral;
//     typedef ObjectLiteralProperty;
//     typedef Literal;
//     typedef ExpressionList;
//     typedef PropertyList;
//     typedef FormalParameter;
//     typedef FormalParameters;
//     // For constructing objects returned by the traversing functions.
//     typedef Factory;
//   };
//   // ...
// };

template <typename Traits>
class ParserBase : public Traits {
 public:
  // Shorten type names defined by Traits.
  typedef typename Traits::Type::Expression ExpressionT;
  typedef typename Traits::Type::Identifier IdentifierT;
  typedef typename Traits::Type::FormalParameter FormalParameterT;
  typedef typename Traits::Type::FormalParameters FormalParametersT;
  typedef typename Traits::Type::FunctionLiteral FunctionLiteralT;
  typedef typename Traits::Type::Literal LiteralT;
  typedef typename Traits::Type::ObjectLiteralProperty ObjectLiteralPropertyT;
  typedef typename Traits::Type::StatementList StatementListT;
  typedef typename Traits::Type::ExpressionClassifier ExpressionClassifier;

  ParserBase(Zone* zone, Scanner* scanner, uintptr_t stack_limit,
             v8::Extension* extension, AstValueFactory* ast_value_factory,
             ParserRecorder* log, typename Traits::Type::Parser this_object)
      : Traits(this_object),
        scope_(NULL),
        function_state_(NULL),
        extension_(extension),
        fni_(NULL),
        ast_value_factory_(ast_value_factory),
        log_(log),
        mode_(PARSE_EAGERLY),  // Lazy mode must be set explicitly.
        parsing_module_(false),
        stack_limit_(stack_limit),
        zone_(zone),
        scanner_(scanner),
        stack_overflow_(false),
        allow_lazy_(false),
        allow_natives_(false),
        allow_tailcalls_(false),
        allow_harmony_restrictive_declarations_(false),
        allow_harmony_do_expressions_(false),
        allow_harmony_for_in_(false),
        allow_harmony_function_name_(false),
        allow_harmony_function_sent_(false),
        allow_harmony_async_await_(false) {}

#define ALLOW_ACCESSORS(name)                           \
  bool allow_##name() const { return allow_##name##_; } \
  void set_allow_##name(bool allow) { allow_##name##_ = allow; }

#define SCANNER_ACCESSORS(name)                                  \
  bool allow_##name() const { return scanner_->allow_##name(); } \
  void set_allow_##name(bool allow) {                            \
    return scanner_->set_allow_##name(allow);                    \
  }

  ALLOW_ACCESSORS(lazy);
  ALLOW_ACCESSORS(natives);
  ALLOW_ACCESSORS(tailcalls);
  ALLOW_ACCESSORS(harmony_restrictive_declarations);
  ALLOW_ACCESSORS(harmony_do_expressions);
  ALLOW_ACCESSORS(harmony_for_in);
  ALLOW_ACCESSORS(harmony_function_name);
  ALLOW_ACCESSORS(harmony_function_sent);
  ALLOW_ACCESSORS(harmony_async_await);
  SCANNER_ACCESSORS(harmony_exponentiation_operator);

#undef SCANNER_ACCESSORS
#undef ALLOW_ACCESSORS

  uintptr_t stack_limit() const { return stack_limit_; }

 protected:
  enum AllowRestrictedIdentifiers {
    kAllowRestrictedIdentifiers,
    kDontAllowRestrictedIdentifiers
  };

  enum Mode {
    PARSE_LAZILY,
    PARSE_EAGERLY
  };

  enum VariableDeclarationContext {
    kStatementListItem,
    kStatement,
    kForStatement
  };

  class Checkpoint;
  class ObjectLiteralCheckerBase;

  // ---------------------------------------------------------------------------
  // FunctionState and BlockState together implement the parser's scope stack.
  // The parser's current scope is in scope_. BlockState and FunctionState
  // constructors push on the scope stack and the destructors pop. They are also
  // used to hold the parser's per-function and per-block state.
  class BlockState BASE_EMBEDDED {
   public:
    BlockState(Scope** scope_stack, Scope* scope)
        : scope_stack_(scope_stack), outer_scope_(*scope_stack) {
      *scope_stack_ = scope;
    }
    ~BlockState() { *scope_stack_ = outer_scope_; }

   private:
    Scope** scope_stack_;
    Scope* outer_scope_;
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

    void AddExplicitTailCall(ExpressionT expr, const Scanner::Location& loc) {
      if (!has_explicit_tail_calls()) {
        loc_ = loc;
        has_explicit_tail_calls_ = true;
      }
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

  class FunctionState BASE_EMBEDDED {
   public:
    FunctionState(FunctionState** function_state_stack, Scope** scope_stack,
                  Scope* scope, FunctionKind kind,
                  typename Traits::Type::Factory* factory);
    ~FunctionState();

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

    Scanner::Location this_location() const { return this_location_; }
    Scanner::Location super_location() const { return super_location_; }
    Scanner::Location return_location() const { return return_location_; }
    void set_this_location(Scanner::Location location) {
      this_location_ = location;
    }
    void set_super_location(Scanner::Location location) {
      super_location_ = location;
    }
    void set_return_location(Scanner::Location location) {
      return_location_ = location;
    }

    bool is_generator() const { return IsGeneratorFunction(kind_); }
    bool is_async_function() const { return IsAsyncFunction(kind_); }
    bool is_resumable() const { return is_generator() || is_async_function(); }

    FunctionKind kind() const { return kind_; }
    FunctionState* outer() const { return outer_function_state_; }

    void set_generator_object_variable(
        typename Traits::Type::GeneratorVariable* variable) {
      DCHECK(variable != NULL);
      DCHECK(is_resumable());
      generator_object_variable_ = variable;
    }
    typename Traits::Type::GeneratorVariable* generator_object_variable()
        const {
      return generator_object_variable_;
    }

    typename Traits::Type::Factory* factory() { return factory_; }

    const List<DestructuringAssignment>& destructuring_assignments_to_rewrite()
        const {
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
    void AddExplicitTailCallExpression(ExpressionT expression,
                                       const Scanner::Location& loc) {
      DCHECK(expression->IsCall());
      if (return_expr_context() ==
          ReturnExprContext::kInsideValidReturnStatement) {
        tail_call_expressions_.AddExplicitTailCall(expression, loc);
      }
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

    void next_function_is_parenthesized(bool parenthesized) {
      next_function_is_parenthesized_ = parenthesized;
    }

    bool this_function_is_parenthesized() const {
      return this_function_is_parenthesized_;
    }

   private:
    void AddDestructuringAssignment(DestructuringAssignment pair) {
      destructuring_assignments_to_rewrite_.Add(pair);
    }

    V8_INLINE Scope* scope() { return *scope_stack_; }

    void AddNonPatternForRewriting(ExpressionT expr) {
      non_patterns_to_rewrite_.Add(expr, (*scope_stack_)->zone());
    }

    // Used to assign an index to each literal that needs materialization in
    // the function.  Includes regexp literals, and boilerplate for object and
    // array literals.
    int next_materialized_literal_index_;

    // Properties count estimation.
    int expected_property_count_;

    // Location of most recent use of 'this' (invalid if none).
    Scanner::Location this_location_;

    // Location of most recent 'return' statement (invalid if none).
    Scanner::Location return_location_;

    // Location of call to the "super" constructor (invalid if none).
    Scanner::Location super_location_;

    FunctionKind kind_;
    // For generators, this variable may hold the generator object. It variable
    // is used by yield expressions and return statements. It is not necessary
    // for generator functions to have this variable set.
    Variable* generator_object_variable_;

    FunctionState** function_state_stack_;
    FunctionState* outer_function_state_;
    Scope** scope_stack_;
    Scope* outer_scope_;

    List<DestructuringAssignment> destructuring_assignments_to_rewrite_;
    TailCallExpressionList tail_call_expressions_;
    ReturnExprContext return_expr_context_;
    ZoneList<ExpressionT> non_patterns_to_rewrite_;

    typename Traits::Type::Factory* factory_;

    // If true, the next (and immediately following) function literal is
    // preceded by a parenthesis.
    bool next_function_is_parenthesized_;

    // The value of the parents' next_function_is_parenthesized_, as it applies
    // to this function. Filled in by constructor.
    bool this_function_is_parenthesized_;

    friend class ParserTraits;
    friend class PreParserTraits;
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

  Scope* NewScope(Scope* parent, ScopeType scope_type) {
    // Must always pass the function kind for FUNCTION_SCOPE.
    DCHECK(scope_type != FUNCTION_SCOPE);
    return NewScope(parent, scope_type, kNormalFunction);
  }

  Scope* NewScope(Scope* parent, ScopeType scope_type, FunctionKind kind) {
    DCHECK(ast_value_factory());
    Scope* result = new (zone())
        Scope(zone(), parent, scope_type, ast_value_factory(), kind);
    result->Initialize();
    return result;
  }

  Scanner* scanner() const { return scanner_; }
  AstValueFactory* ast_value_factory() const { return ast_value_factory_; }
  int position() { return scanner_->location().beg_pos; }
  int peek_position() { return scanner_->peek_location().beg_pos; }
  bool stack_overflow() const { return stack_overflow_; }
  void set_stack_overflow() { stack_overflow_ = true; }
  Mode mode() const { return mode_; }
  Zone* zone() const { return zone_; }

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

  bool peek_any_identifier() {
    Token::Value next = peek();
    return next == Token::IDENTIFIER || next == Token::ENUM ||
           next == Token::AWAIT || next == Token::ASYNC ||
           next == Token::FUTURE_STRICT_RESERVED_WORD || next == Token::LET ||
           next == Token::STATIC || next == Token::YIELD;
  }

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
    Expect(Token::IDENTIFIER, ok);
    if (!*ok) return;
    if (!scanner()->is_literal_contextual_keyword(keyword)) {
      ReportUnexpectedToken(scanner()->current_token());
      *ok = false;
    }
  }

  bool CheckInOrOf(ForEachStatement::VisitMode* visit_mode, bool* ok) {
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
      ReportMessageAt(octal, message);
      scanner()->clear_octal_position();
      *ok = false;
    }
  }
  // for now, this check just collects statistics.
  void CheckDecimalLiteralWithLeadingZero(int* use_counts, int beg_pos,
                                          int end_pos) {
    Scanner::Location token_location =
        scanner()->decimal_with_leading_zero_position();
    if (token_location.IsValid() && beg_pos <= token_location.beg_pos &&
        token_location.end_pos <= end_pos) {
      scanner()->clear_decimal_with_leading_zero_position();
      if (use_counts != nullptr)
        ++use_counts[v8::Isolate::kDecimalWithLeadingZeroInStrictMode];
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

  void CheckDestructuringElement(ExpressionT element,
                                 ExpressionClassifier* classifier, int beg_pos,
                                 int end_pos);

  // Checking the name of a function literal. This has to be done after parsing
  // the function, since the function can declare itself strict.
  void CheckFunctionName(LanguageMode language_mode, IdentifierT function_name,
                         FunctionNameValidity function_name_validity,
                         const Scanner::Location& function_name_loc, bool* ok) {
    if (function_name_validity == kSkipFunctionNameCheck) return;
    // The function name needs to be checked in strict mode.
    if (is_sloppy(language_mode)) return;

    if (this->IsEvalOrArguments(function_name)) {
      Traits::ReportMessageAt(function_name_loc,
                              MessageTemplate::kStrictEvalArguments);
      *ok = false;
      return;
    }
    if (function_name_validity == kFunctionNameIsStrictReserved) {
      Traits::ReportMessageAt(function_name_loc,
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

  typename Traits::Type::Factory* factory() {
    return function_state_->factory();
  }

  LanguageMode language_mode() { return scope_->language_mode(); }
  bool is_generator() const { return function_state_->is_generator(); }
  bool is_async_function() const {
    return function_state_->is_async_function();
  }
  bool is_resumable() const { return function_state_->is_resumable(); }

  // Report syntax errors.
  void ReportMessage(MessageTemplate::Template message, const char* arg = NULL,
                     ParseErrorType error_type = kSyntaxError) {
    Scanner::Location source_location = scanner()->location();
    Traits::ReportMessageAt(source_location, message, arg, error_type);
  }

  void ReportMessageAt(Scanner::Location location,
                       MessageTemplate::Template message,
                       ParseErrorType error_type = kSyntaxError) {
    Traits::ReportMessageAt(location, message, reinterpret_cast<const char*>(0),
                            error_type);
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
    Traits::ReportMessageAt(error.location, error.message, error.arg,
                            error.type);
  }

  void ValidateExpression(const ExpressionClassifier* classifier, bool* ok) {
    if (!classifier->is_valid_expression() ||
        classifier->has_cover_initialized_name()) {
      const Scanner::Location& a = classifier->expression_error().location;
      const Scanner::Location& b =
          classifier->cover_initialized_name_error().location;
      if (a.beg_pos < 0 || (b.beg_pos >= 0 && a.beg_pos > b.beg_pos)) {
        ReportClassifierError(classifier->cover_initialized_name_error());
      } else {
        ReportClassifierError(classifier->expression_error());
      }
      *ok = false;
    }
  }

  void ValidateFormalParameterInitializer(
      const ExpressionClassifier* classifier, bool* ok) {
    if (!classifier->is_valid_formal_parameter_initializer()) {
      ReportClassifierError(classifier->formal_parameter_initializer_error());
      *ok = false;
    }
  }

  void ValidateBindingPattern(const ExpressionClassifier* classifier,
                              bool* ok) {
    if (!classifier->is_valid_binding_pattern() ||
        !classifier->is_valid_async_binding_pattern()) {
      const Scanner::Location& a = classifier->binding_pattern_error().location;
      const Scanner::Location& b =
          classifier->async_binding_pattern_error().location;
      if (a.beg_pos < 0 || (b.beg_pos >= 0 && a.beg_pos > b.beg_pos)) {
        ReportClassifierError(classifier->async_binding_pattern_error());
      } else {
        ReportClassifierError(classifier->binding_pattern_error());
      }
      *ok = false;
    }
  }

  void ValidateAssignmentPattern(const ExpressionClassifier* classifier,
                                 bool* ok) {
    if (!classifier->is_valid_assignment_pattern()) {
      ReportClassifierError(classifier->assignment_pattern_error());
      *ok = false;
    }
  }

  void ValidateFormalParameters(const ExpressionClassifier* classifier,
                                LanguageMode language_mode,
                                bool allow_duplicates, bool* ok) {
    if (!allow_duplicates &&
        !classifier->is_valid_formal_parameter_list_without_duplicates()) {
      ReportClassifierError(classifier->duplicate_formal_parameter_error());
      *ok = false;
    } else if (is_strict(language_mode) &&
               !classifier->is_valid_strict_mode_formal_parameters()) {
      ReportClassifierError(classifier->strict_mode_formal_parameter_error());
      *ok = false;
    }
  }

  void ValidateArrowFormalParameters(const ExpressionClassifier* classifier,
                                     ExpressionT expr,
                                     bool parenthesized_formals, bool is_async,
                                     bool* ok) {
    if (classifier->is_valid_binding_pattern()) {
      // A simple arrow formal parameter: IDENTIFIER => BODY.
      if (!this->IsIdentifier(expr)) {
        Traits::ReportMessageAt(scanner()->location(),
                                MessageTemplate::kUnexpectedToken,
                                Token::String(scanner()->current_token()));
        *ok = false;
      }
    } else if (!classifier->is_valid_arrow_formal_parameters()) {
      // If after parsing the expr, we see an error but the expression is
      // neither a valid binding pattern nor a valid parenthesized formal
      // parameter list, show the "arrow formal parameters" error if the formals
      // started with a parenthesis, and the binding pattern error otherwise.
      const typename ExpressionClassifier::Error& error =
          parenthesized_formals ? classifier->arrow_formal_parameters_error()
                                : classifier->binding_pattern_error();
      ReportClassifierError(error);
      *ok = false;
    }
    if (is_async && !classifier->is_valid_async_arrow_formal_parameters()) {
      const typename ExpressionClassifier::Error& error =
          classifier->async_arrow_formal_parameters_error();
      ReportClassifierError(error);
      *ok = false;
    }
  }

  void ValidateLetPattern(const ExpressionClassifier* classifier, bool* ok) {
    if (!classifier->is_valid_let_pattern()) {
      ReportClassifierError(classifier->let_pattern_error());
      *ok = false;
    }
  }

  void CheckNoTailCallExpressions(const ExpressionClassifier* classifier,
                                  bool* ok) {
    if (FLAG_harmony_explicit_tailcalls &&
        classifier->has_tail_call_expression()) {
      ReportClassifierError(classifier->tail_call_expression_error());
      *ok = false;
    }
  }

  void ExpressionUnexpectedToken(ExpressionClassifier* classifier) {
    MessageTemplate::Template message = MessageTemplate::kUnexpectedToken;
    const char* arg;
    Scanner::Location location = scanner()->peek_location();
    GetUnexpectedTokenMessage(peek(), &message, &location, &arg);
    classifier->RecordExpressionError(location, message, arg);
  }

  void BindingPatternUnexpectedToken(ExpressionClassifier* classifier) {
    MessageTemplate::Template message = MessageTemplate::kUnexpectedToken;
    const char* arg;
    Scanner::Location location = scanner()->peek_location();
    GetUnexpectedTokenMessage(peek(), &message, &location, &arg);
    classifier->RecordBindingPatternError(location, message, arg);
  }

  void ArrowFormalParametersUnexpectedToken(ExpressionClassifier* classifier) {
    MessageTemplate::Template message = MessageTemplate::kUnexpectedToken;
    const char* arg;
    Scanner::Location location = scanner()->peek_location();
    GetUnexpectedTokenMessage(peek(), &message, &location, &arg);
    classifier->RecordArrowFormalParametersError(location, message, arg);
  }

  // Recursive descent functions:

  // Parses an identifier that is valid for the current scope, in particular it
  // fails on strict mode future reserved keywords in a strict scope. If
  // allow_eval_or_arguments is kAllowEvalOrArguments, we allow "eval" or
  // "arguments" as identifier even in strict mode (this is needed in cases like
  // "var foo = eval;").
  IdentifierT ParseIdentifier(AllowRestrictedIdentifiers, bool* ok);
  IdentifierT ParseAndClassifyIdentifier(ExpressionClassifier* classifier,
                                         bool* ok);
  // Parses an identifier or a strict mode future reserved word, and indicate
  // whether it is strict mode future reserved. Allows passing in is_generator
  // for the case of parsing the identifier in a function expression, where the
  // relevant "is_generator" bit is of the function being parsed, not the
  // containing
  // function.
  IdentifierT ParseIdentifierOrStrictReservedWord(bool is_generator,
                                                  bool* is_strict_reserved,
                                                  bool* ok);
  IdentifierT ParseIdentifierOrStrictReservedWord(bool* is_strict_reserved,
                                                  bool* ok) {
    return ParseIdentifierOrStrictReservedWord(this->is_generator(),
                                               is_strict_reserved, ok);
  }

  IdentifierT ParseIdentifierName(bool* ok);

  ExpressionT ParseRegExpLiteral(bool seen_equal,
                                 ExpressionClassifier* classifier, bool* ok);

  ExpressionT ParsePrimaryExpression(ExpressionClassifier* classifier,
                                     bool* is_async, bool* ok);
  ExpressionT ParsePrimaryExpression(ExpressionClassifier* classifier,
                                     bool* ok) {
    bool is_async;
    return ParsePrimaryExpression(classifier, &is_async, ok);
  }
  ExpressionT ParseExpression(bool accept_IN, bool* ok);
  ExpressionT ParseExpression(bool accept_IN, ExpressionClassifier* classifier,
                              bool* ok);
  ExpressionT ParseArrayLiteral(ExpressionClassifier* classifier, bool* ok);
  ExpressionT ParsePropertyName(IdentifierT* name, bool* is_get, bool* is_set,
                                bool* is_await, bool* is_computed_name,
                                ExpressionClassifier* classifier, bool* ok);
  ExpressionT ParseObjectLiteral(ExpressionClassifier* classifier, bool* ok);
  ObjectLiteralPropertyT ParsePropertyDefinition(
      ObjectLiteralCheckerBase* checker, bool in_class, bool has_extends,
      MethodKind kind, bool* is_computed_name, bool* has_seen_constructor,
      ExpressionClassifier* classifier, IdentifierT* name, bool* ok);
  typename Traits::Type::ExpressionList ParseArguments(
      Scanner::Location* first_spread_pos, bool maybe_arrow,
      ExpressionClassifier* classifier, bool* ok);
  typename Traits::Type::ExpressionList ParseArguments(
      Scanner::Location* first_spread_pos, ExpressionClassifier* classifier,
      bool* ok) {
    return ParseArguments(first_spread_pos, false, classifier, ok);
  }

  ExpressionT ParseAssignmentExpression(bool accept_IN,
                                        ExpressionClassifier* classifier,
                                        bool* ok);
  ExpressionT ParseYieldExpression(bool accept_IN,
                                   ExpressionClassifier* classifier, bool* ok);
  ExpressionT ParseTailCallExpression(ExpressionClassifier* classifier,
                                      bool* ok);
  ExpressionT ParseConditionalExpression(bool accept_IN,
                                         ExpressionClassifier* classifier,
                                         bool* ok);
  ExpressionT ParseBinaryExpression(int prec, bool accept_IN,
                                    ExpressionClassifier* classifier, bool* ok);
  ExpressionT ParseUnaryExpression(ExpressionClassifier* classifier, bool* ok);
  ExpressionT ParsePostfixExpression(ExpressionClassifier* classifier,
                                     bool* ok);
  ExpressionT ParseLeftHandSideExpression(ExpressionClassifier* classifier,
                                          bool* ok);
  ExpressionT ParseMemberWithNewPrefixesExpression(
      ExpressionClassifier* classifier, bool* is_async, bool* ok);
  ExpressionT ParseMemberExpression(ExpressionClassifier* classifier,
                                    bool* is_async, bool* ok);
  ExpressionT ParseMemberExpressionContinuation(
      ExpressionT expression, bool* is_async, ExpressionClassifier* classifier,
      bool* ok);
  ExpressionT ParseArrowFunctionLiteral(bool accept_IN,
                                        const FormalParametersT& parameters,
                                        bool is_async,
                                        const ExpressionClassifier& classifier,
                                        bool* ok);
  ExpressionT ParseTemplateLiteral(ExpressionT tag, int start,
                                   ExpressionClassifier* classifier, bool* ok);
  void AddTemplateExpression(ExpressionT);
  ExpressionT ParseSuperExpression(bool is_new,
                                   ExpressionClassifier* classifier, bool* ok);
  ExpressionT ParseNewTargetExpression(bool* ok);

  void ParseFormalParameter(FormalParametersT* parameters,
                            ExpressionClassifier* classifier, bool* ok);
  void ParseFormalParameterList(FormalParametersT* parameters,
                                ExpressionClassifier* classifier, bool* ok);
  void CheckArityRestrictions(int param_count, FunctionKind function_type,
                              bool has_rest, int formals_start_pos,
                              int formals_end_pos, bool* ok);

  bool IsNextLetKeyword();

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
    if (!Traits::IsIdentifier(expression)) return false;
    if (is_strict(language_mode()) &&
        Traits::IsEvalOrArguments(Traits::AsIdentifier(expression))) {
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
  void CheckPossibleEvalCall(ExpressionT expression, Scope* scope) {
    if (Traits::IsIdentifier(expression) &&
        Traits::IsEval(Traits::AsIdentifier(expression))) {
      scope->RecordEvalCall();
      if (is_sloppy(scope->language_mode())) {
        // For sloppy scopes we also have to record the call at function level,
        // in case it includes declarations that will be hoisted.
        scope->DeclarationScope()->RecordEvalCall();
      }
    }
  }

  // Used to validate property names in object literals and class literals
  enum PropertyKind {
    kAccessorProperty,
    kValueProperty,
    kMethodProperty
  };

  class ObjectLiteralCheckerBase {
   public:
    explicit ObjectLiteralCheckerBase(ParserBase* parser) : parser_(parser) {}

    virtual void CheckProperty(Token::Value property, PropertyKind type,
                               MethodKind method_type, bool* ok) = 0;

    virtual ~ObjectLiteralCheckerBase() {}

   protected:
    ParserBase* parser() const { return parser_; }
    Scanner* scanner() const { return parser_->scanner(); }

   private:
    ParserBase* parser_;
  };

  // Validation per ES6 object literals.
  class ObjectLiteralChecker : public ObjectLiteralCheckerBase {
   public:
    explicit ObjectLiteralChecker(ParserBase* parser)
        : ObjectLiteralCheckerBase(parser), has_seen_proto_(false) {}

    void CheckProperty(Token::Value property, PropertyKind type,
                       MethodKind method_type, bool* ok) override;

   private:
    bool IsProto() { return this->scanner()->LiteralMatches("__proto__", 9); }

    bool has_seen_proto_;
  };

  // Validation per ES6 class literals.
  class ClassLiteralChecker : public ObjectLiteralCheckerBase {
   public:
    explicit ClassLiteralChecker(ParserBase* parser)
        : ObjectLiteralCheckerBase(parser), has_seen_constructor_(false) {}

    void CheckProperty(Token::Value property, PropertyKind type,
                       MethodKind method_type, bool* ok) override;

   private:
    bool IsConstructor() {
      return this->scanner()->LiteralMatches("constructor", 11);
    }
    bool IsPrototype() {
      return this->scanner()->LiteralMatches("prototype", 9);
    }

    bool has_seen_constructor_;
  };

  Scope* scope_;                   // Scope stack.
  FunctionState* function_state_;  // Function state stack.
  v8::Extension* extension_;
  FuncNameInferrer* fni_;
  AstValueFactory* ast_value_factory_;  // Not owned.
  ParserRecorder* log_;
  Mode mode_;
  bool parsing_module_;
  uintptr_t stack_limit_;

 private:
  Zone* zone_;

  Scanner* scanner_;
  bool stack_overflow_;

  bool allow_lazy_;
  bool allow_natives_;
  bool allow_tailcalls_;
  bool allow_harmony_restrictive_declarations_;
  bool allow_harmony_do_expressions_;
  bool allow_harmony_for_in_;
  bool allow_harmony_function_name_;
  bool allow_harmony_function_sent_;
  bool allow_harmony_async_await_;
};

template <class Traits>
ParserBase<Traits>::FunctionState::FunctionState(
    FunctionState** function_state_stack, Scope** scope_stack, Scope* scope,
    FunctionKind kind, typename Traits::Type::Factory* factory)
    : next_materialized_literal_index_(0),
      expected_property_count_(0),
      this_location_(Scanner::Location::invalid()),
      return_location_(Scanner::Location::invalid()),
      super_location_(Scanner::Location::invalid()),
      kind_(kind),
      generator_object_variable_(NULL),
      function_state_stack_(function_state_stack),
      outer_function_state_(*function_state_stack),
      scope_stack_(scope_stack),
      outer_scope_(*scope_stack),
      tail_call_expressions_(scope->zone()),
      return_expr_context_(ReturnExprContext::kInsideValidBlock),
      non_patterns_to_rewrite_(0, scope->zone()),
      factory_(factory),
      next_function_is_parenthesized_(false),
      this_function_is_parenthesized_(false) {
  *scope_stack_ = scope;
  *function_state_stack = this;
  if (outer_function_state_) {
    this_function_is_parenthesized_ =
        outer_function_state_->next_function_is_parenthesized_;
    outer_function_state_->next_function_is_parenthesized_ = false;
  }
}


template <class Traits>
ParserBase<Traits>::FunctionState::~FunctionState() {
  *scope_stack_ = outer_scope_;
  *function_state_stack_ = outer_function_state_;
}

template <class Traits>
void ParserBase<Traits>::GetUnexpectedTokenMessage(
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
    default:
      const char* name = Token::String(token);
      DCHECK(name != NULL);
      *arg = name;
      break;
  }
}


template <class Traits>
void ParserBase<Traits>::ReportUnexpectedToken(Token::Value token) {
  return ReportUnexpectedTokenAt(scanner_->location(), token);
}


template <class Traits>
void ParserBase<Traits>::ReportUnexpectedTokenAt(
    Scanner::Location source_location, Token::Value token,
    MessageTemplate::Template message) {
  const char* arg;
  GetUnexpectedTokenMessage(token, &message, &source_location, &arg);
  Traits::ReportMessageAt(source_location, message, arg);
}


template <class Traits>
typename ParserBase<Traits>::IdentifierT ParserBase<Traits>::ParseIdentifier(
    AllowRestrictedIdentifiers allow_restricted_identifiers, bool* ok) {
  ExpressionClassifier classifier(this);
  auto result = ParseAndClassifyIdentifier(&classifier, ok);
  if (!*ok) return Traits::EmptyIdentifier();

  if (allow_restricted_identifiers == kDontAllowRestrictedIdentifiers) {
    ValidateAssignmentPattern(&classifier, ok);
    if (!*ok) return Traits::EmptyIdentifier();
    ValidateBindingPattern(&classifier, ok);
    if (!*ok) return Traits::EmptyIdentifier();
  }

  return result;
}


template <class Traits>
typename ParserBase<Traits>::IdentifierT
ParserBase<Traits>::ParseAndClassifyIdentifier(ExpressionClassifier* classifier,
                                               bool* ok) {
  Token::Value next = Next();
  if (next == Token::IDENTIFIER || next == Token::ASYNC ||
      (next == Token::AWAIT && !parsing_module_)) {
    IdentifierT name = this->GetSymbol(scanner());
    // When this function is used to read a formal parameter, we don't always
    // know whether the function is going to be strict or sloppy.  Indeed for
    // arrow functions we don't always know that the identifier we are reading
    // is actually a formal parameter.  Therefore besides the errors that we
    // must detect because we know we're in strict mode, we also record any
    // error that we might make in the future once we know the language mode.
    if (this->IsEval(name)) {
      classifier->RecordStrictModeFormalParameterError(
          scanner()->location(), MessageTemplate::kStrictEvalArguments);
      if (is_strict(language_mode())) {
        classifier->RecordBindingPatternError(
            scanner()->location(), MessageTemplate::kStrictEvalArguments);
      }
    }
    if (this->IsArguments(name)) {
      scope_->RecordArgumentsUsage();
      classifier->RecordStrictModeFormalParameterError(
          scanner()->location(), MessageTemplate::kStrictEvalArguments);
      if (is_strict(language_mode())) {
        classifier->RecordBindingPatternError(
            scanner()->location(), MessageTemplate::kStrictEvalArguments);
      }
    }
    if (this->IsAwait(name)) {
      if (is_async_function()) {
        classifier->RecordPatternError(
            scanner()->location(), MessageTemplate::kAwaitBindingIdentifier);
      }
      classifier->RecordAsyncArrowFormalParametersError(
          scanner()->location(), MessageTemplate::kAwaitBindingIdentifier);
    }

    if (classifier->duplicate_finder() != nullptr &&
        scanner()->FindSymbol(classifier->duplicate_finder(), 1) != 0) {
      classifier->RecordDuplicateFormalParameterError(scanner()->location());
    }
    return name;
  } else if (is_sloppy(language_mode()) &&
             (next == Token::FUTURE_STRICT_RESERVED_WORD ||
              next == Token::ESCAPED_STRICT_RESERVED_WORD ||
              next == Token::LET || next == Token::STATIC ||
              (next == Token::YIELD && !is_generator()))) {
    classifier->RecordStrictModeFormalParameterError(
        scanner()->location(), MessageTemplate::kUnexpectedStrictReserved);
    if (next == Token::ESCAPED_STRICT_RESERVED_WORD &&
        is_strict(language_mode())) {
      ReportUnexpectedToken(next);
      *ok = false;
      return Traits::EmptyIdentifier();
    }
    if (next == Token::LET ||
        (next == Token::ESCAPED_STRICT_RESERVED_WORD &&
         scanner()->is_literal_contextual_keyword(CStrVector("let")))) {
      classifier->RecordLetPatternError(scanner()->location(),
                                        MessageTemplate::kLetInLexicalBinding);
    }
    return this->GetSymbol(scanner());
  } else {
    this->ReportUnexpectedToken(next);
    *ok = false;
    return Traits::EmptyIdentifier();
  }
}


template <class Traits>
typename ParserBase<Traits>::IdentifierT
ParserBase<Traits>::ParseIdentifierOrStrictReservedWord(
    bool is_generator, bool* is_strict_reserved, bool* ok) {
  Token::Value next = Next();
  if (next == Token::IDENTIFIER || (next == Token::AWAIT && !parsing_module_) ||
      next == Token::ASYNC) {
    *is_strict_reserved = false;
  } else if (next == Token::FUTURE_STRICT_RESERVED_WORD || next == Token::LET ||
             next == Token::STATIC || (next == Token::YIELD && !is_generator)) {
    *is_strict_reserved = true;
  } else {
    ReportUnexpectedToken(next);
    *ok = false;
    return Traits::EmptyIdentifier();
  }

  IdentifierT name = this->GetSymbol(scanner());
  if (this->IsArguments(name)) scope_->RecordArgumentsUsage();
  return name;
}

template <class Traits>
typename ParserBase<Traits>::IdentifierT
ParserBase<Traits>::ParseIdentifierName(bool* ok) {
  Token::Value next = Next();
  if (next != Token::IDENTIFIER && next != Token::ASYNC &&
      next != Token::ENUM && next != Token::AWAIT && next != Token::LET &&
      next != Token::STATIC && next != Token::YIELD &&
      next != Token::FUTURE_STRICT_RESERVED_WORD &&
      next != Token::ESCAPED_KEYWORD &&
      next != Token::ESCAPED_STRICT_RESERVED_WORD && !Token::IsKeyword(next)) {
    this->ReportUnexpectedToken(next);
    *ok = false;
    return Traits::EmptyIdentifier();
  }

  IdentifierT name = this->GetSymbol(scanner());
  if (this->IsArguments(name)) scope_->RecordArgumentsUsage();
  return name;
}


template <class Traits>
typename ParserBase<Traits>::ExpressionT ParserBase<Traits>::ParseRegExpLiteral(
    bool seen_equal, ExpressionClassifier* classifier, bool* ok) {
  int pos = peek_position();
  if (!scanner()->ScanRegExpPattern(seen_equal)) {
    Next();
    ReportMessage(MessageTemplate::kUnterminatedRegExp);
    *ok = false;
    return Traits::EmptyExpression();
  }

  int literal_index = function_state_->NextMaterializedLiteralIndex();

  IdentifierT js_pattern = this->GetNextSymbol(scanner());
  Maybe<RegExp::Flags> flags = scanner()->ScanRegExpFlags();
  if (flags.IsNothing()) {
    Next();
    ReportMessage(MessageTemplate::kMalformedRegExpFlags);
    *ok = false;
    return Traits::EmptyExpression();
  }
  int js_flags = flags.FromJust();
  Next();
  return factory()->NewRegExpLiteral(js_pattern, js_flags, literal_index, pos);
}


#define CHECK_OK  ok); \
  if (!*ok) return this->EmptyExpression(); \
  ((void)0
#define DUMMY )  // to make indentation work
#undef DUMMY

// Used in functions where the return type is not ExpressionT.
#define CHECK_OK_CUSTOM(x) ok); \
  if (!*ok) return this->x(); \
  ((void)0
#define DUMMY )  // to make indentation work
#undef DUMMY

template <class Traits>
typename ParserBase<Traits>::ExpressionT
ParserBase<Traits>::ParsePrimaryExpression(ExpressionClassifier* classifier,
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
  //   AsyncFunctionExpression

  int beg_pos = peek_position();
  switch (peek()) {
    case Token::THIS: {
      BindingPatternUnexpectedToken(classifier);
      Consume(Token::THIS);
      return this->ThisExpression(scope_, factory(), beg_pos);
    }

    case Token::NULL_LITERAL:
    case Token::TRUE_LITERAL:
    case Token::FALSE_LITERAL:
      BindingPatternUnexpectedToken(classifier);
      return this->ExpressionFromLiteral(Next(), beg_pos, scanner(), factory());
    case Token::SMI:
    case Token::NUMBER:
      BindingPatternUnexpectedToken(classifier);
      return this->ExpressionFromLiteral(Next(), beg_pos, scanner(), factory());

    case Token::ASYNC:
      if (allow_harmony_async_await() &&
          !scanner()->HasAnyLineTerminatorAfterNext() &&
          PeekAhead() == Token::FUNCTION) {
        Consume(Token::ASYNC);
        return this->ParseAsyncFunctionExpression(CHECK_OK);
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
      IdentifierT name = ParseAndClassifyIdentifier(classifier, CHECK_OK);
      return this->ExpressionFromIdentifier(
          name, beg_pos, scanner()->location().end_pos, scope_, factory());
    }

    case Token::STRING: {
      BindingPatternUnexpectedToken(classifier);
      Consume(Token::STRING);
      return this->ExpressionFromString(beg_pos, scanner(), factory());
    }

    case Token::ASSIGN_DIV:
      classifier->RecordBindingPatternError(
          scanner()->peek_location(), MessageTemplate::kUnexpectedTokenRegExp);
      return this->ParseRegExpLiteral(true, classifier, ok);

    case Token::DIV:
      classifier->RecordBindingPatternError(
          scanner()->peek_location(), MessageTemplate::kUnexpectedTokenRegExp);
      return this->ParseRegExpLiteral(false, classifier, ok);

    case Token::LBRACK:
      return this->ParseArrayLiteral(classifier, ok);

    case Token::LBRACE:
      return this->ParseObjectLiteral(classifier, ok);

    case Token::LPAREN: {
      // Arrow function formal parameters are either a single identifier or a
      // list of BindingPattern productions enclosed in parentheses.
      // Parentheses are not valid on the LHS of a BindingPattern, so we use the
      // is_valid_binding_pattern() check to detect multiple levels of
      // parenthesization.
      if (!classifier->is_valid_binding_pattern()) {
        ArrowFormalParametersUnexpectedToken(classifier);
      }
      classifier->RecordPatternError(scanner()->peek_location(),
                                     MessageTemplate::kUnexpectedToken,
                                     Token::String(Token::LPAREN));
      Consume(Token::LPAREN);
      if (Check(Token::RPAREN)) {
        // ()=>x.  The continuation that looks for the => is in
        // ParseAssignmentExpression.
        classifier->RecordExpressionError(scanner()->location(),
                                          MessageTemplate::kUnexpectedToken,
                                          Token::String(Token::RPAREN));
        return factory()->NewEmptyParentheses(beg_pos);
      } else if (Check(Token::ELLIPSIS)) {
        // (...x)=>x.  The continuation that looks for the => is in
        // ParseAssignmentExpression.
        int ellipsis_pos = position();
        int expr_pos = peek_position();
        classifier->RecordExpressionError(scanner()->location(),
                                          MessageTemplate::kUnexpectedToken,
                                          Token::String(Token::ELLIPSIS));
        classifier->RecordNonSimpleParameter();
        ExpressionT expr =
            this->ParseAssignmentExpression(true, classifier, CHECK_OK);
        if (!this->IsIdentifier(expr) && !IsValidPattern(expr)) {
          classifier->RecordArrowFormalParametersError(
              Scanner::Location(ellipsis_pos, scanner()->location().end_pos),
              MessageTemplate::kInvalidRestParameter);
        }
        if (peek() == Token::COMMA) {
          ReportMessageAt(scanner()->peek_location(),
                          MessageTemplate::kParamAfterRest);
          *ok = false;
          return this->EmptyExpression();
        }
        Expect(Token::RPAREN, CHECK_OK);
        return factory()->NewSpread(expr, ellipsis_pos, expr_pos);
      }
      // Heuristically try to detect immediately called functions before
      // seeing the call parentheses.
      function_state_->next_function_is_parenthesized(peek() ==
                                                      Token::FUNCTION);
      ExpressionT expr = this->ParseExpression(true, classifier, CHECK_OK);
      Expect(Token::RPAREN, CHECK_OK);
      return expr;
    }

    case Token::CLASS: {
      BindingPatternUnexpectedToken(classifier);
      Consume(Token::CLASS);
      int class_token_position = position();
      IdentifierT name = this->EmptyIdentifier();
      bool is_strict_reserved_name = false;
      Scanner::Location class_name_location = Scanner::Location::invalid();
      if (peek_any_identifier()) {
        name = ParseIdentifierOrStrictReservedWord(&is_strict_reserved_name,
                                                   CHECK_OK);
        class_name_location = scanner()->location();
      }
      return this->ParseClassLiteral(classifier, name, class_name_location,
                                     is_strict_reserved_name,
                                     class_token_position, ok);
    }

    case Token::TEMPLATE_SPAN:
    case Token::TEMPLATE_TAIL:
      BindingPatternUnexpectedToken(classifier);
      return this->ParseTemplateLiteral(Traits::NoTemplateTag(), beg_pos,
                                        classifier, ok);

    case Token::MOD:
      if (allow_natives() || extension_ != NULL) {
        BindingPatternUnexpectedToken(classifier);
        return this->ParseV8Intrinsic(ok);
      }
      break;

    case Token::DO:
      if (allow_harmony_do_expressions()) {
        BindingPatternUnexpectedToken(classifier);
        return Traits::ParseDoExpression(ok);
      }
      break;

    default:
      break;
  }

  ReportUnexpectedToken(Next());
  *ok = false;
  return this->EmptyExpression();
}


template <class Traits>
typename ParserBase<Traits>::ExpressionT ParserBase<Traits>::ParseExpression(
    bool accept_IN, bool* ok) {
  ExpressionClassifier classifier(this);
  ExpressionT result = ParseExpression(accept_IN, &classifier, CHECK_OK);
  Traits::RewriteNonPattern(&classifier, CHECK_OK);
  return result;
}

template <class Traits>
typename ParserBase<Traits>::ExpressionT ParserBase<Traits>::ParseExpression(
    bool accept_IN, ExpressionClassifier* classifier, bool* ok) {
  // Expression ::
  //   AssignmentExpression
  //   Expression ',' AssignmentExpression

  ExpressionClassifier binding_classifier(this);
  ExpressionT result =
      this->ParseAssignmentExpression(accept_IN, &binding_classifier, CHECK_OK);
  classifier->Accumulate(&binding_classifier,
                         ExpressionClassifier::AllProductions);
  bool is_simple_parameter_list = this->IsIdentifier(result);
  bool seen_rest = false;
  while (peek() == Token::COMMA) {
    CheckNoTailCallExpressions(classifier, CHECK_OK);
    if (seen_rest) {
      // At this point the production can't possibly be valid, but we don't know
      // which error to signal.
      classifier->RecordArrowFormalParametersError(
          scanner()->peek_location(), MessageTemplate::kParamAfterRest);
    }
    Consume(Token::COMMA);
    bool is_rest = false;
    if (peek() == Token::ELLIPSIS) {
      // 'x, y, ...z' in CoverParenthesizedExpressionAndArrowParameterList only
      // as the formal parameters of'(x, y, ...z) => foo', and is not itself a
      // valid expression or binding pattern.
      ExpressionUnexpectedToken(classifier);
      BindingPatternUnexpectedToken(classifier);
      Consume(Token::ELLIPSIS);
      seen_rest = is_rest = true;
    }
    int pos = position(), expr_pos = peek_position();
    ExpressionT right = this->ParseAssignmentExpression(
        accept_IN, &binding_classifier, CHECK_OK);
    classifier->Accumulate(&binding_classifier,
                           ExpressionClassifier::AllProductions);
    if (is_rest) {
      if (!this->IsIdentifier(right) && !IsValidPattern(right)) {
        classifier->RecordArrowFormalParametersError(
            Scanner::Location(pos, scanner()->location().end_pos),
            MessageTemplate::kInvalidRestParameter);
      }
      right = factory()->NewSpread(right, pos, expr_pos);
    }
    is_simple_parameter_list =
        is_simple_parameter_list && this->IsIdentifier(right);
    result = factory()->NewBinaryOperation(Token::COMMA, result, right, pos);
  }
  if (!is_simple_parameter_list || seen_rest) {
    classifier->RecordNonSimpleParameter();
  }

  return result;
}


template <class Traits>
typename ParserBase<Traits>::ExpressionT ParserBase<Traits>::ParseArrayLiteral(
    ExpressionClassifier* classifier, bool* ok) {
  // ArrayLiteral ::
  //   '[' Expression? (',' Expression?)* ']'

  int pos = peek_position();
  typename Traits::Type::ExpressionList values =
      this->NewExpressionList(4, zone_);
  int first_spread_index = -1;
  Expect(Token::LBRACK, CHECK_OK);
  while (peek() != Token::RBRACK) {
    ExpressionT elem = this->EmptyExpression();
    if (peek() == Token::COMMA) {
      elem = this->GetLiteralTheHole(peek_position(), factory());
    } else if (peek() == Token::ELLIPSIS) {
      int start_pos = peek_position();
      Consume(Token::ELLIPSIS);
      int expr_pos = peek_position();
      ExpressionT argument =
          this->ParseAssignmentExpression(true, classifier, CHECK_OK);
      CheckNoTailCallExpressions(classifier, CHECK_OK);
      elem = factory()->NewSpread(argument, start_pos, expr_pos);

      if (first_spread_index < 0) {
        first_spread_index = values->length();
      }

      if (argument->IsAssignment()) {
        classifier->RecordPatternError(
            Scanner::Location(start_pos, scanner()->location().end_pos),
            MessageTemplate::kInvalidDestructuringTarget);
      } else {
        CheckDestructuringElement(argument, classifier, start_pos,
                                  scanner()->location().end_pos);
      }

      if (peek() == Token::COMMA) {
        classifier->RecordPatternError(
            Scanner::Location(start_pos, scanner()->location().end_pos),
            MessageTemplate::kElementAfterRest);
      }
    } else {
      int beg_pos = peek_position();
      elem = this->ParseAssignmentExpression(true, classifier, CHECK_OK);
      CheckNoTailCallExpressions(classifier, CHECK_OK);
      CheckDestructuringElement(elem, classifier, beg_pos,
                                scanner()->location().end_pos);
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
    Traits::QueueNonPatternForRewriting(result);
  }
  return result;
}

template <class Traits>
typename ParserBase<Traits>::ExpressionT ParserBase<Traits>::ParsePropertyName(
    IdentifierT* name, bool* is_get, bool* is_set, bool* is_await,
    bool* is_computed_name, ExpressionClassifier* classifier, bool* ok) {
  Token::Value token = peek();
  int pos = peek_position();

  // For non computed property names we normalize the name a bit:
  //
  //   "12" -> 12
  //   12.3 -> "12.3"
  //   12.30 -> "12.3"
  //   identifier -> "identifier"
  //
  // This is important because we use the property name as a key in a hash
  // table when we compute constant properties.
  switch (token) {
    case Token::STRING:
      Consume(Token::STRING);
      *name = this->GetSymbol(scanner());
      break;

    case Token::SMI:
      Consume(Token::SMI);
      *name = this->GetNumberAsSymbol(scanner());
      break;

    case Token::NUMBER:
      Consume(Token::NUMBER);
      *name = this->GetNumberAsSymbol(scanner());
      break;

    case Token::LBRACK: {
      *is_computed_name = true;
      Consume(Token::LBRACK);
      ExpressionClassifier computed_name_classifier(this);
      ExpressionT expression =
          ParseAssignmentExpression(true, &computed_name_classifier, CHECK_OK);
      Traits::RewriteNonPattern(&computed_name_classifier, CHECK_OK);
      classifier->Accumulate(&computed_name_classifier,
                             ExpressionClassifier::ExpressionProductions);
      Expect(Token::RBRACK, CHECK_OK);
      return expression;
    }

    default:
      *name = ParseIdentifierName(CHECK_OK);
      scanner()->IsGetOrSet(is_get, is_set);
      if (this->IsAwait(*name)) {
        *is_await = true;
      }
      break;
  }

  uint32_t index;
  return this->IsArrayIndex(*name, &index)
             ? factory()->NewNumberLiteral(index, pos)
             : factory()->NewStringLiteral(*name, pos);
}

template <class Traits>
typename ParserBase<Traits>::ObjectLiteralPropertyT
ParserBase<Traits>::ParsePropertyDefinition(
    ObjectLiteralCheckerBase* checker, bool in_class, bool has_extends,
    MethodKind method_kind, bool* is_computed_name, bool* has_seen_constructor,
    ExpressionClassifier* classifier, IdentifierT* name, bool* ok) {
  DCHECK(!in_class || IsStaticMethod(method_kind) ||
         has_seen_constructor != nullptr);
  ExpressionT value = this->EmptyExpression();
  bool is_get = false;
  bool is_set = false;
  bool is_await = false;
  bool is_generator = Check(Token::MUL);
  bool is_async = false;
  const bool is_static = IsStaticMethod(method_kind);

  Token::Value name_token = peek();

  if (is_generator) {
    method_kind |= MethodKind::Generator;
  } else if (allow_harmony_async_await() && name_token == Token::ASYNC &&
             !scanner()->HasAnyLineTerminatorAfterNext() &&
             PeekAhead() != Token::LPAREN && PeekAhead()) {
    is_async = true;
  }

  int next_beg_pos = scanner()->peek_location().beg_pos;
  int next_end_pos = scanner()->peek_location().end_pos;
  ExpressionT name_expression = ParsePropertyName(
      name, &is_get, &is_set, &is_await, is_computed_name, classifier,
      CHECK_OK_CUSTOM(EmptyObjectLiteralProperty));

  if (fni_ != nullptr && !*is_computed_name) {
    this->PushLiteralName(fni_, *name);
  }

  if (!in_class && !is_generator) {
    DCHECK(!IsStaticMethod(method_kind));

    if (peek() == Token::COLON) {
      // PropertyDefinition
      //    PropertyName ':' AssignmentExpression
      if (!*is_computed_name) {
        checker->CheckProperty(name_token, kValueProperty, MethodKind::Normal,
                               CHECK_OK_CUSTOM(EmptyObjectLiteralProperty));
      }
      Consume(Token::COLON);
      int beg_pos = peek_position();
      value = this->ParseAssignmentExpression(
          true, classifier, CHECK_OK_CUSTOM(EmptyObjectLiteralProperty));
      CheckDestructuringElement(value, classifier, beg_pos,
                                scanner()->location().end_pos);

      return factory()->NewObjectLiteralProperty(name_expression, value,
                                                 is_static, *is_computed_name);
    }

    if (Token::IsIdentifier(name_token, language_mode(), this->is_generator(),
                            parsing_module_) &&
        (peek() == Token::COMMA || peek() == Token::RBRACE ||
         peek() == Token::ASSIGN)) {
      // PropertyDefinition
      //    IdentifierReference
      //    CoverInitializedName
      //
      // CoverInitializedName
      //    IdentifierReference Initializer?
      if (classifier->duplicate_finder() != nullptr &&
          scanner()->FindSymbol(classifier->duplicate_finder(), 1) != 0) {
        classifier->RecordDuplicateFormalParameterError(scanner()->location());
      }
      if (name_token == Token::LET) {
        classifier->RecordLetPatternError(
            scanner()->location(), MessageTemplate::kLetInLexicalBinding);
      }
      if (is_await && is_async_function()) {
        classifier->RecordPatternError(
            Scanner::Location(next_beg_pos, next_end_pos),
            MessageTemplate::kAwaitBindingIdentifier);
      }
      ExpressionT lhs = this->ExpressionFromIdentifier(
          *name, next_beg_pos, next_end_pos, scope_, factory());
      CheckDestructuringElement(lhs, classifier, next_beg_pos, next_end_pos);

      if (peek() == Token::ASSIGN) {
        Consume(Token::ASSIGN);
        ExpressionClassifier rhs_classifier(this);
        ExpressionT rhs = this->ParseAssignmentExpression(
            true, &rhs_classifier, CHECK_OK_CUSTOM(EmptyObjectLiteralProperty));
        Traits::RewriteNonPattern(&rhs_classifier,
                                  CHECK_OK_CUSTOM(EmptyObjectLiteralProperty));
        classifier->Accumulate(&rhs_classifier,
                               ExpressionClassifier::ExpressionProductions);
        value = factory()->NewAssignment(Token::ASSIGN, lhs, rhs,
                                         RelocInfo::kNoPosition);
        classifier->RecordCoverInitializedNameError(
            Scanner::Location(next_beg_pos, scanner()->location().end_pos),
            MessageTemplate::kInvalidCoverInitializedName);

        if (allow_harmony_function_name()) {
          Traits::SetFunctionNameFromIdentifierRef(rhs, lhs);
        }
      } else {
        value = lhs;
      }

      return factory()->NewObjectLiteralProperty(
          name_expression, value, ObjectLiteralProperty::COMPUTED, is_static,
          false);
    }
  }

  // Method definitions are never valid in patterns.
  classifier->RecordPatternError(
      Scanner::Location(next_beg_pos, scanner()->location().end_pos),
      MessageTemplate::kInvalidDestructuringTarget);

  if (is_async && !IsSpecialMethod(method_kind)) {
    DCHECK(!is_get);
    DCHECK(!is_set);
    bool dont_care;
    name_expression = ParsePropertyName(
        name, &dont_care, &dont_care, &dont_care, is_computed_name, classifier,
        CHECK_OK_CUSTOM(EmptyObjectLiteralProperty));
    method_kind |= MethodKind::Async;
  }

  if (is_generator || peek() == Token::LPAREN) {
    // MethodDefinition
    //    PropertyName '(' StrictFormalParameters ')' '{' FunctionBody '}'
    //    '*' PropertyName '(' StrictFormalParameters ')' '{' FunctionBody '}'
    if (!*is_computed_name) {
      checker->CheckProperty(name_token, kMethodProperty, method_kind,
                             CHECK_OK_CUSTOM(EmptyObjectLiteralProperty));
    }

    FunctionKind kind = is_generator
                            ? FunctionKind::kConciseGeneratorMethod
                            : is_async ? FunctionKind::kAsyncConciseMethod
                                       : FunctionKind::kConciseMethod;

    if (in_class && !IsStaticMethod(method_kind) &&
        this->IsConstructor(*name)) {
      *has_seen_constructor = true;
      kind = has_extends ? FunctionKind::kSubclassConstructor
                         : FunctionKind::kBaseConstructor;
    }

    value = this->ParseFunctionLiteral(
        *name, scanner()->location(), kSkipFunctionNameCheck, kind,
        RelocInfo::kNoPosition, FunctionLiteral::kAccessorOrMethod,
        language_mode(), CHECK_OK_CUSTOM(EmptyObjectLiteralProperty));

    return factory()->NewObjectLiteralProperty(name_expression, value,
                                               ObjectLiteralProperty::COMPUTED,
                                               is_static, *is_computed_name);
  }

  if (in_class && name_token == Token::STATIC && IsNormalMethod(method_kind)) {
    // ClassElement (static)
    //    'static' MethodDefinition
    *name = this->EmptyIdentifier();
    ObjectLiteralPropertyT property = ParsePropertyDefinition(
        checker, true, has_extends, MethodKind::Static, is_computed_name,
        nullptr, classifier, name, ok);
    Traits::RewriteNonPattern(classifier, ok);
    return property;
  }

  if (is_get || is_set) {
    // MethodDefinition (Accessors)
    //    get PropertyName '(' ')' '{' FunctionBody '}'
    //    set PropertyName '(' PropertySetParameterList ')' '{' FunctionBody '}'
    *name = this->EmptyIdentifier();
    bool dont_care = false;
    name_token = peek();

    name_expression = ParsePropertyName(
        name, &dont_care, &dont_care, &dont_care, is_computed_name, classifier,
        CHECK_OK_CUSTOM(EmptyObjectLiteralProperty));

    if (!*is_computed_name) {
      checker->CheckProperty(name_token, kAccessorProperty, method_kind,
                             CHECK_OK_CUSTOM(EmptyObjectLiteralProperty));
    }

    typename Traits::Type::FunctionLiteral value = this->ParseFunctionLiteral(
        *name, scanner()->location(), kSkipFunctionNameCheck,
        is_get ? FunctionKind::kGetterFunction : FunctionKind::kSetterFunction,
        RelocInfo::kNoPosition, FunctionLiteral::kAccessorOrMethod,
        language_mode(), CHECK_OK_CUSTOM(EmptyObjectLiteralProperty));

    // Make sure the name expression is a string since we need a Name for
    // Runtime_DefineAccessorPropertyUnchecked and since we can determine this
    // statically we can skip the extra runtime check.
    if (!*is_computed_name) {
      name_expression =
          factory()->NewStringLiteral(*name, name_expression->position());
    }

    return factory()->NewObjectLiteralProperty(
        name_expression, value,
        is_get ? ObjectLiteralProperty::GETTER : ObjectLiteralProperty::SETTER,
        is_static, *is_computed_name);
  }

  Token::Value next = Next();
  ReportUnexpectedToken(next);
  *ok = false;
  return this->EmptyObjectLiteralProperty();
}


template <class Traits>
typename ParserBase<Traits>::ExpressionT ParserBase<Traits>::ParseObjectLiteral(
    ExpressionClassifier* classifier, bool* ok) {
  // ObjectLiteral ::
  // '{' (PropertyDefinition (',' PropertyDefinition)* ','? )? '}'

  int pos = peek_position();
  typename Traits::Type::PropertyList properties =
      this->NewPropertyList(4, zone_);
  int number_of_boilerplate_properties = 0;
  bool has_computed_names = false;
  ObjectLiteralChecker checker(this);

  Expect(Token::LBRACE, CHECK_OK);

  while (peek() != Token::RBRACE) {
    FuncNameInferrer::State fni_state(fni_);

    const bool in_class = false;
    const bool has_extends = false;
    bool is_computed_name = false;
    IdentifierT name = this->EmptyIdentifier();
    ObjectLiteralPropertyT property = this->ParsePropertyDefinition(
        &checker, in_class, has_extends, MethodKind::Normal, &is_computed_name,
        NULL, classifier, &name, CHECK_OK);

    if (is_computed_name) {
      has_computed_names = true;
    }

    // Count CONSTANT or COMPUTED properties to maintain the enumeration order.
    if (!has_computed_names && this->IsBoilerplateProperty(property)) {
      number_of_boilerplate_properties++;
    }
    properties->Add(property, zone());

    if (peek() != Token::RBRACE) {
      // Need {} because of the CHECK_OK macro.
      Expect(Token::COMMA, CHECK_OK);
    }

    if (fni_ != nullptr) fni_->Infer();

    if (allow_harmony_function_name()) {
      Traits::SetFunctionNameFromPropertyName(property, name);
    }
  }
  Expect(Token::RBRACE, CHECK_OK);

  // Computation of literal_index must happen before pre parse bailout.
  int literal_index = function_state_->NextMaterializedLiteralIndex();

  return factory()->NewObjectLiteral(properties,
                                     literal_index,
                                     number_of_boilerplate_properties,
                                     pos);
}

template <class Traits>
typename Traits::Type::ExpressionList ParserBase<Traits>::ParseArguments(
    Scanner::Location* first_spread_arg_loc, bool maybe_arrow,
    ExpressionClassifier* classifier, bool* ok) {
  // Arguments ::
  //   '(' (AssignmentExpression)*[','] ')'

  Scanner::Location spread_arg = Scanner::Location::invalid();
  typename Traits::Type::ExpressionList result =
      this->NewExpressionList(4, zone_);
  Expect(Token::LPAREN, CHECK_OK_CUSTOM(NullExpressionList));
  bool done = (peek() == Token::RPAREN);
  bool was_unspread = false;
  int unspread_sequences_count = 0;
  while (!done) {
    int start_pos = peek_position();
    bool is_spread = Check(Token::ELLIPSIS);
    int expr_pos = peek_position();

    ExpressionT argument = this->ParseAssignmentExpression(
        true, classifier, CHECK_OK_CUSTOM(NullExpressionList));
    CheckNoTailCallExpressions(classifier, CHECK_OK_CUSTOM(NullExpressionList));
    Traits::RewriteNonPattern(classifier, CHECK_OK_CUSTOM(NullExpressionList));
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
      return this->NullExpressionList();
    }
    done = (peek() != Token::COMMA);
    if (!done) {
      Next();
    }
  }
  Scanner::Location location = scanner_->location();
  if (Token::RPAREN != Next()) {
    ReportMessageAt(location, MessageTemplate::kUnterminatedArgList);
    *ok = false;
    return this->NullExpressionList();
  }
  *first_spread_arg_loc = spread_arg;

  if ((!maybe_arrow || peek() != Token::ARROW) && spread_arg.IsValid()) {
    // Unspread parameter sequences are translated into array literals in the
    // parser. Ensure that the number of materialized literals matches between
    // the parser and preparser
    Traits::MaterializeUnspreadArgumentsLiterals(unspread_sequences_count);
  }

  return result;
}

// Precedence = 2
template <class Traits>
typename ParserBase<Traits>::ExpressionT
ParserBase<Traits>::ParseAssignmentExpression(bool accept_IN,
                                              ExpressionClassifier* classifier,
                                              bool* ok) {
  // AssignmentExpression ::
  //   ConditionalExpression
  //   ArrowFunction
  //   YieldExpression
  //   LeftHandSideExpression AssignmentOperator AssignmentExpression
  bool is_destructuring_assignment = false;
  int lhs_beg_pos = peek_position();

  if (peek() == Token::YIELD && is_generator()) {
    return this->ParseYieldExpression(accept_IN, classifier, ok);
  }

  FuncNameInferrer::State fni_state(fni_);
  ParserBase<Traits>::Checkpoint checkpoint(this);
  ExpressionClassifier arrow_formals_classifier(this,
                                                classifier->duplicate_finder());

  bool is_async = allow_harmony_async_await() && peek() == Token::ASYNC &&
                  !scanner()->HasAnyLineTerminatorAfterNext();

  bool parenthesized_formals = peek() == Token::LPAREN;
  if (!is_async && !parenthesized_formals) {
    ArrowFormalParametersUnexpectedToken(&arrow_formals_classifier);
  }
  ExpressionT expression = this->ParseConditionalExpression(
      accept_IN, &arrow_formals_classifier, CHECK_OK);

  if (is_async && peek_any_identifier() && PeekAhead() == Token::ARROW) {
    // async Identifier => AsyncConciseBody
    IdentifierT name =
        ParseAndClassifyIdentifier(&arrow_formals_classifier, CHECK_OK);
    expression = this->ExpressionFromIdentifier(
        name, position(), scanner()->location().end_pos, scope_, factory());
  }

  if (peek() == Token::ARROW) {
    classifier->RecordPatternError(scanner()->peek_location(),
                                   MessageTemplate::kUnexpectedToken,
                                   Token::String(Token::ARROW));
    ValidateArrowFormalParameters(&arrow_formals_classifier, expression,
                                  parenthesized_formals, is_async, CHECK_OK);
    // This reads strangely, but is correct: it checks whether any
    // sub-expression of the parameter list failed to be a valid formal
    // parameter initializer. Since YieldExpressions are banned anywhere
    // in an arrow parameter list, this is correct.
    // TODO(adamk): Rename "FormalParameterInitializerError" to refer to
    // "YieldExpression", which is its only use.
    ValidateFormalParameterInitializer(&arrow_formals_classifier, ok);

    Scanner::Location loc(lhs_beg_pos, scanner()->location().end_pos);
    Scope* scope = this->NewScope(scope_, FUNCTION_SCOPE,
                                  is_async ? FunctionKind::kAsyncArrowFunction
                                           : FunctionKind::kArrowFunction);
    // Because the arrow's parameters were parsed in the outer scope, any
    // usage flags that might have been triggered there need to be copied
    // to the arrow scope.
    scope_->PropagateUsageFlagsToScope(scope);
    FormalParametersT parameters(scope);
    if (!arrow_formals_classifier.is_simple_parameter_list()) {
      scope->SetHasNonSimpleParameters();
      parameters.is_simple = false;
    }

    checkpoint.Restore(&parameters.materialized_literals_count);

    scope->set_start_position(lhs_beg_pos);
    Scanner::Location duplicate_loc = Scanner::Location::invalid();
    this->ParseArrowFunctionFormalParameterList(&parameters, expression, loc,
                                                &duplicate_loc, CHECK_OK);
    if (duplicate_loc.IsValid()) {
      arrow_formals_classifier.RecordDuplicateFormalParameterError(
          duplicate_loc);
    }
    expression = this->ParseArrowFunctionLiteral(
        accept_IN, parameters, is_async, arrow_formals_classifier, CHECK_OK);

    if (fni_ != nullptr) fni_->Infer();

    return expression;
  }

  if (this->IsValidReferenceExpression(expression)) {
    arrow_formals_classifier.ForgiveAssignmentPatternError();
  }

  // "expression" was not itself an arrow function parameter list, but it might
  // form part of one.  Propagate speculative formal parameter error locations.
  // Do not merge pending non-pattern expressions yet!
  classifier->Accumulate(
      &arrow_formals_classifier,
      ExpressionClassifier::StandardProductions |
          ExpressionClassifier::FormalParametersProductions |
          ExpressionClassifier::CoverInitializedNameProduction |
          ExpressionClassifier::AsyncArrowFormalParametersProduction |
          ExpressionClassifier::AsyncBindingPatternProduction,
      false);

  if (!Token::IsAssignmentOp(peek())) {
    // Parsed conditional expression only (no assignment).
    // Now pending non-pattern expressions must be merged.
    classifier->MergeNonPatterns(&arrow_formals_classifier);
    return expression;
  }

  // Now pending non-pattern expressions must be discarded.
  arrow_formals_classifier.Discard();

  CheckNoTailCallExpressions(classifier, CHECK_OK);

  if (IsValidPattern(expression) && peek() == Token::ASSIGN) {
    classifier->ForgiveCoverInitializedNameError();
    ValidateAssignmentPattern(classifier, CHECK_OK);
    is_destructuring_assignment = true;
  } else {
    expression = this->CheckAndRewriteReferenceExpression(
        expression, lhs_beg_pos, scanner()->location().end_pos,
        MessageTemplate::kInvalidLhsInAssignment, CHECK_OK);
  }

  expression = this->MarkExpressionAsAssigned(expression);

  Token::Value op = Next();  // Get assignment operator.
  if (op != Token::ASSIGN) {
    classifier->RecordPatternError(scanner()->location(),
                                   MessageTemplate::kUnexpectedToken,
                                   Token::String(op));
  }
  int pos = position();

  ExpressionClassifier rhs_classifier(this);

  ExpressionT right =
      this->ParseAssignmentExpression(accept_IN, &rhs_classifier, CHECK_OK);
  CheckNoTailCallExpressions(&rhs_classifier, CHECK_OK);
  Traits::RewriteNonPattern(&rhs_classifier, CHECK_OK);
  classifier->Accumulate(
      &rhs_classifier,
      ExpressionClassifier::ExpressionProductions |
          ExpressionClassifier::CoverInitializedNameProduction |
          ExpressionClassifier::AsyncArrowFormalParametersProduction);

  // TODO(1231235): We try to estimate the set of properties set by
  // constructors. We define a new property whenever there is an
  // assignment to a property of 'this'. We should probably only add
  // properties if we haven't seen them before. Otherwise we'll
  // probably overestimate the number of properties.
  if (op == Token::ASSIGN && this->IsThisProperty(expression)) {
    function_state_->AddProperty();
  }

  this->CheckAssigningFunctionLiteralToProperty(expression, right);

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

  if (op == Token::ASSIGN && allow_harmony_function_name()) {
    Traits::SetFunctionNameFromIdentifierRef(right, expression);
  }

  if (op == Token::ASSIGN_EXP) {
    DCHECK(!is_destructuring_assignment);
    return Traits::RewriteAssignExponentiation(expression, right, pos);
  }

  ExpressionT result = factory()->NewAssignment(op, expression, right, pos);

  if (is_destructuring_assignment) {
    result = factory()->NewRewritableExpression(result);
    Traits::QueueDestructuringAssignmentForRewriting(result);
  }

  return result;
}

template <class Traits>
typename ParserBase<Traits>::ExpressionT
ParserBase<Traits>::ParseYieldExpression(bool accept_IN,
                                         ExpressionClassifier* classifier,
                                         bool* ok) {
  // YieldExpression ::
  //   'yield' ([no line terminator] '*'? AssignmentExpression)?
  int pos = peek_position();
  classifier->RecordPatternError(scanner()->peek_location(),
                                 MessageTemplate::kInvalidDestructuringTarget);
  classifier->RecordFormalParameterInitializerError(
      scanner()->peek_location(), MessageTemplate::kYieldInParameter);
  Expect(Token::YIELD, CHECK_OK);
  ExpressionT generator_object =
      factory()->NewVariableProxy(function_state_->generator_object_variable());
  ExpressionT expression = Traits::EmptyExpression();
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
        expression = ParseAssignmentExpression(accept_IN, classifier, CHECK_OK);
        Traits::RewriteNonPattern(classifier, CHECK_OK);
        break;
    }
  }

  if (delegating) {
    return Traits::RewriteYieldStar(generator_object, expression, pos);
  }

  expression = Traits::BuildIteratorResult(expression, false);
  // Hackily disambiguate o from o.next and o [Symbol.iterator]().
  // TODO(verwaest): Come up with a better solution.
  typename Traits::Type::YieldExpression yield =
      factory()->NewYield(generator_object, expression, pos);
  return yield;
}

template <class Traits>
typename ParserBase<Traits>::ExpressionT
ParserBase<Traits>::ParseTailCallExpression(ExpressionClassifier* classifier,
                                            bool* ok) {
  // TailCallExpression::
  //   'continue' MemberExpression  Arguments
  //   'continue' CallExpression  Arguments
  //   'continue' MemberExpression  TemplateLiteral
  //   'continue' CallExpression  TemplateLiteral
  Expect(Token::CONTINUE, CHECK_OK);
  int pos = position();
  int sub_expression_pos = peek_position();
  ExpressionT expression =
      this->ParseLeftHandSideExpression(classifier, CHECK_OK);
  CheckNoTailCallExpressions(classifier, CHECK_OK);

  Scanner::Location loc(pos, scanner()->location().end_pos);
  if (!expression->IsCall()) {
    Scanner::Location sub_loc(sub_expression_pos, loc.end_pos);
    ReportMessageAt(sub_loc, MessageTemplate::kUnexpectedInsideTailCall);
    *ok = false;
    return Traits::EmptyExpression();
  }
  if (Traits::IsDirectEvalCall(expression)) {
    Scanner::Location sub_loc(sub_expression_pos, loc.end_pos);
    ReportMessageAt(sub_loc, MessageTemplate::kUnexpectedTailCallOfEval);
    *ok = false;
    return Traits::EmptyExpression();
  }
  if (!is_strict(language_mode())) {
    ReportMessageAt(loc, MessageTemplate::kUnexpectedSloppyTailCall);
    *ok = false;
    return Traits::EmptyExpression();
  }
  ReturnExprContext return_expr_context =
      function_state_->return_expr_context();
  if (return_expr_context != ReturnExprContext::kInsideValidReturnStatement) {
    MessageTemplate::Template msg = MessageTemplate::kNone;
    switch (return_expr_context) {
      case ReturnExprContext::kInsideValidReturnStatement:
        UNREACHABLE();
        return Traits::EmptyExpression();
      case ReturnExprContext::kInsideValidBlock:
        msg = MessageTemplate::kUnexpectedTailCall;
        break;
      case ReturnExprContext::kInsideTryBlock:
        msg = MessageTemplate::kUnexpectedTailCallInTryBlock;
        break;
      case ReturnExprContext::kInsideForInOfBody:
        msg = MessageTemplate::kUnexpectedTailCallInForInOf;
        break;
    }
    ReportMessageAt(loc, msg);
    *ok = false;
    return Traits::EmptyExpression();
  }
  classifier->RecordTailCallExpressionError(
      loc, MessageTemplate::kUnexpectedTailCall);
  function_state_->AddExplicitTailCallExpression(expression, loc);
  return expression;
}

// Precedence = 3
template <class Traits>
typename ParserBase<Traits>::ExpressionT
ParserBase<Traits>::ParseConditionalExpression(bool accept_IN,
                                               ExpressionClassifier* classifier,
                                               bool* ok) {
  // ConditionalExpression ::
  //   LogicalOrExpression
  //   LogicalOrExpression '?' AssignmentExpression ':' AssignmentExpression

  int pos = peek_position();
  // We start using the binary expression parser for prec >= 4 only!
  ExpressionT expression =
      this->ParseBinaryExpression(4, accept_IN, classifier, CHECK_OK);
  if (peek() != Token::CONDITIONAL) return expression;
  CheckNoTailCallExpressions(classifier, CHECK_OK);
  Traits::RewriteNonPattern(classifier, CHECK_OK);
  ArrowFormalParametersUnexpectedToken(classifier);
  BindingPatternUnexpectedToken(classifier);
  Consume(Token::CONDITIONAL);
  // In parsing the first assignment expression in conditional
  // expressions we always accept the 'in' keyword; see ECMA-262,
  // section 11.12, page 58.
  ExpressionT left = ParseAssignmentExpression(true, classifier, CHECK_OK);
  Traits::RewriteNonPattern(classifier, CHECK_OK);
  Expect(Token::COLON, CHECK_OK);
  ExpressionT right =
      ParseAssignmentExpression(accept_IN, classifier, CHECK_OK);
  Traits::RewriteNonPattern(classifier, CHECK_OK);
  return factory()->NewConditional(expression, left, right, pos);
}


// Precedence >= 4
template <class Traits>
typename ParserBase<Traits>::ExpressionT
ParserBase<Traits>::ParseBinaryExpression(int prec, bool accept_IN,
                                          ExpressionClassifier* classifier,
                                          bool* ok) {
  DCHECK(prec >= 4);
  ExpressionT x = this->ParseUnaryExpression(classifier, CHECK_OK);
  for (int prec1 = Precedence(peek(), accept_IN); prec1 >= prec; prec1--) {
    // prec1 >= 4
    while (Precedence(peek(), accept_IN) == prec1) {
      CheckNoTailCallExpressions(classifier, CHECK_OK);
      Traits::RewriteNonPattern(classifier, CHECK_OK);
      BindingPatternUnexpectedToken(classifier);
      ArrowFormalParametersUnexpectedToken(classifier);
      Token::Value op = Next();
      int pos = position();

      const bool is_right_associative = op == Token::EXP;
      const int next_prec = is_right_associative ? prec1 : prec1 + 1;
      ExpressionT y =
          ParseBinaryExpression(next_prec, accept_IN, classifier, CHECK_OK);
      if (op != Token::OR && op != Token::AND) {
        CheckNoTailCallExpressions(classifier, CHECK_OK);
      }
      Traits::RewriteNonPattern(classifier, CHECK_OK);

      if (this->ShortcutNumericLiteralBinaryExpression(&x, y, op, pos,
                                                       factory())) {
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
        x = Traits::RewriteExponentiation(x, y, pos);
      } else {
        // We have a "normal" binary operation.
        x = factory()->NewBinaryOperation(op, x, y, pos);
      }
    }
  }
  return x;
}


template <class Traits>
typename ParserBase<Traits>::ExpressionT
ParserBase<Traits>::ParseUnaryExpression(ExpressionClassifier* classifier,
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
    BindingPatternUnexpectedToken(classifier);
    ArrowFormalParametersUnexpectedToken(classifier);

    op = Next();
    int pos = position();
    ExpressionT expression = ParseUnaryExpression(classifier, CHECK_OK);
    CheckNoTailCallExpressions(classifier, CHECK_OK);
    Traits::RewriteNonPattern(classifier, CHECK_OK);

    if (op == Token::DELETE && is_strict(language_mode())) {
      if (this->IsIdentifier(expression)) {
        // "delete identifier" is a syntax error in strict mode.
        ReportMessage(MessageTemplate::kStrictDelete);
        *ok = false;
        return this->EmptyExpression();
      }
    }

    if (peek() == Token::EXP) {
      ReportUnexpectedToken(Next());
      *ok = false;
      return this->EmptyExpression();
    }

    // Allow Traits do rewrite the expression.
    return this->BuildUnaryExpression(expression, op, pos, factory());
  } else if (Token::IsCountOp(op)) {
    BindingPatternUnexpectedToken(classifier);
    ArrowFormalParametersUnexpectedToken(classifier);
    op = Next();
    int beg_pos = peek_position();
    ExpressionT expression = this->ParseUnaryExpression(classifier, CHECK_OK);
    CheckNoTailCallExpressions(classifier, CHECK_OK);
    expression = this->CheckAndRewriteReferenceExpression(
        expression, beg_pos, scanner()->location().end_pos,
        MessageTemplate::kInvalidLhsInPrefixOp, CHECK_OK);
    this->MarkExpressionAsAssigned(expression);
    Traits::RewriteNonPattern(classifier, CHECK_OK);

    return factory()->NewCountOperation(op,
                                        true /* prefix */,
                                        expression,
                                        position());

  } else if (is_async_function() && peek() == Token::AWAIT) {
    int beg_pos = peek_position();
    switch (PeekAhead()) {
      case Token::RPAREN:
      case Token::RBRACK:
      case Token::RBRACE:
      case Token::ASSIGN:
      case Token::COMMA: {
        Next();
        IdentifierT name = this->GetSymbol(scanner());

        // Possibly async arrow formals --- record ExpressionError just in case.
        ExpressionUnexpectedToken(classifier);
        classifier->RecordAsyncBindingPatternError(
            Scanner::Location(beg_pos, scanner()->location().end_pos),
            MessageTemplate::kAwaitBindingIdentifier);
        classifier->RecordAsyncArrowFormalParametersError(
            Scanner::Location(beg_pos, scanner()->location().end_pos),
            MessageTemplate::kAwaitBindingIdentifier);

        return this->ExpressionFromIdentifier(
            name, beg_pos, scanner()->location().end_pos, scope_, factory());
      }
      default:
        break;
    }
    Consume(Token::AWAIT);

    ExpressionT value = ParseUnaryExpression(classifier, CHECK_OK);

    classifier->RecordFormalParameterInitializerError(
        Scanner::Location(beg_pos, scanner()->location().end_pos),
        MessageTemplate::kAwaitExpressionFormalParameter);
    return Traits::RewriteAwaitExpression(value, beg_pos);
  } else {
    return this->ParsePostfixExpression(classifier, ok);
  }
}


template <class Traits>
typename ParserBase<Traits>::ExpressionT
ParserBase<Traits>::ParsePostfixExpression(ExpressionClassifier* classifier,
                                           bool* ok) {
  // PostfixExpression ::
  //   LeftHandSideExpression ('++' | '--')?

  int lhs_beg_pos = peek_position();
  ExpressionT expression =
      this->ParseLeftHandSideExpression(classifier, CHECK_OK);
  if (!scanner()->HasAnyLineTerminatorBeforeNext() &&
      Token::IsCountOp(peek())) {
    CheckNoTailCallExpressions(classifier, CHECK_OK);
    BindingPatternUnexpectedToken(classifier);
    ArrowFormalParametersUnexpectedToken(classifier);

    expression = this->CheckAndRewriteReferenceExpression(
        expression, lhs_beg_pos, scanner()->location().end_pos,
        MessageTemplate::kInvalidLhsInPostfixOp, CHECK_OK);
    expression = this->MarkExpressionAsAssigned(expression);
    Traits::RewriteNonPattern(classifier, CHECK_OK);

    Token::Value next = Next();
    expression =
        factory()->NewCountOperation(next,
                                     false /* postfix */,
                                     expression,
                                     position());
  }
  return expression;
}

template <class Traits>
typename ParserBase<Traits>::ExpressionT
ParserBase<Traits>::ParseLeftHandSideExpression(
    ExpressionClassifier* classifier, bool* ok) {
  // LeftHandSideExpression ::
  //   (NewExpression | MemberExpression) ...

  if (FLAG_harmony_explicit_tailcalls && peek() == Token::CONTINUE) {
    return this->ParseTailCallExpression(classifier, ok);
  }

  bool is_async = false;
  ExpressionT result = this->ParseMemberWithNewPrefixesExpression(
      classifier, &is_async, CHECK_OK);

  while (true) {
    switch (peek()) {
      case Token::LBRACK: {
        CheckNoTailCallExpressions(classifier, CHECK_OK);
        Traits::RewriteNonPattern(classifier, CHECK_OK);
        BindingPatternUnexpectedToken(classifier);
        ArrowFormalParametersUnexpectedToken(classifier);
        Consume(Token::LBRACK);
        int pos = position();
        ExpressionT index = ParseExpression(true, classifier, CHECK_OK);
        Traits::RewriteNonPattern(classifier, CHECK_OK);
        result = factory()->NewProperty(result, index, pos);
        Expect(Token::RBRACK, CHECK_OK);
        break;
      }

      case Token::LPAREN: {
        CheckNoTailCallExpressions(classifier, CHECK_OK);
        int pos;
        Traits::RewriteNonPattern(classifier, CHECK_OK);
        BindingPatternUnexpectedToken(classifier);
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
        typename Traits::Type::ExpressionList args =
            ParseArguments(&spread_pos, is_async, classifier, CHECK_OK);

        if (V8_UNLIKELY(is_async && peek() == Token::ARROW)) {
          if (args->length()) {
            // async ( Arguments ) => ...
            return Traits::ExpressionListToExpression(args);
          }
          // async () => ...
          return factory()->NewEmptyParentheses(pos);
        }

        ArrowFormalParametersUnexpectedToken(classifier);

        // Keep track of eval() calls since they disable all local variable
        // optimizations.
        // The calls that need special treatment are the
        // direct eval calls. These calls are all of the form eval(...), with
        // no explicit receiver.
        // These calls are marked as potentially direct eval calls. Whether
        // they are actually direct calls to eval is determined at run time.
        this->CheckPossibleEvalCall(result, scope_);

        bool is_super_call = result->IsSuperCallReference();
        if (spread_pos.IsValid()) {
          args = Traits::PrepareSpreadArguments(args);
          result = Traits::SpreadCall(result, args, pos);
        } else {
          result = factory()->NewCall(result, args, pos);
        }

        // Explicit calls to the super constructor using super() perform an
        // implicit binding assignment to the 'this' variable.
        if (is_super_call) {
          ExpressionT this_expr = this->ThisExpression(scope_, factory(), pos);
          result =
              factory()->NewAssignment(Token::INIT, this_expr, result, pos);
        }

        if (fni_ != NULL) fni_->RemoveLastFunction();
        break;
      }

      case Token::PERIOD: {
        CheckNoTailCallExpressions(classifier, CHECK_OK);
        Traits::RewriteNonPattern(classifier, CHECK_OK);
        BindingPatternUnexpectedToken(classifier);
        ArrowFormalParametersUnexpectedToken(classifier);
        Consume(Token::PERIOD);
        int pos = position();
        IdentifierT name = ParseIdentifierName(CHECK_OK);
        result = factory()->NewProperty(
            result, factory()->NewStringLiteral(name, pos), pos);
        if (fni_ != NULL) this->PushLiteralName(fni_, name);
        break;
      }

      case Token::TEMPLATE_SPAN:
      case Token::TEMPLATE_TAIL: {
        CheckNoTailCallExpressions(classifier, CHECK_OK);
        Traits::RewriteNonPattern(classifier, CHECK_OK);
        BindingPatternUnexpectedToken(classifier);
        ArrowFormalParametersUnexpectedToken(classifier);
        result = ParseTemplateLiteral(result, position(), classifier, CHECK_OK);
        break;
      }

      default:
        return result;
    }
  }
}

template <class Traits>
typename ParserBase<Traits>::ExpressionT
ParserBase<Traits>::ParseMemberWithNewPrefixesExpression(
    ExpressionClassifier* classifier, bool* is_async, bool* ok) {
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
    BindingPatternUnexpectedToken(classifier);
    ArrowFormalParametersUnexpectedToken(classifier);
    Consume(Token::NEW);
    int new_pos = position();
    ExpressionT result = this->EmptyExpression();
    if (peek() == Token::SUPER) {
      const bool is_new = true;
      result = ParseSuperExpression(is_new, classifier, CHECK_OK);
    } else if (peek() == Token::PERIOD) {
      return ParseNewTargetExpression(CHECK_OK);
    } else {
      result = this->ParseMemberWithNewPrefixesExpression(classifier, is_async,
                                                          CHECK_OK);
    }
    Traits::RewriteNonPattern(classifier, CHECK_OK);
    if (peek() == Token::LPAREN) {
      // NewExpression with arguments.
      Scanner::Location spread_pos;
      typename Traits::Type::ExpressionList args =
          this->ParseArguments(&spread_pos, classifier, CHECK_OK);

      if (spread_pos.IsValid()) {
        args = Traits::PrepareSpreadArguments(args);
        result = Traits::SpreadCallNew(result, args, new_pos);
      } else {
        result = factory()->NewCallNew(result, args, new_pos);
      }
      // The expression can still continue with . or [ after the arguments.
      result = this->ParseMemberExpressionContinuation(result, is_async,
                                                       classifier, CHECK_OK);
      return result;
    }
    // NewExpression without arguments.
    return factory()->NewCallNew(result, this->NewExpressionList(0, zone_),
                                 new_pos);
  }
  // No 'new' or 'super' keyword.
  return this->ParseMemberExpression(classifier, is_async, ok);
}

template <class Traits>
typename ParserBase<Traits>::ExpressionT
ParserBase<Traits>::ParseMemberExpression(ExpressionClassifier* classifier,
                                          bool* is_async, bool* ok) {
  // MemberExpression ::
  //   (PrimaryExpression | FunctionLiteral | ClassLiteral)
  //     ('[' Expression ']' | '.' Identifier | Arguments | TemplateLiteral)*

  // The '[' Expression ']' and '.' Identifier parts are parsed by
  // ParseMemberExpressionContinuation, and the Arguments part is parsed by the
  // caller.

  // Parse the initial primary or function expression.
  ExpressionT result = this->EmptyExpression();
  if (peek() == Token::FUNCTION) {
    BindingPatternUnexpectedToken(classifier);
    ArrowFormalParametersUnexpectedToken(classifier);

    Consume(Token::FUNCTION);
    int function_token_position = position();

    if (allow_harmony_function_sent() && peek() == Token::PERIOD) {
      // function.sent
      int pos = position();
      ExpectMetaProperty(CStrVector("sent"), "function.sent", pos, CHECK_OK);

      if (!is_generator()) {
        // TODO(neis): allow escaping into closures?
        ReportMessageAt(scanner()->location(),
                        MessageTemplate::kUnexpectedFunctionSent);
        *ok = false;
        return this->EmptyExpression();
      }

      return this->FunctionSentExpression(scope_, factory(), pos);
    }

    bool is_generator = Check(Token::MUL);
    IdentifierT name = this->EmptyIdentifier();
    bool is_strict_reserved_name = false;
    Scanner::Location function_name_location = Scanner::Location::invalid();
    FunctionLiteral::FunctionType function_type =
        FunctionLiteral::kAnonymousExpression;
    if (peek_any_identifier()) {
      name = ParseIdentifierOrStrictReservedWord(
          is_generator, &is_strict_reserved_name, CHECK_OK);
      function_name_location = scanner()->location();
      function_type = FunctionLiteral::kNamedExpression;
    }
    result = this->ParseFunctionLiteral(
        name, function_name_location,
        is_strict_reserved_name ? kFunctionNameIsStrictReserved
                                : kFunctionNameValidityUnknown,
        is_generator ? FunctionKind::kGeneratorFunction
                     : FunctionKind::kNormalFunction,
        function_token_position, function_type, language_mode(), CHECK_OK);
  } else if (peek() == Token::SUPER) {
    const bool is_new = false;
    result = ParseSuperExpression(is_new, classifier, CHECK_OK);
  } else {
    result = ParsePrimaryExpression(classifier, is_async, CHECK_OK);
  }

  result =
      ParseMemberExpressionContinuation(result, is_async, classifier, CHECK_OK);
  return result;
}


template <class Traits>
typename ParserBase<Traits>::ExpressionT
ParserBase<Traits>::ParseSuperExpression(bool is_new,
                                         ExpressionClassifier* classifier,
                                         bool* ok) {
  Expect(Token::SUPER, CHECK_OK);
  int pos = position();

  Scope* scope = scope_->ReceiverScope();
  FunctionKind kind = scope->function_kind();
  if (IsConciseMethod(kind) || IsAccessorFunction(kind) ||
      IsClassConstructor(kind)) {
    if (peek() == Token::PERIOD || peek() == Token::LBRACK) {
      scope->RecordSuperPropertyUsage();
      return this->SuperPropertyReference(scope_, factory(), pos);
    }
    // new super() is never allowed.
    // super() is only allowed in derived constructor
    if (!is_new && peek() == Token::LPAREN && IsSubclassConstructor(kind)) {
      // TODO(rossberg): This might not be the correct FunctionState for the
      // method here.
      function_state_->set_super_location(scanner()->location());
      return this->SuperCallReference(scope_, factory(), pos);
    }
  }

  ReportMessageAt(scanner()->location(), MessageTemplate::kUnexpectedSuper);
  *ok = false;
  return this->EmptyExpression();
}

template <class Traits>
void ParserBase<Traits>::ExpectMetaProperty(Vector<const char> property_name,
                                            const char* full_name, int pos,
                                            bool* ok) {
  Consume(Token::PERIOD);
  ExpectContextualKeyword(property_name, ok);
  if (!*ok) return;
  if (scanner()->literal_contains_escapes()) {
    Traits::ReportMessageAt(
        Scanner::Location(pos, scanner()->location().end_pos),
        MessageTemplate::kInvalidEscapedMetaProperty, full_name);
    *ok = false;
  }
}

template <class Traits>
typename ParserBase<Traits>::ExpressionT
ParserBase<Traits>::ParseNewTargetExpression(bool* ok) {
  int pos = position();
  ExpectMetaProperty(CStrVector("target"), "new.target", pos, CHECK_OK);

  if (!scope_->ReceiverScope()->is_function_scope()) {
    ReportMessageAt(scanner()->location(),
                    MessageTemplate::kUnexpectedNewTarget);
    *ok = false;
    return this->EmptyExpression();
  }

  return this->NewTargetExpression(scope_, factory(), pos);
}

template <class Traits>
typename ParserBase<Traits>::ExpressionT
ParserBase<Traits>::ParseMemberExpressionContinuation(
    ExpressionT expression, bool* is_async, ExpressionClassifier* classifier,
    bool* ok) {
  // Parses this part of MemberExpression:
  // ('[' Expression ']' | '.' Identifier | TemplateLiteral)*
  while (true) {
    switch (peek()) {
      case Token::LBRACK: {
        *is_async = false;
        Traits::RewriteNonPattern(classifier, CHECK_OK);
        BindingPatternUnexpectedToken(classifier);
        ArrowFormalParametersUnexpectedToken(classifier);

        Consume(Token::LBRACK);
        int pos = position();
        ExpressionT index = this->ParseExpression(true, classifier, CHECK_OK);
        Traits::RewriteNonPattern(classifier, CHECK_OK);
        expression = factory()->NewProperty(expression, index, pos);
        if (fni_ != NULL) {
          this->PushPropertyName(fni_, index);
        }
        Expect(Token::RBRACK, CHECK_OK);
        break;
      }
      case Token::PERIOD: {
        *is_async = false;
        Traits::RewriteNonPattern(classifier, CHECK_OK);
        BindingPatternUnexpectedToken(classifier);
        ArrowFormalParametersUnexpectedToken(classifier);

        Consume(Token::PERIOD);
        int pos = position();
        IdentifierT name = ParseIdentifierName(CHECK_OK);
        expression = factory()->NewProperty(
            expression, factory()->NewStringLiteral(name, pos), pos);
        if (fni_ != NULL) {
          this->PushLiteralName(fni_, name);
        }
        break;
      }
      case Token::TEMPLATE_SPAN:
      case Token::TEMPLATE_TAIL: {
        *is_async = false;
        Traits::RewriteNonPattern(classifier, CHECK_OK);
        BindingPatternUnexpectedToken(classifier);
        ArrowFormalParametersUnexpectedToken(classifier);
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
        expression =
            ParseTemplateLiteral(expression, pos, classifier, CHECK_OK);
        break;
      }
      case Token::ILLEGAL: {
        ReportUnexpectedTokenAt(scanner()->peek_location(), Token::ILLEGAL);
        *ok = false;
        return this->EmptyExpression();
      }
      default:
        return expression;
    }
  }
  DCHECK(false);
  return this->EmptyExpression();
}


template <class Traits>
void ParserBase<Traits>::ParseFormalParameter(
    FormalParametersT* parameters, ExpressionClassifier* classifier, bool* ok) {
  // FormalParameter[Yield,GeneratorParameter] :
  //   BindingElement[?Yield, ?GeneratorParameter]
  bool is_rest = parameters->has_rest;

  ExpressionT pattern = ParsePrimaryExpression(classifier, ok);
  if (!*ok) return;

  ValidateBindingPattern(classifier, ok);
  if (!*ok) return;

  if (!Traits::IsIdentifier(pattern)) {
    parameters->is_simple = false;
    ValidateFormalParameterInitializer(classifier, ok);
    if (!*ok) return;
    classifier->RecordNonSimpleParameter();
  }

  ExpressionT initializer = Traits::EmptyExpression();
  if (!is_rest && Check(Token::ASSIGN)) {
    ExpressionClassifier init_classifier(this);
    initializer = ParseAssignmentExpression(true, &init_classifier, ok);
    if (!*ok) return;
    Traits::RewriteNonPattern(&init_classifier, ok);
    ValidateFormalParameterInitializer(&init_classifier, ok);
    if (!*ok) return;
    parameters->is_simple = false;
    init_classifier.Discard();
    classifier->RecordNonSimpleParameter();

    if (allow_harmony_function_name()) {
      Traits::SetFunctionNameFromIdentifierRef(initializer, pattern);
    }
  }

  Traits::AddFormalParameter(parameters, pattern, initializer,
                             scanner()->location().end_pos, is_rest);
}


template <class Traits>
void ParserBase<Traits>::ParseFormalParameterList(
    FormalParametersT* parameters, ExpressionClassifier* classifier, bool* ok) {
  // FormalParameters[Yield,GeneratorParameter] :
  //   [empty]
  //   FormalParameterList[?Yield, ?GeneratorParameter]
  //
  // FormalParameterList[Yield,GeneratorParameter] :
  //   FunctionRestParameter[?Yield]
  //   FormalsList[?Yield, ?GeneratorParameter]
  //   FormalsList[?Yield, ?GeneratorParameter] , FunctionRestParameter[?Yield]
  //
  // FormalsList[Yield,GeneratorParameter] :
  //   FormalParameter[?Yield, ?GeneratorParameter]
  //   FormalsList[?Yield, ?GeneratorParameter] ,
  //     FormalParameter[?Yield,?GeneratorParameter]

  DCHECK_EQ(0, parameters->Arity());

  if (peek() != Token::RPAREN) {
    do {
      if (parameters->Arity() > Code::kMaxArguments) {
        ReportMessage(MessageTemplate::kTooManyParameters);
        *ok = false;
        return;
      }
      parameters->has_rest = Check(Token::ELLIPSIS);
      ParseFormalParameter(parameters, classifier, ok);
      if (!*ok) return;
    } while (!parameters->has_rest && Check(Token::COMMA));

    if (parameters->has_rest) {
      parameters->is_simple = false;
      classifier->RecordNonSimpleParameter();
      if (peek() == Token::COMMA) {
        ReportMessageAt(scanner()->peek_location(),
                      MessageTemplate::kParamAfterRest);
        *ok = false;
        return;
      }
    }
  }

  for (int i = 0; i < parameters->Arity(); ++i) {
    auto parameter = parameters->at(i);
    Traits::DeclareFormalParameter(parameters->scope, parameter, classifier);
  }
}

template <class Traits>
void ParserBase<Traits>::CheckArityRestrictions(int param_count,
                                                FunctionKind function_kind,
                                                bool has_rest,
                                                int formals_start_pos,
                                                int formals_end_pos, bool* ok) {
  if (IsGetterFunction(function_kind)) {
    if (param_count != 0) {
      ReportMessageAt(Scanner::Location(formals_start_pos, formals_end_pos),
                      MessageTemplate::kBadGetterArity);
      *ok = false;
    }
  } else if (IsSetterFunction(function_kind)) {
    if (param_count != 1) {
      ReportMessageAt(Scanner::Location(formals_start_pos, formals_end_pos),
                      MessageTemplate::kBadSetterArity);
      *ok = false;
    }
    if (has_rest) {
      ReportMessageAt(Scanner::Location(formals_start_pos, formals_end_pos),
                      MessageTemplate::kBadSetterRestParameter);
      *ok = false;
    }
  }
}


template <class Traits>
bool ParserBase<Traits>::IsNextLetKeyword() {
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

template <class Traits>
typename ParserBase<Traits>::ExpressionT
ParserBase<Traits>::ParseArrowFunctionLiteral(
    bool accept_IN, const FormalParametersT& formal_parameters, bool is_async,
    const ExpressionClassifier& formals_classifier, bool* ok) {
  if (peek() == Token::ARROW && scanner_->HasAnyLineTerminatorBeforeNext()) {
    // ASI inserts `;` after arrow parameters if a line terminator is found.
    // `=> ...` is never a valid expression, so report as syntax error.
    // If next token is not `=>`, it's a syntax error anyways.
    ReportUnexpectedTokenAt(scanner_->peek_location(), Token::ARROW);
    *ok = false;
    return this->EmptyExpression();
  }

  typename Traits::Type::StatementList body;
  int num_parameters = formal_parameters.scope->num_parameters();
  int materialized_literal_count = -1;
  int expected_property_count = -1;
  Scanner::Location super_loc;

  FunctionKind arrow_kind = is_async ? kAsyncArrowFunction : kArrowFunction;
  {
    typename Traits::Type::Factory function_factory(ast_value_factory());
    FunctionState function_state(&function_state_, &scope_,
                                 formal_parameters.scope, arrow_kind,
                                 &function_factory);

    function_state.SkipMaterializedLiterals(
        formal_parameters.materialized_literals_count);

    this->ReindexLiterals(formal_parameters);

    Expect(Token::ARROW, CHECK_OK);

    if (peek() == Token::LBRACE) {
      // Multiple statement body
      Consume(Token::LBRACE);
      bool is_lazily_parsed =
          (mode() == PARSE_LAZILY && scope_->AllowsLazyParsing());
      if (is_lazily_parsed) {
        body = this->NewStatementList(0, zone());
        this->SkipLazyFunctionBody(&materialized_literal_count,
                                   &expected_property_count, CHECK_OK);
        if (formal_parameters.materialized_literals_count > 0) {
          materialized_literal_count +=
              formal_parameters.materialized_literals_count;
        }
      } else {
        body = this->ParseEagerFunctionBody(
            this->EmptyIdentifier(), RelocInfo::kNoPosition, formal_parameters,
            arrow_kind, FunctionLiteral::kAnonymousExpression, CHECK_OK);
        materialized_literal_count =
            function_state.materialized_literal_count();
        expected_property_count = function_state.expected_property_count();
      }
    } else {
      // Single-expression body
      int pos = position();
      ExpressionClassifier classifier(this);
      DCHECK(ReturnExprContext::kInsideValidBlock ==
             function_state_->return_expr_context());
      ReturnExprScope allow_tail_calls(
          function_state_, ReturnExprContext::kInsideValidReturnStatement);
      body = this->NewStatementList(1, zone());
      this->AddParameterInitializationBlock(formal_parameters, body, is_async,
                                            CHECK_OK);
      if (is_async) {
        this->ParseAsyncArrowSingleExpressionBody(body, accept_IN, &classifier,
                                                  pos, CHECK_OK);
        Traits::RewriteNonPattern(&classifier, CHECK_OK);
      } else {
        ExpressionT expression =
            ParseAssignmentExpression(accept_IN, &classifier, CHECK_OK);
        Traits::RewriteNonPattern(&classifier, CHECK_OK);
        body->Add(factory()->NewReturnStatement(expression, pos), zone());
        if (allow_tailcalls() && !is_sloppy(language_mode())) {
          // ES6 14.6.1 Static Semantics: IsInTailPosition
          this->MarkTailPosition(expression);
        }
      }
      materialized_literal_count = function_state.materialized_literal_count();
      expected_property_count = function_state.expected_property_count();
      this->MarkCollectedTailCallExpressions();
    }
    super_loc = function_state.super_location();

    formal_parameters.scope->set_end_position(scanner()->location().end_pos);

    // Arrow function formal parameters are parsed as StrictFormalParameterList,
    // which is not the same as "parameters of a strict function"; it only means
    // that duplicates are not allowed.  Of course, the arrow function may
    // itself be strict as well.
    const bool allow_duplicate_parameters = false;
    this->ValidateFormalParameters(&formals_classifier, language_mode(),
                                   allow_duplicate_parameters, CHECK_OK);

    // Validate strict mode.
    if (is_strict(language_mode())) {
      CheckStrictOctalLiteral(formal_parameters.scope->start_position(),
                              scanner()->location().end_pos, CHECK_OK);
    }
    this->CheckConflictingVarDeclarations(formal_parameters.scope, CHECK_OK);

    Traits::RewriteDestructuringAssignments();
  }

  FunctionLiteralT function_literal = factory()->NewFunctionLiteral(
      this->EmptyIdentifierString(), formal_parameters.scope, body,
      materialized_literal_count, expected_property_count, num_parameters,
      FunctionLiteral::kNoDuplicateParameters,
      FunctionLiteral::kAnonymousExpression,
      FunctionLiteral::kShouldLazyCompile, arrow_kind,
      formal_parameters.scope->start_position());

  function_literal->set_function_token_position(
      formal_parameters.scope->start_position());
  if (super_loc.IsValid()) function_state_->set_super_location(super_loc);

  if (fni_ != NULL) this->InferFunctionName(fni_, function_literal);

  return function_literal;
}


template <typename Traits>
typename ParserBase<Traits>::ExpressionT
ParserBase<Traits>::ParseTemplateLiteral(ExpressionT tag, int start,
                                         ExpressionClassifier* classifier,
                                         bool* ok) {
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
    typename Traits::TemplateLiteralState ts = Traits::OpenTemplateLiteral(pos);
    Traits::AddTemplateSpan(&ts, true);
    return Traits::CloseTemplateLiteral(&ts, start, tag);
  }

  Consume(Token::TEMPLATE_SPAN);
  int pos = position();
  typename Traits::TemplateLiteralState ts = Traits::OpenTemplateLiteral(pos);
  Traits::AddTemplateSpan(&ts, false);
  Token::Value next;

  // If we open with a TEMPLATE_SPAN, we must scan the subsequent expression,
  // and repeat if the following token is a TEMPLATE_SPAN as well (in this
  // case, representing a TemplateMiddle).

  do {
    CheckTemplateOctalLiteral(pos, peek_position(), CHECK_OK);
    next = peek();
    if (next == Token::EOS) {
      ReportMessageAt(Scanner::Location(start, peek_position()),
                      MessageTemplate::kUnterminatedTemplate);
      *ok = false;
      return Traits::EmptyExpression();
    } else if (next == Token::ILLEGAL) {
      Traits::ReportMessageAt(
          Scanner::Location(position() + 1, peek_position()),
          MessageTemplate::kUnexpectedToken, "ILLEGAL", kSyntaxError);
      *ok = false;
      return Traits::EmptyExpression();
    }

    int expr_pos = peek_position();
    ExpressionT expression = this->ParseExpression(true, classifier, CHECK_OK);
    CheckNoTailCallExpressions(classifier, CHECK_OK);
    Traits::RewriteNonPattern(classifier, CHECK_OK);
    Traits::AddTemplateExpression(&ts, expression);

    if (peek() != Token::RBRACE) {
      ReportMessageAt(Scanner::Location(expr_pos, peek_position()),
                      MessageTemplate::kUnterminatedTemplateExpr);
      *ok = false;
      return Traits::EmptyExpression();
    }

    // If we didn't die parsing that expression, our next token should be a
    // TEMPLATE_SPAN or TEMPLATE_TAIL.
    next = scanner()->ScanTemplateContinuation();
    Next();
    pos = position();

    if (next == Token::EOS) {
      ReportMessageAt(Scanner::Location(start, pos),
                      MessageTemplate::kUnterminatedTemplate);
      *ok = false;
      return Traits::EmptyExpression();
    } else if (next == Token::ILLEGAL) {
      Traits::ReportMessageAt(
          Scanner::Location(position() + 1, peek_position()),
          MessageTemplate::kUnexpectedToken, "ILLEGAL", kSyntaxError);
      *ok = false;
      return Traits::EmptyExpression();
    }

    Traits::AddTemplateSpan(&ts, next == Token::TEMPLATE_TAIL);
  } while (next == Token::TEMPLATE_SPAN);

  DCHECK_EQ(next, Token::TEMPLATE_TAIL);
  CheckTemplateOctalLiteral(pos, peek_position(), CHECK_OK);
  // Once we've reached a TEMPLATE_TAIL, we can close the TemplateLiteral.
  return Traits::CloseTemplateLiteral(&ts, start, tag);
}


template <typename Traits>
typename ParserBase<Traits>::ExpressionT
ParserBase<Traits>::CheckAndRewriteReferenceExpression(
    ExpressionT expression, int beg_pos, int end_pos,
    MessageTemplate::Template message, bool* ok) {
  return this->CheckAndRewriteReferenceExpression(expression, beg_pos, end_pos,
                                                  message, kReferenceError, ok);
}


template <typename Traits>
typename ParserBase<Traits>::ExpressionT
ParserBase<Traits>::CheckAndRewriteReferenceExpression(
    ExpressionT expression, int beg_pos, int end_pos,
    MessageTemplate::Template message, ParseErrorType type, bool* ok) {
  if (this->IsIdentifier(expression) && is_strict(language_mode()) &&
      this->IsEvalOrArguments(this->AsIdentifier(expression))) {
    ReportMessageAt(Scanner::Location(beg_pos, end_pos),
                    MessageTemplate::kStrictEvalArguments, kSyntaxError);
    *ok = false;
    return this->EmptyExpression();
  }
  if (expression->IsValidReferenceExpression()) {
    return expression;
  }
  if (expression->IsCall()) {
    // If it is a call, make it a runtime error for legacy web compatibility.
    // Rewrite `expr' to `expr[throw ReferenceError]'.
    ExpressionT error = this->NewThrowReferenceError(message, beg_pos);
    return factory()->NewProperty(expression, error, beg_pos);
  }
  ReportMessageAt(Scanner::Location(beg_pos, end_pos), message, type);
  *ok = false;
  return this->EmptyExpression();
}


template <typename Traits>
bool ParserBase<Traits>::IsValidReferenceExpression(ExpressionT expression) {
  return this->IsAssignableIdentifier(expression) || expression->IsProperty();
}


template <typename Traits>
void ParserBase<Traits>::CheckDestructuringElement(
    ExpressionT expression, ExpressionClassifier* classifier, int begin,
    int end) {
  if (!IsValidPattern(expression) && !expression->IsAssignment() &&
      !IsValidReferenceExpression(expression)) {
    classifier->RecordAssignmentPatternError(
        Scanner::Location(begin, end),
        MessageTemplate::kInvalidDestructuringTarget);
  }
}


#undef CHECK_OK
#undef CHECK_OK_CUSTOM

template <typename Traits>
void ParserBase<Traits>::ObjectLiteralChecker::CheckProperty(
    Token::Value property, PropertyKind type, MethodKind method_type,
    bool* ok) {
  DCHECK(!IsStaticMethod(method_type));
  DCHECK(!IsSpecialMethod(method_type) || type == kMethodProperty);

  if (property == Token::SMI || property == Token::NUMBER) return;

  if (type == kValueProperty && IsProto()) {
    if (has_seen_proto_) {
      this->parser()->ReportMessage(MessageTemplate::kDuplicateProto);
      *ok = false;
      return;
    }
    has_seen_proto_ = true;
    return;
  }
}

template <typename Traits>
void ParserBase<Traits>::ClassLiteralChecker::CheckProperty(
    Token::Value property, PropertyKind type, MethodKind method_type,
    bool* ok) {
  DCHECK(type == kMethodProperty || type == kAccessorProperty);

  if (property == Token::SMI || property == Token::NUMBER) return;

  if (IsStaticMethod(method_type)) {
    if (IsPrototype()) {
      this->parser()->ReportMessage(MessageTemplate::kStaticPrototype);
      *ok = false;
      return;
    }
  } else if (IsConstructor()) {
    const bool is_generator = IsGeneratorMethod(method_type);
    const bool is_async = IsAsyncMethod(method_type);
    if (is_generator || is_async || type == kAccessorProperty) {
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
