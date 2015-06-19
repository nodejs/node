// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PREPARSER_H
#define V8_PREPARSER_H

#include "src/v8.h"

#include "src/bailout-reason.h"
#include "src/func-name-inferrer.h"
#include "src/hashmap.h"
#include "src/scanner.h"
#include "src/scopes.h"
#include "src/token.h"

namespace v8 {
namespace internal {


// When parsing the formal parameters of a function, we usually don't yet know
// if the function will be strict, so we cannot yet produce errors for
// parameter names or duplicates.  Instead, we remember the locations of these
// errors if they occur and produce the errors later.
class FormalParameterErrorLocations BASE_EMBEDDED {
 public:
  FormalParameterErrorLocations()
      : eval_or_arguments(Scanner::Location::invalid()),
        undefined(Scanner::Location::invalid()),
        duplicate(Scanner::Location::invalid()),
        reserved(Scanner::Location::invalid()) {}

  Scanner::Location eval_or_arguments;
  Scanner::Location undefined;
  Scanner::Location duplicate;
  Scanner::Location reserved;
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
//     typedef FormalParameterScope;
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
  typedef typename Traits::Type::FormalParameterScope FormalParameterScopeT;
  typedef typename Traits::Type::FunctionLiteral FunctionLiteralT;
  typedef typename Traits::Type::Literal LiteralT;
  typedef typename Traits::Type::ObjectLiteralProperty ObjectLiteralPropertyT;

  ParserBase(Zone* zone, Scanner* scanner, uintptr_t stack_limit,
             v8::Extension* extension, AstValueFactory* ast_value_factory,
             ParserRecorder* log, typename Traits::Type::Parser this_object)
      : Traits(this_object),
        parenthesized_function_(false),
        scope_(NULL),
        function_state_(NULL),
        extension_(extension),
        fni_(NULL),
        ast_value_factory_(ast_value_factory),
        log_(log),
        mode_(PARSE_EAGERLY),  // Lazy mode must be set explicitly.
        stack_limit_(stack_limit),
        zone_(zone),
        scanner_(scanner),
        stack_overflow_(false),
        allow_lazy_(false),
        allow_natives_(false),
        allow_harmony_arrow_functions_(false),
        allow_harmony_object_literals_(false),
        allow_harmony_sloppy_(false),
        allow_harmony_computed_property_names_(false),
        allow_harmony_rest_params_(false),
        allow_harmony_spreadcalls_(false),
        allow_strong_mode_(false) {}

  // Getters that indicate whether certain syntactical constructs are
  // allowed to be parsed by this instance of the parser.
  bool allow_lazy() const { return allow_lazy_; }
  bool allow_natives() const { return allow_natives_; }
  bool allow_harmony_arrow_functions() const {
    return allow_harmony_arrow_functions_;
  }
  bool allow_harmony_modules() const { return scanner()->HarmonyModules(); }
  bool allow_harmony_classes() const { return scanner()->HarmonyClasses(); }
  bool allow_harmony_object_literals() const {
    return allow_harmony_object_literals_;
  }
  bool allow_harmony_sloppy() const { return allow_harmony_sloppy_; }
  bool allow_harmony_unicode() const { return scanner()->HarmonyUnicode(); }
  bool allow_harmony_computed_property_names() const {
    return allow_harmony_computed_property_names_;
  }
  bool allow_harmony_rest_params() const {
    return allow_harmony_rest_params_;
  }
  bool allow_harmony_spreadcalls() const { return allow_harmony_spreadcalls_; }
  bool allow_harmony_destructuring() const {
    return allow_harmony_destructuring_;
  }

  bool allow_strong_mode() const { return allow_strong_mode_; }

  // Setters that determine whether certain syntactical constructs are
  // allowed to be parsed by this instance of the parser.
  void set_allow_lazy(bool allow) { allow_lazy_ = allow; }
  void set_allow_natives(bool allow) { allow_natives_ = allow; }
  void set_allow_harmony_arrow_functions(bool allow) {
    allow_harmony_arrow_functions_ = allow;
  }
  void set_allow_harmony_modules(bool allow) {
    scanner()->SetHarmonyModules(allow);
  }
  void set_allow_harmony_classes(bool allow) {
    scanner()->SetHarmonyClasses(allow);
  }
  void set_allow_harmony_object_literals(bool allow) {
    allow_harmony_object_literals_ = allow;
  }
  void set_allow_harmony_sloppy(bool allow) {
    allow_harmony_sloppy_ = allow;
  }
  void set_allow_harmony_unicode(bool allow) {
    scanner()->SetHarmonyUnicode(allow);
  }
  void set_allow_harmony_computed_property_names(bool allow) {
    allow_harmony_computed_property_names_ = allow;
  }
  void set_allow_harmony_rest_params(bool allow) {
    allow_harmony_rest_params_ = allow;
  }
  void set_allow_harmony_spreadcalls(bool allow) {
    allow_harmony_spreadcalls_ = allow;
  }
  void set_allow_strong_mode(bool allow) { allow_strong_mode_ = allow; }
  void set_allow_harmony_destructuring(bool allow) {
    allow_harmony_destructuring_ = allow;
  }


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
        : scope_stack_(scope_stack), outer_scope_(*scope_stack), scope_(scope) {
      *scope_stack_ = scope_;
    }
    ~BlockState() { *scope_stack_ = outer_scope_; }

   private:
    Scope** scope_stack_;
    Scope* outer_scope_;
    Scope* scope_;
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

    int NextHandlerIndex() { return next_handler_index_++; }
    int handler_count() { return next_handler_index_; }

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

    FunctionKind kind() const { return kind_; }
    FunctionState* outer() const { return outer_function_state_; }

    void set_generator_object_variable(
        typename Traits::Type::GeneratorVariable* variable) {
      DCHECK(variable != NULL);
      DCHECK(is_generator());
      generator_object_variable_ = variable;
    }
    typename Traits::Type::GeneratorVariable* generator_object_variable()
        const {
      return generator_object_variable_;
    }

    typename Traits::Type::Factory* factory() { return factory_; }

   private:
    // Used to assign an index to each literal that needs materialization in
    // the function.  Includes regexp literals, and boilerplate for object and
    // array literals.
    int next_materialized_literal_index_;

    // Used to assign a per-function index to try and catch handlers.
    int next_handler_index_;

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
    typename Traits::Type::Factory* factory_;

    friend class ParserTraits;
    friend class Checkpoint;
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
      next_handler_index_ = function_state_->next_handler_index_;
      expected_property_count_ = function_state_->expected_property_count_;
    }

    void Restore() {
      function_state_->next_materialized_literal_index_ =
          next_materialized_literal_index_;
      function_state_->next_handler_index_ = next_handler_index_;
      function_state_->expected_property_count_ = expected_property_count_;
    }

   private:
    FunctionState* function_state_;
    int next_materialized_literal_index_;
    int next_handler_index_;
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

  Scope* NewScope(Scope* parent, ScopeType scope_type,
                  FunctionKind kind = kNormalFunction) {
    DCHECK(ast_value_factory());
    DCHECK(scope_type != MODULE_SCOPE || allow_harmony_modules());
    DCHECK((scope_type == FUNCTION_SCOPE && IsValidFunctionKind(kind)) ||
           kind == kNormalFunction);
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
    return next == Token::IDENTIFIER || next == Token::FUTURE_RESERVED_WORD ||
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

  void ExpectContextualKeyword(Vector<const char> keyword, bool* ok) {
    Expect(Token::IDENTIFIER, ok);
    if (!*ok) return;
    if (!scanner()->is_literal_contextual_keyword(keyword)) {
      ReportUnexpectedToken(scanner()->current_token());
      *ok = false;
    }
  }

  bool CheckInOrOf(
      bool accept_OF, ForEachStatement::VisitMode* visit_mode, bool* ok) {
    if (Check(Token::IN)) {
      if (is_strong(language_mode())) {
        ReportMessageAt(scanner()->location(), "strong_for_in");
        *ok = false;
      } else {
        *visit_mode = ForEachStatement::ENUMERATE;
      }
      return true;
    } else if (accept_OF && CheckContextualKeyword(CStrVector("of"))) {
      *visit_mode = ForEachStatement::ITERATE;
      return true;
    }
    return false;
  }

  // Checks whether an octal literal was last seen between beg_pos and end_pos.
  // If so, reports an error. Only called for strict mode and template strings.
  void CheckOctalLiteral(int beg_pos, int end_pos, const char* error,
                         bool* ok) {
    Scanner::Location octal = scanner()->octal_position();
    if (octal.IsValid() && beg_pos <= octal.beg_pos &&
        octal.end_pos <= end_pos) {
      ReportMessageAt(octal, error);
      scanner()->clear_octal_position();
      *ok = false;
    }
  }

  inline void CheckStrictOctalLiteral(int beg_pos, int end_pos, bool* ok) {
    CheckOctalLiteral(beg_pos, end_pos, "strict_octal_literal", ok);
  }

  inline void CheckTemplateOctalLiteral(int beg_pos, int end_pos, bool* ok) {
    CheckOctalLiteral(beg_pos, end_pos, "template_octal_literal", ok);
  }

  // Checking the name of a function literal. This has to be done after parsing
  // the function, since the function can declare itself strict.
  void CheckFunctionName(LanguageMode language_mode, FunctionKind kind,
                         IdentifierT function_name,
                         bool function_name_is_strict_reserved,
                         const Scanner::Location& function_name_loc,
                         bool* ok) {
    // Property names are never checked.
    if (IsConciseMethod(kind) || IsAccessorFunction(kind)) return;
    // The function name needs to be checked in strict mode.
    if (is_sloppy(language_mode)) return;

    if (this->IsEvalOrArguments(function_name)) {
      Traits::ReportMessageAt(function_name_loc, "strict_eval_arguments");
      *ok = false;
      return;
    }
    if (function_name_is_strict_reserved) {
      Traits::ReportMessageAt(function_name_loc, "unexpected_strict_reserved");
      *ok = false;
      return;
    }
    if (is_strong(language_mode) && this->IsUndefined(function_name)) {
      Traits::ReportMessageAt(function_name_loc, "strong_undefined");
      *ok = false;
      return;
    }
  }

  // Checking the parameter names of a function literal. This has to be done
  // after parsing the function, since the function can declare itself strict.
  void CheckFunctionParameterNames(LanguageMode language_mode,
                                   bool strict_params,
                                   const FormalParameterErrorLocations& locs,
                                   bool* ok) {
    if (is_sloppy(language_mode) && !strict_params) return;
    if (is_strict(language_mode) && locs.eval_or_arguments.IsValid()) {
      Traits::ReportMessageAt(locs.eval_or_arguments, "strict_eval_arguments");
      *ok = false;
      return;
    }
    if (is_strict(language_mode) && locs.reserved.IsValid()) {
      Traits::ReportMessageAt(locs.reserved, "unexpected_strict_reserved");
      *ok = false;
      return;
    }
    if (is_strong(language_mode) && locs.undefined.IsValid()) {
      Traits::ReportMessageAt(locs.undefined, "strong_undefined");
      *ok = false;
      return;
    }
    // TODO(arv): When we add support for destructuring in setters we also need
    // to check for duplicate names.
    if (locs.duplicate.IsValid()) {
      Traits::ReportMessageAt(locs.duplicate, "strict_param_dupe");
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

  // Report syntax errors.
  void ReportMessage(const char* message, const char* arg = NULL,
                     ParseErrorType error_type = kSyntaxError) {
    Scanner::Location source_location = scanner()->location();
    Traits::ReportMessageAt(source_location, message, arg, error_type);
  }

  void ReportMessageAt(Scanner::Location location, const char* message,
                       ParseErrorType error_type = kSyntaxError) {
    Traits::ReportMessageAt(location, message, reinterpret_cast<const char*>(0),
                            error_type);
  }

  void ReportUnexpectedToken(Token::Value token);
  void ReportUnexpectedTokenAt(Scanner::Location location, Token::Value token);

  class ExpressionClassifier {
   public:
    struct Error {
      Error()
          : location(Scanner::Location::invalid()),
            message(nullptr),
            arg(nullptr) {}

      Scanner::Location location;
      const char* message;
      const char* arg;

      bool HasError() const { return location.IsValid(); }
    };

    ExpressionClassifier() {}

    bool is_valid_expression() const { return !expression_error_.HasError(); }

    bool is_valid_binding_pattern() const {
      return !binding_pattern_error_.HasError();
    }

    bool is_valid_assignment_pattern() const {
      return !assignment_pattern_error_.HasError();
    }

    const Error& expression_error() const { return expression_error_; }

    const Error& binding_pattern_error() const {
      return binding_pattern_error_;
    }

    const Error& assignment_pattern_error() const {
      return assignment_pattern_error_;
    }

    void RecordExpressionError(const Scanner::Location& loc,
                               const char* message, const char* arg = nullptr) {
      if (!is_valid_expression()) return;
      expression_error_.location = loc;
      expression_error_.message = message;
      expression_error_.arg = arg;
    }

    void RecordBindingPatternError(const Scanner::Location& loc,
                                   const char* message,
                                   const char* arg = nullptr) {
      if (!is_valid_binding_pattern()) return;
      binding_pattern_error_.location = loc;
      binding_pattern_error_.message = message;
      binding_pattern_error_.arg = arg;
    }

    void RecordAssignmentPatternError(const Scanner::Location& loc,
                                      const char* message,
                                      const char* arg = nullptr) {
      if (!is_valid_assignment_pattern()) return;
      assignment_pattern_error_.location = loc;
      assignment_pattern_error_.message = message;
      assignment_pattern_error_.arg = arg;
    }

   private:
    Error expression_error_;
    Error binding_pattern_error_;
    Error assignment_pattern_error_;
  };

  void ReportClassifierError(
      const typename ExpressionClassifier::Error& error) {
    Traits::ReportMessageAt(error.location, error.message, error.arg,
                            kSyntaxError);
  }

  void ValidateExpression(const ExpressionClassifier* classifier, bool* ok) {
    if (!classifier->is_valid_expression()) {
      ReportClassifierError(classifier->expression_error());
      *ok = false;
    }
  }

  void ValidateBindingPattern(const ExpressionClassifier* classifier,
                              bool* ok) {
    if (!classifier->is_valid_binding_pattern()) {
      ReportClassifierError(classifier->binding_pattern_error());
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

  void BindingPatternUnexpectedToken(ExpressionClassifier* classifier) {
    classifier->RecordBindingPatternError(
        scanner()->location(), "unexpected_token", Token::String(peek()));
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
  // whether it is strict mode future reserved.
  IdentifierT ParseIdentifierOrStrictReservedWord(bool* is_strict_reserved,
                                                  bool* ok);
  IdentifierT ParseIdentifierName(bool* ok);
  // Parses an identifier and determines whether or not it is 'get' or 'set'.
  IdentifierT ParseIdentifierNameOrGetOrSet(bool* is_get, bool* is_set,
                                            bool* ok);


  ExpressionT ParseRegExpLiteral(bool seen_equal,
                                 ExpressionClassifier* classifier, bool* ok);

  ExpressionT ParsePrimaryExpression(ExpressionClassifier* classifier,
                                     bool* ok);
  ExpressionT ParseExpression(bool accept_IN, bool* ok);
  ExpressionT ParseExpression(bool accept_IN, ExpressionClassifier* classifier,
                              bool* ok);
  ExpressionT ParseArrayLiteral(ExpressionClassifier* classifier, bool* ok);
  ExpressionT ParsePropertyName(IdentifierT* name, bool* is_get, bool* is_set,
                                bool* is_static, bool* is_computed_name,
                                ExpressionClassifier* classifier, bool* ok);
  ExpressionT ParseObjectLiteral(ExpressionClassifier* classifier, bool* ok);
  ObjectLiteralPropertyT ParsePropertyDefinition(
      ObjectLiteralCheckerBase* checker, bool in_class, bool has_extends,
      bool is_static, bool* is_computed_name, bool* has_seen_constructor,
      ExpressionClassifier* classifier, bool* ok);
  typename Traits::Type::ExpressionList ParseArguments(
      Scanner::Location* first_spread_pos, ExpressionClassifier* classifier,
      bool* ok);
  ExpressionT ParseAssignmentExpression(bool accept_IN,
                                        ExpressionClassifier* classifier,
                                        bool* ok);
  ExpressionT ParseYieldExpression(ExpressionClassifier* classifier, bool* ok);
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
      ExpressionClassifier* classifier, bool* ok);
  ExpressionT ParseMemberExpression(ExpressionClassifier* classifier, bool* ok);
  ExpressionT ParseMemberExpressionContinuation(
      ExpressionT expression, ExpressionClassifier* classifier, bool* ok);
  ExpressionT ParseArrowFunctionLiteral(
      Scope* function_scope, const FormalParameterErrorLocations& error_locs,
      bool has_rest, ExpressionClassifier* classifier, bool* ok);
  ExpressionT ParseTemplateLiteral(ExpressionT tag, int start,
                                   ExpressionClassifier* classifier, bool* ok);
  void AddTemplateExpression(ExpressionT);
  ExpressionT ParseSuperExpression(bool is_new,
                                   ExpressionClassifier* classifier, bool* ok);
  ExpressionT ParseStrongInitializationExpression(
      ExpressionClassifier* classifier, bool* ok);
  ExpressionT ParseStrongSuperCallExpression(ExpressionClassifier* classifier,
                                             bool* ok);

  void ParseFormalParameter(FormalParameterScopeT* scope,
                            FormalParameterErrorLocations* locs, bool is_rest,
                            bool* ok);
  int ParseFormalParameterList(FormalParameterScopeT* scope,
                               FormalParameterErrorLocations* locs,
                               bool* has_rest, bool* ok);
  void CheckArityRestrictions(
      int param_count, FunctionLiteral::ArityRestriction arity_restriction,
      int formals_start_pos, int formals_end_pos, bool* ok);

  // Checks if the expression is a valid reference expression (e.g., on the
  // left-hand side of assignments). Although ruled out by ECMA as early errors,
  // we allow calls for web compatibility and rewrite them to a runtime throw.
  ExpressionT CheckAndRewriteReferenceExpression(
      ExpressionT expression,
      Scanner::Location location, const char* message, bool* ok);

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
                               bool is_static, bool is_generator, bool* ok) = 0;

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

    void CheckProperty(Token::Value property, PropertyKind type, bool is_static,
                       bool is_generator, bool* ok) override;

   private:
    bool IsProto() { return this->scanner()->LiteralMatches("__proto__", 9); }

    bool has_seen_proto_;
  };

  // Validation per ES6 class literals.
  class ClassLiteralChecker : public ObjectLiteralCheckerBase {
   public:
    explicit ClassLiteralChecker(ParserBase* parser)
        : ObjectLiteralCheckerBase(parser), has_seen_constructor_(false) {}

    void CheckProperty(Token::Value property, PropertyKind type, bool is_static,
                       bool is_generator, bool* ok) override;

   private:
    bool IsConstructor() {
      return this->scanner()->LiteralMatches("constructor", 11);
    }
    bool IsPrototype() {
      return this->scanner()->LiteralMatches("prototype", 9);
    }

    bool has_seen_constructor_;
  };

  // If true, the next (and immediately following) function literal is
  // preceded by a parenthesis.
  // Heuristically that means that the function will be called immediately,
  // so never lazily compile it.
  bool parenthesized_function_;

  Scope* scope_;                   // Scope stack.
  FunctionState* function_state_;  // Function state stack.
  v8::Extension* extension_;
  FuncNameInferrer* fni_;
  AstValueFactory* ast_value_factory_;  // Not owned.
  ParserRecorder* log_;
  Mode mode_;
  uintptr_t stack_limit_;

 private:
  Zone* zone_;

  Scanner* scanner_;
  bool stack_overflow_;

  bool allow_lazy_;
  bool allow_natives_;
  bool allow_harmony_arrow_functions_;
  bool allow_harmony_object_literals_;
  bool allow_harmony_sloppy_;
  bool allow_harmony_computed_property_names_;
  bool allow_harmony_rest_params_;
  bool allow_harmony_spreadcalls_;
  bool allow_harmony_destructuring_;
  bool allow_strong_mode_;
};


class PreParserIdentifier {
 public:
  PreParserIdentifier() : type_(kUnknownIdentifier) {}
  static PreParserIdentifier Default() {
    return PreParserIdentifier(kUnknownIdentifier);
  }
  static PreParserIdentifier Eval() {
    return PreParserIdentifier(kEvalIdentifier);
  }
  static PreParserIdentifier Arguments() {
    return PreParserIdentifier(kArgumentsIdentifier);
  }
  static PreParserIdentifier Undefined() {
    return PreParserIdentifier(kUndefinedIdentifier);
  }
  static PreParserIdentifier FutureReserved() {
    return PreParserIdentifier(kFutureReservedIdentifier);
  }
  static PreParserIdentifier FutureStrictReserved() {
    return PreParserIdentifier(kFutureStrictReservedIdentifier);
  }
  static PreParserIdentifier Let() {
    return PreParserIdentifier(kLetIdentifier);
  }
  static PreParserIdentifier Static() {
    return PreParserIdentifier(kStaticIdentifier);
  }
  static PreParserIdentifier Yield() {
    return PreParserIdentifier(kYieldIdentifier);
  }
  static PreParserIdentifier Prototype() {
    return PreParserIdentifier(kPrototypeIdentifier);
  }
  static PreParserIdentifier Constructor() {
    return PreParserIdentifier(kConstructorIdentifier);
  }
  bool IsEval() const { return type_ == kEvalIdentifier; }
  bool IsArguments() const { return type_ == kArgumentsIdentifier; }
  bool IsEvalOrArguments() const { return IsEval() || IsArguments(); }
  bool IsUndefined() const { return type_ == kUndefinedIdentifier; }
  bool IsLet() const { return type_ == kLetIdentifier; }
  bool IsStatic() const { return type_ == kStaticIdentifier; }
  bool IsYield() const { return type_ == kYieldIdentifier; }
  bool IsPrototype() const { return type_ == kPrototypeIdentifier; }
  bool IsConstructor() const { return type_ == kConstructorIdentifier; }
  bool IsFutureReserved() const { return type_ == kFutureReservedIdentifier; }
  bool IsFutureStrictReserved() const {
    return type_ == kFutureStrictReservedIdentifier ||
           type_ == kLetIdentifier || type_ == kStaticIdentifier ||
           type_ == kYieldIdentifier;
  }

  // Allow identifier->name()[->length()] to work. The preparser
  // does not need the actual positions/lengths of the identifiers.
  const PreParserIdentifier* operator->() const { return this; }
  const PreParserIdentifier raw_name() const { return *this; }

  int position() const { return 0; }
  int length() const { return 0; }

 private:
  enum Type {
    kUnknownIdentifier,
    kFutureReservedIdentifier,
    kFutureStrictReservedIdentifier,
    kLetIdentifier,
    kStaticIdentifier,
    kYieldIdentifier,
    kEvalIdentifier,
    kArgumentsIdentifier,
    kUndefinedIdentifier,
    kPrototypeIdentifier,
    kConstructorIdentifier
  };

  explicit PreParserIdentifier(Type type) : type_(type) {}
  Type type_;

  friend class PreParserExpression;
};


class PreParserExpression {
 public:
  static PreParserExpression Default() {
    return PreParserExpression(TypeField::encode(kExpression));
  }

  static PreParserExpression Spread(PreParserExpression expression) {
    return PreParserExpression(TypeField::encode(kSpreadExpression));
  }

  static PreParserExpression FromIdentifier(PreParserIdentifier id) {
    return PreParserExpression(TypeField::encode(kIdentifierExpression) |
                               IdentifierTypeField::encode(id.type_));
  }

  static PreParserExpression BinaryOperation(PreParserExpression left,
                                             Token::Value op,
                                             PreParserExpression right) {
    ValidArrowParam valid_arrow_param_list =
        (op == Token::COMMA && !left.is_single_parenthesized() &&
         !right.is_single_parenthesized())
            ? std::min(left.ValidateArrowParams(), right.ValidateArrowParams())
            : kInvalidArrowParam;
    return PreParserExpression(
        TypeField::encode(kBinaryOperationExpression) |
        IsValidArrowParamListField::encode(valid_arrow_param_list));
  }

  static PreParserExpression StringLiteral() {
    return PreParserExpression(TypeField::encode(kStringLiteralExpression));
  }

  static PreParserExpression UseStrictStringLiteral() {
    return PreParserExpression(TypeField::encode(kStringLiteralExpression) |
                               IsUseStrictField::encode(true));
  }

  static PreParserExpression UseStrongStringLiteral() {
    return PreParserExpression(TypeField::encode(kStringLiteralExpression) |
                               IsUseStrongField::encode(true));
  }

  static PreParserExpression This() {
    return PreParserExpression(TypeField::encode(kExpression) |
                               ExpressionTypeField::encode(kThisExpression));
  }

  static PreParserExpression ThisProperty() {
    return PreParserExpression(
        TypeField::encode(kExpression) |
        ExpressionTypeField::encode(kThisPropertyExpression));
  }

  static PreParserExpression Property() {
    return PreParserExpression(
        TypeField::encode(kExpression) |
        ExpressionTypeField::encode(kPropertyExpression));
  }

  static PreParserExpression Call() {
    return PreParserExpression(TypeField::encode(kExpression) |
                               ExpressionTypeField::encode(kCallExpression));
  }

  static PreParserExpression NoTemplateTag() {
    return PreParserExpression(TypeField::encode(kExpression) |
                               ExpressionTypeField::encode(
                                  kNoTemplateTagExpression));
  }

  bool IsIdentifier() const {
    return TypeField::decode(code_) == kIdentifierExpression;
  }

  PreParserIdentifier AsIdentifier() const {
    DCHECK(IsIdentifier());
    return PreParserIdentifier(IdentifierTypeField::decode(code_));
  }

  bool IsStringLiteral() const {
    return TypeField::decode(code_) == kStringLiteralExpression;
  }

  bool IsUseStrictLiteral() const {
    return TypeField::decode(code_) == kStringLiteralExpression &&
           IsUseStrictField::decode(code_);
  }

  bool IsUseStrongLiteral() const {
    return TypeField::decode(code_) == kStringLiteralExpression &&
           IsUseStrongField::decode(code_);
  }

  bool IsThis() const {
    return TypeField::decode(code_) == kExpression &&
           ExpressionTypeField::decode(code_) == kThisExpression;
  }

  bool IsThisProperty() const {
    return TypeField::decode(code_) == kExpression &&
           ExpressionTypeField::decode(code_) == kThisPropertyExpression;
  }

  bool IsProperty() const {
    return TypeField::decode(code_) == kExpression &&
           (ExpressionTypeField::decode(code_) == kPropertyExpression ||
            ExpressionTypeField::decode(code_) == kThisPropertyExpression);
  }

  bool IsCall() const {
    return TypeField::decode(code_) == kExpression &&
           ExpressionTypeField::decode(code_) == kCallExpression;
  }

  bool IsValidReferenceExpression() const {
    return IsIdentifier() || IsProperty();
  }

  bool IsValidArrowParamList(FormalParameterErrorLocations* locs,
                             const Scanner::Location& params_loc) const {
    ValidArrowParam valid = ValidateArrowParams();
    if (ParenthesizationField::decode(code_) == kMultiParenthesizedExpression) {
      return false;
    }
    switch (valid) {
      case kInvalidArrowParam:
        return false;
      case kInvalidStrongArrowParam:
        locs->undefined = params_loc;
        return true;
      case kInvalidStrictReservedArrowParam:
        locs->reserved = params_loc;
        return true;
      case kInvalidStrictEvalArgumentsArrowParam:
        locs->eval_or_arguments = params_loc;
        return true;
      default:
        DCHECK_EQ(valid, kValidArrowParam);
        return true;
    }
  }

  // At the moment PreParser doesn't track these expression types.
  bool IsFunctionLiteral() const { return false; }
  bool IsCallNew() const { return false; }

  bool IsNoTemplateTag() const {
    return TypeField::decode(code_) == kExpression &&
           ExpressionTypeField::decode(code_) == kNoTemplateTagExpression;
  }

  bool IsSpreadExpression() const {
    return TypeField::decode(code_) == kSpreadExpression;
  }

  PreParserExpression AsFunctionLiteral() { return *this; }

  bool IsBinaryOperation() const {
    return TypeField::decode(code_) == kBinaryOperationExpression;
  }

  bool is_single_parenthesized() const {
    return ParenthesizationField::decode(code_) != kNotParenthesized;
  }

  void increase_parenthesization_level() {
    code_ = ParenthesizationField::update(
        code_, is_single_parenthesized() ? kMultiParenthesizedExpression
                                         : kParanthesizedExpression);
  }

  // Dummy implementation for making expression->somefunc() work in both Parser
  // and PreParser.
  PreParserExpression* operator->() { return this; }

  // More dummy implementations of things PreParser doesn't need to track:
  void set_index(int index) {}  // For YieldExpressions
  void set_should_eager_compile() {}

  int position() const { return RelocInfo::kNoPosition; }
  void set_function_token_position(int position) {}

 private:
  enum Type {
    kExpression,
    kIdentifierExpression,
    kStringLiteralExpression,
    kBinaryOperationExpression,
    kSpreadExpression
  };

  enum Parenthesization {
    kNotParenthesized,
    kParanthesizedExpression,
    kMultiParenthesizedExpression
  };

  enum ExpressionType {
    kThisExpression,
    kThisPropertyExpression,
    kPropertyExpression,
    kCallExpression,
    kNoTemplateTagExpression
  };

  // These validity constraints are ordered such that a value of N implies lack
  // of errors M < N.
  enum ValidArrowParam {
    kInvalidArrowParam,
    kInvalidStrictEvalArgumentsArrowParam,
    kInvalidStrictReservedArrowParam,
    kInvalidStrongArrowParam,
    kValidArrowParam
  };

  explicit PreParserExpression(uint32_t expression_code)
      : code_(expression_code) {}

  V8_INLINE ValidArrowParam ValidateArrowParams() const {
    if (IsBinaryOperation()) {
      return IsValidArrowParamListField::decode(code_);
    }
    if (!IsIdentifier()) {
      return kInvalidArrowParam;
    }
    PreParserIdentifier ident = AsIdentifier();
    // In strict mode, eval and arguments are not valid formal parameter names.
    if (ident.IsEval() || ident.IsArguments()) {
      return kInvalidStrictEvalArgumentsArrowParam;
    }
    // In strict mode, future reserved words are not valid either, and as they
    // produce different errors we allot them their own error code.
    if (ident.IsFutureStrictReserved()) {
      return kInvalidStrictReservedArrowParam;
    }
    // In strong mode, 'undefined' isn't a valid formal parameter name either.
    if (ident.IsUndefined()) {
      return kInvalidStrongArrowParam;
    }
    return kValidArrowParam;
  }

  // The first five bits are for the Type and Parenthesization.
  typedef BitField<Type, 0, 3> TypeField;
  typedef BitField<Parenthesization, TypeField::kNext, 2> ParenthesizationField;

  // The rest of the bits are interpreted depending on the value
  // of the Type field, so they can share the storage.
  typedef BitField<ExpressionType, ParenthesizationField::kNext, 3>
      ExpressionTypeField;
  typedef BitField<bool, ParenthesizationField::kNext, 1> IsUseStrictField;
  typedef BitField<bool, IsUseStrictField::kNext, 1> IsUseStrongField;
  typedef BitField<ValidArrowParam, ParenthesizationField::kNext, 3>
      IsValidArrowParamListField;
  typedef BitField<PreParserIdentifier::Type, ParenthesizationField::kNext, 10>
      IdentifierTypeField;

  uint32_t code_;
};


// The pre-parser doesn't need to build lists of expressions, identifiers, or
// the like.
template <typename T>
class PreParserList {
 public:
  // These functions make list->Add(some_expression) work (and do nothing).
  PreParserList() : length_(0) {}
  PreParserList* operator->() { return this; }
  void Add(T, void*) { ++length_; }
  int length() const { return length_; }
 private:
  int length_;
};


typedef PreParserList<PreParserExpression> PreParserExpressionList;


class PreParserStatement {
 public:
  static PreParserStatement Default() {
    return PreParserStatement(kUnknownStatement);
  }

  static PreParserStatement Jump() {
    return PreParserStatement(kJumpStatement);
  }

  static PreParserStatement FunctionDeclaration() {
    return PreParserStatement(kFunctionDeclaration);
  }

  // Creates expression statement from expression.
  // Preserves being an unparenthesized string literal, possibly
  // "use strict".
  static PreParserStatement ExpressionStatement(
      PreParserExpression expression) {
    if (expression.IsUseStrictLiteral()) {
      return PreParserStatement(kUseStrictExpressionStatement);
    }
    if (expression.IsUseStrongLiteral()) {
      return PreParserStatement(kUseStrongExpressionStatement);
    }
    if (expression.IsStringLiteral()) {
      return PreParserStatement(kStringLiteralExpressionStatement);
    }
    return Default();
  }

  bool IsStringLiteral() {
    return code_ == kStringLiteralExpressionStatement;
  }

  bool IsUseStrictLiteral() {
    return code_ == kUseStrictExpressionStatement;
  }

  bool IsUseStrongLiteral() { return code_ == kUseStrongExpressionStatement; }

  bool IsFunctionDeclaration() {
    return code_ == kFunctionDeclaration;
  }

  bool IsJumpStatement() {
    return code_ == kJumpStatement;
  }

 private:
  enum Type {
    kUnknownStatement,
    kJumpStatement,
    kStringLiteralExpressionStatement,
    kUseStrictExpressionStatement,
    kUseStrongExpressionStatement,
    kFunctionDeclaration
  };

  explicit PreParserStatement(Type code) : code_(code) {}
  Type code_;
};


typedef PreParserList<PreParserStatement> PreParserStatementList;


class PreParserFactory {
 public:
  explicit PreParserFactory(void* unused_value_factory) {}
  PreParserExpression NewStringLiteral(PreParserIdentifier identifier,
                                       int pos) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewNumberLiteral(double number,
                                       int pos) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewRegExpLiteral(PreParserIdentifier js_pattern,
                                       PreParserIdentifier js_flags,
                                       int literal_index,
                                       int pos) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewArrayLiteral(PreParserExpressionList values,
                                      int literal_index,
                                      int pos) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewObjectLiteralProperty(PreParserExpression key,
                                               PreParserExpression value,
                                               ObjectLiteralProperty::Kind kind,
                                               bool is_static,
                                               bool is_computed_name) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewObjectLiteralProperty(PreParserExpression key,
                                               PreParserExpression value,
                                               bool is_static,
                                               bool is_computed_name) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewObjectLiteral(PreParserExpressionList properties,
                                       int literal_index,
                                       int boilerplate_properties,
                                       bool has_function,
                                       int pos) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewVariableProxy(void* variable) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewProperty(PreParserExpression obj,
                                  PreParserExpression key,
                                  int pos) {
    if (obj.IsThis()) {
      return PreParserExpression::ThisProperty();
    }
    return PreParserExpression::Property();
  }
  PreParserExpression NewUnaryOperation(Token::Value op,
                                        PreParserExpression expression,
                                        int pos) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewBinaryOperation(Token::Value op,
                                         PreParserExpression left,
                                         PreParserExpression right, int pos) {
    return PreParserExpression::BinaryOperation(left, op, right);
  }
  PreParserExpression NewCompareOperation(Token::Value op,
                                          PreParserExpression left,
                                          PreParserExpression right, int pos) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewAssignment(Token::Value op,
                                    PreParserExpression left,
                                    PreParserExpression right,
                                    int pos) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewYield(PreParserExpression generator_object,
                               PreParserExpression expression,
                               Yield::Kind yield_kind,
                               int pos) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewConditional(PreParserExpression condition,
                                     PreParserExpression then_expression,
                                     PreParserExpression else_expression,
                                     int pos) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewCountOperation(Token::Value op,
                                        bool is_prefix,
                                        PreParserExpression expression,
                                        int pos) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewCall(PreParserExpression expression,
                              PreParserExpressionList arguments,
                              int pos) {
    return PreParserExpression::Call();
  }
  PreParserExpression NewCallNew(PreParserExpression expression,
                                 PreParserExpressionList arguments,
                                 int pos) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewCallRuntime(const AstRawString* name,
                                     const Runtime::Function* function,
                                     PreParserExpressionList arguments,
                                     int pos) {
    return PreParserExpression::Default();
  }
  PreParserStatement NewReturnStatement(PreParserExpression expression,
                                        int pos) {
    return PreParserStatement::Default();
  }
  PreParserExpression NewFunctionLiteral(
      PreParserIdentifier name, AstValueFactory* ast_value_factory,
      Scope* scope, PreParserStatementList body, int materialized_literal_count,
      int expected_property_count, int handler_count, int parameter_count,
      FunctionLiteral::ParameterFlag has_duplicate_parameters,
      FunctionLiteral::FunctionType function_type,
      FunctionLiteral::IsFunctionFlag is_function,
      FunctionLiteral::EagerCompileHint eager_compile_hint, FunctionKind kind,
      int position) {
    return PreParserExpression::Default();
  }

  PreParserExpression NewSpread(PreParserExpression expression, int pos) {
    return PreParserExpression::Spread(expression);
  }

  // Return the object itself as AstVisitor and implement the needed
  // dummy method right in this class.
  PreParserFactory* visitor() { return this; }
  int* ast_properties() {
    static int dummy = 42;
    return &dummy;
  }
};


class PreParser;

class PreParserTraits {
 public:
  struct Type {
    // TODO(marja): To be removed. The Traits object should contain all the data
    // it needs.
    typedef PreParser* Parser;

    // PreParser doesn't need to store generator variables.
    typedef void GeneratorVariable;

    typedef int AstProperties;

    // Return types for traversing functions.
    typedef PreParserIdentifier Identifier;
    typedef PreParserExpression Expression;
    typedef PreParserExpression YieldExpression;
    typedef PreParserExpression FunctionLiteral;
    typedef PreParserExpression ClassLiteral;
    typedef PreParserExpression ObjectLiteralProperty;
    typedef PreParserExpression Literal;
    typedef PreParserExpressionList ExpressionList;
    typedef PreParserExpressionList PropertyList;
    typedef PreParserIdentifier FormalParameter;
    typedef DuplicateFinder FormalParameterScope;
    typedef PreParserStatementList StatementList;

    // For constructing objects returned by the traversing functions.
    typedef PreParserFactory Factory;
  };

  explicit PreParserTraits(PreParser* pre_parser) : pre_parser_(pre_parser) {}

  // Helper functions for recursive descent.
  static bool IsEval(PreParserIdentifier identifier) {
    return identifier.IsEval();
  }

  static bool IsArguments(PreParserIdentifier identifier) {
    return identifier.IsArguments();
  }

  static bool IsEvalOrArguments(PreParserIdentifier identifier) {
    return identifier.IsEvalOrArguments();
  }

  static bool IsUndefined(PreParserIdentifier identifier) {
    return identifier.IsUndefined();
  }

  static bool IsPrototype(PreParserIdentifier identifier) {
    return identifier.IsPrototype();
  }

  static bool IsConstructor(PreParserIdentifier identifier) {
    return identifier.IsConstructor();
  }

  // Returns true if the expression is of type "this.foo".
  static bool IsThisProperty(PreParserExpression expression) {
    return expression.IsThisProperty();
  }

  static bool IsIdentifier(PreParserExpression expression) {
    return expression.IsIdentifier();
  }

  static PreParserIdentifier AsIdentifier(PreParserExpression expression) {
    return expression.AsIdentifier();
  }

  static bool IsFutureStrictReserved(PreParserIdentifier identifier) {
    return identifier.IsFutureStrictReserved();
  }

  static bool IsBoilerplateProperty(PreParserExpression property) {
    // PreParser doesn't count boilerplate properties.
    return false;
  }

  static bool IsArrayIndex(PreParserIdentifier string, uint32_t* index) {
    return false;
  }

  static PreParserExpression GetPropertyValue(PreParserExpression property) {
    return PreParserExpression::Default();
  }

  // Functions for encapsulating the differences between parsing and preparsing;
  // operations interleaved with the recursive descent.
  static void PushLiteralName(FuncNameInferrer* fni, PreParserIdentifier id) {
    // PreParser should not use FuncNameInferrer.
    UNREACHABLE();
  }

  static void PushPropertyName(FuncNameInferrer* fni,
                               PreParserExpression expression) {
    // PreParser should not use FuncNameInferrer.
    UNREACHABLE();
  }

  static void InferFunctionName(FuncNameInferrer* fni,
                                PreParserExpression expression) {
    // PreParser should not use FuncNameInferrer.
    UNREACHABLE();
  }

  static void CheckFunctionLiteralInsideTopLevelObjectLiteral(
      Scope* scope, PreParserExpression property, bool* has_function) {}

  static void CheckAssigningFunctionLiteralToProperty(
      PreParserExpression left, PreParserExpression right) {}

  // PreParser doesn't need to keep track of eval calls.
  static void CheckPossibleEvalCall(PreParserExpression expression,
                                    Scope* scope) {}

  static PreParserExpression MarkExpressionAsAssigned(
      PreParserExpression expression) {
    // TODO(marja): To be able to produce the same errors, the preparser needs
    // to start tracking which expressions are variables and which are assigned.
    return expression;
  }

  bool ShortcutNumericLiteralBinaryExpression(PreParserExpression* x,
                                              PreParserExpression y,
                                              Token::Value op,
                                              int pos,
                                              PreParserFactory* factory) {
    return false;
  }

  PreParserExpression BuildUnaryExpression(PreParserExpression expression,
                                           Token::Value op, int pos,
                                           PreParserFactory* factory) {
    return PreParserExpression::Default();
  }

  PreParserExpression NewThrowReferenceError(const char* type, int pos) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewThrowSyntaxError(
      const char* type, Handle<Object> arg, int pos) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewThrowTypeError(
      const char* type, Handle<Object> arg, int pos) {
    return PreParserExpression::Default();
  }

  // Reporting errors.
  void ReportMessageAt(Scanner::Location location, const char* message,
                       const char* arg = NULL,
                       ParseErrorType error_type = kSyntaxError);
  void ReportMessageAt(int start_pos, int end_pos, const char* message,
                       const char* arg = NULL,
                       ParseErrorType error_type = kSyntaxError);

  // "null" return type creators.
  static PreParserIdentifier EmptyIdentifier() {
    return PreParserIdentifier::Default();
  }
  static PreParserIdentifier EmptyIdentifierString() {
    return PreParserIdentifier::Default();
  }
  static PreParserExpression EmptyExpression() {
    return PreParserExpression::Default();
  }
  static PreParserExpression EmptyLiteral() {
    return PreParserExpression::Default();
  }
  static PreParserExpression EmptyObjectLiteralProperty() {
    return PreParserExpression::Default();
  }
  static PreParserExpression EmptyFunctionLiteral() {
    return PreParserExpression::Default();
  }
  static PreParserExpressionList NullExpressionList() {
    return PreParserExpressionList();
  }

  // Odd-ball literal creators.
  static PreParserExpression GetLiteralTheHole(int position,
                                               PreParserFactory* factory) {
    return PreParserExpression::Default();
  }

  // Producing data during the recursive descent.
  PreParserIdentifier GetSymbol(Scanner* scanner);
  PreParserIdentifier GetNumberAsSymbol(Scanner* scanner);

  static PreParserIdentifier GetNextSymbol(Scanner* scanner) {
    return PreParserIdentifier::Default();
  }

  static PreParserExpression ThisExpression(Scope* scope,
                                            PreParserFactory* factory,
                                            int pos) {
    return PreParserExpression::This();
  }

  static PreParserExpression SuperReference(Scope* scope,
                                            PreParserFactory* factory) {
    return PreParserExpression::Default();
  }

  static PreParserExpression DefaultConstructor(bool call_super, Scope* scope,
                                                int pos, int end_pos) {
    return PreParserExpression::Default();
  }

  static PreParserExpression ExpressionFromLiteral(
      Token::Value token, int pos, Scanner* scanner,
      PreParserFactory* factory) {
    return PreParserExpression::Default();
  }

  static PreParserExpression ExpressionFromIdentifier(
      PreParserIdentifier name, int start_position, int end_position,
      Scope* scope, PreParserFactory* factory) {
    return PreParserExpression::FromIdentifier(name);
  }

  PreParserExpression ExpressionFromString(int pos,
                                           Scanner* scanner,
                                           PreParserFactory* factory = NULL);

  PreParserExpression GetIterator(PreParserExpression iterable,
                                  PreParserFactory* factory) {
    return PreParserExpression::Default();
  }

  static PreParserExpressionList NewExpressionList(int size, Zone* zone) {
    return PreParserExpressionList();
  }

  static PreParserStatementList NewStatementList(int size, Zone* zone) {
    return PreParserStatementList();
  }

  static PreParserExpressionList NewPropertyList(int size, Zone* zone) {
    return PreParserExpressionList();
  }

  V8_INLINE void SkipLazyFunctionBody(int* materialized_literal_count,
                                      int* expected_property_count, bool* ok) {
    UNREACHABLE();
  }

  V8_INLINE PreParserStatementList
  ParseEagerFunctionBody(PreParserIdentifier function_name, int pos,
                         Variable* fvar, Token::Value fvar_init_op,
                         FunctionKind kind, bool* ok);

  V8_INLINE void ParseArrowFunctionFormalParameters(
      Scope* scope, PreParserExpression expression,
      const Scanner::Location& params_loc,
      FormalParameterErrorLocations* error_locs, bool* is_rest, bool* ok);

  struct TemplateLiteralState {};

  TemplateLiteralState OpenTemplateLiteral(int pos) {
    return TemplateLiteralState();
  }
  void AddTemplateSpan(TemplateLiteralState*, bool) {}
  void AddTemplateExpression(TemplateLiteralState*, PreParserExpression) {}
  PreParserExpression CloseTemplateLiteral(TemplateLiteralState*, int,
                                           PreParserExpression tag) {
    if (IsTaggedTemplate(tag)) {
      // Emulate generation of array literals for tag callsite
      // 1st is array of cooked strings, second is array of raw strings
      MaterializeTemplateCallsiteLiterals();
    }
    return EmptyExpression();
  }
  inline void MaterializeTemplateCallsiteLiterals();
  PreParserExpression NoTemplateTag() {
    return PreParserExpression::NoTemplateTag();
  }
  static bool IsTaggedTemplate(const PreParserExpression tag) {
    return !tag.IsNoTemplateTag();
  }

  V8_INLINE bool DeclareFormalParameter(DuplicateFinder* scope,
                                        PreParserIdentifier param,
                                        bool is_rest);

  void CheckConflictingVarDeclarations(Scope* scope, bool* ok) {}

  // Temporary glue; these functions will move to ParserBase.
  PreParserExpression ParseV8Intrinsic(bool* ok);
  PreParserExpression ParseFunctionLiteral(
      PreParserIdentifier name, Scanner::Location function_name_location,
      bool name_is_strict_reserved, FunctionKind kind,
      int function_token_position, FunctionLiteral::FunctionType type,
      FunctionLiteral::ArityRestriction arity_restriction, bool* ok);

  PreParserExpression ParseClassLiteral(PreParserIdentifier name,
                                        Scanner::Location class_name_location,
                                        bool name_is_strict_reserved, int pos,
                                        bool* ok);

  PreParserExpressionList PrepareSpreadArguments(PreParserExpressionList list) {
    return list;
  }

  inline void MaterializeUnspreadArgumentsLiterals(int count);

  inline PreParserExpression SpreadCall(PreParserExpression function,
                                        PreParserExpressionList args, int pos);

  inline PreParserExpression SpreadCallNew(PreParserExpression function,
                                           PreParserExpressionList args,
                                           int pos);

 private:
  PreParser* pre_parser_;
};


// Preparsing checks a JavaScript program and emits preparse-data that helps
// a later parsing to be faster.
// See preparse-data-format.h for the data format.

// The PreParser checks that the syntax follows the grammar for JavaScript,
// and collects some information about the program along the way.
// The grammar check is only performed in order to understand the program
// sufficiently to deduce some information about it, that can be used
// to speed up later parsing. Finding errors is not the goal of pre-parsing,
// rather it is to speed up properly written and correct programs.
// That means that contextual checks (like a label being declared where
// it is used) are generally omitted.
class PreParser : public ParserBase<PreParserTraits> {
 public:
  typedef PreParserIdentifier Identifier;
  typedef PreParserExpression Expression;
  typedef PreParserStatement Statement;

  enum PreParseResult {
    kPreParseStackOverflow,
    kPreParseSuccess
  };

  PreParser(Zone* zone, Scanner* scanner, AstValueFactory* ast_value_factory,
            ParserRecorder* log, uintptr_t stack_limit)
      : ParserBase<PreParserTraits>(zone, scanner, stack_limit, NULL,
                                    ast_value_factory, log, this) {}

  // Pre-parse the program from the character stream; returns true on
  // success (even if parsing failed, the pre-parse data successfully
  // captured the syntax error), and false if a stack-overflow happened
  // during parsing.
  PreParseResult PreParseProgram(int* materialized_literals = 0) {
    Scope* scope = NewScope(scope_, SCRIPT_SCOPE);
    PreParserFactory factory(NULL);
    FunctionState top_scope(&function_state_, &scope_, scope, kNormalFunction,
                            &factory);
    bool ok = true;
    int start_position = scanner()->peek_location().beg_pos;
    ParseStatementList(Token::EOS, &ok);
    if (stack_overflow()) return kPreParseStackOverflow;
    if (!ok) {
      ReportUnexpectedToken(scanner()->current_token());
    } else if (is_strict(scope_->language_mode())) {
      CheckStrictOctalLiteral(start_position, scanner()->location().end_pos,
                              &ok);
    }
    if (materialized_literals) {
      *materialized_literals = function_state_->materialized_literal_count();
    }
    return kPreParseSuccess;
  }

  // Parses a single function literal, from the opening parentheses before
  // parameters to the closing brace after the body.
  // Returns a FunctionEntry describing the body of the function in enough
  // detail that it can be lazily compiled.
  // The scanner is expected to have matched the "function" or "function*"
  // keyword and parameters, and have consumed the initial '{'.
  // At return, unless an error occurred, the scanner is positioned before the
  // the final '}'.
  PreParseResult PreParseLazyFunction(
      LanguageMode language_mode, FunctionKind kind, ParserRecorder* log,
      Scanner::BookmarkScope* bookmark = nullptr);

 private:
  friend class PreParserTraits;

  static const int kLazyParseTrialLimit = 200;

  // These types form an algebra over syntactic categories that is just
  // rich enough to let us recognize and propagate the constructs that
  // are either being counted in the preparser data, or is important
  // to throw the correct syntax error exceptions.

  // All ParseXXX functions take as the last argument an *ok parameter
  // which is set to false if parsing failed; it is unchanged otherwise.
  // By making the 'exception handling' explicit, we are forced to check
  // for failure at the call sites.
  Statement ParseStatementListItem(bool* ok);
  void ParseStatementList(int end_token, bool* ok,
                          Scanner::BookmarkScope* bookmark = nullptr);
  Statement ParseStatement(bool* ok);
  Statement ParseSubStatement(bool* ok);
  Statement ParseFunctionDeclaration(bool* ok);
  Statement ParseClassDeclaration(bool* ok);
  Statement ParseBlock(bool* ok);
  Statement ParseVariableStatement(VariableDeclarationContext var_context,
                                   bool* ok);
  Statement ParseVariableDeclarations(VariableDeclarationContext var_context,
                                      int* num_decl,
                                      Scanner::Location* first_initializer_loc,
                                      Scanner::Location* bindings_loc,
                                      bool* ok);
  Statement ParseExpressionOrLabelledStatement(bool* ok);
  Statement ParseIfStatement(bool* ok);
  Statement ParseContinueStatement(bool* ok);
  Statement ParseBreakStatement(bool* ok);
  Statement ParseReturnStatement(bool* ok);
  Statement ParseWithStatement(bool* ok);
  Statement ParseSwitchStatement(bool* ok);
  Statement ParseDoWhileStatement(bool* ok);
  Statement ParseWhileStatement(bool* ok);
  Statement ParseForStatement(bool* ok);
  Statement ParseThrowStatement(bool* ok);
  Statement ParseTryStatement(bool* ok);
  Statement ParseDebuggerStatement(bool* ok);
  Expression ParseConditionalExpression(bool accept_IN, bool* ok);
  Expression ParseObjectLiteral(bool* ok);
  Expression ParseV8Intrinsic(bool* ok);

  V8_INLINE void SkipLazyFunctionBody(int* materialized_literal_count,
                                      int* expected_property_count, bool* ok);
  V8_INLINE PreParserStatementList
  ParseEagerFunctionBody(PreParserIdentifier function_name, int pos,
                         Variable* fvar, Token::Value fvar_init_op,
                         FunctionKind kind, bool* ok);

  Expression ParseFunctionLiteral(
      Identifier name, Scanner::Location function_name_location,
      bool name_is_strict_reserved, FunctionKind kind, int function_token_pos,
      FunctionLiteral::FunctionType function_type,
      FunctionLiteral::ArityRestriction arity_restriction, bool* ok);
  void ParseLazyFunctionLiteralBody(bool* ok,
                                    Scanner::BookmarkScope* bookmark = nullptr);

  PreParserExpression ParseClassLiteral(PreParserIdentifier name,
                                        Scanner::Location class_name_location,
                                        bool name_is_strict_reserved, int pos,
                                        bool* ok);
};


void PreParserTraits::MaterializeTemplateCallsiteLiterals() {
  pre_parser_->function_state_->NextMaterializedLiteralIndex();
  pre_parser_->function_state_->NextMaterializedLiteralIndex();
}


void PreParserTraits::MaterializeUnspreadArgumentsLiterals(int count) {
  for (int i = 0; i < count; ++i) {
    pre_parser_->function_state_->NextMaterializedLiteralIndex();
  }
}


PreParserExpression PreParserTraits::SpreadCall(PreParserExpression function,
                                                PreParserExpressionList args,
                                                int pos) {
  return pre_parser_->factory()->NewCall(function, args, pos);
}

PreParserExpression PreParserTraits::SpreadCallNew(PreParserExpression function,
                                                   PreParserExpressionList args,
                                                   int pos) {
  return pre_parser_->factory()->NewCallNew(function, args, pos);
}


bool PreParserTraits::DeclareFormalParameter(
    DuplicateFinder* duplicate_finder, PreParserIdentifier current_identifier,
    bool is_rest) {
  return pre_parser_->scanner()->FindSymbol(duplicate_finder, 1) != 0;
}


void PreParserTraits::ParseArrowFunctionFormalParameters(
    Scope* scope, PreParserExpression params,
    const Scanner::Location& params_loc,
    FormalParameterErrorLocations* error_locs, bool* is_rest, bool* ok) {
  // TODO(wingo): Detect duplicated identifiers in paramlists.  Detect parameter
  // lists that are too long.
  if (!params.IsValidArrowParamList(error_locs, params_loc)) {
    *ok = false;
    ReportMessageAt(params_loc, "malformed_arrow_function_parameter_list");
    return;
  }
}


PreParserStatementList PreParser::ParseEagerFunctionBody(
    PreParserIdentifier function_name, int pos, Variable* fvar,
    Token::Value fvar_init_op, FunctionKind kind, bool* ok) {
  ParsingModeScope parsing_mode(this, PARSE_EAGERLY);

  ParseStatementList(Token::RBRACE, ok);
  if (!*ok) return PreParserStatementList();

  Expect(Token::RBRACE, ok);
  return PreParserStatementList();
}


PreParserStatementList PreParserTraits::ParseEagerFunctionBody(
    PreParserIdentifier function_name, int pos, Variable* fvar,
    Token::Value fvar_init_op, FunctionKind kind, bool* ok) {
  return pre_parser_->ParseEagerFunctionBody(function_name, pos, fvar,
                                             fvar_init_op, kind, ok);
}


template <class Traits>
ParserBase<Traits>::FunctionState::FunctionState(
    FunctionState** function_state_stack, Scope** scope_stack, Scope* scope,
    FunctionKind kind, typename Traits::Type::Factory* factory)
    : next_materialized_literal_index_(0),
      next_handler_index_(0),
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
      factory_(factory) {
  *scope_stack_ = scope;
  *function_state_stack = this;
}


template <class Traits>
ParserBase<Traits>::FunctionState::~FunctionState() {
  *scope_stack_ = outer_scope_;
  *function_state_stack_ = outer_function_state_;
}


template<class Traits>
void ParserBase<Traits>::ReportUnexpectedToken(Token::Value token) {
  return ReportUnexpectedTokenAt(scanner_->location(), token);
}


template<class Traits>
void ParserBase<Traits>::ReportUnexpectedTokenAt(
    Scanner::Location source_location, Token::Value token) {

  // Four of the tokens are treated specially
  switch (token) {
    case Token::EOS:
      return ReportMessageAt(source_location, "unexpected_eos");
    case Token::SMI:
    case Token::NUMBER:
      return ReportMessageAt(source_location, "unexpected_token_number");
    case Token::STRING:
      return ReportMessageAt(source_location, "unexpected_token_string");
    case Token::IDENTIFIER:
      return ReportMessageAt(source_location, "unexpected_token_identifier");
    case Token::FUTURE_RESERVED_WORD:
      return ReportMessageAt(source_location, "unexpected_reserved");
    case Token::LET:
    case Token::STATIC:
    case Token::YIELD:
    case Token::FUTURE_STRICT_RESERVED_WORD:
      return ReportMessageAt(source_location,
                             is_strict(language_mode())
                                 ? "unexpected_strict_reserved"
                                 : "unexpected_token_identifier");
    case Token::TEMPLATE_SPAN:
    case Token::TEMPLATE_TAIL:
      return Traits::ReportMessageAt(source_location,
          "unexpected_template_string");
    default:
      const char* name = Token::String(token);
      DCHECK(name != NULL);
      Traits::ReportMessageAt(source_location, "unexpected_token", name);
  }
}


template <class Traits>
typename ParserBase<Traits>::IdentifierT ParserBase<Traits>::ParseIdentifier(
    AllowRestrictedIdentifiers allow_restricted_identifiers, bool* ok) {
  ExpressionClassifier classifier;
  auto result = ParseAndClassifyIdentifier(&classifier, ok);
  if (!*ok) return Traits::EmptyIdentifier();

  if (allow_restricted_identifiers == kDontAllowRestrictedIdentifiers) {
    ValidateAssignmentPattern(&classifier, ok);
    if (!*ok) return Traits::EmptyIdentifier();
    ValidateBindingPattern(&classifier, ok);
    if (!*ok) return Traits::EmptyIdentifier();
  } else {
    ValidateExpression(&classifier, ok);
    if (!*ok) return Traits::EmptyIdentifier();
  }

  return result;
}


template <class Traits>
typename ParserBase<Traits>::IdentifierT
ParserBase<Traits>::ParseAndClassifyIdentifier(ExpressionClassifier* classifier,
                                               bool* ok) {
  Token::Value next = Next();
  if (next == Token::IDENTIFIER) {
    IdentifierT name = this->GetSymbol(scanner());
    if (is_strict(language_mode()) && this->IsEvalOrArguments(name)) {
      classifier->RecordBindingPatternError(scanner()->location(),
                                            "strict_eval_arguments");
    }
    if (is_strong(language_mode()) && this->IsUndefined(name)) {
      // TODO(dslomov): allow 'undefined' in nested patterns.
      classifier->RecordBindingPatternError(scanner()->location(),
                                            "strong_undefined");
      classifier->RecordAssignmentPatternError(scanner()->location(),
                                               "strong_undefined");
    }
    if (is_strong(language_mode()) && this->IsArguments(name)) {
      classifier->RecordExpressionError(scanner()->location(),
                                        "strong_arguments");
    }
    if (this->IsArguments(name)) scope_->RecordArgumentsUsage();
    return name;
  } else if (is_sloppy(language_mode()) &&
             (next == Token::FUTURE_STRICT_RESERVED_WORD ||
              next == Token::LET || next == Token::STATIC ||
              (next == Token::YIELD && !is_generator()))) {
    return this->GetSymbol(scanner());
  } else {
    this->ReportUnexpectedToken(next);
    *ok = false;
    return Traits::EmptyIdentifier();
  }
}


template <class Traits>
typename ParserBase<Traits>::IdentifierT ParserBase<
    Traits>::ParseIdentifierOrStrictReservedWord(bool* is_strict_reserved,
                                                 bool* ok) {
  Token::Value next = Next();
  if (next == Token::IDENTIFIER) {
    *is_strict_reserved = false;
  } else if (next == Token::FUTURE_STRICT_RESERVED_WORD || next == Token::LET ||
             next == Token::STATIC ||
             (next == Token::YIELD && !this->is_generator())) {
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
  if (next != Token::IDENTIFIER && next != Token::FUTURE_RESERVED_WORD &&
      next != Token::LET && next != Token::STATIC && next != Token::YIELD &&
      next != Token::FUTURE_STRICT_RESERVED_WORD && !Token::IsKeyword(next)) {
    this->ReportUnexpectedToken(next);
    *ok = false;
    return Traits::EmptyIdentifier();
  }

  IdentifierT name = this->GetSymbol(scanner());
  if (this->IsArguments(name)) scope_->RecordArgumentsUsage();
  return name;
}


template <class Traits>
typename ParserBase<Traits>::IdentifierT
ParserBase<Traits>::ParseIdentifierNameOrGetOrSet(bool* is_get,
                                                  bool* is_set,
                                                  bool* ok) {
  IdentifierT result = ParseIdentifierName(ok);
  if (!*ok) return Traits::EmptyIdentifier();
  scanner()->IsGetOrSet(is_get, is_set);
  return result;
}


template <class Traits>
typename ParserBase<Traits>::ExpressionT ParserBase<Traits>::ParseRegExpLiteral(
    bool seen_equal, ExpressionClassifier* classifier, bool* ok) {
  int pos = peek_position();
  if (!scanner()->ScanRegExpPattern(seen_equal)) {
    Next();
    ReportMessage("unterminated_regexp");
    *ok = false;
    return Traits::EmptyExpression();
  }

  int literal_index = function_state_->NextMaterializedLiteralIndex();

  IdentifierT js_pattern = this->GetNextSymbol(scanner());
  if (!scanner()->ScanRegExpFlags()) {
    Next();
    ReportMessage("malformed_regexp_flags");
    *ok = false;
    return Traits::EmptyExpression();
  }
  IdentifierT js_flags = this->GetNextSymbol(scanner());
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
                                           bool* ok) {
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

  int beg_pos = scanner()->peek_location().beg_pos;
  int end_pos = scanner()->peek_location().end_pos;
  ExpressionT result = this->EmptyExpression();
  Token::Value token = peek();
  switch (token) {
    case Token::THIS: {
      BindingPatternUnexpectedToken(classifier);
      Consume(Token::THIS);
      if (is_strong(language_mode())) {
        // Constructors' usages of 'this' in strong mode are parsed separately.
        // TODO(rossberg): this does not work with arrow functions yet.
        if (i::IsConstructor(function_state_->kind())) {
          ReportMessage("strong_constructor_this");
          *ok = false;
          break;
        }
      }
      scope_->RecordThisUsage();
      result = this->ThisExpression(scope_, factory(), beg_pos);
      break;
    }

    case Token::NULL_LITERAL:
    case Token::TRUE_LITERAL:
    case Token::FALSE_LITERAL:
      BindingPatternUnexpectedToken(classifier);
      Next();
      result =
          this->ExpressionFromLiteral(token, beg_pos, scanner(), factory());
      break;
    case Token::SMI:
    case Token::NUMBER:
      classifier->RecordBindingPatternError(scanner()->location(),
                                            "unexpected_token_number");
      Next();
      result =
          this->ExpressionFromLiteral(token, beg_pos, scanner(), factory());
      break;

    case Token::IDENTIFIER:
    case Token::LET:
    case Token::STATIC:
    case Token::YIELD:
    case Token::FUTURE_STRICT_RESERVED_WORD: {
      // Using eval or arguments in this context is OK even in strict mode.
      IdentifierT name = ParseAndClassifyIdentifier(classifier, CHECK_OK);
      result = this->ExpressionFromIdentifier(name, beg_pos, end_pos, scope_,
                                              factory());
      break;
    }

    case Token::STRING: {
      classifier->RecordBindingPatternError(scanner()->location(),
                                            "unexpected_token_string");
      Consume(Token::STRING);
      result = this->ExpressionFromString(beg_pos, scanner(), factory());
      break;
    }

    case Token::ASSIGN_DIV:
      result = this->ParseRegExpLiteral(true, classifier, CHECK_OK);
      break;

    case Token::DIV:
      result = this->ParseRegExpLiteral(false, classifier, CHECK_OK);
      break;

    case Token::LBRACK:
      result = this->ParseArrayLiteral(classifier, CHECK_OK);
      break;

    case Token::LBRACE:
      result = this->ParseObjectLiteral(classifier, CHECK_OK);
      break;

    case Token::LPAREN:
      BindingPatternUnexpectedToken(classifier);
      Consume(Token::LPAREN);
      if (allow_harmony_arrow_functions() && Check(Token::RPAREN)) {
        // As a primary expression, the only thing that can follow "()" is "=>".
        Scope* scope = this->NewScope(scope_, ARROW_SCOPE);
        scope->set_start_position(beg_pos);
        FormalParameterErrorLocations error_locs;
        bool has_rest = false;
        result = this->ParseArrowFunctionLiteral(scope, error_locs, has_rest,
                                                 classifier, CHECK_OK);
      } else {
        // Heuristically try to detect immediately called functions before
        // seeing the call parentheses.
        parenthesized_function_ = (peek() == Token::FUNCTION);
        result = this->ParseExpression(true, classifier, CHECK_OK);
        result->increase_parenthesization_level();
        Expect(Token::RPAREN, CHECK_OK);
      }
      break;

    case Token::CLASS: {
      BindingPatternUnexpectedToken(classifier);
      Consume(Token::CLASS);
      if (!allow_harmony_sloppy() && is_sloppy(language_mode())) {
        ReportMessage("sloppy_lexical");
        *ok = false;
        break;
      }
      int class_token_position = position();
      IdentifierT name = this->EmptyIdentifier();
      bool is_strict_reserved_name = false;
      Scanner::Location class_name_location = Scanner::Location::invalid();
      if (peek_any_identifier()) {
        name = ParseIdentifierOrStrictReservedWord(&is_strict_reserved_name,
                                                   CHECK_OK);
        class_name_location = scanner()->location();
      }
      result = this->ParseClassLiteral(name, class_name_location,
                                       is_strict_reserved_name,
                                       class_token_position, CHECK_OK);
      break;
    }

    case Token::TEMPLATE_SPAN:
    case Token::TEMPLATE_TAIL:
      result = this->ParseTemplateLiteral(Traits::NoTemplateTag(), beg_pos,
                                          classifier, CHECK_OK);
      break;

    case Token::MOD:
      if (allow_natives() || extension_ != NULL) {
        result = this->ParseV8Intrinsic(CHECK_OK);
        break;
      }
      // If we're not allowing special syntax we fall-through to the
      // default case.

    default: {
      Next();
      ReportUnexpectedToken(token);
      *ok = false;
    }
  }

  return result;
}


template <class Traits>
typename ParserBase<Traits>::ExpressionT ParserBase<Traits>::ParseExpression(
    bool accept_IN, bool* ok) {
  ExpressionClassifier classifier;
  ExpressionT result = ParseExpression(accept_IN, &classifier, CHECK_OK);
  ValidateExpression(&classifier, CHECK_OK);
  return result;
}


// Precedence = 1
template <class Traits>
typename ParserBase<Traits>::ExpressionT ParserBase<Traits>::ParseExpression(
    bool accept_IN, ExpressionClassifier* classifier, bool* ok) {
  // Expression ::
  //   AssignmentExpression
  //   Expression ',' AssignmentExpression

  ExpressionT result =
      this->ParseAssignmentExpression(accept_IN, classifier, CHECK_OK);
  while (peek() == Token::COMMA) {
    Expect(Token::COMMA, CHECK_OK);
    int pos = position();
    ExpressionT right =
        this->ParseAssignmentExpression(accept_IN, classifier, CHECK_OK);
    result = factory()->NewBinaryOperation(Token::COMMA, result, right, pos);
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
  Expect(Token::LBRACK, CHECK_OK);
  while (peek() != Token::RBRACK) {
    ExpressionT elem = this->EmptyExpression();
    if (peek() == Token::COMMA) {
      if (is_strong(language_mode())) {
        ReportMessageAt(scanner()->peek_location(), "strong_ellision");
        *ok = false;
        return this->EmptyExpression();
      }
      elem = this->GetLiteralTheHole(peek_position(), factory());
    } else {
      elem = this->ParseAssignmentExpression(true, classifier, CHECK_OK);
    }
    values->Add(elem, zone_);
    if (peek() != Token::RBRACK) {
      Expect(Token::COMMA, CHECK_OK);
    }
  }
  Expect(Token::RBRACK, CHECK_OK);

  // Update the scope information before the pre-parsing bailout.
  int literal_index = function_state_->NextMaterializedLiteralIndex();

  return factory()->NewArrayLiteral(values, literal_index, pos);
}


template <class Traits>
typename ParserBase<Traits>::ExpressionT ParserBase<Traits>::ParsePropertyName(
    IdentifierT* name, bool* is_get, bool* is_set, bool* is_static,
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

    case Token::LBRACK:
      if (allow_harmony_computed_property_names_) {
        *is_computed_name = true;
        Consume(Token::LBRACK);
        ExpressionT expression =
            ParseAssignmentExpression(true, classifier, CHECK_OK);
        Expect(Token::RBRACK, CHECK_OK);
        return expression;
      }

    // Fall through.
    case Token::STATIC:
      *is_static = true;

    // Fall through.
    default:
      *name = ParseIdentifierNameOrGetOrSet(is_get, is_set, CHECK_OK);
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
    bool is_static, bool* is_computed_name, bool* has_seen_constructor,
    ExpressionClassifier* classifier, bool* ok) {
  DCHECK(!in_class || is_static || has_seen_constructor != nullptr);
  ExpressionT value = this->EmptyExpression();
  IdentifierT name = this->EmptyIdentifier();
  bool is_get = false;
  bool is_set = false;
  bool name_is_static = false;
  bool is_generator = allow_harmony_object_literals_ && Check(Token::MUL);

  Token::Value name_token = peek();
  int next_beg_pos = scanner()->peek_location().beg_pos;
  int next_end_pos = scanner()->peek_location().end_pos;
  ExpressionT name_expression = ParsePropertyName(
      &name, &is_get, &is_set, &name_is_static, is_computed_name, classifier,
      CHECK_OK_CUSTOM(EmptyObjectLiteralProperty));

  if (fni_ != nullptr && !*is_computed_name) {
    this->PushLiteralName(fni_, name);
  }

  if (!in_class && !is_generator && peek() == Token::COLON) {
    // PropertyDefinition : PropertyName ':' AssignmentExpression
    if (!*is_computed_name) {
      checker->CheckProperty(name_token, kValueProperty, is_static,
                             is_generator,
                             CHECK_OK_CUSTOM(EmptyObjectLiteralProperty));
    }
    Consume(Token::COLON);
    value = this->ParseAssignmentExpression(
        true, classifier, CHECK_OK_CUSTOM(EmptyObjectLiteralProperty));

  } else if (is_generator ||
             (allow_harmony_object_literals_ && peek() == Token::LPAREN)) {
    // Concise Method
    if (!*is_computed_name) {
      checker->CheckProperty(name_token, kMethodProperty, is_static,
                             is_generator,
                             CHECK_OK_CUSTOM(EmptyObjectLiteralProperty));
    }

    FunctionKind kind = is_generator ? FunctionKind::kConciseGeneratorMethod
                                     : FunctionKind::kConciseMethod;

    if (in_class && !is_static && this->IsConstructor(name)) {
      *has_seen_constructor = true;
      kind = has_extends ? FunctionKind::kSubclassConstructor
                         : FunctionKind::kBaseConstructor;
    }

    if (!in_class) kind = WithObjectLiteralBit(kind);

    value = this->ParseFunctionLiteral(
        name, scanner()->location(),
        false,  // reserved words are allowed here
        kind, RelocInfo::kNoPosition, FunctionLiteral::ANONYMOUS_EXPRESSION,
        FunctionLiteral::NORMAL_ARITY,
        CHECK_OK_CUSTOM(EmptyObjectLiteralProperty));

    return factory()->NewObjectLiteralProperty(name_expression, value,
                                               ObjectLiteralProperty::COMPUTED,
                                               is_static, *is_computed_name);

  } else if (in_class && name_is_static && !is_static) {
    // static MethodDefinition
    return ParsePropertyDefinition(checker, true, has_extends, true,
                                   is_computed_name, nullptr, classifier, ok);
  } else if (is_get || is_set) {
    // Accessor
    name = this->EmptyIdentifier();
    bool dont_care = false;
    name_token = peek();

    name_expression = ParsePropertyName(
        &name, &dont_care, &dont_care, &dont_care, is_computed_name, classifier,
        CHECK_OK_CUSTOM(EmptyObjectLiteralProperty));

    if (!*is_computed_name) {
      checker->CheckProperty(name_token, kAccessorProperty, is_static,
                             is_generator,
                             CHECK_OK_CUSTOM(EmptyObjectLiteralProperty));
    }

    FunctionKind kind = FunctionKind::kAccessorFunction;
    if (!in_class) kind = WithObjectLiteralBit(kind);
    typename Traits::Type::FunctionLiteral value = this->ParseFunctionLiteral(
        name, scanner()->location(),
        false,  // reserved words are allowed here
        kind, RelocInfo::kNoPosition, FunctionLiteral::ANONYMOUS_EXPRESSION,
        is_get ? FunctionLiteral::GETTER_ARITY : FunctionLiteral::SETTER_ARITY,
        CHECK_OK_CUSTOM(EmptyObjectLiteralProperty));

    // Make sure the name expression is a string since we need a Name for
    // Runtime_DefineAccessorPropertyUnchecked and since we can determine this
    // statically we can skip the extra runtime check.
    if (!*is_computed_name) {
      name_expression =
          factory()->NewStringLiteral(name, name_expression->position());
    }

    return factory()->NewObjectLiteralProperty(
        name_expression, value,
        is_get ? ObjectLiteralProperty::GETTER : ObjectLiteralProperty::SETTER,
        is_static, *is_computed_name);

  } else if (!in_class && allow_harmony_object_literals_ &&
             Token::IsIdentifier(name_token, language_mode(),
                                 this->is_generator())) {
    DCHECK(!*is_computed_name);
    DCHECK(!is_static);
    value = this->ExpressionFromIdentifier(name, next_beg_pos, next_end_pos,
                                           scope_, factory());
    return factory()->NewObjectLiteralProperty(
        name_expression, value, ObjectLiteralProperty::COMPUTED, false, false);

  } else {
    Token::Value next = Next();
    ReportUnexpectedToken(next);
    *ok = false;
    return this->EmptyObjectLiteralProperty();
  }

  return factory()->NewObjectLiteralProperty(name_expression, value, is_static,
                                             *is_computed_name);
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
  bool has_function = false;
  bool has_computed_names = false;
  ObjectLiteralChecker checker(this);

  Expect(Token::LBRACE, CHECK_OK);

  while (peek() != Token::RBRACE) {
    if (fni_ != nullptr) fni_->Enter();

    const bool in_class = false;
    const bool is_static = false;
    const bool has_extends = false;
    bool is_computed_name = false;
    ObjectLiteralPropertyT property = this->ParsePropertyDefinition(
        &checker, in_class, has_extends, is_static, &is_computed_name, NULL,
        classifier, CHECK_OK);

    if (is_computed_name) {
      has_computed_names = true;
    }

    // Mark top-level object literals that contain function literals and
    // pretenure the literal so it can be added as a constant function
    // property. (Parser only.)
    this->CheckFunctionLiteralInsideTopLevelObjectLiteral(scope_, property,
                                                          &has_function);

    // Count CONSTANT or COMPUTED properties to maintain the enumeration order.
    if (!has_computed_names && this->IsBoilerplateProperty(property)) {
      number_of_boilerplate_properties++;
    }
    properties->Add(property, zone());

    if (peek() != Token::RBRACE) {
      // Need {} because of the CHECK_OK macro.
      Expect(Token::COMMA, CHECK_OK);
    }

    if (fni_ != nullptr) {
      fni_->Infer();
      fni_->Leave();
    }
  }
  Expect(Token::RBRACE, CHECK_OK);

  // Computation of literal_index must happen before pre parse bailout.
  int literal_index = function_state_->NextMaterializedLiteralIndex();

  return factory()->NewObjectLiteral(properties,
                                     literal_index,
                                     number_of_boilerplate_properties,
                                     has_function,
                                     pos);
}


template <class Traits>
typename Traits::Type::ExpressionList ParserBase<Traits>::ParseArguments(
    Scanner::Location* first_spread_arg_loc, ExpressionClassifier* classifier,
    bool* ok) {
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
    bool is_spread = allow_harmony_spreadcalls() && (peek() == Token::ELLIPSIS);
    int start_pos = peek_position();
    if (is_spread) Consume(Token::ELLIPSIS);

    ExpressionT argument = this->ParseAssignmentExpression(
        true, classifier, CHECK_OK_CUSTOM(NullExpressionList));
    if (is_spread) {
      if (!spread_arg.IsValid()) {
        spread_arg.beg_pos = start_pos;
        spread_arg.end_pos = peek_position();
      }
      argument = factory()->NewSpread(argument, start_pos);
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
      ReportMessage("too_many_arguments");
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
    ReportMessageAt(location, "unterminated_arg_list");
    *ok = false;
    return this->NullExpressionList();
  }
  *first_spread_arg_loc = spread_arg;

  if (spread_arg.IsValid()) {
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

  Scanner::Location lhs_location = scanner()->peek_location();

  if (peek() == Token::YIELD && is_generator()) {
    return this->ParseYieldExpression(classifier, ok);
  }

  if (fni_ != NULL) fni_->Enter();
  ParserBase<Traits>::Checkpoint checkpoint(this);
  ExpressionT expression =
      this->ParseConditionalExpression(accept_IN, classifier, CHECK_OK);

  if (allow_harmony_arrow_functions() && peek() == Token::ARROW) {
    checkpoint.Restore();
    FormalParameterErrorLocations error_locs;
    Scanner::Location loc(lhs_location.beg_pos, scanner()->location().end_pos);
    bool has_rest = false;
    Scope* scope = this->NewScope(scope_, ARROW_SCOPE);
    scope->set_start_position(lhs_location.beg_pos);
    this->ParseArrowFunctionFormalParameters(scope, expression, loc,
                                             &error_locs, &has_rest, CHECK_OK);
    expression = this->ParseArrowFunctionLiteral(scope, error_locs, has_rest,
                                                 classifier, CHECK_OK);
    return expression;
  }

  if (!Token::IsAssignmentOp(peek())) {
    if (fni_ != NULL) fni_->Leave();
    // Parsed conditional expression only (no assignment).
    return expression;
  }

  expression = this->CheckAndRewriteReferenceExpression(
      expression, lhs_location, "invalid_lhs_in_assignment", CHECK_OK);
  expression = this->MarkExpressionAsAssigned(expression);

  Token::Value op = Next();  // Get assignment operator.
  int pos = position();
  ExpressionT right =
      this->ParseAssignmentExpression(accept_IN, classifier, CHECK_OK);

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
    if ((op == Token::INIT_VAR
         || op == Token::INIT_CONST_LEGACY
         || op == Token::ASSIGN)
        && (!right->IsCall() && !right->IsCallNew())) {
      fni_->Infer();
    } else {
      fni_->RemoveLastFunction();
    }
    fni_->Leave();
  }

  return factory()->NewAssignment(op, expression, right, pos);
}

template <class Traits>
typename ParserBase<Traits>::ExpressionT
ParserBase<Traits>::ParseYieldExpression(ExpressionClassifier* classifier,
                                         bool* ok) {
  // YieldExpression ::
  //   'yield' ([no line terminator] '*'? AssignmentExpression)?
  int pos = peek_position();
  Expect(Token::YIELD, CHECK_OK);
  ExpressionT generator_object =
      factory()->NewVariableProxy(function_state_->generator_object_variable());
  ExpressionT expression = Traits::EmptyExpression();
  Yield::Kind kind = Yield::kSuspend;
  if (!scanner()->HasAnyLineTerminatorBeforeNext()) {
    if (Check(Token::MUL)) kind = Yield::kDelegating;
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
        // a Yield::kSuspend operation, given only one look-ahead token.
        if (kind == Yield::kSuspend)
          break;
        DCHECK_EQ(Yield::kDelegating, kind);
        // Delegating yields require an RHS; fall through.
      default:
        expression = ParseAssignmentExpression(false, classifier, CHECK_OK);
        break;
    }
  }
  if (kind == Yield::kDelegating) {
    // var iterator = subject[Symbol.iterator]();
    expression = this->GetIterator(expression, factory());
  }
  typename Traits::Type::YieldExpression yield =
      factory()->NewYield(generator_object, expression, kind, pos);
  if (kind == Yield::kDelegating) {
    yield->set_index(function_state_->NextHandlerIndex());
  }
  return yield;
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
  Consume(Token::CONDITIONAL);
  // In parsing the first assignment expression in conditional
  // expressions we always accept the 'in' keyword; see ECMA-262,
  // section 11.12, page 58.
  ExpressionT left = ParseAssignmentExpression(true, classifier, CHECK_OK);
  Expect(Token::COLON, CHECK_OK);
  ExpressionT right =
      ParseAssignmentExpression(accept_IN, classifier, CHECK_OK);
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
      Token::Value op = Next();
      Scanner::Location op_location = scanner()->location();
      int pos = position();
      ExpressionT y =
          ParseBinaryExpression(prec1 + 1, accept_IN, classifier, CHECK_OK);

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
        if (cmp == Token::EQ && is_strong(language_mode())) {
          ReportMessageAt(op_location, "strong_equal");
          *ok = false;
          return this->EmptyExpression();
        }
        x = factory()->NewCompareOperation(cmp, x, y, pos);
        if (cmp != op) {
          // The comparison was negated - add a NOT.
          x = factory()->NewUnaryOperation(Token::NOT, x, pos);
        }

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

  Token::Value op = peek();
  if (Token::IsUnaryOp(op)) {
    BindingPatternUnexpectedToken(classifier);

    op = Next();
    int pos = position();
    ExpressionT expression = ParseUnaryExpression(classifier, CHECK_OK);

    if (op == Token::DELETE && is_strict(language_mode())) {
      if (is_strong(language_mode())) {
        ReportMessage("strong_delete");
        *ok = false;
        return this->EmptyExpression();
      } else if (this->IsIdentifier(expression)) {
        // "delete identifier" is a syntax error in strict mode.
        ReportMessage("strict_delete");
        *ok = false;
        return this->EmptyExpression();
      }
    }

    // Allow Traits do rewrite the expression.
    return this->BuildUnaryExpression(expression, op, pos, factory());
  } else if (Token::IsCountOp(op)) {
    BindingPatternUnexpectedToken(classifier);
    op = Next();
    Scanner::Location lhs_location = scanner()->peek_location();
    ExpressionT expression = this->ParseUnaryExpression(classifier, CHECK_OK);
    expression = this->CheckAndRewriteReferenceExpression(
        expression, lhs_location, "invalid_lhs_in_prefix_op", CHECK_OK);
    this->MarkExpressionAsAssigned(expression);

    return factory()->NewCountOperation(op,
                                        true /* prefix */,
                                        expression,
                                        position());

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

  Scanner::Location lhs_location = scanner()->peek_location();
  ExpressionT expression =
      this->ParseLeftHandSideExpression(classifier, CHECK_OK);
  if (!scanner()->HasAnyLineTerminatorBeforeNext() &&
      Token::IsCountOp(peek())) {
    BindingPatternUnexpectedToken(classifier);

    expression = this->CheckAndRewriteReferenceExpression(
        expression, lhs_location, "invalid_lhs_in_postfix_op", CHECK_OK);
    expression = this->MarkExpressionAsAssigned(expression);

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

  ExpressionT result =
      this->ParseMemberWithNewPrefixesExpression(classifier, CHECK_OK);

  while (true) {
    switch (peek()) {
      case Token::LBRACK: {
        BindingPatternUnexpectedToken(classifier);
        Consume(Token::LBRACK);
        int pos = position();
        ExpressionT index = ParseExpression(true, classifier, CHECK_OK);
        result = factory()->NewProperty(result, index, pos);
        Expect(Token::RBRACK, CHECK_OK);
        break;
      }

      case Token::LPAREN: {
        BindingPatternUnexpectedToken(classifier);

        if (is_strong(language_mode()) && this->IsIdentifier(result) &&
            this->IsEval(this->AsIdentifier(result))) {
          ReportMessage("strong_direct_eval");
          *ok = false;
          return this->EmptyExpression();
        }
        int pos;
        if (scanner()->current_token() == Token::IDENTIFIER) {
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
            ParseArguments(&spread_pos, classifier, CHECK_OK);

        // Keep track of eval() calls since they disable all local variable
        // optimizations.
        // The calls that need special treatment are the
        // direct eval calls. These calls are all of the form eval(...), with
        // no explicit receiver.
        // These calls are marked as potentially direct eval calls. Whether
        // they are actually direct calls to eval is determined at run time.
        this->CheckPossibleEvalCall(result, scope_);

        if (spread_pos.IsValid()) {
          args = Traits::PrepareSpreadArguments(args);
          result = Traits::SpreadCall(result, args, pos);
        } else {
          result = factory()->NewCall(result, args, pos);
        }
        if (fni_ != NULL) fni_->RemoveLastFunction();
        break;
      }

      case Token::PERIOD: {
        BindingPatternUnexpectedToken(classifier);
        Consume(Token::PERIOD);
        int pos = position();
        IdentifierT name = ParseIdentifierName(CHECK_OK);
        result = factory()->NewProperty(
            result, factory()->NewStringLiteral(name, pos), pos);
        if (fni_ != NULL) this->PushLiteralName(fni_, name);
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
    ExpressionClassifier* classifier, bool* ok) {
  // NewExpression ::
  //   ('new')+ MemberExpression

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
    Consume(Token::NEW);
    int new_pos = position();
    ExpressionT result = this->EmptyExpression();
    if (peek() == Token::SUPER) {
      const bool is_new = true;
      result = ParseSuperExpression(is_new, classifier, CHECK_OK);
    } else {
      result = this->ParseMemberWithNewPrefixesExpression(classifier, CHECK_OK);
    }
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
      result =
          this->ParseMemberExpressionContinuation(result, classifier, CHECK_OK);
      return result;
    }
    // NewExpression without arguments.
    return factory()->NewCallNew(result, this->NewExpressionList(0, zone_),
                                 new_pos);
  }
  // No 'new' or 'super' keyword.
  return this->ParseMemberExpression(classifier, ok);
}


template <class Traits>
typename ParserBase<Traits>::ExpressionT
ParserBase<Traits>::ParseMemberExpression(ExpressionClassifier* classifier,
                                          bool* ok) {
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

    Consume(Token::FUNCTION);
    int function_token_position = position();
    bool is_generator = Check(Token::MUL);
    IdentifierT name = this->EmptyIdentifier();
    bool is_strict_reserved_name = false;
    Scanner::Location function_name_location = Scanner::Location::invalid();
    FunctionLiteral::FunctionType function_type =
        FunctionLiteral::ANONYMOUS_EXPRESSION;
    if (peek_any_identifier()) {
      name = ParseIdentifierOrStrictReservedWord(&is_strict_reserved_name,
                                                 CHECK_OK);
      function_name_location = scanner()->location();
      function_type = FunctionLiteral::NAMED_EXPRESSION;
    }
    result = this->ParseFunctionLiteral(
        name, function_name_location, is_strict_reserved_name,
        is_generator ? FunctionKind::kGeneratorFunction
                     : FunctionKind::kNormalFunction,
        function_token_position, function_type, FunctionLiteral::NORMAL_ARITY,
        CHECK_OK);
  } else if (peek() == Token::SUPER) {
    const bool is_new = false;
    result = ParseSuperExpression(is_new, classifier, CHECK_OK);
  } else {
    result = ParsePrimaryExpression(classifier, CHECK_OK);
  }

  result = ParseMemberExpressionContinuation(result, classifier, CHECK_OK);
  return result;
}


template <class Traits>
typename ParserBase<Traits>::ExpressionT
ParserBase<Traits>::ParseStrongInitializationExpression(
    ExpressionClassifier* classifier, bool* ok) {
  // InitializationExpression ::  (strong mode)
  //  'this' '.' IdentifierName '=' AssignmentExpression
  //  'this' '[' Expression ']' '=' AssignmentExpression

  if (fni_ != NULL) fni_->Enter();

  Consume(Token::THIS);
  int pos = position();
  function_state_->set_this_location(scanner()->location());
  scope_->RecordThisUsage();
  ExpressionT this_expr = this->ThisExpression(scope_, factory(), pos);

  ExpressionT left = this->EmptyExpression();
  switch (peek()) {
    case Token::LBRACK: {
      Consume(Token::LBRACK);
      int pos = position();
      ExpressionT index = this->ParseExpression(true, classifier, CHECK_OK);
      left = factory()->NewProperty(this_expr, index, pos);
      if (fni_ != NULL) {
        this->PushPropertyName(fni_, index);
      }
      Expect(Token::RBRACK, CHECK_OK);
      break;
    }
    case Token::PERIOD: {
      Consume(Token::PERIOD);
      int pos = position();
      IdentifierT name = ParseIdentifierName(CHECK_OK);
      left = factory()->NewProperty(
          this_expr, factory()->NewStringLiteral(name, pos), pos);
      if (fni_ != NULL) {
        this->PushLiteralName(fni_, name);
      }
      break;
    }
    default:
      ReportMessage("strong_constructor_this");
      *ok = false;
      return this->EmptyExpression();
  }

  if (peek() != Token::ASSIGN) {
    ReportMessageAt(function_state_->this_location(),
                    "strong_constructor_this");
    *ok = false;
    return this->EmptyExpression();
  }
  Consume(Token::ASSIGN);
  left = this->MarkExpressionAsAssigned(left);

  ExpressionT right =
      this->ParseAssignmentExpression(true, classifier, CHECK_OK);
  this->CheckAssigningFunctionLiteralToProperty(left, right);
  function_state_->AddProperty();
  if (fni_ != NULL) {
    // Check if the right hand side is a call to avoid inferring a
    // name if we're dealing with "this.a = function(){...}();"-like
    // expression.
    if (!right->IsCall() && !right->IsCallNew()) {
      fni_->Infer();
    } else {
      fni_->RemoveLastFunction();
    }
    fni_->Leave();
  }

  if (function_state_->return_location().IsValid()) {
    ReportMessageAt(function_state_->return_location(),
                    "strong_constructor_return_misplaced");
    *ok = false;
    return this->EmptyExpression();
  }

  return factory()->NewAssignment(Token::ASSIGN, left, right, pos);
}


template <class Traits>
typename ParserBase<Traits>::ExpressionT
ParserBase<Traits>::ParseStrongSuperCallExpression(
    ExpressionClassifier* classifier, bool* ok) {
  // SuperCallExpression ::  (strong mode)
  //  'super' '(' ExpressionList ')'
  BindingPatternUnexpectedToken(classifier);

  Consume(Token::SUPER);
  int pos = position();
  Scanner::Location super_loc = scanner()->location();
  ExpressionT expr = this->SuperReference(scope_, factory());

  if (peek() != Token::LPAREN) {
    ReportMessage("strong_constructor_super");
    *ok = false;
    return this->EmptyExpression();
  }

  Scanner::Location spread_pos;
  typename Traits::Type::ExpressionList args =
      ParseArguments(&spread_pos, classifier, CHECK_OK);

  // TODO(rossberg): This doesn't work with arrow functions yet.
  if (!IsSubclassConstructor(function_state_->kind())) {
    ReportMessage("unexpected_super");
    *ok = false;
    return this->EmptyExpression();
  } else if (function_state_->super_location().IsValid()) {
    ReportMessageAt(scanner()->location(), "strong_super_call_duplicate");
    *ok = false;
    return this->EmptyExpression();
  } else if (function_state_->this_location().IsValid()) {
    ReportMessageAt(scanner()->location(), "strong_super_call_misplaced");
    *ok = false;
    return this->EmptyExpression();
  } else if (function_state_->return_location().IsValid()) {
    ReportMessageAt(function_state_->return_location(),
                    "strong_constructor_return_misplaced");
    *ok = false;
    return this->EmptyExpression();
  }

  function_state_->set_super_location(super_loc);
  if (spread_pos.IsValid()) {
    args = Traits::PrepareSpreadArguments(args);
    return Traits::SpreadCall(expr, args, pos);
  } else {
    return factory()->NewCall(expr, args, pos);
  }
}


template <class Traits>
typename ParserBase<Traits>::ExpressionT
ParserBase<Traits>::ParseSuperExpression(bool is_new,
                                         ExpressionClassifier* classifier,
                                         bool* ok) {
  Expect(Token::SUPER, CHECK_OK);

  // TODO(wingo): Does this actually work with lazily compiled arrows?
  FunctionState* function_state = function_state_;
  while (IsArrowFunction(function_state->kind())) {
    function_state = function_state->outer();
  }
  // TODO(arv): Handle eval scopes similarly.

  FunctionKind kind = function_state->kind();
  if (IsConciseMethod(kind) || IsAccessorFunction(kind) ||
      i::IsConstructor(kind)) {
    if (peek() == Token::PERIOD || peek() == Token::LBRACK) {
      scope_->RecordSuperPropertyUsage();
      return this->SuperReference(scope_, factory());
    }
    // new super() is never allowed.
    // super() is only allowed in derived constructor
    if (!is_new && peek() == Token::LPAREN && IsSubclassConstructor(kind)) {
      if (is_strong(language_mode())) {
        // Super calls in strong mode are parsed separately.
        ReportMessageAt(scanner()->location(), "strong_constructor_super");
        *ok = false;
        return this->EmptyExpression();
      }
      function_state->set_super_location(scanner()->location());
      return this->SuperReference(scope_, factory());
    }
  }

  ReportMessageAt(scanner()->location(), "unexpected_super");
  *ok = false;
  return this->EmptyExpression();
}


template <class Traits>
typename ParserBase<Traits>::ExpressionT
ParserBase<Traits>::ParseMemberExpressionContinuation(
    ExpressionT expression, ExpressionClassifier* classifier, bool* ok) {
  // Parses this part of MemberExpression:
  // ('[' Expression ']' | '.' Identifier | TemplateLiteral)*
  while (true) {
    switch (peek()) {
      case Token::LBRACK: {
        BindingPatternUnexpectedToken(classifier);

        Consume(Token::LBRACK);
        int pos = position();
        ExpressionT index = this->ParseExpression(true, classifier, CHECK_OK);
        expression = factory()->NewProperty(expression, index, pos);
        if (fni_ != NULL) {
          this->PushPropertyName(fni_, index);
        }
        Expect(Token::RBRACK, CHECK_OK);
        break;
      }
      case Token::PERIOD: {
        BindingPatternUnexpectedToken(classifier);

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
        BindingPatternUnexpectedToken(classifier);
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
      default:
        return expression;
    }
  }
  DCHECK(false);
  return this->EmptyExpression();
}


template <class Traits>
void ParserBase<Traits>::ParseFormalParameter(
    FormalParameterScopeT* scope, FormalParameterErrorLocations* locs,
    bool is_rest, bool* ok) {
  // FormalParameter[Yield,GeneratorParameter] :
  //   BindingElement[?Yield, ?GeneratorParameter]
  bool is_strict_reserved;
  IdentifierT name =
      ParseIdentifierOrStrictReservedWord(&is_strict_reserved, ok);
  if (!*ok) return;

  // Store locations for possible future error reports.
  if (!locs->eval_or_arguments.IsValid() && this->IsEvalOrArguments(name)) {
    locs->eval_or_arguments = scanner()->location();
  }
  if (!locs->undefined.IsValid() && this->IsUndefined(name)) {
    locs->undefined = scanner()->location();
  }
  if (!locs->reserved.IsValid() && is_strict_reserved) {
    locs->reserved = scanner()->location();
  }
  bool was_declared = Traits::DeclareFormalParameter(scope, name, is_rest);
  if (!locs->duplicate.IsValid() && was_declared) {
    locs->duplicate = scanner()->location();
  }
}


template <class Traits>
int ParserBase<Traits>::ParseFormalParameterList(
    FormalParameterScopeT* scope, FormalParameterErrorLocations* locs,
    bool* is_rest, bool* ok) {
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

  int parameter_count = 0;

  if (peek() != Token::RPAREN) {
    do {
      if (++parameter_count > Code::kMaxArguments) {
        ReportMessage("too_many_parameters");
        *ok = false;
        return -1;
      }
      *is_rest = allow_harmony_rest_params() && Check(Token::ELLIPSIS);
      ParseFormalParameter(scope, locs, *is_rest, ok);
      if (!*ok) return -1;
    } while (!*is_rest && Check(Token::COMMA));

    if (*is_rest && peek() == Token::COMMA) {
      ReportMessageAt(scanner()->peek_location(), "param_after_rest");
      *ok = false;
      return -1;
    }
  }

  return parameter_count;
}


template <class Traits>
void ParserBase<Traits>::CheckArityRestrictions(
    int param_count, FunctionLiteral::ArityRestriction arity_restriction,
    int formals_start_pos, int formals_end_pos, bool* ok) {
  switch (arity_restriction) {
    case FunctionLiteral::GETTER_ARITY:
      if (param_count != 0) {
        ReportMessageAt(Scanner::Location(formals_start_pos, formals_end_pos),
                        "bad_getter_arity");
        *ok = false;
      }
      break;
    case FunctionLiteral::SETTER_ARITY:
      if (param_count != 1) {
        ReportMessageAt(Scanner::Location(formals_start_pos, formals_end_pos),
                        "bad_setter_arity");
        *ok = false;
      }
      break;
    default:
      break;
  }
}


template <class Traits>
typename ParserBase<Traits>::ExpressionT
ParserBase<Traits>::ParseArrowFunctionLiteral(
    Scope* scope, const FormalParameterErrorLocations& error_locs,
    bool has_rest, ExpressionClassifier* classifier, bool* ok) {
  if (peek() == Token::ARROW && scanner_->HasAnyLineTerminatorBeforeNext()) {
    // ASI inserts `;` after arrow parameters if a line terminator is found.
    // `=> ...` is never a valid expression, so report as syntax error.
    // If next token is not `=>`, it's a syntax error anyways.
    ReportUnexpectedTokenAt(scanner_->peek_location(), Token::ARROW);
    *ok = false;
    return this->EmptyExpression();
  }

  typename Traits::Type::StatementList body;
  int num_parameters = scope->num_parameters();
  int materialized_literal_count = -1;
  int expected_property_count = -1;
  int handler_count = 0;
  Scanner::Location super_loc;

  {
    typename Traits::Type::Factory function_factory(ast_value_factory());
    FunctionState function_state(&function_state_, &scope_, scope,
                                 kArrowFunction, &function_factory);

    if (peek() == Token::ARROW) {
      BindingPatternUnexpectedToken(classifier);
    }
    Expect(Token::ARROW, CHECK_OK);

    if (peek() == Token::LBRACE) {
      // Multiple statement body
      Consume(Token::LBRACE);
      bool is_lazily_parsed =
          (mode() == PARSE_LAZILY && scope_->AllowsLazyCompilation());
      if (is_lazily_parsed) {
        body = this->NewStatementList(0, zone());
        this->SkipLazyFunctionBody(&materialized_literal_count,
                                   &expected_property_count, CHECK_OK);
      } else {
        body = this->ParseEagerFunctionBody(
            this->EmptyIdentifier(), RelocInfo::kNoPosition, NULL,
            Token::INIT_VAR, kArrowFunction, CHECK_OK);
        materialized_literal_count =
            function_state.materialized_literal_count();
        expected_property_count = function_state.expected_property_count();
        handler_count = function_state.handler_count();
      }
    } else {
      // Single-expression body
      int pos = position();
      parenthesized_function_ = false;
      ExpressionT expression =
          ParseAssignmentExpression(true, classifier, CHECK_OK);
      body = this->NewStatementList(1, zone());
      body->Add(factory()->NewReturnStatement(expression, pos), zone());
      materialized_literal_count = function_state.materialized_literal_count();
      expected_property_count = function_state.expected_property_count();
      handler_count = function_state.handler_count();
    }
    super_loc = function_state.super_location();

    scope->set_end_position(scanner()->location().end_pos);

    // Arrow function formal parameters are parsed as StrictFormalParameterList,
    // which is not the same as "parameters of a strict function"; it only means
    // that duplicates are not allowed.  Of course, the arrow function may
    // itself be strict as well.
    const bool use_strict_params = true;
    this->CheckFunctionParameterNames(language_mode(), use_strict_params,
                                      error_locs, CHECK_OK);

    // Validate strict mode.
    if (is_strict(language_mode())) {
      CheckStrictOctalLiteral(scope->start_position(),
                              scanner()->location().end_pos, CHECK_OK);
      this->CheckConflictingVarDeclarations(scope, CHECK_OK);
    }
  }

  FunctionLiteralT function_literal = factory()->NewFunctionLiteral(
      this->EmptyIdentifierString(), ast_value_factory(), scope, body,
      materialized_literal_count, expected_property_count, handler_count,
      num_parameters, FunctionLiteral::kNoDuplicateParameters,
      FunctionLiteral::ANONYMOUS_EXPRESSION, FunctionLiteral::kIsFunction,
      FunctionLiteral::kShouldLazyCompile, FunctionKind::kArrowFunction,
      scope->start_position());

  function_literal->set_function_token_position(scope->start_position());
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
                      "unterminated_template");
      *ok = false;
      return Traits::EmptyExpression();
    } else if (next == Token::ILLEGAL) {
      Traits::ReportMessageAt(
          Scanner::Location(position() + 1, peek_position()),
          "unexpected_token", "ILLEGAL", kSyntaxError);
      *ok = false;
      return Traits::EmptyExpression();
    }

    int expr_pos = peek_position();
    ExpressionT expression = this->ParseExpression(true, classifier, CHECK_OK);
    Traits::AddTemplateExpression(&ts, expression);

    if (peek() != Token::RBRACE) {
      ReportMessageAt(Scanner::Location(expr_pos, peek_position()),
                      "unterminated_template_expr");
      *ok = false;
      return Traits::EmptyExpression();
    }

    // If we didn't die parsing that expression, our next token should be a
    // TEMPLATE_SPAN or TEMPLATE_TAIL.
    next = scanner()->ScanTemplateContinuation();
    Next();
    pos = position();

    if (next == Token::EOS) {
      ReportMessageAt(Scanner::Location(start, pos), "unterminated_template");
      *ok = false;
      return Traits::EmptyExpression();
    } else if (next == Token::ILLEGAL) {
      Traits::ReportMessageAt(
          Scanner::Location(position() + 1, peek_position()),
          "unexpected_token", "ILLEGAL", kSyntaxError);
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
typename ParserBase<Traits>::ExpressionT ParserBase<
    Traits>::CheckAndRewriteReferenceExpression(ExpressionT expression,
                                                Scanner::Location location,
                                                const char* message, bool* ok) {
  if (this->IsIdentifier(expression)) {
    if (is_strict(language_mode()) &&
        this->IsEvalOrArguments(this->AsIdentifier(expression))) {
      this->ReportMessageAt(location, "strict_eval_arguments", kSyntaxError);
      *ok = false;
      return this->EmptyExpression();
    }
    if (is_strong(language_mode()) &&
        this->IsUndefined(this->AsIdentifier(expression))) {
      this->ReportMessageAt(location, "strong_undefined", kSyntaxError);
      *ok = false;
      return this->EmptyExpression();
    }
  }
  if (expression->IsValidReferenceExpression()) {
    return expression;
  } else if (expression->IsCall()) {
    // If it is a call, make it a runtime error for legacy web compatibility.
    // Rewrite `expr' to `expr[throw ReferenceError]'.
    int pos = location.beg_pos;
    ExpressionT error = this->NewThrowReferenceError(message, pos);
    return factory()->NewProperty(expression, error, pos);
  } else {
    this->ReportMessageAt(location, message, kReferenceError);
    *ok = false;
    return this->EmptyExpression();
  }
}


#undef CHECK_OK
#undef CHECK_OK_CUSTOM


template <typename Traits>
void ParserBase<Traits>::ObjectLiteralChecker::CheckProperty(
    Token::Value property, PropertyKind type, bool is_static, bool is_generator,
    bool* ok) {
  DCHECK(!is_static);
  DCHECK(!is_generator || type == kMethodProperty);

  if (property == Token::SMI || property == Token::NUMBER) return;

  if (type == kValueProperty && IsProto()) {
    if (has_seen_proto_) {
      this->parser()->ReportMessage("duplicate_proto");
      *ok = false;
      return;
    }
    has_seen_proto_ = true;
    return;
  }
}


template <typename Traits>
void ParserBase<Traits>::ClassLiteralChecker::CheckProperty(
    Token::Value property, PropertyKind type, bool is_static, bool is_generator,
    bool* ok) {
  DCHECK(type == kMethodProperty || type == kAccessorProperty);

  if (property == Token::SMI || property == Token::NUMBER) return;

  if (is_static) {
    if (IsPrototype()) {
      this->parser()->ReportMessage("static_prototype");
      *ok = false;
      return;
    }
  } else if (IsConstructor()) {
    if (is_generator || type == kAccessorProperty) {
      const char* msg =
          is_generator ? "constructor_is_generator" : "constructor_is_accessor";
      this->parser()->ReportMessage(msg);
      *ok = false;
      return;
    }
    if (has_seen_constructor_) {
      this->parser()->ReportMessage("duplicate_constructor");
      *ok = false;
      return;
    }
    has_seen_constructor_ = true;
    return;
  }
}
} }  // v8::internal

#endif  // V8_PREPARSER_H
