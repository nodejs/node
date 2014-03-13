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

#ifndef V8_PREPARSER_H
#define V8_PREPARSER_H

#include "hashmap.h"
#include "scopes.h"
#include "token.h"
#include "scanner.h"
#include "v8.h"

namespace v8 {
namespace internal {

// Common base class shared between parser and pre-parser.
template <typename Traits>
class ParserBase : public Traits {
 public:
  ParserBase(Scanner* scanner, uintptr_t stack_limit,
             v8::Extension* extension,
             typename Traits::Type::Parser this_object)
      : Traits(this_object),
        parenthesized_function_(false),
        scope_(NULL),
        function_state_(NULL),
        extension_(extension),
        scanner_(scanner),
        stack_limit_(stack_limit),
        stack_overflow_(false),
        allow_lazy_(false),
        allow_natives_syntax_(false),
        allow_generators_(false),
        allow_for_of_(false) { }

  // Getters that indicate whether certain syntactical constructs are
  // allowed to be parsed by this instance of the parser.
  bool allow_lazy() const { return allow_lazy_; }
  bool allow_natives_syntax() const { return allow_natives_syntax_; }
  bool allow_generators() const { return allow_generators_; }
  bool allow_for_of() const { return allow_for_of_; }
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
  void set_allow_for_of(bool allow) { allow_for_of_ = allow; }
  void set_allow_modules(bool allow) { scanner()->SetHarmonyModules(allow); }
  void set_allow_harmony_scoping(bool allow) {
    scanner()->SetHarmonyScoping(allow);
  }
  void set_allow_harmony_numeric_literals(bool allow) {
    scanner()->SetHarmonyNumericLiterals(allow);
  }

 protected:
  enum AllowEvalOrArgumentsAsIdentifier {
    kAllowEvalOrArguments,
    kDontAllowEvalOrArguments
  };

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
        typename Traits::Type::Zone* zone = NULL);
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
      ASSERT(variable != NULL);
      ASSERT(!is_generator());
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
    Isolate* isolate_;   // Only used by ParserTraits.
    int saved_ast_node_id_;  // Only used by ParserTraits.
    typename Traits::Type::Factory factory_;

    friend class ParserTraits;
  };

  Scanner* scanner() const { return scanner_; }
  int position() { return scanner_->location().beg_pos; }
  int peek_position() { return scanner_->peek_location().beg_pos; }
  bool stack_overflow() const { return stack_overflow_; }
  void set_stack_overflow() { stack_overflow_ = true; }

  INLINE(Token::Value peek()) {
    if (stack_overflow_) return Token::ILLEGAL;
    return scanner()->peek();
  }

  INLINE(Token::Value Next()) {
    if (stack_overflow_) return Token::ILLEGAL;
    {
      int marker;
      if (reinterpret_cast<uintptr_t>(&marker) < stack_limit_) {
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
    ASSERT(next == token);
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

  // Determine precedence of given token.
  static int Precedence(Token::Value token, bool accept_IN) {
    if (token == Token::IN && !accept_IN)
      return 0;  // 0 precedence will terminate binary expression parsing
    return Token::Precedence(token);
  }

  typename Traits::Type::Factory* factory() {
    return function_state_->factory();
  }

  bool is_classic_mode() const { return scope_->is_classic_mode(); }

  bool is_generator() const { return function_state_->is_generator(); }

  // Report syntax errors.
  void ReportMessage(const char* message, Vector<const char*> args) {
    Scanner::Location source_location = scanner()->location();
    Traits::ReportMessageAt(source_location, message, args);
  }

  void ReportMessageAt(Scanner::Location location, const char* message) {
    Traits::ReportMessageAt(location, message, Vector<const char*>::empty());
  }

  void ReportUnexpectedToken(Token::Value token);

  // Recursive descent functions:

  // Parses an identifier that is valid for the current scope, in particular it
  // fails on strict mode future reserved keywords in a strict scope. If
  // allow_eval_or_arguments is kAllowEvalOrArguments, we allow "eval" or
  // "arguments" as identifier even in strict mode (this is needed in cases like
  // "var foo = eval;").
  typename Traits::Type::Identifier ParseIdentifier(
      AllowEvalOrArgumentsAsIdentifier,
      bool* ok);
  // Parses an identifier or a strict mode future reserved word, and indicate
  // whether it is strict mode future reserved.
  typename Traits::Type::Identifier ParseIdentifierOrStrictReservedWord(
      bool* is_strict_reserved,
      bool* ok);
  typename Traits::Type::Identifier ParseIdentifierName(bool* ok);
  // Parses an identifier and determines whether or not it is 'get' or 'set'.
  typename Traits::Type::Identifier ParseIdentifierNameOrGetOrSet(bool* is_get,
                                                                bool* is_set,
                                                                bool* ok);

  typename Traits::Type::Expression ParseRegExpLiteral(bool seen_equal,
                                                       bool* ok);

  typename Traits::Type::Expression ParsePrimaryExpression(bool* ok);

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
    ObjectLiteralChecker(ParserBase* parser, LanguageMode mode)
        : parser_(parser),
          finder_(scanner()->unicode_cache()),
          language_mode_(mode) { }

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
    LanguageMode language_mode_;
  };

  // If true, the next (and immediately following) function literal is
  // preceded by a parenthesis.
  // Heuristically that means that the function will be called immediately,
  // so never lazily compile it.
  bool parenthesized_function_;

  typename Traits::Type::Scope* scope_;  // Scope stack.
  FunctionState* function_state_;  // Function state stack.
  v8::Extension* extension_;

 private:
  Scanner* scanner_;
  uintptr_t stack_limit_;
  bool stack_overflow_;

  bool allow_lazy_;
  bool allow_natives_syntax_;
  bool allow_generators_;
  bool allow_for_of_;
};


class PreParserIdentifier {
 public:
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
  static PreParserIdentifier Yield() {
    return PreParserIdentifier(kYieldIdentifier);
  }
  bool IsEval() { return type_ == kEvalIdentifier; }
  bool IsArguments() { return type_ == kArgumentsIdentifier; }
  bool IsEvalOrArguments() { return type_ >= kEvalIdentifier; }
  bool IsYield() { return type_ == kYieldIdentifier; }
  bool IsFutureReserved() { return type_ == kFutureReservedIdentifier; }
  bool IsFutureStrictReserved() {
    return type_ == kFutureStrictReservedIdentifier;
  }
  bool IsValidStrictVariable() { return type_ == kUnknownIdentifier; }

 private:
  enum Type {
    kUnknownIdentifier,
    kFutureReservedIdentifier,
    kFutureStrictReservedIdentifier,
    kYieldIdentifier,
    kEvalIdentifier,
    kArgumentsIdentifier
  };
  explicit PreParserIdentifier(Type type) : type_(type) {}
  Type type_;

  friend class PreParserExpression;
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
    return PreParserExpression(kIdentifierFlag |
                               (id.type_ << kIdentifierShift));
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

  static PreParserExpression StrictFunction() {
    return PreParserExpression(kStrictFunctionExpression);
  }

  bool IsIdentifier() { return (code_ & kIdentifierFlag) != 0; }

  // Only works corretly if it is actually an identifier expression.
  PreParserIdentifier AsIdentifier() {
    return PreParserIdentifier(
        static_cast<PreParserIdentifier::Type>(code_ >> kIdentifierShift));
  }

  bool IsStringLiteral() { return (code_ & kStringLiteralFlag) != 0; }

  bool IsUseStrictLiteral() {
    return (code_ & kStringLiteralMask) == kUseStrictString;
  }

  bool IsThis() { return code_ == kThisExpression; }

  bool IsThisProperty() { return code_ == kThisPropertyExpression; }

  bool IsStrictFunction() { return code_ == kStrictFunctionExpression; }

 private:
  // First two/three bits are used as flags.
  // Bit 0 and 1 represent identifiers or strings literals, and are
  // mutually exclusive, but can both be absent.
  enum {
    kUnknownExpression = 0,
    // Identifiers
    kIdentifierFlag = 1,  // Used to detect labels.
    kIdentifierShift = 3,

    kStringLiteralFlag = 2,  // Used to detect directive prologue.
    kUnknownStringLiteral = kStringLiteralFlag,
    kUseStrictString = kStringLiteralFlag | 8,
    kStringLiteralMask = kUseStrictString,

    // Below here applies if neither identifier nor string literal.
    kThisExpression = 4,
    kThisPropertyExpression = 8,
    kStrictFunctionExpression = 12
  };

  explicit PreParserExpression(int expression_code) : code_(expression_code) {}

  int code_;
};


class PreParserScope {
 public:
  explicit PreParserScope(PreParserScope* outer_scope, ScopeType scope_type)
      : scope_type_(scope_type) {
    if (outer_scope) {
      scope_inside_with_ =
          outer_scope->scope_inside_with_ || is_with_scope();
      language_mode_ = outer_scope->language_mode();
    } else {
      scope_inside_with_ = is_with_scope();
      language_mode_ = CLASSIC_MODE;
    }
  }

  bool is_with_scope() const { return scope_type_ == WITH_SCOPE; }
  bool is_classic_mode() const {
    return language_mode() == CLASSIC_MODE;
  }
  bool is_extended_mode() {
    return language_mode() == EXTENDED_MODE;
  }
  bool inside_with() const {
    return scope_inside_with_;
  }

  ScopeType type() { return scope_type_; }
  LanguageMode language_mode() const { return language_mode_; }
  void SetLanguageMode(LanguageMode language_mode) {
    language_mode_ = language_mode;
  }

 private:
  ScopeType scope_type_;
  bool scope_inside_with_;
  LanguageMode language_mode_;
};


class PreParserFactory {
 public:
  explicit PreParserFactory(void* extra_param) {}

  PreParserExpression NewRegExpLiteral(PreParserIdentifier js_pattern,
                                       PreParserIdentifier js_flags,
                                       int literal_index,
                                       int pos) {
    return PreParserExpression::Default();
  }
};


class PreParser;

class PreParserTraits {
 public:
  struct Type {
    typedef PreParser* Parser;

    // Types used by FunctionState and BlockState.
    typedef PreParserScope Scope;
    typedef PreParserFactory Factory;
    // PreParser doesn't need to store generator variables.
    typedef void GeneratorVariable;
    // No interaction with Zones.
    typedef void Zone;

    // Return types for traversing functions.
    typedef PreParserIdentifier Identifier;
    typedef PreParserExpression Expression;
  };

  explicit PreParserTraits(PreParser* pre_parser) : pre_parser_(pre_parser) {}

  // Custom operations executed when FunctionStates are created and
  // destructed. (The PreParser doesn't need to do anything.)
  template<typename FS>
  static void SetUpFunctionState(FS* function_state, void*) {}
  template<typename FS>
  static void TearDownFunctionState(FS* function_state) {}

  // Helper functions for recursive descent.
  static bool IsEvalOrArguments(PreParserIdentifier identifier) {
    return identifier.IsEvalOrArguments();
  }

  // Reporting errors.
  void ReportMessageAt(Scanner::Location location,
                       const char* message,
                       Vector<const char*> args);
  void ReportMessageAt(Scanner::Location location,
                       const char* type,
                       const char* name_opt);
  void ReportMessageAt(int start_pos,
                       int end_pos,
                       const char* type,
                       const char* name_opt);

  // "null" return type creators.
  static PreParserIdentifier EmptyIdentifier() {
    return PreParserIdentifier::Default();
  }
  static PreParserExpression EmptyExpression() {
    return PreParserExpression::Default();
  }

  // Producing data during the recursive descent.
  PreParserIdentifier GetSymbol(Scanner* scanner);
  static PreParserIdentifier NextLiteralString(Scanner* scanner,
                                               PretenureFlag tenured) {
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

  // Temporary glue; these functions will move to ParserBase.
  PreParserExpression ParseArrayLiteral(bool* ok);
  PreParserExpression ParseObjectLiteral(bool* ok);
  PreParserExpression ParseExpression(bool accept_IN, bool* ok);
  PreParserExpression ParseV8Intrinsic(bool* ok);

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

  enum PreParseResult {
    kPreParseStackOverflow,
    kPreParseSuccess
  };

  PreParser(Scanner* scanner,
            ParserRecorder* log,
            uintptr_t stack_limit)
      : ParserBase<PreParserTraits>(scanner, stack_limit, NULL, this),
        log_(log) {}

  // Pre-parse the program from the character stream; returns true on
  // success (even if parsing failed, the pre-parse data successfully
  // captured the syntax error), and false if a stack-overflow happened
  // during parsing.
  PreParseResult PreParseProgram() {
    PreParserScope scope(scope_, GLOBAL_SCOPE);
    FunctionState top_scope(&function_state_, &scope_, &scope, NULL);
    bool ok = true;
    int start_position = scanner()->peek_location().beg_pos;
    ParseSourceElements(Token::EOS, &ok);
    if (stack_overflow()) return kPreParseStackOverflow;
    if (!ok) {
      ReportUnexpectedToken(scanner()->current_token());
    } else if (!scope_->is_classic_mode()) {
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
  PreParseResult PreParseLazyFunction(LanguageMode mode,
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

  class Statement {
   public:
    static Statement Default() {
      return Statement(kUnknownStatement);
    }

    static Statement FunctionDeclaration() {
      return Statement(kFunctionDeclaration);
    }

    // Creates expression statement from expression.
    // Preserves being an unparenthesized string literal, possibly
    // "use strict".
    static Statement ExpressionStatement(Expression expression) {
      if (expression.IsUseStrictLiteral()) {
        return Statement(kUseStrictExpressionStatement);
      }
      if (expression.IsStringLiteral()) {
        return Statement(kStringLiteralExpressionStatement);
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

    explicit Statement(Type code) : code_(code) {}
    Type code_;
  };

  enum SourceElements {
    kUnknownSourceElements
  };

  typedef int Arguments;

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

  Expression ParseExpression(bool accept_IN, bool* ok);
  Expression ParseAssignmentExpression(bool accept_IN, bool* ok);
  Expression ParseYieldExpression(bool* ok);
  Expression ParseConditionalExpression(bool accept_IN, bool* ok);
  Expression ParseBinaryExpression(int prec, bool accept_IN, bool* ok);
  Expression ParseUnaryExpression(bool* ok);
  Expression ParsePostfixExpression(bool* ok);
  Expression ParseLeftHandSideExpression(bool* ok);
  Expression ParseMemberExpression(bool* ok);
  Expression ParseMemberExpressionContinuation(PreParserExpression expression,
                                               bool* ok);
  Expression ParseMemberWithNewPrefixesExpression(bool* ok);
  Expression ParseArrayLiteral(bool* ok);
  Expression ParseObjectLiteral(bool* ok);
  Expression ParseV8Intrinsic(bool* ok);

  Arguments ParseArguments(bool* ok);
  Expression ParseFunctionLiteral(
      Identifier name,
      Scanner::Location function_name_location,
      bool name_is_strict_reserved,
      bool is_generator,
      bool* ok);
  void ParseLazyFunctionLiteralBody(bool* ok);

  // Logs the currently parsed literal as a symbol in the preparser data.
  void LogSymbol();
  // Log the currently parsed string literal.
  Expression GetStringSymbol();

  bool CheckInOrOf(bool accept_OF);

  ParserRecorder* log_;
};


template<class Traits>
ParserBase<Traits>::FunctionState::FunctionState(
    FunctionState** function_state_stack,
    typename Traits::Type::Scope** scope_stack,
    typename Traits::Type::Scope* scope,
    typename Traits::Type::Zone* extra_param)
    : next_materialized_literal_index_(JSFunction::kLiteralsPrefixSize),
      next_handler_index_(0),
      expected_property_count_(0),
      is_generator_(false),
      generator_object_variable_(NULL),
      function_state_stack_(function_state_stack),
      outer_function_state_(*function_state_stack),
      scope_stack_(scope_stack),
      outer_scope_(*scope_stack),
      isolate_(NULL),
      saved_ast_node_id_(0),
      factory_(extra_param) {
  *scope_stack_ = scope;
  *function_state_stack = this;
  Traits::SetUpFunctionState(this, extra_param);
}


template<class Traits>
ParserBase<Traits>::FunctionState::~FunctionState() {
  *scope_stack_ = outer_scope_;
  *function_state_stack_ = outer_function_state_;
  Traits::TearDownFunctionState(this);
}


template<class Traits>
void ParserBase<Traits>::ReportUnexpectedToken(Token::Value token) {
  // We don't report stack overflows here, to avoid increasing the
  // stack depth even further.  Instead we report it after parsing is
  // over, in ParseProgram.
  if (token == Token::ILLEGAL && stack_overflow()) {
    return;
  }
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
    case Token::YIELD:
    case Token::FUTURE_STRICT_RESERVED_WORD:
      return ReportMessageAt(source_location,
                             is_classic_mode() ? "unexpected_token_identifier"
                                               : "unexpected_strict_reserved");
    default:
      const char* name = Token::String(token);
      ASSERT(name != NULL);
      Traits::ReportMessageAt(
          source_location, "unexpected_token", Vector<const char*>(&name, 1));
  }
}


template<class Traits>
typename Traits::Type::Identifier ParserBase<Traits>::ParseIdentifier(
    AllowEvalOrArgumentsAsIdentifier allow_eval_or_arguments,
    bool* ok) {
  Token::Value next = Next();
  if (next == Token::IDENTIFIER) {
    typename Traits::Type::Identifier name = this->GetSymbol(scanner());
    if (allow_eval_or_arguments == kDontAllowEvalOrArguments &&
        !is_classic_mode() && this->IsEvalOrArguments(name)) {
      ReportMessageAt(scanner()->location(), "strict_eval_arguments");
      *ok = false;
    }
    return name;
  } else if (is_classic_mode() && (next == Token::FUTURE_STRICT_RESERVED_WORD ||
                                   (next == Token::YIELD && !is_generator()))) {
    return this->GetSymbol(scanner());
  } else {
    this->ReportUnexpectedToken(next);
    *ok = false;
    return Traits::EmptyIdentifier();
  }
}


template <class Traits>
typename Traits::Type::Identifier ParserBase<
    Traits>::ParseIdentifierOrStrictReservedWord(bool* is_strict_reserved,
                                                 bool* ok) {
  Token::Value next = Next();
  if (next == Token::IDENTIFIER) {
    *is_strict_reserved = false;
  } else if (next == Token::FUTURE_STRICT_RESERVED_WORD ||
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
typename Traits::Type::Identifier ParserBase<Traits>::ParseIdentifierName(
    bool* ok) {
  Token::Value next = Next();
  if (next != Token::IDENTIFIER && next != Token::FUTURE_RESERVED_WORD &&
      next != Token::FUTURE_STRICT_RESERVED_WORD && !Token::IsKeyword(next)) {
    this->ReportUnexpectedToken(next);
    *ok = false;
    return Traits::EmptyIdentifier();
  }
  return this->GetSymbol(scanner());
}


template <class Traits>
typename Traits::Type::Identifier
ParserBase<Traits>::ParseIdentifierNameOrGetOrSet(bool* is_get,
                                                  bool* is_set,
                                                  bool* ok) {
  typename Traits::Type::Identifier result = ParseIdentifierName(ok);
  if (!*ok) return Traits::EmptyIdentifier();
  if (scanner()->is_literal_ascii() &&
      scanner()->literal_length() == 3) {
    const char* token = scanner()->literal_ascii_string().start();
    *is_get = strncmp(token, "get", 3) == 0;
    *is_set = !*is_get && strncmp(token, "set", 3) == 0;
  }
  return result;
}


template <class Traits>
typename Traits::Type::Expression
ParserBase<Traits>::ParseRegExpLiteral(bool seen_equal, bool* ok) {
  int pos = peek_position();
  if (!scanner()->ScanRegExpPattern(seen_equal)) {
    Next();
    ReportMessage("unterminated_regexp", Vector<const char*>::empty());
    *ok = false;
    return Traits::EmptyExpression();
  }

  int literal_index = function_state_->NextMaterializedLiteralIndex();

  typename Traits::Type::Identifier js_pattern =
      this->NextLiteralString(scanner(), TENURED);
  if (!scanner()->ScanRegExpFlags()) {
    Next();
    ReportMessageAt(scanner()->location(), "invalid_regexp_flags");
    *ok = false;
    return Traits::EmptyExpression();
  }
  typename Traits::Type::Identifier js_flags =
      this->NextLiteralString(scanner(), TENURED);
  Next();
  return factory()->NewRegExpLiteral(js_pattern, js_flags, literal_index, pos);
}


#define CHECK_OK  ok); \
  if (!*ok) return this->EmptyExpression(); \
  ((void)0
#define DUMMY )  // to make indentation work
#undef DUMMY

template <class Traits>
typename Traits::Type::Expression ParserBase<Traits>::ParsePrimaryExpression(
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
  //   '(' Expression ')'

  int pos = peek_position();
  typename Traits::Type::Expression result = this->EmptyExpression();
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
    case Token::YIELD:
    case Token::FUTURE_STRICT_RESERVED_WORD: {
      // Using eval or arguments in this context is OK even in strict mode.
      typename Traits::Type::Identifier name =
          ParseIdentifier(kAllowEvalOrArguments, CHECK_OK);
      result =
          this->ExpressionFromIdentifier(name, pos, scope_, factory());
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
      // Heuristically try to detect immediately called functions before
      // seeing the call parentheses.
      parenthesized_function_ = (peek() == Token::FUNCTION);
      result = this->ParseExpression(true, CHECK_OK);
      Expect(Token::RPAREN, CHECK_OK);
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

#undef CHECK_OK


template <typename Traits>
void ParserBase<Traits>::ObjectLiteralChecker::CheckProperty(
    Token::Value property,
    PropertyKind type,
    bool* ok) {
  int old;
  if (property == Token::NUMBER) {
    old = finder_.AddNumber(scanner()->literal_ascii_string(), type);
  } else if (scanner()->is_literal_ascii()) {
    old = finder_.AddAsciiSymbol(scanner()->literal_ascii_string(), type);
  } else {
    old = finder_.AddUtf16Symbol(scanner()->literal_utf16_string(), type);
  }
  PropertyKind old_type = static_cast<PropertyKind>(old);
  if (HasConflict(old_type, type)) {
    if (IsDataDataConflict(old_type, type)) {
      // Both are data properties.
      if (language_mode_ == CLASSIC_MODE) return;
      parser()->ReportMessageAt(scanner()->location(),
                               "strict_duplicate_property");
    } else if (IsDataAccessorConflict(old_type, type)) {
      // Both a data and an accessor property with the same name.
      parser()->ReportMessageAt(scanner()->location(),
                               "accessor_data_property");
    } else {
      ASSERT(IsAccessorAccessorConflict(old_type, type));
      // Both accessors of the same type.
      parser()->ReportMessageAt(scanner()->location(),
                               "accessor_get_set");
    }
    *ok = false;
  }
}


} }  // v8::internal

#endif  // V8_PREPARSER_H
