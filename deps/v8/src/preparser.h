// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PREPARSER_H
#define V8_PREPARSER_H

#include "src/v8.h"

#include "src/func-name-inferrer.h"
#include "src/hashmap.h"
#include "src/scanner.h"
#include "src/scopes.h"
#include "src/token.h"

namespace v8 {
namespace internal {

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

// - Miscellanous other tasks interleaved with the recursive descent. For
// example, Parser keeps track of which function literals should be marked as
// pretenured, and PreParser doesn't care.

// The traits are expected to contain the following typedefs:
// struct Traits {
//   // In particular...
//   struct Type {
//     // Used by FunctionState and BlockState.
//     typedef Scope;
//     typedef GeneratorVariable;
//     typedef Zone;
//     // Return types for traversing functions.
//     typedef Identifier;
//     typedef Expression;
//     typedef FunctionLiteral;
//     typedef ObjectLiteralProperty;
//     typedef Literal;
//     typedef ExpressionList;
//     typedef PropertyList;
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
  typedef typename Traits::Type::FunctionLiteral FunctionLiteralT;

  ParserBase(Scanner* scanner, uintptr_t stack_limit, v8::Extension* extension,
             ParserRecorder* log, typename Traits::Type::Zone* zone,
             typename Traits::Type::Parser this_object)
      : Traits(this_object),
        parenthesized_function_(false),
        scope_(NULL),
        function_state_(NULL),
        extension_(extension),
        fni_(NULL),
        log_(log),
        mode_(PARSE_EAGERLY),  // Lazy mode must be set explicitly.
        scanner_(scanner),
        stack_limit_(stack_limit),
        stack_overflow_(false),
        allow_lazy_(false),
        allow_natives_syntax_(false),
        allow_generators_(false),
        allow_arrow_functions_(false),
        zone_(zone) {}

  // Getters that indicate whether certain syntactical constructs are
  // allowed to be parsed by this instance of the parser.
  bool allow_lazy() const { return allow_lazy_; }
  bool allow_natives_syntax() const { return allow_natives_syntax_; }
  bool allow_generators() const { return allow_generators_; }
  bool allow_arrow_functions() const { return allow_arrow_functions_; }
  bool allow_modules() const { return scanner()->HarmonyModules(); }
  bool allow_harmony_scoping() const { return scanner()->HarmonyScoping(); }
  bool allow_harmony_numeric_literals() const {
    return scanner()->HarmonyNumericLiterals();
  }

  // Setters that determine whether certain syntactical constructs are
  // allowed to be parsed by this instance of the parser.
  void set_allow_lazy(bool allow) { allow_lazy_ = allow; }
  void set_allow_natives_syntax(bool allow) { allow_natives_syntax_ = allow; }
  void set_allow_generators(bool allow) { allow_generators_ = allow; }
  void set_allow_arrow_functions(bool allow) { allow_arrow_functions_ = allow; }
  void set_allow_modules(bool allow) { scanner()->SetHarmonyModules(allow); }
  void set_allow_harmony_scoping(bool allow) {
    scanner()->SetHarmonyScoping(allow);
  }
  void set_allow_harmony_numeric_literals(bool allow) {
    scanner()->SetHarmonyNumericLiterals(allow);
  }

 protected:
  friend class Traits::Type::Checkpoint;

  enum AllowEvalOrArgumentsAsIdentifier {
    kAllowEvalOrArguments,
    kDontAllowEvalOrArguments
  };

  enum Mode {
    PARSE_LAZILY,
    PARSE_EAGERLY
  };

  class ParserCheckpoint;

  // ---------------------------------------------------------------------------
  // FunctionState and BlockState together implement the parser's scope stack.
  // The parser's current scope is in scope_. BlockState and FunctionState
  // constructors push on the scope stack and the destructors pop. They are also
  // used to hold the parser's per-function and per-block state.
  class BlockState BASE_EMBEDDED {
   public:
    BlockState(typename Traits::Type::Scope** scope_stack,
               typename Traits::Type::Scope* scope)
        : scope_stack_(scope_stack),
          outer_scope_(*scope_stack),
          scope_(scope) {
      *scope_stack_ = scope_;
    }
    ~BlockState() { *scope_stack_ = outer_scope_; }

   private:
    typename Traits::Type::Scope** scope_stack_;
    typename Traits::Type::Scope* outer_scope_;
    typename Traits::Type::Scope* scope_;
  };

  class FunctionState BASE_EMBEDDED {
   public:
    FunctionState(
        FunctionState** function_state_stack,
        typename Traits::Type::Scope** scope_stack,
        typename Traits::Type::Scope* scope,
        typename Traits::Type::Zone* zone = NULL,
        AstValueFactory* ast_value_factory = NULL);
    FunctionState(FunctionState** function_state_stack,
                  typename Traits::Type::Scope** scope_stack,
                  typename Traits::Type::Scope** scope,
                  typename Traits::Type::Zone* zone = NULL,
                  AstValueFactory* ast_value_factory = NULL);
    ~FunctionState();

    int NextMaterializedLiteralIndex() {
      return next_materialized_literal_index_++;
    }
    int materialized_literal_count() {
      return next_materialized_literal_index_ - JSFunction::kLiteralsPrefixSize;
    }

    int NextHandlerIndex() { return next_handler_index_++; }
    int handler_count() { return next_handler_index_; }

    void AddProperty() { expected_property_count_++; }
    int expected_property_count() { return expected_property_count_; }

    void set_is_generator(bool is_generator) { is_generator_ = is_generator; }
    bool is_generator() const { return is_generator_; }

    void set_generator_object_variable(
        typename Traits::Type::GeneratorVariable* variable) {
      DCHECK(variable != NULL);
      DCHECK(!is_generator());
      generator_object_variable_ = variable;
      is_generator_ = true;
    }
    typename Traits::Type::GeneratorVariable* generator_object_variable()
        const {
      return generator_object_variable_;
    }

    typename Traits::Type::Factory* factory() { return &factory_; }

   private:
    // Used to assign an index to each literal that needs materialization in
    // the function.  Includes regexp literals, and boilerplate for object and
    // array literals.
    int next_materialized_literal_index_;

    // Used to assign a per-function index to try and catch handlers.
    int next_handler_index_;

    // Properties count estimation.
    int expected_property_count_;

    // Whether the function is a generator.
    bool is_generator_;
    // For generators, this variable may hold the generator object. It variable
    // is used by yield expressions and return statements. It is not necessary
    // for generator functions to have this variable set.
    Variable* generator_object_variable_;

    FunctionState** function_state_stack_;
    FunctionState* outer_function_state_;
    typename Traits::Type::Scope** scope_stack_;
    typename Traits::Type::Scope* outer_scope_;
    int saved_ast_node_id_;  // Only used by ParserTraits.
    typename Traits::Type::Zone* extra_param_;
    typename Traits::Type::Factory factory_;

    friend class ParserTraits;
    friend class ParserCheckpoint;
  };

  // Annoyingly, arrow functions first parse as comma expressions, then when we
  // see the => we have to go back and reinterpret the arguments as being formal
  // parameters.  To do so we need to reset some of the parser state back to
  // what it was before the arguments were first seen.
  class ParserCheckpoint : public Traits::Type::Checkpoint {
   public:
    template <typename Parser>
    explicit ParserCheckpoint(Parser* parser)
        : Traits::Type::Checkpoint(parser) {
      function_state_ = parser->function_state_;
      next_materialized_literal_index_ =
          function_state_->next_materialized_literal_index_;
      next_handler_index_ = function_state_->next_handler_index_;
      expected_property_count_ = function_state_->expected_property_count_;
    }

    void Restore() {
      Traits::Type::Checkpoint::Restore();
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

  Scanner* scanner() const { return scanner_; }
  int position() { return scanner_->location().beg_pos; }
  int peek_position() { return scanner_->peek_location().beg_pos; }
  bool stack_overflow() const { return stack_overflow_; }
  void set_stack_overflow() { stack_overflow_ = true; }
  Mode mode() const { return mode_; }
  typename Traits::Type::Zone* zone() const { return zone_; }

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
    return next == Token::IDENTIFIER ||
        next == Token::FUTURE_RESERVED_WORD ||
        next == Token::FUTURE_STRICT_RESERVED_WORD ||
        next == Token::LET ||
        next == Token::YIELD;
  }

  bool CheckContextualKeyword(Vector<const char> keyword) {
    if (peek() == Token::IDENTIFIER &&
        scanner()->is_next_contextual_keyword(keyword)) {
      Consume(Token::IDENTIFIER);
      return true;
    }
    return false;
  }

  void ExpectContextualKeyword(Vector<const char> keyword, bool* ok) {
    Expect(Token::IDENTIFIER, ok);
    if (!*ok) return;
    if (!scanner()->is_literal_contextual_keyword(keyword)) {
      ReportUnexpectedToken(scanner()->current_token());
      *ok = false;
    }
  }

  // Checks whether an octal literal was last seen between beg_pos and end_pos.
  // If so, reports an error. Only called for strict mode.
  void CheckOctalLiteral(int beg_pos, int end_pos, bool* ok) {
    Scanner::Location octal = scanner()->octal_position();
    if (octal.IsValid() && beg_pos <= octal.beg_pos &&
        octal.end_pos <= end_pos) {
      ReportMessageAt(octal, "strict_octal_literal");
      scanner()->clear_octal_position();
      *ok = false;
    }
  }

  // Validates strict mode for function parameter lists. This has to be
  // done after parsing the function, since the function can declare
  // itself strict.
  void CheckStrictFunctionNameAndParameters(
      IdentifierT function_name,
      bool function_name_is_strict_reserved,
      const Scanner::Location& function_name_loc,
      const Scanner::Location& eval_args_error_loc,
      const Scanner::Location& dupe_error_loc,
      const Scanner::Location& reserved_loc,
      bool* ok) {
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
    if (eval_args_error_loc.IsValid()) {
      Traits::ReportMessageAt(eval_args_error_loc, "strict_eval_arguments");
      *ok = false;
      return;
    }
    if (dupe_error_loc.IsValid()) {
      Traits::ReportMessageAt(dupe_error_loc, "strict_param_dupe");
      *ok = false;
      return;
    }
    if (reserved_loc.IsValid()) {
      Traits::ReportMessageAt(reserved_loc, "unexpected_strict_reserved");
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

  StrictMode strict_mode() { return scope_->strict_mode(); }
  bool is_generator() const { return function_state_->is_generator(); }

  // Report syntax errors.
  void ReportMessage(const char* message, const char* arg = NULL,
                     bool is_reference_error = false) {
    Scanner::Location source_location = scanner()->location();
    Traits::ReportMessageAt(source_location, message, arg, is_reference_error);
  }

  void ReportMessageAt(Scanner::Location location, const char* message,
                       bool is_reference_error = false) {
    Traits::ReportMessageAt(location, message,
                            reinterpret_cast<const char*>(NULL),
                            is_reference_error);
  }

  void ReportUnexpectedToken(Token::Value token);

  // Recursive descent functions:

  // Parses an identifier that is valid for the current scope, in particular it
  // fails on strict mode future reserved keywords in a strict scope. If
  // allow_eval_or_arguments is kAllowEvalOrArguments, we allow "eval" or
  // "arguments" as identifier even in strict mode (this is needed in cases like
  // "var foo = eval;").
  IdentifierT ParseIdentifier(
      AllowEvalOrArgumentsAsIdentifier,
      bool* ok);
  // Parses an identifier or a strict mode future reserved word, and indicate
  // whether it is strict mode future reserved.
  IdentifierT ParseIdentifierOrStrictReservedWord(
      bool* is_strict_reserved,
      bool* ok);
  IdentifierT ParseIdentifierName(bool* ok);
  // Parses an identifier and determines whether or not it is 'get' or 'set'.
  IdentifierT ParseIdentifierNameOrGetOrSet(bool* is_get,
                                            bool* is_set,
                                            bool* ok);

  ExpressionT ParseRegExpLiteral(bool seen_equal, bool* ok);

  ExpressionT ParsePrimaryExpression(bool* ok);
  ExpressionT ParseExpression(bool accept_IN, bool* ok);
  ExpressionT ParseArrayLiteral(bool* ok);
  ExpressionT ParseObjectLiteral(bool* ok);
  typename Traits::Type::ExpressionList ParseArguments(bool* ok);
  ExpressionT ParseAssignmentExpression(bool accept_IN, bool* ok);
  ExpressionT ParseYieldExpression(bool* ok);
  ExpressionT ParseConditionalExpression(bool accept_IN, bool* ok);
  ExpressionT ParseBinaryExpression(int prec, bool accept_IN, bool* ok);
  ExpressionT ParseUnaryExpression(bool* ok);
  ExpressionT ParsePostfixExpression(bool* ok);
  ExpressionT ParseLeftHandSideExpression(bool* ok);
  ExpressionT ParseMemberWithNewPrefixesExpression(bool* ok);
  ExpressionT ParseMemberExpression(bool* ok);
  ExpressionT ParseMemberExpressionContinuation(ExpressionT expression,
                                                bool* ok);
  ExpressionT ParseArrowFunctionLiteral(int start_pos, ExpressionT params_ast,
                                        bool* ok);

  // Checks if the expression is a valid reference expression (e.g., on the
  // left-hand side of assignments). Although ruled out by ECMA as early errors,
  // we allow calls for web compatibility and rewrite them to a runtime throw.
  ExpressionT CheckAndRewriteReferenceExpression(
      ExpressionT expression,
      Scanner::Location location, const char* message, bool* ok);

  // Used to detect duplicates in object literals. Each of the values
  // kGetterProperty, kSetterProperty and kValueProperty represents
  // a type of object literal property. When parsing a property, its
  // type value is stored in the DuplicateFinder for the property name.
  // Values are chosen so that having intersection bits means the there is
  // an incompatibility.
  // I.e., you can add a getter to a property that already has a setter, since
  // kGetterProperty and kSetterProperty doesn't intersect, but not if it
  // already has a getter or a value. Adding the getter to an existing
  // setter will store the value (kGetterProperty | kSetterProperty), which
  // is incompatible with adding any further properties.
  enum PropertyKind {
    kNone = 0,
    // Bit patterns representing different object literal property types.
    kGetterProperty = 1,
    kSetterProperty = 2,
    kValueProperty = 7,
    // Helper constants.
    kValueFlag = 4
  };

  // Validation per ECMA 262 - 11.1.5 "Object Initialiser".
  class ObjectLiteralChecker {
   public:
    ObjectLiteralChecker(ParserBase* parser, StrictMode strict_mode)
        : parser_(parser),
          finder_(scanner()->unicode_cache()),
          strict_mode_(strict_mode) { }

    void CheckProperty(Token::Value property, PropertyKind type, bool* ok);

   private:
    ParserBase* parser() const { return parser_; }
    Scanner* scanner() const { return parser_->scanner(); }

    // Checks the type of conflict based on values coming from PropertyType.
    bool HasConflict(PropertyKind type1, PropertyKind type2) {
      return (type1 & type2) != 0;
    }
    bool IsDataDataConflict(PropertyKind type1, PropertyKind type2) {
      return ((type1 & type2) & kValueFlag) != 0;
    }
    bool IsDataAccessorConflict(PropertyKind type1, PropertyKind type2) {
      return ((type1 ^ type2) & kValueFlag) != 0;
    }
    bool IsAccessorAccessorConflict(PropertyKind type1, PropertyKind type2) {
      return ((type1 | type2) & kValueFlag) == 0;
    }

    ParserBase* parser_;
    DuplicateFinder finder_;
    StrictMode strict_mode_;
  };

  // If true, the next (and immediately following) function literal is
  // preceded by a parenthesis.
  // Heuristically that means that the function will be called immediately,
  // so never lazily compile it.
  bool parenthesized_function_;

  typename Traits::Type::Scope* scope_;  // Scope stack.
  FunctionState* function_state_;  // Function state stack.
  v8::Extension* extension_;
  FuncNameInferrer* fni_;
  ParserRecorder* log_;
  Mode mode_;

 private:
  Scanner* scanner_;
  uintptr_t stack_limit_;
  bool stack_overflow_;

  bool allow_lazy_;
  bool allow_natives_syntax_;
  bool allow_generators_;
  bool allow_arrow_functions_;

  typename Traits::Type::Zone* zone_;  // Only used by Parser.
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
  static PreParserIdentifier FutureReserved() {
    return PreParserIdentifier(kFutureReservedIdentifier);
  }
  static PreParserIdentifier FutureStrictReserved() {
    return PreParserIdentifier(kFutureStrictReservedIdentifier);
  }
  static PreParserIdentifier Let() {
    return PreParserIdentifier(kLetIdentifier);
  }
  static PreParserIdentifier Yield() {
    return PreParserIdentifier(kYieldIdentifier);
  }
  bool IsEval() const { return type_ == kEvalIdentifier; }
  bool IsArguments() const { return type_ == kArgumentsIdentifier; }
  bool IsEvalOrArguments() const { return type_ >= kEvalIdentifier; }
  bool IsYield() const { return type_ == kYieldIdentifier; }
  bool IsFutureReserved() const { return type_ == kFutureReservedIdentifier; }
  bool IsFutureStrictReserved() const {
    return type_ == kFutureStrictReservedIdentifier;
  }
  bool IsValidStrictVariable() const { return type_ == kUnknownIdentifier; }

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
    kYieldIdentifier,
    kEvalIdentifier,
    kArgumentsIdentifier
  };
  explicit PreParserIdentifier(Type type) : type_(type) {}
  Type type_;

  friend class PreParserExpression;
  friend class PreParserScope;
};


// Bits 0 and 1 are used to identify the type of expression:
// If bit 0 is set, it's an identifier.
// if bit 1 is set, it's a string literal.
// If neither is set, it's no particular type, and both set isn't
// use yet.
class PreParserExpression {
 public:
  static PreParserExpression Default() {
    return PreParserExpression(kUnknownExpression);
  }

  static PreParserExpression FromIdentifier(PreParserIdentifier id) {
    return PreParserExpression(kTypeIdentifier |
                               (id.type_ << kIdentifierShift));
  }

  static PreParserExpression BinaryOperation(PreParserExpression left,
                                             Token::Value op,
                                             PreParserExpression right) {
    int code = ((op == Token::COMMA) && !left.is_parenthesized() &&
                !right.is_parenthesized())
                   ? left.ArrowParamListBit() & right.ArrowParamListBit()
                   : 0;
    return PreParserExpression(kTypeBinaryOperation | code);
  }

  static PreParserExpression EmptyArrowParamList() {
    // Any expression for which IsValidArrowParamList() returns true
    // will work here.
    return FromIdentifier(PreParserIdentifier::Default());
  }

  static PreParserExpression StringLiteral() {
    return PreParserExpression(kUnknownStringLiteral);
  }

  static PreParserExpression UseStrictStringLiteral() {
    return PreParserExpression(kUseStrictString);
  }

  static PreParserExpression This() {
    return PreParserExpression(kThisExpression);
  }

  static PreParserExpression ThisProperty() {
    return PreParserExpression(kThisPropertyExpression);
  }

  static PreParserExpression Property() {
    return PreParserExpression(kPropertyExpression);
  }

  static PreParserExpression Call() {
    return PreParserExpression(kCallExpression);
  }

  bool IsIdentifier() const { return (code_ & kTypeMask) == kTypeIdentifier; }

  PreParserIdentifier AsIdentifier() const {
    DCHECK(IsIdentifier());
    return PreParserIdentifier(
        static_cast<PreParserIdentifier::Type>(code_ >> kIdentifierShift));
  }

  bool IsStringLiteral() const {
    return (code_ & kTypeMask) == kTypeStringLiteral;
  }

  bool IsUseStrictLiteral() const {
    return (code_ & kUseStrictString) == kUseStrictString;
  }

  bool IsThis() const { return (code_ & kThisExpression) == kThisExpression; }

  bool IsThisProperty() const {
    return (code_ & kThisPropertyExpression) == kThisPropertyExpression;
  }

  bool IsProperty() const {
    return (code_ & kPropertyExpression) == kPropertyExpression ||
           (code_ & kThisPropertyExpression) == kThisPropertyExpression;
  }

  bool IsCall() const { return (code_ & kCallExpression) == kCallExpression; }

  bool IsValidReferenceExpression() const {
    return IsIdentifier() || IsProperty();
  }

  bool IsValidArrowParamList() const {
    return (ArrowParamListBit() & kBinaryOperationArrowParamList) != 0 &&
           (code_ & kMultiParenthesizedExpression) == 0;
  }

  // At the moment PreParser doesn't track these expression types.
  bool IsFunctionLiteral() const { return false; }
  bool IsCallNew() const { return false; }

  PreParserExpression AsFunctionLiteral() { return *this; }

  bool IsBinaryOperation() const {
    return (code_ & kTypeMask) == kTypeBinaryOperation;
  }

  bool is_parenthesized() const {
    return (code_ & kParenthesizedExpression) != 0;
  }

  void increase_parenthesization_level() {
    code_ |= is_parenthesized() ? kMultiParenthesizedExpression
                                : kParenthesizedExpression;
  }

  // Dummy implementation for making expression->somefunc() work in both Parser
  // and PreParser.
  PreParserExpression* operator->() { return this; }

  // More dummy implementations of things PreParser doesn't need to track:
  void set_index(int index) {}  // For YieldExpressions
  void set_parenthesized() {}

  int position() const { return RelocInfo::kNoPosition; }
  void set_function_token_position(int position) {}
  void set_ast_properties(int* ast_properties) {}
  void set_dont_optimize_reason(BailoutReason dont_optimize_reason) {}

  bool operator==(const PreParserExpression& other) const {
    return code_ == other.code_;
  }
  bool operator!=(const PreParserExpression& other) const {
    return code_ != other.code_;
  }

 private:
  // Least significant 2 bits are used as expression type. The third least
  // significant bit tracks whether an expression is parenthesized. If the
  // expression is an identifier or a string literal, the other bits
  // describe the type/ (see PreParserIdentifier::Type and string literal
  // constants below). For binary operations, the other bits are flags
  // which further describe the contents of the expression.
  enum {
    kUnknownExpression = 0,
    kTypeMask = 1 | 2,
    kParenthesizedExpression = (1 << 2),
    kMultiParenthesizedExpression = (1 << 3),

    // Identifiers
    kTypeIdentifier = 1,  // Used to detect labels.
    kIdentifierShift = 5,
    kTypeStringLiteral = 2,  // Used to detect directive prologue.
    kUnknownStringLiteral = kTypeStringLiteral,
    kUseStrictString = kTypeStringLiteral | 32,
    kStringLiteralMask = kUseStrictString,

    // Binary operations. Those are needed to detect certain keywords and
    // duplicated identifier in parameter lists for arrow functions, because
    // they are initially parsed as comma-separated expressions.
    kTypeBinaryOperation = 3,
    kBinaryOperationArrowParamList = (1 << 4),

    // Below here applies if neither identifier nor string literal. Reserve the
    // 2 least significant bits for flags.
    kThisExpression = (1 << 4),
    kThisPropertyExpression = (2 << 4),
    kPropertyExpression = (3 << 4),
    kCallExpression = (4 << 4)
  };

  explicit PreParserExpression(int expression_code) : code_(expression_code) {}

  V8_INLINE int ArrowParamListBit() const {
    if (IsBinaryOperation()) return code_ & kBinaryOperationArrowParamList;
    if (IsIdentifier()) {
      const PreParserIdentifier ident = AsIdentifier();
      // A valid identifier can be an arrow function parameter list
      // except for eval, arguments, yield, and reserved keywords.
      if (ident.IsEval() || ident.IsArguments() || ident.IsYield() ||
          ident.IsFutureStrictReserved())
        return 0;
      return kBinaryOperationArrowParamList;
    }
    return 0;
  }

  int code_;
};


// PreParserExpressionList doesn't actually store the expressions because
// PreParser doesn't need to.
class PreParserExpressionList {
 public:
  // These functions make list->Add(some_expression) work (and do nothing).
  PreParserExpressionList() : length_(0) {}
  PreParserExpressionList* operator->() { return this; }
  void Add(PreParserExpression, void*) { ++length_; }
  int length() const { return length_; }
 private:
  int length_;
};


class PreParserStatement {
 public:
  static PreParserStatement Default() {
    return PreParserStatement(kUnknownStatement);
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

  bool IsFunctionDeclaration() {
    return code_ == kFunctionDeclaration;
  }

 private:
  enum Type {
    kUnknownStatement,
    kStringLiteralExpressionStatement,
    kUseStrictExpressionStatement,
    kFunctionDeclaration
  };

  explicit PreParserStatement(Type code) : code_(code) {}
  Type code_;
};



// PreParserStatementList doesn't actually store the statements because
// the PreParser does not need them.
class PreParserStatementList {
 public:
  // These functions make list->Add(some_expression) work as no-ops.
  PreParserStatementList() {}
  PreParserStatementList* operator->() { return this; }
  void Add(PreParserStatement, void*) {}
};


class PreParserScope {
 public:
  explicit PreParserScope(PreParserScope* outer_scope, ScopeType scope_type,
                          void* = NULL)
      : scope_type_(scope_type) {
    strict_mode_ = outer_scope ? outer_scope->strict_mode() : SLOPPY;
  }

  ScopeType type() { return scope_type_; }
  StrictMode strict_mode() const { return strict_mode_; }
  void SetStrictMode(StrictMode strict_mode) { strict_mode_ = strict_mode; }

  // When PreParser is in use, lazy compilation is already being done,
  // things cannot get lazier than that.
  bool AllowsLazyCompilation() const { return false; }

  void set_start_position(int position) {}
  void set_end_position(int position) {}

  bool IsDeclared(const PreParserIdentifier& identifier) const { return false; }
  void DeclareParameter(const PreParserIdentifier& identifier, VariableMode) {}

  // Allow scope->Foo() to work.
  PreParserScope* operator->() { return this; }

 private:
  ScopeType scope_type_;
  StrictMode strict_mode_;
};


class PreParserFactory {
 public:
  explicit PreParserFactory(void* extra_param1, void* extra_param2) {}
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
  PreParserExpression NewObjectLiteralProperty(bool is_getter,
                                               PreParserExpression value,
                                               int pos) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewObjectLiteralProperty(PreParserExpression key,
                                               PreParserExpression value) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewObjectLiteral(PreParserExpressionList properties,
                                       int literal_index,
                                       int boilerplate_properties,
                                       bool has_function,
                                       int pos) {
    return PreParserExpression::Default();
  }
  PreParserExpression NewVariableProxy(void* generator_variable) {
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
  PreParserStatement NewReturnStatement(PreParserExpression expression,
                                        int pos) {
    return PreParserStatement::Default();
  }
  PreParserExpression NewFunctionLiteral(
      PreParserIdentifier name, AstValueFactory* ast_value_factory,
      const PreParserScope& scope, PreParserStatementList body,
      int materialized_literal_count, int expected_property_count,
      int handler_count, int parameter_count,
      FunctionLiteral::ParameterFlag has_duplicate_parameters,
      FunctionLiteral::FunctionType function_type,
      FunctionLiteral::IsFunctionFlag is_function,
      FunctionLiteral::IsParenthesizedFlag is_parenthesized,
      FunctionLiteral::KindFlag kind, int position) {
    return PreParserExpression::Default();
  }

  // Return the object itself as AstVisitor and implement the needed
  // dummy method right in this class.
  PreParserFactory* visitor() { return this; }
  BailoutReason dont_optimize_reason() { return kNoReason; }
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

    // Used by FunctionState and BlockState.
    typedef PreParserScope Scope;
    typedef PreParserScope ScopePtr;

    class Checkpoint BASE_EMBEDDED {
     public:
      template <typename Parser>
      explicit Checkpoint(Parser* parser) {}
      void Restore() {}
    };

    // PreParser doesn't need to store generator variables.
    typedef void GeneratorVariable;
    // No interaction with Zones.
    typedef void Zone;

    typedef int AstProperties;
    typedef Vector<PreParserIdentifier> ParameterIdentifierVector;

    // Return types for traversing functions.
    typedef PreParserIdentifier Identifier;
    typedef PreParserExpression Expression;
    typedef PreParserExpression YieldExpression;
    typedef PreParserExpression FunctionLiteral;
    typedef PreParserExpression ObjectLiteralProperty;
    typedef PreParserExpression Literal;
    typedef PreParserExpressionList ExpressionList;
    typedef PreParserExpressionList PropertyList;
    typedef PreParserStatementList StatementList;

    // For constructing objects returned by the traversing functions.
    typedef PreParserFactory Factory;
  };

  explicit PreParserTraits(PreParser* pre_parser) : pre_parser_(pre_parser) {}

  // Custom operations executed when FunctionStates are created and
  // destructed. (The PreParser doesn't need to do anything.)
  template<typename FunctionState>
  static void SetUpFunctionState(FunctionState* function_state, void*) {}
  template<typename FunctionState>
  static void TearDownFunctionState(FunctionState* function_state, void*) {}

  // Helper functions for recursive descent.
  static bool IsEvalOrArguments(PreParserIdentifier identifier) {
    return identifier.IsEvalOrArguments();
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
    return identifier.IsYield() || identifier.IsFutureStrictReserved();
  }

  static bool IsBoilerplateProperty(PreParserExpression property) {
    // PreParser doesn't count boilerplate properties.
    return false;
  }

  static bool IsArrayIndex(PreParserIdentifier string, uint32_t* index) {
    return false;
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
      PreParserScope* scope, PreParserExpression value, bool* has_function) {}

  static void CheckAssigningFunctionLiteralToProperty(
      PreParserExpression left, PreParserExpression right) {}

  // PreParser doesn't need to keep track of eval calls.
  static void CheckPossibleEvalCall(PreParserExpression expression,
                                    PreParserScope* scope) {}

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
  PreParserScope NewScope(PreParserScope* outer_scope, ScopeType scope_type) {
    return PreParserScope(outer_scope, scope_type);
  }

  // Reporting errors.
  void ReportMessageAt(Scanner::Location location,
                       const char* message,
                       const char* arg = NULL,
                       bool is_reference_error = false);
  void ReportMessageAt(int start_pos,
                       int end_pos,
                       const char* message,
                       const char* arg = NULL,
                       bool is_reference_error = false);

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
  static PreParserExpression EmptyArrowParamList() {
    return PreParserExpression::EmptyArrowParamList();
  }
  static PreParserExpression EmptyLiteral() {
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

  static PreParserIdentifier GetNextSymbol(Scanner* scanner) {
    return PreParserIdentifier::Default();
  }

  static PreParserExpression ThisExpression(PreParserScope* scope,
                                            PreParserFactory* factory) {
    return PreParserExpression::This();
  }

  static PreParserExpression ExpressionFromLiteral(
      Token::Value token, int pos, Scanner* scanner,
      PreParserFactory* factory) {
    return PreParserExpression::Default();
  }

  static PreParserExpression ExpressionFromIdentifier(
      PreParserIdentifier name, int pos, PreParserScope* scope,
      PreParserFactory* factory) {
    return PreParserExpression::FromIdentifier(name);
  }

  PreParserExpression ExpressionFromString(int pos,
                                           Scanner* scanner,
                                           PreParserFactory* factory = NULL);

  PreParserExpression GetIterator(PreParserExpression iterable,
                                  PreParserFactory* factory) {
    return PreParserExpression::Default();
  }

  static PreParserExpressionList NewExpressionList(int size, void* zone) {
    return PreParserExpressionList();
  }

  static PreParserStatementList NewStatementList(int size, void* zone) {
    return PreParserStatementList();
  }

  static PreParserExpressionList NewPropertyList(int size, void* zone) {
    return PreParserExpressionList();
  }

  V8_INLINE void SkipLazyFunctionBody(PreParserIdentifier function_name,
                                      int* materialized_literal_count,
                                      int* expected_property_count, bool* ok) {
    UNREACHABLE();
  }

  V8_INLINE PreParserStatementList
      ParseEagerFunctionBody(PreParserIdentifier function_name, int pos,
                             Variable* fvar, Token::Value fvar_init_op,
                             bool is_generator, bool* ok);

  // Utility functions
  int DeclareArrowParametersFromExpression(PreParserExpression expression,
                                           PreParserScope* scope,
                                           Scanner::Location* dupe_loc,
                                           bool* ok) {
    // TODO(aperez): Detect duplicated identifiers in paramlists.
    *ok = expression.IsValidArrowParamList();
    return 0;
  }

  static AstValueFactory* ast_value_factory() { return NULL; }

  void CheckConflictingVarDeclarations(PreParserScope scope, bool* ok) {}

  // Temporary glue; these functions will move to ParserBase.
  PreParserExpression ParseV8Intrinsic(bool* ok);
  PreParserExpression ParseFunctionLiteral(
      PreParserIdentifier name,
      Scanner::Location function_name_location,
      bool name_is_strict_reserved,
      bool is_generator,
      int function_token_position,
      FunctionLiteral::FunctionType type,
      FunctionLiteral::ArityRestriction arity_restriction,
      bool* ok);

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

  PreParser(Scanner* scanner, ParserRecorder* log, uintptr_t stack_limit)
      : ParserBase<PreParserTraits>(scanner, stack_limit, NULL, log, NULL,
                                    this) {}

  // Pre-parse the program from the character stream; returns true on
  // success (even if parsing failed, the pre-parse data successfully
  // captured the syntax error), and false if a stack-overflow happened
  // during parsing.
  PreParseResult PreParseProgram() {
    PreParserScope scope(scope_, GLOBAL_SCOPE);
    FunctionState top_scope(&function_state_, &scope_, &scope);
    bool ok = true;
    int start_position = scanner()->peek_location().beg_pos;
    ParseSourceElements(Token::EOS, &ok);
    if (stack_overflow()) return kPreParseStackOverflow;
    if (!ok) {
      ReportUnexpectedToken(scanner()->current_token());
    } else if (scope_->strict_mode() == STRICT) {
      CheckOctalLiteral(start_position, scanner()->location().end_pos, &ok);
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
  PreParseResult PreParseLazyFunction(StrictMode strict_mode,
                                      bool is_generator,
                                      ParserRecorder* log);

 private:
  friend class PreParserTraits;

  // These types form an algebra over syntactic categories that is just
  // rich enough to let us recognize and propagate the constructs that
  // are either being counted in the preparser data, or is important
  // to throw the correct syntax error exceptions.

  enum VariableDeclarationContext {
    kSourceElement,
    kStatement,
    kForStatement
  };

  // If a list of variable declarations includes any initializers.
  enum VariableDeclarationProperties {
    kHasInitializers,
    kHasNoInitializers
  };


  enum SourceElements {
    kUnknownSourceElements
  };

  // All ParseXXX functions take as the last argument an *ok parameter
  // which is set to false if parsing failed; it is unchanged otherwise.
  // By making the 'exception handling' explicit, we are forced to check
  // for failure at the call sites.
  Statement ParseSourceElement(bool* ok);
  SourceElements ParseSourceElements(int end_token, bool* ok);
  Statement ParseStatement(bool* ok);
  Statement ParseFunctionDeclaration(bool* ok);
  Statement ParseBlock(bool* ok);
  Statement ParseVariableStatement(VariableDeclarationContext var_context,
                                   bool* ok);
  Statement ParseVariableDeclarations(VariableDeclarationContext var_context,
                                      VariableDeclarationProperties* decl_props,
                                      int* num_decl,
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

  V8_INLINE void SkipLazyFunctionBody(PreParserIdentifier function_name,
                                      int* materialized_literal_count,
                                      int* expected_property_count, bool* ok);
  V8_INLINE PreParserStatementList
      ParseEagerFunctionBody(PreParserIdentifier function_name, int pos,
                             Variable* fvar, Token::Value fvar_init_op,
                             bool is_generator, bool* ok);

  Expression ParseFunctionLiteral(
      Identifier name,
      Scanner::Location function_name_location,
      bool name_is_strict_reserved,
      bool is_generator,
      int function_token_pos,
      FunctionLiteral::FunctionType function_type,
      FunctionLiteral::ArityRestriction arity_restriction,
      bool* ok);
  void ParseLazyFunctionLiteralBody(bool* ok);

  bool CheckInOrOf(bool accept_OF);
};


PreParserStatementList PreParser::ParseEagerFunctionBody(
    PreParserIdentifier function_name, int pos, Variable* fvar,
    Token::Value fvar_init_op, bool is_generator, bool* ok) {
  ParsingModeScope parsing_mode(this, PARSE_EAGERLY);

  ParseSourceElements(Token::RBRACE, ok);
  if (!*ok) return PreParserStatementList();

  Expect(Token::RBRACE, ok);
  return PreParserStatementList();
}


PreParserStatementList PreParserTraits::ParseEagerFunctionBody(
    PreParserIdentifier function_name, int pos, Variable* fvar,
    Token::Value fvar_init_op, bool is_generator, bool* ok) {
  return pre_parser_->ParseEagerFunctionBody(function_name, pos, fvar,
                                             fvar_init_op, is_generator, ok);
}


template<class Traits>
ParserBase<Traits>::FunctionState::FunctionState(
    FunctionState** function_state_stack,
    typename Traits::Type::Scope** scope_stack,
    typename Traits::Type::Scope* scope,
    typename Traits::Type::Zone* extra_param,
    AstValueFactory* ast_value_factory)
    : next_materialized_literal_index_(JSFunction::kLiteralsPrefixSize),
      next_handler_index_(0),
      expected_property_count_(0),
      is_generator_(false),
      generator_object_variable_(NULL),
      function_state_stack_(function_state_stack),
      outer_function_state_(*function_state_stack),
      scope_stack_(scope_stack),
      outer_scope_(*scope_stack),
      saved_ast_node_id_(0),
      extra_param_(extra_param),
      factory_(extra_param, ast_value_factory) {
  *scope_stack_ = scope;
  *function_state_stack = this;
  Traits::SetUpFunctionState(this, extra_param);
}


template <class Traits>
ParserBase<Traits>::FunctionState::FunctionState(
    FunctionState** function_state_stack,
    typename Traits::Type::Scope** scope_stack,
    typename Traits::Type::Scope** scope,
    typename Traits::Type::Zone* extra_param,
    AstValueFactory* ast_value_factory)
    : next_materialized_literal_index_(JSFunction::kLiteralsPrefixSize),
      next_handler_index_(0),
      expected_property_count_(0),
      is_generator_(false),
      generator_object_variable_(NULL),
      function_state_stack_(function_state_stack),
      outer_function_state_(*function_state_stack),
      scope_stack_(scope_stack),
      outer_scope_(*scope_stack),
      saved_ast_node_id_(0),
      extra_param_(extra_param),
      factory_(extra_param, ast_value_factory) {
  *scope_stack_ = *scope;
  *function_state_stack = this;
  Traits::SetUpFunctionState(this, extra_param);
}


template <class Traits>
ParserBase<Traits>::FunctionState::~FunctionState() {
  *scope_stack_ = outer_scope_;
  *function_state_stack_ = outer_function_state_;
  Traits::TearDownFunctionState(this, extra_param_);
}


template<class Traits>
void ParserBase<Traits>::ReportUnexpectedToken(Token::Value token) {
  Scanner::Location source_location = scanner()->location();

  // Four of the tokens are treated specially
  switch (token) {
    case Token::EOS:
      return ReportMessageAt(source_location, "unexpected_eos");
    case Token::NUMBER:
      return ReportMessageAt(source_location, "unexpected_token_number");
    case Token::STRING:
      return ReportMessageAt(source_location, "unexpected_token_string");
    case Token::IDENTIFIER:
      return ReportMessageAt(source_location, "unexpected_token_identifier");
    case Token::FUTURE_RESERVED_WORD:
      return ReportMessageAt(source_location, "unexpected_reserved");
    case Token::LET:
    case Token::YIELD:
    case Token::FUTURE_STRICT_RESERVED_WORD:
      return ReportMessageAt(source_location, strict_mode() == SLOPPY
          ? "unexpected_token_identifier" : "unexpected_strict_reserved");
    default:
      const char* name = Token::String(token);
      DCHECK(name != NULL);
      Traits::ReportMessageAt(source_location, "unexpected_token", name);
  }
}


template<class Traits>
typename ParserBase<Traits>::IdentifierT ParserBase<Traits>::ParseIdentifier(
    AllowEvalOrArgumentsAsIdentifier allow_eval_or_arguments,
    bool* ok) {
  Token::Value next = Next();
  if (next == Token::IDENTIFIER) {
    IdentifierT name = this->GetSymbol(scanner());
    if (allow_eval_or_arguments == kDontAllowEvalOrArguments &&
        strict_mode() == STRICT && this->IsEvalOrArguments(name)) {
      ReportMessage("strict_eval_arguments");
      *ok = false;
    }
    return name;
  } else if (strict_mode() == SLOPPY &&
             (next == Token::FUTURE_STRICT_RESERVED_WORD ||
             (next == Token::LET) ||
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
  } else if (next == Token::FUTURE_STRICT_RESERVED_WORD ||
             next == Token::LET ||
             (next == Token::YIELD && !this->is_generator())) {
    *is_strict_reserved = true;
  } else {
    ReportUnexpectedToken(next);
    *ok = false;
    return Traits::EmptyIdentifier();
  }
  return this->GetSymbol(scanner());
}


template <class Traits>
typename ParserBase<Traits>::IdentifierT
ParserBase<Traits>::ParseIdentifierName(bool* ok) {
  Token::Value next = Next();
  if (next != Token::IDENTIFIER && next != Token::FUTURE_RESERVED_WORD &&
      next != Token::LET && next != Token::YIELD &&
      next != Token::FUTURE_STRICT_RESERVED_WORD && !Token::IsKeyword(next)) {
    this->ReportUnexpectedToken(next);
    *ok = false;
    return Traits::EmptyIdentifier();
  }
  return this->GetSymbol(scanner());
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
    bool seen_equal, bool* ok) {
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
    ReportMessage("invalid_regexp_flags");
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
ParserBase<Traits>::ParsePrimaryExpression(bool* ok) {
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
  //   '(' Expression ')'

  int pos = peek_position();
  ExpressionT result = this->EmptyExpression();
  Token::Value token = peek();
  switch (token) {
    case Token::THIS: {
      Consume(Token::THIS);
      result = this->ThisExpression(scope_, factory());
      break;
    }

    case Token::NULL_LITERAL:
    case Token::TRUE_LITERAL:
    case Token::FALSE_LITERAL:
    case Token::NUMBER:
      Next();
      result = this->ExpressionFromLiteral(token, pos, scanner(), factory());
      break;

    case Token::IDENTIFIER:
    case Token::LET:
    case Token::YIELD:
    case Token::FUTURE_STRICT_RESERVED_WORD: {
      // Using eval or arguments in this context is OK even in strict mode.
      IdentifierT name = ParseIdentifier(kAllowEvalOrArguments, CHECK_OK);
      result = this->ExpressionFromIdentifier(name, pos, scope_, factory());
      break;
    }

    case Token::STRING: {
      Consume(Token::STRING);
      result = this->ExpressionFromString(pos, scanner(), factory());
      break;
    }

    case Token::ASSIGN_DIV:
      result = this->ParseRegExpLiteral(true, CHECK_OK);
      break;

    case Token::DIV:
      result = this->ParseRegExpLiteral(false, CHECK_OK);
      break;

    case Token::LBRACK:
      result = this->ParseArrayLiteral(CHECK_OK);
      break;

    case Token::LBRACE:
      result = this->ParseObjectLiteral(CHECK_OK);
      break;

    case Token::LPAREN:
      Consume(Token::LPAREN);
      if (allow_arrow_functions() && peek() == Token::RPAREN) {
        // Arrow functions are the only expression type constructions
        // for which an empty parameter list "()" is valid input.
        Consume(Token::RPAREN);
        result = this->ParseArrowFunctionLiteral(
            pos, this->EmptyArrowParamList(), CHECK_OK);
      } else {
        // Heuristically try to detect immediately called functions before
        // seeing the call parentheses.
        parenthesized_function_ = (peek() == Token::FUNCTION);
        result = this->ParseExpression(true, CHECK_OK);
        result->increase_parenthesization_level();
        Expect(Token::RPAREN, CHECK_OK);
      }
      break;

    case Token::MOD:
      if (allow_natives_syntax() || extension_ != NULL) {
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

// Precedence = 1
template <class Traits>
typename ParserBase<Traits>::ExpressionT ParserBase<Traits>::ParseExpression(
    bool accept_IN, bool* ok) {
  // Expression ::
  //   AssignmentExpression
  //   Expression ',' AssignmentExpression

  ExpressionT result = this->ParseAssignmentExpression(accept_IN, CHECK_OK);
  while (peek() == Token::COMMA) {
    Expect(Token::COMMA, CHECK_OK);
    int pos = position();
    ExpressionT right = this->ParseAssignmentExpression(accept_IN, CHECK_OK);
    result = factory()->NewBinaryOperation(Token::COMMA, result, right, pos);
  }
  return result;
}


template <class Traits>
typename ParserBase<Traits>::ExpressionT ParserBase<Traits>::ParseArrayLiteral(
    bool* ok) {
  // ArrayLiteral ::
  //   '[' Expression? (',' Expression?)* ']'

  int pos = peek_position();
  typename Traits::Type::ExpressionList values =
      this->NewExpressionList(4, zone_);
  Expect(Token::LBRACK, CHECK_OK);
  while (peek() != Token::RBRACK) {
    ExpressionT elem = this->EmptyExpression();
    if (peek() == Token::COMMA) {
      elem = this->GetLiteralTheHole(peek_position(), factory());
    } else {
      elem = this->ParseAssignmentExpression(true, CHECK_OK);
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
typename ParserBase<Traits>::ExpressionT ParserBase<Traits>::ParseObjectLiteral(
    bool* ok) {
  // ObjectLiteral ::
  // '{' ((
  //       ((IdentifierName | String | Number) ':' AssignmentExpression) |
  //       (('get' | 'set') (IdentifierName | String | Number) FunctionLiteral)
  //      ) ',')* '}'
  // (Except that the trailing comma is not required.)

  int pos = peek_position();
  typename Traits::Type::PropertyList properties =
      this->NewPropertyList(4, zone_);
  int number_of_boilerplate_properties = 0;
  bool has_function = false;

  ObjectLiteralChecker checker(this, strict_mode());

  Expect(Token::LBRACE, CHECK_OK);

  while (peek() != Token::RBRACE) {
    if (fni_ != NULL) fni_->Enter();

    typename Traits::Type::Literal key = this->EmptyLiteral();
    Token::Value next = peek();
    int next_pos = peek_position();

    switch (next) {
      case Token::FUTURE_RESERVED_WORD:
      case Token::FUTURE_STRICT_RESERVED_WORD:
      case Token::LET:
      case Token::YIELD:
      case Token::IDENTIFIER: {
        bool is_getter = false;
        bool is_setter = false;
        IdentifierT id =
            ParseIdentifierNameOrGetOrSet(&is_getter, &is_setter, CHECK_OK);
        if (fni_ != NULL) this->PushLiteralName(fni_, id);

        if ((is_getter || is_setter) && peek() != Token::COLON) {
          // Special handling of getter and setter syntax:
          // { ... , get foo() { ... }, ... , set foo(v) { ... v ... } , ... }
          // We have already read the "get" or "set" keyword.
          Token::Value next = Next();
          if (next != i::Token::IDENTIFIER &&
              next != i::Token::FUTURE_RESERVED_WORD &&
              next != i::Token::FUTURE_STRICT_RESERVED_WORD &&
              next != i::Token::LET &&
              next != i::Token::YIELD &&
              next != i::Token::NUMBER &&
              next != i::Token::STRING &&
              !Token::IsKeyword(next)) {
            ReportUnexpectedToken(next);
            *ok = false;
            return this->EmptyLiteral();
          }
          // Validate the property.
          PropertyKind type = is_getter ? kGetterProperty : kSetterProperty;
          checker.CheckProperty(next, type, CHECK_OK);
          IdentifierT name = this->GetSymbol(scanner_);
          typename Traits::Type::FunctionLiteral value =
              this->ParseFunctionLiteral(
                  name, scanner()->location(),
                  false,  // reserved words are allowed here
                  false,  // not a generator
                  RelocInfo::kNoPosition, FunctionLiteral::ANONYMOUS_EXPRESSION,
                  is_getter ? FunctionLiteral::GETTER_ARITY
                            : FunctionLiteral::SETTER_ARITY,
                  CHECK_OK);
          typename Traits::Type::ObjectLiteralProperty property =
              factory()->NewObjectLiteralProperty(is_getter, value, next_pos);
          if (this->IsBoilerplateProperty(property)) {
            number_of_boilerplate_properties++;
          }
          properties->Add(property, zone());
          if (peek() != Token::RBRACE) {
            // Need {} because of the CHECK_OK macro.
            Expect(Token::COMMA, CHECK_OK);
          }

          if (fni_ != NULL) {
            fni_->Infer();
            fni_->Leave();
          }
          continue;  // restart the while
        }
        // Failed to parse as get/set property, so it's just a normal property
        // (which might be called "get" or "set" or something else).
        key = factory()->NewStringLiteral(id, next_pos);
        break;
      }
      case Token::STRING: {
        Consume(Token::STRING);
        IdentifierT string = this->GetSymbol(scanner_);
        if (fni_ != NULL) this->PushLiteralName(fni_, string);
        uint32_t index;
        if (this->IsArrayIndex(string, &index)) {
          key = factory()->NewNumberLiteral(index, next_pos);
          break;
        }
        key = factory()->NewStringLiteral(string, next_pos);
        break;
      }
      case Token::NUMBER: {
        Consume(Token::NUMBER);
        key = this->ExpressionFromLiteral(Token::NUMBER, next_pos, scanner_,
                                          factory());
        break;
      }
      default:
        if (Token::IsKeyword(next)) {
          Consume(next);
          IdentifierT string = this->GetSymbol(scanner_);
          key = factory()->NewStringLiteral(string, next_pos);
        } else {
          Token::Value next = Next();
          ReportUnexpectedToken(next);
          *ok = false;
          return this->EmptyLiteral();
        }
    }

    // Validate the property
    checker.CheckProperty(next, kValueProperty, CHECK_OK);

    Expect(Token::COLON, CHECK_OK);
    ExpressionT value = this->ParseAssignmentExpression(true, CHECK_OK);

    typename Traits::Type::ObjectLiteralProperty property =
        factory()->NewObjectLiteralProperty(key, value);

    // Mark top-level object literals that contain function literals and
    // pretenure the literal so it can be added as a constant function
    // property. (Parser only.)
    this->CheckFunctionLiteralInsideTopLevelObjectLiteral(scope_, value,
                                                          &has_function);

    // Count CONSTANT or COMPUTED properties to maintain the enumeration order.
    if (this->IsBoilerplateProperty(property)) {
      number_of_boilerplate_properties++;
    }
    properties->Add(property, zone());

    if (peek() != Token::RBRACE) {
      // Need {} because of the CHECK_OK macro.
      Expect(Token::COMMA, CHECK_OK);
    }

    if (fni_ != NULL) {
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
    bool* ok) {
  // Arguments ::
  //   '(' (AssignmentExpression)*[','] ')'

  typename Traits::Type::ExpressionList result =
      this->NewExpressionList(4, zone_);
  Expect(Token::LPAREN, CHECK_OK_CUSTOM(NullExpressionList));
  bool done = (peek() == Token::RPAREN);
  while (!done) {
    ExpressionT argument = this->ParseAssignmentExpression(
        true, CHECK_OK_CUSTOM(NullExpressionList));
    result->Add(argument, zone_);
    if (result->length() > Code::kMaxArguments) {
      ReportMessage("too_many_arguments");
      *ok = false;
      return this->NullExpressionList();
    }
    done = (peek() == Token::RPAREN);
    if (!done) {
      // Need {} because of the CHECK_OK_CUSTOM macro.
      Expect(Token::COMMA, CHECK_OK_CUSTOM(NullExpressionList));
    }
  }
  Expect(Token::RPAREN, CHECK_OK_CUSTOM(NullExpressionList));
  return result;
}

// Precedence = 2
template <class Traits>
typename ParserBase<Traits>::ExpressionT
ParserBase<Traits>::ParseAssignmentExpression(bool accept_IN, bool* ok) {
  // AssignmentExpression ::
  //   ConditionalExpression
  //   ArrowFunction
  //   YieldExpression
  //   LeftHandSideExpression AssignmentOperator AssignmentExpression

  Scanner::Location lhs_location = scanner()->peek_location();

  if (peek() == Token::YIELD && is_generator()) {
    return this->ParseYieldExpression(ok);
  }

  if (fni_ != NULL) fni_->Enter();
  ParserCheckpoint checkpoint(this);
  ExpressionT expression =
      this->ParseConditionalExpression(accept_IN, CHECK_OK);

  if (allow_arrow_functions() && peek() == Token::ARROW) {
    checkpoint.Restore();
    expression = this->ParseArrowFunctionLiteral(lhs_location.beg_pos,
                                                 expression, CHECK_OK);
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
  ExpressionT right = this->ParseAssignmentExpression(accept_IN, CHECK_OK);

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
ParserBase<Traits>::ParseYieldExpression(bool* ok) {
  // YieldExpression ::
  //   'yield' ([no line terminator] '*'? AssignmentExpression)?
  int pos = peek_position();
  Expect(Token::YIELD, CHECK_OK);
  ExpressionT generator_object =
      factory()->NewVariableProxy(function_state_->generator_object_variable());
  ExpressionT expression = Traits::EmptyExpression();
  Yield::Kind kind = Yield::SUSPEND;
  if (!scanner()->HasAnyLineTerminatorBeforeNext()) {
    if (Check(Token::MUL)) kind = Yield::DELEGATING;
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
        // a Yield::SUSPEND operation, given only one look-ahead token.
        if (kind == Yield::SUSPEND)
          break;
        DCHECK(kind == Yield::DELEGATING);
        // Delegating yields require an RHS; fall through.
      default:
        expression = ParseAssignmentExpression(false, CHECK_OK);
        break;
    }
  }
  if (kind == Yield::DELEGATING) {
    // var iterator = subject[Symbol.iterator]();
    expression = this->GetIterator(expression, factory());
  }
  typename Traits::Type::YieldExpression yield =
      factory()->NewYield(generator_object, expression, kind, pos);
  if (kind == Yield::DELEGATING) {
    yield->set_index(function_state_->NextHandlerIndex());
  }
  return yield;
}


// Precedence = 3
template <class Traits>
typename ParserBase<Traits>::ExpressionT
ParserBase<Traits>::ParseConditionalExpression(bool accept_IN, bool* ok) {
  // ConditionalExpression ::
  //   LogicalOrExpression
  //   LogicalOrExpression '?' AssignmentExpression ':' AssignmentExpression

  int pos = peek_position();
  // We start using the binary expression parser for prec >= 4 only!
  ExpressionT expression = this->ParseBinaryExpression(4, accept_IN, CHECK_OK);
  if (peek() != Token::CONDITIONAL) return expression;
  Consume(Token::CONDITIONAL);
  // In parsing the first assignment expression in conditional
  // expressions we always accept the 'in' keyword; see ECMA-262,
  // section 11.12, page 58.
  ExpressionT left = ParseAssignmentExpression(true, CHECK_OK);
  Expect(Token::COLON, CHECK_OK);
  ExpressionT right = ParseAssignmentExpression(accept_IN, CHECK_OK);
  return factory()->NewConditional(expression, left, right, pos);
}


// Precedence >= 4
template <class Traits>
typename ParserBase<Traits>::ExpressionT
ParserBase<Traits>::ParseBinaryExpression(int prec, bool accept_IN, bool* ok) {
  DCHECK(prec >= 4);
  ExpressionT x = this->ParseUnaryExpression(CHECK_OK);
  for (int prec1 = Precedence(peek(), accept_IN); prec1 >= prec; prec1--) {
    // prec1 >= 4
    while (Precedence(peek(), accept_IN) == prec1) {
      Token::Value op = Next();
      int pos = position();
      ExpressionT y = ParseBinaryExpression(prec1 + 1, accept_IN, CHECK_OK);

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
ParserBase<Traits>::ParseUnaryExpression(bool* ok) {
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
    op = Next();
    int pos = position();
    ExpressionT expression = ParseUnaryExpression(CHECK_OK);

    // "delete identifier" is a syntax error in strict mode.
    if (op == Token::DELETE && strict_mode() == STRICT &&
        this->IsIdentifier(expression)) {
      ReportMessage("strict_delete");
      *ok = false;
      return this->EmptyExpression();
    }

    // Allow Traits do rewrite the expression.
    return this->BuildUnaryExpression(expression, op, pos, factory());
  } else if (Token::IsCountOp(op)) {
    op = Next();
    Scanner::Location lhs_location = scanner()->peek_location();
    ExpressionT expression = this->ParseUnaryExpression(CHECK_OK);
    expression = this->CheckAndRewriteReferenceExpression(
        expression, lhs_location, "invalid_lhs_in_prefix_op", CHECK_OK);
    this->MarkExpressionAsAssigned(expression);

    return factory()->NewCountOperation(op,
                                        true /* prefix */,
                                        expression,
                                        position());

  } else {
    return this->ParsePostfixExpression(ok);
  }
}


template <class Traits>
typename ParserBase<Traits>::ExpressionT
ParserBase<Traits>::ParsePostfixExpression(bool* ok) {
  // PostfixExpression ::
  //   LeftHandSideExpression ('++' | '--')?

  Scanner::Location lhs_location = scanner()->peek_location();
  ExpressionT expression = this->ParseLeftHandSideExpression(CHECK_OK);
  if (!scanner()->HasAnyLineTerminatorBeforeNext() &&
      Token::IsCountOp(peek())) {
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
ParserBase<Traits>::ParseLeftHandSideExpression(bool* ok) {
  // LeftHandSideExpression ::
  //   (NewExpression | MemberExpression) ...

  ExpressionT result = this->ParseMemberWithNewPrefixesExpression(CHECK_OK);

  while (true) {
    switch (peek()) {
      case Token::LBRACK: {
        Consume(Token::LBRACK);
        int pos = position();
        ExpressionT index = ParseExpression(true, CHECK_OK);
        result = factory()->NewProperty(result, index, pos);
        Expect(Token::RBRACK, CHECK_OK);
        break;
      }

      case Token::LPAREN: {
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
            result->AsFunctionLiteral()->set_parenthesized();
          }
        }
        typename Traits::Type::ExpressionList args = ParseArguments(CHECK_OK);

        // Keep track of eval() calls since they disable all local variable
        // optimizations.
        // The calls that need special treatment are the
        // direct eval calls. These calls are all of the form eval(...), with
        // no explicit receiver.
        // These calls are marked as potentially direct eval calls. Whether
        // they are actually direct calls to eval is determined at run time.
        this->CheckPossibleEvalCall(result, scope_);
        result = factory()->NewCall(result, args, pos);
        if (fni_ != NULL) fni_->RemoveLastFunction();
        break;
      }

      case Token::PERIOD: {
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
ParserBase<Traits>::ParseMemberWithNewPrefixesExpression(bool* ok) {
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
    Consume(Token::NEW);
    int new_pos = position();
    ExpressionT result = this->ParseMemberWithNewPrefixesExpression(CHECK_OK);
    if (peek() == Token::LPAREN) {
      // NewExpression with arguments.
      typename Traits::Type::ExpressionList args =
          this->ParseArguments(CHECK_OK);
      result = factory()->NewCallNew(result, args, new_pos);
      // The expression can still continue with . or [ after the arguments.
      result = this->ParseMemberExpressionContinuation(result, CHECK_OK);
      return result;
    }
    // NewExpression without arguments.
    return factory()->NewCallNew(result, this->NewExpressionList(0, zone_),
                                 new_pos);
  }
  // No 'new' keyword.
  return this->ParseMemberExpression(ok);
}


template <class Traits>
typename ParserBase<Traits>::ExpressionT
ParserBase<Traits>::ParseMemberExpression(bool* ok) {
  // MemberExpression ::
  //   (PrimaryExpression | FunctionLiteral)
  //     ('[' Expression ']' | '.' Identifier | Arguments)*

  // The '[' Expression ']' and '.' Identifier parts are parsed by
  // ParseMemberExpressionContinuation, and the Arguments part is parsed by the
  // caller.

  // Parse the initial primary or function expression.
  ExpressionT result = this->EmptyExpression();
  if (peek() == Token::FUNCTION) {
    Consume(Token::FUNCTION);
    int function_token_position = position();
    bool is_generator = allow_generators() && Check(Token::MUL);
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
    result = this->ParseFunctionLiteral(name,
                                        function_name_location,
                                        is_strict_reserved_name,
                                        is_generator,
                                        function_token_position,
                                        function_type,
                                        FunctionLiteral::NORMAL_ARITY,
                                        CHECK_OK);
  } else {
    result = ParsePrimaryExpression(CHECK_OK);
  }

  result = ParseMemberExpressionContinuation(result, CHECK_OK);
  return result;
}


template <class Traits>
typename ParserBase<Traits>::ExpressionT
ParserBase<Traits>::ParseMemberExpressionContinuation(ExpressionT expression,
                                                      bool* ok) {
  // Parses this part of MemberExpression:
  // ('[' Expression ']' | '.' Identifier)*
  while (true) {
    switch (peek()) {
      case Token::LBRACK: {
        Consume(Token::LBRACK);
        int pos = position();
        ExpressionT index = this->ParseExpression(true, CHECK_OK);
        expression = factory()->NewProperty(expression, index, pos);
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
        expression = factory()->NewProperty(
            expression, factory()->NewStringLiteral(name, pos), pos);
        if (fni_ != NULL) {
          this->PushLiteralName(fni_, name);
        }
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
typename ParserBase<Traits>::ExpressionT ParserBase<
    Traits>::ParseArrowFunctionLiteral(int start_pos, ExpressionT params_ast,
                                       bool* ok) {
  // TODO(aperez): Change this to use ARROW_SCOPE
  typename Traits::Type::ScopePtr scope =
      this->NewScope(scope_, FUNCTION_SCOPE);
  typename Traits::Type::StatementList body;
  typename Traits::Type::AstProperties ast_properties;
  BailoutReason dont_optimize_reason = kNoReason;
  int num_parameters = -1;
  int materialized_literal_count = -1;
  int expected_property_count = -1;
  int handler_count = 0;

  {
    FunctionState function_state(&function_state_, &scope_, &scope, zone(),
                                 this->ast_value_factory());
    Scanner::Location dupe_error_loc = Scanner::Location::invalid();
    num_parameters = Traits::DeclareArrowParametersFromExpression(
        params_ast, scope_, &dupe_error_loc, ok);
    if (!*ok) {
      ReportMessageAt(
          Scanner::Location(start_pos, scanner()->location().beg_pos),
          "malformed_arrow_function_parameter_list");
      return this->EmptyExpression();
    }

    if (num_parameters > Code::kMaxArguments) {
      ReportMessageAt(Scanner::Location(params_ast->position(), position()),
                      "too_many_parameters");
      *ok = false;
      return this->EmptyExpression();
    }

    Expect(Token::ARROW, CHECK_OK);

    if (peek() == Token::LBRACE) {
      // Multiple statemente body
      Consume(Token::LBRACE);
      bool is_lazily_parsed =
          (mode() == PARSE_LAZILY && scope_->AllowsLazyCompilation());
      if (is_lazily_parsed) {
        body = this->NewStatementList(0, zone());
        this->SkipLazyFunctionBody(this->EmptyIdentifier(),
                                   &materialized_literal_count,
                                   &expected_property_count, CHECK_OK);
      } else {
        body = this->ParseEagerFunctionBody(
            this->EmptyIdentifier(), RelocInfo::kNoPosition, NULL,
            Token::INIT_VAR, false,  // Not a generator.
            CHECK_OK);
        materialized_literal_count =
            function_state.materialized_literal_count();
        expected_property_count = function_state.expected_property_count();
        handler_count = function_state.handler_count();
      }
    } else {
      // Single-expression body
      int pos = position();
      parenthesized_function_ = false;
      ExpressionT expression = ParseAssignmentExpression(true, CHECK_OK);
      body = this->NewStatementList(1, zone());
      body->Add(factory()->NewReturnStatement(expression, pos), zone());
      materialized_literal_count = function_state.materialized_literal_count();
      expected_property_count = function_state.expected_property_count();
      handler_count = function_state.handler_count();
    }

    scope->set_start_position(start_pos);
    scope->set_end_position(scanner()->location().end_pos);

    // Arrow function *parameter lists* are always checked as in strict mode.
    bool function_name_is_strict_reserved = false;
    Scanner::Location function_name_loc = Scanner::Location::invalid();
    Scanner::Location eval_args_error_loc = Scanner::Location::invalid();
    Scanner::Location reserved_loc = Scanner::Location::invalid();
    this->CheckStrictFunctionNameAndParameters(
        this->EmptyIdentifier(), function_name_is_strict_reserved,
        function_name_loc, eval_args_error_loc, dupe_error_loc, reserved_loc,
        CHECK_OK);

    // Validate strict mode.
    if (strict_mode() == STRICT) {
      CheckOctalLiteral(start_pos, scanner()->location().end_pos, CHECK_OK);
    }

    if (allow_harmony_scoping() && strict_mode() == STRICT)
      this->CheckConflictingVarDeclarations(scope, CHECK_OK);

    ast_properties = *factory()->visitor()->ast_properties();
    dont_optimize_reason = factory()->visitor()->dont_optimize_reason();
  }

  FunctionLiteralT function_literal = factory()->NewFunctionLiteral(
      this->EmptyIdentifierString(), this->ast_value_factory(), scope, body,
      materialized_literal_count, expected_property_count, handler_count,
      num_parameters, FunctionLiteral::kNoDuplicateParameters,
      FunctionLiteral::ANONYMOUS_EXPRESSION, FunctionLiteral::kIsFunction,
      FunctionLiteral::kNotParenthesized, FunctionLiteral::kArrowFunction,
      start_pos);

  function_literal->set_function_token_position(start_pos);
  function_literal->set_ast_properties(&ast_properties);
  function_literal->set_dont_optimize_reason(dont_optimize_reason);

  if (fni_ != NULL) this->InferFunctionName(fni_, function_literal);

  return function_literal;
}


template <typename Traits>
typename ParserBase<Traits>::ExpressionT
ParserBase<Traits>::CheckAndRewriteReferenceExpression(
    ExpressionT expression,
    Scanner::Location location, const char* message, bool* ok) {
  if (strict_mode() == STRICT && this->IsIdentifier(expression) &&
      this->IsEvalOrArguments(this->AsIdentifier(expression))) {
    this->ReportMessageAt(location, "strict_eval_arguments", false);
    *ok = false;
    return this->EmptyExpression();
  } else if (expression->IsValidReferenceExpression()) {
    return expression;
  } else if (expression->IsCall()) {
    // If it is a call, make it a runtime error for legacy web compatibility.
    // Rewrite `expr' to `expr[throw ReferenceError]'.
    int pos = location.beg_pos;
    ExpressionT error = this->NewThrowReferenceError(message, pos);
    return factory()->NewProperty(expression, error, pos);
  } else {
    this->ReportMessageAt(location, message, true);
    *ok = false;
    return this->EmptyExpression();
  }
}


#undef CHECK_OK
#undef CHECK_OK_CUSTOM


template <typename Traits>
void ParserBase<Traits>::ObjectLiteralChecker::CheckProperty(
    Token::Value property,
    PropertyKind type,
    bool* ok) {
  int old;
  if (property == Token::NUMBER) {
    old = scanner()->FindNumber(&finder_, type);
  } else {
    old = scanner()->FindSymbol(&finder_, type);
  }
  PropertyKind old_type = static_cast<PropertyKind>(old);
  if (HasConflict(old_type, type)) {
    if (IsDataDataConflict(old_type, type)) {
      // Both are data properties.
      if (strict_mode_ == SLOPPY) return;
      parser()->ReportMessage("strict_duplicate_property");
    } else if (IsDataAccessorConflict(old_type, type)) {
      // Both a data and an accessor property with the same name.
      parser()->ReportMessage("accessor_data_property");
    } else {
      DCHECK(IsAccessorAccessorConflict(old_type, type));
      // Both accessors of the same type.
      parser()->ReportMessage("accessor_get_set");
    }
    *ok = false;
  }
}


} }  // v8::internal

#endif  // V8_PREPARSER_H
